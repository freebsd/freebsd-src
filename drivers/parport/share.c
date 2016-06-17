/* $Id: parport_share.c,v 1.15 1998/01/11 12:06:17 philip Exp $
 * Parallel-port resource manager code.
 * 
 * Authors: David Campbell <campbell@tirian.che.curtin.edu.au>
 *          Tim Waugh <tim@cyberelk.demon.co.uk>
 *          Jose Renau <renau@acm.org>
 *          Philip Blundell <philb@gnu.org>
 *	    Andrea Arcangeli
 *
 * based on work by Grant Guenther <grant@torque.net>
 *          and Philip Blundell
 *
 * Any part of this program may be used in documents licensed under
 * the GNU Free Documentation License, Version 1.1 or any later version
 * published by the Free Software Foundation.
 */

#undef PARPORT_DEBUG_SHARING		/* undef for production */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/threads.h>
#include <linux/parport.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kmod.h>

#include <linux/spinlock.h>
#include <asm/irq.h>

#undef PARPORT_PARANOID

#define PARPORT_DEFAULT_TIMESLICE	(HZ/5)

unsigned long parport_default_timeslice = PARPORT_DEFAULT_TIMESLICE;
int parport_default_spintime =  DEFAULT_SPIN_TIME;

static struct parport *portlist = NULL, *portlist_tail = NULL;
static spinlock_t parportlist_lock = SPIN_LOCK_UNLOCKED;

static struct parport_driver *driver_chain = NULL;
static spinlock_t driverlist_lock = SPIN_LOCK_UNLOCKED;

/* What you can do to a port that's gone away.. */
static void dead_write_lines (struct parport *p, unsigned char b){}
static unsigned char dead_read_lines (struct parport *p) { return 0; }
static unsigned char dead_frob_lines (struct parport *p, unsigned char b,
			     unsigned char c) { return 0; }
static void dead_onearg (struct parport *p){}
static void dead_initstate (struct pardevice *d, struct parport_state *s) { }
static void dead_state (struct parport *p, struct parport_state *s) { }
static void dead_noargs (void) { }
static size_t dead_write (struct parport *p, const void *b, size_t l, int f)
{ return 0; }
static size_t dead_read (struct parport *p, void *b, size_t l, int f)
{ return 0; }
static struct parport_operations dead_ops = {
	dead_write_lines,	/* data */
	dead_read_lines,
	dead_write_lines,	/* control */
	dead_read_lines,
	dead_frob_lines,
	dead_read_lines,	/* status */
	dead_onearg,		/* enable_irq */
	dead_onearg,		/* disable_irq */
	dead_onearg,		/* data_forward */
	dead_onearg,		/* data_reverse */
	dead_initstate,		/* init_state */
	dead_state,
	dead_state,
	dead_noargs,		/* xxx_use_count */
	dead_noargs,
	dead_write,		/* epp */
	dead_read,
	dead_write,
	dead_read,
	dead_write,		/* ecp */
	dead_read,
	dead_write,
	dead_write,		/* compat */
	dead_read,		/* nibble */
	dead_read		/* byte */
};

/* Call attach(port) for each registered driver. */
static void attach_driver_chain(struct parport *port)
{
	struct parport_driver *drv;
	void (**attach) (struct parport *);
	int count = 0, i;

	/* This is complicated because attach() must be able to block,
	 * but we can't let it do that while we're holding a
	 * spinlock. */

	spin_lock (&driverlist_lock);
	for (drv = driver_chain; drv; drv = drv->next)
		count++;
	spin_unlock (&driverlist_lock);

	/* Drivers can unregister here; that's okay.  If they register
	 * they'll be given an attach during parport_register_driver,
	 * so that's okay too.  The only worry is that someone might
	 * get given an attach twice if they registered just before
	 * this function gets called. */

	/* Hmm, this could be fixed with a generation number..
	 * FIXME */

	attach = kmalloc (sizeof (void(*)(struct parport *)) * count,
			  GFP_KERNEL);
	if (!attach) {
		printk (KERN_WARNING "parport: not enough memory to attach\n");
		return;
	}

	spin_lock (&driverlist_lock);
	for (i = 0, drv = driver_chain; drv && i < count; drv = drv->next)
		attach[i++] = drv->attach;
	spin_unlock (&driverlist_lock);

	for (count = 0; count < i; count++)
		(*attach[count]) (port);

	kfree (attach);
}

/* Call detach(port) for each registered driver. */
static void detach_driver_chain(struct parport *port)
{
	struct parport_driver *drv;

	spin_lock (&driverlist_lock);
	for (drv = driver_chain; drv; drv = drv->next)
		drv->detach (port);
	spin_unlock (&driverlist_lock);
}

