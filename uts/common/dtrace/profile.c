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
 */
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */


#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cpuvar.h>
#include <sys/kmem.h>
#include <sys/strsubr.h>
#include <sys/dtrace.h>
#include <sys/cyclic.h>
#include <sys/atomic.h>

static dev_info_t *profile_devi;
static dtrace_provider_id_t profile_id;

/*
 * Regardless of platform, the stack frames look like this in the case of the
 * profile provider:
 *
 *	profile_fire
 *	cyclic_expire
 *	cyclic_fire
 *	[ cbe ]
 *	[ interrupt code ]
 *
 * On x86, there are five frames from the generic interrupt code; further, the
 * interrupted instruction appears as its own stack frame, giving us a total of
 * 10.
 *
 * On SPARC, the picture is further complicated because the compiler
 * optimizes away tail-calls -- so the following frames are optimized away:
 *
 * 	profile_fire
 *	cyclic_expire
 *
 * This gives three frames.  However, on DEBUG kernels, the cyclic_expire
 * frame cannot be tail-call eliminated, yielding four frames in this case.
 *
 * All of the above constraints lead to the mess below.  Yes, the profile
 * provider should ideally figure this out on-the-fly by hitting one of its own
 * probes and then walking its own stack trace.  This is complicated, however,
 * and the static definition doesn't seem to be overly brittle.  Still, we
 * allow for a manual override in case we get it completely wrong.
 */
#ifdef __x86
#define	PROF_ARTIFICIAL_FRAMES	10
#else
#ifdef __sparc
#ifdef DEBUG
#define	PROF_ARTIFICIAL_FRAMES	4
#else
#define	PROF_ARTIFICIAL_FRAMES	3
#endif
#endif
#endif

#define	PROF_NAMELEN		15

#define	PROF_PROFILE		0
#define	PROF_TICK		1
#define	PROF_PREFIX_PROFILE	"profile-"
#define	PROF_PREFIX_TICK	"tick-"

typedef struct profile_probe {
	char		prof_name[PROF_NAMELEN];
	dtrace_id_t	prof_id;
	int		prof_kind;
	hrtime_t	prof_interval;
	cyclic_id_t	prof_cyclic;
} profile_probe_t;

typedef struct profile_probe_percpu {
	hrtime_t	profc_expected;
	hrtime_t	profc_interval;
	profile_probe_t	*profc_probe;
} profile_probe_percpu_t;

hrtime_t	profile_interval_min = NANOSEC / 5000;		/* 5000 hz */
int		profile_aframes = 0;				/* override */

static int profile_rates[] = {
    97, 199, 499, 997, 1999,
    4001, 4999, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0
};

static int profile_ticks[] = {
    1, 10, 100, 500, 1000,
    5000, 0, 0, 0, 0,
    0, 0, 0, 0, 0
};

/*
 * profile_max defines the upper bound on the number of profile probes that
 * can exist (this is to prevent malicious or clumsy users from exhausing
 * system resources by creating a slew of profile probes). At mod load time,
 * this gets its value from PROFILE_MAX_DEFAULT or profile-max-probes if it's
 * present in the profile.conf file.
 */
#define	PROFILE_MAX_DEFAULT	1000	/* default max. number of probes */
static uint32_t profile_max;		/* maximum number of profile probes */
static uint32_t profile_total;	/* current number of profile probes */

static void
profile_fire(void *arg)
{
	profile_probe_percpu_t *pcpu = arg;
	profile_probe_t *prof = pcpu->profc_probe;
	hrtime_t late;

	late = dtrace_gethrtime() - pcpu->profc_expected;
	pcpu->profc_expected += pcpu->profc_interval;

	dtrace_probe(prof->prof_id, CPU->cpu_profile_pc,
	    CPU->cpu_profile_upc, late, 0, 0);
}

static void
profile_tick(void *arg)
{
	profile_probe_t *prof = arg;

	dtrace_probe(prof->prof_id, CPU->cpu_profile_pc,
	    CPU->cpu_profile_upc, 0, 0, 0);
}

static void
profile_create(hrtime_t interval, const char *name, int kind)
{
	profile_probe_t *prof;
	int nr_frames = PROF_ARTIFICIAL_FRAMES + dtrace_mach_aframes();

	if (profile_aframes)
		nr_frames = profile_aframes;

	if (interval < profile_interval_min)
		return;

	if (dtrace_probe_lookup(profile_id, NULL, NULL, name) != 0)
		return;

	atomic_add_32(&profile_total, 1);
	if (profile_total > profile_max) {
		atomic_add_32(&profile_total, -1);
		return;
	}

	prof = kmem_zalloc(sizeof (profile_probe_t), KM_SLEEP);
	(void) strcpy(prof->prof_name, name);
	prof->prof_interval = interval;
	prof->prof_cyclic = CYCLIC_NONE;
	prof->prof_kind = kind;
	prof->prof_id = dtrace_probe_create(profile_id,
	    NULL, NULL, name, nr_frames, prof);
}

