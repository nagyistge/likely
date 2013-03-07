/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright 2013 Joshua C. Klontz                                           *
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License");           *
 * you may not use this file except in compliance with the License.          *
 * You may obtain a copy of the License at                                   *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 * Unless required by applicable law or agreed to in writing, software       *
 * distributed under the License is distributed on an "AS IS" BASIS,         *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  *
 * See the License for the specific language governing permissions and       *
 * limitations under the License.                                            *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <llvm/Intrinsics.h>
#include <llvm/LLVMContext.h>
#include <llvm/DerivedTypes.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/IRBuilder.h>
#include <llvm/Module.h>
#include <llvm/Operator.h>
#include <llvm/PassManager.h>
#include <llvm/Type.h>
#include <llvm/Value.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <stdarg.h>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "likely.h"

using namespace llvm;
using namespace std;

double likely_element(const likely_matrix *m, uint32_t c, uint32_t x, uint32_t y, uint32_t t)
{
    assert((m != NULL) && "Null matrix!");
    const int columnStep = m->channels;
    const int rowStep = m->channels * columnStep;
    const int frameStep = m->rows * rowStep;
    const int index = t*frameStep + y*rowStep + x*columnStep + c;

    switch (likely_type(m)) {
      case likely_matrix::u8:  return  ((uint8_t*)m->data)[index];
      case likely_matrix::u16: return ((uint16_t*)m->data)[index];
      case likely_matrix::u32: return ((uint32_t*)m->data)[index];
      case likely_matrix::u64: return ((uint64_t*)m->data)[index];
      case likely_matrix::i8:  return   ((int8_t*)m->data)[index];
      case likely_matrix::i16: return  ((int16_t*)m->data)[index];
      case likely_matrix::i32: return  ((int32_t*)m->data)[index];
      case likely_matrix::i64: return  ((int64_t*)m->data)[index];
      case likely_matrix::f32: return    ((float*)m->data)[index];
      case likely_matrix::f64: return   ((double*)m->data)[index];
      default:                 assert(!"Unsupported element type!");
    }
    return numeric_limits<double>::quiet_NaN();
}

void likely_set_element(likely_matrix *m, double value, uint32_t c, uint32_t x, uint32_t y, uint32_t t)
{
    assert((m != NULL) && "Null matrix!");
    const int columnStep = m->channels;
    const int rowStep = m->channels * columnStep;
    const int frameStep = m->rows * rowStep;
    const int index = t*frameStep + y*rowStep + x*columnStep + c;

    switch (likely_type(m)) {
      case likely_matrix::u8:   ((uint8_t*)m->data)[index] = value; break;
      case likely_matrix::u16: ((uint16_t*)m->data)[index] = value; break;
      case likely_matrix::u32: ((uint32_t*)m->data)[index] = value; break;
      case likely_matrix::u64: ((uint64_t*)m->data)[index] = value; break;
      case likely_matrix::i8:    ((int8_t*)m->data)[index] = value; break;
      case likely_matrix::i16:  ((int16_t*)m->data)[index] = value; break;
      case likely_matrix::i32:  ((int32_t*)m->data)[index] = value; break;
      case likely_matrix::i64:  ((int64_t*)m->data)[index] = value; break;
      case likely_matrix::f32:    ((float*)m->data)[index] = value; break;
      case likely_matrix::f64:   ((double*)m->data)[index] = value; break;
      default:                 assert(!"Unsupported element type!");
    }
}

namespace likely
{

static const uint8_t numArities = 4; // 0 through 3
static Module *TheModule = NULL;
static StructType *TheMatrixStruct = NULL;

static vector<string> split(const string &s, char delim)
{
    vector<string> elems;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim))
        elems.push_back(item);
    return elems;
}

static int indexOf(const vector<string> &v, const string &s)
{
    auto index = find(v.begin(), v.end(), s);
    if (index == v.end()) return -1;
    return distance(v.begin(), index);
}

struct Node
{
    string name;
    vector<Node> children;
};

struct Definition
{
    string name, documentation;
    vector<string> parameters, equation;

    Definition() = default;
    Definition(const smatch &sm)
    {
        if (sm.size() != 5) {
            printf("ERROR - Definition expected 5 fields.\n");
            abort();
        }

        name = sm[2];
        parameters = split(string(sm[3]).substr(1, string(sm[3]).size()-2), ',');
        documentation = sm[4];

        // Equation
        static const regex syntax("^\\s*([+\\-*/]|\\w+).*$");
        string unparsed = sm[1];
        while (!unparsed.empty()) {
            smatch sm;
            regex_match(unparsed, sm, syntax);
            if (sm.size() < 2) {
                fprintf(stderr, "ERROR - Failed to parse %s", unparsed.c_str());
                abort();
            }
            const string &token = sm[1];
            equation.push_back(token);
            unparsed = unparsed.substr(unparsed.find(token.c_str())+token.size());
        }
    }

