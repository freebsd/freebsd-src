dnl ######################################################################
dnl check style of fixmount check_mount() function
AC_DEFUN(AMU_CHECK_CHECKMOUNT_STYLE,
[
AC_CACHE_CHECK(style of fixmount check_mount(),
ac_cv_style_checkmount,
[
# select the correct style for unmounting filesystems
case "${host_os_name}" in
	svr4* | sysv4* | solaris2* | sunos5* )
			ac_cv_style_checkmount=svr4 ;;
	bsd44* | bsdi* | freebsd* | netbsd* | openbsd* | darwin* | rhapsody* )
			ac_cv_style_checkmount=bsd44 ;;
	aix* )
			ac_cv_style_checkmount=aix ;;
	osf* )
			ac_cv_style_checkmount=osf ;;
	ultrix* )
			ac_cv_style_checkmount=ultrix ;;
	* )
			ac_cv_style_checkmount=default ;;
esac
])
am_utils_checkmount_style_file="check_mount.c"
am_utils_link_files=${am_utils_link_files}fixmount/${am_utils_checkmount_style_file}:conf/checkmount/checkmount_${ac_cv_style_checkmount}.c" "

])
dnl ======================================================================
