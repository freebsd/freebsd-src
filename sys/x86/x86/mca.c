/*-
 * Copyright (c) 2009 Advanced Computing Technologies LLC
 * Written by: John H. Baldwin <jhb@FreeBSD.org>
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
 */

/*
 * Support for x86 machine check architecture.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef __amd64__
#define	DEV_APIC
#else
#include "opt_apic.h"
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <machine/cputypes.h>
#include <x86/mca.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

/* Modes for mca_scan() */
enum scan_mode {
	POLLED,
	MCE,
	CMCI,
};

#ifdef DEV_APIC
/*
 * State maintained for each monitored MCx bank to control the
 * corrected machine check interrupt threshold.
 */
struct cmc_state {
	int	max_threshold;
	int	last_intr;
};
#endif

struct mca_internal {
	struct mca_record rec;
	int		logged;
	STAILQ_ENTRY(mca_internal) link;
};

static MALLOC_DEFINE(M_MCA, "MCA", "Machine Check Architecture");

static int mca_count;		/* Number of records stored. */

SYSCTL_NODE(_hw, OID_AUTO, mca, CTLFLAG_RD, NULL, "Machine Check Architecture");

static int mca_enabled = 1;
TUNABLE_INT("hw.mca.enabled", &mca_enabled);
SYSCTL_INT(_hw_mca, OID_AUTO, enabled, CTLFLAG_RDTUN, &mca_enabled, 0,
    "Administrative toggle for machine check support");

static int amd10h_L1TP = 1;
TUNABLE_INT("hw.mca.amd10h_L1TP", &amd10h_L1TP);
SYSCTL_INT(_hw_mca, OID_AUTO, amd10h_L1TP, CTLFLAG_RDTUN, &amd10h_L1TP, 0,
    "Administrative toggle for logging of level one TLB parity (L1TP) errors");

int workaround_erratum383;
SYSCTL_INT(_hw_mca, OID_AUTO, erratum383, CTLFLAG_RD, &workaround_erratum383, 0,
    "Is the workaround for Erratum 383 on AMD Family 10h processors enabled?");

static STAILQ_HEAD(, mca_internal) mca_records;
static struct callout mca_timer;
static int mca_ticks = 3600;	/* Check hourly by default. */
static struct task mca_task;
static struct mtx mca_lock;

#ifdef DEV_APIC
static struct cmc_state **cmc_state;	/* Indexed by cpuid, bank */
static int cmc_banks;
static int cmc_throttle = 60;	/* Time in seconds to throttle CMCI. */
#endif

static int
sysctl_positive_int(SYSCTL_HANDLER_ARGS)
{
	int error, value;

	value = *(int *)arg1;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	if (value <= 0)
		return (EINVAL);
	*(int *)arg1 = value;
	return (0);
}

static int
sysctl_mca_records(SYSCTL_HANDLER_ARGS)
{
	int *name = (int *)arg1;
	u_int namelen = arg2;
	struct mca_record record;
	struct mca_internal *rec;
	int i;

	if (namelen != 1)
		return (EINVAL);

	if (name[0] < 0 || name[0] >= mca_count)
		return (EINVAL);

	mtx_lock_spin(&mca_lock);
	if (name[0] >= mca_count) {
		mtx_unlock_spin(&mca_lock);
		return (EINVAL);
	}
	i = 0;
	STAILQ_FOREACH(rec, &mca_records, link) {
		if (i == name[0]) {
			record = rec->rec;
			break;
		}
		i++;
	}
	mtx_unlock_spin(&mca_lock);
	return (SYSCTL_OUT(req, &record, sizeof(record)));
}

static const char *
mca_error_ttype(uint16_t mca_error)
{

	switch ((mca_error & 0x000c) >> 2) {
	case 0:
		return ("I");
	case 1:
		return ("D");
	case 2:
		return ("G");
	}
	return ("?");
}

