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
 * SCSP subroutines
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
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
#include <netatm/uni/unisig_var.h>

#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Hash an SCSP cache key
 *
 * Arguments:
 *	ckp	pointer to an SCSP cache key structure
 *
 * Returns:
 *	hashed value
 *
 */
int
scsp_hash(ckp)
	Scsp_ckey	*ckp;
{
	int	i, j, h;

	/*
	 * Turn cache key into a positive integer
	 */
	h = 0;
	for (i = ckp->key_len-1, j = 0;
			i > 0 && j < sizeof(int);
			i--, j++)
		h = (h << 8) + ckp->key[i];
	h = abs(h);

	/*
	 * Return the hashed value
	 */
	return(h % SCSP_HASHSZ);
}


/*
 * Compare two SCSP IDs
 *
 * Arguments:
 *	id1p	pointer to an SCSP ID structure
 *	id2p	pointer to an SCSP ID structure
 *
 * Returns:
 *	< 0	id1 is less than id2
 *	0	id1 and id2 are equal
 *	> 0	id1 is greater than id2
 *
 */
int
scsp_cmp_id(id1p, id2p)
	Scsp_id	*id1p;
	Scsp_id	*id2p;
{
	int	diff, i;

	/*
	 * Compare the two IDs, byte for byte
	 */
	for (i = 0; i < id1p->id_len && i < id2p->id_len; i++) {
		diff = id1p->id[i] - id2p->id[i];
		if (diff) {
			return(diff);
		}
	}

	/*
	 * IDs are equal.  If lengths differ, the longer ID is
	 * greater than the shorter.
	 */
	return(id1p->id_len - id2p->id_len);
}


/*
 * Compare two SCSP cache keys
 *
 * Arguments:
 *	ck1p	pointer to an SCSP cache key structure
 *	ck2p	pointer to an SCSP cache key structure
 *
 * Returns:
 *	< 0	ck1 is less than ck2
 *	0	ck1 and ck2 are equal
 *	> 0	ck1 is greater than ck2
 *
 */
int
scsp_cmp_key(ck1p, ck2p)
	Scsp_ckey	*ck1p;
	Scsp_ckey	*ck2p;
{
	int	diff, i;

	/*
	 * Compare the two keys, byte for byte
	 */
	for (i = 0; i < ck1p->key_len && i < ck2p->key_len; i++) {
		diff = ck1p->key[i] - ck2p->key[i];
		if (diff)
			return(diff);
	}

	/*
	 * Keys are equal.  If lengths differ, the longer key is
	 * greater than the shorter.
	 */
	return(ck1p->key_len - ck2p->key_len);
}


/*
 * Check whether the host system is an ATMARP server for
 * the LIS associated with a given interface
 *
 * Arguments:
 *	netif	pointer to the network interface name
 *
 * Returns:
 *	1	host is a server
 *	0	host is not a server
 *
 */
int
scsp_is_atmarp_server(netif)
	char	*netif;
{
	int			rc;
	int			buf_len = sizeof(struct air_asrv_rsp);
	struct atminfreq	air;
	struct air_asrv_rsp	*asrv_info;

	/*
	 * Get interface information from the kernel
	 */
	strcpy(air.air_int_intf, netif);
	air.air_opcode = AIOCS_INF_ASV;
	buf_len = do_info_ioctl(&air, buf_len);
	if (buf_len < 0)
		return(0);

	/*
	 * Check the interface's ATMARP server address
	 */
	asrv_info = (struct air_asrv_rsp *) air.air_buf_addr;
	rc = (asrv_info->asp_addr.address_format == T_ATM_ABSENT) &&
			(asrv_info->asp_subaddr.address_format ==
				T_ATM_ABSENT);
	UM_FREE(asrv_info);
	return(rc);
}


/*
 * Make a copy of a cache summary entry
 *
 * Arguments:
 *	csep	pointer to CSE entry to copy
 *
 * Returns:
 *	0	copy failed
 *	else	pointer to new CSE entry
 *
 */
