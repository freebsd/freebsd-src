/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <sys/syscallsubr.h>
#include <sys/un.h>
#include <sys/domain.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <sys/sysproto.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_ioctl.h>
#include <compat/svr4/svr4_stropts.h>
#include <compat/svr4/svr4_socket.h>

static int svr4_soo_close(struct file *, struct thread *);
static int svr4_ptm_alloc(struct thread *);
static  d_open_t	streamsopen;

struct svr4_sockcache_head svr4_head;

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

static struct cdev *dt_ptm, *dt_arp, *dt_icmp, *dt_ip, *dt_tcp, *dt_udp, *dt_rawip,
	*dt_unix_dgram, *dt_unix_stream, *dt_unix_ord_stream;

static struct fileops svr4_netops = {
	.fo_read = soo_read,
	.fo_write = soo_write,
	.fo_ioctl = soo_ioctl,
	.fo_poll = soo_poll,
	.fo_kqfilter = soo_kqfilter,
	.fo_stat = soo_stat,
	.fo_close =  svr4_soo_close
};
 
static struct cdevsw streams_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	streamsopen,
	.d_name =	"streams",
};
 
struct streams_softc {
	struct isa_device *dev;
} ;

#define UNIT(dev) minor(dev)	/* assume one minor number per unit */

typedef	struct streams_softc *sc_p;

static	int
streams_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		/* XXX should make sure it isn't already loaded first */
		dt_ptm = make_dev(&streams_cdevsw, dev_ptm, 0, 0, 0666,
			"ptm");
		dt_arp = make_dev(&streams_cdevsw, dev_arp, 0, 0, 0666,
			"arp");
		dt_icmp = make_dev(&streams_cdevsw, dev_icmp, 0, 0, 0666,
			"icmp");
		dt_ip = make_dev(&streams_cdevsw, dev_ip, 0, 0, 0666,
			"ip");
		dt_tcp = make_dev(&streams_cdevsw, dev_tcp, 0, 0, 0666,
			"tcp");
		dt_udp = make_dev(&streams_cdevsw, dev_udp, 0, 0, 0666,
			"udp");
		dt_rawip = make_dev(&streams_cdevsw, dev_rawip, 0, 0, 0666,
			"rawip");
		dt_unix_dgram = make_dev(&streams_cdevsw, dev_unix_dgram,
			0, 0, 0666, "ticlts");
		dt_unix_stream = make_dev(&streams_cdevsw, dev_unix_stream,
			0, 0, 0666, "ticots");
		dt_unix_ord_stream = make_dev(&streams_cdevsw,
			dev_unix_ord_stream, 0, 0, 0666, "ticotsord");

		if (! (dt_ptm && dt_arp && dt_icmp && dt_ip && dt_tcp &&
				dt_udp && dt_rawip && dt_unix_dgram &&
				dt_unix_stream && dt_unix_ord_stream)) {
			printf("WARNING: device config for STREAMS failed\n");
			printf("Suggest unloading streams KLD\n");
		}
		return 0;
	case MOD_UNLOAD:
	  	/* XXX should check to see if it's busy first */
		destroy_dev(dt_ptm);
		destroy_dev(dt_arp);
		destroy_dev(dt_icmp);
		destroy_dev(dt_ip);
		destroy_dev(dt_tcp);
		destroy_dev(dt_udp);
		destroy_dev(dt_rawip);
		destroy_dev(dt_unix_dgram);
		destroy_dev(dt_unix_stream);
		destroy_dev(dt_unix_ord_stream);

		return 0;
	default:
		return EOPNOTSUPP;
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
MODULE_VERSION(streams, 1);

/*
 * We only need open() and close() routines.  open() calls socreate()
 * to allocate a "real" object behind the stream and mallocs some state
 * info for use by the svr4 emulator;  close() deallocates the state
 * information and passes the underlying object to the normal socket close
 * routine.
 */
