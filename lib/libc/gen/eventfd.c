/*-
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2005-2020 Rich Felker, et al.
 * Copyright (c) 2020 Greg V
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/eventfd.h>
#include <sys/specialfd.h>
#include <unistd.h>
#include "un-namespace.h"
#include "libc_private.h"

int eventfd(unsigned int initval, int flags)
{
	struct specialfd_eventfd args;

	args.initval = initval;
	args.flags = flags;
	return (__sys___specialfd(SPECIALFD_EVENTFD, &args, sizeof(args)));
}

int eventfd_read(int fd, eventfd_t *value)
{
	return (sizeof(*value) == _read(fd, value, sizeof(*value)) ? 0 : -1);
}

int eventfd_write(int fd, eventfd_t value)
{
	return (sizeof(value) == _write(fd, &value, sizeof(value)) ? 0 : -1);
}
