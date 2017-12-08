/*-
 * Copyright (c) 2011 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * Copyright (c) 2014 Pedro Souza <pedrosouza@freebsd.org>
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

typedef void	interp_init_t(void *ctx);
typedef int	interp_run_t(void *ctx, const char *input);
typedef int	interp_incl_t(void *ctx, const char *filename);
typedef int	interp_load_def_t(void *ctx);		// load default configuration files

struct interp {
	interp_init_t	*init;
	interp_run_t	*run;
	interp_incl_t	*incl;
	interp_load_def_t *load_configs;
	void		*context;
};

#define	INTERP_INIT(i)		do {			\
	if (((i) != NULL) && ((i)->init != NULL)) {	\
		((i)->init((i)->context));		\
	}						\
} while (0)

#define	INTERP_RUN(i, input)	\
	((i)->run(((i)->context), input))

#define	INTERP_INCL(i, filename)	\
	((i)->incl(((i)->context), filename))

#define	INTERP_LOAD_DEF_CONFIG(i)	\
	((i)->load_configs(((i)->context)))



extern struct interp	boot_interp_simple;
extern struct interp	boot_interp_forth;
extern struct interp    boot_interp_lua;


extern struct interp	*interp;

int perform(int argc, char *argv[]);
void prompt(void);

/*
 * Default config loader for interp_simple & intep_forth
 * Use it if your interpreter does not use a custom config
 * file.
 *
 * Calls interp->include with 'loader.rc' or 'boot.conf'
 */
int default_load_config(void *ctx);

struct includeline
{
    struct includeline	*next;
    int			flags;
    int			line;
#define SL_QUIET	(1<<0)
#define SL_IGNOREERR	(1<<1)
    char		text[0];
};
