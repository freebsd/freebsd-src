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

#ifndef _GSSMISC_H_
#define _GSSMISC_H_

#include <gssapi/gssapi_generic.h>
#include <stdio.h>

extern FILE *display_file;

int send_token(int s, int flags, gss_buffer_t tok);
int recv_token(int s, int *flags, gss_buffer_t tok);
void display_status(char *msg, OM_uint32 maj_stat, OM_uint32 min_stat);
void display_ctx_flags(OM_uint32 flags);
void print_token(gss_buffer_t tok);

/* Token types */
#define TOKEN_NOOP		(1<<0)
#define TOKEN_CONTEXT		(1<<1)
#define TOKEN_DATA		(1<<2)
#define TOKEN_MIC		(1<<3)

/* Token flags */
#define TOKEN_CONTEXT_NEXT	(1<<4)
#define TOKEN_WRAPPED		(1<<5)
#define TOKEN_ENCRYPTED		(1<<6)
#define TOKEN_SEND_MIC		(1<<7)

extern gss_buffer_t empty_token;

#endif