    static Definition get(const string &description)
    {
        // Parse likely_index_html for definitions
        if (definitions.size() == 0)
            for (const Definition &definition : Definition::definitionsFromString(indexHTML()))
                definitions[definition.name] = definition;

        const string name = description.substr(0, description.find('('));
        Definition definition = definitions[name];
        if (name != definition.name) {
            fprintf(stderr, "ERROR - Missing definition for: %s\n", name.c_str());
            abort();
        }

        return definition;
    }

    static vector<Definition> definitionsFromString(const string &str)
    {
        static const string begin = "<div class=\"likely\">";
        static const string end = "</div>";
        static const regex  syntax("\\s*\\$\\$(.*)\\$\\$\\s*<h4>(.*)<small>(.*)</small></h4>\\s*<p>(.*)</p>\\s*");

        vector<Definition> definitions;
        size_t startDefinition = str.find(begin.c_str());
        while (startDefinition != string::npos) {
            size_t endDefinition = str.find(end.c_str(), startDefinition+begin.length());
            if (endDefinition == string::npos) {
                fprintf(stderr, "ERROR - Unclosed definition, missing %s\n", end.c_str());
                abort();
            }
            const string definition = str.substr(startDefinition+begin.length(), endDefinition-startDefinition-begin.length());
            smatch sm;
            if (!regex_match(definition, sm, syntax)) {
                fprintf(stderr, "ERROR - Invalid definition: %s\n", definition.c_str());
                abort();
            }
            definitions.push_back(Definition(sm));
            startDefinition = str.find(begin.c_str(), endDefinition+1, begin.length());
        }

        return definitions;
    }

private:
    static map<string,Definition> definitions;
};

map<string, Definition> Definition::definitions;

struct MatrixBuilder
{
    const likely_matrix *m;
    Value *v;
    IRBuilder<> *b;
    Function *f;
    Twine name;

    MatrixBuilder(const likely_matrix *matrix, Value *value, IRBuilder<> *builder, Function *function, const Twine &name_)
        : m(matrix), v(value), b(builder), f(function), name(name_) {}

    static Constant *constant(int value, int bits = 32) { return Constant::getIntegerValue(Type::getInt32Ty(getGlobalContext()), APInt(bits, value)); }
    static Constant *constant(bool value) { return constant(value, 1); }
    static Constant *constant(float value) { return ConstantFP::get(Type::getFloatTy(getGlobalContext()), value == 0 ? -0.0f : value); }
    static Constant *constant(double value) { return ConstantFP::get(Type::getDoubleTy(getGlobalContext()), value == 0 ? -0.0 : value); }
    static Constant *zero() { return constant(0); }
    static Constant *one() { return constant(1); }
    Constant *autoConstant(double value) const { return likely_is_floating(m) ? ((likely_depth(m) == 64) ? constant(value) : constant(float(value))) : constant(int(value), likely_depth(m)); }
    AllocaInst *autoAlloca(double value, const Twine &name = "") const { AllocaInst *alloca = b->CreateAlloca(ty(), 0, name); b->CreateStore(autoConstant(value), alloca); return alloca; }

    Value *data(Value *matrix, const Twine &name = "") const { return b->CreateLoad(b->CreateStructGEP(matrix, 0), name+"_data"); }
    Value *data(Value *matrix, Type *type, const Twine &name = "") const { return b->CreatePointerCast(data(matrix, name), type); }
    Value *channels(Value *matrix, const Twine &name = "") const { return b->CreateLoad(b->CreateStructGEP(matrix, 1), name+"_channels"); }
    Value *columns(Value *matrix, const Twine &name = "") const { return b->CreateLoad(b->CreateStructGEP(matrix, 2), name+"_columns"); }
    Value *rows(Value *matrix, const Twine &name = "") const { return b->CreateLoad(b->CreateStructGEP(matrix, 3), name+"_rows"); }
    Value *frames(Value *matrix, const Twine &name = "") const { return b->CreateLoad(b->CreateStructGEP(matrix, 4), name+"_frames"); }
    Value *hash(Value *matrix, const Twine &name = "") const { return b->CreateLoad(b->CreateStructGEP(matrix, 5), name+"_hash"); }

    Value *data(bool cast = true) const { return cast ? data(v, ptrTy(), name) : data(v, name); }
    Value *channels() const { return likely_is_single_channel(m) ? static_cast<Value*>(one()) : channels(v, name); }
    Value *columns() const { return likely_is_single_column(m) ? static_cast<Value*>(one()) : columns(v, name); }
    Value *rows() const { return likely_is_single_row(m) ? static_cast<Value*>(one()) : rows(v, name); }
    Value *frames() const { return likely_is_single_frame(m) ? static_cast<Value*>(one()) : frames(v, name); }
    Value *hash() const { return hash(v, name); }

