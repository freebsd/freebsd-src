dnl $Id: db.m4,v 1.1 2000/07/19 11:21:07 joda Exp $
dnl
dnl tests for various db libraries
dnl
AC_DEFUN([rk_DB],[berkeley_db=db
AC_ARG_WITH(berkeley-db,
[  --without-berkeley-db   if you don't want berkeley db],[
if test "$withval" = no; then
	berkeley_db=""
fi
])
if test "$berkeley_db"; then
  AC_CHECK_HEADERS([				\
	db.h					\
	db_185.h				\
  ])
fi

AC_FIND_FUNC_NO_LIBS2(dbopen, $berkeley_db, [
#include <stdio.h>
#if defined(HAVE_DB_185_H)
#include <db_185.h>
#elif defined(HAVE_DB_H)
#include <db.h>
#endif
],[NULL, 0, 0, 0, NULL])

AC_FIND_FUNC_NO_LIBS(dbm_firstkey, $berkeley_db gdbm ndbm)
AC_FIND_FUNC_NO_LIBS(db_create, $berkeley_db)

DBLIB="$LIB_dbopen"
if test "$LIB_dbopen" != "$LIB_db_create"; then
        DBLIB="$DBLIB $LIB_db_create"
fi
if test "$LIB_dbopen" != "$LIB_dbm_firstkey"; then
	DBLIB="$DBLIB $LIB_dbm_firstkey"
fi
AC_SUBST(DBLIB)dnl

])
