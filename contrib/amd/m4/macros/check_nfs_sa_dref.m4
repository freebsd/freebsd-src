dnl ######################################################################
dnl check the correct way to dereference the address part of the nfs fhandle
AC_DEFUN(AMU_CHECK_NFS_SA_DREF,
[
AC_CACHE_CHECK(nfs address dereferencing style,
ac_cv_nfs_sa_dref_style,
[
# select the correct nfs address dereferencing style
case "${host_os}" in
	hpux[[6-9]]* | hpux10* | sunos[[34]]* | solaris1* )
		ac_cv_nfs_sa_dref_style=default ;;
	svr4* | sysv4* | solaris* | sunos* | hpux* )
		ac_cv_nfs_sa_dref_style=svr4 ;;
	386bsd* | bsdi1* )
		ac_cv_nfs_sa_dref_style=386bsd ;;
	bsd44* | bsdi* | freebsd* | netbsd* | openbsd* | darwin* | rhapsody* )
		ac_cv_nfs_sa_dref_style=bsd44 ;;
	linux* )
		ac_cv_nfs_sa_dref_style=linux ;;
	aix* )
		ac_cv_nfs_sa_dref_style=aix3 ;;
	aoi* )
		ac_cv_nfs_sa_dref_style=aoi ;;
	isc3 )
		ac_cv_nfs_sa_dref_style=isc3 ;;
	* )
		ac_cv_nfs_sa_dref_style=default ;;
esac
])
am_utils_nfs_sa_dref=$srcdir"/conf/sa_dref/sa_dref_"$ac_cv_nfs_sa_dref_style".h"
AC_SUBST_FILE(am_utils_nfs_sa_dref)
])
dnl ======================================================================
