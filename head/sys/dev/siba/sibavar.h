/*-
 * Copyright (c) 2007 Bruce M. Simpson.
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

#ifndef _SIBA_SIBAVAR_H_
#define _SIBA_SIBAVAR_H_

#include <sys/rman.h>

struct siba_softc {
	device_t		 sc_dev;	/* Device ID */
	struct resource		*sc_mem;	/* Memory window on nexus */

	bus_space_tag_t		 sc_bt;
	bus_space_handle_t	 sc_bh;
	bus_addr_t		 sc_maddr;
	bus_size_t		 sc_msize;

	uint8_t			 sc_ncores;
};

struct siba_devinfo {
	struct resource_list	 sdi_rl;
	/*devhandle_t		 sdi_devhandle; XXX*/
	/*struct rman sdi_intr_rman;*/

	/* Accessors are needed for ivars below. */
	uint16_t		 sdi_vid;
	uint16_t		 sdi_devid;
	uint8_t			 sdi_rev;
	uint8_t			 sdi_idx;	/* core index on bus */
	uint8_t			 sdi_irq;	/* TODO */
};

#define siba_read_2(sc, core, reg)				\
	bus_space_read_2((sc)->sc_bt, (sc)->sc_bh,		\
			 (core * SIBA_CORE_LEN) + (reg))

#define siba_read_4(sc, core, reg)				\
	bus_space_read_4((sc)->sc_bt, (sc)->sc_bh,		\
			 (core * SIBA_CORE_LEN) + (reg))

#define siba_write_2(sc, core, reg, val)			\
	bus_space_write_2((sc)->sc_bt, (sc)->sc_bh,		\
			 (core * SIBA_CORE_LEN) + (reg), (val))

#define siba_write_4(sc, core, reg, val)			\
	bus_space_write_4((sc)->sc_bt, (sc)->sc_bh,		\
			 (core * SIBA_CORE_LEN) + (reg), (val))

enum siba_device_ivars {
	SIBA_IVAR_VENDOR,
	SIBA_IVAR_DEVICE,
	SIBA_IVAR_REVID,
	SIBA_IVAR_CORE_INDEX
};

#define	SIBA_ACCESSOR(var, ivar, type)				\
	__BUS_ACCESSOR(siba, var, SIBA, ivar, type)

SIBA_ACCESSOR(vendor,		VENDOR,		uint16_t)
SIBA_ACCESSOR(device,		DEVICE,		uint16_t)
SIBA_ACCESSOR(revid,		REVID,		uint8_t)
SIBA_ACCESSOR(core_index,	CORE_INDEX,	uint8_t)

#undef SIBA_ACCESSOR

#endif /* _SIBA_SIBAVAR_H_ */
