/* Buffer management for tar.

   Copyright 1988, 1992, 1993, 1994, 1996, 1997, 1999, 2000, 2001 Free
   Software Foundation, Inc.

   Written by John Gilmore, on 1985-08-25.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* $FreeBSD$ */

#include "system.h"

#include <signal.h>

#if MSDOS
# include <process.h>
#endif

#if XENIX
# include <sys/inode.h>
#endif

#include <fnmatch.h>
#include <human.h>
#include <quotearg.h>

#include "common.h"
#include "rmt.h"

#define	PREAD 0			/* read file descriptor from pipe() */
#define	PWRITE 1		/* write file descriptor from pipe() */

/* Number of retries before giving up on read.  */
#define	READ_ERROR_MAX 10

/* Globbing pattern to append to volume label if initial match failed.  */
#define VOLUME_LABEL_APPEND " Volume [1-9]*"

/* Variables.  */

static tarlong prev_written;	/* bytes written on previous volumes */
static tarlong bytes_written;	/* bytes written on this volume */

/* FIXME: The following variables should ideally be static to this
   module.  However, this cannot be done yet.  The cleanup continues!  */

union block *record_start;	/* start of record of archive */
union block *record_end;	/* last+1 block of archive record */
union block *current_block;	/* current block of archive */
enum access_mode access_mode;	/* how do we handle the archive */
off_t records_read;		/* number of records read from this archive */
off_t records_written;		/* likewise, for records written */

static struct stat archive_stat; /* stat block for archive file */

static off_t record_start_block; /* block ordinal at record_start */

/* Where we write list messages (not errors, not interactions) to.  Stdout
   unless we're writing a pipe, in which case stderr.  */
FILE *stdlis;

static void backspace_output PARAMS ((void));
static int new_volume PARAMS ((enum access_mode));
static void archive_write_error PARAMS ((ssize_t)) __attribute__ ((noreturn));
static void archive_read_error PARAMS ((void));

#if !MSDOS
/* Obnoxious test to see if dimwit is trying to dump the archive.  */
dev_t ar_dev;
ino_t ar_ino;
#endif

/* PID of child program, if compress_option or remote archive access.  */
static pid_t child_pid;

/* Error recovery stuff  */
static int read_error_count;

/* Have we hit EOF yet?  */
static int hit_eof;

/* Checkpointing counter */
static int checkpoint;

/* We're reading, but we just read the last block and its time to update.  */
/* As least EXTERN like this one as possible.  FIXME!  */
extern int time_to_start_writing;

int file_to_switch_to = -1;	/* if remote update, close archive, and use
				   this descriptor to write to */

static int volno = 1;		/* which volume of a multi-volume tape we're
				   on */
static int global_volno = 1;	/* volume number to print in external
				   messages */
static pid_t grandchild_pid;

/* The pointer save_name, which is set in function dump_file() of module
   create.c, points to the original long filename instead of the new,
   shorter mangled name that is set in start_header() of module create.c.
   The pointer save_name is only used in multi-volume mode when the file
   being processed is non-sparse; if a file is split between volumes, the
   save_name is used in generating the LF_MULTIVOL record on the second
   volume.  (From Pierce Cantrell, 1991-08-13.)  */

char *save_name;		/* name of the file we are currently writing */
off_t save_totsize;		/* total size of file we are writing, only
				   valid if save_name is nonzero */
off_t save_sizeleft;		/* where we are in the file we are writing,
				   only valid if save_name is nonzero */

bool write_archive_to_stdout;

/* Used by flush_read and flush_write to store the real info about saved
   names.  */
static char *real_s_name;
static off_t real_s_totsize;
static off_t real_s_sizeleft;

/* Functions.  */

void
print_total_written (void)
{
  tarlong written = prev_written + bytes_written;
  char bytes[sizeof (tarlong) * CHAR_BIT];
  char abbr[LONGEST_HUMAN_READABLE + 1];
  char rate[LONGEST_HUMAN_READABLE + 1];
  double seconds;

#if HAVE_CLOCK_GETTIME
  struct timespec now;
  if (clock_gettime (CLOCK_REALTIME, &now) == 0)
    seconds = ((now.tv_sec - start_timespec.tv_sec)
	       + (now.tv_nsec - start_timespec.tv_nsec) / 1e9);
  else
#endif
    seconds = time (0) - start_time;

  sprintf (bytes, TARLONG_FORMAT, written);

  /* Amanda 2.4.1p1 looks for "Total bytes written: [0-9][0-9]*".  */
  fprintf (stderr, _("Total bytes written: %s (%sB, %sB/s)\n"), bytes,
	   human_readable ((uintmax_t) written, abbr, 1, -1024),
	   (0 < seconds && written / seconds < (uintmax_t) -1
	    ? human_readable ((uintmax_t) (written / seconds), rate, 1, -1024)
	    : "?"));
}

/* Compute and return the block ordinal at current_block.  */
off_t
current_block_ordinal (void)
{
  return record_start_block + (current_block - record_start);
}

/* If the EOF flag is set, reset it, as well as current_block, etc.  */
void
reset_eof (void)
{
  if (hit_eof)
    {
      hit_eof = 0;
      current_block = record_start;
      record_end = record_start + blocking_factor;
      access_mode = ACCESS_WRITE;
    }
}

/* Return the location of the next available input or output block.
   Return zero for EOF.  Once we have returned zero, we just keep returning
   it, to avoid accidentally going on to the next file on the tape.  */
