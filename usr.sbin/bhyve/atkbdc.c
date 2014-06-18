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

#include <sys/types.h>

#include <machine/vmm.h>

#include <stdio.h>

#include "inout.h"
#include "pci_lpc.h"

#define	KBD_DATA_PORT		0x60

#define	KBD_STS_CTL_PORT	0x64
#define	 KBD_SYS_FLAG		0x4

#define	KBDC_RESET		0xfe

static int
atkbdc_data_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
    uint32_t *eax, void *arg)
{
	if (bytes != 1)
		return (INOUT_ERROR);

	*eax = 0;

	return (INOUT_OK);
}

static int
atkbdc_sts_ctl_handler(struct vmctx *ctx, int vcpu, int in, int port,
    int bytes, uint32_t *eax, void *arg)
{
	int retval;

	if (bytes != 1)
		return (INOUT_ERROR);

	retval = INOUT_OK;
	if (in) {
		*eax = KBD_SYS_FLAG;	/* system passed POST */
	} else {
		switch (*eax) {
		case KBDC_RESET:	/* Pulse "reset" line. */
			retval = INOUT_RESET;
			break;
		}
	}

	return (retval);
}

INOUT_PORT(atkdbc, KBD_DATA_PORT, IOPORT_F_INOUT, atkbdc_data_handler);
SYSRES_IO(KBD_DATA_PORT, 1);
INOUT_PORT(atkbdc, KBD_STS_CTL_PORT,  IOPORT_F_INOUT,
    atkbdc_sts_ctl_handler);
SYSRES_IO(KBD_STS_CTL_PORT, 1);
