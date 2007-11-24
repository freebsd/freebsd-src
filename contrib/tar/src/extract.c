/* Extract files from a tar archive.

   Copyright 1988, 1992, 1993, 1994, 1996, 1997, 1998, 1999, 2000,
   2001 Free Software Foundation, Inc.

   Written by John Gilmore, on 1985-11-19.

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
#include <quotearg.h>

#if HAVE_UTIME_H
# include <utime.h>
#else
struct utimbuf
  {
    long actime;
    long modtime;
  };
#endif

#include "common.h"

int we_are_root;		/* true if our effective uid == 0 */
static mode_t newdir_umask;	/* umask when creating new directories */
static mode_t current_umask;	/* current umask (which is set to 0 if -p) */

/* Status of the permissions of a file that we are extracting.  */
enum permstatus
{
  /* This file may have existed already; its permissions are unknown.  */
  UNKNOWN_PERMSTATUS,

  /* This file was created using the permissions from the archive.  */
  ARCHIVED_PERMSTATUS,

  /* This is an intermediate directory; the archive did not specify
     its permissions.  */
  INTERDIR_PERMSTATUS
};

/* List of directories whose statuses we need to extract after we've
   finished extracting their subsidiary files.  If you consider each
   contiguous subsequence of elements of the form [D]?[^D]*, where [D]
   represents an element where AFTER_SYMLINKS is nonzero and [^D]
   represents an element where AFTER_SYMLINKS is zero, then the head
   of the subsequence has the longest name, and each non-head element
   in the prefix is an ancestor (in the directory hierarchy) of the
   preceding element.  */

struct delayed_set_stat
  {
    struct delayed_set_stat *next;
    struct stat stat_info;
    size_t file_name_len;
    mode_t invert_permissions;
    enum permstatus permstatus;
    bool after_symlinks;
    char file_name[1];
  };

static struct delayed_set_stat *delayed_set_stat_head;

/* List of symbolic links whose creation we have delayed.  */
struct delayed_symlink
  {
    /* The next delayed symbolic link in the list.  */
    struct delayed_symlink *next;

    /* The device, inode number and last-modified time of the placeholder.  */
    dev_t dev;
    ino_t ino;
    time_t mtime;

    /* The desired owner and group of the symbolic link.  */
    uid_t uid;
    gid_t gid;

    /* A list of sources for this symlink.  The sources are all to be
       hard-linked together.  */
    struct string_list *sources;

    /* The desired target of the desired link.  */
    char target[1];
  };

static struct delayed_symlink *delayed_symlink_head;

struct string_list
  {
    struct string_list *next;
    char string[1];
  };

/*  Set up to extract files.  */
void
extr_init (void)
{
  we_are_root = geteuid () == 0;
#ifndef __FreeBSD__
  same_permissions_option += we_are_root;
#endif
  same_owner_option += we_are_root;
  xalloc_fail_func = extract_finish;

  /* Option -p clears the kernel umask, so it does not affect proper
     restoration of file permissions.  New intermediate directories will
     comply with umask at start of program.  */

  newdir_umask = umask (0);
  if (0 < same_permissions_option)
    current_umask = 0;
  else
    {
      umask (newdir_umask);	/* restore the kernel umask */
      current_umask = newdir_umask;
#ifdef __FreeBSD__
      same_permissions_option++;
#endif
    }
}

/* If restoring permissions, restore the mode for FILE_NAME from
   information given in *STAT_INFO (where *CURRENT_STAT_INFO gives
   the current status if CURRENT_STAT_INFO is nonzero); otherwise invert the
   INVERT_PERMISSIONS bits from the file's current permissions.
   PERMSTATUS specifies the status of the file's permissions.
   TYPEFLAG specifies the type of the file.  */
static void
set_mode (char const *file_name, struct stat const *stat_info,
	  struct stat const *current_stat_info,
	  mode_t invert_permissions, enum permstatus permstatus,
	  char typeflag)
{
  mode_t mode;

  if (0 < same_permissions_option
      && permstatus != INTERDIR_PERMSTATUS)
    {
      mode = stat_info->st_mode;

      /* If we created the file and it has a usual mode, then its mode
	 is normally set correctly already.  But on many hosts, some
	 directories inherit the setgid bits from their parents, so we
	 we must set directories' modes explicitly.  */
      if (permstatus == ARCHIVED_PERMSTATUS
	  && ! (mode & ~ MODE_RWX)
	  && typeflag != DIRTYPE
	  && typeflag != GNUTYPE_DUMPDIR)
	return;
    }
  else if (! invert_permissions)
    return;
  else
    {
      /* We must inspect a directory's current permissions, since the
	 directory may have inherited its setgid bit from its parent.

	 INVERT_PERMISSIONS happens to be nonzero only for directories
	 that we created, so there's no point optimizing this code for
	 other cases.  */
      struct stat st;
      if (! current_stat_info)
	{
	  if (stat (file_name, &st) != 0)
	    {
	      stat_error (file_name);
	      return;
	    }
	  current_stat_info = &st;
	}
      mode = current_stat_info->st_mode ^ invert_permissions;
    }

  if (chmod (file_name, mode & ~ current_umask) != 0)
    chmod_error_details (file_name, mode);
}

