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

/*
 * apr_uri.c: URI related utility things
 * 
 */

#include <stdlib.h>

#include "apu.h"
#include "apr.h"
#include "apr_general.h"
#include "apr_strings.h"

#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "apr_uri.h"

typedef struct schemes_t schemes_t;

/** Structure to store various schemes and their default ports */
struct schemes_t {
    /** The name of the scheme */
    const char *name;
    /** The default port for the scheme */
    apr_port_t default_port;
};

/* Some WWW schemes and their default ports; this is basically /etc/services */
/* This will become global when the protocol abstraction comes */
/* As the schemes are searched by a linear search, */
/* they are sorted by their expected frequency */
static schemes_t schemes[] =
{
    {"http",     APR_URI_HTTP_DEFAULT_PORT},
    {"ftp",      APR_URI_FTP_DEFAULT_PORT},
    {"https",    APR_URI_HTTPS_DEFAULT_PORT},
    {"gopher",   APR_URI_GOPHER_DEFAULT_PORT},
    {"ldap",     APR_URI_LDAP_DEFAULT_PORT},
    {"nntp",     APR_URI_NNTP_DEFAULT_PORT},
    {"snews",    APR_URI_SNEWS_DEFAULT_PORT},
    {"imap",     APR_URI_IMAP_DEFAULT_PORT},
    {"pop",      APR_URI_POP_DEFAULT_PORT},
    {"sip",      APR_URI_SIP_DEFAULT_PORT},
    {"rtsp",     APR_URI_RTSP_DEFAULT_PORT},
    {"wais",     APR_URI_WAIS_DEFAULT_PORT},
    {"z39.50r",  APR_URI_WAIS_DEFAULT_PORT},
    {"z39.50s",  APR_URI_WAIS_DEFAULT_PORT},
    {"prospero", APR_URI_PROSPERO_DEFAULT_PORT},
    {"nfs",      APR_URI_NFS_DEFAULT_PORT},
    {"tip",      APR_URI_TIP_DEFAULT_PORT},
    {"acap",     APR_URI_ACAP_DEFAULT_PORT},
    {"telnet",   APR_URI_TELNET_DEFAULT_PORT},
    {"ssh",      APR_URI_SSH_DEFAULT_PORT},
    { NULL, 0xFFFF }     /* unknown port */
};

APU_DECLARE(apr_port_t) apr_uri_port_of_scheme(const char *scheme_str)
{
    schemes_t *scheme;

    if (scheme_str) {
        for (scheme = schemes; scheme->name != NULL; ++scheme) {
            if (strcasecmp(scheme_str, scheme->name) == 0) {
                return scheme->default_port;
            }
        }
    }
    return 0;
}

/* Unparse a apr_uri_t structure to an URI string.
 * Optionally suppress the password for security reasons.
 */
APU_DECLARE(char *) apr_uri_unparse(apr_pool_t *p, 
                                    const apr_uri_t *uptr, 
                                    unsigned flags)
{
    char *ret = "";

    /* If suppressing the site part, omit both user name & scheme://hostname */
    if (!(flags & APR_URI_UNP_OMITSITEPART)) {

        /* Construct a "user:password@" string, honoring the passed
         * APR_URI_UNP_ flags: */
        if (uptr->user || uptr->password) {
            ret = apr_pstrcat(p,
                      (uptr->user     && !(flags & APR_URI_UNP_OMITUSER))
                          ? uptr->user : "",
                      (uptr->password && !(flags & APR_URI_UNP_OMITPASSWORD))
                          ? ":" : "",
                      (uptr->password && !(flags & APR_URI_UNP_OMITPASSWORD))
                          ? ((flags & APR_URI_UNP_REVEALPASSWORD)
                              ? uptr->password : "XXXXXXXX")
                          : "",
                      ((uptr->user     && !(flags & APR_URI_UNP_OMITUSER)) ||
                       (uptr->password && !(flags & APR_URI_UNP_OMITPASSWORD)))
                          ? "@" : "", 
                      NULL);
        }

        /* Construct scheme://site string */
        if (uptr->hostname) {
            int is_default_port;
            const char *lbrk = "", *rbrk = "";

            if (strchr(uptr->hostname, ':')) { /* v6 literal */
                lbrk = "[";
                rbrk = "]";
            }

            is_default_port =
                (uptr->port_str == NULL ||
                 uptr->port == 0 ||
                 uptr->port == apr_uri_port_of_scheme(uptr->scheme));

            ret = apr_pstrcat(p, "//", ret, lbrk, uptr->hostname, rbrk,
                        is_default_port ? "" : ":",
                        is_default_port ? "" : uptr->port_str,
                        NULL);
        }
	if (uptr->scheme) {
	    ret = apr_pstrcat(p, uptr->scheme, ":", ret, NULL);
	}
    }
    
    /* Should we suppress all path info? */
    if (!(flags & APR_URI_UNP_OMITPATHINFO)) {
        /* Append path, query and fragment strings: */
        ret = apr_pstrcat(p,
                          ret,
                          (uptr->path)
                              ? uptr->path : "",
                          (uptr->query    && !(flags & APR_URI_UNP_OMITQUERY))
                              ? "?" : "",
                          (uptr->query    && !(flags & APR_URI_UNP_OMITQUERY))
                              ? uptr->query : "",
                          (uptr->fragment && !(flags & APR_URI_UNP_OMITQUERY))
                              ? "#" : NULL,
                          (uptr->fragment && !(flags & APR_URI_UNP_OMITQUERY))
                              ? uptr->fragment : NULL,
                          NULL);
    }
    return ret;
}

