/*
 * Copyright (c) 2008, 2009 QLogic Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _QIB_TRACE_H
#define _QIB_TRACE_H

struct qib_evt_buf;

extern struct qib_evt_buf *qib_trace_buf;

void qib_trace_put8(struct qib_evt_buf *buf,
		   int cpu, u64 tsc, u32 dbgmask, u8);

void qib_trace_put16(struct qib_evt_buf *buf,
		     int cpu, u64 tsc, u32 dbgmask, u16);

void qib_trace_put32(struct qib_evt_buf *buf,
		     int cpu, u64 tsc, u32 dbgmask, u32 val);

void qib_trace_put64(struct qib_evt_buf *buf,
		     int cpu, u64 tsc, u32 dbgmask, u64 val);

void qib_trace_putblob(struct qib_evt_buf *buf,
		       int cpu, u64 tsc, u32 dbgmask, void *blob, u16 len);

void qib_trace_putstr(struct qib_evt_buf *buf,
		      int cpu, u64 tsc, u32 dbgmask, const char *str);

void qib_trace_vputstr(struct qib_evt_buf *buf,
		       int cpu, u64 tsc, u32 dbgmask, const char *fmt, ...)
		       __attribute__ ((format (printf, 5, 6)));

int __init qib_trace_init(void);
void qib_trace_fini(void);

#endif
