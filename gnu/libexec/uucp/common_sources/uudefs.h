/* uudefs.h
   Miscellaneous definitions for the UUCP package.

   Copyright (C) 1991, 1992, 1993, 1995 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#if ANSI_C
/* These structures are used in prototypes but are not defined in this
   header file.  */
struct uuconf_system;
struct uuconf_timespan;
#endif

/* The tlog enumeration holds the different types of logging.  */
enum tlog
{
  /* Normal log entry.  */
  LOG_NORMAL,
  /* Error log entry.  */
  LOG_ERROR,
  /* Fatal log entry.  */
  LOG_FATAL
#if DEBUG > 1
    ,
  /* Debugging log entry.  */
  LOG_DEBUG,
  /* Start debugging log entry.  */
  LOG_DEBUG_START,
  /* Continue debugging log entry.  */
  LOG_DEBUG_CONTINUE,
  /* End debugging log entry.  */
  LOG_DEBUG_END
#endif
};

/* The tstatus_type enumeration holds the kinds of status information
   we put in the status file.  The order of entries here corresponds
   to the order of entries in the azStatus array.  */
enum tstatus_type
{
  /* Conversation complete.  */
  STATUS_COMPLETE,
  /* Port unavailable.  */
  STATUS_PORT_FAILED,
  /* Dial failed.  */
  STATUS_DIAL_FAILED,
  /* Login failed.  */
  STATUS_LOGIN_FAILED,
  /* Handshake failed.  */
  STATUS_HANDSHAKE_FAILED,
  /* Failed after logging in.  */
  STATUS_FAILED,
  /* Talking to remote system.  */
  STATUS_TALKING,
  /* Wrong time to call.  */
  STATUS_WRONG_TIME,
  /* Number of status values.  */
  STATUS_VALUES
};

/* An array to convert status entries to strings.  If more status entries
   are added, this array must be extended.  */
extern const char *azStatus[];

/* The sstatus structure holds the contents of a system status file.  */
struct sstatus
{
  /* Current status of conversation.  */
  enum tstatus_type ttype;
  /* Number of failed retries.  */
  int cretries;
  /* Time of last call in seconds since epoch (determined by
     ixsysdep_time).  */
  long ilast;
  /* Number of seconds until a retry is permitted.  */
  int cwait;
  /* String in status file.  Only used when reading status file, not
     when writing.  May be NULL.  Should be freed with ubuffree.  */
  char *zstring;
};

/* How long we have to wait for the next call, given the number of retries
   we have already made.  This should probably be configurable.  */
#define CRETRY_WAIT(c) ((c) * 10 * 60)

/* The scmd structure holds a complete UUCP command.  */
struct scmd
{
  /* Command ('S' for send, 'R' for receive, 'X' for execute, 'E' for
     simple execution, 'H' for hangup, 'Y' for hangup confirm, 'N' for
     hangup deny).  */
  char bcmd;
  /* Grade of the command ('\0' if from remote system).  */
  char bgrade;
  /* Sequence handle for fsysdep_did_work.  */
  pointer pseq;
  /* File name to transfer from.  */
  const char *zfrom;
  /* File name to transfer to.  */
  const char *zto;
  /* User who requested transfer.  */
  const char *zuser;
  /* Options.  */
  const char *zoptions;
  /* Temporary file name ('S' and 'E').  */
  const char *ztemp;
  /* Mode to give newly created file ('S' and 'E').  */
  unsigned int imode;
  /* User to notify on remote system (optional; 'S' and 'E').  */
  const char *znotify;
  /* File size (-1 if not supplied) ('S', 'E' and 'R').  */
  long cbytes;
  /* Command to execute ('E').  */
  const char *zcmd;
  /* Position to restart from ('R').  */
  long ipos;
};

#if DEBUG > 1

/* We allow independent control over several different types of
   debugging output, using a bit string with individual bits dedicated
   to particular debugging types.  */

/* The bit string is stored in iDebug.  */
extern int iDebug;

