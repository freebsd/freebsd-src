/*	$NetBSD: Locore.c,v 1.7 2000/08/20 07:04:59 tsubai Exp $	*/

/*
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
/*
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
 *
 * $FreeBSD$
 */

#include <sys/systm.h>

#include <machine/stdarg.h>

#include <dev/ofw/openfirm.h>

static ihandle_t stdin;
static ihandle_t stdout;

char	*OF_buf;

void	ofbcopy(const void *, void *, size_t);

/* Initialiaser */

void
OF_init(int (*openfirm)(void *))
{
	phandle_t	chosen;

	set_openfirm_callback(openfirm);
	if ((chosen = OF_finddevice("/chosen")) == -1)
		OF_exit();
	OF_getprop(chosen, "stdin", &stdin, sizeof(stdin));
	OF_getprop(chosen, "stdout", &stdout, sizeof(stdout));
}

void
OF_printf(const char *fmt, ...)
{
	va_list	va;
	char	buf[1024];

	va_start(va, fmt);
	vsprintf(buf, fmt, va);
	OF_write(stdout, buf, strlen(buf));
	va_end(va);
}

/*
 * Generic functions
 */

/* Test to see if a service exists. */
int
OF_test(char *name)
{
	static struct {
		char	*name;
		int	nargs;
		int	nreturns;
		char	*service;
		int	missing;
	} args = {
		"test",
		1,
		1,
	};

	args.service = name;
	if (openfirmware(&args) == -1)
		return -1;
	return args.missing;
}

/* Return firmware millisecond count. */
int
OF_milliseconds()
{
	static struct {
		char	*name;
		int	nargs;
		int	nreturns;
		int	ms;
	} args = {
		"milliseconds",
		0,
		1,
	};
	
	openfirmware(&args);
	return args.ms;
}

/*
 * Device tree functions
 */

/* Return the next sibling of this node or 0. */
phandle_t
OF_peer(phandle_t node)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		phandle_t	node;
		phandle_t	next;
	} args = {
		"peer",
		1,
		1,
	};

	args.node = node;
	if (openfirmware(&args) == -1)
		return -1;
	return args.next;
}

/* Return the first child of this node or 0. */
phandle_t
OF_child(phandle_t node)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		phandle_t	node;
		phandle_t	child;
	} args = {
		"child",
		1,
		1,
	};

	args.node = node;
	if (openfirmware(&args) == -1)
		return -1;
	return args.child;
}

/* Return the parent of this node or 0. */
phandle_t
OF_parent(phandle_t node)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		phandle_t	node;
		phandle_t	parent;
	} args = {
		"parent",
		1,
		1,
	};

	args.node = node;
	if (openfirmware(&args) == -1)
		return -1;
	return args.parent;
}

/* Return the package handle that corresponds to an instance handle. */
phandle_t
OF_instance_to_package(ihandle_t instance)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		ihandle_t	instance;
		phandle_t	package;
	} args = {
		"instance-to-package",
		1,
		1,
	};
	
	args.instance = instance;
	if (openfirmware(&args) == -1)
		return -1;
	return args.package;
}

/* Get the length of a property of a package. */
int
OF_getproplen(phandle_t package, char *propname)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		phandle_t	package;
		char 		*propname;
		int		proplen;
	} args = {
		"getproplen",
		2,
		1,
	};

	args.package = package;
	args.propname = propname;
	if (openfirmware(&args) == -1)
		return -1;
	return args.proplen;
}

/* Get the value of a property of a package. */
int
OF_getprop(phandle_t package, char *propname, void *buf, int buflen)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		phandle_t	package;
		char		*propname;
		void		*buf;
		int		buflen;
		int		size;
	} args = {
		"getprop",
		4,
		1,
	};
	
	args.package = package;
	args.propname = propname;
	args.buf = buf;
	args.buflen = buflen;
	if (openfirmware(&args) == -1)
		return -1;
	return args.size;
}

