/* Native support for Motorola 88k running Harris CX/UX.
   Copyright 1988, 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include "gdbcore.h"
#include <sys/user.h>

#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "symtab.h"

#ifndef USER			/* added to support BCS ptrace_user */
#define USER ptrace_user
#endif
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/file.h>
#include "gdb_stat.h"

#include "symtab.h"
#include "setjmp.h"
#include "value.h"

#include <sys/ptrace.h>

/* CX/UX provides them already, but as word offsets instead of char offsets */
#define SXIP_OFFSET (PT_SXIP * 4)
#define SNIP_OFFSET (PT_SNIP * 4)
#define SFIP_OFFSET (PT_SFIP * 4)
#define PSR_OFFSET  (PT_PSR  * sizeof(int))
#define FPSR_OFFSET (PT_FPSR * sizeof(int))
#define FPCR_OFFSET (PT_FPCR * sizeof(int))

#define XREGADDR(r) (((char *)&u.pt_x0-(char *)&u) + \
                     ((r)-X0_REGNUM)*sizeof(X_REGISTER_RAW_TYPE))

extern int have_symbol_file_p();

extern jmp_buf stack_jmp;

extern int errno;
extern char registers[REGISTER_BYTES];

void
fetch_inferior_registers (regno)
     int regno;		/* Original value discarded */
{
  register unsigned int regaddr;
  char buf[MAX_REGISTER_RAW_SIZE];
  register int i;

  struct USER u;
  unsigned int offset;

  offset = (char *) &u.pt_r0 - (char *) &u; 
  regaddr = offset; /* byte offset to r0;*/

/*  offset = ptrace (3, inferior_pid, (PTRACE_ARG3_TYPE) offset, 0) - KERNEL_U_ADDR; */
  for (regno = 0; regno < PC_REGNUM; regno++)
    {
      /*regaddr = register_addr (regno, offset);*/
	/* 88k enhancement  */
        
      for (i = 0; i < REGISTER_RAW_SIZE (regno); i += sizeof (int))
 	{
 	  *(int *) &buf[i] = ptrace (3, inferior_pid,
				     (PTRACE_ARG3_TYPE) regaddr, 0);
 	  regaddr += sizeof (int);
 	}
      supply_register (regno, buf);
    }
    /* now load up registers 32-37; special pc registers */
    *(int *) &buf[0] = ptrace (3, inferior_pid,
			       (PTRACE_ARG3_TYPE) PSR_OFFSET,0);
    supply_register (PSR_REGNUM, buf);
    *(int *) &buf[0] = ptrace (3, inferior_pid,
			       (PTRACE_ARG3_TYPE) FPSR_OFFSET,0);
    supply_register (FPSR_REGNUM, buf);
    *(int *) &buf[0] = ptrace (3, inferior_pid,
			       (PTRACE_ARG3_TYPE) FPCR_OFFSET,0);
    supply_register (FPCR_REGNUM, buf);
    *(int *) &buf[0] = ptrace (3,inferior_pid,
			       (PTRACE_ARG3_TYPE) SXIP_OFFSET ,0);
    supply_register (SXIP_REGNUM, buf);
    *(int *) &buf[0] = ptrace (3, inferior_pid,
			       (PTRACE_ARG3_TYPE) SNIP_OFFSET,0);
    supply_register (SNIP_REGNUM, buf);
    *(int *) &buf[0] = ptrace (3, inferior_pid,
			       (PTRACE_ARG3_TYPE) SFIP_OFFSET,0);
    supply_register (SFIP_REGNUM, buf);

    if (target_is_m88110) 
      {
        for (regaddr = XREGADDR(X0_REGNUM), regno = X0_REGNUM;
             regno < NUM_REGS; 
             regno++, regaddr += 16)
          {
            X_REGISTER_RAW_TYPE xval;

            *(int *) &xval.w1 = ptrace (3, inferior_pid,
                                        (PTRACE_ARG3_TYPE) regaddr, 0);
            *(int *) &xval.w2 = ptrace (3, inferior_pid,
                                        (PTRACE_ARG3_TYPE) (regaddr+4), 0);
            *(int *) &xval.w3 = ptrace (3, inferior_pid,
                                        (PTRACE_ARG3_TYPE) (regaddr+8), 0);
            *(int *) &xval.w4 = ptrace (3, inferior_pid,
                                        (PTRACE_ARG3_TYPE) (regaddr+12), 0);
            supply_register(regno, (void *)&xval);
          }
      }
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  register unsigned int regaddr;
  char buf[80];

  struct USER u;

  unsigned int offset = (char *) &u.pt_r0 - (char *) &u;

  regaddr = offset;

  /* Don't try to deal with EXIP_REGNUM or ENIP_REGNUM, because I think either
     svr3 doesn't run on an 88110, or the kernel isolates the different (not
     completely sure this is true, but seems to be.  */
  if (regno >= 0)
    {
      /*      regaddr = register_addr (regno, offset); */
      if (regno < PC_REGNUM)
	{ 
	  regaddr = offset + regno * sizeof (int);
	  errno = 0;
	  ptrace (6, inferior_pid,
		  (PTRACE_ARG3_TYPE) regaddr, read_register (regno));
	  if (errno != 0)
	    {
	      sprintf (buf, "writing register number %d", regno);
	      perror_with_name (buf);
	    }
	}
      else if (regno == PSR_REGNUM)
        ptrace (6, inferior_pid,
		(PTRACE_ARG3_TYPE) PSR_OFFSET, read_register(regno));
      else if (regno == FPSR_REGNUM)
        ptrace (6, inferior_pid,
		(PTRACE_ARG3_TYPE) FPSR_OFFSET, read_register(regno));
      else if (regno == FPCR_REGNUM)
        ptrace (6, inferior_pid,
	        (PTRACE_ARG3_TYPE) FPCR_OFFSET, read_register(regno));
      else if (regno == SXIP_REGNUM)
	ptrace (6, inferior_pid,
		(PTRACE_ARG3_TYPE) SXIP_OFFSET, read_register(regno));
      else if (regno == SNIP_REGNUM)
	ptrace (6, inferior_pid,
		(PTRACE_ARG3_TYPE) SNIP_OFFSET, read_register(regno));
      else if (regno == SFIP_REGNUM)
	ptrace (6, inferior_pid,
		(PTRACE_ARG3_TYPE) SFIP_OFFSET, read_register(regno));
      else if (target_is_m88110 && regno < NUM_REGS)
        {
          X_REGISTER_RAW_TYPE xval;
          
          read_register_bytes(REGISTER_BYTE(regno), (char *)&xval, 
                              sizeof(X_REGISTER_RAW_TYPE));
          regaddr = XREGADDR(regno);
          ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) regaddr, xval.w1);
          ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) regaddr+4, xval.w2);
          ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) regaddr+8, xval.w3);
          ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) regaddr+12, xval.w4);
        }
      else
	printf_unfiltered ("Bad register number for store_inferior routine\n");
    }
  else
    { 
      for (regno = 0; regno < PC_REGNUM; regno++)
	{
	  /*      regaddr = register_addr (regno, offset); */
	  errno = 0;
	  regaddr = offset + regno * sizeof (int);
	  ptrace (6, inferior_pid,
		  (PTRACE_ARG3_TYPE) regaddr, read_register (regno));
	  if (errno != 0)
	    {
	      sprintf (buf, "writing register number %d", regno);
	      perror_with_name (buf);
	    }
	}
      ptrace (6, inferior_pid,
              (PTRACE_ARG3_TYPE) PSR_OFFSET, read_register(regno));
      ptrace (6, inferior_pid,
              (PTRACE_ARG3_TYPE) FPSR_OFFSET,read_register(regno));
      ptrace (6, inferior_pid,
              (PTRACE_ARG3_TYPE) FPCR_OFFSET,read_register(regno));
      ptrace (6,inferior_pid,
	      (PTRACE_ARG3_TYPE) SXIP_OFFSET,read_register(SXIP_REGNUM));
      ptrace (6,inferior_pid,
	      (PTRACE_ARG3_TYPE) SNIP_OFFSET,read_register(SNIP_REGNUM));
      ptrace (6,inferior_pid,
	      (PTRACE_ARG3_TYPE) SFIP_OFFSET,read_register(SFIP_REGNUM));
      if (target_is_m88110)
        {
          for (regno = X0_REGNUM; regno < NUM_REGS; regno++) 
            {
              X_REGISTER_RAW_TYPE xval;
     
              read_register_bytes(REGISTER_BYTE(regno), (char *)&xval, 
                                  sizeof(X_REGISTER_RAW_TYPE));
              regaddr = XREGADDR(regno);
              ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) regaddr, xval.w1);
              ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) (regaddr+4), xval.w2);
              ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) (regaddr+8), xval.w3);
              ptrace (6, inferior_pid, (PTRACE_ARG3_TYPE) (regaddr+12), xval.w4);
            }
        }
    }
}

