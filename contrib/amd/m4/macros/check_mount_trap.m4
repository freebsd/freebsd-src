dnl ######################################################################
dnl check the mount system call trap needed to mount(2) a filesystem
AC_DEFUN(AMU_CHECK_MOUNT_TRAP,
[
AC_CACHE_CHECK(mount trap system-call style,
ac_cv_mount_trap,
[
# select the correct style to mount(2) a filesystem
case "${host_os_name}" in
	solaris1* | sunos[[34]]* )
		ac_cv_mount_trap=default ;;
	hpux[[6-9]]* | hpux10* )
		ac_cv_mount_trap=hpux ;;
	svr4* | sysv4* | solaris* | sunos* | aoi* | hpux* )
		ac_cv_mount_trap=svr4 ;;
	news4* | riscix* )
		ac_cv_mount_trap=news4 ;;
	linux* )
		ac_cv_mount_trap=linux ;;
	irix* )
		ac_cv_mount_trap=irix ;;
	aux* )
		ac_cv_mount_trap=aux ;;
	hcx* )
		ac_cv_mount_trap=hcx ;;
	rtu6* )
		ac_cv_mount_trap=rtu6 ;;
	dgux* )
		ac_cv_mount_trap=dgux ;;
	aix* )
		ac_cv_mount_trap=aix3 ;;
	mach2* | mach3* )
		ac_cv_mount_trap=mach3 ;;
	ultrix* )
		ac_cv_mount_trap=ultrix ;;
	isc3* )
		ac_cv_mount_trap=isc3 ;;
	stellix* )
		ac_cv_mount_trap=stellix ;;
	* )
		ac_cv_mount_trap=default ;;
esac
])
am_utils_mount_trap=$srcdir"/conf/trap/trap_"$ac_cv_mount_trap".h"
AC_SUBST_FILE(am_utils_mount_trap)
])
dnl ======================================================================
