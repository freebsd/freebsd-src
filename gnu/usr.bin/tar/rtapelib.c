/* Functions for communicating with a remote tape drive.
   Copyright (C) 1988, 1992 Free Software Foundation, Inc.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* The man page rmt(8) for /etc/rmt documents the remote mag tape
   protocol which rdump and rrestore use.  Unfortunately, the man
   page is *WRONG*.  The author of the routines I'm including originally
   wrote his code just based on the man page, and it didn't work, so he
   went to the rdump source to figure out why.  The only thing he had to
   change was to check for the 'F' return code in addition to the 'E',
   and to separate the various arguments with \n instead of a space.  I
   personally don't think that this is much of a problem, but I wanted to
   point it out. -- Arnold Robbins

   Originally written by Jeff Lee, modified some by Arnold Robbins.
   Redone as a library that can replace open, read, write, etc., by
   Fred Fish, with some additional work by Arnold Robbins.
   Modified to make all rmtXXX calls into macros for speed by Jay Fenlason.
   Use -DHAVE_NETDB_H for rexec code, courtesy of Dan Kegel, srs!dan.  */

/* $FreeBSD$ */

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>

#ifdef HAVE_SYS_MTIO_H
#include <sys/ioctl.h>
#include <sys/mtio.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#include <errno.h>
#include <setjmp.h>
#include <sys/stat.h>

#ifndef errno
extern int errno;
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef STDC_HEADERS
#include <string.h>
#include <stdlib.h>
#endif

/* Maximum size of a fully qualified host name.  */
#define MAXHOSTLEN 257

/* Size of buffers for reading and writing commands to rmt.
   (An arbitrary limit.)  */
#define CMDBUFSIZE 64

#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif

/* Maximum number of simultaneous remote tape connections.
   (Another arbitrary limit.)  */
#define MAXUNIT	4

/* Return the parent's read side of remote tape connection FILDES.  */
#define READ(fildes) (from_rmt[fildes][0])

/* Return the parent's write side of remote tape connection FILDES.  */
#define WRITE(fildes) (to_rmt[fildes][1])

/* The pipes for receiving data from remote tape drives.  */
static int from_rmt[MAXUNIT][2] =
{-1, -1, -1, -1, -1, -1, -1, -1};

/* The pipes for sending data to remote tape drives.  */
static int to_rmt[MAXUNIT][2] =
{-1, -1, -1, -1, -1, -1, -1, -1};

/* Temporary variable used by macros in rmt.h.  */
char *__rmt_path;

/* Close remote tape connection FILDES.  */

static void
_rmt_shutdown (fildes)
     int fildes;
{
  close (READ (fildes));
  close (WRITE (fildes));
  READ (fildes) = -1;
  WRITE (fildes) = -1;
}

/* Attempt to perform the remote tape command specified in BUF
   on remote tape connection FILDES.
   Return 0 if successful, -1 on error.  */

static int
command (fildes, buf)
     int fildes;
     char *buf;
{
  register int buflen;
  RETSIGTYPE (*pipe_handler) ();

  /* Save the current pipe handler and try to make the request.  */

  pipe_handler = signal (SIGPIPE, SIG_IGN);
  buflen = strlen (buf);
  if (write (WRITE (fildes), buf, buflen) == buflen)
    {
      signal (SIGPIPE, pipe_handler);
      return 0;
    }

  /* Something went wrong.  Close down and go home.  */

  signal (SIGPIPE, pipe_handler);
  _rmt_shutdown (fildes);
  errno = EIO;
  return -1;
}

/* Read and return the status from remote tape connection FILDES.
   If an error occurred, return -1 and set errno.  */

