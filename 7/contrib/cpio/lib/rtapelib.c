/* Functions for communicating with a remote tape drive.

   Copyright (C) 1988, 1992, 1994, 1996, 1997, 1999, 2000, 2001, 2004,
   2005, 2006 Free Software Foundation, Inc.

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
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* The man page rmt(8) for /etc/rmt documents the remote mag tape protocol
   which rdump and rrestore use.  Unfortunately, the man page is *WRONG*.
   The author of the routines I'm including originally wrote his code just
   based on the man page, and it didn't work, so he went to the rdump source
   to figure out why.  The only thing he had to change was to check for the
   'F' return code in addition to the 'E', and to separate the various
   arguments with \n instead of a space.  I personally don't think that this
   is much of a problem, but I wanted to point it out. -- Arnold Robbins

   Originally written by Jeff Lee, modified some by Arnold Robbins.  Redone
   as a library that can replace open, read, write, etc., by Fred Fish, with
   some additional work by Arnold Robbins.  Modified to make all rmt* calls
   into macros for speed by Jay Fenlason.  Use -DWITH_REXEC for rexec
   code, courtesy of Dan Kegel.  */

#include "system.h"
#include "system-ioctl.h"

#include <safe-read.h>
#include <full-write.h>

/* Try hard to get EOPNOTSUPP defined.  486/ISC has it in net/errno.h,
   3B2/SVR3 has it in sys/inet.h.  Otherwise, like on MSDOS, use EINVAL.  */

#ifndef EOPNOTSUPP
# if HAVE_NET_ERRNO_H
#  include <net/errno.h>
# endif
# if HAVE_SYS_INET_H
#  include <sys/inet.h>
# endif
# ifndef EOPNOTSUPP
#  define EOPNOTSUPP EINVAL
# endif
#endif

#include <signal.h>

#if HAVE_NETDB_H
# include <netdb.h>
#endif

#include <rmt.h>
#include <rmt-command.h>

/* Exit status if exec errors.  */
#define EXIT_ON_EXEC_ERROR 128

/* FIXME: Size of buffers for reading and writing commands to rmt.  */
#define COMMAND_BUFFER_SIZE 64

#ifndef RETSIGTYPE
# define RETSIGTYPE void
#endif

/* FIXME: Maximum number of simultaneous remote tape connections.  */
#define MAXUNIT	4

#define	PREAD 0			/* read  file descriptor from pipe() */
#define	PWRITE 1		/* write file descriptor from pipe() */

/* Return the parent's read side of remote tape connection Fd.  */
#define READ_SIDE(Fd) (from_remote[Fd][PREAD])

/* Return the parent's write side of remote tape connection Fd.  */
#define WRITE_SIDE(Fd) (to_remote[Fd][PWRITE])

/* The pipes for receiving data from remote tape drives.  */
static int from_remote[MAXUNIT][2] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};

/* The pipes for sending data to remote tape drives.  */
static int to_remote[MAXUNIT][2] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};

char *rmt_command = DEFAULT_RMT_COMMAND;

/* Temporary variable used by macros in rmt.h.  */
char *rmt_dev_name__;

/* If true, always consider file names to be local, even if they contain
   colons */
bool force_local_option;



/* Close remote tape connection HANDLE, and reset errno to ERRNO_VALUE.  */
static void
_rmt_shutdown (int handle, int errno_value)
{
  close (READ_SIDE (handle));
  close (WRITE_SIDE (handle));
  READ_SIDE (handle) = -1;
  WRITE_SIDE (handle) = -1;
  errno = errno_value;
}

/* Attempt to perform the remote tape command specified in BUFFER on
   remote tape connection HANDLE.  Return 0 if successful, -1 on
   error.  */
static int
do_command (int handle, const char *buffer)
{
  /* Save the current pipe handler and try to make the request.  */

  size_t length = strlen (buffer);
  RETSIGTYPE (*pipe_handler) () = signal (SIGPIPE, SIG_IGN);
  ssize_t written = full_write (WRITE_SIDE (handle), buffer, length);
  signal (SIGPIPE, pipe_handler);

  if (written == length)
    return 0;

  /* Something went wrong.  Close down and go home.  */

  _rmt_shutdown (handle, EIO);
  return -1;
}

