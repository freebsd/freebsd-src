/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/netdevice.h>

#include <linux/mlx4/driver.h>
#include <linux/mlx4/device.h>
#include <linux/mlx4/cmd.h>

#include "mlx4_en.h"

MODULE_AUTHOR("Liran Liss, Yevgeny Petrilin");
MODULE_DESCRIPTION("Mellanox ConnectX HCA Ethernet driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION " ("DRV_RELDATE")");

static const char mlx4_en_version[] =
	DRV_NAME ": Mellanox ConnectX HCA Ethernet driver v"
	DRV_VERSION " (" DRV_RELDATE ")\n";

#define MLX4_EN_PARM_INT(X, def_val, desc) \
	static unsigned int X = def_val;\
	module_param(X , uint, 0444); \
	MODULE_PARM_DESC(X, desc);


/*
 * Device scope module parameters
 */


/* Enable RSS TCP traffic */
MLX4_EN_PARM_INT(tcp_rss, 1,
		 "Enable RSS for incomming TCP traffic or disabled (0)");
/* Enable RSS UDP traffic */
MLX4_EN_PARM_INT(udp_rss, 1,
		 "Enable RSS for incomming UDP traffic or disabled (0)");

/* Number of LRO sessions per Rx ring (rounded up to a power of two) */
MLX4_EN_PARM_INT(num_lro, MLX4_EN_MAX_LRO_DESCRIPTORS,
		 "Number of LRO sessions per ring or disabled (0)");

/* Allow reassembly of fragmented IP packets */
MLX4_EN_PARM_INT(ip_reasm, 1, "Allow reassembly of fragmented IP packets (!0)");

/* Priority pausing */
MLX4_EN_PARM_INT(pfctx, 0, "Priority based Flow Control policy on TX[7:0]."
			   " Per priority bit mask");
MLX4_EN_PARM_INT(pfcrx, 0, "Priority based Flow Control policy on RX[7:0]."
			   " Per priority bit mask");

static int mlx4_en_get_profile(struct mlx4_en_dev *mdev)
{
	struct mlx4_en_profile *params = &mdev->profile;
	int i;

	params->tcp_rss = tcp_rss;
	params->udp_rss = udp_rss;
	if (params->udp_rss && !mdev->dev->caps.udp_rss) {
		mlx4_warn(mdev, "UDP RSS is not supported on this device.\n");
		params->udp_rss = 0;
	}
	params->num_lro = min_t(int, num_lro , MLX4_EN_MAX_LRO_DESCRIPTORS);
	params->ip_reasm = ip_reasm;
	for (i = 1; i <= MLX4_MAX_PORTS; i++) {
		params->prof[i].rx_pause = 1;
		params->prof[i].rx_ppp = pfcrx;
		params->prof[i].tx_pause = 1;
		params->prof[i].tx_ppp = pfctx;
		params->prof[i].tx_ring_size = MLX4_EN_DEF_TX_RING_SIZE;
		params->prof[i].rx_ring_size = MLX4_EN_DEF_RX_RING_SIZE;
		params->prof[i].tx_ring_num = MLX4_EN_NUM_HASH_RINGS + 1 +
			(!!pfcrx) * MLX4_EN_NUM_PPP_RINGS;
	}

	return 0;
}

static void *get_netdev(struct mlx4_dev *dev, void *ctx, u8 port)
{
	struct mlx4_en_dev *endev = ctx;

	return endev->pndev[port];
}

static void mlx4_en_event(struct mlx4_dev *dev, void *endev_ptr,
			  enum mlx4_dev_event event, int port)
{
	struct mlx4_en_dev *mdev = (struct mlx4_en_dev *) endev_ptr;
	struct mlx4_en_priv *priv;

	if (!mdev->pndev[port])
		return;

	priv = netdev_priv(mdev->pndev[port]);
	switch (event) {
	case MLX4_DEV_EVENT_PORT_UP:
	case MLX4_DEV_EVENT_PORT_DOWN:
		/* To prevent races, we poll the link state in a separate
		  task rather than changing it here */
		priv->link_state = event;
		queue_work(mdev->workqueue, &priv->linkstate_task);
		break;

	case MLX4_DEV_EVENT_CATASTROPHIC_ERROR:
		mlx4_err(mdev, "Internal error detected, restarting device\n");
		break;

	default:
		mlx4_warn(mdev, "Unhandled event: %d\n", event);
	}
}

static void mlx4_en_remove(struct mlx4_dev *dev, void *endev_ptr)
{
	struct mlx4_en_dev *mdev = endev_ptr;
	int i;

	mutex_lock(&mdev->state_lock);
	mdev->device_up = false;
	mutex_unlock(&mdev->state_lock);

	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH)
		if (mdev->pndev[i])
			mlx4_en_destroy_netdev(mdev->pndev[i]);

	flush_workqueue(mdev->workqueue);
	destroy_workqueue(mdev->workqueue);
	mlx4_mr_free(dev, &mdev->mr);
	mlx4_uar_free(dev, &mdev->priv_uar);
	mlx4_pd_free(dev, mdev->priv_pdn);
	kfree(mdev);
}

