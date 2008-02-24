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
 | iSCSI - Session Manager
 | $Id: isc_sm.c,v 1.30 2007/04/22 09:53:09 danny Exp danny $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/iscsi/initiator/isc_sm.c,v 1.1 2007/07/24 15:35:02 scottl Exp $");

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
#include <sys/bus.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_periph.h>

#include <dev/iscsi/initiator/iscsi.h>
#include <dev/iscsi/initiator/iscsivar.h>

static void
_async(isc_session_t *sp, pduq_t *pq)
{
     debug_called(8);

     iscsi_async(sp, pq);

     pdu_free(sp->isc, pq);
}

static void
_reject(isc_session_t *sp, pduq_t *pq)
{
     pduq_t	*opq;
     pdu_t	*pdu;
     reject_t	*reject;
     int	itt;

     debug_called(8);
     pdu = mtod(pq->mp, pdu_t *);
     itt = pdu->ipdu.bhs.itt;
     reject = &pq->pdu.ipdu.reject;
     sdebug(2, "itt=%x reason=0x%x", ntohl(itt), reject->reason);
     opq = i_search_hld(sp, itt, 0);
     if(opq != NULL)
	  iscsi_reject(sp, opq, pq);
     else {
	  switch(pq->pdu.ipdu.bhs.opcode) {
	  case ISCSI_LOGOUT_CMD: // XXX: wasabi does this - can't figure out why
	       sdebug(2, "ISCSI_LOGOUT_CMD ...");
	       break;
	  default:
	       xdebug("%d] we lost something itt=%x",
		      sp->sid, ntohl(pq->pdu.ipdu.bhs.itt));
	  }
     }
     pdu_free(sp->isc, pq);
}

static void
_r2t(isc_session_t *sp, pduq_t *pq)
{
     pduq_t	*opq;

     debug_called(8);
     opq = i_search_hld(sp, pq->pdu.ipdu.bhs.itt, 1);
     if(opq != NULL) {
	  iscsi_r2t(sp, opq, pq);
     } 
     else {
	  r2t_t		*r2t = &pq->pdu.ipdu.r2t;

	  xdebug("%d] we lost something itt=%x r2tSN=%d bo=%x ddtl=%x",
		 sp->sid, ntohl(pq->pdu.ipdu.bhs.itt),
		 ntohl(r2t->r2tSN), ntohl(r2t->bo), ntohl(r2t->ddtl));
     }
     pdu_free(sp->isc, pq);
}

static void
_scsi_rsp(isc_session_t *sp, pduq_t *pq)
{
     pduq_t	*opq;

     debug_called(8);
     opq = i_search_hld(sp, pq->pdu.ipdu.bhs.itt, 0);
     debug(5, "itt=%x pq=%p opq=%p", ntohl(pq->pdu.ipdu.bhs.itt), pq, opq);
     if(opq != NULL)
	  iscsi_done(sp, opq, pq);
     else
	  xdebug("%d] we lost something itt=%x",
		 sp->sid, ntohl(pq->pdu.ipdu.bhs.itt));
     pdu_free(sp->isc, pq);
}

static void
_read_data(isc_session_t *sp, pduq_t *pq)
{
     pduq_t		*opq;

     debug_called(8);
     opq = i_search_hld(sp, pq->pdu.ipdu.bhs.itt, 1);
     if(opq != NULL) {
	  if(scsi_decap(sp, opq, pq) != 1) {
	       i_remove_hld(sp, opq); // done
	       pdu_free(sp->isc, opq);
	  }
     }
     else
	  xdebug("%d] we lost something itt=%x",
		 sp->sid, ntohl(pq->pdu.ipdu.bhs.itt));
     pdu_free(sp->isc, pq);
}
/*
 | this is a kludge,
 | the jury is not back with a veredict, user or kernel
 */
