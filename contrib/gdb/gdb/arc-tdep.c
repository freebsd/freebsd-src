/* ARC target-dependent stuff.
   Copyright (C) 1995, 1997 Free Software Foundation, Inc.

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
#include "gdbcore.h"
#include "target.h"
#include "floatformat.h"
#include "symtab.h"
#include "gdbcmd.h"

/* Current CPU, set with the "set cpu" command.  */
static int arc_bfd_mach_type;
char *arc_cpu_type;
char *tmp_arc_cpu_type;

/* Table of cpu names.  */
struct {
  char *name;
  int value;
} arc_cpu_type_table[] = {
  { "base", bfd_mach_arc_base },
  { NULL, 0 }
};

/* Used by simulator.  */
int display_pipeline_p;
int cpu_timer;
/* This one must have the same type as used in the emulator.
   It's currently an enum so this should be ok for now.  */
int debug_pipeline_p;

#define ARC_CALL_SAVED_REG(r) ((r) >= 16 && (r) < 24)

#define OPMASK	0xf8000000

/* Instruction field accessor macros.
   See the Programmer's Reference Manual.  */
#define X_OP(i) (((i) >> 27) & 0x1f)
#define X_A(i) (((i) >> 21) & 0x3f)
#define X_B(i) (((i) >> 15) & 0x3f)
#define X_C(i) (((i) >> 9) & 0x3f)
#define X_D(i) ((((i) & 0x1ff) ^ 0x100) - 0x100)
#define X_L(i) (((((i) >> 5) & 0x3ffffc) ^ 0x200000) - 0x200000)
#define X_N(i) (((i) >> 5) & 3)
#define X_Q(i) ((i) & 0x1f)

/* Return non-zero if X is a short immediate data indicator.  */
#define SHIMM_P(x) ((x) == 61 || (x) == 63)

/* Return non-zero if X is a "long" (32 bit) immediate data indicator.  */
#define LIMM_P(x) ((x) == 62)

/* Build a simple instruction.  */
#define BUILD_INSN(op, a, b, c, d) \
  ((((op) & 31) << 27) \
   | (((a) & 63) << 21) \
   | (((b) & 63) << 15) \
   | (((c) & 63) << 9) \
   | ((d) & 511))

/* Codestream stuff.  */
static void codestream_read PARAMS ((unsigned int *, int));
static void codestream_seek PARAMS ((CORE_ADDR));
static unsigned int codestream_fill PARAMS ((int));

#define CODESTREAM_BUFSIZ 16 
static CORE_ADDR codestream_next_addr;
static CORE_ADDR codestream_addr;
static unsigned int codestream_buf[CODESTREAM_BUFSIZ];
static int codestream_off;
static int codestream_cnt;

#define codestream_tell() \
  (codestream_addr + codestream_off * sizeof (codestream_buf[0]))
#define codestream_peek() \
  (codestream_cnt == 0 \
   ? codestream_fill (1) \
   : codestream_buf[codestream_off])
#define codestream_get() \
  (codestream_cnt-- == 0 \
   ? codestream_fill (0) \
   : codestream_buf[codestream_off++])

static unsigned int 
codestream_fill (peek_flag)
    int peek_flag;
{
  codestream_addr = codestream_next_addr;
  codestream_next_addr += CODESTREAM_BUFSIZ * sizeof (codestream_buf[0]);
  codestream_off = 0;
  codestream_cnt = CODESTREAM_BUFSIZ;
  read_memory (codestream_addr, (char *) codestream_buf,
	       CODESTREAM_BUFSIZ * sizeof (codestream_buf[0]));
  /* FIXME: check return code?  */

  /* Handle byte order differences.  */
  if (HOST_BYTE_ORDER != TARGET_BYTE_ORDER)
    {
      register unsigned int i, j, n = sizeof (codestream_buf[0]);
      register char tmp, *p;
      for (i = 0, p = (char *) codestream_buf; i < CODESTREAM_BUFSIZ;
	   ++i, p += n)
	for (j = 0; j < n / 2; ++j)
	  tmp = p[j], p[j] = p[n - 1 - j], p[n - 1 - j] = tmp;
    }
  
  if (peek_flag)
    return codestream_peek ();
  else
    return codestream_get ();
}

