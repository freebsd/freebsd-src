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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/stdarg.h>
#include <machine/bus.h>
#include <machine/pmap.h>
#include <machine/ofw_machdep.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofwvar.h>
#include "ofw_if.h"

static int ofw_real_init(ofw_t, void *openfirm);
static int ofw_real_test(ofw_t, const char *name);
static phandle_t ofw_real_peer(ofw_t, phandle_t node);
static phandle_t ofw_real_child(ofw_t, phandle_t node);
static phandle_t ofw_real_parent(ofw_t, phandle_t node);
static phandle_t ofw_real_instance_to_package(ofw_t, ihandle_t instance);
static ssize_t ofw_real_getproplen(ofw_t, phandle_t package, 
    const char *propname);
static ssize_t ofw_real_getprop(ofw_t, phandle_t package, const char *propname, 
    void *buf, size_t buflen);
static int ofw_real_nextprop(ofw_t, phandle_t package, const char *previous, 
    char *buf, size_t);
static int ofw_real_setprop(ofw_t, phandle_t package, const char *propname,
    const void *buf, size_t len);
static ssize_t ofw_real_canon(ofw_t, const char *device, char *buf, size_t len);
static phandle_t ofw_real_finddevice(ofw_t, const char *device);
static ssize_t ofw_real_instance_to_path(ofw_t, ihandle_t instance, char *buf, 
    size_t len);
static ssize_t ofw_real_package_to_path(ofw_t, phandle_t package, char *buf, 
    size_t len);
static int ofw_real_call_method(ofw_t, ihandle_t instance, const char *method, 
    int nargs, int nreturns, unsigned long *args_and_returns);
static ihandle_t ofw_real_open(ofw_t, const char *device);
static void ofw_real_close(ofw_t, ihandle_t instance);
static ssize_t ofw_real_read(ofw_t, ihandle_t instance, void *addr, size_t len);
static ssize_t ofw_real_write(ofw_t, ihandle_t instance, const void *addr, 
    size_t len);
static int ofw_real_seek(ofw_t, ihandle_t instance, u_int64_t pos);
static caddr_t ofw_real_claim(ofw_t, void *virt, size_t size, u_int align);
static void ofw_real_release(ofw_t, void *virt, size_t size);
static void ofw_real_enter(ofw_t);
static void ofw_real_exit(ofw_t);

static ofw_method_t ofw_real_methods[] = {
	OFWMETHOD(ofw_init,			ofw_real_init),
	OFWMETHOD(ofw_peer,			ofw_real_peer),
	OFWMETHOD(ofw_child,			ofw_real_child),
	OFWMETHOD(ofw_parent,			ofw_real_parent),
	OFWMETHOD(ofw_instance_to_package,	ofw_real_instance_to_package),
	OFWMETHOD(ofw_getproplen,		ofw_real_getproplen),
	OFWMETHOD(ofw_getprop,			ofw_real_getprop),
	OFWMETHOD(ofw_nextprop,			ofw_real_nextprop),
	OFWMETHOD(ofw_setprop,			ofw_real_setprop),
	OFWMETHOD(ofw_canon,			ofw_real_canon),
	OFWMETHOD(ofw_finddevice,		ofw_real_finddevice),
	OFWMETHOD(ofw_instance_to_path,		ofw_real_instance_to_path),
	OFWMETHOD(ofw_package_to_path,		ofw_real_package_to_path),

	OFWMETHOD(ofw_test,			ofw_real_test),
	OFWMETHOD(ofw_call_method,		ofw_real_call_method),
	OFWMETHOD(ofw_open,			ofw_real_open),
	OFWMETHOD(ofw_close,			ofw_real_close),
	OFWMETHOD(ofw_read,			ofw_real_read),
	OFWMETHOD(ofw_write,			ofw_real_write),
	OFWMETHOD(ofw_seek,			ofw_real_seek),
	OFWMETHOD(ofw_claim,			ofw_real_claim),
	OFWMETHOD(ofw_release,			ofw_real_release),
	OFWMETHOD(ofw_enter,			ofw_real_enter),
	OFWMETHOD(ofw_exit,			ofw_real_exit),

	{ 0, 0 }
};

static ofw_def_t ofw_real = {
	OFW_STD_REAL,
	ofw_real_methods,
	0
};
OFW_DEF(ofw_real);

MALLOC_DEFINE(M_OFWREAL, "ofwreal", "Open Firmware Real Mode Bounce Page");

static int (*openfirmware)(void *);

static vm_offset_t	of_bounce_phys;
static caddr_t		of_bounce_virt;
static off_t		of_bounce_offset;
static size_t		of_bounce_size;
static struct mtx	of_bounce_mtx;

