/*-
 * Copyright (c) 1999 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: region.c,v 1.14 2000/08/08 14:12:25 iwasaki Exp $
 *	$FreeBSD$
 */

/*
 * Region I/O subroutine
 */

#include <sys/param.h>
#include <sys/queue.h>

#include <dev/acpi/aml/aml_amlmem.h>
#include <dev/acpi/aml/aml_name.h>
#include <dev/acpi/aml/aml_region.h>
#include <dev/acpi/aml/aml_common.h>

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"

int	aml_debug_prompt_regoutput = 0;
int	aml_debug_prompt_reginput = 1;

static void	aml_simulation_regload(const char *dumpfile);

struct ACPIRegionContent {
	TAILQ_ENTRY(ACPIRegionContent) links;
	int		regtype;
	u_int32_t	addr;
	u_int8_t	value;
};

TAILQ_HEAD(ACPIRegionContentList, ACPIRegionContent);
struct	ACPIRegionContentList RegionContentList;

static int	aml_simulation_initialized = 0;

static void
aml_simulation_init()
{

	aml_simulation_initialized = 1;
	TAILQ_INIT(&RegionContentList);
	aml_simulation_regload("region.ini");
}

static int
aml_simulate_regcontent_add(int regtype, u_int32_t addr, u_int8_t value)
{
	struct	ACPIRegionContent *rc;

	rc = malloc(sizeof(struct ACPIRegionContent));
	if (rc == NULL) {
		return (-1);	/* malloc fail */
	}
	rc->regtype = regtype;
	rc->addr = addr;
	rc->value = value;

	TAILQ_INSERT_TAIL(&RegionContentList, rc, links);
	return (0);
}

static int
aml_simulate_regcontent_read(int regtype, u_int32_t addr, u_int8_t *valuep)
{
	struct	ACPIRegionContent *rc;

	if (!aml_simulation_initialized) {
		aml_simulation_init();
	}
	TAILQ_FOREACH(rc, &RegionContentList, links) {
		if (rc->regtype == regtype && rc->addr == addr) {
			*valuep = rc->value;
			return (1);	/* found */
		}
	}

	return (aml_simulate_regcontent_add(regtype, addr, 0));
}

static int
aml_simulate_regcontent_write(int regtype, u_int32_t addr, u_int8_t *valuep)
{
	struct	ACPIRegionContent *rc;

	if (!aml_simulation_initialized) {
		aml_simulation_init();
	}
	TAILQ_FOREACH(rc, &RegionContentList, links) {
		if (rc->regtype == regtype && rc->addr == addr) {
			rc->value = *valuep;
			return (1);	/* exists */
		}
	}

	return (aml_simulate_regcontent_add(regtype, addr, *valuep));
}

static u_int32_t
aml_simulate_prompt(char *msg, u_int32_t def_val)
{
	char		buf[16], *ep;
	u_int32_t	val;

	val = def_val;
	printf("DEBUG");
	if (msg != NULL) {
		printf("%s", msg);
	}
	printf("(default: 0x%x / %u) >>", val, val);
	fflush(stdout);

	bzero(buf, sizeof buf);
	while (1) {
		if (read(0, buf, sizeof buf) == 0) {
			continue;
		}
		if (buf[0] == '\n') {
			break;	/* use default value */
		}
		if (buf[0] == '0' && buf[1] == 'x') {
			val = strtoq(buf, &ep, 16);
		} else {
			val = strtoq(buf, &ep, 10);
		}
		break;
	}
	return (val);
}

static void
aml_simulation_regload(const char *dumpfile)
{
	char	buf[256], *np, *ep;
	struct	ACPIRegionContent rc;
	FILE	*fp;

	if (!aml_simulation_initialized) {
		return;
	}
	if ((fp = fopen(dumpfile, "r")) == NULL) {
		warn(dumpfile);
		return;
	}
	while (fgets(buf, sizeof buf, fp) != NULL) {
		np = buf;
		/* reading region type */
		rc.regtype = strtoq(np, &ep, 10);
		if (np == ep) {
			continue;
		}
		np = ep;

		/* reading address */
		rc.addr = strtoq(np, &ep, 16);
		if (np == ep) {
			continue;
		}
		np = ep;

		/* reading value */
		rc.value = strtoq(np, &ep, 16);
		if (np == ep) {
			continue;
		}
		aml_simulate_regcontent_write(rc.regtype, rc.addr, &rc.value);
	}

	fclose(fp);
}

