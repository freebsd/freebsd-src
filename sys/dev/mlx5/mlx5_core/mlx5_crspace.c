/*-
 * Copyright (c) 2013-2018, Mellanox Technologies, Ltd.  All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <linux/pci.h>
#include <linux/delay.h>
#include <dev/mlx5/driver.h>
#include "mlx5_core.h"

enum {
	PCI_CTRL_OFFSET = 0x4,
	PCI_COUNTER_OFFSET = 0x8,
	PCI_SEMAPHORE_OFFSET = 0xc,

	PCI_ADDR_OFFSET = 0x10,
	PCI_DATA_OFFSET = 0x14,

	PCI_FLAG_BIT_OFFS = 31,
	PCI_SPACE_BIT_OFFS = 0,
	PCI_SPACE_BIT_LEN = 16,
	PCI_SIZE_VLD_BIT_OFFS = 28,
	PCI_SIZE_VLD_BIT_LEN = 1,
	PCI_STATUS_BIT_OFFS = 29,
	PCI_STATUS_BIT_LEN = 3,
};

enum {
	IFC_MAX_RETRIES = 2048
};

#define MLX5_EXTRACT_C(source, offset, size)	\
	((((unsigned)(source)) >> (offset)) & MLX5_ONES32(size))
#define MLX5_EXTRACT(src, start, len)		\
	(((len) == 32) ? (src) : MLX5_EXTRACT_C(src, start, len))
#define MLX5_ONES32(size)			\
	((size) ? (0xffffffff >> (32 - (size))) : 0)
#define MLX5_MASK32(offset, size)		\
	(MLX5_ONES32(size) << (offset))
#define MLX5_MERGE_C(rsrc1, rsrc2, start, len)  \
	((((rsrc2) << (start)) & (MLX5_MASK32((start), (len)))) | \
	 ((rsrc1) & (~MLX5_MASK32((start), (len)))))
#define MLX5_MERGE(rsrc1, rsrc2, start, len)	\
	(((len) == 32) ? (rsrc2) : MLX5_MERGE_C(rsrc1, rsrc2, start, len))

#define MLX5_SEMAPHORE_SPACE_DOMAIN 0xA

static int mlx5_pciconf_wait_on_flag(struct mlx5_core_dev *dev,
				     u8 expected_val)
{
	int retries = 0;
	u32 flag;

	for(;;) {
		pci_read_config_dword(dev->pdev, dev->vsec_addr +
				      PCI_ADDR_OFFSET, &flag);
		flag = MLX5_EXTRACT(flag, PCI_FLAG_BIT_OFFS, 1);
		if (flag == expected_val)
			return (0);
		retries++;
		if (retries > IFC_MAX_RETRIES)
			return (-EBUSY);
		if ((retries & 0xf) == 0)
			usleep_range(1000, 2000);
	}
}

static int mlx5_pciconf_read(struct mlx5_core_dev *dev,
			     unsigned int offset, u32 *data)
{
	u32 address;
	int ret;

	if (MLX5_EXTRACT(offset, 31, 1))
		return -EINVAL;
	address = MLX5_MERGE(offset, 0, PCI_FLAG_BIT_OFFS, 1);
	pci_write_config_dword(dev->pdev, dev->vsec_addr +
			       PCI_ADDR_OFFSET, address);
	ret = mlx5_pciconf_wait_on_flag(dev, 1);
	if (ret)
		return (ret);
	return pci_read_config_dword(dev->pdev, dev->vsec_addr +
				     PCI_DATA_OFFSET, data);
}

static int mlx5_pciconf_write(struct mlx5_core_dev *dev,
			      unsigned int offset, u32 data)
{
	u32 address;

	if (MLX5_EXTRACT(offset, 31, 1))
		return -EINVAL;

	/* Set flag to 0x1 */
	address = MLX5_MERGE(offset, 1, PCI_FLAG_BIT_OFFS, 1);

	pci_write_config_dword(dev->pdev, dev->vsec_addr +
			       PCI_DATA_OFFSET, data);

	pci_write_config_dword(dev->pdev, dev->vsec_addr +
			       PCI_ADDR_OFFSET, address);

	/* Wait for the flag to be cleared */
	return mlx5_pciconf_wait_on_flag(dev, 0);

}

