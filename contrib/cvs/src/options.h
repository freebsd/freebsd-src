/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS source distribution.
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

/* By default, CVS stores its modules and other such items in flat
   text files (MY_NDBM enables this).  Turning off MY_NDBM causes CVS
   to look for a system-supplied ndbm database library and use it
   instead.  That may speed things up, but the default setting
   generally works fine too.  */

#ifndef MY_NDBM
#define	MY_NDBM
#endif

/*
 * The cvs admin command is restricted to the members of the group
 * CVS_ADMIN_GROUP.  If this group does not exist, all users are
 * allowed to run cvs admin.  To disable the cvs admin for all users,
 * create an empty group CVS_ADMIN_GROUP.  To disable access control
 * for cvs admin, comment out the define below.
 */
#ifndef CVS_ADMIN_GROUP
#define CVS_ADMIN_GROUP "cvsadmin"
#endif

/*
 * When locking the repository, some sites like to remove locks and
 * assume the program that created them went away if the lock has
 * existed for a long time.  This used to be the default for previous
 * versions of CVS.  CVS now attempts to be much more robust, so lock
 * files should not be left around by mistake. The new behaviour will
 * never remove old locks (they must now be removed by hand).
 * Enabling CVS_FUDGELOCKS will cause CVS to remove locks that are
 * older than CVSLCKAGE seconds.
 * 
 * Use of this option is NOT recommended.
 */
#ifndef CVS_FUDGELOCKS
/* #define CVS_FUDGELOCKS */
#endif

/* Define this to enable the SETXID support.  The way to use this is
   to create a group with no users in it (except perhaps cvs
   administrators), set the cvs executable to setgid that group, chown
   all the repository files to that group, and change all directory
   permissions in the repository to 770.  The last person to modify a
   file will own it, but as long as directory permissions are set
   right that won't matter.  You'll need a system which inherits file
   groups from the parent directory (WARNING: using the wrong kind of
   system (I think Solaris 2.4 is the wrong kind, for example) will
   create a security hole!  You will receive no warning other than the
   fact that files in the working directory are owned by the group
   which cvs is setgid to).

   One security hole which has been reported is that setgid is not
   turned off when the editor is invoked--most editors provide a way
   to execute a shell, or the user can specify an editor (this one is
   large enough to drive a truck through).  Don't assume that the
   holes described here are the only ones; I don't know how carefully
   SETXID has been inspected for security holes.  */
#ifndef SETXID_SUPPORT
/* #define SETXID_SUPPORT */
#endif


/* End of CVS configuration section */
