/* Intel 386 target-dependent stuff.
   Copyright (C) 1988, 1989, 1991, 1994, 1995, 1996, 1998
   Free Software Foundation, Inc.

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
#include "gdb_string.h"
#include "frame.h"
#include "inferior.h"
#include "gdbcore.h"
#include "target.h"
#include "floatformat.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "command.h"

static long i386_get_frame_setup PARAMS ((CORE_ADDR));

static void i386_follow_jump PARAMS ((void));

static void codestream_read PARAMS ((unsigned char *, int));

static void codestream_seek PARAMS ((CORE_ADDR));

static unsigned char codestream_fill PARAMS ((int));

CORE_ADDR skip_trampoline_code PARAMS ((CORE_ADDR, char *));

static int gdb_print_insn_i386 (bfd_vma, disassemble_info *);

void _initialize_i386_tdep PARAMS ((void));

/* This is the variable the is set with "set disassembly-flavor",
 and its legitimate values. */
static char att_flavor[] = "att";
static char intel_flavor[] = "intel";
static char *valid_flavors[] = {
  att_flavor,
  intel_flavor,
  NULL
};
static char *disassembly_flavor = att_flavor;

/* Stdio style buffering was used to minimize calls to ptrace, but this
   buffering did not take into account that the code section being accessed
   may not be an even number of buffers long (even if the buffer is only
   sizeof(int) long).  In cases where the code section size happened to
   be a non-integral number of buffers long, attempting to read the last
   buffer would fail.  Simply using target_read_memory and ignoring errors,
   rather than read_memory, is not the correct solution, since legitimate
   access errors would then be totally ignored.  To properly handle this
   situation and continue to use buffering would require that this code
   be able to determine the minimum code section size granularity (not the
   alignment of the section itself, since the actual failing case that
   pointed out this problem had a section alignment of 4 but was not a
   multiple of 4 bytes long), on a target by target basis, and then
   adjust it's buffer size accordingly.  This is messy, but potentially
   feasible.  It probably needs the bfd library's help and support.  For
   now, the buffer size is set to 1.  (FIXME -fnf) */

#define CODESTREAM_BUFSIZ 1	/* Was sizeof(int), see note above. */
static CORE_ADDR codestream_next_addr;
static CORE_ADDR codestream_addr;
static unsigned char codestream_buf[CODESTREAM_BUFSIZ];
static int codestream_off;
static int codestream_cnt;

#define codestream_tell() (codestream_addr + codestream_off)
#define codestream_peek() (codestream_cnt == 0 ? \
			   codestream_fill(1): codestream_buf[codestream_off])
#define codestream_get() (codestream_cnt-- == 0 ? \
			 codestream_fill(0) : codestream_buf[codestream_off++])

static unsigned char 
codestream_fill (peek_flag)
    int peek_flag;
{
  codestream_addr = codestream_next_addr;
  codestream_next_addr += CODESTREAM_BUFSIZ;
  codestream_off = 0;
  codestream_cnt = CODESTREAM_BUFSIZ;
  read_memory (codestream_addr, (char *) codestream_buf, CODESTREAM_BUFSIZ);
  
  if (peek_flag)
    return (codestream_peek());
  else
    return (codestream_get());
}

static void
codestream_seek (place)
    CORE_ADDR place;
{
  codestream_next_addr = place / CODESTREAM_BUFSIZ;
  codestream_next_addr *= CODESTREAM_BUFSIZ;
  codestream_cnt = 0;
  codestream_fill (1);
  while (codestream_tell() != place)
    codestream_get ();
}

static void
codestream_read (buf, count)
     unsigned char *buf;
     int count;
{
  unsigned char *p;
  int i;
  p = buf;
  for (i = 0; i < count; i++)
    *p++ = codestream_get ();
}

/* next instruction is a jump, move to target */

