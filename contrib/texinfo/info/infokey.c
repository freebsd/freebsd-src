/* infokey.c -- compile ~/.infokey to ~/.info.
   $Id: infokey.c,v 1.5 2002/02/26 16:17:57 karl Exp $

   Copyright (C) 1999, 2001, 02 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Andrew Bettison <andrewb@zip.com.au>. */

#include "info.h"
#include "infomap.h"
#include "infokey.h"
#include "key.h"
#include "getopt.h"

static char *program_name = "infokey";

/* Non-zero means print version info only. */
static int print_version_p = 0;

/* Non-zero means print a short description of the options. */
static int print_help_p = 0;

/* String specifying the source file.  This is set by the user on the
   command line, or a default is used. */
static char *input_filename = (char *) NULL;

/* String specifying the name of the file to output to.  This is
   set by the user on the command line, or a default is used. */
static char *output_filename = (char *) NULL;

/* Structure describing the options that Infokey accepts.  We pass this
   structure to getopt_long ().  If you add or otherwise change this
   structure, you must also change the string which follows it. */
static struct option long_options[] =
{
  {"output", 1, 0, 'o'},
  {"help", 0, &print_help_p, 1},
  {"version", 0, &print_version_p, 1},
  {NULL, 0, NULL, 0}
};

/* String describing the shorthand versions of the long options found above. */
static char *short_options = "o:";

/* Structure for holding the compiled sections. */
enum sect_e
  {
    info = 0,
    ea = 1,
    var = 2,
  };
struct sect
  {
    unsigned int cur;
    unsigned char data[INFOKEY_MAX_SECTIONLEN];
  };

/* Some "forward" declarations. */
static char *mkpath ();
static int compile (), write_infokey_file ();
static void syntax_error (), error_message (), suggest_help (), short_help ();


/* **************************************************************** */
/*                                                                  */
/*             Main Entry Point to the Infokey Program              */
/*                                                                  */
/* **************************************************************** */

int
main (argc, argv)
     int argc;
     char **argv;
{
  int getopt_long_index;	/* Index returned by getopt_long (). */
  NODE *initial_node;		/* First node loaded by Info. */

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  while (1)
    {
      int option_character;

      option_character = getopt_long
	(argc, argv, short_options, long_options, &getopt_long_index);

      /* getopt_long () returns EOF when there are no more long options. */
      if (option_character == EOF)
	break;

      /* If this is a long option, then get the short version of it. */
      if (option_character == 0 && long_options[getopt_long_index].flag == 0)
	option_character = long_options[getopt_long_index].val;

      /* Case on the option that we have received. */
      switch (option_character)
	{
	case 0:
	  break;

	  /* User is specifying the name of a file to output to. */
	case 'o':
	  if (output_filename)
	    free (output_filename);
	  output_filename = xstrdup (optarg);
	  break;

	default:
	  suggest_help ();
	  xexit (1);
	}
    }

  /* If the user specified --version, then show the version and exit. */
  if (print_version_p)
    {
      printf ("%s (GNU %s) %s\n", program_name, PACKAGE, VERSION);
      puts ("");
      printf (_ ("Copyright (C) %s Free Software Foundation, Inc.\n\
There is NO warranty.  You may redistribute this software\n\
under the terms of the GNU General Public License.\n\
For more information about these matters, see the files named COPYING.\n"),
	      "1999");
      xexit (0);
    }

  /* If the `--help' option was present, show the help and exit. */
  if (print_help_p)
    {
      short_help ();
      xexit (0);
    }

  /* If there is one argument remaining, it is the name of the input
     file. */
  if (optind == argc - 1)
    {
      if (input_filename)
	free (input_filename);
      input_filename = xstrdup (argv[optind]);
    }
  else if (optind != argc)
    {
      error_message (0, _("incorrect number of arguments"));
      suggest_help ();
      xexit (1);
    }

  /* Use default filenames where none given. */
  {
    char *homedir;

    homedir = getenv ("HOME");
#ifdef __MSDOS__
    if (!homedir)
      homedir = ".";
#endif
    if (!input_filename)
      input_filename = mkpath (homedir, INFOKEY_SRCFILE);
    if (!output_filename)
      output_filename = mkpath (homedir, INFOKEY_FILE);
  }

  {
    FILE *inf;
    FILE *outf;
    int write_error;
    static struct sect sections[3];

    /* Open the input file. */
    inf = fopen (input_filename, "r");
    if (!inf)
      {
	error_message (errno, _("cannot open input file `%s'"), input_filename);
	xexit (1);
      }

    /* Compile the input file to its verious sections, then write the
       section data to the output file. */

    if (compile (inf, input_filename, sections))
      {
	/* Open the output file. */
	outf = fopen (output_filename, FOPEN_WBIN);
	if (!outf)
	  {
	    error_message (errno, _("cannot create output file `%s'"), output_filename);
	    xexit (1);
	  }

	/* Write the contents of the output file and close it.  If there is
	   an error writing to the file, delete it and exit with a failure
	   status.  */
	write_error = 0;
	if (!write_infokey_file (outf, sections))
	  {
	    error_message (errno, _("error writing to `%s'"), output_filename);
	    write_error = 1;
	  }
	if (fclose (outf) == EOF)
	  {
	    error_message (errno, _("error closing output file `%s'"), output_filename);
	    write_error = 1;
	  }
	if (write_error)
	  {
	    unlink (output_filename);
	    xexit (1);
	  }
      }

    /* Close the input file. */
    fclose (inf);
  }

  xexit (0);
}

