dnl ######################################################################
dnl check the correct network transport type to use
AC_DEFUN(AMU_CHECK_NETWORK_TRANSPORT_TYPE,
[
AC_CACHE_CHECK(network transport type,
ac_cv_transport_type,
[
# select the correct type
case "${host_os_name}" in
	solaris1* | sunos[[34]]* | hpux[[6-9]]* | hpux10* )
		ac_cv_transport_type=sockets ;;
	solaris* | sunos* | hpux* )
		ac_cv_transport_type=tli ;;
	* )
		ac_cv_transport_type=sockets ;;
esac
])
am_utils_link_files=${am_utils_link_files}libamu/transputil.c:conf/transp/transp_${ac_cv_transport_type}.c" "

# append transport utilities object to LIBOBJS for automatic compilation
AC_LIBOBJ(transputil)
if test $ac_cv_transport_type = tli
then
  AC_DEFINE(HAVE_TRANSPORT_TYPE_TLI)
fi
])
dnl ======================================================================
