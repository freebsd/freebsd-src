/* Tar -- a tape archiver.
   Copyright (C) 1988, 1992, 1993 Free Software Foundation

This file is part of GNU Tar.

GNU Tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Tar; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * A tar (tape archiver) program.
 *
 * Written by John Gilmore, ihnp4!hoptoad!gnu, starting 25 Aug 85.
 */

#include <stdio.h>
#include <sys/types.h>		/* Needed for typedefs in tar.h */
#include "getopt.h"

/*
 * The following causes "tar.h" to produce definitions of all the
 * global variables, rather than just "extern" declarations of them.
 */
#define TAR_EXTERN		/**/
#include "tar.h"

#include "port.h"
#include "gnuregex.h"
#include "fnmatch.h"

/*
 * We should use a conversion routine that does reasonable error
 * checking -- atoi doesn't.  For now, punt.  FIXME.
 */
#define intconv	atoi
PTR ck_malloc ();
PTR ck_realloc ();
extern int getoldopt ();
extern void read_and ();
extern void list_archive ();
extern void extract_archive ();
extern void diff_archive ();
extern void create_archive ();
extern void update_archive ();
extern void junk_archive ();
extern void init_volume_number ();
extern void closeout_volume_number ();

/* JF */
extern time_t get_date ();

time_t new_time;

static FILE *namef;		/* File to read names from */
static char **n_argv;		/* Argv used by name routines */
static int n_argc;		/* Argc used by name routines */
static char **n_ind;		/* Store an array of names */
static int n_indalloc;		/* How big is the array? */
static int n_indused;		/* How many entries does it have? */
static int n_indscan;		/* How many of the entries have we scanned? */


extern FILE *msg_file;

int check_exclude ();
void add_exclude ();
void add_exclude_file ();
void addname ();
void describe ();
void diff_init ();
void extr_init ();
int is_regex ();
void name_add ();
void name_init ();
void options ();
char *un_quote_string ();
int nlpsfreed = 0;

#ifndef S_ISLNK
#define lstat stat
#endif

#ifndef DEFBLOCKING
#define DEFBLOCKING 20
#endif

#ifndef DEF_AR_FILE
#define DEF_AR_FILE "tar.out"
#endif

/* For long options that unconditionally set a single flag, we have getopt
   do it.  For the others, we share the code for the equivalent short
   named option, the name of which is stored in the otherwise-unused `val'
   field of the `struct option'; for long options that have no equivalent
   short option, we use nongraphic characters as pseudo short option
   characters, starting (for no particular reason) with character 10. */

struct option long_options[] =
{
  {"create", 0, 0, 'c'},
  {"append", 0, 0, 'r'},
  {"extract", 0, 0, 'x'},
  {"get", 0, 0, 'x'},
  {"list", 0, 0, 't'},
  {"update", 0, 0, 'u'},
  {"catenate", 0, 0, 'A'},
  {"concatenate", 0, 0, 'A'},
  {"compare", 0, 0, 'd'},
  {"diff", 0, 0, 'd'},
  {"delete", 0, 0, 14},
  {"help", 0, 0, 12},

  {"null", 0, 0, 16},
  {"directory", 1, 0, 'C'},
  {"record-number", 0, &f_sayblock, 1},
  {"files-from", 1, 0, 'T'},
  {"label", 1, 0, 'V'},
  {"exclude-from", 1, 0, 'X'},
  {"exclude", 1, 0, 15},
  {"file", 1, 0, 'f'},
  {"block-size", 1, 0, 'b'},
  {"version", 0, 0, 11},
  {"verbose", 0, 0, 'v'},
  {"totals", 0, &f_totals, 1},

  {"read-full-blocks", 0, &f_reblock, 1},
  {"starting-file", 1, 0, 'K'},
  {"to-stdout", 0, &f_exstdout, 1},
  {"ignore-zeros", 0, &f_ignorez, 1},
  {"keep-old-files", 0, 0, 'k'},
  {"same-permissions", 0, &f_use_protection, 1},
  {"preserve-permissions", 0, &f_use_protection, 1},
  {"modification-time", 0, &f_modified, 1},
  {"preserve", 0, 0, 10},
  {"same-order", 0, &f_sorted_names, 1},
  {"same-owner", 0, &f_do_chown, 1},
  {"preserve-order", 0, &f_sorted_names, 1},

  {"newer", 1, 0, 'N'},
  {"after-date", 1, 0, 'N'},
  {"newer-mtime", 1, 0, 13},
  {"incremental", 0, 0, 'G'},
  {"listed-incremental", 1, 0, 'g'},
  {"multi-volume", 0, &f_multivol, 1},
  {"info-script", 1, 0, 'F'},
  {"new-volume-script", 1, 0, 'F'},
  {"absolute-paths", 0, &f_absolute_paths, 1},
  {"interactive", 0, &f_confirm, 1},
  {"confirmation", 0, &f_confirm, 1},

  {"verify", 0, &f_verify, 1},
  {"dereference", 0, &f_follow_links, 1},
  {"one-file-system", 0, &f_local_filesys, 1},
  {"old-archive", 0, 0, 'o'},
  {"portability", 0, 0, 'o'},
  {"compress", 0, 0, 'Z'},
  {"uncompress", 0, 0, 'Z'},
  {"block-compress", 0, &f_compress_block, 1},
  {"gzip", 0, 0, 'z'},
  {"ungzip", 0, 0, 'z'},
  {"use-compress-program", 1, 0, 18},