static void *mlx4_en_add(struct mlx4_dev *dev)
{
	static int mlx4_en_version_printed;
	struct mlx4_en_dev *mdev;
	int i;
	int err;

	if (!mlx4_en_version_printed) {
		printk(KERN_INFO "%s", mlx4_en_version);
		mlx4_en_version_printed++;
	}

	mdev = kzalloc(sizeof *mdev, GFP_KERNEL);
	if (!mdev) {
		dev_err(&dev->pdev->dev, "Device struct alloc failed, "
			"aborting.\n");
		err = -ENOMEM;
		goto err_free_res;
	}

	if (mlx4_pd_alloc(dev, &mdev->priv_pdn))
		goto err_free_dev;

	if (mlx4_uar_alloc(dev, &mdev->priv_uar))
		goto err_pd;

	mdev->uar_map = ioremap(mdev->priv_uar.pfn << PAGE_SHIFT, PAGE_SIZE);
	if (!mdev->uar_map)
		goto err_uar;
	spin_lock_init(&mdev->uar_lock);

	mdev->dev = dev;
	mdev->dma_device = &(dev->pdev->dev);
	mdev->pdev = dev->pdev;
	mdev->device_up = false;

	mdev->LSO_support = !!(dev->caps.flags & (1 << 15));
	if (!mdev->LSO_support)
		mlx4_warn(mdev, "LSO not supported, please upgrade to later "
				"FW version to enable LSO\n");

	if (mlx4_mr_alloc(mdev->dev, mdev->priv_pdn, 0, ~0ull,
			 MLX4_PERM_LOCAL_WRITE |  MLX4_PERM_LOCAL_READ,
			 0, 0, &mdev->mr)) {
		mlx4_err(mdev, "Failed allocating memory region\n");
		goto err_uar;
	}
	if (mlx4_mr_enable(mdev->dev, &mdev->mr)) {
		mlx4_err(mdev, "Failed enabling memory region\n");
		goto err_mr;
	}

	/* Build device profile according to supplied module parameters */
	err = mlx4_en_get_profile(mdev);
	if (err) {
		mlx4_err(mdev, "Bad module parameters, aborting.\n");
		goto err_mr;
	}

	/* Configure wich ports to start according to module parameters */
	mdev->port_cnt = 0;
	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH)
		mdev->port_cnt++;

	/* If we did not receive an explicit number of Rx rings, default to
	 * the number of completion vectors populated by the mlx4_core */
	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH) {
		mlx4_info(mdev, "Using %d tx rings for port:%d\n",
			  mdev->profile.prof[i].tx_ring_num, i);
		mdev->profile.prof[i].rx_ring_num = rounddown_pow_of_two(
			min_t(int, dev->caps.num_comp_vectors, MAX_RX_RINGS/2)) +
		(mdev->profile.udp_rss ? rounddown_pow_of_two(
			min_t(int, dev->caps.num_comp_vectors, MAX_RX_RINGS/2)) : 1);
		mlx4_info(mdev, "Defaulting to %d rx rings for port:%d\n",
			  mdev->profile.prof[i].rx_ring_num, i);
	}

	/* Create our own workqueue for reset/multicast tasks
	 * Note: we cannot use the shared workqueue because of deadlocks caused
	 *       by the rtnl lock */
	mdev->workqueue = create_singlethread_workqueue("mlx4_en");
	if (!mdev->workqueue) {
		err = -ENOMEM;
		goto err_mr;
	}

	/* At this stage all non-port specific tasks are complete:
	 * mark the card state as up */
	mutex_init(&mdev->state_lock);
	mdev->device_up = true;

	/* Setup ports */

	/* Create a netdev for each port */
	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH) {
		mlx4_info(mdev, "Activating port:%d\n", i);
		if (mlx4_en_init_netdev(mdev, i, &mdev->profile.prof[i])) {
			mdev->pndev[i] = NULL;
			goto err_free_netdev;
		}
	}
	return mdev;


