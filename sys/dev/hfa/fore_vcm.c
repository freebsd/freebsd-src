/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Virtual Channel Management
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>
#include <pci/pcivar.h>
#include <dev/hfa/fore.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>
#include <dev/hfa/fore_var.h>
#include <dev/hfa/fore_include.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * VCC Stack Instantiation
 * 
 * This function is called via the common driver code during a device VCC
 * stack instantiation.  The common code has already validated some of
 * the request so we just need to check a few more Fore-specific details.
 *
 * Called at splnet.
 *
 * Arguments:
 *	cup	pointer to device common unit
 *	cvp	pointer to common VCC entry
 *
 * Returns:
 *	0	instantiation successful
 *	err 	instantiation failed - reason indicated
 *
 */
int
fore_instvcc(cup, cvp)
	Cmn_unit	*cup;
	Cmn_vcc		*cvp;
{
	Fore_vcc	*fvp = (Fore_vcc *)cvp;
	Atm_attributes	*ap = &fvp->fv_connvc->cvc_attr;

	/*
	 * Validate requested AAL
	 */
	switch (ap->aal.type) {

	case ATM_AAL0:
		fvp->fv_aal = FORE_AAL_0;
		break;

	case ATM_AAL3_4:
		fvp->fv_aal = FORE_AAL_4;
		if ((ap->aal.v.aal4.forward_max_SDU_size > FORE_IFF_MTU) ||
		    (ap->aal.v.aal4.backward_max_SDU_size > FORE_IFF_MTU))
			return (EINVAL);
		break;

	case ATM_AAL5:
		fvp->fv_aal = FORE_AAL_5;
		if ((ap->aal.v.aal5.forward_max_SDU_size > FORE_IFF_MTU) ||
		    (ap->aal.v.aal5.backward_max_SDU_size > FORE_IFF_MTU))
			return (EINVAL);
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

static const u_int rate_tab[255] = {
    353207, /*   0 */
    312501, /*   1 */ 312501, /*   2 */
    312501, /*   3 */ 312501, /*   4 */
    312501, /*   5 */ 312501, /*   6 */
    312501, /*   7 */ 312501, /*   8 */
    312501, /*   9 */ 312501, /*  10 */
    312501, /*  11 */ 312501, /*  12 */
    312501, /*  13 */ 312501, /*  14 */
    312501, /*  15 */ 312501, /*  16 */
    312501, /*  17 */ 284091, /*  18 */
    284091, /*  19 */ 284091, /*  20 */
    284091, /*  21 */ 284091, /*  22 */
    284091, /*  23 */ 284091, /*  24 */
    284091, /*  25 */ 284091, /*  26 */
    284091, /*  27 */ 284091, /*  28 */
    284091, /*  29 */ 284091, /*  30 */
    284091, /*  31 */ 284091, /*  32 */
    284091, /*  33 */ 284091, /*  34 */
    284091, /*  35 */ 284091, /*  36 */
    284091, /*  37 */ 284091, /*  38 */
    260417, /*  39 */ 260417, /*  40 */
    260417, /*  41 */ 260417, /*  42 */
    260417, /*  43 */ 260417, /*  44 */
    260417, /*  45 */ 260417, /*  46 */
    260417, /*  47 */ 260417, /*  48 */
    260417, /*  49 */ 260417, /*  50 */
    260417, /*  51 */ 260417, /*  52 */
    260417, /*  53 */ 260417, /*  54 */
    260417, /*  55 */ 240385, /*  56 */
    240385, /*  57 */ 240385, /*  58 */
    240385, /*  59 */ 240385, /*  60 */
    240385, /*  61 */ 240385, /*  62 */
    240385, /*  63 */ 240385, /*  64 */
    240385, /*  65 */ 240385, /*  66 */
    240385, /*  67 */ 240385, /*  68 */
    240385, /*  69 */ 240385, /*  70 */
    223215, /*  71 */ 223215, /*  72 */
    223215, /*  73 */ 223215, /*  74 */
    223215, /*  75 */ 223215, /*  76 */
    223215, /*  77 */ 223215, /*  78 */
    223215, /*  79 */ 223215, /*  80 */
    223215, /*  81 */ 223215, /*  82 */
    223215, /*  83 */ 208334, /*  84 */
    208334, /*  85 */ 208334, /*  86 */
    208334, /*  87 */ 208334, /*  88 */
    208334, /*  89 */ 208334, /*  90 */
    208334, /*  91 */ 208334, /*  92 */
    208334, /*  93 */ 208334, /*  94 */
    195313, /*  95 */ 195313, /*  96 */
    195313, /*  97 */ 195313, /*  98 */
    195313, /* 101 */ 195313, /* 102 */
    195313, /* 103 */ 183824, /* 104 */
    183824, /* 105 */ 183824, /* 106 */
    183824, /* 107 */ 183824, /* 108 */
    183824, /* 109 */ 183824, /* 110 */
    183824, /* 111 */ 183824, /* 112 */
    173612, /* 113 */ 173612, /* 114 */
    173612, /* 115 */ 173612, /* 116 */
    173612, /* 117 */ 173612, /* 118 */
    173612, /* 119 */ 173612, /* 120 */
    164474, /* 121 */ 164474, /* 122 */
    164474, /* 123 */ 164474, /* 124 */
    164474, /* 125 */ 164474, /* 126 */
    164474, /* 127 */ 156250, /* 128 */
    156250, /* 129 */ 156250, /* 130 */
    156250, /* 131 */ 156250, /* 132 */
    156250, /* 133 */ 148810, /* 134 */
    148810, /* 135 */ 148810, /* 136 */
    148810, /* 137 */ 148810, /* 138 */
    148810, /* 139 */ 142046, /* 140 */
    142046, /* 141 */ 142046, /* 142 */
    142046, /* 143 */ 142046, /* 144 */
    135870, /* 145 */ 135870, /* 146 */
    135870, /* 147 */ 135870, /* 148 */
    130209, /* 149 */ 130209, /* 150 */
    130209, /* 151 */ 130209, /* 152 */
    130209, /* 153 */ 125000, /* 154 */
    125000, /* 155 */ 125000, /* 156 */
    125000, /* 157 */ 120193, /* 158 */
    120193, /* 159 */ 120193, /* 160 */
    115741, /* 161 */ 115741, /* 162 */
    115741, /* 163 */ 115741, /* 164 */
    111608, /* 165 */ 111608, /* 166 */
    111608, /* 167 */ 107759, /* 168 */
    107759, /* 169 */ 107759, /* 170 */
    104167, /* 171 */ 104167, /* 172 */
    104167, /* 173 */ 100807, /* 174 */
    100807, /* 175 */  97657, /* 176 */
     97657, /* 177 */  97657, /* 178 */
     94697, /* 179 */  94697, /* 180 */
     91912, /* 181 */  91912, /* 182 */
     89286, /* 183 */  89286, /* 184 */
     86806, /* 185 */  86806, /* 186 */
     84460, /* 187 */  84460, /* 188 */
     82237, /* 189 */  82237, /* 190 */
     80129, /* 191 */  78125, /* 192 */
     78126, /* 193 */  76220, /* 194 */
     74405, /* 195 */  74405, /* 196 */
     72675, /* 197 */  71023, /* 198 */
     69445, /* 199 */  69445, /* 200 */
     67935, /* 201 */  66490, /* 202 */
     65105, /* 203 */  63776, /* 204 */
     62500, /* 205 */  61275, /* 206 */
     60097, /* 207 */  58963, /* 208 */
     57871, /* 209 */  56819, /* 210 */
     54825, /* 211 */  53880, /* 212 */
     52967, /* 213 */  51230, /* 214 */
     50404, /* 215 */  48829, /* 216 */
     47349, /* 217 */  46642, /* 218 */
     45290, /* 219 */  44015, /* 220 */
     42809, /* 221 */  41119, /* 222 */
     40065, /* 223 */  39063, /* 224 */
     37651, /* 225 */  36338, /* 226 */
     35113, /* 227 */  33968, /* 228 */
     32553, /* 229 */  31250, /* 230 */
     30049, /* 231 */  28936, /* 232 */
     27655, /* 233 */  26261, /* 234 */
     25000, /* 235 */  23855, /* 236 */
     22645, /* 237 */  21259, /* 238 */
     20033, /* 239 */  18826, /* 240 */
     17557, /* 241 */  16277, /* 242 */
     15025, /* 243 */  13767, /* 244 */
     12551, /* 245 */  11282, /* 246 */
     10017, /* 247 */   8779, /* 248 */
      7531, /* 249 */   6263, /* 250 */
      5017, /* 251 */   3761, /* 252 */
      2509, /* 253 */   1254, /* 254 */
};

/*
 * Find the best match of the high part of the Rate Control Information
 * 
 * This function is called when a VC is opened in order to help
 * in converting Fore's rate to PCR.
 * The Fore's Rate Control Information is encoded as 32-bit field
 * comprised of two 16-bit subfields.
 *
 * Arguments:
 *	*pcr		Peak Cell Rate, will be updated with actual value
 *
 * Returns:
 *	descr		the rate descriptor
 *
 */
static uint32_t
pcr2rate(int32_t *pcr)
{
	u_int i;

	if (*pcr >= rate_tab[0]) {
		/* special case link rate */
		*pcr = rate_tab[0];
		return (0);
	}

	for (i = 0; i < sizeof(rate_tab) / sizeof(rate_tab[0]); i++)
		if (*pcr >= rate_tab[i])
			break;
	if (i == sizeof(rate_tab) / sizeof(rate_tab[0])) {
		/* smaller than smallest */
		i--;
	}
	/* update with the actual value */
	*pcr = rate_tab[i];
	return ((255 - i) << 16) | i;
}

/*
 * Open a VCC
 * 
 * This function is called via the common driver code after receiving a
 * stack *_INIT command.  The common code has already validated most of
 * the request so we just need to check a few more Fore-specific details.
 * Then we just issue the command to the CP.  Note that we can't wait around
 * for the CP to process the command, so we return success for now and abort
 * the connection if the command later fails.
 *
 * Called at splimp.
 *
 * Arguments:
 *	cup	pointer to device common unit
 *	cvp	pointer to common VCC entry
 *
 * Returns:
 *	0	open successful
 *	else 	open failed
 *
 */
int
fore_openvcc(cup, cvp)
	Cmn_unit	*cup;
	Cmn_vcc		*cvp;
{
	Fore_unit	*fup = (Fore_unit *)cup;
	Fore_vcc	*fvp = (Fore_vcc *)cvp;
	H_cmd_queue	*hcp;
	Cmd_queue	*cqp;
	struct vccb	*vcp;

	vcp = fvp->fv_connvc->cvc_vcc;

	ATM_DEBUG4("fore_openvcc: fup=%p, fvp=%p, vcc=(%d,%d)\n", 
		fup, fvp, vcp->vc_vpi, vcp->vc_vci);

	/*
	 * Validate the VPI and VCI values
	 */
	if ((vcp->vc_vpi > fup->fu_pif.pif_maxvpi) ||
	    (vcp->vc_vci > fup->fu_pif.pif_maxvci)) {
		return (1);
	}

	/*
	 * Compute the PCR (but only for outgoing VCCs)
	 */
	fvp->rate = FORE_DEF_RATE;
	if ((vcp->vc_type & VCC_OUT) && cvp->cv_connvc) {
		Atm_attributes *attr = &cvp->cv_connvc->cvc_attr;

		if (attr && attr->traffic.v.forward.PCR_all_traffic > 0 &&
		    attr->traffic.v.forward.PCR_all_traffic < rate_tab[0] &&
		    (fup->fu_shape == FUS_SHAPE_ALL ||
		    (fup->fu_shape == FUS_SHAPE_ONE &&
		    fup->fu_num_shaped == 0))) {
			fvp->rate = pcr2rate(&attr->traffic.v.forward.
			    PCR_all_traffic);
			fup->fu_num_shaped++;
		}
	}

	/*
	 * Only need to tell the CP about incoming VCCs
	 */
	if ((vcp->vc_type & VCC_IN) == 0) {
		DEVICE_LOCK((Cmn_unit *)fup);
		fup->fu_open_vcc++;
		fvp->fv_state = CVS_ACTIVE;
		DEVICE_UNLOCK((Cmn_unit *)fup);
		return (0);
	}

	/*
	 * Queue command at end of command queue
	 */
	hcp = fup->fu_cmd_tail;
	if ((*hcp->hcq_status) & QSTAT_FREE) {

		/*
		 * Queue entry available, so set our view of things up
		 */
		hcp->hcq_code = CMD_ACT_VCCIN;
		hcp->hcq_arg = fvp;
		fup->fu_cmd_tail = hcp->hcq_next;
		fvp->fv_flags |= FVF_ACTCMD;

		/*
		 * Now set the CP-resident queue entry - the CP will grab
		 * the command when the op-code is set.
		 */
		cqp = hcp->hcq_cpelem;
		(*hcp->hcq_status) = QSTAT_PENDING;
		cqp->cmdq_act.act_vccid = CP_WRITE(vcp->vc_vci);
		if (fvp->fv_aal == FORE_AAL_0)
			cqp->cmdq_act.act_batch = CP_WRITE(1);
		cqp->cmdq_act.act_spec = CP_WRITE(
			ACT_SET_SPEC(BUF_STRAT_1, fvp->fv_aal,
				CMD_ACT_VCCIN | CMD_INTR_REQ));
	} else {
		/*
		 * Command queue full
		 */
		fup->fu_stats->st_drv.drv_cm_full++;
		return (1);
	}

	return (0);
}


/*
 * Close a VCC
 * 
 * This function is called via the common driver code after receiving a
 * stack *_TERM command.  The common code has already validated most of
 * the request so we just need to check a few more Fore-specific details.
 * Then we just issue the command to the CP.  Note that we can't wait around
 * for the CP to process the command, so we return success for now and whine
 * if the command later fails.
 *
 * Called at splimp.
 *
 * Arguments:
 *	cup	pointer to device common unit
 *	cvp	pointer to common VCC entry
 *
 * Returns:
 *	0	close successful
 *	else 	close failed
 *
 */
int
fore_closevcc(cup, cvp)
	Cmn_unit	*cup;
	Cmn_vcc		*cvp;
{
	Fore_unit	*fup = (Fore_unit *)cup;
	Fore_vcc	*fvp = (Fore_vcc *)cvp;
	H_xmit_queue	*hxp;
	H_cmd_queue	*hcp;
	Cmd_queue	*cqp;
	struct vccb	*vcp;
	int		i, err = 0;

	vcp = fvp->fv_connvc->cvc_vcc;

	ATM_DEBUG4("fore_closevcc: fup=%p, fvp=%p, vcc=(%d,%d)\n", 
		fup, fvp, vcp->vc_vpi, vcp->vc_vci);

	DEVICE_LOCK((Cmn_unit *)fup);

	/*
	 * Clear any references to this VCC in our transmit queue
	 */
	for (hxp = fup->fu_xmit_head, i = 0;
	     (*hxp->hxq_status != QSTAT_FREE) && (i < XMIT_QUELEN);
	     hxp = hxp->hxq_next, i++) {
		if (hxp->hxq_vcc == fvp) {
			hxp->hxq_vcc = NULL;
		}
	}

	/*
	 * Clear any references to this VCC in our command queue
	 */
	for (hcp = fup->fu_cmd_head, i = 0;
	     (*hcp->hcq_status != QSTAT_FREE) && (i < CMD_QUELEN);
	     hcp = hcp->hcq_next, i++) {
		switch (hcp->hcq_code) {

		case CMD_ACT_VCCIN:
		case CMD_ACT_VCCOUT:
			if (hcp->hcq_arg == fvp) {
				hcp->hcq_arg = NULL;
			}
			break;
		}
	}

	/*
	 * If this VCC has been previously activated, then we need to tell
	 * the CP to deactivate it.
	 */
	if (fvp->fv_flags & FVF_ACTCMD) {

		/*
		 * Queue command at end of command queue
		 */
		hcp = fup->fu_cmd_tail;
		if ((*hcp->hcq_status) & QSTAT_FREE) {

			/*
			 * Queue entry available, so set our view of things up
			 */
			hcp->hcq_code = CMD_DACT_VCCIN;
			hcp->hcq_arg = fvp;
			fup->fu_cmd_tail = hcp->hcq_next;

			/*
			 * Now set the CP-resident queue entry - the CP will 
			 * grab the command when the op-code is set.
			 */
			cqp = hcp->hcq_cpelem;
			(*hcp->hcq_status) = QSTAT_PENDING;
			cqp->cmdq_dact.dact_vccid = CP_WRITE(vcp->vc_vci);
			cqp->cmdq_dact.dact_cmd =
				CP_WRITE(CMD_DACT_VCCIN|CMD_INTR_REQ);
		} else {
			/*
			 * Command queue full
			 *
			 * If we get here, we'll be getting out-of-sync with
			 * the CP because we can't (for now at least) do
			 * anything about close errors in the common code.
			 * This won't be too bad, since we'll just toss any
			 * PDUs received from the VCC and the sigmgr's will
			 * always get open failures when trying to use this
			 * (vpi,vci)...oh, well...always gotta have that one
			 * last bug to fix! XXX
			 */
			fup->fu_stats->st_drv.drv_cm_full++;
			err = 1;
		}
	}

	/*
	 * Finish up...
	 */
	if (fvp->fv_state == CVS_ACTIVE)
		fup->fu_open_vcc--;

	if (fvp->rate != 0)
		fup->fu_num_shaped--;

	DEVICE_UNLOCK((Cmn_unit *)fup);

	return (err);
}