/*
 * After the VM is up, allocate a wired, low memory bounce page.
 */

static void ofw_real_bounce_alloc(void *);

SYSINIT(ofw_real_bounce_alloc, SI_SUB_VM, SI_ORDER_ANY, 
    ofw_real_bounce_alloc, NULL);

static void
ofw_real_start(void)
{
	mtx_lock(&of_bounce_mtx);
	of_bounce_offset = 0;
}
	
static void
ofw_real_stop(void)
{
	mtx_unlock(&of_bounce_mtx);
}

static void
ofw_real_bounce_alloc(void *junk)
{
	/*
	 * Check that ofw_real is actually in use before allocating wads 
	 * of memory. Do this by checking if our mutex has been set up.
	 */
	if (!mtx_initialized(&of_bounce_mtx))
		return;

	/*
	 * Allocate a page of contiguous, wired physical memory that can
	 * fit into a 32-bit address space.
	 */

	mtx_lock(&of_bounce_mtx);

	of_bounce_virt = contigmalloc(PAGE_SIZE, M_OFWREAL, 0,
			     0, BUS_SPACE_MAXADDR_32BIT, PAGE_SIZE, PAGE_SIZE);
	of_bounce_phys = vtophys(of_bounce_virt);
	of_bounce_size = PAGE_SIZE;

	mtx_unlock(&of_bounce_mtx);
}

static cell_t
ofw_real_map(const void *buf, size_t len)
{
	cell_t phys;

	mtx_assert(&of_bounce_mtx, MA_OWNED);

	if (of_bounce_virt == NULL) {
		if (!pmap_bootstrapped)
			return (cell_t)buf;

		/*
		 * XXX: It is possible for us to get called before the VM has
		 * come online, but after the MMU is up. We don't have the
		 * bounce buffer yet, but can no longer presume a 1:1 mapping.
		 * Grab the physical address of the buffer, and hope it is
		 * in range if this happens.
		 */
		return (cell_t)vtophys(buf);
	}

	/*
	 * Make sure the bounce page offset satisfies any reasonable
	 * alignment constraint.
	 */
	of_bounce_offset += of_bounce_offset % sizeof(register_t);

	if (of_bounce_offset + len > of_bounce_size) {
		panic("Oversize Open Firmware call!");
		return 0;
	}

	memcpy(of_bounce_virt + of_bounce_offset, buf, len);
	phys = of_bounce_phys + of_bounce_offset;

	of_bounce_offset += len;

	return phys;
}

static void
ofw_real_unmap(cell_t physaddr, void *buf, size_t len)
{
	mtx_assert(&of_bounce_mtx, MA_OWNED);

	if (of_bounce_virt == NULL)
		return;

	memcpy(buf,of_bounce_virt + (physaddr - of_bounce_phys),len);
}

/* Initialiser */

static int
ofw_real_init(ofw_t ofw, void *openfirm)
{
	openfirmware = (int (*)(void *))openfirm;

	mtx_init(&of_bounce_mtx, "OF Bounce Page", MTX_DEF, 0);
	of_bounce_virt = NULL;
	return (0);
}

/*
 * Generic functions
 */

/* Test to see if a service exists. */
static int
ofw_real_test(ofw_t ofw, const char *name)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t service;
		cell_t missing;
	} args = {
		(cell_t)"test",
		1,
		1,
	};

	ofw_real_start();

	args.service = ofw_real_map(name, strlen(name) + 1);
	if (args.service == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_stop();
	return (args.missing);
}

/*
 * Device tree functions
 */

/* Return the next sibling of this node or 0. */
static phandle_t
ofw_real_peer(ofw_t ofw, phandle_t node)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t node;
		cell_t next;
	} args = {
		(cell_t)"peer",
		1,
		1,
	};

	args.node = node;
	if (openfirmware(&args) == -1)
		return (-1);
	return (args.next);
}

/* Return the first child of this node or 0. */
static phandle_t
ofw_real_child(ofw_t ofw, phandle_t node)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t node;
		cell_t child;
	} args = {
		(cell_t)"child",
		1,
		1,
	};

	args.node = node;
	if (openfirmware(&args) == -1)
		return (-1);
	return (args.child);
}

/* Return the parent of this node or 0. */
static phandle_t
ofw_real_parent(ofw_t ofw, phandle_t node)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t node;
		cell_t parent;
	} args = {
		(cell_t)"parent",
		1,
		1,
	};

	args.node = node;
	if (openfirmware(&args) == -1)
		return (-1);
	return (args.parent);
}

/* Return the package handle that corresponds to an instance handle. */
static phandle_t
ofw_real_instance_to_package(ofw_t ofw, ihandle_t instance)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t package;
	} args = {
		(cell_t)"instance-to-package",
		1,
		1,
	};

	args.instance = instance;
	if (openfirmware(&args) == -1)
		return (-1);
	return (args.package);
}

