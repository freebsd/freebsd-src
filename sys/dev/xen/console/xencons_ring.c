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
#include <machine/stdarg.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/hypervisor.h>
#include <machine/xen/xen_intr.h>
#include <sys/cons.h>


#include <dev/xen/console/xencons_ring.h>
#include <machine/xen/evtchn.h>
#include <xen/interface/io/console.h>


#define console_evtchn	console.domU.evtchn
extern char *console_page;
 
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
	PANIC_IF((prod - cons) > sizeof(intf->out));
	
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

	intf = xencons_interface();

	cons = intf->in_cons;
	prod = intf->in_prod;

	/* XXX needs locking */
	while (cons != prod) {
		xencons_rx(intf->in + MASK_XENCONS_IDX(cons, intf->in), 1);
		cons++;
	}

	mb();
	intf->in_cons = cons;

	notify_remote_via_evtchn(xen_start_info->console_evtchn);

	xencons_tx();
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
					INTR_TYPE_MISC | INTR_MPSAFE, NULL);
	if (err) {
		XENPRINTF("XEN console request irq failed %i\n", err);
		return err;
	}

	return 0;
}
#ifdef notyet
void 
xencons_suspend(void)
{

	if (!xen_start_info->console_evtchn)
		return;

	unbind_evtchn_from_irqhandler(xen_start_info->console_evtchn, NULL);
}

void 
xencons_resume(void)
{

	(void)xencons_ring_init();
}
#endif
/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 8
 * tab-width: 4
 * indent-tabs-mode: t
 * End:
 */
