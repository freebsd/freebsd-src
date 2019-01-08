/*
 * Copyright (c) 2006 QLogic, Inc.  All rights reserved.
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

#include <linux/netdevice.h>
#include <linux/parser.h>
#include <linux/if.h>

#include "vnic_util.h"
#include "vnic_config.h"
#include "vnic_ib.h"
#include "vnic_viport.h"
#include "vnic_main.h"
#include "vnic_stats.h"

/*
 * target eiocs are added by writing
 *
 * ioc_guid=<EIOC GUID>,dgid=<dest GID>,pkey=<P_key>,name=<interface_name>
 * to the create_primary  sysfs attribute.
 */
enum {
	VNIC_OPT_ERR = 0,
	VNIC_OPT_IOC_GUID = 1 << 0,
	VNIC_OPT_DGID = 1 << 1,
	VNIC_OPT_PKEY = 1 << 2,
	VNIC_OPT_NAME = 1 << 3,
	VNIC_OPT_INSTANCE = 1 << 4,
	VNIC_OPT_RXCSUM = 1 << 5,
	VNIC_OPT_TXCSUM = 1 << 6,
	VNIC_OPT_HEARTBEAT = 1 << 7,
	VNIC_OPT_IOC_STRING = 1 << 8,
	VNIC_OPT_IB_MULTICAST = 1 << 9,
	VNIC_OPT_ALL = (VNIC_OPT_IOC_GUID |
			VNIC_OPT_DGID | VNIC_OPT_NAME | VNIC_OPT_PKEY),
};

static match_table_t vnic_opt_tokens = {
	{VNIC_OPT_IOC_GUID, "ioc_guid=%s"},
	{VNIC_OPT_DGID, "dgid=%s"},
	{VNIC_OPT_PKEY, "pkey=%x"},
	{VNIC_OPT_NAME, "name=%s"},
	{VNIC_OPT_INSTANCE, "instance=%d"},
	{VNIC_OPT_RXCSUM, "rx_csum=%s"},
	{VNIC_OPT_TXCSUM, "tx_csum=%s"},
	{VNIC_OPT_HEARTBEAT, "heartbeat=%d"},
	{VNIC_OPT_IOC_STRING, "ioc_string=\"%s"},
	{VNIC_OPT_IB_MULTICAST, "ib_multicast=%s"},
	{VNIC_OPT_ERR, NULL}
};

void vnic_release_dev(struct device *dev)
{
	struct dev_info *dev_info =
	    container_of(dev, struct dev_info, dev);

	complete(&dev_info->released);

}

struct class vnic_class = {
	.name = "infiniband_qlgc_vnic",
	.dev_release = vnic_release_dev
};

struct dev_info interface_dev;

