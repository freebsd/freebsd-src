dnl ######################################################################
dnl Check syslog.h for 'facilitynames' table
AC_DEFUN([NTP_FACILITYNAMES], [

AC_CACHE_CHECK(
    [for facilitynames in syslog.h],
    [ac_cv_HAVE_SYSLOG_FACILITYNAMES],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#define SYSLOG_NAMES
		#include <stdlib.h>
		#include <syslog.h>
	    ]],
	    [[
		void *fnames = facilitynames;
	    ]]
	)]
	[ac_cv_HAVE_SYSLOG_FACILITYNAMES=yes],
	[ac_cv_HAVE_SYSLOG_FACILITYNAMES=no]
    )]
)
case "$ac_cv_HAVE_SYSLOG_FACILITYNAMES" in
 yes)
    AC_DEFINE([HAVE_SYSLOG_FACILITYNAMES], [1], [syslog.h provides facilitynames])
    ;;
 no)
    AC_MSG_WARN([No facilitynames in <syslog.h>])
esac
])
dnl ======================================================================