  {"same-permissions", 0, &f_use_protection, 1},
  {"sparse", 0, &f_sparse_files, 1},
  {"tape-length", 1, 0, 'L'},
  {"remove-files", 0, &f_remove_files, 1},
  {"ignore-failed-read", 0, &f_ignore_failed_read, 1},
  {"checkpoint", 0, &f_checkpoint, 1},
  {"show-omitted-dirs", 0, &f_show_omitted_dirs, 1},
  {"volno-file", 1, 0, 17},
  {"force-local", 0, &f_force_local, 1},
  {"atime-preserve", 0, &f_atime_preserve, 1},

  {"unlink", 0, &f_unlink, 1},
  {"fast-read", 0, &f_fast_read, 1},

  {0, 0, 0, 0}
};

/*
 * Main routine for tar.
 */
void
main (argc, argv)
     int argc;
     char **argv;
{
  extern char version_string[];

  tar = argv[0];		/* JF: was "tar" Set program name */
  filename_terminator = '\n';
  errors = 0;

  options (argc, argv);

  if (!n_argv)
    name_init (argc, argv);

  if (f_volno_file)
    init_volume_number ();

  switch (cmd_mode)
    {
    case CMD_CAT:
    case CMD_UPDATE:
    case CMD_APPEND:
      update_archive ();
      break;
    case CMD_DELETE:
      junk_archive ();
      break;
    case CMD_CREATE:
      create_archive ();
      if (f_totals)
	fprintf (stderr, "Total bytes written: %d\n", tot_written);
      break;
    case CMD_EXTRACT:
      if (f_volhdr)
	{
	  const char *err;
	  label_pattern = (struct re_pattern_buffer *)
	    ck_malloc (sizeof *label_pattern);
	  err = re_compile_pattern (f_volhdr, strlen (f_volhdr),
				    label_pattern);
	  if (err)
	    {
	      fprintf (stderr, "Bad regular expression: %s\n",
		       err);
	      errors++;
	      break;
	    }

	}
      extr_init ();
      read_and (extract_archive);
      break;
    case CMD_LIST:
      if (f_volhdr)
	{
	  const char *err;
	  label_pattern = (struct re_pattern_buffer *)
	    ck_malloc (sizeof *label_pattern);
	  err = re_compile_pattern (f_volhdr, strlen (f_volhdr),
				    label_pattern);
	  if (err)
	    {
	      fprintf (stderr, "Bad regular expression: %s\n",
		       err);
	      errors++;
	      break;
	    }
	}
      read_and (list_archive);
#if 0
      if (!errors)
	errors = different;
#endif
      break;
    case CMD_DIFF:
      diff_init ();
      read_and (diff_archive);
      break;
    case CMD_VERSION:
      fprintf (stderr, "%s\n", version_string);
      break;
    case CMD_NONE:
      msg ("you must specify exactly one of the r, c, t, x, or d options\n");
      fprintf (stderr, "For more information, type ``%s --help''.\n", tar);
      exit (EX_ARGSBAD);
    }
  if (f_volno_file)
    closeout_volume_number ();
  exit (errors ? EX_ARGSBAD : 0);	/* FIXME (should be EX_NONDESCRIPT) */
  /* NOTREACHED */
}


/*
 * Parse the options for tar.
 */
