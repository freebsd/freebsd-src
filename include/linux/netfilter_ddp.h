#ifndef __LINUX_DDP_NETFILTER_H
#define __LINUX_DDP_NETFILTER_H

/* DDP-specific defines for netfilter.  Complete me sometime.
 * (C)1998 Rusty Russell -- This code is GPL.
 */

#include <linux/netfilter.h>

/* Appletalk hooks */
#define NF_DDP_INPUT	0
#define NF_DDP_FORWARD	1
#define NF_DDP_OUTPUT	2
#endif /*__LINUX_DDP_NETFILTER_H*/
