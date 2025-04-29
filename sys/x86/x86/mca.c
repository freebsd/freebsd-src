/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Hudson River Trading LLC
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
#include <x86/apicvar.h>
#include <machine/cpu.h>
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
	time_t	last_intr;
};

struct amd_et_state {
	int	cur_threshold;
	time_t	last_intr;
};
#endif

struct mca_internal {
	struct mca_record rec;
	STAILQ_ENTRY(mca_internal) link;
};

struct mca_enumerator_ops {
        unsigned int (*ctl)(int);
        unsigned int (*status)(int);
        unsigned int (*addr)(int);
        unsigned int (*misc)(int);
};

static MALLOC_DEFINE(M_MCA, "MCA", "Machine Check Architecture");

static volatile int mca_count;	/* Number of records stored. */
static int mca_banks;		/* Number of per-CPU register banks. */
static int mca_maxcount = -1;	/* Limit on records stored. (-1 = unlimited) */

static SYSCTL_NODE(_hw, OID_AUTO, mca, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
    "Machine Check Architecture");

static int mca_enabled = 1;
SYSCTL_INT(_hw_mca, OID_AUTO, enabled, CTLFLAG_RDTUN, &mca_enabled, 0,
    "Administrative toggle for machine check support");

static int log_corrected = 1;
SYSCTL_INT(_hw_mca, OID_AUTO, log_corrected, CTLFLAG_RWTUN, &log_corrected, 0,
    "Log corrected errors to the console");

static int amd10h_L1TP = 1;
SYSCTL_INT(_hw_mca, OID_AUTO, amd10h_L1TP, CTLFLAG_RDTUN, &amd10h_L1TP, 0,
    "Administrative toggle for logging of level one TLB parity (L1TP) errors");

static int intel6h_HSD131;
SYSCTL_INT(_hw_mca, OID_AUTO, intel6h_HSD131, CTLFLAG_RDTUN, &intel6h_HSD131, 0,
    "Administrative toggle for logging of spurious corrected errors");

int workaround_erratum383;
SYSCTL_INT(_hw_mca, OID_AUTO, erratum383, CTLFLAG_RDTUN,
    &workaround_erratum383, 0,
    "Is the workaround for Erratum 383 on AMD Family 10h processors enabled?");

static STAILQ_HEAD(, mca_internal) mca_freelist;
static int mca_freecount;
static STAILQ_HEAD(, mca_internal) mca_records;
static STAILQ_HEAD(, mca_internal) mca_pending;
static int mca_ticks = 300;
static struct taskqueue *mca_tq;
static struct task mca_resize_task;
static struct timeout_task mca_scan_task;
static struct mtx mca_lock;

static unsigned int
mca_ia32_ctl_reg(int bank)
{
	return (MSR_MC_CTL(bank));
}

static unsigned int
mca_ia32_status_reg(int bank)
{
	return (MSR_MC_STATUS(bank));
}

static unsigned int
mca_ia32_addr_reg(int bank)
{
	return (MSR_MC_ADDR(bank));
}

static unsigned int
mca_ia32_misc_reg(int bank)
{
	return (MSR_MC_MISC(bank));
}

static unsigned int
mca_smca_ctl_reg(int bank)
{
        return (MSR_SMCA_MC_CTL(bank));
}

static unsigned int
mca_smca_status_reg(int bank)
{
        return (MSR_SMCA_MC_STATUS(bank));
}

static unsigned int
mca_smca_addr_reg(int bank)
{
        return (MSR_SMCA_MC_ADDR(bank));
}

static unsigned int
mca_smca_misc_reg(int bank)
{
        return (MSR_SMCA_MC_MISC(bank));
}

static struct mca_enumerator_ops mca_msr_ops = {
        .ctl    = mca_ia32_ctl_reg,
        .status = mca_ia32_status_reg,
        .addr   = mca_ia32_addr_reg,
        .misc   = mca_ia32_misc_reg
};

#ifdef DEV_APIC
static struct cmc_state **cmc_state;		/* Indexed by cpuid, bank. */
static struct amd_et_state **amd_et_state;	/* Indexed by cpuid, bank. */
static int cmc_throttle = 60;	/* Time in seconds to throttle CMCI. */

static int amd_elvt = -1;

