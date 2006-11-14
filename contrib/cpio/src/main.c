/* $FreeBSD$ */

/* main.c - main program and argument processing for cpio.
   Copyright (C) 1990, 1991, 1992, 2001, 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Phil Nelson <phil@cs.wwu.edu>,
   David MacKenzie <djm@gnu.ai.mit.edu>,
   John Oleynick <juo@klinzhai.rutgers.edu>,
   and Sergey Poznyakoff <gray@mirddin.farlep.net> */

#include <system.h>

#include <stdio.h>
#include <getopt.h>
#include <argp.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_LOCALE_H
#  include <locale.h>
#endif

#include "filetypes.h"
#include "cpiohdr.h"
#include "dstring.h"
#include "extern.h"
#include <rmt.h>
#include <localedir.h>

enum cpio_options {
  NO_ABSOLUTE_FILENAMES_OPTION=256,  
  ABSOLUTE_FILENAMES_OPTION,  
  NO_PRESERVE_OWNER_OPTION,      
  ONLY_VERIFY_CRC_OPTION,        
  RENAME_BATCH_FILE_OPTION,      
  RSH_COMMAND_OPTION,            
  QUIET_OPTION,                  
  SPARSE_OPTION,                 
  FORCE_LOCAL_OPTION,            
  DEBUG_OPTION,                  
  BLOCK_SIZE_OPTION,             
  TO_STDOUT_OPTION,              

  USAGE_OPTION,               
  LICENSE_OPTION,             
  VERSION_OPTION
};

const char *argp_program_version = "cpio (" PACKAGE_NAME ") " VERSION;
const char *argp_program_bug_address = "<" PACKAGE_BUGREPORT ">";
static char doc[] = N_("GNU `cpio' copies files to and from archives\n\
\n\
Examples:\n\
  # Copy files named in name-list to the archive\n\
  cpio -o < name-list [> archive]\n\
  # Extract files from the archive\n\
  cpio -i [< archive]\n\
  # Copy files named in name-list to destination-directory\n\
  cpio -p destination-directory < name-list\n");

/*  Print usage error message and exit with error.  */

#define USAGE_ERROR(args) do { error args; exit(2); } while (0)
#define CHECK_USAGE(cond, opt, mode_opt) \
 if (cond) USAGE_ERROR((0, 0, _("%s is meaningless with %s"), opt, mode_opt));

static struct argp_option options[] = {
  {NULL, 0, NULL, 0,
   N_("Main operation mode:"), 10},
  {"create", 'o', 0, 0,
   N_("Create the archive (run in copy-out mode)"), 10},
  {"extract", 'i', 0, 0,
   N_("Extract files from an archive (run in copy-in mode)")},
  {"pass-through", 'p', 0, 0,
   N_("Run in copy-pass mode"), 10},
  {"list", 't', 0, 0,
   N_("Print a table of contents of the input"), 10},

  {NULL, 0, NULL, 0,
   N_("Operation modifiers valid in any mode:"), 100},

  {"file", 'F', N_("[[USER@]HOST:]FILE-NAME"), 0,
   N_("Use this FILE-NAME instead of standard input or output. Optional USER and HOST specify the user and host names in case of a remote archive"), 110},
  {"force-local", FORCE_LOCAL_OPTION, 0, 0,
   N_("Archive file is local, even if its name contains colons"), 110},
  {"format", 'H', N_("FORMAT"), 0,
   N_("Use given archive FORMAT"), 110},
  {NULL, 'B', NULL, 0,
   N_("Set the I/O block size to 5120 bytes"), 110},
  {"block-size", BLOCK_SIZE_OPTION, N_("BLOCK-SIZE"), 0,
   N_("Set the I/O block size to BLOCK-SIZE * 512 bytes"), 110},
  {NULL, 'c', NULL, 0,
   N_("Use the old portable (ASCII) archive format"), 0},
  {"dot", 'V', NULL, 0, 
   N_("Print a \".\" for each file processed"), 110},
  {"io-size", 'C', N_("NUMBER"), 0,
   N_("Set the I/O block size to the given NUMBER of bytes"), 110},
  {"message", 'M', N_("STRING"), 0,
   N_("Print STRING when the end of a volume of the backup media is reached"),
   110},
  {"nonmatching", 'f', 0, 0,
   N_("Only copy files that do not match any of the given patterns"), 110},
  {"numeric-uid-gid", 'n', 0, 0,
   N_("In the verbose table of contents listing, show numeric UID and GID"),
   110},
  {"rsh-command", RSH_COMMAND_OPTION, N_("COMMAND"), 0,
   N_("Use remote COMMAND instead of rsh"), 110},
  {"quiet", QUIET_OPTION, NULL, 0,
   N_("Do not print the number of blocks copied"), 110},
  {"verbose", 'v', NULL, 0,
   N_("Verbosely list the files processed"), 110},
#ifdef DEBUG_CPIO
  {"debug", DEBUG_OPTION, NULL, 0,
   N_("Enable debugging info"), 110},
#endif
  {"warning", 'W', N_("FLAG"), 0,
   N_("Control warning display. Currently FLAG is one of 'none', 'truncate', 'all'. Multiple options accumulate."), 110 },

