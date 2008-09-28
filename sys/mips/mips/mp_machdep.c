#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/cons.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/atomic.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/smp.h>

static struct mtx ap_boot_mtx;
extern struct pcpu __pcpu[];
extern int num_tlbentries;
void mips_start_timer(void);
static volatile int aps_ready = 0;

u_int32_t boot_cpu_id;


void
cpu_mp_announce(void)
{
}

/*
 * To implement IPIs on MIPS CPU, we use the Interrupt Line 2 ( bit 4 of cause
 * register) and a bitmap to avoid redundant IPI interrupts. To interrupt a
 * set of CPUs, the sender routine runs in a ' loop ' sending interrupts to
 * all the specified CPUs. A single Mutex (smp_ipi_mtx) is used for all IPIs
 * that spinwait for delivery. This includes the following IPIs
 * IPI_RENDEZVOUS
 * IPI_INVLPG
 * IPI_INVLTLB
 * IPI_INVLRNG
 */

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(u_int32_t cpus, u_int ipi)
{
	struct pcpu *pcpu;
	u_int cpuid, new_pending, old_pending;

	CTR3(KTR_SMP, "%s: cpus: %x, ipi: %x\n", __func__, cpus, ipi);

	while ((cpuid = ffs(cpus)) != 0) {
		cpuid--;
		cpus &= ~(1 << cpuid);
		pcpu = pcpu_find(cpuid);

		if (pcpu) {
			do {
				old_pending = pcpu->pc_pending_ipis;
				new_pending = old_pending | ipi;
			} while (!atomic_cmpset_int(&pcpu->pc_pending_ipis,
			    old_pending, new_pending));	

			if (old_pending)
				continue;

			mips_ipi_send (cpuid);
		}
	}
}

/*
 * send an IPI to all CPUs EXCEPT myself
 */
void
ipi_all_but_self(u_int ipi)
{

	ipi_selected(PCPU_GET(other_cpus), ipi);
}

/*
 * Handle an IPI sent to this processor.
 */
intrmask_t
smp_handle_ipi(struct trapframe *frame)
{
	cpumask_t cpumask;		/* This cpu mask */
	u_int	ipi, ipi_bitmap;

	ipi_bitmap = atomic_readandclear_int(PCPU_PTR(pending_ipis));
	cpumask = PCPU_GET(cpumask);

	CTR1(KTR_SMP, "smp_handle_ipi(), ipi_bitmap=%x", ipi_bitmap);
	while (ipi_bitmap) {
		/*
		 * Find the lowest set bit.
		 */
		ipi = ipi_bitmap & ~(ipi_bitmap - 1);
		ipi_bitmap &= ~ipi;
		switch (ipi) {
		case IPI_INVLTLB:
			CTR0(KTR_SMP, "IPI_INVLTLB");
			break;

		case IPI_RENDEZVOUS:
			CTR0(KTR_SMP, "IPI_RENDEZVOUS");
			smp_rendezvous_action();
			break;

		case IPI_AST:
			CTR0(KTR_SMP, "IPI_AST");
			break;

		case IPI_STOP:
			CTR0(KTR_SMP, "IPI_STOP");
			atomic_set_int(&stopped_cpus, cpumask);

			while ((started_cpus & cpumask) == 0)
			    ;
			atomic_clear_int(&started_cpus, cpumask);
			atomic_clear_int(&stopped_cpus, cpumask);
			break;
		}
	}
	return CR_INT_IPI;
}

void
cpu_mp_setmaxid(void)
{

	mp_maxid = MAXCPU - 1;
}

void
smp_init_secondary(u_int32_t cpuid)
{

	if (cpuid >=  MAXCPU)
		panic ("cpu id exceeds MAXCPU\n");

	/* tlb init */
	R4K_SetWIRED(0);
	R4K_TLBFlush(num_tlbentries);
	R4K_SetWIRED(VMWIRED_ENTRIES);
	MachSetPID(0);

	Mips_SyncCache();

	mips_cp0_status_write(0);
	while (!aps_ready)
		;

	mips_sync(); mips_sync();
	/* Initialize curthread. */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	PCPU_SET(curthread, PCPU_GET(idlethread));

	mtx_lock_spin(&ap_boot_mtx);

	smp_cpus++;

	CTR1(KTR_SMP, "SMP: AP CPU #%d Launched", PCPU_GET(cpuid));

	/* Build our map of 'other' CPUs. */
	PCPU_SET(other_cpus, all_cpus & ~PCPU_GET(cpumask));

	printf("SMP: AP CPU #%d Launched!\n", PCPU_GET(cpuid));

	if (smp_cpus == mp_ncpus) {
		smp_started = 1;
		smp_active = 1;
	}

	mtx_unlock_spin(&ap_boot_mtx);

	while (smp_started == 0)
		; /* nothing */
	/* Enable Interrupt */
	mips_cp0_status_write(SR_INT_ENAB);
	/* ok, now grab sched_lock and enter the scheduler */
	mtx_lock_spin(&sched_lock);

	/*
	 * Correct spinlock nesting.  The idle thread context that we are
	 * borrowing was created so that it would start out with a single
	 * spin lock (sched_lock) held in fork_trampoline().  Since we've
	 * explicitly acquired locks in this function, the nesting count
	 * is now 2 rather than 1.  Since we are nested, calling
	 * spinlock_exit() will simply adjust the counts without allowing
	 * spin lock using code to interrupt us.
	 */
	spinlock_exit();
	KASSERT(curthread->td_md.md_spinlock_count == 1, ("invalid count"));

	binuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	/* kick off the clock on this cpu */
	mips_start_timer();
	cpu_throw(NULL, choosethread());	/* doesn't return */

	panic("scheduler returned us to %s", __func__);
}

static int
smp_start_secondary(int cpuid)
{
	struct pcpu *pcpu;
	int i;

	if (bootverbose)
		printf("smp_start_secondary: starting cpu %d\n", cpuid);

	pcpu_init(&__pcpu[cpuid], cpuid, sizeof(struct pcpu));

	if (bootverbose)
		printf("smp_start_secondary: cpu %d started\n", cpuid);

	return 1;
}

int
cpu_mp_probe(void)
{
	int i, cpus;

	/* XXX: Need to check for valid platforms here. */

	boot_cpu_id = PCPU_GET(cpuid);
	KASSERT(boot_cpu_id == 0, ("cpu_mp_probe() called on non-primary CPU"));
	all_cpus = PCPU_GET(cpumask);
	mp_ncpus = 1;

	/* Make sure we have at least one secondary CPU. */
	cpus = 0;
	for (i = 0; i < MAXCPU; i++) {
		cpus++;
	}
	return (cpus);
}

void
cpu_mp_start(void)
{
	int i, cpuid;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	cpuid = 1;
	for (i = 0; i < MAXCPU; i++) {

		if (i == boot_cpu_id)
			continue;
		if (smp_start_secondary(i)) {
			all_cpus |= (1 << cpuid);
			mp_ncpus++;
		cpuid++;
		}
	}
	idle_mask |= CR_INT_IPI;
	PCPU_SET(other_cpus, all_cpus & ~PCPU_GET(cpumask));
}

static void
release_aps(void *dummy __unused)
{
	if (bootverbose && mp_ncpus > 1)
		printf("%s: releasing secondary CPUs\n", __func__);
	atomic_store_rel_int(&aps_ready, 1);

	while (mp_ncpus > 1 && smp_started == 0)
		; /* nothing */
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, release_aps, NULL);
