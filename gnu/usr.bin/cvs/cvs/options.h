/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * This file holds (most of) the configuration tweaks that can be made to
 * customize CVS for your site.  CVS comes configured for a typical SunOS 4.x
 * environment.  The comments for each configurable item are intended to be
 * self-explanatory.  All #defines are tested first to see if an over-riding
 * option was specified on the "make" command line.
 * 
 * If special libraries are needed, you will have to edit the Makefile.in file
 * or the configure script directly.  Sorry.
 */

/*
 * CVS provides the most features when used in conjunction with the Version-5
 * release of RCS.  Thus, it is the default.  This also assumes that GNU diff
 * Version-1.15 is being used as well -- you will have to configure your RCS
 * V5 release separately to make this the case. If you do not have RCS V5 and
 * GNU diff V1.15, comment out this define. You should not try mixing and
 * matching other combinations of these tools.
 */
#ifndef HAVE_RCS5
#define	HAVE_RCS5
#endif

/*
 * If, before installing this version of CVS, you were running RCS V4 AND you
 * are installing this CVS and RCS V5 and GNU diff 1.15 all at the same time,
 * you should turn on the following define.  It only exists to try to do
 * reasonable things with your existing checked out files when you upgrade to
 * RCS V5, since the keyword expansion formats have changed with RCS V5.
 * 
 * If you already have been running with RCS5, or haven't been running with CVS
 * yet at all, or are sticking with RCS V4 for now, leave the commented out.
 */
#ifndef HAD_RCS4
/* #define	HAD_RCS4 */
#endif

/*
 * For portability and heterogeneity reasons, CVS is shipped by default using
 * my own text-file version of the ndbm database library in the src/myndbm.c
 * file.  If you want better performance and are not concerned about
 * heterogeneous hosts accessing your modules file, turn this option off.
 */
#ifndef MY_NDBM
#define	MY_NDBM
#endif

/*
 * The "diff" program to execute when creating patch output.  This "diff"
 * must support the "-c" option for context diffing.  Specify a full
 * pathname if your site wants to use a particular diff.  If you are
 * using the GNU version of diff (version 1.15 or later), this should
 * be "diff -a".  
 * 
 * NOTE: this program is only used for the ``patch'' sub-command (and
 * for ``update'' if you are using the server).  The other commands
 * use rcsdiff which will use whatever version of diff was specified
 * when rcsdiff was built on your system.
 */

#ifndef DIFF
#define	DIFF	"diff -a"
#endif

/*
 * The "grep" program to execute when checking to see if a merged file had
 * any conflicts.  This "grep" must support a standard basic
 * regular expression as an argument.  Specify a full pathname if your site
 * wants to use a particular grep.
 */

#ifndef GREP
#define GREP "grep"
#endif

/*
 * The "rm" program to execute when pruning directories that are not part of
 * a release.  This "rm" must support the "-fr" options.  Specify a full
 * pathname if your site wants to use a particular rm.
 */
#ifndef RM
#define	RM	"rm"
#endif

/*
 * The "sort" program to execute when displaying the module database. Specify
 * a full pathname if your site wants to use a particular sort.
 */
#ifndef SORT
#define	SORT	"sort"
#endif

/*
 * The "patch" program to run when using the CVS server and accepting
 * patches across the network.  Specify a full pathname if your site
 * wants to use a particular patch.
 */
#ifndef PATCH_PROGRAM
#define PATCH_PROGRAM	"patch"
#endif

/*
 * By default, RCS programs are executed with the shell or through execlp(),
 * so the user's PATH environment variable is searched.  If you'd like to
 * bind all RCS programs to a certain directory (perhaps one not in most
 * people's PATH) then set the default in RCSBIN_DFLT.  Note that setting
 * this here will cause all RCS programs to be executed from this directory,
 * unless the user overrides the default with the RCSBIN environment variable
 * or the "-b" option to CVS.
 * 
 * This define should be either the empty string ("") or a full pathname to the
 * directory containing all the installed programs from the RCS distribution.
 */
#ifndef RCSBIN_DFLT
#define	RCSBIN_DFLT	""
#endif

/*
 * The default editor to use, if one does not specify the "-e" option to cvs,
 * or does not have an EDITOR environment variable.  I set this to just "vi",
 * and use the shell to find where "vi" actually is.  This allows sites with
 * /usr/bin/vi or /usr/ucb/vi to work equally well (assuming that your PATH
 * is reasonable).
 */
#ifndef EDITOR_DFLT
#define	EDITOR_DFLT	"vi"
#endif

/*
 * The default umask to use when creating or otherwise setting file or
 * directory permissions in the repository.  Must be a value in the
 * range of 0 through 0777.  For example, a value of 002 allows group
 * rwx access and world rx access; a value of 007 allows group rwx
 * access but no world access.  This value is overridden by the value
 * of the CVSUMASK environment variable, which is interpreted as an
 * octal number.
 */
#ifndef UMASK_DFLT
#define	UMASK_DFLT	002
#endif

