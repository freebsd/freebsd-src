dnl ######################################################################
dnl check the unmount system call arguments needed for
AC_DEFUN(AMU_CHECK_UNMOUNT_ARGS,
[
AC_CACHE_CHECK(unmount system-call arguments,
ac_cv_unmount_args,
[
# select the correct style to mount(2) a filesystem
case "${host_os_name}" in
	aix* )
		ac_cv_unmount_args="mnt->mnt_passno, 0" ;;
	ultrix* )
		ac_cv_unmount_args="mnt->mnt_passno" ;;
	* )
		ac_cv_unmount_args="mnt->mnt_dir" ;;
esac
])
am_utils_unmount_args=$ac_cv_unmount_args
AC_SUBST(am_utils_unmount_args)
])
dnl ======================================================================
