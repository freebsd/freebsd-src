/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __KDUMP_H__
#define __KDUMP_H__

extern bool decimal, fancy, resolv;

#define	_print_number64(first,i,n,c,d) do {				\
	uint64_t __v;							\
									\
	if (quad_align && (((ptrdiff_t)((i) - (first))) & 1) == 1) {	\
		(i)++;							\
		(n)--;							\
	}								\
	if (quad_slots == 2)						\
		__v = (uint64_t)(uint32_t)(i)[0] |			\
		    ((uint64_t)(uint32_t)(i)[1]) << 32;			\
	else								\
		__v = (uint64_t)*(i);					\
	if (d)								\
		printf("%c%jd", (c), (intmax_t)__v);			\
	else								\
		printf("%c%#jx", (c), (uintmax_t)__v);			\
	(i) += quad_slots;						\
	(n) -= quad_slots;						\
	(c) = ',';							\
} while (0)

#define _print_number(i,n,c,d) do {					\
	if (d)								\
		printf("%c%jd", c, (intmax_t)*i);			\
	else								\
		printf("%c%#jx", c, (uintmax_t)(u_register_t)*i);	\
	i++;								\
	n--;								\
	c = ',';							\
} while (0)

#define	print_number(i,n,c)		_print_number(i,n,c,decimal)
#define	print_decimal_number(i,n,c)	_print_number(i,n,c,true)
#define	print_number64(first,i,n,c)	_print_number64(first,i,n,c,decimal)
#define	print_decimal_number64(first,i,n,c) _print_number64(first,i,n,c,true)

void	decode_filemode(int value);
void	print_integer_arg(const char *(*decoder)(int), int value);
void	print_integer_arg_valid(const char *(*decoder)(int), int value);
void	print_mask_arg(bool (*decoder)(FILE *, int, int *), int value);
void	print_mask_arg0(bool (*decoder)(FILE *, int, int *), int value);
void	print_mask_arg32(bool (*decoder)(FILE *, uint32_t, uint32_t *),
	    uint32_t value);
void	print_mask_argul(bool (*decoder)(FILE *, u_long, u_long *),
	    u_long value);
bool	print_mask_arg_part(bool (*decoder)(FILE *, int, int *),
	    int value, int *rem);

#ifdef SYSDECODE_HAVE_LINUX
bool ktrstruct_linux(const char *name, const char *data, size_t datalen);
void ktrsyscall_linux(struct ktr_syscall *ktr, register_t **resip,
    int *resnarg, char *resc);
#ifdef __amd64__
void ktrsyscall_linux32(struct ktr_syscall *ktr, register_t **resip,
    int *resnarg, char *resc);
#endif
#endif /* SYSDECODE_HAVE_LINUX */

#endif /* !__KDUMP_H__ */
