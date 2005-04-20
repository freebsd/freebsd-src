/* Diff files from a tar archive.

   Copyright 1988, 1992, 1993, 1994, 1996, 1997, 1999, 2000, 2001 Free
   Software Foundation, Inc.

   Written by John Gilmore, on 1987-04-30.

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

#if HAVE_UTIME_H
# include <utime.h>
#else
struct utimbuf
  {
    long actime;
    long modtime;
  };
#endif

#if HAVE_LINUX_FD_H
# include <linux/fd.h>
#endif

#include <quotearg.h>

#include "common.h"
#include "rmt.h"

/* Spare space for messages, hopefully safe even after gettext.  */
#define MESSAGE_BUFFER_SIZE 100

/* Nonzero if we are verifying at the moment.  */
int now_verifying;

/* File descriptor for the file we are diffing.  */
static int diff_handle;

/* Area for reading file contents into.  */
static char *diff_buffer;

/* Initialize for a diff operation.  */
void
diff_init (void)
{
  diff_buffer = valloc (record_size);
  if (!diff_buffer)
    xalloc_die ();
}

/* Sigh about something that differs by writing a MESSAGE to stdlis,
   given MESSAGE is nonzero.  Also set the exit status if not already.  */
static void
report_difference (const char *message)
{
  if (message)
    fprintf (stdlis, "%s: %s\n", quotearg_colon (current_file_name), message);

  if (exit_status == TAREXIT_SUCCESS)
    exit_status = TAREXIT_DIFFERS;
}

/* Take a buffer returned by read_and_process and do nothing with it.  */
static int
process_noop (size_t size, char *data)
{
  /* Yes, I know.  SIZE and DATA are unused in this function.  Some
     compilers may even report it.  That's OK, just relax!  */
  return 1;
}

static int
process_rawdata (size_t bytes, char *buffer)
{
  ssize_t status = safe_read (diff_handle, diff_buffer, bytes);
  char message[MESSAGE_BUFFER_SIZE];

  if (status != bytes)
    {
      if (status < 0)
	{
	  read_error (current_file_name);
	  report_difference (0);
	}
      else
	{
	  sprintf (message, _("Could only read %lu of %lu bytes"),
		   (unsigned long) status, (unsigned long) bytes);
	  report_difference (message);
	}
      return 0;
    }

  if (memcmp (buffer, diff_buffer, bytes))
    {
      report_difference (_("Contents differ"));
      return 0;
    }

  return 1;
}

/* Directory contents, only for GNUTYPE_DUMPDIR.  */

static char *dumpdir_cursor;

static int
process_dumpdir (size_t bytes, char *buffer)
{
  if (memcmp (buffer, dumpdir_cursor, bytes))
    {
      report_difference (_("Contents differ"));
      return 0;
    }

  dumpdir_cursor += bytes;
  return 1;
}

/* Some other routine wants SIZE bytes in the archive.  For each chunk
   of the archive, call PROCESSOR with the size of the chunk, and the
   address of the chunk it can work with.  The PROCESSOR should return
   nonzero for success.  It it return error once, continue skipping
   without calling PROCESSOR anymore.  */
static void
read_and_process (off_t size, int (*processor) (size_t, char *))
{
  union block *data_block;
  size_t data_size;

  if (multi_volume_option)
    save_sizeleft = size;
  while (size)
    {
      data_block = find_next_block ();
      if (! data_block)
	{
	  ERROR ((0, 0, _("Unexpected EOF in archive")));
	  return;
	}

      data_size = available_space_after (data_block);
      if (data_size > size)
	data_size = size;
      if (!(*processor) (data_size, data_block->buffer))
	processor = process_noop;
      set_next_block_after ((union block *)
			    (data_block->buffer + data_size - 1));
      size -= data_size;
      if (multi_volume_option)
	save_sizeleft -= data_size;
    }
}

/* JK This routine should be used more often than it is ... look into
   that.  Anyhow, what it does is translate the sparse information on the
   header, and in any subsequent extended headers, into an array of
   structures with true numbers, as opposed to character strings.  It
   simply makes our life much easier, doing so many comparisons and such.
   */
