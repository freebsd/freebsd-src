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
 *	@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_output.c,v 1.3 1999/08/28 01:15:33 peter Exp $
 *
 */

/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * Output packet processing
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/ethernet.h>
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
#include <unistd.h>

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_output.c,v 1.3 1999/08/28 01:15:33 peter Exp $");
#endif


/*
 * Put a long integer into the output buffer
 *
 * This routine is provided for cases where long ints may not be
 * word-aligned in the output buffer.
 *
 * Arguments:
 *	l	long integer
 *	cp	pointer to output buffer
 *
 * Returns:
 *	None
 *
 */
static void
put_long(l, cp)
	u_long	l;
	u_char	*cp;
{
	u_long	nl;

	/*
	 * Convert to network order and copy to output buffer
	 */
	nl = htonl(l);
	UM_COPY(&nl, cp, sizeof(u_long));
}


/*
 * Format a Sender or Receiver ID
 *
 * Arguments:
 *	idp	ponter to ID structure
 *	buff	pointer to ID
 *
 * Returns:
 *	0	input was invalid
 *	else	length of ID processed
 *
 */
static int
scsp_format_id(idp, buff)
	Scsp_id	*idp;
	char	*buff;
{
	/*
	 * Copy the ID
	 */
	UM_COPY(idp->id, buff, idp->id_len);

	/*
	 * Return the ID length
	 */
	return(idp->id_len);
}


/*
 * Format the Mandatory Common Part of an SCSP input packet
 *
 * Arguments:
 *	mcp	pointer to MCP
 *	buff	pointer to mandatory common part
 *
 * Returns:
 *	0	input was invalid
 *	else	length of MCP in message
 *
 */
static int
scsp_format_mcp(mcp, buff)
	Scsp_mcp	*mcp;
	char		*buff;
{
	int			len;
	char			*odp;
	struct scsp_nmcp	*smp;

	/*
	 * Set the protocol ID
	 */
	smp = (struct scsp_nmcp *)buff;
	smp->sm_pid = htons(mcp->pid);

	/*
	 * Set the server group ID
	 */
	smp->sm_sgid = htons(mcp->sgid);

	/*
	 * Set the flags
	 */
	smp->sm_flags = htons(mcp->flags);

	/*
	 * Set the sender ID and length
	 */
	smp->sm_sid_len = mcp->sid.id_len;
	odp = buff + sizeof(struct scsp_nmcp);
	len = scsp_format_id(&mcp->sid, odp);
	if (len == 0) {
		goto mcp_invalid;
	 }

	/*
	 * Set the receiver ID and length
	 */
	smp->sm_rid_len = mcp->rid.id_len;
	odp += mcp->sid.id_len;
	len = scsp_format_id(&mcp->rid, odp);
	if (len == 0) {
		goto mcp_invalid;
	 }

	/*
	 * Set the record count
	 */
	smp->sm_rec_cnt = htons(mcp->rec_cnt);

	/*
	 * Return the length of data we processed
	 */
	return(sizeof(struct scsp_nmcp) + mcp->sid.id_len +
			mcp->rid.id_len);

mcp_invalid:
	return(0);
}


/*
 * Format an Extension
 *
 * Arguments:
 *	exp	pointer to extension in internal format
 *	buff	pointer to output buffer
 *	blen	space available in buffer
 *
 * Returns:
 *	0	input was invalid
 *	else	length of extension processed
 *
 */
static int
scsp_format_ext(exp, buff, blen)
	Scsp_ext	*exp;
	char		*buff;
	int		blen;
{
	struct scsp_next	*sep;

	/*
	 * Make sure there's room in the buffer
	 */
	if (blen < (sizeof(struct scsp_next) + exp->len))
		return(0);

	/*
	 * Set the type
	 */
	sep = (struct scsp_next *)buff;
	sep->se_type = htons(exp->type);

	/*
	 * Set the length
	 */
	sep->se_len = htons(exp->len);

	/*
	 * Set the value
	 */
	if (exp->len > 0) {
		buff += sizeof(struct scsp_next);
		UM_COPY((caddr_t)exp + sizeof(Scsp_ext),
				buff,
				exp->len);
	}

	/*
	 * Return the number of bytes processed
	 */
	return(sizeof(struct scsp_next) + exp->len);
}


/*
 * Format the ATMARP part of a CSA record
 *
 * Arguments:
 *	acsp	pointer to ATMARP protocol-specific CSA record
 *	buff	pointer to output buffer
 *
 * Returns:
 *	0	input was invalid
 *	else	length of record processed
 *
 */
