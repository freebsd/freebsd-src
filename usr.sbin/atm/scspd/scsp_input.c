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
 * Input packet processing
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

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


static int scsp_parse_atmarp __P((char *, int, Scsp_atmarp_csa **));


/*
 * Get a long ingeter
 *
 * This routine is provided to handle long integers that may not
 * be word-aligned in the input buffer.
 *
 * Arguments:
 *	cp	pointer to long int in message
 *
 * Returns:
 *	int	long int in host order
 *
 */
static u_long
get_long(cp)
	u_char	*cp;
{
	int	i;
	u_long	l;

	/*
	 * Read the long out of the input buffer
	 */
	l = 0;
	for (i = 0; i < sizeof(u_long); i++)
		l = (l << 8) + *cp++;

	/*
	 * Return the value in host order
	 */
	return(l);
}


/*
 * Free an SCSP Cache Alignment message in internal format
 *
 * Arguments:
 *	cap	pointer to CA message
 *
 * Returns:
 *	None
 *
 */
static void
scsp_free_ca(cap)
	Scsp_ca	*cap;
{
	Scsp_csa	*csap, *ncsap;

	/*
	 * Return if there's nothing to free
	 */
	if (cap == (Scsp_ca *)0)
		return;

	/*
	 * Free the CSAS records
	 */
	for (csap = cap->ca_csa_rec; csap; csap = ncsap) {
		ncsap = csap->next;
		SCSP_FREE_CSA(csap);
	}
	/*
	 * Free the CA message structure
	 */
	UM_FREE(cap);
}


/*
 * Free an SCSP Cache State Update Request, Cache State Update Reply,
 * or Cache State Update Solicit message in internal format
 *
 * Arguments:
 *	csup	pointer to CSU message
 *
 * Returns:
 *	None
 *
 */
static void
scsp_free_csu(csup)
	Scsp_csu_msg	*csup;
{
	Scsp_csa	*csap, *ncsap;

	/*
	 * Return if there's nothing to free
	 */
	if (csup == (Scsp_csu_msg *)0)
		return;

	/*
	 * Free the CSA records
	 */
	for (csap = csup->csu_csa_rec; csap; csap = ncsap) {
		ncsap = csap->next;
		SCSP_FREE_CSA(csap);
	}

	/*
	 * Free the CSU message structure
	 */
	UM_FREE(csup);
}


/*
 * Free an SCSP Hello message in  internal format
 *
 * Arguments:
 *	hp	pointer to Hello message
 *
 * Returns:
 *	None
 *
 */
static void
scsp_free_hello(hp)
	Scsp_hello	*hp;
{
	/*
	 * Return if there's nothing to free
	 */
	if (hp == (Scsp_hello *)0)
		return;

	/*
	 * Free the Hello message structure
	 */
	UM_FREE(hp);
}


/*
 * Free an SCSP message in  internal format
 *
 * Arguments:
 *	msg	pointer to input packet
 *
 * Returns:
 *	None
 *
 */
void
scsp_free_msg(msg)
	Scsp_msg	*msg;
{
	Scsp_ext	*exp, *nexp;

	/*
	 * Return if there's nothing to free
	 */
	if (msg == (Scsp_msg *)0)
		return;

	/*
	 * Free the message body
	 */
	switch(msg->sc_msg_type) {
	case SCSP_CA_MSG:
		scsp_free_ca(msg->sc_ca);
		break;
	case SCSP_CSU_REQ_MSG:
	case SCSP_CSU_REPLY_MSG:
	case SCSP_CSUS_MSG:
		scsp_free_csu(msg->sc_csu_msg);
		break;
	case SCSP_HELLO_MSG:
		scsp_free_hello(msg->sc_hello);
		break;
	}

	/*
	 * Free any extensions
	 */
	for (exp = msg->sc_ext; exp; exp = nexp) {
		nexp = exp->next;
		UM_FREE(exp);
	}

	/*
	 * Free the message structure
	 */
	UM_FREE(msg);
}


