/* *INDENT-OFF* */ /* THIS FILE IS GENERATED */

/* Dynamic architecture support for GDB, the GNU debugger.
   Copyright 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* This file was created with the aid of ``gdbarch.sh''.

   The Bourne shell script ``gdbarch.sh'' creates the files
   ``new-gdbarch.c'' and ``new-gdbarch.h and then compares them
   against the existing ``gdbarch.[hc]''.  Any differences found
   being reported.

   If editing this file, please also run gdbarch.sh and merge any
   changes into that script. Conversely, when making sweeping changes
   to this file, modifying gdbarch.sh and using its output may prove
   easier. */

#ifndef GDBARCH_H
#define GDBARCH_H

#include "dis-asm.h" /* Get defs for disassemble_info, which unfortunately is a typedef. */
#if !GDB_MULTI_ARCH
#include "value.h" /* For default_coerce_float_to_double which is referenced by a macro.  */
#endif

struct frame_info;
struct value;
struct objfile;
struct minimal_symbol;

extern struct gdbarch *current_gdbarch;


/* If any of the following are defined, the target wasn't correctly
   converted. */

#if GDB_MULTI_ARCH
#if defined (EXTRA_FRAME_INFO)
#error "EXTRA_FRAME_INFO: replaced by struct frame_extra_info"
#endif
#endif

#if GDB_MULTI_ARCH
#if defined (FRAME_FIND_SAVED_REGS)
#error "FRAME_FIND_SAVED_REGS: replaced by FRAME_INIT_SAVED_REGS"
#endif
#endif

#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PURE) && defined (GDB_TM_FILE)
#error "GDB_TM_FILE: Pure multi-arch targets do not have a tm.h file."
#endif


/* The following are pre-initialized by GDBARCH. */

extern const struct bfd_arch_info * gdbarch_bfd_arch_info (struct gdbarch *gdbarch);
/* set_gdbarch_bfd_arch_info() - not applicable - pre-initialized. */
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_ARCHITECTURE)
#error "Non multi-arch definition of TARGET_ARCHITECTURE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_ARCHITECTURE)
#define TARGET_ARCHITECTURE (gdbarch_bfd_arch_info (current_gdbarch))
#endif
#endif

extern int gdbarch_byte_order (struct gdbarch *gdbarch);
/* set_gdbarch_byte_order() - not applicable - pre-initialized. */
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_BYTE_ORDER)
#error "Non multi-arch definition of TARGET_BYTE_ORDER"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_BYTE_ORDER)
#define TARGET_BYTE_ORDER (gdbarch_byte_order (current_gdbarch))
#endif
#endif


/* The following are initialized by the target dependent code. */

/* Number of bits in a char or unsigned char for the target machine.
   Just like CHAR_BIT in <limits.h> but describes the target machine.
   v::TARGET_CHAR_BIT:int:char_bit::::8 * sizeof (char):8::0:
  
   Number of bits in a short or unsigned short for the target machine. */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_SHORT_BIT)
#define TARGET_SHORT_BIT (2*TARGET_CHAR_BIT)
#endif

extern int gdbarch_short_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_short_bit (struct gdbarch *gdbarch, int short_bit);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_SHORT_BIT)
#error "Non multi-arch definition of TARGET_SHORT_BIT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_SHORT_BIT)
#define TARGET_SHORT_BIT (gdbarch_short_bit (current_gdbarch))
#endif
#endif

/* Number of bits in an int or unsigned int for the target machine. */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_INT_BIT)
#define TARGET_INT_BIT (4*TARGET_CHAR_BIT)
#endif

extern int gdbarch_int_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_int_bit (struct gdbarch *gdbarch, int int_bit);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_INT_BIT)
#error "Non multi-arch definition of TARGET_INT_BIT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_INT_BIT)
#define TARGET_INT_BIT (gdbarch_int_bit (current_gdbarch))
#endif
#endif

/* Number of bits in a long or unsigned long for the target machine. */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_LONG_BIT)
#define TARGET_LONG_BIT (4*TARGET_CHAR_BIT)
#endif

extern int gdbarch_long_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_long_bit (struct gdbarch *gdbarch, int long_bit);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_LONG_BIT)
#error "Non multi-arch definition of TARGET_LONG_BIT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_LONG_BIT)
#define TARGET_LONG_BIT (gdbarch_long_bit (current_gdbarch))
#endif
#endif

/* Number of bits in a long long or unsigned long long for the target
   machine. */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_LONG_LONG_BIT)
#define TARGET_LONG_LONG_BIT (2*TARGET_LONG_BIT)
#endif

extern int gdbarch_long_long_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_long_long_bit (struct gdbarch *gdbarch, int long_long_bit);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_LONG_LONG_BIT)
#error "Non multi-arch definition of TARGET_LONG_LONG_BIT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_LONG_LONG_BIT)
#define TARGET_LONG_LONG_BIT (gdbarch_long_long_bit (current_gdbarch))
#endif
#endif

/* Number of bits in a float for the target machine. */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_FLOAT_BIT)
#define TARGET_FLOAT_BIT (4*TARGET_CHAR_BIT)
#endif

extern int gdbarch_float_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_float_bit (struct gdbarch *gdbarch, int float_bit);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_FLOAT_BIT)
#error "Non multi-arch definition of TARGET_FLOAT_BIT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_FLOAT_BIT)
#define TARGET_FLOAT_BIT (gdbarch_float_bit (current_gdbarch))
#endif
#endif

/* Number of bits in a double for the target machine. */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_DOUBLE_BIT)
#define TARGET_DOUBLE_BIT (8*TARGET_CHAR_BIT)
#endif

extern int gdbarch_double_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_double_bit (struct gdbarch *gdbarch, int double_bit);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_DOUBLE_BIT)
#error "Non multi-arch definition of TARGET_DOUBLE_BIT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_DOUBLE_BIT)
#define TARGET_DOUBLE_BIT (gdbarch_double_bit (current_gdbarch))
#endif
#endif

/* Number of bits in a long double for the target machine. */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_LONG_DOUBLE_BIT)
#define TARGET_LONG_DOUBLE_BIT (8*TARGET_CHAR_BIT)
#endif

extern int gdbarch_long_double_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_long_double_bit (struct gdbarch *gdbarch, int long_double_bit);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_LONG_DOUBLE_BIT)
#error "Non multi-arch definition of TARGET_LONG_DOUBLE_BIT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_LONG_DOUBLE_BIT)
#define TARGET_LONG_DOUBLE_BIT (gdbarch_long_double_bit (current_gdbarch))
#endif
#endif

/* For most targets, a pointer on the target and its representation as an
   address in GDB have the same size and "look the same".  For such a
   target, you need only set TARGET_PTR_BIT / ptr_bit and TARGET_ADDR_BIT
   / addr_bit will be set from it.
  
   If TARGET_PTR_BIT and TARGET_ADDR_BIT are different, you'll probably
   also need to set POINTER_TO_ADDRESS and ADDRESS_TO_POINTER as well.
  
   ptr_bit is the size of a pointer on the target */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_PTR_BIT)
#define TARGET_PTR_BIT (TARGET_INT_BIT)
#endif

extern int gdbarch_ptr_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_ptr_bit (struct gdbarch *gdbarch, int ptr_bit);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_PTR_BIT)
#error "Non multi-arch definition of TARGET_PTR_BIT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_PTR_BIT)
#define TARGET_PTR_BIT (gdbarch_ptr_bit (current_gdbarch))
#endif
#endif

/* addr_bit is the size of a target address as represented in gdb */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_ADDR_BIT)
#define TARGET_ADDR_BIT (TARGET_PTR_BIT)
#endif

extern int gdbarch_addr_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_addr_bit (struct gdbarch *gdbarch, int addr_bit);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_ADDR_BIT)
#error "Non multi-arch definition of TARGET_ADDR_BIT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_ADDR_BIT)
#define TARGET_ADDR_BIT (gdbarch_addr_bit (current_gdbarch))
#endif
#endif

/* Number of bits in a BFD_VMA for the target object file format. */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_BFD_VMA_BIT)
#define TARGET_BFD_VMA_BIT (TARGET_ARCHITECTURE->bits_per_address)
#endif

extern int gdbarch_bfd_vma_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_bfd_vma_bit (struct gdbarch *gdbarch, int bfd_vma_bit);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_BFD_VMA_BIT)
#error "Non multi-arch definition of TARGET_BFD_VMA_BIT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_BFD_VMA_BIT)
#define TARGET_BFD_VMA_BIT (gdbarch_bfd_vma_bit (current_gdbarch))
#endif
#endif

/* One if `char' acts like `signed char', zero if `unsigned char'. */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_CHAR_SIGNED)
#define TARGET_CHAR_SIGNED (1)
#endif

extern int gdbarch_char_signed (struct gdbarch *gdbarch);
extern void set_gdbarch_char_signed (struct gdbarch *gdbarch, int char_signed);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_CHAR_SIGNED)
#error "Non multi-arch definition of TARGET_CHAR_SIGNED"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_CHAR_SIGNED)
#define TARGET_CHAR_SIGNED (gdbarch_char_signed (current_gdbarch))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_READ_PC)
#define TARGET_READ_PC(ptid) (generic_target_read_pc (ptid))
#endif

typedef CORE_ADDR (gdbarch_read_pc_ftype) (ptid_t ptid);
extern CORE_ADDR gdbarch_read_pc (struct gdbarch *gdbarch, ptid_t ptid);
extern void set_gdbarch_read_pc (struct gdbarch *gdbarch, gdbarch_read_pc_ftype *read_pc);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_READ_PC)
#error "Non multi-arch definition of TARGET_READ_PC"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_READ_PC)
#define TARGET_READ_PC(ptid) (gdbarch_read_pc (current_gdbarch, ptid))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_WRITE_PC)
#define TARGET_WRITE_PC(val, ptid) (generic_target_write_pc (val, ptid))
#endif

typedef void (gdbarch_write_pc_ftype) (CORE_ADDR val, ptid_t ptid);
extern void gdbarch_write_pc (struct gdbarch *gdbarch, CORE_ADDR val, ptid_t ptid);
extern void set_gdbarch_write_pc (struct gdbarch *gdbarch, gdbarch_write_pc_ftype *write_pc);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_WRITE_PC)
#error "Non multi-arch definition of TARGET_WRITE_PC"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_WRITE_PC)
#define TARGET_WRITE_PC(val, ptid) (gdbarch_write_pc (current_gdbarch, val, ptid))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_READ_FP)
#define TARGET_READ_FP() (generic_target_read_fp ())
#endif

typedef CORE_ADDR (gdbarch_read_fp_ftype) (void);
extern CORE_ADDR gdbarch_read_fp (struct gdbarch *gdbarch);
extern void set_gdbarch_read_fp (struct gdbarch *gdbarch, gdbarch_read_fp_ftype *read_fp);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_READ_FP)
#error "Non multi-arch definition of TARGET_READ_FP"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_READ_FP)
#define TARGET_READ_FP() (gdbarch_read_fp (current_gdbarch))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_WRITE_FP)
#define TARGET_WRITE_FP(val) (generic_target_write_fp (val))
#endif

typedef void (gdbarch_write_fp_ftype) (CORE_ADDR val);
extern void gdbarch_write_fp (struct gdbarch *gdbarch, CORE_ADDR val);
extern void set_gdbarch_write_fp (struct gdbarch *gdbarch, gdbarch_write_fp_ftype *write_fp);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_WRITE_FP)
#error "Non multi-arch definition of TARGET_WRITE_FP"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_WRITE_FP)
#define TARGET_WRITE_FP(val) (gdbarch_write_fp (current_gdbarch, val))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_READ_SP)
#define TARGET_READ_SP() (generic_target_read_sp ())
#endif

typedef CORE_ADDR (gdbarch_read_sp_ftype) (void);
extern CORE_ADDR gdbarch_read_sp (struct gdbarch *gdbarch);
extern void set_gdbarch_read_sp (struct gdbarch *gdbarch, gdbarch_read_sp_ftype *read_sp);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_READ_SP)
#error "Non multi-arch definition of TARGET_READ_SP"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_READ_SP)
#define TARGET_READ_SP() (gdbarch_read_sp (current_gdbarch))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_WRITE_SP)
#define TARGET_WRITE_SP(val) (generic_target_write_sp (val))
#endif

typedef void (gdbarch_write_sp_ftype) (CORE_ADDR val);
extern void gdbarch_write_sp (struct gdbarch *gdbarch, CORE_ADDR val);
extern void set_gdbarch_write_sp (struct gdbarch *gdbarch, gdbarch_write_sp_ftype *write_sp);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_WRITE_SP)
#error "Non multi-arch definition of TARGET_WRITE_SP"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_WRITE_SP)
#define TARGET_WRITE_SP(val) (gdbarch_write_sp (current_gdbarch, val))
#endif
#endif

/* Function for getting target's idea of a frame pointer.  FIXME: GDB's
   whole scheme for dealing with "frames" and "frame pointers" needs a
   serious shakedown. */

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_VIRTUAL_FRAME_POINTER)
#define TARGET_VIRTUAL_FRAME_POINTER(pc, frame_regnum, frame_offset) (legacy_virtual_frame_pointer (pc, frame_regnum, frame_offset))
#endif

typedef void (gdbarch_virtual_frame_pointer_ftype) (CORE_ADDR pc, int *frame_regnum, LONGEST *frame_offset);
extern void gdbarch_virtual_frame_pointer (struct gdbarch *gdbarch, CORE_ADDR pc, int *frame_regnum, LONGEST *frame_offset);
extern void set_gdbarch_virtual_frame_pointer (struct gdbarch *gdbarch, gdbarch_virtual_frame_pointer_ftype *virtual_frame_pointer);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_VIRTUAL_FRAME_POINTER)
#error "Non multi-arch definition of TARGET_VIRTUAL_FRAME_POINTER"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_VIRTUAL_FRAME_POINTER)
#define TARGET_VIRTUAL_FRAME_POINTER(pc, frame_regnum, frame_offset) (gdbarch_virtual_frame_pointer (current_gdbarch, pc, frame_regnum, frame_offset))
#endif
#endif

extern int gdbarch_register_read_p (struct gdbarch *gdbarch);

