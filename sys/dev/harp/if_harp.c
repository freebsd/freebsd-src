/*
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
 * Author: Harti Brandt <harti@freebsd.org>
 *
 * HARP pseudo-driver. This driver when loaded attaches to all ngATM drivers
 * in the system and creates a HARP physical interface for each of them.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/syslog.h>

#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/netisr.h>

#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>
#include <netatm/atm_vc.h>

#include <net/if_atm.h>

#define HARP_MTU	9188

/*
 * Physical interface softc
 */
struct harp_softc {
	Cmn_unit	cmn;
	struct ifnet	*parent;
	LIST_ENTRY(harp_softc) link;
};

struct harp_vcc {
	struct cmn_vcc	cmn;
};

MODULE_VERSION(harp, 1);
MODULE_DEPEND(harp, atm, 1, 1, 1);

/* hooks from if_atmsubr.c */
extern void (*atm_harp_input_p)(struct ifnet *ifp, struct mbuf **m,
    struct atm_pseudohdr *ah, void *rxhand);
extern void (*atm_harp_attach_p)(struct ifnet *);
extern void (*atm_harp_detach_p)(struct ifnet *);

static MALLOC_DEFINE(M_HARP, "harp", "Harp pseudo interface");

static uma_zone_t harp_nif_zone;
static uma_zone_t harp_vcc_zone;

/* List of all existing 'harp' interfaces */
static LIST_HEAD(, harp_softc) harp_softc_list =
    LIST_HEAD_INITIALIZER(harp_softc_list);

static struct stack_defn harp_svaal5 = {
	NULL,
	SAP_CPCS_AAL5,
	SDF_TERM,
	atm_dev_inst,
	atm_dev_lower,
	NULL,
	0,
};

static struct stack_defn *harp_services = &harp_svaal5;

/*
 * Map between constants
 */
static const struct {
	u_int	vendor;
	u_int	api;
	u_int	dev;
} map_devs[] = {
	[ATM_DEVICE_UNKNOWN] =
		{ VENDOR_UNKNOWN, VENDAPI_UNKNOWN, DEV_UNKNOWN },
	[ATM_DEVICE_PCA200E] =
		{ VENDOR_FORE, VENDAPI_FORE_1, DEV_FORE_PCA200E },
	[ATM_DEVICE_HE155] =
		{ VENDOR_FORE, VENDAPI_FORE_2, DEV_FORE_HE155 },
	[ATM_DEVICE_HE622] =
		{ VENDOR_FORE, VENDAPI_FORE_2, DEV_FORE_HE622 },
	[ATM_DEVICE_ENI155P] =
		{ VENDOR_ENI, VENDAPI_ENI_1, DEV_ENI_155P },
	[ATM_DEVICE_ADP155P] =
		{ VENDOR_ENI, VENDAPI_ENI_1, DEV_ENI_155P },
	[ATM_DEVICE_FORELE25] =
		{ VENDOR_FORE, VENDAPI_IDT_1, DEV_FORE_LE25 },
	[ATM_DEVICE_FORELE155] =
		{ VENDOR_FORE, VENDAPI_IDT_1, DEV_FORE_LE155 },
	[ATM_DEVICE_NICSTAR25] =
		{ VENDOR_IDT, VENDAPI_IDT_1, DEV_IDT_25 },
	[ATM_DEVICE_NICSTAR155] =
		{ VENDOR_IDT, VENDAPI_IDT_1, DEV_IDT_155 },
	[ATM_DEVICE_IDTABR25] =
		{ VENDOR_IDT, VENDAPI_IDT_2, DEV_IDTABR_25 },
	[ATM_DEVICE_IDTABR155] =
		{ VENDOR_IDT, VENDAPI_IDT_2, DEV_IDTABR_155 },
	[ATM_DEVICE_PROATM25] =
		{ VENDOR_PROSUM, VENDAPI_IDT_2, DEV_PROATM_25 },
	[ATM_DEVICE_PROATM155] =
		{ VENDOR_PROSUM, VENDAPI_IDT_2, DEV_PROATM_155 },
};

/*
 * Return zero if this interface is ok for us.
 * XXX This should go away when we have full ngATM-ified the en driver.
 */
