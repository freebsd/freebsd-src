/*
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
 *
 * $FreeBSD$
 *
 * Core of NCP protocol
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/uio.h>

#include <netipx/ipx.h>
#include <netipx/ipx_var.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_sock.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_ncp.h>
#include <netncp/ncp_rq.h>
#include <netncp/nwerror.h>

#ifdef NCP_DATA_DEBUG
static
void m_dumpm(struct mbuf *m) {
	char *p;
	int len;
	printf("d=");
	while(m) {
		p=mtod(m,char *);
		len=m->m_len;
		printf("(%d)",len);
		while(len--){
			printf("%02x ",((int)*(p++)) & 0xff);
		}
		m=m->m_next;
	};
	printf("\n");
}
#endif /* NCP_DATA_DEBUG */

int
ncp_chkintr(struct ncp_conn *conn, struct proc *p)
{
	sigset_t tmpset;

	if (p == NULL)
		return 0;
	PROC_LOCK(p);
	tmpset = p->p_siglist;
	SIGSETNAND(tmpset, p->p_sigmask);
	SIGSETNAND(tmpset, p->p_sigignore);
	if (SIGNOTEMPTY(p->p_siglist) && NCP_SIGMASK(tmpset)) {
		PROC_UNLOCK(p);
                return EINTR;
	}
	PROC_UNLOCK(p);
	return 0;
}

/*
 * Process initial NCP handshake (attach)
 * NOTE: Since all functions below may change conn attributes, they
 * should be called with LOCKED connection, also they use procp & ucred
 */
int
ncp_ncp_connect(struct ncp_conn *conn)
{
	struct ncp_rq *rqp;
	struct ncp_rphdr *rp;
	int error;
	
	error = ncp_rq_alloc_any(NCP_ALLOC_SLOT, 0, conn, conn->procp, conn->ucred, &rqp);
	if (error)
		return error;

	conn->flags &= ~(NCPFL_SIGNACTIVE | NCPFL_SIGNWANTED |
	    NCPFL_ATTACHED | NCPFL_LOGGED | NCPFL_INVALID);
	conn->seq = 0;
	error = ncp_request_int(rqp);
	if (!error) {
		rp = mtod(rqp->rp.md_top, struct ncp_rphdr*);
		conn->connid = rp->conn_low + (rp->conn_high << 8);
	}
	ncp_rq_done(rqp);
	if (error)
		return error;
	conn->flags |= NCPFL_ATTACHED | NCPFL_WASATTACHED;
	return 0;
}

int
ncp_ncp_disconnect(struct ncp_conn *conn)
{
	struct ncp_rq *rqp;
	int error;

	NCPSDEBUG("for connid=%d\n",conn->nc_id);
#ifdef NCPBURST
	ncp_burst_disconnect(conn);
#endif
	if (conn->flags & NCPFL_ATTACHED) {
		error = ncp_rq_alloc_any(NCP_FREE_SLOT, 0, conn, conn->procp, conn->ucred, &rqp);
		if (!error) {
			ncp_request_int(rqp);
			ncp_rq_done(rqp);
		}
	}
	ncp_conn_invalidate(conn);
	ncp_sock_disconnect(conn);
	return 0;
}

/*
 * All negotiation functions expect a locked connection
 */

int
ncp_negotiate_buffersize(struct ncp_conn *conn, int size, int *target)
{
	struct ncp_rq *rqp;
	u_int16_t bsize;
	int error;

	error = ncp_rq_alloc(0x21, conn, conn->procp, conn->ucred, &rqp);
	if (error)
		return error;
	mb_put_uint16be(&rqp->rq, size);
	error = ncp_request(rqp);
	if (error)
		return error;
	md_get_uint16be(&rqp->rp, &bsize);
	*target = min(bsize, size);
	ncp_rq_done(rqp);
	return error;
}