static int
scsp_format_atmarp(acsp, buff)
	Scsp_atmarp_csa	*acsp;
	char		*buff;
{
	char			*cp;
	int			len, pkt_len;
	struct scsp_atmarp_ncsa	*sanp;

	/*
	 * Figure out how long PDU is going to be
	 */
	pkt_len = sizeof(struct scsp_atmarp_ncsa);
	switch (acsp->sa_sha.address_format) {
	case T_ATM_ENDSYS_ADDR:
		pkt_len += acsp->sa_sha.address_length;
		break;

	case T_ATM_E164_ADDR:
		pkt_len += acsp->sa_sha.address_length;
		if (acsp->sa_ssa.address_format == T_ATM_ENDSYS_ADDR)
			pkt_len += acsp->sa_ssa.address_length;
		break;
	}

	switch (acsp->sa_tha.address_format) {
	case T_ATM_ENDSYS_ADDR:
		pkt_len += acsp->sa_tha.address_length;
		break;

	case T_ATM_E164_ADDR:
		pkt_len += acsp->sa_tha.address_length;
		if (acsp->sa_tha.address_format == T_ATM_ENDSYS_ADDR)
			pkt_len += acsp->sa_tha.address_length;
		break;
	}

	if (acsp->sa_spa.s_addr != 0)
		pkt_len += sizeof(struct in_addr);

	if (acsp->sa_tpa.s_addr != 0)
		pkt_len += sizeof(struct in_addr);

	/*
	 * Set up pointers
	 */
	sanp = (struct scsp_atmarp_ncsa *)buff;
	cp = (char *)sanp + sizeof(struct scsp_atmarp_ncsa);

	/*
	 * Build fields
	 */
	sanp->sa_hrd = htons(ARP_ATMFORUM);
	sanp->sa_pro = htons(ETHERTYPE_IP);

	/* sa_sha */
	len = acsp->sa_sha.address_length;
	switch (acsp->sa_sha.address_format) {
	case T_ATM_ENDSYS_ADDR:
		sanp->sa_shtl = ARP_TL_NSAPA | (len & ARP_TL_LMASK);

		/* sa_sha */
		UM_COPY(acsp->sa_sha.address, cp, len);
		cp += len;

		sanp->sa_sstl = 0;
		break;

	case T_ATM_E164_ADDR:
		sanp->sa_shtl = ARP_TL_E164 | (len & ARP_TL_LMASK);

		/* sa_sha */
		UM_COPY(acsp->sa_sha.address, cp, len);
		cp += len;

		if (acsp->sa_ssa.address_format == T_ATM_ENDSYS_ADDR) {
			len = acsp->sa_ssa.address_length;
			sanp->sa_sstl = ARP_TL_NSAPA |
					(len & ARP_TL_LMASK);

			/* sa_ssa */
			UM_COPY(acsp->sa_ssa.address, cp, len);
			cp += len;
		} else
			sanp->sa_sstl = 0;
		break;

	default:
		sanp->sa_shtl = 0;
		sanp->sa_sstl = 0;
	}

	/* sa_state */
	sanp->sa_state = acsp->sa_state;
	sanp->sa_fill1 = 0;

	/* sa_spa */
	if (acsp->sa_spa.s_addr != 0) {
		sanp->sa_spln = sizeof(struct in_addr);
		UM_COPY(&acsp->sa_spa, cp, sizeof(struct in_addr));
		cp += sizeof(struct in_addr);
	}

	/* sa_tha */
	len = acsp->sa_tha.address_length;
	switch (acsp->sa_tha.address_format) {
	case T_ATM_ENDSYS_ADDR:
		sanp->sa_thtl = ARP_TL_NSAPA | (len & ARP_TL_LMASK);

		/* sa_tha */
		UM_COPY(acsp->sa_tha.address, cp, len);
		cp += len;

		sanp->sa_tstl = 0;
		break;

	case T_ATM_E164_ADDR:
		sanp->sa_thtl = ARP_TL_E164 | (len & ARP_TL_LMASK);

		/* sa_tha */
		UM_COPY(acsp->sa_tha.address, cp, len);
		cp += len;

		if (acsp->sa_tsa.address_format == T_ATM_ENDSYS_ADDR) {
			len = acsp->sa_tha.address_length;
			sanp->sa_tstl = ARP_TL_NSAPA |
					(len & ARP_TL_LMASK);

			/* sa_tsa */
			UM_COPY(acsp->sa_tsa.address, cp, len);
			cp += len;
		} else
			sanp->sa_tstl = 0;
		break;

	default:
		sanp->sa_thtl = 0;
		sanp->sa_tstl = 0;
	}

	/* sa_tpa */
	if (acsp->sa_tpa.s_addr != 0) {
		sanp->sa_tpln = sizeof(struct in_addr);
		UM_COPY(&acsp->sa_tpa, cp, sizeof(struct in_addr));
	}

	return(pkt_len);
}


