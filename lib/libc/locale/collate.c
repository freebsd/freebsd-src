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
 * $Id: collate.c,v 1.9 1996/11/26 02:49:31 ache Exp $
 */

#include <rune.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sysexits.h>
#include "collate.h"

char *_PathLocale;
int __collate_load_error = 1;
char __collate_version[STR_LEN];
u_char __collate_substitute_table[UCHAR_MAX + 1][STR_LEN];
struct __collate_st_char_pri __collate_char_pri_table[UCHAR_MAX + 1];
struct __collate_st_chain_pri __collate_chain_pri_table[TABLE_SIZE];

#define FREAD(a, b, c, d) \
	do { \
		if(fread(a, b, c, d) != c) { \
			fclose(d); \
			return -1; \
		} \
	} while(0)

void __collate_err(int ex, const char *f) __dead2;

int
__collate_load_tables(encoding)
	char *encoding;
{
	char buf[PATH_MAX];
	FILE *fp;
	int save_load_error;

	save_load_error = __collate_load_error;
	__collate_load_error = 1;
	if (!encoding) {
		__collate_load_error = save_load_error;
		return -1;
	}
	if (!strcmp(encoding, "C") || !strcmp(encoding, "POSIX"))
		return 0;
	if (!_PathLocale) {
		__collate_load_error = save_load_error;
		return -1;
	}
	(void) snprintf(buf, sizeof buf, "%s/%s/LC_COLLATE",
			_PathLocale, encoding);
	if ((fp = fopen(buf, "r")) == NULL) {
		__collate_load_error = save_load_error;
		return -1;
	}
	FREAD(__collate_version, sizeof(__collate_version), 1, fp);
	if (strcmp(__collate_version, COLLATE_VERSION) != 0) {
		fclose(fp);
		return -1;
	}
	FREAD(__collate_substitute_table, sizeof(__collate_substitute_table),
	      1, fp);
	FREAD(__collate_char_pri_table, sizeof(__collate_char_pri_table), 1,
	      fp);
	FREAD(__collate_chain_pri_table, sizeof(__collate_chain_pri_table), 1,
	      fp);
	fclose(fp);
	__collate_load_error = 0;
	return 0;
}

u_char *
__collate_substitute(s)
	const u_char *s;
{
	int dest_len = 0, len = 0;
	int delta = strlen(s);
	u_char *dest_str = NULL;

	if(s == NULL || *s == '\0')
		return __collate_strdup("");
	while(*s) {
		len += strlen(__collate_substitute_table[*s]);
		while(dest_len <= len) {
			if(!dest_str)
				dest_str = calloc(dest_len = delta, 1);
			else
				dest_str = realloc(dest_str, dest_len += delta);
			if(dest_str == NULL)
				__collate_err(EX_OSERR, __FUNCTION__);
		}
		strcat(dest_str, __collate_substitute_table[*s++]);
	}
	return dest_str;
}

void
__collate_lookup(t, len, prim, sec)
	u_char *t;
	int *len, *prim, *sec;
{
	struct __collate_st_chain_pri *p2;

	*len = 1;
	*prim = *sec = 0;
	for(p2 = __collate_chain_pri_table; p2->str[0]; p2++) {
		if(strncmp(t, p2->str, strlen(p2->str)) == 0) {
			*len = strlen(p2->str);
			*prim = p2->prim;
			*sec = p2->sec;
			return;
		}
	}
	*prim = __collate_char_pri_table[*t].prim;
	*sec = __collate_char_pri_table[*t].sec;
}

u_char *
__collate_strdup(s)
	u_char *s;
{
	u_char *t = strdup(s);

	if (t == NULL)
		__collate_err(EX_OSERR, __FUNCTION__);
	return t;
}

void
__collate_err(int ex, const char *f)
{
	extern char *__progname;                /* Program name, from crt0. */
	const char *s;
	int serrno = errno;

	s = __progname;
	write(STDERR_FILENO, s, strlen(s));
	write(STDERR_FILENO, ": ", 2);
	s = f;
	write(STDERR_FILENO, s, strlen(s));
	write(STDERR_FILENO, ": ", 2);
	s = strerror(serrno);
	write(STDERR_FILENO, s, strlen(s));
	write(STDERR_FILENO, "\n", 1);
	exit(ex);
}

#ifdef COLLATE_DEBUG
void
__collate_print_tables()
{
	int i;
	struct __collate_st_chain_pri *p2;

	printf("Substitute table:\n");
	for (i = 0; i < UCHAR_MAX + 1; i++)
	    if (i != *__collate_substitute_table[i])
		printf("\t'%c' --> \"%s\"\n", i,
		       __collate_substitute_table[i]);
	printf("Chain priority table:\n");
	for (p2 = __collate_chain_pri_table; p2->str[0]; p2++)
		printf("\t\"%s\" : %d %d\n\n", p2->str, p2->prim, p2->sec);
	printf("Char priority table:\n");
	for (i = 0; i < UCHAR_MAX + 1; i++)
		printf("\t'%c' : %d %d\n", i, __collate_char_pri_table[i].prim,
		       __collate_char_pri_table[i].sec);
}
#endif
