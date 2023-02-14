/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1994-1995 Søren Schmidt
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sdt.h>

#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>
#include <compat/linux/linux_dtrace.h>
#include <compat/linux/linux_util.h>

/* DTrace init */
LIN_SDT_PROVIDER_DECLARE(LINUX_DTRACE);

/*
 * Before adding new stubs to this file, please check if a stub can be added to
 * the machine-independent code in sys/compat/linux/linux_dummy.c (or
 * sys/x86/linux/linux_dummy_x86.c).
 */

UNIMPLEMENTED(break);
UNIMPLEMENTED(ftime);
UNIMPLEMENTED(gtty);
UNIMPLEMENTED(stty);
UNIMPLEMENTED(lock);
UNIMPLEMENTED(mpx);
UNIMPLEMENTED(prof);
UNIMPLEMENTED(profil);
UNIMPLEMENTED(ulimit);

DUMMY(bdflush);
DUMMY(fstat);
DUMMY(olduname);
DUMMY(stime);
DUMMY(uname);
DUMMY(vm86);
DUMMY(vm86old);
/* Linux 4.11: */
DUMMY(arch_prctl);
/* Linux 5.0: */
DUMMY(clock_adjtime64);
DUMMY(io_pgetevents_time64);
DUMMY(mq_timedsend_time64);
DUMMY(mq_timedreceive_time64);
