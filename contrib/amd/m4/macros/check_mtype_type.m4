dnl ######################################################################
dnl check the correct type for the mount type in the mount() system call
dnl If you change this one, you must also fix the check_mtype_printf_type.m4.
AC_DEFUN(AMU_CHECK_MTYPE_TYPE,
[
AC_CACHE_CHECK(type of mount type field in mount() call,
ac_cv_mtype_type,
[
# select the correct type
case "${host_os_name}" in
	osf* | freebsd2* | bsdi2* | aix* | ultrix* )
		ac_cv_mtype_type=int ;;
	* )
		ac_cv_mtype_type="char *" ;;
esac
])
AC_DEFINE_UNQUOTED(MTYPE_TYPE, $ac_cv_mtype_type)
])
dnl ======================================================================
