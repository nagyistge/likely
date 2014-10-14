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

#ifndef LIKELY_IO_H
#define LIKELY_IO_H

#include <likely/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*!
 * \defgroup io I/O
 * \brief Read, write and render matricies (\c likely/io.h).
 * @{
 */

/*!
 * \brief How to read a file from disk.
 *
 * Available options are listed in \ref likely_file_type_mask.
 * \see likely_read
 */
typedef uint32_t likely_file_type;

/*!
 * \brief \ref likely_file_type bit format.
 */
enum likely_file_type_mask
{
    likely_file_decoded = 0x00000001, /*!< \brief The file is a \ref likely_matrix, do not decode it. */
    likely_file_encoded = 0x00000002, /*!< \brief The file is an image, image set or video, decode it. */
    likely_file_binary  = likely_file_decoded | likely_file_encoded, /*!< \brief The file is either \ref likely_file_decoded or \ref likely_file_encoded. */
    likely_file_text    = 0x00000004, /*!< \brief The file is text, do not decode it. */
    likely_file_url     = 0x00000008, /*!< \brief The file is not on the local file system, download it instead. */
};

/*!
 * \brief Read a \ref likely_matrix from a file.
 * \param[in] file_name The name of the file to open and read.
 * \param[in] type How to process the file after reading.
 * \return Pointer to the new \ref likely_matrix constructed from the file, or \c NULL if the file could not be processed.
 * \see likely_write
 */
LIKELY_EXPORT likely_mat likely_read(const char *file_name, likely_file_type type);

/*!
 * \brief Write a \ref likely_matrix to a file.
 *
 * The file extension in \p file_name is used to determine the desired file format.
 * \param[in] image The matrix to write.
 * \param[in] file_name The file to write the matrix to.
 * \return \p image if successful, \c NULL otherwise.
 * \see likely_read
 */
LIKELY_EXPORT likely_mat likely_write(likely_const_mat image, const char *file_name);

/*!
 * \brief Decode a \ref likely_matrix.
 *
 * The format of \p buffer is determined automatically.
 * \param[in] buffer The buffer to decode.
 * \return Decoded image if successful, \c NULL otherwise.
 * \see likely_encode
 */
LIKELY_EXPORT likely_mat likely_decode(likely_const_mat buffer);

/*!
 * \brief Encode a \ref likely_matrix.
 *
 * \param[in] image The image to encode.
 * \param[in] extension The desired encoding format.
 * \return Encoded buffer if successful, \c NULL otherwise.
 * \see likely_decode
 */
LIKELY_EXPORT likely_mat likely_encode(likely_const_mat image, const char *extension);

/*!
 * \brief Convert a \ref likely_matrix to a string suitable for printing.
 * \param[in] mat Matrix to stringify.
 * \return Stringified matrix.
 * \see likely_to_string_n
 */
LIKELY_EXPORT likely_mat likely_to_string(likely_const_mat mat);

/*!
 * \brief Convert an array of \ref likely_matrix to a string suitable for printing.
 * \param[in] mats Vector of matricies to stringify.
 * \param[in] n Length of \p mats.
 * \return Stringified matrix.
 * \see likely_to_string likely_to_string_va
 */
LIKELY_EXPORT likely_mat likely_to_string_n(likely_const_mat *mats, size_t n);

/*!
 * \brief Convert a list of \ref likely_matrix to a string suitable for printing.
 * \param[in] mat <tt>NULL</tt>-terminated list of matricies to stringify.
 * \return Stringified matrix.
 * \see likely_to_string_n
 */
LIKELY_EXPORT likely_mat likely_to_string_va(likely_const_mat mat, ...);

/*!
 * \brief Convert a \ref likely_matrix to three channels with 8-bit depth suitable for displaying.
 * \param[in] mat Matrix to convert.
 * \param[out] min The minimum value in \p mat. May be \c NULL.
 * \param[out] max The maximum value in \p mat. May be \c NULL.
 * \return A three-channel 8-bit matrix suitable for displaying.
 */
LIKELY_EXPORT likely_mat likely_render(likely_const_mat mat, double *min, double *max);

LIKELY_EXPORT void likely_show(likely_const_mat m, const char *title);
LIKELY_EXPORT void likely_show_callback(likely_const_env env, void *); // Useable as a likely_repl_callback

/** @} */ // end of io

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // LIKELY_IO_H
