/* windres.c -- a program to manipulate Windows resources
   Copyright 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2007
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.
   Rewritten by Kai Tietz, Onevision.

   This file is part of GNU Binutils.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* This program can read and write Windows resources in various
   formats.  In particular, it can act like the rc resource compiler
   program, and it can act like the cvtres res to COFF conversion
   program.

   It is based on information taken from the following sources:

   * Microsoft documentation.

   * The rcl program, written by Gunther Ebert
     <gunther.ebert@ixos-leipzig.de>.

   * The res2coff program, written by Pedro A. Aranda <paag@tid.es>.  */

#include "sysdep.h"
#include <assert.h>
#include <time.h>
#include "bfd.h"
#include "getopt.h"
#include "bucomm.h"
#include "libiberty.h"
#include "safe-ctype.h"
#include "obstack.h"
#include "windres.h"

/* Defined in bfd/binary.c.  Used to set architecture and machine of input
   binary files.  */
extern enum bfd_architecture  bfd_external_binary_architecture;
extern unsigned long          bfd_external_machine;

/* Used by resrc.c at least.  */

int verbose = 0;

int target_is_bigendian = 0;
const char *def_target_arch;

static void set_endianess (bfd *, const char *);

/* An enumeration of format types.  */

enum res_format
{
  /* Unknown format.  */
  RES_FORMAT_UNKNOWN,
  /* Textual RC file.  */
  RES_FORMAT_RC,
  /* Binary RES file.  */
  RES_FORMAT_RES,
  /* COFF file.  */
  RES_FORMAT_COFF
};

/* A structure used to map between format types and strings.  */

struct format_map
{
  const char *name;
  enum res_format format;
};

/* A mapping between names and format types.  */

static const struct format_map format_names[] =
{
  { "rc", RES_FORMAT_RC },
  { "res", RES_FORMAT_RES },
  { "coff", RES_FORMAT_COFF },
  { NULL, RES_FORMAT_UNKNOWN }
};

/* A mapping from file extensions to format types.  */

static const struct format_map format_fileexts[] =
{
  { "rc", RES_FORMAT_RC },
  { "res", RES_FORMAT_RES },
  { "exe", RES_FORMAT_COFF },
  { "obj", RES_FORMAT_COFF },
  { "o", RES_FORMAT_COFF },
  { NULL, RES_FORMAT_UNKNOWN }
};

/* A list of include directories.  */

struct include_dir
{
  struct include_dir *next;
  char *dir;
};

static struct include_dir *include_dirs;

/* Static functions.  */

static void res_init (void);
static int extended_menuitems (const rc_menuitem *);
static enum res_format format_from_name (const char *, int);
static enum res_format format_from_filename (const char *, int);
static void usage (FILE *, int);
static int cmp_res_entry (const void *, const void *);
static rc_res_directory *sort_resources (rc_res_directory *);
static void reswr_init (void);
static const char * quot (const char *);

static rc_uint_type target_get_8 (const void *, rc_uint_type);
static void target_put_8 (void *, rc_uint_type);
static rc_uint_type target_get_16 (const void *, rc_uint_type);
static void target_put_16 (void *, rc_uint_type);
static rc_uint_type target_get_32 (const void *, rc_uint_type);
static void target_put_32 (void *, rc_uint_type);


/* When we are building a resource tree, we allocate everything onto
   an obstack, so that we can free it all at once if we want.  */

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

/* The resource building obstack.  */

static struct obstack res_obstack;

/* Initialize the resource building obstack.  */

static void
res_init (void)
{
  obstack_init (&res_obstack);
}

/* Allocate space on the resource building obstack.  */

void *
res_alloc (rc_uint_type bytes)
{
  return (void *) obstack_alloc (&res_obstack, (size_t) bytes);
}

/* We also use an obstack to save memory used while writing out a set
   of resources.  */

static struct obstack reswr_obstack;

/* Initialize the resource writing obstack.  */

static void
reswr_init (void)
{
  obstack_init (&reswr_obstack);
}

/* Allocate space on the resource writing obstack.  */

void *
reswr_alloc (rc_uint_type bytes)
{
  return (void *) obstack_alloc (&reswr_obstack, (size_t) bytes);
}

/* Open a file using the include directory search list.  */

FILE *
open_file_search (const char *filename, const char *mode, const char *errmsg,
		  char **real_filename)
{
  FILE *e;
  struct include_dir *d;

  e = fopen (filename, mode);
  if (e != NULL)
    {
      *real_filename = xstrdup (filename);
      return e;
    }

  if (errno == ENOENT)
    {
      for (d = include_dirs; d != NULL; d = d->next)
	{
	  char *n;

	  n = (char *) xmalloc (strlen (d->dir) + strlen (filename) + 2);
	  sprintf (n, "%s/%s", d->dir, filename);
	  e = fopen (n, mode);
	  if (e != NULL)
	    {
	      *real_filename = n;
	      return e;
	    }

	  if (errno != ENOENT)
	    break;
	}
    }

  fatal (_("can't open %s `%s': %s"), errmsg, filename, strerror (errno));

  /* Return a value to avoid a compiler warning.  */
  return NULL;
}