/*
 * The cvs admin command is restricted to the members of the group
 * CVS_ADMIN_GROUP.  If this group does not exist, all users are
 * allowed to run cvs admin.  To disable the cvs admin for all users,
 * create an empty group CVS_ADMIN_GROUP.  To disable access control for
 * cvs admin, comment out the define below.
 */
#ifndef CVS_ADMIN_GROUP
#define CVS_ADMIN_GROUP "cvsadmin"
#endif

/*
 * The Repository file holds the path to the directory within the source
 * repository that contains the RCS ,v files for each CVS working directory.
 * This path is either a full-path or a path relative to CVSROOT.
 * 
 * The only advantage that I can see to having a relative path is that One can
 * change the physical location of the master source repository, change one's
 * CVSROOT environment variable, and CVS will work without problems.  I
 * recommend using full-paths.
 */
#ifndef RELATIVE_REPOS
/* #define	RELATIVE_REPOS	 */
#endif

/*
 * When committing or importing files, you must enter a log message.
 * Normally, you can do this either via the -m flag on the command line or an
 * editor will be started for you.  If you like to use logging templates (the
 * rcsinfo file within the $CVSROOT/CVSROOT directory), you might want to
 * force people to use the editor even if they specify a message with -m.
 * Enabling FORCE_USE_EDITOR will cause the -m message to be appended to the
 * temp file when the editor is started.
 */
#ifndef FORCE_USE_EDITOR
/* #define 	FORCE_USE_EDITOR */
#endif

/*
 * When locking the repository, some sites like to remove locks and assume
 * the program that created them went away if the lock has existed for a long
 * time.  This used to be the default for previous versions of CVS.  CVS now
 * attempts to be much more robust, so lock files should not be left around
 * by mistake. The new behaviour will never remove old locks (they must now
 * be removed by hand).  Enabling CVS_FUDGELOCKS will cause CVS to remove
 * locks that are older than CVSLCKAGE seconds.
 * Use of this option is NOT recommended.
 */
#ifndef CVS_FUDGELOCKS
/* #define CVS_FUDGELOCKS */
#endif

/*
 * When committing a permanent change, CVS and RCS make a log entry of
 * who committed the change.  If you are committing the change logged in
 * as "root" (not under "su" or other root-priv giving program), CVS/RCS
 * cannot determine who is actually making the change.
 *
 * As such, by default, CVS disallows changes to be committed by users
 * logged in as "root".  You can disable this option by commenting
 * out the lines below.
 */
#ifndef CVS_BADROOT
#define	CVS_BADROOT
#endif

/*
 * The "cvs admin" command allows people to get around most of the logging
 * and info procedures within CVS.  For exmaple, "cvs tag tagname filename"
 * will perform some validity checks on the tag, while "cvs admin -Ntagname"
 * will not perform those checks.  For this reason, some sites may wish to
 * disable the admin function completely.
 *
 * To disable the admin function, uncomment the lines below.
 */
#ifndef CVS_NOADMIN
/* #define CVS_NOADMIN */
#endif

/*
 * The "cvs diff" command accepts all the single-character options that GNU
 * diff (1.15) accepts.  Except -D.  GNU diff uses -D as a way to put
 * cpp-style #define's around the output differences.  CVS, by default, uses
 * -D to specify a free-form date (like "cvs diff -D '1 week ago'").  If
 * you would prefer that the -D option of "cvs diff" work like the GNU diff
 * option, then comment out this define.
 */
#ifndef CVS_DIFFDATE
#define	CVS_DIFFDATE
#endif

/*
 * define this to enable the SETXID support (see FAQ 4D.13)
 */
#ifndef SETXID_SUPPORT
/* #define SETXID_SUPPORT */
#endif

/*
 * The authenticated client/server is under construction.  Don't
 * define either of these unless you're testing them, in which case 
 * you're me and you already know that.
 */
#undef AUTH_CLIENT_SUPPORT
#undef AUTH_SERVER_SUPPORT

/*
 * If you are working with a large remote repository and a 'cvs checkout' is
 * swamping your network and memory, define these to enable flow control.
 * You will end up with even less guarantees of a consistant checkout,
 * but that may be better than no checkout at all.  The master server process
 * will monitor how far it is getting behind, if it reaches the high water
 * mark, it will signal the child process to stop generating data when
 * convenient (ie: no locks are held, currently at the beginning of a 
 * new directory).  Once the buffer has drained sufficiently to reach the
 * low water mark, it will be signalled to start again.
 * -- EXPERIMENTAL! --  A better solution may be in the works.
 * You may override the default hi/low watermarks here too.
 */
#ifndef SERVER_FLOWCONTROL
/* #define SERVER_FLOWCONTROL */
/* #define SERVER_HI_WATER (2 * 1024 * 1024) */
/* #define SERVER_LO_WATER (1 * 1024 * 1024) */
#endif

/* End of CVS configuration section */

/*
 * Externs that are included in libc, but are used frequently enough to
 * warrant defining here.
 */
#ifndef STDC_HEADERS
extern void exit ();
#endif

#ifndef getwd
extern char *getwd ();
#endif

