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

#ifndef	_SYS_PMCLOG_H_
#define	_SYS_PMCLOG_H_

#include <sys/pmc.h>

enum pmclog_type {
	PMCLOG_TYPE_CLOSELOG,
	PMCLOG_TYPE_DROPNOTIFY,
	PMCLOG_TYPE_INITIALIZE,
	PMCLOG_TYPE_MAPPINGCHANGE,
	PMCLOG_TYPE_PCSAMPLE,
	PMCLOG_TYPE_PMCALLOCATE,
	PMCLOG_TYPE_PMCATTACH,
	PMCLOG_TYPE_PMCDETACH,
	PMCLOG_TYPE_PROCCSW,
	PMCLOG_TYPE_PROCEXEC,
	PMCLOG_TYPE_PROCEXIT,
	PMCLOG_TYPE_PROCFORK,
	PMCLOG_TYPE_SYSEXIT,
	PMCLOG_TYPE_USERDATA
};

#define	PMCLOG_MAPPING_INSERT			0x01
#define	PMCLOG_MAPPING_DELETE			0x02

/*
 * A log entry descriptor comprises of a 32 bit header and a 64 bit
 * time stamp followed by as many 32 bit words are required to record
 * the event.
 *
 * Header field format:
 *
 *  31           24           16                                   0
 *   +------------+------------+-----------------------------------+
 *   |    MAGIC   |    TYPE    |               LENGTH              |
 *   +------------+------------+-----------------------------------+
 *
 * MAGIC 	is the constant PMCLOG_HEADER_MAGIC.
 * TYPE  	contains a value of type enum pmclog_type.
 * LENGTH	contains the length of the event record, in bytes.
 */

#define	PMCLOG_ENTRY_HEADER				\
	uint32_t		pl_header;		\
	uint32_t		pl_ts_sec;		\
	uint32_t		pl_ts_nsec;


/*
 * The following structures are used to describe the size of each kind
 * of log entry to sizeof().  To keep the compiler from adding
 * padding, the fields of each structure are aligned to their natural
 * boundaries, and the structures are marked as 'packed'.
 *
 * The actual reading and writing of the log file is always in terms
 * of 4 byte quantities.
 */

struct pmclog_closelog {
	PMCLOG_ENTRY_HEADER
};

struct pmclog_dropnotify {
	PMCLOG_ENTRY_HEADER
};

struct pmclog_initialize {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_version;	/* driver version */
	uint32_t		pl_cpu;		/* enum pmc_cputype */
} __packed;

struct pmclog_mappingchange {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_type;
	uintfptr_t		pl_start;	/* 8 byte aligned */
	uintfptr_t		pl_end;
	uint32_t		pl_pid;
	char			pl_pathname[PATH_MAX];
} __packed;


struct pmclog_pcsample {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pid;
	uintfptr_t		pl_pc;		/* 8 byte aligned */
	uint32_t		pl_pmcid;
} __packed;

struct pmclog_pmcallocate {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pmcid;
	uint32_t		pl_event;
	uint32_t		pl_flags;
} __packed;

struct pmclog_pmcattach {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pmcid;
	uint32_t		pl_pid;
	char			pl_pathname[PATH_MAX];
} __packed;

struct pmclog_pmcdetach {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pmcid;
	uint32_t		pl_pid;
} __packed;

struct pmclog_proccsw {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pmcid;
	uint64_t		pl_value;	/* keep 8 byte aligned */
	uint32_t		pl_pid;
} __packed;

struct pmclog_procexec {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pid;
	char			pl_pathname[PATH_MAX];
} __packed;

struct pmclog_procexit {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pmcid;
	uint64_t		pl_value;	/* keep 8 byte aligned */
	uint32_t		pl_pid;
} __packed;

struct pmclog_procfork {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_oldpid;
	uint32_t		pl_newpid;
} __packed;

struct pmclog_sysexit {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pid;
} __packed;

struct pmclog_userdata {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_userdata;
} __packed;

union pmclog_entry {		/* only used to size scratch areas */
	struct pmclog_closelog		pl_cl;
	struct pmclog_dropnotify	pl_dn;
	struct pmclog_initialize	pl_i;
	struct pmclog_pcsample		pl_s;
	struct pmclog_pmcallocate	pl_a;
	struct pmclog_pmcattach		pl_t;
	struct pmclog_pmcdetach		pl_d;
	struct pmclog_proccsw		pl_c;
	struct pmclog_procexec		pl_x;
	struct pmclog_procexit		pl_e;
	struct pmclog_procfork		pl_f;
	struct pmclog_sysexit		pl_se;
	struct pmclog_userdata		pl_u;
};

#define	PMCLOG_HEADER_MAGIC					0xEEU

#define	PMCLOG_HEADER_TO_LENGTH(H)				\
	((H) & 0x0000FFFF)
#define	PMCLOG_HEADER_TO_TYPE(H)				\
	(((H) & 0x00FF0000) >> 16)
#define	PMCLOG_HEADER_TO_MAGIC(H)				\
	(((H) & 0xFF000000) >> 24)
#define	PMCLOG_HEADER_CHECK_MAGIC(H)				\
	(PMCLOG_HEADER_TO_MAGIC(H) == PMCLOG_HEADER_MAGIC)

#ifdef	_KERNEL

/*
 * Prototypes
 */
int	pmclog_configure_log(struct pmc_owner *_po, int _logfd);
int	pmclog_deconfigure_log(struct pmc_owner *_po);
int	pmclog_flush(struct pmc_owner *_po);
void	pmclog_initialize(void);
void	pmclog_process_closelog(struct pmc_owner *po);
void	pmclog_process_dropnotify(struct pmc_owner *po);
void	pmclog_process_mappingchange(struct pmc_owner *po, pid_t pid, int type,
    uintfptr_t start, uintfptr_t end, char *path);
void	pmclog_process_pcsample(struct pmc *_pm, struct pmc_sample *_ps);
void	pmclog_process_pmcallocate(struct pmc *_pm);
void	pmclog_process_pmcattach(struct pmc *_pm, pid_t _pid, char *_path);
void	pmclog_process_pmcdetach(struct pmc *_pm, pid_t _pid);
void	pmclog_process_proccsw(struct pmc *_pm, struct pmc_process *_pp,
    pmc_value_t _v);
void	pmclog_process_procexec(struct pmc_owner *_po, pid_t _pid, char *_path);
void	pmclog_process_procexit(struct pmc *_pm, struct pmc_process *_pp);
void	pmclog_process_procfork(struct pmc_owner *_po, pid_t _oldpid, pid_t _newpid);
void	pmclog_process_sysexit(struct pmc_owner *_po, pid_t _pid);
int	pmclog_process_userlog(struct pmc_owner *_po,
    struct pmc_op_writelog *_wl);
void	pmclog_shutdown(void);
#endif	/* _KERNEL */

#endif	/* _SYS_PMCLOG_H_ */