/*
 * Parse a Sender or Receiver ID
 *
 * Arguments:
 *	buff	pointer to ID
 *	id_len	length of ID
 *	idp	pointer to structure to receive the ID
 *
 * Returns:
 *	0	input was invalid
 *	else	length of ID processed
 *
 */
static int
scsp_parse_id(buff, id_len, idp)
	char	*buff;
	int	id_len;
	Scsp_id	*idp;
{
	/*
	 * Sanity check
	 */
	if (!buff ||
			id_len == 0 || id_len > SCSP_MAX_ID_LEN ||
			!idp) {
		return(0);
	}

	/*
	 * Save the ID length
	 */
	idp->id_len = id_len;

	/*
	 * Get the ID
	 */
	UM_COPY(buff, idp->id, id_len);

	/*
	 * Return the ID length
	 */
	return(id_len);
}


/*
 * Parse the Mandatory Common Part of an SCSP input packet
 *
 * Arguments:
 *	buff	pointer to mandatory common part
 *	pdu_len	length of input packet
 *	mcp	pointer to location of MCP in decoded record
 *
 * Returns:
 *	0	input was invalid
 *	else	length of MCP in message
 *
 */
static int
scsp_parse_mcp(buff, pdu_len, mcp)
	char		*buff;
	int		pdu_len;
	Scsp_mcp	*mcp;
{
	int			len;
	u_char			*idp;
	struct scsp_nmcp	*smp;

	/*
	 * Get the protocol ID
	 */
	smp = (struct scsp_nmcp *)buff;
	mcp->pid = ntohs(smp->sm_pid);
	if (mcp->pid < SCSP_PROTO_ATMARP ||
			mcp->pid > SCSP_PROTO_LNNI) {
		/* Protocol ID is invalid */
		goto mcp_invalid;
	}

	/*
	 * Get the server group ID
	 */
	mcp->sgid = ntohs(smp->sm_sgid);

	/*
	 * Get the flags
	 */
	mcp->flags = ntohs(smp->sm_flags);

	/*
	 * Get the sender ID and length
	 */
	idp = (u_char *) ((caddr_t)smp + sizeof(struct scsp_nmcp));
	len = scsp_parse_id(idp, smp->sm_sid_len, &mcp->sid);
	if (len == 0) {
		goto mcp_invalid;
	 }

	/*
	 * Get the receiver ID and length
	 */
	idp += len;
	len = scsp_parse_id(idp, smp->sm_rid_len, &mcp->rid);
	if (len == 0) {
		goto mcp_invalid;
	 }

	/*
	 * Get the record count
	 */
	mcp->rec_cnt = ntohs(smp->sm_rec_cnt);

	/*
	 * Return the length of data we processed
	 */
	return(sizeof(struct scsp_nmcp) + smp->sm_sid_len +
			smp->sm_rid_len);

mcp_invalid:
	return(0);
}


/*
 * Parse an Extension
 *
 * Arguments:
 *	buff	pointer to Extension
 *	pdu_len	length of buffer
 *	expp	pointer to location to receive pointer to the Extension
 *
 * Returns:
 *	0	input was invalid
 *	else	length of Extension processed
 *
 */
static int
scsp_parse_ext(buff, pdu_len, expp)
	char		*buff;
	int		pdu_len;
	Scsp_ext	**expp;
{
	int			len;
	struct scsp_next	*sep;
	Scsp_ext		*exp;

	/*
	 * Get memory for the extension
	 */
	sep = (struct scsp_next *)buff;
	len = sizeof(Scsp_ext) + ntohs(sep->se_len);
	exp = (Scsp_ext *)UM_ALLOC(len);
	if (!exp) {
		goto ext_invalid;
	}
	UM_ZERO(exp, len);

	/*
	 * Get the type
	 */
	exp->type = ntohs(sep->se_type);

	/*
	 * Get the length
	 */
	exp->len = ntohs(sep->se_len);

	/*
	 * Get the value
	 */
	if (exp->len > 0) {
		UM_COPY((caddr_t)sep + sizeof(struct scsp_next),
				(caddr_t)exp + sizeof(Scsp_ext),
				exp->len);
	}

	/*
	 * Save a pointer to the extension and return the
	 * number of bytes processed
	 */
	*expp = exp;
	return(sizeof(struct scsp_next) + exp->len);

ext_invalid:
	if (exp) {
		UM_FREE(exp);
	}
	return(0);
}


