/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include <sys/param.h>
#include <sys/types.h>

struct thread;

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/ofw/openfirm.h>
#include <gdb/gdb.h>

#include "opal.h"

static gdb_probe_f uart_opal_dbg_probe;
static gdb_init_f uart_opal_dbg_init;
static gdb_term_f uart_opal_dbg_term;
static gdb_getc_f uart_opal_dbg_getc;
static gdb_putc_f uart_opal_dbg_putc;

GDB_DBGPORT(uart_opal, uart_opal_dbg_probe,
    uart_opal_dbg_init, uart_opal_dbg_term,
    uart_opal_dbg_getc, uart_opal_dbg_putc);

static int64_t termnum;

static int
uart_opal_dbg_probe(void)
{
	char buf[64];
	cell_t reg;
	phandle_t dev;

	if (!getenv_string("hw.uart.dbgport", buf, sizeof(buf)))
		return (-1);
	if ((dev = OF_finddevice(buf)) == -1)
		return (-1);

	if (OF_getprop(dev, "device_type", buf, sizeof(buf)) == -1)
		return (-1);
	if (strcmp(buf, "serial") != 0)
		return (-1);

	if (OF_getprop(dev, "compatible", buf, sizeof(buf)) == -1)
		return (-1);
	if (strcmp(buf, "ibm,opal-console-raw") != 0)
		return (-1);

	reg = ~0U;
	OF_getencprop(dev, "reg", &reg, sizeof(reg));
	if (reg == ~0U)
		return (-1);
	termnum = reg;

	return (0);
}

static void
uart_opal_dbg_init(void)
{
}

static void
uart_opal_dbg_term(void)
{
}

static int
uart_opal_dbg_getc(void)
{
	char c;
	int err;
	int64_t len;
	uint64_t lenp, bufp;

	len = 1;
	if (pmap_bootstrapped) {
		lenp = vtophys(&len);
		bufp = vtophys(&c);
	} else {
		lenp = (uint64_t)&len;
		bufp = (uint64_t)&c;
	}

	err = opal_call(OPAL_CONSOLE_READ, termnum, lenp, bufp);
	if (err != OPAL_SUCCESS || len != 1)
		return (-1);

	opal_call(OPAL_POLL_EVENTS, NULL);

	return (c);
}

static void
uart_opal_dbg_putc(int c)
{
	char ch;
	int err;
	int64_t len;
	uint64_t lenp, bufp;

	ch = (unsigned char)c;
	len = 1;
	if (pmap_bootstrapped) {
		lenp = vtophys(&len);
		bufp = vtophys(&ch);
	} else {
		lenp = (uint64_t)&len;
		bufp = (uint64_t)&ch;
	}

	for (;;) {
		err = opal_call(OPAL_CONSOLE_WRITE, termnum, lenp, bufp);
		/* Clear FIFO if needed. */
		if (err == OPAL_BUSY_EVENT)
			opal_call(OPAL_POLL_EVENTS, NULL);
		else
			/* break on success or unrecoverable errors */
			break;
	}
	DELAY(100);
}