static int
harp_check_if(const struct ifnet *ifp)
{
	if (ifp->if_type == IFT_ATM && strcmp(ifp->if_name, "en"))
		return (0);
	else
		return (-1);
}

/*
 * Instantiate a VCC stack.
 *
 * Could check for correct attributes here.
 */
static int
harp_instvcc(Cmn_unit *up, Cmn_vcc *vp)
{
	struct harp_softc *sc;

	if (up == NULL || vp == NULL || vp->cv_connvc == NULL)
		return (EINVAL);

	sc = (struct harp_softc *)up;

	return (0);
}

/*
 * Open a VCC.
 */
static int
harp_openvcc(Cmn_unit *up, Cmn_vcc *vp)
{
	struct harp_softc *sc;
	struct atmio_openvcc data;
	Atm_attributes *attrib;
	struct vccb *vccinf;
	const struct ifatm_mib *mib;
	int err;

	if (up == NULL || vp == NULL || vp->cv_connvc == NULL)
		return (EINVAL);

	sc = (struct harp_softc *)up;
	mib = sc->parent->if_linkmib;

	attrib = &vp->cv_connvc->cvc_attr;
	vccinf = vp->cv_connvc->cvc_vcc;

	if (attrib == NULL || vccinf == NULL)
		return (EINVAL);

	if (vccinf->vc_vpi >= (1 << mib->vpi_bits) ||
	    vccinf->vc_vci >= (1 << mib->vci_bits))
		return (EINVAL);

	memset(&data, 0, sizeof(data));

	switch (attrib->aal.type) {

	  case ATM_AAL0:
		data.param.aal = ATMIO_AAL_0;
		break;

	  case ATM_AAL5:
		data.param.aal = ATMIO_AAL_5;
		break;

	  default:
		return (EINVAL);
	}
	data.param.vpi = vccinf->vc_vpi;
	data.param.vci = vccinf->vc_vci;
	data.param.rmtu = HARP_MTU;
	data.param.tmtu = HARP_MTU;

	switch (attrib->bearer.v.bearer_class) {

	  case T_ATM_CLASS_C:
		data.param.traffic = ATMIO_TRAFFIC_VBR;
		break;

	  case T_ATM_CLASS_X:
		switch (attrib->bearer.v.traffic_type) {

		  case T_ATM_CBR:
			data.param.traffic = ATMIO_TRAFFIC_CBR;
			break;

		  case T_ATM_VBR:
			data.param.traffic = ATMIO_TRAFFIC_VBR;
			break;

		  case T_ATM_ABR:
			/* not really supported by HARP */
			return (EINVAL);

		  default:
		  case T_ATM_UBR:
			data.param.traffic = ATMIO_TRAFFIC_UBR;
			break;
		}
		break;

	  default:
		return (EINVAL);
	}
	data.param.tparam.pcr = attrib->traffic.v.forward.PCR_all_traffic;
	data.param.tparam.scr = attrib->traffic.v.forward.SCR_all_traffic;
	data.param.tparam.mbs = attrib->traffic.v.forward.MBS_all_traffic;

	data.rxhand = sc;
	data.param.flags = ATMIO_FLAG_HARP;

	err = (*sc->parent->if_ioctl)(sc->parent, SIOCATMOPENVCC,
	    (caddr_t)&data);

	return (err);
}

/*
 * Close VCC
 */
static int
harp_closevcc(Cmn_unit *up, Cmn_vcc *vp)
{
	struct harp_softc *sc;
	struct atmio_closevcc data;
	int err;

	if (vp == NULL || vp->cv_connvc == NULL ||
	    vp->cv_connvc->cvc_vcc == NULL)
		return (EINVAL);

	sc = (struct harp_softc *)up;
	
	data.vpi = vp->cv_connvc->cvc_vcc->vc_vpi;
	data.vci = vp->cv_connvc->cvc_vcc->vc_vci;

	err = (*sc->parent->if_ioctl)(sc->parent, SIOCATMCLOSEVCC,
	    (caddr_t)&data);

	return (err);
}

/*
 * IOCTLs
 */
static int
harp_ioctl(int code, caddr_t addr, caddr_t arg)
{
	return (ENOSYS);
}

/*
 * Output data
 */
