/* Native-dependent code for NetBSD/i386, for GDB.
   Copyright 1988, 1989, 1991, 1992, 1994, 1996, 2000, 2001, 2002
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
#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"

#ifndef HAVE_GREGSET_T
typedef struct reg gregset_t;
#endif

#ifndef HAVE_FPREGSET_T
typedef struct fpreg fpregset_t;
#endif

#include "gregset.h"

/* Prototypes for i387_supply_fsave etc.  */
#include "i387-nat.h"  

struct md_core
{
  struct reg intreg;
  char freg[108];
};

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size, int which,
		      CORE_ADDR ignore)
{
  struct md_core *core_reg = (struct md_core *) core_reg_sect;

  /* We get everything from one section.  */
  if (which != 0)
    return;

  /* Integer registers.  */
  supply_gregset (&core_reg->intreg);

  /* Floating point registers.  */
  i387_supply_fsave (core_reg->freg);
}

static void
fetch_elfcore_registers (char *core_reg_sect, unsigned core_reg_size, int which,
                         CORE_ADDR ignore)
{
  gregset_t gregset;

  switch (which)
    {
    case 0:  /* Integer registers.  */
      if (core_reg_size != sizeof (struct reg))
	warning ("Wrong size register set in core file.");
      else
	{
	  memcpy (&gregset, core_reg_sect, sizeof (gregset));
	  supply_gregset (&gregset);
	}
      break;

    case 2:  /* Floating point registers.  */
      if (core_reg_size != 108)
	warning ("Wrong size FP register set in core file.");
      else
	i387_supply_fsave (core_reg_sect);
      break;

    case 3:  /* "Extended" floating point registers.  This is gdb-speak
		for SSE/SSE2. */
      if (core_reg_size != 512)
	warning ("Wrong size XMM register set in core file.");
      else
	i387_supply_fxsave (core_reg_sect);
      break;

    default:
      /* Don't know what kind of register request this is; just ignore it.  */
      break;
    }
}

/* Register that we are able to handle i386nbsd core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */

static struct core_fns i386nbsd_core_fns =
{
  bfd_target_unknown_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};

static struct core_fns i386nbsd_elfcore_fns =
{
  bfd_target_elf_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_elfcore_registers,		/* core_read_registers */
  NULL					/* next */
};

void
_initialize_i386nbsd_nat (void)
{
  add_core_fns (&i386nbsd_core_fns);
  add_core_fns (&i386nbsd_elfcore_fns);
}
