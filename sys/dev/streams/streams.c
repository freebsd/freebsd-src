/*
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
 * Copyright (c) 1997 Todd Vierling
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
 * 3. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Stolen from NetBSD /sys/compat/svr4/svr4_net.c.  Pseudo-device driver
 * skeleton produced from /usr/share/examples/drivers/make_pseudo_driver.sh
 * in 3.0-980524-SNAP then hacked a bit (but probably not enough :-).
 *
 * $Id$
 */

#include "streams.h"		/* generated file.. defines NSTREAMS */
#include "opt_devfs.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>		/* SYSINIT stuff */
#include <sys/conf.h>		/* cdevsw stuff */
#include <sys/malloc.h>		/* malloc region definitions */
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#include <sys/domain.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/proc.h>
#include <sys/uio.h>
#ifdef DEVFS
#include <sys/devfsext.h>	/* DEVFS defintitions */
#endif /* DEVFS */

#include <sys/sysproto.h>

#include <svr4/svr4_types.h>
#include <svr4/svr4_util.h>
#include <svr4/svr4_signal.h>
#include <svr4/svr4_ioctl.h>
#include <svr4/svr4_stropts.h>
#include <svr4/svr4_socket.h>

static int svr4_soo_close __P((struct file *, struct proc *));
static int svr4_ptm_alloc __P((struct proc *));
static  d_open_t	streamsopen;

struct svr4_sockcache_entry {
	struct proc *p;		/* Process for the socket		*/
	void *cookie;		/* Internal cookie used for matching	*/
	struct sockaddr_un sock;/* Pathname for the socket		*/
	dev_t dev;		/* Device where the socket lives on	*/
	ino_t ino;		/* Inode where the socket lives on	*/
	TAILQ_ENTRY(svr4_sockcache_entry) entries;
};

TAILQ_HEAD(svr4_sockcache_head, svr4_sockcache_entry) svr4_head;

/* Initialization flag (set/queried by svr4_mod LKM) */
int svr4_str_initialized = 0;

/*
 * Device minor numbers
 */
enum {
	dev_ptm			= 10,
	dev_arp			= 26,
	dev_icmp		= 27,
	dev_ip			= 28,
	dev_tcp			= 35,
	dev_udp			= 36,
	dev_rawip		= 37,
	dev_unix_dgram		= 38,
	dev_unix_stream		= 39,
	dev_unix_ord_stream	= 40
};

static struct fileops svr4_netops = {
	soo_read, soo_write, soo_ioctl, soo_poll, svr4_soo_close
};
 
#define CDEV_MAJOR 103
static struct cdevsw streams_cdevsw = {
	/* open */	streamsopen,
	/* close */	noclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	noioctl,
	/* stop */	nostop,
	/* reset */	noreset,
	/* devtotty */	nodevtotty,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"streams",
	/* parms */	noparms,
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* maxio */	0,
	/* bmaj */	-1
};
 
struct streams_softc {
	struct isa_device *dev;
#ifdef DEVFS
  /*
   * If this ever becomes an LKM we'll want this crud so we can deallocate
   * devfs entries when the module is unloaded
   */
	void *devfs_ptm;
        void *devfs_arp;
        void *devfs_icmp;
        void *devfs_ip;
        void *devfs_tcp;
        void *devfs_udp;
        void *devfs_rawip;
        void *devfs_unix_dgram;
        void *devfs_unix_stream;
        void *devfs_unix_ord_stream;
#endif
} ;

#define UNIT(dev) minor(dev)	/* assume one minor number per unit */

typedef	struct streams_softc *sc_p;

static sc_p sca[NSTREAMS];

static	int
streams_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		cdevsw_add(&streams_cdevsw);
		return 0;
	case MOD_UNLOAD:
		return 0;
	default:
		break;
	}
	return 0;
}

static moduledata_t streams_mod = {
	"streams",
	streams_modevent,
	0
};
DECLARE_MODULE(streams, streams_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);

/*
 * We only need open() and close() routines.  open() calls socreate()
 * to allocate a "real" object behind the stream and mallocs some state
 * info for use by the svr4 emulator;  close() deallocates the state
 * information and passes the underlying object to the normal socket close
 * routine.
 */
