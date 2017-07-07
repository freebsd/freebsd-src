/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1994 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (C) 2003, 2004 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "autoconf.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>

/* need struct timeval */
#if HAVE_TIME_H && (!HAVE_SYS_TIME_H || TIME_WITH_SYS_TIME)
# include <time.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <gssapi/gssapi_generic.h>
#include "gss-misc.h"
/* for store_32_be */
#include "k5-platform.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
extern char *malloc();
#endif

FILE *display_file;

gss_buffer_desc empty_token_buf = { 0, (void *)"" };
gss_buffer_t empty_token = &empty_token_buf;

static void display_status_1(char *m, OM_uint32 code, int type);

static int
write_all(int fildes, char *buf, unsigned int nbyte)
{
    int ret;
    char *ptr;

    for (ptr = buf; nbyte; ptr += ret, nbyte -= ret) {
        ret = send(fildes, ptr, nbyte, 0);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            return ret;
        } else if (ret == 0) {
            return ptr - buf;
        }
    }

    return ptr - buf;
}

static int
read_all(int fildes, char *buf, unsigned int nbyte)
{
    int ret;
    char *ptr;
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(fildes, &rfds);
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    for (ptr = buf; nbyte; ptr += ret, nbyte -= ret) {
        if (select(FD_SETSIZE, &rfds, NULL, NULL, &tv) <= 0 ||
            !FD_ISSET(fildes, &rfds))
            return ptr - buf;
        ret = recv(fildes, ptr, nbyte, 0);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            return ret;
        } else if (ret == 0) {
            return ptr - buf;
        }
    }

    return ptr - buf;
}

/*
 * Function: send_token
 *
 * Purpose: Writes a token to a file descriptor.
 *
 * Arguments:
 *
 *      s               (r) an open file descriptor
 *      flags           (r) the flags to write
 *      tok             (r) the token to write
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * If the flags are non-null, send_token writes the token flags (a
 * single byte, even though they're passed in in an integer). Next,
 * the token length (as a network long) and then the token data are
 * written to the file descriptor s.  It returns 0 on success, and -1
 * if an error occurs or if it could not write all the data.
 */
int
send_token(int s, int flags, gss_buffer_t tok)
{
    int ret;
    unsigned char char_flags = (unsigned char)flags;
    unsigned char lenbuf[4];

    if (char_flags) {
        ret = write_all(s, (char *)&char_flags, 1);
        if (ret != 1) {
            perror("sending token flags");
            return -1;
        }
    }
    if (tok->length > 0xffffffffUL)
        abort();
    store_32_be(tok->length, lenbuf);
    ret = write_all(s, (char *)lenbuf, 4);
    if (ret < 0) {
        perror("sending token length");
        return -1;
    } else if (ret != 4) {
        if (display_file) {
            fprintf(display_file,
                    "sending token length: %d of %d bytes written\n", ret, 4);
        }
        return -1;
    }

    ret = write_all(s, tok->value, tok->length);
    if (ret < 0) {
        perror("sending token data");
        return -1;
    } else if ((size_t)ret != tok->length) {
        if (display_file) {
            fprintf(display_file,
                    "sending token data: %d of %d bytes written\n",
                    ret, (int)tok->length);
        }
        return -1;
    }

    return 0;
}

/*
 * Function: recv_token
 *
 * Purpose: Reads a token from a file descriptor.
 *
 * Arguments:
 *
 *      s               (r) an open file descriptor
 *      flags           (w) the read flags
 *      tok             (w) the read token
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * recv_token reads the token flags (a single byte, even though
 * they're stored into an integer, then reads the token length (as a
 * network long), allocates memory to hold the data, and then reads
 * the token data from the file descriptor s.  It blocks to read the
 * length and data, if necessary.  On a successful return, the token
 * should be freed with gss_release_buffer.  It returns 0 on success,
 * and -1 if an error occurs or if it could not read all the data.
 */
