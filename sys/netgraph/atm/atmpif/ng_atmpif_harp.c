/*-
 * Copyright 2003 Harti Brandt
 * Copyright 2003 Vincent Jardin
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
 */

/*
 * ATM Virtal Adapter Support
 * --------------------------
 *
 * API between HARP and Netgraph
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <vm/uma.h>

#include <net/if.h>

#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_var.h>
#include <netatm/atm_ioctl.h>

#include <net/netisr.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/atm/ng_atmpif.h>
#include <netgraph/atm/atmpif/ng_atmpif_var.h>

/*
 * Local definitions
 */

/*
 * Local methods
 */

static int vatmpif_nunits = 0;

/*
 * ATM Interface services
 *
 * this virtual device does not use a soft SAR of the AAL5 PDU, neither
 * of the AAL3/4 PDU.
 */
static struct stack_defn	vatmpif_svaal5 = {
	sd_next: NULL,
	sd_sap: SAP_CPCS_AAL5,
	sd_flag: SDF_TERM,		/* no soft SAR */
	sd_inst: atm_dev_inst,
	sd_lower: atm_dev_lower,
	sd_upper: NULL,
	sd_toku: 0,
};
static struct stack_defn	vatmpif_svaal4 = {
	sd_next: &vatmpif_svaal5,
	sd_sap: SAP_CPCS_AAL3_4,
	sd_flag: SDF_TERM,		/* no soft SAR */
	sd_inst: atm_dev_inst,
	sd_lower: atm_dev_lower,
	sd_upper: NULL,
	sd_toku: 0,
};
static struct stack_defn	vatmpif_svaal0 = {
	sd_next: &vatmpif_svaal4,
	sd_sap: SAP_ATM,
	sd_flag: SDF_TERM,		/* no soft SAR */
	sd_inst: atm_dev_inst,
	sd_lower: atm_dev_lower,
	sd_upper: NULL,
	sd_toku: 0,
};
static struct stack_defn	*vatmpif_services = &vatmpif_svaal0;

/******************************************************************
		    HARP API METHODS
******************************************************************/

/*
 * Local methods
 */
static int vatmpif_harp_ioctl(int code, caddr_t data, caddr_t arg);
static int vatmpif_harp_instvcc(Cmn_unit *cup, Cmn_vcc *cvp);
static int vatmpif_harp_openvcc(Cmn_unit *cup, Cmn_vcc *cvp);
static int vatmpif_harp_closevcc(Cmn_unit *cup, Cmn_vcc *cvp);
static void vatmpif_harp_output(Cmn_unit *cup, Cmn_vcc *cvp, KBuffer *m);
static atm_intr_t vatmpif_harp_recv_stack;

/*
 * Attach an virtual ATM physical inteface with the HARP stack
 *
 * Each virtual ATM device interface must register itself here
 * upon completing the netgraph node constructor.
 *
 * Arguments:
 * 	node	pointer on the netgraph node
 *
 * Returns:
 * 	0	successful
 * 	errno	failed - reason indicated
 */
