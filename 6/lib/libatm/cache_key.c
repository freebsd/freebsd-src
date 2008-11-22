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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * User Space Library Functions
 * ----------------------------
 *
 * SCSP cache key computation
 *
 */

#include <sys/types.h>
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

#include <md5.h>
#include <string.h>

#include "libatm.h"

/*
 * Compute an SCSP cache key
 *
 * Arguments:
 *	ap	pointer to an Atm_addr with the ATM address
 *	ip	pointer to a struct in_addr with the IP address
 *	ol	the required length of the cache key
 *	op	pointer to receive cache key
 *
 * Returns:
 *	none
 *
 */
void
scsp_cache_key(const Atm_addr *ap, const struct in_addr *ip, int ol, char *op)
{
	int	i, len;
	char	buff[32], digest[16];
	MD5_CTX	context;

	/*
	 * Initialize
	 */
	bzero(buff, sizeof(buff));

	/*
	 * Copy the addresses into a buffer for MD5 computation
	 */
	len = sizeof(struct in_addr) + ap->address_length;
	if (len > (int)sizeof(buff))
		len = sizeof(buff);
	bcopy(ip, buff, sizeof(struct in_addr));
	bcopy(ap->address, &buff[sizeof(struct in_addr)],
			len - sizeof(struct in_addr));

	/*
	 * Compute the MD5 digest of the combined IP and ATM addresses
	 */
	MD5Init(&context);
	MD5Update(&context, buff, len);
	MD5Final(digest, &context);

	/*
	 * Fold the 16-byte digest to the required length
	 */
	bzero((caddr_t)op, ol);
	for (i = 0; i < 16; i++) {
		op[i % ol] = op[i % ol] ^ digest[i];
	}
}
