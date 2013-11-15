dnl
dnl Automated Testing Framework (atf)
dnl
dnl Copyright 2011 Google Inc.
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

dnl ATF_CHECK_SH([version-spec])
dnl
dnl Checks if atf-sh is present.  If version-spec is provided, ensures that
dnl the installed version of atf-sh matches the required version.  This
dnl argument must be something like '>= 0.14' and accepts any version
dnl specification supported by pkg-config.
dnl
dnl Defines and substitutes ATF_SH with the full path to the atf-sh interpreter.
AC_DEFUN([ATF_CHECK_SH], [
    spec="atf-sh[]m4_default_nblank([ $1], [])"
    _ATF_CHECK_ARG_WITH(
        [AC_MSG_CHECKING([for ${spec}])
         PKG_CHECK_EXISTS([${spec}], [found=yes], [found=no])
         if test "${found}" = yes; then
             ATF_SH="$(${PKG_CONFIG} --variable=interpreter atf-sh)"
             AC_SUBST([ATF_SH], [${ATF_SH}])
             found_atf_sh=yes
         fi
         AC_MSG_RESULT([${ATF_SH}])],
        [required ${spec} not found])
])
