/* main.c - main program and argument processing for cpio.
   Copyright (C) 1990, 1991, 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Written by Phil Nelson <phil@cs.wwu.edu>,
   David MacKenzie <djm@gnu.ai.mit.edu>,
   and John Oleynick <juo@klinzhai.rutgers.edu>.  */

/* $FreeBSD: src/contrib/cpio/main.c,v 1.3 1999/09/15 01:47:13 peter Exp $ */

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if (defined(BSD) && (BSD >= 199306))
#include <locale.h>
#endif
#include "filetypes.h"
#include "system.h"
#include "cpiohdr.h"
#include "dstring.h"
#include "extern.h"
#include "rmt.h"

struct option long_opts[] =
{
  {"null", 0, 0, '0'},
  {"append", 0, 0, 'A'},
  {"block-size", 1, 0, 130},
  {"create", 0, 0, 'o'},
  {"dereference", 0, 0, 'L'},
  {"dot", 0, 0, 'V'},
  {"extract", 0, 0, 'i'},
  {"file", 1, 0, 'F'},
  {"force-local", 0, &f_force_local, 1},
  {"format", 1, 0, 'H'},
  {"help", 0, 0, 132},
  {"io-size", 1, 0, 'C'},
  {"link", 0, &link_flag, TRUE},
  {"list", 0, &table_flag, TRUE},
  {"make-directories", 0, &create_dir_flag, TRUE},
  {"message", 1, 0, 'M'},
  {"no-absolute-filenames", 0, 0, 136},
  {"no-preserve-owner", 0, 0, 134},
  {"nonmatching", 0, &copy_matching_files, FALSE},
  {"numeric-uid-gid", 0, &numeric_uid, TRUE},
  {"only-verify-crc", 0, 0, 139},
  {"owner", 1, 0, 'R'},
  {"pass-through", 0, 0, 'p'},
  {"pattern-file", 1, 0, 'E'},
  {"preserve-modification-time", 0, &retain_time_flag, TRUE},
  {"rename", 0, &rename_flag, TRUE},
  {"rename-batch-file", 1, 0, 137},
  {"quiet", 0, 0, 138},
  {"sparse", 0, 0, 135},
  {"swap", 0, 0, 'b'},
  {"swap-bytes", 0, 0, 's'},
  {"swap-halfwords", 0, 0, 'S'},
  {"reset-access-time", 0, &reset_time_flag, TRUE},
  {"unconditional", 0, &unconditional_flag, TRUE},
  {"verbose", 0, &verbose_flag, TRUE},
  {"version", 0, 0, 131},
#ifdef DEBUG_CPIO
  {"debug", 0, &debug_flag, TRUE},
#endif
  {0, 0, 0, 0}
};

/*  Print usage message and exit with error.  */

void
usage (fp, status)
  FILE *fp;
  int status;
{
  fprintf (fp, "\
Usage: %s {-o|--create} [-0acvABLV] [-C bytes] [-H format] [-M message]\n\
       [-O [[user@]host:]archive] [-F [[user@]host:]archive]\n\
       [--file=[[user@]host:]archive] [--format=format] [--message=message]\n\
       [--null] [--reset-access-time] [--verbose] [--dot] [--append]\n\
       [--block-size=blocks] [--dereference] [--io-size=bytes] [--quiet]\n\
       [--force-local] [--help] [--version] < name-list [> archive]\n", program_name);
  fprintf (fp, "\
       %s {-i|--extract} [-bcdfmnrtsuvBSV] [-C bytes] [-E file] [-H format]\n\
       [-M message] [-R [user][:.][group]] [-I [[user@]host:]archive]\n\
       [-F [[user@]host:]archive] [--file=[[user@]host:]archive]\n\
       [--make-directories] [--nonmatching] [--preserve-modification-time]\n\
       [--numeric-uid-gid] [--rename] [--list] [--swap-bytes] [--swap] [--dot]\n\
       [--unconditional] [--verbose] [--block-size=blocks] [--swap-halfwords]\n\
       [--io-size=bytes] [--pattern-file=file] [--format=format]\n\
       [--owner=[user][:.][group]] [--no-preserve-owner] [--message=message]\n\
       [--force-local] [--no-absolute-filenames] [--sparse] [--only-verify-crc]\n\
       [--quiet] [--help] [--version] [pattern...] [< archive]\n",
	   program_name);
  fprintf (fp, "\
       %s {-p|--pass-through} [-0adlmuvLV] [-R [user][:.][group]]\n\
       [--null] [--reset-access-time] [--make-directories] [--link] [--quiet]\n\
       [--preserve-modification-time] [--unconditional] [--verbose] [--dot]\n\
       [--dereference] [--owner=[user][:.][group]] [--no-preserve-owner]\n\
       [--sparse] [--help] [--version] destination-directory < name-list\n", program_name);
  exit (status);
}

/* Process the arguments.  Set all options and set up the copy pass
   directory or the copy in patterns.  */