  /* ********** */
  {NULL, 0, NULL, 0,
   N_("Operation modifiers valid only in copy-in mode:"), 200},
  {"pattern-file", 'E', N_("FILE"), 0,
   N_("In copy-in mode, read additional patterns specifying filenames to extract or list from FILE"), 210},
  {"no-absolute-filenames", NO_ABSOLUTE_FILENAMES_OPTION, 0, 0,
   N_("Create all files relative to the current directory"), 210},
  {"absolute-filenames", ABSOLUTE_FILENAMES_OPTION, 0, 0,
   N_("do not strip leading file name components that contain \"..\" and leading slashes from file names"), 210},
  {"only-verify-crc", ONLY_VERIFY_CRC_OPTION, 0, 0,
   N_("When reading a CRC format archive in copy-in mode, only verify the CRC's of each file in the archive, don't actually extract the files"), 210},
  {"rename", 'r', 0, 0,
   N_("Interactively rename files"), 210},
  {"rename-batch-file", RENAME_BATCH_FILE_OPTION, N_("FILE"), OPTION_HIDDEN,
   "", 210},
  {"swap", 'b', NULL, 0,
   N_("Swap both halfwords of words and bytes of halfwords in the data. Equivalent to -sS"), 210},
  {"swap-bytes", 's', NULL, 0,
   N_("Swap the bytes of each halfword in the files"), 210},
  {"swap-halfwords", 'S', NULL, 0,
   N_("Swap the halfwords of each word (4 bytes) in the files"),
   210},
  {"to-stdout", TO_STDOUT_OPTION, NULL, 0,
   N_("Extract files to standard output"), 210},

  /* ********** */
  {NULL, 0, NULL, 0,
   N_("Operation modifiers valid only in copy-out mode:"), 300},
  {"append", 'A', 0, 0,
   N_("Append to an existing archive."), 310 },
  {NULL, 'O', N_("[[USER@]HOST:]FILE-NAME"), 0,
   N_("Archive filename to use instead of standard output. Optional USER and HOST specify the user and host names in case of a remote archive"), 310},
  
  /* ********** */
  {NULL, 0, NULL, 0,
   N_("Operation modifiers valid only in copy-pass mode:"), 400},
  {"link", 'l', 0, 0,
   N_("Link files instead of copying them, when  possible"), 410},

  /* ********** */
  {NULL, 0, NULL, 0,
   N_("Operation modifiers valid for copy-out and copy-pass modes:"), 500},
  {"null", '0', 0, 0,
   N_("A list of filenames is terminated by a null character instead of a newline"), 510 },
  {NULL, 'I', N_("[[USER@]HOST:]FILE-NAME"), 0,
   N_("Archive filename to use instead of standard input. Optional USER and HOST specify the user and host names in case of a remote archive"), 510},
  {"dereference", 'L', 0, 0,
   N_("Dereference  symbolic  links  (copy  the files that they point to instead of copying the links)."), 510},
  {"owner", 'R', N_("[USER][:.][GROUP]"), 0,
   N_("Set the ownership of all files created to the specified USER and/or GROUP"), 510},
  {"sparse", SPARSE_OPTION, NULL, 0,
   N_("Write files with large blocks of zeros as sparse files"), 510},
  {"reset-access-time", 'a', NULL, 0,
   N_("Reset the access times of files after reading them"), 510},

  /* ********** */
  {NULL, 0, NULL, 0,
   N_("Operation modifiers valid for copy-in and copy-pass modes:"), 600},
  {"preserve-modification-time", 'm', 0, 0,
   N_("Retain previous file modification times when creating files"), 610},
  {"make-directories", 'd', 0, 0,
   N_("Create leading directories where needed"), 610},
  {"no-preserve-owner", NO_PRESERVE_OWNER_OPTION, 0, 0,
   N_("Do not change the ownership of the files"), 610},
  {"unconditional", 'u', NULL, 0,
   N_("Replace all files unconditionally"), 610},