err_free_netdev:
	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH) {
		if (mdev->pndev[i])
			mlx4_en_destroy_netdev(mdev->pndev[i]);
	}

	mutex_lock(&mdev->state_lock);
	mdev->device_up = false;
	mutex_unlock(&mdev->state_lock);
	flush_workqueue(mdev->workqueue);

	/* Stop event queue before we drop down to release shared SW state */
	destroy_workqueue(mdev->workqueue);

err_mr:
	mlx4_mr_free(dev, &mdev->mr);
err_uar:
	mlx4_uar_free(dev, &mdev->priv_uar);
err_pd:
	mlx4_pd_free(dev, mdev->priv_pdn);
err_free_dev:
	kfree(mdev);
err_free_res:
	return NULL;
}

enum mlx4_query_reply mlx4_en_query(void *endev_ptr, void *int_dev)
{
	struct mlx4_en_dev *mdev = endev_ptr;
	struct net_device *netdev = int_dev;
	int p;
	
	for (p = 1; p <= MLX4_MAX_PORTS; ++p)
		if (mdev->pndev[p] == netdev)
			return p;

	return MLX4_QUERY_NOT_MINE;
}

static struct pci_device_id mlx4_en_pci_table[] = {
	{ PCI_VDEVICE(MELLANOX, 0x6340) }, /* MT25408 "Hermon" SDR */
	{ PCI_VDEVICE(MELLANOX, 0x634a) }, /* MT25408 "Hermon" DDR */
	{ PCI_VDEVICE(MELLANOX, 0x6354) }, /* MT25408 "Hermon" QDR */
	{ PCI_VDEVICE(MELLANOX, 0x6732) }, /* MT25408 "Hermon" DDR PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x673c) }, /* MT25408 "Hermon" QDR PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x6368) }, /* MT25408 "Hermon" EN 10GigE */
	{ PCI_VDEVICE(MELLANOX, 0x6750) }, /* MT25408 "Hermon" EN 10GigE PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x6372) }, /* MT25458 ConnectX EN 10GBASE-T 10GigE */
	{ PCI_VDEVICE(MELLANOX, 0x675a) }, /* MT25458 ConnectX EN 10GBASE-T+Gen2 10GigE */
	{ PCI_VDEVICE(MELLANOX, 0x6764) }, /* MT26468 ConnectX EN 10GigE PCIe gen2 */
	{ PCI_VDEVICE(MELLANOX, 0x6746) }, /* MT26438 ConnectX VPI PCIe 2.0 5GT/s - IB QDR / 10GigE Virt+ */
	{ PCI_VDEVICE(MELLANOX, 0x676e) }, /* MT26478 ConnectX EN 40GigE PCIe 2.0 5GT/s */
	{ PCI_VDEVICE(MELLANOX, 0x6778) }, /* MT26488 ConnectX VPI PCIe 2.0 5GT/s - IB DDR / 10GigE Virt+ */
	{ PCI_VDEVICE(MELLANOX, 0x1000) },
	{ PCI_VDEVICE(MELLANOX, 0x1001) },
	{ PCI_VDEVICE(MELLANOX, 0x1002) },
	{ PCI_VDEVICE(MELLANOX, 0x1003) },
	{ PCI_VDEVICE(MELLANOX, 0x1004) },
	{ PCI_VDEVICE(MELLANOX, 0x1005) },
	{ PCI_VDEVICE(MELLANOX, 0x1006) },
	{ PCI_VDEVICE(MELLANOX, 0x1007) },
	{ PCI_VDEVICE(MELLANOX, 0x1008) },
	{ PCI_VDEVICE(MELLANOX, 0x1009) },
	{ PCI_VDEVICE(MELLANOX, 0x100a) },
	{ PCI_VDEVICE(MELLANOX, 0x100b) },
	{ PCI_VDEVICE(MELLANOX, 0x100c) },
	{ PCI_VDEVICE(MELLANOX, 0x100d) },
	{ PCI_VDEVICE(MELLANOX, 0x100e) },
	{ PCI_VDEVICE(MELLANOX, 0x100f) },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, mlx4_en_pci_table);

static struct mlx4_interface mlx4_en_interface = {
	.add	= mlx4_en_add,
	.remove	= mlx4_en_remove,
	.event	= mlx4_en_event,
	.query  = mlx4_en_query,
	.get_prot_dev	= get_netdev,
	.protocol	= MLX4_PROT_EN,
};

static int __init mlx4_en_init(void)
{
	return mlx4_register_interface(&mlx4_en_interface);
}

static void __exit mlx4_en_cleanup(void)
{
	mlx4_unregister_interface(&mlx4_en_interface);
}

module_init(mlx4_en_init);
module_exit(mlx4_en_cleanup);