static inline bool
amd_thresholding_supported(void)
{
	if (cpu_vendor_id != CPU_VENDOR_AMD &&
	    cpu_vendor_id != CPU_VENDOR_HYGON)
		return (false);
	/*
	 * The RASCap register is wholly reserved in families 0x10-0x15 (through model 1F).
	 *
	 * It begins to be documented in family 0x15 model 30 and family 0x16,
	 * but neither of these families documents the ScalableMca bit, which
	 * supposedly defines the presence of this feature on family 0x17.
	 */
	if (CPUID_TO_FAMILY(cpu_id) >= 0x10 && CPUID_TO_FAMILY(cpu_id) <= 0x16)
		return (true);
	if (CPUID_TO_FAMILY(cpu_id) >= 0x17)
		return ((amd_rascap & AMDRAS_SCALABLE_MCA) != 0);
	return (false);
}
#endif

static inline bool
cmci_supported(uint64_t mcg_cap)
{
	/*
	 * MCG_CAP_CMCI_P bit is reserved in AMD documentation.  Until
	 * it is defined, do not use it to check for CMCI support.
	 */
	if (cpu_vendor_id != CPU_VENDOR_INTEL)
		return (false);
	return ((mcg_cap & MCG_CAP_CMCI_P) != 0);
}

static inline bool
tes_supported(uint64_t mcg_cap)
{

	/*
	 * MCG_CAP_TES_P bit is reserved in AMD documentation.  Until
	 * it is defined, do not use it to check for TES support.
	 */
	if (cpu_vendor_id != CPU_VENDOR_INTEL)
		return (false);
	return ((mcg_cap & MCG_CAP_TES_P) != 0);
}

static inline bool
ser_supported(uint64_t mcg_cap)
{

	return (tes_supported(mcg_cap) && (mcg_cap & MCG_CAP_SER_P) != 0);
}

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

static const char *
mca_addres_mode(uint64_t mca_misc)
{

	switch ((mca_misc & MC_MISC_ADDRESS_MODE) >> 6) {
	case 0x0:
		return ("Segment Offset");
	case 0x1:
		return ("Linear Address");
	case 0x2:
		return ("Physical Address");
	case 0x3:
		return ("Memory Address");
	case 0x7:
		return ("Generic");
	}
	return ("???");
}

static int
mca_mute(const struct mca_record *rec)
{

	/*
	 * Skip spurious corrected parity errors generated by Intel Haswell-
	 * and Broadwell-based CPUs (see HSD131, HSM142, HSW131 and BDM48
	 * erratum respectively), unless reporting is enabled.
	 * Note that these errors also have been observed with the D0-stepping
	 * of Haswell, while at least initially the CPU specification updates
	 * suggested only the C0-stepping to be affected.  Similarly, Celeron
	 * 2955U with a CPU ID of 0x45 apparently are also concerned with the
	 * same problem, with HSM142 only referring to 0x3c and 0x46.
	 */
	if (cpu_vendor_id == CPU_VENDOR_INTEL &&
	    CPUID_TO_FAMILY(cpu_id) == 0x6 &&
	    (CPUID_TO_MODEL(cpu_id) == 0x3c ||	/* HSD131, HSM142, HSW131 */
	    CPUID_TO_MODEL(cpu_id) == 0x3d ||	/* BDM48 */
	    CPUID_TO_MODEL(cpu_id) == 0x45 ||
	    CPUID_TO_MODEL(cpu_id) == 0x46) &&	/* HSM142 */
	    rec->mr_bank == 0 &&
	    (rec->mr_status & 0xa0000000ffffffff) == 0x80000000000f0005 &&
	    !intel6h_HSD131)
	    	return (1);

	return (0);
}

