/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NETLOGIC_BSD */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/limits.h>
#include <sys/bus.h>

#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/unistd.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>

#include <machine/reg.h>
#include <machine/cpu.h>
#include <machine/hwfunc.h>
#include <machine/mips_opcode.h>
#include <machine/param.h>
#include <machine/intr_machdep.h>

#include <mips/nlm/hal/mips-extns.h>
#include <mips/nlm/hal/haldefs.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/hal/cop2.h>
#include <mips/nlm/hal/fmn.h>
#include <mips/nlm/hal/pic.h>

#include <mips/nlm/msgring.h>
#include <mips/nlm/interrupt.h>
#include <mips/nlm/xlp.h>
#include <mips/nlm/board.h>

#define MSGRNG_NSTATIONS 1024
/*
 * Keep track of our message ring handler threads, each core has a
 * different message station. Ideally we will need to start a few
 * message handling threads every core, and wake them up depending on
 * load
 */
struct msgring_thread {
	struct thread	*thread; /* msgring handler threads */
	int	needed;		/* thread needs to wake up */
};
static struct msgring_thread msgring_threads[XLP_MAX_CORES * XLP_MAX_THREADS];
static struct proc *msgring_proc;	/* all threads are under a proc */

/*
 * The device drivers can register a handler for the the messages sent
 * from a station (corresponding to the device).
 */
struct tx_stn_handler {
	msgring_handler action;
	void *arg;
};
static struct tx_stn_handler msgmap[MSGRNG_NSTATIONS];
static struct mtx	msgmap_lock;
uint64_t xlp_cms_base;
uint32_t xlp_msg_thread_mask;
static int xlp_msg_threads_per_core = 3; /* Make tunable */

static void create_msgring_thread(int hwtid);
static int msgring_process_fast_intr(void *arg);
/*
 * Boot time init, called only once
 */
void
xlp_msgring_config(void)
{
	unsigned int thrmask, mask;
	int i;

	/* TODO: Add other nodes */
	xlp_cms_base = nlm_get_cms_regbase(0);

	mtx_init(&msgmap_lock, "msgring", NULL, MTX_SPIN);
	if (xlp_threads_per_core < xlp_msg_threads_per_core)
		xlp_msg_threads_per_core = xlp_threads_per_core;
	thrmask = ((1 << xlp_msg_threads_per_core) - 1);
	/*thrmask <<= xlp_threads_per_core - xlp_msg_threads_per_core;*/
	mask = 0;
	for (i = 0; i < XLP_MAX_CORES; i++) {
		mask <<= XLP_MAX_THREADS;
		mask |= thrmask;
	}
	xlp_msg_thread_mask = xlp_hw_thread_mask & mask;
	printf("Initializing CMS...@%jx, Message handler thread mask %#jx\n",
	    (uintmax_t)xlp_cms_base, (uintmax_t)xlp_msg_thread_mask);
}

/*
 * Initialize the messaging subsystem.
 *
 * Message Stations are shared among all threads in a cpu core, this
 * has to be called once from every core which is online.
 */
void
xlp_msgring_iodi_config(void)
{
	void *cookie;

	xlp_msgring_config();
/*	nlm_cms_default_setup(0,0,0,0); */
	nlm_cms_credit_setup(50);
	create_msgring_thread(0);
	cpu_establish_hardintr("msgring", msgring_process_fast_intr, NULL,
	    NULL, IRQ_MSGRING, INTR_TYPE_NET, &cookie);
}

void
nlm_cms_credit_setup(int credit)
{
	int src, qid, i;

#if 0
	/* there are a total of 18 src stations on XLP. */
	printf("Setting up CMS credits!\n");
	for (src=0; src<18; src++) {
		for(qid=0; qid<1024; qid++) {
			nlm_cms_setup_credits(xlp_cms_base, qid, src, credit);
		}
	}
#endif
	printf("Setting up CMS credits!\n");
	/* CPU Credits */
	for (i = 1; i < 8; i++) {
		src = (i << 4);
		for (qid = 0; qid < 1024; qid++)
			nlm_cms_setup_credits(xlp_cms_base, qid, src, credit);
	}
	/* PCIE Credits */
	for(i = 0; i < 4; i++) {
		src = (256 + (i * 2));
		for(qid = 0; qid < 1024; qid++)
			nlm_cms_setup_credits(xlp_cms_base, qid, src, credit);
	}
	/* DTE Credits */
	src = 264;
	for (qid = 0; qid < 1024; qid++)
		nlm_cms_setup_credits(xlp_cms_base, qid, src, credit);
	/* RSA Credits */
	src = 272;
	for (qid = 0; qid < 1024; qid++)
		nlm_cms_setup_credits(xlp_cms_base, qid, src, credit);

	/* Crypto Credits */
	src = 281;
	for (qid = 0; qid < 1024; qid++)
		nlm_cms_setup_credits(xlp_cms_base, qid, src, credit);

	/* CMP Credits */
	src = 298;
	for (qid = 0; qid < 1024; qid++)
		nlm_cms_setup_credits(xlp_cms_base, qid, src, credit);

	/* POE Credits */
	src = 384;
	for(qid = 0; qid < 1024; qid++)
		nlm_cms_setup_credits(xlp_cms_base, qid, src, credit);

	/* NAE Credits */
	src = 476;
	for(qid = 0; qid < 1024; qid++)
		nlm_cms_setup_credits(xlp_cms_base, qid, src, credit);
}