int
vatmpif_harp_attach(node_p node)
{
	Vatmpif_unit *vup;
	static int unit = 0;
	int err;

	/*
	 * Sanity check
	 */
	if (node == NULL)
		return (EINVAL);

	/*
	 * Get the virtual unit structure
	 */
	vup = (Vatmpif_unit *)NG_NODE_PRIVATE(node);
	if (vup == NULL)
		return (EINVAL);

	/*
	 * Start initializing the HARP binding
	 */
	vup->vu_unit = unit;
	/* 9188 bytes: Default ATM network interface MTU + LLC/SNAP header */
	vup->vu_mtu  = ATM_NIF_MTU + 8;
	vup->vu_vcc_zone = vatmpif_vcc_zone;
	vup->vu_nif_zone = vatmpif_nif_zone;
	vup->vu_ioctl = vatmpif_harp_ioctl;
	vup->vu_instvcc = vatmpif_harp_instvcc;
	vup->vu_openvcc = vatmpif_harp_openvcc;
	vup->vu_closevcc = vatmpif_harp_closevcc;
	vup->vu_output = vatmpif_harp_output;
	vup->vu_softc = vup;

	/*
	 * Consider this virtual unit assigned
	 */
	unit++;

	/*
	 * Get our device type and setup the adapter config info
	 * - at least as much as we can
	 */
	vup->vu_config.ac_vendor = VENDOR_NETGRAPH;
	vup->vu_config.ac_vendapi = VENDAPI_NETGRAPH_1;
	vup->vu_config.ac_device = DEV_VATMPIF;
	vup->vu_config.ac_media = MEDIA_VIRTUAL;
	vup->vu_config.ac_serial = (u_long)node;
	vup->vu_config.ac_bustype = BUS_VIRTUAL;
	vup->vu_config.ac_busslot = NGM_ATMPIF_COOKIE;
	vup->vu_config.ac_ram = (u_long)node;
	vup->vu_config.ac_ramsize = sizeof(*node);
	vup->vu_config.ac_macaddr = vup->conf.macaddr;
	snprintf(vup->vu_config.ac_hard_vers,
	    sizeof(vup->vu_config.ac_hard_vers),
	    "%s", "Virt. ATM 1.0");
	snprintf(vup->vu_config.ac_firm_vers,
	    sizeof(vup->vu_config.ac_firm_vers),
	    "%d", __FreeBSD__);

	/*
	 * Set the interface capabilities
	 */
	vup->vu_pif.pif_maxvpi = VATMPIF_MAX_VPI;
	vup->vu_pif.pif_maxvci = VATMPIF_MAX_VCI;
	vup->vu_pif.pif_pcr = vup->conf.pcr;

	/*
	 * Register this interface with ATM core services
	 */
	if ((err = atm_physif_register((Cmn_unit *)vup,
	    VATMPIF_DEV_NAME, vatmpif_services)) != 0 ) {
		/*
		 * Registration failed - back everything out
		 *
		 * The netgraph node must not be created.
		 */
		return (err);
	}

	vatmpif_nunits++;

	/*
	 * Mark device initialization completed
	 */
	vup->vu_flags |= CUF_INITED;

	/* Done */
	return (0);
}

/*
 * Halt driver processing
 *
 * This will be called just prior the destruction of the Netgraph's node.
 *
 * Arguments:
 * 	node	pointer on the netgraph node
 *
 * Returns:
 * 	0	detach was successful
 * 	errno	detach failed - reason indicated
 */
int
vatmpif_harp_detach(node_p node)
{
	Vatmpif_unit *vup = (Vatmpif_unit *)NG_NODE_PRIVATE(node);
	int err;

	/*
	 * Deregister device from kernel services
	 */
	if ((err = atm_physif_deregister((Cmn_unit *)vup)))
		return (err);

	vatmpif_nunits--;

	/*
	 * Clear device initialized
	 */
	vup->vu_flags &= ~CUF_INITED;

	/* Done */
	return (0);
}

/*
 * Handle netatm core service interface ioctl requests
 *
 * Arguments:
 * 	code		ioctl function (sub)code
 * 	data		data to/from ioctl
 * 	arg		optional code-specific argument
 *
 * Returns:
 * 	0		request processed successfully
 * 	errno		request failed - reason code
 */
