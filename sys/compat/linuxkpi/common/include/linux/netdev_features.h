/*-
 * Copyright (c) 2020-2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUXKPI_LINUX_NETDEV_FEATURES_H_
#define	_LINUXKPI_LINUX_NETDEV_FEATURES_H_

#include <linux/types.h>
#include <linux/bitops.h>

typedef	uint32_t		netdev_features_t;

#define	NETIF_F_HIGHDMA		BIT(0)	/* Can DMA to high memory. */
#define	NETIF_F_SG		BIT(1)	/* Can do scatter/gather I/O. */
#define	NETIF_F_IP_CSUM		BIT(2)	/* Can csum TCP/UDP on IPv4. */
#define	NETIF_F_IPV6_CSUM	BIT(3)	/* Can csum TCP/UDP on IPv6. */
#define	NETIF_F_TSO		BIT(4)	/* Can do TCP over IPv4 segmentation. */
#define	NETIF_F_TSO6		BIT(5)	/* Can do TCP over IPv6 segmentation. */
#define	NETIF_F_RXCSUM		BIT(6)	/* Can do receive csum offload. */
#define	NETIF_F_HW_CSUM		BIT(7)	/* Can csum packets (which?). */
#define	NETIF_F_HW_TC		BIT(8)	/* Can offload TC. */

#define	NETIF_F_CSUM_MASK	(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM)

#define	NETIF_F_BITS							\
   "\20\1HIGHDMA\2SG\3IP_CSUM\4IPV6_CSUM\5TSO\6TSO6\7RXCSUM"		\
   "\10HW_CSUM\11HW_TC"

#endif	/* _LINUXKPI_LINUX_NETDEV_FEATURES_H_ */