/*
 * Format a Cache State Advertisement or Cache State Advertisement
 * Summary record
 *
 * Arguments:
 *	csapp	pointer to CSA or CSAS
 *	buff	pointer to output buffer
 *
 * Returns:
 *	0	input was invalid
 *	else	length of record processed
 *
 */
static int
scsp_format_csa(csap, buff)
	Scsp_csa	*csap;
	char		*buff;
{
	int			len = 0;
	char			*odp;
	struct scsp_ncsa	*scp;

	/*
	 * Set the hop count
	 */
	scp = (struct scsp_ncsa *)buff;
	scp->scs_hop_cnt = htons(csap->hops);

	/*
	 * Set the null flag
	 */
	if (csap->null) {
		scp->scs_nfill = htons(SCSP_CSAS_NULL);
	}

	/*
	 * Set the sequence number
	 */
	put_long(csap->seq, (u_char *)&scp->scs_seq);

	/*
	 * Set the cache key
	 */
	scp->scs_ck_len = csap->key.key_len;
	odp = buff + sizeof(struct scsp_ncsa);
	UM_COPY(csap->key.key, odp, scp->scs_ck_len);

	/*
	 * Set the originator ID
	 */
	odp += scp->scs_ck_len;
	scp->scs_oid_len = scsp_format_id(&csap->oid, odp);

	/*
	 * Set the protocol-specific data, if present.  At the
	 * moment, we only handle data for ATMARP.
	 */
	if (csap->atmarp_data) {
		odp += scp->scs_oid_len;
		len = scsp_format_atmarp(csap->atmarp_data, odp);
	}

	/*
	 * Set the record length
	 */
	scp->scs_len = htons(sizeof(struct scsp_ncsa) +
			scp->scs_ck_len + scp->scs_oid_len +
			len);

	/*
	 * Return the length of data we processed
	 */
	return(ntohs(scp->scs_len));
}


/*
 * Format a Cache Alignment message
 *
 * Arguments:
 *	cap	pointer to CA message
 *	buff	pointer to output buffer
 *	blen	space available in buffer
 *
 * Returns:
 *	0	input was invalid
 *	else	length of CA message processed
 *
 */
static int
scsp_format_ca(cap, buff, blen)
	Scsp_ca	*cap;
	char	*buff;
	int	blen;
{
	int		i, len, proc_len;
	struct scsp_nca	*scap;
	Scsp_csa	*csap;

	/*
	 * Set the sequence number
	 */
	scap = (struct scsp_nca *)buff;
	put_long(cap->ca_seq, (u_char *)&scap->sca_seq);
	proc_len = sizeof(scap->sca_seq);
	buff += sizeof(scap->sca_seq);

	/*
	 * Set the flags
	 */
	cap->ca_mcp.flags = 0;
	if (cap->ca_m)
		cap->ca_mcp.flags |= SCSP_CA_M;
	if (cap->ca_i)
		cap->ca_mcp.flags |= SCSP_CA_I;
	if (cap->ca_o)
		cap->ca_mcp.flags |= SCSP_CA_O;

	/*
	 * Format the mandatory common part of the message
	 */
	len = scsp_format_mcp(&cap->ca_mcp, buff);
	if (len == 0)
		goto ca_invalid;
	buff += len;
	proc_len += len;

	/*
	 * Put any CSAS records into the message
	 */
	for (i = 0, csap = cap->ca_csa_rec; i < cap->ca_mcp.rec_cnt;
			i++, csap = csap->next) {
		len = scsp_format_csa(csap, buff);
		buff += len;
		proc_len += len;
		if (proc_len > blen) {
			scsp_log(LOG_CRIT, "scsp_format_ca: buffer overflow");
			abort();
		}
	}

	/*
	 * Return the length of processed data
	 */
	return(proc_len);

ca_invalid:
	return(0);
}


