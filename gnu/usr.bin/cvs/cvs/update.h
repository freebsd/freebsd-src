/* Definitions of routines shared between local and client/server
   "update" code.  */

/* List of files that we have either processed or are willing to
   ignore.  Any file not on this list gets a question mark printed.  */
extern List *ignlist;

extern int
update_filesdone_proc PROTO((int err, char *repository, char *update_dir));
