/*
 * Copyright (c) 1998 Robert Nordier
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>

#include <stddef.h>
#include "elfh.h"
#include "endian.h"

#define SET_ME	0xeeeeeeee    /* filled in by btxld */

/*
 * ELF header template.
 */
const struct elfh elfhdr = {
    {
	{
	    ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3,     /* e_ident */
	    ELFCLASS32, ELFDATA2LSB, EV_CURRENT, 0,
	    'F', 'r', 'e', 'e', 'B', 'S', 'D', 0
	},
	HTOLE16(ET_EXEC),			    /* e_type */
	HTOLE16(EM_386),			    /* e_machine */
	HTOLE32(EV_CURRENT),			    /* e_version */
	HTOLE32(SET_ME),			    /* e_entry */
	HTOLE32(offsetof(struct elfh, p)),	    /* e_phoff */
	HTOLE32(offsetof(struct elfh, sh)),	    /* e_shoff */
	0,					    /* e_flags */
	HTOLE16(sizeof(elfhdr.e)),		    /* e_ehsize */
	HTOLE16(sizeof(elfhdr.p[0])),		    /* e_phentsize */
	HTOLE16(sizeof(elfhdr.p) / sizeof(elfhdr.p[0])), /* e_phnum */
	HTOLE16(sizeof(elfhdr.sh[0])),		    /* e_shentsize */
	HTOLE16(sizeof(elfhdr.sh) / sizeof(elfhdr.sh[0])), /* e_shnum */
	HTOLE16(1)				    /* e_shstrndx */
    },
    {
	{
	    HTOLE32(PT_LOAD),			    /* p_type */
	    HTOLE32(sizeof(elfhdr)),		    /* p_offset */
	    HTOLE32(SET_ME),			    /* p_vaddr */
	    HTOLE32(SET_ME),			    /* p_paddr */
	    HTOLE32(SET_ME),			    /* p_filesz */
	    HTOLE32(SET_ME),			    /* p_memsz */
	    HTOLE32(PF_R | PF_X),		    /* p_flags */
	    HTOLE32(0x1000)			    /* p_align */
	},
	{
	    HTOLE32(PT_LOAD),			    /* p_type */
	    HTOLE32(SET_ME),			    /* p_offset */
	    HTOLE32(SET_ME),			    /* p_vaddr */
	    HTOLE32(SET_ME),			    /* p_paddr */
	    HTOLE32(SET_ME),			    /* p_filesz */
	    HTOLE32(SET_ME),			    /* p_memsz */
	    HTOLE32(PF_R | PF_W),		    /* p_flags */
	    HTOLE32(0x1000)			    /* p_align */
	}
    },
    {
	{
	    0, HTOLE32(SHT_NULL), 0, 0, 0, 0, HTOLE32(SHN_UNDEF), 0, 0, 0
	},
	{
	    HTOLE32(1),				    /* sh_name */
	    HTOLE32(SHT_STRTAB), 		    /* sh_type */
	    0,					    /* sh_flags */
	    0,					    /* sh_addr */
	    HTOLE32(offsetof(struct elfh, shstrtab)), /* sh_offset */
	    HTOLE32(sizeof(elfhdr.shstrtab)),	    /* sh_size */
	    HTOLE32(SHN_UNDEF),			    /* sh_link */
	    0,					    /* sh_info */
	    HTOLE32(1),				    /* sh_addralign */
	    0					    /* sh_entsize */
	},
	{
	    HTOLE32(0xb),			    /* sh_name */
	    HTOLE32(SHT_PROGBITS),		    /* sh_type */
	    HTOLE32(SHF_EXECINSTR | SHF_ALLOC),	    /* sh_flags */
	    HTOLE32(SET_ME),			    /* sh_addr */
	    HTOLE32(SET_ME),			    /* sh_offset */
	    HTOLE32(SET_ME),			    /* sh_size */
	    HTOLE32(SHN_UNDEF),			    /* sh_link */
	    0,					    /* sh_info */
	    HTOLE32(4),				    /* sh_addralign */
	    0					    /* sh_entsize */
	},
	{
	    HTOLE32(0x11),			    /* sh_name */
	    HTOLE32(SHT_PROGBITS),		    /* sh_type */
	    HTOLE32(SHF_ALLOC | SHF_WRITE),	    /* sh_flags */
	    HTOLE32(SET_ME),			    /* sh_addr */
	    HTOLE32(SET_ME),			    /* sh_offset */
	    HTOLE32(SET_ME),			    /* sh_size */
	    HTOLE32(SHN_UNDEF),			    /* sh_link */
	    0,					    /* sh_info */
	    HTOLE32(4),				    /* sh_addralign */
	    0					    /* sh_entsize */
	}
    },
    "\0.shstrtab\0.text\0.data" 		    /* shstrtab */
};
