/* chat.c
   Chat routine for the UUCP package.

   Copyright (C) 1991, 1992, 1993, 1995 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#if USE_RCS_ID
const char chat_rcsid[] = "$FreeBSD$";
#endif

#include <ctype.h>
#include <errno.h>

#include "uudefs.h"
#include "uuconf.h"
#include "conn.h"
#include "prot.h"
#include "system.h"

/* Local functions.  */

static int icexpect P((struct sconnection *qconn, int cstrings,
		       char **azstrings, size_t *aclens,
		       int ctimeout, boolean fstrip));
static boolean fcsend P((struct sconnection *qconn, pointer puuconf,
			 const char *zsend,
			 const struct uuconf_system *qsys,
			 const struct uuconf_dialer *qdial,
			 const char *zphone,
			 boolean ftranslate, boolean fstrip));
static boolean fcecho_send_strip P((struct sconnection *qconn,
				    const char *z, size_t clen));
static boolean fcecho_send_nostrip P((struct sconnection  *qconn,
				      const char *z, size_t clen));
static boolean fcecho_send P((struct sconnection *qconn, const char *z,
			      size_t clen, boolean fstrip));
static boolean fcphone P((struct sconnection *qconn,
			  pointer puuconf,
			  const struct uuconf_dialer *qdial,
			  const char *zphone,
			  boolean (*pfwrite) P((struct sconnection *qc,
						const char *zwrite,
						size_t cwrite)),
			  boolean ftranslate, boolean *pfquote));
static boolean fctranslate P((pointer puuconf, const char *zphone,
			      const char **pzprefix,
			      const char **pzsuffix));
static boolean fcprogram P((struct sconnection *qconn, pointer puuconf,
			    char **pzprogram,
			    const struct uuconf_system *qsys,
			    const struct uuconf_dialer *qdial,
			    const char *zphone, const char *zport,
			    long ibaud));

/* Run a chat script with the other system.  The chat script is a
   series of expect send pairs.  We wait for the expect string to show
   up, and then we send the send string.  The chat string for a system
   holds the expect and send strings separated by a single space.  */

