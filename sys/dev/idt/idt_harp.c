/*
 * Copyright (c) 2000, 2001 Richard Hodges and Matriplex, inc.
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
 *    This product includes software developed by Matriplex, inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 *
 * This driver is derived from the Nicstar driver by Mark Tinguely, and
 * some of the original driver still exists here.  Those portions are...
 *   Copyright (c) 1996, 1997, 1998, 1999 Mark Tinguely
 *   All rights reserved.
 *
 ******************************************************************************
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>

#include <sys/bus.h>
#include <sys/conf.h>

#include <sys/module.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/netisr.h>
#include <net/if_var.h>

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

#include <dev/idt/idtreg.h>
#include <dev/idt/idtvar.h>

/******************************************************************************
 *
 * HARP-specific definitions
 *
 */

#define	IDT_DEV_NAME	"idt"

#define	IDT_IFF_MTU	9188
#define	IDT_MAX_VCI	1023	/* 0 - 1023 */
#define	IDT_MAX_VPI	0

#define iv_next		iv_cmn.cv_next
#define iv_toku		iv_cmn.cv_toku
#define iv_upper	iv_cmn.cv_upper
#define iv_vccb		iv_cmn.cv_connvc	/* HARP 3.0 */
#define iv_state	iv_cmn.cv_state
#define	iu_pif		iu_cmn.cu_pif
#define	iu_unit		iu_cmn.cu_unit
#define	iu_flags	iu_cmn.cu_flags
#define	iu_mtu		iu_cmn.cu_mtu
#define	iu_open_vcc	iu_cmn.cu_open_vcc
#define iu_instvcc	iu_cmn.cu_instvcc	/* HARP 3.0 */
#define	iu_vcc		iu_cmn.cu_vcc
#define	iu_vcc_zone	iu_cmn.cu_vcc_zone
#define	iu_nif_zone	iu_cmn.cu_nif_zone
#define	iu_ioctl	iu_cmn.cu_ioctl
#define	iu_openvcc	iu_cmn.cu_openvcc
#define	iu_closevcc	iu_cmn.cu_closevcc
#define	iu_output	iu_cmn.cu_output
#define	iu_config	iu_cmn.cu_config
#define	iu_softc	iu_cmn.cu_softc

/*
 * ATM Interface services
 */
static struct stack_defn idt_svaal5 = {
	NULL,
	SAP_CPCS_AAL5,
	SDF_TERM,
	atm_dev_inst,
	atm_dev_lower,
	NULL,
	0,
};
static struct stack_defn idt_svaal4 = {
	&idt_svaal5,
	SAP_CPCS_AAL3_4,
	SDF_TERM,
	atm_dev_inst,
	atm_dev_lower,
	NULL,
	0,
};
static struct stack_defn idt_svaal0 = {
	&idt_svaal4,
	SAP_ATM,
	SDF_TERM,
	atm_dev_inst,
	atm_dev_lower,
	NULL,
	0,
};
struct stack_defn *idt_services = &idt_svaal0;

extern uma_zone_t idt_nif_zone;
extern uma_zone_t idt_vcc_zone;

static int idt_atm_bearerclass(struct attr_bearer *);
#ifdef T_ATM_BUFQUEUE
static CONNECTION *idt_atm_harpconn(Cmn_unit *, Cmn_vcc *);
#endif
static int idt_atm_ioctl(int, caddr_t, caddr_t);

static void idt_output(Cmn_unit *, Cmn_vcc *, KBuffer *);
static int idt_openvcc(Cmn_unit *, Cmn_vcc *);
static int idt_closevcc(Cmn_unit *, Cmn_vcc *);
static int idt_instvcc(Cmn_unit *, Cmn_vcc *);

static void idt_recv_stack(void *, KBuffer *);

