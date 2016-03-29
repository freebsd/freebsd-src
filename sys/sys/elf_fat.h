/*-
 * Copyright (c) 2016 Mahdi Mokhtari.
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
 */

#ifndef _SYS_ELF_FAT_H_
#define	_SYS_ELF_FAT_H_

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/elf_common.h>
#include <sys/elf_generic.h>
#include <sys/elf.h>

/* Values for the magic number bytes. */
#define	FATELF_MAG0		0xFa
#define	FATELF_MAG1		't'
#define	FATELF_MAG2		0x0E
#define	FATELF_MAG3		0x1f
#define	FATELF_MAGIC	"\xFat\x0E\x1f"	/* magic string */
#define	FATELF_MAG_LEN		4		/* magic string size */

#define	IS_FATELF(fehdr)	((fehdr).fe_magic[EI_MAG0] == FATELF_MAG0 && \
			 (fehdr).fe_magic[EI_MAG1] == FATELF_MAG1 && \
			 (fehdr).fe_magic[EI_MAG2] == FATELF_MAG2 && \
			 (fehdr).fe_magic[EI_MAG3] == FATELF_MAG3)

typedef struct {
	unsigned char fe_magic[4]; /* always SHOULD BE FATELF_MAGIC */
	uint16_t fe_version; /* currently is 0 for FreeBSD */
	uint16_t fe_nrecords;
} FatElf_FEhdr;

typedef struct {
	unsigned char ei_class;
	unsigned char ei_data;
	unsigned char ei_version;
	unsigned char ei_osabi; /* XXX actually NOT used, just for the sake of alignment XXX */

	uint32_t e_version; /* ELF format version. */

	uint16_t e_machine; /* Machine architecture. */
	uint16_t e_phentsize; /* Size of program header entry. */

	uint64_t r_offset;
	uint64_t r_size;
} FatElf_record;

#endif /* !_SYS_ELF_FAT_H_ */
