dnl ######################################################################
dnl set OS libraries specific to an OS:
dnl libnsl/libsocket are needed only on solaris and some svr4 systems.
dnl Using a typical macro has proven unsuccesful, because on some other
dnl systems such as irix, including libnsl and or libsocket actually breaks
dnl lots of code.  So I am forced to use a special purpose macro that sets
dnl the libraries based on the OS.  Sigh.  -Erez.
AC_DEFUN(AMU_CHECK_OS_LIBS,
[
AC_CACHE_CHECK(for additional OS libraries,
ac_cv_os_libs,
[
# select the correct set of libraries to link with
case "${host_os_name}" in
	svr4* | sysv4* | solaris2* | sunos5* | aoi* )
			ac_cv_os_libs="-lsocket -lnsl" ;;
	* )
			ac_cv_os_libs=none ;;
esac
])
# set list of libraries to link with
if test "$ac_cv_os_libs" != none
then
  LIBS="$ac_cv_os_libs $LIBS"
fi

])
dnl ======================================================================
