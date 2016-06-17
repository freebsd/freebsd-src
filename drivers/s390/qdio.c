/*
 *
 * linux/drivers/s390/qdio.c
 *
 * Linux for S/390 QDIO base support, Hipersocket base support
 * version 2
 *
 * Copyright 2000,2002 IBM Corporation
 * Author(s): Utz Bacher <utz.bacher@de.ibm.com>
 *
 * Restriction: only 63 iqdio subchannels would have its own indicator,
 * after that, subsequent subchannels share one indicator
 *
 *
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* we want the eyecatcher to be at the top of the code */
void volatile qdio_eyecatcher(void)
{
	return;
}

#include <linux/config.h>

#include <linux/module.h>

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/mm.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <asm/semaphore.h>
#include <asm/page.h>

#include <asm/debug.h>

#include <asm/qdio.h>

#define VERSION_QDIO_C "$Revision: 1.145 $"

/****************** MODULE PARAMETER VARIABLES ********************/
MODULE_AUTHOR("Utz Bacher <utz.bacher@de.ibm.com>");
MODULE_DESCRIPTION("QDIO base support version 2, " \
		   "Copyright 2000 IBM Corporation");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,12))
MODULE_LICENSE("GPL");
#endif

/******************** HERE WE GO ***********************************/

static const char *version="QDIO base support version 2 ("
	VERSION_QDIO_C "/" VERSION_QDIO_H ")";

#ifdef QDIO_PERFORMANCE_STATS
static int proc_perf_file_registration;
unsigned long i_p_c=0,i_p_nc=0,o_p_c=0,o_p_nc=0,ii_p_c=0,ii_p_nc=0;

static struct {
	unsigned int tl_runs;

	unsigned int siga_outs;
	unsigned int siga_ins;
	unsigned int siga_syncs;
	unsigned int pcis;
	unsigned int thinints;
	unsigned int fast_reqs;

	__u64 start_time_outbound;
	unsigned int outbound_cnt;
	unsigned int outbound_time;
	__u64 start_time_inbound;
	unsigned int inbound_cnt;
	unsigned int inbound_time;
} perf_stats;

#endif /* QDIO_PERFORMANCE_STATS */

static int hydra_thinints=0;

static int indicator_used[INDICATORS_PER_CACHELINE];
static __u32 * volatile indicators;
static __u32 volatile spare_indicator;

static debug_info_t *qdio_dbf_setup=NULL;
static debug_info_t *qdio_dbf_sbal=NULL;
static debug_info_t *qdio_dbf_trace=NULL;
static debug_info_t *qdio_dbf_sense=NULL;
#ifdef QDIO_DBF_LIKE_HELL
static debug_info_t *qdio_dbf_slsb_out=NULL;
static debug_info_t *qdio_dbf_slsb_in=NULL;
#endif /* QDIO_DBF_LIKE_HELL */

static qdio_irq_t *first_irq[QDIO_IRQ_BUCKETS]={
	[0 ... (QDIO_IRQ_BUCKETS-1)] = NULL
};
static rwlock_t irq_list_lock[QDIO_IRQ_BUCKETS]={
	[0 ... (QDIO_IRQ_BUCKETS-1)] = RW_LOCK_UNLOCKED
};

static struct semaphore init_sema;

static qdio_chsc_area_t *chsc_area;
/* iQDIO stuff: */
static volatile qdio_q_t *tiq_list=NULL; /* volatile as it could change
					   during a while loop */
static spinlock_t ttiq_list_lock=SPIN_LOCK_UNLOCKED;
static int register_thinint_result;
static void tiqdio_tl(unsigned long);
static DECLARE_TASKLET(tiqdio_tasklet,tiqdio_tl,0);

#define HEXDUMP16(importance,header,ptr) \
QDIO_PRINT_##importance(header "%02x %02x %02x %02x  " \
			"%02x %02x %02x %02x  %02x %02x %02x %02x  " \
			"%02x %02x %02x %02x\n",*(((char*)ptr)), \
			*(((char*)ptr)+1),*(((char*)ptr)+2), \
			*(((char*)ptr)+3),*(((char*)ptr)+4), \
			*(((char*)ptr)+5),*(((char*)ptr)+6), \
			*(((char*)ptr)+7),*(((char*)ptr)+8), \
			*(((char*)ptr)+9),*(((char*)ptr)+10), \
			*(((char*)ptr)+11),*(((char*)ptr)+12), \
			*(((char*)ptr)+13),*(((char*)ptr)+14), \
			*(((char*)ptr)+15)); \
QDIO_PRINT_##importance(header "%02x %02x %02x %02x  %02x %02x %02x %02x  " \
			"%02x %02x %02x %02x  %02x %02x %02x %02x\n", \
			*(((char*)ptr)+16),*(((char*)ptr)+17), \
			*(((char*)ptr)+18),*(((char*)ptr)+19), \
			*(((char*)ptr)+20),*(((char*)ptr)+21), \
			*(((char*)ptr)+22),*(((char*)ptr)+23), \
			*(((char*)ptr)+24),*(((char*)ptr)+25), \
			*(((char*)ptr)+26),*(((char*)ptr)+27), \
			*(((char*)ptr)+28),*(((char*)ptr)+29), \
			*(((char*)ptr)+30),*(((char*)ptr)+31));

#define atomic_swap(a,b) xchg((int*)a.counter,b)

/* not a macro, as one of the arguments is atomic_read */
static inline int qdio_min(int a,int b)
{
	if (a<b)
		return a;
	else
		return b;
}

/* unlikely as the later the better */
#define SYNC_MEMORY if (unlikely(q->siga_sync)) qdio_siga_sync_q(q)
#define SYNC_MEMORY_ALL if (unlikely(q->siga_sync)) \
	qdio_siga_sync(q,~0U,~0U)
#define SYNC_MEMORY_ALL_OUTB if (unlikely(q->siga_sync)) \
	qdio_siga_sync(q,~0U,0)

#define NOW qdio_get_micros()
#define SAVE_TIMESTAMP(q) q->timing.last_transfer_time=NOW
#define GET_SAVED_TIMESTAMP(q) (q->timing.last_transfer_time)
#define SAVE_FRONTIER(q,val) q->last_move_ftc=val
#define GET_SAVED_FRONTIER(q) (q->last_move_ftc)

#define MY_MODULE_STRING(x) #x

#ifdef QDIO_32_BIT
#define QDIO_GET_32BIT_ADDR(x) ((__u32)(long)x)
#else /* QDIO_32_BIT */
#define QDIO_GET_32BIT_ADDR(x) ((__u32)(unsigned long)x)
#endif /* QDIO_32_BIT */

#define QDIO_PFIX_GET_ADDR(x) ((flags&QDIO_PFIX) ? \
			       pfix_get_addr(x):((unsigned long)x))

/***************** SCRUBBER HELPER ROUTINES **********************/

static inline volatile __u64 qdio_get_micros(void)
{
        __u64 time;

        asm volatile ("STCK %0" : "=m" (time));
        return time>>12; /* time>>12 is microseconds*/
}
static inline unsigned long qdio_get_millis(void)
{
	return (unsigned long)(qdio_get_micros()>>12);
}

static __inline__ int atomic_return_add (int i, atomic_t *v)
{
	int old, new;
	__CS_LOOP(old, new, v, i, "ar");
	return old;
}

static void qdio_wait_nonbusy(unsigned int timeout)
{
        unsigned int start;
        char dbf_text[15];

	sprintf(dbf_text,"wtnb%4x",timeout);
	QDIO_DBF_TEXT3(0,trace,dbf_text);

	start=qdio_get_millis();
	for (;;) {
		set_task_state(current,TASK_INTERRUPTIBLE);
		if (qdio_get_millis()-start>timeout) {
			goto out;
		}
		schedule_timeout(((start+timeout-qdio_get_millis())>>10)*HZ);
	}
out:
	set_task_state(current,TASK_RUNNING);
}

static int qdio_wait_for_no_use_count(atomic_t *use_count)
{
	unsigned long start;

	QDIO_DBF_TEXT3(0,trace,"wtnousec");
	start=qdio_get_millis();
	for (;;) {
		if (qdio_get_millis()-start>QDIO_NO_USE_COUNT_TIMEOUT) {
			QDIO_DBF_TEXT1(1,trace,"WTNOUSTO");
			return -ETIME;
		}
		if (!atomic_read(use_count)) {
			QDIO_DBF_TEXT3(0,trace,"wtnoused");
			return 0;
		}
		qdio_wait_nonbusy(QDIO_NO_USE_COUNT_TIME);
	}
}

/* unfortunately, we can't just xchg the values; in do_QDIO we want to reserve
 * the q in any case, so that we'll not be interrupted when we are in
 * qdio_mark_tiq... shouldn't have a really bad impact, as reserving almost
 * ever works (last famous words) */
static inline int qdio_reserve_q(qdio_q_t *q)
{
	return atomic_return_add(1,&q->use_count);
}

static inline void qdio_release_q(qdio_q_t *q)
{
	atomic_dec(&q->use_count);
}
#ifdef QDIO_DBF_LIKE_HELL 
#define set_slsb(x,y) \
  if(q->queue_type==QDIO_TRACE_QTYPE) { \
        if(q->is_input_q) { \
            QDIO_DBF_HEX2(0,slsb_in,&q->slsb,QDIO_MAX_BUFFERS_PER_Q); \
        } else { \
            QDIO_DBF_HEX2(0,slsb_out,&q->slsb,QDIO_MAX_BUFFERS_PER_Q); \
        } \
  } \
  qdio_set_slsb(x,y); \
  if(q->queue_type==QDIO_TRACE_QTYPE) { \
        if(q->is_input_q) { \
            QDIO_DBF_HEX2(0,slsb_in,&q->slsb,QDIO_MAX_BUFFERS_PER_Q); \
        } else { \
            QDIO_DBF_HEX2(0,slsb_out,&q->slsb,QDIO_MAX_BUFFERS_PER_Q); \
        } \
  }
#else /* QDIO_DBF_LIKE_HELL */
#define set_slsb(x,y) qdio_set_slsb(x,y)
#endif /* QDIO_DBF_LIKE_HELL */
static volatile inline void qdio_set_slsb(volatile char *slsb,
					  unsigned char value)
{
	xchg((char*)slsb,value);
}

static inline int qdio_siga_sync(qdio_q_t *q,
				 unsigned int gpr2,
				 unsigned int gpr3)
{
	int cc;

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"sigasync");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(int));
	QDIO_DBF_HEX4(0,trace,&gpr2,sizeof(int));
	QDIO_DBF_HEX4(0,trace,&gpr3,sizeof(int));
#endif /* QDIO_DBF_LIKE_HELL */

#ifdef QDIO_PERFORMANCE_STATS
	perf_stats.siga_syncs++;
#endif /* QDIO_PERFORMANCE_STATS */

#ifdef QDIO_32_BIT
	asm volatile (
		"lhi	0,2	\n\t"
		"lr	1,%1	\n\t"
		"lr	2,%2	\n\t"
		"lr	3,%3	\n\t"
		"siga   0	\n\t"
		"ipm	%0	\n\t"
		"srl	%0,28	\n\t"
		: "=d" (cc)
		: "d" (0x10000|q->irq), "d" (gpr2), "d" (gpr3)
		: "cc", "0", "1", "2", "3"
		);
#else /* QDIO_32_BIT */
	asm volatile (
		"lghi	0,2	\n\t"
		"llgfr	1,%1	\n\t"
		"llgfr	2,%2	\n\t"
		"llgfr	3,%3	\n\t"
		"siga   0	\n\t"
		"ipm	%0	\n\t"
		"srl	%0,28	\n\t"
		: "=d" (cc)
		: "d" (0x10000|q->irq), "d" (gpr2), "d" (gpr3)
		: "cc", "0", "1", "2", "3"
		);
#endif /* QDIO_32_BIT */

	if (cc) {
#ifndef QDIO_DBF_LIKE_HELL
		/* when QDIO_DBF_LIKE_HELL, we put that already out */
		QDIO_DBF_TEXT3(0,trace,"sigasync");
		QDIO_DBF_HEX3(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
		QDIO_DBF_HEX3(0,trace,&cc,sizeof(int*));
	}

	return cc;
}

static inline int qdio_siga_sync_q(qdio_q_t *q)
{
	if (q->is_input_q) {
		return qdio_siga_sync(q,0,q->mask);
	} else {
		return qdio_siga_sync(q,q->mask,0);
	}
}

/* returns QDIO_SIGA_ERROR_ACCESS_EXCEPTION as cc, when SIGA returns
   an access exception */
static inline int qdio_siga_output(qdio_q_t *q)
{
	int cc;
	__u32 busy_bit;
	__u64 start_time=0;

#ifdef QDIO_PERFORMANCE_STATS
	perf_stats.siga_outs++;
#endif /* QDIO_PERFORMANCE_STATS */

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"sigaout");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	for (;;) {

#ifdef QDIO_32_BIT
		asm volatile (
		      "lhi	0,0	\n\t"
		      "lr	1,%2	\n\t"
		      "lr	2,%3	\n\t"
		      "siga	0	\n\t"
		      "0:"
		      "ipm	%0	\n\t"
		      "srl	%0,28	\n\t"
		      "srl	0,31	\n\t"
		      "lr	%1,0	\n\t"
		      "1:	\n\t"
		      ".section .fixup,\"ax\"\n\t"
		      "2:	\n\t"
		      "lhi	%0,%4	\n\t"
		      "bras	1,3f	\n\t"
		      ".long 1b	\n\t"
		      "3:	\n\t"
		      "l	1,0(1)	\n\t"
		      "br	1	\n\t"
		      ".previous	\n\t"
		      ".section __ex_table,\"a\"\n\t"
		      ".align 4	\n\t"
		      ".long	0b,2b	\n\t"
		      ".previous	\n\t"
			: "=d" (cc), "=d" (busy_bit)
			: "d" (0x10000|q->irq), "d" (q->mask),
			"i" (QDIO_SIGA_ERROR_ACCESS_EXCEPTION)
			: "cc", "0", "1", "2", "memory"
		);
#else /* QDIO_32_BIT */
		asm volatile (
		      "lghi	0,0	\n\t"
		      "llgfr	1,%2	\n\t"
		      "llgfr	2,%3	\n\t"
	      	      "siga	0	\n\t"
		      "0:"
		      "ipm	%0	\n\t"
		      "srl	%0,28	\n\t"
		      "srl	0,31	\n\t"
		      "llgfr	%1,0	\n\t"
		      "1:	\n\t"
		      ".section .fixup,\"ax\"\n\t"
		      "lghi	%0,%4	\n\t"
		      "jg	1b	\n\t"
		      ".previous\n\t"
      		      ".section __ex_table,\"a\"\n\t"
      		      ".align 8	\n\t"
      		      ".quad	0b,1b	\n\t"
      		      ".previous	\n\t"
		      : "=d" (cc), "=d" (busy_bit)
		      : "d" (0x10000|q->irq), "d" (q->mask),
	      	      "i" (QDIO_SIGA_ERROR_ACCESS_EXCEPTION)
	      	      : "cc", "0", "1", "2", "memory"
	     );
#endif /* QDIO_32_BIT */

//QDIO_PRINT_ERR("cc=%x, busy=%x\n",cc,busy_bit);
	     if ( (cc==2) && (busy_bit) &&
		  (q->is_iqdio_q) ) {
		     if (!start_time) start_time=NOW;
		     if ((NOW-start_time)>QDIO_BUSY_BIT_PATIENCE)
			     break;
	     } else
		     break;
	}

	if ((cc==2) && (busy_bit)) cc|=QDIO_SIGA_ERROR_B_BIT_SET;

	if (cc) {
#ifndef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT3(0,trace,"sigaout");
		QDIO_DBF_HEX3(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
		QDIO_DBF_HEX3(0,trace,&cc,sizeof(int*));
	}

	return cc;
}

static inline int qdio_siga_input(qdio_q_t *q)
{
	int cc;

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"sigain");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

#ifdef QDIO_PERFORMANCE_STATS
	perf_stats.siga_ins++;
#endif /* QDIO_PERFORMANCE_STATS */

#ifdef QDIO_32_BIT
	asm volatile (
		"lhi	0,1	\n\t"
		"lr	1,%1	\n\t"
		"lr	2,%2	\n\t"
		"siga   0	\n\t"
		"ipm	%0	\n\t"
		"srl	%0,28	\n\t"
		: "=d" (cc)
		: "d" (0x10000|q->irq), "d" (q->mask)
		: "cc", "0", "1", "2", "memory"
		);
#else /* QDIO_32_BIT */
	asm volatile (
		"lghi	0,1	\n\t"
		"llgfr	1,%1	\n\t"
		"llgfr	2,%2	\n\t"
		"siga   0	\n\t"
		"ipm	%0	\n\t"
		"srl	%0,28	\n\t"
		: "=d" (cc)
		: "d" (0x10000|q->irq), "d" (q->mask)
		: "cc", "0", "1", "2", "memory"
		);
#endif /* QDIO_32_BIT */

	if (cc) {
#ifndef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT3(0,trace,"sigain");
		QDIO_DBF_HEX3(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
		QDIO_DBF_HEX3(0,trace,&cc,sizeof(int*));
	}

	return cc;
}