/******************************************************************************
 *
 *                       HARP GLUE SECTION
 *
 ******************************************************************************
 *
 * Handle netatm core service interface ioctl requests
 *
 * Called at splnet.
 *
 * Arguments:
 *    code       ioctl function (sub)code
 *    data       data to/from ioctl
 *    arg        optional code-specific argument
 *
 * Returns:
 *    0          request processed successfully
 *    error      request failed - reason code
 *
 */
static int
idt_atm_ioctl(int code, caddr_t addr, caddr_t arg)
{
#ifdef T_ATM_BUFQUEUE
	CONNECTION *connection;
	TX_QUEUE *txq;
	struct mbuf *m;
	Cmn_unit *cup;
	Cmn_vcc *cvp;
	int retval;
#endif

	switch (code) {

#ifdef T_ATM_BUFQUEUE
	case T_ATM_BUFQUEUE:
		cup = (Cmn_unit *) addr;
		cvp = (Cmn_vcc *) arg;
		connection = idt_atm_harpconn(cup, cvp);
		if (connection == NULL)
			return (-1);
		retval = 0;
		txq = connection->queue;
		if (txq == NULL)
			return (-1);
		for (m = txq->mget; m != NULL; m = m->m_nextpkt)
			retval += m->m_pkthdr.len;
		return (retval);
#endif
	}

	return (ENOSYS);
}

#ifdef T_ATM_BUFQUEUE
/*******************************************************************************
 *
 *  Get connection pointer from Cmn_unit and Cmn_vcc
 *
 *  in:  Cmn_unit and Cmn_vcc
 * out:  connection (NULL=error)
 *
 *  Date first: 05/31/2001  last: 05/31/2001
 */

static CONNECTION *
idt_atm_harpconn(Cmn_unit * cup, Cmn_vcc * cvp)
{
	struct vccb *vccinf;	/* from HARP struct */
	IDT *idt;
	int vpi;
	int vci;

	idt = (IDT *) cup;
	if (idt == NULL || cvp == NULL)
		return (NULL);

	if (cvp->cv_connvc == NULL)
		return (NULL);

	vccinf = cvp->cv_connvc->cvc_vcc;

	if (vccinf == NULL)
		return (NULL);

	vpi = vccinf->vc_vpi;
	vci = vccinf->vc_vci;

	return (idt_connect_find(idt, vpi, vci));
}
#endif	/* T_ATM_BUFQUEUE */

/*******************************************************************************
 *
 *  Get CBR/VBR/UBR class from bearer attribute
 *
 *  in:
 * out:  NICCBR/NICVBR/NICABR/NICUBR
 *
 *  Date first: 06/12/2001  last: 06/13/2001
 */

static int
idt_atm_bearerclass(struct attr_bearer * bearer)
{
	switch (bearer->v.bearer_class) {
		case T_ATM_CLASS_A:return (NICCBR);
	case T_ATM_CLASS_C:
		if (idt_sysctl_vbriscbr)
			return (NICCBR);	/* use CBR slots for VBR VC's */
		else
			return (NICVBR);
	case T_ATM_CLASS_X:
		if (bearer->v.traffic_type == T_ATM_CBR)
			return (NICCBR);
		if (bearer->v.traffic_type == T_ATM_VBR)
			return (NICVBR);
		return (NICUBR);
	}
	return (NICUBR);
}

/*  The flag idt_sysctl_vbriscbr allows us to set up a CBR VC as if it were
 *  VBR.  This is primarily to avoid cell loss at a switch that cannot seem
 *  to buffer one or two cells of jitter.  This jitter is created when many
 *  CBR slots have been taken, and a new CBR VC cannot use the optimally
 *  spaced slots, and has to use nearby slots instead.
 *
 *  In this case, we want to use the VC SCR as the CBR value.  The PCR and MBS
 *  is only of interest to the switch.
 *
 *******************************************************************************
 *
 *  Initialize HARP service
 *  called from device attach
 */

