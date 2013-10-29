/*-
 * Copyright (c) 2012 Dag-Erling Smørgrav
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $Id: t.h 657 2013-03-06 22:59:05Z des $
 */

#ifndef T_H_INCLUDED
#define T_H_INCLUDED

#include <security/openpam_attr.h>

struct t_test {
	int (*func)(void *);
	const char *desc;
	void *arg;
};

#define T_FUNC(n, d)				\
	static int t_ ## n ## _func(void *);	\
	static const struct t_test t_ ## n =	\
	    { t_ ## n ## _func, d, NULL };	\
	static int t_ ## n ## _func(OPENPAM_UNUSED(void *arg))

#define T_FUNC_ARG(n, d, a)			\
	static int t_ ## n ## _func(void *);	\
	static const struct t_test t_ ## n =	\
	    { t_ ## n ## _func, d, a };		\
	static int t_ ## n ## _func(void *arg)

#define T(n)					\
	&t_ ## n

extern const char *t_progname;

const struct t_test **t_prepare(int, char **);
void t_cleanup(void);

void t_verbose(const char *, ...)
	OPENPAM_FORMAT((__printf__, 1, 2));

/*
 * Convenience functions for temp files
 */
struct t_file {
	char *name;
	FILE *file;
	struct t_file *prev, *next;
};

struct t_file *t_fopen(const char *);
int t_fprintf(struct t_file *, const char *, ...);
int t_ferror(struct t_file *);
int t_feof(struct t_file *);
void t_frewind(struct t_file *);
void t_fclose(struct t_file *);
void t_fcloseall(void);

#endif
