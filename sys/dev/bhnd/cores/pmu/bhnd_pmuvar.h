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

#ifndef _BHND_CORES_PMU_BHND_PMUVAR_H_
#define _BHND_CORES_PMU_BHND_PMUVAR_H_

#include <sys/types.h>
#include <sys/rman.h>

#include "bhnd_pmu.h"

DECLARE_CLASS(bhnd_pmu_driver);
extern devclass_t bhnd_pmu_devclass;

int	bhnd_pmu_probe(device_t dev);

int	bhnd_pmu_attach(device_t dev, struct bhnd_resource *res);
int	bhnd_pmu_detach(device_t dev);
int	bhnd_pmu_suspend(device_t dev);
int	bhnd_pmu_resume(device_t dev);

/* 
 * BHND PMU device quirks / features
 */
enum {
	/** No quirks */
	BPMU_QUIRK_NONE			= 0,

	/** On BCM4328-derived chipsets, the CLK_CTL_ST register CCS_HTAVAIL
	 *  and CCS_ALPAVAIL bits are swapped; the BHND_CCS0_* constants should
	 *  be used. */
	BPMU_QUIRK_CLKCTL_CCS0	= 1
};

/**
 * bhnd_pmu driver instance state.
 */
struct bhnd_pmu_softc {
	device_t		 dev;
	uint32_t		 quirks;	/**< device quirk flags */
	uint32_t		 caps;		/**< pmu capability flags. */

	struct bhnd_chipid	 cid;		/**< chip identification */
	struct bhnd_board_info	 board;		/**< board identification */
	device_t		 chipc_dev;	/**< chipcommon device */

	struct bhnd_resource	*res;		/**< pmu register block. */
	int			 rid;		/**< pmu register RID */

	struct mtx		 mtx;		/**< state mutex */

	uint32_t		 ilp_cps;	/**< measured ILP cycles per
						     second, or 0 */
};

#define	BPMU_LOCK_INIT(sc) \
	mtx_init(&(sc)->mtx, device_get_nameunit((sc)->dev), \
	    "BHND chipc driver lock", MTX_DEF)
#define	BPMU_LOCK(sc)				mtx_lock(&(sc)->mtx)
#define	BPMU_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	BPMU_LOCK_ASSERT(sc, what)		mtx_assert(&(sc)->mtx, what)
#define	BPMU_LOCK_DESTROY(sc)			mtx_destroy(&(sc)->mtx)

#endif /* _BHND_CORES_PMU_BHND_PMUVAR_H_ */
