dnl ######################################################################
dnl check if libwrap (if exists), requires the caller to define the variables
dnl deny_severity and allow_severity.
AC_DEFUN([AMU_CHECK_LIBWRAP_SEVERITY],
[
AC_CACHE_CHECK([if libwrap wants caller to define allow_severity and deny_severity], ac_cv_need_libwrap_severity_vars, [
# save, then reset $LIBS back to original value
SAVEDLIBS="$LIBS"
LIBS="$LIBS -lwrap"
# run program one without defining our own severity variables
AC_TRY_RUN(
[
int main()
{
   exit(0);
}
],[ac_tmp_val1="yes"],[ac_tmp_val1="no"])
# run program two with defining our own severity variables
AC_TRY_RUN(
[
int deny_severity, allow_severity, rfc931_timeout;
int main()
{
   exit(0);
}
],[ac_tmp_val2="yes"],[ac_tmp_val2="no"])
# restore original value of $LIBS
LIBS="$SAVEDLIBS"
# now decide what to do
if test "$ac_tmp_val1" = "no" && test "$ac_tmp_val2" = "yes"
then
	ac_cv_need_libwrap_severity_vars="yes"
else
	ac_cv_need_libwrap_severity_vars="no"
fi
])
if test "$ac_cv_need_libwrap_severity_vars" = "yes"
then
	AC_DEFINE(NEED_LIBWRAP_SEVERITY_VARIABLES)
fi
])
