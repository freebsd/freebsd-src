dnl ######################################################################
dnl check style of unmounting filesystems
AC_DEFUN(AMU_CHECK_UMOUNT_STYLE,
[
AC_CACHE_CHECK(style of unmounting filesystems,
ac_cv_style_umount,
[
# select the correct style for unmounting filesystems
case "${host_os_name}" in
	bsd44* | bsdi* | freebsd* | netbsd* | openbsd* | darwin* | rhapsody* )
			ac_cv_style_umount=bsd44 ;;
	osf* )
			ac_cv_style_umount=osf ;;
	* )
			ac_cv_style_umount=default ;;
esac
])
am_utils_umount_style_file="umount_fs.c"
am_utils_link_files=${am_utils_link_files}libamu/${am_utils_umount_style_file}:conf/umount/umount_${ac_cv_style_umount}.c" "

# append un-mount utilities object to LIBOBJS for automatic compilation
AC_LIBOBJ(umount_fs)
])
dnl ======================================================================
