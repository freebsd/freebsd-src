dnl Copyright 2011 The Kyua Authors.
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

dnl \file m4/fs.m4
dnl File system related checks.
dnl
dnl The macros in this file check for features required in the utils/fs
dnl module.  The global KYUA_FS_MODULE macro will call all checks required
dnl for the library.


dnl KYUA_FS_GETCWD_DYN
dnl
dnl Checks whether getcwd(NULL, 0) works; i.e. if getcwd(3) can dynamically
dnl allocate the output buffer to fit the whole current path.
AC_DEFUN([KYUA_FS_GETCWD_DYN], [
    AC_CACHE_CHECK(
        [whether getcwd(NULL, 0) works],
        [kyua_cv_getcwd_dyn], [
        AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <stdlib.h>
#include <unistd.h>
], [
    char *cwd = getcwd(NULL, 0);
    return (cwd != NULL) ? EXIT_SUCCESS : EXIT_FAILURE;
])],
        [kyua_cv_getcwd_dyn=yes],
        [kyua_cv_getcwd_dyn=no])
    ])
    if test "${kyua_cv_getcwd_dyn}" = yes; then
        AC_DEFINE_UNQUOTED([HAVE_GETCWD_DYN], [1],
                           [Define to 1 if getcwd(NULL, 0) works])
    fi
])


dnl KYUA_FS_LCHMOD
dnl
dnl Checks whether lchmod(3) exists and if it works.  Some systems, such as
dnl Ubuntu 10.04.1 LTS, provide a lchmod(3) stub that is not implemented yet
dnl allows programs to compile cleanly (albeit for a warning).  It would be
dnl nice to detect if lchmod(3) works at run time to prevent side-effects of
dnl this test but doing so means we will keep receiving a noisy compiler
dnl warning.
AC_DEFUN([KYUA_FS_LCHMOD], [
    AC_CACHE_CHECK(
        [for a working lchmod],
        [kyua_cv_lchmod_works], [
        AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
], [
    int fd = open("conftest.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("creation of conftest.txt failed");
        return EXIT_FAILURE;
    }

    return lchmod("conftest.txt", 0640) != -1 ?  EXIT_SUCCESS : EXIT_FAILURE;
])],
        [kyua_cv_lchmod_works=yes],
        [kyua_cv_lchmod_works=no])
    ])
    rm -f conftest.txt
    if test "${kyua_cv_lchmod_works}" = yes; then
        AC_DEFINE_UNQUOTED([HAVE_WORKING_LCHMOD], [1],
                           [Define to 1 if your lchmod works])
    fi
])


dnl KYUA_FS_UNMOUNT
dnl
dnl Detect the correct method to unmount a file system.
AC_DEFUN([KYUA_FS_UNMOUNT], [
    AC_CHECK_FUNCS([unmount], [have_unmount2=yes], [have_unmount2=no])
    if test "${have_unmount2}" = no; then
        have_umount8=yes
        AC_PATH_PROG([UMOUNT], [umount], [have_umount8=no])
        if test "${have_umount8}" = yes; then
            AC_DEFINE_UNQUOTED([UMOUNT], ["${UMOUNT}"],
                               [Set to the path of umount(8)])
        else
            AC_MSG_ERROR([Don't know how to unmount a file system])
        fi
    fi
])


dnl KYUA_FS_MODULE
dnl
dnl Performs all checks needed by the utils/fs library.
AC_DEFUN([KYUA_FS_MODULE], [
    AC_CHECK_HEADERS([sys/mount.h sys/statvfs.h sys/vfs.h])
    AC_CHECK_FUNCS([statfs statvfs])
    KYUA_FS_GETCWD_DYN
    KYUA_FS_LCHMOD
    KYUA_FS_UNMOUNT
])
