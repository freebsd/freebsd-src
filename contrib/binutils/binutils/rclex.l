%{ /* rclex.l -- lexer for Windows rc files parser  */
/* Copyright 1997, 1998 Free Software Foundation, Inc.
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* This is a lex input file which generates a lexer used by the
   Windows rc file parser.  It basically just recognized a bunch of
   keywords.  */

#include "bfd.h"
#include "bucomm.h"
#include "libiberty.h"
#include "windres.h"
#include "rcparse.h"

#include <ctype.h>
#include <assert.h>

/* Whether we are in rcdata mode, in which we returns the lengths of
   strings.  */

static int rcdata_mode;

/* List of allocated strings.  */

struct alloc_string
{
  struct alloc_string *next;
  char *s;
};

static struct alloc_string *strings;

/* Local functions.  */

static void cpp_line PARAMS ((const char *));
static char *handle_quotes PARAMS ((const char *, unsigned long *));
static char *get_string PARAMS ((int));

%}

%%

"BEGIN"			{ return BEG; }
"{"			{ return BEG; }
"END"			{ return END; }
"}"			{ return END; }
"ACCELERATORS"		{ return ACCELERATORS; }
"VIRTKEY"		{ return VIRTKEY; }
"ASCII"			{ return ASCII; }
"NOINVERT"		{ return NOINVERT; }
"SHIFT"			{ return SHIFT; }
"CONTROL"		{ return CONTROL; }
"ALT"			{ return ALT; }
"BITMAP"		{ return BITMAP; }
"CURSOR"		{ return CURSOR; }
"DIALOG"		{ return DIALOG; }
"DIALOGEX"		{ return DIALOGEX; }
"EXSTYLE"		{ return EXSTYLE; }
"CAPTION"		{ return CAPTION; }
"CLASS"			{ return CLASS; }
"STYLE"			{ return STYLE; }
"AUTO3STATE"		{ return AUTO3STATE; }
"AUTOCHECKBOX"		{ return AUTOCHECKBOX; }
"AUTORADIOBUTTON"	{ return AUTORADIOBUTTON; }
"CHECKBOX"		{ return CHECKBOX; }
"COMBOBOX"		{ return COMBOBOX; }
"CTEXT"			{ return CTEXT; }
"DEFPUSHBUTTON"		{ return DEFPUSHBUTTON; }
"EDITTEXT"		{ return EDITTEXT; }
"GROUPBOX"		{ return GROUPBOX; }
"LISTBOX"		{ return LISTBOX; }
"LTEXT"			{ return LTEXT; }
"PUSHBOX"		{ return PUSHBOX; }
"PUSHBUTTON"		{ return PUSHBUTTON; }
"RADIOBUTTON"		{ return RADIOBUTTON; }
"RTEXT"			{ return RTEXT; }
"SCROLLBAR"		{ return SCROLLBAR; }
"STATE3"		{ return STATE3; }
"USERBUTTON"		{ return USERBUTTON; }
"BEDIT"			{ return BEDIT; }
"HEDIT"			{ return HEDIT; }
"IEDIT"			{ return IEDIT; }
"FONT"			{ return FONT; }
"ICON"			{ return ICON; }
"LANGUAGE"		{ return LANGUAGE; }
"CHARACTERISTICS"	{ return CHARACTERISTICS; }
"VERSION"		{ return VERSIONK; }
"MENU"			{ return MENU; }
"MENUEX"		{ return MENUEX; }
"MENUITEM"		{ return MENUITEM; }
"SEPARATOR"		{ return SEPARATOR; }
"POPUP"			{ return POPUP; }
"CHECKED"		{ return CHECKED; }
"GRAYED"		{ return GRAYED; }
"HELP"			{ return HELP; }
"INACTIVE"		{ return INACTIVE; }
"MENUBARBREAK"		{ return MENUBARBREAK; }
"MENUBREAK"		{ return MENUBREAK; }
"MESSAGETABLE"		{ return MESSAGETABLE; }
"RCDATA"		{ return RCDATA; }
"STRINGTABLE"		{ return STRINGTABLE; }
"VERSIONINFO"		{ return VERSIONINFO; }
"FILEVERSION"		{ return FILEVERSION; }
"PRODUCTVERSION"	{ return PRODUCTVERSION; }
"FILEFLAGSMASK"		{ return FILEFLAGSMASK; }
"FILEFLAGS"		{ return FILEFLAGS; }
"FILEOS"		{ return FILEOS; }
"FILETYPE"		{ return FILETYPE; }
"FILESUBTYPE"		{ return FILESUBTYPE; }
"VALUE"			{ return VALUE; }
"MOVEABLE"		{ return MOVEABLE; }
"FIXED"			{ return FIXED; }
"PURE"			{ return PURE; }
"IMPURE"		{ return IMPURE; }
"PRELOAD"		{ return PRELOAD; }
"LOADONCALL"		{ return LOADONCALL; }
"DISCARDABLE"		{ return DISCARDABLE; }
"NOT"			{ return NOT; }

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
			    return BLOCKSTRINGFILEINFO;
			  else if (strncmp (s, "VarFileInfo",
					    sizeof "VarFileInfo" - 1) == 0
				   && s + sizeof "VarFileInfo" - 1 == send)
			    return BLOCKVARFILEINFO;
			  else
			    {
			      char *r;

			      r = get_string (send - s + 1);
			      strncpy (r, s, send - s);
			      r[send - s] = '\0';
			      yylval.s = r;
			      return BLOCK;
			    }
			}

