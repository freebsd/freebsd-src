# Copyright © 2013 Red Hat, Inc.
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that copyright
# notice and this permission notice appear in supporting documentation, and
# that the name of the copyright holders not be used in advertising or
# publicity pertaining to distribution of the software without specific,
# written prior permission.  The copyright holders make no representations
# about the suitability of this software for any purpose.  It is provided "as
# is" without express or implied warranty.
#
# THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
# INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
# EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
# DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
# OF THIS SOFTWARE.

AC_PREREQ([2.62])

AC_INIT([libevdev],
        [1.2.99.1],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=libevdev],
        [libevdev],
        [http://freedesktop.org/wiki/Software/libevdev/])

AC_CONFIG_SRCDIR([libevdev/libevdev.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])

# Before making a release, the LIBEVDEV_LT_VERSION string should be
# modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A
LIBEVDEV_LT_VERSION=2:3:0
AC_SUBST(LIBEVDEV_LT_VERSION)


AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC_C99

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT
LT_PATH_LD

with_ldflags=""
if test "x$lt_cv_prog_gnu_ld" = "xyes"; then
	CC_CHECK_FLAGS_APPEND([with_ldflags], [LDFLAGS], [\
				-Wl,--as-needed \
				-Wl,--gc-sections \
				-Wl,-z,relro \
				-Wl,-z,now])
fi
AC_SUBST([GNU_LD_FLAGS], $with_ldflags)

PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(CHECK, [check >= 0.9.9], [HAVE_CHECK="yes"], [HAVE_CHECK="no"])
if test "x$HAVE_CHECK" = "xyes"; then
	AC_PATH_PROG(VALGRIND, [valgrind])
else
	AC_MSG_WARN([check not found - skipping building unit tests])
fi
AM_CONDITIONAL(HAVE_VALGRIND, [test "x$VALGRIND" != "x"])
AM_CONDITIONAL(BUILD_TESTS, [test "x$HAVE_CHECK" = "xyes"])

with_cflags=""
if test "x$GCC" = "xyes"; then
	CC_CHECK_FLAGS_APPEND([with_cflags], [CFLAGS], [\
				-Wall \
				-Wextra \
				-Wno-unused-parameter \
				-Wstrict-prototypes \
				-Wmissing-prototypes \
				-fvisibility=hidden \
				-pipe \
				-fno-strict-aliasing \
				-ffunction-sections \
				-fdata-sections \
				-fno-strict-aliasing \
				-fdiagnostics-show-option \
				-fno-common])
fi
AC_SUBST([GCC_CFLAGS], $with_cflags)

AC_PATH_PROG(DOXYGEN, [doxygen])
if test "x$DOXYGEN" = "x"; then
	AC_MSG_WARN([doxygen not found - required for documentation])
	have_doxygen="no"
else
	have_doxygen="yes"
fi
AM_CONDITIONAL([HAVE_DOXYGEN], [test "x$have_doxygen" = "xyes"])

AC_MSG_CHECKING([whether to build with gcov])
AC_ARG_ENABLE([gcov],
	      [AS_HELP_STRING([--enable-gcov],
			      [Whether to enable coverage testing (default:disabled)])],
	      [],
	      [enable_gcov=no],
	      )
AS_IF([test "x$enable_gcov" != "xno"],
      [
       GCOV_CFLAGS="-fprofile-arcs -ftest-coverage"
       GCOV_LDFLAGS="-fprofile-arcs -ftest-coverage"
       enable_gcov=yes
       ],
)

AM_PATH_PYTHON([2.6])

AM_CONDITIONAL([GCOV_ENABLED], [test "x$enable_gcov" != "xno"])
AC_SUBST([GCOV_CFLAGS])
AC_SUBST([GCOV_LDFLAGS])
AC_MSG_RESULT([$enable_gcov])

AC_CONFIG_FILES([Makefile
		 libevdev/Makefile
		 doc/Makefile
		 doc/libevdev.doxygen
		 doc/libevdev.man
		 tools/Makefile
		 test/Makefile
		 libevdev.pc])
AC_OUTPUT

AC_MSG_RESULT([
	       Build documentation	${have_doxygen}
	       Build unit-tests		${HAVE_CHECK}
	       Enable profiling		${enable_gcov}
	       ])
