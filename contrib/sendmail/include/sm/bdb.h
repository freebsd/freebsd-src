/*
 * Copyright (c) 2002, 2003 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	$Id: bdb.h,v 1.1.2.4 2003/03/06 16:27:38 ca Exp $
 */

#ifndef	SM_BDB_H
#define SM_BDB_H

#if NEWDB
# include <db.h>
# ifndef DB_VERSION_MAJOR
#  define DB_VERSION_MAJOR 1
# endif /* ! DB_VERSION_MAJOR */

# if DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1

#  define DBTXN	NULL ,

/*
**  Always turn on DB_FCNTL_LOCKING for DB 4.1.x since its
**  "workaround" for accepting an empty (locked) file depends on
**  this flag. Notice: this requires 4.1.24 + patch (which should be
**  part of 4.1.25).
*/

#  define SM_DB_FLAG_ADD(flag)	(flag) |= DB_FCNTL_LOCKING

# else /* DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1 */

#  define DBTXN
#  if !HASFLOCK && defined(DB_FCNTL_LOCKING)
#   define SM_DB_FLAG_ADD(flag)	(flag) |= DB_FCNTL_LOCKING
#  else /* !HASFLOCK && defined(DB_FCNTL_LOCKING) */
#   define SM_DB_FLAG_ADD(flag)	((void) 0)
#  endif /* !HASFLOCK && defined(DB_FCNTL_LOCKING) */

# endif /* DB_VERSION_MAJOR >= 4 && DB_VERSION_MINOR >= 1 */
#endif /* NEWDB */

#endif /* ! SM_BDB_H */