/* Compare two resource ID's.  We consider name entries to come before
   numeric entries, because that is how they appear in the COFF .rsrc
   section.  */

int
res_id_cmp (rc_res_id a, rc_res_id b)
{
  if (! a.named)
    {
      if (b.named)
	return 1;
      if (a.u.id > b.u.id)
	return 1;
      else if (a.u.id < b.u.id)
	return -1;
      else
	return 0;
    }
  else
    {
      unichar *as, *ase, *bs, *bse;

      if (! b.named)
	return -1;

      as = a.u.n.name;
      ase = as + a.u.n.length;
      bs = b.u.n.name;
      bse = bs + b.u.n.length;

      while (as < ase)
	{
	  int i;

	  if (bs >= bse)
	    return 1;
	  i = (int) *as - (int) *bs;
	  if (i != 0)
	    return i;
	  ++as;
	  ++bs;
	}

      if (bs < bse)
	return -1;

      return 0;
    }
}

/* Print a resource ID.  */

void
res_id_print (FILE *stream, rc_res_id id, int quote)
{
  if (! id.named)
    fprintf (stream, "%u", (int) id.u.id);
  else
    {
      if (quote)
	unicode_print_quoted (stream, id.u.n.name, id.u.n.length);
      else
      unicode_print (stream, id.u.n.name, id.u.n.length);
    }
}

/* Print a list of resource ID's.  */

void
res_ids_print (FILE *stream, int cids, const rc_res_id *ids)
{
  int i;

  for (i = 0; i < cids; i++)
    {
      res_id_print (stream, ids[i], 1);
      if (i + 1 < cids)
	fprintf (stream, ": ");
    }
}

/* Convert an ASCII string to a resource ID.  */

void
res_string_to_id (rc_res_id *res_id, const char *string)
{
  res_id->named = 1;
  unicode_from_ascii (&res_id->u.n.length, &res_id->u.n.name, string);
}

/* Convert an unicode string to a resource ID.  */
void
res_unistring_to_id (rc_res_id *res_id, const unichar *u)
{
  res_id->named = 1;
  res_id->u.n.length = unichar_len (u);
  res_id->u.n.name = unichar_dup_uppercase (u);
}

/* Define a resource.  The arguments are the resource tree, RESOURCES,
   and the location at which to put it in the tree, CIDS and IDS.
   This returns a newly allocated rc_res_resource structure, which the
   caller is expected to initialize.  If DUPOK is non-zero, then if a
   resource with this ID exists, it is returned.  Otherwise, a warning
   is issued, and a new resource is created replacing the existing
   one.  */

rc_res_resource *
define_resource (rc_res_directory **resources, int cids,
		 const rc_res_id *ids, int dupok)
{
  rc_res_entry *re = NULL;
  int i;

  assert (cids > 0);
  for (i = 0; i < cids; i++)
    {
      rc_res_entry **pp;

      if (*resources == NULL)
	{
	  static unsigned int timeval;

	  /* Use the same timestamp for every resource created in a
             single run.  */
	  if (timeval == 0)
	    timeval = time (NULL);

	  *resources = ((rc_res_directory *)
			res_alloc (sizeof (rc_res_directory)));
	  (*resources)->characteristics = 0;
	  (*resources)->time = timeval;
	  (*resources)->major = 0;
	  (*resources)->minor = 0;
	  (*resources)->entries = NULL;
	}

      for (pp = &(*resources)->entries; *pp != NULL; pp = &(*pp)->next)
	if (res_id_cmp ((*pp)->id, ids[i]) == 0)
	  break;

      if (*pp != NULL)
	re = *pp;
      else
	{
	  re = (rc_res_entry *) res_alloc (sizeof (rc_res_entry));
	  re->next = NULL;
	  re->id = ids[i];
	  if ((i + 1) < cids)
	    {
	      re->subdir = 1;
	      re->u.dir = NULL;
	    }
	  else
	    {
	      re->subdir = 0;
	      re->u.res = NULL;
	    }

	  *pp = re;
	}

      if ((i + 1) < cids)
	{
	  if (! re->subdir)
	    {
	      fprintf (stderr, "%s: ", program_name);
	      res_ids_print (stderr, i, ids);
	      fprintf (stderr, _(": expected to be a directory\n"));
	      xexit (1);
	    }

	  resources = &re->u.dir;
	}
    }

  if (re->subdir)
    {
      fprintf (stderr, "%s: ", program_name);
      res_ids_print (stderr, cids, ids);
      fprintf (stderr, _(": expected to be a leaf\n"));
      xexit (1);
    }

  if (re->u.res != NULL)
    {
      if (dupok)
	return re->u.res;

      fprintf (stderr, _("%s: warning: "), program_name);
      res_ids_print (stderr, cids, ids);
      fprintf (stderr, _(": duplicate value\n"));
    }

  re->u.res = ((rc_res_resource *)
	       res_alloc (sizeof (rc_res_resource)));
  memset (re->u.res, 0, sizeof (rc_res_resource));

  re->u.res->type = RES_TYPE_UNINITIALIZED;
  return re->u.res;
}

