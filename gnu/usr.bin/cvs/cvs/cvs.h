/* @(#)cvs.h 1.72 92/03/31	 */

#include "system.h"
#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include "hash.h"
#include "rcs.h"
#include "regex.h"
#include "fnmatch.h"
#include "getopt.h"
#include "wait.h"
#include "config.h"
#ifdef MY_NDBM
#include "myndbm.h"
#else
#include <ndbm.h>
#endif				/* !MY_NDBM */

/* XXX - for now this is static */
#undef PATH_MAX
#ifdef MAXPATHLEN
#define	PATH_MAX MAXPATHLEN+2
#else
#define	PATH_MAX 1024+2
#endif

/* just in case this implementation does not define this */
#ifndef L_tmpnam
#define	L_tmpnam	50
#endif

#if __STDC__
#define	CONST	const
#define	PTR	void *
#else
#define	CONST
#define	PTR	char *
#endif

/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.3 kit.
 * 
 * Definitions for the CVS Administrative directory and the files it contains.
 * Here as #define's to make changing the names a simple task.
 */
#define	CVSADM		"CVS"
#define	CVSADM_ENT	"CVS/Entries"
#define	CVSADM_ENTBAK	"CVS/Entries.Backup"
#define	CVSADM_ENTSTAT	"CVS/Entries.Static"
#define	CVSADM_REP	"CVS/Repository"
#define	CVSADM_CIPROG	"CVS/Checkin.prog"
#define	CVSADM_UPROG	"CVS/Update.prog"
#define	CVSADM_TAG	"CVS/Tag"

/*
 * The following are obsolete and are maintained here only so that they can be
 * cleaned up during the transition
 */
#define	OCVSADM		"CVS.adm"	/* for CVS 1.2 and earlier */
#define	CVSADM_FILE	"CVS/Files"
#define	CVSADM_MOD	"CVS/Mod"

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
#define	CVSROOTADM_EDITINFO	"editinfo"
#define	CVSROOTADM_HISTORY	"history"
#define	CVSROOTADM_IGNORE	"cvsignore"
#define CVSNULLREPOS		"Emptydir"	/* an empty directory */

/* support for the modules file (CVSROOTADM_MODULES) */
#define	CVSMODULE_OPTS	"ad:i:lo:s:t:u:"/* options in modules file */
#define CVSMODULE_SPEC	'&'		/* special delimiter */

/*
 * The following are obsolete and are maintained here only so that they can be
 * cleaned up during the transition
 */
#define	OCVSROOTADM		"CVSROOT.adm"	/* for CVS 1.2 and earlier */

/* Other CVS file names */
#define	CVSATTIC	"Attic"
#define	CVSLCK		"#cvs.lock"
#define	CVSTFL		"#cvs.tfl"
#define	CVSRFL		"#cvs.rfl"
#define	CVSWFL		"#cvs.wfl"
#define	CVSEXT_OPT	",p"
#define	CVSEXT_LOG	",t"
#define	CVSPREFIX	",,"
#define CVSDOTIGNORE	".cvsignore"

/* miscellaneous CVS defines */
#define	CVSEDITPREFIX	"CVS: "
#define	CVSLCKAGE	(60*60)		/* 1-hour old lock files cleaned up */
#define	CVSLCKSLEEP	30		/* wait 30 seconds before retrying */
#define	CVSBRANCH	"1.1.1"		/* RCS branch used for vendor srcs */
#define	BAKPREFIX	".#"		/* when rcsmerge'ing */
#define	DEVNULL		"/dev/null"

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

#define	EDITOR_ENV	"EDITOR"	/* which editor to use */
/* #define	EDITOR_DFLT		   Set by config.h */

#define	CVSROOT_ENV	"CVSROOT"	/* source directory root */
#define	CVSROOT_DFLT	NULL		/* No dflt; must set for checkout */

#define	IGNORE_ENV	"CVSIGNORE"	/* More files to ignore */

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
#define	MAXMESGLEN	10000		/* max RCS log message size */
#define MAXDATELEN	50		/* max length for a date */