boolean
fchat (qconn, puuconf, qchat, qsys, qdial, zphone, ftranslate, zport, ibaud)
     struct sconnection *qconn;
     pointer puuconf;
     const struct uuconf_chat *qchat;
     const struct uuconf_system *qsys;
     const struct uuconf_dialer *qdial;
     const char *zphone;
     boolean ftranslate;
     const char *zport;
     long ibaud;
{
  int cstrings;
  char **azstrings;
  size_t *aclens;
  char **pzchat;
  char *zbuf;
  size_t cbuflen;
  boolean fret;
  int i;

  /* First run the program, if any.  */
  if (qchat->uuconf_pzprogram != NULL)
    {
      if (! fcprogram (qconn, puuconf, qchat->uuconf_pzprogram, qsys, qdial,
		       zphone, zport, ibaud))
	return FALSE;
    }

  /* If there's no chat script, we're done.  */
  if (qchat->uuconf_pzchat == NULL)
    return TRUE;

  if (qchat->uuconf_pzfail == NULL)
    {
      cstrings = 1;
      azstrings = (char **) xmalloc (sizeof (char *));
      aclens = (size_t *) xmalloc (sizeof (size_t));
    }
  else
    {
      char **pz;

      /* We leave string number 0 for the chat script.  */
      cstrings = 1;
      for (pz = qchat->uuconf_pzfail; *pz != NULL; pz++)
	++cstrings;

      azstrings = (char **) xmalloc (cstrings * sizeof (char *));
      aclens = (size_t *) xmalloc (cstrings * sizeof (size_t));

      /* Get the strings into the array, and handle all the escape
	 characters.  */
      for (cstrings = 1, pz = qchat->uuconf_pzfail;
	   *pz != NULL;
	   cstrings++, pz++)
	{
	  azstrings[cstrings] = zbufcpy (*pz);
	  aclens[cstrings] = cescape (azstrings[cstrings]);
	}
    }

  cbuflen = 0;
  zbuf = NULL;
  fret = TRUE;

  pzchat = qchat->uuconf_pzchat;

  while (*pzchat != NULL)
    {
      size_t clen;

      /* Loop over subexpects and subsends.  */
      while (TRUE)
	{
	  char *ztimeout;
	  int ctimeout;

	  /* Copy the expect string into the buffer so that we can
	     modify it in cescape.  */
	  clen = strlen (*pzchat);
	  if (clen >= cbuflen)
	    {
	      ubuffree (zbuf);
	      zbuf = zbufalc (clen + 1);
	      cbuflen = clen;
	    }
	  memcpy (zbuf, *pzchat, clen + 1);

	  azstrings[0] = zbuf;
	  if (azstrings[0][0] == '-')
	    ++azstrings[0];

	  /* \Wnum at the end of the string is a timeout.  */
	  ctimeout = qchat->uuconf_ctimeout;
	  ztimeout = strrchr (azstrings[0], '\\');
	  if (ztimeout != NULL && ztimeout[1] == 'W')
	    {
	      char *zend;
	      int cval;

	      cval = (int) strtol (ztimeout + 2, &zend, 10);
	      if (zend != ztimeout + 2 && *zend == '\0')
		{
		  ctimeout = cval;
		  *ztimeout = '\0';
		}
	    }

	  aclens[0] = cescape (azstrings[0]);

	  if (aclens[0] == 0
	      || (aclens[0] == 2
		  && strcmp (azstrings[0], "\"\"") == 0))
	    {
	      /* There is no subexpect sequence.  If there is a
		 subsend sequence we move on to it.  Otherwise we let
		 this expect succeed.  This is somewhat inconsistent,
		 but it seems to be the traditional approach.  */
	      if (pzchat[1] == NULL || pzchat[1][0] != '-')
		break;
	    }
	  else
	    {
	      int istr;

	      istr = icexpect (qconn, cstrings, azstrings, aclens,
			       ctimeout, qchat->uuconf_fstrip);

	      /* If we found the string, break out of the
		 subexpect/subsend loop.  */
	      if (istr == 0)
		break;

	      /* If we got an error, return FALSE.  */
	      if (istr < -1)
		{
		  fret = FALSE;
		  break;
		}

	      /* If we found a failure string, log it and get out.  */
	      if (istr > 0)
		{
		  ulog (LOG_ERROR, "Chat script failed: Got \"%s\"",
			qchat->uuconf_pzfail[istr - 1]);
		  fret = FALSE;
		  break;
		}

	      /* We timed out; look for a send subsequence.  If none,
		 the chat script has failed.  */
	      if (pzchat[1] == NULL || pzchat[1][0] != '-')
		{
		  ulog (LOG_ERROR, "Timed out in chat script");
		  fret = FALSE;
		  break;
		}
	    }

	  /* Send the send subsequence without the leading '-'.  A
	     \"\" will send nothing.  An empty string will send a
	     carriage return.  */
	  ++pzchat;
	  if (! fcsend (qconn, puuconf, *pzchat + 1, qsys, qdial, zphone,
			ftranslate, qchat->uuconf_fstrip))
	    {
	      fret = FALSE;
	      break;
	    }

	  /* If there is no expect subsequence, we are done.  */
	  if (pzchat[1] == NULL || pzchat[1][0] != '-')
	    break;

	  /* Move on to next expect subsequence.  */
	  ++pzchat;
	}

      if (! fret)
	break;

      /* Move on to the send string.  If there is none, we have
	 succeeded.  */
      do
	{
	  ++pzchat;
	}
      while (*pzchat != NULL && (*pzchat)[0] == '-');

      if (*pzchat == NULL)
	break;

      if (**pzchat != '\0')
	{
	  if (! fcsend (qconn, puuconf, *pzchat, qsys, qdial, zphone,
			ftranslate, qchat->uuconf_fstrip))
	    {
	      fret = FALSE;
	      break;
	    }
	}

      ++pzchat;
    }

  ubuffree (zbuf);
  for (i = 1; i < cstrings; i++)
    ubuffree (azstrings[i]);
  xfree ((pointer) azstrings);
  xfree ((pointer) aclens);

  return fret;
}

/* Read characters and wait for one of a set of memory strings to come
   in.  This returns the index into the array of the string that
   arrives, or -1 on timeout, or -2 on error.  */