/*
 * Parse a Cache State Advertisement or Cache State Advertisement
 * Summary record
 *
 * Arguments:
 *	buff	pointer to CSA or CSAS record
 *	pdu_len	length of input packet
 *	csapp	pointer to location to put pointer to CSA or CSAS
 *
 * Returns:
 *	0	input was invalid
 *	else	length of record processed
 *
 */
static int
scsp_parse_csa(buff, pdu_len, csapp)
	char		*buff;
	int		pdu_len;
	Scsp_csa	**csapp;
{
	int			len;
	char			*idp;
	struct scsp_ncsa	*scp;
	Scsp_csa		*csap = NULL;

	/*
	 * Check the record length
	 */
	scp = (struct scsp_ncsa *)buff;
	if (ntohs(scp->scs_len) < (sizeof(struct scsp_ncsa) +
			scp->scs_ck_len + scp->scs_oid_len)) {
		goto csa_invalid;
	}

	/*
	 * Get memory for the returned structure
	 */
	len = sizeof(Scsp_csa) + ntohs(scp->scs_len) -
			sizeof(struct scsp_ncsa) - scp->scs_ck_len -
			scp->scs_oid_len;
	csap = (Scsp_csa *)UM_ALLOC(len);
	if (!csap) {
		goto csa_invalid;
	}
	UM_ZERO(csap, len);

	/*
	 * Get the hop count
	 */
	csap->hops = ntohs(scp->scs_hop_cnt);

	/*
	 * Set the null flag
	 */
	csap->null = (ntohs(scp->scs_nfill) & SCSP_CSAS_NULL) != 0;

	/*
	 * Get the sequence number
	 */
	csap->seq = get_long((u_char *)&scp->scs_seq);

	/*
	 * Get the cache key
	 */
	if (scp->scs_ck_len == 0 ||
			scp->scs_ck_len > SCSP_MAX_KEY_LEN) {
		goto csa_invalid;
	}
	csap->key.key_len = scp->scs_ck_len;
	idp = (char *) ((caddr_t)scp + sizeof(struct scsp_ncsa));
	UM_COPY(idp, csap->key.key, scp->scs_ck_len);

	/*
	 * Get the originator ID
	 */
	idp += scp->scs_ck_len;
	len = scsp_parse_id(idp, scp->scs_oid_len, &csap->oid);
	if (len == 0) {
		goto csa_invalid;
	}

	/*
	 * Get the protocol-specific data, if present
	 */
	len = ntohs(scp->scs_len) - (sizeof(struct scsp_ncsa) +
			scp->scs_ck_len + scp->scs_oid_len);
	if (len > 0) {
		idp += scp->scs_oid_len;
		len = scsp_parse_atmarp(idp, len, &csap->atmarp_data);
		if (len == 0)
			goto csa_invalid;
	}

	/*
	 * Set a pointer to the MCP and return the length
	 * of data we processed
	 */
	*csapp = csap;
	return(ntohs(scp->scs_len));

csa_invalid:
	if (csap)
		SCSP_FREE_CSA(csap);
	return(0);
}


/*
 * Parse a Cache Alignment message
 *
 * Arguments:
 *	buff	pointer to start of CA in message
 *	pdu_len	length of input packet
 *	capp	pointer to location to put pointer to CA message
 *
 * Returns:
 *	0	input was invalid
 *	else	length of CA message processed
 *
 */
