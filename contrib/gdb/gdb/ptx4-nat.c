/* Native-dependent code for ptx 4.0
   Copyright 1988, 1989, 1991, 1992, 1994, 1999, 2000, 2001
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/param.h>
#include <fcntl.h>

/* Prototypes for supply_gregset etc. */
#include "gregset.h"

/*  Given a pointer to a general register set in /proc format (gregset_t *),
   unpack the register contents and supply them as gdb's idea of the current
   register values. */

void
supply_gregset (gregset_t *gregsetp)
{
  supply_register (EAX_REGNUM, (char *) &(*gregsetp)[EAX]);
  supply_register (EDX_REGNUM, (char *) &(*gregsetp)[EDX]);
  supply_register (ECX_REGNUM, (char *) &(*gregsetp)[ECX]);
  supply_register (EBX_REGNUM, (char *) &(*gregsetp)[EBX]);
  supply_register (ESI_REGNUM, (char *) &(*gregsetp)[ESI]);
  supply_register (EDI_REGNUM, (char *) &(*gregsetp)[EDI]);
  supply_register (ESP_REGNUM, (char *) &(*gregsetp)[UESP]);
  supply_register (EBP_REGNUM, (char *) &(*gregsetp)[EBP]);
  supply_register (EIP_REGNUM, (char *) &(*gregsetp)[EIP]);
  supply_register (EFLAGS_REGNUM, (char *) &(*gregsetp)[EFL]);
}

void
fill_gregset (gregset_t *gregsetp, int regno)
{
  int regi;

  for (regi = 0; regi < NUM_REGS; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  (*gregsetp)[regi] = *(greg_t *) & registers[REGISTER_BYTE (regi)];
	}
    }
}

/*  Given a pointer to a floating point register set in /proc format
   (fpregset_t *), unpack the register contents and supply them as gdb's
   idea of the current floating point register values. */

void
supply_fpregset (fpregset_t *fpregsetp)
{
  supply_fpu_registers ((struct fpusave *) &fpregsetp->fp_reg_set);
  supply_fpa_registers ((struct fpasave *) &fpregsetp->f_wregs);
}

/*  Given a pointer to a floating point register set in /proc format
   (fpregset_t *), update the register specified by REGNO from gdb's idea
   of the current floating point register set.  If REGNO is -1, update
   them all. */

void
fill_fpregset (fpregset_t *fpregsetp, int regno)
{
  int regi;
  char *to;
  char *from;

  /* FIXME: see m68k-tdep.c for an example, for the m68k. */
}

/*
 * This doesn't quite do the same thing as the procfs.c version, but give
 * it the same name so we don't have to put an ifdef in solib.c.
 */
/* this could use elf_interpreter() from elfread.c */
int
proc_iterate_over_mappings (int (*func) (int, CORE_ADDR))
{
  vaddr_t curseg, memptr;
  pt_vseg_t pv;
  int rv, cmperr;
  sec_ptr interp_sec;
  char *interp_content;
  int interp_fd, funcstat;
  unsigned int size;
  char buf1[NBPG], buf2[NBPG];

  /*
   * The following is really vile.  We can get the name of the
   * shared library from the exec_bfd, and we can get a list of
   * each virtual memory segment, but there is no simple way to
   * find the mapped segment from the shared library (ala
   * procfs's PIOCOPENMEM).  As a pretty nasty kludge, we
   * compare the virtual memory segment to the contents of the
   * .interp file.  If they match, we assume that we've got the
   * right one.
   */

  /*
   * TODO: for attach, use XPT_OPENT to get the executable, in
   * case we're attached without knowning the executable's
   * filename.
   */

#ifdef VERBOSE_DEBUG
  printf ("proc_iter\n");
#endif
  interp_sec = bfd_get_section_by_name (exec_bfd, ".interp");
  if (!interp_sec)
    {
      return 0;
    }

  size = bfd_section_size (exec_bfd, interp_sec);
  interp_content = alloca (size);
  if (0 == bfd_get_section_contents (exec_bfd, interp_sec,
				     interp_content, (file_ptr) 0, size))
    {
      return 0;
    }

#ifdef VERBOSE_DEBUG
  printf ("proc_iter: \"%s\"\n", interp_content);
#endif
  interp_fd = open (interp_content, O_RDONLY, 0);
  if (-1 == interp_fd)
    {
      return 0;
    }

  curseg = 0;
  while (1)
    {
      rv = ptrace (PT_NEXT_VSEG, PIDGET (inferior_ptid), &pv, curseg);
#ifdef VERBOSE_DEBUG
      printf ("PT_NEXT_VSEG: rv %d errno %d\n", rv, errno);
#endif
      if (-1 == rv)
	break;
      if (0 == rv)
	break;
#ifdef VERBOSE_DEBUG
      printf ("pv.pv_start 0x%x pv_size 0x%x pv_prot 0x%x\n",
	      pv.pv_start, pv.pv_size, pv.pv_prot);
#endif
      curseg = pv.pv_start + pv.pv_size;

      rv = lseek (interp_fd, 0, SEEK_SET);
      if (-1 == rv)
	{
	  perror ("lseek");
	  close (interp_fd);
	  return 0;
	}
      for (memptr = pv.pv_start; memptr < pv.pv_start + pv.pv_size;
	   memptr += NBPG)
	{
#ifdef VERBOSE_DEBUG
	  printf ("memptr 0x%x\n", memptr);
#endif
	  rv = read (interp_fd, buf1, NBPG);
	  if (-1 == rv)
	    {
	      perror ("read");
	      close (interp_fd);
	      return 0;
	    }
	  rv = ptrace (PT_RDATA_PAGE, PIDGET (inferior_ptid), buf2,
		       memptr);
	  if (-1 == rv)
	    {
	      perror ("ptrace");
	      close (interp_fd);
	      return 0;
	    }
	  cmperr = memcmp (buf1, buf2, NBPG);
	  if (cmperr)
	    break;
	}
      if (0 == cmperr)
	{
	  /* this is it */
	  funcstat = (*func) (interp_fd, pv.pv_start);
	  break;
	}
    }
  close (interp_fd);
  return 0;
}