Scsp_cse *
scsp_dup_cse(csep)
	Scsp_cse	*csep;
{
	Scsp_cse	*dupp;

	/*
	 * Allocate memory for the duplicate
	 */
	dupp = (Scsp_cse *)UM_ALLOC(sizeof(Scsp_cse));
	if (!dupp) {
		scsp_mem_err("scsp_dup_cse: sizeof(Scsp_cse)");
	}

	/*
	 * Copy data to the duplicate
	 */
	UM_COPY(csep, dupp, sizeof(Scsp_cse));
	dupp->sc_next = (Scsp_cse *)0;

	return(dupp);
}


/*
 * Make a copy of a CSA or CSAS record
 *
 * Arguments:
 *	csap	pointer to CSE entry to copy
 *
 * Returns:
 *	0	copy failed
 *	else	pointer to new CSA or CSAS record
 *
 */
Scsp_csa *
scsp_dup_csa(csap)
	Scsp_csa	*csap;
{
	Scsp_csa	*dupp;
	Scsp_atmarp_csa	*adp;

	/*
	 * Allocate memory for the duplicate
	 */
	dupp = (Scsp_csa *)UM_ALLOC(sizeof(Scsp_csa));
	if (!dupp) {
		scsp_mem_err("scsp_dup_csa: sizeof(Scsp_csa)");
	}

	/*
	 * Copy data to the duplicate
	 */
	UM_COPY(csap, dupp, sizeof(Scsp_csa));
	dupp->next = (Scsp_csa *)0;

	/*
	 * Copy protocol-specific data, if it's present
	 */
	if (csap->atmarp_data) {
		adp = (Scsp_atmarp_csa *)UM_ALLOC(sizeof(Scsp_atmarp_csa));
		if (!adp) {
			scsp_mem_err("scsp_dup_csa: sizeof(Scsp_atmarp_csa)");
		}
		UM_COPY(csap->atmarp_data, adp, sizeof(Scsp_atmarp_csa));
		dupp->atmarp_data  = adp;
	}

	return(dupp);
}


/*
 * Copy a cache summary entry into a CSAS
 *
 * Arguments:
 *	csep	pointer to CSE entry to copy
 *
 * Returns:
 *	0	copy failed
 *	else	pointer to CSAS record summarizing the entry
 *
 */
Scsp_csa *
scsp_cse2csas(csep)
	Scsp_cse	*csep;
{
	Scsp_csa	*csap;

	/*
	 * Allocate memory for the duplicate
	 */
	csap = (Scsp_csa *)UM_ALLOC(sizeof(Scsp_csa));
	if (!csap) {
		scsp_mem_err("scsp_cse2csas: sizeof(Scsp_csa)");
	}
	UM_ZERO(csap, sizeof(Scsp_csa));

	/*
	 * Copy data to the CSAS entry
	 */
	csap->seq = csep->sc_seq;
	csap->key = csep->sc_key;
	csap->oid = csep->sc_oid;

	return(csap);
}


/*
 * Copy an ATMARP cache entry into a cache summary entry
 *
 * Arguments:
 *	aap	pointer to ATMARP cache entry to copy
 *
 * Returns:
 *	0	copy failed
 *	else	pointer to CSE record summarizing the entry
 *
 */
Scsp_cse *
scsp_atmarp2cse(aap)
	Scsp_atmarp_msg	*aap;
{
	Scsp_cse	*csep;

	/*
	 * Allocate memory for the duplicate
	 */
	csep = (Scsp_cse *)UM_ALLOC(sizeof(Scsp_cse));
	if (!csep) {
		scsp_mem_err("scsp_atmarp2cse: sizeof(Scsp_cse)");
	}
	UM_ZERO(csep, sizeof(Scsp_cse));

	/*
	 * Copy data to the CSE entry
	 */
	csep->sc_seq = aap->sa_seq;
	csep->sc_key = aap->sa_key;
	csep->sc_oid = aap->sa_oid;

	return(csep);
}


/*
 * Clean up a DCS block.  This routine is called to clear out any
 * lingering state information when the CA FSM reverts to an 'earlier'
 * state (Down or Master/Slave Negotiation).
 *
 * Arguments:
 *	dcsp	pointer to a DCS control block for the neighbor
 *
 * Returns:
 *	none
 *
 */
