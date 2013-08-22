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

#include "opt_kdtrace.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sdt.h>

#include <sys/dtrace.h>
#include <sys/dtrace_bsd.h>

/* DTrace methods. */
static void	sdt_getargdesc(void *, dtrace_id_t, void *, dtrace_argdesc_t *);
static void	sdt_provide_probes(void *, dtrace_probedesc_t *);
static void	sdt_destroy(void *, dtrace_id_t, void *);
static void	sdt_enable(void *, dtrace_id_t, void *);
static void	sdt_disable(void *, dtrace_id_t, void *);

static d_open_t	sdt_open;
static void	sdt_load(void *);
static int	sdt_unload(void *);
static void	sdt_create_provider(struct sdt_provider *);
static void	sdt_create_probe(struct sdt_probe *);
static void	sdt_kld_load(void *, struct linker_file *);
static void	sdt_kld_unload(void *, struct linker_file *, int *);

static MALLOC_DEFINE(M_SDT, "SDT", "DTrace SDT providers");

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
	sdt_destroy,
};

static struct cdev	*sdt_cdev;

static TAILQ_HEAD(, sdt_provider) sdt_prov_list;

eventhandler_tag	sdt_kld_load_tag;
eventhandler_tag	sdt_kld_unload_tag;

static void
sdt_create_provider(struct sdt_provider *prov)
{
	struct sdt_provider *curr, *newprov;

	TAILQ_FOREACH(curr, &sdt_prov_list, prov_entry)
		if (strcmp(prov->name, curr->name) == 0) {
			/* The provider has already been defined. */
			curr->sdt_refs++;
			return;
		}

	/*
	 * Make a copy of prov so that we don't lose fields if its module is
	 * unloaded but the provider isn't destroyed. This could happen with
	 * a provider that spans multiple modules.
	 */
	newprov = malloc(sizeof(*newprov), M_SDT, M_WAITOK | M_ZERO);
	newprov->name = strdup(prov->name, M_SDT);
	prov->sdt_refs = newprov->sdt_refs = 1;
	TAILQ_INIT(&newprov->probe_list);

	TAILQ_INSERT_TAIL(&sdt_prov_list, newprov, prov_entry);

	(void)dtrace_register(newprov->name, &sdt_attr, DTRACE_PRIV_USER, NULL,
	    &sdt_pops, NULL, (dtrace_provider_id_t *)&newprov->id);
	prov->id = newprov->id;
}

static void
sdt_create_probe(struct sdt_probe *probe)
{
	struct sdt_provider *prov;
	char mod[DTRACE_MODNAMELEN];
	char func[DTRACE_FUNCNAMELEN];
	char name[DTRACE_NAMELEN];
	size_t len;

	TAILQ_FOREACH(prov, &sdt_prov_list, prov_entry)
		if (strcmp(prov->name, probe->prov->name) == 0)
			break;

	KASSERT(prov != NULL, ("probe defined without a provider"));

	/* If no module name was specified, use the module filename. */
	if (*probe->mod == 0) {
		len = strlcpy(mod, probe->sdtp_lf->filename, sizeof(mod));
		if (len > 3 && strcmp(mod + len - 3, ".ko") == 0)
			mod[len - 3] = '\0';
	} else
		strlcpy(mod, probe->mod, sizeof(mod));

	/*
	 * Unfortunately this is necessary because the Solaris DTrace
	 * code mixes consts and non-consts with casts to override
	 * the incompatibilies. On FreeBSD, we use strict warnings
	 * in the C compiler, so we have to respect const vs non-const.
	 */
	strlcpy(func, probe->func, sizeof(func));
	strlcpy(name, probe->name, sizeof(name));

	if (dtrace_probe_lookup(prov->id, mod, func, name) != DTRACE_IDNONE)
		return;

	TAILQ_INSERT_TAIL(&prov->probe_list, probe, probe_entry);

	(void)dtrace_probe_create(prov->id, mod, func, name, 1, probe);
}

/* Probes are created through the SDT module load/unload hook. */
static void
sdt_provide_probes(void *arg, dtrace_probedesc_t *desc)
{
}

static void
sdt_enable(void *arg __unused, dtrace_id_t id, void *parg)
{
	struct sdt_probe *probe = parg;

	probe->id = id;
	probe->sdtp_lf->nenabled++;
}

static void
sdt_disable(void *arg __unused, dtrace_id_t id, void *parg)
{
	struct sdt_probe *probe = parg;

	KASSERT(probe->sdtp_lf->nenabled > 0, ("no probes enabled"));

	probe->id = 0;
	probe->sdtp_lf->nenabled--;
}

static void
sdt_getargdesc(void *arg, dtrace_id_t id, void *parg, dtrace_argdesc_t *desc)
{
	struct sdt_argtype *argtype;
	struct sdt_probe *probe = parg;

	if (desc->dtargd_ndx < probe->n_args) {
		TAILQ_FOREACH(argtype, &probe->argtype_list, argtype_entry) {
			if (desc->dtargd_ndx == argtype->ndx) {
				/* XXX */
				desc->dtargd_mapping = desc->dtargd_ndx;
				strlcpy(desc->dtargd_native, argtype->type,
				    sizeof(desc->dtargd_native));
				desc->dtargd_xlate[0] = '\0'; /* XXX */
			}
		}
	} else
		desc->dtargd_ndx = DTRACE_ARGNONE;
}

