/*-
 * Copyright (c) 2000 Doug Rabson
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
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <sys/dkstat.h>

#include <machine/atomic.h>
#include <machine/globaldata.h>
#include <machine/pmap.h>
#include <machine/rpb.h>
#include <machine/clock.h>

volatile u_int		stopped_cpus;
volatile u_int		started_cpus;
volatile u_int		checkstate_probed_cpus;
volatile u_int		checkstate_need_ast;
volatile u_int		checkstate_pending_ast;
struct proc*		checkstate_curproc[MAXCPU];
int			checkstate_cpustate[MAXCPU];
u_long			checkstate_pc[MAXCPU];
volatile u_int		resched_cpus;
void (*cpustop_restartfunc) __P((void));
int			mp_ncpus;

int			smp_started;
int			boot_cpu_id;
u_int32_t		all_cpus;

static struct globaldata	*cpuno_to_globaldata[MAXCPU];

int smp_active = 0;	/* are the APs allowed to run? */
SYSCTL_INT(_machdep, OID_AUTO, smp_active, CTLFLAG_RW, &smp_active, 0, "");

/* Is forwarding of a interrupt to the CPU holding the ISR lock enabled ? */
int forward_irq_enabled = 1;
SYSCTL_INT(_machdep, OID_AUTO, forward_irq_enabled, CTLFLAG_RW,
	   &forward_irq_enabled, 0, "");

/* Enable forwarding of a signal to a process running on a different CPU */
static int forward_signal_enabled = 1;
SYSCTL_INT(_machdep, OID_AUTO, forward_signal_enabled, CTLFLAG_RW,
	   &forward_signal_enabled, 0, "");

/* Enable forwarding of roundrobin to all other cpus */
static int forward_roundrobin_enabled = 1;
SYSCTL_INT(_machdep, OID_AUTO, forward_roundrobin_enabled, CTLFLAG_RW,
	   &forward_roundrobin_enabled, 0, "");

/*
 * Communicate with a console running on a secondary processor.
 * Return 1 on failure.
 */
static int
smp_send_secondary_command(const char *command, int cpuno)
{
	u_int64_t mask;

	mask = 1L << cpuno;
	struct pcs *cpu = LOCATE_PCS(hwrpb, cpuno);
	int i, len;

	/*
	 * Sanity check.
	 */
	len = strlen(command);
	if (len > sizeof(cpu->pcs_buffer.rxbuf)) {
		printf("smp_send_secondary_command: command '%s' too long\n",
		    command);
		return 0;
	}

	/*
	 * Wait for the rx bit to clear.
	 */
	for (i = 0; i < 100000; i++) {
		if (!(hwrpb->rpb_rxrdy & mask))
			break;
		DELAY(10);
	}
	if (hwrpb->rpb_rxrdy & mask)
		return 0;

	/*
	 * Write the command into the processor's buffer.
	 */
	bcopy(command, cpu->pcs_buffer.rxbuf, len);
	cpu->pcs_buffer.rxlen = len;

	/*
	 * Set the bit in the rxrdy mask and let the secondary try to
	 * handle the command.
	 */
	atomic_set_64(&hwrpb->rpb_rxrdy, mask);

	/*
	 * Wait for the rx bit to clear.
	 */
	for (i = 0; i < 100000; i++) {
		if (!(hwrpb->rpb_rxrdy & mask))
			break;
		DELAY(10);
	}
	if (hwrpb->rpb_rxrdy & mask)
		return 0;

	return 1;
}

void
smp_init_secondary(void)
{

	mtx_lock(&Giant);

	printf("smp_init_secondary: called\n");
	CTR0(KTR_SMP, "smp_init_secondary");

	/*
	 * Add to mask.
	 */
	smp_started = 1;
	if (PCPU_GET(cpuno) + 1 > mp_ncpus)
		mp_ncpus = PCPU_GET(cpuno) + 1;
	spl0();

	mtx_unlock(&Giant);
}

extern void smp_init_secondary_glue(void);

static int
smp_start_secondary(int cpuno)
{

	printf("smp_start_secondary: starting cpu %d\n", cpuno);

	sz = round_page(UPAGES * PAGE_SIZE);
	globaldata = malloc(sz, M_TEMP, M_NOWAIT);
	if (!globaldata) {
		printf("smp_start_secondary: can't allocate memory\n");
		return 0;
	}
	
	globaldata_init(globaldata, cpuno, sz);

	/*
	 * Fire it up and hope for the best.
	 */
	if (!smp_send_secondary_command("START\r\n", cpuno)) {
		printf("smp_init_secondary: can't send START command\n");
		free(globaldata, M_TEMP);
		return 0;
	}
	       
	/*
	 * It worked (I think).
	 */
	/* if (bootverbose) */
		printf("smp_init_secondary: cpu %d started\n", cpuno);

	return 1;
}

/*
 * Initialise a struct globaldata.
 */
