/*
 * Copyright (c) 2000-2001 Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* Must come after sys/malloc.h */
#include <sys/filedesc.h>
#include <sys/mbuf.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <net/if.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>

#define SMB_GETDEV(dev)		((struct smb_dev*)(dev)->si_drv1)
#define	SMB_CHECKMINOR(dev)	do { \
				    sdp = SMB_GETDEV(dev); \
				    if (sdp == NULL) return ENXIO; \
				} while(0)

static d_open_t	 nsmb_dev_open;
static d_close_t nsmb_dev_close;
static d_read_t	 nsmb_dev_read;
static d_write_t nsmb_dev_write;
static d_ioctl_t nsmb_dev_ioctl;
static d_poll_t	 nsmb_dev_poll;

MODULE_DEPEND(netsmb, libiconv, 1, 1, 1);
MODULE_VERSION(netsmb, NSMB_VERSION);

static int smb_version = NSMB_VERSION;


SYSCTL_DECL(_net_smb);
SYSCTL_INT(_net_smb, OID_AUTO, version, CTLFLAG_RD, &smb_version, 0, "");

static MALLOC_DEFINE(M_NSMBDEV, "NETSMBDEV", "NET/SMB device");


/*
int smb_dev_queue(struct smb_dev *ndp, struct smb_rq *rqp, int prio);
*/

static struct cdevsw nsmb_cdevsw = {
	/* open */	nsmb_dev_open,
	/* close */	nsmb_dev_close,
	/* read */	nsmb_dev_read,
	/* write */	nsmb_dev_write,
	/* ioctl */ 	nsmb_dev_ioctl,
	/* poll */	nsmb_dev_poll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	NSMB_NAME,
	/* maj */	NSMB_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
#ifndef FB_CURRENT
	/* bmaj */	-1
#endif
};

static eventhandler_tag nsmb_dev_tag;

static void
nsmb_dev_clone(void *arg, char *name, int namelen, dev_t *dev)
{
	int min;

	if (*dev != NODEV)
		return;
	if (dev_stdclone(name, NULL, NSMB_NAME, &min) != 1)
		return;
	*dev = make_dev(&nsmb_cdevsw, min, 0, 0, 0600, NSMB_NAME"%d", min);
}

static int
nsmb_dev_open(dev_t dev, int oflags, int devtype, struct thread *td)
{
	struct smb_dev *sdp;
	struct ucred *cred = td->td_ucred;
	int s;

	sdp = SMB_GETDEV(dev);
	if (sdp && (sdp->sd_flags & NSMBFL_OPEN))
		return EBUSY;
	if (sdp == NULL) {
		sdp = malloc(sizeof(*sdp), M_NSMBDEV, M_WAITOK);
		dev->si_drv1 = (void*)sdp;
	}
	/*
	 * XXX: this is just crazy - make a device for an already passed device...
	 * someone should take care of it.
	 */
	if ((dev->si_flags & SI_NAMED) == 0)
		make_dev(&nsmb_cdevsw, minor(dev), cred->cr_uid, cred->cr_gid, 0700,
		    NSMB_NAME"%d", dev2unit(dev));
	bzero(sdp, sizeof(*sdp));
/*
	STAILQ_INIT(&sdp->sd_rqlist);
	STAILQ_INIT(&sdp->sd_rplist);
	bzero(&sdp->sd_pollinfo, sizeof(struct selinfo));
*/
	s = splimp();
	sdp->sd_level = -1;
	sdp->sd_flags |= NSMBFL_OPEN;
	splx(s);
	return 0;
}

static int
nsmb_dev_close(dev_t dev, int flag, int fmt, struct thread *td)
{
	struct smb_dev *sdp;
	struct smb_vc *vcp;
	struct smb_share *ssp;
	struct smb_cred scred;
	int s;

	SMB_CHECKMINOR(dev);
	s = splimp();
	if ((sdp->sd_flags & NSMBFL_OPEN) == 0) {
		splx(s);
		return EBADF;
	}
	smb_makescred(&scred, td, NULL);
	ssp = sdp->sd_share;
	if (ssp != NULL)
		smb_share_rele(ssp, &scred);
	vcp = sdp->sd_vc;
	if (vcp != NULL)
		smb_vc_rele(vcp, &scred);
/*
	smb_flushq(&sdp->sd_rqlist);
	smb_flushq(&sdp->sd_rplist);
*/
	dev->si_drv1 = NULL;
	free(sdp, M_NSMBDEV);
	destroy_dev(dev);
	splx(s);
	return 0;
}