static int
vatmpif_harp_ioctl(int code, caddr_t data, caddr_t arg)
{
	struct atminfreq		*aip = (struct atminfreq *)data;
	struct atm_pif			*pip;
	Vatmpif_unit			*vup;
	caddr_t				buf = aip->air_buf_addr;
	struct air_vinfo_rsp		*avr;
	size_t				count, len, buf_len = aip->air_buf_len;
	int				err = 0;
	char				ifname[2 * IFNAMSIZ];

	ATM_DEBUG3("%s: code=%d, opcode=%d\n", __func__, code, aip->air_opcode);

	switch (aip->air_opcode) {

	case AIOCS_INF_VST:
		/*
		 * Get vendor statistics
		 */
		pip = (struct atm_pif *)arg;
		vup = (Vatmpif_unit *)pip;
		if (pip == NULL)
			return (ENXIO);
		snprintf(ifname, sizeof(ifname), "%s%d",
		    pip->pif_name, pip->pif_unit);

		/*
		 * Cast response structure onto user's buffer
		 */
		avr = (struct air_vinfo_rsp *)(void *)buf;

		/*
		 * How lare is the response structure ?
		 */
		len = sizeof(struct air_vinfo_rsp);

		/*
		 * Sanity check - enough room for response structure
		 */
		if (buf_len < len)
			return ENOSPC;

		/*
		 * Copy interface name into response structure
		 */
		if ((err = copyout(ifname, avr->avsp_intf, IFNAMSIZ)) != 0)
			break;

		/*
		 * Advance the buffer address and decrement the size
		 */
		buf += len;
		buf_len -= len;

		/*
		 * Get the vendor stats
		 */
		/* vup->vu_stats */

		/*
		 * Stick as much of it as we have room for
		 * into the response
		 */
		count = MIN(sizeof(Vatmpif_stats), buf_len);

		/*
		 * Copy stats into user's buffer. Return value is
		 * amount of data copied.
		 */
		if ((err = copyout((caddr_t)&vup->vu_stats, buf,
		    buf_len)) != 0)
			break;
		buf += count;
		buf_len -= count;
		if (count < sizeof(Vatmpif_stats))
			err = ENOSPC;

		/*
		 * Record amount we are returning as vendor info...
		 */
		if ((err = copyout(&count, &avr->avsp_len, sizeof(count))) != 0)
			break;

		/*
		 * Update the reply pointers and lengths
		 */
		aip->air_buf_addr = buf;
		aip->air_buf_len = buf_len;
		break;

	default:
		err = ENOSYS;
		break;
	}

	return (err);
}

/*
 * Get CBR/VBR/ABR/UBR from bearer attribute
 *
 * Arguments:
 * 		bearer		T_ATM_BEARER_CAP option value structure
 *
 * Returns:
 * 		Driver traffic class
 */
static Vatmpif_traffic_type
vatmpif_bearerclass(struct attr_bearer *bearer)
{
	switch (bearer->v.bearer_class) {
	case T_ATM_CLASS_A:
		return (VATMPIF_TRAF_CBR);
	case T_ATM_CLASS_C:
		return (VATMPIF_TRAF_VBR);
	case T_ATM_CLASS_X:
		switch (bearer->v.traffic_type) {
		case T_ATM_CBR:
			return (VATMPIF_TRAF_CBR);
		case T_ATM_VBR:
			return (VATMPIF_TRAF_VBR);
		case T_ATM_ABR:
			return (VATMPIF_TRAF_ABR);
		case T_ATM_NULL:
		case T_ATM_UBR:
			return (VATMPIF_TRAF_UBR);
		}
		break;
	}

	/* never reached */
	log(LOG_ERR, "%s: could not determine the traffic type.\n", __func__);
	return (VATMPIF_TRAF_UBR);
}

/*
 * VCC Stack Instantiation
 *
 * This function is called via the common driver code during a device VCC
 * stack instantiation. The common code has already validated some of
 * the request so we just need to check a few more VATMPIF-specific details.
 *
 * Arguments:
 * 		cup		pointer to device common unit
 * 		cvp		pointer to common VCC entry
 *
 * Returns:
 * 		0		instantiation successful
 * 		errno	instantiation failed - reason indicated
 */
