/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2019 Leandro Lupori
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/linker_set.h>

#include <dev/ofw/openfirm.h>
#include <gdb/gdb.h>

#include "phyp-hvcall.h"

static gdb_probe_f uart_phyp_dbg_probe;
static gdb_init_f uart_phyp_dbg_init;
static gdb_term_f uart_phyp_dbg_term;
static gdb_getc_f uart_phyp_dbg_getc;
static gdb_putc_f uart_phyp_dbg_putc;

GDB_DBGPORT(uart_phyp, uart_phyp_dbg_probe,
    uart_phyp_dbg_init, uart_phyp_dbg_term,
    uart_phyp_dbg_getc, uart_phyp_dbg_putc);

static struct uart_phyp_dbgport {
	cell_t vtermid;
	union {
		uint64_t u64[2];
		char str[16];
	} inbuf;
	uint64_t inbuflen;
} dbgport;

static int
uart_phyp_dbg_probe(void)
{
	char buf[64];
	cell_t reg;
	phandle_t vty;

	if (!getenv_string("hw.uart_phyp.dbgport", buf, sizeof(buf)))
		return (-1);

	if ((vty = OF_finddevice(buf)) == -1)
		return (-1);

	if (OF_getprop(vty, "name", buf, sizeof(buf)) <= 0)
		return (-1);
	if (strcmp(buf, "vty") != 0)
		return (-1);

	if (OF_getprop(vty, "device_type", buf, sizeof(buf)) == -1)
		return (-1);
	if (strcmp(buf, "serial") != 0)
		return (-1);

	if (OF_getprop(vty, "compatible", buf, sizeof(buf)) <= 0)
		return (-1);
	if (strcmp(buf, "hvterm1") != 0)
		return (-1);

	reg = ~0U;
	OF_getencprop(vty, "reg", &reg, sizeof(reg));
	if (reg == ~0U)
		return (-1);

	dbgport.vtermid = reg;
	dbgport.inbuflen = 0;

	return (0);
}

static void
uart_phyp_dbg_init(void)
{
}

static void
uart_phyp_dbg_term(void)
{
}

static int
uart_phyp_dbg_getc(void)
{
	int c, err, next;

	if (dbgport.inbuflen == 0) {
		err = phyp_pft_hcall(H_GET_TERM_CHAR, dbgport.vtermid,
		    0, 0, 0, &dbgport.inbuflen, &dbgport.inbuf.u64[0],
		    &dbgport.inbuf.u64[1]);
		if (err != H_SUCCESS)
			return (-1);
	}

	if (dbgport.inbuflen == 0)
		return (-1);

	c = dbgport.inbuf.str[0];
	dbgport.inbuflen--;

	if (dbgport.inbuflen == 0)
		return (c);

	/*
	 * Since version 2.11.0, QEMU became bug-compatible
	 * with PowerVM's vty, by inserting a \0 after every \r.
	 * Filter it here.
	 */
	next = 1;
	if (c == '\r' && dbgport.inbuf.str[next] == '\0') {
		next++;
		dbgport.inbuflen--;
	}

	if (dbgport.inbuflen > 0)
		memmove(&dbgport.inbuf.str[0], &dbgport.inbuf.str[next],
		    dbgport.inbuflen);

	return (c);
}

static void
uart_phyp_dbg_putc(int c)
{
	int	err;

	union {
		uint64_t u64;
		unsigned char bytes[8];
	} cbuf;

	cbuf.bytes[0] = (unsigned char)c;

	do {
		err = phyp_hcall(H_PUT_TERM_CHAR, dbgport.vtermid, 1,
		    cbuf.u64, 0);
		DELAY(100);
	} while (err == H_BUSY);
}