/* Dump details about a single machine check. */
static void
mca_log(const struct mca_record *rec)
{
	uint16_t mca_error;

	if (mca_mute(rec))
		return;

	if (!log_corrected && (rec->mr_status & MC_STATUS_UC) == 0 &&
	    (!tes_supported(rec->mr_mcg_cap) ||
	    ((rec->mr_status & MC_STATUS_TES_STATUS) >> 53) != 0x2))
		return;

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
		if (cmci_supported(rec->mr_mcg_cap))
			printf("(%lld) ", ((long long)rec->mr_status &
			    MC_STATUS_COR_COUNT) >> 38);
		if (tes_supported(rec->mr_mcg_cap)) {
			switch ((rec->mr_status & MC_STATUS_TES_STATUS) >> 53) {
			case 0x1:
				printf("(Green) ");
				break;
			case 0x2:
				printf("(Yellow) ");
				break;
			}
		}
	}
	if (rec->mr_status & MC_STATUS_EN)
		printf("EN ");
	if (rec->mr_status & MC_STATUS_PCC)
		printf("PCC ");
	if (ser_supported(rec->mr_mcg_cap)) {
		if (rec->mr_status & MC_STATUS_S)
			printf("S ");
		if (rec->mr_status & MC_STATUS_AR)
			printf("AR ");
	}
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
	case 0x0006:
		printf("SMM handler code access violation");
		break;
	case 0x0400:
		printf("internal timer error");
		break;
	case 0x0e0b:
		printf("generic I/O error");
		if (rec->mr_cpu_vendor_id == CPU_VENDOR_INTEL &&
		    (rec->mr_status & MC_STATUS_MISCV)) {
			printf(" (pci%d:%d:%d:%d)",
			    (int)((rec->mr_misc & MC_MISC_PCIE_SEG) >> 32),
			    (int)((rec->mr_misc & MC_MISC_PCIE_BUS) >> 24),
			    (int)((rec->mr_misc & MC_MISC_PCIE_SLOT) >> 19),
			    (int)((rec->mr_misc & MC_MISC_PCIE_FUNC) >> 16));
		}
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

		/* Extended memory error. */
		if ((mca_error & 0xef80) == 0x0280) {
			printf("%s channel ", mca_error_mmtype(mca_error));
			if ((mca_error & 0x000f) != 0x000f)
				printf("%d", mca_error & 0x000f);
			else
				printf("??");
			printf(" extended memory error");
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
	if (rec->mr_status & MC_STATUS_ADDRV) {
		printf("MCA: Address 0x%llx", (long long)rec->mr_addr);
		if (ser_supported(rec->mr_mcg_cap) &&
		    (rec->mr_status & MC_STATUS_MISCV)) {
			printf(" (Mode: %s, LSB: %d)",
			    mca_addres_mode(rec->mr_misc),
			    (int)(rec->mr_misc & MC_MISC_RA_LSB));
		}
		printf("\n");
	}
	if (rec->mr_status & MC_STATUS_MISCV)
		printf("MCA: Misc 0x%llx\n", (long long)rec->mr_misc);
}

static bool
mca_is_mce(uint64_t mcg_cap, uint64_t status, bool *recoverablep)
{

	/* Corrected error. */
	if ((status & MC_STATUS_UC) == 0)
		return (0);

	/* Spurious MCA error. */
	if ((status & MC_STATUS_EN) == 0)
		return (0);

	/* The processor does not support software error recovery. */
	if (!ser_supported(mcg_cap)) {
		*recoverablep = false;
		return (1);
	}

	/* Context might have been corrupted. */
	if (status & MC_STATUS_PCC) {
		*recoverablep = false;
		return (1);
	}

	/* Uncorrected software recoverable. */
	if (status & MC_STATUS_S) {
		/* Action required vs optional. */
		if (status & MC_STATUS_AR)
			*recoverablep = false;
		return (1);
	}

	/* Uncorrected no action required. */
	return (0);
}

static int
mca_check_status(enum scan_mode mode, uint64_t mcg_cap, int bank,
    struct mca_record *rec, bool *recoverablep)
{
	uint64_t status;
	u_int p[4];
	bool mce, recover;

	status = rdmsr(mca_msr_ops.status(bank));
	if (!(status & MC_STATUS_VAL))
		return (0);

	recover = *recoverablep;
	mce = mca_is_mce(mcg_cap, status, &recover);
	if (mce != (mode == MCE))
		return (0);
	*recoverablep = recover;

	/* Save exception information. */
	rec->mr_status = status;
	rec->mr_bank = bank;
	rec->mr_addr = 0;
	if (status & MC_STATUS_ADDRV)
		rec->mr_addr = rdmsr(mca_msr_ops.addr(bank));
	rec->mr_misc = 0;
	if (status & MC_STATUS_MISCV)
		rec->mr_misc = rdmsr(mca_msr_ops.misc(bank));
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
	if (!mce || recover) {
		wrmsr(mca_msr_ops.status(bank), 0);
		do_cpuid(0, p);
	}
	return (1);
}

static void
mca_resize_freelist(void)
{
	struct mca_internal *next, *rec;
	STAILQ_HEAD(, mca_internal) tmplist;
	int count, i, desired_max, desired_min;

	/*
	 * Ensure we have at least one record for each bank and one
	 * record per CPU, but no more than twice that amount.
	 */
	desired_min = imax(mp_ncpus, mca_banks);
	desired_max = imax(mp_ncpus, mca_banks) * 2;
	STAILQ_INIT(&tmplist);
	mtx_lock_spin(&mca_lock);
	while (mca_freecount > desired_max) {
		rec = STAILQ_FIRST(&mca_freelist);
		KASSERT(rec != NULL, ("mca_freecount is %d, but list is empty",
		    mca_freecount));
		STAILQ_REMOVE_HEAD(&mca_freelist, link);
		mca_freecount--;
		STAILQ_INSERT_TAIL(&tmplist, rec, link);
	}
	while (mca_freecount < desired_min) {
		count = desired_min - mca_freecount;
		mtx_unlock_spin(&mca_lock);
		for (i = 0; i < count; i++) {
			rec = malloc(sizeof(*rec), M_MCA, M_WAITOK);
			STAILQ_INSERT_TAIL(&tmplist, rec, link);
		}
		mtx_lock_spin(&mca_lock);
		STAILQ_CONCAT(&mca_freelist, &tmplist);
		mca_freecount += count;
	}
	mtx_unlock_spin(&mca_lock);
	STAILQ_FOREACH_SAFE(rec, &tmplist, link, next)
		free(rec, M_MCA);
}