static int
status (fildes)
     int fildes;
{
  int i;
  char c, *cp;
  char buffer[CMDBUFSIZE];

  /* Read the reply command line.  */

  for (i = 0, cp = buffer; i < CMDBUFSIZE; i++, cp++)
    {
      if (read (READ (fildes), cp, 1) != 1)
	{
	  _rmt_shutdown (fildes);
	  errno = EIO;
	  return -1;
	}
      if (*cp == '\n')
	{
	  *cp = '\0';
	  break;
	}
    }

  if (i == CMDBUFSIZE)
    {
      _rmt_shutdown (fildes);
      errno = EIO;
      return -1;
    }

  /* Check the return status.  */

  for (cp = buffer; *cp; cp++)
    if (*cp != ' ')
      break;

  if (*cp == 'E' || *cp == 'F')
    {
      errno = atoi (cp + 1);
      /* Skip the error message line.  */
      while (read (READ (fildes), &c, 1) == 1)
	if (c == '\n')
	  break;

      if (*cp == 'F')
	_rmt_shutdown (fildes);

      return -1;
    }

  /* Check for mis-synced pipes. */

  if (*cp != 'A')
    {
      _rmt_shutdown (fildes);
      errno = EIO;
      return -1;
    }

  /* Got an `A' (success) response.  */
  return atoi (cp + 1);
}

#ifdef HAVE_NETDB_H
/* Execute /etc/rmt as user USER on remote system HOST using rexec.
   Return a file descriptor of a bidirectional socket for stdin and stdout.
   If USER is NULL, or an empty string, use the current username.

   By default, this code is not used, since it requires that
   the user have a .netrc file in his/her home directory, or that the
   application designer be willing to have rexec prompt for login and
   password info.  This may be unacceptable, and .rhosts files for use
   with rsh are much more common on BSD systems.  */

static int
_rmt_rexec (host, user)
     char *host;
     char *user;
{
  struct servent *rexecserv;
  int save_stdin = dup (fileno (stdin));
  int save_stdout = dup (fileno (stdout));
  int tape_fd;			/* Return value. */

  /* When using cpio -o < filename, stdin is no longer the tty.
     But the rexec subroutine reads the login and the passwd on stdin,
     to allow remote execution of the command.
     So, reopen stdin and stdout on /dev/tty before the rexec and
     give them back their original value after.  */
  if (freopen ("/dev/tty", "r", stdin) == NULL)
    freopen ("/dev/null", "r", stdin);
  if (freopen ("/dev/tty", "w", stdout) == NULL)
    freopen ("/dev/null", "w", stdout);

  rexecserv = getservbyname ("exec", "tcp");
  if (NULL == rexecserv)
    {
      fprintf (stderr, "exec/tcp: service not available");
      exit (1);
    }
  if (user != NULL && *user == '\0')
    user = NULL;
  tape_fd = rexec (&host, rexecserv->s_port, user, NULL,
		   "/etc/rmt", (int *) NULL);
  fclose (stdin);
  fdopen (save_stdin, "r");
  fclose (stdout);
  fdopen (save_stdout, "w");

  return tape_fd;
}

#endif /* HAVE_NETDB_H */

/* Open a magtape device on the system specified in PATH, as the given user.
   PATH has the form `[user@]system:/dev/????'.
   If COMPAT is defined, it can also have the form `system[.user]:/dev/????'.

   OFLAG is O_RDONLY, O_WRONLY, etc.
   MODE is ignored; 0666 is always used.

   If successful, return the remote tape pipe number plus BIAS.
   On error, return -1.  */

