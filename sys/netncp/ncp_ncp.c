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
 * $FreeBSD$
 *
 * Core of NCP protocol
 */
#include "opt_inet.h"
#include "opt_ipx.h"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/signalvar.h>
#include <sys/mbuf.h>

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_var.h>
#endif

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_sock.h>
#include <netncp/ncp_subr.h>
#include <netncp/ncp_ncp.h>
#include <netncp/ncp_rq.h>
#include <netncp/nwerror.h>

static int ncp_do_request(struct ncp_conn *,struct ncp_rq *rqp);
static int ncp_negotiate_buffersize(struct ncp_conn *conn, int size, int *target);
static int ncp_renegotiate_connparam(struct ncp_conn *conn, int buffsize, int in_options);
static void ncp_sign_packet(struct ncp_conn *conn, struct ncp_rq *rqp, int *size);


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
	tmpset = p->p_siglist;
	SIGSETNAND(tmpset, p->p_sigmask);
	SIGSETNAND(tmpset, p->p_sigignore);
	if (SIGNOTEMPTY(p->p_siglist) && NCP_SIGMASK(tmpset))
                return EINTR;
	return 0;
}

/*
 * Process initial NCP handshake (attach)
 * NOTE: Since all functions below may change conn attributes, they
 * should be called with LOCKED connection, also they use procp & ucred
 */
int
ncp_ncp_connect(struct ncp_conn *conn) {
	int error;
	struct ncp_rphdr *rp;
	DECLARE_RQ;
	
	conn->flags &= ~(NCPFL_INVALID | NCPFL_SIGNACTIVE | NCPFL_SIGNWANTED);
	conn->seq = 0;
	checkbad(ncp_rq_head(rqp,NCP_ALLOC_SLOT,0,conn->procp,conn->ucred));
	error=ncp_do_request(conn,rqp);
	if (!error) {
		rp = mtod(rqp->rp, struct ncp_rphdr*);
		conn->connid = rp->conn_low + (rp->conn_high << 8);
	}
	ncp_rq_done(rqp);
	if (error) return error;
	conn->flags |= NCPFL_ATTACHED;

	error = ncp_renegotiate_connparam(conn, NCP_DEFAULT_BUFSIZE, 0);
	if (error == NWE_SIGNATURE_LEVEL_CONFLICT) {
		printf("Unable to negotiate requested security level\n");
		error = EOPNOTSUPP;
	}
	if (error) {
		ncp_ncp_disconnect(conn);
		return error;
	}
#ifdef NCPBURST
	ncp_burst_connect(conn);
#endif
bad:
	return error;
}

int
ncp_ncp_disconnect(struct ncp_conn *conn) {
	int error;
	struct ncp_rqhdr *ncprq;
	DECLARE_RQ;

	NCPSDEBUG("for connid=%d\n",conn->nc_id);
#ifdef NCPBURST
	ncp_burst_disconnect(conn);
#endif
	error=ncp_rq_head(rqp,NCP_FREE_SLOT,0,conn->procp,conn->ucred);
	ncprq = mtod(rqp->rq,struct ncp_rqhdr*);
	error=ncp_do_request(conn,rqp);
	ncp_rq_done(rqp);
	ncp_conn_invalidate(conn);
	ncp_sock_disconnect(conn);
	return 0;
}
/* 
 * Make a signature for the current packet and add it at the end of the
 * packet.
 */
static void
ncp_sign_packet(struct ncp_conn *conn, struct ncp_rq *rqp, int *size) {
	u_char data[64];

	bzero(data, sizeof(data));
	bcopy(conn->sign_root, data, 8);
	setdle(data, 8, *size);
	m_copydata(rqp->rq, sizeof(struct ncp_rqhdr)-1,
		min((*size) - sizeof(struct ncp_rqhdr)+1, 52),data+12);
	ncp_sign(conn->sign_state, data, conn->sign_state);
	ncp_rq_mem(rqp, (void*)conn->sign_state, 8);
	(*size) += 8;
}

/*
 * Low level send rpc, here we do not attempt to restore any connection,
 * Connection expected to be locked
 */