union block *
find_next_block (void)
{
  if (current_block == record_end)
    {
      if (hit_eof)
	return 0;
      flush_archive ();
      if (current_block == record_end)
	{
	  hit_eof = 1;
	  return 0;
	}
    }
  return current_block;
}

/* Indicate that we have used all blocks up thru BLOCK.
   FIXME: should the arg have an off-by-1?  */
void
set_next_block_after (union block *block)
{
  while (block >= current_block)
    current_block++;

  /* Do *not* flush the archive here.  If we do, the same argument to
     set_next_block_after could mean the next block (if the input record
     is exactly one block long), which is not what is intended.  */

  if (current_block > record_end)
    abort ();
}

/* Return the number of bytes comprising the space between POINTER
   through the end of the current buffer of blocks.  This space is
   available for filling with data, or taking data from.  POINTER is
   usually (but not always) the result previous find_next_block call.  */
size_t
available_space_after (union block *pointer)
{
  return record_end->buffer - pointer->buffer;
}

/* Close file having descriptor FD, and abort if close unsuccessful.  */
static void
xclose (int fd)
{
  if (close (fd) != 0)
    close_error (_("(pipe)"));
}

/* Duplicate file descriptor FROM into becoming INTO.
   INTO is closed first and has to be the next available slot.  */
static void
xdup2 (int from, int into)
{
  if (from != into)
    {
      int status = close (into);

      if (status != 0 && errno != EBADF)
	{
	  int e = errno;
	  FATAL_ERROR ((0, e, _("Cannot close")));
	}
      status = dup (from);
      if (status != into)
	{
	  if (status < 0)
	    {
	      int e = errno;
	      FATAL_ERROR ((0, e, _("Cannot dup")));
	    }
	  abort ();
	}
      xclose (from);
    }
}

#if MSDOS

/* Set ARCHIVE for writing, then compressing an archive.  */
static void
child_open_for_compress (void)
{
  FATAL_ERROR ((0, 0, _("Cannot use compressed or remote archives")));
}

/* Set ARCHIVE for uncompressing, then reading an archive.  */
static void
child_open_for_uncompress (void)
{
  FATAL_ERROR ((0, 0, _("Cannot use compressed or remote archives")));
}

#else /* not MSDOS */

/* Return nonzero if NAME is the name of a regular file, or if the file
   does not exist (so it would be created as a regular file).  */
static int
is_regular_file (const char *name)
{
  struct stat stbuf;

  if (stat (name, &stbuf) == 0)
    return S_ISREG (stbuf.st_mode);
  else
    return errno == ENOENT;
}

static ssize_t
write_archive_buffer (void)
{
  ssize_t status;
  ssize_t written = 0;

  while (0 <= (status = rmtwrite (archive, record_start->buffer + written,
				  record_size - written)))
    {
      written += status;
      if (written == record_size
	  || _isrmt (archive)
	  || ! (S_ISFIFO (archive_stat.st_mode)
		|| S_ISSOCK (archive_stat.st_mode)))
	break;
    }

  return written ? written : status;
}

/* Set ARCHIVE for writing, then compressing an archive.  */
static void
child_open_for_compress (void)
{
  int parent_pipe[2];
  int child_pipe[2];
  int wait_status;

  xpipe (parent_pipe);
  child_pid = xfork ();

  if (child_pid > 0)
    {
      /* The parent tar is still here!  Just clean up.  */

      archive = parent_pipe[PWRITE];
      xclose (parent_pipe[PREAD]);
      return;
    }

  /* The new born child tar is here!  */

  program_name = _("tar (child)");

  xdup2 (parent_pipe[PREAD], STDIN_FILENO);
  xclose (parent_pipe[PWRITE]);

  /* Check if we need a grandchild tar.  This happens only if either:
     a) we are writing stdout: to force reblocking;
     b) the file is to be accessed by rmt: compressor doesn't know how;
     c) the file is not a plain file.  */

  if (strcmp (archive_name_array[0], "-") != 0
      && !_remdev (archive_name_array[0])
      && is_regular_file (archive_name_array[0]))
    {
      if (backup_option)
	maybe_backup_file (archive_name_array[0], 1);

      /* We don't need a grandchild tar.  Open the archive and launch the
	 compressor.  */

      archive = creat (archive_name_array[0], MODE_RW);
      if (archive < 0)
	{
	  int saved_errno = errno;

	  if (backup_option)
	    undo_last_backup ();
	  errno = saved_errno;
	  open_fatal (archive_name_array[0]);
	}
      xdup2 (archive, STDOUT_FILENO);
      execlp (use_compress_program_option, use_compress_program_option,
	      (char *) 0);
      exec_fatal (use_compress_program_option);
    }

  /* We do need a grandchild tar.  */

  xpipe (child_pipe);
  grandchild_pid = xfork ();

  if (grandchild_pid == 0)
    {
      /* The newborn grandchild tar is here!  Launch the compressor.  */

      program_name = _("tar (grandchild)");

      xdup2 (child_pipe[PWRITE], STDOUT_FILENO);
      xclose (child_pipe[PREAD]);
      execlp (use_compress_program_option, use_compress_program_option,
	      (char *) 0);
      exec_fatal (use_compress_program_option);
    }

  /* The child tar is still here!  */

  /* Prepare for reblocking the data from the compressor into the archive.  */

  xdup2 (child_pipe[PREAD], STDIN_FILENO);
  xclose (child_pipe[PWRITE]);

  if (strcmp (archive_name_array[0], "-") == 0)
    archive = STDOUT_FILENO;
  else
    {
      archive = rmtcreat (archive_name_array[0], MODE_RW, rsh_command_option);
      if (archive < 0)
	open_fatal (archive_name_array[0]);
    }

  /* Let's read out of the stdin pipe and write an archive.  */

  while (1)
    {
      ssize_t status = 0;
      char *cursor;
      size_t length;

      /* Assemble a record.  */

      for (length = 0, cursor = record_start->buffer;
	   length < record_size;
	   length += status, cursor += status)
	{
	  size_t size = record_size - length;

	  if (size < BLOCKSIZE)
	    size = BLOCKSIZE;
	  status = safe_read (STDIN_FILENO, cursor, size);
	  if (status <= 0)
	    break;
	}

      if (status < 0)
	read_fatal (use_compress_program_option);

      /* Copy the record.  */

      if (status == 0)
	{
	  /* We hit the end of the file.  Write last record at
	     full length, as the only role of the grandchild is
	     doing proper reblocking.  */

	  if (length > 0)
	    {
	      memset (record_start->buffer + length, 0, record_size - length);
	      status = write_archive_buffer ();
	      if (status != record_size)
		archive_write_error (status);
	    }

	  /* There is nothing else to read, break out.  */
	  break;
	}

      status = write_archive_buffer ();
      if (status != record_size)
	archive_write_error (status);
    }

#if 0
  close_archive ();
#endif

  /* Propagate any failure of the grandchild back to the parent.  */

  while (waitpid (grandchild_pid, &wait_status, 0) == -1)
    if (errno != EINTR)
      {
	waitpid_error (use_compress_program_option);
	break;
      }

  if (WIFSIGNALED (wait_status))
    {
      kill (child_pid, WTERMSIG (wait_status));
      exit_status = TAREXIT_FAILURE;
    }
  else if (WEXITSTATUS (wait_status) != 0)
    exit_status = WEXITSTATUS (wait_status);

  exit (exit_status);
}