/* Get the next property of a package. */
int
OF_nextprop(phandle_t package, char *previous, char *buf)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		phandle_t	package;
		char 		*previous;
		char		*buf;
		int		flag;
	} args = {
		"nextprop",
		3,
		1,
	};

	args.package = package;
	args.previous = previous;
	args.buf = buf;
	if (openfirmware(&args) == -1)
		return -1;
	return args.flag;
}

/* Set the value of a property of a package. */
/* XXX Has a bug on FirePower */
int
OF_setprop(phandle_t package, char *propname, void *buf, int len)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		phandle_t	package;
		char		*propname;
		void		*buf;
		int		len;
		int		size;
	} args = {
		"setprop",
		4,
		1,
	};
	
	args.package = package;
	args.propname = propname;
	args.buf = buf;
	args.len = len;
	if (openfirmware(&args) == -1)
		return -1;
	return args.size;
}

/* Convert a device specifier to a fully qualified pathname. */
int
OF_canon(const char *device, char *buf, int len)
{
	static struct {
		char	*name;
		int	nargs;
		int	nreturns;
		char	*device;
		char	*buf;
		int	len;
		int	size;
	} args = {
		"canon",
		3,
		1,
	};
	
	args.device = (char *)(uintptr_t *)device;
	args.buf = buf;
	args.len = len;
	if (openfirmware(&args) == -1)
		return -1;
	return args.size;
}

/* Return a package handle for the specified device. */
phandle_t
OF_finddevice(const char *device)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		char		*device;
		phandle_t	package;
	} args = {
		"finddevice",
		1,
		1,
	};	
	
	args.device = (char *)(uintptr_t *)device;
	if (openfirmware(&args) == -1)
		return -1;
	return args.package;
}

/* Return the fully qualified pathname corresponding to an instance. */
int
OF_instance_to_path(ihandle_t instance, char *buf, int len)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		ihandle_t	instance;
		char		*buf;
		int		len;
		int		size;
	} args = {
		"instance-to-path",
		3,
		1,
	};

	args.instance = instance;
	args.buf = buf;
	args.len = len;
	if (openfirmware(&args) == -1)
		return -1;
	return(args.size);
}

/* Return the fully qualified pathname corresponding to a package. */
int
OF_package_to_path(phandle_t package, char *buf, int len)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		phandle_t	package;
		char		*buf;
		int		len;
		int		size;
	} args = {
		"package-to-path",
		3,
		1,
	};

	args.package = package;
	args.buf = buf;
	args.len = len;
	if (openfirmware(&args) == -1)
		return -1;
	return(args.size);
}

/*  Call the method in the scope of a given instance. */
int
OF_call_method(char *method, ihandle_t instance, int nargs, int nreturns, ...)
{
	va_list ap;
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		char		*method;
		ihandle_t	instance;
		int		args_n_results[12];
	} args = {
		"call-method",
		2,
		1,
	};
	int *ip, n;

	if (nargs > 6)
		return -1;
	args.nargs = nargs + 2;
	args.nreturns = nreturns + 1;
	args.method = method;
	args.instance = instance;
	va_start(ap, nreturns);
	for (ip = args.args_n_results + (n = nargs); --n >= 0;)
		*--ip = va_arg(ap, int);

	if (openfirmware(&args) == -1)
		return -1;
	if (args.args_n_results[nargs])
		return args.args_n_results[nargs];
	for (ip = args.args_n_results + nargs + (n = args.nreturns); --n > 0;)
		*va_arg(ap, int *) = *--ip;
	va_end(ap);
	return 0;
}

/*
 * Device I/O functions.
 */

/* Open an instance for a device. */
ihandle_t
OF_open(char *device)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		char		*device;
		ihandle_t	instance;
	} args = {
		"open",
		1,
		1,
	};
	
	args.device = device;
	if (openfirmware(&args) == -1 || args.instance == 0) {
		return -1;
	}
	return args.instance;
}

/* Close an instance. */
void
OF_close(ihandle_t instance)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		ihandle_t	instance;
	} args = {
		"close",
		1,
		0,
	};
	
	args.instance = instance;
	openfirmware(&args);
}

