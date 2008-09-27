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
 | $Id: iscsi.c,v 1.35 2007/04/22 08:58:29 danny Exp danny $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_iscsi_initiator.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/bus.h>
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
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <vm/uma.h>

#include <dev/iscsi/initiator/iscsi.h>
#include <dev/iscsi/initiator/iscsivar.h>

static char *iscsi_driver_version = "2.0.99";

static struct isc_softc isc;

MALLOC_DEFINE(M_ISCSI, "iSCSI", "iSCSI driver");

#ifdef ISCSI_INITIATOR_DEBUG
int iscsi_debug = ISCSI_INITIATOR_DEBUG;
SYSCTL_INT(_debug, OID_AUTO, iscsi_initiator, CTLFLAG_RW, &iscsi_debug, 0,
	"iSCSI driver debug flag");

struct mtx iscsi_dbg_mtx;
#endif


static char isid[6+1] = {
     0x80,
     'D',
     'I',
     'B',
     '0',
     '0',
     0
};

static int	i_create_session(struct cdev *dev, int *ndev);

static int	i_ping(struct cdev *dev);
static int	i_send(struct cdev *dev, caddr_t arg, struct thread *td);
static int	i_recv(struct cdev *dev, caddr_t arg, struct thread *td);
static int	i_setsoc(isc_session_t *sp, int fd, struct thread *td);

static d_open_t iscsi_open;
static d_close_t iscsi_close;
static d_ioctl_t iscsi_ioctl;
#ifdef ISCSI_INITIATOR_DEBUG
static d_read_t iscsi_read;
#endif

static struct cdevsw iscsi_cdevsw = {
     .d_version = D_VERSION,
     .d_open	= iscsi_open,
     .d_close	= iscsi_close,
     .d_ioctl	= iscsi_ioctl,
#ifdef ISCSI_INITIATOR_DEBUG
     .d_read	= iscsi_read,
#endif
     .d_name	= "iSCSI",
};

static int
iscsi_open(struct cdev *dev, int flags, int otype, struct thread *td)
{
     debug_called(8);

     debug(7, "dev=%d", dev2unit(dev));

     if(dev2unit(dev) > MAX_SESSIONS) {
	  // should not happen
          return ENODEV;
     }
     if(dev2unit(dev) == MAX_SESSIONS) {
#if 1
	  struct isc_softc *sc = (struct isc_softc *)dev->si_drv1;

	  // this should be in iscsi_start
	  if(sc->cam_sim == NULL)
	       ic_init(sc);
#endif
     }
     return 0;
}

static int
iscsi_close(struct cdev *dev, int flag, int otyp, struct thread *td)
{
     struct isc		*sc;
     isc_session_t	*sp;

     debug_called(8);

     debug(3, "flag=%x", flag);

     sc = (struct isc *)dev->si_drv1;
     if(dev2unit(dev) == MAX_SESSIONS) {
	  return 0;
     }
     sp = (isc_session_t *)dev->si_drv2;
     if(sp != NULL) {
	  sdebug(2, "session=%d flags=%x", dev2unit(dev), sp->flags );
	  /*
	   | if still in full phase, this probably means
	   | that something went realy bad.
	   | it could be a result from 'shutdown', in which case
	   | we will ignore it (so buffers can be flushed).
	   | the problem is that there is no way of differentiating
	   | between a shutdown procedure and 'iscontrol' dying.
	   */
	  if(sp->flags & ISC_FFPHASE)
	       // delay in case this is a shutdown.
	       tsleep(sp, PRIBIO, "isc-cls", 60*hz);
	  ism_stop(sp);
     }
     debug(2, "done");
     return 0;
}

