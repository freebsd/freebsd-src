/******************************************************************************
 * evtchn.c
 * 
 * Xenolinux driver for receiving and demuxing event-channel signals.
 * 
 * Copyright (c) 2004, K A Fraser
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/selinfo.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>

#include <machine/cpufunc.h>
#include <machine/intr_machdep.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xen_intr.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <machine/xen/synch_bitops.h>
#include <machine/xen/hypervisor.h>
#include <machine/xen/evtchn.h>


typedef struct evtchn_sotfc {

	struct selinfo  ev_rsel;
} evtchn_softc_t;


#ifdef linuxcrap
/* NB. This must be shared amongst drivers if more things go in /dev/xen */
static devfs_handle_t xen_dev_dir;
#endif

/* Only one process may open /dev/xen/evtchn at any time. */
static unsigned long evtchn_dev_inuse;

/* Notification ring, accessed via /dev/xen/evtchn. */

#define EVTCHN_RING_SIZE     2048  /* 2048 16-bit entries */

#define EVTCHN_RING_MASK(_i) ((_i)&(EVTCHN_RING_SIZE-1))
static uint16_t *ring;
static unsigned int ring_cons, ring_prod, ring_overflow;

/* Which ports is user-space bound to? */
static uint32_t bound_ports[32];

/* Unique address for processes to sleep on */
static void *evtchn_waddr = &ring;

static struct mtx lock, upcall_lock;

static d_read_t      evtchn_read;
static d_write_t     evtchn_write;
static d_ioctl_t     evtchn_ioctl;
static d_poll_t      evtchn_poll;
static d_open_t      evtchn_open;
static d_close_t     evtchn_close;


void 
evtchn_device_upcall(int port)
{
	mtx_lock(&upcall_lock);

	mask_evtchn(port);
	clear_evtchn(port);

	if ( ring != NULL ) {
		if ( (ring_prod - ring_cons) < EVTCHN_RING_SIZE ) {
			ring[EVTCHN_RING_MASK(ring_prod)] = (uint16_t)port;
			if ( ring_cons == ring_prod++ ) {
				wakeup(evtchn_waddr);
			}
		}
		else {
			ring_overflow = 1;
		}
	}

	mtx_unlock(&upcall_lock);
}

static void 
__evtchn_reset_buffer_ring(void)
{
	/* Initialise the ring to empty. Clear errors. */
	ring_cons = ring_prod = ring_overflow = 0;
}

static int
evtchn_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	int rc;
	unsigned int count, c, p, sst = 0, bytes1 = 0, bytes2 = 0;
	count = uio->uio_resid;
    
	count &= ~1; /* even number of bytes */

	if ( count == 0 )
	{
		rc = 0;
		goto out;
	}

	if ( count > PAGE_SIZE )
		count = PAGE_SIZE;

	for ( ; ; ) {
		if ( (c = ring_cons) != (p = ring_prod) )
			break;

		if ( ring_overflow ) {
			rc = EFBIG;
			goto out;
		}

		if (sst != 0) {
			rc = EINTR;
			goto out;
		}

		/* PCATCH == check for signals before and after sleeping 
		 * PWAIT == priority of waiting on resource 
		 */
		sst = tsleep(evtchn_waddr, PWAIT|PCATCH, "evchwt", 10);
	}

	/* Byte lengths of two chunks. Chunk split (if any) is at ring wrap. */
	if ( ((c ^ p) & EVTCHN_RING_SIZE) != 0 ) {
		bytes1 = (EVTCHN_RING_SIZE - EVTCHN_RING_MASK(c)) * sizeof(uint16_t);
		bytes2 = EVTCHN_RING_MASK(p) * sizeof(uint16_t);
	}
	else {
		bytes1 = (p - c) * sizeof(uint16_t);
		bytes2 = 0;
	}

	/* Truncate chunks according to caller's maximum byte count. */
	if ( bytes1 > count ) {
		bytes1 = count;
		bytes2 = 0;
	}
	else if ( (bytes1 + bytes2) > count ) {
		bytes2 = count - bytes1;
	}
    
	if ( uiomove(&ring[EVTCHN_RING_MASK(c)], bytes1, uio) ||
	     ((bytes2 != 0) && uiomove(&ring[0], bytes2, uio)))
		/* keeping this around as its replacement is not equivalent 
		 * copyout(&ring[0], &buf[bytes1], bytes2) 
		 */
	{
		rc = EFAULT;
		goto out;
	}

	ring_cons += (bytes1 + bytes2) / sizeof(uint16_t);

	rc = bytes1 + bytes2;

 out:
    
	return rc;
}

static int 
evtchn_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	int  rc, i, count;
    
	count = uio->uio_resid;
    
	uint16_t *kbuf = (uint16_t *)malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK);


	if ( kbuf == NULL )
		return ENOMEM;

	count &= ~1; /* even number of bytes */

	if ( count == 0 ) {
		rc = 0;
		goto out;
	}

	if ( count > PAGE_SIZE )
		count = PAGE_SIZE;

	if ( uiomove(kbuf, count, uio) != 0 ) {
		rc = EFAULT;
		goto out;
	}

	mtx_lock_spin(&lock);
	for ( i = 0; i < (count/2); i++ )
		if ( test_bit(kbuf[i], &bound_ports[0]) )
			unmask_evtchn(kbuf[i]);
	mtx_unlock_spin(&lock);

	rc = count;

 out:
	free(kbuf, M_DEVBUF);
	return rc;
}