static int
icexpect (qconn, cstrings, azstrings, aclens, ctimeout, fstrip)
     struct sconnection *qconn;
     int cstrings;
     char **azstrings;
     size_t *aclens;
     int ctimeout;
     boolean fstrip;
{
  int i;
  size_t cmax;
  char *zhave;
  size_t chave;
  long iendtime;
#if DEBUG > 1
  int cchars;
  int iolddebug;
#endif

  cmax = aclens[0];
  for (i = 1; i < cstrings; i++)
    if (cmax < aclens[i])
      cmax = aclens[i];

  zhave = zbufalc (cmax);
  chave = 0;

  iendtime = ixsysdep_time ((long *) NULL) + ctimeout;

#if DEBUG > 1
  cchars = 0;
  iolddebug = iDebug;
  if (FDEBUGGING (DEBUG_CHAT))
    {
      udebug_buffer ("icexpect: Looking for", azstrings[0],
		     aclens[0]);
      ulog (LOG_DEBUG_START, "icexpect: Got \"");
      iDebug &=~ (DEBUG_INCOMING | DEBUG_PORT);
    }
#endif

  while (TRUE)
    {
      int bchar;

      /* If we have no more time, get out.  */
      if (ctimeout <= 0)
	{
#if DEBUG > 1
	  if (FDEBUGGING (DEBUG_CHAT))
	    {
	      ulog (LOG_DEBUG_END, "\" (timed out)");
	      iDebug = iolddebug;
	    }
#endif
	  ubuffree (zhave);
	  return -1;
	}

      /* Read one character at a time.  We could use a more complex
	 algorithm to read in larger batches, but it's probably not
	 worth it.  If the buffer is full, shift it left; we already
	 know that no string matches, and the buffer holds the largest
	 string, so this can't lose a match.  */
      if (chave >= cmax)
	{
	  size_t imove;

	  for (imove = 0; imove < cmax - 1; imove++)
	    zhave[imove] = zhave[imove + 1];
	  --chave;
	}

      /* The timeout/error return values from breceive_char are the
	 same as for this function.  */
      bchar = breceive_char (qconn, ctimeout, TRUE);
      if (bchar < 0)
	{
#if DEBUG > 1
	  if (FDEBUGGING (DEBUG_CHAT))
	    {
	      /* If there was an error, it will probably be logged in
		 the middle of our string, but this is only debugging
		 so it's not a big deal.  */
	      ulog (LOG_DEBUG_END, "\" (%s)",
		    bchar == -1 ? "timed out" : "error");
	      iDebug = iolddebug;
	    }
#endif
	  ubuffree (zhave);
	  return bchar;
	}

      /* Strip the parity bit if desired.  */
      if (fstrip)
	bchar &= 0x7f;

      zhave[chave] = (char) bchar;
      ++chave;

#if DEBUG > 1
      if (FDEBUGGING (DEBUG_CHAT))
	{
	  char ab[5];

	  ++cchars;
	  if (cchars > 60)
	    {
	      ulog (LOG_DEBUG_END, "\"");
	      ulog (LOG_DEBUG_START, "icexpect: Got \"");
	      cchars = 0;
	    }
	  (void) cdebug_char (ab, bchar);
	  ulog (LOG_DEBUG_CONTINUE, "%s", ab);
	}
#endif

      /* See if any of the strings can be found in the buffer.  Since
	 we read one character at a time, the string can only be found
	 at the end of the buffer.  */
      for (i = 0; i < cstrings; i++)
	{
	  if (aclens[i] <= chave
	      && memcmp (zhave + chave - aclens[i], azstrings[i],
			 aclens[i]) == 0)
	    {
#if DEBUG > 1
	      if (FDEBUGGING (DEBUG_CHAT))
		{
		  if (i == 0)
		    ulog (LOG_DEBUG_END, "\" (found it)");
		  else
		    {
		      ulog (LOG_DEBUG_END, "\"");
		      udebug_buffer ("icexpect: Found", azstrings[i],
				     aclens[i]);
		    }
		  iDebug = iolddebug;
		}
#endif
	      ubuffree (zhave);
	      return i;
	    }
	}

      ctimeout = (int) (iendtime - ixsysdep_time ((long *) NULL));
    }
}

#if DEBUG > 1

/* Debugging function for fcsend.  This takes the fquote variable, the
   length of the string (0 if this an informational string which can
   be printed directly) and the string itself.  It returns the new
   value for fquote.  The fquote variable is TRUE if the debugging
   output is in the middle of a quoted string.  */

static size_t cCsend_chars;
static int iColddebug;

static boolean fcsend_debug P((boolean, size_t, const char *));

static boolean
fcsend_debug (fquote, clen, zbuf)
     boolean fquote;
     size_t clen;
     const char *zbuf;
{
  size_t cwas;

  if (! FDEBUGGING (DEBUG_CHAT))
    return TRUE;

  cwas = cCsend_chars;
  if (clen > 0)
    cCsend_chars += clen;
  else
    cCsend_chars += strlen (zbuf);
  if (cCsend_chars > 60 && cwas > 10)
    {
      ulog (LOG_DEBUG_END, "%s", fquote ? "\"" : "");
      fquote = FALSE;
      ulog (LOG_DEBUG_START, "fcsend: Writing");
      cCsend_chars = 0;
    }

  if (clen == 0)
    {
      ulog (LOG_DEBUG_CONTINUE, "%s %s", fquote ? "\"" : "", zbuf);
      return FALSE;
    }
  else
    {
      int i;

      if (! fquote)
	ulog (LOG_DEBUG_CONTINUE, " \"");
      for (i = 0; i < clen; i++)
	{
	  char ab[5];

	  (void) cdebug_char (ab, zbuf[i]);
	  ulog (LOG_DEBUG_CONTINUE, "%s", ab);
	}

      return TRUE;
    }
}

/* Finish up the debugging information for fcsend.  */

static void ucsend_debug_end P((boolean, boolean));

static void
ucsend_debug_end (fquote, ferr)
     boolean fquote;
     boolean ferr;
{
  if (! FDEBUGGING (DEBUG_CHAT))
    return;

  if (fquote)
    ulog (LOG_DEBUG_CONTINUE, "\"");

  if (ferr)
    ulog (LOG_DEBUG_CONTINUE, " (error)");

  ulog (LOG_DEBUG_END, "%s", "");

  iDebug = iColddebug;
}

#else /* DEBUG <= 1 */

/* Use macro definitions to make fcsend look neater.  */

#define fcsend_debug(fquote, clen, zbuf) TRUE

#define ucsend_debug_end(fquote, ferror)

#endif /* DEBUG <= 1 */

/* Send a string out.  This has to parse escape sequences as it goes.
   Note that it handles the dialer escape sequences (\e, \E, \D, \T)
   although they make no sense for chatting with a system.  */

