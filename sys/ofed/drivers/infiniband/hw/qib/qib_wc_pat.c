/*
 * Copyright (c) 2008 QLogic Corporation. All rights reserved.
 * Copyright (c) 2006-2007 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *        Redistribution and use in source and binary forms, with or
 *        without modification, are permitted provided that the following
 *        conditions are met:
 *
 *         - Redistributions of source code must retain the above
 *           copyright notice, this list of conditions and the following
 *           disclaimer.
 *
 *         - Redistributions in binary form must reproduce the above
 *           copyright notice, this list of conditions and the following
 *           disclaimer in the documentation and/or other materials
 *           provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <asm/processor.h>
#include <linux/io.h>
#include <linux/pci.h>
#include "qib.h"
#include "qib_wc_pat.h"

static u32 old_pat_lo[NR_CPUS] = {0};
static u32 old_pat_hi[NR_CPUS] = {0};
static u32 new_pat_lo[NR_CPUS] = {0};
static unsigned int wc_enabled;

#define QIB_PAT_MASK    (0xFFFFF8FF)    /* PAT1 mask for the PAT MSR */
#define QIB_PAT_EXP     (0x00000400)    /* expected PAT1 value (WT) */
#define QIB_PAT_MOD     (0x00000100)    /* PAT1 value to select WC */
#define QIB_WC_MASK     (~_PAGE_PCD)    /* selects PAT1 for this page */
#define QIB_WC_FLAGS    (_PAGE_PWT)     /* selects PAT1 for this page */

#if defined(__i386__) || defined(__x86_64__)

#define X86_MSR_PAT_OFFSET  0x277

/*  Returns non-zero if we have a chipset write-combining problem */
static int have_wc_errata(void)
{
	struct pci_dev *dev;
	u8 rev;

	if (qib_wc_pat == 2)
		return 0;

	dev = pci_get_class(PCI_CLASS_BRIDGE_HOST << 8, NULL);
	if (dev != NULL) {
		/*
		 * ServerWorks LE chipsets < rev 6 have problems with
		 * write-combining.
		 */
		if (dev->vendor == PCI_VENDOR_ID_SERVERWORKS &&
		    dev->device == PCI_DEVICE_ID_SERVERWORKS_LE) {
			pci_read_config_byte(dev, PCI_CLASS_REVISION, &rev);
			if (rev <= 5) {
				qib_dbg("Serverworks LE rev < 6 detected. "
					  "Write-combining disabled\n");
				pci_dev_put(dev);
				return -ENOSYS;
			}
		}
		/* Intel 450NX errata # 23. Non ascending cacheline evictions
		   to write combining memory may resulting in data corruption
		 */
		if (dev->vendor == PCI_VENDOR_ID_INTEL &&
		    dev->device == PCI_DEVICE_ID_INTEL_82451NX) {
			qib_dbg("Intel 450NX MMC detected. "
				  "Write-combining disabled.\n");
			pci_dev_put(dev);
			return -ENOSYS;
		}
		pci_dev_put(dev);
	}
	return 0;
}

static void rd_old_pat(void *err)
{
	*(int *)err |= rdmsr_safe(X86_MSR_PAT_OFFSET,
				  &old_pat_lo[smp_processor_id()],
				  &old_pat_hi[smp_processor_id()]);
}

static void wr_new_pat(void *err)
{
	new_pat_lo[smp_processor_id()] =
		(old_pat_lo[smp_processor_id()] & QIB_PAT_MASK) |
		QIB_PAT_MOD;

	*(int *)err |= wrmsr_safe(X86_MSR_PAT_OFFSET,
				  new_pat_lo[smp_processor_id()],
				  old_pat_hi[smp_processor_id()]);
}