static void
_nop_out(isc_session_t *sp)
{
     pduq_t	*pq;
     nop_out_t	*nop_out;

     debug_called(8);

     sdebug(4, "cws=%d", sp->cws);
     if(sp->cws == 0) {
	  /*
	   | only send a nop if window is closed.
	   */
	  if((pq = pdu_alloc(sp->isc, 0)) == NULL)
	       // I guess we ran out of resources
	       return;
	  nop_out = &pq->pdu.ipdu.nop_out;
	  nop_out->opcode = ISCSI_NOP_OUT;
	  nop_out->itt = htonl(sp->sn.itt);
	  nop_out->ttt = -1;
	  nop_out->I = 1;
	  nop_out->F = 1;
	  if(isc_qout(sp, pq) != 0) {
	       sdebug(1, "failed");
	       pdu_free(sp->isc, pq);
	  }
     }
}

static void
_nop_in(isc_session_t *sp, pduq_t *pq)
{
     pdu_t	*pp = &pq->pdu;
     nop_in_t	*nop_in = &pp->ipdu.nop_in;
     bhs_t	*bhs = &pp->ipdu.bhs;

     debug_called(8);

     sdebug(5, "itt=%x ttt=%x", htonl(nop_in->itt), htonl(nop_in->ttt));
     if(nop_in->itt == -1) {
	  if(pp->ds_len != 0) {
	       /*
		| according to RFC 3720 this should be zero
		| what to do if not?
		*/
	       xdebug("%d] dslen not zero", sp->sid);
	  }
	  if(nop_in->ttt != -1) {
	       nop_out_t	*nop_out;
	       /*
		| target wants a nop_out
	        */
	       bhs->opcode = ISCSI_NOP_OUT;
	       bhs->I = 1;
	       bhs->F = 1;
	       /*
		| we are reusing the pdu, so bhs->ttt == nop_in->ttt;
		| and need to zero out 'Reserved'
		| small cludge here.
	        */
	       nop_out = &pp->ipdu.nop_out;
	       nop_out->sn.maxcmd = 0;
	       memset(nop_out->mbz, 0, sizeof(nop_out->mbz));

	       (void)isc_qout(sp, pq); //XXX: should check return?
	       return;
	  }
	  //else {
	       // just making noise?
	       // see 10.9.1: target does not want and answer.
	  //}

     } else
     if(nop_in->ttt == -1) {
	  /*
	   | it is an answer to a nop_in from us
	   */
	  if(nop_in->itt != -1) {
#ifdef ISC_WAIT4PING
	       // XXX: MUTEX please
	       if(sp->flags & ISC_WAIT4PING) {
		    i_nqueue_rsp(sp, pq);
		    wakeup(&sp->rsp);
		    return;
	       }
#endif
	  }
     }
     /*
      | drop it
      */
     pdu_free(sp->isc, pq);
     return;
}

int
i_prepPDU(isc_session_t *sp, pduq_t *pq)
{
     size_t	len, n;
     pdu_t	*pp = &pq->pdu;
     bhs_t	*bhp = &pp->ipdu.bhs;

     len = sizeof(bhs_t);
     if(pp->ahs_len) {
	  len += pp->ahs_len;
	  bhp->AHSLength =  pp->ahs_len / 4;
     }
     if(sp->hdrDigest)
	  len += 4;
     if(pp->ds_len) {
	  n = pp->ds_len;
	  len += n;
#if BYTE_ORDER == LITTLE_ENDIAN
	  bhp->DSLength = ((n & 0x00ff0000) >> 16)
	       | (n & 0x0000ff00)
	       | ((n & 0x000000ff) << 16);
#else
	  bhp->DSLength = n;
#endif
	  if(len & 03) {
	       n = 4 - (len & 03);
	       len += n;
	  }
	  if(sp->dataDigest)
	       len += 4;
     }

     pq->len = len;
     len -= sizeof(bhs_t);
     if(sp->opt.maxBurstLength && (len > sp->opt.maxBurstLength)) {
	  xdebug("%d] pdu len=%zd > %d",
		 sp->sid, len, sp->opt.maxBurstLength);
	  // XXX: when this happens it used to hang ...
	  return E2BIG;
     }
     return 0;
}