/* Check time after successfully setting FILE_NAME's time stamp to T.  */
static void
check_time (char const *file_name, time_t t)
{
  time_t now;
  if (start_time < t && (now = time (0)) < t)
    WARN ((0, 0, _("%s: time stamp %s is %lu s in the future"),
	   file_name, tartime (t), (unsigned long) (t - now)));
}

/* Restore stat attributes (owner, group, mode and times) for
   FILE_NAME, using information given in *STAT_INFO.
   If CURRENT_STAT_INFO is nonzero, *CURRENT_STAT_INFO is the
   file's currernt status.
   If not restoring permissions, invert the
   INVERT_PERMISSIONS bits from the file's current permissions.
   PERMSTATUS specifies the status of the file's permissions.
   TYPEFLAG specifies the type of the file.  */

/* FIXME: About proper restoration of symbolic link attributes, we still do
   not have it right.  Pretesters' reports tell us we need further study and
   probably more configuration.  For now, just use lchown if it exists, and
   punt for the rest.  Sigh!  */

static void
set_stat (char const *file_name, struct stat const *stat_info,
	  struct stat const *current_stat_info,
	  mode_t invert_permissions, enum permstatus permstatus,
	  char typeflag)
{
  struct utimbuf utimbuf;

  if (typeflag != SYMTYPE)
    {
      /* We do the utime before the chmod because some versions of utime are
	 broken and trash the modes of the file.  */

      if (! touch_option && permstatus != INTERDIR_PERMSTATUS)
	{
	  /* We set the accessed time to `now', which is really the time we
	     started extracting files, unless incremental_option is used, in
	     which case .st_atime is used.  */

	  /* FIXME: incremental_option should set ctime too, but how?  */

	  if (incremental_option)
	    utimbuf.actime = stat_info->st_atime;
	  else
	    utimbuf.actime = start_time;

	  utimbuf.modtime = stat_info->st_mtime;

	  if (utime (file_name, &utimbuf) < 0)
	    utime_error (file_name);
	  else
	    {
	      check_time (file_name, stat_info->st_atime);
	      check_time (file_name, stat_info->st_mtime);
	    }
	}

      /* Some systems allow non-root users to give files away.  Once this
	 done, it is not possible anymore to change file permissions, so we
	 have to set permissions prior to possibly giving files away.  */

      set_mode (file_name, stat_info, current_stat_info,
		invert_permissions, permstatus, typeflag);
    }

  if (0 < same_owner_option && permstatus != INTERDIR_PERMSTATUS)
    {
      /* When lchown exists, it should be used to change the attributes of
	 the symbolic link itself.  In this case, a mere chown would change
	 the attributes of the file the symbolic link is pointing to, and
	 should be avoided.  */

      if (typeflag == SYMTYPE)
	{
#if HAVE_LCHOWN
	  if (lchown (file_name, stat_info->st_uid, stat_info->st_gid) < 0)
	    chown_error_details (file_name,
				 stat_info->st_uid, stat_info->st_gid);
#endif
	}
      else
	{
	  if (chown (file_name, stat_info->st_uid, stat_info->st_gid) < 0)
	    chown_error_details (file_name,
				 stat_info->st_uid, stat_info->st_gid);

	  /* On a few systems, and in particular, those allowing to give files
	     away, changing the owner or group destroys the suid or sgid bits.
	     So let's attempt setting these bits once more.  */
	  if (stat_info->st_mode & (S_ISUID | S_ISGID | S_ISVTX))
	    set_mode (file_name, stat_info, 0,
		      invert_permissions, permstatus, typeflag);
	}
    }
}

/* Remember to restore stat attributes (owner, group, mode and times)
   for the directory FILE_NAME, using information given in *STAT_INFO,
   once we stop extracting files into that directory.
   If not restoring permissions, remember to invert the
   INVERT_PERMISSIONS bits from the file's current permissions.
   PERMSTATUS specifies the status of the file's permissions.  */
static void
delay_set_stat (char const *file_name, struct stat const *stat_info,
		mode_t invert_permissions, enum permstatus permstatus)
{
  size_t file_name_len = strlen (file_name);
  struct delayed_set_stat *data =
    xmalloc (offsetof (struct delayed_set_stat, file_name)
	     + file_name_len + 1);
  data->file_name_len = file_name_len;
  strcpy (data->file_name, file_name);
  data->invert_permissions = invert_permissions;
  data->permstatus = permstatus;
  data->after_symlinks = 0;
  data->stat_info = *stat_info;
  data->next = delayed_set_stat_head;
  delayed_set_stat_head = data;
}

/* Update the delayed_set_stat info for an intermediate directory
   created on the path to DIR_NAME.  The intermediate directory turned
   out to be the same as this directory, e.g. due to ".." or symbolic
   links.  *DIR_STAT_INFO is the status of the directory.  */