static  int
streamsopen(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	int type, protocol;
	int fd, extraref;
	struct file *fp;
	struct socket *so;
	int error;
	int family;
	struct proc *p = td->td_proc;
	
	PROC_LOCK(p);
	if (td->td_dupfd >= 0) {
	  PROC_UNLOCK(p);
	  return ENODEV;
	}
	PROC_UNLOCK(p);

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
	  return svr4_ptm_alloc(td);

	default:
	  return EOPNOTSUPP;
	}

	if ((error = falloc(td, &fp, &fd)) != 0)
	  return error;
	/* An extra reference on `fp' has been held for us by falloc(). */

	if ((error = socreate(family, &so, type, protocol,
	    td->td_ucred, td)) != 0) {
	  FILEDESC_LOCK_FAST(p->p_fd);
	  /* Check the fd table entry hasn't changed since we made it. */
	  extraref = 0;
	  if (p->p_fd->fd_ofiles[fd] == fp) {
	    p->p_fd->fd_ofiles[fd] = NULL;
	    extraref = 1;
	  }
	  FILEDESC_UNLOCK_FAST(p->p_fd);
	  if (extraref)
	    fdrop(fp, td);
	  fdrop(fp, td);
	  return error;
	}

	FILEDESC_LOCK_FAST(p->p_fd);
	fp->f_data = so;
	fp->f_flag = FREAD|FWRITE;
	fp->f_ops = &svr4_netops;
	fp->f_type = DTYPE_SOCKET;
	FILEDESC_UNLOCK_FAST(p->p_fd);

	(void)svr4_stream_get(fp);
	fdrop(fp, td);
	PROC_LOCK(p);
	td->td_dupfd = fd;
	PROC_UNLOCK(p);
	return ENXIO;
}

static int
svr4_ptm_alloc(td)
	struct thread *td;
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
	struct proc *p;
	register_t fd;
	int error, l, n;

	fd = -1;
	n = 0;
	l = 0;
	p = td->td_proc;
	while (fd == -1) {
		ptyname[8] = ttyletters[l];
		ptyname[9] = ttynumbers[n];

		error = kern_open(td, ptyname, UIO_SYSSPACE, O_RDWR, 0);
		switch (error) {
		case ENOENT:
		case ENXIO:
			return error;
		case 0:
			PROC_LOCK(p);
			td->td_dupfd = td->td_retval[0];
			PROC_UNLOCK(p);
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

	so = fp->f_data;

	/*
	 * mpfixme: lock socketbuffer here
	 */
	if (so->so_emuldata) {
		return so->so_emuldata;
	}

	/* Allocate a new one. */
	st = malloc(sizeof(struct svr4_strm), M_TEMP, M_WAITOK);
	st->s_family = so->so_proto->pr_domain->dom_family;
	st->s_cmd = ~0;
	st->s_afd = -1;
	st->s_eventmask = 0;
	/*
	 * avoid a race where we loose due to concurrancy issues
	 * of two threads trying to allocate the so_emuldata.
	 */
	if (so->so_emuldata) {
		/* lost the race, use the existing emuldata */
		FREE(st, M_TEMP);
		st = so->so_emuldata;
	} else {
		/* we won, or there was no race, use our copy */
		so->so_emuldata = st;
		fp->f_ops = &svr4_netops;
	}

	return st;
}

void
svr4_delete_socket(p, fp)
	struct proc *p;
	struct file *fp;
{
	struct svr4_sockcache_entry *e;
	void *cookie = ((struct socket *)fp->f_data)->so_emuldata;

	while (svr4_str_initialized != 2) {
		if (atomic_cmpset_acq_int(&svr4_str_initialized, 0, 1)) {
			TAILQ_INIT(&svr4_head);
			atomic_store_rel_int(&svr4_str_initialized, 2);
		}
		return;
	}

	TAILQ_FOREACH(e, &svr4_head, entries)
		if (e->p == p && e->cookie == cookie) {
			TAILQ_REMOVE(&svr4_head, e, entries);
			DPRINTF(("svr4_delete_socket: %s [%p,%d,%d]\n",
				 e->sock.sun_path, p, (int)e->dev, e->ino));
			free(e, M_TEMP);
			return;
		}
}

static int
svr4_soo_close(struct file *fp, struct thread *td)
{
        struct socket *so = fp->f_data;
	
	/*	CHECKUNIT_DIAG(ENXIO);*/

	svr4_delete_socket(td->td_proc, fp);
	free(so->so_emuldata, M_TEMP);
	return soo_close(fp, td);
}