static int vnic_parse_options(const char *buf, struct path_param *param)
{
	char *options, *sep_opt;
	char *p;
	char dgid[3];
	substring_t args[MAX_OPT_ARGS];
	int opt_mask = 0;
	int token;
	int ret = -EINVAL;
	int i, len;

	options = kstrdup(buf, GFP_KERNEL);
	if (!options)
		return -ENOMEM;

	sep_opt = options;
	while ((p = strsep(&sep_opt, ",")) != NULL) {
		if (!*p)
			continue;

		token = match_token(p, vnic_opt_tokens, args);
		opt_mask |= token;

		switch (token) {
		case VNIC_OPT_IOC_GUID:
			p = match_strdup(args);
			param->ioc_guid = cpu_to_be64(simple_strtoull(p, NULL,
								      16));
			kfree(p);
			break;

		case VNIC_OPT_DGID:
			p = match_strdup(args);
			if (strlen(p) != 32) {
				printk(KERN_WARNING PFX
				       "bad dest GID parameter '%s'\n", p);
				kfree(p);
				goto out;
			}

			for (i = 0; i < 16; ++i) {
				strlcpy(dgid, p + i * 2, 3);
				param->dgid[i] = simple_strtoul(dgid, NULL,
								16);

			}
			kfree(p);
			break;

		case VNIC_OPT_PKEY:
			if (match_hex(args, &token)) {
				printk(KERN_WARNING PFX
				       "bad P_key parameter '%s'\n", p);
				goto out;
			}
			param->pkey = cpu_to_be16(token);
			break;

		case VNIC_OPT_NAME:
			p = match_strdup(args);
			if (strlen(p) >= IFNAMSIZ) {
				printk(KERN_WARNING PFX
				       "interface name parameter too long\n");
				kfree(p);
				goto out;
			}
			strcpy(param->name, p);
			kfree(p);
			break;
		case VNIC_OPT_INSTANCE:
			if (match_int(args, &token)) {
				printk(KERN_WARNING PFX
				       "bad instance parameter '%s'\n", p);
				goto out;
			}

			if (token > 255 || token < 0) {
				printk(KERN_WARNING PFX
				       "instance parameter must be"
				       " >= 0 and <= 255\n");
				goto out;
			}

			param->instance = token;
			break;
		case VNIC_OPT_RXCSUM:
			p = match_strdup(args);
			if (!strncmp(p, "true", 4))
				param->rx_csum = 1;
			else if (!strncmp(p, "false", 5))
				param->rx_csum = 0;
			else {
				printk(KERN_WARNING PFX
				       "bad rx_csum parameter."
				       " must be 'true' or 'false'\n");
				kfree(p);
				goto out;
			}
			kfree(p);
			break;
		case VNIC_OPT_TXCSUM:
			p = match_strdup(args);
			if (!strncmp(p, "true", 4))
				param->tx_csum = 1;
			else if (!strncmp(p, "false", 5))
				param->tx_csum = 0;
			else {
				printk(KERN_WARNING PFX
				       "bad tx_csum parameter."
				       " must be 'true' or 'false'\n");
				kfree(p);
				goto out;
			}
			kfree(p);
			break;
		case VNIC_OPT_HEARTBEAT:
			if (match_int(args, &token)) {
				printk(KERN_WARNING PFX
				       "bad instance parameter '%s'\n", p);
				goto out;
			}

			if (token > 6000 || token <= 0) {
				printk(KERN_WARNING PFX
				       "heartbeat parameter must be"
				       " > 0 and <= 6000\n");
				goto out;
			}
			param->heartbeat = token;
			break;
		case VNIC_OPT_IOC_STRING:
			p = match_strdup(args);
			len = strlen(p);
			if (len > MAX_IOC_STRING_LEN) {
				printk(KERN_WARNING PFX
				       "ioc string parameter too long\n");
				kfree(p);
				goto out;
			}
			strcpy(param->ioc_string, p);
			if (*(p + len - 1) != '\"') {
				strcat(param->ioc_string, ",");
				kfree(p);
				p = strsep(&sep_opt, "\"");
				strcat(param->ioc_string, p);
				sep_opt++;
			} else {
				*(param->ioc_string + len - 1) = '\0';
				kfree(p);
			}
			break;
		case VNIC_OPT_IB_MULTICAST:
			p = match_strdup(args);
			if (!strncmp(p, "true", 4))
				param->ib_multicast = 1;
			else if (!strncmp(p, "false", 5))
				param->ib_multicast = 0;
			else {
					printk(KERN_WARNING PFX
					"bad ib_multicast parameter."
					" must be 'true' or 'false'\n");
				kfree(p);
				goto out;
			}
			kfree(p);
			break;
		default:
			printk(KERN_WARNING PFX
			       "unknown parameter or missing value "
			       "'%s' in target creation request\n", p);
			goto out;
		}

	}

	if ((opt_mask & VNIC_OPT_ALL) == VNIC_OPT_ALL)
		ret = 0;
	else
		for (i = 0; i < ARRAY_SIZE(vnic_opt_tokens); ++i)
			if ((vnic_opt_tokens[i].token & VNIC_OPT_ALL) &&
			    !(vnic_opt_tokens[i].token & opt_mask))
				printk(KERN_WARNING PFX
				       "target creation request is "
				       "missing parameter '%s'\n",
				       vnic_opt_tokens[i].pattern);

out:
	kfree(options);
	return ret;

}

static ssize_t show_vnic_state(struct device *dev,
			       struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, dev_info);
	switch (vnic->state) {
	case VNIC_UNINITIALIZED:
		return sprintf(buf, "VNIC_UNINITIALIZED\n");
	case VNIC_REGISTERED:
		return sprintf(buf, "VNIC_REGISTERED\n");
	default:
		return sprintf(buf, "INVALID STATE\n");
	}

}

static DEVICE_ATTR(vnic_state, S_IRUGO, show_vnic_state, NULL);

static ssize_t show_rx_csum(struct device *dev,
			    struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, dev_info);

	if (vnic->config->use_rx_csum)
		return sprintf(buf, "true\n");
	else
		return sprintf(buf, "false\n");
}

static DEVICE_ATTR(rx_csum, S_IRUGO, show_rx_csum, NULL);

static ssize_t show_tx_csum(struct device *dev,
			    struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, dev_info);

	if (vnic->config->use_tx_csum)
		return sprintf(buf, "true\n");
	else
		return sprintf(buf, "false\n");
}

static DEVICE_ATTR(tx_csum, S_IRUGO, show_tx_csum, NULL);

