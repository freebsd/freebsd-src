/*-
 * Copyright (c) 2003 Jake Burkholder.
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef _DEV_ZS_ZSVAR_H_
#define	_DEV_ZS_ZSVAR_H_

struct zs_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_bt;
	bus_space_handle_t	sc_bh;
	struct zstty_softc	*sc_child[ZS_NCHAN];
	void			*sc_softih;
};

struct zstty_softc {
	device_t		sc_dev;
	struct zs_softc		*sc_parent;
	bus_space_tag_t		sc_bt;
	bus_space_handle_t	sc_csr;
	bus_space_handle_t	sc_data;
	dev_t			sc_si;
	struct tty		*sc_tty;
	int			sc_icnt;
	uint8_t			*sc_iput;
	uint8_t			*sc_iget;
	int			sc_ocnt;
	uint8_t			*sc_oget;
	int			sc_brg_clk;
	int			sc_alt_break_state;
	struct mtx		sc_mtx;
	uint8_t			sc_console;
	uint8_t			sc_tx_busy;
	uint8_t			sc_tx_done;
	uint8_t			sc_preg_held;
	uint8_t			sc_creg[16];
	uint8_t			sc_preg[16];
	uint8_t			sc_ibuf[CBLOCK];
	uint8_t			sc_obuf[CBLOCK];
};

int zs_attach(device_t dev);
int zs_probe(device_t dev);
void zs_intr(void *v);

int zstty_attach(device_t dev);
int zstty_probe(device_t dev);

int zstty_console(device_t dev, char *mode, int len);

void zstty_set_speed(struct zstty_softc *sc, int ospeed);

#endif
