/* HPPA ELF support for BFD.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.

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

/* This file holds definitions specific to the HPPA ELF ABI.  Note
   that most of this is not actually implemented by BFD.  */

#ifndef _ELF_HPPA_H
#define _ELF_HPPA_H

/* Processor specific flags for the ELF header e_flags field.  */

/* Target processor IDs to be placed in the low 16 bits of the flags
   field.  Note these names are shared with SOM, and therefore do not
   follow ELF naming conventions.  */

/* PA 1.0 big endian.  */
#ifndef CPU_PA_RISC1_0
#define CPU_PA_RISC1_0		0x0000020b
#endif

/* PA 1.1 big endian.  */
#ifndef CPU_PA_RISC1_1
#define CPU_PA_RISC1_1		0x00000210
#endif

/* PA 1.0 little endian (unsupported) is 0x0000028b.  */
/* PA 1.1 little endian (unsupported) is 0x00000290.  */

/* Trap null address dereferences.  */
#define ELF_PARISC_TRAPNIL	0x00010000

/* .PARISC.archext section is present.  */
#define EF_PARISC_EXT		0x00020000

/* Processor specific section types.  */

/* Holds the global offset table, a table of pointers to external
   data.  */
#define SHT_PARISC_GOT		SHT_LOPROC+0

/* Nonloadable section containing information in architecture
   extensions used by the code.  */
#define SHT_PARISC_ARCH		SHT_LOPROC+1

/* Section in which $global$ is defined.  */
#define SHT_PARISC_GLOBAL	SHT_LOPROC+2

/* Section holding millicode routines (mul, div, rem, dyncall, etc.  */
#define SHT_PARISC_MILLI	SHT_LOPROC+3

/* Section holding unwind information for use by debuggers.  */
#define SHT_PARISC_UNWIND	SHT_LOPROC+4

/* Section holding the procedure linkage table.  */
#define SHT_PARISC_PLT		SHT_LOPROC+5

/* Short initialized and uninitialized data.  */
#define SHT_PARISC_SDATA	SHT_LOPROC+6
#define SHT_PARISC_SBSS		SHT_LOPROC+7

/* Optional section holding argument location/relocation info.  */
#define SHT_PARISC_SYMEXTN	SHT_LOPROC+8

/* Option section for linker stubs.  */
#define SHT_PARISC_STUBS	SHT_LOPROC+9

/* Processor specific section flags.  */

/* This section is near the global data pointer and thus allows short
   addressing modes to be used.  */
#define SHF_PARISC_SHORT        0x20000000

/* Processor specific symbol types.  */

/* Millicode function entry point.  */
#define STT_PARISC_MILLICODE	STT_LOPROC+0

#endif /* _ELF_HPPA_H */