static int
scsp_parse_ca(buff, pdu_len, capp)
	char	*buff;
	int	pdu_len;
	Scsp_ca	**capp;
{
	int		i, len, proc_len;
	struct scsp_nca	*scap;
	Scsp_ca		*cap;
	Scsp_csa	**csapp;

	/*
	 * Get memory for the returned structure
	 */
	scap = (struct scsp_nca *)buff;
	cap = (Scsp_ca *)UM_ALLOC(sizeof(Scsp_ca));
	if (!cap) {
		goto ca_invalid;
	}
	UM_ZERO(cap, sizeof(Scsp_ca));

	/*
	 * Get the sequence number
	 */
	cap->ca_seq = get_long((u_char *)&scap->sca_seq);
	proc_len = sizeof(scap->sca_seq);
	buff += sizeof(scap->sca_seq);

	/*
	 * Process the mandatory common part of the message
	 */
	len = scsp_parse_mcp(buff,
			pdu_len - proc_len,
			&cap->ca_mcp);
	if (len == 0)
		goto ca_invalid;
	buff += len;
	proc_len += len;

	/*
	 * Set the flags
	 */
	cap->ca_m = (cap->ca_mcp.flags & SCSP_CA_M) != 0;
	cap->ca_i = (cap->ca_mcp.flags & SCSP_CA_I) != 0;
	cap->ca_o = (cap->ca_mcp.flags & SCSP_CA_O) != 0;

	/*
	 * Get the CSAS records from the message
	 */
	for (i = 0, csapp = &cap->ca_csa_rec; i < cap->ca_mcp.rec_cnt;
			i++, csapp = &(*csapp)->next) {
		len = scsp_parse_csa(buff, pdu_len - proc_len, csapp);
		buff += len;
		proc_len += len;
	}

	/*
	 * Set the address of the CA message and
	 * return the length of processed data
	 */
	*capp = cap;
	return(proc_len);

ca_invalid:
	if (cap)
		scsp_free_ca(cap);
	return(0);
}


/*
 * Parse the ATMARP-specific part of a CSA record
 *
 * Arguments:
 *	buff	pointer to ATMARP part of CSU message
 *	pdu_len	length of data to process
 *	acspp	pointer to location to put pointer to CSU message
 *
 * Returns:
 *	0	input was invalid
 *	else	length of CSU Req message processed
 *
 */
