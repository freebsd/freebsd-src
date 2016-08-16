/*-
 * Copyright (c) 2015-2016 Landon Fuller <landon@landonf.org>
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

#ifndef _BHND_NVRAM_BHND_SPROM_PARSER_H_
#define _BHND_NVRAM_BHND_SPROM_PARSER_H_

#include <dev/bhnd/bhnd.h>

struct bhnd_sprom;

int	bhnd_sprom_init(struct bhnd_sprom *sprom, struct bhnd_resource *r,
	    bus_size_t offset);
void	bhnd_sprom_fini(struct bhnd_sprom *sprom);
int	bhnd_sprom_getvar(struct bhnd_sprom *sc, const char *name, void *buf,
	    size_t *len, bhnd_nvram_type type);
int	bhnd_sprom_setvar(struct bhnd_sprom *sc, const char *name,
	    const void *buf, size_t len, bhnd_nvram_type type);

/**
 * bhnd sprom parser instance state.
 */
struct bhnd_sprom {
	device_t		 dev;		/**< sprom parent device */

	uint8_t			 sp_rev;	/**< sprom revision */
	
	struct bhnd_resource	*sp_res;	/**< sprom resource. */
	bus_size_t		 sp_res_off;	/**< offset to sprom image */

	uint8_t			*sp_shadow;	/**< sprom shadow */
	bus_size_t		 sp_size_max;	/**< maximum possible sprom length */
	size_t			 sp_size;	/**< shadow size */
	size_t			 sp_capacity;	/**< shadow buffer capacity */
};

#endif /* _BHND_NVRAM_BHND_SPROM_PARSER_H_ */