typedef void (gdbarch_register_read_ftype) (struct gdbarch *gdbarch, int regnum, char *buf);
extern void gdbarch_register_read (struct gdbarch *gdbarch, int regnum, char *buf);
extern void set_gdbarch_register_read (struct gdbarch *gdbarch, gdbarch_register_read_ftype *register_read);

extern int gdbarch_register_write_p (struct gdbarch *gdbarch);

typedef void (gdbarch_register_write_ftype) (struct gdbarch *gdbarch, int regnum, char *buf);
extern void gdbarch_register_write (struct gdbarch *gdbarch, int regnum, char *buf);
extern void set_gdbarch_register_write (struct gdbarch *gdbarch, gdbarch_register_write_ftype *register_write);

extern int gdbarch_num_regs (struct gdbarch *gdbarch);
extern void set_gdbarch_num_regs (struct gdbarch *gdbarch, int num_regs);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (NUM_REGS)
#error "Non multi-arch definition of NUM_REGS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (NUM_REGS)
#define NUM_REGS (gdbarch_num_regs (current_gdbarch))
#endif
#endif

/* This macro gives the number of pseudo-registers that live in the
   register namespace but do not get fetched or stored on the target.
   These pseudo-registers may be aliases for other registers,
   combinations of other registers, or they may be computed by GDB. */

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (NUM_PSEUDO_REGS)
#define NUM_PSEUDO_REGS (0)
#endif

extern int gdbarch_num_pseudo_regs (struct gdbarch *gdbarch);
extern void set_gdbarch_num_pseudo_regs (struct gdbarch *gdbarch, int num_pseudo_regs);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (NUM_PSEUDO_REGS)
#error "Non multi-arch definition of NUM_PSEUDO_REGS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (NUM_PSEUDO_REGS)
#define NUM_PSEUDO_REGS (gdbarch_num_pseudo_regs (current_gdbarch))
#endif
#endif

extern int gdbarch_sp_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_sp_regnum (struct gdbarch *gdbarch, int sp_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SP_REGNUM)
#error "Non multi-arch definition of SP_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SP_REGNUM)
#define SP_REGNUM (gdbarch_sp_regnum (current_gdbarch))
#endif
#endif

extern int gdbarch_fp_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_fp_regnum (struct gdbarch *gdbarch, int fp_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FP_REGNUM)
#error "Non multi-arch definition of FP_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FP_REGNUM)
#define FP_REGNUM (gdbarch_fp_regnum (current_gdbarch))
#endif
#endif

extern int gdbarch_pc_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_pc_regnum (struct gdbarch *gdbarch, int pc_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (PC_REGNUM)
#error "Non multi-arch definition of PC_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (PC_REGNUM)
#define PC_REGNUM (gdbarch_pc_regnum (current_gdbarch))
#endif
#endif

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (FP0_REGNUM)
#define FP0_REGNUM (-1)
#endif

extern int gdbarch_fp0_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_fp0_regnum (struct gdbarch *gdbarch, int fp0_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FP0_REGNUM)
#error "Non multi-arch definition of FP0_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FP0_REGNUM)
#define FP0_REGNUM (gdbarch_fp0_regnum (current_gdbarch))
#endif
#endif

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (NPC_REGNUM)
#define NPC_REGNUM (-1)
#endif

extern int gdbarch_npc_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_npc_regnum (struct gdbarch *gdbarch, int npc_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (NPC_REGNUM)
#error "Non multi-arch definition of NPC_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (NPC_REGNUM)
#define NPC_REGNUM (gdbarch_npc_regnum (current_gdbarch))
#endif
#endif

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (NNPC_REGNUM)
#define NNPC_REGNUM (-1)
#endif

extern int gdbarch_nnpc_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_nnpc_regnum (struct gdbarch *gdbarch, int nnpc_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (NNPC_REGNUM)
#error "Non multi-arch definition of NNPC_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (NNPC_REGNUM)
#define NNPC_REGNUM (gdbarch_nnpc_regnum (current_gdbarch))
#endif
#endif

/* Convert stab register number (from `r' declaration) to a gdb REGNUM. */

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (STAB_REG_TO_REGNUM)
#define STAB_REG_TO_REGNUM(stab_regnr) (no_op_reg_to_regnum (stab_regnr))
#endif

typedef int (gdbarch_stab_reg_to_regnum_ftype) (int stab_regnr);
extern int gdbarch_stab_reg_to_regnum (struct gdbarch *gdbarch, int stab_regnr);
extern void set_gdbarch_stab_reg_to_regnum (struct gdbarch *gdbarch, gdbarch_stab_reg_to_regnum_ftype *stab_reg_to_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (STAB_REG_TO_REGNUM)
#error "Non multi-arch definition of STAB_REG_TO_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (STAB_REG_TO_REGNUM)
#define STAB_REG_TO_REGNUM(stab_regnr) (gdbarch_stab_reg_to_regnum (current_gdbarch, stab_regnr))
#endif
#endif

/* Provide a default mapping from a ecoff register number to a gdb REGNUM. */

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (ECOFF_REG_TO_REGNUM)
#define ECOFF_REG_TO_REGNUM(ecoff_regnr) (no_op_reg_to_regnum (ecoff_regnr))
#endif

typedef int (gdbarch_ecoff_reg_to_regnum_ftype) (int ecoff_regnr);
extern int gdbarch_ecoff_reg_to_regnum (struct gdbarch *gdbarch, int ecoff_regnr);
extern void set_gdbarch_ecoff_reg_to_regnum (struct gdbarch *gdbarch, gdbarch_ecoff_reg_to_regnum_ftype *ecoff_reg_to_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (ECOFF_REG_TO_REGNUM)
#error "Non multi-arch definition of ECOFF_REG_TO_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (ECOFF_REG_TO_REGNUM)
#define ECOFF_REG_TO_REGNUM(ecoff_regnr) (gdbarch_ecoff_reg_to_regnum (current_gdbarch, ecoff_regnr))
#endif
#endif

/* Provide a default mapping from a DWARF register number to a gdb REGNUM. */

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (DWARF_REG_TO_REGNUM)
#define DWARF_REG_TO_REGNUM(dwarf_regnr) (no_op_reg_to_regnum (dwarf_regnr))
#endif

typedef int (gdbarch_dwarf_reg_to_regnum_ftype) (int dwarf_regnr);
extern int gdbarch_dwarf_reg_to_regnum (struct gdbarch *gdbarch, int dwarf_regnr);
extern void set_gdbarch_dwarf_reg_to_regnum (struct gdbarch *gdbarch, gdbarch_dwarf_reg_to_regnum_ftype *dwarf_reg_to_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (DWARF_REG_TO_REGNUM)
#error "Non multi-arch definition of DWARF_REG_TO_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (DWARF_REG_TO_REGNUM)
#define DWARF_REG_TO_REGNUM(dwarf_regnr) (gdbarch_dwarf_reg_to_regnum (current_gdbarch, dwarf_regnr))
#endif
#endif

/* Convert from an sdb register number to an internal gdb register number.
   This should be defined in tm.h, if REGISTER_NAMES is not set up
   to map one to one onto the sdb register numbers. */

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (SDB_REG_TO_REGNUM)
#define SDB_REG_TO_REGNUM(sdb_regnr) (no_op_reg_to_regnum (sdb_regnr))
#endif

typedef int (gdbarch_sdb_reg_to_regnum_ftype) (int sdb_regnr);
extern int gdbarch_sdb_reg_to_regnum (struct gdbarch *gdbarch, int sdb_regnr);
extern void set_gdbarch_sdb_reg_to_regnum (struct gdbarch *gdbarch, gdbarch_sdb_reg_to_regnum_ftype *sdb_reg_to_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SDB_REG_TO_REGNUM)
#error "Non multi-arch definition of SDB_REG_TO_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SDB_REG_TO_REGNUM)
#define SDB_REG_TO_REGNUM(sdb_regnr) (gdbarch_sdb_reg_to_regnum (current_gdbarch, sdb_regnr))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (DWARF2_REG_TO_REGNUM)
#define DWARF2_REG_TO_REGNUM(dwarf2_regnr) (no_op_reg_to_regnum (dwarf2_regnr))
#endif

typedef int (gdbarch_dwarf2_reg_to_regnum_ftype) (int dwarf2_regnr);
extern int gdbarch_dwarf2_reg_to_regnum (struct gdbarch *gdbarch, int dwarf2_regnr);
extern void set_gdbarch_dwarf2_reg_to_regnum (struct gdbarch *gdbarch, gdbarch_dwarf2_reg_to_regnum_ftype *dwarf2_reg_to_regnum);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (DWARF2_REG_TO_REGNUM)
#error "Non multi-arch definition of DWARF2_REG_TO_REGNUM"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (DWARF2_REG_TO_REGNUM)
#define DWARF2_REG_TO_REGNUM(dwarf2_regnr) (gdbarch_dwarf2_reg_to_regnum (current_gdbarch, dwarf2_regnr))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (REGISTER_NAME)
#define REGISTER_NAME(regnr) (legacy_register_name (regnr))
#endif

typedef char * (gdbarch_register_name_ftype) (int regnr);
extern char * gdbarch_register_name (struct gdbarch *gdbarch, int regnr);
extern void set_gdbarch_register_name (struct gdbarch *gdbarch, gdbarch_register_name_ftype *register_name);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_NAME)
#error "Non multi-arch definition of REGISTER_NAME"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_NAME)
#define REGISTER_NAME(regnr) (gdbarch_register_name (current_gdbarch, regnr))
#endif
#endif

extern int gdbarch_register_size (struct gdbarch *gdbarch);
extern void set_gdbarch_register_size (struct gdbarch *gdbarch, int register_size);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_SIZE)
#error "Non multi-arch definition of REGISTER_SIZE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_SIZE)
#define REGISTER_SIZE (gdbarch_register_size (current_gdbarch))
#endif
#endif

extern int gdbarch_register_bytes (struct gdbarch *gdbarch);
extern void set_gdbarch_register_bytes (struct gdbarch *gdbarch, int register_bytes);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_BYTES)
#error "Non multi-arch definition of REGISTER_BYTES"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_BYTES)
#define REGISTER_BYTES (gdbarch_register_bytes (current_gdbarch))
#endif
#endif

typedef int (gdbarch_register_byte_ftype) (int reg_nr);
extern int gdbarch_register_byte (struct gdbarch *gdbarch, int reg_nr);
extern void set_gdbarch_register_byte (struct gdbarch *gdbarch, gdbarch_register_byte_ftype *register_byte);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_BYTE)
#error "Non multi-arch definition of REGISTER_BYTE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_BYTE)
#define REGISTER_BYTE(reg_nr) (gdbarch_register_byte (current_gdbarch, reg_nr))
#endif
#endif

typedef int (gdbarch_register_raw_size_ftype) (int reg_nr);
extern int gdbarch_register_raw_size (struct gdbarch *gdbarch, int reg_nr);
extern void set_gdbarch_register_raw_size (struct gdbarch *gdbarch, gdbarch_register_raw_size_ftype *register_raw_size);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_RAW_SIZE)
#error "Non multi-arch definition of REGISTER_RAW_SIZE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_RAW_SIZE)
#define REGISTER_RAW_SIZE(reg_nr) (gdbarch_register_raw_size (current_gdbarch, reg_nr))
#endif
#endif

extern int gdbarch_max_register_raw_size (struct gdbarch *gdbarch);
extern void set_gdbarch_max_register_raw_size (struct gdbarch *gdbarch, int max_register_raw_size);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (MAX_REGISTER_RAW_SIZE)
#error "Non multi-arch definition of MAX_REGISTER_RAW_SIZE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (MAX_REGISTER_RAW_SIZE)
#define MAX_REGISTER_RAW_SIZE (gdbarch_max_register_raw_size (current_gdbarch))
#endif
#endif

typedef int (gdbarch_register_virtual_size_ftype) (int reg_nr);
extern int gdbarch_register_virtual_size (struct gdbarch *gdbarch, int reg_nr);
extern void set_gdbarch_register_virtual_size (struct gdbarch *gdbarch, gdbarch_register_virtual_size_ftype *register_virtual_size);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_VIRTUAL_SIZE)
#error "Non multi-arch definition of REGISTER_VIRTUAL_SIZE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_VIRTUAL_SIZE)
#define REGISTER_VIRTUAL_SIZE(reg_nr) (gdbarch_register_virtual_size (current_gdbarch, reg_nr))
#endif
#endif

extern int gdbarch_max_register_virtual_size (struct gdbarch *gdbarch);
extern void set_gdbarch_max_register_virtual_size (struct gdbarch *gdbarch, int max_register_virtual_size);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (MAX_REGISTER_VIRTUAL_SIZE)
#error "Non multi-arch definition of MAX_REGISTER_VIRTUAL_SIZE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (MAX_REGISTER_VIRTUAL_SIZE)
#define MAX_REGISTER_VIRTUAL_SIZE (gdbarch_max_register_virtual_size (current_gdbarch))
#endif
#endif

typedef struct type * (gdbarch_register_virtual_type_ftype) (int reg_nr);
extern struct type * gdbarch_register_virtual_type (struct gdbarch *gdbarch, int reg_nr);
extern void set_gdbarch_register_virtual_type (struct gdbarch *gdbarch, gdbarch_register_virtual_type_ftype *register_virtual_type);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_VIRTUAL_TYPE)
#error "Non multi-arch definition of REGISTER_VIRTUAL_TYPE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_VIRTUAL_TYPE)
#define REGISTER_VIRTUAL_TYPE(reg_nr) (gdbarch_register_virtual_type (current_gdbarch, reg_nr))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (DO_REGISTERS_INFO)
#define DO_REGISTERS_INFO(reg_nr, fpregs) (do_registers_info (reg_nr, fpregs))
#endif

typedef void (gdbarch_do_registers_info_ftype) (int reg_nr, int fpregs);
extern void gdbarch_do_registers_info (struct gdbarch *gdbarch, int reg_nr, int fpregs);
extern void set_gdbarch_do_registers_info (struct gdbarch *gdbarch, gdbarch_do_registers_info_ftype *do_registers_info);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (DO_REGISTERS_INFO)
#error "Non multi-arch definition of DO_REGISTERS_INFO"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (DO_REGISTERS_INFO)
#define DO_REGISTERS_INFO(reg_nr, fpregs) (gdbarch_do_registers_info (current_gdbarch, reg_nr, fpregs))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (PRINT_FLOAT_INFO)
#define PRINT_FLOAT_INFO() (default_print_float_info ())
#endif

