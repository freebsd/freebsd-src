/* Native support for the SGI Iris running IRIX version 5, for GDB.
   Copyright 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998,
   1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Alessandro Forin(af@cs.cmu.edu) at CMU
   and by Per Bothner(bothner@cs.wisc.edu) at U.Wisconsin.
   Implemented for Irix 4.x by Garrett A. Wollman.
   Modified for Irix 5.x by Ian Lance Taylor.

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
#include "target.h"
#include "regcache.h"

#include "gdb_string.h"
#include <sys/time.h>
#include <sys/procfs.h>
#include <setjmp.h>		/* For JB_XXX.  */

/* Prototypes for supply_gregset etc. */
#include "gregset.h"

static void fetch_core_registers (char *, unsigned int, int, CORE_ADDR);

/* Size of elements in jmpbuf */

#define JB_ELEMENT_SIZE 4

/*
 * See the comment in m68k-tdep.c regarding the utility of these functions.
 *
 * These definitions are from the MIPS SVR4 ABI, so they may work for
 * any MIPS SVR4 target.
 */

void
supply_gregset (gregset_t *gregsetp)
{
  register int regi;
  register greg_t *regp = &(*gregsetp)[0];
  int gregoff = sizeof (greg_t) - MIPS_REGSIZE;
  static char zerobuf[MAX_REGISTER_RAW_SIZE] =
  {0};

  for (regi = 0; regi <= CTX_RA; regi++)
    supply_register (regi, (char *) (regp + regi) + gregoff);

  supply_register (PC_REGNUM, (char *) (regp + CTX_EPC) + gregoff);
  supply_register (HI_REGNUM, (char *) (regp + CTX_MDHI) + gregoff);
  supply_register (LO_REGNUM, (char *) (regp + CTX_MDLO) + gregoff);
  supply_register (CAUSE_REGNUM, (char *) (regp + CTX_CAUSE) + gregoff);

  /* Fill inaccessible registers with zero.  */
  supply_register (BADVADDR_REGNUM, zerobuf);
}

void
fill_gregset (gregset_t *gregsetp, int regno)
{
  int regi;
  register greg_t *regp = &(*gregsetp)[0];

  /* Under Irix6, if GDB is built with N32 ABI and is debugging an O32
     executable, we have to sign extend the registers to 64 bits before
     filling in the gregset structure.  */

  for (regi = 0; regi <= CTX_RA; regi++)
    if ((regno == -1) || (regno == regi))
      *(regp + regi) =
	extract_signed_integer (&registers[REGISTER_BYTE (regi)],
				REGISTER_RAW_SIZE (regi));

  if ((regno == -1) || (regno == PC_REGNUM))
    *(regp + CTX_EPC) =
      extract_signed_integer (&registers[REGISTER_BYTE (PC_REGNUM)],
			      REGISTER_RAW_SIZE (PC_REGNUM));

  if ((regno == -1) || (regno == CAUSE_REGNUM))
    *(regp + CTX_CAUSE) =
      extract_signed_integer (&registers[REGISTER_BYTE (CAUSE_REGNUM)],
			      REGISTER_RAW_SIZE (CAUSE_REGNUM));

  if ((regno == -1) || (regno == HI_REGNUM))
    *(regp + CTX_MDHI) =
      extract_signed_integer (&registers[REGISTER_BYTE (HI_REGNUM)],
			      REGISTER_RAW_SIZE (HI_REGNUM));

  if ((regno == -1) || (regno == LO_REGNUM))
    *(regp + CTX_MDLO) =
      extract_signed_integer (&registers[REGISTER_BYTE (LO_REGNUM)],
			      REGISTER_RAW_SIZE (LO_REGNUM));
}

/*
 * Now we do the same thing for floating-point registers.
 * We don't bother to condition on FP0_REGNUM since any
 * reasonable MIPS configuration has an R3010 in it.
 *
 * Again, see the comments in m68k-tdep.c.
 */

void
supply_fpregset (fpregset_t *fpregsetp)
{
  register int regi;
  static char zerobuf[MAX_REGISTER_RAW_SIZE] =
  {0};

  /* FIXME, this is wrong for the N32 ABI which has 64 bit FP regs. */

  for (regi = 0; regi < 32; regi++)
    supply_register (FP0_REGNUM + regi,
		     (char *) &fpregsetp->fp_r.fp_regs[regi]);

  supply_register (FCRCS_REGNUM, (char *) &fpregsetp->fp_csr);

  /* FIXME: how can we supply FCRIR_REGNUM?  SGI doesn't tell us. */
  supply_register (FCRIR_REGNUM, zerobuf);
}

