/*
 * Copyright (c) 1999, Boris Popov
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
 * $FreeBSD: src/sys/netncp/ncp_mod.c,v 1.2 1999/10/12 10:36:59 bp Exp $
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/uio.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_ncp.h>
#include <netncp/ncp_user.h>
#include <netncp/ncp_rq.h>
#include <netncp/ncp_nls.h>

int ncp_version = NCP_VERSION;

static int ncp_sysent;

SYSCTL_NODE(_net, OID_AUTO, ncp, CTLFLAG_RW, NULL, "NetWare requester");
SYSCTL_INT(_net_ncp, OID_AUTO, sysent, CTLFLAG_RD, &ncp_sysent, 0, "");
SYSCTL_INT(_net_ncp, OID_AUTO, version, CTLFLAG_RD, &ncp_version, 0, "");

static int
ncp_conn_frag_rq(struct ncp_conn *conn, struct proc *p, struct ncp_conn_frag *nfp);

/*
 * Attach to NCP server
 */
struct sncp_connect_args {
	struct ncp_conn_args *li;
	int *connHandle;
};

static int 
__P(sncp_connect(struct proc *p, struct sncp_connect_args *uap)){
	int connHandle = 0, error;
	struct ncp_conn *conn;
	struct ncp_handle *handle;
	struct ncp_conn_args li;

	checkbad(copyin(uap->li,&li,sizeof(li)));
	checkbad(copyout(&connHandle,uap->connHandle,sizeof(connHandle))); /* check before */
	li.password = li.user = NULL;
	error = ncp_conn_getattached(&li, p, p->p_ucred, NCPM_WRITE | NCPM_EXECUTE, &conn);
	if (error) {
		error = ncp_connect(&li, p, p->p_ucred, &conn);
	}
	if (!error) {
		error = ncp_conn_gethandle(conn, p, &handle);
		copyout(&handle->nh_id, uap->connHandle, sizeof(uap->connHandle));
		ncp_conn_unlock(conn,p);
	}
bad:
	p->p_retval[0]=error;
	return error;
}

struct sncp_request_args {
	int connHandle;
	int fn;
	struct ncp_buf *ncpbuf;
};

static int ncp_conn_handler(struct proc *p, struct sncp_request_args *uap,
	struct ncp_conn *conn, struct ncp_handle *handle);

static int
__P(sncp_request(struct proc *p, struct sncp_request_args *uap)){
	int error = 0, rqsize;
	struct ncp_conn *conn;
	struct ncp_handle *handle;
	DECLARE_RQ;

	error = ncp_conn_findhandle(uap->connHandle,p,&handle);
	if (error) return error;
	conn = handle->nh_conn;
	if (uap->fn == NCP_CONN)
		return ncp_conn_handler(p, uap, conn, handle);
	error = copyin(&uap->ncpbuf->rqsize, &rqsize, sizeof(int));
	if (error) return(error);
	error = ncp_conn_lock(conn,p,p->p_ucred,NCPM_EXECUTE);
	if (error) return(error);
	ncp_rq_head(rqp,NCP_REQUEST,uap->fn,p,p->p_ucred);
	if (rqsize)
		error = ncp_rq_usermem(rqp,(caddr_t)uap->ncpbuf->packet, rqsize);
	if (!error) {
		error = ncp_request(conn, rqp);
		if (error == 0 && rqp->rpsize)
			ncp_rp_usermem(rqp, (caddr_t)uap->ncpbuf->packet, 
				rqp->rpsize);
		copyout(&rqp->cs, &uap->ncpbuf->cs, sizeof(rqp->cs));
		copyout(&rqp->cc, &uap->ncpbuf->cc, sizeof(rqp->cc));
		copyout(&rqp->rpsize, &uap->ncpbuf->rpsize, sizeof(rqp->rpsize));
	}
	ncp_rq_done(rqp);
	ncp_conn_unlock(conn,p);
	return error;
}

static int
ncp_conn_handler(struct proc *p, struct sncp_request_args *uap,
	struct ncp_conn *conn, struct ncp_handle *hp)
{
	int error=0, rqsize, subfn;
	struct ucred *cred;
	
	char *pdata;