    void setData(Value *matrix, Value *value) const { b->CreateStore(value, b->CreateStructGEP(matrix, 0)); }
    void setChannels(Value *matrix, Value *value) const { b->CreateStore(value, b->CreateStructGEP(matrix, 1)); }
    void setColumns(Value *matrix, Value *value) const { b->CreateStore(value, b->CreateStructGEP(matrix, 2)); }
    void setRows(Value *matrix, Value *value) const { b->CreateStore(value, b->CreateStructGEP(matrix, 3)); }
    void setFrames(Value *matrix, Value *value) const { b->CreateStore(value, b->CreateStructGEP(matrix, 4)); }
    void setHash(Value *matrix, Value *value) const { b->CreateStore(value, b->CreateStructGEP(matrix, 5)); }

    void setData(Value *value) const { setData(v, value); }
    void setChannels(Value *value) const { setChannels(v, value); }
    void setColumns(Value *value) const { setColumns(v, value); }
    void setRows(Value *value) const { setRows(v, value); }
    void setFrames(Value *value) const { setFrames(v, value); }
    void setHash(Value *value) const { setHash(v, value); }

    void copyHeaderTo(Value *matrix) const {
        setChannels(matrix, channels());
        setColumns(matrix, columns());
        setRows(matrix, rows());
        setFrames(matrix, frames());
        setHash(matrix, hash());
    }

    void deallocate() const {
        static Function *free = TheModule->getFunction("free");
        if (!free) {
            Type *freeReturn = Type::getVoidTy(getGlobalContext());
            vector<Type*> freeParams;
            freeParams.push_back(Type::getInt8PtrTy(getGlobalContext()));
            FunctionType* freeType = FunctionType::get(freeReturn, freeParams, false);
            free = Function::Create(freeType, GlobalValue::ExternalLinkage, "free", TheModule);
            free->setCallingConv(CallingConv::C);
        }

        vector<Value*> freeArgs;
        freeArgs.push_back(b->CreateStructGEP(v, 0));
        b->CreateCall(free, freeArgs);
        setData(ConstantPointerNull::get(Type::getInt8PtrTy(getGlobalContext())));
    }

    Value *get(int mask) const { return b->CreateAnd(hash(), constant(mask, 16)); }
    void set(int value, int mask) const { setHash(b->CreateOr(b->CreateAnd(hash(), constant(~mask, 16)), b->CreateAnd(constant(value, 16), constant(mask, 16)))); }
    void setBit(bool on, int mask) const { on ? setHash(b->CreateOr(hash(), constant(mask, 16))) : setHash(b->CreateAnd(hash(), constant(~mask, 16))); }

    Value *bits() const { return get(likely_matrix::Depth); }
    void setBits(int bits) const { set(bits, likely_matrix::Depth); }
    Value *isSigned() const { return get(likely_matrix::Signed); }
    void setSigned(bool isSigned) const { setBit(isSigned, likely_matrix::Signed); }
    Value *isFloating() const { return get(likely_matrix::Floating); }
    void setFloating(bool isFloating) const { if (isFloating) setSigned(true); setBit(isFloating, likely_matrix::Floating); }
    Value *type() const { return get(likely_matrix::Depth + likely_matrix::Floating + likely_matrix::Signed); }
    void setType(int type) const { set(type, likely_matrix::Depth + likely_matrix::Floating + likely_matrix::Signed); }
    Value *isParallel() const { return get(likely_matrix::Parallel); }
    void setParallel(bool isParallel) const { setBit(isParallel, likely_matrix::Parallel); }
    Value *isHeterogeneous() const { return get(likely_matrix::Heterogeneous); }
    void setHeterogeneous(bool isHeterogeneous) const { setBit(isHeterogeneous, likely_matrix::Heterogeneous); }
    Value *isSingleChannel() const { return get(likely_matrix::SingleChannel); }
    void setSingleChannel(bool isSingleChannel) const { setBit(isSingleChannel, likely_matrix::SingleChannel); }
    Value *isSingleColumn() const { return get(likely_matrix::SingleColumn); }
    void setSingleColumn(bool isSingleColumn) { setBit(isSingleColumn, likely_matrix::SingleColumn); }
    Value *isSingleRow() const { return get(likely_matrix::SingleRow); }
    void setSingleRow(bool isSingleRow) const { setBit(isSingleRow, likely_matrix::SingleRow); }
    Value *isSingleFrame() const { return get(likely_matrix::SingleFrame); }
    void setSingleFrame(bool isSingleFrame) const { setBit(isSingleFrame, likely_matrix::SingleFrame); }
    Value *elements() const { return b->CreateMul(b->CreateMul(b->CreateMul(channels(), columns()), rows()), frames()); }
    Value *bytes() const { return b->CreateMul(b->CreateUDiv(b->CreateCast(Instruction::ZExt, bits(), Type::getInt32Ty(getGlobalContext())), constant(8, 32)), elements()); }

