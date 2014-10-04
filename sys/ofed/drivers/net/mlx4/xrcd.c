/*
 * Copyright (c) 2006, 2007 Cisco Systems, Inc.  All rights reserved.
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
 */

#include <linux/errno.h>

#include "mlx4.h"

int mlx4_xrcd_alloc(struct mlx4_dev *dev, u32 *xrcdn)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	*xrcdn = mlx4_bitmap_alloc(&priv->xrcd_bitmap);
	if (*xrcdn == -1)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL_GPL(mlx4_xrcd_alloc);

void mlx4_xrcd_free(struct mlx4_dev *dev, u32 xrcdn)
{
	mlx4_bitmap_free(&mlx4_priv(dev)->xrcd_bitmap, xrcdn);
}
EXPORT_SYMBOL_GPL(mlx4_xrcd_free);

int __devinit mlx4_init_xrcd_table(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	return mlx4_bitmap_init(&priv->xrcd_bitmap, (1 << 16),
				(1 << 16) - 1, dev->caps.reserved_xrcds + 1, 0);
}

void mlx4_cleanup_xrcd_table(struct mlx4_dev *dev)
{
	mlx4_bitmap_cleanup(&mlx4_priv(dev)->xrcd_bitmap);
}