static const char *
mca_error_level(uint16_t mca_error)
{

	switch (mca_error & 0x0003) {
	case 0:
		return ("L0");
	case 1:
		return ("L1");
	case 2:
		return ("L2");
	case 3:
		return ("LG");
	}
	return ("L?");
}

static const char *
mca_error_request(uint16_t mca_error)
{

	switch ((mca_error & 0x00f0) >> 4) {
	case 0x0:
		return ("ERR");
	case 0x1:
		return ("RD");
	case 0x2:
		return ("WR");
	case 0x3:
		return ("DRD");
	case 0x4:
		return ("DWR");
	case 0x5:
		return ("IRD");
	case 0x6:
		return ("PREFETCH");
	case 0x7:
		return ("EVICT");
	case 0x8:
		return ("SNOOP");
	}
	return ("???");
}

static const char *
mca_error_mmtype(uint16_t mca_error)
{

	switch ((mca_error & 0x70) >> 4) {
	case 0x0:
		return ("GEN");
	case 0x1:
		return ("RD");
	case 0x2:
		return ("WR");
	case 0x3:
		return ("AC");
	case 0x4:
		return ("MS");
	}
	return ("???");
}

/* Dump details about a single machine check. */
static void __nonnull(1)
mca_log(const struct mca_record *rec)
{
	uint16_t mca_error;

	printf("MCA: Bank %d, Status 0x%016llx\n", rec->mr_bank,
	    (long long)rec->mr_status);
	printf("MCA: Global Cap 0x%016llx, Status 0x%016llx\n",
	    (long long)rec->mr_mcg_cap, (long long)rec->mr_mcg_status);
	printf("MCA: Vendor \"%s\", ID 0x%x, APIC ID %d\n", cpu_vendor,
	    rec->mr_cpu_id, rec->mr_apic_id);
	printf("MCA: CPU %d ", rec->mr_cpu);
	if (rec->mr_status & MC_STATUS_UC)
		printf("UNCOR ");
	else {
		printf("COR ");
		if (rec->mr_mcg_cap & MCG_CAP_CMCI_P)
			printf("(%lld) ", ((long long)rec->mr_status &
			    MC_STATUS_COR_COUNT) >> 38);
	}
	if (rec->mr_status & MC_STATUS_PCC)
		printf("PCC ");
	if (rec->mr_status & MC_STATUS_OVER)
		printf("OVER ");
	mca_error = rec->mr_status & MC_STATUS_MCA_ERROR;
	switch (mca_error) {
		/* Simple error codes. */
	case 0x0000:
		printf("no error");
		break;
	case 0x0001:
		printf("unclassified error");
		break;
	case 0x0002:
		printf("ucode ROM parity error");
		break;
	case 0x0003:
		printf("external error");
		break;
	case 0x0004:
		printf("FRC error");
		break;
	case 0x0005:
		printf("internal parity error");
		break;
	case 0x0400:
		printf("internal timer error");
		break;
	default:
		if ((mca_error & 0xfc00) == 0x0400) {
			printf("internal error %x", mca_error & 0x03ff);
			break;
		}

		/* Compound error codes. */

		/* Memory hierarchy error. */
		if ((mca_error & 0xeffc) == 0x000c) {
			printf("%s memory error", mca_error_level(mca_error));
			break;
		}

		/* TLB error. */
		if ((mca_error & 0xeff0) == 0x0010) {
			printf("%sTLB %s error", mca_error_ttype(mca_error),
			    mca_error_level(mca_error));
			break;
		}

		/* Memory controller error. */
		if ((mca_error & 0xef80) == 0x0080) {
			printf("%s channel ", mca_error_mmtype(mca_error));
			if ((mca_error & 0x000f) != 0x000f)
				printf("%d", mca_error & 0x000f);
			else
				printf("??");
			printf(" memory error");
			break;
		}
		
		/* Cache error. */
		if ((mca_error & 0xef00) == 0x0100) {
			printf("%sCACHE %s %s error",
			    mca_error_ttype(mca_error),
			    mca_error_level(mca_error),
			    mca_error_request(mca_error));
			break;
		}

		/* Bus and/or Interconnect error. */
		if ((mca_error & 0xe800) == 0x0800) {			
			printf("BUS%s ", mca_error_level(mca_error));
			switch ((mca_error & 0x0600) >> 9) {
			case 0:
				printf("Source");
				break;
			case 1:
				printf("Responder");
				break;
			case 2:
				printf("Observer");
				break;
			default:
				printf("???");
				break;
			}
			printf(" %s ", mca_error_request(mca_error));
			switch ((mca_error & 0x000c) >> 2) {
			case 0:
				printf("Memory");
				break;
			case 2:
				printf("I/O");
				break;
			case 3:
				printf("Other");
				break;
			default:
				printf("???");
				break;
			}
			if (mca_error & 0x0100)
				printf(" timed out");
			break;
		}

		printf("unknown error %x", mca_error);
		break;
	}
	printf("\n");
	if (rec->mr_status & MC_STATUS_ADDRV)
		printf("MCA: Address 0x%llx\n", (long long)rec->mr_addr);
	if (rec->mr_status & MC_STATUS_MISCV)
		printf("MCA: Misc 0x%llx\n", (long long)rec->mr_misc);
}

