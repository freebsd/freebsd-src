/*
 * Copyright (c) 2006 Cisco Systems, Inc.  All rights reserved.
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

#ifndef MLX4_DRIVER_H
#define MLX4_DRIVER_H

#include <linux/mlx4/device.h>

struct mlx4_dev;

#define MLX4_MAC_MASK	   0xffffffffffffULL
#define MLX4_BE_SHORT_MASK cpu_to_be16(0xffff)
#define MLX4_BE_WORD_MASK  cpu_to_be32(0xffffffff)

enum mlx4_dev_event {
	MLX4_DEV_EVENT_CATASTROPHIC_ERROR,
	MLX4_DEV_EVENT_PORT_UP,
	MLX4_DEV_EVENT_PORT_DOWN,
	MLX4_DEV_EVENT_PORT_REINIT,
	MLX4_DEV_EVENT_PORT_MGMT_CHANGE,
	MLX4_DEV_EVENT_SLAVE_INIT,
	MLX4_DEV_EVENT_SLAVE_SHUTDOWN,
};

struct mlx4_interface {
	void *			(*add)	 (struct mlx4_dev *dev);
	void			(*remove)(struct mlx4_dev *dev, void *context);
	void			(*event) (struct mlx4_dev *dev, void *context,
					  enum mlx4_dev_event event, unsigned long param);
	void *			(*get_dev)(struct mlx4_dev *dev, void *context, u8 port);
	struct list_head	list;
	enum mlx4_protocol	protocol;
};

enum {
	MLX4_MAX_DEVICES	= 32,
	MLX4_DEVS_TBL_SIZE	= MLX4_MAX_DEVICES + 1,
	MLX4_DBDF2VAL_STR_SIZE	= 512,
	MLX4_STR_NAME_SIZE	= 64,
	MLX4_MAX_BDF_VALS	= 2,
	MLX4_ENDOF_TBL		= -1LL
};

struct mlx4_dbdf2val {
	u64 dbdf;
	int val[MLX4_MAX_BDF_VALS];
};

struct mlx4_range {
	int min;
	int max;
};

/*
 * mlx4_dbdf2val_lst struct holds all the data needed to convert
 * dbdf-to-value-list string into dbdf-to-value table.
 * dbdf-to-value-list string is a comma separated list of dbdf-to-value strings.
 * the format of dbdf-to-value string is: "[mmmm:]bb:dd.f-v1[;v2]"
 * mmmm - Domain number (optional)
 * bb - Bus number
 * dd - device number
 * f  - Function number
 * v1 - First value related to the domain-bus-device-function.
 * v2 - Second value related to the domain-bus-device-function (optional).
 * bb, dd - Two hexadecimal digits without preceding 0x.
 * mmmm - Four hexadecimal digits without preceding 0x.
 * f  - One hexadecimal without preceding 0x.
 * v1,v2 - Number with normal convention (e.g 100, 0xd3).
 * dbdf-to-value-list string format:
 *     "[mmmm:]bb:dd.f-v1[;v2],[mmmm:]bb:dd.f-v1[;v2],..."
 *
 */
struct mlx4_dbdf2val_lst {
	char		name[MLX4_STR_NAME_SIZE];    /* String name */
	char		str[MLX4_DBDF2VAL_STR_SIZE]; /* dbdf2val list str */
	struct mlx4_dbdf2val tbl[MLX4_DEVS_TBL_SIZE];/* dbdf to value table */
	int		num_vals;		     /* # of vals per dbdf */
	int		def_val[MLX4_MAX_BDF_VALS];  /* Default values */
	struct mlx4_range range;		     /* Valid values range */
};

int mlx4_fill_dbdf2val_tbl(struct mlx4_dbdf2val_lst *dbdf2val_lst);
int mlx4_get_val(struct mlx4_dbdf2val *tbl, struct pci_dev *pdev, int idx,
		 int *val);

int mlx4_register_interface(struct mlx4_interface *intf);
void mlx4_unregister_interface(struct mlx4_interface *intf);

void *mlx4_get_protocol_dev(struct mlx4_dev *dev, enum mlx4_protocol proto,
			    int port);

#ifndef ETH_ALEN
#define ETH_ALEN	6
#endif
static inline u64 mlx4_mac_to_u64(u8 *addr)
{
	u64 mac = 0;
	int i;

	for (i = 0; i < ETH_ALEN; i++) {
		mac <<= 8;
		mac |= addr[i];
	}
	return mac;
}

#endif /* MLX4_DRIVER_H */
