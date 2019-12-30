/*-
 * Copyright (c) 2019 Justin Hibbits
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/endian.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/spr.h>
#include <machine/trap.h>
#include "opal.h"

struct opal_hmi_event {
	uint8_t 	version;
	uint8_t 	severity;
	uint8_t 	type;
	uint8_t 	disposition;
	uint8_t 	rsvd_1[4];
	uint64_t	hmer;
	uint64_t	tfmr;
	union {
		struct {
			uint8_t 	xstop_type;
			uint8_t 	rsvd_2[3];
			uint32_t	xstop_reason;
			union {
				uint32_t	pir;
				uint32_t	chip_id;
			};
		};
	};
};

#define	HMI_DISP_RECOVERED	0
#define	HMI_DISP_NOT_RECOVERED	1

static void
opal_hmi_event_handler(void *unused, struct opal_msg *msg)
{
	struct opal_hmi_event	evt;

	memcpy(&evt, &msg->params, sizeof(evt));
	printf("Hypervisor Maintenance Event received"
	    "(Severity %d, type %d, HMER: %016lx).\n",
	    evt.severity, evt.type, evt.hmer);

	if (evt.disposition == HMI_DISP_NOT_RECOVERED)
		panic("Unrecoverable hypervisor maintenance exception on CPU %d",
		    evt.pir);

	return;
}

static int
opal_hmi_handler2(struct trapframe *frame)
{
	/*
	 * Use DMAP preallocated pcpu memory to handle
	 * the phys flags pointer.
	 */
	uint64_t *flags = PCPU_PTR(aim.opal_hmi_flags);
	int err;

	*flags = 0;
	err = opal_call(OPAL_HANDLE_HMI2, DMAP_TO_PHYS((vm_offset_t)flags));

	if (*flags & OPAL_HMI_FLAGS_TOD_TB_FAIL)
		panic("TOD/TB recovery failure");

	if (err == OPAL_SUCCESS)
		return (0);

	printf("HMI handler failed!  OPAL error code: %d\n", err);

	return (-1);
}

static int
opal_hmi_handler(struct trapframe *frame)
{
	int err;

	err = opal_call(OPAL_HANDLE_HMI);

	if (err == OPAL_SUCCESS)
		return (0);

	printf("HMI handler failed!  OPAL error code: %d\n", err);

	return (-1);
}

static void
opal_setup_hmi(void *data)
{
	/* This only works for OPAL, so first make sure we have it. */
	if (opal_check() != 0)
		return;

	if (opal_call(OPAL_CHECK_TOKEN, OPAL_HANDLE_HMI2) == OPAL_TOKEN_PRESENT)
		hmi_handler = opal_hmi_handler2;
	else if (opal_call(OPAL_CHECK_TOKEN, OPAL_HANDLE_HMI) == OPAL_TOKEN_PRESENT)
		hmi_handler = opal_hmi_handler;
	else {
		printf("Warning: No OPAL HMI handler found.\n");
		return;
	}

	EVENTHANDLER_REGISTER(OPAL_HMI_EVT, opal_hmi_event_handler, NULL,
	    EVENTHANDLER_PRI_ANY);

	if (bootverbose)
		printf("Installed OPAL HMI handler.\n");
}

SYSINIT(opal_setup_hmi, SI_SUB_CPU, SI_ORDER_ANY, opal_setup_hmi, NULL);
