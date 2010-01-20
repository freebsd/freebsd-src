/*-
 * Copyright (c) 2005 Joseph Koshy
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

#ifndef	_PMCLOG_H_
#define	_PMCLOG_H_

#include <sys/pmclog.h>

enum pmclog_state {
	PMCLOG_OK,
	PMCLOG_EOF,
	PMCLOG_REQUIRE_DATA,
	PMCLOG_ERROR
};

struct pmclog_ev_dropnotify {
};

struct pmclog_ev_closelog {
};

struct pmclog_ev_initialize {
	uint32_t	pl_version;
	uint32_t	pl_arch;
};

struct pmclog_ev_mappingchange {
	uint32_t	pl_type;
	pid_t		pl_pid;
	uintfptr_t	pl_start;
	uintfptr_t	pl_end;
	char		pl_pathname[PATH_MAX];
};

struct pmclog_ev_pcsample {
	uintfptr_t	pl_pc;
	pid_t		pl_pid;
	pmc_id_t	pl_pmcid;
	uint32_t	pl_usermode;
};

struct pmclog_ev_pmcallocate {
	uint32_t	pl_event;
	const char *	pl_evname;
	uint32_t	pl_flags;
	pmc_id_t	pl_pmcid;
};

struct pmclog_ev_pmcattach {
	pmc_id_t	pl_pmcid;
	pid_t		pl_pid;
	char		pl_pathname[PATH_MAX];
};

struct pmclog_ev_pmcdetach {
	pmc_id_t	pl_pmcid;
	pid_t		pl_pid;
};

struct pmclog_ev_proccsw {
	pid_t		pl_pid;
	pmc_id_t	pl_pmcid;
	pmc_value_t	pl_value;
};

struct pmclog_ev_procexec {
	pid_t		pl_pid;
	pmc_id_t	pl_pmcid;
	uintfptr_t	pl_entryaddr;
	char		pl_pathname[PATH_MAX];
};

struct pmclog_ev_procexit {
	uint32_t	pl_pid;
	pmc_id_t	pl_pmcid;
	pmc_value_t	pl_value;
};

struct pmclog_ev_procfork {
	pid_t		pl_oldpid;
	pid_t		pl_newpid;
};

struct pmclog_ev_sysexit {
	pid_t		pl_pid;
};

struct pmclog_ev_userdata {
	uint32_t	pl_userdata;
};

struct pmclog_ev {
	enum pmclog_state pl_state;	/* state after 'get_event()' */
	off_t		  pl_offset;	/* byte offset in stream */
	size_t		  pl_count;	/* count of records so far */
	struct timespec   pl_ts;	/* log entry timestamp */
	enum pmclog_type  pl_type;	/* type of log entry */
	union { 			/* log entry data */
		struct pmclog_ev_closelog	pl_cl;
		struct pmclog_ev_dropnotify	pl_dn;
		struct pmclog_ev_initialize	pl_i;
		struct pmclog_ev_mappingchange	pl_m;
		struct pmclog_ev_pcsample	pl_s;
		struct pmclog_ev_pmcallocate	pl_a;
		struct pmclog_ev_pmcattach	pl_t;
		struct pmclog_ev_pmcdetach	pl_d;
		struct pmclog_ev_proccsw	pl_c;
		struct pmclog_ev_procexec	pl_x;
		struct pmclog_ev_procexit	pl_e;
		struct pmclog_ev_procfork	pl_f;
		struct pmclog_ev_sysexit	pl_se;
		struct pmclog_ev_userdata	pl_u;
	} pl_u;
};

#define	PMCLOG_FD_NONE				(-1)

void	*pmclog_open(int _fd);
int	pmclog_feed(void *_cookie, char *_data, int _len);
int	pmclog_read(void *_cookie, struct pmclog_ev *_ev);
void	pmclog_close(void *_cookie);

#endif

