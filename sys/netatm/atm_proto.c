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
 * Core ATM Services
 * -----------------
 *
 * ATM socket protocol family support definitions
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <machine/clock.h>
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

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


struct protosw atmsw[] = {
{	SOCK_DGRAM,				/* ioctl()-only */
	&atmdomain,
	0,
	0,
	0,			/* pr_input */
	0,			/* pr_output */
	0,			/* pr_ctlinput */
	0,			/* pr_ctloutput */
	0,			/* pr_ousrreq */
	0,			/* pr_init */
	0,			/* pr_fasttimo */
	0,			/* pr_slowtimo */
	0,			/* pr_drain */
	&atm_dgram_usrreqs,	/* pr_usrreqs */
},

{	SOCK_SEQPACKET,				/* AAL-5 */
	&atmdomain,
	ATM_PROTO_AAL5,
	PR_ATOMIC|PR_CONNREQUIRED,
	0,			/* pr_input */
	0,			/* pr_output */
	0,			/* pr_ctlinput */
	atm_aal5_ctloutput,	/* pr_ctloutput */
	0,			/* pr_ousrreq */
	0,			/* pr_init */
	0,			/* pr_fasttimo */
	0,			/* pr_slowtimo */
	0,			/* pr_drain */
	&atm_aal5_usrreqs,	/* pr_usrreqs */
},

#ifdef XXX
{	SOCK_SEQPACKET,				/* SSCOP */
	&atmdomain,
	ATM_PROTO_SSCOP,
	PR_ATOMIC|PR_CONNREQUIRED|PR_WANTRCVD,
	x,			/* pr_input */
	x,			/* pr_output */
	x,			/* pr_ctlinput */
	x,			/* pr_ctloutput */
	0,			/* pr_ousrreq */
	0,			/* pr_init */
	0,			/* pr_fasttimo */
	0,			/* pr_slowtimo */
	x,			/* pr_drain */
	x,			/* pr_usrreqs */
},
#endif
};

struct domain atmdomain = {
	AF_ATM,
	"atm",
#if defined(__FreeBSD__)
	atm_initialize,
#else
	0,
#endif
	0,
	0, 
	atmsw,
	&atmsw[sizeof(atmsw) / sizeof(atmsw[0])]
};

DOMAIN_SET(atm);


#if (defined(__FreeBSD__) && (BSD >= 199506))
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
atm_proto_notsupp2(so, addr, p)
	struct socket	*so;
	struct sockaddr	*addr;
	struct proc	*p;
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
atm_proto_notsupp4(so, i, m, addr, m2, p)
	struct socket	*so;
	int		i;
	KBuffer		*m;
	struct sockaddr	*addr;
	KBuffer		*m2;
	struct proc	*p;
{
	return (EOPNOTSUPP);
}

#endif	/* (defined(__FreeBSD__) && (BSD >= 199506)) */