void
options (argc, argv)
     int argc;
     char **argv;
{
  register int c;		/* Option letter */
  int ind = -1;

  /* Set default option values */
  blocking = DEFBLOCKING;	/* From Makefile */
  ar_files = (char **) ck_malloc (sizeof (char *) * 10);
  ar_files_len = 10;
  n_ar_files = 0;
  cur_ar_file = 0;

  /* Parse options */
  while ((c = getoldopt (argc, argv,
	       "-01234567Ab:BcC:df:F:g:GhikK:lL:mMN:oOpPrRsStT:uvV:wWxX:zZ",
			 long_options, &ind)) != EOF)
    {
      switch (c)
	{
	case 0:		/* long options that set a single flag */
	  break;
	case 1:
	  /* File name or non-parsed option */
	  name_add (optarg);
	  break;
	case 'C':
	  name_add ("-C");
	  name_add (optarg);
	  break;
	case 10:		/* preserve */
	  f_use_protection = f_sorted_names = 1;
	  break;
	case 11:
	  if (cmd_mode != CMD_NONE)
	    goto badopt;
	  cmd_mode = CMD_VERSION;
	  break;
	case 12:		/* help */
	  printf ("This is GNU tar, the tape archiving program.\n");
	  describe ();
	  exit (1);
	case 13:
	  f_new_files++;
	  goto get_newer;

	case 14:		/* Delete in the archive */
	  if (cmd_mode != CMD_NONE)
	    goto badopt;
	  cmd_mode = CMD_DELETE;
	  break;

	case 15:
	  f_exclude++;
	  add_exclude (optarg);
	  break;

	case 16:		/* -T reads null terminated filenames. */
	  filename_terminator = '\0';
	  break;

	case 17:
	  f_volno_file = optarg;
	  break;

	case 18:
	  if (f_compressprog)
	    {
	      msg ("Only one compression option permitted\n");
	      exit (EX_ARGSBAD);
	    }
	  f_compressprog = optarg;
	  break;

	case 'g':		/* We are making a GNU dump; save
				   directories at the beginning of
				   the archive, and include in each
				   directory its contents */
	  if (f_oldarch)
	    goto badopt;
	  f_gnudump++;
	  gnu_dumpfile = optarg;
	  break;


	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	  {
	    /* JF this'll have to be modified for other
				   systems, of course! */
	    int d, add;
	    static char buf[50];

	    d = getoldopt (argc, argv, "lmh");
#ifdef MAYBEDEF
	    sprintf (buf, "/dev/rmt/%d%c", c, d);
#else
#ifndef LOW_NUM
#define LOW_NUM 0
#define MID_NUM 8
#define HGH_NUM 16
#endif
	    if (d == 'l')
	      add = LOW_NUM;
	    else if (d == 'm')
	      add = MID_NUM;
	    else if (d == 'h')
	      add = HGH_NUM;
	    else
	      goto badopt;

	    sprintf (buf, "/dev/rmt%d", add + c - '0');
#endif
	    if (n_ar_files == ar_files_len)
	      ar_files
		= (char **)
		ck_malloc (sizeof (char *)
			   * (ar_files_len *= 2));
	    ar_files[n_ar_files++] = buf;
	  }
	  break;

	case 'A':		/* Arguments are tar files,
				   just cat them onto the end
				   of the archive.  */
	  if (cmd_mode != CMD_NONE)
	    goto badopt;
	  cmd_mode = CMD_CAT;
	  break;

	case 'b':		/* Set blocking factor */
	  blocking = intconv (optarg);
	  break;

	case 'B':		/* Try to reblock input */
	  f_reblock++;		/* For reading 4.2BSD pipes */
	  break;

	case 'c':		/* Create an archive */
	  if (cmd_mode != CMD_NONE)
	    goto badopt;
	  cmd_mode = CMD_CREATE;
	  break;

#if 0
	case 'C':
	  if (chdir (optarg) < 0)
	    msg_perror ("Can't change directory to %d", optarg);
	  break;
#endif

	case 'd':		/* Find difference tape/disk */
	  if (cmd_mode != CMD_NONE)
	    goto badopt;
	  cmd_mode = CMD_DIFF;
	  break;

	case 'f':		/* Use ar_file for the archive */
	  if (n_ar_files == ar_files_len)
	    ar_files
	      = (char **) ck_malloc (sizeof (char *)
				     * (ar_files_len *= 2));

	  ar_files[n_ar_files++] = optarg;
	  break;

	case 'F':
	  /* Since -F is only useful with -M , make it implied */
	  f_run_script_at_end++;/* run this script at the end */
	  info_script = optarg;	/* of each tape */
	  f_multivol++;
	  break;

	case 'G':		/* We are making a GNU dump; save
				   directories at the beginning of
				   the archive, and include in each
				   directory its contents */
	  if (f_oldarch)
	    goto badopt;
	  f_gnudump++;
	  gnu_dumpfile = 0;
	  break;

	case 'h':
	  f_follow_links++;	/* follow symbolic links */
	  break;

	case 'i':
	  f_ignorez++;		/* Ignore zero records (eofs) */
	  /*
			 * This can't be the default, because Unix tar
			 * writes two records of zeros, then pads out the
			 * block with garbage.
			 */
	  break;

	case 'k':		/* Don't overwrite files */
#ifdef NO_OPEN3
	  msg ("can't keep old files on this system");
	  exit (EX_ARGSBAD);
#else
	  f_keep++;
#endif
	  break;

	case 'K':
	  f_startfile++;
	  addname (optarg);
	  break;

	case 'l':		/* When dumping directories, don't
				   dump files/subdirectories that are
				   on other filesystems. */
	  f_local_filesys++;
	  break;

	case 'L':
	  tape_length = intconv (optarg);
	  f_multivol++;
	  break;
	case 'm':
	  f_modified++;
	  break;

	case 'M':		/* Make Multivolume archive:
				   When we can't write any more
				   into the archive, re-open it,
				   and continue writing */
	  f_multivol++;
	  break;

	case 'N':		/* Only write files newer than X */
	get_newer:
	  f_new_files++;
	  new_time = get_date (optarg, (PTR) 0);
	  if (new_time == (time_t) - 1)
	    {
	      msg ("invalid date format `%s'", optarg);
	      exit (EX_ARGSBAD);
	    }
	  break;

	case 'o':		/* Generate old archive */
	  if (f_gnudump /* || f_dironly */ )
	    goto badopt;
	  f_oldarch++;
	  break;

	case 'O':
	  f_exstdout++;
	  break;

	case 'p':
	  f_use_protection++;
	  break;

	case 'P':
	  f_absolute_paths++;
	  break;

	case 'r':		/* Append files to the archive */
	  if (cmd_mode != CMD_NONE)
	    goto badopt;
	  cmd_mode = CMD_APPEND;
	  break;

	case 'R':
	  f_sayblock++;		/* Print block #s for debug */
	  break;		/* of bad tar archives */

	case 's':
	  f_sorted_names++;	/* Names to extr are sorted */
	  break;

	case 'S':		/* deal with sparse files */
	  f_sparse_files++;
	  break;
	case 't':
	  if (cmd_mode != CMD_NONE)
	    goto badopt;
	  cmd_mode = CMD_LIST;
	  f_verbose++;		/* "t" output == "cv" or "xv" */
	  break;

	case 'T':
	  name_file = optarg;
	  f_namefile++;
	  break;

	case 'u':		/* Append files to the archive that
				   aren't there, or are newer than the
				   copy in the archive */
	  if (cmd_mode != CMD_NONE)
	    goto badopt;
	  cmd_mode = CMD_UPDATE;
	  break;

	case 'v':
	  f_verbose++;
	  break;

	case 'V':
	  f_volhdr = optarg;
	  break;

	case 'w':
	  f_confirm++;
	  break;

	case 'W':
	  f_verify++;
	  break;

	case 'x':		/* Extract files from the archive */
	  if (cmd_mode != CMD_NONE)
	    goto badopt;
	  cmd_mode = CMD_EXTRACT;
	  break;

	case 'X':
	  f_exclude++;
	  add_exclude_file (optarg);
	  break;

	case 'z':
	  if (f_compressprog)
	    {
	      msg ("Only one compression option permitted\n");
	      exit (EX_ARGSBAD);
	    }
	  f_compressprog = "gzip";
	  break;

	case 'Z':
	  if (f_compressprog)
	    {
	      msg ("Only one compression option permitted\n");
	      exit (EX_ARGSBAD);
	    }
	  f_compressprog = "compress";
	  break;

	case '?':
	badopt:
	  msg ("Unknown option.  Use '%s --help' for a complete list of options.", tar);
	  exit (EX_ARGSBAD);

	}
    }

  blocksize = blocking * RECORDSIZE;
  if (n_ar_files == 0)
    {
      n_ar_files = 1;
      ar_files[0] = getenv ("TAPE");	/* From environment, or */
      if (ar_files[0] == 0)
	ar_files[0] = DEF_AR_FILE;	/* From Makefile */
    }
  if (n_ar_files > 1 && !f_multivol)
    {
      msg ("Multiple archive files requires --multi-volume\n");
      exit (EX_ARGSBAD);
    }
  if (f_compress_block && !f_compressprog)
    {
      msg ("You must use a compression option (--gzip, --compress\n\
or --use-compress-program) with --block-compress.\n");
      exit (EX_ARGSBAD);
    }
}


