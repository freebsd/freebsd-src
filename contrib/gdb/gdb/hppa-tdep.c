/* Target-dependent code for the HP PA architecture, for GDB.
   Copyright 1986, 1987, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
   Free Software Foundation, Inc.

   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).

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

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "value.h"

/* For argument passing to the inferior */
#include "symtab.h"

#ifdef USG
#include <sys/types.h>
#endif

#include <sys/param.h>
#include <signal.h>

#ifdef COFF_ENCAPSULATE
#include "a.out.encap.h"
#else
#endif
#ifndef N_SET_MAGIC
#define N_SET_MAGIC(exec, val) ((exec).a_magic = (val))
#endif

/*#include <sys/user.h>		After a.out.h  */
#include <sys/file.h>
#include "gdb_stat.h"
#include "wait.h"

#include "gdbcore.h"
#include "gdbcmd.h"
#include "target.h"
#include "symfile.h"
#include "objfiles.h"

static int restore_pc_queue PARAMS ((struct frame_saved_regs *));

static int hppa_alignof PARAMS ((struct type *));

CORE_ADDR frame_saved_pc PARAMS ((struct frame_info *));

static int prologue_inst_adjust_sp PARAMS ((unsigned long));

static int is_branch PARAMS ((unsigned long));

static int inst_saves_gr PARAMS ((unsigned long));

static int inst_saves_fr PARAMS ((unsigned long));

static int pc_in_interrupt_handler PARAMS ((CORE_ADDR));

static int pc_in_linker_stub PARAMS ((CORE_ADDR));

static int compare_unwind_entries PARAMS ((const void *, const void *));

static void read_unwind_info PARAMS ((struct objfile *));

static void internalize_unwinds PARAMS ((struct objfile *,
					 struct unwind_table_entry *,
					 asection *, unsigned int,
					 unsigned int, CORE_ADDR));
static void pa_print_registers PARAMS ((char *, int, int));
static void pa_print_fp_reg PARAMS ((int));


/* Routines to extract various sized constants out of hppa 
   instructions. */

/* This assumes that no garbage lies outside of the lower bits of 
   value. */

int
sign_extend (val, bits)
     unsigned val, bits;
{
  return (int)(val >> bits - 1 ? (-1 << bits) | val : val);
}

/* For many immediate values the sign bit is the low bit! */

int
low_sign_extend (val, bits)
     unsigned val, bits;
{
  return (int)((val & 0x1 ? (-1 << (bits - 1)) : 0) | val >> 1);
}
/* extract the immediate field from a ld{bhw}s instruction */

unsigned
get_field (val, from, to)
     unsigned val, from, to;
{
  val = val >> 31 - to;
  return val & ((1 << 32 - from) - 1);
}

unsigned
set_field (val, from, to, new_val)
     unsigned *val, from, to;
{
  unsigned mask = ~((1 << (to - from + 1)) << (31 - from));
  return *val = *val & mask | (new_val << (31 - from));
}

/* extract a 3-bit space register number from a be, ble, mtsp or mfsp */

extract_3 (word)
     unsigned word;
{
  return GET_FIELD (word, 18, 18) << 2 | GET_FIELD (word, 16, 17);
}
       
extract_5_load (word)
     unsigned word;
{
  return low_sign_extend (word >> 16 & MASK_5, 5);
}

/* extract the immediate field from a st{bhw}s instruction */

int
extract_5_store (word)
     unsigned word;
{
  return low_sign_extend (word & MASK_5, 5);
}

/* extract the immediate field from a break instruction */

unsigned
extract_5r_store (word)
     unsigned word;
{
  return (word & MASK_5);
}

/* extract the immediate field from a {sr}sm instruction */

unsigned
extract_5R_store (word)
     unsigned word;
{
  return (word >> 16 & MASK_5);
}

/* extract an 11 bit immediate field */

int
extract_11 (word)
     unsigned word;
{
  return low_sign_extend (word & MASK_11, 11);
}

/* extract a 14 bit immediate field */

int
extract_14 (word)
     unsigned word;
{
  return low_sign_extend (word & MASK_14, 14);
}

/* deposit a 14 bit constant in a word */

unsigned
deposit_14 (opnd, word)
     int opnd;
     unsigned word;
{
  unsigned sign = (opnd < 0 ? 1 : 0);

  return word | ((unsigned)opnd << 1 & MASK_14)  | sign;
}

/* extract a 21 bit constant */

int
extract_21 (word)
     unsigned word;
{
  int val;

  word &= MASK_21;
  word <<= 11;
  val = GET_FIELD (word, 20, 20);
  val <<= 11;
  val |= GET_FIELD (word, 9, 19);
  val <<= 2;
  val |= GET_FIELD (word, 5, 6);
  val <<= 5;
  val |= GET_FIELD (word, 0, 4);
  val <<= 2;
  val |= GET_FIELD (word, 7, 8);
  return sign_extend (val, 21) << 11;
}

/* deposit a 21 bit constant in a word. Although 21 bit constants are
   usually the top 21 bits of a 32 bit constant, we assume that only
   the low 21 bits of opnd are relevant */

unsigned
deposit_21 (opnd, word)
     unsigned opnd, word;
{
  unsigned val = 0;

  val |= GET_FIELD (opnd, 11 + 14, 11 + 18);
  val <<= 2;
  val |= GET_FIELD (opnd, 11 + 12, 11 + 13);
  val <<= 2;
  val |= GET_FIELD (opnd, 11 + 19, 11 + 20);
  val <<= 11;
  val |= GET_FIELD (opnd, 11 + 1, 11 + 11);
  val <<= 1;
  val |= GET_FIELD (opnd, 11 + 0, 11 + 0);
  return word | val;
}

/* extract a 12 bit constant from branch instructions */

int
extract_12 (word)
     unsigned word;
{
  return sign_extend (GET_FIELD (word, 19, 28) |
		      GET_FIELD (word, 29, 29) << 10 |
		      (word & 0x1) << 11, 12) << 2;
}

/* Deposit a 17 bit constant in an instruction (like bl). */

unsigned int
deposit_17 (opnd, word)
     unsigned opnd, word;
{
  word |= GET_FIELD (opnd, 15 + 0, 15 + 0); /* w */
  word |= GET_FIELD (opnd, 15 + 1, 15 + 5) << 16; /* w1 */
  word |= GET_FIELD (opnd, 15 + 6, 15 + 6) << 2; /* w2[10] */
  word |= GET_FIELD (opnd, 15 + 7, 15 + 16) << 3; /* w2[0..9] */

  return word;
}

/* extract a 17 bit constant from branch instructions, returning the
   19 bit signed value. */

int
extract_17 (word)
     unsigned word;
{
  return sign_extend (GET_FIELD (word, 19, 28) |
		      GET_FIELD (word, 29, 29) << 10 |
		      GET_FIELD (word, 11, 15) << 11 |
		      (word & 0x1) << 16, 17) << 2;
}


/* Compare the start address for two unwind entries returning 1 if 
   the first address is larger than the second, -1 if the second is
   larger than the first, and zero if they are equal.  */

static int
compare_unwind_entries (arg1, arg2)
     const void *arg1;
     const void *arg2;
{
  const struct unwind_table_entry *a = arg1;
  const struct unwind_table_entry *b = arg2;

  if (a->region_start > b->region_start)
    return 1;
  else if (a->region_start < b->region_start)
    return -1;
  else
    return 0;
}

static void
internalize_unwinds (objfile, table, section, entries, size, text_offset)
     struct objfile *objfile;
     struct unwind_table_entry *table;
     asection *section;
     unsigned int entries, size;
     CORE_ADDR text_offset;
{
  /* We will read the unwind entries into temporary memory, then
     fill in the actual unwind table.  */
  if (size > 0)
    {
      unsigned long tmp;
      unsigned i;
      char *buf = alloca (size);

      bfd_get_section_contents (objfile->obfd, section, buf, 0, size);

      /* Now internalize the information being careful to handle host/target
	 endian issues.  */
      for (i = 0; i < entries; i++)
	{
	  table[i].region_start = bfd_get_32 (objfile->obfd,
						  (bfd_byte *)buf);
	  table[i].region_start += text_offset;
	  buf += 4;
	  table[i].region_end = bfd_get_32 (objfile->obfd, (bfd_byte *)buf);
	  table[i].region_end += text_offset;
	  buf += 4;
	  tmp = bfd_get_32 (objfile->obfd, (bfd_byte *)buf);
	  buf += 4;
	  table[i].Cannot_unwind = (tmp >> 31) & 0x1;
	  table[i].Millicode = (tmp >> 30) & 0x1;
	  table[i].Millicode_save_sr0 = (tmp >> 29) & 0x1;
	  table[i].Region_description = (tmp >> 27) & 0x3;
	  table[i].reserved1 = (tmp >> 26) & 0x1;
	  table[i].Entry_SR = (tmp >> 25) & 0x1;
	  table[i].Entry_FR = (tmp >> 21) & 0xf;
	  table[i].Entry_GR = (tmp >> 16) & 0x1f;
	  table[i].Args_stored = (tmp >> 15) & 0x1;
	  table[i].Variable_Frame = (tmp >> 14) & 0x1;
	  table[i].Separate_Package_Body = (tmp >> 13) & 0x1;
	  table[i].Frame_Extension_Millicode = (tmp >> 12 ) & 0x1;
	  table[i].Stack_Overflow_Check = (tmp >> 11) & 0x1;
	  table[i].Two_Instruction_SP_Increment = (tmp >> 10) & 0x1;
	  table[i].Ada_Region = (tmp >> 9) & 0x1;
	  table[i].reserved2 = (tmp >> 5) & 0xf;
	  table[i].Save_SP = (tmp >> 4) & 0x1;
	  table[i].Save_RP = (tmp >> 3) & 0x1;
	  table[i].Save_MRP_in_frame = (tmp >> 2) & 0x1;
	  table[i].extn_ptr_defined = (tmp >> 1) & 0x1;
	  table[i].Cleanup_defined = tmp & 0x1;
	  tmp = bfd_get_32 (objfile->obfd, (bfd_byte *)buf);
	  buf += 4;
	  table[i].MPE_XL_interrupt_marker = (tmp >> 31) & 0x1;
	  table[i].HP_UX_interrupt_marker = (tmp >> 30) & 0x1;
	  table[i].Large_frame = (tmp >> 29) & 0x1;
	  table[i].reserved4 = (tmp >> 27) & 0x3;
	  table[i].Total_frame_size = tmp & 0x7ffffff;
	}
    }
}

/* Read in the backtrace information stored in the `$UNWIND_START$' section of
   the object file.  This info is used mainly by find_unwind_entry() to find
   out the stack frame size and frame pointer used by procedures.  We put
   everything on the psymbol obstack in the objfile so that it automatically
   gets freed when the objfile is destroyed.  */

static void
read_unwind_info (objfile)
     struct objfile *objfile;
{
  asection *unwind_sec, *elf_unwind_sec, *stub_unwind_sec;
  unsigned unwind_size, elf_unwind_size, stub_unwind_size, total_size;
  unsigned index, unwind_entries, elf_unwind_entries;
  unsigned stub_entries, total_entries;
  CORE_ADDR text_offset;
  struct obj_unwind_info *ui;