static boolean
fcsend (qconn, puuconf, z, qsys, qdial, zphone, ftranslate, fstrip)
     struct sconnection *qconn;
     pointer puuconf;
     const char *z;
     const struct uuconf_system *qsys;
     const struct uuconf_dialer *qdial;
     const char *zphone;
     boolean ftranslate;
     boolean fstrip;
{
  boolean fnocr;
  boolean (*pfwrite) P((struct sconnection *, const char *, size_t));
  char *zcallout_login;
  char *zcallout_pass;
  boolean fquote;

  if (strcmp (z, "\"\"") == 0)
    return TRUE;

  fnocr = FALSE;
  pfwrite = fconn_write;
  zcallout_login = NULL;
  zcallout_pass = NULL;

#if DEBUG > 1
  if (FDEBUGGING (DEBUG_CHAT))
    {
      ulog (LOG_DEBUG_START, "fcsend: Writing");
      fquote = FALSE;
      cCsend_chars = 0;
      iColddebug = iDebug;
      iDebug &=~ (DEBUG_OUTGOING | DEBUG_PORT);
    }
#endif

  while (*z != '\0')
    {
      const char *zlook;
      boolean fsend;
      char bsend;

      zlook = z + strcspn ((char *) z, "\\BE");

      if (zlook > z)
	{
	  size_t c;

	  c = zlook - z;
	  fquote = fcsend_debug (fquote, c, z);
	  if (! (*pfwrite) (qconn, z, c))
	    {
	      ucsend_debug_end (fquote, TRUE);
	      return FALSE;
	    }
	}

      if (*zlook == '\0')
	break;

      z = zlook;

      fsend = FALSE;
      switch (*z)
	{
	case 'B':
	  if (strncmp (z, "BREAK", 5) == 0)
	    {
	      fquote = fcsend_debug (fquote, (size_t) 0, "break");
	      if (! fconn_break (qconn))
		{
		  ucsend_debug_end (fquote, TRUE);
		  return FALSE;
		}
	      fnocr = TRUE;
	      z += 5;
	    }
	  else
	    {
	      fsend = TRUE;
	      bsend = 'B';
	      ++z;
	    }
	  break;
	case 'E':
	  if (strncmp (z, "EOT", 3) == 0)
	    {
	      fsend = TRUE;
	      bsend = '\004';
	      fnocr = TRUE;
	      z += 3;
	    }
	  else
	    {
	      fsend = TRUE;
	      bsend = 'E';
	      ++z;
	    }
	  break;
	case '\\':
	  ++z;
	  switch (*z)
	    {
	    case '-':
	      fsend = TRUE;
	      bsend = '-';
	      break;
	    case 'b':
	      fsend = TRUE;
	      bsend = '\b';
	      break;
	    case 'c':
	      fnocr = TRUE;
	      break;
	    case 'd':
	      fquote = fcsend_debug (fquote, (size_t) 0, "sleep");
	      usysdep_sleep (1);
	      break;
	    case 'e':
	      fquote = fcsend_debug (fquote, (size_t) 0, "echo-check-off");
	      pfwrite = fconn_write;
	      break;
	    case 'E':
	      fquote = fcsend_debug (fquote, (size_t) 0, "echo-check-on");
	      if (fstrip)
		pfwrite = fcecho_send_strip;
	      else
		pfwrite = fcecho_send_nostrip;
	      break;
	    case 'K':
	      fquote = fcsend_debug (fquote, (size_t) 0, "break");
	      if (! fconn_break (qconn))
		{
		  ucsend_debug_end (fquote, TRUE);
		  return FALSE;
		}
	      break;
	    case 'n':
	      fsend = TRUE;
	      bsend = '\n';
	      break;
	    case 'N':
	      fsend = TRUE;
	      bsend = '\0';
	      break;
	    case 'p':
	      fquote = fcsend_debug (fquote, (size_t) 0, "pause");
	      usysdep_pause ();
	      break;
	    case 'r':
	      fsend = TRUE;
	      bsend = '\r';
	      break;
	    case 's':
	      fsend = TRUE;
	      bsend = ' ';
	      break;
	    case 't':
	      fsend = TRUE;
	      bsend = '\t';
	      break;
	    case '\0':
	      --z;
	      /* Fall through.  */
	    case '\\':
	      fsend = TRUE;
	      bsend = '\\';
	      break;
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
	      fsend = TRUE;
	      bsend = *z - '0';
	      if (z[1] >= '0' && z[1] <= '7')
		bsend = (char) (8 * bsend + *++z - '0');
	      if (z[1] >= '0' && z[1] <= '7')
		bsend = (char) (8 * bsend + *++z - '0');
	      break;
	    case 'x':
	      fsend = TRUE;
	      bsend = 0;
	      while (isxdigit (BUCHAR (z[1])))
		{
		  if (isdigit (BUCHAR (z[1])))
		    bsend = (char) (16 * bsend + *++z - '0');
		  else if (isupper (BUCHAR (z[1])))
		    bsend = (char) (16 * bsend + *++z - 'A' + 10);
		  else
		    bsend = (char) (16 * bsend + *++z - 'a' + 10);
		}
	      break;
	    case 'L':
	      {
		const char *zlog;
		char *zcopy;
		size_t clen;

		if (qsys == NULL)
		  {
		    ucsend_debug_end (fquote, TRUE);
		    ulog (LOG_ERROR, "Illegal use of \\L");
		    return FALSE;
		  }
		zlog = qsys->uuconf_zcall_login;
		if (zlog == NULL)
		  {
		    ucsend_debug_end (fquote, TRUE);
		    ulog (LOG_ERROR, "No login defined");
		    return FALSE;
		  }
		if (zlog[0] == '*' && zlog[1] == '\0')
		  {
		    if (zcallout_login == NULL)
		      {
			int iuuconf;

			iuuconf = uuconf_callout (puuconf, qsys,
						  &zcallout_login,
						  &zcallout_pass);
			if (iuuconf == UUCONF_NOT_FOUND
			    || zcallout_login == NULL)
			  {
			    ucsend_debug_end (fquote, TRUE);
			    ulog (LOG_ERROR, "No login defined");
			    return FALSE;
			  }
			else if (iuuconf != UUCONF_SUCCESS)
			  {
			    ucsend_debug_end (fquote, TRUE);
			    ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
			    return FALSE;
			  }
		      }
		    zlog = zcallout_login;
		  }
		zcopy = zbufcpy (zlog);
		clen = cescape (zcopy);
		fquote = fcsend_debug (fquote, (size_t) 0, "login");
		fquote = fcsend_debug (fquote, clen, zcopy);
		if (! (*pfwrite) (qconn, zcopy, clen))
		  {
		    ubuffree (zcopy);
		    ucsend_debug_end (fquote, TRUE);
		    return FALSE;
		  }
		ubuffree (zcopy);
	      }
	      break;
	    case 'P':
	      {
		const char *zpass;
		char *zcopy;
		size_t clen;

		if (qsys == NULL)
		  {
		    ucsend_debug_end (fquote, TRUE);
		    ulog (LOG_ERROR, "Illegal use of \\P");
		    return FALSE;
		  }
		zpass = qsys->uuconf_zcall_password;
		if (zpass == NULL)
		  {
		    ucsend_debug_end (fquote, TRUE);
		    ulog (LOG_ERROR, "No password defined");
		    return FALSE;
		  }
		if (zpass[0] == '*' && zpass[1] == '\0')
		  {
		    if (zcallout_pass == NULL)
		      {
			int iuuconf;

			iuuconf = uuconf_callout (puuconf, qsys,
						  &zcallout_login,
						  &zcallout_pass);
			if (iuuconf == UUCONF_NOT_FOUND
			    || zcallout_pass == NULL)
			  {
			    ucsend_debug_end (fquote, TRUE);
			    ulog (LOG_ERROR, "No password defined");
			    return FALSE;
			  }
			else if (iuuconf != UUCONF_SUCCESS)
			  {
			    ucsend_debug_end (fquote, TRUE);
			    ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
			    return FALSE;
			  }
		      }
		    zpass = zcallout_pass;
		  }
		zcopy = zbufcpy (zpass);
		clen = cescape (zcopy);
		fquote = fcsend_debug (fquote, (size_t) 0, "password");
		fquote = fcsend_debug (fquote, clen, zcopy);
		if (! (*pfwrite) (qconn, zcopy, clen))
		  {
		    ubuffree (zcopy);
		    ucsend_debug_end (fquote, TRUE);
		    return FALSE;
		  }
		ubuffree (zcopy);
	      }
	      break;
	    case 'D':
	      if (qdial == NULL || zphone == NULL)
		{
		  ucsend_debug_end (fquote, TRUE);
		  ulog (LOG_ERROR, "Illegal use of \\D");
		  return FALSE;
		}
	      fquote = fcsend_debug (fquote, (size_t) 0, "\\D");
	      if (! fcphone (qconn, puuconf, qdial, zphone, pfwrite,
			     ftranslate, &fquote))
		{
		  ucsend_debug_end (fquote, TRUE);
		  return FALSE;
		}
	      break;
	    case 'T':
	      if (qdial == NULL || zphone == NULL)
		{
		  ucsend_debug_end (fquote, TRUE);
		  ulog (LOG_ERROR, "Illegal use of \\T");
		  return FALSE;
		}
	      fquote = fcsend_debug (fquote, (size_t) 0, "\\T");
	      if (! fcphone (qconn, puuconf, qdial, zphone, pfwrite, TRUE,
			     &fquote))
		{
		  ucsend_debug_end (fquote, TRUE);
		  return FALSE;
		}
	      break;
	    case 'M':
	      if (qdial == NULL)
		{
		  ucsend_debug_end (fquote, TRUE);
		  ulog (LOG_ERROR, "Illegal use of \\M");
		  return FALSE;
		}
	      fquote = fcsend_debug (fquote, (size_t) 0, "ignore-carrier");
	      if (! fconn_carrier (qconn, FALSE))
		{
		  ucsend_debug_end (fquote, TRUE);
		  return FALSE;
		}
	      break;
	    case 'm':
	      if (qdial == NULL)
		{
		  ucsend_debug_end (fquote, TRUE);
		  ulog (LOG_ERROR, "Illegal use of \\m");
		  return FALSE;
		}
	      if (qdial->uuconf_fcarrier)
		{
		  fquote = fcsend_debug (fquote, (size_t) 0, "need-carrier");
		  if (! fconn_carrier (qconn, TRUE))
		    {
		      ucsend_debug_end (fquote, TRUE);
		      return FALSE;
		    }
		}
	      break;
	    default:
	      /* This error message will screw up any debugging
		 information, but it's easily avoidable.  */
	      ulog (LOG_ERROR,
		    "Unrecognized escape sequence \\%c in send string",
		    *z);
	      fsend = TRUE;
	      bsend = *z;
	      break;
	    }
	  ++z;
	  break;
#if DEBUG > 0
	default:
	  ulog (LOG_FATAL, "fcsend: Can't happen");
	  break;
#endif
	}
      
      if (fsend)
	{
	  fquote = fcsend_debug (fquote, (size_t) 1, &bsend);
	  if (! (*pfwrite) (qconn, &bsend, (size_t) 1))
	    {
	      ucsend_debug_end (fquote, TRUE);
	      return FALSE;
	    }
	}
    }

  xfree ((pointer) zcallout_login);
  xfree ((pointer) zcallout_pass);

  /* Output a final carriage return, unless there was a \c.  Don't
     bother to check for an echo.  */
  if (! fnocr)
    {
      char b;

      b = '\r';
      fquote = fcsend_debug (fquote, (size_t) 1, &b);
      if (! fconn_write (qconn, &b, (size_t) 1))
	{
	  ucsend_debug_end (fquote, TRUE);
	  return FALSE;
	}
    }

  ucsend_debug_end (fquote, FALSE);

  return TRUE;
}

