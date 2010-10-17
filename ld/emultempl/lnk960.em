# This shell script emits a C file. -*- C -*-
# It does some substitutions.
cat >e${EMULATION_NAME}.c <<EOF
/* intel coff loader emulation specific stuff
   Copyright 1991, 1992, 1994, 1995, 1996, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.
   Written by Steve Chamberlain steve@cygnus.com

This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "libiberty.h"
#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"

/*#include "archures.h"*/
#include "ld.h"
#include "ldmain.h"
#include "ldmisc.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldfile.h"
#include "ldemul.h"

typedef struct lib_list {
  char *name;
  struct lib_list *next;
} lib_list_type;

static lib_list_type *hll_list;
static lib_list_type **hll_list_tail = &hll_list;

static lib_list_type *syslib_list;
static lib_list_type **syslib_list_tail = &syslib_list;


static void
append (lib_list_type ***list, char *name)
{
  lib_list_type *element = (lib_list_type *) xmalloc (sizeof (lib_list_type));

  element->name = name;
  element->next = (lib_list_type *) NULL;
  **list = element;
  *list = &element->next;

}

static bfd_boolean had_hll = FALSE;
static bfd_boolean had_hll_name = FALSE;

static void
lnk960_hll (char *name)
{
  had_hll = TRUE;
  if (name != (char *) NULL)
    {
      had_hll_name = TRUE;
      append (&hll_list_tail, name);
    }
}

static void
lnk960_syslib (char *name)
{
  append (&syslib_list_tail, name);
}


#ifdef GNU960

static void
lnk960_before_parse (void)
{
  static char *env_variables[] = { "G960LIB", "G960BASE", 0 };
  char **p;
  char *env ;

  for (p = env_variables; *p; p++)
    {
      env = (char *) getenv (*p);
      if (env)
	ldfile_add_library_path (concat (env, "/lib/libcoff", ""), FALSE);
    }

  env = (char *) getenv ("I960BASE");
  if (env)
    ldfile_add_library_path(concat (env, "/lib", ""), FALSE);

  ldfile_output_architecture = bfd_arch_i960;
  ldfile_output_machine = bfd_mach_i960_core;
}

#else	/* not GNU960 */

static void
lnk960_before_parse (void)
{
  char *name = getenv ("I960BASE");

  if (name == (char *) NULL)
    {
      name = getenv("G960BASE");
      if (name == (char *) NULL)
	einfo ("%P%F I960BASE and G960BASE not set\n");
    }


  ldfile_add_library_path (concat (name, "/lib", ""), FALSE);
  ldfile_output_architecture = bfd_arch_i960;
  ldfile_output_machine = bfd_mach_i960_core;
}

#endif	/* GNU960 */


static void
add_on (lib_list_type *list, lang_input_file_enum_type search)
{
  while (list)
    {
      lang_add_input_file (list->name, search, (char *) NULL);
      list = list->next;
    }
}

static void
lnk960_after_parse (void)
{
  /* If there has been no arch, default to -KB */
  if (ldfile_output_machine_name[0] == 0)
    ldfile_add_arch ("KB");

  /* if there has been no hll list then add our own */

  if (had_hll && !had_hll_name)
    {
      append (&hll_list_tail, "cg");
      if (ldfile_output_machine == bfd_mach_i960_ka_sa
	  || ldfile_output_machine == bfd_mach_i960_ca)
	append (&hll_list_tail, "fpg");
    }

  add_on (hll_list, lang_input_file_is_l_enum);
  add_on (syslib_list, lang_input_file_is_search_file_enum);
}

static void
lnk960_before_allocation (void)
{
}

static void
lnk960_after_allocation (void)
{
  if (!link_info.relocatable)
    {
      lang_abs_symbol_at_end_of (".text", "_etext");
      lang_abs_symbol_at_end_of (".data", "_edata");
      lang_abs_symbol_at_beginning_of (".bss", "_bss_start");
      lang_abs_symbol_at_end_of (".bss", "_end");
    }
}


static struct
 {
   unsigned  long number;
   char *name;
 }