  text_offset = ANOFFSET (objfile->section_offsets, 0);
  ui = (struct obj_unwind_info *)obstack_alloc (&objfile->psymbol_obstack,
						sizeof (struct obj_unwind_info));

  ui->table = NULL;
  ui->cache = NULL;
  ui->last = -1;

  /* Get hooks to all unwind sections.   Note there is no linker-stub unwind
     section in ELF at the moment.  */
  unwind_sec = bfd_get_section_by_name (objfile->obfd, "$UNWIND_START$");
  elf_unwind_sec = bfd_get_section_by_name (objfile->obfd, ".PARISC.unwind");
  stub_unwind_sec = bfd_get_section_by_name (objfile->obfd, "$UNWIND_END$");

  /* Get sizes and unwind counts for all sections.  */
  if (unwind_sec)
    {
      unwind_size = bfd_section_size (objfile->obfd, unwind_sec);
      unwind_entries = unwind_size / UNWIND_ENTRY_SIZE;
    }
  else
    {
      unwind_size = 0;
      unwind_entries = 0;
    }

  if (elf_unwind_sec)
    {
      elf_unwind_size = bfd_section_size (objfile->obfd, elf_unwind_sec);
      elf_unwind_entries = elf_unwind_size / UNWIND_ENTRY_SIZE;
    }
  else
    {
      elf_unwind_size = 0;
      elf_unwind_entries = 0;
    }

  if (stub_unwind_sec)
    {
      stub_unwind_size = bfd_section_size (objfile->obfd, stub_unwind_sec);
      stub_entries = stub_unwind_size / STUB_UNWIND_ENTRY_SIZE;
    }
  else
    {
      stub_unwind_size = 0;
      stub_entries = 0;
    }

  /* Compute total number of unwind entries and their total size.  */
  total_entries = unwind_entries + elf_unwind_entries + stub_entries;
  total_size = total_entries * sizeof (struct unwind_table_entry);

  /* Allocate memory for the unwind table.  */
  ui->table = obstack_alloc (&objfile->psymbol_obstack, total_size);
  ui->last = total_entries - 1;

  /* Internalize the standard unwind entries.  */
  index = 0;
  internalize_unwinds (objfile, &ui->table[index], unwind_sec,
		       unwind_entries, unwind_size, text_offset);
  index += unwind_entries;
  internalize_unwinds (objfile, &ui->table[index], elf_unwind_sec,
		       elf_unwind_entries, elf_unwind_size, text_offset);
  index += elf_unwind_entries;

  /* Now internalize the stub unwind entries.  */
  if (stub_unwind_size > 0)
    {
      unsigned int i;
      char *buf = alloca (stub_unwind_size);

      /* Read in the stub unwind entries.  */
      bfd_get_section_contents (objfile->obfd, stub_unwind_sec, buf,
				0, stub_unwind_size);

      /* Now convert them into regular unwind entries.  */
      for (i = 0; i < stub_entries; i++, index++)
	{
	  /* Clear out the next unwind entry.  */
	  memset (&ui->table[index], 0, sizeof (struct unwind_table_entry));

	  /* Convert offset & size into region_start and region_end.  
	     Stuff away the stub type into "reserved" fields.  */
	  ui->table[index].region_start = bfd_get_32 (objfile->obfd,
						      (bfd_byte *) buf);
	  ui->table[index].region_start += text_offset;
	  buf += 4;
	  ui->table[index].stub_type = bfd_get_8 (objfile->obfd,
						  (bfd_byte *) buf);
	  buf += 2;
	  ui->table[index].region_end
	    = ui->table[index].region_start + 4 * 
	      (bfd_get_16 (objfile->obfd, (bfd_byte *) buf) - 1);
	  buf += 2;
	}

    }

  /* Unwind table needs to be kept sorted.  */
  qsort (ui->table, total_entries, sizeof (struct unwind_table_entry),
	 compare_unwind_entries);

  /* Keep a pointer to the unwind information.  */
  objfile->obj_private = (PTR) ui;
}

/* Lookup the unwind (stack backtrace) info for the given PC.  We search all
   of the objfiles seeking the unwind table entry for this PC.  Each objfile
   contains a sorted list of struct unwind_table_entry.  Since we do a binary
   search of the unwind tables, we depend upon them to be sorted.  */

struct unwind_table_entry *
find_unwind_entry(pc)
     CORE_ADDR pc;
{
  int first, middle, last;
  struct objfile *objfile;

  ALL_OBJFILES (objfile)
    {
      struct obj_unwind_info *ui;

      ui = OBJ_UNWIND_INFO (objfile);

      if (!ui)
	{
	  read_unwind_info (objfile);
	  ui = OBJ_UNWIND_INFO (objfile);
	}

      /* First, check the cache */

      if (ui->cache
	  && pc >= ui->cache->region_start
	  && pc <= ui->cache->region_end)
	return ui->cache;

      /* Not in the cache, do a binary search */

      first = 0;
      last = ui->last;

      while (first <= last)
	{
	  middle = (first + last) / 2;
	  if (pc >= ui->table[middle].region_start
	      && pc <= ui->table[middle].region_end)
	    {
	      ui->cache = &ui->table[middle];
	      return &ui->table[middle];
	    }

	  if (pc < ui->table[middle].region_start)
	    last = middle - 1;
	  else
	    first = middle + 1;
	}
    }				/* ALL_OBJFILES() */
  return NULL;
}

/* Return the adjustment necessary to make for addresses on the stack
   as presented by hpread.c.

   This is necessary because of the stack direction on the PA and the
   bizarre way in which someone (?) decided they wanted to handle
   frame pointerless code in GDB.  */
int
hpread_adjust_stack_address (func_addr)
     CORE_ADDR func_addr;
{
  struct unwind_table_entry *u;

  u = find_unwind_entry (func_addr);
  if (!u)
    return 0;
  else
    return u->Total_frame_size << 3;
}

/* Called to determine if PC is in an interrupt handler of some
   kind.  */

static int
pc_in_interrupt_handler (pc)
     CORE_ADDR pc;
{
  struct unwind_table_entry *u;
  struct minimal_symbol *msym_us;

  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* Oh joys.  HPUX sets the interrupt bit for _sigreturn even though
     its frame isn't a pure interrupt frame.  Deal with this.  */
  msym_us = lookup_minimal_symbol_by_pc (pc);

  return u->HP_UX_interrupt_marker && !IN_SIGTRAMP (pc, SYMBOL_NAME (msym_us));
}

/* Called when no unwind descriptor was found for PC.  Returns 1 if it
   appears that PC is in a linker stub.  */

static int
pc_in_linker_stub (pc)
     CORE_ADDR pc;
{
  int found_magic_instruction = 0;
  int i;
  char buf[4];

  /* If unable to read memory, assume pc is not in a linker stub.  */
  if (target_read_memory (pc, buf, 4) != 0)
    return 0;

  /* We are looking for something like

     ; $$dyncall jams RP into this special spot in the frame (RP')
     ; before calling the "call stub"
     ldw     -18(sp),rp

     ldsid   (rp),r1         ; Get space associated with RP into r1
     mtsp    r1,sp           ; Move it into space register 0
     be,n    0(sr0),rp)      ; back to your regularly scheduled program
     */

  /* Maximum known linker stub size is 4 instructions.  Search forward
     from the given PC, then backward.  */
  for (i = 0; i < 4; i++)
    {
      /* If we hit something with an unwind, stop searching this direction.  */

      if (find_unwind_entry (pc + i * 4) != 0)
	break;

      /* Check for ldsid (rp),r1 which is the magic instruction for a 
	 return from a cross-space function call.  */
      if (read_memory_integer (pc + i * 4, 4) == 0x004010a1)
	{
	  found_magic_instruction = 1;
	  break;
	}
      /* Add code to handle long call/branch and argument relocation stubs
	 here.  */
    }

  if (found_magic_instruction != 0)
    return 1;

  /* Now look backward.  */
  for (i = 0; i < 4; i++)
    {
      /* If we hit something with an unwind, stop searching this direction.  */

      if (find_unwind_entry (pc - i * 4) != 0)
	break;

      /* Check for ldsid (rp),r1 which is the magic instruction for a 
	 return from a cross-space function call.  */
      if (read_memory_integer (pc - i * 4, 4) == 0x004010a1)
	{
	  found_magic_instruction = 1;
	  break;
	}
      /* Add code to handle long call/branch and argument relocation stubs
	 here.  */
    }
  return found_magic_instruction;
}

static int
find_return_regnum(pc)
     CORE_ADDR pc;
{
  struct unwind_table_entry *u;

  u = find_unwind_entry (pc);

  if (!u)
    return RP_REGNUM;

  if (u->Millicode)
    return 31;

  return RP_REGNUM;
}

/* Return size of frame, or -1 if we should use a frame pointer.  */
int
find_proc_framesize (pc)
     CORE_ADDR pc;
{
  struct unwind_table_entry *u;
  struct minimal_symbol *msym_us;

  u = find_unwind_entry (pc);

  if (!u)
    {
      if (pc_in_linker_stub (pc))
	/* Linker stubs have a zero size frame.  */
	return 0;
      else
	return -1;
    }

  msym_us = lookup_minimal_symbol_by_pc (pc);

  /* If Save_SP is set, and we're not in an interrupt or signal caller,
     then we have a frame pointer.  Use it.  */
  if (u->Save_SP && !pc_in_interrupt_handler (pc)
      && !IN_SIGTRAMP (pc, SYMBOL_NAME (msym_us)))
    return -1;

  return u->Total_frame_size << 3;
}

/* Return offset from sp at which rp is saved, or 0 if not saved.  */
static int rp_saved PARAMS ((CORE_ADDR));

static int
rp_saved (pc)
     CORE_ADDR pc;
{
  struct unwind_table_entry *u;

  u = find_unwind_entry (pc);

  if (!u)
    {
      if (pc_in_linker_stub (pc))
	/* This is the so-called RP'.  */
	return -24;
      else
	return 0;
    }

  if (u->Save_RP)
    return -20;
  else if (u->stub_type != 0)
    {
      switch (u->stub_type)
	{
	case EXPORT:
	case IMPORT:
	  return -24;
	case PARAMETER_RELOCATION:
	  return -8;
	default:
	  return 0;
	}
    }
  else
    return 0;
}

int
frameless_function_invocation (frame)
     struct frame_info *frame;
{
  struct unwind_table_entry *u;

  u = find_unwind_entry (frame->pc);

  if (u == 0)
    return 0;

  return (u->Total_frame_size == 0 && u->stub_type == 0);
}

CORE_ADDR
saved_pc_after_call (frame)
     struct frame_info *frame;
{
  int ret_regnum;
  CORE_ADDR pc;
  struct unwind_table_entry *u;

  ret_regnum = find_return_regnum (get_frame_pc (frame));
  pc = read_register (ret_regnum) & ~0x3;
  
  /* If PC is in a linker stub, then we need to dig the address
     the stub will return to out of the stack.  */
  u = find_unwind_entry (pc);
  if (u && u->stub_type != 0)
    return frame_saved_pc (frame);
  else
    return pc;
}

CORE_ADDR
frame_saved_pc (frame)
     struct frame_info *frame;
{
  CORE_ADDR pc = get_frame_pc (frame);
  struct unwind_table_entry *u;