int
__rmt_open (path, oflag, mode, bias)
     char *path;
     int oflag;
     int mode;
     int bias;
{
  int i, rc;
  char buffer[CMDBUFSIZE];	/* Command buffer.  */
  char system[MAXHOSTLEN];	/* The remote host name.  */
  char device[CMDBUFSIZE];	/* The remote device name.  */
  char login[CMDBUFSIZE];	/* The remote user name.  */
  char *sys, *dev, *user;	/* For copying into the above buffers.  */
  char *tar_rsh;

  sys = system;
  dev = device;
  user = login;

  /* Find an unused pair of file descriptors.  */

  for (i = 0; i < MAXUNIT; i++)
    if (READ (i) == -1 && WRITE (i) == -1)
      break;

  if (i == MAXUNIT)
    {
      errno = EMFILE;
      return -1;
    }

  /* Pull apart the system and device, and optional user.
     Don't munge the original string.  */

  while (*path != '@'
#ifdef COMPAT
	 && *path != '.'
#endif
	 && *path != ':')
    {
      *sys++ = *path++;
    }
  *sys = '\0';
  path++;

  if (*(path - 1) == '@')
    {
      /* Saw user part of user@host.  Start over. */
      strcpy (user, system);
      sys = system;
      while (*path != ':')
	{
	  *sys++ = *path++;
	}
      *sys = '\0';
      path++;
    }
#ifdef COMPAT
  else if (*(path - 1) == '.')
    {
      while (*path != ':')
	{
	  *user++ = *path++;
	}
      *user = '\0';
      path++;
    }
#endif
  else
    *user = '\0';

  while (*path)
    {
      *dev++ = *path++;
    }
  *dev = '\0';

#ifdef HAVE_NETDB_H
  /* Execute the remote command using rexec.  */
  READ (i) = WRITE (i) = _rmt_rexec (system, login);
  if (READ (i) < 0)
    return -1;
#else /* !HAVE_NETDB_H */
  /* Set up the pipes for the `rsh' command, and fork.  */

  if (pipe (to_rmt[i]) == -1 || pipe (from_rmt[i]) == -1)
    return -1;

  rc = fork ();
  if (rc == -1)
    return -1;

  if (rc == 0)
    {
      /* Child.  */
      close (0);
      dup (to_rmt[i][0]);
      close (to_rmt[i][0]);
      close (to_rmt[i][1]);

      close (1);
      dup (from_rmt[i][1]);
      close (from_rmt[i][0]);
      close (from_rmt[i][1]);

      setuid (getuid ());
      setgid (getgid ());

      tar_rsh = getenv("TAR_RSH");

      if (*login)
	{
	  if (tar_rsh) {
	    execlp (tar_rsh, tar_rsh, "-l", login, system,
		    "/etc/rmt", (char *) 0);
	  } else {
	    execl ("/usr/bin/rsh", "rsh", "-l", login, system,
		   "/etc/rmt", (char *) 0);
	    execlp ("rsh", "rsh", "-l", login, system,
		    "/etc/rmt", (char *) 0);
	  }
	}
      else
	{
	  if (tar_rsh) {
	    execlp (tar_rsh, tar_rsh, system,
		    "/etc/rmt", (char *) 0);
	  } else {
	    execl ("/usr/bin/rsh", "rsh", system,
		   "/etc/rmt", (char *) 0);
	    execlp ("rsh", "rsh", system,
		    "/etc/rmt", (char *) 0);
	  }
	}

      /* Bad problems if we get here.  */

      perror ("cannot execute remote shell");
      _exit (1);
    }

  /* Parent.  */
  close (to_rmt[i][0]);
  close (from_rmt[i][1]);
#endif /* !HAVE_NETDB_H */

  /* Attempt to open the tape device.  */

  sprintf (buffer, "O%s\n%d\n", device, oflag);
  if (command (i, buffer) == -1 || status (i) == -1)
    return -1;

  return i + bias;
}

/* Close remote tape connection FILDES and shut down.
   Return 0 if successful, -1 on error.  */

int
__rmt_close (fildes)
     int fildes;
{
  int rc;

  if (command (fildes, "C\n") == -1)
    return -1;

  rc = status (fildes);
  _rmt_shutdown (fildes);
  return rc;
}

/* Read up to NBYTE bytes into BUF from remote tape connection FILDES.
   Return the number of bytes read on success, -1 on error.  */