/* locked by the locks in qdio_activate and qdio_cleanup */
static __u32 * volatile qdio_get_indicator(void)
{
	int i=1;
	int found=0;

	while (i<INDICATORS_PER_CACHELINE) {
		if (!indicator_used[i]) {
			indicator_used[i]=1;
			found=1;
			break;
		}
		i++;
	}

	if (found)
		return indicators+i;
	else
		return (__u32 * volatile) &spare_indicator;
}

/* locked by the locks in qdio_activate and qdio_cleanup */
static void qdio_put_indicator(__u32 *addr)
{
	int i;

	if ( (addr) && (addr!=&spare_indicator) ) {
		i=addr-indicators;
		indicator_used[i]=0;
	}
}

static inline volatile void tiqdio_clear_summary_bit(__u32 *location)
{
#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT5(0,trace,"clrsummb");
	QDIO_DBF_HEX5(0,trace,&location,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
	xchg(location,0);
}

static inline volatile void tiqdio_set_summary_bit(__u32 *location)
{
#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT5(0,trace,"setsummb");
	QDIO_DBF_HEX5(0,trace,&location,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
	xchg(location,-1);
}

static inline void tiqdio_sched_tl(void)
{
	tasklet_hi_schedule(&tiqdio_tasklet);
}

static inline void qdio_mark_tiq(qdio_q_t *q)
{
	unsigned long flags;
#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"mark iq");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	spin_lock_irqsave(&ttiq_list_lock,flags);
	if (unlikely(atomic_read(&q->is_in_shutdown))) goto out_unlock;

	if (q->is_input_q) {
		if ((q->list_prev) || (q->list_next)) goto out_unlock;

		if (!tiq_list) {
			tiq_list=q;
			q->list_prev=q;
			q->list_next=q;
		} else {
			q->list_next=tiq_list;
			q->list_prev=tiq_list->list_prev;
			tiq_list->list_prev->list_next=q;
			tiq_list->list_prev=q;
		}
		spin_unlock_irqrestore(&ttiq_list_lock,flags);

		tiqdio_set_summary_bit((__u32*)q->dev_st_chg_ind);
		tiqdio_sched_tl();
	}
	return;
out_unlock:
	spin_unlock_irqrestore(&ttiq_list_lock,flags);
	return;
}

static inline void qdio_mark_q(qdio_q_t *q)
{
#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"mark q");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	if (unlikely(atomic_read(&q->is_in_shutdown))) return;

	tasklet_schedule(&q->tasklet);
}

static inline int qdio_stop_polling(qdio_q_t *q)
{
#ifdef QDIO_USE_PROCESSING_STATE
	int gsf;

	if (!atomic_swap(&q->polling,0)) return 1;

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"stoppoll");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	/* show the card that we are not polling anymore */
	if (q->is_input_q) {
		gsf=GET_SAVED_FRONTIER(q);
		set_slsb(&q->slsb.acc.val[(gsf+QDIO_MAX_BUFFERS_PER_Q-1)&
			 (QDIO_MAX_BUFFERS_PER_Q-1)],SLSB_P_INPUT_NOT_INIT);
		/* we don't issue this SYNC_MEMORY, as we trust Rick T and
		 * moreover will not use the PROCESSING state, so q->polling
		 * was 0
		SYNC_MEMORY;*/
		if (q->slsb.acc.val[gsf]==SLSB_P_INPUT_PRIMED) {
			/* set our summary bit again, as otherwise there is a
			 * small window we can miss between resetting it and
			 * checking for PRIMED state */
			if (q->is_thinint_q)
				tiqdio_set_summary_bit
					((__u32*)q->dev_st_chg_ind);
			return 0;
		}
	}
#endif /* QDIO_USE_PROCESSING_STATE */
	return 1;
}

/* see the comment in do_QDIO and before qdio_reserve_q about the
 * sophisticated locking outside of unmark_q, so that we don't need to
 * disable the interrupts :-) */
static inline void qdio_unmark_q(qdio_q_t *q)
{
	unsigned long flags;

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"unmark q");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	if ((!q->list_prev)||(!q->list_next)) return;

	if ((q->is_thinint_q)&&(q->is_input_q)) {
		/* iQDIO */
		spin_lock_irqsave(&ttiq_list_lock,flags);
		if (q->list_next==q) {
			/* q was the only interesting q */
			tiq_list=NULL;
			q->list_next=NULL;
			q->list_prev=NULL;
		} else {
			q->list_next->list_prev=q->list_prev;
			q->list_prev->list_next=q->list_next;
			tiq_list=q->list_next;
			q->list_next=NULL;
			q->list_prev=NULL;
		}
		spin_unlock_irqrestore(&ttiq_list_lock,flags);
	}
}

static inline volatile unsigned long tiqdio_clear_global_summary(void)
{
	unsigned long time;

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT5(0,trace,"clrglobl");
#endif /* QDIO_DBF_LIKE_HELL */

#ifdef QDIO_32_BIT
	asm volatile (
		"lhi	1,3	\n\t"
		".insn	rre,0xb2650000,2,0	\n\t"
		"lr	%0,3	\n\t"
		: "=d" (time) : : "cc", "1", "2", "3"
		);
#else /* QDIO_32_BIT */
	asm volatile (
		"lghi	1,3	\n\t"
		".insn	rre,0xb2650000,2,0	\n\t"
		"lgr	%0,3	\n\t"
		: "=d" (time) : : "cc", "1", "2", "3"
		);
#endif /* QDIO_32_BIT */

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_HEX5(0,trace,&time,sizeof(unsigned long));
#endif /* QDIO_DBF_LIKE_HELL */
	return time;
}

/************************* OUTBOUND ROUTINES *******************************/

static inline void qdio_translate_buffer_back(qdio_q_t *q,int bufno)
{
	if (unlikely(!q->is_0copy_sbals_q))
		memcpy(q->qdio_buffers[bufno],
		       (void*)q->sbal[bufno],SBAL_SIZE);
}

inline static int qdio_get_outbound_buffer_frontier(qdio_q_t *q)
{
	int f,f_mod_no;
	volatile char *slsb;
	int first_not_to_check;
	char dbf_text[15];
	char slsbyte;

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"getobfro");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	slsb=&q->slsb.acc.val[0];
	f_mod_no=f=q->first_to_check;
	/* f point to already processed elements, so f+no_used is correct...
	 * ... but: we don't check 128 buffers, as otherwise
	 * qdio_has_outbound_q_moved would return 0 */
	first_not_to_check=f+qdio_min(atomic_read(&q->number_of_buffers_used),
				      (QDIO_MAX_BUFFERS_PER_Q-1));

	if ((!q->is_iqdio_q)&&(!q->hydra_gives_outbound_pcis)) {
		SYNC_MEMORY;
	}

check_next:
	if (f==first_not_to_check) goto out;
	slsbyte=slsb[f_mod_no];

	/* the hydra has not fetched the output yet */
	if (slsbyte==SLSB_CU_OUTPUT_PRIMED) {
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT5(0,trace,"outpprim");
#endif /* QDIO_DBF_LIKE_HELL */
		goto out;
	}

	/* the hydra got it */
	if (slsbyte==SLSB_P_OUTPUT_EMPTY) {
		atomic_dec(&q->number_of_buffers_used);
		f++;
		f_mod_no=f&(QDIO_MAX_BUFFERS_PER_Q-1);
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT5(0,trace,"outpempt");
#endif /* QDIO_DBF_LIKE_HELL */
		goto check_next;
	}

	if (slsbyte==SLSB_P_OUTPUT_ERROR) {
		QDIO_DBF_TEXT3(0,trace,"outperr");
		sprintf(dbf_text,"%x-%x-%x",f_mod_no,
			q->sbal[f_mod_no]->element[14].sbalf.value,
			q->sbal[f_mod_no]->element[15].sbalf.value);
		QDIO_DBF_TEXT3(1,trace,dbf_text);
		QDIO_DBF_HEX2(1,sbal,q->sbal[f_mod_no],256);

		/* kind of process the buffer */
		set_slsb(&q->slsb.acc.val[f_mod_no],
			 SLSB_P_OUTPUT_NOT_INIT);

		qdio_translate_buffer_back(q,f_mod_no);

		/* we increment the frontier, as this buffer
		 * was processed obviously */
		atomic_dec(&q->number_of_buffers_used);
		f_mod_no=(f_mod_no+1)&(QDIO_MAX_BUFFERS_PER_Q-1);

		if (q->qdio_error)
			q->error_status_flags|=
				QDIO_STATUS_MORE_THAN_ONE_QDIO_ERROR;
		q->qdio_error=SLSB_P_OUTPUT_ERROR;
		q->error_status_flags|=QDIO_STATUS_LOOK_FOR_ERROR;

		goto out;
	}

	/* no new buffers */
#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT5(0,trace,"outpni");
#endif /* QDIO_DBF_LIKE_HELL */
	goto out;

out:
	return (q->first_to_check=f_mod_no);
}

/* all buffers are processed */
inline static int qdio_is_outbound_q_done(qdio_q_t *q)
{
	int no_used;
#ifdef QDIO_DBF_LIKE_HELL
	char dbf_text[15];
#endif /* QDIO_DBF_LIKE_HELL */

	no_used=atomic_read(&q->number_of_buffers_used);

#ifdef QDIO_DBF_LIKE_HELL
	if (no_used) {
		sprintf(dbf_text,"oqisnt%02x",no_used);
		QDIO_DBF_TEXT4(0,trace,dbf_text);
	} else {
		QDIO_DBF_TEXT4(0,trace,"oqisdone");
	}
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
	return (no_used==0);
}

inline static int qdio_has_outbound_q_moved(qdio_q_t *q)
{
	int i;

	i=qdio_get_outbound_buffer_frontier(q);

	if ( (i!=GET_SAVED_FRONTIER(q)) ||
	     (q->error_status_flags&QDIO_STATUS_LOOK_FOR_ERROR) ) {
		SAVE_FRONTIER(q,i);
		SAVE_TIMESTAMP(q);
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT4(0,trace,"oqhasmvd");
		QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
		return 1;
	} else {
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT4(0,trace,"oqhsntmv");
		QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
		return 0;
	}
}

inline static void qdio_kick_outbound_q(qdio_q_t *q)
{
	int result;
#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"kickoutq");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	if (!q->siga_out) return;

	result=qdio_siga_output(q);

	if (result) {
		if (q->siga_error)
			q->error_status_flags|=
				QDIO_STATUS_MORE_THAN_ONE_SIGA_ERROR;
		q->error_status_flags|=
			QDIO_STATUS_LOOK_FOR_ERROR;
		q->siga_error=result;
	}
}

inline static void qdio_kick_outbound_handler(qdio_q_t *q)
{
#ifdef QDIO_DBF_LIKE_HELL
	char dbf_text[15];
#endif /* QDIO_DBF_LIKE_HELL */

	int start=q->first_element_to_kick;
	/* last_move_ftc was just updated */
	int real_end=GET_SAVED_FRONTIER(q);
	int end=(real_end+QDIO_MAX_BUFFERS_PER_Q-1)&
		(QDIO_MAX_BUFFERS_PER_Q-1);
	int count=(end+QDIO_MAX_BUFFERS_PER_Q+1-start)&
		(QDIO_MAX_BUFFERS_PER_Q-1);

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"kickouth");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

#ifdef QDIO_DBF_LIKE_HELL
	sprintf(dbf_text,"s=%2xc=%2x",start,count);
	QDIO_DBF_TEXT4(0,trace,dbf_text);
#endif /* QDIO_DBF_LIKE_HELL */

	if (q->state==QDIO_IRQ_STATE_ACTIVE)
		q->handler(q->irq,QDIO_STATUS_OUTBOUND_INT|
			   q->error_status_flags,
			   q->qdio_error,q->siga_error,q->q_no,start,count,
			   q->int_parm);

	/* for the next time: */
	q->first_element_to_kick=real_end;
	q->qdio_error=0;
	q->siga_error=0;
	q->error_status_flags=0;
}

static void qdio_outbound_processing(qdio_q_t *q)
{
#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"qoutproc");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	if (unlikely(qdio_reserve_q(q))) {
		qdio_release_q(q);
#ifdef QDIO_PERFORMANCE_STATS
		o_p_c++;
#endif /* QDIO_PERFORMANCE_STATS */
		/* as we're sissies, we'll check next time */
		if (likely(!atomic_read(&q->is_in_shutdown))) {
			qdio_mark_q(q);
#ifdef QDIO_DBF_LIKE_HELL
			QDIO_DBF_TEXT4(0,trace,"busy,agn");
#endif /* QDIO_DBF_LIKE_HELL */
		}
		return;
	}
#ifdef QDIO_PERFORMANCE_STATS
	o_p_nc++;
#endif /* QDIO_PERFORMANCE_STATS */

#ifdef QDIO_PERFORMANCE_STATS
	perf_stats.tl_runs++;
#endif /* QDIO_PERFORMANCE_STATS */

	if (qdio_has_outbound_q_moved(q)) {
		qdio_kick_outbound_handler(q);
	}

	if (q->is_iqdio_q) {
		/* for asynchronous queues, we better check, if the fill
		 * level is too high */
		if (atomic_read(&q->number_of_buffers_used)>
		    IQDIO_FILL_LEVEL_TO_POLL) {
			qdio_mark_q(q);
		}
	} else if (!q->hydra_gives_outbound_pcis) {
		if (!qdio_is_outbound_q_done(q)) {
			qdio_mark_q(q);
		}
	}

	qdio_release_q(q);
}

/************************* INBOUND ROUTINES *******************************/


inline static int qdio_get_inbound_buffer_frontier(qdio_q_t *q)
{
	int f,f_mod_no;
	volatile char *slsb;
	char slsbyte;
	int first_not_to_check;
	char dbf_text[15];

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"getibfro");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	slsb=&q->slsb.acc.val[0];
	f_mod_no=f=q->first_to_check;
	/* we don't check 128 buffers, as otherwise qdio_has_inbound_q_moved
	 * would return 0 */
	first_not_to_check=f+qdio_min(atomic_read(&q->number_of_buffers_used),
				      (QDIO_MAX_BUFFERS_PER_Q-1));

	/* we don't use this one, as a PCI or we after a thin interrupt
	 * will sync the queues
	SYNC_MEMORY;*/

check_next:
	f_mod_no=f&(QDIO_MAX_BUFFERS_PER_Q-1);
	if (f==first_not_to_check) goto out;
	slsbyte=slsb[f_mod_no];

	/* CU_EMPTY means frontier is reached */
	if (slsbyte==SLSB_CU_INPUT_EMPTY) {
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT5(0,trace,"inptempt");
#endif /* QDIO_DBF_LIKE_HELL */
		goto out;
	}

	/* P_PRIMED means set slsb to P_PROCESSING and move on */
	if (slsbyte==SLSB_P_INPUT_PRIMED) {
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT5(0,trace,"inptprim");
#endif /* QDIO_DBF_LIKE_HELL */

#ifdef QDIO_USE_PROCESSING_STATE
		/* as soon as running under VM, polling the input queues will
		 * kill VM in terms of CP overhead */
		if (q->siga_sync) {
			set_slsb(&slsb[f_mod_no],SLSB_P_INPUT_NOT_INIT);
		} else {
			set_slsb(&slsb[f_mod_no],SLSB_P_INPUT_PROCESSING);
			atomic_set(&q->polling,1);
		}
#else /* QDIO_USE_PROCESSING_STATE */
		set_slsb(&slsb[f_mod_no],SLSB_P_INPUT_NOT_INIT);
#endif /* QDIO_USE_PROCESSING_STATE */
		/* not needed, as the inbound queue will be synced on the next
		 * siga-r
		SYNC_MEMORY;*/
		f++;
		atomic_dec(&q->number_of_buffers_used);
		goto check_next;
	}

	if ( (slsbyte==SLSB_P_INPUT_NOT_INIT) ||
	     (slsbyte==SLSB_P_INPUT_PROCESSING) ) {
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT5(0,trace,"inpnipro");
#endif /* QDIO_DBF_LIKE_HELL */
		goto out;
	}

	/* P_ERROR means frontier is reached, break and report error */
	if (slsbyte==SLSB_P_INPUT_ERROR) {
		sprintf(dbf_text,"inperr%2x",f_mod_no);
		QDIO_DBF_TEXT3(1,trace,dbf_text);
		QDIO_DBF_HEX2(1,sbal,q->sbal[f_mod_no],256);

		/* kind of process the buffer */
		set_slsb(&slsb[f_mod_no],SLSB_P_INPUT_NOT_INIT);

		if (q->qdio_error)
			q->error_status_flags|=
				QDIO_STATUS_MORE_THAN_ONE_QDIO_ERROR;
		q->qdio_error=SLSB_P_INPUT_ERROR;
		q->error_status_flags|=QDIO_STATUS_LOOK_FOR_ERROR;

		/* we increment the frontier, as this buffer
		 * was processed obviously */
		f_mod_no=(f_mod_no+1)&(QDIO_MAX_BUFFERS_PER_Q-1);
		atomic_dec(&q->number_of_buffers_used);

		goto out;
	}

	/* everything else means frontier not changed (HALTED or so) */
out:
	q->first_to_check=f_mod_no;

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_HEX4(0,trace,&q->first_to_check,sizeof(int));
#endif /* QDIO_DBF_LIKE_HELL */

	return q->first_to_check;
}