static int 
evtchn_ioctl(struct cdev *dev, unsigned long cmd, caddr_t arg, 
	     int mode, struct thread *td __unused)
{
	int rc = 0;
    
	mtx_lock_spin(&lock);
    
	switch ( cmd )
	{
	case EVTCHN_RESET:
		__evtchn_reset_buffer_ring();
		break;
	case EVTCHN_BIND:
		if ( !synch_test_and_set_bit((int)arg, &bound_ports[0]) )
			unmask_evtchn((int)arg);
		else
			rc = EINVAL;
		break;
	case EVTCHN_UNBIND:
		if ( synch_test_and_clear_bit((int)arg, &bound_ports[0]) )
			mask_evtchn((int)arg);
		else
			rc = EINVAL;
		break;
	default:
		rc = ENOSYS;
		break;
	}

	mtx_unlock_spin(&lock);   

	return rc;
}

static int
evtchn_poll(struct cdev *dev, int poll_events, struct thread *td)
{

	evtchn_softc_t *sc;
	unsigned int mask = POLLOUT | POLLWRNORM;
    
	sc = dev->si_drv1;
    
	if ( ring_cons != ring_prod )
		mask |= POLLIN | POLLRDNORM;
	else if ( ring_overflow )
		mask = POLLERR;
	else
		selrecord(td, &sc->ev_rsel);


	return mask;
}


static int 
evtchn_open(struct cdev *dev, int flag, int otyp, struct thread *td)
{
	uint16_t *_ring;
    
	if (flag & O_NONBLOCK)
		return EBUSY;

	if ( synch_test_and_set_bit(0, &evtchn_dev_inuse) )
		return EBUSY;

	if ( (_ring = (uint16_t *)malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK)) == NULL )
		return ENOMEM;

	mtx_lock_spin(&lock);
	ring = _ring;
	__evtchn_reset_buffer_ring();
	mtx_unlock_spin(&lock);


	return 0;
}

static int 
evtchn_close(struct cdev *dev, int flag, int otyp, struct thread *td __unused)
{
	int i;

	mtx_lock_spin(&lock);
	if (ring != NULL) {
		free(ring, M_DEVBUF);
		ring = NULL;
	}
	for ( i = 0; i < NR_EVENT_CHANNELS; i++ )
		if ( synch_test_and_clear_bit(i, &bound_ports[0]) )
			mask_evtchn(i);
	mtx_unlock_spin(&lock);

	evtchn_dev_inuse = 0;

	return 0;
}

static struct cdevsw evtchn_devsw = {
	d_version:   D_VERSION,
	d_open:      evtchn_open,
	d_close:     evtchn_close,
	d_read:      evtchn_read,
	d_write:     evtchn_write,
	d_ioctl:     evtchn_ioctl,
	d_poll:      evtchn_poll,
	d_name:      "evtchn",
	d_flags:     0,
};


/* XXX  - if this device is ever supposed to support use by more than one process
 * this global static will have to go away
 */
static struct cdev *evtchn_dev;



static int 
evtchn_init(void *dummy __unused)
{
	/* XXX I believe we don't need these leaving them here for now until we 
	 * have some semblance of it working 
	 */
	mtx_init(&upcall_lock, "evtchup", NULL, MTX_DEF);

	/* (DEVFS) create '/dev/misc/evtchn'. */
	evtchn_dev = make_dev(&evtchn_devsw, 0, UID_ROOT, GID_WHEEL, 0600, "xen/evtchn");

	mtx_init(&lock, "evch", NULL, MTX_SPIN | MTX_NOWITNESS);

	evtchn_dev->si_drv1 = malloc(sizeof(evtchn_softc_t), M_DEVBUF, M_WAITOK);
	bzero(evtchn_dev->si_drv1, sizeof(evtchn_softc_t));

	/* XXX I don't think we need any of this rubbish */
#if 0
	if ( err != 0 )
	{
		printk(KERN_ALERT "Could not register /dev/misc/evtchn\n");
		return err;
	}

	/* (DEVFS) create directory '/dev/xen'. */
	xen_dev_dir = devfs_mk_dir(NULL, "xen", NULL);

	/* (DEVFS) &link_dest[pos] == '../misc/evtchn'. */
	pos = devfs_generate_path(evtchn_miscdev.devfs_handle, 
				  &link_dest[3], 
				  sizeof(link_dest) - 3);
	if ( pos >= 0 )
		strncpy(&link_dest[pos], "../", 3);
	/* (DEVFS) symlink '/dev/xen/evtchn' -> '../misc/evtchn'. */
	(void)devfs_mk_symlink(xen_dev_dir, 
			       "evtchn", 
			       DEVFS_FL_DEFAULT, 
			       &link_dest[pos],
			       &symlink_handle, 
			       NULL);

	/* (DEVFS) automatically destroy the symlink with its destination. */
	devfs_auto_unregister(evtchn_miscdev.devfs_handle, symlink_handle);
#endif
	printk("Event-channel device installed.\n");

	return 0;
}


SYSINIT(evtchn_init, SI_SUB_DRIVERS, SI_ORDER_FIRST, evtchn_init, NULL);


