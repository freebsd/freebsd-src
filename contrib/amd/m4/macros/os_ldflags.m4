dnl ######################################################################
dnl Specify additional linker options based on the OS and the compiler
AC_DEFUN(AMU_OS_LDFLAGS,
[
AC_CACHE_CHECK(additional linker flags,
ac_cv_os_ldflags,
[
case "${host_os}" in
	solaris2.7* | sunos5.7* )
		# find LDAP: off until Sun includes ldap headers.
		case "${CC}" in
			* )
				#ac_cv_os_ldflags="-L/usr/lib/fn"
				;;
		esac
		;;
	* )	ac_cv_os_ldflags="" ;;
esac
])
LDFLAGS="$LDFLAGS $ac_cv_os_ldflags"
])
dnl ======================================================================
