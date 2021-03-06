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

#ifndef LIKELY_FRONTEND_H
#define LIKELY_FRONTEND_H

#include <stddef.h>
#include <likely/runtime.h>
#include <likely/io.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*!
 * \defgroup frontend Frontend
 * \brief Parse source code into an abstract syntax tree (\c likely/frontend.h).
 * @{
 */

/*!
 * \brief How to interpret a \ref likely_abstract_syntax_tree.
 *
 * Available options are listed in \ref likely_abstract_syntax_tree_types.
 */
typedef uint32_t likely_abstract_syntax_tree_type;

/*!
 * \brief \ref likely_abstract_syntax_tree_type options.
 */
enum likely_abstract_syntax_tree_types
{
    likely_ast_list     = 0, /*!< likely_abstract_syntax_tree::atoms and likely_abstract_syntax_tree::num_atoms are accessible. */
    likely_ast_atom     = 1, /*!< likely_abstract_syntax_tree::atom and likely_abstract_syntax_tree::atom_len are accessible. */
    likely_ast_operator = 2, /*!< A \ref likely_ast_atom identified as a operator during evaluation. */
    likely_ast_operand  = 3, /*!< A \ref likely_ast_atom identified as a operand during evaluation. */
    likely_ast_integer  = 4, /*!< A \ref likely_ast_atom identified as an integer during evaluation. */
    likely_ast_real     = 5, /*!< A \ref likely_ast_atom identified as a real during evaluation. */
    likely_ast_string   = 6, /*!< A \ref likely_ast_atom identified as a string during evaluation. */
    likely_ast_type     = 7, /*!< A \ref likely_ast_atom identified as a type during evaluation. */
    likely_ast_invalid  = 8, /*!< A \ref likely_ast_atom that could not be identified during evaluation. */
};

typedef struct likely_abstract_syntax_tree *likely_ast; /*!< \brief Pointer to a \ref likely_abstract_syntax_tree. */
typedef struct likely_abstract_syntax_tree const *likely_const_ast; /*!< \brief Pointer to a constant \ref likely_abstract_syntax_tree. */

/*!
 * \brief An abstract syntax tree.
 *
 * The \ref likely_abstract_syntax_tree represents the final state of source code after tokenization and parsing.
 * This structure is provided to the backend for code generation.
 *
 * The \ref likely_abstract_syntax_tree is designed as a node in a \a tree, where each node is either a list or an atom.
 * A \a list is an array of \ref likely_ast which themselves are lists or atoms.
 * An \a atom is a single-word string, also called a \a token.
 * In tree-terminology a list is a \a branch, and an atom is a \a leaf.
 *
 * In Likely source code, parenthesis, periods and colons are used to construct lists, and everything else is an atom.
 *
 * \par Abstract Syntax Tree Construction
 * | Function                  | Description                     |
 * |---------------------------|---------------------------------|
 * | \ref likely_atom          | \copybrief likely_atom          |
 * | \ref likely_list          | \copybrief likely_list          |
 * | \ref likely_lex           | \copybrief likely_lex           |
 * | \ref likely_parse         | \copybrief likely_parse         |
 * | \ref likely_lex_and_parse | \copybrief likely_lex_and_parse |
 *
 * \see \ref reference_counting
 */
struct likely_abstract_syntax_tree
{
    union {
        struct
        {
            const likely_ast * const atoms; /*!< \brief List elements. */
            uint32_t num_atoms; /*!< \brief Length of \ref atoms. */
        }; /*!< \brief Accessible when <tt>\ref type == \ref likely_ast_list</tt>. */
        struct
        {
            const char * const atom; /*!< \brief <tt>NULL</tt>-terminated single-word token. */
            uint32_t atom_len; /*!< \brief Length of \ref atom, excluding the <tt>NULL</tt>-terminator. */
        }; /*!< \brief Accessible when <tt>\ref type != \ref likely_ast_list</tt>. */
    }; /*!< \brief A list or an atom. */
    likely_const_ast parent; /*!< \brief This node's predecessor, or \c NULL if this node is the root. */
    uint32_t ref_count; /*!< \brief Reference count used by \ref likely_retain_ast and \ref likely_release_ast to track ownership. */
    likely_abstract_syntax_tree_type type; /*!< \brief Interpretation of \ref likely_abstract_syntax_tree. */
    uint32_t begin_line; /*!< \brief Source code beginning line number. */
    uint32_t begin_column; /*!< \brief Source code beginning column number. */
    uint32_t end_line; /*!< \brief Source code ending line number. */
    uint32_t end_column; /*!< \brief Source code ending column number (inclusive). */
};

/*!
 * \brief Construct a new atom from a string.
 * \param[in] atom The string to copy for \ref likely_abstract_syntax_tree::atom.
 * \param[in] atom_len The length of \p atom to copy, and the value for \ref likely_abstract_syntax_tree::atom_len.
 * \return A pointer to the new \ref likely_abstract_syntax_tree, or \c NULL if \c malloc failed.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_list
 */
LIKELY_EXPORT likely_ast likely_atom(const char *atom, uint32_t atom_len);

/*!
 * \brief Construct a new list from an array of atoms.
 * \note This function takes ownership of \p atoms. Call \ref likely_retain_ast for elements in \p atoms where you wish to retain a copy.
 * \param[in] atoms The atoms to take for \ref likely_abstract_syntax_tree::atoms.
 * \param[in] num_atoms The length of \p atoms, and the value for \ref likely_abstract_syntax_tree::num_atoms.
 * \return A pointer to the new \ref likely_abstract_syntax_tree, or \c NULL if \c malloc failed.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_atom
 */
LIKELY_EXPORT likely_ast likely_list(const likely_ast *atoms, uint32_t num_atoms);

/*!
 * \brief Retain a reference to an abstract syntax tree.
 *
 * Increments \ref likely_abstract_syntax_tree::ref_count.
 * \param[in] ast Abstract syntax tree to add a reference. May be \c NULL.
 * \return \p ast.
 * \remark This function is \ref reentrant.
 * \see \ref likely_release_ast
 */
LIKELY_EXPORT likely_ast likely_retain_ast(likely_const_ast ast);

/*!
 * \brief Release a reference to an abstract syntax tree.
 *
 * Decrements \ref likely_abstract_syntax_tree::ref_count.
 * \param[in] ast Abstract syntax tree to subtract a reference. May be \c NULL.
 * \remark This function is \ref reentrant.
 * \see \ref likely_retain_ast
 */
LIKELY_EXPORT void likely_release_ast(likely_const_ast ast);

typedef struct likely_error *likely_err; /*!< \brief Pointer to a \ref likely_error. */
typedef struct likely_error const *likely_const_err; /*!< \brief Pointer to a constant \ref likely_error. */

/*!
 * \brief A compilation error.
 * \par Error Construction
 * | Function            | Description               |
 * |---------------------|---------------------------|
 * | \ref likely_erratum | \copybrief likely_erratum |
 *
 * \see \ref reference_counting
 */
struct likely_error
{
    likely_const_err parent; /*!< \brief Predecessor error, or \c NULL if this error is the root. */
    uint32_t ref_count; /*!< \brief Reference count used by \ref likely_retain_err and \ref likely_release_err to track ownership. */
    likely_const_ast where; /*!< \brief Location of the error. */
    char what[]; /*!< \brief Error message. */
};

/*!
 * \brief Signature of a function to call when a compilation error occurs.
 * \see \ref likely_set_error_callback
 */
typedef void (*likely_error_callback)(likely_err err, void *context);

/*!
 * \brief Construct a new error.
 * \param[in] parent A previous error that caused this error. May be \c NULL.
 * \param[in] where \ref likely_error::where.
 * \param[in] format <tt>printf</tt>-style string to populate \ref likely_error::what.
 * \return Pointer to a new \ref likely_error, or \c NULL if \c malloc failed.
 * \remark This function is \ref reentrant.
 */
LIKELY_EXPORT likely_err likely_erratum(likely_const_err parent, likely_const_ast where, const char *format, ...);

/*!
 * \brief Retain a reference to an error.
 *
 * Increments \ref likely_error::ref_count.
 * \param[in] err Error to add a reference. May be \c NULL.
 * \return \p err.
 * \remark This function is \ref reentrant.
 * \see \ref likely_release_err
 */
LIKELY_EXPORT likely_err likely_retain_err(likely_const_err err);

/*!
 * \brief Release a reference to an error.
 *
 * Decrements \ref likely_error::ref_count.
 * \param[in] err Error to subtract a reference. May be \c NULL.
 * \remark This function is \ref reentrant.
 * \see \ref likely_retain_err
 */
LIKELY_EXPORT void likely_release_err(likely_const_err err);

/*!
 * \brief Assign the function to call when a compilation error occurs.
 *
 * By default, the error is converted to a string using \ref likely_err_to_string and printed to \c stderr.
 * \param[in] callback The function to call when a compilation error occurs.
 * \param[in] context User-defined data to pass to \p callback.
 * \remark This function is \ref thread-safe.
 */
LIKELY_EXPORT void likely_set_error_callback(likely_error_callback callback, void *context);

/*!
 * \brief Trigger a recoverable error.
 *
 * Calls the \ref likely_error_callback set by \ref likely_set_error_callback.
 * \param[in] where Location of the error.
 * \param[in] what Description of the error.
 * \return \c false.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_ensure
 */
LIKELY_EXPORT bool likely_throw(likely_const_ast where, const char *what);

/*!
 * \brief Convert the error to a string suitable for printing.
 * \param[in] err The error to convert to a string.
 * \return A \ref likely_string.
 * \remark This function is \ref thread-safe.
 */
LIKELY_EXPORT likely_mat likely_err_to_string(likely_err err);

/*!
 * \brief Perform lexical analysis, converting source code into a list of tokens.
 *
 * The output from this function is usually the input to \ref likely_parse.
 * \param[in] source Code from which to extract tokens.
 * \param[in] file_type How to interpret \p source.
 * \return A list of tokens extracted from \p source.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_lex_and_parse
 */
LIKELY_EXPORT likely_ast likely_lex(const char *source, likely_file_type file_type);

/*!
 * \brief Perform syntactic analysis, converting a list of tokens into an abstract syntax tree.
 *
 * The input to this function is usually the output from \ref likely_lex.
 * \param[in] tokens List of tokens from which build the abstract syntax tree.
 * \return An abstract syntax tree built from \p tokens.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_lex_and_parse
 */
LIKELY_EXPORT likely_ast likely_parse(likely_const_ast tokens);

/*!
 * \brief Convenient alternative to \ref likely_lex followed by \ref likely_parse.
 * \par Implementation
 * \snippet src/frontend.cpp likely_lex_and_parse implementation.
 * \param[in] source Code from which to extract tokens and build the abstract syntax tree.
 * \param[in] file_type How to interpret \p source when extracting tokens.
 * \return An abstract syntax tree built from \p source.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_ast_to_string \ref likely_lex_parse_and_eval
 */
LIKELY_EXPORT likely_ast likely_lex_and_parse(const char *source, likely_file_type file_type);

/*!
 * \brief Convert an abstract syntax tree into a string.
 *
 * The opposite of \ref likely_lex_and_parse.
 * The returned \ref likely_matrix::data is valid \ref likely_file_lisp code.
 * \param[in] ast The abstract syntax tree to convert into a string.
 * \param[in] depth Maximum levels of \p ast to print or \c -1 for all.
 *                  Unprinted atoms are represented with a single space.
 * \return A \ref likely_string.
 * \remark This function is \ref thread-safe.
 */
LIKELY_EXPORT likely_mat likely_ast_to_string(likely_const_ast ast, int depth);

/*!
 * \brief Compare two abstract syntax trees.
 * \param[in] a Abstract syntax tree to be compared.
 * \param[in] b Abstract syntax tree to be compared.
 * \return <tt>-1 if (a < b), 0 if (a == b), 1 if (a > b)</tt>.
 * \remark This function is \ref thread-safe.
 */
LIKELY_EXPORT int likely_ast_compare(likely_const_ast a, likely_const_ast b);

/*!
 * \brief Returns true if the \ref likely_abstract_syntax_tree::atom is an assignment.
 *
 * \par Implementation
 * \snippet src/frontend.cpp likely_is_definition implementation.
 * \param[in] ast The abstract syntax tree to examine.
 * \return \c true if \param ast is an assignment, \c false otherwise.
 */
 LIKELY_EXPORT bool likely_is_definition(likely_const_ast ast);

/*!
 * \brief Find the first \ref likely_abstract_syntax_tree::atom that isn't an assignment (=) operator.
 *
 * Searches using an in order traversal of \p ast.
 * \param[in] ast The abstract syntax tree to traverse.
 * \return The first \ref likely_abstract_syntax_tree::atom that isn't an assignment (=) operator.
 * \remark This function is \ref thread-safe.
 */
LIKELY_EXPORT const char *likely_symbol(likely_const_ast ast);

/*!
 * \brief Convert a \ref likely_type to a string.
 *
 * The opposite of \ref likely_type_from_string.
 * The returned \ref likely_matrix::data is valid \ref likely_file_lisp code.
 * \param[in] type The type to convert to a string.
 * \return A \ref likely_string.
 * \remark This function is \ref thread-safe.
 */
LIKELY_EXPORT likely_mat likely_type_to_string(likely_type type);

/*!
 * \brief Convert a string to a \ref likely_type.
 *
 * The opposite of \ref likely_type_to_string.
 * \par Implementation
 * \snippet src/frontend.cpp likely_type_from_string implementation.
 * \param[in] str String to convert to a \ref likely_type.
 * \param[out] ok Successful conversion. May be \c NULL.
 * \return A \ref likely_type from \p str on success, \ref likely_void otherwise.
 * \remark This function is \ref thread-safe.
 */
LIKELY_EXPORT likely_type likely_type_from_string(const char *str, bool *ok);

/*!
 * \brief Determine the appropriate \ref likely_type for a binary operation.
 * \par Implementation
 * \snippet src/frontend.cpp likely_type_from_types implementation.
 * \param[in] a Type to be consolidated.
 * \param[in] b Type to be consolidated.
 * \return The appropriate \ref likely_type for an operation involving \p a and \p b.
 * \remark This function is \ref thread-safe.
 */
LIKELY_EXPORT likely_type likely_type_from_types(likely_type a, likely_type b);

/*!
 * \brief Construct a pointer type.
 * \param[in] element_type Element type.
 * \return Compound pointer type.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_element_type
 */
LIKELY_EXPORT likely_type likely_pointer_type(likely_type element_type);

/*!
 * \brief Retrieve the element type of a pointer.
 * \param[in] pointer_type Compound pointer type.
 * \return Element type.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_pointer_type
 */
LIKELY_EXPORT likely_type likely_element_type(likely_type pointer_type);

/*!
 * \brief Construct a struct type.
 * \param[in] name Struct name.
 * \param[in] member_types Member types. May be \c NULL if \p members is \c 0.
 * \param[in] members Length of \p member_types.
 * \return Compound struct type.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_member_types
 */
LIKELY_EXPORT likely_type likely_struct_type(const char *name, const likely_type *member_types, uint32_t members);

/*!
 * \brief Get the name of a struct.
 * \param[in] struct_type Compoint struct type.
 * \return Struct name as a \ref likely_string.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_struct_type
 */
LIKELY_EXPORT likely_mat likely_struct_name(likely_type struct_type);

/*!
 * \brief Get the number of members in a struct.
 * \param[in] struct_type Compound struct type.
 * \return The number of members in \p struct_type.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_struct_type
 */
LIKELY_EXPORT uint32_t likely_struct_members(likely_type struct_type);

/*!
 * \brief Retrieve the member types of a struct.
 * \param[in] struct_type Compound struct type.
 * \param[out] member_types Member types, large enough to hold \ref likely_struct_members elements. May be \c NULL if \ref likely_struct_members is \c 0.
 * \remark This function is \ref thread-safe.
 * \see \ref likely_struct_type
 */
LIKELY_EXPORT void likely_member_types(likely_type struct_type, likely_type *member_types);

/** @} */ // end of frontend

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // LIKELY_FRONTEND_H
