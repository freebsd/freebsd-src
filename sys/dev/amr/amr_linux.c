/*-
 * Copyright (c) 2005 Paul Saab
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bio.h>

#if defined(__amd64__) /* Assume amd64 wants 32 bit Linux */
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif
#include <compat/linux/linux_ioctl.h>

#include <sys/bus.h>
#include <sys/stat.h>

#include <machine/bus.h>

#include <dev/amr/amrreg.h>
#include <dev/amr/amrvar.h>

/* There are multiple ioctl number ranges that need to be handled */
#define AMR_LINUX_IOCTL_MIN  0x00000
#define AMR_LINUX_IOCTL_MAX  0x50000

static linux_ioctl_function_t amr_linux_ioctl;
static struct linux_ioctl_handler amr_linux_handler = {amr_linux_ioctl,
						       AMR_LINUX_IOCTL_MIN,
						       AMR_LINUX_IOCTL_MAX};

SYSINIT  (amr_register,   SI_SUB_KLD, SI_ORDER_MIDDLE,
	  linux_ioctl_register_handler, &amr_linux_handler);
SYSUNINIT(amr_unregister, SI_SUB_KLD, SI_ORDER_MIDDLE,
	  linux_ioctl_unregister_handler, &amr_linux_handler);

static d_open_t		amr_linux_open;
static d_close_t	amr_linux_close;

static int		amr_linux_isopen;
static struct cdev *	amr_linux_dev_t;

static struct cdevsw megadev_cdevsw = {
        .d_version =    D_VERSION,
        .d_flags =      D_NEEDGIANT,
        .d_open =       amr_linux_open,
        .d_close =      amr_linux_close,
        .d_name =       "megadev",
};


static int
amr_linux_open(struct cdev * dev, int flags, int fmt, d_thread_t *td)
{

	amr_linux_isopen++;
	return (0);
};

static int
amr_linux_close(struct cdev * dev, int flags, int fmt, d_thread_t *td)
{

	amr_linux_isopen--;
	return (0);
};

static int
amr_linux_init(void)
{
	devclass_t		devclass;
	struct amr_softc	*sc;
	int			i, linux_no_adapters, max_unit;

	devclass = devclass_find("amr");
	if (devclass == NULL)
		return (0);

	max_unit = devclass_get_maxunit(devclass);
	if (max_unit == 0)
		return (0);

	for (i = 0; i < max_unit; i++) {
		sc = devclass_get_softc(devclass, i);
		if (sc == NULL)
			break;
	}

	linux_no_adapters = i;
	for (i = 0; i < linux_no_adapters; i++) {
		sc = devclass_get_softc(devclass, i);
		if (sc == NULL)
			break;
		sc->amr_linux_no_adapters = linux_no_adapters;
	}

	return (linux_no_adapters);
}

static int
amr_linux_modevent(module_t mod, int cmd, void *data)
{

	switch (cmd) {
	case MOD_LOAD:
		if (amr_linux_init() == 0)
			return (ENXIO);

		if (amr_linux_dev_t)
			return (EEXIST);
		
		amr_linux_dev_t = make_dev(&megadev_cdevsw, 0, UID_ROOT,
		    GID_OPERATOR, S_IRUSR | S_IWUSR, "megadev%d", 0);
		if (amr_linux_dev_t == NULL)
			return (ENXIO);
		break;

	case MOD_UNLOAD:
		if (amr_linux_isopen)
			return (EBUSY);
		if (amr_linux_dev_t)
			destroy_dev(amr_linux_dev_t);
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

static moduledata_t amr_linux_mod = {"amr_linux", amr_linux_modevent, NULL};
DECLARE_MODULE(amr_linux, amr_linux_mod, SI_SUB_PSEUDO, SI_ORDER_MIDDLE);
MODULE_DEPEND(amr, linux, 1, 1, 1);

static int
amr_linux_ioctl(d_thread_t *p, struct linux_ioctl_args *args)
{
	devclass_t		devclass;
	struct amr_softc	*sc;
	struct amr_linux_ioctl	ali;
	int			adapter, error;

	devclass = devclass_find("amr");
	if (devclass == NULL)
		return (ENOENT);

	error = copyin((caddr_t)args->arg, &ali, sizeof(ali));
	if (error)
		return (error);
	if (ali.ui.fcs.opcode == 0x82)
		adapter = 0;
	else
		adapter	= (ali.ui.fcs.adapno) ^ 'm' << 8;

	sc = devclass_get_softc(devclass, adapter);
	if (sc == NULL)
		return (ENOENT);

	return (amr_linux_ioctl_int(sc->amr_dev_t, args->cmd,
	    (caddr_t)args->arg, 0, p));
}