typedef void (gdbarch_print_float_info_ftype) (void);
extern void gdbarch_print_float_info (struct gdbarch *gdbarch);
extern void set_gdbarch_print_float_info (struct gdbarch *gdbarch, gdbarch_print_float_info_ftype *print_float_info);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (PRINT_FLOAT_INFO)
#error "Non multi-arch definition of PRINT_FLOAT_INFO"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (PRINT_FLOAT_INFO)
#define PRINT_FLOAT_INFO() (gdbarch_print_float_info (current_gdbarch))
#endif
#endif

/* MAP a GDB RAW register number onto a simulator register number.  See
   also include/...-sim.h. */

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (REGISTER_SIM_REGNO)
#define REGISTER_SIM_REGNO(reg_nr) (default_register_sim_regno (reg_nr))
#endif

typedef int (gdbarch_register_sim_regno_ftype) (int reg_nr);
extern int gdbarch_register_sim_regno (struct gdbarch *gdbarch, int reg_nr);
extern void set_gdbarch_register_sim_regno (struct gdbarch *gdbarch, gdbarch_register_sim_regno_ftype *register_sim_regno);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_SIM_REGNO)
#error "Non multi-arch definition of REGISTER_SIM_REGNO"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_SIM_REGNO)
#define REGISTER_SIM_REGNO(reg_nr) (gdbarch_register_sim_regno (current_gdbarch, reg_nr))
#endif
#endif

#if defined (REGISTER_BYTES_OK)
/* Legacy for systems yet to multi-arch REGISTER_BYTES_OK */
#if !defined (REGISTER_BYTES_OK_P)
#define REGISTER_BYTES_OK_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (REGISTER_BYTES_OK_P)
#define REGISTER_BYTES_OK_P() (0)
#endif

extern int gdbarch_register_bytes_ok_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_BYTES_OK_P)
#error "Non multi-arch definition of REGISTER_BYTES_OK"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_BYTES_OK_P)
#define REGISTER_BYTES_OK_P() (gdbarch_register_bytes_ok_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (REGISTER_BYTES_OK)
#define REGISTER_BYTES_OK(nr_bytes) (internal_error (__FILE__, __LINE__, "REGISTER_BYTES_OK"), 0)
#endif

typedef int (gdbarch_register_bytes_ok_ftype) (long nr_bytes);
extern int gdbarch_register_bytes_ok (struct gdbarch *gdbarch, long nr_bytes);
extern void set_gdbarch_register_bytes_ok (struct gdbarch *gdbarch, gdbarch_register_bytes_ok_ftype *register_bytes_ok);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_BYTES_OK)
#error "Non multi-arch definition of REGISTER_BYTES_OK"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_BYTES_OK)
#define REGISTER_BYTES_OK(nr_bytes) (gdbarch_register_bytes_ok (current_gdbarch, nr_bytes))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (CANNOT_FETCH_REGISTER)
#define CANNOT_FETCH_REGISTER(regnum) (cannot_register_not (regnum))
#endif

typedef int (gdbarch_cannot_fetch_register_ftype) (int regnum);
extern int gdbarch_cannot_fetch_register (struct gdbarch *gdbarch, int regnum);
extern void set_gdbarch_cannot_fetch_register (struct gdbarch *gdbarch, gdbarch_cannot_fetch_register_ftype *cannot_fetch_register);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (CANNOT_FETCH_REGISTER)
#error "Non multi-arch definition of CANNOT_FETCH_REGISTER"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (CANNOT_FETCH_REGISTER)
#define CANNOT_FETCH_REGISTER(regnum) (gdbarch_cannot_fetch_register (current_gdbarch, regnum))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (CANNOT_STORE_REGISTER)
#define CANNOT_STORE_REGISTER(regnum) (cannot_register_not (regnum))
#endif

typedef int (gdbarch_cannot_store_register_ftype) (int regnum);
extern int gdbarch_cannot_store_register (struct gdbarch *gdbarch, int regnum);
extern void set_gdbarch_cannot_store_register (struct gdbarch *gdbarch, gdbarch_cannot_store_register_ftype *cannot_store_register);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (CANNOT_STORE_REGISTER)
#error "Non multi-arch definition of CANNOT_STORE_REGISTER"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (CANNOT_STORE_REGISTER)
#define CANNOT_STORE_REGISTER(regnum) (gdbarch_cannot_store_register (current_gdbarch, regnum))
#endif
#endif

/* setjmp/longjmp support. */

#if defined (GET_LONGJMP_TARGET)
/* Legacy for systems yet to multi-arch GET_LONGJMP_TARGET */
#if !defined (GET_LONGJMP_TARGET_P)
#define GET_LONGJMP_TARGET_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (GET_LONGJMP_TARGET_P)
#define GET_LONGJMP_TARGET_P() (0)
#endif

extern int gdbarch_get_longjmp_target_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (GET_LONGJMP_TARGET_P)
#error "Non multi-arch definition of GET_LONGJMP_TARGET"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (GET_LONGJMP_TARGET_P)
#define GET_LONGJMP_TARGET_P() (gdbarch_get_longjmp_target_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (GET_LONGJMP_TARGET)
#define GET_LONGJMP_TARGET(pc) (internal_error (__FILE__, __LINE__, "GET_LONGJMP_TARGET"), 0)
#endif

typedef int (gdbarch_get_longjmp_target_ftype) (CORE_ADDR *pc);
extern int gdbarch_get_longjmp_target (struct gdbarch *gdbarch, CORE_ADDR *pc);
extern void set_gdbarch_get_longjmp_target (struct gdbarch *gdbarch, gdbarch_get_longjmp_target_ftype *get_longjmp_target);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (GET_LONGJMP_TARGET)
#error "Non multi-arch definition of GET_LONGJMP_TARGET"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (GET_LONGJMP_TARGET)
#define GET_LONGJMP_TARGET(pc) (gdbarch_get_longjmp_target (current_gdbarch, pc))
#endif
#endif

/* Non multi-arch DUMMY_FRAMES are a mess (multi-arch ones are not that
   much better but at least they are vaguely consistent).  The headers
   and body contain convoluted #if/#else sequences for determine how
   things should be compiled.  Instead of trying to mimic that
   behaviour here (and hence entrench it further) gdbarch simply
   reqires that these methods be set up from the word go.  This also
   avoids any potential problems with moving beyond multi-arch partial. */

extern int gdbarch_use_generic_dummy_frames (struct gdbarch *gdbarch);
extern void set_gdbarch_use_generic_dummy_frames (struct gdbarch *gdbarch, int use_generic_dummy_frames);
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) && defined (USE_GENERIC_DUMMY_FRAMES)
#error "Non multi-arch definition of USE_GENERIC_DUMMY_FRAMES"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) || !defined (USE_GENERIC_DUMMY_FRAMES)
#define USE_GENERIC_DUMMY_FRAMES (gdbarch_use_generic_dummy_frames (current_gdbarch))
#endif
#endif

extern int gdbarch_call_dummy_location (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_location (struct gdbarch *gdbarch, int call_dummy_location);
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) && defined (CALL_DUMMY_LOCATION)
#error "Non multi-arch definition of CALL_DUMMY_LOCATION"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) || !defined (CALL_DUMMY_LOCATION)
#define CALL_DUMMY_LOCATION (gdbarch_call_dummy_location (current_gdbarch))
#endif
#endif