int
aml_region_read_simple(struct aml_region_handle *h, vm_offset_t offset, 
    u_int32_t *valuep)
{
	int		i, state;
	u_int8_t	val;
	u_int32_t	value;

	state = 0;
	value = val = 0;
	for (i = 0; i < h->unit; i++) {
		state = aml_simulate_regcontent_read(h->regtype,
		    h->addr + offset + i, &val);
		if (state == -1) {
			goto out;
		}
		value |= val << (i * 8);
	}
	*valuep = value;
out:
	return (state);
}

int
aml_region_write_simple(struct aml_region_handle *h, vm_offset_t offset,
    u_int32_t value)
{
	int		i, state;
	u_int8_t	val;

	state = 0;
	val = 0;
	for (i = 0; i < h->unit; i++) {
		val = value & 0xff;
		state = aml_simulate_regcontent_write(h->regtype,
		    h->addr + offset + i, &val);
		if (state == -1) {
			goto out;
		}
		value = value >> 8;
	}
out:
	return (state);
}

u_int32_t
aml_region_prompt_read(struct aml_region_handle *h, u_int32_t value)
{
	u_int32_t 	retval;
	char		buf[64];

	retval = value;
	sprintf(buf, "[read(%d, 0x%x)&mask:0x%x]",
	    h->regtype, h->addr, value);
	if (aml_debug_prompt_reginput) {
		retval = aml_simulate_prompt(buf, value);
	} else {
		printf("\t%s\n", buf);
	}

	return (retval);
}

u_int32_t
aml_region_prompt_write(struct aml_region_handle *h, u_int32_t value)
{
	u_int32_t 	retval;
	char		buf[64];

	retval = value;
	if (aml_debug_prompt_regoutput) {
		printf("\n");
		sprintf(buf, "[write(%d, 0x%x, 0x%x)]",
		    h->regtype, value, h->addr);
		retval = aml_simulate_prompt(buf, value);
	}

	return (retval);
}

int
aml_region_prompt_update_value(u_int32_t orgval, u_int32_t value,
    struct aml_region_handle *h)
{
	int	state;

	state = 0;
	if (orgval != value) {
		state = aml_region_io(h->env, AML_REGION_OUTPUT, h->regtype,
		    h->flags, &value, h->baseaddr, h->bitoffset, h->bitlen);
		if (state == -1) {
			goto out;
		}
	}

out:
	return (state);
}

static int
aml_simulate_region_io_buffer(int io, int regtype, u_int32_t flags,
    u_int8_t *buffer, u_int32_t baseaddr, u_int32_t bitoffset, u_int32_t bitlen)
{
	u_int8_t	val;
	u_int8_t	offsetlow, offsethigh;
	u_int32_t	addr, byteoffset, bytelen;
	int		state, i;

	val = 0;
	offsetlow = offsethigh = 0;
	state = 0;

	byteoffset = bitoffset / 8;
	bytelen = bitlen / 8 + ((bitlen % 8) ? 1 : 0);
	addr = baseaddr + byteoffset;
	offsetlow = bitoffset % 8;
	assert(offsetlow == 0);

	if (bytelen > 1) {
		offsethigh = (bitlen - (8 - offsetlow)) % 8;
	}
	assert(offsethigh == 0);

	for (i = bytelen; i > 0; i--, addr++) {
		switch (io) {
		case AML_REGION_INPUT:
			val = 0;
			state = aml_simulate_regcontent_read(regtype, addr, &val);
			if (state == -1) {
				goto finish;
			}
			buffer[bytelen - i] = val;
			break;
		case AML_REGION_OUTPUT:
			val = buffer[bytelen - i];
			state = aml_simulate_regcontent_write(regtype,
			    addr, &val);
			if (state == -1) {
				goto finish;
			}
			break;
		}
	}
finish:
	return (state);
}

static u_int32_t
aml_simulate_region_read(struct aml_environ *env, int regtype,
    u_int32_t flags, u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen)
{
	int	value;
	int	state;

	state = aml_region_io(env, AML_REGION_INPUT, regtype, flags, &value,
	    addr, bitoffset, bitlen);
	assert(state != -1);
	return (value);
}

int
aml_simulate_region_read_into_buffer(int regtype, u_int32_t flags,
    u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen, u_int8_t *buffer)
{
	int	state;

	state = aml_simulate_region_io_buffer(AML_REGION_INPUT, regtype, flags,
	    buffer, addr, bitoffset, bitlen);
	assert(state != -1);
	return (state);
}

