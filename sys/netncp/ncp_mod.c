/*-
 * Copyright (c) 2003 Tim J. Robbins.
 * Copyright (c) 1999, 2000, 2001 Boris Popov
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/ioccom.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_ncp.h>
#include <netncp/ncp_user.h>
#include <netncp/ncp_rq.h>
#include <netncp/ncp_nls.h>
#include <netncp/ncpio.h>

int ncp_version = NCP_VERSION;

SYSCTL_NODE(_net, OID_AUTO, ncp, CTLFLAG_RW, NULL, "NetWare requester");
SYSCTL_INT(_net_ncp, OID_AUTO, version, CTLFLAG_RD, &ncp_version, 0, "");

MODULE_VERSION(ncp, 1);
MODULE_DEPEND(ncp, libmchain, 1, 1, 1);

static struct cdev *ncp_dev;

static d_ioctl_t	ncp_ioctl;

static struct cdevsw ncp_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_ioctl =	ncp_ioctl,
	.d_name =	"ncp",
};

static int ncp_conn_frag_rq(struct ncp_conn *, struct thread *,
    struct ncp_conn_frag *);
static int ncp_conn_handler(struct thread *, struct ncpioc_request *,
    struct ncp_conn *, struct ncp_handle *);
static int sncp_conn_scan(struct thread *, struct ncpioc_connscan *);
static int sncp_connect(struct thread *, struct ncpioc_connect *);
static int sncp_request(struct thread *, struct ncpioc_request *);

static int
ncp_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{

	switch (cmd) {
	case NCPIOC_CONNECT:
		return (sncp_connect(td, (struct ncpioc_connect *)data));
	case NCPIOC_CONNSCAN:
		return (sncp_conn_scan(td, (struct ncpioc_connscan *)data));
	case NCPIOC_REQUEST:
		return (sncp_request(td, (struct ncpioc_request *)data));
	}
	return (EINVAL);
}

/*
 * Attach to NCP server
 */

static int
sncp_connect(struct thread *td, struct ncpioc_connect *args)
{
	int connHandle = 0, error;
	struct ncp_conn *conn;
	struct ncp_handle *handle;
	struct ncp_conn_args li;

	checkbad(copyin(args->ioc_li,&li,sizeof(li)));
	/* XXX Should be useracc() */
	checkbad(copyout(&connHandle,args->ioc_connhandle,
	    sizeof(connHandle)));
	li.password = li.user = NULL;
	error = ncp_conn_getattached(&li, td, td->td_ucred, NCPM_WRITE | NCPM_EXECUTE, &conn);
	if (error) {
		error = ncp_conn_alloc(&li, td, td->td_ucred, &conn);
		if (error)
			goto bad;
		error = ncp_conn_reconnect(conn);
		if (error)
			ncp_conn_free(conn);
	}
	if (!error) {
		error = ncp_conn_gethandle(conn, td, &handle);
		copyout(&handle->nh_id, args->ioc_connhandle,
		    sizeof(args->ioc_connhandle));
		ncp_conn_unlock(conn,td);
	}
bad:
	return error;
}

static int
sncp_request(struct thread *td, struct ncpioc_request *args)
{
	struct ncp_rq *rqp;
	struct ncp_conn *conn;
	struct ncp_handle *handle;
	int error = 0, rqsize;

	error = ncp_conn_findhandle(args->ioc_connhandle, td, &handle);
	if (error)
		return error;
	conn = handle->nh_conn;
	if (args->ioc_fn == NCP_CONN)
		return ncp_conn_handler(td, args, conn, handle);
	error = copyin(&args->ioc_ncpbuf->rqsize, &rqsize, sizeof(int));
	if (error)
		return(error);
	error = ncp_rq_alloc(args->ioc_fn, conn, td, td->td_ucred, &rqp);
	if (error)
		return error;
	if (rqsize) {
		error = mb_put_mem(&rqp->rq, (caddr_t)args->ioc_ncpbuf->packet,
		    rqsize, MB_MUSER);
		if (error)
			goto bad;
	}
	rqp->nr_flags |= NCPR_DONTFREEONERR;
	error = ncp_request(rqp);
	if (error == 0 && rqp->nr_rpsize)
		error = md_get_mem(&rqp->rp, (caddr_t)args->ioc_ncpbuf->packet, 
				rqp->nr_rpsize, MB_MUSER);
	copyout(&rqp->nr_cs, &args->ioc_ncpbuf->cs, sizeof(rqp->nr_cs));
	copyout(&rqp->nr_cc, &args->ioc_ncpbuf->cc, sizeof(rqp->nr_cc));
	copyout(&rqp->nr_rpsize, &args->ioc_ncpbuf->rpsize, sizeof(rqp->nr_rpsize));
bad:
	ncp_rq_done(rqp);
	return error;
}