static int
ncp_negotiate_size_and_options(struct ncp_conn *conn, int size, int options, 
	    int *ret_size, u_int8_t *ret_options)
{
	struct ncp_rq *rqp;
	u_int16_t rs;
	int error;

	error = ncp_rq_alloc(0x61, conn, conn->procp, conn->ucred, &rqp);
	if (error)
		return error;
	mb_put_uint16be(&rqp->rq, size);
	mb_put_uint8(&rqp->rq, options);
	rqp->nr_minrplen = 2 + 2 + 1;
	error = ncp_request(rqp);
	if (error)
		return error;
	md_get_uint16be(&rqp->rp, &rs);
	*ret_size = (rs == 0) ? size : min(rs, size);
	md_get_uint16be(&rqp->rp, &rs);		/* skip echo socket */
	md_get_uint8(&rqp->rp, ret_options);
	ncp_rq_done(rqp);
	return error;
}

int
ncp_renegotiate_connparam(struct ncp_conn *conn, int buffsize, u_int8_t in_options)
{
	u_int8_t options;
	int neg_buffsize, error, sl, ckslevel, ilen;

	sl = conn->li.sig_level;
	if (sl >= 2)
		in_options |= NCP_SECURITY_LEVEL_SIGN_HEADERS;
	if (conn->li.saddr.sa_family == AF_IPX) {
		ilen = sizeof(ckslevel);
		error = kernel_sysctlbyname(curproc, "net.ipx.ipx.checksum",
		    &ckslevel, &ilen, NULL, 0, NULL);
		if (error)
			return error;
		if (ckslevel == 2)
			in_options |= NCP_IPX_CHECKSUM;
	}
	error = ncp_negotiate_size_and_options(conn, buffsize, in_options, 
	    &neg_buffsize, &options);
	if (!error) {
		if (conn->li.saddr.sa_family == AF_IPX &&
		    ((options ^ in_options) & NCP_IPX_CHECKSUM)) {
			if (ckslevel == 2) {
				printf("Server refuses to support IPX checksums\n");
				return NWE_REQUESTER_FAILURE;
			}
			in_options |= NCP_IPX_CHECKSUM;
			error = 1;
		}
		if ((options ^ in_options) & 2) {
			if (sl == 0 || sl == 3)
				return NWE_SIGNATURE_LEVEL_CONFLICT;
			if (sl == 1) {
				in_options |= NCP_SECURITY_LEVEL_SIGN_HEADERS;
				error = 1;
			}
		}
		if (error) {
			error = ncp_negotiate_size_and_options(conn,
		    	    buffsize, in_options, &neg_buffsize, &options);
			if ((options ^ in_options) & 3) {
				return NWE_SIGNATURE_LEVEL_CONFLICT;
			}
		}
	} else {
		in_options &= ~NCP_SECURITY_LEVEL_SIGN_HEADERS;
		error = ncp_negotiate_buffersize(conn, NCP_DEFAULT_BUFSIZE,
			      &neg_buffsize);
	}			  
	if (error) return error;
	if ((neg_buffsize < 512) || (neg_buffsize > NCP_MAX_BUFSIZE))
		return EINVAL;
	conn->buffer_size = neg_buffsize;
	if (in_options & NCP_SECURITY_LEVEL_SIGN_HEADERS)
		conn->flags |= NCPFL_SIGNWANTED;
	if (conn->li.saddr.sa_family == AF_IPX)
		ncp_sock_checksum(conn, in_options & NCP_IPX_CHECKSUM);
	return 0;
}

void
ncp_check_rq(struct ncp_conn *conn){
	return;
	if (conn->flags & NCPFL_INTR) return;
	/* first, check for signals */
	if (ncp_chkintr(conn,conn->procp)) {
		conn->flags |= NCPFL_INTR;
	}
	return;
}

int
ncp_get_bindery_object_id(struct ncp_conn *conn, 
		u_int16_t object_type, char *object_name, 
		struct ncp_bindery_object *target,
		struct proc *p,struct ucred *cred)
{
	struct ncp_rq *rqp;
	int error;

