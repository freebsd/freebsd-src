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
 *	@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_socket.c,v 1.3 1999/08/28 01:15:34 peter Exp $
 *
 */


/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * SCSP socket management routines
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
#include <fcntl.h>
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
__RCSID("@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_socket.c,v 1.3 1999/08/28 01:15:34 peter Exp $");
#endif


/*
 * Local variables
 */
static struct t_atm_llc llc_scsp = {
	T_ATM_LLC_SHARING,
	8,
	{0xaa, 0xaa, 0x03, 0x00, 0x00, 0x5e, 0x00, 0x05}
};

static struct t_atm_aal5	aal5 = {
	0,			/* forward_max_SDU_size */
	0,			/* backward_max_SDU_size */
	0			/* SSCS_type */
};

static struct t_atm_traffic	traffic = {
	{			/* forward */
		T_ATM_ABSENT,	/* PCR_high_priority */
		0,		/* PCR_all_traffic */
		T_ATM_ABSENT,	/* SCR_high_priority */
		T_ATM_ABSENT,	/* SCR_all_traffic */
		T_ATM_ABSENT,	/* MBS_high_priority */
		T_ATM_ABSENT,	/* MBS_all_traffic */
		T_NO		/* tagging */
	},
	{			/* backward */
		T_ATM_ABSENT,	/* PCR_high_priority */
		0,		/* PCR_all_traffic */
		T_ATM_ABSENT,	/* SCR_high_priority */
		T_ATM_ABSENT,	/* SCR_all_traffic */
		T_ATM_ABSENT,	/* MBS_high_priority */
		T_ATM_ABSENT,	/* MBS_all_traffic */
		T_NO		/* tagging */
	},
        T_YES			/* best_effort */
};

static struct t_atm_bearer	bearer = {
	T_ATM_CLASS_X,		/* bearer_class */
	T_ATM_NULL,		/* traffic_type */
	T_ATM_NULL,		/* timing_requirements */
	T_NO,			/* clipping_susceptibility */
	T_ATM_1_TO_1		/* connection_configuration */
};

static struct t_atm_qos	qos = {
	T_ATM_NETWORK_CODING,		/* coding_standard */
	{				/* forward */
		T_ATM_QOS_CLASS_0	/* qos_class */
	},
	{				/* backward */
		T_ATM_QOS_CLASS_0	/* qos_class */
	}
};

static struct t_atm_app_name	appname = {
	"SCSP"
};


/*
 * Find a DCS, given its socket
 *
 * Arguments:
 *	sd	socket descriptor
 *
 * Returns:
 *	0	not found
 *	address of DCS block corresponding to socket
 *
 */
Scsp_dcs *
scsp_find_dcs(sd)
	int	sd;
{
	Scsp_server	*ssp;
	Scsp_dcs	*dcsp = NULL;

	/*
	 * Loop through the list of servers
	 */
	for (ssp = scsp_server_head; ssp; ssp = ssp->ss_next) {
		/*
		 * Check all the DCSs chained from each server
		 */
		for (dcsp = ssp->ss_dcs; dcsp; dcsp = dcsp->sd_next) {
			if (dcsp->sd_sock == sd)
				break;
		}
	}

	return(dcsp);
}


/*
 * Find a server, given its socket
 *
 * Arguments:
 *	sd	socket descriptor
 *
 * Returns:
 *	0	not found
 *	address of server block corresponding to socket
 *
 */
Scsp_server *
scsp_find_server(sd)
	int	sd;
{
	Scsp_server	*ssp;

	/*
	 * Loop through the list of servers
	 */
	for (ssp = scsp_server_head; ssp; ssp = ssp->ss_next) {
		if (ssp->ss_sock == sd)
			break;
	}

	return(ssp);
}


/*
 * Connect to a directly connected server
 *
 * Arguments:
 *	dcsp	pointer to DCS block for server
 *
 * Returns:
 *	0	success (dcsp->sd_sock is set)
 *	else	errno indicating reason for failure
 *
 */
int
scsp_dcs_connect(dcsp)
	Scsp_dcs	*dcsp;