int
idt_harp_init(nicstar_reg_t *idt)
{
	long long tsc_val;
	u_char idt_mac[6];
	int i;
	int error;

	error = 0;

	/*
	 * Start initializing it
	 */
	idt->iu_unit = device_get_unit(idt->dev);
	idt->iu_mtu = IDT_IFF_MTU;
	idt->iu_ioctl = idt_atm_ioctl;
	idt->iu_openvcc = idt_openvcc;
	idt->iu_instvcc = idt_instvcc;
	idt->iu_closevcc = idt_closevcc;
	idt->iu_output = idt_output;
	idt->iu_vcc_zone = idt_vcc_zone;
	idt->iu_nif_zone = idt_nif_zone;
	idt->iu_softc = (void *)idt;

	/*
	 * Copy serial number into config space
	 */
	idt->iu_config.ac_serial = 0;

	idt->iu_config.ac_vendor = VENDOR_IDT;
	idt->iu_config.ac_vendapi = VENDAPI_IDT_1;
	idt->iu_config.ac_device = DEV_IDT_155;
	idt->iu_config.ac_media = MEDIA_UNKNOWN;
	idt->iu_config.ac_bustype = BUS_PCI;

	idt->iu_pif.pif_pcr = idt->cellrate_rmax;	/* ATM_PCR_OC3C; */
	idt->iu_pif.pif_maxvpi = idt->conn_maxvpi;
	idt->iu_pif.pif_maxvci = idt->conn_maxvci;

	snprintf(idt->iu_config.ac_hard_vers,
		 sizeof(idt->iu_config.ac_hard_vers),
		 idt->hardware);
	snprintf(idt->iu_config.ac_firm_vers,
		 sizeof(idt->iu_config.ac_firm_vers),
		 IDT_VERSION);
	/*
	 * Save device ram info for user-level programs NOTE: This really
	 * points to start of EEPROM and includes all the device registers
	 * in the lower 2 Megabytes.
	 */
	idt->iu_config.ac_ram = NULL;
	idt->iu_config.ac_ramsize = 0;

	for (i = 0; i < 6; i++) {
		idt_mac[i] = nicstar_eeprom_rd(idt, (0x6c + i));
	}

	/* looks like bad MAC */
	if ((idt_mac[3] | idt_mac[4] | idt_mac[5]) == 0) {
		GET_RDTSC(tsc_val);	/* 24 bits on 500mhz CPU is about
					 * 30msec */
		idt_mac[0] = 0x00;
		idt_mac[1] = 0x20;
		idt_mac[2] = 0x48;	/* use Fore prefix */
		idt_mac[3] = (tsc_val >> 16) & 0xff;
		idt_mac[4] = (tsc_val >> 8) & 0xff;
		idt_mac[5] = (tsc_val) & 0xff;
		device_printf(idt->dev,
			"Cannot read MAC address from EEPROM, generating it.\n");
	}
	bcopy(&idt_mac, &idt->iu_pif.pif_macaddr.ma_data, sizeof(idt_mac));

	device_printf(idt->dev, "MAC address %6D, HWrev=%d\n",
		(u_int8_t *)&idt->iu_pif.pif_macaddr.ma_data, ":",
		idt->pci_rev);

	idt->iu_config.ac_macaddr = idt->iu_pif.pif_macaddr;

	/*
	 * Register this interface with ATM core services
	 */
	error = atm_physif_register(&idt->iu_cmn, IDT_DEV_NAME, idt_services);
	if (error != 0) {
		/*
		 * Registration failed - back everything out
		 */

		log(LOG_ERR, "%s(): atm_physif_register failed\n", __func__);
		return (error);
	}
	idt->iu_flags |= CUF_INITED;

#if BSD >= 199506
	/*
	 * Add hook to out shutdown function at_shutdown (
	 * (bootlist_fn)idt_pci_shutdown, idt, SHUTDOWN_POST_SYNC );
	 */
#endif

	return (error);
}

/*******************************************************************************
 *
 *  Output data
 */