int
aml_simulate_region_write(struct aml_environ *env, int regtype,
    u_int32_t flags, u_int32_t value, u_int32_t addr, u_int32_t bitoffset,
    u_int32_t bitlen)
{
	int	state;

	state = aml_region_io(env, AML_REGION_OUTPUT, regtype, flags,
	    &value, addr, bitoffset, bitlen);
	assert(state != -1);
	return (state);
}

int
aml_simulate_region_write_from_buffer(int regtype, u_int32_t flags,
    u_int8_t *buffer, u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen)
{
	int	state;

	state = aml_simulate_region_io_buffer(AML_REGION_OUTPUT, regtype,
	    flags, buffer, addr, bitoffset, bitlen);
	assert(state != -1);
	return (state);
}

int
aml_simulate_region_bcopy(struct aml_environ *env, int regtype,
    u_int32_t flags, u_int32_t addr,
    u_int32_t bitoffset, u_int32_t bitlen,
    u_int32_t dflags, u_int32_t daddr,
    u_int32_t dbitoffset, u_int32_t dbitlen)
{
	u_int32_t	len, i;
	u_int32_t	value;
	int		state;

	len = (bitlen > dbitlen) ? dbitlen : bitlen;
	len = len / 8 + ((len % 8) ? 1 : 0);

	for (i = 0; i < len; i++) {
		state = aml_region_io(env, AML_REGION_INPUT, regtype,
		    flags, &value, addr, bitoffset + i * 8, 8);
		assert(state != -1);
		state = aml_region_io(env, AML_REGION_OUTPUT, regtype,
		    dflags, &value, daddr, dbitoffset + i * 8, 8);
		assert(state != -1);
	}

	return (0);
}

u_int32_t
aml_region_read(struct aml_environ *env, int regtype, u_int32_t flags,
    u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen)
{

	AML_REGION_READ_DEBUG(regtype, flags, addr, bitoffset, bitlen);

	return (aml_simulate_region_read(env, regtype, flags, addr,
	    bitoffset, bitlen));
}

int
aml_region_read_into_buffer(struct aml_environ *env, int regtype,
    u_int32_t flags, u_int32_t addr, u_int32_t bitoffset,
    u_int32_t bitlen, u_int8_t *buffer)
{

	AML_REGION_READ_INTO_BUFFER_DEBUG(regtype, flags, addr, bitoffset, bitlen);

	return (aml_simulate_region_read_into_buffer(regtype, flags, addr,
	    bitoffset, bitlen, buffer));
}

int
aml_region_write(struct aml_environ *env, int regtype, u_int32_t flags,
    u_int32_t value, u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen)
{

	AML_REGION_WRITE_DEBUG(regtype, flags, value, addr, bitoffset, bitlen);

	return (aml_simulate_region_write(env, regtype, flags, value, addr,
	    bitoffset, bitlen));
}

int
aml_region_write_from_buffer(struct aml_environ *env, int regtype,
    u_int32_t flags, u_int8_t *buffer, u_int32_t addr, u_int32_t bitoffset,
    u_int32_t bitlen)
{

	AML_REGION_WRITE_FROM_BUFFER_DEBUG(regtype, flags,
	    addr, bitoffset, bitlen);

	return (aml_simulate_region_write_from_buffer(regtype, flags, buffer,
	    addr, bitoffset, bitlen));
}

int
aml_region_bcopy(struct aml_environ *env, int regtype, u_int32_t flags,
    u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen,
    u_int32_t dflags, u_int32_t daddr,
    u_int32_t dbitoffset, u_int32_t dbitlen)
{

	AML_REGION_BCOPY_DEBUG(regtype, flags, addr, bitoffset, bitlen,
	    dflags, daddr, dbitoffset, dbitlen);

	return (aml_simulate_region_bcopy(env, regtype, flags, addr, bitoffset,
	    bitlen, dflags, daddr, dbitoffset, dbitlen));
}

void
aml_simulation_regdump(const char *dumpfile)
{
	struct	ACPIRegionContent *rc;
	FILE	*fp;

	if (!aml_simulation_initialized) {
		return;
	}
	if ((fp = fopen(dumpfile, "w")) == NULL) {
		warn(dumpfile);
		return;
	}
	while (!TAILQ_EMPTY(&RegionContentList)) {
		rc = TAILQ_FIRST(&RegionContentList);
		fprintf(fp, "%d	0x%x	0x%x\n",
		    rc->regtype, rc->addr, rc->value);
		TAILQ_REMOVE(&RegionContentList, rc, links);
		free(rc);
	}

	fclose(fp);
	TAILQ_INIT(&RegionContentList);
}