/* Debug abnormal events.  */
#define DEBUG_ABNORMAL (01)
/* Debug chat scripts.  */
#define DEBUG_CHAT (02)
/* Debug initial handshake.  */
#define DEBUG_HANDSHAKE (04)
/* Debug UUCP protocol.  */
#define DEBUG_UUCP_PROTO (010)
/* Debug protocols.  */
#define DEBUG_PROTO (020)
/* Debug port actions.  */
#define DEBUG_PORT (040)
/* Debug configuration files.  */
#define DEBUG_CONFIG (0100)
/* Debug spool directory actions.  */
#define DEBUG_SPOOLDIR (0200)
/* Debug executions.  */
#define DEBUG_EXECUTE (0400)
/* Debug incoming data.  */
#define DEBUG_INCOMING (01000)
/* Debug outgoing data.  */
#define DEBUG_OUTGOING (02000)

/* Maximum possible value for iDebug.  */
#define DEBUG_MAX (03777)

/* Intializer for array of debug names.  The index of the name in the
   array is the corresponding bit position in iDebug.  We only check
   for prefixes, so these names only need to be long enough to
   distinguish each name from every other.  The last entry must be
   NULL.  The string "all" is also recognized to turn on all
   debugging.  */
#define DEBUG_NAMES \
  { "a", "ch", "h", "u", "pr", "po", "co", "s", "e", "i", "o", NULL }

/* The prefix to use to turn off all debugging.  */
#define DEBUG_NONE "n"

/* Check whether a particular type of debugging is being done.  */
#define FDEBUGGING(i) ((iDebug & (i)) != 0)

/* These macros are used to output debugging information.  I use
   several different macros depending on the number of arguments
   because no macro can take a variable number of arguments and I
   don't want to use double parentheses.  */
#define DEBUG_MESSAGE0(i, z) \
  do { if (FDEBUGGING (i)) ulog (LOG_DEBUG, (z)); } while (0)
#define DEBUG_MESSAGE1(i, z, a1) \
  do { if (FDEBUGGING (i)) ulog (LOG_DEBUG, (z), (a1)); } while (0)
#define DEBUG_MESSAGE2(i, z, a1, a2) \
  do { if (FDEBUGGING (i)) ulog (LOG_DEBUG, (z), (a1), (a2)); } while (0)
#define DEBUG_MESSAGE3(i, z, a1, a2, a3) \
  do \
    { \
      if (FDEBUGGING (i)) \
	ulog (LOG_DEBUG, (z), (a1), (a2), (a3)); \
    } \
  while (0)
#define DEBUG_MESSAGE4(i, z, a1, a2, a3, a4) \
  do \
    { \
      if (FDEBUGGING (i)) \
	ulog (LOG_DEBUG, (z), (a1), (a2), (a3), (a4)); \
    } \
  while (0)

#else /* DEBUG <= 1 */

/* If debugging information is not being compiled, provide versions of
   the debugging macros which just disappear.  */
#define DEBUG_MESSAGE0(i, z)
#define DEBUG_MESSAGE1(i, z, a1)
#define DEBUG_MESSAGE2(i, z, a1, a2)
#define DEBUG_MESSAGE3(i, z, a1, a2, a3)
#define DEBUG_MESSAGE4(i, z, a1, a2, a3, a4)

#endif /* DEBUG <= 1 */

/* Functions.  */

/* Given an unknown system name, return information for an unknown
   system.  If unknown systems are not permitted, this returns FALSE.
   Otherwise, it translates the name as necessary for the spool
   directory, and fills in *qsys.  */
extern boolean funknown_system P((pointer puuconf, const char *zsystem,
				  struct uuconf_system *qsys));

/* See whether a file belongs in the spool directory.  */
extern boolean fspool_file P((const char *zfile));

/* See if the current time matches a time span.  If not, return FALSE.
   Otherwise, return TRUE and set *pival and *pcretry to the values
   from the matching element of the span.  */
extern boolean ftimespan_match P((const struct uuconf_timespan *qspan,
				  long *pival, int *pcretry));

/* Remove all occurrences of the local system name followed by an
   exclamation point from the start of the argument.  Return the
   possibly shortened argument.  */
