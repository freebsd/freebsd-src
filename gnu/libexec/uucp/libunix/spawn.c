/* spawn.c
   Spawn a program securely.

   Copyright (C) 1992 Ian Lance Taylor

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"

#include <errno.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#ifndef environ
extern char **environ;
#endif

/* Spawn a child in a fairly secure fashion.  This returns the process
   ID of the child or -1 on error.  It takes far too many arguments:

   pazargs -- arguments (element 0 is command)
   aidescs -- file descriptors for stdin, stdout and stderr
   fkeepuid -- TRUE if euid should be left unchanged
   fkeepenv -- TRUE if environment should be left unmodified
   zchdir -- directory to chdir to
   fnosigs -- TRUE if child should ignore SIGHUP, SIGINT and SIGQUIT
   fshell -- TRUE if should try /bin/sh if execve gets ENOEXEC
   zpath -- value for environment variable PATH
   zuu_machine -- value for environment variable UU_MACHINE
   zuu_user -- value for environment variable UU_USER

   The aidescs array is three elements long.  0 is stdin, 1 is stdout
   and 2 is stderr.  The array may contain either file descriptor
   numbers to dup appropriately, or one of the following:

   SPAWN_NULL -- set descriptor to /dev/null
   SPAWN_READ_PIPE -- set aidescs element to pipe for parent to read
   SPAWN_WRITE_PIPE -- set aidescs element to pipe for parent to write

   If fkeepenv is FALSE, a standard environment is created.  The
   environment arguments (zpath, zuu_machine and zuu_user) are only
   used if fkeepenv is FALSE; any of them may be NULL.

   This routine expects that all file descriptors have been set to
   close-on-exec, so it doesn't have to worry about closing them
   explicitly.  It sets the close-on-exec flag for the new pipe
   descriptors it returns.  */

