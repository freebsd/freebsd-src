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
 *	@(#) $FreeBSD: src/sys/netatm/uni/uniip.c,v 1.4 1999/08/28 00:49:03 peter Exp $
 *
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * UNI IP interface module
 *
 */

#include <netatm/kern_include.h>

#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>
#include <netatm/uni/uniip_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/netatm/uni/uniip.c,v 1.4 1999/08/28 00:49:03 peter Exp $");
#endif


/*
 * Local functions
 */
static int	uniip_ipact __P((struct ip_nif *));
static int	uniip_ipdact __P((struct ip_nif *));


/*
 * Global variables
 */
struct uniip	*uniip_head = NULL;

struct ip_serv	uniip_ipserv = {
	uniip_ipact,
	uniip_ipdact,
	uniarp_ioctl,
	uniarp_pvcopen,
	uniarp_svcout,
	uniarp_svcin,
	uniarp_svcactive,
	uniarp_vcclose,
	NULL,
	{ { ATM_AAL5, ATM_ENC_LLC} },
};


/*
 * Local variables
 */
static struct sp_info  uniip_pool = {
	"uni ip pool",			/* si_name */
	sizeof(struct uniip),		/* si_blksiz */
	2,				/* si_blkcnt */
	100				/* si_maxallow */
};


/*
 * Process module loading notification
 * 
 * Called whenever the uni module is initializing.  
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0	initialization successful
 *	errno	initialization failed - reason indicated
 *
 */
int
uniip_start()
{
	int	err;

	/*
	 * Tell arp to initialize stuff
	 */
	err = uniarp_start();

	return (err);
}


/*
 * Process module unloading notification
 * 
 * Called whenever the uni module is about to be unloaded.  All signalling
 * instances will have been previously detached.  All uniip resources
 * must be freed now.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0	shutdown was successful 
 *	errno	shutdown failed - reason indicated
 *
 */
int
uniip_stop()
{

	/*
	 * All IP interfaces should be gone
	 */
	if (uniip_head)
		return (EBUSY);

	/*
	 * Tell arp to stop
	 */
	uniarp_stop();

	/*
	 * Free our storage pools
	 */
	atm_release_pool(&uniip_pool);

	return (0);
}


/*
 * Process IP Network Interface Activation
 * 
 * Called whenever an IP network interface becomes active.
 *
 * Called at splnet.
 *
 * Arguments:
 *	inp	pointer to IP network interface
 *
 * Returns:
 *	0 	command successful
 *	errno	command failed - reason indicated
 *
 */
static int
uniip_ipact(inp)
	struct ip_nif	*inp;
{
	struct uniip		*uip;

	/*
	 * Make sure we don't already have this interface
	 */
	for (uip = uniip_head; uip; uip = uip->uip_next) {
		if (uip->uip_ipnif == inp)
			return (EEXIST);
	}

	/*
	 * Get a new interface control block
	 */
	uip = (struct uniip *)atm_allocate(&uniip_pool);
	if (uip == NULL)
		return (ENOMEM);

	/*
	 * Initialize and link up
	 */
	uip->uip_ipnif = inp;
	LINK2TAIL(uip, struct uniip, uniip_head, uip_next);

	/*
	 * Link from IP world
	 */
	inp->inf_isintf = (caddr_t)uip;

	/*
	 * Tell arp about new interface
	 */
	uniarp_ipact(uip);

	return (0);
}


/*
 * Process IP Network Interface Deactivation
 * 
 * Called whenever an IP network interface becomes inactive.
 *
 * Called at splnet.
 *
 * Arguments:
 *	inp	pointer to IP network interface
 *
 * Returns:
 *	0 	command successful
 *	errno	command failed - reason indicated
 *
 */
static int
uniip_ipdact(inp)
	struct ip_nif	*inp;
{
	struct uniip		*uip;

	/*
	 * Get the appropriate IP interface block
	 */
	uip = (struct uniip *)inp->inf_isintf;
	if (uip == NULL)
		return (ENXIO);

	/*
	 * Let arp know about this
	 */
	uniarp_ipdact(uip);

	/*
	 * Free interface info
	 */
	UNLINK(uip, struct uniip, uniip_head, uip_next);
	if (uip->uip_prefix != NULL)
		KM_FREE(uip->uip_prefix, 
			uip->uip_nprefix * sizeof(struct uniarp_prf), M_DEVBUF);
	atm_free((caddr_t)uip);

	return (0);
}

