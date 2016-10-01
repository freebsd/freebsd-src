/*-
 * Copyright (c) 2015-2016 Landon Fuller <landonf@FreeBSD.org>
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

#ifndef _BHND_NVRAM_BHND_NVRAMVAR_H_
#define _BHND_NVRAM_BHND_NVRAMVAR_H_

#include <sys/param.h>
#include <sys/bus.h>

#include "bhnd_nvram_parser.h"

DECLARE_CLASS(bhnd_nvram_driver);

int	bhnd_nvram_probe(device_t dev);
int	bhnd_nvram_attach(device_t dev, void *data, size_t size,
	    bhnd_nvram_format fmt);
int	bhnd_nvram_resume(device_t dev);
int	bhnd_nvram_suspend(device_t dev);
int	bhnd_nvram_detach(device_t dev);

/**
 * bhnd_nvram driver instance state. Must be first member of all subclass
 * softc structures.
 */
struct bhnd_nvram_softc {
	device_t		 	dev;
	struct mtx		 	mtx;	/**< nvram mutex */
	struct bhnd_nvram		nvram;	/**< nvram shadow */
};


#define	BHND_NVRAM_LOCK_INIT(sc) \
	mtx_init(&(sc)->mtx, device_get_nameunit((sc)->dev), \
	    "bhnd_nvram lock", MTX_DEF)
#define	BHND_NVRAM_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	BHND_NVRAM_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	BHND_NVRAM_LOCK_ASSERT(sc, what)	mtx_assert(&(sc)->mtx, what)
#define	BHND_NVRAM_LOCK_DESTROY(sc)		mtx_destroy(&(sc)->mtx)

#endif /* _BHND_NVRAM_BHND_NVRAMVAR_H_ */
