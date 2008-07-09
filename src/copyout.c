/* copyout.c - create a cpio archive
   Copyright (C) 1990, 1991, 1992, 2001, 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <system.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "filetypes.h"
#include "cpiohdr.h"
#include "dstring.h"
#include "extern.h"
#include "defer.h"
#include <rmt.h>

/* Read FILE_SIZE bytes of FILE_NAME from IN_FILE_DES and
   compute and return a checksum for them.  */

static unsigned long
read_for_checksum (int in_file_des, int file_size, char *file_name)
{
  unsigned long crc;
  char buf[BUFSIZ];
  int bytes_left;
  int bytes_read;
  int i;

  crc = 0;

  for (bytes_left = file_size; bytes_left > 0; bytes_left -= bytes_read)
    {
      bytes_read = read (in_file_des, buf, BUFSIZ);
      if (bytes_read < 0)
	error (1, errno, _("cannot read checksum for %s"), file_name);
      if (bytes_read == 0)
	break;
      if (bytes_left < bytes_read)
        bytes_read = bytes_left;
      for (i = 0; i < bytes_read; ++i)
	crc += buf[i] & 0xff;
    }
  if (lseek (in_file_des, 0L, SEEK_SET))
    error (1, errno, _("cannot read checksum for %s"), file_name);

  return crc;
}

/* Write out NULs to fill out the rest of the current block on
   OUT_FILE_DES.  */

static void
tape_clear_rest_of_block (int out_file_des)
{
  while (output_size < io_block_size)
    {
      if ((io_block_size - output_size) > 512)
	tape_buffered_write (zeros_512, out_file_des, 512);
      else
	tape_buffered_write (zeros_512, out_file_des, io_block_size - output_size);
    }
}

/* Write NULs on OUT_FILE_DES to move from OFFSET (the current location)
   to the end of the header.  */

static void
tape_pad_output (int out_file_des, int offset)
{
  int pad;

  if (archive_format == arf_newascii || archive_format == arf_crcascii)
    pad = (4 - (offset % 4)) % 4;
  else if (archive_format == arf_tar || archive_format == arf_ustar)
    pad = (512 - (offset % 512)) % 512;
  else if (archive_format != arf_oldascii && archive_format != arf_hpoldascii)
    pad = (2 - (offset % 2)) % 2;
  else
    pad = 0;

  if (pad != 0)
    tape_buffered_write (zeros_512, out_file_des, pad);
}


/* When creating newc and crc archives if a file has multiple (hard)
   links, we don't put any of them into the archive until we have seen
   all of them (or until we get to the end of the list of files that
   are going into the archive and know that we have seen all of the links
   to the file that we will see).  We keep these "defered" files on
   this list.   */

struct deferment *deferouts = NULL;

/* Count the number of other (hard) links to this file that have
   already been defered.  */

static int
count_defered_links_to_dev_ino (struct new_cpio_header *file_hdr)
{
  struct deferment *d;
  int	ino;
  int 	maj;
  int   min;
  int 	count;
  ino = file_hdr->c_ino;
  maj = file_hdr->c_dev_maj;
  min = file_hdr->c_dev_min;
  count = 0;
  for (d = deferouts; d != NULL; d = d->next)
    {
      if ( (d->header.c_ino == ino) && (d->header.c_dev_maj == maj)
	  && (d->header.c_dev_min == min) )
	++count;
    }
  return count;
}

/* Is this file_hdr the last (hard) link to a file?  I.e., have
   we already seen and defered all of the other links?  */

static int
last_link (struct new_cpio_header *file_hdr)
{
  int	other_files_sofar;

  other_files_sofar = count_defered_links_to_dev_ino (file_hdr);
  if (file_hdr->c_nlink == (other_files_sofar + 1) )
    {
      return 1;
    }
  return 0;
}


/* Add the file header for a link that is being defered to the deferouts
   list.  */

static void
add_link_defer (struct new_cpio_header *file_hdr)
{
  struct deferment *d;
  d = create_deferment (file_hdr);
  d->next = deferouts;
  deferouts = d;
}