/*
 * Print as much help as the user's gonna get.
 *
 * We have to sprinkle in the KLUDGE lines because too many compilers
 * cannot handle character strings longer than about 512 bytes.  Yuk!
 * In particular, MS-DOS and Xenix MSC and PDP-11 V7 Unix have this
 * problem.
 */
void
describe ()
{
  puts ("choose one of the following:");
  fputs ("\
-A, --catenate,\n\
    --concatenate	append tar files to an archive\n\
-c, --create		create a new archive\n\
-d, --diff,\n\
    --compare		find differences between archive and file system\n\
--delete		delete from the archive (not for use on mag tapes!)\n\
-r, --append		append files to the end of an archive\n\
-t, --list		list the contents of an archive\n\
-u, --update		only append files that are newer than copy in archive\n\
-x, --extract,\n\
    --get		extract files from an archive\n", stdout);

  fprintf (stdout, "\
Other options:\n\
--atime-preserve	don't change access times on dumped files\n\
-b, --block-size N	block size of Nx512 bytes (default N=%d)\n", DEFBLOCKING);
  fputs ("\
-B, --read-full-blocks	reblock as we read (for reading 4.2BSD pipes)\n\
-C, --directory DIR	change to directory DIR\n\
--checkpoint		print directory names while reading the archive\n\
", stdout);			/* KLUDGE */
  fprintf (stdout, "\
-f, --file [HOSTNAME:]F	use archive file or device F (default %s)\n",
	   DEF_AR_FILE);
  fputs ("\
--force-local		archive file is local even if has a colon\n\
-F, --info-script F\n\
    --new-volume-script F run script at end of each tape (implies -M)\n\
-G, --incremental	create/list/extract old GNU-format incremental backup\n\
-g, --listed-incremental F create/list/extract new GNU-format incremental backup\n\
-h, --dereference	don't dump symlinks; dump the files they point to\n\
-i, --ignore-zeros	ignore blocks of zeros in archive (normally mean EOF)\n\
--ignore-failed-read	don't exit with non-zero status on unreadable files\n\
-k, --keep-old-files	keep existing files; don't overwrite them from archive\n\
-K, --starting-file F	begin at file F in the archive\n\
-l, --one-file-system	stay in local file system when creating an archive\n\
-L, --tape-length N	change tapes after writing N*1024 bytes\n\
", stdout);			/* KLUDGE */
  fputs ("\
-m, --modification-time	don't extract file modified time\n\
-M, --multi-volume	create/list/extract multi-volume archive\n\
-N, --after-date DATE,\n\
    --newer DATE	only store files newer than DATE\n\
-o, --old-archive,\n\
    --portability	write a V7 format archive, rather than ANSI format\n\
-O, --to-stdout		extract files to standard output\n\
-p, --same-permissions,\n\
    --preserve-permissions extract all protection information\n\
-P, --absolute-paths	don't strip leading `/'s from file names\n\
--preserve		like -p -s\n\
", stdout);			/* KLUDGE */
  fputs ("\
-R, --record-number	show record number within archive with each message\n\
--remove-files		remove files after adding them to the archive\n\
-s, --same-order,\n\
    --preserve-order	list of names to extract is sorted to match archive\n\
--same-owner		create extracted files with the same ownership \n\
-S, --sparse		handle sparse files efficiently\n\
-T, --files-from F	get names to extract or create from file F\n\
--null			-T reads null-terminated names, disable -C\n\
--totals		print total bytes written with --create\n\
-v, --verbose		verbosely list files processed\n\
-V, --label NAME	create archive with volume name NAME\n\
--version		print tar program version number\n\
-w, --interactive,\n\
    --confirmation	ask for confirmation for every action\n\
", stdout);			/* KLUDGE */
  fputs ("\
-W, --verify		attempt to verify the archive after writing it\n\
--exclude FILE		exclude file FILE\n\
-X, --exclude-from FILE	exclude files listed in FILE\n\
-Z, --compress,\n\
    --uncompress      	filter the archive through compress\n\
-z, --gzip,\n\
    --ungzip		filter the archive through gzip\n\
--use-compress-program PROG\n\
			filter the archive through PROG (which must accept -d)\n\
--block-compress	block the output of compression program for tapes\n\
-[0-7][lmh]		specify drive and density\n\
--unlink		unlink files before creating them\n\
--fast-read 		stop after desired names in archive have been found\n\
", stdout);
}

