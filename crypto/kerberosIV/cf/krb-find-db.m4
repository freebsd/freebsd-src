dnl $Id: krb-find-db.m4,v 1.5.16.1 2000/08/16 04:11:57 assar Exp $
dnl
dnl find a suitable database library
dnl
dnl AC_FIND_DB(libraries)
AC_DEFUN(KRB_FIND_DB, [

lib_dbm=no
lib_db=no

for i in $1; do

	if test "$i"; then
		m="lib$i"
		l="-l$i"
	else
		m="libc"
		l=""
	fi	

	AC_MSG_CHECKING(for dbm_open in $m)
	AC_CACHE_VAL(ac_cv_krb_dbm_open_$m, [

	save_LIBS="$LIBS"
	LIBS="$l $LIBS"
	AC_TRY_RUN([
#include <unistd.h>
#include <fcntl.h>
#if defined(HAVE_NDBM_H)
#include <ndbm.h>
#elif defined(HAVE_GDBM_NDBM_H)
#include <gdbm/ndbm.h>
#elif defined(HAVE_DBM_H)
#include <dbm.h>
#elif defined(HAVE_RPCSVC_DBM_H)
#include <rpcsvc/dbm.h>
#elif defined(HAVE_DB_H)
#define DB_DBM_HSEARCH 1
#include <db.h>
#endif
int main()
{
  DBM *d;

  d = dbm_open("conftest", O_RDWR | O_CREAT, 0666);
  if(d == NULL)
    return 1;
  dbm_close(d);
  return 0;
}], [
	if test -f conftest.db; then
		ac_res=db
	else
		ac_res=dbm
	fi], ac_res=no, ac_res=no)

	LIBS="$save_LIBS"

	eval ac_cv_krb_dbm_open_$m=$ac_res])
	eval ac_res=\$ac_cv_krb_dbm_open_$m
	AC_MSG_RESULT($ac_res)

	if test "$lib_dbm" = no -a $ac_res = dbm; then
		lib_dbm="$l"
	elif test "$lib_db" = no -a $ac_res = db; then
		lib_db="$l"
		break
	fi
done

AC_MSG_CHECKING(for NDBM library)
ac_ndbm=no
if test "$lib_db" != no; then
	LIB_DBM="$lib_db"
	ac_ndbm=yes
	AC_DEFINE(HAVE_NEW_DB, 1, [Define if NDBM really is DB (creates files ending in .db).])
	if test "$LIB_DBM"; then
		ac_res="yes, $LIB_DBM"
	else
		ac_res=yes
	fi
elif test "$lib_dbm" != no; then
	LIB_DBM="$lib_dbm"
	ac_ndbm=yes
	if test "$LIB_DBM"; then
		ac_res="yes, $LIB_DBM"
	else
		ac_res=yes
	fi
else
	LIB_DBM=""
	ac_res=no
fi
test "$ac_ndbm" = yes && AC_DEFINE(NDBM, 1, [Define if you have NDBM (and not DBM)])dnl
AC_SUBST(LIB_DBM)
DBLIB="$LIB_DBM"
AC_SUBST(DBLIB)
AC_MSG_RESULT($ac_res)

])