extern char *zremove_local_sys P((struct uuconf_system *qlocalsys,
				  char *z));

/* Determine the maximum size that may ever be transferred, given a
   timesize span.  If there are any time gaps larger than 1 hour not
   described by the timesize span, this returns -1.  Otherwise it
   returns the largest size that may be transferred at some time.  */
extern long cmax_size_ever P((const struct uuconf_timespan *qtimesize));

/* Send mail about a file transfer.  */
extern boolean fmail_transfer P((boolean fok, const char *zuser,
				 const char *zmail, const char *zwhy,
				 const char *zfrom, const char *zfromsys,
				 const char *zto, const char *ztosys,
				 const char *zsaved));

/* See whether a file is in one of a list of directories.  The zpubdir
   argument is used to pass the directory names to zsysdep_local_file.
   If fcheck is FALSE, this does not check accessibility.  Otherwise,
   if freadable is TRUE, the user zuser must have read access to the
   file and all appropriate directories; if freadable is FALSE zuser
   must have write access to the appropriate directories.  The zuser
   argument may be NULL, in which case all users must have the
   appropriate access (this is used for a remote request).  */
extern boolean fin_directory_list P((const char *zfile,
				     char **pzdirs,
				     const char *zpubdir,
				     boolean fcheck,
				     boolean freadable,
				     const char *zuser));

/* Parse a command string.  */
extern boolean fparse_cmd P((char *zcmd, struct scmd *qcmd));

/* Make a log entry.  */
#ifdef __GNUC__
#define GNUC_VERSION __GNUC__
#else
#define GNUC_VERSION 0
#endif

#if ANSI_C && HAVE_VFPRINTF
extern void ulog P((enum tlog ttype, const char *zfmt, ...))
#if GNUC_VERSION > 1
#ifdef __printf0like
     __printf0like (2, 3)
#else
     __attribute__ ((format (printf, 2, 3)))
#endif
#endif
     ;
#else
extern void ulog ();
#endif

#undef GNUC_VERSION

/* Report an error returned by one of the uuconf routines.  */
extern void ulog_uuconf P((enum tlog ttype, pointer puuconf,
			   int iuuconf));

/* Set the function to call if a fatal error occurs.  */
extern void ulog_fatal_fn P((void (*pfn) P((void))));

/* If ffile is TRUE, send log entries to the log file rather than to
   stderr.  */
extern void ulog_to_file P((pointer puuconf, boolean ffile));

/* Set the ID number used by the logging functions.  */
extern void ulog_id P((int iid));

/* Set the system name used by the logging functions.  */
extern void ulog_system P((const char *zsystem));

/* Set the system and user name used by the logging functions.  */
extern void ulog_user P((const char *zuser));

/* Set the device name used by the logging functions.  */
extern void ulog_device P((const char *zdevice));

/* Close the log file.  */
extern void ulog_close P((void));

/* Make an entry in the statistics file.  */
extern void ustats P((boolean fsucceeded, const char *zuser,
		      const char *zsystem, boolean fsent,
		      long cbytes, long csecs, long cmicros,
		      boolean fcaller));

/* Close the statistics file.  */
extern void ustats_close P((void));

#if DEBUG > 1
/* A debugging routine to output a buffer.  This outputs zhdr, the
   buffer length clen, and the contents of the buffer in quotation
   marks.  */
extern void udebug_buffer P((const char *zhdr, const char *zbuf,
			     size_t clen));

/* A debugging routine to make a readable version of a character.
   This takes a buffer at least 5 bytes long, and returns the length
   of the string it put into it (not counting the null byte).  */
extern size_t cdebug_char P((char *z, int ichar));

/* Parse a debugging option string.  This can either be a number or a
   comma separated list of debugging names.  This returns a value for
   iDebug.  */
extern int idebug_parse P((const char *));

#endif /* DEBUG <= 1 */

/* Copy one file to another.  */
extern boolean fcopy_file P((const char *zfrom, const char *zto,
			     boolean fpublic, boolean fmkdirs,
			     boolean fsignals));

