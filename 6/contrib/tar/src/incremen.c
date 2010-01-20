/* GNU dump extensions to tar.

   Copyright 1988, 1992, 1993, 1994, 1996, 1997, 1999, 2000, 2001 Free
   Software Foundation, Inc.

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

#include "system.h"
#include <getline.h>
#include <hash.h>
#include <quotearg.h>
#include "common.h"

/* Variable sized generic character buffers.  */

struct accumulator
{
  size_t allocated;
  size_t length;
  char *pointer;
};

/* Amount of space guaranteed just after a reallocation.  */
#define ACCUMULATOR_SLACK 50

/* Return the accumulated data from an ACCUMULATOR buffer.  */
static char *
get_accumulator (struct accumulator *accumulator)
{
  return accumulator->pointer;
}

/* Allocate and return a new accumulator buffer.  */
static struct accumulator *
new_accumulator (void)
{
  struct accumulator *accumulator
    = xmalloc (sizeof (struct accumulator));

  accumulator->allocated = ACCUMULATOR_SLACK;
  accumulator->pointer = xmalloc (ACCUMULATOR_SLACK);
  accumulator->length = 0;
  return accumulator;
}

/* Deallocate an ACCUMULATOR buffer.  */
static void
delete_accumulator (struct accumulator *accumulator)
{
  free (accumulator->pointer);
  free (accumulator);
}

/* At the end of an ACCUMULATOR buffer, add a DATA block of SIZE bytes.  */
static void
add_to_accumulator (struct accumulator *accumulator,
		    const char *data, size_t size)
{
  if (accumulator->length + size > accumulator->allocated)
    {
      accumulator->allocated = accumulator->length + size + ACCUMULATOR_SLACK;
      accumulator->pointer =
	xrealloc (accumulator->pointer, accumulator->allocated);
    }
  memcpy (accumulator->pointer + accumulator->length, data, size);
  accumulator->length += size;
}

/* Incremental dump specialities.  */

/* Which child files to save under a directory.  */
enum children {NO_CHILDREN, CHANGED_CHILDREN, ALL_CHILDREN};

/* Directory attributes.  */
struct directory
  {
    dev_t device_number;	/* device number for directory */
    ino_t inode_number;		/* inode number for directory */
    enum children children;
    char nfs;
    char found;
    char name[1];		/* path name of directory */
  };

static Hash_table *directory_table;

#if HAVE_ST_FSTYPE_STRING
  static char const nfs_string[] = "nfs";
# define NFS_FILE_STAT(st) (strcmp ((st).st_fstype, nfs_string) == 0)
#else
# define ST_DEV_MSB(st) (~ (dev_t) 0 << (sizeof (st).st_dev * CHAR_BIT - 1))
# define NFS_FILE_STAT(st) (((st).st_dev & ST_DEV_MSB (st)) != 0)
#endif

/* Calculate the hash of a directory.  */
static unsigned
hash_directory (void const *entry, unsigned n_buckets)
{
  struct directory const *directory = entry;
  return hash_string (directory->name, n_buckets);
}

/* Compare two directories for equality.  */
static bool
compare_directories (void const *entry1, void const *entry2)
{
  struct directory const *directory1 = entry1;
  struct directory const *directory2 = entry2;
  return strcmp (directory1->name, directory2->name) == 0;
}

/* Create and link a new directory entry for directory NAME, having a
   device number DEV and an inode number INO, with NFS indicating
   whether it is an NFS device and FOUND indicating whether we have
   found that the directory exists.  */
static struct directory *
note_directory (char const *name, dev_t dev, ino_t ino, bool nfs, bool found)
{
  size_t size = offsetof (struct directory, name) + strlen (name) + 1;
  struct directory *directory = xmalloc (size);

  directory->device_number = dev;
  directory->inode_number = ino;
  directory->children = CHANGED_CHILDREN;
  directory->nfs = nfs;
  directory->found = found;
  strcpy (directory->name, name);

  if (! ((directory_table
	  || (directory_table = hash_initialize (0, 0, hash_directory,
						 compare_directories, 0)))
	 && hash_insert (directory_table, directory)))
    xalloc_die ();

  return directory;
}

