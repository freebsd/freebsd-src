/* $FreeBSD$ */
/* $Id: nfs4_dev.c,v 1.10 2003/11/05 14:58:59 rees Exp $ */

/*-
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/poll.h>
#include <sys/mutex.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/wait.h>
#include <sys/signalvar.h>

#include <nfs4client/nfs4_dev.h>

#ifdef NFS4DEVVERBOSE
#define NFS4DEV_DEBUG(X...) printf(X)
#else
#define NFS4DEV_DEBUG(X...)
#endif

#define NFS4DEV_NAME "nfs4"
#define CDEV_MINOR 1

MALLOC_DEFINE(M_NFS4DEV, "NFS4 dev", "NFS4 device");

struct nfs4dev_upcall {
  	/* request msg */
	struct nfs4dev_msg up_reqmsg;
  	size_t up_reqmsglen;

	/* reply (payload only) */
	caddr_t up_rep;
	size_t * up_replen;	

	int up_copied;		/* non-zero when reply has been copied to 
				   '*up_rep' */
				   
	int up_error;		/* non-zero if an error occured */

	TAILQ_ENTRY(nfs4dev_upcall) up_entry;
};


#define nfs4dev_upcall_get(MP) MALLOC((MP), struct nfs4dev_upcall *, sizeof(struct nfs4dev_upcall), M_NFS4DEV, M_WAITOK | M_ZERO)

#define nfs4dev_upcall_put(MP) FREE((MP), M_NFS4DEV)

static int nfs4dev_nopen = 0;
static struct thread * nfs4dev_reader = NULL;
static struct cdev *nfs4device = 0;
static struct mtx nfs4dev_daemon_mtx;

static int nfs4dev_xid = 0;
/* queue of pending upcalls */
TAILQ_HEAD(, nfs4dev_upcall) nfs4dev_newq;
static struct mtx nfs4dev_newq_mtx;

/* queue of upcalls waiting for replys */
TAILQ_HEAD(, nfs4dev_upcall) nfs4dev_waitq;
static struct mtx nfs4dev_waitq_mtx;

/* dev hooks */
static d_open_t  nfs4dev_open;
static d_close_t nfs4dev_close;
static d_ioctl_t nfs4dev_ioctl;
static d_poll_t  nfs4dev_poll;

static struct cdevsw nfs4dev_cdevsw = {
#if (__FreeBSD_version > 502102)
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
#endif
	.d_open =	nfs4dev_open,
	.d_close =	nfs4dev_close,
	.d_ioctl =	nfs4dev_ioctl,
	.d_poll =	nfs4dev_poll,
	.d_name =	NFS4DEV_NAME,
};

static int nfs4dev_reply(caddr_t);
static int nfs4dev_request(caddr_t);

/* Userland requests a new operation to service */
static int 
nfs4dev_request(caddr_t addr)
{
	struct nfs4dev_upcall * u;
	struct nfs4dev_msg * m = (struct nfs4dev_msg *) addr;

	mtx_lock(&nfs4dev_newq_mtx);

	if (TAILQ_EMPTY(&nfs4dev_newq)) {
  		mtx_unlock(&nfs4dev_newq_mtx);
		return EAGAIN;	
	}

	u = TAILQ_FIRST(&nfs4dev_newq);
	TAILQ_REMOVE(&nfs4dev_newq, u, up_entry);
	mtx_unlock(&nfs4dev_newq_mtx);

	bcopy(&u->up_reqmsg, m, sizeof(struct nfs4dev_msg));

	mtx_lock(&nfs4dev_waitq_mtx);
	TAILQ_INSERT_TAIL(&nfs4dev_waitq, u, up_entry);
	mtx_unlock(&nfs4dev_waitq_mtx);

	return 0;
}

