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
 * Copyright 2024 Mark Johnston <markj@FreeBSD.org>
 */

/*
 * This file contains a reimplementation of the statically-defined tracing (SDT)
 * framework for DTrace. Probes and SDT providers are defined using the macros
 * in sys/sdt.h, which append all the needed structures to linker sets. When
 * this module is loaded, it iterates over all of the loaded modules and
 * registers probes and providers with the DTrace framework based on the
 * contents of these linker sets.
 *
 * A list of SDT providers is maintained here since a provider may span multiple
 * modules. When a kernel module is unloaded, a provider defined in that module
 * is unregistered only if no other modules refer to it. The DTrace framework is
 * responsible for destroying individual probes when a kernel module is
 * unloaded; in particular, probes may not span multiple kernel modules.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/lockstat.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sdt.h>

#include <sys/dtrace.h>
#include <sys/dtrace_bsd.h>

#include <cddl/dev/dtrace/dtrace_cddl.h>

/* DTrace methods. */
static void	sdt_getargdesc(void *, dtrace_id_t, void *, dtrace_argdesc_t *);
static uint64_t	sdt_getargval(void *, dtrace_id_t, void *, int, int);
static void	sdt_provide_probes(void *, dtrace_probedesc_t *);
static void	sdt_destroy(void *, dtrace_id_t, void *);
static void	sdt_enable(void *, dtrace_id_t, void *);
static void	sdt_disable(void *, dtrace_id_t, void *);

static void	sdt_load(void);
static int	sdt_unload(void);
static void	sdt_create_provider(struct sdt_provider *);
static void	sdt_create_probe(struct sdt_probe *);
static void	sdt_kld_load(void *, struct linker_file *);
static void	sdt_kld_unload_try(void *, struct linker_file *, int *);

static MALLOC_DEFINE(M_SDT, "SDT", "DTrace SDT providers");

static int sdt_probes_enabled_count;
static int lockstat_enabled_count;

static dtrace_pattr_t sdt_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static dtrace_pops_t sdt_pops = {
	.dtps_provide =		sdt_provide_probes,
	.dtps_provide_module =	NULL,
	.dtps_enable =		sdt_enable,
	.dtps_disable =		sdt_disable,
	.dtps_suspend =		NULL,
	.dtps_resume =		NULL,
	.dtps_getargdesc =	sdt_getargdesc,
	.dtps_getargval =	sdt_getargval,
	.dtps_usermode =	NULL,
	.dtps_destroy =		sdt_destroy,
};

static TAILQ_HEAD(, sdt_provider) sdt_prov_list;

static eventhandler_tag	sdt_kld_load_tag;
static eventhandler_tag	sdt_kld_unload_try_tag;

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
	const char *from;
	char *to;
	size_t len;
	int aframes;

	if (probe->version != (int)sizeof(*probe)) {
		printf("ignoring probe %p, version %u expected %u\n",
		    probe, probe->version, (int)sizeof(*probe));
		return;
	}

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
	if (func[0] == '\0')
		strcpy(func, "none");

	from = probe->name;
	to = name;
	for (len = 0; len < (sizeof(name) - 1) && *from != '\0';
	    len++, from++, to++) {
		if (from[0] == '_' && from[1] == '_') {
			*to = '-';
			from++;
		} else
			*to = *from;
	}
	*to = '\0';

	if (dtrace_probe_lookup(prov->id, mod, func, name) != DTRACE_IDNONE)
		return;

	aframes = 1; /* unwind past sdt_probe() */
	if (strcmp(prov->name, "lockstat") == 0) {
		/*
		 * Locking primitives instrumented by lockstat automatically
		 * disable inlining.  Step forward an extra frame so that DTrace
		 * variables like "caller" provide the function trying to
		 * acquire or release the lock rather than an internal function.
		 */
		aframes++;
	}
	(void)dtrace_probe_create(prov->id, mod, func, name, aframes, probe);
}

/*
 * Probes are created through the SDT module load/unload hook, so this function
 * has nothing to do. It only exists because the DTrace provider framework
 * requires one of provide_probes and provide_module to be defined.
 */
static void
sdt_provide_probes(void *arg, dtrace_probedesc_t *desc)
{
}

struct sdt_enable_cb_arg {
	struct sdt_probe *probe;
	int cpu;
	int arrived;
	int done;
	bool enable;
};

