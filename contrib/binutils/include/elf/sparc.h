/* SPARC ELF support for BFD.
   Copyright (C) 1996, 1997 Free Software Foundation, Inc.
   By Doug Evans, Cygnus Support, <dje@cygnus.com>.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _ELF_SPARC_H
#define _ELF_SPARC_H

/* Processor specific flags for the ELF header e_flags field.  */

/* These are defined by Sun.  */

#define EF_SPARC_32PLUS_MASK	0xffff00	/* bits indicating V8+ type */
#define EF_SPARC_32PLUS		0x000100	/* generic V8+ features */
#define EF_SPARC_SUN_US1	0x000200	/* Sun UltraSPARC1 extensions */
#define EF_SPARC_HAL_R1		0x000400	/* HAL R1 extensions */

/* This name is used in the V9 ABI.  */
#define EF_SPARC_EXT_MASK	0xffff00	/* reserved for vendor extensions */

/* V9 memory models */
#define EF_SPARCV9_MM		0x3		/* memory model mask */
#define EF_SPARCV9_TSO		0x0		/* total store ordering */
#define EF_SPARCV9_PSO		0x1		/* partial store ordering */
#define EF_SPARCV9_RMO		0x2		/* relaxed store ordering */

/* Section indices.  */

#define SHN_BEFORE		0xff00		/* used with SHF_ORDERED */
#define SHN_AFTER		0xff01		/* used with SHF_ORDERED */

/* Section flags.  */

#define SHF_EXCLUDE		0x80000000	/* exclude from linking */
#define SHF_ORDERED		0x40000000	/* treat sh_link,sh_info specially */

/* Symbol types.  */

#define STT_REGISTER		13		/* global reg reserved to app. */

/* Relocation types.  */

enum elf_sparc_reloc_type {
    R_SPARC_NONE = 0,
    R_SPARC_8, R_SPARC_16, R_SPARC_32,
    R_SPARC_DISP8, R_SPARC_DISP16, R_SPARC_DISP32,
    R_SPARC_WDISP30, R_SPARC_WDISP22,
    R_SPARC_HI22, R_SPARC_22,
    R_SPARC_13, R_SPARC_LO10,
    R_SPARC_GOT10, R_SPARC_GOT13, R_SPARC_GOT22,
    R_SPARC_PC10, R_SPARC_PC22,
    R_SPARC_WPLT30,
    R_SPARC_COPY,
    R_SPARC_GLOB_DAT, R_SPARC_JMP_SLOT,
    R_SPARC_RELATIVE,
    R_SPARC_UA32,

    /* ??? These 6 relocs are new but not currently used.  For binary
       compatility in the sparc64-elf toolchain, we leave them out.
       A non-binary upward compatible change is expected for sparc64-elf.  */
#ifndef SPARC64_OLD_RELOCS
    /* ??? New relocs on the UltraSPARC.  Not sure what they're for yet.  */
    R_SPARC_PLT32, R_SPARC_HIPLT22, R_SPARC_LOPLT10,
    R_SPARC_PCPLT32, R_SPARC_PCPLT22, R_SPARC_PCPLT10,
#endif

    /* v9 relocs */
    R_SPARC_10, R_SPARC_11, R_SPARC_64,
    R_SPARC_OLO10, R_SPARC_HH22, R_SPARC_HM10, R_SPARC_LM22,
    R_SPARC_PC_HH22, R_SPARC_PC_HM10, R_SPARC_PC_LM22,
    R_SPARC_WDISP16, R_SPARC_WDISP19,
    R_SPARC_UNUSED_42,
    R_SPARC_7, R_SPARC_5, R_SPARC_6,
    R_SPARC_DISP64, R_SPARC_PLT64,
    R_SPARC_HIX22, R_SPARC_LOX10,
    R_SPARC_H44, R_SPARC_M44, R_SPARC_L44,
    R_SPARC_REGISTER,
    R_SPARC_UA64, R_SPARC_UA16,

    R_SPARC_max
};

/* Relocation macros.  */

#define ELF64_R_TYPE_DATA(info)		(((bfd_vma) (info) << 32) >> 40)
#define ELF64_R_TYPE_ID(info)		(((bfd_vma) (info) << 56) >> 56)
#define ELF64_R_TYPE_INFO(data, type)	(((bfd_vma) (data) << 8) \
					 + (bfd_vma) (type))

#define DT_SPARC_REGISTER	0x70000001

/*
 * FIXME: NOT ABI -- GET RID OF THIS
 * Defines the format used by the .plt.  Currently defined values are
 *   0 -- reserved to SI
 *   1 -- absolute address in .got.plt
 *   2 -- got-relative address in .got.plt
 */

#define DT_SPARC_PLTFMT		0x70000001

#endif /* _ELF_SPARC_H */
