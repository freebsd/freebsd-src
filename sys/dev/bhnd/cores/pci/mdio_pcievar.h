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

#ifndef _BHND_CORES_PCI_MDIO_PCIEVAR_H_
#define _BHND_CORES_PCI_MDIO_PCIEVAR_H_

#include <dev/mdio/mdio.h>
#include "mdio_if.h"

DECLARE_CLASS(bhnd_mdio_pcie_driver);

int bhnd_mdio_pcie_attach(device_t dev, struct bhnd_resource *mem_res,
    int mem_rid, bus_size_t offset, bool c22ext);

struct bhnd_mdio_pcie_softc {
	device_t		 dev;		/**< mdio device */
	struct mtx		 sc_mtx;	/**< mdio register lock */

	struct bhnd_resource	*mem_res;	/**< parent pcie registers */
	int			 mem_rid;	/**< MDIO register resID, or
						     -1 if mem_res reference is
						     borrowed. */
	bus_size_t		 mem_off;	/**< mdio register offset */

	bool			 c22ext;	/**< automatically rewrite C45
						     register requests made
						     to the PCIe SerDes slave
						     to use its non-standard
						     C22 address extension
						     mechanism. */
};

#define	BHND_MDIO_PCIE_LOCK_INIT(sc) \
	mtx_init(&(sc)->sc_mtx, device_get_nameunit((sc)->dev), \
	    "bhnd_pci_mdio register lock", MTX_DEF)
#define	BHND_MDIO_PCIE_LOCK(sc)			mtx_lock(&(sc)->sc_mtx)
#define	BHND_MDIO_PCIE_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define	BHND_MDIO_PCIE_LOCK_ASSERT(sc, what)	mtx_assert(&(sc)->sc_mtx, what)
#define	BHND_MDIO_PCIE_LOCK_DESTROY(sc)		mtx_destroy(&(sc)->sc_mtx)

#endif /* _BHND_CORES_PCI_MDIO_PCIEVAR_H_ */