static int 
ncp_do_request(struct ncp_conn *conn, struct ncp_rq *rqp) {
	int error=EIO,len, dosend, plen = 0, gotpacket, s;
	struct socket *so;
	struct proc *p = conn->procp;
	struct ncp_rqhdr *rq;
	struct ncp_rphdr *rp=NULL;
	struct timeval tv;
	struct mbuf *m, *mreply = NULL;
	
	conn->nc_rq = rqp;
	rqp->conn = conn;
	if (p == NULL)
		p = curproc;	/* XXX maybe procpage ? */
	if (!ncp_conn_valid(conn)) {
		printf("%s: conn not valid\n",__FUNCTION__);
		return (error);
	}
	so = conn->ncp_so;
	if (!so) {
		printf("%s: ncp_so is NULL !\n",__FUNCTION__);
		ncp_conn_invalidate(conn);	/* wow ! how we do that ? */
		return EBADF;
	}
	/*
	 * Flush out replies on previous reqs
	 */
	s = splnet();
	while (1/*so->so_rcv.sb_cc*/) {
		if (ncp_poll(so,POLLIN) == 0) break;
		if (ncp_sock_recv(so,&m,&len) != 0) break;
		m_freem(m);
	}
	rq = mtod(rqp->rq,struct ncp_rqhdr *);
	rq->seq = conn->seq;
	m = rqp->rq;
	len = 0;
	while (m) {
		len += m->m_len;
		m = m->m_next;
	}
	rqp->rq->m_pkthdr.len = len;
	switch(rq->fn) {
	    case 0x15: case 0x16: case 0x17: case 0x23:
		m = rqp->rq;
		*((u_int16_t*)(mtod(m,u_int8_t*)+sizeof(*rq))) = htons(len-2-sizeof(*rq));
		break;
	}
	if (conn->flags & NCPFL_SIGNACTIVE) {
		ncp_sign_packet(conn, rqp, &len);
		rqp->rq->m_pkthdr.len = len;
	}
	rq->conn_low = conn->connid & 0xff;
	/* rq->task = p->p_pgrp->pg_id & 0xff; */ /*p->p_pid*/
	/* XXX: this is temporary fix till I find a better solution */
	rq->task = rq->conn_low;
	rq->conn_high = conn->connid >> 8;
	rqp->rexmit = conn->li.retry_count;
	for(dosend = 1;;) {
		if (rqp->rexmit-- == 0) {
			error = ETIMEDOUT;
			break;
		}
		error = 0;
		if (dosend) {
			NCPSDEBUG("send:%04x f=%02x c=%d l=%d s=%d t=%d\n",rq->type, rq->fn, (rq->conn_high << 8) + rq->conn_low,
				rqp->rq->m_pkthdr.len, rq->seq, rq->task
			);
			error = ncp_sock_send(so, rqp->rq, rqp);
			if (error) break;
		}
		tv.tv_sec = conn->li.timeout;
		tv.tv_usec = 0;
		error = ncp_sock_rselect(so, p, &tv, POLLIN);
		if (error == EWOULDBLOCK )	/* timeout expired */
			continue;
		error = ncp_chkintr(conn, p);
		if (error == EINTR) 		/* we dont restart */
			break;
		if (error) break;
		/*
		 * At this point it is possible to get more than one
		 * reply from server. In general, last reply should be for
		 * current request, but not always. So, we loop through
		 * all replies to find the right answer and flush others.
		 */
		gotpacket = 0;	/* nothing good found */
		dosend = 1;	/* resend rq if error */
		for (;;) {
			error = 0;
			if (ncp_poll(so,POLLIN) == 0) break;
/*			if (so->so_rcv.sb_cc == 0) {
				break;
			}*/
			error = ncp_sock_recv(so,&m,&len);
			if (error) break; 		/* must be more checks !!! */
			if (m->m_len < sizeof(*rp)) {
				m = m_pullup(m, sizeof(*rp));
				if (m == NULL) {
					printf("%s: reply too short\n",__FUNCTION__);
					continue;
				}
			}
			rp = mtod(m, struct ncp_rphdr*);
			if (len == sizeof(*rp) && rp->type == NCP_POSITIVE_ACK) {
				NCPSDEBUG("got positive acknowledge\n");
				m_freem(m);
				rqp->rexmit = conn->li.retry_count;
				dosend = 0;	/* server just busy and will reply ASAP */
				continue;
			}
			NCPSDEBUG("recv:%04x c=%d l=%d s=%d t=%d cc=%02x cs=%02x\n",rp->type,
			    (rp->conn_high << 8) + rp->conn_low, len, rp->seq, rp->task,
			     rp->completion_code, rp->connection_state);
			NCPDDEBUG(m);
			if ( (rp->type == NCP_REPLY) && 
			    ((rq->type == NCP_ALLOC_SLOT) || 
			    ((rp->conn_low == rq->conn_low) &&
			     (rp->conn_high == rq->conn_high)
			    ))) {
				if (rq->seq > rp->seq || (rq->seq == 0 && rp->seq == 0xff)) {
					dosend = 1;
				}
				if (rp->seq == rq->seq) {
					if (gotpacket) {
						m_freem(m);
					} else {
						gotpacket = 1;
						mreply = m;
						plen = len;
					}
					continue;	/* look up other for other packets */
				}
			}
			m_freem(m);
			NCPSDEBUG("reply mismatch\n");
		} /* for receive */
		if (error) break;
		if (gotpacket) break;
		/* try to resend, or just wait */
	}
	splx(s);
	conn->seq++;
	if (error) {
		NCPSDEBUG("error=%d\n",error);
		if (error != EINTR)			/* if not just interrupt */
			ncp_conn_invalidate(conn);	/* only reconnect to restore */
		return(error);
	}
	if (conn->flags & NCPFL_SIGNACTIVE) {
		/* XXX: check reply signature */
		m_adj(mreply, -8);
		plen -= 8;
	}
	len = plen;
	m = mreply;
	rp = mtod(m, struct ncp_rphdr*);
	len -= sizeof(*rp);
	rqp->rpsize = len;
	rqp->cc = error = rp->completion_code;
	if (error) error |= 0x8900;	/* server error */
	rqp->cs = rp->connection_state;
	if (rqp->cs & (NCP_CS_BAD_CONN | NCP_CS_SERVER_DOWN)) {
		NCPSDEBUG("server drop us\n");
		ncp_conn_invalidate(conn);
		error = ECONNRESET;
	}
	rqp->rp = m;
	rqp->mrp = m;
	rqp->bpos = mtod(m, caddr_t) + sizeof(*rp);
	return error;
}

