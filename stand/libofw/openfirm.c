/*	$NetBSD: Locore.c,v 1.7 2000/08/20 07:04:59 tsubai Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
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
/*-
 * Copyright (C) 2000 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
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
#include <sys/endian.h>

#include <machine/stdarg.h>

#include <stand.h>

#include "openfirm.h"

int (*openfirmware)(void *);

phandle_t chosen;
ihandle_t mmu;
ihandle_t memory;
int	  real_mode = 0;

#define IN(x)		htobe32((cell_t)x)
#define OUT(x)		be32toh(x)
#define SETUP(a, b, c, d)		\
	a.name = IN( (b) );		\
	a.nargs = IN( (c) );		\
	a.nreturns = IN( (d) );

/* Initialiser */

void
OF_init(int (*openfirm)(void *))
{
	phandle_t options;
	char	  mode[sizeof("true")];

	openfirmware = openfirm;

	if ((chosen = OF_finddevice("/chosen")) == -1)
		OF_exit();
	if (OF_getprop(chosen, "memory", &memory, sizeof(memory)) == -1) {
		memory = OF_open("/memory");
		if (memory == -1)
			memory = OF_open("/memory@0");
		if (memory == -1)
			OF_exit();
	}
	if (OF_getprop(chosen, "mmu", &mmu, sizeof(mmu)) == -1)
		OF_exit();

	/*
	 * Check if we run in real mode. If so, we do not need to map
	 * memory later on.
	 */
	options = OF_finddevice("/options");
	if (OF_getprop(options, "real-mode?", mode, sizeof(mode)) > 0 &&
	    strcmp(mode, "true") == 0)
		real_mode = 1;
}

/*
 * Generic functions
 */

/* Test to see if a service exists. */
int
OF_test(char *name)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t service;
		cell_t missing;
	} args = {};
	SETUP(args, "test", 1, 1);

	args.service = IN(name);
	if (openfirmware(&args) == -1)
		return (-1);
	return (OUT(args.missing));
}

/* Return firmware millisecond count. */
int
OF_milliseconds(void)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t ms;
	} args = {};
	SETUP(args, "milliseconds", 0, 1);

	openfirmware(&args);
	return (OUT(args.ms));
}

/*
 * Device tree functions
 */

/* Return the next sibling of this node or 0. */
phandle_t
OF_peer(phandle_t node)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t node;
		cell_t next;
	} args = {};
	SETUP(args, "peer", 1, 1);

	args.node = node;
	if (openfirmware(&args) == -1)
		return (-1);
	return (args.next);
}

/* Return the first child of this node or 0. */
phandle_t
OF_child(phandle_t node)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t node;
		cell_t child;
	} args = {};
	SETUP(args, "child", 1, 1);

	args.node = node;
	if (openfirmware(&args) == -1)
		return (-1);
	return (args.child);
}

/* Return the parent of this node or 0. */
phandle_t
OF_parent(phandle_t node)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t node;
		cell_t parent;
	} args = {};
	SETUP(args, "parent", 1, 1);

	args.node = node;
	if (openfirmware(&args) == -1)
		return (-1);
	return (args.parent);
}

/* Return the package handle that corresponds to an instance handle. */
phandle_t
OF_instance_to_package(ihandle_t instance)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t package;
	} args = {};
	SETUP(args, "instance-to-package", 1, 1);

	args.instance = instance;
	if (openfirmware(&args) == -1)
		return (-1);
	return (args.package);
}

/* Get the length of a property of a package. */
int
OF_getproplen(phandle_t package, const char *propname)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t package;
		cell_t propname;
		cell_t proplen;
	} args = {};
	SETUP(args, "getproplen", 2, 1);

	args.package = package;
	args.propname = IN(propname);
	if (openfirmware(&args) == -1)
		return (-1);
	return (OUT(args.proplen));
}

/* Get the value of a property of a package. */
int
OF_getprop(phandle_t package, const char *propname, void *buf, int buflen)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t package;
		cell_t propname;
		cell_t buf;
		cell_t buflen;
		cell_t size;
	} args = {};
	SETUP(args, "getprop", 4, 1);

	args.package = package;
	args.propname = IN(propname);
	args.buf = IN(buf);
	args.buflen = IN(buflen);
	if (openfirmware(&args) == -1)
		return (-1);
	return (OUT(args.size));
}

/* Decode a binary property from a package. */
int
OF_getencprop(phandle_t package, const char *propname, cell_t *buf, int buflen)
{
	int retval, i;
	retval = OF_getprop(package, propname, buf, buflen);
	if (retval == -1)
		return (retval);

	for (i = 0; i < buflen/4; i++)
		buf[i] = be32toh((uint32_t)buf[i]);

	return (retval);
}

