/*-
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
 * Copyright (c) 2000 Munehiro Matsuda <haro@tk.kubota.co.jp>
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
 *	$Id: aml_region.c,v 1.10 2000/08/09 14:47:44 iwasaki Exp $
 *	$FreeBSD$
 */

/*
 * Region I/O subroutine
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/aml/aml_common.h>
#include <dev/acpi/aml/aml_region.h>
#include <dev/acpi/aml/aml_name.h>

#ifndef ACPI_NO_OSDFUNC_INLINE
#include <machine/acpica_osd.h>
#endif

/*
 * Dummy functions for aml_region_io_simple()
 */
u_int32_t
aml_region_prompt_read(struct aml_region_handle *h, u_int32_t value)
{

	return (value);
}

u_int32_t
aml_region_prompt_write(struct aml_region_handle *h, u_int32_t value)
{

	return (value);
}

int
aml_region_prompt_update_value(u_int32_t orgval, u_int32_t value,
    struct aml_region_handle *h)
{
	return (0);
}

/*
 * Primitive functions for aml_region_io_simple()
 */
int
aml_region_read_simple(struct aml_region_handle *h, vm_offset_t offset, u_int32_t *valuep)
{
	u_int32_t value;

	switch (h->regtype) {
	case AML_REGION_SYSMEM:
		/* XXX should be MI */
		switch (h->unit) {
		case 1:
			value = *(volatile u_int8_t *)(h->vaddr + offset);
			value &= 0xff;
			break;
		case 2:
			value = *(volatile u_int16_t *)(h->vaddr + offset);
			value &= 0xffff;
			break;
		case 4:
			value = *(volatile u_int32_t *)(h->vaddr + offset);
			break;
		}
		break;
	case AML_REGION_SYSIO:
		switch (h->unit) {
		case 1:
			value = OsdIn8(h->addr + offset);
			value &= 0xff;
			break;
		case 2:
			value = OsdIn16(h->addr + offset);
			value &= 0xffff;
			break;
		case 4:
			value = OsdIn32(h->addr + offset);
			break;
		}
		break;
	case AML_REGION_PCICFG:
		switch (h->unit) {
		case 1:
			OsdReadPciCfgByte(h->pci_bus, h->pci_devfunc,
			    h->addr + offset, (UINT8 *)&value);
			value &= 0xff;
			break;
		case 2:
			OsdReadPciCfgWord(h->pci_bus, h->pci_devfunc,
			    h->addr + offset, (UINT16 *)&value);
			value &= 0xffff;
			break;
		case 4:
			OsdReadPciCfgDword(h->pci_bus, h->pci_devfunc,
			    h->addr + offset, &value);
			break;
		}
		break;
	default:
		printf("aml_region_read_simple: not supported yet (%d)\n",
		    h->regtype);
		value = 0;
		break;
	}
	*valuep = value;
	return (0);
}

int
aml_region_write_simple(struct aml_region_handle *h, vm_offset_t offset, u_int32_t value)
{

	switch (h->regtype) {
	case AML_REGION_SYSMEM:
		/* XXX should be MI */
		switch (h->unit) {
		case 1:
			value &= 0xff;
			*(volatile u_int8_t *)(h->vaddr + offset) = value;
			break;
		case 2:
			value &= 0xffff;
			*(volatile u_int16_t *)(h->vaddr + offset) = value;
			break;
		case 4:
			*(volatile u_int32_t *)(h->vaddr + offset) = value;
			break;
		}
		break;
	case AML_REGION_SYSIO:
		switch (h->unit) {
		case 1:
			value &= 0xff;
			OsdOut8(h->addr + offset, value);
			break;
		case 2:
			value &= 0xffff;
			OsdOut16(h->addr + offset, value);
			break;
		case 4:
			OsdOut32(h->addr + offset, value);
			break;
		}
		break;
	case AML_REGION_PCICFG:
		switch (h->unit) {
		case 1:
			OsdWritePciCfgByte(h->pci_bus, h->pci_devfunc,
			    h->addr + offset, value);
			break;
		case 2:
			OsdWritePciCfgWord(h->pci_bus, h->pci_devfunc,
			    h->addr + offset, value);
			break;
		case 4:
			OsdWritePciCfgDword(h->pci_bus, h->pci_devfunc,
			    h->addr + offset, value);
			break;
		}
		break;
	default:
		printf("aml_region_write_simple: not supported yet (%d)\n",
		    h->regtype);
		break;
	}

	return (0);
}