  {NULL, 0, NULL, 0,
   N_("Informative options:"), 700 },

  {"help",  '?', 0, 0,  N_("Give this help list"), -1},
  {"usage", USAGE_OPTION, 0, 0,  N_("Give a short usage message"), -1},
  {"license", LICENSE_OPTION, 0, 0, N_("Print license and exit"), -1},
  {"version", VERSION_OPTION, 0, 0,  N_("Print program version"), -1},
  /* FIXME -V (--dot) conflicts with the default short option for
     --version */
  
  {0, 0, 0, 0}
};

static char *input_archive_name = 0;
static char *output_archive_name = 0;

static void
license ()
{
  printf ("%s (%s) %s\n%s\n", program_name, PACKAGE_NAME, PACKAGE_VERSION,
	  "Copyright (C) 2004 Free Software Foundation, Inc.\n");
  printf (_("   GNU cpio is free software; you can redistribute it and/or modify\n"
    "   it under the terms of the GNU General Public License as published by\n"
    "   the Free Software Foundation; either version 2 of the License, or\n"
    "   (at your option) any later version.\n"
    "\n"
    "   GNU cpio is distributed in the hope that it will be useful,\n"
    "   but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "   GNU General Public License for more details.\n"
    "\n"
    "   You should have received a copy of the GNU General Public License\n"
    "   along with GNU cpio; if not, write to the Free Software\n"
    "   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA\n\n"));
  exit (0);
}

static int
warn_control (char *arg)
{
  static struct warn_tab {
    char *name;
    int flag;
  } warn_tab[] = {
    { "none",       CPIO_WARN_ALL       },
    { "truncate",   CPIO_WARN_TRUNCATE  },
    { "all",        CPIO_WARN_ALL       },
    { NULL }
  };
  struct warn_tab *wt;
  int offset = 0;

  if (strcmp (arg, "none") == 0)
    {
      warn_option = 0;
      return 0;
    }
  
  if (strlen (arg) > 2 && memcmp (arg, "no-", 3) == 0)
    offset = 3;
      
  for (wt = warn_tab; wt->name; wt++)
    if (strcmp (arg + offset, wt->name) == 0)
      {
	if (offset)
	  warn_option &= ~wt->flag;
	else
	  warn_option |= wt->flag;
	return 0;
      }

  return 1;
}

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case '0':		/* Read null-terminated filenames.  */
      name_end = '\0';
      break;

    case 'a':		/* Reset access times.  */
      reset_time_flag = true;
      break;

    case 'A':		/* Append to the archive.  */
      append_flag = true;
      break;

    case 'b':		/* Swap bytes and halfwords.  */
      swap_bytes_flag = true;
      swap_halfwords_flag = true;
      break;

    case 'B':		/* Set block size to 5120.  */
      io_block_size = 5120;
      break;

    case BLOCK_SIZE_OPTION:		/* --block-size */
      io_block_size = atoi (arg);
      if (io_block_size < 1)
	error (2, 0, _("invalid block size"));
      io_block_size *= 512;
      break;

    case 'c':		/* Use the old portable ASCII format.  */
      if (archive_format != arf_unknown)
	USAGE_ERROR ((0, 0, _("Archive format multiply defined")));
#ifdef SVR4_COMPAT
      archive_format = arf_newascii; /* -H newc.  */
#else
      archive_format = arf_oldascii; /* -H odc.  */
