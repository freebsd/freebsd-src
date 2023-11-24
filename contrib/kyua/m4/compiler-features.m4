dnl Copyright 2010 The Kyua Authors.
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
    AC_CACHE_CHECK(
        [whether __attribute__((noreturn)) is supported],
        [kyua_cv_attribute_noreturn], [
        AC_RUN_IFELSE([AC_LANG_PROGRAM([], [
#if ((__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2)
    return 0;
#else
    return 1;
#endif
            ])],
            [kyua_cv_attribute_noreturn=yes],
            [kyua_cv_attribute_noreturn=no])
    ])
    if test "${kyua_cv_attribute_noreturn}" = yes; then
        attribute_value="__attribute__((noreturn))"
    else
        attribute_value=""
    fi
    AC_SUBST([ATTRIBUTE_NORETURN], [${attribute_value}])
])


dnl
dnl KYUA_ATTRIBUTE_PURE
dnl
dnl Checks if the current compiler has a way to mark functions as pure.
dnl
AC_DEFUN([KYUA_ATTRIBUTE_PURE], [
    AC_CACHE_CHECK(
        [whether __attribute__((__pure__)) is supported],
        [kyua_cv_attribute_pure], [
        AC_COMPILE_IFELSE(
            [AC_LANG_PROGRAM([
static int function(int, int) __attribute__((__pure__));

static int
function(int a, int b)
{
    return a + b;
}], [
    return function(3, 4);
])],
            [kyua_cv_attribute_pure=yes],
            [kyua_cv_attribute_pure=no])
    ])
    if test "${kyua_cv_attribute_pure}" = yes; then
        attribute_value="__attribute__((__pure__))"
    else
        attribute_value=""
    fi
    AC_SUBST([ATTRIBUTE_PURE], [${attribute_value}])
])


dnl
dnl KYUA_ATTRIBUTE_UNUSED
dnl
dnl Checks if the current compiler has a way to mark parameters as unused
dnl so that the -Wunused-parameter warning can be avoided.
dnl
AC_DEFUN([KYUA_ATTRIBUTE_UNUSED], [
    AC_CACHE_CHECK(
        [whether __attribute__((__unused__)) is supported],
        [kyua_cv_attribute_unused], [
        AC_COMPILE_IFELSE(
            [AC_LANG_PROGRAM([
static void
function(int a __attribute__((__unused__)))
{
}], [
    function(3);
    return 0;
])],
            [kyua_cv_attribute_unused=yes],
            [kyua_cv_attribute_unused=no])
    ])
    if test "${kyua_cv_attribute_unused}" = yes; then
        attribute_value="__attribute__((__unused__))"
    else
        attribute_value=""
    fi
    AC_SUBST([ATTRIBUTE_UNUSED], [${attribute_value}])
])