/* Read from an instance. */
int
OF_read(ihandle_t instance, void *addr, int len)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		ihandle_t	instance;
		void		*addr;
		int		len;
		int		actual;
	} args = {
		"read",
		3,
		1,
	};

	args.instance = instance;
	args.addr = addr;
	args.len = len;
	if (openfirmware(&args) == -1)
		return -1;

	return args.actual;
}

/* Write to an instance. */
int
OF_write(ihandle_t instance, void *addr, int len)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		ihandle_t	instance;
		void		*addr;
		int		len;
		int		actual;
	} args = {
		"write",
		3,
		1,
	};

	args.instance = instance;
	args.addr = addr;
	args.len = len;
	if (openfirmware(&args) == -1)
		return -1;
	return args.actual;
}

/* Seek to a position. */
int
OF_seek(ihandle_t instance, u_int64_t pos)
{
	static struct {
		char		*name;
		int		nargs;
		int		nreturns;
		ihandle_t	instance;
		int		poshi;
		int		poslo;
		int		status;
	} args = {
		"seek",
		3,
		1,
	};
	
	args.instance = instance;
	args.poshi = (int)(pos >> 32);
	args.poslo = (int)pos;
	if (openfirmware(&args) == -1)
		return -1;
	return args.status;
}

/*
 * Memory functions.
 */

/* Claim an area of memory. */
void *
OF_claim(void *virt, u_int size, u_int align)
{
	static struct {
		char	*name;
		int	 nargs;
		int	 nreturns;
		void	 *virt;
		u_int	 size;
		u_int	 align;
		void	 *baseaddr;
	} args = {
		"claim",
		3,
		1,
	};

	args.virt = virt;
	args.size = size;
	args.align = align;
	if (openfirmware(&args) == -1)
		return (void *)-1;
	return args.baseaddr;
}

/* Release an area of memory. */
void
OF_release(void *virt, u_int size)
{
	static struct {
		char	*name;
		int	nargs;
		int	nreturns;
		void	*virt;
		u_int	size;
	} args = {
		"release",
		2,
		0,
	};
	
	args.virt = virt;
	args.size = size;
	openfirmware(&args);
}

/*
 * Control transfer functions.
 */

/* Reset the system and call "boot <bootspec>". */
void
OF_boot(char *bootspec)
{
	static struct {
		char	*name;
		int	nargs;
		int	nreturns;
		char	*bootspec;
	} args = {
		"boot",
		1,
		0,
	};

	args.bootspec = bootspec;
	openfirmware(&args);
	for (;;);			/* just in case */
}

/* Suspend and drop back to the OpenFirmware interface. */
void
OF_enter()
{
	static struct {
		char *name;
		int nargs;
		int nreturns;
	} args = {
		"enter",
		0,
		0
	};

	openfirmware(&args);
	return;				/* We may come back. */
}

/* Shut down and drop back to the OpenFirmware interface. */
void
OF_exit()
{
	static struct {
		char *name;
		int nargs;
		int nreturns;
	} args = {
		"exit",
		0,
		0
	};

	openfirmware(&args);
	for (;;);			/* just in case */
}

/* Free <size> bytes starting at <virt>, then call <entry> with <arg>. */
#ifdef	__notyet__
void
OF_chain(void *virt, u_int size, void (*entry)(), void *arg, u_int len)
{
	static struct {
		char	*name;
		int	nargs;
		int	nreturns;
		void	*virt;
		u_int	size;
		void	(*entry)();
		void	*arg;
		u_int	len;
	} args = {
		"chain",
		5,
		0,
	};

	args.virt = virt;
	args.size = size;
	args.entry = entry;
	args.arg = arg;
	args.len = len;
	openfirmware(&args);
}
#else
void
OF_chain(void *virt, u_int size,
    void (*entry)(void *, u_int, void *, void *, u_int), void *arg, u_int len)
{
	/*
	 * This is a REALLY dirty hack till the firmware gets this going
	 */
#if 0
	OF_release(virt, size);
#endif
	entry(0, 0, openfirmware, arg, len);
}
#endif

void
ofbcopy(const void *src, void *dst, size_t len)
{
	const char *sp = src;
	char *dp = dst;

	if (src == dst)
		return;

	while (len-- > 0)
		*dp++ = *sp++;
}
