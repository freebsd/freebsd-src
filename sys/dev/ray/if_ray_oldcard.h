/*
 * Copyright (C) 2000
 * Dr. Duncan McLennan Barclay, dmlb@ragnet.demon.co.uk.
 *
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DUNCAN BARCLAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DUNCAN BARCLAY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

/*
 * Hacks for working around the PCCard layer problems in NEWBUS kludge
 * and OLDCARD.
 *
 * Now that /sys/pccard/pcic.c can support multiple memory maps per
 * slot correctly this is historical code. It is being left in the tree
 * for a short while until interactions between OLDCARD and pccardd
 * are ironed out. The common and attribute memory is now always
 * mapped in. dmlb 17/1/01
 *
 * The driver assumes that the common memory is always mapped in,
 * for the moment we ensure this with the following macro at the
 * head of each function and by using functions to access attribute
 * memory. Hysterical raisins led to the non-"reflexive" approach.
 * Roll on NEWCARD and it can all die...
 *
 * We call the pccard layer to change and restore the mapping each
 * time we use the attribute memory.
 *
 * These could become marcos around bus_activate_resource, but
 * the functions do made hacking them around safer.
 */

#if RAY_NEED_CM_REMAPPING
#define	RAY_MAP_CM(sc)		ray_attr_mapcm(sc)
#else
#define RAY_MAP_CM(sc)
#endif /* RAY_NEED_CM_REMAPPING */

#if RAY_NEED_AM_REMAPPING
static __inline void	ray_attr_mapam		(struct ray_softc *sc);
static __inline u_int8_t	ray_attr_read_1	(struct ray_softc *sc, off_t offset);
static __inline void	ray_attr_write_1	(struct ray_softc *sc, off_t offset, u_int8_t byte);
#undef ATTR_READ_1
#define ATTR_READ_1(sc, off)		ray_attr_read_1((sc), (off))
#undef ATTR_WRITE_1
#define ATTR_WRITE_1(sc, off, val)	ray_attr_write_1((sc), (off), (val))
#endif /* RAY_NEED_AM_REMAPPING */

#if (RAY_NEED_AM_REMAPPING || RAY_NEED_CM_REMAPPING)
static __inline void	ray_attr_mapcm		(struct ray_softc *sc);
#endif /* (RAY_NEED_AM_REMAPPING || RAY_NEED_CM_REMAPPING) */

#if (RAY_NEED_AM_REMAPPING || RAY_NEED_CM_REMAPPING)
static __inline void
ray_attr_mapcm(struct ray_softc *sc)
{
	bus_activate_resource(sc->dev, SYS_RES_MEMORY, sc->cm_rid, sc->cm_res);
#if RAY_DEBUG & RAY_DBG_CM
	{
		u_long flags = 0xffff;
		CARD_GET_RES_FLAGS(device_get_parent(sc->dev), sc->dev,
		    SYS_RES_MEMORY, sc->cm_rid, &flags);
		RAY_PRINTF(sc, "common memory\n"
		    ".  start 0x%0lx count 0x%0lx flags 0x%0lx",
		    bus_get_resource_start(sc->dev, SYS_RES_MEMORY, sc->cm_rid),
		    bus_get_resource_count(sc->dev, SYS_RES_MEMORY, sc->cm_rid),
		    flags);
	}
#endif /* RAY_DEBUG & RAY_DBG_CM */
}
#endif /* (RAY_NEED_AM_REMAPPING || RAY_NEED_CM_REMAPPING) */

#if RAY_NEED_AM_REMAPPING
static __inline void
ray_attr_mapam(struct ray_softc *sc)
{
	bus_activate_resource(sc->dev, SYS_RES_MEMORY, sc->am_rid, sc->am_res);
#if RAY_DEBUG & RAY_DBG_CM
	{
		u_long flags = 0xffff;
		CARD_GET_RES_FLAGS(device_get_parent(sc->dev), sc->dev,
		    SYS_RES_MEMORY, sc->am_rid, &flags);
		RAY_PRINTF(sc, "attribute memory\n"
		    ".  start 0x%0lx count 0x%0lx flags 0x%0lx",
		    bus_get_resource_start(sc->dev, SYS_RES_MEMORY, sc->am_rid),
		    bus_get_resource_count(sc->dev, SYS_RES_MEMORY, sc->am_rid),
		    flags);
	}
#endif /* RAY_DEBUG & RAY_DBG_CM */
}

static __inline u_int8_t
ray_attr_read_1(struct ray_softc *sc, off_t offset)
{
	u_int8_t byte;

	ray_attr_mapam(sc);
	byte = (u_int8_t)bus_space_read_1(sc->am_bst, sc->am_bsh, offset);
	ray_attr_mapcm(sc);

	return (byte);
}

static __inline void
ray_attr_write_1(struct ray_softc *sc, off_t offset, u_int8_t byte)
{
	ray_attr_mapam(sc);
	bus_space_write_1(sc->am_bst, sc->am_bsh, offset, byte);
	ray_attr_mapcm(sc);
}
#endif /* RAY_NEED_AM_REMAPPING */