static int
ncp_mod_login(struct ncp_conn *conn, char *user, int objtype, char *password,
	struct thread *td, struct ucred *cred)
{
	int error;

	if (ncp_suser(cred) != 0 && cred->cr_uid != conn->nc_owner->cr_uid)
		return EACCES;
	conn->li.user = ncp_str_dup(user);
	if (conn->li.user == NULL)
		return ENOMEM;
	conn->li.password = ncp_str_dup(password);
	if (conn->li.password == NULL) {
		error = ENOMEM;
		goto bad;
	}
	ncp_str_upper(conn->li.user);
	if ((conn->li.opt & NCP_OPT_NOUPCASEPASS) == 0)
		ncp_str_upper(conn->li.password);
	conn->li.objtype = objtype;
	error = ncp_conn_login(conn, td, cred);
	return error;
bad:
	if (conn->li.user) {
		free(conn->li.user, M_NCPDATA);
		conn->li.user = NULL;
	}
	if (conn->li.password) {
		free(conn->li.password, M_NCPDATA);
		conn->li.password = NULL;
	}
	return error;
}

static int
ncp_conn_handler(struct thread *td, struct ncpioc_request *args,
	struct ncp_conn *conn, struct ncp_handle *hp)
{
	int error = 0, rqsize, subfn;
	struct ucred *cred;

	char *pdata;

