/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Netflix, Inc
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

#ifndef	COMND_H
#define	COMND_H

#include <sys/queue.h>
#include <sys/linker_set.h>

/*
 * Regularized parsing of simple arguments built on top of getopt_long.
 */

typedef enum arg_type {
	arg_none = 0,
	arg_uint8,
	arg_uint16,
	arg_uint32,
	arg_uint64,
	arg_size,
	arg_string,
	arg_path,
} arg_type;

// XXX need to change to offsetof for opts and args.
// we then need to allocate ctx and pass that into the cmd
// stuff. this will be a little tricky and we may need to expand
// arg_type stuff.

struct opts {
	const char	*long_arg;
	int		short_arg;
	arg_type	at;
	void 		*ptr;			//  XXXX change to offset of
	const char	*descr;
};

// XXX TDB: subcommand vs actual argument. maybe with subcmd?
// XXX TBD: do we need parsing callback functions?
struct args {
	arg_type	at;
	void 		*ptr;			//  XXXX change to offset of
	const char	*descr;
};

typedef void (cmd_load_cb_t)(void *, void *);
struct cmd;
typedef void (cmd_fn_t)(const struct cmd *nf, int argc, char *argv[]);

struct cmd  {
	SLIST_ENTRY(cmd)	link;
	const char		*name;
	cmd_fn_t		*fn;
	size_t			ctx_size;
	const struct opts	*opts;
	const struct args	*args;
	const char		*descr;
	SLIST_HEAD(,cmd)	subcmd;
	struct cmd		*parent;
};

void cmd_register(struct cmd *, struct cmd *);
#define CMD_COMMAND(c)							\
    static void cmd_register_##c(void) __attribute__((constructor));	\
    static void cmd_register_##c(void) { cmd_register(NULL, &c); }
#define CMD_SUBCOMMAND(c,sc)						\
    static void cmd_register_##c_##sc(void) __attribute__((constructor)); \
    static void cmd_register_##c_##sc(void) { cmd_register(&c, &sc); }

int arg_parse(int argc, char * const *argv, const struct cmd *f);
void arg_help(int argc, char * const *argv, const struct cmd *f);
void cmd_init(void);
void cmd_load_dir(const char *dir, cmd_load_cb_t *cb, void *argp);
int cmd_dispatch(int argc, char *argv[], const struct cmd *);

#endif /* COMND_H */
