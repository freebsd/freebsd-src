/*-
 * Copyright (c) 2002 by Thomas Moestl <tmm@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ioctl.h>

#include <dev/ofw/openfirmio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "pathnames.h"
#include "ofw_util.h"

/* Constants controlling the layout of the output. */
#define	LVLINDENT	2
#define	NAMEINDENT	2
#define	DUMPINDENT	4
#define	CHARSPERLINE	60
#define	BYTESPERLINE	(CHARSPERLINE / 3)

/* Default space reserved for properties. */
#define	PROPBUFLEN	8192

#define	OFW_IOCTL(fd, cmd, val)	do {					\
	if (ioctl(fd, cmd, val) == -1)					\
		err(1, "ioctl(..., " #cmd ", ...) failed");		\
} while (0)

int
ofw_open(void)
{
	int fd;

	if ((fd = open(PATH_DEV_OPENFIRM, O_RDONLY)) == -1)
		err(1, "could not open " PATH_DEV_OPENFIRM);
	return (fd);
}

void
ofw_close(int fd)
{

	close(fd);
}

phandle_t
ofw_root(int fd)
{

	return (ofw_peer(fd, 0));
}

phandle_t
ofw_peer(int fd, phandle_t node)
{
	phandle_t rv;

	rv = node;
	OFW_IOCTL(fd, OFIOCGETNEXT, &rv);
	return (rv);
}

phandle_t
ofw_child(int fd, phandle_t node)
{
	phandle_t rv;

	rv = node;
	OFW_IOCTL(fd, OFIOCGETCHILD, &rv);
	return (rv);
}

phandle_t
ofw_finddevice(int fd, char *name)
{
	struct ofiocdesc d;

	d.of_nodeid = 0;
	d.of_namelen = strlen(name);
	d.of_name = name;
	d.of_buflen = 0;
	d.of_buf = NULL;
	if (ioctl(fd, OFIOCFINDDEVICE, &d) == -1) {
		if (errno == ENOENT)
			err(2, "Node '%s' not found", name);
		else
			err(1, "ioctl(..., OFIOCFINDDEVICE, ...) failed");
	}
	return (d.of_nodeid);
}

int
ofw_firstprop(int fd, phandle_t node, char *buf, int buflen)
{

	return (ofw_nextprop(fd, node, NULL, buf, buflen));
}

int
ofw_nextprop(int fd, phandle_t node, char *prev, char *buf, int buflen)
{
	struct ofiocdesc d;

	d.of_nodeid = node;
	d.of_namelen = prev != NULL ? strlen(prev) : 0;
	d.of_name = prev;
	d.of_buflen = buflen;
	d.of_buf = buf;
	if (ioctl(fd, OFIOCNEXTPROP, &d) == -1) {
		if (errno == ENOENT)
			return (0);
		else
			err(1, "ioctl(..., OFIOCNEXTPROP, ...) failed");
	}
	return (d.of_buflen);
}

static void *
ofw_malloc(int size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		err(1, "malloc() failed");
	return (p);
}

int
ofw_getprop(int fd, phandle_t node, const char *name, void *buf, int buflen)
{
	struct ofiocdesc d;

	d.of_nodeid = node;
	d.of_namelen = strlen(name);
	d.of_name = name;
	d.of_buflen = buflen;
	d.of_buf = buf;
	OFW_IOCTL(fd, OFIOCGET, &d);
	return (d.of_buflen);
}

int
ofw_getproplen(int fd, phandle_t node, const char *name)
{
	struct ofiocdesc d;

	d.of_nodeid = node;
	d.of_namelen = strlen(name);
	d.of_name = name;
	OFW_IOCTL(fd, OFIOCGETPROPLEN, &d);
	return (d.of_buflen);
}

