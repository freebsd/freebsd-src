/* $CVSid: @(#)cvs.h 1.86 94/10/22 $	 */

/*
 * basic information used in all source files
 *
 */


#include "config.h"		/* this is stuff found via autoconf */
#include "options.h"		/* these are some larger questions which
				   can't easily be automatically checked
				   for */

/* Changed from if __STDC__ to ifdef __STDC__ because of Sun's acc compiler */

#ifdef __STDC__
#define	PTR	void *
#else
#define	PTR	char *
#endif

/* Add prototype support.  */
#ifndef PROTO
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#define PROTO(ARGS) ARGS
#else
#define PROTO(ARGS) ()
#endif
#endif

#include <stdio.h>

/* Under OS/2, <stdio.h> doesn't define popen()/pclose(). */
#ifdef USE_OWN_POPEN
#include "popen.h"
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
extern void exit ();
extern char *getenv();
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef SERVER_SUPPORT
/* If the system doesn't provide strerror, it won't be declared in
   string.h.  */
char *strerror ();
#endif

#include <fnmatch.h> /* This is supposed to be available on Posix systems */

#include <ctype.h>
#include <pwd.h>
#include <signal.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#else
#ifndef errno
extern int errno;
#endif /* !errno */
#endif /* HAVE_ERRNO_H */

#include "system.h"

#include "hash.h"
#if defined(SERVER_SUPPORT) || defined(CLIENT_SUPPORT)
#include "server.h"
#include "client.h"
#endif

#ifdef MY_NDBM
#include "myndbm.h"
#else
#include <ndbm.h>
#endif /* MY_NDBM */

#include "regex.h"
#include "getopt.h"
#include "wait.h"

#include "rcs.h"


/* XXX - for now this is static */
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define	PATH_MAX MAXPATHLEN+2
#else
#define	PATH_MAX 1024+2
#endif
#endif /* PATH_MAX */

/* just in case this implementation does not define this */
#ifndef L_tmpnam
#define	L_tmpnam	50
#endif


/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 *
 * Definitions for the CVS Administrative directory and the files it contains.
 * Here as #define's to make changing the names a simple task.
 */

#ifdef USE_VMS_FILENAMES
#define CVSADM          "CVS"
#define CVSADM_ENT      "CVS/Entries."
#define CVSADM_ENTBAK   "CVS/Entries.Backup"
#define CVSADM_ENTLOG   "CVS/Entries.Log"
#define CVSADM_ENTSTAT  "CVS/Entries.Static"
#define CVSADM_REP      "CVS/Repository."
#define CVSADM_ROOT     "CVS/Root."
#define CVSADM_CIPROG   "CVS/Checkin.prog"
#define CVSADM_UPROG    "CVS/Update.prog"
#define CVSADM_TAG      "CVS/Tag."
#define CVSADM_NOTIFY   "CVS/Notify."
#define CVSADM_NOTIFYTMP "CVS/Notify.tmp"
#define CVSADM_BASE      "CVS/Base"
#define CVSADM_TEMPLATE "CVS/Template."
#else /* USE_VMS_FILENAMES */
#define	CVSADM		"CVS"
#define	CVSADM_ENT	"CVS/Entries"
#define	CVSADM_ENTBAK	"CVS/Entries.Backup"
#define CVSADM_ENTLOG	"CVS/Entries.Log"
#define	CVSADM_ENTSTAT	"CVS/Entries.Static"
#define	CVSADM_REP	"CVS/Repository"
#define	CVSADM_ROOT	"CVS/Root"
#define	CVSADM_CIPROG	"CVS/Checkin.prog"
#define	CVSADM_UPROG	"CVS/Update.prog"
#define	CVSADM_TAG	"CVS/Tag"
#define CVSADM_NOTIFY	"CVS/Notify"
#define CVSADM_NOTIFYTMP "CVS/Notify.tmp"
/* A directory in which we store base versions of files we currently are
   editing with "cvs edit".  */
#define CVSADM_BASE     "CVS/Base"
/* File which contains the template for use in log messages.  */
#define CVSADM_TEMPLATE "CVS/Template"
#endif /* USE_VMS_FILENAMES */

/* This is the special directory which we use to store various extra
   per-directory information in the repository.  It must be the same as
   CVSADM to avoid creating a new reserved directory name which users cannot
   use, but is a separate #define because if anyone changes it (which I don't
   recommend), one needs to deal with old, unconverted, repositories.
   
   See fileattr.h for details about file attributes, the only thing stored
   in CVSREP currently.  */
