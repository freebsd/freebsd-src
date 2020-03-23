dnl Copyright 2010 Google Inc.
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are
dnl met:
dnl
dnl * Redistributions of source code must retain the above copyright
dnl   notice, this list of conditions and the following disclaimer.
dnl * Redistributions in binary form must reproduce the above copyright
dnl   notice, this list of conditions and the following disclaimer in the
dnl   documentation and/or other materials provided with the distribution.
dnl * Neither the name of Google Inc. nor the names of its contributors
dnl   may be used to endorse or promote products derived from this software
dnl   without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
dnl "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
dnl LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
dnl A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
dnl OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
dnl SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
dnl LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
dnl DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
dnl THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
dnl (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
dnl OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

dnl \file compiler-flags.m4
dnl
dnl Macros to check for the existence of compiler flags.  The macros in this
dnl file support both C and C++.
dnl
dnl Be aware that, in order to detect a flag accurately, we may need to enable
dnl strict warning checking in the compiler (i.e. enable -Werror).  Some
dnl compilers, e.g. Clang, report unknown -W flags as warnings unless -Werror is
dnl selected.  This fact would confuse the flag checks below because we would
dnl conclude that a flag is valid while in reality it is not.  To resolve this,
dnl the macros below will pass -Werror to the compiler along with any other flag
dnl being checked.


dnl Checks for a compiler flag and sets a result variable.
dnl
dnl This is an auxiliary macro for the implementation of _KYUA_FLAG.
dnl
dnl \param 1 The shell variable containing the compiler name.  Used for
dnl     reporting purposes only.  C or CXX.
dnl \param 2 The shell variable containing the flags for the compiler.
dnl     CFLAGS or CXXFLAGS.
dnl \param 3 The name of the compiler flag to check for.
dnl \param 4 The shell variable to set with the result of the test.  Will
dnl     be set to 'yes' if the flag is valid, 'no' otherwise.
dnl \param 5 Additional, optional flags to pass to the C compiler while
dnl     looking for the flag in $3.  We use this here to pass -Werror to the
dnl     flag checks (unless we are checking for -Werror already).
AC_DEFUN([_KYUA_FLAG_AUX], [
    if test x"${$4-unset}" = xunset; then
        AC_MSG_CHECKING(whether ${$1} supports $3)
        saved_flags="${$2}"
        $4=no
        $2="${$2} $5 $3"
        AC_LINK_IFELSE([AC_LANG_PROGRAM([], [return 0;])],
                       AC_MSG_RESULT(yes)
                       $4=yes,
                       AC_MSG_RESULT(no))
        $2="${saved_flags}"
    fi
])


dnl Checks for a compiler flag and appends it to a result variable.
dnl
dnl \param 1 The shell variable containing the compiler name.  Used for
dnl     reporting purposes only.  CC or CXX.
dnl \param 2 The shell variable containing the flags for the compiler.
dnl     CFLAGS or CXXFLAGS.
dnl \param 3 The name of the compiler flag to check for.
dnl \param 4 The shell variable to which to append $3 if the flag is valid.
AC_DEFUN([_KYUA_FLAG], [
    _KYUA_FLAG_AUX([$1], [$2], [-Werror], [kyua_$1_has_werror])
    if test "$3" = "-Werror"; then
        found=${kyua_$1_has_werror}
    else
        found=unset
        if test ${kyua_$1_has_werror} = yes; then
            _KYUA_FLAG_AUX([$1], [$2], [$3], [found], [-Werror])
        else
            _KYUA_FLAG_AUX([$1], [$2], [$3], [found], [])
        fi
    fi
    if test ${found} = yes; then
        $4="${$4} $3"
    fi
])


dnl Checks for a C compiler flag and appends it to a variable.
dnl
dnl \pre The current language is C.
dnl
dnl \param 1 The name of the compiler flag to check for.
dnl \param 2 The shell variable to which to append $1 if the flag is valid.
AC_DEFUN([KYUA_CC_FLAG], [
    AC_LANG_ASSERT([C])
    _KYUA_FLAG([CC], [CFLAGS], [$1], [$2])
])


dnl Checks for a C++ compiler flag and appends it to a variable.
dnl
dnl \pre The current language is C++.
dnl
dnl \param 1 The name of the compiler flag to check for.
dnl \param 2 The shell variable to which to append $1 if the flag is valid.
AC_DEFUN([KYUA_CXX_FLAG], [
    AC_LANG_ASSERT([C++])
    _KYUA_FLAG([CXX], [CXXFLAGS], [$1], [$2])
])


dnl Checks for a set of C compiler flags and appends them to CFLAGS.
dnl
dnl The checks are performed independently and only when all the checks are
dnl done, the output variable is modified.
dnl
dnl \param 1 Whitespace-separated list of C flags to check.
AC_DEFUN([KYUA_CC_FLAGS], [
    AC_LANG_PUSH([C])
    valid_cflags=
    for f in $1; do
        KYUA_CC_FLAG(${f}, valid_cflags)
    done
    if test -n "${valid_cflags}"; then
        CFLAGS="${CFLAGS} ${valid_cflags}"
    fi
    AC_LANG_POP([C])
])


dnl Checks for a set of C++ compiler flags and appends them to CXXFLAGS.
dnl
dnl The checks are performed independently and only when all the checks are
dnl done, the output variable is modified.
dnl
dnl \pre The current language is C++.
dnl
dnl \param 1 Whitespace-separated list of C flags to check.
AC_DEFUN([KYUA_CXX_FLAGS], [
    AC_LANG_PUSH([C++])
    valid_cxxflags=
    for f in $1; do
        KYUA_CXX_FLAG(${f}, valid_cxxflags)
    done
    if test -n "${valid_cxxflags}"; then
        CXXFLAGS="${CXXFLAGS} ${valid_cxxflags}"
    fi
    AC_LANG_POP([C++])
])