	cred = td->td_ucred;
	error = copyin(&args->ioc_ncpbuf->rqsize, &rqsize, sizeof(int));
	if (error)
		return(error);
	error = 0;
	pdata = args->ioc_ncpbuf->packet;
	subfn = *(pdata++) & 0xff;
	rqsize--;
	switch (subfn) {
	    case NCP_CONN_READ: case NCP_CONN_WRITE: {
		struct ncp_rw rwrq;
		struct uio auio;
		struct iovec iov;

		if (rqsize != sizeof(rwrq))
			return (EBADRPC);
		error = copyin(pdata,&rwrq,rqsize);
		if (error)
			return (error);
		iov.iov_base = rwrq.nrw_base;
		iov.iov_len = rwrq.nrw_cnt;
		auio.uio_iov = &iov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = rwrq.nrw_offset;
		auio.uio_resid = rwrq.nrw_cnt;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_rw = (subfn == NCP_CONN_READ) ? UIO_READ : UIO_WRITE;
		auio.uio_td = td;
		if (subfn == NCP_CONN_READ)
			error = ncp_read(conn, &rwrq.nrw_fh, &auio, cred);
		else
			error = ncp_write(conn, &rwrq.nrw_fh, &auio, cred);
		rwrq.nrw_cnt -= auio.uio_resid;
		/*td->td_retval[0] = rwrq.nrw_cnt;*/
		break;
	    } /* case int_read/write */
	    case NCP_CONN_SETFLAGS: {
		u_int16_t mask, flags;

		error = copyin(pdata,&mask, sizeof(mask));
		if (error)
			return error;
		pdata += sizeof(mask);
		error = copyin(pdata,&flags,sizeof(flags));
		if (error)
			return error;
		error = ncp_conn_lock(conn, td, cred, NCPM_WRITE);
		if (error)
			return error;
		if (mask & NCPFL_PERMANENT) {
			conn->flags &= ~NCPFL_PERMANENT;
			conn->flags |= (flags & NCPFL_PERMANENT);
		}
		if (mask & NCPFL_PRIMARY) {
			error = ncp_conn_setprimary(conn, flags & NCPFL_PRIMARY);
			if (error) {
				ncp_conn_unlock(conn, td);
				break;
			}
		}
		ncp_conn_unlock(conn, td);
		break;
	    }
	    case NCP_CONN_LOGIN: {
		struct ncp_conn_login la;

		if (rqsize != sizeof(la))
			return EBADRPC;
		if (conn->flags & NCPFL_LOGGED)
			return EALREADY;
		if ((error = copyin(pdata,&la,rqsize)) != 0)
			break;
		error = ncp_conn_lock(conn, td, cred, NCPM_EXECUTE | NCPM_WRITE);
		if (error)
			return error;
		error = ncp_mod_login(conn, la.username, la.objtype,
		    la.password, td, td->td_ucred);
		ncp_conn_unlock(conn, td);
		break;
	    }
	    case NCP_CONN_GETINFO: {
		struct ncp_conn_stat ncs;
		int len = sizeof(ncs);

		error = ncp_conn_lock(conn, td, td->td_ucred, NCPM_READ);
		if (error)
			return error;
		ncp_conn_getinfo(conn, &ncs);
		copyout(&len, &args->ioc_ncpbuf->rpsize, sizeof(int));
		error = copyout(&ncs, &args->ioc_ncpbuf->packet, len);
		ncp_conn_unlock(conn, td);
		break;
	    }
	    case NCP_CONN_GETUSER: {
		int len;

		error = ncp_conn_lock(conn, td, td->td_ucred, NCPM_READ);
		if (error)
			return error;
		len = (conn->li.user) ? strlen(conn->li.user) + 1 : 0;
		copyout(&len, &args->ioc_ncpbuf->rpsize, sizeof(int));
		if (len) {
			error = copyout(conn->li.user,
			    &args->ioc_ncpbuf->packet, len);
		}
		ncp_conn_unlock(conn, td);
		break;
	    }
	    case NCP_CONN_CONN2REF: {
		int len = sizeof(int);

		error = ncp_conn_lock(conn, td, td->td_ucred, NCPM_READ);
		if (error)
			return error;
		copyout(&len, &args->ioc_ncpbuf->rpsize, sizeof(int));
		if (len) {
			error = copyout(&conn->nc_id,
			    &args->ioc_ncpbuf->packet, len);
		}
		ncp_conn_unlock(conn, td);
		break;
	    }
	    case NCP_CONN_FRAG: {
		struct ncp_conn_frag nf;

		if (rqsize != sizeof(nf))
			return (EBADRPC);
		if ((error = copyin(pdata, &nf, rqsize)) != 0) break;
		error = ncp_conn_lock(conn, td, cred, NCPM_EXECUTE);
		if (error)
			return error;
		error = ncp_conn_frag_rq(conn, td, &nf);
		ncp_conn_unlock(conn, td);
		copyout(&nf, &pdata, sizeof(nf));
		td->td_retval[0] = error;
		break;
	    }
	    case NCP_CONN_DUP: {
		struct ncp_handle *newhp;
		int len = sizeof(NWCONN_HANDLE);

		error = ncp_conn_lock(conn, td, cred, NCPM_READ);
		if (error) break;
		copyout(&len, &args->ioc_ncpbuf->rpsize, len);
		error = ncp_conn_gethandle(conn, td, &newhp);
		if (!error)
			error = copyout(&newhp->nh_id,
			    args->ioc_ncpbuf->packet, len);
		ncp_conn_unlock(conn, td);
		break;
	    }
	    case NCP_CONN_CONNCLOSE: {
		error = ncp_conn_lock(conn, td, cred, NCPM_EXECUTE);
		if (error) break;
		ncp_conn_puthandle(hp, td, 0);
		error = ncp_conn_free(conn);
		if (error)
			ncp_conn_unlock(conn, td);
		break;
	    }
	    default:
		    error = EOPNOTSUPP;
	}
	return error;
}

