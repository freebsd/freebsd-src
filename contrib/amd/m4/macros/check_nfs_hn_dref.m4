dnl ######################################################################
dnl check the correct way to dereference the hostname part of the nfs fhandle
AC_DEFUN(AMU_CHECK_NFS_HN_DREF,
[
AC_CACHE_CHECK(nfs hostname dereferencing style,
ac_cv_nfs_hn_dref_style,
[
# select the correct nfs address dereferencing style
case "${host_os_name}" in
	linux* )
		ac_cv_nfs_hn_dref_style=linux ;;
	isc3 )
		ac_cv_nfs_hn_dref_style=isc3 ;;
	* )
		ac_cv_nfs_hn_dref_style=default ;;
esac
])
am_utils_nfs_hn_dref=$srcdir"/conf/hn_dref/hn_dref_"$ac_cv_nfs_hn_dref_style".h"
AC_SUBST_FILE(am_utils_nfs_hn_dref)
])
dnl ======================================================================
