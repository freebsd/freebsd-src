/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#ifndef _ENETC_MDIO_H_
#define _ENETC_MDIO_H_

/* MDIO registers */
#define ENETC_MDIO_CFG		0x0	/* MDIO configuration and status */
#define MDIO_CFG_CLKDIV(x)	((((x) >> 1) & 0xff) << 8)

#define ENETC_MDIO_CTL		0x4	/* MDIO control */
#define MDIO_CTL_DEV_ADDR(x)	((x) & 0x1f)
#define MDIO_CTL_PORT_ADDR(x)	(((x) & 0x1f) << 5)

#define ENETC_MDIO_DATA		0x8	/* MDIO data */
#define MDIO_DATA(x)		((x) & 0xffff)

#define ENETC_MDIO_ADDR		0xc	/* MDIO address */

#define MDIO_CFG_BSY		BIT(0)
#define MDIO_CFG_RD_ER		BIT(1)
#define MDIO_CFG_ENC45		BIT(6)
#define MDIO_CFG_NEG		BIT(23)
#define MDIO_CTL_READ		BIT(15)
#define MII_ADDR_C45		BIT(30)

/* MDIO configuration and helpers */
#define ENETC_MDC_DIV		258
#define ENETC_TIMEOUT		1000

int enetc_mdio_write(struct resource*, int, int, int, int);
int enetc_mdio_read(struct resource*, int, int, int);

#endif