  /* BSD, HPUX & OSF1 all lay out the hardware state in the same manner
     at the base of the frame in an interrupt handler.  Registers within
     are saved in the exact same order as GDB numbers registers.  How
     convienent.  */
  if (pc_in_interrupt_handler (pc))
    return read_memory_integer (frame->frame + PC_REGNUM * 4, 4) & ~0x3;

#ifdef FRAME_SAVED_PC_IN_SIGTRAMP
  /* Deal with signal handler caller frames too.  */
  if (frame->signal_handler_caller)
    {
      CORE_ADDR rp;
      FRAME_SAVED_PC_IN_SIGTRAMP (frame, &rp);
      return rp & ~0x3;
    }
#endif

  if (frameless_function_invocation (frame))
    {
      int ret_regnum;

      ret_regnum = find_return_regnum (pc);

      /* If the next frame is an interrupt frame or a signal
	 handler caller, then we need to look in the saved
	 register area to get the return pointer (the values
	 in the registers may not correspond to anything useful).  */
      if (frame->next 
	  && (frame->next->signal_handler_caller
	      || pc_in_interrupt_handler (frame->next->pc)))
	{
	  struct frame_saved_regs saved_regs;

	  get_frame_saved_regs (frame->next, &saved_regs);
	  if (read_memory_integer (saved_regs.regs[FLAGS_REGNUM], 4) & 0x2)
	    {
	      pc = read_memory_integer (saved_regs.regs[31], 4) & ~0x3;

	      /* Syscalls are really two frames.  The syscall stub itself
		 with a return pointer in %rp and the kernel call with
		 a return pointer in %r31.  We return the %rp variant
		 if %r31 is the same as frame->pc.  */
	      if (pc == frame->pc)
		pc = read_memory_integer (saved_regs.regs[RP_REGNUM], 4) & ~0x3;
	    }
	  else
	    pc = read_memory_integer (saved_regs.regs[RP_REGNUM], 4) & ~0x3;
	}
      else
	pc = read_register (ret_regnum) & ~0x3;
    }
  else
    {
      int rp_offset;

restart:
      rp_offset = rp_saved (pc);
      /* Similar to code in frameless function case.  If the next
	 frame is a signal or interrupt handler, then dig the right
	 information out of the saved register info.  */
      if (rp_offset == 0
	  && frame->next
	  && (frame->next->signal_handler_caller
	      || pc_in_interrupt_handler (frame->next->pc)))
	{
	  struct frame_saved_regs saved_regs;

	  get_frame_saved_regs (frame->next, &saved_regs);
	  if (read_memory_integer (saved_regs.regs[FLAGS_REGNUM], 4) & 0x2)
	    {
	      pc = read_memory_integer (saved_regs.regs[31], 4) & ~0x3;

	      /* Syscalls are really two frames.  The syscall stub itself
		 with a return pointer in %rp and the kernel call with
		 a return pointer in %r31.  We return the %rp variant
		 if %r31 is the same as frame->pc.  */
	      if (pc == frame->pc)
		pc = read_memory_integer (saved_regs.regs[RP_REGNUM], 4) & ~0x3;
	    }
	  else
	    pc = read_memory_integer (saved_regs.regs[RP_REGNUM], 4) & ~0x3;
	}
      else if (rp_offset == 0)
	pc = read_register (RP_REGNUM) & ~0x3;
      else
	pc = read_memory_integer (frame->frame + rp_offset, 4) & ~0x3;
    }

  /* If PC is inside a linker stub, then dig out the address the stub
     will return to. 

     Don't do this for long branch stubs.  Why?  For some unknown reason
     _start is marked as a long branch stub in hpux10.  */
  u = find_unwind_entry (pc);
  if (u && u->stub_type != 0
      && u->stub_type != LONG_BRANCH)
    {
      unsigned int insn;

      /* If this is a dynamic executable, and we're in a signal handler,
	 then the call chain will eventually point us into the stub for
	 _sigreturn.  Unlike most cases, we'll be pointed to the branch
	 to the real sigreturn rather than the code after the real branch!. 

	 Else, try to dig the address the stub will return to in the normal
	 fashion.  */
      insn = read_memory_integer (pc, 4);
      if ((insn & 0xfc00e000) == 0xe8000000)
	return (pc + extract_17 (insn) + 8) & ~0x3;
      else
	goto restart;
    }

  return pc;
}

/* We need to correct the PC and the FP for the outermost frame when we are
   in a system call.  */

void
init_extra_frame_info (fromleaf, frame)
     int fromleaf;
     struct frame_info *frame;
{
  int flags;
  int framesize;

  if (frame->next && !fromleaf)
    return;

  /* If the next frame represents a frameless function invocation
     then we have to do some adjustments that are normally done by
     FRAME_CHAIN.  (FRAME_CHAIN is not called in this case.)  */
  if (fromleaf)
    {
      /* Find the framesize of *this* frame without peeking at the PC
	 in the current frame structure (it isn't set yet).  */
      framesize = find_proc_framesize (FRAME_SAVED_PC (get_next_frame (frame)));

      /* Now adjust our base frame accordingly.  If we have a frame pointer
	 use it, else subtract the size of this frame from the current
	 frame.  (we always want frame->frame to point at the lowest address
	 in the frame).  */
      if (framesize == -1)
	frame->frame = read_register (FP_REGNUM);
      else
	frame->frame -= framesize;
      return;
    }

  flags = read_register (FLAGS_REGNUM);
  if (flags & 2)	/* In system call? */
    frame->pc = read_register (31) & ~0x3;

  /* The outermost frame is always derived from PC-framesize

     One might think frameless innermost frames should have
     a frame->frame that is the same as the parent's frame->frame.
     That is wrong; frame->frame in that case should be the *high*
     address of the parent's frame.  It's complicated as hell to
     explain, but the parent *always* creates some stack space for
     the child.  So the child actually does have a frame of some
     sorts, and its base is the high address in its parent's frame.  */
  framesize = find_proc_framesize(frame->pc);
  if (framesize == -1)
    frame->frame = read_register (FP_REGNUM);
  else
    frame->frame = read_register (SP_REGNUM) - framesize;
}

/* Given a GDB frame, determine the address of the calling function's frame.
   This will be used to create a new GDB frame struct, and then
   INIT_EXTRA_FRAME_INFO and INIT_FRAME_PC will be called for the new frame.

   This may involve searching through prologues for several functions
   at boundaries where GCC calls HP C code, or where code which has
   a frame pointer calls code without a frame pointer.  */

CORE_ADDR
frame_chain (frame)
     struct frame_info *frame;
{
  int my_framesize, caller_framesize;
  struct unwind_table_entry *u;
  CORE_ADDR frame_base;
  struct frame_info *tmp_frame;

  /* Handle HPUX, BSD, and OSF1 style interrupt frames first.  These
     are easy; at *sp we have a full save state strucutre which we can
     pull the old stack pointer from.  Also see frame_saved_pc for
     code to dig a saved PC out of the save state structure.  */
  if (pc_in_interrupt_handler (frame->pc))
    frame_base = read_memory_integer (frame->frame + SP_REGNUM * 4, 4);
#ifdef FRAME_BASE_BEFORE_SIGTRAMP
  else if (frame->signal_handler_caller)
    {
      FRAME_BASE_BEFORE_SIGTRAMP (frame, &frame_base);
    }
#endif
  else
    frame_base = frame->frame;

  /* Get frame sizes for the current frame and the frame of the 
     caller.  */
  my_framesize = find_proc_framesize (frame->pc);
  caller_framesize = find_proc_framesize (FRAME_SAVED_PC(frame));

  /* If caller does not have a frame pointer, then its frame
     can be found at current_frame - caller_framesize.  */
  if (caller_framesize != -1)
    return frame_base - caller_framesize;

  /* Both caller and callee have frame pointers and are GCC compiled
     (SAVE_SP bit in unwind descriptor is on for both functions.
     The previous frame pointer is found at the top of the current frame.  */
  if (caller_framesize == -1 && my_framesize == -1)
    return read_memory_integer (frame_base, 4);

  /* Caller has a frame pointer, but callee does not.  This is a little
     more difficult as GCC and HP C lay out locals and callee register save
     areas very differently.

     The previous frame pointer could be in a register, or in one of 
     several areas on the stack.

     Walk from the current frame to the innermost frame examining 
     unwind descriptors to determine if %r3 ever gets saved into the
     stack.  If so return whatever value got saved into the stack.
     If it was never saved in the stack, then the value in %r3 is still
     valid, so use it. 

     We use information from unwind descriptors to determine if %r3
     is saved into the stack (Entry_GR field has this information).  */

  tmp_frame = frame;
  while (tmp_frame)
    {
      u = find_unwind_entry (tmp_frame->pc);

      if (!u)
	{
	  /* We could find this information by examining prologues.  I don't
	     think anyone has actually written any tools (not even "strip")
	     which leave them out of an executable, so maybe this is a moot
	     point.  */
	  warning ("Unable to find unwind for PC 0x%x -- Help!", tmp_frame->pc);
	  return 0;
	}

      /* Entry_GR specifies the number of callee-saved general registers
	 saved in the stack.  It starts at %r3, so %r3 would be 1.  */
      if (u->Entry_GR >= 1 || u->Save_SP
	  || tmp_frame->signal_handler_caller
	  || pc_in_interrupt_handler (tmp_frame->pc))
	break;
      else
	tmp_frame = tmp_frame->next;
    }

  if (tmp_frame)
    {
      /* We may have walked down the chain into a function with a frame
	 pointer.  */
      if (u->Save_SP
	  && !tmp_frame->signal_handler_caller
	  && !pc_in_interrupt_handler (tmp_frame->pc))
	return read_memory_integer (tmp_frame->frame, 4);
      /* %r3 was saved somewhere in the stack.  Dig it out.  */
      else 
	{
	  struct frame_saved_regs saved_regs;

	  /* Sick.

	     For optimization purposes many kernels don't have the
	     callee saved registers into the save_state structure upon
	     entry into the kernel for a syscall; the optimization
	     is usually turned off if the process is being traced so
	     that the debugger can get full register state for the
	     process.
	      
	     This scheme works well except for two cases:

	       * Attaching to a process when the process is in the
	       kernel performing a system call (debugger can't get
	       full register state for the inferior process since
	       the process wasn't being traced when it entered the
	       system call).

	       * Register state is not complete if the system call
	       causes the process to core dump.


	     The following heinous code is an attempt to deal with
	     the lack of register state in a core dump.  It will
	     fail miserably if the function which performs the
	     system call has a variable sized stack frame.  */

	  get_frame_saved_regs (tmp_frame, &saved_regs);

	  /* Abominable hack.  */
	  if (current_target.to_has_execution == 0
	      && ((saved_regs.regs[FLAGS_REGNUM]
	           && (read_memory_integer (saved_regs.regs[FLAGS_REGNUM], 4)
		       & 0x2))
		  || (saved_regs.regs[FLAGS_REGNUM] == 0
		      && read_register (FLAGS_REGNUM) & 0x2)))
	    {
	      u = find_unwind_entry (FRAME_SAVED_PC (frame));
	      if (!u)
		return read_memory_integer (saved_regs.regs[FP_REGNUM], 4);
	      else
		return frame_base - (u->Total_frame_size << 3);
	    }
	
	  return read_memory_integer (saved_regs.regs[FP_REGNUM], 4);
	}
    }
  else
    {
      struct frame_saved_regs saved_regs;

      /* Get the innermost frame.  */
      tmp_frame = frame;
      while (tmp_frame->next != NULL)
	tmp_frame = tmp_frame->next;

      get_frame_saved_regs (tmp_frame, &saved_regs);
      /* Abominable hack.  See above.  */
      if (current_target.to_has_execution == 0
	  && ((saved_regs.regs[FLAGS_REGNUM]
	       && (read_memory_integer (saved_regs.regs[FLAGS_REGNUM], 4)
		   & 0x2))
	      || (saved_regs.regs[FLAGS_REGNUM] == 0
		  && read_register (FLAGS_REGNUM)  & 0x2)))
	{
	  u = find_unwind_entry (FRAME_SAVED_PC (frame));
	  if (!u)
	    return read_memory_integer (saved_regs.regs[FP_REGNUM], 4);
	   else
	    return frame_base - (u->Total_frame_size << 3);
	}
	
      /* The value in %r3 was never saved into the stack (thus %r3 still
	 holds the value of the previous frame pointer).  */
      return read_register (FP_REGNUM);
    }
}


