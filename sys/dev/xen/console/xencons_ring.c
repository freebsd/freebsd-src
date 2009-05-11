#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/consio.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/cons.h>

#include <machine/stdarg.h>
#include <machine/xen/xen-os.h>
#include <xen/hypervisor.h>
#include <xen/xen_intr.h>
#include <sys/cons.h>

#include <xen/xen_intr.h>
#include <xen/evtchn.h>
#include <xen/interface/io/console.h>

#include <dev/xen/console/xencons_ring.h>
#include <xen/evtchn.h>
#include <xen/interface/io/console.h>

#define console_evtchn	console.domU.evtchn
static unsigned int console_irq;
extern char *console_page;
extern struct mtx              cn_mtx;

static inline struct xencons_interface *
xencons_interface(void)
{
	return (struct xencons_interface *)console_page;
}


int
xencons_has_input(void)
{
	struct xencons_interface *intf; 

	intf = xencons_interface();		

	return (intf->in_cons != intf->in_prod);
}


int 
xencons_ring_send(const char *data, unsigned len)
{
	struct xencons_interface *intf; 
	XENCONS_RING_IDX cons, prod;
	int sent;

	intf = xencons_interface();
	cons = intf->out_cons;
	prod = intf->out_prod;
	sent = 0;

	mb();
	KASSERT((prod - cons) <= sizeof(intf->out),
		("console send ring inconsistent"));
	
	while ((sent < len) && ((prod - cons) < sizeof(intf->out)))
		intf->out[MASK_XENCONS_IDX(prod++, intf->out)] = data[sent++];

	wmb();
	intf->out_prod = prod;

	notify_remote_via_evtchn(xen_start_info->console_evtchn);

	return sent;

}	


static xencons_receiver_func *xencons_receiver;

void 
xencons_handle_input(void *unused)
{
	struct xencons_interface *intf;
	XENCONS_RING_IDX cons, prod;

	CN_LOCK(cn_mtx);
	intf = xencons_interface();

	cons = intf->in_cons;
	prod = intf->in_prod;
	CN_UNLOCK(cn_mtx);
	
	/* XXX needs locking */
	while (cons != prod) {
		xencons_rx(intf->in + MASK_XENCONS_IDX(cons, intf->in), 1);
		cons++;
	}

	mb();
	intf->in_cons = cons;

	CN_LOCK(cn_mtx);
	notify_remote_via_evtchn(xen_start_info->console_evtchn);

	xencons_tx();
	CN_UNLOCK(cn_mtx);
}

void 
xencons_ring_register_receiver(xencons_receiver_func *f)
{
	xencons_receiver = f;
}

int
xencons_ring_init(void)
{
	int err;

	if (!xen_start_info->console_evtchn)
		return 0;

	err = bind_caller_port_to_irqhandler(xen_start_info->console_evtchn,
		"xencons", xencons_handle_input, NULL,
		INTR_TYPE_MISC | INTR_MPSAFE, &console_irq);
	if (err) {
		return err;
	}

	return 0;
}

extern void xencons_suspend(void);
extern void xencons_resume(void);

void 
xencons_suspend(void)
{

	if (!xen_start_info->console_evtchn)
		return;

	unbind_from_irqhandler(console_irq);
}

void 
xencons_resume(void)
{

	(void)xencons_ring_init();
}

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 8
 * tab-width: 4
 * indent-tabs-mode: t
 * End:
 */
