/*-
 * Copyright 2003 Alexander Kabaev.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/kse.h>
#include <signal.h>
#include <string.h>

#include "thr_private.h"

__strong_reference(clock_gettime, _thr_clock_gettime);
__strong_reference(kse_exit, _thr_kse_exit);
__strong_reference(kse_wakeup, _thr_kse_wakeup);
__strong_reference(kse_create, _thr_kse_create);
__strong_reference(kse_thr_interrupt, _thr_kse_thr_interrupt);
__strong_reference(kse_release, _thr_kse_release);

__strong_reference(sigaction, _thr_sigaction);
__strong_reference(sigprocmask, _thr_sigprocmask);
__strong_reference(sigemptyset, _thr_sigemptyset);
__strong_reference(sigaddset, _thr_sigaddset);
__strong_reference(sigfillset, _thr_sigfillset);
__strong_reference(sigismember, _thr_sigismember);
__strong_reference(sigdelset, _thr_sigdelset);

__strong_reference(memset, _thr_memset);
__strong_reference(memcpy, _thr_memcpy);
__strong_reference(strcpy, _thr_strcpy);
__strong_reference(strlen, _thr_strlen);
__strong_reference(bzero, _thr_bzero);

__strong_reference(__sys_write, _thr__sys_write);
