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
	int		value = *valuep, readval;
	int		i;
	u_int32_t	byteoffset, bytelen;

	masklow = maskhigh = 0xff;
	val = readval = 0;
	value = *valuep;

	byteoffset = bitoffset / 8;
	bytelen = bitlen / 8 + ((bitlen % 8) ? 1 : 0);
	addr = origin + byteoffset;
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
