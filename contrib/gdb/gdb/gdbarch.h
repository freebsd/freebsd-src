/* Architecture commands for GDB, the GNU debugger.
   Copyright 1998, Free Software Foundation, Inc.

This file is part of GDB.

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

#ifndef GDBARCH_H
#define GDBARCH_H


/* The target-system-dependant byte order is dynamic */

/* TARGET_BYTE_ORDER_SELECTABLE_P determines if the target endianness
   is selectable at runtime.  The user can use the `set endian'
   command to change it.  TARGET_BYTE_ORDER_AUTO is nonzero when
   target_byte_order should be auto-detected (from the program image
   say). */

#ifndef TARGET_BYTE_ORDER_SELECTABLE_P
/* compat - Catch old targets that define TARGET_BYTE_ORDER_SLECTABLE
   when they should have defined TARGET_BYTE_ORDER_SELECTABLE_P 1 */
#ifdef TARGET_BYTE_ORDER_SELECTABLE
#define TARGET_BYTE_ORDER_SELECTABLE_P 1
#else
#define TARGET_BYTE_ORDER_SELECTABLE_P 0
#endif
#endif

extern int target_byte_order;
#ifdef TARGET_BYTE_ORDER_SELECTABLE
/* compat - Catch old targets that define TARGET_BYTE_ORDER_SELECTABLE
   and expect defs.h to re-define TARGET_BYTE_ORDER. */
#undef TARGET_BYTE_ORDER
#endif
#ifndef TARGET_BYTE_ORDER
#define TARGET_BYTE_ORDER (target_byte_order + 0)
#endif

extern int target_byte_order_auto;
#ifndef TARGET_BYTE_ORDER_AUTO
#define TARGET_BYTE_ORDER_AUTO (target_byte_order_auto + 0)
#endif



/* The target-system-dependant BFD architecture is dynamic */

extern int target_architecture_auto;
#ifndef TARGET_ARCHITECTURE_AUTO
#define TARGET_ARCHITECTURE_AUTO (target_architecture_auto + 0)
#endif

extern const struct bfd_arch_info *target_architecture;
#ifndef TARGET_ARCHITECTURE
#define TARGET_ARCHITECTURE (target_architecture + 0)
#endif

/* Notify the target dependant backend of a change to the selected
   architecture. A zero return status indicates that the target did
   not like the change. */

extern int (*target_architecture_hook) PARAMS ((const struct bfd_arch_info *)); 



/* The target-system-dependant disassembler is semi-dynamic */

#include "dis-asm.h"		/* Get defs for disassemble_info */

extern int dis_asm_read_memory PARAMS ((bfd_vma memaddr, bfd_byte *myaddr,
					int len, disassemble_info *info));

extern void dis_asm_memory_error PARAMS ((int status, bfd_vma memaddr,
					  disassemble_info *info));

extern void dis_asm_print_address PARAMS ((bfd_vma addr,
					   disassemble_info *info));

extern int (*tm_print_insn) PARAMS ((bfd_vma, disassemble_info*));
extern disassemble_info tm_print_insn_info;
#ifndef TARGET_PRINT_INSN
#define TARGET_PRINT_INSN(vma, info) (*tm_print_insn) (vma, info)
#endif
#ifndef TARGET_PRINT_INSN_INFO
#define TARGET_PRINT_INSN_INFO (&tm_print_insn_info)
#endif



/* Set the dynamic target-system-dependant parameters (architecture,
   byte-order, ...) using information found in the BFD */

extern void set_gdbarch_from_file PARAMS ((bfd *));


/* Explicitly set the dynamic target-system-dependant parameters based
   on bfd_architecture and machine. */

extern void set_architecture_from_arch_mach PARAMS ((enum bfd_architecture, unsigned long));


/* gdbarch trace variable */
extern int gdbarch_debug;

#endif
