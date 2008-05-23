/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * Portions Copyright 2006-2008 John Birrell jb@freebsd.org
 *
 * $FreeBSD$
 *
 */

#ifndef KDTRACE_HOOKS
#define KDTRACE_HOOKS
#endif

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <sys/dtrace.h>
#include <sys/sdt.h>

#define SDT_ADDR2NDX(addr) (((uintptr_t)(addr)) >> 4)

static d_open_t sdt_open;
static int	sdt_unload(void);
static void	sdt_getargdesc(void *, dtrace_id_t, void *, dtrace_argdesc_t *);
static void	sdt_provide_probes(void *, dtrace_probedesc_t *);
static void	sdt_destroy(void *, dtrace_id_t, void *);
static void	sdt_enable(void *, dtrace_id_t, void *);
static void	sdt_disable(void *, dtrace_id_t, void *);
static void	sdt_load(void *);

static struct cdevsw sdt_cdevsw = {
	.d_version	= D_VERSION,
	.d_open		= sdt_open,
	.d_name		= "sdt",
};

static dtrace_pattr_t sdt_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static dtrace_pops_t sdt_pops = {
	sdt_provide_probes,
	NULL,
	sdt_enable,
	sdt_disable,
	NULL,
	NULL,
	sdt_getargdesc,
	NULL,
	NULL,
	sdt_destroy
};

static struct cdev		*sdt_cdev;

static int
sdt_argtype_callback(struct sdt_argtype *argtype, void *arg)
{
	dtrace_argdesc_t *desc = arg;

	if (desc->dtargd_ndx == argtype->ndx) {
		desc->dtargd_mapping = desc->dtargd_ndx; /* XXX */
		strlcpy(desc->dtargd_native, argtype->type,
		    sizeof(desc->dtargd_native));
		desc->dtargd_xlate[0] = '\0'; /* XXX */
	}

	return (0);
}

static void
sdt_getargdesc(void *arg, dtrace_id_t id, void *parg, dtrace_argdesc_t *desc)
{
	struct sdt_probe *probe = parg;

	if (desc->dtargd_ndx < probe->n_args)
		(void) (sdt_argtype_listall(probe, sdt_argtype_callback, desc));
	else
		desc->dtargd_ndx = DTRACE_ARGNONE;

	return;
}

static int
sdt_probe_callback(struct sdt_probe *probe, void *arg __unused)
{
	struct sdt_provider *prov = probe->prov;
	char mod[64];
	char func[64];
	char name[64];

	/*
	 * Unfortunately this is necessary because the Solaris DTrace
	 * code mixes consts and non-consts with casts to override
	 * the incompatibilies. On FreeBSD, we use strict warnings
	 * in gcc, so we have to respect const vs non-const.
	 */
	strlcpy(mod, probe->mod, sizeof(mod));
	strlcpy(func, probe->func, sizeof(func));
	strlcpy(name, probe->name, sizeof(name));

	if (dtrace_probe_lookup(prov->id, mod, func, name) != 0)
		return (0);

	(void) dtrace_probe_create(prov->id, probe->mod, probe->func,
	    probe->name, 0, probe);

	return (0);
}

static int
sdt_provider_entry(struct sdt_provider *prov, void *arg)
{
	return (sdt_probe_listall(prov, sdt_probe_callback, NULL));
}

static void
sdt_provide_probes(void *arg, dtrace_probedesc_t *desc)
{
	if (desc != NULL)
		return;

	(void) sdt_provider_listall(sdt_provider_entry, NULL);
}

static void
sdt_destroy(void *arg, dtrace_id_t id, void *parg)
{
	/* Nothing to do here. */
}

static void
sdt_enable(void *arg, dtrace_id_t id, void *parg)
{
	struct sdt_probe *probe = parg;

	probe->id = id;
}

static void
sdt_disable(void *arg, dtrace_id_t id, void *parg)
{
	struct sdt_probe *probe = parg;

	probe->id = 0;
}

static int
sdt_provider_reg_callback(struct sdt_provider *prov, void *arg __unused)
{
	return (dtrace_register(prov->name, &sdt_attr, DTRACE_PRIV_USER,
	    NULL, &sdt_pops, NULL, (dtrace_provider_id_t *) &prov->id));
}

static void
sdt_load(void *dummy)
{
	/* Create the /dev/dtrace/sdt entry. */
	sdt_cdev = make_dev(&sdt_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "dtrace/sdt");

	sdt_probe_func = dtrace_probe;

	(void) sdt_provider_listall(sdt_provider_reg_callback, NULL);
}

static int
sdt_provider_unreg_callback(struct sdt_provider *prov, void *arg __unused)
{
	return (dtrace_unregister(prov->id));
}

static int
sdt_unload()
{
	int error = 0;

	sdt_probe_func = sdt_probe_stub;

	(void) sdt_provider_listall(sdt_provider_unreg_callback, NULL);
	
	destroy_dev(sdt_cdev);

	return (error);
}

/* ARGSUSED */
static int
sdt_modevent(module_t mod __unused, int type, void *data __unused)
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

/* ARGSUSED */
static int
sdt_open(struct cdev *dev __unused, int oflags __unused, int devtype __unused, struct thread *td __unused)
{
	return (0);
}

SYSINIT(sdt_load, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, sdt_load, NULL);
SYSUNINIT(sdt_unload, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, sdt_unload, NULL);

DEV_MODULE(sdt, sdt_modevent, NULL);
MODULE_VERSION(sdt, 1);
MODULE_DEPEND(sdt, dtrace, 1, 1, 1);
MODULE_DEPEND(sdt, opensolaris, 1, 1, 1);
