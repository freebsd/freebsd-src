%{ /* rclex.l -- lexer for Windows rc files parser  */
/* Copyright 1997, 1998, 1999, 2001, 2002, 2003, 2005
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

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

/* This is a lex input file which generates a lexer used by the
   Windows rc file parser.  It basically just recognized a bunch of
   keywords.  */

#include "bfd.h"
#include "bucomm.h"
#include "libiberty.h"
#include "safe-ctype.h"
#include "windres.h"
#include "rcparse.h"

#include <assert.h>

#define YY_NO_UNPUT

/* Whether we are in rcdata mode, in which we returns the lengths of
   strings.  */

static int rcdata_mode;

/* Whether we are supressing lines from cpp (including windows.h or
   headers from your C sources may bring in externs and typedefs).
   When active, we return IGNORED_TOKEN, which lets us ignore these
   outside of resource constructs.  Thus, it isn't required to protect
   all the non-preprocessor lines in your header files with #ifdef
   RC_INVOKED.  It also means your RC file can't include other RC
   files if they're named "*.h".  Sorry.  Name them *.rch or whatever.  */

static int suppress_cpp_data;

#define MAYBE_RETURN(x) return suppress_cpp_data ? IGNORED_TOKEN : (x)

/* The first filename we detect in the cpp output.  We use this to
   tell included files from the original file.  */

static char *initial_fn;

/* List of allocated strings.  */

struct alloc_string
{
  struct alloc_string *next;
  char *s;
};

static struct alloc_string *strings;

/* Local functions.  */

static void cpp_line (const char *);
static char *handle_quotes (const char *, unsigned long *);
static char *get_string (int);

%}

%%