/*ARGSUSED*/
static void
profile_provide(void *arg, const dtrace_probedesc_t *desc)
{
	int i, j, rate, kind;
	hrtime_t val = 0, mult = 1, len;
	const char *name, *suffix = NULL;

	const struct {
		char *prefix;
		int kind;
	} types[] = {
		{ PROF_PREFIX_PROFILE, PROF_PROFILE },
		{ PROF_PREFIX_TICK, PROF_TICK },
		{ NULL, NULL }
	};

	const struct {
		char *name;
		hrtime_t mult;
	} suffixes[] = {
		{ "ns", 	NANOSEC / NANOSEC },
		{ "nsec",	NANOSEC / NANOSEC },
		{ "us",		NANOSEC / MICROSEC },
		{ "usec",	NANOSEC / MICROSEC },
		{ "ms",		NANOSEC / MILLISEC },
		{ "msec",	NANOSEC / MILLISEC },
		{ "s",		NANOSEC / SEC },
		{ "sec",	NANOSEC / SEC },
		{ "m",		NANOSEC * (hrtime_t)60 },
		{ "min",	NANOSEC * (hrtime_t)60 },
		{ "h",		NANOSEC * (hrtime_t)(60 * 60) },
		{ "hour",	NANOSEC * (hrtime_t)(60 * 60) },
		{ "d",		NANOSEC * (hrtime_t)(24 * 60 * 60) },
		{ "day",	NANOSEC * (hrtime_t)(24 * 60 * 60) },
		{ "hz",		0 },
		{ NULL }
	};

	if (desc == NULL) {
		char n[PROF_NAMELEN];

		/*
		 * If no description was provided, provide all of our probes.
		 */
		for (i = 0; i < sizeof (profile_rates) / sizeof (int); i++) {
			if ((rate = profile_rates[i]) == 0)
				continue;

			(void) snprintf(n, PROF_NAMELEN, "%s%d",
			    PROF_PREFIX_PROFILE, rate);
			profile_create(NANOSEC / rate, n, PROF_PROFILE);
		}

		for (i = 0; i < sizeof (profile_ticks) / sizeof (int); i++) {
			if ((rate = profile_ticks[i]) == 0)
				continue;

			(void) snprintf(n, PROF_NAMELEN, "%s%d",
			    PROF_PREFIX_TICK, rate);
			profile_create(NANOSEC / rate, n, PROF_TICK);
		}

		return;
	}

	name = desc->dtpd_name;

	for (i = 0; types[i].prefix != NULL; i++) {
		len = strlen(types[i].prefix);

		if (strncmp(name, types[i].prefix, len) != 0)
			continue;
		break;
	}

	if (types[i].prefix == NULL)
		return;

	kind = types[i].kind;
	j = strlen(name) - len;

	/*
	 * We need to start before any time suffix.
	 */
	for (j = strlen(name); j >= len; j--) {
		if (name[j] >= '0' && name[j] <= '9')
			break;
		suffix = &name[j];
	}

	ASSERT(suffix != NULL);

	/*
	 * Now determine the numerical value present in the probe name.
	 */
	for (; j >= len; j--) {
		if (name[j] < '0' || name[j] > '9')
			return;

		val += (name[j] - '0') * mult;
		mult *= (hrtime_t)10;
	}

	if (val == 0)
		return;

	/*
	 * Look-up the suffix to determine the multiplier.
	 */
	for (i = 0, mult = 0; suffixes[i].name != NULL; i++) {
		if (strcasecmp(suffixes[i].name, suffix) == 0) {
			mult = suffixes[i].mult;
			break;
		}
	}

	if (suffixes[i].name == NULL && *suffix != '\0')
		return;

	if (mult == 0) {
		/*
		 * The default is frequency-per-second.
		 */
		val = NANOSEC / val;
	} else {
		val *= mult;
	}

	profile_create(val, name, kind);
}

/*ARGSUSED*/
static void
profile_destroy(void *arg, dtrace_id_t id, void *parg)
{
	profile_probe_t *prof = parg;

	ASSERT(prof->prof_cyclic == CYCLIC_NONE);
	kmem_free(prof, sizeof (profile_probe_t));

	ASSERT(profile_total >= 1);
	atomic_add_32(&profile_total, -1);
}

