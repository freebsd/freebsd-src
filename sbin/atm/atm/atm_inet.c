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
 * User configuration and display program
 * --------------------------------------
 *
 * IP support
 *
 */

#include <sys/param.h>  
#include <sys/socket.h> 
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
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

#include "atm.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Process add command for a TCP/IP PVC
 * 
 * Command format: 
 *	atm add pvc <intf> <vpi> <vci> <aal> <encaps> IP <netif>
 *		<IP addr> | dynamic
 *
 * Arguments:
 *	argc	number of remaining arguments to command
 *	argv	pointer to remaining argument strings
 *	cmdp	pointer to command description 
 *	app	pointer to AIOCAPVC structure
 *	intp	pointer to air_int_rsp structure with information
 *		about the physical interface that is the PVC is for.
 *
 * Returns:
 *	none
 *
 */
void
ip_pvcadd(argc, argv, cmdp, app, intp)
	int			argc;
	char			**argv;
	struct cmd		*cmdp;
	struct atmaddreq	*app;
	struct air_int_rsp	*intp;
{
	char	*cp;
	char	nhelp[128];
	int	i, netif_pref_len, netif_no;

	/*
	 * Yet more validation
	 */
	if (argc < 2) {
		strcpy(nhelp, cmdp->help);
		cp = strstr(nhelp, "<netif>");
		if (cp)
			strcpy(cp, "ip {dyn|<dst>}");
		fprintf(stderr, "%s: Invalid number of arguments:\n",
				prog);
		fprintf(stderr, "\tformat is: %s%s %s\n",
				prefix, cmdp->name, nhelp);
		exit(1);
	}

	/*
	 * Validate and set network interface
	 */
	UM_ZERO(app->aar_pvc_intf, sizeof(app->aar_pvc_intf));
	netif_pref_len = strlen(intp->anp_nif_pref);
	cp = &argv[0][netif_pref_len];
	netif_no = atoi(cp);
	for (i=0; i<strlen(cp); i++) {
		if (cp[i] < '0' || cp[i] > '9') {
			netif_no = -1;
			break;
		}
	}
	if ((strlen(argv[0]) > sizeof(app->aar_pvc_intf) - 1) ||
			(netif_no < 0)) {
		fprintf(stderr, "%s: Illegal network interface name\n",
				prog);
		exit(1);
	}
	if (strncasecmp(intp->anp_nif_pref, argv[0], netif_pref_len) ||
			strlen (argv[0]) <= netif_pref_len ||
			netif_no > intp->anp_nif_cnt - 1) {
		fprintf(stderr, "%s: network interface %s is not associated with interface %s\n",
				prog,
				argv[0],
				intp->anp_intf);
		exit(1);
	}
	strcpy(app->aar_pvc_intf, argv[0]);
	argc--; argv++;

	/*
	 * Set PVC destination address
	 */
	UM_ZERO(&app->aar_pvc_dst, sizeof(struct sockaddr));
	if (strcasecmp(argv[0], "dynamic") == 0 ||
			strcasecmp(argv[0], "dyn") == 0) {

		/*
		 * Destination is dynamically determined
		 */
		app->aar_pvc_flags |= PVC_DYN;
	} else {

		/*
		 * Get destination IP address
		 */
		struct sockaddr_in	*sin;

		sin = (struct sockaddr_in *) &app->aar_pvc_dst;
		sin->sin_addr.s_addr =
				get_ip_addr(argv[0])->sin_addr.s_addr;
	}
	argc--; argv++;
}

