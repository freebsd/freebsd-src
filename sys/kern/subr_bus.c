/*-
 * Copyright (c) 1997,1998,2003 Doug Rabson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/filio.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/condvar.h>
#include <sys/queue.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/selinfo.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/bus.h>

#include <machine/stdarg.h>

#include <vm/uma.h>

SYSCTL_NODE(_hw, OID_AUTO, bus, CTLFLAG_RW, NULL, NULL);
SYSCTL_NODE(, OID_AUTO, dev, CTLFLAG_RW, NULL, NULL);

/*
 * Used to attach drivers to devclasses.
 */
typedef struct driverlink *driverlink_t;
struct driverlink {
	kobj_class_t	driver;
	TAILQ_ENTRY(driverlink) link;	/* list of drivers in devclass */
};

/*
 * Forward declarations
 */
typedef TAILQ_HEAD(devclass_list, devclass) devclass_list_t;
typedef TAILQ_HEAD(driver_list, driverlink) driver_list_t;
typedef TAILQ_HEAD(device_list, device) device_list_t;

struct devclass {
	TAILQ_ENTRY(devclass) link;
	devclass_t	parent;		/* parent in devclass hierarchy */
	driver_list_t	drivers;     /* bus devclasses store drivers for bus */
	char		*name;
	device_t	*devices;	/* array of devices indexed by unit */
	int		maxunit;	/* size of devices array */

	struct sysctl_ctx_list sysctl_ctx;
	struct sysctl_oid *sysctl_tree;
};

/**
 * @brief Implementation of device.
 */
struct device {
	/*
	 * A device is a kernel object. The first field must be the
	 * current ops table for the object.
	 */
	KOBJ_FIELDS;

	/*
	 * Device hierarchy.
	 */
	TAILQ_ENTRY(device)	link;	/**< list of devices in parent */
	TAILQ_ENTRY(device)	devlink; /**< global device list membership */
	device_t	parent;		/**< parent of this device  */
	device_list_t	children;	/**< list of child devices */

	/*
	 * Details of this device.
	 */
	driver_t	*driver;	/**< current driver */
	devclass_t	devclass;	/**< current device class */
	int		unit;		/**< current unit number */
	char*		nameunit;	/**< name+unit e.g. foodev0 */
	char*		desc;		/**< driver specific description */
	int		busy;		/**< count of calls to device_busy() */
	device_state_t	state;		/**< current device state  */
	u_int32_t	devflags;	/**< api level flags for device_get_flags() */
	u_short		flags;		/**< internal device flags  */
#define	DF_ENABLED	1		/* device should be probed/attached */
#define	DF_FIXEDCLASS	2		/* devclass specified at create time */
#define	DF_WILDCARD	4		/* unit was originally wildcard */
#define	DF_DESCMALLOCED	8		/* description was malloced */
#define	DF_QUIET	16		/* don't print verbose attach message */
#define	DF_DONENOMATCH	32		/* don't execute DEVICE_NOMATCH again */
#define	DF_EXTERNALSOFTC 64		/* softc not allocated by us */
#define	DF_REBID	128		/* Can rebid after attach */
	u_char	order;			/**< order from device_add_child_ordered() */
	u_char	pad;
	void	*ivars;			/**< instance variables  */
	void	*softc;			/**< current driver's variables  */

	struct sysctl_ctx_list sysctl_ctx; /**< state for sysctl variables  */
	struct sysctl_oid *sysctl_tree;	/**< state for sysctl variables */
};

static MALLOC_DEFINE(M_BUS, "bus", "Bus data structures");
static MALLOC_DEFINE(M_BUS_SC, "bus-sc", "Bus data structures, softc");

#ifdef BUS_DEBUG

static int bus_debug = 1;
TUNABLE_INT("bus.debug", &bus_debug);
SYSCTL_INT(_debug, OID_AUTO, bus_debug, CTLFLAG_RW, &bus_debug, 0,
    "Debug bus code");

#define PDEBUG(a)	if (bus_debug) {printf("%s:%d: ", __func__, __LINE__), printf a; printf("\n");}
#define DEVICENAME(d)	((d)? device_get_name(d): "no device")
#define DRIVERNAME(d)	((d)? d->name : "no driver")
#define DEVCLANAME(d)	((d)? d->name : "no devclass")

/**
 * Produce the indenting, indent*2 spaces plus a '.' ahead of that to
 * prevent syslog from deleting initial spaces
 */
#define indentprintf(p)	do { int iJ; printf("."); for (iJ=0; iJ<indent; iJ++) printf("  "); printf p ; } while (0)

static void print_device_short(device_t dev, int indent);
static void print_device(device_t dev, int indent);
void print_device_tree_short(device_t dev, int indent);
void print_device_tree(device_t dev, int indent);
static void print_driver_short(driver_t *driver, int indent);
static void print_driver(driver_t *driver, int indent);
static void print_driver_list(driver_list_t drivers, int indent);
static void print_devclass_short(devclass_t dc, int indent);
static void print_devclass(devclass_t dc, int indent);
void print_devclass_list_short(void);
void print_devclass_list(void);

#else
/* Make the compiler ignore the function calls */
#define PDEBUG(a)			/* nop */
#define DEVICENAME(d)			/* nop */
#define DRIVERNAME(d)			/* nop */
#define DEVCLANAME(d)			/* nop */

#define print_device_short(d,i)		/* nop */
#define print_device(d,i)		/* nop */
#define print_device_tree_short(d,i)	/* nop */
#define print_device_tree(d,i)		/* nop */
#define print_driver_short(d,i)		/* nop */
#define print_driver(d,i)		/* nop */
#define print_driver_list(d,i)		/* nop */
#define print_devclass_short(d,i)	/* nop */
#define print_devclass(d,i)		/* nop */
#define print_devclass_list_short()	/* nop */
#define print_devclass_list()		/* nop */
#endif

/*
 * dev sysctl tree
 */

enum {
	DEVCLASS_SYSCTL_PARENT,
};

static int
devclass_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	devclass_t dc = (devclass_t)arg1;
	const char *value;

	switch (arg2) {
	case DEVCLASS_SYSCTL_PARENT:
		value = dc->parent ? dc->parent->name : "";
		break;
	default:
		return (EINVAL);
	}
	return (SYSCTL_OUT(req, value, strlen(value)));
}

static void
devclass_sysctl_init(devclass_t dc)
{

	if (dc->sysctl_tree != NULL)
		return;
	sysctl_ctx_init(&dc->sysctl_ctx);
	dc->sysctl_tree = SYSCTL_ADD_NODE(&dc->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_dev), OID_AUTO, dc->name,
	    CTLFLAG_RD, 0, "");
	SYSCTL_ADD_PROC(&dc->sysctl_ctx, SYSCTL_CHILDREN(dc->sysctl_tree),
	    OID_AUTO, "%parent", CTLFLAG_RD,
	    dc, DEVCLASS_SYSCTL_PARENT, devclass_sysctl_handler, "A",
	    "parent class");
}

enum {
	DEVICE_SYSCTL_DESC,
	DEVICE_SYSCTL_DRIVER,
	DEVICE_SYSCTL_LOCATION,
	DEVICE_SYSCTL_PNPINFO,
	DEVICE_SYSCTL_PARENT,
};

static int
device_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	const char *value;
	char *buf;
	int error;

	buf = NULL;
	switch (arg2) {
	case DEVICE_SYSCTL_DESC:
		value = dev->desc ? dev->desc : "";
		break;
	case DEVICE_SYSCTL_DRIVER:
		value = dev->driver ? dev->driver->name : "";
		break;
	case DEVICE_SYSCTL_LOCATION:
		value = buf = malloc(1024, M_BUS, M_WAITOK | M_ZERO);
		bus_child_location_str(dev, buf, 1024);
		break;
	case DEVICE_SYSCTL_PNPINFO:
		value = buf = malloc(1024, M_BUS, M_WAITOK | M_ZERO);
		bus_child_pnpinfo_str(dev, buf, 1024);
		break;
	case DEVICE_SYSCTL_PARENT:
		value = dev->parent ? dev->parent->nameunit : "";
		break;
	default:
		return (EINVAL);
	}
	error = SYSCTL_OUT(req, value, strlen(value));
	if (buf != NULL)
		free(buf, M_BUS);
	return (error);
}

static void
device_sysctl_init(device_t dev)
{
	devclass_t dc = dev->devclass;

	if (dev->sysctl_tree != NULL)
		return;
	devclass_sysctl_init(dc);
	sysctl_ctx_init(&dev->sysctl_ctx);
	dev->sysctl_tree = SYSCTL_ADD_NODE(&dev->sysctl_ctx,
	    SYSCTL_CHILDREN(dc->sysctl_tree), OID_AUTO,
	    dev->nameunit + strlen(dc->name),
	    CTLFLAG_RD, 0, "");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%desc", CTLFLAG_RD,
	    dev, DEVICE_SYSCTL_DESC, device_sysctl_handler, "A",
	    "device description");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%driver", CTLFLAG_RD,
	    dev, DEVICE_SYSCTL_DRIVER, device_sysctl_handler, "A",
	    "device driver name");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%location", CTLFLAG_RD,
	    dev, DEVICE_SYSCTL_LOCATION, device_sysctl_handler, "A",
	    "device location relative to parent");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%pnpinfo", CTLFLAG_RD,
	    dev, DEVICE_SYSCTL_PNPINFO, device_sysctl_handler, "A",
	    "device identification");
	SYSCTL_ADD_PROC(&dev->sysctl_ctx, SYSCTL_CHILDREN(dev->sysctl_tree),
	    OID_AUTO, "%parent", CTLFLAG_RD,
	    dev, DEVICE_SYSCTL_PARENT, device_sysctl_handler, "A",
	    "parent device");
}

static void
device_sysctl_fini(device_t dev)
{
	if (dev->sysctl_tree == NULL)
		return;
	sysctl_ctx_free(&dev->sysctl_ctx);
	dev->sysctl_tree = NULL;
}

/*
 * /dev/devctl implementation
 */

/*
 * This design allows only one reader for /dev/devctl.  This is not desirable
 * in the long run, but will get a lot of hair out of this implementation.
 * Maybe we should make this device a clonable device.
 *
 * Also note: we specifically do not attach a device to the device_t tree
 * to avoid potential chicken and egg problems.  One could argue that all
 * of this belongs to the root node.  One could also further argue that the
 * sysctl interface that we have not might more properly be an ioctl
 * interface, but at this stage of the game, I'm not inclined to rock that
 * boat.
 *
 * I'm also not sure that the SIGIO support is done correctly or not, as
 * I copied it from a driver that had SIGIO support that likely hasn't been
 * tested since 3.4 or 2.2.8!
 */

static int sysctl_devctl_disable(SYSCTL_HANDLER_ARGS);
static int devctl_disable = 0;
TUNABLE_INT("hw.bus.devctl_disable", &devctl_disable);
SYSCTL_PROC(_hw_bus, OID_AUTO, devctl_disable, CTLTYPE_INT | CTLFLAG_RW, 0, 0,
    sysctl_devctl_disable, "I", "devctl disable");

static d_open_t		devopen;
static d_close_t	devclose;
static d_read_t		devread;
static d_ioctl_t	devioctl;
static d_poll_t		devpoll;

static struct cdevsw dev_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	devopen,
	.d_close =	devclose,
	.d_read =	devread,
	.d_ioctl =	devioctl,
	.d_poll =	devpoll,
	.d_name =	"devctl",
};

struct dev_event_info
{
	char *dei_data;
	TAILQ_ENTRY(dev_event_info) dei_link;
};

TAILQ_HEAD(devq, dev_event_info);

static struct dev_softc
{
	int	inuse;
	int	nonblock;
	struct mtx mtx;
	struct cv cv;
	struct selinfo sel;
	struct devq devq;
	struct proc *async_proc;
} devsoftc;

static struct cdev *devctl_dev;

static void
devinit(void)
{
	devctl_dev = make_dev(&dev_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "devctl");
	mtx_init(&devsoftc.mtx, "dev mtx", "devd", MTX_DEF);
	cv_init(&devsoftc.cv, "dev cv");
	TAILQ_INIT(&devsoftc.devq);
}

static int
devopen(struct cdev *dev, int oflags, int devtype, d_thread_t *td)
{
	if (devsoftc.inuse)
		return (EBUSY);
	/* move to init */
	devsoftc.inuse = 1;
	devsoftc.nonblock = 0;
	devsoftc.async_proc = NULL;
	return (0);
}

static int
devclose(struct cdev *dev, int fflag, int devtype, d_thread_t *td)
{
	devsoftc.inuse = 0;
	mtx_lock(&devsoftc.mtx);
	cv_broadcast(&devsoftc.cv);
	mtx_unlock(&devsoftc.mtx);

	return (0);
}

/*
 * The read channel for this device is used to report changes to
 * userland in realtime.  We are required to free the data as well as
 * the n1 object because we allocate them separately.  Also note that
 * we return one record at a time.  If you try to read this device a
 * character at a time, you will loose the rest of the data.  Listening
 * programs are expected to cope.
 */
