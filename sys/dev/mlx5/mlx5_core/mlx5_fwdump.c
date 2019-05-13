/*-
 * Copyright (c) 2018, Mellanox Technologies, Ltd.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/device.h>
#include <dev/mlx5/mlx5_core/mlx5_core.h>
#include <dev/mlx5/mlx5io.h>

extern const struct mlx5_crspace_regmap mlx5_crspace_regmap_mt4117[];
extern const struct mlx5_crspace_regmap mlx5_crspace_regmap_mt4115[];
extern const struct mlx5_crspace_regmap mlx5_crspace_regmap_connectx5[];

static MALLOC_DEFINE(M_MLX5_DUMP, "MLX5DUMP", "MLX5 Firmware dump");

static unsigned
mlx5_fwdump_getsize(const struct mlx5_crspace_regmap *rege)
{
	const struct mlx5_crspace_regmap *r;
	unsigned sz;

	for (sz = 0, r = rege; r->cnt != 0; r++)
		sz += r->cnt;
	return (sz);
}

static void
mlx5_fwdump_destroy_dd(struct mlx5_core_dev *mdev)
{

	mtx_assert(&mdev->dump_lock, MA_OWNED);
	free(mdev->dump_data, M_MLX5_DUMP);
	mdev->dump_data = NULL;
}

void
mlx5_fwdump_prep(struct mlx5_core_dev *mdev)
{
	int error;

	mdev->dump_data = NULL;
	error = mlx5_vsc_find_cap(mdev);
	if (error != 0) {
		/* Inability to create a firmware dump is not fatal. */
		device_printf((&mdev->pdev->dev)->bsddev, "WARN: "
		    "mlx5_fwdump_prep failed %d\n", error);
		return;
	}
	switch (pci_get_device(mdev->pdev->dev.bsddev)) {
	case 0x1013:
		mdev->dump_rege = mlx5_crspace_regmap_mt4115;
		break;
	case 0x1015:
		mdev->dump_rege = mlx5_crspace_regmap_mt4117;
		break;
	case 0x1017:
	case 0x1019:
		mdev->dump_rege = mlx5_crspace_regmap_connectx5;
		break;
	default:
		return; /* silently fail, do not prevent driver attach */
	}
	mdev->dump_size = mlx5_fwdump_getsize(mdev->dump_rege);
	mdev->dump_data = malloc(mdev->dump_size * sizeof(uint32_t),
	    M_MLX5_DUMP, M_WAITOK | M_ZERO);
	mdev->dump_valid = false;
	mdev->dump_copyout = false;
}

void
mlx5_fwdump(struct mlx5_core_dev *mdev)
{
	const struct mlx5_crspace_regmap *r;
	uint32_t i, ri;
	int error;

	dev_info(&mdev->pdev->dev, "Issuing FW dump\n");
	mtx_lock(&mdev->dump_lock);
	if (mdev->dump_data == NULL)
		goto failed;
	if (mdev->dump_valid) {
		/* only one dump */
		dev_warn(&mdev->pdev->dev,
		    "Only one FW dump can be captured aborting FW dump\n");
		goto failed;
	}

	/* mlx5_vsc already warns, be silent. */
	error = mlx5_vsc_lock(mdev);
	if (error != 0)
		goto failed;
	error = mlx5_vsc_set_space(mdev, MLX5_VSC_DOMAIN_PROTECTED_CRSPACE);
	if (error != 0)
		goto unlock_vsc;
	for (i = 0, r = mdev->dump_rege; r->cnt != 0; r++) {
		for (ri = 0; ri < r->cnt; ri++) {
			error = mlx5_vsc_read(mdev, r->addr + ri * 4,
			    &mdev->dump_data[i]);
			if (error != 0)
				goto unlock_vsc;
			i++;
		}
	}
	mdev->dump_valid = true;
unlock_vsc:
	mlx5_vsc_unlock(mdev);
failed:
	mtx_unlock(&mdev->dump_lock);
}

void
mlx5_fwdump_clean(struct mlx5_core_dev *mdev)
{

	mtx_lock(&mdev->dump_lock);
	while (mdev->dump_copyout)
		msleep(&mdev->dump_copyout, &mdev->dump_lock, 0, "mlx5fwc", 0);
	mlx5_fwdump_destroy_dd(mdev);
	mtx_unlock(&mdev->dump_lock);
}

static int
mlx5_fwdump_reset(struct mlx5_core_dev *mdev)
{
	int error;

	error = 0;
	mtx_lock(&mdev->dump_lock);
	if (mdev->dump_data != NULL) {
		while (mdev->dump_copyout) {
			msleep(&mdev->dump_copyout, &mdev->dump_lock,
			    0, "mlx5fwr", 0);
		}
		mdev->dump_valid = false;
	} else {
		error = ENOENT;
	}
	mtx_unlock(&mdev->dump_lock);
	return (error);
}

static int
mlx5_dbsf_to_core(const struct mlx5_tool_addr *devaddr,
    struct mlx5_core_dev **mdev)
{
	device_t dev;
	struct pci_dev *pdev;

	dev = pci_find_dbsf(devaddr->domain, devaddr->bus, devaddr->slot,
	    devaddr->func);
	if (dev == NULL)
		return (ENOENT);
	if (device_get_devclass(dev) != mlx5_core_driver.bsdclass)
		return (EINVAL);
	pdev = device_get_softc(dev);
	*mdev = pci_get_drvdata(pdev);
	if (*mdev == NULL)
		return (ENOENT);
	return (0);
}