static void
harp_output(Cmn_unit *cu, Cmn_vcc *cv, KBuffer *m)
{
	struct harp_softc *sc = (struct harp_softc *)cu;
	struct atm_pseudohdr *aph;
	int error;
	int mlen;

	if (cv == NULL || cv->cv_connvc == NULL ||
	    cv->cv_connvc->cvc_vcc == NULL) {
		m_freem(m);
		return;
	}
	M_ASSERTPKTHDR(m);

	/*
	 * Harp seems very broken with regard to mbuf handling. The length
	 * in the packet header is mostly broken here so recompute it.
	 */
	m->m_pkthdr.len = mlen = m_length(m, NULL);

	/*
	 * Prepend pseudo-hdr. Drivers don't care about the flags.
	 */
	M_PREPEND(m, sizeof(*aph), M_DONTWAIT);
	if (m == NULL)
		return;

	aph = mtod(m, struct atm_pseudohdr *);
	ATM_PH_VPI(aph) = cv->cv_connvc->cvc_vcc->vc_vpi;
	ATM_PH_SETVCI(aph, cv->cv_connvc->cvc_vcc->vc_vci);
	ATM_PH_FLAGS(aph) = 0;

	error = atm_output(sc->parent, m, NULL, NULL);

	if (error) {
		printf("%s: error %d\n", __func__, error);
		sc->cmn.cu_pif.pif_oerrors++;
		cv->cv_connvc->cvc_vcc->vc_oerrors++;
		if (cv->cv_connvc->cvc_vcc->vc_nif)
			cv->cv_connvc->cvc_vcc->vc_nif->nif_if.if_oerrors++;
		return;
	}

	/* statistics */
	sc->cmn.cu_pif.pif_opdus++;
	sc->cmn.cu_pif.pif_obytes += mlen;
	cv->cv_connvc->cvc_vcc->vc_opdus++;
	cv->cv_connvc->cvc_vcc->vc_obytes += mlen;
	if (cv->cv_connvc->cvc_vcc->vc_nif) {
		cv->cv_connvc->cvc_vcc->vc_nif->nif_obytes += mlen;
		cv->cv_connvc->cvc_vcc->vc_nif->nif_if.if_obytes += mlen;
		cv->cv_connvc->cvc_vcc->vc_nif->nif_if.if_opackets++;
	}
}

/*
 * Attach a new interface
 */