static int
nfs4dev_reply(caddr_t addr)
{
	struct nfs4dev_upcall * u;
	struct nfs4dev_msg * m = (struct nfs4dev_msg *) addr;
	int error;

	if (m->msg_vers != NFS4DEV_VERSION) {
	  	printf("nfs4dev version mismatch\n");
		return EINVAL;
	}

	if (m->msg_type > NFS4DEV_MAX_TYPE) {
		NFS4DEV_DEBUG("nfs4dev: unsupported message type\n");
		return EINVAL;
	}

	if (m->msg_len == 0 || m->msg_len > NFS4DEV_MSG_MAX_DATALEN) {
	  	NFS4DEV_DEBUG("bad message length\n");
		return EINVAL;
	}
	  	
	/* match the reply with a request */
	mtx_lock(&nfs4dev_waitq_mtx);
	TAILQ_FOREACH(u, &nfs4dev_waitq, up_entry) {
		if (m->msg_xid == u->up_reqmsg.msg_xid) {
			if (m->msg_type == u->up_reqmsg.msg_type)
		  		goto found;
			NFS4DEV_DEBUG("nfs4dev: op type mismatch!\n");
			break;
		}
	}
	mtx_unlock(&nfs4dev_waitq_mtx);

	NFS4DEV_DEBUG("nfs4dev msg op: %d xid: %x not found.\n",
	    m->msg_type, m->msg_xid);

	error = EIO;
	goto bad;

found:
	TAILQ_REMOVE(&nfs4dev_waitq, u, up_entry);
	mtx_unlock(&nfs4dev_waitq_mtx);

	if (m->msg_error) {
		error = m->msg_error;
		goto bad;
	}

	if (m->msg_len > *u->up_replen) {
		error = EFAULT;
		goto bad;
	}

	bcopy(m->msg_data, u->up_rep, m->msg_len);
	*u->up_replen = m->msg_len;

	u->up_copied = m->msg_len;
	wakeup(u);

	return 0;
bad:
	u->up_error = error;
	wakeup(u);
	return error;
}

void
nfs4dev_init(void)
{
  	nfs4dev_xid = arc4random();
	TAILQ_INIT(&nfs4dev_newq);	
	TAILQ_INIT(&nfs4dev_waitq);	
	mtx_init(&nfs4dev_newq_mtx, "nfs4dev newq", NULL, MTX_DEF);
	mtx_init(&nfs4dev_waitq_mtx, "nfs4dev waitq", NULL, MTX_DEF);

	mtx_init(&nfs4dev_daemon_mtx, "nfs4dev state", NULL, MTX_DEF);

	nfs4device = make_dev(&nfs4dev_cdevsw, CDEV_MINOR, (uid_t)0, (gid_t)0,
	    S_IRUSR | S_IWUSR, "nfs4");
}

void
nfs4dev_uninit(void)
{
  	struct proc * dead = NULL;

  	mtx_lock(&nfs4dev_daemon_mtx);
  	if (nfs4dev_nopen) {
		if (nfs4dev_reader == NULL) {
			NFS4DEV_DEBUG("nfs4dev uninit(): unregistered reader\n");
		} else {
			dead = nfs4dev_reader->td_proc;
		}
	}
  	mtx_unlock(&nfs4dev_daemon_mtx);

	if (dead != NULL) {
		NFS4DEV_DEBUG("nfs4dev_uninit(): you forgot to kill attached daemon (pid: %u)\n",
		    dead->p_pid);
		PROC_LOCK(dead);
		psignal(dead, SIGTERM);
		PROC_UNLOCK(dead);
	}

	/* XXX moot? */
	nfs4dev_purge();

  	mtx_destroy(&nfs4dev_newq_mtx);
  	mtx_destroy(&nfs4dev_waitq_mtx);
  	mtx_destroy(&nfs4dev_daemon_mtx);

	destroy_dev(nfs4device);
}

/* device interface functions */
static int
nfs4dev_open(struct cdev *dev, int flags, int fmt, d_thread_t *td)
{
	if (dev != nfs4device) 
		return ENODEV;

  	mtx_lock(&nfs4dev_daemon_mtx);
	if (nfs4dev_nopen) {
  		mtx_unlock(&nfs4dev_daemon_mtx);
	  	return EBUSY;
	}

	nfs4dev_nopen++;
	nfs4dev_reader = curthread;
  	mtx_unlock(&nfs4dev_daemon_mtx);

	return (0);
}

static int
nfs4dev_close(struct cdev *dev, int flags, int fmt, d_thread_t *td)
{
	struct nfs4dev_upcall * u;

	if (dev != nfs4device) 
		return ENODEV;

  	mtx_lock(&nfs4dev_daemon_mtx);
	if (!nfs4dev_nopen) {
  		mtx_unlock(&nfs4dev_daemon_mtx);
	  	return ENOENT;
	}

	nfs4dev_nopen--; 
	nfs4dev_reader = NULL;
  	mtx_unlock(&nfs4dev_daemon_mtx);

	mtx_lock(&nfs4dev_waitq_mtx);

	while (!TAILQ_EMPTY(&nfs4dev_waitq)) {
		u = TAILQ_FIRST(&nfs4dev_waitq);
		TAILQ_REMOVE(&nfs4dev_waitq, u, up_entry);
		u->up_error = EINTR;
		wakeup(u);
	}

	mtx_unlock(&nfs4dev_waitq_mtx);

	return 0;
}

