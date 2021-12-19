/*-
 * Copyright (c) 2020-2021 The FreeBSD Foundation
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
 *
 * $FreeBSD$
 */
#ifndef	_LINUXKPI_LINUX_NETDEV_FEATURES_H_
#define	_LINUXKPI_LINUX_NETDEV_FEATURES_H_

#include <linux/types.h>
#include <linux/bitops.h>

typedef	uint32_t		netdev_features_t;

#define NETIF_F_HIGHDMA         BIT(0)
#define NETIF_F_SG              BIT(1)
#define NETIF_F_IP_CSUM         BIT(2)
#define NETIF_F_IPV6_CSUM       BIT(3)
#define NETIF_F_TSO             BIT(4)
#define NETIF_F_TSO6            BIT(5)
#define NETIF_F_RXCSUM          BIT(6)

#define NETIF_F_CSUM_MASK       (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM)

#endif	/* _LINUXKPI_LINUX_NETDEV_FEATURES_H_ */
