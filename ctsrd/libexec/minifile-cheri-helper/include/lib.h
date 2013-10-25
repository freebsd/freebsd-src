/*-
 * Copyright (c) 2011 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#ifndef _CHERIDEMO_LIB_H_
#define	_CHERIDEMO_LIB_H_

#include "mips.h"
#include "stdarg.h"

/*
 * General-purpose routines.  These routines are available in both kernel and
 * userspace.
 */
void	*memcpy(void *dest, const void *src, size_t n);
void	*memset(void *s, int c, size_t n);
size_t	 strlen(const char *str);

int	kvprintf(char const *fmt, void (*func)(int, void*), void *arg,
	    int radix, va_list ap);
int	sprintf(char *buf, const char *cfmt, ...);
int	vsprintf(char *buf, const char *cfmt, va_list ap);
int	snprintf(char *str, size_t size, const char *format, ...);
int	vsnprintf(char *str, size_t size, const char *format, va_list ap);

/*
 * General-purpose inlines.
 */
static inline int
imax(int a, int b)
{

	return (a > b ? a : b);
}

/*
 * Kernel library routines.
 */
int	kernel_printf(const char *fmt, ...);
int	kernel_vprintf(const char *fmt, va_list ap);

/*
 * Routines specific to "userspace".
 */
int		user_printf(const char *fmt, ...);
int		user_vprintf(const char *fmt, va_list ap);
register_t	user_syscall(register_t sysno);
register_t	user_syscall1(register_t sysno, register_t a1);
void		user_putc(char ch);
void		user_puts(const char *str);

void		user_fb_putdword(void *framebufferp, u_int x, u_int y,
		    uint64_t d);
void		user_fb_puthword(void *framebufferp, u_int x, u_int y,
		    uint16_t h);

uint32_t	user_ts_get_pixel(void);

/*
 * Simulator routines.
 */
void	simulator_dump_regs(void);
void	simulator_dump_cp2(void);
void	simulator_terminate(void);

#endif /* _CHERIDEMO_LIB_H_ */
