/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _KERNEL_LOG_H_
#define _KERNEL_LOG_H_

#include "kernel_interface.h"

/* Use this unsigned int and its constants to set the log output  *
 * verbosity.  Use of the log_* functions should be surrounded by *
 * if statements of the form "if( verbose & VERBOSE_FOO )" where  *
 * VERBOSE_FOO is the constant below which corresponds to the     *
 * type of event you're logging.                                  */
#define VERBOSE_NOLOG       0x00000000 /* no log output, please. */
#define VERBOSE_DEMOTE_DENY 0x00000001 /* log demotions and access denials. */
#define VERBOSE_PIPE        0x00000002 /* log changes to pipe "levels". */
#define VERBOSE_LOG_SOCKETS 0x00000004 /* log UNIX domain socket setup. */
#define VERBOSE_LOG_OBJECTS 0x00000008 /* log opening of objects. */
#ifdef TRUST
#define VERBOSE_TRUST       0x00000020 /* log when trust stops demotion. */
#endif

#ifndef VERBOSITY_SETTING
#define	VERBOSITY_SETTING(level) extern unsigned int lomac_verbose_##level;
#endif
VERBOSITY_SETTING(demote_deny);
VERBOSITY_SETTING(log_sockets);
VERBOSITY_SETTING(log_objects);
#ifdef LOMAC_DEBUG_PIPE
VERBOSITY_SETTING(pipe);
#endif
#ifdef TRUST
VERBOSITY_SETTING(trust);
#endif

lomac_log_t *log_start( void );
void log_append_string( lomac_log_t *s, const char *data_s );
void log_append_int( lomac_log_t *s, int data );
void log_append_subject_id( lomac_log_t *s, const lomac_subject_t *p_subject );
void log_append_object_id( lomac_log_t *s, const lomac_object_t *p_object );
void log_print( lomac_log_t *s );

#endif
