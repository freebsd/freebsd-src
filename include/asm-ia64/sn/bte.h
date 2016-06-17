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


#ifndef _ASM_IA64_SN_BTE_H
#define _ASM_IA64_SN_BTE_H

#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/cache.h>
#include <asm/sn/io.h>
#include <asm/delay.h>


/* #define BTE_DEBUG */
/* #define BTE_DEBUG_VERBOSE */

#ifdef BTE_DEBUG
#  define BTE_PRINTK(x) printk x	/* Terse */
#  ifdef BTE_DEBUG_VERBOSE
#    define BTE_PRINTKV(x) printk x	/* Verbose */
#  else
#    define BTE_PRINTKV(x)
#  endif /* BTE_DEBUG_VERBOSE */
#else
#  define BTE_PRINTK(x)
#  define BTE_PRINTKV(x)
#endif	/* BTE_DEBUG */


#ifndef L1_CACHE_MASK
#define L1_CACHE_MASK		(L1_CACHE_BYTES - 1)
#endif

#ifndef L1_CACHE_ALIGNED
#define L1_CACHE_ALIGNED(_p)	(((u64)(_p) & L1_CACHE_MASK) == 0)
#endif


/* BTE status register only supports 16 bits for length field */
#define BTE_LEN_BITS (16)
#define BTE_LEN_MASK ((1 << BTE_LEN_BITS) - 1)
#define BTE_MAX_XFER ((1 << BTE_LEN_BITS) * L1_CACHE_BYTES)


/* Define hardware */
#define BTES_PER_NODE 2


/* Define hardware modes */
#define BTE_NOTIFY (IBCT_NOTIFY)
#define BTE_NORMAL BTE_NOTIFY
#define BTE_ZERO_FILL (BTE_NOTIFY | IBCT_ZFIL_MODE)
/* Use a reserved bit to let the caller specify a wait for any BTE */
#define BTE_WACQUIRE (0x4000)
/* macro to force the IBCT0 value valid */
#define BTE_VALID_MODE(x) ((x) & (IBCT_NOTIFY | IBCT_ZFIL_MODE))


/*
 * Handle locking of the bte interfaces.
 *
 * All transfers spinlock the interface before setting up the SHUB
 * registers.  Sync transfers hold the lock until all processing is
 * complete.  Async transfers release the lock as soon as the transfer
 * is initiated.
 *
 * To determine if an interface is available, we must check both the
 * busy bit and the spinlock for that interface.
 */
#define BTE_LOCK_IF_AVAIL(_x) (\
	(*pda.cpu_bte_if[_x]->most_rcnt_na & (IBLS_BUSY | IBLS_ERROR)) && \
	(!(spin_trylock(&(pda.cpu_bte_if[_x]->spinlock)))) \
	)

/*
 * Some macros to simplify reading.
 * Start with macros to locate the BTE control registers.
 */
#define BTEREG_LNSTAT_ADDR ((u64 *)(bte->bte_base_addr))
#define BTEREG_SRC_ADDR ((u64 *)(bte->bte_base_addr + BTEOFF_SRC))
#define BTEREG_DEST_ADDR ((u64 *)(bte->bte_base_addr + BTEOFF_DEST))
#define BTEREG_CTRL_ADDR ((u64 *)(bte->bte_base_addr + BTEOFF_CTRL))
#define BTEREG_NOTIF_ADDR ((u64 *)(bte->bte_base_addr + BTEOFF_NOTIFY))


/* Possible results from bte_copy and bte_unaligned_copy */
typedef enum {
	BTE_SUCCESS,		/* 0 is success */
	BTEFAIL_NOTAVAIL,	/* BTE not available */
	BTEFAIL_POISON,		/* poison page */
	BTEFAIL_PROT,		/* Protection violation */
	BTEFAIL_ACCESS,		/* access error */
	BTEFAIL_TOUT,		/* Time out */
	BTEFAIL_XTERR,		/* Diretory error */
	BTEFAIL_DIR,		/* Diretory error */
	BTEFAIL_ERROR,		/* Generic error */
} bte_result_t;


/*
 * Structure defining a bte.  An instance of this
 * structure is created in the nodepda for each
 * bte on that node (as defined by BTES_PER_NODE)
 * This structure contains everything necessary
 * to work with a BTE.
 */
struct bteinfo_s {
	u64 volatile notify ____cacheline_aligned;
	char *bte_base_addr ____cacheline_aligned;
	spinlock_t spinlock;
	cnodeid_t bte_cnode;	/* cnode                            */
	int bte_error_count;	/* Number of errors encountered     */
	int bte_num;		/* 0 --> BTE0, 1 --> BTE1           */
	int cleanup_active;	/* Interface is locked for cleanup  */
	volatile bte_result_t bh_error;	/* error while processing   */
	u64 volatile *most_rcnt_na;
	void *scratch_buf;	/* Node local scratch buffer        */
};


/*
 * Function prototypes (functions defined in bte.c, used elsewhere)
 */
extern bte_result_t bte_copy(u64, u64, u64, u64, void *);
extern bte_result_t bte_unaligned_copy(u64, u64, u64, u64);
extern void bte_error_handler(unsigned long);


/*
 * The following is the prefered way of calling bte_unaligned_copy
 * If the copy is fully cache line aligned, then bte_copy is
 * used instead.  Since bte_copy is inlined, this saves a call
 * stack.  NOTE: bte_copy is called synchronously and does block
 * until the transfer is complete.  In order to get the asynch
 * version of bte_copy, you must perform this check yourself.
 */
#define BTE_UNALIGNED_COPY(src, dest, len, mode)                        \
	(((len & L1_CACHE_MASK) || (src & L1_CACHE_MASK) ||             \
	  (dest & L1_CACHE_MASK)) ?                                     \
	 bte_unaligned_copy(src, dest, len, mode) :              	\
	 bte_copy(src, dest, len, mode, NULL))


#endif	/* _ASM_IA64_SN_BTE_H */
