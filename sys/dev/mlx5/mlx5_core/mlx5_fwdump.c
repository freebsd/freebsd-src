/*-
 * Copyright (c) 2018, 2019 Mellanox Technologies, Ltd.  All rights reserved.
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

#include "opt_rss.h"
#include "opt_ratelimit.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/device.h>
#include <dev/mlx5/port.h>
#include <dev/mlx5/mlx5_core/mlx5_core.h>
#include <dev/mlx5/mlx5io.h>
#include <dev/mlx5/diagnostics.h>

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

static int mlx5_fw_dump_enable = 1;
SYSCTL_INT(_hw_mlx5, OID_AUTO, fw_dump_enable, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &mlx5_fw_dump_enable, 0,
    "Enable fw dump setup and op");

void
mlx5_fwdump_prep(struct mlx5_core_dev *mdev)
{
	device_t dev;
	int error, vsc_addr;
	unsigned i, sz;
	u32 addr, in, out, next_addr;

	mdev->dump_data = NULL;

	TUNABLE_INT_FETCH("hw.mlx5.fw_dump_enable", &mlx5_fw_dump_enable);
	if (!mlx5_fw_dump_enable) {
		mlx5_core_warn(mdev,
		    "Firmware dump administratively prohibited\n");
		return;
	}

	DROP_GIANT();

	error = mlx5_vsc_find_cap(mdev);
	if (error != 0) {
		/* Inability to create a firmware dump is not fatal. */
		mlx5_core_warn(mdev,
		    "Unable to find vendor-specific capability, error %d\n",
		    error);
		goto pickup_g;
	}
	error = mlx5_vsc_lock(mdev);
	if (error != 0)
		goto pickup_g;
	error = mlx5_vsc_set_space(mdev, MLX5_VSC_DOMAIN_SCAN_CRSPACE);
	if (error != 0) {
		mlx5_core_warn(mdev, "VSC scan space is not supported\n");
		goto unlock_vsc;
	}
	dev = mdev->pdev->dev.bsddev;
	vsc_addr = mdev->vsc_addr;
	if (vsc_addr == 0) {
		mlx5_core_warn(mdev, "Cannot read VSC, no address\n");
		goto unlock_vsc;
	}

	in = 0;
	for (sz = 1, addr = 0;;) {
		MLX5_VSC_SET(vsc_addr, &in, address, addr);
		pci_write_config(dev, vsc_addr + MLX5_VSC_ADDR_OFFSET, in, 4);
		error = mlx5_vsc_wait_on_flag(mdev, 1);
		if (error != 0) {
			mlx5_core_warn(mdev,
		    "Failed waiting for read complete flag, error %d addr %#x\n",
			    error, addr);
			goto unlock_vsc;
		}
		pci_read_config(dev, vsc_addr + MLX5_VSC_DATA_OFFSET, 4);
		out = pci_read_config(dev, vsc_addr + MLX5_VSC_ADDR_OFFSET, 4);
		next_addr = MLX5_VSC_GET(vsc_addr, &out, address);
		if (next_addr == 0 || next_addr == addr)
			break;
		if (next_addr != addr + 4)
			sz++;
		addr = next_addr;
	}
	if (sz == 1) {
		mlx5_core_warn(mdev, "no output from scan space\n");
		goto unlock_vsc;
	}

	/*
	 * We add a sentinel element at the end of the array to
	 * terminate the read loop in mlx5_fwdump(), so allocate sz + 1.
	 */
	mdev->dump_rege = malloc((sz + 1) * sizeof(struct mlx5_crspace_regmap),
	    M_MLX5_DUMP, M_WAITOK | M_ZERO);

	for (i = 0, addr = 0;;) {
		mdev->dump_rege[i].cnt++;
		MLX5_VSC_SET(vsc_addr, &in, address, addr);
		pci_write_config(dev, vsc_addr + MLX5_VSC_ADDR_OFFSET, in, 4);
		error = mlx5_vsc_wait_on_flag(mdev, 1);
		if (error != 0) {
			mlx5_core_warn(mdev,
		    "Failed waiting for read complete flag, error %d addr %#x\n",
			    error, addr);
			free(mdev->dump_rege, M_MLX5_DUMP);
			mdev->dump_rege = NULL;
			goto unlock_vsc;
		}
		pci_read_config(dev, vsc_addr + MLX5_VSC_DATA_OFFSET, 4);
		out = pci_read_config(dev, vsc_addr + MLX5_VSC_ADDR_OFFSET, 4);
		next_addr = MLX5_VSC_GET(vsc_addr, &out, address);
		if (next_addr == 0 || next_addr == addr)
			break;
		if (next_addr != addr + 4) {
			if (++i == sz) {
				mlx5_core_err(mdev,
		    "Inconsistent hw crspace reads (1): sz %u i %u addr %#lx",
				    sz, i, (unsigned long)addr);
				break;
			}
			mdev->dump_rege[i].addr = next_addr;
		}
		addr = next_addr;
	}
	/* i == sz case already reported by loop above */
	if (i + 1 != sz && i != sz) {
		mlx5_core_err(mdev,
		    "Inconsistent hw crspace reads (2): sz %u i %u addr %#lx",
		    sz, i, (unsigned long)addr);
	}

	mdev->dump_size = mlx5_fwdump_getsize(mdev->dump_rege);
	mdev->dump_data = malloc(mdev->dump_size * sizeof(uint32_t),
	    M_MLX5_DUMP, M_WAITOK | M_ZERO);
	mdev->dump_valid = false;
	mdev->dump_copyout = false;