static int
mlx5_fwdump_copyout(struct mlx5_core_dev *mdev, struct mlx5_fwdump_get *fwg)
{
	const struct mlx5_crspace_regmap *r;
	struct mlx5_fwdump_reg rv, *urv;
	uint32_t i, ri;
	int error;

	mtx_lock(&mdev->dump_lock);
	if (mdev->dump_data == NULL) {
		mtx_unlock(&mdev->dump_lock);
		return (ENOENT);
	}
	if (fwg->buf == NULL) {
		fwg->reg_filled = mdev->dump_size;
		mtx_unlock(&mdev->dump_lock);
		return (0);
	}
	if (!mdev->dump_valid) {
		mtx_unlock(&mdev->dump_lock);
		return (ENOENT);
	}
	mdev->dump_copyout = true;
	mtx_unlock(&mdev->dump_lock);

	urv = fwg->buf;
	for (i = 0, r = mdev->dump_rege; r->cnt != 0; r++) {
		for (ri = 0; ri < r->cnt; ri++) {
			if (i >= fwg->reg_cnt)
				goto out;
			rv.addr = r->addr + ri * 4;
			rv.val = mdev->dump_data[i];
			error = copyout(&rv, urv, sizeof(rv));
			if (error != 0)
				return (error);
			urv++;
			i++;
		}
	}
out:
	fwg->reg_filled = i;
	mtx_lock(&mdev->dump_lock);
	mdev->dump_copyout = false;
	wakeup(&mdev->dump_copyout);
	mtx_unlock(&mdev->dump_lock);
	return (0);
}

static int
mlx5_fw_reset(struct mlx5_core_dev *mdev)
{
	device_t dev, bus;
	int error;

	error = -mlx5_set_mfrl_reg(mdev, MLX5_FRL_LEVEL3);
	if (error == 0) {
		dev = mdev->pdev->dev.bsddev;
		mtx_lock(&Giant);
		bus = device_get_parent(dev);
		error = BUS_RESET_CHILD(device_get_parent(bus), bus,
		    DEVF_RESET_DETACH);
		mtx_unlock(&Giant);
	}
	return (error);
}

static int
mlx5_ctl_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct mlx5_core_dev *mdev;
	struct mlx5_fwdump_get *fwg;
	struct mlx5_tool_addr *devaddr;
	struct mlx5_fw_update *fu;
	struct firmware fake_fw;
	int error;

	error = 0;
	switch (cmd) {
	case MLX5_FWDUMP_GET:
		if ((fflag & FREAD) == 0) {
			error = EBADF;
			break;
		}
		fwg = (struct mlx5_fwdump_get *)data;
		devaddr = &fwg->devaddr;
		error = mlx5_dbsf_to_core(devaddr, &mdev);
		if (error != 0)
			break;
		error = mlx5_fwdump_copyout(mdev, fwg);
		break;
	case MLX5_FWDUMP_RESET:
		if ((fflag & FWRITE) == 0) {
			error = EBADF;
			break;
		}
		devaddr = (struct mlx5_tool_addr *)data;
		error = mlx5_dbsf_to_core(devaddr, &mdev);
		if (error == 0)
			error = mlx5_fwdump_reset(mdev);
		break;
	case MLX5_FWDUMP_FORCE:
		if ((fflag & FWRITE) == 0) {
			error = EBADF;
			break;
		}
		devaddr = (struct mlx5_tool_addr *)data;
		error = mlx5_dbsf_to_core(devaddr, &mdev);
		if (error != 0)
			break;
		mlx5_fwdump(mdev);
		break;
	case MLX5_FW_UPDATE:
		if ((fflag & FWRITE) == 0) {
			error = EBADF;
			break;
		}
		fu = (struct mlx5_fw_update *)data;
		if (fu->img_fw_data_len > 10 * 1024 * 1024) {
			error = EINVAL;
			break;
		}
		devaddr = &fu->devaddr;
		error = mlx5_dbsf_to_core(devaddr, &mdev);
		if (error != 0)
			break;
		bzero(&fake_fw, sizeof(fake_fw));
		fake_fw.name = "umlx_fw_up";
		fake_fw.datasize = fu->img_fw_data_len;
		fake_fw.version = 1;
		fake_fw.data = (void *)kmem_malloc(fu->img_fw_data_len,
		    M_WAITOK);
		if (fake_fw.data == NULL) {
			error = ENOMEM;
			break;
		}
		error = copyin(fu->img_fw_data, __DECONST(void *, fake_fw.data),
		    fu->img_fw_data_len);
		if (error == 0)
			error = -mlx5_firmware_flash(mdev, &fake_fw);
		kmem_free((vm_offset_t)fake_fw.data, fu->img_fw_data_len);
		break;
	case MLX5_FW_RESET:
		if ((fflag & FWRITE) == 0) {
			error = EBADF;
			break;
		}
		devaddr = (struct mlx5_tool_addr *)data;
		error = mlx5_dbsf_to_core(devaddr, &mdev);
		if (error != 0)
			break;
		error = mlx5_fw_reset(mdev);
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static struct cdevsw mlx5_ctl_devsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	mlx5_ctl_ioctl,
};

static struct cdev *mlx5_ctl_dev;

int
mlx5_ctl_init(void)
{
	struct make_dev_args mda;
	int error;

	make_dev_args_init(&mda);
	mda.mda_flags = MAKEDEV_WAITOK | MAKEDEV_CHECKNAME;
	mda.mda_devsw = &mlx5_ctl_devsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_OPERATOR;
	mda.mda_mode = 0640;
	error = make_dev_s(&mda, &mlx5_ctl_dev, "mlx5ctl");
	return (-error);
}

void
mlx5_ctl_fini(void)
{

	if (mlx5_ctl_dev != NULL)
		destroy_dev(mlx5_ctl_dev);

}