typedef CORE_ADDR (gdbarch_call_dummy_address_ftype) (void);
extern CORE_ADDR gdbarch_call_dummy_address (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_address (struct gdbarch *gdbarch, gdbarch_call_dummy_address_ftype *call_dummy_address);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (CALL_DUMMY_ADDRESS)
#error "Non multi-arch definition of CALL_DUMMY_ADDRESS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (CALL_DUMMY_ADDRESS)
#define CALL_DUMMY_ADDRESS() (gdbarch_call_dummy_address (current_gdbarch))
#endif
#endif

extern CORE_ADDR gdbarch_call_dummy_start_offset (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_start_offset (struct gdbarch *gdbarch, CORE_ADDR call_dummy_start_offset);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (CALL_DUMMY_START_OFFSET)
#error "Non multi-arch definition of CALL_DUMMY_START_OFFSET"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (CALL_DUMMY_START_OFFSET)
#define CALL_DUMMY_START_OFFSET (gdbarch_call_dummy_start_offset (current_gdbarch))
#endif
#endif

extern CORE_ADDR gdbarch_call_dummy_breakpoint_offset (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_breakpoint_offset (struct gdbarch *gdbarch, CORE_ADDR call_dummy_breakpoint_offset);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (CALL_DUMMY_BREAKPOINT_OFFSET)
#error "Non multi-arch definition of CALL_DUMMY_BREAKPOINT_OFFSET"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (CALL_DUMMY_BREAKPOINT_OFFSET)
#define CALL_DUMMY_BREAKPOINT_OFFSET (gdbarch_call_dummy_breakpoint_offset (current_gdbarch))
#endif
#endif

extern int gdbarch_call_dummy_breakpoint_offset_p (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_breakpoint_offset_p (struct gdbarch *gdbarch, int call_dummy_breakpoint_offset_p);
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) && defined (CALL_DUMMY_BREAKPOINT_OFFSET_P)
#error "Non multi-arch definition of CALL_DUMMY_BREAKPOINT_OFFSET_P"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) || !defined (CALL_DUMMY_BREAKPOINT_OFFSET_P)
#define CALL_DUMMY_BREAKPOINT_OFFSET_P (gdbarch_call_dummy_breakpoint_offset_p (current_gdbarch))
#endif
#endif

extern int gdbarch_call_dummy_length (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_length (struct gdbarch *gdbarch, int call_dummy_length);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (CALL_DUMMY_LENGTH)
#error "Non multi-arch definition of CALL_DUMMY_LENGTH"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (CALL_DUMMY_LENGTH)
#define CALL_DUMMY_LENGTH (gdbarch_call_dummy_length (current_gdbarch))
#endif
#endif

typedef int (gdbarch_pc_in_call_dummy_ftype) (CORE_ADDR pc, CORE_ADDR sp, CORE_ADDR frame_address);
extern int gdbarch_pc_in_call_dummy (struct gdbarch *gdbarch, CORE_ADDR pc, CORE_ADDR sp, CORE_ADDR frame_address);
extern void set_gdbarch_pc_in_call_dummy (struct gdbarch *gdbarch, gdbarch_pc_in_call_dummy_ftype *pc_in_call_dummy);
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) && defined (PC_IN_CALL_DUMMY)
#error "Non multi-arch definition of PC_IN_CALL_DUMMY"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) || !defined (PC_IN_CALL_DUMMY)
#define PC_IN_CALL_DUMMY(pc, sp, frame_address) (gdbarch_pc_in_call_dummy (current_gdbarch, pc, sp, frame_address))
#endif
#endif

extern int gdbarch_call_dummy_p (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_p (struct gdbarch *gdbarch, int call_dummy_p);
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) && defined (CALL_DUMMY_P)
#error "Non multi-arch definition of CALL_DUMMY_P"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) || !defined (CALL_DUMMY_P)
#define CALL_DUMMY_P (gdbarch_call_dummy_p (current_gdbarch))
#endif
#endif

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (CALL_DUMMY_WORDS)
#define CALL_DUMMY_WORDS (legacy_call_dummy_words)
#endif

extern LONGEST * gdbarch_call_dummy_words (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_words (struct gdbarch *gdbarch, LONGEST * call_dummy_words);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (CALL_DUMMY_WORDS)
#error "Non multi-arch definition of CALL_DUMMY_WORDS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (CALL_DUMMY_WORDS)
#define CALL_DUMMY_WORDS (gdbarch_call_dummy_words (current_gdbarch))
#endif
#endif

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (SIZEOF_CALL_DUMMY_WORDS)
#define SIZEOF_CALL_DUMMY_WORDS (legacy_sizeof_call_dummy_words)
#endif

extern int gdbarch_sizeof_call_dummy_words (struct gdbarch *gdbarch);
extern void set_gdbarch_sizeof_call_dummy_words (struct gdbarch *gdbarch, int sizeof_call_dummy_words);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SIZEOF_CALL_DUMMY_WORDS)
#error "Non multi-arch definition of SIZEOF_CALL_DUMMY_WORDS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SIZEOF_CALL_DUMMY_WORDS)
#define SIZEOF_CALL_DUMMY_WORDS (gdbarch_sizeof_call_dummy_words (current_gdbarch))
#endif
#endif

extern int gdbarch_call_dummy_stack_adjust_p (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_stack_adjust_p (struct gdbarch *gdbarch, int call_dummy_stack_adjust_p);
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) && defined (CALL_DUMMY_STACK_ADJUST_P)
#error "Non multi-arch definition of CALL_DUMMY_STACK_ADJUST_P"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) || !defined (CALL_DUMMY_STACK_ADJUST_P)
#define CALL_DUMMY_STACK_ADJUST_P (gdbarch_call_dummy_stack_adjust_p (current_gdbarch))
#endif
#endif

extern int gdbarch_call_dummy_stack_adjust (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_stack_adjust (struct gdbarch *gdbarch, int call_dummy_stack_adjust);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (CALL_DUMMY_STACK_ADJUST)
#error "Non multi-arch definition of CALL_DUMMY_STACK_ADJUST"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (CALL_DUMMY_STACK_ADJUST)
#define CALL_DUMMY_STACK_ADJUST (gdbarch_call_dummy_stack_adjust (current_gdbarch))
#endif
#endif

typedef void (gdbarch_fix_call_dummy_ftype) (char *dummy, CORE_ADDR pc, CORE_ADDR fun, int nargs, struct value **args, struct type *type, int gcc_p);
extern void gdbarch_fix_call_dummy (struct gdbarch *gdbarch, char *dummy, CORE_ADDR pc, CORE_ADDR fun, int nargs, struct value **args, struct type *type, int gcc_p);
extern void set_gdbarch_fix_call_dummy (struct gdbarch *gdbarch, gdbarch_fix_call_dummy_ftype *fix_call_dummy);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FIX_CALL_DUMMY)
#error "Non multi-arch definition of FIX_CALL_DUMMY"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FIX_CALL_DUMMY)
#define FIX_CALL_DUMMY(dummy, pc, fun, nargs, args, type, gcc_p) (gdbarch_fix_call_dummy (current_gdbarch, dummy, pc, fun, nargs, args, type, gcc_p))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (INIT_FRAME_PC_FIRST)
#define INIT_FRAME_PC_FIRST(fromleaf, prev) (init_frame_pc_noop (fromleaf, prev))
#endif

typedef void (gdbarch_init_frame_pc_first_ftype) (int fromleaf, struct frame_info *prev);
extern void gdbarch_init_frame_pc_first (struct gdbarch *gdbarch, int fromleaf, struct frame_info *prev);
extern void set_gdbarch_init_frame_pc_first (struct gdbarch *gdbarch, gdbarch_init_frame_pc_first_ftype *init_frame_pc_first);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (INIT_FRAME_PC_FIRST)
#error "Non multi-arch definition of INIT_FRAME_PC_FIRST"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (INIT_FRAME_PC_FIRST)
#define INIT_FRAME_PC_FIRST(fromleaf, prev) (gdbarch_init_frame_pc_first (current_gdbarch, fromleaf, prev))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (INIT_FRAME_PC)
#define INIT_FRAME_PC(fromleaf, prev) (init_frame_pc_default (fromleaf, prev))
#endif

typedef void (gdbarch_init_frame_pc_ftype) (int fromleaf, struct frame_info *prev);
extern void gdbarch_init_frame_pc (struct gdbarch *gdbarch, int fromleaf, struct frame_info *prev);
extern void set_gdbarch_init_frame_pc (struct gdbarch *gdbarch, gdbarch_init_frame_pc_ftype *init_frame_pc);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (INIT_FRAME_PC)
#error "Non multi-arch definition of INIT_FRAME_PC"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (INIT_FRAME_PC)
#define INIT_FRAME_PC(fromleaf, prev) (gdbarch_init_frame_pc (current_gdbarch, fromleaf, prev))
#endif
#endif

extern int gdbarch_believe_pcc_promotion (struct gdbarch *gdbarch);
extern void set_gdbarch_believe_pcc_promotion (struct gdbarch *gdbarch, int believe_pcc_promotion);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (BELIEVE_PCC_PROMOTION)
#error "Non multi-arch definition of BELIEVE_PCC_PROMOTION"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (BELIEVE_PCC_PROMOTION)
#define BELIEVE_PCC_PROMOTION (gdbarch_believe_pcc_promotion (current_gdbarch))
#endif
#endif

extern int gdbarch_believe_pcc_promotion_type (struct gdbarch *gdbarch);
extern void set_gdbarch_believe_pcc_promotion_type (struct gdbarch *gdbarch, int believe_pcc_promotion_type);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (BELIEVE_PCC_PROMOTION_TYPE)
#error "Non multi-arch definition of BELIEVE_PCC_PROMOTION_TYPE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (BELIEVE_PCC_PROMOTION_TYPE)
#define BELIEVE_PCC_PROMOTION_TYPE (gdbarch_believe_pcc_promotion_type (current_gdbarch))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (COERCE_FLOAT_TO_DOUBLE)
#define COERCE_FLOAT_TO_DOUBLE(formal, actual) (default_coerce_float_to_double (formal, actual))
#endif

typedef int (gdbarch_coerce_float_to_double_ftype) (struct type *formal, struct type *actual);
extern int gdbarch_coerce_float_to_double (struct gdbarch *gdbarch, struct type *formal, struct type *actual);
extern void set_gdbarch_coerce_float_to_double (struct gdbarch *gdbarch, gdbarch_coerce_float_to_double_ftype *coerce_float_to_double);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (COERCE_FLOAT_TO_DOUBLE)
#error "Non multi-arch definition of COERCE_FLOAT_TO_DOUBLE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (COERCE_FLOAT_TO_DOUBLE)
#define COERCE_FLOAT_TO_DOUBLE(formal, actual) (gdbarch_coerce_float_to_double (current_gdbarch, formal, actual))
#endif
#endif

/* GET_SAVED_REGISTER is like DUMMY_FRAMES.  It is at level one as the
   old code has strange #ifdef interaction.  So far no one has found
   that default_get_saved_register() is the default they are after. */

typedef void (gdbarch_get_saved_register_ftype) (char *raw_buffer, int *optimized, CORE_ADDR *addrp, struct frame_info *frame, int regnum, enum lval_type *lval);
extern void gdbarch_get_saved_register (struct gdbarch *gdbarch, char *raw_buffer, int *optimized, CORE_ADDR *addrp, struct frame_info *frame, int regnum, enum lval_type *lval);
extern void set_gdbarch_get_saved_register (struct gdbarch *gdbarch, gdbarch_get_saved_register_ftype *get_saved_register);
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) && defined (GET_SAVED_REGISTER)
#error "Non multi-arch definition of GET_SAVED_REGISTER"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH >= GDB_MULTI_ARCH_PARTIAL) || !defined (GET_SAVED_REGISTER)
#define GET_SAVED_REGISTER(raw_buffer, optimized, addrp, frame, regnum, lval) (gdbarch_get_saved_register (current_gdbarch, raw_buffer, optimized, addrp, frame, regnum, lval))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (REGISTER_CONVERTIBLE)
#define REGISTER_CONVERTIBLE(nr) (generic_register_convertible_not (nr))
#endif

typedef int (gdbarch_register_convertible_ftype) (int nr);
extern int gdbarch_register_convertible (struct gdbarch *gdbarch, int nr);
extern void set_gdbarch_register_convertible (struct gdbarch *gdbarch, gdbarch_register_convertible_ftype *register_convertible);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_CONVERTIBLE)
#error "Non multi-arch definition of REGISTER_CONVERTIBLE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_CONVERTIBLE)
#define REGISTER_CONVERTIBLE(nr) (gdbarch_register_convertible (current_gdbarch, nr))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (REGISTER_CONVERT_TO_VIRTUAL)
#define REGISTER_CONVERT_TO_VIRTUAL(regnum, type, from, to) (internal_error (__FILE__, __LINE__, "REGISTER_CONVERT_TO_VIRTUAL"), 0)
#endif

typedef void (gdbarch_register_convert_to_virtual_ftype) (int regnum, struct type *type, char *from, char *to);
extern void gdbarch_register_convert_to_virtual (struct gdbarch *gdbarch, int regnum, struct type *type, char *from, char *to);
extern void set_gdbarch_register_convert_to_virtual (struct gdbarch *gdbarch, gdbarch_register_convert_to_virtual_ftype *register_convert_to_virtual);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_CONVERT_TO_VIRTUAL)
#error "Non multi-arch definition of REGISTER_CONVERT_TO_VIRTUAL"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_CONVERT_TO_VIRTUAL)
#define REGISTER_CONVERT_TO_VIRTUAL(regnum, type, from, to) (gdbarch_register_convert_to_virtual (current_gdbarch, regnum, type, from, to))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (REGISTER_CONVERT_TO_RAW)
#define REGISTER_CONVERT_TO_RAW(type, regnum, from, to) (internal_error (__FILE__, __LINE__, "REGISTER_CONVERT_TO_RAW"), 0)
#endif

typedef void (gdbarch_register_convert_to_raw_ftype) (struct type *type, int regnum, char *from, char *to);
extern void gdbarch_register_convert_to_raw (struct gdbarch *gdbarch, struct type *type, int regnum, char *from, char *to);
extern void set_gdbarch_register_convert_to_raw (struct gdbarch *gdbarch, gdbarch_register_convert_to_raw_ftype *register_convert_to_raw);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REGISTER_CONVERT_TO_RAW)
#error "Non multi-arch definition of REGISTER_CONVERT_TO_RAW"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REGISTER_CONVERT_TO_RAW)
#define REGISTER_CONVERT_TO_RAW(type, regnum, from, to) (gdbarch_register_convert_to_raw (current_gdbarch, type, regnum, from, to))
#endif
#endif

/* This function is called when the value of a pseudo-register needs to
   be updated.  Typically it will be defined on a per-architecture
   basis. */

#if defined (FETCH_PSEUDO_REGISTER)
/* Legacy for systems yet to multi-arch FETCH_PSEUDO_REGISTER */
#if !defined (FETCH_PSEUDO_REGISTER_P)
#define FETCH_PSEUDO_REGISTER_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (FETCH_PSEUDO_REGISTER_P)
#define FETCH_PSEUDO_REGISTER_P() (0)
#endif

extern int gdbarch_fetch_pseudo_register_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FETCH_PSEUDO_REGISTER_P)
#error "Non multi-arch definition of FETCH_PSEUDO_REGISTER"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FETCH_PSEUDO_REGISTER_P)
#define FETCH_PSEUDO_REGISTER_P() (gdbarch_fetch_pseudo_register_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (FETCH_PSEUDO_REGISTER)
#define FETCH_PSEUDO_REGISTER(regnum) (internal_error (__FILE__, __LINE__, "FETCH_PSEUDO_REGISTER"), 0)
#endif

typedef void (gdbarch_fetch_pseudo_register_ftype) (int regnum);
extern void gdbarch_fetch_pseudo_register (struct gdbarch *gdbarch, int regnum);
extern void set_gdbarch_fetch_pseudo_register (struct gdbarch *gdbarch, gdbarch_fetch_pseudo_register_ftype *fetch_pseudo_register);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FETCH_PSEUDO_REGISTER)
#error "Non multi-arch definition of FETCH_PSEUDO_REGISTER"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FETCH_PSEUDO_REGISTER)
#define FETCH_PSEUDO_REGISTER(regnum) (gdbarch_fetch_pseudo_register (current_gdbarch, regnum))
#endif
#endif

/* This function is called when the value of a pseudo-register needs to
   be set or stored.  Typically it will be defined on a
   per-architecture basis. */

#if defined (STORE_PSEUDO_REGISTER)
/* Legacy for systems yet to multi-arch STORE_PSEUDO_REGISTER */
#if !defined (STORE_PSEUDO_REGISTER_P)
#define STORE_PSEUDO_REGISTER_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (STORE_PSEUDO_REGISTER_P)
#define STORE_PSEUDO_REGISTER_P() (0)
#endif

extern int gdbarch_store_pseudo_register_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (STORE_PSEUDO_REGISTER_P)
#error "Non multi-arch definition of STORE_PSEUDO_REGISTER"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (STORE_PSEUDO_REGISTER_P)
#define STORE_PSEUDO_REGISTER_P() (gdbarch_store_pseudo_register_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (STORE_PSEUDO_REGISTER)
#define STORE_PSEUDO_REGISTER(regnum) (internal_error (__FILE__, __LINE__, "STORE_PSEUDO_REGISTER"), 0)
#endif

typedef void (gdbarch_store_pseudo_register_ftype) (int regnum);
extern void gdbarch_store_pseudo_register (struct gdbarch *gdbarch, int regnum);
extern void set_gdbarch_store_pseudo_register (struct gdbarch *gdbarch, gdbarch_store_pseudo_register_ftype *store_pseudo_register);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (STORE_PSEUDO_REGISTER)
#error "Non multi-arch definition of STORE_PSEUDO_REGISTER"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (STORE_PSEUDO_REGISTER)
#define STORE_PSEUDO_REGISTER(regnum) (gdbarch_store_pseudo_register (current_gdbarch, regnum))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (POINTER_TO_ADDRESS)
#define POINTER_TO_ADDRESS(type, buf) (unsigned_pointer_to_address (type, buf))
#endif

typedef CORE_ADDR (gdbarch_pointer_to_address_ftype) (struct type *type, void *buf);
extern CORE_ADDR gdbarch_pointer_to_address (struct gdbarch *gdbarch, struct type *type, void *buf);
extern void set_gdbarch_pointer_to_address (struct gdbarch *gdbarch, gdbarch_pointer_to_address_ftype *pointer_to_address);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (POINTER_TO_ADDRESS)
#error "Non multi-arch definition of POINTER_TO_ADDRESS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (POINTER_TO_ADDRESS)
#define POINTER_TO_ADDRESS(type, buf) (gdbarch_pointer_to_address (current_gdbarch, type, buf))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (ADDRESS_TO_POINTER)
#define ADDRESS_TO_POINTER(type, buf, addr) (unsigned_address_to_pointer (type, buf, addr))
#endif

typedef void (gdbarch_address_to_pointer_ftype) (struct type *type, void *buf, CORE_ADDR addr);
extern void gdbarch_address_to_pointer (struct gdbarch *gdbarch, struct type *type, void *buf, CORE_ADDR addr);
extern void set_gdbarch_address_to_pointer (struct gdbarch *gdbarch, gdbarch_address_to_pointer_ftype *address_to_pointer);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (ADDRESS_TO_POINTER)
#error "Non multi-arch definition of ADDRESS_TO_POINTER"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (ADDRESS_TO_POINTER)
#define ADDRESS_TO_POINTER(type, buf, addr) (gdbarch_address_to_pointer (current_gdbarch, type, buf, addr))
#endif
#endif

#if defined (INTEGER_TO_ADDRESS)
/* Legacy for systems yet to multi-arch INTEGER_TO_ADDRESS */
#if !defined (INTEGER_TO_ADDRESS_P)
#define INTEGER_TO_ADDRESS_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (INTEGER_TO_ADDRESS_P)
#define INTEGER_TO_ADDRESS_P() (0)
#endif

extern int gdbarch_integer_to_address_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (INTEGER_TO_ADDRESS_P)
#error "Non multi-arch definition of INTEGER_TO_ADDRESS"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (INTEGER_TO_ADDRESS_P)
#define INTEGER_TO_ADDRESS_P() (gdbarch_integer_to_address_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (INTEGER_TO_ADDRESS)
#define INTEGER_TO_ADDRESS(type, buf) (internal_error (__FILE__, __LINE__, "INTEGER_TO_ADDRESS"), 0)
#endif

