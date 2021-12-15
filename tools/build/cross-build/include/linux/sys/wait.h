/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2021 Jessica Clarke <jrtc27@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

/*
 * glibc's sys/wait.h and stdlib.h both define various wait-related constants,
 * depending on __USE_XOPEN(2K8) and if the other header has been included.
 * Since they each probe the other's include guard to determine that, there is
 * a window between a header defining its include guard and checking for the
 * other's within which, if the other is included for the first time, they both
 * believe the other has already defined the relevant macros etc, and so
 * neither ends up doing so. This was not previously hit, and is still not hit
 * when using glibc normally (though seems extremely fragile). However, as of
 * glibc 2.34, signal.h, included by sys/wait, includes a new bits/sigstksz,
 * which in turn includes unistd.h (when _SC_SIGSTKSZ_SOURCE is defined, which
 * is implied by _GNU_SOURCE), which we wrap and include stdlib.h from,
 * creating the exact aforementioned situation that breaks. Thus, forcefully
 * include stdlib.h first whenever sys/wait.h is as a workaround, since that
 * way round still works.
 */
#include <stdlib.h>
#include_next <sys/wait.h>