static int
scsp_parse_atmarp(buff, pdu_len, acspp)
	char		*buff;
	int		pdu_len;
	Scsp_atmarp_csa	**acspp;
{
	int			len, proc_len;
	struct scsp_atmarp_ncsa	*sacp;
	Scsp_atmarp_csa		*acsp = NULL;

	/*
	 * Initial packet verification
	 */
	sacp = (struct scsp_atmarp_ncsa *)buff;
	if ((sacp->sa_hrd != ntohs(ARP_ATMFORUM)) ||
			(sacp->sa_pro != ntohs(ETHERTYPE_IP)))
		goto acs_invalid;

	/*
	 * Get memory for the returned structure
	 */
	acsp = (Scsp_atmarp_csa *)UM_ALLOC(sizeof(Scsp_atmarp_csa));
	if (!acsp) {
		goto acs_invalid;
	}
	UM_ZERO(acsp, sizeof(Scsp_atmarp_csa));

	/*
	 * Get state code
	 */
	acsp->sa_state = sacp->sa_state;
	proc_len = sizeof(struct scsp_atmarp_ncsa);

	/*
	 * Verify/gather source ATM address
	 */
	acsp->sa_sha.address_format = T_ATM_ABSENT;
	acsp->sa_sha.address_length = 0;
	if ((len = (sacp->sa_shtl & ARP_TL_LMASK)) != 0) {
		if (sacp->sa_shtl & ARP_TL_E164) {
			if (len > sizeof(Atm_addr_e164))
				goto acs_invalid;
			acsp->sa_sha.address_format = T_ATM_E164_ADDR;
		} else {
			if (len != sizeof(Atm_addr_nsap))
				goto acs_invalid;
			acsp->sa_sha.address_format = T_ATM_ENDSYS_ADDR;
		}
		acsp->sa_sha.address_length = len;
		if (pdu_len < proc_len + len)
			goto acs_invalid;
		UM_COPY(&buff[proc_len], (char *)acsp->sa_sha.address,
				len);
		proc_len += len;
	}

	/*
	 * Verify/gather source ATM subaddress
	 */
	acsp->sa_ssa.address_format = T_ATM_ABSENT;
	acsp->sa_ssa.address_length = 0;
	if ((len = (sacp->sa_sstl & ARP_TL_LMASK)) != 0) {
		if (((sacp->sa_sstl & ARP_TL_TMASK) != ARP_TL_NSAPA) ||
				(len != sizeof(Atm_addr_nsap)))
			goto acs_invalid;
		acsp->sa_ssa.address_format = T_ATM_ENDSYS_ADDR;
		acsp->sa_ssa.address_length = len;
		if (pdu_len < proc_len + len)
			goto acs_invalid;
		UM_COPY(&buff[proc_len], (char *)acsp->sa_ssa.address,
				len);
		proc_len += len;
	}

	/*
	 * Verify/gather source IP address
	 */
	if ((len = sacp->sa_spln) != 0) {
		if (len != sizeof(struct in_addr))
			goto acs_invalid;
		if (pdu_len < proc_len + len)
			goto acs_invalid;
		UM_COPY(&buff[proc_len], (char *)&acsp->sa_spa, len);
		proc_len += len;
	} else {
		acsp->sa_spa.s_addr = 0;
	}

	/*
	 * Verify/gather target ATM address
	 */
	acsp->sa_tha.address_format = T_ATM_ABSENT;
	acsp->sa_tha.address_length = 0;
	if ((len = (sacp->sa_thtl & ARP_TL_LMASK)) != 0) {
		if (sacp->sa_thtl & ARP_TL_E164) {
			if (len > sizeof(Atm_addr_e164))
				goto acs_invalid;
			acsp->sa_tha.address_format = T_ATM_E164_ADDR;
		} else {
			if (len != sizeof(Atm_addr_nsap))
				goto acs_invalid;
			acsp->sa_tha.address_format = T_ATM_ENDSYS_ADDR;
		}
		acsp->sa_tha.address_length = len;
		if (pdu_len < proc_len + len)
			goto acs_invalid;
		UM_COPY(&buff[proc_len], (char *)acsp->sa_tha.address,
				len);
		proc_len += len;
	}

	/*
	 * Verify/gather target ATM subaddress
	 */
	acsp->sa_tsa.address_format = T_ATM_ABSENT;
	acsp->sa_tsa.address_length = 0;
	if ((len = (sacp->sa_tstl & ARP_TL_LMASK)) != 0) {
		if (((sacp->sa_tstl & ARP_TL_TMASK) != ARP_TL_NSAPA) ||
				(len != sizeof(Atm_addr_nsap)))
			goto acs_invalid;
		acsp->sa_tsa.address_format = T_ATM_ENDSYS_ADDR;
		acsp->sa_tsa.address_length = len;
		if (pdu_len < proc_len + len)
			goto acs_invalid;
		UM_COPY(&buff[proc_len], (char *)acsp->sa_tsa.address,
				len);
		proc_len += len;
	}

	/*
	 * Verify/gather target IP address
	 */
	if ((len = sacp->sa_tpln) != 0) {
		if (len != sizeof(struct in_addr))
			goto acs_invalid;
		if (pdu_len < proc_len + len)
			goto acs_invalid;
		UM_COPY(&buff[proc_len], (char *)&acsp->sa_tpa, len);
		proc_len += len;
	} else {
		acsp->sa_tpa.s_addr = 0;
	}

	/*
	 * Verify packet length
	 */
	if (proc_len != pdu_len)
		goto acs_invalid;

	*acspp = acsp;
	return(proc_len);

acs_invalid:
	if (acsp)
		UM_FREE(acsp);
	return(0);
}


