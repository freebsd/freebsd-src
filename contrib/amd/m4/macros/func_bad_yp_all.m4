dnl Check for a yp_all() function that does not leak a file descriptor
dnl to the ypserv process.
AC_DEFUN(AMU_FUNC_BAD_YP_ALL,
[
AC_CACHE_CHECK(for a file-descriptor leakage clean yp_all,
ac_cv_func_yp_all_clean,
[
# clean by default
ac_cv_func_yp_all_clean=yes
# select the correct type
case "${host_os_name}" in
	irix* )
		ac_cv_func_yp_all_clean=no ;;
	linux* )
		# RedHat 5.1 systems with glibc glibc-2.0.7-19 or below
		# leak a UDP socket from yp_all()
		case "`cat /etc/redhat-release /dev/null 2>/dev/null`" in
			*5.1* )
				ac_cv_func_yp_all_clean=no ;;
		esac
esac
])
if test $ac_cv_func_yp_all_clean = no
then
  AC_DEFINE(HAVE_BAD_YP_ALL)
fi
])