/* Get the length of a property of a package. */
static ssize_t
ofw_real_getproplen(ofw_t ofw, phandle_t package, const char *propname)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t package;
		cell_t propname;
		cell_t proplen;
	} args = {
		(cell_t)"getproplen",
		2,
		1,
	};

	ofw_real_start();

	args.package = package;
	args.propname = ofw_real_map(propname, strlen(propname) + 1);
	if (args.propname == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_stop();
	return (args.proplen);
}

/* Get the value of a property of a package. */
static ssize_t
ofw_real_getprop(ofw_t ofw, phandle_t package, const char *propname, void *buf, 
    size_t buflen)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t package;
		cell_t propname;
		cell_t buf;
		cell_t buflen;
		cell_t size;
	} args = {
		(cell_t)"getprop",
		4,
		1,
	};

	ofw_real_start();

	args.package = package;
	args.propname = ofw_real_map(propname, strlen(propname) + 1);
	args.buf = ofw_real_map(buf, buflen);
	args.buflen = buflen;
	if (args.propname == 0 || args.buf == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_unmap(args.buf, buf, buflen);

	ofw_real_stop();
	return (args.size);
}

/* Get the next property of a package. */
static int
ofw_real_nextprop(ofw_t ofw, phandle_t package, const char *previous, 
    char *buf, size_t size)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t package;
		cell_t previous;
		cell_t buf;
		cell_t flag;
	} args = {
		(cell_t)"nextprop",
		3,
		1,
	};

	ofw_real_start();

	args.package = package;
	args.previous = ofw_real_map(previous, strlen(previous) + 1);
	args.buf = ofw_real_map(buf, size);
	if (args.previous == 0 || args.buf == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_unmap(args.buf, buf, size);

	ofw_real_stop();
	return (args.flag);
}

/* Set the value of a property of a package. */
/* XXX Has a bug on FirePower */
static int
ofw_real_setprop(ofw_t ofw, phandle_t package, const char *propname,
    const void *buf, size_t len)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t package;
		cell_t propname;
		cell_t buf;
		cell_t len;
		cell_t size;
	} args = {
		(cell_t)"setprop",
		4,
		1,
	};

	ofw_real_start();

	args.package = package;
	args.propname = ofw_real_map(propname, strlen(propname) + 1);
	args.buf = ofw_real_map(buf, len);
	args.len = len;
	if (args.propname == 0 || args.buf == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_stop();
	return (args.size);
}

/* Convert a device specifier to a fully qualified pathname. */
static ssize_t
ofw_real_canon(ofw_t ofw, const char *device, char *buf, size_t len)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t device;
		cell_t buf;
		cell_t len;
		cell_t size;
	} args = {
		(cell_t)"canon",
		3,
		1,
	};

	ofw_real_start();

	args.device = ofw_real_map(device, strlen(device) + 1);
	args.buf = ofw_real_map(buf, len);
	args.len = len;
	if (args.device == 0 || args.buf == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_unmap(args.buf, buf, len);

	ofw_real_stop();
	return (args.size);
}

/* Return a package handle for the specified device. */
static phandle_t
ofw_real_finddevice(ofw_t ofw, const char *device)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t device;
		cell_t package;
	} args = {
		(cell_t)"finddevice",
		1,
		1,
	};

	ofw_real_start();

	args.device = ofw_real_map(device, strlen(device) + 1);
	if (args.device == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_stop();
	return (args.package);
}

/* Return the fully qualified pathname corresponding to an instance. */
static ssize_t
ofw_real_instance_to_path(ofw_t ofw, ihandle_t instance, char *buf, size_t len)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t buf;
		cell_t len;
		cell_t size;
	} args = {
		(cell_t)"instance-to-path",
		3,
		1,
	};

	ofw_real_start();

	args.instance = instance;
	args.buf = ofw_real_map(buf, len);
	args.len = len;
	if (args.buf == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_unmap(args.buf, buf, len);

	ofw_real_stop();
	return (args.size);
}

/* Return the fully qualified pathname corresponding to a package. */
static ssize_t
ofw_real_package_to_path(ofw_t ofw, phandle_t package, char *buf, size_t len)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t package;
		cell_t buf;
		cell_t len;
		cell_t size;
	} args = {
		(cell_t)"package-to-path",
		3,
		1,
	};

	ofw_real_start();

	args.package = package;
	args.buf = ofw_real_map(buf, len);
	args.len = len;
	if (args.buf == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_unmap(args.buf, buf, len);

	ofw_real_stop();
	return (args.size);
}