/* We are about to put a file into a newc or crc archive that is
   multiply linked.  We have already seen and defered all of the
   other links to the file but haven't written them into the archive.
   Write the other links into the archive, and remove them from the
   deferouts list.  */

static void
writeout_other_defers (struct new_cpio_header *file_hdr, int out_des)
{
  struct deferment *d;
  struct deferment *d_prev;
  int	ino;
  int 	maj;
  int   min;
  ino = file_hdr->c_ino;
  maj = file_hdr->c_dev_maj;
  min = file_hdr->c_dev_min;
  d_prev = NULL;
  d = deferouts;
  while (d != NULL)
    {
      if ( (d->header.c_ino == ino) && (d->header.c_dev_maj == maj)
	  && (d->header.c_dev_min == min) )
	{
	  struct deferment *d_free;
	  d->header.c_filesize = 0;
	  write_out_header (&d->header, out_des);
	  if (d_prev != NULL)
	    d_prev->next = d->next;
	  else
	    deferouts = d->next;
	  d_free = d;
	  d = d->next;
	  free_deferment (d_free);
	}
      else
	{
	  d_prev = d;
	  d = d->next;
	}
    }
  return;
}

/* Write a file into the archive.  This code is the same as
   the code in process_copy_out(), but we need it here too
   for writeout_final_defers() to call.  */

static void
writeout_defered_file (struct new_cpio_header *header, int out_file_des)
{
  int in_file_des;
  struct new_cpio_header file_hdr;
  struct utimbuf times;		/* For setting file times.  */
  /* Initialize this in case it has members we don't know to set.  */
  bzero (&times, sizeof (struct utimbuf));

  file_hdr = *header;


  in_file_des = open (header->c_name,
		      O_RDONLY | O_BINARY, 0);
  if (in_file_des < 0)
    {
      error (0, errno, "%s", header->c_name);
      return;
    }

  if (archive_format == arf_crcascii)
    file_hdr.c_chksum = read_for_checksum (in_file_des,
					   file_hdr.c_filesize,
					   header->c_name);

  write_out_header (&file_hdr, out_file_des);
  copy_files_disk_to_tape (in_file_des, out_file_des, file_hdr.c_filesize, header->c_name);
  warn_if_file_changed(header->c_name, file_hdr.c_filesize, file_hdr.c_mtime);

  if (archive_format == arf_tar || archive_format == arf_ustar)
    add_inode (file_hdr.c_ino, file_hdr.c_name, file_hdr.c_dev_maj,
	       file_hdr.c_dev_min);

  tape_pad_output (out_file_des, file_hdr.c_filesize);

  if (close (in_file_des) < 0)
    error (0, errno, "%s", header->c_name);
  if (reset_time_flag)
    {
      times.actime = file_hdr.c_mtime;
      times.modtime = file_hdr.c_mtime;
      /* Debian hack: Silently ignore EROFS because reading the file
         won't have upset its timestamp if it's on a read-only
         filesystem.  This has been submitted as a suggestion to
         "bug-gnu-utils@prep.ai.mit.edu".  -BEM */
      if (utime (file_hdr.c_name, &times) < 0
	  && errno != EROFS)
	error (0, errno, "%s", file_hdr.c_name);
    }
  return;
}

/* When writing newc and crc format archives we defer multiply linked
   files until we have seen all of the links to the file.  If a file
   has links to it that aren't going into the archive, then we will
   never see the "last" link to the file, so at the end we just write 
   all of the leftover defered files into the archive.  */

static void
writeout_final_defers (int out_des)
{
  struct deferment *d;
  int other_count;
  while (deferouts != NULL)
    {
      d = deferouts;
      other_count = count_defered_links_to_dev_ino (&d->header);
      if (other_count == 1)
	{
	  writeout_defered_file (&d->header, out_des);
	}
      else
	{
	  struct new_cpio_header file_hdr;
	  file_hdr = d->header;
	  file_hdr.c_filesize = 0;
	  write_out_header (&file_hdr, out_des);
	}
      deferouts = deferouts->next;
    }
}