machine_table[] =
{
  { bfd_mach_i960_core	,"CORE" },
  { bfd_mach_i960_kb_sb	,"KB" },
  { bfd_mach_i960_kb_sb	,"SB" },
  { bfd_mach_i960_mc	,"MC" },
  { bfd_mach_i960_xa	,"XA" },
  { bfd_mach_i960_ca	,"CA" },
  { bfd_mach_i960_ka_sa	,"KA" },
  { bfd_mach_i960_ka_sa	,"SA" },
  { bfd_mach_i960_jx	,"JX" },
  { bfd_mach_i960_hx	,"HX" },

  { bfd_mach_i960_core	,"core" },
  { bfd_mach_i960_kb_sb	,"kb" },
  { bfd_mach_i960_kb_sb	,"sb" },
  { bfd_mach_i960_mc	,"mc" },
  { bfd_mach_i960_xa	,"xa" },
  { bfd_mach_i960_ca	,"ca" },
  { bfd_mach_i960_ka_sa	,"ka" },
  { bfd_mach_i960_ka_sa	,"sa" },
  { bfd_mach_i960_jx	,"jx" },
  { bfd_mach_i960_hx	,"hx" },

  { 0, (char *) NULL }
};

static void
lnk960_set_output_arch (void)
{
  /* Set the output architecture and machine if possible */
  unsigned int i;
  ldfile_output_machine = bfd_mach_i960_core;
  for (i= 0; machine_table[i].name != (char*) NULL; i++)
    {
      if (strcmp (ldfile_output_machine_name, machine_table[i].name) == 0)
	{
	  ldfile_output_machine = machine_table[i].number;
	  break;
	}
    }
  bfd_set_arch_mach (output_bfd, ldfile_output_architecture,
		     ldfile_output_machine);
}

static char *
lnk960_choose_target (int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
#ifdef GNU960

  return bfd_make_targ_name (BFD_COFF_FORMAT, 0);

#else

  char *from_outside = getenv (TARGET_ENVIRON);
  if (from_outside != (char *) NULL)
    return from_outside;
#ifdef LNK960_LITTLE
  return "coff-Intel-little";
#else
  return "coff-Intel-big";
#endif
#endif

}

static char *
lnk960_get_script (int *isfile)
EOF

if test -n "$COMPILE_IN"
then
# Scripts compiled in.

# sed commands to quote an ld script as a C string.
sc="-f stringify.sed"

cat >>e${EMULATION_NAME}.c <<EOF
{
  *isfile = 0;

  if (link_info.relocatable && config.build_constructors)
    return
EOF
sed $sc ldscripts/${EMULATION_NAME}.xu                 >> e${EMULATION_NAME}.c
echo '  ; else if (link_info.relocatable) return'     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xr                 >> e${EMULATION_NAME}.c
echo '  ; else if (!config.text_read_only) return'     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xbn                >> e${EMULATION_NAME}.c
echo '  ; else if (!config.magic_demand_paged) return' >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xn                 >> e${EMULATION_NAME}.c
echo '  ; else return'                                 >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.x                  >> e${EMULATION_NAME}.c
echo '; }'                                             >> e${EMULATION_NAME}.c

else
# Scripts read from the filesystem.

cat >>e${EMULATION_NAME}.c <<EOF
{
  *isfile = 1;

  if (link_info.relocatable && config.build_constructors)
    return "ldscripts/${EMULATION_NAME}.xu";
  else if (link_info.relocatable)
    return "ldscripts/${EMULATION_NAME}.xr";
  else if (!config.text_read_only)
    return "ldscripts/${EMULATION_NAME}.xbn";
  else if (!config.magic_demand_paged)
    return "ldscripts/${EMULATION_NAME}.xn";
  else
    return "ldscripts/${EMULATION_NAME}.x";
}
EOF

fi

cat >>e${EMULATION_NAME}.c <<EOF

struct ld_emulation_xfer_struct ld_lnk960_emulation =
{
  lnk960_before_parse,
  lnk960_syslib,
  lnk960_hll,
  lnk960_after_parse,
  NULL,			/* after_open */
  lnk960_after_allocation,
  lnk960_set_output_arch,
  lnk960_choose_target,
  lnk960_before_allocation,
  lnk960_get_script,
  "lnk960",
  "",
  NULL,	/* finish */
  NULL,	/* create output section statements */
  NULL,	/* open dynamic archive */
  NULL,	/* place orphan */
  NULL,	/* set symbols */
  NULL,	/* parse args */
  NULL,	/* add_options */
  NULL,	/* handle_option */
  NULL,	/* unrecognized file */
  NULL,	/* list options */
  NULL,	/* recognized file */
  NULL,	/* find_potential_libraries */
  NULL	/* new_vers_pattern */
};
EOF
