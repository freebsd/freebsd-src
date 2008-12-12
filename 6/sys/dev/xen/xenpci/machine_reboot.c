#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/interrupt.h>
#include <sys/pcpu.h>

#include <machine/xen/xen-os.h>
#include <machine/xen/xenvar.h>
#include <xen/hypervisor.h>
#include <xen/xen_intr.h>
#include <xen/evtchn.h>
#include <sys/smp.h>

#include <dev/xen/xenpci/xenpcivar.h>

struct ap_suspend_info {
	int      do_spin;
	atomic_t nr_spinning;
};

#ifdef CONFIG_SMP

/*
 * Spinning prevents, for example, APs touching grant table entries while
 * the shared grant table is not mapped into the address space imemdiately
 * after resume.
 */
static void ap_suspend(void *_info)
{
	struct ap_suspend_info *info = _info;

	BUG_ON(!irqs_disabled());

	atomic_inc(&info->nr_spinning);
	mb();

	while (info->do_spin)
		cpu_relax();

	mb();
	atomic_dec(&info->nr_spinning);
}

#define initiate_ap_suspend(i)	smp_call_function(ap_suspend, i, 0, 0)

#else /* !defined(CONFIG_SMP) */

#define initiate_ap_suspend(i)	0

#endif

static int bp_suspend(void)
{
	int suspend_cancelled;

	suspend_cancelled = HYPERVISOR_suspend(0);
	if (!suspend_cancelled)
		xenpci_resume();

	return suspend_cancelled;
}

void
xen_suspend()
{
	int suspend_cancelled;
	//struct ap_suspend_info info;

	if (DEVICE_SUSPEND(root_bus)) {
		printf("xen_suspend: device_suspend failed\n");
		return;
	}

	critical_enter();

	/* Prevent any races with evtchn_interrupt() handler. */
	irq_suspend();

#if 0
	info.do_spin = 1;
	atomic_set(&info.nr_spinning, 0);
	smp_mb();

	nr_cpus = num_online_cpus() - 1;

	err = initiate_ap_suspend(&info);
	if (err < 0) {
		critical_exit();
		//xenbus_suspend_cancel();
		return err;
	}

	while (atomic_read(&info.nr_spinning) != nr_cpus)
		cpu_relax();
#endif

	disable_intr();
	suspend_cancelled = bp_suspend();
	//resume_notifier(suspend_cancelled);
	enable_intr();

#if 0
	smp_mb();
	info.do_spin = 0;
	while (atomic_read(&info.nr_spinning) != 0)
		cpu_relax();
#endif

	critical_exit();

	if (!suspend_cancelled)
		DEVICE_RESUME(root_bus);
#if 0
	else
		xenbus_suspend_cancel();
#endif
}