void
scsp_dcs_cleanup(dcsp)
	Scsp_dcs	*dcsp;
{
	Scsp_cse	*csep, *ncsep;
	Scsp_csa	*csap, *next_csap;
	Scsp_csu_rexmt	*rxp, *rx_next;

	/*
	 * Free any CSAS entries waiting to be sent
	 */
	for (csep = dcsp->sd_ca_csas; csep; csep = ncsep) {
		ncsep = csep->sc_next;
		UNLINK(csep, Scsp_cse, dcsp->sd_ca_csas, sc_next);
		UM_FREE(csep);
	}

	/*
	 * Free any entries on the CRL
	 */
	for (csap = dcsp->sd_crl; csap; csap = next_csap) {
		next_csap = csap->next;
		UNLINK(csap, Scsp_csa, dcsp->sd_crl, next);
		SCSP_FREE_CSA(csap);
	}

	/*
	 * Free any saved CA message and cancel the CA
	 * retransmission timer
	 */
	if (dcsp->sd_ca_rexmt_msg) {
		scsp_free_msg(dcsp->sd_ca_rexmt_msg);
		dcsp->sd_ca_rexmt_msg = (Scsp_msg *)0;
	}
	HARP_CANCEL(&dcsp->sd_ca_rexmt_t);

	/*
	 * Free any saved CSU Solicit message and cancel the CSUS
	 * retransmission timer
	 */
	if (dcsp->sd_csus_rexmt_msg) {
		scsp_free_msg(dcsp->sd_csus_rexmt_msg);
		dcsp->sd_csus_rexmt_msg = (Scsp_msg *)0;
	}
	HARP_CANCEL(&dcsp->sd_csus_rexmt_t);

	/*
	 * Free any entries on the CSU Request retransmission queue
	 */
	for (rxp = dcsp->sd_csu_rexmt; rxp; rxp = rx_next) {
		rx_next = rxp->sr_next;
		HARP_CANCEL(&rxp->sr_t);
		for (csap = rxp->sr_csa; csap; csap = next_csap) {
			next_csap = csap->next;
			SCSP_FREE_CSA(csap);
		}
		UNLINK(rxp, Scsp_csu_rexmt, dcsp->sd_csu_rexmt,
				sr_next);
		UM_FREE(rxp);
	}
}


/*
 * Delete an SCSP DCS block and any associated information
 *
 * Arguments:
 *	dcsp	pointer to a DCS control block to delete
 *
 * Returns:
 *	none
 *
 */
void
scsp_dcs_delete(dcsp)
	Scsp_dcs	*dcsp;
{
	Scsp_cse	*csep, *next_cse;
	Scsp_csu_rexmt	*rxp, *next_rxp;
	Scsp_csa	*csap, *next_csa;

	/*
	 * Cancel any pending DCS timers
	 */
	HARP_CANCEL(&dcsp->sd_open_t);
	HARP_CANCEL(&dcsp->sd_hello_h_t);
	HARP_CANCEL(&dcsp->sd_hello_rcv_t);
	HARP_CANCEL(&dcsp->sd_ca_rexmt_t);
	HARP_CANCEL(&dcsp->sd_csus_rexmt_t);

	/*
	 * Unlink the DCS block from the server block
	 */
	UNLINK(dcsp, Scsp_dcs, dcsp->sd_server->ss_dcs, sd_next);

	/*
	 * Close the VCC to the DCS, if one is open
	 */
	if (dcsp->sd_sock != -1) {
		(void)close(dcsp->sd_sock);
	}

	/*
	 * Free any saved CA message
	 */
	if (dcsp->sd_ca_rexmt_msg) {
		scsp_free_msg(dcsp->sd_ca_rexmt_msg);
	}

	/*
	 * Free any pending CSAs waiting for cache alignment
	 */
	for (csep = dcsp->sd_ca_csas; csep; csep = next_cse) {
		next_cse = csep->sc_next;
		UM_FREE(csep);
	}

	/*
	 * Free anything on the cache request list
	 */
	for (csap = dcsp->sd_crl; csap; csap = next_csa) {
		next_csa = csap->next;
		SCSP_FREE_CSA(csap);
	}

	/*
	 * Free any saved CSUS message
	 */
	if (dcsp->sd_csus_rexmt_msg) {
		scsp_free_msg(dcsp->sd_csus_rexmt_msg);
	}

	/*
	 * Free anything on the CSU Request retransmit queue
	 */
	for (rxp = dcsp->sd_csu_rexmt; rxp; rxp = next_rxp) {
		/*
		 * Cancel the retransmit timer
		 */
		HARP_CANCEL(&rxp->sr_t);

		/*
		 * Free the CSAs to be retransmitted
		 */
		for (csap = rxp->sr_csa; csap; csap = next_csa) {
			next_csa = csap->next;
			SCSP_FREE_CSA(csap);
		}

		/*
		 * Free the CSU Req retransmission control block
		 */
		next_rxp = rxp->sr_next;
		UM_FREE(rxp);
	}

	/*
	 * Free the DCS block
	 */
	UM_FREE(dcsp);
}


