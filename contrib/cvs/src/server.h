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

/* We have a new Entries line for a file.  TAG or DATE can be NULL.  */
extern void server_register
    PROTO((char *name, char *version, char *timestamp,
	     char *options, char *tag, char *date, char *conflict));

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

/*
 * We just successfully updated FILE (bare filename, no directory).
 * REPOSITORY is the directory for the repository.  This is called
 * after server_register or server_scratch, in the latter case the
 * file is to be removed.  UPDATED indicates whether the file is now
 * up to date (SERVER_UPDATED, yes, SERVER_MERGED, no, SERVER_PATCHED,
 * yes, but file is a diff from user version to repository version).
 */
enum server_updated_arg4 {SERVER_UPDATED, SERVER_MERGED, SERVER_PATCHED};
extern void server_updated
    PROTO((char *file, char *update_dir, char *repository,
	     enum server_updated_arg4 updated, struct stat *,
	     unsigned char *checksum));

/* Set the Entries.Static flag.  */
extern void server_set_entstat PROTO((char *update_dir, char *repository));
/* Clear it.  */
extern void server_clear_entstat PROTO((char *update_dir, char *repository));

/* Set or clear a per-directory sticky tag or date.  */
extern void server_set_sticky PROTO((char *update_dir, char *repository,
				       char *tag,
				       char *date));
/* Send Template response.  */
extern void server_template PROTO ((char *, char *));

extern void server_update_entries
    PROTO((char *file, char *update_dir, char *repository,
	     enum server_updated_arg4 updated));

enum progs {PROG_CHECKIN, PROG_UPDATE};
extern void server_prog PROTO((char *, char *, enum progs));
extern void server_cleanup PROTO((int sig));

#ifdef SERVER_FLOWCONTROL
/* Pause if it's convenient to avoid memory blowout */
extern void server_pause_check PROTO((void));
#endif /* SERVER_FLOWCONTROL */

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

extern int use_unchanged;
