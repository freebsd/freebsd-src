/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <dirent.h>
#include <errno.h>

#include "ibverbs.h"

HIDDEN int abi_ver;

struct ibv_sysfs_dev {
	char		        sysfs_name[IBV_SYSFS_NAME_MAX];
	char		        ibdev_name[IBV_SYSFS_NAME_MAX];
	char		        sysfs_path[IBV_SYSFS_PATH_MAX];
	char		        ibdev_path[IBV_SYSFS_PATH_MAX];
	struct ibv_sysfs_dev   *next;
	int			abi_ver;
	int			have_driver;
};

struct ibv_driver_name {
	char		       *name;
	struct ibv_driver_name *next;
};

struct ibv_driver {
	const char	       *name;
	ibv_driver_init_func	init_func;
	struct ibv_driver      *next;
};

static struct ibv_sysfs_dev *sysfs_dev_list;
static struct ibv_driver_name *driver_name_list;
static struct ibv_driver *head_driver, *tail_driver;

static int find_sysfs_devs(void)
{
#ifdef __linux__
	char class_path[IBV_SYSFS_PATH_MAX];
	DIR *class_dir;
	struct dirent *dent;
	struct ibv_sysfs_dev *sysfs_dev = NULL;
	char value[8];
	int ret = 0;

	snprintf(class_path, sizeof class_path, "%s/class/infiniband_verbs",
		 ibv_get_sysfs_path());

	class_dir = opendir(class_path);
	if (!class_dir)
		return ENOSYS;

	while ((dent = readdir(class_dir))) {
		struct stat buf;

		if (dent->d_name[0] == '.')
			continue;

		if (!sysfs_dev)
			sysfs_dev = malloc(sizeof *sysfs_dev);
		if (!sysfs_dev) {
			ret = ENOMEM;
			goto out;
		}

		snprintf(sysfs_dev->sysfs_path, sizeof sysfs_dev->sysfs_path,
			 "%s/%s", class_path, dent->d_name);

		if (stat(sysfs_dev->sysfs_path, &buf)) {
			fprintf(stderr, PFX "Warning: couldn't stat '%s'.\n",
				sysfs_dev->sysfs_path);
			continue;
		}

		if (!S_ISDIR(buf.st_mode))
			continue;

		snprintf(sysfs_dev->sysfs_name, sizeof sysfs_dev->sysfs_name,
			"%s", dent->d_name);

		if (ibv_read_sysfs_file(sysfs_dev->sysfs_path, "ibdev",
					sysfs_dev->ibdev_name,
					sizeof sysfs_dev->ibdev_name) < 0) {
			fprintf(stderr, PFX "Warning: no ibdev class attr for '%s'.\n",
				dent->d_name);
			continue;
		}

		snprintf(sysfs_dev->ibdev_path, sizeof sysfs_dev->ibdev_path,
			 "%s/class/infiniband/%s", ibv_get_sysfs_path(),
			 sysfs_dev->ibdev_name);

		sysfs_dev->next        = sysfs_dev_list;
		sysfs_dev->have_driver = 0;
		if (ibv_read_sysfs_file(sysfs_dev->sysfs_path, "abi_version",
					value, sizeof value) > 0)
			sysfs_dev->abi_ver = strtol(value, NULL, 10);
		else
			sysfs_dev->abi_ver = 0;

		sysfs_dev_list = sysfs_dev;
		sysfs_dev      = NULL;
	}

 out:
	if (sysfs_dev)
		free(sysfs_dev);

	closedir(class_dir);
	return ret;
#else
	char class_path[IBV_SYSFS_PATH_MAX];
	struct ibv_sysfs_dev *sysfs_dev = NULL;
	char value[8];
	int ret = 0;
	int i;

	snprintf(class_path, sizeof class_path, "%s/class/infiniband_verbs",
		 ibv_get_sysfs_path());

	for (i = 0; i < 256; i++) {
		if (!sysfs_dev)
			sysfs_dev = malloc(sizeof *sysfs_dev);
		if (!sysfs_dev) {
			ret = ENOMEM;
			goto out;
		}

		snprintf(sysfs_dev->sysfs_path, sizeof sysfs_dev->sysfs_path,
			 "%s/uverbs%d", class_path, i);

		snprintf(sysfs_dev->sysfs_name, sizeof sysfs_dev->sysfs_name,
			"uverbs%d", i);

		if (ibv_read_sysfs_file(sysfs_dev->sysfs_path, "ibdev",
					sysfs_dev->ibdev_name,
					sizeof sysfs_dev->ibdev_name) < 0)
			continue;

		snprintf(sysfs_dev->ibdev_path, sizeof sysfs_dev->ibdev_path,
			 "%s/class/infiniband/%s", ibv_get_sysfs_path(),
			 sysfs_dev->ibdev_name);

		sysfs_dev->next        = sysfs_dev_list;
		sysfs_dev->have_driver = 0;
		if (ibv_read_sysfs_file(sysfs_dev->sysfs_path, "abi_version",
					value, sizeof value) > 0)
			sysfs_dev->abi_ver = strtol(value, NULL, 10);
		else
			sysfs_dev->abi_ver = 0;

		sysfs_dev_list = sysfs_dev;
		sysfs_dev      = NULL;
	}

 out:
	if (sysfs_dev)
		free(sysfs_dev);

	return ret;
	
#endif
}