static ssize_t show_current_path(struct device *dev,
				 struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);
	struct vnic *vnic = container_of(info, struct vnic, dev_info);
	unsigned long flags;
	size_t length;

	spin_lock_irqsave(&vnic->current_path_lock, flags);
	if (vnic->current_path == &vnic->primary_path)
		length = sprintf(buf, "primary_path\n");
	else if (vnic->current_path == &vnic->secondary_path)
		length = sprintf(buf, "secondary path\n");
	else
		length = sprintf(buf, "none\n");
	spin_unlock_irqrestore(&vnic->current_path_lock, flags);
	return length;
}

static DEVICE_ATTR(current_path, S_IRUGO, show_current_path, NULL);

static struct attribute *vnic_dev_attrs[] = {
	&dev_attr_vnic_state.attr,
	&dev_attr_rx_csum.attr,
	&dev_attr_tx_csum.attr,
	&dev_attr_current_path.attr,
	NULL
};

struct attribute_group vnic_dev_attr_group = {
	.attrs = vnic_dev_attrs,
};

static inline void print_dgid(u8 *dgid)
{
	int i;

	for (i = 0; i < 16; i += 2)
		printk("%04x", be16_to_cpu(*(__be16 *)&dgid[i]));
}

static inline int is_dgid_zero(u8 *dgid)
{
	int i;

	for (i = 0; i < 16; i++) {
		if (dgid[i] != 0)
			return 1;
	}
	return 0;
}

static int create_netpath(struct netpath *npdest,
			  struct path_param *p_params)
{
	struct viport_config	*viport_config;
	struct viport		*viport;
	struct vnic		*vnic;
	struct list_head	*ptr;
	int			ret = 0;

	list_for_each(ptr, &vnic_list) {
		vnic = list_entry(ptr, struct vnic, list_ptrs);
		if (vnic->primary_path.viport) {
			viport_config = vnic->primary_path.viport->config;
			if ((viport_config->ioc_guid == p_params->ioc_guid)
			    && (viport_config->control_config.vnic_instance
				== p_params->instance)
			    && (be64_to_cpu(p_params->ioc_guid))) {
				SYS_ERROR("GUID %llx,"
					  " INSTANCE %d already in use\n",
					  be64_to_cpu(p_params->ioc_guid),
					  p_params->instance);
				ret = -EINVAL;
				goto out;
			}
		}

		if (vnic->secondary_path.viport) {
			viport_config = vnic->secondary_path.viport->config;
			if ((viport_config->ioc_guid == p_params->ioc_guid)
			    && (viport_config->control_config.vnic_instance
				== p_params->instance)
			    && (be64_to_cpu(p_params->ioc_guid))) {
				SYS_ERROR("GUID %llx,"
					  " INSTANCE %d already in use\n",
					  be64_to_cpu(p_params->ioc_guid),
					  p_params->instance);
				ret = -EINVAL;
				goto out;
			}
		}
	}

	if (npdest->viport) {
		SYS_ERROR("create_netpath: path already exists\n");
		ret = -EINVAL;
		goto out;
	}

	viport_config = config_alloc_viport(p_params);
	if (!viport_config) {
		SYS_ERROR("create_netpath: failed creating viport config\n");
		ret = -1;
		goto out;
	}

	/*User specified heartbeat value is in 1/100s of a sec*/
	if (p_params->heartbeat != -1) {
		viport_config->hb_interval =
			msecs_to_jiffies(p_params->heartbeat * 10);
		viport_config->hb_timeout =
			(p_params->heartbeat << 6) * 10000; /* usec */
	}

	viport_config->path_idx = 0;

	viport = viport_allocate(viport_config);
	if (!viport) {
		SYS_ERROR("create_netpath: failed creating viport\n");
		kfree(viport_config);
		ret = -1;
		goto out;
	}

	npdest->viport = viport;
	viport->parent = npdest;
	viport->vnic = npdest->parent;

	if (is_dgid_zero(p_params->dgid) &&  p_params->ioc_guid != 0
	   &&  p_params->pkey != 0) {
		viport_kick(viport);
		vnic_disconnected(npdest->parent, npdest);
	} else {
		printk(KERN_WARNING "Specified parameters IOCGUID=%llx, "
			"P_Key=%x, DGID=", be64_to_cpu(p_params->ioc_guid),
			p_params->pkey);
		print_dgid(p_params->dgid);
		printk(" insufficient for establishing %s path for interface "
			"%s. Hence, path will not be established.\n",
			(npdest->second_bias ? "secondary" : "primary"),
			p_params->name);
	}
out:
	return ret;
}

static struct vnic *create_vnic(struct path_param *param)
{
	struct vnic_config *vnic_config;
	struct vnic *vnic;
	struct list_head *ptr;