"#"[^\n]*		{
			  cpp_line (yytext);
			}

[0-9][x0-9A-Fa-f]*L	{
			  yylval.i.val = strtoul (yytext, 0, 0);
			  yylval.i.dword = 1;
			  return NUMBER;
			}

[0-9][x0-9A-Fa-f]*	{
			  yylval.i.val = strtoul (yytext, 0, 0);
			  yylval.i.dword = 0;
			  return NUMBER;
			}

("\""[^\"\n]*"\""[ \t]*)+ {
			  char *s;
			  unsigned long length;

			  s = handle_quotes (yytext, &length);
			  if (! rcdata_mode)
			    {
			      yylval.s = s;
			      return QUOTEDSTRING;
			    }
			  else
			    {
			      yylval.ss.length = length;
			      yylval.ss.s = s;
			      return SIZEDSTRING;
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
			  return STRING;
			}

[\n]			{ ++rc_lineno; }
[ \t\r]+		{ /* ignore whitespace */ }
.			{ return *yytext; }

%%
#ifndef yywrap
/* This is needed for some versions of lex.  */
int yywrap ()
{
  return 1;
}
#endif

/* Handle a C preprocessor line.  */

static void
cpp_line (s)
     const char *s;
{
  int line;
  char *send, *fn;

  ++s;
  while (isspace (*s))
    ++s;
  
  line = strtol (s, &send, 0);
  if (*send != '\0' && ! isspace (*send))
    return;

  /* Subtract 1 because we are about to count the newline.  */
  rc_lineno = line - 1;

  s = send;
  while (isspace (*s))
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
}

/* Handle a quoted string.  The quotes are stripped.  A pair of quotes
   in a string are turned into a single quote.  Adjacent strings are
   merged separated by whitespace are merged, as in C.  */

static char *
handle_quotes (input, len)
     const char *input;
     unsigned long *len;
{
  char *ret, *s;
  const char *t;
  int ch;

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
	      *s++ = ESCAPE_A;
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
	      while (1)
		{
		  if (*t >= '0' && *t <= '9')
		    ch = (ch << 4) | (*t - '0');
		  else if (*t >= 'a' && *t <= 'f')
		    ch = (ch << 4) | (*t - 'a');
		  else if (*t >= 'A' && *t <= 'F')
		    ch = (ch << 4) | (*t - 'A');
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
	  assert (isspace (*t));
	  while (isspace (*t))
	    ++t;
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
get_string (len)
     int len;
{
  struct alloc_string *as;

  as = (struct alloc_string *) xmalloc (sizeof *as);
  as->s = xmalloc (len);

  as->next = strings;
  strings = as->next;

  return as->s;
}

/* Discard all the strings we have allocated.  The parser calls this
   when it no longer needs them.  */

void
rcparse_discard_strings ()
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
rcparse_rcdata ()
{
  rcdata_mode = 1;
}

/* Go back to normal mode from rcdata mode.  */

void
rcparse_normal ()
{
  rcdata_mode = 0;
}