/* Ask kmod for some lowlevel drivers. */
static void get_lowlevel_driver (void)
{
	/* There is no actual module called this: you should set
	 * up an alias for modutils. */
	request_module ("parport_lowlevel");
}

/**
 *	parport_register_driver - register a parallel port device driver
 *	@drv: structure describing the driver
 *
 *	This can be called by a parallel port device driver in order
 *	to receive notifications about ports being found in the
 *	system, as well as ports no longer available.
 *
 *	The @drv structure is allocated by the caller and must not be
 *	deallocated until after calling parport_unregister_driver().
 *
 *	The driver's attach() function may block.  The port that
 *	attach() is given will be valid for the duration of the
 *	callback, but if the driver wants to take a copy of the
 *	pointer it must call parport_get_port() to do so.  Calling
 *	parport_register_device() on that port will do this for you.
 *
 *	The driver's detach() function may not block.  The port that
 *	detach() is given will be valid for the duration of the
 *	callback, but if the driver wants to take a copy of the
 *	pointer it must call parport_get_port() to do so.
 *
 *	Returns 0 on success.  Currently it always succeeds.
 **/

int parport_register_driver (struct parport_driver *drv)
{
	struct parport *port;
	struct parport **ports;
	int count = 0, i;

	if (!portlist)
		get_lowlevel_driver ();

	/* We have to take the portlist lock for this to be sure
	 * that port is valid for the duration of the callback. */

	/* This is complicated by the fact that attach must be allowed
	 * to block, so we can't be holding any spinlocks when we call
	 * it.  But we need to hold a spinlock to iterate over the
	 * list of ports.. */

	spin_lock (&parportlist_lock);
	for (port = portlist; port; port = port->next)
		count++;
	spin_unlock (&parportlist_lock);

	ports = kmalloc (sizeof (struct parport *) * count, GFP_KERNEL);
	if (!ports)
		printk (KERN_WARNING "parport: not enough memory to attach\n");
	else {
		spin_lock (&parportlist_lock);
		for (i = 0, port = portlist; port && i < count;
		     port = port->next)
			ports[i++] = port;
		spin_unlock (&parportlist_lock);

		for (count = 0; count < i; count++)
			drv->attach (ports[count]);

		kfree (ports);
	}

	spin_lock (&driverlist_lock);
	drv->next = driver_chain;
	driver_chain = drv;
	spin_unlock (&driverlist_lock);

	return 0;
}

/**
 *	parport_unregister_driver - deregister a parallel port device driver
 *	@arg: structure describing the driver that was given to
 *	      parport_register_driver()
 *
 *	This should be called by a parallel port device driver that
 *	has registered itself using parport_register_driver() when it
 *	is about to be unloaded.
 *
 *	When it returns, the driver's attach() routine will no longer
 *	be called, and for each port that attach() was called for, the
 *	detach() routine will have been called.
 *
 *	If the caller's attach() function can block, it is their
 *	responsibility to make sure to wait for it to exit before
 *	unloading.
 *
 *	All the driver's detach() calls are guaranteed to have
 *	finished by the time this function returns.
 *
 *	The driver's detach() call is not allowed to block.
 **/

void parport_unregister_driver (struct parport_driver *arg)
{
	struct parport_driver *drv = driver_chain, *olddrv = NULL;

	while (drv) {
		if (drv == arg) {
			struct parport *port;

			spin_lock (&driverlist_lock);
			if (olddrv)
				olddrv->next = drv->next;
			else
				driver_chain = drv->next;
			spin_unlock (&driverlist_lock);

			/* Call the driver's detach routine for each
			 * port to clean up any resources that the
			 * attach routine acquired. */
			spin_lock (&parportlist_lock);
			for (port = portlist; port; port = port->next)
				drv->detach (port);
			spin_unlock (&parportlist_lock);

			return;
		}
		olddrv = drv;
		drv = drv->next;
	}
}

static void free_port (struct parport *port)
{
	int d;
	for (d = 0; d < 5; d++) {
		if (port->probe_info[d].class_name)
			kfree (port->probe_info[d].class_name);
		if (port->probe_info[d].mfr)
			kfree (port->probe_info[d].mfr);
		if (port->probe_info[d].model)
			kfree (port->probe_info[d].model);
		if (port->probe_info[d].cmdset)
			kfree (port->probe_info[d].cmdset);
		if (port->probe_info[d].description)
			kfree (port->probe_info[d].description);
	}

	kfree(port->name);
	kfree(port);
}