{
	int			rc, sd;
	struct sockaddr_atm	DCS_addr;

	/*
	 * If the DCS already has an open connection, just return
	 */
	if (dcsp->sd_sock != -1) {
		return(0);
	}

	/*
	 * Open an ATM socket
	 */
	sd = socket(PF_ATM, SOCK_SEQPACKET, ATM_PROTO_AAL5);
	if (sd == -1) {
		return(ESOCKTNOSUPPORT);
	}
	if (sd > scsp_max_socket) {
		scsp_max_socket = sd;
	}

	/*
	 * Set up connection parameters for SCSP connection
	 */
	UM_ZERO(&DCS_addr, sizeof(DCS_addr));
#if (defined(BSD) && (BSD >= 199103))
	DCS_addr.satm_len = sizeof(DCS_addr);
#endif
	DCS_addr.satm_family = AF_ATM;
	DCS_addr.satm_addr.t_atm_sap_addr.SVE_tag_addr =
			T_ATM_PRESENT;
	DCS_addr.satm_addr.t_atm_sap_addr.SVE_tag_selector =
			T_ATM_PRESENT;
	DCS_addr.satm_addr.t_atm_sap_addr.address_format =
			dcsp->sd_addr.address_format;
	DCS_addr.satm_addr.t_atm_sap_addr.address_length =
			dcsp->sd_addr.address_length;
	UM_COPY(dcsp->sd_addr.address,
			DCS_addr.satm_addr.t_atm_sap_addr.address,
			dcsp->sd_addr.address_length);

	DCS_addr.satm_addr.t_atm_sap_layer2.SVE_tag =
			T_ATM_PRESENT;
	DCS_addr.satm_addr.t_atm_sap_layer2.ID_type =
			T_ATM_SIMPLE_ID;
	DCS_addr.satm_addr.t_atm_sap_layer2.ID.simple_ID =
			T_ATM_BLLI2_I8802;

        DCS_addr.satm_addr.t_atm_sap_layer3.SVE_tag =
			T_ATM_ABSENT;
        DCS_addr.satm_addr.t_atm_sap_appl.SVE_tag =
			T_ATM_ABSENT;

	/*
	 * Bind the socket to our address
	 */
	if (bind(sd, (struct sockaddr *)&DCS_addr,
			sizeof(DCS_addr))) {
		rc = errno;
		goto connect_fail;
	}

	/*
	 * Set non-blocking operation
	 */
#ifdef sun
	rc = fcntl(sd, F_SETFL, FNBIO + FNDELAY);
#else
	rc = fcntl(sd, F_SETFL, O_NONBLOCK);
#endif
	if (rc == -1) {
		scsp_log(LOG_ERR, "scsp_dcs_connect: fcntl failed");
		rc = errno;
		goto connect_fail;
	}

	/*
	 * Set AAL 5 options
	 */
	aal5.forward_max_SDU_size = dcsp->sd_server->ss_mtu;
	aal5.backward_max_SDU_size = dcsp->sd_server->ss_mtu;
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_AAL5, (caddr_t)&aal5,
			sizeof(aal5)) < 0) {
		rc = EOPNOTSUPP;
		goto connect_fail;
	}

	/*
	 * Set traffic options
	 */
	switch(dcsp->sd_server->ss_media) {
	case MEDIA_TAXI_100:
		traffic.forward.PCR_all_traffic = ATM_PCR_TAXI100;
		traffic.backward.PCR_all_traffic = ATM_PCR_TAXI100;
		break;
	case MEDIA_TAXI_140:
		traffic.forward.PCR_all_traffic = ATM_PCR_TAXI140;
		traffic.backward.PCR_all_traffic = ATM_PCR_TAXI140;
		break;
	case MEDIA_OC3C:
	case MEDIA_UTP155:
		traffic.forward.PCR_all_traffic = ATM_PCR_OC3C;
		traffic.backward.PCR_all_traffic = ATM_PCR_OC3C;
		break;
	case MEDIA_OC12C:
		traffic.forward.PCR_all_traffic = ATM_PCR_OC12C;
		traffic.backward.PCR_all_traffic = ATM_PCR_OC12C;
		break;
	case MEDIA_UNKNOWN:
		break;
	}

	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_TRAFFIC,
			(caddr_t)&traffic, sizeof(traffic)) < 0) {
		rc = EOPNOTSUPP;
		goto connect_fail;
	}

	/*
	 * Set bearer capability options
	 */
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_BEARER_CAP,
			(caddr_t)&bearer, sizeof(bearer)) < 0) {
		rc = EOPNOTSUPP;
		goto connect_fail;
	}

	/*
	 * Set QOS options
	 */
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_QOS,
			(caddr_t)&qos, sizeof(qos)) < 0) {
		rc = EOPNOTSUPP;
		goto connect_fail;
	}

	/*
	 * Set LLC identifier
	 */
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_LLC,
			(caddr_t)&llc_scsp, sizeof(llc_scsp)) < 0) {
		rc = EOPNOTSUPP;
		goto connect_fail;
	}

	/*
	 * Set application name
	 */
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_APP_NAME,
			(caddr_t)&appname, sizeof(appname)) < 0) {
		rc = EOPNOTSUPP;
		goto connect_fail;
	}

	/*
	 * Connect to DCS
	 */
	if (connect(sd, (struct sockaddr *)&DCS_addr,
			sizeof(DCS_addr)) < 0 &&
			errno != EINPROGRESS) {
		rc = errno;
		goto connect_fail;
	}

	/*
	 * Set return values
	 */
	dcsp->sd_sock = sd;
	return(0);

