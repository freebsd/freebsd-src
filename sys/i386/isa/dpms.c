/*-
 * Copyright (c) 2008 Yahoo!, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/*
 * Copyright (c) 2004 Benjamin Close <Benjamin.Close@clearchain.com>
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
 */

/*
 * Support for managing the display via DPMS for suspend/resume.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i386/isa/dpms.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/module.h>

#include <machine/vm86.h>

/*
 * VESA DPMS States 
 */
#define DPMS_ON		0x00
#define DPMS_STANDBY	0x01
#define DPMS_SUSPEND	0x02
#define DPMS_OFF	0x04
#define DPMS_REDUCEDON	0x08

#define	VBE_DPMS_FUNCTION	0x4F10
#define	VBE_DPMS_GET_SUPPORTED_STATES 0x00
#define	VBE_DPMS_GET_STATE	0x02
#define	VBE_DPMS_SET_STATE	0x01
#define VBE_MAJORVERSION_MASK	0x0F
#define VBE_MINORVERSION_MASK	0xF0

struct dpms_softc {
	int	dpms_supported_states;
	int	dpms_initial_state;
};

static int	dpms_attach(device_t);
static int	dpms_detach(device_t);
static int	dpms_get_supported_states(int *);
static int	dpms_get_current_state(int *);
static void	dpms_identify(driver_t *, device_t);
static int	dpms_probe(device_t);
static int	dpms_resume(device_t);
static int	dpms_set_state(int);
static int	dpms_suspend(device_t);

static device_method_t dpms_methods[] = {
	DEVMETHOD(device_identify,	dpms_identify),
	DEVMETHOD(device_probe,		dpms_probe),
	DEVMETHOD(device_attach,	dpms_attach),
	DEVMETHOD(device_detach,	dpms_detach),
	DEVMETHOD(device_suspend,	dpms_suspend),
	DEVMETHOD(device_resume,	dpms_resume),
	{ 0, 0 }
};

static driver_t dpms_driver = {
	"dpms",
	dpms_methods,
	sizeof(struct dpms_softc),
};

static devclass_t dpms_devclass;

DRIVER_MODULE(dpms, vgapci, dpms_driver, dpms_devclass, NULL, NULL);

static void
dpms_identify(driver_t *driver, device_t parent)
{

	/*
	 * XXX: The DPMS VBE only allows for manipulating a single
	 * monitor, but we don't know which one.  Just attach to the
	 * first vgapci(4) device we encounter and hope it is the
	 * right one.
	 */
	if (devclass_get_device(dpms_devclass, 0) == NULL)
		device_add_child(parent, "dpms", 0);
}

static int
dpms_probe(device_t dev)
{
	int error, states;

	error = dpms_get_supported_states(&states);
	if (error)
		return (error);
	device_set_desc(dev, "DPMS suspend/resume");
	device_quiet(dev);
	return (BUS_PROBE_DEFAULT);
}

static int
dpms_attach(device_t dev)
{
	struct dpms_softc *sc;
	int error;

	sc = device_get_softc(dev);
	error = dpms_get_supported_states(&sc->dpms_supported_states);
	if (error)
		return (error);
	error = dpms_get_current_state(&sc->dpms_initial_state);
	return (error);
}

static int
dpms_detach(device_t dev)
{

	return (0);
}

static int
dpms_suspend(device_t dev)
{

	dpms_set_state(DPMS_OFF);
	return (0);
}

static int
dpms_resume(device_t dev)
{
	struct dpms_softc *sc;

	sc = device_get_softc(dev);
	dpms_set_state(sc->dpms_initial_state);
	return (0);
}

static int
dpms_call_bios(int subfunction, int *bh)
{
	struct vm86frame vmf;
	int error;

	bzero(&vmf, sizeof(vmf));
	vmf.vmf_ax = VBE_DPMS_FUNCTION;
	vmf.vmf_bl = subfunction;
	vmf.vmf_bh = *bh;
	vmf.vmf_es = 0;
	vmf.vmf_di = 0;
	error = vm86_intcall(0x10, &vmf);
	if (error == 0 && (vmf.vmf_eax & 0xffff) != 0x004f)
		error = ENXIO;
	if (error == 0)
		*bh = vmf.vmf_bh;
	return (error);
}

static int
dpms_get_supported_states(int *states)
{

	*states = 0;
	return (dpms_call_bios(VBE_DPMS_GET_SUPPORTED_STATES, states));
}

static int
dpms_get_current_state(int *state)
{

	*state = 0;
	return (dpms_call_bios(VBE_DPMS_GET_STATE, state));
}

static int
dpms_set_state(int state)
{

	return (dpms_call_bios(VBE_DPMS_SET_STATE, &state));
}