void ibv_register_driver(const char *name, ibv_driver_init_func init_func)
{
	struct ibv_driver *driver;

	driver = malloc(sizeof *driver);
	if (!driver) {
		fprintf(stderr, PFX "Warning: couldn't allocate driver for %s\n", name);
		return;
	}

	driver->name      = name;
	driver->init_func = init_func;
	driver->next      = NULL;

	if (tail_driver)
		tail_driver->next = driver;
	else
		head_driver = driver;
	tail_driver = driver;
}

static void load_driver(const char *name)
{
	char *so_name;
	void *dlhandle;

#define __IBV_QUOTE(x)	#x
#define IBV_QUOTE(x)	__IBV_QUOTE(x)

	if (asprintf(&so_name,
		     name[0] == '/' ?
		     "%s-" IBV_QUOTE(IBV_DEVICE_LIBRARY_EXTENSION) ".so" :
		     "lib%s-" IBV_QUOTE(IBV_DEVICE_LIBRARY_EXTENSION) ".so",
		     name) < 0) {
		fprintf(stderr, PFX "Warning: couldn't load driver '%s'.\n",
			name);
		return;
	}

	dlhandle = dlopen(so_name, RTLD_NOW);
	if (!dlhandle) {
		fprintf(stderr, PFX "Warning: couldn't load driver '%s': %s\n",
			name, dlerror());
		goto out;
	}

out:
	free(so_name);
}

static void load_drivers(void)
{
	struct ibv_driver_name *name, *next_name;
	const char *env;
	char *list, *env_name;

	/*
	 * Only use drivers passed in through the calling user's
	 * environment if we're not running setuid.
	 */
	if (getuid() == geteuid()) {
		if ((env = getenv("RDMAV_DRIVERS"))) {
			list = strdupa(env);
			while ((env_name = strsep(&list, ":;")))
				load_driver(env_name);
		} else if ((env = getenv("IBV_DRIVERS"))) {
			list = strdupa(env);
			while ((env_name = strsep(&list, ":;")))
				load_driver(env_name);
		}
	}

	for (name = driver_name_list, next_name = name ? name->next : NULL;
	     name;
	     name = next_name, next_name = name ? name->next : NULL) {
		load_driver(name->name);
		free(name->name);
		free(name);
	}
}

static void read_config_file(const char *path)
{
	FILE *conf;
	char *line = NULL;
	char *config;
	char *field;
	size_t buflen = 0;
	ssize_t len;

	conf = fopen(path, "r");
	if (!conf) {
		fprintf(stderr, PFX "Warning: couldn't read config file %s.\n",
			path);
		return;
	}

	while ((len = getline(&line, &buflen, conf)) != -1) {
		config = line + strspn(line, "\t ");
		if (config[0] == '\n' || config[0] == '#')
			continue;

		field = strsep(&config, "\n\t ");

		if (strcmp(field, "driver") == 0) {
			struct ibv_driver_name *driver_name;

			config += strspn(config, "\t ");
			field = strsep(&config, "\n\t ");

			driver_name = malloc(sizeof *driver_name);
			if (!driver_name) {
				fprintf(stderr, PFX "Warning: couldn't allocate "
					"driver name '%s'.\n", field);
				continue;
			}

			driver_name->name = strdup(field);
			if (!driver_name->name) {
				fprintf(stderr, PFX "Warning: couldn't allocate "
					"driver name '%s'.\n", field);
				free(driver_name);
				continue;
			}

			driver_name->next = driver_name_list;
			driver_name_list  = driver_name;
		} else
			fprintf(stderr, PFX "Warning: ignoring bad config directive "
				"'%s' in file '%s'.\n", field, path);
	}

	if (line)
		free(line);
	fclose(conf);
}

