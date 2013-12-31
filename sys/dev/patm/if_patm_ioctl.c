/*-
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * Driver for IDT77252 based cards like ProSum's.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_natm.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/condvar.h>
#include <vm/uma.h>

#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_atm.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_atm.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/mbpool.h>

#include <dev/utopia/utopia.h>
#include <dev/patm/idt77252reg.h>
#include <dev/patm/if_patmvar.h>

/*
 * Open the VCC with the given parameters
 */
static int
patm_open_vcc(struct patm_softc *sc, struct atmio_openvcc *arg)
{
	u_int cid;
	struct patm_vcc *vcc;
	int error = 0;

	patm_debug(sc, VCC, "Open VCC: %u.%u flags=%#x", arg->param.vpi,
	    arg->param.vci, arg->param.flags);

	if (!LEGAL_VPI(sc, arg->param.vpi) || !LEGAL_VCI(sc, arg->param.vci))
		return (EINVAL);
	if (arg->param.vci == 0 && (arg->param.vpi != 0 ||
	    !(arg->param.flags & ATMIO_FLAG_NOTX) ||
	    arg->param.aal != ATMIO_AAL_RAW))
		return (EINVAL);
	cid = PATM_CID(sc, arg->param.vpi, arg->param.vci);

	if ((arg->param.flags & ATMIO_FLAG_NOTX) &&
	    (arg->param.flags & ATMIO_FLAG_NORX))
		return (EINVAL);

	if ((arg->param.traffic == ATMIO_TRAFFIC_ABR) &&
	    (arg->param.flags & (ATMIO_FLAG_NOTX | ATMIO_FLAG_NORX)))
		return (EINVAL);

	/* allocate vcc */
	vcc = uma_zalloc(sc->vcc_zone, M_NOWAIT | M_ZERO);
	if (vcc == NULL)
		return (ENOMEM);

	mtx_lock(&sc->mtx);
	if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		/* stopped while we have analyzed the arguments */
		error = EIO;
		goto done;
	}
	if (sc->vccs[cid] != NULL) {
		/* ups, already open */
		error = EBUSY;
		goto done;
	}

	/* check some parameters */
	vcc->cid = cid;
	vcc->vcc = arg->param;
	vcc->vflags = 0;
	vcc->rxhand = arg->rxhand;
	switch (vcc->vcc.aal) {

	  case ATMIO_AAL_0:
	  case ATMIO_AAL_34:
	  case ATMIO_AAL_5:
		break;

	  case ATMIO_AAL_RAW:
		if (arg->param.vci == 0 &&
		    !(arg->param.flags & ATMIO_FLAG_NOTX)) {
			error = EINVAL;
			goto done;
		}
		break;

	  default:
		error = EINVAL;
		goto done;
	}
	switch (vcc->vcc.traffic) {

	  case ATMIO_TRAFFIC_VBR:
	  case ATMIO_TRAFFIC_UBR:
	  case ATMIO_TRAFFIC_CBR:
	  case ATMIO_TRAFFIC_ABR:
		break;

	  default:
		error = EINVAL;
		goto done;
	}

	/* initialize */
	vcc->chain = NULL;
	vcc->last = NULL;
	vcc->ibytes = vcc->ipackets = 0;
	vcc->obytes = vcc->opackets = 0;

	/* ask the TX and RX sides */
	patm_debug(sc, VCC, "Open VCC: asking Rx/Tx");
	if (!(vcc->vcc.flags & ATMIO_FLAG_NOTX) &&
	     (error = patm_tx_vcc_can_open(sc, vcc)) != 0)
		goto done;
	if (!(vcc->vcc.flags & ATMIO_FLAG_NORX) &&
	     (error = patm_rx_vcc_can_open(sc, vcc)) != 0)
		goto done;

	/* ok - go ahead */
	sc->vccs[cid] = vcc;
	patm_load_vc(sc, vcc, 0);

	/* don't free below */
	vcc = NULL;
	sc->vccs_open++;

	/* done */
  done:
	mtx_unlock(&sc->mtx);
	if (vcc != NULL)
		uma_zfree(sc->vcc_zone, vcc);
	return (error);
}

void
patm_load_vc(struct patm_softc *sc, struct patm_vcc *vcc, int reload)
{

	patm_debug(sc, VCC, "Open VCC: opening");
	if (!(vcc->vcc.flags & ATMIO_FLAG_NOTX))
		patm_tx_vcc_open(sc, vcc);
	if (!(vcc->vcc.flags & ATMIO_FLAG_NORX))
		patm_rx_vcc_open(sc, vcc);

	if (!reload) {
		/* inform management about non-NG and NG-PVCs */
		if (!(vcc->vcc.flags & ATMIO_FLAG_NG) ||
		     (vcc->vcc.flags & ATMIO_FLAG_PVC))
			ATMEV_SEND_VCC_CHANGED(IFP2IFATM(sc->ifp), vcc->vcc.vpi,
			    vcc->vcc.vci, 1);
	}

	patm_debug(sc, VCC, "Open VCC: now open");
}

/*
 * Try to close the given VCC
 */