/* To see if a frame chain is valid, see if the caller looks like it
   was compiled with gcc. */

int
frame_chain_valid (chain, thisframe)
     CORE_ADDR chain;
     struct frame_info *thisframe;
{
  struct minimal_symbol *msym_us;
  struct minimal_symbol *msym_start;
  struct unwind_table_entry *u, *next_u = NULL;
  struct frame_info *next;

  if (!chain)
    return 0;

  u = find_unwind_entry (thisframe->pc);

  if (u == NULL)
    return 1;

  /* We can't just check that the same of msym_us is "_start", because
     someone idiotically decided that they were going to make a Ltext_end
     symbol with the same address.  This Ltext_end symbol is totally
     indistinguishable (as nearly as I can tell) from the symbol for a function
     which is (legitimately, since it is in the user's namespace)
     named Ltext_end, so we can't just ignore it.  */
  msym_us = lookup_minimal_symbol_by_pc (FRAME_SAVED_PC (thisframe));
  msym_start = lookup_minimal_symbol ("_start", NULL, NULL);
  if (msym_us
      && msym_start
      && SYMBOL_VALUE_ADDRESS (msym_us) == SYMBOL_VALUE_ADDRESS (msym_start))
    return 0;

  /* Grrrr.  Some new idiot decided that they don't want _start for the
     PRO configurations; $START$ calls main directly....  Deal with it.  */
  msym_start = lookup_minimal_symbol ("$START$", NULL, NULL);
  if (msym_us
      && msym_start
      && SYMBOL_VALUE_ADDRESS (msym_us) == SYMBOL_VALUE_ADDRESS (msym_start))
    return 0;

  next = get_next_frame (thisframe);
  if (next)
    next_u = find_unwind_entry (next->pc);

  /* If this frame does not save SP, has no stack, isn't a stub,
     and doesn't "call" an interrupt routine or signal handler caller,
     then its not valid.  */
  if (u->Save_SP || u->Total_frame_size || u->stub_type != 0
      || (thisframe->next && thisframe->next->signal_handler_caller)
      || (next_u && next_u->HP_UX_interrupt_marker))
    return 1;

  if (pc_in_linker_stub (thisframe->pc))
    return 1;

  return 0;
}

/*
 * These functions deal with saving and restoring register state
 * around a function call in the inferior. They keep the stack
 * double-word aligned; eventually, on an hp700, the stack will have
 * to be aligned to a 64-byte boundary.
 */

void
push_dummy_frame (inf_status)
     struct inferior_status *inf_status;
{
  CORE_ADDR sp, pc, pcspace;
  register int regnum;
  int int_buffer;
  double freg_buffer;

  /* Oh, what a hack.  If we're trying to perform an inferior call
     while the inferior is asleep, we have to make sure to clear
     the "in system call" bit in the flag register (the call will
     start after the syscall returns, so we're no longer in the system
     call!)  This state is kept in "inf_status", change it there.

     We also need a number of horrid hacks to deal with lossage in the
     PC queue registers (apparently they're not valid when the in syscall
     bit is set).  */
  pc = target_read_pc (inferior_pid);
  int_buffer = read_register (FLAGS_REGNUM);
  if (int_buffer & 0x2)
    {
      unsigned int sid;
      int_buffer &= ~0x2;
      memcpy (inf_status->registers, &int_buffer, 4);
      memcpy (inf_status->registers + REGISTER_BYTE (PCOQ_HEAD_REGNUM), &pc, 4);
      pc += 4;
      memcpy (inf_status->registers + REGISTER_BYTE (PCOQ_TAIL_REGNUM), &pc, 4);
      pc -= 4;
      sid = (pc >> 30) & 0x3;
      if (sid == 0)
	pcspace = read_register (SR4_REGNUM);
      else
	pcspace = read_register (SR4_REGNUM + 4 + sid);
      memcpy (inf_status->registers + REGISTER_BYTE (PCSQ_HEAD_REGNUM),
	      &pcspace, 4);
      memcpy (inf_status->registers + REGISTER_BYTE (PCSQ_TAIL_REGNUM),
	      &pcspace, 4);
    }
  else
    pcspace = read_register (PCSQ_HEAD_REGNUM);

  /* Space for "arguments"; the RP goes in here. */
  sp = read_register (SP_REGNUM) + 48;
  int_buffer = read_register (RP_REGNUM) | 0x3;
  write_memory (sp - 20, (char *)&int_buffer, 4);

  int_buffer = read_register (FP_REGNUM);
  write_memory (sp, (char *)&int_buffer, 4);

  write_register (FP_REGNUM, sp);

  sp += 8;

  for (regnum = 1; regnum < 32; regnum++)
    if (regnum != RP_REGNUM && regnum != FP_REGNUM)
      sp = push_word (sp, read_register (regnum));

  sp += 4;

  for (regnum = FP0_REGNUM; regnum < NUM_REGS; regnum++)
    {
      read_register_bytes (REGISTER_BYTE (regnum), (char *)&freg_buffer, 8);
      sp = push_bytes (sp, (char *)&freg_buffer, 8);
    }
  sp = push_word (sp, read_register (IPSW_REGNUM));
  sp = push_word (sp, read_register (SAR_REGNUM));
  sp = push_word (sp, pc);
  sp = push_word (sp, pcspace);
  sp = push_word (sp, pc + 4);
  sp = push_word (sp, pcspace);
  write_register (SP_REGNUM, sp);
}

void
find_dummy_frame_regs (frame, frame_saved_regs)
     struct frame_info *frame;
     struct frame_saved_regs *frame_saved_regs;
{
  CORE_ADDR fp = frame->frame;
  int i;

  frame_saved_regs->regs[RP_REGNUM] = fp - 20 & ~0x3;
  frame_saved_regs->regs[FP_REGNUM] = fp;
  frame_saved_regs->regs[1] = fp + 8;

  for (fp += 12, i = 3; i < 32; i++)
    {
      if (i != FP_REGNUM)
	{
	  frame_saved_regs->regs[i] = fp;
	  fp += 4;
	}
    }

  fp += 4;
  for (i = FP0_REGNUM; i < NUM_REGS; i++, fp += 8)
    frame_saved_regs->regs[i] = fp;

  frame_saved_regs->regs[IPSW_REGNUM] = fp;
  frame_saved_regs->regs[SAR_REGNUM] = fp + 4;
  frame_saved_regs->regs[PCOQ_HEAD_REGNUM] = fp + 8;
  frame_saved_regs->regs[PCSQ_HEAD_REGNUM] = fp + 12;
  frame_saved_regs->regs[PCOQ_TAIL_REGNUM] = fp + 16;
  frame_saved_regs->regs[PCSQ_TAIL_REGNUM] = fp + 20;
}

void
hppa_pop_frame ()
{
  register struct frame_info *frame = get_current_frame ();
  register CORE_ADDR fp, npc, target_pc;
  register int regnum;
  struct frame_saved_regs fsr;
  double freg_buffer;

  fp = FRAME_FP (frame);
  get_frame_saved_regs (frame, &fsr);

#ifndef NO_PC_SPACE_QUEUE_RESTORE
  if (fsr.regs[IPSW_REGNUM])    /* Restoring a call dummy frame */
    restore_pc_queue (&fsr);
#endif

  for (regnum = 31; regnum > 0; regnum--)
    if (fsr.regs[regnum])
      write_register (regnum, read_memory_integer (fsr.regs[regnum], 4));

  for (regnum = NUM_REGS - 1; regnum >= FP0_REGNUM ; regnum--)
    if (fsr.regs[regnum])
      {
	read_memory (fsr.regs[regnum], (char *)&freg_buffer, 8);
        write_register_bytes (REGISTER_BYTE (regnum), (char *)&freg_buffer, 8);
      }

  if (fsr.regs[IPSW_REGNUM])
    write_register (IPSW_REGNUM,
                    read_memory_integer (fsr.regs[IPSW_REGNUM], 4));

  if (fsr.regs[SAR_REGNUM])
    write_register (SAR_REGNUM,
                    read_memory_integer (fsr.regs[SAR_REGNUM], 4));

  /* If the PC was explicitly saved, then just restore it.  */
  if (fsr.regs[PCOQ_TAIL_REGNUM])
    {
      npc = read_memory_integer (fsr.regs[PCOQ_TAIL_REGNUM], 4);
      write_register (PCOQ_TAIL_REGNUM, npc);
    }
  /* Else use the value in %rp to set the new PC.  */
  else 
    {
      npc = read_register (RP_REGNUM);
      target_write_pc (npc, 0);
    }

  write_register (FP_REGNUM, read_memory_integer (fp, 4));

  if (fsr.regs[IPSW_REGNUM])    /* call dummy */
    write_register (SP_REGNUM, fp - 48);
  else
    write_register (SP_REGNUM, fp);

  /* The PC we just restored may be inside a return trampoline.  If so
     we want to restart the inferior and run it through the trampoline.

     Do this by setting a momentary breakpoint at the location the
     trampoline returns to. 

     Don't skip through the trampoline if we're popping a dummy frame.  */
  target_pc = SKIP_TRAMPOLINE_CODE (npc & ~0x3) & ~0x3;
  if (target_pc && !fsr.regs[IPSW_REGNUM])
    {
      struct symtab_and_line sal;
      struct breakpoint *breakpoint;
      struct cleanup *old_chain;

      /* Set up our breakpoint.   Set it to be silent as the MI code
	 for "return_command" will print the frame we returned to.  */
      sal = find_pc_line (target_pc, 0);
      sal.pc = target_pc;
      breakpoint = set_momentary_breakpoint (sal, NULL, bp_finish);
      breakpoint->silent = 1;

      /* So we can clean things up.  */
      old_chain = make_cleanup (delete_breakpoint, breakpoint);

      /* Start up the inferior.  */
      clear_proceed_status ();
      proceed_to_finish = 1;
      proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 0);

      /* Perform our cleanups.  */
      do_cleanups (old_chain);
    }
  flush_cached_frames ();
}

/*
 * After returning to a dummy on the stack, restore the instruction
 * queue space registers. */

static int
restore_pc_queue (fsr)
     struct frame_saved_regs *fsr;
{
  CORE_ADDR pc = read_pc ();
  CORE_ADDR new_pc = read_memory_integer (fsr->regs[PCOQ_HEAD_REGNUM], 4);
  struct target_waitstatus w;
  int insn_count;