#define CVSREP "CVS"

/*
 * Definitions for the CVSROOT Administrative directory and the files it
 * contains.  This directory is created as a sub-directory of the $CVSROOT
 * environment variable, and holds global administration information for the
 * entire source repository beginning at $CVSROOT.
 */
#define	CVSROOTADM		"CVSROOT"
#define	CVSROOTADM_MODULES	"modules"
#define	CVSROOTADM_LOGINFO	"loginfo"
#define	CVSROOTADM_RCSINFO	"rcsinfo"
#define CVSROOTADM_COMMITINFO	"commitinfo"
#define CVSROOTADM_TAGINFO      "taginfo"
#define	CVSROOTADM_EDITINFO	"editinfo"
#define	CVSROOTADM_HISTORY	"history"
#define CVSROOTADM_VALTAGS	"val-tags"
#define	CVSROOTADM_IGNORE	"cvsignore"
#define	CVSROOTADM_CHECKOUTLIST "checkoutlist"
#define CVSROOTADM_WRAPPER	"cvswrappers"
#define CVSROOTADM_NOTIFY	"notify"
#define CVSROOTADM_USERS	"users"

#define CVSNULLREPOS		"Emptydir"	/* an empty directory */

/* Other CVS file names */

/* Files go in the attic if the head main branch revision is dead,
   otherwise they go in the regular repository directories.  The whole
   concept of having an attic is sort of a relic from before death
   support but on the other hand, it probably does help the speed of
   some operations (such as main branch checkouts and updates).  */
#define	CVSATTIC	"Attic"

#define	CVSLCK		"#cvs.lock"
#define	CVSRFL		"#cvs.rfl"
#define	CVSWFL		"#cvs.wfl"
#define CVSRFLPAT	"#cvs.rfl.*"	/* wildcard expr to match read locks */
#define	CVSEXT_LOG	",t"
#define	CVSPREFIX	",,"
#define CVSDOTIGNORE	".cvsignore"
#define CVSDOTWRAPPER   ".cvswrappers"

/* miscellaneous CVS defines */
#define	CVSEDITPREFIX	"CVS: "
#define	CVSLCKAGE	(60*60)		/* 1-hour old lock files cleaned up */
#define	CVSLCKSLEEP	30		/* wait 30 seconds before retrying */
#define	CVSBRANCH	"1.1.1"		/* RCS branch used for vendor srcs */

#ifdef USE_VMS_FILENAMES
#define BAKPREFIX       "_$"
#define DEVNULL         "NLA0:"
#else /* USE_VMS_FILENAMES */
#define	BAKPREFIX	".#"		/* when rcsmerge'ing */
#ifndef DEVNULL
#define	DEVNULL		"/dev/null"
#endif
#endif /* USE_VMS_FILENAMES */

#define	FALSE		0
#define	TRUE		1

/*
 * Special tags. -rHEAD	refers to the head of an RCS file, regardless of any
 * sticky tags. -rBASE	refers to the current revision the user has checked
 * out This mimics the behaviour of RCS.
 */
#define	TAG_HEAD	"HEAD"
#define	TAG_BASE	"BASE"

/* Environment variable used by CVS */
#define	CVSREAD_ENV	"CVSREAD"	/* make files read-only */
#define	CVSREAD_DFLT	FALSE		/* writable files by default */

#define	RCSBIN_ENV	"RCSBIN"	/* RCS binary directory */
/* #define	RCSBIN_DFLT		   Set by config.h */

#define	EDITOR1_ENV	"CVSEDITOR"	/* which editor to use */
#define	EDITOR2_ENV	"VISUAL"	/* which editor to use */
#define	EDITOR3_ENV	"EDITOR"	/* which editor to use */
/* #define	EDITOR_DFLT		   Set by config.h */

#define	CVSROOT_ENV	"CVSROOT"	/* source directory root */
#define	CVSROOT_DFLT	NULL		/* No dflt; must set for checkout */

#define	IGNORE_ENV	"CVSIGNORE"	/* More files to ignore */
#define WRAPPER_ENV     "CVSWRAPPERS"   /* name of the wrapper file */

#define	CVSUMASK_ENV	"CVSUMASK"	/* Effective umask for repository */
/* #define	CVSUMASK_DFLT		   Set by config.h */

/*
 * If the beginning of the Repository matches the following string, strip it
 * so that the output to the logfile does not contain a full pathname.
 *
 * If the CVSROOT environment variable is set, it overrides this define.
 */