/*
 * Here we will try to restore any loggedin & dropped connection,
 * connection should be locked on entry
 */
int ncp_restore_login(struct ncp_conn *conn);
int
ncp_restore_login(struct ncp_conn *conn) {
	int error, oldflags;

	if (conn->flags & NCPFL_RESTORING) {
		printf("Hey, ncp_restore_login called twise !!!\n");
		return 0;
	}
	oldflags = conn->flags;
	printf("Restoring connection, flags = %d\n",oldflags);
	if ((oldflags & NCPFL_LOGGED) == 0) {
		return ECONNRESET;	/* no need to restore empty conn */
	}
	conn->flags &= ~(NCPFL_LOGGED | NCPFL_ATTACHED);
	conn->flags |= NCPFL_RESTORING;
	do {	/* not a loop */
		error = ncp_reconnect(conn);
		if (error) break;
		if (conn->li.user)
			error = ncp_login_object(conn, conn->li.user, conn->li.objtype, conn->li.password,conn->procp,conn->ucred);
		if (error) break;
		conn->flags |= NCPFL_LOGGED;
	} while(0);
	if (error) {
		conn->flags = oldflags | NCPFL_INVALID;
	}
	conn->flags &= ~NCPFL_RESTORING;
	return error;
}

int
ncp_request(struct ncp_conn *conn, struct ncp_rq *rqp) {
	int error, rcnt;
/*	struct ncp_rqhdr *rq = mtod(rqp->rq,struct ncp_rqhdr*);*/

	error = ncp_conn_lock(conn,rqp->p,rqp->cred,NCPM_EXECUTE);
	if  (error) return error;
	rcnt = NCP_RESTORE_COUNT;
	for(;;) {
		if (!ncp_conn_valid(conn)) {
			if (rcnt==0) {
				error = ECONNRESET;
				break;
			}
			rcnt--;
			error = ncp_restore_login(conn);
			if (error)
				continue;
		}
		error=ncp_do_request(conn, rqp);
		if (ncp_conn_valid(conn))	/* not just error ! */
			break;
	}
	ncp_conn_unlock(conn,rqp->p);
	return error;
}

/*
 * All negotiation functions expect a locked connection
 */
static int
ncp_negotiate_buffersize(struct ncp_conn *conn, int size, int *target) {
	int error;
	DECLARE_RQ;

	NCP_RQ_HEAD(0x21,conn->procp,conn->ucred);
	ncp_rq_word_hl(rqp, size);
	checkbad(ncp_request(conn,rqp));
	*target = min(ncp_rp_word_hl(rqp), size);
	NCP_RQ_EXIT;
	return error;
}