static int
aml_region_io_buffer(boolean_t io, int regtype, u_int32_t flags,
    u_int8_t *buffer, u_int32_t baseaddr, u_int32_t bitoffset, u_int32_t bitlen)
{
	vm_offset_t	addr, vaddr;
	size_t		len;
	const char	*funcname[] = {
		"aml_region_read_into_buffer",
		"aml_region_write_from_buffer"
	};

	if (regtype != AML_REGION_SYSMEM) {
		printf("%s: region type isn't system memory!\n", funcname[io]);
		return (-1);
	}

	if (bitlen % 8) {
		printf("%s: bit length isn't a multiple of 8!\n", funcname[io]);
	}
	if (bitoffset % 8) {
		printf("%s: bit offset isn't a multiple of 8!\n", funcname[io]);
	}

	addr = baseaddr + bitoffset / 8;
	len = bitlen / 8 + ((bitlen % 8) ? 1 : 0);

	OsdMapMemory((void *)addr, len, (void **)&vaddr);

	switch (io) {
	case AML_REGION_INPUT:
		bcopy((void *)vaddr, (void *)buffer, len);
		break;
	case AML_REGION_OUTPUT:
		bcopy((void *)buffer, (void *)vaddr, len);
		break;
	}

	OsdUnMapMemory((void *)vaddr, len);

	return (0);
}

u_int32_t
aml_region_read(struct aml_environ *env, int regtype, u_int32_t flags,
    u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen)
{
	int	value;
	int	state;

	AML_REGION_READ_DEBUG(regtype, flags, addr, bitoffset, bitlen);

	state = aml_region_io(env, AML_REGION_INPUT, regtype,
	    flags, &value, addr, bitoffset, bitlen);
	AML_SYSASSERT(state != -1);

        return (value);
}

int
aml_region_read_into_buffer(struct aml_environ *env, int regtype,
    u_int32_t flags, u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen,
    u_int8_t *buffer)
{
	int	state;

	AML_REGION_READ_INTO_BUFFER_DEBUG(regtype, flags, addr, bitoffset, bitlen);
	state = aml_region_io_buffer(AML_REGION_INPUT, regtype, flags,
	    buffer, addr, bitoffset, bitlen);

        return (state);
}

int
aml_region_write(struct aml_environ *env, int regtype, u_int32_t flags,
    u_int32_t value, u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen)
{
	int	state;

	AML_REGION_WRITE_DEBUG(regtype, flags, value, addr, bitoffset, bitlen);

	state = aml_region_io(env, AML_REGION_OUTPUT, regtype,
	    flags, &value, addr, bitoffset, bitlen);
	AML_SYSASSERT(state != -1);

        return (state);
}

int
aml_region_write_from_buffer(struct aml_environ *env, int regtype,
    u_int32_t flags, u_int8_t *buffer, u_int32_t addr, u_int32_t bitoffset,
    u_int32_t bitlen)
{
	int	state;

	AML_REGION_WRITE_FROM_BUFFER_DEBUG(regtype, flags,
	    addr, bitoffset, bitlen);

	state = aml_region_io_buffer(AML_REGION_OUTPUT, regtype, flags,
	    buffer, addr, bitoffset, bitlen);

        return (state);
}

int
aml_region_bcopy(struct aml_environ *env, int regtype,
    u_int32_t flags, u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen,
    u_int32_t dflags, u_int32_t daddr, u_int32_t dbitoffset, u_int32_t dbitlen)
{
	vm_offset_t	from_addr, from_vaddr;
	vm_offset_t	to_addr, to_vaddr;
	size_t		len;

	AML_REGION_BCOPY_DEBUG(regtype, flags, addr, bitoffset, bitlen,
	    dflags, daddr, dbitoffset, dbitlen);

	if (regtype != AML_REGION_SYSMEM) {
		printf("aml_region_bcopy: region type isn't system memory!\n");
		return (-1);
	}

	if ((bitlen % 8) || (dbitlen % 8)) {
		printf("aml_region_bcopy: bit length isn't a multiple of 8!\n");
	}
	if ((bitoffset % 8) || (dbitoffset % 8)) {
		printf("aml_region_bcopy: bit offset isn't a multiple of 8!\n");
	}

	from_addr = addr + bitoffset / 8;
	to_addr = daddr + dbitoffset / 8;

	len = (bitlen > dbitlen) ? dbitlen : bitlen;
	len = len / 8 + ((len % 8) ? 1 : 0);

	OsdMapMemory((void *)from_addr, len, (void **)&from_vaddr);
	OsdMapMemory((void *)to_addr, len, (void **)&to_vaddr);

	bcopy((void *)from_vaddr, (void *)to_vaddr, len);

	OsdUnMapMemory((void *)from_vaddr, len);
	OsdUnMapMemory((void *)to_vaddr, len);

	return (0);
}
