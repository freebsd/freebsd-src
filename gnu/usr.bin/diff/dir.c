/* Read, sort and compare two directories.  Used for GNU DIFF.
   Copyright (C) 1988, 1989, 1992 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU DIFF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "diff.h"

static int compare_names ();

/* Read the directory named by DIR and store into DIRDATA a sorted vector
   of filenames for its contents.  DIR->desc == -1 means this directory is
   known to be nonexistent, so set DIRDATA to an empty vector.
   Return -1 (setting errno) if error, 0 otherwise.  */

struct dirdata
{
  char **files;	/* Sorted names of files in dir, terminated by (char *) 0. */
  char *data;	/* Allocated storage for file names.  */
};

static int
dir_sort (dir, dirdata)
     struct file_data *dir;
     struct dirdata *dirdata;
{
  register struct direct *next;
  register int i;

  /* Address of block containing the files that are described.  */
  char **files;

  /* Number of files in directory.  */
  int nfiles;

  /* Allocated and used storage for file name data.  */
  char *data;
  size_t data_alloc, data_used;

  dirdata->files = 0;
  dirdata->data = 0;
  nfiles = 0;

  if (dir->desc != -1)
    {
      /* Open the directory and check for errors.  */
      register DIR *reading = opendir (dir->name);
      if (!reading)
	return -1;

      /* Initialize the table of filenames.  */

      data_alloc = max (1, (size_t) dir->stat.st_size);
      data_used = 0;
      dirdata->data = data = (char *) xmalloc (data_alloc);

      /* Read the directory entries, and insert the subfiles
	 into the `data' table.  */

      while ((errno = 0, (next = readdir (reading)) != 0))
	{
	  char *d_name = next->d_name;
	  size_t d_size;

	  /* Ignore the files `.' and `..' */
	  if (d_name[0] == '.'
	      && (d_name[1] == 0 || (d_name[1] == '.' && d_name[2] == 0)))
	    continue;

	  if (excluded_filename (d_name))
	    continue;

	  d_size = strlen (d_name) + 1;
	  while (data_alloc < data_used + d_size)
	    dirdata->data = data = (char *) xrealloc (data, data_alloc *= 2);
	  bcopy (d_name, data + data_used, d_size);
	  data_used += d_size;
	  nfiles++;
	}
      if (errno)
	{
	  int e = errno;
	  closedir (reading);
	  errno = e;
	  return -1;
	}
#ifdef VOID_CLOSEDIR
      closedir (reading);
#else
      if (closedir (reading) != 0)
	return -1;
#endif
    }

  /* Create the `files' table from the `data' table.  */
  dirdata->files = files = (char **) xmalloc (sizeof (char *) * (nfiles + 1));
  for (i = 0;  i < nfiles;  i++)
    {
      files[i] = data;
      data += strlen (data) + 1;
    }
  files[nfiles] = 0;

  /* Sort the table.  */
  qsort (files, nfiles, sizeof (char *), compare_names);

  return 0;
}

/* Sort the files now in the table.  */

static int
compare_names (file1, file2)
     char **file1, **file2;
{
  return strcmp (*file1, *file2);
}

/* Compare the contents of two directories named in FILEVEC[0] and FILEVEC[1].
   This is a top-level routine; it does everything necessary for diff
   on two directories.

   FILEVEC[0].desc == -1 says directory FILEVEC[0] doesn't exist,
   but pretend it is empty.  Likewise for FILEVEC[1].

   HANDLE_FILE is a caller-provided subroutine called to handle each file.
   It gets five operands: dir and name (rel to original working dir) of file
   in dir 0, dir and name pathname of file in dir 1, and the recursion depth.

   For a file that appears in only one of the dirs, one of the name-args
   to HANDLE_FILE is zero.

   DEPTH is the current depth in recursion, used for skipping top-level
   files by the -S option.

   Returns the maximum of all the values returned by HANDLE_FILE,
   or 2 if trouble is encountered in opening files.  */

int
diff_dirs (filevec, handle_file, depth)
     struct file_data filevec[];
     int (*handle_file) ();
     int depth;
{
  struct dirdata dirdata[2];
  int val = 0;			/* Return value.  */
  int i;

  /* Get sorted contents of both dirs.  */
  for (i = 0; i < 2; i++)
    if (dir_sort (&filevec[i], &dirdata[i]) != 0)
      {
	perror_with_name (filevec[i].name);
	val = 2;
      }

  if (val == 0)
    {
      register char **files0 = dirdata[0].files;
      register char **files1 = dirdata[1].files;
      char *name0 = filevec[0].name;
      char *name1 = filevec[1].name;

      /* If `-S name' was given, and this is the topmost level of comparison,
	 ignore all file names less than the specified starting name.  */

      if (dir_start_file && depth == 0)
	{
	  while (*files0 && strcmp (*files0, dir_start_file) < 0)
	    files0++;
	  while (*files1 && strcmp (*files1, dir_start_file) < 0)
	    files1++;
	}

      /* Loop while files remain in one or both dirs.  */
      while (*files0 || *files1)
	{
	  /* Compare next name in dir 0 with next name in dir 1.
	     At the end of a dir,
	     pretend the "next name" in that dir is very large.  */
	  int nameorder = (!*files0 ? 1 : !*files1 ? -1
			   : strcmp (*files0, *files1));
	  int v1 = (*handle_file) (name0, 0 < nameorder ? 0 : *files0++,
				   name1, nameorder < 0 ? 0 : *files1++,
				   depth + 1);
	  if (v1 > val)
	    val = v1;
	}
    }
  
  for (i = 0; i < 2; i++)
    {
      if (dirdata[i].files)
	free (dirdata[i].files);
      if (dirdata[i].data)
	free (dirdata[i].data);
    }

  return val;
}
