/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Adrian Chadd <adrian@FreeBSD.org>.
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
#ifndef	__AR40XX_HW_H__
#define	__AR40XX_HW_H__

extern	int ar40xx_hw_ess_reset(struct ar40xx_softc *sc);
extern	int ar40xx_hw_init_globals(struct ar40xx_softc *sc);
extern	int ar40xx_hw_vlan_init(struct ar40xx_softc *sc);
extern	int ar40xx_hw_sw_hw_apply(struct ar40xx_softc *sc);
extern	int ar40xx_hw_wait_bit(struct ar40xx_softc *sc, int reg,
	    uint32_t mask, uint32_t val);
extern	int ar40xx_hw_read_switch_mac_address(struct ar40xx_softc *sc,
	    struct ether_addr *ea);
extern	int ar40xx_hw_write_switch_mac_address(struct ar40xx_softc *sc,
	    struct ether_addr *ea);

#endif	/* __AR40XX_HW_H__ */