/* Define a standard resource.  This is a version of define_resource
   that just takes type, name, and language arguments.  */

rc_res_resource *
define_standard_resource (rc_res_directory **resources, int type,
			  rc_res_id name, rc_uint_type language, int dupok)
{
  rc_res_id a[3];

  a[0].named = 0;
  a[0].u.id = type;
  a[1] = name;
  a[2].named = 0;
  a[2].u.id = language;
  return define_resource (resources, 3, a, dupok);
}

/* Comparison routine for resource sorting.  */

static int
cmp_res_entry (const void *p1, const void *p2)
{
  const rc_res_entry **re1, **re2;

  re1 = (const rc_res_entry **) p1;
  re2 = (const rc_res_entry **) p2;
  return res_id_cmp ((*re1)->id, (*re2)->id);
}

/* Sort the resources.  */

static rc_res_directory *
sort_resources (rc_res_directory *resdir)
{
  int c, i;
  rc_res_entry *re;
  rc_res_entry **a;

  if (resdir->entries == NULL)
    return resdir;

  c = 0;
  for (re = resdir->entries; re != NULL; re = re->next)
    ++c;

  /* This is a recursive routine, so using xmalloc is probably better
     than alloca.  */
  a = (rc_res_entry **) xmalloc (c * sizeof (rc_res_entry *));

  for (i = 0, re = resdir->entries; re != NULL; re = re->next, i++)
    a[i] = re;

  qsort (a, c, sizeof (rc_res_entry *), cmp_res_entry);

  resdir->entries = a[0];
  for (i = 0; i < c - 1; i++)
    a[i]->next = a[i + 1];
  a[i]->next = NULL;

  free (a);

  /* Now sort the subdirectories.  */

  for (re = resdir->entries; re != NULL; re = re->next)
    if (re->subdir)
      re->u.dir = sort_resources (re->u.dir);

  return resdir;
}

/* Return whether the dialog resource DIALOG is a DIALOG or a
   DIALOGEX.  */

int
extended_dialog (const rc_dialog *dialog)
{
  const rc_dialog_control *c;

  if (dialog->ex != NULL)
    return 1;

  for (c = dialog->controls; c != NULL; c = c->next)
    if (c->data != NULL || c->help != 0)
      return 1;

  return 0;
}

/* Return whether MENUITEMS are a MENU or a MENUEX.  */

int
extended_menu (const rc_menu *menu)
{
  return extended_menuitems (menu->items);
}

static int
extended_menuitems (const rc_menuitem *menuitems)
{
  const rc_menuitem *mi;

  for (mi = menuitems; mi != NULL; mi = mi->next)
    {
      if (mi->help != 0 || mi->state != 0)
	return 1;
      if (mi->popup != NULL && mi->id != 0)
	return 1;
      if ((mi->type
	   & ~ (MENUITEM_CHECKED
		| MENUITEM_GRAYED
		| MENUITEM_HELP
		| MENUITEM_INACTIVE
		| MENUITEM_MENUBARBREAK
		| MENUITEM_MENUBREAK))
	  != 0)
	return 1;
      if (mi->popup != NULL)
	{
	  if (extended_menuitems (mi->popup))
	    return 1;
	}
    }

  return 0;
}

/* Convert a string to a format type, or exit if it can't be done.  */

static enum res_format
format_from_name (const char *name, int exit_on_error)
{
  const struct format_map *m;

  for (m = format_names; m->name != NULL; m++)
    if (strcasecmp (m->name, name) == 0)
      break;

  if (m->name == NULL && exit_on_error)
    {
      non_fatal (_("unknown format type `%s'"), name);
      fprintf (stderr, _("%s: supported formats:"), program_name);
      for (m = format_names; m->name != NULL; m++)
	fprintf (stderr, " %s", m->name);
      fprintf (stderr, "\n");
      xexit (1);
    }

  return m->format;
}

