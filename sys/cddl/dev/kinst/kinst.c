/*
 * SPDX-License-Identifier: CDDL 1.0
 *
 * Copyright 2022 Christos Margiolis <christos@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/module.h>

#include <sys/dtrace.h>

#include "kinst.h"

MALLOC_DEFINE(M_KINST, "kinst", "Kernel Instruction Tracing");

static d_open_t		kinst_open;
static d_close_t	kinst_close;
static d_ioctl_t	kinst_ioctl;

static void	kinst_provide_module(void *, modctl_t *);
static void	kinst_getargdesc(void *, dtrace_id_t, void *,
		    dtrace_argdesc_t *);
static void	kinst_destroy(void *, dtrace_id_t, void *);
static void	kinst_enable(void *, dtrace_id_t, void *);
static void	kinst_disable(void *, dtrace_id_t, void *);
static int	kinst_load(void *);
static int	kinst_unload(void *);
static int	kinst_modevent(module_t, int, void *);

static dtrace_pattr_t kinst_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static dtrace_pops_t kinst_pops = {
	.dtps_provide		= NULL,
	.dtps_provide_module	= kinst_provide_module,
	.dtps_enable		= kinst_enable,
	.dtps_disable		= kinst_disable,
	.dtps_suspend		= NULL,
	.dtps_resume		= NULL,
	.dtps_getargdesc	= kinst_getargdesc,
	.dtps_getargval		= NULL,
	.dtps_usermode		= NULL,
	.dtps_destroy		= kinst_destroy
};

static struct cdevsw kinst_cdevsw = {
	.d_name			= "kinst",
	.d_version		= D_VERSION,
	.d_flags		= D_TRACKCLOSE,
	.d_open			= kinst_open,
	.d_close		= kinst_close,
	.d_ioctl		= kinst_ioctl,
};

static dtrace_provider_id_t	kinst_id;
struct kinst_probe_list	*kinst_probetab;
static struct cdev	*kinst_cdev;

void
kinst_probe_create(struct kinst_probe *kp, linker_file_t lf)
{
	kp->kp_id = dtrace_probe_create(kinst_id, lf->filename,
	    kp->kp_func, kp->kp_name, 3, kp);

	LIST_INSERT_HEAD(KINST_GETPROBE(kp->kp_patchpoint), kp, kp_hashnext);
}

static int
kinst_open(struct cdev *dev __unused, int oflags __unused, int devtype __unused,
    struct thread *td __unused)
{
	return (0);
}

static int
kinst_close(struct cdev *dev __unused, int fflag __unused, int devtype __unused,
    struct thread *td __unused)
{
	dtrace_condense(kinst_id);
	return (0);
}

static int
kinst_linker_file_cb(linker_file_t lf, void *arg)
{
	dtrace_kinst_probedesc_t *pd;

	pd = arg;
	if (pd->kpd_mod[0] != '\0' && strcmp(pd->kpd_mod, lf->filename) != 0)
		return (0);

	/*
	 * Invoke kinst_make_probe_function() once for each function symbol in
	 * the module "lf".
	 */
	return (linker_file_function_listall(lf, kinst_make_probe, arg));
}

static int
kinst_ioctl(struct cdev *dev __unused, u_long cmd, caddr_t addr,
    int flags __unused, struct thread *td __unused)
{
	dtrace_kinst_probedesc_t *pd;
	int error = 0;

	switch (cmd) {
	case KINSTIOC_MAKEPROBE:
		pd = (dtrace_kinst_probedesc_t *)addr;
		pd->kpd_func[sizeof(pd->kpd_func) - 1] = '\0';
		pd->kpd_mod[sizeof(pd->kpd_mod) - 1] = '\0';

		/* Loop over all functions in the kernel and loaded modules. */
		error = linker_file_foreach(kinst_linker_file_cb, pd);
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static void
kinst_provide_module(void *arg, modctl_t *lf)
{
}

static void
kinst_getargdesc(void *arg, dtrace_id_t id, void *parg, dtrace_argdesc_t *desc)
{
	desc->dtargd_ndx = DTRACE_ARGNONE;
}

static void
kinst_destroy(void *arg, dtrace_id_t id, void *parg)
{
	struct kinst_probe *kp = parg;

	LIST_REMOVE(kp, kp_hashnext);
	free(kp, M_KINST);
}

static void
kinst_enable(void *arg, dtrace_id_t id, void *parg)
{
	struct kinst_probe *kp = parg;

	kinst_patch_tracepoint(kp, kp->kp_patchval);
}

static void
kinst_disable(void *arg, dtrace_id_t id, void *parg)
{
	struct kinst_probe *kp = parg;

	kinst_patch_tracepoint(kp, kp->kp_savedval);
}

static int
kinst_load(void *dummy)
{
	int error;

	error = kinst_trampoline_init();
	if (error != 0)
		return (error);

	error = dtrace_register("kinst", &kinst_attr, DTRACE_PRIV_USER, NULL,
	    &kinst_pops, NULL, &kinst_id);
	if (error != 0) {
		kinst_trampoline_deinit();
		return (error);
	}
	kinst_probetab = malloc(KINST_PROBETAB_MAX *
	    sizeof(struct kinst_probe_list), M_KINST, M_WAITOK | M_ZERO);
	for (int i = 0; i < KINST_PROBETAB_MAX; i++)
		LIST_INIT(&kinst_probetab[i]);
	kinst_cdev = make_dev(&kinst_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "dtrace/kinst");
	dtrace_invop_add(kinst_invop);
	return (0);
}

static int
kinst_unload(void *dummy)
{
	free(kinst_probetab, M_KINST);
	kinst_trampoline_deinit();
	dtrace_invop_remove(kinst_invop);
	destroy_dev(kinst_cdev);

	return (dtrace_unregister(kinst_id));
}

static int
kinst_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		KINST_LOG(
		    "kinst: This provider is experimental, exercise caution");
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

SYSINIT(kinst_load, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, kinst_load, NULL);
SYSUNINIT(kinst_unload, SI_SUB_DTRACE_PROVIDER, SI_ORDER_ANY, kinst_unload,
    NULL);

DEV_MODULE(kinst, kinst_modevent, NULL);
MODULE_VERSION(kinst, 1);
MODULE_DEPEND(kinst, dtrace, 1, 1, 1);
MODULE_DEPEND(kinst, opensolaris, 1, 1, 1);
