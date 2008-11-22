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
 * IP checksum computation
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

#include "libatm.h"

/*
 * Compute an IP checksum
 *
 * This code was taken from RFC 1071.
 *
 * "The following "C" code algorithm computes the checksum with an inner
 * loop that sums 16 bits at a time in a 32-bit accumulator."
 *
 * Arguments:
 *	addr	pointer to the buffer whose checksum is to be computed
 *	count	number of bytes to include in the checksum
 *
 * Returns:
 *	the computed checksum
 *
 */
short
ip_checksum(const char *addr, int count)
{
	/* Compute Internet Checksum for "count" bytes
	 * beginning at location "addr".
	 */
	long sum = 0;

	while( count > 1 ) {
		/* This is the inner loop */
		sum += ntohs(* (const unsigned short *)(const void *)addr);
		addr += sizeof(unsigned short);
		count -= sizeof(unsigned short);
	}

	/* Add left-over byte, if any */
	if( count > 0 )
		sum += * (const unsigned char *) addr;

	/* Fold 32-bit sum to 16 bits */
	while (sum>>16)
		sum = (sum & 0xffff) + (sum >> 16);

	return((short)~sum);
}