static void
codestream_seek (place)
    CORE_ADDR place;
{
  codestream_next_addr = place / CODESTREAM_BUFSIZ;
  codestream_next_addr *= CODESTREAM_BUFSIZ;
  codestream_cnt = 0;
  codestream_fill (1);
  while (codestream_tell () != place)
    codestream_get ();
}

/* This function is currently unused but leave in for now.  */

static void
codestream_read (buf, count)
     unsigned int *buf;
     int count;
{
  unsigned int *p;
  int i;
  p = buf;
  for (i = 0; i < count; i++)
    *p++ = codestream_get ();
}

/* Set up prologue scanning and return the first insn.  */

static unsigned int
setup_prologue_scan (pc)
     CORE_ADDR pc;
{
  unsigned int insn;

  codestream_seek (pc);
  insn = codestream_get ();

  return insn;
}

/*
 * Find & return amount a local space allocated, and advance codestream to
 * first register push (if any).
 * If entry sequence doesn't make sense, return -1, and leave 
 * codestream pointer random.
 */

static long
arc_get_frame_setup (pc)
     CORE_ADDR pc;
{
  unsigned int insn;
  /* Size of frame or -1 if unrecognizable prologue.  */
  int frame_size = -1;
  /* An initial "sub sp,sp,N" may or may not be for a stdarg fn.  */
  int maybe_stdarg_decr = -1;

  insn = setup_prologue_scan (pc);

  /* The authority for what appears here is the home-grown ABI.
     The most recent version is 1.2.  */

  /* First insn may be "sub sp,sp,N" if stdarg fn.  */
  if ((insn & BUILD_INSN (-1, -1, -1, -1, 0))
      == BUILD_INSN (10, SP_REGNUM, SP_REGNUM, SHIMM_REGNUM, 0))
    {
      maybe_stdarg_decr = X_D (insn);
      insn = codestream_get ();
    }

  if ((insn & BUILD_INSN (-1, 0, -1, -1, -1))	/* st blink,[sp,4] */
      == BUILD_INSN (2, 0, SP_REGNUM, BLINK_REGNUM, 4))
    {
      insn = codestream_get ();
      /* Frame may not be necessary, even though blink is saved.
	 At least this is something we recognize.  */
      frame_size = 0;
    }

  if ((insn & BUILD_INSN (-1, 0, -1, -1, -1))		/* st fp,[sp] */
      == BUILD_INSN (2, 0, SP_REGNUM, FP_REGNUM, 0))
    {	
      insn = codestream_get ();
      if ((insn & BUILD_INSN (-1, -1, -1, -1, 0))
	       != BUILD_INSN (12, FP_REGNUM, SP_REGNUM, SP_REGNUM, 0))
	return -1;

      /* Check for stack adjustment sub sp,sp,N.  */
      insn = codestream_peek ();
      if ((insn & BUILD_INSN (-1, -1, -1, 0, 0))
	  == BUILD_INSN (10, SP_REGNUM, SP_REGNUM, 0, 0))
	{
	  if (LIMM_P (X_C (insn)))
	    frame_size = codestream_get ();
	  else if (SHIMM_P (X_C (insn)))
	    frame_size = X_D (insn);
	  else
	    return -1;
	  if (frame_size < 0)
	    return -1;

          codestream_get ();

	  /* This sequence is used to get the address of the return
	     buffer for a function that returns a structure.  */
	  insn = codestream_peek ();
	  if (insn & OPMASK == 0x60000000)
	    codestream_get ();
	}
      /* Frameless fn.  */
      else
	{
	  frame_size = 0;
	}
    }

  /* If we found a "sub sp,sp,N" and nothing else, it may or may not be a
     stdarg fn.  The stdarg decrement is not treated as part of the frame size,
     so we have a dilemma: what do we return?  For now, if we get a
     "sub sp,sp,N" and nothing else assume this isn't a stdarg fn.  One way
     to fix this completely would be to add a bit to the function descriptor
     that says the function is a stdarg function.  */

  if (frame_size < 0 && maybe_stdarg_decr > 0)
    return maybe_stdarg_decr;
  return frame_size;
}

