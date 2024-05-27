dnl ######################################################################
dnl Common m4sh code for compiler stuff
AC_DEFUN([NTP_COMPILER], [
AC_REQUIRE([AC_PROG_CC_STDC])

AC_USE_SYSTEM_EXTENSIONS

CC_NOFORMAT=
CFLAGS_NTP=
CPPFLAGS_NTP=
LDADD_NTP=
LDFLAGS_NTP=
AC_SUBST([CC_NOFORMAT])
AC_SUBST([CFLAGS_NTP])
AC_SUBST([CPPFLAGS_NTP])
AC_SUBST([LDADD_NTP])
AC_SUBST([LDFLAGS_NTP])

case "$ac_cv_prog_cc_c89" in
 no)
    AC_MSG_WARN([ANSI C89/ISO C90 is the minimum to compile NTP]
		[ version 4.2.5 and higher.])
    ;;
esac

AC_CACHE_CHECK(
    [if $CC can handle @%:@warning],
    [ntp_cv_cpp_warning],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM([[]], [[#warning foo]])],
	[ntp_cv_cpp_warning=yes],
	[ntp_cv_cpp_warning=no]
    )]
)
case "$ntp_cv_cpp_warning" in
 no)
    AC_DEFINE([NO_OPTION_NAME_WARNINGS], [1],
	[Should we avoid @%:@warning on option name collisions?])
esac

AC_CACHE_CHECK(
    [if $CC supports __attribute__((...))],
    [ntp_cv_cc_attribute],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[]],
	    [[void foo(void) __attribute__((__noreturn__));]]
	    )],
	[ntp_cv_cc_attribute=yes],
	[ntp_cv_cc_attribute=no]
    )]
)
case "$ntp_cv_cc_attribute" in
 yes)
    AC_DEFINE([HAVE___ATTRIBUTE__], [],
	[defined if C compiler supports __attribute__((...))])
esac
AH_VERBATIM(
    [HAVE___ATTRIBUTE___VERBATIM],
    [
	/* define away __attribute__() if unsupported */
	#ifndef HAVE___ATTRIBUTE__
	# define __attribute__(x) /* empty */
	#endif
	#define ISC_PLATFORM_NORETURN_PRE
	#define ISC_PLATFORM_NORETURN_POST __attribute__((__noreturn__))
    ]
)