unlock_vsc:
	mlx5_vsc_unlock(mdev);
pickup_g:
	PICKUP_GIANT();
}

int
mlx5_fwdump(struct mlx5_core_dev *mdev)
{
	const struct mlx5_crspace_regmap *r;
	uint32_t i, ri;
	int error;

	mlx5_core_info(mdev, "Issuing FW dump\n");
	mtx_lock(&mdev->dump_lock);
	if (mdev->dump_data == NULL) {
		error = EIO;
		goto failed;
	}
	if (mdev->dump_valid) {
		/* only one dump */
		mlx5_core_warn(mdev,
		    "Only one FW dump can be captured aborting FW dump\n");
		error = EEXIST;
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
	return (error);
}

void
mlx5_fwdump_clean(struct mlx5_core_dev *mdev)
{

	mtx_lock(&mdev->dump_lock);
	while (mdev->dump_copyout)
		msleep(&mdev->dump_copyout, &mdev->dump_lock, 0, "mlx5fwc", 0);
	mlx5_fwdump_destroy_dd(mdev);
	mtx_unlock(&mdev->dump_lock);
	free(mdev->dump_rege, M_MLX5_DUMP);
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
		bus_topo_lock();
		bus = device_get_parent(dev);
		error = BUS_RESET_CHILD(device_get_parent(bus), bus,
		    DEVF_RESET_DETACH);
		bus_topo_unlock();
	}
	return (error);
}

static int
mlx5_eeprom_copyout(struct mlx5_core_dev *dev, struct mlx5_eeprom_get *eeprom_info)
{
	struct mlx5_eeprom eeprom;
	int error;

	eeprom.i2c_addr = MLX5_I2C_ADDR_LOW;
	eeprom.device_addr = 0;
	eeprom.page_num = MLX5_EEPROM_LOW_PAGE;
	eeprom.page_valid = 0;

	/* Read three first bytes to get important info */
	error = mlx5_get_eeprom_info(dev, &eeprom);
	if (error != 0) {
		mlx5_core_err(dev,
		    "Failed reading EEPROM initial information\n");
		return (error);
	}
	eeprom_info->eeprom_info_page_valid = eeprom.page_valid;
	eeprom_info->eeprom_info_out_len = eeprom.len;

	if (eeprom_info->eeprom_info_buf == NULL)
		return (0);
	/*
	 * Allocate needed length buffer and additional space for
	 * page 0x03
	 */
	eeprom.data = malloc(eeprom.len + MLX5_EEPROM_PAGE_LENGTH,
	    M_MLX5_EEPROM, M_WAITOK | M_ZERO);

	/* Read the whole eeprom information */
	error = mlx5_get_eeprom(dev, &eeprom);
	if (error != 0) {
		mlx5_core_err(dev, "Failed reading EEPROM error = %d\n",
		    error);
		error = 0;
		/*
		 * Continue printing partial information in case of
		 * an error
		 */
	}
	error = copyout(eeprom.data, eeprom_info->eeprom_info_buf,
	    eeprom.len);
	free(eeprom.data, M_MLX5_EEPROM);

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
	struct mlx5_eeprom_get *eeprom_info;
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
		error = mlx5_fwdump(mdev);
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
	case MLX5_EEPROM_GET:
		if ((fflag & FREAD) == 0) {
			error = EBADF;
			break;
		}
		eeprom_info = (struct mlx5_eeprom_get *)data;
		devaddr = &eeprom_info->devaddr;
		error = mlx5_dbsf_to_core(devaddr, &mdev);
		if (error != 0)
			break;
		error = mlx5_eeprom_copyout(mdev, eeprom_info);
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
