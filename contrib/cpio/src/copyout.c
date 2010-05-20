/* $FreeBSD$ */

/* copyout.c - create a cpio archive
   Copyright (C) 1990, 1991, 1992, 2001, 2003, 2004,
   2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301 USA.  */

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
#include <paxlib.h>

static int check_rdev ();

/* Read FILE_SIZE bytes of FILE_NAME from IN_FILE_DES and
   compute and return a checksum for them.  */

static unsigned int
read_for_checksum (int in_file_des, int file_size, char *file_name)
{
  unsigned int crc;
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
  write_nuls_to_file (io_block_size - output_size, out_file_des, 
                      tape_buffered_write);
}

/* Write NULs on OUT_FILE_DES to move from OFFSET (the current location)
   to the end of the header.  */

static void
tape_pad_output (int out_file_des, int offset)
{
  size_t pad;

  if (archive_format == arf_newascii || archive_format == arf_crcascii)
    pad = (4 - (offset % 4)) % 4;
  else if (archive_format == arf_tar || archive_format == arf_ustar)
    pad = (512 - (offset % 512)) % 512;
  else if (archive_format != arf_oldascii && archive_format != arf_hpoldascii)
    pad = (2 - (offset % 2)) % 2;
  else
    pad = 0;

  if (pad != 0)
    write_nuls_to_file (pad, out_file_des, tape_buffered_write);
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
count_defered_links_to_dev_ino (struct cpio_file_stat *file_hdr)
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
last_link (struct cpio_file_stat *file_hdr)
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
add_link_defer (struct cpio_file_stat *file_hdr)
{
  struct deferment *d;
  d = create_deferment (file_hdr);
  d->next = deferouts;
  deferouts = d;
}

/* We are about to put a file into a newc or crc archive that is
   multiply linked.  We have already seen and deferred all of the
   other links to the file but haven't written them into the archive.
   Write the other links into the archive, and remove them from the
   deferouts list.  */

static void
writeout_other_defers (struct cpio_file_stat *file_hdr, int out_des)
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
writeout_defered_file (struct cpio_file_stat *header, int out_file_des)
{
  int in_file_des;
  struct cpio_file_stat file_hdr;

  file_hdr = *header;


  in_file_des = open (header->c_name,
		      O_RDONLY | O_BINARY, 0);
  if (in_file_des < 0)
    {
      open_error (header->c_name);
      return;
    }

  if (archive_format == arf_crcascii)
    file_hdr.c_chksum = read_for_checksum (in_file_des,
					   file_hdr.c_filesize,
					   header->c_name);

  if (write_out_header (&file_hdr, out_file_des))
    return;
  copy_files_disk_to_tape (in_file_des, out_file_des, file_hdr.c_filesize,
			   header->c_name);
  warn_if_file_changed(header->c_name, file_hdr.c_filesize, file_hdr.c_mtime);

  if (archive_format == arf_tar || archive_format == arf_ustar)
    add_inode (file_hdr.c_ino, file_hdr.c_name, file_hdr.c_dev_maj,
	       file_hdr.c_dev_min);

  tape_pad_output (out_file_des, file_hdr.c_filesize);

  if (reset_time_flag)
    set_file_times (in_file_des, file_hdr.c_name, file_hdr.c_mtime,
		    file_hdr.c_mtime);
  if (close (in_file_des) < 0)
    close_error (header->c_name);
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
	  struct cpio_file_stat file_hdr;
	  file_hdr = d->header;
	  file_hdr.c_filesize = 0;
	  write_out_header (&file_hdr, out_des);
	}
      deferouts = deferouts->next;
    }
}

/* FIXME: to_ascii could be used instead of to_oct() and to_octal() from tar,
   so it should be moved to paxutils too.
   Allowed values for logbase are: 1 (binary), 2, 3 (octal), 4 (hex) */
int
to_ascii (char *where, uintmax_t v, size_t digits, unsigned logbase)
{
  static char codetab[] = "0123456789ABCDEF";
  int i = digits;
  
  do
    {
      where[--i] = codetab[(v & ((1 << logbase) - 1))];
      v >>= logbase;
    }
  while (i);

  return v != 0;
}

static void
field_width_error (const char *filename, const char *fieldname)
{
  error (0, 0, _("%s: field width not sufficient for storing %s"),
	 filename, fieldname);
}

static void
field_width_warning (const char *filename, const char *fieldname)
{
  if (warn_option & CPIO_WARN_TRUNCATE)
    error (0, 0, _("%s: truncating %s"), filename, fieldname);
}

