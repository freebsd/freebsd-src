/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _FELIX_VAR_H_
#define _FELIX_VAR_H_

#define FELIX_INIT_TIMEOUT	5000	/* msec */

#define	FELIX_DEV_NAME	"Felix TSN Switch driver"
#define	FELIX_MAX_PORTS	6
#define	FELIX_NUM_VLANS	4096

#define	PCI_VENDOR_FREESCALE	0x1957
#define	FELIX_DEV_ID		0xEEF0

#define	FELIX_BAR_MDIO		0
#define	FELIX_BAR_REGS		4

#define	FELIX_LOCK(_sc)			mtx_lock(&(_sc)->mtx)
#define	FELIX_UNLOCK(_sc)			mtx_unlock(&(_sc)->mtx)
#define	FELIX_LOCK_ASSERT(_sc, _what)	mtx_assert(&(_sc)->mtx, (_what))

#define FELIX_RD4(sc, reg)		bus_read_4((sc)->regs, reg)
#define FELIX_WR4(sc, reg, value)	bus_write_4((sc)->regs, reg, value)

#define FELIX_DEVGMII_PORT_RD4(sc, port, reg)	\
	FELIX_RD4(sc, \
	    FELIX_DEVGMII_BASE + (FELIX_DEVGMII_PORT_OFFSET * (port)) + reg)
#define FELIX_DEVGMII_PORT_WR4(sc, port, reg, value)	\
	FELIX_WR4(sc, \
	    FELIX_DEVGMII_BASE + (FELIX_DEVGMII_PORT_OFFSET * (port)) + reg, \
	    value)

#define FELIX_ANA_PORT_RD4(sc, port, reg)	\
	FELIX_RD4(sc, \
	    FELIX_ANA_PORT_BASE + (FELIX_ANA_PORT_OFFSET * (port)) + reg)
#define FELIX_ANA_PORT_WR4(sc, port, reg, value)	\
	FELIX_WR4(sc, \
	    FELIX_ANA_PORT_BASE + (FELIX_ANA_PORT_OFFSET * (port)) + reg, \
	    value)

#define FELIX_REW_PORT_RD4(sc, port, reg)	\
	FELIX_RD4(sc, \
	    FELIX_REW_PORT_BASE + (FELIX_REW_PORT_OFFSET * (port)) + reg)
#define FELIX_REW_PORT_WR4(sc, port, reg, value)	\
	FELIX_WR4(sc, \
	    FELIX_REW_PORT_BASE + (FELIX_REW_PORT_OFFSET * (port)) + reg, \
	    value)

struct felix_pci_id {
	uint16_t vendor;
	uint16_t device;
	const char *desc;
};

struct felix_port {
	if_t			ifp;
	device_t                miibus;
	char                    *ifname;

	uint32_t                phyaddr;

	int			fixed_link_status;
	bool			fixed_port;
	bool			cpu_port;
};

typedef struct felix_softc {
	device_t		dev;
	struct resource		*regs;
	struct resource		*mdio;

	etherswitch_info_t	info;
	struct callout		tick_callout;
	struct mtx		mtx;
	struct felix_port	ports[FELIX_MAX_PORTS];

	int			vlan_mode;
	int                     vlans[FELIX_NUM_VLANS];

	uint32_t		timer_ticks;
} *felix_softc_t;

#endif
