/*
 * Copyright (c) 2006 "David Kirchner" <dpk@dpk.net>. All rights reserved.
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

#ifndef __SYSDECODE_SUPPORT_H__
#define __SYSDECODE_SUPPORT_H__

/*
 * This is taken from the xlat tables originally in truss which were
 * in turn taken from strace.
 */
struct name_table {
	uintmax_t val;
	const char *str;
};

/*
 * These are simple support macros. print_or utilizes a variable
 * defined in the calling function to track whether or not it should
 * print a logical-OR character ('|') before a string. if_print_or
 * simply handles the necessary "if" statement used in many lines
 * of this file.
 */
#define print_or(fp,str,orflag) do {                     \
	if (orflag) fputc(fp, '|'); else orflag = true;  \
	fprintf(fp, str); }                              \
	while (0)
#define if_print_or(fp,i,flag,orflag) do {         \
	if ((i & flag) == flag)                    \
	print_or(fp,#flag,orflag); }               \
	while (0)

const char *lookup_value(struct name_table *, uintmax_t);
void	print_integer(FILE *, int, int);
bool	print_mask_0(FILE *, struct name_table *, int, int *);
bool	print_mask_0ul(FILE *, struct name_table *, u_long, u_long *);
bool	print_mask_int(FILE *, struct name_table *, int, int *);
void	print_mask_part(FILE *, struct name_table *, uintmax_t *, bool *);
bool	print_value(FILE *, struct name_table *, uintmax_t);

#endif /* !__SYSDECODE_SUPPORT_H__ */