inline static int qdio_has_inbound_q_moved(qdio_q_t *q)
{
	int i;

#ifdef QDIO_PERFORMANCE_STATS
	static int old_pcis=0;
	static int old_thinints=0;

	if ((old_pcis==perf_stats.pcis)&&(old_thinints==perf_stats.thinints))
		perf_stats.start_time_inbound=NOW;
	else
		old_pcis=perf_stats.pcis;
#endif /* QDIO_PERFORMANCE_STATS */

	i=qdio_get_inbound_buffer_frontier(q);
	if ( (i!=GET_SAVED_FRONTIER(q)) ||
	     (q->error_status_flags&QDIO_STATUS_LOOK_FOR_ERROR) ) {
		SAVE_FRONTIER(q,i);
		if ((!q->siga_sync)&&(!q->hydra_gives_outbound_pcis)) {
			SAVE_TIMESTAMP(q);
		}

#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT4(0,trace,"inhasmvd");
		QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
		return 1;
	} else {
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT4(0,trace,"inhsntmv");
		QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
		return 0;
	}
}

/* means, no more buffers to be filled */
inline static int iqdio_is_inbound_q_done(qdio_q_t *q)
{
	int no_used;
#ifdef QDIO_DBF_LIKE_HELL
	char dbf_text[15];
#endif /* QDIO_DBF_LIKE_HELL */

	no_used=atomic_read(&q->number_of_buffers_used);

	/* propagate the change from 82 to 80 through VM */
	SYNC_MEMORY;

#ifdef QDIO_DBF_LIKE_HELL
	if (no_used) {
		sprintf(dbf_text,"iqisnt%02x",no_used);
		QDIO_DBF_TEXT4(0,trace,dbf_text);
	} else {
		QDIO_DBF_TEXT4(0,trace,"iniqisdo");
	}
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	if (!no_used) {
		return 1;
	}

	if (q->siga_sync) {
		if (q->slsb.acc.val[q->first_to_check]==SLSB_P_INPUT_PRIMED) {
			/* ok, the next input buffer is primed. that means,
			 * that device state change indicator and adapter
			 * local summary are set, so we will find it next
			 * time.
			 * we will return 0 below, as there is nothing to
			 * do, except of scheduling ourselves for the next
			 * time. */
			tiqdio_set_summary_bit((__u32*)q->dev_st_chg_ind);
			tiqdio_sched_tl();
		} else {
			/* nothing more to do, if next buffer is not PRIMED.
			 * note that we did a SYNC_MEMORY before, that there
			 * has been a sychnronization.
			 * we will return 0 below, as there is nothing to do
			 * (stop_polling not necessary, as we have not been
			 * using the PROCESSING state */
		}
	} else {
		/* we'll check for more primed buffers
		 * in qeth_stop_polling */
	}

	return 0;
}

inline static int qdio_is_inbound_q_done(qdio_q_t *q)
{
	int no_used;
#ifdef QDIO_DBF_LIKE_HELL
	char dbf_text[15];
#endif /* QDIO_DBF_LIKE_HELL */

	no_used=atomic_read(&q->number_of_buffers_used);

	/* we need that one for synchronization with Hydra, as Hydra
	 * does a kind of PCI avoidance */
	SYNC_MEMORY;

	if (!no_used) {
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT4(0,trace,"inqisdnA");
		QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
		QDIO_DBF_TEXT4(0,trace,dbf_text);
#endif /* QDIO_DBF_LIKE_HELL */
		return 1;
	}

	if (q->slsb.acc.val[q->first_to_check]==SLSB_P_INPUT_PRIMED) {
		/* we got something to do */
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT4(0,trace,"inqisntA");
		QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */
		return 0;
	}

	/* on VM, we don't poll, so the q is always done here */
	if (q->siga_sync) return 1;
	if (q->hydra_gives_outbound_pcis) return 1;

	/* at this point we know, that inbound first_to_check
	   has (probably) not moved (see qdio_inbound_processing) */
	if (NOW>GET_SAVED_TIMESTAMP(q)+q->timing.threshold) {
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT4(0,trace,"inqisdon");
		QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
		sprintf(dbf_text,"pf%02xcn%02x",q->first_to_check,no_used);
		QDIO_DBF_TEXT4(0,trace,dbf_text);
#endif /* QDIO_DBF_LIKE_HELL */
		return 1;
	} else {
#ifdef QDIO_DBF_LIKE_HELL
		QDIO_DBF_TEXT4(0,trace,"inqisntd");
		QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
		sprintf(dbf_text,"pf%02xcn%02x",q->first_to_check,no_used);
		QDIO_DBF_TEXT4(0,trace,dbf_text);
#endif /* QDIO_DBF_LIKE_HELL */
		return 0;
	}
}

inline static void qdio_kick_inbound_handler(qdio_q_t *q)
{
	int count=0;
	int start,end,real_end,i;
#ifdef QDIO_DBF_LIKE_HELL
	char dbf_text[15];

	QDIO_DBF_TEXT4(0,trace,"kickinh");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	start=q->first_element_to_kick;
	real_end=q->first_to_check;
	end=(real_end+QDIO_MAX_BUFFERS_PER_Q-1)&(QDIO_MAX_BUFFERS_PER_Q-1);

	i=start;
	while (1) {
		count++;
		qdio_translate_buffer_back(q,i);
		if (i==end) break;
		i=(i+1)&(QDIO_MAX_BUFFERS_PER_Q-1);
	}

#ifdef QDIO_DBF_LIKE_HELL
	sprintf(dbf_text,"s=%2xc=%2x",start,count);
	QDIO_DBF_TEXT4(0,trace,dbf_text);
#endif /* QDIO_DBF_LIKE_HELL */

	if (likely(q->state==QDIO_IRQ_STATE_ACTIVE))
		q->handler(q->irq,
			   QDIO_STATUS_INBOUND_INT|q->error_status_flags,
			   q->qdio_error,q->siga_error,q->q_no,start,count,
			   q->int_parm);

	/* for the next time: */
	q->first_element_to_kick=real_end;
	q->qdio_error=0;
	q->siga_error=0;
	q->error_status_flags=0;

#ifdef QDIO_PERFORMANCE_STATS
	perf_stats.inbound_time+=NOW-perf_stats.start_time_inbound;
	perf_stats.inbound_cnt++;
#endif /* QDIO_PERFORMANCE_STATS */
}

static inline void tiqdio_inbound_processing(qdio_q_t *q)
{
	qdio_irq_t *irq_ptr;
	qdio_q_t *oq;
	int i;

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"iqinproc");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	/* we first want to reserve the q, so that we know, that we don't
	 * interrupt ourselves and call qdio_unmark_q, as is_in_shutdown might
	 * be set */
	if (unlikely(qdio_reserve_q(q))) {
		qdio_release_q(q);
#ifdef QDIO_PERFORMANCE_STATS
		ii_p_c++;
#endif /* QDIO_PERFORMANCE_STATS */
		/* as we might just be about to stop polling, we make
		 * sure that we check again at least once more */
		tiqdio_sched_tl();
		return;
	}
#ifdef QDIO_PERFORMANCE_STATS
	ii_p_nc++;
#endif /* QDIO_PERFORMANCE_STATS */
	if (unlikely(atomic_read(&q->is_in_shutdown))) {
		qdio_unmark_q(q);
		goto out;
	}

	if (*(q->dev_st_chg_ind)) {
		tiqdio_clear_summary_bit((__u32*)q->dev_st_chg_ind);

		if (q->hydra_gives_outbound_pcis) {
			if (!q->siga_sync_done_on_thinints) {
				SYNC_MEMORY_ALL;
			} else if ((!q->siga_sync_done_on_outb_tis)&&
				   (q->hydra_gives_outbound_pcis)) {
				SYNC_MEMORY_ALL_OUTB;
			}
		} else {
			SYNC_MEMORY;
		}

		/* maybe we have to do work on our outbound queues... at least
		 * we have to check Hydra outbound-int-capable thinint-capable
		 * queues */
		if (q->hydra_gives_outbound_pcis) {
			irq_ptr=(qdio_irq_t*)q->irq_ptr;
 			for (i=0;i<irq_ptr->no_output_qs;i++) {
 				oq=irq_ptr->output_qs[i];
#ifdef QDIO_PERFORMANCE_STATS
				perf_stats.tl_runs--;
#endif /* QDIO_PERFORMANCE_STATS */
				if (!qdio_is_outbound_q_done(oq)) {
					qdio_outbound_processing(oq);
				}
			}
		}

		if (qdio_has_inbound_q_moved(q)) {
			qdio_kick_inbound_handler(q);
			if (iqdio_is_inbound_q_done(q)) {
				if (!qdio_stop_polling(q)) {
					/* we set the flags to get into
					 * the stuff next time, see also
					 * comment in qdio_stop_polling */
					tiqdio_set_summary_bit
						((__u32*)q->dev_st_chg_ind);
					tiqdio_sched_tl();
				}
			}
		}
	}
out:
	qdio_release_q(q);
}

static void qdio_inbound_processing(qdio_q_t *q)
{
	int q_laps=0;

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"qinproc");
	QDIO_DBF_HEX4(0,trace,&q,sizeof(void*));
#endif /* QDIO_DBF_LIKE_HELL */

	if (unlikely(qdio_reserve_q(q))) {
		qdio_release_q(q);
#ifdef QDIO_PERFORMANCE_STATS
		i_p_c++;
#endif /* QDIO_PERFORMANCE_STATS */
		/* as we're sissies, we'll check next time */
		if (likely(!atomic_read(&q->is_in_shutdown))) {
			qdio_mark_q(q);
#ifdef QDIO_DBF_LIKE_HELL
			QDIO_DBF_TEXT4(0,trace,"busy,agn");
#endif /* QDIO_DBF_LIKE_HELL */
		}
		return;
	}
#ifdef QDIO_PERFORMANCE_STATS
	i_p_nc++;
	perf_stats.tl_runs++;
#endif /* QDIO_PERFORMANCE_STATS */

again:
	if (qdio_has_inbound_q_moved(q)) {
		qdio_kick_inbound_handler(q);
		if (!qdio_stop_polling(q)) {
			q_laps++;
			if (q_laps<QDIO_Q_LAPS) goto again;
		}
		qdio_mark_q(q);
	} else {
		if (!qdio_is_inbound_q_done(q)) /* means poll time is 
						   not yet over */
			qdio_mark_q(q);
	}

	qdio_release_q(q);
}

/************************* MAIN ROUTINES *******************************/

static inline void tiqdio_inbound_checks(void)
{
	qdio_q_t *q;
#ifdef QDIO_USE_PROCESSING_STATE
	int q_laps=0;
#endif /* QDIO_USE_PROCESSING_STATE */

#ifdef QDIO_DBF_LIKE_HELL
	char dbf_text[15];

	QDIO_DBF_TEXT4(0,trace,"iqdinbck");
#endif /* QDIO_DBF_LIKE_HELL */

#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT5(0,trace,"iqlocsum");
#endif /* QDIO_DBF_LIKE_HELL */

#ifdef QDIO_USE_PROCESSING_STATE
again:
#endif /* QDIO_USE_PROCESSING_STATE */

	q=(qdio_q_t*)tiq_list;
	/* switch all active queues to processing state */
	do {
		if (!q) break;
		tiqdio_inbound_processing(q);
		q=(qdio_q_t*)q->list_next;
	} while (q!=(qdio_q_t*)tiq_list);

	/* switch off all queues' processing state */
#ifdef QDIO_USE_PROCESSING_STATE
	q=(qdio_q_t*)tiq_list;
	do {
		if (!q) {
			tiqdio_sched_tl();
			break;
		}
		/* under VM, we have not used the PROCESSING state, so no
		 * need to stop polling */
		if (q->siga_sync) {
			q=(qdio_q_t*)q->list_next;
			continue;
		}

		if (unlikely(qdio_reserve_q(q))) {
			qdio_release_q(q);
#ifdef QDIO_PERFORMANCE_STATS
			ii_p_c++;
#endif /* QDIO_PERFORMANCE_STATS */
			/* as we might just be about to stop polling, we make
			 * sure that we check again at least once more */

			/* sanity -- we'd get here without setting the
			 * dev st chg ind */
			tiqdio_set_summary_bit((__u32*)q->dev_st_chg_ind);
			tiqdio_sched_tl();
			break;
		}
		if (!qdio_stop_polling(q)) {
			q_laps++;
			if (q_laps<QDIO_Q_LAPS) {
				qdio_release_q(q);
				goto again;
			} else {
				/* we set the flags to get into the stuff
				 * next time, see also comment in
				 * qdio_stop_polling */
				tiqdio_set_summary_bit((__u32*)
						      q->dev_st_chg_ind);
				tiqdio_sched_tl();
			}
		}
		qdio_release_q(q);
		q=(qdio_q_t*)q->list_next;
	} while (q!=(qdio_q_t*)tiq_list);
#endif /* QDIO_USE_PROCESSING_STATE */
}

static void tiqdio_tl(unsigned long data)
{
#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"tiqdio_tl");
#endif /* QDIO_DBF_LIKE_HELL */
#ifdef QDIO_PERFORMANCE_STATS
	perf_stats.tl_runs++;
#endif /* QDIO_PERFORMANCE_STATS */

	tiqdio_inbound_checks();
}

/********************* GENERAL HELPER_ROUTINES ***********************/

static qdio_irq_t *qdio_get_irq_ptr(int irq)
{
	qdio_irq_t *irq_ptr;
	int bucket=irq&(QDIO_IRQ_BUCKETS-1);

	read_lock(&irq_list_lock[bucket]);
	irq_ptr=first_irq[bucket];
	while (irq_ptr) {
		if (irq_ptr->irq==irq) break;
		irq_ptr=irq_ptr->next;
	}
	read_unlock(&irq_list_lock[bucket]);
	return irq_ptr;
}

static qdio_irq_t *qdio_get_irq_ptr_wolock(int irq)
{
	qdio_irq_t *irq_ptr=first_irq[irq&(QDIO_IRQ_BUCKETS-1)];

	while (irq_ptr) {
		if (irq_ptr->irq==irq) break;
		irq_ptr=irq_ptr->next;
	}
	return irq_ptr;
}

/* irq_ptr->irq should be set already! */
static void qdio_insert_irq_ptr(qdio_irq_t *irq_ptr)
{
	qdio_irq_t *i_p;
	int irq,bucket;

	if (!irq_ptr) return;

	irq=irq_ptr->irq;
	bucket=irq&(QDIO_IRQ_BUCKETS-1);

	write_lock(&irq_list_lock[bucket]);

	if (irq_ptr==qdio_get_irq_ptr_wolock(irq)) goto out;

	if (!first_irq[bucket]) {
		first_irq[bucket]=irq_ptr;
	} else {
		i_p=first_irq[bucket];
		while (i_p->next)
			i_p=i_p->next;
		i_p->next=irq_ptr;
	}
	irq_ptr->next=NULL;
out:
	write_unlock(&irq_list_lock[bucket]);
}

static void qdio_remove_irq_ptr(qdio_irq_t *irq_ptr)
{
	qdio_irq_t *i_p;
	int irq,bucket;

	if (!irq_ptr) return;

	irq=irq_ptr->irq;
	bucket=irq&(QDIO_IRQ_BUCKETS-1);

	write_lock(&irq_list_lock[bucket]);

	if (!qdio_get_irq_ptr_wolock(irq)) goto out;

	if (first_irq[irq&(QDIO_IRQ_BUCKETS-1)]==irq_ptr) {
		first_irq[irq&(QDIO_IRQ_BUCKETS-1)]=irq_ptr->next;
	} else {
		for (i_p=first_irq[irq&(QDIO_IRQ_BUCKETS-1)];
		     i_p->next!=irq_ptr;i_p=i_p->next);
		i_p->next=irq_ptr->next;
	}
	irq_ptr->next=NULL;
out:
	write_unlock(&irq_list_lock[bucket]);
}

static void qdio_release_irq_memory(qdio_irq_t *irq_ptr)
{
	int i,j;
	int available;

	for (i=0;i<QDIO_MAX_QUEUES_PER_IRQ;i++) {
		if (!irq_ptr->input_qs[i]) goto next;
		available=0;
		if (!irq_ptr->input_qs[i]->is_0copy_sbals_q)
			for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
				if (!available) {
					if (irq_ptr->input_qs[i]->sbal[j])
						kfree((void*)irq_ptr->
						      input_qs[i]->sbal[j]);
					available=PAGE_SIZE;
				}
				available-=sizeof(sbal_t);
			}
		if (irq_ptr->input_qs[i]->slib)
			kfree(irq_ptr->input_qs[i]->slib);
			kfree(irq_ptr->input_qs[i]);

next:
		if (!irq_ptr->output_qs[i]) continue;
		available=0;
		if (!irq_ptr->output_qs[i]->is_0copy_sbals_q)
			for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
				if (!available) {
					if (irq_ptr->output_qs[i]->sbal[j])
						kfree((void*)irq_ptr->
						      output_qs[i]->sbal[j]);
					available=PAGE_SIZE;
				}
				available-=sizeof(sbal_t);
			}
		if (irq_ptr->output_qs[i]->slib)
			kfree(irq_ptr->output_qs[i]->slib);
		kfree(irq_ptr->output_qs[i]);

	}
	if (irq_ptr->qdr) kfree(irq_ptr->qdr);
	kfree(irq_ptr);
}

