/*
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
 * ATM Forum UNI 3.0/3.1 Signalling Manager
 * ----------------------------------------
 *
 * Protocol processing module
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/uni/unisig_var.h>
#include <netatm/uni/unisig_msg.h>

/*
 * Free a UNISIG signalling message
 *
 * Free the passed message and any IEs that are attached to it
 *
 * Arguments:
 *	msg	pointer to UNISIG protocol instance
 *
 * Returns:
 *	none
 *
 */
void
unisig_free_msg(msg)
	struct unisig_msg	*msg;
{
	int			i;
	struct ie_generic	*ie, *ienxt;

	ATM_DEBUG1("unisig_free_msg: msg=%p\n", msg);

	/*
	 * First free all the IEs
	 */
	for (i=0; i<UNI_MSG_IE_CNT; i++) {
		ie = msg->msg_ie_vec[i];
		while (ie) {
			ienxt = ie->ie_next;
			uma_zfree(unisig_ie_zone, ie);
			ie = ienxt;
		}
	}

	/*
	 * Finally, free the message structure itself
	 */
	uma_zfree(unisig_msg_zone, msg);
}

/*
 * Verify a VCCB
 *
 * Search UNISIG's VCCB queue to verify that a VCCB belongs to UNISIG.
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance
 *	svp	pointer to a VCCB
 *
 * Returns:
 *	TRUE	the VCCB belongs to UNISIG
 *	FALSE	the VCCB doesn't belong to UNISIG
 *
 */
int
unisig_verify_vccb(usp, uvp)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;

{
	struct unisig_vccb	*utp, *uvnext;

	for (utp = Q_HEAD(usp->us_vccq, struct unisig_vccb);
			utp; utp = uvnext){
		uvnext = Q_NEXT(utp, struct unisig_vccb, uv_sigelem);
		if (uvp == utp) {
			return(TRUE);
		}
	}
	return(FALSE);
}


/*
 * Find a connection
 *
 * Find a VCCB given the call reference
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance
 *	cref	the call reference to search for
 *
 * Returns:
 *	0	there is no such VCCB
 *	uvp	the address of the VCCB
 *
 */
struct unisig_vccb *
unisig_find_conn(usp, cref)
	struct unisig	*usp;
	u_int		cref;

{
	struct unisig_vccb	*uvp, *uvnext;

	for (uvp = Q_HEAD(usp->us_vccq, struct unisig_vccb); uvp;
			uvp = uvnext){
		uvnext = Q_NEXT(uvp, struct unisig_vccb, uv_sigelem);
		if (uvp->uv_call_ref == cref)
			break;
	}
	return(uvp);
}


/*
 * Find a VCCB
 *
 * Find a VCCB given the VPI and VCI.
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance
 *	vpi	the VPI to search for
 *	vci	the VCI to search for
 *	dir	the direction of the VCC (VCC_IN, VCC_OUT, or both).
 *		If dir is set to zero, return the address of any VCCB
 *		with the given VPI/VCI, regardless of direction.
 *
 * Returns:
 *	0	there is no such VCCB
 *	uvp	the address of the VCCB
 *
 */
struct unisig_vccb *
unisig_find_vpvc(usp, vpi, vci, dir)
	struct unisig	*usp;
	int		vpi, vci;
	u_char		dir;

{
	struct unisig_vccb	*uvp, *uvnext;

	for (uvp = Q_HEAD(usp->us_vccq, struct unisig_vccb); uvp;
			uvp = uvnext){
		uvnext = Q_NEXT(uvp, struct unisig_vccb, uv_sigelem);
		if (uvp->uv_vpi == vpi &&
				uvp->uv_vci == vci &&
				(uvp->uv_type & dir) == dir)
			break;
	}
	return(uvp);
}


/*
 * Allocate a call reference value
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance
 *
 * Returns:
 *	0	call reference not available
 *	cref	the call reference value
 *
 */
int
unisig_alloc_call_ref(usp)
	struct unisig	*usp;

{
	int	cref;

	/*
	 * Get the next call reference value
	 */
	cref = usp->us_cref;

	/*
	 * Make sure it hasn't got too large
	 */
	if (cref >= UNI_MSG_CALL_REF_DUMMY) {
		/* XXX */
		log(LOG_ERR, "uni: call reference limit reached\n");
		return(0);
	}
	