static void read_config(void)
{
	DIR *conf_dir;
	struct dirent *dent;
	char *path;

	conf_dir = opendir(IBV_CONFIG_DIR);
	if (!conf_dir) {
		fprintf(stderr, PFX "Warning: couldn't open config directory '%s'.\n",
			IBV_CONFIG_DIR);
		return;
	}

	while ((dent = readdir(conf_dir))) {
		struct stat buf;

		if (asprintf(&path, "%s/%s", IBV_CONFIG_DIR, dent->d_name) < 0) {
			fprintf(stderr, PFX "Warning: couldn't read config file %s/%s.\n",
				IBV_CONFIG_DIR, dent->d_name);
			return;
		}

		if (stat(path, &buf)) {
			fprintf(stderr, PFX "Warning: couldn't stat config file '%s'.\n",
				path);
			goto next;
		}

		if (!S_ISREG(buf.st_mode))
			goto next;

		read_config_file(path);
next:
		free(path);
	}

	closedir(conf_dir);
}

static struct ibv_device *try_driver(struct ibv_driver *driver,
				     struct ibv_sysfs_dev *sysfs_dev)
{
	struct ibv_device *dev;
	char value[8];

	dev = driver->init_func(sysfs_dev->sysfs_path, sysfs_dev->abi_ver);
	if (!dev)
		return NULL;

	if (ibv_read_sysfs_file(sysfs_dev->ibdev_path, "node_type", value, sizeof value) < 0) {
		fprintf(stderr, PFX "Warning: no node_type attr under %s.\n",
			sysfs_dev->ibdev_path);
			dev->node_type = IBV_NODE_UNKNOWN;
	} else {
		dev->node_type = strtol(value, NULL, 10);
		if (dev->node_type < IBV_NODE_CA || dev->node_type > IBV_NODE_RNIC)
			dev->node_type = IBV_NODE_UNKNOWN;
	}
out:

	switch (dev->node_type) {
	case IBV_NODE_CA:
	case IBV_NODE_SWITCH:
	case IBV_NODE_ROUTER:
		dev->transport_type = IBV_TRANSPORT_IB;
		break;
	case IBV_NODE_RNIC:
		dev->transport_type = IBV_TRANSPORT_IWARP;
		break;
	default:
		dev->transport_type = IBV_TRANSPORT_UNKNOWN;
		break;
	}

	strcpy(dev->dev_name,   sysfs_dev->sysfs_name);
	strcpy(dev->dev_path,   sysfs_dev->sysfs_path);
	strcpy(dev->name,       sysfs_dev->ibdev_name);
	strcpy(dev->ibdev_path, sysfs_dev->ibdev_path);

	return dev;
}

static struct ibv_device *try_drivers(struct ibv_sysfs_dev *sysfs_dev)
{
	struct ibv_driver *driver;
	struct ibv_device *dev;

	for (driver = head_driver; driver; driver = driver->next) {
		dev = try_driver(driver, sysfs_dev);
		if (dev)
			return dev;
	}

	return NULL;
}

static int check_abi_version(const char *path)
{
	char value[8];

	if (ibv_read_sysfs_file(path, "class/infiniband_verbs/abi_version",
				value, sizeof value) < 0) {
		return ENOSYS;
	}

	abi_ver = strtol(value, NULL, 10);

	if (abi_ver < IB_USER_VERBS_MIN_ABI_VERSION ||
	    abi_ver > IB_USER_VERBS_MAX_ABI_VERSION) {
		fprintf(stderr, PFX "Fatal: kernel ABI version %d "
			"doesn't match library version %d.\n",
			abi_ver, IB_USER_VERBS_MAX_ABI_VERSION);
		return ENOSYS;
	}

	return 0;
}

