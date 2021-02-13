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

/* The code in this file is copied from APR (initially from APR 1.7.0)
 * to provide compatibility for building against APR < 1.5.0.
 */

#include "private/svn_dep_compat.h"

#if !APR_VERSION_AT_LEAST(1,5,0)

#include <apr_lib.h>
#include <apr_strings.h>

/* from apr_escape_test_char.h */
#define T_ESCAPE_SHELL_CMD     (1)
#define T_ESCAPE_PATH_SEGMENT  (2)
#define T_OS_ESCAPE_PATH       (4)
#define T_ESCAPE_ECHO          (8)
#define T_ESCAPE_URLENCODED    (16)
#define T_ESCAPE_XML           (32)
#define T_ESCAPE_LDAP_DN       (64)
#define T_ESCAPE_LDAP_FILTER   (128)

static const unsigned char test_char_table[256] = {
    224,222,222,222,222,222,222,222,222,222,223,222,222,222,222,222,222,222,222,222,
    222,222,222,222,222,222,222,222,222,222,222,222,6,16,127,22,17,22,49,17,
    145,145,129,80,80,0,0,18,0,0,0,0,0,0,0,0,0,0,16,87,
    119,16,119,23,16,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,23,223,23,23,0,23,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,23,23,23,17,222,222,222,222,222,222,222,222,222,222,222,222,222,
    222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,
    222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,
    222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,
    222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,
    222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,
    222,222,222,222,222,222,222,222,222,222,222,222,222,222,222,222 
};

/* from apr_encode_private.h */
#if APR_CHARSET_EBCDIC
#error This Subversion compatibility code for APR<1.5 does not support EBCDIC.
#else                           /* APR_CHARSET_EBCDIC */
#define ENCODE_TO_ASCII(ch)  (ch)
#define ENCODE_TO_NATIVE(ch)  (ch)
#endif                          /* !APR_CHARSET_EBCDIC */

/* we assume the folks using this ensure 0 <= c < 256... which means
 * you need a cast to (unsigned char) first, you can't just plug a
 * char in here and get it to work, because if char is signed then it
 * will first be sign extended.
 */
#define TEST_CHAR(c, f)        (test_char_table[(unsigned)(c)] & (f))

APR_DECLARE(apr_status_t) apr_escape_shell(char *escaped, const char *str,
        apr_ssize_t slen, apr_size_t *len)
{
    unsigned char *d;
    const unsigned char *s;
    apr_size_t size = 1;
    int found = 0;

    d = (unsigned char *) escaped;
    s = (const unsigned char *) str;

    if (s) {
        if (d) {
            for (; *s && slen; ++s, slen--) {
#if defined(OS2) || defined(WIN32)
                /*
                 * Newlines to Win32/OS2 CreateProcess() are ill advised.
                 * Convert them to spaces since they are effectively white
                 * space to most applications
                 */
                if (*s == '\r' || *s == '\n') {
                    if (d) {
                        *d++ = ' ';
                        found = 1;
                    }
                    continue;
                }
#endif
                if (TEST_CHAR(*s, T_ESCAPE_SHELL_CMD)) {
                    *d++ = '\\';
                    size++;
                    found = 1;
                }
                *d++ = *s;
                size++;
            }
            *d = '\0';
        }
        else {
            for (; *s && slen; ++s, slen--) {
                if (TEST_CHAR(*s, T_ESCAPE_SHELL_CMD)) {
                    size++;
                    found = 1;
                }
                size++;
            }
        }
    }

    if (len) {
        *len = size;
    }
    if (!found) {
        return APR_NOTFOUND;
    }

    return APR_SUCCESS;
}

#else  /* APR_VERSION_AT_LEAST(1,5,0) */

/* Silence OSX ranlib warnings about object files with no symbols. */
#include <apr.h>
extern const apr_uint32_t svn__fake__apr_escape;
const apr_uint32_t svn__fake__apr_escape = 0xdeadbeef;

#endif /* !APR_VERSION_AT_LEAST(1,5,0) */
