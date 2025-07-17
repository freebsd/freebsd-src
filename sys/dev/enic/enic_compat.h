/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _ENIC_COMPAT_H_
#define _ENIC_COMPAT_H_

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/priv.h>

#include <machine/bus.h>
#include <machine/resource.h>

#define ETH_ALEN	ETHER_ADDR_LEN

#define typeof		__typeof__
#define __iomem
#define unlikely(x)	__builtin_expect((x),0)

#define le16_to_cpu
#define le32_to_cpu
#define le64_to_cpu
#define cpu_to_le16
#define cpu_to_le32
#define cpu_to_le64

#define pr_err(y, args...) dev_err(0, y, ##args)
#define pr_warn(y, args...) dev_warning(0, y, ##args)
#define BUG() pr_err("BUG at %s:%d", __func__, __LINE__)

#define VNIC_ALIGN(x, a)	__ALIGN_MASK(x, (typeof(x))(a)-1)
#define __ALIGN_MASK(x, mask)	(((x)+(mask))&~(mask))
#define udelay(t) DELAY(t)
#define usleep(x) pause("ENIC usleep", ((x) * 1000000 / hz + 1))

#define dev_printk(level, fmt, args...)	\
	printf(fmt, ## args)

#define dev_err(x, args...) dev_printk(ERR, args)
/*#define dev_info(x, args...) dev_printk(INFO,  args)*/
#define dev_info(x, args...)

#define __le16 uint16_t
#define __le32 uint32_t
#define __le64 uint64_t

#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1 : __min2; })

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1 : __max2; })

#endif /* _ENIC_COMPAT_H_ */
