/*
 * This file implements the perfmon subsystem which is used
 * to program the IA-64 Performance Monitoring Unit (PMU).
 *
 * Originaly Written by Ganesh Venkitachalam, IBM Corp.
 * Copyright (C) 1999 Ganesh Venkitachalam <venkitac@us.ibm.com>
 *
 * Modifications by Stephane Eranian, Hewlett-Packard Co.
 * Modifications by David Mosberger-Tang, Hewlett-Packard Co.
 *
 * Copyright (C) 1999-2003  Hewlett Packard Co
 *               Stephane Eranian <eranian@hpl.hp.com>
 *               David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/wrapper.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/smp.h>

#include <asm/bitops.h>
#include <asm/errno.h>
#include <asm/page.h>
#include <asm/perfmon.h>
#include <asm/processor.h>
#include <asm/signal.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/delay.h> /* for ia64_get_itc() */

#ifdef CONFIG_PERFMON

/*
 * For PMUs which rely on the debug registers for some features, you must
 * you must enable the following flag to activate the support for
 * accessing the registers via the perfmonctl() interface.
 */
#if defined(CONFIG_ITANIUM) || defined(CONFIG_MCKINLEY)
#define PFM_PMU_USES_DBR	1
#endif

/*
 * perfmon context states
 */
#define PFM_CTX_DISABLED	0
#define PFM_CTX_ENABLED		1

/*
 * Reset register flags
 */
#define PFM_PMD_LONG_RESET	1
#define PFM_PMD_SHORT_RESET	2

/*
 * Misc macros and definitions
 */
#define PMU_FIRST_COUNTER	4
#define PMU_MAX_PMCS		256
#define PMU_MAX_PMDS		256

/*
 * type of a PMU register (bitmask).
 * bitmask structure:
 * 	bit0   : register implemented
 * 	bit1   : end marker 
 * 	bit2-3 : reserved
 * 	bit4-7 : register type
 * 	bit8-31: reserved
 */
#define PFM_REG_IMPL		0x1 /* register implemented */
#define PFM_REG_END		0x2 /* end marker */
#define PFM_REG_MONITOR		(0x1<<4|PFM_REG_IMPL) /* a PMC with a pmc.pm field only */
#define PFM_REG_COUNTING	(0x2<<4|PFM_REG_IMPL) /* a PMC with a pmc.pm AND pmc.oi, a PMD used as a counter */
#define PFM_REG_CONTROL		(0x3<<4|PFM_REG_IMPL) /* PMU control register */
#define	PFM_REG_CONFIG		(0x4<<4|PFM_REG_IMPL) /* refine configuration */
#define PFM_REG_BUFFER	 	(0x5<<4|PFM_REG_IMPL) /* PMD used as buffer */

#define PMC_IS_LAST(i)	(pmu_conf.pmc_desc[i].type & PFM_REG_END)
#define PMD_IS_LAST(i)	(pmu_conf.pmd_desc[i].type & PFM_REG_END)

#define PFM_IS_DISABLED() pmu_conf.disabled

#define PMC_OVFL_NOTIFY(ctx, i)	((ctx)->ctx_soft_pmds[i].flags &  PFM_REGFL_OVFL_NOTIFY)
#define PFM_FL_INHERIT_MASK	(PFM_FL_INHERIT_NONE|PFM_FL_INHERIT_ONCE|PFM_FL_INHERIT_ALL)

/* i assume unsigned */
#define PMC_IS_IMPL(i)	  (i< PMU_MAX_PMCS && (pmu_conf.pmc_desc[i].type & PFM_REG_IMPL))
#define PMD_IS_IMPL(i)	  (i< PMU_MAX_PMDS && (pmu_conf.pmd_desc[i].type & PFM_REG_IMPL))

/* XXX: these three assume that register i is implemented */
#define PMD_IS_COUNTING(i) (pmu_conf.pmd_desc[i].type == PFM_REG_COUNTING)
#define PMC_IS_COUNTING(i) (pmu_conf.pmc_desc[i].type == PFM_REG_COUNTING)
#define PMC_IS_MONITOR(i)  (pmu_conf.pmc_desc[i].type == PFM_REG_MONITOR)
#define PMC_DFL_VAL(i)     pmu_conf.pmc_desc[i].default_value
#define PMC_RSVD_MASK(i)   pmu_conf.pmc_desc[i].reserved_mask
#define PMD_PMD_DEP(i)	   pmu_conf.pmd_desc[i].dep_pmd[0]
#define PMC_PMD_DEP(i)	   pmu_conf.pmc_desc[i].dep_pmd[0]

/* k assume unsigned */
#define IBR_IS_IMPL(k)	  (k<pmu_conf.num_ibrs)
#define DBR_IS_IMPL(k)	  (k<pmu_conf.num_dbrs)

#define CTX_IS_ENABLED(c) 	((c)->ctx_flags.state == PFM_CTX_ENABLED)
#define CTX_OVFL_NOBLOCK(c)	((c)->ctx_fl_block == 0)
#define CTX_INHERIT_MODE(c)	((c)->ctx_fl_inherit)
#define CTX_HAS_SMPL(c)		((c)->ctx_psb != NULL)
/* XXX: does not support more than 64 PMDs */
#define CTX_USED_PMD(ctx, mask) (ctx)->ctx_used_pmds[0] |= (mask)
#define CTX_IS_USED_PMD(ctx, c) (((ctx)->ctx_used_pmds[0] & (1UL << (c))) != 0UL)


#define CTX_USED_IBR(ctx,n) 	(ctx)->ctx_used_ibrs[(n)>>6] |= 1UL<< ((n) % 64)
#define CTX_USED_DBR(ctx,n) 	(ctx)->ctx_used_dbrs[(n)>>6] |= 1UL<< ((n) % 64)
#define CTX_USES_DBREGS(ctx)	(((pfm_context_t *)(ctx))->ctx_fl_using_dbreg==1)

#ifdef CONFIG_SMP
#define GET_ACTIVATION()	pmu_owners[smp_processor_id()].activation_number
#define INC_ACTIVATION()	pmu_owners[smp_processor_id()].activation_number++
#define SET_ACTIVATION(c)	(c)->ctx_last_activation = GET_ACTIVATION()
#define SET_LAST_CPU(ctx, v)	(ctx)->ctx_last_cpu = (v)
#define GET_LAST_CPU(ctx)	(ctx)->ctx_last_cpu
#else /* !CONFIG_SMP */
#define SET_ACTIVATION(t)	do {} while(0)
#define GET_ACTIVATION(t)	do {} while(0)
#define INC_ACTIVATION(t)	do {} while(0)
#define SET_LAST_CPU(ctx, v)	do {} while(0)
#define GET_LAST_CPU(ctx)	do {} while(0)
#endif /* CONFIG_SMP */


#define PFM_INVALID_ACTIVATION	(~0UL)

#define SET_PMU_OWNER(t)    do { pmu_owners[smp_processor_id()].owner = (t); } while(0)
#define PMU_OWNER()	    pmu_owners[smp_processor_id()].owner

#define LOCK_PFS()	    spin_lock(&pfm_sessions.pfs_lock)
#define UNLOCK_PFS()	    spin_unlock(&pfm_sessions.pfs_lock)

#define PFM_REG_RETFLAG_SET(flags, val)	do { flags &= ~PFM_REG_RETFL_MASK; flags |= (val); } while(0)

#define TASK_PTREGS(t) (((struct pt_regs *)((unsigned long) (t) + IA64_STK_OFFSET))-1)

/*
 * cmp0 must be the value of pmc0
 */
#define PMC0_HAS_OVFL(cmp0)  (cmp0 & ~0x1UL)


/*
 * debugging
 */
#define DBprintk(a) \
	do { \
		if (pfm_sysctl.debug >0) { printk("%s.%d: CPU%d ", __FUNCTION__, __LINE__, smp_processor_id()); printk a; } \
	} while (0)

#define DBprintk_ovfl(a) \
	do { \
		if (pfm_sysctl.debug > 0 && pfm_sysctl.debug_ovfl >0) { printk("%s.%d: CPU%d ", __FUNCTION__, __LINE__, smp_processor_id()); printk a; } \
	} while (0)



/* 
 * Architected PMC structure
 */
typedef struct {
	unsigned long pmc_plm:4;	/* privilege level mask */
	unsigned long pmc_ev:1;		/* external visibility */
	unsigned long pmc_oi:1;		/* overflow interrupt */
	unsigned long pmc_pm:1;		/* privileged monitor */
	unsigned long pmc_ig1:1;	/* reserved */
	unsigned long pmc_es:8;		/* event select */
	unsigned long pmc_ig2:48;	/* reserved */
} pfm_monitor_t;

/*
 * There is one such data structure per perfmon context. It is used to describe the
 * sampling buffer. It is to be shared among siblings whereas the pfm_context 
 * is not.
 * Therefore we maintain a refcnt which is incremented on fork().
 * This buffer is private to the kernel only the actual sampling buffer 
 * including its header are exposed to the user. This construct allows us to 
 * export the buffer read-write, if needed, without worrying about security 
 * problems.
 */
typedef struct _pfm_smpl_buffer_desc {
	spinlock_t		psb_lock;	/* protection lock */
	unsigned long		psb_refcnt;	/* how many users for the buffer */
	int			psb_flags;	/* bitvector of flags (not yet used) */

	void			*psb_addr;	/* points to location of first entry */
	unsigned long		psb_entries;	/* maximum number of entries */
	unsigned long		psb_size;	/* aligned size of buffer */
	unsigned long		psb_index;	/* next free entry slot XXX: must use the one in buffer */
	unsigned long		psb_entry_size;	/* size of each entry including entry header */

	perfmon_smpl_hdr_t	*psb_hdr;	/* points to sampling buffer header */

	struct _pfm_smpl_buffer_desc *psb_next;	/* next psb, used for rvfreeing of psb_hdr */

} pfm_smpl_buffer_desc_t;

/*
 * psb_flags
 */
#define PSB_HAS_VMA	0x1		/* a virtual mapping for the buffer exists */

#define LOCK_PSB(p)	spin_lock(&(p)->psb_lock)
#define UNLOCK_PSB(p)	spin_unlock(&(p)->psb_lock)

/*
 * 64-bit software counter structure
 */
typedef struct {
	u64 val;	/* virtual 64bit counter value */
	u64 lval;	/* last value */
	u64 long_reset;	/* reset value on sampling overflow */
	u64 short_reset;/* reset value on overflow */
	u64 reset_pmds[4]; /* which other pmds to reset when this counter overflows */
	u64 seed;	/* seed for random-number generator */
	u64 mask;	/* mask for random-number generator */
	unsigned int flags; /* notify/do not notify */
} pfm_counter_t;

/*
 * perfmon context. One per process, is cloned on fork() depending on 
 * inheritance flags
 */
typedef struct {
	unsigned int state:1;		/* 0=disabled, 1=enabled */
	unsigned int inherit:2;		/* inherit mode */
	unsigned int block:1;		/* when 1, task will blocked on user notifications */
	unsigned int system:1;		/* do system wide monitoring */
	unsigned int frozen:1;		/* pmu must be kept frozen on ctxsw in */
	unsigned int protected:1;	/* allow access to creator of context only */
	unsigned int using_dbreg:1;	/* using range restrictions (debug registers) */
	unsigned int excl_idle:1;	/* exclude idle task in system wide session */
	unsigned int unsecure:1;	/* sp = 0 for non self-monitored task */
	unsigned int reserved:22;
} pfm_context_flags_t;

/*
 * perfmon context: encapsulates all the state of a monitoring session
 * XXX: probably need to change layout
 */
typedef struct pfm_context {
	pfm_smpl_buffer_desc_t	*ctx_psb;		/* sampling buffer, if any */
	unsigned long		ctx_smpl_vaddr;		/* user level virtual address of smpl buffer */

	spinlock_t		ctx_lock;
	pfm_context_flags_t	ctx_flags;		/* block/noblock */

	struct task_struct	*ctx_notify_task;	/* who to notify on overflow */
	struct task_struct	*ctx_owner;		/* pid of creator (debug) */

	unsigned long		ctx_ovfl_regs[4];	/* which registers overflowed (notification) */
	unsigned long		ctx_smpl_regs[4];	/* which registers to record on overflow */

	struct semaphore	ctx_restart_sem;   	/* use for blocking notification mode */

	unsigned long		ctx_used_pmds[4];	/* bitmask of PMD used                 */
	unsigned long		ctx_reload_pmds[4];	/* bitmask of PMD to reload on ctxsw   */

	unsigned long		ctx_used_pmcs[4];	/* bitmask PMC used by context         */
	unsigned long		ctx_reload_pmcs[4];	/* bitmask of PMC to reload on ctxsw   */

	unsigned long		ctx_used_ibrs[4];	/* bitmask of used IBR (speedup ctxsw) */
	unsigned long		ctx_used_dbrs[4];	/* bitmask of used DBR (speedup ctxsw) */

	pfm_counter_t		ctx_soft_pmds[IA64_NUM_PMD_REGS]; /* XXX: size should be dynamic */

	u64			ctx_saved_psr;		/* copy of psr used for lazy ctxsw */
	unsigned long		ctx_saved_cpus_allowed;	/* copy of the task cpus_allowed (system wide) */
	unsigned long		ctx_last_activation;	/* context last activation number for last_cpu */
	unsigned int		ctx_last_cpu;		/* CPU id of current or last CPU used (SMP only) */
	unsigned int		ctx_cpu;		/* cpu to which perfmon is applied (system wide) */

	struct tasklet_struct   ctx_tasklet;		/* used for sending signal-based notifications */
} pfm_context_t;

#define PFM_GET_CTX(t)	((pfm_context_t *)(t)->thread.pfm_context)
#define LOCK_CTX(ctx)	spin_lock(&(ctx)->ctx_lock)
#define UNLOCK_CTX(ctx)	spin_unlock(&(ctx)->ctx_lock)

#define ctx_fl_inherit		ctx_flags.inherit
#define ctx_fl_block		ctx_flags.block
#define ctx_fl_system		ctx_flags.system
#define ctx_fl_frozen		ctx_flags.frozen
#define ctx_fl_protected	ctx_flags.protected
#define ctx_fl_using_dbreg	ctx_flags.using_dbreg
#define ctx_fl_excl_idle	ctx_flags.excl_idle
#define ctx_fl_unsecure		ctx_flags.unsecure

/*
 * global information about all sessions
 * mostly used to synchronize between system wide and per-process
 */
typedef struct {
	spinlock_t		pfs_lock;		   /* lock the structure */

	unsigned int 		pfs_task_sessions;	   /* number of per task sessions */
	unsigned int		pfs_sys_sessions;	   /* number of per system wide sessions */
	unsigned int		pfs_sys_use_dbregs;	   /* incremented when a system wide session uses debug regs */
	unsigned int		pfs_ptrace_use_dbregs;	   /* incremented when a process uses debug regs */
	struct task_struct	*pfs_sys_session[NR_CPUS]; /* point to task owning a system-wide session */
} pfm_session_t;

/*
 * information about a PMC or PMD.
 * dep_pmd[]: a bitmask of dependent PMD registers 
 * dep_pmc[]: a bitmask of dependent PMC registers
 */
typedef struct {
	unsigned int		type;
	int			pm_pos;
	unsigned long		default_value;	/* power-on default value */
	unsigned long		reserved_mask;	/* bitmask of reserved bits */
	int			(*read_check)(struct task_struct *task, unsigned int cnum, unsigned long *val, struct pt_regs *regs);
	int			(*write_check)(struct task_struct *task, unsigned int cnum, unsigned long *val, struct pt_regs *regs);
	unsigned long		dep_pmd[4];
	unsigned long		dep_pmc[4];
} pfm_reg_desc_t;

/* assume cnum is a valid monitor */
#define PMC_PM(cnum, val)	(((val) >> (pmu_conf.pmc_desc[cnum].pm_pos)) & 0x1)
#define PMC_WR_FUNC(cnum)	(pmu_conf.pmc_desc[cnum].write_check)
#define PMD_WR_FUNC(cnum)	(pmu_conf.pmd_desc[cnum].write_check)
#define PMD_RD_FUNC(cnum)	(pmu_conf.pmd_desc[cnum].read_check)

/*
 * This structure is initialized at boot time and contains
 * a description of the PMU main characteristics.
 */
typedef struct {
	unsigned int  disabled;		/* indicates if perfmon is working properly */
	unsigned long ovfl_val;		/* overflow value for generic counters   */
	unsigned long impl_pmcs[4];	/* bitmask of implemented PMCS */
	unsigned long impl_pmds[4];	/* bitmask of implemented PMDS */
	unsigned int  num_pmcs;		/* number of implemented PMCS */
	unsigned int  num_pmds;		/* number of implemented PMDS */
	unsigned int  num_ibrs;		/* number of implemented IBRS */
	unsigned int  num_dbrs;		/* number of implemented DBRS */
	unsigned int  num_counters;	/* number of PMD/PMC counters */
	pfm_reg_desc_t *pmc_desc;	/* detailed PMC register dependencies descriptions */
	pfm_reg_desc_t *pmd_desc;	/* detailed PMD register dependencies descriptions */
} pmu_config_t;

/*
 * perfmon command descriptions
 */
typedef struct {
	int		(*cmd_func)(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs);
	int		cmd_flags;
	unsigned int	cmd_narg;
	size_t		cmd_argsize;
} pfm_cmd_desc_t;

#define PFM_CMD_PID		0x1	/* command requires pid argument */
#define PFM_CMD_ARG_READ	0x2	/* command must read argument(s) */
#define PFM_CMD_ARG_RW		0x4	/* command must read/write argument(s) */
#define PFM_CMD_CTX		0x8	/* command needs a perfmon context */
#define PFM_CMD_NOCHK		0x10	/* command does not need to check task's state */

#define PFM_CMD_IDX(cmd)	(cmd)

#define PFM_CMD_IS_VALID(cmd)	((PFM_CMD_IDX(cmd) >= 0) && (PFM_CMD_IDX(cmd) < PFM_CMD_COUNT) \
				  && pfm_cmd_tab[PFM_CMD_IDX(cmd)].cmd_func != NULL)

#define PFM_CMD_USE_PID(cmd)	((pfm_cmd_tab[PFM_CMD_IDX(cmd)].cmd_flags & PFM_CMD_PID) != 0)
#define PFM_CMD_READ_ARG(cmd)	((pfm_cmd_tab[PFM_CMD_IDX(cmd)].cmd_flags & PFM_CMD_ARG_READ) != 0)
#define PFM_CMD_RW_ARG(cmd)	((pfm_cmd_tab[PFM_CMD_IDX(cmd)].cmd_flags & PFM_CMD_ARG_RW) != 0)
#define PFM_CMD_USE_CTX(cmd)	((pfm_cmd_tab[PFM_CMD_IDX(cmd)].cmd_flags & PFM_CMD_CTX) != 0)
#define PFM_CMD_CHK(cmd)	((pfm_cmd_tab[PFM_CMD_IDX(cmd)].cmd_flags & PFM_CMD_NOCHK) == 0)

#define PFM_CMD_ARG_MANY	-1 /* cannot be zero */
#define PFM_CMD_NARG(cmd)	(pfm_cmd_tab[PFM_CMD_IDX(cmd)].cmd_narg)
#define PFM_CMD_ARG_SIZE(cmd)	(pfm_cmd_tab[PFM_CMD_IDX(cmd)].cmd_argsize)

typedef struct {
	int	debug;		/* turn on/off debugging via syslog */
	int	debug_ovfl;	/* turn on/off debug printk in overflow handler */
	int	fastctxsw;	/* turn on/off fast (unsecure) ctxsw */
} pfm_sysctl_t;

typedef struct {
	unsigned long pfm_spurious_ovfl_intr_count; /* keep track of spurious ovfl interrupts */
	unsigned long pfm_ovfl_intr_count; /* keep track of ovfl interrupts */
	unsigned long pfm_recorded_samples_count;
	unsigned long pfm_full_smpl_buffer_count; /* how many times the sampling buffer was full */
	char pad[SMP_CACHE_BYTES] ____cacheline_aligned;
} pfm_stats_t;

