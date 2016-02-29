/*-
 * Copyright (c) 2011-2012 Semihalf.
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
 *
 * $FreeBSD$
 */

#ifndef FMAN_H_
#define FMAN_H_

/**
 * FMan driver instance data.
 */
struct fman_softc {
	device_t dev;
	struct resource *mem_res;
	struct resource *irq_res;
	struct resource *err_irq_res;
	int mem_rid;
	int irq_rid;
	int err_irq_rid;

	t_Handle fm_handle;
	t_Handle muram_handle;
};


/**
 * @group QMan bus interface.
 * @{
 */
int	fman_attach(device_t dev);
int	fman_detach(device_t dev);
int	fman_suspend(device_t dev);
int	fman_resume(device_t dev);
int	fman_shutdown(device_t dev);
int	fman_read_ivar(device_t dev, device_t child, int index,
	    uintptr_t *result);
/** @} */

uint32_t	fman_get_clock(struct fman_softc *sc);
int	fman_get_handle(t_Handle *fmh);
int	fman_get_muram_handle(t_Handle *muramh);
int	fman_get_bushandle(vm_offset_t *fm_base);

#endif /* FMAN_H_ */
