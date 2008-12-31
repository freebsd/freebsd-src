/*-
 * Copyright (c) 2001-2003
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
 * ForeHE driver.
 *
 * Ioctl handler.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/hatm/if_hatm_ioctl.c,v 1.13.6.1 2008/11/25 02:59:29 kensmith Exp $");

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
#include <sys/queue.h>
#include <sys/syslog.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <vm/uma.h>

#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_atm.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_atm.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/utopia/utopia.h>
#include <dev/hatm/if_hatmconf.h>
#include <dev/hatm/if_hatmreg.h>
#include <dev/hatm/if_hatmvar.h>

static u_int hatm_natm_traffic = ATMIO_TRAFFIC_UBR;
static u_int hatm_natm_pcr = 0;

static int hatm_sysctl_natm_traffic(SYSCTL_HANDLER_ARGS);

SYSCTL_DECL(_hw_atm);

SYSCTL_PROC(_hw_atm, OID_AUTO, natm_traffic, CTLTYPE_UINT | CTLFLAG_RW,
    &hatm_natm_traffic, sizeof(hatm_natm_traffic), hatm_sysctl_natm_traffic,
    "IU", "traffic type for NATM connections");
SYSCTL_UINT(_hw_atm, OID_AUTO, natm_pcr, CTLFLAG_RW,
    &hatm_natm_pcr, 0, "PCR for NATM connections");

/*
 * Try to open the given VCC.
 */
static int
hatm_open_vcc(struct hatm_softc *sc, struct atmio_openvcc *arg)
{
	u_int cid;
	struct hevcc *vcc;
	int error = 0;

	DBG(sc, VCC, ("Open VCC: %u.%u flags=%#x", arg->param.vpi,
	    arg->param.vci, arg->param.flags));

	if ((arg->param.vpi & ~HE_VPI_MASK) ||
	    (arg->param.vci & ~HE_VCI_MASK) ||
	    (arg->param.vci == 0))
		return (EINVAL);
	cid = HE_CID(arg->param.vpi, arg->param.vci);

	if ((arg->param.flags & ATMIO_FLAG_NOTX) &&
	    (arg->param.flags & ATMIO_FLAG_NORX))
		return (EINVAL);

	vcc = uma_zalloc(sc->vcc_zone, M_NOWAIT | M_ZERO);
	if (vcc == NULL)
		return (ENOMEM);

	mtx_lock(&sc->mtx);
	if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		error = EIO;
		goto done;
	}
	if (sc->vccs[cid] != NULL) {
		error = EBUSY;
		goto done;
	}
	vcc->param = arg->param;
	vcc->rxhand = arg->rxhand;
	switch (vcc->param.aal) {

	  case ATMIO_AAL_0:
	  case ATMIO_AAL_5:
	  case ATMIO_AAL_RAW:
		break;

	  default:
		error = EINVAL;
		goto done;
	}
	switch (vcc->param.traffic) {

	  case ATMIO_TRAFFIC_UBR:
	  case ATMIO_TRAFFIC_CBR:
	  case ATMIO_TRAFFIC_ABR:
		break;

	  default:
		error = EINVAL;
		goto done;
	}
	vcc->ntpds = 0;
	vcc->chain = vcc->last = NULL;
	vcc->ibytes = vcc->ipackets = 0;
	vcc->obytes = vcc->opackets = 0;

	if (!(vcc->param.flags & ATMIO_FLAG_NOTX) &&
	     (error = hatm_tx_vcc_can_open(sc, cid, vcc)) != 0)
		goto done;

	/* ok - go ahead */
	sc->vccs[cid] = vcc;
	hatm_load_vc(sc, cid, 0);

	/* don't free below */
	vcc = NULL;
	sc->open_vccs++;

  done:
	mtx_unlock(&sc->mtx);
	if (vcc != NULL)
		uma_zfree(sc->vcc_zone, vcc);
	return (error);
}

void
hatm_load_vc(struct hatm_softc *sc, u_int cid, int reopen)
{
	struct hevcc *vcc = sc->vccs[cid];

	if (!(vcc->param.flags & ATMIO_FLAG_NOTX))
		hatm_tx_vcc_open(sc, cid);
	if (!(vcc->param.flags & ATMIO_FLAG_NORX))
		hatm_rx_vcc_open(sc, cid);

	if (reopen)
		return;

	/* inform management about non-NG and NG-PVCs */
	if (!(vcc->param.flags & ATMIO_FLAG_NG) ||
	     (vcc->param.flags & ATMIO_FLAG_PVC))
		ATMEV_SEND_VCC_CHANGED(IFP2IFATM(sc->ifp), vcc->param.vpi,
		    vcc->param.vci, 1);
}