/**
 *	parport_get_port - increment a port's reference count
 *	@port: the port
 *
 *	This ensure's that a struct parport pointer remains valid
 *	until the matching parport_put_port() call.
 **/

struct parport *parport_get_port (struct parport *port)
{
	atomic_inc (&port->ref_count);
	return port;
}

/**
 *	parport_put_port - decrement a port's reference count
 *	@port: the port
 *
 *	This should be called once for each call to parport_get_port(),
 *	once the port is no longer needed.
 **/

void parport_put_port (struct parport *port)
{
	if (atomic_dec_and_test (&port->ref_count))
		/* Can destroy it now. */
		free_port (port);

	return;
}

/**
 *	parport_enumerate - return a list of the system's parallel ports
 *
 *	This returns the head of the list of parallel ports in the
 *	system, as a &struct parport.  The structure that is returned
 *	describes the first port in the list, and its 'next' member
 *	points to the next port, or %NULL if it's the last port.
 *
 *	If there are no parallel ports in the system,
 *	parport_enumerate() will return %NULL.
 **/

struct parport *parport_enumerate(void)
{
	/* Don't use this: use parport_register_driver instead. */

	if (!portlist)
		get_lowlevel_driver ();

	return portlist;
}

/**
 *	parport_register_port - register a parallel port
 *	@base: base I/O address
 *	@irq: IRQ line
 *	@dma: DMA channel
 *	@ops: pointer to the port driver's port operations structure
 *
 *	When a parallel port (lowlevel) driver finds a port that
 *	should be made available to parallel port device drivers, it
 *	should call parport_register_port().  The @base, @irq, and
 *	@dma parameters are for the convenience of port drivers, and
 *	for ports where they aren't meaningful needn't be set to
 *	anything special.  They can be altered afterwards by adjusting
 *	the relevant members of the parport structure that is returned
 *	and represents the port.  They should not be tampered with
 *	after calling parport_announce_port, however.
 *
 *	If there are parallel port device drivers in the system that
 *	have registered themselves using parport_register_driver(),
 *	they are not told about the port at this time; that is done by
 *	parport_announce_port().
 *
 *	The @ops structure is allocated by the caller, and must not be
 *	deallocated before calling parport_unregister_port().
 *
 *	If there is no memory to allocate a new parport structure,
 *	this function will return %NULL.
 **/

struct parport *parport_register_port(unsigned long base, int irq, int dma,
				      struct parport_operations *ops)
{
	struct parport *tmp;
	int portnum;
	int device;
	char *name;

	tmp = kmalloc(sizeof(struct parport), GFP_KERNEL);
	if (!tmp) {
		printk(KERN_WARNING "parport: memory squeeze\n");
		return NULL;
	}

	/* Search for the lowest free parport number. */

	spin_lock_irq (&parportlist_lock);
	for (portnum = 0; ; portnum++) {
		struct parport *itr = portlist;
		while (itr) {
			if (itr->number == portnum)
				/* No good, already used. */
				break;
			else
				itr = itr->next;
		}

		if (itr == NULL)
			/* Got to the end of the list. */
			break;
	}
	spin_unlock_irq (&parportlist_lock);
	
	/* Init our structure */
 	memset(tmp, 0, sizeof(struct parport));
	tmp->base = base;
	tmp->irq = irq;
	tmp->dma = dma;
	tmp->muxport = tmp->daisy = tmp->muxsel = -1;
	tmp->modes = 0;
 	tmp->next = NULL;
	tmp->devices = tmp->cad = NULL;
	tmp->flags = 0;
	tmp->ops = ops;
	tmp->portnum = tmp->number = portnum;
	tmp->physport = tmp;
	memset (tmp->probe_info, 0, 5 * sizeof (struct parport_device_info));
	tmp->cad_lock = RW_LOCK_UNLOCKED;
	spin_lock_init(&tmp->waitlist_lock);
	spin_lock_init(&tmp->pardevice_lock);
	tmp->ieee1284.mode = IEEE1284_MODE_COMPAT;
	tmp->ieee1284.phase = IEEE1284_PH_FWD_IDLE;
	init_MUTEX_LOCKED (&tmp->ieee1284.irq); /* actually a semaphore at 0 */
	tmp->spintime = parport_default_spintime;
	atomic_set (&tmp->ref_count, 1);

	name = kmalloc(15, GFP_KERNEL);
	if (!name) {
		printk(KERN_ERR "parport: memory squeeze\n");
		kfree(tmp);
		return NULL;
	}
	sprintf(name, "parport%d", portnum);
	tmp->name = name;

