/*
 * Copyright (c) 2004-2008 Voltaire Inc.  All rights reserved.
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
 *
 */

#define _GNU_SOURCE

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <endian.h>
#include <byteswap.h>
#include <sys/poll.h>
#include <syslog.h>
#include <netinet/in.h>

#include <common.h>

void
ibwarn(const char * const fn, char *msg, ...)
{
	char buf[512];
	va_list va;
	int n;

	va_start(va, msg);
	n = vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);

	printf("ibwarn: [%d] %s: %s\n", getpid(), fn, buf);
}

void
ibpanic(const char * const fn, char *msg, ...)
{
	char buf[512];
	va_list va;
	int n;

	va_start(va, msg);
	n = vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);

	printf("ibpanic: [%d] %s: %s: (%m)\n", getpid(), fn, buf);
	syslog(LOG_ALERT, "ibpanic: [%d] %s: %s: (%m)\n", getpid(), fn, buf);

	exit(-1);
}

void
logmsg(const char * const fn, char *msg, ...)
{
	char buf[512];
	va_list va;
	int n;

	va_start(va, msg);
	n = vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);

	syslog(LOG_ALERT, "[%d] %s: %s: (%m)\n", getpid(), fn, buf);
}

void
xdump(FILE *file, char *msg, void *p, int size)
{
#define HEX(x)  ((x) < 10 ? '0' + (x) : 'a' + ((x) -10))
        uint8_t *cp = p;
        int i;

	if (msg)
		fputs(msg, file);

        for (i = 0; i < size;) {
                fputc(HEX(*cp >> 4), file);
                fputc(HEX(*cp & 0xf), file);
                if (++i >= size)
                        break;
                fputc(HEX(cp[1] >> 4), file);
                fputc(HEX(cp[1] & 0xf), file);
                if ((++i) % 16)
                        fputc(' ', file);
                else
                        fputc('\n', file);
                cp += 2;
        }
        if (i % 16) {
                fputc('\n', file);
        }
}