static void
idt_output(Cmn_unit * cmnunit, Cmn_vcc * cmnvcc, KBuffer * m)
{
	struct vccb *vccinf;	/* from HARP struct */
	IDT *idt;
	int vpi;
	int vci;
	int flags;

	idt = (IDT *) cmnunit;
	flags = 0;

	if (cmnvcc == NULL) {
		device_printf(idt->dev, "idt_output arg error #1\n");
		goto bad;
	}
	if (cmnvcc->cv_connvc == NULL) {
		device_printf(idt->dev, "idt_output arg error #2\n");
		goto bad;
	}
	vccinf = cmnvcc->cv_connvc->cvc_vcc;
	if (vccinf == NULL) {
		device_printf(idt->dev, "idt_output arg error #3\n");
		goto bad;
	}
	vpi = vccinf->vc_vpi;
	vci = vccinf->vc_vci;

#ifdef CVF_MPEG2TS		/* option to split bufs into small TS bufs */
	if (cmnvcc->cv_flags & CVF_MPEG2TS)
		flags = 1;
#endif

	idt_transmit(idt, m, vpi, vci, flags);

	return;
bad:
	m_freem(m);
	return;
}

/*******************************************************************************
 *
 *  Open VCC
 */

static int
idt_openvcc(Cmn_unit * cmnunit, Cmn_vcc * cmnvcc)
{
	Atm_attributes *attrib;	/* from HARP struct */
	struct vccb *vccinf;	/* from HARP struct */
	CONNECTION *connection;
	IDT *idt;
	int vpi;
	int vci;
	int class;		/* NICCBR, NICVBR, or NICUBR */

	idt = (IDT *) cmnunit;

	if (cmnvcc == NULL || cmnvcc->cv_connvc == NULL) {
		printf("idt_openvcc: bad request #1.\n");
		return (1);
	}
	attrib = &cmnvcc->cv_connvc->cvc_attr;
	vccinf = cmnvcc->cv_connvc->cvc_vcc;

	if (attrib == NULL || vccinf == NULL) {
		printf("idt_openvcc: bad request #2.\n");
		return (1);
	}
	vpi = vccinf->vc_vpi;
	vci = vccinf->vc_vci;

	connection = idt_connect_find(idt, vpi, vci);
	if (connection == NULL) {
		printf("idt_openvcc: vpi/vci invalid: %d/%d\n", vpi, vci);
		return (1);
	}
	if (connection->status) {
		printf("idt_openvcc: connection already open %d/%d\n", vpi, vci);
		return (1);
	}
	connection->status = 1;
	connection->recv = NULL;
	connection->rlen = 0;
	connection->maxpdu = 20000;
	connection->aal = IDTAAL5;
	connection->traf_pcr = attrib->traffic.v.forward.PCR_all_traffic;
	connection->traf_scr = attrib->traffic.v.forward.SCR_all_traffic;
	connection->vccinf = vccinf;	/* 12/15/2000 */

	if (connection->traf_pcr <= 0)
		connection->traf_pcr = connection->traf_scr;
	if (connection->traf_scr <= 0)
		connection->traf_scr = connection->traf_pcr;

	class = idt_atm_bearerclass(&attrib->bearer);
	if (vpi == 0 && vci == 5)
		class = NICABR;	/* higher priority than UBR */
	if (vpi == 0 && vci == 16)
		class = NICABR;

	if (connection->traf_pcr < 0) {	/* neither PCR nor SCR given */
		connection->traf_pcr = 1;
		connection->traf_scr = 1;
		class = NICUBR;	/* so give it lowest priority */
	}
	connection->class = class;

	if (idt_connect_txopen(idt, connection)) {
		device_printf(idt->dev, "cannot open connection for %d/%d\n",
			vpi, vci);
		return (1);
	}
	if (idt_sysctl_logvcs)
		printf("idt_openvcc: %d/%d, PCR=%d, SCR=%d\n", vpi, vci,
		    connection->traf_pcr, connection->traf_scr);
	idt_connect_opencls(idt, connection, 1);	/* open entry in rcv
							 * connect table */

	return (0);
}

