/* Intel 386 native support.
   Copyright (C) 1988, 1989, 1991, 1992 Free Software Foundation, Inc.

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
#include "language.h"
#include "gdbcore.h"

#ifdef USG
#include <sys/types.h>
#endif

#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/file.h>
#include "gdb_stat.h"

#include <stddef.h>
#include <sys/ptrace.h>

/* Does AIX define this in <errno.h>?  */
extern int errno;

#ifndef NO_SYS_REG_H
#include <sys/reg.h>
#endif

#include "floatformat.h"

#include "target.h"


/* this table must line up with REGISTER_NAMES in tm-i386v.h */
/* symbols like 'EAX' come from <sys/reg.h> */
static int regmap[] = 
{
  EAX, ECX, EDX, EBX,
  USP, EBP, ESI, EDI,
  EIP, EFL, CS, SS,
  DS, ES, FS, GS,
};

/* blockend is the value of u.u_ar0, and points to the
 * place where GS is stored
 */

int
i386_register_u_addr (blockend, regnum)
     int blockend;
     int regnum;
{
#if 0
  /* this will be needed if fp registers are reinstated */
  /* for now, you can look at them with 'info float'
   * sys5 wont let you change them with ptrace anyway
   */
  if (regnum >= FP0_REGNUM && regnum <= FP7_REGNUM) 
    {
      int ubase, fpstate;
      struct user u;
      ubase = blockend + 4 * (SS + 1) - KSTKSZ;
      fpstate = ubase + ((char *)&u.u_fpstate - (char *)&u);
      return (fpstate + 0x1c + 10 * (regnum - FP0_REGNUM));
    } 
  else
#endif
    return (blockend + 4 * regmap[regnum]);
  
}

/* The code below only work on the aix ps/2 (i386-ibm-aix) -
 * mtranle@paris - Sat Apr 11 10:34:12 1992
 */

struct env387 
{
  unsigned short control;
  unsigned short r0;
  unsigned short status;
  unsigned short r1;
  unsigned short tag;
  unsigned short r2;
  unsigned long eip;
  unsigned short code_seg;
  unsigned short opcode;
  unsigned long operand;
  unsigned short operand_seg;
  unsigned short r3;
  unsigned char regs[8][10];
};

static
print_387_status (status, ep)
     unsigned short status;
     struct env387 *ep;
{
  int i;
  int bothstatus;
  int top;
  int fpreg;
  unsigned char *p;
  
  bothstatus = ((status != 0) && (ep->status != 0));
  if (status != 0) 
    {
      if (bothstatus)
	printf_unfiltered ("u: ");
      print_387_status_word (status);
    }
  
  if (ep->status != 0) 
    {
      if (bothstatus)
	printf_unfiltered ("e: ");
      print_387_status_word (ep->status);
    }
  
  print_387_control_word (ep->control);
  printf_unfiltered ("last exception: ");
  printf_unfiltered ("opcode %s; ", local_hex_string(ep->opcode));
  printf_unfiltered ("pc %s:", local_hex_string(ep->code_seg));
  printf_unfiltered ("%s; ", local_hex_string(ep->eip));
  printf_unfiltered ("operand %s", local_hex_string(ep->operand_seg));
  printf_unfiltered (":%s\n", local_hex_string(ep->operand));

  top = ((ep->status >> 11) & 7);

  printf_unfiltered ("regno  tag  msb              lsb  value\n");
  for (fpreg = 7; fpreg >= 0; fpreg--) 
    {
      double val;

      printf_unfiltered ("%s %d: ", fpreg == top ? "=>" : "  ", fpreg);

      switch ((ep->tag >> ((7 - fpreg) * 2)) & 3) 
	{
	case 0: printf_unfiltered ("valid "); break;
	case 1: printf_unfiltered ("zero  "); break;
	case 2: printf_unfiltered ("trap  "); break;
	case 3: printf_unfiltered ("empty "); break;
	}
      for (i = 9; i >= 0; i--)
	printf_unfiltered ("%02x", ep->regs[fpreg][i]);
      
      i387_to_double ((char *)ep->regs[fpreg], (char *)&val);
      printf_unfiltered ("  %#g\n", val);
    }
}

static struct env387 core_env387;

