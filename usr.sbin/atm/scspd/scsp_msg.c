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
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * SCSP message-handling routines
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h> 
#include <netatm/queue.h> 
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
  
#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Copy CSAS records into a CA record
 *
 * Arguments:
 *	dcsp	pointer to DCS block for DCS
 *	cap	pointer to CA record for CSASs
 *
 *
 * Returns:
 *	none
 *
 */
static void
scsp_ca_csas_setup(dcsp, cap)
	Scsp_dcs	*dcsp;
	Scsp_ca		*cap;
{
	int		csas_len, len, mtu;
	Scsp_server	*ssp = dcsp->sd_server;
	Scsp_cse	*csep, *next_csep;
	Scsp_csa	*csap;

	/*
	 * Loop through pending CSAS records
	 */
	len = sizeof(struct scsp_nhdr) + sizeof(struct scsp_nmcp) +
			ssp->ss_lsid.id_len +
			dcsp->sd_dcsid.id_len;
	csas_len = sizeof(struct scsp_ncsa) +
			dcsp->sd_server->ss_id_len +
			dcsp->sd_server->ss_ckey_len;
	mtu = dcsp->sd_server->ss_mtu;
	for (csep = dcsp->sd_ca_csas;
			csep && (len < mtu - csas_len);
			csep = next_csep) {
		next_csep = csep->sc_next;
		csap = scsp_cse2csas(csep);
		LINK2TAIL(csap, Scsp_csa, cap->ca_csa_rec, next);
		len += csas_len;
		UNLINK(csep, Scsp_cse, dcsp->sd_ca_csas, sc_next);
		UM_FREE(csep);
		cap->ca_mcp.rec_cnt++;
	}
}


/*
 * Process CSA records from a CSU Request that may be in response to
 * CSAS records sent in a CSUS
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to received message
 *
 * Returns:
 *	none
 *
 */