void
name_add (name)
     char *name;
{
  if (n_indalloc == n_indused)
    {
      n_indalloc += 10;
      n_ind = (char **) (n_indused ? ck_realloc (n_ind, n_indalloc * sizeof (char *)): ck_malloc (n_indalloc * sizeof (char *)));
    }
  n_ind[n_indused++] = name;
}

/*
 * Set up to gather file names for tar.
 *
 * They can either come from stdin or from argv.
 */
void
name_init (argc, argv)
     int argc;
     char **argv;
{

  if (f_namefile)
    {
      if (optind < argc)
	{
	  msg ("too many args with -T option");
	  exit (EX_ARGSBAD);
	}
      if (!strcmp (name_file, "-"))
	{
	  namef = stdin;
	}
      else
	{
	  namef = fopen (name_file, "r");
	  if (namef == NULL)
	    {
	      msg_perror ("can't open file %s", name_file);
	      exit (EX_BADFILE);
	    }
	}
    }
  else
    {
      /* Get file names from argv, after options. */
      n_argc = argc;
      n_argv = argv;
    }
}

/* Read the next filename read from STREAM and null-terminate it.
   Put it into BUFFER, reallocating and adjusting *PBUFFER_SIZE if necessary.
   Return the new value for BUFFER, or NULL at end of file. */

char *
read_name_from_file (buffer, pbuffer_size, stream)
     char *buffer;
     size_t *pbuffer_size;
     FILE *stream;
{
  register int c;
  register int indx = 0;
  register size_t buffer_size = *pbuffer_size;

  while ((c = getc (stream)) != EOF && c != filename_terminator)
    {
      if (indx == buffer_size)
	{
	  buffer_size += NAMSIZ;
	  buffer = ck_realloc (buffer, buffer_size + 2);
	}
      buffer[indx++] = c;
    }
  if (indx == 0 && c == EOF)
    return NULL;
  if (indx == buffer_size)
    {
      buffer_size += NAMSIZ;
      buffer = ck_realloc (buffer, buffer_size + 2);
    }
  buffer[indx] = '\0';
  *pbuffer_size = buffer_size;
  return buffer;
}

/*
 * Get the next name from argv or the name file.
 *
 * Result is in static storage and can't be relied upon across two calls.
 *
 * If CHANGE_DIRS is non-zero, treat a filename of the form "-C" as
 * meaning that the next filename is the name of a directory to change to.
 * If `filename_terminator' is '\0', CHANGE_DIRS is effectively always 0.
 */

char *
name_next (change_dirs)
     int change_dirs;
{
  static char *buffer;		/* Holding pattern */
  static int buffer_siz;
  register char *p;
  register char *q = 0;
  register int next_name_is_dir = 0;
  extern char *un_quote_string ();

  if (buffer_siz == 0)
    {
      buffer = ck_malloc (NAMSIZ + 2);
      buffer_siz = NAMSIZ;
    }
  if (filename_terminator == '\0')
    change_dirs = 0;
tryagain:
  if (namef == NULL)
    {
      if (n_indscan < n_indused)
	p = n_ind[n_indscan++];
      else if (optind < n_argc)
	/* Names come from argv, after options */
	p = n_argv[optind++];
      else
	{
	  if (q)
	    msg ("Missing filename after -C");
	  return NULL;
	}

      /* JF trivial support for -C option.  I don't know if
		   chdir'ing at this point is dangerous or not.
		   It seems to work, which is all I ask. */
      if (change_dirs && !q && p[0] == '-' && p[1] == 'C' && p[2] == '\0')
	{
	  q = p;
	  goto tryagain;
	}
      if (q)
	{
	  if (chdir (p) < 0)
	    msg_perror ("Can't chdir to %s", p);
	  q = 0;
	  goto tryagain;
	}
      /* End of JF quick -C hack */

#if 0
      if (f_exclude && check_exclude (p))
	goto tryagain;
#endif
      return un_quote_string (p);
    }
  while (p = read_name_from_file (buffer, &buffer_siz, namef))
    {
      buffer = p;
      if (*p == '\0')
	continue;		/* Ignore empty lines. */
      q = p + strlen (p) - 1;
      while (q > p && *q == '/')/* Zap trailing "/"s. */
	*q-- = '\0';
      if (change_dirs && next_name_is_dir == 0
	  && p[0] == '-' && p[1] == 'C' && p[2] == '\0')
	{
	  next_name_is_dir = 1;
	  goto tryagain;
	}
      if (next_name_is_dir)
	{
	  if (chdir (p) < 0)
	    msg_perror ("Can't change to directory %s", p);
	  next_name_is_dir = 0;
	  goto tryagain;
	}
#if 0
      if (f_exclude && check_exclude (p))
	goto tryagain;
#endif
      return un_quote_string (p);
    }
  return NULL;
}