/* Get the next property of a package. */
int
OF_nextprop(phandle_t package, const char *previous, char *buf)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t package;
		cell_t previous;
		cell_t buf;
		cell_t flag;
	} args = {};
	SETUP(args, "nextprop", 3, 1);

	args.package = package;
	args.previous = IN(previous);
	args.buf = IN(buf);
	if (openfirmware(&args) == -1)
		return (-1);
	return (OUT(args.flag));
}

/* Set the value of a property of a package. */
/* XXX Has a bug on FirePower */
int
OF_setprop(phandle_t package, const char *propname, void *buf, int len)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t package;
		cell_t propname;
		cell_t buf;
		cell_t len;
		cell_t size;
	} args = {};
	SETUP(args, "setprop", 4, 1);

	args.package = package;
	args.propname = IN(propname);
	args.buf = IN(buf);
	args.len = IN(len);
	if (openfirmware(&args) == -1)
		return (-1);
	return (OUT(args.size));
}

/* Convert a device specifier to a fully qualified pathname. */
int
OF_canon(const char *device, char *buf, int len)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t device;
		cell_t buf;
		cell_t len;
		cell_t size;
	} args = {};
	SETUP(args, "canon", 3, 1);

	args.device = IN(device);
	args.buf = IN(buf);
	args.len = IN(len);
	if (openfirmware(&args) == -1)
		return (-1);
	return (OUT(args.size));
}

/* Return a package handle for the specified device. */
phandle_t
OF_finddevice(const char *device)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t device;
		cell_t package;
	} args = {};
	SETUP(args, "finddevice", 1, 1);

	args.device = IN(device);
	if (openfirmware(&args) == -1)
		return (-1);
	return (args.package);
}

/* Return the fully qualified pathname corresponding to an instance. */
int
OF_instance_to_path(ihandle_t instance, char *buf, int len)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t buf;
		cell_t len;
		cell_t size;
	} args = {};
	SETUP(args, "instance-to-path", 3, 1);

	args.instance = instance;
	args.buf = IN(buf);
	args.len = IN(len);
	if (openfirmware(&args) == -1)
		return (-1);
	return (OUT(args.size));
}

/* Return the fully qualified pathname corresponding to a package. */
int
OF_package_to_path(phandle_t package, char *buf, int len)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t package;
		cell_t buf;
		cell_t len;
		cell_t size;
	} args = {};
	SETUP(args, "package-to-path", 3, 1);

	args.package = package;
	args.buf = IN(buf);
	args.len = IN(len);
	if (openfirmware(&args) == -1)
		return (-1);
	return (OUT(args.size));
}

/*  Call the method in the scope of a given instance. */
int
OF_call_method(char *method, ihandle_t instance, int nargs, int nreturns, ...)
{
	va_list ap;
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t instance;
		cell_t args_n_results[12];
	} args = {};
	SETUP(args, "call-method", nargs + 2, nreturns + 1);
	cell_t *cp;
	int n;

	if (nargs > 6)
		return (-1);
	args.method = IN(method);
	args.instance = instance;
	va_start(ap, nreturns);
	for (cp = (cell_t *)(args.args_n_results + (n = nargs)); --n >= 0;)
		*--cp = IN(va_arg(ap, cell_t));
	if (openfirmware(&args) == -1)
		return (-1);
	if (args.args_n_results[nargs])
		return (OUT(args.args_n_results[nargs]));
	/* XXX what if ihandles or phandles are returned */
	for (cp = (cell_t *)(args.args_n_results + nargs +
	    (n = be32toh(args.nreturns))); --n > 0;)
		*va_arg(ap, cell_t *) = OUT(*--cp);
	va_end(ap);
	return (0);
}

/*
 * Device I/O functions
 */

/* Open an instance for a device. */
ihandle_t
OF_open(char *device)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t device;
		cell_t instance;
	} args = {};
	SETUP(args, "open", 1, 1);

	args.device = IN(device);
	if (openfirmware(&args) == -1 || args.instance == 0) {
		return (-1);
	}
	return (args.instance);
}

/* Close an instance. */
void
OF_close(ihandle_t instance)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
	} args = {};
	SETUP(args, "close", 1, 0);

	args.instance = instance;
	openfirmware(&args);
}

/* Read from an instance. */
int
OF_read(ihandle_t instance, void *addr, int len)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t addr;
		cell_t len;
		cell_t actual;
	} args = {};
	SETUP(args, "read", 3, 1);

	args.instance = instance;
	args.addr = IN(addr);
	args.len = IN(len);