	SYS_INFO("create_vnic: name = %s\n", param->name);
	list_for_each(ptr, &vnic_list) {
		vnic = list_entry(ptr, struct vnic, list_ptrs);
		if (!strcmp(vnic->config->name, param->name)) {
			SYS_ERROR("vnic %s already exists\n",
				   param->name);
			return NULL;
		}
	}

	vnic_config = config_alloc_vnic();
	if (!vnic_config) {
		SYS_ERROR("create_vnic: failed creating vnic config\n");
		return NULL;
	}

	if (param->rx_csum != -1)
		vnic_config->use_rx_csum = param->rx_csum;

	if (param->tx_csum != -1)
		vnic_config->use_tx_csum = param->tx_csum;

	strcpy(vnic_config->name, param->name);
	vnic = vnic_allocate(vnic_config);
	if (!vnic) {
		SYS_ERROR("create_vnic: failed allocating vnic\n");
		goto free_vnic_config;
	}

	init_completion(&vnic->dev_info.released);

	vnic->dev_info.dev.class = NULL;
	vnic->dev_info.dev.parent = &interface_dev.dev;
	vnic->dev_info.dev.release = vnic_release_dev;
	dev_set_name(&vnic->dev_info.dev, vnic_config->name);

	if (device_register(&vnic->dev_info.dev)) {
		SYS_ERROR("create_vnic: error in registering"
			  " vnic class dev\n");
		goto free_vnic;
	}

	if (sysfs_create_group(&vnic->dev_info.dev.kobj,
			       &vnic_dev_attr_group)) {
		SYS_ERROR("create_vnic: error in creating"
			  "vnic attr group\n");
		goto err_attr;

	}

	if (vnic_setup_stats_files(vnic))
		goto err_stats;

	return vnic;
err_stats:
	sysfs_remove_group(&vnic->dev_info.dev.kobj,
			   &vnic_dev_attr_group);
err_attr:
	device_unregister(&vnic->dev_info.dev);
	wait_for_completion(&vnic->dev_info.released);
free_vnic:
	list_del(&vnic->list_ptrs);
	kfree(vnic);
free_vnic_config:
	kfree(vnic_config);
	return NULL;
}

static ssize_t vnic_sysfs_force_failover(struct device *dev,
					struct device_attribute *dev_attr, const char *buf,
					size_t count)
{
	struct vnic *vnic;
	struct list_head *ptr;
	int ret = -EINVAL;

	if (count > IFNAMSIZ) {
		printk(KERN_WARNING PFX "invalid vnic interface name\n");
		return ret;
	}

	SYS_INFO("vnic_sysfs_force_failover: name = %s\n", buf);
	list_for_each(ptr, &vnic_list) {
		vnic = list_entry(ptr, struct vnic, list_ptrs);
		if (!strcmp(vnic->config->name, buf)) {
			vnic_force_failover(vnic);
			return count;
		}
	}

	printk(KERN_WARNING PFX "vnic interface '%s' does not exist\n", buf);
	return ret;
}

DEVICE_ATTR(force_failover, S_IWUSR, NULL, vnic_sysfs_force_failover);

static ssize_t vnic_sysfs_unfailover(struct device *dev,
					struct device_attribute *dev_attr, const char *buf,
					size_t count)
{
	struct vnic *vnic;
	struct list_head *ptr;
	int ret = -EINVAL;

	if (count > IFNAMSIZ) {
		printk(KERN_WARNING PFX "invalid vnic interface name\n");
		return ret;
	}

	SYS_INFO("vnic_sysfs_unfailover: name = %s\n", buf);
	list_for_each(ptr, &vnic_list) {
		vnic = list_entry(ptr, struct vnic, list_ptrs);
		if (!strcmp(vnic->config->name, buf)) {
			vnic_unfailover(vnic);
			return count;
		}
	}

	printk(KERN_WARNING PFX "vnic interface '%s' does not exist\n", buf);
	return ret;
}

DEVICE_ATTR(unfailover, S_IWUSR, NULL, vnic_sysfs_unfailover);

static ssize_t vnic_delete(struct device *dev, struct device_attribute *dev_attr,
			   const char *buf, size_t count)
{
	struct vnic *vnic;
	struct list_head *ptr;
	int ret = -EINVAL;

	if (count > IFNAMSIZ) {
		printk(KERN_WARNING PFX "invalid vnic interface name\n");
		return ret;
	}

	SYS_INFO("vnic_delete: name = %s\n", buf);
	list_for_each(ptr, &vnic_list) {
		vnic = list_entry(ptr, struct vnic, list_ptrs);
		if (!strcmp(vnic->config->name, buf)) {
			vnic_free(vnic);
			return count;
		}
	}

	printk(KERN_WARNING PFX "vnic interface '%s' does not exist\n", buf);
	return ret;
}