  /* Advance past break instruction in the call dummy. */
  write_register (PCOQ_HEAD_REGNUM, pc + 4);
  write_register (PCOQ_TAIL_REGNUM, pc + 8);

  /*
   * HPUX doesn't let us set the space registers or the space
   * registers of the PC queue through ptrace. Boo, hiss.
   * Conveniently, the call dummy has this sequence of instructions
   * after the break:
   *    mtsp r21, sr0
   *    ble,n 0(sr0, r22)
   *
   * So, load up the registers and single step until we are in the
   * right place.
   */

  write_register (21, read_memory_integer (fsr->regs[PCSQ_HEAD_REGNUM], 4));
  write_register (22, new_pc);

  for (insn_count = 0; insn_count < 3; insn_count++)
    {
      /* FIXME: What if the inferior gets a signal right now?  Want to
	 merge this into wait_for_inferior (as a special kind of
	 watchpoint?  By setting a breakpoint at the end?  Is there
	 any other choice?  Is there *any* way to do this stuff with
	 ptrace() or some equivalent?).  */
      resume (1, 0);
      target_wait (inferior_pid, &w);

      if (w.kind == TARGET_WAITKIND_SIGNALLED)
        {
          stop_signal = w.value.sig;
          terminal_ours_for_output ();
          printf_unfiltered ("\nProgram terminated with signal %s, %s.\n",
			     target_signal_to_name (stop_signal),
			     target_signal_to_string (stop_signal));
          gdb_flush (gdb_stdout);
          return 0;
        }
    }
  target_terminal_ours ();
  target_fetch_registers (-1);
  return 1;
}

CORE_ADDR
hppa_push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     int struct_return;
     CORE_ADDR struct_addr;
{
  /* array of arguments' offsets */
  int *offset = (int *)alloca(nargs * sizeof (int));
  int cum = 0;
  int i, alignment;
  
  for (i = 0; i < nargs; i++)
    {
      cum += TYPE_LENGTH (VALUE_TYPE (args[i]));

    /* value must go at proper alignment. Assume alignment is a
	 power of two.*/
      alignment = hppa_alignof (VALUE_TYPE (args[i]));
      if (cum % alignment)
	cum = (cum + alignment) & -alignment;
      offset[i] = -cum;
    }
  sp += max ((cum + 7) & -8, 16);

  for (i = 0; i < nargs; i++)
    write_memory (sp + offset[i], VALUE_CONTENTS (args[i]),
		  TYPE_LENGTH (VALUE_TYPE (args[i])));

  if (struct_return)
    write_register (28, struct_addr);
  return sp + 32;
}

/*
 * Insert the specified number of args and function address
 * into a call sequence of the above form stored at DUMMYNAME.
 *
 * On the hppa we need to call the stack dummy through $$dyncall.
 * Therefore our version of FIX_CALL_DUMMY takes an extra argument,
 * real_pc, which is the location where gdb should start up the
 * inferior to do the function call.
 */

CORE_ADDR
hppa_fix_call_dummy (dummy, pc, fun, nargs, args, type, gcc_p)
     char *dummy;
     CORE_ADDR pc;
     CORE_ADDR fun;
     int nargs;
     value_ptr *args;
     struct type *type;
     int gcc_p;
{
  CORE_ADDR dyncall_addr;
  struct minimal_symbol *msymbol;
  struct minimal_symbol *trampoline;
  int flags = read_register (FLAGS_REGNUM);
  struct unwind_table_entry *u;

  trampoline = NULL;
  msymbol = lookup_minimal_symbol ("$$dyncall", NULL, NULL);
  if (msymbol == NULL)
    error ("Can't find an address for $$dyncall trampoline");

  dyncall_addr = SYMBOL_VALUE_ADDRESS (msymbol);

  /* FUN could be a procedure label, in which case we have to get
     its real address and the value of its GOT/DP.  */
  if (fun & 0x2)
    {
      /* Get the GOT/DP value for the target function.  It's
	 at *(fun+4).  Note the call dummy is *NOT* allowed to
	 trash %r19 before calling the target function.  */
      write_register (19, read_memory_integer ((fun & ~0x3) + 4, 4));

      /* Now get the real address for the function we are calling, it's
	 at *fun.  */
      fun = (CORE_ADDR) read_memory_integer (fun & ~0x3, 4);
    }
  else
    {

#ifndef GDB_TARGET_IS_PA_ELF
      /* FUN could be either an export stub, or the real address of a
	 function in a shared library.  We must call an import stub
	 rather than the export stub or real function for lazy binding
	 to work correctly.  */
      if (som_solib_get_got_by_pc (fun))
	{
	  struct objfile *objfile;
	  struct minimal_symbol *funsymbol, *stub_symbol;
	  CORE_ADDR newfun = 0;

	  funsymbol = lookup_minimal_symbol_by_pc (fun);
	  if (!funsymbol)
	    error ("Unable to find minimal symbol for target fucntion.\n");

	  /* Search all the object files for an import symbol with the
	     right name. */
	  ALL_OBJFILES (objfile)
	    {
	      stub_symbol = lookup_minimal_symbol (SYMBOL_NAME (funsymbol),
						   NULL, objfile);
	      /* Found a symbol with the right name.  */
	      if (stub_symbol)
		{
		  struct unwind_table_entry *u;
		  /* It must be a shared library trampoline.  */
		  if (SYMBOL_TYPE (stub_symbol) != mst_solib_trampoline)
		    continue;

		  /* It must also be an import stub.  */
		  u = find_unwind_entry (SYMBOL_VALUE (stub_symbol));
		  if (!u || u->stub_type != IMPORT)
		    continue;

		  /* OK.  Looks like the correct import stub.  */
		  newfun = SYMBOL_VALUE (stub_symbol);
		  fun = newfun;
		}
	    }
	  if (newfun == 0)
	    write_register (19, som_solib_get_got_by_pc (fun));
	}
#endif
    }

  /* If we are calling an import stub (eg calling into a dynamic library)
     then have sr4export call the magic __d_plt_call routine which is linked
     in from end.o.  (You can't use _sr4export to call the import stub as
     the value in sp-24 will get fried and you end up returning to the
     wrong location.  You can't call the import stub directly as the code
     to bind the PLT entry to a function can't return to a stack address.)  */
  u = find_unwind_entry (fun);
  if (u && u->stub_type == IMPORT)
    {
      CORE_ADDR new_fun;

      /* Prefer __gcc_plt_call over the HP supplied routine because
	 __gcc_plt_call works for any number of arguments.  */
      trampoline = lookup_minimal_symbol ("__gcc_plt_call", NULL, NULL);
      if (trampoline == NULL)
	trampoline = lookup_minimal_symbol ("__d_plt_call", NULL, NULL);

      if (trampoline == NULL)
	error ("Can't find an address for __d_plt_call or __gcc_plt_call trampoline");

      /* This is where sr4export will jump to.  */
      new_fun = SYMBOL_VALUE_ADDRESS (trampoline);

      if (strcmp (SYMBOL_NAME (trampoline), "__d_plt_call") == 0)
	{
	  /* We have to store the address of the stub in __shlib_funcptr.  */
	  msymbol = lookup_minimal_symbol ("__shlib_funcptr", NULL,
					   (struct objfile *)NULL);
	  if (msymbol == NULL)
	    error ("Can't find an address for __shlib_funcptr");

	  target_write_memory (SYMBOL_VALUE_ADDRESS (msymbol), (char *)&fun, 4);

	  /* We want sr4export to call __d_plt_call, so we claim it is
	     the final target.  Clear trampoline.  */
	  fun = new_fun;
	  trampoline = NULL;
	}
    }

  /* Store upper 21 bits of function address into ldil.  fun will either be
     the final target (most cases) or __d_plt_call when calling into a shared
     library and __gcc_plt_call is not available.  */
  store_unsigned_integer
    (&dummy[FUNC_LDIL_OFFSET],
     INSTRUCTION_SIZE,
     deposit_21 (fun >> 11,
		 extract_unsigned_integer (&dummy[FUNC_LDIL_OFFSET],
					   INSTRUCTION_SIZE)));

  /* Store lower 11 bits of function address into ldo */
  store_unsigned_integer
    (&dummy[FUNC_LDO_OFFSET],
     INSTRUCTION_SIZE,
     deposit_14 (fun & MASK_11,
		 extract_unsigned_integer (&dummy[FUNC_LDO_OFFSET],
					   INSTRUCTION_SIZE)));
#ifdef SR4EXPORT_LDIL_OFFSET

  {
    CORE_ADDR trampoline_addr;

    /* We may still need sr4export's address too.  */

    if (trampoline == NULL)
      {
	msymbol = lookup_minimal_symbol ("_sr4export", NULL, NULL);
	if (msymbol == NULL)
	  error ("Can't find an address for _sr4export trampoline");

	trampoline_addr = SYMBOL_VALUE_ADDRESS (msymbol);
      }
    else
      trampoline_addr = SYMBOL_VALUE_ADDRESS (trampoline);


    /* Store upper 21 bits of trampoline's address into ldil */
    store_unsigned_integer
      (&dummy[SR4EXPORT_LDIL_OFFSET],
       INSTRUCTION_SIZE,
       deposit_21 (trampoline_addr >> 11,
		   extract_unsigned_integer (&dummy[SR4EXPORT_LDIL_OFFSET],
					     INSTRUCTION_SIZE)));

    /* Store lower 11 bits of trampoline's address into ldo */
    store_unsigned_integer
      (&dummy[SR4EXPORT_LDO_OFFSET],
       INSTRUCTION_SIZE,
       deposit_14 (trampoline_addr & MASK_11,
		   extract_unsigned_integer (&dummy[SR4EXPORT_LDO_OFFSET],
					     INSTRUCTION_SIZE)));
  }
#endif

  write_register (22, pc);

  /* If we are in a syscall, then we should call the stack dummy
     directly.  $$dyncall is not needed as the kernel sets up the
     space id registers properly based on the value in %r31.  In
     fact calling $$dyncall will not work because the value in %r22
     will be clobbered on the syscall exit path. 

     Similarly if the current PC is in a shared library.  Note however,
     this scheme won't work if the shared library isn't mapped into
     the same space as the stack.  */
  if (flags & 2)
    return pc;
#ifndef GDB_TARGET_IS_PA_ELF
  else if (som_solib_get_got_by_pc (target_read_pc (inferior_pid)))
    return pc;
#endif
  else
    return dyncall_addr;

}

/* Get the PC from %r31 if currently in a syscall.  Also mask out privilege
   bits.  */

CORE_ADDR
target_read_pc (pid)
     int pid;
{
  int flags = read_register (FLAGS_REGNUM);

  if (flags & 2) {
    return read_register (31) & ~0x3;
  }
  return read_register (PC_REGNUM) & ~0x3;
}

/* Write out the PC.  If currently in a syscall, then also write the new
   PC value into %r31.  */

void
target_write_pc (v, pid)
     CORE_ADDR v;
     int pid;
{
  int flags = read_register (FLAGS_REGNUM);

  /* If in a syscall, then set %r31.  Also make sure to get the 
     privilege bits set correctly.  */
  if (flags & 2)
    write_register (31, (long) (v | 0x3));

  write_register (PC_REGNUM, (long) v);
  write_register (NPC_REGNUM, (long) v + 4);
}

