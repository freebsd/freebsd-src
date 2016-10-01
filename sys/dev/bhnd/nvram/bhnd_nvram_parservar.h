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

#ifndef _BHND_NVRAM_BHND_NVRAM_PARSERVAR_H_
#define _BHND_NVRAM_BHND_NVRAM_PARSERVAR_H_

#include <sys/types.h>

#include "bhnd_nvram_common.h"

#include "bhnd_nvram_parser.h"

#define	NVRAM_IDX_VAR_THRESH	15		/**< index is generated if minimum variable count is met */
#define	NVRAM_IDX_OFFSET_MAX	UINT16_MAX	/**< maximum indexable offset */
#define	NVRAM_IDX_LEN_MAX	UINT8_MAX	/**< maximum indexable key/value length */

#define	NVRAM_KEY_MAX		64		/**< maximum key length (not incl. NUL) */
#define	NVRAM_VAL_MAX		255		/**< maximum value length (not incl. NUL) */

#define	NVRAM_DEVPATH_STR	"devpath"	/**< name prefix of device path aliases */
#define	NVRAM_DEVPATH_LEN	(sizeof(NVRAM_DEVPATH_STR) - 1)

#define	NVRAM_SMALL_HASH_SIZE	16		/**< hash table size for pending/default tuples */

/**
 * NVRAM devpath record.
 * 
 * Aliases index values to full device paths.
 */
struct bhnd_nvram_devpath {
	u_long	 index;	/** alias index */
	char	*path;	/** aliased path */

	LIST_ENTRY(bhnd_nvram_devpath) dp_link;
};

/**
 * NVRAM index record.
 * 
 * Provides entry offsets into a backing NVRAM buffer.
 */
struct bhnd_nvram_idx_entry {
	uint16_t	env_offset;	/**< offset to env string */
	uint8_t		key_len;	/**< key length */
	uint8_t		val_len;	/**< value length */
};

/**
 * NVRAM index.
 * 
 * Provides a compact binary search index into the backing NVRAM buffer.
 */
struct bhnd_nvram_idx {
	size_t				num_entries;	/**< entry count */
	struct bhnd_nvram_idx_entry	entries[];	/**< index entries */
};

#endif /* _BHND_NVRAM_BHND_NVRAM_PARSERVAR_H_ */
