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
 *	@(#) $FreeBSD: src/usr.sbin/atm/atmarpd/atmarp_subr.c,v 1.3 1999/08/28 01:15:30 peter Exp $
 *
 */


/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * SCSP-ATMARP server interface: misc. subroutines
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
#include <netatm/uni/unisig_var.h>
#include <netatm/uni/uniip_var.h>
 
#include <errno.h>
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
__RCSID("@(#) $FreeBSD: src/usr.sbin/atm/atmarpd/atmarp_subr.c,v 1.3 1999/08/28 01:15:30 peter Exp $");
#endif


/*
 * Find an ATMARP interface, given its socket number
 *
 * Arguments:
 *	sd	socket descriptor
 *
 * Returns:
 *	0	failure
 *	else	pointer to interface associated with socket
 *
 */
Atmarp_intf *
atmarp_find_intf_sock(sd)
	int	sd;
{
	Atmarp_intf	*aip;

	/*
	 * Loop through the list of interfaces
	 */
	for (aip = atmarp_intf_head; aip; aip = aip->ai_next) {
		if (aip->ai_scsp_sock == sd)
			break;
	}

	return(aip);
}


/*
 * Find an ATMARP interface, given its name
 *
 * Arguments:
 *	name	pointer to network interface name
 *
 * Returns:
 *	0	failure
 *	else	pointer to interface associated with name
 *
 */
Atmarp_intf *
atmarp_find_intf_name(name)
	char	*name;
{
	Atmarp_intf	*aip;

	/*
	 * Loop through the list of interfaces
	 */
	for (aip = atmarp_intf_head; aip; aip = aip->ai_next) {
		if (strcmp(name, aip->ai_intf) == 0)
			break;
	}

	return(aip);
}


/*
 * Clear the mark field on all ATMARP cache entries
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
void
atmarp_clear_marks()

{
	int		i;
	Atmarp_intf	*aip;
	Atmarp		*aap;

	/*
	 * Loop through list of interfaces
	 */
	for (aip = atmarp_intf_head; aip; aip = aip->ai_next) {
		/*
		 * Clear mark on every entry in the interface's cache
		 */
		for (i = 0; i < ATMARP_HASHSIZ; i++ ) {
			for (aap = aip->ai_arptbl[i]; aap;
					aap = aap->aa_next) {
				aap->aa_mark = 0;
			}
		}
	}
}


/*
 * Check whether the host system is an ATMARP server for
 * the LIS associated with a given interface
 *
 * Arguments:
 *	aip	pointer to an ATMARP interface control block
 *
 * Returns:
 *	1	host is a server
 *	0	host is not a server
 *
 */
