/*-
 * Copyright (c) 1999 Takanori Watanabe
 * Copyright (c) 1999, 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: aml_common.c,v 1.9 2000/08/09 14:47:43 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>

#ifndef _KERNEL
#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#else /* _KERNEL */
#include "opt_acpi.h"
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <dev/acpi/acpireg.h>
#ifndef ACPI_NO_OSDFUNC_INLINE
#include <machine/acpica_osd.h>
#endif /* !ACPI_NO_OSDFUNC_INLINE */
#endif /* !_KERNEL */

#include <dev/acpi/aml/aml_common.h>
#include <dev/acpi/aml/aml_env.h>
#include <dev/acpi/aml/aml_evalobj.h>
#include <dev/acpi/aml/aml_name.h>
#include <dev/acpi/aml/aml_obj.h>
#include <dev/acpi/aml/aml_parse.h>
#include <dev/acpi/aml/aml_status.h>
#include <dev/acpi/aml/aml_store.h>

/* for debugging */
#ifdef AML_DEBUG
int	aml_debug = 1;
#else	/* !AML_DEBUG */
int	aml_debug = 0;
#endif	/* AML_DEBUG */
#ifdef _KERNEL
SYSCTL_INT(_debug, OID_AUTO, aml_debug, CTLFLAG_RW, &aml_debug, 1, "");
#endif /* _KERNEL */

static void	 aml_print_nameseg(u_int8_t *dp);

static void
aml_print_nameseg(u_int8_t *dp)
{

	if (dp[3] != '_') {
		AML_DEBUGPRINT("%c%c%c%c", dp[0], dp[1], dp[2], dp[3]);
	} else if (dp[2] != '_') {
		AML_DEBUGPRINT("%c%c%c_", dp[0], dp[1], dp[2]);
	} else if (dp[1] != '_') {
		AML_DEBUGPRINT("%c%c__", dp[0], dp[1]);
	} else if (dp[0] != '_') {
		AML_DEBUGPRINT("%c___", dp[0]);
	}
}

void
aml_print_namestring(u_int8_t *dp)
{
	int	segcount;
	int	i;

	if (dp[0] == '\\') {
		AML_DEBUGPRINT("%c", dp[0]);
		dp++;
	} else if (dp[0] == '^') {
		while (dp[0] == '^') {
			AML_DEBUGPRINT("%c", dp[0]);
			dp++;
		}
	}
	if (dp[0] == 0x00) {	/* NullName */
		/* AML_DEBUGPRINT("<null>"); */
		dp++;
	} else if (dp[0] == 0x2e) {	/* DualNamePrefix */
		aml_print_nameseg(dp + 1);
		AML_DEBUGPRINT("%c", '.');
		aml_print_nameseg(dp + 5);
	} else if (dp[0] == 0x2f) {	/* MultiNamePrefix */
		segcount = dp[1];
		for (i = 0, dp += 2; i < segcount; i++, dp += 4) {
			if (i > 0) {
				AML_DEBUGPRINT("%c", '.');
			}
			aml_print_nameseg(dp);
		}
	} else			/* NameSeg */
		aml_print_nameseg(dp);
}

int
aml_print_curname(struct aml_name *name)
{
	struct	aml_name *root;

	root = aml_get_rootname();
	if (name == root) {
		AML_DEBUGPRINT("\\");
		return (0);
	} else {
		aml_print_curname(name->parent);
	}
	aml_print_nameseg(name->name);
	AML_DEBUGPRINT(".");
	return (0);
}

void
aml_print_indent(int indent)
{
	int	i;

	for (i = 0; i < indent; i++)
		AML_DEBUGPRINT("    ");
}