/* blockend is the address of the end of the user structure */

m88k_register_u_addr (blockend, regnum)
     int blockend, regnum;
{
  struct USER u;
  int ustart = blockend - sizeof (struct USER);

  if (regnum < PSR_REGNUM)
      return (ustart + ((int) &u.pt_r0 - (int) &u) +
              REGISTER_SIZE * regnum);
  else if (regnum == PSR_REGNUM)
      return (ustart + ((int) &u.pt_psr) - (int) &u);
  else if (regnum == FPSR_REGNUM)
      return (ustart + ((int) &u.pt_fpsr) - (int) &u);
  else if (regnum == FPCR_REGNUM)
      return (ustart + ((int) &u.pt_fpcr) - (int) &u);
  else if (regnum == SXIP_REGNUM)
      return (ustart + SXIP_OFFSET);
  else if (regnum == SNIP_REGNUM)
      return (ustart + SNIP_OFFSET);
  else if (regnum == SFIP_REGNUM)
      return (ustart + SFIP_OFFSET);
  else if (target_is_m88110) 
      return (ustart + ((int) &u.pt_x0 - (int) &u) +   /* Must be X register */
               sizeof(u.pt_x0) * (regnum - X0_REGNUM));
  else
      return (blockend + REGISTER_SIZE * regnum);
}

