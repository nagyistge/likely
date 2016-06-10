; ModuleID = 'likely'
source_filename = "likely"

%u0Matrix = type { i32, i32, i32, i32, i32, i32, [0 x i8] }
%u8Matrix = type { i32, i32, i32, i32, i32, i32, [0 x i8] }

; Function Attrs: argmemonly nounwind
declare noalias %u0Matrix* @likely_new(i32 zeroext, i32 zeroext, i32 zeroext, i32 zeroext, i32 zeroext, i8* noalias nocapture) #0

; Function Attrs: norecurse nounwind
define private void @multiply_add_tmp_thunk0({ %u8Matrix*, %u8Matrix*, float, float }* noalias nocapture readonly, i64, i64) #1 {
entry:
  %3 = getelementptr inbounds { %u8Matrix*, %u8Matrix*, float, float }, { %u8Matrix*, %u8Matrix*, float, float }* %0, i64 0, i32 0
  %4 = load %u8Matrix*, %u8Matrix** %3, align 8
  %5 = getelementptr inbounds { %u8Matrix*, %u8Matrix*, float, float }, { %u8Matrix*, %u8Matrix*, float, float }* %0, i64 0, i32 1
  %6 = load %u8Matrix*, %u8Matrix** %5, align 8
  %7 = getelementptr inbounds { %u8Matrix*, %u8Matrix*, float, float }, { %u8Matrix*, %u8Matrix*, float, float }* %0, i64 0, i32 2
  %8 = load float, float* %7, align 4
  %9 = getelementptr inbounds { %u8Matrix*, %u8Matrix*, float, float }, { %u8Matrix*, %u8Matrix*, float, float }* %0, i64 0, i32 3
  %10 = load float, float* %9, align 4
  %11 = getelementptr inbounds %u8Matrix, %u8Matrix* %6, i64 0, i32 2
  %channels1 = load i32, i32* %11, align 4, !range !0
  %dst_c = zext i32 %channels1 to i64
  %12 = getelementptr inbounds %u8Matrix, %u8Matrix* %6, i64 0, i32 3
  %columns2 = load i32, i32* %12, align 4, !range !0
  %dst_x = zext i32 %columns2 to i64
  %13 = mul i64 %dst_c, %2
  %14 = mul i64 %13, %dst_x
  br label %y_body

y_body:                                           ; preds = %y_body, %entry
  %y = phi i64 [ %1, %entry ], [ %y_increment, %y_body ]
  %15 = getelementptr %u8Matrix, %u8Matrix* %6, i64 0, i32 6, i64 %y
  %16 = load i8, i8* %15, align 1, !llvm.mem.parallel_loop_access !1
  %17 = uitofp i8 %16 to float
  %18 = fmul fast float %17, %8
  %val = fadd fast float %18, %10
  %19 = getelementptr %u8Matrix, %u8Matrix* %4, i64 0, i32 6, i64 %y
  %20 = fcmp fast olt float %val, 0.000000e+00
  %. = select i1 %20, float -5.000000e-01, float 5.000000e-01
  %21 = fadd fast float %., %val
  %22 = fcmp fast ogt float %21, 0.000000e+00
  %23 = select i1 %22, float %21, float 0.000000e+00
  %24 = fptoui float %23 to i8
  %25 = fcmp fast ogt float %21, 2.550000e+02
  %26 = select i1 %25, i8 -1, i8 %24
  store i8 %26, i8* %19, align 1, !llvm.mem.parallel_loop_access !1
  %y_increment = add nuw nsw i64 %y, 1
  %y_postcondition = icmp eq i64 %y_increment, %14
  br i1 %y_postcondition, label %y_exit, label %y_body

y_exit:                                           ; preds = %y_body
  ret void
}

declare void @likely_fork(i8* noalias nocapture, i8* noalias nocapture, i64)

define %u8Matrix* @multiply_add(%u8Matrix*, float, float) {
entry:
  %3 = getelementptr inbounds %u8Matrix, %u8Matrix* %0, i64 0, i32 2
  %channels = load i32, i32* %3, align 4, !range !0
  %4 = getelementptr inbounds %u8Matrix, %u8Matrix* %0, i64 0, i32 3
  %columns = load i32, i32* %4, align 4, !range !0
  %5 = getelementptr inbounds %u8Matrix, %u8Matrix* %0, i64 0, i32 4
  %rows = load i32, i32* %5, align 4, !range !0
  %6 = call %u0Matrix* @likely_new(i32 29704, i32 %channels, i32 %columns, i32 %rows, i32 1, i8* null)
  %dst = bitcast %u0Matrix* %6 to %u8Matrix*
  %7 = zext i32 %rows to i64
  %8 = alloca { %u8Matrix*, %u8Matrix*, float, float }, align 8
  %9 = bitcast { %u8Matrix*, %u8Matrix*, float, float }* %8 to %u0Matrix**
  store %u0Matrix* %6, %u0Matrix** %9, align 8
  %10 = getelementptr inbounds { %u8Matrix*, %u8Matrix*, float, float }, { %u8Matrix*, %u8Matrix*, float, float }* %8, i64 0, i32 1
  store %u8Matrix* %0, %u8Matrix** %10, align 8
  %11 = getelementptr inbounds { %u8Matrix*, %u8Matrix*, float, float }, { %u8Matrix*, %u8Matrix*, float, float }* %8, i64 0, i32 2
  store float %1, float* %11, align 8
  %12 = getelementptr inbounds { %u8Matrix*, %u8Matrix*, float, float }, { %u8Matrix*, %u8Matrix*, float, float }* %8, i64 0, i32 3
  store float %2, float* %12, align 4
  %13 = bitcast { %u8Matrix*, %u8Matrix*, float, float }* %8 to i8*
  call void @likely_fork(i8* bitcast (void ({ %u8Matrix*, %u8Matrix*, float, float }*, i64, i64)* @multiply_add_tmp_thunk0 to i8*), i8* %13, i64 %7)
  ret %u8Matrix* %dst
}

attributes #0 = { argmemonly nounwind }
attributes #1 = { norecurse nounwind }

!0 = !{i32 1, i32 -1}
!1 = distinct !{!1}