int
isc_qout(isc_session_t *sp, pduq_t *pq)
{
     int error = 0;

     debug_called(8);

     if(pq->len == 0 && (error = i_prepPDU(sp, pq)))
	  return error;

     if(pq->pdu.ipdu.bhs.I)
	  i_nqueue_isnd(sp, pq);
     else
     if(pq->pdu.ipdu.data_out.opcode == ISCSI_WRITE_DATA)
	  i_nqueue_wsnd(sp, pq);
     else
	  i_nqueue_csnd(sp, pq);

     sdebug(5, "enqued: pq=%p", pq);
#ifdef ISC_OWAITING
     if(sp->flags & ISC_OWAITING) {
	  mtx_lock(&sp->io_mtx);	// XXX
	  wakeup(&sp->flags);
	  mtx_unlock(&sp->io_mtx);	// XXX
     }
#else
     wakeup(&sp->flags);
#endif
     return error;
}
/*
 | called when a fullPhase is restarted
 */
static int
ism_restart(isc_session_t *sp)
{
     int lastcmd;

     sdebug(2, "restart ...");
     sp->flags |= ISC_SM_HOLD;
     lastcmd = iscsi_requeue(sp);
#if 0
     if(lastcmd != sp->sn.cmd) {
	  sdebug(1, "resetting CmdSN to=%d (from %d)", lastcmd, sp->sn.cmd);
	  sp->sn.cmd = lastcmd;
     }
#endif
     sp->flags &= ~ISC_SM_HOLD;
     return 0;
}

int
ism_fullfeature(struct cdev *dev, int flag)
{
     isc_session_t *sp = (isc_session_t *)dev->si_drv2;
     int	error;

     sdebug(2, "flag=%d", flag);

     error = 0;
     switch(flag) {
     case 0: // stop
	  sp->flags &= ~ISC_FFPHASE;
	  break;
     case 1: // start
	  error = ic_fullfeature(dev);
	  break;
     case 2: // restart
	  error = ism_restart(sp);
	  break;
     }
     return error;
}

void
ism_recv(isc_session_t *sp, pduq_t *pq)
{
     bhs_t	*bhs;
     int	statSN;

     debug_called(8);

     bhs = &pq->pdu.ipdu.bhs;
     statSN = ntohl(bhs->OpcodeSpecificFields[1]);
#if 0
     {
	  /*
	   | this code is only for debugging.
	   */
	  sn_t	*sn = &sp->sn;
	  if(sp->cws == 0) {
	       if((sp->flags & ISC_STALLED) == 0) {
		    sdebug(4, "window closed: max=0x%x exp=0x%x opcode=0x%x cmd=0x%x cws=%d.",
			   sn->maxCmd, sn->expCmd, bhs->opcode, sn->cmd, sp->cws);
		    sp->flags |= ISC_STALLED;
	       } else 
	       if(sp->flags & ISC_STALLED) {
		    sdebug(4, "window opened: max=0x%x exp=0x%x opcode=0x%x cmd=0x%x cws=%d.",
			   sn->maxCmd, sn->expCmd, bhs->opcode, sn->cmd, sp->cws);
		    sp->flags &= ~ISC_STALLED;;
	       }
	  }
     }
#endif

#ifdef notyet
     if(sp->sn.expCmd != sn->cmd) {
	  sdebug(1, "we lost something ... exp=0x%x cmd=0x%x",
		 sn->expCmd, sn->cmd);
     }
#endif
     sdebug(5, "opcode=0x%x itt=0x%x stat#0x%x maxcmd=0x%0x",
	    bhs->opcode, ntohl(bhs->itt), statSN, sp->sn.maxCmd);

     switch(bhs->opcode) {
     case ISCSI_READ_DATA: {
	  data_in_t 	*cmd = &pq->pdu.ipdu.data_in;

	  if(cmd->S == 0)
	       break;
     }

     default:
	  if(statSN > (sp->sn.stat + 1)) {
	       sdebug(1, "we lost some rec=0x%x exp=0x%x",
		      statSN, sp->sn.stat);
	       // XXX: must do some error recovery here.
	  }
	  sp->sn.stat = statSN;
     }

     switch(bhs->opcode) {
     case ISCSI_LOGIN_RSP:
     case ISCSI_TEXT_RSP:
     case ISCSI_LOGOUT_RSP:
	  i_nqueue_rsp(sp, pq);
	  wakeup(&sp->rsp);
	  sdebug(3, "wakeup rsp");
	  break;

     case ISCSI_NOP_IN:		_nop_in(sp, pq);	break;
     case ISCSI_SCSI_RSP:	_scsi_rsp(sp, pq);	break;
     case ISCSI_READ_DATA:	_read_data(sp, pq);	break;
     case ISCSI_R2T:		_r2t(sp, pq);		break;
     case ISCSI_REJECT:		_reject(sp, pq);	break;
     case ISCSI_ASYNC:		_async(sp, pq);		break;

     case ISCSI_TASK_RSP:
     default:
	  sdebug(1, "opcode=0x%x itt=0x%x not implemented yet",
		 bhs->opcode, ntohl(bhs->itt));
	  break;
     }
}