static void
mca_resize(void *context, int pending)
{

	mca_resize_freelist();
}

static void
mca_record_entry(enum scan_mode mode, const struct mca_record *record)
{
	struct mca_internal *rec;

	if (mode == POLLED) {
		rec = malloc(sizeof(*rec), M_MCA, M_WAITOK);
		mtx_lock_spin(&mca_lock);
	} else {
		mtx_lock_spin(&mca_lock);
		rec = STAILQ_FIRST(&mca_freelist);
		if (rec == NULL) {
			printf("MCA: Unable to allocate space for an event.\n");
			mca_log(record);
			mtx_unlock_spin(&mca_lock);
			return;
		}
		STAILQ_REMOVE_HEAD(&mca_freelist, link);
		mca_freecount--;
	}

	rec->rec = *record;
	STAILQ_INSERT_TAIL(&mca_pending, rec, link);
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
static int
update_threshold(enum scan_mode mode, int valid, int last_intr, int count,
    int cur_threshold, int max_threshold)
{
	u_int delta;
	int limit;

	delta = (u_int)(time_uptime - last_intr);
	limit = cur_threshold;

	/*
	 * If an interrupt was received less than cmc_throttle seconds
	 * since the previous interrupt and the count from the current
	 * event is greater than or equal to the current threshold,
	 * double the threshold up to the max.
	 */
	if (mode == CMCI && valid) {
		if (delta < cmc_throttle && count >= limit &&
		    limit < max_threshold) {
			limit = min(limit << 1, max_threshold);
		}
		return (limit);
	}

	/*
	 * When the banks are polled, check to see if the threshold
	 * should be lowered.
	 */
	if (mode != POLLED)
		return (limit);

	/* If a CMCI occurred recently, do nothing for now. */
	if (delta < cmc_throttle)
		return (limit);

	/*
	 * Compute a new limit based on the average rate of events per
	 * cmc_throttle seconds since the last interrupt.
	 */
	if (valid) {
		limit = count * cmc_throttle / delta;
		if (limit <= 0)
			limit = 1;
		else if (limit > max_threshold)
			limit = max_threshold;
	} else {
		limit = 1;
	}
	return (limit);
}

static void
cmci_update(enum scan_mode mode, int bank, int valid, struct mca_record *rec)
{
	struct cmc_state *cc;
	uint64_t ctl;
	int cur_threshold, new_threshold;
	int count;

	/* Fetch the current limit for this bank. */
	cc = &cmc_state[PCPU_GET(cpuid)][bank];
	ctl = rdmsr(MSR_MC_CTL2(bank));
	count = (rec->mr_status & MC_STATUS_COR_COUNT) >> 38;
	cur_threshold = ctl & MC_CTL2_THRESHOLD;

	new_threshold = update_threshold(mode, valid, cc->last_intr, count,
	    cur_threshold, cc->max_threshold);

	if (mode == CMCI && valid)
		cc->last_intr = time_uptime;
	if (new_threshold != cur_threshold) {
		ctl &= ~MC_CTL2_THRESHOLD;
		ctl |= new_threshold;
		wrmsr(MSR_MC_CTL2(bank), ctl);
	}
}

static void
amd_thresholding_update(enum scan_mode mode, int bank, int valid)
{
	struct amd_et_state *cc;
	uint64_t misc;
	int new_threshold;
	int count;

	cc = &amd_et_state[PCPU_GET(cpuid)][bank];
	misc = rdmsr(mca_msr_ops.misc(bank));
	count = (misc & MC_MISC_AMD_CNT_MASK) >> MC_MISC_AMD_CNT_SHIFT;
	count = count - (MC_MISC_AMD_CNT_MAX - cc->cur_threshold);

	new_threshold = update_threshold(mode, valid, cc->last_intr, count,
	    cc->cur_threshold, MC_MISC_AMD_CNT_MAX);

	cc->cur_threshold = new_threshold;
	misc &= ~MC_MISC_AMD_CNT_MASK;
	misc |= (uint64_t)(MC_MISC_AMD_CNT_MAX - cc->cur_threshold)
	    << MC_MISC_AMD_CNT_SHIFT;
	misc &= ~MC_MISC_AMD_OVERFLOW;
	wrmsr(mca_msr_ops.misc(bank), misc);
	if (mode == CMCI && valid)
		cc->last_intr = time_uptime;
}
#endif

/*
 * This scans all the machine check banks of the current CPU to see if
 * there are any machine checks.  Any non-recoverable errors are
 * reported immediately via mca_log().  The current thread must be
 * pinned when this is called.  The 'mode' parameter indicates if we
 * are being called from the MC exception handler, the CMCI handler,
 * or the periodic poller.
 */
static int
mca_scan(enum scan_mode mode, bool *recoverablep)
{
	struct mca_record rec;
	uint64_t mcg_cap;
	int count = 0, i, valid;

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

		valid = mca_check_status(mode, mcg_cap, i, &rec, recoverablep);
		if (valid) {
			count++;
			if (*recoverablep)
				mca_record_entry(mode, &rec);
			else
				mca_log(&rec);
		}

#ifdef DEV_APIC
		/*
		 * If this is a bank this CPU monitors via CMCI,
		 * update the threshold.
		 */
		if (PCPU_GET(cmci_mask) & 1 << i) {
			if (cmc_state != NULL)
				cmci_update(mode, i, valid, &rec);
			else
				amd_thresholding_update(mode, i, valid);
		}
#endif
	}
	return (count);
}