	/*
	 * Chain the entry to our list.
	 *
	 * This function must not run from an irq handler so we don' t need
	 * to clear irq on the local CPU. -arca
	 */

	spin_lock(&parportlist_lock);

	/* We are locked against anyone else performing alterations, but
	 * because of parport_enumerate people can still _read_ the list
	 * while we are changing it; so be careful..
	 *
	 * It's okay to have portlist_tail a little bit out of sync
	 * since it's only used for changing the list, not for reading
	 * from it.
	 */

	if (portlist_tail)
		portlist_tail->next = tmp;
	portlist_tail = tmp;
	if (!portlist)
		portlist = tmp;
	spin_unlock(&parportlist_lock);

	for (device = 0; device < 5; device++)
		/* assume the worst */
		tmp->probe_info[device].class = PARPORT_CLASS_LEGACY;

	tmp->waithead = tmp->waittail = NULL;

	return tmp;
}

/**
 *	parport_announce_port - tell device drivers about a parallel port
 *	@port: parallel port to announce
 *
 *	After a port driver has registered a parallel port with
 *	parport_register_port, and performed any necessary
 *	initialisation or adjustments, it should call
 *	parport_announce_port() in order to notify all device drivers
 *	that have called parport_register_driver().  Their attach()
 *	functions will be called, with @port as the parameter.
 **/

void parport_announce_port (struct parport *port)
{
#ifdef CONFIG_PARPORT_1284
	/* Analyse the IEEE1284.3 topology of the port. */
	if (parport_daisy_init (port) == 0) {
		/* No devices were detected.  Perhaps they are in some
                   funny state; let's try to reset them and see if
                   they wake up. */
		parport_daisy_fini (port);
		parport_write_control (port, PARPORT_CONTROL_SELECT);
		udelay (50);
		parport_write_control (port,
				       PARPORT_CONTROL_SELECT |
				       PARPORT_CONTROL_INIT);
		udelay (50);
		parport_daisy_init (port);
	}
#endif

	/* Let drivers know that a new port has arrived. */
	attach_driver_chain (port);
}

/**
 *	parport_unregister_port - deregister a parallel port
 *	@port: parallel port to deregister
 *
 *	When a parallel port driver is forcibly unloaded, or a
 *	parallel port becomes inaccessible, the port driver must call
 *	this function in order to deal with device drivers that still
 *	want to use it.
 *
 *	The parport structure associated with the port has its
 *	operations structure replaced with one containing 'null'
 *	operations that return errors or just don't do anything.
 *
 *	Any drivers that have registered themselves using
 *	parport_register_driver() are notified that the port is no
 *	longer accessible by having their detach() routines called
 *	with @port as the parameter.
 **/

void parport_unregister_port(struct parport *port)
{
	struct parport *p;

	port->ops = &dead_ops;

	/* Spread the word. */
	detach_driver_chain (port);

#ifdef CONFIG_PARPORT_1284
	/* Forget the IEEE1284.3 topology of the port. */
	parport_daisy_fini (port);
#endif

	spin_lock(&parportlist_lock);

	/* We are protected from other people changing the list, but
	 * they can still see it (using parport_enumerate).  So be
	 * careful about the order of writes.. */
	if (portlist == port) {
		if ((portlist = port->next) == NULL)
			portlist_tail = NULL;
	} else {
		for (p = portlist; (p != NULL) && (p->next != port); 
		     p=p->next);
		if (p) {
			if ((p->next = port->next) == NULL)
				portlist_tail = p;
		}
		else printk (KERN_WARNING
			     "%s not found in port list!\n", port->name);
	}
	spin_unlock(&parportlist_lock);

	/* Yes, parport_enumerate _is_ unsafe.  Don't use it. */
	parport_put_port (port);
}