/*
 * VCC has been finally closed.
 */
void
hatm_vcc_closed(struct hatm_softc *sc, u_int cid)
{
	struct hevcc *vcc = sc->vccs[cid];

	/* inform management about non-NG and NG-PVCs */
	if (!(vcc->param.flags & ATMIO_FLAG_NG) ||
	    (vcc->param.flags & ATMIO_FLAG_PVC))
		ATMEV_SEND_VCC_CHANGED(IFP2IFATM(sc->ifp), HE_VPI(cid), HE_VCI(cid), 0);

	sc->open_vccs--;
	uma_zfree(sc->vcc_zone, vcc);
	sc->vccs[cid] = NULL;
}

/*
 * Try to close the given VCC
 */
static int
hatm_close_vcc(struct hatm_softc *sc, struct atmio_closevcc *arg)
{
	u_int cid;
	struct hevcc *vcc;
	int error = 0;

	DBG(sc, VCC, ("Close VCC: %u.%u", arg->vpi, arg->vci));

	if((arg->vpi & ~HE_VPI_MASK) ||
	   (arg->vci & ~HE_VCI_MASK) ||
	   (arg->vci == 0))
		return (EINVAL);
	cid = HE_CID(arg->vpi, arg->vci);

	mtx_lock(&sc->mtx);
	vcc = sc->vccs[cid];
	if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		error = EIO;
		goto done;
	}

	if (vcc == NULL || !(vcc->vflags & HE_VCC_OPEN)) {
		error = ENOENT;
		goto done;
	}

	if (vcc->vflags & HE_VCC_TX_OPEN)
		hatm_tx_vcc_close(sc, cid);
	if (vcc->vflags & HE_VCC_RX_OPEN)
		hatm_rx_vcc_close(sc, cid);

	if (vcc->param.flags & ATMIO_FLAG_ASYNC)
		goto done;

	while ((sc->ifp->if_drv_flags & IFF_DRV_RUNNING) &&
	       (vcc->vflags & (HE_VCC_TX_CLOSING | HE_VCC_RX_CLOSING)))
		cv_wait(&sc->vcc_cv, &sc->mtx);

	if (!(sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		error = EIO;
		goto done;
	}

	if (!(vcc->vflags & ATMIO_FLAG_NOTX))
		hatm_tx_vcc_closed(sc, cid);

	hatm_vcc_closed(sc, cid);

  done:
	mtx_unlock(&sc->mtx);
	return (error);
}

/*
 * IOCTL handler
 */
int
hatm_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct hatm_softc *sc = ifp->if_softc;
	struct atmio_vcctable *vtab;
	int error = 0;

	switch (cmd) {

	  case SIOCSIFADDR:
		mtx_lock(&sc->mtx);
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
			hatm_initialize(sc);
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
				hatm_initialize(sc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				hatm_stop(sc);
			}
		}
		mtx_unlock(&sc->mtx);
		break;

	  case SIOCGIFMEDIA:
	  case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->media, cmd);
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

	  case SIOCATMGVCCS:
		/* return vcc table */
		vtab = atm_getvccs((struct atmio_vcc **)sc->vccs,
		    HE_MAX_VCCS, sc->open_vccs, &sc->mtx, 1);
		error = copyout(vtab, ifr->ifr_data, sizeof(*vtab) +
		    vtab->count * sizeof(vtab->vccs[0]));
		free(vtab, M_DEVBUF);
		break;

	  case SIOCATMGETVCCS:	/* netgraph internal use */
		vtab = atm_getvccs((struct atmio_vcc **)sc->vccs,
		    HE_MAX_VCCS, sc->open_vccs, &sc->mtx, 0);
		if (vtab == NULL) {
			error = ENOMEM;
			break;
		}
		*(void **)data = vtab;
		break;

	  case SIOCATMOPENVCC:		/* kernel internal use */
		error = hatm_open_vcc(sc, (struct atmio_openvcc *)data);
		break;

	  case SIOCATMCLOSEVCC:		/* kernel internal use */
		error = hatm_close_vcc(sc, (struct atmio_closevcc *)data);
		break;

	  default:
		DBG(sc, IOCTL, ("cmd=%08lx arg=%p", cmd, data));
		error = EINVAL;
		break;
	}

	return (error);
}

static int
hatm_sysctl_natm_traffic(SYSCTL_HANDLER_ARGS)
{
	int error;
	int tmp;

	tmp = hatm_natm_traffic;
	error = sysctl_handle_int(oidp, &tmp, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (tmp != ATMIO_TRAFFIC_UBR && tmp != ATMIO_TRAFFIC_CBR)
		return (EINVAL);

	hatm_natm_traffic = tmp;
	return (0);
}
