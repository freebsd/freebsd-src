# This shell script emits a C file. -*- C -*-
#   Copyright 1991, 1993, 1996, 1997, 1998, 1999, 2000
#   Free Software Foundation, Inc.
#
# This file is part of GLD, the Gnu Linker.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

# This file is sourced from elf32.em, and defines extra arm-elf
# specific routines.
#
cat >>e${EMULATION_NAME}.c <<EOF

static int no_pipeline_knowledge = 0;
static char *thumb_entry_symbol = NULL;


static void
gld${EMULATION_NAME}_before_parse ()
{
#ifndef TARGET_			/* I.e., if not generic.  */
  ldfile_set_output_arch ("`echo ${ARCH}`");
#endif /* not TARGET_ */
  config.dynamic_link = ${DYNAMIC_LINK-true};
  config.has_shared = `if test -n "$GENERATE_SHLIB_SCRIPT" ; then echo true ; else echo false ; fi`;
}


static void arm_elf_after_open PARAMS((void));

static void
arm_elf_after_open ()
{
  if (strstr (bfd_get_target (output_bfd), "arm") == NULL)
    {
      /* The arm backend needs special fields in the output hash structure.
	 These will only be created if the output format is an arm format,
	 hence we do not support linking and changing output formats at the
	 same time.  Use a link followed by objcopy to change output formats.  */
      einfo ("%F%X%P: error: cannot change output format whilst linking ARM binaries\n");
      return;
    }

  {
    LANG_FOR_EACH_INPUT_STATEMENT (is)
      {
	/* The interworking bfd must be the last one to be processed */
	if (!is->next)
	  bfd_elf32_arm_get_bfd_for_interworking (is->the_bfd, & link_info);
      }
  }

  /* Call the standard elf routine.  */
  gld${EMULATION_NAME}_after_open ();
}


static void arm_elf_before_allocation PARAMS ((void));

static void
arm_elf_before_allocation ()
{
  /* Call the standard elf routine.  */
  gld${EMULATION_NAME}_before_allocation ();

  /* We should be able to set the size of the interworking stub section */

  /* Here we rummage through the found bfds to collect glue information */
  /* FIXME: should this be based on a command line option? krk@cygnus.com */
  {
    LANG_FOR_EACH_INPUT_STATEMENT (is)
      {
	if (!bfd_elf32_arm_process_before_allocation (is->the_bfd, & link_info,
						      no_pipeline_knowledge))
	  {
	    /* xgettext:c-format */
	    einfo (_("Errors encountered processing file %s"), is->filename);
	  }
      }
  }

  /* We have seen it all. Allocate it, and carry on */
  bfd_elf32_arm_allocate_interworking_sections (& link_info);
}


static void gld${EMULATION_NAME}_finish PARAMS ((void));

static void
gld${EMULATION_NAME}_finish PARAMS((void))
{
  struct bfd_link_hash_entry * h;

  if (thumb_entry_symbol == NULL)
    return;
  
  h = bfd_link_hash_lookup (link_info.hash, thumb_entry_symbol,
			    false, false, true);

  if (h != (struct bfd_link_hash_entry *) NULL
      && (h->type == bfd_link_hash_defined
	  || h->type == bfd_link_hash_defweak)
      && h->u.def.section->output_section != NULL)
    {
      static char buffer[32];
      bfd_vma val;
      
      /* Special procesing is required for a Thumb entry symbol.  The
	 bottom bit of its address must be set.  */
      val = (h->u.def.value
	     + bfd_get_section_vma (output_bfd,
				    h->u.def.section->output_section)
	     + h->u.def.section->output_offset);
      
      val |= 1;

      /* Now convert this value into a string and store it in entry_symbol
         where the lang_finish() function will pick it up.  */
      buffer[0] = '0';
      buffer[1] = 'x';
      
      sprintf_vma (buffer + 2, val);

      if (entry_symbol != NULL && entry_from_cmdline)
	einfo (_("%P: warning: '--thumb-entry %s' is overriding '-e %s'\n"),
	       thumb_entry_symbol, entry_symbol);
      entry_symbol = buffer;
    }
  else
    einfo (_("%P: warning: connot find thumb start symbol %s\n"),
	   thumb_entry_symbol);
}

EOF

# Define some shell vars to insert bits of code into the standard elf
# parse_args and list_options functions.
#
PARSE_AND_LIST_PROLOGUE='
#define OPTION_THUMB_ENTRY		301
'

PARSE_AND_LIST_SHORTOPTS=p

PARSE_AND_LIST_LONGOPTS='
  { "no-pipeline-knowledge", no_argument, NULL, '\'p\''},
  { "thumb-entry", required_argument, NULL, OPTION_THUMB_ENTRY},
'

PARSE_AND_LIST_OPTIONS='
  fprintf (file, _("  -p --no-pipeline-knowledge  Stop the linker knowing about the pipeline length\n"));
  fprintf (file, _("     --thumb-entry=<sym>      Set the entry point to be Thumb symbol <sym>\n"));
'

PARSE_AND_LIST_ARGS_CASES='
    case '\'p\'':
      no_pipeline_knowledge = 1;
      break;

    case OPTION_THUMB_ENTRY:
      thumb_entry_symbol = optarg;
      break;
'

# We have our own after_open and before_allocation functions, but they call
# the standard routines, so give them a different name.
LDEMUL_AFTER_OPEN=arm_elf_after_open
LDEMUL_BEFORE_ALLOCATION=arm_elf_before_allocation

# Replace the elf before_parse function with our own.
LDEMUL_BEFORE_PARSE=gld"${EMULATION_NAME}"_before_parse

# Call the extra arm-elf function
LDEMUL_FINISH=gld${EMULATION_NAME}_finish