typedef CORE_ADDR (gdbarch_integer_to_address_ftype) (struct type *type, void *buf);
extern CORE_ADDR gdbarch_integer_to_address (struct gdbarch *gdbarch, struct type *type, void *buf);
extern void set_gdbarch_integer_to_address (struct gdbarch *gdbarch, gdbarch_integer_to_address_ftype *integer_to_address);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (INTEGER_TO_ADDRESS)
#error "Non multi-arch definition of INTEGER_TO_ADDRESS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (INTEGER_TO_ADDRESS)
#define INTEGER_TO_ADDRESS(type, buf) (gdbarch_integer_to_address (current_gdbarch, type, buf))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (RETURN_VALUE_ON_STACK)
#define RETURN_VALUE_ON_STACK(type) (generic_return_value_on_stack_not (type))
#endif

typedef int (gdbarch_return_value_on_stack_ftype) (struct type *type);
extern int gdbarch_return_value_on_stack (struct gdbarch *gdbarch, struct type *type);
extern void set_gdbarch_return_value_on_stack (struct gdbarch *gdbarch, gdbarch_return_value_on_stack_ftype *return_value_on_stack);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (RETURN_VALUE_ON_STACK)
#error "Non multi-arch definition of RETURN_VALUE_ON_STACK"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (RETURN_VALUE_ON_STACK)
#define RETURN_VALUE_ON_STACK(type) (gdbarch_return_value_on_stack (current_gdbarch, type))
#endif
#endif

typedef void (gdbarch_extract_return_value_ftype) (struct type *type, char *regbuf, char *valbuf);
extern void gdbarch_extract_return_value (struct gdbarch *gdbarch, struct type *type, char *regbuf, char *valbuf);
extern void set_gdbarch_extract_return_value (struct gdbarch *gdbarch, gdbarch_extract_return_value_ftype *extract_return_value);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (EXTRACT_RETURN_VALUE)
#error "Non multi-arch definition of EXTRACT_RETURN_VALUE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (EXTRACT_RETURN_VALUE)
#define EXTRACT_RETURN_VALUE(type, regbuf, valbuf) (gdbarch_extract_return_value (current_gdbarch, type, regbuf, valbuf))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (PUSH_ARGUMENTS)
#define PUSH_ARGUMENTS(nargs, args, sp, struct_return, struct_addr) (default_push_arguments (nargs, args, sp, struct_return, struct_addr))
#endif

typedef CORE_ADDR (gdbarch_push_arguments_ftype) (int nargs, struct value **args, CORE_ADDR sp, int struct_return, CORE_ADDR struct_addr);
extern CORE_ADDR gdbarch_push_arguments (struct gdbarch *gdbarch, int nargs, struct value **args, CORE_ADDR sp, int struct_return, CORE_ADDR struct_addr);
extern void set_gdbarch_push_arguments (struct gdbarch *gdbarch, gdbarch_push_arguments_ftype *push_arguments);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (PUSH_ARGUMENTS)
#error "Non multi-arch definition of PUSH_ARGUMENTS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (PUSH_ARGUMENTS)
#define PUSH_ARGUMENTS(nargs, args, sp, struct_return, struct_addr) (gdbarch_push_arguments (current_gdbarch, nargs, args, sp, struct_return, struct_addr))
#endif
#endif

typedef void (gdbarch_push_dummy_frame_ftype) (void);
extern void gdbarch_push_dummy_frame (struct gdbarch *gdbarch);
extern void set_gdbarch_push_dummy_frame (struct gdbarch *gdbarch, gdbarch_push_dummy_frame_ftype *push_dummy_frame);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (PUSH_DUMMY_FRAME)
#error "Non multi-arch definition of PUSH_DUMMY_FRAME"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (PUSH_DUMMY_FRAME)
#define PUSH_DUMMY_FRAME (gdbarch_push_dummy_frame (current_gdbarch))
#endif
#endif

#if defined (PUSH_RETURN_ADDRESS)
/* Legacy for systems yet to multi-arch PUSH_RETURN_ADDRESS */
#if !defined (PUSH_RETURN_ADDRESS_P)
#define PUSH_RETURN_ADDRESS_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (PUSH_RETURN_ADDRESS_P)
#define PUSH_RETURN_ADDRESS_P() (0)
#endif

extern int gdbarch_push_return_address_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (PUSH_RETURN_ADDRESS_P)
#error "Non multi-arch definition of PUSH_RETURN_ADDRESS"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (PUSH_RETURN_ADDRESS_P)
#define PUSH_RETURN_ADDRESS_P() (gdbarch_push_return_address_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (PUSH_RETURN_ADDRESS)
#define PUSH_RETURN_ADDRESS(pc, sp) (internal_error (__FILE__, __LINE__, "PUSH_RETURN_ADDRESS"), 0)
#endif

typedef CORE_ADDR (gdbarch_push_return_address_ftype) (CORE_ADDR pc, CORE_ADDR sp);
extern CORE_ADDR gdbarch_push_return_address (struct gdbarch *gdbarch, CORE_ADDR pc, CORE_ADDR sp);
extern void set_gdbarch_push_return_address (struct gdbarch *gdbarch, gdbarch_push_return_address_ftype *push_return_address);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (PUSH_RETURN_ADDRESS)
#error "Non multi-arch definition of PUSH_RETURN_ADDRESS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (PUSH_RETURN_ADDRESS)
#define PUSH_RETURN_ADDRESS(pc, sp) (gdbarch_push_return_address (current_gdbarch, pc, sp))
#endif
#endif

typedef void (gdbarch_pop_frame_ftype) (void);
extern void gdbarch_pop_frame (struct gdbarch *gdbarch);
extern void set_gdbarch_pop_frame (struct gdbarch *gdbarch, gdbarch_pop_frame_ftype *pop_frame);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (POP_FRAME)
#error "Non multi-arch definition of POP_FRAME"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (POP_FRAME)
#define POP_FRAME (gdbarch_pop_frame (current_gdbarch))
#endif
#endif

typedef void (gdbarch_store_struct_return_ftype) (CORE_ADDR addr, CORE_ADDR sp);
extern void gdbarch_store_struct_return (struct gdbarch *gdbarch, CORE_ADDR addr, CORE_ADDR sp);
extern void set_gdbarch_store_struct_return (struct gdbarch *gdbarch, gdbarch_store_struct_return_ftype *store_struct_return);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (STORE_STRUCT_RETURN)
#error "Non multi-arch definition of STORE_STRUCT_RETURN"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (STORE_STRUCT_RETURN)
#define STORE_STRUCT_RETURN(addr, sp) (gdbarch_store_struct_return (current_gdbarch, addr, sp))
#endif
#endif

typedef void (gdbarch_store_return_value_ftype) (struct type *type, char *valbuf);
extern void gdbarch_store_return_value (struct gdbarch *gdbarch, struct type *type, char *valbuf);
extern void set_gdbarch_store_return_value (struct gdbarch *gdbarch, gdbarch_store_return_value_ftype *store_return_value);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (STORE_RETURN_VALUE)
#error "Non multi-arch definition of STORE_RETURN_VALUE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (STORE_RETURN_VALUE)
#define STORE_RETURN_VALUE(type, valbuf) (gdbarch_store_return_value (current_gdbarch, type, valbuf))
#endif
#endif

#if defined (EXTRACT_STRUCT_VALUE_ADDRESS)
/* Legacy for systems yet to multi-arch EXTRACT_STRUCT_VALUE_ADDRESS */
#if !defined (EXTRACT_STRUCT_VALUE_ADDRESS_P)
#define EXTRACT_STRUCT_VALUE_ADDRESS_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (EXTRACT_STRUCT_VALUE_ADDRESS_P)
#define EXTRACT_STRUCT_VALUE_ADDRESS_P() (0)
#endif

extern int gdbarch_extract_struct_value_address_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (EXTRACT_STRUCT_VALUE_ADDRESS_P)
#error "Non multi-arch definition of EXTRACT_STRUCT_VALUE_ADDRESS"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (EXTRACT_STRUCT_VALUE_ADDRESS_P)
#define EXTRACT_STRUCT_VALUE_ADDRESS_P() (gdbarch_extract_struct_value_address_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (EXTRACT_STRUCT_VALUE_ADDRESS)
#define EXTRACT_STRUCT_VALUE_ADDRESS(regbuf) (internal_error (__FILE__, __LINE__, "EXTRACT_STRUCT_VALUE_ADDRESS"), 0)
#endif

typedef CORE_ADDR (gdbarch_extract_struct_value_address_ftype) (char *regbuf);
extern CORE_ADDR gdbarch_extract_struct_value_address (struct gdbarch *gdbarch, char *regbuf);
extern void set_gdbarch_extract_struct_value_address (struct gdbarch *gdbarch, gdbarch_extract_struct_value_address_ftype *extract_struct_value_address);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (EXTRACT_STRUCT_VALUE_ADDRESS)
#error "Non multi-arch definition of EXTRACT_STRUCT_VALUE_ADDRESS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (EXTRACT_STRUCT_VALUE_ADDRESS)
#define EXTRACT_STRUCT_VALUE_ADDRESS(regbuf) (gdbarch_extract_struct_value_address (current_gdbarch, regbuf))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (USE_STRUCT_CONVENTION)
#define USE_STRUCT_CONVENTION(gcc_p, value_type) (generic_use_struct_convention (gcc_p, value_type))
#endif

typedef int (gdbarch_use_struct_convention_ftype) (int gcc_p, struct type *value_type);
extern int gdbarch_use_struct_convention (struct gdbarch *gdbarch, int gcc_p, struct type *value_type);
extern void set_gdbarch_use_struct_convention (struct gdbarch *gdbarch, gdbarch_use_struct_convention_ftype *use_struct_convention);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (USE_STRUCT_CONVENTION)
#error "Non multi-arch definition of USE_STRUCT_CONVENTION"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (USE_STRUCT_CONVENTION)
#define USE_STRUCT_CONVENTION(gcc_p, value_type) (gdbarch_use_struct_convention (current_gdbarch, gcc_p, value_type))
#endif
#endif

typedef void (gdbarch_frame_init_saved_regs_ftype) (struct frame_info *frame);
extern void gdbarch_frame_init_saved_regs (struct gdbarch *gdbarch, struct frame_info *frame);
extern void set_gdbarch_frame_init_saved_regs (struct gdbarch *gdbarch, gdbarch_frame_init_saved_regs_ftype *frame_init_saved_regs);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FRAME_INIT_SAVED_REGS)
#error "Non multi-arch definition of FRAME_INIT_SAVED_REGS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FRAME_INIT_SAVED_REGS)
#define FRAME_INIT_SAVED_REGS(frame) (gdbarch_frame_init_saved_regs (current_gdbarch, frame))
#endif
#endif

#if defined (INIT_EXTRA_FRAME_INFO)
/* Legacy for systems yet to multi-arch INIT_EXTRA_FRAME_INFO */
#if !defined (INIT_EXTRA_FRAME_INFO_P)
#define INIT_EXTRA_FRAME_INFO_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (INIT_EXTRA_FRAME_INFO_P)
#define INIT_EXTRA_FRAME_INFO_P() (0)
#endif

extern int gdbarch_init_extra_frame_info_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (INIT_EXTRA_FRAME_INFO_P)
#error "Non multi-arch definition of INIT_EXTRA_FRAME_INFO"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (INIT_EXTRA_FRAME_INFO_P)
#define INIT_EXTRA_FRAME_INFO_P() (gdbarch_init_extra_frame_info_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (INIT_EXTRA_FRAME_INFO)
#define INIT_EXTRA_FRAME_INFO(fromleaf, frame) (internal_error (__FILE__, __LINE__, "INIT_EXTRA_FRAME_INFO"), 0)
#endif

typedef void (gdbarch_init_extra_frame_info_ftype) (int fromleaf, struct frame_info *frame);
extern void gdbarch_init_extra_frame_info (struct gdbarch *gdbarch, int fromleaf, struct frame_info *frame);
extern void set_gdbarch_init_extra_frame_info (struct gdbarch *gdbarch, gdbarch_init_extra_frame_info_ftype *init_extra_frame_info);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (INIT_EXTRA_FRAME_INFO)
#error "Non multi-arch definition of INIT_EXTRA_FRAME_INFO"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (INIT_EXTRA_FRAME_INFO)
#define INIT_EXTRA_FRAME_INFO(fromleaf, frame) (gdbarch_init_extra_frame_info (current_gdbarch, fromleaf, frame))
#endif
#endif

typedef CORE_ADDR (gdbarch_skip_prologue_ftype) (CORE_ADDR ip);
extern CORE_ADDR gdbarch_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR ip);
extern void set_gdbarch_skip_prologue (struct gdbarch *gdbarch, gdbarch_skip_prologue_ftype *skip_prologue);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SKIP_PROLOGUE)
#error "Non multi-arch definition of SKIP_PROLOGUE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SKIP_PROLOGUE)
#define SKIP_PROLOGUE(ip) (gdbarch_skip_prologue (current_gdbarch, ip))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (PROLOGUE_FRAMELESS_P)
#define PROLOGUE_FRAMELESS_P(ip) (generic_prologue_frameless_p (ip))
#endif

typedef int (gdbarch_prologue_frameless_p_ftype) (CORE_ADDR ip);
extern int gdbarch_prologue_frameless_p (struct gdbarch *gdbarch, CORE_ADDR ip);
extern void set_gdbarch_prologue_frameless_p (struct gdbarch *gdbarch, gdbarch_prologue_frameless_p_ftype *prologue_frameless_p);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (PROLOGUE_FRAMELESS_P)
#error "Non multi-arch definition of PROLOGUE_FRAMELESS_P"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (PROLOGUE_FRAMELESS_P)
#define PROLOGUE_FRAMELESS_P(ip) (gdbarch_prologue_frameless_p (current_gdbarch, ip))
#endif
#endif

typedef int (gdbarch_inner_than_ftype) (CORE_ADDR lhs, CORE_ADDR rhs);
extern int gdbarch_inner_than (struct gdbarch *gdbarch, CORE_ADDR lhs, CORE_ADDR rhs);
extern void set_gdbarch_inner_than (struct gdbarch *gdbarch, gdbarch_inner_than_ftype *inner_than);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (INNER_THAN)
#error "Non multi-arch definition of INNER_THAN"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (INNER_THAN)
#define INNER_THAN(lhs, rhs) (gdbarch_inner_than (current_gdbarch, lhs, rhs))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (BREAKPOINT_FROM_PC)
#define BREAKPOINT_FROM_PC(pcptr, lenptr) (legacy_breakpoint_from_pc (pcptr, lenptr))
#endif

