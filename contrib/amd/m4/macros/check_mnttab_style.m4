dnl ######################################################################
dnl check style of accessing the mount table file
AC_DEFUN(AMU_CHECK_MNTTAB_STYLE,
[
AC_CACHE_CHECK(mount table style,
ac_cv_style_mnttab,
[
# select the correct style for mount table manipulation functions
case "${host_os_name}" in
	aix* )
			ac_cv_style_mnttab=aix ;;
	bsd* | bsdi* | freebsd* | netbsd* | openbsd* | darwin* | rhapsody* )
			ac_cv_style_mnttab=bsd ;;
	isc3* )
			ac_cv_style_mnttab=isc3 ;;
	mach3* )
			ac_cv_style_mnttab=mach3 ;;
	osf* )
			ac_cv_style_mnttab=osf ;;
	svr4* | sysv4* | solaris2* | sunos5* | aoi* )
			ac_cv_style_mnttab=svr4 ;;
	ultrix* )
			ac_cv_style_mnttab=ultrix ;;
	* )
			ac_cv_style_mnttab=file ;;
esac
])
am_utils_link_files=${am_utils_link_files}libamu/mtabutil.c:conf/mtab/mtab_${ac_cv_style_mnttab}.c" "

# append mtab utilities object to LIBOBJS for automatic compilation
AC_LIBOBJ(mtabutil)
])
dnl ======================================================================