int
atmarp_is_server(aip)
	Atmarp_intf	*aip;
{
	int			rc;
	int			buf_len = sizeof(struct air_asrv_rsp);
	struct atminfreq	air;
	struct air_asrv_rsp	*asrv_info;

	/*
	 * Get interface information from the kernel
	 */
	strcpy(air.air_int_intf, aip->ai_intf);
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
 * Check whether an interface is up and ready for service
 *
 * Arguments:
 *	aip	pointer to network interface block
 *
 * Returns:
 *	0	interface not ready, errno has reason
 *	1	interface is ready to go (interface block is updated)
 *
 */
int
atmarp_if_ready(aip)
	Atmarp_intf	*aip;
{
	int			i, len, mtu, rc, sel;
	Atmarp			*aap = (Atmarp *)0;
	struct atminfreq	air;
	struct air_netif_rsp	*netif_rsp = (struct air_netif_rsp *)0;
	struct air_int_rsp	*intf_rsp = (struct air_int_rsp *)0;
	struct sockaddr_in	*ip_addr;
	struct sockaddr_in	subnet_mask;
	Atm_addr_nsap		*anp;

	/*
	 * Get the IP address and physical interface name
	 * associated with the network interface
	 */
	UM_ZERO(&air, sizeof(struct atminfreq));
	air.air_opcode = AIOCS_INF_NIF;
	strcpy(air.air_netif_intf, aip->ai_intf);
	len = do_info_ioctl(&air, sizeof(struct air_netif_rsp));
	if (len <= 0) {
		goto if_ready_fail;
	}
	netif_rsp = (struct air_netif_rsp *)air.air_buf_addr;

	ip_addr = (struct sockaddr_in *)&netif_rsp->anp_proto_addr;
	if (ip_addr->sin_family != AF_INET ||
			ip_addr->sin_addr.s_addr == 0) {
		errno = EAFNOSUPPORT;
		goto if_ready_fail;
	}

	/*
	 * Get the MTU for the network interface
	 */
	mtu = get_mtu(aip->ai_intf);
	if (mtu < 0) {
		goto if_ready_fail;
	}


	/*
	 * Get the subnet mask associated with the
	 * network interface
	 */
	rc = get_subnet_mask(aip->ai_intf, &subnet_mask);
	if (rc || subnet_mask.sin_family != AF_INET) {
		goto if_ready_fail;
	}

	/*
	 * Get physical interface information
	 */
	UM_ZERO(&air, sizeof(struct atminfreq));
	air.air_opcode = AIOCS_INF_INT;
	strcpy(air.air_int_intf, netif_rsp->anp_phy_intf);
	len = do_info_ioctl(&air, sizeof(struct air_int_rsp));
	if (len <= 0) {
		goto if_ready_fail;
	}
	intf_rsp = (struct air_int_rsp *)air.air_buf_addr;

	/*
	 * Check the signalling manager
	 */
	if (intf_rsp->anp_sig_proto != ATM_SIG_UNI30 &&
			intf_rsp->anp_sig_proto != ATM_SIG_UNI31 &&
			intf_rsp->anp_sig_proto != ATM_SIG_UNI40) {
		errno = EINVAL;
		goto if_ready_fail;
	}

	/*
	 * Check the interface state
	 */
	if (intf_rsp->anp_sig_state != UNISIG_ACTIVE) {
		errno = EINVAL;
		goto if_ready_fail;
	}

	/*
	 * Check the address format
	 */
	if (intf_rsp->anp_addr.address_format != T_ATM_ENDSYS_ADDR &&
			!(intf_rsp->anp_addr.address_format ==
				T_ATM_E164_ADDR &&
			intf_rsp->anp_subaddr.address_format ==
				T_ATM_ENDSYS_ADDR)) {
		errno = EINVAL;
		goto if_ready_fail;
	}

	/*
	 * Find the selector byte value for the interface
	 */
	for (i=0; i<strlen(aip->ai_intf); i++) {
		if (aip->ai_intf[i] >= '0' &&
				aip->ai_intf[i] <= '9')
			break;
	}
	sel = atoi(&aip->ai_intf[i]);

	/*
	 * Make sure we're the server for this interface's LIS
	 */
	if (!atmarp_is_server(aip)) {
		rc = EINVAL;
		goto if_ready_fail;
	}

	/*
	 * If we already have the interface active and the address
	 * hasn't changed, return
	 */
	if (aip->ai_state != AI_STATE_NULL &&
			bcmp((caddr_t) &((struct sockaddr_in *)
				&netif_rsp->anp_proto_addr)->sin_addr,
				(caddr_t)&aip->ai_ip_addr,
				sizeof(aip->ai_ip_addr)) == 0 &&
			ATM_ADDR_EQUAL(&intf_rsp->anp_addr,
				&aip->ai_atm_addr) &&
			ATM_ADDR_EQUAL(&intf_rsp->anp_subaddr,
				&aip->ai_atm_subaddr)) {
		return(1);
	}

	/*
	 * Delete any existing ATMARP cache entry for this interface
	 */
	ATMARP_LOOKUP(aip, aip->ai_ip_addr.s_addr, aap);
	if (aap) {
		ATMARP_DELETE(aip, aap);
		UM_FREE(aap);
	}

	/*
	 * Update the interface entry
	 */
	aip->ai_ip_addr = ((struct sockaddr_in *)
			&netif_rsp->anp_proto_addr)->sin_addr;
	aip->ai_subnet_mask = subnet_mask.sin_addr;
	aip->ai_mtu = mtu + 8;
	ATM_ADDR_COPY(&intf_rsp->anp_addr,
			&aip->ai_atm_addr);
	ATM_ADDR_COPY(&intf_rsp->anp_subaddr,
			&aip->ai_atm_subaddr);
	anp = (Atm_addr_nsap *)aip->ai_atm_addr.address;
	if (aip->ai_atm_addr.address_format == T_ATM_ENDSYS_ADDR) {
		anp->aan_sel = sel;
	} else if (aip->ai_atm_addr.address_format ==
				T_ATM_E164_ADDR &&
			aip->ai_atm_subaddr.address_format ==
				T_ATM_ENDSYS_ADDR) {
		anp->aan_sel = sel;
	}

	/*
	 * Get a new ATMARP cache for the interface
	 */
	aap = (Atmarp *)UM_ALLOC(sizeof(Atmarp));
	if (!aap) {
		atmarp_mem_err("atmarp_if_ready: sizeof(Atmarp)");
	}
	UM_ZERO(aap, sizeof(Atmarp));
	
	/*
	 * Fill out the entry
	 */
	aap->aa_dstip = aip->ai_ip_addr;
	ATM_ADDR_COPY(&intf_rsp->anp_addr, &aap->aa_dstatm);
	ATM_ADDR_COPY(&intf_rsp->anp_subaddr,
			&aap->aa_dstatmsub);
	aap->aa_key.key_len = SCSP_ATMARP_KEY_LEN;
	scsp_cache_key(&aap->aa_dstatm, &aap->aa_dstip,
			SCSP_ATMARP_KEY_LEN, aap->aa_key.key);
	aap->aa_oid.id_len = SCSP_ATMARP_ID_LEN;
	aap->aa_seq = SCSP_CSA_SEQ_MIN;
	UM_COPY(&aap->aa_dstip.s_addr, aap->aa_oid.id,
			SCSP_ATMARP_ID_LEN);
	aap->aa_intf = aip;
	aap->aa_flags = AAF_SERVER;
	aap->aa_origin = UAO_LOCAL;

	/*
	 * Add the entry to the cache
	 */
	ATMARP_ADD(aip, aap);
	
	/*
	 * Free dynamic data
	 */
	UM_FREE(netif_rsp);
	UM_FREE(intf_rsp);

	return(1);

if_ready_fail:
	if (netif_rsp)
		UM_FREE(netif_rsp);
	if (intf_rsp)
		UM_FREE(intf_rsp);

	return(0);
}


/*
 * Copy an ATMARP cache entry from kernel format into an entry
 * suitable for our cache
 *
 * Arguments:
 *	cp	pointer to kernel entry
 *
 * Returns:
 *	pointer to a new cache entry
 *	0	error
 *
 */
Atmarp *
atmarp_copy_cache_entry(cp)
	struct air_arp_rsp	*cp;

{
	struct sockaddr_in	*ipp;
	Atmarp_intf		*aip;
	Atmarp			*aap;

	/*
	 * Sanity checks
	 */
	if (!cp)
		return((Atmarp *)0);
	aip = atmarp_find_intf_name(cp->aap_intf);
	if (!aip)
		return((Atmarp *)0);

	/*
	 * Get a new cache entry
	 */
	aap = (Atmarp *)UM_ALLOC(sizeof(Atmarp));
	if (!aap) {
		errno = ENOMEM;
		return((Atmarp *)0);
	}
	UM_ZERO(aap, sizeof(Atmarp));
	aap->aa_intf = aip;

	/*
	 * Copy fields from the kernel entry to the new entry
	 */
	ipp = (struct sockaddr_in *)&cp->aap_arp_addr;
	UM_COPY(&ipp->sin_addr.s_addr, &aap->aa_dstip.s_addr,
			sizeof(aap->aa_dstip.s_addr));
        ATM_ADDR_COPY(&cp->aap_addr, &aap->aa_dstatm);
        ATM_ADDR_COPY(&cp->aap_subaddr, &aap->aa_dstatmsub);
	if (cp->aap_origin == UAO_PERM)
		aap->aa_flags |= AAF_PERM;
	aap->aa_origin = cp->aap_origin;

	/*
	 * Set up fields for SCSP
	 */
	aap->aa_key.key_len = SCSP_ATMARP_KEY_LEN;
	scsp_cache_key(&cp->aap_addr, &aap->aa_dstip,
			SCSP_ATMARP_KEY_LEN, (char *)aap->aa_key.key);
	aap->aa_oid.id_len = SCSP_ATMARP_ID_LEN;
	UM_COPY(&aip->ai_ip_addr.s_addr, aap->aa_oid.id,
			SCSP_ATMARP_ID_LEN);
	aap->aa_seq = SCSP_CSA_SEQ_MIN;

	return(aap);
}


/*
 * Send an updated ATMARP cache entry to the kernel
 *
 * Arguments:
 *	aap	pointer to updated entry
 *
 * Returns:
 *	0	success
 *	errno	reason for failure
 *
 */
int
atmarp_update_kernel(aap)
	Atmarp	*aap;
{
	int			rc = 0, sd;
	struct atmaddreq	aar;
	struct sockaddr_in	*ipp;

	/*
	 * Build ioctl request
	 */
	UM_ZERO(&aar, sizeof(aar));
	aar.aar_opcode = AIOCS_ADD_ARP;
	strncpy(aar.aar_arp_intf, aap->aa_intf->ai_intf,
			sizeof(aar.aar_arp_intf));
	aar.aar_arp_origin = UAO_SCSP;
	ATM_ADDR_COPY(&aap->aa_dstatm, &aar.aar_arp_addr);
	ipp = (struct sockaddr_in *)&aar.aar_arp_dst;
	ipp->sin_family = AF_INET;
#if (defined(BSD) && (BSD >= 199103))
	ipp->sin_len = sizeof(struct sockaddr_in);
#endif
	ipp->sin_addr = aap->aa_dstip;
	
	/*
	 * Pass the new mapping to the kernel
	 */
	sd = socket(AF_ATM, SOCK_DGRAM, 0);
	if (sd < 0) {
		return(errno);
	}
	if (ioctl(sd, AIOCADD, (caddr_t)&aar) < 0) {
		rc = errno;
	}

	(void)close(sd);
	return(rc);
}


/*
 * Read the ATMARP cache from the kernel and scan it, processing
 * all entries
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
void
atmarp_get_updated_cache()
{
	int			i, len, rc;
	struct atminfreq	air;
	struct air_arp_rsp	*cp;
	struct sockaddr_in	*ipp;
	Atmarp_intf		*aip;
	Atmarp			*aap;

	/*
	 * Set up the request
	 */
	air.air_opcode = AIOCS_INF_ARP;
	air.air_arp_flags = ARP_RESET_REF;
	ipp = (struct sockaddr_in *)&air.air_arp_addr;
#if (defined(BSD) && (BSD >= 199103))
	ipp->sin_len = sizeof(struct sockaddr_in);
#endif
	ipp->sin_family = AF_INET;
	ipp->sin_addr.s_addr = INADDR_ANY;

	/*
	 * Issue an ATMARP information request IOCTL
	 */
	len = do_info_ioctl(&air, sizeof(struct air_arp_rsp) * 200);
	if (len < 0) {
		return;
	}

	/*
	 * Clear marks on all our cache entries
	 */
	atmarp_clear_marks();

	/*
	 * Loop through the cache, processing each entry
	 */
	for (cp = (struct air_arp_rsp *) air.air_buf_addr;
			len > 0;
			cp++, len -= sizeof(struct air_arp_rsp)) {
		atmarp_process_cache_entry(cp);
	}

	/*
	 * Now delete any old entries that aren't in the kernel's
	 * cache any more
	 */
	for (aip = atmarp_intf_head; aip; aip = aip->ai_next) {
		for (i = 0; i < ATMARP_HASHSIZ; i++) {
			for (aap = aip->ai_arptbl[i]; aap;
					aap = aap->aa_next) {
				/*
				 * Don't delete the entry for the server
				 */
				if (aap->aa_flags & AAF_SERVER)
					continue;
				/*
				 * Delete any entry that isn't marked
				 */
				if (!aap->aa_mark) {
					rc = atmarp_scsp_update(aap,
						SCSP_ASTATE_DEL);
					if (rc == 0)
						ATMARP_DELETE(aip, aap);
				}
			}
		}
	}

	/*
	 * Free the ioctl response
	 */
	UM_FREE(air.air_buf_addr);
}


