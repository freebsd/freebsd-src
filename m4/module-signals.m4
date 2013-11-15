dnl
dnl Automated Testing Framework (atf)
dnl
dnl Copyright (c) 2008 The NetBSD Foundation, Inc.
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl 1. Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl 2. Redistributions in binary form must reproduce the above copyright
dnl    notice, this list of conditions and the following disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
dnl CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
dnl INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
dnl IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
dnl DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
dnl DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
dnl GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
dnl INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
dnl IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
dnl OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
dnl IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
dnl

AC_DEFUN([ATF_MODULE_SIGNALS], [
    AC_CACHE_CHECK(
        [for the last valid signal],
        [kyua_cv_signal_lastno], [
        AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>], [
    int i;
    FILE *f;

    i = 0;
    while (i < 1024) {
        i++;
        if (i != SIGKILL && i != SIGSTOP) {
            struct sigaction sa;
            int ret;

            sa.sa_handler = SIG_IGN;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;

            ret = sigaction(i, &sa, NULL);
            if (ret == -1) {
                if (errno == EINVAL) {
                    i--;
                    break;
                } else
                    err(EXIT_FAILURE, "sigaction failed");
            }
        }
    }
    if (i == 100)
        errx(EXIT_FAILURE, "too much signals");

    f = fopen("conftest.cnt", "w");
    if (f == NULL)
        err(EXIT_FAILURE, "failed to open file");

    fprintf(f, "%d\n", i);
    fclose(f);

    return EXIT_SUCCESS;
])],
        [if test ! -f conftest.cnt; then
             kyua_cv_signal_lastno=15
         else
             kyua_cv_signal_lastno=$(cat conftest.cnt)
             rm -f conftest.cnt
         fi],
        [kyua_cv_signal_lastno=15])
    ])
    AC_DEFINE_UNQUOTED([LAST_SIGNO], [${kyua_cv_signal_lastno}],
                       [Define to the last valid signal number])
])
