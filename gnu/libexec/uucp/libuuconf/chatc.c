/* chatc.c
   Subroutines to handle chat script commands.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_chatc_rcsid[] = "$Id: chatc.c,v 1.2 1994/05/07 18:12:01 ache Exp $";
#endif

#include <ctype.h>
#include <errno.h>

static int icchat P((pointer pglobal, int argc, char **argv,
		     pointer pvar, pointer pinfo));
static int icchat_fail P((pointer pglobal, int argc, char **argv,
			  pointer pvar, pointer pinfo));
static int icunknown P((pointer pglobal, int argc, char **argv,
			pointer pvar, pointer pinfo));

/* The chat script commands.  */

static const struct cmdtab_offset asChat_cmds[] =
{
  { "chat", UUCONF_CMDTABTYPE_FN,
      offsetof (struct uuconf_chat, uuconf_pzchat), icchat },
  { "chat-program", UUCONF_CMDTABTYPE_FULLSTRING,
      offsetof (struct uuconf_chat, uuconf_pzprogram), NULL },
  { "chat-timeout", UUCONF_CMDTABTYPE_INT,
      offsetof (struct uuconf_chat, uuconf_ctimeout), NULL },
  { "chat-fail", UUCONF_CMDTABTYPE_FN | 2,
      offsetof (struct uuconf_chat, uuconf_pzfail), icchat_fail },
  { "chat-seven-bit", UUCONF_CMDTABTYPE_BOOLEAN,
      offsetof (struct uuconf_chat, uuconf_fstrip), NULL },
  { NULL, 0, 0, NULL }
};

#define CCHAT_CMDS (sizeof asChat_cmds / sizeof asChat_cmds[0])

/* Handle a chat script command.  The chat script commands are entered
   as UUCONF_CMDTABTYPE_PREFIX, and the commands are routed to this
   function.  We copy the command table onto the stack and repoint it
   at qchat in order to make the function reentrant.  The return value
   can include UUCONF_CMDTABRET_KEEP, but should not include
   UUCONF_CMDTABRET_EXIT.  */

int
_uuconf_ichat_cmd (qglobal, argc, argv, qchat, pblock)
     struct sglobal *qglobal;
     int argc;
     char **argv;
     struct uuconf_chat *qchat;
     pointer pblock;
{
  char *zchat;
  struct uuconf_cmdtab as[CCHAT_CMDS];
  int iret;

  /* This is only invoked when argv[0] will contain the string "chat";
     the specific chat script command comes after that point.  */
  for (zchat = argv[0]; *zchat != '\0'; zchat++)
    if ((*zchat == 'c' || *zchat == 'C')
	&& strncasecmp (zchat, "chat", sizeof "chat" - 1) == 0)
      break;
  if (*zchat == '\0')
    return UUCONF_SYNTAX_ERROR;
  argv[0] = zchat;

  _uuconf_ucmdtab_base (asChat_cmds, CCHAT_CMDS, (char *) qchat, as);

  iret = uuconf_cmd_args ((pointer) qglobal, argc, argv, as, pblock,
			  icunknown, 0, pblock);
  return iret &~ UUCONF_CMDTABRET_EXIT;
}

/* Handle the "chat" command.  This breaks up substrings in expect
   strings, and sticks the arguments into a NULL terminated array.  */

static int
icchat (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char ***ppz = (char ***) pvar;
  pointer pblock = pinfo;
  int i;

  *ppz = NULL;

  for (i = 1; i < argc; i += 2)
    {
      char *z, *zdash;
      int iret;

      /* Break the expect string into substrings.  */
      z = argv[i];
      zdash = strchr (z, '-');
      while (zdash != NULL)
	{
	  *zdash = '\0';
	  iret = _uuconf_iadd_string (qglobal, z, TRUE, FALSE, ppz,
				      pblock);
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	  *zdash = '-';
	  z = zdash;
	  zdash = strchr (z + 1, '-');
	}

      iret = _uuconf_iadd_string (qglobal, z, FALSE, FALSE, ppz, pblock);
      if (iret != UUCONF_SUCCESS)
	return iret;

      /* Add the send string without breaking it up.  If it starts
	 with a dash we must replace it with an escape sequence, to
	 prevent it from being interpreted as a subsend.  */

      if (i + 1 < argc)
	{
	  if (argv[i + 1][0] != '-')
	    iret = _uuconf_iadd_string (qglobal, argv[i + 1], FALSE,
					FALSE, ppz, pblock);
	  else
	    {
	      size_t clen;

	      clen = strlen (argv[i + 1]);
	      z = uuconf_malloc (pblock, clen + 2);
	      if (z == NULL)
		{
		  qglobal->ierrno = errno;
		  return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
		}
	      z[0] = '\\';
	      memcpy ((pointer) (z + 1), (pointer) argv[i + 1], clen + 1);
	      iret = _uuconf_iadd_string (qglobal, z, FALSE, FALSE, ppz,
					  pblock);
	    }
	  if (iret != UUCONF_SUCCESS)
	    return iret;
	}
    }

  return UUCONF_CMDTABRET_KEEP;
}

/* Add a new chat failure string.  */

/*ARGSUSED*/
static int
icchat_fail (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char ***ppz = (char ***) pvar;
  pointer pblock = pinfo;

  return _uuconf_iadd_string (qglobal, argv[1], TRUE, FALSE, ppz, pblock);
}

/* Return a syntax error for an unknown command.  */

/*ARGSUSED*/
static int
icunknown (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  return UUCONF_SYNTAX_ERROR;
}
