/*
 * Copyright (c) 2001 M. Warner Losh.  All Rights Reserved.
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

/*
 *	Per-slot data table.
 */
struct pcic_slot {
	int index;			/* Index register */
	int data;			/* Data register */
	int offset;			/* Offset value for index */
	char controller;		/* Device type */
	char revision;			/* Device Revision */
	struct slot *slt;		/* Back ptr to slot */
	u_char (*getb)(struct pcic_slot *, int);
	void   (*putb)(struct pcic_slot *, int, u_char);
	u_char	*regs;			/* Pointer to regs in mem */
};

struct pcic_softc 
{
	int			unit;
	struct pcic_slot	slots[PCIC_MAX_SLOTS];
};

extern devclass_t	pcic_devclass;

int pcic_probe(device_t dev);
int pcic_attach(device_t dev);
int pcic_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r);
int pcic_deactivate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r);
int pcic_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_intr_t *intr, void *arg, void **cookiep);
int pcic_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie);
int pcic_set_res_flags(device_t bus, device_t child, int restype, int rid,
    u_long value);
int pcic_get_res_flags(device_t bus, device_t child, int restype, int rid,
    u_long *value);
int pcic_get_memory_offset(device_t bus, device_t child, int rid,
    u_int32_t *offset);
int pcic_set_memory_offset(device_t bus, device_t child, int rid,
    u_int32_t offset, u_int32_t *deltap);