/*
 * Store a new record on the mca_records list while enforcing
 * mca_maxcount.
 */
static void
mca_store_record(struct mca_internal *mca)
{

	/*
	 * If we are storing no records (mca_maxcount == 0),
	 * we just free this record.
	 *
	 * If we are storing records (mca_maxcount != 0) and
	 * we have free space on the list, store the record
	 * and increment mca_count.
	 *
	 * If we are storing records and we do not have free
	 * space on the list, store the new record at the
	 * tail and free the oldest one from the head.
	 */
	if (mca_maxcount != 0)
		STAILQ_INSERT_TAIL(&mca_records, mca, link);
	if (mca_maxcount < 0 || mca_count < mca_maxcount)
		mca_count++;
	else {
		if (mca_maxcount != 0) {
			mca = STAILQ_FIRST(&mca_records);
			STAILQ_REMOVE_HEAD(&mca_records, link);
		}
		STAILQ_INSERT_TAIL(&mca_freelist, mca, link);
		mca_freecount++;
	}
}

/*
 * Do the work to process machine check records which have just been
 * gathered. Print any pending logs to the console. Queue them for storage.
 * Trigger a resizing of the free list.
 */
static void
mca_process_records(enum scan_mode mode)
{
	struct mca_internal *mca;

	mtx_lock_spin(&mca_lock);
	while ((mca = STAILQ_FIRST(&mca_pending)) != NULL) {
		STAILQ_REMOVE_HEAD(&mca_pending, link);
		mca_log(&mca->rec);
		mca_store_record(mca);
	}
	mtx_unlock_spin(&mca_lock);
	if (mode == POLLED)
		mca_resize_freelist();
	else if (!cold)
		taskqueue_enqueue(mca_tq, &mca_resize_task);
}

/*
 * Scan the machine check banks on all CPUs by binding to each CPU in
 * turn.  If any of the CPUs contained new machine check records, log
 * them to the console.
 */
static void
mca_scan_cpus(void *context, int pending)
{
	struct thread *td;
	int cpu;
	bool recoverable = true;

	mca_resize_freelist();
	td = curthread;
	thread_lock(td);
	CPU_FOREACH(cpu) {
		sched_bind(td, cpu);
		thread_unlock(td);
		mca_scan(POLLED, &recoverable);
		thread_lock(td);
		sched_unbind(td);
	}
	thread_unlock(td);
	if (!STAILQ_EMPTY(&mca_pending))
		mca_process_records(POLLED);
	taskqueue_enqueue_timeout_sbt(mca_tq, &mca_scan_task,
	    mca_ticks * SBT_1S, 0, C_PREL(1));
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
		taskqueue_enqueue_timeout_sbt(mca_tq, &mca_scan_task,
		    0, 0, 0);
	return (0);
}