void
to_ascii_or_warn (char *where, uintmax_t n, size_t digits,
		  unsigned logbase,
		  const char *filename, const char *fieldname)
{
  if (to_ascii (where, n, digits, logbase))
    field_width_warning (filename, fieldname);
}    

int
to_ascii_or_error (char *where, uintmax_t n, size_t digits,
		   unsigned logbase,
		   const char *filename, const char *fieldname)
{
  if (to_ascii (where, n, digits, logbase))
    {
      field_width_error (filename, fieldname);
      return 1;
    }
  return 0;
}    


int
write_out_new_ascii_header (const char *magic_string,
			    struct cpio_file_stat *file_hdr, int out_des)
{
  char ascii_header[110];
  char *p;

  p = stpcpy (ascii_header, magic_string);
  to_ascii_or_warn (p, file_hdr->c_ino, 8, LG_16,
		    file_hdr->c_name, _("inode number"));
  p += 8;
  to_ascii_or_warn (p, file_hdr->c_mode, 8, LG_16, file_hdr->c_name,
		    _("file mode"));
  p += 8;
  to_ascii_or_warn (p, file_hdr->c_uid, 8, LG_16, file_hdr->c_name,
		    _("uid"));
  p += 8;
  to_ascii_or_warn (p, file_hdr->c_gid, 8, LG_16, file_hdr->c_name,
		    _("gid"));
  p += 8;
  to_ascii_or_warn (p, file_hdr->c_nlink, 8, LG_16, file_hdr->c_name,
		    _("number of links"));
  p += 8;
  to_ascii_or_warn (p, file_hdr->c_mtime, 8, LG_16, file_hdr->c_name,
		    _("modification time"));
  p += 8;
  if (to_ascii_or_error (p, file_hdr->c_filesize, 8, LG_16, file_hdr->c_name,
			 _("file size")))
    return 1;
  p += 8;
  if (to_ascii_or_error (p, file_hdr->c_dev_maj, 8, LG_16, file_hdr->c_name,
			 _("device major number")))
    return 1;
  p += 8;
  if (to_ascii_or_error (p, file_hdr->c_dev_min, 8, LG_16, file_hdr->c_name,
			 _("device minor number")))
    return 1;
  p += 8;
  if (to_ascii_or_error (p, file_hdr->c_rdev_maj, 8, LG_16, file_hdr->c_name,
			 _("rdev major")))
    return 1;
  p += 8;
  if (to_ascii_or_error (p, file_hdr->c_rdev_min, 8, LG_16, file_hdr->c_name,
			 _("rdev minor")))
    return 1;
  p += 8;
  if (to_ascii_or_error (p, file_hdr->c_namesize, 8, LG_16, file_hdr->c_name,
			 _("name size")))
    return 1;
  p += 8;
  to_ascii (p, file_hdr->c_chksum & 0xffffffff, 8, LG_16);

  tape_buffered_write (ascii_header, out_des, sizeof ascii_header);

  /* Write file name to output.  */
  tape_buffered_write (file_hdr->c_name, out_des, (long) file_hdr->c_namesize);
  tape_pad_output (out_des, file_hdr->c_namesize + sizeof ascii_header);
  return 0;
}  

int
write_out_old_ascii_header (dev_t dev, dev_t rdev,
			    struct cpio_file_stat *file_hdr, int out_des)
{
  char ascii_header[76];
  char *p = ascii_header;
  
  to_ascii (p, file_hdr->c_magic, 6, LG_8);
  p += 6;
  to_ascii_or_warn (p, dev, 6, LG_8, file_hdr->c_name, _("device number"));
  p += 6;
  to_ascii_or_warn (p, file_hdr->c_ino, 6, LG_8, file_hdr->c_name,
		    _("inode number"));
  p += 6;
  to_ascii_or_warn (p, file_hdr->c_mode, 6, LG_8, file_hdr->c_name,
		    _("file mode"));
  p += 6;
  to_ascii_or_warn (p, file_hdr->c_uid, 6, LG_8, file_hdr->c_name, _("uid"));
  p += 6;
  to_ascii_or_warn (p, file_hdr->c_gid, 6, LG_8, file_hdr->c_name, _("gid"));
  p += 6;
  to_ascii_or_warn (p, file_hdr->c_nlink, 6, LG_8, file_hdr->c_name,
		    _("number of links"));
  p += 6;
  to_ascii_or_warn (p, rdev, 6, LG_8, file_hdr->c_name, _("rdev"));
  p += 6;
  to_ascii_or_warn (p, file_hdr->c_mtime, 11, LG_8, file_hdr->c_name,
		    _("modification time"));
  p += 11;
  if (to_ascii_or_error (p, file_hdr->c_namesize, 6, LG_8, file_hdr->c_name,
			 _("name size")))
    return 1;
  p += 6;
  if (to_ascii_or_error (p, file_hdr->c_filesize, 11, LG_8, file_hdr->c_name,
			 _("file size")))
    return 1;

