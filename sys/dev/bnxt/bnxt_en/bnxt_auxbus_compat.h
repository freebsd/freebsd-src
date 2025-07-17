/*-
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2024 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _BNXT_AUXILIARY_COMPAT_H_
#define _BNXT_AUXILIARY_COMPAT_H_

#include <linux/device.h>
#include <linux/idr.h>

#define KBUILD_MODNAME		"if_bnxt"
#define AUXILIARY_NAME_SIZE	32

struct auxiliary_device_id {
	char name[AUXILIARY_NAME_SIZE];
	uint64_t driver_data;
};
#define	MODULE_DEVICE_TABLE_BUS_auxiliary(_bus, _table)

struct auxiliary_device {
	struct device dev;
	const char *name;
	uint32_t id;
	struct list_head list;
};

struct auxiliary_driver {
	int (*probe)(struct auxiliary_device *auxdev, const struct auxiliary_device_id *id);
	void (*remove)(struct auxiliary_device *auxdev);
	const char *name;
	struct device_driver driver;
	const struct auxiliary_device_id *id_table;
	struct list_head list;
};

int auxiliary_device_init(struct auxiliary_device *auxdev);
int auxiliary_device_add(struct auxiliary_device *auxdev);
void auxiliary_device_uninit(struct auxiliary_device *auxdev);
void auxiliary_device_delete(struct auxiliary_device *auxdev);
int auxiliary_driver_register(struct auxiliary_driver *auxdrv);
void auxiliary_driver_unregister(struct auxiliary_driver *auxdrv);

static inline void *auxiliary_get_drvdata(struct auxiliary_device *auxdev)
{
	return dev_get_drvdata(&auxdev->dev);
}

static inline void auxiliary_set_drvdata(struct auxiliary_device *auxdev, void *data)
{
	dev_set_drvdata(&auxdev->dev, data);
}
#endif /* _BNXT_AUXILIARY_COMPAT_H_ */
