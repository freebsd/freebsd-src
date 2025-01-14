/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1988, 1993, 1994
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
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Prodrive Technologies, https://prodrive-technologies.com/
 * Author: Ed Schouten <ed@FreeBSD.org>
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Jake Freeland <jfree@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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

#ifndef _SYSLOGD_H_
#define _SYSLOGD_H_

#include <sys/param.h>
#include <sys/nv.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/uio.h>

#define SYSLOG_NAMES
#include <sys/syslog.h>

#include <regex.h>
#include <stdbool.h>
#include <stdio.h>

#include "ttymsg.h"

#define	MAXLINE		8192		/* maximum line length */
#define	MAXSVLINE	MAXLINE		/* maximum saved line length */
#define	MAXUNAMES	20		/* maximum number of user names */

/* Timestamps of log entries. */
struct logtime {
	struct tm	tm;
	suseconds_t	usec;
};

enum filt_proptype {
	FILT_PROP_NOOP,
	FILT_PROP_MSG,
	FILT_PROP_HOSTNAME,
	FILT_PROP_PROGNAME,
};

enum filt_cmptype {
	FILT_CMP_CONTAINS,
	FILT_CMP_EQUAL,
	FILT_CMP_STARTS,
	FILT_CMP_REGEX,
};

/*
 * This structure holds a property-based filter
 */
struct prop_filter {
	enum filt_proptype prop_type;
	enum filt_cmptype cmp_type;
	uint8_t cmp_flags;
#define	FILT_FLAG_EXCLUDE	(1 << 0)
#define	FILT_FLAG_EXTENDED	(1 << 1)
#define	FILT_FLAG_ICASE		(1 << 2)
	char *pflt_strval;
	regex_t *pflt_re;
};

enum f_type {
	F_UNUSED,	/* unused entry */
	F_FILE,		/* regular file */
	F_TTY,		/* terminal */
	F_CONSOLE,	/* console terminal */
	F_FORW,		/* remote machine */
	F_USERS,	/* list of users */
	F_WALL,		/* everyone logged on */
	F_PIPE,		/* pipe to program */
};

struct forw_addr {
	struct sockaddr_storage laddr;
	struct sockaddr_storage raddr;
};

/*
 * This structure represents the files that will have log
 * copies printed.
 * We require f_file to be valid if f_type is F_FILE, F_CONSOLE, F_TTY
 * or if f_type is F_PIPE and f_pid > 0.
 */
struct filed {
	enum f_type f_type;

	/* Used for filtering. */
	char	f_host[MAXHOSTNAMELEN];		/* host from which to recd. */
	char	f_program[MAXPATHLEN];		/* program this applies to */
	struct prop_filter *f_prop_filter;	/* property-based filter */
	u_char	f_pmask[LOG_NFACILITIES+1];	/* priority mask */
	u_char	f_pcmp[LOG_NFACILITIES+1];	/* compare priority */
#define PRI_LT	0x1
#define PRI_EQ	0x2
#define PRI_GT	0x4

	/* Logging destinations. */
	int	f_file;				/* file descriptor */
	int	f_flags;			/* file-specific flags */
#define	FFLAG_SYNC	0x01
#define	FFLAG_NEEDSYNC	0x02
	union {
		char	f_uname[MAXUNAMES][MAXLOGNAME];	/* F_WALL, F_USERS */
		char	f_fname[MAXPATHLEN];	/* F_FILE, F_CONSOLE, F_TTY */
		struct {
			char	f_hname[MAXHOSTNAMELEN];
			int	*f_addr_fds;
			size_t	f_num_addr_fds;
			struct forw_addr *f_addrs;
		};				/* F_FORW */
		struct {
			char	f_pname[MAXPATHLEN];
			int	f_procdesc;
			struct deadq_entry *f_dq;
		};				/* F_PIPE */
	};

	/* Book-keeping. */
	char	f_prevline[MAXSVLINE];		/* last message logged */
	time_t	f_time;				/* time this was last written */
	struct logtime f_lasttime;		/* time of last occurrence */
	int	f_prevpri;			/* pri of f_prevline */
	size_t	f_prevlen;			/* length of f_prevline */
	int	f_prevcount;			/* repetition cnt of prevline */
	u_int	f_repeatcount;			/* number of "repeated" msgs */
	STAILQ_ENTRY(filed) next;		/* next in linked list */
};

/*
 * List of iovecs to which entries can be appended.
 * Used for constructing the message to be logged.
 */
struct iovlist {
	struct iovec	iov[TTYMSG_IOV_MAX];
	size_t		iovcnt;
	size_t		totalsize;
};

extern const char *ConfFile;
extern char LocalHostName[MAXHOSTNAMELEN];

void closelogfiles(void);
void logerror(const char *);
int p_open(const char *, pid_t *);
nvlist_t *readconfigfile(const char *);
void wallmsg(const struct filed *, struct iovec *, const int);

#endif /* !_SYSLOGD_H_ */
