dnl ######################################################################
dnl check the autofs flavor
AC_DEFUN([AMU_CHECK_AUTOFS_STYLE],
[
AC_CACHE_CHECK(autofs style,
ac_cv_autofs_style,
[
# select the correct style to mount(2) a filesystem
case "${host_os}" in
       solaris1* | solaris2.[[0-4]] )
	       ac_cv_autofs_style=default ;;
       solaris2.5* )
               ac_cv_autofs_style=solaris_v1 ;;
       # Solaris 8+ uses the AutoFS V3/V4 protocols, but they are very similar
       # to V2, so use one style for all.
       solaris* )
               ac_cv_autofs_style=solaris_v2_v3 ;;
       irix6* )
	       ac_cv_autofs_style=solaris_v1 ;;
       linux* )
               ac_cv_autofs_style=linux ;;
       * )
               ac_cv_autofs_style=default ;;
esac
])
# always make a link and include the file name, otherwise on systems where
# autofs support has not been ported yet check_fs_{headers, mntent}.m4 add
# ops_autofs.o to AMD_FS_OBJS, but there's no way to build it.
am_utils_link_files=${am_utils_link_files}amd/ops_autofs.c:conf/autofs/autofs_${ac_cv_autofs_style}.c" "amu_autofs_prot.h:conf/autofs/autofs_${ac_cv_autofs_style}.h" "

# set headers in a macro for Makefile.am files to use (for dependencies)
AMU_AUTOFS_PROT_HEADER='${top_builddir}/'amu_autofs_prot.h
AC_SUBST(AMU_AUTOFS_PROT_HEADER)
])
dnl ======================================================================