DEVICE_ATTR(delete_vnic, S_IWUSR, NULL, vnic_delete);

static ssize_t show_viport_state(struct device *dev,
				 struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);
	struct netpath *path = container_of(info, struct netpath, dev_info);
	switch (path->viport->state) {
	case VIPORT_DISCONNECTED:
		return sprintf(buf, "VIPORT_DISCONNECTED\n");
	case VIPORT_CONNECTED:
		return sprintf(buf, "VIPORT_CONNECTED\n");
	default:
		return sprintf(buf, "INVALID STATE\n");
	}

}

static DEVICE_ATTR(viport_state, S_IRUGO, show_viport_state, NULL);

static ssize_t show_link_state(struct device *dev,
			       struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);
	struct netpath *path = container_of(info, struct netpath, dev_info);

	switch (path->viport->link_state) {
	case LINK_UNINITIALIZED:
		return sprintf(buf, "LINK_UNINITIALIZED\n");
	case LINK_INITIALIZE:
		return sprintf(buf, "LINK_INITIALIZE\n");
	case LINK_INITIALIZECONTROL:
		return sprintf(buf, "LINK_INITIALIZECONTROL\n");
	case LINK_INITIALIZEDATA:
		return sprintf(buf, "LINK_INITIALIZEDATA\n");
	case LINK_CONTROLCONNECT:
		return sprintf(buf, "LINK_CONTROLCONNECT\n");
	case LINK_CONTROLCONNECTWAIT:
		return sprintf(buf, "LINK_CONTROLCONNECTWAIT\n");
	case LINK_INITVNICREQ:
		return sprintf(buf, "LINK_INITVNICREQ\n");
	case LINK_INITVNICRSP:
		return sprintf(buf, "LINK_INITVNICRSP\n");
	case LINK_BEGINDATAPATH:
		return sprintf(buf, "LINK_BEGINDATAPATH\n");
	case LINK_CONFIGDATAPATHREQ:
		return sprintf(buf, "LINK_CONFIGDATAPATHREQ\n");
	case LINK_CONFIGDATAPATHRSP:
		return sprintf(buf, "LINK_CONFIGDATAPATHRSP\n");
	case LINK_DATACONNECT:
		return sprintf(buf, "LINK_DATACONNECT\n");
	case LINK_DATACONNECTWAIT:
		return sprintf(buf, "LINK_DATACONNECTWAIT\n");
	case LINK_XCHGPOOLREQ:
		return sprintf(buf, "LINK_XCHGPOOLREQ\n");
	case LINK_XCHGPOOLRSP:
		return sprintf(buf, "LINK_XCHGPOOLRSP\n");
	case LINK_INITIALIZED:
		return sprintf(buf, "LINK_INITIALIZED\n");
	case LINK_IDLE:
		return sprintf(buf, "LINK_IDLE\n");
	case LINK_IDLING:
		return sprintf(buf, "LINK_IDLING\n");
	case LINK_CONFIGLINKREQ:
		return sprintf(buf, "LINK_CONFIGLINKREQ\n");
	case LINK_CONFIGLINKRSP:
		return sprintf(buf, "LINK_CONFIGLINKRSP\n");
	case LINK_CONFIGADDRSREQ:
		return sprintf(buf, "LINK_CONFIGADDRSREQ\n");
	case LINK_CONFIGADDRSRSP:
		return sprintf(buf, "LINK_CONFIGADDRSRSP\n");
	case LINK_REPORTSTATREQ:
		return sprintf(buf, "LINK_REPORTSTATREQ\n");
	case LINK_REPORTSTATRSP:
		return sprintf(buf, "LINK_REPORTSTATRSP\n");
	case LINK_HEARTBEATREQ:
		return sprintf(buf, "LINK_HEARTBEATREQ\n");
	case LINK_HEARTBEATRSP:
		return sprintf(buf, "LINK_HEARTBEATRSP\n");
	case LINK_RESET:
		return sprintf(buf, "LINK_RESET\n");
	case LINK_RESETRSP:
		return sprintf(buf, "LINK_RESETRSP\n");
	case LINK_RESETCONTROL:
		return sprintf(buf, "LINK_RESETCONTROL\n");
	case LINK_RESETCONTROLRSP:
		return sprintf(buf, "LINK_RESETCONTROLRSP\n");
	case LINK_DATADISCONNECT:
		return sprintf(buf, "LINK_DATADISCONNECT\n");
	case LINK_CONTROLDISCONNECT:
		return sprintf(buf, "LINK_CONTROLDISCONNECT\n");
	case LINK_CLEANUPDATA:
		return sprintf(buf, "LINK_CLEANUPDATA\n");
	case LINK_CLEANUPCONTROL:
		return sprintf(buf, "LINK_CLEANUPCONTROL\n");
	case LINK_DISCONNECTED:
		return sprintf(buf, "LINK_DISCONNECTED\n");
	case LINK_RETRYWAIT:
		return sprintf(buf, "LINK_RETRYWAIT\n");
	default:
		return sprintf(buf, "INVALID STATE\n");

	}

}
static DEVICE_ATTR(link_state, S_IRUGO, show_link_state, NULL);