/*  We really don't handle ABR, but use it as a higher priority UBR.  The
 *  idea is that a UBR connection that gives a PCR (like 0/16) should
 *  be given preference over a UBR connection that wants "everything else".
 *
 *  Note that CLASS_X is typically UBR, but the traffic type information
 *  element may still specify CBR or VBR.
 *
 *******************************************************************************
 *
 *  Close VCC
 */

static int
idt_closevcc(Cmn_unit * cmnunit, Cmn_vcc * cmnvcc)
{
	CONNECTION *connection;
	nicstar_reg_t *idt = (nicstar_reg_t *) cmnunit;
	int vpi;
	int vci;

	if (cmnvcc && cmnvcc->cv_connvc && cmnvcc->cv_connvc->cvc_vcc) {
		vpi = cmnvcc->cv_connvc->cvc_vcc->vc_vpi;
		vci = cmnvcc->cv_connvc->cvc_vcc->vc_vci;
	} else {
		printf("idt_closevcc: bad vcivpi\n");
		return (0);
	}
	connection = idt_connect_find(idt, vpi, vci);

	if (connection == NULL) {
		printf("idt_closevcc: vpi/vci invalid: %d/%d\n", vpi, vci);
		return (0);
	}
	idt_connect_opencls(idt, connection, 0);	/* close entry in rcv
							 * connect table */

	if (connection->status == 0)
		printf("idt_closevcc: close on empty connection %d/%d\n", vpi, vci);
	if (connection->recv != NULL)
		m_freem(connection->recv);	/* recycle mbuf of partial PDU */
	idt_connect_txclose(idt, connection);
	connection->status = 0;
	connection->recv = NULL;
	connection->rlen = 0;
	connection->maxpdu = 0;
	connection->aal = 0;
	connection->traf_pcr = 0;
	connection->traf_scr = 0;

	if (idt_sysctl_logvcs)
		printf("idt_closevcc: vpi=%d vci=%d\n", vpi, vci);

	return (0);
}

/*
 *
 * VCC Stack Instantiation
 *
 * This function is called via the common driver code during a device VCC
 * stack instantiation.  The common code has already validated some of
 * the request so we just need to check a few more IDT-specific details.
 *
 * Called at splnet.
 *
 * Arguments:
 *    cup    pointer to device common unit
 *    cvp    pointer to common VCC entry
 *
 * Returns:
 *    0    instantiation successful
 *    err     instantiation failed - reason indicated
 *
 */
