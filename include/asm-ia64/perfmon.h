/*
 * Copyright (C) 2001-2003 Hewlett-Packard Co
 *               Stephane Eranian <eranian@hpl.hp.com>
 */

#ifndef _ASM_IA64_PERFMON_H
#define _ASM_IA64_PERFMON_H

/*
 * perfmon comamnds supported on all CPU models
 */
#define PFM_WRITE_PMCS		0x01
#define PFM_WRITE_PMDS		0x02
#define PFM_READ_PMDS		0x03
#define PFM_STOP		0x04
#define PFM_START		0x05
#define PFM_ENABLE		0x06
#define PFM_DISABLE		0x07
#define PFM_CREATE_CONTEXT	0x08
#define PFM_DESTROY_CONTEXT	0x09
#define PFM_RESTART		0x0a
#define PFM_PROTECT_CONTEXT	0x0b
#define PFM_GET_FEATURES	0x0c
#define PFM_DEBUG		0x0d
#define PFM_UNPROTECT_CONTEXT	0x0e
#define PFM_GET_PMC_RESET_VAL	0x0f


/*
 * CPU model specific commands (may not be supported on all models)
 */
#define PFM_WRITE_IBRS		0x20
#define PFM_WRITE_DBRS		0x21

/*
 * context flags
 */
#define PFM_FL_INHERIT_NONE	 0x00	/* never inherit a context across fork (default) */
#define PFM_FL_INHERIT_ONCE	 0x01	/* clone pfm_context only once across fork() */
#define PFM_FL_INHERIT_ALL	 0x02	/* always clone pfm_context across fork() */
#define PFM_FL_NOTIFY_BLOCK    	 0x04	/* block task on user level notifications */
#define PFM_FL_SYSTEM_WIDE	 0x08	/* create a system wide context */
#define PFM_FL_EXCL_IDLE         0x20   /* exclude idle task from system wide session */
#define PFM_FL_UNSECURE		 0x40   /* allow unsecure monitoring for non self-monitoring task */

/*
 * PMC flags
 */
#define PFM_REGFL_OVFL_NOTIFY	0x1	/* send notification on overflow */
#define PFM_REGFL_RANDOM	0x2	/* randomize sampling interval */

/*
 * PMD/PMC/IBR/DBR return flags (ignored on input)
 *
 * Those flags are used on output and must be checked in case EAGAIN is returned
 * by any of the calls using a pfarg_reg_t or pfarg_dbreg_t structure.
 */
#define PFM_REG_RETFL_NOTAVAIL	(1U<<31) /* set if register is implemented but not available */
#define PFM_REG_RETFL_EINVAL	(1U<<30) /* set if register entry is invalid */
#define PFM_REG_RETFL_MASK	(PFM_REG_RETFL_NOTAVAIL|PFM_REG_RETFL_EINVAL)

#define PFM_REG_HAS_ERROR(flag)	(((flag) & PFM_REG_RETFL_MASK) != 0)

/*
 * Request structure used to define a context
 */
typedef struct {
	unsigned long ctx_smpl_entries;	/* how many entries in sampling buffer */
	unsigned long ctx_smpl_regs[4];	/* which pmds to record on overflow */

	pid_t	      ctx_notify_pid;	/* which process to notify on overflow */
	int	      ctx_flags;	/* noblock/block, inherit flags */
	void	      *ctx_smpl_vaddr;	/* returns address of BTB buffer */

	unsigned long ctx_cpu_mask;	/* on which CPU to enable perfmon (systemwide) */

	unsigned long reserved[8];	/* for future use */
} pfarg_context_t;

/*
 * Request structure used to write/read a PMC or PMD
 */
typedef struct {
	unsigned int	reg_num;	/* which register */
	unsigned int	reg_flags;	/* PMC: notify/don't notify. PMD/PMC: return flags */
	unsigned long	reg_value;	/* configuration (PMC) or initial value (PMD) */

	unsigned long	reg_long_reset;	/* reset after sampling buffer overflow (large) */
	unsigned long	reg_short_reset;/* reset after counter overflow (small) */

	unsigned long	reg_reset_pmds[4];   /* which other counters to reset on overflow */
	unsigned long	reg_random_seed;     /* seed value when randomization is used */
	unsigned long	reg_random_mask;     /* bitmask used to limit random value */
	unsigned long	reg_last_reset_value;/* last value used to reset the PMD (PFM_READ_PMDS) */

	unsigned long   reserved[13];	/* for future use */
} pfarg_reg_t;

typedef struct {
	unsigned int	dbreg_num;	/* which register */
	unsigned int	dbreg_flags;	/* dbregs return flags */
	unsigned long	dbreg_value;	/* configuration (PMC) or initial value (PMD) */
	unsigned long	reserved[6];
} pfarg_dbreg_t;

