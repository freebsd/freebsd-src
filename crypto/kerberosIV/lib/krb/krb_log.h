/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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

/* $Id: krb_log.h,v 1.3 1999/12/02 16:58:42 joda Exp $ */

#include <krb.h>

#ifndef __KRB_LOG_H__
#define __KRB_LOG_H__

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(X)
#endif

__BEGIN_DECLS

/* logging.c */

typedef int (*krb_log_func_t) __P((FILE *, const char *, va_list));

typedef krb_log_func_t krb_warnfn_t;

struct krb_log_facility;

int krb_vlogger __P((struct krb_log_facility*, const char *, va_list)) 
	__attribute__ ((format (printf, 2, 0)));
int krb_logger __P((struct krb_log_facility*, const char *, ...))
	__attribute__ ((format (printf, 2, 3)));
int krb_openlog __P((struct krb_log_facility*, char*, FILE*, krb_log_func_t));

void krb_set_warnfn  __P((krb_warnfn_t));
krb_warnfn_t krb_get_warnfn  __P((void));
void krb_warning  __P((const char*, ...))
	__attribute__ ((format (printf, 1, 2)));

void kset_logfile __P((char*));
void krb_log __P((const char*, ...))
	__attribute__ ((format (printf, 1, 2)));
char *klog __P((int, const char*, ...))
	__attribute__ ((format (printf, 2, 3)));

__END_DECLS

#endif /* __KRB_LOG_H__ */
