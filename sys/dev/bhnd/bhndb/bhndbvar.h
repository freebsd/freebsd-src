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

#ifndef _BHND_BHNDBVAR_H_
#define _BHND_BHNDBVAR_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <dev/bhnd/bhndvar.h>
#include "bhndb.h"

#include "bhndb_if.h"

/*
 * Definitions shared by bhndb(4) driver implementations.
 */

DECLARE_CLASS(bhndb_driver);

struct bhndb_resources;

int	bhndb_attach(device_t dev, bhnd_devclass_t bridge_devclass);

int	bhndb_generic_probe(device_t dev);
int	bhndb_generic_detach(device_t dev);
int	bhndb_generic_suspend(device_t dev);
int	bhndb_generic_resume(device_t dev);
int	bhndb_generic_init_full_config(device_t dev, device_t child,
	    const struct bhndb_hw_priority *hw_prio_table);

int	bhnd_generic_br_suspend_child(device_t dev, device_t child);
int	bhnd_generic_br_resume_child(device_t dev, device_t child);

/** 
 * bhndb child address space. Children either operate in the bridged
 * SoC address space, or within the address space mapped to the host
 * device (e.g. the PCI BAR(s)).
 */
typedef enum {
	BHNDB_ADDRSPACE_BRIDGED,	/**< bridged (SoC) address space */
	BHNDB_ADDRSPACE_NATIVE		/**< host address space */
} bhndb_addrspace;

/** bhndb child instance state */
struct bhndb_devinfo {
	bhndb_addrspace		addrspace;	/**< child address space. */
	struct resource_list    resources;	/**< child resources. */
};

/**
 * bhndb driver instance state. Must be first member of all subclass
 * softc structures.
 */
struct bhndb_softc {
	device_t			 dev;		/**< bridge device */
	struct bhnd_chipid		 chipid;	/**< chip identification */
	bhnd_devclass_t			 bridge_class;	/**< bridge core type */
	struct bhnd_core_info		 bridge_core;	/**< bridge core. not populated until
							  *  full bridge config is initialized */
	bool				 have_br_core;	/**< false if not yet available */

	device_t			 parent_dev;	/**< parent device */
	device_t			 bus_dev;	/**< child bhnd(4) bus */

	struct mtx			 sc_mtx;	/**< resource lock. */
	struct bhndb_resources		*bus_res;	/**< bus resource state */
};

#endif /* _BHND_BHNDBVAR_H_ */