/* Write out a phone number with optional dialcode translation.  The
   pfquote argument is only used for debugging.  */

static boolean
fcphone (qconn, puuconf, qdial, zphone, pfwrite, ftranslate, pfquote)
     struct sconnection *qconn;
     pointer puuconf;
     const struct uuconf_dialer *qdial;
     const char *zphone;
     boolean (*pfwrite) P((struct sconnection *qc, const char *zwrite,
			   size_t cwrite));
     boolean ftranslate;
     boolean *pfquote;
{
  const char *zprefix, *zsuffix;

  if (ftranslate)
    {
      if (! fctranslate (puuconf, zphone, &zprefix, &zsuffix))
	return FALSE;
    }
  else
    {
      zprefix = zphone;
      zsuffix = NULL;
    }

  while (zprefix != NULL)
    {
      while (TRUE)
	{
	  const char *z;
	  const char *zstr;

	  z = zprefix + strcspn ((char *) zprefix, "=-");
	  if (z > zprefix)
	    {
	      size_t clen;

	      clen = z - zprefix;
	      *pfquote = fcsend_debug (*pfquote, clen, zprefix);
	      if (! (*pfwrite) (qconn, zprefix, clen))
		return FALSE;
	    }

	  if (*z == '=')
	    zstr = qdial->uuconf_zdialtone;
	  else if (*z == '-')
	    zstr = qdial->uuconf_zpause;
	  else			/* *z == '\0' */
	    break;

	  if (zstr != NULL)
	    {
	      *pfquote = fcsend_debug (*pfquote, strlen (zstr), zstr);
	      if (! (*pfwrite) (qconn, zstr, strlen (zstr)))
		return FALSE;
	    }

	  zprefix = z + 1;
	}

      zprefix = zsuffix;
      zsuffix = NULL;
    }

  return TRUE;
}