static int __nonnull(2)
mca_check_status(int bank, struct mca_record *rec)
{
	uint64_t status;
	u_int p[4];

	status = rdmsr(MSR_MC_STATUS(bank));
	if (!(status & MC_STATUS_VAL))
		return (0);

	/* Save exception information. */
	rec->mr_status = status;
	rec->mr_bank = bank;
	rec->mr_addr = 0;
	if (status & MC_STATUS_ADDRV)
		rec->mr_addr = rdmsr(MSR_MC_ADDR(bank));
	rec->mr_misc = 0;
	if (status & MC_STATUS_MISCV)
		rec->mr_misc = rdmsr(MSR_MC_MISC(bank));
	rec->mr_tsc = rdtsc();
	rec->mr_apic_id = PCPU_GET(apic_id);
	rec->mr_mcg_cap = rdmsr(MSR_MCG_CAP);
	rec->mr_mcg_status = rdmsr(MSR_MCG_STATUS);
	rec->mr_cpu_id = cpu_id;
	rec->mr_cpu_vendor_id = cpu_vendor_id;
	rec->mr_cpu = PCPU_GET(cpuid);

	/*
	 * Clear machine check.  Don't do this for uncorrectable
	 * errors so that the BIOS can see them.
	 */
	if (!(rec->mr_status & (MC_STATUS_PCC | MC_STATUS_UC))) {
		wrmsr(MSR_MC_STATUS(bank), 0);
		do_cpuid(0, p);
	}
	return (1);
}

static void __nonnull(1)
mca_record_entry(const struct mca_record *record)
{
	struct mca_internal *rec;

	rec = malloc(sizeof(*rec), M_MCA, M_NOWAIT);
	if (rec == NULL) {
		printf("MCA: Unable to allocate space for an event.\n");
		mca_log(record);
		return;
	}

	rec->rec = *record;
	rec->logged = 0;
	mtx_lock_spin(&mca_lock);
	STAILQ_INSERT_TAIL(&mca_records, rec, link);
	mca_count++;
	mtx_unlock_spin(&mca_lock);
}

#ifdef DEV_APIC
/*
 * Update the interrupt threshold for a CMCI.  The strategy is to use
 * a low trigger that interrupts as soon as the first event occurs.
 * However, if a steady stream of events arrive, the threshold is
 * increased until the interrupts are throttled to once every
 * cmc_throttle seconds or the periodic scan.  If a periodic scan
 * finds that the threshold is too high, it is lowered.
 */