void
aml_showobject(union aml_object * obj)
{
	int	debug;
	int	i;

	if (obj == NULL) {
		printf("NO object\n");
		return;
	}
	debug = aml_debug;
	aml_debug = 1;
	switch (obj->type) {
	case aml_t_num:
		printf("Num:0x%x\n", obj->num.number);
		break;
	case aml_t_processor:
		printf("Processor:No %d,Port 0x%x length 0x%x\n",
		    obj->proc.id, obj->proc.addr, obj->proc.len);
		break;
	case aml_t_mutex:
		printf("Mutex:Level %d\n", obj->mutex.level);
		break;
	case aml_t_powerres:
		printf("PowerResource:Level %d Order %d\n",
		    obj->pres.level, obj->pres.order);
		break;
	case aml_t_opregion:
		printf("OprationRegion:Busspace%d, Offset 0x%x Length 0x%x\n",
		    obj->opregion.space, obj->opregion.offset,
		    obj->opregion.length);
		break;
	case aml_t_field:
		printf("Fieldelement:flag 0x%x offset 0x%x len 0x%x {",
		    obj->field.flags, obj->field.bitoffset,
		    obj->field.bitlen);
		switch (obj->field.f.ftype) {
		case f_t_field:
			aml_print_namestring(obj->field.f.fld.regname);
			break;
		case f_t_index:
			aml_print_namestring(obj->field.f.ifld.indexname);
			printf(" ");
			aml_print_namestring(obj->field.f.ifld.dataname);
			break;
		case f_t_bank:
			aml_print_namestring(obj->field.f.bfld.regname);
			printf(" ");
			aml_print_namestring(obj->field.f.bfld.bankname);
			printf("0x%x", obj->field.f.bfld.bankvalue);
			break;
		}
		printf("}\n");
		break;
	case aml_t_method:
		printf("Method: Arg %d From %p To %p\n", obj->meth.argnum,
		    obj->meth.from, obj->meth.to);
		break;
	case aml_t_buffer:
		printf("Buffer: size:0x%x Data %p\n", obj->buffer.size,
		    obj->buffer.data);
		break;
	case aml_t_device:
		printf("Device\n");
		break;
	case aml_t_bufferfield:
		printf("Bufferfield:offset 0x%x len 0x%x Origin %p\n",
		    obj->bfld.bitoffset, obj->bfld.bitlen, obj->bfld.origin);
		break;
	case aml_t_string:
		printf("String:%s\n", obj->str.string);
		break;
	case aml_t_package:
		printf("Package:elements %d \n", obj->package.elements);
		for (i = 0; i < obj->package.elements; i++) {
			if (obj->package.objects[i] == NULL) {
				break;
			}
			if (obj->package.objects[i]->type < 0) {
				continue;
			}
			printf("        ");
			aml_showobject(obj->package.objects[i]);
		}
		break;
	case aml_t_therm:
		printf("Thermalzone\n");
		break;
	case aml_t_event:
		printf("Event\n");
		break;
	case aml_t_ddbhandle:
		printf("DDBHANDLE\n");
		break;
	case aml_t_objref:
		if (obj->objref.alias == 1) {
			printf("Alias");
		} else {
			printf("Object reference");
			if (obj->objref.offset >= 0) {
				printf(" (offset 0x%x)", obj->objref.offset);
			}
		}
		printf(" of ");
		aml_showobject(obj->objref.ref);
		break;
	default:
		printf("UNK ID=%d\n", obj->type);
	}

	aml_debug = debug;
}

void
aml_showtree(struct aml_name * aname, int lev)
{
	int	i;
	struct	aml_name *ptr;
	char	name[5];

	for (i = 0; i < lev; i++) {
		printf("  ");
	}
	strncpy(name, aname->name, 4);
	name[4] = 0;
	printf("%s  ", name);
	if (aname->property != NULL) {
		aml_showobject(aname->property);
	} else {
		printf("\n");
	}
	for (ptr = aname->child; ptr; ptr = ptr->brother)
		aml_showtree(ptr, lev + 1);
}

/*
 * Common Region I/O Stuff
 */

static __inline u_int64_t
aml_adjust_bitmask(u_int32_t flags, u_int32_t bitlen)
{
	u_int64_t	bitmask;

	switch (AML_FIELDFLAGS_ACCESSTYPE(flags)) {
	case AML_FIELDFLAGS_ACCESS_ANYACC:
		if (bitlen <= 8) {
			bitmask = 0x000000ff;
			break;
		}
		if (bitlen <= 16) {
			bitmask = 0x0000ffff;
			break;
		}
		bitmask = 0xffffffff;
		break;
	case AML_FIELDFLAGS_ACCESS_BYTEACC:
		bitmask = 0x000000ff;
		break;
	case AML_FIELDFLAGS_ACCESS_WORDACC:
		bitmask = 0x0000ffff;
		break;
	case AML_FIELDFLAGS_ACCESS_DWORDACC:
	default:
		bitmask = 0xffffffff;
		break;
	}

	switch (bitlen) {
	case 16:
		bitmask |= 0x0000ffff;
		break;
	case 32:
		bitmask |= 0xffffffff;
		break;
	}

	return (bitmask);
}