#endif
      break;

    case 'C':		/* Block size.  */
      io_block_size = atoi (arg);
      if (io_block_size < 1)
	error (2, 0, _("invalid block size"));
      break;

    case 'd':		/* Create directories where needed.  */
      create_dir_flag = true;
      break;

    case 'f':		/* Only copy files not matching patterns.  */
      copy_matching_files = false;
      break;

    case 'E':		/* Pattern file name.  */
      pattern_file_name = arg;
      break;

    case 'F':		/* Archive file name.  */
      archive_name = arg;
      break;

    case 'H':		/* Header format name.  */
      if (archive_format != arf_unknown)
	USAGE_ERROR ((0, 0, _("Archive format multiply defined")));
      if (!strcasecmp (arg, "crc"))
	archive_format = arf_crcascii;
      else if (!strcasecmp (arg, "newc"))
	archive_format = arf_newascii;
      else if (!strcasecmp (arg, "odc"))
	archive_format = arf_oldascii;
      else if (!strcasecmp (arg, "bin"))
	archive_format = arf_binary;
      else if (!strcasecmp (arg, "ustar"))
	archive_format = arf_ustar;
      else if (!strcasecmp (arg, "tar"))
	archive_format = arf_tar;
      else if (!strcasecmp (arg, "hpodc"))
	archive_format = arf_hpoldascii;
      else if (!strcasecmp (arg, "hpbin"))
	archive_format = arf_hpbinary;
      else
	error (2, 0, _("\
invalid archive format `%s'; valid formats are:\n\
crc newc odc bin ustar tar (all-caps also recognized)"), arg);
      break;
	  
    case 'i':		/* Copy-in mode.  */
      if (copy_function != 0)
	USAGE_ERROR ((0, 0, _("Mode already defined")));
      copy_function = process_copy_in;
      break;

    case 'I':		/* Input archive file name.  */
      input_archive_name = arg;
      break;

    case 'k':		/* Handle corrupted archives.  We always handle
			   corrupted archives, but recognize this
			   option for compatability.  */
      break;

    case 'l':		/* Link files when possible.  */
      link_flag = true;
      break;

    case 'L':		/* Dereference symbolic links.  */
      xstat = stat;
      break;

    case 'm':		/* Retain previous file modify times.  */
      retain_time_flag = true;
      break;

    case 'M':		/* New media message.  */
      set_new_media_message (arg);
      break;

    case 'n':		/* Long list owner and group as numbers.  */
      numeric_uid = true;
      break;

    case NO_ABSOLUTE_FILENAMES_OPTION:		/* --no-absolute-filenames */
      abs_paths_flag = false;
      break;
	
    case ABSOLUTE_FILENAMES_OPTION:		/* --absolute-filenames */
      abs_paths_flag = true;
      break;
	
    case NO_PRESERVE_OWNER_OPTION:		/* --no-preserve-owner */
      if (set_owner_flag || set_group_flag)
	USAGE_ERROR ((0, 0,
	             _("--no-preserve-owner cannot be used with --owner")));
      no_chown_flag = true;
      break;

    case 'o':		/* Copy-out mode.  */
      if (copy_function != 0)
	USAGE_ERROR ((0, 0, _("Mode already defined")));
      copy_function = process_copy_out;
      break;

    case 'O':		/* Output archive file name.  */
      output_archive_name = arg;
      break;

    case ONLY_VERIFY_CRC_OPTION:
      only_verify_crc_flag = true;
      break;

    case 'p':		/* Copy-pass mode.  */
      if (copy_function != 0)
	USAGE_ERROR ((0, 0, _("Mode already defined")));
      copy_function = process_copy_pass;
      break;
	  
    case RSH_COMMAND_OPTION:
      rsh_command_option = arg;
      break;

    case 'r':		/* Interactively rename.  */
      rename_flag = true;
      break;

    case RENAME_BATCH_FILE_OPTION:
      rename_batch_file = arg;
      break;

    case QUIET_OPTION:
      quiet_flag = true;
      break;

    case 'R':		/* Set the owner.  */
      if (no_chown_flag)
	USAGE_ERROR ((0, 0,
		      _("--owner cannot be used with --no-preserve-owner")));
      {
	char *e, *u, *g;

	e = parse_user_spec (arg, &set_owner, &set_group, &u, &g);
	if (e)
	  error (2, 0, "%s: %s", arg, e);
	if (u)
	  {
	    free (u);
	    set_owner_flag = true;
	  }
	if (g)
	  {
	    free (g);
	    set_group_flag = true;
	  }
      }
      break;

    case 's':		/* Swap bytes.  */
      swap_bytes_flag = true;
      break;

    case 'S':		/* Swap halfwords.  */
      swap_halfwords_flag = true;
      break;

    case 't':		/* Only print a list.  */
      table_flag = true;
      break;

    case 'u':		/* Replace all!  Unconditionally!  */
      unconditional_flag = true;
      break;

    case 'v':		/* Verbose!  */
      verbose_flag = true;
      break;

    case 'V':		/* Print `.' for each file.  */
      dot_flag = true;
      break;

    case 'W':
      if (warn_control (arg))
	argp_error (state, _("Invalid value for --warning option: %s"), arg);
      break;
      
    case SPARSE_OPTION:
      sparse_flag = true;
      break;

    case FORCE_LOCAL_OPTION:
      force_local_option = 1;
      break;