/* return the alignment of a type in bytes. Structures have the maximum
   alignment required by their fields. */

static int
hppa_alignof (type)
     struct type *type;
{
  int max_align, align, i;
  CHECK_TYPEDEF (type);
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_PTR:
    case TYPE_CODE_INT:
    case TYPE_CODE_FLT:
      return TYPE_LENGTH (type);
    case TYPE_CODE_ARRAY:
      return hppa_alignof (TYPE_FIELD_TYPE (type, 0));
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      max_align = 2;
      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  /* Bit fields have no real alignment. */
	  if (!TYPE_FIELD_BITPOS (type, i))
	    {
	      align = hppa_alignof (TYPE_FIELD_TYPE (type, i));
	      max_align = max (max_align, align);
	    }
	}
      return max_align;
    default:
      return 4;
    }
}

/* Print the register regnum, or all registers if regnum is -1 */

void
pa_do_registers_info (regnum, fpregs)
     int regnum;
     int fpregs;
{
  char raw_regs [REGISTER_BYTES];
  int i;
  
  for (i = 0; i < NUM_REGS; i++)
    read_relative_register_raw_bytes (i, raw_regs + REGISTER_BYTE (i));
  if (regnum == -1)
    pa_print_registers (raw_regs, regnum, fpregs);
  else if (regnum < FP0_REGNUM)
    printf_unfiltered ("%s %x\n", reg_names[regnum], *(long *)(raw_regs +
						    REGISTER_BYTE (regnum)));
  else
    pa_print_fp_reg (regnum);
}

static void
pa_print_registers (raw_regs, regnum, fpregs)
     char *raw_regs;
     int regnum;
     int fpregs;
{
  int i,j;
  long val;

  for (i = 0; i < 18; i++)
    {
      for (j = 0; j < 4; j++)
	{
	  val =
	    extract_signed_integer (raw_regs + REGISTER_BYTE (i+(j*18)), 4);
	  printf_unfiltered ("%8.8s: %8x  ", reg_names[i+(j*18)], val);
	}
      printf_unfiltered ("\n");
    }
  
  if (fpregs)
    for (i = 72; i < NUM_REGS; i++)
      pa_print_fp_reg (i);
}

static void
pa_print_fp_reg (i)
     int i;
{
  unsigned char raw_buffer[MAX_REGISTER_RAW_SIZE];
  unsigned char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];

  /* Get 32bits of data.  */
  read_relative_register_raw_bytes (i, raw_buffer);

  /* Put it in the buffer.  No conversions are ever necessary.  */
  memcpy (virtual_buffer, raw_buffer, REGISTER_RAW_SIZE (i));

  fputs_filtered (reg_names[i], gdb_stdout);
  print_spaces_filtered (8 - strlen (reg_names[i]), gdb_stdout);
  fputs_filtered ("(single precision)     ", gdb_stdout);

  val_print (REGISTER_VIRTUAL_TYPE (i), virtual_buffer, 0, gdb_stdout, 0,
	     1, 0, Val_pretty_default);
  printf_filtered ("\n");

  /* If "i" is even, then this register can also be a double-precision
     FP register.  Dump it out as such.  */
  if ((i % 2) == 0)
    {
      /* Get the data in raw format for the 2nd half.  */
      read_relative_register_raw_bytes (i + 1, raw_buffer);

      /* Copy it into the appropriate part of the virtual buffer.  */
      memcpy (virtual_buffer + REGISTER_RAW_SIZE (i), raw_buffer,
	      REGISTER_RAW_SIZE (i));

      /* Dump it as a double.  */
      fputs_filtered (reg_names[i], gdb_stdout);
      print_spaces_filtered (8 - strlen (reg_names[i]), gdb_stdout);
      fputs_filtered ("(double precision)     ", gdb_stdout);

      val_print (builtin_type_double, virtual_buffer, 0, gdb_stdout, 0,
		 1, 0, Val_pretty_default);
      printf_filtered ("\n");
    }
}

/* Return one if PC is in the call path of a trampoline, else return zero.

   Note we return one for *any* call trampoline (long-call, arg-reloc), not
   just shared library trampolines (import, export).  */

int
in_solib_call_trampoline (pc, name)
     CORE_ADDR pc;
     char *name;
{
  struct minimal_symbol *minsym;
  struct unwind_table_entry *u;
  static CORE_ADDR dyncall = 0;
  static CORE_ADDR sr4export = 0;

/* FIXME XXX - dyncall and sr4export must be initialized whenever we get a
   new exec file */

  /* First see if PC is in one of the two C-library trampolines.  */
  if (!dyncall)
    {
      minsym = lookup_minimal_symbol ("$$dyncall", NULL, NULL);
      if (minsym)
	dyncall = SYMBOL_VALUE_ADDRESS (minsym);
      else
	dyncall = -1;
    }

  if (!sr4export)
    {
      minsym = lookup_minimal_symbol ("_sr4export", NULL, NULL);
      if (minsym)
	sr4export = SYMBOL_VALUE_ADDRESS (minsym);
      else
	sr4export = -1;
    }

  if (pc == dyncall || pc == sr4export)
    return 1;

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub, then return now.  */
  if (u->stub_type == 0)
    return 0;

  /* By definition a long-branch stub is a call stub.  */
  if (u->stub_type == LONG_BRANCH)
    return 1;

  /* The call and return path execute the same instructions within
     an IMPORT stub!  So an IMPORT stub is both a call and return
     trampoline.  */
  if (u->stub_type == IMPORT)
    return 1;

  /* Parameter relocation stubs always have a call path and may have a
     return path.  */
  if (u->stub_type == PARAMETER_RELOCATION
      || u->stub_type == EXPORT)
    {
      CORE_ADDR addr;

      /* Search forward from the current PC until we hit a branch
	 or the end of the stub.  */
      for (addr = pc; addr <= u->region_end; addr += 4)
	{
	  unsigned long insn;

	  insn = read_memory_integer (addr, 4);

	  /* Does it look like a bl?  If so then it's the call path, if
	     we find a bv or be first, then we're on the return path.  */
	  if ((insn & 0xfc00e000) == 0xe8000000)
	    return 1;
	  else if ((insn & 0xfc00e001) == 0xe800c000
		   || (insn & 0xfc000000) == 0xe0000000)
	    return 0;
	}

      /* Should never happen.  */
      warning ("Unable to find branch in parameter relocation stub.\n");
      return 0;
    }

  /* Unknown stub type.  For now, just return zero.  */
  return 0;
}

/* Return one if PC is in the return path of a trampoline, else return zero.

   Note we return one for *any* call trampoline (long-call, arg-reloc), not
   just shared library trampolines (import, export).  */

int
in_solib_return_trampoline (pc, name)
     CORE_ADDR pc;
     char *name;
{
  struct unwind_table_entry *u;

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub or it's just a long branch stub, then
     return zero.  */
  if (u->stub_type == 0 || u->stub_type == LONG_BRANCH)
    return 0;

  /* The call and return path execute the same instructions within
     an IMPORT stub!  So an IMPORT stub is both a call and return
     trampoline.  */
  if (u->stub_type == IMPORT)
    return 1;

  /* Parameter relocation stubs always have a call path and may have a
     return path.  */
  if (u->stub_type == PARAMETER_RELOCATION
      || u->stub_type == EXPORT)
    {
      CORE_ADDR addr;

      /* Search forward from the current PC until we hit a branch
	 or the end of the stub.  */
      for (addr = pc; addr <= u->region_end; addr += 4)
	{
	  unsigned long insn;

	  insn = read_memory_integer (addr, 4);

	  /* Does it look like a bl?  If so then it's the call path, if
	     we find a bv or be first, then we're on the return path.  */
	  if ((insn & 0xfc00e000) == 0xe8000000)
	    return 0;
	  else if ((insn & 0xfc00e001) == 0xe800c000
		   || (insn & 0xfc000000) == 0xe0000000)
	    return 1;
	}

      /* Should never happen.  */
      warning ("Unable to find branch in parameter relocation stub.\n");
      return 0;
    }

  /* Unknown stub type.  For now, just return zero.  */
  return 0;

}

/* Figure out if PC is in a trampoline, and if so find out where
   the trampoline will jump to.  If not in a trampoline, return zero.

   Simple code examination probably is not a good idea since the code
   sequences in trampolines can also appear in user code.

   We use unwinds and information from the minimal symbol table to
   determine when we're in a trampoline.  This won't work for ELF
   (yet) since it doesn't create stub unwind entries.  Whether or
   not ELF will create stub unwinds or normal unwinds for linker
   stubs is still being debated.

   This should handle simple calls through dyncall or sr4export,
   long calls, argument relocation stubs, and dyncall/sr4export
   calling an argument relocation stub.  It even handles some stubs
   used in dynamic executables.  */