/*
 * Process an ATMARP cache entry from the kernel.  If we already
 * have the entry in our local cache, update it, otherwise, add
 * it.  In either case, mark our local copy so we know it's still
 * in the kernel's cache.
 *
 * Arguments:
 *	cp	pointer to kernel's cache entry
 *
 * Returns:
 *	none
 *
 */
void
atmarp_process_cache_entry(cp)
	struct air_arp_rsp	*cp;

{
	int			rc;
	struct sockaddr_in	*ipp = (struct sockaddr_in *)&cp->aap_arp_addr;
	Atmarp_intf		*aip;
	Atmarp			*aap;

	/*
	 * See whether the entry is for an interface that's
	 * both configured and up
	 */
	aip = atmarp_find_intf_name(cp->aap_intf);
	if (!aip || aip->ai_state != AI_STATE_UP)
		return;

	/*
	 * Make sure the entry is valid
	 */
	if (!(cp->aap_flags & ARPF_VALID))
		return;

	/*
	 * See whether we have the entry in our cache already
	 */
	ATMARP_LOOKUP(aip, ipp->sin_addr.s_addr, aap);
	if (aap) {
		/*
		 * We already have this in our cache--update it
		 */
		aap->aa_mark = 1;
		if ((cp->aap_flags & ARPF_REFRESH) &&
				cp->aap_origin != UAO_SCSP) {
			aap->aa_seq++;
			rc = atmarp_scsp_update(aap, SCSP_ASTATE_UPD);
		}
	} else {
		/*
		 * This is a new entry--add it to the cache
		 */
		aap = atmarp_copy_cache_entry(cp);
		if (!aap)
			return;
		ATMARP_ADD(aip, aap);
		aap->aa_mark = 1;
		rc = atmarp_scsp_update(aap, SCSP_ASTATE_NEW);
	}