#ifdef DEBUG_CPIO
    case DEBUG_OPTION:
      debug_flag = true;
      break;
#endif

    case TO_STDOUT_OPTION:
      to_stdout_option = true;
      break;
      
    case '?':
      argp_state_help (state, state->out_stream, ARGP_HELP_STD_HELP);
      break;

    case USAGE_OPTION:
      argp_state_help (state, state->out_stream,
		       ARGP_HELP_USAGE | ARGP_HELP_EXIT_OK);
      break;

    case VERSION_OPTION:
      fprintf (state->out_stream, "%s\n", argp_program_version);
      exit (0);

    case LICENSE_OPTION:
      license ();
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp argp = {
  options,
  parse_opt,
  N_("[destination-directory]"),
  doc,
  NULL,
  NULL,
  NULL
};

/* Process the arguments.  Set all options and set up the copy pass
   directory or the copy in patterns.  */

void
process_args (int argc, char *argv[])
{
  void (*copy_in) ();		/* Work around for pcc bug.  */
  void (*copy_out) ();
  int index;

  if (argc < 2)
    USAGE_ERROR ((0, 0,
		  _("You must specify one of -oipt options.\nTry `%s --help' or `%s --usage' for more information.\n"),
		  program_name, program_name));

  xstat = lstat;

  if (argp_parse (&argp, argc, argv, ARGP_IN_ORDER|ARGP_NO_HELP, &index, NULL))
    exit (1); 

  /* Do error checking and look at other args.  */

  if (copy_function == 0)
    {
      if (table_flag)
	copy_function = process_copy_in;
      else
	USAGE_ERROR ((0, 0,
		      _("You must specify one of -oipt options.\nTry `%s --help' or `%s --usage' for more information.\n"),
		      program_name, program_name));
    }

  /* Work around for pcc bug.  */
  copy_in = process_copy_in;
  copy_out = process_copy_out;

  if (copy_function == copy_in)
    {
      archive_des = 0;
      CHECK_USAGE(link_flag, "--link", "--extract");
      CHECK_USAGE(reset_time_flag, "--reset", "--extract");
      CHECK_USAGE(xstat != lstat, "--dereference", "--extract");
      CHECK_USAGE(append_flag, "--append", "--extract");
      CHECK_USAGE(sparse_flag, "--sparse", "--extract");
      CHECK_USAGE(output_archive_name, "-O", "--extract");
      if (to_stdout_option)
	{
	  CHECK_USAGE(create_dir_flag, "--make-directories", "--to-stdout");
	  CHECK_USAGE(rename_flag, "--rename", "--to-stdout");
	  CHECK_USAGE(no_chown_flag, "--no-preserve-owner", "--to-stdout");
	  CHECK_USAGE(set_owner_flag||set_group_flag, "--owner", "--to-stdout");
	  CHECK_USAGE(retain_time_flag, "--preserve-modification-time",
		      "--to-stdout");
	}
      
      if (archive_name && input_archive_name)
	USAGE_ERROR((0, 0, _("Both -I and -F are used in copy-in mode")));

      if (archive_format == arf_crcascii)
	crc_i_flag = true;
      num_patterns = argc - index;
      save_patterns = &argv[index];
      if (input_archive_name)
	archive_name = input_archive_name;
    }
  else if (copy_function == copy_out)
    {
      if (index != argc)
	USAGE_ERROR ((0, 0, _("Too many arguments")));

      archive_des = 1;
      CHECK_USAGE(create_dir_flag, "--make-directories", "--create");
      CHECK_USAGE(rename_flag, "--rename", "--create");
      CHECK_USAGE(table_flag, "--list", "--create");
      CHECK_USAGE(unconditional_flag, "--unconditional", "--create");
      CHECK_USAGE(link_flag, "--link", "--create");
      CHECK_USAGE(retain_time_flag, "--preserve-modification-time",
		  "--create");
      CHECK_USAGE(no_chown_flag, "--no-preserve-owner", "--create");
      CHECK_USAGE(set_owner_flag||set_group_flag, "--owner", "--create");
      CHECK_USAGE(swap_bytes_flag, "--swap-bytes (--swap)", "--create");
      CHECK_USAGE(swap_halfwords_flag, "--swap-halfwords (--swap)",
		  "--create");
      CHECK_USAGE(to_stdout_option, "--to-stdout", "--create");
      
      if (append_flag && !(archive_name || output_archive_name))
	USAGE_ERROR ((0, 0,
		      _("--append is used but no archive file name is given (use -F or -O options")));

      CHECK_USAGE(rename_batch_file, "--rename-batch-file", "--create");
      CHECK_USAGE(abs_paths_flag, "--absolute-pathnames", "--create");
      CHECK_USAGE(input_archive_name, "-I", "--create");
      if (archive_name && output_archive_name)
	USAGE_ERROR ((0, 0, _("Both -O and -F are used in copy-out mode")));

      if (archive_format == arf_unknown)
	archive_format = arf_binary;
      if (output_archive_name)
	archive_name = output_archive_name;
    }
  else
    {
      /* Copy pass.  */
      if (index != argc - 1)
	USAGE_ERROR ((0, 0, _("Too many arguments")));

      if (archive_format != arf_unknown)
	USAGE_ERROR((0, 0,
		     _("Archive format is not specified in copy-pass mode (use --format option)")));

      CHECK_USAGE(swap_bytes_flag, "--swap-bytes (--swap)", "--pass-through");
      CHECK_USAGE(swap_halfwords_flag, "--swap-halfwords (--swap)",
		  "--pass-through");
      CHECK_USAGE(table_flag, "--list", "--pass-through");
      CHECK_USAGE(rename_flag, "--rename", "--pass-through");
      CHECK_USAGE(append_flag, "--append", "--pass-through");
      CHECK_USAGE(rename_batch_file, "--rename-batch-file", "--pass-through");
      CHECK_USAGE(abs_paths_flag, "--absolute-pathnames",
		  "--pass-through");
      CHECK_USAGE(to_stdout_option, "--to-stdout", "--pass-through");
      
      directory_name = argv[index];
    }

  if (archive_name)
    {
      if (copy_function != copy_in && copy_function != copy_out)
	USAGE_ERROR ((0, 0,
		      _("-F can be used only with --create or --extract")));
      archive_des = open_archive (archive_name);
      if (archive_des < 0)
	error (1, errno, "%s", archive_name);
    }
		     
  /* Prevent SysV non-root users from giving away files inadvertantly.
     This happens automatically on BSD, where only root can give
     away files.  */
  if (set_owner_flag == false && set_group_flag == false && geteuid ())
    no_chown_flag = true;
}