static int
proc_out(isc_session_t *sp)
{
     sn_t	*sn = &sp->sn;
     pduq_t	*pq;
     int	error, ndone = 0;
     int	which;

     debug_called(8);

     while(1) {
	  pdu_t *pp;
	  bhs_t	*bhs;

	  /*
	   | check if there is outstanding work in:
	   | 1- the Inmediate queue
	   | 2- the R2T queue
	   | 3- the cmd queue, only if the command window allows it.
	   */
	  which = BIT(0) | BIT(1);
	  if(SNA_GT(sn->cmd, sn->maxCmd) == 0)
	       which |= BIT(2);

	  if((pq = i_dqueue_snd(sp, which)) == NULL)
	       break;

	  pp = &pq->pdu;
	  bhs = &pp->ipdu.bhs;
	  switch(bhs->opcode) {
	  case ISCSI_SCSI_CMD:
	       sn->itt++;
	       bhs->itt = htonl(sn->itt);

	  case ISCSI_LOGIN_CMD:
	  case ISCSI_TEXT_CMD:
	  case ISCSI_LOGOUT_CMD:
	  case ISCSI_SNACK:
	  case ISCSI_NOP_OUT:
	  case ISCSI_TASK_CMD:
	       bhs->CmdSN = htonl(sn->cmd);
	       if(bhs->I == 0)
		    sn->cmd++;

	  case ISCSI_WRITE_DATA:
	       bhs->ExpStSN = htonl(sn->stat);
	       break;

	  default:
	       // XXX: can this happen?
	       xdebug("bad opcode=0x%x sn(cmd=0x%x expCmd=0x%x maxCmd=0x%x expStat=0x%x itt=0x%x)",
		      bhs->opcode,
		      sn->cmd, sn->expCmd, sn->maxCmd, sn->expStat, sn->itt);
	       // XXX: and now?
	  }

	  sdebug(5, "opcode=0x%x sn(cmd=0x%x expCmd=0x%x maxCmd=0x%x expStat=0x%x itt=0x%x)",
		bhs->opcode,
		sn->cmd, sn->expCmd, sn->maxCmd, sn->expStat, sn->itt);

	  if(pq->ccb)
	       i_nqueue_hld(sp, pq);

	  if((error = isc_sendPDU(sp, pq)) == 0)
	       ndone++;
	  else {
	       xdebug("error=%d ndone=%d opcode=0x%x ccb=%p itt=%x",
		      error, ndone, bhs->opcode, pq->ccb, ntohl(bhs->itt));
	       if(error == EPIPE) {
		    // XXX: better do some error recovery ...
		    break;
	       }
#if 0
	       if(pq->ccb) {
		    i_remove_hld(sp, pq);
		    pq->ccb->ccb_h.status |= CAM_UNREC_HBA_ERROR; // some better error?
		    XPT_DONE(pq->ccb);
	       }
	       else {
		    // XXX: now what?
		    // how do we pass back an error?
	       }
#endif
	  }
	  if(pq->ccb == NULL || error)
	       pdu_free(sp->isc, pq);
     }
     return ndone;
}

/*
 | survives link breakdowns.
 */
