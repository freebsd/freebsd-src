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
 * ATM Forum UNI Support
 * ---------------------
 *
 * UNI IP interface module
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>

#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>
#include <netatm/uni/uniip_var.h>

#include <vm/uma.h>

/*
 * Local functions
 */
static int	uniip_ipact(struct ip_nif *);
static int	uniip_ipdact(struct ip_nif *);


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
static uma_zone_t	uniip_zone;

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

	uniip_zone = uma_zcreate("uni ip", sizeof(struct uniip), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, 0);
	if (uniip_zone == NULL)
		panic("uniip_start: uma_zcreate");

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
	uma_zdestroy(uniip_zone);
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
	uip = uma_zalloc(uniip_zone, M_WAITOK | M_ZERO);
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
		free(uip->uip_prefix, M_DEVBUF);
	uma_zfree(uniip_zone, uip);
	return (0);
}