static int
devread(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct dev_event_info *n1;
	int rv;

	mtx_lock(&devsoftc.mtx);
	while (TAILQ_EMPTY(&devsoftc.devq)) {
		if (devsoftc.nonblock) {
			mtx_unlock(&devsoftc.mtx);
			return (EAGAIN);
		}
		rv = cv_wait_sig(&devsoftc.cv, &devsoftc.mtx);
		if (rv) {
			/*
			 * Need to translate ERESTART to EINTR here? -- jake
			 */
			mtx_unlock(&devsoftc.mtx);
			return (rv);
		}
	}
	n1 = TAILQ_FIRST(&devsoftc.devq);
	TAILQ_REMOVE(&devsoftc.devq, n1, dei_link);
	mtx_unlock(&devsoftc.mtx);
	rv = uiomove(n1->dei_data, strlen(n1->dei_data), uio);
	free(n1->dei_data, M_BUS);
	free(n1, M_BUS);
	return (rv);
}

static	int
devioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, d_thread_t *td)
{
	switch (cmd) {

	case FIONBIO:
		if (*(int*)data)
			devsoftc.nonblock = 1;
		else
			devsoftc.nonblock = 0;
		return (0);
	case FIOASYNC:
		if (*(int*)data)
			devsoftc.async_proc = td->td_proc;
		else
			devsoftc.async_proc = NULL;
		return (0);

		/* (un)Support for other fcntl() calls. */
	case FIOCLEX:
	case FIONCLEX:
	case FIONREAD:
	case FIOSETOWN:
	case FIOGETOWN:
	default:
		break;
	}
	return (ENOTTY);
}

static	int
devpoll(struct cdev *dev, int events, d_thread_t *td)
{
	int	revents = 0;

	mtx_lock(&devsoftc.mtx);
	if (events & (POLLIN | POLLRDNORM)) {
		if (!TAILQ_EMPTY(&devsoftc.devq))
			revents = events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &devsoftc.sel);
	}
	mtx_unlock(&devsoftc.mtx);

	return (revents);
}

/**
 * @brief Queue data to be read from the devctl device
 *
 * Generic interface to queue data to the devctl device.  It is
 * assumed that @p data is properly formatted.  It is further assumed
 * that @p data is allocated using the M_BUS malloc type.
 */
void
devctl_queue_data(char *data)
{
	struct dev_event_info *n1 = NULL;
	struct proc *p;

	n1 = malloc(sizeof(*n1), M_BUS, M_NOWAIT);
	if (n1 == NULL)
		return;
	n1->dei_data = data;
	mtx_lock(&devsoftc.mtx);
	TAILQ_INSERT_TAIL(&devsoftc.devq, n1, dei_link);
	cv_broadcast(&devsoftc.cv);
	mtx_unlock(&devsoftc.mtx);
	selwakeup(&devsoftc.sel);
	p = devsoftc.async_proc;
	if (p != NULL) {
		PROC_LOCK(p);
		psignal(p, SIGIO);
		PROC_UNLOCK(p);
	}
}

/**
 * @brief Send a 'notification' to userland, using standard ways
 */
void
devctl_notify(const char *system, const char *subsystem, const char *type,
    const char *data)
{
	int len = 0;
	char *msg;

	if (system == NULL)
		return;		/* BOGUS!  Must specify system. */
	if (subsystem == NULL)
		return;		/* BOGUS!  Must specify subsystem. */
	if (type == NULL)
		return;		/* BOGUS!  Must specify type. */
	len += strlen(" system=") + strlen(system);
	len += strlen(" subsystem=") + strlen(subsystem);
	len += strlen(" type=") + strlen(type);
	/* add in the data message plus newline. */
	if (data != NULL)
		len += strlen(data);
	len += 3;	/* '!', '\n', and NUL */
	msg = malloc(len, M_BUS, M_NOWAIT);
	if (msg == NULL)
		return;		/* Drop it on the floor */
	snprintf(msg, len, "!system=%s subsystem=%s type=%s %s\n", system,
	    subsystem, type, data);
	devctl_queue_data(msg);
}

/*
 * Common routine that tries to make sending messages as easy as possible.
 * We allocate memory for the data, copy strings into that, but do not
 * free it unless there's an error.  The dequeue part of the driver should
 * free the data.  We don't send data when the device is disabled.  We do
 * send data, even when we have no listeners, because we wish to avoid
 * races relating to startup and restart of listening applications.
 *
 * devaddq is designed to string together the type of event, with the
 * object of that event, plus the plug and play info and location info
 * for that event.  This is likely most useful for devices, but less
 * useful for other consumers of this interface.  Those should use
 * the devctl_queue_data() interface instead.
 */
static void
devaddq(const char *type, const char *what, device_t dev)
{
	char *data = NULL;
	char *loc = NULL;
	char *pnp = NULL;
	const char *parstr;

	if (devctl_disable)
		return;
	data = malloc(1024, M_BUS, M_NOWAIT);
	if (data == NULL)
		goto bad;

	/* get the bus specific location of this device */
	loc = malloc(1024, M_BUS, M_NOWAIT);
	if (loc == NULL)
		goto bad;
	*loc = '\0';
	bus_child_location_str(dev, loc, 1024);

	/* Get the bus specific pnp info of this device */
	pnp = malloc(1024, M_BUS, M_NOWAIT);
	if (pnp == NULL)
		goto bad;
	*pnp = '\0';
	bus_child_pnpinfo_str(dev, pnp, 1024);

	/* Get the parent of this device, or / if high enough in the tree. */
	if (device_get_parent(dev) == NULL)
		parstr = ".";	/* Or '/' ? */
	else
		parstr = device_get_nameunit(device_get_parent(dev));
	/* String it all together. */
	snprintf(data, 1024, "%s%s at %s %s on %s\n", type, what, loc, pnp,
	  parstr);
	free(loc, M_BUS);
	free(pnp, M_BUS);
	devctl_queue_data(data);
	return;
bad:
	free(pnp, M_BUS);
	free(loc, M_BUS);
	free(data, M_BUS);
	return;
}

/*
 * A device was added to the tree.  We are called just after it successfully
 * attaches (that is, probe and attach success for this device).  No call
 * is made if a device is merely parented into the tree.  See devnomatch
 * if probe fails.  If attach fails, no notification is sent (but maybe
 * we should have a different message for this).
 */
static void
devadded(device_t dev)
{
	char *pnp = NULL;
	char *tmp = NULL;

	pnp = malloc(1024, M_BUS, M_NOWAIT);
	if (pnp == NULL)
		goto fail;
	tmp = malloc(1024, M_BUS, M_NOWAIT);
	if (tmp == NULL)
		goto fail;
	*pnp = '\0';
	bus_child_pnpinfo_str(dev, pnp, 1024);
	snprintf(tmp, 1024, "%s %s", device_get_nameunit(dev), pnp);
	devaddq("+", tmp, dev);
fail:
	if (pnp != NULL)
		free(pnp, M_BUS);
	if (tmp != NULL)
		free(tmp, M_BUS);
	return;
}

/*
 * A device was removed from the tree.  We are called just before this
 * happens.
 */
static void
devremoved(device_t dev)
{
	char *pnp = NULL;
	char *tmp = NULL;

	pnp = malloc(1024, M_BUS, M_NOWAIT);
	if (pnp == NULL)
		goto fail;
	tmp = malloc(1024, M_BUS, M_NOWAIT);
	if (tmp == NULL)
		goto fail;
	*pnp = '\0';
	bus_child_pnpinfo_str(dev, pnp, 1024);
	snprintf(tmp, 1024, "%s %s", device_get_nameunit(dev), pnp);
	devaddq("-", tmp, dev);
fail:
	if (pnp != NULL)
		free(pnp, M_BUS);
	if (tmp != NULL)
		free(tmp, M_BUS);
	return;
}

/*
 * Called when there's no match for this device.  This is only called
 * the first time that no match happens, so we don't keep getitng this
 * message.  Should that prove to be undesirable, we can change it.
 * This is called when all drivers that can attach to a given bus
 * decline to accept this device.  Other errrors may not be detected.
 */
static void
devnomatch(device_t dev)
{
	devaddq("?", "", dev);
}

static int
sysctl_devctl_disable(SYSCTL_HANDLER_ARGS)
{
	struct dev_event_info *n1;
	int dis, error;

	dis = devctl_disable;
	error = sysctl_handle_int(oidp, &dis, 0, req);
	if (error || !req->newptr)
		return (error);
	mtx_lock(&devsoftc.mtx);
	devctl_disable = dis;
	if (dis) {
		while (!TAILQ_EMPTY(&devsoftc.devq)) {
			n1 = TAILQ_FIRST(&devsoftc.devq);
			TAILQ_REMOVE(&devsoftc.devq, n1, dei_link);
			free(n1->dei_data, M_BUS);
			free(n1, M_BUS);
		}
	}
	mtx_unlock(&devsoftc.mtx);
	return (0);
}

/* End of /dev/devctl code */

TAILQ_HEAD(,device)	bus_data_devices;
static int bus_data_generation = 1;

kobj_method_t null_methods[] = {
	{ 0, 0 }
};

DEFINE_CLASS(null, null_methods, 0);

/*
 * Devclass implementation
 */

static devclass_list_t devclasses = TAILQ_HEAD_INITIALIZER(devclasses);


/**
 * @internal
 * @brief Find or create a device class
 *
 * If a device class with the name @p classname exists, return it,
 * otherwise if @p create is non-zero create and return a new device
 * class.
 *
 * If @p parentname is non-NULL, the parent of the devclass is set to
 * the devclass of that name.
 *
 * @param classname	the devclass name to find or create
 * @param parentname	the parent devclass name or @c NULL
 * @param create	non-zero to create a devclass
 */
static devclass_t
devclass_find_internal(const char *classname, const char *parentname,
		       int create)
{
	devclass_t dc;

	PDEBUG(("looking for %s", classname));
	if (!classname)
		return (NULL);

	TAILQ_FOREACH(dc, &devclasses, link) {
		if (!strcmp(dc->name, classname))
			break;
	}

	if (create && !dc) {
		PDEBUG(("creating %s", classname));
		dc = malloc(sizeof(struct devclass) + strlen(classname) + 1,
		    M_BUS, M_NOWAIT|M_ZERO);
		if (!dc)
			return (NULL);
		dc->parent = NULL;
		dc->name = (char*) (dc + 1);
		strcpy(dc->name, classname);
		TAILQ_INIT(&dc->drivers);
		TAILQ_INSERT_TAIL(&devclasses, dc, link);

		bus_data_generation_update();
	}
	if (parentname && dc && !dc->parent) {
		dc->parent = devclass_find_internal(parentname, 0, FALSE);
	}

	return (dc);
}

/**
 * @brief Create a device class
 *
 * If a device class with the name @p classname exists, return it,
 * otherwise create and return a new device class.
 *
 * @param classname	the devclass name to find or create
 */
devclass_t
devclass_create(const char *classname)
{
	return (devclass_find_internal(classname, 0, TRUE));
}

/**
 * @brief Find a device class
 *
 * If a device class with the name @p classname exists, return it,
 * otherwise return @c NULL.
 *
 * @param classname	the devclass name to find
 */
devclass_t
devclass_find(const char *classname)
{
	return (devclass_find_internal(classname, 0, FALSE));
}

/**
 * @brief Add a device driver to a device class
 *
 * Add a device driver to a devclass. This is normally called
 * automatically by DRIVER_MODULE(). The BUS_DRIVER_ADDED() method of
 * all devices in the devclass will be called to allow them to attempt
 * to re-probe any unmatched children.
 *
 * @param dc		the devclass to edit
 * @param driver	the driver to register
 */
int
devclass_add_driver(devclass_t dc, driver_t *driver)
{
	driverlink_t dl;
	int i;

	PDEBUG(("%s", DRIVERNAME(driver)));

	dl = malloc(sizeof *dl, M_BUS, M_NOWAIT|M_ZERO);
	if (!dl)
		return (ENOMEM);

	/*
	 * Compile the driver's methods. Also increase the reference count
	 * so that the class doesn't get freed when the last instance
	 * goes. This means we can safely use static methods and avoids a
	 * double-free in devclass_delete_driver.
	 */
	kobj_class_compile((kobj_class_t) driver);

	/*
	 * Make sure the devclass which the driver is implementing exists.
	 */
	devclass_find_internal(driver->name, 0, TRUE);

	dl->driver = driver;
	TAILQ_INSERT_TAIL(&dc->drivers, dl, link);
	driver->refs++;		/* XXX: kobj_mtx */

	/*
	 * Call BUS_DRIVER_ADDED for any existing busses in this class.
	 */
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i])
			BUS_DRIVER_ADDED(dc->devices[i], driver);

	bus_data_generation_update();
	return (0);
}

/**
 * @brief Delete a device driver from a device class
 *
 * Delete a device driver from a devclass. This is normally called
 * automatically by DRIVER_MODULE().
 *
 * If the driver is currently attached to any devices,
 * devclass_delete_driver() will first attempt to detach from each
 * device. If one of the detach calls fails, the driver will not be
 * deleted.
 *
 * @param dc		the devclass to edit
 * @param driver	the driver to unregister
 */