	cred = p->p_ucred;
	error = copyin(&uap->ncpbuf->rqsize, &rqsize, sizeof(int));
	if (error) return(error);
	error = 0;
	pdata = uap->ncpbuf->packet;
	subfn = *(pdata++) & 0xff;
	rqsize--;
	switch (subfn) {
	    case NCP_CONN_READ: case NCP_CONN_WRITE: {
		struct ncp_rw rwrq;
		struct uio auio;
		struct iovec iov;
	
		if (rqsize != sizeof(rwrq)) return (EBADRPC);	
		error = copyin(pdata,&rwrq,rqsize);
		if (error) return (error);
		iov.iov_base = rwrq.nrw_base;
		iov.iov_len = rwrq.nrw_cnt;
		auio.uio_iov = &iov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = rwrq.nrw_offset;
		auio.uio_resid = rwrq.nrw_cnt;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_rw = (subfn == NCP_CONN_READ) ? UIO_READ : UIO_WRITE;
		auio.uio_procp = p;
		error = ncp_conn_lock(conn,p,cred,NCPM_EXECUTE);
		if (error) return(error);
		if (subfn == NCP_CONN_READ)
			error = ncp_read(conn, &rwrq.nrw_fh, &auio, cred);
		else
			error = ncp_write(conn, &rwrq.nrw_fh, &auio, cred);
		rwrq.nrw_cnt -= auio.uio_resid;
		ncp_conn_unlock(conn,p);
		p->p_retval[0] = rwrq.nrw_cnt;
		break;
	    } /* case int_read/write */
	    case NCP_CONN_SETFLAGS: {
		u_int16_t mask, flags;

		error = copyin(pdata,&mask, sizeof(mask));
		if (error) return error;
		pdata += sizeof(mask);
		error = copyin(pdata,&flags,sizeof(flags));
		if (error) return error;
		error = ncp_conn_lock(conn,p,cred,NCPM_WRITE);
		if (error) return error;
		if (mask & NCPFL_PERMANENT) {
			conn->flags &= ~NCPFL_PERMANENT;
			conn->flags |= (flags & NCPFL_PERMANENT);
		}
		if (mask & NCPFL_PRIMARY) {
			error = ncp_conn_setprimary(conn, flags & NCPFL_PRIMARY);
			if (error) {
				ncp_conn_unlock(conn,p);
				break;
			}
		}
		ncp_conn_unlock(conn,p);
		break;
	    }
	    case NCP_CONN_LOGIN: {
		struct ncp_conn_login la;

		if (rqsize != sizeof(la)) return (EBADRPC);	
		if ((error = copyin(pdata,&la,rqsize)) != 0) break;
		error = ncp_conn_lock(conn, p, cred, NCPM_EXECUTE | NCPM_WRITE);
		if (error) return error;
		error = ncp_login(conn, la.username, la.objtype, la.password, p, p->p_ucred);
		ncp_conn_unlock(conn, p);
		p->p_retval[0] = error;
		break;
	    }
	    case NCP_CONN_GETINFO: {
		struct ncp_conn_stat ncs;
		int len = sizeof(ncs);

		error = ncp_conn_lock(conn, p, p->p_ucred, NCPM_READ);
		if (error) return error;
		ncp_conn_getinfo(conn, &ncs);
		copyout(&len, &uap->ncpbuf->rpsize, sizeof(int));
		error = copyout(&ncs, &uap->ncpbuf->packet, len);
		ncp_conn_unlock(conn, p);
		break;
	    }
	    case NCP_CONN_GETUSER: {
		int len;

		error = ncp_conn_lock(conn, p, p->p_ucred, NCPM_READ);
		if (error) return error;
		len = (conn->li.user) ? strlen(conn->li.user) + 1 : 0;
		copyout(&len, &uap->ncpbuf->rpsize, sizeof(int));
		if (len) {
			error = copyout(conn->li.user, &uap->ncpbuf->packet, len);
		}
		ncp_conn_unlock(conn, p);
		break;
	    }
	    case NCP_CONN_CONN2REF: {
		int len = sizeof(int);

		error = ncp_conn_lock(conn, p, p->p_ucred, NCPM_READ);
		if (error) return error;
		copyout(&len, &uap->ncpbuf->rpsize, sizeof(int));
		if (len) {
			error = copyout(&conn->nc_id, &uap->ncpbuf->packet, len);
		}
		ncp_conn_unlock(conn, p);
		break;
	    }
	    case NCP_CONN_FRAG: {
		struct ncp_conn_frag nf;

		if (rqsize != sizeof(nf)) return (EBADRPC);	
		if ((error = copyin(pdata, &nf, rqsize)) != 0) break;
		error = ncp_conn_lock(conn, p, cred, NCPM_EXECUTE);
		if (error) return error;
		error = ncp_conn_frag_rq(conn, p, &nf);
		ncp_conn_unlock(conn, p);
		copyout(&nf, &pdata, sizeof(nf));
		p->p_retval[0] = error;
		break;
	    }
	    case NCP_CONN_DUP: {
		struct ncp_handle *newhp;
		int len = sizeof(NWCONN_HANDLE);

		error = ncp_conn_lock(conn, p, cred, NCPM_READ);
		if (error) break;
		copyout(&len, &uap->ncpbuf->rpsize, len);
		error = ncp_conn_gethandle(conn, p, &newhp);
		if (!error)
			error = copyout(&newhp->nh_id, uap->ncpbuf->packet, len);
		ncp_conn_unlock(conn,p);
		break;
	    }
	    case NCP_CONN_CONNCLOSE: {
		error = ncp_conn_lock(conn, p, cred, NCPM_EXECUTE);
		if (error) break;
		ncp_conn_puthandle(hp, p, 0);
		error = ncp_disconnect(conn);
		if (error)
			ncp_conn_unlock(conn, p);
		break;
	    }
	    default:
		    error = EOPNOTSUPP;
	}
	return error;
}

struct sncp_conn_scan_args {
	struct ncp_conn_args *li;
	int *connHandle;
};