/*  Call the method in the scope of a given instance. */
static int
ofw_real_call_method(ofw_t ofw, ihandle_t instance, const char *method, 
    int nargs, int nreturns, unsigned long *args_and_returns)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t method;
		cell_t instance;
		cell_t args_n_results[12];
	} args = {
		(cell_t)"call-method",
		2,
		1,
	};
	cell_t *cp;
	unsigned long *ap;
	int n;

	if (nargs > 6)
		return (-1);

	ofw_real_start();
	args.nargs = nargs + 2;
	args.nreturns = nreturns + 1;
	args.method = ofw_real_map(method, strlen(method) + 1);
	args.instance = instance;

	ap = args_and_returns;
	for (cp = args.args_n_results + (n = nargs); --n >= 0;)
		*--cp = *(ap++);
	if (args.method == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_stop();
	if (args.args_n_results[nargs])
		return (args.args_n_results[nargs]);
	for (cp = args.args_n_results + nargs + (n = args.nreturns); --n > 0;)
		*(ap++) = *--cp;
	return (0);
}

/*
 * Device I/O functions
 */

/* Open an instance for a device. */
static ihandle_t
ofw_real_open(ofw_t ofw, const char *device)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t device;
		cell_t instance;
	} args = {
		(cell_t)"open",
		1,
		1,
	};

	ofw_real_start();

	args.device = ofw_real_map(device, strlen(device) + 1);
	if (args.device == 0 || openfirmware(&args) == -1 
	    || args.instance == 0) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_stop();
	return (args.instance);
}

/* Close an instance. */
static void
ofw_real_close(ofw_t ofw, ihandle_t instance)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
	} args = {
		(cell_t)"close",
		1,
		0,
	};

	args.instance = instance;
	openfirmware(&args);
}

/* Read from an instance. */
static ssize_t
ofw_real_read(ofw_t ofw, ihandle_t instance, void *addr, size_t len)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t addr;
		cell_t len;
		cell_t actual;
	} args = {
		(cell_t)"read",
		3,
		1,
	};

	ofw_real_start();

	args.instance = instance;
	args.addr = ofw_real_map(addr, len);
	args.len = len;
	if (args.addr == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_unmap(args.addr, addr, len);

	ofw_real_stop();
	return (args.actual);
}

/* Write to an instance. */
static ssize_t
ofw_real_write(ofw_t ofw, ihandle_t instance, const void *addr, size_t len)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t addr;
		cell_t len;
		cell_t actual;
	} args = {
		(cell_t)"write",
		3,
		1,
	};

	ofw_real_start();

	args.instance = instance;
	args.addr = ofw_real_map(addr, len);
	args.len = len;
	if (args.addr == 0 || openfirmware(&args) == -1) {
		ofw_real_stop();
		return (-1);
	}
	ofw_real_stop();
	return (args.actual);
}

/* Seek to a position. */
static int
ofw_real_seek(ofw_t ofw, ihandle_t instance, u_int64_t pos)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t instance;
		cell_t poshi;
		cell_t poslo;
		cell_t status;
	} args = {
		(cell_t)"seek",
		3,
		1,
	};

	args.instance = instance;
	args.poshi = pos >> 32;
	args.poslo = pos;
	if (openfirmware(&args) == -1)
		return (-1);
	return (args.status);
}

/*
 * Memory functions
 */

/* Claim an area of memory. */
static caddr_t
ofw_real_claim(ofw_t ofw, void *virt, size_t size, u_int align)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t virt;
		cell_t size;
		cell_t align;
		cell_t baseaddr;
	} args = {
		(cell_t)"claim",
		3,
		1,
	};

	args.virt = (cell_t)virt;
	args.size = size;
	args.align = align;
	if (openfirmware(&args) == -1)
		return ((void *)-1);
	return ((void *)args.baseaddr);
}

/* Release an area of memory. */
static void
ofw_real_release(ofw_t ofw, void *virt, size_t size)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
		cell_t virt;
		cell_t size;
	} args = {
		(cell_t)"release",
		2,
		0,
	};

	args.virt = (cell_t)virt;
	args.size = size;
	openfirmware(&args);
}

/*
 * Control transfer functions
 */

/* Suspend and drop back to the Open Firmware interface. */
static void
ofw_real_enter(ofw_t ofw)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
	} args = {
		(cell_t)"enter",
		0,
		0,
	};

	openfirmware(&args);
	/* We may come back. */
}

/* Shut down and drop back to the Open Firmware interface. */
static void
ofw_real_exit(ofw_t ofw)
{
	struct {
		cell_t name;
		cell_t nargs;
		cell_t nreturns;
	} args = {
		(cell_t)"exit",
		0,
		0,
	};

	openfirmware(&args);
	for (;;)			/* just in case */
		;
}