static char *
mkpath (dir, file)
     const char *dir;
     const char *file;
{
  char *p;

  p = xmalloc (strlen (dir) + 1 + strlen (file) + 2);
  strcpy (p, dir);
  strcat (p, "/");
  strcat (p, file);
  return p;
}


/* Compilation - the real work.

	Source file syntax
	------------------
	The source file is a line-based text file with the following
	structure:

		# comments
		# more comments

		#info
		u	prev-line
		d	next-line
		^a	invalid		# just beep
		\ku	prev-line
		#stop
		\kd	next-line
		q	quit		# of course!

		#echo-area
		^a	echo-area-beg-of-line
		^e	echo-area-end-of-line
		\kr	echo-area-forward
		\kl	echo-area-backward
		\kh	echo-area-beg-of-line
		\ke	echo-area-end-of-line

		#var
		scroll-step=1
		ISO-Latin=Off

	Lines starting with '#' are comments, and are ignored.  Blank
	lines are ignored.  Each section is introduced by one of the
	following lines:

		#info
		#echo-area
		#var
	
	The sections may occur in any order.  Each section may be
	omitted completely.  If the 'info' section is the first in the
	file, its '#info' line may be omitted.
	
	The 'info' and 'echo-area' sections
	-----------------------------------
	Each line in the 'info' or 'echo-area' sections has the
	following syntax:

		key-sequence SPACE action-name [ SPACE [ # comment ] ] \n
	
	Where SPACE is one or more white space characters excluding
	newline, "action-name" is the name of a GNU Info command,
	"comment" is any sequence of characters excluding newline, and
	"key-sequence" is a concatenation of one or more key definitions
	using the following syntax:

	   1.	A carat ^ followed by one character indicates a single
	   	control character;

	   2.	A backslash \ followed by one, two, or three octal
		digits indicates a single character having that ASCII
		code;

	   3.	\n indicates a single NEWLINE;
		\e indicates a single ESC;
		\r indicates a single CR;
		\t indicates a single TAB;
		\b indicates a single BACKSPACE;
	
	   4.	\ku indicates the Up Arrow key;
	   	\kd indicates the Down Arrow key;
	   	\kl indicates the Left Arrow key;
	   	\kr indicates the Right Arrow key;
	   	\kP indicates the Page Up (PRIOR) key;
	   	\kN indicates the Page Down (NEXT) key;
	   	\kh indicates the Home key;
	   	\ke indicates the End key;
	   	\kx indicates the DEL key;
		\k followed by any other character indicates a single
		control-K, and the following character is interpreted
		as in rules 1, 2, 3, 5 and 6.

	   5.	\m followed by any sequence defined in rules 1, 2, 3, 4
		or 6 indicates the "Meta" modification of that key.

	   6.	A backslash \ followed by any character not described
	   	above indicates that character itself.  In particular:
		\\ indicates a single backslash \,
		\  (backslash-space) indicates a single space,
		\^ indicates a single caret ^,

	If the following line:

		#stop
	
	occurs anywhere in an 'info' or 'echo-area' section, that
	indicates to GNU Info to suppress all of its default key
	bindings in that context.
	
	The 'var' section
	-----------------
	Each line in the 'var' section has the following syntax:

		variable-name = value \n
	
	Where "variable-name" is the name of a GNU Info variable and
	"value" is the value that GNU Info will assign to that variable
	when commencing execution.  There must be no white space in the
	variable name, nor between the variable name and the '='.  All
	characters immediately following the '=', up to but not
	including the terminating newline, are considered to be the
	value that will be assigned.  In other words, white space
	following the '=' is not ignored.
 */