void
globaldata_init(struct globaldata *globaldata, int cpuno, size_t sz)
{

	bzero(globaldata, sz);
	globaldata->gd_idlepcbphys = vtophys((vm_offset_t) &globaldata->gd_idlepcb);
	globaldata->gd_idlepcb.apcb_ksp = (u_int64_t)
		((caddr_t) globaldata + sz - sizeof(struct trapframe));
	globaldata->gd_idlepcb.apcb_ptbr = proc0.p_addr->u_pcb.pcb_hw.apcb_ptbr;
	globaldata->gd_cpuno = cpuno;
	globaldata->gd_other_cpus = all_cpus & ~(1 << cpuno);
	globaldata->gd_next_asn = 0;
	globaldata->gd_current_asngen = 1;
	globaldata->gd_cpuid = cpuno;
	cpuno_to_globaldata[cpuno] = globaldata;
}

struct globaldata *
globaldata_find(int cpuno)
{

	return cpuno_to_globaldata[cpuno];
}

/* Other stuff */

/* lock around the MP rendezvous */
static struct mtx smp_rv_mtx;

static void
init_locks(void)
{

	mtx_init(&smp_rv_mtx, "smp rendezvous", MTX_SPIN);
}

void
mp_start()
{
}

void
mp_announce()
{
}

void
smp_invltlb()
{
}


/*
 * When called the executing CPU will send an IPI to all other CPUs
 *  requesting that they halt execution.
 *
 * Usually (but not necessarily) called with 'other_cpus' as its arg.
 *
 *  - Signals all CPUs in map to stop.
 *  - Waits for each to stop.
 *
 * Returns:
 *  -1: error
 *   0: NA
 *   1: ok
 *
 * XXX FIXME: this is not MP-safe, needs a lock to prevent multiple CPUs
 *            from executing at same time.
 */
int
stop_cpus(u_int map)
{
	int i;

	if (!smp_started)
		return 0;

	CTR1(KTR_SMP, "stop_cpus(%x)", map);

	i = 0;
	while ((stopped_cpus & map) != map) {
		/* spin */
		i++;
		if (i == 100000) {
			printf("timeout stopping cpus\n");
			break;
		}
	}

	printf("stopped_cpus=%x\n", stopped_cpus);

	return 1;
}


/*
 * Called by a CPU to restart stopped CPUs. 
 *
 * Usually (but not necessarily) called with 'stopped_cpus' as its arg.
 *
 *  - Signals all CPUs in map to restart.
 *  - Waits for each to restart.
 *
 * Returns:
 *  -1: error
 *   0: NA
 *   1: ok
 */
int
restart_cpus(u_int map)
{

	if (!smp_started)
		return 0;

	CTR1(KTR_SMP, "restart_cpus(%x)", map);

	started_cpus = map;		/* signal other cpus to restart */

	while ((stopped_cpus & map) != 0) /* wait for each to clear its bit */
		;

	return 1;
}

/*
 * All-CPU rendezvous.  CPUs are signalled, all execute the setup function 
 * (if specified), rendezvous, execute the action function (if specified),
 * rendezvous again, execute the teardown function (if specified), and then
 * resume.
 *
 * Note that the supplied external functions _must_ be reentrant and aware
 * that they are running in parallel and in an unknown lock context.
 */
static void (*smp_rv_setup_func)(void *arg);
static void (*smp_rv_action_func)(void *arg);
static void (*smp_rv_teardown_func)(void *arg);
static void *smp_rv_func_arg;
static volatile int smp_rv_waiters[2];

void
smp_rendezvous_action(void)
{

	/* setup function */
	if (smp_rv_setup_func != NULL)
		smp_rv_setup_func(smp_rv_func_arg);
	/* spin on entry rendezvous */
	atomic_add_int(&smp_rv_waiters[0], 1);
	while (smp_rv_waiters[0] < mp_ncpus)
		;
	/* action function */
	if (smp_rv_action_func != NULL)
		smp_rv_action_func(smp_rv_func_arg);
	/* spin on exit rendezvous */
	atomic_add_int(&smp_rv_waiters[1], 1);
	while (smp_rv_waiters[1] < mp_ncpus)
		;
	/* teardown function */
	if (smp_rv_teardown_func != NULL)
		smp_rv_teardown_func(smp_rv_func_arg);
}

void
smp_rendezvous(void (* setup_func)(void *), 
	void (* action_func)(void *),
	void (* teardown_func)(void *),
	void *arg)
{

	/* obtain rendezvous lock */
	mtx_lock_spin(&smp_rv_mtx);

	/* set static function pointers */
	smp_rv_setup_func = setup_func;
	smp_rv_action_func = action_func;
	smp_rv_teardown_func = teardown_func;
	smp_rv_func_arg = arg;
	smp_rv_waiters[0] = 0;
	smp_rv_waiters[1] = 0;

	/* call executor function */
	smp_rendezvous_action();

	/* release lock */
	mtx_unlock_spin(&smp_rv_mtx);
}

static u_int64_t
atomic_readandclear(u_int64_t* p)
{
	u_int64_t v, temp;

	__asm__ __volatile__ (
	: "=&r"(v), "=&r"(temp), "=m" (*p)
	: "m"(*p)
	: "memory");
	return v;
}