connect_fail:
	/*
	 * Close the socket if something didn't work
	 */
	(void)close(sd);
	dcsp->sd_sock = -1;
	if (rc == 0)
		rc = EFAULT;
	return(rc);
}


/*
 * Listen for ATM connections from DCSs
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	sock	socket which is listening (also set in
		ssp->ss_dcs_lsock)
 *	-1	error encountered (reason in errno)
 *
 */
int
scsp_dcs_listen(ssp)
	Scsp_server	*ssp;
{
	int			rc, sd;
	struct sockaddr_atm	ls_addr;

	/*
	 * Open a socket
	 */
	sd = socket(PF_ATM, SOCK_SEQPACKET, ATM_PROTO_AAL5);
	if (sd == -1) {
		rc = errno;
		goto listen_fail;
	}
	if (sd > scsp_max_socket) {
		scsp_max_socket = sd;
	}

	/*
	 * Set up our address
	 */
	UM_ZERO(&ls_addr, sizeof(ls_addr));
#if (defined(BSD) && (BSD >= 199103))
	ls_addr.satm_len = sizeof(ls_addr);
#endif
	ls_addr.satm_family = AF_ATM;
	ls_addr.satm_addr.t_atm_sap_addr.SVE_tag_addr = T_ATM_PRESENT;
	ls_addr.satm_addr.t_atm_sap_addr.SVE_tag_selector =
			T_ATM_PRESENT;
	ls_addr.satm_addr.t_atm_sap_addr.address_format =
			ssp->ss_addr.address_format;
	ls_addr.satm_addr.t_atm_sap_addr.address_length =
			ssp->ss_addr.address_length;
	UM_COPY(ssp->ss_addr.address,
			ls_addr.satm_addr.t_atm_sap_addr.address,
			ssp->ss_addr.address_length);

	ls_addr.satm_addr.t_atm_sap_layer2.SVE_tag = T_ATM_PRESENT;
	ls_addr.satm_addr.t_atm_sap_layer2.ID_type = T_ATM_SIMPLE_ID;
	ls_addr.satm_addr.t_atm_sap_layer2.ID.simple_ID =
			T_ATM_BLLI2_I8802;

	ls_addr.satm_addr.t_atm_sap_layer3.SVE_tag = T_ATM_ABSENT;
	ls_addr.satm_addr.t_atm_sap_appl.SVE_tag = T_ATM_ABSENT;

	/*
	 * Bind the socket to our address
	 */
	rc = bind(sd, (struct sockaddr *)&ls_addr, sizeof(ls_addr));
	if (rc == -1) {
		rc = errno;
		goto listen_fail;
	}

	/*
	 * Set non-blocking I/O
	 */
#ifdef sun
	rc = fcntl(sd, F_SETFL, FNBIO + FNDELAY);
#else
	rc = fcntl(sd, F_SETFL, O_NONBLOCK);
#endif
	if (rc == -1) {
		scsp_log(LOG_ERR, "scsp_dcs_listen: fcntl failed");
		rc = errno;
		goto listen_fail;
	}

	/*
	 * Set AAL 5 options
	 */
	aal5.forward_max_SDU_size = ssp->ss_mtu;
	aal5.backward_max_SDU_size = ssp->ss_mtu;
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_AAL5, (caddr_t)&aal5,
			sizeof(aal5)) < 0) {
		rc = EOPNOTSUPP;
		goto listen_fail;
	}

	/*
	 * Set traffic options
	 */
	switch(ssp->ss_media) {
	case MEDIA_TAXI_100:
		traffic.forward.PCR_all_traffic = ATM_PCR_TAXI100;
		traffic.backward.PCR_all_traffic = ATM_PCR_TAXI100;
		break;
	case MEDIA_TAXI_140:
		traffic.forward.PCR_all_traffic = ATM_PCR_TAXI140;
		traffic.backward.PCR_all_traffic = ATM_PCR_TAXI140;
		break;
	case MEDIA_OC3C:
	case MEDIA_UTP155:
		traffic.forward.PCR_all_traffic = ATM_PCR_OC3C;
		traffic.backward.PCR_all_traffic = ATM_PCR_OC3C;
		break;
	case MEDIA_OC12C:
		traffic.forward.PCR_all_traffic = ATM_PCR_OC12C;
		traffic.backward.PCR_all_traffic = ATM_PCR_OC12C;
		break;
	case MEDIA_UNKNOWN:
		break;
	}

	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_TRAFFIC,
			(caddr_t)&traffic, sizeof(traffic)) < 0) {
		rc = EOPNOTSUPP;
		goto listen_fail;
	}

	/*
	 * Set bearer capability options
	 */
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_BEARER_CAP,
			(caddr_t)&bearer, sizeof(bearer)) < 0) {
		rc = EOPNOTSUPP;
		goto listen_fail;
	}

	/*
	 * Set QOS options
	 */
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_QOS,
			(caddr_t)&qos, sizeof(qos)) < 0) {
		rc = EOPNOTSUPP;
		goto listen_fail;
	}

	/*
	 * Set LLC identifier
	 */
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_LLC,
			(caddr_t)&llc_scsp, sizeof(llc_scsp)) < 0) {
		rc = EOPNOTSUPP;
		goto listen_fail;
	}

	/*
	 * Set application name
	 */
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_APP_NAME,
			(caddr_t)&appname, sizeof(appname)) < 0) {
		rc = EOPNOTSUPP;
		goto listen_fail;
	}

	/*
	 * Listen for new connections
	 */
	if (listen(sd, 5) < 0) {
		rc = errno;
		goto listen_fail;
	}

	ssp->ss_dcs_lsock = sd;
	return(sd);