static int
idt_instvcc(Cmn_unit * cmnunit, Cmn_vcc * cmnvcc)
{
	Atm_attributes *attrib;	/* from HARP struct */
	IDT *idt;
	int class, pcr, scr;
	int slots_vc, slots_cur, slots_max;

	if (cmnvcc == NULL)
		return (EINVAL);
	if (cmnvcc->cv_connvc == NULL)
		return (EINVAL);

	idt = (IDT *) cmnunit;
	if (idt == NULL)
		return (EINVAL);

	attrib = &cmnvcc->cv_connvc->cvc_attr;

	if (attrib == NULL)
		return (EINVAL);

	pcr = attrib->traffic.v.forward.PCR_all_traffic;
	scr = attrib->traffic.v.forward.SCR_all_traffic;

	if (pcr <= 0)
		pcr = scr;	/* if PCR missing, default to SCR */
	if (pcr <= 0)
		pcr = 1;
	if (scr <= 0)
		scr = pcr;

	class = idt_atm_bearerclass(&attrib->bearer);
	if (class == NICCBR) {
		slots_max = idt->txslots_max;
		slots_cur = idt->txslots_cur;
		slots_vc = idt_slots_cbr(idt, scr);	/* 06/13/2001: now using
							 * SCR */
		if (slots_vc + slots_cur > slots_max) {
			if (idt_sysctl_logvcs)
				device_printf(idt->dev,
					"Insufficient bandwidth (vc=%d cur=%d max=%d)\n",
				    	slots_vc, slots_cur, slots_max);
			return (EINVAL);
		}
	}
	/* This part was take from /sys/dev/hfa/fore_vcm.c */

	switch (attrib->aal.type) {
	case ATM_AAL0:
		break;
	case ATM_AAL3_4:
		if ((attrib->aal.v.aal4.forward_max_SDU_size > IDT_IFF_MTU) ||
		    (attrib->aal.v.aal4.backward_max_SDU_size > IDT_IFF_MTU))
			return (EINVAL);
		break;
	case ATM_AAL5:
		if ((attrib->aal.v.aal5.forward_max_SDU_size > IDT_IFF_MTU) ||
		    (attrib->aal.v.aal5.backward_max_SDU_size > IDT_IFF_MTU))
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

/*
 * Pass Incoming PDU up Stack
 *
 * This function is called via the core ATM interrupt queue callback
 * set in fore_recv_drain().  It will pass the supplied incoming
 * PDU up the incoming VCC's stack.
 *
 * Called at splnet.
 *
 * Arguments:
 *    tok        token to identify stack instantiation
 *    m        pointer to incoming PDU buffer chain
 *
 * Returns:
 *    none
 */
static void
idt_recv_stack(void *tok, KBuffer * m)
{
	Idt_vcc *ivp = (Idt_vcc *) tok;
	int err;

	if ((m->m_flags & M_PKTHDR) == 0) {
		printf("idt_recv_stack: Warning - mbuf chain has no header.\n");
		KB_FREEALL(m);
		return;
	}
	/*
	 * Send the data up the stack
	 */
	STACK_CALL(CPCS_UNITDATA_SIG, ivp->iv_upper,
	    ivp->iv_toku, ivp->iv_vccb, (int)m, 0, err);
	if (err)
		KB_FREEALL(m);

	return;
}

/******************************************************************************
 *
 *  Enqueue received PDU for HARP to handle
 *
 *  in:  IDT device, mbuf, vpi, vci
 *
 * Date last: 12/14/2000
 */

void
idt_receive(nicstar_reg_t * idt, struct mbuf * m, int vpi, int vci)
{
	caddr_t cp;
	Cmn_vcc *vcc;
	int space;

	/*
	 * The STACK_CALL needs to happen at splnet() in order for the stack
	 * sequence processing to work.  Schedule an interrupt queue
	 * callback at splnet() since we are currently at device level.
	 */

	/*
	 * Prepend callback function pointer and token value to buffer. We
	 * have already guaranteed that the space is available in the first
	 * buffer.
	 */

	/*
	 * vcc = atm_dev_vcc_find(&idt->iu_cmn, (vpivci>> 16), vpivci &
	 * 0xffff, VCC_IN);
	 */

	vcc = atm_dev_vcc_find(&idt->iu_cmn, vpi, vci, VCC_IN);

	if (vcc == NULL) {	/* harp stack not ready or no vcc */
		printf("idt_receive: no VCC %d/%d\n", vpi, vci);
		KB_FREEALL(m);
		return;
	}
	space = m->m_data - idt_mbuf_base(m);
	if (space < sizeof(atm_intr_func_t) + sizeof(int)) {
		printf("idt_receive: NOT enough buffer space (%d).\n", space);
		KB_FREEALL(m);
		return;
	}
	KB_HEADADJ(m, sizeof(atm_intr_func_t) + sizeof(int));
	KB_DATASTART(m, cp, caddr_t);
	*((atm_intr_func_t *) cp) = idt_recv_stack;
	cp += sizeof(atm_intr_func_t);

	*((void **)cp) = (void *)vcc;

	/*
	 * Schedule callback
	 */
	if (! netisr_queue(NETISR_ATM, m))
		KB_FREEALL(m);
}