/*
 * Shut down a server.  This routine is called when a connection to
 * a server is lost.  It will clear the server's state without deleting
 * the server.
 *
 * Arguments:
 *	ssp	pointer to a server control block
 *
 * Returns:
 *	none
 *
 */
void
scsp_server_shutdown(ssp)
	Scsp_server	*ssp;
{
	int		i;
	Scsp_dcs	*dcsp;
	Scsp_cse	*csep;

	/*
	 * Trace the shutdown
	 */
	if (scsp_trace_mode & (SCSP_TRACE_IF_MSG | SCSP_TRACE_CFSM)) {
		scsp_trace("Server %s being shut down\n",
				ssp->ss_name);
	}

	/*
	 * Terminate up all the DCS connections and clean
	 * up the control blocks
	 */
	for (dcsp = ssp->ss_dcs; dcsp; dcsp = dcsp->sd_next) {
		if (dcsp->sd_sock != -1) {
			(void)close(dcsp->sd_sock);
			dcsp->sd_sock = -1;
		}
		HARP_CANCEL(&dcsp->sd_open_t);
		HARP_CANCEL(&dcsp->sd_hello_h_t);
		HARP_CANCEL(&dcsp->sd_hello_rcv_t);
		scsp_dcs_cleanup(dcsp);
		dcsp->sd_hello_state = SCSP_HFSM_DOWN;
		dcsp->sd_ca_state = SCSP_CAFSM_DOWN;
		dcsp->sd_client_state = SCSP_CIFSM_NULL;
	}

	/*
	 * Clean up the server control block
	 */
	if (ssp->ss_sock != -1) {
		(void)close(ssp->ss_sock);
		ssp->ss_sock = -1;
	}
	if (ssp->ss_dcs_lsock != -1) {
		(void)close(ssp->ss_dcs_lsock);
		ssp->ss_dcs_lsock = -1;
	}
	ssp->ss_state = SCSP_SS_NULL;

	/*
	 * Free the entries in the server's summary cache
	 */
	for (i = 0; i < SCSP_HASHSZ; i++) {
		while (ssp->ss_cache[i]) {
			csep = ssp->ss_cache[i];
			UNLINK(csep, Scsp_cse, ssp->ss_cache[i],
					sc_next);
			UM_FREE(csep);
		}
	}
}


/*
 * Delete an SCSP server block and any associated information
 *
 * Arguments:
 *	ssp	pointer to a server control block to delete
 *
 * Returns:
 *	none
 *
 */
void
scsp_server_delete(ssp)
	Scsp_server	*ssp;
{
	int		i;
	Scsp_dcs	*dcsp, *next_dcs;
	Scsp_cse	*csep, *next_cse;

	/*
	 * Unlink the server block from the chain
	 */
	UNLINK(ssp, Scsp_server, scsp_server_head, ss_next);

	/*
	 * Free the DCS blocks associated with the server
	 */
	for (dcsp = ssp->ss_dcs; dcsp; dcsp = next_dcs) {
		next_dcs = dcsp->sd_next;
		scsp_dcs_delete(dcsp);
	}

	/*
	 * Free the entries in the server's summary cache
	 */
	for (i = 0; i < SCSP_HASHSZ; i++) {
		for (csep = ssp->ss_cache[i]; csep; csep = next_cse) {
			next_cse = csep->sc_next;
			UM_FREE(csep);
		}
	}

	/*
	 * Free the server block
	 */
	UM_FREE(ssp->ss_name);
	UM_FREE(ssp);
}