static char *
get_status_string (int handle, char *command_buffer)
{
  char *cursor;
  int counter;

  /* Read the reply command line.  */

  for (counter = 0, cursor = command_buffer;
       counter < COMMAND_BUFFER_SIZE;
       counter++, cursor++)
    {
      if (safe_read (READ_SIDE (handle), cursor, 1) != 1)
	{
	  _rmt_shutdown (handle, EIO);
	  return 0;
	}
      if (*cursor == '\n')
	{
	  *cursor = '\0';
	  break;
	}
    }

  if (counter == COMMAND_BUFFER_SIZE)
    {
      _rmt_shutdown (handle, EIO);
      return 0;
    }

  /* Check the return status.  */

  for (cursor = command_buffer; *cursor; cursor++)
    if (*cursor != ' ')
      break;

  if (*cursor == 'E' || *cursor == 'F')
    {
      /* Skip the error message line.  */

      /* FIXME: there is better to do than merely ignoring error messages
	 coming from the remote end.  Translate them, too...  */

      {
	char character;

	while (safe_read (READ_SIDE (handle), &character, 1) == 1)
	  if (character == '\n')
	    break;
      }

      errno = atoi (cursor + 1);

      if (*cursor == 'F')
	_rmt_shutdown (handle, errno);

      return 0;
    }

  /* Check for mis-synced pipes.  */

  if (*cursor != 'A')
    {
      _rmt_shutdown (handle, EIO);
      return 0;
    }

  /* Got an `A' (success) response.  */

  return cursor + 1;
}

/* Read and return the status from remote tape connection HANDLE.  If
   an error occurred, return -1 and set errno.  */
static long int
get_status (int handle)
{
  char command_buffer[COMMAND_BUFFER_SIZE];
  const char *status = get_status_string (handle, command_buffer);
  if (status)
    {
      long int result = atol (status);
      if (0 <= result)
	return result;
      errno = EIO;
    }
  return -1;
}

static off_t
get_status_off (int handle)
{
  char command_buffer[COMMAND_BUFFER_SIZE];
  const char *status = get_status_string (handle, command_buffer);

  if (! status)
    return -1;
  else
    {
      /* Parse status, taking care to check for overflow.
	 We can't use standard functions,
	 since off_t might be longer than long.  */

      off_t count = 0;
      int negative;

      for (;  *status == ' ' || *status == '\t';  status++)
	continue;

      negative = *status == '-';
      status += negative || *status == '+';

      for (;;)
	{
	  int digit = *status++ - '0';
	  if (9 < (unsigned) digit)
	    break;
	  else
	    {
	      off_t c10 = 10 * count;
	      off_t nc = negative ? c10 - digit : c10 + digit;
	      if (c10 / 10 != count || (negative ? c10 < nc : nc < c10))
		return -1;
	      count = nc;
	    }
	}

      return count;
    }
}

#if WITH_REXEC

/* Execute /etc/rmt as user USER on remote system HOST using rexec.
   Return a file descriptor of a bidirectional socket for stdin and
   stdout.  If USER is zero, use the current username.

   By default, this code is not used, since it requires that the user
   have a .netrc file in his/her home directory, or that the
   application designer be willing to have rexec prompt for login and
   password info.  This may be unacceptable, and .rhosts files for use
   with rsh are much more common on BSD systems.  */
static int
_rmt_rexec (char *host, char *user)
{
  int saved_stdin = dup (STDIN_FILENO);
  int saved_stdout = dup (STDOUT_FILENO);
  struct servent *rexecserv;
  int result;

  /* When using cpio -o < filename, stdin is no longer the tty.  But the
     rexec subroutine reads the login and the passwd on stdin, to allow
     remote execution of the command.  So, reopen stdin and stdout on
     /dev/tty before the rexec and give them back their original value
     after.  */

  if (! freopen ("/dev/tty", "r", stdin))
    freopen ("/dev/null", "r", stdin);
  if (! freopen ("/dev/tty", "w", stdout))
    freopen ("/dev/null", "w", stdout);

  if (rexecserv = getservbyname ("exec", "tcp"), !rexecserv)
    error (EXIT_ON_EXEC_ERROR, 0, _("exec/tcp: Service not available"));

  result = rexec (&host, rexecserv->s_port, user, 0, rmt_command, 0);
  if (fclose (stdin) == EOF)
    error (0, errno, _("stdin"));
  fdopen (saved_stdin, "r");
  if (fclose (stdout) == EOF)
    error (0, errno, _("stdout"));
  fdopen (saved_stdout, "w");

  return result;
}