pid_t
ixsspawn (pazargs, aidescs, fkeepuid, fkeepenv, zchdir, fnosigs, fshell,
	  zpath, zuu_machine, zuu_user)
     const char **pazargs;
     int aidescs[3];
     boolean fkeepuid;
     boolean fkeepenv;
     const char *zchdir;
     boolean fnosigs;
     boolean fshell;
     const char *zpath;
     const char *zuu_machine;
     const char *zuu_user;
{
  char *zshcmd;
  int i;
  char *azenv[9];
  char **pazenv;
  boolean ferr;
  int ierr = 0;
  int onull;
  int aichild_descs[3];
  int cpar_close;
  int aipar_close[4];
  int cchild_close;
  int aichild_close[3];
  pid_t iret = 0;
  const char *zcmd;

  /* If we might have to use the shell, allocate enough space for the
     quoted command before forking.  Otherwise the allocation would
     modify the data segment and we could not safely use vfork.  */
  zshcmd = NULL;
  if (fshell)
    {
      size_t clen;

      clen = 0;
      for (i = 0; pazargs[i] != NULL; i++)
	clen += strlen (pazargs[i]);
      zshcmd = zbufalc (2 * clen + i);
    }

  /* Set up a standard environment.  This is again done before forking
     because it will modify the data segment.  */
  if (fkeepenv)
    pazenv = environ;
  else
    {
      const char *zterm, *ztz;
      char *zspace;
      int ienv;

      if (zpath == NULL)
	zpath = CMDPATH;

      azenv[0] = zbufalc (sizeof "PATH=" + strlen (zpath));
      sprintf (azenv[0], "PATH=%s", zpath);
      zspace = azenv[0] + sizeof "PATH=" - 1;
      while ((zspace = strchr (zspace, ' ')) != NULL)
	*zspace = ':';
    
      azenv[1] = zbufalc (sizeof "HOME=" + strlen (zSspooldir));
      sprintf (azenv[1], "HOME=%s", zSspooldir);

      zterm = getenv ("TERM");
      if (zterm == NULL)
	zterm = "unknown";
      azenv[2] = zbufalc (sizeof "TERM=" + strlen (zterm));
      sprintf (azenv[2], "TERM=%s", zterm);

      azenv[3] = zbufcpy ("SHELL=/bin/sh");
  
      azenv[4] = zbufalc (sizeof "USER=" + strlen (OWNER));
      sprintf (azenv[4], "USER=%s", OWNER);

      ienv = 5;

      ztz = getenv ("TZ");
      if (ztz != NULL)
	{
	  azenv[ienv] = zbufalc (sizeof "TZ=" + strlen (ztz));
	  sprintf (azenv[ienv], "TZ=%s", ztz);
	  ++ienv;
	}

      if (zuu_machine != NULL)
	{
	  azenv[ienv] = zbufalc (sizeof "UU_MACHINE="
				 + strlen (zuu_machine));
	  sprintf (azenv[ienv], "UU_MACHINE=%s", zuu_machine);
	  ++ienv;
	}

      if (zuu_user != NULL)
	{
	  azenv[ienv] = zbufalc (sizeof "UU_USER="
				 + strlen (zuu_user));
	  sprintf (azenv[ienv], "UU_USER=%s", zuu_user);
	  ++ienv;
	}

      azenv[ienv] = NULL;
      pazenv = azenv;
    }

  /* Set up any needed pipes.  */

  ferr = FALSE;
  onull = -1;
  cpar_close = 0;
  cchild_close = 0;

  for (i = 0; i < 3; i++)
    {
      if (aidescs[i] == SPAWN_NULL)
	{
	  if (onull < 0)
	    {
	      onull = open ((char *) "/dev/null", O_RDWR);
	      if (onull < 0
		  || fcntl (onull, F_SETFD,
			    fcntl (onull, F_GETFD, 0) | FD_CLOEXEC) < 0)
		{
		  ierr = errno;
		  (void) close (onull);
		  ferr = TRUE;
		  break;
		}
	      aipar_close[cpar_close] = onull;
	      ++cpar_close;
	    }
	  aichild_descs[i] = onull;
	}
      else if (aidescs[i] != SPAWN_READ_PIPE
	       && aidescs[i] != SPAWN_WRITE_PIPE)
	aichild_descs[i] = aidescs[i];
      else
	{
	  int aipipe[2];

	  if (pipe (aipipe) < 0)
	    {
	      ierr = errno;
	      ferr = TRUE;
	      break;
	    }

	  if (aidescs[i] == SPAWN_READ_PIPE)
	    {
	      aidescs[i] = aipipe[0];
	      aichild_close[cchild_close] = aipipe[0];
	      aichild_descs[i] = aipipe[1];
	      aipar_close[cpar_close] = aipipe[1];
	    }
	  else
	    {
	      aidescs[i] = aipipe[1];
	      aichild_close[cchild_close] = aipipe[1];
	      aichild_descs[i] = aipipe[0];
	      aipar_close[cpar_close] = aipipe[0];
	    }

	  ++cpar_close;
	  ++cchild_close;

	  if (fcntl (aidescs[i], F_SETFD,
		     fcntl (aidescs[i], F_GETFD, 0) | FD_CLOEXEC) < 0)
	    {
	      ierr = errno;
	      ferr = TRUE;
	      break;
	    }	      
	}
    }

#if DEBUG > 1
  if (! ferr && FDEBUGGING (DEBUG_EXECUTE))
    {
      ulog (LOG_DEBUG_START, "Forking %s", pazargs[0]);
      for (i = 1; pazargs[i] != NULL; i++)
	ulog (LOG_DEBUG_CONTINUE, " %s", pazargs[i]);
      ulog (LOG_DEBUG_END, "%s", "");
    }
#endif

  if (! ferr)
    {
      /* This should really be vfork if available.  */
      iret = ixsfork ();
      if (iret < 0)
	{
	  ferr = TRUE;
	  ierr = errno;
	}
    }

  if (ferr)
    {
      for (i = 0; i < cchild_close; i++)
	(void) close (aichild_close[i]);
      iret = -1;
    }

  if (iret != 0)
    {
      /* The parent.  Close the child's ends of the pipes and return
	 the process ID, or an error.  */
      for (i = 0; i < cpar_close; i++)
	(void) close (aipar_close[i]);
      ubuffree (zshcmd);
      if (! fkeepenv)
	{
	  char **pz;

	  for (pz = azenv; *pz != NULL; pz++)
	    ubuffree (*pz);
	}
      errno = ierr;
      return iret;
    }

  /* The child.  */

#ifdef STDIN_FILENO
#if STDIN_FILENO != 0 || STDOUT_FILENO != 1 || STDERR_FILENO != 2
 #error The following code makes invalid assumptions
#endif
#endif

  for (i = 0; i < 3; i++)
    {
      if (aichild_descs[i] != i)
	(void) dup2 (aichild_descs[i], i);
      /* This should only be necessary if aichild_descs[i] == i, but
	 some systems copy the close-on-exec flag for a dupped
	 descriptor, which is wrong according to POSIX.  */
      (void) fcntl (i, F_SETFD, fcntl (i, F_GETFD, 0) &~ FD_CLOEXEC);
    }

  zcmd = pazargs[0];
  pazargs[0] = strrchr (zcmd, '/');
  if (pazargs[0] == NULL)
    pazargs[0] = zcmd;
  else
    ++pazargs[0];

  if (! fkeepuid)
    {
      (void) setuid (getuid ());
      (void) setgid (getgid ());
    }

  if (zchdir != NULL)
    (void) chdir (zchdir);

  if (fnosigs)
    {
#ifdef SIGHUP
      (void) signal (SIGHUP, SIG_IGN);
#endif
#ifdef SIGINT
      (void) signal (SIGINT, SIG_IGN);
#endif
#ifdef SIGQUIT
      (void) signal (SIGQUIT, SIG_IGN);
#endif
    }

  (void) execve ((char *) zcmd, (char **) pazargs, pazenv);

  /* The exec failed.  If permitted, try using /bin/sh to execute a
     shell script.  */

  if (errno == ENOEXEC && fshell)
    {
      char *zto;
      const char *azshargs[4];
      
      pazargs[0] = zcmd;
      zto = zshcmd;
      for (i = 0; pazargs[i] != NULL; i++)
	{
	  const char *zfrom;

	  for (zfrom = pazargs[i]; *zfrom != '\0'; zfrom++)
	    {
	      /* Some versions of /bin/sh appear to have a bug such
		 that quoting a '/' sometimes causes an error.  I
		 don't know exactly when this happens (I can recreate
		 it on Ultrix 4.0), but in any case it is harmless to
		 not quote a '/'.  */
	      if (*zfrom != '/')
		*zto++ = '\\';
	      *zto++ = *zfrom;
	    }
	  *zto++ = ' ';
	}
      *(zto - 1) = '\0';

      azshargs[0] = "sh";
      azshargs[1] = "-c";
      azshargs[2] = zshcmd;
      azshargs[3] = NULL;

      (void) execve ((char *) "/bin/sh", (char **) azshargs, pazenv);
    }

  _exit (EXIT_FAILURE);

  /* Avoid compiler warning.  */
  return -1;
}