static void
cmci_update(enum scan_mode mode, int bank, int valid, struct mca_record *rec)
{
	struct cmc_state *cc;
	uint64_t ctl;
	u_int delta;
	int count, limit;

	/* Fetch the current limit for this bank. */
	cc = &cmc_state[PCPU_GET(cpuid)][bank];
	ctl = rdmsr(MSR_MC_CTL2(bank));
	count = (rec->mr_status & MC_STATUS_COR_COUNT) >> 38;
	delta = (u_int)(ticks - cc->last_intr);

	/*
	 * If an interrupt was received less than cmc_throttle seconds
	 * since the previous interrupt and the count from the current
	 * event is greater than or equal to the current threshold,
	 * double the threshold up to the max.
	 */
	if (mode == CMCI && valid) {
		limit = ctl & MC_CTL2_THRESHOLD;
		if (delta < cmc_throttle && count >= limit &&
		    limit < cc->max_threshold) {
			limit = min(limit << 1, cc->max_threshold);
			ctl &= ~MC_CTL2_THRESHOLD;
			ctl |= limit;
			wrmsr(MSR_MC_CTL2(bank), limit);
		}
		cc->last_intr = ticks;
		return;
	}

	/*
	 * When the banks are polled, check to see if the threshold
	 * should be lowered.
	 */
	if (mode != POLLED)
		return;

	/* If a CMCI occured recently, do nothing for now. */
	if (delta < cmc_throttle)
		return;

	/*
	 * Compute a new limit based on the average rate of events per
	 * cmc_throttle seconds since the last interrupt.
	 */
	if (valid) {
		count = (rec->mr_status & MC_STATUS_COR_COUNT) >> 38;
		limit = count * cmc_throttle / delta;
		if (limit <= 0)
			limit = 1;
		else if (limit > cc->max_threshold)
			limit = cc->max_threshold;
	} else
		limit = 1;
	if ((ctl & MC_CTL2_THRESHOLD) != limit) {
		ctl &= ~MC_CTL2_THRESHOLD;
		ctl |= limit;
		wrmsr(MSR_MC_CTL2(bank), limit);
	}
}
#endif

/*
 * This scans all the machine check banks of the current CPU to see if
 * there are any machine checks.  Any non-recoverable errors are
 * reported immediately via mca_log().  The current thread must be
 * pinned when this is called.  The 'mode' parameter indicates if we
 * are being called from the MC exception handler, the CMCI handler,
 * or the periodic poller.  In the MC exception case this function
 * returns true if the system is restartable.  Otherwise, it returns a
 * count of the number of valid MC records found.
 */
static int
mca_scan(enum scan_mode mode)
{
	struct mca_record rec;
	uint64_t mcg_cap, ucmask;
	int count, i, recoverable, valid;

	count = 0;
	recoverable = 1;
	ucmask = MC_STATUS_UC | MC_STATUS_PCC;

	/* When handling a MCE#, treat the OVER flag as non-restartable. */
	if (mode == MCE)
		ucmask |= MC_STATUS_OVER;
	mcg_cap = rdmsr(MSR_MCG_CAP);
	for (i = 0; i < (mcg_cap & MCG_CAP_COUNT); i++) {
#ifdef DEV_APIC
		/*
		 * For a CMCI, only check banks this CPU is
		 * responsible for.
		 */
		if (mode == CMCI && !(PCPU_GET(cmci_mask) & 1 << i))
			continue;
#endif

		valid = mca_check_status(i, &rec);
		if (valid) {
			count++;
			if (rec.mr_status & ucmask) {
				recoverable = 0;
				mca_log(&rec);
			}
			mca_record_entry(&rec);
		}
	
#ifdef DEV_APIC
		/*
		 * If this is a bank this CPU monitors via CMCI,
		 * update the threshold.
		 */
		if (PCPU_GET(cmci_mask) & 1 << i)
			cmci_update(mode, i, valid, &rec);
#endif
	}
	return (mode == MCE ? recoverable : count);
}

/*
 * Scan the machine check banks on all CPUs by binding to each CPU in
 * turn.  If any of the CPUs contained new machine check records, log
 * them to the console.
 */
