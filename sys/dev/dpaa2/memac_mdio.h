/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Bjoern A. Zeeb
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

#ifndef	__MEMAC_MDIO_H
#define	__MEMAC_MDIO_H

/* -------------------------------------------------------------------------- */

struct memacphy_softc_common {
	device_t			dev;
	device_t			dpnidev;
	int				phy;
};

int memacphy_miibus_readreg(device_t, int, int);
int memacphy_miibus_writereg(device_t, int, int, int);
void memacphy_miibus_statchg(struct memacphy_softc_common *);
int memacphy_set_ni_dev(struct memacphy_softc_common *, device_t);
int memacphy_get_phy_loc(struct memacphy_softc_common *, int *);


/* -------------------------------------------------------------------------- */

struct memac_mdio_softc_common {
	device_t		dev;
	struct resource		*mem_res;
	bool			is_little_endian;
};

int memac_miibus_readreg(struct memac_mdio_softc_common *, int, int);
int memac_miibus_writereg(struct memac_mdio_softc_common *, int, int, int);

ssize_t memac_mdio_get_property(device_t, device_t, const char *,
    void *, size_t, device_property_type_t);
int memac_mdio_read_ivar(device_t, device_t, int, uintptr_t *);

int memac_mdio_generic_attach(struct memac_mdio_softc_common *);
int memac_mdio_generic_detach(struct memac_mdio_softc_common *);

#endif	/* __MEMAC_MDIO_H */
