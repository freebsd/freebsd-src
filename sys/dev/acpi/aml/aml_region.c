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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/acpi.h>

#include <dev/acpi/aml/aml_common.h>
#include <dev/acpi/aml/aml_region.h>
#include <dev/acpi/aml/aml_name.h>

#ifndef ACPI_NO_OSDFUNC_INLINE
#include <machine/acpica_osd.h>
#endif

#define AML_REGION_INPUT	0
#define AML_REGION_OUTPUT	1

#define AML_REGION_SYSMEM	0
#define AML_REGION_SYSIO	1
#define AML_REGION_PCICFG	2
#define AML_REGION_EMBCTL	3
#define AML_REGION_SMBUS	4

static int
aml_region_io_system(struct aml_environ *env, boolean_t io, int regtype,
    u_int32_t flags, u_int32_t *valuep, u_int32_t baseaddr,
    u_int32_t bitoffset, u_int32_t bitlen)
{
	u_int8_t	val, tmp, masklow, maskhigh;
	u_int8_t	offsetlow, offsethigh;
	vm_offset_t	addr, vaddr, byteoffset, bytelen;
	u_int32_t	pci_bus;
	u_int32_t	pci_devfunc;
	int		value, readval;
	int		state, i;
	int		debug;

	/* save debug level and shut it up */
	debug = aml_debug;
	aml_debug = 0;

	value = *valuep;
	val = readval = 0;
	masklow = maskhigh = 0xff;
	state = 0;
	vaddr = 0;
	pci_bus = pci_devfunc = 0;

	byteoffset = bitoffset / 8;
	bytelen = bitlen / 8 + ((bitlen % 8) ? 1 : 0);
	addr = baseaddr + byteoffset;
	offsetlow = bitoffset % 8;
	if (bytelen > 1) {
		offsethigh = (bitlen - (8 - offsetlow)) % 8;
	} else {
		offsethigh = 0;
	}

	if (offsetlow) {
		masklow = (~((1 << bitlen) - 1) << offsetlow) | \
		    ~(0xff << offsetlow);
		AML_DEBUGPRINT("\t[offsetlow = 0x%x, masklow = 0x%x, ~masklow = 0x%x]\n",
		    offsetlow, masklow, ~masklow & 0xff);
	}
	if (offsethigh) {
		maskhigh = 0xff << offsethigh;
		AML_DEBUGPRINT("\t[offsethigh = 0x%x, maskhigh = 0x%x, ~maskhigh = 0x%x]\n",
		    offsethigh, maskhigh, ~maskhigh & 0xff);
	}

	if (regtype == AML_REGION_SYSMEM) {
		OsdMapMemory((void *)addr, bytelen, (void **)&vaddr);
	}
	if (regtype == AML_REGION_PCICFG) {
		/* Access to PCI Config space */
		struct aml_name *pci_info;

		/* Obtain PCI bus number */
		pci_info = aml_search_name(env, "_BBN");
		if (!pci_info || pci_info->property->type != aml_t_num) {
			AML_DEBUGPRINT("Cannot locate _BBN. Using default 0\n");
			pci_bus = 0;
		} else {
			AML_DEBUGPRINT("found _BBN: %d\n",
			    pci_info->property->num.number);
			pci_bus = pci_info->property->num.number & 0xff;
		}

		/* Obtain device & function number */
		pci_info = aml_search_name(env, "_ADR");
		if (!pci_info || pci_info->property->type != aml_t_num) {
			printf("Cannot locate: _ADR\n");
			state = -1;
			goto io_done;
		}
		pci_devfunc = pci_info->property->num.number;

		AML_DEBUGPRINT("[pci%d.%d]", pci_bus, pci_devfunc);
	}

	/* simple I/O ? */
	if (offsetlow == 0 && offsethigh == 0 &&
	    (bitlen == 8 || bitlen == 16 || bitlen == 32)) {
		switch (io) {
		case AML_REGION_INPUT:
			switch (regtype) {
			case AML_REGION_SYSMEM:
				/* XXX should be MI */
				switch (bitlen) {
				case 8:
					value = *(volatile u_int8_t *)(vaddr);
					value &= 0xff;
					break;
				case 16:
					value = *(volatile u_int16_t *)(vaddr);
					value &= 0xffff;
					break;
				case 32:
					value = *(volatile u_int32_t *)(vaddr);
					break;
				}
				break;
			case AML_REGION_SYSIO:
				switch (bitlen) {
				case 8:
					value = OsdIn8(addr);
					value &= 0xff;
					break;
				case 16:
					value = OsdIn16(addr);
					value &= 0xffff;
					break;
				case 32:
					value = OsdIn32(addr);
					break;
				}
				break;
			case AML_REGION_PCICFG:
				switch (bitlen) {
				case 8:
					OsdReadPciCfgByte(pci_bus, pci_devfunc,
					    addr, (UINT8 *)&value);
					value &= 0xff;
					break;
				case 16:
					OsdReadPciCfgWord(pci_bus, pci_devfunc,
					    addr, (UINT16 *)&value);
					value &= 0xffff;
					break;
				case 32:
					OsdReadPciCfgDword(pci_bus, pci_devfunc,
					    addr, &value);
					break;
				}
				break;
			default:
				printf("aml_region_io_system: not supported yet (%d)\n",
				    regtype);
				value = 0;
				break;
			}
			*valuep = value;
			break;
		case AML_REGION_OUTPUT:
			switch (regtype) {
			case AML_REGION_SYSMEM:
				/* XXX should be MI */
				switch (bitlen) {
				case 8:
					value &= 0xff;
					*(volatile u_int8_t *)(vaddr) = value;
					break;
				case 16:
					value &= 0xffff;
					*(volatile u_int16_t *)(vaddr) = value;
					break;
				case 32:
					*(volatile u_int32_t *)(vaddr) = value;
					break;
				}
				break;
			case AML_REGION_SYSIO:
				switch (bitlen) {
				case 8:
					value &= 0xff;
					OsdOut8(addr, value);
					break;
				case 16:
					value &= 0xffff;
					OsdOut16(addr, value);
					break;
				case 32:
					OsdOut32(addr, value);
					break;
				}
				break;
			case AML_REGION_PCICFG:
				switch (bitlen) {
				case 8:
					OsdWritePciCfgByte(pci_bus, pci_devfunc,
					    addr, value);
					break;
				case 16:
					OsdWritePciCfgWord(pci_bus, pci_devfunc,
					    addr, value);
					break;
				case 32:
					OsdWritePciCfgDword(pci_bus, pci_devfunc,
					    addr, value);
					break;
				}
				break;
			default:
				printf("aml_region_io_system: not supported yet (%d)\n",
				    regtype);
				break;
			}
			break;
		}
		goto io_done;
	}

	for (i = 0; i < bytelen; i++) {
		/* XXX */
		switch (regtype) {
		case AML_REGION_SYSMEM:
			val = *(volatile u_int8_t *)(vaddr + i);
			break;
		case AML_REGION_SYSIO:
			val = OsdIn8(addr + i);
			break;
		case AML_REGION_PCICFG:
			OsdReadPciCfgByte(pci_bus, pci_devfunc, addr + i, &val);
			break;
		default:
			printf("aml_region_io_system: not supported yet (%d)\n",
			    regtype);
			val = 0;
			break;
		}

		AML_DEBUGPRINT("\t[%d:0x%02x@0x%x]", regtype, val, addr + i);

		switch (io) {
		case AML_REGION_INPUT:
			tmp = val;
			/* the lowest byte? */
			if (i == 0) {
				if (offsetlow) {
					readval = tmp & ~masklow;
				} else {
					readval = tmp;
				}
			} else {
				if (i == bytelen - 1 && offsethigh) {
					tmp = tmp & ~maskhigh;
				}
				readval = (tmp << (8 * i)) | readval;
			}

			AML_DEBUGPRINT("\n");
			/* goto to next byte... */
			if (i < bytelen - 1) {
				continue;
			}
			/* final adjustment before finishing region access */
			if (offsetlow) {
				readval = readval >> offsetlow;
			}
			AML_DEBUGPRINT("\t[read(%d, 0x%x)&mask:0x%x]\n",
			    regtype, addr + i, readval);
			value = readval;
			*valuep = value;

			break;
		case AML_REGION_OUTPUT:
			tmp = value & 0xff;
			/* the lowest byte? */
			if (i == 0) {
				if (offsetlow) {
					tmp = (val & masklow) | tmp << offsetlow;
				}
				value = value >> (8 - offsetlow);
			} else {
				if (i == bytelen - 1 && offsethigh) {
					tmp = (val & maskhigh) | tmp;
				}
				value = value >> 8;
			}

			AML_DEBUGPRINT("->[%d:0x%02x@0x%x]\n",
			    regtype, tmp, addr + i);
			val = tmp;

			/* XXX */
			switch (regtype) {
			case AML_REGION_SYSMEM:
				*(volatile u_int8_t *)(vaddr + i) = val;
				break;
			case AML_REGION_SYSIO:
				OsdOut8(addr + i, val);
				break;
			case AML_REGION_PCICFG:
				OsdWritePciCfgByte(pci_bus, pci_devfunc,
				    addr + i, val);
				break;
			default:
				printf("aml_region_io_system: not supported yet (%d)\n",
				    regtype);
				break;
			}

			break;
		}
	}

io_done:
	if (regtype == AML_REGION_SYSMEM) {
		OsdUnMapMemory((void *)vaddr, bytelen);
	}

	aml_debug = debug;	/* restore debug devel */

	return (state);
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

	state = aml_region_io_system(env, AML_REGION_INPUT, regtype,
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

	state = aml_region_io_buffer(AML_REGION_INPUT, regtype, flags,
	    buffer, addr, bitoffset, bitlen);

        return (state);
}

int
aml_region_write(struct aml_environ *env, int regtype, u_int32_t flags,
    u_int32_t value, u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen)
{
	int	state;

	state = aml_region_io_system(env, AML_REGION_OUTPUT, regtype,
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
