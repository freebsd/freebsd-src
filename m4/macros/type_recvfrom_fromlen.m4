dnl ######################################################################
dnl check the correct type for the 6th argument to recvfrom()
AC_DEFUN([AMU_TYPE_RECVFROM_FROMLEN],
[
AC_CACHE_CHECK(non-pointer type of 6th (fromlen) argument to recvfrom(),
ac_cv_recvfrom_fromlen,
[
# select the correct type
case "${host_os}" in
	linux* )
		ac_cv_recvfrom_fromlen="unsigned int" ;;
	aix[[1-3]]* )
		ac_cv_recvfrom_fromlen="int" ;;
	aix* )
		ac_cv_recvfrom_fromlen="size_t" ;;
	# old macosx used "int" but all newer ones use socklen_t
	macosx-10.[[0-3]]* )
		ac_cv_recvfrom_fromlen="int" ;;
	macosx* )
		ac_cv_recvfrom_fromlen="socklen_t" ;;
	*bsd* )
		ac_cv_recvfrom_fromlen="socklen_t" ;;
	* )
		ac_cv_recvfrom_fromlen="int" ;;
esac
])
AC_DEFINE_UNQUOTED(RECVFROM_FROMLEN_TYPE, $ac_cv_recvfrom_fromlen)
])
dnl ======================================================================