listen_fail:
	/*
	 * Close the socket if anything didn't work
	 */
	(void)close(sd);
	if (rc == 0)
		errno = EFAULT;
	else
		errno = rc;
	ssp->ss_dcs_lsock = -1;
	return(-1);
}


/*
 * Accept a connection from a DCS
 *
 * Arguments:
 *	ssp	pointer to server block
 *
 * Returns:
 *	address of DCS with new connection
 *	0	failure (errno has reason)
 *
 */
Scsp_dcs *
scsp_dcs_accept(ssp)
	Scsp_server	*ssp;
{
	int			len, rc, sd;
	struct sockaddr_atm	dcs_sockaddr;
	struct t_atm_sap_addr	*dcs_addr = &dcs_sockaddr.satm_addr.t_atm_sap_addr;
	Atm_addr		dcs_atmaddr;
	Scsp_dcs		*dcsp;

	/*
	 * Accept the new connection
	 */
	len = sizeof(dcs_sockaddr);
	sd = accept(ssp->ss_dcs_lsock,
			(struct sockaddr *)&dcs_sockaddr, &len);
	if (sd < 0) {
		return((Scsp_dcs *)0);
	}
	if (sd > scsp_max_socket) {
		scsp_max_socket = sd;
	}

	/*
	 * Copy the DCS's address from the sockaddr to an Atm_addr
	 */
	if (dcs_addr->SVE_tag_addr != T_ATM_PRESENT) {
		dcs_atmaddr.address_format = T_ATM_ABSENT;
		dcs_atmaddr.address_length = 0;
	} else {
		dcs_atmaddr.address_format = dcs_addr->address_format;
		dcs_atmaddr.address_length = dcs_addr->address_length;
		UM_COPY(dcs_addr->address, dcs_atmaddr.address,
				dcs_addr->address_length);
	}

	/*
	 * Find out which DCS this connection is for
	 */
	for (dcsp = ssp->ss_dcs; dcsp; dcsp = dcsp->sd_next) {
		/*
		 * Compare DCS's address to address
		 * configured by user
		 */
		if (ATM_ADDR_EQUAL(&dcsp->sd_addr,
				&dcs_atmaddr))
			break;
	}

	/*
	 * Make sure we have this DCS configured
	 */
	if (!dcsp) {
		errno = EINVAL;
		goto dcs_accept_fail;
	}

	/*
	 * Make sure we are in a state to accept the connection
	 */
	if (ssp->ss_state != SCSP_SS_ACTIVE) {
		errno = EACCES;
		goto dcs_accept_fail;
	}

	/*
	 * Make sure we don't already have a connection to this DCS
	 */
	if (dcsp->sd_sock != -1) {
		errno = EALREADY;
		goto dcs_accept_fail;
	}

	/*
	 * Set application name
	 */
	if (setsockopt(sd, T_ATM_SIGNALING, T_ATM_APP_NAME,
			(caddr_t)&appname, sizeof(appname)) < 0) {
		rc = EOPNOTSUPP;
		goto dcs_accept_fail;
	}

	/*
	 * Set non-blocking I/O
	 */
#ifdef sun
		rc = fcntl(sd, F_SETFL, FNBIO + FNDELAY);
#else
		rc = fcntl(sd, F_SETFL, O_NONBLOCK);
#endif
	if (rc == -1) {
		goto dcs_accept_fail;
	}

	/*
	 * Cancel the open retry timer
	 */
	HARP_CANCEL(&dcsp->sd_open_t);

	/*
	 * Save the socket address and return the
	 * address of the DCS
	 */
	dcsp->sd_sock = sd;
	return(dcsp);

dcs_accept_fail:
	/*
	 * An error has occured--clean up and return
	 */
	(void)close(sd);
	return((Scsp_dcs *)0);
}