static inline void qdio_wakeup(atomic_t *var,qdio_irq_t *irq_ptr)
{
	wait_queue_head_t *wait_q;

#ifdef QDIO_DBF_LIKE_HELL
	char dbf_text[20];
	sprintf(dbf_text,"qwkp%4x",irq_ptr->irq);
	QDIO_DBF_TEXT3(0,trace,dbf_text);
#endif /* QDIO_DBF_LIKE_HELL */

	atomic_set(var,1);
	if ((wait_q=&irq_ptr->wait_q))
		wake_up(wait_q);
}

static int qdio_sleepon(atomic_t *var,int timeout,qdio_irq_t *irq_ptr)
{
        __u64 stop;
	int retval;
        DECLARE_WAITQUEUE (current_wait_q,current);

#ifdef QDIO_DBF_LIKE_HELL
	char dbf_text[20];
	sprintf(dbf_text,"qslp%4x",irq_ptr->irq);
	QDIO_DBF_TEXT3(0,trace,dbf_text);
#endif /* QDIO_DBF_LIKE_HELL */

	add_wait_queue(&irq_ptr->wait_q,&current_wait_q);
        stop=(qdio_get_micros()>>10)+timeout;
        for (;;) {
		set_task_state(current,TASK_INTERRUPTIBLE);
		if (atomic_read(var)) {
			atomic_set(var,0);
                        retval=0;
			goto out;
                }
                if (qdio_get_micros()>>10>stop) {
#ifdef QDIO_DBF_LIKE_HELL
			sprintf(dbf_text,"%xtime",irq_ptr->irq);
			QDIO_DBF_TEXT3(0,trace,dbf_text);
#endif /* QDIO_DBF_LIKE_HELL */
			retval=-ETIME;
			goto out;
		}
                schedule_timeout(((stop-(qdio_get_micros()>>10))>>10)*HZ);
        }
 out:
	set_task_state(current,TASK_RUNNING);
	remove_wait_queue(&irq_ptr->wait_q,&current_wait_q);
	return retval;
}

static void qdio_set_impl_params(qdio_irq_t *irq_ptr,
				 unsigned int qib_param_field_format,
			  /* pointer to 128 bytes or NULL, if no param field */
				 unsigned char *qib_param_field,
				 unsigned int no_input_qs,
				 unsigned int no_output_qs,
			  /* pointer to no_queues*128 words of data or NULL */
				 unsigned long *input_slib_elements,
				 unsigned long *output_slib_elements)
{
	int i,j;

	if (!irq_ptr) return;

	irq_ptr->qib.pfmt=qib_param_field_format;
	if (qib_param_field)
		memcpy(irq_ptr->qib.parm,qib_param_field,
		       QDIO_MAX_BUFFERS_PER_Q);

	if (input_slib_elements)
		for (i=0;i<no_input_qs;i++) {
			for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++)
				irq_ptr->input_qs[i]->slib->slibe[j].parms=
					input_slib_elements[
						i*QDIO_MAX_BUFFERS_PER_Q+j];
		}
	if (output_slib_elements)
		for (i=0;i<no_output_qs;i++) {
			for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++)
				irq_ptr->output_qs[i]->slib->slibe[j].parms=
					output_slib_elements[
						i*QDIO_MAX_BUFFERS_PER_Q+j];
		}
}



static int qdio_alloc_qs(qdio_irq_t *irq_ptr,int no_input_qs,int no_output_qs,
			 qdio_handler_t *input_handler,
			 qdio_handler_t *output_handler,
			 unsigned long int_parm,int q_format,
			 unsigned long flags,
			 void **inbound_sbals_array,
			 void **outbound_sbals_array)
{
	qdio_q_t *q;
	int i,j,result=0;
	char dbf_text[20]; /* see qdio_initialize */
	void *ptr;
	int available;

	for (i=0;i<no_input_qs;i++) {
		q=kmalloc(sizeof(qdio_q_t),GFP_KERNEL);

		if (!q) {
			QDIO_PRINT_ERR("kmalloc of q failed!\n");
			goto out;
		}
		memset(q,0,sizeof(qdio_q_t));

		sprintf(dbf_text,"in-q%4x",i);
		QDIO_DBF_TEXT0(0,setup,dbf_text);
		QDIO_DBF_HEX0(0,setup,&q,sizeof(void*));

		q->slib=kmalloc(PAGE_SIZE,GFP_KERNEL);
		if (!q->slib) {
			QDIO_PRINT_ERR("kmalloc of slib failed!\n");
			goto out;
		}
		memset(q->slib,0,PAGE_SIZE);
		q->sl=(sl_t*)(((char*)q->slib)+PAGE_SIZE/2);

		available=0;
		if (flags&QDIO_INBOUND_0COPY_SBALS) {
			for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
				q->sbal[j]=*(inbound_sbals_array++);
			}
		} else for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
			if (!available) {
				q->sbal[j]=kmalloc(PAGE_SIZE,GFP_KERNEL);
				if (!q->sbal[j]) {
					goto out;
				}
				available=PAGE_SIZE;
				memset((void*)q->sbal[j],0,PAGE_SIZE);
			} else {
				q->sbal[j]=(volatile sbal_t *)
					(((char*)q->sbal[j-1])+sizeof(sbal_t));
			}
			available-=sizeof(sbal_t);
		}

                q->queue_type=q_format;
		q->int_parm=int_parm;
		irq_ptr->input_qs[i]=q;
		q->irq=irq_ptr->irq;
		q->irq_ptr=irq_ptr;
		q->mask=1<<(31-i);
		q->q_no=i;
		q->is_input_q=1;
		q->is_0copy_sbals_q=flags&QDIO_INBOUND_0COPY_SBALS;
		q->first_to_check=0;
		q->last_move_ftc=0;
		q->handler=input_handler;
		q->dev_st_chg_ind=irq_ptr->dev_st_chg_ind;

		q->tasklet.data=(unsigned long)q;
		/* q->is_thinint_q isn't valid at this time, but
		 * irq_ptr->is_thinint_irq is */
		q->tasklet.func=(void(*)(unsigned long))
			((irq_ptr->is_thinint_irq)?&tiqdio_inbound_processing:
			 &qdio_inbound_processing);

/*		for (j=0;j<QDIO_STATS_NUMBER;j++)
			q->timing.last_transfer_times[j]=(qdio_get_micros()/
							  QDIO_STATS_NUMBER)*j;
		q->timing.last_transfer_index=QDIO_STATS_NUMBER-1;
*/

		/* fill in slib */
		if (i>0) irq_ptr->input_qs[i-1]->slib->nsliba=
				 QDIO_PFIX_GET_ADDR(q->slib);
		q->slib->sla=QDIO_PFIX_GET_ADDR(q->sl);
		q->slib->slsba=QDIO_PFIX_GET_ADDR((void *)&q->slsb.acc.val[0]);

		/* fill in sl */
		for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++)
			q->sl->element[j].sbal=
				QDIO_PFIX_GET_ADDR((void *)q->sbal[j]);

		QDIO_DBF_TEXT2(0,setup,"sl-sb-b0");
		ptr=(void*)q->sl;
		QDIO_DBF_HEX2(0,setup,&ptr,sizeof(void*));
		ptr=(void*)&q->slsb;
		QDIO_DBF_HEX2(0,setup,&ptr,sizeof(void*));
		ptr=(void*)q->sbal[0];
		QDIO_DBF_HEX2(0,setup,&ptr,sizeof(void*));

		/* fill in slsb */
		for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
			set_slsb(&q->slsb.acc.val[j],
		   		 SLSB_P_INPUT_NOT_INIT);
			q->sbal[j]->element[1].sbalf.i1.key=
				QDIO_STORAGE_ACC_KEY;
		}
	}

	for (i=0;i<no_output_qs;i++) {
		q=kmalloc(sizeof(qdio_q_t),GFP_KERNEL);

		if (!q) {
			goto out;
		}
		memset(q,0,sizeof(qdio_q_t));

		sprintf(dbf_text,"outq%4x",i);
		QDIO_DBF_TEXT0(0,setup,dbf_text);
		QDIO_DBF_HEX0(0,setup,&q,sizeof(void*));

		q->slib=kmalloc(PAGE_SIZE,GFP_KERNEL);
		if (!q->slib) {
			QDIO_PRINT_ERR("kmalloc of slib failed!\n");
			goto out;
		}
		memset(q->slib,0,PAGE_SIZE);
		q->sl=(sl_t*)(((char*)q->slib)+PAGE_SIZE/2);

		available=0;
		if (flags&QDIO_OUTBOUND_0COPY_SBALS) {
			for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
				q->sbal[j]=*(outbound_sbals_array++);
			}
		} else for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
			if (!available) {
				q->sbal[j]=kmalloc(PAGE_SIZE,GFP_KERNEL);
				if (!q->sbal[j]) {
					goto out;
				}
				available=PAGE_SIZE;
				memset((void*)q->sbal[j],0,PAGE_SIZE);
			} else {
				q->sbal[j]=(volatile sbal_t *)
					(((char*)q->sbal[j-1])+sizeof(sbal_t));
			}
			available-=sizeof(sbal_t);
		}

                q->queue_type=q_format;
		q->int_parm=int_parm;
		irq_ptr->output_qs[i]=q;
		q->is_input_q=0;
		q->is_0copy_sbals_q=flags&QDIO_OUTBOUND_0COPY_SBALS;
		q->irq=irq_ptr->irq;
		q->irq_ptr=irq_ptr;
		q->mask=1<<(31-i);
		q->q_no=i;
		q->first_to_check=0;
		q->last_move_ftc=0;
		q->handler=output_handler;

		q->tasklet.data=(unsigned long)q;
		q->tasklet.func=(void(*)(unsigned long))
			&qdio_outbound_processing;

		/* fill in slib */
		if (i>0) irq_ptr->output_qs[i-1]->slib->nsliba=
				 QDIO_PFIX_GET_ADDR(q->slib);
		q->slib->sla=QDIO_PFIX_GET_ADDR(q->sl);
		q->slib->slsba=QDIO_PFIX_GET_ADDR((void *)&q->slsb.acc.val[0]);

		/* fill in sl */
		for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++)
			q->sl->element[j].sbal=
				QDIO_PFIX_GET_ADDR((void *)q->sbal[j]);

		QDIO_DBF_TEXT2(0,setup,"sl-sb-b0");
		ptr=(void*)q->sl;
		QDIO_DBF_HEX2(0,setup,&ptr,sizeof(void*));
		ptr=(void*)&q->slsb;
		QDIO_DBF_HEX2(0,setup,&ptr,sizeof(void*));
		ptr=(void*)q->sbal[0];
		QDIO_DBF_HEX2(0,setup,&ptr,sizeof(void*));

		/* fill in slsb */
		for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
			set_slsb(&q->slsb.acc.val[j],
		   		 SLSB_P_OUTPUT_NOT_INIT);
			q->sbal[j]->element[1].sbalf.i1.key=
				QDIO_STORAGE_ACC_KEY;
		}
	}

	result=1;
out:
	return result;
}

static void qdio_fill_thresholds(qdio_irq_t *irq_ptr,
				 unsigned int no_input_qs,
				 unsigned int no_output_qs,
				 unsigned int min_input_threshold,
				 unsigned int max_input_threshold,
				 unsigned int min_output_threshold,
				 unsigned int max_output_threshold)
{
	int i;
	qdio_q_t *q;

	for (i=0;i<no_input_qs;i++) {
		q=irq_ptr->input_qs[i];
		q->timing.threshold=max_input_threshold;
/*		for (j=0;j<QDIO_STATS_CLASSES;j++) {
			q->threshold_classes[j].threshold=
				min_input_threshold+
				(max_input_threshold-min_input_threshold)/
				QDIO_STATS_CLASSES;
		}
		qdio_use_thresholds(q,QDIO_STATS_CLASSES/2);*/
	}
	for (i=0;i<no_output_qs;i++) {
		q=irq_ptr->output_qs[i];
		q->timing.threshold=max_output_threshold;
/*		for (j=0;j<QDIO_STATS_CLASSES;j++) {
			q->threshold_classes[j].threshold=
				min_output_threshold+
				(max_output_threshold-min_output_threshold)/
				QDIO_STATS_CLASSES;
		}
		qdio_use_thresholds(q,QDIO_STATS_CLASSES/2);*/
	}
}

static int tiqdio_thinint_handler(__u32 intparm)
{
#ifdef QDIO_DBF_LIKE_HELL
	QDIO_DBF_TEXT4(0,trace,"thin_int");
#endif /* QDIO_DBF_LIKE_HELL */

#ifdef QDIO_PERFORMANCE_STATS
	perf_stats.thinints++;
	perf_stats.start_time_inbound=NOW;
#endif /* QDIO_PERFORMANCE_STATS */

	/* VM will do the SVS for us
	 * issue SVS to benefit from iqdio interrupt avoidance (SVS clears AISOI)*/
	if (!MACHINE_IS_VM) {
		tiqdio_clear_global_summary();
	}

	tiqdio_inbound_checks();
	return 0;
}

static void qdio_set_state(qdio_irq_t *irq_ptr,int state)
{
	int i;
	char dbf_text[15];

	QDIO_DBF_TEXT5(0,trace,"newstate");
	sprintf(dbf_text,"%4x%4x",irq_ptr->irq,state);
	QDIO_DBF_TEXT5(0,trace,dbf_text);

	irq_ptr->state=state;
	for (i=0;i<irq_ptr->no_input_qs;i++)
		irq_ptr->input_qs[i]->state=state;
	for (i=0;i<irq_ptr->no_output_qs;i++)
		irq_ptr->output_qs[i]->state=state;
	mb();
}

static void qdio_handler(int irq,devstat_t *devstat,struct pt_regs *p)
{
	qdio_irq_t *irq_ptr;
	qdio_q_t *q;
	int i;
	int cstat,dstat;
	int rqparam;
	char dbf_text[15]="qintXXXX";

        cstat = devstat->cstat;
        dstat = devstat->dstat;
        rqparam=devstat->intparm;

	*((int*)(&dbf_text[4]))=irq;
	QDIO_DBF_HEX4(0,trace,dbf_text,QDIO_DBF_TRACE_LEN);
	
	if (!rqparam) {
		QDIO_PRINT_STUPID("got unsolicited interrupt in qdio " \
				  "handler, irq 0x%x\n",irq);
		return;
	}

	irq_ptr=qdio_get_irq_ptr(irq);
	if (!irq_ptr) {
		sprintf(dbf_text,"uint%4x",irq);
		QDIO_DBF_TEXT2(1,trace,dbf_text);
		QDIO_PRINT_ERR("received interrupt on unused irq 0x%04x!\n",
			       irq);
		return;
	}

	if (devstat->flag&DEVSTAT_FLAG_SENSE_AVAIL) {
		sprintf(dbf_text,"sens%4x",irq);
		QDIO_DBF_TEXT2(1,trace,dbf_text);
		QDIO_DBF_HEX0(0,sense,&devstat->ii.irb,QDIO_DBF_SENSE_LEN);

		QDIO_PRINT_WARN("sense data available on qdio channel.\n");
		HEXDUMP16(WARN,"irb: ",&devstat->ii.irb);
		HEXDUMP16(WARN,"sense data: ",&devstat->ii.sense.data[0]);
	}

	irq_ptr->io_result_cstat=ioinfo[irq]->devstat.cstat;
	irq_ptr->io_result_dstat=ioinfo[irq]->devstat.dstat;
	irq_ptr->io_result_flags=ioinfo[irq]->devstat.flag;

	if ( (rqparam==QDIO_DOING_ACTIVATE) &&
	     (cstat & SCHN_STAT_PCI) ) {
#ifdef QDIO_PERFORMANCE_STATS
		perf_stats.pcis++;
		perf_stats.start_time_inbound=NOW;
#endif /* QDIO_PERFORMANCE_STATS */
		for (i=0;i<irq_ptr->no_input_qs;i++) {
			q=irq_ptr->input_qs[i];
			if (q->is_input_q&QDIO_FLAG_NO_INPUT_INTERRUPT_CONTEXT)
				qdio_mark_q(q);
			else {
#ifdef QDIO_PERFORMANCE_STATS
				perf_stats.tl_runs--;
#endif /* QDIO_PERFORMANCE_STATS */
				qdio_inbound_processing(q);
			}
		}
		if (irq_ptr->hydra_gives_outbound_pcis) {
 			for (i=0;i<irq_ptr->no_output_qs;i++) {
 				q=irq_ptr->output_qs[i];
#ifdef QDIO_PERFORMANCE_STATS
				perf_stats.tl_runs--;
#endif /* QDIO_PERFORMANCE_STATS */
				if (!qdio_is_outbound_q_done(q)) {
					if (!irq_ptr->sync_done_on_outb_pcis) {
						SYNC_MEMORY;
					}
					qdio_outbound_processing(q);
				}
			}
		}
		return;
	}

	if ( (rqparam==QDIO_DOING_ACTIVATE) &&
	     (!cstat) &&
	     (!dstat) ) {
		rqparam=QDIO_DOING_CLEANUP;
	}

	if ( (rqparam==QDIO_DOING_ESTABLISH) &&
	     ( (cstat) ||
	       (dstat & ~(DEV_STAT_CHN_END|DEV_STAT_DEV_END)) ) ) {
		sprintf(dbf_text,"ick1%4x",irq);
		QDIO_DBF_TEXT2(1,trace,dbf_text);
		QDIO_DBF_HEX2(0,trace,&rqparam,sizeof(int));
		QDIO_DBF_HEX2(0,trace,&dstat,sizeof(int));
		QDIO_DBF_HEX2(0,trace,&cstat,sizeof(int));
		QDIO_PRINT_ERR("received check condition on establish " \
			       "queues on irq 0x%x (cs=x%x, ds=x%x).\n",
			       irq,cstat,dstat);
		qdio_set_state(irq_ptr,QDIO_IRQ_STATE_STOPPED);
		return;
	}

	if ( ( (rqparam==QDIO_DOING_ACTIVATE) &&
	       (dstat==(DEV_STAT_CHN_END|DEV_STAT_DEV_END)) ) ||
	     ( (rqparam==QDIO_DOING_CLEANUP) || (!rqparam) ) ) {
		qdio_wakeup(&irq_ptr->interrupt_has_been_cleaned,irq_ptr);
		return;
	}

	if ( (rqparam==QDIO_DOING_ACTIVATE) &&
	     ( (cstat & ~SCHN_STAT_PCI) ||
	       (dstat) ) ) {
		sprintf(dbf_text,"ick2%4x",irq);
		QDIO_DBF_TEXT2(1,trace,dbf_text);
		QDIO_DBF_HEX2(0,trace,&rqparam,sizeof(int));
		QDIO_DBF_HEX2(0,trace,&dstat,sizeof(int));
		QDIO_DBF_HEX2(0,trace,&cstat,sizeof(int));
		QDIO_PRINT_ERR("received check condition on activate " \
			       "queues on irq 0x%x (cs=x%x, ds=x%x).\n",
			       irq,cstat,dstat);
		if (irq_ptr->no_input_qs) {
			q=irq_ptr->input_qs[0];
		} else if (irq_ptr->no_output_qs) {
			q=irq_ptr->output_qs[0];
		} else {
			QDIO_PRINT_ERR("oops... no queue registered on irq " \
				  "0x%x!?\n",irq);
			goto omit_handler_call;
		}
		q->handler(q->irq,QDIO_STATUS_ACTIVATE_CHECK_CONDITION|
			   QDIO_STATUS_LOOK_FOR_ERROR,
			   0,0,0,-1,-1,q->int_parm);
	omit_handler_call:
		qdio_set_state(irq_ptr,QDIO_IRQ_STATE_STOPPED);
		return;
	}

	qdio_wakeup(&irq_ptr->interrupt_has_arrived,irq_ptr);
}