static int 
__P(sncp_conn_scan(struct proc *p, struct sncp_conn_scan_args *uap)){
	int connHandle = 0, error;
	struct ncp_conn_args li, *lip;
	struct ncp_conn *conn;
	struct ncp_handle *hp;
	char *user = NULL, *password = NULL;

	if (uap->li) {
		if (copyin(uap->li,&li,sizeof(li))) return EFAULT;
		lip = &li;
	} else {
		lip = NULL;
	}

	if (lip != NULL) {
		lip->server[sizeof(lip->server)-1]=0; /* just to make sure */
		ncp_str_upper(lip->server);
		if (lip->user) {
			user = ncp_str_dup(lip->user);
			if (user == NULL) return EINVAL;
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
	error = ncp_conn_getbyli(lip,p,p->p_ucred,NCPM_EXECUTE,&conn);
	if (!error) { 		/* already have this login */
		ncp_conn_gethandle(conn, p, &hp);
		connHandle = hp->nh_id;
		ncp_conn_unlock(conn,p);
		copyout(&connHandle,uap->connHandle,sizeof(connHandle));
	}
	if (user) free(user, M_NCPDATA);
	if (password) free(password, M_NCPDATA);
	p->p_retval[0] = error;
	return error;

}

int
ncp_conn_frag_rq(struct ncp_conn *conn, struct proc *p, struct ncp_conn_frag *nfp){
	int error = 0, i, rpsize;
	u_int32_t fsize;
	NW_FRAGMENT *fp;
	DECLARE_RQ;

	ncp_rq_head(rqp,NCP_REQUEST,nfp->fn,p,p->p_ucred);
	if (nfp->rqfcnt) {
		for(fp = nfp->rqf, i = 0; i < nfp->rqfcnt; i++, fp++) {
			checkbad(ncp_rq_usermem(rqp,(caddr_t)fp->fragAddress, fp->fragSize));
		}
	}
	checkbad(ncp_request(conn, rqp));
	rpsize = rqp->rpsize;
	if (rpsize && nfp->rpfcnt) {
		for(fp = nfp->rpf, i = 0; i < nfp->rpfcnt; i++, fp++) {
			checkbad(copyin(&fp->fragSize, &fsize, sizeof (fsize)));
			fsize = min(fsize, rpsize);
			checkbad(ncp_rp_usermem(rqp,(caddr_t)fp->fragAddress, fsize));
			rpsize -= fsize;
			checkbad(copyout(&fsize, &fp->fragSize, sizeof (fsize)));
		}
	}
	nfp->cs = rqp->cs;
	nfp->cc = rqp->cc;
	NCP_RQ_EXIT;
	return error;
}

/*
 * Internal functions, here should be all calls that do not require connection.
 * To simplify possible future movement to cdev, we use IOCTL macros.
 * Pretty much of this stolen from ioctl() function.
 */
struct sncp_intfn_args {
	u_long	com;
	caddr_t	data;
};

static int
sncp_intfn(struct proc *p, struct sncp_intfn_args *uap)
{
	return ENOSYS;
}
/*
 * define our new system calls
 */
static struct sysent newent[] = {
	{2, 	(sy_call_t*)sncp_conn_scan},
	{2,	(sy_call_t*)sncp_connect},
	{2,	(sy_call_t*)sncp_intfn},
	{3,	(sy_call_t*)sncp_request}
};

#define	SC_SIZE	sizeof(newent)/sizeof(struct sysent)
/*
 * Miscellaneous modules must have their own save areas...
 */
static struct sysent	oldent[SC_SIZE];	/* save are for old callslot entry*/

/*
 * Number of syscall entries for a.out executables
 */
/*#define nsysent SYS_MAXSYSCALL*/
#define nsysent (aout_sysvec.sv_size)


static int
ncp_load(void) {
	int i, ff, scnt, err=0;

	while(1) {
		/* Search the table looking for an enough number of slots... */
		for (scnt=0, ff = -1, i = 0; i < nsysent; i++) {
			if (sysent[i].sy_call == (sy_call_t *)lkmnosys) {
				if (ff == -1) {
				    ff = i;
				    scnt = 1;
				} else {
				    scnt++;
				    if (scnt == SC_SIZE) break;
				}
			} else {
				ff = -1;
			}
		}
		/* out of allocable slots?*/
		if(i == nsysent || ff == -1) {
			err = ENFILE;
			break;
		}
		err = ncp_init();
		if (err) break;
		bcopy(&sysent[ff], &oldent, sizeof(struct sysent)*SC_SIZE);
		bcopy(&newent, &sysent[ff], sizeof(struct sysent)*SC_SIZE);
		ncp_sysent = ff;	/* slot in sysent[]*/
		printf("ncp_load: [%d-%d]\n",ff,i);
		break;
	}

	return( err);
}

static int
ncp_unload(void) {
	ncp_done();
	bcopy(&oldent, &sysent[ncp_sysent], sizeof(struct sysent) * SC_SIZE);
	printf( "ncp_unload: unloaded\n");
	return 0;
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
                                                               \
static moduledata_t ncp_mod = {
	"ncp",
	ncp_mod_handler,
	NULL
};
DECLARE_MODULE(ncp, ncp_mod, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY);
