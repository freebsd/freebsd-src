/* Declarations for file attribute munging features.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#ifndef FILEATTR_H

/* File containing per-file attributes.  The format of this file is in
   cvs.texinfo but here is a quick summary.  The file contains a
   series of entries:

   ENT-TYPE FILENAME <tab> ATTRNAME = ATTRVAL
     {; ATTRNAME = ATTRVAL} <linefeed>

   ENT-TYPE is 'F' for a file.

   ENT-TYPE is 'D', and FILENAME empty, for default attributes.

   Other ENT-TYPE are reserved for future expansion.

   Note that the order of the line is not significant; CVS is free to
   rearrange them at its convenience.

   FIXME: this implementation doesn't handle '\0' in any of the
   fields.  We are encouraged to fix this (by cvs.texinfo).

   By convention, ATTRNAME starting with '_' is for an attribute given
   special meaning by CVS; other ATTRNAMEs are for user-defined attributes
   (or will be, once we add commands to manipulate user-defined attributes).

   Builtin attributes:

   _watched: Present means the file is watched and should be checked out
   read-only.

   _watchers: Users with watches for this file.  Value is
   WATCHER > TYPE { , WATCHER > TYPE }
   where WATCHER is a username, and TYPE is edit,unedit,commit separated by
   + (or nothing if none; there is no "none" or "all" keyword).

   _editors: Users editing this file.  Value is
   EDITOR > VAL { , EDITOR > VAL }
   where EDITOR is a username, and VAL is TIME+HOSTNAME+PATHNAME, where
   TIME is when the "cvs edit" command happened,
   and HOSTNAME and PATHNAME are for the working directory.  */

#define CVSREP_FILEATTR "CVS/fileattr"

/* Prepare for a new directory with repository REPOS.  If REPOS is NULL,
   then prepare for a "non-directory"; the caller can call fileattr_write
   and fileattr_free, but must not call fileattr_get or fileattr_set.  */
extern void fileattr_startdir PROTO ((const char *repos));

/* Get the attribute ATTRNAME for file FILENAME.  The return value
   points into memory managed by the fileattr_* routines, should not
   be altered by the caller, and is only good until the next call to
   fileattr_clear or fileattr_set.  It points to the value, terminated
   by '\0' or ';'.  Return NULL if said file lacks said attribute.
   If FILENAME is NULL, return default attributes (attributes for
   files created in the future).  */
extern char *fileattr_get PROTO ((const char *filename, const char *attrname));

/* Like fileattr_get, but return a pointer to a newly malloc'd string
   terminated by '\0' (or NULL if said file lacks said attribute).  */
extern char *fileattr_get0 PROTO ((const char *filename,
				   const char *attrname));

/* This is just a string manipulation function; it does not manipulate
   file attributes as such.  

   LIST is in the format

   ATTRNAME NAMEVALSEP ATTRVAL {ENTSEP ATTRNAME NAMEVALSEP ATTRVAL}

   And we want to put in an attribute with name NAME and value VAL,
   replacing the already-present attribute with name NAME if there is
   one.  Or if VAL is NULL remove attribute NAME.  Return a new
   malloc'd list; don't muck with the one passed in.  If we are removing
   the last attribute return NULL.  LIST can be NULL to mean that we
   started out without any attributes.

   Examples:

   fileattr_modify ("abc=def", "xxx", "val", '=', ';')) => "abc=def;xxx=val"
   fileattr_modify ("abc=def", "abc", "val", '=', ';')) => "abc=val"
   fileattr_modify ("abc=v1;def=v2", "abc", "val", '=', ';'))
     => "abc=val;def=v2"
   fileattr_modify ("abc=v1;def=v2", "def", "val", '=', ';'))
     => "abc=v1;def=val"
   fileattr_modify ("abc=v1;def=v2", "xxx", "val", '=', ';'))
     => "abc=v1;def=v2;xxx=val"
   fileattr_modify ("abc=v1;def=v2;ghi=v3", "def", "val", '=', ';'))
     => "abc=v1;def=val;ghi=v3"
*/

extern char *fileattr_modify PROTO ((char *list, const char *attrname,
				     const char *attrval, int namevalsep,
				     int entsep));

/* Set attribute ATTRNAME for file FILENAME to ATTRVAL.  If ATTRVAL is NULL,
   the attribute is removed.  Changes are not written to disk until the
   next call to fileattr_write.  If FILENAME is NULL, set attributes for
   files created in the future.  If ATTRVAL is NULL, remove that attribute.  */
extern void fileattr_set PROTO ((const char *filename, const char *attrname,
				 const char *attrval));

/* Get all the attributes for file FILENAME.  They are returned as malloc'd
   data in an unspecified format which is guaranteed only to be good for
   passing to fileattr_setall, or NULL if no attributes.  If FILENAME is
   NULL, get default attributes.  */
extern char *fileattr_getall PROTO ((const char *filename));

/* Set the attributes for file FILENAME to ATTRS, overwriting all previous
   attributes for that file.  ATTRS was obtained from a previous call to
   fileattr_getall (malloc'd data or NULL).  */
extern void fileattr_setall PROTO ((const char *filename, const char *attrs));

/* Set the attributes for file FILENAME in whatever manner is appropriate
   for a newly created file.  */
extern void fileattr_newfile PROTO ((const char *filename));

/* Write out all modified attributes.  */
extern void fileattr_write PROTO ((void));

/* Free all memory allocated by fileattr_*.  */
extern void fileattr_free PROTO ((void));

#define FILEATTR_H 1
#endif /* fileattr.h */
