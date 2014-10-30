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

dnl ATF_ARG_WITH
dnl
dnl Adds a --with-atf flag to the configure script that allows the user to
dnl enable or disable atf support.
dnl
dnl The ATF_CHECK_{C,CXX,SH} macros honor the flag defined herein if
dnl instantiated.  If not instantiated, they will request the presence of
dnl the libraries unconditionally.
dnl
dnl Defines the WITH_ATF Automake conditional if ATF has been found by any
dnl of the ATF_CHECK_{C,CXX,SH} macros.
AC_DEFUN([ATF_ARG_WITH], [
    m4_define([atf_arg_with_called], [yes])

    m4_divert_text([DEFAULTS], [with_atf=auto])
    AC_ARG_WITH([atf],
                [AS_HELP_STRING([--with-atf=<yes|no|auto>],
                                [build atf-based test programs])],
                [with_atf=${withval}], [with_atf=auto])

    m4_divert_text([DEFAULTS], [
        found_atf_c=no
        found_atf_cxx=no
        found_atf_sh=no
    ])
    AM_CONDITIONAL([WITH_ATF], [test x"${found_atf_c}" = x"yes" -o \
                                     x"${found_atf_cxx}" = x"yes" -o \
                                     x"${found_atf_sh}" = x"yes"])
])

dnl _ATF_CHECK_ARG_WITH(check, error_message)
dnl
dnl Internal macro to execute a check conditional on the --with-atf flag
dnl and handle the result accordingly.
dnl
dnl 'check' specifies the piece of code to be run to detect the feature.
dnl This code must set the 'found' shell variable to yes or no depending
dnl on the raw result of the check.
AC_DEFUN([_ATF_CHECK_ARG_WITH], [
    m4_ifdef([atf_arg_with_called], [
        m4_fatal([ATF_ARG_WITH must be called after the ATF_CHECK_* checks])
    ])

    m4_divert_text([DEFAULTS], [with_atf=yes])

    if test x"${with_atf}" = x"no"; then
        _found=no
    else
        $1
        if test x"${with_atf}" = x"auto"; then
            _found="${found}"
        else
            if test x"${found}" = x"yes"; then
                _found=yes
            else
                AC_MSG_ERROR([$2])
            fi
        fi
    fi
])
