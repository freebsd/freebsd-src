dnl
dnl $Id$
dnl
dnl stuff used by telnet

AC_DEFUN([rk_TELNET],[
AC_DEFINE(AUTHENTICATION, 1, 
	[Define if you want authentication support in telnet.])dnl
AC_DEFINE(ENCRYPTION, 1,
	[Define if you want encryption support in telnet.])dnl
AC_DEFINE(DES_ENCRYPTION, 1,
	[Define if you want to use DES encryption in telnet.])dnl
AC_DEFINE(DIAGNOSTICS, 1,
	[Define this to enable diagnostics in telnet.])dnl
AC_DEFINE(OLD_ENVIRON, 1,
	[Define this to enable old environment option in telnet.])dnl
if false; then
	AC_DEFINE(ENV_HACK, 1,
		[Define this if you want support for broken ENV_{VAR,VAL} telnets.])
fi

# Simple test for streamspty, based on the existance of getmsg(), alas
# this breaks on SunOS4 which have streams but BSD-like ptys
#
# And also something wierd has happend with dec-osf1, fallback to bsd-ptys

case "$host" in
*-*-aix3*|*-*-sunos4*|*-*-osf*|*-*-hpux1[[01]]*)
	;;
*)
	AC_CHECK_FUNC(getmsg)
	if test "$ac_cv_func_getmsg" = "yes"; then
		AC_CACHE_CHECK([if getmsg works], ac_cv_func_getmsg_works,
		AC_RUN_IFELSE([AC_LANG_SOURCE([[
			#include <stdio.h>
			#include <errno.h>

			int main(int argc, char **argv)
			{
			  int ret;
			  ret = getmsg(open("/dev/null", 0), NULL, NULL, NULL);
			  if(ret < 0 && errno == ENOSYS)
			    return 1;
			  return 0;
			}
			]])], [ac_cv_func_getmsg_works=yes], 
			[ac_cv_func_getmsg_works=no],
			[ac_cv_func_getmsg_works=no]))
		if test "$ac_cv_func_getmsg_works" = "yes"; then
			AC_DEFINE(HAVE_GETMSG, 1,
				[Define if you have a working getmsg.])
			AC_DEFINE(STREAMSPTY, 1,
				[Define if you have streams ptys.])
		fi
	fi
	;;
esac

AH_BOTTOM([

/* Set this to the default system lead string for telnetd 
 * can contain %-escapes: %s=sysname, %m=machine, %r=os-release
 * %v=os-version, %t=tty, %h=hostname, %d=date and time
 */
#undef USE_IM

/* Used with login -p */
#undef LOGIN_ARGS

/* set this to a sensible login */
#ifndef LOGIN_PATH
#define LOGIN_PATH BINDIR "/login"
#endif
])
])