static ssize_t show_heartbeat(struct device *dev,
			      struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);

	struct netpath *path = container_of(info, struct netpath, dev_info);

	/* hb_inteval is in jiffies, convert it back to
	 * 1/100ths of a second
	 */
	return sprintf(buf, "%d\n",
		(jiffies_to_msecs(path->viport->config->hb_interval)/10));
}

static DEVICE_ATTR(heartbeat, S_IRUGO, show_heartbeat, NULL);

static ssize_t show_ioc_guid(struct device *dev,
			     struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);

	struct netpath *path = container_of(info, struct netpath, dev_info);

	return sprintf(buf, "%llx\n",
				__be64_to_cpu(path->viport->config->ioc_guid));
}

static DEVICE_ATTR(ioc_guid, S_IRUGO, show_ioc_guid, NULL);

static inline void get_dgid_string(u8 *dgid, char *buf)
{
	int i;
	char holder[5];

	for (i = 0; i < 16; i += 2) {
		sprintf(holder, "%04x", be16_to_cpu(*(__be16 *)&dgid[i]));
		strcat(buf, holder);
	}

	strcat(buf, "\n");
}

static ssize_t show_dgid(struct device *dev,
			 struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);

	struct netpath *path = container_of(info, struct netpath, dev_info);

	get_dgid_string(path->viport->config->path_info.path.dgid.raw, buf);

	return strlen(buf);
}

static DEVICE_ATTR(dgid, S_IRUGO, show_dgid, NULL);

static ssize_t show_pkey(struct device *dev,
			 struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);

	struct netpath *path = container_of(info, struct netpath, dev_info);

	return sprintf(buf, "%x\n", path->viport->config->path_info.path.pkey);
}

static DEVICE_ATTR(pkey, S_IRUGO, show_pkey, NULL);

static ssize_t show_hca_info(struct device *dev,
			     struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);

	struct netpath *path = container_of(info, struct netpath, dev_info);

	return sprintf(buf, "vnic-%s-%d\n", path->viport->config->ibdev->name,
						path->viport->config->port);
}

static DEVICE_ATTR(hca_info, S_IRUGO, show_hca_info, NULL);

static ssize_t show_ioc_string(struct device *dev,
			       struct device_attribute *dev_attr, char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);

	struct netpath *path = container_of(info, struct netpath, dev_info);

	return sprintf(buf, "%s\n", path->viport->config->ioc_string);
}

static  DEVICE_ATTR(ioc_string, S_IRUGO, show_ioc_string, NULL);

static ssize_t show_multicast_state(struct device *dev,
				    struct device_attribute *dev_attr,
				    char *buf)
{
	struct dev_info *info =	container_of(dev, struct dev_info, dev);

	struct netpath *path = container_of(info, struct netpath, dev_info);

	if (!(path->viport->features_supported & VNIC_FEAT_INBOUND_IB_MC))
		return sprintf(buf, "feature not enabled\n");

	switch (path->viport->mc_info.state) {
	case MCAST_STATE_INVALID:
		return sprintf(buf, "state=Invalid\n");
	case MCAST_STATE_JOINING:
		return sprintf(buf, "state=Joining MGID:" VNIC_GID_FMT "\n",
			VNIC_GID_RAW_ARG(path->viport->mc_info.mgid.raw));
	case MCAST_STATE_ATTACHING:
		return sprintf(buf, "state=Attaching MGID:" VNIC_GID_FMT
			" MLID:%X\n",
			VNIC_GID_RAW_ARG(path->viport->mc_info.mgid.raw),
			path->viport->mc_info.mlid);
	case MCAST_STATE_JOINED_ATTACHED:
		return sprintf(buf,
			"state=Joined & Attached MGID:" VNIC_GID_FMT
			" MLID:%X\n",
			VNIC_GID_RAW_ARG(path->viport->mc_info.mgid.raw),
			path->viport->mc_info.mlid);
	case MCAST_STATE_DETACHING:
		return sprintf(buf, "state=Detaching MGID: " VNIC_GID_FMT "\n",
			VNIC_GID_RAW_ARG(path->viport->mc_info.mgid.raw));
	case MCAST_STATE_RETRIED:
		return sprintf(buf, "state=Retries Exceeded\n");
	}
	return sprintf(buf, "invalid state\n");
}