/*
 * Parse a Cache State Update Request, Cache State Update Reply, or
 * Cache State Update Solicit message.  These all have the same format,
 * a Mandatory Common Part followed by a number of CSA or CSAS records.
 *
 * Arguments:
 *	buff	pointer to start of CSU message
 *	pdu_len	length of input packet
 *	csupp	pointer to location to put pointer to CSU message
 *
 * Returns:
 *	0	input was invalid
 *	else	length of CSU Req message processed
 *
 */
static int
scsp_parse_csu(buff, pdu_len, csupp)
	char		*buff;
	int		pdu_len;
	Scsp_csu_msg	**csupp;
{
	int			i, len, proc_len;
	Scsp_csu_msg		*csup;
	Scsp_csa		**csapp;

	/*
	 * Get memory for the returned structure
	 */
	csup = (Scsp_csu_msg *)UM_ALLOC(sizeof(Scsp_csu_msg));
	if (!csup) {
		goto csu_invalid;
	}
	UM_ZERO(csup, sizeof(Scsp_csu_msg));

	/*
	 * Process the mandatory common part of the message
	 */
	len = scsp_parse_mcp(buff, pdu_len, &csup->csu_mcp);
	if (len == 0)
		goto csu_invalid;
	buff += len;
	proc_len = len;

	/*
	 * Get the CSAS records from the message
	 */
	for (i = 0, csapp = &csup->csu_csa_rec;
			i < csup->csu_mcp.rec_cnt;
			i++, csapp = &(*csapp)->next) {
		len = scsp_parse_csa(buff, pdu_len - proc_len, csapp);
		buff += len;
		proc_len += len;
	}

	/*
	 * Set the address of the CSU Req message and
	 * return the length of processed data
	 */
	*csupp = csup;
	return(proc_len);

csu_invalid:
	if (csup)
		scsp_free_csu(csup);
	return(0);
}


/*
 * Parse a Hello message
 *
 * Arguments:
 *	buff	pointer to start of Hello in message
 *	pdu_len	length of input packet
 *	hpp	pointer to location to put pointer to Hello message
 *
 * Returns:
 *	0	input was invalid
 *	else	length of Hello message processed
 *
 */
static int
scsp_parse_hello(buff, pdu_len, hpp)
	char		*buff;
	int		pdu_len;
	Scsp_hello	**hpp;
{
	int			i, len, proc_len;
	struct scsp_nhello	*shp = (struct scsp_nhello *)buff;
	Scsp_hello		*hp;
	Scsp_id			*idp;
	Scsp_id			**ridpp;

	/*
	 * Get memory for the returned structure
	 */
	hp = (Scsp_hello *)UM_ALLOC(sizeof(Scsp_hello));
	if (!hp) {
		goto hello_invalid;
	}
	UM_ZERO(hp, sizeof(Scsp_hello));

	/*
	 * Get the hello interval
	 */
	hp->hello_int = ntohs(shp->sch_hi);

	/*
	 * Get the dead factor
	 */
	hp->dead_factor = ntohs(shp->sch_df);

	/*
	 * Get the family ID
	 */
	hp->family_id = ntohs(shp->sch_fid);

	/*
	 * Process the mandatory common part of the message
	 */
	proc_len = sizeof(struct scsp_nhello) -
			sizeof(struct scsp_nmcp);
	buff += proc_len;
	len = scsp_parse_mcp(buff, pdu_len - proc_len,
			&hp->hello_mcp);
	if (len == 0)
		goto hello_invalid;
	buff += len;
	proc_len += len;

	/*
	 * Get additional receiver ID records from the message
	 */
	for (i = 0, ridpp = &hp->hello_mcp.rid.next;
			i < hp->hello_mcp.rec_cnt;
			i++, ridpp = &idp->next) {
		idp = (Scsp_id *)UM_ALLOC(sizeof(Scsp_id));
		if (!idp) {
			goto hello_invalid;
		}
		UM_ZERO(idp, sizeof(Scsp_id));
		len = scsp_parse_id(buff,
				hp->hello_mcp.rid.id_len,
				idp);
		if (len == 0) {
			UM_FREE(idp);
			goto hello_invalid;
		}
		buff += len;
		proc_len += len;
		*ridpp = idp;
	}

	/*
	 * Set the address of the CA message and
	 * return the length of processed data
	 */
	*hpp = hp;
	return(proc_len);

hello_invalid:
	if (hp)
		scsp_free_hello(hp);
	return(0);
}


