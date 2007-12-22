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
 * Core ATM Services
 * -----------------
 *
 * ATM socket protocol family support definitions
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
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

#error "NET_NEEDS_GIANT"

struct protosw atmsw[] = {
{
	.pr_type =		SOCK_DGRAM,		/* ioctl()-only */
	.pr_domain =		&atmdomain,
	.pr_usrreqs =		&atm_dgram_usrreqs
},

{
	.pr_type =		SOCK_SEQPACKET,		/* AAL-5 */
	.pr_domain =		&atmdomain,
	.pr_protocol =		ATM_PROTO_AAL5,
	.pr_flags =		PR_ATOMIC|PR_CONNREQUIRED,
	.pr_ctloutput =		atm_aal5_ctloutput,
	.pr_usrreqs =		&atm_aal5_usrreqs
},

#ifdef XXX
{
	.pr_type =		SOCK_SEQPACKET,		/* SSCOP */
	.pr_domain =		&atmdomain,
	.pr_protocol =		ATM_PROTO_SSCOP,
	.pr_flags =		PR_ATOMIC|PR_CONNREQUIRED|PR_WANTRCVD,
	.pr_input =		x,
	.pr_output =		x,
	.pr_ctlinput =		x,
	.pr_ctloutput =		x,
	.pr_drain =		x,
	.pr_usrreqs =		x
},
#endif
};

struct domain atmdomain = {
	.dom_family =		AF_ATM,
	.dom_name =		"atm",
	.dom_init =		atm_initialize,
	.dom_protosw =		atmsw,
	.dom_protoswNPROTOSW =	&atmsw[sizeof(atmsw) / sizeof(atmsw[0])]
};

DOMAIN_SET(atm);

SYSCTL_NODE(_net, PF_ATM, harp, CTLFLAG_RW, 0, "HARP/ATM family");
SYSCTL_NODE(_net_harp, OID_AUTO, atm, CTLFLAG_RW, 0, "ATM layer");

/*
 * Protocol request not supported
 *
 * Arguments:
 *	so	pointer to socket
 *
 * Returns:
 *	errno	error - operation not supported
 *
 */
int
atm_proto_notsupp1(so)
	struct socket	*so;
{
	return (EOPNOTSUPP);
}


/*
 * Protocol request not supported
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to protocol address
 *	p	pointer to process
 *
 * Returns:
 *	errno	error - operation not supported
 *
 */
int
atm_proto_notsupp2(so, addr, td)
	struct socket	*so;
	struct sockaddr	*addr;
	struct thread	*td;
{
	return (EOPNOTSUPP);
}


/*
 * Protocol request not supported
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to pointer to protocol address
 *
 * Returns:
 *	errno	error - operation not supported
 *
 */
int
atm_proto_notsupp3(so, addr)
	struct socket	*so;
	struct sockaddr	**addr;
{
	return (EOPNOTSUPP);
}


/*
 * Protocol request not supported
 *
 * Arguments:
 *	so	pointer to socket
 *	i	integer
 *	m	pointer to kernel buffer
 *	addr	pointer to protocol address
 *	m2	pointer to kernel buffer
 *	p	pointer to process
 *
 * Returns:
 *	errno	error - operation not supported
 *
 */
int
atm_proto_notsupp4(so, i, m, addr, m2, td)
	struct socket	*so;
	int		i;
	KBuffer		*m;
	struct sockaddr	*addr;
	KBuffer		*m2;
	struct thread	*td;
{
	return (EOPNOTSUPP);
}

/*
 * Protocol request not supported
 *
 * Arguments:
 *	so	pointer to socket
 *
 */
void
atm_proto_notsupp5(so)
	struct socket	*so;
{

}