static int
sysctl_mca_maxcount(SYSCTL_HANDLER_ARGS)
{
	struct mca_internal *mca;
	int error, i;
	bool doresize;

	i = mca_maxcount;
	error = sysctl_handle_int(oidp, &i, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	mtx_lock_spin(&mca_lock);
	mca_maxcount = i;
	doresize = false;
	if (mca_maxcount >= 0)
		while (mca_count > mca_maxcount) {
			mca = STAILQ_FIRST(&mca_records);
			STAILQ_REMOVE_HEAD(&mca_records, link);
			mca_count--;
			STAILQ_INSERT_TAIL(&mca_freelist, mca, link);
			mca_freecount++;
			doresize = true;
		}
	mtx_unlock_spin(&mca_lock);
	if (doresize && !cold)
		taskqueue_enqueue(mca_tq, &mca_resize_task);
	return (error);
}

static void
mca_startup(void *dummy)
{

	if (mca_banks <= 0)
		return;

	/* CMCIs during boot may have claimed items from the freelist. */
	mca_resize_freelist();

	taskqueue_start_threads(&mca_tq, 1, PI_SWI(SWI_TQ), "mca taskq");
	taskqueue_enqueue_timeout_sbt(mca_tq, &mca_scan_task,
	    mca_ticks * SBT_1S, 0, C_PREL(1));
}
SYSINIT(mca_startup, SI_SUB_KICK_SCHEDULER, SI_ORDER_ANY, mca_startup, NULL);

#ifdef DEV_APIC
static void
cmci_setup(void)
{
	int i;

	cmc_state = malloc((mp_maxid + 1) * sizeof(struct cmc_state *), M_MCA,
	    M_WAITOK);
	for (i = 0; i <= mp_maxid; i++)
		cmc_state[i] = malloc(sizeof(struct cmc_state) * mca_banks,
		    M_MCA, M_WAITOK | M_ZERO);
	SYSCTL_ADD_PROC(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "cmc_throttle", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &cmc_throttle, 0, sysctl_positive_int, "I",
	    "Interval in seconds to throttle corrected MC interrupts");
}

static void
amd_thresholding_setup(void)
{
	u_int i;

	amd_et_state = malloc((mp_maxid + 1) * sizeof(struct amd_et_state *),
	    M_MCA, M_WAITOK);
	for (i = 0; i <= mp_maxid; i++)
		amd_et_state[i] = malloc(sizeof(struct amd_et_state) *
		    mca_banks, M_MCA, M_WAITOK | M_ZERO);
	SYSCTL_ADD_PROC(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "cmc_throttle", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
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

	mca_banks = mcg_cap & MCG_CAP_COUNT;
	mtx_init(&mca_lock, "mca", NULL, MTX_SPIN);
	STAILQ_INIT(&mca_records);
	STAILQ_INIT(&mca_pending);
	mca_tq = taskqueue_create_fast("mca", M_WAITOK,
	    taskqueue_thread_enqueue, &mca_tq);
	TIMEOUT_TASK_INIT(mca_tq, &mca_scan_task, 0, mca_scan_cpus, NULL);
	STAILQ_INIT(&mca_freelist);
	TASK_INIT(&mca_resize_task, 0, mca_resize, NULL);
	mca_resize_freelist();
	SYSCTL_ADD_INT(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "count", CTLFLAG_RD, (int *)(uintptr_t)&mca_count, 0,
	    "Record count");
	SYSCTL_ADD_PROC(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "maxcount", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &mca_maxcount, 0, sysctl_mca_maxcount, "I",
	    "Maximum record count (-1 is unlimited)");
	SYSCTL_ADD_PROC(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "interval", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    &mca_ticks, 0, sysctl_positive_int, "I",
	    "Periodic interval in seconds to scan for machine checks");
	SYSCTL_ADD_NODE(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "records", CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_mca_records,
	    "Machine check records");
	SYSCTL_ADD_PROC(NULL, SYSCTL_STATIC_CHILDREN(_hw_mca), OID_AUTO,
	    "force_scan", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
	    sysctl_mca_scan, "I", "Force an immediate scan for machine checks");
#ifdef DEV_APIC
	if (cmci_supported(mcg_cap))
		cmci_setup();
	else if (amd_thresholding_supported())
		amd_thresholding_setup();
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

	KASSERT(i < mca_banks, ("CPU %d has more MC banks", PCPU_GET(cpuid)));

	/*
	 * It is possible for some APs to report CMCI support even if the BSP
	 * does not, apparently due to a BIOS bug.
	 */
	if (cmc_state == NULL) {
		if (bootverbose) {
			printf(
		    "AP %d (%d,%d) reports CMCI support but the BSP does not\n",
			    PCPU_GET(cpuid), PCPU_GET(apic_id),
			    PCPU_GET(acpi_id));
		}
		return;
	}

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

	KASSERT(i < mca_banks, ("CPU %d has more MC banks", PCPU_GET(cpuid)));

	/* See cmci_monitor(). */
	if (cmc_state == NULL)
		return;

	/* Ignore banks not monitored by this CPU. */
	if (!(PCPU_GET(cmci_mask) & 1 << i))
		return;

	cc = &cmc_state[PCPU_GET(cpuid)][i];
	cc->last_intr = 0;
	ctl = rdmsr(MSR_MC_CTL2(i));
	ctl &= ~MC_CTL2_THRESHOLD;
	ctl |= MC_CTL2_CMCI_EN | 1;
	wrmsr(MSR_MC_CTL2(i), ctl);
}

/*
 * Apply an AMD ET configuration to the corresponding MSR.
 */
static void
amd_thresholding_start(struct amd_et_state *cc, int bank)
{
	uint64_t misc;

	KASSERT(amd_elvt >= 0, ("ELVT offset is not set"));

	misc = rdmsr(mca_msr_ops.misc(bank));

	misc &= ~MC_MISC_AMD_INT_MASK;
	misc |= MC_MISC_AMD_INT_LVT;

	misc &= ~MC_MISC_AMD_LVT_MASK;
	misc |= (uint64_t)amd_elvt << MC_MISC_AMD_LVT_SHIFT;

	misc &= ~MC_MISC_AMD_CNT_MASK;
	misc |= (uint64_t)(MC_MISC_AMD_CNT_MAX - cc->cur_threshold)
	    << MC_MISC_AMD_CNT_SHIFT;

	misc &= ~MC_MISC_AMD_OVERFLOW;
	misc |= MC_MISC_AMD_CNTEN;

	wrmsr(mca_msr_ops.misc(bank), misc);
}

static void
amd_thresholding_monitor(int i)
{
	struct amd_et_state *cc;
	uint64_t misc;

	/*
	 * Kludge: On 10h, banks after 4 are not thresholding but also may have
	 * bogus Valid bits.  Skip them.  This is definitely fixed in 15h, but
	 * I have not investigated whether it is fixed in earlier models.
	 */
	if (CPUID_TO_FAMILY(cpu_id) < 0x15 && i >= 5)
		return;

	/* The counter must be valid and present. */
	misc = rdmsr(mca_msr_ops.misc(i));
	if ((misc & (MC_MISC_AMD_VAL | MC_MISC_AMD_CNTP)) !=
	    (MC_MISC_AMD_VAL | MC_MISC_AMD_CNTP))
		return;

	/* The register should not be locked. */
	if ((misc & MC_MISC_AMD_LOCK) != 0) {
		if (bootverbose)
			printf("%s: 0x%jx: Bank %d: locked\n", __func__,
			    (uintmax_t)misc, i);
		return;
	}

	/*
	 * If counter is enabled then either the firmware or another CPU
	 * has already claimed it.
	 */
	if ((misc & MC_MISC_AMD_CNTEN) != 0) {
		if (bootverbose)
			printf("%s: 0x%jx: Bank %d: already enabled\n",
			    __func__, (uintmax_t)misc, i);
		return;
	}

	/*
	 * Configure an Extended Interrupt LVT register for reporting
	 * counter overflows if that feature is supported and the first
	 * extended register is available.
	 */
	amd_elvt = lapic_enable_mca_elvt();
	if (amd_elvt < 0) {
		printf("%s: Bank %d: lapic enable mca elvt failed: %d\n",
		    __func__, i, amd_elvt);
		return;
	}

	cc = &amd_et_state[PCPU_GET(cpuid)][i];
	cc->cur_threshold = 1;
	amd_thresholding_start(cc, i);

	/* Mark this bank as monitored. */
	PCPU_SET(cmci_mask, PCPU_GET(cmci_mask) | 1 << i);
}

static void
amd_thresholding_resume(int i)
{
	struct amd_et_state *cc;

	KASSERT(i < mca_banks, ("CPU %d has more MC banks", PCPU_GET(cpuid)));

	/* Ignore banks not monitored by this CPU. */
	if (!(PCPU_GET(cmci_mask) & 1 << i))
		return;

	cc = &amd_et_state[PCPU_GET(cpuid)][i];
	cc->last_intr = 0;
	cc->cur_threshold = 1;
	amd_thresholding_start(cc, i);
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
	int i, skip, family;

	family = CPUID_TO_FAMILY(cpu_id);

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
		if (IS_BSP() && boot)
			mca_setup(mcg_cap);

		/*
		 * Disable logging of level one TLB parity (L1TP) errors by
		 * the data cache as an alternative workaround for AMD Family
		 * 10h Erratum 383.  Unlike the recommended workaround, there
		 * is no performance penalty to this workaround.  However,
		 * L1TP errors will go unreported.
		 */
		if (cpu_vendor_id == CPU_VENDOR_AMD && family == 0x10 &&
		    !amd10h_L1TP) {
			mask = rdmsr(MSR_MC0_CTL_MASK);
			if ((mask & (1UL << 5)) == 0)
				wrmsr(MSR_MC0_CTL_MASK, mask | (1UL << 5));
		}
		if (amd_rascap & AMDRAS_SCALABLE_MCA) {
			mca_msr_ops.ctl = mca_smca_ctl_reg;
			mca_msr_ops.status = mca_smca_status_reg;
			mca_msr_ops.addr = mca_smca_addr_reg;
			mca_msr_ops.misc = mca_smca_misc_reg;
		}

		/* Enable local MCE if supported. */
		if (cpu_vendor_id == CPU_VENDOR_INTEL &&
		    (mcg_cap & MCG_CAP_LMCE_P) &&
		    (rdmsr(MSR_IA32_FEATURE_CONTROL) &
		     IA32_FEATURE_CONTROL_LMCE_EN))
			wrmsr(MSR_MCG_EXT_CTL, rdmsr(MSR_MCG_EXT_CTL) | 1);

		/*
		 * The cmci_monitor() must not be executed
		 * simultaneously by several CPUs.
		 */
		if (boot)
			mtx_lock_spin(&mca_lock);

		for (i = 0; i < (mcg_cap & MCG_CAP_COUNT); i++) {
			/* By default enable logging of all errors. */
			ctl = 0xffffffffffffffffUL;
			skip = 0;

			if (cpu_vendor_id == CPU_VENDOR_INTEL) {
				/*
				 * For P6 models before Nehalem MC0_CTL is
				 * always enabled and reserved.
				 */
				if (i == 0 && family == 0x6
				    && CPUID_TO_MODEL(cpu_id) < 0x1a)
					skip = 1;
			} else if (cpu_vendor_id == CPU_VENDOR_AMD) {
				/* BKDG for Family 10h: unset GartTblWkEn. */
				if (i == MC_AMDNB_BANK && family >= 0xf &&
				    family < 0x17)
					ctl &= ~(1UL << 10);
			}

			if (!skip)
				wrmsr(mca_msr_ops.ctl(i), ctl);

#ifdef DEV_APIC
			if (cmci_supported(mcg_cap)) {
				if (boot)
					cmci_monitor(i);
				else
					cmci_resume(i);
			} else if (amd_thresholding_supported()) {
				if (boot)
					amd_thresholding_monitor(i);
				else
					amd_thresholding_resume(i);
			}
#endif

			/* Clear all errors. */
			wrmsr(mca_msr_ops.status(i), 0);
		}
		if (boot)
			mtx_unlock_spin(&mca_lock);

#ifdef DEV_APIC
		if (cmci_supported(mcg_cap) &&
		    PCPU_GET(cmci_mask) != 0 && boot)
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
void
mca_intr(void)
{
	uint64_t mcg_status;
	int count;
	bool lmcs, recoverable;

	if (!(cpu_feature & CPUID_MCA)) {
		/*
		 * Just print the values of the old Pentium registers
		 * and panic.
		 */
		printf("MC Type: 0x%jx  Address: 0x%jx\n",
		    (uintmax_t)rdmsr(MSR_P5_MC_TYPE),
		    (uintmax_t)rdmsr(MSR_P5_MC_ADDR));
		panic("Machine check exception");
	}

	/* Scan the banks and check for any non-recoverable errors. */
	mcg_status = rdmsr(MSR_MCG_STATUS);
	recoverable = (mcg_status & MCG_STATUS_RIPV) != 0;
	lmcs = (cpu_vendor_id != CPU_VENDOR_INTEL ||
	    (mcg_status & MCG_STATUS_LMCS));
	count = mca_scan(MCE, &recoverable);

	if (!recoverable) {
		/*
		 * Only panic if the error was detected local to this CPU.
		 * Some errors will assert a machine check on all CPUs, but
		 * only certain CPUs will find a valid bank to log.
		 */
		while (!lmcs && count == 0)
			cpu_spinwait();

		panic("Unrecoverable machine check exception");
	}

	/* Clear MCIP. */
	wrmsr(MSR_MCG_STATUS, mcg_status & ~MCG_STATUS_MCIP);
}

#ifdef DEV_APIC
/* Called for a CMCI (correctable machine check interrupt). */
void
cmc_intr(void)
{
	bool recoverable = true;

	/*
	 * Serialize MCA bank scanning to prevent collisions from
	 * sibling threads.
	 *
	 * If we found anything, log them to the console.
	 */
	if (mca_scan(CMCI, &recoverable) != 0)
		mca_process_records(CMCI);
}
#endif