typedef unsigned char * (gdbarch_breakpoint_from_pc_ftype) (CORE_ADDR *pcptr, int *lenptr);
extern unsigned char * gdbarch_breakpoint_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pcptr, int *lenptr);
extern void set_gdbarch_breakpoint_from_pc (struct gdbarch *gdbarch, gdbarch_breakpoint_from_pc_ftype *breakpoint_from_pc);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (BREAKPOINT_FROM_PC)
#error "Non multi-arch definition of BREAKPOINT_FROM_PC"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (BREAKPOINT_FROM_PC)
#define BREAKPOINT_FROM_PC(pcptr, lenptr) (gdbarch_breakpoint_from_pc (current_gdbarch, pcptr, lenptr))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (MEMORY_INSERT_BREAKPOINT)
#define MEMORY_INSERT_BREAKPOINT(addr, contents_cache) (default_memory_insert_breakpoint (addr, contents_cache))
#endif

typedef int (gdbarch_memory_insert_breakpoint_ftype) (CORE_ADDR addr, char *contents_cache);
extern int gdbarch_memory_insert_breakpoint (struct gdbarch *gdbarch, CORE_ADDR addr, char *contents_cache);
extern void set_gdbarch_memory_insert_breakpoint (struct gdbarch *gdbarch, gdbarch_memory_insert_breakpoint_ftype *memory_insert_breakpoint);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (MEMORY_INSERT_BREAKPOINT)
#error "Non multi-arch definition of MEMORY_INSERT_BREAKPOINT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (MEMORY_INSERT_BREAKPOINT)
#define MEMORY_INSERT_BREAKPOINT(addr, contents_cache) (gdbarch_memory_insert_breakpoint (current_gdbarch, addr, contents_cache))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (MEMORY_REMOVE_BREAKPOINT)
#define MEMORY_REMOVE_BREAKPOINT(addr, contents_cache) (default_memory_remove_breakpoint (addr, contents_cache))
#endif

typedef int (gdbarch_memory_remove_breakpoint_ftype) (CORE_ADDR addr, char *contents_cache);
extern int gdbarch_memory_remove_breakpoint (struct gdbarch *gdbarch, CORE_ADDR addr, char *contents_cache);
extern void set_gdbarch_memory_remove_breakpoint (struct gdbarch *gdbarch, gdbarch_memory_remove_breakpoint_ftype *memory_remove_breakpoint);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (MEMORY_REMOVE_BREAKPOINT)
#error "Non multi-arch definition of MEMORY_REMOVE_BREAKPOINT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (MEMORY_REMOVE_BREAKPOINT)
#define MEMORY_REMOVE_BREAKPOINT(addr, contents_cache) (gdbarch_memory_remove_breakpoint (current_gdbarch, addr, contents_cache))
#endif
#endif

extern CORE_ADDR gdbarch_decr_pc_after_break (struct gdbarch *gdbarch);
extern void set_gdbarch_decr_pc_after_break (struct gdbarch *gdbarch, CORE_ADDR decr_pc_after_break);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (DECR_PC_AFTER_BREAK)
#error "Non multi-arch definition of DECR_PC_AFTER_BREAK"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (DECR_PC_AFTER_BREAK)
#define DECR_PC_AFTER_BREAK (gdbarch_decr_pc_after_break (current_gdbarch))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (PREPARE_TO_PROCEED)
#define PREPARE_TO_PROCEED(select_it) (default_prepare_to_proceed (select_it))
#endif

typedef int (gdbarch_prepare_to_proceed_ftype) (int select_it);
extern int gdbarch_prepare_to_proceed (struct gdbarch *gdbarch, int select_it);
extern void set_gdbarch_prepare_to_proceed (struct gdbarch *gdbarch, gdbarch_prepare_to_proceed_ftype *prepare_to_proceed);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (PREPARE_TO_PROCEED)
#error "Non multi-arch definition of PREPARE_TO_PROCEED"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (PREPARE_TO_PROCEED)
#define PREPARE_TO_PROCEED(select_it) (gdbarch_prepare_to_proceed (current_gdbarch, select_it))
#endif
#endif

extern CORE_ADDR gdbarch_function_start_offset (struct gdbarch *gdbarch);
extern void set_gdbarch_function_start_offset (struct gdbarch *gdbarch, CORE_ADDR function_start_offset);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FUNCTION_START_OFFSET)
#error "Non multi-arch definition of FUNCTION_START_OFFSET"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FUNCTION_START_OFFSET)
#define FUNCTION_START_OFFSET (gdbarch_function_start_offset (current_gdbarch))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (REMOTE_TRANSLATE_XFER_ADDRESS)
#define REMOTE_TRANSLATE_XFER_ADDRESS(gdb_addr, gdb_len, rem_addr, rem_len) (generic_remote_translate_xfer_address (gdb_addr, gdb_len, rem_addr, rem_len))
#endif

typedef void (gdbarch_remote_translate_xfer_address_ftype) (CORE_ADDR gdb_addr, int gdb_len, CORE_ADDR *rem_addr, int *rem_len);
extern void gdbarch_remote_translate_xfer_address (struct gdbarch *gdbarch, CORE_ADDR gdb_addr, int gdb_len, CORE_ADDR *rem_addr, int *rem_len);
extern void set_gdbarch_remote_translate_xfer_address (struct gdbarch *gdbarch, gdbarch_remote_translate_xfer_address_ftype *remote_translate_xfer_address);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REMOTE_TRANSLATE_XFER_ADDRESS)
#error "Non multi-arch definition of REMOTE_TRANSLATE_XFER_ADDRESS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REMOTE_TRANSLATE_XFER_ADDRESS)
#define REMOTE_TRANSLATE_XFER_ADDRESS(gdb_addr, gdb_len, rem_addr, rem_len) (gdbarch_remote_translate_xfer_address (current_gdbarch, gdb_addr, gdb_len, rem_addr, rem_len))
#endif
#endif

extern CORE_ADDR gdbarch_frame_args_skip (struct gdbarch *gdbarch);
extern void set_gdbarch_frame_args_skip (struct gdbarch *gdbarch, CORE_ADDR frame_args_skip);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FRAME_ARGS_SKIP)
#error "Non multi-arch definition of FRAME_ARGS_SKIP"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FRAME_ARGS_SKIP)
#define FRAME_ARGS_SKIP (gdbarch_frame_args_skip (current_gdbarch))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (FRAMELESS_FUNCTION_INVOCATION)
#define FRAMELESS_FUNCTION_INVOCATION(fi) (generic_frameless_function_invocation_not (fi))
#endif

typedef int (gdbarch_frameless_function_invocation_ftype) (struct frame_info *fi);
extern int gdbarch_frameless_function_invocation (struct gdbarch *gdbarch, struct frame_info *fi);
extern void set_gdbarch_frameless_function_invocation (struct gdbarch *gdbarch, gdbarch_frameless_function_invocation_ftype *frameless_function_invocation);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FRAMELESS_FUNCTION_INVOCATION)
#error "Non multi-arch definition of FRAMELESS_FUNCTION_INVOCATION"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FRAMELESS_FUNCTION_INVOCATION)
#define FRAMELESS_FUNCTION_INVOCATION(fi) (gdbarch_frameless_function_invocation (current_gdbarch, fi))
#endif
#endif

typedef CORE_ADDR (gdbarch_frame_chain_ftype) (struct frame_info *frame);
extern CORE_ADDR gdbarch_frame_chain (struct gdbarch *gdbarch, struct frame_info *frame);
extern void set_gdbarch_frame_chain (struct gdbarch *gdbarch, gdbarch_frame_chain_ftype *frame_chain);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FRAME_CHAIN)
#error "Non multi-arch definition of FRAME_CHAIN"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FRAME_CHAIN)
#define FRAME_CHAIN(frame) (gdbarch_frame_chain (current_gdbarch, frame))
#endif
#endif

/* Define a default FRAME_CHAIN_VALID, in the form that is suitable for
   most targets.  If FRAME_CHAIN_VALID returns zero it means that the
   given frame is the outermost one and has no caller.
  
   XXXX - both default and alternate frame_chain_valid functions are
   deprecated.  New code should use dummy frames and one of the generic
   functions. */

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (FRAME_CHAIN_VALID)
#define FRAME_CHAIN_VALID(chain, thisframe) (func_frame_chain_valid (chain, thisframe))
#endif

typedef int (gdbarch_frame_chain_valid_ftype) (CORE_ADDR chain, struct frame_info *thisframe);
extern int gdbarch_frame_chain_valid (struct gdbarch *gdbarch, CORE_ADDR chain, struct frame_info *thisframe);
extern void set_gdbarch_frame_chain_valid (struct gdbarch *gdbarch, gdbarch_frame_chain_valid_ftype *frame_chain_valid);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FRAME_CHAIN_VALID)
#error "Non multi-arch definition of FRAME_CHAIN_VALID"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FRAME_CHAIN_VALID)
#define FRAME_CHAIN_VALID(chain, thisframe) (gdbarch_frame_chain_valid (current_gdbarch, chain, thisframe))
#endif
#endif

typedef CORE_ADDR (gdbarch_frame_saved_pc_ftype) (struct frame_info *fi);
extern CORE_ADDR gdbarch_frame_saved_pc (struct gdbarch *gdbarch, struct frame_info *fi);
extern void set_gdbarch_frame_saved_pc (struct gdbarch *gdbarch, gdbarch_frame_saved_pc_ftype *frame_saved_pc);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FRAME_SAVED_PC)
#error "Non multi-arch definition of FRAME_SAVED_PC"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FRAME_SAVED_PC)
#define FRAME_SAVED_PC(fi) (gdbarch_frame_saved_pc (current_gdbarch, fi))
#endif
#endif

typedef CORE_ADDR (gdbarch_frame_args_address_ftype) (struct frame_info *fi);
extern CORE_ADDR gdbarch_frame_args_address (struct gdbarch *gdbarch, struct frame_info *fi);
extern void set_gdbarch_frame_args_address (struct gdbarch *gdbarch, gdbarch_frame_args_address_ftype *frame_args_address);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FRAME_ARGS_ADDRESS)
#error "Non multi-arch definition of FRAME_ARGS_ADDRESS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FRAME_ARGS_ADDRESS)
#define FRAME_ARGS_ADDRESS(fi) (gdbarch_frame_args_address (current_gdbarch, fi))
#endif
#endif

typedef CORE_ADDR (gdbarch_frame_locals_address_ftype) (struct frame_info *fi);
extern CORE_ADDR gdbarch_frame_locals_address (struct gdbarch *gdbarch, struct frame_info *fi);
extern void set_gdbarch_frame_locals_address (struct gdbarch *gdbarch, gdbarch_frame_locals_address_ftype *frame_locals_address);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FRAME_LOCALS_ADDRESS)
#error "Non multi-arch definition of FRAME_LOCALS_ADDRESS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FRAME_LOCALS_ADDRESS)
#define FRAME_LOCALS_ADDRESS(fi) (gdbarch_frame_locals_address (current_gdbarch, fi))
#endif
#endif

typedef CORE_ADDR (gdbarch_saved_pc_after_call_ftype) (struct frame_info *frame);
extern CORE_ADDR gdbarch_saved_pc_after_call (struct gdbarch *gdbarch, struct frame_info *frame);
extern void set_gdbarch_saved_pc_after_call (struct gdbarch *gdbarch, gdbarch_saved_pc_after_call_ftype *saved_pc_after_call);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SAVED_PC_AFTER_CALL)
#error "Non multi-arch definition of SAVED_PC_AFTER_CALL"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SAVED_PC_AFTER_CALL)
#define SAVED_PC_AFTER_CALL(frame) (gdbarch_saved_pc_after_call (current_gdbarch, frame))
#endif
#endif

typedef int (gdbarch_frame_num_args_ftype) (struct frame_info *frame);
extern int gdbarch_frame_num_args (struct gdbarch *gdbarch, struct frame_info *frame);
extern void set_gdbarch_frame_num_args (struct gdbarch *gdbarch, gdbarch_frame_num_args_ftype *frame_num_args);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (FRAME_NUM_ARGS)
#error "Non multi-arch definition of FRAME_NUM_ARGS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (FRAME_NUM_ARGS)
#define FRAME_NUM_ARGS(frame) (gdbarch_frame_num_args (current_gdbarch, frame))
#endif
#endif

#if defined (STACK_ALIGN)
/* Legacy for systems yet to multi-arch STACK_ALIGN */
#if !defined (STACK_ALIGN_P)
#define STACK_ALIGN_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (STACK_ALIGN_P)
#define STACK_ALIGN_P() (0)
#endif

extern int gdbarch_stack_align_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (STACK_ALIGN_P)
#error "Non multi-arch definition of STACK_ALIGN"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (STACK_ALIGN_P)
#define STACK_ALIGN_P() (gdbarch_stack_align_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (STACK_ALIGN)
#define STACK_ALIGN(sp) (internal_error (__FILE__, __LINE__, "STACK_ALIGN"), 0)
#endif

typedef CORE_ADDR (gdbarch_stack_align_ftype) (CORE_ADDR sp);
extern CORE_ADDR gdbarch_stack_align (struct gdbarch *gdbarch, CORE_ADDR sp);
extern void set_gdbarch_stack_align (struct gdbarch *gdbarch, gdbarch_stack_align_ftype *stack_align);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (STACK_ALIGN)
#error "Non multi-arch definition of STACK_ALIGN"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (STACK_ALIGN)
#define STACK_ALIGN(sp) (gdbarch_stack_align (current_gdbarch, sp))
#endif
#endif

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (EXTRA_STACK_ALIGNMENT_NEEDED)
#define EXTRA_STACK_ALIGNMENT_NEEDED (1)
#endif

