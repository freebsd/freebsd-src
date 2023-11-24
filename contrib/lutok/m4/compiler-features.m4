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

dnl
dnl KYUA_REQUIRE_CXX
dnl
dnl Ensures the C++ compiler detected by AC_PROG_CXX is present and works.
dnl The compiler check should be performed here, but it seems like Autoconf
dnl does not allow it.
dnl
AC_DEFUN([KYUA_REQUIRE_CXX], [
    AC_CACHE_CHECK([whether the C++ compiler works],
                   [atf_cv_prog_cxx_works],
                   [AC_LANG_PUSH([C++])
                    AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
                                   [atf_cv_prog_cxx_works=yes],
                                   [atf_cv_prog_cxx_works=no])
                    AC_LANG_POP([C++])])
    if test "${atf_cv_prog_cxx_works}" = no; then
        AC_MSG_ERROR([C++ compiler cannot create executables])
    fi
])

dnl
dnl KYUA_ATTRIBUTE_NORETURN
dnl
dnl Checks if the current compiler has a way to mark functions that do not
dnl return and defines ATTRIBUTE_NORETURN to the appropriate string.
dnl
AC_DEFUN([KYUA_ATTRIBUTE_NORETURN], [
    dnl This check is overly simple and should be fixed.  For example,
    dnl Sun's cc does support the noreturn attribute but CC (the C++
    dnl compiler) does not.  And in that case, CC just raises a warning
    dnl during compilation, not an error.
    AC_MSG_CHECKING(whether __attribute__((noreturn)) is supported)
    AC_RUN_IFELSE([AC_LANG_PROGRAM([], [
#if ((__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2)
    return 0;
#else
    return 1;
#endif
        ])],
        [AC_MSG_RESULT(yes)
         value="__attribute__((noreturn))"],
        [AC_MSG_RESULT(no)
         value=""]
    )
    AC_SUBST([ATTRIBUTE_NORETURN], [${value}])
])


dnl
dnl KYUA_ATTRIBUTE_UNUSED
dnl
dnl Checks if the current compiler has a way to mark parameters as unused
dnl so that the -Wunused-parameter warning can be avoided.
dnl
AC_DEFUN([KYUA_ATTRIBUTE_UNUSED], [
    AC_MSG_CHECKING(whether __attribute__((__unused__)) is supported)
    AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([
static void
function(int a __attribute__((__unused__)))
{
}], [
    function(3);
    return 0;
])],
        [AC_MSG_RESULT(yes)
         value="__attribute__((__unused__))"],
        [AC_MSG_RESULT(no)
         value=""]
    )
    AC_SUBST([ATTRIBUTE_UNUSED], [${value}])
])
