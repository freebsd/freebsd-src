/*-
 * Copyright (c) 1995 Alex Tatmanjants <alex@elvisti.kiev.ua>
 *		at Electronni Visti IA, Kiev, Ukraine.
 *			All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
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

#ifndef _COLLATE_H_
#define	_COLLATE_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <limits.h>

#define STR_LEN 10
#define TABLE_SIZE 100
#define COLLATE_VERSION "1.0\n"

struct __collate_st_char_pri {
	int prim, sec;
};
struct __collate_st_chain_pri {
	u_char str[STR_LEN];
	int prim, sec;
};

extern int __collate_load_error;
extern int __collate_substitute_nontrivial;
extern char __collate_version[STR_LEN];
extern u_char __collate_substitute_table[UCHAR_MAX + 1][STR_LEN];
extern struct __collate_st_char_pri __collate_char_pri_table[UCHAR_MAX + 1];
extern struct __collate_st_chain_pri __collate_chain_pri_table[TABLE_SIZE];

__BEGIN_DECLS
u_char	*__collate_strdup __P((u_char *));
u_char	*__collate_substitute __P((const u_char *));
int	__collate_load_tables __P((char *));
void	__collate_lookup __P((const u_char *, int *, int *, int *));
int	__collate_range_cmp __P((int, int));
#ifdef COLLATE_DEBUG
void	__collate_print_tables __P((void));
#endif
__END_DECLS

#endif /* !_COLLATE_H_ */
