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
 * SPANS Signalling Manager
 * ---------------------------
 *
 * SPANS-related utility routines.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_sigmgr.h>

#include "spans_xdr.h"
#include <netatm/spans/spans_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


#ifdef NOTDEF
/* XXX -- Remove all SAP checks? */
#define	MAX_SAP_ENT	1
static struct {
	spans_sap	spans_sap;
	Sap_t	local_sap;
} sap_table[MAX_SAP_ENT] = {
	{SPANS_SAP_IP, SAP_IP},
};


/*
 * Translate an internal SAP to a SPANS SAP
 *
 * Search the SAP table for the given SAP.  Put the corresponding SPANS
 * SAP into the indicated variable.
 *
 * Arguments:
 *	lsap	the value of the internal SAP
 *	ssap	a pointer to the variable to receive the SPANS SAP value
 *
 * Returns:
 *	TRUE	the SAP was found; *ssap is valid
 *	FALSE	the SAP was not found; *ssap is not valid
 *
 */
int
spans_get_spans_sap(lsap, ssap)
	Sap_t	lsap;
	spans_sap	*ssap;
{
	int i;

	/*
	 * Search the SAP table for the given local SAP
	 */
	for (i=0; i< MAX_SAP_ENT; i++) {
		if (sap_table[i].local_sap == lsap) {
			*ssap = sap_table[i].spans_sap;
			return(TRUE);
		}
	}
	return(FALSE);
}


/*
 * Translate a SPANS SAP to internal format
 *
 * Search the SAP table for the given SAP.  Put the corresponding
 * internal SAP into the indicated variable.
 *
 * Arguments:
 *	ssap	the value of the SPANS SAP
 *	lsap	a pointer to the variable to receive the internal
 *		SAP value
 *
 * Returns:
 *	TRUE	the SAP was found; *lsap is valid
 *	FALSE	the SAP was not found; *lsap is not valid
 *
 */
int
spans_get_local_sap(ssap, lsap)
	spans_sap	ssap;
	Sap_t	*lsap;
{
	int i;

	/*
	 * Search the SAP table for the given SPANS SAP
	 */
	for (i=0; i< MAX_SAP_ENT; i++) {
		if (sap_table[i].spans_sap == ssap) {
			*lsap = sap_table[i].local_sap;
			return(TRUE);
		}
	}
	return(FALSE);
}
#endif


/*
 * Allocate an ephemeral SPANS SAP
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance
 *
 * Returns:
 *	a SPANS ephemeral SAP number
 *
 */
int
spans_ephemeral_sap(spp)
	struct spans	*spp;
{
	return(SPANS_SAP_EPHEMERAL);
}


/*
 * Translate an internal AAL designator to a SPANS AAL type
 *
 * Arguments:
 *	laal	internal AAL designation
 *	saal	a pointer to the variable to receive the SPANS AAL type
 *
 * Returns:
 *	TRUE	the AAL was found; *saal is valid
 *	FALSE	the AAL was not found; *saal is not valid
 *
 */
int
spans_get_spans_aal(laal, saal)
	Aal_t		laal;
	spans_aal	*saal;
{
	/*
	 *
	 */
	switch (laal) {
	case ATM_AAL0:
		*saal = SPANS_AAL0;
		return(TRUE);
	case ATM_AAL1:
		*saal = SPANS_AAL1;
		return(TRUE);
	case ATM_AAL2:
		*saal = SPANS_AAL2;
		return(TRUE);
	case ATM_AAL3_4:
		*saal = SPANS_AAL4;
		return(TRUE);
	case ATM_AAL5:
		*saal = SPANS_AAL5;
		return(TRUE);
	default:
		return(FALSE);
	}
}


/*
 * Translate a SPANS AAL type to an internal AAL designator
 *
 * Arguments:
 *	saal	the SPANS AAL type
 *	laal	a pointer to the variable to receive the internal
 *		AAL designation
 *
 * Returns:
 *	TRUE	the AAL was found; *laal is valid
 *	FALSE	the AAL was not found; *laal is not valid
 *
 */
int
spans_get_local_aal(saal, laal)
	spans_aal	saal;
	Aal_t		*laal;
{
	/*
	 *
	 */
	switch (saal) {
	case SPANS_AAL0:
		*laal = ATM_AAL0;
		return(TRUE);
	case SPANS_AAL1:
		*laal = ATM_AAL1;
		return(TRUE);
	case SPANS_AAL2:
		*laal = ATM_AAL2;
		return(TRUE);
	case SPANS_AAL3:
	case SPANS_AAL4:
		*laal = ATM_AAL3_4;
		return(TRUE);
	case SPANS_AAL5:
		*laal = ATM_AAL5;
		return(TRUE);
	default:
		return(FALSE);
	}
}


/*
 * Verify a VCCB
 *
 * Search SPANS's VCCB queue to verify that a VCCB belongs to SPANS.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance
 *	svp	pointer to a VCCB
 *
 * Returns:
 *	TRUE	the VCCB belongs to SPANS
 *	FALSE	the VCCB doesn't belong to SPANS
 *
 */
int
spans_verify_vccb(spp, svp)
	struct spans		*spp;
	struct spans_vccb	*svp;

{
	struct spans_vccb	*vcp, *vcnext;

	for (vcp = Q_HEAD(spp->sp_vccq, struct spans_vccb);
			vcp; vcp = vcnext){
		vcnext = Q_NEXT(vcp, struct spans_vccb, sv_sigelem);
		if (svp == vcp) {
			return(TRUE);
		}
	}
	return(FALSE);
}