static void
sdt_probe_update_cb(void *_arg)
{
	struct sdt_enable_cb_arg *arg;
	struct sdt_tracepoint *tp;

	arg = _arg;
	if (arg->cpu != curcpu) {
		atomic_add_rel_int(&arg->arrived, 1);
		while (atomic_load_acq_int(&arg->done) == 0)
			cpu_spinwait();
		return;
	} else {
		while (atomic_load_acq_int(&arg->arrived) != mp_ncpus - 1)
			cpu_spinwait();
	}

	STAILQ_FOREACH(tp, &arg->probe->tracepoint_list, tracepoint_entry) {
		if (arg->enable)
			sdt_tracepoint_patch(tp->patchpoint, tp->target);
		else
			sdt_tracepoint_restore(tp->patchpoint);
	}

	atomic_store_rel_int(&arg->done, 1);
}

static void
sdt_probe_update(struct sdt_probe *probe, bool enable)
{
	struct sdt_enable_cb_arg cbarg;

	sched_pin();
	cbarg.probe = probe;
	cbarg.cpu = curcpu;
	atomic_store_rel_int(&cbarg.arrived, 0);
	atomic_store_rel_int(&cbarg.done, 0);
	cbarg.enable = enable;
	smp_rendezvous(NULL, sdt_probe_update_cb, NULL, &cbarg);
	sched_unpin();
}

static void
sdt_enable(void *arg __unused, dtrace_id_t id, void *parg)
{
	struct sdt_probe *probe;

	probe = parg;

	probe->id = id;
	probe->sdtp_lf->nenabled++;
	if (strcmp(probe->prov->name, "lockstat") == 0) {
		lockstat_enabled_count++;
		if (lockstat_enabled_count == 1)
			lockstat_enabled = true;
	}
	sdt_probes_enabled_count++;
	if (sdt_probes_enabled_count == 1)
		sdt_probes_enabled = true;

	sdt_probe_update(probe, true);
}

static void
sdt_disable(void *arg __unused, dtrace_id_t id, void *parg)
{
	struct sdt_probe *probe;

	probe = parg;
	KASSERT(probe->sdtp_lf->nenabled > 0, ("no probes enabled"));

	sdt_probe_update(probe, false);

	sdt_probes_enabled_count--;
	if (sdt_probes_enabled_count == 0)
		sdt_probes_enabled = false;
	if (strcmp(probe->prov->name, "lockstat") == 0) {
		lockstat_enabled_count--;
		if (lockstat_enabled_count == 0)
			lockstat_enabled = false;
	}
	probe->id = 0;
	probe->sdtp_lf->nenabled--;
}

static void
sdt_getargdesc(void *arg, dtrace_id_t id, void *parg, dtrace_argdesc_t *desc)
{
	struct sdt_argtype *argtype;
	struct sdt_probe *probe = parg;

	if (desc->dtargd_ndx >= probe->n_args) {
		desc->dtargd_ndx = DTRACE_ARGNONE;
		return;
	}

	TAILQ_FOREACH(argtype, &probe->argtype_list, argtype_entry) {
		if (desc->dtargd_ndx == argtype->ndx) {
			desc->dtargd_mapping = desc->dtargd_ndx;
			if (argtype->type == NULL) {
				desc->dtargd_native[0] = '\0';
				desc->dtargd_xlate[0] = '\0';
				continue;
			}
			strlcpy(desc->dtargd_native, argtype->type,
			    sizeof(desc->dtargd_native));
			if (argtype->xtype != NULL)
				strlcpy(desc->dtargd_xlate, argtype->xtype,
				    sizeof(desc->dtargd_xlate));
		}
	}
}

/*
 * Fetch arguments beyond the first five passed directly to dtrace_probe().
 * FreeBSD's SDT implement currently only supports up to 6 arguments, so we just
 * need to handle arg5 here.
 */
static uint64_t
sdt_getargval(void *arg __unused, dtrace_id_t id __unused,
    void *parg __unused, int argno, int aframes __unused)
{
	if (argno != 5) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return (0);
	} else {
		return (curthread->t_dtrace_sdt_arg[argno - 5]);
	}
}

static void
sdt_destroy(void *arg, dtrace_id_t id, void *parg)
{
}

static void
sdt_kld_load_providers(struct linker_file *lf)
{
	struct sdt_provider **prov, **begin, **end;

	if (linker_file_lookup_set(lf, "sdt_providers_set", &begin, &end,
	    NULL) == 0) {
		for (prov = begin; prov < end; prov++)
			sdt_create_provider(*prov);
	}
}

