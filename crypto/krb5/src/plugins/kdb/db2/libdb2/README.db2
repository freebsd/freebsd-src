#	@(#)README	8.28 (Berkeley) 11/2/95

This is version 2.0-ALPHA of the Berkeley DB code.
THIS IS A PRELIMINARY RELEASE.

For information on compiling and installing this software, see the file
PORT/README.

Newer versions of this software will periodically be made available by
anonymous ftp from ftp.cs.berkeley.edu:ucb/4bsd/db.tar.{Z,gz} and from
ftp.harvard.edu:margo/db.tar.{Z,gz}.  If you want to receive announcements
of future releases of this software, send email to the contact address
below.

Email questions may be addressed to dbinfo@eecs.harvard.edu.

============================================
Distribution contents:

README		This file.
CHANGELOG	List of changes, per version.
btree		B+tree access method.
db		The db_open interface routine.
docs		Various USENIX papers, and the formatted manual pages.
hash		Extended linear hashing access method.
lock		Lock manager.
log		Log manager.
man		The unformatted manual pages.
mpool		The buffer manager support.
mutex		Mutex support.
recno		The fixed/variable length record access method.
test		Test package.
txn		Transaction support.

============================================
Debugging:

If you're running a memory checker (e.g. Purify) on DB, make sure that
you recompile it with "-DPURIFY" in the CFLAGS, first.  By default,
allocated pages are not initialized by the DB code, and they will show
up as reads of uninitialized memory in the buffer write routines.
