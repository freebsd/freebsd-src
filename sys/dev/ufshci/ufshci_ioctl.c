/*-
 * Copyright (c) 2026, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioccom.h>

#include "ufshci_private.h"
#include "ufshci_ioctl.h"

static d_ioctl_t ufshci_ioctl;

static struct cdevsw ufshci_cdevsw = {
	.d_version = D_VERSION,
	.d_ioctl = ufshci_ioctl,
	.d_name = "ufshci",
};

static int
ufshci_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int flag,
    struct thread *td)
{
	switch (cmd) {
	default:
		return (ENOTTY);
	}
}

int
ufshci_ioctl_construct(struct ufshci_controller *ctrlr, device_t dev)
{
	struct make_dev_args md_args;
	int error;

	/*
	 * Set si_drv1 as the node is created. A separate assignment after
	 * make_dev leaves a window where an open that races the attach finds
	 * it unset.
	 */
	make_dev_args_init(&md_args);
	md_args.mda_devsw = &ufshci_cdevsw;
	md_args.mda_uid = UID_ROOT;
	md_args.mda_gid = GID_WHEEL;
	md_args.mda_mode = 0600;
	md_args.mda_unit = device_get_unit(dev);
	md_args.mda_si_drv1 = ctrlr;

	error = make_dev_s(&md_args, &ctrlr->cdev, "ufshci%d",
	    device_get_unit(dev));
	if (error != 0)
		return (error);

	return (0);
}

void
ufshci_ioctl_destruct(struct ufshci_controller *ctrlr)
{
	if (ctrlr->cdev != NULL) {
		destroy_dev(ctrlr->cdev);
		ctrlr->cdev = NULL;
	}
}
