dnl ######################################################################
dnl check if a system needs to restart its signal handlers
AC_DEFUN(AMU_CHECK_RESTARTABLE_SIGNAL_HANDLER,
[
AC_CACHE_CHECK(if system needs to restart signal handlers,
ac_cv_restartable_signal_handler,
[
# select the correct systems to restart signal handlers
case "${host_os_name}" in
	svr3* | svr4* | sysv4* | solaris2* | sunos5* | aoi* | irix* )
			ac_cv_restartable_signal_handler=yes ;;
	* )
			ac_cv_restartable_signal_handler=no ;;
esac
])
# define REINSTALL_SIGNAL_HANDLER if need to
if test "$ac_cv_restartable_signal_handler" = yes
then
  AC_DEFINE(REINSTALL_SIGNAL_HANDLER)
fi
])
dnl ======================================================================
