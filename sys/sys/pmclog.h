/*-
 * Copyright (c) 2005-2007, Joseph Koshy
 * Copyright (c) 2007 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
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
	/* V1 ABI */
	PMCLOG_TYPE_CLOSELOG,
	PMCLOG_TYPE_DROPNOTIFY,
	PMCLOG_TYPE_INITIALIZE,
	PMCLOG_TYPE_MAPPINGCHANGE, /* unused in v1 */
	PMCLOG_TYPE_PCSAMPLE,
	PMCLOG_TYPE_PMCALLOCATE,
	PMCLOG_TYPE_PMCATTACH,
	PMCLOG_TYPE_PMCDETACH,
	PMCLOG_TYPE_PROCCSW,
	PMCLOG_TYPE_PROCEXEC,
	PMCLOG_TYPE_PROCEXIT,
	PMCLOG_TYPE_PROCFORK,
	PMCLOG_TYPE_SYSEXIT,
	PMCLOG_TYPE_USERDATA,
	/*
	 * V2 ABI
	 *
	 * The MAP_{IN,OUT} event types obsolete the MAPPING_CHANGE
	 * event type.  The CALLCHAIN event type obsoletes the
	 * PCSAMPLE event type.
	 */
	PMCLOG_TYPE_MAP_IN,
	PMCLOG_TYPE_MAP_OUT,
	PMCLOG_TYPE_CALLCHAIN,
	/*
	 * V3 ABI
	 *
	 * New variant of PMCLOG_TYPE_PMCALLOCATE for dynamic event.
	 */
	PMCLOG_TYPE_PMCALLOCATEDYN
};

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

struct pmclog_callchain {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pid;
	uint32_t		pl_pmcid;
	uint32_t		pl_cpuflags;
	/* 8 byte aligned */
	uintptr_t		pl_pc[PMC_CALLCHAIN_DEPTH_MAX];
} __packed;

#define	PMC_CALLCHAIN_CPUFLAGS_TO_CPU(CF)	(((CF) >> 16) & 0xFFFF)
#define	PMC_CALLCHAIN_CPUFLAGS_TO_USERMODE(CF)	((CF) & PMC_CC_F_USERSPACE)
#define	PMC_CALLCHAIN_TO_CPUFLAGS(CPU,FLAGS)	\
	(((CPU) << 16) | ((FLAGS) & 0xFFFF))

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

struct pmclog_map_in {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pid;
	uintfptr_t		pl_start;	/* 8 byte aligned */
	char			pl_pathname[PATH_MAX];
} __packed;

struct pmclog_map_out {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pid;
	uintfptr_t		pl_start;	/* 8 byte aligned */
	uintfptr_t		pl_end;
} __packed;

struct pmclog_pcsample {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pid;
	uintfptr_t		pl_pc;		/* 8 byte aligned */
	uint32_t		pl_pmcid;
	uint32_t		pl_usermode;
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
	uintfptr_t		pl_start;	/* keep 8 byte aligned */
	uint32_t		pl_pmcid;
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

struct pmclog_pmcallocatedyn {
	PMCLOG_ENTRY_HEADER
	uint32_t		pl_pmcid;
	uint32_t		pl_event;
	uint32_t		pl_flags;
	char			pl_evname[PMC_NAME_MAX];
} __packed;

union pmclog_entry {		/* only used to size scratch areas */
	struct pmclog_callchain		pl_cc;
	struct pmclog_closelog		pl_cl;
	struct pmclog_dropnotify	pl_dn;
	struct pmclog_initialize	pl_i;
	struct pmclog_map_in		pl_mi;
	struct pmclog_map_out		pl_mo;
	struct pmclog_pcsample		pl_s;
	struct pmclog_pmcallocate	pl_a;
	struct pmclog_pmcallocatedyn	pl_ad;
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
int	pmclog_configure_log(struct pmc_mdep *_md, struct pmc_owner *_po,
    int _logfd);
int	pmclog_deconfigure_log(struct pmc_owner *_po);
int	pmclog_flush(struct pmc_owner *_po);
int	pmclog_close(struct pmc_owner *_po);
void	pmclog_initialize(void);
void	pmclog_process_callchain(struct pmc *_pm, struct pmc_sample *_ps);
void	pmclog_process_closelog(struct pmc_owner *po);
void	pmclog_process_dropnotify(struct pmc_owner *po);
void	pmclog_process_map_in(struct pmc_owner *po, pid_t pid,
    uintfptr_t start, const char *path);
void	pmclog_process_map_out(struct pmc_owner *po, pid_t pid,
    uintfptr_t start, uintfptr_t end);
void	pmclog_process_pmcallocate(struct pmc *_pm);
void	pmclog_process_pmcattach(struct pmc *_pm, pid_t _pid, char *_path);
void	pmclog_process_pmcdetach(struct pmc *_pm, pid_t _pid);
void	pmclog_process_proccsw(struct pmc *_pm, struct pmc_process *_pp,
    pmc_value_t _v);
void	pmclog_process_procexec(struct pmc_owner *_po, pmc_id_t _pmid, pid_t _pid,
    uintfptr_t _startaddr, char *_path);
void	pmclog_process_procexit(struct pmc *_pm, struct pmc_process *_pp);
void	pmclog_process_procfork(struct pmc_owner *_po, pid_t _oldpid, pid_t _newpid);
void	pmclog_process_sysexit(struct pmc_owner *_po, pid_t _pid);
int	pmclog_process_userlog(struct pmc_owner *_po,
    struct pmc_op_writelog *_wl);
void	pmclog_shutdown(void);
#endif	/* _KERNEL */

#endif	/* _SYS_PMCLOG_H_ */
