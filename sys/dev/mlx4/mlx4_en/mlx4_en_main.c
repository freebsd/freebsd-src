/*
 * Copyright (c) 2007, 2014 Mellanox Technologies. All rights reserved.
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

#define	LINUXKPI_PARAM_PREFIX mlx4_

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/slab.h>

#include <dev/mlx4/driver.h>
#include <dev/mlx4/device.h>
#include <dev/mlx4/cmd.h>

#include "en.h"

/* Mellanox ConnectX HCA Ethernet driver */

#define MLX4_EN_PARM_INT(X, def_val, desc) \
	static unsigned int X = def_val;\
	module_param(X , uint, 0444); \
	MODULE_PARM_DESC(X, desc);


/*
 * Device scope module parameters
 */

/* Enable RSS UDP traffic */
MLX4_EN_PARM_INT(udp_rss, 1,
		 "Enable RSS for incoming UDP traffic or disabled (0)");

/* Priority pausing */
MLX4_EN_PARM_INT(pfctx, 0, "Priority based Flow Control policy on TX[7:0]."
			   " Per priority bit mask");
MLX4_EN_PARM_INT(pfcrx, 0, "Priority based Flow Control policy on RX[7:0]."
			   " Per priority bit mask");

MLX4_EN_PARM_INT(inline_thold, MAX_INLINE,
		 "Threshold for using inline data (range: 17-104, default: 104)");

#define MAX_PFC_TX     0xff
#define MAX_PFC_RX     0xff

static int mlx4_en_get_profile(struct mlx4_en_dev *mdev)
{
	struct mlx4_en_profile *params = &mdev->profile;
	int i;

	params->udp_rss = udp_rss;
	params->num_tx_rings_p_up = min_t(int, mp_ncpus,
			MLX4_EN_MAX_TX_RING_P_UP);
	if (params->udp_rss && !(mdev->dev->caps.flags
					& MLX4_DEV_CAP_FLAG_UDP_RSS)) {
		mlx4_warn(mdev, "UDP RSS is not supported on this device.\n");
		params->udp_rss = 0;
	}
	for (i = 1; i <= MLX4_MAX_PORTS; i++) {
		params->prof[i].rx_pause = 1;
		params->prof[i].rx_ppp = pfcrx;
		params->prof[i].tx_pause = 1;
		params->prof[i].tx_ppp = pfctx;
		params->prof[i].tx_ring_size = MLX4_EN_DEF_TX_RING_SIZE;
		params->prof[i].rx_ring_size = MLX4_EN_DEF_RX_RING_SIZE;
		params->prof[i].tx_ring_num = params->num_tx_rings_p_up *
			MLX4_EN_NUM_UP;
		params->prof[i].rss_rings = 0;
		params->prof[i].inline_thold = inline_thold;
	}

	return 0;
}

static void *mlx4_en_get_netdev(struct mlx4_dev *dev, void *ctx, u8 port)
{
	struct mlx4_en_dev *endev = ctx;

	return endev->pndev[port];
}

static void mlx4_en_event(struct mlx4_dev *dev, void *endev_ptr,
			  enum mlx4_dev_event event, unsigned long port)
{
	struct mlx4_en_dev *mdev = (struct mlx4_en_dev *) endev_ptr;
	struct mlx4_en_priv *priv;

	switch (event) {
	case MLX4_DEV_EVENT_PORT_UP:
	case MLX4_DEV_EVENT_PORT_DOWN:
		if (!mdev->pndev[port])
			return;
		priv = mlx4_netdev_priv(mdev->pndev[port]);
		/* To prevent races, we poll the link state in a separate
		  task rather than changing it here */
		priv->link_state = event;
		queue_work(mdev->workqueue, &priv->linkstate_task);
		break;

	case MLX4_DEV_EVENT_CATASTROPHIC_ERROR:
		mlx4_err(mdev, "Internal error detected, restarting device\n");
		break;

	case MLX4_DEV_EVENT_SLAVE_INIT:
	case MLX4_DEV_EVENT_SLAVE_SHUTDOWN:
		break;
	default:
		if (port < 1 || port > dev->caps.num_ports ||
		    !mdev->pndev[port])
			return;
		mlx4_warn(mdev, "Unhandled event %d for port %d\n", event,
			  (int) port);
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
	(void) mlx4_mr_free(dev, &mdev->mr);
	iounmap(mdev->uar_map);
	mlx4_uar_free(dev, &mdev->priv_uar);
	mlx4_pd_free(dev, mdev->priv_pdn);
	kfree(mdev);
}

static void mlx4_en_activate(struct mlx4_dev *dev, void *ctx)
{
	int i;
	struct mlx4_en_dev *mdev = ctx;

	/* Create a netdev for each port */
	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH) {
		mlx4_info(mdev, "Activating port:%d\n", i);
		if (mlx4_en_init_netdev(mdev, i, &mdev->profile.prof[i]))
			mdev->pndev[i] = NULL;
	}
}