int mlx5_pciconf_cap9_sem(struct mlx5_core_dev *dev, int state)
{
	u32 counter = 0;
	int retries = 0;
	u32 lock_val;

	if (!dev->vsec_addr)
		return -ENXIO;

	if (state == UNLOCK) {
		pci_write_config_dword(dev->pdev, dev->vsec_addr +
				       PCI_SEMAPHORE_OFFSET, 0);
		return (0);
	}
	do {
		if (retries > IFC_MAX_RETRIES * 10)
			return -EBUSY;
		pci_read_config_dword(dev->pdev, dev->vsec_addr +
				      PCI_SEMAPHORE_OFFSET, &lock_val);
		if (lock_val != 0) {
			retries++;
			if (retries > IFC_MAX_RETRIES * 10)
				return -EBUSY;
			usleep_range(1000, 2000);
			continue;
		}
		pci_read_config_dword(dev->pdev, dev->vsec_addr +
				      PCI_COUNTER_OFFSET, &counter);
		pci_write_config_dword(dev->pdev, dev->vsec_addr +
				       PCI_SEMAPHORE_OFFSET, counter);
		pci_read_config_dword(dev->pdev, dev->vsec_addr +
				      PCI_SEMAPHORE_OFFSET, &lock_val);
		retries++;
	} while (counter != lock_val);
	return 0;
}

static int mlx5_pciconf_set_addr_space(struct mlx5_core_dev *dev,
				       u16 space)
{
	u32 val;

	pci_read_config_dword(dev->pdev, dev->vsec_addr +
			      PCI_CTRL_OFFSET, &val);

	val = MLX5_MERGE(val, space, PCI_SPACE_BIT_OFFS,
			 PCI_SPACE_BIT_LEN);
	pci_write_config_dword(dev->pdev, dev->vsec_addr +
			       PCI_CTRL_OFFSET, val);

	pci_read_config_dword(dev->pdev, dev->vsec_addr +
			      PCI_CTRL_OFFSET, &val);

	if (MLX5_EXTRACT(val, PCI_STATUS_BIT_OFFS,
			 PCI_STATUS_BIT_LEN) == 0)
		return -EINVAL;

	return 0;
}

static int mlx5_get_vendor_cap_addr(struct mlx5_core_dev *dev)
{
	int vend_cap;
	int ret;

	vend_cap = pci_find_capability(dev->pdev, CAP_ID);
	if (!vend_cap)
		return 0;
	dev->vsec_addr = vend_cap;
	ret = mlx5_pciconf_cap9_sem(dev, LOCK);
	if (ret) {
		mlx5_core_warn(dev,
		    "pciconf_cap9_sem locking failure\n");
		return 0;
	}
	if (mlx5_pciconf_set_addr_space(dev,
	       MLX5_SEMAPHORE_SPACE_DOMAIN))
		vend_cap = 0;
	ret = mlx5_pciconf_cap9_sem(dev, UNLOCK);
	if (ret)
		mlx5_core_warn(dev,
		    "pciconf_cap9_sem unlocking failure\n");
	return vend_cap;
}

int mlx5_pciconf_set_sem_addr_space(struct mlx5_core_dev *dev,
				    u32 sem_space_address, int state)
{
	u32 data, id = 0;
	int ret;

	if (!dev->vsec_addr)
		return -ENXIO;

	ret = mlx5_pciconf_set_addr_space(dev,
					  MLX5_SEMAPHORE_SPACE_DOMAIN);
	if (ret)
		return (ret);

	if (state == LOCK)
		/* Get a unique ID based on the counter */
		pci_read_config_dword(dev->pdev, dev->vsec_addr +
				      PCI_COUNTER_OFFSET, &id);

	/* Try to modify lock */
	ret = mlx5_pciconf_write(dev, sem_space_address, id);
	if (ret)
		return (ret);

	/* Verify lock was modified */
	ret = mlx5_pciconf_read(dev, sem_space_address, &data);
	if (ret)
		return -EINVAL;

	if (data != id)
		return -EBUSY;

	return 0;
}

void mlx5_vsec_init(struct mlx5_core_dev *dev)
{
	dev->vsec_addr = mlx5_get_vendor_cap_addr(dev);
}

