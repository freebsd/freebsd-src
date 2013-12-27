dnl $Id: aclocal.m4,v 1.9 2013/06/02 21:31:27 tim Exp $
dnl
dnl OpenSSH-specific autoconf macros
dnl

dnl OSSH_CHECK_CFLAG_COMPILE(check_flag[, define_flag])
dnl Check that $CC accepts a flag 'check_flag'. If it is supported append
dnl 'define_flag' to $CFLAGS. If 'define_flag' is not specified, then append
dnl 'check_flag'.
AC_DEFUN([OSSH_CHECK_CFLAG_COMPILE], [{
	AC_MSG_CHECKING([if $CC supports $1])
	saved_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $1"
	_define_flag="$2"
	test "x$_define_flag" = "x" && _define_flag="$1"
	AC_COMPILE_IFELSE([AC_LANG_SOURCE([[int main(void) { return 0; }]])],
		[
if `grep -i "unrecognized option" conftest.err >/dev/null`
then
		AC_MSG_RESULT([no])
		CFLAGS="$saved_CFLAGS"
else
		AC_MSG_RESULT([yes])
		 CFLAGS="$saved_CFLAGS $_define_flag"
fi],
		[ AC_MSG_RESULT([no])
		  CFLAGS="$saved_CFLAGS" ]
	)
}])


dnl OSSH_CHECK_HEADER_FOR_FIELD(field, header, symbol)
dnl Does AC_EGREP_HEADER on 'header' for the string 'field'
dnl If found, set 'symbol' to be defined. Cache the result.
dnl TODO: This is not foolproof, better to compile and read from there
AC_DEFUN(OSSH_CHECK_HEADER_FOR_FIELD, [
# look for field '$1' in header '$2'
	dnl This strips characters illegal to m4 from the header filename
	ossh_safe=`echo "$2" | sed 'y%./+-%__p_%'`
	dnl
	ossh_varname="ossh_cv_$ossh_safe""_has_"$1
	AC_MSG_CHECKING(for $1 field in $2)
	AC_CACHE_VAL($ossh_varname, [
		AC_EGREP_HEADER($1, $2, [ dnl
			eval "$ossh_varname=yes" dnl
		], [ dnl
			eval "$ossh_varname=no" dnl
		]) dnl
	])
	ossh_result=`eval 'echo $'"$ossh_varname"`
	if test -n "`echo $ossh_varname`"; then
		AC_MSG_RESULT($ossh_result)
		if test "x$ossh_result" = "xyes"; then
			AC_DEFINE($3, 1, [Define if you have $1 in $2])
		fi
	else
		AC_MSG_RESULT(no)
	fi
])

dnl Check for socklen_t: historically on BSD it is an int, and in
dnl POSIX 1g it is a type of its own, but some platforms use different
dnl types for the argument to getsockopt, getpeername, etc.  So we
dnl have to test to find something that will work.
AC_DEFUN([TYPE_SOCKLEN_T],
[
   AC_CHECK_TYPE([socklen_t], ,[
      AC_MSG_CHECKING([for socklen_t equivalent])
      AC_CACHE_VAL([curl_cv_socklen_t_equiv],
      [
	 # Systems have either "struct sockaddr *" or
	 # "void *" as the second argument to getpeername
	 curl_cv_socklen_t_equiv=
	 for arg2 in "struct sockaddr" void; do
	    for t in int size_t unsigned long "unsigned long"; do
	       AC_TRY_COMPILE([
		  #include <sys/types.h>
		  #include <sys/socket.h>

		  int getpeername (int, $arg2 *, $t *);
	       ],[
		  $t len;
		  getpeername(0,0,&len);
	       ],[
		  curl_cv_socklen_t_equiv="$t"
		  break
	       ])
	    done
	 done

	 if test "x$curl_cv_socklen_t_equiv" = x; then
	    AC_MSG_ERROR([Cannot find a type to use in place of socklen_t])
	 fi
      ])
      AC_MSG_RESULT($curl_cv_socklen_t_equiv)
      AC_DEFINE_UNQUOTED(socklen_t, $curl_cv_socklen_t_equiv,
			[type to use in place of socklen_t if not defined])],
      [#include <sys/types.h>
#include <sys/socket.h>])
])