/*
 * Read an SCSP message from a directly connected server
 *
 * Arguments:
 *	dcsp	pointer to DCS block that has data
 *
 * Returns:
 *	0	success
 *	else	errno indicating reason for failure
 *
 */
int
scsp_dcs_read(dcsp)
	Scsp_dcs	*dcsp;

{
	int			len, rc;
	char			*buff = (char *)0;
	Scsp_server		*ssp = dcsp->sd_server;
	Scsp_msg		*msg;

	/*
	 * Get a buffer to hold the entire message
	 */
	len = ssp->ss_mtu;
	buff = (char *)UM_ALLOC(len);
	if (!buff) {
		scsp_mem_err("scsp_dcs_read: ssp->ss_mtu");
	}

	/*
	 * Read the message
	 */
	len = read(dcsp->sd_sock, buff, len);
	if (len < 0) {
		goto dcs_read_fail;
	}

	/*
	 * Parse the input message and pass it to the Hello FSM
	 */
	msg = scsp_parse_msg(buff, len);
	if (msg) {
		/*
		 * Write the message to the trace file if
		 * it's of a type we're tracing
		 */
		if (((scsp_trace_mode & SCSP_TRACE_HELLO_MSG) &&
				msg->sc_msg_type == SCSP_HELLO_MSG) ||
				((scsp_trace_mode & SCSP_TRACE_CA_MSG) &&
				msg->sc_msg_type != SCSP_HELLO_MSG)) {
			scsp_trace_msg(dcsp, msg, 1);
			scsp_trace("\n");
		}

		/*
		 * Pass the message to the Hello FSM
		 */
		rc = scsp_hfsm(dcsp, SCSP_HFSM_RCVD, msg);
		scsp_free_msg(msg);
	} else {
		/*
		 * Message was invalid.  Write it to the trace file
		 * if we're tracing messages.
		 */
		if (scsp_trace_mode & (SCSP_TRACE_HELLO_MSG &
				SCSP_TRACE_CA_MSG)) {
			int	i;
			scsp_trace("Invalid message received:\n");
			scsp_trace("0x");
			for (i = 0; i < len; i++) {
				scsp_trace("%02x ", (u_char)buff[i]);
			}
			scsp_trace("\n");
		}
	}
	UM_FREE(buff);

	return(0);

dcs_read_fail:
	/*
	 * Error on read--check for special conditions
	 */
	rc = errno;
	if (errno == ECONNRESET) {
		/*
		 * VCC has been closed--pass the event to
		 * the Hello FSM
		 */
		rc = scsp_hfsm(dcsp, SCSP_HFSM_VC_CLOSED,
				(Scsp_msg *)0);
	}
	if (errno == ECONNREFUSED) {
		/*
		 * VCC open failed--set a timer and try
		 * again when it fires
		 */
		HARP_TIMER(&dcsp->sd_open_t,
				SCSP_Open_Interval,
				scsp_open_timeout);
		rc = 0;
	}

	if (buff)
		UM_FREE(buff);
	return(rc);
}


/*
 * Listen for Unix connections from SCSP client servers
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	sock	socket which is listening
 *	-1	error (reason in errno)
 *
 */