static void
i386_follow_jump ()
{
  unsigned char buf[4];
  long delta;

  int data16;
  CORE_ADDR pos;

  pos = codestream_tell ();

  data16 = 0;
  if (codestream_peek () == 0x66)
    {
      codestream_get ();
      data16 = 1;
    }

  switch (codestream_get ())
    {
    case 0xe9:
      /* relative jump: if data16 == 0, disp32, else disp16 */
      if (data16)
	{
	  codestream_read (buf, 2);
	  delta = extract_signed_integer (buf, 2);

	  /* include size of jmp inst (including the 0x66 prefix).  */
	  pos += delta + 4; 
	}
      else
	{
	  codestream_read (buf, 4);
	  delta = extract_signed_integer (buf, 4);

	  pos += delta + 5;
	}
      break;
    case 0xeb:
      /* relative jump, disp8 (ignore data16) */
      codestream_read (buf, 1);
      /* Sign-extend it.  */
      delta = extract_signed_integer (buf, 1);

      pos += delta + 2;
      break;
    }
  codestream_seek (pos);
}

/*
 * find & return amound a local space allocated, and advance codestream to
 * first register push (if any)
 *
 * if entry sequence doesn't make sense, return -1, and leave 
 * codestream pointer random
 */

static long
i386_get_frame_setup (pc)
     CORE_ADDR pc;
{
  unsigned char op;

  codestream_seek (pc);

  i386_follow_jump ();

  op = codestream_get ();

  if (op == 0x58)		/* popl %eax */
    {
      /*
       * this function must start with
       * 
       *    popl %eax		  0x58
       *    xchgl %eax, (%esp)  0x87 0x04 0x24
       * or xchgl %eax, 0(%esp) 0x87 0x44 0x24 0x00
       *
       * (the system 5 compiler puts out the second xchg
       * inst, and the assembler doesn't try to optimize it,
       * so the 'sib' form gets generated)
       * 
       * this sequence is used to get the address of the return
       * buffer for a function that returns a structure
       */
      int pos;
      unsigned char buf[4];
      static unsigned char proto1[3] = { 0x87,0x04,0x24 };
      static unsigned char proto2[4] = { 0x87,0x44,0x24,0x00 };
      pos = codestream_tell ();
      codestream_read (buf, 4);
      if (memcmp (buf, proto1, 3) == 0)
	pos += 3;
      else if (memcmp (buf, proto2, 4) == 0)
	pos += 4;

      codestream_seek (pos);
      op = codestream_get (); /* update next opcode */
    }

  if (op == 0x68 || op == 0x6a)
    {
      /*
       * this function may start with
       *
       *   pushl constant
       *   call _probe
       *   addl $4, %esp
       *      followed by 
       *     pushl %ebp
       *     etc.
       */
      int pos;
      unsigned char buf[8];

      /* Skip past the pushl instruction; it has either a one-byte 
         or a four-byte operand, depending on the opcode.  */
      pos = codestream_tell ();
      if (op == 0x68)
	pos += 4;
      else
	pos += 1;
      codestream_seek (pos);

      /* Read the following 8 bytes, which should be "call _probe" (6 bytes)
         followed by "addl $4,%esp" (2 bytes).  */
      codestream_read (buf, sizeof (buf));
      if (buf[0] == 0xe8 && buf[6] == 0xc4 && buf[7] == 0x4)
	pos += sizeof (buf);
      codestream_seek (pos);
      op = codestream_get (); /* update next opcode */
    }

  if (op == 0x55)		/* pushl %ebp */
    {			
      /* check for movl %esp, %ebp - can be written two ways */
      switch (codestream_get ())
	{
	case 0x8b:
	  if (codestream_get () != 0xec)
	    return (-1);
	  break;
	case 0x89:
	  if (codestream_get () != 0xe5)
	    return (-1);
	  break;
	default:
	  return (-1);
	}
      /* check for stack adjustment 
       *
       *  subl $XXX, %esp
       *
       * note: you can't subtract a 16 bit immediate
       * from a 32 bit reg, so we don't have to worry
       * about a data16 prefix 
       */
      op = codestream_peek ();
      if (op == 0x83)
	{
	  /* subl with 8 bit immed */
	  codestream_get ();
	  if (codestream_get () != 0xec)
	    /* Some instruction starting with 0x83 other than subl.  */
	    {
	      codestream_seek (codestream_tell () - 2);
	      return 0;
	    }
	  /* subl with signed byte immediate 
	   * (though it wouldn't make sense to be negative)
	   */
	  return (codestream_get());
	}
      else if (op == 0x81)
	{
	  char buf[4];
	  /* Maybe it is subl with 32 bit immedediate.  */
	  codestream_get();
	  if (codestream_get () != 0xec)
	    /* Some instruction starting with 0x81 other than subl.  */
	    {
	      codestream_seek (codestream_tell () - 2);
	      return 0;
	    }
	  /* It is subl with 32 bit immediate.  */
	  codestream_read ((unsigned char *)buf, 4);
	  return extract_signed_integer (buf, 4);
	}
      else
	{
	  return (0);
	}
    }
  else if (op == 0xc8)
    {
      char buf[2];
      /* enter instruction: arg is 16 bit unsigned immed */
      codestream_read ((unsigned char *)buf, 2);
      codestream_get (); /* flush final byte of enter instruction */
      return extract_unsigned_integer (buf, 2);
    }
  return (-1);
}

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */

int
i386_frame_num_args (fi)
     struct frame_info *fi;
{
#if 1
  return -1;
#else
  /* This loses because not only might the compiler not be popping the
     args right after the function call, it might be popping args from both
     this call and a previous one, and we would say there are more args
     than there really are.  */

  int retpc;						
  unsigned char op;					
  struct frame_info *pfi;

  /* on the 386, the instruction following the call could be:
     popl %ecx        -  one arg
     addl $imm, %esp  -  imm/4 args; imm may be 8 or 32 bits
     anything else    -  zero args  */

  int frameless;

  FRAMELESS_FUNCTION_INVOCATION (fi, frameless);
  if (frameless)
    /* In the absence of a frame pointer, GDB doesn't get correct values
       for nameless arguments.  Return -1, so it doesn't print any
       nameless arguments.  */
    return -1;

  pfi = get_prev_frame_info (fi);			
  if (pfi == 0)
    {
      /* Note:  this can happen if we are looking at the frame for
	 main, because FRAME_CHAIN_VALID won't let us go into
	 start.  If we have debugging symbols, that's not really
	 a big deal; it just means it will only show as many arguments
	 to main as are declared.  */
      return -1;
    }
  else
    {
      retpc = pfi->pc;					
      op = read_memory_integer (retpc, 1);			
      if (op == 0x59)					
	/* pop %ecx */			       
	return 1;				
      else if (op == 0x83)
	{
	  op = read_memory_integer (retpc+1, 1);	
	  if (op == 0xc4)				
	    /* addl $<signed imm 8 bits>, %esp */	
	    return (read_memory_integer (retpc+2,1)&0xff)/4;
	  else
	    return 0;
	}
      else if (op == 0x81)
	{ /* add with 32 bit immediate */
	  op = read_memory_integer (retpc+1, 1);	
	  if (op == 0xc4)				
	    /* addl $<imm 32>, %esp */		
	    return read_memory_integer (retpc+2, 4) / 4;
	  else
	    return 0;
	}
      else
	{
	  return 0;
	}
    }
#endif
}

/*
 * parse the first few instructions of the function to see
 * what registers were stored.
 *
 * We handle these cases:
 *
 * The startup sequence can be at the start of the function,
 * or the function can start with a branch to startup code at the end.
 *
 * %ebp can be set up with either the 'enter' instruction, or 
 * 'pushl %ebp, movl %esp, %ebp' (enter is too slow to be useful,
 * but was once used in the sys5 compiler)
 *
 * Local space is allocated just below the saved %ebp by either the
 * 'enter' instruction, or by 'subl $<size>, %esp'.  'enter' has
 * a 16 bit unsigned argument for space to allocate, and the
 * 'addl' instruction could have either a signed byte, or
 * 32 bit immediate.
 *
 * Next, the registers used by this function are pushed.  In
 * the sys5 compiler they will always be in the order: %edi, %esi, %ebx
 * (and sometimes a harmless bug causes it to also save but not restore %eax);
 * however, the code below is willing to see the pushes in any order,
 * and will handle up to 8 of them.
 *
 * If the setup sequence is at the end of the function, then the
 * next instruction will be a branch back to the start.
 */

void
i386_frame_find_saved_regs (fip, fsrp)
     struct frame_info *fip;
     struct frame_saved_regs *fsrp;
{
  long locals = -1;
  unsigned char op;
  CORE_ADDR dummy_bottom;
  CORE_ADDR adr;
  CORE_ADDR pc;
  int i;
  
  memset (fsrp, 0, sizeof *fsrp);
  
