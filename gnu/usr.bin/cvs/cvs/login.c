/*
 * Copyright (c) 1995, Cyclic Software, Bloomington, IN, USA
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with CVS.
 * 
 * Allow user to log in for an authenticating server.
 */

#include "cvs.h"

#ifdef AUTH_CLIENT_SUPPORT   /* This covers the rest of the file. */

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#ifndef lint
static const char rcsid[] = "$CVSid: @(#)login.c 1.1 95/10/01 $";
USE(rcsid);
#endif

#ifndef CVS_PASSWORD_FILE 
#define CVS_PASSWORD_FILE ".cvspass"
#endif


/* The return value will need to be freed. */
char *
construct_cvspass_filename ()
{
  char *homedir;
  char *passfile;

  /* Construct absolute pathname to user's password file. */
  /* todo: does this work under Win-NT and OS/2 ? */
  homedir = getenv ("HOME");
  if (! homedir)
    {
      error (1, errno, "could not find out home directory");
      return (char *) NULL;
    }
  
  passfile =
    (char *) xmalloc (strlen (homedir) + strlen (CVS_PASSWORD_FILE) + 3);
  strcpy (passfile, homedir);
  strcat (passfile, "/");
  strcat (passfile, CVS_PASSWORD_FILE);
  
  /* Safety first and last, Scouts. */
  if (isfile (passfile))
    /* xchmod() is too polite. */
    chmod (passfile, 0600);

  return passfile;
}


/* Prompt for a password, and store it in the file "CVS/.cvspass".
 *
 * Because the user might be accessing multiple repositories, with
 * different passwords for each one, the format of ~/.cvspass is:
 *
 * user@host:/path cleartext_password
 * user@host:/path cleartext_password
 * ...
 *
 * Of course, the "user@" might be left off -- it's just based on the
 * value of CVSroot.
 *
 * Like .netrc, the file's permissions are the only thing preventing
 * it from being read by others.  Unlike .netrc, we will not be
 * fascist about it, at most issuing a warning, and never refusing to
 * work.
 */
