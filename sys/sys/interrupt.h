/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
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
 * $Id: interrupt.h,v 1.4 1997/06/02 10:46:20 dfr Exp $
 */

/* XXX currently dev_instance must be set to the ISA device_id or -1 for PCI */
#define	INTR_FAST		0x00000001 /* fast interrupt handler */
#define INTR_EXCL		0x00010000 /* excl. intr, default is shared */

struct intrec *intr_create(void *dev_instance, int irq, inthand2_t handler,
			   void *arg, intrmask_t *maskptr, int flags);

int intr_destroy(struct intrec *idesc);

int intr_connect(struct intrec *idesc);
int intr_disconnect(struct intrec *idesc);
int intr_registered(int irq);

/* XXX emulate old interface for now ... */
int register_intr __P((int intr, int device_id, u_int flags,
		       inthand2_t *handler, u_int *maskptr, int unit));
int unregister_intr(int intr, inthand2_t handler);
