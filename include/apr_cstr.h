/* ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */

/**
 * @file apr_cstr.h
 * @brief C string goodies.
 */

#ifndef APR_CSTR_H
#define APR_CSTR_H

#include <apr.h>          /* for apr_size_t */
#include <apr_pools.h>    /* for apr_pool_t */
#include <apr_tables.h>   /* for apr_array_header_t */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup apr_cstr C (POSIX) locale string functions
 * @ingroup apr_strings
 *
 * The apr_cstr_* functions provide traditional C char * string text handling,
 * and notabilty they treat all text in the C (a.k.a. POSIX) locale using the
 * minimal POSIX character set, represented in either ASCII or a corresponding
 * EBCDIC subset.
 *
 * Character values outside of that set are treated as opaque bytes, and all
 * multi-byte character sequences are handled as individual distinct octets.
 *
 * Multi-byte characters sequences whose octets fall in the ASCII range cause
 * unexpected results, such as in the ISO-2022-JP code page where ASCII octets
 * occur within both shift-state and multibyte sequences.
 *
 * In the case of the UTF-8 encoding, all multibyte characters all fall outside
 * of the C/POSIX range of characters, so these functions are generally safe
 * to use on UTF-8 strings. The programmer must be aware that each octet may
 * not represent a distinct printable character in such encodings.
 *
 * The standard C99/POSIX string functions, rather than apr_cstr, should be
 * used in all cases where the current locale and encoding of the text is
 * significant.
 * @{
 */


/** Divide @a input into substrings, interpreting any char from @a sep
 * as a token separator.
 *
 * Return an array of copies of those substrings (plain const char*),
 * allocating both the array and the copies in @a pool.
 *
 * None of the elements added to the array contain any of the
 * characters in @a sep_chars, and none of the new elements are empty
 * (thus, it is possible that the returned array will have length
 * zero).
 *
 * If @a chop_whitespace is TRUE, then remove leading and trailing
 * whitespace from the returned strings.
 *
 * @since New in 1.6
 */
APR_DECLARE(apr_array_header_t *) apr_cstr_split(const char *input,
                                                 const char *sep_chars,
                                                 int chop_whitespace,
                                                 apr_pool_t *pool);

/** Like apr_cstr_split(), but append to existing @a array instead of
 * creating a new one.  Allocate the copied substrings in @a pool
 * (i.e., caller decides whether or not to pass @a array->pool as @a pool).
 *
 * @since New in 1.6
 */
APR_DECLARE(void) apr_cstr_split_append(apr_array_header_t *array,
                                        const char *input,
                                        const char *sep_chars,
                                        int chop_whitespace,
                                        apr_pool_t *pool);


/** Return @c TRUE iff @a str matches any of the elements of @a list, a list
 * of zero or more glob patterns.
 *
 * @since New in 1.6
 */
APR_DECLARE(int) apr_cstr_match_glob_list(const char *str,
                                          const apr_array_header_t *list);

/** Return @c TRUE iff @a str exactly matches any of the elements of @a list.
 *
 * @since New in 1.6
 */
APR_DECLARE(int) apr_cstr_match_list(const char *str,
                                     const apr_array_header_t *list);

/**
 * Get the next token from @a *str interpreting any char from @a sep as a
 * token separator.  Separators at the beginning of @a str will be skipped.
 * Returns a pointer to the beginning of the first token in @a *str or NULL
 * if no token is left.  Modifies @a str such that the next call will return
 * the next token.
 *
 * @note The content of @a *str may be modified by this function.
 *
 * @since New in 1.6.
 */
APR_DECLARE(char *) apr_cstr_tokenize(const char *sep, char **str);

/**
 * Return the number of line breaks in @a msg, allowing any kind of newline
 * termination (CR, LF, CRLF, or LFCR), even inconsistent.
 *
 * @since New in 1.6.
 */
APR_DECLARE(int) apr_cstr_count_newlines(const char *msg);

#if 0 /* XXX: stringbuf logic is not present in APR */
/**
 * Return a cstring which is the concatenation of @a strings (an array
 * of char *) each followed by @a separator (that is, @a separator
 * will also end the resulting string).  Allocate the result in @a pool.
 * If @a strings is empty, then return the empty string.
 *
 * @since New in 1.6.
 */
APR_DECLARE(char *) apr_cstr_join(const apr_array_header_t *strings,
                                  const char *separator,
                                  apr_pool_t *pool);
#endif

/**
 * Perform a case-insensitive comparison of two strings @a atr1 and @a atr2,
 * treating upper and lower case values of the 26 standard C/POSIX alphabetic
 * characters as equivalent. Extended latin characters outside of this set
 * are treated as unique octets, irrespective of the current locale.
 *
 * Returns in integer greater than, equal to, or less than 0,
 * according to whether @a str1 is considered greater than, equal to,
 * or less than @a str2.
 *
 * @since New in 1.6.
 */
APR_DECLARE(int) apr_cstr_casecmp(const char *str1, const char *str2);

/**
 * Perform a case-insensitive comparison of two strings @a atr1 and @a atr2,
 * treating upper and lower case values of the 26 standard C/POSIX alphabetic
 * characters as equivalent. Extended latin characters outside of this set
 * are treated as unique octets, irrespective of the current locale.
 *
 * Returns in integer greater than, equal to, or less than 0,
 * according to whether @a str1 is considered greater than, equal to,
 * or less than @a str2.
 *
 * @since New in 1.6.
 */
APR_DECLARE(int) apr_cstr_casecmpn(const char *str1,
                                   const char *str2,
                                   apr_size_t n);

/**
 * Parse the C string @a str into a 64 bit number, and return it in @a *n.
 * Assume that the number is represented in base @a base.
 * Raise an error if conversion fails (e.g. due to overflow), or if the
 * converted number is smaller than @a minval or larger than @a maxval.
 *
 * Leading whitespace in @a str is skipped in a locale-dependent way.
 * After that, the string may contain an optional '+' (positive, default)
 * or '-' (negative) character, followed by an optional '0x' prefix if
 * @a base is 0 or 16, followed by numeric digits appropriate for the base.
 * If there are any more characters after the numeric digits, an error is
 * returned.
 *
 * If @a base is zero, then a leading '0x' or '0X' prefix means hexadecimal,
 * else a leading '0' means octal (implemented, though not documented, in
 * apr_strtoi64() in APR 0.9.0 through 1.5.0), else use base ten.
 *
 * @since New in 1.6.
 */
APR_DECLARE(apr_status_t) apr_cstr_strtoi64(apr_int64_t *n, const char *str,
                                            apr_int64_t minval,
                                            apr_int64_t maxval,
                                            int base);

/**
 * Parse the C string @a str into a 64 bit number, and return it in @a *n.
 * Assume that the number is represented in base 10.
 * Raise an error if conversion fails (e.g. due to overflow).
 *
 * The behaviour otherwise is as described for apr_cstr_strtoi64().
 *
 * @since New in 1.6.
 */
APR_DECLARE(apr_status_t) apr_cstr_atoi64(apr_int64_t *n, const char *str);

/**
 * Parse the C string @a str into a 32 bit number, and return it in @a *n.
 * Assume that the number is represented in base 10.
 * Raise an error if conversion fails (e.g. due to overflow).
 *
 * The behaviour otherwise is as described for apr_cstr_strtoi64().
 *
 * @since New in 1.6.
 */
APR_DECLARE(apr_status_t) apr_cstr_atoi(int *n, const char *str);

/**
 * Parse the C string @a str into an unsigned 64 bit number, and return
 * it in @a *n. Assume that the number is represented in base @a base.
 * Raise an error if conversion fails (e.g. due to overflow), or if the
 * converted number is smaller than @a minval or larger than @a maxval.
 *
 * Leading whitespace in @a str is skipped in a locale-dependent way.
 * After that, the string may contain an optional '+' (positive, default)
 * or '-' (negative) character, followed by an optional '0x' prefix if
 * @a base is 0 or 16, followed by numeric digits appropriate for the base.
 * If there are any more characters after the numeric digits, an error is
 * returned.
 *
 * If @a base is zero, then a leading '0x' or '0X' prefix means hexadecimal,
 * else a leading '0' means octal (as implemented, though not documented, in
 * apr_strtoi64(), else use base ten.
 *
 * @warning The implementation returns APR_ERANGE if the parsed number
 * is greater than APR_INT64_MAX, even if it is not greater than @a maxval.
 *
 * @since New in 1.6.
 */
APR_DECLARE(apr_status_t) apr_cstr_strtoui64(apr_uint64_t *n, const char *str,
                                             apr_uint64_t minval,
                                             apr_uint64_t maxval,
                                             int base);

/**
 * Parse the C string @a str into an unsigned 64 bit number, and return
 * it in @a *n. Assume that the number is represented in base 10.
 * Raise an error if conversion fails (e.g. due to overflow).
 *
 * The behaviour otherwise is as described for apr_cstr_strtoui64(),
 * including the upper limit of APR_INT64_MAX.
 *
 * @since New in 1.6.
 */
APR_DECLARE(apr_status_t) apr_cstr_atoui64(apr_uint64_t *n, const char *str);

/**
 * Parse the C string @a str into an unsigned 32 bit number, and return
 * it in @a *n. Assume that the number is represented in base 10.
 * Raise an error if conversion fails (e.g. due to overflow).
 *
 * The behaviour otherwise is as described for apr_cstr_strtoui64(),
 * including the upper limit of APR_INT64_MAX.
 *
 * @since New in 1.6.
 */
APR_DECLARE(apr_status_t) apr_cstr_atoui(unsigned int *n, const char *str);

/**
 * Skip the common prefix @a prefix from the C string @a str, and return
 * a pointer to the next character after the prefix.
 * Return @c NULL if @a str does not start with @a prefix.
 *
 * @since New in 1.6.
 */
APR_DECLARE(const char *) apr_cstr_skip_prefix(const char *str,
                                               const char *prefix);

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* SVN_STRING_H */
