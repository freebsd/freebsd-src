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
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/eventhandler.h>
#include <machine/clock.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_ioctl.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>
#include <pci/pcireg.h>
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

	DEVICE_UNLOCK((Cmn_unit *)fup);

	return (err);
}

