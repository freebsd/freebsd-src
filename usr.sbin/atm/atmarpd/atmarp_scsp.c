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
 *	@(#) $FreeBSD: src/usr.sbin/atm/atmarpd/atmarp_scsp.c,v 1.3 1999/08/28 01:15:30 peter Exp $
 *
 */

/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * SCSP-ATMARP server interface: SCSP/ATMARP interface code
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
#include <netatm/uni/uniip_var.h>
 
#include <errno.h>
#include <fcntl.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "../scspd/scsp_msg.h"
#include "../scspd/scsp_if.h"
#include "../scspd/scsp_var.h"
#include "atmarp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/usr.sbin/atm/atmarpd/atmarp_scsp.c,v 1.3 1999/08/28 01:15:30 peter Exp $");
#endif


/*
 * Send the cache for a LIS to SCSP
 *
 *
 * Arguments:
 *	aip	pointer to interface block
 *
 * Returns:
 *	0	cache sent to SCSP OK
 *	errno	reason for failure
 *
 */
int
atmarp_scsp_cache(aip, msg)
	Atmarp_intf	*aip;
	Scsp_if_msg	*msg;
{
	int		i, len, rc = 0;
	Atmarp		*aap;
	Scsp_if_msg	*smp = (Scsp_if_msg *)0;
	Scsp_atmarp_msg	*sap;

	/*
	 * Figure out how big the message needs to be
	 */
	len = sizeof(Scsp_if_msg_hdr);
	for (i = 0; i < ATMARP_HASHSIZ; i++) {
		for (aap = aip->ai_arptbl[i]; aap; aap = aap->aa_next) {
			len += sizeof(Scsp_atmarp_msg);
		}
	}

	/*
	 * Get memory for the cache message
	 */
	smp = (Scsp_if_msg *)UM_ALLOC(len);
	if (!smp) {
		atmarp_mem_err("atmarp_scsp_cache: len");
	}
	UM_ZERO(smp, len);

	/*
	 * Set header fields in SCSP message
	 */
	smp->si_type = SCSP_CACHE_RSP;
	smp->si_proto = SCSP_PROTO_ATMARP;
	smp->si_len = len;
	smp->si_tok = msg->si_tok;

	/*
	 * Loop through the cache, adding each entry to the SCSP
	 * Cache Response message
	 */
	sap = &smp->si_atmarp;
	for (i = 0; i < ATMARP_HASHSIZ; i++) {
		for (aap = aip->ai_arptbl[i]; aap; aap = aap->aa_next) {
			sap->sa_state = SCSP_ASTATE_NEW;
			sap->sa_cpa = aap->aa_dstip;
			ATM_ADDR_COPY(&aap->aa_dstatm, &sap->sa_cha);
			ATM_ADDR_COPY(&aap->aa_dstatmsub, &sap->sa_csa);
			sap->sa_key = aap->aa_key;
			sap->sa_oid = aap->aa_oid;
			sap->sa_seq = aap->aa_seq;
			sap++;
		}
	}

	/*
	 * Send the message to SCSP
	 */
	rc = atmarp_scsp_out(aip, (char *)smp, len);

	/*
	 * Free the message
	 */
	if (smp)
		UM_FREE(smp);

	return(rc);
}


/*
 * Answer a reqeust for information about a cache entry
 *
 * Arguments:
 *	aap	pointer to entry
 *	state	entry's new state
 *
 * Returns:
 *	0	success
 *	errno	reason for failure
 *
 */
int
atmarp_scsp_solicit(aip, smp)
	Atmarp_intf	*aip;
	Scsp_if_msg	*smp;
{
	int		i, rc = 0;
	Atmarp		*aap;
	Scsp_if_msg	*rsp = (Scsp_if_msg *)0;

	/*
	 * Search the interface's ATMARP cache for an entry with
	 * the specified cache key and origin ID
	 */
	for (i = 0; i < ATMARP_HASHSIZ; i++) {
		for (aap = aip->ai_arptbl[i]; aap; aap = aap->aa_next) {
			if (KEY_EQUAL(&aap->aa_key,
					&smp->si_sum.ss_key) &&
					OID_EQUAL(&aap->aa_oid,
					&smp->si_sum.ss_oid))
				break;
		}
		if (aap)
			break;
	}

	/*
	 * Get storage for a Solicit Response
	 */
	rsp = (Scsp_if_msg *)UM_ALLOC(sizeof(Scsp_if_msg));
	if (!rsp) {
		atmarp_mem_err("atmarp_scsp_solicit: sizeof(Scsp_if_msg)");
	}
	UM_ZERO(rsp, sizeof(Scsp_if_msg));

	/*
	 * Fill out the Solicit Rsp
	 */
	rsp->si_type = SCSP_SOLICIT_RSP;
	rsp->si_proto = smp->si_proto;
	rsp->si_tok = smp->si_tok;

	if (aap) {
		/*
		 * Copy fields from the ATMARP entry to the SCSP
		 * Update Request message
		 */
		rsp->si_rc = SCSP_RSP_OK;
		rsp->si_len = sizeof(Scsp_if_msg_hdr) +
				sizeof(Scsp_atmarp_msg);
		rsp->si_atmarp.sa_state = SCSP_ASTATE_UPD;
		rsp->si_atmarp.sa_cpa = aap->aa_dstip;
		ATM_ADDR_COPY(&aap->aa_dstatm, &rsp->si_atmarp.sa_cha);
		ATM_ADDR_COPY(&aap->aa_dstatmsub, &rsp->si_atmarp.sa_csa);
		rsp->si_atmarp.sa_key = aap->aa_key;
		rsp->si_atmarp.sa_oid = aap->aa_oid;
		rsp->si_atmarp.sa_seq = aap->aa_seq;
	} else {
		/*
		 * Entry not found--set return code
		 */
		rsp->si_rc = SCSP_RSP_NOT_FOUND;
		rsp->si_len = smp->si_len;
		rsp->si_sum = smp->si_sum;
	}

	/*
	 * Send the message to SCSP
	 */
	rc = atmarp_scsp_out(aip, (char *)rsp, rsp->si_len);
	UM_FREE(rsp);

	return(rc);
}


/*
 * Send a cache update to SCSP
 *
 * Arguments:
 *	aap	pointer to entry
 *	state	entry's new state
 *
 * Returns:
 *	0	success
 *	errno	reason for failure
 *
 */
int
atmarp_scsp_update(aap, state)
	Atmarp		*aap;
	int		state;
{
	int		rc = 0;
	Atmarp_intf	*aip = aap->aa_intf;
	Scsp_if_msg	*smp = (Scsp_if_msg *)0;

	/*
	 * Make sure the connection to SCSP is active
	 */
	if (aip->ai_state == AI_STATE_NULL) {
		return(0);
	}

	/*
	 * Get memory for the cache message
	 */
	smp = (Scsp_if_msg *)UM_ALLOC(sizeof(Scsp_if_msg));
	if (!smp) {
		atmarp_mem_err("atmarp_scsp_update: sizeof(Scsp_if_msg)");
	}
	UM_ZERO(smp, sizeof(Scsp_if_msg));

	/*
	 * Set header fields in SCSP message
	 */
	smp->si_type = SCSP_UPDATE_REQ;
	smp->si_proto = SCSP_PROTO_ATMARP;
	smp->si_len = sizeof(Scsp_if_msg_hdr) + sizeof(Scsp_atmarp_msg);

	/*
	 * Copy fields from the ATMARP entry to the SCSP
	 * Update Request message
	 */
	smp->si_atmarp.sa_state = state;
	smp->si_atmarp.sa_cpa = aap->aa_dstip;
	ATM_ADDR_COPY(&aap->aa_dstatm, &smp->si_atmarp.sa_cha);
	ATM_ADDR_COPY(&aap->aa_dstatmsub, &smp->si_atmarp.sa_csa);
	smp->si_atmarp.sa_key = aap->aa_key;
	smp->si_atmarp.sa_oid = aap->aa_oid;
	smp->si_atmarp.sa_seq = aap->aa_seq;

	/*
	 * Send the message to SCSP
	 */
	rc = atmarp_scsp_out(aap->aa_intf, (char *)smp, smp->si_len);

	UM_FREE(smp);
	return(rc);
}


/*
 * Respond to a Cache Update Indication from SCSP
 *
 *
 * Arguments:
 *	aip	pointer to interface control block
 *	smp	pointer to message from SCSP
 *
 * Returns:
 *	0	Message processed OK
 *	errno	Reason for failure
 *
 */
int
atmarp_scsp_update_in(aip, smp)
	Atmarp_intf	*aip;
	Scsp_if_msg	*smp;
{
	int	accept, rc;
	Atmarp	*aap;

	/*
	 * Look up the entry
	 */
	ATMARP_LOOKUP(aip, smp->si_atmarp.sa_cpa.s_addr, aap);

	/*
	 * Whether we accept the request depends on whether we
	 * already have an entry for it
	 */
	 if (!aap) {
		/*
		 * We don't have this entry--accept it
		 */
		accept = 1;
	} else {
		/*
		 * We do have an entry for this host--check the
		 * origin ID
		 */
		if (bcmp(&aip->ai_ip_addr.s_addr,
				smp->si_atmarp.sa_oid.id,
				SCSP_ATMARP_ID_LEN) == 0) {
			/*
			 * The received entry originated with us--
			 * reject it
			 */
			accept = 0;
		} else if (bcmp(&aip->ai_ip_addr.s_addr,
				aap->aa_oid.id,
				SCSP_ATMARP_ID_LEN) == 0) {
			/*
			 * We originated the entry we currently have--
			 * only accept the new one if SCSP has higher
			 * priority than the existing entry
			 */
			accept = aap->aa_origin < UAO_SCSP;
		} else {
			/*
			 * Accept the entry if it is more up-to-date
			 * than the existing entry
			 */
			accept = KEY_EQUAL(&aap->aa_key,
					&smp->si_atmarp.sa_key) &&
				OID_EQUAL(&aap->aa_oid,
					&smp->si_atmarp.sa_oid) &&
				(aap->aa_seq < smp->si_atmarp.sa_seq);
		}
	}

	/*
	 * Add the entry to the cache, if appropriate
	 */
	if (accept) {
		if (!aap) {
			/*
			 * Copy info from SCSP to a new cache entry
			 */
			aap = (Atmarp *)UM_ALLOC(sizeof(Atmarp));
			if (!aap)
				atmarp_mem_err("atmarp_scsp_update_in: sizeof(Atmarp)");
			UM_ZERO(aap, sizeof(Atmarp));

			aap->aa_dstip = smp->si_atmarp.sa_cpa;
			aap->aa_dstatm = smp->si_atmarp.sa_cha;
			aap->aa_dstatmsub = smp->si_atmarp.sa_csa;
			aap->aa_key = smp->si_atmarp.sa_key;
			aap->aa_oid = smp->si_atmarp.sa_oid;
			aap->aa_seq = smp->si_atmarp.sa_seq;
			aap->aa_intf = aip;
			aap->aa_origin = UAO_SCSP;

			/*
			 * Add the new entry to our cache
			 */
			ATMARP_ADD(aip, aap);
		} else {
			/*
			 * Update the existing entry
			 */
			aap->aa_dstip = smp->si_atmarp.sa_cpa;
			aap->aa_dstatm = smp->si_atmarp.sa_cha;
			aap->aa_dstatmsub = smp->si_atmarp.sa_csa;
			aap->aa_key = smp->si_atmarp.sa_key;
			aap->aa_oid = smp->si_atmarp.sa_oid;
			aap->aa_seq = smp->si_atmarp.sa_seq;
			aap->aa_origin = UAO_SCSP;
		}

		/*
		 * Send the updated entry to the kernel
		 */
		if (atmarp_update_kernel(aap) == 0)
			rc = SCSP_RSP_OK;
		else
			rc = SCSP_RSP_REJ;
	} else {
		rc = SCSP_RSP_REJ;
	}

	/*
	 * Turn the received message into a response
	 */
	smp->si_type = SCSP_UPDATE_RSP;
	smp->si_rc = rc;

	/*
	 * Send the message to SCSP
	 */
	rc = atmarp_scsp_out(aip, (char *)smp, smp->si_len);

	return(rc);
}


/*
 * Read and process a message from SCSP
 *
 *
 * Arguments:
 *	aip	interface for read
 *
 * Returns:
 *	0	success
 *	errno	reason for failure
 *
 */
int
atmarp_scsp_read(aip)
	Atmarp_intf	*aip;
{
	int		len, rc = 0;
	char		*buff = (char *)0;
	Scsp_if_msg	*smp;
	Scsp_if_msg_hdr	msg_hdr;

	/*
	 * Read the header of the message from SCSP
	 */
	len = read(aip->ai_scsp_sock, (char *)&msg_hdr,
			sizeof(msg_hdr));
	if (len == -1) {
		rc = errno;
		goto read_fail;
	} else if (len != sizeof(msg_hdr)) {
		rc = EMSGSIZE;
		goto read_fail;
	}

	/*
	 * Get a buffer that will hold the message
	 */
	buff = UM_ALLOC(msg_hdr.sh_len);
	if (!buff)
		atmarp_mem_err("atmarp_scsp_read: msg_hdr.sh_len");
	UM_COPY(&msg_hdr, buff, sizeof(msg_hdr));

	/*
	 * Read the rest of the message, if there is more than
	 * just a header
	 */
	len = msg_hdr.sh_len - sizeof(msg_hdr);
	if (len > 0) {
		len = read(aip->ai_scsp_sock, buff + sizeof(msg_hdr),
				len);
		if (len == -1) {
			rc = errno;
			goto read_fail;
		} else if (len != msg_hdr.sh_len - sizeof(msg_hdr)) {
			rc = EMSGSIZE;
			goto read_fail;
		}
	}

	/*
	 * Handle the message based on its type
	 */
	smp = (Scsp_if_msg *)buff;
	switch(smp->si_type) {
	case SCSP_CFG_RSP:
		if (smp->si_rc != SCSP_RSP_OK) {
			rc = EINVAL;
			goto read_fail;
		}
		break;
	case SCSP_CACHE_IND:
		rc = atmarp_scsp_cache(aip, smp);
		break;
	case SCSP_SOLICIT_IND:
		rc = atmarp_scsp_solicit(aip, smp);
		break;
	case SCSP_UPDATE_IND:
		rc = atmarp_scsp_update_in(aip, smp);
		break;
	case SCSP_UPDATE_RSP:
		/*
		 * Ignore Update Responses
		 */
		break;
	default:
		atmarp_log(LOG_ERR, "Unexpected SCSP message received");
		return(EOPNOTSUPP);
	}

	UM_FREE(buff);
	return(rc);

read_fail:
	if (buff) {
		UM_FREE(buff);
	}

	/*
	 * Error on socket to SCSP--close the socket and set the state
	 * so that we know to retry when the cache timer fires.
	 */
	atmarp_scsp_close(aip);

	return(rc);
}


/*
 * Send a message to SCSP
 *
 *
 * Arguments:
 *	aip	pointer to ATMARP interface to send message on
 *	buff	pointer to message buffer
 *	len	length of message
 *
 * Returns:
 *	0	message sent
 *	errno	reason for failure
 *
 */
int
atmarp_scsp_out(aip, buff, len)
	Atmarp_intf	*aip;
	char		*buff;
	int		len;
{
	int	rc;

	/*
	 * Send the message to SCSP
	 */
	rc = write(aip->ai_scsp_sock, buff, len);
	if (rc == len)
		return(0);

	/*
	 * Error on write--close the socket to SCSP, clean up and
	 * set the state so that we know to retry when the cache
	 * timer fires.
	 */
	atmarp_scsp_close(aip);

	/*
	 * Set the return code
	 */
	if (rc < 0) {
		rc = errno;
	} else {
		rc = EFAULT;
	}

	return(rc);
}


/*
 * Set up a socket and connect to SCSP
 *
 * Arguments:
 *	aip	pointer to interface block
 *
 * Returns:
 *	0	success, ai_scsp_sock is set
 *	errno	reason for failure
 *
 *
 */
int
atmarp_scsp_connect(aip)
	Atmarp_intf	*aip;
{
	int		len, rc, sd;
	char		*sn;
	Scsp_if_msg	cfg_msg;

	static struct sockaddr	local_addr = {
#if (defined(BSD) && (BSD >= 199103))
		sizeof(struct sockaddr),	/* sa_len */
#endif
		AF_UNIX,			/* sa_family */
		ATMARP_SOCK_PREFIX		/* sa_data */
	};
	static struct sockaddr	scsp_addr = {
#if (defined(BSD) && (BSD >= 199103))
		sizeof(struct sockaddr),	/* sa_len */
#endif
		AF_UNIX,			/* sa_family */
		SCSPD_SOCK_NAME			/* sa_data */
	};

	/*
	 * Construct a name for the socket
	 */
	strncpy(local_addr.sa_data, ATMARP_SOCK_PREFIX,
			sizeof(local_addr.sa_data));
	(void)strncat(local_addr.sa_data, aip->ai_intf,
			sizeof(local_addr.sa_data));
	sn = strdup(local_addr.sa_data);
	if (!sn)
		atmarp_mem_err("atmarp_scsp_connect: strdup");

	/*
	 * Clean up any old socket
	 */
	rc = unlink(sn);
	if (rc < 0 && errno != ENOENT)
		return(errno);

	/*
	 * Open a socket to SCSP
	 */
	sd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sd == -1) {
		UM_FREE(sn);
		return(errno);
	}
	if (sd > atmarp_max_socket) {
		atmarp_max_socket = sd;
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
		rc = errno;
		goto scsp_connect_fail;
	}

	/*
	 * Bind the local socket address
	 */
	rc = bind(sd, &local_addr, sizeof(local_addr));
	if (rc) {
		rc = errno;
		goto scsp_connect_fail;
	}

	/*
	 * Connect to SCSP
	 */
	rc = connect(sd, &scsp_addr, sizeof(scsp_addr));
	if (rc) {
		rc = errno;
		goto scsp_connect_fail;
	}

	/*
	 * Save socket information in interface control block
	 */
	aip->ai_scsp_sock = sd;
	aip->ai_scsp_sockname = sn;
	aip->ai_state = AI_STATE_UP;

	/*
	 * Send configuration information to SCSP
	 */
	UM_ZERO(&cfg_msg, sizeof(cfg_msg));
	cfg_msg.si_type = SCSP_CFG_REQ;
	cfg_msg.si_proto = SCSP_PROTO_ATMARP;
	strcpy(cfg_msg.si_cfg.atmarp_netif, aip->ai_intf);
	len =sizeof(Scsp_if_msg_hdr) + strlen(aip->ai_intf) + 1;
	cfg_msg.si_len = len;
	rc = atmarp_scsp_out(aip, (char *)&cfg_msg, len);
	if (rc) {
		return(rc);
	}

	return(0);

scsp_connect_fail:
	(void)close(sd);
	aip->ai_scsp_sock = -1;
	UM_FREE(sn);
	aip->ai_scsp_sockname = NULL;
	aip->ai_state = AI_STATE_NULL;
	return(rc);
}


