dnl $Id: mips-abi.m4,v 1.6 2002/04/30 16:46:05 joda Exp $
dnl
dnl
dnl Check for MIPS/IRIX ABI flags. Sets $abi and $abilibdirext to some
dnl value.

AC_DEFUN(AC_MIPS_ABI, [
AC_ARG_WITH(mips_abi,
	AC_HELP_STRING([--with-mips-abi=abi],[ABI to use for IRIX (32, n32, or 64)]))

case "$host_os" in
irix*)
with_mips_abi="${with_mips_abi:-yes}"
if test -n "$GCC"; then

# GCC < 2.8 only supports the O32 ABI. GCC >= 2.8 has a flag to select
# which ABI to use, but only supports (as of 2.8.1) the N32 and 64 ABIs.
#
# Default to N32, but if GCC doesn't grok -mabi=n32, we assume an old
# GCC and revert back to O32. The same goes if O32 is asked for - old
# GCCs doesn't like the -mabi option, and new GCCs can't output O32.
#
# Don't you just love *all* the different SGI ABIs?

case "${with_mips_abi}" in 
        32|o32) abi='-mabi=32';  abilibdirext=''     ;;
       n32|yes) abi='-mabi=n32'; abilibdirext='32'  ;;
        64) abi='-mabi=64';  abilibdirext='64'   ;;
	no) abi=''; abilibdirext='';;
         *) AC_MSG_ERROR("Invalid ABI specified") ;;
esac
if test -n "$abi" ; then
ac_foo=krb_cv_gcc_`echo $abi | tr =- __`
dnl
dnl can't use AC_CACHE_CHECK here, since it doesn't quote CACHE-ID to
dnl AC_MSG_RESULT
dnl
AC_MSG_CHECKING([if $CC supports the $abi option])
AC_CACHE_VAL($ac_foo, [
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $abi"
AC_TRY_COMPILE(,int x;, eval $ac_foo=yes, eval $ac_foo=no)
CFLAGS="$save_CFLAGS"
])
ac_res=`eval echo \\\$$ac_foo`
AC_MSG_RESULT($ac_res)
if test $ac_res = no; then
# Try to figure out why that failed...
case $abi in
	-mabi=32) 
	save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -mabi=n32"
	AC_TRY_COMPILE(,int x;, ac_res=yes, ac_res=no)
	CLAGS="$save_CFLAGS"
	if test $ac_res = yes; then
		# New GCC
		AC_MSG_ERROR([$CC does not support the $with_mips_abi ABI])
	fi
	# Old GCC
	abi=''
	abilibdirext=''
	;;
	-mabi=n32|-mabi=64)
		if test $with_mips_abi = yes; then
			# Old GCC, default to O32
			abi=''
			abilibdirext=''
		else
			# Some broken GCC
			AC_MSG_ERROR([$CC does not support the $with_mips_abi ABI])
		fi
	;;
esac
fi #if test $ac_res = no; then
fi #if test -n "$abi" ; then
else
case "${with_mips_abi}" in
        32|o32) abi='-32'; abilibdirext=''     ;;
       n32|yes) abi='-n32'; abilibdirext='32'  ;;
        64) abi='-64'; abilibdirext='64'   ;;
	no) abi=''; abilibdirext='';;
         *) AC_MSG_ERROR("Invalid ABI specified") ;;
esac
fi #if test -n "$GCC"; then
;;
esac
])