	return;
}


/*
 * Print an SCSP ID
 *
 * Arguments:
 *	df	pointer to a FILE for the dump
 *	ip	pointer to the SCSP ID to print
 *
 * Returns:
 *	None
 *
 */
static void
print_scsp_id(df, ip)
	FILE	*df;
	Scsp_id	*ip;
{
	int	i;

	fprintf(df, "\t  next:            %p\n", ip->next);
	fprintf(df, "\t  id_len:          %d\n", ip->id_len);
	fprintf(df, "\t  id:              0x");
	for (i = 0; i < ip->id_len; i++) {
		fprintf(df, "%0x ", ip->id[i]);
	}
	fprintf(df, "\n");
}


/*
 * Print an SCSP cacke key
 *
 * Arguments:
 *	df	pointer to a FILE for the dump
 *	cp	pointer to the cacke key to print
 *
 * Returns:
 *	None
 *
 */
static void
print_scsp_cache_key(df, cp)
	FILE		*df;
	Scsp_ckey	*cp;
{
	int	i;

	fprintf(df, "\t  key_len:         %d\n", cp->key_len);
	fprintf(df, "\t  key:             0x");
	for (i = 0; i < cp->key_len; i++) {
		fprintf(df, "%0x ", cp->key[i]);
	}
	fprintf(df, "\n");
}