/* Given a pc value, skip it forward past the function prologue by
   disassembling instructions that appear to be a prologue.

   If FRAMELESS_P is set, we are only testing to see if the function
   is frameless.  If it is a frameless function, return PC unchanged.
   This allows a quicker answer.  */

CORE_ADDR
skip_prologue (pc, frameless_p)
     CORE_ADDR pc;
     int frameless_p;
{
  unsigned int insn;
  int i, frame_size;

  if ((frame_size = arc_get_frame_setup (pc)) < 0)
    return (pc);

  if (frameless_p)
    return frame_size == 0 ? pc : codestream_tell ();

  /* Skip over register saves.  */
  for (i = 0; i < 8; i++)
    {
      insn = codestream_peek ();
      if ((insn & BUILD_INSN (-1, 0, -1, 0, 0))
	  != BUILD_INSN (2, 0, SP_REGNUM, 0, 0))
	break; /* not st insn */
      if (! ARC_CALL_SAVED_REG (X_C (insn)))
	break;
      codestream_get ();
    }

  return codestream_tell ();
}

/* Return the return address for a frame.
   This is used to implement FRAME_SAVED_PC.
   This is taken from frameless_look_for_prologue.  */

CORE_ADDR
arc_frame_saved_pc (frame)
     struct frame_info *frame;
{
  CORE_ADDR func_start;
  unsigned int insn;

  func_start = get_pc_function_start (frame->pc) + FUNCTION_START_OFFSET;
  if (func_start == 0)
    {
      /* Best guess.  */
      return ARC_PC_TO_REAL_ADDRESS (read_memory_integer (FRAME_FP (frame) + 4, 4));
    }

  /* The authority for what appears here is the home-grown ABI.
     The most recent version is 1.2.  */

  insn = setup_prologue_scan (func_start);

  /* First insn may be "sub sp,sp,N" if stdarg fn.  */
  if ((insn & BUILD_INSN (-1, -1, -1, -1, 0))
      == BUILD_INSN (10, SP_REGNUM, SP_REGNUM, SHIMM_REGNUM, 0))
    insn = codestream_get ();

  /* If the next insn is "st blink,[sp,4]" we can get blink from there.
     Otherwise this is a leaf function and we can use blink.  Note that
     this still allows for the case where a leaf function saves/clobbers/
     restores blink.  */

  if ((insn & BUILD_INSN (-1, 0, -1, -1, -1))	/* st blink,[sp,4] */
      != BUILD_INSN (2, 0, SP_REGNUM, BLINK_REGNUM, 4))
    return ARC_PC_TO_REAL_ADDRESS (read_register (BLINK_REGNUM));
  else
    return ARC_PC_TO_REAL_ADDRESS (read_memory_integer (FRAME_FP (frame) + 4, 4));
}

/*
 * Parse the first few instructions of the function to see
 * what registers were stored.
 *
 * The startup sequence can be at the start of the function.
 * 'st blink,[sp+4], st fp,[sp], mov fp,sp' 
 *
 * Local space is allocated just below by sub sp,sp,nnn.
 * Next, the registers used by this function are stored (as offsets from sp).
 */

void
frame_find_saved_regs (fip, fsrp)
     struct frame_info *fip;
     struct frame_saved_regs *fsrp;
{
  long locals;
  unsigned int insn;
  CORE_ADDR dummy_bottom;
  CORE_ADDR adr;
  int i, regnum, offset;

  memset (fsrp, 0, sizeof *fsrp);

  /* If frame is the end of a dummy, compute where the beginning would be.  */
  dummy_bottom = fip->frame - 4 - REGISTER_BYTES - CALL_DUMMY_LENGTH;

