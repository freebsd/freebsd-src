dnl ######################################################################
dnl Define mount type to hide amd mounts from df(1)
dnl
dnl This has to be determined individually per OS.  Depending on whatever
dnl mount options are defined in the system header files such as
dnl MNTTYPE_IGNORE or MNTTYPE_AUTO, or others does not work: some OSs define
dnl some of these then use other stuff; some do not define them at all in
dnl the headers, but still use it; and more.  After a long attempt to get
dnl this automatically configured, I came to the conclusion that the semi-
dnl automatic per-host-os determination here is the best.
dnl
AC_DEFUN(AMU_CHECK_HIDE_MOUNT_TYPE,
[
AC_CACHE_CHECK(for mount type to hide from df,
ac_cv_hide_mount_type,
[
case "${host_os}" in
	irix* | hpux* )
		ac_cv_hide_mount_type="ignore"
		;;
	sunos4* )
		ac_cv_hide_mount_type="auto"
		;;
	* )
		ac_cv_hide_mount_type="nfs"
		;;
esac
])
AC_DEFINE_UNQUOTED(HIDE_MOUNT_TYPE, "$ac_cv_hide_mount_type")
])
dnl ======================================================================