static void
fill_in_sparse_array (void)
{
  int counter;

  /* Allocate space for our scratch space; it's initially 10 elements
     long, but can change in this routine if necessary.  */

  sp_array_size = 10;
  sparsearray = xmalloc (sp_array_size * sizeof (struct sp_array));

  /* There are at most five of these structures in the header itself;
     read these in first.  */

  for (counter = 0; counter < SPARSES_IN_OLDGNU_HEADER; counter++)
    {
      /* Compare to 0, or use !(int)..., for Pyramid's dumb compiler.  */
      if (current_header->oldgnu_header.sp[counter].numbytes == 0)
	break;

      sparsearray[counter].offset =
	OFF_FROM_HEADER (current_header->oldgnu_header.sp[counter].offset);
      sparsearray[counter].numbytes =
	SIZE_FROM_HEADER (current_header->oldgnu_header.sp[counter].numbytes);
    }

  /* If the header's extended, we gotta read in exhdr's till we're done.  */

  if (current_header->oldgnu_header.isextended)
    {
      /* How far into the sparsearray we are `so far'.  */
      static int so_far_ind = SPARSES_IN_OLDGNU_HEADER;
      union block *exhdr;

      while (1)
	{
	  exhdr = find_next_block ();
	  if (!exhdr)
	    FATAL_ERROR ((0, 0, _("Unexpected EOF in archive")));

	  for (counter = 0; counter < SPARSES_IN_SPARSE_HEADER; counter++)
	    {
	      if (counter + so_far_ind > sp_array_size - 1)
		{
		  /* We just ran out of room in our scratch area -
		     realloc it.  */

		  sp_array_size *= 2;
		  sparsearray =
		    xrealloc (sparsearray,
			      sp_array_size * sizeof (struct sp_array));
		}

	      /* Convert the character strings into offsets and sizes.  */

	      sparsearray[counter + so_far_ind].offset =
		OFF_FROM_HEADER (exhdr->sparse_header.sp[counter].offset);
	      sparsearray[counter + so_far_ind].numbytes =
		SIZE_FROM_HEADER (exhdr->sparse_header.sp[counter].numbytes);
	    }

	  /* If this is the last extended header for this file, we can
	     stop.  */

	  if (!exhdr->sparse_header.isextended)
	    break;

	  so_far_ind += SPARSES_IN_SPARSE_HEADER;
	  set_next_block_after (exhdr);
	}

      /* Be sure to skip past the last one.  */

      set_next_block_after (exhdr);
    }
}

/* JK Diff'ing a sparse file with its counterpart on the tar file is a
   bit of a different story than a normal file.  First, we must know what
   areas of the file to skip through, i.e., we need to construct a
   sparsearray, which will hold all the information we need.  We must
   compare small amounts of data at a time as we find it.  */

/* FIXME: This does not look very solid to me, at first glance.  Zero areas
   are not checked, spurious sparse entries seemingly goes undetected, and
   I'm not sure overall identical sparsity is verified.  */

