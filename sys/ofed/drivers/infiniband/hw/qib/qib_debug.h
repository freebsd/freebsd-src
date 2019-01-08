/*
 * Copyright (c) 2006, 2007, 2009 QLogic Corporation. All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc. All rights reserved.
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

#ifndef _QIB_DEBUG_H
#define _QIB_DEBUG_H

#include "qib_trace.h"

#ifndef _QIB_DEBUGGING  /* debugging enabled or not */
#define _QIB_DEBUGGING 1
#endif

#if _QIB_DEBUGGING

/*
 * Mask values for debugging.  The scheme allows us to compile out any
 * of the debug tracing stuff, and if compiled in, to enable or disable
 * dynamically.  This can be set at modprobe time also:
 *      modprobe qlogic_ib.ko qlogic_ib_debug=7
 */

#define __QIB_INFO        0x1   /* logging of dev_info and dev_err */
#define __QIB_DBG         0x2   /* generic debug */
/* leave some low verbosity spots open */
#define __QIB_RVPKTDBG    0x10  /* verbose pktrcv debug */
#define __QIB_INITDBG     0x20  /* init-level debug */
#define __QIB_VERBDBG     0x40  /* very verbose debug */
#define __QIB_PKTDBG      0x80  /* print packet data */
#define __QIB_PROCDBG     0x100 /* process init/exit debug */
#define __QIB_MMDBG       0x200 /* mmap, etc debug */
#define __QIB_ERRPKTDBG   0x400 /* packet error debugging */
#define __QIB_SDMADBG     0x800 /* Send DMA */
#define __QIB_VPKTDBG     0x1000 /* Dump IB contents being copied to send buf */
#define __QIB_LINKVERBDBG 0x200000      /* very verbose linkchange debug */

#else                           /* _QIB_DEBUGGING */

/*
 * define all of these even with debugging off, for the few places that do
 * if(qlogic_ib_debug & _QIB_xyzzy), but in a way that will make the
 * compiler eliminate the code
 */

#define __QIB_DBG       0x0     /* generic debug */
#define __QIB_VERBDBG   0x0     /* very verbose debug */
#define __QIB_PKTDBG    0x0     /* print packet data */
#define __QIB_PROCDBG   0x0      /* process init/exit debug */
#define __QIB_MMDBG     0x0     /* mmap, etc debug */
#define __QIB_ERRPKTDBG 0x0     /* packet error debugging */
#define __QIB_SDMADBG   0x0     /* Send DMA */
#define __QIB_LINKVERBDBG 0x0   /* very verbose linkchange debug */

#endif                          /* _QIB_DEBUGGING */

#define __QIB_VERBOSEDBG __QIB_VERBDBG

#define qib_log(which, fmt, ...)					  \
	do {								  \
		if (qib_trace_buf) {					  \
			int cpu = smp_processor_id();                     \
			cycles_t now = get_cycles();                      \
			qib_trace_vputstr(qib_trace_buf, cpu, now, which, \
					  "%s " fmt, __func__, ##__VA_ARGS__); \
		}							  \
	} while (0)

#endif                          /* _QIB_DEBUG_H */