/* Given a phone number, run it through dial code translation
   returning two strings.  */

static boolean
fctranslate (puuconf, zphone, pzprefix, pzsuffix)
     pointer puuconf;
     const char *zphone;
     const char **pzprefix;
     const char **pzsuffix;
{
  int iuuconf;
  char *zdialcode, *zto;
  const char *zfrom;
  char *ztrans;

  *pzprefix = zphone;
  *pzsuffix = NULL;

  zdialcode = zbufalc (strlen (zphone) + 1);
  zfrom = zphone;
  zto = zdialcode;
  while (*zfrom != '\0' && isalpha (BUCHAR (*zfrom)))
    *zto++ = *zfrom++;
  *zto = '\0';

  if (*zdialcode == '\0')
    {
      ubuffree (zdialcode);
      return TRUE;
    }

  iuuconf = uuconf_dialcode (puuconf, zdialcode, &ztrans);

  ubuffree (zdialcode);

  if (iuuconf == UUCONF_NOT_FOUND)
    return TRUE;
  else if (iuuconf != UUCONF_SUCCESS)
    {
      ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
      return FALSE;
    }
  else
    {
      /* We really should figure out a way to free up ztrans here.  */
      *pzprefix = ztrans;
      *pzsuffix = zfrom;
      return TRUE;
    }
}