int
scsp_server_listen()
{
	int	rc, sd;

	static struct sockaddr	scsp_addr = {
#if (defined(BSD) && (BSD >= 199103))
		sizeof(struct sockaddr),	/* sa_len */
#endif
		AF_UNIX,			/* sa_family */
		SCSPD_SOCK_NAME			/* sa_data */
	};

	/*
	 * Unlink any old socket
	 */
	rc = unlink(SCSPD_SOCK_NAME);
	if (rc < 0 && errno != ENOENT)
		return(-1);

	/*
	 * Open a socket
	 */
	sd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sd == -1) {
		return(-1);
	}
	if (sd > scsp_max_socket) {
		scsp_max_socket = sd;
	}

	/*
	 * Bind the socket's address
	 */
	rc = bind(sd, &scsp_addr, sizeof(scsp_addr));
	if (rc == -1) {
		(void)close(sd);
		return(-1);
	}

	/*
	 * Set non-blocking I/O
	 */
#ifdef sun
		rc = fcntl(sd, F_SETFL, FNBIO + FNDELAY);
#else
		rc = fcntl(sd, F_SETFL, O_NONBLOCK);
#endif
	if (rc == -1) {
		(void)close(sd);
		return(-1);
	}

	/*
	 * Listen for new connections
	 */
	if (listen(sd, 5) < 0) {
		(void)close(sd);
		return(-1);
	}

	return(sd);
}


/*
 * Accept a connection from a server
 *
 * We accept a connection, but we won't know which server it is
 * from until we get the configuration data from the server.  We
 * put the connection on a 'pending' queue and will assign it to
 * a server when the config data arrives.
 *
 * Arguments:
 *	ls	listening socket to accept from
 *
 * Returns:
 *	0	success
 *	errno	reason for failure
 *
 */
int
scsp_server_accept(ls)
	int	ls;

{
	int		len, rc, sd;
	struct sockaddr	server_addr;
	Scsp_pending	*psp;

	/*
	 * Accept the new connection
	 */
	len = sizeof(server_addr);
	sd = accept(ls, (struct sockaddr *)&server_addr, &len);
	if (sd < 0) {
		return(errno);
	}
	if (sd > scsp_max_socket) {
		scsp_max_socket = sd;
	}

	/*
	 * Set non-blocking operation
	 */
#ifdef sun
	rc = fcntl(sd, F_SETFL, FNBIO + FNDELAY);
#else
	rc = fcntl(sd, F_SETFL, O_NONBLOCK);
#endif
	if (rc == -1) {
		(void)close(sd);
		rc = errno;
	}

	/*
	 * Put the new socket on the 'pending' queue
	 */
	psp = (Scsp_pending *) UM_ALLOC(sizeof(Scsp_pending));
	if (!psp) {
		scsp_mem_err("scsp_server_accept: sizeof(Scsp_pending)");
	}
	psp->sp_sock = sd;
	LINK2TAIL(psp, Scsp_pending, scsp_pending_head, sp_next);

	return(0);
}


/*
 * Read a server interface message from a socket
 *
 * Arguments:
 *	sd	socket to read from
 *
 * Returns:
 *	msg	pointer to message read
 *	0	failure (errno has reason)
 *
 */
Scsp_if_msg *
scsp_if_sock_read(sd)
	int	sd;

{
	int		len;
	char		*buff = (char *)0;
	Scsp_if_msg	*msg;
	Scsp_if_msg_hdr	msg_hdr;

	/*
	 * Read the message header from the socket
	 */
	len = read(sd, (char *)&msg_hdr, sizeof(msg_hdr));
	if (len != sizeof(msg_hdr)) {
		if (len >= 0)
			errno = EINVAL;
		goto socket_read_fail;
	}

	/*
	 * Get a buffer and read the rest of the message into it
	 */
	buff = (char *)UM_ALLOC(msg_hdr.sh_len);
	if (!buff) {
		scsp_mem_err("scsp_if_sock_read: msg_hdr.sh_len");
	}
	msg = (Scsp_if_msg *)buff;
	msg->si_hdr = msg_hdr;
	len = read(sd, &buff[sizeof(Scsp_if_msg_hdr)],
			msg->si_len - sizeof(Scsp_if_msg_hdr));
	if (len != msg->si_len - sizeof(Scsp_if_msg_hdr)) {
		if (len >= 0) {
			errno = EINVAL;
		}
		goto socket_read_fail;
	}

	/*
	 * Trace the message
	 */
	if (scsp_trace_mode & SCSP_TRACE_IF_MSG) {
		scsp_trace("Received server I/F message:\n");
		print_scsp_if_msg(scsp_trace_file, msg);
		scsp_trace("\n");
	}

	return(msg);

socket_read_fail:
	if (buff)
		UM_FREE(buff);
	return((Scsp_if_msg *)0);
}