/*
 * Format a Cache State Update Request, Cache State Update Reply, or
 * Cache State Update Solicit message.  These all have the same format,
 * a Mandatory Common Part followed by a number of CSA or CSAS records.
 *
 * Arguments:
 *	csup	pointer to location to put pointer to CSU Req message
 *	buff	pointer to output buffer
 *	blen	space available in buffer
 *
 * Returns:
 *	0	input was invalid
 *	else	length of CSU Req message processed
 *
 */
static int
scsp_format_csu(csup, buff, blen)
	Scsp_csu_msg	*csup;
	char		*buff;
	int		blen;
{
	int			i, len, proc_len;
	struct scsp_ncsu_msg	*scsup;
	Scsp_csa		*csap;

	/*
	 * Format the mandatory common part of the message
	 */
	scsup = (struct scsp_ncsu_msg *)buff;
	len = scsp_format_mcp(&csup->csu_mcp, buff);
	if (len == 0)
		goto csu_invalid;
	buff += len;
	proc_len = len;

	/*
	 * Put the CSAS records into the message
	 */
	for (i = 0, csap = csup->csu_csa_rec;
			i < csup->csu_mcp.rec_cnt && csap;
			i++, csap = csap->next) {
		len = scsp_format_csa(csap, buff);
		buff += len;
		proc_len += len;
		if (proc_len > blen) {
			scsp_log(LOG_CRIT, "scsp_format_csu: buffer overflow");
			abort();
		}
	}

	/*
	 * Return the length of processed data
	 */
	return(proc_len);

csu_invalid:
	return(0);
}


/*
 * Format a Hello message
 *
 * Arguments:
 *	hpp	pointer to Hello message
 *	buff	pointer to output buffer
 *	blen	space available in buffer
 *
 * Returns:
 *	0	input was invalid
 *	else	length of Hello message processed
 *
 */
static int
scsp_format_hello(hp, buff, blen)
	Scsp_hello	*hp;
	char		*buff;
	int		blen;
{
	int			len, proc_len;
	struct scsp_nhello	*shp;
	Scsp_id			*ridp;

	/*
	 * Set the hello interval
	 */
	shp = (struct scsp_nhello *)buff;
	shp->sch_hi = htons(hp->hello_int);

	/*
	 * Set the dead factor
	 */
	shp->sch_df = htons(hp->dead_factor);

	/*
	 * Set the family ID
	 */
	shp->sch_fid = htons(hp->family_id);

	/*
	 * Process the mandatory common part of the message
	 */
	proc_len = sizeof(struct scsp_nhello) -
			sizeof(struct scsp_nmcp);
	buff += proc_len;
	len = scsp_format_mcp(&hp->hello_mcp, buff);
	if (len == 0)
		goto hello_invalid;
	proc_len += len;
	buff += len;

	/*
	 * Add any additional receiver ID records to the message
	 */
	for (ridp = hp->hello_mcp.rid.next; ridp;
			ridp = ridp->next) {
		len = scsp_format_id(ridp, buff);
		if (len == 0) {
			goto hello_invalid;
		}
		proc_len += len;
		buff += len;
	}

	/*
	 * Return the length of the Hello message body
	 */
	if (proc_len > blen) {
		scsp_log(LOG_CRIT, "scsp_format_hello: buffer overflow");
		abort();
	}
	return(proc_len);

hello_invalid:
	return(0);
}


/*
 * Format an SCSP output packet
 *
 * Arguments:
 *	dcsp	pointer to DCS for which message is being prepared
 *	msg	pointer to input packet
 *	bpp	pointer to location to put pointer to formatted packet
 *
 * Returns:
 *	0	input packet was invalid
 *	else	length of formatted packet
 *
 */