static int add_to_section (), lookup_action ();

/* Compile the input file into its various sections.  Return true if no
   error was encountered.
 */
static int
compile (fp, filename, sections)
     FILE *fp;
     const char *filename;
     struct sect sections[];
{
  int error = 0;
  char rescan = 0;
  unsigned int lnum = 0;
  int c;

  /* This parser is a true state machine, with no sneaky fetching
     of input characters inside the main loop.  In other words, all
     state is fully represented by the following variables:
   */
  enum
    {
      start_of_line,
      start_of_comment,
      in_line_comment,
      in_trailing_comment,
      get_keyseq,
      got_keyseq,
      get_action,
      got_action,
      get_varname,
      got_varname,
      get_equals,
      got_equals,
      get_value,
    }
  state = start_of_line;
  enum sect_e section = info;
  enum
    {
      normal,
      slosh,
      control,
      octal,
      special_key,
    }
  seqstate;			/* used if state == get_keyseq */
  char meta = 0;
  char ocnt;			/* used if state == get_keyseq && seqstate == octal */

  /* Data is accumulated in the following variables.  The code
     avoids overflowing these strings, and throws an error
     where appropriate if a string limit is exceeded.  These string
     lengths are arbitrary (and should be large enough) and their
     lengths are not hard-coded anywhere else, so increasing them
     here will not break anything.  */
  char oval;
  char comment[10];
  unsigned int clen;
  char seq[20];
  unsigned int slen;
  char act[80];
  unsigned int alen;
  char varn[80];
  unsigned int varlen;
  char val[80];
  unsigned int vallen;

#define	To_seq(c) \
		  do { \
		    if (slen < sizeof seq) \
		      seq[slen++] = meta ? Meta(c) : (c); \
		    else \
		      { \
			syntax_error(filename, lnum, _("key sequence too long")); \
			error = 1; \
		      } \
		    meta = 0; \
		  } while (0)

  sections[info].cur = 1;
  sections[info].data[0] = 0;
  sections[ea].cur = 1;
  sections[ea].data[0] = 0;
  sections[var].cur = 0;

