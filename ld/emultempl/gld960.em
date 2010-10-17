# This shell script emits a C file. -*- C -*-
# It does some substitutions.
cat >e${EMULATION_NAME}.c <<EOF
/* Copyright 1991, 1992, 1994, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
 * emulate the Intels port of  gld
 */


#include "bfd.h"
#include "sysdep.h"
#include "libiberty.h"
#include "bfdlink.h"

#include "ld.h"
#include "ldmisc.h"
#include "ldmain.h"

#include "ldexp.h"
#include "ldlang.h"
#include "ldfile.h"
#include "ldemul.h"

#ifdef GNU960

static void
gld960_before_parse (void)
{
  static char *env_variables[] = { "G960LIB", "G960BASE", 0 };
  char **p;
  char *env ;

  for ( p = env_variables; *p; p++ ){
    env =  (char *) getenv(*p);
    if (env) {
      ldfile_add_library_path (concat (env,
				       "/lib/libbout",
				       (const char *) NULL),
			       FALSE);
    }
  }
  ldfile_output_architecture = bfd_arch_i960;
}

#else	/* not GNU960 */

static void gld960_before_parse (void)
{
  char *env ;
  env =  getenv("G960LIB");
  if (env) {
    ldfile_add_library_path(env, FALSE);
  }
  env = getenv("G960BASE");
  if (env)
    ldfile_add_library_path (concat (env, "/lib", (const char *) NULL), FALSE);
  ldfile_output_architecture = bfd_arch_i960;
}

#endif	/* GNU960 */


static void
gld960_set_output_arch (void)
{
  bfd_set_arch_mach(output_bfd, ldfile_output_architecture, bfd_mach_i960_core);
}

static char *
gld960_choose_target (int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
#ifdef GNU960

  output_filename = "b.out";
  return bfd_make_targ_name(BFD_BOUT_FORMAT, 0);

#else

  char *from_outside = getenv(TARGET_ENVIRON);
  output_filename = "b.out";

  if (from_outside != (char *)NULL)
    return from_outside;

  return "b.out.little";

#endif
}

static char *
gld960_get_script (int *isfile)
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

struct ld_emulation_xfer_struct ld_gld960_emulation =
{
  gld960_before_parse,
  syslib_default,
  hll_default,
  after_parse_default,
  after_open_default,
  after_allocation_default,
  gld960_set_output_arch,
  gld960_choose_target,
  before_allocation_default,
  gld960_get_script,
  "960",
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