"BEGIN"			{ MAYBE_RETURN (BEG); }
"{"			{ MAYBE_RETURN (BEG); }
"END"			{ MAYBE_RETURN (END); }
"}"			{ MAYBE_RETURN (END); }
"ACCELERATORS"		{ MAYBE_RETURN (ACCELERATORS); }
"VIRTKEY"		{ MAYBE_RETURN (VIRTKEY); }
"ASCII"			{ MAYBE_RETURN (ASCII); }
"NOINVERT"		{ MAYBE_RETURN (NOINVERT); }
"SHIFT"			{ MAYBE_RETURN (SHIFT); }
"CONTROL"		{ MAYBE_RETURN (CONTROL); }
"ALT"			{ MAYBE_RETURN (ALT); }
"BITMAP"		{ MAYBE_RETURN (BITMAP); }
"CURSOR"		{ MAYBE_RETURN (CURSOR); }
"DIALOG"		{ MAYBE_RETURN (DIALOG); }
"DIALOGEX"		{ MAYBE_RETURN (DIALOGEX); }
"EXSTYLE"		{ MAYBE_RETURN (EXSTYLE); }
"CAPTION"		{ MAYBE_RETURN (CAPTION); }
"CLASS"			{ MAYBE_RETURN (CLASS); }
"STYLE"			{ MAYBE_RETURN (STYLE); }
"AUTO3STATE"		{ MAYBE_RETURN (AUTO3STATE); }
"AUTOCHECKBOX"		{ MAYBE_RETURN (AUTOCHECKBOX); }
"AUTORADIOBUTTON"	{ MAYBE_RETURN (AUTORADIOBUTTON); }
"CHECKBOX"		{ MAYBE_RETURN (CHECKBOX); }
"COMBOBOX"		{ MAYBE_RETURN (COMBOBOX); }
"CTEXT"			{ MAYBE_RETURN (CTEXT); }
"DEFPUSHBUTTON"		{ MAYBE_RETURN (DEFPUSHBUTTON); }
"EDITTEXT"		{ MAYBE_RETURN (EDITTEXT); }
"GROUPBOX"		{ MAYBE_RETURN (GROUPBOX); }
"LISTBOX"		{ MAYBE_RETURN (LISTBOX); }
"LTEXT"			{ MAYBE_RETURN (LTEXT); }
"PUSHBOX"		{ MAYBE_RETURN (PUSHBOX); }
"PUSHBUTTON"		{ MAYBE_RETURN (PUSHBUTTON); }
"RADIOBUTTON"		{ MAYBE_RETURN (RADIOBUTTON); }
"RTEXT"			{ MAYBE_RETURN (RTEXT); }
"SCROLLBAR"		{ MAYBE_RETURN (SCROLLBAR); }
"STATE3"		{ MAYBE_RETURN (STATE3); }
"USERBUTTON"		{ MAYBE_RETURN (USERBUTTON); }
"BEDIT"			{ MAYBE_RETURN (BEDIT); }
"HEDIT"			{ MAYBE_RETURN (HEDIT); }
"IEDIT"			{ MAYBE_RETURN (IEDIT); }
"FONT"			{ MAYBE_RETURN (FONT); }
"ICON"			{ MAYBE_RETURN (ICON); }
"LANGUAGE"		{ MAYBE_RETURN (LANGUAGE); }
"CHARACTERISTICS"	{ MAYBE_RETURN (CHARACTERISTICS); }
"VERSION"		{ MAYBE_RETURN (VERSIONK); }
"MENU"			{ MAYBE_RETURN (MENU); }
"MENUEX"		{ MAYBE_RETURN (MENUEX); }
"MENUITEM"		{ MAYBE_RETURN (MENUITEM); }
"SEPARATOR"		{ MAYBE_RETURN (SEPARATOR); }
"POPUP"			{ MAYBE_RETURN (POPUP); }
"CHECKED"		{ MAYBE_RETURN (CHECKED); }
"GRAYED"		{ MAYBE_RETURN (GRAYED); }
"HELP"			{ MAYBE_RETURN (HELP); }
"INACTIVE"		{ MAYBE_RETURN (INACTIVE); }
"MENUBARBREAK"		{ MAYBE_RETURN (MENUBARBREAK); }
"MENUBREAK"		{ MAYBE_RETURN (MENUBREAK); }
"MESSAGETABLE"		{ MAYBE_RETURN (MESSAGETABLE); }
"RCDATA"		{ MAYBE_RETURN (RCDATA); }
"STRINGTABLE"		{ MAYBE_RETURN (STRINGTABLE); }
"VERSIONINFO"		{ MAYBE_RETURN (VERSIONINFO); }
"FILEVERSION"		{ MAYBE_RETURN (FILEVERSION); }
"PRODUCTVERSION"	{ MAYBE_RETURN (PRODUCTVERSION); }
"FILEFLAGSMASK"		{ MAYBE_RETURN (FILEFLAGSMASK); }
"FILEFLAGS"		{ MAYBE_RETURN (FILEFLAGS); }
"FILEOS"		{ MAYBE_RETURN (FILEOS); }
"FILETYPE"		{ MAYBE_RETURN (FILETYPE); }
"FILESUBTYPE"		{ MAYBE_RETURN (FILESUBTYPE); }
"VALUE"			{ MAYBE_RETURN (VALUE); }
"MOVEABLE"		{ MAYBE_RETURN (MOVEABLE); }
"FIXED"			{ MAYBE_RETURN (FIXED); }
"PURE"			{ MAYBE_RETURN (PURE); }
"IMPURE"		{ MAYBE_RETURN (IMPURE); }
"PRELOAD"		{ MAYBE_RETURN (PRELOAD); }
"LOADONCALL"		{ MAYBE_RETURN (LOADONCALL); }
"DISCARDABLE"		{ MAYBE_RETURN (DISCARDABLE); }
"NOT"			{ MAYBE_RETURN (NOT); }

