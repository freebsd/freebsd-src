/* Semi-dynamic architecture support for GDB, the GNU debugger.
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

#include "defs.h"
#include "bfd.h"
#include "gdbcmd.h"



/* Non-zero if we want to trace architecture code.  */

#ifndef GDBARCH_DEBUG
#define GDBARCH_DEBUG 0
#endif
int gdbarch_debug = GDBARCH_DEBUG;


/* Functions to manipulate the endianness of the target.  */

#ifdef TARGET_BYTE_ORDER_SELECTABLE
/* compat - Catch old targets that expect a selectable byte-order to
   default to BIG_ENDIAN */
#ifndef TARGET_BYTE_ORDER_DEFAULT
#define TARGET_BYTE_ORDER_DEFAULT BIG_ENDIAN
#endif
#endif
#ifndef TARGET_BYTE_ORDER_DEFAULT
/* compat - Catch old non byte-order selectable targets that do not
   define TARGET_BYTE_ORDER_DEFAULT and instead expect
   TARGET_BYTE_ORDER to be used as the default.  For targets that
   defined neither TARGET_BYTE_ORDER nor TARGET_BYTE_ORDER_DEFAULT the
   below will get a strange compiler warning. */
#define TARGET_BYTE_ORDER_DEFAULT TARGET_BYTE_ORDER
#endif
int target_byte_order = TARGET_BYTE_ORDER_DEFAULT;
int target_byte_order_auto = 1;

/* Chain containing the \"set endian\" commands.  */
static struct cmd_list_element *endianlist = NULL;

/* Called by ``show endian''.  */
static void show_endian PARAMS ((char *, int));
static void
show_endian (args, from_tty)
     char *args;
     int from_tty;
{
  char *msg =
    (TARGET_BYTE_ORDER_AUTO
     ? "The target endianness is set automatically (currently %s endian)\n"
     : "The target is assumed to be %s endian\n");
  printf_unfiltered (msg, (TARGET_BYTE_ORDER == BIG_ENDIAN ? "big" : "little"));
}

/* Called if the user enters ``set endian'' without an argument.  */
static void set_endian PARAMS ((char *, int));
static void
set_endian (args, from_tty)
     char *args;
     int from_tty;
{
  printf_unfiltered ("\"set endian\" must be followed by \"auto\", \"big\" or \"little\".\n");
  show_endian (args, from_tty);
}

/* Called by ``set endian big''.  */
static void set_endian_big PARAMS ((char *, int));
static void
set_endian_big (args, from_tty)
     char *args;
     int from_tty;
{
  if (TARGET_BYTE_ORDER_SELECTABLE_P)
    {
      target_byte_order = BIG_ENDIAN;
      target_byte_order_auto = 0;
    }
  else
    {
      printf_unfiltered ("Byte order is not selectable.");
      show_endian (args, from_tty);
    }
}

/* Called by ``set endian little''.  */
static void set_endian_little PARAMS ((char *, int));
static void
set_endian_little (args, from_tty)
     char *args;
     int from_tty;
{
  if (TARGET_BYTE_ORDER_SELECTABLE_P)
    {
      target_byte_order = LITTLE_ENDIAN;
      target_byte_order_auto = 0;
    }
  else
    {
      printf_unfiltered ("Byte order is not selectable.");
      show_endian (args, from_tty);
    }
}

/* Called by ``set endian auto''.  */
static void set_endian_auto PARAMS ((char *, int));
static void
set_endian_auto (args, from_tty)
     char *args;
     int from_tty;
{
  if (TARGET_BYTE_ORDER_SELECTABLE_P)
    {
      target_byte_order_auto = 1;
    }
  else
    {
      printf_unfiltered ("Byte order is not selectable.");
      show_endian (args, from_tty);
    }
}

/* Set the endianness from a BFD.  */
static void set_endian_from_file PARAMS ((bfd *));
static void
set_endian_from_file (abfd)
     bfd *abfd;
{
  if (TARGET_BYTE_ORDER_SELECTABLE_P)
    {
      int want;
      
      if (bfd_big_endian (abfd))
	want = BIG_ENDIAN;
      else
	want = LITTLE_ENDIAN;
      if (TARGET_BYTE_ORDER_AUTO)
	target_byte_order = want;
      else if (TARGET_BYTE_ORDER != want)
	warning ("%s endian file does not match %s endian target.",
		 want == BIG_ENDIAN ? "big" : "little",
		 TARGET_BYTE_ORDER == BIG_ENDIAN ? "big" : "little");
    }
  else
    {
      if (bfd_big_endian (abfd)
	  ? TARGET_BYTE_ORDER != BIG_ENDIAN
	  : TARGET_BYTE_ORDER == BIG_ENDIAN)
	warning ("%s endian file does not match %s endian target.",
		 bfd_big_endian (abfd) ? "big" : "little",
		 TARGET_BYTE_ORDER == BIG_ENDIAN ? "big" : "little");
    }
}



/* Functions to manipulate the architecture of the target */

int target_architecture_auto = 1;
extern const struct bfd_arch_info bfd_default_arch_struct;
const struct bfd_arch_info *target_architecture = &bfd_default_arch_struct;
int (*target_architecture_hook) PARAMS ((const struct bfd_arch_info *ap));