/* Work out a format type given a file name.  If INPUT is non-zero,
   it's OK to look at the file itself.  */

static enum res_format
format_from_filename (const char *filename, int input)
{
  const char *ext;
  FILE *e;
  bfd_byte b1, b2, b3, b4, b5;
  int magic;

  /* If we have an extension, see if we recognize it as implying a
     particular format.  */
  ext = strrchr (filename, '.');
  if (ext != NULL)
    {
      const struct format_map *m;

      ++ext;
      for (m = format_fileexts; m->name != NULL; m++)
	if (strcasecmp (m->name, ext) == 0)
	  return m->format;
    }

  /* If we don't recognize the name of an output file, assume it's a
     COFF file.  */
  if (! input)
    return RES_FORMAT_COFF;

  /* Read the first few bytes of the file to see if we can guess what
     it is.  */
  e = fopen (filename, FOPEN_RB);
  if (e == NULL)
    fatal ("%s: %s", filename, strerror (errno));

  b1 = getc (e);
  b2 = getc (e);
  b3 = getc (e);
  b4 = getc (e);
  b5 = getc (e);

  fclose (e);

  /* A PE executable starts with 0x4d 0x5a.  */
  if (b1 == 0x4d && b2 == 0x5a)
    return RES_FORMAT_COFF;

  /* A COFF .o file starts with a COFF magic number.  */
  magic = (b2 << 8) | b1;
  switch (magic)
    {
    case 0x14c: /* i386 */
    case 0x166: /* MIPS */
    case 0x184: /* Alpha */
    case 0x268: /* 68k */
    case 0x1f0: /* PowerPC */
    case 0x290: /* PA */
      return RES_FORMAT_COFF;
    }

  /* A RES file starts with 0x0 0x0 0x0 0x0 0x20 0x0 0x0 0x0.  */
  if (b1 == 0 && b2 == 0 && b3 == 0 && b4 == 0 && b5 == 0x20)
    return RES_FORMAT_RES;

  /* If every character is printable or space, assume it's an RC file.  */
  if ((ISPRINT (b1) || ISSPACE (b1))
      && (ISPRINT (b2) || ISSPACE (b2))
      && (ISPRINT (b3) || ISSPACE (b3))
      && (ISPRINT (b4) || ISSPACE (b4))
      && (ISPRINT (b5) || ISSPACE (b5)))
    return RES_FORMAT_RC;

  /* Otherwise, we give up.  */
  fatal (_("can not determine type of file `%s'; use the -J option"),
	 filename);

  /* Return something to silence the compiler warning.  */
  return RES_FORMAT_UNKNOWN;
}

/* Print a usage message and exit.  */

static void
usage (FILE *stream, int status)
{
  fprintf (stream, _("Usage: %s [option(s)] [input-file] [output-file]\n"),
	   program_name);
  fprintf (stream, _(" The options are:\n\
  -i --input=<file>            Name input file\n\
  -o --output=<file>           Name output file\n\
  -J --input-format=<format>   Specify input format\n\
  -O --output-format=<format>  Specify output format\n\
  -F --target=<target>         Specify COFF target\n\
     --preprocessor=<program>  Program to use to preprocess rc file\n\
  -I --include-dir=<dir>       Include directory when preprocessing rc file\n\
  -D --define <sym>[=<val>]    Define SYM when preprocessing rc file\n\
  -U --undefine <sym>          Undefine SYM when preprocessing rc file\n\
  -v --verbose                 Verbose - tells you what it's doing\n\
  -c --codepage=<codepage>     Specify default codepage\n\
  -l --language=<val>          Set language when reading rc file\n\
     --use-temp-file           Use a temporary file instead of popen to read\n\
                               the preprocessor output\n\
     --no-use-temp-file        Use popen (default)\n"));
#ifdef YYDEBUG
  fprintf (stream, _("\
     --yydebug                 Turn on parser debugging\n"));
#endif
  fprintf (stream, _("\
  -r                           Ignored for compatibility with rc\n\
  @<file>                      Read options from <file>\n\
  -h --help                    Print this help message\n\
  -V --version                 Print version information\n"));
  fprintf (stream, _("\
FORMAT is one of rc, res, or coff, and is deduced from the file name\n\
extension if not specified.  A single file name is an input file.\n\
No input-file is stdin, default rc.  No output-file is stdout, default rc.\n"));

  list_supported_targets (program_name, stream);

  if (REPORT_BUGS_TO[0] && status == 0)
    fprintf (stream, _("Report bugs to %s\n"), REPORT_BUGS_TO);

  exit (status);
}

/* Quote characters that will confuse the shell when we run the preprocessor.  */