  /* Check if the PC is in the stack, in a dummy frame.  */
  if (dummy_bottom <= fip->pc && fip->pc <= fip->frame) 
    {
      /* all regs were saved by push_call_dummy () */
      adr = fip->frame;
      for (i = 0; i < NUM_REGS; i++) 
	{
	  adr -= REGISTER_RAW_SIZE (i);
	  fsrp->regs[i] = adr;
	}
      return;
    }

  locals = arc_get_frame_setup (get_pc_function_start (fip->pc));

  if (locals >= 0) 
    {
      /* Set `adr' to the value of `sp'.  */
      adr = fip->frame - locals;
      for (i = 0; i < 8; i++)
	{
	  insn = codestream_get ();
	  if ((insn & BUILD_INSN (-1, 0, -1, 0, 0))
	       != BUILD_INSN (2, 0, SP_REGNUM, 0, 0))
	    break;
          regnum = X_C (insn);
	  offset = X_D (insn);
	  fsrp->regs[regnum] = adr + offset;
	}
    }

  fsrp->regs[PC_REGNUM] = fip->frame + 4;
  fsrp->regs[FP_REGNUM] = fip->frame;
}

void
push_dummy_frame ()
{
  CORE_ADDR sp = read_register (SP_REGNUM);
  int regnum;
  char regbuf[MAX_REGISTER_RAW_SIZE];

  read_register_gen (PC_REGNUM, regbuf);
  write_memory (sp+4, regbuf, REGISTER_SIZE);
  read_register_gen (FP_REGNUM, regbuf);
  write_memory (sp, regbuf, REGISTER_SIZE);
  write_register (FP_REGNUM, sp);
  for (regnum = 0; regnum < NUM_REGS; regnum++)
    {
      read_register_gen (regnum, regbuf);
      sp = push_bytes (sp, regbuf, REGISTER_RAW_SIZE (regnum));
    }
  sp += (2*REGISTER_SIZE);
  write_register (SP_REGNUM, sp);
}

void
pop_frame ()
{
  struct frame_info *frame = get_current_frame ();
  CORE_ADDR fp;
  int regnum;
  struct frame_saved_regs fsr;
  char regbuf[MAX_REGISTER_RAW_SIZE];
  
  fp = FRAME_FP (frame);
  get_frame_saved_regs (frame, &fsr);
  for (regnum = 0; regnum < NUM_REGS; regnum++) 
    {
      CORE_ADDR adr;
      adr = fsr.regs[regnum];
      if (adr)
	{
	  read_memory (adr, regbuf, REGISTER_RAW_SIZE (regnum));
	  write_register_bytes (REGISTER_BYTE (regnum), regbuf,
				REGISTER_RAW_SIZE (regnum));
	}
    }
  write_register (FP_REGNUM, read_memory_integer (fp, 4));
  write_register (PC_REGNUM, read_memory_integer (fp + 4, 4));
  write_register (SP_REGNUM, fp + 8);
  flush_cached_frames ();
}

/* Simulate single-step.  */

typedef enum
{
  NORMAL4, /* a normal 4 byte insn */
  NORMAL8, /* a normal 8 byte insn */
  BRANCH4, /* a 4 byte branch insn, including ones without delay slots */
  BRANCH8, /* an 8 byte branch insn, including ones with delay slots */
} insn_type;

/* Return the type of INSN and store in TARGET the destination address of a
   branch if this is one.  */
/* ??? Need to verify all cases are properly handled.  */

