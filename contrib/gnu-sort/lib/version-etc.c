/* Utility to help print --version output in a consistent format.
   Copyright (C) 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Jim Meyering. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include "unlocked-io.h"
#include "version-etc.h"

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define _(Text) Text
#endif

/* Default copyright goes to the FSF. */

char* version_etc_copyright =
  /* Do *not* mark this string for translation.  */
  "Copyright (C) 2002 Free Software Foundation, Inc.";


/* Display the --version information the standard way.

   If COMMAND_NAME is NULL, the PACKAGE is asumed to be the name of
   the program.  The formats are therefore:

   PACKAGE VERSION

   or

   COMMAND_NAME (PACKAGE) VERSION.  */
void
version_etc (FILE *stream,
	     const char *command_name, const char *package,
	     const char *version, const char *authors)
{
  if (command_name)
    fprintf (stream, "%s (%s) %s\n", command_name, package, version);
  else
    fprintf (stream, "%s %s\n", package, version);
  fprintf (stream, _("Written by %s.\n"), authors);
  putc ('\n', stream);

  fputs (version_etc_copyright, stream);
  putc ('\n', stream);

  fputs (_("\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"),
	 stream);
}
