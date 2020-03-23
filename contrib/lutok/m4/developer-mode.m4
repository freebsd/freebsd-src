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

dnl \file developer-mode.m4
dnl
dnl "Developer mode" is a mode in which the build system reports any
dnl build-time warnings as fatal errors.  This helps in minimizing the
dnl amount of trivial coding problems introduced in the code.
dnl Unfortunately, this is not bullet-proof due to the wide variety of
dnl compilers available and their different warning diagnostics.
dnl
dnl When developer mode support is added to a package, the compilation will
dnl gain a bunch of extra warning diagnostics.  These will NOT be enforced
dnl unless developer mode is enabled.
dnl
dnl Developer mode is enabled when the user requests it through the
dnl configure command line, or when building from the repository.  The
dnl latter is to minimize the risk of committing new code with warnings
dnl into the tree.


dnl Adds "developer mode" support to the package.
dnl
dnl This macro performs the actual definition of the --enable-developer
dnl flag and implements all of its logic.  See the file-level comment for
dnl details as to what this implies.
AC_DEFUN([KYUA_DEVELOPER_MODE], [
    m4_foreach([language], [$1], [m4_set_add([languages], language)])

    AC_ARG_ENABLE(
        [developer],
        AS_HELP_STRING([--enable-developer], [enable developer features]),,
        [if test -d ${srcdir}/.git; then
             AC_MSG_NOTICE([building from HEAD; developer mode autoenabled])
             enable_developer=yes
         else
             enable_developer=no
         fi])

    #
    # The following warning flags should also be enabled but cannot be.
    # Reasons given below.
    #
    # -Wold-style-cast: Raises errors when using TIOCGWINSZ, at least under
    #                   Mac OS X.  This is due to the way _IOR is defined.
    #

    try_c_cxx_flags="-D_FORTIFY_SOURCE=2 \
                     -Wall \
                     -Wcast-qual \
                     -Wextra \
                     -Wpointer-arith \
                     -Wredundant-decls \
                     -Wreturn-type \
                     -Wshadow \
                     -Wsign-compare \
                     -Wswitch \
                     -Wwrite-strings"

    try_c_flags="-Wmissing-prototypes \
                 -Wno-traditional \
                 -Wstrict-prototypes"

    try_cxx_flags="-Wabi \
                   -Wctor-dtor-privacy \
                   -Wno-deprecated \
                   -Wno-non-template-friend \
                   -Wno-pmf-conversions \
                   -Wnon-virtual-dtor \
                   -Woverloaded-virtual \
                   -Wreorder \
                   -Wsign-promo \
                   -Wsynth"

    if test ${enable_developer} = yes; then
        try_werror=yes
        try_c_cxx_flags="${try_c_cxx_flags} -g -Werror"
    else
        try_werror=no
        try_c_cxx_flags="${try_c_cxx_flags} -DNDEBUG"
    fi

    m4_set_contains([languages], [C],
                    [KYUA_CC_FLAGS(${try_c_cxx_flags} ${try_c_flags})])
    m4_set_contains([languages], [C++],
                    [KYUA_CXX_FLAGS(${try_c_cxx_flags} ${try_cxx_flags})])
])