u_int32_t
aml_adjust_readvalue(u_int32_t flags, u_int32_t bitoffset, u_int32_t bitlen,
    u_int32_t orgval)
{
	u_int32_t	offset, retval;
	u_int64_t	bitmask;

	offset = bitoffset;	/* XXX bitoffset may change in this function! */
	bitmask = aml_adjust_bitmask(flags, bitlen);
	retval = (orgval >> offset) & (~(bitmask << bitlen)) & bitmask;

	return (retval);
}

u_int32_t
aml_adjust_updatevalue(u_int32_t flags, u_int32_t bitoffset, u_int32_t bitlen,
    u_int32_t orgval, u_int32_t value)
{
	u_int32_t	offset, retval;
	u_int64_t	bitmask;

	offset = bitoffset;	/* XXX bitoffset may change in this function! */
	bitmask = aml_adjust_bitmask(flags, bitlen);
	retval = orgval;
	switch (AML_FIELDFLAGS_UPDATERULE(flags)) {
	case AML_FIELDFLAGS_UPDATE_PRESERVE:
		retval &= (~(((u_int64_t)1 << bitlen) - 1) << offset) |
			  (~(bitmask << offset));
		break;
	case AML_FIELDFLAGS_UPDATE_WRITEASONES:
		retval =  (~(((u_int64_t)1 << bitlen) - 1) << offset) |
			  (~(bitmask << offset));
		retval &= bitmask;	/* trim the upper bits */
		break;
	case AML_FIELDFLAGS_UPDATE_WRITEASZEROS:
		retval = 0;
		break;
	default:
		printf("illegal update rule: %d\n", flags);
		return (orgval);
	}

	retval |= (value << (offset & bitmask));
	return (retval);
}

/*
 * BufferField I/O
 */

#define AML_BUFFER_INPUT	0
#define AML_BUFFER_OUTPUT	1

static int	 aml_bufferfield_io(int io, u_int32_t *valuep,
				    u_int8_t *origin, u_int32_t bitoffset,
				    u_int32_t bitlen);

static int
aml_bufferfield_io(int io, u_int32_t *valuep, u_int8_t *origin,
    u_int32_t bitoffset, u_int32_t bitlen)
{
	u_int8_t	val, tmp, masklow, maskhigh;
	u_int8_t	offsetlow, offsethigh;
	u_int8_t	*addr;
	int		i;
	u_int32_t	value, readval;
	u_int32_t	byteoffset, bytelen;

	masklow = maskhigh = 0xff;
	val = readval = 0;
	value = *valuep;

	byteoffset = bitoffset / 8;
	bytelen = bitlen / 8 + ((bitlen % 8) ? 1 : 0);
	addr = origin + byteoffset;

	/* simple I/O ? */
	if (bitlen <= 8 || bitlen == 16 || bitlen == 32) {
		bcopy(addr, &readval, bytelen);
		AML_DEBUGPRINT("\n\t[bufferfield:0x%x@%p:%d,%d]",
		    readval, addr, bitoffset % 8, bitlen);
		switch (io) {
		case AML_BUFFER_INPUT:
			value = aml_adjust_readvalue(AML_FIELDFLAGS_ACCESS_BYTEACC, 
			    bitoffset % 8, bitlen, readval);
			*valuep = value;
			AML_DEBUGPRINT("\n[read(bufferfield, %p)&mask:0x%x]\n",
			    addr, value);
			break;
		case AML_BUFFER_OUTPUT:
			value = aml_adjust_updatevalue(AML_FIELDFLAGS_ACCESS_BYTEACC, 
			    bitoffset % 8, bitlen, readval, value);
			bcopy(&value, addr, bytelen);
			AML_DEBUGPRINT("->[bufferfield:0x%x@%p:%d,%d]",
			    value, addr, bitoffset % 8, bitlen);
			break;
		}
		goto out;
	}
 
	offsetlow = bitoffset % 8;
	if (bytelen > 1) {
		offsethigh = (bitlen - (8 - offsetlow)) % 8;
	} else {
		offsethigh = 0;
	}

	if (offsetlow) {
		masklow = (~((1 << bitlen) - 1) << offsetlow) | ~(0xff << offsetlow);
		AML_DEBUGPRINT("\t[offsetlow = 0x%x, masklow = 0x%x, ~masklow = 0x%x]\n",
		    offsetlow, masklow, ~masklow & 0xff);
	}
	if (offsethigh) {
		maskhigh = 0xff << offsethigh;
		AML_DEBUGPRINT("\t[offsethigh = 0x%x, maskhigh = 0x%x, ~maskhigh = 0x%x]\n",
		    offsethigh, maskhigh, ~maskhigh & 0xff);
	}
	for (i = bytelen; i > 0; i--, addr++) {
		val = *addr;

		AML_DEBUGPRINT("\t[bufferfield:0x%02x@%p]", val, addr);

		switch (io) {
		case AML_BUFFER_INPUT:
			tmp = val;
			/* the lowest byte? */
			if (i == bytelen) {
				if (offsetlow) {
					readval = tmp & ~masklow;
				} else {
					readval = tmp;
				}
			} else {
				if (i == 1 && offsethigh) {
					tmp = tmp & ~maskhigh;
				}
				readval = (tmp << (8 * (bytelen - i))) | readval;
			}

			AML_DEBUGPRINT("\n");
			/* goto to next byte... */
			if (i > 1) {
				continue;
			}
			/* final adjustment before finishing region access */
			if (offsetlow) {
				readval = readval >> offsetlow;
			}
			AML_DEBUGPRINT("[read(bufferfield, %p)&mask:0x%x]\n",
			    addr, readval);
			*valuep = readval;

			break;

		case AML_BUFFER_OUTPUT:
			tmp = value & 0xff;
			/* the lowest byte? */
			if (i == bytelen) {
				if (offsetlow) {
					tmp = (val & masklow) | tmp << offsetlow;
				}
				value = value >> (8 - offsetlow);
			} else {
				if (i == 1 && offsethigh) {
					tmp = (val & maskhigh) | tmp;
				}
				value = value >> 8;
			}

			AML_DEBUGPRINT("->[bufferfield:0x%02x@%p]\n",
			    tmp, addr);
			*addr = tmp;
		}
	}
out:
	return (0);
}

