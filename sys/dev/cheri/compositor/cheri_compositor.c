/*-
 * Copyright (c) 2013 Philip Withnall
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/dev/cheri/compositor/cheri_compositor.c 245380 2013-05-08 20:56:00Z pwithnall $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/consio.h>				/* struct vt_mode */
#include <sys/fbio.h>				/* video_adapter_t */
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/cheri/compositor/cheri_compositor_internal.h>

/*
 * Device driver for the CHERI compositor peripheral. It has one sub-driver
 * ('cfb') which exposes compositor memory, and can be mapped by any graphical
 * program without needing special privileges. Various management ioctl()s are
 * implemented on this device (by code in cheri_compositor_reg.c), some of which
 * _do_ require privileges.
 *
 * The CFB driver is designed to be mmap()ped by a graphical program, but does
 * not _require_ capabilities to access it.
 *
 * See the documentation at the top of cheri_compositor.h for a complete overview
 * of the design.
 */

devclass_t	cheri_compositor_devclass;

int
cheri_compositor_attach(struct cheri_compositor_softc *sc)
{
	int error;

	error = cheri_compositor_cfb_attach(sc);
	if (error)
		goto error;

	CHERI_COMPOSITOR_LOCK_INIT(sc);

	return (0);

error:
	cheri_compositor_cfb_detach(sc);

	return (error);
}

void
cheri_compositor_detach(struct cheri_compositor_softc *sc)
{
	CHERI_COMPOSITOR_LOCK_DESTROY(sc);

	cheri_compositor_cfb_detach(sc);
}
