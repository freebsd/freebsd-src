/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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
 * Intel Stratix 10 Service Layer
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/vmem.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/fdt/simplebus.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm64/intel/intel-smc.h>
#include <arm64/intel/stratix10-svc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

struct s10_svc_softc {
	device_t		dev;
	vmem_t			*vmem;
	intel_smc_callfn_t	callfn;
};

static int
s10_data_claim(struct s10_svc_softc *sc)
{
	struct arm_smccc_res res;
	register_t a0, a1, a2;
	int ret;

	ret = 0;

	while (1) {
		a0 = INTEL_SIP_SMC_FPGA_CONFIG_COMPLETED_WRITE;
		a1 = 0;
		a2 = 0;

		ret = sc->callfn(a0, a1, a2, 0, 0, 0, 0, 0, &res);
		if (ret == INTEL_SIP_SMC_FPGA_CONFIG_STATUS_BUSY)
			continue;

		break;
	}

	return (ret);
}

int
s10_svc_send(device_t dev, struct s10_svc_msg *msg)
{
	struct s10_svc_softc *sc;
	struct arm_smccc_res res;
	register_t a0, a1, a2;
	int ret;

	sc = device_get_softc(dev);

	a0 = 0;
	a1 = 0;
	a2 = 0;

	switch (msg->command) {
	case COMMAND_RECONFIG:
		a0 = INTEL_SIP_SMC_FPGA_CONFIG_START;
		a1 = msg->flags;
		break;
	case COMMAND_RECONFIG_DATA_SUBMIT:
		a0 = INTEL_SIP_SMC_FPGA_CONFIG_WRITE;
		a1 = (uint64_t)msg->payload;
		a2 = (uint64_t)msg->payload_length;
		break;
	case COMMAND_RECONFIG_DATA_CLAIM:
		ret = s10_data_claim(sc);
		return (ret);
	default:
		return (-1);
	}

	ret = sc->callfn(a0, a1, a2, 0, 0, 0, 0, 0, &res);

	return (ret);
}

int
s10_svc_allocate_memory(device_t dev, struct s10_svc_mem *mem, int size)
{
	struct s10_svc_softc *sc;

	sc = device_get_softc(dev);

	if (size <= 0)
		return (EINVAL);

	if (vmem_alloc(sc->vmem, size,
	    M_FIRSTFIT | M_NOWAIT, &mem->paddr)) {
		device_printf(dev, "Can't allocate memory\n");
		return (ENOMEM);
	}

	mem->size = size;
	mem->fill = 0;
	mem->vaddr = (vm_offset_t)pmap_mapdev(mem->paddr, mem->size);

	return (0);
}

void
s10_svc_free_memory(device_t dev, struct s10_svc_mem *mem)
{
	struct s10_svc_softc *sc;

	sc = device_get_softc(dev);

	vmem_free(sc->vmem, mem->paddr, mem->size);
}

static int
s10_get_memory(struct s10_svc_softc *sc)
{
	struct arm_smccc_res res;
	vmem_addr_t addr;
	vmem_size_t size;
	vmem_t *vmem;

	sc->callfn(INTEL_SIP_SMC_FPGA_CONFIG_GET_MEM,
	    0, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 != INTEL_SIP_SMC_STATUS_OK)
		return (ENXIO);

	vmem = vmem_create("stratix10 vmem", 0, 0, PAGE_SIZE,
	    PAGE_SIZE, M_BESTFIT | M_WAITOK);
	if (vmem == NULL)
		return (ENXIO);

	addr = res.a1;
	size = res.a2;

	device_printf(sc->dev, "Shared memory address 0x%lx size 0x%lx\n",
	    addr, size);

	vmem_add(vmem, addr, size, 0);

	sc->vmem = vmem;

	return (0);
}

static intel_smc_callfn_t
s10_svc_get_callfn(struct s10_svc_softc *sc, phandle_t node)
{
	char method[16];

	if ((OF_getprop(node, "method", method, sizeof(method))) > 0) {
		if (strcmp(method, "hvc") == 0)
			return (arm_smccc_hvc);
		else if (strcmp(method, "smc") == 0)
			return (arm_smccc_smc);
		else
			device_printf(sc->dev,
			    "Invalid method \"%s\"\n", method);
	} else
		device_printf(sc->dev, "SMC method not provided\n");

	return (NULL);
}

static int
s10_svc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "intel,stratix10-svc"))
		return (ENXIO);

	device_set_desc(dev, "Stratix 10 SVC");

	return (BUS_PROBE_DEFAULT);
}

static int
s10_svc_attach(device_t dev)
{
	struct s10_svc_softc *sc;
	phandle_t node;

	node = ofw_bus_get_node(dev);

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (device_get_unit(dev) != 0)
		return (ENXIO);

	sc->callfn = s10_svc_get_callfn(sc, node);
	if (sc->callfn == NULL)
		return (ENXIO);

	if (s10_get_memory(sc) != 0)
		return (ENXIO);

	return (0);
}

static device_method_t s10_svc_methods[] = {
	DEVMETHOD(device_probe,		s10_svc_probe),
	DEVMETHOD(device_attach,	s10_svc_attach),
	{ 0, 0 }
};

static driver_t s10_svc_driver = {
	"s10_svc",
	s10_svc_methods,
	sizeof(struct s10_svc_softc),
};

EARLY_DRIVER_MODULE(s10_svc, simplebus, s10_svc_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
