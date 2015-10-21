dnl ######################################################################
dnl rlimit capabilities checks
AC_DEFUN([NTP_RLIMIT_ITEMS], [

AC_CACHE_CHECK(
    [for RLIMIT_MEMLOCK],
    [ntp_cv_rlimit_memlock],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_SYS_TYPES_H
		# include <sys/types.h>
		#endif
		#ifdef HAVE_SYS_TIME_H
		# include <sys/time.h>
		#endif
		#ifdef HAVE_SYS_RESOURCE_H
		# include <sys/resource.h>
		#endif
	    ]],
	    [[
		getrlimit(RLIMIT_MEMLOCK, 0);
	    ]]
	)],
	[ntp_cv_rlimit_memlock=yes],
	[ntp_cv_rlimit_memlock=no]
    )]
)
case "$ntp_cv_rlimit_memlock" in
 yes)
    AC_SUBST([HAVE_RLIMIT_MEMLOCK])
    HAVE_RLIMIT_MEMLOCK=" memlock 32"
esac

AC_CACHE_CHECK(
    [for RLIMIT_STACK],
    [ntp_cv_rlimit_stack],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_SYS_TYPES_H
		# include <sys/types.h>
		#endif
		#ifdef HAVE_SYS_TIME_H
		# include <sys/time.h>
		#endif
		#ifdef HAVE_SYS_RESOURCE_H
		# include <sys/resource.h>
		#endif
	    ]],
	    [[
		getrlimit(RLIMIT_STACK, 0);
	    ]]
	)],
	[ntp_cv_rlimit_stack=yes],
	[ntp_cv_rlimit_stack=no]
    )]
)
case "$ntp_cv_rlimit_stack" in
 yes)
    AC_SUBST([HAVE_RLIMIT_STACK])
    HAVE_RLIMIT_STACK=" stacksize 50"
esac

])dnl
dnl ======================================================================