static int
iscsi_ioctl(struct cdev *dev, u_long cmd, caddr_t arg, int mode, struct thread *td)
{
     struct isc		*sc;
     isc_session_t	*sp;
     isc_opt_t		*opt;
     int		error;

     sc = (struct isc *)dev->si_drv1;
     debug_called(8);

     error = 0;
     if(dev2unit(dev) == MAX_SESSIONS) {
	  /*
	   | non Session commands
	   */
	  if(sc == NULL)
	       return ENXIO;

	  switch(cmd) {
	  case ISCSISETSES:
	       error = i_create_session(dev, (int *)arg);
	       if(error == 0)
		    
	       break;

	  default:
	       error = ENXIO; // XXX:
	  }
	  return error;
     }
     sp = (isc_session_t *)dev->si_drv2;
     /*
      | session commands
      */
     if(sp == NULL)
	  return ENXIO;

     sdebug(6, "dev=%d cmd=%d", dev2unit(dev), (int)(cmd & 0xff));

     switch(cmd) {
     case ISCSISETSOC:
	  error = i_setsoc(sp, *(u_int *)arg, td);
	  break;

     case ISCSISETOPT:
	  opt = (isc_opt_t *)arg;
	  error = i_setopt(sp, opt);
	  break;

     case ISCSISEND:
	  error = i_send(dev, arg, td);
	  break;

     case ISCSIRECV:
	  error = i_recv(dev, arg, td);
	  break;

     case ISCSIPING:
	  error = i_ping(dev);
	  break;

     case ISCSISTART:
	  error = sp->soc == NULL? ENOTCONN: ism_fullfeature(dev, 1);
	  if(error == 0) {
	       sp->proc = td->td_proc;
	       SYSCTL_ADD_UINT(&sp->clist,
			       SYSCTL_CHILDREN(sp->oid),
			       OID_AUTO,
			       "pid",
			       CTLFLAG_RD,
			       &sp->proc->p_pid, sizeof(pid_t), "control process id");
	  }
	  break;

     case ISCSIRESTART:
	  error = sp->soc == NULL? ENOTCONN: ism_fullfeature(dev, 2);
	  break;

     case ISCSISTOP:
	  error = ism_fullfeature(dev, 0);
	  break;
	  
     case ISCSISIGNAL: {
	  int sig = *(int *)arg;

	  if(sig < 0 || sig > _SIG_MAXSIG)
	       error = EINVAL;
	  else
		sp->signal = sig;
	  break;
     }

     case ISCSIGETCAM: {
	  iscsi_cam_t *cp = (iscsi_cam_t *)arg;

	  error = ic_getCamVals(sp, cp);
	  break;
     }

     default:
	  error = ENOIOCTL;
     }

     return error;
}