  while (!error && (rescan || (c = fgetc (fp)) != EOF))
    {
      rescan = 0;
      switch (state)
	{
	case start_of_line:
	  lnum++;
	  if (c == '#')
	    state = start_of_comment;
	  else if (c != '\n')
	    {
	      switch (section)
		{
		case info:
		case ea:
		  state = get_keyseq;
		  seqstate = normal;
		  slen = 0;
		  break;
		case var:
		  state = get_varname;
		  varlen = 0;
		  break;
		}
	      rescan = 1;
	    }
	  break;

	case start_of_comment:
	  clen = 0;
	  state = in_line_comment;
	  /* fall through */
	case in_line_comment:
	  if (c == '\n')
	    {
	      state = start_of_line;
	      comment[clen] = '\0';
	      if (strcmp (comment, "info") == 0)
		section = info;
	      else if (strcmp (comment, "echo-area") == 0)
		section = ea;
	      else if (strcmp (comment, "var") == 0)
		section = var;
	      else if (strcmp (comment, "stop") == 0
		       && (section == info || section == ea))
		sections[section].data[0] = 1;
	    }
	  else if (clen < sizeof comment - 1)
	    comment[clen++] = c;
	  break;

	case in_trailing_comment:
	  if (c == '\n')
	    state = start_of_line;
	  break;

	case get_keyseq:
	  switch (seqstate)
	    {
	    case normal:
	      if (c == '\n' || isspace (c))
		{
		  state = got_keyseq;
		  rescan = 1;
		  if (slen == 0)
		    {
		      syntax_error (filename, lnum, _("missing key sequence"));
		      error = 1;
		    }
		}
	      else if (c == '\\')
		seqstate = slosh;
	      else if (c == '^')
		seqstate = control;
	      else
		To_seq (c);
	      break;

	    case slosh:
	      switch (c)
		{
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
		  seqstate = octal;
		  oval = c - '0';
		  ocnt = 1;
		  break;
		case 'b':
		  To_seq ('\b');
		  seqstate = normal;
		  break;
		case 'e':
		  To_seq ('\033');
		  seqstate = normal;
		  break;
		case 'n':
		  To_seq ('\n');
		  seqstate = normal;
		  break;
		case 'r':
		  To_seq ('\r');
		  seqstate = normal;
		  break;
		case 't':
		  To_seq ('\t');
		  seqstate = normal;
		  break;
		case 'm':
		  meta = 1;
		  seqstate = normal;
		  break;
		case 'k':
		  seqstate = special_key;
		  break;
		default:
		  /* Backslash followed by any other char 
		     just means that char.  */
		  To_seq (c);
		  seqstate = normal;
		  break;
		}
	      break;

	    case octal:
	      switch (c)
		{
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
		  if (++ocnt <= 3)
		    oval = oval * 8 + c - '0';
		  if (ocnt == 3)
		    seqstate = normal;
		  break;
		default:
		  ocnt = 4;
		  seqstate = normal;
		  rescan = 1;
		  break;
		}
	      if (seqstate != octal)
		{
		  if (oval)
		    To_seq (oval);
		  else
		    {
		      syntax_error (filename, lnum, _("NUL character (\\000) not permitted"));
		      error = 1;
		    }
		}
	      break;

	    case special_key:
	      To_seq (SK_ESCAPE);
	      switch (c)
		{
		case 'u': To_seq (SK_UP_ARROW); break;
		case 'd': To_seq (SK_DOWN_ARROW); break;
		case 'r': To_seq (SK_RIGHT_ARROW); break;
		case 'l': To_seq (SK_LEFT_ARROW); break;
		case 'U': To_seq (SK_PAGE_UP); break;
		case 'D': To_seq (SK_PAGE_DOWN); break;
		case 'h': To_seq (SK_HOME); break;
		case 'e': To_seq (SK_END); break;
		case 'x': To_seq (SK_DELETE); break;
		default:  To_seq (SK_LITERAL); rescan = 1; break;
		}
	      seqstate = normal;
	      break;

	    case control:
	      if (CONTROL (c))
		To_seq (CONTROL (c));
	      else
		{
		  syntax_error (filename, lnum, _("NUL character (^%c) not permitted"), c);
		  error = 1;
		}
	      seqstate = normal;
	      break;
	    }
	  break;

	case got_keyseq:
	  if (isspace (c) && c != '\n')
	    break;
	  state = get_action;
	  alen = 0;
	  /* fall through */
	case get_action:
	  if (c == '\n' || isspace (c))
	    {
	      int a;

	      state = got_action;
	      rescan = 1;
	      if (alen == 0)
		{
		  syntax_error (filename, lnum, _("missing action name"), c);
		  error = 1;
		}
	      else
		{
		  act[alen] = '\0';
		  a = lookup_action (act);
		  if (a != -1)
		    {
		      char av = a;

		      if (!(add_to_section (&sections[section], seq, slen)
			    && add_to_section (&sections[section], "", 1)
			    && add_to_section (&sections[section], &av, 1)))
			{
			  syntax_error (filename, lnum, _("section too long"));
			  error = 1;
			}
		    }
		  else
		    {
		      syntax_error (filename, lnum, _("unknown action `%s'"), act);
		      error = 1;
		    }
		}
	    }
	  else if (alen < sizeof act - 1)
	    act[alen++] = c;
	  else
	    {
	      syntax_error (filename, lnum, _("action name too long"));
	      error = 1;
	    }
	  break;
	
	case got_action:
	  if (c == '#')
	    state = in_trailing_comment;
	  else if (c == '\n')
	    state = start_of_line;
	  else if (!isspace (c))
	    {
	      syntax_error (filename, lnum, _("extra characters following action `%s'"), act);
	      error = 1;
	    }
	  break;

	case get_varname:
	  if (c == '=')
	    {
	      if (varlen == 0)
		{
		  syntax_error (filename, lnum, _("missing variable name"));
		  error = 1;
		}
	      state = get_value;
	      vallen = 0;
	    }
	  else if (c == '\n' || isspace (c))
	    {
	      syntax_error (filename, lnum, _("missing `=' immediately after variable name"));
	      error = 1;
	    }
	  else if (varlen < sizeof varn)
	    varn[varlen++] = c;
	  else
	    {
	      syntax_error (filename, lnum, _("variable name too long"));
	      error = 1;
	    }
	  break;
	
	case get_value:
	  if (c == '\n')
	    {
	      state = start_of_line;
	      if (!(add_to_section (&sections[section], varn, varlen)
		    && add_to_section (&sections[section], "", 1)
		    && add_to_section (&sections[section], val, vallen)
		    && add_to_section (&sections[section], "", 1)))
		{
		  syntax_error (filename, lnum, _("section too long"));
		  error = 1;
		}
	    }
	  else if (vallen < sizeof val)
	    val[vallen++] = c;
	  else
	    {
	      syntax_error (filename, lnum, _("value too long"));
	      error = 1;
	    }
	  break;
	}
    }

#undef To_seq

