dnl ######################################################################
dnl Specify additional cpp options based on the OS and the compiler
AC_DEFUN(AMU_OS_CPPFLAGS,
[
AC_CACHE_CHECK(additional preprocessor flags,
ac_cv_os_cppflags,
[
case "${host_os}" in
# off for now, posix may be a broken thing for nextstep3...
#	nextstep* )
#		ac_cv_os_cppflags="-D_POSIX_SOURCE"
#		;;
	* )	ac_cv_os_cppflags="" ;;
esac
])
CPPFLAGS="$CPPFLAGS $ac_cv_os_cppflags"
])
dnl ======================================================================
