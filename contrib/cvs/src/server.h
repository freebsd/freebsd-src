/* Interface between the server and the rest of CVS.  */

/* Miscellaneous stuff which isn't actually particularly server-specific.  */
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

#ifdef SERVER_SUPPORT

/*
 * Nonzero if we are using the server.  Used by various places to call
 * server-specific functions.
 */
extern int server_active;
extern int server_expanding;

/* Server functions exported to the rest of CVS.  */

/* Run the server.  */
extern int server PROTO((int argc, char **argv));

/* See server.c for description.  */
extern void server_pathname_check PROTO ((char *));

/* We have a new Entries line for a file.  TAG or DATE can be NULL.  */
extern void server_register
    PROTO((char *name, char *version, char *timestamp,
	     char *options, char *tag, char *date, char *conflict));

/* Set the modification time of the next file sent.  This must be
   followed by a call to server_updated on the same file.  */
extern void server_modtime PROTO ((struct file_info *finfo,
				   Vers_TS *vers_ts));

/*
 * We want to nuke the Entries line for a file, and (unless
 * server_scratch_entry_only is subsequently called) the file itself.
 */
extern void server_scratch PROTO((char *name));

/*
 * The file which just had server_scratch called on it needs to have only
 * the Entries line removed, not the file itself.
 */
extern void server_scratch_entry_only PROTO((void));

/*
 * We just successfully checked in FILE (which is just the bare
 * filename, with no directory).  REPOSITORY is the directory for the
 * repository.
 */
extern void server_checked_in
    PROTO((char *file, char *update_dir, char *repository));

extern void server_copy_file
    PROTO((char *file, char *update_dir, char *repository, char *newfile));

/* Send the appropriate responses for a file described by FINFO and
   VERS.  This is called after server_register or server_scratch.  In
   the latter case the file is to be removed (and VERS can be NULL).
   In the former case, VERS must be non-NULL, and UPDATED indicates
   whether the file is now up to date (SERVER_UPDATED, yes,
   SERVER_MERGED, no, SERVER_PATCHED, yes, but file is a diff from
   user version to repository version, SERVER_RCS_DIFF, yes, like
   SERVER_PATCHED but with an RCS style diff).  MODE is the mode the
   file should get, or (mode_t) -1 if this should be obtained from the
   file itself.  CHECKSUM is the MD5 checksum of the file, or NULL if
   this need not be sent.  If FILEBUF is not NULL, it holds the
   contents of the file, in which case the file itself may not exist.
   If FILEBUF is not NULL, server_updated will free it.  */
enum server_updated_arg4
{
    SERVER_UPDATED,
    SERVER_MERGED,
    SERVER_PATCHED,
    SERVER_RCS_DIFF
};
#ifdef __STDC__
struct buffer;
#endif

extern void server_updated
    PROTO((struct file_info *finfo, Vers_TS *vers,
	   enum server_updated_arg4 updated, mode_t mode,
	   unsigned char *checksum, struct buffer *filebuf));

/* Whether we should send RCS format patches.  */
extern int server_use_rcs_diff PROTO((void));

/* Set the Entries.Static flag.  */
extern void server_set_entstat PROTO((char *update_dir, char *repository));
/* Clear it.  */
extern void server_clear_entstat PROTO((char *update_dir, char *repository));

/* Set or clear a per-directory sticky tag or date.  */
extern void server_set_sticky PROTO((char *update_dir, char *repository,
				     char *tag, char *date, int nonbranch));
/* Send Template response.  */
extern void server_template PROTO ((char *, char *));

extern void server_update_entries
    PROTO((char *file, char *update_dir, char *repository,
	   enum server_updated_arg4 updated));

/* Pointer to a malloc'd string which is the directory which
   the server should prepend to the pathnames which it sends
   to the client.  */
extern char *server_dir;

enum progs {PROG_CHECKIN, PROG_UPDATE};
extern void server_prog PROTO((char *, char *, enum progs));
extern void server_cleanup PROTO((int sig));

#ifdef SERVER_FLOWCONTROL
/* Pause if it's convenient to avoid memory blowout */
extern void server_pause_check PROTO((void));
#endif /* SERVER_FLOWCONTROL */

#ifdef AUTH_SERVER_SUPPORT
extern char *CVS_Username;
extern int system_auth;
#endif /* AUTH_SERVER_SUPPORT */

#endif /* SERVER_SUPPORT */

/* Stuff shared with the client.  */
struct request
{
  /* Name of the request.  */
  char *name;

#ifdef SERVER_SUPPORT
  /*
   * Function to carry out the request.  ARGS is the text of the command
   * after name and, if present, a single space, have been stripped off.
   */
  void (*func) PROTO((char *args));
#endif

  /* Stuff for use by the client.  */
  enum {
      /*
       * Failure to implement this request can imply a fatal
       * error.  This should be set only for commands which were in the
       * original version of the protocol; it should not be set for new
       * commands.
       */
      rq_essential,

      /* Some servers might lack this request.  */
      rq_optional,

      /*
       * Set by the client to one of the following based on what this
       * server actually supports.
       */
      rq_supported,
      rq_not_supported,

      /*
       * If the server supports this request, and we do too, tell the
       * server by making the request.
       */
      rq_enableme
      } status;
};

/* Table of requests ending with an entry with a NULL name.  */
extern struct request requests[];

/* Gzip library, see zlib.c.  */
extern void gunzip_and_write PROTO ((int, char *, unsigned char *, size_t));
extern void read_and_gzip PROTO ((int, char *, unsigned char **, size_t *,
				  size_t *, int));
