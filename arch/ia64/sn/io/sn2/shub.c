/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997, 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident  "$Revision: 1.167 $"

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <asm/smp.h>
#include <asm/irq.h>
#include <asm/hw_irq.h>
#include <asm/system.h>
#include <asm/sn/sgi.h>
#include <asm/uaccess.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/xtalk/xtalk.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/intr.h>
#include <asm/sn/sn2/shub_mmr_t.h>
#include <asm/sal.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/sndrv.h>

#define SHUB_NUM_ECF_REGISTERS 8

/*
 * A backport of the 2.5 scheduler is used by many vendors of 2.4-based
 * distributions.
 * We can only guess its presence by the lack of the SCHED_YIELD flag.
 * If the heuristic doesn't work, change this define by hand.
 */
#ifndef SCHED_YIELD
#define __HAVE_NEW_SCHEDULER    1
#endif


static uint32_t	shub_perf_counts[SHUB_NUM_ECF_REGISTERS];

static shubreg_t shub_perf_counts_regs[SHUB_NUM_ECF_REGISTERS] = {
	SH_PERFORMANCE_COUNTER0,
	SH_PERFORMANCE_COUNTER1,
	SH_PERFORMANCE_COUNTER2,
	SH_PERFORMANCE_COUNTER3,
	SH_PERFORMANCE_COUNTER4,
	SH_PERFORMANCE_COUNTER5,
	SH_PERFORMANCE_COUNTER6,
	SH_PERFORMANCE_COUNTER7
};

static inline void
shub_mmr_write(cnodeid_t cnode, shubreg_t reg, uint64_t val)
{
	int		   nasid = cnodeid_to_nasid(cnode);
	volatile uint64_t *addr = (uint64_t *)(GLOBAL_MMR_ADDR(nasid, reg));

	*addr = val;
	__ia64_mf_a();
}

static inline void
shub_mmr_write_iospace(cnodeid_t cnode, shubreg_t reg, uint64_t val)
{
	int		   nasid = cnodeid_to_nasid(cnode);

	REMOTE_HUB_S(nasid, reg, val);
}

static inline void
shub_mmr_write32(cnodeid_t cnode, shubreg_t reg, uint32_t val)
{
	int		   nasid = cnodeid_to_nasid(cnode);
	volatile uint32_t *addr = (uint32_t *)(GLOBAL_MMR_ADDR(nasid, reg));

	*addr = val;
	__ia64_mf_a();
}

static inline uint64_t
shub_mmr_read(cnodeid_t cnode, shubreg_t reg)
{
	int		  nasid = cnodeid_to_nasid(cnode);
	volatile uint64_t val;

	val = *(uint64_t *)(GLOBAL_MMR_ADDR(nasid, reg));
	__ia64_mf_a();

	return val;
}

static inline uint64_t
shub_mmr_read_iospace(cnodeid_t cnode, shubreg_t reg)
{
	int		  nasid = cnodeid_to_nasid(cnode);

	return REMOTE_HUB_L(nasid, reg);
}

static inline uint32_t
shub_mmr_read32(cnodeid_t cnode, shubreg_t reg)
{
	int		  nasid = cnodeid_to_nasid(cnode);
	volatile uint32_t val;

	val = *(uint32_t *)(GLOBAL_MMR_ADDR(nasid, reg));
	__ia64_mf_a();

	return val;
}

static int
reset_shub_stats(cnodeid_t cnode)
{
	int i;

	for (i=0; i < SHUB_NUM_ECF_REGISTERS; i++) {
		shub_perf_counts[i] = 0;
		shub_mmr_write32(cnode, shub_perf_counts_regs[i], 0);
	}
	return 0;
}

static int
configure_shub_stats(cnodeid_t cnode, unsigned long arg)
{
	uint64_t	*p = (uint64_t *)arg;
	uint64_t	i;
	uint64_t	regcnt;
	uint64_t	regval[2];

	if (copy_from_user((void *)&regcnt, p, sizeof(regcnt)))
	    return -EFAULT;

	for (p++, i=0; i < regcnt; i++, p += 2) {
		if (copy_from_user((void *)regval, (void *)p, sizeof(regval)))
		    return -EFAULT;
		if (regval[0] & 0x7) {
		    printk("Error: configure_shub_stats: unaligned address 0x%016lx\n", regval[0]);
		    return -EINVAL;
		}
		shub_mmr_write(cnode, (shubreg_t)regval[0], regval[1]);
	}
	return 0;
}

