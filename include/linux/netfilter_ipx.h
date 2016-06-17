#ifndef __LINUX_IPX_NETFILTER_H
#define __LINUX_IPX_NETFILTER_H

/* IPX-specific defines for netfilter.  Complete me sometime.
 * (C)1998 Rusty Russell -- This code is GPL.
 */

#include <linux/netfilter.h>

/* IPX Hooks */
#define NF_IPX_INPUT	0
#define NF_IPX_FORWARD	1
#define NF_IPX_OUTPUT	2
#endif /*__LINUX_IPX_NETFILTER_H*/
