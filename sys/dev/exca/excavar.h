/* $FreeBSD$ */

/*
 * Copyright (c) 2002 M Warner Losh.  All rights reserved.
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
 *
 * This software may be derived from NetBSD i82365.c and other files with
 * the following copyright:
 *
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _SYS_DEV_EXCA_EXCAVAR_H
#define _SYS_DEV_EXCA_EXCAVAR_H

/*
 * Structure to manage the ExCA part of the chip.
 */
struct exca_softc;

struct exca_softc 
{
	device_t	dev;
	int		memalloc;
	struct		pccard_mem_handle mem[EXCA_MEM_WINS];
	int		ioalloc;
	struct		pccard_io_handle io[EXCA_IO_WINS];
	bus_space_tag_t	bst;
	bus_space_handle_t bsh;
	uint32_t	flags;
#define EXCA_SOCKET_PRESENT	0x00000001
#define EXCA_HAS_MEMREG_WIN	0x00000002
	uint32_t	offset;
};

void exca_init(struct exca_softc *sc, device_t dev, 
    bus_space_tag_t, bus_space_handle_t, uint32_t);
int exca_io_map(struct exca_softc *sc, int width, struct resource *r);
int exca_io_unmap_res(struct exca_softc *sc, struct resource *res);
int exca_is_pcic(struct exca_softc *sc);
int exca_mem_map(struct exca_softc *sc, int kind, struct resource *res);
int exca_mem_set_flags(struct exca_softc *sc, struct resource *res,
    uint32_t flags);
int exca_mem_set_offset(struct exca_softc *sc, struct resource *res,
    uint32_t cardaddr, uint32_t *deltap);
int exca_mem_unmap_res(struct exca_softc *sc, struct resource *res);
int exca_probe_slots(device_t dev, struct exca_softc *);
void exca_reset(struct exca_softc *, device_t child);

static __inline uint8_t
exca_read(struct exca_softc *sc, int reg)
{
	return (bus_space_read_1(sc->bst, sc->bsh, sc->offset + reg));
}

static __inline void
exca_write(struct exca_softc *sc, int reg, uint8_t val)
{
	bus_space_write_1(sc->bst, sc->bsh, sc->offset + reg, val);
}

static __inline void
exca_setb(struct exca_softc *sc, int reg, uint8_t mask)
{
	exca_write(sc, reg, exca_read(sc, reg) | mask);
}

static __inline void
exca_clrb(struct exca_softc *sc, int reg, uint8_t mask)
{
	exca_write(sc, reg, exca_read(sc, reg) & ~mask);
}

#endif /* !_SYS_DEV_EXCA_EXCAVAR_H */
