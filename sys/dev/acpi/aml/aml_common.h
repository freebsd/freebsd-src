/*-
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: aml_common.h,v 1.4 2000/08/08 14:12:05 iwasaki Exp $
 *	$FreeBSD$
 */

#ifndef _AML_COMMON_H_
#define _AML_COMMON_H_

#ifdef _KERNEL
#define AML_SYSABORT() do {						\
	printf("aml: fatal errer at %s:%d\n", __FILE__, __LINE__);	\
	panic("panic in AML interpreter!");				\
} while(0)
#define AML_SYSASSERT(x) do {						\
	if (!(x)) {							\
		AML_SYSABORT();						\
	}								\
} while(0)
#define AML_SYSERRX(eval, fmt, args...) do {				\
	printf(fmt, args);						\
} while(0)
#define AML_DEBUGGER(x, y)	/* no debugger in kernel */
#else /* !_KERNEL */
#define AML_SYSASSERT(x)	assert(x)
#define AML_SYSABORT()  	abort()
#define AML_SYSERRX(eval, fmt, args...)	errx(eval, fmt, args)
#define AML_DEBUGGER(x, y)	aml_dbgr(x, y)
#endif /* _KERNEL */

union	aml_object;
struct	aml_name;

extern int	aml_debug;

#define AML_DEBUGPRINT(args...) do {					\
	if (aml_debug) {						\
		printf(args);						\
	}								\
} while(0)

void		 aml_showobject(union aml_object *);
void		 aml_showtree(struct aml_name *, int);
int		 aml_print_curname(struct aml_name *);
void		 aml_print_namestring(u_int8_t *);
void		 aml_print_indent(int);

u_int32_t	 aml_bufferfield_read(u_int8_t *, u_int32_t, u_int32_t);
int		 aml_bufferfield_write(u_int32_t, u_int8_t *,
				       u_int32_t, u_int32_t);

#endif /* !_AML_COMMON_H_ */