int
devclass_delete_driver(devclass_t busclass, driver_t *driver)
{
	devclass_t dc = devclass_find(driver->name);
	driverlink_t dl;
	device_t dev;
	int i;
	int error;

	PDEBUG(("%s from devclass %s", driver->name, DEVCLANAME(busclass)));

	if (!dc)
		return (0);

	/*
	 * Find the link structure in the bus' list of drivers.
	 */
	TAILQ_FOREACH(dl, &busclass->drivers, link) {
		if (dl->driver == driver)
			break;
	}

	if (!dl) {
		PDEBUG(("%s not found in %s list", driver->name,
		    busclass->name));
		return (ENOENT);
	}

	/*
	 * Disassociate from any devices.  We iterate through all the
	 * devices in the devclass of the driver and detach any which are
	 * using the driver and which have a parent in the devclass which
	 * we are deleting from.
	 *
	 * Note that since a driver can be in multiple devclasses, we
	 * should not detach devices which are not children of devices in
	 * the affected devclass.
	 */
	for (i = 0; i < dc->maxunit; i++) {
		if (dc->devices[i]) {
			dev = dc->devices[i];
			if (dev->driver == driver && dev->parent &&
			    dev->parent->devclass == busclass) {
				if ((error = device_detach(dev)) != 0)
					return (error);
				device_set_driver(dev, NULL);
			}
		}
	}

	TAILQ_REMOVE(&busclass->drivers, dl, link);
	free(dl, M_BUS);

	/* XXX: kobj_mtx */
	driver->refs--;
	if (driver->refs == 0)
		kobj_class_free((kobj_class_t) driver);

	bus_data_generation_update();
	return (0);
}

/**
 * @brief Quiesces a set of device drivers from a device class
 *
 * Quiesce a device driver from a devclass. This is normally called
 * automatically by DRIVER_MODULE().
 *
 * If the driver is currently attached to any devices,
 * devclass_quiesece_driver() will first attempt to quiesce each
 * device.
 *
 * @param dc		the devclass to edit
 * @param driver	the driver to unregister
 */
int
devclass_quiesce_driver(devclass_t busclass, driver_t *driver)
{
	devclass_t dc = devclass_find(driver->name);
	driverlink_t dl;
	device_t dev;
	int i;
	int error;

	PDEBUG(("%s from devclass %s", driver->name, DEVCLANAME(busclass)));

	if (!dc)
		return (0);

	/*
	 * Find the link structure in the bus' list of drivers.
	 */
	TAILQ_FOREACH(dl, &busclass->drivers, link) {
		if (dl->driver == driver)
			break;
	}

	if (!dl) {
		PDEBUG(("%s not found in %s list", driver->name,
		    busclass->name));
		return (ENOENT);
	}

	/*
	 * Quiesce all devices.  We iterate through all the devices in
	 * the devclass of the driver and quiesce any which are using
	 * the driver and which have a parent in the devclass which we
	 * are quiescing.
	 *
	 * Note that since a driver can be in multiple devclasses, we
	 * should not quiesce devices which are not children of
	 * devices in the affected devclass.
	 */
	for (i = 0; i < dc->maxunit; i++) {
		if (dc->devices[i]) {
			dev = dc->devices[i];
			if (dev->driver == driver && dev->parent &&
			    dev->parent->devclass == busclass) {
				if ((error = device_quiesce(dev)) != 0)
					return (error);
			}
		}
	}

	return (0);
}

/**
 * @internal
 */
static driverlink_t
devclass_find_driver_internal(devclass_t dc, const char *classname)
{
	driverlink_t dl;

	PDEBUG(("%s in devclass %s", classname, DEVCLANAME(dc)));

	TAILQ_FOREACH(dl, &dc->drivers, link) {
		if (!strcmp(dl->driver->name, classname))
			return (dl);
	}

	PDEBUG(("not found"));
	return (NULL);
}

/**
 * @brief Search a devclass for a driver
 *
 * This function searches the devclass's list of drivers and returns
 * the first driver whose name is @p classname or @c NULL if there is
 * no driver of that name.
 *
 * @param dc		the devclass to search
 * @param classname	the driver name to search for
 */
kobj_class_t
devclass_find_driver(devclass_t dc, const char *classname)
{
	driverlink_t dl;

	dl = devclass_find_driver_internal(dc, classname);
	if (dl)
		return (dl->driver);
	return (NULL);
}

/**
 * @brief Return the name of the devclass
 */
const char *
devclass_get_name(devclass_t dc)
{
	return (dc->name);
}

/**
 * @brief Find a device given a unit number
 *
 * @param dc		the devclass to search
 * @param unit		the unit number to search for
 * 
 * @returns		the device with the given unit number or @c
 *			NULL if there is no such device
 */
device_t
devclass_get_device(devclass_t dc, int unit)
{
	if (dc == NULL || unit < 0 || unit >= dc->maxunit)
		return (NULL);
	return (dc->devices[unit]);
}

/**
 * @brief Find the softc field of a device given a unit number
 *
 * @param dc		the devclass to search
 * @param unit		the unit number to search for
 * 
 * @returns		the softc field of the device with the given
 *			unit number or @c NULL if there is no such
 *			device
 */
void *
devclass_get_softc(devclass_t dc, int unit)
{
	device_t dev;

	dev = devclass_get_device(dc, unit);
	if (!dev)
		return (NULL);

	return (device_get_softc(dev));
}

/**
 * @brief Get a list of devices in the devclass
 *
 * An array containing a list of all the devices in the given devclass
 * is allocated and returned in @p *devlistp. The number of devices
 * in the array is returned in @p *devcountp. The caller should free
 * the array using @c free(p, M_TEMP).
 *
 * @param dc		the devclass to examine
 * @param devlistp	points at location for array pointer return
 *			value
 * @param devcountp	points at location for array size return value
 *
 * @retval 0		success
 * @retval ENOMEM	the array allocation failed
 */
int
devclass_get_devices(devclass_t dc, device_t **devlistp, int *devcountp)
{
	int count, i;
	device_t *list;

	count = devclass_get_count(dc);
	list = malloc(count * sizeof(device_t), M_TEMP, M_NOWAIT|M_ZERO);
	if (!list)
		return (ENOMEM);

	count = 0;
	for (i = 0; i < dc->maxunit; i++) {
		if (dc->devices[i]) {
			list[count] = dc->devices[i];
			count++;
		}
	}

	*devlistp = list;
	*devcountp = count;

	return (0);
}

/**
 * @brief Get a list of drivers in the devclass
 *
 * An array containing a list of pointers to all the drivers in the
 * given devclass is allocated and returned in @p *listp.  The number
 * of drivers in the array is returned in @p *countp. The caller should
 * free the array using @c free(p, M_TEMP).
 *
 * @param dc		the devclass to examine
 * @param listp		gives location for array pointer return value
 * @param countp	gives location for number of array elements
 *			return value
 *
 * @retval 0		success
 * @retval ENOMEM	the array allocation failed
 */
int
devclass_get_drivers(devclass_t dc, driver_t ***listp, int *countp)
{
	driverlink_t dl;
	driver_t **list;
	int count;

	count = 0;
	TAILQ_FOREACH(dl, &dc->drivers, link)
		count++;
	list = malloc(count * sizeof(driver_t *), M_TEMP, M_NOWAIT);
	if (list == NULL)
		return (ENOMEM);

	count = 0;
	TAILQ_FOREACH(dl, &dc->drivers, link) {
		list[count] = dl->driver;
		count++;
	}
	*listp = list;
	*countp = count;

	return (0);
}

/**
 * @brief Get the number of devices in a devclass
 *
 * @param dc		the devclass to examine
 */
int
devclass_get_count(devclass_t dc)
{
	int count, i;

	count = 0;
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i])
			count++;
	return (count);
}

/**
 * @brief Get the maximum unit number used in a devclass
 *
 * Note that this is one greater than the highest currently-allocated
 * unit.
 *
 * @param dc		the devclass to examine
 */
int
devclass_get_maxunit(devclass_t dc)
{
	return (dc->maxunit);
}

/**
 * @brief Find a free unit number in a devclass
 *
 * This function searches for the first unused unit number greater
 * that or equal to @p unit.
 *
 * @param dc		the devclass to examine
 * @param unit		the first unit number to check
 */
int
devclass_find_free_unit(devclass_t dc, int unit)
{
	if (dc == NULL)
		return (unit);
	while (unit < dc->maxunit && dc->devices[unit] != NULL)
		unit++;
	return (unit);
}

/**
 * @brief Set the parent of a devclass
 *
 * The parent class is normally initialised automatically by
 * DRIVER_MODULE().
 *
 * @param dc		the devclass to edit
 * @param pdc		the new parent devclass
 */
void
devclass_set_parent(devclass_t dc, devclass_t pdc)
{
	dc->parent = pdc;
}

/**
 * @brief Get the parent of a devclass
 *
 * @param dc		the devclass to examine
 */
devclass_t
devclass_get_parent(devclass_t dc)
{
	return (dc->parent);
}

struct sysctl_ctx_list *
devclass_get_sysctl_ctx(devclass_t dc)
{
	return (&dc->sysctl_ctx);
}

struct sysctl_oid *
devclass_get_sysctl_tree(devclass_t dc)
{
	return (dc->sysctl_tree);
}

/**
 * @internal
 * @brief Allocate a unit number
 *
 * On entry, @p *unitp is the desired unit number (or @c -1 if any
 * will do). The allocated unit number is returned in @p *unitp.

 * @param dc		the devclass to allocate from
 * @param unitp		points at the location for the allocated unit
 *			number
 *
 * @retval 0		success
 * @retval EEXIST	the requested unit number is already allocated
 * @retval ENOMEM	memory allocation failure
 */
static int
devclass_alloc_unit(devclass_t dc, int *unitp)
{
	int unit = *unitp;

	PDEBUG(("unit %d in devclass %s", unit, DEVCLANAME(dc)));

	/* If we were given a wired unit number, check for existing device */
	/* XXX imp XXX */
	if (unit != -1) {
		if (unit >= 0 && unit < dc->maxunit &&
		    dc->devices[unit] != NULL) {
			if (bootverbose)
				printf("%s: %s%d already exists; skipping it\n",
				    dc->name, dc->name, *unitp);
			return (EEXIST);
		}
	} else {
		/* Unwired device, find the next available slot for it */
		unit = 0;
		while (unit < dc->maxunit && dc->devices[unit] != NULL)
			unit++;
	}

	/*
	 * We've selected a unit beyond the length of the table, so let's
	 * extend the table to make room for all units up to and including
	 * this one.
	 */
	if (unit >= dc->maxunit) {
		device_t *newlist;
		int newsize;

		newsize = roundup((unit + 1), MINALLOCSIZE / sizeof(device_t));
		newlist = malloc(sizeof(device_t) * newsize, M_BUS, M_NOWAIT);
		if (!newlist)
			return (ENOMEM);
		bcopy(dc->devices, newlist, sizeof(device_t) * dc->maxunit);
		bzero(newlist + dc->maxunit,
		    sizeof(device_t) * (newsize - dc->maxunit));
		if (dc->devices)
			free(dc->devices, M_BUS);
		dc->devices = newlist;
		dc->maxunit = newsize;
	}
	PDEBUG(("now: unit %d in devclass %s", unit, DEVCLANAME(dc)));

	*unitp = unit;
	return (0);
}

/**
 * @internal
 * @brief Add a device to a devclass
 *
 * A unit number is allocated for the device (using the device's
 * preferred unit number if any) and the device is registered in the
 * devclass. This allows the device to be looked up by its unit
 * number, e.g. by decoding a dev_t minor number.
 *
 * @param dc		the devclass to add to
 * @param dev		the device to add
 *
 * @retval 0		success
 * @retval EEXIST	the requested unit number is already allocated
 * @retval ENOMEM	memory allocation failure
 */
static int
devclass_add_device(devclass_t dc, device_t dev)
{
	int buflen, error;

	PDEBUG(("%s in devclass %s", DEVICENAME(dev), DEVCLANAME(dc)));

	buflen = snprintf(NULL, 0, "%s%d$", dc->name, dev->unit);
	if (buflen < 0)
		return (ENOMEM);
	dev->nameunit = malloc(buflen, M_BUS, M_NOWAIT|M_ZERO);
	if (!dev->nameunit)
		return (ENOMEM);

	if ((error = devclass_alloc_unit(dc, &dev->unit)) != 0) {
		free(dev->nameunit, M_BUS);
		dev->nameunit = NULL;
		return (error);
	}
	dc->devices[dev->unit] = dev;
	dev->devclass = dc;
	snprintf(dev->nameunit, buflen, "%s%d", dc->name, dev->unit);

	return (0);
}

/**
 * @internal
 * @brief Delete a device from a devclass
 *
 * The device is removed from the devclass's device list and its unit
 * number is freed.

 * @param dc		the devclass to delete from
 * @param dev		the device to delete
 *
 * @retval 0		success
 */
static int
devclass_delete_device(devclass_t dc, device_t dev)
{
	if (!dc || !dev)
		return (0);

	PDEBUG(("%s in devclass %s", DEVICENAME(dev), DEVCLANAME(dc)));

	if (dev->devclass != dc || dc->devices[dev->unit] != dev)
		panic("devclass_delete_device: inconsistent device class");
	dc->devices[dev->unit] = NULL;
	if (dev->flags & DF_WILDCARD)
		dev->unit = -1;
	dev->devclass = NULL;
	free(dev->nameunit, M_BUS);
	dev->nameunit = NULL;

	return (0);
}