/* Write out header FILE_HDR, including the file name, to file
   descriptor OUT_DES.  */

void
write_out_header (struct new_cpio_header *file_hdr, int out_des)
{
  if (archive_format == arf_newascii || archive_format == arf_crcascii)
    {
      char ascii_header[112];
      char *magic_string;

      if (archive_format == arf_crcascii)
	magic_string = "070702";
      else
	magic_string = "070701";
      sprintf (ascii_header,
	       "%6s%08lx%08lx%08lx%08lx%08lx%08lx%08lx%08lx%08lx%08lx%08lx%08lx%08lx",
	       magic_string,
	       file_hdr->c_ino, file_hdr->c_mode, file_hdr->c_uid,
	       file_hdr->c_gid, file_hdr->c_nlink, file_hdr->c_mtime,
	     file_hdr->c_filesize, file_hdr->c_dev_maj, file_hdr->c_dev_min,
	   file_hdr->c_rdev_maj, file_hdr->c_rdev_min, file_hdr->c_namesize,
	       file_hdr->c_chksum);
      tape_buffered_write (ascii_header, out_des, 110L);

      /* Write file name to output.  */
      tape_buffered_write (file_hdr->c_name, out_des, (long) file_hdr->c_namesize);
      tape_pad_output (out_des, file_hdr->c_namesize + 110);
    }
  else if (archive_format == arf_oldascii || archive_format == arf_hpoldascii)
    {
      char ascii_header[78];
      dev_t dev;
      dev_t rdev;

      if (archive_format == arf_oldascii)
	{
	  dev = makedev (file_hdr->c_dev_maj, file_hdr->c_dev_min);
	  rdev = makedev (file_hdr->c_rdev_maj, file_hdr->c_rdev_min);
	}
      else
	{
	  /* HP/UX cpio creates archives that look just like ordinary archives,
	     but for devices it sets major = 0, minor = 1, and puts the
	     actual major/minor number in the filesize field.  */
	  switch (file_hdr->c_mode & CP_IFMT)
	    {
	      case CP_IFCHR:
	      case CP_IFBLK:
#ifdef CP_IFSOCK
	      case CP_IFSOCK:
#endif
#ifdef CP_IFIFO
	      case CP_IFIFO:
#endif
		file_hdr->c_filesize = makedev (file_hdr->c_rdev_maj,
						file_hdr->c_rdev_min);
		rdev = 1;
		break;
	      default:
		dev = makedev (file_hdr->c_dev_maj, file_hdr->c_dev_min);
		rdev = makedev (file_hdr->c_rdev_maj, file_hdr->c_rdev_min);
		break;
	    }
	}

      if ((warn_option & CPIO_WARN_TRUNCATE) && (file_hdr->c_ino >> 16) != 0)
	error (0, 0, _("%s: truncating inode number"), file_hdr->c_name);

      /* Debian hack: The type of dev_t has changed in glibc.  Fixed output
         to ensure that a long int is passed to sprintf.  This has been
         reported to "bug-gnu-utils@prep.ai.mit.edu". (1998/5/26) -BEM */
      sprintf (ascii_header,
	       "%06ho%06lo%06lo%06lo%06lo%06lo%06lo%06lo%011lo%06lo%011lo",
	       file_hdr->c_magic & 0xFFFF, (long) dev & 0xFFFF,
	       file_hdr->c_ino & 0xFFFF, file_hdr->c_mode & 0xFFFF,
	       file_hdr->c_uid & 0xFFFF, file_hdr->c_gid & 0xFFFF,
	       file_hdr->c_nlink & 0xFFFF, (long) rdev & 0xFFFF,
	       file_hdr->c_mtime, file_hdr->c_namesize & 0xFFFF,
	       file_hdr->c_filesize);
      tape_buffered_write (ascii_header, out_des, 76L);

      /* Write file name to output.  */
      tape_buffered_write (file_hdr->c_name, out_des, (long) file_hdr->c_namesize);
    }
  else if (archive_format == arf_tar || archive_format == arf_ustar)
    {
      write_out_tar_header (file_hdr, out_des);
    }
  else
    {
      struct old_cpio_header short_hdr;

      short_hdr.c_magic = 070707;
      short_hdr.c_dev = makedev (file_hdr->c_dev_maj, file_hdr->c_dev_min);

      if ((warn_option & CPIO_WARN_TRUNCATE) && (file_hdr->c_ino >> 16) != 0)
	error (0, 0, _("%s: truncating inode number"), file_hdr->c_name);

      short_hdr.c_ino = file_hdr->c_ino & 0xFFFF;
      short_hdr.c_mode = file_hdr->c_mode & 0xFFFF;
      short_hdr.c_uid = file_hdr->c_uid & 0xFFFF;
      short_hdr.c_gid = file_hdr->c_gid & 0xFFFF;
      short_hdr.c_nlink = file_hdr->c_nlink & 0xFFFF;
      if (archive_format != arf_hpbinary)
	short_hdr.c_rdev = makedev (file_hdr->c_rdev_maj, file_hdr->c_rdev_min);
      else
	{
	  switch (file_hdr->c_mode & CP_IFMT)
	    {
	      /* HP/UX cpio creates archives that look just like ordinary 
		 archives, but for devices it sets major = 0, minor = 1, and 
		 puts the actual major/minor number in the filesize field.  */
	      case CP_IFCHR:
	      case CP_IFBLK:
#ifdef CP_IFSOCK
	      case CP_IFSOCK:
#endif
#ifdef CP_IFIFO
	      case CP_IFIFO:
#endif
		file_hdr->c_filesize = makedev (file_hdr->c_rdev_maj,
						file_hdr->c_rdev_min);
		short_hdr.c_rdev = makedev (0, 1);
		break;
	      default:
		short_hdr.c_rdev = makedev (file_hdr->c_rdev_maj, 
					    file_hdr->c_rdev_min);
		break;
	    }
	}
      short_hdr.c_mtimes[0] = file_hdr->c_mtime >> 16;
      short_hdr.c_mtimes[1] = file_hdr->c_mtime & 0xFFFF;

      short_hdr.c_namesize = file_hdr->c_namesize & 0xFFFF;

      short_hdr.c_filesizes[0] = file_hdr->c_filesize >> 16;
      short_hdr.c_filesizes[1] = file_hdr->c_filesize & 0xFFFF;

      /* Output the file header.  */
      tape_buffered_write ((char *) &short_hdr, out_des, 26L);

      /* Write file name to output.  */
      tape_buffered_write (file_hdr->c_name, out_des, (long) file_hdr->c_namesize);

      tape_pad_output (out_des, file_hdr->c_namesize + 26);
    }
}