static void wr_old_pat(void *err)
{
	u32 cur_pat_lo, cur_pat_hi;

	*(int *)err |= rdmsr_safe(X86_MSR_PAT_OFFSET,
				  &cur_pat_lo, &cur_pat_hi);

	if (*(int *) err)
		goto done;

	/* only restore old PAT if it currently has the expected values */
	if (cur_pat_lo != new_pat_lo[smp_processor_id()] ||
	    cur_pat_hi != old_pat_hi[smp_processor_id()])
		goto done;

	*(int *)err |= wrmsr_safe(X86_MSR_PAT_OFFSET,
				  old_pat_lo[smp_processor_id()],
				  old_pat_hi[smp_processor_id()]);
done: ;
}

static int validate_old_pat(void)
{
	int ret = 0;
	int ncpus = num_online_cpus();
	int i;
	int onetime = 1;
	u32 my_pat1 = old_pat_lo[smp_processor_id()] & ~QIB_PAT_MASK;

	if (qib_wc_pat == 2)
		goto done;

	for (i = 0; i < ncpus; i++) {
		u32 this_pat1 = old_pat_lo[i] & ~QIB_PAT_MASK;
		if (this_pat1 != my_pat1) {
			qib_dbg("Inconsistent PAT1 settings across CPUs\n");
			ret = -ENOSYS;
			goto done;
		} else if (this_pat1 == QIB_PAT_MOD) {
			if (onetime) {
				qib_dbg("PAT1 has already been "
					"modified for WC (warning)\n");
				onetime = 0;
			}
		} else if (this_pat1 != QIB_PAT_EXP) {
			qib_dbg("PAT1 not in expected WT state\n");
			ret = -ENOSYS;
			goto done;
		}
	}
done:
	return ret;
}

static int read_and_modify_pat(void)
{
	int ret = 0;

	preempt_disable();
	rd_old_pat(&ret);
	if (!ret)
		smp_call_function(rd_old_pat, &ret, 1);
	if (ret)
		goto out;

	if (validate_old_pat())
		goto out;

	wr_new_pat(&ret);
	if (ret)
		goto out;

	smp_call_function(wr_new_pat, &ret, 1);
	BUG_ON(ret); /* have inconsistent PAT state */
out:
	preempt_enable();
	return ret;
}

static int restore_pat(void)
{
	int ret = 0;

	preempt_disable();
	wr_old_pat(&ret);
	if (!ret) {
		smp_call_function(wr_old_pat, &ret, 1);
		BUG_ON(ret); /* have inconsistent PAT state */
	}

	preempt_enable();
	return ret;
}

int qib_enable_wc_pat(void)
{
	struct cpuinfo_x86 *c = &cpu_data(0);
	int ret;

	if (wc_enabled)
		return 0;

	if (!cpu_has(c, X86_FEATURE_MSR) ||
	    !cpu_has(c, X86_FEATURE_PAT)) {
		qib_dbg("WC PAT not available on this processor\n");
		return -ENOSYS;
	}

	if (have_wc_errata())
		return -ENOSYS;

	ret = read_and_modify_pat();
	if (!ret)
		wc_enabled = 1;
	else
		qib_dbg("Failed to enable WC PAT\n");
	return ret ? -EIO  : 0;
}

void qib_disable_wc_pat(void)
{
	if (wc_enabled) {
		if (!restore_pat())
			wc_enabled = 0;
		else
			qib_dbg("Failed to disable WC PAT\n");
	}
}

pgprot_t pgprot_writecombine(pgprot_t _prot)
{
	return wc_enabled ?
		__pgprot(pgprot_val(_prot) | QIB_WC_FLAGS) :
		pgprot_noncached(_prot);
}

int qib_wc_pat_enabled(void)
{
	return wc_enabled;
}

#else /* !(defined(__i386__) || defined(__x86_64__)) */

int qib_enable_wc_pat(void){ return 0; }
void qib_disable_wc_pat(void){}

pgprot_t pgprot_writecombine(pgprot_t _prot)
{
	return pgprot_noncached(_prot);
}

int qib_wc_pat_enabled(void)
{
	return 0;
}

#endif
