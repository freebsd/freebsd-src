/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013-2014 Ruslan Bukin <br@bsdpad.com>
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
 */

#define	READ4(_sc, _reg)	\
	bus_space_read_4(_sc->bst, _sc->bsh, _reg)
#define	WRITE4(_sc, _reg, _val)	\
	bus_space_write_4(_sc->bst, _sc->bsh, _reg, _val)
#define	READ2(_sc, _reg)	\
	bus_space_read_2(_sc->bst, _sc->bsh, _reg)
#define	WRITE2(_sc, _reg, _val)	\
	bus_space_write_2(_sc->bst, _sc->bsh, _reg, _val)
#define	READ1(_sc, _reg)	\
	bus_space_read_1(_sc->bst, _sc->bsh, _reg)
#define	WRITE1(_sc, _reg, _val)	\
	bus_space_write_1(_sc->bst, _sc->bsh, _reg, _val)

uint32_t pll4_configure_output(uint32_t mfi, uint32_t mfn, uint32_t mfd);
uint32_t tcon_bypass(void);
