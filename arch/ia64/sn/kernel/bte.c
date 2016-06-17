/*
 *
 *
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <linux/config.h>
#include <asm/sn/nodepda.h>
#include <asm/sn/addrs.h>
#include <asm/sn/arch.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/pda.h>
#include <asm/sn/sn2/shubio.h>
#include <asm/nodedata.h>

#include <linux/bootmem.h>
#include <linux/string.h>
#include <linux/sched.h>

#include <asm/sn/bte.h>


/*
 * The base address of for each set of bte registers.
 */
static int bte_offsets[] = { IIO_IBLS0, IIO_IBLS1 };


/************************************************************************
 * Block Transfer Engine copy related functions.
 *
 ***********************************************************************/


/*
 * bte_copy(src, dest, len, mode, notification)
 *
 * Use the block transfer engine to move kernel memory from src to dest
 * using the assigned mode.
 *
 * Paramaters:
 *   src - physical address of the transfer source.
 *   dest - physical address of the transfer destination.
 *   len - number of bytes to transfer from source to dest.
 *   mode - hardware defined.  See reference information
 *          for IBCT0/1 in the SHUB Programmers Reference
 *   notification - kernel virtual address of the notification cache
 *                  line.  If NULL, the default is used and
 *                  the bte_copy is synchronous.
 *
 * NOTE:  This function requires src, dest, and len to
 * be cacheline aligned.
 */
bte_result_t
bte_copy(u64 src, u64 dest, u64 len, u64 mode, void *notification)
{
	int bte_to_use;
	u64 transfer_size;
	struct bteinfo_s *bte;
	bte_result_t bte_status;
	unsigned long irq_flags;


	BTE_PRINTK(("bte_copy(0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%p)\n",
		    src, dest, len, mode, notification));

	if (len == 0) {
		return BTE_SUCCESS;
	}

	ASSERT(!((len & L1_CACHE_MASK) ||
		 (src & L1_CACHE_MASK) || (dest & L1_CACHE_MASK)));
	ASSERT(len < ((BTE_LEN_MASK + 1) << L1_CACHE_SHIFT));

	do {
		local_irq_save(irq_flags);

		bte_to_use = 0;
		/* Attempt to lock one of the BTE interfaces. */
		while ((bte_to_use < BTES_PER_NODE) &&
		       BTE_LOCK_IF_AVAIL(bte_to_use)) {
			bte_to_use++;
		}

		if (bte_to_use < BTES_PER_NODE) {
			break;
		}

		local_irq_restore(irq_flags);

		if (!(mode & BTE_WACQUIRE)) {
			return BTEFAIL_NOTAVAIL;
		}

		/* Wait until a bte is available. */
		udelay(10);
	} while (1);

	bte = pda.cpu_bte_if[bte_to_use];
	BTE_PRINTKV(("Got a lock on bte %d\n", bte_to_use));


	if (notification == NULL) {
		/* User does not want to be notified. */
		bte->most_rcnt_na = &bte->notify;
	} else {
		bte->most_rcnt_na = notification;
	}

	/* Calculate the number of cache lines to transfer. */
	transfer_size = ((len >> L1_CACHE_SHIFT) & BTE_LEN_MASK);

	/* Initialize the notification to a known value. */
	*bte->most_rcnt_na = -1L;

	/* Set the status reg busy bit and transfer length */
	BTE_PRINTKV(("IBLS - HUB_S(0x%p, 0x%lx)\n",
		     BTEREG_LNSTAT_ADDR, IBLS_BUSY | transfer_size));
	HUB_S(BTEREG_LNSTAT_ADDR, (IBLS_BUSY | transfer_size));

	/* Set the source and destination registers */
	BTE_PRINTKV(("IBSA - HUB_S(0x%p, 0x%lx)\n", BTEREG_SRC_ADDR,
		     (TO_PHYS(src))));
	HUB_S(BTEREG_SRC_ADDR, (TO_PHYS(src)));
	BTE_PRINTKV(("IBDA - HUB_S(0x%p, 0x%lx)\n", BTEREG_DEST_ADDR,
		     (TO_PHYS(dest))));
	HUB_S(BTEREG_DEST_ADDR, (TO_PHYS(dest)));

	/* Set the notification register */
	BTE_PRINTKV(("IBNA - HUB_S(0x%p, 0x%lx)\n", BTEREG_NOTIF_ADDR,
		     (TO_PHYS(ia64_tpa(bte->most_rcnt_na)))));
	HUB_S(BTEREG_NOTIF_ADDR, (TO_PHYS(ia64_tpa(bte->most_rcnt_na))));


	/* Initiate the transfer */
	BTE_PRINTK(("IBCT - HUB_S(0x%p, 0x%lx)\n", BTEREG_CTRL_ADDR,
		     BTE_VALID_MODE(mode)));
	HUB_S(BTEREG_CTRL_ADDR, BTE_VALID_MODE(mode));

	spin_unlock_irqrestore(&bte->spinlock, irq_flags);


	if (notification != NULL) {
		return BTE_SUCCESS;
	}

	while (*bte->most_rcnt_na == -1UL) {
	}


	BTE_PRINTKV((" Delay Done.  IBLS = 0x%lx, most_rcnt_na = 0x%lx\n",
				HUB_L(BTEREG_LNSTAT_ADDR), *bte->most_rcnt_na));

	if (*bte->most_rcnt_na & IBLS_ERROR) {
		bte_status = *bte->most_rcnt_na & ~IBLS_ERROR;
		*bte->most_rcnt_na = 0L;
	} else {
		bte_status = BTE_SUCCESS;
	}
	BTE_PRINTK(("Returning status is 0x%lx and most_rcnt_na is 0x%lx\n",
				HUB_L(BTEREG_LNSTAT_ADDR), *bte->most_rcnt_na));

	return bte_status;
}