/*
 * Get informtion about a server from the kernel
 *
 * Arguments:
 *	ssp	pointer to the server block
 *
 * Returns:
 *	0	server info is OK
 *	errno	server is not ready
 *
 */
int
scsp_get_server_info(ssp)
	Scsp_server	*ssp;
{
	int			i, len, mtu, rc, sel;
	struct atminfreq	air;
	struct air_netif_rsp	*netif_rsp = (struct air_netif_rsp *)0;
	struct air_int_rsp	*intf_rsp = (struct air_int_rsp *)0;
	struct air_cfg_rsp	*cfg_rsp = (struct air_cfg_rsp *)0;
	struct sockaddr_in	*ip_addr;
	Atm_addr_nsap		*anp;

	/*
	 * Make sure we're the server for the interface
	 */
	if (!scsp_is_atmarp_server(ssp->ss_intf)) {
		rc = EINVAL;
		goto server_info_done;
	}

	/*
	 * Get the IP address and physical interface name
	 * associated with the network interface
	 */
	UM_ZERO(&air, sizeof(struct atminfreq));
	air.air_opcode = AIOCS_INF_NIF;
	strcpy(air.air_netif_intf, ssp->ss_intf);
	len = do_info_ioctl(&air, sizeof(struct air_netif_rsp));
	if (len <= 0) {
		rc = EIO;
		goto server_info_done;
	}
	netif_rsp = (struct air_netif_rsp *)air.air_buf_addr;

	ip_addr = (struct sockaddr_in *)&netif_rsp->anp_proto_addr;
	if (ip_addr->sin_family != AF_INET ||
			ip_addr->sin_addr.s_addr == 0) {
		rc = EADDRNOTAVAIL;
		goto server_info_done;
	}

	/*
	 * Get the MTU for the network interface
	 */
	mtu = get_mtu(ssp->ss_intf);
	if (mtu < 0) {
		rc = EIO;
		goto server_info_done;
	}

	/*
	 * Get the ATM address associated with the
	 * physical interface
	 */
	UM_ZERO(&air, sizeof(struct atminfreq));
	air.air_opcode = AIOCS_INF_INT;
	strcpy(air.air_int_intf, netif_rsp->anp_phy_intf);
	len = do_info_ioctl(&air, sizeof(struct air_int_rsp));
	if (len <= 0) {
		rc = EIO;
		goto server_info_done;
	}
	intf_rsp = (struct air_int_rsp *)air.air_buf_addr;

	/*
	 * Make sure we're running UNI signalling
	 */
	if (intf_rsp->anp_sig_proto != ATM_SIG_UNI30 &&
			intf_rsp->anp_sig_proto != ATM_SIG_UNI31 &&
			intf_rsp->anp_sig_proto != ATM_SIG_UNI40) {
		rc = EINVAL;
		goto server_info_done;
	}

	/*
	 * Check the physical interface's state
	 */
	if (intf_rsp->anp_sig_state != UNISIG_ACTIVE) {
		rc = EHOSTDOWN;
		goto server_info_done;
	}

	/*
	 * Make sure the interface's address is valid
	 */
	if (intf_rsp->anp_addr.address_format != T_ATM_ENDSYS_ADDR &&
			!(intf_rsp->anp_addr.address_format ==
				T_ATM_E164_ADDR &&
			intf_rsp->anp_subaddr.address_format ==
				T_ATM_ENDSYS_ADDR)) {
		rc = EINVAL;
		goto server_info_done;
	}

	/*
	 * Find the selector byte value for the interface
	 */
	for (i=0; i<strlen(ssp->ss_intf); i++) {
		if (ssp->ss_intf[i] >= '0' &&
				ssp->ss_intf[i] <= '9')
			break;
	}
	sel = atoi(&ssp->ss_intf[i]);

	/*
	 * Get configuration information associated with the
	 * physical interface
	 */
	UM_ZERO(&air, sizeof(struct atminfreq));
	air.air_opcode = AIOCS_INF_CFG;
	strcpy(air.air_int_intf, netif_rsp->anp_phy_intf);
	len = do_info_ioctl(&air, sizeof(struct air_cfg_rsp));
	if (len <= 0) {
		rc = EIO;
		goto server_info_done;
	}
	cfg_rsp = (struct air_cfg_rsp *)air.air_buf_addr;

	/*
	 * Update the server entry
	 */
	UM_COPY(&ip_addr->sin_addr, ssp->ss_lsid.id, ssp->ss_id_len);
	ssp->ss_lsid.id_len = ssp->ss_id_len;
	ssp->ss_mtu = mtu + 8;
	ATM_ADDR_COPY(&intf_rsp->anp_addr, &ssp->ss_addr);
	ATM_ADDR_COPY(&intf_rsp->anp_subaddr, &ssp->ss_subaddr);
	if (ssp->ss_addr.address_format == T_ATM_ENDSYS_ADDR) {
		anp = (Atm_addr_nsap *)ssp->ss_addr.address;
		anp->aan_sel = sel;
	} else if (ssp->ss_addr.address_format == T_ATM_E164_ADDR &&
			ssp->ss_subaddr.address_format ==
				T_ATM_ENDSYS_ADDR) {
		anp = (Atm_addr_nsap *)ssp->ss_subaddr.address;
		anp->aan_sel = sel;
	}
	ssp->ss_media = cfg_rsp->acp_cfg.ac_media;
	rc = 0;

	/*
	 * Free dynamic data
	 */
server_info_done:
	if (netif_rsp)
		UM_FREE(netif_rsp);
	if (intf_rsp)
		UM_FREE(intf_rsp);
	if (cfg_rsp)
		UM_FREE(cfg_rsp);

	return(rc);
}