/* this is for mp3 */
int qdio_synchronize(int irq,unsigned int flags,
		     unsigned int queue_number)
{
	unsigned int gpr2,gpr3;
	int cc;
	qdio_q_t *q;
	qdio_irq_t *irq_ptr;
	char dbf_text[15]="SyncXXXX";
	void *ptr;

	*((int*)(&dbf_text[4]))=irq;
	QDIO_DBF_HEX4(0,trace,dbf_text,QDIO_DBF_TRACE_LEN);
	*((int*)(&dbf_text[0]))=flags;
	*((int*)(&dbf_text[4]))=queue_number;
	QDIO_DBF_HEX4(0,trace,dbf_text,QDIO_DBF_TRACE_LEN);

	irq_ptr=qdio_get_irq_ptr(irq);
	if (!irq_ptr) return -ENODEV;

	if (flags&QDIO_FLAG_SYNC_INPUT) {
		q=irq_ptr->input_qs[queue_number];
		if (!q) return -EINVAL;

		gpr2=0;
		gpr3=q->mask;
	} else if (flags&QDIO_FLAG_SYNC_OUTPUT) {
		q=irq_ptr->output_qs[queue_number];
		if (!q) return -EINVAL;

		gpr2=q->mask;
		gpr3=0;
	} else return -EINVAL;

#ifdef QDIO_32_BIT
	asm volatile (
		"lhi	0,2	\n\t"
		"lr	1,%1	\n\t"
		"lr	2,%2	\n\t"
		"lr	3,%3	\n\t"
		"siga   0	\n\t"
		"ipm	%0	\n\t"
		"srl	%0,28	\n\t"
		: "=d" (cc)
		: "d" (0x10000|q->irq), "d" (gpr2), "d" (gpr3)
		: "cc", "0", "1", "2", "3"
		);
#else /* QDIO_32_BIT */
	asm volatile (
		"lghi	0,2	\n\t"
		"llgfr	1,%1	\n\t"
		"llgfr	2,%2	\n\t"
		"llgfr	3,%3	\n\t"
		"siga   0	\n\t"
		"ipm	%0	\n\t"
		"srl	%0,28	\n\t"
		: "=d" (cc)
		: "d" (0x10000|q->irq), "d" (gpr2), "d" (gpr3)
		: "cc", "0", "1", "2", "3"
		);
#endif /* QDIO_32_BIT */

	ptr=&cc;
	if (cc) QDIO_DBF_HEX3(0,trace,&ptr,sizeof(int));

	return cc;
}

unsigned char qdio_get_slsb_state(int irq,unsigned int flag,
				  unsigned int queue_number,
				  unsigned int qidx)
{
	qdio_irq_t *irq_ptr;
	qdio_q_t *q;

	irq_ptr=qdio_get_irq_ptr(irq);
	if (!irq_ptr) return SLSB_ERROR_DURING_LOOKUP;

	if (flag&QDIO_FLAG_SYNC_INPUT) {
		q=irq_ptr->input_qs[queue_number];
	} else if (flag&QDIO_FLAG_SYNC_OUTPUT) {
		q=irq_ptr->output_qs[queue_number];
	} else return SLSB_ERROR_DURING_LOOKUP;

	return q->slsb.acc.val[qidx&(QDIO_MAX_BUFFERS_PER_Q-1)];
}

static int qdio_chsc(qdio_chsc_area_t *chsc_area)
{
	int cc;

#ifdef QDIO_32_BIT
	asm volatile (
		".insn	rre,0xb25f0000,%1,0	\n\t"
		"ipm	%0	\n\t"
		"srl	%0,28	\n\t"
		: "=d" (cc) : "d" (chsc_area) : "cc"
		);
#else /* QDIO_32_BIT */
	asm volatile (
		".insn	rre,0xb25f0000,%1,0	\n\t"
		"ipm	%0	\n\t"
		"srl	%0,28	\n\t"
		: "=d" (cc) : "d" (chsc_area) : "cc"
		);
#endif /* QDIO_32_BIT */

	return cc;
}

static unsigned char qdio_check_siga_needs(int sch)
{
	int resp_code,result;

	memset(chsc_area,0,sizeof(qdio_chsc_area_t));
	chsc_area->request_block.command_code1=0x0010; /* length */
	chsc_area->request_block.command_code2=0x0024; /* op code */
	chsc_area->request_block.first_sch=sch;
	chsc_area->request_block.last_sch=sch;

	result=qdio_chsc(chsc_area);

	if (result) {
		QDIO_PRINT_WARN("CHSC returned cc %i. Using all " \
				"SIGAs for sch x%x.\n",
				result,sch);
		return -1; /* all flags set */
	}

	resp_code=chsc_area->request_block.operation_data_area.
		store_qdio_data_response.response_code;
	if (resp_code!=QDIO_CHSC_RESPONSE_CODE_OK) {
		QDIO_PRINT_WARN("response upon checking SIGA needs " \
				"is 0x%x. Using all SIGAs for sch x%x.\n",
				resp_code,sch);
		return -1; /* all flags set */
	}
	if (
	    (!(chsc_area->request_block.operation_data_area.
	       store_qdio_data_response.flags&CHSC_FLAG_QDIO_CAPABILITY)) ||
	    (!(chsc_area->request_block.operation_data_area.
	       store_qdio_data_response.flags&CHSC_FLAG_VALIDITY)) ||
	    (chsc_area->request_block.operation_data_area.
	     store_qdio_data_response.sch!=sch)
	    ) {
		QDIO_PRINT_WARN("huh? problems checking out sch x%x... " \
				"using all SIGAs.\n",sch);
		return CHSC_FLAG_SIGA_INPUT_NECESSARY |
			CHSC_FLAG_SIGA_OUTPUT_NECESSARY |
			CHSC_FLAG_SIGA_SYNC_NECESSARY; /* worst case */
	}

	return chsc_area->request_block.operation_data_area.
		store_qdio_data_response.qdioac;
}

static int qdio_check_for_hydra_thinints(void)
{
	int i,result;

	memset(chsc_area,0,sizeof(qdio_chsc_area_t));
	chsc_area->request_block.command_code1=0x0010;
	chsc_area->request_block.command_code2=0x0010;
	result=qdio_chsc(chsc_area);

	if (result) {
		QDIO_PRINT_WARN("CHSC returned cc %i. Won't use adapter " \
				"interrupts for any Hydra.\n",result);
		return 0;
	}

	i=chsc_area->request_block.operation_data_area.
		store_qdio_data_response.response_code;
	if (i!=1) {
		QDIO_PRINT_WARN("Was not able to determine general " \
				"characteristics of all Hydras aboard.\n");
		return 0;
	}

	/* 4: request block
	 * 2: general char
	 * 512: chsc char */
	/* check for bit 67 */
	if ( (*(((unsigned int*)(chsc_area))+4+2+2)&0x10000000)!=0x10000000) {
		return 0;
	} else {
		return 1;
	}
}

/* the chsc_area is locked by the lock in qdio_activate */
static unsigned int tiqdio_check_chsc_availability(void) {
	int result;
	int i;

	memset(chsc_area,0,sizeof(qdio_chsc_area_t));
	chsc_area->request_block.command_code1=0x0010;
	chsc_area->request_block.command_code2=0x0010;
	result=qdio_chsc(chsc_area);
	if (result) {
		QDIO_PRINT_WARN("Was not able to determine " \
				"available CHSCs, cc=%i.\n",
				result);
		result=-EIO;
		goto exit;
	}
	result=0;
	i=chsc_area->request_block.operation_data_area.
		store_qdio_data_response.response_code;
	if (i!=1) {
		QDIO_PRINT_WARN("Was not able to determine " \
				"available CHSCs.\n");
		result=-EIO;
		goto exit;
	}
	/* 4: request block
	 * 2: general char
	 * 512: chsc char */
	/* check for bit 41 */
	if ( (*(((unsigned int*)(chsc_area))+4+2+1)&0x00400000)!=0x00400000) {
		QDIO_PRINT_WARN("Adapter interruption facility not " \
				"installed.\n");
		result=-ENOENT;
		goto exit;
	}
	/* check for bits 107 and 108 */
	if ( (*(((unsigned int*)(chsc_area))+4+512+3)&0x00180000)!=
	     0x00180000 ) {
		QDIO_PRINT_WARN("Set Chan Subsys. Char. & Fast-CHSCs " \
				"not available.\n");
		result=-ENOENT;
		goto exit;
	}
exit:
	return result;
}

/* the chsc_area is locked by the lock in qdio_activate */
static unsigned int tiqdio_set_subchannel_ind(qdio_irq_t *irq_ptr,
					     int reset_to_zero)
{
	unsigned long real_addr_local_summary_bit;
	unsigned long real_addr_dev_st_chg_ind;
	void *ptr;
	char dbf_text[15];

	unsigned int resp_code;
	int result;

	if (!irq_ptr->is_thinint_irq) return -ENODEV;

	if (reset_to_zero) {
		real_addr_local_summary_bit=0;
		real_addr_dev_st_chg_ind=0;
	} else {
		real_addr_local_summary_bit=
			virt_to_phys((volatile void *)indicators);
		real_addr_dev_st_chg_ind=
			virt_to_phys((volatile void *)irq_ptr->dev_st_chg_ind);
	}

	memset(chsc_area,0,sizeof(qdio_chsc_area_t));
	chsc_area->request_block.command_code1=0x0fe0;
	chsc_area->request_block.command_code2=0x0021;
	chsc_area->request_block.operation_code=0;
	chsc_area->request_block.image_id=0;

	chsc_area->request_block.operation_data_area.set_chsc.
		summary_indicator_addr=real_addr_local_summary_bit;
	chsc_area->request_block.operation_data_area.set_chsc.
		subchannel_indicator_addr=real_addr_dev_st_chg_ind;
	chsc_area->request_block.operation_data_area.set_chsc.
		ks=QDIO_STORAGE_ACC_KEY;
	chsc_area->request_block.operation_data_area.set_chsc.
		kc=QDIO_STORAGE_ACC_KEY;
	chsc_area->request_block.operation_data_area.set_chsc.
		isc=TIQDIO_THININT_ISC;
	chsc_area->request_block.operation_data_area.set_chsc.
		subsystem_id=(1<<16)+irq_ptr->irq;

	result=qdio_chsc(chsc_area);
	if (result) {
		QDIO_PRINT_WARN("could not set indicators on irq x%x, " \
				"cc=%i.\n",irq_ptr->irq,result);
		return -EIO;
	}

	resp_code=chsc_area->response_block.response_code;
	if (resp_code!=QDIO_CHSC_RESPONSE_CODE_OK) {
		QDIO_PRINT_WARN("response upon setting indicators " \
				"is 0x%x.\n",resp_code);
		sprintf(dbf_text,"sidR%4x",resp_code);
		QDIO_DBF_TEXT1(0,trace,dbf_text);
		QDIO_DBF_TEXT1(0,setup,dbf_text);
		ptr=&chsc_area->response_block;
		QDIO_DBF_HEX2(1,setup,&ptr,QDIO_DBF_SETUP_LEN);
		return -EIO;
	}

	QDIO_DBF_TEXT2(0,setup,"setscind");
	QDIO_DBF_HEX2(0,setup,&real_addr_local_summary_bit,
		      sizeof(unsigned long));
	QDIO_DBF_HEX2(0,setup,&real_addr_dev_st_chg_ind,sizeof(unsigned long));
	return 0;
}

/* chsc_area would have to be locked if called from outside qdio_activate */
static unsigned int tiqdio_set_delay_target(qdio_irq_t *irq_ptr,
       					   unsigned long delay_target)
{
	unsigned int resp_code;
	int result;
	void *ptr;
	char dbf_text[15];

	if (!irq_ptr->is_thinint_irq) return -ENODEV;

	memset(chsc_area,0,sizeof(qdio_chsc_area_t));
	chsc_area->request_block.command_code1=0x0fe0;
	chsc_area->request_block.command_code2=0x1027;
	chsc_area->request_block.operation_data_area.set_chsc_fast.
		delay_target=delay_target<<16;

	result=qdio_chsc(chsc_area);
	if (result) {
		QDIO_PRINT_WARN("could not set delay target on irq x%x, " \
				"cc=%i. Continuing.\n",irq_ptr->irq,result);
		return -EIO;
	}

	resp_code=chsc_area->response_block.response_code;
	if (resp_code!=QDIO_CHSC_RESPONSE_CODE_OK) {
		QDIO_PRINT_WARN("response upon setting delay target " \
				"is 0x%x. Continuing.\n",resp_code);
		sprintf(dbf_text,"sdtR%4x",resp_code);
		QDIO_DBF_TEXT1(0,trace,dbf_text);
		QDIO_DBF_TEXT1(0,setup,dbf_text);
		ptr=&chsc_area->response_block;
		QDIO_DBF_HEX2(1,trace,&ptr,QDIO_DBF_TRACE_LEN);
	}
	QDIO_DBF_TEXT2(0,trace,"delytrgt");
	QDIO_DBF_HEX2(0,trace,&delay_target,sizeof(unsigned long));
	return 0;
}