/* Write out a string making sure the each character is echoed back.
   There are two versions of this function, one which strips the
   parity bit from the characters and one which does not.  This is so
   that I can use a single function pointer in fcsend, and to avoid
   using any static variables so that I can put chat scripts in a
   library some day.  */

static boolean
fcecho_send_strip (qconn, zwrite, cwrite)
     struct sconnection *qconn;
     const char *zwrite;
     size_t cwrite;
{
  return fcecho_send (qconn, zwrite, cwrite, TRUE);
}

static boolean
fcecho_send_nostrip (qconn, zwrite, cwrite)
     struct sconnection *qconn;
     const char *zwrite;
     size_t cwrite;
{
  return fcecho_send (qconn, zwrite, cwrite, FALSE);
}

static boolean
fcecho_send (qconn, zwrite, cwrite, fstrip)
     struct sconnection *qconn;
     const char *zwrite;
     size_t cwrite;
     boolean fstrip;
{
  const char *zend;

  zend = zwrite + cwrite;

  for (; zwrite < zend; zwrite++)
    {
      int b;
      char bwrite;

      bwrite = *zwrite;
      if (! fconn_write (qconn, &bwrite, (size_t) 1))
	return FALSE;
      if (fstrip)
	bwrite &= 0x7f;
      do
	{
	  /* We arbitrarily wait five seconds for the echo.  */
	  b = breceive_char (qconn, 5, TRUE);
	  /* Now b == -1 on timeout, -2 on error.  */
	  if (b < 0)
	    {
	      if (b == -1)
		ulog (LOG_ERROR, "Character not echoed");
	      return FALSE;
	    }
	  if (fstrip)
	    b &= 0x7f;
	}
      while (b != BUCHAR (bwrite));
    }

  return TRUE;
}

/* Run a chat program.  Expand any escape sequences and call a system
   dependent program to run it.  */

