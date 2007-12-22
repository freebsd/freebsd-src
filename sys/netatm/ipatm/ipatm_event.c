/*-
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
 */

/*
 * IP Over ATM Support
 * -------------------
 *
 * IP VCC event handler
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <netinet/in.h>
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

#include <netatm/ipatm/ipatm.h>
#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>

/*
 * Process an IP VCC timeout
 * 
 * Called when a previously scheduled ipvcc control block timer expires.  
 * Processing will be based on the current ipvcc state.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to ipvcc timer control block
 *
 * Returns:
 *	none
 *
 */
void
ipatm_timeout(tip)
	struct atm_time	*tip;
{
	struct ipvcc	*ivp;

	/*
	 * Back-off to ipvcc control block
	 */
	ivp = (struct ipvcc *)
		((caddr_t)tip - offsetof(struct ipvcc, iv_time));

	/*
	 * Process timeout based on protocol state
	 */
	switch (ivp->iv_state) {

	case IPVCC_PMAP:
		/*
		 * Give up waiting for arp response
		 */
		(void) ipatm_closevc(ivp, T_ATM_CAUSE_TEMPORARY_FAILURE);
		break;

	case IPVCC_POPEN:
	case IPVCC_PACCEPT:
		/*
		 * Give up waiting for signalling manager response
		 */
		(void) ipatm_closevc(ivp, T_ATM_CAUSE_TEMPORARY_FAILURE);
		break;

	case IPVCC_ACTPENT:
		/*
		 * Try again to get an ARP entry
		 */
		switch ((*ivp->iv_ipnif->inf_serv->is_arp_pvcopen)(ivp)) {

		case MAP_PROCEEDING:
			/*
			 * Wait for answer
			 */
			ivp->iv_state = IPVCC_ACTIVE;
			break;

		case MAP_VALID:
			/*
			 * We've got our answer already
			 */
			ivp->iv_state = IPVCC_ACTIVE;
			ivp->iv_flags |= IVF_MAPOK;
			ivp->iv_dst.s_addr = ivp->iv_arpent->am_dstip.s_addr;
			break;

		case MAP_FAILED:
			/*
			 * Try again later
			 */
			IPVCC_TIMER(ivp, 5 * ATM_HZ);
			break;

		default:
			panic("ipatm_timeout: invalid am_pvcopen return");
		}
		break;

	default:
		log(LOG_ERR, "ipatm: invalid timer state: ivp=%p, state=%d\n",
			ivp, ivp->iv_state);
	}
}


/*
 * Process IP VCC Connected Notification
 * 
 * Arguments:
 *	toku	owner's connection token (ipvcc protocol block)
 *
 * Returns:
 *	none
 *
 */
void
ipatm_connected(toku)
	void		*toku;
{
	struct ipvcc	*ivp = (struct ipvcc *)toku;

	/*
	 * SVC is connected
	 */
	if ((ivp->iv_state != IPVCC_POPEN) &&
	    (ivp->iv_state != IPVCC_PACCEPT)) {
		log(LOG_ERR, "ipatm: invalid CALL_CONNECTED state=%d\n",
			ivp->iv_state);
		return;
	}

	/*
	 * Verify possible negotiated parameter values
	 */
	if (ivp->iv_state == IPVCC_POPEN) {
		Atm_attributes	*ap = &ivp->iv_conn->co_connvc->cvc_attr;
		int		mtu = (ivp->iv_flags & IVF_LLC) ?
						ATM_NIF_MTU + IPATM_LLC_LEN :
						ATM_NIF_MTU;

		/*       
		 * Verify final MTU
		 */     
		if (ap->aal.type == ATM_AAL5) {
			if ((ap->aal.v.aal5.forward_max_SDU_size < mtu) ||
			    (ap->aal.v.aal5.backward_max_SDU_size > mtu)) {
				(void) ipatm_closevc(ivp,
				      T_ATM_CAUSE_AAL_PARAMETERS_NOT_SUPPORTED);
				return;
			}
		} else {
			if ((ap->aal.v.aal4.forward_max_SDU_size < mtu) ||
			    (ap->aal.v.aal4.backward_max_SDU_size > mtu)) {
				(void) ipatm_closevc(ivp,
				      T_ATM_CAUSE_AAL_PARAMETERS_NOT_SUPPORTED);
				return;
			}
		}
	}

	/*
	 * Finish up VCC activation
	 */
	ipatm_activate(ivp);
}


/*
 * Process IP VCC Cleared Notification
 * 
 * Arguments:
 *	toku	owner's connection token (ipvcc protocol block)
 *	cause	pointer to cause code
 *
 * Returns:
 *	none
 *
 */
void
ipatm_cleared(toku, cause)
	void		*toku;
	struct t_atm_cause	*cause;
{
	struct ipvcc	*ivp = (struct ipvcc *)toku;


	/*
	 * VCC has been cleared, so figure out what's next
	 */
	ivp->iv_conn = NULL;

	switch (ivp->iv_state) {

	case IPVCC_POPEN:
		/*
		 * Call setup failed, see if there is another
		 * set of vcc parameters to try
		 */
		ivp->iv_state = IPVCC_CLOSED;
		if (ipatm_retrysvc(ivp)) {
			(void) ipatm_closevc(ivp, cause->cause_value);
		}
		break;

	case IPVCC_PACCEPT:
	case IPVCC_ACTPENT:
	case IPVCC_ACTIVE:
		ivp->iv_state = IPVCC_CLOSED;
		(void) ipatm_closevc(ivp, cause->cause_value);
		break;
	}
}