/*
 * Close the name file, if any.
 */
void
name_close ()
{

  if (namef != NULL && namef != stdin)
    fclose (namef);
}


/*
 * Gather names in a list for scanning.
 * Could hash them later if we really care.
 *
 * If the names are already sorted to match the archive, we just
 * read them one by one.  name_gather reads the first one, and it
 * is called by name_match as appropriate to read the next ones.
 * At EOF, the last name read is just left in the buffer.
 * This option lets users of small machines extract an arbitrary
 * number of files by doing "tar t" and editing down the list of files.
 */
void
name_gather ()
{
  register char *p;
  static struct name *namebuf;	/* One-name buffer */
  static namelen;
  static char *chdir_name;

  if (f_sorted_names)
    {
      if (!namelen)
	{
	  namelen = NAMSIZ;
	  namebuf = (struct name *) ck_malloc (sizeof (struct name) + NAMSIZ);
	}
      p = name_next (0);
      if (p)
	{
	  if (*p == '-' && p[1] == 'C' && p[2] == '\0')
	    {
	      p = name_next (0);
	      chdir_name = p ? strdup(p) : p;
	      p = name_next (0);
	      if (!chdir_name)
		{
		  msg ("Missing file name after -C");
		  exit (EX_ARGSBAD);
		}
	      namebuf->change_dir = chdir_name;
	    }
	  namebuf->length = strlen (p);
	  if (namebuf->length >= namelen)
	    {
	      namebuf = (struct name *) ck_realloc (namebuf, sizeof (struct name) + namebuf->length);
	      namelen = namebuf->length;
	    }
	  strncpy (namebuf->name, p, namebuf->length);
	  namebuf->name[namebuf->length] = 0;
	  namebuf->next = (struct name *) NULL;
	  namebuf->found = 0;
	  namelist = namebuf;
	  namelast = namelist;
	}
      return;
    }

  /* Non sorted names -- read them all in */
  while (p = name_next (0))
    addname (p);
}

/*
 * Add a name to the namelist.
 */
void
addname (name)
     char *name;		/* pointer to name */
{
  register int i;		/* Length of string */
  register struct name *p;	/* Current struct pointer */
  static char *chdir_name;
  char *new_name ();

  if (name[0] == '-' && name[1] == 'C' && name[2] == '\0')
    {
      name = name_next (0);
      chdir_name = name ? strdup(name) : name;
      name = name_next (0);
      if (!chdir_name)
	{
	  msg ("Missing file name after -C");
	  exit (EX_ARGSBAD);
	}
      if (chdir_name[0] != '/')
	{
	  char *path = ck_malloc (PATH_MAX);
#if defined(__MSDOS__) || defined(HAVE_GETCWD) || defined(_POSIX_VERSION)
	  if (!getcwd (path, PATH_MAX))
	    {
	      msg ("Couldn't get current directory.");
	      exit (EX_SYSTEM);
	    }
#else
	  char *getwd ();

	  if (!getwd (path))
	    {
	      msg ("Couldn't get current directory: %s", path);
	      exit (EX_SYSTEM);
	    }
#endif
	  chdir_name = new_name (path, chdir_name);
	  free (path);
	}
    }

  if (name)
    {
      i = strlen (name);
      /*NOSTRICT*/
      p = (struct name *) malloc ((unsigned) (sizeof (struct name) + i));
    }
  else
    p = (struct name *) malloc ((unsigned) (sizeof (struct name)));
  if (!p)
    {
      if (name)
	msg ("cannot allocate mem for name '%s'.", name);
      else
	msg ("cannot allocate mem for chdir record.");
      exit (EX_SYSTEM);
    }
  p->next = (struct name *) NULL;
  if (name)
    {
      p->fake = 0;
      p->length = i;
      strncpy (p->name, name, i);
      p->name[i] = '\0';	/* Null term */
    }
  else
    p->fake = 1;
  p->found = 0;
  p->regexp = 0;		/* Assume not a regular expression */
  p->firstch = 1;		/* Assume first char is literal */
  p->change_dir = chdir_name;
  p->dir_contents = 0;		/* JF */
  if (name)
    {
      if (index (name, '*') || index (name, '[') || index (name, '?'))
	{
	  p->regexp = 1;	/* No, it's a regexp */
	  if (name[0] == '*' || name[0] == '[' || name[0] == '?')
	    p->firstch = 0;	/* Not even 1st char literal */
	}
    }

  if (namelast)
    namelast->next = p;
  namelast = p;
  if (!namelist)
    namelist = p;
}

/*
 * Return nonzero if name P (from an archive) matches any name from
 * the namelist, zero if not.
 */