/*
 * perfmon internal variables
 */
static pfm_session_t	pfm_sessions;	/* global sessions information */
static struct proc_dir_entry *perfmon_dir; /* for debug only */
static pfm_stats_t	pfm_stats[NR_CPUS];
static pfm_intr_handler_desc_t	*pfm_alternate_intr_handler;

/* sysctl() controls */
static pfm_sysctl_t pfm_sysctl;

static ctl_table pfm_ctl_table[]={
	{1, "debug", &pfm_sysctl.debug, sizeof(int), 0666, NULL, &proc_dointvec, NULL,},
	{2, "debug_ovfl", &pfm_sysctl.debug_ovfl, sizeof(int), 0666, NULL, &proc_dointvec, NULL,},
	{3, "fastctxsw", &pfm_sysctl.fastctxsw, sizeof(int), 0600, NULL, &proc_dointvec, NULL,},
	{ 0, },
};
static ctl_table pfm_sysctl_dir[] = {
	{1, "perfmon", NULL, 0, 0755, pfm_ctl_table, },
 	{0,},
};
static ctl_table pfm_sysctl_root[] = {
	{1, "kernel", NULL, 0, 0755, pfm_sysctl_dir, },
 	{0,},
};
static struct ctl_table_header *pfm_sysctl_header;

static void pfm_vm_close(struct vm_area_struct * area);

static struct vm_operations_struct pfm_vm_ops={
	.close =  pfm_vm_close
};

/*
 * keep track of task owning the PMU per CPU.
 */
static struct {
	struct task_struct *owner;
	unsigned long	   activation_number;
	char 		   pad[SMP_CACHE_BYTES] ____cacheline_aligned;
} pmu_owners[NR_CPUS];



/*
 * forward declarations
 */
static void pfm_reset_pmu(struct task_struct *);
#ifndef CONFIG_SMP
static unsigned long pfm_lazy_save_regs (struct task_struct *ta);
#endif

#if   defined(CONFIG_ITANIUM)
#include "perfmon_itanium.h"
#elif defined(CONFIG_MCKINLEY)
#include "perfmon_mckinley.h"
#else
#include "perfmon_generic.h"
#endif

static inline void
pfm_clear_psr_pp(void)
{
	__asm__ __volatile__ ("rsm psr.pp;; srlz.i;;"::: "memory");
}

static inline void
pfm_set_psr_pp(void)
{
	__asm__ __volatile__ ("ssm psr.pp;; srlz.i;;"::: "memory");
}

static inline void
pfm_clear_psr_up(void)
{
	__asm__ __volatile__ ("rsm psr.up;; srlz.i;;"::: "memory");
}

static inline void
pfm_set_psr_up(void)
{
	__asm__ __volatile__ ("ssm psr.up;; srlz.i;;"::: "memory");
}

static inline unsigned long
pfm_get_psr(void)
{
	unsigned long tmp;
	__asm__ __volatile__ ("mov %0=psr;;": "=r"(tmp) :: "memory");
	return tmp;
}

static inline void
pfm_set_psr_l(unsigned long val)
{
	__asm__ __volatile__ ("mov psr.l=%0;; srlz.i;;"::"r"(val): "memory");
}



static inline void
pfm_freeze_pmu(void)
{
	ia64_set_pmc(0,1UL);
	ia64_srlz_d();
}

static inline void
pfm_unfreeze_pmu(void)
{
	ia64_set_pmc(0,0UL);
	ia64_srlz_d();
}

static inline void
pfm_restore_ibrs(unsigned long *ibrs, unsigned int nibrs)
{
	int i;

	for (i=0; i < nibrs; i++) {
		ia64_set_ibr(i, ibrs[i]);
	}
	ia64_srlz_i();
}

static inline void
pfm_restore_dbrs(unsigned long *dbrs, unsigned int ndbrs)
{
	int i;

	for (i=0; i < ndbrs; i++) {
		ia64_set_dbr(i, dbrs[i]);
	}
	ia64_srlz_d();
}

static inline void
pfm_restore_pmcs(unsigned long *pmcs, unsigned long mask)
{
	int i;

	DBprintk(("mask=0x%lx\n", mask));
	for (i=0; mask; i++, mask>>=1) {
		if ((mask & 0x1) == 0) continue;
		ia64_set_pmc(i, pmcs[i]);
		DBprintk(("pmc[%d]=0x%lx\n", i, pmcs[i]));
	}
	ia64_srlz_d();
}

static inline void
pfm_restore_pmds(unsigned long *pmds, unsigned long mask)
{
	int i;
	unsigned long val, ovfl_val = pmu_conf.ovfl_val;

	DBprintk(("mask=0x%lx\n", mask));
	for (i=0; mask; i++, mask>>=1) {
		if ((mask & 0x1) == 0) continue;
		val = PMD_IS_COUNTING(i) ? pmds[i] & ovfl_val : pmds[i];
		ia64_set_pmd(i, val);
		DBprintk(("pmd[%d]=0x%lx\n", i, val));
	}
	ia64_srlz_d();
}

static inline void
pfm_save_pmds(unsigned long *pmds, unsigned long mask)
{
	int i;

	ia64_srlz_d();

	for (i=0; mask; i++, mask>>=1) {
		if (mask & 0x1) pmds[i] = ia64_get_pmd(i);
	}
}

static inline unsigned long
pfm_read_soft_counter(pfm_context_t *ctx, int i)
{
	return ctx->ctx_soft_pmds[i].val + (ia64_get_pmd(i) & pmu_conf.ovfl_val);
}

static inline void
pfm_write_soft_counter(pfm_context_t *ctx, int i, unsigned long val)
{
	ctx->ctx_soft_pmds[i].val = val  & ~pmu_conf.ovfl_val;
	/*
	 * writing to unimplemented part is ignore, so we do not need to
	 * mask off top part
	 */
	ia64_set_pmd(i, val & pmu_conf.ovfl_val);
}

/*
 * Generates a unique (per CPU) timestamp
 */
static inline unsigned long
pfm_get_stamp(void)
{
	/*
	 * XXX: must find something more efficient
	 */
	return ia64_get_itc();
}

/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the
 * area and marking the pages as reserved.
 */
static inline unsigned long
pfm_kvirt_to_pa(unsigned long adr)
{
	__u64 pa = ia64_tpa(adr);
	//DBprintk(("kv2pa(%lx-->%lx)\n", adr, pa));
	return pa;
}