static void
sdt_kld_load_probes(struct linker_file *lf)
{
	struct sdt_probe **p_begin, **p_end;
	struct sdt_argtype **a_begin, **a_end;
	struct sdt_tracepoint *tp_begin, *tp_end;

	if (linker_file_lookup_set(lf, "sdt_probes_set", &p_begin, &p_end,
	    NULL) == 0) {
		for (struct sdt_probe **probe = p_begin; probe < p_end;
		    probe++) {
			(*probe)->sdtp_lf = lf;
			sdt_create_probe(*probe);
			TAILQ_INIT(&(*probe)->argtype_list);
			STAILQ_INIT(&(*probe)->tracepoint_list);
		}
	}

	if (linker_file_lookup_set(lf, "sdt_argtypes_set", &a_begin, &a_end,
	    NULL) == 0) {
		for (struct sdt_argtype **argtype = a_begin; argtype < a_end;
		    argtype++) {
			(*argtype)->probe->n_args++;
			TAILQ_INSERT_TAIL(&(*argtype)->probe->argtype_list,
			    *argtype, argtype_entry);
		}
	}

	if (linker_file_lookup_set(lf, __XSTRING(_SDT_TRACEPOINT_SET),
	    &tp_begin, &tp_end, NULL) == 0) {
		for (struct sdt_tracepoint *tp = tp_begin; tp < tp_end; tp++) {
			if (!sdt_tracepoint_valid(tp->patchpoint, tp->target)) {
				printf(
			    "invalid tracepoint %#jx->%#jx for %s:%s:%s:%s\n",
				    (uintmax_t)tp->patchpoint,
				    (uintmax_t)tp->target,
				    tp->probe->prov->name, tp->probe->mod,
				    tp->probe->func, tp->probe->name);
				continue;
			}
			STAILQ_INSERT_TAIL(&tp->probe->tracepoint_list, tp,
			    tracepoint_entry);
		}
	}
}

/*
 * Called from the kernel linker when a module is loaded, before
 * dtrace_module_loaded() is called. This is done so that it's possible to
 * register new providers when modules are loaded. The DTrace framework
 * explicitly disallows calling into the framework from the provide_module
 * provider method, so we cannot do this there.
 */
static void
sdt_kld_load(void *arg __unused, struct linker_file *lf)
{
	sdt_kld_load_providers(lf);
	sdt_kld_load_probes(lf);
}

static bool
sdt_kld_unload_providers(struct linker_file *lf)
{
	struct sdt_provider *prov, **curr, **begin, **end, *tmp;

	if (linker_file_lookup_set(lf, "sdt_providers_set", &begin, &end,
	    NULL))
		/* No DTrace providers are declared in this file. */
		return (true);

	/*
	 * Go through all the providers declared in this linker file and
	 * unregister any that aren't declared in another loaded file.
	 */
	for (curr = begin; curr < end; curr++) {
		TAILQ_FOREACH_SAFE(prov, &sdt_prov_list, prov_entry, tmp) {
			if (strcmp(prov->name, (*curr)->name) != 0)
				continue;

			if (prov->sdt_refs == 1) {
				if (dtrace_unregister(prov->id) != 0) {
					return (false);
				}
				TAILQ_REMOVE(&sdt_prov_list, prov, prov_entry);
				free(prov->name, M_SDT);
				free(prov, M_SDT);
			} else
				prov->sdt_refs--;
			break;
		}
	}

	return (true);
}