static void
sig_propagate(int sig)
{
  kill (grandchild_pid, sig);
  exit (TAREXIT_FAILURE);
}

/* Set ARCHIVE for uncompressing, then reading an archive.  */
static void
child_open_for_uncompress (void)
{
  int parent_pipe[2];
  int child_pipe[2];
  int wait_status;

  xpipe (parent_pipe);
  child_pid = xfork ();

  if (child_pid > 0)
    {
      /* The parent tar is still here!  Just clean up.  */

      read_full_records_option = 1;
      archive = parent_pipe[PREAD];
      xclose (parent_pipe[PWRITE]);
      return;
    }

  /* The newborn child tar is here!  */

  program_name = _("tar (child)");

  xdup2 (parent_pipe[PWRITE], STDOUT_FILENO);
  xclose (parent_pipe[PREAD]);

  /* Check if we need a grandchild tar.  This happens only if either:
     a) we're reading stdin: to force unblocking;
     b) the file is to be accessed by rmt: compressor doesn't know how;
     c) the file is not a plain file.  */

  if (strcmp (archive_name_array[0], "-") != 0
      && !_remdev (archive_name_array[0])
      && is_regular_file (archive_name_array[0]))
    {
      /* We don't need a grandchild tar.  Open the archive and lauch the
	 uncompressor.  */

      archive = open (archive_name_array[0], O_RDONLY | O_BINARY, MODE_RW);
      if (archive < 0)
	open_fatal (archive_name_array[0]);
      xdup2 (archive, STDIN_FILENO);
      execlp (use_compress_program_option, use_compress_program_option,
	      "-d", (char *) 0);
      exec_fatal (use_compress_program_option);
    }

  /* We do need a grandchild tar.  */

  xpipe (child_pipe);
  grandchild_pid = xfork ();

  if (grandchild_pid == 0)
    {
      /* The newborn grandchild tar is here!  Launch the uncompressor.  */

      program_name = _("tar (grandchild)");

      xdup2 (child_pipe[PREAD], STDIN_FILENO);
      xclose (child_pipe[PWRITE]);
      execlp (use_compress_program_option, use_compress_program_option,
	      "-d", (char *) 0);
      exec_fatal (use_compress_program_option);
    }

  /* The child tar is still here!  */
  signal (SIGTERM, sig_propagate);

  /* Prepare for unblocking the data from the archive into the
     uncompressor.  */

  xdup2 (child_pipe[PWRITE], STDOUT_FILENO);
  xclose (child_pipe[PREAD]);

  if (strcmp (archive_name_array[0], "-") == 0)
    archive = STDIN_FILENO;
  else
    archive = rmtopen (archive_name_array[0], O_RDONLY | O_BINARY,
		       MODE_RW, rsh_command_option);
  if (archive < 0)
    open_fatal (archive_name_array[0]);

  /* Let's read the archive and pipe it into stdout.  */

  while (1)
    {
      char *cursor;
      size_t maximum;
      size_t count;
      ssize_t status;

      read_error_count = 0;

    error_loop:
      status = rmtread (archive, record_start->buffer, record_size);
      if (status < 0)
	{
	  archive_read_error ();
	  goto error_loop;
	}
      if (status == 0)
	break;
      cursor = record_start->buffer;
      maximum = status;
      while (maximum)
	{
	  count = maximum < BLOCKSIZE ? maximum : BLOCKSIZE;
	  if (full_write (STDOUT_FILENO, cursor, count) != count)
	    write_error (use_compress_program_option);
	  cursor += count;
	  maximum -= count;
	}
    }

  xclose (STDOUT_FILENO);
#if 0
  close_archive ();
#endif

  /* Propagate any failure of the grandchild back to the parent.  */

  while (waitpid (grandchild_pid, &wait_status, 0) == -1)
    if (errno != EINTR)
      {
	waitpid_error (use_compress_program_option);
	break;
      }

  if (WIFSIGNALED (wait_status))
    {
      kill (child_pid, WTERMSIG (wait_status));
      exit_status = TAREXIT_FAILURE;
    }
  else if (WEXITSTATUS (wait_status) != 0)
    exit_status = WEXITSTATUS (wait_status);

  exit (exit_status);
}