/*
 * Parse an SCSP input packet
 *
 * Arguments:
 *	buff	pointer to input packet
 *	pdu_len	length of input packet
 *
 * Returns:
 *	NULL	input packet was invalid
 *	else	pointer to packet in internal format
 *
 */
Scsp_msg *
scsp_parse_msg(buff, pdu_len)
	char			*buff;
	int			pdu_len;
{
	int			ext_off, len, plen;
	struct scsp_nhdr	*shp;
	Scsp_msg		*msg = (Scsp_msg *)0;
	Scsp_ext		**expp;

	/*
	 * Check the message checksum
	 */
	if (ip_checksum(buff, pdu_len) != 0) {
		/*
		 * Checksum was bad--discard the message
		 */
		goto ignore;
	}

	/*
	 * Allocate storage for the message
	 */
	msg = (Scsp_msg *)UM_ALLOC(sizeof(Scsp_msg));
	if (!msg) {
		goto ignore;
	}
	UM_ZERO(msg, sizeof(Scsp_msg));

	/*
	 * Decode the fixed header
	 *
	 * Check the version
	 */
	shp = (struct scsp_nhdr *)buff;
	if (shp->sh_ver != SCSP_VER_1)
		goto ignore;

	/*
	 * Get the message type
	 */
	msg->sc_msg_type = shp->sh_type;

	/*
	 * Get and check the length
	 */
	len = ntohs(shp->sh_len);
	if (len != pdu_len)
		goto ignore;

	/*
	 * Get the extension offset
	 */
	ext_off = ntohs(shp->sh_ext_off);

	/*
	 * Decode the body of the message, depending on the type
	 */
	buff += sizeof(struct scsp_nhdr);
	len -= sizeof(struct scsp_nhdr);
	switch(msg->sc_msg_type) {
	case SCSP_CA_MSG:
		plen = scsp_parse_ca(buff, len, &msg->sc_ca);
		break;
	case SCSP_CSU_REQ_MSG:
	case SCSP_CSU_REPLY_MSG:
	case SCSP_CSUS_MSG:
		plen = scsp_parse_csu(buff, len, &msg->sc_csu_msg);
		break;
	case SCSP_HELLO_MSG:
		plen = scsp_parse_hello(buff, len, &msg->sc_hello);
		break;
	default:
		goto ignore;
	}
	if (plen == 0) {
		goto ignore;
	}
	buff += plen;
	len -= plen;

	/*
	 * Decode any extensions
	 */
	if (ext_off != 0) {
		for (expp = &msg->sc_ext; len > 0;
				expp = &(*expp)->next) {
			plen = scsp_parse_ext(buff, len, expp);
			if (plen == 0) {
				goto ignore;
			}
			buff += plen;
			len -= plen;
		}
	}

	/*
	 * Make sure we handled the whole message
	 */
	if (len != 0) {
		goto ignore;
	}

	/*
	 * Return the address of the SCSP message in internal format
	 */
	return(msg);

ignore:
	if (msg)
		scsp_free_msg(msg);
	return(Scsp_msg *)0;
}