extern int gdbarch_extra_stack_alignment_needed (struct gdbarch *gdbarch);
extern void set_gdbarch_extra_stack_alignment_needed (struct gdbarch *gdbarch, int extra_stack_alignment_needed);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (EXTRA_STACK_ALIGNMENT_NEEDED)
#error "Non multi-arch definition of EXTRA_STACK_ALIGNMENT_NEEDED"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (EXTRA_STACK_ALIGNMENT_NEEDED)
#define EXTRA_STACK_ALIGNMENT_NEEDED (gdbarch_extra_stack_alignment_needed (current_gdbarch))
#endif
#endif

#if defined (REG_STRUCT_HAS_ADDR)
/* Legacy for systems yet to multi-arch REG_STRUCT_HAS_ADDR */
#if !defined (REG_STRUCT_HAS_ADDR_P)
#define REG_STRUCT_HAS_ADDR_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (REG_STRUCT_HAS_ADDR_P)
#define REG_STRUCT_HAS_ADDR_P() (0)
#endif

extern int gdbarch_reg_struct_has_addr_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REG_STRUCT_HAS_ADDR_P)
#error "Non multi-arch definition of REG_STRUCT_HAS_ADDR"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REG_STRUCT_HAS_ADDR_P)
#define REG_STRUCT_HAS_ADDR_P() (gdbarch_reg_struct_has_addr_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (REG_STRUCT_HAS_ADDR)
#define REG_STRUCT_HAS_ADDR(gcc_p, type) (internal_error (__FILE__, __LINE__, "REG_STRUCT_HAS_ADDR"), 0)
#endif

typedef int (gdbarch_reg_struct_has_addr_ftype) (int gcc_p, struct type *type);
extern int gdbarch_reg_struct_has_addr (struct gdbarch *gdbarch, int gcc_p, struct type *type);
extern void set_gdbarch_reg_struct_has_addr (struct gdbarch *gdbarch, gdbarch_reg_struct_has_addr_ftype *reg_struct_has_addr);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (REG_STRUCT_HAS_ADDR)
#error "Non multi-arch definition of REG_STRUCT_HAS_ADDR"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (REG_STRUCT_HAS_ADDR)
#define REG_STRUCT_HAS_ADDR(gcc_p, type) (gdbarch_reg_struct_has_addr (current_gdbarch, gcc_p, type))
#endif
#endif

#if defined (SAVE_DUMMY_FRAME_TOS)
/* Legacy for systems yet to multi-arch SAVE_DUMMY_FRAME_TOS */
#if !defined (SAVE_DUMMY_FRAME_TOS_P)
#define SAVE_DUMMY_FRAME_TOS_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (SAVE_DUMMY_FRAME_TOS_P)
#define SAVE_DUMMY_FRAME_TOS_P() (0)
#endif

extern int gdbarch_save_dummy_frame_tos_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SAVE_DUMMY_FRAME_TOS_P)
#error "Non multi-arch definition of SAVE_DUMMY_FRAME_TOS"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SAVE_DUMMY_FRAME_TOS_P)
#define SAVE_DUMMY_FRAME_TOS_P() (gdbarch_save_dummy_frame_tos_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (SAVE_DUMMY_FRAME_TOS)
#define SAVE_DUMMY_FRAME_TOS(sp) (internal_error (__FILE__, __LINE__, "SAVE_DUMMY_FRAME_TOS"), 0)
#endif

typedef void (gdbarch_save_dummy_frame_tos_ftype) (CORE_ADDR sp);
extern void gdbarch_save_dummy_frame_tos (struct gdbarch *gdbarch, CORE_ADDR sp);
extern void set_gdbarch_save_dummy_frame_tos (struct gdbarch *gdbarch, gdbarch_save_dummy_frame_tos_ftype *save_dummy_frame_tos);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SAVE_DUMMY_FRAME_TOS)
#error "Non multi-arch definition of SAVE_DUMMY_FRAME_TOS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SAVE_DUMMY_FRAME_TOS)
#define SAVE_DUMMY_FRAME_TOS(sp) (gdbarch_save_dummy_frame_tos (current_gdbarch, sp))
#endif
#endif

extern int gdbarch_parm_boundary (struct gdbarch *gdbarch);
extern void set_gdbarch_parm_boundary (struct gdbarch *gdbarch, int parm_boundary);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (PARM_BOUNDARY)
#error "Non multi-arch definition of PARM_BOUNDARY"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (PARM_BOUNDARY)
#define PARM_BOUNDARY (gdbarch_parm_boundary (current_gdbarch))
#endif
#endif

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_FLOAT_FORMAT)
#define TARGET_FLOAT_FORMAT (default_float_format (current_gdbarch))
#endif

extern const struct floatformat * gdbarch_float_format (struct gdbarch *gdbarch);
extern void set_gdbarch_float_format (struct gdbarch *gdbarch, const struct floatformat * float_format);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_FLOAT_FORMAT)
#error "Non multi-arch definition of TARGET_FLOAT_FORMAT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_FLOAT_FORMAT)
#define TARGET_FLOAT_FORMAT (gdbarch_float_format (current_gdbarch))
#endif
#endif

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_DOUBLE_FORMAT)
#define TARGET_DOUBLE_FORMAT (default_double_format (current_gdbarch))
#endif

extern const struct floatformat * gdbarch_double_format (struct gdbarch *gdbarch);
extern void set_gdbarch_double_format (struct gdbarch *gdbarch, const struct floatformat * double_format);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_DOUBLE_FORMAT)
#error "Non multi-arch definition of TARGET_DOUBLE_FORMAT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_DOUBLE_FORMAT)
#define TARGET_DOUBLE_FORMAT (gdbarch_double_format (current_gdbarch))
#endif
#endif

/* Default (value) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_LONG_DOUBLE_FORMAT)
#define TARGET_LONG_DOUBLE_FORMAT (default_double_format (current_gdbarch))
#endif

extern const struct floatformat * gdbarch_long_double_format (struct gdbarch *gdbarch);
extern void set_gdbarch_long_double_format (struct gdbarch *gdbarch, const struct floatformat * long_double_format);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_LONG_DOUBLE_FORMAT)
#error "Non multi-arch definition of TARGET_LONG_DOUBLE_FORMAT"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_LONG_DOUBLE_FORMAT)
#define TARGET_LONG_DOUBLE_FORMAT (gdbarch_long_double_format (current_gdbarch))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (CONVERT_FROM_FUNC_PTR_ADDR)
#define CONVERT_FROM_FUNC_PTR_ADDR(addr) (core_addr_identity (addr))
#endif

typedef CORE_ADDR (gdbarch_convert_from_func_ptr_addr_ftype) (CORE_ADDR addr);
extern CORE_ADDR gdbarch_convert_from_func_ptr_addr (struct gdbarch *gdbarch, CORE_ADDR addr);
extern void set_gdbarch_convert_from_func_ptr_addr (struct gdbarch *gdbarch, gdbarch_convert_from_func_ptr_addr_ftype *convert_from_func_ptr_addr);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (CONVERT_FROM_FUNC_PTR_ADDR)
#error "Non multi-arch definition of CONVERT_FROM_FUNC_PTR_ADDR"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (CONVERT_FROM_FUNC_PTR_ADDR)
#define CONVERT_FROM_FUNC_PTR_ADDR(addr) (gdbarch_convert_from_func_ptr_addr (current_gdbarch, addr))
#endif
#endif

/* On some machines there are bits in addresses which are not really
   part of the address, but are used by the kernel, the hardware, etc.
   for special purposes.  ADDR_BITS_REMOVE takes out any such bits so
   we get a "real" address such as one would find in a symbol table.
   This is used only for addresses of instructions, and even then I'm
   not sure it's used in all contexts.  It exists to deal with there
   being a few stray bits in the PC which would mislead us, not as some
   sort of generic thing to handle alignment or segmentation (it's
   possible it should be in TARGET_READ_PC instead). */

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (ADDR_BITS_REMOVE)
#define ADDR_BITS_REMOVE(addr) (core_addr_identity (addr))
#endif

typedef CORE_ADDR (gdbarch_addr_bits_remove_ftype) (CORE_ADDR addr);
extern CORE_ADDR gdbarch_addr_bits_remove (struct gdbarch *gdbarch, CORE_ADDR addr);
extern void set_gdbarch_addr_bits_remove (struct gdbarch *gdbarch, gdbarch_addr_bits_remove_ftype *addr_bits_remove);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (ADDR_BITS_REMOVE)
#error "Non multi-arch definition of ADDR_BITS_REMOVE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (ADDR_BITS_REMOVE)
#define ADDR_BITS_REMOVE(addr) (gdbarch_addr_bits_remove (current_gdbarch, addr))
#endif
#endif

/* It is not at all clear why SMASH_TEXT_ADDRESS is not folded into
   ADDR_BITS_REMOVE. */

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (SMASH_TEXT_ADDRESS)
#define SMASH_TEXT_ADDRESS(addr) (core_addr_identity (addr))
#endif

typedef CORE_ADDR (gdbarch_smash_text_address_ftype) (CORE_ADDR addr);
extern CORE_ADDR gdbarch_smash_text_address (struct gdbarch *gdbarch, CORE_ADDR addr);
extern void set_gdbarch_smash_text_address (struct gdbarch *gdbarch, gdbarch_smash_text_address_ftype *smash_text_address);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SMASH_TEXT_ADDRESS)
#error "Non multi-arch definition of SMASH_TEXT_ADDRESS"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SMASH_TEXT_ADDRESS)
#define SMASH_TEXT_ADDRESS(addr) (gdbarch_smash_text_address (current_gdbarch, addr))
#endif
#endif

/* FIXME/cagney/2001-01-18: This should be split in two.  A target method that indicates if
   the target needs software single step.  An ISA method to implement it.
  
   FIXME/cagney/2001-01-18: This should be replaced with something that inserts breakpoints
   using the breakpoint system instead of blatting memory directly (as with rs6000).
  
   FIXME/cagney/2001-01-18: The logic is backwards.  It should be asking if the target can
   single step.  If not, then implement single step using breakpoints. */

#if defined (SOFTWARE_SINGLE_STEP)
/* Legacy for systems yet to multi-arch SOFTWARE_SINGLE_STEP */
#if !defined (SOFTWARE_SINGLE_STEP_P)
#define SOFTWARE_SINGLE_STEP_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (SOFTWARE_SINGLE_STEP_P)
#define SOFTWARE_SINGLE_STEP_P() (0)
#endif

extern int gdbarch_software_single_step_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SOFTWARE_SINGLE_STEP_P)
#error "Non multi-arch definition of SOFTWARE_SINGLE_STEP"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SOFTWARE_SINGLE_STEP_P)
#define SOFTWARE_SINGLE_STEP_P() (gdbarch_software_single_step_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (SOFTWARE_SINGLE_STEP)
#define SOFTWARE_SINGLE_STEP(sig, insert_breakpoints_p) (internal_error (__FILE__, __LINE__, "SOFTWARE_SINGLE_STEP"), 0)
#endif

typedef void (gdbarch_software_single_step_ftype) (enum target_signal sig, int insert_breakpoints_p);
extern void gdbarch_software_single_step (struct gdbarch *gdbarch, enum target_signal sig, int insert_breakpoints_p);
extern void set_gdbarch_software_single_step (struct gdbarch *gdbarch, gdbarch_software_single_step_ftype *software_single_step);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SOFTWARE_SINGLE_STEP)
#error "Non multi-arch definition of SOFTWARE_SINGLE_STEP"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SOFTWARE_SINGLE_STEP)
#define SOFTWARE_SINGLE_STEP(sig, insert_breakpoints_p) (gdbarch_software_single_step (current_gdbarch, sig, insert_breakpoints_p))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (TARGET_PRINT_INSN)
#define TARGET_PRINT_INSN(vma, info) (legacy_print_insn (vma, info))
#endif

typedef int (gdbarch_print_insn_ftype) (bfd_vma vma, disassemble_info *info);
extern int gdbarch_print_insn (struct gdbarch *gdbarch, bfd_vma vma, disassemble_info *info);
extern void set_gdbarch_print_insn (struct gdbarch *gdbarch, gdbarch_print_insn_ftype *print_insn);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (TARGET_PRINT_INSN)
#error "Non multi-arch definition of TARGET_PRINT_INSN"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (TARGET_PRINT_INSN)
#define TARGET_PRINT_INSN(vma, info) (gdbarch_print_insn (current_gdbarch, vma, info))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (SKIP_TRAMPOLINE_CODE)
#define SKIP_TRAMPOLINE_CODE(pc) (generic_skip_trampoline_code (pc))
#endif

typedef CORE_ADDR (gdbarch_skip_trampoline_code_ftype) (CORE_ADDR pc);
extern CORE_ADDR gdbarch_skip_trampoline_code (struct gdbarch *gdbarch, CORE_ADDR pc);
extern void set_gdbarch_skip_trampoline_code (struct gdbarch *gdbarch, gdbarch_skip_trampoline_code_ftype *skip_trampoline_code);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (SKIP_TRAMPOLINE_CODE)
#error "Non multi-arch definition of SKIP_TRAMPOLINE_CODE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (SKIP_TRAMPOLINE_CODE)
#define SKIP_TRAMPOLINE_CODE(pc) (gdbarch_skip_trampoline_code (current_gdbarch, pc))
#endif
#endif

/* For SVR4 shared libraries, each call goes through a small piece of
   trampoline code in the ".plt" section.  IN_SOLIB_CALL_TRAMPOLINE evaluates
   to nonzero if we are current stopped in one of these. */

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (IN_SOLIB_CALL_TRAMPOLINE)
#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) (generic_in_solib_call_trampoline (pc, name))
#endif

typedef int (gdbarch_in_solib_call_trampoline_ftype) (CORE_ADDR pc, char *name);
extern int gdbarch_in_solib_call_trampoline (struct gdbarch *gdbarch, CORE_ADDR pc, char *name);
extern void set_gdbarch_in_solib_call_trampoline (struct gdbarch *gdbarch, gdbarch_in_solib_call_trampoline_ftype *in_solib_call_trampoline);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (IN_SOLIB_CALL_TRAMPOLINE)
#error "Non multi-arch definition of IN_SOLIB_CALL_TRAMPOLINE"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (IN_SOLIB_CALL_TRAMPOLINE)
#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) (gdbarch_in_solib_call_trampoline (current_gdbarch, pc, name))
#endif
#endif