#endif /* not MSDOS */

/* Check the LABEL block against the volume label, seen as a globbing
   pattern.  Return true if the pattern matches.  In case of failure,
   retry matching a volume sequence number before giving up in
   multi-volume mode.  */
static int
check_label_pattern (union block *label)
{
  char *string;
  int result;

  if (! memchr (label->header.name, '\0', sizeof label->header.name))
    return 0;

  if (fnmatch (volume_label_option, label->header.name, 0) == 0)
    return 1;

  if (!multi_volume_option)
    return 0;

  string = xmalloc (strlen (volume_label_option)
		    + sizeof VOLUME_LABEL_APPEND + 1);
  strcpy (string, volume_label_option);
  strcat (string, VOLUME_LABEL_APPEND);
  result = fnmatch (string, label->header.name, 0) == 0;
  free (string);
  return result;
}

/* Open an archive file.  The argument specifies whether we are
   reading or writing, or both.  */
void
open_archive (enum access_mode wanted_access)
{
  int backed_up_flag = 0;

  stdlis = to_stdout_option ? stderr : stdout;

  if (record_size == 0)
    FATAL_ERROR ((0, 0, _("Invalid value for record_size")));

  if (archive_names == 0)
    FATAL_ERROR ((0, 0, _("No archive name given")));

  current_file_name = 0;
  current_link_name = 0;
  save_name = 0;
  real_s_name = 0;

  if (multi_volume_option)
    {
      if (verify_option)
	FATAL_ERROR ((0, 0, _("Cannot verify multi-volume archives")));
      record_start = valloc (record_size + (2 * BLOCKSIZE));
      if (record_start)
	record_start += 2;
    }
  else
    record_start = valloc (record_size);
  if (!record_start)
    FATAL_ERROR ((0, 0, _("Cannot allocate memory for blocking factor %d"),
		  blocking_factor));

  current_block = record_start;
  record_end = record_start + blocking_factor;
  /* When updating the archive, we start with reading.  */
  access_mode = wanted_access == ACCESS_UPDATE ? ACCESS_READ : wanted_access;

  if (use_compress_program_option)
    {
      if (multi_volume_option)
	FATAL_ERROR ((0, 0, _("Cannot use multi-volume compressed archives")));
      if (verify_option)
	FATAL_ERROR ((0, 0, _("Cannot verify compressed archives")));

      switch (wanted_access)
	{
	case ACCESS_READ:
	  child_open_for_uncompress ();
	  break;

	case ACCESS_WRITE:
	  child_open_for_compress ();
	  break;

	case ACCESS_UPDATE:
	  FATAL_ERROR ((0, 0, _("Cannot update compressed archives")));
	  break;
	}

      if (wanted_access == ACCESS_WRITE
	  && strcmp (archive_name_array[0], "-") == 0)
	stdlis = stderr;
    }
  else if (strcmp (archive_name_array[0], "-") == 0)
    {
      read_full_records_option = 1; /* could be a pipe, be safe */
      if (verify_option)
	FATAL_ERROR ((0, 0, _("Cannot verify stdin/stdout archive")));

      switch (wanted_access)
	{
	case ACCESS_READ:
	  archive = STDIN_FILENO;
	  break;

	case ACCESS_WRITE:
	  archive = STDOUT_FILENO;
	  stdlis = stderr;
	  break;

	case ACCESS_UPDATE:
	  archive = STDIN_FILENO;
	  stdlis = stderr;
	  write_archive_to_stdout = 1;
	  break;
	}
    }
  else if (verify_option)
    archive = rmtopen (archive_name_array[0], O_RDWR | O_CREAT | O_BINARY,
		       MODE_RW, rsh_command_option);
  else
    switch (wanted_access)
      {
      case ACCESS_READ:
	archive = rmtopen (archive_name_array[0], O_RDONLY | O_BINARY,
			   MODE_RW, rsh_command_option);
	break;

      case ACCESS_WRITE:
	if (backup_option)
	  {
	    maybe_backup_file (archive_name_array[0], 1);
	    backed_up_flag = 1;
	  }
	archive = rmtcreat (archive_name_array[0], MODE_RW,
			    rsh_command_option);
	break;

      case ACCESS_UPDATE:
	archive = rmtopen (archive_name_array[0], O_RDWR | O_CREAT | O_BINARY,
			   MODE_RW, rsh_command_option);
	break;
      }

  if (archive < 0
      || (! _isrmt (archive) && fstat (archive, &archive_stat) < 0))
    {
      int saved_errno = errno;

      if (backed_up_flag)
	undo_last_backup ();
      errno = saved_errno;
      open_fatal (archive_name_array[0]);
    }

#if !MSDOS

  /* Detect if outputting to "/dev/null".  */
  {
    static char const dev_null[] = "/dev/null";
    struct stat dev_null_stat;

    dev_null_output =
      (strcmp (archive_name_array[0], dev_null) == 0
       || (! _isrmt (archive)
	   && S_ISCHR (archive_stat.st_mode)
	   && stat (dev_null, &dev_null_stat) == 0
	   && archive_stat.st_dev == dev_null_stat.st_dev
	   && archive_stat.st_ino == dev_null_stat.st_ino));
  }

  if (!_isrmt (archive) && S_ISREG (archive_stat.st_mode))
    {
      ar_dev = archive_stat.st_dev;
      ar_ino = archive_stat.st_ino;
    }
  else
    ar_dev = 0;

#endif /* not MSDOS */

#if MSDOS
  setmode (archive, O_BINARY);
#endif

  switch (wanted_access)
    {
    case ACCESS_UPDATE:
      records_written = 0;
    case ACCESS_READ:
      records_read = 0;
      record_end = record_start; /* set up for 1st record = # 0 */
      find_next_block ();	/* read it in, check for EOF */

      if (volume_label_option)
	{
	  union block *label = find_next_block ();

	  if (!label)
	    FATAL_ERROR ((0, 0, _("Archive not labeled to match %s"),
			  quote (volume_label_option)));
	  if (!check_label_pattern (label))
	    FATAL_ERROR ((0, 0, _("Volume %s does not match %s"),
			  quote_n (0, label->header.name),
			  quote_n (1, volume_label_option)));
	}
      break;

    case ACCESS_WRITE:
      records_written = 0;
      if (volume_label_option)
	{
	  memset (record_start, 0, BLOCKSIZE);
	  if (multi_volume_option)
	    sprintf (record_start->header.name, "%s Volume 1",
		     volume_label_option);
	  else
	    strcpy (record_start->header.name, volume_label_option);

	  assign_string (&current_file_name, record_start->header.name);

	  record_start->header.typeflag = GNUTYPE_VOLHDR;
	  TIME_TO_CHARS (start_time, record_start->header.mtime);
	  finish_header (record_start);
#if 0
	  current_block++;
#endif
	}
      break;
    }
}