int qdio_cleanup(int irq,int how)
{
	qdio_irq_t *irq_ptr;
	int i,result;
	int do_an_irqrestore=0;
	unsigned long flags;
	int timeout;
	char dbf_text[15]="12345678";

	result=0;
	sprintf(dbf_text,"qcln%4x",irq);
	QDIO_DBF_TEXT1(0,trace,dbf_text);
	QDIO_DBF_TEXT0(0,setup,dbf_text);

	irq_ptr=qdio_get_irq_ptr(irq);
	if (!irq_ptr) return -ENODEV;

	spin_lock(&irq_ptr->setting_up_lock);

	/* mark all qs as uninteresting */
	for (i=0;i<irq_ptr->no_input_qs;i++) {
		atomic_set(&irq_ptr->input_qs[i]->is_in_shutdown,1);
	}
	for (i=0;i<irq_ptr->no_output_qs;i++) {
		atomic_set(&irq_ptr->output_qs[i]->is_in_shutdown,1);
	}

	tasklet_kill(&tiqdio_tasklet);

	for (i=0;i<irq_ptr->no_input_qs;i++) {
		qdio_unmark_q(irq_ptr->input_qs[i]);
		tasklet_kill(&irq_ptr->input_qs[i]->tasklet);
		if (qdio_wait_for_no_use_count(&irq_ptr->input_qs[i]->
					       use_count))
			result=-EINPROGRESS;
	}

	for (i=0;i<irq_ptr->no_output_qs;i++) {
		tasklet_kill(&irq_ptr->output_qs[i]->tasklet);
		if (qdio_wait_for_no_use_count(&irq_ptr->output_qs[i]->
					       use_count))
			result=-EINPROGRESS;
	}

	atomic_set(&irq_ptr->interrupt_has_been_cleaned,0);

	/* cleanup subchannel */
	s390irq_spin_lock_irqsave(irq,flags);
	if (how&QDIO_FLAG_CLEANUP_USING_CLEAR) {
		clear_IO(irq_ptr->irq,QDIO_DOING_CLEANUP,0);
		timeout=QDIO_CLEANUP_CLEAR_TIMEOUT;
	} else if (how&QDIO_FLAG_CLEANUP_USING_HALT) {
		halt_IO(irq_ptr->irq,QDIO_DOING_CLEANUP,0);
		timeout=QDIO_CLEANUP_HALT_TIMEOUT;
	} else { /* default behaviour */
		halt_IO(irq_ptr->irq,QDIO_DOING_CLEANUP,0);
		timeout=QDIO_CLEANUP_HALT_TIMEOUT;
	}
	s390irq_spin_unlock_irqrestore(irq,flags);

	if (qdio_sleepon(&irq_ptr->interrupt_has_been_cleaned,
			 timeout,irq_ptr)==-ETIME) {
		s390irq_spin_lock_irqsave(irq,flags);
		QDIO_PRINT_INFO("Did not get interrupt on %s_IO, " \
				"irq=0x%x.\n",
				(how==QDIO_FLAG_CLEANUP_USING_CLEAR)?
				"clear":"halt",irq);
		do_an_irqrestore=1;
	}

	if (irq_ptr->is_thinint_irq) {
		qdio_put_indicator((__u32*)irq_ptr->dev_st_chg_ind);
		tiqdio_set_subchannel_ind(irq_ptr,1); /* reset adapter
							interrupt indicators */
	}

	/* exchange int handlers, if necessary */
	if ((void*)ioinfo[irq]->irq_desc.handler
	    ==(void*)qdio_handler) {
		ioinfo[irq]->irq_desc.handler=
			irq_ptr->original_int_handler;
/*		irq_ptr->original_int_handler=NULL; */
	}

	qdio_set_state(irq_ptr,QDIO_IRQ_STATE_INACTIVE);

	if (do_an_irqrestore)
		s390irq_spin_unlock_irqrestore(irq,flags);

	spin_unlock(&irq_ptr->setting_up_lock);

	qdio_remove_irq_ptr(irq_ptr);
	qdio_release_irq_memory(irq_ptr);

	QDIO_DBF_TEXT3(0,setup,"MOD_DEC_");
	MOD_DEC_USE_COUNT;

	return result;
}

unsigned long qdio_get_characteristics(int irq)
{
	qdio_irq_t *irq_ptr;
	int result=0;

	irq_ptr=qdio_get_irq_ptr(irq);
	if (!irq_ptr) return -ENODEV;

	if (irq_ptr->state!=QDIO_IRQ_STATE_ACTIVE) {
		return -EBUSY;
	}

	if (irq_ptr->state==QDIO_IRQ_STATE_INACTIVE) {
		result|=QDIO_STATE_INACTIVE;
	} else if (irq_ptr->state==QDIO_IRQ_STATE_ESTABLISHED) {
		result|=QDIO_STATE_ESTABLISHED;
	} else if (irq_ptr->state==QDIO_IRQ_STATE_ACTIVE) {
		result|=QDIO_STATE_ACTIVE;
	} else if (irq_ptr->state==QDIO_IRQ_STATE_STOPPED) {
		result|=QDIO_STATE_STOPPED;
	}

	if (irq_ptr->hydra_gives_outbound_pcis)
		result|=QDIO_STATE_MUST_USE_OUTB_PCI;

	return result;
}

int qdio_initialize(qdio_initialize_t *init_data)
{
	int i,ciw_cnt;
	unsigned long saveflags;
	qdio_irq_t *irq_ptr=NULL;
	int result,result2;
	int found;
	unsigned long flags;
	char dbf_text[20]; /* if a printf would print out more than 8 chars */

	down_interruptible(&init_sema);

	sprintf(dbf_text,"qini%4x",init_data->irq);
	QDIO_DBF_TEXT0(0,setup,dbf_text);
	QDIO_DBF_TEXT0(0,trace,dbf_text);
	sprintf(dbf_text,"qfmt:%x",init_data->q_format);
	QDIO_DBF_TEXT0(0,setup,dbf_text);
	QDIO_DBF_TEXT0(0,setup,init_data->adapter_name);
	QDIO_DBF_HEX0(0,setup,init_data->adapter_name,8);
	sprintf(dbf_text,"qpff%4x",init_data->qib_param_field_format);
	QDIO_DBF_TEXT0(0,setup,dbf_text);
	QDIO_DBF_HEX0(0,setup,&init_data->qib_param_field,sizeof(char*));
	QDIO_DBF_HEX0(0,setup,&init_data->input_slib_elements,sizeof(long*));
	QDIO_DBF_HEX0(0,setup,&init_data->output_slib_elements,sizeof(long*));
	sprintf(dbf_text,"miit%4x",init_data->min_input_threshold);
	QDIO_DBF_TEXT0(0,setup,dbf_text);
	sprintf(dbf_text,"mait%4x",init_data->min_input_threshold);
	QDIO_DBF_TEXT0(0,setup,dbf_text);
	sprintf(dbf_text,"miot%4x",init_data->max_output_threshold);
	QDIO_DBF_TEXT0(0,setup,dbf_text);
	sprintf(dbf_text,"maot%4x",init_data->max_output_threshold);
	QDIO_DBF_TEXT0(0,setup,dbf_text);
	sprintf(dbf_text,"niq:%4x",init_data->no_input_qs);
	QDIO_DBF_TEXT0(0,setup,dbf_text);
	sprintf(dbf_text,"noq:%4x",init_data->no_output_qs);
	QDIO_DBF_TEXT0(0,setup,dbf_text);
	QDIO_DBF_HEX0(0,setup,&init_data->input_handler,sizeof(void*));
	QDIO_DBF_HEX0(0,setup,&init_data->output_handler,sizeof(void*));
	QDIO_DBF_HEX0(0,setup,&init_data->int_parm,sizeof(long));
	QDIO_DBF_HEX0(0,setup,&init_data->flags,sizeof(long));
	QDIO_DBF_HEX0(0,setup,&init_data->input_sbal_addr_array,sizeof(void*));
	QDIO_DBF_HEX0(0,setup,&init_data->output_sbal_addr_array,sizeof(void*));
	flags=init_data->flags;

	/* sanity checks */
	if (qdio_get_irq_ptr(init_data->irq)) {
		result=-EBUSY;
		goto out;
	}

	if (ioinfo[init_data->irq]==INVALID_STORAGE_AREA) {
		result=-ENODEV;
		goto out;
	}

	if (!ioinfo[init_data->irq]) {
		QDIO_PRINT_WARN("ioinfo[%i] is NULL!\n",init_data->irq);
		result=-ENODEV;
		goto out;
	}

#define DEVSTAT_FLAG_IRQ_QDIO ~0UL
	if (!(ioinfo[init_data->irq]->devstat.flag&DEVSTAT_FLAG_IRQ_QDIO)) {
		QDIO_PRINT_WARN("ioinfo[%i]->devstat.flag=0x%08x\n",
				init_data->irq,
				ioinfo[init_data->irq]->devstat.flag);
		result=-ENODEV;
		goto out;
	}

	if ( (init_data->no_input_qs>QDIO_MAX_QUEUES_PER_IRQ) ||
	     (init_data->no_output_qs>QDIO_MAX_QUEUES_PER_IRQ) ||
	     ((init_data->no_input_qs) && (!init_data->input_handler)) ||
	     ((init_data->no_output_qs) && (!init_data->output_handler)) ) {
		result=-EINVAL;
		goto out;
	}

	if ( (init_data->flags&QDIO_INBOUND_0COPY_SBALS)&&
	     (!init_data->input_sbal_addr_array) ) {
		result=-EINVAL;
		goto out;
	}
	if ( (init_data->flags&QDIO_OUTBOUND_0COPY_SBALS)&&
	     (!init_data->output_sbal_addr_array) ) {
		result=-EINVAL;
		goto out;
	}

	/* create irq */
	irq_ptr=kmalloc(sizeof(qdio_irq_t),GFP_DMA);

	QDIO_DBF_TEXT0(0,setup,"irq_ptr:");
	QDIO_DBF_HEX0(0,setup,&irq_ptr,sizeof(void*));

	if (!irq_ptr) {
		QDIO_PRINT_ERR("kmalloc of irq_ptr failed!\n");
		result=-ENOMEM;
		goto out;
	}

	memset(irq_ptr,0,sizeof(qdio_irq_t)); /* wipes qib.ac,
						 required by ar7063 */

	irq_ptr->qdr=kmalloc(sizeof(qdr_t),GFP_DMA);
  	if (!(irq_ptr->qdr)) {
   		kfree(irq_ptr->qdr);
   		kfree(irq_ptr);
    		QDIO_PRINT_ERR("kmalloc of irq_ptr->qdr failed!\n");
     		result=-ENOMEM;
      		goto out;
       	}
	memset(irq_ptr->qdr,0,sizeof(qdr_t));
	QDIO_DBF_TEXT0(0,setup,"qdr:");
	QDIO_DBF_HEX0(0,setup,&irq_ptr->qdr,sizeof(void*));

	init_waitqueue_head(&irq_ptr->wait_q);

	irq_ptr->int_parm=init_data->int_parm;

	irq_ptr->irq=init_data->irq;
	irq_ptr->no_input_qs=init_data->no_input_qs;
	irq_ptr->no_output_qs=init_data->no_output_qs;

	if (init_data->q_format==QDIO_IQDIO_QFMT) {
		irq_ptr->is_iqdio_irq=1;
		irq_ptr->is_thinint_irq=1;
	} else {
		irq_ptr->is_iqdio_irq=0;
		irq_ptr->is_thinint_irq=hydra_thinints;
	}
	sprintf(dbf_text,"is_i_t%1x%1x",
		irq_ptr->is_iqdio_irq,irq_ptr->is_thinint_irq);
	QDIO_DBF_TEXT2(0,setup,dbf_text);

	if (irq_ptr->is_thinint_irq) {
		irq_ptr->dev_st_chg_ind=qdio_get_indicator();
		QDIO_DBF_HEX1(0,setup,&irq_ptr->dev_st_chg_ind,sizeof(void*));
		if (!irq_ptr->dev_st_chg_ind) {
			QDIO_PRINT_WARN("no indicator location available " \
					"for irq 0x%x\n",irq_ptr->irq);
			qdio_release_irq_memory(irq_ptr);
			result=-ENOBUFS;
			goto out;
		}
	}

	if (flags&QDIO_PFIX)
		irq_ptr->other_flags |= QDIO_PFIX;

	/* defaults */
	irq_ptr->commands.eq=DEFAULT_ESTABLISH_QS_CMD;
	irq_ptr->commands.count_eq=DEFAULT_ESTABLISH_QS_COUNT;
	irq_ptr->commands.aq=DEFAULT_ACTIVATE_QS_CMD;
	irq_ptr->commands.count_aq=DEFAULT_ACTIVATE_QS_COUNT;

	if (!qdio_alloc_qs(irq_ptr,init_data->no_input_qs,
			   init_data->no_output_qs,
			   init_data->input_handler,
			   init_data->output_handler,init_data->int_parm,
			   init_data->q_format,init_data->flags,
			   init_data->input_sbal_addr_array,
			   init_data->output_sbal_addr_array)) {
		qdio_release_irq_memory(irq_ptr);
		result=-ENOMEM;
		goto out;
	}

	qdio_set_state(irq_ptr,QDIO_IRQ_STATE_INACTIVE);

	irq_ptr->setting_up_lock=SPIN_LOCK_UNLOCKED;

	MOD_INC_USE_COUNT;
	QDIO_DBF_TEXT3(0,setup,"MOD_INC_");

	spin_lock(&irq_ptr->setting_up_lock);

	qdio_insert_irq_ptr(irq_ptr);

	qdio_fill_thresholds(irq_ptr,init_data->no_input_qs,
			     init_data->no_output_qs,
			     init_data->min_input_threshold,
			     init_data->max_input_threshold,
			     init_data->min_output_threshold,
			     init_data->max_output_threshold);

	/* fill in qdr */
	irq_ptr->qdr->qfmt=init_data->q_format;
	irq_ptr->qdr->iqdcnt=init_data->no_input_qs;
	irq_ptr->qdr->oqdcnt=init_data->no_output_qs;
	irq_ptr->qdr->iqdsz=sizeof(qdesfmt0_t)/4; /* size in words */
	irq_ptr->qdr->oqdsz=sizeof(qdesfmt0_t)/4;

	irq_ptr->qdr->qiba=QDIO_PFIX_GET_ADDR(&irq_ptr->qib);
	irq_ptr->qdr->qkey=QDIO_STORAGE_ACC_KEY;

	/* fill in qib */
	irq_ptr->qib.qfmt=init_data->q_format;
	if (init_data->no_input_qs) irq_ptr->qib.isliba=
				 QDIO_PFIX_GET_ADDR(irq_ptr->input_qs[0]->slib);
	if (init_data->no_output_qs) irq_ptr->qib.osliba=(unsigned long)
				  QDIO_PFIX_GET_ADDR(irq_ptr->output_qs[0]->slib);
	memcpy(irq_ptr->qib.ebcnam,init_data->adapter_name,8);

	qdio_set_impl_params(irq_ptr,init_data->qib_param_field_format,
			     init_data->qib_param_field,
			     init_data->no_input_qs,
			     init_data->no_output_qs,
			     init_data->input_slib_elements,
			     init_data->output_slib_elements);

	/* first input descriptors, then output descriptors */
	for (i=0;i<init_data->no_input_qs;i++) {
		irq_ptr->input_qs[i]->is_iqdio_q=
			(init_data->q_format==QDIO_IQDIO_QFMT)?1:0;
		irq_ptr->input_qs[i]->is_thinint_q=irq_ptr->is_thinint_irq;

		irq_ptr->qdr->qdf0[i].sliba=
			QDIO_PFIX_GET_ADDR(irq_ptr->input_qs[i]->slib);

		irq_ptr->qdr->qdf0[i].sla=
			QDIO_PFIX_GET_ADDR(irq_ptr->input_qs[i]->sl);

		irq_ptr->qdr->qdf0[i].slsba=
			QDIO_PFIX_GET_ADDR
			((void *)&irq_ptr->input_qs[i]->slsb.acc.val[0]);

		irq_ptr->qdr->qdf0[i].akey=QDIO_STORAGE_ACC_KEY;
		irq_ptr->qdr->qdf0[i].bkey=QDIO_STORAGE_ACC_KEY;
		irq_ptr->qdr->qdf0[i].ckey=QDIO_STORAGE_ACC_KEY;
		irq_ptr->qdr->qdf0[i].dkey=QDIO_STORAGE_ACC_KEY;
	}

	for (i=0;i<init_data->no_output_qs;i++) {
		irq_ptr->output_qs[i]->is_iqdio_q=
			(init_data->q_format==QDIO_IQDIO_QFMT)?1:0;
		irq_ptr->output_qs[i]->is_thinint_q=irq_ptr->is_thinint_irq;

		irq_ptr->qdr->qdf0[i+init_data->no_input_qs].sliba=
			QDIO_PFIX_GET_ADDR(irq_ptr->output_qs[i]->slib);

		irq_ptr->qdr->qdf0[i+init_data->no_input_qs].sla=
			QDIO_PFIX_GET_ADDR(irq_ptr->output_qs[i]->sl);

		irq_ptr->qdr->qdf0[i+init_data->no_input_qs].slsba=
			QDIO_PFIX_GET_ADDR
			((void *)&irq_ptr->output_qs[i]->slsb.acc.val[0]);

		irq_ptr->qdr->qdf0[i+init_data->no_input_qs].akey=
			QDIO_STORAGE_ACC_KEY;
		irq_ptr->qdr->qdf0[i+init_data->no_input_qs].bkey=
			QDIO_STORAGE_ACC_KEY;
		irq_ptr->qdr->qdf0[i+init_data->no_input_qs].ckey=
			QDIO_STORAGE_ACC_KEY;
		irq_ptr->qdr->qdf0[i+init_data->no_input_qs].dkey=
			QDIO_STORAGE_ACC_KEY;
	}
	/* qdr, qib, sls, slsbs, slibs, sbales filled. */

	s390irq_spin_lock_irqsave(irq_ptr->irq,saveflags);
	/* keep track of original int handler */
	irq_ptr->original_int_handler=ioinfo[irq_ptr->irq]->irq_desc.handler;

	/* insert qdio int handler */
	ioinfo[irq_ptr->irq]->irq_desc.handler=(void*)qdio_handler;

	s390irq_spin_unlock_irqrestore(irq_ptr->irq,saveflags);

#define TAKEOVER_CIW(x) irq_ptr->commands.x=ioinfo[irq_ptr->irq]->senseid.ciw[ciw_cnt].cmd; irq_ptr->commands.count_##x=ioinfo[irq_ptr->irq]->senseid.ciw[ciw_cnt].count
	/* get qdio commands */
	found=0;
	for (ciw_cnt=0;ciw_cnt<MAX_CIWS;ciw_cnt++) {
		switch (ioinfo[irq_ptr->irq]->senseid.ciw[ciw_cnt].ct) {
		case CIW_TYPE_RCD: TAKEOVER_CIW(rcd); break;
		case CIW_TYPE_SII: TAKEOVER_CIW(sii); break;
		case CIW_TYPE_RNI: TAKEOVER_CIW(rni); break;
		case CIW_TYPE_EQUEUE: TAKEOVER_CIW(eq); found++; break;
		case CIW_TYPE_AQUEUE: TAKEOVER_CIW(aq); found++; break;
		}
		if ( (ciw_cnt>0) &&
		     (ioinfo[irq_ptr->irq]->senseid.ciw[ciw_cnt].ct==0) &&
		     (ioinfo[irq_ptr->irq]->senseid.ciw[ciw_cnt].cmd==0) &&
		     (ioinfo[irq_ptr->irq]->senseid.ciw[ciw_cnt].count) )
			break;
	}
	if (found<2) {
		QDIO_DBF_TEXT2(1,setup,"no ciws");
		QDIO_PRINT_INFO("No CIWs found for QDIO commands. Trying to " \
				"use defaults.\n");
	}

	/* the thinint CHSC stuff */
	if (irq_ptr->is_thinint_irq) {
/*		iqdio_enable_adapter_int_facility(irq_ptr);*/

		if (tiqdio_check_chsc_availability()) {
			QDIO_PRINT_ERR("Not all CHSCs supported. " \
				       "Continuing.\n");
		}
		result=tiqdio_set_subchannel_ind(irq_ptr,0);
		if (result) {
			spin_unlock(&irq_ptr->setting_up_lock);
			qdio_cleanup(irq_ptr->irq,
				     QDIO_FLAG_CLEANUP_USING_CLEAR);
			goto out2;
		}
		tiqdio_set_delay_target(irq_ptr,TIQDIO_DELAY_TARGET);
	}

	/* establish q */
	irq_ptr->ccw.cmd_code=irq_ptr->commands.eq;
	irq_ptr->ccw.flags=CCW_FLAG_SLI;
	irq_ptr->ccw.count=irq_ptr->commands.count_eq;
	irq_ptr->ccw.cda=QDIO_GET_32BIT_ADDR(QDIO_PFIX_GET_ADDR(irq_ptr->qdr));

	s390irq_spin_lock_irqsave(irq_ptr->irq,saveflags);
	atomic_set(&irq_ptr->interrupt_has_arrived,0);

	result=do_IO(irq_ptr->irq,&irq_ptr->ccw,QDIO_DOING_ESTABLISH,0,
		     ((irq_ptr->other_flags&QDIO_PFIX)?DOIO_USE_DIAG98:0));
	if (result) {
		result2=do_IO(irq_ptr->irq,&irq_ptr->ccw,
			      QDIO_DOING_ESTABLISH,0,
			      ((irq_ptr->other_flags&QDIO_PFIX)?
			       DOIO_USE_DIAG98:0));
		sprintf(dbf_text,"eq:io%4x",result);
		QDIO_DBF_TEXT2(1,setup,dbf_text);
		if (result2) {
			sprintf(dbf_text,"eq:io%4x",result);
			QDIO_DBF_TEXT2(1,setup,dbf_text);
		}
		QDIO_PRINT_WARN("establish queues on irq %04x: do_IO " \
                           "returned %i, next try returned %i\n",
                           irq_ptr->irq,result,result2);
		result=result2;
	}

	s390irq_spin_unlock_irqrestore(irq_ptr->irq,saveflags);

	if (result) {
		spin_unlock(&irq_ptr->setting_up_lock);
		qdio_cleanup(irq_ptr->irq,QDIO_FLAG_CLEANUP_USING_CLEAR);
		goto out2;
	}

	result=qdio_sleepon(&irq_ptr->interrupt_has_arrived,
			    QDIO_ESTABLISH_TIMEOUT,irq_ptr);

	if (result) {
		QDIO_PRINT_ERR("establish queues on irq %04x: timed out\n",
			   irq_ptr->irq);
		QDIO_DBF_TEXT2(1,setup,"eq:timeo");
		spin_unlock(&irq_ptr->setting_up_lock);
		qdio_cleanup(irq_ptr->irq,QDIO_FLAG_CLEANUP_USING_CLEAR);
		goto out2;
	}

	if (!(irq_ptr->io_result_dstat & DEV_STAT_DEV_END)) {
		QDIO_DBF_TEXT2(1,setup,"eq:no de");
		QDIO_DBF_HEX2(0,setup,&irq_ptr->io_result_dstat,
			      sizeof(irq_ptr->io_result_dstat));
		QDIO_DBF_HEX2(0,setup,&irq_ptr->io_result_cstat,
			      sizeof(irq_ptr->io_result_cstat));
		QDIO_DBF_HEX2(0,setup,&irq_ptr->io_result_flags,
			      sizeof(irq_ptr->io_result_flags));
		QDIO_PRINT_ERR("establish queues on irq %04x: didn't get " \
			       "device end: dstat=%02x, cstat=%02x, " \
			       "flags=%02x\n",
			       irq_ptr->irq,irq_ptr->io_result_dstat,
			       irq_ptr->io_result_cstat,
			       irq_ptr->io_result_flags);
		spin_unlock(&irq_ptr->setting_up_lock);
		qdio_cleanup(irq_ptr->irq,QDIO_FLAG_CLEANUP_USING_CLEAR);
		result=-EIO;
		goto out2;
	}

	if (irq_ptr->io_result_dstat & ~(DEV_STAT_CHN_END|DEV_STAT_DEV_END)) {
		QDIO_DBF_TEXT2(1,setup,"eq:badio");
		QDIO_DBF_HEX2(0,setup,&irq_ptr->io_result_dstat,
			      sizeof(irq_ptr->io_result_dstat));
		QDIO_DBF_HEX2(0,setup,&irq_ptr->io_result_cstat,
			      sizeof(irq_ptr->io_result_cstat));
		QDIO_DBF_HEX2(0,setup,&irq_ptr->io_result_flags,
			      sizeof(irq_ptr->io_result_flags));
		QDIO_PRINT_ERR("establish queues on irq %04x: got " \
			       "the following devstat: dstat=%02x, " \
			       "cstat=%02x, flags=%02x\n",
			       irq_ptr->irq,irq_ptr->io_result_dstat,
			       irq_ptr->io_result_cstat,
			       irq_ptr->io_result_flags);
		result=-EIO;
		goto out;
	}

	if (MACHINE_IS_VM)
		irq_ptr->qdioac=qdio_check_siga_needs(irq_ptr->irq);
	else { 
                irq_ptr->qdioac=CHSC_FLAG_SIGA_INPUT_NECESSARY
                        | CHSC_FLAG_SIGA_OUTPUT_NECESSARY;
        }
	sprintf(dbf_text,"qdioac%2x",irq_ptr->qdioac);
	QDIO_DBF_TEXT2(0,setup,dbf_text);

	sprintf(dbf_text,"qib ac%2x",irq_ptr->qib.ac);
	QDIO_DBF_TEXT2(0,setup,dbf_text);

	if (init_data->flags&QDIO_USE_OUTBOUND_PCIS) {
		irq_ptr->hydra_gives_outbound_pcis=
			irq_ptr->qib.ac&QIB_AC_OUTBOUND_PCI_SUPPORTED;
	} else {
		irq_ptr->hydra_gives_outbound_pcis=0;
	}
	irq_ptr->sync_done_on_outb_pcis=
		irq_ptr->qdioac&CHSC_FLAG_SIGA_SYNC_DONE_ON_OUTB_PCIS;

	for (i=0;i<init_data->no_input_qs;i++) {
		irq_ptr->input_qs[i]->siga_sync=
			irq_ptr->qdioac&CHSC_FLAG_SIGA_SYNC_NECESSARY;
		irq_ptr->input_qs[i]->siga_in=
			irq_ptr->qdioac&CHSC_FLAG_SIGA_INPUT_NECESSARY;
		irq_ptr->input_qs[i]->siga_out=
			irq_ptr->qdioac&CHSC_FLAG_SIGA_OUTPUT_NECESSARY;
		irq_ptr->input_qs[i]->siga_sync_done_on_thinints=
			irq_ptr->qdioac&CHSC_FLAG_SIGA_SYNC_DONE_ON_THININTS;
		irq_ptr->input_qs[i]->hydra_gives_outbound_pcis=
			irq_ptr->hydra_gives_outbound_pcis;
		irq_ptr->input_qs[i]->siga_sync_done_on_outb_tis=
			( (irq_ptr->qdioac&
			   (CHSC_FLAG_SIGA_SYNC_DONE_ON_OUTB_PCIS|
			    CHSC_FLAG_SIGA_SYNC_DONE_ON_THININTS))==
			  (CHSC_FLAG_SIGA_SYNC_DONE_ON_OUTB_PCIS|
			   CHSC_FLAG_SIGA_SYNC_DONE_ON_THININTS) );
	}

	for (i=0;i<init_data->no_output_qs;i++) {
		irq_ptr->output_qs[i]->siga_sync=
			irq_ptr->qdioac&CHSC_FLAG_SIGA_SYNC_NECESSARY;
		irq_ptr->output_qs[i]->siga_in=
			irq_ptr->qdioac&CHSC_FLAG_SIGA_INPUT_NECESSARY;
		irq_ptr->output_qs[i]->siga_out=
			irq_ptr->qdioac&CHSC_FLAG_SIGA_OUTPUT_NECESSARY;
		irq_ptr->output_qs[i]->siga_sync_done_on_thinints=
			irq_ptr->qdioac&CHSC_FLAG_SIGA_SYNC_DONE_ON_THININTS;
		irq_ptr->output_qs[i]->hydra_gives_outbound_pcis=
			irq_ptr->hydra_gives_outbound_pcis;
		irq_ptr->output_qs[i]->siga_sync_done_on_outb_tis=
			( (irq_ptr->qdioac&
			   (CHSC_FLAG_SIGA_SYNC_DONE_ON_OUTB_PCIS|
			    CHSC_FLAG_SIGA_SYNC_DONE_ON_THININTS))==
			  (CHSC_FLAG_SIGA_SYNC_DONE_ON_OUTB_PCIS|
			   CHSC_FLAG_SIGA_SYNC_DONE_ON_THININTS) );
	}

	if (init_data->qib_param_field)
		memcpy(init_data->qib_param_field,irq_ptr->qib.parm,
		       QDIO_MAX_BUFFERS_PER_Q);

	qdio_set_state(irq_ptr,QDIO_IRQ_STATE_ESTABLISHED);

 out:
	if (irq_ptr) {
		spin_unlock(&irq_ptr->setting_up_lock);
	}
 out2:
	up(&init_sema);

	return result;
}

