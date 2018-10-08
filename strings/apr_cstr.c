/*    Licensed to the Apache Software Foundation (ASF) under one
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
 */

#include "apr.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_fnmatch.h"
#if 0
#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#endif
#include "apr_want.h"
#include "apr_cstr.h"

APR_DECLARE(void) apr_cstr_split_append(apr_array_header_t *array,
                                        const char *input,
                                        const char *sep_chars,
                                        int chop_whitespace,
                                        apr_pool_t *pool)
{
  char *pats;
  char *p;

  pats = apr_pstrdup(pool, input);  /* strtok wants non-const data */
  p = apr_cstr_tokenize(sep_chars, &pats);

  while (p)
    {
      if (chop_whitespace)
        {
          while (apr_isspace(*p))
            p++;

          {
            char *e = p + (strlen(p) - 1);
            while ((e >= p) && (apr_isspace(*e)))
              e--;
            *(++e) = '\0';
          }
        }

      if (p[0] != '\0')
        APR_ARRAY_PUSH(array, const char *) = p;

      p = apr_cstr_tokenize(sep_chars, &pats);
    }

  return;
}


APR_DECLARE(apr_array_header_t *) apr_cstr_split(const char *input,
                                                 const char *sep_chars,
                                                 int chop_whitespace,
                                                 apr_pool_t *pool)
{
  apr_array_header_t *a = apr_array_make(pool, 5, sizeof(input));
  apr_cstr_split_append(a, input, sep_chars, chop_whitespace, pool);
  return a;
}


APR_DECLARE(int) apr_cstr_match_glob_list(const char *str,
                                          const apr_array_header_t *list)
{
  int i;

  for (i = 0; i < list->nelts; i++)
    {
      const char *this_pattern = APR_ARRAY_IDX(list, i, char *);

      if (apr_fnmatch(this_pattern, str, 0) == APR_SUCCESS)
        return TRUE;
    }

  return FALSE;
}

APR_DECLARE(int) apr_cstr_match_list(const char *str,
                                     const apr_array_header_t *list)
{
  int i;

  for (i = 0; i < list->nelts; i++)
    {
      const char *this_str = APR_ARRAY_IDX(list, i, char *);

      if (strcmp(this_str, str) == 0)
        return TRUE;
    }

  return FALSE;
}

APR_DECLARE(char *) apr_cstr_tokenize(const char *sep, char **str)
{
    char *token;
    char *next;
    char csep;

    /* check parameters */
    if ((sep == NULL) || (str == NULL) || (*str == NULL))
        return NULL;

    /* let APR handle edge cases and multiple separators */
    csep = *sep;
    if (csep == '\0' || sep[1] != '\0')
      return apr_strtok(NULL, sep, str);

    /* skip characters in sep (will terminate at '\0') */
    token = *str;
    while (*token == csep)
        ++token;

    if (!*token)          /* no more tokens */
        return NULL;

    /* skip valid token characters to terminate token and
     * prepare for the next call (will terminate at '\0)
     */
    next = strchr(token, csep);
    if (next == NULL)
      {
        *str = token + strlen(token);
      }
    else
      {
        *next = '\0';
        *str = next + 1;
      }

    return token;
}

APR_DECLARE(int) apr_cstr_count_newlines(const char *msg)
{
  int count = 0;
  const char *p;

  for (p = msg; *p; p++)
    {
      if (*p == '\n')
        {
          count++;
          if (*(p + 1) == '\r')
            p++;
        }
      else if (*p == '\r')
        {
          count++;
          if (*(p + 1) == '\n')
            p++;
        }
    }

  return count;
}

#if 0 /* XXX: stringbuf logic is not present in APR */
APR_DECLARE(char *) apr_cstr_join(const apr_array_header_t *strings,
                                  const char *separator,
                                  apr_pool_t *pool)
{
  svn_stringbuf_t *new_str = svn_stringbuf_create_empty(pool);
  size_t sep_len = strlen(separator);
  int i;

  for (i = 0; i < strings->nelts; i++)
    {
      const char *string = APR_ARRAY_IDX(strings, i, const char *);
      svn_stringbuf_appendbytes(new_str, string, strlen(string));
      svn_stringbuf_appendbytes(new_str, separator, sep_len);
    }
  return new_str->data;
}
#endif

