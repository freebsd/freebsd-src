/*
 * Copyright (c) 2000,2004 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $Id: rtbl.h 17760 2006-06-30 13:42:39Z lha $ */

#ifndef __rtbl_h__
#define __rtbl_h__

#ifndef ROKEN_LIB_FUNCTION
#ifdef _WIN32
#define ROKEN_LIB_FUNCTION _stdcall
#else
#define ROKEN_LIB_FUNCTION
#endif
#endif

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct rtbl_data;
typedef struct rtbl_data *rtbl_t;

#define RTBL_ALIGN_LEFT		0
#define RTBL_ALIGN_RIGHT	1

/* flags */
#define RTBL_HEADER_STYLE_NONE	1

int ROKEN_LIB_FUNCTION
rtbl_add_column (rtbl_t, const char*, unsigned int);

int ROKEN_LIB_FUNCTION
rtbl_add_column_by_id (rtbl_t, unsigned int, const char*, unsigned int);

int ROKEN_LIB_FUNCTION
rtbl_add_column_entryv_by_id (rtbl_t table, unsigned int id,
			      const char *fmt, ...)
	__attribute__ ((format (printf, 3, 0)));

int ROKEN_LIB_FUNCTION
rtbl_add_column_entry (rtbl_t, const char*, const char*);

int ROKEN_LIB_FUNCTION
rtbl_add_column_entryv (rtbl_t, const char*, const char*, ...)
	__attribute__ ((format (printf, 3, 0)));

int ROKEN_LIB_FUNCTION
rtbl_add_column_entry_by_id (rtbl_t, unsigned int, const char*);

rtbl_t ROKEN_LIB_FUNCTION
rtbl_create (void);

void ROKEN_LIB_FUNCTION
rtbl_destroy (rtbl_t);

int ROKEN_LIB_FUNCTION
rtbl_format (rtbl_t, FILE*);

unsigned int ROKEN_LIB_FUNCTION
rtbl_get_flags (rtbl_t);

int ROKEN_LIB_FUNCTION
rtbl_new_row (rtbl_t);

int ROKEN_LIB_FUNCTION
rtbl_set_column_affix_by_id (rtbl_t, unsigned int, const char*, const char*);

int ROKEN_LIB_FUNCTION
rtbl_set_column_prefix (rtbl_t, const char*, const char*);

void ROKEN_LIB_FUNCTION
rtbl_set_flags (rtbl_t, unsigned int);

int ROKEN_LIB_FUNCTION
rtbl_set_prefix (rtbl_t, const char*);

int ROKEN_LIB_FUNCTION
rtbl_set_separator (rtbl_t, const char*);

#ifdef __cplusplus
}
#endif

#endif /* __rtbl_h__ */