int
name_match (p)
     register char *p;
{
  register struct name *nlp;
  struct name *tmpnlp;
  register int len;

again:
  if (0 == (nlp = namelist))	/* Empty namelist is easy */
    return 1;
  if (nlp->fake)
    {
      if (nlp->change_dir && chdir (nlp->change_dir))
	msg_perror ("Can't change to directory %s", nlp->change_dir);
      namelist = 0;
      return 1;
    }
  len = strlen (p);
  for (; nlp != 0; nlp = nlp->next)
    {
      /* If first chars don't match, quick skip */
      if (nlp->firstch && nlp->name[0] != p[0])
	continue;

      /* Regular expressions (shell globbing, actually). */
      if (nlp->regexp)
	{
	  if (fnmatch (nlp->name, p, FNM_LEADING_DIR) == 0)
	    {
	      nlp->found = 1;	/* Remember it matched */
	      if (f_startfile)
		{
		  free ((void *) namelist);
		  namelist = 0;
		}
	      if (nlp->change_dir && chdir (nlp->change_dir))
		msg_perror ("Can't change to directory %s", nlp->change_dir);
	      return 1;		/* We got a match */
	    }
	  continue;
	}

      /* Plain Old Strings */
      if (nlp->length <= len	/* Archive len >= specified */
	  && (p[nlp->length] == '\0' || p[nlp->length] == '/')
      /* Full match on file/dirname */
	  && strncmp (p, nlp->name, nlp->length) == 0)	/* Name compare */
	{
	  nlp->found = 1;	/* Remember it matched */
	  if (f_startfile)
	    {
	      free ((void *) namelist);
	      namelist = 0;
	    }
	  if (nlp->change_dir && chdir (nlp->change_dir))
	    msg_perror ("Can't change to directory %s", nlp->change_dir);
	  if (f_fast_read) {
	      if (strcmp(p, nlp->name) == 0) {
		  /* remove the current entry, since we found a match */
		  /* use brute force, this code is a mess anyway */
		  if (namelist->next == NULL) {
		      /* the list contains one element */
		      free(namelist);
		      namelist = NULL;
		  } else {
		      if (nlp == namelist) {
			  /* the first element is the one */
			  tmpnlp = namelist->next;
			  free(namelist);
			  namelist = tmpnlp;
		      } else {
			  tmpnlp = namelist;
			  while (tmpnlp->next != nlp) {
			      tmpnlp = tmpnlp->next;
			  }
			  tmpnlp->next = nlp->next;
			  free(nlp);
		      }
		  }
		  /* set a boolean to decide wether we started with a  */
		  /* non-empty  namelist, that was emptied */
		  nlpsfreed = 1;
	      }
	  }
	  return 1;		/* We got a match */

	}
    }

  /*
	 * Filename from archive not found in namelist.
	 * If we have the whole namelist here, just return 0.
	 * Otherwise, read the next name in and compare it.
	 * If this was the last name, namelist->found will remain on.
	 * If not, we loop to compare the newly read name.
	 */
  if (f_sorted_names && namelist->found)
    {
      name_gather ();		/* Read one more */
      if (!namelist->found)
	goto again;
    }
  return 0;
}


/*
 * Print the names of things in the namelist that were not matched.
 */
void
names_notfound ()
{
  register struct name *nlp, *next;
  register char *p;

  for (nlp = namelist; nlp != 0; nlp = next)
    {
      next = nlp->next;
      if (!nlp->found)
	msg ("%s not found in archive", nlp->name);

      /*
		 * We could free() the list, but the process is about
		 * to die anyway, so save some CPU time.  Amigas and
		 * other similarly broken software will need to waste
		 * the time, though.
		 */
#ifdef amiga
      if (!f_sorted_names)
	free (nlp);
#endif
    }
  namelist = (struct name *) NULL;
  namelast = (struct name *) NULL;

  if (f_sorted_names)
    {
      while (0 != (p = name_next (1)))
	msg ("%s not found in archive", p);
    }
}

/* These next routines were created by JF */

void
name_expand ()
{
  ;
}

/* This is like name_match(), except that it returns a pointer to the name
   it matched, and doesn't set ->found  The caller will have to do that
   if it wants to.  Oh, and if the namelist is empty, it returns 0, unlike
   name_match(), which returns TRUE */

struct name *
name_scan (p)
     register char *p;
{
  register struct name *nlp;
  register int len;

again:
  if (0 == (nlp = namelist))	/* Empty namelist is easy */
    return 0;
  len = strlen (p);
  for (; nlp != 0; nlp = nlp->next)
    {
      /* If first chars don't match, quick skip */
      if (nlp->firstch && nlp->name[0] != p[0])
	continue;

      /* Regular expressions */
      if (nlp->regexp)
	{
	  if (fnmatch (nlp->name, p, FNM_LEADING_DIR) == 0)
	    return nlp;		/* We got a match */
	  continue;
	}

      /* Plain Old Strings */
      if (nlp->length <= len	/* Archive len >= specified */
	  && (p[nlp->length] == '\0' || p[nlp->length] == '/')
      /* Full match on file/dirname */
	  && strncmp (p, nlp->name, nlp->length) == 0)	/* Name compare */
	return nlp;		/* We got a match */
    }

  /*
	 * Filename from archive not found in namelist.
	 * If we have the whole namelist here, just return 0.
	 * Otherwise, read the next name in and compare it.
	 * If this was the last name, namelist->found will remain on.
	 * If not, we loop to compare the newly read name.
	 */
  if (f_sorted_names && namelist->found)
    {
      name_gather ();		/* Read one more */
      if (!namelist->found)
	goto again;
    }
  return (struct name *) 0;
}

/* This returns a name from the namelist which doesn't have ->found set.
   It sets ->found before returning, so successive calls will find and return
   all the non-found names in the namelist */

struct name *gnu_list_name;