static void
diff_sparse_files (off_t size_of_file)
{
  off_t remaining_size = size_of_file;
  char *buffer = xmalloc (BLOCKSIZE * sizeof (char));
  size_t buffer_size = BLOCKSIZE;
  union block *data_block = 0;
  int counter = 0;
  int different = 0;

  fill_in_sparse_array ();

  while (remaining_size > 0)
    {
      ssize_t status;
      size_t chunk_size;
      off_t offset;

#if 0
      off_t amount_read = 0;
#endif

      data_block = find_next_block ();
      if (!data_block)
	FATAL_ERROR ((0, 0, _("Unexpected EOF in archive")));
      chunk_size = sparsearray[counter].numbytes;
      if (!chunk_size)
	break;

      offset = sparsearray[counter].offset;
      if (lseek (diff_handle, offset, SEEK_SET) < 0)
	{
	  seek_error_details (current_file_name, offset);
	  report_difference (0);
	}

      /* Take care to not run out of room in our buffer.  */

      while (buffer_size < chunk_size)
	{
	  if (buffer_size * 2 < buffer_size)
	    xalloc_die ();
	  buffer_size *= 2;
	  buffer = xrealloc (buffer, buffer_size * sizeof (char));
	}

      while (chunk_size > BLOCKSIZE)
	{
	  if (status = safe_read (diff_handle, buffer, BLOCKSIZE),
	      status != BLOCKSIZE)
	    {
	      if (status < 0)
		{
		  read_error (current_file_name);
		  report_difference (0);
		}
	      else
		{
		  char message[MESSAGE_BUFFER_SIZE];

		  sprintf (message, _("Could only read %lu of %lu bytes"),
			   (unsigned long) status, (unsigned long) chunk_size);
		  report_difference (message);
		}
	      break;
	    }

	  if (memcmp (buffer, data_block->buffer, BLOCKSIZE))
	    {
	      different = 1;
	      break;
	    }

	  chunk_size -= status;
	  remaining_size -= status;
	  set_next_block_after (data_block);
	  data_block = find_next_block ();
	  if (!data_block)
	    FATAL_ERROR ((0, 0, _("Unexpected EOF in archive")));
	}
      if (status = safe_read (diff_handle, buffer, chunk_size),
	  status != chunk_size)
	{
	  if (status < 0)
	    {
	      read_error (current_file_name);
	      report_difference (0);
	    }
	  else
	    {
	      char message[MESSAGE_BUFFER_SIZE];

	      sprintf (message, _("Could only read %lu of %lu bytes"),
		       (unsigned long) status, (unsigned long) chunk_size);
	      report_difference (message);
	    }
	  break;
	}

      if (memcmp (buffer, data_block->buffer, chunk_size))
	{
	  different = 1;
	  break;
	}
#if 0
      amount_read += chunk_size;
      if (amount_read >= BLOCKSIZE)
	{
	  amount_read = 0;
	  set_next_block_after (data_block);
	  data_block = find_next_block ();
	  if (!data_block)
	    FATAL_ERROR ((0, 0, _("Unexpected EOF in archive")));
	}
#endif
      set_next_block_after (data_block);
      counter++;
      remaining_size -= chunk_size;
    }

#if 0
  /* If the number of bytes read isn't the number of bytes supposedly in
     the file, they're different.  */

  if (amount_read != size_of_file)
    different = 1;
#endif

  set_next_block_after (data_block);
  free (sparsearray);

  if (different)
    report_difference (_("Contents differ"));
}

/* Call either stat or lstat over STAT_DATA, depending on
   --dereference (-h), for a file which should exist.  Diagnose any
   problem.  Return nonzero for success, zero otherwise.  */
static int
get_stat_data (char const *file_name, struct stat *stat_data)
{
  int status = deref_stat (dereference_option, file_name, stat_data);

  if (status != 0)
    {
      if (errno == ENOENT)
	{
	  report_difference (_("does not exist"));
	}
      else
	{
	  stat_error (file_name);
          report_difference (0);
	}
      return 0;
    }

  return 1;
}