static int
nsmb_dev_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct smb_dev *sdp;
	struct smb_vc *vcp;
	struct smb_share *ssp;
	struct smb_cred scred;
	int error = 0;

	SMB_CHECKMINOR(dev);
	if ((sdp->sd_flags & NSMBFL_OPEN) == 0)
		return EBADF;

	smb_makescred(&scred, td, NULL);
	switch (cmd) {
	    case SMBIOC_OPENSESSION:
		if (sdp->sd_vc)
			return EISCONN;
		error = smb_usr_opensession((struct smbioc_ossn*)data,
		    &scred, &vcp);
		if (error)
			break;
		sdp->sd_vc = vcp;
		smb_vc_unlock(vcp, 0, td);
		sdp->sd_level = SMBL_VC;
		break;
	    case SMBIOC_OPENSHARE:
		if (sdp->sd_share)
			return EISCONN;
		if (sdp->sd_vc == NULL)
			return ENOTCONN;
		error = smb_usr_openshare(sdp->sd_vc,
		    (struct smbioc_oshare*)data, &scred, &ssp);
		if (error)
			break;
		sdp->sd_share = ssp;
		smb_share_unlock(ssp, 0, td);
		sdp->sd_level = SMBL_SHARE;
		break;
	    case SMBIOC_REQUEST:
		if (sdp->sd_share == NULL)
			return ENOTCONN;
		error = smb_usr_simplerequest(sdp->sd_share,
		    (struct smbioc_rq*)data, &scred);
		break;
	    case SMBIOC_T2RQ:
		if (sdp->sd_share == NULL)
			return ENOTCONN;
		error = smb_usr_t2request(sdp->sd_share,
		    (struct smbioc_t2rq*)data, &scred);
		break;
	    case SMBIOC_SETFLAGS: {
		struct smbioc_flags *fl = (struct smbioc_flags*)data;
		int on;
	
		if (fl->ioc_level == SMBL_VC) {
			if (fl->ioc_mask & SMBV_PERMANENT) {
				on = fl->ioc_flags & SMBV_PERMANENT;
				if ((vcp = sdp->sd_vc) == NULL)
					return ENOTCONN;
				error = smb_vc_get(vcp, LK_EXCLUSIVE, &scred);
				if (error)
					break;
				if (on && (vcp->obj.co_flags & SMBV_PERMANENT) == 0) {
					vcp->obj.co_flags |= SMBV_PERMANENT;
					smb_vc_ref(vcp);
				} else if (!on && (vcp->obj.co_flags & SMBV_PERMANENT)) {
					vcp->obj.co_flags &= ~SMBV_PERMANENT;
					smb_vc_rele(vcp, &scred);
				}
				smb_vc_put(vcp, &scred);
			} else
				error = EINVAL;
		} else if (fl->ioc_level == SMBL_SHARE) {
			if (fl->ioc_mask & SMBS_PERMANENT) {
				on = fl->ioc_flags & SMBS_PERMANENT;
				if ((ssp = sdp->sd_share) == NULL)
					return ENOTCONN;
				error = smb_share_get(ssp, LK_EXCLUSIVE, &scred);
				if (error)
					break;
				if (on && (ssp->obj.co_flags & SMBS_PERMANENT) == 0) {
					ssp->obj.co_flags |= SMBS_PERMANENT;
					smb_share_ref(ssp);
				} else if (!on && (ssp->obj.co_flags & SMBS_PERMANENT)) {
					ssp->obj.co_flags &= ~SMBS_PERMANENT;
					smb_share_rele(ssp, &scred);
				}
				smb_share_put(ssp, &scred);
			} else
				error = EINVAL;
			break;
		} else
			error = EINVAL;
		break;
	    }
	    case SMBIOC_LOOKUP:
		if (sdp->sd_vc || sdp->sd_share)
			return EISCONN;
		vcp = NULL;
		ssp = NULL;
		error = smb_usr_lookup((struct smbioc_lookup*)data, &scred, &vcp, &ssp);
		if (error)
			break;
		if (vcp) {
			sdp->sd_vc = vcp;
			smb_vc_unlock(vcp, 0, td);
			sdp->sd_level = SMBL_VC;
		}
		if (ssp) {
			sdp->sd_share = ssp;
			smb_share_unlock(ssp, 0, td);
			sdp->sd_level = SMBL_SHARE;
		}
		break;
	    case SMBIOC_READ: case SMBIOC_WRITE: {
		struct smbioc_rw *rwrq = (struct smbioc_rw*)data;
		struct uio auio;
		struct iovec iov;
	
		if ((ssp = sdp->sd_share) == NULL)
			return ENOTCONN;
		iov.iov_base = rwrq->ioc_base;
		iov.iov_len = rwrq->ioc_cnt;
		auio.uio_iov = &iov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = rwrq->ioc_offset;
		auio.uio_resid = rwrq->ioc_cnt;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_rw = (cmd == SMBIOC_READ) ? UIO_READ : UIO_WRITE;
		auio.uio_td = td;
		if (cmd == SMBIOC_READ)
			error = smb_read(ssp, rwrq->ioc_fh, &auio, &scred);
		else
			error = smb_write(ssp, rwrq->ioc_fh, &auio, &scred);
		rwrq->ioc_cnt -= auio.uio_resid;
		break;
	    }
	    default:
		error = ENODEV;
	}
	return error;
}