static int 
nfs4dev_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
  	int error;

	if (dev != nfs4device)
	 	return ENODEV; 

	if (data == NULL) 
	  	return EFAULT;

	if (nfs4dev_reader != curthread)
		nfs4dev_reader = curthread;

	switch (cmd) {
	case NFS4DEVIOCGET:
		error = nfs4dev_request(data);
	break;
	case NFS4DEVIOCPUT:
		error = nfs4dev_reply(data);
	break;
	default:
		NFS4DEV_DEBUG("nfs4dev_ioctl: unkown ioctl cmd %d\n", (int)cmd);
		error = EOPNOTSUPP;
	break;
	}

	return error;
}

static int 
nfs4dev_poll(struct cdev *dev, int events, struct thread *td)
{
  	int revents;

	if (dev != nfs4device) 
	  return EINVAL;

	mtx_lock(&nfs4dev_daemon_mtx);
	if (nfs4dev_nopen == 0) {
		mtx_unlock(&nfs4dev_daemon_mtx);
		return 0;
	}
	mtx_unlock(&nfs4dev_daemon_mtx);

	revents = 0;

	/* check readable data */
	mtx_lock(&nfs4dev_newq_mtx);
	  if (!TAILQ_EMPTY(&nfs4dev_newq))
	    revents |= POLLIN;
	mtx_unlock(&nfs4dev_newq_mtx);

	mtx_lock(&nfs4dev_waitq_mtx);
	  if (!TAILQ_EMPTY(&nfs4dev_waitq))
	    revents |= POLLOUT;
	mtx_unlock(&nfs4dev_waitq_mtx);

	return revents;
}

int 
nfs4dev_call(uint32_t type, caddr_t req_data, size_t req_len, caddr_t rep_data, size_t * rep_lenp)
{
  	struct nfs4dev_upcall * u;
	int error = 0; 
	unsigned int xtmp;

	mtx_lock(&nfs4dev_daemon_mtx);
	if (nfs4dev_nopen == 0) {
		mtx_unlock(&nfs4dev_daemon_mtx);
		return EINVAL;
	}
	mtx_unlock(&nfs4dev_daemon_mtx);

	if (type > NFS4DEV_MAX_TYPE)
	  return EOPNOTSUPP;

	NFS4DEV_DEBUG("upcall %d/%d:%d\n", type, req_len, *rep_lenp);

	nfs4dev_upcall_get(u);

	u->up_error = 0;
	u->up_rep = rep_data;
	u->up_replen = rep_lenp;
	u->up_copied = 0;

	u->up_reqmsg.msg_vers = NFS4DEV_VERSION;
	/* XXX efficient copying */
	bcopy(req_data, u->up_reqmsg.msg_data, req_len);
	u->up_reqmsg.msg_len  = req_len;

	mtx_lock(&nfs4dev_newq_mtx);

	/* get new XID */
	while ((xtmp = arc4random() % 256) == 0);
	nfs4dev_xid += xtmp;
	u->up_reqmsg.msg_xid = nfs4dev_xid;

	TAILQ_INSERT_TAIL(&nfs4dev_newq, u, up_entry);
	mtx_unlock(&nfs4dev_newq_mtx);


	NFS4DEV_DEBUG("nfs4dev op: %d xid: %x sleeping\n", u->up_reqmsg.msg_type, u->up_reqmsg.msg_xid);

	do {
		tsleep(u, PLOCK, "nfs4dev", 0);
	} while (u->up_copied == 0 && u->up_error == 0);

	/* upcall now removed from the queue */

	NFS4DEV_DEBUG("nfs4dev prog: %d xid: %x continues...\n", 
	    u->up_reqmsg.msg_type, u->up_reqmsg.msg_xid);

	if (u->up_error) {
		error = u->up_error; 
		NFS4DEV_DEBUG("nfs4dev prog: %d xid: %x error: %d\n", 
	    	    u->up_reqmsg.msg_type, u->up_reqmsg.msg_xid, u->up_error);
		goto out;
	}

out:
	nfs4dev_upcall_put(u);
	return error;
}

void
nfs4dev_purge(void)
{
  	struct nfs4dev_upcall * u;

	mtx_lock(&nfs4dev_newq_mtx);
	while (!TAILQ_EMPTY(&nfs4dev_newq)) {
		u = TAILQ_FIRST(&nfs4dev_newq);
		TAILQ_REMOVE(&nfs4dev_newq, u, up_entry);
		u->up_error = EINTR;
		wakeup(u);
	}
	mtx_unlock(&nfs4dev_newq_mtx);

	mtx_lock(&nfs4dev_waitq_mtx);
	while (!TAILQ_EMPTY(&nfs4dev_waitq)) {
		u = TAILQ_FIRST(&nfs4dev_waitq);
		TAILQ_REMOVE(&nfs4dev_waitq, u, up_entry);
		u->up_error = EINTR;
		wakeup(u);
	}
	mtx_unlock(&nfs4dev_waitq_mtx);
}
