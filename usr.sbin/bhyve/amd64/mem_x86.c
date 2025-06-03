/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/tree.h>
#include <machine/vmm.h>

#include <stdio.h>

#include "debug.h"
#include "mem.h"

static int
no_mem_handler(struct vcpu *vcpu __unused, int dir, uint64_t addr __unused,
    int size, uint64_t *val, void *arg1 __unused, long arg2 __unused)
{
	if (dir == MEM_F_READ) {
		switch (size) {
		case 1:
			*val = 0xff;
			break;
		case 2:
			*val = 0xffff;
			break;
		case 4:
			*val = 0xffffffff;
			break;
		case 8:
			*val = 0xffffffffffffffff;
			break;
		}
	}
	return (0);
}

static struct mem_range fb_entry = {
	.handler = no_mem_handler,
	.base = 0,
	.size = 0xffffffffffffffff,
};

/*
 * x86 hardware ignores writes without receiver, and returns all 1's
 * from reads without response to transaction.
 */
int
mmio_handle_non_backed_mem(struct vcpu *vcpu __unused, uint64_t paddr,
    struct mem_range **mr_paramp)
{
	*mr_paramp = &fb_entry;
	EPRINTLN("Emulating access to non-existent address to %#lx\n",
	    paddr);
	return (0);
}