    Value *columnStep() const { Value *columnStep = channels(); columnStep->setName(name+"_cStep"); return columnStep; }
    Value *rowStep() const { return b->CreateMul(columns(), columnStep(), name+"_rStep"); }
    Value *frameStep() const { return b->CreateMul(rows(), rowStep(), name+"_tStep"); }
    Value *aliasColumnStep(const MatrixBuilder &other) const { return (m->channels == other.m->channels) ? other.columnStep() : columnStep(); }
    Value *aliasRowStep(const MatrixBuilder &other) const { return (m->columns == other.m->columns) ? other.rowStep() : rowStep(); }
    Value *aliasFrameStep(const MatrixBuilder &other) const { return (m->rows == other.m->rows) ? other.frameStep() : frameStep(); }

    Value *index(Value *c) const { return likely_is_single_channel(m) ? constant(0) : c; }
    Value *index(Value *c, Value *x) const { return likely_is_single_column(m) ? index(c) : b->CreateAdd(b->CreateMul(x, columnStep()), index(c)); }
    Value *index(Value *c, Value *x, Value *y) const { return likely_is_single_row(m) ? index(c, x) : b->CreateAdd(b->CreateMul(y, rowStep()), index(c, x)); }
    Value *index(Value *c, Value *x, Value *y, Value *f) const { return likely_is_single_frame(m) ? index(c, x, y) : b->CreateAdd(b->CreateMul(f, frameStep()), index(c, x, y)); }
    Value *aliasIndex(const MatrixBuilder &other, Value *c, Value *x) const { return likely_is_single_column(m) ? index(c) : b->CreateAdd(b->CreateMul(x, aliasColumnStep(other)), index(c)); }
    Value *aliasIndex(const MatrixBuilder &other, Value *c, Value *x, Value *y) const { return likely_is_single_row(m) ? aliasIndex(other, c, x) : b->CreateAdd(b->CreateMul(y, aliasRowStep(other)), aliasIndex(other, c, x)); }
    Value *aliasIndex(const MatrixBuilder &other, Value *c, Value *x, Value *y, Value *f) const { return likely_is_single_frame(m) ? aliasIndex(other, c, x, y) : b->CreateAdd(b->CreateMul(f, aliasFrameStep(other)), aliasIndex(other, c, x, y)); }

    void deindex(Value *i, Value **c) const {
        *c = likely_is_single_channel(m) ? constant(0) : i;
    }
    void deindex(Value *i, Value **c, Value **x) const {
        Value *rem;
        if (likely_is_single_column(m)) {
            rem = i;
            *x = constant(0);
        } else {
            Value *step = columnStep();
            rem = b->CreateURem(i, step, name+"_xRem");
            *x = b->CreateExactUDiv(b->CreateSub(i, rem), step, name+"_x");
        }
        deindex(rem, c);
    }
    void deindex(Value *i, Value **c, Value **x, Value **y) const {
        Value *rem;
        if (likely_is_single_row(m)) {
            rem = i;
            *y = constant(0);
        } else {
            Value *step = rowStep();
            rem = b->CreateURem(i, step, name+"_yRem");
            *y = b->CreateExactUDiv(b->CreateSub(i, rem), step, name+"_y");
        }
        deindex(rem, c, x);
    }
    void deindex(Value *i, Value **c, Value **x, Value **y, Value **t) const {
        Value *rem;
        if (likely_is_single_frame(m)) {
            rem = i;
            *t = constant(0);
        } else {
            Value *step = frameStep();
            rem = b->CreateURem(i, step, name+"_tRem");
            *t = b->CreateExactUDiv(b->CreateSub(i, rem), step, name+"_t");
        }
        deindex(rem, c, x, y);
    }

    LoadInst *load(Value *matrix, Type *type, Value *i) const { return b->CreateLoad(b->CreateGEP(data(matrix, type), i)); }
    LoadInst *load(Value *i) const { return b->CreateLoad(b->CreateGEP(data(), i)); }
    StoreInst *store(Value *matrix, Type *type, Value *i, Value *value) const { Value *d = data(matrix, type);
                                                                                d->dump(); i->dump();
                                                                                Value *idx = b->CreateGEP(d, i);
                                                                                return b->CreateStore(value, idx); }
    StoreInst *store(Value *i, Value *value) const { return b->CreateStore(value, b->CreateGEP(data(), i)); }

    Value *cast(Value *i, const MatrixBuilder &dst) const { return (likely_type(m) == likely_type(dst.m)) ? i : b->CreateCast(CastInst::getCastOpcode(i, likely_is_signed(m), dst.ty(), likely_is_signed(dst.m)), i, dst.ty()); }
    Value *add(Value *i, Value *j, const Twine &name = "") const { return likely_is_floating(m) ? b->CreateFAdd(i, j, name) : b->CreateAdd(i, j, name); }
    Value *multiply(Value *i, Value *j, const Twine &name = "") const { return likely_is_floating(m) ? b->CreateFMul(i, j, name) : b->CreateMul(i, j, name); }