void
xlp_msgring_cpu_init(uint32_t cpuid)
{
	int queue,i;

	queue = CMS_CPU_PUSHQ(0, ((cpuid >> 2) & 0x7), (cpuid & 0x3), 0);
	/* temp allocate 4 segments to each output queue */
	nlm_cms_alloc_onchip_q(xlp_cms_base, queue, 4);
	/* Enable high watermark and non empty interrupt */
	nlm_cms_per_queue_level_intr(xlp_cms_base, queue,2,0);
	for(i=0;i<8;i++) {
		/* temp distribute the credits to all CPU stations */
		nlm_cms_setup_credits(xlp_cms_base, queue, i * 16, 8);
	}
}

void
xlp_cpu_msgring_handler(int bucket, int size, int code, int stid,
		    struct nlm_fmn_msg *msg, void *data)
{
	int i;

	printf("vc:%d srcid:%d size:%d\n",bucket,stid,size);
	for(i=0;i<size;i++) {
		printf("msg->msg[%d]:0x%jx ", i, (uintmax_t)msg->msg[i]);
	}
	printf("\n");
}

/*
 * Drain out max_messages for the buckets set in the bucket mask.
 * Use max_msgs = 0 to drain out all messages.
 */
int
xlp_handle_msg_vc(int vc, int max_msgs)
{
	struct nlm_fmn_msg msg;
	int i, srcid = 0, size = 0, code = 0;
	struct tx_stn_handler *he;
	uint32_t mflags, status;

	for (i = 0; i < max_msgs; i++) {
		mflags = nlm_save_flags_cop2();
		status = nlm_fmn_msgrcv(vc, &srcid, &size, &code, &msg);
		nlm_restore_flags(mflags);
		if (status != 0) /* If there is no msg or error */
			break;
		if (srcid < 0 && srcid >= 1024) {
			printf("[%s]: bad src id %d\n", __func__, srcid);
			continue;
		}
		he = &msgmap[srcid];
		if(he->action != NULL)
			(he->action)(vc, size, code, srcid, &msg, he->arg);
#if 0 /* debug */
		else
			printf("[%s]: No Handler for message from stn_id=%d,"
			    " vc=%d, size=%d, msg0=%jx, dropping message\n",
			    __func__, srcid, vc, size, (uintmax_t)msg.msg[0]);
#endif
	}

	return (i);
}

static int
msgring_process_fast_intr(void *arg)
{
	struct msgring_thread *mthd;
	struct thread *td;
	int	cpu;

	cpu = nlm_cpuid();
	mthd = &msgring_threads[cpu];
	td = mthd->thread;

	/* clear pending interrupts */
	nlm_write_c0_eirr(1ULL << IRQ_MSGRING);

	/* wake up the target thread */
	mthd->needed = 1;
	thread_lock(td);
	if (TD_AWAITING_INTR(td)) {
		TD_CLR_IWAIT(td);
		sched_add(td, SRQ_INTR);
	}

	thread_unlock(td);
	return (FILTER_HANDLED);
}

u_int fmn_msgcount[32][4];
u_int fmn_loops[32];