static void
ism_proc(void *vp)
{
     isc_session_t 	*sp = (isc_session_t *)vp;
     int		odone;

     debug_called(8);
     sdebug(3, "started");

     sp->flags |= ISC_SM_RUNNING;
     do {
	  if(sp->flags & ISC_SM_HOLD)
	       odone = 0;
	  else
	       odone = proc_out(sp);
	  sdebug(7, "odone=%d", odone);
	  if(odone == 0) {
	       mtx_lock(&sp->io_mtx);
#ifdef ISC_OWAITING
	       sp->flags |= ISC_OWAITING;
#endif
	       if((msleep(&sp->flags, &sp->io_mtx, PRIBIO, "isc_proc", hz*30) == EWOULDBLOCK)
		  && (sp->flags & ISC_CON_RUNNING))
		    _nop_out(sp);
#ifdef ISC_OWAITING
	       sp->flags &= ~ISC_OWAITING;
#endif
	       mtx_unlock(&sp->io_mtx);
	  }
     } while(sp->flags & ISC_SM_RUN);

     sp->flags &= ~ISC_SM_RUNNING;

#if __FreeBSD_version >= 700000
     destroy_dev(sp->dev);
#endif

     sdebug(3, "terminated");

     wakeup(sp);
     kthread_exit(0);
}

#if 0
static int
isc_dump_options(SYSCTL_HANDLER_ARGS)
{
     int error;
     isc_session_t *sp;
     char	buf[1024], *bp;

     sp = (isc_session_t *)arg1;
     bp = buf;
     sprintf(bp, "targetname='%s'", sp->opt.targetName);
     bp += strlen(bp);
     sprintf(bp, " targetname='%s'", sp->opt.targetAddress);
     error = SYSCTL_OUT(req, buf, strlen(buf));
     return error;
}
#endif

static int
isc_dump_stats(SYSCTL_HANDLER_ARGS)
{
     isc_session_t	*sp;
     struct isc_softc	*sc;
     char	buf[1024], *bp;
     int 	error, n;

     sp = (isc_session_t *)arg1;
     sc = sp->isc;

     bp = buf;
     n = sizeof(buf);
     snprintf(bp, n, "recv=%d sent=%d", sp->stats.nrecv, sp->stats.nsent);
     bp += strlen(bp);
     n -= strlen(bp);
     snprintf(bp, n, " flags=0x%08x pdus-alloc=%d pdus-max=%d", 
		  sp->flags, sc->npdu_alloc, sc->npdu_max);
     bp += strlen(bp);
     n -= strlen(bp);
     snprintf(bp, n, " cws=%d cmd=%x exp=%x max=%x stat=%x itt=%x",
		  sp->cws, sp->sn.cmd, sp->sn.expCmd, sp->sn.maxCmd, sp->sn.stat, sp->sn.itt);
     error = SYSCTL_OUT(req, buf, strlen(buf));
     return error;
}

static int
isc_sysctl_targetName(SYSCTL_HANDLER_ARGS)
{
     char	buf[128], **cp;
     int 	error;

     cp = (char **)arg1;
     snprintf(buf, sizeof(buf), "%s", *cp);
     error = SYSCTL_OUT(req, buf, strlen(buf));
     return error;
}
     
static int
isc_sysctl_targetAddress(SYSCTL_HANDLER_ARGS)
{
     char	buf[128], **cp;
     int 	error;

     cp = (char **)arg1;
     snprintf(buf, sizeof(buf), "%s", *cp);
     error = SYSCTL_OUT(req, buf, strlen(buf));
     return error;
}
     