static int
sncp_conn_scan(struct thread *td, struct ncpioc_connscan *args)
{
	int connHandle = 0, error;
	struct ncp_conn_args li, *lip;
	struct ncp_conn *conn;
	struct ncp_handle *hp;
	char *user = NULL, *password = NULL;

	if (args->ioc_li) {
		if (copyin(args->ioc_li, &li, sizeof(li)))
			return EFAULT;
		lip = &li;
	} else {
		lip = NULL;
	}

	if (lip != NULL) {
		lip->server[sizeof(lip->server)-1]=0; /* just to make sure */
		ncp_str_upper(lip->server);
		if (lip->user) {
			user = ncp_str_dup(lip->user);
			if (user == NULL)
				return EINVAL;
			ncp_str_upper(user);
		}
		if (lip->password) {
			password = ncp_str_dup(lip->password);
			if (password == NULL) {
				if (user)
					free(user, M_NCPDATA);
				return EINVAL;
			}
			ncp_str_upper(password);
		}
		lip->user = user;
		lip->password = password;
	}
	error = ncp_conn_getbyli(lip, td, td->td_ucred, NCPM_EXECUTE, &conn);
	if (!error) {		/* already have this login */
		ncp_conn_gethandle(conn, td, &hp);
		connHandle = hp->nh_id;
		ncp_conn_unlock(conn, td);
		copyout(&connHandle, args->ioc_connhandle, sizeof(connHandle));
	}
	if (user)
		free(user, M_NCPDATA);
	if (password)
		free(password, M_NCPDATA);
	return error;

}

int
ncp_conn_frag_rq(struct ncp_conn *conn, struct thread *td,
		 struct ncp_conn_frag *nfp)
{
	NW_FRAGMENT *fp;
	struct ncp_rq *rqp;
	u_int32_t fsize;
	int error, i, rpsize;

	error = ncp_rq_alloc(nfp->fn, conn, td, td->td_ucred, &rqp);
	if (error)
		return error;
	for(fp = nfp->rqf, i = 0; i < nfp->rqfcnt; i++, fp++) {
		error = mb_put_mem(&rqp->rq, (caddr_t)fp->fragAddress, fp->fragSize, MB_MUSER);
		if (error)
			goto bad;
	}
	rqp->nr_flags |= NCPR_DONTFREEONERR;
	error = ncp_request(rqp);
	if (error)
		goto bad;
	rpsize = rqp->nr_rpsize;
	if (rpsize && nfp->rpfcnt) {
		for(fp = nfp->rpf, i = 0; i < nfp->rpfcnt; i++, fp++) {
			error = copyin(&fp->fragSize, &fsize, sizeof (fsize));
			if (error)
				break;
			fsize = min(fsize, rpsize);
			error = md_get_mem(&rqp->rp, (caddr_t)fp->fragAddress, fsize, MB_MUSER);
			if (error)
				break;
			rpsize -= fsize;
			error = copyout(&fsize, &fp->fragSize, sizeof (fsize));
			if (error)
				break;
		}
	}
	nfp->cs = rqp->nr_cs;
	nfp->cc = rqp->nr_cc;
bad:
	ncp_rq_done(rqp);
	return error;
}

static int
ncp_load(void)
{
	int error;

	if ((error = ncp_init()) != 0)
		return (error);
	ncp_dev = make_dev(&ncp_cdevsw, 0, 0, 0, 0666, "ncp");
	printf("ncp_load: loaded\n");
	return (0);
}

static int
ncp_unload(void)
{
	int error;

	error = ncp_done();
	if (error)
		return (error);
	destroy_dev(ncp_dev);
	printf("ncp_unload: unloaded\n");
	return (0);
}

static int
ncp_mod_handler(module_t mod, int type, void *data)
{
	int error;

	switch (type) {
	case MOD_LOAD:
		error = ncp_load();
		break;
	case MOD_UNLOAD:
		error = ncp_unload();
		break;
	default:
		error = EINVAL;
	}
	return error;
}

static moduledata_t ncp_mod = {
	"ncp",
	ncp_mod_handler,
	NULL
};
DECLARE_MODULE(ncp, ncp_mod, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY);
