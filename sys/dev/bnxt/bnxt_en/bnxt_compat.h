/*
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2026 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef BNXT_COMPAT_H
#define BNXT_COMPAT_H

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/bitstring.h>

#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/iflib.h>
#include <linux/types.h>

#if (__FreeBSD_version < 1500000)

#define if_getmtu(ifp)	(ifp)->if_mtu
#ifndef if_name
#define if_name(ifp)	(ifp)->if_xname
#endif
#define if_getsoftc(ifp) (ifp)->if_softc
#define if_getflags(ifp) (ifp)->if_flags

#endif


#if (__FreeBSD_version >= 1403000)

#define BNXT_CP_TASK_TYPE	struct task
#define BNXT_CP_TASK_FUNC(name)	void name(void *context, int pending)
#define BNXT_CP_TASK_INIT(ctx, task, func)	\
	iflib_config_task_init(ctx, task, func)
#define BNXT_CP_TASK_ENQUEUE(ctx, task)		\
	iflib_config_task_enqueue(ctx, task)

#else

#define BNXT_CP_TASK_TYPE	struct grouptask
#define BNXT_CP_TASK_FUNC(name)	void name(void *context)
#define BNXT_CP_TASK_INIT(ctx, task, func)	\
	iflib_config_gtask_init(ctx, task, func, "dflt_cp")
#define BNXT_CP_TASK_ENQUEUE(ctx, task)		\
	GROUPTASK_ENQUEUE(task)

#endif

#if (__FreeBSD_version >= 1600007)
#define KTLS_IFLIB_SUPPORT
#define KTLS_REC_SEQ_NUMBER
#endif

#endif /* BNXT_COMPAT_H */