void
i386_float_info ()
{
  struct env387 fps;
  int fpsaved = 0;
  /* We need to reverse the order of the registers.  Apparently AIX stores
     the highest-numbered ones first.  */
  struct env387 fps_fixed;
  int i;

  if (inferior_pid)
    {
      char buf[10];
      unsigned short status;

      ptrace (PT_READ_FPR, inferior_pid, buf, offsetof(struct env387, status));
      memcpy (&status, buf, sizeof (status));
      fpsaved = status;
    }
  else
    {
      if ((fpsaved = core_env387.status) != 0)
	memcpy(&fps, &core_env387, sizeof(fps));
    }
  
  if (fpsaved == 0) 
    {
      printf_unfiltered ("no floating point status saved\n");
      return;
    }

  if (inferior_pid)
    {
      int offset;
      for (offset = 0; offset < sizeof(fps); offset += 10)
	{
	  char buf[10];
	  ptrace (PT_READ_FPR, inferior_pid, buf, offset);
	  memcpy ((char *)&fps.control + offset, buf,
		  MIN(10, sizeof(fps) - offset));
	}
    } 
  fps_fixed = fps;
  for (i = 0; i < 8; ++i)
    memcpy (fps_fixed.regs[i], fps.regs[7 - i], 10);
  print_387_status (0, &fps_fixed);
}

/* Fetch one register.  */
static void
fetch_register (regno)
     int regno;
{
  char buf[MAX_REGISTER_RAW_SIZE];
  if (regno < FP0_REGNUM)
    *(int *)buf = ptrace (PT_READ_GPR, inferior_pid,
			  PT_REG(regmap[regno]), 0, 0);
  else
    ptrace (PT_READ_FPR, inferior_pid, buf,
	    (regno - FP0_REGNUM)*10 + offsetof(struct env387, regs));
  supply_register (regno, buf);
}

void
fetch_inferior_registers (regno)
     int regno;
{
  if (regno < 0)
    for (regno = 0; regno < NUM_REGS; regno++)
      fetch_register (regno);
  else
    fetch_register (regno);
}

/* store one register */
static void
store_register (regno)
     int regno;
{
  char buf[80];
  extern char registers[];
  errno = 0;
  if (regno < FP0_REGNUM)
    ptrace (PT_WRITE_GPR, inferior_pid, PT_REG(regmap[regno]),
	    *(int *) &registers[REGISTER_BYTE (regno)], 0);
  else
    ptrace (PT_WRITE_FPR, inferior_pid, &registers[REGISTER_BYTE (regno)],
	    (regno - FP0_REGNUM)*10 + offsetof(struct env387, regs));

  if (errno != 0)
    {
      sprintf (buf, "writing register number %d", regno);
      perror_with_name (buf);
    }
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */
void
store_inferior_registers (regno)
     int regno;
{
  if (regno < 0)
    for (regno = 0; regno < NUM_REGS; regno++)
      store_register (regno);
  else
    store_register (regno);
}

#ifndef CD_AX			/* defined in sys/i386/coredump.h */
# define CD_AX	0
# define CD_BX	1
# define CD_CX	2
# define CD_DX	3
# define CD_SI	4
# define CD_DI	5
# define CD_BP	6
# define CD_SP	7
# define CD_FL	8
# define CD_IP	9
# define CD_CS	10
# define CD_DS	11
# define CD_ES	12
# define CD_FS	13
# define CD_GS	14
# define CD_SS	15
#endif

/*
 * The order here in core_regmap[] has to be the same as in 
 * regmap[] above.
 */
static int core_regmap[] = 
{
  CD_AX, CD_CX, CD_DX, CD_BX,
  CD_SP, CD_BP, CD_SI, CD_DI,
  CD_IP, CD_FL, CD_CS, CD_SS,
  CD_DS, CD_ES, CD_FS, CD_GS,
};

static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned int reg_addr;	/* ignored */
{

  if (which == 0)
    {
      /* Integer registers */

#define cd_regs(n) ((int *)core_reg_sect)[n]
#define regs(n) *((int *) &registers[REGISTER_BYTE (n)])

      int i;
      for (i = 0; i < FP0_REGNUM; i++)
	regs(i) = cd_regs(core_regmap[i]);
    }
  else if (which == 2)
    {
      /* Floating point registers */

      if (core_reg_size >= sizeof (core_env387))
	memcpy (&core_env387, core_reg_sect, core_reg_size);
      else
	fprintf_unfiltered (gdb_stderr, "Couldn't read float regs from core file\n");
    }
}


/* Register that we are able to handle i386aix core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */

static struct core_fns i386aix_core_fns =
{
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_i386aix ()
{
  add_core_fns (&i386aix_core_fns);
}