static void
repair_delayed_set_stat (char const *dir_name,
			 struct stat const *dir_stat_info)
{
  struct delayed_set_stat *data;
  for (data = delayed_set_stat_head;  data;  data = data->next)
    {
      struct stat st;
      if (stat (data->file_name, &st) != 0)
	{
	  stat_error (data->file_name);
	  return;
	}

      if (st.st_dev == dir_stat_info->st_dev
	  && st.st_ino == dir_stat_info->st_ino)
	{
	  data->stat_info = current_stat;
	  data->invert_permissions = (MODE_RWX
				      & (current_stat.st_mode ^ st.st_mode));
	  data->permstatus = ARCHIVED_PERMSTATUS;
	  return;
	}
    }

  ERROR ((0, 0, _("%s: Unexpected inconsistency when making directory"),
	  quotearg_colon (dir_name)));
}

/* After a file/link/symlink/directory creation has failed, see if
   it's because some required directory was not present, and if so,
   create all required directories.  Return non-zero if a directory
   was created.  */
static int
make_directories (char *file_name)
{
  char *cursor0 = file_name + FILESYSTEM_PREFIX_LEN (file_name);
  char *cursor;			/* points into path */
  int did_something = 0;	/* did we do anything yet? */
  int mode;
  int invert_permissions;
  int status;

  
  for (cursor = cursor0; *cursor; cursor++)
    {
      if (! ISSLASH (*cursor))
	continue;

      /* Avoid mkdir of empty string, if leading or double '/'.  */

      if (cursor == cursor0 || ISSLASH (cursor[-1]))
	continue;

      /* Avoid mkdir where last part of path is "." or "..".  */

      if (cursor[-1] == '.'
	  && (cursor == cursor0 + 1 || ISSLASH (cursor[-2])
	      || (cursor[-2] == '.'
		  && (cursor == cursor0 + 2 || ISSLASH (cursor[-3])))))
	continue;

      *cursor = '\0';		/* truncate the path there */
      mode = MODE_RWX & ~ newdir_umask;
      invert_permissions = we_are_root ? 0 : MODE_WXUSR & ~ mode;
      status = mkdir (file_name, mode ^ invert_permissions);

      if (status == 0)
	{
	  /* Create a struct delayed_set_stat even if
	     invert_permissions is zero, because
	     repair_delayed_set_stat may need to update the struct.  */
	  delay_set_stat (file_name,
			  &current_stat /* ignored */,
			  invert_permissions, INTERDIR_PERMSTATUS);

	  print_for_mkdir (file_name, cursor - file_name, mode);
	  did_something = 1;

	  *cursor = '/';
	  continue;
	}

      *cursor = '/';

      if (errno == EEXIST
#if MSDOS
	  /* Turbo C mkdir gives a funny errno.  */
	  || errno == EACCES
#endif
	  )
	/* Directory already exists.  */
	continue;

      /* Some other error in the mkdir.  We return to the caller.  */
      break;
    }

  return did_something;		/* tell them to retry if we made one */
}

/* Prepare to extract a file.
   Return zero if extraction should not proceed.  */

static int
prepare_to_extract (char const *file_name)
{
  if (to_stdout_option)
    return 0;

  if (old_files_option == UNLINK_FIRST_OLD_FILES
      && !remove_any_file (file_name, recursive_unlink_option)
      && errno && errno != ENOENT)
    {
      unlink_error (file_name);
      return 0;
    }

  return 1;
}

/* Attempt repairing what went wrong with the extraction.  Delete an
   already existing file or create missing intermediate directories.
   Return nonzero if we somewhat increased our chances at a successful
   extraction.  errno is properly restored on zero return.  */
static int
maybe_recoverable (char *file_name, int *interdir_made)
{
  if (*interdir_made)
    return 0;

  switch (errno)
    {
    case EEXIST:
      /* Remove an old file, if the options allow this.  */

      switch (old_files_option)
	{
	default:
	  return 0;

	case DEFAULT_OLD_FILES:
	case OVERWRITE_OLD_DIRS:
	case OVERWRITE_OLD_FILES:
	  {
	    int r = remove_any_file (file_name, 0);
	    errno = EEXIST;
	    return r;
	  }
	}

    case ENOENT:
      /* Attempt creating missing intermediate directories.  */
      if (! make_directories (file_name))
	{
	  errno = ENOENT;
	  return 0;
	}
      *interdir_made = 1;
      return 1;

    default:
      /* Just say we can't do anything about it...  */

      return 0;
    }
}

