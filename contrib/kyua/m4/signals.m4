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
dnl KYUA_LAST_SIGNO
dnl
dnl Detect the last valid signal number.
dnl
AC_DEFUN([KYUA_LAST_SIGNO], [
    AC_CACHE_CHECK(
        [for the last valid signal],
        [kyua_cv_signals_lastno], [
        AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>], [
    static const int max_signals = 256;
    int i;
    FILE *f;

    i = 0;
    while (i < max_signals) {
        i++;
        if (i != SIGKILL && i != SIGSTOP) {
            struct sigaction sa;
            int ret;

            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;

            ret = sigaction(i, &sa, NULL);
            if (ret == -1) {
                warn("sigaction(%d) failed", i);
                if (errno == EINVAL) {
                    i--;
                    break;
                } else
                    err(EXIT_FAILURE, "sigaction failed");
            }
        }
    }
    if (i == max_signals)
        errx(EXIT_FAILURE, "too many signals");

    f = fopen("conftest.cnt", "w");
    if (f == NULL)
        err(EXIT_FAILURE, "failed to open file");

    fprintf(f, "%d\n", i);
    fclose(f);

    return EXIT_SUCCESS;
])],
        [if test ! -f conftest.cnt; then
             kyua_cv_signals_lastno=15
         else
             kyua_cv_signals_lastno=$(cat conftest.cnt)
             rm -f conftest.cnt
         fi],
        [kyua_cv_signals_lastno=15])
    ])
    AC_DEFINE_UNQUOTED([LAST_SIGNO], [${kyua_cv_signals_lastno}],
                       [Define to the last valid signal number])
])