  tape_buffered_write (ascii_header, out_des, sizeof ascii_header);

  /* Write file name to output.  */
  tape_buffered_write (file_hdr->c_name, out_des, file_hdr->c_namesize);
  return 0;
}

void
hp_compute_dev (struct cpio_file_stat *file_hdr, dev_t *pdev, dev_t *prdev)
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
      *pdev = *prdev = makedev (0, 1);
      break;

    default:
      *pdev = makedev (file_hdr->c_dev_maj, file_hdr->c_dev_min);
      *prdev = makedev (file_hdr->c_rdev_maj, file_hdr->c_rdev_min);
      break;
    }
}

int
write_out_binary_header (dev_t rdev,
			 struct cpio_file_stat *file_hdr, int out_des)
{
  struct old_cpio_header short_hdr;

  short_hdr.c_magic = 070707;
  short_hdr.c_dev = makedev (file_hdr->c_dev_maj, file_hdr->c_dev_min);

  if ((warn_option & CPIO_WARN_TRUNCATE) && (file_hdr->c_ino >> 16) != 0)
    error (0, 0, _("%s: truncating inode number"), file_hdr->c_name);

  short_hdr.c_ino = file_hdr->c_ino & 0xFFFF;
  if (short_hdr.c_ino != file_hdr->c_ino)
    field_width_warning (file_hdr->c_name, _("inode number"));
  
  short_hdr.c_mode = file_hdr->c_mode & 0xFFFF;
  if (short_hdr.c_mode != file_hdr->c_mode)
    field_width_warning (file_hdr->c_name, _("file mode"));
  
  short_hdr.c_uid = file_hdr->c_uid & 0xFFFF;
  if (short_hdr.c_uid != file_hdr->c_uid)
    field_width_warning (file_hdr->c_name, _("uid"));
  
  short_hdr.c_gid = file_hdr->c_gid & 0xFFFF;
  if (short_hdr.c_gid != file_hdr->c_gid)
    field_width_warning (file_hdr->c_name, _("gid"));
  
  short_hdr.c_nlink = file_hdr->c_nlink & 0xFFFF;
  if (short_hdr.c_nlink != file_hdr->c_nlink)
    field_width_warning (file_hdr->c_name, _("number of links"));
		      
  short_hdr.c_rdev = rdev;
  short_hdr.c_mtimes[0] = file_hdr->c_mtime >> 16;
  short_hdr.c_mtimes[1] = file_hdr->c_mtime & 0xFFFF;

  short_hdr.c_namesize = file_hdr->c_namesize & 0xFFFF;
  if (short_hdr.c_namesize != file_hdr->c_namesize)
    {
      field_width_error (file_hdr->c_name, _("name size"));
      return 1;
    }
		      
  short_hdr.c_filesizes[0] = file_hdr->c_filesize >> 16;
  short_hdr.c_filesizes[1] = file_hdr->c_filesize & 0xFFFF;

  if (((off_t)short_hdr.c_filesizes[0] << 16) + short_hdr.c_filesizes[1]
       != file_hdr->c_filesize)
    {
      field_width_error (file_hdr->c_name, _("file size"));
      return 1;
    }
		      
  /* Output the file header.  */
  tape_buffered_write ((char *) &short_hdr, out_des, 26);

  /* Write file name to output.  */
  tape_buffered_write (file_hdr->c_name, out_des, file_hdr->c_namesize);

  tape_pad_output (out_des, file_hdr->c_namesize + 26);
  return 0;
}


/* Write out header FILE_HDR, including the file name, to file
   descriptor OUT_DES.  */