/* Return a directory entry for a given path NAME, or zero if none found.  */
static struct directory *
find_directory (char *name)
{
  if (! directory_table)
    return 0;
  else
    {
      size_t size = offsetof (struct directory, name) + strlen (name) + 1;
      struct directory *dir = alloca (size);
      strcpy (dir->name, name);
      return hash_lookup (directory_table, dir);
    }
}

static int
compare_dirents (const void *first, const void *second)
{
  return strcmp ((*(char *const *) first) + 1,
		 (*(char *const *) second) + 1);
}

char *
get_directory_contents (char *path, dev_t device)
{
  struct accumulator *accumulator;

  /* Recursively scan the given PATH.  */

  {
    char *dirp = savedir (path);	/* for scanning directory */
    char const *entry;	/* directory entry being scanned */
    size_t entrylen;	/* length of directory entry */
    char *name_buffer;		/* directory, `/', and directory member */
    size_t name_buffer_size;	/* allocated size of name_buffer, minus 2 */
    size_t name_length;		/* used length in name_buffer */
    struct directory *directory; /* for checking if already already seen */
    enum children children;

    if (! dirp)
      savedir_error (path);
    errno = 0;

    name_buffer_size = strlen (path) + NAME_FIELD_SIZE;
    name_buffer = xmalloc (name_buffer_size + 2);
    strcpy (name_buffer, path);
    if (! ISSLASH (path[strlen (path) - 1]))
      strcat (name_buffer, "/");
    name_length = strlen (name_buffer);

    directory = find_directory (path);
    children = directory ? directory->children : CHANGED_CHILDREN;

    accumulator = new_accumulator ();

    if (children != NO_CHILDREN)
      for (entry = dirp;
	   (entrylen = strlen (entry)) != 0;
	   entry += entrylen + 1)
	{
	  if (name_buffer_size <= entrylen + name_length)
	    {
	      do
		name_buffer_size += NAME_FIELD_SIZE;
	      while (name_buffer_size <= entrylen + name_length);
	      name_buffer = xrealloc (name_buffer, name_buffer_size + 2);
	    }
	  strcpy (name_buffer + name_length, entry);

	  if (excluded_name (name_buffer))
	    add_to_accumulator (accumulator, "N", 1);
	  else
	    {
	      struct stat stat_data;

	      if (deref_stat (dereference_option, name_buffer, &stat_data))
		{
		  if (ignore_failed_read_option)
		    stat_warn (name_buffer);
		  else
		    stat_error (name_buffer);
		  continue;
		}

	      if (S_ISDIR (stat_data.st_mode))
		{
		  bool nfs = NFS_FILE_STAT (stat_data);

		  if (directory = find_directory (name_buffer), directory)
		    {
		      /* With NFS, the same file can have two different devices
			 if an NFS directory is mounted in multiple locations,
			 which is relatively common when automounting.
			 To avoid spurious incremental redumping of
			 directories, consider all NFS devices as equal,
			 relying on the i-node to establish differences.  */

		      if (! (((directory->nfs & nfs)
			      || directory->device_number == stat_data.st_dev)
			     && directory->inode_number == stat_data.st_ino))
			{
			  if (verbose_option)
			    WARN ((0, 0, _("%s: Directory has been renamed"),
				   quotearg_colon (name_buffer)));
			  directory->children = ALL_CHILDREN;
			  directory->nfs = nfs;
			  directory->device_number = stat_data.st_dev;
			  directory->inode_number = stat_data.st_ino;
			}
		      directory->found = 1;
		    }
		  else
		    {
		      if (verbose_option)
			WARN ((0, 0, _("%s: Directory is new"),
			       quotearg_colon (name_buffer)));
		      directory = note_directory (name_buffer,
						  stat_data.st_dev,
						  stat_data.st_ino, nfs, 1);
		      directory->children = 
			((listed_incremental_option
			  || newer_mtime_option <= stat_data.st_mtime
			  || (after_date_option && 
			      newer_ctime_option <= stat_data.st_ctime))
			 ? ALL_CHILDREN
			 : CHANGED_CHILDREN);
		    }

		  if (one_file_system_option && device != stat_data.st_dev)
		    directory->children = NO_CHILDREN;
		  else if (children == ALL_CHILDREN)
		    directory->children = ALL_CHILDREN;

		  add_to_accumulator (accumulator, "D", 1);
		}

	      else if (one_file_system_option && device != stat_data.st_dev)
		add_to_accumulator (accumulator, "N", 1);

#ifdef S_ISHIDDEN
	      else if (S_ISHIDDEN (stat_data.st_mode))
		{
		  add_to_accumulator (accumulator, "D", 1);
		  add_to_accumulator (accumulator, entry, entrylen);
		  add_to_accumulator (accumulator, "A", 2);
		  continue;
		}
#endif

	      else
		if (children == CHANGED_CHILDREN
		    && stat_data.st_mtime < newer_mtime_option
		    && (!after_date_option
			|| stat_data.st_ctime < newer_ctime_option))
		  add_to_accumulator (accumulator, "N", 1);
		else
		  add_to_accumulator (accumulator, "Y", 1);
	    }

	  add_to_accumulator (accumulator, entry, entrylen + 1);
	}

    add_to_accumulator (accumulator, "\000\000", 2);

    free (name_buffer);
    free (dirp);
  }

  /* Sort the contents of the directory, now that we have it all.  */

  {
    char *pointer = get_accumulator (accumulator);
    size_t counter;
    char *cursor;
    char *buffer;
    char **array;
    char **array_cursor;

    counter = 0;
    for (cursor = pointer; *cursor; cursor += strlen (cursor) + 1)
      counter++;

    if (! counter)
      {
	delete_accumulator (accumulator);
	return 0;
      }

    array = xmalloc (sizeof (char *) * (counter + 1));

    array_cursor = array;
    for (cursor = pointer; *cursor; cursor += strlen (cursor) + 1)
      *array_cursor++ = cursor;
    *array_cursor = 0;

    qsort (array, counter, sizeof (char *), compare_dirents);

    buffer = xmalloc (cursor - pointer + 2);

    cursor = buffer;
    for (array_cursor = array; *array_cursor; array_cursor++)
      {
	char *string = *array_cursor;

	while ((*cursor++ = *string++))
	  continue;
      }
    *cursor = '\0';

    delete_accumulator (accumulator);
    free (array);
    return buffer;
  }
}