/*
 * Process an ARP Event Notification
 * 
 * Arguments:
 *	ivp	pointer to IP VCC control block
 *	event	ARP event type
 *
 * Returns:
 *	none
 *
 */
void
ipatm_arpnotify(ivp, event)
	struct ipvcc	*ivp;
	int		event;
{
	struct sockaddr_in	sin;
	struct ifnet		*ifp;

	/*
	 * Process event
	 */
	switch (event) {

	case MAP_VALID:
		switch (ivp->iv_state) {

		case IPVCC_PMAP:
			/*
			 * We've got our destination, however, first we'll
			 * check to make sure no other VCC to our destination
			 * has also had it's ARP table entry completed.
			 * If we don't find a better VCC to use, then we'll
			 * go ahead and open this SVC.
			 */
			sin.sin_family = AF_INET;
			sin.sin_addr.s_addr = ivp->iv_dst.s_addr;
			if (ipatm_iptovc(&sin, ivp->iv_ipnif->inf_nif) != ivp) {
				/*
				 * We found a better VCC, so use it and
				 * get rid of this VCC
				 */
				if (ivp->iv_queue) {
					ifp = (struct ifnet *)
						ivp->iv_ipnif->inf_nif;
					(void) ipatm_ifoutput(ifp, 
						ivp->iv_queue, 
						(struct sockaddr *)&sin);
					ivp->iv_queue = NULL;
				}
				(void) ipatm_closevc(ivp,
						T_ATM_CAUSE_UNSPECIFIED_NORMAL);

			} else {
				/*
				 * We like this VCC...
				 */
				ivp->iv_flags |= IVF_MAPOK;
				if (ipatm_opensvc(ivp)) {
					(void) ipatm_closevc(ivp,
						T_ATM_CAUSE_TEMPORARY_FAILURE);
				}
			}
			break;

		case IPVCC_POPEN:
		case IPVCC_PACCEPT:
		case IPVCC_ACTIVE:
			/*
			 * Everything looks good, so accept new mapping
			 */
			ivp->iv_flags |= IVF_MAPOK;
			ivp->iv_dst.s_addr = ivp->iv_arpent->am_dstip.s_addr;

			/*
			 * Send queued packet
			 */
			if ((ivp->iv_state == IPVCC_ACTIVE) && ivp->iv_queue) {
				sin.sin_family = AF_INET;
				sin.sin_addr.s_addr = ivp->iv_dst.s_addr;
				ifp = (struct ifnet *)ivp->iv_ipnif->inf_nif;
				(void) ipatm_ifoutput(ifp, ivp->iv_queue, 
					(struct sockaddr *)&sin);
				ivp->iv_queue = NULL;
			}
			break;
		}
		break;

	case MAP_INVALID:
		switch (ivp->iv_state) {

		case IPVCC_POPEN:
		case IPVCC_PACCEPT:
		case IPVCC_ACTIVE:

			/*
			 * Mapping has gone stale, so we cant use this VCC
			 * until the mapping is refreshed
			 */
			ivp->iv_flags &= ~IVF_MAPOK;
			break;
		}
		break;

	case MAP_FAILED:
		/*
		 * ARP lookup failed, just trash it all
		 */
		(void) ipatm_closevc(ivp, T_ATM_CAUSE_TEMPORARY_FAILURE);
		break;

	case MAP_CHANGED:
		/*
		 * ARP mapping has changed
		 */
		if (ivp->iv_flags & IVF_PVC) {
			/*
			 * For PVCs, just reset lookup cache if needed
			 */
			if (last_map_ipvcc == ivp) {
				last_map_ipdst = 0;
				last_map_ipvcc = NULL;
			}
		} else {
			/*
			 * Close SVC if it has already used this mapping
			 */
			switch (ivp->iv_state) {

			case IPVCC_POPEN:
			case IPVCC_ACTIVE:
				(void) ipatm_closevc(ivp,
					T_ATM_CAUSE_UNSPECIFIED_NORMAL);
				break;
			}
		}
		break;

	default:
		log(LOG_ERR, "ipatm: unknown arp event %d, ivp=%p\n",
			event, ivp);
	}
}


/*
 * Process an IP VCC idle timer tick
 * 
 * This function is called every IPATM_IDLE_TIME seconds, in order to
 * scan for idle IP VCC's.  If an active VCC reaches the idle time limit,
 * then it will be closed.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to the VCC idle timer control block
 *
 * Returns:
 *	none
 *
 */
void
ipatm_itimeout(tip)
	struct atm_time	*tip;
{
	struct ipvcc	*ivp, *inext;
	struct ip_nif	*inp;


	/*
	 * Schedule next timeout
	 */
	atm_timeout(&ipatm_itimer, IPATM_IDLE_TIME, ipatm_itimeout);

	/*
	 * Check for disabled idle timeout
	 */
	if (ipatm_vcidle == 0)
		return;

	/*
	 * Check out all IP VCCs
	 */
	for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
		for (ivp = Q_HEAD(inp->inf_vcq, struct ipvcc); ivp;
				ivp = inext) {

			inext = Q_NEXT(ivp, struct ipvcc, iv_elem);

			/*
			 * Looking for active, idle SVCs
			 */
			if (ivp->iv_flags & (IVF_PVC | IVF_NOIDLE))
				continue;
			if (ivp->iv_state != IPVCC_ACTIVE)
				continue;
			if (++ivp->iv_idle < ipatm_vcidle)
				continue;

			/*
			 * OK, we found one - close the VCC
			 */
			(void) ipatm_closevc(ivp,
					T_ATM_CAUSE_UNSPECIFIED_NORMAL);
		}
	}
}