static int
patm_close_vcc(struct patm_softc *sc, struct atmio_closevcc *arg)
{
	u_int cid;
	struct patm_vcc *vcc;
	int error = 0;

	patm_debug(sc, VCC, "Close VCC: %u.%u", arg->vpi, arg->vci);

	if (!LEGAL_VPI(sc, arg->vpi) || !LEGAL_VCI(sc, arg->vci))
		return (EINVAL);
	cid = PATM_CID(sc, arg->vpi, arg->vci);

	mtx_lock(&sc->mtx);
	if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		/* stopped while we have analyzed the arguments */
		error = EIO;
		goto done;
	}

	vcc = sc->vccs[cid];
	if (vcc == NULL || !(vcc->vflags & PATM_VCC_OPEN)) {
		error = ENOENT;
		goto done;
	}

	if (vcc->vflags & PATM_VCC_TX_OPEN)
		patm_tx_vcc_close(sc, vcc);
	if (vcc->vflags & PATM_VCC_RX_OPEN)
		patm_rx_vcc_close(sc, vcc);

	if (vcc->vcc.flags & ATMIO_FLAG_ASYNC)
		goto done;

	while (vcc->vflags & (PATM_VCC_TX_CLOSING | PATM_VCC_RX_CLOSING)) {
		cv_wait(&sc->vcc_cv, &sc->mtx);
		if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			/* ups, has been stopped */
			error = EIO;
			goto done;
		}
	}

	if (!(vcc->vcc.flags & ATMIO_FLAG_NOTX))
		patm_tx_vcc_closed(sc, vcc);
	if (!(vcc->vcc.flags & ATMIO_FLAG_NORX))
		patm_rx_vcc_closed(sc, vcc);

	patm_vcc_closed(sc, vcc);

  done:
	mtx_unlock(&sc->mtx);

	return (error);
}

/*
 * VCC has been finally closed.
 */
void
patm_vcc_closed(struct patm_softc *sc, struct patm_vcc *vcc)
{

	/* inform management about non-NG and NG-PVCs */
	if (!(vcc->vcc.flags & ATMIO_FLAG_NG) ||
	    (vcc->vcc.flags & ATMIO_FLAG_PVC))
		ATMEV_SEND_VCC_CHANGED(IFP2IFATM(sc->ifp), vcc->vcc.vpi,
		    vcc->vcc.vci, 0);

	sc->vccs_open--;
	sc->vccs[vcc->cid] = NULL;
	uma_zfree(sc->vcc_zone, vcc);
}

int
patm_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct patm_softc *sc = ifp->if_softc;
	int error = 0;
	uint32_t cfg;
	struct atmio_vcctable *vtab;

	switch (cmd) {

	  case SIOCSIFADDR:
		mtx_lock(&sc->mtx);
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
			patm_initialize(sc);
		switch (ifa->ifa_addr->sa_family) {

#ifdef INET
		  case AF_INET:
		  case AF_INET6:
			ifa->ifa_rtrequest = atm_rtrequest;
			break;
#endif
		  default:
			break;
		}
		mtx_unlock(&sc->mtx);
		break;

	  case SIOCSIFFLAGS:
		mtx_lock(&sc->mtx);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				patm_initialize(sc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				patm_stop(sc);
			}
		}
		mtx_unlock(&sc->mtx);
		break;

	  case SIOCGIFMEDIA:
	  case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->media, cmd);

		/*
		 * We need to toggle unassigned/idle cells ourself because
		 * the 77252 generates null cells for spacing. When switching
		 * null cells of it gets the timing wrong.
		 */
		mtx_lock(&sc->mtx);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			if (sc->utopia.state & UTP_ST_UNASS) {
				if (!(sc->flags & PATM_UNASS)) {
					cfg = patm_nor_read(sc, IDT_NOR_CFG);
					cfg &= ~IDT_CFG_IDLECLP;
					patm_nor_write(sc, IDT_NOR_CFG, cfg);
					sc->flags |= PATM_UNASS;
				}
			} else {
				if (sc->flags & PATM_UNASS) {
					cfg = patm_nor_read(sc, IDT_NOR_CFG);
					cfg |= IDT_CFG_IDLECLP;
					patm_nor_write(sc, IDT_NOR_CFG, cfg);
					sc->flags &= ~PATM_UNASS;
				}
			}
		} else {
			if (sc->utopia.state & UTP_ST_UNASS)
				sc->flags |= PATM_UNASS;
			else
				sc->flags &= ~PATM_UNASS;
		}
		mtx_unlock(&sc->mtx);
		break;

	  case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		if (ifr->ifr_mtu > ATMMTU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	  case SIOCATMOPENVCC:		/* kernel internal use */
		error = patm_open_vcc(sc, (struct atmio_openvcc *)data);
		break;

	  case SIOCATMCLOSEVCC:		/* kernel internal use */
		error = patm_close_vcc(sc, (struct atmio_closevcc *)data);
		break;

	  case SIOCATMGVCCS:	/* external use */
		/* return vcc table */
		vtab = atm_getvccs((struct atmio_vcc **)sc->vccs,
		    sc->mmap->max_conn, sc->vccs_open, &sc->mtx, 1);
		error = copyout(vtab, ifr->ifr_data, sizeof(*vtab) +
		    vtab->count * sizeof(vtab->vccs[0]));
		free(vtab, M_DEVBUF);
		break;

	  case SIOCATMGETVCCS:	/* netgraph internal use */
		vtab = atm_getvccs((struct atmio_vcc **)sc->vccs,
		    sc->mmap->max_conn, sc->vccs_open, &sc->mtx, 0);
		if (vtab == NULL) {
			error = ENOMEM;
			break;
		}
		*(void **)data = vtab;
		break;

	  default:
		patm_debug(sc, IOCTL, "unknown cmd=%08lx arg=%p", cmd, data);
		error = EINVAL;
		break;
	}

	return (error);
}