static FILE *listed_incremental_stream;

void
read_directory_file (void)
{
  int fd;
  FILE *fp;
  char *buf = 0;
  size_t bufsize;

  /* Open the file for both read and write.  That way, we can write
     it later without having to reopen it, and don't have to worry if
     we chdir in the meantime.  */
  fd = open (listed_incremental_option, O_RDWR | O_CREAT, MODE_RW);
  if (fd < 0)
    {
      open_error (listed_incremental_option);
      return;
    }

  fp = fdopen (fd, "r+");
  if (! fp)
    {
      open_error (listed_incremental_option);
      close (fd);
      return;
    }

  listed_incremental_stream = fp;

  if (0 < getline (&buf, &bufsize, fp))
    {
      char *ebuf;
      int n;
      long lineno = 1;
      unsigned long u = (errno = 0, strtoul (buf, &ebuf, 10));
      time_t t = u;
      if (buf == ebuf || (u == 0 && errno == EINVAL))
	ERROR ((0, 0, "%s:1: %s", quotearg_colon (listed_incremental_option),
		_("Invalid time stamp")));
      else if (t != u || (u == -1 && errno == ERANGE))
	ERROR ((0, 0, "%s:1: %s", quotearg_colon (listed_incremental_option),
		_("Time stamp out of range")));
      else
	newer_mtime_option = t;

      while (0 < (n = getline (&buf, &bufsize, fp)))
	{
	  dev_t dev;
	  ino_t ino;
	  int nfs = buf[0] == '+';
	  char *strp = buf + nfs;

	  lineno++;

	  if (buf[n - 1] == '\n')
	    buf[n - 1] = '\0';

	  errno = 0;
	  dev = u = strtoul (strp, &ebuf, 10);
	  if (strp == ebuf || (u == 0 && errno == EINVAL))
	    ERROR ((0, 0, "%s:%ld: %s",
		    quotearg_colon (listed_incremental_option), lineno,
		    _("Invalid device number")));
	  else if (dev != u || (u == -1 && errno == ERANGE))
	    ERROR ((0, 0, "%s:%ld: %s",
		    quotearg_colon (listed_incremental_option), lineno,
		    _("Device number out of range")));
	  strp = ebuf;

	  errno = 0;
	  ino = u = strtoul (strp, &ebuf, 10);
	  if (strp == ebuf || (u == 0 && errno == EINVAL))
	    ERROR ((0, 0, "%s:%ld: %s",
		    quotearg_colon (listed_incremental_option), lineno,
		    _("Invalid inode number")));
	  else if (ino != u || (u == -1 && errno == ERANGE))
	    ERROR ((0, 0, "%s:%ld: %s",
		    quotearg_colon (listed_incremental_option), lineno,
		    _("Inode number out of range")));
	  strp = ebuf;

	  strp++;
	  unquote_string (strp);
	  note_directory (strp, dev, ino, nfs, 0);
	}
    }

  if (ferror (fp))
    read_error (listed_incremental_option);
  if (buf)
    free (buf);
}