static int
iscsi_read(struct cdev *dev, struct uio *uio, int ioflag)
{
#ifdef  ISCSI_INITIATOR_DEBUG
     struct isc_softc	*sc;
     isc_session_t	*sp;
     pduq_t 		*pq;
     char		buf[1024];

     sc = (struct isc_softc *)dev->si_drv1;
     sp = (isc_session_t *)dev->si_drv2;
     if(dev2unit(dev) == MAX_SESSIONS) {
	  sprintf(buf, "/----- Session ------/\n");
	  uiomove(buf, strlen(buf), uio);
	  int	i = 0;

	  TAILQ_FOREACH(sp, &sc->isc_sess, sp_link) {
	       if(uio->uio_resid == 0)
		    return 0;
	       sprintf(buf, "%03d] '%s' '%s'\n", i++, sp->opt.targetAddress, sp->opt.targetName);
	       uiomove(buf, strlen(buf), uio);
	  }
     }
     else {
	  int	i = 0;
	  struct socket	*so = sp->soc;
#define pukeit(i, pq) do {\
	       sprintf(buf, "%03d] %06x %02x %x %ld %jd\n",\
		       i, ntohl( pq->pdu.ipdu.bhs.CmdSN), \
		       pq->pdu.ipdu.bhs.opcode, ntohl(pq->pdu.ipdu.bhs.itt),\
		       (long)pq->ts.sec, pq->ts.frac);\
	       } while(0)

	  sprintf(buf, "%d/%d /---- hld -----/\n", sp->stats.nhld, sp->stats.max_hld);
	  uiomove(buf, strlen(buf), uio);
	  TAILQ_FOREACH(pq, &sp->hld, pq_link) {
	       if(uio->uio_resid == 0)
		    return 0;
	       pukeit(i, pq); i++;
	       uiomove(buf, strlen(buf), uio);
	  }
	  sprintf(buf, "%d/%d /---- rsp -----/\n", sp->stats.nrsp, sp->stats.max_rsp);
	  uiomove(buf, strlen(buf), uio);
	  i = 0;
	  TAILQ_FOREACH(pq, &sp->rsp, pq_link) {
	       if(uio->uio_resid == 0)
		    return 0;
	       pukeit(i, pq); i++;
	       uiomove(buf, strlen(buf), uio);
	  }
	  sprintf(buf, "%d/%d /---- csnd -----/\n", sp->stats.ncsnd, sp->stats.max_csnd);
	  i = 0;
	  uiomove(buf, strlen(buf), uio);
	  TAILQ_FOREACH(pq, &sp->csnd, pq_link) {
	       if(uio->uio_resid == 0)
		    return 0;
	       pukeit(i, pq); i++;
	       uiomove(buf, strlen(buf), uio);
	  }
	  sprintf(buf, "%d/%d /---- wsnd -----/\n", sp->stats.nwsnd, sp->stats.max_wsnd);
	  i = 0;
	  uiomove(buf, strlen(buf), uio);
	  TAILQ_FOREACH(pq, &sp->wsnd, pq_link) {
	       if(uio->uio_resid == 0)
		    return 0;
	       pukeit(i, pq); i++;
	       uiomove(buf, strlen(buf), uio);
	  }
	  sprintf(buf, "%d/%d /---- isnd -----/\n", sp->stats.nisnd, sp->stats.max_isnd);
	  i = 0;
	  uiomove(buf, strlen(buf), uio);
	  TAILQ_FOREACH(pq, &sp->isnd, pq_link) {
	       if(uio->uio_resid == 0)
		    return 0;
	       pukeit(i, pq); i++;
	       uiomove(buf, strlen(buf), uio);
	  }

	  sprintf(buf, "/---- Stats ---/\n");
	  uiomove(buf, strlen(buf), uio);

	  sprintf(buf, "recv=%d sent=%d\n", sp->stats.nrecv, sp->stats.nsent);
	  uiomove(buf, strlen(buf), uio);

	  sprintf(buf, "flags=%x pdus: alloc=%d max=%d\n", 
		  sp->flags, sc->npdu_alloc, sc->npdu_max);
	  uiomove(buf, strlen(buf), uio);

	  sprintf(buf, "cws=%d last cmd=%x exp=%x max=%x stat=%x itt=%x\n",
		  sp->cws, sp->sn.cmd, sp->sn.expCmd, sp->sn.maxCmd, sp->sn.stat, sp->sn.itt);
	  uiomove(buf, strlen(buf), uio);

	  sprintf(buf, "/---- socket -----/\nso_count=%d so_state=%x\n", so->so_count, so->so_state);
	  uiomove(buf, strlen(buf), uio);

     }
#endif
     return 0;
}

static int
i_ping(struct cdev *dev)
{
     return 0;
}
/*
 | low level I/O
 */
static int
i_setsoc(isc_session_t *sp, int fd, struct thread *td)
{
     int error = 0;

     if(sp->soc != NULL)
	  isc_stop_receiver(sp);

     error = fget(td, fd, &sp->fp);
     if(error)
	  return error;

     if((error = fgetsock(td, fd, &sp->soc, 0)) == 0) {
	  sp->td = td;
	  isc_start_receiver(sp);
     }
     else {
	  fdrop(sp->fp, td);
	  sp->fp = NULL;
     }

     return error;
}

