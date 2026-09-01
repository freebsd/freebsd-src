/*
 * Copyright (c) 2026 Ahmad Khalifa <vexeduxr@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef RTL8723BU_H
#define RTL8723BU_H

#include <dev/rtwn/rtl8723b/r23b.h>

/* r23bu_init.c */
int r23bu_power_on(struct rtwn_softc *);
void r23bu_power_off(struct rtwn_softc *);
void r23bu_init_bb(struct rtwn_softc *);
void r23bu_init_ampdu(struct rtwn_softc *);
void r23bu_init_rx_agg(struct rtwn_softc *sc);

/* r23bu_rom.c */
void r23bu_parse_rom(struct rtwn_softc *, uint8_t *);

#endif