static void
mca_scan_cpus(void *context, int pending)
{
	struct mca_internal *mca;
	struct thread *td;
	int count, cpu;

	td = curthread;
	count = 0;
	thread_lock(td);
	CPU_FOREACH(cpu) {
		sched_bind(td, cpu);
		thread_unlock(td);
		count += mca_scan(POLLED);
		thread_lock(td);
		sched_unbind(td);
	}
	thread_unlock(td);
	if (count != 0) {
		mtx_lock_spin(&mca_lock);
		STAILQ_FOREACH(mca, &mca_records, link) {
			if (!mca->logged) {
				mca->logged = 1;
				mtx_unlock_spin(&mca_lock);
				mca_log(&mca->rec);
				mtx_lock_spin(&mca_lock);
			}
		}
		mtx_unlock_spin(&mca_lock);
	}
}

static void
mca_periodic_scan(void *arg)
{

	taskqueue_enqueue(taskqueue_thread, &mca_task);
	callout_reset(&mca_timer, mca_ticks * hz, mca_periodic_scan, NULL);
}

static int
sysctl_mca_scan(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	i = 0;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error)
		return (error);
	if (i)
		taskqueue_enqueue(taskqueue_thread, &mca_task);
	return (0);
}

static void
mca_startup(void *dummy)
{

	if (!mca_enabled || !(cpu_feature & CPUID_MCA))
		return;

	callout_reset(&mca_timer, mca_ticks * hz, mca_periodic_scan,
		    NULL);
}
SYSINIT(mca_startup, SI_SUB_SMP, SI_ORDER_ANY, mca_startup, NULL);

#ifdef DEV_APIC
static void
cmci_setup(uint64_t mcg_cap)
{
	int i;

	cmc_state = malloc((mp_maxid + 1) * sizeof(struct cmc_state **),
	    M_MCA, M_WAITOK);
	cmc_banks = mcg_cap & MCG_CAP_COUNT;
	for (i = 0; i <= mp_maxid; i++)
		cmc_state[i] = malloc(sizeof(struct cmc_state) * cmc_banks,
		    M_MCA, M_WAITOK | M_ZERO);
	SYSCTL_ADD_PROC(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "cmc_throttle", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    &cmc_throttle, 0, sysctl_positive_int, "I",
	    "Interval in seconds to throttle corrected MC interrupts");
}
#endif

static void
mca_setup(uint64_t mcg_cap)
{

	/*
	 * On AMD Family 10h processors, unless logging of level one TLB
	 * parity (L1TP) errors is disabled, enable the recommended workaround
	 * for Erratum 383.
	 */
	if (cpu_vendor_id == CPU_VENDOR_AMD &&
	    CPUID_TO_FAMILY(cpu_id) == 0x10 && amd10h_L1TP)
		workaround_erratum383 = 1;

	mtx_init(&mca_lock, "mca", NULL, MTX_SPIN);
	STAILQ_INIT(&mca_records);
	TASK_INIT(&mca_task, 0x8000, mca_scan_cpus, NULL);
	callout_init(&mca_timer, CALLOUT_MPSAFE);
	SYSCTL_ADD_INT(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "count", CTLFLAG_RD, &mca_count, 0, "Record count");
	SYSCTL_ADD_PROC(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "interval", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, &mca_ticks,
	    0, sysctl_positive_int, "I",
	    "Periodic interval in seconds to scan for machine checks");
	SYSCTL_ADD_NODE(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "records", CTLFLAG_RD, sysctl_mca_records, "Machine check records");
	SYSCTL_ADD_PROC(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "force_scan", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
	    sysctl_mca_scan, "I", "Force an immediate scan for machine checks");
#ifdef DEV_APIC
	if (mcg_cap & MCG_CAP_CMCI_P)
		cmci_setup(mcg_cap);
#endif
}

#ifdef DEV_APIC
/*
 * See if we should monitor CMCI for this bank.  If CMCI_EN is already
 * set in MC_CTL2, then another CPU is responsible for this bank, so
 * ignore it.  If CMCI_EN returns zero after being set, then this bank
 * does not support CMCI_EN.  If this CPU sets CMCI_EN, then it should
 * now monitor this bank.
 */
