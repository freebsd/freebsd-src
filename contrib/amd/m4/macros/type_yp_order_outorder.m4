dnl ######################################################################
dnl check the correct type for the 3rd argument to yp_order()
AC_DEFUN(AMU_TYPE_YP_ORDER_OUTORDER,
[
AC_CACHE_CHECK(pointer type of 3rd argument to yp_order(),
ac_cv_yp_order_outorder,
[
# select the correct type
case "${host_os}" in
	aix[[1-3]]* | aix4.[[0-2]]* | sunos[[34]]* | solaris1* )
		ac_cv_yp_order_outorder=int ;;
	solaris* | svr4* | sysv4* | sunos* | hpux* | aix* )
		ac_cv_yp_order_outorder="unsigned long" ;;
	osf* )
		# DU4 man page is wrong, headers are right
		ac_cv_yp_order_outorder="unsigned int" ;;
	* )
		ac_cv_yp_order_outorder=int ;;
esac
])
AC_DEFINE_UNQUOTED(YP_ORDER_OUTORDER_TYPE, $ac_cv_yp_order_outorder)
])
dnl ======================================================================
