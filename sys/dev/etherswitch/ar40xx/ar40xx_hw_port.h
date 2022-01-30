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
#ifndef	__AR40XX_HW_PORT_H__
#define	__AR40XX_HW_PORT_H__

extern	int ar40xx_hw_port_init(struct ar40xx_softc *sc, int port);
extern	int ar40xx_hw_port_cpuport_setup(struct ar40xx_softc *sc);
extern	int ar40xx_hw_port_link_up(struct ar40xx_softc *sc, int port);
extern	int ar40xx_hw_port_link_down(struct ar40xx_softc *sc, int port);
extern	int ar40xx_hw_get_port_pvid(struct ar40xx_softc *sc, int port,
	    int *pvid);
extern	int ar40xx_hw_set_port_pvid(struct ar40xx_softc *sc, int port,
	    int pvid);
extern	int ar40xx_hw_port_setup(struct ar40xx_softc *sc, int port,
	    uint32_t members);

#endif	/* __AR40XX_HW_PORT_H__ */