int
scsp_format_msg(dcsp, msg, bpp)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	char		**bpp;
{
	char			*buff = (char *)0, *e_buff = (char *)0;
	int			buff_len, e_buff_len;
	int			e_len, len, plen;
	struct scsp_nhdr	*shp;
	Scsp_ext		*exp;

	/*
	 * Allocate a buffer for the message
	 */
	buff_len = dcsp->sd_server->ss_mtu;
	buff = (char *)UM_ALLOC(buff_len);
	if (!buff) {
		scsp_mem_err("scsp_format_msg: dcsp->sd_server->ss_mtu");
	}
	UM_ZERO(buff, buff_len);
	*bpp = buff;

	/*
	 * Encode the fixed header
	 *
	 * Set the version
	 */
	shp = (struct scsp_nhdr *)buff;
	shp->sh_ver = SCSP_VER_1;

	/*
	 * Set the message type
	 */
	shp->sh_type = msg->sc_msg_type;

	/*
	 * Point past the fixed header
	 */
	len = sizeof(struct scsp_nhdr);
	buff_len -= len;

	/*
	 * Encode any extensions into a temporary buffer
	 */
	e_len = 0;
	if (msg->sc_ext) {
		/*
		 * Get a buffer for the extensions
		 */
		e_buff_len = 1024;
		e_buff = (char *)UM_ALLOC(e_buff_len);
		if (!buff) {
			scsp_mem_err("scsp_format_msg: e_buff_len");
		}
		UM_ZERO(e_buff, e_buff_len);

		/*
		 * Encode the extensions
		 */
		for (exp = msg->sc_ext = 0; exp; exp = exp->next) {
			plen = scsp_format_ext(exp, e_buff + e_len,
					e_buff_len - e_len);
			if (plen == 0) {
				goto ignore;
			}
			e_len += plen;
		}

		/*
		 * Free the buffer if we didn't use it
		 */
		if (!e_len) {
			UM_FREE(e_buff);
			e_buff = (char *)0;
		}
	}
	buff_len -= e_len;

	/*
	 * Encode the body of the message, depending on the type
	 */
	switch(msg->sc_msg_type) {
	case SCSP_CA_MSG:
		plen = scsp_format_ca(msg->sc_ca, buff + len, buff_len);
		break;
	case SCSP_CSU_REQ_MSG:
	case SCSP_CSU_REPLY_MSG:
	case SCSP_CSUS_MSG:
		plen = scsp_format_csu(msg->sc_csu_msg, buff + len,
				buff_len);
		break;
	case SCSP_HELLO_MSG:
		plen = scsp_format_hello(msg->sc_hello, buff + len,
				buff_len);
		break;
	default:
		goto ignore;
	}
	if (plen == 0) {
		goto ignore;
	}
	len += plen;

	/*
	 * Copy the extensions to the end of the message
	 */
	if (e_len) {
		shp->sh_ext_off = htons(len);
		UM_COPY(e_buff, buff + len, e_len);
		UM_FREE(e_buff);
	}

	/*
	 * Set the length
	 */
	shp->sh_len = htons(len);

	/*
	 * Compute the message checksum
	 */
	shp->sh_checksum = htons(ip_checksum(buff, len));

	/*
	 * Return the length of the buffer
	 */
	return(len);

ignore:
	if (buff)
		UM_FREE(buff);
	if (e_buff)
		UM_FREE(e_buff);
	*bpp = (char *)0;
	return(0);
}


/*
 * Send an SCSP message
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message to send
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_send_msg(dcsp, msg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
{
	int	len, rc;
	char	*buff;

	/*
	 * Make sure we have a socket open
	 */
	if (dcsp->sd_sock == -1) {
		return(EBADF);
	}

	/*
	 * Trace the message
	 */
	if (((scsp_trace_mode & SCSP_TRACE_HELLO_MSG) &&
			msg->sc_msg_type == SCSP_HELLO_MSG) ||
			((scsp_trace_mode & SCSP_TRACE_CA_MSG) &&
			msg->sc_msg_type != SCSP_HELLO_MSG)) {
		scsp_trace_msg(dcsp, msg, 0);
		scsp_trace("\n");
	}

	/*
	 * Put the message into network format
	 */
	len = scsp_format_msg(dcsp, msg, &buff);
	if (len == 0) {
		scsp_log(LOG_ERR, "scsp_send_msg: message conversion failed\n");
		abort();
	}

	/*
	 * Write the message to the DCS
	 */
	rc = write(dcsp->sd_sock, (void *)buff, len);
	UM_FREE(buff);
	if (rc == len || (rc == -1 && errno == EINPROGRESS)) {
		rc = 0;
	} else {
		/*
		 * There was an error on the write--close the VCC
		 */
		(void)close(dcsp->sd_sock);
		dcsp->sd_sock = -1;

		/*
		 * Inform the Hello FSM
		 */
		(void)scsp_hfsm(dcsp, SCSP_HFSM_VC_CLOSED,
				(Scsp_msg *)0);

		/*
		 * Set the return code
		 */
		if (rc == -1)
			rc = errno;
		else
			rc = EINVAL;
	}

	return(rc);
}
