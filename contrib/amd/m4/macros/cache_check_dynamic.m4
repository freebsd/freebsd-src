dnl ######################################################################
dnl New versions of the cache functions which also dynamically evaluate the
dnl cache-id field, so that it may contain shell variables to expand
dnl dynamically for the creation of $ac_cv_* variables on the fly.
dnl In addition, this function allows you to call COMMANDS which generate
dnl output on the command line, because it prints its own AC_MSG_CHECKING
dnl after COMMANDS are run.
dnl
dnl ======================================================================
dnl AMU_CACHE_CHECK_DYNAMIC(MESSAGE, CACHE-ID, COMMANDS)
define(AMU_CACHE_CHECK_DYNAMIC,
[
ac_tmp=`echo $2`
if eval "test \"`echo '$''{'$ac_tmp'+set}'`\" = set"; then
  AC_MSG_CHECKING([$1])
  echo $ECHO_N "(cached) $ECHO_C" 1>&AS_MESSAGE_FD([])
dnl XXX: for older autoconf versions
dnl  echo $ac_n "(cached) $ac_c" 1>&AS_MESSAGE_FD([])
else
  $3
  AC_MSG_CHECKING([$1])
fi
ac_tmp_val=`eval eval "echo '$''{'$ac_tmp'}'"`
AC_MSG_RESULT($ac_tmp_val)
])
dnl ======================================================================