static int
vatmpif_harp_instvcc(Cmn_unit *cup, Cmn_vcc *cvp)
{
	Vatmpif_unit		*vup = (Vatmpif_unit *)cup;
	Vatmpif_vcc		*vvp = (Vatmpif_vcc *)cvp;
	Atm_attributes		*ap = &vvp->vv_connvc->cvc_attr;
	int32_t			pcr = 0;
	int32_t			scr = 0;
	Vatmpif_traffic_type	traffic = VATMPIF_TRAF_UBR;

	ATM_DEBUG3("%s: vup=%p, vvp=%p\n", __func__, vup, vvp);

	if (ap->traffic.tag == T_ATM_PRESENT) {
		pcr = ap->traffic.v.forward.PCR_all_traffic;
		scr = ap->traffic.v.forward.SCR_all_traffic;
	}
	if (pcr < 0)
		pcr = 0;
	if (scr < 0)
		scr = 0;

	KASSERT(ap->bearer.tag == T_ATM_PRESENT, ("Bearer tag is required"));
	traffic = vatmpif_bearerclass(&ap->bearer);
	/* Guarantee PCR of the PVC with CBR */
	if (traffic == VATMPIF_TRAF_CBR &&
	    vup->vu_cur_pcr + pcr > vup->vu_pif.pif_pcr) {
		return (EINVAL);
	}
	/* Guarantee SCR of the PVC with VBR */
	if (traffic == VATMPIF_TRAF_VBR &&
	    vup->vu_cur_pcr + scr > vup->vu_pif.pif_pcr) {
		return (EINVAL);
	}

	/*
	 * Validate requested AAL
	 */
	KASSERT(ap->aal.tag == T_ATM_PRESENT, ("AAL tag is required"));
	switch (ap->aal.type) {
	case ATM_AAL0:
		break;

	case ATM_AAL1:
		break;

	case ATM_AAL2:
		return (EINVAL);

	case ATM_AAL3_4:
		if (ap->aal.v.aal4.forward_max_SDU_size > vup->vu_mtu ||
		    ap->aal.v.aal4.backward_max_SDU_size > vup->vu_mtu)
			return (EINVAL);
		break;

	case ATM_AAL5:
		if (ap->aal.v.aal5.forward_max_SDU_size > vup->vu_mtu ||
		    ap->aal.v.aal5.backward_max_SDU_size > vup->vu_mtu)
			return (EINVAL);
		break;

	default:
		return (EINVAL);
	}
	/* Done */
	return (0);
}

/*
 * Open a VCC
 *
 * This function is called via the common driver code after receiving a
 * stack *_INIT command. The common has already validated most of
 * the request so we just need to check a few more VATMPIF-specific details.
 * Then we just forward to the Netgraph node.
 * 
 * Called at splimp.
 *
 * Arguments:
 * 	cup	pointer to device common unit
 * 	cvp	pointer to common VCC entry
 *
 * Returns:
 * 	0    	open successful
 * 	errno	open failed - reason indicated
 */
static int
vatmpif_harp_openvcc(Cmn_unit *cup, Cmn_vcc *cvp)
{
	Vatmpif_unit	*vup = (Vatmpif_unit *)cup;
	Vatmpif_vcc	*vvp = (Vatmpif_vcc *)cvp;
	struct vccb	*vcp = vvp->vv_connvc->cvc_vcc;
	Atm_attributes	*ap = &vvp->vv_connvc->cvc_attr;

	ATM_DEBUG5("%s: vup=%p, vvp=%p, vcc=(%d,%d)\n", __func__,
		vup, vvp, vcp->vc_vpi, vcp->vc_vci);

	/*
	 * We only need to open incoming VC's so outbound VC's
	 * just get set to CVS_ACTIVE state.
	 */
	if ((vcp->vc_type & VCC_IN) == 0) {
		/*
		 * Set the state and return - nothing else needed
		 */
		vvp->vv_state = CVS_ACTIVE;
		return (0);
	}

	/*
	 * Set the AAL and traffic
	 */
	switch (ap->aal.type) {
	case ATM_AAL0:
		vvp->vv_aal = VATMPIF_AAL_0;
		break;
	case ATM_AAL2:
		return (EINVAL);
	case ATM_AAL3_4:
		vvp->vv_aal = VATMPIF_AAL_4;
		break;
	case ATM_AAL5:
		vvp->vv_aal = VATMPIF_AAL_5;
		break;
	default:
		return (EINVAL);
	}
	vvp->vv_traffic_type = vatmpif_bearerclass(&ap->bearer);
	vvp->vv_traffic = ap->traffic.v;

	switch (vvp->vv_traffic_type) {
	case VATMPIF_TRAF_ABR:
		/* TODO */
	case VATMPIF_TRAF_UBR:
		break;
	case VATMPIF_TRAF_VBR:
		vup->vu_cur_pcr += vvp->vv_traffic.forward.SCR_all_traffic;
		break;
	case VATMPIF_TRAF_CBR:
		vup->vu_cur_pcr += vvp->vv_traffic.forward.PCR_all_traffic;
		break;
	}

	/*
	 * Indicate VC active
	 */
	vvp->vv_state = CVS_ACTIVE;

	/* Done */
	return (0);
}