static void
sdt_destroy(void *arg, dtrace_id_t id, void *parg)
{
	struct sdt_probe *probe;

	probe = parg;
	TAILQ_REMOVE(&probe->prov->probe_list, probe, probe_entry);
}

/*
 * Called from the kernel linker when a module is loaded, before
 * dtrace_module_loaded() is called. This is done so that it's possible to
 * register new providers when modules are loaded. We cannot do this in the
 * provide_module method since it's called with the provider lock held
 * and dtrace_register() will try to acquire it again.
 */
static void
sdt_kld_load(void *arg __unused, struct linker_file *lf)
{
	struct sdt_provider **prov, **begin, **end;
	struct sdt_probe **probe, **p_begin, **p_end;
	struct sdt_argtype **argtype, **a_begin, **a_end;

	if (linker_file_lookup_set(lf, "sdt_providers_set", &begin, &end, NULL))
		return;
	for (prov = begin; prov < end; prov++)
		sdt_create_provider(*prov);

	if (linker_file_lookup_set(lf, "sdt_probes_set", &p_begin, &p_end,
	    NULL))
		return;
	for (probe = p_begin; probe < p_end; probe++) {
		(*probe)->sdtp_lf = lf;
		sdt_create_probe(*probe);
		TAILQ_INIT(&(*probe)->argtype_list);
	}

	if (linker_file_lookup_set(lf, "sdt_argtypes_set", &a_begin, &a_end,
	    NULL))
		return;
	for (argtype = a_begin; argtype < a_end; argtype++) {
		(*argtype)->probe->n_args++;
		TAILQ_INSERT_TAIL(&(*argtype)->probe->argtype_list, *argtype,
		    argtype_entry);
	}
}

static void
sdt_kld_unload(void *arg __unused, struct linker_file *lf, int *error __unused)
{
	struct sdt_provider *prov, **curr, **begin, **end, *tmp;

	if (*error != 0)
		/* We already have an error, so don't do anything. */
		return;
	else if (linker_file_lookup_set(lf, "sdt_providers_set", &begin, &end, NULL))
		/* No DTrace providers are declared in this file. */
		return;

	/*
	 * Go through all the providers declared in this linker file and
	 * unregister any that aren't declared in another loaded file.
	 */
	for (curr = begin; curr < end; curr++) {
		TAILQ_FOREACH_SAFE(prov, &sdt_prov_list, prov_entry, tmp) {
			if (strcmp(prov->name, (*curr)->name) == 0) {
				if (prov->sdt_refs == 1) {
					TAILQ_REMOVE(&sdt_prov_list, prov,
					    prov_entry);
					dtrace_unregister(prov->id);
					free(prov->name, M_SDT);
					free(prov, M_SDT);
				} else
					prov->sdt_refs--;
				break;
			}
		}
	}
}

static int
sdt_linker_file_cb(linker_file_t lf, void *arg __unused)
{

	sdt_kld_load(NULL, lf);

	return (0);
}

static void
sdt_load(void *arg __unused)
{

	TAILQ_INIT(&sdt_prov_list);

	/* Create the /dev/dtrace/sdt entry. */
	sdt_cdev = make_dev(&sdt_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "dtrace/sdt");

	sdt_probe_func = dtrace_probe;

	sdt_kld_load_tag = EVENTHANDLER_REGISTER(kld_load, sdt_kld_load, NULL,
	    EVENTHANDLER_PRI_ANY);
	sdt_kld_unload_tag = EVENTHANDLER_REGISTER(kld_unload, sdt_kld_unload,
	    NULL, EVENTHANDLER_PRI_ANY);

	/* Pick up probes from the kernel and already-loaded linker files. */
	linker_file_foreach(sdt_linker_file_cb, NULL);
}

static int
sdt_unload(void *arg __unused)
{
	struct sdt_provider *prov, *tmp;

	EVENTHANDLER_DEREGISTER(kld_load, sdt_kld_load_tag);
	EVENTHANDLER_DEREGISTER(kld_unload, sdt_kld_unload_tag);

	sdt_probe_func = sdt_probe_stub;

	TAILQ_FOREACH_SAFE(prov, &sdt_prov_list, prov_entry, tmp) {
		TAILQ_REMOVE(&sdt_prov_list, prov, prov_entry);
		dtrace_unregister(prov->id);
		free(prov->name, M_SDT);
		free(prov, M_SDT);
	}

	destroy_dev(sdt_cdev);

	return (0);
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
sdt_open(struct cdev *dev __unused, int oflags __unused, int devtype __unused,
    struct thread *td __unused)
{

	return (0);
}

SYSINIT(sdt_load, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, sdt_load, NULL);
SYSUNINIT(sdt_unload, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, sdt_unload, NULL);

DEV_MODULE(sdt, sdt_modevent, NULL);
MODULE_VERSION(sdt, 1);
MODULE_DEPEND(sdt, dtrace, 1, 1, 1);
MODULE_DEPEND(sdt, opensolaris, 1, 1, 1);