/* Here is the hand-optimized parse_uri_components().  There are some wild
 * tricks we could pull in assembly language that we don't pull here... like we
 * can do word-at-time scans for delimiter characters using the same technique
 * that fast memchr()s use.  But that would be way non-portable. -djg
 */

/* We have a apr_table_t that we can index by character and it tells us if the
 * character is one of the interesting delimiters.  Note that we even get
 * compares for NUL for free -- it's just another delimiter.
 */

#define T_COLON           0x01        /* ':' */
#define T_SLASH           0x02        /* '/' */
#define T_QUESTION        0x04        /* '?' */
#define T_HASH            0x08        /* '#' */
#define T_NUL             0x80        /* '\0' */

#if APR_CHARSET_EBCDIC
/* Delimiter table for the EBCDIC character set */
static const unsigned char uri_delims[256] = {
    T_NUL,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,T_SLASH,0,0,0,0,0,0,0,0,0,0,0,0,0,T_QUESTION,
    0,0,0,0,0,0,0,0,0,0,T_COLON,T_HASH,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
#else
/* Delimiter table for the ASCII character set */
static const unsigned char uri_delims[256] = {
    T_NUL,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,T_HASH,0,0,0,0,0,0,0,0,0,0,0,T_SLASH,
    0,0,0,0,0,0,0,0,0,0,T_COLON,0,0,0,0,T_QUESTION,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
#endif


/* it works like this:
    if (uri_delims[ch] & NOTEND_foobar) {
        then we're not at a delimiter for foobar
    }
*/

/* Note that we optimize the scheme scanning here, we cheat and let the
 * compiler know that it doesn't have to do the & masking.
 */
#define NOTEND_SCHEME     (0xff)
#define NOTEND_HOSTINFO   (T_SLASH | T_QUESTION | T_HASH | T_NUL)
#define NOTEND_PATH       (T_QUESTION | T_HASH | T_NUL)

/* parse_uri_components():
 * Parse a given URI, fill in all supplied fields of a uri_components
 * structure. This eliminates the necessity of extracting host, port,
 * path, query info repeatedly in the modules.
 * Side effects:
 *  - fills in fields of uri_components *uptr
 *  - none on any of the r->* fields
 */
APU_DECLARE(apr_status_t) apr_uri_parse(apr_pool_t *p, const char *uri, 
                                        apr_uri_t *uptr)
{
    const char *s;
    const char *s1;
    const char *hostinfo;
    char *endstr;
    int port;
    int v6_offset1 = 0, v6_offset2 = 0;

    /* Initialize the structure. parse_uri() and parse_uri_components()
     * can be called more than once per request.
     */
    memset (uptr, '\0', sizeof(*uptr));
    uptr->is_initialized = 1;

    /* We assume the processor has a branch predictor like most --
     * it assumes forward branches are untaken and backwards are taken.  That's
     * the reason for the gotos.  -djg
     */
    if (uri[0] == '/') {
        /* RFC2396 #4.3 says that two leading slashes mean we have an
         * authority component, not a path!  Fixing this looks scary
         * with the gotos here.  But if the existing logic is valid,
         * then presumably a goto pointing to deal_with_authority works.
         *
         * RFC2396 describes this as resolving an ambiguity.  In the
         * case of three or more slashes there would seem to be no
         * ambiguity, so it is a path after all.
         */
        if (uri[1] == '/' && uri[2] != '/') {
            s = uri + 2 ;
            goto deal_with_authority ;
        }

deal_with_path:
        /* we expect uri to point to first character of path ... remember
         * that the path could be empty -- http://foobar?query for example
         */
        s = uri;
        while ((uri_delims[*(unsigned char *)s] & NOTEND_PATH) == 0) {
            ++s;
        }
        if (s != uri) {
            uptr->path = apr_pstrmemdup(p, uri, s - uri);
        }
        if (*s == 0) {
            return APR_SUCCESS;
        }
        if (*s == '?') {
            ++s;
            s1 = strchr(s, '#');
            if (s1) {
                uptr->fragment = apr_pstrdup(p, s1 + 1);
                uptr->query = apr_pstrmemdup(p, s, s1 - s);
            }
            else {
                uptr->query = apr_pstrdup(p, s);
            }
            return APR_SUCCESS;
        }
        /* otherwise it's a fragment */
        uptr->fragment = apr_pstrdup(p, s + 1);
        return APR_SUCCESS;
    }

    /* find the scheme: */
    s = uri;
    while ((uri_delims[*(unsigned char *)s] & NOTEND_SCHEME) == 0) {
        ++s;
    }
    /* scheme must be non-empty and followed by : */
    if (s == uri || s[0] != ':') {
        goto deal_with_path;        /* backwards predicted taken! */
    }

    uptr->scheme = apr_pstrmemdup(p, uri, s - uri);
    if (s[1] != '/' || s[2] != '/') {
        uri = s + 1;
        goto deal_with_path;
    }

    s += 3;

deal_with_authority:
    hostinfo = s;
    while ((uri_delims[*(unsigned char *)s] & NOTEND_HOSTINFO) == 0) {
        ++s;
    }
    uri = s;        /* whatever follows hostinfo is start of uri */
    uptr->hostinfo = apr_pstrmemdup(p, hostinfo, uri - hostinfo);

    /* If there's a username:password@host:port, the @ we want is the last @...
     * too bad there's no memrchr()... For the C purists, note that hostinfo
     * is definately not the first character of the original uri so therefore
     * &hostinfo[-1] < &hostinfo[0] ... and this loop is valid C.
     */
    do {
        --s;
    } while (s >= hostinfo && *s != '@');
    if (s < hostinfo) {
        /* again we want the common case to be fall through */
deal_with_host:
        /* We expect hostinfo to point to the first character of
         * the hostname.  If there's a port it is the first colon,
         * except with IPv6.
         */
        if (*hostinfo == '[') {
            v6_offset1 = 1;
            v6_offset2 = 2;
            s = memchr(hostinfo, ']', uri - hostinfo);
            if (s == NULL) {
                return APR_EGENERAL;
            }
            if (*++s != ':') {
                s = NULL; /* no port */
            }
        }
        else {
            s = memchr(hostinfo, ':', uri - hostinfo);
        }
        if (s == NULL) {
            /* we expect the common case to have no port */
            uptr->hostname = apr_pstrmemdup(p,
                                            hostinfo + v6_offset1,
                                            uri - hostinfo - v6_offset2);
            goto deal_with_path;
        }
        uptr->hostname = apr_pstrmemdup(p,
                                        hostinfo + v6_offset1,
                                        s - hostinfo - v6_offset2);
        ++s;
        uptr->port_str = apr_pstrmemdup(p, s, uri - s);
        if (uri != s) {
            port = strtol(uptr->port_str, &endstr, 10);
            uptr->port = port;
            if (*endstr == '\0') {
                goto deal_with_path;
            }
            /* Invalid characters after ':' found */
            return APR_EGENERAL;
        }
        uptr->port = apr_uri_port_of_scheme(uptr->scheme);
        goto deal_with_path;
    }

    /* first colon delimits username:password */
    s1 = memchr(hostinfo, ':', s - hostinfo);
    if (s1) {
        uptr->user = apr_pstrmemdup(p, hostinfo, s1 - hostinfo);
        ++s1;
        uptr->password = apr_pstrmemdup(p, s1, s - s1);
    }
    else {
        uptr->user = apr_pstrmemdup(p, hostinfo, s - hostinfo);
    }
    hostinfo = s + 1;
    goto deal_with_host;
}

/* Special case for CONNECT parsing: it comes with the hostinfo part only */
/* See the INTERNET-DRAFT document "Tunneling SSL Through a WWW Proxy"
 * currently at http://www.mcom.com/newsref/std/tunneling_ssl.html
 * for the format of the "CONNECT host:port HTTP/1.0" request
 */
APU_DECLARE(apr_status_t) apr_uri_parse_hostinfo(apr_pool_t *p, 
                                                 const char *hostinfo, 
                                                 apr_uri_t *uptr)
{
    const char *s;
    char *endstr;
    const char *rsb;
    int v6_offset1 = 0;

    /* Initialize the structure. parse_uri() and parse_uri_components()
     * can be called more than once per request.
     */
    memset(uptr, '\0', sizeof(*uptr));
    uptr->is_initialized = 1;
    uptr->hostinfo = apr_pstrdup(p, hostinfo);

    /* We expect hostinfo to point to the first character of
     * the hostname.  There must be a port, separated by a colon
     */
    if (*hostinfo == '[') {
        if ((rsb = strchr(hostinfo, ']')) == NULL ||
            *(rsb + 1) != ':') {
            return APR_EGENERAL;
        }
        /* literal IPv6 address */
        s = rsb + 1;
        ++hostinfo;
        v6_offset1 = 1;
    }
    else {
        s = strchr(hostinfo, ':');
    }
    if (s == NULL) {
        return APR_EGENERAL;
    }
    uptr->hostname = apr_pstrndup(p, hostinfo, s - hostinfo - v6_offset1);
    ++s;
    uptr->port_str = apr_pstrdup(p, s);
    if (*s != '\0') {
        uptr->port = (unsigned short) strtol(uptr->port_str, &endstr, 10);
        if (*endstr == '\0') {
            return APR_SUCCESS;
        }
        /* Invalid characters after ':' found */
    }
    return APR_EGENERAL;
}
