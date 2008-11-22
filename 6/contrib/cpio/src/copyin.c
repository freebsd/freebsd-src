/* $FreeBSD$ */

/* copyin.c - extract or list a cpio archive
   Copyright (C) 1990,1991,1992,2001,2002,2003,2004 Free Software Foundation, Inc.

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
#include "dirname.h"
#include <rmt.h>
#ifndef	FNM_PATHNAME
#include <fnmatch.h>
#endif
#include <langinfo.h>

#ifndef HAVE_LCHOWN
#define lchown chown
#endif

static void copyin_regular_file(struct new_cpio_header* file_hdr,
				int in_file_des);

void
warn_junk_bytes (long bytes_skipped)
{
  error (0, 0, ngettext ("warning: skipped %ld byte of junk",
			 "warning: skipped %ld bytes of junk", bytes_skipped),
	 bytes_skipped);
}


static int
query_rename(struct new_cpio_header* file_hdr, FILE *tty_in, FILE *tty_out,
	     FILE *rename_in)
{
  char *str_res;		/* Result for string function.  */
  static dynamic_string new_name;	/* New file name for rename option.  */
  static int initialized_new_name = false;
  if (!initialized_new_name)
  {
    ds_init (&new_name, 128);
    initialized_new_name = true;
  }

  if (rename_flag)
    {
      fprintf (tty_out, _("rename %s -> "), file_hdr->c_name);
      fflush (tty_out);
      str_res = ds_fgets (tty_in, &new_name);
    }
  else
    {
      str_res = ds_fgetstr (rename_in, &new_name, '\n');
    }
  if (str_res == NULL || str_res[0] == 0)
    {
      return -1;
    }
  else
  /* Debian hack: file_hrd.c_name is sometimes set to
     point to static memory by code in tar.c.  This
     causes a segfault.  This has been fixed and an
     additional check to ensure that the file name
     is not too long has been added.  (Reported by
     Horst Knobloch.)  This bug has been reported to
     "bug-gnu-utils@prep.ai.mit.edu". (99/1/6) -BEM */
    {
      if (archive_format != arf_tar && archive_format != arf_ustar)
	{
	  free (file_hdr->c_name);
	  file_hdr->c_name = xstrdup (new_name.ds_string);
	}
      else
	{
	  if (is_tar_filename_too_long (new_name.ds_string))
	    error (0, 0, _("%s: file name too long"),
		   new_name.ds_string);
	  else
	    strcpy (file_hdr->c_name, new_name.ds_string);
	}
    }
  return 0;
}

/* Skip the padding on IN_FILE_DES after a header or file,
   up to the next header.
   The number of bytes skipped is based on OFFSET -- the current offset
   from the last start of a header (or file) -- and the current
   header type.  */

static void
tape_skip_padding (int in_file_des, int offset)
{
  int pad;

  if (archive_format == arf_crcascii || archive_format == arf_newascii)
    pad = (4 - (offset % 4)) % 4;
  else if (archive_format == arf_binary || archive_format == arf_hpbinary)
    pad = (2 - (offset % 2)) % 2;
  else if (archive_format == arf_tar || archive_format == arf_ustar)
    pad = (512 - (offset % 512)) % 512;
  else
    pad = 0;

  if (pad != 0)
    tape_toss_input (in_file_des, pad);
}


static void
list_file(struct new_cpio_header* file_hdr, int in_file_des)
{
  if (verbose_flag)
    {
#ifdef CP_IFLNK
      if ((file_hdr->c_mode & CP_IFMT) == CP_IFLNK)
	{
	  if (archive_format != arf_tar && archive_format != arf_ustar)
	    {
	      char *link_name = NULL;	/* Name of hard and symbolic links.  */

	      link_name = (char *) xmalloc ((unsigned int) file_hdr->c_filesize + 1);
	      link_name[file_hdr->c_filesize] = '\0';
	      tape_buffered_read (link_name, in_file_des, file_hdr->c_filesize);
	      long_format (file_hdr, link_name);
	      free (link_name);
	      tape_skip_padding (in_file_des, file_hdr->c_filesize);
	      return;
	    }
	  else
	    {
	      long_format (file_hdr, file_hdr->c_tar_linkname);
	      return;
	    }
	}
      else
#endif
	long_format (file_hdr, (char *) 0);
    }
  else
    {
      /* Debian hack: Modified to print a list of filenames
	 terminiated by a null character when the -t and -0
	 flags are used.  This has been submitted as a
	 suggestion to "bug-gnu-utils@prep.ai.mit.edu".  -BEM */
      printf ("%s%c", file_hdr->c_name, name_end);
    }

  crc = 0;
  tape_toss_input (in_file_des, file_hdr->c_filesize);
  tape_skip_padding (in_file_des, file_hdr->c_filesize);
  if (only_verify_crc_flag)
    {
#ifdef CP_IFLNK
      if ((file_hdr->c_mode & CP_IFMT) == CP_IFLNK)
	{
	  return;   /* links don't have a checksum */
	}
#endif
      if (crc != file_hdr->c_chksum)
	{
	  error (0, 0, _("%s: checksum error (0x%x, should be 0x%x)"),
		 file_hdr->c_name, crc, file_hdr->c_chksum);
	}
    }
}

static int
try_existing_file(struct new_cpio_header* file_hdr, int in_file_des,
		  int *existing_dir)
{
  struct stat file_stat;

  *existing_dir = false;
  if (lstat (file_hdr->c_name, &file_stat) == 0)
    {
      if (S_ISDIR (file_stat.st_mode)
	  && ((file_hdr->c_mode & CP_IFMT) == CP_IFDIR))
	{
	  /* If there is already a directory there that
	     we are trying to create, don't complain about
	     it.  */
	  *existing_dir = true;
	  return 0;
	}
      else if (!unconditional_flag
	       && file_hdr->c_mtime <= file_stat.st_mtime)
	{
	  error (0, 0, _("%s not created: newer or same age version exists"),
		 file_hdr->c_name);
	  tape_toss_input (in_file_des, file_hdr->c_filesize);
	  tape_skip_padding (in_file_des, file_hdr->c_filesize);
	  return -1;	/* Go to the next file.  */
	}
      else if (S_ISDIR (file_stat.st_mode) 
		? rmdir (file_hdr->c_name)
		: unlink (file_hdr->c_name))
	{
	  error (0, errno, _("cannot remove current %s"),
		 file_hdr->c_name);
	  tape_toss_input (in_file_des, file_hdr->c_filesize);
	  tape_skip_padding (in_file_des, file_hdr->c_filesize);
	  return -1;	/* Go to the next file.  */
	}
    }
  return 0;
}