  return !error;
}

/* Add some characters to a section's data.  Return true if all the
   characters fit, or false if the section's size limit was exceeded.
 */
static int
add_to_section (s, str, len)
     struct sect *s;
     const char *str;
     unsigned int len;
{
  if (s->cur + len > sizeof s->data)
    return 0;
  strncpy (s->data + s->cur, str, len);
  s->cur += len;
  return 1;
}

/* Translate from an action name to its numeric code.  This uses the
   auto-generated array in key.c.
 */
static int
lookup_action (actname)
     const char *actname;
{
  int i;

  if (strcmp ("invalid", actname) == 0)
    return A_INVALID;
  for (i = 0; function_key_array[i].name != NULL; i++)
    if (strcmp (function_key_array[i].name, actname) == 0)
      return function_key_array[i].code;
  return -1;
}

/* Put an integer to an infokey file.
   Integers are stored as two bytes, low order first,
   in radix INFOKEY_RADIX.
 */
static int
putint (i, fp)
     int i;
     FILE *fp;
{
  return fputc (i % INFOKEY_RADIX, fp) != EOF
    && fputc ((i / INFOKEY_RADIX) % INFOKEY_RADIX, fp) != EOF;
}

/* Write an entire section to an infokey file.  If the section is
   empty, simply omit it.
 */