#endif /* WITH_REXEC */

/* Place into BUF a string representing OFLAG, which must be suitable
   as argument 2 of `open'.  BUF must be large enough to hold the
   result.  This function should generate a string that decode_oflag
   can parse.  */
static void
encode_oflag (char *buf, int oflag)
{
  sprintf (buf, "%d ", oflag);

  switch (oflag & O_ACCMODE)
    {
    case O_RDONLY: strcat (buf, "O_RDONLY"); break;
    case O_RDWR: strcat (buf, "O_RDWR"); break;
    case O_WRONLY: strcat (buf, "O_WRONLY"); break;
    default: abort ();
    }

#ifdef O_APPEND
  if (oflag & O_APPEND) strcat (buf, "|O_APPEND");
#endif
  if (oflag & O_CREAT) strcat (buf, "|O_CREAT");
#ifdef O_DSYNC
  if (oflag & O_DSYNC) strcat (buf, "|O_DSYNC");
#endif
  if (oflag & O_EXCL) strcat (buf, "|O_EXCL");
#ifdef O_LARGEFILE
  if (oflag & O_LARGEFILE) strcat (buf, "|O_LARGEFILE");
#endif
#ifdef O_NOCTTY
  if (oflag & O_NOCTTY) strcat (buf, "|O_NOCTTY");
#endif
  if (oflag & O_NONBLOCK) strcat (buf, "|O_NONBLOCK");
#ifdef O_RSYNC
  if (oflag & O_RSYNC) strcat (buf, "|O_RSYNC");
#endif
#ifdef O_SYNC
  if (oflag & O_SYNC) strcat (buf, "|O_SYNC");
#endif
  if (oflag & O_TRUNC) strcat (buf, "|O_TRUNC");
}

/* Open a file (a magnetic tape device?) on the system specified in
   FILE_NAME, as the given user. FILE_NAME has the form `[USER@]HOST:FILE'.
   OPEN_MODE is O_RDONLY, O_WRONLY, etc.  If successful, return the
   remote pipe number plus BIAS.  REMOTE_SHELL may be overridden.  On
   error, return -1.  */