  /* if frame is the end of a dummy, compute where the
   * beginning would be
   */
  dummy_bottom = fip->frame - 4 - REGISTER_BYTES - CALL_DUMMY_LENGTH;
  
  /* check if the PC is in the stack, in a dummy frame */
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
  
  pc = get_pc_function_start (fip->pc);
  if (pc != 0)
    locals = i386_get_frame_setup (pc);
  
  if (locals >= 0) 
    {
      adr = fip->frame - 4 - locals;
      for (i = 0; i < 8; i++) 
	{
	  op = codestream_get ();
	  if (op < 0x50 || op > 0x57)
	    break;
#ifdef I386_REGNO_TO_SYMMETRY
	  /* Dynix uses different internal numbering.  Ick.  */
	  fsrp->regs[I386_REGNO_TO_SYMMETRY(op - 0x50)] = adr;
#else
	  fsrp->regs[op - 0x50] = adr;
#endif
	  adr -= 4;
	}
    }
  
  fsrp->regs[PC_REGNUM] = fip->frame + 4;
  fsrp->regs[FP_REGNUM] = fip->frame;
}

/* return pc of first real instruction */

int
i386_skip_prologue (pc)
     int pc;
{
  unsigned char op;
  int i;
  static unsigned char pic_pat[6] = { 0xe8, 0, 0, 0, 0, /* call   0x0 */
				      0x5b,             /* popl   %ebx */
				    };
  CORE_ADDR pos;
  
  if (i386_get_frame_setup (pc) < 0)
    return (pc);
  
  /* found valid frame setup - codestream now points to 
   * start of push instructions for saving registers
   */
  
  /* skip over register saves */
  for (i = 0; i < 8; i++)
    {
      op = codestream_peek ();
      /* break if not pushl inst */
      if (op < 0x50 || op > 0x57) 
	break;
      codestream_get ();
    }

  /* The native cc on SVR4 in -K PIC mode inserts the following code to get
     the address of the global offset table (GOT) into register %ebx.
      call	0x0
      popl	%ebx
      movl	%ebx,x(%ebp)	(optional)
      addl	y,%ebx
     This code is with the rest of the prologue (at the end of the
     function), so we have to skip it to get to the first real
     instruction at the start of the function.  */
     
  pos = codestream_tell ();
  for (i = 0; i < 6; i++)
    {
      op = codestream_get ();
      if (pic_pat [i] != op)
	break;
    }
  if (i == 6)
    {
      unsigned char buf[4];
      long delta = 6;

      op = codestream_get ();
      if (op == 0x89)			/* movl %ebx, x(%ebp) */
	{
	  op = codestream_get ();
	  if (op == 0x5d)		/* one byte offset from %ebp */
	    {
	      delta += 3;
	      codestream_read (buf, 1);
	    }
	  else if (op == 0x9d)		/* four byte offset from %ebp */
	    {
	      delta += 6;
	      codestream_read (buf, 4);
	    }
	  else				/* unexpected instruction */
	      delta = -1;
          op = codestream_get ();
	}
					/* addl y,%ebx */
      if (delta > 0 && op == 0x81 && codestream_get () == 0xc3) 
	{
	    pos += delta + 6;
	}
    }
  codestream_seek (pos);
  
  i386_follow_jump ();
  
  return (codestream_tell ());
}

void
i386_push_dummy_frame ()
{
  CORE_ADDR sp = read_register (SP_REGNUM);
  int regnum;
  char regbuf[MAX_REGISTER_RAW_SIZE];
  
  sp = push_word (sp, read_register (PC_REGNUM));
  sp = push_word (sp, read_register (FP_REGNUM));
  write_register (FP_REGNUM, sp);
  for (regnum = 0; regnum < NUM_REGS; regnum++)
    {
      read_register_gen (regnum, regbuf);
      sp = push_bytes (sp, regbuf, REGISTER_RAW_SIZE (regnum));
    }
  write_register (SP_REGNUM, sp);
}

void
i386_pop_frame ()
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

