#
# SYNOPSIS
#
#   NTP_FUNC_REALPATH
#
# DESCRIPTION
#
#   This macro defines HAVE_FUNC_REALPATH if we have a realpath()
#   function that accepts NULL as the 2nd argument.
#
# LICENSE
#
#   Copyright (c) 2020 Network Time Foundation
#
#   Author: Harlan Stenn <stenn@nwtime.org>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 1

AC_DEFUN([NTP_FUNC_REALPATH], [
	AC_MSG_CHECKING([for a POSIX-2008 compliant realpath()])
	AC_REQUIRE([AC_PROG_CC_C99])

	AC_LANG_PUSH([C])

	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
			#include <sys/param.h>
			#include <stdlib.h>
			int main() { return (NULL == realpath(".", NULL)); }
			]])],
		ans="yes",
		ans="no",
		ans="CROSS COMPILE!"
		)
	AC_MSG_RESULT([$ans])
	case "$ans" in
	 yes)
	    AC_DEFINE([HAVE_FUNC_POSIX_REALPATH], [1],
			[Define to 1 if we have realpath() that supports NULL as the 2nd argument])
	    ;;
	esac

	AC_LANG_POP([C])
	]);