/**
 * @internal
 * @brief Make a new device and add it as a child of @p parent
 *
 * @param parent	the parent of the new device
 * @param name		the devclass name of the new device or @c NULL
 *			to leave the devclass unspecified
 * @parem unit		the unit number of the new device of @c -1 to
 *			leave the unit number unspecified
 *
 * @returns the new device
 */
static device_t
make_device(device_t parent, const char *name, int unit)
{
	device_t dev;
	devclass_t dc;

	PDEBUG(("%s at %s as unit %d", name, DEVICENAME(parent), unit));

	if (name) {
		dc = devclass_find_internal(name, 0, TRUE);
		if (!dc) {
			printf("make_device: can't find device class %s\n",
			    name);
			return (NULL);
		}
	} else {
		dc = NULL;
	}

	dev = malloc(sizeof(struct device), M_BUS, M_NOWAIT|M_ZERO);
	if (!dev)
		return (NULL);

	dev->parent = parent;
	TAILQ_INIT(&dev->children);
	kobj_init((kobj_t) dev, &null_class);
	dev->driver = NULL;
	dev->devclass = NULL;
	dev->unit = unit;
	dev->nameunit = NULL;
	dev->desc = NULL;
	dev->busy = 0;
	dev->devflags = 0;
	dev->flags = DF_ENABLED;
	dev->order = 0;
	if (unit == -1)
		dev->flags |= DF_WILDCARD;
	if (name) {
		dev->flags |= DF_FIXEDCLASS;
		if (devclass_add_device(dc, dev)) {
			kobj_delete((kobj_t) dev, M_BUS);
			return (NULL);
		}
	}
	dev->ivars = NULL;
	dev->softc = NULL;

	dev->state = DS_NOTPRESENT;

	TAILQ_INSERT_TAIL(&bus_data_devices, dev, devlink);
	bus_data_generation_update();

	return (dev);
}

/**
 * @internal
 * @brief Print a description of a device.
 */
static int
device_print_child(device_t dev, device_t child)
{
	int retval = 0;

	if (device_is_alive(child))
		retval += BUS_PRINT_CHILD(dev, child);
	else
		retval += device_printf(child, " not found\n");

	return (retval);
}

/**
 * @brief Create a new device
 *
 * This creates a new device and adds it as a child of an existing
 * parent device. The new device will be added after the last existing
 * child with order zero.
 * 
 * @param dev		the device which will be the parent of the
 *			new child device
 * @param name		devclass name for new device or @c NULL if not
 *			specified
 * @param unit		unit number for new device or @c -1 if not
 *			specified
 * 
 * @returns		the new device
 */
device_t
device_add_child(device_t dev, const char *name, int unit)
{
	return (device_add_child_ordered(dev, 0, name, unit));
}

/**
 * @brief Create a new device
 *
 * This creates a new device and adds it as a child of an existing
 * parent device. The new device will be added after the last existing
 * child with the same order.
 * 
 * @param dev		the device which will be the parent of the
 *			new child device
 * @param order		a value which is used to partially sort the
 *			children of @p dev - devices created using
 *			lower values of @p order appear first in @p
 *			dev's list of children
 * @param name		devclass name for new device or @c NULL if not
 *			specified
 * @param unit		unit number for new device or @c -1 if not
 *			specified
 * 
 * @returns		the new device
 */
device_t
device_add_child_ordered(device_t dev, int order, const char *name, int unit)
{
	device_t child;
	device_t place;

	PDEBUG(("%s at %s with order %d as unit %d",
	    name, DEVICENAME(dev), order, unit));

	child = make_device(dev, name, unit);
	if (child == NULL)
		return (child);
	child->order = order;

	TAILQ_FOREACH(place, &dev->children, link) {
		if (place->order > order)
			break;
	}

	if (place) {
		/*
		 * The device 'place' is the first device whose order is
		 * greater than the new child.
		 */
		TAILQ_INSERT_BEFORE(place, child, link);
	} else {
		/*
		 * The new child's order is greater or equal to the order of
		 * any existing device. Add the child to the tail of the list.
		 */
		TAILQ_INSERT_TAIL(&dev->children, child, link);
	}

	bus_data_generation_update();
	return (child);
}

/**
 * @brief Delete a device
 *
 * This function deletes a device along with all of its children. If
 * the device currently has a driver attached to it, the device is
 * detached first using device_detach().
 * 
 * @param dev		the parent device
 * @param child		the device to delete
 *
 * @retval 0		success
 * @retval non-zero	a unit error code describing the error
 */
int
device_delete_child(device_t dev, device_t child)
{
	int error;
	device_t grandchild;

	PDEBUG(("%s from %s", DEVICENAME(child), DEVICENAME(dev)));

	/* remove children first */
	while ( (grandchild = TAILQ_FIRST(&child->children)) ) {
		error = device_delete_child(child, grandchild);
		if (error)
			return (error);
	}

	if ((error = device_detach(child)) != 0)
		return (error);
	if (child->devclass)
		devclass_delete_device(child->devclass, child);
	TAILQ_REMOVE(&dev->children, child, link);
	TAILQ_REMOVE(&bus_data_devices, child, devlink);
	kobj_delete((kobj_t) child, M_BUS);

	bus_data_generation_update();
	return (0);
}

/**
 * @brief Find a device given a unit number
 *
 * This is similar to devclass_get_devices() but only searches for
 * devices which have @p dev as a parent.
 *
 * @param dev		the parent device to search
 * @param unit		the unit number to search for.  If the unit is -1,
 *			return the first child of @p dev which has name
 *			@p classname (that is, the one with the lowest unit.)
 *
 * @returns		the device with the given unit number or @c
 *			NULL if there is no such device
 */
device_t
device_find_child(device_t dev, const char *classname, int unit)
{
	devclass_t dc;
	device_t child;

	dc = devclass_find(classname);
	if (!dc)
		return (NULL);

	if (unit != -1) {
		child = devclass_get_device(dc, unit);
		if (child && child->parent == dev)
			return (child);
	} else {
		for (unit = 0; unit < devclass_get_maxunit(dc); unit++) {
			child = devclass_get_device(dc, unit);
			if (child && child->parent == dev)
				return (child);
		}
	}
	return (NULL);
}

/**
 * @internal
 */
static driverlink_t
first_matching_driver(devclass_t dc, device_t dev)
{
	if (dev->devclass)
		return (devclass_find_driver_internal(dc, dev->devclass->name));
	return (TAILQ_FIRST(&dc->drivers));
}

/**
 * @internal
 */
static driverlink_t
next_matching_driver(devclass_t dc, device_t dev, driverlink_t last)
{
	if (dev->devclass) {
		driverlink_t dl;
		for (dl = TAILQ_NEXT(last, link); dl; dl = TAILQ_NEXT(dl, link))
			if (!strcmp(dev->devclass->name, dl->driver->name))
				return (dl);
		return (NULL);
	}
	return (TAILQ_NEXT(last, link));
}

/**
 * @internal
 */
static int
device_probe_child(device_t dev, device_t child)
{
	devclass_t dc;
	driverlink_t best = 0;
	driverlink_t dl;
	int result, pri = 0;
	int hasclass = (child->devclass != 0);

	GIANT_REQUIRED;

	dc = dev->devclass;
	if (!dc)
		panic("device_probe_child: parent device has no devclass");

	/*
	 * If the state is already probed, then return.  However, don't
	 * return if we can rebid this object.
	 */
	if (child->state == DS_ALIVE && (child->flags & DF_REBID) == 0)
		return (0);

	for (; dc; dc = dc->parent) {
		for (dl = first_matching_driver(dc, child);
		     dl;
		     dl = next_matching_driver(dc, child, dl)) {
			PDEBUG(("Trying %s", DRIVERNAME(dl->driver)));
			device_set_driver(child, dl->driver);
			if (!hasclass)
				device_set_devclass(child, dl->driver->name);

			/* Fetch any flags for the device before probing. */
			resource_int_value(dl->driver->name, child->unit,
			    "flags", &child->devflags);

			result = DEVICE_PROBE(child);

			/* Reset flags and devclass before the next probe. */
			child->devflags = 0;
			if (!hasclass)
				device_set_devclass(child, 0);

			/*
			 * If the driver returns SUCCESS, there can be
			 * no higher match for this device.
			 */
			if (result == 0) {
				best = dl;
				pri = 0;
				break;
			}

			/*
			 * The driver returned an error so it
			 * certainly doesn't match.
			 */
			if (result > 0) {
				device_set_driver(child, 0);
				continue;
			}

			/*
			 * A priority lower than SUCCESS, remember the
			 * best matching driver. Initialise the value
			 * of pri for the first match.
			 */
			if (best == 0 || result > pri) {
				best = dl;
				pri = result;
				continue;
			}
		}
		/*
		 * If we have an unambiguous match in this devclass,
		 * don't look in the parent.
		 */
		if (best && pri == 0)
			break;
	}

	/*
	 * If we found a driver, change state and initialise the devclass.
	 */
	/* XXX What happens if we rebid and got no best? */
	if (best) {
		/*
		 * If this device was atached, and we were asked to
		 * rescan, and it is a different driver, then we have
		 * to detach the old driver and reattach this new one.
		 * Note, we don't have to check for DF_REBID here
		 * because if the state is > DS_ALIVE, we know it must
		 * be.
		 *
		 * This assumes that all DF_REBID drivers can have
		 * their probe routine called at any time and that
		 * they are idempotent as well as completely benign in
		 * normal operations.
		 *
		 * We also have to make sure that the detach
		 * succeeded, otherwise we fail the operation (or
		 * maybe it should just fail silently?  I'm torn).
		 */
		if (child->state > DS_ALIVE && best->driver != child->driver)
			if ((result = device_detach(dev)) != 0)
				return (result);

		/* Set the winning driver, devclass, and flags. */
		if (!child->devclass)
			device_set_devclass(child, best->driver->name);
		device_set_driver(child, best->driver);
		resource_int_value(best->driver->name, child->unit,
		    "flags", &child->devflags);

		if (pri < 0) {
			/*
			 * A bit bogus. Call the probe method again to make
			 * sure that we have the right description.
			 */
			DEVICE_PROBE(child);
#if 0
			child->flags |= DF_REBID;
#endif
		} else
			child->flags &= ~DF_REBID;
		child->state = DS_ALIVE;

		bus_data_generation_update();
		return (0);
	}

	return (ENXIO);
}

/**
 * @brief Return the parent of a device
 */
device_t
device_get_parent(device_t dev)
{
	return (dev->parent);
}

/**
 * @brief Get a list of children of a device
 *
 * An array containing a list of all the children of the given device
 * is allocated and returned in @p *devlistp. The number of devices
 * in the array is returned in @p *devcountp. The caller should free
 * the array using @c free(p, M_TEMP).
 *
 * @param dev		the device to examine
 * @param devlistp	points at location for array pointer return
 *			value
 * @param devcountp	points at location for array size return value
 *
 * @retval 0		success
 * @retval ENOMEM	the array allocation failed
 */
int
device_get_children(device_t dev, device_t **devlistp, int *devcountp)
{
	int count;
	device_t child;
	device_t *list;

	count = 0;
	TAILQ_FOREACH(child, &dev->children, link) {
		count++;
	}

	list = malloc(count * sizeof(device_t), M_TEMP, M_NOWAIT|M_ZERO);
	if (!list)
		return (ENOMEM);

	count = 0;
	TAILQ_FOREACH(child, &dev->children, link) {
		list[count] = child;
		count++;
	}

	*devlistp = list;
	*devcountp = count;

	return (0);
}

/**
 * @brief Return the current driver for the device or @c NULL if there
 * is no driver currently attached
 */
driver_t *
device_get_driver(device_t dev)
{
	return (dev->driver);
}

/**
 * @brief Return the current devclass for the device or @c NULL if
 * there is none.
 */
devclass_t
device_get_devclass(device_t dev)
{
	return (dev->devclass);
}

/**
 * @brief Return the name of the device's devclass or @c NULL if there
 * is none.
 */
const char *
device_get_name(device_t dev)
{
	if (dev != NULL && dev->devclass)
		return (devclass_get_name(dev->devclass));
	return (NULL);
}

/**
 * @brief Return a string containing the device's devclass name
 * followed by an ascii representation of the device's unit number
 * (e.g. @c "foo2").
 */
const char *
device_get_nameunit(device_t dev)
{
	return (dev->nameunit);
}

/**
 * @brief Return the device's unit number.
 */
int
device_get_unit(device_t dev)
{
	return (dev->unit);
}

/**
 * @brief Return the device's description string
 */
const char *
device_get_desc(device_t dev)
{
	return (dev->desc);
}

/**
 * @brief Return the device's flags
 */
u_int32_t
device_get_flags(device_t dev)
{
	return (dev->devflags);
}

struct sysctl_ctx_list *
device_get_sysctl_ctx(device_t dev)
{
	return (&dev->sysctl_ctx);
}