static void *
pfm_rvmalloc(unsigned long size)
{
	void *mem;
	unsigned long adr, page;

	mem=vmalloc(size);
	if (mem) {
		//printk("perfmon: CPU%d pfm_rvmalloc(%ld)=%p\n", smp_processor_id(), size, mem);
		memset(mem, 0, size); /* Clear the ram out, no junk to the user */
		adr=(unsigned long) mem;
		while (size > 0) {
			page = pfm_kvirt_to_pa(adr);
			mem_map_reserve(virt_to_page(__va(page)));
			adr  += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
	}
	return mem;
}

static void
pfm_rvfree(void *mem, unsigned long size)
{
	unsigned long adr, page = 0;

	if (mem) {
		adr=(unsigned long) mem;
		while (size > 0) {
			page = pfm_kvirt_to_pa(adr);
			mem_map_unreserve(virt_to_page(__va(page)));
			adr+=PAGE_SIZE;
			size-=PAGE_SIZE;
		}
		vfree(mem);
	}
	return;
}

/*
 * This function gets called from mm/mmap.c:exit_mmap() only when there is a sampling buffer
 * attached to the context AND the current task has a mapping for it, i.e., it is the original
 * creator of the context.
 *
 * This function is used to remember the fact that the vma describing the sampling buffer
 * has now been removed. It can only be called when no other tasks share the same mm context.
 *
 */
static void 
pfm_vm_close(struct vm_area_struct *vma)
{
	pfm_smpl_buffer_desc_t *psb = (pfm_smpl_buffer_desc_t *)vma->vm_private_data;

	if (psb == NULL) {
		printk(KERN_DEBUG "perfmon: psb is null in [%d]\n", current->pid);
		return;
	}
	/*
	 * Add PSB to list of buffers to free on release_thread() when no more users
	 *
	 * This call is safe because, once the count is zero is cannot be modified anymore.
	 * This is not because there is no more user of the mm context, that the sampling
	 * buffer is not being used anymore outside of this task. In fact, it can still
	 * be accessed from within the kernel by another task (such as the monitored task).
	 *
	 * Therefore, we only move the psb into the list of buffers to free when we know
	 * nobody else is using it.
	 * The linked list if independent of the perfmon context, because in the case of
	 * multi-threaded processes, the last thread may not have been involved with
	 * monitoring however it will be the one removing the vma and it should therefore
	 * also remove the sampling buffer. This buffer cannot be removed until the vma
	 * is removed.
	 *
	 * This function cannot remove the buffer from here, because exit_mmap() must first
	 * complete. Given that there is no other vma related callback in the generic code,
	 * we have created our own with the linked list of sampling buffers to free. The list
	 * is part of the thread structure. In release_thread() we check if the list is
	 * empty. If not we call into perfmon to free the buffer and psb. That is the only
	 * way to ensure a safe deallocation of the sampling buffer which works when
	 * the buffer is shared between distinct processes or with multi-threaded programs.
	 *
	 * We need to lock the psb because the refcnt test and flag manipulation must
	 * looked like an atomic operation vis a vis pfm_context_exit()
	 */
	LOCK_PSB(psb);

	if (psb->psb_refcnt == 0) {

		psb->psb_next = current->thread.pfm_smpl_buf_list;
		current->thread.pfm_smpl_buf_list = psb;

		DBprintk(("[%d] add smpl @%p size %lu to smpl_buf_list psb_flags=0x%x\n", 
			current->pid, psb->psb_hdr, psb->psb_size, psb->psb_flags));
	}
	DBprintk(("[%d] clearing psb_flags=0x%x smpl @%p size %lu\n", 
			current->pid, psb->psb_flags, psb->psb_hdr, psb->psb_size));
	/*
	 * decrement the number vma for the buffer
	 */
	psb->psb_flags &= ~PSB_HAS_VMA;

	UNLOCK_PSB(psb);
}

/*
 * This function is called from pfm_destroy_context() and also from pfm_inherit()
 * to explicitely remove the sampling buffer mapping from the user level address space.
 */
static int
pfm_remove_smpl_mapping(struct task_struct *task)
{
	pfm_context_t *ctx = task->thread.pfm_context;
	pfm_smpl_buffer_desc_t *psb;
	int r;

	/*
	 * some sanity checks first
	 */
	if (ctx == NULL || task->mm == NULL || ctx->ctx_smpl_vaddr == 0 || ctx->ctx_psb == NULL) {
		printk(KERN_DEBUG "perfmon: invalid context mm=%p\n", task->mm);
		return -1;
	}
	psb = ctx->ctx_psb;

	down_write(&task->mm->mmap_sem);

	r = do_munmap(task->mm, ctx->ctx_smpl_vaddr, psb->psb_size);

	up_write(&task->mm->mmap_sem);
	if (r !=0) {
		printk(KERN_DEBUG "perfmon: pid %d unable to unmap sampling buffer "
		       "@0x%lx size=%ld\n", task->pid, ctx->ctx_smpl_vaddr, psb->psb_size);
	}

	DBprintk(("[%d] do_unmap(0x%lx, %ld)=%d refcnt=%lu psb_flags=0x%x\n",
		task->pid, ctx->ctx_smpl_vaddr, psb->psb_size, r, psb->psb_refcnt, psb->psb_flags));

	return 0;
}

static pfm_context_t *
pfm_context_alloc(void)
{
	pfm_context_t *ctx;

	/* allocate context descriptor */
	ctx = kmalloc(sizeof(pfm_context_t), GFP_KERNEL);
	if (ctx) memset(ctx, 0, sizeof(pfm_context_t));
	
	return ctx;
}

static void
pfm_context_free(pfm_context_t *ctx)
{
	if (ctx) {
		DBprintk(("kill tasklet for ctx %p\n", ctx));

		tasklet_kill(&ctx->ctx_tasklet);

		DBprintk(("free ctx @%p\n", ctx));
		kfree(ctx);
	}
}

static int
pfm_remap_buffer(unsigned long buf, unsigned long addr, unsigned long size)
{
	unsigned long page;

	DBprintk(("CPU%d buf=0x%lx addr=0x%lx size=%ld\n", smp_processor_id(), buf, addr, size));

	while (size > 0) {
		page = pfm_kvirt_to_pa(buf);

		if (remap_page_range(addr, page, PAGE_SIZE, PAGE_READONLY)) return -ENOMEM;

		addr  += PAGE_SIZE;
		buf   += PAGE_SIZE;
		size  -= PAGE_SIZE;
	}
	return 0;
}

/*
 * counts the number of PMDS to save per entry.
 * This code is generic enough to accomodate more than 64 PMDS when they become available
 */
static unsigned long
pfm_smpl_entry_size(unsigned long *which, unsigned long size)
{
	unsigned long res = 0;
	int i;

	for (i=0; i < size; i++, which++) res += hweight64(*which);

	DBprintk(("weight=%ld\n", res));

	return res;
}

/*
 * Allocates the sampling buffer and remaps it into caller's address space
 */
static int
pfm_smpl_buffer_alloc(pfm_context_t *ctx, unsigned long *which_pmds, unsigned long entries, 
		      void **user_vaddr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;
	unsigned long size, regcount;
	void *smpl_buf;
	pfm_smpl_buffer_desc_t *psb;


	/* note that regcount might be 0, in this case only the header for each
	 * entry will be recorded.
	 */
	regcount = pfm_smpl_entry_size(which_pmds, 1);

	if ((sizeof(perfmon_smpl_hdr_t)+ entries*sizeof(perfmon_smpl_entry_t)) <= entries) {
		DBprintk(("requested entries %lu is too big\n", entries));
		return -EINVAL;
	}

	/*
	 * 1 buffer hdr and for each entry a header + regcount PMDs to save
	 */
	size = PAGE_ALIGN(  sizeof(perfmon_smpl_hdr_t)
			  + entries * (sizeof(perfmon_smpl_entry_t) + regcount*sizeof(u64)));

	DBprintk(("sampling buffer size=%lu bytes\n", size));

	/*
	 * check requested size to avoid Denial-of-service attacks
	 * XXX: may have to refine this test	
	 * Check against address space limit.
	 *
	 * if ((mm->total_vm << PAGE_SHIFT) + len> current->rlim[RLIMIT_AS].rlim_cur) 
	 * 	return -ENOMEM;
	 */
	if (size > current->rlim[RLIMIT_MEMLOCK].rlim_cur) return -EAGAIN;

	/*
	 * We do the easy to undo allocations first.
 	 *
	 * pfm_rvmalloc(), clears the buffer, so there is no leak
	 */
	smpl_buf = pfm_rvmalloc(size);
	if (smpl_buf == NULL) {
		DBprintk(("Can't allocate sampling buffer\n"));
		return -ENOMEM;
	}

	DBprintk(("smpl_buf @%p\n", smpl_buf));

	/* allocate sampling buffer descriptor now */
	psb = kmalloc(sizeof(*psb), GFP_KERNEL);
	if (psb == NULL) {
		DBprintk(("Can't allocate sampling buffer descriptor\n"));
		goto error_kmalloc;
	}

	/* allocate vma */
	vma = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (!vma) {
		DBprintk(("Cannot allocate vma\n"));
		goto error_kmem;
	}
	/*
	 * partially initialize the vma for the sampling buffer
	 *
	 * The VM_DONTCOPY flag is very important as it ensures that the mapping
	 * will never be inherited for any child process (via fork()) which is always 
	 * what we want.
	 */
	vma->vm_mm	     = mm;
	vma->vm_flags	     = VM_READ| VM_MAYREAD |VM_RESERVED|VM_DONTCOPY;
	vma->vm_page_prot    = PAGE_READONLY; /* XXX may need to change */
	vma->vm_ops	     = &pfm_vm_ops; /* necesarry to get the close() callback */
	vma->vm_pgoff	     = 0;
	vma->vm_file	     = NULL;
	vma->vm_raend	     = 0;
	vma->vm_private_data = psb;	/* information needed by the pfm_vm_close() function */

	/*
	 * Now we have everything we need and we can initialize
	 * and connect all the data structures
	 */

	psb->psb_hdr	 = smpl_buf;
	psb->psb_addr    = ((char *)smpl_buf)+sizeof(perfmon_smpl_hdr_t); /* first entry */
	psb->psb_size    = size; /* aligned size */
	psb->psb_index   = 0;
	psb->psb_entries = entries;
	psb->psb_refcnt  = 1;
	psb->psb_flags   = PSB_HAS_VMA;

	spin_lock_init(&psb->psb_lock);

	/*
	 * XXX: will need to do cacheline alignment to avoid false sharing in SMP mode and
	 * multitask monitoring.
	 */
	psb->psb_entry_size = sizeof(perfmon_smpl_entry_t) + regcount*sizeof(u64);

	DBprintk(("psb @%p entry_size=%ld hdr=%p addr=%p refcnt=%lu psb_flags=0x%x\n", 
		  (void *)psb,psb->psb_entry_size, (void *)psb->psb_hdr, 
		  (void *)psb->psb_addr, psb->psb_refcnt, psb->psb_flags));

	/* initialize some of the fields of user visible buffer header */
	psb->psb_hdr->hdr_version    = PFM_SMPL_VERSION;
	psb->psb_hdr->hdr_entry_size = psb->psb_entry_size;
	psb->psb_hdr->hdr_pmds[0]    = which_pmds[0];

	/*
	 * Let's do the difficult operations next.
	 *
	 * now we atomically find some area in the address space and
	 * remap the buffer in it.
	 */
	down_write(&current->mm->mmap_sem);


	/* find some free area in address space, must have mmap sem held */
	vma->vm_start = get_unmapped_area(NULL, 0, size, 0, MAP_PRIVATE|MAP_ANONYMOUS);
	if (vma->vm_start == 0UL) {
		DBprintk(("Cannot find unmapped area for size %ld\n", size));
		up_write(&current->mm->mmap_sem);
		goto error;
	}
	vma->vm_end = vma->vm_start + size;

	DBprintk(("entries=%ld aligned size=%ld, unmapped @0x%lx\n", entries, size, vma->vm_start));

	/* can only be applied to current, need to have the mm semaphore held when called */
	if (pfm_remap_buffer((unsigned long)smpl_buf, vma->vm_start, size)) {
		DBprintk(("Can't remap buffer\n"));
		up_write(&current->mm->mmap_sem);
		goto error;
	}

	/*
	 * now insert the vma in the vm list for the process, must be
	 * done with mmap lock held
	 */
	insert_vm_struct(mm, vma);

	mm->total_vm  += size >> PAGE_SHIFT;

	up_write(&current->mm->mmap_sem);

	/* store which PMDS to record */
	ctx->ctx_smpl_regs[0] = which_pmds[0];


	/* link to perfmon context */
	ctx->ctx_psb        = psb;

	/*
	 * keep track of user level virtual address 
	 */
	ctx->ctx_smpl_vaddr = *(unsigned long *)user_vaddr = vma->vm_start;

	return 0;

error:
	kmem_cache_free(vm_area_cachep, vma);
error_kmem:
	kfree(psb);
error_kmalloc:
	pfm_rvfree(smpl_buf, size);
	return -ENOMEM;
}

static int
pfm_reserve_session(struct task_struct *task, int is_syswide, unsigned long cpu_mask)
{
	unsigned long m, undo_mask;
	unsigned int n, i;

	/*
	 * validy checks on cpu_mask have been done upstream
	 */
	LOCK_PFS();

	if (is_syswide) {
		/* 
		 * cannot mix system wide and per-task sessions
		 */
		if (pfm_sessions.pfs_task_sessions > 0UL) {
			DBprintk(("system wide not possible, %u conflicting task_sessions\n", 
			  	pfm_sessions.pfs_task_sessions));
			goto abort;
		}

		m = cpu_mask; undo_mask = 0UL; n = 0;
		DBprintk(("cpu_mask=0x%lx\n", cpu_mask));
		for(i=0; m; i++, m>>=1) {

			if ((m & 0x1) == 0UL) continue;

			if (pfm_sessions.pfs_sys_session[i]) goto undo;

			DBprintk(("reserving CPU%d currently on CPU%d\n", i, smp_processor_id()));

			pfm_sessions.pfs_sys_session[i] = task;
			undo_mask |= 1UL << i;
			n++;
		}
		pfm_sessions.pfs_sys_sessions += n;
	} else {
		if (pfm_sessions.pfs_sys_sessions) goto abort;
		pfm_sessions.pfs_task_sessions++;
	}
	UNLOCK_PFS();
	return 0;
undo:
	DBprintk(("system wide not possible, conflicting session [%d] on CPU%d\n",
  		pfm_sessions.pfs_sys_session[i]->pid, i));

	for(i=0; undo_mask; i++, undo_mask >>=1) {
		pfm_sessions.pfs_sys_session[i] = NULL;
	}
abort:
	UNLOCK_PFS();

	return -EBUSY;

}

static int
pfm_unreserve_session(struct task_struct *task, int is_syswide, unsigned long cpu_mask)
{
	pfm_context_t *ctx;
	unsigned long m;
	unsigned int n, i;

	ctx = task ? task->thread.pfm_context : NULL;

	/*
	 * validy checks on cpu_mask have been done upstream
	 */
	LOCK_PFS();

	DBprintk(("[%d] sys_sessions=%u task_sessions=%u dbregs=%u syswide=%d cpu_mask=0x%lx\n",
		task->pid,
		pfm_sessions.pfs_sys_sessions,
		pfm_sessions.pfs_task_sessions,
		pfm_sessions.pfs_sys_use_dbregs,
		is_syswide,
		cpu_mask));


	if (is_syswide) {
		m = cpu_mask; n = 0;
		for(i=0; m; i++, m>>=1) {
			if ((m & 0x1) == 0UL) continue;
			pfm_sessions.pfs_sys_session[i] = NULL;
			n++;
		}
		/* 
		 * would not work with perfmon+more than one bit in cpu_mask
		 */
		if (ctx && ctx->ctx_fl_using_dbreg) {
			if (pfm_sessions.pfs_sys_use_dbregs == 0) {
				printk(KERN_DEBUG "perfmon: invalid release for [%d] "
				       "sys_use_dbregs=0\n", task->pid);
			} else {
				pfm_sessions.pfs_sys_use_dbregs--;
			}
		}
		pfm_sessions.pfs_sys_sessions -= n;

		DBprintk(("CPU%d sys_sessions=%u\n", 
			smp_processor_id(), pfm_sessions.pfs_sys_sessions));
	} else {
		pfm_sessions.pfs_task_sessions--;
		DBprintk(("[%d] task_sessions=%u\n", 
			task->pid, pfm_sessions.pfs_task_sessions));
	}

	UNLOCK_PFS();

	return 0;
}

static void
pfm_send_notification_signal(unsigned long data)
{
	pfm_context_t *ctx = (pfm_context_t *)data;
	struct siginfo si;
	int ret;

	DBprintk(("[%d] tasklet called\n", current->pid));

	LOCK_CTX(ctx);

	if (ctx->ctx_notify_task == NULL) {
		printk(KERN_INFO "perfmon: tasklet lost notify_task\n");
		goto nothing_to_do;
	}
	/* no leak */
	memset(&si,0, sizeof(si));

	si.si_addr        = NULL;
	si.si_pid         = current->pid; /* irrelevant */
	si.si_signo       = SIGPROF;
	si.si_code        = PROF_OVFL; /* indicates a perfmon SIGPROF signal */
	si.si_pfm_ovfl[0] = ctx->ctx_ovfl_regs[0];

	if (ctx->ctx_notify_task != current) read_lock(&tasklist_lock);

	DBprintk_ovfl(("[%d] tasklet sending notification to [%d]\n", current->pid, ctx->ctx_notify_task->pid));

	ret = send_sig_info(SIGPROF, &si, ctx->ctx_notify_task);
	if (ret != 0) printk(KERN_ERR "send_sig_info(process %d, SIGPROF)=%d\n", ctx->ctx_notify_task->pid, ret);

	/*
	 * now undo the protections in order
	 */
	if (ctx->ctx_notify_task != current) read_unlock(&tasklist_lock);
nothing_to_do:
	UNLOCK_CTX(ctx);
}

/*
 * XXX: do something better here
 */
static int
pfm_bad_permissions(struct task_struct *task)
{
	/* stolen from bad_signal() */
	return (current->session != task->session)
	    && (current->euid ^ task->suid) && (current->euid ^ task->uid)
	    && (current->uid ^ task->suid) && (current->uid ^ task->uid);
}

static int
pfx_is_sane(struct task_struct *task, pfarg_context_t *pfx)
{
	unsigned long smpl_pmds = pfx->ctx_smpl_regs[0];
	int ctx_flags;
	int cpu;

	/* valid signal */

	/* cannot send to process 1, 0 means do not notify */
	if (pfx->ctx_notify_pid == 1) {
		DBprintk(("invalid notify_pid %d\n", pfx->ctx_notify_pid));
		return -EINVAL;
	}
	ctx_flags = pfx->ctx_flags;

	if ((ctx_flags & PFM_FL_INHERIT_MASK) == (PFM_FL_INHERIT_ONCE|PFM_FL_INHERIT_ALL)) {
		DBprintk(("invalid inherit mask 0x%x\n",ctx_flags & PFM_FL_INHERIT_MASK));
		return -EINVAL;
	}

	if (ctx_flags & PFM_FL_SYSTEM_WIDE) {
		DBprintk(("cpu_mask=0x%lx\n", pfx->ctx_cpu_mask));
		/*
		 * cannot block in this mode 
		 */
		if (ctx_flags & PFM_FL_NOTIFY_BLOCK) {
			DBprintk(("cannot use blocking mode when in system wide monitoring\n"));
			return -EINVAL;
		}
		/*
		 * must only have one bit set in the CPU mask
		 */
		if (hweight64(pfx->ctx_cpu_mask) != 1UL) {
			DBprintk(("invalid CPU mask specified\n"));
			return -EINVAL;
		}
		/*
		 * and it must be a valid CPU
		 */
		cpu = ffz(~pfx->ctx_cpu_mask);
		if (cpu_online(cpu) == 0) {
			DBprintk(("CPU%d is not online\n", cpu));
			return -EINVAL;
		}
		/*
		 * check for pre-existing pinning, if conflicting reject
		 */
		if (task->cpus_allowed != ~0UL && (task->cpus_allowed & (1UL<<cpu)) == 0) {
			DBprintk(("[%d] pinned on 0x%lx, mask for CPU%d \n", task->pid, 
				task->cpus_allowed, cpu));
			return -EINVAL;
		}

	} else {
		/*
		 * must provide a target for the signal in blocking mode even when
		 * no counter is configured with PFM_FL_REG_OVFL_NOTIFY
		 */
		if ((ctx_flags & PFM_FL_NOTIFY_BLOCK) && pfx->ctx_notify_pid == 0) {
			DBprintk(("must have notify_pid when blocking for [%d]\n", task->pid));
			return -EINVAL;
		}
#if 0
		if ((ctx_flags & PFM_FL_NOTIFY_BLOCK) && pfx->ctx_notify_pid == task->pid) {
			DBprintk(("cannot notify self when blocking for [%d]\n", task->pid));
			return -EINVAL;
		}
#endif
	}
	/* verify validity of smpl_regs */
	if ((smpl_pmds & pmu_conf.impl_pmds[0]) != smpl_pmds) {
		DBprintk(("invalid smpl_regs 0x%lx\n", smpl_pmds));
		return -EINVAL;
	}
	/* probably more to add here */

	return 0;
}

static int
pfm_context_create(struct task_struct *task, pfm_context_t *ctx, void *req, int count, 
		   struct pt_regs *regs)
{
	pfarg_context_t tmp;
	void *uaddr = NULL;
	int ret;
	int ctx_flags;
	pid_t notify_pid;

	/* a context has already been defined */
	if (ctx) return -EBUSY;

	/*
	 * not yet supported
	 */
	if (task != current) return -EINVAL;

	if (__copy_from_user(&tmp, req, sizeof(tmp))) return -EFAULT;

	ret = pfx_is_sane(task, &tmp);
	if (ret < 0) return ret;

	ctx_flags = tmp.ctx_flags;

	ret = pfm_reserve_session(task, ctx_flags & PFM_FL_SYSTEM_WIDE, tmp.ctx_cpu_mask);
	if (ret) goto abort;

	ret = -ENOMEM;

	ctx = pfm_context_alloc();
	if (!ctx) goto error;

	/* record the creator (important for inheritance) */
	ctx->ctx_owner = current;

	notify_pid = tmp.ctx_notify_pid;

	spin_lock_init(&ctx->ctx_lock);

	if (notify_pid == current->pid) {

		ctx->ctx_notify_task = current;
		task->thread.pfm_context = ctx;

	} else if (notify_pid!=0) {
		struct task_struct *notify_task;

		read_lock(&tasklist_lock);

		notify_task = find_task_by_pid(notify_pid);

		if (notify_task) {

			ret = -EPERM;

			/*
			 * check if we can send this task a signal
			 */
			if (pfm_bad_permissions(notify_task)) {
				read_unlock(&tasklist_lock);
				goto buffer_error;
			}

			/* 
		 	 * make visible
		 	 * must be done inside critical section
		 	 *
		 	 * if the initialization does not go through it is still
		 	 * okay because child will do the scan for nothing which
		 	 * won't hurt.
		 	 */
			task->thread.pfm_context = ctx;

			/*
			 * will cause task to check on exit for monitored
			 * processes that would notify it. see release_thread()
			 * Note: the scan MUST be done in release thread, once the
			 * task has been detached from the tasklist otherwise you are
			 * exposed to race conditions.
			 */
			atomic_add(1, &ctx->ctx_notify_task->thread.pfm_notifiers_check);

			ctx->ctx_notify_task = notify_task;
		}
		read_unlock(&tasklist_lock);
	}

	/*
	 * notification process does not exist
	 */
	if (notify_pid != 0 && ctx->ctx_notify_task == NULL) {
		ret = -EINVAL;
		goto buffer_error;
	}

	if (tmp.ctx_smpl_entries) {
		DBprintk(("sampling entries=%lu\n",tmp.ctx_smpl_entries));

		ret = pfm_smpl_buffer_alloc(ctx, tmp.ctx_smpl_regs, 
						 tmp.ctx_smpl_entries, &uaddr);
		if (ret<0) goto buffer_error;

		tmp.ctx_smpl_vaddr = uaddr;
	}
	/* initialization of context's flags */
	ctx->ctx_fl_inherit   = ctx_flags & PFM_FL_INHERIT_MASK;
	ctx->ctx_fl_block     = (ctx_flags & PFM_FL_NOTIFY_BLOCK) ? 1 : 0;
	ctx->ctx_fl_system    = (ctx_flags & PFM_FL_SYSTEM_WIDE) ? 1: 0;
	ctx->ctx_fl_excl_idle = (ctx_flags & PFM_FL_EXCL_IDLE) ? 1: 0;
	ctx->ctx_fl_unsecure  = (ctx_flags & PFM_FL_UNSECURE) ? 1: 0;
	ctx->ctx_fl_frozen    = 0;
	/*
	 * setting this flag to 0 here means, that the creator or the task that the
	 * context is being attached are granted access. Given that a context can only
	 * be created for the calling process this, in effect only allows the creator
	 * to access the context. See pfm_protect() for more.
	 */
	ctx->ctx_fl_protected = 0;

	/* for system wide mode only (only 1 bit set) */
	ctx->ctx_cpu = ffz(~tmp.ctx_cpu_mask);

	/* SMP only, means no CPU */
	ctx->ctx_last_activation = PFM_INVALID_ACTIVATION;
	SET_LAST_CPU(ctx, -1);

	sema_init(&ctx->ctx_restart_sem, 0); /* init this semaphore to locked */

	/*
	 * initialize tasklet for signal notifications
	 *
	 * ALL signal-based (or any notification using data structures
	 * external to perfmon) MUST use tasklets to avoid lock contentions
	 * when a signal has to be sent for overflow interrupt handler.
	 */
	tasklet_init(&ctx->ctx_tasklet, pfm_send_notification_signal, (unsigned long)ctx);

	if (__copy_to_user(req, &tmp, sizeof(tmp))) {
		ret = -EFAULT;
		goto buffer_error;
	}

	DBprintk(("context=%p, pid=%d notify_task=%p\n",
			(void *)ctx, task->pid, ctx->ctx_notify_task));

	DBprintk(("context=%p, pid=%d flags=0x%x inherit=%d block=%d system=%d excl_idle=%d unsecure=%d\n", 
			(void *)ctx, task->pid, ctx_flags, ctx->ctx_fl_inherit, 
			ctx->ctx_fl_block, ctx->ctx_fl_system, 
			ctx->ctx_fl_excl_idle,
			ctx->ctx_fl_unsecure));

	/*
	 * when no notification is required, we can make this visible at the last moment
	 */
	if (notify_pid == 0) task->thread.pfm_context = ctx;
	/*
	 * pin task to CPU and force reschedule on exit to ensure
	 * that when back to user level the task runs on the designated
	 * CPU.
	 */
	if (ctx->ctx_fl_system) {
		ctx->ctx_saved_cpus_allowed = task->cpus_allowed;
		task->cpus_allowed = tmp.ctx_cpu_mask;
		task->need_resched = 1;
		DBprintk(("[%d] rescheduled allowed=0x%lx\n", task->pid, task->cpus_allowed));
	}

	return 0;

buffer_error:
	pfm_context_free(ctx);
error:
	pfm_unreserve_session(task, ctx_flags & PFM_FL_SYSTEM_WIDE , tmp.ctx_cpu_mask);
abort:
	/* make sure we don't leave anything behind */
	task->thread.pfm_context = NULL;

	return ret;
}

static inline unsigned long
pfm_new_counter_value (pfm_counter_t *reg, int is_long_reset)
{
	unsigned long val = is_long_reset ? reg->long_reset : reg->short_reset;
	unsigned long new_seed, old_seed = reg->seed, mask = reg->mask;
	extern unsigned long carta_random32 (unsigned long seed);

	if (reg->flags & PFM_REGFL_RANDOM) {
		new_seed = carta_random32(old_seed);
		val -= (old_seed & mask);	/* counter values are negative numbers! */
		if ((mask >> 32) != 0)
			/* construct a full 64-bit random value: */
			new_seed |= carta_random32(old_seed >> 32) << 32;
		reg->seed = new_seed;
	}
	reg->lval = val;
	return val;
}

static void
pfm_reset_regs(pfm_context_t *ctx, unsigned long *ovfl_regs, int flag)
{
	unsigned long mask = ovfl_regs[0];
	unsigned long reset_others = 0UL;
	unsigned long val;
	int i, is_long_reset = (flag == PFM_PMD_LONG_RESET);

	/*
	 * now restore reset value on sampling overflowed counters
	 */
	mask >>= PMU_FIRST_COUNTER;
	for(i = PMU_FIRST_COUNTER; mask; i++, mask >>= 1) {
		if (mask & 0x1) {
			val = pfm_new_counter_value(ctx->ctx_soft_pmds + i, is_long_reset);
			reset_others |= ctx->ctx_soft_pmds[i].reset_pmds[0];

			DBprintk_ovfl(("[%d] %s reset soft_pmd[%d]=%lx\n", current->pid,
				  is_long_reset ? "long" : "short", i, val));

			/* upper part is ignored on rval */
			pfm_write_soft_counter(ctx, i, val);
		}
	}

	/*
	 * Now take care of resetting the other registers
	 */
	for(i = 0; reset_others; i++, reset_others >>= 1) {

		if ((reset_others & 0x1) == 0) continue;

		val = pfm_new_counter_value(ctx->ctx_soft_pmds + i, is_long_reset);

		if (PMD_IS_COUNTING(i)) {
			pfm_write_soft_counter(ctx, i, val);
		} else {
			ia64_set_pmd(i, val);
		}
		DBprintk_ovfl(("[%d] %s reset_others pmd[%d]=%lx\n", current->pid,
			  is_long_reset ? "long" : "short", i, val));
	}
	ia64_srlz_d();
}

static int
pfm_write_pmcs(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct thread_struct *th = &task->thread;
	pfarg_reg_t tmp, *req = (pfarg_reg_t *)arg;
	unsigned long value, reset_pmds;
	unsigned int cnum, reg_flags, flags;
	int is_monitor, is_counting;
	int i, ret = -EINVAL;
#define PFM_CHECK_PMC_PM(x, y, z) ((x)->ctx_fl_system ^ PMC_PM(y, z))

	/* we don't quite support this right now */
	if (task != current) return -EINVAL;

	if (!CTX_IS_ENABLED(ctx)) return -EINVAL;


	/* XXX: ctx locking may be required here */

	for (i = 0; i < count; i++, req++) {

		if (__copy_from_user(&tmp, req, sizeof(tmp))) return -EFAULT;

		cnum       = tmp.reg_num;
		reg_flags  = tmp.reg_flags;
		value      = tmp.reg_value;
		reset_pmds = tmp.reg_reset_pmds[0];
		flags      = 0;

		is_counting = PMC_IS_COUNTING(cnum);
		is_monitor  = PMC_IS_MONITOR(cnum);

		/* 
		 * we reject all non implemented PMC as well
		 * as attempts to modify PMC[0-3] which are used
		 * as status registers by the PMU
		 */
		if (!PMC_IS_IMPL(cnum) || cnum < 4) {
			DBprintk(("pmc[%u] is unimplemented or invalid\n", cnum));
			goto error;
		}
		/*
		 * If the PMC is a monitor, then if the value is not the default:
		 * 	- system-wide session: PMCx.pm=1 (privileged monitor)
		 * 	- per-task           : PMCx.pm=0 (user monitor)
		 */
		if ((is_monitor || is_counting) && value != PMC_DFL_VAL(cnum) && PFM_CHECK_PMC_PM(ctx, cnum, value)) {
			DBprintk(("pmc%u pmc_pm=%ld fl_system=%d\n", 
				cnum, 
				PMC_PM(cnum, value), 
				ctx->ctx_fl_system));
			goto error;
		}

		if (is_counting) {
			pfm_monitor_t *p = (pfm_monitor_t *)&value;
			/*
		 	 * enforce generation of overflow interrupt. Necessary on all
		 	 * CPUs.
		 	 */
			p->pmc_oi = 1;

			if (reg_flags & PFM_REGFL_OVFL_NOTIFY) {
				/*
			 	 * must have a target for the signal
			 	 */
				if (ctx->ctx_notify_task == NULL) {
					DBprintk(("cannot set ovfl_notify: no notify_task\n"));
					goto error;
				}
				flags |= PFM_REGFL_OVFL_NOTIFY;
			}

			if (reg_flags & PFM_REGFL_RANDOM) flags |= PFM_REGFL_RANDOM;

			/* verify validity of reset_pmds */
			if ((reset_pmds & pmu_conf.impl_pmds[0]) != reset_pmds) {
				DBprintk(("invalid reset_pmds 0x%lx for pmc%u\n", reset_pmds, cnum));
				goto error;
			}
		} else if (reg_flags & (PFM_REGFL_OVFL_NOTIFY|PFM_REGFL_RANDOM)) {
				DBprintk(("cannot set ovfl_notify or random on pmc%u\n", cnum));
				goto error;
		}

		/*
		 * execute write checker, if any
		 */
		if (PMC_WR_FUNC(cnum)) {
			ret = PMC_WR_FUNC(cnum)(task, cnum, &value, regs);
			if (ret) goto error;
			ret = -EINVAL;
		}

		/*
		 * no error on this register
		 */
		PFM_REG_RETFLAG_SET(tmp.reg_flags, 0);

		/*
		 * update register return value, abort all if problem during copy.
		 * we only modify the reg_flags field. no check mode is fine because
		 * access has been verified upfront in sys_perfmonctl().
		 *
		 * If this fails, then the software state is not modified
		 */
		if (__put_user(tmp.reg_flags, &req->reg_flags)) return -EFAULT;

		/*
		 * Now we commit the changes to the software state
		 */

		/* 
		 * full flag update each time a register is programmed
		 */
		ctx->ctx_soft_pmds[cnum].flags = flags;

		if (is_counting) {
			ctx->ctx_soft_pmds[cnum].reset_pmds[0] = reset_pmds;

			/* mark all PMDS to be accessed as used */
			CTX_USED_PMD(ctx, reset_pmds);
		}

		/*
		 * Needed in case the user does not initialize the equivalent
		 * PMD. Clearing is done in reset_pmu() so there is no possible
		 * leak here.
		 */
		CTX_USED_PMD(ctx, pmu_conf.pmc_desc[cnum].dep_pmd[0]);

		/* 
		 * keep copy the pmc, used for register reload
		 */
		th->pmc[cnum] = value;

		ia64_set_pmc(cnum, value);

		DBprintk(("[%d] pmc[%u]=0x%lx flags=0x%x used_pmds=0x%lx\n", 
			  task->pid, cnum, value, 
			  ctx->ctx_soft_pmds[cnum].flags, 
			  ctx->ctx_used_pmds[0]));

	}

	return 0;

error:
	PFM_REG_RETFLAG_SET(tmp.reg_flags, PFM_REG_RETFL_EINVAL);

	if (__put_user(tmp.reg_flags, &req->reg_flags)) ret = -EFAULT;

	DBprintk(("[%d] pmc[%u]=0x%lx error %d\n", task->pid, cnum, value, ret));

	return ret;
}

static int
pfm_write_pmds(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	pfarg_reg_t tmp, *req = (pfarg_reg_t *)arg;
	unsigned long value, hw_value;
	unsigned int cnum;
	int i;
	int ret = -EINVAL;

	/* we don't quite support this right now */
	if (task != current) return -EINVAL;

	/* 
	 * Cannot do anything before PMU is enabled 
	 */
	if (!CTX_IS_ENABLED(ctx)) return -EINVAL;

	/* XXX: ctx locking may be required here */


	for (i = 0; i < count; i++, req++) {

		if (__copy_from_user(&tmp, req, sizeof(tmp))) return -EFAULT;

		cnum  = tmp.reg_num;
		value = tmp.reg_value;

		if (!PMD_IS_IMPL(cnum)) {
			DBprintk(("pmd[%u] is unimplemented or invalid\n", cnum));
			goto abort_mission;
		}

		/*
		 * execute write checker, if any
		 */
		if (PMD_WR_FUNC(cnum)) {
			unsigned long v = value;
			ret = PMD_WR_FUNC(cnum)(task, cnum, &v, regs);
			if (ret) goto abort_mission;
			value = v;
			ret = -EINVAL;
		}
		hw_value = value;
		/*
		 * no error on this register
		 */
		PFM_REG_RETFLAG_SET(tmp.reg_flags, 0);

		if (__put_user(tmp.reg_flags, &req->reg_flags)) return -EFAULT;

		/*
		 * now commit changes to software state
		 */

		/* update virtualized (64bits) counter */
		if (PMD_IS_COUNTING(cnum)) {
			ctx->ctx_soft_pmds[cnum].lval = value;
			ctx->ctx_soft_pmds[cnum].val  = value & ~pmu_conf.ovfl_val;

			hw_value = value & pmu_conf.ovfl_val;

			ctx->ctx_soft_pmds[cnum].long_reset  = tmp.reg_long_reset;
			ctx->ctx_soft_pmds[cnum].short_reset = tmp.reg_short_reset;

			ctx->ctx_soft_pmds[cnum].seed = tmp.reg_random_seed;
			ctx->ctx_soft_pmds[cnum].mask = tmp.reg_random_mask;
		}

		/* keep track of what we use */
		CTX_USED_PMD(ctx, pmu_conf.pmd_desc[(cnum)].dep_pmd[0]);

		/* mark this register as used as well */
		CTX_USED_PMD(ctx, RDEP(cnum));

		/* writes to unimplemented part is ignored, so this is safe */
		ia64_set_pmd(cnum, hw_value);

		/* to go away */
		ia64_srlz_d();

		DBprintk(("[%d] pmd[%u]: value=0x%lx hw_value=0x%lx soft_pmd=0x%lx  short_reset=0x%lx "
			  "long_reset=0x%lx hw_pmd=%lx notify=%c used_pmds=0x%lx reset_pmds=0x%lx psr=%d\n",
				task->pid, cnum,
				value, hw_value,
				ctx->ctx_soft_pmds[cnum].val,
				ctx->ctx_soft_pmds[cnum].short_reset,
				ctx->ctx_soft_pmds[cnum].long_reset,
				ia64_get_pmd(cnum) & pmu_conf.ovfl_val,
				PMC_OVFL_NOTIFY(ctx, cnum) ? 'Y':'N',
				ctx->ctx_used_pmds[0],
				ctx->ctx_soft_pmds[cnum].reset_pmds[0], ia64_psr(regs)->sp));
	}

	return 0;

abort_mission:
	/*
	 * for now, we have only one possibility for error
	 */
	PFM_REG_RETFLAG_SET(tmp.reg_flags, PFM_REG_RETFL_EINVAL);

	/*
	 * we change the return value to EFAULT in case we cannot write register return code.
	 * The caller first must correct this error, then a resubmission of the request will
	 * eventually yield the EINVAL.
	 */
	if (__put_user(tmp.reg_flags, &req->reg_flags)) ret = -EFAULT;

	DBprintk(("[%d] pmc[%u]=0x%lx ret %d\n", task->pid, cnum, value, ret));

	return ret;
}

static int
pfm_read_pmds(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct thread_struct *th = &task->thread;
	unsigned long val, lval;
	pfarg_reg_t *req = (pfarg_reg_t *)arg;
	unsigned int cnum, reg_flags = 0;
	int i, ret = 0;
#if __GNUC__ < 3
	int foo;
#endif

	if (!CTX_IS_ENABLED(ctx)) {
		DBprintk(("context for [%d] is disabled\n", task->pid));
		return -EINVAL;
	}

	/*
	 * XXX: MUST MAKE SURE WE DON"T HAVE ANY PENDING OVERFLOW BEFORE READING
	 * This is required when the monitoring has been stoppped by user or kernel.
	 * If it is still going on, then that's fine because we a re not guaranteed
	 * to return an accurate value in this case.
	 */

	/* XXX: ctx locking may be required here */

	/*
	 * should we need to access the PMU, serialization is needed
	 */
	ia64_srlz_d();

	for (i = 0; i < count; i++, req++) {

#if __GNUC__ < 3
		foo = __get_user(cnum, &req->reg_num);
		if (foo) return -EFAULT;
		foo = __get_user(reg_flags, &req->reg_flags);
		if (foo) return -EFAULT;
#else
		if (__get_user(cnum, &req->reg_num)) return -EFAULT;
		if (__get_user(reg_flags, &req->reg_flags)) return -EFAULT;
#endif
		lval = 0UL;

		if (!PMD_IS_IMPL(cnum)) goto abort_mission;
		/*
		 * we can only read the register that we use. That includes
		 * the one we explicitely initialize AND the one we want included
		 * in the sampling buffer (smpl_regs).
		 *
		 * Having this restriction allows optimization in the ctxsw routine
		 * without compromising security (leaks)
		 */
		if (!CTX_IS_USED_PMD(ctx, cnum)) goto abort_mission;

		/*
		 * we can access the registers directly only when task
		 * is the OWNER of the local PMU. In SMP, this can
		 * happen only when task == current. In addition
		 * this can happen when task != currrent but
		 * only in UP mode.
		 */
		if (task == PMU_OWNER()) {
			val = ia64_get_pmd(cnum);
			DBprintk(("reading pmd[%u]=0x%lx from hw\n", cnum, val));
		} else {
			/* context has been saved */
			val = th->pmd[cnum];
		}

		if (PMD_IS_COUNTING(cnum)) {
			/*
			 * XXX: need to check for overflow
			 */
			val &= pmu_conf.ovfl_val;
			val += ctx->ctx_soft_pmds[cnum].val;

			lval = ctx->ctx_soft_pmds[cnum].lval;
		} 

		/*
		 * execute read checker, if any
		 */
		if (PMD_RD_FUNC(cnum)) {
			unsigned long v = val;
			ret = PMD_RD_FUNC(cnum)(task, cnum, &v, regs);
			val = v;
		}

		PFM_REG_RETFLAG_SET(reg_flags, ret);

		DBprintk(("read pmd[%u] ret=%d value=0x%lx pmc=0x%lx\n", 
					cnum, ret, val, ia64_get_pmc(cnum)));

		/*
		 * update register return value, abort all if problem during copy.
		 * we only modify the reg_flags field. no check mode is fine because
		 * access has been verified upfront in sys_perfmonctl().
		 */
		if (__put_user(cnum, &req->reg_num)) return -EFAULT;
		if (__put_user(val, &req->reg_value)) return -EFAULT;
		if (__put_user(reg_flags, &req->reg_flags)) return -EFAULT;
		if (__put_user(lval, &req->reg_last_reset_value)) return -EFAULT;
	}

	return 0;

abort_mission:
	PFM_REG_RETFLAG_SET(reg_flags, PFM_REG_RETFL_EINVAL);
	/* 
	 * XXX: if this fails, we stick with the original failure, flag not updated!
	 */
	__put_user(reg_flags, &req->reg_flags);

	return -EINVAL;
}

#ifdef PFM_PMU_USES_DBR
/*
 * Only call this function when a process it trying to
 * write the debug registers (reading is always allowed)
 */
int
pfm_use_debug_registers(struct task_struct *task)
{
	pfm_context_t *ctx = task->thread.pfm_context;
	int ret = 0;

	DBprintk(("called for [%d]\n", task->pid));

	/*
	 * do it only once
	 */
	if (task->thread.flags & IA64_THREAD_DBG_VALID) return 0;

	/*
	 * Even on SMP, we do not need to use an atomic here because
	 * the only way in is via ptrace() and this is possible only when the
	 * process is stopped. Even in the case where the ctxsw out is not totally
	 * completed by the time we come here, there is no way the 'stopped' process
	 * could be in the middle of fiddling with the pfm_write_ibr_dbr() routine.
	 * So this is always safe.
	 */
	if (ctx && ctx->ctx_fl_using_dbreg == 1) return -1;

	LOCK_PFS();

	/*
	 * We cannot allow setting breakpoints when system wide monitoring
	 * sessions are using the debug registers.
	 */
	if (pfm_sessions.pfs_sys_use_dbregs> 0)
		ret = -1;
	else
		pfm_sessions.pfs_ptrace_use_dbregs++;

	DBprintk(("ptrace_use_dbregs=%u  sys_use_dbregs=%u by [%d] ret = %d\n", 
		  pfm_sessions.pfs_ptrace_use_dbregs, 
		  pfm_sessions.pfs_sys_use_dbregs, 
		  task->pid, ret));

	UNLOCK_PFS();

	return ret;
}

/*
 * This function is called for every task that exits with the
 * IA64_THREAD_DBG_VALID set. This indicates a task which was
 * able to use the debug registers for debugging purposes via
 * ptrace(). Therefore we know it was not using them for
 * perfmormance monitoring, so we only decrement the number
 * of "ptraced" debug register users to keep the count up to date
 */
int
pfm_release_debug_registers(struct task_struct *task)
{
	int ret;

	LOCK_PFS();
	if (pfm_sessions.pfs_ptrace_use_dbregs == 0) {
		printk(KERN_DEBUG "perfmon: invalid release for [%d] ptrace_use_dbregs=0\n",
		       task->pid);
		ret = -1;
	}  else {
		pfm_sessions.pfs_ptrace_use_dbregs--;
		ret = 0;
	}
	UNLOCK_PFS();

	return ret;
}
#else /* PFM_PMU_USES_DBR is true */
/*
 * in case, the PMU does not use the debug registers, these two functions are nops.
 * The first function is called from arch/ia64/kernel/ptrace.c.
 * The second function is called from arch/ia64/kernel/process.c.
 */
int
pfm_use_debug_registers(struct task_struct *task)
{
	return 0;
}

int
pfm_release_debug_registers(struct task_struct *task)
{
	return 0;
}
#endif /* PFM_PMU_USES_DBR */

static int
pfm_restart(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	 struct pt_regs *regs)
{
	void *sem = &ctx->ctx_restart_sem;

	/* 
	 * Cannot do anything before PMU is enabled 
	 */
	if (!CTX_IS_ENABLED(ctx)) return -EINVAL;

	if (task == current) {
		DBprintk(("restarting self %d frozen=%d ovfl_regs=0x%lx\n", 
			task->pid, 
			ctx->ctx_fl_frozen,
			ctx->ctx_ovfl_regs[0]));

		pfm_reset_regs(ctx, ctx->ctx_ovfl_regs, PFM_PMD_LONG_RESET);

		ctx->ctx_ovfl_regs[0] = 0UL;

		/*
		 * We ignore block/don't block because we never block
		 * for a self-monitoring process.
		 */
		ctx->ctx_fl_frozen = 0;

		if (CTX_HAS_SMPL(ctx)) {
			ctx->ctx_psb->psb_hdr->hdr_count = 0;
			ctx->ctx_psb->psb_index = 0;
		}

		/* simply unfreeze */
		pfm_unfreeze_pmu();

		return 0;
	} 
	/* restart on another task */

	/*
	 * if blocking, then post the semaphore.
	 * if non-blocking, then we ensure that the task will go into
	 * pfm_overflow_must_block() before returning to user mode. 
	 * We cannot explicitely reset another task, it MUST always
	 * be done by the task itself. This works for system wide because
	 * the tool that is controlling the session is doing "self-monitoring".
	 *
	 * XXX: what if the task never goes back to user?
	 *
	 */
	if (CTX_OVFL_NOBLOCK(ctx) == 0) {
		DBprintk(("unblocking %d \n", task->pid));
		up(sem);
	} else {
		task->thread.pfm_ovfl_block_reset = 1;
	}
#if 0
	/*
	 * in case of non blocking mode, then it's just a matter of
	 * of reseting the sampling buffer (if any) index. The PMU
	 * is already active.
	 */

	/*
	 * must reset the header count first
	 */
	if (CTX_HAS_SMPL(ctx)) {
		DBprintk(("resetting sampling indexes for %d \n", task->pid));
		ctx->ctx_psb->psb_hdr->hdr_count = 0;
		ctx->ctx_psb->psb_index = 0;
	}
#endif
	return 0;
}

static int
pfm_stop(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	 struct pt_regs *regs)
{
	/* we don't quite support this right now */
	if (task != current) return -EINVAL;

	/* 
	 * Cannot do anything before PMU is enabled 
	 */
	if (!CTX_IS_ENABLED(ctx)) return -EINVAL;

	DBprintk(("[%d] fl_system=%d owner=%p current=%p\n",
				current->pid,
				ctx->ctx_fl_system, PMU_OWNER(),
				current));

	/* simply stop monitoring but not the PMU */
	if (ctx->ctx_fl_system) {

		/* disable dcr pp */
		ia64_set_dcr(ia64_get_dcr() & ~IA64_DCR_PP);

		/* stop monitoring */
		pfm_clear_psr_pp();
		ia64_srlz_i();

		PFM_CPUINFO_CLEAR(PFM_CPUINFO_DCR_PP);

		ia64_psr(regs)->pp = 0;

	} else {

		/* stop monitoring */
		pfm_clear_psr_up();
		ia64_srlz_i();

		/*
		 * clear user level psr.up
		 */
		ia64_psr(regs)->up = 0;
	}
	return 0;
}

static int
pfm_disable(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	   struct pt_regs *regs)
{	
	/* we don't quite support this right now */
	if (task != current) return -EINVAL;

	if (!CTX_IS_ENABLED(ctx)) return -EINVAL;

	/*
	 * stop monitoring, freeze PMU, and save state in context
	 * this call will clear IA64_THREAD_PM_VALID for per-task sessions.
	 */
	pfm_flush_regs(task);

	if (ctx->ctx_fl_system) {	
		ia64_psr(regs)->pp = 0;
	} else {
		ia64_psr(regs)->up = 0;
	}
	/* 
	 * goes back to default behavior: no user level control
	 * no need to change live psr.sp because useless at the kernel level
	 */
	ia64_psr(regs)->sp = 1;

	DBprintk(("enabling psr.sp for [%d]\n", current->pid));

	ctx->ctx_flags.state = PFM_CTX_DISABLED;

	return 0;
}

static int
pfm_context_destroy(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	 struct pt_regs *regs)
{
	/* we don't quite support this right now */
	if (task != current) return -EINVAL;

	/*
	 * if context was never enabled, then there is not much
	 * to do
	 */
	if (!CTX_IS_ENABLED(ctx)) goto skipped_stop;

	/*
	 * Disable context: stop monitoring, flush regs to software state (useless here), 
	 * and freeze PMU
	 * 
	 * The IA64_THREAD_PM_VALID is cleared by pfm_flush_regs() called from pfm_disable()
	 */
	pfm_disable(task, ctx, arg, count, regs);

	if (ctx->ctx_fl_system) {	
		ia64_psr(regs)->pp = 0;
	} else {
		ia64_psr(regs)->up = 0;
	}

skipped_stop:
	/*
	 * remove sampling buffer mapping, if any
	 */
	if (ctx->ctx_smpl_vaddr) {
		pfm_remove_smpl_mapping(task);
		ctx->ctx_smpl_vaddr = 0UL;
	}
	/* now free context and related state */
	pfm_context_exit(task);

	return 0;
}

/*
 * does nothing at the moment
 */
static int
pfm_context_unprotect(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	 struct pt_regs *regs)
{
	return 0;
}

static int
pfm_protect_context(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	 struct pt_regs *regs)
{
	/*
	 * from now on, only the creator of the context has access to it
	 */
	ctx->ctx_fl_protected = 1;

	/*
	 * reinforce secure monitoring: cannot toggle psr.up
	 */
	if (ctx->ctx_fl_unsecure == 0) ia64_psr(regs)->sp = 1;

	DBprintk(("[%d] protected psr.sp=%d\n", task->pid, ia64_psr(regs)->sp));

	return 0;
}

static int
pfm_debug(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	 struct pt_regs *regs)
{
	unsigned int mode = *(unsigned int *)arg;

	pfm_sysctl.debug = mode == 0 ? 0 : 1;

	printk(KERN_INFO "perfmon debugging %s\n", pfm_sysctl.debug ? "on" : "off");

	return 0;
}

#ifdef PFM_PMU_USES_DBR

typedef struct {
	unsigned long ibr_mask:56;
	unsigned long ibr_plm:4;
	unsigned long ibr_ig:3;
	unsigned long ibr_x:1;
} ibr_mask_reg_t;

typedef struct {
	unsigned long dbr_mask:56;
	unsigned long dbr_plm:4;
	unsigned long dbr_ig:2;
	unsigned long dbr_w:1;
	unsigned long dbr_r:1;
} dbr_mask_reg_t;

typedef union {
	unsigned long  val;
	ibr_mask_reg_t ibr;
	dbr_mask_reg_t dbr;
} dbreg_t;


static int
pfm_write_ibr_dbr(int mode, struct task_struct *task, void *arg, int count, struct pt_regs *regs)
{
	struct thread_struct *thread = &task->thread;
	pfm_context_t *ctx = task->thread.pfm_context;
	pfarg_dbreg_t tmp, *req = (pfarg_dbreg_t *)arg;
	dbreg_t dbreg;
	unsigned int rnum;
	int first_time;
	int i, ret = 0;

	/*
	 * we do not need to check for ipsr.db because we do clear ibr.x, dbr.r, and dbr.w
	 * ensuring that no real breakpoint can be installed via this call.
	 */

	first_time = ctx->ctx_fl_using_dbreg == 0;

	/*
	 * check for debug registers in system wide mode
	 *
	 */
	LOCK_PFS();
	if (ctx->ctx_fl_system && first_time) {
		if (pfm_sessions.pfs_ptrace_use_dbregs) 
			ret = -EBUSY;
		else
			pfm_sessions.pfs_sys_use_dbregs++;
	}
	UNLOCK_PFS();

	if (ret != 0) return ret;

	if (ctx->ctx_fl_system) {
		/* we mark ourselves as owner  of the debug registers */
		ctx->ctx_fl_using_dbreg = 1;
		DBprintk(("system-wide setting fl_using_dbreg for [%d]\n", task->pid));
	} else if (first_time) {
			ret= -EBUSY;
			if ((thread->flags & IA64_THREAD_DBG_VALID) != 0) {
				DBprintk(("debug registers already in use for [%d]\n", task->pid));
				goto abort_mission;
			}
			/* we mark ourselves as owner  of the debug registers */
			ctx->ctx_fl_using_dbreg = 1;

			DBprintk(("setting fl_using_dbreg for [%d]\n", task->pid));
			/* 
			 * Given debug registers cannot be used for both debugging 
			 * and performance monitoring at the same time, we reuse
			 * the storage area to save and restore the registers on ctxsw.
			 */
			memset(task->thread.dbr, 0, sizeof(task->thread.dbr));
			memset(task->thread.ibr, 0, sizeof(task->thread.ibr));
	}

	if (first_time) {
		DBprintk(("[%d] clearing ibrs,dbrs\n", task->pid));
		/*
	 	 * clear hardware registers to make sure we don't
	 	 * pick up stale state. 
		 *
		 * for a system wide session, we do not use
		 * thread.dbr, thread.ibr because this process
		 * never leaves the current CPU and the state
		 * is shared by all processes running on it
	 	 */
		for (i=0; i < pmu_conf.num_ibrs; i++) {
			ia64_set_ibr(i, 0UL);
		}
		ia64_srlz_i();
		for (i=0; i < pmu_conf.num_dbrs; i++) {
			ia64_set_dbr(i, 0UL);
		}
		ia64_srlz_d();
	}

	ret = -EFAULT;

	/*
	 * Now install the values into the registers
	 */
	for (i = 0; i < count; i++, req++) {
		
		if (__copy_from_user(&tmp, req, sizeof(tmp))) goto abort_mission;
		
		rnum      = tmp.dbreg_num;
		dbreg.val = tmp.dbreg_value;
		
		ret = -EINVAL;

		if ((mode == 0 && !IBR_IS_IMPL(rnum)) || ((mode == 1) && !DBR_IS_IMPL(rnum))) {
			DBprintk(("invalid register %u val=0x%lx mode=%d i=%d count=%d\n", 
				  rnum, dbreg.val, mode, i, count));

			goto abort_mission;
		}

		/*
		 * make sure we do not install enabled breakpoint
		 */
		if (rnum & 0x1) {
			if (mode == 0) 
				dbreg.ibr.ibr_x = 0;
			else
				dbreg.dbr.dbr_r = dbreg.dbr.dbr_w = 0;
		}

		/*
		 * clear return flags and copy back to user
		 *
		 * XXX: fix once EAGAIN is implemented
		 */
		ret = -EFAULT;

		PFM_REG_RETFLAG_SET(tmp.dbreg_flags, 0);

		if (__copy_to_user(req, &tmp, sizeof(tmp))) goto abort_mission;

		/*
		 * Debug registers, just like PMC, can only be modified
		 * by a kernel call. Moreover, perfmon() access to those
		 * registers are centralized in this routine. The hardware
		 * does not modify the value of these registers, therefore,
		 * if we save them as they are written, we can avoid having
		 * to save them on context switch out. This is made possible
		 * by the fact that when perfmon uses debug registers, ptrace()
		 * won't be able to modify them concurrently.
		 */
		if (mode == 0) {
			CTX_USED_IBR(ctx, rnum);

			ia64_set_ibr(rnum, dbreg.val);
			ia64_srlz_i();

			thread->ibr[rnum] = dbreg.val;

			DBprintk(("write ibr%u=0x%lx used_ibrs=0x%lx\n", rnum, dbreg.val, ctx->ctx_used_ibrs[0]));
		} else {
			CTX_USED_DBR(ctx, rnum);

			ia64_set_dbr(rnum, dbreg.val);
			ia64_srlz_d();

			thread->dbr[rnum] = dbreg.val;

			DBprintk(("write dbr%u=0x%lx used_dbrs=0x%lx\n", rnum, dbreg.val, ctx->ctx_used_dbrs[0]));
		}
	}

	return 0;

abort_mission:
	/*
	 * in case it was our first attempt, we undo the global modifications
	 */
	if (first_time) {
		LOCK_PFS();
		if (ctx->ctx_fl_system) {
			pfm_sessions.pfs_sys_use_dbregs--;
		}
		UNLOCK_PFS();
		ctx->ctx_fl_using_dbreg = 0;
	}
	/*
	 * install error return flag
	 */
	if (ret != -EFAULT) {
		/*
		 * XXX: for now we can only come here on EINVAL
		 */
		PFM_REG_RETFLAG_SET(tmp.dbreg_flags, PFM_REG_RETFL_EINVAL);
		if (__put_user(tmp.dbreg_flags, &req->dbreg_flags)) ret = -EFAULT;
	}
	return ret;
}

static int
pfm_write_ibrs(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	 struct pt_regs *regs)
{	
	/* we don't quite support this right now */
	if (task != current) return -EINVAL;

	if (!CTX_IS_ENABLED(ctx)) return -EINVAL;

	return pfm_write_ibr_dbr(0, task, arg, count, regs);
}

static int
pfm_write_dbrs(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	 struct pt_regs *regs)
{	
	/* we don't quite support this right now */
	if (task != current) return -EINVAL;

	if (!CTX_IS_ENABLED(ctx)) return -EINVAL;

	return pfm_write_ibr_dbr(1, task, arg, count, regs);
}

#endif /* PFM_PMU_USES_DBR */

static int
pfm_get_features(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	pfarg_features_t tmp;

	memset(&tmp, 0, sizeof(tmp));

	tmp.ft_version      = PFM_VERSION;
	tmp.ft_smpl_version = PFM_SMPL_VERSION;

	if (__copy_to_user(arg, &tmp, sizeof(tmp))) return -EFAULT;

	return 0;
}

static int
pfm_start(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	  struct pt_regs *regs)
{
	/* we don't quite support this right now */
	if (task != current) return -EINVAL;

	/* 
	 * Cannot do anything before PMU is enabled 
	 */
	if (!CTX_IS_ENABLED(ctx)) return -EINVAL;

	DBprintk(("[%d] fl_system=%d owner=%p current=%p\n",
				current->pid,
				ctx->ctx_fl_system, PMU_OWNER(),
				current));

	if (PMU_OWNER() != task) {
		printk(KERN_DEBUG "perfmon: pfm_start task [%d] not pmu owner\n", task->pid);
		return -EINVAL;
	}

	if (ctx->ctx_fl_system) {
		
		PFM_CPUINFO_SET(PFM_CPUINFO_DCR_PP);

		/* set user level psr.pp */
		ia64_psr(regs)->pp = 1;

		/* start monitoring at kernel level */
		pfm_set_psr_pp();

		/* enable dcr pp */
		ia64_set_dcr(ia64_get_dcr()|IA64_DCR_PP);

		ia64_srlz_i();

	} else {
		if ((task->thread.flags & IA64_THREAD_PM_VALID) == 0) {
			printk(KERN_DEBUG "perfmon: pfm_start task flag not set for [%d]\n",
			       task->pid);
			return -EINVAL;
		}
		/* set user level psr.up */
		ia64_psr(regs)->up = 1;

		/* start monitoring at kernel level */
		pfm_set_psr_up();

		ia64_srlz_i();
	}

	return 0;
}

static int
pfm_enable(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	   struct pt_regs *regs)
{
	/* we don't quite support this right now */
	if (task != current) {
		DBprintk(("task [%d] != current [%d]\n", task->pid, current->pid));
		return -EINVAL;
	}

#ifndef CONFIG_SMP
	if (ctx->ctx_fl_system == 0 && PMU_OWNER()  && PMU_OWNER() != current) 
		pfm_lazy_save_regs(PMU_OWNER());
#endif

	/* reset all registers to stable quiet state */
	pfm_reset_pmu(task);

	/* make sure nothing starts */
	if (ctx->ctx_fl_system) {
		ia64_psr(regs)->pp = 0;
		ia64_psr(regs)->up = 0; /* just to make sure! */

		/* make sure monitoring is stopped */
		pfm_clear_psr_pp();
		ia64_srlz_i();

		PFM_CPUINFO_CLEAR(PFM_CPUINFO_DCR_PP);
		PFM_CPUINFO_SET(PFM_CPUINFO_SYST_WIDE);
		if (ctx->ctx_fl_excl_idle) PFM_CPUINFO_SET(PFM_CPUINFO_EXCL_IDLE);
	} else {
		/*
		 * needed in case the task was a passive task during
		 * a system wide session and now wants to have its own
		 * session
		 */
		ia64_psr(regs)->pp = 0; /* just to make sure! */
		ia64_psr(regs)->up = 0;

		/* make sure monitoring is stopped */
		pfm_clear_psr_up();
		ia64_srlz_i();

		DBprintk(("clearing psr.sp for [%d]\n", current->pid));

		/* allow user level control  */
		ia64_psr(regs)->sp = 0;

		/* PMU state will be saved/restored on ctxsw */
		task->thread.flags |= IA64_THREAD_PM_VALID;
	}

	SET_PMU_OWNER(task);

	ctx->ctx_flags.state = PFM_CTX_ENABLED;
	SET_LAST_CPU(ctx, smp_processor_id());
	INC_ACTIVATION();
	SET_ACTIVATION(ctx);

	/* simply unfreeze */
	pfm_unfreeze_pmu();

	return 0;
}

static int
pfm_get_pmc_reset(struct task_struct *task, pfm_context_t *ctx, void *arg, int count, 
	   struct pt_regs *regs)
{
	pfarg_reg_t tmp, *req = (pfarg_reg_t *)arg;
	unsigned int cnum;
	int i, ret = -EINVAL;

	for (i = 0; i < count; i++, req++) {

		if (__copy_from_user(&tmp, req, sizeof(tmp))) return -EFAULT;

		cnum = tmp.reg_num;

		if (!PMC_IS_IMPL(cnum)) goto abort_mission;

		tmp.reg_value = PMC_DFL_VAL(cnum);

		PFM_REG_RETFLAG_SET(tmp.reg_flags, 0);

		DBprintk(("pmc_reset_val pmc[%u]=0x%lx\n", cnum, tmp.reg_value)); 

		if (__copy_to_user(req, &tmp, sizeof(tmp))) return -EFAULT;
	}
	return 0;
abort_mission:
	PFM_REG_RETFLAG_SET(tmp.reg_flags, PFM_REG_RETFL_EINVAL);
	if (__copy_to_user(req, &tmp, sizeof(tmp))) ret = -EFAULT;

	return ret;
}

/*
 * functions MUST be listed in the increasing order of their index (see permfon.h)
 */
static pfm_cmd_desc_t pfm_cmd_tab[]={
/* 0  */{ NULL, 0, 0, 0}, /* not used */
/* 1  */{ pfm_write_pmcs, PFM_CMD_PID|PFM_CMD_CTX|PFM_CMD_ARG_RW, PFM_CMD_ARG_MANY, sizeof(pfarg_reg_t)}, 
/* 2  */{ pfm_write_pmds, PFM_CMD_PID|PFM_CMD_CTX|PFM_CMD_ARG_RW, PFM_CMD_ARG_MANY, sizeof(pfarg_reg_t)},
/* 3  */{ pfm_read_pmds,PFM_CMD_PID|PFM_CMD_CTX|PFM_CMD_ARG_RW, PFM_CMD_ARG_MANY, sizeof(pfarg_reg_t)}, 
/* 4  */{ pfm_stop, PFM_CMD_PID|PFM_CMD_CTX, 0, 0},
/* 5  */{ pfm_start, PFM_CMD_PID|PFM_CMD_CTX, 0, 0},
/* 6  */{ pfm_enable, PFM_CMD_PID|PFM_CMD_CTX, 0, 0},
/* 7  */{ pfm_disable, PFM_CMD_PID|PFM_CMD_CTX, 0, 0},
/* 8  */{ pfm_context_create, PFM_CMD_PID|PFM_CMD_ARG_RW, 1, sizeof(pfarg_context_t)},
/* 9  */{ pfm_context_destroy, PFM_CMD_PID|PFM_CMD_CTX, 0, 0},
/* 10 */{ pfm_restart, PFM_CMD_PID|PFM_CMD_CTX|PFM_CMD_NOCHK, 0, 0},
/* 11 */{ pfm_protect_context, PFM_CMD_PID|PFM_CMD_CTX, 0, 0},
/* 12 */{ pfm_get_features, PFM_CMD_ARG_RW, 0, 0},
/* 13 */{ pfm_debug, 0, 1, sizeof(unsigned int)},
/* 14 */{ pfm_context_unprotect, PFM_CMD_PID|PFM_CMD_CTX, 0, 0},
/* 15 */{ pfm_get_pmc_reset, PFM_CMD_ARG_RW, PFM_CMD_ARG_MANY, sizeof(pfarg_reg_t)},
/* 16 */{ NULL, 0, 0, 0}, /* not used */
/* 17 */{ NULL, 0, 0, 0}, /* not used */
/* 18 */{ NULL, 0, 0, 0}, /* not used */
/* 19 */{ NULL, 0, 0, 0}, /* not used */
/* 20 */{ NULL, 0, 0, 0}, /* not used */
/* 21 */{ NULL, 0, 0, 0}, /* not used */
/* 22 */{ NULL, 0, 0, 0}, /* not used */
/* 23 */{ NULL, 0, 0, 0}, /* not used */
/* 24 */{ NULL, 0, 0, 0}, /* not used */
/* 25 */{ NULL, 0, 0, 0}, /* not used */
/* 26 */{ NULL, 0, 0, 0}, /* not used */
/* 27 */{ NULL, 0, 0, 0}, /* not used */
/* 28 */{ NULL, 0, 0, 0}, /* not used */
/* 29 */{ NULL, 0, 0, 0}, /* not used */
/* 30 */{ NULL, 0, 0, 0}, /* not used */
/* 31 */{ NULL, 0, 0, 0}, /* not used */
#ifdef PFM_PMU_USES_DBR
/* 32 */{ pfm_write_ibrs, PFM_CMD_PID|PFM_CMD_CTX|PFM_CMD_ARG_RW, PFM_CMD_ARG_MANY, sizeof(pfarg_dbreg_t)},
/* 33 */{ pfm_write_dbrs, PFM_CMD_PID|PFM_CMD_CTX|PFM_CMD_ARG_RW, PFM_CMD_ARG_MANY, sizeof(pfarg_dbreg_t)}
#endif
};
#define PFM_CMD_COUNT	(sizeof(pfm_cmd_tab)/sizeof(pfm_cmd_desc_t))

static int
check_task_state(struct task_struct *task)
{
	int ret = 0;
#ifdef CONFIG_SMP
	/* We must wait until the state has been completely
	 * saved. There can be situations where the reader arrives before
	 * after the task is marked as STOPPED but before pfm_save_regs()
	 * is completed.
	 */
	for (;;) {

		task_lock(task);
		DBprintk((" [%d] state=%ld\n", task->pid, task->state));
		if (!task_has_cpu(task)) break;
		task_unlock(task);

		do {
			if (task->state != TASK_ZOMBIE && task->state != TASK_STOPPED) {
				DBprintk(("warning [%d] not in stable state %ld\n", task->pid, task->state));
				return -EBUSY;
			}
			barrier();
			cpu_relax();
		} while (task_has_cpu(task));
	}
	task_unlock(task);
#else
	if (task->state != TASK_ZOMBIE && task->state != TASK_STOPPED) {
		DBprintk(("warning [%d] not in stable state %ld\n", task->pid, task->state));
		ret = -EBUSY;
	}
#endif
	return ret;
}

asmlinkage long
sys_perfmonctl (pid_t pid, int cmd, void *arg, int count, long arg5, long arg6, long arg7, 
		long arg8, long stack)
{
	struct pt_regs *regs = (struct pt_regs *)&stack;
	struct task_struct *task = current;
	pfm_context_t *ctx;
	size_t sz;
	long ret;
	int narg;

	/* 
	 * reject any call if perfmon was disabled at initialization time
	 */
	if (PFM_IS_DISABLED()) return -ENOSYS;

	DBprintk(("cmd=%d idx=%d valid=%d narg=0x%x\n", cmd, PFM_CMD_IDX(cmd), 
		  PFM_CMD_IS_VALID(cmd), PFM_CMD_NARG(cmd)));

	if (PFM_CMD_IS_VALID(cmd) == 0) return -EINVAL;

	/* ingore arguments when command has none */
	narg = PFM_CMD_NARG(cmd);
	if ((narg == PFM_CMD_ARG_MANY  && count == 0) || (narg > 0 && narg != count)) return -EINVAL;

	sz = PFM_CMD_ARG_SIZE(cmd);

	if (PFM_CMD_READ_ARG(cmd) && !access_ok(VERIFY_READ, arg, sz*count)) return -EFAULT;

	if (PFM_CMD_RW_ARG(cmd) && !access_ok(VERIFY_WRITE, arg, sz*count)) return -EFAULT;

	if (PFM_CMD_USE_PID(cmd))  {
		/* 
		 * XXX: may need to fine tune this one
		 */
		if (pid < 2) return -EPERM;

		if (pid != current->pid) {

			ret = -ESRCH;

			read_lock(&tasklist_lock);

			task = find_task_by_pid(pid);

			if (!task) goto abort_call;

			ret = -EPERM;

			if (pfm_bad_permissions(task)) goto abort_call;

			if (PFM_CMD_CHK(cmd)) {
				ret = check_task_state(task);
				if (ret != 0) {
					DBprintk(("check_task_state=%ld for [%d]\n", ret, task->pid));
					goto abort_call;
				}
			}
		} 
	} 

	ctx = PFM_GET_CTX(task);

	if (PFM_CMD_USE_CTX(cmd)) {
		ret = -EINVAL;
	       if (ctx == NULL) {
			DBprintk(("no context for task %d\n", task->pid));
			goto abort_call;
	       }


	       ret = -EPERM;
	       /*
		* we only grant access to the context if:
		* 	- the caller is the creator of the context (ctx_owner)
		*  OR   - the context is attached to the caller AND The context IS NOT 
		*  	  in protected mode
		*/
	       if (ctx->ctx_owner != current && (ctx->ctx_fl_protected || task != current)) {
				DBprintk(("context protected, no access for [%d]\n", task->pid));
				goto abort_call;
	       }
	}

	ret = (*pfm_cmd_tab[PFM_CMD_IDX(cmd)].cmd_func)(task, ctx, arg, count, regs);

abort_call:
	if (task != current) read_unlock(&tasklist_lock);

	return ret;
}

void asmlinkage
pfm_ovfl_block_reset(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, 
		      u64 arg6, u64 arg7, long info)
{
	struct thread_struct *th = &current->thread;
	pfm_context_t *ctx = current->thread.pfm_context;
	int ret;

	/*
	 * clear the flag, to make sure we won't get here
	 * again
	 */
	th->pfm_ovfl_block_reset = 0;

	/*
	 * do some sanity checks first
	 */
	if (!ctx) {
		printk(KERN_DEBUG "perfmon: [%d] has no PFM context\n", current->pid);
		return;
	}

	if (CTX_OVFL_NOBLOCK(ctx)) goto non_blocking;

	DBprintk(("[%d] before sleeping\n", current->pid));

	/*
	 * may go through without blocking on SMP systems
	 * if restart has been received already by the time we call down()
	 */
	ret = down_interruptible(&ctx->ctx_restart_sem);

	DBprintk(("[%d] after sleeping ret=%d\n", current->pid, ret));

	/*
	 * in case of interruption of down() we don't restart anything
	 */
	if (ret >= 0) {

non_blocking:
		/* we reactivate on context switch */
		ctx->ctx_fl_frozen = 0;
		/*
		 * the ovfl_sem is cleared by the restart task and this is safe because we always
		 * use the local reference
		 */

		pfm_reset_regs(ctx, ctx->ctx_ovfl_regs, PFM_PMD_LONG_RESET);

		ctx->ctx_ovfl_regs[0] = 0UL;

		/*
		 * Unlock sampling buffer and reset index atomically
		 * XXX: not really needed when blocking
		 */
		if (CTX_HAS_SMPL(ctx)) {
			ctx->ctx_psb->psb_hdr->hdr_count = 0;
			ctx->ctx_psb->psb_index = 0;
		}

		pfm_unfreeze_pmu();

		/* state restored, can go back to work (user mode) */
	}
}

/*
 * This function will record an entry in the sampling if it is not full already.
 * Input:
 * 	ovfl_mask: mask of overflowed PMD. MUST NEVER be 0.
 * Return:
 * 	0 : buffer is not full (did not BECOME full: still space or was already full)
 * 	1 : buffer is full (recorded the last entry)
 */
static int
pfm_record_sample(struct task_struct *task, pfm_context_t *ctx, unsigned long ovfl_mask, struct pt_regs *regs)
{
	pfm_smpl_buffer_desc_t *psb = ctx->ctx_psb;
	unsigned long *e, m, idx;
	perfmon_smpl_entry_t *h;
	int j;

	idx = ia64_fetch_and_add(1, &psb->psb_index);
	DBprintk_ovfl(("recording index=%ld entries=%ld\n", idx-1, psb->psb_entries));

	/*
	 * XXX: there is a small chance that we could run out on index before resetting
	 * but index is unsigned long, so it will take some time.....
	 * We use > instead of == because fetch_and_add() is off by one (see below)
	 *
	 * This case can happen in non-blocking mode or with multiple processes.
	 * For non-blocking, we need to reload and continue.
	 */
	if (idx > psb->psb_entries) return 0;

	/* first entry is really entry 0, not 1 caused by fetch_and_add */
	idx--;

	h = (perfmon_smpl_entry_t *)(((char *)psb->psb_addr) + idx*(psb->psb_entry_size));

	/*
	 * initialize entry header
	 */
	h->pid  = ctx->ctx_fl_system ? current->pid : task->pid;
	h->cpu  = smp_processor_id();
	h->last_reset_value = ovfl_mask ? ctx->ctx_soft_pmds[ffz(~ovfl_mask)].lval : 0UL;
	h->ip   = regs ? regs->cr_iip | ((regs->cr_ipsr >> 41) & 0x3): 0x0UL;
	h->regs = ovfl_mask; 			/* which registers overflowed */

	/* guaranteed to monotonically increase on each cpu */
	h->stamp  = pfm_get_stamp();

	/* position for first pmd */
	e = (unsigned long *)(h+1);

	/*
	 * selectively store PMDs in increasing index number
	 */
	m = ctx->ctx_smpl_regs[0];
	for (j=0; m; m >>=1, j++) {

		if ((m & 0x1) == 0) continue;

		if (PMD_IS_COUNTING(j)) {
			*e  =  pfm_read_soft_counter(ctx, j);
		} else {
			*e = ia64_get_pmd(j); /* slow */
		}
		DBprintk_ovfl(("e=%p pmd%d =0x%lx\n", (void *)e, j, *e));
		e++;
	}
	pfm_stats[smp_processor_id()].pfm_recorded_samples_count++;

	/*
	 * make the new entry visible to user, needs to be atomic
	 */
	ia64_fetch_and_add(1, &psb->psb_hdr->hdr_count);

	DBprintk_ovfl(("index=%ld entries=%ld hdr_count=%ld\n", 
				idx, psb->psb_entries, psb->psb_hdr->hdr_count));
	/* 
	 * sampling buffer full ? 
	 */
	if (idx == (psb->psb_entries-1)) {
		DBprintk_ovfl(("sampling buffer full\n"));
		/*
		 * XXX: must reset buffer in blocking mode and lost notified
		 */
		pfm_stats[smp_processor_id()].pfm_full_smpl_buffer_count++;
		return 1;
	}
	return 0;
}

/*
 * main overflow processing routine.
 * it can be called from the interrupt path or explicitely during the context switch code
 * Return:
 *	new value of pmc[0]. if 0x0 then unfreeze, else keep frozen
 */
static unsigned long
pfm_overflow_handler(struct task_struct *task, pfm_context_t *ctx, u64 pmc0, struct pt_regs *regs)
{
	unsigned long mask;
	struct thread_struct *t;
	unsigned long old_val;
	unsigned long ovfl_notify = 0UL, ovfl_pmds = 0UL;
	int i;
	int ret = 1;
	/*
	 * It is never safe to access the task for which the overflow interrupt is destinated
	 * using the current variable as the interrupt may occur in the middle of a context switch
	 * where current does not hold the task that is running yet.
	 *
	 * For monitoring, however, we do need to get access to the task which caused the overflow
	 * to account for overflow on the counters.
	 *
	 * We accomplish this by maintaining a current owner of the PMU per CPU. During context
	 * switch the ownership is changed in a way such that the reflected owner is always the
	 * valid one, i.e. the one that caused the interrupt.
	 */

	t   = &task->thread;

	/*
	 * XXX: debug test
	 * Don't think this could happen given upfront tests
	 */
	if ((t->flags & IA64_THREAD_PM_VALID) == 0 && ctx->ctx_fl_system == 0) {
		printk(KERN_DEBUG "perfmon: Spurious overflow interrupt: process %d not "
		       "using perfmon\n", task->pid);
		return 0x1;
	}
	/*
	 * sanity test. Should never happen
	 */
	if ((pmc0 & 0x1) == 0) {
		printk(KERN_DEBUG "perfmon: pid %d pmc0=0x%lx assumption error for freeze bit\n",
		       task->pid, pmc0);
		return 0x0;
	}

	mask = pmc0 >> PMU_FIRST_COUNTER;

	DBprintk_ovfl(("pmc0=0x%lx pid=%d iip=0x%lx, %s"
		  " mode used_pmds=0x%lx used_pmcs=0x%lx reload_pmcs=0x%lx\n", 
			pmc0, task->pid, (regs ? regs->cr_iip : 0), 
			CTX_OVFL_NOBLOCK(ctx) ? "nonblocking" : "blocking",
			ctx->ctx_used_pmds[0],
			ctx->ctx_used_pmcs[0],
			ctx->ctx_reload_pmcs[0]));

	/*
	 * First we update the virtual counters
	 */
	for (i = PMU_FIRST_COUNTER; mask ; i++, mask >>= 1) {

		/* skip pmd which did not overflow */
		if ((mask & 0x1) == 0) continue;

		DBprintk_ovfl(("pmd[%d] overflowed hw_pmd=0x%lx soft_pmd=0x%lx\n", 
			  i, ia64_get_pmd(i), ctx->ctx_soft_pmds[i].val));

		/*
		 * Note that the pmd is not necessarily 0 at this point as qualified events
		 * may have happened before the PMU was frozen. The residual count is not
		 * taken into consideration here but will be with any read of the pmd via
		 * pfm_read_pmds().
		 */
		old_val                    = ctx->ctx_soft_pmds[i].val;
		ctx->ctx_soft_pmds[i].val += 1 + pmu_conf.ovfl_val;

		/*
		 * check for overflow condition
		 */
		if (old_val > ctx->ctx_soft_pmds[i].val) {

			ovfl_pmds |= 1UL << i;

			if (PMC_OVFL_NOTIFY(ctx, i)) {
				ovfl_notify |= 1UL << i;
			}
		} else {
			/*
			 * clear top bits (maintain counts in lower part, may not always be zero)
			 */
			ia64_set_pmd(i, ia64_get_pmd(i) & pmu_conf.ovfl_val);
		}
		DBprintk_ovfl(("soft_pmd[%d].val=0x%lx old_val=0x%lx pmd=0x%lx ovfl_pmds=0x%lx ovfl_notify=0x%lx\n", 
			  i, ctx->ctx_soft_pmds[i].val, old_val, 
			  ia64_get_pmd(i) & pmu_conf.ovfl_val, ovfl_pmds, ovfl_notify));
	}

	/*
	 * check for sampling buffer
	 *
	 * if present, record sample only when a 64-bit counter has overflowed.
	 * We propagate notification ONLY when buffer becomes full.
	 */
	if(CTX_HAS_SMPL(ctx) && ovfl_pmds) {
		ret = pfm_record_sample(task, ctx, ovfl_pmds, regs);
		if (ret == 1) {
			/*
			 * Sampling buffer became full
			 * If no notication was requested, then we reset buffer index
			 * and reset registers (done below) and resume.
			 * If notification requested, then defer reset until pfm_restart()
			 */
			if (ovfl_notify == 0UL) {
				ctx->ctx_psb->psb_hdr->hdr_count = 0UL;
				ctx->ctx_psb->psb_index		 = 0UL;
			}
		} else {
			/*
			 * sample recorded in buffer, no need to notify user
			 */
			ovfl_notify = 0UL;
		}
	}

	/*
	 * No overflow requiring a user level notification
	 */
	if (ovfl_notify == 0UL) {
		if (ovfl_pmds) 
			pfm_reset_regs(ctx, &ovfl_pmds, PFM_PMD_SHORT_RESET);
		return 0x0;
	}

	/* 
	 * keep track of what to reset when unblocking 
	 */
	ctx->ctx_ovfl_regs[0]  = ovfl_pmds; 

	/*
	 * As a consequence of the overflow, we always resume
	 * with monitoring turned off. pfm_restart() will
	 * reactivate.
	 */
	ctx->ctx_fl_frozen = 1;

	/*
	 * we have come to this point because there was an overflow and that notification
	 * was requested. The notify_task may have disappeared, in which case notify_task
	 * is NULL.
	 */
	LOCK_CTX(ctx);

	if (ctx->ctx_notify_task) {
		if (CTX_OVFL_NOBLOCK(ctx) == 0 && ctx->ctx_notify_task != task) {
			t->pfm_ovfl_block_reset = 1; /* will cause blocking */
		} else {
			t->pfm_ovfl_block_reset = 0;
		}

		DBprintk_ovfl(("[%d] scheduling tasklet\n", current->pid));

		/*
		 * the tasklet is responsible for sending the notification
		 * not the PMU owner nor the current task.
		 */
		tasklet_schedule(&ctx->ctx_tasklet);

	} else {
		DBprintk_ovfl(("notification task has disappeared !\n"));
		t->pfm_ovfl_block_reset = 0;
	}

	UNLOCK_CTX(ctx);

	DBprintk_ovfl(("return pmc0=0x%x must_block=%ld\n",
				ctx->ctx_fl_frozen ? 0x1 : 0x0, t->pfm_ovfl_block_reset));

	return ctx->ctx_fl_frozen ? 0x1 : 0x0;
}

static void
pfm_interrupt_handler(int irq, void *arg, struct pt_regs *regs)
{
	u64 pmc0;
	struct task_struct *task;
	pfm_context_t *ctx;

	pfm_stats[smp_processor_id()].pfm_ovfl_intr_count++;

	/*
	 * if an alternate handler is registered, just bypass the default one
	 */
	if (pfm_alternate_intr_handler) {
		(*pfm_alternate_intr_handler->handler)(irq, arg, regs);
		return;
	}

	/* 
	 * srlz.d done before arriving here
	 *
	 * This is slow
	 */
	pmc0 = ia64_get_pmc(0); 
	task = PMU_OWNER();
	/*
	 * if we have some pending bits set
	 * assumes : if any PM[0].bit[63-1] is set, then PMC[0].fr = 1
	 */
	if (PMC0_HAS_OVFL(pmc0) && task) {
		/* 
		 * we assume that pmc0.fr is always set here
		 */
		ctx = PFM_GET_CTX(task);

		/* sanity check */
		if (!ctx) {
			printk(KERN_DEBUG "perfmon: Spurious overflow interrupt: process %d has "
			       "no PFM context\n", task->pid);
			return;
		}
		/* 
		 * assume PMC[0].fr = 1 at this point 
		 */
		pmc0 = pfm_overflow_handler(task, ctx, pmc0, regs);

		/*
		 * we can only update pmc0 when the overflow
		 * is for the current context or we are in system
		 * wide mode. In UP (per-task) the current
		 * task may not be the one owning the PMU,
		 * same thing for system-wide.
		 */
		if (task == current || ctx->ctx_fl_system) {
			/*
		 	 * We always clear the overflow status bits and either unfreeze
		 	 * or keep the PMU frozen.
		 	 */
			ia64_set_pmc(0, pmc0);
			ia64_srlz_d();
		} else {
			task->thread.pmc[0] = pmc0;
		}
	} else {
		pfm_stats[smp_processor_id()].pfm_spurious_ovfl_intr_count++;
	}
}

/* for debug only */
static int
pfm_proc_info(char *page)
{
	char *p = page;
	int i;

	p += sprintf(p, "fastctxsw              : %s\n", pfm_sysctl.fastctxsw > 0 ? "Yes": "No");
	p += sprintf(p, "ovfl_mask              : 0x%lx\n", pmu_conf.ovfl_val);

	for(i=0; i < NR_CPUS; i++) {
		if (cpu_online(i) == 0) continue;
		p += sprintf(p, "CPU%-2d overflow intrs   : %lu\n", i, pfm_stats[i].pfm_ovfl_intr_count);
		p += sprintf(p, "CPU%-2d spurious intrs   : %lu\n", i, pfm_stats[i].pfm_spurious_ovfl_intr_count);
		p += sprintf(p, "CPU%-2d recorded samples : %lu\n", i, pfm_stats[i].pfm_recorded_samples_count);
		p += sprintf(p, "CPU%-2d smpl buffer full : %lu\n", i, pfm_stats[i].pfm_full_smpl_buffer_count);
		p += sprintf(p, "CPU%-2d syst_wide        : %d\n", i, cpu_data(i)->pfm_syst_info & PFM_CPUINFO_SYST_WIDE ? 1 : 0);
		p += sprintf(p, "CPU%-2d dcr_pp           : %d\n", i, cpu_data(i)->pfm_syst_info & PFM_CPUINFO_DCR_PP ? 1 : 0);
		p += sprintf(p, "CPU%-2d exclude idle     : %d\n", i, cpu_data(i)->pfm_syst_info & PFM_CPUINFO_EXCL_IDLE ? 1 : 0);
		p += sprintf(p, "CPU%-2d owner            : %d\n", i, pmu_owners[i].owner ? pmu_owners[i].owner->pid: -1);
		p += sprintf(p, "CPU%-2d activations      : %lu\n", i, pmu_owners[i].activation_number);
	}

	LOCK_PFS();

	p += sprintf(p, "proc_sessions          : %u\n"
			"sys_sessions           : %u\n"
			"sys_use_dbregs         : %u\n"
			"ptrace_use_dbregs      : %u\n", 
			pfm_sessions.pfs_task_sessions, 
			pfm_sessions.pfs_sys_sessions,
			pfm_sessions.pfs_sys_use_dbregs,
			pfm_sessions.pfs_ptrace_use_dbregs);

	UNLOCK_PFS();

	return p - page;
}

/* /proc interface, for debug only */
static int
perfmon_read_entry(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = pfm_proc_info(page);

	if (len <= off+count) *eof = 1;

	*start = page + off;
	len   -= off;

	if (len>count) len = count;
	if (len<0) len = 0;

	return len;
}

/*
 * we come here as soon as local_cpu_data->pfm_syst_wide is set. this happens
 * during pfm_enable() hence before pfm_start(). We cannot assume monitoring
 * is active or inactive based on mode. We must rely on the value in 
 * local_cpu_data->pfm_syst_info
 */
void
pfm_syst_wide_update_task(struct task_struct *task, unsigned long info, int is_ctxswin)
{
	struct pt_regs *regs;
	unsigned long dcr;
	unsigned long dcr_pp;

	dcr_pp = info & PFM_CPUINFO_DCR_PP ? 1 : 0;

	/*
	 * pid 0 is guaranteed to be the idle task. There is one such task with pid 0 
	 * on every CPU, so we can rely on the pid to identify the idle task.
	 */
	if ((info & PFM_CPUINFO_EXCL_IDLE) == 0 || task->pid) {
		regs = (struct pt_regs *)((unsigned long) task + IA64_STK_OFFSET);
		regs--;
		ia64_psr(regs)->pp = is_ctxswin ? dcr_pp : 0;
		return;
	}
	/*
	 * we are the idle task  and there is exclusion.
	 *
	 * if monitoring has started
	 */
	if (dcr_pp) {
		dcr = ia64_get_dcr();
		/* 
		 * context switching in? 
		 */
		if (is_ctxswin) {
			/* mask monitoring for the idle task */
			ia64_set_dcr(dcr & ~IA64_DCR_PP);
			pfm_clear_psr_pp();
			ia64_srlz_i();
			return;
		}
		/* 
		 * context switching out
		 * restore normal kernel level settings 
		 *
		 * Due to inlining this odd if-then-else construction generates 
		 * better code.
	         */
		ia64_set_dcr(dcr |IA64_DCR_PP);
		pfm_set_psr_pp();
		ia64_srlz_i();
	}
}

#ifdef CONFIG_SMP
void
pfm_save_regs(struct task_struct *task)
{
	pfm_context_t *ctx;
	struct thread_struct *t;
	u64 psr;

	ctx = PFM_GET_CTX(task);
	if (ctx == NULL) goto save_error;
	t = &task->thread;

	/* 
	 * sanity check 
	 */
	if (ctx->ctx_last_activation != GET_ACTIVATION()) {
		DBprintk(("ctx_activation=%lu activation=%lu: no save\n", 
			ctx->ctx_last_activation, GET_ACTIVATION()));
		return;
	}

	/*
	 * save current PSR: needed because we modify it
	 */
	psr = pfm_get_psr();

	/*
	 * stop monitoring:
	 * This is the last instruction which may generate an overflow
	 *
	 * We do not need to set psr.sp because, it is irrelevant in kernel.
	 * It will be restored from ipsr when going back to user level
	 */
	pfm_clear_psr_up();

	/*
	 * keep a copy of the saved psr (for reload)
	 */
	ctx->ctx_saved_psr = psr;

	/*
	 * release ownership of this PMU.
	 */
	SET_PMU_OWNER(NULL);

	/*
	 * we systematically save the PMD as we have no
	 * guarantee we will be schedule at that same 
	 * CPU again.
	 */
	pfm_save_pmds(t->pmd, ctx->ctx_used_pmds[0]);

	/* 
	 * save pmc0 ia64_srlz_d() done in pfm_save_pmds() 
	 * we will need it on the restore path to check
	 * for pending overflow.
	 */
	t->pmc[0] = ia64_get_pmc(0);

	return;

save_error:
	printk(KERN_ERR "perfmon: pfm_save_regs CPU%d [%d] NULL context PM_VALID=%ld\n", 
		smp_processor_id(), task->pid,
		task->thread.flags & IA64_THREAD_PM_VALID);
}

#else /* !CONFIG_SMP */

void
pfm_save_regs(struct task_struct *task)
{
	pfm_context_t *ctx;
	u64 psr;

	ctx = PFM_GET_CTX(task);
	if (ctx == NULL) goto save_error;
	/*
	 * save current PSR: needed because we modify it
	 */
	psr = pfm_get_psr();

	/*
	 * stop monitoring:
	 * This is the last instruction which may generate an overflow
	 *
	 * We do not need to set psr.sp because, it is irrelevant in kernel.
	 * It will be restored from ipsr when going back to user level
	 */
	pfm_clear_psr_up();

	/*
	 * keep a copy of the saved psr (for reload)
	 */
	ctx->ctx_saved_psr = psr;

	return;
save_error:
	printk(KERN_ERR "perfmon: pfm_save_regs CPU%d [%d] NULL context PM_VALID=%ld\n", 
		smp_processor_id(), task->pid,
		task->thread.flags & IA64_THREAD_PM_VALID);
}

static unsigned long
pfm_lazy_save_regs (struct task_struct *task)
{
	pfm_context_t *ctx;
	struct thread_struct *t;

	ctx = PFM_GET_CTX(task);
	t   = &task->thread;

	DBprintk(("on [%d] used_pmds=0x%lx\n", task->pid, ctx->ctx_used_pmds[0]));

	/*
	 * release ownership of this PMU.
	 * must be done before we save the registers.
	 *
	 * after this call any PMU interrupt is treated
	 * as spurious.
	 */
	SET_PMU_OWNER(NULL);

	/*
	 * save all the pmds we use
	 */
	pfm_save_pmds(t->pmd, ctx->ctx_used_pmds[0]);

	/* 
	 * save pmc0 ia64_srlz_d() done in pfm_save_pmds() 
	 * it is needed to check for pended overflow
	 * on the restore path
	 */
	t->pmc[0] = ia64_get_pmc(0);

	return t->pmc[0];
}
#endif /* CONFIG_SMP */

#ifdef CONFIG_SMP
void
pfm_load_regs (struct task_struct *task)
{
	pfm_context_t *ctx;
	struct thread_struct *t;
	struct task_struct *owner;
	unsigned long pmc_mask = 0UL, pmd_mask = 0UL;
	u64 psr;

	ctx = PFM_GET_CTX(task);
	if (unlikely(ctx == NULL)) {
		printk(KERN_ERR "perfmon: pfm_load_regs() null context\n");
		return;
	}

	owner = PMU_OWNER();
	t     = &task->thread;

	/*
	 * possible on unload
	 */
	if ((t->flags & IA64_THREAD_PM_VALID) == 0) {
		DBprintk(("[%d] PM_VALID=0, nothing to do\n", task->pid));
		return;
	}

	/*
	 * we restore ALL the debug registers to avoid picking up 
	 * stale state.
	 *
	 * This must be done even when the task is still the owner
	 * as the registers may have been modified via ptrace()
	 * (not perfmon) by the previous task. 
	 */
	if (ctx->ctx_fl_using_dbreg) {
		pfm_restore_ibrs(t->ibr, pmu_conf.num_ibrs);
		pfm_restore_dbrs(t->dbr, pmu_conf.num_dbrs);
	}

	/*
	 * retrieve saved psr
	 */
	psr = ctx->ctx_saved_psr;

	/*
	 * if we were the last user of the PMU on that CPU, 
	 * then nothing to do except restore psr
	 */
	if (GET_LAST_CPU(ctx) == smp_processor_id() && ctx->ctx_last_activation == GET_ACTIVATION()) {
		/*
		 * retrieve partial reload masks (due to user modifications)
		 */
		pmc_mask = 0UL;
		pmd_mask = 0UL;

		if (pmc_mask || pmd_mask) DBprintk(("partial reload [%d] pmd_mask=0x%lx pmc_mask=0x%lx\n", task->pid, pmd_mask, pmc_mask));
	} else {
		/*
	 	 * To avoid leaking information to the user level when psr.sp=0,
	 	 * we must reload ALL implemented pmds (even the ones we don't use).
	 	 * In the kernel we only allow PFM_READ_PMDS on registers which
	 	 * we initialized or requested (sampling) so there is no risk there.
	 	 */
		pmd_mask = pfm_sysctl.fastctxsw ?  ctx->ctx_used_pmds[0] : ctx->ctx_reload_pmds[0];

		/* 
	 	 * ALL accessible PMCs are systematically reloaded, unused registers
	 	 * get their default (from pfm_reset_pmu_state()) values to avoid picking 
	 	 * up stale configuration.
	 	 *
	 	 * PMC0 is never in the mask. It is always restored separately.  
	 	 */	
		pmc_mask = ctx->ctx_reload_pmcs[0];

		DBprintk(("full reload for [%d] owner=%d activation=%lu last_activation=%lu last_cpu=%d pmd_mask=0x%lx pmc_mask=0x%lx\n", 
			task->pid, owner ? owner->pid : -1, 
			GET_ACTIVATION(), ctx->ctx_last_activation,
			GET_LAST_CPU(ctx), pmd_mask, pmc_mask));

	}
	
	if (pmd_mask) pfm_restore_pmds(t->pmd, pmd_mask);
	if (pmc_mask) pfm_restore_pmcs(t->pmc, pmc_mask);

	/*
	 * check for pending overflow at the time the state
	 * was saved.
	 */
	if (PMC0_HAS_OVFL(t->pmc[0])) {
		struct pt_regs *regs = TASK_PTREGS(task);
		pfm_overflow_handler(task, ctx, t->pmc[0], regs);
	}

	/*
	 * fl_frozen==1 when we are in blocking mode waiting for restart
	 */
	if (ctx->ctx_fl_frozen == 0) {
		pfm_unfreeze_pmu();
	}

	SET_LAST_CPU(ctx, smp_processor_id());
	
	/*
	 * dump activation value for this PMU
	 */
	INC_ACTIVATION();
	/*
	 * record current activation for this context
	 */
	SET_ACTIVATION(ctx);

	/*
	 * establish new ownership. Interrupts
	 * are still masked at this point.
	 */
	SET_PMU_OWNER(task);

	/*
	 * restore the psr we changed
	 */
	pfm_set_psr_l(psr);

}
#else /*  !CONFIG_SMP */
/*
 * reload PMU state for UP kernels
 */
void
pfm_load_regs (struct task_struct *task)
{
	struct thread_struct *t;
	pfm_context_t *ctx;
	struct task_struct *owner;
	unsigned long pmd_mask, pmc_mask;
	unsigned long prev_pmc0 = ~0UL;
	u64 psr;

	owner      = PMU_OWNER();
	ctx        = PFM_GET_CTX(task);
	t          = &task->thread;

	/*
	 * we restore ALL the debug registers to avoid picking up 
	 * stale state.
	 *
	 * This must be done even when the task is still the owner
	 * as the registers may have been modified via ptrace()
	 * (not perfmon) by the previous task. 
	 */
	if (ctx->ctx_fl_using_dbreg) {
		pfm_restore_ibrs(t->ibr, pmu_conf.num_ibrs);
		pfm_restore_dbrs(t->dbr, pmu_conf.num_dbrs);
	}

	/*
	 * retrieved save psr
	 */
	psr = ctx->ctx_saved_psr;

	/*
	 * short path, our state is still there, just
	 * need to restore psr and we go
	 *
	 * we do not touch either PMC nor PMD. the psr is not touched
	 * by the overflow_handler. So we are safe w.r.t. to interrupt
	 * concurrency even without interrupt masking.
	 */
	if (owner == task) {
		pfm_set_psr_l(psr);
		return;
	}

	DBprintk(("reload for [%d] owner=%d\n", task->pid, owner ? owner->pid : -1));

	/*
	 * someone else is still using the PMU, first push it out and
	 * then we'll be able to install our stuff !
	 *
	 * Upon return, there will be no owner for the current PMU
	 */
	if (owner) prev_pmc0 = pfm_lazy_save_regs(owner);
	/*
	 * To avoid leaking information to the user level when psr.sp=0,
	 * we must reload ALL implemented pmds (even the ones we don't use).
	 * In the kernel we only allow PFM_READ_PMDS on registers which
	 * we initialized or requested (sampling) so there is no risk there.
	 */
	pmd_mask = pfm_sysctl.fastctxsw ?  ctx->ctx_used_pmds[0] : ctx->ctx_reload_pmds[0];
	
	/* 
	 * ALL accessible PMCs are systematically reloaded, unused registers
	 * get their default (from pfm_reset_pmu_state()) values to avoid picking 
	 * up stale configuration.
	 *
	 * PMC0 is never in the mask. It is always restored separately.  
	 */	
	pmc_mask = ctx->ctx_reload_pmcs[0];

	pfm_restore_pmds(t->pmd, pmd_mask);
	pfm_restore_pmcs(t->pmc, pmc_mask);

	/*
	 * Check for pending overflow when state was last saved.
	 * invoked handler is overflow status bits set.
	 *
	 * Any PMU overflow in flight at this point, will still
	 * be treated as spurious because we have no declared
	 * owner. Note that the first level interrupt handler 
	 * DOES NOT TOUCH any PMC except PMC0 for which we have 
	 * a copy already.
	 */
	if (PMC0_HAS_OVFL(t->pmc[0])) {
		struct pt_regs *regs = TASK_PTREGS(task);
		pfm_overflow_handler(task, ctx, t->pmc[0], regs);
	}



	/*
	 * fl_frozen==1 when we are in blocking mode waiting for restart
	 */
	if (ctx->ctx_fl_frozen == 0) {
		pfm_unfreeze_pmu();
	} else if (prev_pmc0 == 0UL && ctx->ctx_fl_frozen) {
		/*
		 * owner is still NULL at this point.
		 *
		 * if the previous owner (from lazy_save_regs())
		 * was not in frozen state, then we need to freeze
		 * the PMU if the new context is frozen.
		 *
		 * on McKinley this will generate a spurious interrupt
		 * but we have no other way.
		 */
		pfm_freeze_pmu();
	}

	/*
	 * establish new ownership. If there was an in-flight
	 * overflow interrupt, it will be treated as spurious 
	 * before and after the call, because no overflow
	 * status bit can possibly be set. No new overflow
	 * can be generated because, at this point, psr.up
	 * is still cleared.
	 */
	SET_PMU_OWNER(task);

	/*
	 * restore the psr. This is the point at which 
	 * new overflow interrupts can be generated again.
	 */
	pfm_set_psr_l(psr);
}
#endif /* CONFIG_SMP */

/*
 * XXX: make this routine able to work with non current context
 */
static void
pfm_reset_pmu(struct task_struct *task)
{
	struct thread_struct *t = &task->thread;
	pfm_context_t *ctx = t->pfm_context;
	int i;

	if (task != current) {
		printk("perfmon: invalid task in pfm_reset_pmu()\n");
		return;
	}

	/* Let's make sure the PMU is frozen */
	pfm_freeze_pmu();

	/*
	 * install reset values for PMC. We skip PMC0 (done above)
	 * XX: good up to 64 PMCS
	 */
	for (i=1; (pmu_conf.pmc_desc[i].type & PFM_REG_END) == 0; i++) {
		if ((pmu_conf.pmc_desc[i].type & PFM_REG_IMPL) == 0) continue;
		ia64_set_pmc(i, PMC_DFL_VAL(i));
		/*
		 * When restoring context, we must restore ALL pmcs, even the ones 
		 * that the task does not use to avoid leaks and possibly corruption
		 * of the sesion because of configuration conflicts. So here, we 
		 * initialize the entire set used in the context switch restore routine.
	 	 */
		t->pmc[i] = PMC_DFL_VAL(i);
		DBprintk(("pmc[%d]=0x%lx\n", i, t->pmc[i]));
	}

	/*
	 * clear reset values for PMD. 
	 * XXX: good up to 64 PMDS.
	 */
	for (i=0; (pmu_conf.pmd_desc[i].type & PFM_REG_END) == 0; i++) {
		if ((pmu_conf.pmd_desc[i].type & PFM_REG_IMPL) == 0) continue;
		ia64_set_pmd(i, 0UL);
		t->pmd[i] = 0UL;
	}

	/*
	 * On context switched restore, we must restore ALL pmc and ALL pmd even
	 * when they are not actively used by the task. In UP, the incoming process 
	 * may otherwise pick up left over PMC, PMD state from the previous process.
	 * As opposed to PMD, stale PMC can cause harm to the incoming
	 * process because they may change what is being measured. 
	 * Therefore, we must systematically reinstall the entire
	 * PMC state. In SMP, the same thing is possible on the 
	 * same CPU but also on between 2 CPUs. 
	 *
	 * The problem with PMD is information leaking especially
	 * to user level when psr.sp=0
	 *
	 * There is unfortunately no easy way to avoid this problem
	 * on either UP or SMP. This definitively slows down the
	 * pfm_load_regs() function. 
	 */
	
	 /*
	  * We must include all the PMC in this mask to make sure we don't
	  * see any side effect of a stale state, such as opcode matching
	  * or range restrictions, for instance.
	  *
	  * We never directly restore PMC0 so we do not include it in the mask.
	  */
	ctx->ctx_reload_pmcs[0] = pmu_conf.impl_pmcs[0] & ~0x1;
	/*
	 * We must include all the PMD in this mask to avoid picking
	 * up stale value and leak information, especially directly
	 * at the user level when psr.sp=0
	 */
	ctx->ctx_reload_pmds[0] = pmu_conf.impl_pmds[0];

	/* 
	 * Keep track of the pmds we want to sample
	 * XXX: may be we don't need to save/restore the DEAR/IEAR pmds
	 * but we do need the BTB for sure. This is because of a hardware
	 * buffer of 1 only for non-BTB pmds.
	 *
	 * We ignore the unimplemented pmds specified by the user
	 */
	ctx->ctx_used_pmds[0] = ctx->ctx_smpl_regs[0];
	ctx->ctx_used_pmcs[0] = 1; /* always save/restore PMC[0] */

	/*
	 * useful in case of re-enable after disable
	 */
	ctx->ctx_used_ibrs[0] = 0UL;
	ctx->ctx_used_dbrs[0] = 0UL;

	ia64_srlz_d();
}

/*
 * This function is called when a thread exits (from exit_thread()).
 * This is a simplified pfm_save_regs() that simply flushes the current
 * register state into the save area taking into account any pending
 * overflow. This time no notification is sent because the task is dying
 * anyway. The inline processing of overflows avoids loosing some counts.
 * The PMU is frozen on exit from this call and is to never be reenabled
 * again for this task.
 *
 */
void
pfm_flush_regs (struct task_struct *task)
{
	pfm_context_t *ctx;
	u64 pmc0;
	unsigned long mask2, val;
	int i;

	ctx = task->thread.pfm_context;

	if (ctx == NULL) return;

	/* 
	 * that's it if context already disabled
	 */
	if (ctx->ctx_flags.state == PFM_CTX_DISABLED) return;

	/*
	 * stop monitoring:
	 * This is the only way to stop monitoring without destroying overflow
	 * information in PMC[0].
	 * This is the last instruction which can cause overflow when monitoring
	 * in kernel.
	 * By now, we could still have an overflow interrupt in-flight.
	 */
	if (ctx->ctx_fl_system) {

		/* disable dcr pp */
		ia64_set_dcr(ia64_get_dcr() & ~IA64_DCR_PP);

		/* stop monitoring */
		pfm_clear_psr_pp();
		ia64_srlz_i();

		PFM_CPUINFO_CLEAR(PFM_CPUINFO_SYST_WIDE);
		PFM_CPUINFO_CLEAR(PFM_CPUINFO_DCR_PP);
		PFM_CPUINFO_CLEAR(PFM_CPUINFO_EXCL_IDLE);
	} else  {

		/* stop monitoring */
		pfm_clear_psr_up();
		ia64_srlz_i();

		/* no more save/restore on ctxsw */
		current->thread.flags &= ~IA64_THREAD_PM_VALID;
	}

	/*
	 * Mark the PMU as not owned
	 * This will cause the interrupt handler to do nothing in case an overflow
	 * interrupt was in-flight
	 * This also guarantees that pmc0 will contain the final state
	 * It virtually gives us full control on overflow processing from that point
	 * on.
	 * It must be an atomic operation.
	 */
	SET_PMU_OWNER(NULL);

	/*
	 * read current overflow status:
	 *
	 * we are guaranteed to read the final stable state
	 */
	ia64_srlz_d();
	pmc0 = ia64_get_pmc(0); /* slow */

	/*
	 * freeze PMU:
	 *
	 * This destroys the overflow information. This is required to make sure
	 * next process does not start with monitoring on if not requested
	 */
	pfm_freeze_pmu();

	/*
	 * We don't need to restore psr, because we are on our way out
	 */

	/*
	 * This loop flushes the PMD into the PFM context.
	 * It also processes overflow inline.
	 *
	 * IMPORTANT: No notification is sent at this point as the process is dying.
	 * The implicit notification will come from a SIGCHILD or a return from a
	 * waitpid().
	 *
	 */
#ifdef CONFIG_SMP
	if (GET_LAST_CPU(ctx) != smp_processor_id())
		printk(KERN_DEBUG "perfmon: [%d] last_cpu=%d\n",
		       task->pid, GET_LAST_CPU(ctx));
#endif

	/*
	 * we save all the used pmds
	 * we take care of overflows for pmds used as counters
	 */
	mask2 = ctx->ctx_used_pmds[0];
	for (i = 0; mask2; i++, mask2>>=1) {

		/* skip non used pmds */
		if ((mask2 & 0x1) == 0) continue;

		val = ia64_get_pmd(i);

		if (PMD_IS_COUNTING(i)) {
			DBprintk(("[%d] pmd[%d] soft_pmd=0x%lx hw_pmd=0x%lx\n", 
				task->pid, 
				i, 
				ctx->ctx_soft_pmds[i].val, 
				val & pmu_conf.ovfl_val));

			/* collect latest results */
			ctx->ctx_soft_pmds[i].val += val & pmu_conf.ovfl_val;

			/*
			 * now everything is in ctx_soft_pmds[] and we need
			 * to clear the saved context from save_regs() such that
			 * pfm_read_pmds() gets the correct value
			 */
			task->thread.pmd[i] = 0;

			/* 
			 * take care of overflow inline
			 */
			if (pmc0 & (1UL << i)) {
				ctx->ctx_soft_pmds[i].val += 1 + pmu_conf.ovfl_val;
				DBprintk(("[%d] pmd[%d] overflowed soft_pmd=0x%lx\n",
					task->pid, i, ctx->ctx_soft_pmds[i].val));
			}
		} else {
			DBprintk(("[%d] pmd[%d] hw_pmd=0x%lx\n", task->pid, i, val));
			/* 
			 * not a counter, just save value as is
			 */
			task->thread.pmd[i] = val;
		}
	}
	SET_LAST_CPU(ctx, -1);
}


/*
 * task is the newly created task, pt_regs for new child
 */
int
pfm_inherit(struct task_struct *task, struct pt_regs *regs)
{
	pfm_context_t *ctx;
	pfm_context_t *nctx;
	struct thread_struct *thread;
	unsigned long m;
	int i;

	/*
	 * the new task was copied from parent and therefore points
	 * to the parent's context at this point
	 */
	ctx    = task->thread.pfm_context;
	thread = &task->thread;

	/*
	 * for secure sessions, make sure child cannot mess up 
	 * the monitoring session.
	 */
	if (ctx->ctx_fl_unsecure == 0) {
		ia64_psr(regs)->sp = 1;
	 	DBprintk(("enabling psr.sp for [%d]\n", task->pid));
	} else {
	 	DBprintk(("psr.sp=%d [%d]\n", ia64_psr(regs)->sp, task->pid));
	}


	/*
	 * if there was a virtual mapping for the sampling buffer
	 * the mapping is NOT inherited across fork() (see VM_DONTCOPY), 
	 * so we don't have to explicitely remove it here. 
	 *
	 *
	 * Part of the clearing of fields is also done in
	 * copy_thread() because the fiels are outside the
	 * pfm_context structure and can affect tasks not
	 * using perfmon.
	 */

	/* clear pending notification */
	task->thread.pfm_ovfl_block_reset = 0;

	/*
	 * clear cpu pinning restriction for child
	 */
	if (ctx->ctx_fl_system) {
		task->cpus_allowed = ctx->ctx_saved_cpus_allowed;
		task->need_resched = 1;

	 	DBprintk(("setting cpus_allowed for [%d] to 0x%lx from 0x%lx\n", 
			task->pid,
			ctx->ctx_saved_cpus_allowed, 
			current->cpus_allowed));
	}

	/*
	 * takes care of easiest case first
	 */
	if (CTX_INHERIT_MODE(ctx) == PFM_FL_INHERIT_NONE) {

		DBprintk(("removing PFM context for [%d]\n", task->pid));

		task->thread.pfm_context = NULL;

		/* 
		 * we must clear psr.up because the new child does
		 * not have a context and the PM_VALID flag is cleared
		 * in copy_thread().
		 *
		 * we do not clear psr.pp because it is always
		 * controlled by the system wide logic and we should
		 * never be here when system wide is running anyway
		 */
	 	ia64_psr(regs)->up = 0;

		/* copy_thread() clears IA64_THREAD_PM_VALID */
		return 0;
	}
	nctx = pfm_context_alloc();
	if (nctx == NULL) return -ENOMEM;

	/* copy content */
	*nctx = *ctx;

	if (CTX_INHERIT_MODE(ctx) == PFM_FL_INHERIT_ONCE) {
		nctx->ctx_fl_inherit = PFM_FL_INHERIT_NONE;
		DBprintk(("downgrading to INHERIT_NONE for [%d]\n", task->pid));
		/*
		 * downgrade parent: once means only first child!
		 */
		ctx->ctx_fl_inherit = PFM_FL_INHERIT_NONE;
	}
	/*
	 * task is not yet visible in the tasklist, so we do 
	 * not need to lock the newly created context.
	 * However, we must grab the tasklist_lock to ensure
	 * that the ctx_owner or ctx_notify_task do not disappear
	 * while we increment their check counters.
	 */
	read_lock(&tasklist_lock);

	if (nctx->ctx_notify_task) 
		atomic_inc(&nctx->ctx_notify_task->thread.pfm_notifiers_check);

	if (nctx->ctx_owner)
		atomic_inc(&nctx->ctx_owner->thread.pfm_owners_check);

	read_unlock(&tasklist_lock);


	LOCK_PFS();
	pfm_sessions.pfs_task_sessions++;
	UNLOCK_PFS();

	/* initialize counters in new context */
	m = nctx->ctx_used_pmds[0] >> PMU_FIRST_COUNTER;
	for(i = PMU_FIRST_COUNTER ; m ; m>>=1, i++) {
		if ((m & 0x1) && pmu_conf.pmd_desc[i].type == PFM_REG_COUNTING) {
			nctx->ctx_soft_pmds[i].val = nctx->ctx_soft_pmds[i].lval & ~pmu_conf.ovfl_val;
			thread->pmd[i]	      	   = nctx->ctx_soft_pmds[i].lval & pmu_conf.ovfl_val;
		} else {
			thread->pmd[i]	      	   = 0UL; /* reset to initial state */
		}
	}

	nctx->ctx_fl_frozen    = 0;
	nctx->ctx_ovfl_regs[0] = 0UL;
	SET_LAST_CPU(nctx, -1);

	/*
	 * here nctx->ctx_psb == ctx->ctx_psb
	 *
	 * increment reference count to sampling
	 * buffer, if any. Note that this is independent
	 * from the virtual mapping. The latter is never
	 * inherited while the former will be if context
	 * is setup to something different from PFM_FL_INHERIT_NONE
	 */
	if (nctx->ctx_psb) {
		LOCK_PSB(nctx->ctx_psb);

		nctx->ctx_psb->psb_refcnt++;

	 	DBprintk(("updated smpl @ %p refcnt=%lu psb_flags=0x%x\n", 
			ctx->ctx_psb->psb_hdr,
			ctx->ctx_psb->psb_refcnt,
			ctx->ctx_psb->psb_flags));

		UNLOCK_PSB(nctx->ctx_psb);

		/*
	 	 * remove any pointer to sampling buffer mapping
	 	 */
		nctx->ctx_smpl_vaddr = 0;
	}

	sema_init(&nctx->ctx_restart_sem, 0); /* reset this semaphore to locked */

	/*
	 * propagate kernel psr in new context (used for first ctxsw in
	 */
	nctx->ctx_saved_psr = pfm_get_psr();

	/*
	 * force a full reload on ctxsw in
	 */
	nctx->ctx_last_activation = PFM_INVALID_ACTIVATION;
	SET_LAST_CPU(nctx, -1);

	/*
	 * initialize tasklet for signal notifications
	 *
	 * ALL signal-based (or any notification using data structures
	 * external to perfmon) MUST use tasklets to avoid lock contentions
	 * when a signal has to be sent for overflow interrupt handler.
	 */
	tasklet_init(&nctx->ctx_tasklet, pfm_send_notification_signal, (unsigned long)nctx);

	/* link with new task */
	thread->pfm_context = nctx;

	DBprintk(("nctx=%p for process [%d]\n", (void *)nctx, task->pid));

	/*
	 * the copy_thread routine automatically clears
	 * IA64_THREAD_PM_VALID, so we need to reenable it, if it was used by the caller
	 */
	if (current->thread.flags & IA64_THREAD_PM_VALID) {
		DBprintk(("setting PM_VALID for [%d]\n", task->pid));
		thread->flags |= IA64_THREAD_PM_VALID;
	}
	return 0;
}

/* 
 *
 * We cannot touch any of the PMU registers at this point as we may
 * not be running on the same CPU the task was last run on.  Therefore
 * it is assumed that the PMU has been stopped appropriately in
 * pfm_flush_regs() called from exit_thread(). 
 *
 * The function is called in the context of the parent via a release_thread()
 * and wait4(). The task is not in the tasklist anymore.
 */
void
pfm_context_exit(struct task_struct *task)
{
	pfm_context_t *ctx = task->thread.pfm_context;

	/*
	 * check sampling buffer
	 */
	if (ctx->ctx_psb) {
		pfm_smpl_buffer_desc_t *psb = ctx->ctx_psb;

		LOCK_PSB(psb);

		DBprintk(("sampling buffer from [%d] @%p size %ld refcnt=%lu psb_flags=0x%x\n",
			task->pid,
			psb->psb_hdr, psb->psb_size, psb->psb_refcnt, psb->psb_flags));

		/*
		 * in the case where we are the last user, we may be able to free
		 * the buffer
		 */
		psb->psb_refcnt--;

		if (psb->psb_refcnt == 0) {

			/*
			 * The flag is cleared in pfm_vm_close(). which gets 
			 * called from do_exit() via exit_mm(). 
			 * By the time we come here, the task has no more mm context.
			 *
			 * We can only free the psb and buffer here after the vm area
			 * describing the buffer has been removed. This normally happens 
			 * as part of do_exit() but the entire mm context is ONLY removed
			 * once its reference counts goes to zero. This is typically
			 * the case except for multi-threaded (several tasks) processes.
			 *
			 * See pfm_vm_close() and pfm_cleanup_smpl_buf() for more details.
			 */
			if ((psb->psb_flags & PSB_HAS_VMA) == 0) {

				DBprintk(("cleaning sampling buffer from [%d] @%p size %ld\n",
					task->pid,
					psb->psb_hdr, psb->psb_size));

				/* 
				 * free the buffer and psb 
				 */
				pfm_rvfree(psb->psb_hdr, psb->psb_size);
				kfree(psb);
				psb = NULL;
			} 
		} 
		/* psb may have been deleted */
		if (psb) UNLOCK_PSB(psb);
	} 

	DBprintk(("cleaning [%d] pfm_context @%p notify_task=%p check=%d mm=%p\n", 
		task->pid, ctx, 
		ctx->ctx_notify_task, 
		atomic_read(&task->thread.pfm_notifiers_check), task->mm));

	/*
	 * To avoid getting the notified task or owner task scan the entire process 
	 * list when they exit, we decrement notifiers_check and owners_check respectively.
	 *
	 * Of course, there is race condition between decreasing the value and the 
	 * task exiting. The danger comes from the fact that, in both cases, we have a 
	 * direct pointer to a task structure thereby bypassing the tasklist. 
	 * We must make sure that, if we have task!= NULL, the target task is still 
	 * present and is identical to the initial task specified 
	 * during pfm_context_create(). It may already be detached from the tasklist but 
	 * that's okay. Note that it is okay if we miss the deadline and the task scans 
	 * the list for nothing, it will affect performance but not correctness. 
	 * The correctness is ensured by using the ctx_lock which prevents the 
	 * notify_task from changing the fields in our context.
	 * Once holdhing this lock, if we see task!= NULL, then it will stay like
	 * that until we release the lock. If it is NULL already then we came too late.
	 */
	LOCK_CTX(ctx);

	if (ctx->ctx_notify_task != NULL) {
		DBprintk(("[%d], [%d] atomic_sub on [%d] notifiers=%u\n", current->pid,
			task->pid,
			ctx->ctx_notify_task->pid, 
			atomic_read(&ctx->ctx_notify_task->thread.pfm_notifiers_check)));

		atomic_dec(&ctx->ctx_notify_task->thread.pfm_notifiers_check);
	}

	if (ctx->ctx_owner != NULL) {
		DBprintk(("[%d], [%d] atomic_sub on [%d] owners=%u\n", 
			 current->pid, 
			 task->pid,
			 ctx->ctx_owner->pid, 
			 atomic_read(&ctx->ctx_owner->thread.pfm_owners_check)));

		atomic_dec(&ctx->ctx_owner->thread.pfm_owners_check);
	}

	UNLOCK_CTX(ctx);

	pfm_unreserve_session(task, ctx->ctx_fl_system, 1UL << ctx->ctx_cpu);

	if (ctx->ctx_fl_system) {
		/*
	 	 * remove any CPU pinning
	 	 */
		task->cpus_allowed = ctx->ctx_saved_cpus_allowed;
		task->need_resched = 1;
	} 

	pfm_context_free(ctx);
	/* 
	 *  clean pfm state in thread structure,
	 */
	task->thread.pfm_context          = NULL;
	task->thread.pfm_ovfl_block_reset = 0;

	/* pfm_notifiers is cleaned in pfm_cleanup_notifiers() */
}

/*
 * function invoked from release_thread when pfm_smpl_buf_list is not NULL
 */
int
pfm_cleanup_smpl_buf(struct task_struct *task)
{
	pfm_smpl_buffer_desc_t *tmp, *psb = task->thread.pfm_smpl_buf_list;

	if (psb == NULL) {
		printk(KERN_DEBUG "perfmon: psb is null in [%d]\n", current->pid);
		return -1;
	}
	/*
	 * Walk through the list and free the sampling buffer and psb
	 */
	while (psb) {
		DBprintk(("[%d] freeing smpl @%p size %ld\n", current->pid, psb->psb_hdr, psb->psb_size));

		pfm_rvfree(psb->psb_hdr, psb->psb_size);
		tmp = psb->psb_next;
		kfree(psb);
		psb = tmp;
	}

	/* just in case */
	task->thread.pfm_smpl_buf_list = NULL;

	return 0;
}

/*
 * function invoked from release_thread to make sure that the ctx_owner field does not
 * point to an unexisting task.
 */
void
pfm_cleanup_owners(struct task_struct *task)
{
	struct task_struct *p;
	pfm_context_t *ctx;

	DBprintk(("called by [%d] for [%d]\n", current->pid, task->pid));

	read_lock(&tasklist_lock);

	for_each_task(p) {
		/*
		 * It is safe to do the 2-step test here, because thread.ctx
		 * is cleaned up only in release_thread() and at that point
		 * the task has been detached from the tasklist which is an
		 * operation which uses the write_lock() on the tasklist_lock
		 * so it cannot run concurrently to this loop. So we have the
		 * guarantee that if we find p and it has a perfmon ctx then
		 * it is going to stay like this for the entire execution of this
		 * loop.
		 */
		ctx = p->thread.pfm_context;

		//DBprintk(("[%d] scanning task [%d] ctx=%p\n", task->pid, p->pid, ctx));

		if (ctx && ctx->ctx_owner == task) {
			DBprintk(("trying for owner [%d] in [%d]\n", task->pid, p->pid));
			/*
			 * the spinlock is required to take care of a race condition
			 * with the send_sig_info() call. We must make sure that 
			 * either the send_sig_info() completes using a valid task,
			 * or the notify_task is cleared before the send_sig_info()
			 * can pick up a stale value. Note that by the time this
			 * function is executed the 'task' is already detached from the
			 * tasklist. The problem is that the notifiers have a direct
			 * pointer to it. It is okay to send a signal to a task in this
			 * stage, it simply will have no effect. But it is better than sending
			 * to a completely destroyed task or worse to a new task using the same
			 * task_struct address.
			 */
			LOCK_CTX(ctx);

			ctx->ctx_owner = NULL;

			UNLOCK_CTX(ctx);

			DBprintk(("done for notifier [%d] in [%d]\n", task->pid, p->pid));
		}
	}
	read_unlock(&tasklist_lock);

	atomic_set(&task->thread.pfm_owners_check, 0);
}


/*
 * function called from release_thread to make sure that the ctx_notify_task is not pointing
 * to an unexisting task
 */
void
pfm_cleanup_notifiers(struct task_struct *task)
{
	struct task_struct *p;
	pfm_context_t *ctx;

	DBprintk(("called by [%d] for [%d]\n", current->pid, task->pid));

	read_lock(&tasklist_lock);

	for_each_task(p) {
		/*
		 * It is safe to do the 2-step test here, because thread.ctx
		 * is cleaned up only in release_thread() and at that point
		 * the task has been detached from the tasklist which is an
		 * operation which uses the write_lock() on the tasklist_lock
		 * so it cannot run concurrently to this loop. So we have the
		 * guarantee that if we find p and it has a perfmon ctx then
		 * it is going to stay like this for the entire execution of this
		 * loop.
		 */
		ctx = p->thread.pfm_context;

		//DBprintk(("[%d] scanning task [%d] ctx=%p\n", task->pid, p->pid, ctx));

		if (ctx && ctx->ctx_notify_task == task) {
			DBprintk(("trying for notifier [%d] in [%d]\n", task->pid, p->pid));
			/*
			 * the spinlock is required to take care of a race condition
			 * with the send_sig_info() call. We must make sure that 
			 * either the send_sig_info() completes using a valid task,
			 * or the notify_task is cleared before the send_sig_info()
			 * can pick up a stale value. Note that by the time this
			 * function is executed the 'task' is already detached from the
			 * tasklist. The problem is that the notifiers have a direct
			 * pointer to it. It is okay to send a signal to a task in this
			 * stage, it simply will have no effect. But it is better than sending
			 * to a completely destroyed task or worse to a new task using the same
			 * task_struct address.
			 */
			LOCK_CTX(ctx);

			ctx->ctx_notify_task = NULL;

			UNLOCK_CTX(ctx);

			DBprintk(("done for notifier [%d] in [%d]\n", task->pid, p->pid));
		}
	}
	read_unlock(&tasklist_lock);

	atomic_set(&task->thread.pfm_notifiers_check, 0);
}

static struct irqaction perfmon_irqaction = {
	.handler = pfm_interrupt_handler,
	.flags   = SA_INTERRUPT,
	.name    = "perfmon"
};

int
pfm_install_alternate_syswide_subsystem(pfm_intr_handler_desc_t *hdl)
{
	int ret;

	/* some sanity checks */
	if (hdl == NULL || hdl->handler == NULL) return -EINVAL;

	/* do the easy test first */
	if (pfm_alternate_intr_handler) return -EBUSY;

	/* reserve our session */
	ret = pfm_reserve_session(NULL, 1, cpu_online_map);
	if (ret) return ret;

	if (pfm_alternate_intr_handler) {
		printk(KERN_DEBUG "perfmon: install_alternate, intr_handler not NULL "
		       "after reserve\n");
		return -EINVAL;
	}

	pfm_alternate_intr_handler = hdl;

	return 0;
}

int
pfm_remove_alternate_syswide_subsystem(pfm_intr_handler_desc_t *hdl)
{
	if (hdl == NULL) return -EINVAL;

	/* cannot remove someone else's handler! */
	if (pfm_alternate_intr_handler != hdl) return -EINVAL;

	pfm_alternate_intr_handler = NULL;

	/* 
	 * XXX: assume cpu_online_map has not changed since reservation 
	 */
	pfm_unreserve_session(NULL, 1, cpu_online_map);

	return 0;
}

/*
 * perfmon initialization routine, called from the initcall() table
 */
int __init
pfm_init(void)
{
	unsigned int n, n_counters, i;

	pmu_conf.disabled = 1;

	printk(KERN_INFO "perfmon: version %u.%u IRQ %u\n", PFM_VERSION_MAJ, PFM_VERSION_MIN,
	       IA64_PERFMON_VECTOR);

	/*
	 * compute the number of implemented PMD/PMC from the
	 * description tables
	 */
	n = 0;
	for (i=0; PMC_IS_LAST(i) == 0;  i++) {
		if (PMC_IS_IMPL(i) == 0) continue;
		pmu_conf.impl_pmcs[i>>6] |= 1UL << (i&63);
		n++;
	}
	pmu_conf.num_pmcs = n;

	n = 0; n_counters = 0;
	for (i=0; PMD_IS_LAST(i) == 0;  i++) {
		if (PMD_IS_IMPL(i) == 0) continue;
		pmu_conf.impl_pmds[i>>6] |= 1UL << (i&63);
		n++;
		if (PMD_IS_COUNTING(i)) n_counters++;
	}
	pmu_conf.num_pmds      = n;
	pmu_conf.num_counters  = n_counters;

	printk(KERN_INFO "perfmon: %u PMCs, %u PMDs, %u counters (%lu bits)\n",
	       pmu_conf.num_pmcs,
	       pmu_conf.num_pmds,
	       pmu_conf.num_counters,
	       ffz(pmu_conf.ovfl_val));

	/* sanity check */
	if (pmu_conf.num_pmds >= IA64_NUM_PMD_REGS || pmu_conf.num_pmcs >= IA64_NUM_PMC_REGS) {
		printk(KERN_ERR "perfmon: not enough pmc/pmd, perfmon disabled\n");
		return -1;
	}

	/*
	 * for now here for debug purposes
	 */
	perfmon_dir = create_proc_read_entry ("perfmon", 0, 0, perfmon_read_entry, NULL);
	if (perfmon_dir == NULL) {
		printk(KERN_ERR "perfmon: cannot create /proc entry, perfmon disabled\n");
		return -1;
	}

	/*
	 * create /proc/sys/kernel/perfmon
	 */
	pfm_sysctl_header = register_sysctl_table(pfm_sysctl_root, 0);

	/*
	 * initialize all our spinlocks
	 */
	spin_lock_init(&pfm_sessions.pfs_lock);

	/* we are all set */
	pmu_conf.disabled = 0;

	return 0;
}

__initcall(pfm_init);

void
pfm_init_percpu(void)
{
	int i;

	if (smp_processor_id() == 0)
		register_percpu_irq(IA64_PERFMON_VECTOR, &perfmon_irqaction);

	ia64_set_pmv(IA64_PERFMON_VECTOR);
	ia64_srlz_d();

	/*
	 * we first initialize the PMU to a stable state.
	 * the values may have been changed from their power-up
	 * values by software executed before the kernel took over.
	 *
	 * At this point, pmu_conf has not yet been initialized
	 *
	 * On McKinley, this code is ineffective until PMC4 is initialized.
	 */
	for (i=1; PMC_IS_LAST(i) == 0;  i++) {
		if (PMC_IS_IMPL(i) == 0) continue;
		ia64_set_pmc(i, PMC_DFL_VAL(i));
	}

	for (i=0; PMD_IS_LAST(i); i++) {
		if (PMD_IS_IMPL(i) == 0) continue;
		ia64_set_pmd(i, 0UL);
	}
	pfm_freeze_pmu();
}

#else /* !CONFIG_PERFMON */

asmlinkage long
sys_perfmonctl (int pid, int cmd, void *req, int count, long arg5, long arg6, 
		long arg7, long arg8, long stack)
{
	return -ENOSYS;
}

#endif /* !CONFIG_PERFMON */
