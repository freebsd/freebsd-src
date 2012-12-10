/*-
 * Copyright (c) 2012 Advanced Computing Technologies LLC
 * Written by George Neville-Neil gnn@freebsd.org
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <sys/dtrace.h>
#include "../sys/dtrace_bsd.h"


static int	dtio_unload(void);
static void	dtio_getargdesc(void *, dtrace_id_t, void *,
		    dtrace_argdesc_t *);
static void	dtio_provide(void *, dtrace_probedesc_t *);
static void	dtio_destroy(void *, dtrace_id_t, void *);
static void	dtio_enable(void *, dtrace_id_t, void *);
static void	dtio_disable(void *, dtrace_id_t, void *);
static void	dtio_load(void *);

static dtrace_pattr_t dtio_attr = {
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
};

static char    *kernel = "kernel";

/*
 * Name strings.
 */
static char	*dtio_start_str = "start";
static char	*dtio_done_str = "done";
static char	*dtio_wait_start_str = "wait-start";
static char	*dtio_wait_done_str = "wait-done";

static dtrace_pops_t dtio_pops = {
	dtio_provide,
	NULL,
	dtio_enable,
	dtio_disable,
	NULL,
	NULL,
	dtio_getargdesc,
	NULL,
	NULL,
	dtio_destroy
};

static dtrace_provider_id_t	dtio_id;

extern uint32_t	dtio_start_id;
extern uint32_t	dtio_done_id;
extern uint32_t	dtio_wait_start_id;
extern uint32_t	dtio_wait_done_id;

static void
dtio_getargdesc(void *arg, dtrace_id_t id, void *parg,
    dtrace_argdesc_t *desc)
{
	const char *p = NULL;

	switch (desc->dtargd_ndx) {
	case 0:
		p = "struct bio *";
		break;
	case 1:
		p = "struct devstat *";
		break;
	default:
		desc->dtargd_ndx = DTRACE_ARGNONE;
	}

	if (p != NULL)
		strlcpy(desc->dtargd_native, p, sizeof(desc->dtargd_native));
}

static void
dtio_provide(void *arg, dtrace_probedesc_t *desc)
{
	if (desc != NULL)
		return;

	if (dtrace_probe_lookup(dtio_id, kernel, NULL, 
				dtio_start_str) == 0) {
		dtio_start_id = dtrace_probe_create(dtio_id, kernel, NULL, 
						   dtio_start_str, 0, NULL);
	}
	if (dtrace_probe_lookup(dtio_id, kernel, NULL, dtio_done_str) == 0) {
		dtio_done_id = dtrace_probe_create(dtio_id, kernel, NULL, 
						   dtio_done_str, 0, NULL);
	}
	if (dtrace_probe_lookup(dtio_id, kernel, NULL, 
				dtio_wait_start_str) == 0) {
		dtio_wait_start_id = dtrace_probe_create(dtio_id, kernel, 
							 NULL, 
							 dtio_wait_start_str, 
							 0, NULL);
	}
	if (dtrace_probe_lookup(dtio_id, kernel, NULL, 
				dtio_wait_done_str) == 0) {
		dtio_wait_done_id = dtrace_probe_create(dtio_id, kernel, NULL, 
						   dtio_wait_done_str, 0, NULL);
	}

}

static void
dtio_destroy(void *arg, dtrace_id_t id, void *parg)
{
}

static void
dtio_enable(void *arg, dtrace_id_t id, void *parg)
{
	if (id == dtio_start_id)
		dtrace_io_start_probe =
			(dtrace_io_start_probe_func_t)dtrace_probe;
	else if (id == dtio_done_id)
		dtrace_io_done_probe =
			(dtrace_io_done_probe_func_t)dtrace_probe;
	else if (id == dtio_wait_start_id)
		dtrace_io_wait_start_probe =
			(dtrace_io_wait_start_probe_func_t)dtrace_probe;
	else if (id == dtio_wait_done_id)
		dtrace_io_wait_done_probe =
			(dtrace_io_wait_done_probe_func_t)dtrace_probe;
	else
		printf("dtrace io provider: unknown ID\n");

}

static void
dtio_disable(void *arg, dtrace_id_t id, void *parg)
{
	if (id == dtio_start_id)
		dtrace_io_start_probe = NULL;
	else if (id == dtio_done_id)
		dtrace_io_done_probe = NULL;
	else if (id == dtio_wait_start_id)
		dtrace_io_wait_start_probe = NULL;
	else if (id == dtio_wait_done_id)
		dtrace_io_wait_done_probe = NULL;
	else 
		printf("dtrace io provider: unknown ID\n");
	
}

static void
dtio_load(void *dummy)
{
	if (dtrace_register("io", &dtio_attr, DTRACE_PRIV_USER, NULL, 
			    &dtio_pops, NULL, &dtio_id) != 0)
		return;
}


static int
dtio_unload()
{
	dtrace_io_start_probe = NULL;
	dtrace_io_done_probe = NULL;
	dtrace_io_wait_start_probe = NULL;
	dtrace_io_wait_done_probe = NULL;

	return (dtrace_unregister(dtio_id));
}

static int
dtio_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		break;

	case MOD_UNLOAD:
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

SYSINIT(dtio_load, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY,
    dtio_load, NULL);
SYSUNINIT(dtio_unload, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY,
    dtio_unload, NULL);

DEV_MODULE(dtio, dtio_modevent, NULL);
MODULE_VERSION(dtio, 1);
MODULE_DEPEND(dtio, dtrace, 1, 1, 1);
MODULE_DEPEND(dtio, opensolaris, 1, 1, 1);