/* Copy an open file to another.  */
extern boolean fcopy_open_file P((openfile_t efrom, const char *zto,
				  boolean fpublic, boolean fmkdirs,
				  boolean fsignals));

/* Translate escape sequences in a buffer, leaving the result in the
   same buffer and returning the length.  */
extern size_t cescape P((char *zbuf));

/* Get a buffer to hold a string of a given size.  The buffer should
   be freed with ubuffree.  */
extern char *zbufalc P((size_t csize));

/* Call zbufalc to allocate a buffer and copy a string into it.  */
extern char *zbufcpy P((const char *z));

/* Free up a buffer returned by zbufalc or zbufcpy.  */
extern void ubuffree P((char *z));

/* Allocate memory without fail.  */
extern pointer xmalloc P((size_t));

/* Realloc memory without fail.  */
extern pointer xrealloc P((pointer, size_t));

/* Free memory (accepts NULL pointers, which some libraries erroneously
   do not).  */
extern void xfree P((pointer));

/* Global variables.  */

/* The name of the program being run.  Set from argv[0].  */
extern const char *zProgram;

/* When a signal occurs, the signal handlers sets the appropriate
   element of the arrays afSignal and afLog_signal to TRUE.  The
   afSignal array is used to check whether a signal occurred.  The
   afLog_signal array tells ulog to log the signal; ulog will clear
   the element after logging it, which means that if a signal comes in
   at just the right moment it will not be logged.  It will always be
   recorded in afSignal, though.  At the moment we handle 5 signals:
   SIGHUP, SIGINT, SIGQUIT, SIGTERM and SIGPIPE (the Unix code also
   handles SIGALRM).  If we want to handle more, the afSignal array
   must be extended; I see little point to handling any of the other
   ANSI C or POSIX signals, as they are either unlikely to occur
   (SIGABRT, SIGUSR1) or nearly impossible to handle cleanly (SIGILL,
   SIGSEGV).  SIGHUP is only logged if fLog_sighup is TRUE.  */
#define INDEXSIG_SIGHUP (0)
#define INDEXSIG_SIGINT (1)
#define INDEXSIG_SIGQUIT (2)
#define INDEXSIG_SIGTERM (3)
#define INDEXSIG_SIGPIPE (4)
#define INDEXSIG_COUNT (5)

extern volatile sig_atomic_t afSignal[INDEXSIG_COUNT];
extern volatile sig_atomic_t afLog_signal[INDEXSIG_COUNT];
extern boolean fLog_sighup;

/* The names of the signals to use in error messages, as an
   initializer for an array.  */
#define INDEXSIG_NAMES \
  { "hangup", "interrupt", "quit", "termination", "SIGPIPE" }

/* Check to see whether we've received a signal.  It would be nice if
   we could use a single variable for this, but we sometimes want to
   clear our knowledge of a signal and that would cause race
   conditions (clearing a single element of the array is not a race
   assuming that we don't care about a particular signal, even if it
   occurs after we've examined the array).  */
#define FGOT_SIGNAL() \
  (afSignal[INDEXSIG_SIGHUP] || afSignal[INDEXSIG_SIGINT] \
   || afSignal[INDEXSIG_SIGQUIT] || afSignal[INDEXSIG_SIGTERM] \
   || afSignal[INDEXSIG_SIGPIPE])

/* If we get a SIGINT in uucico, we continue the current communication
   session but don't start any new ones.  This macros checks for any
   signal other than SIGINT, which means we should get out
   immediately.  */
#define FGOT_QUIT_SIGNAL() \
  (afSignal[INDEXSIG_SIGHUP] || afSignal[INDEXSIG_SIGQUIT] \
   || afSignal[INDEXSIG_SIGTERM] || afSignal[INDEXSIG_SIGPIPE])

/* Device name to log.  This is set by fconn_open.  It may be NULL.  */
extern char *zLdevice;

/* If not NULL, ulog calls this function before outputting anything.
   This is used to support cu.  */
extern void (*pfLstart) P((void));

/* If not NULL, ulog calls this function after outputting everything.
   This is used to support cu.  */
extern void (*pfLend) P((void));