static insn_type
get_insn_type (insn, pc, target)
     unsigned long insn;
     CORE_ADDR pc, *target;
{
  unsigned long limm;

  switch (insn >> 27)
    {
    case 0 : case 1 : case 2 : /* load/store insns */
      if (LIMM_P (X_A (insn))
	  || LIMM_P (X_B (insn))
	  || LIMM_P (X_C (insn)))
	return NORMAL8;
      return NORMAL4;
    case 4 : case 5 : case 6 : /* branch insns */
      *target = pc + 4 + X_L (insn);
      /* ??? It isn't clear that this is always the right answer.
	 The problem occurs when the next insn is an 8 byte insn.  If the
	 branch is conditional there's no worry as there shouldn't be an 8
	 byte insn following.  The programmer may be cheating if s/he knows
	 the branch will never be taken, but we don't deal with that.
	 Note that the programmer is also allowed to play games by putting
	 an insn with long immediate data in the delay slot and then duplicate
	 the long immediate data at the branch target.  Ugh!  */
      if (X_N (insn) == 0)
	return BRANCH4;
      return BRANCH8;
    case 7 : /* jump insns */
      if (LIMM_P (X_B (insn)))
	{
	  limm = read_memory_integer (pc + 4, 4);
	  *target = ARC_PC_TO_REAL_ADDRESS (limm);
	  return BRANCH8;
	}
      if (SHIMM_P (X_B (insn)))
	*target = ARC_PC_TO_REAL_ADDRESS (X_D (insn));
      else
	*target = ARC_PC_TO_REAL_ADDRESS (read_register (X_B (insn)));
      if (X_Q (insn) == 0 && X_N (insn) == 0)
	return BRANCH4;
      return BRANCH8;
    default : /* arithmetic insns, etc. */
      if (LIMM_P (X_A (insn))
	  || LIMM_P (X_B (insn))
	  || LIMM_P (X_C (insn)))
	return NORMAL8;
      return NORMAL4;
    }
}

/* single_step() is called just before we want to resume the inferior, if we
   want to single-step it but there is no hardware or kernel single-step
   support.  We find all the possible targets of the coming instruction and
   breakpoint them.

   single_step is also called just after the inferior stops.  If we had
   set up a simulated single-step, we undo our damage.  */

void
arc_software_single_step (ignore, insert_breakpoints_p)
     enum target_signal ignore; /* sig but we don't need it */
     int insert_breakpoints_p;
{
  static CORE_ADDR next_pc, target;
  static int brktrg_p;
  typedef char binsn_quantum[BREAKPOINT_MAX];
  static binsn_quantum break_mem[2];

  if (insert_breakpoints_p)
    {
      insn_type type;
      CORE_ADDR pc;
      unsigned long insn;

      pc = read_register (PC_REGNUM);
      insn = read_memory_integer (pc, 4);
      type = get_insn_type (insn, pc, &target);

      /* Always set a breakpoint for the insn after the branch.  */
      next_pc = pc + ((type == NORMAL8 || type == BRANCH8) ? 8 : 4);
      target_insert_breakpoint (next_pc, break_mem[0]);

      brktrg_p = 0;

      if ((type == BRANCH4 || type == BRANCH8)
	  /* Watch out for branches to the following location.
	     We just stored a breakpoint there and another call to
	     target_insert_breakpoint will think the real insn is the
	     breakpoint we just stored there.  */
	  && target != next_pc)
	{
	  brktrg_p = 1;
	  target_insert_breakpoint (target, break_mem[1]);
	}

    }
  else
    {
      /* Remove breakpoints.  */
      target_remove_breakpoint (next_pc, break_mem[0]);

      if (brktrg_p)
	target_remove_breakpoint (target, break_mem[1]);

      /* Fix the pc.  */
      stop_pc -= DECR_PC_AFTER_BREAK;
      write_pc (stop_pc);
    }
}

