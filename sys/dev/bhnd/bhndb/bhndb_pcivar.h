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

#ifndef _BHND_BHNDB_PCIVAR_H_
#define _BHND_BHNDB_PCIVAR_H_

#include "bhndbvar.h"

/*
 * bhndb(4) PCI driver subclass.
 */

DECLARE_CLASS(bhndb_pci_driver);

struct bhndb_pci_softc;

/*
 * An interconnect-specific function implementing BHNDB_SET_WINDOW_ADDR
 */
typedef int (*bhndb_pci_set_regwin_t)(struct bhndb_pci_softc *sc,
	         const struct bhndb_regwin *rw, bhnd_addr_t addr);

struct bhndb_pci_softc {
	struct bhndb_softc	bhndb;		/**< parent softc */
	device_t		dev;		/**< bridge device */
	bhnd_devclass_t		pci_devclass;	/**< PCI core's devclass */
	bhndb_pci_set_regwin_t	set_regwin;	/**< regwin handler */
};

#endif /* _BHND_BHNDB_PCIVAR_H_ */