/*
 * Process a CA message
 *
 * Arguments:
 *	dcsp	pointer to a DCS control block for the neighbor
 *	cap	pointer to the CA part of the received message
 *
 * Returns:
 *	none
 *
 */
void
scsp_process_ca(dcsp, cap)
	Scsp_dcs	*dcsp;
	Scsp_ca		*cap;
{
	Scsp_csa	*csap, *next_csap;
	Scsp_cse	*csep;
	Scsp_server	*ssp = dcsp->sd_server;

	/*
	 * Process CSAS records from the CA message
	 */
	for (csap = cap->ca_csa_rec; csap; csap = next_csap) {
		next_csap = csap->next;
		SCSP_LOOKUP(ssp, &csap->key, csep);
		if (!csep || (scsp_cmp_id(&csap->oid,
					&csep->sc_oid) == 0 &&
				csap->seq > csep->sc_seq)) {
			/*
			 * CSAS entry not in cache or more
			 * up to date than cache, add it to CRL
			 */
			UNLINK(csap, Scsp_csa, cap->ca_csa_rec, next);
			LINK2TAIL(csap, Scsp_csa, dcsp->sd_crl, next);
		}
	}
}


/*
 * Process a Cache Response message from a server
 *
 * Arguments:
 *	ssp	pointer to the server block
 *	smp	pointer to the message
 *
 * Returns:
 *	none
 *
 */
void
scsp_process_cache_rsp(ssp, smp)
	Scsp_server	*ssp;
	Scsp_if_msg	*smp;
{
	int		len;
	Scsp_atmarp_msg	*aap;
	Scsp_cse	*csep;

	/*
	 * Loop through the message, processing each cache entry
	 */
	len = smp->si_len;
	len -= sizeof(Scsp_if_msg_hdr);
	aap = &smp->si_atmarp;
	while (len > 0) {
		switch(smp->si_proto) {
		case SCSP_ATMARP_PROTO:
			/*
			 * If we already have an entry with this key,
			 * delete it
			 */
			SCSP_LOOKUP(ssp, &aap->sa_key, csep);
			if (csep) {
				SCSP_DELETE(ssp, csep);
				UM_FREE(csep);
			}

			/*
			 * Copy the data from the server to a cache
			 * summary entry
			 */
			csep = scsp_atmarp2cse(aap);

			/*
			 * Point past this entry
			 */
			len -= sizeof(Scsp_atmarp_msg);
			aap++;
			break;
		case SCSP_NHRP_PROTO:
		default:
			/*
			 * Not implemented yet
			 */
			return;
		}

		/*
		 * Add the new summary entry to the cache
		 */
		SCSP_ADD(ssp, csep);
	}
}