#define	REPOS_STRIP	"/master/"

/*
 * The maximum number of files per each CVS directory. This is mainly for
 * sizing arrays statically rather than dynamically.  3000 seems plenty for
 * now.
 */
#define	MAXFILEPERDIR	3000
#define	MAXLINELEN	5000		/* max input line from a file */
#define	MAXPROGLEN	30000		/* max program length to system() */
#define	MAXLISTLEN	40000		/* For [A-Z]list holders */
#define MAXDATELEN	50		/* max length for a date */

/* structure of a entry record */
struct entnode
{
    char *user;
    char *version;
    char *timestamp;
    char *options;
    char *tag;
    char *date;
    char *conflict;
};
typedef struct entnode Entnode;

/* The type of request that is being done in do_module() */
enum mtype
{
    CHECKOUT, TAG, PATCH, EXPORT
};

/*
 * defines for Classify_File() to determine the current state of a file.
 * These are also used as types in the data field for the list we make for
 * Update_Logfile in commit, import, and add.
 */
enum classify_type
{
    T_UNKNOWN = 1,			/* no old-style analog existed	 */
    T_CONFLICT,				/* C (conflict) list		 */
    T_NEEDS_MERGE,			/* G (needs merging) list	 */
    T_MODIFIED,				/* M (needs checked in) list 	 */
    T_CHECKOUT,				/* O (needs checkout) list	 */
    T_ADDED,				/* A (added file) list		 */
    T_REMOVED,				/* R (removed file) list	 */
    T_REMOVE_ENTRY,			/* W (removed entry) list	 */
    T_UPTODATE,				/* File is up-to-date		 */
#ifdef SERVER_SUPPORT
    T_PATCH,				/* P Like C, but can patch	 */
#endif
    T_TITLE				/* title for node type 		 */
};
typedef enum classify_type Ctype;

/*
 * a struct vers_ts contains all the information about a file including the
 * user and rcs file names, and the version checked out and the head.
 *
 * this is usually obtained from a call to Version_TS which takes a tag argument
 * for the RCS file if desired
 */
struct vers_ts
{
    /* rcs version user file derives from, from CVS/Entries.
     * it can have the following special values:
     *    empty = no user file
     *    0 = user file is new
     *    -vers = user file to be removed.  */
    char *vn_user;

    /* Numeric revision number corresponding to ->vn_tag (->vn_tag
       will often be symbolic).  */
    char *vn_rcs;
    /* If ->tag corresponds to a tag which really exists in this file,
       this is just a copy of ->tag.  If not, this is either NULL or
       the head revision.  (Or something like that, see RCS_getversion
       and friends).  */
    char *vn_tag;

    /* This is the timestamp from stating the file in the working directory.
       It is NULL if there is no file in the working directory.  */
    char *ts_user;
    /* Timestamp from CVS/Entries.  For the server, ts_user and ts_rcs
       are computed in a slightly different way, but the fact remains that
       if they are equal the file in the working directory is unmodified
       and if they differ it is modified.  */
    char *ts_rcs;

    /* Options from CVS/Entries (keyword expansion).  */
    char *options;

    /* If non-NULL, there was a conflict (or merely a merge?  See merge_file)
       and the time stamp in this field is the time stamp of the working
       directory file which was created with the conflict markers in it.
       This is from CVS/Entries.  */
    char *ts_conflict;

    /* Tag specified on the command line, or if none, tag stored in
       CVS/Entries.  */
    char *tag;
    /* Date specified on the command line, or if none, date stored in
       CVS/Entries.  */
    char *date;

    /* Pointer to entries file node  */
    Entnode *entdata;

    /* Pointer to parsed src file info */
    RCSNode *srcfile;
};
typedef struct vers_ts Vers_TS;

/*
 * structure used for list-private storage by Entries_Open() and
 * Version_TS().
 */
struct stickydirtag
{
    int aflag;
    char *tag;
    char *date;
    char *options;
};

/* Flags for find_{names,dirs} routines */
#define W_LOCAL			0x01	/* look for files locally */
#define W_REPOS			0x02	/* look for files in the repository */
#define W_ATTIC			0x04	/* look for files in the attic */

/* Flags for return values of direnter procs for the recursion processor */
enum direnter_type
{
    R_PROCESS = 1,			/* process files and maybe dirs */
    R_SKIP_FILES,			/* don't process files in this dir */
    R_SKIP_DIRS,			/* don't process sub-dirs */
    R_SKIP_ALL				/* don't process files or dirs */
};
typedef enum direnter_type Dtype;

