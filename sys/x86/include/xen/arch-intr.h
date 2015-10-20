/*-
 * SPDX-License-Identifier: MIT OR GPL-2.0-only
 *
 * Copyright © 2015 Julien Grall
 * Copyright © 2021 Elliott Mitchell
 *
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef	_MACHINE__XEN_ARCH_INTR_H_
#define	_MACHINE__XEN_ARCH_INTR_H_

#include <x86/intr_machdep.h>

typedef struct {
	struct intsrc		intsrc;		/* @TOP -> *xen_arch_isrc */
	u_int			vector;		/* Global isrc vector number */
} xen_arch_isrc_t;

#include <dev/xen/bus/intr-internal.h>

#endif	/* _MACHINE__XEN_ARCH_INTR_H_ */
