/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013 Dmitry Chagin <dchagin@FreeBSD.org>
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

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sdt.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <arm64/linux/linux.h>
#include <arm64/linux/linux_proto.h>
#include <compat/linux/linux_dtrace.h>
#include <compat/linux/linux_util.h>

/* DTrace init */
LIN_SDT_PROVIDER_DECLARE(LINUX_DTRACE);

/*
 * Before adding new stubs to this file, please check if a stub can be added to
 * the machine-independent code in sys/compat/linux/linux_dummy.c.
 */

UNIMPLEMENTED(get_thread_area);
UNIMPLEMENTED(set_thread_area);
UNIMPLEMENTED(uselib);

DUMMY(mq_open);
DUMMY(mq_unlink);
DUMMY(mq_timedsend);
DUMMY(mq_timedreceive);
DUMMY(mq_notify);
DUMMY(mq_getsetattr);
DUMMY(kexec_file_load);