static int
ncp_negotiate_size_and_options(struct ncp_conn *conn, int size, int options, 
	    int *ret_size, int *ret_options) {
	int error;
	int rs;
	DECLARE_RQ;

	NCP_RQ_HEAD(0x61,conn->procp,conn->ucred);
	ncp_rq_word_hl(rqp, size);
	ncp_rq_byte(rqp, options);
	checkbad(ncp_request(conn, rqp));
	rs = ncp_rp_word_hl(rqp);
	*ret_size = (rs == 0) ? size : min(rs, size);
	ncp_rp_word_hl(rqp);	/* skip echo socket */
	*ret_options = ncp_rp_byte(rqp);
	NCP_RQ_EXIT;
	return error;
}

static int
ncp_renegotiate_connparam(struct ncp_conn *conn, int buffsize, int in_options)
{
	int neg_buffsize, error, options, sl;

	sl = conn->li.sig_level;
	if (sl >= 2)
		in_options |= NCP_SECURITY_LEVEL_SIGN_HEADERS;
#ifdef IPX
	if (ipxcksum == 2)
		in_options |= NCP_IPX_CHECKSUM;
#endif
	error = ncp_negotiate_size_and_options(conn, buffsize, in_options, 
	    &neg_buffsize, &options);
	if (!error) {
#ifdef IPX
		if ((options ^ in_options) & NCP_IPX_CHECKSUM) {
			if (ipxcksum == 2) {
				printf("Server refuses to support IPX checksums\n");
				return NWE_REQUESTER_FAILURE;
			}
			in_options |= NCP_IPX_CHECKSUM;
			error = 1;
		}
#endif /* IPX */
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
#ifdef IPX
	ncp_sock_checksum(conn, in_options & NCP_IPX_CHECKSUM);
#endif
	return 0;
}

int
ncp_reconnect(struct ncp_conn *conn) {
	int error;

	/* close any open sockets */
	ncp_sock_disconnect(conn);
	switch( conn->li.saddr.sa_family ) {
#ifdef IPX
	    case AF_IPX:
		error = ncp_sock_connect_ipx(conn);
		break;
#endif
#ifdef INET
	    case AF_INET:
		error = ncp_sock_connect_in(conn);
		break;
#endif
	    default:
		return EPROTONOSUPPORT;
	}
	if (!error)
		error = ncp_ncp_connect(conn);
	return error;
}

/*
 * Create conn structure and try to do low level connect
 * Server addr should be filled in.
 */
int
ncp_connect(struct ncp_conn_args *li, struct proc *p, struct ucred *cred,
	struct ncp_conn **aconn)
{
	struct ncp_conn *conn;
	struct ucred *owner;
	int error, isroot;

	if (li->saddr.sa_family != AF_INET && li->saddr.sa_family != AF_IPX)
		return EPROTONOSUPPORT;
	isroot = ncp_suser(cred) == 0;
	/*
	 * Only root can change ownership
	 */
	if (li->owner != NCP_DEFAULT_OWNER && !isroot)
		return EPERM;
	if (li->group != NCP_DEFAULT_GROUP &&
	    !groupmember(li->group, cred) && !isroot)
		return EPERM;
	if (li->owner != NCP_DEFAULT_OWNER) {
		owner = crget();
		owner->cr_uid = li->owner;
	} else {
		owner = cred;
		crhold(owner);
	}
	error = ncp_conn_alloc(p, owner, &conn);
	if (error)
		return (error);
	if (error) {
		ncp_conn_free(conn);
		return error;
	}
	conn->li = *li;
	conn->nc_group = (li->group != NCP_DEFAULT_GROUP) ? 
		li->group : cred->cr_groups[0];

	if (li->retry_count == 0)
		conn->li.retry_count = NCP_RETRY_COUNT;
	if (li->timeout == 0)
		conn->li.timeout = NCP_RETRY_TIMEOUT;
	error = ncp_reconnect(conn);
	if (error) {
		ncp_disconnect(conn);
	} else {
		*aconn=conn;
	}
	return error;
}
/*
 * Break connection and deallocate memory
 */
int
ncp_disconnect(struct ncp_conn *conn) {

	if (ncp_conn_access(conn,conn->ucred,NCPM_WRITE))
		return EACCES;
	if (conn->ref_cnt != 0) return EBUSY;
	if (conn->flags & NCPFL_PERMANENT) return EBUSY;
	if (ncp_conn_valid(conn)) {
		ncp_ncp_disconnect(conn);
	}
	ncp_sock_disconnect(conn);
	ncp_conn_free(conn);
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