/* The type of request that is being done in do_module() */
enum mtype
{
    CHECKOUT, TAG, PATCH
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
    char *vn_user;			/* rcs version user file derives from
					 * it can have the following special
					 * values: 
					 *    empty = no user file	
					 *    0 = user file is new
					 *    -vers = user file to be removed */
    char *vn_rcs;			/* the verion for the rcs file
					 * (tag version?) 	 */
    char *ts_user;			/* the timestamp for the user file */
    char *ts_rcs;			/* the user timestamp from entries */
    char *options;			/* opts from Entries file 
					 * (keyword expansion)	 */
    char *tag;				/* tag stored in the Entries file */
    char *date;				/* date stored in the Entries file */
    Entnode *entdata;			/* pointer to entries file node  */
    RCSNode *srcfile;			/* pointer to parsed src file info */
};
typedef struct vers_ts Vers_TS;

/*
 * structure used for list-private storage by ParseEntries() and
 * Version_TS().
 */
struct stickydirtag
{
    int aflag;
    char *tag;
    char *date;
    char *options;
};

/* flags for run_exec(), the fast system() for CVS */
#define	RUN_NORMAL		0x0000	/* no special behaviour */
#define	RUN_COMBINED		0x0001	/* stdout is duped to stderr */
#define	RUN_REALLY		0x0002	/* do the exec, even if noexec is on */
#define	RUN_STDOUT_APPEND	0x0004	/* append to stdout, don't truncate */
#define	RUN_STDERR_APPEND	0x0008	/* append to stderr, don't truncate */
#define	RUN_SIGIGNORE		0x0010	/* ignore interrupts for command */
#define	RUN_TTY		(char *)0	/* for the benefit of lint */

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

extern char *program_name, *command_name;
extern char *Rcsbin, *Editor, *CVSroot;
extern char *CurDir;
extern int really_quiet, quiet;
extern int use_editor;
extern int cvswrite;

extern int trace;			/* Show all commands */
extern int noexec;			/* Don't modify disk anywhere */
extern int logoff;			/* Don't write history entry */

/* Externs that are included directly in the CVS sources */
#if __STDC__
int Reader_Lock (char *xrepository);
DBM *open_module (void);
FILE *Fopen (char *name, char *mode);
FILE *open_file (char *name, char *mode);
List *Find_Dirs (char *repository, int which);
List *ParseEntries (int aflag);
char *Make_Date (char *rawdate);
char *Name_Repository (char *dir, char *update_dir);
char *Short_Repository (char *repository);
char *getcaller (void);
char *time_stamp (char *file);
char *xmalloc (int bytes);
char *xrealloc (char *ptr, int bytes);
char *xstrdup (char *str);
int No_Difference (char *file, Vers_TS * vers, List * entries);
int Parse_Info (char *infofile, char *repository, int (*callproc) (), int all);
int Reader_Lock (char *xrepository);
int SIG_register (int sig, SIGTYPE (*fn) ());
int Writer_Lock (List * list);
int gethostname (char *name, int namelen);
int ign_name (char *name);
int isdir (char *file);
int isfile (char *file);
int islink (char *file);
int isreadable (char *file);
int iswritable (char *file);
int link_file (char *from, char *to);
int numdots (char *s);
int run_exec (char *stin, char *stout, char *sterr, int flags);
int unlink_file (char *f);
int update (int argc, char *argv[]);
int xcmp (char *file1, char *file2);
int yesno (void);
time_t get_date (char *date, struct timeb *now);
void Create_Admin (char *dir, char *repository, char *tag, char *date);
void Lock_Cleanup (void);
void ParseTag (char **tagp, char **datep);
void Scratch_Entry (List * list, char *fname);
void WriteTag (char *dir, char *tag, char *date);
void cat_module (int status);
void check_entries (char *dir);
void close_module (DBM * db);
void copy_file (char *from, char *to);
void error (int status, int errnum, char *message,...);
void fperror (FILE * fp, int status, int errnum, char *message,...);
void free_names (int *pargc, char *argv[]);
void freevers_ts (Vers_TS ** versp);
void ign_add (char *ign, int hold);
void ign_add_file (char *file, int hold);
void ign_setup (void);
void line2argv (int *pargc, char *argv[], char *line);
void make_directories (char *name);
void make_directory (char *name);
void rename_file (char *from, char *to);
void run_arg (char *s);
void run_args (char *fmt,...);
void run_print (FILE * fp);
void run_setup (char *fmt,...);
void strip_path (char *path);
void update_delproc (Node * p);
void usage (char **cpp);
void xchmod (char *fname, int writable);
int Checkin (int type, char *file, char *repository, char *rcs, char *rev,
	     char *tag, char *message, List * entries);