	error = ncp_rq_alloc_subfn(23, 53, conn, conn->procp, conn->ucred, &rqp);
	mb_put_uint16be(&rqp->rq, object_type);
	ncp_rq_pstring(rqp, object_name);
	rqp->nr_minrplen = 54;
	error = ncp_request(rqp);
	if (error)
		return error;
	md_get_uint32be(&rqp->rp, &target->object_id);
	md_get_uint16be(&rqp->rp, &target->object_type);
	md_get_mem(&rqp->rp, (caddr_t)target->object_name, 48, MB_MSYSTEM);
	ncp_rq_done(rqp);
	return 0;
}

/*
 * target is a 8-byte buffer
 */
int
ncp_get_encryption_key(struct ncp_conn *conn, char *target)
{
	struct ncp_rq *rqp;
	int error;

	error = ncp_rq_alloc_subfn(23, 23, conn, conn->procp, conn->ucred, &rqp);
	if (error)
		return error;
	rqp->nr_minrplen = 8;
	error = ncp_request(rqp);
	if (error)
		return error;
	md_get_mem(&rqp->rp, target, 8, MB_MSYSTEM);
	ncp_rq_done(rqp);
	return error;
}

/*
 * Initialize packet signatures. They a slightly modified MD4.
 * The first 16 bytes of logindata are the shuffled password,
 * the last 8 bytes the encryption key as received from the server.
 */
static int
ncp_sign_start(struct ncp_conn *conn, char *logindata)
{
	char msg[64];
	u_int32_t state[4];

	memcpy(msg, logindata, 24);
	memcpy(msg + 24, "Authorized NetWare Client", 25);
	bzero(msg + 24 + 25, sizeof(msg) - 24 - 25);

	conn->sign_state[0] = 0x67452301;
	conn->sign_state[1] = 0xefcdab89;
	conn->sign_state[2] = 0x98badcfe;
	conn->sign_state[3] = 0x10325476;
	ncp_sign(conn->sign_state, msg, state);
	conn->sign_root[0] = state[0];
	conn->sign_root[1] = state[1];
	conn->flags |= NCPFL_SIGNACTIVE;
	return 0;
}


int
ncp_login_encrypted(struct ncp_conn *conn, struct ncp_bindery_object *object,
	const u_char *key, const u_char *passwd,
	struct proc *p, struct ucred *cred)
{
	struct ncp_rq *rqp;
	struct mbchain *mbp;
	u_int32_t tmpID = htonl(object->object_id);
	u_char buf[16 + 8];
	u_char encrypted[8];
	int error;

	nw_keyhash((u_char*)&tmpID, passwd, strlen(passwd), buf);
	nw_encrypt(key, buf, encrypted);

	error = ncp_rq_alloc_subfn(23, 24, conn, p, cred, &rqp);
	if (error)
		return error;
	mbp = &rqp->rq;
	mb_put_mem(mbp, encrypted, 8, MB_MSYSTEM);
	mb_put_uint16be(mbp, object->object_type);
	ncp_rq_pstring(rqp, object->object_name);
	error = ncp_request(rqp);
	if (!error)
		ncp_rq_done(rqp);
	if ((conn->flags & NCPFL_SIGNWANTED) &&
	    (error == 0 || error == NWE_PASSWORD_EXPIRED)) {
		bcopy(key, buf + 16, 8);
		error = ncp_sign_start(conn, buf);
	}
	return error;
}

int
ncp_login_unencrypted(struct ncp_conn *conn, u_int16_t object_type, 
	const char *object_name, const u_char *passwd,
	struct proc *p, struct ucred *cred)
{
	struct ncp_rq *rqp;
	int error;

