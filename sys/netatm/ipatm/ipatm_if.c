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
 * IP Over ATM Support
 * -------------------
 *
 * Interface Manager
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */
static void	ipatm_closenif __P((struct ip_nif *));


/*
 * Process Network Interface status change
 * 
 * Called whenever a network interface status change is requested.
 *
 * Called at splnet.
 *
 * Arguments:
 *	cmd	command code
 *	nip	pointer to atm network interface control block
 *	arg	command specific parameter
 *
 * Returns:
 *	0 	command successful
 *	errno	command failed - reason indicated
 *
 */
int
ipatm_nifstat(cmd, nip, arg)
	int		cmd;
	struct atm_nif	*nip;
	int		arg;
{
	struct in_ifaddr	*ia;
	struct siginst		*sip;
	struct ip_nif		*inp;
	int	err = 0;

	/*
	 * Look for corresponding IP interface
	 */
	for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
		if (inp->inf_nif == nip)
			break;
	}

	/*
	 * Process command
	 */
	switch (cmd) {

	case NCM_ATTACH:
		/*
		 * Make sure i/f isn't already attached
		 */
		if (inp != NULL) {
			err = EEXIST;
			break;
		}

		/*
		 * Get a new interface block
		 */
		inp = (struct ip_nif *)atm_allocate(&ipatm_nifpool);
		if (inp == NULL) {
			err = ENOMEM;
			break;
		}
		inp->inf_nif = nip;
		inp->inf_state = IPNIF_ADDR;
		inp->inf_arpnotify = ipatm_arpnotify;
		inp->inf_ipinput = ipatm_ipinput;
		inp->inf_createsvc = ipatm_createsvc;
		LINK2TAIL(inp, struct ip_nif, ipatm_nif_head, inf_next);
		break;

	case NCM_DETACH:
		/*
		 * Make sure i/f is attached
		 */
		if (inp == NULL) {
			err = ENODEV;
			break;
		}

		/*
		 * Validate interface stuff
		 */
		if (Q_HEAD(inp->inf_vcq, struct ipvcc))
			panic("ipatm_nifstat: ipvcc queue not empty");

		/*
		 * If we're active, close all our VCCs and tell the
		 * interface service about the deactivation
		 */
		if (inp->inf_state == IPNIF_ACTIVE) {

			ipatm_closenif(inp);

			if (inp->inf_serv)
				(void) (*inp->inf_serv->is_ifdact)(inp);
		}

		/*
		 * Clean up and free block
		 */
		UNLINK(inp, struct ip_nif, ipatm_nif_head, inf_next);
		atm_free((caddr_t)inp);
		break;

	case NCM_SETADDR:
		/*
		 * We only care about IP addresses
		 */
#if (defined(BSD) && (BSD >= 199103))
		if (((struct ifaddr *)arg)->ifa_addr->sa_family != AF_INET)
#else
		if (((struct ifaddr *)arg)->ifa_addr.sa_family != AF_INET)
#endif
			break;

		/*
		 * Make sure i/f is there
		 */
		ia = (struct in_ifaddr *)arg;
		if (inp == NULL)
			panic("ipatm_nifstat: setaddr missing ip_nif");

		/*
		 * Process new address
		 */
		switch (inp->inf_state) {

		case IPNIF_SIGMGR:
		case IPNIF_ADDR:
			inp->inf_addr = ia;

			/*
			 * If signalling manager is not set, wait for it
			 */
			sip = nip->nif_pif->pif_siginst;
			if (sip == NULL) {
				inp->inf_state = IPNIF_SIGMGR;
				break;
			}

			/*
			 * Otherwise, everything's set
			 */
			inp->inf_state = IPNIF_ACTIVE;

			/*
			 * Tell interface service we're around
			 */
			if (sip->si_ipserv) {
				inp->inf_serv = sip->si_ipserv;
				err = (*inp->inf_serv->is_ifact)(inp);
			}

			/*
			 * Reset state if there's been a problem
			 */
			if (err) {
				inp->inf_serv = NULL;
				inp->inf_addr = NULL;
				inp->inf_state = IPNIF_ADDR;
			}
			break;

		case IPNIF_ACTIVE:
			/*
			 * We dont support an address change
			 */
			err = EEXIST;
			break;
		}
		break;

	case NCM_SIGATTACH:
		/*
		 * Make sure i/f is attached
		 */
		if (inp == NULL) {
			err = ENODEV;
			break;
		}

		/*
		 * Are we waiting for the sigmgr attach??
		 */
		if (inp->inf_state != IPNIF_SIGMGR) {
			/*
			 * No, nothing else to do
			 */
			break;
		}

		/*
		 * OK, everything's set
		 */
		inp->inf_state = IPNIF_ACTIVE;

		/*
		 * Tell interface service we're around
		 */
		sip = nip->nif_pif->pif_siginst;
		if (sip->si_ipserv) {
			inp->inf_serv = sip->si_ipserv;
			err = (*inp->inf_serv->is_ifact)(inp);
		}

		/*
		 * Just report any problems, since a NCM_SIGDETACH will
		 * be coming down immediately
		 */
		break;

	case NCM_SIGDETACH:
		/*
		 * Make sure i/f is attached
		 */
		if (inp == NULL) {
			err = ENODEV;
			break;
		}

		/*
		 * Are we currently active??
		 */
		if (inp->inf_state != IPNIF_ACTIVE) {
			/*
			 * No, nothing else to do
			 */
			break;
		}

		/*
		 * Close all the IP VCCs for this interface
		 */
		ipatm_closenif(inp);

		/*
		 * Tell interface service that i/f has gone down
		 */
		if (inp->inf_serv)
			(void) (*inp->inf_serv->is_ifdact)(inp);

		/*
		 * Just have to wait for another sigattach
		 */
		inp->inf_serv = NULL;
		inp->inf_state = IPNIF_SIGMGR;
		break;

	default:
		log(LOG_ERR, "ipatm_nifstat: unknown command %d\n", cmd);
	}

	return (err);
}


/*
 * Close all VCCs on a Network Interface
 * 
 * Called at splnet.
 *
 * Arguments:
 *	inp	pointer to IP network interface
 *
 * Returns:
 *	none
 *
 */
static void
ipatm_closenif(inp)
	struct ip_nif	*inp;
{
	struct ipvcc	*ivp, *inext;

	/*
	 * Close each IP VCC on this interface
	 */
	for (ivp = Q_HEAD(inp->inf_vcq, struct ipvcc); ivp; ivp = inext) {

		inext = Q_NEXT(ivp, struct ipvcc, iv_elem);

		(void) ipatm_closevc(ivp, T_ATM_CAUSE_UNSPECIFIED_NORMAL);
	}
}

