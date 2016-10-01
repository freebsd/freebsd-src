/*-
 * Copyright (c) 2008 John Birrell (jb@freebsd.org)
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

#include <sys/types.h>
#include <sys/ptrace.h>

#include <rtld_db.h>

#include "libproc.h"

struct procstat;

struct proc_handle {
	pid_t	pid;			/* Process ID. */
	int	flags;			/* Process flags. */
	int	status;			/* Process status (PS_*). */
	int	wstat;			/* Process wait status. */
	int	model;			/* Process data model. */
	rd_agent_t *rdap;		/* librtld_db agent */
	rd_loadobj_t *rdobjs;		/* Array of loaded objects. */
	size_t	rdobjsz;		/* Array size. */
	size_t	nobjs;			/* Num. objects currently loaded. */
	rd_loadobj_t *rdexec;		/* rdobj for program executable. */
	struct lwpstatus lwps;		/* Process status. */
	struct procstat *procstat;	/* libprocstat handle. */
	char	execpath[MAXPATHLEN];	/* Path to program executable. */
};

#ifdef DEBUG
#define	DPRINTF(...) 	warn(__VA_ARGS__)
#define	DPRINTFX(...)	warnx(__VA_ARGS__)
#else
#define	DPRINTF(...)    do { } while (0)
#define	DPRINTFX(...)   do { } while (0)
#endif