static void
extract_sparse_file (int fd, off_t *sizeleft, off_t totalsize, char *name)
{
  int sparse_ind = 0;

  /* assuming sizeleft is initially totalsize */

  while (*sizeleft > 0)
    {
      size_t written;
      size_t count;
      union block *data_block = find_next_block ();
      if (! data_block)
	{
	  ERROR ((0, 0, _("Unexpected EOF in archive")));
	  return;
	}
      if (lseek (fd, sparsearray[sparse_ind].offset, SEEK_SET) < 0)
	{
	  seek_error_details (name, sparsearray[sparse_ind].offset);
	  return;
	}
      written = sparsearray[sparse_ind++].numbytes;
      while (written > BLOCKSIZE)
	{
	  count = full_write (fd, data_block->buffer, BLOCKSIZE);
	  written -= count;
	  *sizeleft -= count;
	  if (count != BLOCKSIZE)
	    {
	      write_error_details (name, count, BLOCKSIZE);
	      return;
	    }
	  set_next_block_after (data_block);
	  data_block = find_next_block ();
	  if (! data_block)
	    {
	      ERROR ((0, 0, _("Unexpected EOF in archive")));
	      return;
	    }
	}

      count = full_write (fd, data_block->buffer, written);
      *sizeleft -= count;

      if (count != written)
	{
	  write_error_details (name, count, written);
	  return;
	}

      set_next_block_after (data_block);
    }
}

/* Fix the statuses of all directories whose statuses need fixing, and
   which are not ancestors of FILE_NAME.  If AFTER_SYMLINKS is
   nonzero, do this for all such directories; otherwise, stop at the
   first directory that is marked to be fixed up only after delayed
   symlinks are applied.  */
static void
apply_nonancestor_delayed_set_stat (char const *file_name, bool after_symlinks)
{
  size_t file_name_len = strlen (file_name);
  bool check_for_renamed_directories = 0;

  while (delayed_set_stat_head)
    {
      struct delayed_set_stat *data = delayed_set_stat_head;
      bool skip_this_one = 0;
      struct stat st;
      struct stat const *current_stat_info = 0;

      check_for_renamed_directories |= data->after_symlinks;

      if (after_symlinks < data->after_symlinks
	  || (data->file_name_len < file_name_len
	      && file_name[data->file_name_len]
	      && (ISSLASH (file_name[data->file_name_len])
		  || ISSLASH (file_name[data->file_name_len - 1]))
	      && memcmp (file_name, data->file_name, data->file_name_len) == 0))
	break;

      if (check_for_renamed_directories)
	{
	  current_stat_info = &st;
	  if (stat (data->file_name, &st) != 0)
	    {
	      stat_error (data->file_name);
	      skip_this_one = 1;
	    }
	  else if (! (st.st_dev == data->stat_info.st_dev
		      && (st.st_ino == data->stat_info.st_ino)))
	    {
	      ERROR ((0, 0,
		      _("%s: Directory renamed before its status could be extracted"),
		      quotearg_colon (data->file_name)));
	      skip_this_one = 1;
	    }
	}

      if (! skip_this_one)
	set_stat (data->file_name, &data->stat_info, current_stat_info,
		  data->invert_permissions, data->permstatus, DIRTYPE);

      delayed_set_stat_head = data->next;
      free (data);
    }
}

