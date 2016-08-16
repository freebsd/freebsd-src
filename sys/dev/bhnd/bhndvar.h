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

#ifndef _BHND_BHNDVAR_H_
#define _BHND_BHNDVAR_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include "bhnd.h"

/*
 * Definitions shared by bhnd(4) bus and bhndb(4) bridge driver implementations.
 */

MALLOC_DECLARE(M_BHND);
DECLARE_CLASS(bhnd_driver);

/**
 * bhnd per-device info.  Must be first member of all subclass
 * devinfo structures.
 */
struct bhnd_devinfo {
};

/**
 * bhnd driver instance state. Must be first member of all subclass
 * softc structures.
 */
struct bhnd_softc {
	device_t	dev;		/**< bus device */

	bool		attach_done;	/**< true if initialization of all
					  *  platform devices has been
					  *  completed */
	device_t	chipc_dev;	/**< bhnd_chipc device */ 
	device_t	nvram_dev;	/**< bhnd_nvram device, if any */
	device_t	pmu_dev;	/**< bhnd_pmu device, if any */
};

int			 bhnd_generic_attach(device_t dev);
int			 bhnd_generic_detach(device_t dev);
int			 bhnd_generic_shutdown(device_t dev);
int			 bhnd_generic_resume(device_t dev);
int			 bhnd_generic_suspend(device_t dev);

int			 bhnd_generic_get_probe_order(device_t dev,
			     device_t child);

int			 bhnd_generic_print_child(device_t dev,
			     device_t child);
void			 bhnd_generic_probe_nomatch(device_t dev,
			     device_t child);

device_t		 bhnd_generic_add_child(device_t dev, u_int order,
			     const char *name, int unit);
void			 bhnd_generic_child_deleted(device_t dev,
			     device_t child);
int			 bhnd_generic_suspend_child(device_t dev,
			     device_t child);
int			 bhnd_generic_resume_child(device_t dev,
			     device_t child);

int			 bhnd_generic_get_nvram_var(device_t dev,
			     device_t child, const char *name, void *buf,
			     size_t *size, bhnd_nvram_type type);

#endif /* _BHND_BHNDVAR_H_ */