#if defined(OPENFIRM_DEBUG)
	printf("OF_read: called with instance=%08x, addr=%p, len=%d\n",
	    instance, addr, len);
#endif

	if (openfirmware(&args) == -1)
		return (-1);

#if defined(OPENFIRM_DEBUG)
	printf("OF_read: returning instance=%d, addr=%p, len=%d, actual=%d\n",
	    args.instance, OUT(args.addr), OUT(args.len), OUT(args.actual));
#endif

	return (OUT(args.actual));
}

/* Write to an instance. */
int
OF_write(ihandle_t instance, void *addr, int len)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t addr;
		cell_t len;
		cell_t actual;
	} args = {};
	SETUP(args, "write", 3, 1);

	args.instance = instance;
	args.addr = IN(addr);
	args.len = IN(len);
	if (openfirmware(&args) == -1)
		return (-1);
	return (OUT(args.actual));
}

/* Seek to a position. */
int
OF_seek(ihandle_t instance, uint64_t pos)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t poshi;
		cell_t poslo;
		cell_t status;
	} args = {};
	SETUP(args, "seek", 3, 1);

	args.instance = instance;
	args.poshi = IN(((uint64_t)pos >> 32));
	args.poslo = IN(pos);
	if (openfirmware(&args) == -1)
		return (-1);
	return (OUT(args.status));
}

/* Blocks. */
unsigned int
OF_blocks(ihandle_t instance)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t result;
		cell_t blocks;
	} args = {};
	SETUP(args, "#blocks", 2, 1);

	args.instance = instance;
	if (openfirmware(&args) == -1)
		return ((unsigned int)-1);
	return (OUT(args.blocks));
}

/* Block size. */
int
OF_block_size(ihandle_t instance)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t result;
		cell_t size;
	} args = {};
	SETUP(args, "block-size", 2, 1);

	args.instance = instance;
	if (openfirmware(&args) == -1)
		return (512);
	return (OUT(args.size));
}

/* 
 * Memory functions
 */

/* Claim an area of memory. */
void *
OF_claim(void *virt, u_int size, u_int align)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t virt;
		cell_t size;
		cell_t align;
		cell_t baseaddr;
	} args = {};
	SETUP(args, "claim", 3, 1);

	args.virt = IN(virt);
	args.size = IN(size);
	args.align = IN(align);
	if (openfirmware(&args) == -1)
		return ((void *)-1);
	return ((void *)OUT(args.baseaddr));
}

/* Release an area of memory. */
void
OF_release(void *virt, u_int size)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t virt;
		cell_t size;
	} args = {};
	SETUP(args, "release", 2, 0);

	args.virt = IN(virt);
	args.size = IN(size);
	openfirmware(&args);
}

/*
 * Control transfer functions
 */

/* Reset the system and call "boot <bootspec>". */
void
OF_boot(char *bootspec)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t bootspec;
	} args = {};
	SETUP(args, "boot", 1, 0);

	args.bootspec = IN(bootspec);
	openfirmware(&args);
	for (;;)			/* just in case */
		;
}

/* Suspend and drop back to the Open Firmware interface. */
void
OF_enter(void)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
	} args = {};
	SETUP(args, "enter", 0, 0);

	openfirmware(&args);
	/* We may come back. */
}

/* Shut down and drop back to the Open Firmware interface. */
void
OF_exit(void)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
	} args = {};
	SETUP(args, "exit", 0, 0);

	openfirmware(&args);
	for (;;)			/* just in case */
		;
}

void
OF_quiesce(void)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
	} args = {};
	SETUP(args, "quiesce", 0, 0);

	openfirmware(&args);
}

/* Free <size> bytes starting at <virt>, then call <entry> with <arg>. */
#if 0
void
OF_chain(void *virt, u_int size, void (*entry)(), void *arg, u_int len)
{
	static struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t virt;
		cell_t size;
		cell_t entry;
		cell_t arg;
		cell_t len;
	} args = {};
	SETUP(args, "chain", 5, 0);

	args.virt = IN(virt);
	args.size = IN(size);
	args.entry = IN(entry);
	args.arg = IN(arg);
	args.len = IN(len);
	openfirmware(&args);
}
#else
void
OF_chain(void *virt, u_int size, void (*entry)(), void *arg, u_int len)
{
	/*
	 * This is a REALLY dirty hack till the firmware gets this going
	 */
#if 0
	if (size > 0)
		OF_release(virt, size);
#endif
	((int (*)(u_long, u_long, u_long, void *, u_long))entry)
	    (0, 0, (u_long)openfirmware, arg, len);
}
#endif