    Value *compareLT(Value *i, Value *j) const { return likely_is_floating(m) ? b->CreateFCmpOLT(i, j) : (likely_is_signed(m) ? b->CreateICmpSLT(i, j) : b->CreateICmpULT(i, j)); }
    Value *compareGT(Value *i, Value *j) const { return likely_is_floating(m) ? b->CreateFCmpOGT(i, j) : (likely_is_signed(m) ? b->CreateICmpSGT(i, j) : b->CreateICmpUGT(i, j)); }

    static PHINode *beginLoop(IRBuilder<> &builder, Function *function, BasicBlock *entry, BasicBlock *&loop, BasicBlock *&exit, Value *stop, const Twine &name = "") {
        loop = BasicBlock::Create(getGlobalContext(), "loop_"+name, function);
        builder.CreateBr(loop);
        builder.SetInsertPoint(loop);

        PHINode *i = builder.CreatePHI(Type::getInt32Ty(getGlobalContext()), 2, name);
        i->addIncoming(MatrixBuilder::zero(), entry);
        Value *increment = builder.CreateAdd(i, MatrixBuilder::one(), "increment_"+name);
        BasicBlock *body = BasicBlock::Create(getGlobalContext(), "loop_"+name+"_body", function);
        i->addIncoming(increment, body);

        exit = BasicBlock::Create(getGlobalContext(), "loop_"+name+"_exit", function);
        builder.CreateCondBr(builder.CreateICmpEQ(i, stop, "loop_"+name+"_test"), exit, body);
        builder.SetInsertPoint(body);
        return i;
    }
    PHINode *beginLoop(BasicBlock *entry, BasicBlock *&loop, BasicBlock *&exit, Value *stop, const Twine &name = "") const { return beginLoop(*b, f, entry, loop, exit, stop, name); }

    static void endLoop(IRBuilder<> &builder, BasicBlock *loop, BasicBlock *exit) {
        builder.CreateBr(loop);
        builder.SetInsertPoint(exit);
    }
    void endLoop(BasicBlock *loop, BasicBlock *exit) const { endLoop(*b, loop, exit); }

    template <typename T>
    inline static vector<T> toVector(T value) { vector<T> vector; vector.push_back(value); return vector; }

    static Type *ty(const likely_matrix &m)
    {
        const int bits = likely_depth(&m);
        if (likely_is_floating(&m)) {
            if      (bits == 16) return Type::getHalfTy(getGlobalContext());
            else if (bits == 32) return Type::getFloatTy(getGlobalContext());
            else if (bits == 64) return Type::getDoubleTy(getGlobalContext());
        } else {
            if      (bits == 1)  return Type::getInt1Ty(getGlobalContext());
            else if (bits == 8)  return Type::getInt8Ty(getGlobalContext());
            else if (bits == 16) return Type::getInt16Ty(getGlobalContext());
            else if (bits == 32) return Type::getInt32Ty(getGlobalContext());
            else if (bits == 64) return Type::getInt64Ty(getGlobalContext());
        }
        fprintf(stderr, "ERROR - Invalid matrix type\n");
        abort();
        return NULL;
    }
    inline Type *ty() const { return ty(*m); }
    inline vector<Type*> tys() const { return toVector<Type*>(ty()); }

    static Type *ptrTy(const likely_matrix &m)
    {
        const int bits = likely_depth(&m);
        if (likely_is_floating(&m)) {
            if      (bits == 16) return Type::getHalfPtrTy(getGlobalContext());
            else if (bits == 32) return Type::getFloatPtrTy(getGlobalContext());
            else if (bits == 64) return Type::getDoublePtrTy(getGlobalContext());
        } else {
            if      (bits == 1)  return Type::getInt1PtrTy(getGlobalContext());
            else if (bits == 8)  return Type::getInt8PtrTy(getGlobalContext());
            else if (bits == 16) return Type::getInt16PtrTy(getGlobalContext());
            else if (bits == 32) return Type::getInt32PtrTy(getGlobalContext());
            else if (bits == 64) return Type::getInt64PtrTy(getGlobalContext());
        }
        fprintf(stderr, "ERROR - Invalid matrix type\n");
        abort();
        return NULL;
    }
    inline Type *ptrTy() const { return ptrTy(*m); }
};

class KernelBuilder
{
    vector<string> equation;

public:
    KernelBuilder(const string &description)
    {
        const size_t lParen = description.find('(');
        const string name = description.substr(0, lParen);
        const vector<string> arguments = split(description.substr(lParen+1, description.size()-lParen-2), ',');

        Definition definition = Definition::get(description);
        const vector<string> parameters = definition.parameters;
        if (arguments.size() != parameters.size()) {
            fprintf(stderr, "ERROR - Function %s has %ld parameters but was only given %ld arguments\n", name.c_str(), parameters.size(), arguments.size());
            abort();
        }

        for (const string &token : definition.equation) {
            const int index = indexOf(parameters, token);
            if (index == -1) equation.push_back(token);
            else             equation.push_back(arguments[index]);
        }
    }