int
__rmt_read (fildes, buf, nbyte)
     int fildes;
     char *buf;
     unsigned int nbyte;
{
  int rc, i;
  char buffer[CMDBUFSIZE];

  sprintf (buffer, "R%d\n", nbyte);
  if (command (fildes, buffer) == -1 || (rc = status (fildes)) == -1)
    return -1;

  for (i = 0; i < rc; i += nbyte, buf += nbyte)
    {
      nbyte = read (READ (fildes), buf, rc - i);
      if (nbyte <= 0)
	{
	  _rmt_shutdown (fildes);
	  errno = EIO;
	  return -1;
	}
    }

  return rc;
}

/* Write NBYTE bytes from BUF to remote tape connection FILDES.
   Return the number of bytes written on success, -1 on error.  */

int
__rmt_write (fildes, buf, nbyte)
     int fildes;
     char *buf;
     unsigned int nbyte;
{
  char buffer[CMDBUFSIZE];
  RETSIGTYPE (*pipe_handler) ();

  sprintf (buffer, "W%d\n", nbyte);
  if (command (fildes, buffer) == -1)
    return -1;

  pipe_handler = signal (SIGPIPE, SIG_IGN);
  if (write (WRITE (fildes), buf, nbyte) == nbyte)
    {
      signal (SIGPIPE, pipe_handler);
      return status (fildes);
    }

  /* Write error.  */
  signal (SIGPIPE, pipe_handler);
  _rmt_shutdown (fildes);
  errno = EIO;
  return -1;
}

/* Perform an imitation lseek operation on remote tape connection FILDES.
   Return the new file offset if successful, -1 if on error.  */

long
__rmt_lseek (fildes, offset, whence)
     int fildes;
     long offset;
     int whence;
{
  char buffer[CMDBUFSIZE];

  sprintf (buffer, "L%ld\n%d\n", offset, whence);
  if (command (fildes, buffer) == -1)
    return -1;

  return status (fildes);
}

/* Perform a raw tape operation on remote tape connection FILDES.
   Return the results of the ioctl, or -1 on error.  */

#ifdef MTIOCTOP
int
__rmt_ioctl (fildes, op, arg)
     int fildes, op;
     char *arg;
{
  char c;
  int rc, cnt;
  char buffer[CMDBUFSIZE];

  switch (op)
    {
    default:
      errno = EINVAL;
      return -1;

    case MTIOCTOP:
      /* MTIOCTOP is the easy one.  Nothing is transfered in binary.  */
      sprintf (buffer, "I%d\n%d\n", ((struct mtop *) arg)->mt_op,
	       ((struct mtop *) arg)->mt_count);
      if (command (fildes, buffer) == -1)
	return -1;
      return status (fildes);	/* Return the count.  */

    case MTIOCGET:
      /* Grab the status and read it directly into the structure.
	 This assumes that the status buffer is not padded
	 and that 2 shorts fit in a long without any word
	 alignment problems; i.e., the whole struct is contiguous.
	 NOTE - this is probably NOT a good assumption.  */

      if (command (fildes, "S") == -1 || (rc = status (fildes)) == -1)
	return -1;

      for (; rc > 0; rc -= cnt, arg += cnt)
	{
	  cnt = read (READ (fildes), arg, rc);
	  if (cnt <= 0)
	    {
	      _rmt_shutdown (fildes);
	      errno = EIO;
	      return -1;
	    }
	}

      /* Check for byte position.  mt_type is a small integer field
	 (normally) so we will check its magnitude.  If it is larger than
	 256, we will assume that the bytes are swapped and go through
	 and reverse all the bytes.  */

      if (((struct mtget *) arg)->mt_type < 256)
	return 0;

      for (cnt = 0; cnt < rc; cnt += 2)
	{
	  c = arg[cnt];
	  arg[cnt] = arg[cnt + 1];
	  arg[cnt + 1] = c;
	}

      return 0;
    }
}

#endif