case "$GCC" in
 yes)
    SAVED_CFLAGS="$CFLAGS"
    CFLAGS="$SAVED_CFLAGS -Wstrict-overflow"
    AC_CACHE_CHECK(
	[if $CC can handle -Wstrict-overflow], 
	[ntp_cv_gcc_Wstrict_overflow], 
	[AC_COMPILE_IFELSE(
	    [AC_LANG_PROGRAM([[]], [[]])],
	    [ntp_cv_gcc_Wstrict_overflow=yes],
	    [ntp_cv_gcc_Wstrict_overflow=no]
	)	]
    )
    #
    # $ntp_cv_gcc_Wstrict_overflow is tested later to add the 
    # flag to CFLAGS.
    #
    CFLAGS="$SAVED_CFLAGS -Winit-self"
    AC_CACHE_CHECK(
	[if $CC can handle -Winit-self], 
	[ntp_cv_gcc_Winit_self],
	[
	    AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([[]], [[]])],
		[ntp_cv_gcc_Winit_self=yes],
		[ntp_cv_gcc_Winit_self=no]
	    )
	]
    )
    #
    # $ntp_cv_gcc_Winit_self is tested later to add the 
    # flag to CFLAGS_NTP.
    #
    # libopts specifically builds a string with embedded NULs.
    # This causes a bunch of distracting warnings due to -Wformat.
    #
    CFLAGS="$SAVED_CFLAGS -Wno-format -Wno-format-security"
    AC_CACHE_CHECK(
	[if $CC can handle -Wno-format -Wno-format-security], 
	[ntp_cv_gcc_Wno_format],
	[
	    AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([[]], [[]])],
		[ntp_cv_gcc_Wno_format=yes],
		[ntp_cv_gcc_Wno_format=no]
	    )
	]
    )
    case "$ntp_cv_gcc_Wno_format" in
      no) ntp_cv_gcc_Wno_format_truncation=no
	  ;;
     yes)
	CC_NOFORMAT="-Wno-format -Wno-format-security"
	CFLAGS="$SAVED_CFLAGS -Wformat -Wno-format-truncation -Werror"
	AC_CACHE_CHECK(
		[if $CC can handle -Wformat -Wno-format-truncation], 
		[ntp_cv_gcc_Wno_format_truncation], 
		[AC_COMPILE_IFELSE(
		    [AC_LANG_PROGRAM([[]], [[]])],
		    [ntp_cv_gcc_Wno_format_truncation=yes],
		    [ntp_cv_gcc_Wno_format_truncation=no]
		)	]
	)
	#
	# $ntp_cv_gcc_Wno_format_truncation is tested later to add the 
	# flag to CFLAGS.
	#
    esac

    CFLAGS="$SAVED_CFLAGS"
    AS_UNSET([SAVED_CFLAGS])

    AC_CACHE_CHECK(
	[if linker supports omitting unused code and data],
	[ntp_cv_gc_sections_runs],
	[
	    #  NetBSD will link but likely not run with --gc-sections
	    #  http://bugs.ntp.org/1844
	    #  http://gnats.netbsd.org/40401
	    #  --gc-sections causes attempt to load as linux elf, with
	    #  wrong syscalls in place.  Test a little gauntlet of
	    #  simple stdio read code checking for errors, expecting
	    #  enough syscall differences that the NetBSD code will
	    #  fail even with Linux emulation working as designed.
	    #  A shorter test could be refined by someone with access
	    #  to a NetBSD host with Linux emulation working.
	    origCFLAGS="$CFLAGS"
	    CFLAGS="$CFLAGS -Wl,--gc-sections"
	    AC_LINK_IFELSE(
		[AC_LANG_PROGRAM(
		    [[
			#include <stdlib.h>
			#include <stdio.h>
		    ]],
		    [[
			FILE *	fpC;
			char	buf[32];
			size_t	cch;
			int	read_success_once;

			fpC = fopen("conftest.c", "r");
			if (NULL == fpC)
				exit(1);
			do {
				cch = fread(buf, sizeof(buf), 1, fpC);
				read_success_once |= (0 != cch);
			} while (0 != cch);
			if (!read_success_once)
				exit(2);
			if (!feof(fpC))
				exit(3);
			if (0 != fclose(fpC))
				exit(4);

			exit(EXIT_SUCCESS);
		    ]]
		)],
		[
		    if test "X$cross_compiling" = "Xyes" || grep gc-sections conftest.err ; then
			ntp_cv_gc_sections_runs=no
		    else
			ntp_cv_gc_sections_runs=no
			./conftest >/dev/null 2>&1 && ntp_cv_gc_sections_runs=yes
		    fi
		],
		[ntp_cv_gc_sections_runs=no]
	    )
	    CFLAGS="$origCFLAGS"
	    AS_UNSET([origCFLAGS])
	]
    )
    case "$ntp_cv_gc_sections_runs" in
     yes)
	LDADD_LIBNTP="-Wl,--gc-sections"
	CFLAGS_NTP="$CFLAGS_NTP -ffunction-sections -fdata-sections"
	;;
     no)
	LDADD_LIBNTP=
	;;
    esac
    CFLAGS_NTP="$CFLAGS_NTP -Wall"
    CFLAGS_NTP="$CFLAGS_NTP -Wcast-align"
    CFLAGS_NTP="$CFLAGS_NTP -Wcast-qual"
    # CFLAGS_NTP="$CFLAGS_NTP -Wconversion"
    # CFLAGS_NTP="$CFLAGS_NTP -Werror"
    # CFLAGS_NTP="$CFLAGS_NTP -Wextra"
    # CFLAGS_NTP="$CFLAGS_NTP -Wfloat-equal"
    CFLAGS_NTP="$CFLAGS_NTP -Wmissing-prototypes"
    CFLAGS_NTP="$CFLAGS_NTP -Wpointer-arith"
    CFLAGS_NTP="$CFLAGS_NTP -Wshadow"
    # CFLAGS_NTP="$CFLAGS_NTP -Wtraditional"
    # CFLAGS_NTP="$CFLAGS_NTP -Wwrite-strings"
    case "$ntp_cv_gcc_Winit_self" in
     yes)
	CFLAGS_NTP="$CFLAGS_NTP -Winit-self"
    esac
    case "$ntp_cv_gcc_Wstrict_overflow" in
     yes)
	CFLAGS_NTP="$CFLAGS_NTP -Wstrict-overflow"
    esac
    case "$ntp_cv_gcc_Wno_format_truncation" in
     yes)
	CFLAGS_NTP="$CFLAGS_NTP -Wno-format-truncation"
    esac
    # -W[no-]strict-prototypes might be added by NTP_OPENSSL
esac

NTP_OS_CFLAGS

AC_C_BIGENDIAN
AC_C_VOLATILE
AC_PROG_CPP

])dnl
dnl ======================================================================
