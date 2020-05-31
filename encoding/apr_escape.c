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

/* escape/unescape functions.
 *
 * These functions perform various escaping operations, and are provided in
 * pairs, a function to query the length of and escape existing buffers, as
 * well as companion functions to perform the same process to memory
 * allocated from a pool.
 *
 * The API is designed to have the smallest possible RAM footprint, and so
 * will only allocate the exact amount of RAM needed for each conversion.
 */

#include "apr_escape.h"
#include "apr_escape_test_char.h"
#include "apr_encode_private.h"
#include "apr_lib.h"
#include "apr_strings.h"

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

APR_DECLARE(const char *) apr_pescape_shell(apr_pool_t *p, const char *str)
{
    apr_size_t len;

    switch (apr_escape_shell(NULL, str, APR_ESCAPE_STRING, &len)) {
    case APR_SUCCESS: {
        char *cmd = apr_palloc(p, len);
        apr_escape_shell(cmd, str, APR_ESCAPE_STRING, NULL);
        return cmd;
    }
    case APR_NOTFOUND: {
        break;
    }
    }

    return str;
}

static char x2c(const char *what)
{
    register char digit;

#if !APR_CHARSET_EBCDIC
    digit =
            ((what[0] >= 'A') ? ((what[0] & 0xdf) - 'A') + 10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A') + 10 : (what[1] - '0'));
#else /*APR_CHARSET_EBCDIC*/
    char xstr[5];
    xstr[0]='0';
    xstr[1]='x';
    xstr[2]=what[0];
    xstr[3]=what[1];
    xstr[4]='\0';
    digit = ENCODE_TO_NATIVE[0xFF & strtol(xstr, NULL, 16)];
#endif /*APR_CHARSET_EBCDIC*/
    return (digit);
}

APR_DECLARE(apr_status_t) apr_unescape_url(char *escaped, const char *url,
        apr_ssize_t slen, const char *forbid, const char *reserved, int plus,
        apr_size_t *len)
{
    apr_size_t size = 1;
    int found = 0;
    const char *s = (const char *) url;
    char *d = (char *) escaped;
    register int badesc, badpath;

    if (!url) {
        return APR_NOTFOUND;
    }

    badesc = 0;
    badpath = 0;
    if (s) {
        if (d) {
            for (; *s && slen; ++s, d++, slen--) {
                if (plus && *s == '+') {
                    *d = ' ';
                    found = 1;
                }
                else if (*s != '%') {
                    *d = *s;
                }
                else {
                    if (!apr_isxdigit(*(s + 1)) || !apr_isxdigit(*(s + 2))) {
                        badesc = 1;
                        *d = '%';
                    }
                    else {
                        char decoded;
                        decoded = x2c(s + 1);
                        if ((decoded == '\0')
                                || (forbid && strchr(forbid, decoded))) {
                            badpath = 1;
                            *d = decoded;
                            s += 2;
                            slen -= 2;
                        }
                        else if (reserved && strchr(reserved, decoded)) {
                            *d++ = *s++;
                            *d++ = *s++;
                            *d = *s;
                            size += 2;
                        }
                        else {
                            *d = decoded;
                            s += 2;
                            slen -= 2;
                            found = 1;
                        }
                    }
                }
                size++;
            }
            *d = '\0';
        }
        else {
            for (; *s && slen; ++s, slen--) {
                if (plus && *s == '+') {
                    found = 1;
                }
                else if (*s != '%') {
                    /* character unchanged */
                }
                else {
                    if (!apr_isxdigit(*(s + 1)) || !apr_isxdigit(*(s + 2))) {
                        badesc = 1;
                    }
                    else {
                        char decoded;
                        decoded = x2c(s + 1);
                        if ((decoded == '\0')
                                || (forbid && strchr(forbid, decoded))) {
                            badpath = 1;
                            s += 2;
                            slen -= 2;
                        }
                        else if (reserved && strchr(reserved, decoded)) {
                            s += 2;
                            slen -= 2;
                            size += 2;
                        }
                        else {
                            s += 2;
                            slen -= 2;
                            found = 1;
                        }
                    }
                }
                size++;
            }
        }
    }

    if (len) {
        *len = size;
    }
    if (badesc) {
        return APR_EINVAL;
    }
    else if (badpath) {
        return APR_BADCH;
    }
    else if (!found) {
        return APR_NOTFOUND;
    }

    return APR_SUCCESS;
}

APR_DECLARE(const char *) apr_punescape_url(apr_pool_t *p, const char *url,
        const char *forbid, const char *reserved, int plus)
{
    apr_size_t len;

    switch (apr_unescape_url(NULL, url, APR_ESCAPE_STRING, forbid, reserved,
            plus, &len)) {
    case APR_SUCCESS: {
        char *buf = apr_palloc(p, len);
        apr_unescape_url(buf, url, APR_ESCAPE_STRING, forbid, reserved, plus,
                NULL);
        return buf;
    }
    case APR_EINVAL:
    case APR_BADCH: {
        return NULL;
    }
    case APR_NOTFOUND: {
        break;
    }
    }

    return url;
}

/* c2x takes an unsigned, and expects the caller has guaranteed that
 * 0 <= what < 256... which usually means that you have to cast to
 * unsigned char first, because (unsigned)(char)(x) first goes through
 * signed extension to an int before the unsigned cast.
 *
 * The reason for this assumption is to assist gcc code generation --
 * the unsigned char -> unsigned extension is already done earlier in
 * both uses of this code, so there's no need to waste time doing it
 * again.
 */
static const char c2x_table[] = "0123456789abcdef";

static APR_INLINE unsigned char *c2x(unsigned what, unsigned char prefix,
        unsigned char *where)
{
#if APR_CHARSET_EBCDIC
    what = convert_e2a[(unsigned char)what];
#endif /*APR_CHARSET_EBCDIC*/
    *where++ = prefix;
    *where++ = c2x_table[what >> 4];
    *where++ = c2x_table[what & 0xf];
    return where;
}

APR_DECLARE(apr_status_t) apr_escape_path_segment(char *escaped,
        const char *str, apr_ssize_t slen, apr_size_t *len)
{
    apr_size_t size = 1;
    int found = 0;
    const unsigned char *s = (const unsigned char *) str;
    unsigned char *d = (unsigned char *) escaped;
    unsigned c;

    if (s) {
        if (d) {
            while ((c = *s) && slen) {
                if (TEST_CHAR(c, T_ESCAPE_PATH_SEGMENT)) {
                    d = c2x(c, '%', d);
                    size += 2;
                    found = 1;
                }
                else {
                    *d++ = c;
                }
                ++s;
                size++;
                slen--;
            }
            *d = '\0';
        }
        else {
            while ((c = *s) && slen) {
                if (TEST_CHAR(c, T_ESCAPE_PATH_SEGMENT)) {
                    size += 2;
                    found = 1;
                }
                ++s;
                size++;
                slen--;
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

APR_DECLARE(const char *) apr_pescape_path_segment(apr_pool_t *p,
        const char *str)
{
    apr_size_t len;

    switch (apr_escape_path_segment(NULL, str, APR_ESCAPE_STRING, &len)) {
    case APR_SUCCESS: {
        char *cmd = apr_palloc(p, len);
        apr_escape_path_segment(cmd, str, APR_ESCAPE_STRING, NULL);
        return cmd;
    }
    case APR_NOTFOUND: {
        break;
    }
    }

    return str;
}

APR_DECLARE(apr_status_t) apr_escape_path(char *escaped, const char *path,
        apr_ssize_t slen, int partial, apr_size_t *len)
{
    apr_size_t size = 1;
    int found = 0;
    const unsigned char *s = (const unsigned char *) path;
    unsigned char *d = (unsigned char *) escaped;
    unsigned c;

    if (!path) {
        return APR_NOTFOUND;
    }

    if (!partial) {
        const char *colon = strchr(path, ':');
        const char *slash = strchr(path, '/');

        if (colon && (!slash || colon < slash)) {
            if (d) {
                *d++ = '.';
                *d++ = '/';
            }
            size += 2;
            found = 1;
        }
    }
    if (d) {
        while ((c = *s) && slen) {
            if (TEST_CHAR(c, T_OS_ESCAPE_PATH)) {
                d = c2x(c, '%', d);
                size += 2;
                found = 1;
            }
            else {
                *d++ = c;
            }
            ++s;
            size++;
            slen--;
        }
        *d = '\0';
    }
    else {
        while ((c = *s) && slen) {
            if (TEST_CHAR(c, T_OS_ESCAPE_PATH)) {
                size += 2;
                found = 1;
            }
            ++s;
            size++;
            slen--;
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

APR_DECLARE(const char *) apr_pescape_path(apr_pool_t *p, const char *str,
        int partial)
{
    apr_size_t len;

    switch (apr_escape_path(NULL, str, APR_ESCAPE_STRING, partial, &len)) {
    case APR_SUCCESS: {
        char *path = apr_palloc(p, len);
        apr_escape_path(path, str, APR_ESCAPE_STRING, partial, NULL);
        return path;
    }
    case APR_NOTFOUND: {
        break;
    }
    }

    return str;
}

APR_DECLARE(apr_status_t) apr_escape_urlencoded(char *escaped, const char *str,
        apr_ssize_t slen, apr_size_t *len)
{
    apr_size_t size = 1;
    int found = 0;
    const unsigned char *s = (const unsigned char *) str;
    unsigned char *d = (unsigned char *) escaped;
    unsigned c;

    if (s) {
        if (d) {
            while ((c = *s) && slen) {
                if (TEST_CHAR(c, T_ESCAPE_URLENCODED)) {
                    d = c2x(c, '%', d);
                    size += 2;
                    found = 1;
                }
                else if (c == ' ') {
                    *d++ = '+';
                    found = 1;
                }
                else {
                    *d++ = c;
                }
                ++s;
                size++;
                slen--;
            }
            *d = '\0';
        }
        else {
            while ((c = *s) && slen) {
                if (TEST_CHAR(c, T_ESCAPE_URLENCODED)) {
                    size += 2;
                    found = 1;
                }
                else if (c == ' ') {
                    found = 1;
                }
                ++s;
                size++;
                slen--;
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

APR_DECLARE(const char *) apr_pescape_urlencoded(apr_pool_t *p, const char *str)
{
    apr_size_t len;

    switch (apr_escape_urlencoded(NULL, str, APR_ESCAPE_STRING, &len)) {
    case APR_SUCCESS: {
        char *encoded = apr_palloc(p, len);
        apr_escape_urlencoded(encoded, str, APR_ESCAPE_STRING, NULL);
        return encoded;
    }
    case APR_NOTFOUND: {
        break;
    }
    }

    return str;
}

APR_DECLARE(apr_status_t) apr_escape_entity(char *escaped, const char *str,
        apr_ssize_t slen, int toasc, apr_size_t *len)
{
    apr_size_t size = 1;
    int found = 0;
    const unsigned char *s = (const unsigned char *) str;
    unsigned char *d = (unsigned char *) escaped;
    unsigned c;

    if (s) {
        if (d) {
            while ((c = *s) && slen) {
                if (TEST_CHAR(c, T_ESCAPE_XML)) {
                    switch (c) {
                    case '>': {
                        memcpy(d, "&gt;", 4);
                        size += 4;
                        d += 4;
                        break;
                    }
                    case '<': {
                        memcpy(d, "&lt;", 4);
                        size += 4;
                        d += 4;
                        break;
                    }
                    case '&': {
                        memcpy(d, "&amp;", 5);
                        size += 5;
                        d += 5;
                        break;
                    }
                    case '\"': {
                        memcpy(d, "&quot;", 6);
                        size += 6;
                        d += 6;
                        break;
                    }
                    case '\'': {
                        memcpy(d, "&apos;", 6);
                        size += 6;
                        d += 6;
                        break;
                    }
                    }
                    found = 1;
                }
                else if (toasc && !apr_isascii(c)) {
                    int offset = apr_snprintf((char *) d, 6, "&#%3.3d;", c);
                    size += offset;
                    d += offset;
                    found = 1;
                }
                else {
                    *d++ = c;
                    size++;
                }
                ++s;
                slen--;
            }
            *d = '\0';
        }
        else {
            while ((c = *s) && slen) {
                if (TEST_CHAR(c, T_ESCAPE_XML)) {
                    switch (c) {
                    case '>': {
                        size += 4;
                        break;
                    }
                    case '<': {
                        size += 4;
                        break;
                    }
                    case '&': {
                        size += 5;
                        break;
                    }
                    case '\"': {
                        size += 6;
                        break;
                    }
                    case '\'': {
                        size += 6;
                        break;
                    }
                    }
                    found = 1;
                }
                else if (toasc && !apr_isascii(c)) {
                    char buf[8];
                    size += apr_snprintf(buf, 6, "&#%3.3d;", c);
                    found = 1;
                }
                else {
                    size++;
                }
                ++s;
                slen--;
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

APR_DECLARE(const char *) apr_pescape_entity(apr_pool_t *p, const char *str,
        int toasc)
{
    apr_size_t len;

    switch (apr_escape_entity(NULL, str, APR_ESCAPE_STRING, toasc, &len)) {
    case APR_SUCCESS: {
        char *cmd = apr_palloc(p, len);
        apr_escape_entity(cmd, str, APR_ESCAPE_STRING, toasc, NULL);
        return cmd;
    }
    case APR_NOTFOUND: {
        break;
    }
    }

    return str;
}

/* maximum length of any ISO-LATIN-1 HTML entity name. */
#define MAXENTLEN (6)

APR_DECLARE(apr_status_t) apr_unescape_entity(char *unescaped, const char *str,
        apr_ssize_t slen, apr_size_t *len)
{
    int found = 0;
    apr_size_t size = 1;
    int val, i, j;
    char *d = unescaped;
    const char *s = str;
    const char *ents;
    static const char * const entlist[MAXENTLEN + 1] =
    {
            NULL, /* 0 */
            NULL, /* 1 */
            "lt\074gt\076", /* 2 */
            "amp\046ETH\320eth\360", /* 3 */
            "quot\042Auml\304Euml\313Iuml\317Ouml\326Uuml\334auml\344euml"
            "\353iuml\357ouml\366uuml\374yuml\377", /* 4 */
            "Acirc\302Aring\305AElig\306Ecirc\312Icirc\316Ocirc\324Ucirc"
            "\333THORN\336szlig\337acirc\342aring\345aelig\346ecirc\352"
            "icirc\356ocirc\364ucirc\373thorn\376", /* 5 */
            "Agrave\300Aacute\301Atilde\303Ccedil\307Egrave\310Eacute\311"
            "Igrave\314Iacute\315Ntilde\321Ograve\322Oacute\323Otilde"
            "\325Oslash\330Ugrave\331Uacute\332Yacute\335agrave\340"
            "aacute\341atilde\343ccedil\347egrave\350eacute\351igrave"
            "\354iacute\355ntilde\361ograve\362oacute\363otilde\365"
            "oslash\370ugrave\371uacute\372yacute\375" /* 6 */
    };

    if (s) {
        if (d) {
            for (; *s != '\0' && slen; s++, d++, size++, slen--) {
                if (*s != '&') {
                    *d = *s;
                    continue;
                }
                /* find end of entity */
                for (i = 1; s[i] != ';' && s[i] != '\0' && (slen - i) != 0;
                        i++) {
                    continue;
                }

                if (s[i] == '\0' || (slen - i) == 0) { /* treat as normal data */
                    *d = *s;
                    continue;
                }

                /* is it numeric ? */
                if (s[1] == '#') {
                    for (j = 2, val = 0; j < i && apr_isdigit(s[j]); j++) {
                        val = val * 10 + s[j] - '0';
                    }
                    s += i;
                    if (j < i || val <= 8 || (val >= 11 && val <= 31)
                            || (val >= 127 && val <= 160) || val >= 256) {
                        d--; /* no data to output */
                        size--;
                    }
                    else {
                        *d = ENCODE_TO_ASCII(val);
                        found = 1;
                    }
                }
                else {
                    j = i - 1;
                    if (j > MAXENTLEN || entlist[j] == NULL) {
                        /* wrong length */
                        *d = '&';
                        continue; /* skip it */
                    }
                    for (ents = entlist[j]; *ents != '\0'; ents += i) {
                        if (strncmp(s + 1, ents, j) == 0) {
                            break;
                        }
                    }

                    if (*ents == '\0') {
                        *d = '&'; /* unknown */
                    }
                    else {
                        *d = ENCODE_TO_ASCII(((const unsigned char *) ents)[j]);
                        s += i;
                        slen -= i;
                        found = 1;
                    }
                }
            }
            *d = '\0';
        }
        else {
            for (; *s != '\0' && slen; s++, size++, slen--) {
                if (*s != '&') {
                    continue;
                }
                /* find end of entity */
                for (i = 1; s[i] != ';' && s[i] != '\0' && (slen - i) != 0;
                        i++) {
                    continue;
                }

                if (s[i] == '\0' || (slen - i) == 0) { /* treat as normal data */
                    continue;
                }

                /* is it numeric ? */
                if (s[1] == '#') {
                    for (j = 2, val = 0; j < i && apr_isdigit(s[j]); j++) {
                        val = val * 10 + s[j] - '0';
                    }
                    s += i;
                    if (j < i || val <= 8 || (val >= 11 && val <= 31)
                            || (val >= 127 && val <= 160) || val >= 256) {
                        /* no data to output */
                        size--;
                    }
                    else {
                        found = 1;
                    }
                }
                else {
                    j = i - 1;
                    if (j > MAXENTLEN || entlist[j] == NULL) {
                        /* wrong length */
                        continue; /* skip it */
                    }
                    for (ents = entlist[j]; *ents != '\0'; ents += i) {
                        if (strncmp(s + 1, ents, j) == 0) {
                            break;
                        }
                    }

                    if (*ents == '\0') {
                        /* unknown */
                    }
                    else {
                        s += i;
                        slen -= i;
                        found = 1;
                    }
                }
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

APR_DECLARE(const char *) apr_punescape_entity(apr_pool_t *p, const char *str)
{
    apr_size_t len;

    switch (apr_unescape_entity(NULL, str, APR_ESCAPE_STRING, &len)) {
    case APR_SUCCESS: {
        char *cmd = apr_palloc(p, len);
        apr_unescape_entity(cmd, str, APR_ESCAPE_STRING, NULL);
        return cmd;
    }
    case APR_NOTFOUND: {
        break;
    }
    }

    return str;
}

APR_DECLARE(apr_status_t) apr_escape_echo(char *escaped, const char *str,
        apr_ssize_t slen, int quote, apr_size_t *len)
{
    apr_size_t size = 1;
    int found = 0;
    const unsigned char *s = (const unsigned char *) str;
    unsigned char *d = (unsigned char *) escaped;
    unsigned c;

    if (s) {
        if (d) {
            while ((c = *s) && slen) {
                if (TEST_CHAR(c, T_ESCAPE_ECHO)) {
                    *d++ = '\\';
                    size++;
                    switch (c) {
                    case '\a':
                        *d++ = 'a';
                        size++;
                        found = 1;
                        break;
                    case '\b':
                        *d++ = 'b';
                        size++;
                        found = 1;
                        break;
                    case '\f':
                        *d++ = 'f';
                        size++;
                        found = 1;
                        break;
                    case '\n':
                        *d++ = 'n';
                        size++;
                        found = 1;
                        break;
                    case '\r':
                        *d++ = 'r';
                        size++;
                        found = 1;
                        break;
                    case '\t':
                        *d++ = 't';
                        size++;
                        found = 1;
                        break;
                    case '\v':
                        *d++ = 'v';
                        size++;
                        found = 1;
                        break;
                    case '\\':
                        *d++ = '\\';
                        size++;
                        found = 1;
                        break;
                    case '"':
                        if (quote) {
                            *d++ = c;
                            size++;
                            found = 1;
                        }
                        else {
                            d[-1] = c;
                        }
                        break;
                    default:
                        c2x(c, 'x', d);
                        d += 3;
                        size += 3;
                        found = 1;
                        break;
                    }
                }
                else {
                    *d++ = c;
                    size++;
                }
                ++s;
                slen--;
            }
            *d = '\0';
        }
        else {
            while ((c = *s) && slen) {
                if (TEST_CHAR(c, T_ESCAPE_ECHO)) {
                    size++;
                    switch (c) {
                    case '\a':
                    case '\b':
                    case '\f':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '\v':
                    case '\\':
                        size++;
                        found = 1;
                        break;
                    case '"':
                        if (quote) {
                            size++;
                            found = 1;
                        }
                        break;
                    default:
                        size += 3;
                        found = 1;
                        break;
                    }
                }
                else {
                    size++;
                }
                ++s;
                slen--;
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

APR_DECLARE(const char *) apr_pescape_echo(apr_pool_t *p, const char *str,
        int quote)
{
    apr_size_t len;

    switch (apr_escape_echo(NULL, str, APR_ESCAPE_STRING, quote, &len)) {
    case APR_SUCCESS: {
        char *cmd = apr_palloc(p, len);
        apr_escape_echo(cmd, str, APR_ESCAPE_STRING, quote, NULL);
        return cmd;
    }
    case APR_NOTFOUND: {
        break;
    }
    }

    return str;
}

APR_DECLARE(apr_status_t) apr_escape_hex(char *dest, const void *src,
        apr_size_t srclen, int colon, apr_size_t *len)
{
    const unsigned char *in = src;
    apr_size_t size;

    if (!src) {
        return APR_NOTFOUND;
    }

    if (dest) {
        for (size = 0; size < srclen; size++) {
            if (colon && size) {
                *dest++ = ':';
            }
            *dest++ = c2x_table[in[size] >> 4];
            *dest++ = c2x_table[in[size] & 0xf];
        }
        *dest = '\0';
    }

    if (len) {
        if (colon && srclen) {
            *len = srclen * 3;
        }
        else {
            *len = srclen * 2 + 1;
        }
    }

    return APR_SUCCESS;
}

APR_DECLARE(const char *) apr_pescape_hex(apr_pool_t *p, const void *src,
        apr_size_t srclen, int colon)
{
    apr_size_t len;

    switch (apr_escape_hex(NULL, src, srclen, colon, &len)) {
    case APR_SUCCESS: {
        char *cmd = apr_palloc(p, len);
        apr_escape_hex(cmd, src, srclen, colon, NULL);
        return cmd;
    }
    case APR_NOTFOUND: {
        break;
    }
    }

    return src;
}

APR_DECLARE(apr_status_t) apr_unescape_hex(void *dest, const char *str,
        apr_ssize_t slen, int colon, apr_size_t *len)
{
    apr_size_t size = 0;
    int flip = 0;
    const unsigned char *s = (const unsigned char *) str;
    unsigned char *d = (unsigned char *) dest;
    unsigned c;
    unsigned char u = 0;

    if (s) {
        if (d) {
            while ((c = *s) && slen) {

                if (!flip) {
                    u = 0;
                }

                if (colon && c == ':' && !flip) {
                    ++s;
                    slen--;
                    continue;
                }
                else if (apr_isdigit(c)) {
                    u |= c - '0';
                }
                else if (apr_isupper(c) && c <= 'F') {
                    u |= c - ('A' - 10);
                }
                else if (apr_islower(c) && c <= 'f') {
                    u |= c - ('a' - 10);
                }
                else {
                    return APR_BADCH;
                }

                if (flip) {
                    *d++ = u;
                    size++;
                }
                else {
                    u <<= 4;
                    *d = u;
                }
                flip = !flip;

                ++s;
                slen--;
            }
        }
        else {
            while ((c = *s) && slen) {

                if (colon && c == ':' && !flip) {
                    ++s;
                    slen--;
                    continue;
                }
                else if (apr_isdigit(c)) {
                    /* valid */
                }
                else if (apr_isupper(c) && c <= 'F') {
                    /* valid */
                }
                else if (apr_islower(c) && c <= 'f') {
                    /* valid */
                }
                else {
                    return APR_BADCH;
                }

                if (flip) {
                    size++;
                }
                flip = !flip;

                ++s;
                slen--;
            }
        }
    }

    if (len) {
        *len = size;
    }
    if (!s) {
        return APR_NOTFOUND;
    }

    return APR_SUCCESS;
}

APR_DECLARE(const void *) apr_punescape_hex(apr_pool_t *p, const char *str,
        int colon, apr_size_t *len)
{
    apr_size_t size;

    switch (apr_unescape_hex(NULL, str, APR_ESCAPE_STRING, colon, &size)) {
    case APR_SUCCESS: {
        void *cmd = apr_palloc(p, size);
        apr_unescape_hex(cmd, str, APR_ESCAPE_STRING, colon, len);
        return cmd;
    }
    case APR_BADCH:
    case APR_NOTFOUND: {
        break;
    }
    }

    return NULL;
}

APR_DECLARE(apr_status_t) apr_escape_ldap(char *escaped, const void *str,
        apr_ssize_t slen, int flags, apr_size_t *len)
{
    apr_size_t size = 1;
    int found = 0;
    const unsigned char *s = (const unsigned char *) str;
    unsigned char *d = (unsigned char *) escaped;
    unsigned c;

    if (s) {
        if (d) {
            while (((c = *s) && slen) || (slen > 0)) {
                if (((flags & APR_ESCAPE_LDAP_DN) && TEST_CHAR(c, T_ESCAPE_LDAP_DN))
                     || ((flags & APR_ESCAPE_LDAP_FILTER) && TEST_CHAR(c, T_ESCAPE_LDAP_FILTER))) {
                    d = c2x(c, '\\', d);
                    size += 2;
                    found = 1;
                }
                else {
                    *d++ = c;
                }
                ++s;
                size++;
                slen--;
            }
            *d = '\0';
        }
        else {
            while (((c = *s) && slen) || (slen > 0)) {
                if (((flags & APR_ESCAPE_LDAP_DN) && TEST_CHAR(c, T_ESCAPE_LDAP_DN)) 
                     || ((flags & APR_ESCAPE_LDAP_FILTER) && TEST_CHAR(c, T_ESCAPE_LDAP_FILTER))) {
                    size += 2;
                    found = 1;
                }
                ++s;
                size++;
                slen--;
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

APR_DECLARE(const char *) apr_pescape_ldap(apr_pool_t *p, const void *src,
        apr_ssize_t srclen, int flags)
{
    apr_size_t len;

    switch (apr_escape_ldap(NULL, src, srclen, flags, &len)) {
    case APR_SUCCESS: {
        char *encoded = apr_palloc(p, len);
        apr_escape_ldap(encoded, src, srclen, flags, NULL);
        return encoded;
    }
    case APR_NOTFOUND: {
        break;
    }
    }

    return src;
}