/*
 * Print an ATMARP interface entry
 *
 * Arguments:
 *	df	pointer to a FILE for the dump
 *	aip	pointer to interface entry
 *
 * Returns:
 *	None
 *
 */
void
print_atmarp_intf(df, aip)
	FILE		*df;
	Atmarp_intf	*aip;
{
	if (!aip) {
		fprintf(df, "print_atmarp_intf: NULL interface entry address\n");
		return;
	}

	fprintf(df, "ATMARP network interface entry at %p\n", aip);
	fprintf(df, "\tai_next:           %p\n", aip->ai_next);
	fprintf(df, "\tai_intf:           %s\n", aip->ai_intf);
	fprintf(df, "\tai_ip_addr:        %s\n",
			format_ip_addr(&aip->ai_ip_addr));
	fprintf(df, "\tai_subnet_mask:    %s\n",
			inet_ntoa(aip->ai_subnet_mask));
	fprintf(df, "\tai_mtu:            %d\n", aip->ai_mtu);
	fprintf(df, "\tai_atm_addr:       %s\n",
			format_atm_addr(&aip->ai_atm_addr));
	fprintf(df, "\tai_atm_subaddr:    %s\n",
			format_atm_addr(&aip->ai_atm_subaddr));
	fprintf(df, "\tai_scsp_sock:      %d\n", aip->ai_scsp_sock);
	fprintf(df, "\tai_scsp_sockname:  %s\n", aip->ai_scsp_sockname);
	fprintf(df, "\tai_state:          %d\n", aip->ai_state);
	fprintf(df, "\tai_mark:           %d\n", aip->ai_mark);
}


/*
 * Print an ATMARP cache entry
 *
 * Arguments:
 *	df	pointer to a FILE for the dump
 *	aap	pointer to cache entry
 *
 * Returns:
 *	None
 *
 */