static  int
streamsopen(dev_t dev, int oflags, int devtype, struct proc *p)
{
	int type, protocol;
	int fd;
	struct file *fp;
	struct socket *so;
	int error;
	int family;
	
	if (p->p_dupfd >= 0)
	  return ENODEV;

	switch (minor(dev)) {
	case dev_udp:
	  family = AF_INET;
	  type = SOCK_DGRAM;
	  protocol = IPPROTO_UDP;
	  break;

	case dev_tcp:
	  family = AF_INET;
	  type = SOCK_STREAM;
	  protocol = IPPROTO_TCP;
	  break;

	case dev_ip:
	case dev_rawip:
	  family = AF_INET;
	  type = SOCK_RAW;
	  protocol = IPPROTO_IP;
	  break;

	case dev_icmp:
	  family = AF_INET;
	  type = SOCK_RAW;
	  protocol = IPPROTO_ICMP;
	  break;

	case dev_unix_dgram:
	  family = AF_LOCAL;
	  type = SOCK_DGRAM;
	  protocol = 0;
	  break;

	case dev_unix_stream:
	case dev_unix_ord_stream:
	  family = AF_LOCAL;
	  type = SOCK_STREAM;
	  protocol = 0;
	  break;

	case dev_ptm:
	  return svr4_ptm_alloc(p);

	default:
	  return EOPNOTSUPP;
	}

	if ((error = falloc(p, &fp, &fd)) != 0)
	  return error;

	if ((error = socreate(family, &so, type, protocol, p)) != 0) {
	  p->p_fd->fd_ofiles[fd] = 0;
	  ffree(fp);
	  return error;
	}

	fp->f_flag = FREAD|FWRITE;
	fp->f_type = DTYPE_SOCKET;
	fp->f_ops = &svr4_netops;

	fp->f_data = (caddr_t)so;
	(void)svr4_stream_get(fp);
	p->p_dupfd = fd;
	return ENXIO;
}

static int
svr4_ptm_alloc(p)
	struct proc *p;
{
	/*
	 * XXX this is very, very ugly.  But I can't find a better
	 * way that won't duplicate a big amount of code from
	 * sys_open().  Ho hum...
	 *
	 * Fortunately for us, Solaris (at least 2.5.1) makes the
	 * /dev/ptmx open automatically just open a pty, that (after
	 * STREAMS I_PUSHes), is just a plain pty.  fstat() is used
	 * to get the minor device number to map to a tty.
	 * 
	 * Cycle through the names. If sys_open() returns ENOENT (or
	 * ENXIO), short circuit the cycle and exit.
	 */
	static char ptyname[] = "/dev/ptyXX";
	static char ttyletters[] = "pqrstuwxyzPQRST";
	static char ttynumbers[] = "0123456789abcdef";
	caddr_t sg = stackgap_init();
	char *path = stackgap_alloc(&sg, sizeof(ptyname));
	struct open_args oa;
	int l = 0, n = 0;
	register_t fd = -1;
	int error;

	SCARG(&oa, path) = path;
	SCARG(&oa, flags) = O_RDWR;
	SCARG(&oa, mode) = 0;

	while (fd == -1) {
		ptyname[8] = ttyletters[l];
		ptyname[9] = ttynumbers[n];

		if ((error = copyout(ptyname, path, sizeof(ptyname))) != 0)
			return error;

		switch (error = open(p, &oa)) {
		case ENOENT:
		case ENXIO:
			return error;
		case 0:
			p->p_dupfd = p->p_retval[0];
			return ENXIO;
		default:
			if (ttynumbers[++n] == '\0') {
				if (ttyletters[++l] == '\0')
					break;
				n = 0;
			}
		}
	}
	return ENOENT;
}


struct svr4_strm *
svr4_stream_get(fp)
	struct file *fp;
{
	struct socket *so;
	struct svr4_strm *st;

	if (fp == NULL || fp->f_type != DTYPE_SOCKET)
		return NULL;

	so = (struct socket *) fp->f_data;

       	if (so->so_emuldata)
		return so->so_emuldata;

	/* Allocate a new one. */
	fp->f_ops = &svr4_netops;
	st = malloc(sizeof(struct svr4_strm), M_TEMP, M_WAITOK);
	st->s_family = so->so_proto->pr_domain->dom_family;
	st->s_cmd = ~0;
	st->s_afd = -1;
	st->s_eventmask = 0;
	so->so_emuldata = st;

	return st;
}

void
svr4_delete_socket(p, fp)
	struct proc *p;
	struct file *fp;
{
	struct svr4_sockcache_entry *e;
	void *cookie = ((struct socket *) fp->f_data)->so_emuldata;