/* Diff a file against the archive.  */
void
diff_archive (void)
{
  struct stat stat_data;
  size_t name_length;
  int status;
  struct utimbuf restore_times;

  set_next_block_after (current_header);
  decode_header (current_header, &current_stat, &current_format, 1);

  /* Print the block from current_header and current_stat.  */

  if (verbose_option)
    {
      if (now_verifying)
	fprintf (stdlis, _("Verify "));
      print_header ();
    }

  switch (current_header->header.typeflag)
    {
    default:
      ERROR ((0, 0, _("%s: Unknown file type '%c', diffed as normal file"),
	      quotearg_colon (current_file_name),
	      current_header->header.typeflag));
      /* Fall through.  */

    case AREGTYPE:
    case REGTYPE:
    case GNUTYPE_SPARSE:
    case CONTTYPE:

      /* Appears to be a file.  See if it's really a directory.  */

      name_length = strlen (current_file_name) - 1;
      if (ISSLASH (current_file_name[name_length]))
	goto really_dir;

      if (!get_stat_data (current_file_name, &stat_data))
	{
	  skip_member ();
	  goto quit;
	}

      if (!S_ISREG (stat_data.st_mode))
	{
	  report_difference (_("File type differs"));
	  skip_member ();
	  goto quit;
	}

      if ((current_stat.st_mode & MODE_ALL) != (stat_data.st_mode & MODE_ALL))
	report_difference (_("Mode differs"));

#if !MSDOS
      /* stat() in djgpp's C library gives a constant number of 42 as the
	 uid and gid of a file.  So, comparing an FTP'ed archive just after
	 unpack would fail on MSDOS.  */
      if (stat_data.st_uid != current_stat.st_uid)
	report_difference (_("Uid differs"));
      if (stat_data.st_gid != current_stat.st_gid)
	report_difference (_("Gid differs"));
#endif

      if (stat_data.st_mtime != current_stat.st_mtime)
	report_difference (_("Mod time differs"));
      if (current_header->header.typeflag != GNUTYPE_SPARSE &&
	  stat_data.st_size != current_stat.st_size)
	{
	  report_difference (_("Size differs"));
	  skip_member ();
	  goto quit;
	}

      diff_handle = open (current_file_name, O_RDONLY | O_BINARY);

      if (diff_handle < 0)
	{
	  open_error (current_file_name);
	  skip_member ();
	  report_difference (0);
	  goto quit;
	}

      restore_times.actime = stat_data.st_atime;
      restore_times.modtime = stat_data.st_mtime;

      /* Need to treat sparse files completely differently here.  */

      if (current_header->header.typeflag == GNUTYPE_SPARSE)
	diff_sparse_files (current_stat.st_size);
      else
	{
	  if (multi_volume_option)
	    {
	      assign_string (&save_name, current_file_name);
	      save_totsize = current_stat.st_size;
	      /* save_sizeleft is set in read_and_process.  */
	    }

	  read_and_process (current_stat.st_size, process_rawdata);

	  if (multi_volume_option)
	    assign_string (&save_name, 0);
	}

      status = close (diff_handle);
      if (status != 0)
	close_error (current_file_name);

      if (atime_preserve_option)
	utime (current_file_name, &restore_times);

    quit:
      break;

#if !MSDOS
    case LNKTYPE:
      {
	struct stat link_data;

	if (!get_stat_data (current_file_name, &stat_data))
	  break;
	if (!get_stat_data (current_link_name, &link_data))
	  break;

	if (stat_data.st_dev != link_data.st_dev
	    || stat_data.st_ino != link_data.st_ino)
	  {
	    char *message =
	      xmalloc (MESSAGE_BUFFER_SIZE + 4 * strlen (current_link_name));

	    sprintf (message, _("Not linked to %s"),
		     quote (current_link_name));
	    report_difference (message);
	    free (message);
	    break;
	  }

	break;
      }
#endif /* not MSDOS */

#ifdef HAVE_READLINK
    case SYMTYPE:
      {
	size_t len = strlen (current_link_name);
	char *linkbuf = alloca (len + 1);

	status = readlink (current_file_name, linkbuf, len);

	if (status < 0)
	  {
	    if (errno == ENOENT)
	      readlink_warn (current_file_name);
	    else
	      readlink_error (current_file_name);
	    report_difference (0);
	  }
	else if (status != len
		 || strncmp (current_link_name, linkbuf, len) != 0)
	  report_difference (_("Symlink differs"));

	break;
      }
#endif

    case CHRTYPE:
    case BLKTYPE:
    case FIFOTYPE:

      /* FIXME: deal with umask.  */

      if (!get_stat_data (current_file_name, &stat_data))
	break;

      if (current_header->header.typeflag == CHRTYPE
	  ? !S_ISCHR (stat_data.st_mode)
	  : current_header->header.typeflag == BLKTYPE
	  ? !S_ISBLK (stat_data.st_mode)
	  : /* current_header->header.typeflag == FIFOTYPE */
	  !S_ISFIFO (stat_data.st_mode))
	{
	  report_difference (_("File type differs"));
	  break;
	}

      if ((current_header->header.typeflag == CHRTYPE
	   || current_header->header.typeflag == BLKTYPE)
	  && current_stat.st_rdev != stat_data.st_rdev)
	{
	  report_difference (_("Device number differs"));
	  break;
	}

      if ((current_stat.st_mode & MODE_ALL) != (stat_data.st_mode & MODE_ALL))
	{
	  report_difference (_("Mode differs"));
	  break;
	}

      break;

    case GNUTYPE_DUMPDIR:
      {
	char *dumpdir_buffer = get_directory_contents (current_file_name, 0);

	if (multi_volume_option)
	  {
	    assign_string (&save_name, current_file_name);
	    save_totsize = current_stat.st_size;
	    /* save_sizeleft is set in read_and_process.  */
	  }

	if (dumpdir_buffer)
	  {
	    dumpdir_cursor = dumpdir_buffer;
	    read_and_process (current_stat.st_size, process_dumpdir);
	    free (dumpdir_buffer);
	  }
	else
	  read_and_process (current_stat.st_size, process_noop);

	if (multi_volume_option)
	  assign_string (&save_name, 0);
	/* Fall through.  */
      }

    case DIRTYPE:
      /* Check for trailing /.  */

      name_length = strlen (current_file_name) - 1;

    really_dir:
      while (name_length && ISSLASH (current_file_name[name_length]))
	current_file_name[name_length--] = '\0';	/* zap / */

      if (!get_stat_data (current_file_name, &stat_data))
	break;

      if (!S_ISDIR (stat_data.st_mode))
	{
	  report_difference (_("File type differs"));
	  break;
	}

      if ((current_stat.st_mode & MODE_ALL) != (stat_data.st_mode & MODE_ALL))
	{
	  report_difference (_("Mode differs"));
	  break;
	}

      break;

    case GNUTYPE_VOLHDR:
      break;

    case GNUTYPE_MULTIVOL:
      {
	off_t offset;

	name_length = strlen (current_file_name) - 1;
	if (ISSLASH (current_file_name[name_length]))
	  goto really_dir;

	if (!get_stat_data (current_file_name, &stat_data))
	  break;

	if (!S_ISREG (stat_data.st_mode))
	  {
	    report_difference (_("File type differs"));
	    skip_member ();
	    break;
	  }

	offset = OFF_FROM_HEADER (current_header->oldgnu_header.offset);
	if (stat_data.st_size != current_stat.st_size + offset)
	  {
	    report_difference (_("Size differs"));
	    skip_member ();
	    break;
	  }

	diff_handle = open (current_file_name, O_RDONLY | O_BINARY);

	if (diff_handle < 0)
	  {
	    open_error (current_file_name);
	    report_difference (0);
	    skip_member ();
	    break;
	  }

	if (lseek (diff_handle, offset, SEEK_SET) < 0)
	  {
	    seek_error_details (current_file_name, offset);
	    report_difference (0);
	    break;
	  }

	if (multi_volume_option)
	  {
	    assign_string (&save_name, current_file_name);
	    save_totsize = stat_data.st_size;
	    /* save_sizeleft is set in read_and_process.  */
	  }

	read_and_process (current_stat.st_size, process_rawdata);

	if (multi_volume_option)
	  assign_string (&save_name, 0);

	status = close (diff_handle);
	if (status != 0)
	  close_error (current_file_name);

	break;
      }
    }
}