/*
 * Close a VCC
 *
 * This function is called via the common driver code after receiving a
 * stack *_TERM command. The common code has already validated most of
 * the request so we just need to check a few more VATMPIF-specific detail.
 * Then we just remove the entry from the list.
 *
 * Arguments:
 * 	cup	pointer to device common unit
 * 	cvp	pointer to common VCC entry
 *
 * Returns:
 * 	0    	close successful
 * 	errno	close failed - reason indicated
 */
static int
vatmpif_harp_closevcc(Cmn_unit *cup, Cmn_vcc *cvp)
{
	Vatmpif_unit	*vup = (Vatmpif_unit *)cup;
	Vatmpif_vcc	*vvp = (Vatmpif_vcc *)cvp;
	struct vccb 	*vcp = vvp->vv_connvc->cvc_vcc;

	/* 
	 * If this is an outbound only VCI, then we can close
	 * immediately.
	 */
	if ((vcp->vc_type & VCC_IN) == 0) {
		/*
		 * The state will be set to TERM when we return
		 * to the *_TERM caller.
		 */
		return (0);
	}

	switch (vvp->vv_traffic_type) {
	case VATMPIF_TRAF_ABR:
		/* TODO */
	case VATMPIF_TRAF_UBR:
		break;
	case VATMPIF_TRAF_VBR:
		vup->vu_cur_pcr -= vvp->vv_traffic.forward.SCR_all_traffic;
		break;
	case VATMPIF_TRAF_CBR:
		vup->vu_cur_pcr -= vvp->vv_traffic.forward.PCR_all_traffic;
		break;
	}

	return (0);
}

/*
 * Output a PDU
 *
 * This function is called via the common driver code after receiving a
 * stack *_DATA* command. The command code has already validated most of
 * the request so we just need to check a few more VATMPIF-specific detail.
 * Then we just forward the transmit mbuf to the Netgraph node.
 *
 * Arguments:
 * 	cup	pointer to device common
 * 	cvp	pointer to common VCC entry
 * 	m	pointer to output PDU buffer chain head
 *
 * Returns:
 * 	none
 */
static void
vatmpif_harp_output(Cmn_unit *cup, Cmn_vcc *cvp, KBuffer *m)
{
	Vatmpif_unit	*vup = (Vatmpif_unit *)cup;
	Vatmpif_vcc	*vvp = (Vatmpif_vcc *)cvp;
	struct vccb	*vcp = vvp->vv_connvc->cvc_vcc;
	Atm_attributes	*ap = &vvp->vv_connvc->cvc_attr;
	int		err = 0;
	u_long     	pdulen = 0;

	if (IS_VATMPIF_DEBUG_PACKET(vup))
		atm_dev_pdu_print(cup, cvp, m, __func__);

	/*
	 * Get packet PDU length
	 */
	KB_PLENGET (m, pdulen);

	err = ng_atmpif_transmit(vup, m, vcp->vc_vpi, vcp->vc_vci,
	    0, 0, ap->aal.type);

	/*
	 * Now collect some statistics
	 */
	if (err) {
		vup->vu_pif.pif_oerrors++;
		vcp->vc_oerrors++;
		if (vcp->vc_nif)
			vcp->vc_nif->nif_if.if_oerrors++;
	} else {
		/*   
		 * Good transmission
		 */

		switch (ap->aal.type) {
		case VATMPIF_AAL_0:
			vup->vu_stats.hva_st_ng.ng_tx_rawcell++;
			break;
		case VATMPIF_AAL_4:
			/* TODO */
			break;
		case VATMPIF_AAL_5:
			vup->vu_stats.hva_st_aal5.aal5_xmit +=
			    (pdulen + 47) / 48;
			vup->vu_stats.hva_st_aal5.aal5_pdu_xmit++;
			break;
		default:
			log(LOG_ERR, "%s%d: unknown AAL while %s",
			    vup->vu_pif.pif_name, vup->vu_pif.pif_unit,
			    __func__);
		}

		vup->vu_pif.pif_opdus++;
		vup->vu_pif.pif_obytes += pdulen;
		if (vvp) {
			vcp = vvp->vv_connvc->cvc_vcc;
			vcp->vc_opdus++;
			vcp->vc_obytes += pdulen;
			if (vcp->vc_nif) {
				vcp->vc_nif->nif_obytes += pdulen;
				vcp->vc_nif->nif_if.if_opackets++;
				vcp->vc_nif->nif_if.if_obytes += pdulen;
			}
		}
	}
}