	if (!svr4_str_initialized) {
		TAILQ_INIT(&svr4_head);
		svr4_str_initialized = 1;
		return;
	}

	for (e = svr4_head.tqh_first; e != NULL; e = e->entries.tqe_next)
		if (e->p == p && e->cookie == cookie) {
			TAILQ_REMOVE(&svr4_head, e, entries);
			DPRINTF(("svr4_delete_socket: %s [%p,%d,%d]\n",
				 e->sock.sun_path, p, e->dev, e->ino));
			free(e, M_TEMP);
			return;
		}
}

static int
svr4_soo_close(struct file *fp, struct proc *p)
{
        struct socket *so = (struct socket *)fp->f_data;
	
	/*	CHECKUNIT_DIAG(ENXIO);*/

	svr4_delete_socket(p, fp);
	free(so->so_emuldata, M_TEMP);
	return soo_close(fp, p);
	return (0);
}

/*
 * Now  for some driver initialisation.
 * Occurs ONCE during boot (very early).
 */
static void             
streams_drvinit(void *unused)
{
	int	unit;
	sc_p scp  = sca[unit];

	for (unit = 0; unit < NSTREAMS; unit++) {
		/* 
		 * Allocate storage for this instance .
		 */
		scp = malloc(sizeof(*scp), M_DEVBUF, M_NOWAIT);
		if( scp == NULL) {
			printf("streams%d failed to allocate strorage\n", unit);
			return ;
		}
		bzero(scp, sizeof(*scp));
		sca[unit] = scp;
#if DEVFS
		/* XXX - This stuff is all completely bogus -- It's supposed
		 * to show up in /compat/svr4/dev, but devfs will be mounted
		 * on /dev, won't it?  Sigh.  CHECKALTEXIST() will mean 
		 * device opens will still work (and it will mitigate the
		 * need to run SVR4_MAKEDEV in /compat/svr4/dev, or will
		 * replace it with a script which creates symlinks to entities
		 * in /dev, or something equally 'orrible), but it's
		 * still a botch to put emulator-specific devices in the
		 * "global" part of the filesystem tree (especially scumsucking
		 * devices like these).  Good thing hardly anyone uses 
		 * devfs, right?
		 */
    		scp->devfs_ptm = devfs_add_devswf(&streams_cdevsw, dev_ptm, DV_CHR,
	    		UID_ROOT, GID_KMEM, 0640, "ptmx%d", unit);
    		scp->devfs_arp = devfs_add_devswf(&streams_cdevsw, dev_arp, DV_CHR,
	    		UID_ROOT, GID_KMEM, 0666, "arp%d", unit);
    		scp->devfs_icmp = devfs_add_devswf(&streams_cdevsw, dev_icmp, DV_CHR,
	    		UID_ROOT, GID_KMEM, 0600, "icmp%d", unit);
    		scp->devfs_ip = devfs_add_devswf(&streams_cdevsw, dev_ip, DV_CHR,
	    		UID_ROOT, GID_KMEM, 0600, "ip%d", unit);
    		scp->devfs_tcp = devfs_add_devswf(&streams_cdevsw, dev_tcp, DV_CHR,
	    		UID_ROOT, GID_KMEM, 0666, "tcp%d", unit);
    		scp->devfs_udp = devfs_add_devswf(&streams_cdevsw, dev_udp, DV_CHR,
	    		UID_ROOT, GID_KMEM, 0666, "udp%d", unit);
    		scp->devfs_rawip = devfs_add_devswf(&streams_cdevsw, dev_rawip, DV_CHR,
	    		UID_ROOT, GID_KMEM, 0600, "rawip%d", unit);
    		scp->devfs_unix_dgram = devfs_add_devswf(&streams_cdevsw, dev_unix_dgram, DV_CHR,
	    		UID_ROOT, GID_KMEM, 0666, "ticlts%d", unit);
    		scp->devfs_unix_stream = devfs_add_devswf(&streams_cdevsw, dev_unix_stream, DV_CHR,
	    		UID_ROOT, GID_KMEM, 0666, "ticots%d", unit);
    		scp->devfs_unix_ord_stream = devfs_add_devswf(&streams_cdevsw, dev_unix_ord_stream, DV_CHR,
	    		UID_ROOT, GID_KMEM, 0666, "ticotsord%d", unit);
#endif
	}
}

SYSINIT(streamsdev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+CDEV_MAJOR,
		streams_drvinit, NULL)


