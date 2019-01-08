/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#include "dapl_proto.h"

DAT_IA_ADDRESS_PTR DT_NetAddrAlloc(Per_Test_Data_t * pt_ptr)
{
	DAT_IA_ADDRESS_PTR netaddr;

	netaddr = (DAT_IA_ADDRESS_PTR) DT_Mdep_Malloc(sizeof(DAT_SOCK_ADDR));
	if (!netaddr) {
		DT_Mdep_printf("dapltest: No Memory to create netaddr!\n");
	}
	return netaddr;
}

void DT_NetAddrFree(Per_Test_Data_t * pt_ptr, DAT_IA_ADDRESS_PTR netaddr)
{
	DT_Mdep_Free(netaddr);
}

bool
DT_NetAddrLookupHostAddress(DAT_IA_ADDRESS_PTR to_netaddr,
			    DAT_NAME_PTR hostname)
{
	struct addrinfo *target;
	int rval;

	rval = getaddrinfo(hostname, NULL, NULL, &target);
	if (rval != 0) {
		char *whatzit = "unknown error return";

		switch (rval) {
		case EAI_FAMILY:
			{
				whatzit = "unsupported address family";
				break;
			}
		case EAI_SOCKTYPE:
			{
				whatzit = "unsupported socket type";
				break;
			}
		case EAI_BADFLAGS:
			{
				whatzit = "invalid flags";
				break;
			}
		case EAI_NONAME:
			{
				whatzit = "unknown node name";
				break;
			}
		case EAI_SERVICE:
			{
				whatzit = "service unavailable";
				break;
			}
#if !defined(WIN32) && defined(__USE_GNU)
		case EAI_ADDRFAMILY:
			{
				whatzit = "node has no address in this family";
				break;
			}
		case EAI_NODATA:
			{
				whatzit = "node has no addresses defined";
				break;
			}
#endif
		case EAI_MEMORY:
			{
				whatzit = "out of memory";
				break;
			}
		case EAI_FAIL:
			{
				whatzit = "permanent name server failure";
				break;
			}
		case EAI_AGAIN:
			{
				whatzit = "temporary name server failure";
				break;
			}
#if !defined(WIN32)
		case EAI_SYSTEM:
			{
				whatzit = "system error";
				break;
			}
#endif
		}

		DT_Mdep_printf("getaddrinfo (%s) failed (%s)\n",
			       hostname, whatzit);
		return DAT_FALSE;
	}

	/* Pull out IP address and print it as a sanity check */
	DT_Mdep_printf("Server Name: %s \n", hostname);
	DT_Mdep_printf("Server Net Address: %s\n",
		       inet_ntoa(((struct sockaddr_in *)target->ai_addr)->
				 sin_addr));

	*to_netaddr = *((DAT_IA_ADDRESS_PTR) target->ai_addr);
	freeaddrinfo(target);

	return (DAT_TRUE);
}