int qdio_activate(int irq,int flags)
{
	qdio_irq_t *irq_ptr;
	int i,result=0,result2;
	unsigned long saveflags;
	char dbf_text[20]; /* see qdio_initialize */

	irq_ptr=qdio_get_irq_ptr(irq);
	if (!irq_ptr) return -ENODEV;

	spin_lock(&irq_ptr->setting_up_lock);
	if (irq_ptr->state==QDIO_IRQ_STATE_INACTIVE) {
		result=-EBUSY;
		goto out;
	}

	sprintf(dbf_text,"qact%4x",irq);
	QDIO_DBF_TEXT2(0,setup,dbf_text);
	QDIO_DBF_TEXT2(0,trace,dbf_text);

	/* activate q */
	irq_ptr->ccw.cmd_code=irq_ptr->commands.aq;
	irq_ptr->ccw.flags=CCW_FLAG_SLI;
	irq_ptr->ccw.count=irq_ptr->commands.count_aq;
	irq_ptr->ccw.cda=QDIO_GET_32BIT_ADDR(0); /* no QDIO_PFIX_GET_ADDR here,
						    not needed */

	s390irq_spin_lock_irqsave(irq_ptr->irq,saveflags);
	atomic_set(&irq_ptr->interrupt_has_arrived,0);

	result=do_IO(irq_ptr->irq,&irq_ptr->ccw,QDIO_DOING_ACTIVATE,
		     0,DOIO_REPORT_ALL|DOIO_DENY_PREFETCH|
		     ((irq_ptr->other_flags&QDIO_PFIX)?DOIO_USE_DIAG98:0));
	if (result) {
		result2=do_IO(irq_ptr->irq,&irq_ptr->ccw,
			      QDIO_DOING_ACTIVATE,0,DOIO_REPORT_ALL|
			      ((irq_ptr->other_flags&QDIO_PFIX) ? 
			       DOIO_USE_DIAG98:0));
		sprintf(dbf_text,"aq:io%4x",result);
		QDIO_DBF_TEXT2(1,setup,dbf_text);
		if (result2) {
			sprintf(dbf_text,"aq:io%4x",result);
			QDIO_DBF_TEXT2(1,setup,dbf_text);
		}
		QDIO_PRINT_WARN("activate queues on irq %04x: do_IO " \
                           "returned %i, next try returned %i\n",
                           irq_ptr->irq,result,result2);
		result=result2;
	}

	s390irq_spin_unlock_irqrestore(irq_ptr->irq,saveflags);
	if (result) {
		goto out;
	}

	if (!(flags&QDIO_FLAG_UNDER_INTERRUPT)) {
		result=qdio_sleepon(&irq_ptr->interrupt_has_arrived,
				    QDIO_ACTIVATE_TIMEOUT,irq_ptr);

		if (result!=-ETIME) {
			QDIO_DBF_TEXT2(1,setup,"aq:badio");
			QDIO_DBF_HEX2(0,setup,&irq_ptr->io_result_dstat,
				      sizeof(irq_ptr->io_result_dstat));
			QDIO_DBF_HEX2(0,setup,&irq_ptr->io_result_cstat,
				      sizeof(irq_ptr->io_result_cstat));
			QDIO_DBF_HEX2(0,setup,&irq_ptr->io_result_flags,
				      sizeof(irq_ptr->io_result_flags));
			QDIO_PRINT_ERR("activate queues on irq %04x: got " \
				       "the following devstat: dstat=%02x, " \
				       "cstat=%02x, flags=%02x\n",
				       irq_ptr->irq,irq_ptr->io_result_dstat,
				       irq_ptr->io_result_cstat,
				       irq_ptr->io_result_flags);
			result=-EIO;
			goto out;
		} else { /* result is -ETIME, but timeout is ok for us */
			result=0;
		}
	} else result=0;

	for (i=0;i<irq_ptr->no_input_qs;i++) {
		if (irq_ptr->is_thinint_irq) {
			/* that way we know, that, if we will
			 * get interrupted
			 * by tiqdio_inbound_processing,
			 * qdio_unmark_q will
			 * not be called */
			qdio_reserve_q(irq_ptr->input_qs[i]);
			qdio_mark_tiq(irq_ptr->input_qs[i]);
			qdio_release_q(irq_ptr->input_qs[i]);
		}
	}

	if (flags&QDIO_FLAG_NO_INPUT_INTERRUPT_CONTEXT) {
		for (i=0;i<irq_ptr->no_input_qs;i++) {
			irq_ptr->input_qs[i]->is_input_q|=
				QDIO_FLAG_NO_INPUT_INTERRUPT_CONTEXT;
		}
	}

	qdio_wait_nonbusy(QDIO_ACTIVATE_DELAY);

	qdio_set_state(irq_ptr,QDIO_IRQ_STATE_ACTIVE);

 out:
	spin_unlock(&irq_ptr->setting_up_lock);

	return result;
}

/* buffers filled forwards again to make Rick happy */
static void qdio_do_qdio_fill_input(qdio_q_t *q,unsigned int qidx,
				    unsigned int count,qdio_buffer_t *buffers)
{
	for (;;) {
		if (!q->is_0copy_sbals_q) {
			memcpy((void*)q->sbal[qidx],buffers,SBAL_SIZE);
			q->qdio_buffers[qidx]=buffers;
			buffers++;
		}
		set_slsb(&q->slsb.acc.val[qidx],SLSB_CU_INPUT_EMPTY);
		count--;
		if (!count) break;
		qidx=(qidx+1)&(QDIO_MAX_BUFFERS_PER_Q-1);
	}

	/* not necessary, as the queues are synced during the SIGA read
	SYNC_MEMORY;*/
}

static inline void qdio_do_qdio_fill_output(qdio_q_t *q,unsigned int qidx,
					    unsigned int count,
					    qdio_buffer_t *buffers)
{
	for (;;) {
		if (!q->is_0copy_sbals_q) {
			memcpy((void*)q->sbal[qidx],buffers,SBAL_SIZE);
			q->qdio_buffers[qidx]=buffers;
			buffers++;
		}
		set_slsb(&q->slsb.acc.val[qidx],SLSB_CU_OUTPUT_PRIMED);
		count--;
		if (!count) break;
		qidx=(qidx+1)&(QDIO_MAX_BUFFERS_PER_Q-1);
	}

	/* SIGA write will sync the queues
	SYNC_MEMORY;*/
}

