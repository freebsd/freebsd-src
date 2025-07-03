/*-
 * Copyright (c) 2023-2025 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include <sys/hwt_record.h>

#ifndef _DEV_HWT_HWT_HOOK_H_
#define _DEV_HWT_HWT_HOOK_H_

#define	HWT_SWITCH_IN		0
#define	HWT_SWITCH_OUT		1
#define	HWT_THREAD_EXIT		2
#define	HWT_THREAD_CREATE	3
#define	HWT_THREAD_SET_NAME	4
#define	HWT_RECORD		5
#define	HWT_MMAP		6
#define	HWT_EXEC		7

#define	HWT_CALL_HOOK(td, func, arg)			\
do {							\
	if (hwt_hook != NULL)				\
		(hwt_hook)((td), (func), (arg));	\
} while (0)

#define	HWT_HOOK_INSTALLED	(hwt_hook != NULL)

extern void (*hwt_hook)(struct thread *td, int func, void *arg);

void hwt_hook_load(void);
void hwt_hook_unload(void);

#endif /* !_DEV_HWT_HWT_HOOK_H_ */
