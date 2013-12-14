/*-
 * Copyright (c) 2005 Peter Grehan
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_PLATFORMVAR_H_
#define _MACHINE_PLATFORMVAR_H_

/*
 * A PowerPC platform implementation is declared with a kernel object and
 * an associated method table, similar to a device driver.
 *
 * e.g.
 *
 * static platform_method_t chrp_methods[] = {
 *	PLATFORMMETHOD(platform_probe,		chrp_probe),
 *	PLATFORMMETHOD(platform_mem_regions,	ofw_mem_regions),
 *  ...
 *	PLATFORMMETHOD(platform_smp_first_cpu,	chrp_smp_first_cpu),
 *	PLATFORMMETHOD_END
 * };
 *
 * static platform_def_t chrp_platform = {
 * 	"chrp",
 *	chrp_methods,
 *	sizeof(chrp_platform_softc),	// or 0 if no softc
 * };
 *
 * PLATFORM_DEF(chrp_platform);
 */

#include <sys/kobj.h>
#include <sys/linker_set.h>

struct platform_kobj {
	/*
	 * A platform instance is a kernel object
	 */
	KOBJ_FIELDS;

	/* Platform class, for access to class specific data */
	struct kobj_class *cls;

#if 0
	/*
	 * Utility elements that an instance may use
	 */
	struct mtx	platform_mtx;	/* available for instance use */
	void		*platform_iptr;	/* instance data pointer */

	/*
	 * Opaque data that can be overlaid with an instance-private
	 * structure. Platform code can test that this is large enough at
	 * compile time with a sizeof() test againt it's softc. There
	 * is also a run-time test when the platform kernel object is
	 * registered.
	 */
#define PLATFORM_OPAQUESZ	64
	u_int		platform_opaque[PLATFORM_OPAQUESZ];
#endif
};

typedef struct platform_kobj	*platform_t;
typedef struct kobj_class	platform_def_t;
#define platform_method_t	kobj_method_t

#define PLATFORMMETHOD		KOBJMETHOD
#define	PLATFORMMETHOD_END	KOBJMETHOD_END

#define PLATFORM_DEF(name)	DATA_SET(platform_set, name)

#ifdef FDT
struct fdt_platform_class {
	KOBJ_CLASS_FIELDS;

	const char *fdt_compatible;
};

typedef struct fdt_platform_class fdt_platform_def_t;

extern platform_method_t fdt_platform_methods[];

#define FDT_PLATFORM_DEF(NAME, NAME_STR, size, compatible)	\
static fdt_platform_def_t NAME ## _fdt_platform = {		\
	.name = NAME_STR,					\
	.methods = fdt_platform_methods,			\
	.fdt_compatible = compatible,				\
};								\
static kobj_class_t NAME ## _baseclasses[] =			\
	{ (kobj_class_t)&NAME ## _fdt_platform, NULL };		\
static platform_def_t NAME ## _platform = {			\
	NAME_STR,						\
	NAME ## _methods,					\
	size,							\
	NAME ## _baseclasses,					\
};								\
DATA_SET(platform_set, NAME ## _platform)

#endif

void arm_tmr_cpu_initclocks(platform_t);
void arm_tmr_delay(platform_t, int);

#endif /* _MACHINE_PLATFORMVAR_H_ */