	/*
	 * Bump the call reference value
	 */
	usp->us_cref++;

	return(cref);
}


/*
 * Print an ATM address
 *
 * Convert an ATM address into an ASCII string suitable for printing.
 *
 * Arguments:
 *	p	pointer to an ATM address
 *
 * Returns:
 *	the address of a string with the ASCII representation of the
 *	address.  This routine returns the address of a statically-
 *	allocated buffer, so if repeated calls to this routine are made,
 *	each call will destroy the result of the previous call.
 *
 */
char *
unisig_addr_print(p)
	Atm_addr		*p;
{
	int		i;
	char		*fp, *op, t_buff[16];
	u_char		*cp;
	static char	strbuff[256];

	static char	nf_DCC[] = "0xX.XX.X.XXX.XX.XX.XX.XXXXXX.X";
	static char	nf_ICD[] = "0xX.XX.X.XXX.XX.XX.XX.XXXXXX.X";
	static char	nf_E164[] = "0xX.XXXXXXXX.XX.XX.XXXXXX.X";

	union {
		int	w;
		char	c[4];
	} u1, u2;

	/*
	 * Clear the print buffer
	 */
	bzero(strbuff, sizeof(strbuff));

	/*
	 * Select appropriate printing format
	 */
	switch(p->address_format) {
	case T_ATM_ENDSYS_ADDR:
		/*
		 * Select format by NSAP type
		 */
		switch(((Atm_addr_nsap *)p->address)->aan_afi) {
		default:
		case AFI_DCC:
			fp = nf_DCC;
			break;
		case AFI_ICD:
			fp = nf_ICD;
			break;
		case AFI_E164:
			fp = nf_E164;
			break;
		}

		/*
		 * Loop through the format string, converting the NSAP
		 * to ASCII
		 */
		cp = (u_char *) p->address;
		op = strbuff;
		while (*fp) {
			if (*fp == 'X') {
				/*
				 * If format character is an 'X', put a
				 * two-digit hex representation of the
				 * NSAP byte in the output buffer
				 */
				snprintf(t_buff, sizeof(t_buff),
					"%x", *cp + 512);
				strcpy(op, &t_buff[strlen(t_buff)-2]);
				op++; op++;
				cp++;
			} else {
				/*
				 * If format character isn't an 'X',
				 * just copy it to the output buffer
				 */
				*op = *fp;
				op++;
			}
			fp++;
		}

		break;

	case T_ATM_E164_ADDR:
		/*
		 * Print the IA5 characters of the E.164 address
		 */
		for(i=0; i<p->address_length; i++) {
			snprintf(strbuff + strlen(strbuff),
			    sizeof(strbuff) - strlen(strbuff), "%c",
				((Atm_addr_e164 *)p->address)->aae_addr[i]);
		}
		break;

	case T_ATM_SPANS_ADDR:
		/*
		 * Get address into integers
		 */
		u1.c[0] = ((Atm_addr_spans *)p->address)->aas_addr[0];
		u1.c[1] = ((Atm_addr_spans *)p->address)->aas_addr[1];
		u1.c[2] = ((Atm_addr_spans *)p->address)->aas_addr[2];
		u1.c[3] = ((Atm_addr_spans *)p->address)->aas_addr[3];
		u2.c[0] = ((Atm_addr_spans *)p->address)->aas_addr[4];
		u2.c[1] = ((Atm_addr_spans *)p->address)->aas_addr[5];
		u2.c[2] = ((Atm_addr_spans *)p->address)->aas_addr[6];
		u2.c[3] = ((Atm_addr_spans *)p->address)->aas_addr[7];

		/*
		 * Print the address as two words xxxxx.yyyyyyyy
		 */
		snprintf(strbuff, sizeof(strbuff), "%x.%x", u1.w, u2.w);
		break;

	case T_ATM_ABSENT:
	default:
		strcpy(strbuff, "-");
	}

	return(strbuff);
}


/*
 * Print the contents of a message buffer chain
 *
 * Arguments:
 *	m	pointer to a buffer
 *
 * Returns:
 *	none
 *
 */
void
unisig_print_mbuf(m)
	KBuffer		*m;
{
	int i;
	caddr_t		cp;

	printf("unisig_print_mbuf:\n");
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