/* Perform a write to flush the buffer.  */
void
flush_write (void)
{
  int copy_back;
  ssize_t status;

  if (checkpoint_option && !(++checkpoint % 10))
    WARN ((0, 0, _("Write checkpoint %d"), checkpoint));

  if (tape_length_option && tape_length_option <= bytes_written)
    {
      errno = ENOSPC;
      status = 0;
    }
  else if (dev_null_output)
    status = record_size;
  else
    status = write_archive_buffer ();
  if (status != record_size && !multi_volume_option)
    archive_write_error (status);

  if (status > 0)
    {
      records_written++;
      bytes_written += status;
    }

  if (status == record_size)
    {
      if (multi_volume_option)
	{
	  char *cursor;

	  if (!save_name)
	    {
	      assign_string (&real_s_name, 0);
	      real_s_totsize = 0;
	      real_s_sizeleft = 0;
	      return;
	    }

	  cursor = save_name + FILESYSTEM_PREFIX_LEN (save_name);
	  while (ISSLASH (*cursor))
	    cursor++;

	  assign_string (&real_s_name, cursor);
	  real_s_totsize = save_totsize;
	  real_s_sizeleft = save_sizeleft;
	}
      return;
    }

  /* We're multivol.  Panic if we didn't get the right kind of response.  */

  /* ENXIO is for the UNIX PC.  */
  if (status < 0 && errno != ENOSPC && errno != EIO && errno != ENXIO)
    archive_write_error (status);

  /* If error indicates a short write, we just move to the next tape.  */

  if (!new_volume (ACCESS_WRITE))
    return;

  if (totals_option)
    prev_written += bytes_written;
  bytes_written = 0;

  if (volume_label_option && real_s_name)
    {
      copy_back = 2;
      record_start -= 2;
    }
  else if (volume_label_option || real_s_name)
    {
      copy_back = 1;
      record_start--;
    }
  else
    copy_back = 0;

  if (volume_label_option)
    {
      memset (record_start, 0, BLOCKSIZE);
      sprintf (record_start->header.name, "%s Volume %d",
	       volume_label_option, volno);
      TIME_TO_CHARS (start_time, record_start->header.mtime);
      record_start->header.typeflag = GNUTYPE_VOLHDR;
      finish_header (record_start);
    }

  if (real_s_name)
    {
      int tmp;

      if (volume_label_option)
	record_start++;

      memset (record_start, 0, BLOCKSIZE);

      /* FIXME: Michael P Urban writes: [a long name file] is being written
	 when a new volume rolls around [...]  Looks like the wrong value is
	 being preserved in real_s_name, though.  */

      strcpy (record_start->header.name, real_s_name);
      record_start->header.typeflag = GNUTYPE_MULTIVOL;
      OFF_TO_CHARS (real_s_sizeleft, record_start->header.size);
      OFF_TO_CHARS (real_s_totsize - real_s_sizeleft,
		    record_start->oldgnu_header.offset);
      tmp = verbose_option;
      verbose_option = 0;
      finish_header (record_start);
      verbose_option = tmp;

      if (volume_label_option)
	record_start--;
    }

  status = write_archive_buffer ();
  if (status != record_size)
    archive_write_error (status);

  bytes_written += status;

  if (copy_back)
    {
      record_start += copy_back;
      memcpy (current_block,
	      record_start + blocking_factor - copy_back,
	      copy_back * BLOCKSIZE);
      current_block += copy_back;

      if (real_s_sizeleft >= copy_back * BLOCKSIZE)
	real_s_sizeleft -= copy_back * BLOCKSIZE;
      else if ((real_s_sizeleft + BLOCKSIZE - 1) / BLOCKSIZE <= copy_back)
	assign_string (&real_s_name, 0);
      else
	{
	  char *cursor = save_name + FILESYSTEM_PREFIX_LEN (save_name);

	  while (ISSLASH (*cursor))
	    cursor++;

	  assign_string (&real_s_name, cursor);
	  real_s_sizeleft = save_sizeleft;
	  real_s_totsize = save_totsize;
	}
      copy_back = 0;
    }
}

