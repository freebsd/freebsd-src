/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * $FreeBSD$
 */

#ifndef _SIBA_SIBAVAR_H_
#define _SIBA_SIBAVAR_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/limits.h>

#include <machine/bus.h>
#include <sys/rman.h>

#include "siba.h"

/*
 * Internal definitions shared by siba(4) driver implementations.
 */

struct siba_addrspace;
struct siba_devinfo;
struct siba_port;
struct siba_core_id;

int			 siba_probe(device_t dev);
int			 siba_attach(device_t dev);
int			 siba_detach(device_t dev);
int			 siba_resume(device_t dev);
int			 siba_suspend(device_t dev);

uint16_t		 siba_get_bhnd_mfgid(uint16_t ocp_vendor);

struct siba_core_id	 siba_parse_core_id(uint32_t idhigh, uint32_t idlow,
			     u_int core_idx, int unit);

int			 siba_add_children(device_t bus,
			     const struct bhnd_chipid *chipid);

struct siba_devinfo	*siba_alloc_dinfo(device_t dev,
			     const struct siba_core_id *core_id);
void			 siba_free_dinfo(device_t dev,
			     struct siba_devinfo *dinfo);

struct siba_port	*siba_dinfo_get_port(struct siba_devinfo *dinfo,
			     bhnd_port_type port_type, u_int port_num);

struct siba_addrspace	*siba_find_port_addrspace(struct siba_port *port,
			     uint8_t sid);

int			 siba_append_dinfo_region(struct siba_devinfo *dinfo,
			     bhnd_port_type port_type, u_int port_num,
			     u_int region_num, uint8_t sid, uint32_t base,
			     uint32_t size, uint32_t bus_reserved);

u_int			 siba_admatch_offset(uint8_t addrspace);
int			 siba_parse_admatch(uint32_t am, uint32_t *addr,
			     uint32_t *size);

/* Sonics configuration register blocks */
#define	SIBA_CFG_NUM_2_2	1			/**< sonics <= 2.2 maps SIBA_CFG0. */
#define	SIBA_CFG_NUM_2_3	2			/**< sonics <= 2.3 maps SIBA_CFG0 and SIBA_CFG1 */
#define	SIBA_CFG_NUM_MAX	SIBA_CFG_NUM_2_3	/**< maximum number of supported config
							     register blocks */

/** siba(4) address space descriptor */
struct siba_addrspace {
	uint32_t	sa_base;	/**< base address */
	uint32_t	sa_size;	/**< size */
	u_int		sa_region_num;	/**< bhnd region id */
	uint8_t		sa_sid;		/**< siba-assigned address space ID */
	int		sa_rid;		/**< bus resource id */
	uint32_t	sa_bus_reserved;/**< number of bytes at high end of
					  *  address space reserved for the bus */

	STAILQ_ENTRY(siba_addrspace) sa_link;
};

/** siba(4) port descriptor */
struct siba_port {
	bhnd_port_type		 sp_type;	/**< port type */
	u_int			 sp_num;	/**< port number */
	u_int			 sp_num_addrs;	/**< number of address space mappings */

	STAILQ_HEAD(, siba_addrspace) sp_addrs;	/**< address spaces mapped to this port */
};

/**
 * siba(4) per-core identification info.
 */
struct siba_core_id {
	struct bhnd_core_info	core_info;	/**< standard bhnd(4) core info */
	uint16_t		sonics_vendor;	/**< OCP vendor identifier used to generate
						  *  the JEDEC-106 bhnd(4) vendor identifier. */
	uint8_t			sonics_rev;	/**< sonics backplane revision code */
	uint8_t			num_addrspace;	/**< number of address ranges mapped to
						     this core. */
	uint8_t			num_cfg_blocks;	/**< number of Sonics configuration register
						     blocks mapped to the core's enumeration
						     space */
};

/**
 * siba(4) per-device info
 */
struct siba_devinfo {
	struct resource_list	 resources;	/**< per-core memory regions. */
	struct siba_core_id	 core_id;	/**< core identification info */

	struct siba_port	 device_port;	/**< device port holding ownership
						 *   of all siba address space
						 *   entries for this core. */

	/** SIBA_CFG* register blocks */
	struct bhnd_resource	*cfg[SIBA_CFG_NUM_MAX];

	/** SIBA_CFG* resource IDs */
	int			 cfg_rid[SIBA_CFG_NUM_MAX];
};


/** siba(4) per-instance state */
struct siba_softc {
	struct bhnd_softc	bhnd_sc;	/**< bhnd state */
	device_t		dev;		/**< siba device */
	device_t		hostb_dev;	/**< host bridge core, or NULL */
};

#endif /* _SIBA_SIBAVAR_H_ */