    void makeAllocation(Function *function, const vector<likely_matrix*> &matricies) const
    {
        vector<Value*> srcs;
        Value *dst;
        getValues(function, srcs, dst);

        BasicBlock *entry = BasicBlock::Create(getGlobalContext(), "entry", function);
        IRBuilder<> builder(entry);

        vector<MatrixBuilder> matrixBuilders;
        for (size_t i = 0; i < matricies.size(); i++)
            matrixBuilders.push_back(MatrixBuilder(matricies[i], srcs[i], &builder, function, "src"+to_string(i)));

        matrixBuilders.front().copyHeaderTo(dst);
        builder.CreateRet(matrixBuilders.front().elements());
    }

    void makeKernel(Function *function, const vector<likely_matrix*> &matricies) const
    {
        vector<Value*> srcs;
        Value *dst, *len;
        getValues(function, srcs, dst, len);

        BasicBlock *entry = BasicBlock::Create(getGlobalContext(), "entry", function);
        IRBuilder<> builder(entry);

        vector<MatrixBuilder> matrixBuilders;
        for (size_t i = 0; i < matricies.size(); i++)
            matrixBuilders.push_back(MatrixBuilder(matricies[i], srcs[i], &builder, function, "src"+to_string(i)));

        BasicBlock *loop, *exit;
        PHINode *i = MatrixBuilder::beginLoop(builder, function, entry, loop, exit, len, "i");
        (void) i;
        MatrixBuilder::endLoop(builder, loop, exit);
        builder.CreateRetVoid();
    }

    static void getValues(Function *function, vector<Value*> &srcs, Value *&dst)
    {
        Function::arg_iterator args = function->arg_begin();
        int i = 0;
        while (args != function->arg_end()) {
            Value *src = args++;
            stringstream name; name << "src" << char(int('A')+(i++));
            src->setName(name.str());
            srcs.push_back(src);
        }
        dst = srcs.back();
        srcs.pop_back();
        dst->setName("dst");
    }

    static void getValues(Function *function, vector<Value*> &srcs, Value *&dst, Value *&len)
    {
        getValues(function, srcs, dst);
        len = dst;
        len->setName("len");
        dst = srcs.back();
        srcs.pop_back();
        dst->setName("dst");
    }
};

