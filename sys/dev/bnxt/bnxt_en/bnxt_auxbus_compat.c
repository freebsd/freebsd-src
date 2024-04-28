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

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/delay.h>

#include "bnxt_auxbus_compat.h"

static struct list_head bnxt_aux_bus_dev_list = LINUX_LIST_HEAD_INIT(bnxt_aux_bus_dev_list);
static struct list_head bnxt_aux_bus_drv_list = LINUX_LIST_HEAD_INIT(bnxt_aux_bus_drv_list);
static DEFINE_MUTEX(bnxt_auxbus_lock);

static const struct auxiliary_device_id *auxiliary_match_id(const struct auxiliary_device_id *id,
							    const struct auxiliary_device *auxdev)
{
	for (; id->name[0]; id++) {
		const char *p = strrchr(dev_name(&auxdev->dev), '.');
		int match_size;

		if (!p)
			continue;
		match_size = p - dev_name(&auxdev->dev);

		if (strlen(id->name) == match_size &&
		    !strncmp(dev_name(&auxdev->dev), id->name, match_size))
			return id;
	}
	return NULL;
}

int auxiliary_device_init(struct auxiliary_device *auxdev)
{
	struct device *dev = &auxdev->dev;
	char *modname = KBUILD_MODNAME;
	int ret;

	if (!dev->parent) {
		pr_err("auxiliary_device has a NULL dev->parent\n");
		return -EINVAL;
	}

	if (!auxdev->name) {
		pr_err("auxiliary_device has a NULL name\n");
		return -EINVAL;
	}

	ret = dev_set_name(dev, "%s.%s.%d", modname, auxdev->name, auxdev->id);
	if (ret) {
		dev_err(dev, "auxiliary device dev_set_name failed: %d\n", ret);
		return ret;
	}

	return 0;
}

int auxiliary_device_add(struct auxiliary_device *auxdev)
{
	const struct auxiliary_device_id *id;
	struct auxiliary_driver *auxdrv = NULL;
	bool found = true;
	int ret = 0;

	mutex_lock(&bnxt_auxbus_lock);
	list_for_each_entry(auxdrv, &bnxt_aux_bus_drv_list, list) {
		if (auxdrv) {
			msleep(2 * 1000);

			id = auxiliary_match_id(auxdrv->id_table, auxdev);
			if (id) {
				ret = auxdrv->probe(auxdev, id);
				if (!ret)
					auxdev->dev.driver = &auxdrv->driver;
				else
					found = false;
				break;
			}
		}
	}

	if (found)
		list_add_tail(&auxdev->list, &bnxt_aux_bus_dev_list);
	mutex_unlock(&bnxt_auxbus_lock);

	return ret;
}

void auxiliary_device_uninit(struct auxiliary_device *auxdev)
{
	return;
}

void auxiliary_device_delete(struct auxiliary_device *auxdev)
{
	struct auxiliary_driver *auxdrv;

	mutex_lock(&bnxt_auxbus_lock);
	list_for_each_entry(auxdrv, &bnxt_aux_bus_drv_list, list) {
		if (auxdev->dev.driver != &auxdrv->driver)
			continue;
		if (auxdrv->remove)
			auxdrv->remove(auxdev);
		auxdev->dev.driver = NULL;
	}
	list_del(&auxdev->list);
	mutex_unlock(&bnxt_auxbus_lock);
}

int auxiliary_driver_register(struct auxiliary_driver *auxdrv)
{
	const struct auxiliary_device_id *id;
	struct auxiliary_device *auxdev;
	int ret = 0;

	if (WARN_ON(!auxdrv->probe) || WARN_ON(!auxdrv->id_table))
		return -EINVAL;

	if (auxdrv->name)
		auxdrv->driver.name = kasprintf(GFP_KERNEL, "%s.%s", KBUILD_MODNAME,
						auxdrv->name);
	else
		auxdrv->driver.name = kasprintf(GFP_KERNEL, "%s", KBUILD_MODNAME);
	if (!auxdrv->driver.name)
		return -ENOMEM;

	mutex_lock(&bnxt_auxbus_lock);
	list_for_each_entry(auxdev, &bnxt_aux_bus_dev_list, list) {
		if (auxdev->dev.driver)
			continue;

		id = auxiliary_match_id(auxdrv->id_table, auxdev);
		if (id) {
			ret = auxdrv->probe(auxdev, id);
			if (ret)
				continue;
			auxdev->dev.driver = &auxdrv->driver;
		}
	}
	list_add_tail(&auxdrv->list, &bnxt_aux_bus_drv_list);
	mutex_unlock(&bnxt_auxbus_lock);
	return 0;
}
EXPORT_SYMBOL(auxiliary_driver_register);

void auxiliary_driver_unregister(struct auxiliary_driver *auxdrv)
{
	struct auxiliary_device *auxdev;

	/* PF auxiliary devices are added to the list first and then VF devices.
	 * If we remove PF aux device driver first, it causes failures while
	 * removing VF driver.
	 * We need to remove VF auxiliary drivers first, so walk backwards.
	 */
	mutex_lock(&bnxt_auxbus_lock);
	list_for_each_entry_reverse(auxdev, &bnxt_aux_bus_dev_list, list) {
		if (auxdev->dev.driver != &auxdrv->driver)
			continue;
		if (auxdrv->remove)
			auxdrv->remove(auxdev);
		auxdev->dev.driver = NULL;
	}
	kfree(auxdrv->driver.name);
	list_del(&auxdrv->list);
	mutex_unlock(&bnxt_auxbus_lock);
}
EXPORT_SYMBOL(auxiliary_driver_unregister);