static boolean
fcprogram (qconn, puuconf, pzprogram, qsys, qdial, zphone, zport, ibaud)
     struct sconnection *qconn;
     pointer puuconf;
     char **pzprogram;
     const struct uuconf_system *qsys;
     const struct uuconf_dialer *qdial;
     const char *zphone;
     const char *zport;
     long ibaud;
{
  size_t cargs;
  char **pzpass, **pzarg;
  char **pz;
  char *zcallout_login;
  char *zcallout_pass;
  boolean fret;

  cargs = 1;
  for (pz = pzprogram; *pz != NULL; pz++)
    ++cargs;

  pzpass = (char **) xmalloc (cargs * sizeof (char *));

  zcallout_login = NULL;
  zcallout_pass = NULL;
  fret = TRUE;

  /* Copy the string into memory expanding escape sequences.  */
  for (pz = pzprogram, pzarg = pzpass; *pz != NULL; pz++, pzarg++)
    {
      const char *zfrom;
      size_t calc, clen;
      char *zto;

      if (strchr (*pz, '\\') == NULL)
	{
	  *pzarg = zbufcpy (*pz);
	  continue;
	}
      
      *pzarg = NULL;
      zto = NULL;
      calc = 0;
      clen = 0;

      for (zfrom = *pz; *zfrom != '\0'; zfrom++)
	{
	  const char *zadd = NULL;
	  char *zfree = NULL;
	  size_t cadd;
	  char abadd[15];

	  if (*zfrom != '\\')
	    {
	      if (clen + 2 > calc)
		{
		  char *znew;

		  calc = clen + 50;
		  znew = zbufalc (calc);
		  memcpy (znew, *pzarg, clen);
		  ubuffree (*pzarg);
		  *pzarg = znew;
		  zto = znew + clen;
		}
	      *zto++ = *zfrom;
	      ++clen;
	      continue;
	    }

	  ++zfrom;
	  switch (*zfrom)
	    {
	    case '\0':
	      --zfrom;
	      /* Fall through.  */
	    case '\\':
	      zadd = "\\";
	      break;
	    case 'L':
	      {
		const char *zlog;

		if (qsys == NULL)
		  {
		    ulog (LOG_ERROR, "chat-program: Illegal use of \\L");
		    fret = FALSE;
		    break;
		  }
		zlog = qsys->uuconf_zcall_login;
		if (zlog == NULL)
		  {
		    ulog (LOG_ERROR, "chat-program: No login defined");
		    fret = FALSE;
		    break;
		  }
		if (zlog[0] == '*' && zlog[1] == '\0')
		  {
		    if (zcallout_login == NULL)
		      {
			int iuuconf;

			iuuconf = uuconf_callout (puuconf, qsys,
						  &zcallout_login,
						  &zcallout_pass);
			if (iuuconf == UUCONF_NOT_FOUND
			    || zcallout_login == NULL)
			  {
			    ulog (LOG_ERROR,
				  "chat-program: No login defined");
			    fret = FALSE;
			    break;
			  }
			else if (iuuconf != UUCONF_SUCCESS)
			  {
			    ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
			    fret = FALSE;
			    break;
			  }
		      }
		    zlog = zcallout_login;
		  }
		zfree = zbufcpy (zlog);
		(void) cescape (zfree);
		zadd = zfree;
	      }
	      break;
	    case 'P':
	      {
		const char *zpass;

		if (qsys == NULL)
		  {
		    ulog (LOG_ERROR, "chat-program: Illegal use of \\P");
		    fret = FALSE;
		    break;
		  }
		zpass = qsys->uuconf_zcall_password;
		if (zpass == NULL)
		  {
		    ulog (LOG_ERROR, "chat-program: No password defined");
		    fret = FALSE;
		    break;
		  }
		if (zpass[0] == '*' && zpass[1] == '\0')
		  {
		    if (zcallout_pass == NULL)
		      {
			int iuuconf;

			iuuconf = uuconf_callout (puuconf, qsys,
						  &zcallout_login,
						  &zcallout_pass);
			if (iuuconf == UUCONF_NOT_FOUND
			    || zcallout_pass == NULL)
			  {
			    ulog (LOG_ERROR,
				  "chat-program: No password defined");
			    fret = FALSE;
			    break;
			  }
			else if (iuuconf != UUCONF_SUCCESS)
			  {
			    ulog_uuconf (LOG_ERROR, puuconf, iuuconf);
			    fret = FALSE;
			    break;
			  }
		      }
		    zpass = zcallout_pass;
		  }
		zfree = zbufcpy (zpass);
		(void) cescape (zfree);
		zadd = zfree;
	      }
	      break;
	    case 'D':
	      if (qdial == NULL || zphone == NULL)
		{
		  ulog (LOG_ERROR, "chat-program: Illegal use of \\D");
		  fret = FALSE;
		  break;
		}
	      zadd = zphone;
	      break;
	    case 'T':
	      {
		const char *zprefix, *zsuffix;

		if (qdial == NULL || zphone == NULL)
		  {
		    ulog (LOG_ERROR, "chat-program: Illegal use of \\T");
		    fret = FALSE;
		    break;
		  }

		if (! fctranslate (puuconf, zphone, &zprefix, &zsuffix))
		  {
		    fret = FALSE;
		    break;
		  }

		if (zsuffix == NULL)
		  zadd = zprefix;
		else
		  {
		    size_t cprefix;

		    cprefix = strlen (zprefix);
		    if (clen + cprefix + 1 > calc)
		      {
			char *znew;

			calc = clen + cprefix + 20;
			znew = zbufalc (calc);
			memcpy (znew, *pzarg, clen);
			ubuffree (*pzarg);
			*pzarg = znew;
			zto = znew + clen;
		      }
		    memcpy (zto, zprefix, cprefix);
		    zto += cprefix;
		    clen += cprefix;
		    zadd = zsuffix;
		  }
	      }
	      break;
	    case 'Y':
	      if (zLdevice == NULL && zport == NULL)
		{
		  ulog (LOG_ERROR, "chat-program: Illegal use of \\Y");
		  fret = FALSE;
		  break;
		}
	      /* zLdevice will generally make more sense than zport, but
		 it might not be set yet.  */
	      zadd = zLdevice;
	      if (zadd == NULL)
		zadd = zport;
	      break;
	    case 'Z':
	      if (qsys == NULL)
		{
		  ulog (LOG_ERROR, "chat-program: Illegal use of \\Z");
		  fret = FALSE;
		  break;
		}
	      zadd = qsys->uuconf_zname;
	      break;
	    case 'S':
	      {
		if (ibaud == 0)
		  {
		    ulog (LOG_ERROR, "chat-program: Illegal use of \\S");
		    fret = FALSE;
		    break;
		  }
		sprintf (abadd, "%ld", ibaud);
		zadd = abadd;
	      }
	      break;
	    default:
	      {
		ulog (LOG_ERROR,
		      "chat-program: Unrecognized escape sequence \\%c",
		      *zfrom);
		abadd[0] = *zfrom;
		abadd[1] = '\0';
		zadd = abadd;
	      }
	      break;
	    }

	  if (! fret)
	    break;

	  cadd = strlen (zadd);
	  if (clen + cadd + 1 > calc)
	    {
	      char *znew;

	      calc = clen + cadd + 20;
	      znew = zbufalc (calc);
	      memcpy (znew, *pzarg, clen);
	      ubuffree (*pzarg);
	      *pzarg = znew;
	      zto = znew + clen;
	    }
	  memcpy (zto, zadd, cadd + 1);
	  zto += cadd;
	  clen += cadd;
	  ubuffree (zfree);
	}

      if (! fret)
	break;

      *zto++ = '\0';
      ++clen;
    }

  *pzarg = NULL;

  if (fret)
    fret = fconn_run_chat (qconn, pzpass);

  for (pz = pzpass; *pz != NULL; pz++)
    ubuffree (*pz);
  xfree ((pointer) pzpass);
  xfree ((pointer) zcallout_login);
  xfree ((pointer) zcallout_pass);

  return fret;
}