extern char *program_name, *program_path, *command_name;
extern char *Rcsbin, *Editor, *CVSroot;
extern char *CVSADM_Root;
extern int cvsadmin_root;
extern char *CurDir;
extern int really_quiet, quiet;
extern int use_editor;
extern int cvswrite;
extern mode_t cvsumask;

extern int trace;		/* Show all commands */
extern int noexec;		/* Don't modify disk anywhere */
extern int logoff;		/* Don't write history entry */

extern char hostname[];

/* Externs that are included directly in the CVS sources */

int RCS_settag PROTO((const char *, const char *, const char *));
int RCS_deltag PROTO((const char *, const char *, int));
int RCS_setbranch PROTO((const char *, const char *));
int RCS_lock PROTO((const char *, const char *, int));
int RCS_unlock PROTO((const char *, const char *, int));
int RCS_merge PROTO((const char *, const char *, const char *, const char *));
int RCS_checkout PROTO ((char *rcsfile, char *workfile, char *tag,
			 char *options,
                         char *sout, int flags, int noerr));
/* Flags used by RCS_* functions.  See the description of the individual
   functions for which flags mean what for each function.  */
#define RCS_FLAGS_LOCK 1
#define RCS_FLAGS_FORCE 2
#define RCS_FLAGS_DEAD 4
#define RCS_FLAGS_QUIET 8
#define RCS_FLAGS_MODTIME 16
int RCS_checkin PROTO ((char *rcsfile, char *workfile, char *message,
			char *rev, int flags, int noerr));



#include "error.h"

DBM *open_module PROTO((void));
FILE *open_file PROTO((const char *, const char *));
List *Find_Directories PROTO((char *repository, int which));
void Entries_Close PROTO((List *entries));
List *Entries_Open PROTO((int aflag));
char *Make_Date PROTO((char *rawdate));
char *Name_Repository PROTO((char *dir, char *update_dir));
char *Name_Root PROTO((char *dir, char *update_dir));
void Create_Root PROTO((char *dir, char *rootdir));
int same_directories PROTO((char *dir1, char *dir2));
char *Short_Repository PROTO((char *repository));
char *gca PROTO((char *rev1, char *rev2));
char *getcaller PROTO((void));
char *time_stamp PROTO((char *file));
char *xmalloc PROTO((size_t bytes));
void *xrealloc PROTO((void *ptr, size_t bytes));
char *xstrdup PROTO((const char *str));
void strip_trailing_newlines PROTO((char *str));
int No_Difference PROTO((char *file, Vers_TS * vers, List * entries,
			 char *repository, char *update_dir));
typedef	int (*CALLPROC)	PROTO((char *repository, char *value));
int Parse_Info PROTO((char *infofile, char *repository, CALLPROC callproc, int all));
int Reader_Lock PROTO((char *xrepository));
typedef	RETSIGTYPE (*SIGCLEANUPPROC)	PROTO(());
int SIG_register PROTO((int sig, SIGCLEANUPPROC sigcleanup));
int Writer_Lock PROTO((List * list));
int isdir PROTO((const char *file));
int isfile PROTO((const char *file));
int islink PROTO((const char *file));
int isreadable PROTO((const char *file));
int iswritable PROTO((const char *file));
int isaccessible PROTO((const char *file, const int mode));
int isabsolute PROTO((const char *filename));
char *last_component PROTO((char *path));
char *get_homedir PROTO ((void));

int numdots PROTO((const char *s));
int unlink_file PROTO((const char *f));
int link_file PROTO ((const char *from, const char *to));
int unlink_file_dir PROTO((const char *f));
int update PROTO((int argc, char *argv[]));
int xcmp PROTO((const char *file1, const char *file2));
int yesno PROTO((void));
void *valloc PROTO((size_t bytes));
time_t get_date PROTO((char *date, struct timeb *now));
void Create_Admin PROTO((char *dir, char *update_dir,
			 char *repository, char *tag, char *date));

void Lock_Cleanup PROTO((void));

/* Writelock an entire subtree, well the part specified by ARGC, ARGV, LOCAL,
   and AFLAG, anyway.  */
void lock_tree_for_write PROTO ((int argc, char **argv, int local, int aflag));

/* Remove locks set by lock_tree_for_write.  Currently removes readlocks
   too.  */