/*
 * bte_unaligned_copy(src, dest, len, mode)
 *
 * use the block transfer engine to move kernel
 * memory from src to dest using the assigned mode.
 *
 * Paramaters:
 *   src - physical address of the transfer source.
 *   dest - physical address of the transfer destination.
 *   len - number of bytes to transfer from source to dest.
 *   mode - hardware defined.  See reference information
 *          for IBCT0/1 in the SGI documentation.
 *
 * NOTE: If the source, dest, and len are all cache line aligned,
 * then it would be _FAR_ preferrable to use bte_copy instead.
 */
bte_result_t
bte_unaligned_copy(u64 src, u64 dest, u64 len, u64 mode)
{
	int destFirstCacheOffset;
	u64 headBteSource;
	u64 headBteLen;
	u64 headBcopySrcOffset;
	u64 headBcopyDest;
	u64 headBcopyLen;
	u64 footBteSource;
	u64 footBteLen;
	u64 footBcopyDest;
	u64 footBcopyLen;
	bte_result_t rv;
	char *bteBlock;

	if (len == 0) {
		return BTE_SUCCESS;
	}

	/* temporary buffer used during unaligned transfers */
	bteBlock = pda.cpu_bte_if[0]->scratch_buf;

	headBcopySrcOffset = src & L1_CACHE_MASK;
	destFirstCacheOffset = dest & L1_CACHE_MASK;

	/*
	 * At this point, the transfer is broken into
	 * (up to) three sections.  The first section is
	 * from the start address to the first physical
	 * cache line, the second is from the first physical
	 * cache line to the last complete cache line,
	 * and the third is from the last cache line to the
	 * end of the buffer.  The first and third sections
	 * are handled by bte copying into a temporary buffer
	 * and then bcopy'ing the necessary section into the
	 * final location.  The middle section is handled with
	 * a standard bte copy.
	 *
	 * One nasty exception to the above rule is when the
	 * source and destination are not symetrically
	 * mis-aligned.  If the source offset from the first
	 * cache line is different from the destination offset,
	 * we make the first section be the entire transfer
	 * and the bcopy the entire block into place.
	 */
	if (headBcopySrcOffset == destFirstCacheOffset) {

		/*
		 * Both the source and destination are the same
		 * distance from a cache line boundary so we can
		 * use the bte to transfer the bulk of the
		 * data.
		 */
		headBteSource = src & ~L1_CACHE_MASK;
		headBcopyDest = dest;
		if (headBcopySrcOffset) {
			headBcopyLen =
			    (len >
			     (L1_CACHE_BYTES -
			      headBcopySrcOffset) ? L1_CACHE_BYTES
			     - headBcopySrcOffset : len);
			headBteLen = L1_CACHE_BYTES;
		} else {
			headBcopyLen = 0;
			headBteLen = 0;
		}

		if (len > headBcopyLen) {
			footBcopyLen =
			    (len - headBcopyLen) & L1_CACHE_MASK;
			footBteLen = L1_CACHE_BYTES;

			footBteSource = src + len - footBcopyLen;
			footBcopyDest = dest + len - footBcopyLen;

			if (footBcopyDest ==
			    (headBcopyDest + headBcopyLen)) {
				/*
				 * We have two contigous bcopy
				 * blocks.  Merge them.
				 */
				headBcopyLen += footBcopyLen;
				headBteLen += footBteLen;
			} else if (footBcopyLen > 0) {
				rv = bte_copy(footBteSource,
					      ia64_tpa(bteBlock),
					      footBteLen, mode, NULL);
				if (rv != BTE_SUCCESS) {
					return rv;
				}


				memcpy(__va(footBcopyDest),
				       (char *) bteBlock, footBcopyLen);
			}
		} else {
			footBcopyLen = 0;
			footBteLen = 0;
		}

		if (len > (headBcopyLen + footBcopyLen)) {
			/* now transfer the middle. */
			rv = bte_copy((src + headBcopyLen),
				      (dest +
				       headBcopyLen),
				      (len - headBcopyLen -
				       footBcopyLen), mode, NULL);
			if (rv != BTE_SUCCESS) {
				return rv;
			}

		}
	} else {


		/*
		 * The transfer is not symetric, we will
		 * allocate a buffer large enough for all the
		 * data, bte_copy into that buffer and then
		 * bcopy to the destination.
		 */

		/* Add the leader from source */
		headBteLen = len + (src & L1_CACHE_MASK);
		/* Add the trailing bytes from footer. */
		headBteLen +=
		    L1_CACHE_BYTES - (headBteLen & L1_CACHE_MASK);
		headBteSource = src & ~L1_CACHE_MASK;
		headBcopySrcOffset = src & L1_CACHE_MASK;
		headBcopyDest = dest;
		headBcopyLen = len;
	}

	if (headBcopyLen > 0) {
		rv = bte_copy(headBteSource,
			      ia64_tpa(bteBlock), headBteLen, mode, NULL);
		if (rv != BTE_SUCCESS) {
			return rv;
		}

		memcpy(__va(headBcopyDest), ((char *) bteBlock +
					     headBcopySrcOffset),
		       headBcopyLen);
	}
	return BTE_SUCCESS;
}