/*
 * Write a server interface message to a socket
 *
 * Arguments:
 *	sd	socket to write to
 *	msg	pointer to message to write
 *
 * Returns:
 *	0	success
 *	errno	reason for failure
 *
 */
int
scsp_if_sock_write(sd, msg)
	int		sd;
	Scsp_if_msg	*msg;
{
	int	len, rc;

	/*
	 * Trace the message
	 */
	if (scsp_trace_mode & SCSP_TRACE_IF_MSG) {
		scsp_trace("Writing server I/F message:\n");
		print_scsp_if_msg(scsp_trace_file, msg);
		scsp_trace("\n");
	}

	/*
	 * Write the message to the indicated socket
	 */
	len = write(sd, (char *)msg, msg->si_len);
	if (len != msg->si_len) {
		if (len < 0)
			rc = errno;
		else
			rc = EINVAL;
	} else {
		rc = 0;
	}

	return(rc);
}


/*
 * Read data from a local server
 *
 * Arguments:
 *	ssp	pointer to server block that has data
 *
 * Returns:
 *	0	success
 *	else	errno indicating reason for failure
 *
 */
int
scsp_server_read(ssp)
	Scsp_server	*ssp;
{
	int		rc;
	Scsp_dcs	*dcsp;
	Scsp_if_msg	*msg;

	/*
	 * Read the message
	 */
	msg = scsp_if_sock_read(ssp->ss_sock);
	if (!msg) {
		if (errno == EWOULDBLOCK) {
			/*
			 * Nothing to read--just return
			 */
			return(0);
		} else {
			/*
			 * Error--shut down the server entry
			 */
			scsp_server_shutdown(ssp);
		}
		return(errno);
	}

	/*
	 * Process the received message
	 */
	switch(msg->si_type) {
	case SCSP_NOP_REQ:
		/*
		 * Ignore a NOP
		 */
		break;
	case SCSP_CACHE_RSP:
		/*
		 * Summarize the server's cache and try to open
		 * connections to all of its DCSs
		 */
		scsp_process_cache_rsp(ssp, msg);
		ssp->ss_state = SCSP_SS_ACTIVE;
		for (dcsp = ssp->ss_dcs; dcsp; dcsp = dcsp->sd_next) {
			if (scsp_dcs_connect(dcsp)) {
				/*
				 * Connect failed -- the DCS may not
				 * be up yet, so we'll try again later
				 */
				HARP_TIMER(&dcsp->sd_open_t,
						SCSP_Open_Interval,
						scsp_open_timeout);
			}
		}
		ssp->ss_state = SCSP_SS_ACTIVE;
		break;
	case SCSP_SOLICIT_RSP:
		/*
		 * The server has answered our request for a particular
		 * entry from its cache
		 */
		dcsp = (Scsp_dcs *)msg->si_tok;
		rc = scsp_cfsm(dcsp, SCSP_CIFSM_SOL_RSP, (Scsp_msg *)0,
				msg);
		break;
	case SCSP_UPDATE_REQ:
		/*
		 * Pass the update request to the FSMs for all
		 * DCSs associated with the server
		 */
		if (ssp->ss_state == SCSP_SS_ACTIVE) {
			for (dcsp = ssp->ss_dcs; dcsp;
					dcsp = dcsp->sd_next) {
				rc = scsp_cfsm(dcsp, SCSP_CIFSM_UPD_REQ,
						(Scsp_msg *)0, msg);
			}
		}
		break;
	case SCSP_UPDATE_RSP:
		/*
		 * Pass the update response to the FSM for the
		 * DCS associated with the request
		 */
		dcsp = (Scsp_dcs *)msg->si_tok;
		rc = scsp_cfsm(dcsp, SCSP_CIFSM_UPD_RSP,
				(Scsp_msg *)0, msg);
		break;
	default:
		scsp_log(LOG_ERR, "invalid message type %d from server",
				msg->si_type);
		return(EINVAL);
	}

	UM_FREE(msg);
	return(0);
}


/*
 * Send a Cache Indication to a server
 *
 * Arguments:
 *	ssp	pointer to server block block
 *
 * Returns:
 *	0	success
 *	else	errno indicating reason for failure
 *
 */
