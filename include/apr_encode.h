/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file apr_encode.h
 * @brief APR-UTIL Encoding
 */
#ifndef APR_ENCODE_H
#define APR_ENCODE_H

#include "apr.h"
#include "apr_general.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup APR_Util_Encode Base64/Base64Url/Base32/Base32Hex/Base16 Encoding
 * @ingroup APR_Util
 * @{
 */

/**
 * RFC4648 and RFC7515 compliant BASE64, BASE64URL, BASE32, BASE32HEX
 * and BASE16 encode/decode functions.
 *
 * The following encodings are supported:
 *
 * - Base 64 Encoding
 *
 *   o Use flag APR_ENCODE_NONE
 *   o https://tools.ietf.org/html/rfc4648#section-4
 *
 * - Base 64 Encoding with URL and Filename Safe Alphabet
 *
 *   o Use flag APR_ENCODE_URL
 *   o https://tools.ietf.org/html/rfc4648#section-5
 *
 * - Base 64 URL Encoding without Padding
 *
 *   o Use flag APR_ENCODE_BASE64URL
 *   o https://tools.ietf.org/html/rfc7515#appendix-C
 *
 * - Base 32 Encoding
 *
 *   o Use flag APR_ENCODE_NONE
 *   o https://tools.ietf.org/html/rfc4648#section-6
 *
 * - Base 32 Encoding with Extended Hex Alphabet
 *
 *   o Use flag APR_ENCODE_BASE32HEX
 *   o https://tools.ietf.org/html/rfc4648#section-7
 *
 * - Base 16 Encoding
 *
 *   o Use flags APR_ENCODE_NONE/APR_ENCODE_COLON
 *   o https://tools.ietf.org/html/rfc4648#section-8
 *
 * If a non valid character of any kind including whitespace is passed to any
 * of the decoder functions, APR_BADCH will be returned. In this case decoding
 * will still take place, but the results can not be trusted.
 *
 * If APR_ENCODE_RELAXED is passed to the decoder functions, decoding will be
 * attempted up until the first non valid character. If this results in an
 * invalid state in the decoder, such as but not limited to an odd number of
 * base16 characters, APR_BADCH will still be returned.
 *
 * If APR_ENCODE_RELAXED is not passed to a decoder function, the decoding will
 * be done in constant time regardless of whether the result returns APR_SUCCESS
 * or APR_BADCH.
 *
 * If the dest parameter is NULL, the maximum theoretical buffer size is
 * returned in the len field, including space for a terminating zero character
 * if the destination is a string. This value can be used to allocate buffers
 * of a suitable safe size.
 *
 * If the dest parameter is provided, the encoding or decoding will take place,
 * and the actual number of characters written is returned in the len field,
 * ignoring any terminating zero.
 *
 * Plain strings are not assumed '\0' terminated unless APR_ENCODE_STRING is
 * provided.
 *
 */

/**
 * When passing a string to one of the encode functions, this value can be
 * passed to indicate a string-valued key, and have the length computed
 * automatically.
 */
#define APR_ENCODE_STRING      (-1)

/**
 * Generate RFC4648 base16/base32/base64.
 */
#define APR_ENCODE_NONE 0

/**
 * If relaxed, decode up until the first non base16/base32/base64 character.
 */
#define APR_ENCODE_RELAXED 1

/**
 * Omit the padding character (=) while encoding.
 */
#define APR_ENCODE_NOPADDING 2

/**
 * Generate RFC4648 Base 64 Encoding with URL and Filename Safe Alphabet
 */
#define APR_ENCODE_URL 4

/**
 * Generate RFC7515 BASE64URL
 */
#define APR_ENCODE_BASE64URL (APR_ENCODE_NOPADDING | APR_ENCODE_URL)

/**
 * Generate base32hex encoding instead of base32 encoding
 */
#define APR_ENCODE_BASE32HEX 8

/**
 * Generate base16 with colons between each token.
 */
#define APR_ENCODE_COLON 16

/**
 * Generate base16 with lower case characters.
 */
#define APR_ENCODE_LOWER 32

/**
 * Convert text data to base64.
 * @param dest The destination string, can be NULL.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 64 Encoding. If
 *  APR_ENCODE_NOPADDING, omit the = padding character.	If APR_ENCODE_URL,
 *  use RFC4648 Base 64 Encoding with URL and Filename Safe Alphabet.
 * 	If APR_ENCODE_BASE64URL, use RFC7515 base64url Encoding.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination string, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the string was NULL.
 */
APR_DECLARE(apr_status_t) apr_encode_base64(char *dest, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert binary data to base64.
 * @param dest The destination string, can be NULL.
 * @param src The original buffer.
 * @param slen The length of the original buffer.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 64 Encoding. If
 *  APR_ENCODE_NOPADDING, omit the = padding character.	If APR_ENCODE_URL,
 *  use RFC4648 Base 64 Encoding with URL and Filename Safe Alphabet.
 * 	If APR_ENCODE_BASE64URL, use RFC7515 base64url Encoding.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination string, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the string was NULL.
 */
APR_DECLARE(apr_status_t) apr_encode_base64_binary(char *dest, const unsigned char *src,
        apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert text data to base64, and return the results from a pool.
 * @param p Pool to allocate from.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 64 Encoding. If
 *  APR_ENCODE_NOPADDING, omit the = padding character.	If APR_ENCODE_URL,
 *  use RFC4648 Base 64 Encoding with URL and Filename Safe Alphabet.
 * 	If APR_ENCODE_BASE64URL, use RFC7515 base64url Encoding.
 * @param len If present, returns the number of characters written excluding
 *  the zero pad.
 * @return A zero padded string allocated from the pool on success, or
 * NULL if src was NULL.
 */
APR_DECLARE(const char *)apr_pencode_base64(apr_pool_t * p, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len)__attribute__((nonnull(1)));

/**
 * Convert binary data to base64, and return the results from a pool.
 * @param p Pool to allocate from.
 * @param src The original buffer.
 * @param slen The length of the original buffer.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 64 Encoding. If
 *  APR_ENCODE_NOPADDING, omit the = padding character.	If APR_ENCODE_URL,
 *  use RFC4648 Base 64 Encoding with URL and Filename Safe Alphabet.
 * 	If APR_ENCODE_BASE64URL, use RFC7515 base64url Encoding.
 * @param len If present, returns the number of characters written excluding
 *  the zero pad.
 * @return A zero padded string allocated from the pool on success, or
 * NULL if src was NULL.
 */
APR_DECLARE(const char *)apr_pencode_base64_binary(apr_pool_t * p, const unsigned char *src,
        apr_ssize_t slen, int flags, apr_size_t * len)__attribute__((nonnull(1)));

/**
 * Convert base64 or base64url with or without padding to text data.
 * @param dest The destination string, can be NULL.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, attempt to decode the full original buffer,
 *  and return NULL if any bad character is detected. If APR_ENCODE_RELAXED,
 *  decode until the first non base64/base64url character.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination string, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the string was NULL, or APR_BADCH
 * if a non hex character is present.
 */
APR_DECLARE(apr_status_t) apr_decode_base64(char *dest, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert base64 or base64url with or without padding to binary data.
 * @param dest The destination buffer, can be NULL.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, attempt to decode the full original buffer,
 *  and return NULL if any bad character is detected. If APR_ENCODE_RELAXED,
 *  decode until the first non base64/base64url character.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination buffer, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the src was NULL, or APR_BADCH
 * if a non base64 character is present.
 */
APR_DECLARE(apr_status_t) apr_decode_base64_binary(unsigned char *dest,
        const char *src, apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert base64 or base64url with or without padding to text data, and
 * return the results from a pool.
 * @param p Pool to allocate from.
 * @param src The base64 string to decode.
 * @param slen The length of the base64 string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, attempt to decode the full original buffer,
 *  and return NULL if any bad character is detected. If APR_ENCODE_RELAXED,
 *  decode until the first non base64/base64url character.
 * @param len If present, returns the number of characters written, excluding
 *  the zero padding.
 * @return A string allocated from the pool containing the result with a zero
 *  pad. If src was NULL, or an error occurred, NULL is returned.
 */
APR_DECLARE(const char *)apr_pdecode_base64(apr_pool_t * p, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len)
        __attribute__((nonnull(1)));

/**
 * Convert base64 or base64url with or without padding to binary data, and
 * return the results from a pool.
 * @param p Pool to allocate from.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 *  NUL terminated.
 * @param flags If APR_ENCODE_NONE, attempt to decode the full original buffer,
 *  and return NULL if any bad character is detected. If APR_ENCODE_RELAXED,
 *  decode until the first non base64/base64url character.
 * @param len If present, returns the number of characters written, excluding
 *  the zero padding.
 * @return A buffer allocated from the pool containing the result with a zero
 *  pad. If src was NULL, or an error occurred, NULL is returned.
 */
APR_DECLARE(const unsigned char *)apr_pdecode_base64_binary(apr_pool_t * p,
        const char *src, apr_ssize_t slen, int flags, apr_size_t * len)
        __attribute__((nonnull(1)));

/**
 * Convert text data to base32.
 * @param dest The destination string, can be NULL.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 32 Encoding. If
 *  APR_ENCODE_NOPADDING, omit the = padding character.	If APR_ENCODE_BASE32HEX,
 *  use RFC4648 base32hex Encoding.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination string, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the string was NULL.
 */
APR_DECLARE(apr_status_t) apr_encode_base32(char *dest, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert binary data to base32.
 * @param dest The destination string, can be NULL.
 * @param src The original buffer.
 * @param slen The length of the original buffer.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 32 Encoding. If
 *  APR_ENCODE_NOPADDING, omit the = padding character.	If APR_ENCODE_BASE32HEX,
 *  use RFC4648 base32hex Encoding.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination string, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the string was NULL.
 */
APR_DECLARE(apr_status_t) apr_encode_base32_binary(char *dest, const unsigned char *src,
        apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert text data to base32, and return the results from a pool.
 * @param p Pool to allocate from.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 32 Encoding. If
 *  APR_ENCODE_NOPADDING, omit the = padding character.	If APR_ENCODE_BASE32HEX,
 *  use RFC4648 base32hex Encoding.
 * @param len If present, returns the number of characters written excluding
 *  the zero pad.
 * @return A zero padded string allocated from the pool on success, or
 * NULL if src was NULL.
 */
APR_DECLARE(const char *)apr_pencode_base32(apr_pool_t * p, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len)
        __attribute__((nonnull(1)));

/**
 * Convert binary data to base32, and return the results from a pool.
 * @param p Pool to allocate from.
 * @param src The original buffer.
 * @param slen The length of the original buffer.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 32 Encoding. If
 *  APR_ENCODE_NOPADDING, omit the = padding character.	If APR_ENCODE_BASE32HEX,
 *  use RFC7515 base32hex Encoding.
 * @param len If present, returns the number of characters written excluding
 *  the zero pad.
 * @return A zero padded string allocated from the pool on success, or
 * NULL if src was NULL.
 */
APR_DECLARE(const char *)apr_pencode_base32_binary(apr_pool_t * p, const unsigned char *src,
        apr_ssize_t slen, int flags, apr_size_t * len)
        __attribute__((nonnull(1)));

/**
 * Convert base32 or base32hex with or without padding to text data.
 * @param dest The destination string, can be NULL.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, parse RFC4648 Base 32 Encoding. If
 *  APR_ENCODE_BASE32HEX, use RFC4648 base32hex Encoding.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination buffer, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the string was NULL, or APR_BADCH
 * if a non base32 character is present.
 */
APR_DECLARE(apr_status_t) apr_decode_base32(char *dest, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert base32 or base32hex with or without padding to binary data.
 * @param dest The destination buffer, can be NULL.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, parse RFC4648 Base 32 Encoding. If
 *  APR_ENCODE_BASE32HEX, use RFC4648 base32hex Encoding.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination buffer, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the src was NULL, or APR_BADCH
 * if a non base32 character is present.
 */
APR_DECLARE(apr_status_t) apr_decode_base32_binary(unsigned char *dest,
        const char *src, apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert base32 or base32hex with or without padding to text data, and
 * return the results from a pool.
 * @param p Pool to allocate from.
 * @param src The base32 string to decode.
 * @param slen The length of the base32 string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, parse RFC4648 Base 32 Encoding. If
 *  APR_ENCODE_BASE32HEX, use RFC4648 base32hex Encoding.
 * @param len If present, returns the number of characters written, excluding
 *  the zero padding.
 * @return A string allocated from the pool containing the result with a zero
 *  pad. If src was NULL, or an error occurred, NULL is returned.
 */
APR_DECLARE(const char *)apr_pdecode_base32(apr_pool_t * p, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len)
        __attribute__((nonnull(1)));

/**
 * Convert base32 or base32hex with or without padding to binary data, and
 * return the results from a pool.
 * @param p Pool to allocate from.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 *  NUL terminated.
 * @param flags If APR_ENCODE_NONE, parse RFC4648 Base 32 Encoding. If
 *  APR_ENCODE_BASE32HEX, use RFC4648 base32hex Encoding.
 * @param len If present, returns the number of characters written, excluding
 *  the zero padding.
 * @return A buffer allocated from the pool containing the result with a zero
 *  pad. If src was NULL, or an error occurred, NULL is returned.
 */
APR_DECLARE(const unsigned char *)apr_pdecode_base32_binary(apr_pool_t * p,
        const char *src, apr_ssize_t slen, int flags, apr_size_t * len)
        __attribute__((nonnull(1)));

/**
 * Convert text data to base16 (hex).
 * @param dest The destination string, can be NULL.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 16 Encoding. If
 *  APR_ENCODE_COLON, separate each token with a colon.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination buffer, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the string was NULL.
 */
APR_DECLARE(apr_status_t) apr_encode_base16(char *dest, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert binary data to base16 (hex).
 * @param dest The destination string, can be NULL.
 * @param src The original buffer.
 * @param slen The length of the original buffer.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 16 Encoding. If
 *  APR_ENCODE_COLON, separate each token with a colon.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination buffer, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the string was NULL.
 */
APR_DECLARE(apr_status_t) apr_encode_base16_binary(char *dest,
        const unsigned char *src, apr_ssize_t slen, int flags,
        apr_size_t * len);

/**
 * Convert text data to base16 (hex), and return the results from a
 * pool.
 * @param p Pool to allocate from.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 16 Encoding. If
 *  APR_ENCODE_COLON, separate each token with a colon.
 * @param len If present, returns the number of characters written, excluding
 *  the zero padding.
 * @return A string allocated from the pool containing the result with a zero
 *  pad. If src was NULL, or an error occurred, NULL is returned.
 */
APR_DECLARE(const char *)apr_pencode_base16(apr_pool_t * p, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len)
        __attribute__((nonnull(1)));

/**
 * Convert binary data to base16 (hex), and return the results from a
 * pool.
 * @param p Pool to allocate from.
 * @param src The original buffer.
 * @param slen The length of the original buffer.
 * @param flags If APR_ENCODE_NONE, emit RFC4648 Base 16 Encoding. If
 *  APR_ENCODE_COLON, separate each token with a colon.
 * @param len If present, returns the number of characters written, excluding
 *  the zero padding.
 * @return A string allocated from the pool containing the result with a zero
 *  pad. If src was NULL, or an error occurred, NULL is returned.
 */
APR_DECLARE(const char *)apr_pencode_base16_binary(apr_pool_t * p,
        const unsigned char *src, apr_ssize_t slen,
        int flags, apr_size_t * len)__attribute__((nonnull(1)));

/**
 * Convert base16 (hex) to text data.
 * @param dest The destination string, can be NULL.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, parse RFC4648 Base 16 Encoding. If
 *  APR_ENCODE_COLON, allow tokens to be separated with a colon.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination buffer, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the string was NULL, or APR_BADCH
 * if a non hex character is present. A zero pad is appended to the buffer.
 */
APR_DECLARE(apr_status_t) apr_decode_base16(char *dest, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert base16 (hex) to binary data.
 * @param dest The destination buffer, can be NULL.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, parse RFC4648 Base 16 Encoding. If
 *  APR_ENCODE_COLON, allow tokens to be separated with a colon.
 * @param len If present and src is NULL, returns the maximum possible length
 *  of the destination buffer, including a zero pad. If present and src is
 *  not NULL, returns the number of characters actually written.
 * @return APR_SUCCESS, or APR_NOTFOUND if the string was NULL, or APR_BADCH
 * if a non hex character is present. No zero pad is written to the buffer.
 */
APR_DECLARE(apr_status_t) apr_decode_base16_binary(unsigned char *dest,
        const char *src, apr_ssize_t slen, int flags, apr_size_t * len);

/**
 * Convert base16 (hex) and return the results from a pool.
 * @param p Pool to allocate from.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, parse RFC4648 Base 16 Encoding. If
 *  APR_ENCODE_COLON, allow tokens to be separated with a colon.
 * @param len If present, returns the number of characters written, excluding
 *  the zero padding.
 * @return A buffer allocated from the pool containing the result with a zero
 *  pad. If src was NULL, or an error occurred, NULL is returned.
 */
APR_DECLARE(const char *)apr_pdecode_base16(apr_pool_t * p, const char *src,
        apr_ssize_t slen, int flags, apr_size_t * len)
        __attribute__((nonnull(1)));

/**
 * Convert base16 (hex) to binary data, and return the results from a pool.
 * @param p Pool to allocate from.
 * @param src The original string.
 * @param slen The length of the original string, or APR_ENCODE_STRING if
 * NUL terminated.
 * @param flags If APR_ENCODE_NONE, parse RFC4648 Base 16 Encoding. If
 *  APR_ENCODE_COLON, allow tokens to be separated with a colon.
 * @param len If present, returns the number of characters written, excluding
 *  the zero padding.
 * @return A buffer allocated from the pool containing the result with a zero
 *  pad. If src was NULL, or an error occurred, NULL is returned.
 */
APR_DECLARE(const unsigned char *)apr_pdecode_base16_binary(apr_pool_t * p,
        const char *src, apr_ssize_t slen, int flags, apr_size_t * len)
        __attribute__((nonnull(1)));

/** @} */
#ifdef __cplusplus
}
#endif

#endif                          /* !APR_ENCODE_H */