/* Handle write errors on the archive.  Write errors are always fatal.
   Hitting the end of a volume does not cause a write error unless the
   write was the first record of the volume.  */
static void
archive_write_error (ssize_t status)
{
  /* It might be useful to know how much was written before the error
     occurred.  */
  if (totals_option)
    {
      int e = errno;
      print_total_written ();
      errno = e;
    }

  write_fatal_details (*archive_name_cursor, status, record_size);
}

/* Handle read errors on the archive.  If the read should be retried,
   return to the caller.  */
static void
archive_read_error (void)
{
  read_error (*archive_name_cursor);

  if (record_start_block == 0)
    FATAL_ERROR ((0, 0, _("At beginning of tape, quitting now")));

  /* Read error in mid archive.  We retry up to READ_ERROR_MAX times and
     then give up on reading the archive.  */

  if (read_error_count++ > READ_ERROR_MAX)
    FATAL_ERROR ((0, 0, _("Too many errors, quitting")));
  return;
}

/* Perform a read to flush the buffer.  */
void
flush_read (void)
{
  ssize_t status;		/* result from system call */
  size_t left;			/* bytes left */
  char *more;			/* pointer to next byte to read */

  if (checkpoint_option && !(++checkpoint % 10))
    WARN ((0, 0, _("Read checkpoint %d"), checkpoint));

  /* Clear the count of errors.  This only applies to a single call to
     flush_read.  */

  read_error_count = 0;		/* clear error count */

  if (write_archive_to_stdout && record_start_block != 0)
    {
      archive = STDOUT_FILENO;
      status = write_archive_buffer ();
      archive = STDIN_FILENO;
      if (status != record_size)
	archive_write_error (status);
    }
  if (multi_volume_option)
    {
      if (save_name)
	{
	  char *cursor = save_name + FILESYSTEM_PREFIX_LEN (save_name);

	  while (ISSLASH (*cursor))
	    cursor++;

	  assign_string (&real_s_name, cursor);
	  real_s_sizeleft = save_sizeleft;
	  real_s_totsize = save_totsize;
	}
      else
	{
	  assign_string (&real_s_name, 0);
	  real_s_totsize = 0;
	  real_s_sizeleft = 0;
	}
    }

 error_loop:
  status = rmtread (archive, record_start->buffer, record_size);
  if (status == record_size)
    {
      records_read++;
      return;
    }

  if ((status == 0
       || (status < 0 && errno == ENOSPC)
       || (status > 0 && !read_full_records_option))
      && multi_volume_option)
    {
      union block *cursor;

    try_volume:
      switch (subcommand_option)
	{
	case APPEND_SUBCOMMAND:
	case CAT_SUBCOMMAND:
	case UPDATE_SUBCOMMAND:
	  if (!new_volume (ACCESS_UPDATE))
	    return;
	  break;

	default:
	  if (!new_volume (ACCESS_READ))
	    return;
	  break;
	}

    vol_error:
      status = rmtread (archive, record_start->buffer, record_size);
      if (status < 0)
	{
	  archive_read_error ();
	  goto vol_error;
	}
      if (status != record_size)
	goto short_read;

      cursor = record_start;

      if (cursor->header.typeflag == GNUTYPE_VOLHDR)
	{
	  if (volume_label_option)
	    {
	      if (!check_label_pattern (cursor))
		{
		  WARN ((0, 0, _("Volume %s does not match %s"),
			 quote_n (0, cursor->header.name),
			 quote_n (1, volume_label_option)));
		  volno--;
		  global_volno--;
		  goto try_volume;
		}
	    }
	  if (verbose_option)
	    fprintf (stdlis, _("Reading %s\n"), quote (cursor->header.name));
	  cursor++;
	}
      else if (volume_label_option)
	WARN ((0, 0, _("WARNING: No volume header")));

      if (real_s_name)
	{
	  uintmax_t s1, s2;
	  if (cursor->header.typeflag != GNUTYPE_MULTIVOL
	      || strcmp (cursor->header.name, real_s_name))
	    {
	      WARN ((0, 0, _("%s is not continued on this volume"),
		     quote (real_s_name)));
	      volno--;
	      global_volno--;
	      goto try_volume;
	    }
	  s1 = UINTMAX_FROM_HEADER (cursor->header.size);
	  s2 = UINTMAX_FROM_HEADER (cursor->oldgnu_header.offset);
	  if (real_s_totsize != s1 + s2 || s1 + s2 < s2)
	    {
	      char totsizebuf[UINTMAX_STRSIZE_BOUND];
	      char s1buf[UINTMAX_STRSIZE_BOUND];
	      char s2buf[UINTMAX_STRSIZE_BOUND];

	      WARN ((0, 0, _("%s is the wrong size (%s != %s + %s)"),
		     quote (cursor->header.name),
		     STRINGIFY_BIGINT (save_totsize, totsizebuf),
		     STRINGIFY_BIGINT (s1, s1buf),
		     STRINGIFY_BIGINT (s2, s2buf)));
	      volno--;
	      global_volno--;
	      goto try_volume;
	    }
	  if (real_s_totsize - real_s_sizeleft
	      != OFF_FROM_HEADER (cursor->oldgnu_header.offset))
	    {
	      WARN ((0, 0, _("This volume is out of sequence")));
	      volno--;
	      global_volno--;
	      goto try_volume;
	    }
	  cursor++;
	}
      current_block = cursor;
      records_read++;
      return;
    }
  else if (status < 0)
    {
      archive_read_error ();
      goto error_loop;		/* try again */
    }

 short_read:
  more = record_start->buffer + status;
  left = record_size - status;

  while (left % BLOCKSIZE != 0
	 || (left && status && read_full_records_option))
    {
      if (status)
	while ((status = rmtread (archive, more, left)) < 0)
	  archive_read_error ();

      if (status == 0)
	break;

      if (! read_full_records_option)
	FATAL_ERROR ((0, 0, _("Unaligned block (%lu bytes) in archive"),
		      (unsigned long) (record_size - left)));

      /* User warned us about this.  Fix up.  */

      left -= status;
      more += status;
    }

  /* FIXME: for size=0, multi-volume support.  On the first record, warn
     about the problem.  */

  if (!read_full_records_option && verbose_option
      && record_start_block == 0 && status > 0)
    WARN ((0, 0, _("Record size = %lu blocks"),
	   (unsigned long) ((record_size - left) / BLOCKSIZE)));

  record_end = record_start + (record_size - left) / BLOCKSIZE;
  records_read++;
}

