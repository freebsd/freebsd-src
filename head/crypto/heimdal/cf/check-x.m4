dnl 
dnl See if there is any X11 present
dnl
dnl $Id: check-x.m4 15435 2005-06-16 19:45:52Z lha $

AC_DEFUN([KRB_CHECK_X],[
AC_PATH_XTRA

# try to figure out if we need any additional ld flags, like -R
# and yes, the autoconf X test is utterly broken
if test "$no_x" != yes; then
	AC_CACHE_CHECK(for special X linker flags,krb_cv_sys_x_libs_rpath,[
	ac_save_libs="$LIBS"
	ac_save_cflags="$CFLAGS"
	CFLAGS="$CFLAGS $X_CFLAGS"
	krb_cv_sys_x_libs_rpath=""
	krb_cv_sys_x_libs=""
	for rflag in "" "-R" "-R " "-rpath "; do
		if test "$rflag" = ""; then
			foo="$X_LIBS"
		else
			foo=""
			for flag in $X_LIBS; do
			case $flag in
			-L*)
				foo="$foo $flag `echo $flag | sed \"s/-L/$rflag/\"`"
				;;
			*)
				foo="$foo $flag"
				;;
			esac
			done
		fi
		LIBS="$ac_save_libs $foo $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
		AC_RUN_IFELSE([
		#include <X11/Xlib.h>
		foo(void)
		{
		XOpenDisplay(NULL);
		}
		main(int argc, char **argv)
		{
		return 0;
		}
		],krb_cv_sys_x_libs_rpath="$rflag"; krb_cv_sys_x_libs="$foo"; break,:,
		krb_cv_sys_x_libs_rpath="" ; krb_cv_sys_x_libs="" ; break)
	done
	LIBS="$ac_save_libs"
	CFLAGS="$ac_save_cflags"
	])
	X_LIBS="$krb_cv_sys_x_libs"
fi
])