static int
i_send(struct cdev *dev, caddr_t arg, struct thread *td)
{
     isc_session_t	*sp = (isc_session_t *)dev->si_drv2;
     struct isc_softc	*sc = (struct isc_softc *)dev->si_drv1;
     caddr_t		bp;
     pduq_t		*pq;
     pdu_t		*pp;
     int		n, error;

     debug_called(8);

     if(sp->soc == NULL)
	  return ENOTCONN;

     if((pq = pdu_alloc(sc, 0)) == NULL)
	  return EAGAIN;
     pp = &pq->pdu;

     pq->pdu = *(pdu_t *)arg;
     if((error = i_prepPDU(sp, pq)) != 0)
	  return error;

     sdebug(4, "len=%d ahs_len=%d ds_len=%d", pq->len, pp->ahs_len, pp->ds_len);

     pq->buf = bp = malloc(pq->len - sizeof(union ipdu_u), M_ISCSI, M_WAITOK);

     if(pp->ahs_len) {
	  n = pp->ahs_len;
	  copyin(pp->ahs, bp, n);
	  pp->ahs = (ahs_t *)bp;
	  bp += n;
     }
     if(pp->ds_len) {
	  n = pp->ds_len;
	  copyin(pp->ds, bp, n); // can fail ...
	  pp->ds = bp;
	  bp += n;
	  while(n & 03) {
	       n++;
	       *bp++ = 0;
	  }
     }

     error = isc_qout(sp, pq);

     return error;
}

/*
 | NOTE: must calculate digest if requiered.
 */
static int
i_recv(struct cdev *dev, caddr_t arg, struct thread *td)
{
     isc_session_t	*sp = (isc_session_t *)dev->si_drv2;
     pduq_t		*pq;
     pdu_t		*pp, *up;
     caddr_t		bp;
     int		error, mustfree, cnt;
     size_t		need, have, n;

     debug_called(8);

     if(sp == NULL)
	  return EIO;

     if(sp->soc == NULL)
	  return ENOTCONN;
     cnt = 6;     // XXX: maybe the user can request a time out?
     mtx_lock(&sp->rsp_mtx);
     while((pq = TAILQ_FIRST(&sp->rsp)) == NULL) {
	  msleep(&sp->rsp, &sp->rsp_mtx, PRIBIO, "isc_rsp", hz*10);
	  if(cnt-- == 0) break; // XXX: for now, needs work
     }
     if(pq != NULL) {
	  sp->stats.nrsp--;
	  TAILQ_REMOVE(&sp->rsp, pq, pq_link);
     }
     mtx_unlock(&sp->rsp_mtx);

     sdebug(3, "cnt=%d", cnt);

     if(pq == NULL) {
	  error = ENOTCONN;
	  sdebug(3, "error=%d sp->flags=%x ", error, sp->flags);
	  return error;
     }
     up = (pdu_t *)arg;
     pp = &pq->pdu;
     up->ipdu = pp->ipdu;
     n = 0;
     up->ds_len = 0;
     up->ahs_len = 0;
     error = 0;

     if(pq->mp) {
	  u_int	len;

	  // Grr...
	  len = 0;
	  if(pp->ahs_len) {
	       len += pp->ahs_len;
	       if(sp->hdrDigest)
		    len += 4;
	  }
	  if(pp->ds_len) {
	       len += pp->ds_len;
	       if(sp->hdrDigest)
		    len += 4;
	  }

	  mustfree = 0;
	  if(len > pq->mp->m_len) {
	       mustfree++;
	       bp = malloc(len, M_ISCSI, M_WAITOK);
	       sdebug(4, "need mbufcopy: %d", len);
	       i_mbufcopy(pq->mp, bp, len);
	  } 
	  else
	       bp = mtod(pq->mp, caddr_t);

	  if(pp->ahs_len) {
	       need = pp->ahs_len;
	       if(sp->hdrDigest)
		    need += 4;
	       n = MIN(up->ahs_size, need);
	       error = copyout(bp, (caddr_t)up->ahs, n);
	       up->ahs_len = n;
	       bp += need;
	  }
	  if(!error && pp->ds_len) {
	       need = pp->ds_len;
	       if(sp->hdrDigest)
		    need += 4;
	       if((have = up->ds_size) == 0) {
		    have = up->ahs_size - n;
		    up->ds = (caddr_t)up->ahs + n;
	       }
	       n = MIN(have, need);
	       error = copyout(bp, (caddr_t)up->ds, n);
	       up->ds_len = n;
	  }

	  if(mustfree)
	       free(bp, M_ISCSI);
     }

     sdebug(6, "len=%d ahs_len=%d ds_len=%d", pq->len, pp->ahs_len, pp->ds_len);

     pdu_free(sp->isc, pq);

     return error;
}