/* The newc and crc formats store multiply linked copies of the same file 
   in the archive only once.  The actual data is attached to the last link 
   in the archive, and the other links all have a filesize of 0.  When a 
   file in the archive has multiple links and a filesize of 0, its data is 
   probably "attatched" to another file in the archive, so we can't create
   it right away.  We have to "defer" creating it until we have created
   the file that has the data "attatched" to it.  We keep a list of the
   "defered" links on deferments.  */

struct deferment *deferments = NULL;

/* Add a file header to the deferments list.  For now they all just
   go on one list, although we could optimize this if necessary.  */

static void
defer_copyin (struct new_cpio_header *file_hdr)
{
  struct deferment *d;
  d = create_deferment (file_hdr);
  d->next = deferments;
  deferments = d;
  return;
}

/* We just created a file that (probably) has some other links to it
   which have been defered.  Go through all of the links on the deferments
   list and create any which are links to this file.  */

static void
create_defered_links (struct new_cpio_header *file_hdr)
{
  struct deferment *d;
  struct deferment *d_prev;
  int	ino;
  int 	maj;
  int   min;
  int 	link_res;
  ino = file_hdr->c_ino;
  maj = file_hdr->c_dev_maj;
  min = file_hdr->c_dev_min;
  d = deferments;
  d_prev = NULL;
  while (d != NULL)
    {
      if ( (d->header.c_ino == ino) && (d->header.c_dev_maj == maj)
	  && (d->header.c_dev_min == min) )
	{
	  struct deferment *d_free;
	  link_res = link_to_name (d->header.c_name, file_hdr->c_name);
	  if (link_res < 0)
	    {
	      error (0, errno, _("cannot link %s to %s"),
		     d->header.c_name, file_hdr->c_name);
	    }
	  if (d_prev != NULL)
	    d_prev->next = d->next;
	  else
	    deferments = d->next;
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
}

/* We are skipping a file but there might be other links to it that we
   did not skip, so we have to copy its data for the other links.  Find
   the first link that we didn't skip and try to create that.  That will
   then create the other deferred links.  */

static int
create_defered_links_to_skipped (struct new_cpio_header *file_hdr,
				 int in_file_des)
{
  struct deferment *d;
  struct deferment *d_prev;
  int	ino;
  int 	maj;
  int   min;
  int 	link_res;
  if (file_hdr->c_filesize == 0)
    {
      /* The file doesn't have any data attached to it so we don't have
         to bother.  */
      return -1;
    }
  ino = file_hdr->c_ino;
  maj = file_hdr->c_dev_maj;
  min = file_hdr->c_dev_min;
  d = deferments;
  d_prev = NULL;
  while (d != NULL)
    {
      if ( (d->header.c_ino == ino) && (d->header.c_dev_maj == maj)
	  && (d->header.c_dev_min == min) )
	{
	  if (d_prev != NULL)
	    d_prev->next = d->next;
	  else
	    deferments = d->next;
	  free (file_hdr->c_name);
	  file_hdr->c_name = xstrdup(d->header.c_name);
	  free_deferment (d);
	  copyin_regular_file(file_hdr, in_file_des);
	  return 0;
	}
      else
	{
	  d_prev = d;
	  d = d->next;
	}
    }
  return -1;
}

/* If we had a multiply linked file that really was empty then we would
   have defered all of its links, since we never found any with data
   "attached", and they will still be on the deferment list even when
   we are done reading the whole archive.  Write out all of these
   empty links that are still on the deferments list.  */

static void
create_final_defers ()
{
  struct deferment *d;
  int	link_res;
  int	out_file_des;
  struct utimbuf times;		/* For setting file times.  */
  /* Initialize this in case it has members we don't know to set.  */
  bzero (&times, sizeof (struct utimbuf));
  
  for (d = deferments; d != NULL; d = d->next)
    {
      /* Debian hack: A line, which could cause an endless loop, was
         removed (97/1/2).  It was reported by Ronald F. Guilmette to
         the upstream maintainers. -BEM */
      /* Debian hack:  This was reported by Horst Knobloch. This bug has
         been reported to "bug-gnu-utils@prep.ai.mit.edu". (99/1/6) -BEM
         */
      link_res = link_to_maj_min_ino (d->header.c_name, 
		    d->header.c_dev_maj, d->header.c_dev_min,
		    d->header.c_ino);
      if (link_res == 0)
	{
	  continue;
	}
      out_file_des = open (d->header.c_name,
			   O_CREAT | O_WRONLY | O_BINARY, 0600);
      if (out_file_des < 0 && create_dir_flag)
	{
	  create_all_directories (d->header.c_name);
	  out_file_des = open (d->header.c_name,
			       O_CREAT | O_WRONLY | O_BINARY,
			       0600);
	}
      if (out_file_des < 0)
	{
	  error (0, errno, "%s", d->header.c_name);
	  continue;
	}

      /*
       *  Avoid race condition.
       *  Set chown and chmod before closing the file desc.
       *  pvrabec@redhat.com
       */
       
      /* File is now copied; set attributes.  */
      if (!no_chown_flag)
	if ((fchown (out_file_des,
		    set_owner_flag ? set_owner : d->header.c_uid,
	       set_group_flag ? set_group : d->header.c_gid) < 0)
	    && errno != EPERM)
	  error (0, errno, "%s", d->header.c_name);
      /* chown may have turned off some permissions we wanted. */
      if (fchmod (out_file_des, (int) d->header.c_mode) < 0)
	error (0, errno, "%s", d->header.c_name);

      if (close (out_file_des) < 0)
	error (0, errno, "%s", d->header.c_name);

      if (retain_time_flag)
	{
	  times.actime = times.modtime = d->header.c_mtime;
	  if (utime (d->header.c_name, &times) < 0)
	    error (0, errno, "%s", d->header.c_name);
	}
    }
}

static void
copyin_regular_file (struct new_cpio_header* file_hdr, int in_file_des)
{
  int out_file_des;		/* Output file descriptor.  */

  if (to_stdout_option)
    out_file_des = STDOUT_FILENO;
  else
    {
      /* Can the current file be linked to a previously copied file? */
      if (file_hdr->c_nlink > 1
	  && (archive_format == arf_newascii
	      || archive_format == arf_crcascii) )
	{
	  int link_res;
	  if (file_hdr->c_filesize == 0)
	    {
	      /* The newc and crc formats store multiply linked copies
		 of the same file in the archive only once.  The
		 actual data is attached to the last link in the
		 archive, and the other links all have a filesize
		 of 0.  Since this file has multiple links and a
		 filesize of 0, its data is probably attatched to
		 another file in the archive.  Save the link, and
		 process it later when we get the actual data.  We
		 can't just create it with length 0 and add the
		 data later, in case the file is readonly.  We still
		 lose if its parent directory is readonly (and we aren't
		 running as root), but there's nothing we can do about
		 that.  */
	      defer_copyin (file_hdr);
	      tape_toss_input (in_file_des, file_hdr->c_filesize);
	      tape_skip_padding (in_file_des, file_hdr->c_filesize);
	      return;
	    }
	  /* If the file has data (filesize != 0), then presumably
	     any other links have already been defer_copyin'ed(),
	     but GNU cpio version 2.0-2.2 didn't do that, so we
	     still have to check for links here (and also in case
	     the archive was created and later appeneded to). */
	  /* Debian hack: (97/1/2) This was reported by Ronald
	     F. Guilmette to the upstream maintainers. -BEM */
	  link_res = link_to_maj_min_ino (file_hdr->c_name, 
		    file_hdr->c_dev_maj, file_hdr->c_dev_min,
					  file_hdr->c_ino);
	  if (link_res == 0)
	    {
	      tape_toss_input (in_file_des, file_hdr->c_filesize);
	      tape_skip_padding (in_file_des, file_hdr->c_filesize);
	  return;
	    }
	}
      else if (file_hdr->c_nlink > 1
	       && archive_format != arf_tar
	       && archive_format != arf_ustar)
	{
	  int link_res;
	  /* Debian hack: (97/1/2) This was reported by Ronald
	     F. Guilmette to the upstream maintainers. -BEM */
	  link_res = link_to_maj_min_ino (file_hdr->c_name, 
					  file_hdr->c_dev_maj,
					  file_hdr->c_dev_min,
					  file_hdr->c_ino);
	  if (link_res == 0)
	    {
	      tape_toss_input (in_file_des, file_hdr->c_filesize);
	      tape_skip_padding (in_file_des, file_hdr->c_filesize);
	      return;
	    }
	}
      else if ((archive_format == arf_tar || archive_format == arf_ustar)
	       && file_hdr->c_tar_linkname
	       && file_hdr->c_tar_linkname[0] != '\0')
	{
	  int	link_res;
	  link_res = link_to_name (file_hdr->c_name, file_hdr->c_tar_linkname);
	  if (link_res < 0)
	    {
	      error (0, errno, _("cannot link %s to %s"),
		     file_hdr->c_tar_linkname, file_hdr->c_name);
	    }
	  return;
	}
    
      /* If not linked, copy the contents of the file.  */
      out_file_des = open (file_hdr->c_name,
			   O_CREAT | O_WRONLY | O_BINARY, 0600);
  
      if (out_file_des < 0 && create_dir_flag)
	{
	  create_all_directories (file_hdr->c_name);
	  out_file_des = open (file_hdr->c_name,
			       O_CREAT | O_WRONLY | O_BINARY,
			       0600);
	}
      
      if (out_file_des < 0)
	{
	  error (0, errno, "%s", file_hdr->c_name);
	  tape_toss_input (in_file_des, file_hdr->c_filesize);
	  tape_skip_padding (in_file_des, file_hdr->c_filesize);
	  return;
	}
    }
  
  crc = 0;
  if (swap_halfwords_flag)
    {
      if ((file_hdr->c_filesize % 4) == 0)
	swapping_halfwords = true;
      else
	error (0, 0, _("cannot swap halfwords of %s: odd number of halfwords"),
	       file_hdr->c_name);
    }
  if (swap_bytes_flag)
    {
      if ((file_hdr->c_filesize % 2) == 0)
	swapping_bytes = true;
      else
	error (0, 0, _("cannot swap bytes of %s: odd number of bytes"),
	       file_hdr->c_name);
    }
  copy_files_tape_to_disk (in_file_des, out_file_des, file_hdr->c_filesize);
  disk_empty_output_buffer (out_file_des);
  
  if (to_stdout_option)
    {
      if (archive_format == arf_crcascii)
	{
	  if (crc != file_hdr->c_chksum)
	    error (0, 0, _("%s: checksum error (0x%x, should be 0x%x)"),
		   file_hdr->c_name, crc, file_hdr->c_chksum);
	}
      tape_skip_padding (in_file_des, file_hdr->c_filesize);
      return;
    }
      
  /* Debian hack to fix a bug in the --sparse option.
     This bug has been reported to
     "bug-gnu-utils@prep.ai.mit.edu".  (96/7/10) -BEM */
  if (delayed_seek_count > 0)
    {
      lseek (out_file_des, delayed_seek_count-1, SEEK_CUR);
      write (out_file_des, "", 1);
      delayed_seek_count = 0;
    }
    
  /*
   *  Avoid race condition.
   *  Set chown and chmod before closing the file desc.
   *  pvrabec@redhat.com
   */
   
  /* File is now copied; set attributes.  */
  if (!no_chown_flag)
    if ((fchown (out_file_des,
		set_owner_flag ? set_owner : file_hdr->c_uid,
	   set_group_flag ? set_group : file_hdr->c_gid) < 0)
	&& errno != EPERM)
      error (0, errno, "%s", file_hdr->c_name);
  
  /* chown may have turned off some permissions we wanted. */
  if (fchmod (out_file_des, (int) file_hdr->c_mode) < 0)
    error (0, errno, "%s", file_hdr->c_name);
     
  if (close (out_file_des) < 0)
    error (0, errno, "%s", file_hdr->c_name);

  if (archive_format == arf_crcascii)
    {
      if (crc != file_hdr->c_chksum)
	error (0, 0, _("%s: checksum error (0x%x, should be 0x%x)"),
	       file_hdr->c_name, crc, file_hdr->c_chksum);
    }

  if (retain_time_flag)
    {
      struct utimbuf times;		/* For setting file times.  */
      /* Initialize this in case it has members we don't know to set.  */
      bzero (&times, sizeof (struct utimbuf));

      times.actime = times.modtime = file_hdr->c_mtime;
      if (utime (file_hdr->c_name, &times) < 0)
	error (0, errno, "%s", file_hdr->c_name);
    }
    
  tape_skip_padding (in_file_des, file_hdr->c_filesize);
  if (file_hdr->c_nlink > 1
      && (archive_format == arf_newascii || archive_format == arf_crcascii) )
    {
      /* (see comment above for how the newc and crc formats 
	 store multiple links).  Now that we have the data 
	 for this file, create any other links to it which
	 we defered.  */
      create_defered_links (file_hdr);
    }
}

static void
copyin_directory(struct new_cpio_header* file_hdr, int existing_dir)
{
  int res;			/* Result of various function calls.  */
#ifdef HPUX_CDF
  int cdf_flag;                 /* True if file is a CDF.  */
  int cdf_char;                 /* Index of `+' char indicating a CDF.  */
#endif

  if (to_stdout_option)
    return;
  
  /* Strip any trailing `/'s off the filename; tar puts
     them on.  We might as well do it here in case anybody
     else does too, since they cause strange things to happen.  */
  strip_trailing_slashes (file_hdr->c_name);

  /* Ignore the current directory.  It must already exist,
     and we don't want to change its permission, ownership
     or time.  */
  if (file_hdr->c_name[0] == '.' && file_hdr->c_name[1] == '\0')
    {
      return;
    }

#ifdef HPUX_CDF
  cdf_flag = 0;
#endif
  if (!existing_dir)

    {
#ifdef HPUX_CDF
      /* If the directory name ends in a + and is SUID,
	 then it is a CDF.  Strip the trailing + from
	 the name before creating it.  */
      cdf_char = strlen (file_hdr->c_name) - 1;
      if ( (cdf_char > 0) &&
	   (file_hdr->c_mode & 04000) && 
	   (file_hdr->c_name [cdf_char] == '+') )
	{
	  file_hdr->c_name [cdf_char] = '\0';
	  cdf_flag = 1;
	}
#endif
      res = mkdir (file_hdr->c_name, file_hdr->c_mode);
    }
  else
    res = 0;
  if (res < 0 && create_dir_flag)
    {
      create_all_directories (file_hdr->c_name);
      res = mkdir (file_hdr->c_name, file_hdr->c_mode);
    }
  if (res < 0)
    {
      /* In some odd cases where the file_hdr->c_name includes `.',
	 the directory may have actually been created by
	 create_all_directories(), so the mkdir will fail
	 because the directory exists.  If that's the case,
	 don't complain about it.  */
      struct stat file_stat;
      if ( (errno != EEXIST) ||
	   (lstat (file_hdr->c_name, &file_stat) != 0) ||
	   !(S_ISDIR (file_stat.st_mode) ) )
	{
	  error (0, errno, "%s", file_hdr->c_name);
	  return;
	}
    }
  if (!no_chown_flag)
    if ((chown (file_hdr->c_name,
		set_owner_flag ? set_owner : file_hdr->c_uid,
		set_group_flag ? set_group : file_hdr->c_gid) < 0)
	&& errno != EPERM)
      error (0, errno, "%s", file_hdr->c_name);
  /* chown may have turned off some permissions we wanted. */
  if (chmod (file_hdr->c_name, (int) file_hdr->c_mode) < 0)
    error (0, errno, "%s", file_hdr->c_name);
#ifdef HPUX_CDF
  if (cdf_flag)
    /* Once we "hide" the directory with the chmod(),
       we have to refer to it using name+ instead of name.  */
    file_hdr->c_name [cdf_char] = '+';
#endif
  if (retain_time_flag)
    {
      struct utimbuf times;		/* For setting file times.  */
      /* Initialize this in case it has members we don't know to set.  */
      bzero (&times, sizeof (struct utimbuf));

      times.actime = times.modtime = file_hdr->c_mtime;
      if (utime (file_hdr->c_name, &times) < 0)
	error (0, errno, "%s", file_hdr->c_name);
    }
}

static void
copyin_device(struct new_cpio_header* file_hdr)
{
  int res;			/* Result of various function calls.  */

  if (to_stdout_option)
    return;

  if (file_hdr->c_nlink > 1 && archive_format != arf_tar
      && archive_format != arf_ustar)
    {
      int link_res;
      /* Debian hack:  This was reported by Horst
	 Knobloch. This bug has been reported to
	 "bug-gnu-utils@prep.ai.mit.edu". (99/1/6) -BEM */
      link_res = link_to_maj_min_ino (file_hdr->c_name, 
		    file_hdr->c_dev_maj, file_hdr->c_dev_min,
		    file_hdr->c_ino);
      if (link_res == 0)
	{
	  return;
	}
    }
  else if (archive_format == arf_ustar &&
	   file_hdr->c_tar_linkname && 
	   file_hdr->c_tar_linkname [0] != '\0')
    {
      int	link_res;
      link_res = link_to_name (file_hdr->c_name,
			       file_hdr->c_tar_linkname);
      if (link_res < 0)
	{
	  error (0, errno, _("cannot link %s to %s"),
		 file_hdr->c_tar_linkname, file_hdr->c_name);
	  /* Something must be wrong, because we couldn't
	     find the file to link to.  But can we assume
	     that the device maj/min numbers are correct
	     and fall through to the mknod?  It's probably
	     safer to just return, rather than possibly
	     creating a bogus device file.  */
	}
      return;
    }
  
#ifdef CP_IFIFO
  if ((file_hdr->c_mode & CP_IFMT) == CP_IFIFO)
    res = mkfifo (file_hdr->c_name, file_hdr->c_mode);
  else
#endif
    res = mknod (file_hdr->c_name, file_hdr->c_mode,
	      makedev (file_hdr->c_rdev_maj, file_hdr->c_rdev_min));
  if (res < 0 && create_dir_flag)
    {
      create_all_directories (file_hdr->c_name);
#ifdef CP_IFIFO
      if ((file_hdr->c_mode & CP_IFMT) == CP_IFIFO)
	res = mkfifo (file_hdr->c_name, file_hdr->c_mode);
      else
#endif
	res = mknod (file_hdr->c_name, file_hdr->c_mode,
	      makedev (file_hdr->c_rdev_maj, file_hdr->c_rdev_min));
    }
  if (res < 0)
    {
      error (0, errno, "%s", file_hdr->c_name);
      return;
    }
  if (!no_chown_flag)
    if ((chown (file_hdr->c_name,
		set_owner_flag ? set_owner : file_hdr->c_uid,
		set_group_flag ? set_group : file_hdr->c_gid) < 0)
	&& errno != EPERM)
      error (0, errno, "%s", file_hdr->c_name);
  /* chown may have turned off some permissions we wanted. */
  if (chmod (file_hdr->c_name, file_hdr->c_mode) < 0)
    error (0, errno, "%s", file_hdr->c_name);
  if (retain_time_flag)
    {
      struct utimbuf times;		/* For setting file times.  */
      /* Initialize this in case it has members we don't know to set.  */
      bzero (&times, sizeof (struct utimbuf));

      times.actime = times.modtime = file_hdr->c_mtime;
      if (utime (file_hdr->c_name, &times) < 0)
	error (0, errno, "%s", file_hdr->c_name);
    }
}

static void
copyin_link(struct new_cpio_header *file_hdr, int in_file_des)
{
  char *link_name = NULL;	/* Name of hard and symbolic links.  */
  int res;			/* Result of various function calls.  */

  if (to_stdout_option)
    return;

  if (archive_format != arf_tar && archive_format != arf_ustar)
    {
      link_name = (char *) xmalloc ((unsigned int) file_hdr->c_filesize + 1);
      link_name[file_hdr->c_filesize] = '\0';
      tape_buffered_read (link_name, in_file_des, file_hdr->c_filesize);
      tape_skip_padding (in_file_des, file_hdr->c_filesize);
    }
  else
    {
      link_name = xstrdup (file_hdr->c_tar_linkname);
    }

  res = UMASKED_SYMLINK (link_name, file_hdr->c_name,
			 file_hdr->c_mode);
  if (res < 0 && create_dir_flag)
    {
      create_all_directories (file_hdr->c_name);
      res = UMASKED_SYMLINK (link_name, file_hdr->c_name,
			     file_hdr->c_mode);
    }
  if (res < 0)
    {
      error (0, errno, "%s", file_hdr->c_name);
      free (link_name);
      return;
    }
  if (!no_chown_flag)
    if ((lchown (file_hdr->c_name,
		 set_owner_flag ? set_owner : file_hdr->c_uid,
	     set_group_flag ? set_group : file_hdr->c_gid) < 0)
	&& errno != EPERM)
      {
	error (0, errno, "%s", file_hdr->c_name);
      }
  free (link_name);
}

static void
copyin_file (struct new_cpio_header* file_hdr, int in_file_des)
{
  int existing_dir;

  if (!to_stdout_option
      && try_existing_file (file_hdr, in_file_des, &existing_dir) < 0)
    return;

  /* Do the real copy or link.  */
  switch (file_hdr->c_mode & CP_IFMT)
    {
    case CP_IFREG:
      copyin_regular_file(file_hdr, in_file_des);
      break;

    case CP_IFDIR:
      copyin_directory(file_hdr, existing_dir);
      break;

    case CP_IFCHR:
    case CP_IFBLK:
#ifdef CP_IFSOCK
    case CP_IFSOCK:
#endif
#ifdef CP_IFIFO
    case CP_IFIFO:
#endif
      copyin_device(file_hdr);
      break;

#ifdef CP_IFLNK
    case CP_IFLNK:
      copyin_link(file_hdr, in_file_des);
      break;
#endif

    default:
      error (0, 0, _("%s: unknown file type"), file_hdr->c_name);
      tape_toss_input (in_file_des, file_hdr->c_filesize);
      tape_skip_padding (in_file_des, file_hdr->c_filesize);
    }
}


/* Current time for verbose table.  */
static time_t current_time;


/* Print the file described by FILE_HDR in long format.
   If LINK_NAME is nonzero, it is the name of the file that
   this file is a symbolic link to.  */

void
long_format (struct new_cpio_header *file_hdr, char *link_name)
{
  char mbuf[11];
  char tbuf[40];
  time_t when;
  char *ptbuf;
  static int d_first = -1;

  mode_string (file_hdr->c_mode, mbuf);
  mbuf[10] = '\0';

  /* Get time values ready to print.  */
  when = file_hdr->c_mtime;
  if (d_first < 0)
    d_first = (*nl_langinfo(D_MD_ORDER) == 'd');
  if (current_time - when > 6L * 30L * 24L * 60L * 60L
      || current_time - when < 0L)
	ptbuf = d_first ? "%e %b  %Y" : "%b %e  %Y";
  else
	ptbuf = d_first ? "%e %b %R" : "%b %e %R";
  strftime(tbuf, sizeof(tbuf), ptbuf, localtime(&when));
  ptbuf = tbuf;

  printf ("%s %3lu ", mbuf, file_hdr->c_nlink);

  if (numeric_uid)
    printf ("%-8u %-8u ", (unsigned int) file_hdr->c_uid,
	    (unsigned int) file_hdr->c_gid);
  else
    printf ("%-8.8s %-8.8s ", getuser (file_hdr->c_uid),
	    getgroup (file_hdr->c_gid));

  if ((file_hdr->c_mode & CP_IFMT) == CP_IFCHR
      || (file_hdr->c_mode & CP_IFMT) == CP_IFBLK)
    printf ("%3lu, %3lu ", file_hdr->c_rdev_maj,
	    file_hdr->c_rdev_min);
  else
    printf ("%8lu ", file_hdr->c_filesize);

  printf ("%s ", ptbuf);

  print_name_with_quoting (file_hdr->c_name);
  if (link_name)
    {
      printf (" -> ");
      print_name_with_quoting (link_name);
    }
  putc ('\n', stdout);
}

void
print_name_with_quoting (register char *p)
{
  register unsigned char c;

  while ( (c = *p++) )
    {
      switch (c)
	{
	case '\\':
	  printf ("\\\\");
	  break;

	case '\n':
	  printf ("\\n");
	  break;

	case '\b':
	  printf ("\\b");
	  break;

	case '\r':
	  printf ("\\r");
	  break;

	case '\t':
	  printf ("\\t");
	  break;

	case '\f':
	  printf ("\\f");
	  break;

	case ' ':
	  printf ("\\ ");
	  break;

	case '"':
	  printf ("\\\"");
	  break;

	default:
	  if (isprint (c))
	    putchar (c);
	  else
	    printf ("\\%03o", (unsigned int) c);
	}
    }
}

/* Read a pattern file (for the -E option).  Put a list of
   `num_patterns' elements in `save_patterns'.  Any patterns that were
   already in `save_patterns' (from the command line) are preserved.  */

static void
read_pattern_file ()
{
  int max_new_patterns;
  char **new_save_patterns;
  int new_num_patterns;
  int i;
  dynamic_string pattern_name;
  FILE *pattern_fp;

  if (num_patterns < 0)
    num_patterns = 0;
  max_new_patterns = 1 + num_patterns;
  new_save_patterns = (char **) xmalloc (max_new_patterns * sizeof (char *));
  new_num_patterns = num_patterns;
  ds_init (&pattern_name, 128);

  pattern_fp = fopen (pattern_file_name, "r");
  if (pattern_fp == NULL)
    error (1, errno, "%s", pattern_file_name);
  while (ds_fgetstr (pattern_fp, &pattern_name, '\n') != NULL)
    {
      if (new_num_patterns >= max_new_patterns)
	{
	  max_new_patterns += 1;
	  new_save_patterns = (char **)
	    xrealloc ((char *) new_save_patterns,
		      max_new_patterns * sizeof (char *));
	}
      new_save_patterns[new_num_patterns] = xstrdup (pattern_name.ds_string);
      ++new_num_patterns;
    }
  if (ferror (pattern_fp) || fclose (pattern_fp) == EOF)
    error (1, errno, "%s", pattern_file_name);

  for (i = 0; i < num_patterns; ++i)
    new_save_patterns[i] = save_patterns[i];

  save_patterns = new_save_patterns;
  num_patterns = new_num_patterns;
}




/* Return 16-bit integer I with the bytes swapped.  */
#define swab_short(i) ((((i) << 8) & 0xff00) | (((i) >> 8) & 0x00ff))

/* Read the header, including the name of the file, from file
   descriptor IN_DES into FILE_HDR.  */

void
read_in_header (struct new_cpio_header *file_hdr, int in_des)
{
  long bytes_skipped = 0;	/* Bytes of junk found before magic number.  */

  /* Search for a valid magic number.  */

  if (archive_format == arf_unknown)
    {
      char tmpbuf[512];
      int check_tar;
      int peeked_bytes;

      while (archive_format == arf_unknown)
	{
	  peeked_bytes = tape_buffered_peek (tmpbuf, in_des, 512);
	  if (peeked_bytes < 6)
	    error (1, 0, _("premature end of archive"));

	  if (!strncmp (tmpbuf, "070701", 6))
	    archive_format = arf_newascii;
	  else if (!strncmp (tmpbuf, "070707", 6))
	    archive_format = arf_oldascii;
	  else if (!strncmp (tmpbuf, "070702", 6))
	    {
	      archive_format = arf_crcascii;
	      crc_i_flag = true;
	    }
	  else if ((*((unsigned short *) tmpbuf) == 070707) ||
		   (*((unsigned short *) tmpbuf) == swab_short ((unsigned short) 070707)))
	    archive_format = arf_binary;
	  else if (peeked_bytes >= 512
		   && (check_tar = is_tar_header (tmpbuf)))
	    {
	      if (check_tar == 2)
		archive_format = arf_ustar;
	      else
		archive_format = arf_tar;
	    }
	  else
	    {
	      tape_buffered_read ((char *) tmpbuf, in_des, 1L);
	      ++bytes_skipped;
	    }
	}
    }

  if (archive_format == arf_tar || archive_format == arf_ustar)
    {
      if (append_flag)
	last_header_start = input_bytes - io_block_size +
	  (in_buff - input_buffer);
      if (bytes_skipped > 0)
	warn_junk_bytes (bytes_skipped);

      read_in_tar_header (file_hdr, in_des);
      return;
    }

  file_hdr->c_tar_linkname = NULL;

  tape_buffered_read ((char *) file_hdr, in_des, 6L);
  while (1)
    {
      if (append_flag)
	last_header_start = input_bytes - io_block_size
	  + (in_buff - input_buffer) - 6;
      if (archive_format == arf_newascii
	  && !strncmp ((char *) file_hdr, "070701", 6))
	{
	  if (bytes_skipped > 0)
	    warn_junk_bytes (bytes_skipped);
	  read_in_new_ascii (file_hdr, in_des);
	  break;
	}
      if (archive_format == arf_crcascii
	  && !strncmp ((char *) file_hdr, "070702", 6))
	{
	  if (bytes_skipped > 0)
	    warn_junk_bytes (bytes_skipped);

	  read_in_new_ascii (file_hdr, in_des);
	  break;
	}
      if ( (archive_format == arf_oldascii || archive_format == arf_hpoldascii)
	  && !strncmp ((char *) file_hdr, "070707", 6))
	{
	  if (bytes_skipped > 0)
	    warn_junk_bytes (bytes_skipped);
	  
	  read_in_old_ascii (file_hdr, in_des);
	  break;
	}
      if ( (archive_format == arf_binary || archive_format == arf_hpbinary)
	  && (file_hdr->c_magic == 070707
	      || file_hdr->c_magic == swab_short ((unsigned short) 070707)))
	{
	  /* Having to skip 1 byte because of word alignment is normal.  */
	  if (bytes_skipped > 0)
	    warn_junk_bytes (bytes_skipped);
	  
	  read_in_binary (file_hdr, in_des);
	  break;
	}
      bytes_skipped++;
      bcopy ((char *) file_hdr + 1, (char *) file_hdr, 5);
      tape_buffered_read ((char *) file_hdr + 5, in_des, 1L);
    }
}

/* Fill in FILE_HDR by reading an old-format ASCII format cpio header from
   file descriptor IN_DES, except for the magic number, which is
   already filled in.  */

void
read_in_old_ascii (struct new_cpio_header *file_hdr, int in_des)
{
  char ascii_header[78];
  unsigned long dev;
  unsigned long rdev;

  tape_buffered_read (ascii_header, in_des, 70L);
  ascii_header[70] = '\0';
  sscanf (ascii_header,
	  "%6lo%6lo%6lo%6lo%6lo%6lo%6lo%11lo%6lo%11lo",
	  &dev, &file_hdr->c_ino,
	  &file_hdr->c_mode, &file_hdr->c_uid, &file_hdr->c_gid,
	  &file_hdr->c_nlink, &rdev, &file_hdr->c_mtime,
	  &file_hdr->c_namesize, &file_hdr->c_filesize);
  file_hdr->c_dev_maj = major (dev);
  file_hdr->c_dev_min = minor (dev);
  file_hdr->c_rdev_maj = major (rdev);
  file_hdr->c_rdev_min = minor (rdev);

  /* Read file name from input.  */
  if (file_hdr->c_name != NULL)
    free (file_hdr->c_name);
  file_hdr->c_name = (char *) xmalloc (file_hdr->c_namesize + 1);
  tape_buffered_read (file_hdr->c_name, in_des, (long) file_hdr->c_namesize);

  /* HP/UX cpio creates archives that look just like ordinary archives,
     but for devices it sets major = 0, minor = 1, and puts the
     actual major/minor number in the filesize field.  See if this
     is an HP/UX cpio archive, and if so fix it.  We have to do this
     here because process_copy_in() assumes filesize is always 0
     for devices.  */
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
	if (file_hdr->c_filesize != 0
	    && file_hdr->c_rdev_maj == 0
	    && file_hdr->c_rdev_min == 1)
	  {
	    file_hdr->c_rdev_maj = major (file_hdr->c_filesize);
	    file_hdr->c_rdev_min = minor (file_hdr->c_filesize);
	    file_hdr->c_filesize = 0;
	  }
	break;
      default:
	break;
    }
}

/* Fill in FILE_HDR by reading a new-format ASCII format cpio header from
   file descriptor IN_DES, except for the magic number, which is
   already filled in.  */

void
read_in_new_ascii (struct new_cpio_header *file_hdr, int in_des)
{
  char ascii_header[112];

  tape_buffered_read (ascii_header, in_des, 104L);
  ascii_header[104] = '\0';
  sscanf (ascii_header,
	  "%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx",
	  &file_hdr->c_ino, &file_hdr->c_mode, &file_hdr->c_uid,
	  &file_hdr->c_gid, &file_hdr->c_nlink, &file_hdr->c_mtime,
	  &file_hdr->c_filesize, &file_hdr->c_dev_maj, &file_hdr->c_dev_min,
	&file_hdr->c_rdev_maj, &file_hdr->c_rdev_min, &file_hdr->c_namesize,
	  &file_hdr->c_chksum);
  /* Read file name from input.  */
  if (file_hdr->c_name != NULL)
    free (file_hdr->c_name);
  file_hdr->c_name = (char *) xmalloc (file_hdr->c_namesize);
  tape_buffered_read (file_hdr->c_name, in_des, (long) file_hdr->c_namesize);

  /* In SVR4 ASCII format, the amount of space allocated for the header
     is rounded up to the next long-word, so we might need to drop
     1-3 bytes.  */
  tape_skip_padding (in_des, file_hdr->c_namesize + 110);
}

/* Fill in FILE_HDR by reading a binary format cpio header from
   file descriptor IN_DES, except for the first 6 bytes (the magic
   number, device, and inode number), which are already filled in.  */

void
read_in_binary (struct new_cpio_header *file_hdr, int in_des)
{
  struct old_cpio_header short_hdr;

  /* Copy the data into the short header, then later transfer
     it into the argument long header.  */
  short_hdr.c_dev = ((struct old_cpio_header *) file_hdr)->c_dev;
  short_hdr.c_ino = ((struct old_cpio_header *) file_hdr)->c_ino;
  tape_buffered_read (((char *) &short_hdr) + 6, in_des, 20L);

  /* If the magic number is byte swapped, fix the header.  */
  if (file_hdr->c_magic == swab_short ((unsigned short) 070707))
    {
      static int warned = 0;

      /* Alert the user that they might have to do byte swapping on
	 the file contents.  */
      if (warned == 0)
	{
	  error (0, 0, _("warning: archive header has reverse byte-order"));
	  warned = 1;
	}
      swab_array ((char *) &short_hdr, 13);
    }

  file_hdr->c_dev_maj = major (short_hdr.c_dev);
  file_hdr->c_dev_min = minor (short_hdr.c_dev);
  file_hdr->c_ino = short_hdr.c_ino;
  file_hdr->c_mode = short_hdr.c_mode;
  file_hdr->c_uid = short_hdr.c_uid;
  file_hdr->c_gid = short_hdr.c_gid;
  file_hdr->c_nlink = short_hdr.c_nlink;
  file_hdr->c_rdev_maj = major (short_hdr.c_rdev);
  file_hdr->c_rdev_min = minor (short_hdr.c_rdev);
  file_hdr->c_mtime = (unsigned long) short_hdr.c_mtimes[0] << 16
    | short_hdr.c_mtimes[1];

  file_hdr->c_namesize = short_hdr.c_namesize;
  file_hdr->c_filesize = (unsigned long) short_hdr.c_filesizes[0] << 16
    | short_hdr.c_filesizes[1];

  /* Read file name from input.  */
  if (file_hdr->c_name != NULL)
    free (file_hdr->c_name);
  file_hdr->c_name = (char *) xmalloc (file_hdr->c_namesize);
  tape_buffered_read (file_hdr->c_name, in_des, (long) file_hdr->c_namesize);

  /* In binary mode, the amount of space allocated in the header for
     the filename is `c_namesize' rounded up to the next short-word,
     so we might need to drop a byte.  */
  if (file_hdr->c_namesize % 2)
    tape_toss_input (in_des, 1L);

  /* HP/UX cpio creates archives that look just like ordinary archives,
     but for devices it sets major = 0, minor = 1, and puts the
     actual major/minor number in the filesize field.  See if this
     is an HP/UX cpio archive, and if so fix it.  We have to do this
     here because process_copy_in() assumes filesize is always 0
     for devices.  */
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
	if (file_hdr->c_filesize != 0
	    && file_hdr->c_rdev_maj == 0
	    && file_hdr->c_rdev_min == 1)
	  {
	    file_hdr->c_rdev_maj = major (file_hdr->c_filesize);
	    file_hdr->c_rdev_min = minor (file_hdr->c_filesize);
	    file_hdr->c_filesize = 0;
	  }
	break;
      default:
	break;
    }
}