static int
putsect (s, code, fp)
     struct sect *s;
     int code;
     FILE *fp;
{
  if (s->cur == 0)
    return 1;
  return fputc (code, fp) != EOF
    && putint (s->cur, fp)
    && fwrite (s->data, s->cur, 1, fp) == 1;
}

/* Write an entire infokey file, given an array containing its sections.
 */
static int
write_infokey_file (fp, sections)
     FILE *fp;
     struct sect sections[];
{
  /* Get rid of sections with no effect. */
  if (sections[info].cur == 1 && sections[info].data[0] == 0)
    sections[info].cur = 0;
  if (sections[ea].cur == 1 && sections[ea].data[0] == 0)
    sections[ea].cur = 0;

  /* Write all parts of the file out in order (no lseeks),
     checking for errors all the way. */
  return fputc (INFOKEY_MAGIC_S0, fp) != EOF
    && fputc (INFOKEY_MAGIC_S1, fp) != EOF
    && fputc (INFOKEY_MAGIC_S2, fp) != EOF
    && fputc (INFOKEY_MAGIC_S3, fp) != EOF
    && fputs (VERSION, fp) != EOF
    && fputc ('\0', fp) != EOF
    && putsect (&sections[info], INFOKEY_SECTION_INFO, fp)
    && putsect (&sections[ea], INFOKEY_SECTION_EA, fp)
    && putsect (&sections[var], INFOKEY_SECTION_VAR, fp)
    && fputc (INFOKEY_MAGIC_E0, fp) != EOF
    && fputc (INFOKEY_MAGIC_E1, fp) != EOF
    && fputc (INFOKEY_MAGIC_E2, fp) != EOF
    && fputc (INFOKEY_MAGIC_E3, fp) != EOF;
}


/* Error handling. */

/* Give the user a "syntax error" message in the form
	progname: "filename", line N: message
 */
static void
error_message (error_code, fmt, a1, a2, a3, a4)
     int error_code;
     const char *fmt;
     const void *a1, *a2, *a3, *a4;
{
  fprintf (stderr, "%s: ", program_name);
  fprintf (stderr, fmt, a1, a2, a3, a4);
  if (error_code)
    fprintf (stderr, " - %s", strerror (error_code));
  fprintf (stderr, "\n");
}

/* Give the user a generic error message in the form
	progname: message
 */
static void
syntax_error (filename, linenum, fmt, a1, a2, a3, a4)
     const char *filename;
     unsigned int linenum;
     const char *fmt;
     const void *a1, *a2, *a3, *a4;
{
  fprintf (stderr, "%s: ", program_name);
  fprintf (stderr, _("\"%s\", line %u: "), filename, linenum);
  fprintf (stderr, fmt, a1, a2, a3, a4);
  fprintf (stderr, "\n");
}

/* Produce a gentle rtfm. */
static void
suggest_help ()
{
  fprintf (stderr, _("Try --help for more information.\n"));
}

/* Produce a scaled down description of the available options to Info. */
static void
short_help ()
{
  printf (_ ("\
Usage: %s [OPTION]... [INPUT-FILE]\n\
\n\
Compile infokey source file to infokey file.  Reads INPUT-FILE (default\n\
$HOME/.infokey) and writes compiled key file to (by default) $HOME/.info.\n\
\n\
Options:\n\
  --output FILE        output to FILE instead of $HOME/.info\n\
  --help               display this help and exit.\n\
  --version            display version information and exit.\n\
\n\
Email bug reports to bug-texinfo@gnu.org,\n\
general questions and discussion to help-texinfo@gnu.org.\n\
"),
	  program_name
    );
  xexit (0);
}