int
scsp_send_cache_ind(ssp)
	Scsp_server	*ssp;
{
	int		rc;
	Scsp_if_msg	*msg;

	/*
	 * Get storage for a server interface message
	 */
	msg = (Scsp_if_msg *)UM_ALLOC(sizeof(Scsp_if_msg));
	if (!msg) {
		scsp_mem_err("scsp_send_cache_ind: sizeof(Scsp_if_msg)");
	}
	UM_ZERO(msg, sizeof(Scsp_if_msg));

	/*
	 * Fill out the message
	 */
	msg->si_type = SCSP_CACHE_IND;
	msg->si_rc = 0;
	msg->si_proto = ssp->ss_pid;
	msg->si_len = sizeof(Scsp_if_msg_hdr);
	msg->si_tok = (u_long)ssp;

	/*
	 * Send the message
	 */
	rc = scsp_if_sock_write(ssp->ss_sock, msg);
	UM_FREE(msg);
	return(rc);
}


/*
 * Read data from a pending server connection
 *
 * Arguments:
 *	psp	pointer to pending block that has data
 *
 * Returns:
 *	0	success
 *	else	errno indicating reason for failure
 *
 */
int
scsp_pending_read(psp)
	Scsp_pending	*psp;

{
	int		rc;
	Scsp_server	*ssp;
	Scsp_if_msg	*msg;

	/*
	 * Read the message from the pending socket
	 */
	msg = scsp_if_sock_read(psp->sp_sock);
	if (!msg) {
		rc = errno;
		goto pending_read_fail;
	}

	/*
	 * Make sure this is configuration data
	 */
	if (msg->si_type != SCSP_CFG_REQ) {
		scsp_log(LOG_ERR, "invalid message type %d from pending server",
				msg->si_type);
		rc = EINVAL;
		goto pending_read_fail;
	}

	/*
	 * Find the server this message is for
	 */
	for (ssp = scsp_server_head; ssp; ssp = ssp->ss_next) {
		if (strcmp(ssp->ss_intf, msg->si_cfg.atmarp_netif) == 0)
			break;
	}
	if (!ssp) {
		scsp_log(LOG_ERR, "refused connection from server for %s",
				msg->si_cfg.atmarp_netif);
		rc = EINVAL;
		goto config_reject;
	}

	/*
	 * Make sure the server is ready to go
	 */
	rc = scsp_get_server_info(ssp);
	if (rc) {
		goto config_reject;
	}

	/*
	 * Save the socket
	 */
	ssp->ss_sock = psp->sp_sock;
	ssp->ss_state = SCSP_SS_CFG;
	UNLINK(psp, Scsp_pending, scsp_pending_head, sp_next);
	UM_FREE(psp);

	/*
	 * Listen for connections from the server's DCSs
	 */
	rc = scsp_dcs_listen(ssp);
	if (rc < 0) {
		rc = errno;
		goto config_reject;
	}

	/*
	 * Respond to the configuration message
	 */
	msg->si_type = SCSP_CFG_RSP;
	msg->si_rc = SCSP_RSP_OK;
	msg->si_len = sizeof(Scsp_if_msg_hdr);
	rc = scsp_if_sock_write(ssp->ss_sock, msg);
	if (rc) {
		goto config_error;;
	}

	/*
	 * Ask the server to send us its cache
	 */
	rc = scsp_send_cache_ind(ssp);
	if (rc) {
		goto config_error;
	}

	UM_FREE(msg);
	return(0);

config_reject:
	/*
	 * Respond to the configuration message
	 */
	msg->si_type = SCSP_CFG_RSP;
	msg->si_rc = SCSP_RSP_REJ;
	msg->si_len = sizeof(Scsp_if_msg_hdr);
	(void)scsp_if_sock_write(ssp->ss_sock, msg);

config_error:
	if (ssp->ss_sock != -1) {
		(void)close(ssp->ss_sock);
		ssp->ss_sock = -1;
	}
	if (ssp->ss_dcs_lsock != -1) {
		(void)close(ssp->ss_dcs_lsock);
		ssp->ss_sock = -1;
	}
	ssp->ss_state = SCSP_SS_NULL;
	UM_FREE(msg);

	return(rc);

pending_read_fail:
	/*
	 * Close the socket and free the pending read block
	 */
	(void)close(psp->sp_sock);
	UNLINK(psp, Scsp_pending, scsp_pending_head, sp_next);
	UM_FREE(psp);
	if (msg)
		UM_FREE(msg);
	return(rc);
}