/**
 *	parport_register_device - register a device on a parallel port
 *	@port: port to which the device is attached
 *	@name: a name to refer to the device
 *	@pf: preemption callback
 *	@kf: kick callback (wake-up)
 *	@irq_func: interrupt handler
 *	@flags: registration flags
 *	@handle: data for callback functions
 *
 *	This function, called by parallel port device drivers,
 *	declares that a device is connected to a port, and tells the
 *	system all it needs to know.
 *
 *	The @name is allocated by the caller and must not be
 *	deallocated until the caller calls @parport_unregister_device
 *	for that device.
 *
 *	The preemption callback function, @pf, is called when this
 *	device driver has claimed access to the port but another
 *	device driver wants to use it.  It is given @handle as its
 *	parameter, and should return zero if it is willing for the
 *	system to release the port to another driver on its behalf.
 *	If it wants to keep control of the port it should return
 *	non-zero, and no action will be taken.  It is good manners for
 *	the driver to try to release the port at the earliest
 *	opportunity after its preemption callback rejects a preemption
 *	attempt.  Note that if a preemption callback is happy for
 *	preemption to go ahead, there is no need to release the port;
 *	it is done automatically.  This function may not block, as it
 *	may be called from interrupt context.  If the device driver
 *	does not support preemption, @pf can be %NULL.
 *
 *	The wake-up ("kick") callback function, @kf, is called when
 *	the port is available to be claimed for exclusive access; that
 *	is, parport_claim() is guaranteed to succeed when called from
 *	inside the wake-up callback function.  If the driver wants to
 *	claim the port it should do so; otherwise, it need not take
 *	any action.  This function may not block, as it may be called
 *	from interrupt context.  If the device driver does not want to
 *	be explicitly invited to claim the port in this way, @kf can
 *	be %NULL.
 *
 *	The interrupt handler, @irq_func, is called when an interrupt
 *	arrives from the parallel port.  Note that if a device driver
 *	wants to use interrupts it should use parport_enable_irq(),
 *	and can also check the irq member of the parport structure
 *	representing the port.
 *
 *	The parallel port (lowlevel) driver is the one that has called
 *	request_irq() and whose interrupt handler is called first.
 *	This handler does whatever needs to be done to the hardware to
 *	acknowledge the interrupt (for PC-style ports there is nothing
 *	special to be done).  It then tells the IEEE 1284 code about
 *	the interrupt, which may involve reacting to an IEEE 1284
 *	event depending on the current IEEE 1284 phase.  After this,
 *	it calls @irq_func.  Needless to say, @irq_func will be called
 *	from interrupt context, and may not block.
 *
 *	The %PARPORT_DEV_EXCL flag is for preventing port sharing, and
 *	so should only be used when sharing the port with other device
 *	drivers is impossible and would lead to incorrect behaviour.
 *	Use it sparingly!  Normally, @flags will be zero.
 *
 *	This function returns a pointer to a structure that represents
 *	the device on the port, or %NULL if there is not enough memory
 *	to allocate space for that structure.
 **/

struct pardevice *
parport_register_device(struct parport *port, const char *name,
			int (*pf)(void *), void (*kf)(void *),
			void (*irq_func)(int, void *, struct pt_regs *), 
			int flags, void *handle)
{
	struct pardevice *tmp;

	if (port->physport->flags & PARPORT_FLAG_EXCL) {
		/* An exclusive device is registered. */
		printk (KERN_DEBUG "%s: no more devices allowed\n",
			port->name);
		return NULL;
	}

	if (flags & PARPORT_DEV_LURK) {
		if (!pf || !kf) {
			printk(KERN_INFO "%s: refused to register lurking device (%s) without callbacks\n", port->name, name);
			return NULL;
		}
	}

	/* We up our own module reference count, and that of the port
           on which a device is to be registered, to ensure that
           neither of us gets unloaded while we sleep in (e.g.)
           kmalloc.  To be absolutely safe, we have to require that
           our caller doesn't sleep in between parport_enumerate and
           parport_register_device.. */
	inc_parport_count();
	port->ops->inc_use_count();
	parport_get_port (port);

	tmp = kmalloc(sizeof(struct pardevice), GFP_KERNEL);
	if (tmp == NULL) {
		printk(KERN_WARNING "%s: memory squeeze, couldn't register %s.\n", port->name, name);
		goto out;
	}

	tmp->state = kmalloc(sizeof(struct parport_state), GFP_KERNEL);
	if (tmp->state == NULL) {
		printk(KERN_WARNING "%s: memory squeeze, couldn't register %s.\n", port->name, name);
		goto out_free_pardevice;
	}

	tmp->name = name;
	tmp->port = port;
	tmp->daisy = -1;
	tmp->preempt = pf;
	tmp->wakeup = kf;
	tmp->private = handle;
	tmp->flags = flags;
	tmp->irq_func = irq_func;
	tmp->waiting = 0;
	tmp->timeout = 5 * HZ;

	/* Chain this onto the list */
	tmp->prev = NULL;
	/*
	 * This function must not run from an irq handler so we don' t need
	 * to clear irq on the local CPU. -arca
	 */
	spin_lock(&port->physport->pardevice_lock);

	if (flags & PARPORT_DEV_EXCL) {
		if (port->physport->devices) {
			spin_unlock (&port->physport->pardevice_lock);
			printk (KERN_DEBUG
				"%s: cannot grant exclusive access for "
				"device %s\n", port->name, name);
			goto out_free_all;
		}
		port->flags |= PARPORT_FLAG_EXCL;
	}

	tmp->next = port->physport->devices;
	wmb(); /* Make sure that tmp->next is written before it's
                  added to the list; see comments marked 'no locking
                  required' */
	if (port->physport->devices)
		port->physport->devices->prev = tmp;
	port->physport->devices = tmp;
	spin_unlock(&port->physport->pardevice_lock);

	init_waitqueue_head(&tmp->wait_q);
	tmp->timeslice = parport_default_timeslice;
	tmp->waitnext = tmp->waitprev = NULL;

	/*
	 * This has to be run as last thing since init_state may need other
	 * pardevice fields. -arca
	 */
	port->ops->init_state(tmp, tmp->state);
	parport_device_proc_register(tmp);
	return tmp;

 out_free_all:
	kfree (tmp->state);
 out_free_pardevice:
	kfree (tmp);
 out:
	dec_parport_count();
	port->ops->dec_use_count();
	parport_put_port (port);
	return NULL;
}