/* Extract a file from the archive.  */
void
extract_archive (void)
{
  union block *data_block;
  int fd;
  int status;
  size_t count;
  size_t name_length;
  size_t written;
  int openflag;
  mode_t mode;
  off_t size;
  size_t skipcrud;
  int counter;
  int interdir_made = 0;
  char typeflag;
  union block *exhdr;

#define CURRENT_FILE_NAME (skipcrud + current_file_name)

  set_next_block_after (current_header);
  decode_header (current_header, &current_stat, &current_format, 1);

  if (interactive_option && !confirm ("extract", current_file_name))
    {
      skip_member ();
      return;
    }

  /* Print the block from current_header and current_stat.  */

  if (verbose_option)
    print_header ();

  /* Check for fully specified file names and other atrocities.  */

  skipcrud = 0;
  if (! absolute_names_option)
    {
      if (contains_dot_dot (CURRENT_FILE_NAME))
	{
	  ERROR ((0, 0, _("%s: Member name contains `..'"),
		  quotearg_colon (CURRENT_FILE_NAME)));
	  skip_member ();
	  return;
	}

      skipcrud = FILESYSTEM_PREFIX_LEN (current_file_name);
      while (ISSLASH (CURRENT_FILE_NAME[0]))
	skipcrud++;

      if (skipcrud)
	{
	  static int warned_once;
	  
	  if (!warned_once)
	    {
	      warned_once = 1;
	      WARN ((0, 0, _("Removing leading `%.*s' from member names"),
		     (int) skipcrud, current_file_name));
	    }
	}
    }

  apply_nonancestor_delayed_set_stat (CURRENT_FILE_NAME, 0);

  /* Take a safety backup of a previously existing file.  */

  if (backup_option && !to_stdout_option)
    if (!maybe_backup_file (CURRENT_FILE_NAME, 0))
      {
	int e = errno;
	ERROR ((0, e, _("%s: Was unable to backup this file"),
		quotearg_colon (CURRENT_FILE_NAME)));
	skip_member ();
	return;
      }

  /* Extract the archive entry according to its type.  */

  typeflag = current_header->header.typeflag;
  switch (typeflag)
    {
      /* JK - What we want to do if the file is sparse is loop through
	 the array of sparse structures in the header and read in and
	 translate the character strings representing 1) the offset at
	 which to write and 2) how many bytes to write into numbers,
	 which we store into the scratch array, "sparsearray".  This
	 array makes our life easier the same way it did in creating the
	 tar file that had to deal with a sparse file.

	 After we read in the first five (at most) sparse structures, we
	 check to see if the file has an extended header, i.e., if more
	 sparse structures are needed to describe the contents of the new
	 file.  If so, we read in the extended headers and continue to
	 store their contents into the sparsearray.  */

    case GNUTYPE_SPARSE:
      sp_array_size = 10;
      sparsearray =
	xmalloc (sp_array_size * sizeof (struct sp_array));

      for (counter = 0; counter < SPARSES_IN_OLDGNU_HEADER; counter++)
	{
	  struct sparse const *s = &current_header->oldgnu_header.sp[counter];
	  sparsearray[counter].offset = OFF_FROM_HEADER (s->offset);
	  sparsearray[counter].numbytes = SIZE_FROM_HEADER (s->numbytes);
	  if (!sparsearray[counter].numbytes)
	    break;
	}

      if (current_header->oldgnu_header.isextended)
	{
	  /* Read in the list of extended headers and translate them
	     into the sparsearray as before.  Note that this
	     invalidates current_header.  */

	  /* static */ int ind = SPARSES_IN_OLDGNU_HEADER;

	  while (1)
	    {
	      exhdr = find_next_block ();
	      if (! exhdr)
		{
		  ERROR ((0, 0, _("Unexpected EOF in archive")));
		  return;
		}
	      for (counter = 0; counter < SPARSES_IN_SPARSE_HEADER; counter++)
		{
		  struct sparse const *s = &exhdr->sparse_header.sp[counter];
		  if (counter + ind > sp_array_size - 1)
		    {
		      /* Realloc the scratch area since we've run out of
			 room.  */

		      sp_array_size *= 2;
		      sparsearray =
			xrealloc (sparsearray,
				  sp_array_size * sizeof (struct sp_array));
		    }
		  if (s->numbytes[0] == 0)
		    break;
		  sparsearray[counter + ind].offset =
		    OFF_FROM_HEADER (s->offset);
		  sparsearray[counter + ind].numbytes =
		    SIZE_FROM_HEADER (s->numbytes);
		}
	      if (!exhdr->sparse_header.isextended)
		break;
	      else
		{
		  ind += SPARSES_IN_SPARSE_HEADER;
		  set_next_block_after (exhdr);
		}
	    }
	  set_next_block_after (exhdr);
	}
      /* Fall through.  */

    case AREGTYPE:
    case REGTYPE:
    case CONTTYPE:

      /* Appears to be a file.  But BSD tar uses the convention that a slash
	 suffix means a directory.  */

      name_length = strlen (CURRENT_FILE_NAME);
      if (FILESYSTEM_PREFIX_LEN (CURRENT_FILE_NAME) < name_length
	  && CURRENT_FILE_NAME[name_length - 1] == '/')
	goto really_dir;

      /* FIXME: deal with protection issues.  */

    again_file:
      openflag = (O_WRONLY | O_BINARY | O_CREAT
		  | (old_files_option == OVERWRITE_OLD_FILES
		     ? O_TRUNC
		     : O_EXCL));
      mode = current_stat.st_mode & MODE_RWX & ~ current_umask;

      if (to_stdout_option)
	{
	  fd = STDOUT_FILENO;
	  goto extract_file;
	}

      if (! prepare_to_extract (CURRENT_FILE_NAME))
	{
	  skip_member ();
	  if (backup_option)
	    undo_last_backup ();
	  break;
	}

#if O_CTG
      /* Contiguous files (on the Masscomp) have to specify the size in
	 the open call that creates them.  */

      if (typeflag == CONTTYPE)
	fd = open (CURRENT_FILE_NAME, openflag | O_CTG,
		   mode, current_stat.st_size);
      else
	fd = open (CURRENT_FILE_NAME, openflag, mode);

#else /* not O_CTG */
      if (typeflag == CONTTYPE)
	{
	  static int conttype_diagnosed;

	  if (!conttype_diagnosed)
	    {
	      conttype_diagnosed = 1;
	      WARN ((0, 0, _("Extracting contiguous files as regular files")));
	    }
	}
      fd = open (CURRENT_FILE_NAME, openflag, mode);

#endif /* not O_CTG */

      if (fd < 0)
	{
	  if (maybe_recoverable (CURRENT_FILE_NAME, &interdir_made))
	    goto again_file;

	  open_error (CURRENT_FILE_NAME);
	  skip_member ();
	  if (backup_option)
	    undo_last_backup ();
	  break;
	}

    extract_file:
      if (typeflag == GNUTYPE_SPARSE)
	{
	  char *name;
	  size_t name_length_bis;

	  /* Kludge alert.  NAME is assigned to header.name because
	     during the extraction, the space that contains the header
	     will get scribbled on, and the name will get munged, so any
	     error messages that happen to contain the filename will look
	     REAL interesting unless we do this.  */

	  name_length_bis = strlen (CURRENT_FILE_NAME) + 1;
	  name = xmalloc (name_length_bis);
	  memcpy (name, CURRENT_FILE_NAME, name_length_bis);
	  size = current_stat.st_size;
	  extract_sparse_file (fd, &size, current_stat.st_size, name);
	  free (sparsearray);
	}
      else
	for (size = current_stat.st_size; size > 0; )
	  {
	    if (multi_volume_option)
	      {
		assign_string (&save_name, current_file_name);
		save_totsize = current_stat.st_size;
		save_sizeleft = size;
	      }

	    /* Locate data, determine max length writeable, write it,
	       block that we have used the data, then check if the write
	       worked.  */

	    data_block = find_next_block ();
	    if (! data_block)
	      {
		ERROR ((0, 0, _("Unexpected EOF in archive")));
		break;		/* FIXME: What happens, then?  */
	      }

	    written = available_space_after (data_block);

	    if (written > size)
	      written = size;
	    errno = 0;
	    count = full_write (fd, data_block->buffer, written);
	    size -= count;

	    set_next_block_after ((union block *)
				  (data_block->buffer + written - 1));
	    if (count != written)
	      {
		write_error_details (CURRENT_FILE_NAME, count, written);
		break;
	      }
	  }

      skip_file (size);

      if (multi_volume_option)
	assign_string (&save_name, 0);

      /* If writing to stdout, don't try to do anything to the filename;
	 it doesn't exist, or we don't want to touch it anyway.  */

      if (to_stdout_option)
	break;

      status = close (fd);
      if (status < 0)
	{
	  close_error (CURRENT_FILE_NAME);
	  if (backup_option)
	    undo_last_backup ();
	}

      set_stat (CURRENT_FILE_NAME, &current_stat, 0, 0,
		(old_files_option == OVERWRITE_OLD_FILES
		 ? UNKNOWN_PERMSTATUS
		 : ARCHIVED_PERMSTATUS),
		typeflag);
      break;

    case SYMTYPE:
#ifdef HAVE_SYMLINK
      if (! prepare_to_extract (CURRENT_FILE_NAME))
	break;

      if (absolute_names_option
	  || ! (ISSLASH (current_link_name
			 [FILESYSTEM_PREFIX_LEN (current_link_name)])
		|| contains_dot_dot (current_link_name)))
	{
	  while (status = symlink (current_link_name, CURRENT_FILE_NAME),
		 status != 0)
	    if (!maybe_recoverable (CURRENT_FILE_NAME, &interdir_made))
	      break;

	  if (status == 0)
	    set_stat (CURRENT_FILE_NAME, &current_stat, 0, 0, 0, SYMTYPE);
	  else
	    symlink_error (current_link_name, CURRENT_FILE_NAME);
	}
      else
	{
	  /* This symbolic link is potentially dangerous.  Don't
	     create it now; instead, create a placeholder file, which
	     will be replaced after other extraction is done.  */
	  struct stat st;

	  while (fd = open (CURRENT_FILE_NAME, O_WRONLY | O_CREAT | O_EXCL, 0),
		 fd < 0)
	    if (! maybe_recoverable (CURRENT_FILE_NAME, &interdir_made))
	      break;

	  status = -1;
	  if (fd < 0)
	    open_error (CURRENT_FILE_NAME);
	  else if (fstat (fd, &st) != 0)
	    {
	      stat_error (CURRENT_FILE_NAME);
	      close (fd);
	    }
	  else if (close (fd) != 0)
	    close_error (CURRENT_FILE_NAME);
	  else
	    {
	      struct delayed_set_stat *h;
	      struct delayed_symlink *p =
		xmalloc (offsetof (struct delayed_symlink, target)
			 + strlen (current_link_name) + 1);
	      p->next = delayed_symlink_head;
	      delayed_symlink_head = p;
	      p->dev = st.st_dev;
	      p->ino = st.st_ino;
	      p->mtime = st.st_mtime;
	      p->uid = current_stat.st_uid;
	      p->gid = current_stat.st_gid;
	      p->sources = xmalloc (offsetof (struct string_list, string)
				    + strlen (CURRENT_FILE_NAME) + 1);
	      p->sources->next = 0;
	      strcpy (p->sources->string, CURRENT_FILE_NAME);
	      strcpy (p->target, current_link_name);

	      h = delayed_set_stat_head;
	      if (h && ! h->after_symlinks
		  && strncmp (CURRENT_FILE_NAME, h->file_name, h->file_name_len) == 0
		  && ISSLASH (CURRENT_FILE_NAME[h->file_name_len])
		  && (base_name (CURRENT_FILE_NAME)
		      == CURRENT_FILE_NAME + h->file_name_len + 1))
		{
		  do
		    {
		      h->after_symlinks = 1;

		      if (stat (h->file_name, &st) != 0)
			stat_error (h->file_name);
		      else
			{
			  h->stat_info.st_dev = st.st_dev;
			  h->stat_info.st_ino = st.st_ino;
			}
		    }
		  while ((h = h->next) && ! h->after_symlinks);
		}

	      status = 0;
	    }
	}
  
      if (status != 0 && backup_option)
	undo_last_backup ();
      break;

#else
      {
	static int warned_once;

	if (!warned_once)
	  {
	    warned_once = 1;
	    WARN ((0, 0,
		   _("Attempting extraction of symbolic links as hard links")));
	  }
      }
      typeflag = LNKTYPE;
      /* Fall through.  */
#endif

    case LNKTYPE:
      if (! prepare_to_extract (CURRENT_FILE_NAME))
	break;

    again_link:
      {
	struct stat st1, st2;
	int e;
	size_t skiplinkcrud;

	if (absolute_names_option)
	  skiplinkcrud = 0;
	else {
	  skiplinkcrud = FILESYSTEM_PREFIX_LEN (current_link_name);
	  while (ISSLASH (current_link_name[skiplinkcrud]))
	    skiplinkcrud++;
	}

	/* MSDOS does not implement links.  However, djgpp's link() actually
	   copies the file.  */
	status = link (current_link_name + skiplinkcrud, CURRENT_FILE_NAME);

	if (status == 0)
	  {
	    struct delayed_symlink *ds = delayed_symlink_head;
	    if (ds && stat (current_link_name, &st1) == 0)
	      for (; ds; ds = ds->next)
		if (ds->dev == st1.st_dev
		    && ds->ino == st1.st_ino
		    && ds->mtime == st1.st_mtime)
		  {
		    struct string_list *p = 
		      xmalloc (offsetof (struct string_list, string)
			       + strlen (CURRENT_FILE_NAME) + 1);
		    strcpy (p->string, CURRENT_FILE_NAME);
		    p->next = ds->sources;
		    ds->sources = p;
		    break;
		  }
	    break;
	  }
	if (maybe_recoverable (CURRENT_FILE_NAME, &interdir_made))
	  goto again_link;

	if (incremental_option && errno == EEXIST)
	  break;
	e = errno;
	if (stat (current_link_name, &st1) == 0
	    && stat (CURRENT_FILE_NAME, &st2) == 0
	    && st1.st_dev == st2.st_dev
	    && st1.st_ino == st2.st_ino)
	  break;

	link_error (current_link_name, CURRENT_FILE_NAME);
	if (backup_option)
	  undo_last_backup ();
      }
      break;

#if S_IFCHR
    case CHRTYPE:
      current_stat.st_mode |= S_IFCHR;
      goto make_node;
#endif

#if S_IFBLK
    case BLKTYPE:
      current_stat.st_mode |= S_IFBLK;
#endif

#if S_IFCHR || S_IFBLK
    make_node:
      if (! prepare_to_extract (CURRENT_FILE_NAME))
	break;

      status = mknod (CURRENT_FILE_NAME, current_stat.st_mode,
		      current_stat.st_rdev);
      if (status != 0)
	{
	  if (maybe_recoverable (CURRENT_FILE_NAME, &interdir_made))
	    goto make_node;
	  mknod_error (CURRENT_FILE_NAME);
	  if (backup_option)
	    undo_last_backup ();
	  break;
	};
      set_stat (CURRENT_FILE_NAME, &current_stat, 0, 0,
		ARCHIVED_PERMSTATUS, typeflag);
      break;
#endif

#if HAVE_MKFIFO || defined mkfifo
    case FIFOTYPE:
      if (! prepare_to_extract (CURRENT_FILE_NAME))
	break;

      while (status = mkfifo (CURRENT_FILE_NAME, current_stat.st_mode),
	     status != 0)
	if (!maybe_recoverable (CURRENT_FILE_NAME, &interdir_made))
	  break;

      if (status == 0)
	set_stat (CURRENT_FILE_NAME, &current_stat, 0, 0,
		  ARCHIVED_PERMSTATUS, typeflag);
      else
	{
	  mkfifo_error (CURRENT_FILE_NAME);
	  if (backup_option)
	    undo_last_backup ();
	}
      break;
#endif

    case DIRTYPE:
    case GNUTYPE_DUMPDIR:
      name_length = strlen (CURRENT_FILE_NAME);

    really_dir:
      /* Remove any redundant trailing "/"s.  */
      while (FILESYSTEM_PREFIX_LEN (CURRENT_FILE_NAME) < name_length
	     && CURRENT_FILE_NAME[name_length - 1] == '/')
	name_length--;
      CURRENT_FILE_NAME[name_length] = '\0';

      if (incremental_option)
	{
	  /* Read the entry and delete files that aren't listed in the
	     archive.  */

	  gnu_restore (skipcrud);
	}
      else if (typeflag == GNUTYPE_DUMPDIR)
	skip_member ();

      {
	struct stat st;
	if (stat (CURRENT_FILE_NAME, &st) != 0 || !S_ISDIR (st.st_mode))
	  if (! prepare_to_extract (CURRENT_FILE_NAME))
	    break;
      }

      mode = ((current_stat.st_mode
	       | (we_are_root ? 0 : MODE_WXUSR))
	      & MODE_RWX);

    again_dir:
      status = mkdir (CURRENT_FILE_NAME, mode);

      if (status != 0)
	{
	  if (errno == EEXIST
	      && (interdir_made
		  || old_files_option == OVERWRITE_OLD_DIRS
		  || old_files_option == OVERWRITE_OLD_FILES))
	    {
	      struct stat st;
	      if (stat (CURRENT_FILE_NAME, &st) == 0)
		{
		  if (interdir_made)
		    {
		      repair_delayed_set_stat (CURRENT_FILE_NAME, &st);
		      break;
		    }
		  if (S_ISDIR (st.st_mode))
		    {
		      mode = st.st_mode & ~ current_umask;
		      goto directory_exists;
		    }
		}
	      errno = EEXIST;
	    }
	
	  if (maybe_recoverable (CURRENT_FILE_NAME, &interdir_made))
	    goto again_dir;

	  if (errno != EEXIST)
	    {
	      mkdir_error (CURRENT_FILE_NAME);
	      if (backup_option)
		undo_last_backup ();
	      break;
	    }
	}

    directory_exists:
      if (status == 0
	  || old_files_option == OVERWRITE_OLD_DIRS
	  || old_files_option == OVERWRITE_OLD_FILES)
	delay_set_stat (CURRENT_FILE_NAME, &current_stat,
			MODE_RWX & (mode ^ current_stat.st_mode),
			(status == 0
			 ? ARCHIVED_PERMSTATUS
			 : UNKNOWN_PERMSTATUS));
      break;

    case GNUTYPE_VOLHDR:
      if (verbose_option)
	fprintf (stdlis, _("Reading %s\n"), quote (current_file_name));
      break;

    case GNUTYPE_NAMES:
      extract_mangle ();
      break;

    case GNUTYPE_MULTIVOL:
      ERROR ((0, 0,
	      _("%s: Cannot extract -- file is continued from another volume"),
	      quotearg_colon (current_file_name)));
      skip_member ();
      if (backup_option)
	undo_last_backup ();
      break;

    case GNUTYPE_LONGNAME:
    case GNUTYPE_LONGLINK:
      ERROR ((0, 0, _("Visible long name error")));
      skip_member ();
      if (backup_option)
	undo_last_backup ();
      break;

    default:
      WARN ((0, 0,
	     _("%s: Unknown file type '%c', extracted as normal file"),
	     quotearg_colon (CURRENT_FILE_NAME), typeflag));
      goto again_file;
    }

#undef CURRENT_FILE_NAME
}