int
ofw_getprop_alloc(int fd, phandle_t node, const char *name, void **buf,
    int *buflen, int reserve)
{
	struct ofiocdesc d;
	int len, rv;

	do {
		len = ofw_getproplen(fd, node, name);
		if (len < 0)
			return (len);
		if (*buflen < len + reserve) {
			if (*buf != NULL)
				free(*buf);
			*buflen = len + reserve + PROPBUFLEN;
			*buf = ofw_malloc(*buflen);
		}
		d.of_nodeid = node;
		d.of_namelen = strlen(name);
		d.of_name = name;
		d.of_buflen = *buflen - reserve;
		d.of_buf = *buf;
		rv = ioctl(fd, OFIOCGET, &d);
	} while (rv == -1 && errno == ENOMEM);
	if (rv == -1)
		err(1, "ioctl(..., OFIOCGET, ...) failed");
	return (d.of_buflen);
}

static void
ofw_indent(int level)
{
	int i;

	for (i = 0; i < level; i++)
		putchar(' ');
}

static void
ofw_dump_properties(int fd, phandle_t n, int level, char *pmatch, int raw,
    int str)
{
	static char *pbuf;
	static char *visbuf;
	static char printbuf[CHARSPERLINE + 1];
	static int pblen, vblen;
	char prop[32];
	int nlen, len, i, j, max, vlen;
	unsigned int b;

	for (nlen = ofw_firstprop(fd, n, prop, sizeof(prop)); nlen != 0;
	     nlen = ofw_nextprop(fd, n, prop, prop, sizeof(prop))) {
		if (pmatch != NULL && strcmp(pmatch, prop) != 0)
			continue;
		len = ofw_getprop_alloc(fd, n, prop, (void **)&pbuf, &pblen, 1);
		if (len < 0)
			continue;
		if (raw)
			write(STDOUT_FILENO, pbuf, len);
		else if (str) {
			pbuf[len] = '\0';
			printf("%s\n", pbuf);
		} else {
			ofw_indent(level * LVLINDENT + NAMEINDENT);
			printf("%s:\n", prop);
			/* Print in hex. */
			for (i = 0; i < len; i += BYTESPERLINE) {
				max = len - i;
				max = max > BYTESPERLINE ? BYTESPERLINE : max;
				ofw_indent(level * LVLINDENT + DUMPINDENT);
				for (j = 0; j < max; j++) {
					b = (unsigned char)pbuf[i + j];
					printf("%02x ", b);
				}
				printf("\n");
			}
			/*
			 * strvis() and print if it looks like it is
			 * zero-terminated.
			 */
			if (pbuf[len - 1] == '\0' &&
			    strlen(pbuf) == (unsigned)len - 1) {
				if (vblen < (len - 1) * 4 + 1) {
					if (visbuf != NULL)
						free(visbuf);
					vblen = (PROPBUFLEN + len) * 4 + 1;
					visbuf = ofw_malloc(vblen);
				}
				vlen = strvis(visbuf, pbuf, VIS_TAB | VIS_NL);
				for (i = 0; i < vlen; i += CHARSPERLINE) {
					ofw_indent(level * LVLINDENT +
					    DUMPINDENT);
					strlcpy(printbuf, &visbuf[i],
					    sizeof(printbuf));
					printf("'%s'\n", printbuf);
				}
			}
		}
	}
}

static void
ofw_dump_node(int fd, phandle_t n, int level, int rec, int prop, char *pmatch,
    int raw, int str)
{
	static char *nbuf;
	static int nblen = 0;
	int plen;
	phandle_t c;

	if (!(raw || str)) {
		ofw_indent(level * LVLINDENT);
		printf("Node %#lx", (unsigned long)n);
		plen = ofw_getprop_alloc(fd, n, "name", (void **)&nbuf,
		    &nblen, 1);
		if (plen > 0) {
			nbuf[plen] = '\0';
			printf(": %s\n", nbuf);
		} else
			putchar('\n');
	}
	if (prop)
		ofw_dump_properties(fd, n, level, pmatch, raw, str);
	if (rec) {
		for (c = ofw_child(fd, n); c != 0; c = ofw_peer(fd, c)) {
			ofw_dump_node(fd, c, level + 1, rec, prop, pmatch,
			    raw, str);
		}
	}
}

void
ofw_dump(int fd, char *start, int rec, int prop, char *pmatch, int raw, int str)
{
	phandle_t n;

	n = start == NULL ? ofw_root(fd) : ofw_finddevice(fd, start);
	ofw_dump_node(fd, n, 0, rec, prop, pmatch, raw, str);
}