/* Output incremental data for the directory ENTRY to the file DATA.
   Return nonzero if successful, preserving errno on write failure.  */
static bool
write_directory_file_entry (void *entry, void *data)
{
  struct directory const *directory = entry;
  FILE *fp = data;

  if (directory->found)
    {
      int e;
      char *str = quote_copy_string (directory->name);
      fprintf (fp, "+%lu %lu %s\n" + ! directory->nfs,
	       (unsigned long) directory->device_number,
	       (unsigned long) directory->inode_number,
	       str ? str : directory->name);
      e = errno;
      if (str)
	free (str);
      errno = e;
    }

  return ! ferror (fp);
}

void
write_directory_file (void)
{
  FILE *fp = listed_incremental_stream;

  if (! fp)
    return;

  if (fseek (fp, 0L, SEEK_SET) != 0)
    seek_error (listed_incremental_option);
  if (ftruncate (fileno (fp), (off_t) 0) != 0)
    truncate_error (listed_incremental_option);

  fprintf (fp, "%lu\n", (unsigned long) start_time);
  if (! ferror (fp) && directory_table)
    hash_do_for_each (directory_table, write_directory_file_entry, fp);
  if (ferror (fp))
    write_error (listed_incremental_option);
  if (fclose (fp) != 0)
    close_error (listed_incremental_option);
}

/* Restoration of incremental dumps.  */

void
gnu_restore (size_t skipcrud)
{
  char *archive_dir;
  char *current_dir;
  char *cur, *arc;
  size_t size;
  size_t copied;
  union block *data_block;
  char *to;

#define CURRENT_FILE_NAME (skipcrud + current_file_name)

  current_dir = savedir (CURRENT_FILE_NAME);

  if (!current_dir)
    {
      /* The directory doesn't exist now.  It'll be created.  In any
	 case, we don't have to delete any files out of it.  */

      skip_member ();
      return;
    }

  size = current_stat.st_size;
  if (size != current_stat.st_size)
    xalloc_die ();
  archive_dir = xmalloc (size);
  to = archive_dir;
  for (; size > 0; size -= copied)
    {
      data_block = find_next_block ();
      if (!data_block)
	{
	  ERROR ((0, 0, _("Unexpected EOF in archive")));
	  break;		/* FIXME: What happens then?  */
	}
      copied = available_space_after (data_block);
      if (copied > size)
	copied = size;
      memcpy (to, data_block->buffer, copied);
      to += copied;
      set_next_block_after ((union block *)
			    (data_block->buffer + copied - 1));
    }

  for (cur = current_dir; *cur; cur += strlen (cur) + 1)
    {
      for (arc = archive_dir; *arc; arc += strlen (arc) + 1)
	{
	  arc++;
	  if (!strcmp (arc, cur))
	    break;
	}
      if (*arc == '\0')
	{
	  char *p = new_name (CURRENT_FILE_NAME, cur);
	  if (! interactive_option || confirm ("delete", p))
	    {
	      if (verbose_option)
		fprintf (stdlis, _("%s: Deleting %s\n"),
			 program_name, quote (p));
	      if (! remove_any_file (p, 1))
		{
		  int e = errno;
		  ERROR ((0, e, _("%s: Cannot remove"), quotearg_colon (p)));
		}
	    }
	  free (p);
	}

    }
  free (current_dir);
  free (archive_dir);

#undef CURRENT_FILE_NAME
}