/**
 *	parport_unregister_device - deregister a device on a parallel port
 *	@dev: pointer to structure representing device
 *
 *	This undoes the effect of parport_register_device().
 **/

void parport_unregister_device(struct pardevice *dev)
{
	struct parport *port;

#ifdef PARPORT_PARANOID
	if (dev == NULL) {
		printk(KERN_ERR "parport_unregister_device: passed NULL\n");
		return;
	}
#endif

	parport_device_proc_unregister(dev);

	port = dev->port->physport;

	if (port->cad == dev) {
		printk(KERN_DEBUG "%s: %s forgot to release port\n",
		       port->name, dev->name);
		parport_release (dev);
	}

	spin_lock(&port->pardevice_lock);
	if (dev->next)
		dev->next->prev = dev->prev;
	if (dev->prev)
		dev->prev->next = dev->next;
	else
		port->devices = dev->next;

	if (dev->flags & PARPORT_DEV_EXCL)
		port->flags &= ~PARPORT_FLAG_EXCL;

	spin_unlock(&port->pardevice_lock);

	/* Make sure we haven't left any pointers around in the wait
	 * list. */
	spin_lock (&port->waitlist_lock);
	if (dev->waitprev || dev->waitnext || port->waithead == dev) {
		if (dev->waitprev)
			dev->waitprev->waitnext = dev->waitnext;
		else
			port->waithead = dev->waitnext;
		if (dev->waitnext)
			dev->waitnext->waitprev = dev->waitprev;
		else
			port->waittail = dev->waitprev;
	}
	spin_unlock (&port->waitlist_lock);

	kfree(dev->state);
	kfree(dev);

	dec_parport_count();
	port->ops->dec_use_count();
	parport_put_port (port);

	/* Yes, that's right, someone _could_ still have a pointer to
	 * port, if they used parport_enumerate.  That's why they
	 * shouldn't use it (and use parport_register_driver instead)..
	 */
}

/**
 *	parport_find_number - find a parallel port by number
 *	@number: parallel port number
 *
 *	This returns the parallel port with the specified number, or
 *	%NULL if there is none.
 *
 *	There is an implicit parport_get_port() done already; to throw
 *	away the reference to the port that parport_find_number()
 *	gives you, use parport_put_port().
 */

struct parport *parport_find_number (int number)
{
	struct parport *port, *result = NULL;

	if (!portlist)
		get_lowlevel_driver ();

	spin_lock (&parportlist_lock);
	for (port = portlist; port; port = port->next)
		if (port->number == number) {
			result = parport_get_port (port);
			break;
		}
	spin_unlock (&parportlist_lock);
	return result;
}

/**
 *	parport_find_base - find a parallel port by base address
 *	@base: base I/O address
 *
 *	This returns the parallel port with the specified base
 *	address, or %NULL if there is none.
 *
 *	There is an implicit parport_get_port() done already; to throw
 *	away the reference to the port that parport_find_base()
 *	gives you, use parport_put_port().
 */

struct parport *parport_find_base (unsigned long base)
{
	struct parport *port, *result = NULL;

	if (!portlist)
		get_lowlevel_driver ();

	spin_lock (&parportlist_lock);
	for (port = portlist; port; port = port->next)
		if (port->base == base) {
			result = parport_get_port (port);
			break;
		}
	spin_unlock (&parportlist_lock);
	return result;
}

