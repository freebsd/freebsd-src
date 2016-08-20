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

#ifndef _BHND_NVRAM_BHND_NVRAM_PARSER_H_
#define _BHND_NVRAM_BHND_NVRAM_PARSER_H_

#include <sys/param.h>
#include <sys/bus.h>

#include "bhnd_nvram_common.h"

union bhnd_nvram_ident;

struct bhnd_nvram_idx;
struct bhnd_nvram_ops;
struct bhnd_nvram_devpath;

struct bhnd_nvram;

LIST_HEAD(bhnd_nvram_devpaths, bhnd_nvram_devpath);

int	bhnd_nvram_parser_identify(const union bhnd_nvram_ident *ident,
	    bhnd_nvram_format expected);
int	bhnd_nvram_parser_init(struct bhnd_nvram *sc, device_t owner,
	    const void *data, size_t len, bhnd_nvram_format fmt);
void	bhnd_nvram_parser_fini(struct bhnd_nvram *sc);

int	bhnd_nvram_parser_getvar(struct bhnd_nvram *sc, const char *name,
	    void *buf, size_t *len, bhnd_nvram_type type);
int	bhnd_nvram_parser_setvar(struct bhnd_nvram *sc, const char *name,
	    const void *buf, size_t len, bhnd_nvram_type type);

/** BCM NVRAM header */
struct bhnd_nvram_header {
	uint32_t magic;
	uint32_t size;
	uint32_t cfg0;		/**< crc:8, version:8, sdram_init:16 */
	uint32_t cfg1;		/**< sdram_config:16, sdram_refresh:16 */
	uint32_t sdram_ncdl;	/**< sdram_ncdl */
} __packed;

/** 
 * NVRAM format identification.
 * 
 * To perform identification of the NVRAM format using bhnd_nvram_identify(),
 * read `sizeof(bhnd_nvram_indent)` bytes from the head of the NVRAM data.
 */
union bhnd_nvram_ident {
	struct bhnd_nvram_header	bcm;
	char				btxt[4];
	struct bhnd_tlv_ident {
		uint8_t		tag;
		uint8_t		size[2];
		uint8_t		flags;
	} __packed tlv;
};

/** bhnd nvram parser instance state */
struct bhnd_nvram {
	device_t			 dev;		/**< parent device, or NULL */
	const struct bhnd_nvram_ops	*ops;
	uint8_t				*buf;		/**< nvram data */
	size_t				 buf_size;
	size_t				 num_buf_vars;	/**< number of records in @p buf (0 if not yet calculated) */

	struct bhnd_nvram_idx		*idx;		/**< key index */

	struct bhnd_nvram_devpaths	 devpaths;	/**< device paths */
	struct bhnd_nvram_varmap	 defaults;	/**< default values */
	struct bhnd_nvram_varmap	 pending;	/**< uncommitted writes */
};

#endif /* _BHND_NVRAM_BHND_NVRAM_PARSER_H_ */