static void
isc_add_sysctls(isc_session_t *sp)
{
     debug_called(8);
     sdebug(6, "sid=%d %s", sp->sid, sp->dev->si_name);

     sysctl_ctx_init(&sp->clist);
     sp->oid = SYSCTL_ADD_NODE(&sp->clist,
			       SYSCTL_CHILDREN(sp->isc->oid),
			       OID_AUTO,
			       sp->dev->si_name+5, // iscsi0
			       CTLFLAG_RD,
			       0,
			       "initiator");
     SYSCTL_ADD_PROC(&sp->clist,
		     SYSCTL_CHILDREN(sp->oid),
		     OID_AUTO,
		     "targetname",
		     CTLFLAG_RD,
		     (void *)&sp->opt.targetName, 0,
		     isc_sysctl_targetName, "A", "target name");

     SYSCTL_ADD_PROC(&sp->clist,
		     SYSCTL_CHILDREN(sp->oid),
		     OID_AUTO,
		     "targeaddress",
		     CTLFLAG_RD,
		     (void *)&sp->opt.targetAddress, 0,
		     isc_sysctl_targetAddress, "A", "target address");

     SYSCTL_ADD_PROC(&sp->clist,
		     SYSCTL_CHILDREN(sp->oid),
		     OID_AUTO,
		     "stats",
		     CTLFLAG_RD,
		     (void *)sp, 0,
		     isc_dump_stats, "A", "statistics");
}

void
ism_stop(isc_session_t *sp)
{
     struct isc_softc *sc = sp->isc;
     int	n;

     debug_called(8);
     sdebug(2, "terminating");
     /*
      | first stop the receiver
      */
     isc_stop_receiver(sp);
     /*
      | now stop the xmitter
      */
     n = 5;
     sp->flags &= ~ISC_SM_RUN;
     while(n-- && (sp->flags & ISC_SM_RUNNING)) {
	  sdebug(2, "n=%d", n);
	  wakeup(&sp->flags);
	  tsleep(sp, PRIBIO, "-", 5*hz);
     }
     sdebug(2, "final n=%d", n);
     sp->flags &= ~ISC_FFPHASE;
     
     iscsi_cleanup(sp);

     (void)i_pdu_flush(sp);

     ic_lost_target(sp, sp->sid);

     mtx_lock(&sc->mtx);
     TAILQ_REMOVE(&sc->isc_sess, sp, sp_link);
     sc->nsess--;
     mtx_unlock(&sc->mtx);

#if __FreeBSD_version < 700000
     destroy_dev(sp->dev);
#endif

     mtx_destroy(&sp->rsp_mtx);
     mtx_destroy(&sp->rsv_mtx);
     mtx_destroy(&sp->hld_mtx);
     mtx_destroy(&sp->snd_mtx);
     mtx_destroy(&sp->io_mtx);

     i_freeopt(&sp->opt);
     sc->sessions[sp->sid] = NULL;

     if(sysctl_ctx_free(&sp->clist))
	  xdebug("sysctl_ctx_free failed");

     free(sp, M_ISCSI);
}

int
ism_start(isc_session_t *sp)
{
     debug_called(8);
    /*
     | now is a good time to do some initialization
     */
     TAILQ_INIT(&sp->rsp);
     TAILQ_INIT(&sp->rsv);
     TAILQ_INIT(&sp->csnd);
     TAILQ_INIT(&sp->isnd);
     TAILQ_INIT(&sp->wsnd);
     TAILQ_INIT(&sp->hld);
#if 1
     mtx_init(&sp->rsv_mtx, "iscsi-rsv", NULL, MTX_DEF);
     mtx_init(&sp->rsp_mtx, "iscsi-rsp", NULL, MTX_DEF);
     mtx_init(&sp->snd_mtx, "iscsi-snd", NULL, MTX_DEF);
     mtx_init(&sp->hld_mtx, "iscsi-hld", NULL, MTX_DEF);
#else
     mtx_init(&sp->rsv_mtx, "iscsi-rsv", NULL, MTX_SPIN);
     mtx_init(&sp->rsp_mtx, "iscsi-rsp", NULL, MTX_SPIN);
     mtx_init(&sp->snd_mtx, "iscsi-snd", NULL, MTX_SPIN);
     mtx_init(&sp->hld_mtx, "iscsi-hld", NULL, MTX_SPIN);
#endif
     mtx_init(&sp->io_mtx, "iscsi-io", NULL, MTX_DEF);

     isc_add_sysctls(sp);

     sp->flags |= ISC_SM_RUN;

     return kthread_create(ism_proc, sp, &sp->stp, 0, 0, "ism_%d", sp->sid);
}