int
rmt_open__ (const char *file_name, int open_mode, int bias,
            const char *remote_shell)
{
  int remote_pipe_number;	/* pseudo, biased file descriptor */
  char *file_name_copy;		/* copy of file_name string */
  char *remote_host;		/* remote host name */
  char *remote_file;		/* remote file name (often a device) */
  char *remote_user;		/* remote user name */

  /* Find an unused pair of file descriptors.  */

  for (remote_pipe_number = 0;
       remote_pipe_number < MAXUNIT;
       remote_pipe_number++)
    if (READ_SIDE (remote_pipe_number) == -1
	&& WRITE_SIDE (remote_pipe_number) == -1)
      break;

  if (remote_pipe_number == MAXUNIT)
    {
      errno = EMFILE;
      return -1;
    }

  /* Pull apart the system and device, and optional user.  */

  {
    char *cursor;

    file_name_copy = xstrdup (file_name);
    remote_host = file_name_copy;
    remote_user = 0;
    remote_file = 0;

    for (cursor = file_name_copy; *cursor; cursor++)
      switch (*cursor)
	{
	default:
	  break;

	case '\n':
	  /* Do not allow newlines in the file_name, since the protocol
	     uses newline delimiters.  */
	  free (file_name_copy);
	  errno = ENOENT;
	  return -1;

	case '@':
	  if (!remote_user)
	    {
	      remote_user = remote_host;
	      *cursor = '\0';
	      remote_host = cursor + 1;
	    }
	  break;

	case ':':
	  if (!remote_file)
	    {
	      *cursor = '\0';
	      remote_file = cursor + 1;
	    }
	  break;
	}
  }

  /* FIXME: Should somewhat validate the decoding, here.  */

  if (remote_user && *remote_user == '\0')
    remote_user = 0;

#if WITH_REXEC

  /* Execute the remote command using rexec.  */

  READ_SIDE (remote_pipe_number) = _rmt_rexec (remote_host, remote_user);
  if (READ_SIDE (remote_pipe_number) < 0)
    {
      int e = errno;
      free (file_name_copy);
      errno = e;
      return -1;
    }

  WRITE_SIDE (remote_pipe_number) = READ_SIDE (remote_pipe_number);

#else /* not WITH_REXEC */
  {
    const char *remote_shell_basename;
    pid_t status;

    /* Identify the remote command to be executed.  */

    if (!remote_shell)
      {
#ifdef REMOTE_SHELL
	remote_shell = REMOTE_SHELL;
#else
	free (file_name_copy);
	errno = EIO;
	return -1;
#endif
      }
    remote_shell_basename = base_name (remote_shell);

    /* Set up the pipes for the `rsh' command, and fork.  */

    if (pipe (to_remote[remote_pipe_number]) == -1
	|| pipe (from_remote[remote_pipe_number]) == -1)
      {
	int e = errno;
	free (file_name_copy);
	errno = e;
	return -1;
      }

    status = fork ();
    if (status == -1)
      {
	int e = errno;
	free (file_name_copy);
	errno = e;
	return -1;
      }

    if (status == 0)
      {
	/* Child.  */

	close (STDIN_FILENO);
	dup (to_remote[remote_pipe_number][PREAD]);
	close (to_remote[remote_pipe_number][PREAD]);
	close (to_remote[remote_pipe_number][PWRITE]);

	close (STDOUT_FILENO);
	dup (from_remote[remote_pipe_number][PWRITE]);
	close (from_remote[remote_pipe_number][PREAD]);
	close (from_remote[remote_pipe_number][PWRITE]);

	sys_reset_uid_gid ();

	if (remote_user)
	  execl (remote_shell, remote_shell_basename, remote_host,
		 "-l", remote_user, rmt_command, (char *) 0);
	else
	  execl (remote_shell, remote_shell_basename, remote_host,
		 rmt_command, (char *) 0);

	/* Bad problems if we get here.  */

	/* In a previous version, _exit was used here instead of exit.  */
	error (EXIT_ON_EXEC_ERROR, errno, _("Cannot execute remote shell"));
      }

    /* Parent.  */

    close (from_remote[remote_pipe_number][PWRITE]);
    close (to_remote[remote_pipe_number][PREAD]);
  }
#endif /* not WITH_REXEC */

  /* Attempt to open the tape device.  */

  {
    size_t remote_file_len = strlen (remote_file);
    char *command_buffer = xmalloc (remote_file_len + 1000);
    sprintf (command_buffer, "O%s\n", remote_file);
    encode_oflag (command_buffer + remote_file_len + 2, open_mode);
    strcat (command_buffer, "\n");
    if (do_command (remote_pipe_number, command_buffer) == -1
	|| get_status (remote_pipe_number) == -1)
      {
	int e = errno;
	free (command_buffer);
	free (file_name_copy);
	_rmt_shutdown (remote_pipe_number, e);
	return -1;
      }
    free (command_buffer);
  }

  free (file_name_copy);
  return remote_pipe_number + bias;
}

/* Close remote tape connection HANDLE and shut down.  Return 0 if
   successful, -1 on error.  */
int
rmt_close__ (int handle)
{
  long int status;

  if (do_command (handle, "C\n") == -1)
    return -1;

  status = get_status (handle);
  _rmt_shutdown (handle, errno);
  return status;
}

/* Read up to LENGTH bytes into BUFFER from remote tape connection HANDLE.
   Return the number of bytes read on success, SAFE_READ_ERROR on error.  */
size_t
rmt_read__ (int handle, char *buffer, size_t length)
{
  char command_buffer[COMMAND_BUFFER_SIZE];
  size_t status;
  size_t rlen;
  size_t counter;

  sprintf (command_buffer, "R%lu\n", (unsigned long) length);
  if (do_command (handle, command_buffer) == -1
      || (status = get_status (handle)) == SAFE_READ_ERROR
      || status > length)
    return SAFE_READ_ERROR;

  for (counter = 0; counter < status; counter += rlen, buffer += rlen)
    {
      rlen = safe_read (READ_SIDE (handle), buffer, status - counter);
      if (rlen == SAFE_READ_ERROR || rlen == 0)
	{
	  _rmt_shutdown (handle, EIO);
	  return SAFE_READ_ERROR;
	}
    }

  return status;
}

/* Write LENGTH bytes from BUFFER to remote tape connection HANDLE.
   Return the number of bytes written.  */