/* Exchange the bytes of each element of the array of COUNT shorts
   starting at PTR.  */

void
swab_array (char *ptr, int count)
{
  char tmp;

  while (count-- > 0)
    {
      tmp = *ptr;
      *ptr = *(ptr + 1);
      ++ptr;
      *ptr = tmp;
      ++ptr;
    }
}

/* Return a safer suffix of FILE_NAME, or "." if it has no safer
   suffix.  Check for fully specified file names and other atrocities.  */

static const char *
safer_name_suffix (char const *file_name)
{
  char const *p;

  /* Skip file system prefixes, leading file name components that contain
     "..", and leading slashes.  */

  size_t prefix_len = FILE_SYSTEM_PREFIX_LEN (file_name);

  for (p = file_name + prefix_len; *p;)
    {
      if (p[0] == '.' && p[1] == '.' && (ISSLASH (p[2]) || !p[2]))
	prefix_len = p + 2 - file_name;

      do
	{
	  char c = *p++;
	  if (ISSLASH (c))
	    break;
	}
      while (*p);
    }

  for (p = file_name + prefix_len; ISSLASH (*p); p++)
    continue;
  prefix_len = p - file_name;

  if (prefix_len)
    {
      char *prefix = alloca (prefix_len + 1);
      memcpy (prefix, file_name, prefix_len);
      prefix[prefix_len] = '\0';


      error (0, 0, _("Removing leading `%s' from member names"), prefix);
    }

  if (!*p)
    p = ".";

  return p;
}