void
i386_extract_return_value(type, regbuf, valbuf)
     struct type *type;
     char regbuf[REGISTER_BYTES];
     char *valbuf;
{
/* On AIX, floating point values are returned in floating point registers.  */
#ifdef I386_AIX_TARGET
  if (TYPE_CODE_FLT == TYPE_CODE(type))
    {
      double d;
      /* 387 %st(0), gcc uses this */
      floatformat_to_double (&floatformat_i387_ext,
			     &regbuf[REGISTER_BYTE(FP0_REGNUM)],
			     &d);
      store_floating (valbuf, TYPE_LENGTH (type), d);
    }
  else
#endif /* I386_AIX_TARGET */
    { 
      memcpy (valbuf, regbuf, TYPE_LENGTH (type)); 
    }
}

#ifdef I386V4_SIGTRAMP_SAVED_PC
/* Get saved user PC for sigtramp from the pushed ucontext on the stack
   for all three variants of SVR4 sigtramps.  */

CORE_ADDR
i386v4_sigtramp_saved_pc (frame)
     struct frame_info *frame;
{
  CORE_ADDR saved_pc_offset = 4;
  char *name = NULL;

  find_pc_partial_function (frame->pc, &name, NULL, NULL);
  if (name)
    {
      if (STREQ (name, "_sigreturn"))
	saved_pc_offset = 132 + 14 * 4;
      else if (STREQ (name, "_sigacthandler"))
	saved_pc_offset = 80 + 14 * 4;
      else if (STREQ (name, "sigvechandler"))
	saved_pc_offset = 120 + 14 * 4;
    }

  if (frame->next)
    return read_memory_integer (frame->next->frame + saved_pc_offset, 4);
  return read_memory_integer (read_register (SP_REGNUM) + saved_pc_offset, 4);
}
#endif /* I386V4_SIGTRAMP_SAVED_PC */

#ifdef STATIC_TRANSFORM_NAME
/* SunPRO encodes the static variables.  This is not related to C++ mangling,
   it is done for C too.  */

char *
sunpro_static_transform_name (name)
     char *name;
{
  char *p;
  if (IS_STATIC_TRANSFORM_NAME (name))
    {
      /* For file-local statics there will be a period, a bunch
	 of junk (the contents of which match a string given in the
	 N_OPT), a period and the name.  For function-local statics
	 there will be a bunch of junk (which seems to change the
	 second character from 'A' to 'B'), a period, the name of the
	 function, and the name.  So just skip everything before the
	 last period.  */
      p = strrchr (name, '.');
      if (p != NULL)
	name = p + 1;
    }
  return name;
}
#endif /* STATIC_TRANSFORM_NAME */



/* Stuff for WIN32 PE style DLL's but is pretty generic really. */

CORE_ADDR
skip_trampoline_code (pc, name)
     CORE_ADDR pc;
     char *name;
{
  if (pc && read_memory_unsigned_integer (pc, 2) == 0x25ff) /* jmp *(dest) */
    {
      unsigned long indirect = read_memory_unsigned_integer (pc+2, 4);
      struct minimal_symbol *indsym =
	indirect ? lookup_minimal_symbol_by_pc (indirect) : 0;
      char *symname = indsym ? SYMBOL_NAME(indsym) : 0;

      if (symname) 
	{
	  if (strncmp (symname,"__imp_", 6) == 0
	      || strncmp (symname,"_imp_", 5) == 0)
	    return name ? 1 : read_memory_unsigned_integer (indirect, 4);
	}
    }
  return 0;			/* not a trampoline */
}

static int
gdb_print_insn_i386 (memaddr, info)
     bfd_vma memaddr;
     disassemble_info * info;
{
  /* XXX remove when binutils 2.9.2 is imported */
#if 0
  if (disassembly_flavor == att_flavor)
    return print_insn_i386_att (memaddr, info);
  else if (disassembly_flavor == intel_flavor)
    return print_insn_i386_intel (memaddr, info);
#else
  return print_insn_i386 (memaddr, info);
#endif
}

void
_initialize_i386_tdep ()
{
  tm_print_insn = gdb_print_insn_i386;
  tm_print_insn_info.mach = bfd_lookup_arch (bfd_arch_i386, 0)->mach;

  /* Add the variable that controls the disassembly flavor */
  add_show_from_set(
	    add_set_enum_cmd ("disassembly-flavor", no_class,
				  valid_flavors,
				  (char *) &disassembly_flavor,
				  "Set the disassembly flavor, the valid values are \"att\" and \"intel\", \
and the default value is \"att\".",
				  &setlist),
	    &showlist);

  
}