"BLOCK"[ \t\n]*"\""[^\#\n]*"\"" {
			  char *s, *send;

			  /* This is a hack to let us parse version
                             information easily.  */

			  s = strchr (yytext, '"');
			  ++s;
			  send = strchr (s, '"');
			  if (strncmp (s, "StringFileInfo",
				       sizeof "StringFileInfo" - 1) == 0
			      && s + sizeof "StringFileInfo" - 1 == send)
			    MAYBE_RETURN (BLOCKSTRINGFILEINFO);
			  else if (strncmp (s, "VarFileInfo",
					    sizeof "VarFileInfo" - 1) == 0
				   && s + sizeof "VarFileInfo" - 1 == send)
			    MAYBE_RETURN (BLOCKVARFILEINFO);
			  else
			    {
			      char *r;

			      r = get_string (send - s + 1);
			      strncpy (r, s, send - s);
			      r[send - s] = '\0';
			      yylval.s = r;
			      MAYBE_RETURN (BLOCK);
			    }
			}

"#"[^\n]*		{
			  cpp_line (yytext);
			}

[0-9][x0-9A-Fa-f]*L	{
			  yylval.i.val = strtoul (yytext, 0, 0);
			  yylval.i.dword = 1;
			  MAYBE_RETURN (NUMBER);
			}

[0-9][x0-9A-Fa-f]*	{
			  yylval.i.val = strtoul (yytext, 0, 0);
			  yylval.i.dword = 0;
			  MAYBE_RETURN (NUMBER);
			}