typedef struct {			
	unsigned int	ft_version;	/* perfmon: major [16-31], minor [0-15] */
	unsigned int	ft_smpl_version;/* sampling format: major [16-31], minor [0-15] */
	unsigned long	reserved[4];	/* for future use */
} pfarg_features_t;

/*
 * Entry header in the sampling buffer.
 * The header is directly followed with the PMDS saved in increasing index 
 * order: PMD4, PMD5, .... How many PMDs are present is determined by the 
 * user program during context creation.
 *
 * XXX: in this version of the entry, only up to 64 registers can be recorded
 * This should be enough for quite some time. Always check sampling format
 * before parsing entries!
 *
 * In the case where multiple counters overflow at the same time, the
 * last_reset_value member indicates the initial value of the PMD with
 * the smallest index.  For instance, if PMD2 and PMD5 have overflowed,
 * the last_reset_value member contains the initial value of PMD2.
 */
typedef struct {
	int		pid;		 /* identification of process */
	int		cpu;		 /* which cpu was used */
	unsigned long	last_reset_value;/* initial value of overflowed counter */
	unsigned long	stamp;		 /* timestamp (unique per CPU) */
	unsigned long	ip;		 /* where did the overflow interrupt happened */
	unsigned long	regs;		 /* bitmask of which registers overflowed */
	unsigned long   period;		 /* unused */
} perfmon_smpl_entry_t;

/*
 * This header is at the beginning of the sampling buffer returned to the user.
 * It is exported as Read-Only at this point. It is directly followed by the
 * first record.
 */
typedef struct {
	unsigned int	hdr_version;		/* contains perfmon version (smpl format diffs) */
	unsigned int	reserved;
	unsigned long	hdr_entry_size;		/* size of one entry in bytes */
	unsigned long	hdr_count;		/* how many valid entries */
	unsigned long	hdr_pmds[4];		/* which pmds are recorded */
} perfmon_smpl_hdr_t;

/*
 * Define the version numbers for both perfmon as a whole and the sampling buffer format.
 */
#define PFM_VERSION_MAJ		1U
#define PFM_VERSION_MIN		5U
#define PFM_VERSION		(((PFM_VERSION_MAJ&0xffff)<<16)|(PFM_VERSION_MIN & 0xffff))

#define PFM_SMPL_VERSION_MAJ	1U
#define PFM_SMPL_VERSION_MIN	0U
#define PFM_SMPL_VERSION	(((PFM_SMPL_VERSION_MAJ&0xffff)<<16)|(PFM_SMPL_VERSION_MIN & 0xffff))


#define PFM_VERSION_MAJOR(x)	(((x)>>16) & 0xffff)
#define PFM_VERSION_MINOR(x)	((x) & 0xffff)


#ifdef __KERNEL__

extern long perfmonctl(pid_t pid, int cmd, void *arg, int narg);

typedef struct {
	void (*handler)(int irq, void *arg, struct pt_regs *regs);
} pfm_intr_handler_desc_t;

extern void pfm_save_regs (struct task_struct *);
extern void pfm_load_regs (struct task_struct *);

extern int  pfm_inherit (struct task_struct *, struct pt_regs *);
extern void pfm_context_exit (struct task_struct *);
extern void pfm_flush_regs (struct task_struct *);
extern void pfm_cleanup_notifiers (struct task_struct *);
extern void pfm_cleanup_owners (struct task_struct *);
extern int  pfm_use_debug_registers(struct task_struct *);
extern int  pfm_release_debug_registers(struct task_struct *);
extern int  pfm_cleanup_smpl_buf(struct task_struct *);
extern void pfm_syst_wide_update_task(struct task_struct *, unsigned long info, int is_ctxswin);
extern void pfm_init_percpu(void);

/* 
 * hooks to allow VTune/Prospect to cooperate with perfmon.
 * (reserved for system wide monitoring modules only)
 */
extern int pfm_install_alternate_syswide_subsystem(pfm_intr_handler_desc_t *h);
extern int pfm_remove_alternate_syswide_subsystem(pfm_intr_handler_desc_t *h);

/*
 * describe the content of the local_cpu_date->pfm_syst_info field
 */
#define PFM_CPUINFO_SYST_WIDE	0x1	/* if set a system wide session exist on the CPU */
#define PFM_CPUINFO_DCR_PP	0x2	/* if set a system wide session started on the CPU */
#define PFM_CPUINFO_EXCL_IDLE	0x4	/* system wide session excludes the idle task */

/*
 * macros to set the specific perfmon bits in each CPU's private data area
 */
#define PFM_CPUINFO_CLEAR(v)	local_cpu_data->pfm_syst_info &= ~(v)
#define PFM_CPUINFO_SET(v)	local_cpu_data->pfm_syst_info |= (v)

#endif /* __KERNEL__ */

#endif /* _ASM_IA64_PERFMON_H */