/*ARGSUSED*/
static void
profile_online(void *arg, cpu_t *cpu, cyc_handler_t *hdlr, cyc_time_t *when)
{
	profile_probe_t *prof = arg;
	profile_probe_percpu_t *pcpu;

	pcpu = kmem_zalloc(sizeof (profile_probe_percpu_t), KM_SLEEP);
	pcpu->profc_probe = prof;

	hdlr->cyh_func = profile_fire;
	hdlr->cyh_arg = pcpu;
	hdlr->cyh_level = CY_HIGH_LEVEL;

	when->cyt_interval = prof->prof_interval;
	when->cyt_when = dtrace_gethrtime() + when->cyt_interval;

	pcpu->profc_expected = when->cyt_when;
	pcpu->profc_interval = when->cyt_interval;
}

/*ARGSUSED*/
static void
profile_offline(void *arg, cpu_t *cpu, void *oarg)
{
	profile_probe_percpu_t *pcpu = oarg;

	ASSERT(pcpu->profc_probe == arg);
	kmem_free(pcpu, sizeof (profile_probe_percpu_t));
}

/*ARGSUSED*/
static int
profile_enable(void *arg, dtrace_id_t id, void *parg)
{
	profile_probe_t *prof = parg;
	cyc_omni_handler_t omni;
	cyc_handler_t hdlr;
	cyc_time_t when;

	ASSERT(prof->prof_interval != 0);
	ASSERT(MUTEX_HELD(&cpu_lock));

	if (prof->prof_kind == PROF_TICK) {
		hdlr.cyh_func = profile_tick;
		hdlr.cyh_arg = prof;
		hdlr.cyh_level = CY_HIGH_LEVEL;

		when.cyt_interval = prof->prof_interval;
		when.cyt_when = dtrace_gethrtime() + when.cyt_interval;
	} else {
		ASSERT(prof->prof_kind == PROF_PROFILE);
		omni.cyo_online = profile_online;
		omni.cyo_offline = profile_offline;
		omni.cyo_arg = prof;
	}

	if (prof->prof_kind == PROF_TICK) {
		prof->prof_cyclic = cyclic_add(&hdlr, &when);
	} else {
		prof->prof_cyclic = cyclic_add_omni(&omni);
	}
	return (0);
}

/*ARGSUSED*/
static void
profile_disable(void *arg, dtrace_id_t id, void *parg)
{
	profile_probe_t *prof = parg;

	ASSERT(prof->prof_cyclic != CYCLIC_NONE);
	ASSERT(MUTEX_HELD(&cpu_lock));

	cyclic_remove(prof->prof_cyclic);
	prof->prof_cyclic = CYCLIC_NONE;
}

/*ARGSUSED*/
static int
profile_usermode(void *arg, dtrace_id_t id, void *parg)
{
	return (CPU->cpu_profile_pc == 0);
}

static dtrace_pattr_t profile_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_UNSTABLE, DTRACE_STABILITY_UNSTABLE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
};

static dtrace_pops_t profile_pops = {
	profile_provide,
	NULL,
	profile_enable,
	profile_disable,
	NULL,
	NULL,
	NULL,
	NULL,
	profile_usermode,
	profile_destroy
};

static int
profile_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(devi, "profile", S_IFCHR, 0,
	    DDI_PSEUDO, NULL) == DDI_FAILURE ||
	    dtrace_register("profile", &profile_attr,
	    DTRACE_PRIV_KERNEL | DTRACE_PRIV_USER, NULL,
	    &profile_pops, NULL, &profile_id) != 0) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}

	profile_max = ddi_getprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
	    "profile-max-probes", PROFILE_MAX_DEFAULT);

	ddi_report_dev(devi);
	profile_devi = devi;
	return (DDI_SUCCESS);
}

static int
profile_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (dtrace_unregister(profile_id) != 0)
		return (DDI_FAILURE);

	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
profile_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)profile_devi;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*ARGSUSED*/
static int
profile_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	return (0);
}

static struct cb_ops profile_cb_ops = {
	profile_open,		/* open */
	nodev,			/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops profile_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	profile_info,		/* get_dev_info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	profile_attach,		/* attach */
	profile_detach,		/* detach */
	nodev,			/* reset */
	&profile_cb_ops,	/* driver operations */
	NULL,			/* bus operations */
	nodev,			/* dev power */
	ddi_quiesce_not_needed,		/* quiesce */
};

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,		/* module type (this is a pseudo driver) */
	"Profile Interrupt Tracing",	/* name of module */
	&profile_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}