static int
nsmb_dev_read(dev_t dev, struct uio *uio, int flag)
{
	return EACCES;
}

static int
nsmb_dev_write(dev_t dev, struct uio *uio, int flag)
{
	return EACCES;
}

static int
nsmb_dev_poll(dev_t dev, int events, struct thread *td)
{
	return ENODEV;
}

static int
nsmb_dev_load(module_t mod, int cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	    case MOD_LOAD:
		error = smb_sm_init();
		if (error)
			break;
		error = smb_iod_init();
		if (error) {
			smb_sm_done();
			break;
		}
		cdevsw_add(&nsmb_cdevsw);
		nsmb_dev_tag = EVENTHANDLER_REGISTER(dev_clone, nsmb_dev_clone, 0, 1000);
		printf("netsmb_dev: loaded\n");
		break;
	    case MOD_UNLOAD:
		smb_iod_done();
		error = smb_sm_done();
		error = 0;
		EVENTHANDLER_DEREGISTER(dev_clone, nsmb_dev_tag);
		cdevsw_remove(&nsmb_cdevsw);
		printf("netsmb_dev: unloaded\n");
		break;
	    default:
		error = EINVAL;
		break;
	}
	return error;
}

DEV_MODULE (dev_netsmb, nsmb_dev_load, 0);

/*
 * Convert a file descriptor to appropriate smb_share pointer
 */
static struct file*
nsmb_getfp(struct filedesc* fdp, int fd, int flag)
{
	struct file* fp;

	FILEDESC_LOCK(fdp);
	if (((u_int)fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL ||
	    (fp->f_flag & flag) == 0) {
		FILEDESC_UNLOCK(fdp);
		return (NULL);
	}
	fhold(fp);
	FILEDESC_UNLOCK(fdp);
	return (fp);
}

int
smb_dev2share(int fd, int mode, struct smb_cred *scred,
	struct smb_share **sspp)
{
	struct file *fp;
	struct vnode *vp;
	struct smb_dev *sdp;
	struct smb_share *ssp;
	dev_t dev;
	int error;

	fp = nsmb_getfp(scred->scr_td->td_proc->p_fd, fd, FREAD | FWRITE);
	if (fp == NULL)
		return EBADF;
	vp = (struct vnode*)fp->f_data;
	if (vp == NULL) {
		fdrop(fp, curthread);
		return EBADF;
	}
	dev = vn_todev(vp);
	if (dev == NODEV) {
		fdrop(fp, curthread);
		return EBADF;
	}
	SMB_CHECKMINOR(dev);
	ssp = sdp->sd_share;
	if (ssp == NULL) {
		fdrop(fp, curthread);
		return ENOTCONN;
	}
	error = smb_share_get(ssp, LK_EXCLUSIVE, scred);
	if (error == 0) 
		*sspp = ssp;
	fdrop(fp, curthread);
	return error;
}