void
fill_fpregset (fpregset_t *fpregsetp, int regno)
{
  int regi;
  char *from, *to;

  /* FIXME, this is wrong for the N32 ABI which has 64 bit FP regs. */

  for (regi = FP0_REGNUM; regi < FP0_REGNUM + 32; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &registers[REGISTER_BYTE (regi)];
	  to = (char *) &(fpregsetp->fp_r.fp_regs[regi - FP0_REGNUM]);
	  memcpy (to, from, REGISTER_RAW_SIZE (regi));
	}
    }

  if ((regno == -1) || (regno == FCRCS_REGNUM))
    fpregsetp->fp_csr = *(unsigned *) &registers[REGISTER_BYTE (FCRCS_REGNUM)];
}


/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

int
get_longjmp_target (CORE_ADDR *pc)
{
  char *buf;
  CORE_ADDR jb_addr;

  buf = alloca (TARGET_PTR_BIT / TARGET_CHAR_BIT);
  jb_addr = read_register (A0_REGNUM);

  if (target_read_memory (jb_addr + JB_PC * JB_ELEMENT_SIZE, buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_address (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  return 1;
}

/* Provide registers to GDB from a core file.

   CORE_REG_SECT points to an array of bytes, which were obtained from
   a core file which BFD thinks might contain register contents. 
   CORE_REG_SIZE is its size.

   Normally, WHICH says which register set corelow suspects this is:
     0 --- the general-purpose register set
     2 --- the floating-point register set
   However, for Irix 5, WHICH isn't used.

   REG_ADDR is also unused.  */

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size,
		      int which, CORE_ADDR reg_addr)
{
  if (core_reg_size == REGISTER_BYTES)
    {
      memcpy ((char *) registers, core_reg_sect, core_reg_size);
    }
  else if (MIPS_REGSIZE == 4 &&
	   core_reg_size == (2 * MIPS_REGSIZE) * NUM_REGS)
    {
      /* This is a core file from a N32 executable, 64 bits are saved
         for all registers.  */
      char *srcp = core_reg_sect;
      char *dstp = registers;
      int regno;

      for (regno = 0; regno < NUM_REGS; regno++)
	{
	  if (regno >= FP0_REGNUM && regno < (FP0_REGNUM + 32))
	    {
	      /* FIXME, this is wrong, N32 has 64 bit FP regs, but GDB
	         currently assumes that they are 32 bit.  */
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	      if (REGISTER_RAW_SIZE (regno) == 4)
		{
		  /* copying 4 bytes from eight bytes?
		     I don't see how this can be right...  */
		  srcp += 4;
		}
	      else
		{
		  /* copy all 8 bytes (sizeof(double)) */
		  *dstp++ = *srcp++;
		  *dstp++ = *srcp++;
		  *dstp++ = *srcp++;
		  *dstp++ = *srcp++;
		}
	    }
	  else
	    {
	      srcp += 4;
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	      *dstp++ = *srcp++;
	    }
	}
    }
  else
    {
      warning ("wrong size gregset struct in core file");
      return;
    }

  registers_fetched ();
}

/* Irix 5 uses what appears to be a unique form of shared library
   support.  This is a copy of solib.c modified for Irix 5.  */
/* FIXME: Most of this code could be merged with osfsolib.c and solib.c
   by using next_link_map_member and xfer_link_map_member in solib.c.  */

#include <sys/types.h>
#include <signal.h>
#include <sys/param.h>
#include <fcntl.h>

/* <obj.h> includes <sym.h> and <symconst.h>, which causes conflicts
   with our versions of those files included by tm-mips.h.  Prevent
   <obj.h> from including them with some appropriate defines.  */
#define __SYM_H__
#define __SYMCONST_H__
#include <obj.h>
#ifdef HAVE_OBJLIST_H
#include <objlist.h>
#endif

#ifdef NEW_OBJ_INFO_MAGIC
#define HANDLE_NEW_OBJ_LIST
#endif

#include "symtab.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "command.h"
#include "frame.h"
#include "gdb_regex.h"
#include "inferior.h"
#include "language.h"
#include "gdbcmd.h"

/* The symbol which starts off the list of shared libraries.  */
#define DEBUG_BASE "__rld_obj_head"

/* Irix 6.x introduces a new variant of object lists.
   To be able to debug O32 executables under Irix 6, we have to handle both
   variants.  */

typedef enum
{
  OBJ_LIST_OLD,			/* Pre Irix 6.x object list.  */
  OBJ_LIST_32,			/* 32 Bit Elf32_Obj_Info.  */
  OBJ_LIST_64			/* 64 Bit Elf64_Obj_Info, FIXME not yet implemented.  */
}
obj_list_variant;

/* Define our own link_map structure.
   This will help to share code with osfsolib.c and solib.c.  */

struct link_map
  {
    obj_list_variant l_variant;	/* which variant of object list */
    CORE_ADDR l_lladdr;		/* addr in inferior list was read from */
    CORE_ADDR l_next;		/* address of next object list entry */
  };

/* Irix 5 shared objects are pre-linked to particular addresses
   although the dynamic linker may have to relocate them if the
   address ranges of the libraries used by the main program clash.
   The offset is the difference between the address where the object
   is mapped and the binding address of the shared library.  */
#define LM_OFFSET(so) ((so) -> offset)
/* Loaded address of shared library.  */
#define LM_ADDR(so) ((so) -> lmstart)

char shadow_contents[BREAKPOINT_MAX];	/* Stash old bkpt addr contents */

struct so_list
  {
    struct so_list *next;	/* next structure in linked list */
    struct link_map lm;
    CORE_ADDR offset;		/* prelink to load address offset */
    char *so_name;		/* shared object lib name */
    CORE_ADDR lmstart;		/* lower addr bound of mapped object */
    CORE_ADDR lmend;		/* upper addr bound of mapped object */
    char symbols_loaded;	/* flag: symbols read in yet? */
    char from_tty;		/* flag: print msgs? */
    struct objfile *objfile;	/* objfile for loaded lib */
    struct section_table *sections;
    struct section_table *sections_end;
    struct section_table *textsection;
    bfd *abfd;
  };

static struct so_list *so_list_head;	/* List of known shared objects */
static CORE_ADDR debug_base;	/* Base of dynamic linker structures */
static CORE_ADDR breakpoint_addr;	/* Address where end bkpt is set */

/* Local function prototypes */

static void sharedlibrary_command (char *, int);

static int enable_break (void);

static int disable_break (void);

static void info_sharedlibrary_command (char *, int);

static int symbol_add_stub (void *);

static struct so_list *find_solib (struct so_list *);

static struct link_map *first_link_map_member (void);

static struct link_map *next_link_map_member (struct so_list *);

static void xfer_link_map_member (struct so_list *, struct link_map *);

static CORE_ADDR locate_base (void);

static int solib_map_sections (void *);

/*

   LOCAL FUNCTION

   solib_map_sections -- open bfd and build sections for shared lib

   SYNOPSIS

   static int solib_map_sections (struct so_list *so)

   DESCRIPTION

   Given a pointer to one of the shared objects in our list
   of mapped objects, use the recorded name to open a bfd
   descriptor for the object, build a section table, and then
   relocate all the section addresses by the base address at
   which the shared object was mapped.

   FIXMES

   In most (all?) cases the shared object file name recorded in the
   dynamic linkage tables will be a fully qualified pathname.  For
   cases where it isn't, do we really mimic the systems search
   mechanism correctly in the below code (particularly the tilde
   expansion stuff?).
 */

static int
solib_map_sections (void *arg)
{
  struct so_list *so = (struct so_list *) arg;	/* catch_errors bogon */
  char *filename;
  char *scratch_pathname;
  int scratch_chan;
  struct section_table *p;
  struct cleanup *old_chain;
  bfd *abfd;

  filename = tilde_expand (so->so_name);
  old_chain = make_cleanup (xfree, filename);

  scratch_chan = openp (getenv ("PATH"), 1, filename, O_RDONLY, 0,
			&scratch_pathname);
  if (scratch_chan < 0)
    {
      scratch_chan = openp (getenv ("LD_LIBRARY_PATH"), 1, filename,
			    O_RDONLY, 0, &scratch_pathname);
    }
  if (scratch_chan < 0)
    {
      perror_with_name (filename);
    }
  /* Leave scratch_pathname allocated.  abfd->name will point to it.  */

  abfd = bfd_fdopenr (scratch_pathname, gnutarget, scratch_chan);
  if (!abfd)
    {
      close (scratch_chan);
      error ("Could not open `%s' as an executable file: %s",
	     scratch_pathname, bfd_errmsg (bfd_get_error ()));
    }
  /* Leave bfd open, core_xfer_memory and "info files" need it.  */
  so->abfd = abfd;
  abfd->cacheable = 1;

  if (!bfd_check_format (abfd, bfd_object))
    {
      error ("\"%s\": not in executable format: %s.",
	     scratch_pathname, bfd_errmsg (bfd_get_error ()));
    }
  if (build_section_table (abfd, &so->sections, &so->sections_end))
    {
      error ("Can't find the file sections in `%s': %s",
	     bfd_get_filename (exec_bfd), bfd_errmsg (bfd_get_error ()));
    }

  for (p = so->sections; p < so->sections_end; p++)
    {
      /* Relocate the section binding addresses as recorded in the shared
         object's file by the offset to get the address to which the
         object was actually mapped.  */
      p->addr += LM_OFFSET (so);
      p->endaddr += LM_OFFSET (so);
      so->lmend = (CORE_ADDR) max (p->endaddr, so->lmend);
      if (STREQ (p->the_bfd_section->name, ".text"))
	{
	  so->textsection = p;
	}
    }

  /* Free the file names, close the file now.  */
  do_cleanups (old_chain);

  /* must be non-zero */
  return (1);
}

/*

   LOCAL FUNCTION

   locate_base -- locate the base address of dynamic linker structs

   SYNOPSIS

   CORE_ADDR locate_base (void)

   DESCRIPTION

   For both the SunOS and SVR4 shared library implementations, if the
   inferior executable has been linked dynamically, there is a single
   address somewhere in the inferior's data space which is the key to
   locating all of the dynamic linker's runtime structures.  This
   address is the value of the symbol defined by the macro DEBUG_BASE.
   The job of this function is to find and return that address, or to
   return 0 if there is no such address (the executable is statically
   linked for example).

   For SunOS, the job is almost trivial, since the dynamic linker and
   all of it's structures are statically linked to the executable at
   link time.  Thus the symbol for the address we are looking for has
   already been added to the minimal symbol table for the executable's
   objfile at the time the symbol file's symbols were read, and all we
   have to do is look it up there.  Note that we explicitly do NOT want
   to find the copies in the shared library.

   The SVR4 version is much more complicated because the dynamic linker
   and it's structures are located in the shared C library, which gets
   run as the executable's "interpreter" by the kernel.  We have to go
   to a lot more work to discover the address of DEBUG_BASE.  Because
   of this complexity, we cache the value we find and return that value
   on subsequent invocations.  Note there is no copy in the executable
   symbol tables.

   Irix 5 is basically like SunOS.

   Note that we can assume nothing about the process state at the time
   we need to find this address.  We may be stopped on the first instruc-
   tion of the interpreter (C shared library), the first instruction of
   the executable itself, or somewhere else entirely (if we attached
   to the process for example).

 */

static CORE_ADDR
locate_base (void)
{
  struct minimal_symbol *msymbol;
  CORE_ADDR address = 0;

  msymbol = lookup_minimal_symbol (DEBUG_BASE, NULL, symfile_objfile);
  if ((msymbol != NULL) && (SYMBOL_VALUE_ADDRESS (msymbol) != 0))
    {
      address = SYMBOL_VALUE_ADDRESS (msymbol);
    }
  return (address);
}

/*

   LOCAL FUNCTION

   first_link_map_member -- locate first member in dynamic linker's map

   SYNOPSIS

   static struct link_map *first_link_map_member (void)

   DESCRIPTION

   Read in a copy of the first member in the inferior's dynamic
   link map from the inferior's dynamic linker structures, and return
   a pointer to the link map descriptor.
 */

static struct link_map *
first_link_map_member (void)
{
  struct obj_list *listp;
  struct obj_list list_old;
  struct link_map *lm;
  static struct link_map first_lm;
  CORE_ADDR lladdr;
  CORE_ADDR next_lladdr;

  /* We have not already read in the dynamic linking structures
     from the inferior, lookup the address of the base structure. */
  debug_base = locate_base ();
  if (debug_base == 0)
    return NULL;

  /* Get address of first list entry.  */
  read_memory (debug_base, (char *) &listp, sizeof (struct obj_list *));

  if (listp == NULL)
    return NULL;

  /* Get first list entry.  */
  /* The MIPS Sign extends addresses. */
  lladdr = host_pointer_to_address (listp);
  read_memory (lladdr, (char *) &list_old, sizeof (struct obj_list));

  /* The first entry in the list is the object file we are debugging,
     so skip it.  */
  next_lladdr = host_pointer_to_address (list_old.next);

#ifdef HANDLE_NEW_OBJ_LIST
  if (list_old.data == NEW_OBJ_INFO_MAGIC)
    {
      Elf32_Obj_Info list_32;

      read_memory (lladdr, (char *) &list_32, sizeof (Elf32_Obj_Info));
      if (list_32.oi_size != sizeof (Elf32_Obj_Info))
	return NULL;
      next_lladdr = (CORE_ADDR) list_32.oi_next;
    }
#endif

  if (next_lladdr == 0)
    return NULL;

  first_lm.l_lladdr = next_lladdr;
  lm = &first_lm;
  return lm;
}

/*

   LOCAL FUNCTION

   next_link_map_member -- locate next member in dynamic linker's map

   SYNOPSIS

   static struct link_map *next_link_map_member (so_list_ptr)

   DESCRIPTION

   Read in a copy of the next member in the inferior's dynamic
   link map from the inferior's dynamic linker structures, and return
   a pointer to the link map descriptor.
 */

static struct link_map *
next_link_map_member (struct so_list *so_list_ptr)
{
  struct link_map *lm = &so_list_ptr->lm;
  CORE_ADDR next_lladdr = lm->l_next;
  static struct link_map next_lm;

  if (next_lladdr == 0)
    {
      /* We have hit the end of the list, so check to see if any were
         added, but be quiet if we can't read from the target any more. */
      int status = 0;

      if (lm->l_variant == OBJ_LIST_OLD)
	{
	  struct obj_list list_old;

	  status = target_read_memory (lm->l_lladdr,
				       (char *) &list_old,
				       sizeof (struct obj_list));
	  next_lladdr = host_pointer_to_address (list_old.next);
	}
#ifdef HANDLE_NEW_OBJ_LIST
      else if (lm->l_variant == OBJ_LIST_32)
	{
	  Elf32_Obj_Info list_32;
	  status = target_read_memory (lm->l_lladdr,
				       (char *) &list_32,
				       sizeof (Elf32_Obj_Info));
	  next_lladdr = (CORE_ADDR) list_32.oi_next;
	}
#endif

      if (status != 0 || next_lladdr == 0)
	return NULL;
    }

  next_lm.l_lladdr = next_lladdr;
  lm = &next_lm;
  return lm;
}

/*

   LOCAL FUNCTION

   xfer_link_map_member -- set local variables from dynamic linker's map

   SYNOPSIS

   static void xfer_link_map_member (so_list_ptr, lm)

   DESCRIPTION

   Read in a copy of the requested member in the inferior's dynamic
   link map from the inferior's dynamic linker structures, and fill
   in the necessary so_list_ptr elements.
 */

static void
xfer_link_map_member (struct so_list *so_list_ptr, struct link_map *lm)
{
  struct obj_list list_old;
  CORE_ADDR lladdr = lm->l_lladdr;
  struct link_map *new_lm = &so_list_ptr->lm;
  int errcode;

  read_memory (lladdr, (char *) &list_old, sizeof (struct obj_list));

  new_lm->l_variant = OBJ_LIST_OLD;
  new_lm->l_lladdr = lladdr;
  new_lm->l_next = host_pointer_to_address (list_old.next);

#ifdef HANDLE_NEW_OBJ_LIST
  if (list_old.data == NEW_OBJ_INFO_MAGIC)
    {
      Elf32_Obj_Info list_32;

      read_memory (lladdr, (char *) &list_32, sizeof (Elf32_Obj_Info));
      if (list_32.oi_size != sizeof (Elf32_Obj_Info))
	return;
      new_lm->l_variant = OBJ_LIST_32;
      new_lm->l_next = (CORE_ADDR) list_32.oi_next;

      target_read_string ((CORE_ADDR) list_32.oi_pathname,
			  &so_list_ptr->so_name,
			  list_32.oi_pathname_len + 1, &errcode);
      if (errcode != 0)
	memory_error (errcode, (CORE_ADDR) list_32.oi_pathname);

      LM_ADDR (so_list_ptr) = (CORE_ADDR) list_32.oi_ehdr;
      LM_OFFSET (so_list_ptr) =
	(CORE_ADDR) list_32.oi_ehdr - (CORE_ADDR) list_32.oi_orig_ehdr;
    }
  else
#endif
    {
#if defined (_MIPS_SIM_NABI32) && _MIPS_SIM == _MIPS_SIM_NABI32
      /* If we are compiling GDB under N32 ABI, the alignments in
         the obj struct are different from the O32 ABI and we will get
         wrong values when accessing the struct.
         As a workaround we use fixed values which are good for
         Irix 6.2.  */
      char buf[432];

      read_memory ((CORE_ADDR) list_old.data, buf, sizeof (buf));

      target_read_string (extract_address (&buf[236], 4),
			  &so_list_ptr->so_name,
			  INT_MAX, &errcode);
      if (errcode != 0)
	memory_error (errcode, extract_address (&buf[236], 4));

      LM_ADDR (so_list_ptr) = extract_address (&buf[196], 4);
      LM_OFFSET (so_list_ptr) =
	extract_address (&buf[196], 4) - extract_address (&buf[248], 4);
#else
      struct obj obj_old;

      read_memory ((CORE_ADDR) list_old.data, (char *) &obj_old,
		   sizeof (struct obj));

      target_read_string ((CORE_ADDR) obj_old.o_path,
			  &so_list_ptr->so_name,
			  INT_MAX, &errcode);
      if (errcode != 0)
	memory_error (errcode, (CORE_ADDR) obj_old.o_path);

      LM_ADDR (so_list_ptr) = (CORE_ADDR) obj_old.o_praw;
      LM_OFFSET (so_list_ptr) =
	(CORE_ADDR) obj_old.o_praw - obj_old.o_base_address;
#endif
    }

  catch_errors (solib_map_sections, (char *) so_list_ptr,
		"Error while mapping shared library sections:\n",
		RETURN_MASK_ALL);
}


/*

   LOCAL FUNCTION

   find_solib -- step through list of shared objects

   SYNOPSIS

   struct so_list *find_solib (struct so_list *so_list_ptr)

   DESCRIPTION

   This module contains the routine which finds the names of any
   loaded "images" in the current process. The argument in must be
   NULL on the first call, and then the returned value must be passed
   in on subsequent calls. This provides the capability to "step" down
   the list of loaded objects. On the last object, a NULL value is
   returned.
 */

static struct so_list *
find_solib (struct so_list *so_list_ptr)
{
  struct so_list *so_list_next = NULL;
  struct link_map *lm = NULL;
  struct so_list *new;

  if (so_list_ptr == NULL)
    {
      /* We are setting up for a new scan through the loaded images. */
      if ((so_list_next = so_list_head) == NULL)
	{
	  /* Find the first link map list member. */
	  lm = first_link_map_member ();
	}
    }
  else
    {
      /* We have been called before, and are in the process of walking
         the shared library list.  Advance to the next shared object. */
      lm = next_link_map_member (so_list_ptr);
      so_list_next = so_list_ptr->next;
    }
  if ((so_list_next == NULL) && (lm != NULL))
    {
      new = (struct so_list *) xmalloc (sizeof (struct so_list));
      memset ((char *) new, 0, sizeof (struct so_list));
      /* Add the new node as the next node in the list, or as the root
         node if this is the first one. */
      if (so_list_ptr != NULL)
	{
	  so_list_ptr->next = new;
	}
      else
	{
	  so_list_head = new;
	}
      so_list_next = new;
      xfer_link_map_member (new, lm);
    }
  return (so_list_next);
}

/* A small stub to get us past the arg-passing pinhole of catch_errors.  */

static int
symbol_add_stub (void *arg)
{
  register struct so_list *so = (struct so_list *) arg;		/* catch_errs bogon */
  CORE_ADDR text_addr = 0;
  struct section_addr_info section_addrs;

  memset (&section_addrs, 0, sizeof (section_addrs));
  if (so->textsection)
    text_addr = so->textsection->addr;
  else if (so->abfd != NULL)
    {
      asection *lowest_sect;

      /* If we didn't find a mapped non zero sized .text section, set up
         text_addr so that the relocation in symbol_file_add does no harm.  */

      lowest_sect = bfd_get_section_by_name (so->abfd, ".text");
      if (lowest_sect == NULL)
	bfd_map_over_sections (so->abfd, find_lowest_section,
			       (PTR) &lowest_sect);
      if (lowest_sect)
	text_addr = bfd_section_vma (so->abfd, lowest_sect) + LM_OFFSET (so);
    }


  section_addrs.other[0].name = ".text";
  section_addrs.other[0].addr = text_addr;
  so->objfile = symbol_file_add (so->so_name, so->from_tty,
				 &section_addrs, 0, 0);
  /* must be non-zero */
  return (1);
}

/*

   GLOBAL FUNCTION

   solib_add -- add a shared library file to the symtab and section list

   SYNOPSIS

   void solib_add (char *arg_string, int from_tty,
   struct target_ops *target, int readsyms)

   DESCRIPTION

 */

void
solib_add (char *arg_string, int from_tty, struct target_ops *target, int readsyms)
{
  register struct so_list *so = NULL;	/* link map state variable */

  /* Last shared library that we read.  */
  struct so_list *so_last = NULL;

  char *re_err;
  int count;
  int old;

  if (!readsyms)
    return;

  if ((re_err = re_comp (arg_string ? arg_string : ".")) != NULL)
    {
      error ("Invalid regexp: %s", re_err);
    }

  /* Add the shared library sections to the section table of the
     specified target, if any.  */
  if (target)
    {
      /* Count how many new section_table entries there are.  */
      so = NULL;
      count = 0;
      while ((so = find_solib (so)) != NULL)
	{
	  if (so->so_name[0])
	    {
	      count += so->sections_end - so->sections;
	    }
	}

      if (count)
	{
	  old = target_resize_to_sections (target, count);
	  
	  /* Add these section table entries to the target's table.  */
	  while ((so = find_solib (so)) != NULL)
	    {
	      if (so->so_name[0])
		{
		  count = so->sections_end - so->sections;
		  memcpy ((char *) (target->to_sections + old),
			  so->sections,
			  (sizeof (struct section_table)) * count);
		  old += count;
		}
	    }
	}
    }

  /* Now add the symbol files.  */
  while ((so = find_solib (so)) != NULL)
    {
      if (so->so_name[0] && re_exec (so->so_name))
	{
	  so->from_tty = from_tty;
	  if (so->symbols_loaded)
	    {
	      if (from_tty)
		{
		  printf_unfiltered ("Symbols already loaded for %s\n", so->so_name);
		}
	    }
	  else if (catch_errors
		   (symbol_add_stub, (char *) so,
		    "Error while reading shared library symbols:\n",
		    RETURN_MASK_ALL))
	    {
	      so_last = so;
	      so->symbols_loaded = 1;
	    }
	}
    }

  /* Getting new symbols may change our opinion about what is
     frameless.  */
  if (so_last)
    reinit_frame_cache ();
}

/*

   LOCAL FUNCTION

   info_sharedlibrary_command -- code for "info sharedlibrary"

   SYNOPSIS

   static void info_sharedlibrary_command ()

   DESCRIPTION

   Walk through the shared library list and print information
   about each attached library.
 */

static void
info_sharedlibrary_command (char *ignore, int from_tty)
{
  register struct so_list *so = NULL;	/* link map state variable */
  int header_done = 0;

  if (exec_bfd == NULL)
    {
      printf_unfiltered ("No executable file.\n");
      return;
    }
  while ((so = find_solib (so)) != NULL)
    {
      if (so->so_name[0])
	{
	  if (!header_done)
	    {
	      printf_unfiltered ("%-12s%-12s%-12s%s\n", "From", "To", "Syms Read",
				 "Shared Object Library");
	      header_done++;
	    }
	  printf_unfiltered ("%-12s",
		      local_hex_string_custom ((unsigned long) LM_ADDR (so),
					       "08l"));
	  printf_unfiltered ("%-12s",
			 local_hex_string_custom ((unsigned long) so->lmend,
						  "08l"));
	  printf_unfiltered ("%-12s", so->symbols_loaded ? "Yes" : "No");
	  printf_unfiltered ("%s\n", so->so_name);
	}
    }
  if (so_list_head == NULL)
    {
      printf_unfiltered ("No shared libraries loaded at this time.\n");
    }
}

/*

   GLOBAL FUNCTION

   solib_address -- check to see if an address is in a shared lib

   SYNOPSIS

   char *solib_address (CORE_ADDR address)

   DESCRIPTION

   Provides a hook for other gdb routines to discover whether or
   not a particular address is within the mapped address space of
   a shared library.  Any address between the base mapping address
   and the first address beyond the end of the last mapping, is
   considered to be within the shared library address space, for
   our purposes.

   For example, this routine is called at one point to disable
   breakpoints which are in shared libraries that are not currently
   mapped in.
 */

char *
solib_address (CORE_ADDR address)
{
  register struct so_list *so = 0;	/* link map state variable */

  while ((so = find_solib (so)) != NULL)
    {
      if (so->so_name[0])
	{
	  if ((address >= (CORE_ADDR) LM_ADDR (so)) &&
	      (address < (CORE_ADDR) so->lmend))
	    return (so->so_name);
	}
    }
  return (0);
}

/* Called by free_all_symtabs */

void
clear_solib (void)
{
  struct so_list *next;
  char *bfd_filename;

  disable_breakpoints_in_shlibs (1);

  while (so_list_head)
    {
      if (so_list_head->sections)
	{
	  xfree (so_list_head->sections);
	}
      if (so_list_head->abfd)
	{
	  remove_target_sections (so_list_head->abfd);
	  bfd_filename = bfd_get_filename (so_list_head->abfd);
	  if (!bfd_close (so_list_head->abfd))
	    warning ("cannot close \"%s\": %s",
		     bfd_filename, bfd_errmsg (bfd_get_error ()));
	}
      else
	/* This happens for the executable on SVR4.  */
	bfd_filename = NULL;

      next = so_list_head->next;
      if (bfd_filename)
	xfree (bfd_filename);
      xfree (so_list_head->so_name);
      xfree (so_list_head);
      so_list_head = next;
    }
  debug_base = 0;
}

/*

   LOCAL FUNCTION

   disable_break -- remove the "mapping changed" breakpoint

   SYNOPSIS

   static int disable_break ()

   DESCRIPTION

   Removes the breakpoint that gets hit when the dynamic linker
   completes a mapping change.

 */

static int
disable_break (void)
{
  int status = 1;


  /* Note that breakpoint address and original contents are in our address
     space, so we just need to write the original contents back. */

  if (memory_remove_breakpoint (breakpoint_addr, shadow_contents) != 0)
    {
      status = 0;
    }

  /* For the SVR4 version, we always know the breakpoint address.  For the
     SunOS version we don't know it until the above code is executed.
     Grumble if we are stopped anywhere besides the breakpoint address. */

  if (stop_pc != breakpoint_addr)
    {
      warning ("stopped at unknown breakpoint while handling shared libraries");
    }

  return (status);
}

/*

   LOCAL FUNCTION

   enable_break -- arrange for dynamic linker to hit breakpoint

   SYNOPSIS

   int enable_break (void)

   DESCRIPTION

   This functions inserts a breakpoint at the entry point of the
   main executable, where all shared libraries are mapped in.
 */

static int
enable_break (void)
{
  if (symfile_objfile != NULL
      && target_insert_breakpoint (symfile_objfile->ei.entry_point,
				   shadow_contents) == 0)
    {
      breakpoint_addr = symfile_objfile->ei.entry_point;
      return 1;
    }

  return 0;
}

/*

   GLOBAL FUNCTION

   solib_create_inferior_hook -- shared library startup support

   SYNOPSIS

   void solib_create_inferior_hook()

   DESCRIPTION

   When gdb starts up the inferior, it nurses it along (through the
   shell) until it is ready to execute it's first instruction.  At this
   point, this function gets called via expansion of the macro
   SOLIB_CREATE_INFERIOR_HOOK.

   For SunOS executables, this first instruction is typically the
   one at "_start", or a similar text label, regardless of whether
   the executable is statically or dynamically linked.  The runtime
   startup code takes care of dynamically linking in any shared
   libraries, once gdb allows the inferior to continue.

   For SVR4 executables, this first instruction is either the first
   instruction in the dynamic linker (for dynamically linked
   executables) or the instruction at "start" for statically linked
   executables.  For dynamically linked executables, the system
   first exec's /lib/libc.so.N, which contains the dynamic linker,
   and starts it running.  The dynamic linker maps in any needed
   shared libraries, maps in the actual user executable, and then
   jumps to "start" in the user executable.

   For both SunOS shared libraries, and SVR4 shared libraries, we
   can arrange to cooperate with the dynamic linker to discover the
   names of shared libraries that are dynamically linked, and the
   base addresses to which they are linked.

   This function is responsible for discovering those names and
   addresses, and saving sufficient information about them to allow
   their symbols to be read at a later time.

   FIXME

   Between enable_break() and disable_break(), this code does not
   properly handle hitting breakpoints which the user might have
   set in the startup code or in the dynamic linker itself.  Proper
   handling will probably have to wait until the implementation is
   changed to use the "breakpoint handler function" method.

   Also, what if child has exit()ed?  Must exit loop somehow.
 */

void
solib_create_inferior_hook (void)
{
  if (!enable_break ())
    {
      warning ("shared library handler failed to enable breakpoint");
      return;
    }

  /* Now run the target.  It will eventually hit the breakpoint, at
     which point all of the libraries will have been mapped in and we
     can go groveling around in the dynamic linker structures to find
     out what we need to know about them. */

  clear_proceed_status ();
  stop_soon_quietly = 1;
  stop_signal = TARGET_SIGNAL_0;
  do
    {
      target_resume (pid_to_ptid (-1), 0, stop_signal);
      wait_for_inferior ();
    }
  while (stop_signal != TARGET_SIGNAL_TRAP);

  /* We are now either at the "mapping complete" breakpoint (or somewhere
     else, a condition we aren't prepared to deal with anyway), so adjust
     the PC as necessary after a breakpoint, disable the breakpoint, and
     add any shared libraries that were mapped in. */

  if (DECR_PC_AFTER_BREAK)
    {
      stop_pc -= DECR_PC_AFTER_BREAK;
      write_register (PC_REGNUM, stop_pc);
    }

  if (!disable_break ())
    {
      warning ("shared library handler failed to disable breakpoint");
    }

  /*  solib_add will call reinit_frame_cache.
     But we are stopped in the startup code and we might not have symbols
     for the startup code, so heuristic_proc_start could be called
     and will put out an annoying warning.
     Delaying the resetting of stop_soon_quietly until after symbol loading
     suppresses the warning.  */
  solib_add ((char *) 0, 0, (struct target_ops *) 0, auto_solib_add);
  stop_soon_quietly = 0;
}

/*

   LOCAL FUNCTION

   sharedlibrary_command -- handle command to explicitly add library

   SYNOPSIS

   static void sharedlibrary_command (char *args, int from_tty)

   DESCRIPTION

 */

static void
sharedlibrary_command (char *args, int from_tty)
{
  dont_repeat ();
  solib_add (args, from_tty, (struct target_ops *) 0, 1);
}

void
_initialize_solib (void)
{
  add_com ("sharedlibrary", class_files, sharedlibrary_command,
	   "Load shared object library symbols for files matching REGEXP.");
  add_info ("sharedlibrary", info_sharedlibrary_command,
	    "Status of loaded shared object libraries.");

  add_show_from_set
    (add_set_cmd ("auto-solib-add", class_support, var_boolean,
		  (char *) &auto_solib_add,
		  "Set autoloading of shared library symbols.\n\
If \"on\", symbols from all shared object libraries will be loaded\n\
automatically when the inferior begins execution, when the dynamic linker\n\
informs gdb that a new library has been loaded, or when attaching to the\n\
inferior.  Otherwise, symbols must be loaded manually, using `sharedlibrary'.",
		  &setlist),
     &showlist);
}


/* Register that we are able to handle irix5 core file formats.
   This really is bfd_target_unknown_flavour */

static struct core_fns irix5_core_fns =
{
  bfd_target_unknown_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};

void
_initialize_core_irix5 (void)
{
  add_core_fns (&irix5_core_fns);
}