static int
capture_shub_stats(cnodeid_t cnode, uint32_t *counts)
{
	int 		i;

	for (i=0; i < SHUB_NUM_ECF_REGISTERS; i++) {
		counts[i] = shub_mmr_read32(cnode, shub_perf_counts_regs[i]);
	}
	return 0;
}

static int
shubstats_ioctl(struct inode *inode, struct file *file,
        unsigned int cmd, unsigned long arg)
{
        cnodeid_t       cnode;
        uint64_t        longarg;
	int		nasid;

#ifdef CONFIG_HWGFS_FS
        cnode = (cnodeid_t)file->f_dentry->d_fsdata;
#else
        cnode = (cnodeid_t)file->private_data;
#endif
        if (cnode < 0 || cnode >= numnodes)
                return -ENODEV;

        switch (cmd) {
	case SNDRV_SHUB_CONFIGURE:
		return configure_shub_stats(cnode, arg);
		break;

	case SNDRV_SHUB_RESETSTATS:
		reset_shub_stats(cnode);
		break;

	case SNDRV_SHUB_INFOSIZE:
		longarg = sizeof(shub_perf_counts);
		if (copy_to_user((void *)arg, &longarg, sizeof(longarg))) {
		    return -EFAULT;
		}
		break;

	case SNDRV_SHUB_GETSTATS:
		capture_shub_stats(cnode, shub_perf_counts);
		if (copy_to_user((void *)arg, shub_perf_counts,
				       	sizeof(shub_perf_counts))) {
		    return -EFAULT;
		}
		break;

	case SNDRV_SHUB_GETNASID:
		nasid = cnodeid_to_nasid(cnode);
		if (copy_to_user((void *)arg, &nasid,
				       	sizeof(nasid))) {
		    return -EFAULT;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

struct file_operations shub_mon_fops = {
	        ioctl:          shubstats_ioctl,
};

/*
 * "linkstatd" kernel thread to export SGI Numalink
 * stats via /proc/sgi_sn/linkstats
 */
static struct s_linkstats {
	uint64_t	hs_ni_sn_errors[2];
	uint64_t	hs_ni_cb_errors[2];
	uint64_t	hs_ni_retry_errors[2];
	int		hs_ii_up;
	uint64_t	hs_ii_sn_errors;
	uint64_t	hs_ii_cb_errors;
	uint64_t	hs_ii_retry_errors;
} *sn_linkstats;

static spinlock_t    sn_linkstats_lock;
static unsigned long sn_linkstats_starttime;
static unsigned long sn_linkstats_samples;
static unsigned long sn_linkstats_overflows;
static unsigned long sn_linkstats_update_msecs;

void
sn_linkstats_reset(unsigned long msecs)
{
	int		    cnode;
	uint64_t	    iio_wstat;
	uint64_t	    llp_csr_reg;

	spin_lock(&sn_linkstats_lock);
	memset(sn_linkstats, 0, numnodes * sizeof(struct s_linkstats));
	for (cnode=0; cnode < numnodes; cnode++) {
	    shub_mmr_write(cnode, SH_NI0_LLP_ERR, 0L);
	    shub_mmr_write(cnode, SH_NI1_LLP_ERR, 0L);
	    shub_mmr_write_iospace(cnode, IIO_LLP_LOG, 0L);

	    /* zero the II retry counter */
	    iio_wstat = shub_mmr_read_iospace(cnode, IIO_WSTAT);
	    iio_wstat &= 0xffffffffff00ffff; /* bits 23:16 */
	    shub_mmr_write_iospace(cnode, IIO_WSTAT, iio_wstat);

	    /* Check if the II xtalk link is working */
	    llp_csr_reg = shub_mmr_read_iospace(cnode, IIO_LLP_CSR);
	    if (llp_csr_reg & IIO_LLP_CSR_IS_UP)
		sn_linkstats[cnode].hs_ii_up = 1;
	}

    	sn_linkstats_update_msecs = msecs;
	sn_linkstats_samples = 0;
	sn_linkstats_overflows = 0;
	sn_linkstats_starttime = jiffies;
	spin_unlock(&sn_linkstats_lock);
}

int
linkstatd_thread(void *unused)
{
	int		    cnode;
	int		    overflows;
	uint64_t	    reg[2];
	uint64_t	    iio_wstat = 0L;
	ii_illr_u_t	    illr;
	struct s_linkstats  *lsp;
	struct task_struct  *tsk = current;

	daemonize();

#ifdef __HAVE_NEW_SCHEDULER
	set_user_nice(tsk, 19);
#else
	tsk->nice = 19;
#endif
	sigfillset(&tsk->blocked);
	strcpy(tsk->comm, "linkstatd");

	while(1) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(sn_linkstats_update_msecs * HZ / 1000);

		spin_lock(&sn_linkstats_lock);

		overflows = 0;
		for (lsp=sn_linkstats, cnode=0; cnode < numnodes; cnode++, lsp++) {
			reg[0] = shub_mmr_read(cnode, SH_NI0_LLP_ERR);
			reg[1] = shub_mmr_read(cnode, SH_NI1_LLP_ERR);
			if (lsp->hs_ii_up) {
			    illr = (ii_illr_u_t)shub_mmr_read_iospace(cnode, IIO_LLP_LOG);
			    iio_wstat = shub_mmr_read_iospace(cnode, IIO_WSTAT);
			}

			if (!overflows && (
			    (reg[0] & SH_NI0_LLP_ERR_RX_SN_ERR_COUNT_MASK) == 
				     SH_NI0_LLP_ERR_RX_SN_ERR_COUNT_MASK ||
			    (reg[0] & SH_NI0_LLP_ERR_RX_CB_ERR_COUNT_MASK) ==
			             SH_NI0_LLP_ERR_RX_CB_ERR_COUNT_MASK ||
			    (reg[1] & SH_NI1_LLP_ERR_RX_SN_ERR_COUNT_MASK) ==
			             SH_NI1_LLP_ERR_RX_SN_ERR_COUNT_MASK ||
			    (reg[1] & SH_NI1_LLP_ERR_RX_CB_ERR_COUNT_MASK) ==
			             SH_NI1_LLP_ERR_RX_CB_ERR_COUNT_MASK ||
			    (lsp->hs_ii_up && illr.ii_illr_fld_s.i_sn_cnt == IIO_LLP_SN_MAX) ||
			    (lsp->hs_ii_up && illr.ii_illr_fld_s.i_cb_cnt == IIO_LLP_CB_MAX))) {
			    overflows = 1;
			}

#define LINKSTAT_UPDATE(reg, cnt, mask, shift) cnt += (reg & mask) >> shift

			LINKSTAT_UPDATE(reg[0], lsp->hs_ni_sn_errors[0],
					SH_NI0_LLP_ERR_RX_SN_ERR_COUNT_MASK,
					SH_NI0_LLP_ERR_RX_SN_ERR_COUNT_SHFT);

			LINKSTAT_UPDATE(reg[1], lsp->hs_ni_sn_errors[1],
					SH_NI1_LLP_ERR_RX_SN_ERR_COUNT_MASK,
					SH_NI1_LLP_ERR_RX_SN_ERR_COUNT_SHFT);

			LINKSTAT_UPDATE(reg[0], lsp->hs_ni_cb_errors[0],
					SH_NI0_LLP_ERR_RX_CB_ERR_COUNT_MASK,
					SH_NI0_LLP_ERR_RX_CB_ERR_COUNT_SHFT);

			LINKSTAT_UPDATE(reg[1], lsp->hs_ni_cb_errors[1],
					SH_NI1_LLP_ERR_RX_CB_ERR_COUNT_MASK,
					SH_NI1_LLP_ERR_RX_CB_ERR_COUNT_SHFT);

			LINKSTAT_UPDATE(reg[0], lsp->hs_ni_retry_errors[0],
					SH_NI0_LLP_ERR_RETRY_COUNT_MASK,
					SH_NI0_LLP_ERR_RETRY_COUNT_SHFT);

			LINKSTAT_UPDATE(reg[1], lsp->hs_ni_retry_errors[1],
					SH_NI1_LLP_ERR_RETRY_COUNT_MASK,
					SH_NI1_LLP_ERR_RETRY_COUNT_SHFT);

			if (lsp->hs_ii_up) {
			    /* II sn and cb errors */
			    lsp->hs_ii_sn_errors += illr.ii_illr_fld_s.i_sn_cnt;
			    lsp->hs_ii_cb_errors += illr.ii_illr_fld_s.i_cb_cnt;
			    lsp->hs_ii_retry_errors += (iio_wstat & 0x0000000000ff0000) >> 16;

			    shub_mmr_write(cnode, SH_NI0_LLP_ERR, 0L);
			    shub_mmr_write(cnode, SH_NI1_LLP_ERR, 0L);
			    shub_mmr_write_iospace(cnode, IIO_LLP_LOG, 0L);

			    /* zero the II retry counter */
			    iio_wstat = shub_mmr_read_iospace(cnode, IIO_WSTAT);
			    iio_wstat &= 0xffffffffff00ffff; /* bits 23:16 */
			    shub_mmr_write_iospace(cnode, IIO_WSTAT, iio_wstat);
			}
		}

		sn_linkstats_samples++;
		if (overflows)
		    sn_linkstats_overflows++;

		spin_unlock(&sn_linkstats_lock);
	}
}

static char *
rate_per_minute(uint64_t val, uint64_t secs)
{
	static char	buf[16];
	uint64_t	a=0, b=0, c=0, d=0;

	if (secs) {
		a = 60 * val / secs;
		b = 60 * 10 * val / secs - (10 * a);
		c = 60 * 100 * val / secs - (100 * a) - (10 * b);
		d = 60 * 1000 * val / secs - (1000 * a) - (100 * b) - (10 * c);
	}
	sprintf(buf, "%4lu.%lu%lu%lu", a, b, c, d);

	return buf;
}

int
sn_linkstats_get(char *page)
{
	int			n = 0;
	int			cnode;
	int			nlport;
	struct s_linkstats	*lsp;
	nodepda_t		*npda;
	uint64_t	    	snsum = 0;
	uint64_t	    	cbsum = 0;
	uint64_t	    	retrysum = 0;
	uint64_t	    	snsum_ii = 0;
	uint64_t	    	cbsum_ii = 0;
	uint64_t	    	retrysum_ii = 0;
	uint64_t		secs;

	spin_lock(&sn_linkstats_lock);
	secs = (jiffies - sn_linkstats_starttime) / HZ;

	n += sprintf(page, "# SGI Numalink stats v1 : %lu samples, %lu o/flows, update %lu msecs\n",
		sn_linkstats_samples, sn_linkstats_overflows, sn_linkstats_update_msecs);

	n += sprintf(page+n, "%-37s %8s %8s %8s %8s\n",
		"# Numalink", "sn errs", "cb errs", "cb/min", "retries");

	for (lsp=sn_linkstats, cnode=0; cnode < numnodes; cnode++, lsp++) {
		npda = NODEPDA(cnode);

		/* two NL links on each SHub */
		for (nlport=0; nlport < 2; nlport++) {
			cbsum += lsp->hs_ni_cb_errors[nlport];
			snsum += lsp->hs_ni_sn_errors[nlport];
			retrysum += lsp->hs_ni_retry_errors[nlport];

			/* avoid buffer overrun (should be using seq_read API) */
			if (numnodes > 64)
				continue;

			n += sprintf(page + n, "/%s/link/%d  %8lu %8lu %8s %8lu\n",
			    npda->hwg_node_name, nlport+1, lsp->hs_ni_sn_errors[nlport],
			    lsp->hs_ni_cb_errors[nlport], 
			    rate_per_minute(lsp->hs_ni_cb_errors[nlport], secs),
			    lsp->hs_ni_retry_errors[nlport]);
		}

		/* one II port on each SHub (may not be connected) */
		if (lsp->hs_ii_up) {
		    n += sprintf(page + n, "/%s/xtalk   %8lu %8lu %8s %8lu\n",
			npda->hwg_node_name, lsp->hs_ii_sn_errors,
			lsp->hs_ii_cb_errors, rate_per_minute(lsp->hs_ii_cb_errors, secs),
			lsp->hs_ii_retry_errors);

		    snsum_ii += lsp->hs_ii_sn_errors;
		    cbsum_ii += lsp->hs_ii_cb_errors;
		    retrysum_ii += lsp->hs_ii_retry_errors;
		}
	}

	n += sprintf(page + n, "%-37s %8lu %8lu %8s %8lu\n",
		"System wide NL totals", snsum, cbsum, 
		rate_per_minute(cbsum, secs), retrysum);

	n += sprintf(page + n, "%-37s %8lu %8lu %8s %8lu\n",
		"System wide II totals", snsum_ii, cbsum_ii, 
		rate_per_minute(cbsum_ii, secs), retrysum_ii);

	spin_unlock(&sn_linkstats_lock);

	return n;
}

static int __init
linkstatd_init(void)
{
	if (!ia64_platform_is("sn2"))
		return -ENODEV;

	spin_lock_init(&sn_linkstats_lock);
	sn_linkstats = kmalloc(numnodes * sizeof(struct s_linkstats), GFP_KERNEL);
	sn_linkstats_reset(60000UL); /* default 60 second update interval */
	kernel_thread(linkstatd_thread, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);

	return 0;                                                                       
}

__initcall(linkstatd_init);