static  DEVICE_ATTR(multicast_state, S_IRUGO, show_multicast_state, NULL);

static struct attribute *vnic_path_attrs[] = {
	&dev_attr_viport_state.attr,
	&dev_attr_link_state.attr,
	&dev_attr_heartbeat.attr,
	&dev_attr_ioc_guid.attr,
	&dev_attr_dgid.attr,
	&dev_attr_pkey.attr,
	&dev_attr_hca_info.attr,
	&dev_attr_ioc_string.attr,
	&dev_attr_multicast_state.attr,
	NULL
};

struct attribute_group vnic_path_attr_group = {
	.attrs = vnic_path_attrs,
};


static int setup_path_class_files(struct netpath *path, char *name)
{
	init_completion(&path->dev_info.released);

	path->dev_info.dev.class = NULL;
	path->dev_info.dev.parent = &path->parent->dev_info.dev;
	path->dev_info.dev.release = vnic_release_dev;
	dev_set_name(&path->dev_info.dev, name);

	if (device_register(&path->dev_info.dev)) {
		SYS_ERROR("error in registering path class dev\n");
		goto out;
	}

	if (sysfs_create_group(&path->dev_info.dev.kobj,
			       &vnic_path_attr_group)) {
		SYS_ERROR("error in creating vnic path group attrs");
		goto err_path;
	}

	return 0;

err_path:
	device_unregister(&path->dev_info.dev);
	wait_for_completion(&path->dev_info.released);
out:
	return -1;

}

static inline void update_dgids(u8 *old, u8 *new, char *vnic_name,
				char *path_name)
{
	int i;

	if (!memcmp(old, new, 16))
		return;

	printk(KERN_INFO PFX "Changing dgid from 0x");
	print_dgid(old);
	printk(" to 0x");
	print_dgid(new);
	printk(" for %s path of %s\n", path_name, vnic_name);
	for (i = 0; i < 16; i++)
		old[i] = new[i];
}

static inline void update_ioc_guids(struct path_param *params,
				    struct netpath *path,
				    char *vnic_name, char *path_name)
{
	u64 sid;

	if (path->viport->config->ioc_guid == params->ioc_guid)
		return;

	printk(KERN_INFO PFX "Changing IOC GUID from 0x%llx to 0x%llx "
			 "for %s path of %s\n",
			 __be64_to_cpu(path->viport->config->ioc_guid),
			 __be64_to_cpu(params->ioc_guid), path_name, vnic_name);

	path->viport->config->ioc_guid = params->ioc_guid;

	sid = (SST_AGN << 56) | (SST_OUI << 32) | (CONTROL_PATH_ID << 8)
				| IOC_NUMBER(be64_to_cpu(params->ioc_guid));

	path->viport->config->control_config.ib_config.service_id =
							 cpu_to_be64(sid);

	sid = (SST_AGN << 56) | (SST_OUI << 32) | (DATA_PATH_ID << 8)
				| IOC_NUMBER(be64_to_cpu(params->ioc_guid));

	path->viport->config->data_config.ib_config.service_id =
							 cpu_to_be64(sid);
}

static inline void update_pkeys(__be16 *old, __be16 *new, char *vnic_name,
				char *path_name)
{
	if (*old == *new)
		return;

	printk(KERN_INFO PFX "Changing P_Key from 0x%x to 0x%x "
			 "for %s path of %s\n", *old, *new,
			 path_name, vnic_name);
	*old = *new;
}

static void update_ioc_strings(struct path_param *params, struct netpath *path,
								char *path_name)
{
	if (!strcmp(params->ioc_string, path->viport->config->ioc_string))
		return;

	printk(KERN_INFO PFX "Changing ioc_string to %s for %s path of %s\n",
				params->ioc_string, path_name, params->name);

	strcpy(path->viport->config->ioc_string, params->ioc_string);
}

static void update_path_parameters(struct path_param *params,
				   struct netpath *path)
{
	update_dgids(path->viport->config->path_info.path.dgid.raw,
		params->dgid, params->name,
		(path->second_bias ? "secondary" : "primary"));

	update_ioc_guids(params, path, params->name,
		(path->second_bias ? "secondary" : "primary"));

	update_pkeys(&path->viport->config->path_info.path.pkey,
		&params->pkey, params->name,
		(path->second_bias ? "secondary" : "primary"));

	update_ioc_strings(params, path,
		(path->second_bias ? "secondary" : "primary"));
}