/* Extract the symbolic links whose final extraction were delayed.  */
static void
apply_delayed_symlinks (void)
{
  struct delayed_symlink *ds;

  for (ds = delayed_symlink_head; ds; )
    {
      struct string_list *sources = ds->sources;
      char const *valid_source = 0;

      for (sources = ds->sources; sources; sources = sources->next)
	{
	  char const *source = sources->string;
	  struct stat st;

	  /* Make sure the placeholder file is still there.  If not,
	     don't create a symlink, as the placeholder was probably
	     removed by a later extraction.  */
	  if (lstat (source, &st) == 0
	      && st.st_dev == ds->dev
	      && st.st_ino == ds->ino
	      && st.st_mtime == ds->mtime)
	    {
	      /* Unlink the placeholder, then create a hard link if possible,
		 a symbolic link otherwise.  */
	      if (unlink (source) != 0)
		unlink_error (source);
	      else if (valid_source && link (valid_source, source) == 0)
		;
	      else if (symlink (ds->target, source) != 0)
		symlink_error (ds->target, source);
	      else
		{
		  valid_source = source;
		  st.st_uid = ds->uid;
		  st.st_gid = ds->gid;
		  set_stat (source, &st, 0, 0, 0, SYMTYPE);
		}
	    }
	}

      for (sources = ds->sources; sources; )
	{
	  struct string_list *next = sources->next;
	  free (sources);
	  sources = next;
	}

      {
	struct delayed_symlink *next = ds->next;
	free (ds);
	ds = next;
      }
    }

  delayed_symlink_head = 0;
}

/* Finish the extraction of an archive.  */
void
extract_finish (void)
{
  /* First, fix the status of ordinary directories that need fixing.  */
  apply_nonancestor_delayed_set_stat ("", 0);

  /* Then, apply delayed symlinks, so that they don't affect delayed
     directory status-setting for ordinary directories.  */
  apply_delayed_symlinks ();

  /* Finally, fix the status of directories that are ancestors
     of delayed symlinks.  */
  apply_nonancestor_delayed_set_stat ("", 1);
}

void
fatal_exit (void)
{
  extract_finish ();
  error (TAREXIT_FAILURE, 0, _("Error is not recoverable: exiting now"));
  abort ();
}
