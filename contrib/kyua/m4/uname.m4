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
dnl KYUA_UNAME_ARCHITECTURE
dnl
dnl Checks for the current architecture name (aka processor type) and defines
dnl the KYUA_ARCHITECTURE macro to its value.
dnl
AC_DEFUN([KYUA_UNAME_ARCHITECTURE], [
    AC_MSG_CHECKING([for architecture name])
    AC_ARG_VAR([KYUA_ARCHITECTURE],
               [Name of the system architecture (aka processor type)])
    if test x"${KYUA_ARCHITECTURE-unset}" = x"unset"; then
        KYUA_ARCHITECTURE="$(uname -p)"
    fi
    AC_DEFINE_UNQUOTED([KYUA_ARCHITECTURE], "${KYUA_ARCHITECTURE}",
                       [Name of the system architecture (aka processor type)])
    AC_MSG_RESULT([${KYUA_ARCHITECTURE}])
])

dnl
dnl KYUA_UNAME_PLATFORM
dnl
dnl Checks for the current platform name (aka machine name) and defines
dnl the KYUA_PLATFORM macro to its value.
dnl
AC_DEFUN([KYUA_UNAME_PLATFORM], [
    AC_MSG_CHECKING([for platform name])
    AC_ARG_VAR([KYUA_PLATFORM],
               [Name of the system platform (aka machine name)])
    if test x"${KYUA_PLATFORM-unset}" = x"unset"; then
        KYUA_PLATFORM="$(uname -m)"
    fi
    AC_DEFINE_UNQUOTED([KYUA_PLATFORM], "${KYUA_PLATFORM}",
                       [Name of the system platform (aka machine name)])
    AC_MSG_RESULT([${KYUA_PLATFORM}])
])
