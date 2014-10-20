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
    likely_ast_string   = 3, /*!< A \ref likely_ast_atom identified as a string during evaluation. */
    likely_ast_number   = 4, /*!< A \ref likely_ast_atom identified as a number during evaluation. */
    likely_ast_type     = 5, /*!< A \ref likely_ast_atom identified as a type during evaluation. */
    likely_ast_invalid  = 6, /*!< A \ref likely_ast_atom that could not be identified during evaluation. */
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
 */
struct likely_abstract_syntax_tree
{
    /*!
     * \brief A list or an atom.
     */
    union {
        /*!
         * \brief Accessible when <tt>\ref type == \ref likely_ast_list</tt>.
         */
        struct
        {
            const likely_ast * const atoms; /*!< \brief List elements. */
            uint32_t num_atoms; /*!< \brief Length of \ref atoms. */
        };

        /*!
         * \brief Accessible when <tt>\ref type != \ref likely_ast_list</tt>.
         */
        struct
        {
            const char * const atom; /*!< \brief <tt>NULL</tt>-terminated single-word token. */
            uint32_t atom_len; /*!< \brief Length of \ref atom, excluding the <tt>NULL</tt>-terminator. */
        };
    };

    likely_const_ast parent; /*!< \brief This node's predecessor, or \c NULL if this node is the root. */
    uint32_t ref_count; /*!< \brief Reference count.
                         *
                         * Used by \ref likely_retain_ast and \ref likely_release_ast to track ownership.
                         */
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
 * \see likely_new_list
 */
LIKELY_EXPORT likely_ast likely_new_atom(const char *atom, uint32_t atom_len);

/*!
 * \brief Construct a new list from an array of atoms.
 * \note This function takes ownership of \p atoms. Call \ref likely_retain_ast for elements in \p atoms where you wish to retain a copy.
 * \param[in] atoms The atoms to take for \ref likely_abstract_syntax_tree::atoms.
 * \param[in] num_atoms The length of \p atoms, and the value for \ref likely_abstract_syntax_tree::num_atoms.
 * \return A pointer to the new \ref likely_abstract_syntax_tree, or \c NULL if \c malloc failed.
 * \see likely_new_atom
 */
LIKELY_EXPORT likely_ast likely_new_list(const likely_ast *atoms, likely_size num_atoms);

/*!
 * \brief Retain a reference to an abstract syntax tree.
 *
 * Increments \ref likely_abstract_syntax_tree::ref_count.
 * \param[in] ast Abstract syntax tree to add a reference. May be \c NULL.
 * \return \p ast.
 * \see likely_release_ast
 */
LIKELY_EXPORT likely_ast likely_retain_ast(likely_const_ast ast);

/*!
 * \brief Release a reference to an abstract syntax tree.
 *
 * Decrements \ref likely_abstract_syntax_tree::ref_count.
 * \param[in] ast Abstract syntax tree to subtract a reference. May be \c NULL.
 * \see likely_retain_ast
 */
LIKELY_EXPORT void likely_release_ast(likely_const_ast ast);

typedef struct likely_error *likely_err; /*!< \brief Pointer to a \ref likely_error. */
typedef struct likely_error const *likely_const_err; /*!< \brief Pointer to a constant \ref likely_error. */

/*!
 * \brief A compilation error.
 */
struct likely_error
{
    likely_const_err parent; /*!< \brief Predecessor error, or \c NULL if this error is the root. */
    uint32_t ref_count; /*!< \brief Reference count.
                         *
                         * Used by \ref likely_retain_err and \ref likely_release_err to track ownership.
                         */
    likely_const_ast where; /*!< \brief Location of the error. */
    char what[]; /*!< \brief Error message. */
};
typedef void (*likely_error_callback)(likely_err err, void *context);

/*!
 * \brief Construct a new error.
 * \param[in] where \ref likely_error::where.
 * \param[in] format <tt>printf</tt>-style string to populate \ref likely_error::what.
 * \return Pointer to a new \ref likely_error, or \c NULL if \c malloc failed.
 */
LIKELY_EXPORT likely_err likely_new_err(likely_const_err parent, likely_const_ast where, const char *format, ...);

/*!
 * \brief Retain a reference to an error.
 *
 * Increments \ref likely_error::ref_count.
 * \param[in] err Error to add a reference. May be \c NULL.
 * \return \p err.
 * \see likely_release_err
 */
LIKELY_EXPORT likely_err likely_retain_err(likely_const_err err);

/*!
 * \brief Release a reference to an error.
 *
 * Decrements \ref likely_error::ref_count.
 * \param[in] err Error to subtract a reference. May be \c NULL.
 * \see likely_retain_err
 */
LIKELY_EXPORT void likely_release_err(likely_const_err err);

/*!
 * \brief Conditional abort-style error handling with an error message.
 * \param[in] condition If \c false, print \a format and abort.
 * \param[in] format <tt>printf</tt>-style error message.
 */
LIKELY_EXPORT void likely_assert(bool condition, const char *format, ...);

/*!
 * \brief How to interpret source code.
 *
 * Available options are listed in \ref likely_source_types.
 */
typedef uint32_t likely_source_type;

/*!
 * \brief \ref likely_source_type options.
 */
enum likely_source_types
{
    likely_source_lisp = 0, /*!< Plain source code. */
    likely_source_gfm  = 1, /*!< Source is in [GitHub Flavored Markdown](https://help.github.com/articles/github-flavored-markdown/) code blocks. */
};

/*!
 * \brief Performs lexical analysis, converting source code into a list of tokens.
 * \param[in] source Code from which to extract tokens.
 * \param[in] type How to interpret \p source.
 * \return A list of tokens extracted from \p source.
 */
LIKELY_EXPORT likely_ast likely_lex(const char *source, likely_source_type type);

LIKELY_EXPORT likely_ast likely_ast_from_tokens(likely_const_ast tokens);
LIKELY_EXPORT likely_ast likely_ast_from_string(const char *str, likely_source_type type);
LIKELY_EXPORT likely_mat likely_ast_to_string(likely_const_ast ast);
LIKELY_EXPORT int likely_ast_compare(likely_const_ast a, likely_const_ast b);
LIKELY_EXPORT bool likely_ast_contains(likely_const_ast ast, likely_const_ast sub_ast);
LIKELY_EXPORT const char *likely_get_symbol_name(likely_const_ast ast); // return value valid for lifetime of ast

// Callback-style error handling
LIKELY_EXPORT void likely_set_error_callback(likely_error_callback callback, void *context);
LIKELY_EXPORT bool likely_throw(likely_const_ast where, const char *what);
LIKELY_EXPORT likely_mat likely_error_to_string(likely_err error);

// Type conversion
LIKELY_EXPORT likely_mat likely_type_to_string(likely_size type);
LIKELY_EXPORT likely_mat likely_type_field_to_string(likely_size type);
LIKELY_EXPORT likely_matrix_type likely_type_from_string(const char *str, bool *ok);
LIKELY_EXPORT likely_size likely_type_from_value(double value);
LIKELY_EXPORT likely_size likely_type_from_types(likely_size lhs, likely_size rhs);

/** @} */ // end of frontend

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // LIKELY_FRONTEND_H