("\""[^\"\n]*"\""[ \t\n]*)+ {
			  char *s;
			  unsigned long length;

			  s = handle_quotes (yytext, &length);
			  if (! rcdata_mode)
			    {
			      yylval.s = s;
			      MAYBE_RETURN (QUOTEDSTRING);
			    }
			  else
			    {
			      yylval.ss.length = length;
			      yylval.ss.s = s;
			      MAYBE_RETURN (SIZEDSTRING);
			    }
			}

[A-Za-z][^ ,\t\r\n]*	{
			  char *s;

			  /* I rejected comma in a string in order to
			     handle VIRTKEY, CONTROL in an accelerator
			     resource.  This means that an unquoted
			     file name can not contain a comma.  I
			     don't know what rc permits.  */

			  s = get_string (strlen (yytext) + 1);
			  strcpy (s, yytext);
			  yylval.s = s;
			  MAYBE_RETURN (STRING);
			}

[\n]			{ ++rc_lineno; }
[ \t\r]+		{ /* ignore whitespace */ }
.			{ MAYBE_RETURN (*yytext); }

%%
#ifndef yywrap
/* This is needed for some versions of lex.  */
int yywrap (void)
{
  return 1;
}
#endif

/* Handle a C preprocessor line.  */

static void
cpp_line (const char *s)
{
  int line;
  char *send, *fn;

  ++s;
  while (ISSPACE (*s))
    ++s;
  
  line = strtol (s, &send, 0);
  if (*send != '\0' && ! ISSPACE (*send))
    return;

  /* Subtract 1 because we are about to count the newline.  */
  rc_lineno = line - 1;

  s = send;
  while (ISSPACE (*s))
    ++s;

  if (*s != '"')
    return;

  ++s;
  send = strchr (s, '"');
  if (send == NULL)
    return;

  fn = (char *) xmalloc (send - s + 1);
  strncpy (fn, s, send - s);
  fn[send - s] = '\0';

  free (rc_filename);
  rc_filename = fn;

  if (!initial_fn)
    {
      initial_fn = xmalloc (strlen (fn) + 1);
      strcpy (initial_fn, fn);
    }

  /* Allow the initial file, regardless of name.  Suppress all other
     files if they end in ".h" (this allows included "*.rc").  */
  if (strcmp (initial_fn, fn) == 0
      || strcmp (fn + strlen (fn) - 2, ".h") != 0)
    suppress_cpp_data = 0;
  else
    suppress_cpp_data = 1;
}

/* Handle a quoted string.  The quotes are stripped.  A pair of quotes
   in a string are turned into a single quote.  Adjacent strings are
   merged separated by whitespace are merged, as in C.  */

static char *
handle_quotes (const char *input, unsigned long *len)
{
  char *ret, *s;
  const char *t;
  int ch;
  int num_xdigits;

  ret = get_string (strlen (input) + 1);

  s = ret;
  t = input;
  if (*t == '"')
    ++t;
  while (*t != '\0')
    {
      if (*t == '\\')
	{
	  ++t;
	  switch (*t)
	    {
	    case '\0':
	      rcparse_warning ("backslash at end of string");
	      break;

	    case '\"':
	      rcparse_warning ("use \"\" to put \" in a string");
	      break;

	    case 'a':
	      *s++ = ESCAPE_B; /* Strange, but true...  */
	      ++t;
	      break;

	    case 'b':
	      *s++ = ESCAPE_B;
	      ++t;
	      break;

	    case 'f':
	      *s++ = ESCAPE_F;
	      ++t;
	      break;

	    case 'n':
	      *s++ = ESCAPE_N;
	      ++t;
	      break;

	    case 'r':
	      *s++ = ESCAPE_R;
	      ++t;
	      break;

	    case 't':
	      *s++ = ESCAPE_T;
	      ++t;
	      break;

	    case 'v':
	      *s++ = ESCAPE_V;
	      ++t;
	      break;

	    case '\\':
	      *s++ = *t++;
	      break;

	    case '0': case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
	      ch = *t - '0';
	      ++t;
	      if (*t >= '0' && *t <= '7')
		{
		  ch = (ch << 3) | (*t - '0');
		  ++t;
		  if (*t >= '0' && *t <= '7')
		    {
		      ch = (ch << 3) | (*t - '0');
		      ++t;
		    }
		}
	      *s++ = ch;
	      break;

	    case 'x':
	      ++t;
	      ch = 0;
	      /* We only handle single byte chars here.  Make sure
		 we finish an escape sequence like "/xB0ABC" after
		 the first two digits.  */
              num_xdigits = 2;
 	      while (num_xdigits--)
		{
		  if (*t >= '0' && *t <= '9')
		    ch = (ch << 4) | (*t - '0');
		  else if (*t >= 'a' && *t <= 'f')
		    ch = (ch << 4) | (*t - 'a' + 10);
		  else if (*t >= 'A' && *t <= 'F')
		    ch = (ch << 4) | (*t - 'A' + 10);
		  else
		    break;
		  ++t;
		}
	      *s++ = ch;
	      break;

	    default:
	      rcparse_warning ("unrecognized escape sequence");
	      *s++ = '\\';
	      *s++ = *t++;
	      break;
	    }
	}
      else if (*t != '"')
	*s++ = *t++;
      else if (t[1] == '\0')
	break;
      else if (t[1] == '"')
	{
	  *s++ = '"';
	  t += 2;
	}
      else
	{
	  ++t;
	  assert (ISSPACE (*t));
	  while (ISSPACE (*t))
	    {
	      if ((*t) == '\n')
		++rc_lineno;
	      ++t;
	    }
	  if (*t == '\0')
	    break;
	  assert (*t == '"');
	  ++t;
	}
    }

  *s = '\0';

  *len = s - ret;

  return ret;
}

/* Allocate a string of a given length.  */

static char *
get_string (int len)
{
  struct alloc_string *as;

  as = (struct alloc_string *) xmalloc (sizeof *as);
  as->s = xmalloc (len);

  as->next = strings;
  strings = as;

  return as->s;
}

/* Discard all the strings we have allocated.  The parser calls this
   when it no longer needs them.  */

void
rcparse_discard_strings (void)
{
  struct alloc_string *as;

  as = strings;
  while (as != NULL)
    {
      struct alloc_string *n;

      free (as->s);
      n = as->next;
      free (as);
      as = n;
    }

  strings = NULL;
}

/* Enter rcdata mode.  */

void
rcparse_rcdata (void)
{
  rcdata_mode = 1;
}

/* Go back to normal mode from rcdata mode.  */

void
rcparse_normal (void)
{
  rcdata_mode = 0;
}