/**
 *	parport_claim - claim access to a parallel port device
 *	@dev: pointer to structure representing a device on the port
 *
 *	This function will not block and so can be used from interrupt
 *	context.  If parport_claim() succeeds in claiming access to
 *	the port it returns zero and the port is available to use.  It
 *	may fail (returning non-zero) if the port is in use by another
 *	driver and that driver is not willing to relinquish control of
 *	the port.
 **/

int parport_claim(struct pardevice *dev)
{
	struct pardevice *oldcad;
	struct parport *port = dev->port->physport;
	unsigned long flags;

	if (port->cad == dev) {
		printk(KERN_INFO "%s: %s already owner\n",
		       dev->port->name,dev->name);
		return 0;
	}

	/* Preempt any current device */
	write_lock_irqsave (&port->cad_lock, flags);
	if ((oldcad = port->cad) != NULL) {
		if (oldcad->preempt) {
			if (oldcad->preempt(oldcad->private))
				goto blocked;
			port->ops->save_state(port, dev->state);
		} else
			goto blocked;

		if (port->cad != oldcad) {
			/* I think we'll actually deadlock rather than
                           get here, but just in case.. */
			printk(KERN_WARNING
			       "%s: %s released port when preempted!\n",
			       port->name, oldcad->name);
			if (port->cad)
				goto blocked;
		}
	}

	/* Can't fail from now on, so mark ourselves as no longer waiting.  */
	if (dev->waiting & 1) {
		dev->waiting = 0;

		/* Take ourselves out of the wait list again.  */
		spin_lock_irq (&port->waitlist_lock);
		if (dev->waitprev)
			dev->waitprev->waitnext = dev->waitnext;
		else
			port->waithead = dev->waitnext;
		if (dev->waitnext)
			dev->waitnext->waitprev = dev->waitprev;
		else
			port->waittail = dev->waitprev;
		spin_unlock_irq (&port->waitlist_lock);
		dev->waitprev = dev->waitnext = NULL;
	}

	/* Now we do the change of devices */
	port->cad = dev;

#ifdef CONFIG_PARPORT_1284
	/* If it's a mux port, select it. */
	if (dev->port->muxport >= 0) {
		/* FIXME */
		port->muxsel = dev->port->muxport;
	}

	/* If it's a daisy chain device, select it. */
	if (dev->daisy >= 0) {
		/* This could be lazier. */
		if (!parport_daisy_select (port, dev->daisy,
					   IEEE1284_MODE_COMPAT))
			port->daisy = dev->daisy;
	}
#endif /* IEEE1284.3 support */

	/* Restore control registers */
	port->ops->restore_state(port, dev->state);
	write_unlock_irqrestore(&port->cad_lock, flags);
	dev->time = jiffies;
	return 0;

blocked:
	/* If this is the first time we tried to claim the port, register an
	   interest.  This is only allowed for devices sleeping in
	   parport_claim_or_block(), or those with a wakeup function.  */

	/* The cad_lock is still held for writing here */
	if (dev->waiting & 2 || dev->wakeup) {
		spin_lock (&port->waitlist_lock);
		if (test_and_set_bit(0, &dev->waiting) == 0) {
			/* First add ourselves to the end of the wait list. */
			dev->waitnext = NULL;
			dev->waitprev = port->waittail;
			if (port->waittail) {
				port->waittail->waitnext = dev;
				port->waittail = dev;
			} else
				port->waithead = port->waittail = dev;
		}
		spin_unlock (&port->waitlist_lock);
	}
	write_unlock_irqrestore (&port->cad_lock, flags);
	return -EAGAIN;
}

/**
 *	parport_claim_or_block - claim access to a parallel port device
 *	@dev: pointer to structure representing a device on the port
 *
 *	This behaves like parport_claim(), but will block if necessary
 *	to wait for the port to be free.  A return value of 1
 *	indicates that it slept; 0 means that it succeeded without
 *	needing to sleep.  A negative error code indicates failure.
 **/

