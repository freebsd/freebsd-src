# This shell script emits a C file. -*- C -*-
# It does some substitutions.
(echo;echo;echo;echo)>e${EMULATION_NAME}.c # there, now line numbers match ;-)
cat >>e${EMULATION_NAME}.c <<EOF
/* This file is part of GLD, the Gnu Linker.
   Copyright 1999, 2000 Free Software Foundation, Inc.

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

/* For TI COFF */
/* Need to determine load and run pages for output sections */ 
  
#define TARGET_IS_${EMULATION_NAME}

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"

#include "ld.h"
#include "ldmain.h"
#include "ldmisc.h"

#include "ldexp.h"
#include "ldlang.h"
#include "ldfile.h"
#include "ldemul.h"

#include "getopt.h"

static int coff_version;

static void gld_${EMULATION_NAME}_before_parse PARAMS ((void));
static char *gld_${EMULATION_NAME}_get_script PARAMS ((int *));
static int gld_${EMULATION_NAME}_parse_args PARAMS ((int, char **));
static void gld_${EMULATION_NAME}_list_options PARAMS ((FILE *));

/* TI COFF extra command line options */
#define OPTION_COFF_FORMAT		(300 + 1)

static struct option longopts[] = 
{
  /* TI COFF options */
  {"format", required_argument, NULL, OPTION_COFF_FORMAT },
  {NULL, no_argument, NULL, 0}
};

static void
gld_${EMULATION_NAME}_list_options (file)
    FILE * file;
{
  fprintf (file, _("  --format 0|1|2         Specify which COFF version to use"));
}				  

static int
gld_${EMULATION_NAME}_parse_args(argc, argv)
     int argc;
     char **argv;
{
  int longind;
  int optc;
  int prevoptind = optind;
  int prevopterr = opterr;
  int wanterror;
  static int lastoptind = -1;

  if (lastoptind != optind)
    opterr = 0;
  wanterror = opterr;

  lastoptind = optind;

  optc = getopt_long_only (argc, argv, "-", longopts, &longind);
  opterr = prevopterr;

  switch (optc)
    {
    default:
      if (wanterror)
	xexit (1);
      optind =  prevoptind;
      return 0;

    case OPTION_COFF_FORMAT:
      if ((*optarg == '0' || *optarg == '1' || *optarg == '2')
          && optarg[1] == '\0')
      {
        extern void lang_add_output_format
          PARAMS ((const char *, const char *, const char *, int));
        static char buf[] = "coffX-${OUTPUT_FORMAT_TEMPLATE}";
        coff_version = *optarg - '0';
        buf[4] = *optarg;
	lang_add_output_format (buf, NULL, NULL, 0);	
      }
      else
        {
	  einfo (_("%P%F: invalid COFF format version %s\n"), optarg);

        }
      break;
    }
  return 1;
}

static void
gld_${EMULATION_NAME}_before_parse()
{
#ifndef TARGET_			/* I.e., if not generic.  */
  ldfile_set_output_arch ("`echo ${ARCH}`");
#endif /* not TARGET_ */
}

static char *
gld_${EMULATION_NAME}_get_script (isfile)
     int *isfile;
EOF
if test -n "$COMPILE_IN"
then
# Scripts compiled in.

# sed commands to quote an ld script as a C string.
sc='s/["\\]/\\&/g
s/$/\\n\\/
1s/^/"/
$s/$/n"/
'
cat >>e${EMULATION_NAME}.c <<EOF
{			     
  *isfile = 0;
  if (link_info.relocateable == true && config.build_constructors == true)
    return `sed "$sc" ldscripts/${EMULATION_NAME}.xu`;
  else if (link_info.relocateable == true)
    return `sed "$sc" ldscripts/${EMULATION_NAME}.xr`;
  else if (!config.text_read_only)
    return `sed "$sc" ldscripts/${EMULATION_NAME}.xbn`;
  else if (!config.magic_demand_paged)
    return `sed "$sc" ldscripts/${EMULATION_NAME}.xn`;
  else
    return `sed "$sc" ldscripts/${EMULATION_NAME}.x`;
}
EOF

else
# Scripts read from the filesystem.

cat >>e${EMULATION_NAME}.c <<EOF
{			     
  *isfile = 1;

  if (link_info.relocateable == true && config.build_constructors == true)
    return "ldscripts/${EMULATION_NAME}.xu";
  else if (link_info.relocateable == true)
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
struct ld_emulation_xfer_struct ld_${EMULATION_NAME}_emulation = 
{
  gld_${EMULATION_NAME}_before_parse,
  syslib_default,
  hll_default,
  after_parse_default,
  after_open_default,
  after_allocation_default,
  set_output_arch_default,
  ldemul_default_target,
  before_allocation_default,
  gld_${EMULATION_NAME}_get_script,
  "${EMULATION_NAME}",
  "${OUTPUT_FORMAT}",
  NULL, /* finish */
  NULL, /* create output section statements */
  NULL, /* open dynamic archive */
  NULL, /* place orphan */
  NULL, /* set_symbols */
  gld_${EMULATION_NAME}_parse_args,
  NULL, /* unrecognized_file */
  gld_${EMULATION_NAME}_list_options,
  NULL, /* recognized file */
  NULL 	/* find_potential_libraries */
};
EOF
