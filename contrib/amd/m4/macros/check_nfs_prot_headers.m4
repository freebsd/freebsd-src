dnl ######################################################################
dnl check if system has NFS protocol headers
AC_DEFUN(AMU_CHECK_NFS_PROT_HEADERS,
[
AC_CACHE_CHECK(location of NFS protocol header files,
ac_cv_nfs_prot_headers,
[
# select the correct style for mounting filesystems
case "${host_os}" in
	irix5* )
			ac_cv_nfs_prot_headers=irix5 ;;
	irix* )
			ac_cv_nfs_prot_headers=irix6 ;;
	sunos3* )
			ac_cv_nfs_prot_headers=sunos3 ;;
	sunos4* | solaris1* )
			ac_cv_nfs_prot_headers=sunos4 ;;
	sunos5.[[0-3]]* | solaris2.[[0-3]]* )
			ac_cv_nfs_prot_headers=sunos5_3 ;;
	sunos5.4* | solaris2.4* )
			ac_cv_nfs_prot_headers=sunos5_4 ;;
	sunos5.5* | solaris2.5* )
			ac_cv_nfs_prot_headers=sunos5_5 ;;
	sunos5.6* | solaris2.6* )
			ac_cv_nfs_prot_headers=sunos5_6 ;;
	sunos5.7* | solaris2.7* )
			ac_cv_nfs_prot_headers=sunos5_7 ;;
	sunos* | solaris* )
			ac_cv_nfs_prot_headers=sunos5_8 ;;
	bsdi2*)
			ac_cv_nfs_prot_headers=bsdi2 ;;
	bsdi* )
			ac_cv_nfs_prot_headers=bsdi3 ;;
	freebsd2* )
			ac_cv_nfs_prot_headers=freebsd2 ;;
	freebsd* | freebsdelf* )
			ac_cv_nfs_prot_headers=freebsd3 ;;
	netbsd1.[[0-2]]* )
			ac_cv_nfs_prot_headers=netbsd ;;
	netbsd1.3* )
			ac_cv_nfs_prot_headers=netbsd1_3 ;;
	netbsd* | netbsdelf* )
			ac_cv_nfs_prot_headers=netbsd1_4 ;;
	openbsd* )
			ac_cv_nfs_prot_headers=openbsd ;;
	hpux[[6-9]]* | hpux10* )
			ac_cv_nfs_prot_headers=hpux ;;
	hpux* )
			ac_cv_nfs_prot_headers=hpux11 ;;
	aix[[1-3]]* )
			ac_cv_nfs_prot_headers=aix3 ;;
	aix4.[[01]]* )
			ac_cv_nfs_prot_headers=aix4 ;;
	aix4.2* )
			ac_cv_nfs_prot_headers=aix4_2 ;;
	aix4.3* )
			ac_cv_nfs_prot_headers=aix4_3 ;;
	aix* )
			ac_cv_nfs_prot_headers=aix5_1 ;;
	osf[[1-3]]* )
			ac_cv_nfs_prot_headers=osf2 ;;
	osf4* )
			ac_cv_nfs_prot_headers=osf4 ;;
	osf* )
			ac_cv_nfs_prot_headers=osf5 ;;
	svr4* )
			ac_cv_nfs_prot_headers=svr4 ;;
	sysv4* )	# this is for NCR2 machines
			ac_cv_nfs_prot_headers=ncr2 ;;
	linux* )
			ac_cv_nfs_prot_headers=linux ;;
	nextstep* )
			ac_cv_nfs_prot_headers=nextstep ;;
	ultrix* )
			ac_cv_nfs_prot_headers=ultrix ;;
 	darwin* | rhapsody* )
 			ac_cv_nfs_prot_headers=darwin ;;
	* )
			ac_cv_nfs_prot_headers=default ;;
esac
])

# make sure correct header is linked in top build directory
am_utils_nfs_prot_file="amu_nfs_prot.h"
am_utils_link_files=${am_utils_link_files}${am_utils_nfs_prot_file}:conf/nfs_prot/nfs_prot_${ac_cv_nfs_prot_headers}.h" "

# define the name of the header to be included for other M4 macros
AC_DEFINE_UNQUOTED(AMU_NFS_PROTOCOL_HEADER, "${srcdir}/conf/nfs_prot/nfs_prot_${ac_cv_nfs_prot_headers}.h")

# set headers in a macro for Makefile.am files to use (for dependencies)
AMU_NFS_PROT_HEADER='${top_builddir}/'$am_utils_nfs_prot_file
AC_SUBST(AMU_NFS_PROT_HEADER)
])
dnl ======================================================================