static const char *
quot (const char *string)
{
  static char *buf = 0;
  static int buflen = 0;
  int slen = strlen (string);
  const char *src;
  char *dest;

  if ((buflen < slen * 2 + 2) || ! buf)
    {
      buflen = slen * 2 + 2;
      if (buf)
	free (buf);
      buf = (char *) xmalloc (buflen);
    }

  for (src=string, dest=buf; *src; src++, dest++)
    {
      if (*src == '(' || *src == ')' || *src == ' ')
	*dest++ = '\\';
      *dest = *src;
    }
  *dest = 0;
  return buf;
}

/* Long options.  */

/* 150 isn't special; it's just an arbitrary non-ASCII char value.  */

#define OPTION_PREPROCESSOR	150
#define OPTION_USE_TEMP_FILE	(OPTION_PREPROCESSOR + 1)
#define OPTION_NO_USE_TEMP_FILE	(OPTION_USE_TEMP_FILE + 1)
#define OPTION_YYDEBUG		(OPTION_NO_USE_TEMP_FILE + 1)

static const struct option long_options[] =
{
  {"input", required_argument, 0, 'i'},
  {"output", required_argument, 0, 'o'},
  {"input-format", required_argument, 0, 'J'},
  {"output-format", required_argument, 0, 'O'},
  {"target", required_argument, 0, 'F'},
  {"preprocessor", required_argument, 0, OPTION_PREPROCESSOR},
  {"include-dir", required_argument, 0, 'I'},
  {"define", required_argument, 0, 'D'},
  {"undefine", required_argument, 0, 'U'},
  {"verbose", no_argument, 0, 'v'},
  {"codepage", required_argument, 0, 'c'},
  {"language", required_argument, 0, 'l'},
  {"use-temp-file", no_argument, 0, OPTION_USE_TEMP_FILE},
  {"no-use-temp-file", no_argument, 0, OPTION_NO_USE_TEMP_FILE},
  {"yydebug", no_argument, 0, OPTION_YYDEBUG},
  {"version", no_argument, 0, 'V'},
  {"help", no_argument, 0, 'h'},
  {0, no_argument, 0, 0}
};

/* This keeps gcc happy when using -Wmissing-prototypes -Wstrict-prototypes.  */
int main (int, char **);

/* The main function.  */

