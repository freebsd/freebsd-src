/*-
 * Copyright (c) 2005-2007 Daniel Braniss <danny@cs.huji.ac.il>
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
 *
 */
/*
 | iSCSI
 | $Id: isc_soc.c,v 1.26 2007/05/19 06:09:01 danny Exp danny $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/iscsi/initiator/isc_soc.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_iscsi_initiator.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/ctype.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/socketvar.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/proc.h>
#include <sys/ioccom.h>
#include <sys/queue.h>
#include <sys/kthread.h>
#include <sys/syslog.h>
#include <sys/mbuf.h>
#include <sys/user.h>

#include <dev/iscsi/initiator/iscsi.h>
#include <dev/iscsi/initiator/iscsivar.h>

#ifndef USE_MBUF
#define USE_MBUF
#endif

#ifdef USE_MBUF
/*
 |  a dummy function for freeing external storage for mbuf
 */
static void
nil_fn(void *a, void *b)
{
}
static int nil_refcnt = 0;
#endif /* USE_MBUF */

int
isc_sendPDU(isc_session_t *sp, pduq_t *pq)
{
     pdu_t		*pp = &pq->pdu;
     int		len, error;
#ifdef USE_MBUF
     struct mbuf        *mh, **mp;
#else
     struct uio		*uio = &pq->uio;
     struct iovec	*iv;
#endif /* USE_MBUF */

     debug_called(8);

#ifndef USE_MBUF
     bzero(uio, sizeof(struct uio));
     uio->uio_rw	= UIO_WRITE;
     uio->uio_segflg	= UIO_SYSSPACE;
     uio->uio_td	= sp->td;
     uio->uio_iov 	= iv = pq->iov;

     iv->iov_base	= &pp->ipdu;
     iv->iov_len	= sizeof(union ipdu_u);
     uio->uio_resid	= pq->len;
     iv++;
#else /* USE_MBUF */
     /*  mbuf for the iSCSI header */
     MGETHDR(mh, M_TRYWAIT, MT_DATA);
     mh->m_len = mh->m_pkthdr.len = sizeof(union ipdu_u);
     mh->m_pkthdr.rcvif = NULL;
     MH_ALIGN(mh, sizeof(union ipdu_u));
     bcopy(&pp->ipdu, mh->m_data, sizeof(union ipdu_u));
     mh->m_next = NULL;
#endif /* USE_MBUF */

     if(sp->hdrDigest)
	  pq->pdu.hdr_dig = sp->hdrDigest(&pp->ipdu, sizeof(union ipdu_u), 0);
     if(pp->ahs_len) {
#ifndef USE_MBUF
	  iv->iov_base	= pp->ahs;
	  iv->iov_len	= pp->ahs_len;
	  iv++;
#else /* USE_MBUF */
          /* Add any AHS to the iSCSI hdr mbuf */
          /* XXX Assert: (mh->m_pkthdr.len + pp->ahs_len) < MHLEN */
          bcopy(pp->ahs, (mh->m_data + mh->m_len), pp->ahs_len);
          mh->m_len += pp->ahs_len;
          mh->m_pkthdr.len += pp->ahs_len;
#endif /* USE_MBUF */
	  if(sp->hdrDigest)
	       pq->pdu.hdr_dig = sp->hdrDigest(&pp->ahs, pp->ahs_len, pq->pdu.hdr_dig);
     }
     if(sp->hdrDigest) {
	  debug(2, "hdr_dig=%x", pq->pdu.hdr_dig);
#ifndef USE_MBUF
	  iv->iov_base	= &pp->hdr_dig;
	  iv->iov_len	= sizeof(int);
	  iv++;
#else /* USE_MBUF */
          /* Add header digest to the iSCSI hdr mbuf */ 
          /* XXX Assert: (mh->m_pkthdr.len + 4) < MHLEN */
          bcopy(&pp->hdr_dig, (mh->m_data + mh->m_len), sizeof(int));
          mh->m_len += sizeof(int);
          mh->m_pkthdr.len += sizeof(int);
#endif /* USE_MBUF */
     }
#ifdef USE_MBUF
     mp = &mh->m_next;
#endif /* USE_MBUF */
     if(pq->pdu.ds) {
#ifndef USE_MBUF
	  iv->iov_base	= pp->ds;
	  iv->iov_len	= pp->ds_len;
	  while(iv->iov_len & 03) // the specs say it must be int alligned
	       iv->iov_len++;
	  iv++;
#else /* USE_MBUF */
          struct mbuf   *md;
          int           off = 0;

          len = pp->ds_len;
	  while(len & 03) // the specs say it must be int alligned
	       len++;

          while (len > 0) {
                int       l;
          
                MGET(md, M_TRYWAIT, MT_DATA);
		md->m_ext.ref_cnt = &nil_refcnt;
                l = min(MCLBYTES, len);
                MEXTADD(md, pp->ds + off, l, nil_fn,
                        NULL, 0, EXT_EXTREF);
                md->m_len = l;
                md->m_next = NULL;
                mh->m_pkthdr.len += l;
                *mp = md;
                mp = &md->m_next;

                len -= l;
                off += l;
          } 
#endif /* USE_MBUF */
     }
     if(sp->dataDigest) {
#ifdef USE_MBUF
          struct mbuf   *me;

#endif /* USE_MBUF */
	  pp->ds_dig = sp->dataDigest(pp->ds, pp->ds_len, 0);
#ifndef USE_MBUF
	  iv->iov_base	= &pp->ds_dig;
	  iv->iov_len	= sizeof(int);
	  iv++;
#else /* USE_MBUF */
          MGET(me, M_TRYWAIT, MT_DATA);
          me->m_len = sizeof(int);
          MH_ALIGN(mh, sizeof(int));
          bcopy(&pp->ds_dig, me->m_data, sizeof(int));
          me->m_next = NULL;
     
          mh->m_pkthdr.len += sizeof(int);
          *mp = me;
#endif /* USE_MBUF */
     }

#ifndef USE_MBUF
     uio->uio_iovcnt	= iv - pq->iov;
     sdebug(5, "opcode=%x iovcnt=%d uio_resid=%d itt=%x",
	    pp->ipdu.bhs.opcode, uio->uio_iovcnt, uio->uio_resid,
	    ntohl(pp->ipdu.bhs.itt));
     sdebug(5, "sp=%p sp->soc=%p uio=%p sp->td=%p",
	    sp, sp->soc, uio, sp->td);

     do {
	  len = uio->uio_resid;
	  error = sosend(sp->soc, NULL, uio, 0, 0, 0, sp->td);
	  if(uio->uio_resid == 0 || error || len == uio->uio_resid) {
	       if(uio->uio_resid) {
		    sdebug(2, "uio->uio_resid=%d uio->uio_iovcnt=%d error=%d len=%d",
			   uio->uio_resid, uio->uio_iovcnt, error, len);
		    if(error == 0)
			 error = EAGAIN; // 35
	       }
	       break;
	  }
	  /*
	   | XXX: untested code
	   */
	  sdebug(1, "uio->uio_resid=%d uio->uio_iovcnt=%d",
		uio->uio_resid, uio->uio_iovcnt);
	  iv = uio->uio_iov;
	  len -= uio->uio_resid;
	  while(uio->uio_iovcnt > 0) {
	       if(iv->iov_len > len) {
		    caddr_t	bp = (caddr_t)iv->iov_base;

		    iv->iov_len -= len;
		    iv->iov_base = (void *)&bp[len];
		    break;
	       }
	       len -= iv->iov_len;
	       uio->uio_iovcnt--;
	       uio->uio_iov++;
	       iv++;
	  }
     } while(uio->uio_resid);

     if(error == 0) {
	  sp->stats.nsent++;
	  getbintime(&sp->stats.t_sent);
#else /* USE_MBUF */
     if ((error = sosend(sp->soc, NULL, NULL, mh, 0, 0, sp->td)) != 0) {
           m_freem(mh);
           return (error);
#endif /* USE_MBUF */
     }
#ifndef USE_MBUF
     return error;
#else /* USE_MBUF */
     sp->stats.nsent++;
     getbintime(&sp->stats.t_sent);
     return 0;
#endif /* USE_MBUF */
}

/*
 | wait till a PDU header is received
 | from the socket.
 */
/*
   The format of the BHS is:

   Byte/     0       |       1       |       2       |       3       |
      /              |               |               |               |
     |0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|
     +---------------+---------------+---------------+---------------+
    0|.|I| Opcode    |F|  Opcode-specific fields                     |
     +---------------+---------------+---------------+---------------+
    4|TotalAHSLength | DataSegmentLength                             |
     +---------------+---------------+---------------+---------------+
    8| LUN or Opcode-specific fields                                 |
     +                                                               +
   12|                                                               |
     +---------------+---------------+---------------+---------------+
   16| Initiator Task Tag                                            |
     +---------------+---------------+---------------+---------------+
   20/ Opcode-specific fields                                        /
    +/                                                               /
     +---------------+---------------+---------------+---------------+
   48
 */
static __inline int
so_getbhs(isc_session_t *sp)
{
     bhs_t *bhs		= &sp->bhs;
     struct uio		*uio = &sp->uio;
     struct iovec	*iov = &sp->iov;
     int		error, flags;

     debug_called(8);

     iov->iov_base	= bhs;
     iov->iov_len	= sizeof(bhs_t);

     uio->uio_iov	= iov;
     uio->uio_iovcnt	= 1;
     uio->uio_rw	= UIO_READ;
     uio->uio_segflg	= UIO_SYSSPACE;
     uio->uio_td	= curthread; // why ...
     uio->uio_resid	= sizeof(bhs_t);

     flags = MSG_WAITALL;
     error = soreceive(sp->soc, NULL, uio, 0, 0, &flags);

     if(error)
	  debug(2, "error=%d so_error=%d uio->uio_resid=%d iov.iov_len=%zd",
		error,
		sp->soc->so_error, uio->uio_resid, iov->iov_len);
     if(!error && (uio->uio_resid > 0)) {
	  debug(2, "error=%d so_error=%d uio->uio_resid=%d iov.iov_len=%zd so_state=%x",
		error,
		sp->soc->so_error, uio->uio_resid, iov->iov_len, sp->soc->so_state);
	  error = EAGAIN; // EPIPE;
     }
	  
     return error;
}

/*
 | so_recv gets called when there is at least
 | an iSCSI header in the queue
 */
static int
so_recv(isc_session_t *sp, pduq_t *pq)
{
     struct socket	*so = sp->soc;
     sn_t		*sn = &sp->sn;
     struct uio		*uio = &pq->uio;
     pdu_t		*pp;
     int		error;
     size_t		n, len;
     bhs_t		*bhs;
     u_int		max, exp;

     debug_called(8);
     /*
      | now calculate how much data should be in the buffer
      | NOTE: digest is not verified/calculated - yet
      */
     pp = &pq->pdu;
     bhs = &pp->ipdu.bhs;

     len = 0;
     if(bhs->AHSLength) {
	  pp->ahs_len = bhs->AHSLength * 4;
	  len += pp->ahs_len;
     }
     if(sp->hdrDigest)
	  len += 4;
     if(bhs->DSLength) {
	  n = bhs->DSLength;
#if BYTE_ORDER == LITTLE_ENDIAN
	  pp->ds_len = ((n & 0x00ff0000) >> 16)
	       | (n & 0x0000ff00)
	       | ((n & 0x000000ff) << 16);
#else
	  pp->ds_len = n;
#endif
	  len += pp->ds_len;
	  while(len & 03)
	       len++;
	  if(sp->dataDigest)
	       len += 4;
     }

     if((sp->opt.maxRecvDataSegmentLength > 0) && (len > sp->opt.maxRecvDataSegmentLength)) {
#if 0
	  xdebug("impossible PDU length(%d) opt.maxRecvDataSegmentLength=%d",
		 len, sp->opt.maxRecvDataSegmentLength);
	  // deep trouble here, probably all we can do is
	  // force a disconnect, XXX: check RFC ...
	  log(LOG_ERR,
	      "so_recv: impossible PDU length(%ld) from iSCSI %s/%s\n",
	      len, sp->opt.targetAddress, sp->opt.targetName);
#endif
	  /*
	   | XXX: this will realy screwup the stream.
	   | should clear up the buffer till a valid header
	   | is found, or just close connection ...
	   | should read the RFC.
	   */
	  error = E2BIG;
	  goto out;
     }
     if(len) {
	  int	flags;

	  uio->uio_resid = len;
	  uio->uio_td = curthread; // why ...
	  flags = MSG_WAITALL;

	  error = soreceive(so, NULL, uio, &pq->mp, NULL, &flags);
	  //if(error == EAGAIN)
	  // XXX: this needs work! it hangs iscontrol
	  if(error || uio->uio_resid)
	       goto out;
     }
     pq->len += len;
     sdebug(6, "len=%d] opcode=0x%x ahs_len=0x%x ds_len=0x%x",
	    pq->len, bhs->opcode, pp->ahs_len, pp->ds_len);

     max = ntohl(bhs->MaxCmdSN);
     exp = ntohl(bhs->ExpStSN);

     if(max < exp - 1 &&
	max > exp - _MAXINCR) {
	  sdebug(2,  "bad cmd window size");
	  error = EIO; // XXX: for now;
	  goto out; // error
     }

     if(SNA_GT(max, sn->maxCmd))
	  sn->maxCmd = max;

     if(SNA_GT(exp, sn->expCmd))
	  sn->expCmd = exp;

     sp->cws = sn->maxCmd - sn->expCmd + 1;

     return 0;

 out:
     // XXX: need some work here
     xdebug("have a problem, error=%d", error);
     pdu_free(sp->isc, pq);
     if(!error && uio->uio_resid > 0)
	  error = EPIPE;
     return error;
}
/*
 | wait for something to arrive.
 | and if the pdu is without errors, process it.
 */
static int
so_input(isc_session_t *sp)
{
     pduq_t		*pq;
     int		error;

     debug_called(8);
     /*
      | first read in the iSCSI header
      */
     error = so_getbhs(sp);
     if(error == 0) {
	  /*
	   | now read the rest.
	   */
	  pq = pdu_alloc(sp->isc, 1);  // OK to WAIT
	  pq->pdu.ipdu.bhs = sp->bhs;
	  pq->len = sizeof(bhs_t);	// so far only the header was read
	  error = so_recv(sp, pq);
	  if(error != 0) {
	       error += 0x800; // XXX: just to see the error.
	       // terminal error
	       // XXX: close connection and exit
	  }
	  else {
	       sp->stats.nrecv++;
	       getbintime(&sp->stats.t_recv);
	       ism_recv(sp, pq);
	  }
     }
     return error;
}

/*
 | one per active (connected) session.
 | this thread is responsible for reading
 | in packets from the target.
 */
static void
isc_soc(void *vp)
{
     isc_session_t	*sp = (isc_session_t *)vp;
     struct socket	*so = sp->soc;
     int		error;

     debug_called(8);

     sp->flags |= ISC_CON_RUNNING;

     if(sp->cam_path)
	  ic_release(sp);

     error = 0;
     while(sp->flags & ISC_CON_RUN) {
	  // XXX: hunting ...
	  if(sp->soc == NULL || !(so->so_state & SS_ISCONNECTED)) {
	       debug(2, "sp->soc=%p", sp->soc);
	       break;
	  }
	  error = so_input(sp);
	  if(error == 0) {
#ifdef ISC_OWAITING
	       mtx_lock(&sp->io_mtx);
	       if(sp->flags & ISC_OWAITING) {
		    sp->flags &= ~ISC_OWAITING;
	       }
	       wakeup(&sp->flags);
	       mtx_unlock(&sp->io_mtx);
#else
	       wakeup(&sp->flags);
#endif

	  } else if(error == EPIPE)
	       break;
	  else if(error == EAGAIN) {
	       if(so->so_state & SS_ISCONNECTED) 
		    // there seems to be a problem in 6.0 ...
		    tsleep(sp, PRIBIO, "isc_soc", 2*hz);
	  }
     }
     sdebug(2, "terminated, flags=%x so_count=%d so_state=%x error=%d",
	    sp->flags, so->so_count, so->so_state, error);
     if((sp->proc != NULL) && sp->signal) {
	  PROC_LOCK(sp->proc);
	  psignal(sp->proc, sp->signal);
	  PROC_UNLOCK(sp->proc);
	  sp->flags |= ISC_SIGNALED;
	  sdebug(2, "pid=%d signaled(%d)", sp->proc->p_pid, sp->signal);
     }
     else {
	  // we have to do something ourselves
	  // like closing this session ...
     }
     /*
      | we've been terminated
      */
     // do we need this mutex ...?
     mtx_lock(&sp->io_mtx);
     sp->flags &= ~ISC_CON_RUNNING;
     wakeup(&sp->soc);

     mtx_unlock(&sp->io_mtx);

     kthread_exit(0);
}

void
isc_stop_receiver(isc_session_t *sp)
{
     int n = 5;
     debug_called(8);

     sdebug(4, "sp=%p sp->soc=%p", sp, sp? sp->soc: 0);
     soshutdown(sp->soc, SHUT_RD);

     mtx_lock(&sp->io_mtx);
     sp->flags &= ~ISC_CON_RUN;
     while(n-- && (sp->flags & ISC_CON_RUNNING)) {
	  sdebug(3, "waiting n=%d... flags=%x", n, sp->flags);
	  msleep(&sp->soc, &sp->io_mtx, PRIBIO, "isc_stpc", 5*hz);
     }
     mtx_unlock(&sp->io_mtx);

     if(sp->fp != NULL)
	  fdrop(sp->fp, sp->td);
     fputsock(sp->soc);
     sp->soc = NULL;
     sp->fp = NULL;
}

void
isc_start_receiver(isc_session_t *sp)
{
     debug_called(8);

     sp->flags |= ISC_CON_RUN;
     kthread_create(isc_soc, sp, &sp->soc_proc, 0, 0, "iscsi%d", sp->sid);
}
