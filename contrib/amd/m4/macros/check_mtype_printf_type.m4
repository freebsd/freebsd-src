dnl ######################################################################
dnl check the correct printf-style type for the mount type in the mount()
dnl system call.
dnl If you change this one, you must also fix the check_mtype_type.m4.
AC_DEFUN(AMU_CHECK_MTYPE_PRINTF_TYPE,
[
AC_CACHE_CHECK(printf string to print type field of mount() call,
ac_cv_mtype_printf_type,
[
# select the correct printf type
case "${host_os_name}" in
	osf* | freebsd2* | bsdi2* | aix* | ultrix* )
		ac_cv_mtype_printf_type="%d" ;;
	irix3 | isc3 )
		ac_cv_mtype_printf_type="0x%x" ;;
	* )
		ac_cv_mtype_printf_type="%s" ;;
esac
])
AC_DEFINE_UNQUOTED(MTYPE_PRINTF_TYPE, "$ac_cv_mtype_printf_type")
])
dnl ======================================================================