/*  Flush the current buffer to/from the archive.  */
void
flush_archive (void)
{
  record_start_block += record_end - record_start;
  current_block = record_start;
  record_end = record_start + blocking_factor;

  if (access_mode == ACCESS_READ && time_to_start_writing)
    {
      access_mode = ACCESS_WRITE;
      time_to_start_writing = 0;

      if (file_to_switch_to >= 0)
	{
	  if (rmtclose (archive) != 0)
	    close_warn (*archive_name_cursor);

	  archive = file_to_switch_to;
	}
      else
	backspace_output ();
    }

  switch (access_mode)
    {
    case ACCESS_READ:
      flush_read ();
      break;

    case ACCESS_WRITE:
      flush_write ();
      break;

    case ACCESS_UPDATE:
      abort ();
    }
}

/* Backspace the archive descriptor by one record worth.  If it's a
   tape, MTIOCTOP will work.  If it's something else, try to seek on
   it.  If we can't seek, we lose!  */
static void
backspace_output (void)
{
#ifdef MTIOCTOP
  {
    struct mtop operation;

    operation.mt_op = MTBSR;
    operation.mt_count = 1;
    if (rmtioctl (archive, MTIOCTOP, (char *) &operation) >= 0)
      return;
    if (errno == EIO && rmtioctl (archive, MTIOCTOP, (char *) &operation) >= 0)
      return;
  }
#endif

  {
    off_t position = rmtlseek (archive, (off_t) 0, SEEK_CUR);

    /* Seek back to the beginning of this record and start writing there.  */

    position -= record_size;
    if (position < 0)
      position = 0;
    if (rmtlseek (archive, position, SEEK_SET) != position)
      {
	/* Lseek failed.  Try a different method.  */

	WARN ((0, 0,
	       _("Cannot backspace archive file; it may be unreadable without -i")));

	/* Replace the first part of the record with NULs.  */

	if (record_start->buffer != output_start)
	  memset (record_start->buffer, 0,
		  output_start - record_start->buffer);
      }
  }
}

/* Close the archive file.  */
void
close_archive (void)
{
  if (time_to_start_writing || access_mode == ACCESS_WRITE)
    flush_archive ();

#if !MSDOS

  /* Manage to fully drain a pipe we might be reading, so to not break it on
     the producer after the EOF block.  FIXME: one of these days, GNU tar
     might become clever enough to just stop working, once there is no more
     work to do, we might have to revise this area in such time.  */

  if (fast_read_option && namelist_freed && child_pid > 0)
    kill(child_pid, SIGTERM);

  if (access_mode == ACCESS_READ
      && ! _isrmt (archive)
      && (S_ISFIFO (archive_stat.st_mode) || S_ISSOCK (archive_stat.st_mode)))
    while (rmtread (archive, record_start->buffer, record_size) > 0)
      continue;
#endif

  if (verify_option)
    verify_volume ();

  if (rmtclose (archive) != 0)
    close_warn (*archive_name_cursor);

#if !MSDOS

  if (child_pid)
    {
      int wait_status;

      while (waitpid (child_pid, &wait_status, 0) == -1)
	if (errno != EINTR)
	  {
	    waitpid_error (use_compress_program_option);
	    break;
	  }

      if (!fast_read_option || !namelist_freed)
        if (WIFSIGNALED (wait_status))
	  ERROR ((0, 0, _("Child died with signal %d"),
		  WTERMSIG (wait_status)));
        else if (WEXITSTATUS (wait_status) != 0)
	  ERROR ((0, 0, _("Child returned status %d"),
		  WEXITSTATUS (wait_status)));
    }
#endif /* !MSDOS */

  if (current_file_name)
    free (current_file_name);
  if (current_link_name)
    free (current_link_name);
  if (save_name)
    free (save_name);
  if (real_s_name)
    free (real_s_name);
  free (multi_volume_option ? record_start - 2 : record_start);
}

/* Called to initialize the global volume number.  */
void
init_volume_number (void)
{
  FILE *file = fopen (volno_file_option, "r");

  if (file)
    {
      if (fscanf (file, "%d", &global_volno) != 1
	  || global_volno < 0)
	FATAL_ERROR ((0, 0, _("%s: contains invalid volume number"),
		      quotearg_colon (volno_file_option)));
      if (ferror (file))
	read_error (volno_file_option);
      if (fclose (file) != 0)
	close_error (volno_file_option);
    }
  else if (errno != ENOENT)
    open_error (volno_file_option);
}