	error = ncp_rq_alloc_subfn(23, 20, conn, p, cred, &rqp);
	if (error)
		return error;
	mb_put_uint16be(&rqp->rq, object_type);
	ncp_rq_pstring(rqp, object_name);
	ncp_rq_pstring(rqp, passwd);
	error = ncp_request(rqp);
	if (!error)
		ncp_rq_done(rqp);
	return error;
}

int
ncp_read(struct ncp_conn *conn, ncp_fh *file, struct uio *uiop, struct ucred *cred)
{
	struct ncp_rq *rqp;
	struct mbchain *mbp;
	u_int16_t retlen = 0 ;
	int error = 0, len = 0, tsiz, burstio;

	tsiz = uiop->uio_resid;
#ifdef NCPBURST
	burstio = (ncp_burst_enabled && tsiz > conn->buffer_size);
#else
	burstio = 0;
#endif

	while (tsiz > 0) {
		if (!burstio) {
			len = min(4096 - (uiop->uio_offset % 4096), tsiz);
			len = min(len, conn->buffer_size);
			error = ncp_rq_alloc(72, conn, uiop->uio_procp, cred, &rqp);
			if (error)
				break;
			mbp = &rqp->rq;
			mb_put_uint8(mbp, 0);
			mb_put_mem(mbp, (caddr_t)file, 6, MB_MSYSTEM);
			mb_put_uint32be(mbp, uiop->uio_offset);
			mb_put_uint16be(mbp, len);
			rqp->nr_minrplen = 2;
			error = ncp_request(rqp);
			if (error)
				break;
			md_get_uint16be(&rqp->rp, &retlen);
			if (uiop->uio_offset & 1)
				md_get_mem(&rqp->rp, NULL, 1, MB_MSYSTEM);
			error = md_get_uio(&rqp->rp, uiop, retlen);
			ncp_rq_done(rqp);
		} else {
#ifdef NCPBURST
			error = ncp_burst_read(conn, file, tsiz, &len, &retlen, uiop, cred);
#endif
		}
		if (error)
			break;
		tsiz -= retlen;
		if (retlen < len)
			break;
	}
	return (error);
}

int
ncp_write(struct ncp_conn *conn, ncp_fh *file, struct uio *uiop, struct ucred *cred)
{
	struct ncp_rq *rqp;
	struct mbchain *mbp;
	int error = 0, len, tsiz, backup;

	if (uiop->uio_iovcnt != 1) {
		printf("%s: can't handle iovcnt>1 !!!\n", __func__);
		return EIO;
	}
	tsiz = uiop->uio_resid;
	while (tsiz > 0) {
		len = min(4096 - (uiop->uio_offset % 4096), tsiz);
		len = min(len, conn->buffer_size);
		if (len == 0) {
			printf("gotcha!\n");
		}
		/* rq head */
		error = ncp_rq_alloc(73, conn, uiop->uio_procp, cred, &rqp);
		if (error)
			break;
		mbp = &rqp->rq;
		mb_put_uint8(mbp, 0);
		mb_put_mem(mbp, (caddr_t)file, 6, MB_MSYSTEM);
		mb_put_uint32be(mbp, uiop->uio_offset);
		mb_put_uint16be(mbp, len);
		error = mb_put_uio(mbp, uiop, len);
		if (error) {
			ncp_rq_done(rqp);
			break;
		}
		error = ncp_request(rqp);
		if (!error)
			ncp_rq_done(rqp);
		if (len == 0)
			break;
		if (error) {
			backup = len;
			uiop->uio_iov->iov_base =
			    (char *)uiop->uio_iov->iov_base - backup;
			uiop->uio_iov->iov_len += backup;
			uiop->uio_offset -= backup;
			uiop->uio_resid += backup;
			break;
		}
		tsiz -= len;
	}
	if (error)
		uiop->uio_resid = tsiz;
	switch (error) {
	    case NWE_INSUFFICIENT_SPACE:
		error = ENOSPC;
		break;
	}
	return (error);
}