static void
cmci_monitor(int i)
{
	struct cmc_state *cc;
	uint64_t ctl;

	KASSERT(i < cmc_banks, ("CPU %d has more MC banks", PCPU_GET(cpuid)));

	ctl = rdmsr(MSR_MC_CTL2(i));
	if (ctl & MC_CTL2_CMCI_EN)
		/* Already monitored by another CPU. */
		return;

	/* Set the threshold to one event for now. */
	ctl &= ~MC_CTL2_THRESHOLD;
	ctl |= MC_CTL2_CMCI_EN | 1;
	wrmsr(MSR_MC_CTL2(i), ctl);
	ctl = rdmsr(MSR_MC_CTL2(i));
	if (!(ctl & MC_CTL2_CMCI_EN))
		/* This bank does not support CMCI. */
		return;

	cc = &cmc_state[PCPU_GET(cpuid)][i];

	/* Determine maximum threshold. */
	ctl &= ~MC_CTL2_THRESHOLD;
	ctl |= 0x7fff;
	wrmsr(MSR_MC_CTL2(i), ctl);
	ctl = rdmsr(MSR_MC_CTL2(i));
	cc->max_threshold = ctl & MC_CTL2_THRESHOLD;

	/* Start off with a threshold of 1. */
	ctl &= ~MC_CTL2_THRESHOLD;
	ctl |= 1;
	wrmsr(MSR_MC_CTL2(i), ctl);

	/* Mark this bank as monitored. */
	PCPU_SET(cmci_mask, PCPU_GET(cmci_mask) | 1 << i);
}

/*
 * For resume, reset the threshold for any banks we monitor back to
 * one and throw away the timestamp of the last interrupt.
 */
static void
cmci_resume(int i)
{
	struct cmc_state *cc;
	uint64_t ctl;

	KASSERT(i < cmc_banks, ("CPU %d has more MC banks", PCPU_GET(cpuid)));

	/* Ignore banks not monitored by this CPU. */
	if (!(PCPU_GET(cmci_mask) & 1 << i))
		return;

	cc = &cmc_state[PCPU_GET(cpuid)][i];
	cc->last_intr = -ticks;
	ctl = rdmsr(MSR_MC_CTL2(i));
	ctl &= ~MC_CTL2_THRESHOLD;
	ctl |= MC_CTL2_CMCI_EN | 1;
	wrmsr(MSR_MC_CTL2(i), ctl);
}
#endif

/*
 * Initializes per-CPU machine check registers and enables corrected
 * machine check interrupts.
 */
