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

#ifndef _BHND_NVRAM_BHND_NVRAMVAR_H_
#define _BHND_NVRAM_BHND_NVRAMVAR_H_

/** NVRAM Primitive data types */
typedef enum {
	BHND_NVRAM_DT_UINT8	= 0,	/**< unsigned 8-bit integer */
	BHND_NVRAM_DT_UINT16	= 1,	/**< unsigned 16-bit integer */
	BHND_NVRAM_DT_UINT32	= 2,	/**< unsigned 32-bit integer */
	BHND_NVRAM_DT_INT8	= 3,	/**< signed 8-bit integer */
	BHND_NVRAM_DT_INT16	= 4,	/**< signed 16-bit integer */
	BHND_NVRAM_DT_INT32	= 5,	/**< signed 32-bit integer */
	BHND_NVRAM_DT_CHAR	= 6,	/**< ASCII char */
} bhnd_nvram_dt;

/** NVRAM data type string representations */
typedef enum {
	BHND_NVRAM_VFMT_HEX	= 1,	/**< hex format */
	BHND_NVRAM_VFMT_DEC	= 2,	/**< decimal format */
	BHND_NVRAM_VFMT_MACADDR	= 3,	/**< mac address (canonical form, hex octets,
					     separated with ':') */
	BHND_NVRAM_VFMT_LEDDC	= 4,	/**< LED PWM duty-cycle (2 bytes -- on/off) */
	BHND_NVRAM_VFMT_CCODE	= 5	/**< count code format (2-3 ASCII chars, or hex string) */
} bhnd_nvram_fmt;

/** NVRAM variable flags */
enum {
	BHND_NVRAM_VF_ARRAY	= (1<<0),	/**< variable is an array */
	BHND_NVRAM_VF_MFGINT	= (1<<1),	/**< mfg-internal variable; should not be externally visible */
	BHND_NVRAM_VF_IGNALL1	= (1<<2)	/**< hide variable if its value has all bits set. */
};

#define	BHND_SPROMREV_MAX	UINT8_MAX	/**< maximum supported SPROM revision */

/** SPROM revision compatibility declaration */
struct bhnd_sprom_compat {
	uint8_t		first;	/**< first compatible SPROM revision */
	uint8_t		last;	/**< last compatible SPROM revision, or BHND_SPROMREV_MAX */
};

/** SPROM value descriptor */
struct bhnd_sprom_offset {
	uint16_t	offset;	/**< byte offset within SPROM */
	bool		cont:1;	/**< value should be bitwise OR'd with the
				  *  previous offset descriptor */
	bhnd_nvram_dt	type:7;	/**< data type */
	int8_t		shift;	/**< shift to be applied to the value */
	uint32_t	mask;	/**< mask to be applied to the value(s) */
};

/** SPROM-specific variable definition */
struct bhnd_sprom_var {
	struct bhnd_sprom_compat	 compat;	/**< sprom compatibility declaration */
	const struct bhnd_sprom_offset	*offsets;	/**< offset descriptors */
	size_t				 num_offsets;	/**< number of offset descriptors */
};

/** NVRAM variable definition */
struct bhnd_nvram_var {
	const char			*name;	  	/**< variable name */
	bhnd_nvram_dt			 type;	 	/**< base data type */
	bhnd_nvram_fmt			 fmt;		/**< string format */
	uint32_t			 flags;		/**< BHND_NVRAM_VF_* flags */

	const struct bhnd_sprom_var	*sprom_descs;	/**< SPROM-specific variable descriptors */
	size_t				 num_sp_descs;	/**< number of sprom descriptors */
};

size_t				 bhnd_nvram_type_width(bhnd_nvram_dt dt);
const struct bhnd_nvram_var	*bhnd_nvram_var_defn(const char *varname);

/** Initial bhnd_nvram_crc8 value */
#define	BHND_NVRAM_CRC8_INITIAL	0xFF

/** Valid CRC-8 checksum */
#define	BHND_NVRAM_CRC8_VALID	0x9F

extern const uint8_t bhnd_nvram_crc8_tab[];

/**
 * Calculate CRC-8 over @p buf.
 * 
 * @param buf input buffer
 * @param size buffer size
 * @param crc last computed crc, or BHND_NVRAM_CRC8_INITIAL
 */
static inline uint8_t
bhnd_nvram_crc8(const void *buf, size_t size, uint8_t crc)
{
	const uint8_t *p = (const uint8_t *)buf;
	while (size--)
		crc = bhnd_nvram_crc8_tab[(crc ^ *p++)];

	return (crc);
}


#endif /* _BHND_NVRAM_BHND_NVRAMVAR_H_ */