u_int32_t
aml_bufferfield_read(u_int8_t *origin, u_int32_t bitoffset,
    u_int32_t bitlen)
{
	int	value;

	value = 0;
	aml_bufferfield_io(AML_BUFFER_INPUT, &value, origin,
	    bitoffset, bitlen);
	return (value);
}

int
aml_bufferfield_write(u_int32_t value, u_int8_t *origin,
    u_int32_t bitoffset, u_int32_t bitlen)
{
	int	status;

	status = aml_bufferfield_io(AML_BUFFER_OUTPUT, &value,
	    origin, bitoffset, bitlen);
	return (status);
}

int
aml_region_handle_alloc(struct aml_environ *env, int regtype, u_int32_t flags,
    u_int32_t baseaddr, u_int32_t bitoffset, u_int32_t bitlen,
    struct aml_region_handle *h)
{
	int	state;
	struct	aml_name *pci_info;

	state = 0;
	pci_info = NULL;
	bzero(h, sizeof(struct aml_region_handle));

	h->env = env;
	h->regtype = regtype;
	h->flags = flags;
	h->baseaddr = baseaddr;
	h->bitoffset = bitoffset;
	h->bitlen = bitlen;

	switch (AML_FIELDFLAGS_ACCESSTYPE(flags)) {
	case AML_FIELDFLAGS_ACCESS_ANYACC:
		if (bitlen <= 8) {
			h->unit = 1;
			break;
		}
		if (bitlen <= 16) {
			h->unit = 2;
			break;
		}
		h->unit = 4;
		break;
	case AML_FIELDFLAGS_ACCESS_BYTEACC:
		h->unit = 1;
		break;
	case AML_FIELDFLAGS_ACCESS_WORDACC:
		h->unit = 2;
		break;
	case AML_FIELDFLAGS_ACCESS_DWORDACC:
		h->unit = 4;
		break;
	default:
		h->unit = 1;
		break;
	}

	h->addr = baseaddr + h->unit * ((bitoffset / 8) / h->unit);
	h->bytelen = baseaddr + ((bitoffset + bitlen) / 8) - h->addr +
		     ((bitlen % 8) ? 1 : 0);

#ifdef _KERNEL
	switch (h->regtype) {
	case AML_REGION_SYSMEM:
		OsdMapMemory((void *)h->addr, h->bytelen, (void **)&h->vaddr);
		break;

	case AML_REGION_PCICFG:
		/* Obtain PCI bus number */
		pci_info = aml_search_name(env, "_BBN");
		if (pci_info == NULL || pci_info->property->type != aml_t_num) {
			AML_DEBUGPRINT("Cannot locate _BBN. Using default 0\n");
			h->pci_bus = 0;
		} else {
			AML_DEBUGPRINT("found _BBN: %d\n",
			    pci_info->property->num.number);
			h->pci_bus = pci_info->property->num.number & 0xff;
		}

		/* Obtain device & function number */
		pci_info = aml_search_name(env, "_ADR");
		if (pci_info == NULL || pci_info->property->type != aml_t_num) {
			printf("Cannot locate: _ADR\n");
			state = -1;
			goto out;
		}
		h->pci_devfunc = pci_info->property->num.number;

		AML_DEBUGPRINT("[pci%d.%d]", h->pci_bus, h->pci_devfunc);
		break;

	default:
		break;
	}

out:
#endif	/* _KERNEL */
	return (state);
}