/*
 * Find a VCCB
 *
 * Find a VCCB given the VPI and VCI.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance
 *	vpi	the VPI to search for
 *	vci	the VCI to search for
 *	dir	the direction of the VCC (VCC_IN, VCC_OUT, or both).
 *		If dir is set to zero, return the address of any VCCB
 *		with the given VPI/VCI, regardless of direction.
 *
 * Returns:
 *	0	there is no such VCCB
 *	address	the address of the VCCB
 *
 */
struct spans_vccb *
spans_find_vpvc(spp, vpi, vci, dir)
	struct spans	*spp;
	int		vpi, vci;
	u_char		dir;

{
	struct spans_vccb	*svp, *svnext;

	for (svp = Q_HEAD(spp->sp_vccq, struct spans_vccb); svp;
			svp = svnext){
		svnext = Q_NEXT(svp, struct spans_vccb, sv_sigelem);
		if (svp->sv_vpi == vpi &&
				svp->sv_vci == vci &&
				(svp->sv_type & dir) == dir)
			break;
	}
	return(svp);
}


/*
 * Find a connection
 *
 * Find a VCCB given the connection structure.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance
 *	p	pointer to an spans_atm_conn structure
 *
 * Returns:
 *	0	there is no such VCCB
 *	address	the address of the VCCB
 *
 */
struct spans_vccb *
spans_find_conn(spp, p)
	struct spans		*spp;
	struct spans_atm_conn	*p;
{
	struct spans_vccb	*svp, *svnext;

	for (svp = Q_HEAD(spp->sp_vccq, struct spans_vccb); svp; svp = svnext){
		svnext = Q_NEXT(svp, struct spans_vccb, sv_sigelem);
		if (!bcmp(p, &svp->sv_conn, sizeof (spans_atm_conn)))
			break;
	}
	return(svp);
}


/*
 * Allocate a VPI/VCI pair
 *
 * When we get an open request or indication from the network, we have
 * allocate a VPI and VCI for the conection.  This routine will allocate
 * a VPI/VCI based on the next available VCI in the SPANS protocol block.
 * The VPI/VCI chose must be within the range allowed by the interface and
 * must not already be in use.
 *
 * Currently the Fore ATM interface only supports VPI 0, so this code only
 * allocates a VCI.
 *
 * There's probably a more elegant way to do this.
 *
 * Arguments:
 *	spp	pointer to connection's SPANS protocol instance
 *
 * Returns:
 *	0	no VPI/VCI available
 *	vpvc	the VPI/VCI for the connection
 *
 */
spans_vpvc
spans_alloc_vpvc(spp)
	struct spans		*spp;
{
	int	vpi, vci;

	/*
	 * Loop through the allowable VCIs, starting with the curent one,
	 * to find one that's not in use.
	 */
	while (spp->sp_alloc_vci <= spp->sp_max_vci) {
		vpi = spp->sp_alloc_vpi;
		vci = spp->sp_alloc_vci++;
		if (!spans_find_vpvc(spp, vpi, vci, 0)) {
			return(SPANS_PACK_VPIVCI(vpi, vci));
		}
	}

	/*
	 * Reset the VCI to the minimum
	 */
	spp->sp_alloc_vci = spp->sp_min_vci;

	/*
	 * Try looping through again
	 */
	while (spp->sp_alloc_vci <= spp->sp_max_vci) {
		vpi = spp->sp_alloc_vpi;
		vci = spp->sp_alloc_vci++;
		if (!spans_find_vpvc(spp, vpi, vci, 0)) {
			return(SPANS_PACK_VPIVCI(vpi, vci));
		}
	}

	/*
	 * All allowable VCIs are in use
	 */
	return(0);
}


/*
 * Print a SPANS address
 *
 * Convert a SPANS address into an ASCII string suitable for printing.
 *
 * Arguments:
 *	p	pointer to a struct spans_addr
 *
 * Returns:
 *	the address of a string with the ASCII representation of the
 *	address.
 *
 */
char *
spans_addr_print(p)
	struct spans_addr	*p;
{
	static char	strbuff[80];
	union {
		int	w;
		char	c[4];
	} u1, u2;


	/*
	 * Clear the returned string
	 */
	KM_ZERO(strbuff, sizeof(strbuff));

	/*
	 * Get address into integers
	 */
	u1.c[0] =p->addr[0];
	u1.c[1] =p->addr[1];
	u1.c[2] =p->addr[2];
	u1.c[3] =p->addr[3];
	u2.c[0] =p->addr[4];
	u2.c[1] =p->addr[5];
	u2.c[2] =p->addr[6];
	u2.c[3] =p->addr[7];

	/*
	 * Print and return the string
	 */
	sprintf(strbuff, "%lx.%lx", (u_long)ntohl(u1.w), (u_long)ntohl(u2.w));
	return(strbuff);
}


/*
 * Print a buffer chain
 *
 * Arguments:
 *	m	pointer to a buffer chain
 *
 * Returns:
 *	none
 *
 */
void
spans_dump_buffer(m)
	KBuffer		*m;
{
	int		i;
	caddr_t		cp;

	printf("spans_dump_buffer:\n");
	while (m) {
		KB_DATASTART(m, cp, caddr_t);
		for (i = 0; i < KB_LEN(m); i++) {
			if (i == 0)
				printf("   bfr=%p: ", m);
			printf("%x ", (u_char)*cp++);
		}
		printf("<end_bfr>\n");
		m = KB_NEXT(m);
	}
}