static void
harp_attach(struct ifnet *parent)
{
	struct harp_softc *sc;
	const struct ifatm_mib *mib;
	int error;

	if (harp_check_if(parent) != 0)
		return;

	sc = malloc(sizeof(*sc), M_HARP, M_WAITOK | M_ZERO);

	sc->parent = parent;
	sc->cmn.cu_unit = parent->if_unit;
	sc->cmn.cu_mtu = HARP_MTU;
	sc->cmn.cu_ioctl = harp_ioctl;
	sc->cmn.cu_instvcc = harp_instvcc;
	sc->cmn.cu_openvcc = harp_openvcc;
	sc->cmn.cu_closevcc = harp_closevcc;
	sc->cmn.cu_output = harp_output;
	sc->cmn.cu_vcc_zone = harp_vcc_zone;
	sc->cmn.cu_nif_zone = harp_nif_zone;
	sc->cmn.cu_softc = sc;

	/* config */
	mib = parent->if_linkmib;
	if (mib->device >= sizeof(map_devs) / sizeof(map_devs[0])) {
		sc->cmn.cu_config.ac_vendor = VENDOR_UNKNOWN;
		sc->cmn.cu_config.ac_vendapi = VENDAPI_UNKNOWN;
		sc->cmn.cu_config.ac_device = DEV_UNKNOWN;
	} else {
		sc->cmn.cu_config.ac_vendor = map_devs[mib->device].vendor;
		sc->cmn.cu_config.ac_vendapi = map_devs[mib->device].api;
		sc->cmn.cu_config.ac_device = map_devs[mib->device].dev;
	}

	switch (mib->media) {

	  case IFM_ATM_UTP_25:
		sc->cmn.cu_config.ac_media = MEDIA_UTP25;;
		break;

	  case IFM_ATM_TAXI_100:
		sc->cmn.cu_config.ac_media = MEDIA_TAXI_100;
		break;

	  case IFM_ATM_TAXI_140:
		sc->cmn.cu_config.ac_media = MEDIA_TAXI_140;
		break;

	  case IFM_ATM_MM_155:
	  case IFM_ATM_SM_155:
		sc->cmn.cu_config.ac_media = MEDIA_OC3C;
		break;

	  case IFM_ATM_MM_622:
	  case IFM_ATM_SM_622:
		sc->cmn.cu_config.ac_media = MEDIA_OC12C;
		break;

	  case IFM_ATM_UTP_155:
		sc->cmn.cu_config.ac_media = MEDIA_UTP155;
		break;

	  default:
		sc->cmn.cu_config.ac_media = MEDIA_UNKNOWN;
		break;
	}
	sc->cmn.cu_config.ac_bustype = BUS_PCI;
	sc->cmn.cu_pif.pif_pcr = mib->pcr;
	sc->cmn.cu_pif.pif_maxvpi = (1 << mib->vpi_bits) - 1;
	sc->cmn.cu_pif.pif_maxvci = (1 << mib->vci_bits) - 1;

	snprintf(sc->cmn.cu_config.ac_hard_vers,
	    sizeof(sc->cmn.cu_config.ac_hard_vers), "0x%lx",
	    (u_long)mib->hw_version);
	snprintf(sc->cmn.cu_config.ac_firm_vers,
	    sizeof(sc->cmn.cu_config.ac_firm_vers), "0x%lx",
	    (u_long)mib->sw_version);
	sc->cmn.cu_config.ac_serial = mib->serial;
	sc->cmn.cu_config.ac_ram = 0;
	sc->cmn.cu_config.ac_ramsize = 0;

	sc->cmn.cu_config.ac_macaddr.ma_data[0] =
	    sc->cmn.cu_pif.pif_macaddr.ma_data[0] = mib->esi[0];
	sc->cmn.cu_config.ac_macaddr.ma_data[1] =
	    sc->cmn.cu_pif.pif_macaddr.ma_data[1] = mib->esi[1];
	sc->cmn.cu_config.ac_macaddr.ma_data[2] =
	    sc->cmn.cu_pif.pif_macaddr.ma_data[2] = mib->esi[2];
	sc->cmn.cu_config.ac_macaddr.ma_data[3] =
	    sc->cmn.cu_pif.pif_macaddr.ma_data[3] = mib->esi[3];
	sc->cmn.cu_config.ac_macaddr.ma_data[4] =
	    sc->cmn.cu_pif.pif_macaddr.ma_data[4] = mib->esi[4];
	sc->cmn.cu_config.ac_macaddr.ma_data[5] =
	    sc->cmn.cu_pif.pif_macaddr.ma_data[5] = mib->esi[5];

	error = atm_physif_register(&sc->cmn, parent->if_name, harp_services);
	if (error) {
		log(LOG_ERR, "%s: pif registration failed %d\n",
		    parent->if_name, error);
		free(sc, M_HARP);
		return;
	}
	LIST_INSERT_HEAD(&harp_softc_list, sc, link);

	sc->cmn.cu_flags |= CUF_INITED;
}

/*
 * Destroy a cloned device
 */
static void
harp_detach(struct ifnet *ifp)
{
	struct harp_softc *sc;
	int error;

	LIST_FOREACH(sc, &harp_softc_list, link)
		if (sc->parent == ifp)
			break;
	if (sc == NULL)
		return;

	error = atm_physif_deregister(&sc->cmn);
	if (error)
		log(LOG_ERR, "%s: de-registration failed %d\n", ifp->if_name,
		    error);

	LIST_REMOVE(sc, link);

	free(sc, M_HARP);
}

/*
 * Pass PDU up the stack
 */
static void
harp_recv_stack(void *tok, KBuffer *m)
{
	Cmn_vcc *vcc = tok;
	int err;

	M_ASSERTPKTHDR(m);
	STACK_CALL(CPCS_UNITDATA_SIG, vcc->cv_upper, vcc->cv_toku,
	    vcc->cv_connvc, (intptr_t)m, 0, err);
	if (err) {
		printf("%s: error %d\n", __func__, err);
		KB_FREEALL(m);
	}
}

/*
 * Possible input from NATM
 */