size_t
rmt_write__ (int handle, char *buffer, size_t length)
{
  char command_buffer[COMMAND_BUFFER_SIZE];
  RETSIGTYPE (*pipe_handler) ();
  size_t written;

  sprintf (command_buffer, "W%lu\n", (unsigned long) length);
  if (do_command (handle, command_buffer) == -1)
    return 0;

  pipe_handler = signal (SIGPIPE, SIG_IGN);
  written = full_write (WRITE_SIDE (handle), buffer, length);
  signal (SIGPIPE, pipe_handler);
  if (written == length)
    {
      long int r = get_status (handle);
      if (r < 0)
	return 0;
      if (r == length)
	return length;
      written = r;
    }

  /* Write error.  */

  _rmt_shutdown (handle, EIO);
  return written;
}

/* Perform an imitation lseek operation on remote tape connection
   HANDLE.  Return the new file offset if successful, -1 if on error.  */
off_t
rmt_lseek__ (int handle, off_t offset, int whence)
{
  char command_buffer[COMMAND_BUFFER_SIZE];
  char operand_buffer[UINTMAX_STRSIZE_BOUND];
  uintmax_t u = offset < 0 ? - (uintmax_t) offset : (uintmax_t) offset;
  char *p = operand_buffer + sizeof operand_buffer;

  *--p = 0;
  do
    *--p = '0' + (int) (u % 10);
  while ((u /= 10) != 0);
  if (offset < 0)
    *--p = '-';

  switch (whence)
    {
    case SEEK_SET: whence = 0; break;
    case SEEK_CUR: whence = 1; break;
    case SEEK_END: whence = 2; break;
    default: abort ();
    }

  sprintf (command_buffer, "L%s\n%d\n", p, whence);

  if (do_command (handle, command_buffer) == -1)
    return -1;

  return get_status_off (handle);
}

/* Perform a raw tape operation on remote tape connection HANDLE.
   Return the results of the ioctl, or -1 on error.  */
int
rmt_ioctl__ (int handle, int operation, char *argument)
{
  switch (operation)
    {
    default:
      errno = EOPNOTSUPP;
      return -1;

#ifdef MTIOCTOP
    case MTIOCTOP:
      {
	char command_buffer[COMMAND_BUFFER_SIZE];
	char operand_buffer[UINTMAX_STRSIZE_BOUND];
	uintmax_t u = (((struct mtop *) argument)->mt_count < 0
		       ? - (uintmax_t) ((struct mtop *) argument)->mt_count
		       : (uintmax_t) ((struct mtop *) argument)->mt_count);
	char *p = operand_buffer + sizeof operand_buffer;

        *--p = 0;
	do
	  *--p = '0' + (int) (u % 10);
	while ((u /= 10) != 0);
	if (((struct mtop *) argument)->mt_count < 0)
	  *--p = '-';

	/* MTIOCTOP is the easy one.  Nothing is transferred in binary.  */

	sprintf (command_buffer, "I%d\n%s\n",
		 ((struct mtop *) argument)->mt_op, p);
	if (do_command (handle, command_buffer) == -1)
	  return -1;

	return get_status (handle);
      }
#endif /* MTIOCTOP */

#ifdef MTIOCGET
    case MTIOCGET:
      {
	ssize_t status;
	size_t counter;

	/* Grab the status and read it directly into the structure.  This
	   assumes that the status buffer is not padded and that 2 shorts
	   fit in a long without any word alignment problems; i.e., the
	   whole struct is contiguous.  NOTE - this is probably NOT a good
	   assumption.  */

	if (do_command (handle, "S") == -1
	    || (status = get_status (handle), status == -1))
	  return -1;

	for (; status > 0; status -= counter, argument += counter)
	  {
	    counter = safe_read (READ_SIDE (handle), argument, status);
	    if (counter == SAFE_READ_ERROR || counter == 0)
	      {
		_rmt_shutdown (handle, EIO);
		return -1;
	      }
	  }

	/* Check for byte position.  mt_type (or mt_model) is a small integer
	   field (normally) so we will check its magnitude.  If it is larger
	   than 256, we will assume that the bytes are swapped and go through
	   and reverse all the bytes.  */

	if (((struct mtget *) argument)->MTIO_CHECK_FIELD < 256)
	  return 0;

	for (counter = 0; counter < status; counter += 2)
	  {
	    char copy = argument[counter];

	    argument[counter] = argument[counter + 1];
	    argument[counter + 1] = copy;
	  }

	return 0;
      }
#endif /* MTIOCGET */

    }
}