void
print_atmarp_cache(df, aap)
	FILE	*df;
	Atmarp	*aap;
{
	if (!aap) {
		fprintf(df, "print_atmarp_cache: NULL ATMARP entry address\n");
		return;
	}

	fprintf(df, "ATMARP entry at %p\n", aap);
	fprintf(df, "\taa_next:      %p\n", aap->aa_next);
	fprintf(df, "\taa_dstip:     %s\n", inet_ntoa(aap->aa_dstip));
	fprintf(df, "\taa_dstatm:    %s\n",
			format_atm_addr(&aap->aa_dstatm));
	fprintf(df, "\taa_dstatmsub: %s\n",
			format_atm_addr(&aap->aa_dstatmsub));
	fprintf(df, "\taa_key:\n");
	print_scsp_cache_key(df, &aap->aa_key);
	fprintf(df, "\taa_oid:\n");
	print_scsp_id(df, &aap->aa_oid);
	fprintf(df, "\taa_seq:       %ld (0x%lx)\n", aap->aa_seq,
			aap->aa_seq);
	fprintf(df, "\taa_intf:      %p\n", aap->aa_intf);
	fprintf(df, "\taa_flags:     ");
	if (aap->aa_flags & AAF_PERM)
		fprintf(df, "Permanent ");
	if (aap->aa_flags & AAF_SERVER)
		fprintf(df, "Server ");
	fprintf(df, "\n");
	fprintf(df, "\taa_origin:    %d\n", aap->aa_origin);
	fprintf(df, "\taa_mark:      %d\n", aap->aa_mark);
}


/*
 * Print the entire ATMARP cache
 *
 * Arguments:
 *	df	pointer to a FILE for the dump
 *	aip	pointer to interface whose cache is to be printed
 *
 * Returns:
 *	None
 *
 */
void
dump_atmarp_cache(df, aip)
	FILE		*df;
	Atmarp_intf	*aip;
{
	int	i;
	Atmarp	*aap;

	if (!aip) {
		fprintf(df, "dump_atmarp_cache: NULL interface address\n");
		return;
	}

	fprintf(df, "ATMARP cache for interface %s\n", aip->ai_intf);
	for (i=0; i<ATMARP_HASHSIZ; i++) {
		for (aap=aip->ai_arptbl[i]; aap; aap=aap->aa_next) {
			print_atmarp_cache(df, aap);
		}
	}
}


#ifdef NOTDEF
/*
 * Print an ATMARP super-LIS entry
 *
 * Arguments:
 *	df	pointer to a FILE for the dump
 *	asp	pointer to super-LIS entry to be printed
 *
 * Returns:
 *	None
 *
 */
void
print_atmarp_slis(df, asp)
	FILE		*df;
	Atmarp_slis	*asp;
{
	Atmarp_intf	**aipp;

	if (!asp) {
		fprintf(df, "print_atmarp_slis: NULL SLIS address\n");
		return;
	}

	fprintf(df, "SLIS entry at 0x%0x\n", (u_long)asp);
	fprintf(df, "\tas_next:      0x%0x\n", (u_long)asp->as_next);
	fprintf(df, "\tas_name:      %s\n", asp->as_name);
	fprintf(df, "\tas_cnt:       %d\n", asp->as_cnt);
	for (aipp = &asp->as_intfs; *aipp; aipp++) {
		fprintf(df, "\t%s (%s)\n", (*aipp)->ai_name,
				(*aipp)->ai_intf);
	}
}
#endif


/*
 * Dump ATMARPD information
 *
 * Called as the result of a SIGINT signal.
 *
 * Arguments:
 *	sig	signal number
 *
 * Returns:
 *	None
 *
 */
void
atmarp_sigint(sig)
	int			sig;
{
	Atmarp_intf	*aip;
	FILE		*df;
	char		fname[64];
	static int	dump_no = 0;

	/*
	 * Build a file name
	 */
	UM_ZERO(fname, sizeof(fname));
	sprintf(fname, "/tmp/atmarpd.%d.%03d.out", getpid(), dump_no++);

	/*
	 * Open the output file
	 */
	df = fopen(fname, "w");
	if (df == (FILE *)0)
		return;

	/*
	 * Dump the interface control blocks and
	 * associated ATMARP caches
	 */
	for (aip = atmarp_intf_head; aip; aip = aip->ai_next) {
		print_atmarp_intf(df, aip);
		fprintf(df, "\n");
		dump_atmarp_cache(df, aip);
		fprintf(df, "\n");
	}

	/*
	 * Close the output file
	 */
	(void)fclose(df);
}