int
recv_token(int s, int *flags, gss_buffer_t tok)
{
    int ret;
    unsigned char char_flags;
    unsigned char lenbuf[4];

    ret = read_all(s, (char *)&char_flags, 1);
    if (ret < 0) {
        perror("reading token flags");
        return -1;
    } else if (!ret) {
        if (display_file)
            fputs("reading token flags: 0 bytes read\n", display_file);
        return -1;
    } else {
        *flags = char_flags;
    }

    if (char_flags == 0) {
        lenbuf[0] = 0;
        ret = read_all(s, (char *)&lenbuf[1], 3);
        if (ret < 0) {
            perror("reading token length");
            return -1;
        } else if (ret != 3) {
            if (display_file) {
                fprintf(display_file,
                        "reading token length: %d of %d bytes read\n", ret, 3);
            }
            return -1;
        }
    } else {
        ret = read_all(s, (char *)lenbuf, 4);
        if (ret < 0) {
            perror("reading token length");
            return -1;
        } else if (ret != 4) {
            if (display_file) {
                fprintf(display_file,
                        "reading token length: %d of %d bytes read\n", ret, 4);
            }
            return -1;
        }
    }

    tok->length = load_32_be(lenbuf);
    tok->value = malloc(tok->length ? tok->length : 1);
    if (tok->length && tok->value == NULL) {
        if (display_file)
            fprintf(display_file, "Out of memory allocating token data\n");
        return -1;
    }

    ret = read_all(s, (char *)tok->value, tok->length);
    if (ret < 0) {
        perror("reading token data");
        free(tok->value);
        return -1;
    } else if ((size_t)ret != tok->length) {
        fprintf(stderr, "sending token data: %d of %d bytes written\n",
                ret, (int)tok->length);
        free(tok->value);
        return -1;
    }

    return 0;
}

static void
display_status_1(char *m, OM_uint32 code, int type)
{
    OM_uint32 min_stat;
    gss_buffer_desc msg;
    OM_uint32 msg_ctx;

    msg_ctx = 0;
    while (1) {
        (void)gss_display_status(&min_stat, code, type, GSS_C_NULL_OID,
                                 &msg_ctx, &msg);
        if (display_file) {
            fprintf(display_file, "GSS-API error %s: %s\n", m,
                    (char *)msg.value);
        }
        (void)gss_release_buffer(&min_stat, &msg);

        if (!msg_ctx)
            break;
    }
}

/*
 * Function: display_status
 *
 * Purpose: displays GSS-API messages
 *
 * Arguments:
 *
 *      msg             a string to be displayed with the message
 *      maj_stat        the GSS-API major status code
 *      min_stat        the GSS-API minor status code
 *
 * Effects:
 *
 * The GSS-API messages associated with maj_stat and min_stat are
 * displayed on stderr, each preceeded by "GSS-API error <msg>: " and
 * followed by a newline.
 */
void
display_status(char *msg, OM_uint32 maj_stat, OM_uint32 min_stat)
{
    display_status_1(msg, maj_stat, GSS_C_GSS_CODE);
    display_status_1(msg, min_stat, GSS_C_MECH_CODE);
}

/*
 * Function: display_ctx_flags
 *
 * Purpose: displays the flags returned by context initation in
 *          a human-readable form
 *
 * Arguments:
 *
 *      int             ret_flags
 *
 * Effects:
 *
 * Strings corresponding to the context flags are printed on
 * stdout, preceded by "context flag: " and followed by a newline
 */

void
display_ctx_flags(OM_uint32 flags)
{
    if (flags & GSS_C_DELEG_FLAG)
        fprintf(display_file, "context flag: GSS_C_DELEG_FLAG\n");
    if (flags & GSS_C_MUTUAL_FLAG)
        fprintf(display_file, "context flag: GSS_C_MUTUAL_FLAG\n");
    if (flags & GSS_C_REPLAY_FLAG)
        fprintf(display_file, "context flag: GSS_C_REPLAY_FLAG\n");
    if (flags & GSS_C_SEQUENCE_FLAG)
        fprintf(display_file, "context flag: GSS_C_SEQUENCE_FLAG\n");
    if (flags & GSS_C_CONF_FLAG)
        fprintf(display_file, "context flag: GSS_C_CONF_FLAG \n");
    if (flags & GSS_C_INTEG_FLAG)
        fprintf(display_file, "context flag: GSS_C_INTEG_FLAG \n");
}

void
print_token(gss_buffer_t tok)
{
    size_t i;
    unsigned char *p = tok->value;

    if (!display_file)
        return;
    for (i = 0; i < tok->length; i++, p++) {
        fprintf(display_file, "%02x ", *p);
        if (i % 16 == 15) {
            fprintf(display_file, "\n");
        }
    }
    fprintf(display_file, "\n");
    fflush(display_file);
}

#ifdef _WIN32
#include <sys\timeb.h>
#include <time.h>

int
gettimeofday(struct timeval *tv, void *ignore_tz)
{
    struct _timeb tb;

    _tzset();
    _ftime(&tb);
    if (tv) {
        tv->tv_sec = tb.time;
        tv->tv_usec = tb.millitm * 1000;
    }
    return 0;
}

#endif /* _WIN32 */