#ifdef USE_PROC_FS

#include <sys/procfs.h>

/*  Given a pointer to a general register set in /proc format (gregset_t *),
    unpack the register contents and supply them as gdb's idea of the current
    register values. */

void
supply_gregset (gregsetp)
     gregset_t *gregsetp;
{
    register int regi;
    register greg_t *regp = (greg_t *) gregsetp;

    for (regi=0; regi <= SP_REGNUM; regi++)
	supply_register (regi, (char *) (regp + regi));

    supply_register (SXIP_REGNUM, (char *) (regp + R_XIP));
    supply_register (SNIP_REGNUM, (char *) (regp + R_NIP));
    supply_register (SFIP_REGNUM, (char *) (regp + R_FIP));
    supply_register (PSR_REGNUM, (char *) (regp + R_PSR));
    supply_register (FPSR_REGNUM, (char *) (regp + R_FPSR));
    supply_register (FPCR_REGNUM, (char *) (regp + R_FPCR));
}

void
fill_gregset (gregsetp, regno)
     gregset_t *gregsetp;
     int regno;
{
    int regi;
    register greg_t *regp = (greg_t *) gregsetp;
    extern char registers[];

    for (regi = 0 ; regi <= R_R31 ; regi++)
	if ((regno == -1) || (regno == regi))
	    *(regp + regi) = *(int *) &registers[REGISTER_BYTE(regi)];

    if ((regno == -1) || (regno == SXIP_REGNUM))
	*(regp + R_XIP) = *(int *) &registers[REGISTER_BYTE(SXIP_REGNUM)];
    if ((regno == -1) || (regno == SNIP_REGNUM))
	*(regp + R_NIP) = *(int *) &registers[REGISTER_BYTE(SNIP_REGNUM)];
    if ((regno == -1) || (regno == SFIP_REGNUM))
	*(regp + R_FIP) = *(int *) &registers[REGISTER_BYTE(SFIP_REGNUM)];
    if ((regno == -1) || (regno == PSR_REGNUM))
	*(regp + R_PSR) = *(int *) &registers[REGISTER_BYTE(PSR_REGNUM)];
    if ((regno == -1) || (regno == FPSR_REGNUM))
	*(regp + R_FPSR) = *(int *) &registers[REGISTER_BYTE(FPSR_REGNUM)];
    if ((regno == -1) || (regno == FPCR_REGNUM))
	*(regp + R_FPCR) = *(int *) &registers[REGISTER_BYTE(FPCR_REGNUM)];
}

#endif /* USE_PROC_FS */

/* This support adds the equivalent of adb's % command.  When
   the `add-shared-symbol-files' command is given, this routine scans 
   the dynamic linker's link map and reads the minimal symbols
   from each shared object file listed in the map. */

struct link_map {
  unsigned long l_addr;		/* address at which object is mapped */
  char *l_name;			/* full name of loaded object */
  void *l_ld;			/* dynamic structure of object */
  struct link_map *l_next;	/* next link object */
  struct link_map *l_prev;	/* previous link object */
};

#define LINKS_MAP_POINTER "_ld_tail"
#define LIBC_FILE "/usr/lib/libc.so.1"
#define SHARED_OFFSET 0xf0001000

#ifndef PATH_MAX
#define PATH_MAX 1023		/* maximum size of path name on OS */
#endif

