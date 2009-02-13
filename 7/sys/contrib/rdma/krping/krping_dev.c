/*
 * This code lifted from: 
 * 	Simple `echo' pseudo-device KLD
 * 	Murray Stokely
 * 	Converted to 5.X by Søren (Xride) Straarup
 */

/*
 * /bin/echo "server,port=9999,addr=192.168.69.142,validate" > /dev/krping  
 * /bin/echo "client,port=9999,addr=192.168.69.142,validate" > /dev/krping  
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/module.h>
#include <sys/systm.h>  /* uprintf */
#include <sys/errno.h>
#include <sys/param.h>  /* defines used in kernel.h */
#include <sys/kernel.h> /* types used in module initialization */
#include <sys/conf.h>   /* cdevsw struct */
#include <sys/uio.h>    /* uio struct */
#include <sys/malloc.h>

#include "krping.h"

#define BUFFERSIZE 512

/* Function prototypes */
static d_open_t      krping_open;
static d_close_t     krping_close;
static d_read_t      krping_read;
static d_write_t     krping_write;

/* Character device entry points */
static struct cdevsw krping_cdevsw = {
	.d_version = D_VERSION,
	.d_open = krping_open,
	.d_close = krping_close,
	.d_read = krping_read,
	.d_write = krping_write,
	.d_name = "krping",
};

typedef struct s_krping {
	char msg[BUFFERSIZE];
	int len;
} krping_t;

/* vars */
static struct cdev *krping_dev;

static int
krping_loader(struct module *m, int what, void *arg)
{
	int err = 0;

	switch (what) {
	case MOD_LOAD:                /* kldload */
		krping_init();
		krping_dev = make_dev(&krping_cdevsw, 0, UID_ROOT, GID_WHEEL, 
					0600, "krping");
		printf("Krping device loaded.\n");
		break;
	case MOD_UNLOAD:
		destroy_dev(krping_dev);
		printf("Krping device unloaded.\n");
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}
	return err;
}

static int
krping_open(struct cdev *dev, int oflags, int devtype, struct thread *p)
{
	int err = 0;
	return err;
}

static int
krping_close(struct cdev *dev, int fflag, int devtype, struct thread *p)
{
	return 0;
}

static int
krping_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct krping_cb *cb, *cb2;
	int num=1;
	struct krping_cb_list copy_cbs;

	uprintf("krping: %4s %10s %10s %10s %10s %10s %10s %10s %10s %10s\n",
		"num", "device", "snd bytes", "snd msgs", "rcv bytes", 
		"rcv msgs", "wr bytes", "wr msgs", "rd bytes", "rd msgs");
	TAILQ_INIT(&copy_cbs);

	mtx_lock(&krping_mutex);
	TAILQ_FOREACH(cb, &krping_cbs, list) {
		cb2 = malloc(sizeof(*cb), M_DEVBUF, M_NOWAIT|M_ZERO);
		if (!cb2)
			break;
		bcopy(cb, cb2, sizeof(*cb));
		TAILQ_INSERT_TAIL(&copy_cbs, cb2, list);
	}
	mtx_unlock(&krping_mutex);

	while (!TAILQ_EMPTY(&copy_cbs)) {
		
		cb = TAILQ_FIRST(&copy_cbs);
		TAILQ_REMOVE(&copy_cbs, cb, list);
		if (cb->pd) {
			uprintf("krping: %4d %10s %10u %10u %10u %10u %10u %10u %10u %10u\n",
			     num++, cb->pd->device->name, cb->stats.send_bytes,
			     cb->stats.send_msgs, cb->stats.recv_bytes,
			     cb->stats.recv_msgs, cb->stats.write_bytes,
			     cb->stats.write_msgs,
			     cb->stats.read_bytes,
			     cb->stats.read_msgs);
		} else {
			uprintf("krping: %d listen\n", num++);
		}
		free(cb, M_DEVBUF);
	}
	return 0;
}

static int
krping_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	int err = 0;
	int amt;
	int remain = BUFFERSIZE;
	char *cp;
	krping_t *krpingmsg;

	krpingmsg = malloc(sizeof *krpingmsg, M_DEVBUF, M_WAITOK|M_ZERO);
	if (!krpingmsg) {
		uprintf("Could not malloc mem!\n");
		return ENOMEM;
	}

	cp = krpingmsg->msg;
	while (uio->uio_resid) {
		amt = MIN(uio->uio_resid, remain);
		if (amt == 0)
			break;

		/* Copy the string in from user memory to kernel memory */
		err = uiomove(cp, amt, uio);
		if (err) {
			uprintf("Write failed: bad address!\n");
			return err;
		}
		cp += amt;
		remain -= amt;
	}

	if (uio->uio_resid != 0) {
		uprintf("Message too big. max size is %d!\n", BUFFERSIZE);
		return EMSGSIZE;
	}

	/* null terminate and remove the \n */
	cp--;
	*cp = 0;
	krpingmsg->len = (unsigned long)(cp - krpingmsg->msg);
	uprintf("krping: write string = |%s|\n", krpingmsg->msg);
	err = krping_doit(krpingmsg->msg);
	free(krpingmsg, M_DEVBUF);
	return(err);
}

MODULE_DEPEND(krping, rdma_core, 1, 1, 1);
MODULE_DEPEND(krping, rdma_cma, 1, 1, 1);
DEV_MODULE(krping,krping_loader,NULL);