class FunctionBuilder
{
    static vector<string> descriptions;
    static ExecutionEngine *executionEngine;

public:
    static void *makeFunction(const string &description, uint8_t arity)
    {
        if (TheModule == NULL) initialize();

        Function *function = TheModule->getFunction(description);
        if (function != NULL)
            return executionEngine->getPointerToFunction(function);

        function = getFunction(description, arity, Type::getVoidTy(getGlobalContext()));

        static Function *makeAllocationFunction = NULL;
        static vector<PointerType*> allocationFunctionTypes(numArities, NULL);
        PointerType *allocationFunctionType = allocationFunctionTypes[arity];
        if (allocationFunctionType == NULL) {
            vector<Type*> allocationParams;
            for (int i = 0; i < arity+1; i++)
                allocationParams.push_back(PointerType::getUnqual(TheMatrixStruct));
            Type *allocationReturn = Type::getInt32Ty(getGlobalContext());
            allocationFunctionType = PointerType::getUnqual(FunctionType::get(allocationReturn, allocationParams, false));
            allocationFunctionTypes[arity] = allocationFunctionType;

            if (makeAllocationFunction == NULL) {
                vector<Type*> makeAllocationParams;
                makeAllocationParams.push_back(Type::getInt32Ty(getGlobalContext()));
                makeAllocationParams.push_back(Type::getInt8Ty(getGlobalContext()));
                makeAllocationParams.push_back(PointerType::getUnqual(TheMatrixStruct));
                FunctionType* makeAllocationType = FunctionType::get(allocationFunctionType, makeAllocationParams, true);
                makeAllocationFunction = Function::Create(makeAllocationType, GlobalValue::ExternalLinkage, "likely_make_allocation", TheModule);
                makeAllocationFunction->setCallingConv(CallingConv::C);
            }
        }
        GlobalVariable *allocationFunction = cast<GlobalVariable>(TheModule->getOrInsertGlobal(description+"_allocation", allocationFunctionType));
        allocationFunction->setInitializer(ConstantPointerNull::get(allocationFunctionType));

        static Function *makeKernelFunction = NULL;
        static vector<PointerType*> kernelFunctionTypes(numArities, NULL);
        PointerType *kernelFunctionType = kernelFunctionTypes[arity];
        if (makeKernelFunction == NULL) {
            vector<Type*> kernelParams;
            for (int i=0; i < arity+1; i++)
                kernelParams.push_back(PointerType::getUnqual(TheMatrixStruct));
            kernelParams.push_back(Type::getInt32Ty(getGlobalContext()));
            Type *kernelReturn = Type::getVoidTy(getGlobalContext());
            kernelFunctionTypes[arity] = kernelFunctionType;

            if (makeKernelFunction == NULL) {
                kernelFunctionType = PointerType::getUnqual(FunctionType::get(kernelReturn, kernelParams, false));
                vector<Type*> makeKernelParams;
                makeKernelParams.push_back(Type::getInt32Ty(getGlobalContext()));
                makeKernelParams.push_back(Type::getInt8Ty(getGlobalContext()));
                makeKernelParams.push_back(PointerType::getUnqual(TheMatrixStruct));
                FunctionType* makeUnaryKernelType = FunctionType::get(kernelFunctionType, makeKernelParams, true);
                makeKernelFunction = Function::Create(makeUnaryKernelType, GlobalValue::ExternalLinkage, "likely_make_kernel", TheModule);
                makeKernelFunction->setCallingConv(CallingConv::C);
            }
        }
        GlobalVariable *kernelFunction = cast<GlobalVariable>(TheModule->getOrInsertGlobal(description+"_kernel", kernelFunctionType));
        kernelFunction->setInitializer(ConstantPointerNull::get(kernelFunctionType));

        vector<Value*> srcs;
        Value *dst;
        KernelBuilder::getValues(function, srcs, dst);

        BasicBlock *entry = BasicBlock::Create(getGlobalContext(), "entry", function);
        IRBuilder<> builder(entry);

        vector<GlobalVariable*> kernelHashes;
        for (int i = 0; i < arity; i++) {
            GlobalVariable *kernelHash = cast<GlobalVariable>(TheModule->getOrInsertGlobal(description+"_hash"+to_string(i), Type::getInt16Ty(getGlobalContext())));
            kernelHash->setInitializer(MatrixBuilder::constant(0, 16));
            kernelHashes.push_back(kernelHash);
        }

        vector<Value*> srcHashes;
        for (int i = 0; i < arity; i++)
            srcHashes.push_back(builder.CreateLoad(builder.CreateStructGEP(srcs[i], 5), "src_hash"+to_string(i)));

        Value *hashTest = MatrixBuilder::constant(true);
        for (int i = 0; i < arity; i++)
            hashTest = builder.CreateAnd(hashTest, builder.CreateICmpEQ(builder.CreateLoad(kernelHashes[i]), srcHashes[i]));

        BasicBlock *hashFail = BasicBlock::Create(getGlobalContext(), "hash_fail", function);
        BasicBlock *execute = BasicBlock::Create(getGlobalContext(), "execute", function);
        builder.CreateCondBr(hashTest, execute, hashFail);

        builder.SetInsertPoint(hashFail);
        {
            descriptions.push_back(description);
            vector<Value*> args;
            args.push_back(MatrixBuilder::constant(descriptions.size()-1, 32));
            args.push_back(MatrixBuilder::constant(arity, 8));
            args.insert(args.end(), srcs.begin(), srcs.end());
            args.push_back(ConstantPointerNull::getNullValue(TheMatrixStruct));
            builder.CreateStore(builder.CreateCall(makeAllocationFunction, args), allocationFunction);
            builder.CreateStore(builder.CreateCall(makeKernelFunction, args), kernelFunction);
            for (int i = 0; i < arity; i++)
                builder.CreateStore(srcHashes[i], kernelHashes[i]);
            builder.CreateBr(execute);
        }

        builder.SetInsertPoint(execute);
        {
            vector<Value*> args(srcs); args.push_back(dst);
            Value *kernelSize = builder.CreateCall(builder.CreateLoad(allocationFunction), args);

            static Function *malloc = TheModule->getFunction("malloc");
            if (malloc == NULL) {
                Type *mallocReturn = Type::getInt8PtrTy(getGlobalContext());
                vector<Type*> mallocParams;
                mallocParams.push_back(Type::getInt32Ty(getGlobalContext()));
                FunctionType* mallocType = FunctionType::get(mallocReturn, mallocParams, false);
                malloc = Function::Create(mallocType, GlobalValue::ExternalLinkage, "malloc", TheModule);
                malloc->setCallingConv(CallingConv::C);
            }
            builder.CreateStore(builder.CreateCall(malloc, kernelSize), builder.CreateStructGEP(dst, 0));

            args.push_back(kernelSize);
            builder.CreateCall(builder.CreateLoad(kernelFunction), args);
            builder.CreateRetVoid();
        }

        optimize(function);
        return executionEngine->getPointerToFunction(function);
    }

    static void *makeAllocation(int descriptionIndex, const vector<likely_matrix*> &matricies)
    {
        const string description = descriptions[descriptionIndex];
        const string name = mangledName(description, matricies)+"_allocation";

        Function *function = TheModule->getFunction(name);
        if (function != NULL)
            return executionEngine->getPointerToFunction(function);
        function = getFunction(name, matricies.size(), Type::getInt32Ty(getGlobalContext()));

        KernelBuilder kb(description);
        kb.makeAllocation(function, matricies);

        optimize(function);
        return executionEngine->getPointerToFunction(function);
    }