/*
 * Pass Incoming PDU up to the HARP stack
 *
 * This function is called via the core ATM interrupt queue callback
 * set in vatmpif_harp_recv_drain(). It will pass the supplied incoming
 * PDU up the incoming VCC's stack.
 *
 * Arguments:
 * 	tok	token to identify stack instantiation
 * 	m  	pointer to incoming PDU buffer chain
 *
 * Returns:
 * 	none
 */
static void
vatmpif_harp_recv_stack(void *tok, KBuffer *m)
{
	Vatmpif_vcc	*vvp = (Vatmpif_vcc *)tok;
	int        	err;

	/*
	 * Send the data up the stack
	 */
	STACK_CALL(CPCS_UNITDATA_SIG, vvp->vv_upper,
	    vvp->vv_toku, vvp->vv_connvc, (intptr_t)m, 0, err);
	if (err)
		KB_FREEALL(m);
}

/*
 * Drain Receive Queue
 *
 * The function will process all completed entries at the head of the
 * receive queue. The received segments will be linked into a received
 * PDU buffer cahin and it will then be passed up the PDU's VCC stack
 * function processing by the next higher protocol layer.
 *
 * May be called in interrupt state.
 * Must be called with interrupts locked out.
 *
 * Arguments:
 * 	vup	pointer to the virtual device structure
 * 	m	pointer to incoming PDU buffer chain
 * 	vpi	Virtual Path Identifier
 * 	vci	Virtual Channel Identifier (host order)
 * 	pt	Payload Type Identifier (3 bit)
 *	 		ATM_PT_USER_SDU0
 * 			ATM_PT_USER_SDU1
 * 			ATM_PT_USER_CONG_SDU0
 *	 		ATM_PT_USER_CONG_SDU1
 * 			ATM_PT_NONUSER
 * 			ATM_PT_OAMF5_SEG
 * 			ATM_PT_OAMF5_E2E
 * 	clp	Cell Loss Priority (1 bit)
 *
 * Returns:
 * 	0    	close successful
 * 	errno	close failed - reason indicated
 */