void
verify_volume (void)
{
  if (!diff_buffer)
    diff_init ();

  /* Verifying an archive is meant to check if the physical media got it
     correctly, so try to defeat clever in-memory buffering pertaining to
     this particular media.  On Linux, for example, the floppy drive would
     not even be accessed for the whole verification.

     The code was using fsync only when the ioctl is unavailable, but
     Marty Leisner says that the ioctl does not work when not preceded by
     fsync.  So, until we know better, or maybe to please Marty, let's do it
     the unbelievable way :-).  */

#if HAVE_FSYNC
  fsync (archive);
#endif
#ifdef FDFLUSH
  ioctl (archive, FDFLUSH);
#endif

#ifdef MTIOCTOP
  {
    struct mtop operation;
    int status;

    operation.mt_op = MTBSF;
    operation.mt_count = 1;
    if (status = rmtioctl (archive, MTIOCTOP, (char *) &operation), status < 0)
      {
	if (errno != EIO
	    || (status = rmtioctl (archive, MTIOCTOP, (char *) &operation),
		status < 0))
	  {
#endif
	    if (rmtlseek (archive, (off_t) 0, SEEK_SET) != 0)
	      {
		/* Lseek failed.  Try a different method.  */
		seek_warn (archive_name_array[0]);
		return;
	      }
#ifdef MTIOCTOP
	  }
      }
  }
#endif

  access_mode = ACCESS_READ;
  now_verifying = 1;

  flush_read ();
  while (1)
    {
      enum read_header status = read_header (0);

      if (status == HEADER_FAILURE)
	{
	  int counter = 0;

	  while (status == HEADER_FAILURE);
	    {
	      counter++;
	      status = read_header (0);
	    }
	  ERROR ((0, 0,
		  _("VERIFY FAILURE: %d invalid header(s) detected"), counter));
	}
      if (status == HEADER_ZERO_BLOCK || status == HEADER_END_OF_FILE)
	break;

      diff_archive ();
    }

  access_mode = ACCESS_WRITE;
  now_verifying = 0;
}