/* A target might have problems with watchpoints as soon as the stack
   frame of the current function has been destroyed.  This mostly happens
   as the first action in a funtion's epilogue.  in_function_epilogue_p()
   is defined to return a non-zero value if either the given addr is one
   instruction after the stack destroying instruction up to the trailing
   return instruction or if we can figure out that the stack frame has
   already been invalidated regardless of the value of addr.  Targets
   which don't suffer from that problem could just let this functionality
   untouched. */

typedef int (gdbarch_in_function_epilogue_p_ftype) (struct gdbarch *gdbarch, CORE_ADDR addr);
extern int gdbarch_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR addr);
extern void set_gdbarch_in_function_epilogue_p (struct gdbarch *gdbarch, gdbarch_in_function_epilogue_p_ftype *in_function_epilogue_p);

/* Given a vector of command-line arguments, return a newly allocated
   string which, when passed to the create_inferior function, will be
   parsed (on Unix systems, by the shell) to yield the same vector.
   This function should call error() if the argument vector is not
   representable for this target or if this target does not support
   command-line arguments.
   ARGC is the number of elements in the vector.
   ARGV is an array of strings, one per argument. */

typedef char * (gdbarch_construct_inferior_arguments_ftype) (struct gdbarch *gdbarch, int argc, char **argv);
extern char * gdbarch_construct_inferior_arguments (struct gdbarch *gdbarch, int argc, char **argv);
extern void set_gdbarch_construct_inferior_arguments (struct gdbarch *gdbarch, gdbarch_construct_inferior_arguments_ftype *construct_inferior_arguments);

#if defined (DWARF2_BUILD_FRAME_INFO)
/* Legacy for systems yet to multi-arch DWARF2_BUILD_FRAME_INFO */
#if !defined (DWARF2_BUILD_FRAME_INFO_P)
#define DWARF2_BUILD_FRAME_INFO_P() (1)
#endif
#endif

/* Default predicate for non- multi-arch targets. */
#if (!GDB_MULTI_ARCH) && !defined (DWARF2_BUILD_FRAME_INFO_P)
#define DWARF2_BUILD_FRAME_INFO_P() (0)
#endif

extern int gdbarch_dwarf2_build_frame_info_p (struct gdbarch *gdbarch);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (DWARF2_BUILD_FRAME_INFO_P)
#error "Non multi-arch definition of DWARF2_BUILD_FRAME_INFO"
#endif
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (DWARF2_BUILD_FRAME_INFO_P)
#define DWARF2_BUILD_FRAME_INFO_P() (gdbarch_dwarf2_build_frame_info_p (current_gdbarch))
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (DWARF2_BUILD_FRAME_INFO)
#define DWARF2_BUILD_FRAME_INFO(objfile) (internal_error (__FILE__, __LINE__, "DWARF2_BUILD_FRAME_INFO"), 0)
#endif

typedef void (gdbarch_dwarf2_build_frame_info_ftype) (struct objfile *objfile);
extern void gdbarch_dwarf2_build_frame_info (struct gdbarch *gdbarch, struct objfile *objfile);
extern void set_gdbarch_dwarf2_build_frame_info (struct gdbarch *gdbarch, gdbarch_dwarf2_build_frame_info_ftype *dwarf2_build_frame_info);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (DWARF2_BUILD_FRAME_INFO)
#error "Non multi-arch definition of DWARF2_BUILD_FRAME_INFO"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (DWARF2_BUILD_FRAME_INFO)
#define DWARF2_BUILD_FRAME_INFO(objfile) (gdbarch_dwarf2_build_frame_info (current_gdbarch, objfile))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (ELF_MAKE_MSYMBOL_SPECIAL)
#define ELF_MAKE_MSYMBOL_SPECIAL(sym, msym) (default_elf_make_msymbol_special (sym, msym))
#endif

typedef void (gdbarch_elf_make_msymbol_special_ftype) (asymbol *sym, struct minimal_symbol *msym);
extern void gdbarch_elf_make_msymbol_special (struct gdbarch *gdbarch, asymbol *sym, struct minimal_symbol *msym);
extern void set_gdbarch_elf_make_msymbol_special (struct gdbarch *gdbarch, gdbarch_elf_make_msymbol_special_ftype *elf_make_msymbol_special);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (ELF_MAKE_MSYMBOL_SPECIAL)
#error "Non multi-arch definition of ELF_MAKE_MSYMBOL_SPECIAL"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (ELF_MAKE_MSYMBOL_SPECIAL)
#define ELF_MAKE_MSYMBOL_SPECIAL(sym, msym) (gdbarch_elf_make_msymbol_special (current_gdbarch, sym, msym))
#endif
#endif

/* Default (function) for non- multi-arch platforms. */
#if (!GDB_MULTI_ARCH) && !defined (COFF_MAKE_MSYMBOL_SPECIAL)
#define COFF_MAKE_MSYMBOL_SPECIAL(val, msym) (default_coff_make_msymbol_special (val, msym))
#endif

typedef void (gdbarch_coff_make_msymbol_special_ftype) (int val, struct minimal_symbol *msym);
extern void gdbarch_coff_make_msymbol_special (struct gdbarch *gdbarch, int val, struct minimal_symbol *msym);
extern void set_gdbarch_coff_make_msymbol_special (struct gdbarch *gdbarch, gdbarch_coff_make_msymbol_special_ftype *coff_make_msymbol_special);
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) && defined (COFF_MAKE_MSYMBOL_SPECIAL)
#error "Non multi-arch definition of COFF_MAKE_MSYMBOL_SPECIAL"
#endif
#if GDB_MULTI_ARCH
#if (GDB_MULTI_ARCH > GDB_MULTI_ARCH_PARTIAL) || !defined (COFF_MAKE_MSYMBOL_SPECIAL)
#define COFF_MAKE_MSYMBOL_SPECIAL(val, msym) (gdbarch_coff_make_msymbol_special (current_gdbarch, val, msym))
#endif
#endif

extern struct gdbarch_tdep *gdbarch_tdep (struct gdbarch *gdbarch);


/* Mechanism for co-ordinating the selection of a specific
   architecture.

   GDB targets (*-tdep.c) can register an interest in a specific
   architecture.  Other GDB components can register a need to maintain
   per-architecture data.

   The mechanisms below ensures that there is only a loose connection
   between the set-architecture command and the various GDB
   components.  Each component can independently register their need
   to maintain architecture specific data with gdbarch.

   Pragmatics:

   Previously, a single TARGET_ARCHITECTURE_HOOK was provided.  It
   didn't scale.

   The more traditional mega-struct containing architecture specific
   data for all the various GDB components was also considered.  Since
   GDB is built from a variable number of (fairly independent)
   components it was determined that the global aproach was not
   applicable. */


/* Register a new architectural family with GDB.

   Register support for the specified ARCHITECTURE with GDB.  When
   gdbarch determines that the specified architecture has been
   selected, the corresponding INIT function is called.

   --

   The INIT function takes two parameters: INFO which contains the
   information available to gdbarch about the (possibly new)
   architecture; ARCHES which is a list of the previously created
   ``struct gdbarch'' for this architecture.

   The INIT function parameter INFO shall, as far as possible, be
   pre-initialized with information obtained from INFO.ABFD or
   previously selected architecture (if similar).

   The INIT function shall return any of: NULL - indicating that it
   doesn't recognize the selected architecture; an existing ``struct
   gdbarch'' from the ARCHES list - indicating that the new
   architecture is just a synonym for an earlier architecture (see
   gdbarch_list_lookup_by_info()); a newly created ``struct gdbarch''
   - that describes the selected architecture (see gdbarch_alloc()).

   The DUMP_TDEP function shall print out all target specific values.
   Care should be taken to ensure that the function works in both the
   multi-arch and non- multi-arch cases. */

struct gdbarch_list
{
  struct gdbarch *gdbarch;
  struct gdbarch_list *next;
};

struct gdbarch_info
{
  /* Use default: NULL (ZERO). */
  const struct bfd_arch_info *bfd_arch_info;

  /* Use default: BFD_ENDIAN_UNKNOWN (NB: is not ZERO).  */
  int byte_order;

  /* Use default: NULL (ZERO). */
  bfd *abfd;

  /* Use default: NULL (ZERO). */
  struct gdbarch_tdep_info *tdep_info;
};

typedef struct gdbarch *(gdbarch_init_ftype) (struct gdbarch_info info, struct gdbarch_list *arches);
typedef void (gdbarch_dump_tdep_ftype) (struct gdbarch *gdbarch, struct ui_file *file);

/* DEPRECATED - use gdbarch_register() */
extern void register_gdbarch_init (enum bfd_architecture architecture, gdbarch_init_ftype *);

extern void gdbarch_register (enum bfd_architecture architecture,
                              gdbarch_init_ftype *,
                              gdbarch_dump_tdep_ftype *);


/* Return a freshly allocated, NULL terminated, array of the valid
   architecture names.  Since architectures are registered during the
   _initialize phase this function only returns useful information
   once initialization has been completed. */

extern const char **gdbarch_printable_names (void);


/* Helper function.  Search the list of ARCHES for a GDBARCH that
   matches the information provided by INFO. */

extern struct gdbarch_list *gdbarch_list_lookup_by_info (struct gdbarch_list *arches,  const struct gdbarch_info *info);


/* Helper function.  Create a preliminary ``struct gdbarch''.  Perform
   basic initialization using values obtained from the INFO andTDEP
   parameters.  set_gdbarch_*() functions are called to complete the
   initialization of the object. */

extern struct gdbarch *gdbarch_alloc (const struct gdbarch_info *info, struct gdbarch_tdep *tdep);


/* Helper function.  Free a partially-constructed ``struct gdbarch''.
   It is assumed that the caller freeds the ``struct
   gdbarch_tdep''. */

extern void gdbarch_free (struct gdbarch *);


/* Helper function. Force an update of the current architecture.

   The actual architecture selected is determined by INFO, ``(gdb) set
   architecture'' et.al., the existing architecture and BFD's default
   architecture.  INFO should be initialized to zero and then selected
   fields should be updated.

   Returns non-zero if the update succeeds */

extern int gdbarch_update_p (struct gdbarch_info info);



/* Register per-architecture data-pointer.

   Reserve space for a per-architecture data-pointer.  An identifier
   for the reserved data-pointer is returned.  That identifer should
   be saved in a local static variable.

   The per-architecture data-pointer can be initialized in one of two
   ways: The value can be set explicitly using a call to
   set_gdbarch_data(); the value can be set implicitly using the value
   returned by a non-NULL INIT() callback.  INIT(), when non-NULL is
   called after the basic architecture vector has been created.

   When a previously created architecture is re-selected, the
   per-architecture data-pointer for that previous architecture is
   restored.  INIT() is not called.

   During initialization, multiple assignments of the data-pointer are
   allowed, non-NULL values are deleted by calling FREE().  If the
   architecture is deleted using gdbarch_free() all non-NULL data
   pointers are also deleted using FREE().

   Multiple registrarants for any architecture are allowed (and
   strongly encouraged).  */

struct gdbarch_data;

typedef void *(gdbarch_data_init_ftype) (struct gdbarch *gdbarch);
typedef void (gdbarch_data_free_ftype) (struct gdbarch *gdbarch,
					void *pointer);
extern struct gdbarch_data *register_gdbarch_data (gdbarch_data_init_ftype *init,
						   gdbarch_data_free_ftype *free);
extern void set_gdbarch_data (struct gdbarch *gdbarch,
			      struct gdbarch_data *data,
			      void *pointer);

extern void *gdbarch_data (struct gdbarch_data*);


/* Register per-architecture memory region.

   Provide a memory-region swap mechanism.  Per-architecture memory
   region are created.  These memory regions are swapped whenever the
   architecture is changed.  For a new architecture, the memory region
   is initialized with zero (0) and the INIT function is called.

   Memory regions are swapped / initialized in the order that they are
   registered.  NULL DATA and/or INIT values can be specified.

   New code should use register_gdbarch_data(). */

typedef void (gdbarch_swap_ftype) (void);
extern void register_gdbarch_swap (void *data, unsigned long size, gdbarch_swap_ftype *init);
#define REGISTER_GDBARCH_SWAP(VAR) register_gdbarch_swap (&(VAR), sizeof ((VAR)), NULL)



/* The target-system-dependent byte order is dynamic */

extern int target_byte_order;
#ifndef TARGET_BYTE_ORDER
#define TARGET_BYTE_ORDER (target_byte_order + 0)
#endif

extern int target_byte_order_auto;
#ifndef TARGET_BYTE_ORDER_AUTO
#define TARGET_BYTE_ORDER_AUTO (target_byte_order_auto + 0)
#endif



/* The target-system-dependent BFD architecture is dynamic */

extern int target_architecture_auto;
#ifndef TARGET_ARCHITECTURE_AUTO
#define TARGET_ARCHITECTURE_AUTO (target_architecture_auto + 0)
#endif

extern const struct bfd_arch_info *target_architecture;
#ifndef TARGET_ARCHITECTURE
#define TARGET_ARCHITECTURE (target_architecture + 0)
#endif


/* The target-system-dependent disassembler is semi-dynamic */

extern int dis_asm_read_memory (bfd_vma memaddr, bfd_byte *myaddr,
				unsigned int len, disassemble_info *info);

extern void dis_asm_memory_error (int status, bfd_vma memaddr,
				  disassemble_info *info);

extern void dis_asm_print_address (bfd_vma addr,
				   disassemble_info *info);

extern int (*tm_print_insn) (bfd_vma, disassemble_info*);
extern disassemble_info tm_print_insn_info;
#ifndef TARGET_PRINT_INSN_INFO
#define TARGET_PRINT_INSN_INFO (&tm_print_insn_info)
#endif



/* Set the dynamic target-system-dependent parameters (architecture,
   byte-order, ...) using information found in the BFD */

extern void set_gdbarch_from_file (bfd *);


/* Initialize the current architecture to the "first" one we find on
   our list.  */

extern void initialize_current_architecture (void);

/* For non-multiarched targets, do any initialization of the default
   gdbarch object necessary after the _initialize_MODULE functions
   have run.  */
extern void initialize_non_multiarch ();

/* gdbarch trace variable */
extern int gdbarch_debug;

extern void gdbarch_dump (struct gdbarch *gdbarch, struct ui_file *file);

#endif