static void check_memlock_limit(void)
{
	struct rlimit rlim;

	if (!geteuid())
		return;

	if (getrlimit(RLIMIT_MEMLOCK, &rlim)) {
		fprintf(stderr, PFX "Warning: getrlimit(RLIMIT_MEMLOCK) failed.");
		return;
	}

	if (rlim.rlim_cur <= 32768)
		fprintf(stderr, PFX "Warning: RLIMIT_MEMLOCK is %lu bytes.\n"
			"    This will severely limit memory registrations.\n",
			rlim.rlim_cur);
}

static void add_device(struct ibv_device *dev,
		       struct ibv_device ***dev_list,
		       int *num_devices,
		       int *list_size)
{
	struct ibv_device **new_list;

	if (*list_size <= *num_devices) {
		*list_size = *list_size ? *list_size * 2 : 1;
		new_list = realloc(*dev_list, *list_size * sizeof (struct ibv_device *));
		if (!new_list)
			return;
		*dev_list = new_list;
	}

	(*dev_list)[(*num_devices)++] = dev;
}

HIDDEN int ibverbs_init(struct ibv_device ***list)
{
	const char *sysfs_path;
	struct ibv_sysfs_dev *sysfs_dev, *next_dev;
	struct ibv_device *device;
	int num_devices = 0;
	int list_size = 0;
	int statically_linked = 0;
	int no_driver = 0;
	int ret;

	*list = NULL;

	if (getenv("RDMAV_FORK_SAFE") || getenv("IBV_FORK_SAFE"))
		if (ibv_fork_init())
			fprintf(stderr, PFX "Warning: fork()-safety requested "
				"but init failed\n");

	sysfs_path = ibv_get_sysfs_path();
	if (!sysfs_path)
		return -ENOSYS;

	ret = check_abi_version(sysfs_path);
	if (ret)
		return -ret;

	check_memlock_limit();

	read_config();

	ret = find_sysfs_devs();
	if (ret)
		return -ret;

	for (sysfs_dev = sysfs_dev_list; sysfs_dev; sysfs_dev = sysfs_dev->next) {
		device = try_drivers(sysfs_dev);
		if (device) {
			add_device(device, list, &num_devices, &list_size);
			sysfs_dev->have_driver = 1;
		} else
			no_driver = 1;
	}

	if (!no_driver)
		goto out;

	/*
	 * Check if we can dlopen() ourselves.  If this fails,
	 * libibverbs is probably statically linked into the
	 * executable, and we should just give up, since trying to
	 * dlopen() a driver module will fail spectacularly (loading a
	 * driver .so will bring in dynamic copies of libibverbs and
	 * libdl to go along with the static copies the executable
	 * has, which quickly leads to a crash.
	 */
	{
		void *hand = dlopen(NULL, RTLD_NOW);
		if (!hand) {
			fprintf(stderr, PFX "Warning: dlopen(NULL) failed, "
				"assuming static linking.\n");
			statically_linked = 1;
			goto out;
		}
		dlclose(hand);
	}

	load_drivers();

	for (sysfs_dev = sysfs_dev_list; sysfs_dev; sysfs_dev = sysfs_dev->next) {
		if (sysfs_dev->have_driver)
			continue;

		device = try_drivers(sysfs_dev);
		if (device) {
			add_device(device, list, &num_devices, &list_size);
			sysfs_dev->have_driver = 1;
		}
	}

out:
	for (sysfs_dev = sysfs_dev_list,
		     next_dev = sysfs_dev ? sysfs_dev->next : NULL;
	     sysfs_dev;
	     sysfs_dev = next_dev, next_dev = sysfs_dev ? sysfs_dev->next : NULL) {
		if (!sysfs_dev->have_driver) {
			fprintf(stderr, PFX "Warning: no userspace device-specific "
				"driver found for %s\n", sysfs_dev->sysfs_path);
			if (statically_linked)
				fprintf(stderr, "	When linking libibverbs statically, "
					"driver must be statically linked too.\n");
		}
		free(sysfs_dev);
	}

	return num_devices;
}