static void
harp_input(struct ifnet *ifp, struct mbuf **mp, struct atm_pseudohdr *ah,
    void *rxhand)
{
	struct harp_softc *sc = rxhand;
	Cmn_vcc *vcc;
	char *cp;
	u_int pfxlen;
	struct mbuf *m, *m0;
	int mlen;

	if ((ATM_PH_FLAGS(ah) & ATMIO_FLAG_HARP) == 0)
		return;

	/* grab the packet */
	m = *mp;
	*mp = NULL;

	if (sc->parent != ifp) {
		printf("%s: parent=%p ifp=%p\n", __func__, sc->parent, ifp);
		goto drop;
	}

	vcc = atm_dev_vcc_find(&sc->cmn, ATM_PH_VPI(ah),
	    ATM_PH_VCI(ah), VCC_IN);
	if (vcc == NULL) {
		printf("%s: VCC %u/%u not found\n", __func__,ATM_PH_VPI(ah),
		    ATM_PH_VCI(ah));
		goto drop;
	}

	/* fit two pointers into the mbuf - assume, that the the data is
	 * pointer aligned. If it doesn't fit into the first mbuf, prepend
	 * another one, but leave the packet header where it is. atm_intr
	 * relies on this. */
	mlen = m->m_pkthdr.len;
	pfxlen = sizeof(atm_intr_func_t) + sizeof(void *);
	if (M_LEADINGSPACE(m) < pfxlen) {
		MGET(m0, 0, MT_DATA);
		if (m0 == NULL) {
			printf("%s: no leading space in buffer\n", __func__);
			goto drop;
		}
		m0->m_len = 0;
		m0->m_next = m;
		m = m0;
	}
	m->m_len += pfxlen;
	m->m_data -= pfxlen;
	KB_DATASTART(m, cp, char *);
	*((atm_intr_func_t *)cp) = harp_recv_stack;
	cp += sizeof(atm_intr_func_t);
	*((void **)cp) = (void *)vcc;

	/* count the packet */
	sc->cmn.cu_pif.pif_ipdus++;
	sc->cmn.cu_pif.pif_ibytes += mlen;
	vcc->cv_connvc->cvc_vcc->vc_ipdus++;
	vcc->cv_connvc->cvc_vcc->vc_ibytes += mlen;
	if (vcc->cv_connvc->cvc_vcc->vc_nif) {
		vcc->cv_connvc->cvc_vcc->vc_nif->nif_ibytes += mlen;
		vcc->cv_connvc->cvc_vcc->vc_nif->nif_if.if_ipackets++;
		vcc->cv_connvc->cvc_vcc->vc_nif->nif_if.if_ibytes += mlen;
	}

	/* hand it off */
	netisr_dispatch(NETISR_ATM, m);
	return;

  drop:
	m_freem(m);
}

/*
 * Module loading/unloading
 */
static int
harp_modevent(module_t mod, int event, void *data)
{
	struct ifnet *ifp;

	switch (event) {

	  case MOD_LOAD:
		harp_nif_zone = uma_zcreate("harp nif", sizeof(struct atm_nif),
		    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		if (harp_nif_zone == NULL)
			panic("%s: nif_zone", __func__);

		harp_vcc_zone = uma_zcreate("harp vcc", sizeof(struct harp_vcc),
		    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
		if (harp_vcc_zone == NULL)
			panic("%s: vcc_zone", __func__);

		/* Create harp interfaces for all existing ATM interfaces */
		TAILQ_FOREACH(ifp, &ifnet, if_link)
			harp_attach(ifp);

		atm_harp_attach_p = harp_attach;
		atm_harp_detach_p = harp_detach;
		atm_harp_input_p = harp_input;
		break;

	  case MOD_UNLOAD:
		atm_harp_attach_p = NULL;
		atm_harp_detach_p = NULL;
		atm_harp_input_p = NULL;

		while (!LIST_EMPTY(&harp_softc_list))
			harp_detach(LIST_FIRST(&harp_softc_list)->parent);

		uma_zdestroy(harp_nif_zone);
		uma_zdestroy(harp_vcc_zone);

		break;
	}
	return (0);
}

static moduledata_t harp_mod = {
	"if_harp",
	harp_modevent,
	0
};

DECLARE_MODULE(harp, harp_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