    static void *makeKernel(int descriptionIndex, const vector<likely_matrix*> &matricies)
    {
        const string description = descriptions[descriptionIndex];
        const string name = mangledName(description, matricies)+"_kernel";

        Function *function = TheModule->getFunction(name);
        if (function != NULL)
            return executionEngine->getPointerToFunction(function);
        function = getFunction(name, matricies.size(), Type::getVoidTy(getGlobalContext()), Type::getInt32Ty(getGlobalContext()));

        KernelBuilder kb(description);
        kb.makeKernel(function, matricies);

        optimize(function);
        return executionEngine->getPointerToFunction(function);
    }

private:
    static void initialize()
    {
        InitializeNativeTarget();
        TheModule = new Module("likely", getGlobalContext());

        string error;
        executionEngine = EngineBuilder(TheModule).setEngineKind(EngineKind::JIT).setErrorStr(&error).create();
        if (executionEngine == NULL) {
            fprintf(stderr, "ERROR - Failed to create LLVM ExecutionEngine with error: %s", error.c_str());
            abort();
        }

        TheMatrixStruct = StructType::create("Matrix",
                                             Type::getInt8PtrTy(getGlobalContext()), // data
                                             Type::getInt32Ty(getGlobalContext()),   // channels
                                             Type::getInt32Ty(getGlobalContext()),   // columns
                                             Type::getInt32Ty(getGlobalContext()),   // rows
                                             Type::getInt32Ty(getGlobalContext()),   // frames
                                             Type::getInt16Ty(getGlobalContext()),   // hash
                                             NULL);
    }

    static void optimize(Function *f)
    {
        static FunctionPassManager *functionPassManager = NULL;
        static FunctionPassManager *extraFunctionPassManager = NULL;

        if (functionPassManager == NULL) {
            functionPassManager = new FunctionPassManager(TheModule);
            functionPassManager->add(createVerifierPass(PrintMessageAction));
            functionPassManager->add(createEarlyCSEPass());
            functionPassManager->add(createInstructionCombiningPass());
            functionPassManager->add(createDeadCodeEliminationPass());
            functionPassManager->add(createGVNPass());
            functionPassManager->add(createDeadInstEliminationPass());

            extraFunctionPassManager = new FunctionPassManager(TheModule);
//            extraFunctionPassManager->add(createPrintFunctionPass("----------------------------------------"
//                                                                  "----------------------------------------", &errs()));
//            TheExtraFunctionPassManager->add(createLoopUnrollPass(INT_MAX,8));
        }

        while (functionPassManager->run(*f));
        extraFunctionPassManager->run(*f);
    }

    static string mangledName(const string &description, const vector<likely_matrix*> &matrices)
    {
        stringstream stream; stream << description;
        for (likely_matrix *matrix : matrices)
            stream << "_" << hex << setfill('0') << setw(4) << matrix->hash;
        return stream.str();
    }

    static Function *getFunction(const string &description, int arity, Type *returnType, Type *indexType = NULL)
    {
        PointerType *matrixPointer = PointerType::getUnqual(TheMatrixStruct);
        Function *function;
        switch (arity) {
          case 0: function = cast<Function>(TheModule->getOrInsertFunction(description, returnType, matrixPointer, indexType, NULL)); break;
          case 1: function = cast<Function>(TheModule->getOrInsertFunction(description, returnType, matrixPointer, matrixPointer, indexType, NULL)); break;
          case 2: function = cast<Function>(TheModule->getOrInsertFunction(description, returnType, matrixPointer, matrixPointer, matrixPointer, indexType, NULL)); break;
          case 3: function = cast<Function>(TheModule->getOrInsertFunction(description, returnType, matrixPointer, matrixPointer, matrixPointer, matrixPointer, indexType, NULL)); break;
          default: fprintf(stderr, "ERROR - Invalid function arity: %d\n", arity); abort();
        }
        function->setCallingConv(CallingConv::C);
        return function;
    }
};

vector<string> FunctionBuilder::descriptions;
ExecutionEngine *FunctionBuilder::executionEngine;

} // namespace likely

void *likely_make_function(const char *description, uint8_t arity)
{
    return likely::FunctionBuilder::makeFunction(description, arity);
}

extern "C" {

LIKELY_EXPORT void *likely_make_allocation(int descriptionIndex, uint8_t arity, likely_matrix *src, ...)
{
    vector<likely_matrix*> srcs;
    va_list ap;
    va_start(ap, src);
    while (src != NULL) {
        srcs.push_back(src);
        src = va_arg(ap, likely_matrix*);
    }
    va_end(ap);
    assert(arity == srcs.size());
    return likely::FunctionBuilder::makeAllocation(descriptionIndex, srcs);
}

LIKELY_EXPORT void *likely_make_kernel(int descriptionIndex, uint8_t arity, likely_matrix *src, ...)
{
    vector<likely_matrix*> srcs;
    va_list ap;
    va_start(ap, src);
    while (src != NULL) {
        srcs.push_back(src);
        src = va_arg(ap, likely_matrix*);
    }
    va_end(ap);
    assert(arity == srcs.size());
    return likely::FunctionBuilder::makeKernel(descriptionIndex, srcs);
}

} // extern "C"