void lock_tree_cleanup PROTO ((void));

void ParseTag PROTO((char **tagp, char **datep));
void Scratch_Entry PROTO((List * list, char *fname));
void WriteTag PROTO((char *dir, char *tag, char *date));
void cat_module PROTO((int status));
void check_entries PROTO((char *dir));
void close_module PROTO((DBM * db));
void copy_file PROTO((const char *from, const char *to));
void (*error_set_cleanup PROTO((void (*) (void)))) PROTO ((void));
void fperror PROTO((FILE * fp, int status, int errnum, char *message,...));
void free_names PROTO((int *pargc, char *argv[]));
void freevers_ts PROTO((Vers_TS ** versp));

extern int ign_name PROTO ((char *name));
void ign_add PROTO((char *ign, int hold));
void ign_add_file PROTO((char *file, int hold));
void ign_setup PROTO((void));
void ign_dir_add PROTO((char *name));
int ignore_directory PROTO((char *name));
typedef void (*Ignore_proc) PROTO ((char *, char *));
extern void ignore_files PROTO ((List *, char *, Ignore_proc));
extern int ign_inhibit_server;
extern int ign_case;

#include "update.h"

void line2argv PROTO((int *pargc, char *argv[], char *line));
void make_directories PROTO((const char *name));
void make_directory PROTO((const char *name));
void rename_file PROTO((const char *from, const char *to));
/* Expand wildcards in each element of (ARGC,ARGV).  This is according to the
   files which exist in the current directory, and accordingly to OS-specific
   conventions regarding wildcard syntax.  It might be desirable to change the
   former in the future (e.g. "cvs status *.h" including files which don't exist
   in the working directory).  The result is placed in *PARGC and *PARGV;
   the *PARGV array itself and all the strings it contains are newly
   malloc'd.  It is OK to call it with PARGC == &ARGC or PARGV == &ARGV.  */
extern void expand_wild PROTO ((int argc, char **argv, 
                                int *pargc, char ***pargv));

void strip_path PROTO((char *path));
void strip_trailing_slashes PROTO((char *path));
void update_delproc PROTO((Node * p));
void usage PROTO((const char *const *cpp));
void xchmod PROTO((char *fname, int writable));
char *xgetwd PROTO((void));
int Checkin PROTO((int type, char *file, char *update_dir,
		   char *repository, char *rcs, char *rev,
		   char *tag, char *options, char *message, List *entries));
Ctype Classify_File PROTO((char *file, char *tag, char *date, char *options,
		     int force_tag_match, int aflag, char *repository,
		     List *entries, RCSNode *rcsnode, Vers_TS **versp,
		     char *update_dir, int pipeout));
List *Find_Names PROTO((char *repository, int which, int aflag,
		  List ** optentries));
void Register PROTO((List * list, char *fname, char *vn, char *ts,
	       char *options, char *tag, char *date, char *ts_conflict));
void Update_Logfile PROTO((char *repository, char *xmessage, char *xrevision,
		     FILE * xlogfp, List * xchanges));
Vers_TS *Version_TS PROTO((char *repository, char *options, char *tag,
		     char *date, char *user, int force_tag_match,
		     int set_time, List * entries, RCSNode * rcs));
void do_editor PROTO((char *dir, char **messagep,
		      char *repository, List * changes));

typedef	int (*CALLBACKPROC)	PROTO((int *pargc, char *argv[], char *where,
	char *mwhere, char *mfile, int horten, int local_specified,
	char *omodule, char *msg));

/* This is the structure that the recursion processor passes to the
   fileproc to tell it about a particular file.  */
struct file_info
{
    /* Name of the file, without any directory component.  */
    char *file;

    /* Name of the directory we are in, relative to the directory in
       which this command was issued.  We have cd'd to this directory
       (either in the working directory or in the repository, depending
       on which sort of recursion we are doing).  If we are in the directory
       in which the command was issued, this is "".  */
    char *update_dir;

    /* update_dir and file put together, with a slash between them as
       necessary.  This is the proper way to refer to the file in user
       messages.  */
    char *fullname;

    /* Name of the directory corresponding to the repository which contains
       this file.  */
    char *repository;

    /* The pre-parsed entries for this directory.  */
    List *entries;

    RCSNode *rcs;
};