/* Read a list of file names from the standard input
   and write a cpio collection on the standard output.
   The format of the header depends on the compatibility (-c) flag.  */

void
process_copy_out ()
{
  int res;			/* Result of functions.  */
  dynamic_string input_name;	/* Name of file read from stdin.  */
  struct utimbuf times;		/* For resetting file times after copy.  */
  struct stat file_stat;	/* Stat record for file.  */
  struct new_cpio_header file_hdr; /* Output header information.  */
  int in_file_des;		/* Source file descriptor.  */
  int out_file_des;		/* Output file descriptor.  */
  char *p;

  /* Initialize the copy out.  */
  ds_init (&input_name, 128);
  /* Initialize this in case it has members we don't know to set.  */
  bzero (&times, sizeof (struct utimbuf));
  file_hdr.c_magic = 070707;

  /* Check whether the output file might be a tape.  */
  out_file_des = archive_des;
  if (_isrmt (out_file_des))
    {
      output_is_special = 1;
      output_is_seekable = 0;
    }
  else
    {
      if (fstat (out_file_des, &file_stat))
	error (1, errno, _("standard output is closed"));
      output_is_special =
#ifdef S_ISBLK
	S_ISBLK (file_stat.st_mode) ||
#endif
	S_ISCHR (file_stat.st_mode);
      output_is_seekable = S_ISREG (file_stat.st_mode);
    }

  if (append_flag)
    {
      process_copy_in ();
      prepare_append (out_file_des);
    }

  /* Copy files with names read from stdin.  */
  while (ds_fgetstr (stdin, &input_name, name_end) != NULL)
    {
      /* Check for blank line.  */
      if (input_name.ds_string[0] == 0)
	{
	  error (0, 0, _("blank line ignored"));
	  continue;
	}

      /* Process next file.  */
      if ((*xstat) (input_name.ds_string, &file_stat) < 0)
	error (0, errno, "%s", input_name.ds_string);
      else
	{
	  /* Set values in output header.  */
	  file_hdr.c_dev_maj = major (file_stat.st_dev);
	  file_hdr.c_dev_min = minor (file_stat.st_dev);
	  file_hdr.c_ino = file_stat.st_ino;
	  /* For POSIX systems that don't define the S_IF macros,
	     we can't assume that S_ISfoo means the standard Unix
	     S_IFfoo bit(s) are set.  So do it manually, with a
	     different name.  Bleah.  */
	  file_hdr.c_mode = (file_stat.st_mode & 07777);
	  if (S_ISREG (file_stat.st_mode))
	    file_hdr.c_mode |= CP_IFREG;
	  else if (S_ISDIR (file_stat.st_mode))
	    file_hdr.c_mode |= CP_IFDIR;
#ifdef S_ISBLK
	  else if (S_ISBLK (file_stat.st_mode))
	    file_hdr.c_mode |= CP_IFBLK;
#endif
#ifdef S_ISCHR
	  else if (S_ISCHR (file_stat.st_mode))
	    file_hdr.c_mode |= CP_IFCHR;
#endif
#ifdef S_ISFIFO
	  else if (S_ISFIFO (file_stat.st_mode))
	    file_hdr.c_mode |= CP_IFIFO;
#endif
#ifdef S_ISLNK
	  else if (S_ISLNK (file_stat.st_mode))
	    file_hdr.c_mode |= CP_IFLNK;
#endif
#ifdef S_ISSOCK
	  else if (S_ISSOCK (file_stat.st_mode))
	    file_hdr.c_mode |= CP_IFSOCK;
#endif
#ifdef S_ISNWK
	  else if (S_ISNWK (file_stat.st_mode))
	    file_hdr.c_mode |= CP_IFNWK;
#endif
	  file_hdr.c_uid = file_stat.st_uid;
	  file_hdr.c_gid = file_stat.st_gid;
	  file_hdr.c_nlink = file_stat.st_nlink;
	  file_hdr.c_rdev_maj = major (file_stat.st_rdev);
	  file_hdr.c_rdev_min = minor (file_stat.st_rdev);
	  file_hdr.c_mtime = file_stat.st_mtime;
	  file_hdr.c_filesize = file_stat.st_size;
	  file_hdr.c_chksum = 0;
	  file_hdr.c_tar_linkname = NULL;
	  
	  if (archive_format == arf_tar || archive_format == arf_ustar)
	    {
	      if (file_hdr.c_mode & CP_IFDIR)
		{
		  int len = strlen (input_name.ds_string);
		  /* Make sure the name ends with a slash */
		  if (input_name.ds_string[len-1] != '/')
		    {
		      ds_resize (&input_name, len + 2);
		      input_name.ds_string[len] = '/';
		      input_name.ds_string[len+1] = 0;
		    }
		}
	    }
	  
	  /* Strip leading `./' from the filename.  */
	  p = input_name.ds_string;
	  while (*p == '.' && *(p + 1) == '/')
	    {
	      ++p;
	      while (*p == '/')
		++p;
	    }
#ifndef HPUX_CDF
	  file_hdr.c_name = p;
	  file_hdr.c_namesize = strlen (p) + 1;
#else
	  if ( (archive_format != arf_tar) && (archive_format != arf_ustar) )
	    {
	      /* We mark CDF's in cpio files by adding a 2nd `/' after the
		 "hidden" directory name.  We need to do this so we can
		 properly recreate the directory as hidden (in case the
		 files of a directory go into the archive before the
		 directory itself (e.g from "find ... -depth ... | cpio")).  */
	      file_hdr.c_name = add_cdf_double_slashes (p);
	      file_hdr.c_namesize = strlen (file_hdr.c_name) + 1;
	    }
	  else
	    {
	      /* We don't mark CDF's in tar files.  We assume the "hidden"
		 directory will always go into the archive before any of
		 its files.  */
	      file_hdr.c_name = p;
	      file_hdr.c_namesize = strlen (p) + 1;
	    }
#endif
	  if ((archive_format == arf_tar || archive_format == arf_ustar)
	      && is_tar_filename_too_long (file_hdr.c_name))
	    {
	      error (0, 0, _("%s: file name too long"),
		     file_hdr.c_name);
	      continue;
	    }
	  
	  /* Copy the named file to the output.  */
	  switch (file_hdr.c_mode & CP_IFMT)
	    {
	    case CP_IFREG:
	      if (archive_format == arf_tar || archive_format == arf_ustar)
		{
		  char *otherfile;
		  if ((otherfile = find_inode_file (file_hdr.c_ino,
						    file_hdr.c_dev_maj,
						    file_hdr.c_dev_min)))
		    {
		      file_hdr.c_tar_linkname = otherfile;
		      write_out_header (&file_hdr, out_file_des);
		      break;
		    }
		}
	      if ( (archive_format == arf_newascii || archive_format == arf_crcascii)
		  && (file_hdr.c_nlink > 1) )
		{
		  if (last_link (&file_hdr) )
		    {
		      writeout_other_defers (&file_hdr, out_file_des);
		    }
		  else
		    {
		      add_link_defer (&file_hdr);
		      break;
		    }
		}
	      in_file_des = open (input_name.ds_string,
				  O_RDONLY | O_BINARY, 0);
	      if (in_file_des < 0)
		{
		  error (0, errno, "%s", input_name.ds_string);
		  continue;
		}

	      if (archive_format == arf_crcascii)
		file_hdr.c_chksum = read_for_checksum (in_file_des,
						       file_hdr.c_filesize,
						       input_name.ds_string);

	      write_out_header (&file_hdr, out_file_des);
	      copy_files_disk_to_tape (in_file_des, out_file_des, file_hdr.c_filesize, input_name.ds_string);
	      warn_if_file_changed(input_name.ds_string, file_hdr.c_filesize,
                                   file_hdr.c_mtime);

	      if (archive_format == arf_tar || archive_format == arf_ustar)
		add_inode (file_hdr.c_ino, file_hdr.c_name, file_hdr.c_dev_maj,
			   file_hdr.c_dev_min);

	      tape_pad_output (out_file_des, file_hdr.c_filesize);

	      if (close (in_file_des) < 0)
		error (0, errno, "%s", input_name.ds_string);
	      if (reset_time_flag)
		{
		  times.actime = file_stat.st_atime;
		  times.modtime = file_stat.st_mtime;
		  /* Debian hack: Silently ignore EROFS because
                     reading the file won't have upset its timestamp
                     if it's on a read-only filesystem.  This has been
                     submitted as a suggestion to
                     "bug-gnu-utils@prep.ai.mit.edu".  -BEM */
		  if (utime (file_hdr.c_name, &times) < 0
		      && errno != EROFS)
		    error (0, errno, "%s", file_hdr.c_name);
		}
	      break;

	    case CP_IFDIR:
	      file_hdr.c_filesize = 0;
	      write_out_header (&file_hdr, out_file_des);
	      break;

	    case CP_IFCHR:
	    case CP_IFBLK:
#ifdef CP_IFSOCK
	    case CP_IFSOCK:
#endif
#ifdef CP_IFIFO
	    case CP_IFIFO:
#endif
	      if (archive_format == arf_tar)
		{
		  error (0, 0, _("%s not dumped: not a regular file"),
			 file_hdr.c_name);
		  continue;
		}
	      else if (archive_format == arf_ustar)
		{
		  char *otherfile;
		  if ((otherfile = find_inode_file (file_hdr.c_ino,
						    file_hdr.c_dev_maj,
						    file_hdr.c_dev_min)))
		    {
		      /* This file is linked to another file already in the 
		         archive, so write it out as a hard link. */
		      file_hdr.c_mode = (file_stat.st_mode & 07777);
		      file_hdr.c_mode |= CP_IFREG;
		      file_hdr.c_tar_linkname = otherfile;
		      write_out_header (&file_hdr, out_file_des);
		      break;
		    }
		  add_inode (file_hdr.c_ino, file_hdr.c_name, 
			     file_hdr.c_dev_maj, file_hdr.c_dev_min);
		}
	      file_hdr.c_filesize = 0;
	      write_out_header (&file_hdr, out_file_des);
	      break;

#ifdef CP_IFLNK
	    case CP_IFLNK:
	      {
		char *link_name = (char *) xmalloc (file_stat.st_size + 1);
		int link_size;

		link_size = readlink (input_name.ds_string, link_name,
			              file_stat.st_size);
		if (link_size < 0)
		  {
		    error (0, errno, "%s", input_name.ds_string);
		    free (link_name);
		    continue;
		  }
		file_hdr.c_filesize = link_size;
		if (archive_format == arf_tar || archive_format == arf_ustar)
		  {
		    if (link_size + 1 > 100)
		      {
			error (0, 0, _("%s: symbolic link too long"),
			       file_hdr.c_name);
		      }
		    else
		      {
			link_name[link_size] = '\0';
			file_hdr.c_tar_linkname = link_name;
			write_out_header (&file_hdr, out_file_des);
		      }
		  }
		else
		  {
		    write_out_header (&file_hdr, out_file_des);
		    tape_buffered_write (link_name, out_file_des, link_size);
		    tape_pad_output (out_file_des, link_size);
		  }
		free (link_name);
	      }
	      break;
#endif

	    default:
	      error (0, 0, _("%s: unknown file type"), input_name.ds_string);
	    }

	  if (verbose_flag)
	    fprintf (stderr, "%s\n", input_name.ds_string);
	  if (dot_flag)
	    fputc ('.', stderr);
	}
    }

  writeout_final_defers(out_file_des);
  /* The collection is complete; append the trailer.  */
  file_hdr.c_ino = 0;
  file_hdr.c_mode = 0;
  file_hdr.c_uid = 0;
  file_hdr.c_gid = 0;
  file_hdr.c_nlink = 1;		/* Must be 1 for crc format.  */
  file_hdr.c_dev_maj = 0;
  file_hdr.c_dev_min = 0;
  file_hdr.c_rdev_maj = 0;
  file_hdr.c_rdev_min = 0;
  file_hdr.c_mtime = 0;
  file_hdr.c_chksum = 0;

  file_hdr.c_filesize = 0;
  file_hdr.c_namesize = 11;
  file_hdr.c_name = "TRAILER!!!";
  if (archive_format != arf_tar && archive_format != arf_ustar)
    write_out_header (&file_hdr, out_file_des);
  else
    {
      tape_buffered_write (zeros_512, out_file_des, 512);
      tape_buffered_write (zeros_512, out_file_des, 512);
    }

  /* Fill up the output block.  */
  tape_clear_rest_of_block (out_file_des);
  tape_empty_output_buffer (out_file_des);
  if (dot_flag)
    fputc ('\n', stderr);
  if (!quiet_flag)
    {
      res = (output_bytes + io_block_size - 1) / io_block_size;
      fprintf (stderr, ngettext ("%d block\n", "%d blocks\n", res), res);
    }
}