void
add_shared_symbol_files ()
{
  void *desc;
  struct link_map *ld_map, *lm, lms;
  struct minimal_symbol *minsym; 
  struct objfile *objfile;
  char *path_name;

  if (! inferior_pid)
    {
      warning ("The program has not yet been started.");
      return;
    }

  objfile = symbol_file_add (LIBC_FILE, 0, 0, 0, 0, 1);
  minsym = lookup_minimal_symbol (LINKS_MAP_POINTER, objfile);

  ld_map = (struct link_map *)
    read_memory_integer (((int)SYMBOL_VALUE_ADDRESS(minsym) + SHARED_OFFSET), 4);
  lm = ld_map;
  while (lm)
    {
      int local_errno = 0;

      read_memory ((CORE_ADDR)lm, (char*)&lms, sizeof (struct link_map));
      if (lms.l_name)
	{
	  if (target_read_string ((CORE_ADDR)lms.l_name, &path_name, 
                                  PATH_MAX, &local_errno))
	    {
	      symbol_file_add (path_name, 1, lms.l_addr, 0, 0, 0);
              free(path_name);
	    }
	}
      /* traverse links in reverse order so that we get the
	 the symbols the user actually gets. */
      lm = lms.l_prev;
    }

  /* Getting new symbols may change our opinion about what is
     frameless.  */
  reinit_frame_cache ();
}

#if defined(_ES_MP)

#include <sys/regset.h>

unsigned int
m88k_harris_core_register_addr (regno, reg_ptr)
     int regno, reg_ptr;
{
   unsigned int word_offset;

   switch (regno)
     {
     case PSR_REGNUM:
       word_offset = R_EPSR;
       break;
     case FPSR_REGNUM:
       word_offset = R_FPSR;
       break;
     case FPCR_REGNUM:
       word_offset = R_FPCR;
       break;
     case SXIP_REGNUM:
       word_offset = R_EXIP;
       break;
     case SNIP_REGNUM:
       word_offset = R_ENIP;
       break;
     case SFIP_REGNUM:
       word_offset = R_EFIP;
       break;
     default:
       if (regno <= FP_REGNUM) 
	 word_offset = regno;
       else 
	 word_offset = ((regno - X0_REGNUM) * 4);
     }
   return (word_offset * 4);
}

#endif /* _ES_MP */

void
_initialize_m88k_nat()
{
#ifdef _ES_MP
   /* Enable 88110 support, as we don't support the 88100 under ES/MP.  */

   target_is_m88110 = 1;
#elif defined(_CX_UX)
   /* Determine whether we're running on an 88100 or an 88110.  */
   target_is_m88110 = (sinfo(SYSMACHINE,0) == SYS5800);
#endif /* _CX_UX */
}

#ifdef _ES_MP
/* Given a pointer to a general register set in /proc format (gregset_t *),
   unpack the register contents and supply them as gdb's idea of the current
   register values. */

void
supply_gregset (gregsetp)
     gregset_t *gregsetp;
{
  register int regi;
  register greg_t *regp = (greg_t *) gregsetp;

  for (regi = 0 ; regi < R_R31 ; regi++)
    {
      supply_register (regi, (char *) (regp + regi));
    }
  supply_register (PSR_REGNUM,  (char *) (regp + R_EPSR));
  supply_register (FPSR_REGNUM, (char *) (regp + R_FPSR));
  supply_register (FPCR_REGNUM, (char *) (regp + R_FPCR));
  supply_register (SXIP_REGNUM, (char *) (regp + R_EXIP));
  supply_register (SNIP_REGNUM, (char *) (regp + R_ENIP));
  supply_register (SFIP_REGNUM, (char *) (regp + R_EFIP));
}

/* Given a pointer to a floating point register set in /proc format
   (fpregset_t *), unpack the register contents and supply them as gdb's
   idea of the current floating point register values.  */

void 
supply_fpregset (fpregsetp)
     fpregset_t *fpregsetp;
{
  register int regi;
  char *from;
  
  for (regi = FP0_REGNUM ; regi <= FPLAST_REGNUM ; regi++)
    {
      from = (char *) &((*fpregsetp)[regi-FP0_REGNUM]);
      supply_register (regi, from);
    }
}

#endif /* _ES_MP */

#ifdef _CX_UX

#include <sys/regset.h>

unsigned int m88k_harris_core_register_addr(int regno, int reg_ptr)
{
   unsigned int word_offset;

   switch (regno) {
    case PSR_REGNUM  : word_offset = R_PSR;  break;
    case FPSR_REGNUM : word_offset = R_FPSR; break;
    case FPCR_REGNUM : word_offset = R_FPCR; break;
    case SXIP_REGNUM : word_offset = R_XIP;  break;
    case SNIP_REGNUM : word_offset = R_NIP;  break;
    case SFIP_REGNUM : word_offset = R_FIP;  break;
    default :
      if (regno <= FP_REGNUM) 
            word_offset = regno;
      else 
            word_offset = ((regno - X0_REGNUM) * 4) + R_X0;
   }
   return (word_offset * 4);
}

#endif /* _CX_UX */
