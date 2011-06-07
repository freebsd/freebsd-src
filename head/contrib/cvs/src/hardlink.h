/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* Data type definitions and declarations for hardlink management.  */

/* This file should be #included in CVS source files after cvs.h
   since it relies on types and macros defined there. */

/* The `checked_out' member of a hardlink_info struct is used only
   when files are being checked out or updated.  It is used only when
   hardlinked files are being checked out. */

#ifdef PRESERVE_PERMISSIONS_SUPPORT
struct hardlink_info
{
    Ctype status;		/* as returned from Classify_File() */
    int checked_out;		/* has this file been checked out lately? */
};

extern List *hardlist;
extern char *working_dir;

Node *lookup_file_by_inode PROTO ((const char *));
void update_hardlink_info PROTO ((const char *));
List *list_linked_files_on_disk PROTO ((char *));
int compare_linkage_lists PROTO ((List *, List *));
int find_checkedout_proc PROTO ((Node *, void *));
#endif /* PRESERVE_PERMISSIONS_SUPPORT */