static ssize_t update_params_and_connect(struct path_param *params,
					 struct netpath *path, size_t count)
{
	if (is_dgid_zero(params->dgid) && params->ioc_guid != 0 &&
	    params->pkey != 0) {

		if (!memcmp(path->viport->config->path_info.path.dgid.raw,
			params->dgid, 16) &&
		    params->ioc_guid == path->viport->config->ioc_guid &&
		    params->pkey     == path->viport->config->path_info.path.pkey) {

			printk(KERN_WARNING PFX "All of the dgid, ioc_guid and "
						"pkeys are same as the existing"
						" one. Not updating values.\n");
			return -EINVAL;
		} else {
			if (path->viport->state == VIPORT_CONNECTED) {
				printk(KERN_WARNING PFX "%s path of %s "
					"interface is already in connected "
					"state. Not updating values.\n",
				(path->second_bias ? "Secondary" : "Primary"),
				path->parent->config->name);
				return -EINVAL;
			} else {
				update_path_parameters(params, path);
				viport_kick(path->viport);
				vnic_disconnected(path->parent, path);
				return count;
			}
		}
	} else {
		printk(KERN_WARNING PFX "Either dgid, iocguid, pkey is zero. "
					"No update.\n");
		return -EINVAL;
	}
}

static ssize_t vnic_create_primary(struct device *dev,
				   struct device_attribute *dev_attr,
				   const char *buf, size_t count)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);
	struct vnic_ib_port *target =
	    container_of(info, struct vnic_ib_port, pdev_info);

	struct path_param param;
	int ret = -EINVAL;
	struct vnic *vnic;
	struct list_head    *ptr;

	param.instance = 0;
	param.rx_csum = -1;
	param.tx_csum = -1;
	param.heartbeat = -1;
	param.ib_multicast = -1;
	*param.ioc_string = '\0';

	ret = vnic_parse_options(buf, &param);

	if (ret)
		goto out;

	list_for_each(ptr, &vnic_list) {
		vnic = list_entry(ptr, struct vnic, list_ptrs);
		if (!strcmp(vnic->config->name, param.name)) {
			ret = update_params_and_connect(&param,
							&vnic->primary_path,
							count);
			goto out;
		}
	 }

	param.ibdev = target->dev->dev;
	param.ibport = target;
	param.port = target->port_num;

	vnic = create_vnic(&param);
	if (!vnic) {
		printk(KERN_ERR PFX "creating vnic failed\n");
		ret = -EINVAL;
		goto out;
	}

	if (create_netpath(&vnic->primary_path, &param)) {
		printk(KERN_ERR PFX "creating primary netpath failed\n");
		goto free_vnic;
	}

	if (setup_path_class_files(&vnic->primary_path, "primary_path"))
		goto free_vnic;

	if (vnic && !vnic->primary_path.viport) {
		printk(KERN_ERR PFX "no valid netpaths\n");
		goto free_vnic;
	}

	return count;

free_vnic:
	vnic_free(vnic);
	ret = -EINVAL;
out:
	return ret;
}

DEVICE_ATTR(create_primary, S_IWUSR, NULL, vnic_create_primary);

static ssize_t vnic_create_secondary(struct device *dev,
				     struct device_attribute *dev_attr,
				     const char *buf, size_t count)
{
	struct dev_info *info = container_of(dev, struct dev_info, dev);
	struct vnic_ib_port *target =
	    container_of(info, struct vnic_ib_port, pdev_info);

	struct path_param param;
	struct vnic *vnic = NULL;
	int ret = -EINVAL;
	struct list_head *ptr;
	int found = 0;

	param.instance = 0;
	param.rx_csum = -1;
	param.tx_csum = -1;
	param.heartbeat = -1;
	param.ib_multicast = -1;
	*param.ioc_string = '\0';

	ret = vnic_parse_options(buf, &param);

	if (ret)
		goto out;

	list_for_each(ptr, &vnic_list) {
		vnic = list_entry(ptr, struct vnic, list_ptrs);
		if (!strncmp(vnic->config->name, param.name, IFNAMSIZ)) {
			if (vnic->secondary_path.viport) {
				ret = update_params_and_connect(&param,
								&vnic->secondary_path,
								count);
				goto out;
			}
			found = 1;
			break;
		}
	}

	if (!found) {
		printk(KERN_ERR PFX
		       "primary connection with name '%s' does not exist\n",
		       param.name);
		ret = -EINVAL;
		goto out;
	}

	param.ibdev = target->dev->dev;
	param.ibport = target;
	param.port = target->port_num;

	if (create_netpath(&vnic->secondary_path, &param)) {
		printk(KERN_ERR PFX "creating secondary netpath failed\n");
		ret = -EINVAL;
		goto out;
	}

	if (setup_path_class_files(&vnic->secondary_path, "secondary_path"))
		goto free_vnic;

	return count;

free_vnic:
	vnic_free(vnic);
	ret = -EINVAL;
out:
	return ret;
}

DEVICE_ATTR(create_secondary, S_IWUSR, NULL, vnic_create_secondary);