struct sysctl_oid *
device_get_sysctl_tree(device_t dev)
{
	return (dev->sysctl_tree);
}

/**
 * @brief Print the name of the device followed by a colon and a space
 *
 * @returns the number of characters printed
 */
int
device_print_prettyname(device_t dev)
{
	const char *name = device_get_name(dev);

	if (name == 0)
		return (printf("unknown: "));
	return (printf("%s%d: ", name, device_get_unit(dev)));
}

/**
 * @brief Print the name of the device followed by a colon, a space
 * and the result of calling vprintf() with the value of @p fmt and
 * the following arguments.
 *
 * @returns the number of characters printed
 */
int
device_printf(device_t dev, const char * fmt, ...)
{
	va_list ap;
	int retval;

	retval = device_print_prettyname(dev);
	va_start(ap, fmt);
	retval += vprintf(fmt, ap);
	va_end(ap);
	return (retval);
}

/**
 * @internal
 */
static void
device_set_desc_internal(device_t dev, const char* desc, int copy)
{
	if (dev->desc && (dev->flags & DF_DESCMALLOCED)) {
		free(dev->desc, M_BUS);
		dev->flags &= ~DF_DESCMALLOCED;
		dev->desc = NULL;
	}

	if (copy && desc) {
		dev->desc = malloc(strlen(desc) + 1, M_BUS, M_NOWAIT);
		if (dev->desc) {
			strcpy(dev->desc, desc);
			dev->flags |= DF_DESCMALLOCED;
		}
	} else {
		/* Avoid a -Wcast-qual warning */
		dev->desc = (char *)(uintptr_t) desc;
	}

	bus_data_generation_update();
}

/**
 * @brief Set the device's description
 *
 * The value of @c desc should be a string constant that will not
 * change (at least until the description is changed in a subsequent
 * call to device_set_desc() or device_set_desc_copy()).
 */
void
device_set_desc(device_t dev, const char* desc)
{
	device_set_desc_internal(dev, desc, FALSE);
}

/**
 * @brief Set the device's description
 *
 * The string pointed to by @c desc is copied. Use this function if
 * the device description is generated, (e.g. with sprintf()).
 */
void
device_set_desc_copy(device_t dev, const char* desc)
{
	device_set_desc_internal(dev, desc, TRUE);
}

/**
 * @brief Set the device's flags
 */
void
device_set_flags(device_t dev, u_int32_t flags)
{
	dev->devflags = flags;
}

/**
 * @brief Return the device's softc field
 *
 * The softc is allocated and zeroed when a driver is attached, based
 * on the size field of the driver.
 */
void *
device_get_softc(device_t dev)
{
	return (dev->softc);
}

/**
 * @brief Set the device's softc field
 *
 * Most drivers do not need to use this since the softc is allocated
 * automatically when the driver is attached.
 */
void
device_set_softc(device_t dev, void *softc)
{
	if (dev->softc && !(dev->flags & DF_EXTERNALSOFTC))
		free(dev->softc, M_BUS_SC);
	dev->softc = softc;
	if (dev->softc)
		dev->flags |= DF_EXTERNALSOFTC;
	else
		dev->flags &= ~DF_EXTERNALSOFTC;
}

/**
 * @brief Get the device's ivars field
 *
 * The ivars field is used by the parent device to store per-device
 * state (e.g. the physical location of the device or a list of
 * resources).
 */
void *
device_get_ivars(device_t dev)
{

	KASSERT(dev != NULL, ("device_get_ivars(NULL, ...)"));
	return (dev->ivars);
}

/**
 * @brief Set the device's ivars field
 */
void
device_set_ivars(device_t dev, void * ivars)
{

	KASSERT(dev != NULL, ("device_set_ivars(NULL, ...)"));
	dev->ivars = ivars;
}

/**
 * @brief Return the device's state
 */
device_state_t
device_get_state(device_t dev)
{
	return (dev->state);
}

/**
 * @brief Set the DF_ENABLED flag for the device
 */
void
device_enable(device_t dev)
{
	dev->flags |= DF_ENABLED;
}

/**
 * @brief Clear the DF_ENABLED flag for the device
 */
void
device_disable(device_t dev)
{
	dev->flags &= ~DF_ENABLED;
}

/**
 * @brief Increment the busy counter for the device
 */
void
device_busy(device_t dev)
{
	if (dev->state < DS_ATTACHED)
		panic("device_busy: called for unattached device");
	if (dev->busy == 0 && dev->parent)
		device_busy(dev->parent);
	dev->busy++;
	dev->state = DS_BUSY;
}

/**
 * @brief Decrement the busy counter for the device
 */
void
device_unbusy(device_t dev)
{
	if (dev->state != DS_BUSY)
		panic("device_unbusy: called for non-busy device %s",
		    device_get_nameunit(dev));
	dev->busy--;
	if (dev->busy == 0) {
		if (dev->parent)
			device_unbusy(dev->parent);
		dev->state = DS_ATTACHED;
	}
}

/**
 * @brief Set the DF_QUIET flag for the device
 */
void
device_quiet(device_t dev)
{
	dev->flags |= DF_QUIET;
}

/**
 * @brief Clear the DF_QUIET flag for the device
 */
void
device_verbose(device_t dev)
{
	dev->flags &= ~DF_QUIET;
}

/**
 * @brief Return non-zero if the DF_QUIET flag is set on the device
 */
int
device_is_quiet(device_t dev)
{
	return ((dev->flags & DF_QUIET) != 0);
}

/**
 * @brief Return non-zero if the DF_ENABLED flag is set on the device
 */
int
device_is_enabled(device_t dev)
{
	return ((dev->flags & DF_ENABLED) != 0);
}

/**
 * @brief Return non-zero if the device was successfully probed
 */
int
device_is_alive(device_t dev)
{
	return (dev->state >= DS_ALIVE);
}

/**
 * @brief Return non-zero if the device currently has a driver
 * attached to it
 */
int
device_is_attached(device_t dev)
{
	return (dev->state >= DS_ATTACHED);
}

/**
 * @brief Set the devclass of a device
 * @see devclass_add_device().
 */
int
device_set_devclass(device_t dev, const char *classname)
{
	devclass_t dc;
	int error;

	if (!classname) {
		if (dev->devclass)
			devclass_delete_device(dev->devclass, dev);
		return (0);
	}

	if (dev->devclass) {
		printf("device_set_devclass: device class already set\n");
		return (EINVAL);
	}

	dc = devclass_find_internal(classname, 0, TRUE);
	if (!dc)
		return (ENOMEM);

	error = devclass_add_device(dc, dev);

	bus_data_generation_update();
	return (error);
}

/**
 * @brief Set the driver of a device
 *
 * @retval 0		success
 * @retval EBUSY	the device already has a driver attached
 * @retval ENOMEM	a memory allocation failure occurred
 */
int
device_set_driver(device_t dev, driver_t *driver)
{
	if (dev->state >= DS_ATTACHED)
		return (EBUSY);

	if (dev->driver == driver)
		return (0);

	if (dev->softc && !(dev->flags & DF_EXTERNALSOFTC)) {
		free(dev->softc, M_BUS_SC);
		dev->softc = NULL;
	}
	kobj_delete((kobj_t) dev, 0);
	dev->driver = driver;
	if (driver) {
		kobj_init((kobj_t) dev, (kobj_class_t) driver);
		if (!(dev->flags & DF_EXTERNALSOFTC) && driver->size > 0) {
			dev->softc = malloc(driver->size, M_BUS_SC,
			    M_NOWAIT | M_ZERO);
			if (!dev->softc) {
				kobj_delete((kobj_t) dev, 0);
				kobj_init((kobj_t) dev, &null_class);
				dev->driver = NULL;
				return (ENOMEM);
			}
		}
	} else {
		kobj_init((kobj_t) dev, &null_class);
	}

	bus_data_generation_update();
	return (0);
}

/**
 * @brief Probe a device and attach a driver if possible
 *
 * This function is the core of the device autoconfiguration
 * system. Its purpose is to select a suitable driver for a device and
 * then call that driver to initialise the hardware appropriately. The
 * driver is selected by calling the DEVICE_PROBE() method of a set of
 * candidate drivers and then choosing the driver which returned the
 * best value. This driver is then attached to the device using
 * device_attach().
 *
 * The set of suitable drivers is taken from the list of drivers in
 * the parent device's devclass. If the device was originally created
 * with a specific class name (see device_add_child()), only drivers
 * with that name are probed, otherwise all drivers in the devclass
 * are probed. If no drivers return successful probe values in the
 * parent devclass, the search continues in the parent of that
 * devclass (see devclass_get_parent()) if any.
 *
 * @param dev		the device to initialise
 *
 * @retval 0		success
 * @retval ENXIO	no driver was found
 * @retval ENOMEM	memory allocation failure
 * @retval non-zero	some other unix error code
 */
int
device_probe_and_attach(device_t dev)
{
	int error;

	GIANT_REQUIRED;

	if (dev->state >= DS_ALIVE && (dev->flags & DF_REBID) == 0)
		return (0);

	if (!(dev->flags & DF_ENABLED)) {
		if (bootverbose && device_get_name(dev) != NULL) {
			device_print_prettyname(dev);
			printf("not probed (disabled)\n");
		}
		return (0);
	}
	if ((error = device_probe_child(dev->parent, dev)) != 0) {
		if (!(dev->flags & DF_DONENOMATCH)) {
			BUS_PROBE_NOMATCH(dev->parent, dev);
			devnomatch(dev);
			dev->flags |= DF_DONENOMATCH;
		}
		return (error);
	}
	error = device_attach(dev);

	return (error);
}

/**
 * @brief Attach a device driver to a device
 *
 * This function is a wrapper around the DEVICE_ATTACH() driver
 * method. In addition to calling DEVICE_ATTACH(), it initialises the
 * device's sysctl tree, optionally prints a description of the device
 * and queues a notification event for user-based device management
 * services.
 *
 * Normally this function is only called internally from
 * device_probe_and_attach().
 *
 * @param dev		the device to initialise
 *
 * @retval 0		success
 * @retval ENXIO	no driver was found
 * @retval ENOMEM	memory allocation failure
 * @retval non-zero	some other unix error code
 */
int
device_attach(device_t dev)
{
	int error;

	device_sysctl_init(dev);
	if (!device_is_quiet(dev))
		device_print_child(dev->parent, dev);
	if ((error = DEVICE_ATTACH(dev)) != 0) {
		printf("device_attach: %s%d attach returned %d\n",
		    dev->driver->name, dev->unit, error);
		/* Unset the class; set in device_probe_child */
		if (dev->devclass == 0)
			device_set_devclass(dev, 0);
		device_set_driver(dev, NULL);
		device_sysctl_fini(dev);
		dev->state = DS_NOTPRESENT;
		return (error);
	}
	dev->state = DS_ATTACHED;
	devadded(dev);
	return (0);
}

/**
 * @brief Detach a driver from a device
 *
 * This function is a wrapper around the DEVICE_DETACH() driver
 * method. If the call to DEVICE_DETACH() succeeds, it calls
 * BUS_CHILD_DETACHED() for the parent of @p dev, queues a
 * notification event for user-based device management services and
 * cleans up the device's sysctl tree.
 *
 * @param dev		the device to un-initialise
 *
 * @retval 0		success
 * @retval ENXIO	no driver was found
 * @retval ENOMEM	memory allocation failure
 * @retval non-zero	some other unix error code
 */
int
device_detach(device_t dev)
{
	int error;

	GIANT_REQUIRED;

	PDEBUG(("%s", DEVICENAME(dev)));
	if (dev->state == DS_BUSY)
		return (EBUSY);
	if (dev->state != DS_ATTACHED)
		return (0);

	if ((error = DEVICE_DETACH(dev)) != 0)
		return (error);
	devremoved(dev);
	device_printf(dev, "detached\n");
	if (dev->parent)
		BUS_CHILD_DETACHED(dev->parent, dev);

	if (!(dev->flags & DF_FIXEDCLASS))
		devclass_delete_device(dev->devclass, dev);

	dev->state = DS_NOTPRESENT;
	device_set_driver(dev, NULL);
	device_set_desc(dev, NULL);
	device_sysctl_fini(dev);

	return (0);
}

/**
 * @brief Tells a driver to quiesce itself.
 *
 * This function is a wrapper around the DEVICE_QUIESCE() driver
 * method. If the call to DEVICE_QUIESCE() succeeds.
 *
 * @param dev		the device to quiesce
 *
 * @retval 0		success
 * @retval ENXIO	no driver was found
 * @retval ENOMEM	memory allocation failure
 * @retval non-zero	some other unix error code
 */
int
device_quiesce(device_t dev)
{

	PDEBUG(("%s", DEVICENAME(dev)));
	if (dev->state == DS_BUSY)
		return (EBUSY);
	if (dev->state != DS_ATTACHED)
		return (0);

	return (DEVICE_QUIESCE(dev));
}

/**
 * @brief Notify a device of system shutdown
 *
 * This function calls the DEVICE_SHUTDOWN() driver method if the
 * device currently has an attached driver.
 *
 * @returns the value returned by DEVICE_SHUTDOWN()
 */