int
main (int argc, char **argv)
{
  int c;
  char *input_filename;
  char *output_filename;
  enum res_format input_format;
  enum res_format input_format_tmp;
  enum res_format output_format;
  char *target;
  char *preprocessor;
  char *preprocargs;
  const char *quotedarg;
  int language;
  rc_res_directory *resources;
  int use_temp_file;

#if defined (HAVE_SETLOCALE) && defined (HAVE_LC_MESSAGES)
  setlocale (LC_MESSAGES, "");
#endif
#if defined (HAVE_SETLOCALE)
  setlocale (LC_CTYPE, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  program_name = argv[0];
  xmalloc_set_program_name (program_name);

  expandargv (&argc, &argv);

  bfd_init ();
  set_default_bfd_target ();

  res_init ();

  input_filename = NULL;
  output_filename = NULL;
  input_format = RES_FORMAT_UNKNOWN;
  output_format = RES_FORMAT_UNKNOWN;
  target = NULL;
  preprocessor = NULL;
  preprocargs = NULL;
  language = 0x409;   /* LANG_ENGLISH, SUBLANG_ENGLISH_US.  */
  use_temp_file = 0;

  while ((c = getopt_long (argc, argv, "c:f:i:l:o:I:J:O:F:D:U:rhHvV", long_options,
			   (int *) 0)) != EOF)
    {
      switch (c)
	{
	case 'c':
	  {
	    rc_uint_type ncp;

	    if (optarg[0] == '0' && (optarg[1] == 'x' || optarg[1] == 'X'))
	      ncp = (rc_uint_type) strtol (optarg + 2, NULL, 16);
	    else
	      ncp = (rc_uint_type) strtol (optarg, NULL, 10);
	    if (ncp == CP_UTF16 || ! unicode_is_valid_codepage (ncp))
	      fatal (_("invalid codepage specified.\n"));
	    wind_default_codepage = wind_current_codepage = ncp;
	  }
	  break;

	case 'i':
	  input_filename = optarg;
	  break;

	case 'f':
	  /* For compatibility with rc we accept "-fo <name>" as being the
	     equivalent of "-o <name>".  We do not advertise this fact
	     though, as we do not want users to use non-GNU like command
	     line switches.  */
	  if (*optarg != 'o')
	    fatal (_("invalid option -f\n"));
	  optarg++;
	  if (* optarg == 0)
	    {
	      if (optind == argc)
		fatal (_("No filename following the -fo option.\n"));
	      optarg = argv [optind++];
	    }
	  /* Fall through.  */

	case 'o':
	  output_filename = optarg;
	  break;

	case 'J':
	  input_format = format_from_name (optarg, 1);
	  break;

	case 'O':
	  output_format = format_from_name (optarg, 1);
	  break;

	case 'F':
	  target = optarg;
	  break;

	case OPTION_PREPROCESSOR:
	  preprocessor = optarg;
	  break;

	case 'D':
	case 'U':
	  if (preprocargs == NULL)
	    {
	      quotedarg = quot (optarg);
	      preprocargs = xmalloc (strlen (quotedarg) + 3);
	      sprintf (preprocargs, "-%c%s", c, quotedarg);
	    }
	  else
	    {
	      char *n;

	      quotedarg = quot (optarg);
	      n = xmalloc (strlen (preprocargs) + strlen (quotedarg) + 4);
	      sprintf (n, "%s -%c%s", preprocargs, c, quotedarg);
	      free (preprocargs);
	      preprocargs = n;
	    }
	  break;

	case 'r':
	  /* Ignored for compatibility with rc.  */
	  break;

	case 'v':
	  verbose ++;
	  break;

	case 'I':
	  /* For backward compatibility, should be removed in the future.  */
	  input_format_tmp = format_from_name (optarg, 0);
	  if (input_format_tmp != RES_FORMAT_UNKNOWN)
	    {
	      fprintf (stderr,
	      	       _("Option -I is deprecated for setting the input format, please use -J instead.\n"));
	      input_format = input_format_tmp;
	      break;
	    }

	  if (preprocargs == NULL)
	    {
	      quotedarg = quot (optarg);
	      preprocargs = xmalloc (strlen (quotedarg) + 3);
	      sprintf (preprocargs, "-I%s", quotedarg);
	    }
	  else
	    {
	      char *n;

	      quotedarg = quot (optarg);
	      n = xmalloc (strlen (preprocargs) + strlen (quotedarg) + 4);
	      sprintf (n, "%s -I%s", preprocargs, quotedarg);
	      free (preprocargs);
	      preprocargs = n;
	    }

	  {
	    struct include_dir *n, **pp;

	    n = (struct include_dir *) xmalloc (sizeof *n);
	    n->next = NULL;
	    n->dir = optarg;

	    for (pp = &include_dirs; *pp != NULL; pp = &(*pp)->next)
	      ;
	    *pp = n;
	  }

	  break;

	case 'l':
	  language = strtol (optarg, (char **) NULL, 16);
	  break;

	case OPTION_USE_TEMP_FILE:
	  use_temp_file = 1;
	  break;

	case OPTION_NO_USE_TEMP_FILE:
	  use_temp_file = 0;
	  break;

#ifdef YYDEBUG
	case OPTION_YYDEBUG:
	  yydebug = 1;
	  break;
#endif

	case 'h':
	case 'H':
	  usage (stdout, 0);
	  break;

	case 'V':
	  print_version ("windres");
	  break;

	default:
	  usage (stderr, 1);
	  break;
	}
    }

  if (input_filename == NULL && optind < argc)
    {
      input_filename = argv[optind];
      ++optind;
    }

  if (output_filename == NULL && optind < argc)
    {
      output_filename = argv[optind];
      ++optind;
    }

  if (argc != optind)
    usage (stderr, 1);

  if (input_format == RES_FORMAT_UNKNOWN)
    {
      if (input_filename == NULL)
	input_format = RES_FORMAT_RC;
      else
	input_format = format_from_filename (input_filename, 1);
    }

  if (output_format == RES_FORMAT_UNKNOWN)
    {
      if (output_filename == NULL)
	output_format = RES_FORMAT_RC;
      else
	output_format = format_from_filename (output_filename, 0);
    }

  set_endianess (NULL, target);

  /* Read the input file.  */
  switch (input_format)
    {
    default:
      abort ();
    case RES_FORMAT_RC:
      resources = read_rc_file (input_filename, preprocessor, preprocargs,
				language, use_temp_file);
      break;
    case RES_FORMAT_RES:
      resources = read_res_file (input_filename);
      break;
    case RES_FORMAT_COFF:
      resources = read_coff_rsrc (input_filename, target);
      break;
    }

  if (resources == NULL)
    fatal (_("no resources"));

  /* Sort the resources.  This is required for COFF, convenient for
     rc, and unimportant for res.  */
  resources = sort_resources (resources);

  /* Write the output file.  */
  reswr_init ();

  switch (output_format)
    {
    default:
      abort ();
    case RES_FORMAT_RC:
      write_rc_file (output_filename, resources);
      break;
    case RES_FORMAT_RES:
      write_res_file (output_filename, resources);
      break;
    case RES_FORMAT_COFF:
      write_coff_file (output_filename, target, resources);
      break;
    }

  xexit (0);
  return 0;
}

static void set_endianess (bfd *abfd, const char *target)
{
  const bfd_target *target_vec;

  def_target_arch = NULL;
  target_vec = bfd_find_target (target, abfd);
  if (! target_vec)
    fatal ("Can't detect target endianess and architecture.");
  target_is_bigendian = ((target_vec->byteorder == BFD_ENDIAN_BIG) ? 1 : 0);
  {
    const char *tname = target_vec->name;
    const char **arch = bfd_arch_list();
    if (arch && tname)
      {
	if (strchr (tname, '-') != NULL)
	  tname = strchr (tname, '-') + 1;
	while (*arch != NULL)
	  {
	    const char *in_a = strstr (*arch, tname);
	    char end_ch = (in_a ? in_a[strlen(tname)] : 0);
	    if (in_a && (in_a == *arch || in_a[-1] == ':')
	        && end_ch == 0)
	      {
		def_target_arch = *arch;
		break;
	      }
	    arch++;
	  }
      }
    if (! def_target_arch)
      fatal ("Can't detect architecture.");
  }
}

bfd *
windres_open_as_binary (const char *filename, int rdmode)
{
  bfd *abfd;

  abfd = (rdmode ? bfd_openr (filename, "binary") : bfd_openw (filename, "binary"));
  if (! abfd)
    fatal ("can't open `%s' for %s", filename, (rdmode ? "input" : "output"));

  if (rdmode && ! bfd_check_format (abfd, bfd_object))
    fatal ("can't open `%s' for input.", filename);
  
  return abfd;
}

void
set_windres_bfd_endianess (windres_bfd *wrbfd, int is_bigendian)
{
  assert (!! wrbfd);
  switch (WR_KIND(wrbfd))
  {
  case WR_KIND_BFD_BIN_L:
    if (is_bigendian)
      WR_KIND(wrbfd) = WR_KIND_BFD_BIN_B;
    break;
  case WR_KIND_BFD_BIN_B:
    if (! is_bigendian)
      WR_KIND(wrbfd) = WR_KIND_BFD_BIN_L;
    break;
  default:
    /* only binary bfd can be overriden. */
    abort ();
  }
}

void
set_windres_bfd (windres_bfd *wrbfd, bfd *abfd, asection *sec, rc_uint_type kind)
{
  assert (!! wrbfd);
  switch (kind)
  {
  case WR_KIND_TARGET:
    abfd = NULL;
    sec = NULL;
    break;
  case WR_KIND_BFD:
  case WR_KIND_BFD_BIN_L:
  case WR_KIND_BFD_BIN_B:
    assert (!! abfd);
    assert (!!sec);
    break;
  default:
    abort ();
  }
  WR_KIND(wrbfd) = kind;
  WR_BFD(wrbfd) = abfd;
  WR_SECTION(wrbfd) = sec;
}

void
set_windres_bfd_content(windres_bfd *wrbfd, const void *data, rc_uint_type off,
			rc_uint_type length)
{
  if (WR_KIND(wrbfd) != WR_KIND_TARGET)
    {
      if (! bfd_set_section_contents (WR_BFD(wrbfd), WR_SECTION(wrbfd), data, off, length))
	bfd_fatal ("bfd_set_section_contents");
    }
  else
    abort ();
}

void
get_windres_bfd_content(windres_bfd *wrbfd, void *data, rc_uint_type off,
			rc_uint_type length)
{
  if (WR_KIND(wrbfd) != WR_KIND_TARGET)
    {
      if (! bfd_get_section_contents (WR_BFD(wrbfd), WR_SECTION(wrbfd), data, off, length))
	bfd_fatal ("bfd_get_section_contents");
    }
  else
    abort ();
}

void
windres_put_8 (windres_bfd *wrbfd, void *p, rc_uint_type value)
{
  switch (WR_KIND(wrbfd))
    {
    case WR_KIND_TARGET:
      target_put_8 (p, value);
      break;
    case WR_KIND_BFD:
    case WR_KIND_BFD_BIN_L:
    case WR_KIND_BFD_BIN_B:
      bfd_put_8 (WR_BFD(wrbfd), value, p);
      break;
    default:
      abort ();
    }
}

void
windres_put_16 (windres_bfd *wrbfd, void *data, rc_uint_type value)
{
  switch (WR_KIND(wrbfd))
    {
    case WR_KIND_TARGET:
      target_put_16 (data, value);
      break;
    case WR_KIND_BFD:
    case WR_KIND_BFD_BIN_B:
      bfd_put_16 (WR_BFD(wrbfd), value, data);
      break;
    case WR_KIND_BFD_BIN_L:
      bfd_putl16 (value, data);
      break;
    default:
      abort ();
    }
}

void
windres_put_32 (windres_bfd *wrbfd, void *data, rc_uint_type value)
{
  switch (WR_KIND(wrbfd))
    {
    case WR_KIND_TARGET:
      target_put_32 (data, value);
      break;
    case WR_KIND_BFD:
    case WR_KIND_BFD_BIN_B:
      bfd_put_32 (WR_BFD(wrbfd), value, data);
      break;
    case WR_KIND_BFD_BIN_L:
      bfd_putl32 (value, data);
      break;
    default:
      abort ();
    }
}

rc_uint_type
windres_get_8 (windres_bfd *wrbfd, const void *data, rc_uint_type length)
{
  if (length < 1)
    fatal ("windres_get_8: unexpected eob.");
  switch (WR_KIND(wrbfd))
    {
    case WR_KIND_TARGET:
      return target_get_8 (data, length);
    case WR_KIND_BFD:
    case WR_KIND_BFD_BIN_B:
    case WR_KIND_BFD_BIN_L:
      return bfd_get_8 (WR_BFD(wrbfd), data);
    default:
      abort ();
    }
  return 0;
}

rc_uint_type
windres_get_16 (windres_bfd *wrbfd, const void *data, rc_uint_type length)
{
  if (length < 2)
    fatal ("windres_get_16: unexpected eob.");
  switch (WR_KIND(wrbfd))
    {
    case WR_KIND_TARGET:
      return target_get_16 (data, length);
    case WR_KIND_BFD:
    case WR_KIND_BFD_BIN_B:
      return bfd_get_16 (WR_BFD(wrbfd), data);
    case WR_KIND_BFD_BIN_L:
      return bfd_getl16 (data);
    default:
      abort ();
    }
  return 0;
}

rc_uint_type
windres_get_32 (windres_bfd *wrbfd, const void *data, rc_uint_type length)
{
  if (length < 4)
    fatal ("windres_get_32: unexpected eob.");
  switch (WR_KIND(wrbfd))
    {
    case WR_KIND_TARGET:
      return target_get_32 (data, length);
    case WR_KIND_BFD:
    case WR_KIND_BFD_BIN_B:
      return bfd_get_32 (WR_BFD(wrbfd), data);
    case WR_KIND_BFD_BIN_L:
      return bfd_getl32 (data);
    default:
      abort ();
    }
  return 0;
}

static rc_uint_type
target_get_8 (const void *p, rc_uint_type length)
{
  rc_uint_type ret;
  
  if (length < 1)
    fatal ("Resource too small for getting 8-bit value.");

  ret = (rc_uint_type) *((const bfd_byte *) p);
  return ret & 0xff;
}

static rc_uint_type
target_get_16 (const void *p, rc_uint_type length)
{
  if (length < 2)
    fatal ("Resource too small for getting 16-bit value.");
  
  if (target_is_bigendian)
    return bfd_getb16 (p);
  else
    return bfd_getl16 (p);
}

static rc_uint_type
target_get_32 (const void *p, rc_uint_type length)
{
  if (length < 4)
    fatal ("Resource too small for getting 32-bit value.");
  
  if (target_is_bigendian)
    return bfd_getb32 (p);
  else
    return bfd_getl32 (p);
}

static void
target_put_8 (void *p, rc_uint_type value)
{
  assert (!! p);
  *((bfd_byte *) p)=(bfd_byte) value;
}

static void
target_put_16 (void *p, rc_uint_type value)
{
  assert (!! p);
  
  if (target_is_bigendian)
    bfd_putb16 (value, p);
  else
    bfd_putl16 (value, p);
}

static void
target_put_32 (void *p, rc_uint_type value)
{
  assert (!! p);
  
  if (target_is_bigendian)
    bfd_putb32 (value, p);
  else
    bfd_putl32 (value, p);
}

static int isInComment = 0;

int wr_printcomment (FILE *e, const char *fmt, ...)
{
  va_list arg;
  int r = 0;

  if (isInComment)
    r += fprintf (e, "\n   ");
  else
    fprintf (e, "/* ");
  isInComment = 1;
  if (fmt == NULL)
    return r;
  va_start (arg, fmt);
  r += vfprintf (e, fmt, arg);
  va_end (arg);
  return r;
}

int wr_print (FILE *e, const char *fmt, ...)
{
  va_list arg;
  int r = 0;
  if (isInComment)
    r += fprintf (e, ".  */\n");
  isInComment = 0;
  if (! fmt)
    return r;
  va_start (arg, fmt);
  r += vfprintf (e, fmt, arg);
  va_end (arg);
  return r;    
}
