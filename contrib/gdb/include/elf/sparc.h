/* SPARC ELF support for BFD.
   Copyright (C) 1996 Free Software Foundation, Inc.

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
    R_SPARC_GLOB_JMP,
    R_SPARC_7,
#ifndef SPARC64_OLD_RELOCS
    R_SPARC_5, R_SPARC_6,
#endif

    R_SPARC_max
};

#endif /* _ELF_SPARC_H */