/* Initialize the input and output buffers to their proper size and
   initialize all variables associated with the input and output
   buffers.  */

void
initialize_buffers ()
{
  int in_buf_size, out_buf_size;

  if (copy_function == process_copy_in)
    {
      /* Make sure the input buffer can always hold 2 blocks and that it
	 is big enough to hold 1 tar record (512 bytes) even if it
	 is not aligned on a block boundary.  The extra buffer space
	 is needed by process_copyin and peek_in_buf to automatically
	 figure out what kind of archive it is reading.  */
      if (io_block_size >= 512)
	in_buf_size = 2 * io_block_size;
      else
	in_buf_size = 1024;
      out_buf_size = DISK_IO_BLOCK_SIZE;
    }
  else if (copy_function == process_copy_out)
    {
      in_buf_size = DISK_IO_BLOCK_SIZE;
      out_buf_size = io_block_size;
    }
  else
    {
      in_buf_size = DISK_IO_BLOCK_SIZE;
      out_buf_size = DISK_IO_BLOCK_SIZE;
    }

  input_buffer = (char *) xmalloc (in_buf_size);
  in_buff = input_buffer;
  input_buffer_size = in_buf_size;
  input_size = 0;
  input_bytes = 0;

  output_buffer = (char *) xmalloc (out_buf_size);
  out_buff = output_buffer;
  output_size = 0;
  output_bytes = 0;

  /* Clear the block of zeros.  */
  bzero (zeros_512, 512);
}

int
main (int argc, char *argv[])
{
#ifdef HAVE_LOCALE_H
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  program_name = argv[0];
  
#ifdef __TURBOC__
  _fmode = O_BINARY;		/* Put stdin and stdout in binary mode.  */
#endif
#ifdef __EMX__			/* gcc on OS/2.  */
  _response (&argc, &argv);
  _wildcard (&argc, &argv);
#endif

  process_args (argc, argv);
  umask (0);

  initialize_buffers ();

  (*copy_function) ();

  if (archive_des >= 0 && rmtclose (archive_des) == -1)
    error (1, errno, _("error closing archive"));

  exit (0);
}