static void *mlx4_en_add(struct mlx4_dev *dev)
{
	struct mlx4_en_dev *mdev;
	int i;

	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		goto err_free_res;

	if (mlx4_pd_alloc(dev, &mdev->priv_pdn))
		goto err_free_dev;

	if (mlx4_uar_alloc(dev, &mdev->priv_uar))
		goto err_pd;

	mdev->uar_map = ioremap((phys_addr_t) mdev->priv_uar.pfn << PAGE_SHIFT,
				PAGE_SIZE);
	if (!mdev->uar_map)
		goto err_uar;
	spin_lock_init(&mdev->uar_lock);

	mdev->dev = dev;
	mdev->dma_device = &dev->persist->pdev->dev;
	mdev->pdev = dev->persist->pdev;
	mdev->device_up = false;

	mdev->LSO_support = !!(dev->caps.flags & (1 << 15));
	if (!mdev->LSO_support)
		mlx4_warn(mdev, "LSO not supported, please upgrade to later "
				"FW version to enable LSO\n");

	if (mlx4_mr_alloc(mdev->dev, mdev->priv_pdn, 0, ~0ull,
			 MLX4_PERM_LOCAL_WRITE |  MLX4_PERM_LOCAL_READ,
			 0, 0, &mdev->mr)) {
		mlx4_err(mdev, "Failed allocating memory region\n");
		goto err_map;
	}
	if (mlx4_mr_enable(mdev->dev, &mdev->mr)) {
		mlx4_err(mdev, "Failed enabling memory region\n");
		goto err_mr;
	}

	/* Build device profile according to supplied module parameters */
	if (mlx4_en_get_profile(mdev)) {
		mlx4_err(mdev, "Bad module parameters, aborting\n");
		goto err_mr;
	}

	/* Configure which ports to start according to module parameters */
	mdev->port_cnt = 0;
	mlx4_foreach_port(i, dev, MLX4_PORT_TYPE_ETH)
		mdev->port_cnt++;

	/* Set default number of RX rings*/
	mlx4_en_set_num_rx_rings(mdev);

	/* Create our own workqueue for reset/multicast tasks
	 * Note: we cannot use the shared workqueue because of deadlocks caused
	 *       by the rtnl lock */
	mdev->workqueue = create_singlethread_workqueue("mlx4_en");
	if (!mdev->workqueue)
		goto err_mr;

	/* At this stage all non-port specific tasks are complete:
	 * mark the card state as up */
	mutex_init(&mdev->state_lock);
	mdev->device_up = true;

	return mdev;

err_mr:
	(void) mlx4_mr_free(dev, &mdev->mr);
err_map:
	if (mdev->uar_map)
		iounmap(mdev->uar_map);
err_uar:
	mlx4_uar_free(dev, &mdev->priv_uar);
err_pd:
	mlx4_pd_free(dev, mdev->priv_pdn);
err_free_dev:
	kfree(mdev);
err_free_res:
	return NULL;
}

static struct mlx4_interface mlx4_en_interface = {
	.add		= mlx4_en_add,
	.remove		= mlx4_en_remove,
	.event		= mlx4_en_event,
	.get_dev	= mlx4_en_get_netdev,
	.protocol	= MLX4_PROT_ETH,
	.activate	= mlx4_en_activate,
};

static void mlx4_en_verify_params(void)
{
	if (pfctx > MAX_PFC_TX) {
		pr_warn("mlx4_en: WARNING: illegal module parameter pfctx 0x%x - should be in range 0-0x%x, will be changed to default (0)\n",
			pfctx, MAX_PFC_TX);
		pfctx = 0;
	}

	if (pfcrx > MAX_PFC_RX) {
		pr_warn("mlx4_en: WARNING: illegal module parameter pfcrx 0x%x - should be in range 0-0x%x, will be changed to default (0)\n",
			pfcrx, MAX_PFC_RX);
		pfcrx = 0;
	}

	if (inline_thold < MIN_PKT_LEN || inline_thold > MAX_INLINE) {
		pr_warn("mlx4_en: WARNING: illegal module parameter inline_thold %d - should be in range %d-%d, will be changed to default (%d)\n",
			inline_thold, MIN_PKT_LEN, MAX_INLINE, MAX_INLINE);
		inline_thold = MAX_INLINE;
	}
}

static int __init mlx4_en_init(void)
{
	mlx4_en_verify_params();

	return mlx4_register_interface(&mlx4_en_interface);
}

static void __exit mlx4_en_cleanup(void)
{
	mlx4_unregister_interface(&mlx4_en_interface);
}

module_init_order(mlx4_en_init, SI_ORDER_SIXTH);
module_exit_order(mlx4_en_cleanup, SI_ORDER_SIXTH);

static int
mlx4en_evhand(module_t mod, int event, void *arg)
{
        return (0);
}
static moduledata_t mlx4en_mod = {
        .name = "mlx4en",
	.evhand = mlx4en_evhand,
};
DECLARE_MODULE(mlx4en, mlx4en_mod, SI_SUB_OFED_PREINIT, SI_ORDER_ANY);
MODULE_DEPEND(mlx4en, mlx4, 1, 1, 1);
MODULE_DEPEND(mlx4en, linuxkpi, 1, 1, 1);
