dnl ######################################################################
dnl check the correct way to dereference the address part of the nfs fhandle
AC_DEFUN(AMU_CHECK_NFS_FH_DREF,
[
AC_CACHE_CHECK(nfs file-handle address dereferencing style,
ac_cv_nfs_fh_dref_style,
[
# select the correct nfs address dereferencing style
case "${host_os}" in
	hpux[[6-9]]* | hpux10* )
		ac_cv_nfs_fh_dref_style=hpux ;;
	sunos3* )
		ac_cv_nfs_fh_dref_style=sunos3 ;;
	sunos4* | solaris1* )
		ac_cv_nfs_fh_dref_style=sunos4 ;;
	svr4* | sysv4* | solaris* | sunos* | hpux* )
		ac_cv_nfs_fh_dref_style=svr4 ;;
	bsd44* | bsdi2* | freebsd2.[[01]]* )
		ac_cv_nfs_fh_dref_style=bsd44 ;;
	# all new BSDs changed the type of the
	# filehandle in nfs_args from nfsv2fh_t to u_char.
	freebsd* | freebsdelf* | bsdi* | netbsd* | openbsd* | darwin* | rhapsody* )
		ac_cv_nfs_fh_dref_style=freebsd22 ;;
	aix[[1-3]]* | aix4.[[01]]* )
		ac_cv_nfs_fh_dref_style=aix3 ;;
	aix* )
		ac_cv_nfs_fh_dref_style=aix42 ;;
	irix* )
		ac_cv_nfs_fh_dref_style=irix ;;
	linux* )
		ac_cv_nfs_fh_dref_style=linux ;;
	isc3 )
		ac_cv_nfs_fh_dref_style=isc3 ;;
	osf[[1-3]]* )
		ac_cv_nfs_fh_dref_style=osf2 ;;
	osf* )
		ac_cv_nfs_fh_dref_style=osf4 ;;
	nextstep* )
		ac_cv_nfs_fh_dref_style=nextstep ;;
	* )
		ac_cv_nfs_fh_dref_style=default ;;
esac
])
am_utils_nfs_fh_dref=$srcdir"/conf/fh_dref/fh_dref_"$ac_cv_nfs_fh_dref_style".h"
AC_SUBST_FILE(am_utils_nfs_fh_dref)
])
dnl ======================================================================