int
vatmpif_harp_recv_drain(Vatmpif_unit *vup, KBuffer *m,
    uint8_t vpi, uint16_t vci, uint8_t pt, uint8_t clp, Vatmpif_aal aal)
{
	Vatmpif_vcc	*vvp;
	struct vccb	*vcp;
	u_long     	pdulen = 0;
	caddr_t    	cp;
	int        	err = 0;

	/*
	 * Locate incoming VCC for this PDU
	 */
	vvp = (Vatmpif_vcc *)atm_dev_vcc_find((Cmn_unit *)vup,
	    vpi, vci, VCC_IN);

	if (vvp == NULL) {
		vup->vu_stats.hva_st_ng.ng_rx_novcc++;
		vup->vu_pif.pif_ierrors++;
		KB_FREEALL(m);
		err = EIO;
		goto failed;
	}

	switch (aal) {
	case VATMPIF_AAL_0:
		vup->vu_stats.hva_st_ng.ng_rx_rawcell++;
		break;
	case VATMPIF_AAL_4:
		/* TODO */
		break;
	case VATMPIF_AAL_5:
		vup->vu_stats.hva_st_aal5.aal5_rcvd += (pdulen + 47) / 48;
		vup->vu_stats.hva_st_aal5.aal5_pdu_rcvd++;
		break;
	default:
		vup->vu_stats.hva_st_ng.ng_badpdu++;
		vup->vu_pif.pif_ierrors++;
		KB_FREEALL(m);
		err = EINVAL;
		goto failed;
	}

	/*
	 * TODO:
	 * For now, only user data PDUs are supported
	 */
	if (pt & ATM_HDR_SET_PT(ATM_PT_NONUSER)) {
		vup->vu_stats.hva_st_ng.ng_badpdu++;
		vup->vu_pif.pif_ierrors++;
		if (aal == VATMPIF_AAL_5) {
			vup->vu_stats.hva_st_aal5.aal5_drops +=
			    (pdulen + 47) / 48;
			vup->vu_stats.hva_st_aal5.aal5_pdu_drops++;
		}
		err = EINVAL;
		goto failed;
	}

	if (IS_VATMPIF_DEBUG_PACKET(vup))
		atm_dev_pdu_print((Cmn_unit *)vup, (Cmn_vcc *)vvp, m,
		    __FUNCTION__);

	/*
	 * Get packet PDU length
	 */
	KB_PLENGET(m, pdulen);

	/*
	 * Only try queueing this if there is data
	 * to be handed up to the next layer.
	 */
	if (pdulen == 0) {
		/*
		 * Free zero-length buffer
		 */
		vup->vu_stats.hva_st_ng.ng_badpdu++;
		vup->vu_pif.pif_ierrors++;
		if (aal == VATMPIF_AAL_5)
			vup->vu_stats.hva_st_aal5.aal5_pdu_errs++;
		err = EIO;
		KB_FREEALL(m);
		goto failed;
	}

	/* TODO: process the AAL4 CRC, AAL5 CRC, 
	 * then update aal5_crc_len, aal5_drops, aal5_pdu_crc,
	 * aal5_pdu_errs, aal5_pdu_drops ...
	 */

	/*
	 * Quick count the PDU
	 */
	vup->vu_pif.pif_ipdus++;
	vup->vu_pif.pif_ibytes += pdulen;

	vup->vu_stats.hva_st_ng.ng_rx_pdu++;
	vup->vu_stats.hva_st_atm.atm_rcvd += (pdulen + 47) / 48;

	/*
	 * Update the VCC statistics:
	 * XXX: This code should not be into the driver.
	 */
	vcp = vvp->vv_connvc->cvc_vcc;
	if (vcp) {
		vcp->vc_ipdus++;
		vcp->vc_ibytes += pdulen;
		/*
		 * Update the NIF statistics if any
		 * XXX: beurk !
		 */
		if (vcp->vc_nif) {
			vcp->vc_nif->nif_ibytes += pdulen;
			vcp->vc_nif->nif_if.if_ipackets++;
			vcp->vc_nif->nif_if.if_ibytes += pdulen;
		}
	}

	/*
	 * The STACK_CALL needs to happen at splnet() in order
	 * for the stack sequence processing to work. Schedule an
	 * interrupt queue callback at splnet().
	 */

	/*
	 * Prepend callback function pointer and token value to buffer.
	 * We have already guaranteed that the space is available in the
	 * first buffer because the vatmpif_header structure is greater
	 * than our callback pointer.
	 * XXX 
	 */
	KB_HEADADJ(m, sizeof(atm_intr_func_t) + sizeof(void *));
	KB_DATASTART(m, cp, caddr_t);
	*((atm_intr_func_t *) cp) = vatmpif_harp_recv_stack;
	cp += sizeof (atm_intr_func_t);
	*((void **)cp) = (void *)vvp;

	/*
	 * Schedule callback
	 */
	if ((err = netisr_queue(NETISR_ATM, m))) {	/* (0) on success. */
		/*
		 * queue is full. Unable to pass up to the HARP stack
		 * Update the stats.
		 */
		vup->vu_stats.hva_st_ng.ng_rx_iqfull++;
		vup->vu_pif.pif_ierrors++;
		goto failed;
	}

	/* Done */
	return (0);

failed:
	return (err);
}