CORE_ADDR
skip_trampoline_code (pc, name)
     CORE_ADDR pc;
     char *name;
{
  long orig_pc = pc;
  long prev_inst, curr_inst, loc;
  static CORE_ADDR dyncall = 0;
  static CORE_ADDR sr4export = 0;
  struct minimal_symbol *msym;
  struct unwind_table_entry *u;

/* FIXME XXX - dyncall and sr4export must be initialized whenever we get a
   new exec file */

  if (!dyncall)
    {
      msym = lookup_minimal_symbol ("$$dyncall", NULL, NULL);
      if (msym)
	dyncall = SYMBOL_VALUE_ADDRESS (msym);
      else
	dyncall = -1;
    }

  if (!sr4export)
    {
      msym = lookup_minimal_symbol ("_sr4export", NULL, NULL);
      if (msym)
	sr4export = SYMBOL_VALUE_ADDRESS (msym);
      else
	sr4export = -1;
    }

  /* Addresses passed to dyncall may *NOT* be the actual address
     of the function.  So we may have to do something special.  */
  if (pc == dyncall)
    {
      pc = (CORE_ADDR) read_register (22);

      /* If bit 30 (counting from the left) is on, then pc is the address of
	 the PLT entry for this function, not the address of the function
	 itself.  Bit 31 has meaning too, but only for MPE.  */
      if (pc & 0x2)
	pc = (CORE_ADDR) read_memory_integer (pc & ~0x3, 4);
    }
  else if (pc == sr4export)
    pc = (CORE_ADDR) (read_register (22));

  /* Get the unwind descriptor corresponding to PC, return zero
     if no unwind was found.  */
  u = find_unwind_entry (pc);
  if (!u)
    return 0;

  /* If this isn't a linker stub, then return now.  */
  if (u->stub_type == 0)
    return orig_pc == pc ? 0 : pc & ~0x3;

  /* It's a stub.  Search for a branch and figure out where it goes.
     Note we have to handle multi insn branch sequences like ldil;ble.
     Most (all?) other branches can be determined by examining the contents
     of certain registers and the stack.  */
  loc = pc;
  curr_inst = 0;
  prev_inst = 0;
  while (1)
    {
      /* Make sure we haven't walked outside the range of this stub.  */
      if (u != find_unwind_entry (loc))
	{
	  warning ("Unable to find branch in linker stub");
	  return orig_pc == pc ? 0 : pc & ~0x3;
	}

      prev_inst = curr_inst;
      curr_inst = read_memory_integer (loc, 4);

      /* Does it look like a branch external using %r1?  Then it's the
	 branch from the stub to the actual function.  */
      if ((curr_inst & 0xffe0e000) == 0xe0202000)
	{
	  /* Yup.  See if the previous instruction loaded
	     a value into %r1.  If so compute and return the jump address.  */
	  if ((prev_inst & 0xffe00000) == 0x20200000)
	    return (extract_21 (prev_inst) + extract_17 (curr_inst)) & ~0x3;
	  else
	    {
	      warning ("Unable to find ldil X,%%r1 before ble Y(%%sr4,%%r1).");
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

      /* Does it look like a be 0(sr0,%r21)?  That's the branch from an
	 import stub to an export stub.

	 It is impossible to determine the target of the branch via
	 simple examination of instructions and/or data (consider
	 that the address in the plabel may be the address of the
	 bind-on-reference routine in the dynamic loader).

	 So we have try an alternative approach.

	 Get the name of the symbol at our current location; it should
	 be a stub symbol with the same name as the symbol in the
	 shared library.

	 Then lookup a minimal symbol with the same name; we should
	 get the minimal symbol for the target routine in the shared
	 library as those take precedence of import/export stubs.  */
      if (curr_inst == 0xe2a00000)
	{
	  struct minimal_symbol *stubsym, *libsym;

	  stubsym = lookup_minimal_symbol_by_pc (loc);
	  if (stubsym == NULL)
	    {
	      warning ("Unable to find symbol for 0x%x", loc);
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }

	  libsym = lookup_minimal_symbol (SYMBOL_NAME (stubsym), NULL, NULL);
	  if (libsym == NULL)
	    {
	      warning ("Unable to find library symbol for %s\n",
		       SYMBOL_NAME (stubsym));
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }

	  return SYMBOL_VALUE (libsym);
	}

      /* Does it look like bl X,%rp or bl X,%r0?  Another way to do a
	 branch from the stub to the actual function.  */
      else if ((curr_inst & 0xffe0e000) == 0xe8400000
	       || (curr_inst & 0xffe0e000) == 0xe8000000)
	return (loc + extract_17 (curr_inst) + 8) & ~0x3;

      /* Does it look like bv (rp)?   Note this depends on the
	 current stack pointer being the same as the stack
	 pointer in the stub itself!  This is a branch on from the
	 stub back to the original caller.  */
      else if ((curr_inst & 0xffe0e000) == 0xe840c000)
	{
	  /* Yup.  See if the previous instruction loaded
	     rp from sp - 8.  */
	  if (prev_inst == 0x4bc23ff1)
	    return (read_memory_integer
		    (read_register (SP_REGNUM) - 8, 4)) & ~0x3;
	  else
	    {
	      warning ("Unable to find restore of %%rp before bv (%%rp).");
	      return orig_pc == pc ? 0 : pc & ~0x3;
	    }
	}

      /* What about be,n 0(sr0,%rp)?  It's just another way we return to
	 the original caller from the stub.  Used in dynamic executables.  */
      else if (curr_inst == 0xe0400002)
	{
	  /* The value we jump to is sitting in sp - 24.  But that's
	     loaded several instructions before the be instruction.
	     I guess we could check for the previous instruction being
	     mtsp %r1,%sr0 if we want to do sanity checking.  */
	  return (read_memory_integer 
		  (read_register (SP_REGNUM) - 24, 4)) & ~0x3;
	}

      /* Haven't found the branch yet, but we're still in the stub.
	 Keep looking.  */
      loc += 4;
    }
}

/* For the given instruction (INST), return any adjustment it makes
   to the stack pointer or zero for no adjustment. 

   This only handles instructions commonly found in prologues.  */

static int
prologue_inst_adjust_sp (inst)
     unsigned long inst;
{
  /* This must persist across calls.  */
  static int save_high21;

  /* The most common way to perform a stack adjustment ldo X(sp),sp */
  if ((inst & 0xffffc000) == 0x37de0000)
    return extract_14 (inst);

  /* stwm X,D(sp) */
  if ((inst & 0xffe00000) == 0x6fc00000)
    return extract_14 (inst);

  /* addil high21,%r1; ldo low11,(%r1),%r30)
     save high bits in save_high21 for later use.  */
  if ((inst & 0xffe00000) == 0x28200000)
    {
      save_high21 = extract_21 (inst);
      return 0;
    }

  if ((inst & 0xffff0000) == 0x343e0000)
    return save_high21 + extract_14 (inst);

  /* fstws as used by the HP compilers.  */
  if ((inst & 0xffffffe0) == 0x2fd01220)
    return extract_5_load (inst);

  /* No adjustment.  */
  return 0;
}

/* Return nonzero if INST is a branch of some kind, else return zero.  */

static int
is_branch (inst)
     unsigned long inst;
{
  switch (inst >> 26)
    {
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x28:
    case 0x29:
    case 0x2a:
    case 0x2b:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x38:
    case 0x39:
    case 0x3a:
      return 1;

    default:
      return 0;
    }
}

/* Return the register number for a GR which is saved by INST or
   zero it INST does not save a GR.  */

static int
inst_saves_gr (inst)
     unsigned long inst;
{
  /* Does it look like a stw?  */
  if ((inst >> 26) == 0x1a)
    return extract_5R_store (inst);

  /* Does it look like a stwm?  GCC & HPC may use this in prologues. */
  if ((inst >> 26) == 0x1b)
    return extract_5R_store (inst);

  /* Does it look like sth or stb?  HPC versions 9.0 and later use these
     too.  */
  if ((inst >> 26) == 0x19 || (inst >> 26) == 0x18)
    return extract_5R_store (inst);
      
  return 0;
}

/* Return the register number for a FR which is saved by INST or
   zero it INST does not save a FR.

   Note we only care about full 64bit register stores (that's the only
   kind of stores the prologue will use).

   FIXME: What about argument stores with the HP compiler in ANSI mode? */

static int
inst_saves_fr (inst)
     unsigned long inst;
{
  if ((inst & 0xfc00dfc0) == 0x2c001200)
    return extract_5r_store (inst);
  return 0;
}

/* Advance PC across any function entry prologue instructions
   to reach some "real" code. 

   Use information in the unwind table to determine what exactly should
   be in the prologue.  */

CORE_ADDR
skip_prologue (pc)
     CORE_ADDR pc;
{
  char buf[4];
  CORE_ADDR orig_pc = pc;
  unsigned long inst, stack_remaining, save_gr, save_fr, save_rp, save_sp;
  unsigned long args_stored, status, i, restart_gr, restart_fr;
  struct unwind_table_entry *u;

  restart_gr = 0;
  restart_fr = 0;

restart:
  u = find_unwind_entry (pc);
  if (!u)
    return pc;

  /* If we are not at the beginning of a function, then return now.  */
  if ((pc & ~0x3) != u->region_start)
    return pc;

  /* This is how much of a frame adjustment we need to account for.  */
  stack_remaining = u->Total_frame_size << 3;

  /* Magic register saves we want to know about.  */
  save_rp = u->Save_RP;
  save_sp = u->Save_SP;

  /* An indication that args may be stored into the stack.  Unfortunately
     the HPUX compilers tend to set this in cases where no args were
     stored too!.  */
  args_stored = 1;

  /* Turn the Entry_GR field into a bitmask.  */
  save_gr = 0;
  for (i = 3; i < u->Entry_GR + 3; i++)
    {
      /* Frame pointer gets saved into a special location.  */
      if (u->Save_SP && i == FP_REGNUM)
	continue;

      save_gr |= (1 << i);
    }
  save_gr &= ~restart_gr;

  /* Turn the Entry_FR field into a bitmask too.  */
  save_fr = 0;
  for (i = 12; i < u->Entry_FR + 12; i++)
    save_fr |= (1 << i);
  save_fr &= ~restart_fr;

  /* Loop until we find everything of interest or hit a branch.

     For unoptimized GCC code and for any HP CC code this will never ever
     examine any user instructions.

     For optimzied GCC code we're faced with problems.  GCC will schedule
     its prologue and make prologue instructions available for delay slot
     filling.  The end result is user code gets mixed in with the prologue
     and a prologue instruction may be in the delay slot of the first branch
     or call.

     Some unexpected things are expected with debugging optimized code, so
     we allow this routine to walk past user instructions in optimized
     GCC code.  */
  while (save_gr || save_fr || save_rp || save_sp || stack_remaining > 0
	 || args_stored)
    {
      unsigned int reg_num;
      unsigned long old_stack_remaining, old_save_gr, old_save_fr;
      unsigned long old_save_rp, old_save_sp, next_inst;

      /* Save copies of all the triggers so we can compare them later
	 (only for HPC).  */
      old_save_gr = save_gr;
      old_save_fr = save_fr;
      old_save_rp = save_rp;
      old_save_sp = save_sp;
      old_stack_remaining = stack_remaining;

      status = target_read_memory (pc, buf, 4);
      inst = extract_unsigned_integer (buf, 4);
       
      /* Yow! */
      if (status != 0)
	return pc;

      /* Note the interesting effects of this instruction.  */
      stack_remaining -= prologue_inst_adjust_sp (inst);

      /* There is only one instruction used for saving RP into the stack.  */
      if (inst == 0x6bc23fd9)
	save_rp = 0;

      /* This is the only way we save SP into the stack.  At this time
	 the HP compilers never bother to save SP into the stack.  */
      if ((inst & 0xffffc000) == 0x6fc10000)
	save_sp = 0;

      /* Account for general and floating-point register saves.  */
      reg_num = inst_saves_gr (inst);
      save_gr &= ~(1 << reg_num);

      /* Ugh.  Also account for argument stores into the stack.
	 Unfortunately args_stored only tells us that some arguments
	 where stored into the stack.  Not how many or what kind!

	 This is a kludge as on the HP compiler sets this bit and it
	 never does prologue scheduling.  So once we see one, skip past
	 all of them.   We have similar code for the fp arg stores below.

	 FIXME.  Can still die if we have a mix of GR and FR argument
	 stores!  */
      if (reg_num >= 23 && reg_num <= 26)
	{
	  while (reg_num >= 23 && reg_num <= 26)
	    {
	      pc += 4;
	      status = target_read_memory (pc, buf, 4);
	      inst = extract_unsigned_integer (buf, 4);
	      if (status != 0)
		return pc;
	      reg_num = inst_saves_gr (inst);
	    }
	  args_stored = 0;
	  continue;
	}

      reg_num = inst_saves_fr (inst);
      save_fr &= ~(1 << reg_num);

      status = target_read_memory (pc + 4, buf, 4);
      next_inst = extract_unsigned_integer (buf, 4);
       
      /* Yow! */
      if (status != 0)
	return pc;

      /* We've got to be read to handle the ldo before the fp register
	 save.  */
      if ((inst & 0xfc000000) == 0x34000000
	  && inst_saves_fr (next_inst) >= 4
	  && inst_saves_fr (next_inst) <= 7)
	{
	  /* So we drop into the code below in a reasonable state.  */
	  reg_num = inst_saves_fr (next_inst);
	  pc -= 4;
	}

      /* Ugh.  Also account for argument stores into the stack.
	 This is a kludge as on the HP compiler sets this bit and it
	 never does prologue scheduling.  So once we see one, skip past
	 all of them.  */
      if (reg_num >= 4 && reg_num <= 7)
	{
	  while (reg_num >= 4 && reg_num <= 7)
	    {
	      pc += 8;
	      status = target_read_memory (pc, buf, 4);
	      inst = extract_unsigned_integer (buf, 4);
	      if (status != 0)
		return pc;
	      if ((inst & 0xfc000000) != 0x34000000)
		break;
	      status = target_read_memory (pc + 4, buf, 4);
	      next_inst = extract_unsigned_integer (buf, 4);
	      if (status != 0)
		return pc;
	      reg_num = inst_saves_fr (next_inst);
	    }
	  args_stored = 0;
	  continue;
	}

      /* Quit if we hit any kind of branch.  This can happen if a prologue
	 instruction is in the delay slot of the first call/branch.  */
      if (is_branch (inst))
	break;

      /* What a crock.  The HP compilers set args_stored even if no
	 arguments were stored into the stack (boo hiss).  This could
	 cause this code to then skip a bunch of user insns (up to the
	 first branch).

	 To combat this we try to identify when args_stored was bogusly
	 set and clear it.   We only do this when args_stored is nonzero,
	 all other resources are accounted for, and nothing changed on
	 this pass.  */
      if (args_stored
	  && ! (save_gr || save_fr || save_rp || save_sp || stack_remaining > 0)
	  && old_save_gr == save_gr && old_save_fr == save_fr
	  && old_save_rp == save_rp && old_save_sp == save_sp
	  && old_stack_remaining == stack_remaining)
	break;
      
      /* Bump the PC.  */
      pc += 4;
    }

  /* We've got a tenative location for the end of the prologue.  However
     because of limitations in the unwind descriptor mechanism we may
     have went too far into user code looking for the save of a register
     that does not exist.  So, if there registers we expected to be saved
     but never were, mask them out and restart.

     This should only happen in optimized code, and should be very rare.  */
  if (save_gr || save_fr
      && ! (restart_fr || restart_gr))
    {
      pc = orig_pc;
      restart_gr = save_gr;
      restart_fr = save_fr;
      goto restart;
    }

  return pc;
}

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

void
hppa_frame_find_saved_regs (frame_info, frame_saved_regs)
     struct frame_info *frame_info;
     struct frame_saved_regs *frame_saved_regs;
{
  CORE_ADDR pc;
  struct unwind_table_entry *u;
  unsigned long inst, stack_remaining, save_gr, save_fr, save_rp, save_sp;
  int status, i, reg;
  char buf[4];
  int fp_loc = -1;

  /* Zero out everything.  */
  memset (frame_saved_regs, '\0', sizeof (struct frame_saved_regs));

  /* Call dummy frames always look the same, so there's no need to
     examine the dummy code to determine locations of saved registers;
     instead, let find_dummy_frame_regs fill in the correct offsets
     for the saved registers.  */
  if ((frame_info->pc >= frame_info->frame
       && frame_info->pc <= (frame_info->frame + CALL_DUMMY_LENGTH
			     + 32 * 4 +	 (NUM_REGS - FP0_REGNUM) * 8
			     + 6 * 4)))	
    find_dummy_frame_regs (frame_info, frame_saved_regs);

  /* Interrupt handlers are special too.  They lay out the register
     state in the exact same order as the register numbers in GDB.  */
  if (pc_in_interrupt_handler (frame_info->pc))
    {
      for (i = 0; i < NUM_REGS; i++)
	{
	  /* SP is a little special.  */
	  if (i == SP_REGNUM)
	    frame_saved_regs->regs[SP_REGNUM]
	      = read_memory_integer (frame_info->frame + SP_REGNUM * 4, 4);
	  else
	    frame_saved_regs->regs[i] = frame_info->frame + i * 4;
	}
      return;
    }

#ifdef FRAME_FIND_SAVED_REGS_IN_SIGTRAMP
  /* Handle signal handler callers.  */
  if (frame_info->signal_handler_caller)
    {
      FRAME_FIND_SAVED_REGS_IN_SIGTRAMP (frame_info, frame_saved_regs);
      return;
    }
#endif

  /* Get the starting address of the function referred to by the PC
     saved in frame.  */
  pc = get_pc_function_start (frame_info->pc);

  /* Yow! */
  u = find_unwind_entry (pc);
  if (!u)
    return;

  /* This is how much of a frame adjustment we need to account for.  */
  stack_remaining = u->Total_frame_size << 3;

  /* Magic register saves we want to know about.  */
  save_rp = u->Save_RP;
  save_sp = u->Save_SP;

  /* Turn the Entry_GR field into a bitmask.  */
  save_gr = 0;
  for (i = 3; i < u->Entry_GR + 3; i++)
    {
      /* Frame pointer gets saved into a special location.  */
      if (u->Save_SP && i == FP_REGNUM)
	continue;

      save_gr |= (1 << i);
    }

  /* Turn the Entry_FR field into a bitmask too.  */
  save_fr = 0;
  for (i = 12; i < u->Entry_FR + 12; i++)
    save_fr |= (1 << i);

  /* The frame always represents the value of %sp at entry to the
     current function (and is thus equivalent to the "saved" stack
     pointer.  */
  frame_saved_regs->regs[SP_REGNUM] = frame_info->frame;

  /* Loop until we find everything of interest or hit a branch.

     For unoptimized GCC code and for any HP CC code this will never ever
     examine any user instructions.

     For optimzied GCC code we're faced with problems.  GCC will schedule
     its prologue and make prologue instructions available for delay slot
     filling.  The end result is user code gets mixed in with the prologue
     and a prologue instruction may be in the delay slot of the first branch
     or call.

     Some unexpected things are expected with debugging optimized code, so
     we allow this routine to walk past user instructions in optimized
     GCC code.  */
  while (save_gr || save_fr || save_rp || save_sp || stack_remaining > 0)
    {
      status = target_read_memory (pc, buf, 4);
      inst = extract_unsigned_integer (buf, 4);

      /* Yow! */
      if (status != 0)
	return;

      /* Note the interesting effects of this instruction.  */
      stack_remaining -= prologue_inst_adjust_sp (inst);

      /* There is only one instruction used for saving RP into the stack.  */
      if (inst == 0x6bc23fd9)
	{
	  save_rp = 0;
	  frame_saved_regs->regs[RP_REGNUM] = frame_info->frame - 20;
	}

      /* Just note that we found the save of SP into the stack.  The
	 value for frame_saved_regs was computed above.  */
      if ((inst & 0xffffc000) == 0x6fc10000)
	save_sp = 0;

      /* Account for general and floating-point register saves.  */
      reg = inst_saves_gr (inst);
      if (reg >= 3 && reg <= 18
	  && (!u->Save_SP || reg != FP_REGNUM))
	{
	  save_gr &= ~(1 << reg);

	  /* stwm with a positive displacement is a *post modify*.  */
	  if ((inst >> 26) == 0x1b
	      && extract_14 (inst) >= 0)
	    frame_saved_regs->regs[reg] = frame_info->frame;
	  else
	    {
	      /* Handle code with and without frame pointers.  */
	      if (u->Save_SP)
		frame_saved_regs->regs[reg]
		  = frame_info->frame + extract_14 (inst);
	      else
		frame_saved_regs->regs[reg]
		  = frame_info->frame + (u->Total_frame_size << 3)
		    + extract_14 (inst);
	    }
	}


      /* GCC handles callee saved FP regs a little differently.  

	 It emits an instruction to put the value of the start of
	 the FP store area into %r1.  It then uses fstds,ma with
	 a basereg of %r1 for the stores.

	 HP CC emits them at the current stack pointer modifying
	 the stack pointer as it stores each register.  */

      /* ldo X(%r3),%r1 or ldo X(%r30),%r1.  */
      if ((inst & 0xffffc000) == 0x34610000
	  || (inst & 0xffffc000) == 0x37c10000)
	fp_loc = extract_14 (inst);
	
      reg = inst_saves_fr (inst);
      if (reg >= 12 && reg <= 21)
	{
	  /* Note +4 braindamage below is necessary because the FP status
	     registers are internally 8 registers rather than the expected
	     4 registers.  */
	  save_fr &= ~(1 << reg);
	  if (fp_loc == -1)
	    {
	      /* 1st HP CC FP register store.  After this instruction
		 we've set enough state that the GCC and HPCC code are
		 both handled in the same manner.  */
	      frame_saved_regs->regs[reg + FP4_REGNUM + 4] = frame_info->frame;
	      fp_loc = 8;
	    }
	  else
	    {
	      frame_saved_regs->regs[reg + FP0_REGNUM + 4]
		= frame_info->frame + fp_loc;
	      fp_loc += 8;
	    }
	}

      /* Quit if we hit any kind of branch.  This can happen if a prologue
	 instruction is in the delay slot of the first call/branch.  */
      if (is_branch (inst))
	break;

      /* Bump the PC.  */
      pc += 4;
    }
}

#ifdef MAINTENANCE_CMDS

static void
unwind_command (exp, from_tty)
     char *exp;
     int from_tty;
{
  CORE_ADDR address;
  struct unwind_table_entry *u;

  /* If we have an expression, evaluate it and use it as the address.  */

  if (exp != 0 && *exp != 0)
    address = parse_and_eval_address (exp);
  else
    return;

  u = find_unwind_entry (address);

  if (!u)
    {
      printf_unfiltered ("Can't find unwind table entry for %s\n", exp);
      return;
    }

  printf_unfiltered ("unwind_table_entry (0x%x):\n", u);

  printf_unfiltered ("\tregion_start = ");
  print_address (u->region_start, gdb_stdout);

  printf_unfiltered ("\n\tregion_end = ");
  print_address (u->region_end, gdb_stdout);

#ifdef __STDC__
#define pif(FLD) if (u->FLD) printf_unfiltered (" "#FLD);
#else
#define pif(FLD) if (u->FLD) printf_unfiltered (" FLD");
#endif

  printf_unfiltered ("\n\tflags =");
  pif (Cannot_unwind);
  pif (Millicode);
  pif (Millicode_save_sr0);
  pif (Entry_SR);
  pif (Args_stored);
  pif (Variable_Frame);
  pif (Separate_Package_Body);
  pif (Frame_Extension_Millicode);
  pif (Stack_Overflow_Check);
  pif (Two_Instruction_SP_Increment);
  pif (Ada_Region);
  pif (Save_SP);
  pif (Save_RP);
  pif (Save_MRP_in_frame);
  pif (extn_ptr_defined);
  pif (Cleanup_defined);
  pif (MPE_XL_interrupt_marker);
  pif (HP_UX_interrupt_marker);
  pif (Large_frame);

  putchar_unfiltered ('\n');

#ifdef __STDC__
#define pin(FLD) printf_unfiltered ("\t"#FLD" = 0x%x\n", u->FLD);
#else
#define pin(FLD) printf_unfiltered ("\tFLD = 0x%x\n", u->FLD);
#endif

  pin (Region_description);
  pin (Entry_FR);
  pin (Entry_GR);
  pin (Total_frame_size);
}
#endif /* MAINTENANCE_CMDS */

void
_initialize_hppa_tdep ()
{
  tm_print_insn = print_insn_hppa;

#ifdef MAINTENANCE_CMDS
  add_cmd ("unwind", class_maintenance, unwind_command,
	   "Print unwind table entry at given address.",
	   &maintenanceprintlist);
#endif /* MAINTENANCE_CMDS */
}
