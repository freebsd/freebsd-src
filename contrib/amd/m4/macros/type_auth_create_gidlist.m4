dnl ######################################################################
dnl check the correct type for the 5th argument to authunix_create()
AC_DEFUN(AMU_TYPE_AUTH_CREATE_GIDLIST,
[
AC_CACHE_CHECK(argument type of 5rd argument to authunix_create(),
ac_cv_auth_create_gidlist,
[
# select the correct type
case "${host_os_name}" in
	sunos[[34]]* | bsdi2* | sysv4* | hpux10.10 | ultrix* | aix4* )
		ac_cv_auth_create_gidlist="int" ;;
	* )
		ac_cv_auth_create_gidlist="gid_t" ;;
esac
])
AC_DEFINE_UNQUOTED(AUTH_CREATE_GIDLIST_TYPE, $ac_cv_auth_create_gidlist)
])
dnl ======================================================================