int 
write_out_header (struct cpio_file_stat *file_hdr, int out_des)
{
  dev_t dev;
  dev_t rdev;
  
  switch (archive_format)
    {
    case arf_newascii:
      return write_out_new_ascii_header ("070701", file_hdr, out_des);
      
    case arf_crcascii:
      return write_out_new_ascii_header ("070702", file_hdr, out_des);
      
    case arf_oldascii:
      return write_out_old_ascii_header (makedev (file_hdr->c_dev_maj,
						  file_hdr->c_dev_min),
					 makedev (file_hdr->c_rdev_maj,
						  file_hdr->c_rdev_min),
					 file_hdr, out_des);
      
    case arf_hpoldascii:
      hp_compute_dev (file_hdr, &dev, &rdev);
      return write_out_old_ascii_header (dev, rdev, file_hdr, out_des);
      
    case arf_tar:
    case arf_ustar:
      if (is_tar_filename_too_long (file_hdr->c_name))
	{
	  error (0, 0, _("%s: file name too long"), file_hdr->c_name);
	  return 1;
	}
      write_out_tar_header (file_hdr, out_des); /* FIXME: No error checking */
      return 0;

    case arf_binary:
      return write_out_binary_header (makedev (file_hdr->c_rdev_maj,
					       file_hdr->c_rdev_min),
				      file_hdr, out_des);

    case arf_hpbinary:
      hp_compute_dev (file_hdr, &dev, &rdev);
      /* FIXME: dev ignored. Should it be? */
      return write_out_binary_header (rdev, file_hdr, out_des);

    default:
      abort ();
    }
}

static void
assign_string (char **pvar, char *value)
{
  char *p = xrealloc (*pvar, strlen (value) + 1);
  strcpy (p, value);
  *pvar = p;
}

/* Read a list of file names from the standard input
   and write a cpio collection on the standard output.
   The format of the header depends on the compatibility (-c) flag.  */

void
process_copy_out ()
{
  int res;			/* Result of functions.  */
  dynamic_string input_name;	/* Name of file read from stdin.  */
  struct stat file_stat;	/* Stat record for file.  */
  struct cpio_file_stat file_hdr; /* Output header information.  */
  int in_file_des;		/* Source file descriptor.  */
  int out_file_des;		/* Output file descriptor.  */
  char *orig_file_name = NULL;

  /* Initialize the copy out.  */
  ds_init (&input_name, 128);
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
	stat_error (input_name.ds_string);
      else
	{
	  /* Set values in output header.  */
	  stat_to_cpio (&file_hdr, &file_stat);
	  
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

	  switch (check_rdev (&file_hdr))
	    {
	      case 1:
		error (0, 0, "%s not dumped: major number would be truncated",
		       file_hdr.c_name);
		continue;
	      case 2:
		error (0, 0, "%s not dumped: minor number would be truncated",
		       file_hdr.c_name);
		continue;
	      case 4:
		error (0, 0, "%s not dumped: device number would be truncated",
		       file_hdr.c_name);
		continue;
	    }

	  assign_string (&orig_file_name, input_name.ds_string);
	  cpio_safer_name_suffix (input_name.ds_string, false,
				  abs_paths_flag, true);
#ifndef HPUX_CDF
	  file_hdr.c_name = input_name.ds_string;
	  file_hdr.c_namesize = strlen (input_name.ds_string) + 1;
#else
	  if ( (archive_format != arf_tar) && (archive_format != arf_ustar) )
	    {
	      /* We mark CDF's in cpio files by adding a 2nd `/' after the
		 "hidden" directory name.  We need to do this so we can
		 properly recreate the directory as hidden (in case the
		 files of a directory go into the archive before the
		 directory itself (e.g from "find ... -depth ... | cpio")).  */
	      file_hdr.c_name = add_cdf_double_slashes (input_name.ds_string);
	      file_hdr.c_namesize = strlen (file_hdr.c_name) + 1;
	    }
	  else
	    {
	      /* We don't mark CDF's in tar files.  We assume the "hidden"
		 directory will always go into the archive before any of
		 its files.  */
	      file_hdr.c_name = input_name.ds_string;
	      file_hdr.c_namesize = strlen (input_name.ds_string) + 1;
	    }
#endif

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
		      if (write_out_header (&file_hdr, out_file_des))
			continue;
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
	      in_file_des = open (orig_file_name,
				  O_RDONLY | O_BINARY, 0);
	      if (in_file_des < 0)
		{
		  open_error (orig_file_name);
		  continue;
		}

	      if (archive_format == arf_crcascii)
		file_hdr.c_chksum = read_for_checksum (in_file_des,
						       file_hdr.c_filesize,
						       orig_file_name);

	      if (write_out_header (&file_hdr, out_file_des))
		continue;
	      copy_files_disk_to_tape (in_file_des,
				       out_file_des, file_hdr.c_filesize,
				       orig_file_name);
	      warn_if_file_changed(orig_file_name, file_hdr.c_filesize,
                                   file_hdr.c_mtime);

	      if (archive_format == arf_tar || archive_format == arf_ustar)
		add_inode (file_hdr.c_ino, orig_file_name, file_hdr.c_dev_maj,
			   file_hdr.c_dev_min);

	      tape_pad_output (out_file_des, file_hdr.c_filesize);

	      if (reset_time_flag)
                set_file_times (in_file_des,
				orig_file_name,
                                file_stat.st_atime, file_stat.st_mtime);
	      if (close (in_file_des) < 0)
		close_error (orig_file_name);
	      break;

	    case CP_IFDIR:
	      file_hdr.c_filesize = 0;
	      if (write_out_header (&file_hdr, out_file_des))
		continue;
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
			 orig_file_name);
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
		      if (write_out_header (&file_hdr, out_file_des))
			continue;
		      break;
		    }
		  add_inode (file_hdr.c_ino, orig_file_name, 
			     file_hdr.c_dev_maj, file_hdr.c_dev_min);
		}
	      file_hdr.c_filesize = 0;
	      if (write_out_header (&file_hdr, out_file_des))
		continue;
	      break;