char *
name_from_list ()
{
  if (!gnu_list_name)
    gnu_list_name = namelist;
  while (gnu_list_name && gnu_list_name->found)
    gnu_list_name = gnu_list_name->next;
  if (gnu_list_name)
    {
      gnu_list_name->found++;
      if (gnu_list_name->change_dir)
	if (chdir (gnu_list_name->change_dir) < 0)
	  msg_perror ("can't chdir to %s", gnu_list_name->change_dir);
      return gnu_list_name->name;
    }
  return (char *) 0;
}

void
blank_name_list ()
{
  struct name *n;

  gnu_list_name = 0;
  for (n = namelist; n; n = n->next)
    n->found = 0;
}

char *
new_name (path, name)
     char *path, *name;
{
  char *path_buf;

  path_buf = (char *) malloc (strlen (path) + strlen (name) + 2);
  if (path_buf == 0)
    {
      msg ("Can't allocate memory for name '%s/%s", path, name);
      exit (EX_SYSTEM);
    }
  (void) sprintf (path_buf, "%s/%s", path, name);
  return path_buf;
}

/* returns non-zero if the luser typed 'y' or 'Y', zero otherwise. */

int
confirm (action, file)
     char *action, *file;
{
  int c, nl;
  static FILE *confirm_file = 0;
  extern FILE *msg_file;
  extern char TTY_NAME[];

  fprintf (msg_file, "%s %s?", action, file);
  fflush (msg_file);
  if (!confirm_file)
    {
      confirm_file = (archive == 0) ? fopen (TTY_NAME, "r") : stdin;
      if (!confirm_file)
	{
	  msg ("Can't read confirmation from user");
	  exit (EX_SYSTEM);
	}
    }
  c = getc (confirm_file);
  for (nl = c; nl != '\n' && nl != EOF; nl = getc (confirm_file))
    ;
  return (c == 'y' || c == 'Y');
}

char *x_buffer = 0;
int size_x_buffer;
int free_x_buffer;

char **exclude = 0;
int size_exclude = 0;
int free_exclude = 0;

char **re_exclude = 0;
int size_re_exclude = 0;
int free_re_exclude = 0;

void
add_exclude (name)
     char *name;
{
  /*	char *rname;*/
  /*	char **tmp_ptr;*/
  int size_buf;

  un_quote_string (name);
  size_buf = strlen (name);

  if (x_buffer == 0)
    {
      x_buffer = (char *) ck_malloc (size_buf + 1024);
      free_x_buffer = 1024;
    }
  else if (free_x_buffer <= size_buf)
    {
      char *old_x_buffer;
      char **tmp_ptr;

      old_x_buffer = x_buffer;
      x_buffer = (char *) ck_realloc (x_buffer, size_x_buffer + 1024);
      free_x_buffer = 1024;
      for (tmp_ptr = exclude; tmp_ptr < exclude + size_exclude; tmp_ptr++)
	*tmp_ptr = x_buffer + ((*tmp_ptr) - old_x_buffer);
      for (tmp_ptr = re_exclude; tmp_ptr < re_exclude + size_re_exclude; tmp_ptr++)
	*tmp_ptr = x_buffer + ((*tmp_ptr) - old_x_buffer);
    }

  if (is_regex (name))
    {
      if (free_re_exclude == 0)
	{
	  re_exclude = (char **) (re_exclude ? ck_realloc (re_exclude, (size_re_exclude + 32) * sizeof (char *)): ck_malloc (sizeof (char *) * 32));
	  free_re_exclude += 32;
	}
      re_exclude[size_re_exclude] = x_buffer + size_x_buffer;
      size_re_exclude++;
      free_re_exclude--;
    }
  else
    {
      if (free_exclude == 0)
	{
	  exclude = (char **) (exclude ? ck_realloc (exclude, (size_exclude + 32) * sizeof (char *)): ck_malloc (sizeof (char *) * 32));
	  free_exclude += 32;
	}
      exclude[size_exclude] = x_buffer + size_x_buffer;
      size_exclude++;
      free_exclude--;
    }
  strcpy (x_buffer + size_x_buffer, name);
  size_x_buffer += size_buf + 1;
  free_x_buffer -= size_buf + 1;
}

void
add_exclude_file (file)
     char *file;
{
  FILE *fp;
  char buf[1024];

  if (strcmp (file, "-"))
    fp = fopen (file, "r");
  else
    /* Let's hope the person knows what they're doing. */
    /* Using -X - -T - -f - will get you *REALLY* strange
		   results. . . */
    fp = stdin;

  if (!fp)
    {
      msg_perror ("can't open %s", file);
      exit (2);
    }
  while (fgets (buf, 1024, fp))
    {
      /*		int size_buf;*/
      char *end_str;

      end_str = rindex (buf, '\n');
      if (end_str)
	*end_str = '\0';
      add_exclude (buf);

    }
  fclose (fp);
}

int
is_regex (str)
     char *str;
{
  return index (str, '*') || index (str, '[') || index (str, '?');
}

/* Returns non-zero if the file 'name' should not be added/extracted */
int
check_exclude (name)
     char *name;
{
  int n;
  char *str;
  extern char *strstr ();

  for (n = 0; n < size_re_exclude; n++)
    {
      if (fnmatch (re_exclude[n], name, FNM_LEADING_DIR) == 0)
	return 1;
    }
  for (n = 0; n < size_exclude; n++)
    {
      /* Accept the output from strstr only if it is the last
		   part of the string.  There is certainly a faster way to
		   do this. . . */
      if ((str = strstr (name, exclude[n]))
	  && (str == name || str[-1] == '/')
	  && str[strlen (exclude[n])] == '\0')
	return 1;
    }
  return 0;
}