/************************************************************************
 * Block Transfer Engine initialization functions.
 *
 ***********************************************************************/


/*
 * bte_init_node(nodepda, cnode)
 *
 * Initialize the nodepda structure with BTE base addresses and
 * spinlocks.
 */
void
bte_init_node(nodepda_t * mynodepda, cnodeid_t cnode)
{
	int i;


	/*
	 * Indicate that all the block transfer engines on this node
	 * are available.
	 */

	/*
	 * Allocate one bte_recover_t structure per node.  It holds
	 * the recovery lock for node.  All the bte interface structures
	 * will point at this one bte_recover structure to get the lock.
	 */
	spin_lock_init(&mynodepda->bte_recovery_lock);
	init_timer(&mynodepda->bte_recovery_timer);
	mynodepda->bte_recovery_timer.function = bte_error_handler;
	mynodepda->bte_recovery_timer.data = (unsigned long) mynodepda;

	for (i = 0; i < BTES_PER_NODE; i++) {
		/* >>> Don't know why the 0x1800000L is here.  Robin */
		mynodepda->bte_if[i].bte_base_addr =
		    (char *) LOCAL_MMR_ADDR(bte_offsets[i] | 0x1800000L);

		/*
		 * Initialize the notification and spinlock
		 * so the first transfer can occur.
		 */
		mynodepda->bte_if[i].most_rcnt_na =
		    &(mynodepda->bte_if[i].notify);
		mynodepda->bte_if[i].notify = 0L;
		spin_lock_init(&mynodepda->bte_if[i].spinlock);

		mynodepda->bte_if[i].scratch_buf =
		    alloc_bootmem_node(NODE_DATA(cnode), BTE_MAX_XFER);
		mynodepda->bte_if[i].bte_cnode = cnode;
		mynodepda->bte_if[i].bte_error_count = 0;
		mynodepda->bte_if[i].bte_num = i;
		mynodepda->bte_if[i].cleanup_active = 0;
		mynodepda->bte_if[i].bh_error = 0;
	}

}

/*
 * bte_init_cpu()
 *
 * Initialize the cpupda structure with pointers to the
 * nodepda bte blocks.
 *
 */
void
bte_init_cpu(void)
{
	/* Called by setup.c as each cpu is being added to the nodepda */
	if (local_node_data->active_cpu_count & 0x1) {
		pda.cpu_bte_if[0] = &(nodepda->bte_if[0]);
		pda.cpu_bte_if[1] = &(nodepda->bte_if[1]);
	} else {
		pda.cpu_bte_if[0] = &(nodepda->bte_if[1]);
		pda.cpu_bte_if[1] = &(nodepda->bte_if[0]);
	}
}