void
scsp_csus_ack(dcsp, msg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
{
	Scsp_csu_msg	*csusp;
	Scsp_csa	*csap, *csasp, *next_csasp;

	/*
	 * If this isn't a CSU Request, or there's no outstanding CSUS,
	 * or the outstanding CSUS has already been satisfied, just
	 * return
	 */
	if (!msg || msg->sc_msg_type != SCSP_CSU_REQ_MSG ||
			!dcsp->sd_csus_rexmt_msg ||
			!dcsp->sd_csus_rexmt_msg->sc_csu_msg ||
			!dcsp->sd_csus_rexmt_msg->sc_csu_msg->csu_csa_rec)
		return;


	/*
	 * Loop through the CSASs in the CSUS message, checking for
	 * each in the CSA records of the received CSU Request
	 */
	csusp = dcsp->sd_csus_rexmt_msg->sc_csu_msg;
	for (csasp = csusp->csu_csa_rec; csasp; csasp = next_csasp) {
		next_csasp = csasp->next;
		for (csap = msg->sc_csu_msg->csu_csa_rec;
				csap; csap = csap->next) {
			/*
			 * If the records match, unlink and free the
			 * CSAS from the CSUS
			 */
			if (scsp_cmp_key(&csap->key, &csasp->key) == 0 &&
					scsp_cmp_key(&csap->key, &csasp->key) == 0 &&
					scsp_cmp_id(&csap->oid, &csasp->oid) == 0 &&
					csap->seq >= csasp->seq) {
				UNLINK(csasp, Scsp_csa,
						csusp->csu_csa_rec,
						next);
				SCSP_FREE_CSA(csasp);
				dcsp->sd_csus_rexmt_msg->sc_csu_msg->csu_mcp.rec_cnt--;
				break;
			}
		}
	}

	if (csusp->csu_csa_rec == (Scsp_csa *)0) {
		/*
		 * All CSASs in the CSUS message have been
		 * answered.  Stop the timer and free the
		 * saved message.
		 */
		HARP_CANCEL(&dcsp->sd_csus_rexmt_t);
		scsp_free_msg(dcsp->sd_csus_rexmt_msg);
		dcsp->sd_csus_rexmt_msg = (Scsp_msg *)0;

		/*
		 * If the CRL isn't empty, send another CSUS
		 */
		if (dcsp->sd_crl) {
			(void)scsp_send_csus(dcsp);
		}
	}
}


/*
 * Send a CA message
 *
 * Arguments:
 *	dcsp	pointer to DCS block for DCS
 *
 * Returns:
 *	0	message sent OK
 *	else	errno indicating reason for failure
 *
 */
int
scsp_send_ca(dcsp)
	Scsp_dcs	*dcsp;
{
	int		rc;
	Scsp_msg	*ca_msg;
	Scsp_ca		*cap;
	Scsp_server	*ssp = dcsp->sd_server;

	/*
	 * Get memory for a CA message
	 */
	ca_msg = (Scsp_msg *)UM_ALLOC(sizeof(Scsp_msg));
	if (!ca_msg) {
		scsp_mem_err("scsp_send_ca: sizeof(Scsp_msg)");
	}
	cap = (Scsp_ca *)UM_ALLOC(sizeof(Scsp_ca));
	if (!cap) {
		scsp_mem_err("scsp_send_ca: sizeof(Scsp_ca)");
	}
	UM_ZERO(ca_msg, sizeof(Scsp_msg));
	UM_ZERO(cap, sizeof(Scsp_ca));

	/*
	 * Fill out constant fields
	 */
	ca_msg->sc_msg_type = SCSP_CA_MSG;
	ca_msg->sc_ca = cap;
	cap->ca_seq = dcsp->sd_ca_seq;
	cap->ca_mcp.pid = ssp->ss_pid;
	cap->ca_mcp.sgid = ssp->ss_sgid;
	cap->ca_mcp.sid = ssp->ss_lsid;
	cap->ca_mcp.rid = dcsp->sd_dcsid;

	/*
	 * Fill out state-dependent fields
	 */
	switch(dcsp->sd_ca_state) {
	case SCSP_CAFSM_NEG:
		cap->ca_m = 1;
		cap->ca_i = 1;
		cap->ca_o = 1;
		break;
	case SCSP_CAFSM_MASTER:
		cap->ca_m = 1;
		cap->ca_i = 0;
		scsp_ca_csas_setup(dcsp, cap);
		cap->ca_o = dcsp->sd_ca_csas != (Scsp_cse *)0;
		break;
	case SCSP_CAFSM_SLAVE:
		cap->ca_m = 0;
		cap->ca_i = 0;
		scsp_ca_csas_setup(dcsp, cap);
		cap->ca_o = dcsp->sd_ca_csas != (Scsp_cse *)0;
		break;
	default:
		scsp_log(LOG_ERR, "Invalid state in scsp_send_ca");
		abort();
	}

	/*
	 * Send the CA message and save a pointer to it in case
	 * it needs to be retransmitted
	 */
	rc = scsp_send_msg(dcsp, ca_msg);
	if (rc == 0) {
		dcsp->sd_ca_rexmt_msg = ca_msg;
	} else {
		scsp_free_msg(ca_msg);
	}

	return(rc);
}


/*
 * Send a CSU Solicit message
 *
 * Arguments:
 *	dcsp	pointer to DCS block for DCS
 *
 * Returns:
 *	0	message sent OK
 *	else	errno indicating reason for failure
 *
 */
int
scsp_send_csus(dcsp)
	Scsp_dcs	*dcsp;
{
	int		csas_len, len, mtu, rc;
	Scsp_msg	*csus_msg;
	Scsp_csu_msg	*csusp;
	Scsp_csa	*csasp, *next_csasp;
	Scsp_server	*ssp = dcsp->sd_server;

	/*
	 * If we have a mesage saved for retransmission, use it.
	 * If not, get memory for a new one.
	 */
	if (dcsp->sd_csus_rexmt_msg) {
		csus_msg = dcsp->sd_csus_rexmt_msg;
		csusp = csus_msg->sc_csu_msg;
	} else {
		/*
		 * Get memory for a CSUS message
		 */
		csus_msg = (Scsp_msg *)UM_ALLOC(sizeof(Scsp_msg));
		if (!csus_msg) {
			scsp_mem_err("scsp_send_csus: sizeof(Scsp_msg)");
		}
		csusp = (Scsp_csu_msg *)UM_ALLOC(sizeof(Scsp_csu_msg));
		if (!csusp) {
			scsp_mem_err("scsp_send_csus: sizeof(Scsp_csu_msg)");
		}
		UM_ZERO(csus_msg, sizeof(Scsp_msg));
		UM_ZERO(csusp, sizeof(Scsp_csu_msg));

		/*
		 * Fill out constant fields
		 */
		csus_msg->sc_msg_type = SCSP_CSUS_MSG;
		csus_msg->sc_csu_msg = csusp;
		csusp->csu_mcp.pid = ssp->ss_pid;
		csusp->csu_mcp.sgid = ssp->ss_sgid;
		csusp->csu_mcp.sid = ssp->ss_lsid;
		csusp->csu_mcp.rid = dcsp->sd_dcsid;
	}

	/*
	 * Move CSAS records from CRL into message
	 */
	mtu = dcsp->sd_server->ss_mtu;
	csas_len = sizeof(struct scsp_ncsa) + ssp->ss_id_len +
			ssp->ss_ckey_len;
	len = sizeof(struct scsp_nhdr) + sizeof(struct scsp_nmcp) +
			2 * ssp->ss_id_len +
			csas_len * (csusp->csu_mcp.rec_cnt + 1);
	for (csasp = dcsp->sd_crl;
			csasp && ((len + csas_len) < mtu);
			csasp = next_csasp, len += csas_len) {
		next_csasp = csasp->next;
		csusp->csu_mcp.rec_cnt++;
		UNLINK(csasp, Scsp_csa, dcsp->sd_crl, next);
		LINK2TAIL(csasp, Scsp_csa, csusp->csu_csa_rec, next);
		csasp->hops = 1;
	}

	/*
	 * Send the CSUS message and save a pointer to it in case
	 * it needs to be retransmitted
	 */
	rc = scsp_send_msg(dcsp, csus_msg);
	if (rc == 0) {
		/*
		 * Success--Save a pointer to the message and
		 * start the CSUS retransmit timer
		 */
		dcsp->sd_csus_rexmt_msg = csus_msg;
		HARP_TIMER(&dcsp->sd_csus_rexmt_t,
				dcsp->sd_csus_rexmt_int,
				scsp_csus_retran_timeout);
	} else {
		/*
		 * Error--free the CSUS message
		 */
		scsp_free_msg(csus_msg);
	}

	return(rc);
}


/*
 * Send a CSU Request message
 *
 * Arguments:
 *	dcsp	pointer to DCS block for DCS
 *	csap	pointer to CSAs to include
 *
 * Returns:
 *	0	message sent OK
 *	else	errno indicating reason for failure
 *
 */
int
scsp_send_csu_req(dcsp, csap)
	Scsp_dcs	*dcsp;
	Scsp_csa	*csap;
{
	int		rc;
	Scsp_server	*ssp = dcsp->sd_server;
	Scsp_csa	*cnt_csap;
	Scsp_msg	*csu_msg;
	Scsp_csu_msg	*csup;
	Scsp_csu_rexmt	*rxp;

	/*
	 * Return if CSA list is empty
	 */
	if (!csap)
		return(0);

	/*
	 * Get memory for a CSU Req message
	 */
	csu_msg = (Scsp_msg *)UM_ALLOC(sizeof(Scsp_msg));
	if (!csu_msg) {
		scsp_mem_err("scsp_send_csu_req: sizeof(Scsp_msg)");
	}
	csup = (Scsp_csu_msg *)UM_ALLOC(sizeof(Scsp_csu_msg));
	if (!csup) {
		scsp_mem_err("scsp_send_csu_req: sizeof(Scsp_csu_msg)");
	}
	UM_ZERO(csu_msg, sizeof(Scsp_msg));
	UM_ZERO(csup, sizeof(Scsp_csu_msg));

	/*
	 * Get memory for a CSU Req retransmission queue entry
	 */
	rxp = (Scsp_csu_rexmt *)UM_ALLOC(sizeof(Scsp_csu_rexmt));
	if (!rxp) {
		scsp_mem_err("scsp_send_csu_req: sizeof(Scsp_csu_rexmt)");
	}
	UM_ZERO(rxp, sizeof(Scsp_csu_rexmt));

	/*
	 * Fill out constant fields
	 */
	csu_msg->sc_msg_type = SCSP_CSU_REQ_MSG;
	csu_msg->sc_csu_msg = csup;
	csup->csu_mcp.pid = ssp->ss_pid;
	csup->csu_mcp.sgid = ssp->ss_sgid;
	csup->csu_mcp.sid = ssp->ss_lsid;
	csup->csu_mcp.rid = dcsp->sd_dcsid;

	/*
	 * Put the CSA list into the message
	 */
	csup->csu_csa_rec = csap;
	for (cnt_csap = csap; cnt_csap; cnt_csap = cnt_csap->next) {
		csup->csu_mcp.rec_cnt++;
	}

	/*
	 * Send the CSU Request
	 */
	rc = scsp_send_msg(dcsp, csu_msg);
	if (rc) {
		scsp_free_msg(csu_msg);
		return(rc);
	}
	UM_FREE(csu_msg);
	UM_FREE(csup);

	/*
	 * Save the CSA entries on the CSU Request retransmission
	 * queue and start the retransmission timer
	 */
	rxp->sr_dcs = dcsp;
	rxp->sr_csa = csap;
	HARP_TIMER(&rxp->sr_t, dcsp->sd_csu_rexmt_int,
			scsp_csu_req_retran_timeout);
	LINK2TAIL(rxp, Scsp_csu_rexmt, dcsp->sd_csu_rexmt, sr_next);

	return(0);
}


/*
 * Send a CSU Reply message
 *
 * Arguments:
 *	dcsp	pointer to DCS block for DCS
 *	csap	pointer to CSAs to include
 *
 * Returns:
 *	0	message sent OK
 *	errno	reason for failure
 *
 */
int
scsp_send_csu_reply(dcsp, csap)
	Scsp_dcs	*dcsp;
	Scsp_csa	*csap;
{
	int		rc;
	Scsp_server	*ssp = dcsp->sd_server;
	Scsp_csa	*csap1;
	Scsp_msg	*csu_msg;
	Scsp_csu_msg	*csup;

	/*
	 * Return if CSA list is empty
	 */
	if (!csap)
		return(0);

	/*
	 * Get memory for a CSU Reply message
	 */
	csu_msg = (Scsp_msg *)UM_ALLOC(sizeof(Scsp_msg));
	if (!csu_msg) {
		scsp_mem_err("scsp_send_csu_reply: sizeof(Scsp_msg)");
	}
	csup = (Scsp_csu_msg *)UM_ALLOC(sizeof(Scsp_csu_msg));
	if (!csup) {
		scsp_mem_err("scsp_send_csu_reply: sizeof(Scsp_csu_msg)");
	}
	UM_ZERO(csu_msg, sizeof(Scsp_msg));
	UM_ZERO(csup, sizeof(Scsp_csu_msg));

	/*
	 * Fill out constant fields
	 */
	csu_msg->sc_msg_type = SCSP_CSU_REPLY_MSG;
	csu_msg->sc_csu_msg = csup;
	csup->csu_mcp.pid = ssp->ss_pid;
	csup->csu_mcp.sgid = ssp->ss_sgid;
	csup->csu_mcp.sid = ssp->ss_lsid;
	csup->csu_mcp.rid = dcsp->sd_dcsid;

	/*
	 * Put the CSA list into the message.  Convert the CSAs into
	 * CSASs by freeing the protocol-specific portion.
	 */
	csup->csu_csa_rec = csap;
	for (csap1 = csap; csap1; csap1 = csap1->next) {
		switch(dcsp->sd_server->ss_pid) {
		/*
		 * We currently only support ATMARP
		 */
		case SCSP_PROTO_ATMARP:
			if (csap1->atmarp_data) {
				UM_FREE(csap1->atmarp_data);
				csap1->atmarp_data =
						(Scsp_atmarp_csa *)0;
			}
			break;
		}
		csup->csu_mcp.rec_cnt++;
	}

	/*
	 * Send the CSU Reply
	 */
	rc = scsp_send_msg(dcsp, csu_msg);
	scsp_free_msg(csu_msg);

	return(rc);
}


/*
 * Send a Hello message
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_send_hello(dcsp)
	Scsp_dcs	*dcsp;
{
	int		rc;
	Scsp_msg	*hello;
	Scsp_hello	*hp;

	/*
	 * Get memory for a Hello message
	 */
	hello = (Scsp_msg *)UM_ALLOC(sizeof(Scsp_msg));
	if (!hello) {
		scsp_mem_err("scsp_send_hello: sizeof(Scsp_msg)");
	}
	UM_ZERO(hello, sizeof(Scsp_msg));
	hp = (Scsp_hello *)UM_ALLOC(sizeof(Scsp_hello));
	if (!hp) {
		scsp_mem_err("scsp_send_hello: sizeof(Scsp_hello)");
	}
	UM_ZERO(hp, sizeof(Scsp_hello));

	/*
	 * Set up the Hello message
	 */
	hello->sc_msg_type = SCSP_HELLO_MSG;
	hello->sc_hello = hp;
	hp->hello_int = SCSP_HELLO_Interval;
	hp->dead_factor = SCSP_HELLO_DF;
	hp->family_id = dcsp->sd_server->ss_fid;
	hp->hello_mcp.pid = dcsp->sd_server->ss_pid;
	hp->hello_mcp.sgid = dcsp->sd_server->ss_sgid;
	hp->hello_mcp.flags = 0;
	hp->hello_mcp.rec_cnt = 0;
	hp->hello_mcp.sid = dcsp->sd_server->ss_lsid;
	hp->hello_mcp.rid = dcsp->sd_dcsid;

	/*
	 * Send and free the message
	 */
	rc = scsp_send_msg(dcsp, hello);
	scsp_free_msg(hello);

	return(rc);
}