/* Called to write out the closing global volume number.  */
void
closeout_volume_number (void)
{
  FILE *file = fopen (volno_file_option, "w");

  if (file)
    {
      fprintf (file, "%d\n", global_volno);
      if (ferror (file))
	write_error (volno_file_option);
      if (fclose (file) != 0)
	close_error (volno_file_option);
    }
  else
    open_error (volno_file_option);
}

/* We've hit the end of the old volume.  Close it and open the next one.
   Return nonzero on success.  */
static int
new_volume (enum access_mode access)
{
  static FILE *read_file;
  static int looped;

  if (!read_file && !info_script_option)
    /* FIXME: if fopen is used, it will never be closed.  */
    read_file = archive == STDIN_FILENO ? fopen (TTY_NAME, "r") : stdin;

  if (now_verifying)
    return 0;
  if (verify_option)
    verify_volume ();

  if (rmtclose (archive) != 0)
    close_warn (*archive_name_cursor);

  global_volno++;
  if (global_volno < 0)
    FATAL_ERROR ((0, 0, _("Volume number overflow")));
  volno++;
  archive_name_cursor++;
  if (archive_name_cursor == archive_name_array + archive_names)
    {
      archive_name_cursor = archive_name_array;
      looped = 1;
    }

 tryagain:
  if (looped)
    {
      /* We have to prompt from now on.  */

      if (info_script_option)
	{
	  if (volno_file_option)
	    closeout_volume_number ();
	  if (system (info_script_option) != 0)
	    FATAL_ERROR ((0, 0, _("`%s' command failed"), info_script_option));
	}
      else
	while (1)
	  {
	    char input_buffer[80];

	    fputc ('\007', stderr);
	    fprintf (stderr,
		     _("Prepare volume #%d for %s and hit return: "),
		     global_volno, quote (*archive_name_cursor));
	    fflush (stderr);

	    if (fgets (input_buffer, sizeof input_buffer, read_file) == 0)
	      {
		WARN ((0, 0, _("EOF where user reply was expected")));

		if (subcommand_option != EXTRACT_SUBCOMMAND
		    && subcommand_option != LIST_SUBCOMMAND
		    && subcommand_option != DIFF_SUBCOMMAND)
		  WARN ((0, 0, _("WARNING: Archive is incomplete")));

		fatal_exit ();
	      }
	    if (input_buffer[0] == '\n'
		|| input_buffer[0] == 'y'
		|| input_buffer[0] == 'Y')
	      break;

	    switch (input_buffer[0])
	      {
	      case '?':
		{
		  fprintf (stderr, _("\
 n [name]   Give a new file name for the next (and subsequent) volume(s)\n\
 q          Abort tar\n\
 !          Spawn a subshell\n\
 ?          Print this list\n"));
		}
		break;

	      case 'q':
		/* Quit.  */

		WARN ((0, 0, _("No new volume; exiting.\n")));

		if (subcommand_option != EXTRACT_SUBCOMMAND
		    && subcommand_option != LIST_SUBCOMMAND
		    && subcommand_option != DIFF_SUBCOMMAND)
		  WARN ((0, 0, _("WARNING: Archive is incomplete")));

		fatal_exit ();

	      case 'n':
		/* Get new file name.  */

		{
		  char *name = &input_buffer[1];
		  char *cursor;

		  while (*name == ' ' || *name == '\t')
		    name++;
		  cursor = name;
		  while (*cursor && *cursor != '\n')
		    cursor++;
		  *cursor = '\0';

		  /* FIXME: the following allocation is never reclaimed.  */
		  *archive_name_cursor = xstrdup (name);
		}
		break;

	      case '!':
#if MSDOS
		spawnl (P_WAIT, getenv ("COMSPEC"), "-", 0);
#else /* not MSDOS */
		{
		  pid_t child;
		  const char *shell = getenv ("SHELL");
		  if (! shell)
		    shell = "/bin/sh";
		  child = xfork ();
		  if (child == 0)
		    {
		      execlp (shell, "-sh", "-i", (char *) 0);
		      exec_fatal (shell);
		    }
		  else
		    {
		      int wait_status;
		      while (waitpid (child, &wait_status, 0) == -1)
			if (errno != EINTR)
			  {
			    waitpid_error (shell);
			    break;
			  }
		    }
		}
#endif /* not MSDOS */
		break;
	      }
	  }
    }

  if (verify_option)
    archive = rmtopen (*archive_name_cursor, O_RDWR | O_CREAT, MODE_RW,
		       rsh_command_option);
  else
    switch (access)
      {
      case ACCESS_READ:
	archive = rmtopen (*archive_name_cursor, O_RDONLY, MODE_RW,
			   rsh_command_option);
	break;

      case ACCESS_WRITE:
	if (backup_option)
	  maybe_backup_file (*archive_name_cursor, 1);
	archive = rmtcreat (*archive_name_cursor, MODE_RW,
			    rsh_command_option);
	break;

      case ACCESS_UPDATE:
	archive = rmtopen (*archive_name_cursor, O_RDWR | O_CREAT, MODE_RW,
			   rsh_command_option);
	break;
      }

  if (archive < 0)
    {
      open_warn (*archive_name_cursor);
      if (!verify_option && access == ACCESS_WRITE && backup_option)
	undo_last_backup ();
      goto tryagain;
    }

#if MSDOS
  setmode (archive, O_BINARY);
#endif

  return 1;
}