/* Do the real work of changing the current architecture */
static void
set_arch (arch)
     const struct bfd_arch_info *arch;
{
  /* FIXME: Is it compatible with gdb? */
  /* Check with the target on the setting */
  if (target_architecture_hook != NULL
      && !target_architecture_hook (arch))
    printf_unfiltered ("Target does not support `%s' architecture.\n",
		       arch->printable_name);
  else
    {
      target_architecture_auto = 0;
      target_architecture = arch;
    }
}

/* Called if the user enters ``show architecture'' without an argument. */
static void show_architecture PARAMS ((char *, int));
static void
show_architecture (args, from_tty)
     char *args;
     int from_tty;
{
  const char *arch;
  arch = TARGET_ARCHITECTURE->printable_name;
  if (target_architecture_auto)
    printf_filtered ("The target architecture is set automatically (currently %s)\n", arch);
  else
    printf_filtered ("The target architecture is assumed to be %s\n", arch);
}

/* Called if the user enters ``set architecture'' with or without an
   argument. */
static void set_architecture PARAMS ((char *, int));
static void
set_architecture (args, from_tty)
     char *args;
     int from_tty;
{
  if (args == NULL)
    {
      printf_unfiltered ("\"set architecture\" must be followed by \"auto\" or an architecture name.\n");
    }
  else if (strcmp (args, "auto") == 0)
    {
      target_architecture_auto = 1;
    }
  else
    {
      const struct bfd_arch_info *arch = bfd_scan_arch (args);
      if (arch != NULL)
	set_arch (arch);
      else
	printf_unfiltered ("Architecture `%s' not reconized.\n", args);
    }
}

/* Called if the user enters ``info architecture'' without an argument. */
static void info_architecture PARAMS ((char *, int));
static void
info_architecture (args, from_tty)
     char *args;
     int from_tty;
{
  enum bfd_architecture a;
  printf_filtered ("Available architectures are:\n");
  for (a = bfd_arch_obscure + 1; a < bfd_arch_last; a++)
    {
      const struct bfd_arch_info *ap = bfd_lookup_arch (a, 0);
      if (ap != NULL)
	{
	  do
	    {
	      printf_filtered (" %s", ap->printable_name);
	      ap = ap->next;
	    }
	  while (ap != NULL);
	  printf_filtered ("\n");
	}
    }
}

/* Set the architecture from arch/machine */
void
set_architecture_from_arch_mach (arch, mach)
     enum bfd_architecture arch;
     unsigned long mach;
{
  const struct bfd_arch_info *wanted = bfd_lookup_arch (arch, mach);
  if (wanted != NULL)
    set_arch (wanted);
  else
    fatal ("hardwired architecture/machine not reconized");
}

/* Set the architecture from a BFD */
static void set_architecture_from_file PARAMS ((bfd *));
static void
set_architecture_from_file (abfd)
     bfd *abfd;
{
  const struct bfd_arch_info *wanted = bfd_get_arch_info (abfd);
  if (target_architecture_auto)
    {
      if (target_architecture_hook != NULL
	  && !target_architecture_hook (wanted))
	warning ("Target may not support %s architecture",
		 wanted->printable_name);
      target_architecture = wanted;
    }
  else if (wanted != target_architecture)
    {
      warning ("%s architecture file may be incompatible with %s target.",
	       wanted->printable_name,
	       target_architecture->printable_name);
    }
}



/* Disassembler */

/* Pointer to the target-dependent disassembly function.  */
int (*tm_print_insn) PARAMS ((bfd_vma, disassemble_info *));
disassemble_info tm_print_insn_info;



/* Set the dynamic target-system-dependant parameters (architecture,
   byte-order) using information found in the BFD */

void
set_gdbarch_from_file (abfd)
     bfd *abfd;
{
  set_architecture_from_file (abfd);
  set_endian_from_file (abfd);
}


extern void _initialize_gdbarch PARAMS ((void));
void
_initialize_gdbarch ()
{
  add_prefix_cmd ("endian", class_support, set_endian,
		  "Set endianness of target.",
		  &endianlist, "set endian ", 0, &setlist);
  add_cmd ("big", class_support, set_endian_big,
	   "Set target as being big endian.", &endianlist);
  add_cmd ("little", class_support, set_endian_little,
	   "Set target as being little endian.", &endianlist);
  add_cmd ("auto", class_support, set_endian_auto,
	   "Select target endianness automatically.", &endianlist);
  add_cmd ("endian", class_support, show_endian,
	   "Show endianness of target.", &showlist);

  add_cmd ("architecture", class_support, set_architecture,
	   "Set architecture of target.", &setlist);
  add_alias_cmd ("processor", "architecture", class_support, 1, &setlist);
  add_cmd ("architecture", class_support, show_architecture,
	   "Show architecture of target.", &showlist);
  add_cmd ("architecture", class_support, info_architecture,
	   "List supported target architectures", &infolist);

  INIT_DISASSEMBLE_INFO_NO_ARCH (tm_print_insn_info, gdb_stdout, (fprintf_ftype)fprintf_filtered);
  tm_print_insn_info.flavour = bfd_target_unknown_flavour;
  tm_print_insn_info.read_memory_func = dis_asm_read_memory;
  tm_print_insn_info.memory_error_func = dis_asm_memory_error;
  tm_print_insn_info.print_address_func = dis_asm_print_address;

#ifdef MAINTENANCE_CMDS
  add_show_from_set (add_set_cmd ("archdebug",
				  class_maintenance,
				  var_zinteger,
				  (char *)&gdbarch_debug,
				  "Set architecture debugging.\n\
When non-zero, architecture debugging is enabled.", &setlist),
		     &showlist);
#endif
}
