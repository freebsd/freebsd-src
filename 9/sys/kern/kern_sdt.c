/*-
 * Copyright 2006-2008 John Birrell <jb@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 * Backend for the Statically Defined Tracing (SDT) kernel support. This is
 * required to allow a module to load even though DTrace kernel support may
 * not be present. A module may be built with SDT probes in it which are
 * registered and deregistered via SYSINIT/SYSUNINIT.
 *
 */

#include "opt_kdtrace.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/sx.h>
#include <sys/sdt.h>

/*
 * This is the list of statically defined tracing providers.
 */
static TAILQ_HEAD(sdt_provider_list_head, sdt_provider) sdt_provider_list;

/*
 * Mutex to serialise access to the SDT provider list.
 */
static struct sx sdt_sx;

/*
 * Hook for the DTrace probe function. The 'sdt' provider will set this
 * to dtrace_probe when it loads.
 */
sdt_probe_func_t sdt_probe_func = sdt_probe_stub;

/*
 * This is a stub for probe calls in case kernel DTrace support isn't
 * compiled in. It should never get called because there is no DTrace
 * support to enable it.
 */
void
sdt_probe_stub(uint32_t id, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)
{
	printf("sdt_probe_stub: Why did this get called?\n");
}

/*
 * Called from SYSINIT to register a provider.
 */
void
sdt_provider_register(void *arg)
{
	struct sdt_provider *prov = arg;

	sx_xlock(&sdt_sx);

	TAILQ_INSERT_TAIL(&sdt_provider_list, prov, prov_entry);

	TAILQ_INIT(&prov->probe_list);

	sx_xunlock(&sdt_sx);
}

/*
 * Called from SYSUNINIT to de-register a provider.
 */
void
sdt_provider_deregister(void *arg)
{
	struct sdt_provider *prov = arg;

	sx_xlock(&sdt_sx);

	TAILQ_REMOVE(&sdt_provider_list, prov, prov_entry);

	sx_xunlock(&sdt_sx);
}

/*
 * Called from SYSINIT to register a statically defined trace probe.
 */
void
sdt_probe_register(void *arg)
{
	struct sdt_probe *probe = arg;

	/*
	 * Check the reference structure version. Only version 1 is
	 * supported at the moment.
	 */
	if (probe->version != sizeof(struct sdt_probe)) {
		printf("%s:%s:%s has version %d when %d required\n", probe->mod, probe->func, probe->name, probe->version, (int) sizeof(struct sdt_probe));
		return;
	}

	sx_xlock(&sdt_sx);

	TAILQ_INSERT_TAIL(&probe->prov->probe_list, probe, probe_entry);

	TAILQ_INIT(&probe->argtype_list);

	probe->state = SDT_INIT;

	sx_xunlock(&sdt_sx);
}

/*
 * Called from SYSUNINIT to de-register a statically defined trace probe.
 */
void
sdt_probe_deregister(void *arg)
{
	struct sdt_probe *probe = arg;

	sx_xlock(&sdt_sx);

	if (probe->state == SDT_INIT) {
		TAILQ_REMOVE(&probe->prov->probe_list, probe, probe_entry);
		probe->state = SDT_UNINIT;
	}

	sx_xunlock(&sdt_sx);
}

/*
 * Called from SYSINIT to register a statically defined trace probe argument.
 */
void
sdt_argtype_register(void *arg)
{
	struct sdt_argtype *argtype = arg;

	sx_xlock(&sdt_sx);

	TAILQ_INSERT_TAIL(&argtype->probe->argtype_list, argtype, argtype_entry);

	argtype->probe->n_args++;

	sx_xunlock(&sdt_sx);
}

/*
 * Called from SYSUNINIT to de-register a statically defined trace probe argument.
 */
void
sdt_argtype_deregister(void *arg)
{
	struct sdt_argtype *argtype = arg;

	sx_xlock(&sdt_sx);

	TAILQ_REMOVE(&argtype->probe->argtype_list, argtype, argtype_entry);

	sx_xunlock(&sdt_sx);
}

static void
sdt_init(void *arg)
{ 
	sx_init_flags(&sdt_sx, "Statically Defined Tracing", SX_NOWITNESS);

	TAILQ_INIT(&sdt_provider_list);
}

SYSINIT(sdt, SI_SUB_KDTRACE, SI_ORDER_FIRST, sdt_init, NULL);

static void
sdt_uninit(void *arg)
{ 
	sx_destroy(&sdt_sx);
}

SYSUNINIT(sdt, SI_SUB_KDTRACE, SI_ORDER_FIRST, sdt_uninit, NULL);

/*
 * List statically defined tracing providers.
 */
int
sdt_provider_listall(sdt_provider_listall_func_t callback_func,void *arg)
{
	int error = 0;
	struct sdt_provider *prov;

	sx_xlock(&sdt_sx);

	TAILQ_FOREACH(prov, &sdt_provider_list, prov_entry) {
		if ((error = callback_func(prov, arg)) != 0)
			break;
	}

	sx_xunlock(&sdt_sx);

	return (error);
}

/*
 * List statically defined tracing probes.
 */
int
sdt_probe_listall(struct sdt_provider *prov, 
    sdt_probe_listall_func_t callback_func,void *arg)
{
	int error = 0;
	int locked;
	struct sdt_probe *probe;

	locked = sx_xlocked(&sdt_sx);
	if (!locked)
		sx_xlock(&sdt_sx);

	TAILQ_FOREACH(probe, &prov->probe_list, probe_entry) {
		if ((error = callback_func(probe, arg)) != 0)
			break;
	}

	if (!locked)
		sx_xunlock(&sdt_sx);

	return (error);
}

/*
 * List statically defined tracing probe arguments.
 */
int
sdt_argtype_listall(struct sdt_probe *probe, 
    sdt_argtype_listall_func_t callback_func,void *arg)
{
	int error = 0;
	int locked;
	struct sdt_argtype *argtype;

	locked = sx_xlocked(&sdt_sx);
	if (!locked)
		sx_xlock(&sdt_sx);

	TAILQ_FOREACH(argtype, &probe->argtype_list, argtype_entry) {
		if ((error = callback_func(argtype, arg)) != 0)
			break;
	}

	if (!locked)
		sx_xunlock(&sdt_sx);

	return (error);
}