static bool
sdt_kld_unload_probes(struct linker_file *lf)
{
	struct sdt_probe **p_begin, **p_end;
	struct sdt_argtype **a_begin, **a_end;
	struct sdt_tracepoint *tp_begin, *tp_end;

	if (linker_file_lookup_set(lf, __XSTRING(_SDT_TRACEPOINT_SET),
	    &tp_begin, &tp_end, NULL) == 0) {
		for (struct sdt_tracepoint *tp = tp_begin; tp < tp_end; tp++) {
			struct sdt_tracepoint *tp2;

			if (!sdt_tracepoint_valid(tp->patchpoint, tp->target))
				continue;

			/* Only remove the entry if it is in the list. */
			tp2 = STAILQ_FIRST(&tp->probe->tracepoint_list);
			if (tp2 == tp) {
				STAILQ_REMOVE_HEAD(&tp->probe->tracepoint_list,
				    tracepoint_entry);
			} else if (tp2 != NULL) {
				struct sdt_tracepoint *tp3;

				for (;;) {
					tp3 = STAILQ_NEXT(tp2,
					    tracepoint_entry);
					if (tp3 == NULL)
						break;
					if (tp3 == tp) {
						STAILQ_REMOVE_AFTER(
						    &tp->probe->tracepoint_list,
						    tp2, tracepoint_entry);
						break;
					}
					tp2 = tp3;
				}
			}
		}
	}

	if (linker_file_lookup_set(lf, "sdt_argtypes_set", &a_begin, &a_end,
	    NULL) == 0) {
		for (struct sdt_argtype **argtype = a_begin; argtype < a_end;
		    argtype++) {
			struct sdt_argtype *argtype2;

			/* Only remove the entry if it is in the list. */
			TAILQ_FOREACH(argtype2,
			    &(*argtype)->probe->argtype_list, argtype_entry) {
				if (argtype2 == *argtype) {
					(*argtype)->probe->n_args--;
					TAILQ_REMOVE(
					    &(*argtype)->probe->argtype_list,
					    *argtype, argtype_entry);
					break;
				}
			}
		}
	}

	if (linker_file_lookup_set(lf, "sdt_probes_set", &p_begin, &p_end,
	    NULL) == 0) {
		for (struct sdt_probe **probe = p_begin; probe < p_end;
		    probe++) {
			if ((*probe)->sdtp_lf == lf) {
				if (!TAILQ_EMPTY(&(*probe)->argtype_list))
					return (false);
				if (!STAILQ_EMPTY(&(*probe)->tracepoint_list))
					return (false);

				/*
				 * Don't destroy the probe as there
				 * might be multiple instances of the
				 * same probe in different modules.
				 */
			}
		}
	}

	return (true);
}

static void
sdt_kld_unload_try(void *arg __unused, struct linker_file *lf, int *error)
{
	if (*error != 0)
		/* We already have an error, so don't do anything. */
		return;

	if (!sdt_kld_unload_probes(lf))
		*error = 1;
	else if (!sdt_kld_unload_providers(lf))
		*error = 1;
}

static int
sdt_load_providers_cb(linker_file_t lf, void *arg __unused)
{
	sdt_kld_load_providers(lf);
	return (0);
}

static int
sdt_load_probes_cb(linker_file_t lf, void *arg __unused)
{
	sdt_kld_load_probes(lf);
	return (0);
}

static void
sdt_dtrace_probe(dtrace_id_t id, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5)
{
	curthread->t_dtrace_sdt_arg[0] = arg5;
	dtrace_probe(id, arg0, arg1, arg2, arg3, arg4);
}

static void
sdt_load(void)
{

	TAILQ_INIT(&sdt_prov_list);

	sdt_probe_func = sdt_dtrace_probe;

	sdt_kld_load_tag = EVENTHANDLER_REGISTER(kld_load, sdt_kld_load, NULL,
	    EVENTHANDLER_PRI_ANY);
	sdt_kld_unload_try_tag = EVENTHANDLER_REGISTER(kld_unload_try,
	    sdt_kld_unload_try, NULL, EVENTHANDLER_PRI_ANY);

	/*
	 * Pick up probes from the kernel and already-loaded linker files.
	 * Define providers in a separate pass since a linker file may be using
	 * providers defined in a file that appears later in the list.
	 */
	linker_file_foreach(sdt_load_providers_cb, NULL);
	linker_file_foreach(sdt_load_probes_cb, NULL);
}

static int
sdt_unload(void)
{
	struct sdt_provider *prov, *tmp;
	int ret;

	EVENTHANDLER_DEREGISTER(kld_load, sdt_kld_load_tag);
	EVENTHANDLER_DEREGISTER(kld_unload_try, sdt_kld_unload_try_tag);

	sdt_probe_func = sdt_probe_stub;

	TAILQ_FOREACH_SAFE(prov, &sdt_prov_list, prov_entry, tmp) {
		ret = dtrace_unregister(prov->id);
		if (ret != 0)
			return (ret);
		TAILQ_REMOVE(&sdt_prov_list, prov, prov_entry);
		free(prov->name, M_SDT);
		free(prov, M_SDT);
	}

	return (0);
}

static int
sdt_modevent(module_t mod __unused, int type, void *data __unused)
{
	switch (type) {
	case MOD_LOAD:
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

SYSINIT(sdt_load, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, sdt_load, NULL);
SYSUNINIT(sdt_unload, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, sdt_unload, NULL);

DEV_MODULE(sdt, sdt_modevent, NULL);
MODULE_VERSION(sdt, 1);
MODULE_DEPEND(sdt, dtrace, 1, 1, 1);
MODULE_DEPEND(sdt, opensolaris, 1, 1, 1);