static void
_mca_init(int boot)
{
	uint64_t mcg_cap;
	uint64_t ctl, mask;
	int i, skip;

	/* MCE is required. */
	if (!mca_enabled || !(cpu_feature & CPUID_MCE))
		return;

	if (cpu_feature & CPUID_MCA) {
		if (boot)
			PCPU_SET(cmci_mask, 0);

		mcg_cap = rdmsr(MSR_MCG_CAP);
		if (mcg_cap & MCG_CAP_CTL_P)
			/* Enable MCA features. */
			wrmsr(MSR_MCG_CTL, MCG_CTL_ENABLE);
		if (PCPU_GET(cpuid) == 0 && boot)
			mca_setup(mcg_cap);

		/*
		 * Disable logging of level one TLB parity (L1TP) errors by
		 * the data cache as an alternative workaround for AMD Family
		 * 10h Erratum 383.  Unlike the recommended workaround, there
		 * is no performance penalty to this workaround.  However,
		 * L1TP errors will go unreported.
		 */
		if (cpu_vendor_id == CPU_VENDOR_AMD &&
		    CPUID_TO_FAMILY(cpu_id) == 0x10 && !amd10h_L1TP) {
			mask = rdmsr(MSR_MC0_CTL_MASK);
			if ((mask & (1UL << 5)) == 0)
				wrmsr(MSR_MC0_CTL_MASK, mask | (1UL << 5));
		}
		for (i = 0; i < (mcg_cap & MCG_CAP_COUNT); i++) {
			/* By default enable logging of all errors. */
			ctl = 0xffffffffffffffffUL;
			skip = 0;

			if (cpu_vendor_id == CPU_VENDOR_INTEL) {
				/*
				 * For P6 models before Nehalem MC0_CTL is
				 * always enabled and reserved.
				 */
				if (i == 0 && CPUID_TO_FAMILY(cpu_id) == 0x6
				    && CPUID_TO_MODEL(cpu_id) < 0x1a)
					skip = 1;
			} else if (cpu_vendor_id == CPU_VENDOR_AMD) {
				/* BKDG for Family 10h: unset GartTblWkEn. */
				if (i == 4 && CPUID_TO_FAMILY(cpu_id) >= 0xf)
					ctl &= ~(1UL << 10);
			}

			if (!skip)
				wrmsr(MSR_MC_CTL(i), ctl);

#ifdef DEV_APIC
			if (mcg_cap & MCG_CAP_CMCI_P) {
				if (boot)
					cmci_monitor(i);
				else
					cmci_resume(i);
			}
#endif

			/* Clear all errors. */
			wrmsr(MSR_MC_STATUS(i), 0);
		}

#ifdef DEV_APIC
		if (PCPU_GET(cmci_mask) != 0 && boot)
			lapic_enable_cmc();
#endif
	}

	load_cr4(rcr4() | CR4_MCE);
}

/* Must be executed on each CPU during boot. */
void
mca_init(void)
{

	_mca_init(1);
}

/* Must be executed on each CPU during resume. */
void
mca_resume(void)
{

	_mca_init(0);
}

/*
 * The machine check registers for the BSP cannot be initialized until
 * the local APIC is initialized.  This happens at SI_SUB_CPU,
 * SI_ORDER_SECOND.
 */
static void
mca_init_bsp(void *arg __unused)
{

	mca_init();
}
SYSINIT(mca_init_bsp, SI_SUB_CPU, SI_ORDER_ANY, mca_init_bsp, NULL);

/* Called when a machine check exception fires. */
int
mca_intr(void)
{
	uint64_t mcg_status;
	int recoverable;

	if (!(cpu_feature & CPUID_MCA)) {
		/*
		 * Just print the values of the old Pentium registers
		 * and panic.
		 */
		printf("MC Type: 0x%jx  Address: 0x%jx\n",
		    (uintmax_t)rdmsr(MSR_P5_MC_TYPE),
		    (uintmax_t)rdmsr(MSR_P5_MC_ADDR));
		return (0);
	}

	/* Scan the banks and check for any non-recoverable errors. */
	recoverable = mca_scan(MCE);
	mcg_status = rdmsr(MSR_MCG_STATUS);
	if (!(mcg_status & MCG_STATUS_RIPV))
		recoverable = 0;

	/* Clear MCIP. */
	wrmsr(MSR_MCG_STATUS, mcg_status & ~MCG_STATUS_MCIP);
	return (recoverable);
}

#ifdef DEV_APIC
/* Called for a CMCI (correctable machine check interrupt). */
void
cmc_intr(void)
{
	struct mca_internal *mca;
	int count;

	/*
	 * Serialize MCA bank scanning to prevent collisions from
	 * sibling threads.
	 */
	count = mca_scan(CMCI);

	/* If we found anything, log them to the console. */
	if (count != 0) {
		mtx_lock_spin(&mca_lock);
		STAILQ_FOREACH(mca, &mca_records, link) {
			if (!mca->logged) {
				mca->logged = 1;
				mtx_unlock_spin(&mca_lock);
				mca_log(&mca->rec);
				mtx_lock_spin(&mca_lock);
			}
		}
		mtx_unlock_spin(&mca_lock);
	}
}
#endif