void
aml_region_handle_free(struct aml_region_handle *h)
{
#ifdef _KERNEL
	switch (h->regtype) {
	case AML_REGION_SYSMEM:
		OsdUnMapMemory((void *)h->vaddr, h->bytelen);
		break;

	default:
		break;
	}
#endif	/* _KERNEL */
}

static int
aml_region_io_simple(struct aml_environ *env, int io, int regtype,
    u_int32_t flags, u_int32_t *valuep, u_int32_t baseaddr,
    u_int32_t bitoffset, u_int32_t bitlen)
{
	int		i, state;
	u_int32_t	readval, value, offset, bytelen;
	struct		aml_region_handle handle;

	state = aml_region_handle_alloc(env, regtype, flags,
            baseaddr, bitoffset, bitlen, &handle);
	if (state == -1) {
		goto out;
	}

	readval = 0;
	offset = bitoffset % (handle.unit * 8);
	/* limitation of 32 bits alignment */
	bytelen = (handle.bytelen > 4) ? 4 : handle.bytelen;

	if (io == AML_REGION_INPUT ||
	    AML_FIELDFLAGS_UPDATERULE(flags) == AML_FIELDFLAGS_UPDATE_PRESERVE) {
		for (i = 0; i < bytelen; i += handle.unit) {
			state = aml_region_read_simple(&handle, i, &value);
			if (state == -1) {
				goto out;
			}
			readval |= (value << (i * 8));
		}
		AML_DEBUGPRINT("\t[%d:0x%x@0x%x:%d,%d]",
		    regtype, readval, handle.addr, offset, bitlen);
	}

	switch (io) {
	case AML_REGION_INPUT:
		AML_DEBUGPRINT("\n");
		readval = aml_adjust_readvalue(flags, offset, bitlen, readval);
		value = readval;
		value = aml_region_prompt_read(&handle, value);
		state = aml_region_prompt_update_value(readval, value, &handle);
		if (state == -1) {
			goto out;
		}

		*valuep = value;
		break;
	case AML_REGION_OUTPUT:
		value = *valuep;
		value = aml_adjust_updatevalue(flags, offset,
		    bitlen, readval, value);
		value = aml_region_prompt_write(&handle, value);
		AML_DEBUGPRINT("\t->[%d:0x%x@0x%x:%d,%d]\n", regtype, value,
		    handle.addr, offset, bitlen);
		for (i = 0; i < bytelen; i += handle.unit) {
			state = aml_region_write_simple(&handle, i, value);
			if (state == -1) {
				goto out;
			}
			value = value >> (handle.unit * 8);
		}
		break;
	}

	aml_region_handle_free(&handle);
out:
	return (state);
}

int
aml_region_io(struct aml_environ *env, int io, int regtype,
    u_int32_t flags, u_int32_t *valuep, u_int32_t baseaddr,
    u_int32_t bitoffset, u_int32_t bitlen)
{
	u_int32_t	unit, offset;
	u_int32_t	offadj, bitadj;
	u_int32_t	value, readval;
	int		state, i;

	readval = 0;
	state = 0;
	unit = 4;	/* limitation of 32 bits alignment */
	offset = bitoffset % (unit * 8);
	offadj = 0;
	bitadj = 0;
	if (offset + bitlen > unit * 8) {
		bitadj = bitlen  - (unit * 8 - offset);
	}
	for (i = 0; i < offset + bitlen; i += unit * 8) {
		value = (*valuep) >> offadj;
		state = aml_region_io_simple(env, io, regtype, flags,
		    &value, baseaddr, bitoffset + offadj, bitlen - bitadj);
		if (state == -1) {
			goto out;
		}
		readval |= value << offadj;
		bitadj = offadj = bitlen - bitadj;
	}
	*valuep = readval;

out:
	return (state);
}
