/*
 * Copyright (c) 2026 Mir Saheb Ali <mirsahebali@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef RTL8188FU_H
#define RTL8188FU_H

#include <dev/rtwn/rtl8188f/r88f.h>

/*
 * Function declarations
 */

/* r88fu_init.c */
int	r88fu_power_on(struct rtwn_softc *);
void	r88fu_power_off(struct rtwn_softc *);
void	r88fu_init_rx_agg(struct rtwn_softc *);
void	r88fu_init_tx_agg(struct rtwn_softc *);
void	r88fu_init_bb(struct rtwn_softc *);
void	r88fu_post_init(struct rtwn_softc *);

#endif /* RTL8188FU_H */