/*
 * Propagate a CSA to all the DCSs in the server group except
 * the one the CSA was received from
 *
 * Arguments:
 *	dcsp	pointer to a the DCS the CSA came from
 *	csap	pointer to a the CSA
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_propagate_csa(dcsp, csap)
	Scsp_dcs	*dcsp;
	Scsp_csa	*csap;
{
	int		rc, ret_rc = 0;
	Scsp_server	*ssp = dcsp->sd_server;
	Scsp_dcs	*dcsp1;
	Scsp_csa	*csap1;

	/*
	 * Check the hop count in the CSA
	 */
	if (csap->hops <= 1)
		return(0);

	/*
	 * Pass the cache entry on to the server's other DCSs
	 */
	for (dcsp1 = ssp->ss_dcs; dcsp1; dcsp1 = dcsp1->sd_next) {
		/*
		 * Skip this DCS if it's the one we got
		 * the entry from
		 */
		if (dcsp1 == dcsp)
			continue;

		/*
		 * Copy the  CSA
		 */
		csap1 = scsp_dup_csa(csap);

		/*
		 * Decrement the hop count
		 */
		csap1->hops--;

		/*
		 * Send the copy of the CSA to the CA FSM for the DCS
		 */
		rc = scsp_cafsm(dcsp1, SCSP_CAFSM_CACHE_UPD,
				(void *) csap1);
		if (rc)
			ret_rc = rc;
	}

	return(ret_rc);
}


/*
 * Update SCSP's cache given a CSA or CSAS
 *
 * Arguments:
 *	dcsp	pointer to a DCS 
 *	csap	pointer to a CSA
 *
 * Returns:
 *	none
 *
 */
void
scsp_update_cache(dcsp, csap)
	Scsp_dcs	*dcsp;
	Scsp_csa	*csap;
{
	Scsp_cse	*csep;

	/*
	 * Check whether we already have this in the cache
	 */
	SCSP_LOOKUP(dcsp->sd_server, &csap->key, csep);

	/*
	 * If we don't already have it and it's not being deleted,
	 * build a new cache summary entry
	 */
	if (!csep && !csap->null) {
		/*
		 * Get memory for a new entry
		 */
		csep = (Scsp_cse *)UM_ALLOC(sizeof(Scsp_cse));
		if (!csep) {
			scsp_mem_err("scsp_update_cache: sizeof(Scsp_cse)");
		}
		UM_ZERO(csep, sizeof(Scsp_cse));

		/*
		 * Fill out the new cache summary entry
		 */
		csep->sc_seq = csap->seq;
		csep->sc_key = csap->key;
		csep->sc_oid = csap->oid;

		/*
		 * Add the new entry to the cache
		 */
		SCSP_ADD(dcsp->sd_server, csep);
	} 

	/*
	 * Update or delete the entry
	 */
	if (csap->null) {
		/*
		 * The null flag is set--delete the entry
		 */
		if (csep) {
			SCSP_DELETE(dcsp->sd_server, csep);
			UM_FREE(csep);
		}
	} else {
		/*
		 * Update the existing entry
		 */
		csep->sc_seq = csap->seq;
		csep->sc_oid = csap->oid;
	}
}


/*
 * Reconfigure SCSP
 *
 * Called as the result of a SIGHUP interrupt.  Reread the
 * configuration file and solicit the cache from the server.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
void
scsp_reconfigure()
{
	int		rc;
	Scsp_server	*ssp;

	/*
	 * Log a message saying we're reconfiguring
	 */
	scsp_log(LOG_ERR, "Reconfiguring ...");

	/*
	 * Re-read the configuration file
	 */
	rc = scsp_config(scsp_config_file);
	if (rc) {
		scsp_log(LOG_ERR, "Found %d error%s in configuration file",
				rc, ((rc == 1) ? "" : "s"));
		exit(1);
	}

	/*
	 * If a connection to a server is open, get the cache from
	 * the server
	 */
	for (ssp = scsp_server_head; ssp; ssp = ssp->ss_next) {
		if (ssp->ss_sock != -1) {
			rc = scsp_send_cache_ind(ssp);
		}
	}
}