/* count must be 1 in iqdio */
int do_QDIO(int irq,unsigned int callflags,unsigned int queue_number,
	    unsigned int qidx,unsigned int count,qdio_buffer_t *buffers)
{
	qdio_q_t *q;
	qdio_irq_t *irq_ptr;
	int result;
	int used_elements;

#ifdef QDIO_DBF_LIKE_HELL
	char dbf_text[20];

	sprintf(dbf_text,"doQD%04x",irq);
	QDIO_DBF_TEXT3(0,trace,dbf_text);
#endif /* QDIO_DBF_LIKE_HELL */

	if ( (qidx>QDIO_MAX_BUFFERS_PER_Q) ||
	     (count>QDIO_MAX_BUFFERS_PER_Q) ||
	     (queue_number>QDIO_MAX_QUEUES_PER_IRQ) )
		return -EINVAL;

	if (count==0) return 0;

	irq_ptr=qdio_get_irq_ptr(irq);
	if (!irq_ptr) return -ENODEV;

#ifdef QDIO_DBF_LIKE_HELL
	if (callflags&QDIO_FLAG_SYNC_INPUT)
		QDIO_DBF_HEX3(0,trace,&irq_ptr->input_qs[queue_number],
			      sizeof(void*));
	else
		QDIO_DBF_HEX3(0,trace,&irq_ptr->output_qs[queue_number],
			      sizeof(void*));
	sprintf(dbf_text,"flag%04x",callflags);
	QDIO_DBF_TEXT3(0,trace,dbf_text);
	sprintf(dbf_text,"qi%02xct%02x",qidx,count);
	QDIO_DBF_TEXT3(0,trace,dbf_text);
	if ( ((callflags&QDIO_FLAG_SYNC_INPUT)&&
	      (!irq_ptr->input_qs[queue_number]->is_0copy_sbals_q)) ||
	     ((callflags&QDIO_FLAG_SYNC_OUTPUT)&&
	      (!irq_ptr->output_qs[queue_number]->is_0copy_sbals_q)) )
		QDIO_DBF_HEX5(0,sbal,buffers,256);
#endif /* QDIO_DBF_LIKE_HELL */

	if (irq_ptr->state!=QDIO_IRQ_STATE_ACTIVE) {
		return -EBUSY;
	}

	if (callflags&QDIO_FLAG_SYNC_INPUT) {
		/* This is the inbound handling of queues */
		q=irq_ptr->input_qs[queue_number];

		used_elements=atomic_return_add(count,
						&q->number_of_buffers_used);

		qdio_do_qdio_fill_input(q,qidx,count,buffers);

		if ((used_elements+count==QDIO_MAX_BUFFERS_PER_Q)&&
		    (callflags&QDIO_FLAG_UNDER_INTERRUPT))
			atomic_swap(&q->polling,0);

		if (!used_elements) if (!(callflags&QDIO_FLAG_DONT_SIGA)) {
			if (q->siga_in) {
				result=qdio_siga_input(q);
				if (result) {
					if (q->siga_error)
						q->error_status_flags|=
							QDIO_STATUS_MORE_THAN_ONE_SIGA_ERROR;
					q->error_status_flags|=
						QDIO_STATUS_LOOK_FOR_ERROR;
					q->siga_error=result;
				}
			}

			qdio_mark_q(q);
		}
	} else if (callflags&QDIO_FLAG_SYNC_OUTPUT) {
		/* This is the outbound handling of queues */
#ifdef QDIO_PERFORMANCE_STATS
		perf_stats.start_time_outbound=NOW;
#endif /* QDIO_PERFORMANCE_STATS */
		q=irq_ptr->output_qs[queue_number];

		qdio_do_qdio_fill_output(q,qidx,count,buffers);

		used_elements=atomic_return_add(count,
						&q->number_of_buffers_used);

		if (!(callflags&QDIO_FLAG_DONT_SIGA)) {
			if (q->is_iqdio_q) {
				/* one siga for every sbal */
				while (count--) {
					qdio_kick_outbound_q(q);
				}

				qdio_outbound_processing(q);
			} else {
				/* under VM, we do a SIGA sync
				 * unconditionally */
				SYNC_MEMORY;
				else {
					/* w/o shadow queues (else branch of
					 * SYNC_MEMORY :-/ ), we try to
					 * fast-requeue buffers */
					if (q->slsb.acc.val
					    [(qidx+QDIO_MAX_BUFFERS_PER_Q-1)
					    &(QDIO_MAX_BUFFERS_PER_Q-1)]!=
					    SLSB_CU_OUTPUT_PRIMED) {
						qdio_kick_outbound_q(q);
					} else {
#ifdef QDIO_DBF_LIKE_HELL
						QDIO_DBF_TEXT3(0,trace,
							       "fast-req");
#endif /* QDIO_DBF_LIKE_HELL */
#ifdef QDIO_PERFORMANCE_STATS
						perf_stats.fast_reqs++;
#endif /* QDIO_PERFORMANCE_STATS */
					}
				}
				/* only marking the q could take
				 * too long, the upper layer
				 * module could do a lot of
				 * traffic in that time */
				qdio_outbound_processing(q);
			}
		}

#ifdef QDIO_PERFORMANCE_STATS
		perf_stats.outbound_time+=NOW-perf_stats.start_time_outbound;
		perf_stats.outbound_cnt++;
#endif /* QDIO_PERFORMANCE_STATS */
	} else {
		QDIO_DBF_TEXT3(1,trace,"doQD:inv");
		return -EINVAL;
	}
	return 0;
}

#ifdef QDIO_PERFORMANCE_STATS
static int qdio_perf_procfile_read(char *buffer,char **buffer_location,
       				   off_t offset,int buffer_length,int *eof,
				   void *data)
{
        int c=0,bucket;
	qdio_irq_t *irq_ptr;

        /* we are always called with buffer_length=4k, so we all
           deliver on the first read */
        if (offset>0) return 0;

#define _OUTP_IT(x...) c+=sprintf(buffer+c,x)
	_OUTP_IT("i_p_nc/c=%lu/%lu\n",i_p_nc,i_p_c);
	_OUTP_IT("ii_p_nc/c=%lu/%lu\n",ii_p_nc,ii_p_c);
	_OUTP_IT("o_p_nc/c=%lu/%lu\n",o_p_nc,o_p_c);
	_OUTP_IT("Number of tasklet runs (total)                  : %u\n",
		 perf_stats.tl_runs);
	_OUTP_IT("\n");
	_OUTP_IT("Number of SIGA sync's issued                    : %u\n",
		 perf_stats.siga_syncs);
	_OUTP_IT("Number of SIGA in's issued                      : %u\n",
		 perf_stats.siga_ins);
	_OUTP_IT("Number of SIGA out's issued                     : %u\n",
		 perf_stats.siga_outs);
	_OUTP_IT("Number of PCI's caught                          : %u\n",
		 perf_stats.pcis);
	_OUTP_IT("Number of adapter interrupts caught             : %u\n",
		 perf_stats.thinints);
	_OUTP_IT("Number of fast requeues (outg. SBALs w/o SIGA)  : %u\n",
		 perf_stats.fast_reqs);
	_OUTP_IT("\n");
	_OUTP_IT("Total time of all inbound actions (us) incl. UL : %u\n",
		 perf_stats.inbound_time);
	_OUTP_IT("Number of inbound transfers                     : %u\n",
		 perf_stats.inbound_cnt);
	_OUTP_IT("Total time of all outbound do_QDIOs (us)        : %u\n",
		 perf_stats.outbound_time);
	_OUTP_IT("Number of do_QDIOs outbound                     : %u\n",
		 perf_stats.outbound_cnt);
	_OUTP_IT("\n");

	for (bucket=0;bucket<QDIO_IRQ_BUCKETS;bucket++) {
		irq_ptr=first_irq[bucket];

		while (irq_ptr) {
			_OUTP_IT("Polling time on irq %4x" \
				 "                        : %u\n",
				 irq_ptr->irq,irq_ptr->input_qs[0]->
				 timing.threshold);
			irq_ptr=irq_ptr->next;
		}
	}

        return c;
}

static struct proc_dir_entry *qdio_perf_proc_file;
#endif /* QDIO_PERFORMANCE_STATS */

static void qdio_add_procfs_entry(void)
{
#ifdef QDIO_PERFORMANCE_STATS
        proc_perf_file_registration=0;
	qdio_perf_proc_file=create_proc_entry(QDIO_PERF,
					      S_IFREG|0444,&proc_root);
	if (qdio_perf_proc_file) {
		qdio_perf_proc_file->read_proc=&qdio_perf_procfile_read;
	} else proc_perf_file_registration=-1;

        if (proc_perf_file_registration)
                QDIO_PRINT_WARN("was not able to register perf. " \
				"proc-file (%i).\n",
				proc_perf_file_registration);
#endif /* QDIO_PERFORMANCE_STATS */
}
#ifdef MODULE
static void qdio_remove_procfs_entry(void)
{
#ifdef QDIO_PERFORMANCE_STATS
	perf_stats.tl_runs=0;

        if (!proc_perf_file_registration) /* means if it went ok earlier */
		remove_proc_entry(QDIO_PERF,&proc_root);
#endif /* QDIO_PERFORMANCE_STATS */
}
#endif /* MODULE */

static void tiqdio_register_thinints(void)
{
	char dbf_text[20];
	register_thinint_result=
		s390_register_adapter_interrupt(&tiqdio_thinint_handler);
	if (register_thinint_result) {
		sprintf(dbf_text,"regthn%x",(register_thinint_result&0xff));
		QDIO_DBF_TEXT0(0,setup,dbf_text);
		QDIO_PRINT_ERR("failed to register adapter handler " \
			       "(rc=%i).\nAdapter interrupts might " \
			       "not work. Continuing.\n",
			       register_thinint_result);
	}
}

#ifdef MODULE
static void tiqdio_unregister_thinints(void)
{
	if (!register_thinint_result)
		s390_unregister_adapter_interrupt(&tiqdio_thinint_handler);
}
#endif /* MODULE */

static int qdio_get_qdio_memory(void)
{
	int i;
	indicator_used[0]=1;

	for (i=1;i<INDICATORS_PER_CACHELINE;i++)
		indicator_used[i]=0;
	indicators=(__u32*)kmalloc(sizeof(__u32)*(INDICATORS_PER_CACHELINE),
				   GFP_KERNEL);
       	if (!indicators) return -ENOMEM;
	memset(indicators,0,sizeof(__u32)*(INDICATORS_PER_CACHELINE));

	chsc_area=(qdio_chsc_area_t *)
		kmalloc(sizeof(qdio_chsc_area_t),GFP_KERNEL);
	QDIO_DBF_TEXT3(0,trace,"chscarea"); \
	QDIO_DBF_HEX3(0,trace,&chsc_area,sizeof(void*)); \
	if (!chsc_area) {
		/* ahem... let's call it data area */
		QDIO_PRINT_ERR("not enough memory for data area. Cannot " \
			       "initialize QDIO.\n");
		kfree(indicators);
		return -ENOMEM;
	}
	memset(chsc_area,0,sizeof(qdio_chsc_area_t));
	return 0;
}

#ifdef MODULE
static void qdio_release_qdio_memory(void)
{
	kfree(chsc_area);
	if (indicators) kfree(indicators);
}
#endif /* MODULE */

int init_module(void); /* we want to use it in init_QDIO */

static void qdio_unregister_dbf_views(void)
{
	if (qdio_dbf_setup)
		debug_unregister(qdio_dbf_setup);
	if (qdio_dbf_sbal)
		debug_unregister(qdio_dbf_sbal);
	if (qdio_dbf_sense)
		debug_unregister(qdio_dbf_sense);
	if (qdio_dbf_trace)
		debug_unregister(qdio_dbf_trace);
#ifdef QDIO_DBF_LIKE_HELL
        if (qdio_dbf_slsb_out)
                debug_unregister(qdio_dbf_slsb_out);
        if (qdio_dbf_slsb_in)
                debug_unregister(qdio_dbf_slsb_in);
#endif /* QDIO_DBF_LIKE_HELL */
}

/* this is not __initfunc, as it is called from init_module */
int init_QDIO(void)
{
	char dbf_text[20];
	int res;
#if defined(MODULE)||defined(QDIO_PERFORMANCE_STATS)
	void *ptr;
#endif /* defined(MODULE)||defined(QDIO_PERFORMANCE_STATS) */

	qdio_eyecatcher();

	printk("qdio: loading %s\n",version);

	res=qdio_get_qdio_memory();
	if (res) return res;

	sema_init(&init_sema,1);

	qdio_dbf_setup=debug_register(QDIO_DBF_SETUP_NAME,
				      QDIO_DBF_SETUP_INDEX,
				      QDIO_DBF_SETUP_NR_AREAS,
				      QDIO_DBF_SETUP_LEN);
	if (!qdio_dbf_setup) goto oom;
	debug_register_view(qdio_dbf_setup,&debug_hex_ascii_view);
	debug_set_level(qdio_dbf_setup,QDIO_DBF_SETUP_LEVEL);

	qdio_dbf_sbal=debug_register(QDIO_DBF_SBAL_NAME,
				     QDIO_DBF_SBAL_INDEX,
				     QDIO_DBF_SBAL_NR_AREAS,
				     QDIO_DBF_SBAL_LEN);
	if (!qdio_dbf_sbal) goto oom;

	debug_register_view(qdio_dbf_sbal,&debug_hex_ascii_view);
	debug_set_level(qdio_dbf_sbal,QDIO_DBF_SBAL_LEVEL);

	qdio_dbf_sense=debug_register(QDIO_DBF_SENSE_NAME,
				      QDIO_DBF_SENSE_INDEX,
				      QDIO_DBF_SENSE_NR_AREAS,
				      QDIO_DBF_SENSE_LEN);
	if (!qdio_dbf_sense) goto oom;

	debug_register_view(qdio_dbf_sense,&debug_hex_ascii_view);
	debug_set_level(qdio_dbf_sense,QDIO_DBF_SENSE_LEVEL);

	qdio_dbf_trace=debug_register(QDIO_DBF_TRACE_NAME,
				      QDIO_DBF_TRACE_INDEX,
				      QDIO_DBF_TRACE_NR_AREAS,
				      QDIO_DBF_TRACE_LEN);
	if (!qdio_dbf_trace) goto oom;

	debug_register_view(qdio_dbf_trace,&debug_hex_ascii_view);
	debug_set_level(qdio_dbf_trace,QDIO_DBF_TRACE_LEVEL);

#ifdef QDIO_DBF_LIKE_HELL
        qdio_dbf_slsb_out=debug_register(QDIO_DBF_SLSB_OUT_NAME,
                                         QDIO_DBF_SLSB_OUT_INDEX,
                                         QDIO_DBF_SLSB_OUT_NR_AREAS,
                                         QDIO_DBF_SLSB_OUT_LEN);
        if (!qdio_dbf_slsb_out) goto oom;
        debug_register_view(qdio_dbf_slsb_out,&debug_hex_ascii_view);
        debug_set_level(qdio_dbf_slsb_out,QDIO_DBF_SLSB_OUT_LEVEL);

        qdio_dbf_slsb_in=debug_register(QDIO_DBF_SLSB_IN_NAME,
                                        QDIO_DBF_SLSB_IN_INDEX,
                                        QDIO_DBF_SLSB_IN_NR_AREAS,
                                        QDIO_DBF_SLSB_IN_LEN);
        if (!qdio_dbf_slsb_in) goto oom;
        debug_register_view(qdio_dbf_slsb_in,&debug_hex_ascii_view);
        debug_set_level(qdio_dbf_slsb_in,QDIO_DBF_SLSB_IN_LEVEL);
#endif /* QDIO_DBF_LIKE_HELL */

#ifdef QDIO_PERFORMANCE_STATS
       	memset((void*)&perf_stats,0,sizeof(perf_stats));
	QDIO_DBF_TEXT0(0,setup,"perfstat");
	ptr=&perf_stats;
	QDIO_DBF_HEX0(0,setup,&ptr,sizeof(void*));
#endif /* QDIO_PERFORMANCE_STATS */

#ifdef MODULE
	QDIO_DBF_TEXT0(0,setup,"initmodl");
	ptr=&init_module;
	QDIO_DBF_HEX0(0,setup,&ptr,sizeof(void*));
#endif /* MODULE */

	qdio_add_procfs_entry();

	hydra_thinints=qdio_check_for_hydra_thinints();

	sprintf(dbf_text,"hydrati%1x",hydra_thinints);
	QDIO_DBF_TEXT0(0,setup,dbf_text);

	tiqdio_register_thinints();

	return 0;
 oom:
	QDIO_PRINT_ERR("not enough memory for dbf.\n");
	qdio_unregister_dbf_views();
	return -ENOMEM;
}

#ifdef MODULE
int init_module(void)
{
	return init_QDIO();
}

void cleanup_module(void)
{
	tiqdio_unregister_thinints();

	qdio_remove_procfs_entry();

	qdio_release_qdio_memory();

	qdio_unregister_dbf_views();

  	printk("qdio: %s: module removed\n",version);
}
#else /* MODULE */
static int __init initcall_QDIO(void)
{
	        return init_QDIO();
}
__initcall(initcall_QDIO);
#endif /* MODULE */

EXPORT_SYMBOL(qdio_initialize);
EXPORT_SYMBOL(qdio_activate);
EXPORT_SYMBOL(do_QDIO);
EXPORT_SYMBOL(qdio_cleanup);
EXPORT_SYMBOL(qdio_eyecatcher);
EXPORT_SYMBOL(qdio_synchronize);

