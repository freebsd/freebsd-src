/*	$FreeBSD$ */
/*	$NetBSD: rf_acctrace.h,v 1.3 1999/02/05 00:06:06 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*****************************************************************************
 *
 * acctrace.h -- header file for acctrace.c
 *
 *****************************************************************************/


#ifndef _RF__RF_ACCTRACE_H_
#define _RF__RF_ACCTRACE_H_

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_hist.h>
#include <dev/raidframe/rf_etimer.h>

typedef struct RF_user_acc_stats_s {
	RF_uint64 suspend_ovhd_us;	/* us spent mucking in the
					 * access-suspension code */
	RF_uint64 map_us;	/* us spent mapping the access */
	RF_uint64 lock_us;	/* us spent locking & unlocking stripes,
				 * including time spent blocked */
	RF_uint64 dag_create_us;/* us spent creating the DAGs */
	RF_uint64 dag_retry_us;	/* _total_ us spent retrying the op -- not
				 * broken down into components */
	RF_uint64 exec_us;	/* us spent in DispatchDAG */
	RF_uint64 exec_engine_us;	/* us spent in engine, not including
					 * blocking time */
	RF_uint64 cleanup_us;	/* us spent tearing down the dag & maps, and
				 * generally cleaning up */
}       RF_user_acc_stats_t;

typedef struct RF_recon_acc_stats_s {
	RF_uint32 recon_start_to_fetch_us;
	RF_uint32 recon_fetch_to_return_us;
	RF_uint32 recon_return_to_submit_us;
}       RF_recon_acc_stats_t;

typedef struct RF_acctrace_entry_s {
	union {
		RF_user_acc_stats_t user;
		RF_recon_acc_stats_t recon;
	}       specific;
	RF_uint8 reconacc;	/* whether  this is a tracerec for a user acc
				 * or a recon acc */
	RF_uint64 xor_us;	/* us spent doing XORs */
	RF_uint64 q_us;		/* us spent doing XORs */
	RF_uint64 plog_us;	/* us spent waiting to stuff parity into log */
	RF_uint64 diskqueue_us;	/* _total_ us spent in disk queue(s), incl
				 * concurrent ops */
	RF_uint64 diskwait_us;	/* _total_ us spent waiting actually waiting
				 * on the disk, incl concurrent ops */
	RF_uint64 total_us;	/* total us spent on this access */
	RF_uint64 num_phys_ios;	/* number of physical I/Os invoked */
	RF_uint64 phys_io_us;	/* time of physical I/O */
	RF_Etimer_t tot_timer;	/* a timer used to compute total access time */
	RF_Etimer_t timer;	/* a generic timer val for timing events that
				 * live across procedure boundaries */
	RF_Etimer_t recon_timer;/* generic timer for recon stuff */
	RF_uint64 index;
}       RF_AccTraceEntry_t;

typedef struct RF_AccTotals_s {
	/* user acc stats */
	RF_uint64 suspend_ovhd_us;
	RF_uint64 map_us;
	RF_uint64 lock_us;
	RF_uint64 dag_create_us;
	RF_uint64 dag_retry_us;
	RF_uint64 exec_us;
	RF_uint64 exec_engine_us;
	RF_uint64 cleanup_us;
	RF_uint64 user_reccount;
	/* recon acc stats */
	RF_uint64 recon_start_to_fetch_us;
	RF_uint64 recon_fetch_to_return_us;
	RF_uint64 recon_return_to_submit_us;
	RF_uint64 recon_io_overflow_count;
	RF_uint64 recon_phys_io_us;
	RF_uint64 recon_num_phys_ios;
	RF_uint64 recon_diskwait_us;
	RF_uint64 recon_reccount;
	/* trace entry stats */
	RF_uint64 xor_us;
	RF_uint64 q_us;
	RF_uint64 plog_us;
	RF_uint64 diskqueue_us;
	RF_uint64 diskwait_us;
	RF_uint64 total_us;
	RF_uint64 num_log_ents;
	RF_uint64 phys_io_overflow_count;
	RF_uint64 num_phys_ios;
	RF_uint64 phys_io_us;
	RF_uint64 bigvals;
	/* histograms */
	RF_Hist_t dw_hist[RF_HIST_NUM_BUCKETS];
	RF_Hist_t tot_hist[RF_HIST_NUM_BUCKETS];
}       RF_AccTotals_t;
#if RF_UTILITY == 0
RF_DECLARE_EXTERN_MUTEX(rf_tracing_mutex)
#endif				/* RF_UTILITY == 0 */

	int     rf_ConfigureAccessTrace(RF_ShutdownList_t ** listp);
	void    rf_LogTraceRec(RF_Raid_t * raid, RF_AccTraceEntry_t * rec);
	void    rf_FlushAccessTraceBuf(void);

#endif				/* !_RF__RF_ACCTRACE_H_ */