int parport_claim_or_block(struct pardevice *dev)
{
	int r;

	/* Signal to parport_claim() that we can wait even without a
	   wakeup function.  */
	dev->waiting = 2;

	/* Try to claim the port.  If this fails, we need to sleep.  */
	r = parport_claim(dev);
	if (r == -EAGAIN) {
		unsigned long flags;
#ifdef PARPORT_DEBUG_SHARING
		printk(KERN_DEBUG "%s: parport_claim() returned -EAGAIN\n", dev->name);
#endif
		save_flags (flags);
		cli();
		/* If dev->waiting is clear now, an interrupt
		   gave us the port and we would deadlock if we slept.  */
		if (dev->waiting) {
			interruptible_sleep_on (&dev->wait_q);
			if (signal_pending (current)) {
				restore_flags (flags);
				return -EINTR;
			}
			r = 1;
		} else {
			r = 0;
#ifdef PARPORT_DEBUG_SHARING
			printk(KERN_DEBUG "%s: didn't sleep in parport_claim_or_block()\n",
			       dev->name);
#endif
		}
		restore_flags(flags);
#ifdef PARPORT_DEBUG_SHARING
		if (dev->port->physport->cad != dev)
			printk(KERN_DEBUG "%s: exiting parport_claim_or_block "
			       "but %s owns port!\n", dev->name,
			       dev->port->physport->cad ?
			       dev->port->physport->cad->name:"nobody");
#endif
	}
	dev->waiting = 0;
	return r;
}

/**
 *	parport_release - give up access to a parallel port device
 *	@dev: pointer to structure representing parallel port device
 *
 *	This function cannot fail, but it should not be called without
 *	the port claimed.  Similarly, if the port is already claimed
 *	you should not try claiming it again.
 **/

void parport_release(struct pardevice *dev)
{
	struct parport *port = dev->port->physport;
	struct pardevice *pd;
	unsigned long flags;

	/* Make sure that dev is the current device */
	write_lock_irqsave(&port->cad_lock, flags);
	if (port->cad != dev) {
		write_unlock_irqrestore (&port->cad_lock, flags);
		printk(KERN_WARNING "%s: %s tried to release parport "
		       "when not owner\n", port->name, dev->name);
		return;
	}

#ifdef CONFIG_PARPORT_1284
	/* If this is on a mux port, deselect it. */
	if (dev->port->muxport >= 0) {
		/* FIXME */
		port->muxsel = -1;
	}

	/* If this is a daisy device, deselect it. */
	if (dev->daisy >= 0) {
		parport_daisy_deselect_all (port);
		port->daisy = -1;
	}
#endif

	port->cad = NULL;
	write_unlock_irqrestore(&port->cad_lock, flags);

	/* Save control registers */
	port->ops->save_state(port, dev->state);

	/* If anybody is waiting, find out who's been there longest and
	   then wake them up. (Note: no locking required) */
	/* !!! LOCKING IS NEEDED HERE */
	for (pd = port->waithead; pd; pd = pd->waitnext) {
		if (pd->waiting & 2) { /* sleeping in claim_or_block */
			parport_claim(pd);
			if (waitqueue_active(&pd->wait_q))
				wake_up_interruptible(&pd->wait_q);
			return;
		} else if (pd->wakeup) {
			pd->wakeup(pd->private);
			if (dev->port->cad) /* racy but no matter */
				return;
		} else {
			printk(KERN_ERR "%s: don't know how to wake %s\n", port->name, pd->name);
		}
	}

	/* Nobody was waiting, so walk the list to see if anyone is
	   interested in being woken up. (Note: no locking required) */
	/* !!! LOCKING IS NEEDED HERE */
	for (pd = port->devices; (port->cad == NULL) && pd; pd = pd->next) {
		if (pd->wakeup && pd != dev)
			pd->wakeup(pd->private);
	}
}

static int parport_parse_params (int nports, const char *str[], int val[],
				 int automatic, int none, int nofifo)
{
	unsigned int i;
	for (i = 0; i < nports && str[i]; i++) {
		if (!strncmp(str[i], "auto", 4))
			val[i] = automatic;
		else if (!strncmp(str[i], "none", 4))
			val[i] = none;
		else if (nofifo && !strncmp(str[i], "nofifo", 4))
			val[i] = nofifo;
		else {
			char *ep;
			unsigned long r = simple_strtoul(str[i], &ep, 0);
			if (ep != str[i])
				val[i] = r;
			else {
				printk(KERN_ERR "parport: bad specifier `%s'\n", str[i]);
				return -1;
			}
		}
	}

	return 0;
}

int parport_parse_irqs(int nports, const char *irqstr[], int irqval[])
{
	return parport_parse_params (nports, irqstr, irqval, PARPORT_IRQ_AUTO,
				     PARPORT_IRQ_NONE, 0);
}

int parport_parse_dmas(int nports, const char *dmastr[], int dmaval[])
{
	return parport_parse_params (nports, dmastr, dmaval, PARPORT_DMA_AUTO,
				     PARPORT_DMA_NONE, PARPORT_DMA_NOFIFO);
}
MODULE_LICENSE("GPL");
