/*-
 * Copyright (c) 2014 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/cpuset.h>
#include <sys/systm.h>

#include <machine/vmm.h>

#include "vatpic.h"
#include "vatpit.h"
#include "vmm_ioport.h"

#define	MAX_IOPORTS		1280

ioport_handler_func_t ioport_handler[MAX_IOPORTS] = {
	[TIMER_MODE] = vatpit_handler,
	[TIMER_CNTR0] = vatpit_handler,
	[TIMER_CNTR1] = vatpit_handler,
	[TIMER_CNTR2] = vatpit_handler,
	[NMISC_PORT] = vatpit_nmisc_handler,
	[IO_ICU1] = vatpic_master_handler,
	[IO_ICU1 + ICU_IMR_OFFSET] = vatpic_master_handler,
	[IO_ICU2] = vatpic_slave_handler,
	[IO_ICU2 + ICU_IMR_OFFSET] = vatpic_slave_handler,
	[IO_ELCR1] = vatpic_elc_handler,
	[IO_ELCR2] = vatpic_elc_handler,
};

int
emulate_ioport(struct vm *vm, int vcpuid, struct vm_exit *vmexit)
{
	ioport_handler_func_t handler;

	if (vmexit->u.inout.port >= MAX_IOPORTS)
		return (-1);

	handler = ioport_handler[vmexit->u.inout.port];
	if (handler == NULL)
		return (-1);

	return ((*handler)(vm, vcpuid, vmexit));
}
