/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1997 Michael Smith
 * Copyright (c) 1998 Jonathan Lemon
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SMBIOS_H_
#define _SMBIOS_H_

/*
 * System Management BIOS
 */
#define	SMBIOS_START	0xf0000
#define	SMBIOS_STEP	0x10
#define	SMBIOS_OFF	0
#define	SMBIOS_LEN	4
#define	SMBIOS_SIG	"_SM_"

struct smbios_eps {
	uint8_t		anchor_string[4];		/* '_SM_' */
	uint8_t		checksum;
	uint8_t		length;
	uint8_t		major_version;
	uint8_t		minor_version;
	uint16_t	maximum_structure_size;
	uint8_t		entry_point_revision;
	uint8_t		formatted_area[5];
	uint8_t		intermediate_anchor_string[5];	/* '_DMI_' */
	uint8_t		intermediate_checksum;
	uint16_t	structure_table_length;
	uint32_t	structure_table_address;
	uint16_t	number_structures;
	uint8_t		BCD_revision;
} __packed;

struct smbios_structure_header {
	uint8_t		type;
	uint8_t		length;
	uint16_t	handle;
} __packed;

typedef void (*smbios_callback_t)(struct smbios_structure_header *, void *);

static inline void
smbios_walk_table(uint8_t *p, int entries, smbios_callback_t cb, void *arg)
{
	struct smbios_structure_header *s;

	while (entries--) {
		s = (struct smbios_structure_header *)p;
		cb(s, arg);

		/*
		 * Look for a double-nul after the end of the
		 * formatted area of this structure.
		 */
		p += s->length;
		while (!(p[0] == 0 && p[1] == 0))
			p++;

		/*
		 * Skip over the double-nul to the start of the next
		 * structure.
		 */
		p += 2;
	}
}

#ifdef _KERNEL
void identify_hypervisor_smbios(void);
#endif

#endif /* _SMBIOS_H_ */