static int
i_create_session(struct cdev *dev, int *ndev)
{ 
     struct isc_softc		*sc = (struct isc_softc *)dev->si_drv1;
     isc_session_t	*sp;
     int		error, n;

     debug_called(8);
     sp = (isc_session_t *)malloc(sizeof *sp, M_ISCSI, M_WAITOK | M_ZERO);
     if(sp == NULL)
	  return ENOMEM;
     mtx_lock(&sc->mtx);
     /*
      | search for the lowest unused sid
      */
     for(n = 0; n < MAX_SESSIONS; n++)
	  if(sc->sessions[n] == NULL)
	       break;
     if(n == MAX_SESSIONS) {
	  mtx_unlock(&sc->mtx);
	  free(sp, M_ISCSI);
	  return EPERM;
     }
     TAILQ_INSERT_TAIL(&sc->isc_sess, sp, sp_link);
     sc->nsess++;
     mtx_unlock(&sc->mtx);

     sc->sessions[n] = sp;
     sp->dev = make_dev(&iscsi_cdevsw, n, UID_ROOT, GID_WHEEL, 0600, "iscsi%d", n);
     *ndev = sp->sid = n;
     sp->isc = sc;
     sp->dev->si_drv1 = sc;
     sp->dev->si_drv2 = sp;

     sp->opt.maxRecvDataSegmentLength = 8192;
     sp->opt.maxXmitDataSegmentLength = 8192;

     sp->opt.maxBurstLength = 65536;	// 64k

     sdebug(2, "sessionID=%d", n);
     error = ism_start(sp);

     sdebug(2, "error=%d", error);

     return error;
}

#ifdef notused
static void
iscsi_counters(isc_session_t *sp)
{
     int	h, r, s;
     pduq_t	*pq;

#define _puke(i, pq) do {\
	       debug(2, "%03d] %06x %02x %x %ld %jd %x\n",\
		       i, ntohl( pq->pdu.ipdu.bhs.CmdSN), \
		       pq->pdu.ipdu.bhs.opcode, ntohl(pq->pdu.ipdu.bhs.itt),\
		       (long)pq->ts.sec, pq->ts.frac, pq->flags);\
	       } while(0)

     h = r = s = 0; 
     TAILQ_FOREACH(pq, &sp->hld, pq_link) {
	  _puke(h, pq);
	  h++;
     }
     TAILQ_FOREACH(pq, &sp->rsp, pq_link) r++;
     TAILQ_FOREACH(pq, &sp->csnd, pq_link) s++;
     TAILQ_FOREACH(pq, &sp->wsnd, pq_link) s++;
     TAILQ_FOREACH(pq, &sp->isnd, pq_link) s++;
     debug(2, "hld=%d rsp=%d snd=%d", h, r, s);
}
#endif

static void
iscsi_shutdown(void *v)
{
     struct isc_softc	*sc = (struct isc_softc *)v;
     isc_session_t	*sp;
     int	n;

     debug_called(8);
     if(sc == NULL) {
	  xdebug("sc is NULL!");
	  return;
     }
     if(sc->eh == NULL)
	  debug(2, "sc->eh is NULL!");
     else {
	  EVENTHANDLER_DEREGISTER(shutdown_pre_sync, sc->eh);
	  debug(2, "done n=%d", sc->nsess);
     }
     n = 0;
     TAILQ_FOREACH(sp, &sc->isc_sess, sp_link) {
	  debug(2, "%2d] sp->flags=0x%08x", n, sp->flags);
	  n++;
     }
}