typedef	int (*FILEPROC)		PROTO((struct file_info *finfo));
typedef	int (*FILESDONEPROC)	PROTO((int err, char *repository, char *update_dir));
typedef	Dtype (*DIRENTPROC)	PROTO((char *dir, char *repos, char *update_dir));
typedef	int (*DIRLEAVEPROC)	PROTO((char *dir, int err, char *update_dir));

extern int mkmodules PROTO ((char *dir));
extern int init PROTO ((int argc, char **argv));

int do_module PROTO((DBM * db, char *mname, enum mtype m_type, char *msg,
		CALLBACKPROC callback_proc, char *where, int shorten,
		int local_specified, int run_module_prog, char *extra_arg));
int do_recursion PROTO((FILEPROC xfileproc, FILESDONEPROC xfilesdoneproc,
		  DIRENTPROC xdirentproc, DIRLEAVEPROC xdirleaveproc,
		  Dtype xflags, int xwhich, int xaflag, int xreadlock,
		  int xdosrcs));
void history_write PROTO((int type, char *update_dir, char *revs, char *name,
		    char *repository));
int start_recursion PROTO((FILEPROC fileproc, FILESDONEPROC filesdoneproc,
		     DIRENTPROC direntproc, DIRLEAVEPROC dirleaveproc,
		     int argc, char *argv[], int local, int which,
		     int aflag, int readlock, char *update_preload,
		     int dosrcs, int wd_is_repos));
void SIG_beginCrSect PROTO((void));
void SIG_endCrSect PROTO((void));
void read_cvsrc PROTO((int *argc, char ***argv, char *cmdname));

char *make_message_rcslegal PROTO((char *message));

/* flags for run_exec(), the fast system() for CVS */
#define	RUN_NORMAL		0x0000	/* no special behaviour */
#define	RUN_COMBINED		0x0001	/* stdout is duped to stderr */
#define	RUN_REALLY		0x0002	/* do the exec, even if noexec is on */
#define	RUN_STDOUT_APPEND	0x0004	/* append to stdout, don't truncate */
#define	RUN_STDERR_APPEND	0x0008	/* append to stderr, don't truncate */
#define	RUN_SIGIGNORE		0x0010	/* ignore interrupts for command */
#define	RUN_TTY		(char *)0	/* for the benefit of lint */

void run_arg PROTO((const char *s));
void run_print PROTO((FILE * fp));
#ifdef HAVE_VPRINTF
void run_setup PROTO((const char *fmt,...));
void run_args PROTO((const char *fmt,...));
#else
void run_setup ();
void run_args ();
#endif
int run_exec PROTO((char *stin, char *stout, char *sterr, int flags));

/* other similar-minded stuff from run.c.  */
FILE *run_popen PROTO((const char *, const char *));
int piped_child PROTO((char **, int *, int *));
void close_on_exec PROTO((int));
int filter_stream_through_program PROTO((int, int, char **, pid_t *));

pid_t waitpid PROTO((pid_t, int *, int));

/* Wrappers.  */

typedef enum { WRAP_MERGE, WRAP_COPY } WrapMergeMethod;
typedef enum { WRAP_TOCVS, WRAP_FROMCVS, WRAP_CONFLICT } WrapMergeHas;

void  wrap_setup PROTO((void));
int   wrap_name_has PROTO((const char *name,WrapMergeHas has));
char *wrap_tocvs_process_file PROTO((const char *fileName));
int   wrap_merge_is_copy PROTO((const char *fileName));
char *wrap_fromcvs_process_file PROTO((const char *fileName));
void wrap_add_file PROTO((const char *file,int temp));
void wrap_add PROTO((char *line,int temp));

/* Pathname expansion */
char *expand_path PROTO((char *name, char *file, int line));

/* User variables.  */
extern List *variable_list;

extern void variable_set PROTO ((char *nameval));

int watch PROTO ((int argc, char **argv));
int edit PROTO ((int argc, char **argv));
int unedit PROTO ((int argc, char **argv));
int editors PROTO ((int argc, char **argv));
int watchers PROTO ((int argc, char **argv));
extern int annotate PROTO ((int argc, char **argv));

#if defined(AUTH_CLIENT_SUPPORT) || defined(AUTH_SERVER_SUPPORT)
char *scramble PROTO ((char *str));
char *descramble PROTO ((char *str));
#endif /* AUTH_CLIENT_SUPPORT || AUTH_SERVER_SUPPORT */

extern void tag_check_valid PROTO ((char *, int, char **, int, int, char *));

extern void cvs_output PROTO ((char *, size_t));
extern void cvs_outerr PROTO ((char *, size_t));