Ctype Classify_File (char *file, char *tag, char *date, char *options,
		     int force_tag_match, int aflag, char *repository,
		     List *entries, List *srcfiles, Vers_TS **versp);
List *Find_Names (char *repository, int which, int aflag,
		  List ** optentries);
void Register (List * list, char *fname, char *vn, char *ts,
	       char *options, char *tag, char *date);
void Update_Logfile (char *repository, char *xmessage, char *xrevision,
		     FILE * xlogfp, List * xchanges);
Vers_TS *Version_TS (char *repository, char *options, char *tag,
		     char *date, char *user, int force_tag_match,
		     int set_time, List * entries, List * xfiles);
void do_editor (char *dir, char *message, char *repository,
		List * changes);
int do_module (DBM * db, char *mname, enum mtype m_type, char *msg,
	       int (*callback_proc) (), char *where, int shorten,
	       int local_specified, int run_module_prog, char *extra_arg);
int do_recursion (int (*xfileproc) (), int (*xfilesdoneproc) (),
		  Dtype (*xdirentproc) (), int (*xdirleaveproc) (),
		  Dtype xflags, int xwhich, int xaflag, int xreadlock,
		  int xdosrcs);
int do_update (int argc, char *argv[], char *xoptions, char *xtag,
	       char *xdate, int xforce, int local, int xbuild,
	       int xaflag, int xprune, int xpipeout, int which,
	       char *xjoin_rev1, char *xjoin_rev2, char *preload_update_dir);
void history_write (int type, char *update_dir, char *revs, char *name,
		    char *repository);
int start_recursion (int (*fileproc) (), int (*filesdoneproc) (),
		     Dtype (*direntproc) (), int (*dirleaveproc) (),
		     int argc, char *argv[], int local, int which,
		     int aflag, int readlock, char *update_preload,
		     int dosrcs);
void SIG_beginCrSect ();
void SIG_endCrSect ();
#else				/* !__STDC__ */
DBM *open_module ();
FILE *Fopen ();
FILE *open_file ();
List *Find_Dirs ();
List *Find_Names ();
List *ParseEntries ();
Vers_TS *Version_TS ();
char *Make_Date ();
char *Name_Repository ();
char *Short_Repository ();
char *getcaller ();
char *time_stamp ();
char *xmalloc ();
char *xrealloc ();
char *xstrdup ();
int Checkin ();
Ctype Classify_File ();
int No_Difference ();
int Parse_Info ();
int Reader_Lock ();
int SIG_register ();
int Writer_Lock ();
int do_module ();
int do_recursion ();
int do_update ();
int gethostname ();
int ign_name ();
int isdir ();
int isfile ();
int islink ();
int isreadable ();
int iswritable ();
int link_file ();
int numdots ();
int run_exec ();
int start_recursion ();
int unlink_file ();
int update ();
int xcmp ();
int yesno ();
time_t get_date ();
void Create_Admin ();
void Lock_Cleanup ();
void ParseTag ();
void ParseTag ();
void Register ();
void Scratch_Entry ();
void Update_Logfile ();
void WriteTag ();
void cat_module ();
void check_entries ();
void close_module ();
void copy_file ();
void do_editor ();
void error ();
void fperror ();
void free_names ();
void freevers_ts ();
void history_write ();
void ign_add ();
void ign_add_file ();
void ign_setup ();
void line2argv ();
void make_directories ();
void make_directory ();
void rename_file ();
void run_arg ();
void run_args ();
void run_print ();
void run_setup ();
void strip_path ();
void update_delproc ();
void usage ();
void xchmod ();
void SIG_beginCrSect ();
void SIG_endCrSect ();
#endif				/* __STDC__ */
