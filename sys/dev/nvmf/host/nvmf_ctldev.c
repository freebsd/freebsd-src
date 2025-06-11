/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/nv.h>
#include <dev/nvme/nvme.h>
#include <dev/nvmf/nvmf.h>
#include <dev/nvmf/nvmf_transport.h>
#include <dev/nvmf/host/nvmf_var.h>

static struct cdev *nvmf_cdev;

static int
nvmf_handoff_host(struct nvmf_ioc_nv *nv)
{
	nvlist_t *nvl;
	device_t dev;
	int error;

	error = nvmf_copyin_handoff(nv, &nvl);
	if (error != 0)
		return (error);

	bus_topo_lock();
	dev = device_add_child(root_bus, "nvme", DEVICE_UNIT_ANY);
	if (dev == NULL) {
		bus_topo_unlock();
		error = ENXIO;
		goto out;
	}

	device_set_ivars(dev, nvl);
	error = device_probe_and_attach(dev);
	device_set_ivars(dev, NULL);
	if (error != 0)
		device_delete_child(root_bus, dev);
	bus_topo_unlock();

out:
	nvlist_destroy(nvl);
	return (error);
}

static bool
nvmf_matches(device_t dev, char *name)
{
	struct nvmf_softc *sc = device_get_softc(dev);

	if (strcmp(device_get_nameunit(dev), name) == 0)
		return (true);
	if (strcmp(sc->cdata->subnqn, name) == 0)
		return (true);
	return (false);
}

static int
nvmf_disconnect_by_name(char *name)
{
	devclass_t dc;
	device_t dev;
	int error, unit;
	bool found;

	found = false;
	error = 0;
	bus_topo_lock();
	dc = devclass_find("nvme");
	if (dc == NULL)
		goto out;

	for (unit = 0; unit < devclass_get_maxunit(dc); unit++) {
		dev = devclass_get_device(dc, unit);
		if (dev == NULL)
			continue;
		if (device_get_driver(dev) != &nvme_nvmf_driver)
			continue;
		if (device_get_parent(dev) != root_bus)
			continue;
		if (name != NULL && !nvmf_matches(dev, name))
			continue;

		error = device_delete_child(root_bus, dev);
		if (error != 0)
			break;
		found = true;
	}
out:
	bus_topo_unlock();
	if (error == 0 && !found)
		error = ENOENT;
	return (error);
}

static int
nvmf_disconnect_host(const char **namep)
{
	char *name;
	int error;

	name = malloc(PATH_MAX, M_NVMF, M_WAITOK);
	error = copyinstr(*namep, name, PATH_MAX, NULL);
	if (error == 0)
		error = nvmf_disconnect_by_name(name);
	free(name, M_NVMF);
	return (error);
}

static int
nvmf_ctl_ioctl(struct cdev *dev, u_long cmd, caddr_t arg, int flag,
    struct thread *td)
{
	switch (cmd) {
	case NVMF_HANDOFF_HOST:
		return (nvmf_handoff_host((struct nvmf_ioc_nv *)arg));
	case NVMF_DISCONNECT_HOST:
		return (nvmf_disconnect_host((const char **)arg));
	case NVMF_DISCONNECT_ALL:
		return (nvmf_disconnect_by_name(NULL));
	default:
		return (ENOTTY);
	}
}

static struct cdevsw nvmf_ctl_cdevsw = {
	.d_version = D_VERSION,
	.d_ioctl = nvmf_ctl_ioctl
};

int
nvmf_ctl_load(void)
{
	struct make_dev_args mda;
	int error;

	make_dev_args_init(&mda);
	mda.mda_devsw = &nvmf_ctl_cdevsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_WHEEL;
	mda.mda_mode = 0600;
	error = make_dev_s(&mda, &nvmf_cdev, "nvmf");
	if (error != 0)
		nvmf_cdev = NULL;
	return (error);
}

void
nvmf_ctl_unload(void)
{
	if (nvmf_cdev != NULL) {
		destroy_dev(nvmf_cdev);
		nvmf_cdev = NULL;
	}
}