static void
iscsi_start(void)
{
     struct isc_softc *sc = &isc;
 
     debug_called(8);

     memset(sc, 0, sizeof(struct isc_softc));

     sc->dev = make_dev(&iscsi_cdevsw, MAX_SESSIONS, UID_ROOT, GID_WHEEL, 0600, "iscsi");
     sc->dev->si_drv1 = sc;

     TAILQ_INIT(&sc->isc_sess);
     
     sc->pdu_zone = uma_zcreate("pdu", sizeof(pduq_t),
				NULL, NULL, NULL, NULL,
				0, 0);
     if(sc->pdu_zone == NULL) {
	  printf("iscsi_initiator: uma_zcreate failed");
	  // XXX: and now what?
     }
     uma_zone_set_max(sc->pdu_zone, MAX_PDUS*2+1);
     mtx_init(&sc->mtx, "iscsi", NULL, MTX_DEF);
     mtx_init(&sc->pdu_mtx, "iscsi pdu pool", NULL, MTX_DEF);

#if 0
     // XXX: this will cause a panic if the
     //      module is loaded too early
     if(ic_init(sc) != 0)
	  return;
#else
     sc->cam_sim = NULL;
#endif
     if((sc->eh = EVENTHANDLER_REGISTER(shutdown_pre_sync, iscsi_shutdown,
					sc, SHUTDOWN_PRI_DEFAULT-1)) == NULL)
	  xdebug("shutdown event registration failed\n");
     /*
      | sysctl stuff
      */
     sysctl_ctx_init(&sc->clist);
     sc->oid = SYSCTL_ADD_NODE(&sc->clist,
			       SYSCTL_STATIC_CHILDREN(_net),
			       OID_AUTO,
			       "iscsi",
			       CTLFLAG_RD,
			       0,
			       "iSCSI Subsystem");

     SYSCTL_ADD_STRING(&sc->clist,
		       SYSCTL_CHILDREN(sc->oid),
		       OID_AUTO,
		       "driver_version",
		       CTLFLAG_RD,
		       iscsi_driver_version,
		       0,
		       "iscsi driver version");
 
     SYSCTL_ADD_STRING(&sc->clist,
		       SYSCTL_CHILDREN(sc->oid),
		       OID_AUTO,
		       "isid",
		       CTLFLAG_RW,
		       isid,
		       6+1,
		       "initiator part of the Session Identifier");

     SYSCTL_ADD_INT(&sc->clist,
		    SYSCTL_CHILDREN(sc->oid),
		    OID_AUTO,
		    "sessions",
		    CTLFLAG_RD,
		    &sc->nsess,
		    sizeof(sc->nsess),
		    "number of active session");
}

/*
 | Notes:
 |	unload SHOULD fail if there is activity
 |	activity: there is/are active session/s
 */
static void
iscsi_stop(void)
{
     struct isc_softc	*sc = &isc;
     isc_session_t	*sp, *sp_tmp;

     debug_called(8);

     /*
      | go through all the sessions
      | Note: close should have done this ...
      */
     TAILQ_FOREACH_SAFE(sp, &sc->isc_sess, sp_link, sp_tmp) {
	  //XXX: check for activity ...
	  ism_stop(sp);
     }
     if(sc->cam_sim != NULL)
	  ic_destroy(sc);

     mtx_destroy(&sc->mtx);
     mtx_destroy(&sc->pdu_mtx);
     uma_zdestroy(sc->pdu_zone);

     if(sc->dev)
	  destroy_dev(sc->dev);

     if(sysctl_ctx_free(&sc->clist))
	  xdebug("sysctl_ctx_free failed");
}

static int
iscsi_modevent(module_t mod, int what, void *arg)
{
     debug_called(8);

     switch(what) {
     case MOD_LOAD:
	  iscsi_start();
	  break;

     case MOD_QUIESCE:
#if 1
	  if(isc.nsess) {
	       xdebug("iscsi module busy(nsess=%d), cannot unload", isc.nsess);
	       log(LOG_ERR, "iscsi module busy, cannot unload");
	  }
	  return isc.nsess;
#endif
     case MOD_SHUTDOWN:
	  break;

     case MOD_UNLOAD:
	  iscsi_stop();
	  break;

     default:
	  break;
     }
     return 0;
}
moduledata_t iscsi_mod = {
         "iscsi",
         (modeventhand_t) iscsi_modevent,
         0
};

DECLARE_MODULE(iscsi, iscsi_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_DEPEND(iscsi, cam, 1, 1, 1);