void
process_args (argc, argv)
     int argc;
     char *argv[];
{
  extern char *version_string;
  void (*copy_in) ();		/* Work around for pcc bug.  */
  void (*copy_out) ();
  int c;
  char *input_archive_name = 0;
  char *output_archive_name = 0;

  if (argc < 2)
    usage (stderr, 2);

  xstat = lstat;

  while ((c = getopt_long (argc, argv,
			   "0aAbBcC:dfE:F:H:iI:lLmM:noO:prR:sStuvVz",
			   long_opts, (int *) 0)) != -1)
    {
      switch (c)
	{
	case 0:			/* A long option that just sets a flag.  */
	  break;

	case '0':		/* Read null-terminated filenames.  */
	  name_end = '\0';
	  break;

	case 'a':		/* Reset access times.  */
	  reset_time_flag = TRUE;
	  break;

	case 'A':		/* Append to the archive.  */
	  append_flag = TRUE;
	  break;

	case 'b':		/* Swap bytes and halfwords.  */
	  swap_bytes_flag = TRUE;
	  swap_halfwords_flag = TRUE;
	  break;

	case 'B':		/* Set block size to 5120.  */
	  io_block_size = 5120;
	  break;

	case 130:		/* --block-size */
	  io_block_size = atoi (optarg);
	  if (io_block_size < 1)
	    error (2, 0, "invalid block size");
	  io_block_size *= 512;
	  break;

	case 'c':		/* Use the old portable ASCII format.  */
	  if (archive_format != arf_unknown)
	    usage (stderr, 2);
#ifdef SVR4_COMPAT
	  archive_format = arf_newascii; /* -H newc.  */
#else
	  archive_format = arf_oldascii; /* -H odc.  */
#endif
	  break;

	case 'C':		/* Block size.  */
	  io_block_size = atoi (optarg);
	  if (io_block_size < 1)
	    error (2, 0, "invalid block size");
	  break;

	case 'd':		/* Create directories where needed.  */
	  create_dir_flag = TRUE;
	  break;

	case 'f':		/* Only copy files not matching patterns.  */
	  copy_matching_files = FALSE;
	  break;

	case 'E':		/* Pattern file name.  */
	  pattern_file_name = optarg;
	  break;

	case 'F':		/* Archive file name.  */
	  archive_name = optarg;
	  break;

	case 'H':		/* Header format name.  */
	  if (archive_format != arf_unknown)
	    usage (stderr, 2);
	  if (!strcmp (optarg, "crc") || !strcmp (optarg, "CRC"))
	    archive_format = arf_crcascii;
	  else if (!strcmp (optarg, "newc") || !strcmp (optarg, "NEWC"))
	    archive_format = arf_newascii;
	  else if (!strcmp (optarg, "odc") || !strcmp (optarg, "ODC"))
	    archive_format = arf_oldascii;
	  else if (!strcmp (optarg, "bin") || !strcmp (optarg, "BIN"))
	    archive_format = arf_binary;
	  else if (!strcmp (optarg, "ustar") || !strcmp (optarg, "USTAR"))
	    archive_format = arf_ustar;
	  else if (!strcmp (optarg, "tar") || !strcmp (optarg, "TAR"))
	    archive_format = arf_tar;
	  else if (!strcmp (optarg, "hpodc") || !strcmp (optarg, "HPODC"))
	    archive_format = arf_hpoldascii;
	  else if (!strcmp (optarg, "hpbin") || !strcmp (optarg, "HPBIN"))
	    archive_format = arf_hpbinary;
	  else
	    error (2, 0, "\
invalid archive format `%s'; valid formats are:\n\
crc newc odc bin ustar tar (all-caps also recognized)", optarg);
	  break;

	case 'i':		/* Copy-in mode.  */
	  if (copy_function != 0)
	    usage (stderr, 2);
	  copy_function = process_copy_in;
	  break;

	case 'I':		/* Input archive file name.  */
	  input_archive_name = optarg;
	  break;

	case 'k':		/* Handle corrupted archives.  We always handle
				   corrupted archives, but recognize this
				   option for compatability.  */
	  break;

	case 'l':		/* Link files when possible.  */
	  link_flag = TRUE;
	  break;

	case 'L':		/* Dereference symbolic links.  */
	  xstat = stat;
	  break;

	case 'm':		/* Retain previous file modify times.  */
	  retain_time_flag = TRUE;
	  break;

	case 'M':		/* New media message.  */
	  set_new_media_message (optarg);
	  break;

	case 'n':		/* Long list owner and group as numbers.  */
	  numeric_uid = TRUE;
	  break;

	case 136:		/* --no-absolute-filenames */
	  no_abs_paths_flag = TRUE;
	  break;
	
	case 134:		/* --no-preserve-owner */
	  if (set_owner_flag || set_group_flag)
	    usage (stderr, 2);
	  no_chown_flag = TRUE;
	  break;

	case 'o':		/* Copy-out mode.  */
	  if (copy_function != 0)
	    usage (stderr, 2);
	  copy_function = process_copy_out;
	  break;

	case 'O':		/* Output archive file name.  */
	  output_archive_name = optarg;
	  break;

	case 139:
	  only_verify_crc_flag = TRUE;
	  break;

	case 'p':		/* Copy-pass mode.  */
	  if (copy_function != 0)
	    usage (stderr, 2);
	  copy_function = process_copy_pass;
	  break;

	case 'r':		/* Interactively rename.  */
	  rename_flag = TRUE;
	  break;

	case 137:
	  rename_batch_file = optarg;
	  break;

	case 138:
	  quiet_flag = TRUE;
	  break;

	case 'R':		/* Set the owner.  */
	  if (no_chown_flag)
	    usage (stderr, 2);
#ifndef __MSDOS__
	  {
	    char *e, *u, *g;

	    e = parse_user_spec (optarg, &set_owner, &set_group, &u, &g);
	    if (e)
	      error (2, 0, "%s: %s", optarg, e);
	    if (u)
	      {
		free (u);
		set_owner_flag = TRUE;
	      }
	    if (g)
	      {
		free (g);
		set_group_flag = TRUE;
	      }
	  }
#endif
	  break;

	case 's':		/* Swap bytes.  */
	  swap_bytes_flag = TRUE;
	  break;

	case 'S':		/* Swap halfwords.  */
	  swap_halfwords_flag = TRUE;
	  break;

	case 't':		/* Only print a list.  */
	  table_flag = TRUE;
	  break;

	case 'u':		/* Replace all!  Unconditionally!  */
	  unconditional_flag = TRUE;
	  break;

	case 'v':		/* Verbose!  */
	  verbose_flag = TRUE;
	  break;

	case 'V':		/* Print `.' for each file.  */
	  dot_flag = TRUE;
	  break;

	case 131:
	  printf ("GNU cpio %s", version_string);
	  exit (0);
	  break;

	case 135:
	  sparse_flag = TRUE;
	  break;

	case 132:		/* --help */
	  usage (stdout, 0);
	  break;

	default:
	  usage (stderr, 2);
	}
    }

  /* Do error checking and look at other args.  */

  if (copy_function == 0)
    {
      if (table_flag)
	copy_function = process_copy_in;
      else
	usage (stderr, 2);
    }

  if ((!table_flag || !verbose_flag) && numeric_uid)
    usage (stderr, 2);

  /* Work around for pcc bug.  */
  copy_in = process_copy_in;
  copy_out = process_copy_out;

  if (copy_function == copy_in)
    {
      archive_des = 0;
      if (link_flag || reset_time_flag || xstat != lstat || append_flag
	  || sparse_flag
	  || output_archive_name
	  || (archive_name && input_archive_name))
	usage (stderr, 2);
      if (archive_format == arf_crcascii)
	crc_i_flag = TRUE;
      num_patterns = argc - optind;
      save_patterns = &argv[optind];
      if (input_archive_name)
	archive_name = input_archive_name;
    }
  else if (copy_function == copy_out)
    {
      archive_des = 1;
      if (argc != optind || create_dir_flag || rename_flag
	  || table_flag || unconditional_flag || link_flag
	  || retain_time_flag || no_chown_flag || set_owner_flag
	  || set_group_flag || swap_bytes_flag || swap_halfwords_flag
	  || (append_flag && !(archive_name || output_archive_name))
	  || rename_batch_file || no_abs_paths_flag
	  || input_archive_name || (archive_name && output_archive_name))
	usage (stderr, 2);
      if (archive_format == arf_unknown)
	archive_format = arf_binary;
      if (output_archive_name)
	archive_name = output_archive_name;
    }
  else
    {
      /* Copy pass.  */
      archive_des = -1;
      if (argc - 1 != optind || archive_format != arf_unknown
	  || swap_bytes_flag || swap_halfwords_flag
	  || table_flag || rename_flag || append_flag
	  || rename_batch_file || no_abs_paths_flag)
	usage (stderr, 2);
      directory_name = argv[optind];
    }

  if (archive_name)
    {
      if (copy_function != copy_in && copy_function != copy_out)
	usage (stderr, 2);
      archive_des = open_archive (archive_name);
      if (archive_des < 0)
	error (1, errno, "%s", archive_name);
    }

#ifndef __MSDOS__
  /* Prevent SysV non-root users from giving away files inadvertantly.
     This happens automatically on BSD, where only root can give
     away files.  */
  if (set_owner_flag == FALSE && set_group_flag == FALSE && geteuid ())
    no_chown_flag = TRUE;
#endif
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
main (argc, argv)
     int argc;
     char *argv[];
{
  program_name = argv[0];

#if (defined(BSD) && (BSD >= 199306))
  (void) setlocale (LC_ALL, "");
#endif

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
    error (1, errno, "error closing archive");

  exit (0);
}
