dnl $Id: db.m4,v 1.5 2001/09/13 00:34:07 assar Exp $
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

have_ndbm=no
db_type=unknown

if test "$berkeley_db"; then

  AC_CHECK_HEADERS([				\
	db.h					\
	db_185.h				\
  ])

dnl db_create is used by db3

  AC_FIND_FUNC_NO_LIBS(db_create, $berkeley_db, [
  #include <stdio.h>
  #include <db.h>
  ],[NULL, NULL, 0])

  if test "$ac_cv_func_db_create" = "yes"; then
    db_type=db3
    if test "$ac_cv_funclib_db_create" != "yes"; then
      DBLIB="$ac_cv_funclib_db_create"
    else
      DBLIB=""
    fi
    AC_DEFINE(HAVE_DB3, 1, [define if you have a berkeley db3 library])
  else

dnl dbopen is used by db1/db2

    AC_FIND_FUNC_NO_LIBS(dbopen, $berkeley_db, [
    #include <stdio.h>
    #if defined(HAVE_DB_185_H)
    #include <db_185.h>
    #elif defined(HAVE_DB_H)
    #include <db.h>
    #else
    #error no db.h
    #endif
    ],[NULL, 0, 0, 0, NULL])

    if test "$ac_cv_func_dbopen" = "yes"; then
      db_type=db1
      if test "$ac_cv_funclib_dbopen" != "yes"; then
        DBLIB="$ac_cv_funclib_dbopen"
      else
        DBLIB=""
      fi
      AC_DEFINE(HAVE_DB1, 1, [define if you have a berkeley db1/2 library])
    fi
  fi

dnl test for ndbm compatability

  AC_FIND_FUNC_NO_LIBS2(dbm_firstkey, $ac_cv_funclib_dbopen $ac_cv_funclib_db_create, [
  #include <stdio.h>
  #define DB_DBM_HSEARCH 1
  #include <db.h>
  DBM *dbm;
  ],[NULL])

  if test "$ac_cv_func_dbm_firstkey" = "yes"; then
    if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
      LIB_NDBM="$ac_cv_funclib_dbm_firstkey"
    else
      LIB_NDBM=""
    fi
    AC_DEFINE(HAVE_DB_NDBM, 1, [define if you have ndbm compat in db])
    AC_DEFINE(HAVE_NEW_DB, 1, [Define if NDBM really is DB (creates files *.db)])
  else
    $as_unset ac_cv_func_dbm_firstkey
    $as_unset ac_cv_funclib_dbm_firstkey
  fi

fi # berkeley db

if test "$db_type" = "unknown" -o "$ac_cv_func_dbm_firstkey" = ""; then

  AC_CHECK_HEADERS([				\
	dbm.h					\
	ndbm.h					\
  ])

  AC_FIND_FUNC_NO_LIBS(dbm_firstkey, ndbm, [
  #include <stdio.h>
  #if defined(HAVE_NDBM_H)
  #include <ndbm.h>
  #elif defined(HAVE_DBM_H)
  #include <dbm.h>
  #else
  #error no ndbm.h
  #endif
  DBM *dbm;
  ],[NULL])

  if test "$ac_cv_func_dbm_firstkey" = "yes"; then
    if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
      LIB_NDBM="$ac_cv_funclib_dbm_firstkey"
    else
      LIB_NDBM=""
    fi
    AC_DEFINE(HAVE_NDBM, 1, [define if you have a ndbm library])dnl
    have_ndbm=yes
    if test "$db_type" = "unknown"; then
      db_type=ndbm
      DBLIB="$LIB_NDBM"
    fi
  else

    $as_unset ac_cv_func_dbm_firstkey
    $as_unset ac_cv_funclib_dbm_firstkey

    AC_CHECK_HEADERS([				\
	  gdbm/ndbm.h				\
    ])

    AC_FIND_FUNC_NO_LIBS(dbm_firstkey, gdbm, [
    #include <stdio.h>
    #include <gdbm/ndbm.h>
    DBM *dbm;
    ],[NULL])

    if test "$ac_cv_func_dbm_firstkey" = "yes"; then
      if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
	LIB_NDBM="$ac_cv_funclib_dbm_firstkey"
      else
	LIB_NDBM=""
      fi
      AC_DEFINE(HAVE_NDBM, 1, [define if you have a ndbm library])dnl
      have_ndbm=yes
      if test "$db_type" = "unknown"; then
	db_type=ndbm
	DBLIB="$LIB_NDBM"
      fi
    fi
  fi

fi # unknown

if test "$have_ndbm" = "yes"; then
  AC_MSG_CHECKING([if ndbm is implemented with db])
  AC_TRY_RUN([
#include <unistd.h>
#include <fcntl.h>
#if defined(HAVE_GDBM_NDBM_H)
#include <gdbm/ndbm.h>
#elif defined(HAVE_NDBM_H)
#include <ndbm.h>
#elif defined(HAVE_DBM_H)
#include <dbm.h>
#endif
int main()
{
  DBM *d;

  d = dbm_open("conftest", O_RDWR | O_CREAT, 0666);
  if (d == NULL)
    return 1;
  dbm_close(d);
  return 0;
}],[
    if test -f conftest.db; then
      AC_MSG_RESULT([yes])
      AC_DEFINE(HAVE_NEW_DB, 1, [Define if NDBM really is DB (creates files *.db)])
    else
      AC_MSG_RESULT([no])
    fi],[AC_MSG_RESULT([no])])
fi

AC_SUBST(DBLIB)dnl
AC_SUBST(LIB_NDBM)dnl
])