#ifdef GET_LONGJMP_TARGET
/* Figure out where the longjmp will land.  Slurp the args out of the stack.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

int
get_longjmp_target(pc)
     CORE_ADDR *pc;
{
  char buf[TARGET_PTR_BIT / TARGET_CHAR_BIT];
  CORE_ADDR sp, jb_addr;

  sp = read_register (SP_REGNUM);

  if (target_read_memory (sp + SP_ARG0, /* Offset of first arg on stack */
			  buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  jb_addr = extract_address (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  if (target_read_memory (jb_addr + JB_PC * JB_ELEMENT_SIZE, buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_address (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  return 1;
}
#endif /* GET_LONGJMP_TARGET */

/* Disassemble one instruction.  */

static int
arc_print_insn (vma, info)
     bfd_vma vma;
     disassemble_info *info;
{
  static int current_mach;
  static int current_endian;
  static disassembler_ftype current_disasm;

  if (current_disasm == NULL
      || arc_bfd_mach_type != current_mach
      || TARGET_BYTE_ORDER != current_endian)
    {
      current_mach = arc_bfd_mach_type;
      current_endian = TARGET_BYTE_ORDER;
      current_disasm = arc_get_disassembler (current_mach,
					     current_endian == BIG_ENDIAN);
    }

  return (*current_disasm) (vma, info);
}

/* Command to set cpu type.  */

void
arc_set_cpu_type_command (args, from_tty)
     char *args;
     int from_tty;
{
  int i;

  if (tmp_arc_cpu_type == NULL || *tmp_arc_cpu_type == '\0')
    {
      printf_unfiltered ("The known ARC cpu types are as follows:\n");
      for (i = 0; arc_cpu_type_table[i].name != NULL; ++i)
	printf_unfiltered ("%s\n", arc_cpu_type_table[i].name);

      /* Restore the value.  */
      tmp_arc_cpu_type = strsave (arc_cpu_type);

      return;
    }
  
  if (!arc_set_cpu_type (tmp_arc_cpu_type))
    {
      error ("Unknown cpu type `%s'.", tmp_arc_cpu_type);
      /* Restore its value.  */
      tmp_arc_cpu_type = strsave (arc_cpu_type);
    }
}

static void
arc_show_cpu_type_command (args, from_tty)
     char *args;
     int from_tty;
{
}

/* Modify the actual cpu type.
   Result is a boolean indicating success.  */

int
arc_set_cpu_type (str)
     char *str;
{
  int i, j;

  if (str == NULL)
    return 0;

  for (i = 0; arc_cpu_type_table[i].name != NULL; ++i)
    {
      if (strcasecmp (str, arc_cpu_type_table[i].name) == 0)
	{
	  arc_cpu_type = str;
	  arc_bfd_mach_type = arc_cpu_type_table[i].value;
	  return 1;
	}
    }

  return 0;
}

void
_initialize_arc_tdep ()
{
  struct cmd_list_element *c;

  c = add_set_cmd ("cpu", class_support, var_string_noescape,
		   (char *) &tmp_arc_cpu_type,
		   "Set the type of ARC cpu in use.\n\
This command has two purposes.  In a multi-cpu system it lets one\n\
change the cpu being debugged.  It also gives one access to\n\
cpu-type-specific registers and recognize cpu-type-specific instructions.\
",
		   &setlist);
  c->function.cfunc = arc_set_cpu_type_command;
  c = add_show_from_set (c, &showlist);
  c->function.cfunc = arc_show_cpu_type_command;

  /* We have to use strsave here because the `set' command frees it before
     setting a new value.  */
  tmp_arc_cpu_type = strsave (DEFAULT_ARC_CPU_TYPE);
  arc_set_cpu_type (tmp_arc_cpu_type);

  c = add_set_cmd ("displaypipeline", class_support, var_zinteger,
		   (char *) &display_pipeline_p,
		   "Set pipeline display (simulator only).\n\
When enabled, the state of the pipeline after each cycle is displayed.",
		   &setlist);
  c = add_show_from_set (c, &showlist);

  c = add_set_cmd ("debugpipeline", class_support, var_zinteger,
		   (char *) &debug_pipeline_p,
		   "Set pipeline debug display (simulator only).\n\
When enabled, debugging information about the pipeline is displayed.",
		   &setlist);
  c = add_show_from_set (c, &showlist);

  c = add_set_cmd ("cputimer", class_support, var_zinteger,
		   (char *) &cpu_timer,
		   "Set maximum cycle count (simulator only).\n\
Control will return to gdb if the timer expires.\n\
A negative value disables the timer.",
		   &setlist);
  c = add_show_from_set (c, &showlist);

  tm_print_insn = arc_print_insn;
}