int
device_shutdown(device_t dev)
{
	if (dev->state < DS_ATTACHED)
		return (0);
	return (DEVICE_SHUTDOWN(dev));
}

/**
 * @brief Set the unit number of a device
 *
 * This function can be used to override the unit number used for a
 * device (e.g. to wire a device to a pre-configured unit number).
 */
int
device_set_unit(device_t dev, int unit)
{
	devclass_t dc;
	int err;

	dc = device_get_devclass(dev);
	if (unit < dc->maxunit && dc->devices[unit])
		return (EBUSY);
	err = devclass_delete_device(dc, dev);
	if (err)
		return (err);
	dev->unit = unit;
	err = devclass_add_device(dc, dev);
	if (err)
		return (err);

	bus_data_generation_update();
	return (0);
}

/*======================================*/
/*
 * Some useful method implementations to make life easier for bus drivers.
 */

/**
 * @brief Initialise a resource list.
 *
 * @param rl		the resource list to initialise
 */
void
resource_list_init(struct resource_list *rl)
{
	STAILQ_INIT(rl);
}

/**
 * @brief Reclaim memory used by a resource list.
 *
 * This function frees the memory for all resource entries on the list
 * (if any).
 *
 * @param rl		the resource list to free		
 */
void
resource_list_free(struct resource_list *rl)
{
	struct resource_list_entry *rle;

	while ((rle = STAILQ_FIRST(rl)) != NULL) {
		if (rle->res)
			panic("resource_list_free: resource entry is busy");
		STAILQ_REMOVE_HEAD(rl, link);
		free(rle, M_BUS);
	}
}

/**
 * @brief Add a resource entry.
 *
 * This function adds a resource entry using the given @p type, @p
 * start, @p end and @p count values. A rid value is chosen by
 * searching sequentially for the first unused rid starting at zero.
 *
 * @param rl		the resource list to edit
 * @param type		the resource entry type (e.g. SYS_RES_MEMORY)
 * @param start		the start address of the resource
 * @param end		the end address of the resource
 * @param count		XXX end-start+1
 */
int
resource_list_add_next(struct resource_list *rl, int type, u_long start,
    u_long end, u_long count)
{
	int rid;

	rid = 0;
	while (resource_list_find(rl, type, rid) != NULL)
		rid++;
	resource_list_add(rl, type, rid, start, end, count);
	return (rid);
}

/**
 * @brief Add or modify a resource entry.
 *
 * If an existing entry exists with the same type and rid, it will be
 * modified using the given values of @p start, @p end and @p
 * count. If no entry exists, a new one will be created using the
 * given values.  The resource list entry that matches is then returned.
 *
 * @param rl		the resource list to edit
 * @param type		the resource entry type (e.g. SYS_RES_MEMORY)
 * @param rid		the resource identifier
 * @param start		the start address of the resource
 * @param end		the end address of the resource
 * @param count		XXX end-start+1
 */
struct resource_list_entry *
resource_list_add(struct resource_list *rl, int type, int rid,
    u_long start, u_long end, u_long count)
{
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle) {
		rle = malloc(sizeof(struct resource_list_entry), M_BUS,
		    M_NOWAIT);
		if (!rle)
			panic("resource_list_add: can't record entry");
		STAILQ_INSERT_TAIL(rl, rle, link);
		rle->type = type;
		rle->rid = rid;
		rle->res = NULL;
	}

	if (rle->res)
		panic("resource_list_add: resource entry is busy");

	rle->start = start;
	rle->end = end;
	rle->count = count;
	return (rle);
}

/**
 * @brief Find a resource entry by type and rid.
 *
 * @param rl		the resource list to search
 * @param type		the resource entry type (e.g. SYS_RES_MEMORY)
 * @param rid		the resource identifier
 *
 * @returns the resource entry pointer or NULL if there is no such
 * entry.
 */
struct resource_list_entry *
resource_list_find(struct resource_list *rl, int type, int rid)
{
	struct resource_list_entry *rle;

	STAILQ_FOREACH(rle, rl, link) {
		if (rle->type == type && rle->rid == rid)
			return (rle);
	}
	return (NULL);
}

/**
 * @brief Delete a resource entry.
 *
 * @param rl		the resource list to edit
 * @param type		the resource entry type (e.g. SYS_RES_MEMORY)
 * @param rid		the resource identifier
 */
void
resource_list_delete(struct resource_list *rl, int type, int rid)
{
	struct resource_list_entry *rle = resource_list_find(rl, type, rid);

	if (rle) {
		if (rle->res != NULL)
			panic("resource_list_delete: resource has not been released");
		STAILQ_REMOVE(rl, rle, resource_list_entry, link);
		free(rle, M_BUS);
	}
}

/**
 * @brief Helper function for implementing BUS_ALLOC_RESOURCE()
 *
 * Implement BUS_ALLOC_RESOURCE() by looking up a resource from the list
 * and passing the allocation up to the parent of @p bus. This assumes
 * that the first entry of @c device_get_ivars(child) is a struct
 * resource_list. This also handles 'passthrough' allocations where a
 * child is a remote descendant of bus by passing the allocation up to
 * the parent of bus.
 *
 * Typically, a bus driver would store a list of child resources
 * somewhere in the child device's ivars (see device_get_ivars()) and
 * its implementation of BUS_ALLOC_RESOURCE() would find that list and
 * then call resource_list_alloc() to perform the allocation.
 *
 * @param rl		the resource list to allocate from
 * @param bus		the parent device of @p child
 * @param child		the device which is requesting an allocation
 * @param type		the type of resource to allocate
 * @param rid		a pointer to the resource identifier
 * @param start		hint at the start of the resource range - pass
 *			@c 0UL for any start address
 * @param end		hint at the end of the resource range - pass
 *			@c ~0UL for any end address
 * @param count		hint at the size of range required - pass @c 1
 *			for any size
 * @param flags		any extra flags to control the resource
 *			allocation - see @c RF_XXX flags in
 *			<sys/rman.h> for details
 * 
 * @returns		the resource which was allocated or @c NULL if no
 *			resource could be allocated
 */
struct resource *
resource_list_alloc(struct resource_list *rl, device_t bus, device_t child,
    int type, int *rid, u_long start, u_long end, u_long count, u_int flags)
{
	struct resource_list_entry *rle = 0;
	int passthrough = (device_get_parent(child) != bus);
	int isdefault = (start == 0UL && end == ~0UL);

	if (passthrough) {
		return (BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
		    type, rid, start, end, count, flags));
	}

	rle = resource_list_find(rl, type, *rid);

	if (!rle)
		return (NULL);		/* no resource of that type/rid */

	if (rle->res)
		panic("resource_list_alloc: resource entry is busy");

	if (isdefault) {
		start = rle->start;
		count = ulmax(count, rle->count);
		end = ulmax(rle->end, start + count - 1);
	}

	rle->res = BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
	    type, rid, start, end, count, flags);

	/*
	 * Record the new range.
	 */
	if (rle->res) {
		rle->start = rman_get_start(rle->res);
		rle->end = rman_get_end(rle->res);
		rle->count = count;
	}

	return (rle->res);
}

/**
 * @brief Helper function for implementing BUS_RELEASE_RESOURCE()
 * 
 * Implement BUS_RELEASE_RESOURCE() using a resource list. Normally
 * used with resource_list_alloc().
 * 
 * @param rl		the resource list which was allocated from
 * @param bus		the parent device of @p child
 * @param child		the device which is requesting a release
 * @param type		the type of resource to allocate
 * @param rid		the resource identifier
 * @param res		the resource to release
 * 
 * @retval 0		success
 * @retval non-zero	a standard unix error code indicating what
 *			error condition prevented the operation
 */
int
resource_list_release(struct resource_list *rl, device_t bus, device_t child,
    int type, int rid, struct resource *res)
{
	struct resource_list_entry *rle = 0;
	int passthrough = (device_get_parent(child) != bus);
	int error;

	if (passthrough) {
		return (BUS_RELEASE_RESOURCE(device_get_parent(bus), child,
		    type, rid, res));
	}

	rle = resource_list_find(rl, type, rid);

	if (!rle)
		panic("resource_list_release: can't find resource");
	if (!rle->res)
		panic("resource_list_release: resource entry is not busy");

	error = BUS_RELEASE_RESOURCE(device_get_parent(bus), child,
	    type, rid, res);
	if (error)
		return (error);

	rle->res = NULL;
	return (0);
}

/**
 * @brief Print a description of resources in a resource list
 *
 * Print all resources of a specified type, for use in BUS_PRINT_CHILD().
 * The name is printed if at least one resource of the given type is available.
 * The format is used to print resource start and end.
 *
 * @param rl		the resource list to print
 * @param name		the name of @p type, e.g. @c "memory"
 * @param type		type type of resource entry to print
 * @param format	printf(9) format string to print resource
 *			start and end values
 * 
 * @returns		the number of characters printed
 */
int
resource_list_print_type(struct resource_list *rl, const char *name, int type,
    const char *format)
{
	struct resource_list_entry *rle;
	int printed, retval;

	printed = 0;
	retval = 0;
	/* Yes, this is kinda cheating */
	STAILQ_FOREACH(rle, rl, link) {
		if (rle->type == type) {
			if (printed == 0)
				retval += printf(" %s ", name);
			else
				retval += printf(",");
			printed++;
			retval += printf(format, rle->start);
			if (rle->count > 1) {
				retval += printf("-");
				retval += printf(format, rle->start +
						 rle->count - 1);
			}
		}
	}
	return (retval);
}

/**
 * @brief Releases all the resources in a list.
 *
 * @param rl		The resource list to purge.
 * 
 * @returns		nothing
 */
void
resource_list_purge(struct resource_list *rl)
{
	struct resource_list_entry *rle;

	STAILQ_FOREACH(rle, rl, link) {
		if (rle->res)
			bus_release_resource(rman_get_device(rle->res),
			    rle->type, rle->rid, rle->res);
		STAILQ_REMOVE_HEAD(rl, link);
		free(rle, M_BUS);
	}
}

/**
 * @brief Helper function for implementing DEVICE_PROBE()
 *
 * This function can be used to help implement the DEVICE_PROBE() for
 * a bus (i.e. a device which has other devices attached to it). It
 * calls the DEVICE_IDENTIFY() method of each driver in the device's
 * devclass.
 */
int
bus_generic_probe(device_t dev)
{
	devclass_t dc = dev->devclass;
	driverlink_t dl;

	TAILQ_FOREACH(dl, &dc->drivers, link) {
		DEVICE_IDENTIFY(dl->driver, dev);
	}

	return (0);
}

/**
 * @brief Helper function for implementing DEVICE_ATTACH()
 *
 * This function can be used to help implement the DEVICE_ATTACH() for
 * a bus. It calls device_probe_and_attach() for each of the device's
 * children.
 */
int
bus_generic_attach(device_t dev)
{
	device_t child;

	TAILQ_FOREACH(child, &dev->children, link) {
		device_probe_and_attach(child);
	}

	return (0);
}

/**
 * @brief Helper function for implementing DEVICE_DETACH()
 *
 * This function can be used to help implement the DEVICE_DETACH() for
 * a bus. It calls device_detach() for each of the device's
 * children.
 */
int
bus_generic_detach(device_t dev)
{
	device_t child;
	int error;

	if (dev->state != DS_ATTACHED)
		return (EBUSY);

	TAILQ_FOREACH(child, &dev->children, link) {
		if ((error = device_detach(child)) != 0)
			return (error);
	}

	return (0);
}

/**
 * @brief Helper function for implementing DEVICE_SHUTDOWN()
 *
 * This function can be used to help implement the DEVICE_SHUTDOWN()
 * for a bus. It calls device_shutdown() for each of the device's
 * children.
 */
int
bus_generic_shutdown(device_t dev)
{
	device_t child;

	TAILQ_FOREACH(child, &dev->children, link) {
		device_shutdown(child);
	}

	return (0);
}

/**
 * @brief Helper function for implementing DEVICE_SUSPEND()
 *
 * This function can be used to help implement the DEVICE_SUSPEND()
 * for a bus. It calls DEVICE_SUSPEND() for each of the device's
 * children. If any call to DEVICE_SUSPEND() fails, the suspend
 * operation is aborted and any devices which were suspended are
 * resumed immediately by calling their DEVICE_RESUME() methods.
 */
int
bus_generic_suspend(device_t dev)
{
	int		error;
	device_t	child, child2;

	TAILQ_FOREACH(child, &dev->children, link) {
		error = DEVICE_SUSPEND(child);
		if (error) {
			for (child2 = TAILQ_FIRST(&dev->children);
			     child2 && child2 != child;
			     child2 = TAILQ_NEXT(child2, link))
				DEVICE_RESUME(child2);
			return (error);
		}
	}
	return (0);
}

/**
 * @brief Helper function for implementing DEVICE_RESUME()
 *
 * This function can be used to help implement the DEVICE_RESUME() for
 * a bus. It calls DEVICE_RESUME() on each of the device's children.
 */
int
bus_generic_resume(device_t dev)
{
	device_t	child;

	TAILQ_FOREACH(child, &dev->children, link) {
		DEVICE_RESUME(child);
		/* if resume fails, there's nothing we can usefully do... */
	}
	return (0);
}

