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
dnl KYUA_DOXYGEN
dnl
dnl Adds a --with-doxygen flag to the configure script and, when Doxygen support
dnl is requested by the user, sets DOXYGEN to the path of the Doxygen binary and
dnl enables the WITH_DOXYGEN Automake conditional.
dnl
AC_DEFUN([KYUA_DOXYGEN], [
    AC_ARG_WITH([doxygen],
                AS_HELP_STRING([--with-doxygen],
                               [build documentation for internal APIs]),
                [],
                [with_doxygen=auto])

    if test "${with_doxygen}" = yes; then
        AC_PATH_PROG([DOXYGEN], [doxygen], [])
        if test -z "${DOXYGEN}"; then
            AC_MSG_ERROR([Doxygen explicitly requested but not found])
        fi
    elif test "${with_doxygen}" = auto; then
        AC_PATH_PROG([DOXYGEN], [doxygen], [])
    elif test "${with_doxygen}" = no; then
        DOXYGEN=
    else
        AC_MSG_CHECKING([for doxygen])
        DOXYGEN="${with_doxygen}"
        AC_MSG_RESULT([${DOXYGEN}])
        if test ! -x "${DOXYGEN}"; then
            AC_MSG_ERROR([Doxygen binary ${DOXYGEN} is not executable])
        fi
    fi
    AM_CONDITIONAL([WITH_DOXYGEN], [test -n "${DOXYGEN}"])
    AC_SUBST([DOXYGEN])
])