#if !APR_CHARSET_EBCDIC
/*
 * Our own known-fast translation table for casecmp by character.
 * Only ASCII alpha characters 41-5A are folded to 61-7A, other
 * octets (such as extended latin alphabetics) are never case-folded.
 * NOTE: Other than Alpha A-Z/a-z, each code point is unique!
 */
static const short ucharmap[] = {
    0x0,  0x1,  0x2,  0x3,  0x4,  0x5,  0x6,  0x7,
    0x8,  0x9,  0xa,  0xb,  0xc,  0xd,  0xe,  0xf,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40,  'a',  'b',  'c',  'd',  'e',  'f',  'g',
     'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
     'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
     'x',  'y',  'z', 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60,  'a',  'b',  'c',  'd',  'e',  'f',  'g',
     'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
     'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
     'x',  'y',  'z', 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
#else /* APR_CHARSET_EBCDIC */
/*
 * Derived from apr-iconv/ccs/cp037.c for EBCDIC case comparison,
 * provides unique identity of every char value (strict ISO-646
 * conformance, arbitrary election of an ISO-8859-1 ordering, and
 * very arbitrary control code assignments into C1 to achieve
 * identity and a reversible mapping of code points),
 * then folding the equivalences of ASCII 41-5A into 61-7A, 
 * presenting comparison results in a somewhat ISO/IEC 10646
 * (ASCII-like) order, depending on the EBCDIC code page in use.
 *
 * NOTE: Other than Alpha A-Z/a-z, each code point is unique!
 */
static const short ucharmap[] = {
    0x00, 0x01, 0x02, 0x03, 0x9C, 0x09, 0x86, 0x7F,
    0x97, 0x8D, 0x8E, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x9D, 0x85, 0x08, 0x87,
    0x18, 0x19, 0x92, 0x8F, 0x1C, 0x1D, 0x1E, 0x1F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x0A, 0x17, 0x1B,
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x05, 0x06, 0x07,
    0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04,
    0x98, 0x99, 0x9A, 0x9B, 0x14, 0x15, 0x9E, 0x1A,
    0x20, 0xA0, 0xE2, 0xE4, 0xE0, 0xE1, 0xE3, 0xE5,
    0xE7, 0xF1, 0xA2, 0x2E, 0x3C, 0x28, 0x2B, 0x7C,
    0x26, 0xE9, 0xEA, 0xEB, 0xE8, 0xED, 0xEE, 0xEF,
    0xEC, 0xDF, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0xAC,
    0x2D, 0x2F, 0xC2, 0xC4, 0xC0, 0xC1, 0xC3, 0xC5,
    0xC7, 0xD1, 0xA6, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,
    0xF8, 0xC9, 0xCA, 0xCB, 0xC8, 0xCD, 0xCE, 0xCF,
    0xCC, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,
    0xD8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0xAB, 0xBB, 0xF0, 0xFD, 0xFE, 0xB1,
    0xB0, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,
    0x71, 0x72, 0xAA, 0xBA, 0xE6, 0xB8, 0xC6, 0xA4,
    0xB5, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7A, 0xA1, 0xBF, 0xD0, 0xDD, 0xDE, 0xAE,
    0x5E, 0xA3, 0xA5, 0xB7, 0xA9, 0xA7, 0xB6, 0xBC,
    0xBD, 0xBE, 0x5B, 0x5D, 0xAF, 0xA8, 0xB4, 0xD7,
    0x7B, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0xAD, 0xF4, 0xF6, 0xF2, 0xF3, 0xF5,
    0x7D, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,
    0x71, 0x72, 0xB9, 0xFB, 0xFC, 0xF9, 0xFA, 0xFF,
    0x5C, 0xF7, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7A, 0xB2, 0xD4, 0xD6, 0xD2, 0xD3, 0xD5,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0xB3, 0xDB, 0xDC, 0xD9, 0xDA, 0x9F
};
#endif

APR_DECLARE(int) apr_cstr_casecmp(const char *s1, const char *s2)
{
    const unsigned char *str1 = (const unsigned char *)s1;
    const unsigned char *str2 = (const unsigned char *)s2;
    for (;;)
    {
        const int c1 = (int)(*str1);
        const int c2 = (int)(*str2);
        const int cmp = ucharmap[c1] - ucharmap[c2];
        /* Not necessary to test for !c2, this is caught by cmp */
        if (cmp || !c1)
            return cmp;
        str1++;
        str2++;
    }
}

APR_DECLARE(int) apr_cstr_casecmpn(const char *s1, const char *s2, apr_size_t n)
{
    const unsigned char *str1 = (const unsigned char *)s1;
    const unsigned char *str2 = (const unsigned char *)s2;
    while (n--)
    {
        const int c1 = (int)(*str1);
        const int c2 = (int)(*str2);
        const int cmp = ucharmap[c1] - ucharmap[c2];
        /* Not necessary to test for !c2, this is caught by cmp */
        if (cmp || !c1)
            return cmp;
        str1++;
        str2++;
    }
    return 0;
}

APR_DECLARE(apr_status_t) apr_cstr_strtoui64(apr_uint64_t *n, const char *str,
                                apr_uint64_t minval, apr_uint64_t maxval,
                                int base)
{
  apr_int64_t val;
  char *endptr;

  /* We assume errno is thread-safe. */
  errno = 0; /* APR-0.9 doesn't always set errno */

  /* ### We're throwing away half the number range here.
   * ### APR needs a apr_strtoui64() function. */
  val = apr_strtoi64(str, &endptr, base);
  if (errno == EINVAL || endptr == str || str[0] == '\0' || *endptr != '\0')
    return APR_EINVAL;
  if ((errno == ERANGE && (val == APR_INT64_MIN || val == APR_INT64_MAX)) ||
      val < 0 || (apr_uint64_t)val < minval || (apr_uint64_t)val > maxval)
    return APR_ERANGE;
  *n = val;
  return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_cstr_atoui64(apr_uint64_t *n, const char *str)
{
  return apr_cstr_strtoui64(n, str, 0, APR_UINT64_MAX, 10);
}

APR_DECLARE(apr_status_t) apr_cstr_atoui(unsigned int *n, const char *str)
{
  apr_uint64_t val;
  apr_status_t rv = apr_cstr_strtoui64(&val, str, 0, APR_UINT32_MAX, 10);
  if (rv == APR_SUCCESS)
    *n = (unsigned int)val;
  return rv;
}

APR_DECLARE(apr_status_t) apr_cstr_strtoi64(apr_int64_t *n, const char *str,
                               apr_int64_t minval, apr_int64_t maxval,
                               int base)
{
  apr_int64_t val;
  char *endptr;

  /* We assume errno is thread-safe. */
  errno = 0; /* APR-0.9 doesn't always set errno */

  val = apr_strtoi64(str, &endptr, base);
  if (errno == EINVAL || endptr == str || str[0] == '\0' || *endptr != '\0')
    return APR_EINVAL;
  if ((errno == ERANGE && (val == APR_INT64_MIN || val == APR_INT64_MAX)) ||
      val < minval || val > maxval)
    return APR_ERANGE;
  *n = val;
  return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_cstr_atoi64(apr_int64_t *n, const char *str)
{
  return apr_cstr_strtoi64(n, str, APR_INT64_MIN, APR_INT64_MAX, 10);
}

APR_DECLARE(apr_status_t) apr_cstr_atoi(int *n, const char *str)
{
  apr_int64_t val;
  apr_status_t rv;

  rv = apr_cstr_strtoi64(&val, str, APR_INT32_MIN, APR_INT32_MAX, 10);
  if (rv == APR_SUCCESS)
    *n = (int)val;
  return rv;
}

APR_DECLARE(const char *) apr_cstr_skip_prefix(const char *str, 
                                               const char *prefix)
{
  apr_size_t len = strlen(prefix);

  if (strncmp(str, prefix, len) == 0)
    {
      return str + len;
    }
  else
    {
      return NULL;
    }
}