static void
msgring_process(void * arg)
{
	volatile struct msgring_thread *mthd;
	struct thread *td;
	uint32_t mflags;
	int hwtid, vc, handled, nmsgs;

	hwtid = (intptr_t)arg;
	mthd = &msgring_threads[hwtid];
	td = mthd->thread;
	KASSERT(curthread == td,
	    ("%s:msg_ithread and proc linkage out of sync", __func__));

	/* First bind this thread to the right CPU */
	thread_lock(td);
	sched_bind(td, xlp_hwtid_to_cpuid[hwtid]);
	thread_unlock(td);

	if (hwtid != nlm_cpuid())
		printf("Misscheduled hwtid %d != cpuid %d\n", hwtid, nlm_cpuid());
	mflags = nlm_save_flags_cop2();
	nlm_fmn_cpu_init(IRQ_MSGRING, 0, 0, 0, 0, 0);
	nlm_restore_flags(mflags);

	/* start processing messages */
	for( ; ; ) {
		/*atomic_store_rel_int(&mthd->needed, 0);*/

	        /* enable cop2 access */
		do {
			handled = 0;
			for (vc = 0; vc < 4; vc++) {
				nmsgs = xlp_handle_msg_vc(vc, 1);
				fmn_msgcount[hwtid][vc] += nmsgs;
				handled += nmsgs;
			}
		} while (handled);

		/* sleep */
#if 0
		thread_lock(td);
		if (mthd->needed) {
			thread_unlock(td);
			continue;
		}
		sched_class(td, PRI_ITHD);
		TD_SET_IWAIT(td);
		mi_switch(SW_VOL, NULL);
		thread_unlock(td);
#else
		pause("wmsg", 1);
#endif
		fmn_loops[hwtid]++;
	}
}

static void
create_msgring_thread(int hwtid)
{
	struct msgring_thread *mthd;
	struct thread *td;
	int	error;

	mthd = &msgring_threads[hwtid];
	error = kproc_kthread_add(msgring_process, (void *)(uintptr_t)hwtid,
	    &msgring_proc, &td, RFSTOPPED, 2, "msgrngproc",
	    "msgthr%d", hwtid);
	if (error)
		panic("kproc_kthread_add() failed with %d", error);
	mthd->thread = td;

	thread_lock(td);
	sched_class(td, PRI_ITHD);
	sched_add(td, SRQ_INTR);
	thread_unlock(td);
	CTR2(KTR_INTR, "%s: created %s", __func__, td->td_name);
}

int
register_msgring_handler(int startb, int endb, msgring_handler action,
    void *arg)
{
	int	i;

	printf("Register handler %d-%d %p(%p)\n", startb, endb, action, arg);
	KASSERT(startb >= 0 && startb <= endb && endb < MSGRNG_NSTATIONS,
	    ("Invalid value for for bucket range %d,%d", startb, endb));

	mtx_lock_spin(&msgmap_lock);
	for (i = startb; i <= endb; i++) {
		KASSERT(msgmap[i].action == NULL,
		   ("Bucket %d already used [action %p]", i, msgmap[i].action));
		msgmap[i].action = action;
		msgmap[i].arg = arg;
	}
	mtx_unlock_spin(&msgmap_lock);
	return (0);
}

/*
 * Start message ring processing threads on other CPUs, after SMP start
 */
static void
start_msgring_threads(void *arg)
{
	int	hwt;

	for (hwt = 1; hwt < XLP_MAX_CORES * XLP_MAX_THREADS; hwt++) {
		if ((xlp_msg_thread_mask & (1 << hwt)) == 0)
			continue;
		create_msgring_thread(hwt);
	}
}

SYSINIT(start_msgring_threads, SI_SUB_SMP, SI_ORDER_MIDDLE,
    start_msgring_threads, NULL);

/*
 * DEBUG support, XXX: static buffer, not locked
 */
static int
sys_print_debug(SYSCTL_HANDLER_ARGS)
{
	int error, nb, i, fs;
	static char xprintb[4096], *buf;

	buf = xprintb;
	fs = sizeof(xprintb);
	nb = snprintf(buf, fs,
	    "\nID     vc0       vc1       vc2     vc3     loops\n");
	buf += nb;
	fs -= nb;
	for (i = 0; i < 32; i++) {
		if ((xlp_hw_thread_mask & (1 << i)) == 0)
			continue;
		nb = snprintf(buf, fs,
		    "%2d: %8d %8d %8d %8d %8d\n", i,
		    fmn_msgcount[i][0], fmn_msgcount[i][1],
		    fmn_msgcount[i][2], fmn_msgcount[i][3],
		    fmn_loops[i]);
		buf += nb;
		fs -= nb;
	}
	error = SYSCTL_OUT(req, xprintb, buf - xprintb);
	return (error);
}

SYSCTL_PROC(_debug, OID_AUTO, msgring, CTLTYPE_STRING | CTLFLAG_RD, 0, 0,
    sys_print_debug, "A", "msgring debug info");