#ifdef CP_IFLNK
	    case CP_IFLNK:
	      {
		char *link_name = (char *) xmalloc (file_stat.st_size + 1);
		int link_size;

		link_size = readlink (orig_file_name, link_name,
			              file_stat.st_size);
		if (link_size < 0)
		  {
		    readlink_warn (orig_file_name);
		    free (link_name);
		    continue;
		  }
		link_name[link_size] = 0;
		cpio_safer_name_suffix (link_name, false,
					abs_paths_flag, true);
		link_size = strlen (link_name);
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
			if (write_out_header (&file_hdr, out_file_des))
			  continue;
		      }
		  }
		else
		  {
		    if (write_out_header (&file_hdr, out_file_des))
		      continue;
		    tape_buffered_write (link_name, out_file_des, link_size);
		    tape_pad_output (out_file_des, link_size);
		  }
		free (link_name);
	      }
	      break;
#endif

	    default:
	      error (0, 0, _("%s: unknown file type"), orig_file_name);
	    }
	  
	  if (verbose_flag)
	    fprintf (stderr, "%s\n", orig_file_name);
	  if (dot_flag)
	    fputc ('.', stderr);
	}
    }

  free (orig_file_name);
  
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
  file_hdr.c_name = CPIO_TRAILER_NAME;
  if (archive_format != arf_tar && archive_format != arf_ustar)
    write_out_header (&file_hdr, out_file_des);
  else
    write_nuls_to_file (1024, out_file_des, tape_buffered_write);

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

static int
check_rdev (file_hdr)
     struct cpio_file_stat *file_hdr;
{
  if (archive_format == arf_newascii || archive_format == arf_crcascii)
    {
      if ((file_hdr->c_rdev_maj & 0xFFFFFFFF) != file_hdr->c_rdev_maj)
        return 1;
      if ((file_hdr->c_rdev_min & 0xFFFFFFFF) != file_hdr->c_rdev_min)
        return 2;
    }
  else if (archive_format == arf_oldascii || archive_format == arf_hpoldascii)
    {
#ifndef __MSDOS__
      dev_t rdev;

      rdev = makedev (file_hdr->c_rdev_maj, file_hdr->c_rdev_min);
      if (archive_format == arf_oldascii)
	{
	  if ((rdev & 0xFFFF) != rdev)
	    return 4;
	}
      else
	{
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
		/* We could handle one more bit if longs are >= 33 bits.  */
		if ((rdev & 037777777777) != rdev)
		  return 4;
		break;
	      default:
		if ((rdev & 0xFFFF) != rdev)
		  return 4;
		break;
	    }
	}
#endif
    }
  else if (archive_format == arf_tar || archive_format == arf_ustar)
    {
      /* The major and minor formats are limited to 7 octal digits in ustar
	 format, and to_oct () adds a gratuitous trailing blank to further
	 limit the format to 6 octal digits.  */
      if ((file_hdr->c_rdev_maj & 0777777) != file_hdr->c_rdev_maj)
        return 1;
      if ((file_hdr->c_rdev_min & 0777777) != file_hdr->c_rdev_min)
        return 2;
    }
  else
    {
#ifndef __MSDOS__
      dev_t rdev;

      rdev = makedev (file_hdr->c_rdev_maj, file_hdr->c_rdev_min);
      if (archive_format != arf_hpbinary)
	{
	  if ((rdev & 0xFFFF) != rdev)
	return 4;
    }
  else
    {
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
	    if ((rdev & 0xFFFFFFFF) != rdev)
	      return 4;
	    file_hdr->c_filesize = rdev;
	    rdev = makedev (0, 1);
	    break;
	  default:
	    if ((rdev & 0xFFFF) != rdev)
	      return 4;
	    break;
	}
    }
#endif
  }
  return 0;
}
