/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * $FreeBSD$
 */
#ifndef _ATMCONFIG_H
#define	_ATMCONFIG_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <netgraph/ng_message.h>

#define	DEFAULT_INTERFACE	"hatm0"

struct cmdtab {
	const char	*string;
	const struct cmdtab *sub;
	void		(*func)(int, char *[]);
};

/*
 * client configuration info
 */
struct amodule {
	struct cmdtab	cmd;
	const char	*help;
};

#define	DEF_MODULE(CMDTAB, HELP)				\
struct amodule amodule_1 = { CMDTAB, HELP };

/* print a message if we are verbose */
void	verb(const char *, ...) __printflike(1, 2);

/* print heading */
void	heading(const char *, ...) __printflike(1, 2);

/* before starting output */
void	heading_init(void);

/* stringify an enumerated value */
struct penum {
	int32_t	value;
	const char *str;
};
const char *penum(int32_t value, const struct penum *strtab, char *buf);

enum {
	OPT_NONE,
	OPT_UINT,
	OPT_INT,
	OPT_UINT32,
	OPT_INT32,
	OPT_UINT64,
	OPT_INT64,
	OPT_FLAG,
	OPT_VCI,
	OPT_STRING,
	OPT_SIMPLE,
};
struct option {
	const char *optstr;
	int	opttype;
	void	*optarg;
};

int parse_options(int *_pargc, char ***_pargv,
    const struct option *_opts);

#endif /* _ATMCONFIG_H */