/**
 * @brief Helper function for implementing BUS_PRINT_CHILD().
 *
 * This function prints the first part of the ascii representation of
 * @p child, including its name, unit and description (if any - see
 * device_set_desc()).
 *
 * @returns the number of characters printed
 */
int
bus_print_child_header(device_t dev, device_t child)
{
	int	retval = 0;

	if (device_get_desc(child)) {
		retval += device_printf(child, "<%s>", device_get_desc(child));
	} else {
		retval += printf("%s", device_get_nameunit(child));
	}

	return (retval);
}

/**
 * @brief Helper function for implementing BUS_PRINT_CHILD().
 *
 * This function prints the last part of the ascii representation of
 * @p child, which consists of the string @c " on " followed by the
 * name and unit of the @p dev.
 *
 * @returns the number of characters printed
 */
int
bus_print_child_footer(device_t dev, device_t child)
{
	return (printf(" on %s\n", device_get_nameunit(dev)));
}

/**
 * @brief Helper function for implementing BUS_PRINT_CHILD().
 *
 * This function simply calls bus_print_child_header() followed by
 * bus_print_child_footer().
 *
 * @returns the number of characters printed
 */
int
bus_generic_print_child(device_t dev, device_t child)
{
	int	retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

/**
 * @brief Stub function for implementing BUS_READ_IVAR().
 * 
 * @returns ENOENT
 */
int
bus_generic_read_ivar(device_t dev, device_t child, int index,
    uintptr_t * result)
{
	return (ENOENT);
}

/**
 * @brief Stub function for implementing BUS_WRITE_IVAR().
 * 
 * @returns ENOENT
 */
int
bus_generic_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value)
{
	return (ENOENT);
}

/**
 * @brief Stub function for implementing BUS_GET_RESOURCE_LIST().
 * 
 * @returns NULL
 */
struct resource_list *
bus_generic_get_resource_list(device_t dev, device_t child)
{
	return (NULL);
}

/**
 * @brief Helper function for implementing BUS_DRIVER_ADDED().
 *
 * This implementation of BUS_DRIVER_ADDED() simply calls the driver's
 * DEVICE_IDENTIFY() method to allow it to add new children to the bus
 * and then calls device_probe_and_attach() for each unattached child.
 */
void
bus_generic_driver_added(device_t dev, driver_t *driver)
{
	device_t child;

	DEVICE_IDENTIFY(driver, dev);
	TAILQ_FOREACH(child, &dev->children, link) {
		if (child->state == DS_NOTPRESENT ||
		    (child->flags & DF_REBID))
			device_probe_and_attach(child);
	}
}

/**
 * @brief Helper function for implementing BUS_SETUP_INTR().
 *
 * This simple implementation of BUS_SETUP_INTR() simply calls the
 * BUS_SETUP_INTR() method of the parent of @p dev.
 */
int
bus_generic_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_SETUP_INTR(dev->parent, child, irq, flags,
		    intr, arg, cookiep));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_TEARDOWN_INTR().
 *
 * This simple implementation of BUS_TEARDOWN_INTR() simply calls the
 * BUS_TEARDOWN_INTR() method of the parent of @p dev.
 */
int
bus_generic_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_TEARDOWN_INTR(dev->parent, child, irq, cookie));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_ALLOC_RESOURCE().
 *
 * This simple implementation of BUS_ALLOC_RESOURCE() simply calls the
 * BUS_ALLOC_RESOURCE() method of the parent of @p dev.
 */
struct resource *
bus_generic_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_ALLOC_RESOURCE(dev->parent, child, type, rid,
		    start, end, count, flags));
	return (NULL);
}

/**
 * @brief Helper function for implementing BUS_RELEASE_RESOURCE().
 *
 * This simple implementation of BUS_RELEASE_RESOURCE() simply calls the
 * BUS_RELEASE_RESOURCE() method of the parent of @p dev.
 */
int
bus_generic_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_RELEASE_RESOURCE(dev->parent, child, type, rid,
		    r));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_ACTIVATE_RESOURCE().
 *
 * This simple implementation of BUS_ACTIVATE_RESOURCE() simply calls the
 * BUS_ACTIVATE_RESOURCE() method of the parent of @p dev.
 */
int
bus_generic_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_ACTIVATE_RESOURCE(dev->parent, child, type, rid,
		    r));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_DEACTIVATE_RESOURCE().
 *
 * This simple implementation of BUS_DEACTIVATE_RESOURCE() simply calls the
 * BUS_DEACTIVATE_RESOURCE() method of the parent of @p dev.
 */
int
bus_generic_deactivate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_DEACTIVATE_RESOURCE(dev->parent, child, type, rid,
		    r));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_CONFIG_INTR().
 *
 * This simple implementation of BUS_CONFIG_INTR() simply calls the
 * BUS_CONFIG_INTR() method of the parent of @p dev.
 */
int
bus_generic_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{

	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return (BUS_CONFIG_INTR(dev->parent, irq, trig, pol));
	return (EINVAL);
}

/**
 * @brief Helper function for implementing BUS_GET_RESOURCE().
 *
 * This implementation of BUS_GET_RESOURCE() uses the
 * resource_list_find() function to do most of the work. It calls
 * BUS_GET_RESOURCE_LIST() to find a suitable resource list to
 * search.
 */
int
bus_generic_rl_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct resource_list *		rl = NULL;
	struct resource_list_entry *	rle = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return (EINVAL);

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return (ENOENT);

	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;

	return (0);
}

/**
 * @brief Helper function for implementing BUS_SET_RESOURCE().
 *
 * This implementation of BUS_SET_RESOURCE() uses the
 * resource_list_add() function to do most of the work. It calls
 * BUS_GET_RESOURCE_LIST() to find a suitable resource list to
 * edit.
 */
int
bus_generic_rl_set_resource(device_t dev, device_t child, int type, int rid,
    u_long start, u_long count)
{
	struct resource_list *		rl = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return (EINVAL);

	resource_list_add(rl, type, rid, start, (start + count - 1), count);

	return (0);
}

/**
 * @brief Helper function for implementing BUS_DELETE_RESOURCE().
 *
 * This implementation of BUS_DELETE_RESOURCE() uses the
 * resource_list_delete() function to do most of the work. It calls
 * BUS_GET_RESOURCE_LIST() to find a suitable resource list to
 * edit.
 */
void
bus_generic_rl_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct resource_list *		rl = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return;

	resource_list_delete(rl, type, rid);

	return;
}

/**
 * @brief Helper function for implementing BUS_RELEASE_RESOURCE().
 *
 * This implementation of BUS_RELEASE_RESOURCE() uses the
 * resource_list_release() function to do most of the work. It calls
 * BUS_GET_RESOURCE_LIST() to find a suitable resource list.
 */
int
bus_generic_rl_release_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	struct resource_list *		rl = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return (EINVAL);

	return (resource_list_release(rl, dev, child, type, rid, r));
}

/**
 * @brief Helper function for implementing BUS_ALLOC_RESOURCE().
 *
 * This implementation of BUS_ALLOC_RESOURCE() uses the
 * resource_list_alloc() function to do most of the work. It calls
 * BUS_GET_RESOURCE_LIST() to find a suitable resource list.
 */
struct resource *
bus_generic_rl_alloc_resource(device_t dev, device_t child, int type,
    int *rid, u_long start, u_long end, u_long count, u_int flags)
{
	struct resource_list *		rl = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return (NULL);

	return (resource_list_alloc(rl, dev, child, type, rid,
	    start, end, count, flags));
}

/**
 * @brief Helper function for implementing BUS_CHILD_PRESENT().
 *
 * This simple implementation of BUS_CHILD_PRESENT() simply calls the
 * BUS_CHILD_PRESENT() method of the parent of @p dev.
 */
int
bus_generic_child_present(device_t dev, device_t child)
{
	return (BUS_CHILD_PRESENT(device_get_parent(dev), dev));
}

/*
 * Some convenience functions to make it easier for drivers to use the
 * resource-management functions.  All these really do is hide the
 * indirection through the parent's method table, making for slightly
 * less-wordy code.  In the future, it might make sense for this code
 * to maintain some sort of a list of resources allocated by each device.
 */

/**
 * @brief Wrapper function for BUS_ALLOC_RESOURCE().
 *
 * This function simply calls the BUS_ALLOC_RESOURCE() method of the
 * parent of @p dev.
 */
struct resource *
bus_alloc_resource(device_t dev, int type, int *rid, u_long start, u_long end,
    u_long count, u_int flags)
{
	if (dev->parent == 0)
		return (0);
	return (BUS_ALLOC_RESOURCE(dev->parent, dev, type, rid, start, end,
	    count, flags));
}

/**
 * @brief Wrapper function for BUS_ACTIVATE_RESOURCE().
 *
 * This function simply calls the BUS_ACTIVATE_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_activate_resource(device_t dev, int type, int rid, struct resource *r)
{
	if (dev->parent == 0)
		return (EINVAL);
	return (BUS_ACTIVATE_RESOURCE(dev->parent, dev, type, rid, r));
}

/**
 * @brief Wrapper function for BUS_DEACTIVATE_RESOURCE().
 *
 * This function simply calls the BUS_DEACTIVATE_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_deactivate_resource(device_t dev, int type, int rid, struct resource *r)
{
	if (dev->parent == 0)
		return (EINVAL);
	return (BUS_DEACTIVATE_RESOURCE(dev->parent, dev, type, rid, r));
}

/**
 * @brief Wrapper function for BUS_RELEASE_RESOURCE().
 *
 * This function simply calls the BUS_RELEASE_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_release_resource(device_t dev, int type, int rid, struct resource *r)
{
	if (dev->parent == 0)
		return (EINVAL);
	return (BUS_RELEASE_RESOURCE(dev->parent, dev, type, rid, r));
}

/**
 * @brief Wrapper function for BUS_SETUP_INTR().
 *
 * This function simply calls the BUS_SETUP_INTR() method of the
 * parent of @p dev.
 */
int
bus_setup_intr(device_t dev, struct resource *r, int flags,
    driver_intr_t handler, void *arg, void **cookiep)
{
	int error;

	if (dev->parent != 0) {
		if ((flags &~ INTR_ENTROPY) == (INTR_TYPE_NET | INTR_MPSAFE) &&
		    !debug_mpsafenet)
			flags &= ~INTR_MPSAFE;
		error = BUS_SETUP_INTR(dev->parent, dev, r, flags,
		    handler, arg, cookiep);
		if (error == 0) {
			if (!(flags & (INTR_MPSAFE | INTR_FAST)))
				device_printf(dev, "[GIANT-LOCKED]\n");
			if (bootverbose && (flags & INTR_MPSAFE))
				device_printf(dev, "[MPSAFE]\n");
			if (flags & INTR_FAST)
				device_printf(dev, "[FAST]\n");
		}
	} else
		error = EINVAL;
	return (error);
}

/**
 * @brief Wrapper function for BUS_TEARDOWN_INTR().
 *
 * This function simply calls the BUS_TEARDOWN_INTR() method of the
 * parent of @p dev.
 */
int
bus_teardown_intr(device_t dev, struct resource *r, void *cookie)
{
	if (dev->parent == 0)
		return (EINVAL);
	return (BUS_TEARDOWN_INTR(dev->parent, dev, r, cookie));
}

/**
 * @brief Wrapper function for BUS_SET_RESOURCE().
 *
 * This function simply calls the BUS_SET_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_set_resource(device_t dev, int type, int rid,
    u_long start, u_long count)
{
	return (BUS_SET_RESOURCE(device_get_parent(dev), dev, type, rid,
	    start, count));
}

/**
 * @brief Wrapper function for BUS_GET_RESOURCE().
 *
 * This function simply calls the BUS_GET_RESOURCE() method of the
 * parent of @p dev.
 */
int
bus_get_resource(device_t dev, int type, int rid,
    u_long *startp, u_long *countp)
{
	return (BUS_GET_RESOURCE(device_get_parent(dev), dev, type, rid,
	    startp, countp));
}

/**
 * @brief Wrapper function for BUS_GET_RESOURCE().
 *
 * This function simply calls the BUS_GET_RESOURCE() method of the
 * parent of @p dev and returns the start value.
 */
u_long
bus_get_resource_start(device_t dev, int type, int rid)
{
	u_long start, count;
	int error;

	error = BUS_GET_RESOURCE(device_get_parent(dev), dev, type, rid,
	    &start, &count);
	if (error)
		return (0);
	return (start);
}

/**
 * @brief Wrapper function for BUS_GET_RESOURCE().
 *
 * This function simply calls the BUS_GET_RESOURCE() method of the
 * parent of @p dev and returns the count value.
 */
u_long
bus_get_resource_count(device_t dev, int type, int rid)
{
	u_long start, count;
	int error;

	error = BUS_GET_RESOURCE(device_get_parent(dev), dev, type, rid,
	    &start, &count);
	if (error)
		return (0);
	return (count);
}

/**
 * @brief Wrapper function for BUS_DELETE_RESOURCE().
 *
 * This function simply calls the BUS_DELETE_RESOURCE() method of the
 * parent of @p dev.
 */
void
bus_delete_resource(device_t dev, int type, int rid)
{
	BUS_DELETE_RESOURCE(device_get_parent(dev), dev, type, rid);
}