int
login (argc, argv)
    int argc;
    char **argv;
{
  char *username;
  int i;
  char *passfile;
  FILE *fp;
  char *typed_password, *found_password;
  char linebuf[MAXLINELEN];
  int root_len, already_entered = 0;

  /* Make this a "fully-qualified" CVSroot if necessary. */
  if (! strchr (CVSroot, '@'))
    {
      /* We need to prepend "user@host:". */
      char *tmp;

      printf ("Repository \"%s\" not fully-qualified.\n", CVSroot);
      printf ("Please enter \"user@host:/path\": ");
      fflush (stdout);
      fgets  (linebuf, MAXLINELEN, stdin);

      tmp = xmalloc (strlen (linebuf) + 1);

      strcpy (tmp, linebuf);
      tmp[strlen (linebuf) - 1] = '\0';
      CVSroot = tmp;
    }

  /* Check to make sure it's fully-qualified before going on. */
  if (! CVSroot)
    {
      error (1, 0, "CVSroot is NULL");
    }
  else if ((! strchr (CVSroot, '@')) && (! strchr (CVSroot, ':')))
    {
      error (1, 0, "CVSroot not fully-qualified: %s", CVSroot);
    }


  passfile = construct_cvspass_filename ();
  typed_password = getpass ("Enter CVS password: ");

  /* IF we have a password for this "[user@]host:/path" already
   *  THEN
   *    IF it's the same as the password we read from the prompt
   *     THEN 
   *       do nothing
   *     ELSE
   *       replace the old password with the new one
   *  ELSE
   *    append new entry to the end of the file.
   */

  root_len = strlen (CVSroot);

  /* Yes, the method below reads the user's password file twice.  It's
     inefficient, but we're not talking about a gig of data here. */

  fp = fopen (passfile, "r");
  if (fp == NULL)
    {
      error (1, errno, "unable to open %s", passfile);
      return 1;
    }

  /* Check each line to see if we have this entry already. */
  while (fgets (linebuf, MAXLINELEN, fp) != NULL)
    {
      if (! strncmp (CVSroot, linebuf, root_len))
        {
          already_entered = 1;
          break;
        }
    }
  fclose (fp);

      
  if (already_entered)
    {
      /* This user/host has a password in the file already. */

      /* todo: what about these charsets??? */
      strtok (linebuf, " \n");
      found_password = strtok (NULL, " \n");
      if (strcmp (found_password, typed_password))
        {
          /* typed_password and found_password don't match, so we'll
           * have to update passfile.  We replace the old password
           * with the new one by writing a tmp file whose contents are
           * exactly the same as passfile except that this one entry
           * gets typed_password instead of found_password.  Then we
           * rename the tmp file on top of passfile.
           */
          char *tmp_name;
          FILE *tmp_fp;

          tmp_name = tmpnam (NULL);
          if ((tmp_fp = fopen (tmp_name, "w")) == NULL)
            {
              error (1, errno, "unable to open temp file %s", tmp_name);
              return 1;
            }
          chmod (tmp_name, 0600);

          fp = fopen (passfile, "r");
          if (fp == NULL)
            {
              error (1, errno, "unable to open %s", passfile);
              return 1;
            }
          /* I'm not paranoid, they really ARE out to get me: */
          chmod (passfile, 0600);

          while (fgets (linebuf, MAXLINELEN, fp) != NULL)
            {
              if (strncmp (CVSroot, linebuf, root_len))
                fprintf (tmp_fp, "%s", linebuf);
              else
                fprintf (tmp_fp, "%s %s\n", CVSroot, typed_password);
            }
          fclose (tmp_fp);
          fclose (fp);
          rename_file (tmp_name, passfile);
          chmod (passfile, 0600);
        }
    }
  else
    {
      if ((fp = fopen (passfile, "a")) == NULL)
        {
          error (1, errno, "could not open %s", passfile);
          free (passfile);
          return 1;
        }

      /* It's safer this way, and blank lines in the file are OK. */
      fprintf (fp, "\n%s %s\n", CVSroot, typed_password);
      fclose (fp);
    }

  /* Utter, total, raving paranoia, I know. */
  chmod (passfile, 0600);
  memset (typed_password, 0, strlen (typed_password));

  free (passfile);
  return 0;
}

/* todo: "cvs logout" could erase an entry from the file.
 * But to what purpose?
 */


char *
get_cvs_password (user, host, cvsroot)
{
  int root_len;
  int found_it = 0;
  char *password;
  char linebuf[MAXLINELEN];
  FILE *fp;
  char *passfile;

  passfile = construct_cvspass_filename ();
  fp = fopen (passfile, "r");
  if (fp == NULL)
    {
      error (0, errno, "could not open %s", passfile);
      free (passfile);
      goto prompt_for_it;
    }

  root_len = strlen (CVSroot);

  /* Check each line to see if we have this entry already. */
  while (fgets (linebuf, MAXLINELEN, fp) != NULL)
    {
      if (strncmp (CVSroot, linebuf, root_len) == 0)
        {
          /* This is it!  So break out and deal with linebuf. */
          found_it = 1;
          break;
        }
    }

  if (found_it)
    {
      /* linebuf now contains the line with the password. */
      char *tmp;
      
      strtok (linebuf, " ");
      password = strtok (NULL, "\n");
      
      /* Give it permanent storage. */
      tmp = xmalloc (strlen (password) + 1);
      strcpy (tmp, password);
      tmp[strlen (password)] = '\0';
      memset (password, 0, strlen (password));
      return tmp;
    }
  else
    {
    prompt_for_it:
      return getpass ("CVS password: ");
    }
}

#endif /* AUTH_CLIENT_SUPPORT from beginning of file. */