/* Read the collection from standard input and create files
   in the file system.  */

void
process_copy_in ()
{
  char done = false;		/* True if trailer reached.  */
  FILE *tty_in;			/* Interactive file for rename option.  */
  FILE *tty_out;		/* Interactive file for rename option.  */
  FILE *rename_in;		/* Batch file for rename option.  */
  struct stat file_stat;	/* Output file stat record.  */
  struct new_cpio_header file_hdr;	/* Output header information.  */
  int in_file_des;		/* Input file descriptor.  */
  char skip_file;		/* Flag for use with patterns.  */
  int i;			/* Loop index variable.  */

  /* Initialize the copy in.  */
  if (pattern_file_name)
    {
      read_pattern_file ();
    }
  file_hdr.c_name = NULL;

  if (rename_batch_file)
    {
      rename_in = fopen (rename_batch_file, "r");
      if (rename_in == NULL)
	{
	  error (2, errno, TTY_NAME);
	}
    }
  else if (rename_flag)
    {
      /* Open interactive file pair for rename operation.  */
      tty_in = fopen (TTY_NAME, "r");
      if (tty_in == NULL)
	{
	  error (2, errno, TTY_NAME);
	}
      tty_out = fopen (TTY_NAME, "w");
      if (tty_out == NULL)
	{
	  error (2, errno, TTY_NAME);
	}
    }

  /* Get date and time if needed for processing the table option.  */
  if (table_flag && verbose_flag)
    {
      time (&current_time);
    }

  /* Check whether the input file might be a tape.  */
  in_file_des = archive_des;
  if (_isrmt (in_file_des))
    {
      input_is_special = 1;
      input_is_seekable = 0;
    }
  else
    {
      if (fstat (in_file_des, &file_stat))
	error (1, errno, _("standard input is closed"));
      input_is_special =
#ifdef S_ISBLK
	S_ISBLK (file_stat.st_mode) ||
#endif
	S_ISCHR (file_stat.st_mode);
      input_is_seekable = S_ISREG (file_stat.st_mode);
    }
  output_is_seekable = true;

  /* While there is more input in the collection, process the input.  */
  while (!done)
    {
      swapping_halfwords = swapping_bytes = false;

      /* Start processing the next file by reading the header.  */
      read_in_header (&file_hdr, in_file_des);

#ifdef DEBUG_CPIO
      if (debug_flag)
	{
	  struct new_cpio_header *h;
	  h = &file_hdr;
	  fprintf (stderr, 
		"magic = 0%o, ino = %d, mode = 0%o, uid = %d, gid = %d\n",
		h->c_magic, h->c_ino, h->c_mode, h->c_uid, h->c_gid);
	  fprintf (stderr, 
		"nlink = %d, mtime = %d, filesize = %d, dev_maj = 0x%x\n",
		h->c_nlink, h->c_mtime, h->c_filesize, h->c_dev_maj);
	  fprintf (stderr, 
	        "dev_min = 0x%x, rdev_maj = 0x%x, rdev_min = 0x%x, namesize = %d\n",
		h->c_dev_min, h->c_rdev_maj, h->c_rdev_min, h->c_namesize);
	  fprintf (stderr, 
		"chksum = %d, name = \"%s\", tar_linkname = \"%s\"\n",
		h->c_chksum, h->c_name, 
		h->c_tar_linkname ? h->c_tar_linkname : "(null)" );

	}
#endif
      /* Is this the header for the TRAILER file?  */
      if (strcmp ("TRAILER!!!", file_hdr.c_name) == 0)
	{
	  done = true;
	  break;
	}

      /* Do we have to ignore absolute paths, and if so, does the filename
         have an absolute path?  */
      if (!abs_paths_flag && file_hdr.c_name && file_hdr.c_name [0])
	{
	  const char *p = safer_name_suffix (file_hdr.c_name);

	  if (p != file_hdr.c_name)
	    {
              /* Debian hack: file_hrd.c_name is sometimes set to
                 point to static memory by code in tar.c.  This
                 causes a segfault.  Therefore, memmove is used
                 instead of freeing and reallocating.  (Reported by
                 Horst Knobloch.)  This bug has been reported to
                 "bug-gnu-utils@prep.ai.mit.edu". (99/1/6) -BEM */
	      (void)memmove (file_hdr.c_name, p, (size_t)(strlen (p) + 1));
	    }
	}

      /* Does the file name match one of the given patterns?  */
      if (num_patterns <= 0)
	skip_file = false;
      else
	{
	  skip_file = copy_matching_files;
	  for (i = 0; i < num_patterns
	       && skip_file == copy_matching_files; i++)
	    {
	      if (fnmatch (save_patterns[i], file_hdr.c_name, 0) == 0)
		skip_file = !copy_matching_files;
	    }
	}

      if (skip_file)
	{
	  /* If we're skipping a file with links, there might be other
	     links that we didn't skip, and this file might have the
	     data for the links.  If it does, we'll copy in the data
	     to the links, but not to this file.  */
	  if (file_hdr.c_nlink > 1 && (archive_format == arf_newascii
	      || archive_format == arf_crcascii) )
	    {
	      if (create_defered_links_to_skipped(&file_hdr, in_file_des) < 0)
	        {
		  tape_toss_input (in_file_des, file_hdr.c_filesize);
		  tape_skip_padding (in_file_des, file_hdr.c_filesize);
		}
	    }
	  else
	    {
	      tape_toss_input (in_file_des, file_hdr.c_filesize);
	      tape_skip_padding (in_file_des, file_hdr.c_filesize);
	    }
	}
      else if (table_flag)
	{
	  list_file(&file_hdr, in_file_des);
	}
      else if (append_flag)
	{
	  tape_toss_input (in_file_des, file_hdr.c_filesize);
	  tape_skip_padding (in_file_des, file_hdr.c_filesize);
	}
      else if (only_verify_crc_flag)
	{
#ifdef CP_IFLNK
	  if ((file_hdr.c_mode & CP_IFMT) == CP_IFLNK)
	    {
	      if (archive_format != arf_tar && archive_format != arf_ustar)
		{
		  tape_toss_input (in_file_des, file_hdr.c_filesize);
		  tape_skip_padding (in_file_des, file_hdr.c_filesize);
		  continue;
		}
	    }
#endif
	    crc = 0;
	    tape_toss_input (in_file_des, file_hdr.c_filesize);
	    tape_skip_padding (in_file_des, file_hdr.c_filesize);
	    if (crc != file_hdr.c_chksum)
	      {
		error (0, 0, _("%s: checksum error (0x%x, should be 0x%x)"),
		       file_hdr.c_name, crc, file_hdr.c_chksum);
	      }
         /* Debian hack: -v and -V now work with --only-verify-crc.
            (99/11/10) -BEM */
	    if (verbose_flag)
	      {
		fprintf (stderr, "%s\n", file_hdr.c_name);
	      }
	    if (dot_flag)
	      {
		fputc ('.', stderr);
	      }
	}
      else
	{
	  /* Copy the input file into the directory structure.  */

	  /* Do we need to rename the file? */
	  if (rename_flag || rename_batch_file)
	    {
	      if (query_rename(&file_hdr, tty_in, tty_out, rename_in) < 0)
	        {
		  tape_toss_input (in_file_des, file_hdr.c_filesize);
		  tape_skip_padding (in_file_des, file_hdr.c_filesize);
		  continue;
		}
	    }

	  copyin_file(&file_hdr, in_file_des);

	  if (verbose_flag)
	    fprintf (stderr, "%s\n", file_hdr.c_name);
	  if (dot_flag)
	    fputc ('.', stderr);
	}
    }

  if (dot_flag)
    fputc ('\n', stderr);

  if (append_flag)
    return;

  if (archive_format == arf_newascii || archive_format == arf_crcascii)
    {
      create_final_defers ();
    }
  if (!quiet_flag)
    {
      int blocks;
      blocks = (input_bytes + io_block_size - 1) / io_block_size;
      fprintf (stderr, ngettext ("%d block\n", "%d blocks\n", blocks), blocks);
    }
}