/**
 * @brief Wrapper function for BUS_CHILD_PRESENT().
 *
 * This function simply calls the BUS_CHILD_PRESENT() method of the
 * parent of @p dev.
 */
int
bus_child_present(device_t child)
{
	return (BUS_CHILD_PRESENT(device_get_parent(child), child));
}

/**
 * @brief Wrapper function for BUS_CHILD_PNPINFO_STR().
 *
 * This function simply calls the BUS_CHILD_PNPINFO_STR() method of the
 * parent of @p dev.
 */
int
bus_child_pnpinfo_str(device_t child, char *buf, size_t buflen)
{
	device_t parent;

	parent = device_get_parent(child);
	if (parent == NULL) {
		*buf = '\0';
		return (0);
	}
	return (BUS_CHILD_PNPINFO_STR(parent, child, buf, buflen));
}

/**
 * @brief Wrapper function for BUS_CHILD_LOCATION_STR().
 *
 * This function simply calls the BUS_CHILD_LOCATION_STR() method of the
 * parent of @p dev.
 */
int
bus_child_location_str(device_t child, char *buf, size_t buflen)
{
	device_t parent;

	parent = device_get_parent(child);
	if (parent == NULL) {
		*buf = '\0';
		return (0);
	}
	return (BUS_CHILD_LOCATION_STR(parent, child, buf, buflen));
}

static int
root_print_child(device_t dev, device_t child)
{
	int	retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf("\n");

	return (retval);
}

static int
root_setup_intr(device_t dev, device_t child, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	/*
	 * If an interrupt mapping gets to here something bad has happened.
	 */
	panic("root_setup_intr");
}

/*
 * If we get here, assume that the device is permanant and really is
 * present in the system.  Removable bus drivers are expected to intercept
 * this call long before it gets here.  We return -1 so that drivers that
 * really care can check vs -1 or some ERRNO returned higher in the food
 * chain.
 */
static int
root_child_present(device_t dev, device_t child)
{
	return (-1);
}

static kobj_method_t root_methods[] = {
	/* Device interface */
	KOBJMETHOD(device_shutdown,	bus_generic_shutdown),
	KOBJMETHOD(device_suspend,	bus_generic_suspend),
	KOBJMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	KOBJMETHOD(bus_print_child,	root_print_child),
	KOBJMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	KOBJMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	KOBJMETHOD(bus_setup_intr,	root_setup_intr),
	KOBJMETHOD(bus_child_present,	root_child_present),

	{ 0, 0 }
};

static driver_t root_driver = {
	"root",
	root_methods,
	1,			/* no softc */
};

device_t	root_bus;
devclass_t	root_devclass;

static int
root_bus_module_handler(module_t mod, int what, void* arg)
{
	switch (what) {
	case MOD_LOAD:
		TAILQ_INIT(&bus_data_devices);
		kobj_class_compile((kobj_class_t) &root_driver);
		root_bus = make_device(NULL, "root", 0);
		root_bus->desc = "System root bus";
		kobj_init((kobj_t) root_bus, (kobj_class_t) &root_driver);
		root_bus->driver = &root_driver;
		root_bus->state = DS_ATTACHED;
		root_devclass = devclass_find_internal("root", 0, FALSE);
		devinit();
		return (0);

	case MOD_SHUTDOWN:
		device_shutdown(root_bus);
		return (0);
	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

static moduledata_t root_bus_mod = {
	"rootbus",
	root_bus_module_handler,
	0
};
DECLARE_MODULE(rootbus, root_bus_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);

/**
 * @brief Automatically configure devices
 *
 * This function begins the autoconfiguration process by calling
 * device_probe_and_attach() for each child of the @c root0 device.
 */ 
void
root_bus_configure(void)
{
	device_t dev;

	PDEBUG(("."));

	TAILQ_FOREACH(dev, &root_bus->children, link) {
		device_probe_and_attach(dev);
	}
}

/**
 * @brief Module handler for registering device drivers
 *
 * This module handler is used to automatically register device
 * drivers when modules are loaded. If @p what is MOD_LOAD, it calls
 * devclass_add_driver() for the driver described by the
 * driver_module_data structure pointed to by @p arg
 */
int
driver_module_handler(module_t mod, int what, void *arg)
{
	int error;
	struct driver_module_data *dmd;
	devclass_t bus_devclass;
	kobj_class_t driver;

	dmd = (struct driver_module_data *)arg;
	bus_devclass = devclass_find_internal(dmd->dmd_busname, 0, TRUE);
	error = 0;

	switch (what) {
	case MOD_LOAD:
		if (dmd->dmd_chainevh)
			error = dmd->dmd_chainevh(mod,what,dmd->dmd_chainarg);

		driver = dmd->dmd_driver;
		PDEBUG(("Loading module: driver %s on bus %s",
		    DRIVERNAME(driver), dmd->dmd_busname));
		error = devclass_add_driver(bus_devclass, driver);
		if (error)
			break;

		/*
		 * If the driver has any base classes, make the
		 * devclass inherit from the devclass of the driver's
		 * first base class. This will allow the system to
		 * search for drivers in both devclasses for children
		 * of a device using this driver.
		 */
		if (driver->baseclasses) {
			const char *parentname;
			parentname = driver->baseclasses[0]->name;
			*dmd->dmd_devclass =
				devclass_find_internal(driver->name,
				    parentname, TRUE);
		} else {
			*dmd->dmd_devclass =
				devclass_find_internal(driver->name, 0, TRUE);
		}
		break;

	case MOD_UNLOAD:
		PDEBUG(("Unloading module: driver %s from bus %s",
		    DRIVERNAME(dmd->dmd_driver),
		    dmd->dmd_busname));
		error = devclass_delete_driver(bus_devclass,
		    dmd->dmd_driver);

		if (!error && dmd->dmd_chainevh)
			error = dmd->dmd_chainevh(mod,what,dmd->dmd_chainarg);
		break;
	case MOD_QUIESCE:
		PDEBUG(("Quiesce module: driver %s from bus %s",
		    DRIVERNAME(dmd->dmd_driver),
		    dmd->dmd_busname));
		error = devclass_quiesce_driver(bus_devclass,
		    dmd->dmd_driver);

		if (!error && dmd->dmd_chainevh)
			error = dmd->dmd_chainevh(mod,what,dmd->dmd_chainarg);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

#ifdef BUS_DEBUG

/* the _short versions avoid iteration by not calling anything that prints
 * more than oneliners. I love oneliners.
 */

static void
print_device_short(device_t dev, int indent)
{
	if (!dev)
		return;

	indentprintf(("device %d: <%s> %sparent,%schildren,%s%s%s%s%s,%sivars,%ssoftc,busy=%d\n",
	    dev->unit, dev->desc,
	    (dev->parent? "":"no "),
	    (TAILQ_EMPTY(&dev->children)? "no ":""),
	    (dev->flags&DF_ENABLED? "enabled,":"disabled,"),
	    (dev->flags&DF_FIXEDCLASS? "fixed,":""),
	    (dev->flags&DF_WILDCARD? "wildcard,":""),
	    (dev->flags&DF_DESCMALLOCED? "descmalloced,":""),
	    (dev->flags&DF_REBID? "rebiddable,":""),
	    (dev->ivars? "":"no "),
	    (dev->softc? "":"no "),
	    dev->busy));
}

static void
print_device(device_t dev, int indent)
{
	if (!dev)
		return;

	print_device_short(dev, indent);

	indentprintf(("Parent:\n"));
	print_device_short(dev->parent, indent+1);
	indentprintf(("Driver:\n"));
	print_driver_short(dev->driver, indent+1);
	indentprintf(("Devclass:\n"));
	print_devclass_short(dev->devclass, indent+1);
}

void
print_device_tree_short(device_t dev, int indent)
/* print the device and all its children (indented) */
{
	device_t child;

	if (!dev)
		return;

	print_device_short(dev, indent);

	TAILQ_FOREACH(child, &dev->children, link) {
		print_device_tree_short(child, indent+1);
	}
}

void
print_device_tree(device_t dev, int indent)
/* print the device and all its children (indented) */
{
	device_t child;

	if (!dev)
		return;

	print_device(dev, indent);

	TAILQ_FOREACH(child, &dev->children, link) {
		print_device_tree(child, indent+1);
	}
}

static void
print_driver_short(driver_t *driver, int indent)
{
	if (!driver)
		return;

	indentprintf(("driver %s: softc size = %zd\n",
	    driver->name, driver->size));
}

static void
print_driver(driver_t *driver, int indent)
{
	if (!driver)
		return;

	print_driver_short(driver, indent);
}


static void
print_driver_list(driver_list_t drivers, int indent)
{
	driverlink_t driver;

	TAILQ_FOREACH(driver, &drivers, link) {
		print_driver(driver->driver, indent);
	}
}

static void
print_devclass_short(devclass_t dc, int indent)
{
	if ( !dc )
		return;

	indentprintf(("devclass %s: max units = %d\n", dc->name, dc->maxunit));
}

static void
print_devclass(devclass_t dc, int indent)
{
	int i;

	if ( !dc )
		return;

	print_devclass_short(dc, indent);
	indentprintf(("Drivers:\n"));
	print_driver_list(dc->drivers, indent+1);

	indentprintf(("Devices:\n"));
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i])
			print_device(dc->devices[i], indent+1);
}

void
print_devclass_list_short(void)
{
	devclass_t dc;

	printf("Short listing of devclasses, drivers & devices:\n");
	TAILQ_FOREACH(dc, &devclasses, link) {
		print_devclass_short(dc, 0);
	}
}

void
print_devclass_list(void)
{
	devclass_t dc;

	printf("Full listing of devclasses, drivers & devices:\n");
	TAILQ_FOREACH(dc, &devclasses, link) {
		print_devclass(dc, 0);
	}
}

#endif

/*
 * User-space access to the device tree.
 *
 * We implement a small set of nodes:
 *
 * hw.bus			Single integer read method to obtain the
 *				current generation count.
 * hw.bus.devices		Reads the entire device tree in flat space.
 * hw.bus.rman			Resource manager interface
 *
 * We might like to add the ability to scan devclasses and/or drivers to
 * determine what else is currently loaded/available.
 */

static int
sysctl_bus(SYSCTL_HANDLER_ARGS)
{
	struct u_businfo	ubus;

	ubus.ub_version = BUS_USER_VERSION;
	ubus.ub_generation = bus_data_generation;

	return (SYSCTL_OUT(req, &ubus, sizeof(ubus)));
}
SYSCTL_NODE(_hw_bus, OID_AUTO, info, CTLFLAG_RW, sysctl_bus,
    "bus-related data");

static int
sysctl_devices(SYSCTL_HANDLER_ARGS)
{
	int			*name = (int *)arg1;
	u_int			namelen = arg2;
	int			index;
	struct device		*dev;
	struct u_device		udev;	/* XXX this is a bit big */
	int			error;

	if (namelen != 2)
		return (EINVAL);

	if (bus_data_generation_check(name[0]))
		return (EINVAL);

	index = name[1];

	/*
	 * Scan the list of devices, looking for the requested index.
	 */
	TAILQ_FOREACH(dev, &bus_data_devices, devlink) {
		if (index-- == 0)
			break;
	}
	if (dev == NULL)
		return (ENOENT);

	/*
	 * Populate the return array.
	 */
	bzero(&udev, sizeof(udev));
	udev.dv_handle = (uintptr_t)dev;
	udev.dv_parent = (uintptr_t)dev->parent;
	if (dev->nameunit == NULL)
		udev.dv_name[0] = '\0';
	else
		strlcpy(udev.dv_name, dev->nameunit, sizeof(udev.dv_name));

	if (dev->desc == NULL)
		udev.dv_desc[0] = '\0';
	else
		strlcpy(udev.dv_desc, dev->desc, sizeof(udev.dv_desc));
	if (dev->driver == NULL || dev->driver->name == NULL)
		udev.dv_drivername[0] = '\0';
	else
		strlcpy(udev.dv_drivername, dev->driver->name,
		    sizeof(udev.dv_drivername));
	udev.dv_pnpinfo[0] = '\0';
	udev.dv_location[0] = '\0';
	bus_child_pnpinfo_str(dev, udev.dv_pnpinfo, sizeof(udev.dv_pnpinfo));
	bus_child_location_str(dev, udev.dv_location, sizeof(udev.dv_location));
	udev.dv_devflags = dev->devflags;
	udev.dv_flags = dev->flags;
	udev.dv_state = dev->state;
	error = SYSCTL_OUT(req, &udev, sizeof(udev));
	return (error);
}

SYSCTL_NODE(_hw_bus, OID_AUTO, devices, CTLFLAG_RD, sysctl_devices,
    "system device tree");

int
bus_data_generation_check(int generation)
{
	if (generation != bus_data_generation)
		return (1);

	/* XXX generate optimised lists here? */
	return (0);
}

void
bus_data_generation_update(void)
{
	bus_data_generation++;
}

int
bus_free_resource(device_t dev, int type, struct resource *r)
{
	if (r == NULL)
		return (0);
	return (bus_release_resource(dev, type, rman_get_rid(r), r));
}
