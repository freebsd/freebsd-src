/*
 * Copyright (C) 2002
 * 	Hidetoshi Shimokawa. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#include <sys/param.h>
#include <dev/firewire/firewire.h>
#include <dev/firewire/iec13213.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/kernel.h>
#else
#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#endif

void
crom_init_context(struct crom_context *cc, u_int32_t *p)
{
	struct csrhdr *hdr;

	hdr = (struct csrhdr *)p;
	if (hdr->info_len == 1) {
		/* minimum ROM */
		cc->depth = -1;
	}
	p += 1 + hdr->info_len;
	cc->depth = 0;
	cc->stack[0].dir = (struct csrdirectory *)p;
	cc->stack[0].index = 0;
}

struct csrreg *
crom_get(struct crom_context *cc)
{
	struct crom_ptr *ptr;

	ptr = &cc->stack[cc->depth];
	return (&ptr->dir->entry[ptr->index]);
}

void
crom_next(struct crom_context *cc)
{
	struct crom_ptr *ptr;
	struct csrreg *reg;

	if (cc->depth < 0)
		return;
	reg = crom_get(cc);
	if ((reg->key & CSRTYPE_MASK) == CSRTYPE_D) {
		cc->depth ++;
		if (cc->depth > CROM_MAX_DEPTH) {
			printf("crom_next: too deep\n");
			cc->depth --;
			goto again;
		}
		cc->stack[cc->depth].dir = (struct csrdirectory *)
							(reg + reg->val);
		cc->stack[cc->depth].index = 0;
		return;
	}
again:
	ptr = &cc->stack[cc->depth];
	ptr->index ++;
	if (ptr->index < ptr->dir->crc_len)
		return;
	if (cc->depth > 0) {
		cc->depth--;
		goto again;
	}
	/* no more data */
	cc->depth = -1;
}


struct csrreg *
crom_search_key(struct crom_context *cc, u_int8_t key)
{
	struct csrreg *reg;

	while(cc->depth >= 0) {
		reg = crom_get(cc);
		if (reg->key == key)
			return reg;
		crom_next(cc);
	}
	return NULL;
}

void
crom_parse_text(struct crom_context *cc, char *buf, int len)
{
	struct csrreg *reg;
	struct csrtext *textleaf;
	u_int32_t *bp;
	int i, qlen;
	static char *nullstr = "(null)";

	reg = crom_get(cc);
	if (reg->key != CROM_TEXTLEAF) {
		strncpy(buf, nullstr, len);
		return;
	}
	textleaf = (struct csrtext *)(reg + reg->val);

	/* XXX should check spec and type */

	bp = (u_int32_t *)&buf[0];
	qlen = textleaf->crc_len - 2;
	if (len < qlen * 4)
		qlen = len/4;
	for (i = 0; i < qlen; i ++)
		*bp++ = ntohl(textleaf->text[i]);
	/* make sure to terminate the string */
	if (len <= qlen * 4)
		buf[len - 1] = 0;
	else
		buf[qlen * 4] = 0;
}

u_int16_t
crom_crc(u_int32_t *ptr, int len)
{
	int i, shift;
	u_int32_t data, sum, crc = 0;

	for (i = 0; i < len; i++) {
		data = ptr[i];
		for (shift = 28; shift >= 0; shift -= 4) {
			sum = ((crc >> 12) ^ (data >> shift)) & 0xf;
			crc = (crc << 4) ^ (sum << 12) ^ (sum << 5) ^ sum;
		}
		crc &= 0xffff;
	}
	return((u_int16_t) crc);
}

#ifndef _KERNEL
char *
crom_desc(struct crom_context *cc, char *buf, int len)
{
	struct csrreg *reg;
	struct csrdirectory *dir;
	char *desc;

	reg = crom_get(cc);
	switch (reg->key & CSRTYPE_MASK) {
	case CSRTYPE_I:
		snprintf(buf, len, "%d", reg->val);
		break;
	case CSRTYPE_L:
	case CSRTYPE_C:
		snprintf(buf, len, "offset=0x%04x(%d)", reg->val, reg->val);
		break;
	case CSRTYPE_D:
		dir = (struct csrdirectory *) (reg + reg->val);
		snprintf(buf, len, "len=0x%04x(%d) crc=0x%04x",
			dir->crc_len, dir->crc_len, dir->crc);
	}
	switch (reg->key) {
	case 0x03:
		desc = "module_vendor_ID";
		break;
	case 0x04:
		desc = "hardware_version";
		break;
	case 0x0c:
		desc = "node_capabilities";
		break;
	case 0x12:
		desc = "unit_spec_ID";
		break;
	case 0x13:
		desc = "unit_sw_version";
		break;
	case 0x14:
		desc = "logical_unit_number";
		break;
	case 0x17:
		desc = "model_ID";
		break;
	case 0x38:
		desc = "command_set_spec_ID";
		break;
	case 0x39:
		desc = "command_set";
		break;
	case 0x3a:
		desc = "unit_characteristics";
		break;
	case 0x3b:
		desc = "command_set_revision";
		break;
	case 0x3c:
		desc = "firmware_revision";
		break;
	case 0x3d:
		desc = "reconnect_timeout";
		break;
	case 0x54:
		desc = "management_agent";
		break;
	case 0x81:
		desc = "text_leaf";
		crom_parse_text(cc, buf, len);
		break;
	case 0xd1:
		desc = "unit_directory";
		break;
	case 0xd4:
		desc = "logical_unit_directory";
		break;
	default:
		desc = "unknown";
	}
	return desc;
}
#endif