/*
 * Close a socket connection to SCSP
 *
 * Arguments:
 *	aip	pointer to interface block for connection to be closed
 *
 * Returns:
 *	none
 *
 *
 */
void
atmarp_scsp_close(aip)
	Atmarp_intf	*aip;
{
	/*
	 * Close and unlink the SCSP socket
	 */
	(void)close(aip->ai_scsp_sock);
	aip->ai_scsp_sock = -1;
	(void)unlink(aip->ai_scsp_sockname);
	UM_FREE(aip->ai_scsp_sockname);
	aip->ai_scsp_sockname = NULL;

	aip->ai_state = AI_STATE_NULL;

	return;
}


/*
 * Disconnect an interface from SCSP
 *
 * Arguments:
 *	aip	pointer to interface block for connection to be closed
 *
 * Returns:
 *	0	success, ai_scsp_sock is set
 *	errno	reason for failure
 *
 *
 */
int
atmarp_scsp_disconnect(aip)
	Atmarp_intf	*aip;
{
	int	i;
	Atmarp	*aap;

	/*
	 * Close and unlink the SCSP socket
	 */
	atmarp_scsp_close(aip);

	/*
	 * Free the ATMARP cache associated with the interface
	 */
	for (i = 0; i < ATMARP_HASHSIZ; i++) {
		for (aap = aip->ai_arptbl[i]; aap; aap = aap->aa_next) {
			UM_FREE(aap);
		}
		aip->ai_arptbl[i] = (Atmarp *)0;
	}

	return(0);
}
