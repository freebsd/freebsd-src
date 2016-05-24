/*-
 * Copyright (c) 2016 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#ifndef _VMBUS_VAR_H_
#define _VMBUS_VAR_H_

#include <sys/param.h>

struct vmbus_pcpu_data {
	int		event_flag_cnt;	/* # of event flags */
	u_long		*intr_cnt;
} __aligned(CACHE_LINE_SIZE);

struct vmbus_softc {
	void			(*vmbus_event_proc)(struct vmbus_softc *, int);
	struct vmbus_pcpu_data	vmbus_pcpu[MAXCPU];
	device_t		vmbus_dev;
	int			vmbus_idtvec;
};

extern struct vmbus_softc	*vmbus_sc;

static __inline struct vmbus_softc *
vmbus_get_softc(void)
{
	return vmbus_sc;
}

static __inline device_t
vmbus_get_device(void)
{
	return vmbus_sc->vmbus_dev;
}

#define VMBUS_SC_PCPU_GET(sc, field, cpu)	(sc)->vmbus_pcpu[(cpu)].field
#define VMBUS_SC_PCPU_PTR(sc, field, cpu)	&(sc)->vmbus_pcpu[(cpu)].field
#define VMBUS_PCPU_GET(field, cpu)		\
	VMBUS_SC_PCPU_GET(vmbus_get_softc(), field, (cpu))
#define VMBUS_PCPU_PTR(field, cpu)		\
	VMBUS_SC_PCPU_PTR(vmbus_get_softc(), field, (cpu))

void	vmbus_on_channel_open(const struct hv_vmbus_channel *);
void	vmbus_event_proc(struct vmbus_softc *, int);
void	vmbus_event_proc_compat(struct vmbus_softc *, int);

#endif	/* !_VMBUS_VAR_H_ */
