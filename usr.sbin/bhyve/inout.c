/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker_set.h>

#include <stdio.h>
#include <assert.h>

#include "inout.h"

SET_DECLARE(inout_port_set, struct inout_port);

#define	MAX_IOPORTS	(1 << 16)

static struct {
	const char	*name;
	int		flags;
	inout_func_t	handler;
	void		*arg;
} inout_handlers[MAX_IOPORTS];

static int
default_inout(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
              uint32_t *eax, void *arg)
{
        if (in) {
                switch (bytes) {
                case 4:
                        *eax = 0xffffffff;
                        break;
                case 2:
                        *eax = 0xffff;
                        break;
                case 1:
                        *eax = 0xff;
                        break;
                }
        }
        
        return (0);
}

int
emulate_inout(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
	      uint32_t *eax, int strict)
{
	int flags;
	uint32_t mask;
	inout_func_t handler;
	void *arg;

	assert(port < MAX_IOPORTS);

	handler = inout_handlers[port].handler;

	if (strict && handler == default_inout)
		return (-1);

	if (!in) {
		switch (bytes) {
		case 1:
			mask = 0xff;
			break;
		case 2:
			mask = 0xffff;
			break;
		default:
			mask = 0xffffffff;
			break;
		}
		*eax = *eax & mask;
	}

	flags = inout_handlers[port].flags;
	arg = inout_handlers[port].arg;

	if ((in && (flags & IOPORT_F_IN)) || (!in && (flags & IOPORT_F_OUT)))
		return ((*handler)(ctx, vcpu, in, port, bytes, eax, arg));
	else
		return (-1);
}

void
init_inout(void)
{
	struct inout_port **iopp, *iop;
	int i;

	/*
	 * Set up the default handler for all ports
	 */
	for (i = 0; i < MAX_IOPORTS; i++) {
		inout_handlers[i].name = "default";
		inout_handlers[i].flags = IOPORT_F_IN | IOPORT_F_OUT;
		inout_handlers[i].handler = default_inout;
		inout_handlers[i].arg = NULL;
	}

	/*
	 * Overwrite with specified handlers
	 */
	SET_FOREACH(iopp, inout_port_set) {
		iop = *iopp;
		assert(iop->port < MAX_IOPORTS);
		inout_handlers[iop->port].name = iop->name;
		inout_handlers[iop->port].flags = iop->flags;
		inout_handlers[iop->port].handler = iop->handler;
		inout_handlers[iop->port].arg = NULL;
	}
}

int
register_inout(struct inout_port *iop)
{
	assert(iop->port < MAX_IOPORTS);
	inout_handlers[iop->port].name = iop->name;
	inout_handlers[iop->port].flags = iop->flags;
	inout_handlers[iop->port].handler = iop->handler;
	inout_handlers[iop->port].arg = iop->arg;

	return (0);
}
