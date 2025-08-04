/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2000, 2001 Michael Smith
 * Copyright (c) 2000 BSDi
 * All rights reserved.
 * Copyright (c) 2024 KT Ullavik
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Print information about system device configuration.
 */

#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libxo/xo.h>
#include "devinfo.h"

static bool	rflag;
static bool	vflag;
static int	open_tag_count;
static char	*last_res;

static void	print_indent(int);
static void	print_kvlist(char *);
static char*	xml_safe_string(char *);
static void	print_resource(struct devinfo_res *);
static int	print_device_matching_resource(struct devinfo_res *, void *);
static int	print_device_rman_resources(struct devinfo_rman *, void *);
static void	print_device_props(struct devinfo_dev *);
static int	print_device(struct devinfo_dev *, void *);
static int	print_rman_resource(struct devinfo_res *, void *);
static int	print_rman(struct devinfo_rman *, void *);
static int	print_device_path(struct devinfo_dev *, void *);
static void	print_path(struct devinfo_dev *, char *);
static void	usage(void);

struct indent_arg
{
	int	indent;
	void	*arg;
};


static void
print_indent(int n)
{
	static char	buffer[1024];

	if (n < 1)
		return;
	n = MIN((size_t)n, sizeof(buffer) - 1);
	memset(buffer, ' ', n);
	buffer[n] = '\0';
	xo_emit("{Pa:%s}", buffer);
}

/*
 * Takes a list of key-value pairs in the form
 * "key1=val1 key2=val2 ..." and prints them according
 * to xo formatting.
 */
static void
print_kvlist(char *s)
{
	char *kv;
	char *copy;

	if ((copy = strdup(s)) == NULL)
		xo_err(1, "No memory!");

	while ((kv = strsep(&copy, " ")) != NULL) {
		char* k = strsep(&kv, "=");
		xo_emit("{ea:%s/%s} {d:key/%s}={d:value/%s}", k, kv, k, kv);
	}
	free(copy);
}

static char
*xml_safe_string(char *desc)
{
	int i;
	char *s;

	if ((s = strdup(desc)) == NULL) {
		xo_err(1, "No memory!");
	}

	for (i=0; s[i] != '\0'; i++) {
		if (s[i] == ' ' || s[i] == '/') {
			s[i] = '-';
		}
	}
	return s;
}

/*
 * Print a resource.
 */
void
print_resource(struct devinfo_res *res)
{
	struct devinfo_rman	*rman;
	bool			hexmode;
	rman_res_t		end;
	char			*safe_desc;

	rman = devinfo_handle_to_rman(res->dr_rman);
	hexmode =  (rman->dm_size > 1000) || (rman->dm_size == 0);
	end = res->dr_start + res->dr_size - 1;

	safe_desc = xml_safe_string(rman->dm_desc);
	xo_open_instance(safe_desc);

	if (hexmode) {
		xo_emit("{:start/0x%jx}", res->dr_start);
		if (res->dr_size > 1)
			xo_emit("{D:-}{d:end/0x%jx}", end);
		xo_emit("{e:end/0x%jx}", end);
	} else {
		xo_emit("{:start/%ju}", res->dr_start);
		if (res->dr_size > 1)
			xo_emit("{D:-}{d:end/%ju}", end);
		xo_emit("{e:end/%ju}", end);
	}
	xo_close_instance(safe_desc);
	free(safe_desc);
}

/*
 * Print resource information if this resource matches the
 * given device.
 *
 * If the given indent is 0, return an indicator that a matching
 * resource exists.
 */
int
print_device_matching_resource(struct devinfo_res *res, void *arg)
{
	struct indent_arg	*ia = (struct indent_arg *)arg;
	struct devinfo_dev	*dev = (struct devinfo_dev *)ia->arg;

	if (devinfo_handle_to_device(res->dr_device) == dev) {
		/* in 'detect' mode, found a match */
		if (ia->indent == 0)
			return(1);
		print_indent(ia->indent);
		print_resource(res);
		xo_emit("\n");
	}
	return(0);
}

/*
 * Print resource information for this device and resource manager.
 */
int
print_device_rman_resources(struct devinfo_rman *rman, void *arg)
{
	struct indent_arg	*ia = (struct indent_arg *)arg;
	int			indent;
	char			*safe_desc;

	indent = ia->indent;

	/* check whether there are any resources matching this device */
	ia->indent = 0;
	if (devinfo_foreach_rman_resource(rman,
	    print_device_matching_resource, ia) != 0) {

		/* there are, print header */
		safe_desc = xml_safe_string(rman->dm_desc);
		print_indent(indent);
		xo_emit("<{:description/%s}>\n", rman->dm_desc);
		xo_open_list(safe_desc);

		/* print resources */
		ia->indent = indent + 4;
		devinfo_foreach_rman_resource(rman,
		    print_device_matching_resource, ia);

		xo_close_list(safe_desc);
		free(safe_desc);
	}
	ia->indent = indent;
	return(0);
}

static void
print_device_props(struct devinfo_dev *dev)
{
	if (vflag) {
		if (*dev->dd_desc) {
			xo_emit("<{:description/%s}>", dev->dd_desc);
		}
		if (*dev->dd_pnpinfo) {
			xo_open_container("pnpinfo");
			xo_emit("{D: pnpinfo}");

			if ((strcmp(dev->dd_pnpinfo, "unknown") == 0))
				xo_emit("{D: unknown}");
			else
				print_kvlist(dev->dd_pnpinfo);

			xo_close_container("pnpinfo");
		}
		if (*dev->dd_location) {
			xo_open_container("location");
			xo_emit("{D: at}");
			print_kvlist(dev->dd_location);
			xo_close_container("location");
		}

		// If verbose, then always print state for json/xml.
		if (!(dev->dd_flags & DF_ENABLED))
			xo_emit("{e:state/disabled}");
		else if (dev->dd_flags & DF_SUSPENDED)
			xo_emit("{e:state/suspended}");
		else
			xo_emit("{e:state/enabled}");
	}

	if (!(dev->dd_flags & DF_ENABLED))
		xo_emit("{D: (disabled)}");
	else if (dev->dd_flags & DF_SUSPENDED)
		xo_emit("{D: (suspended)}");
}

/*
 * Print information about a device.
 */
static int
print_device(struct devinfo_dev *dev, void *arg)
{
	struct indent_arg	ia;
	int			indent, ret;
	const char*		devname = dev->dd_name[0] ? dev->dd_name : "unknown";
	bool			printit = vflag || (dev->dd_name[0] != 0 &&
				    dev->dd_state >= DS_ATTACHED);

	if (printit) {
		indent = (int)(intptr_t)arg;
		print_indent(indent);

		xo_open_container(devname);
		xo_emit("{d:devicename/%s}", devname);

		print_device_props(dev);
		xo_emit("\n");
		if (rflag) {
			ia.indent = indent + 4;
			ia.arg = dev;
			devinfo_foreach_rman(print_device_rman_resources,
			    (void *)&ia);
		}
	}

	ret = (devinfo_foreach_device_child(dev, print_device,
	    (void *)((char *)arg + 2)));

	if (printit) {
		xo_close_container(devname);
	}
	return(ret);
}

/*
 * Print information about a resource under a resource manager.
 */
int
print_rman_resource(struct devinfo_res *res, void *arg __unused)
{
	struct devinfo_dev	*dev;
	struct devinfo_rman	*rman;
	rman_res_t		end;
	char			*res_str, *entry = NULL;
	bool			hexmode;

	dev = devinfo_handle_to_device(res->dr_device);
	rman = devinfo_handle_to_rman(res->dr_rman);
	hexmode =  (rman->dm_size > 1000) || (rman->dm_size == 0);
	end = res->dr_start + res->dr_size - 1;

	if (hexmode) {
		if (res->dr_size > 1)
			asprintf(&res_str, "0x%jx-0x%jx", res->dr_start, end);
		else
			asprintf(&res_str, "0x%jx", res->dr_start);
	} else {
		if (res->dr_size > 1)
			asprintf(&res_str, "%ju-%ju", res->dr_start, end);
		else
			asprintf(&res_str, "%ju", res->dr_start);
	}

	xo_emit("{P:    }");

	if (last_res == NULL) {
		// First resource
		xo_open_list(res_str);
	} else if (strcmp(res_str, last_res) != 0) {
		// We can't repeat json keys. So we keep an
		// open list from the last iteration and only
		// create a new list when see a new resource.
		xo_close_list(last_res);
		xo_open_list(res_str);
	}

	dev = devinfo_handle_to_device(res->dr_device);
	if (dev != NULL) {
		if (dev->dd_name[0] != 0) {
			printf(" (%s)", dev->dd_name);
			asprintf(&entry, "{el:%s}{D:%s} {D:(%s)}\n",
			    res_str, res_str, dev->dd_name);
			xo_emit(entry, dev->dd_name);
		} else {
			printf(" (unknown)");
			if (vflag && *dev->dd_pnpinfo)
				printf(" pnpinfo %s", dev->dd_pnpinfo);
			if (vflag && *dev->dd_location)
				printf(" at %s", dev->dd_location);
		}
	} else {
		asprintf(&entry, "{el:%s}{D:%s} {D:----}\n", res_str, res_str);
		xo_emit(entry, "----");
	}
	free(entry);
	last_res = res_str;
	return(0);
}

/*
 * Print information about a resource manager.
 */
int
print_rman(struct devinfo_rman *rman, void *arg __unused)
{
	char* safe_desc = xml_safe_string(rman->dm_desc);

	xo_emit("<{:description/%s}\n>", rman->dm_desc);
	xo_open_container(safe_desc);

	devinfo_foreach_rman_resource(rman, print_rman_resource, 0);

	xo_close_list(last_res);
	xo_close_container(safe_desc);
	free(safe_desc);
	return(0);
}

static void
print_device_path_entry(struct devinfo_dev *dev)
{
	const char *devname = dev->dd_name[0] ? dev->dd_name : "unknown";

	xo_open_container(devname);
	open_tag_count++;
	xo_emit("{:devicename/%s} ", devname);
	print_device_props(dev);
	if (vflag)
		xo_emit("\n");
}

/*
 * Recurse until we find the right dev. On the way up we print path.
 */
static int
print_device_path(struct devinfo_dev *dev, void *xname)
{
	const char *name = xname;
	int rv;

	if (strcmp(dev->dd_name, name) == 0) {
		print_device_path_entry(dev);
		return (1);
	}

	rv = devinfo_foreach_device_child(dev, print_device_path, xname);
	if (rv == 1) {
		xo_emit("{P: }");
		print_device_path_entry(dev);
	}
	return (rv);
}

static void
print_path(struct devinfo_dev *root, char *path)
{
	open_tag_count = 0;
	if (devinfo_foreach_device_child(root, print_device_path,
	    (void *)path) == 0)
		xo_errx(1, "%s: Not found", path);
	if (!vflag)
		xo_emit("\n");

	while (open_tag_count > 0) {
		xo_close_container_d();
		open_tag_count--;
	}
}

static void __dead2
usage(void)
{
	xo_error(
	    "usage: devinfo [-rv]\n",
	    "       devinfo -u [-v]\n",
	    "       devinfo -p dev [-v]\n");
	exit(1);
}

int
main(int argc, char *argv[]) 
{
	struct devinfo_dev	*root;
	int			c, rv;
	bool			uflag;
	char			*path = NULL;

	argc = xo_parse_args(argc, argv);
	if (argc < 0) {
		exit(1);
	}

	uflag = false;
	while ((c = getopt(argc, argv, "p:ruv")) != -1) {
		switch(c) {
		case 'p':
			path = optarg;
			break;
		case 'r':
			rflag = true;
			break;
		case 'u':
			uflag = true;
			break;
		case 'v':
			vflag = true;
			break;
		default:
			usage();
		}
	}

	if (path && (rflag || uflag))
		usage();

	if ((rv = devinfo_init()) != 0) {
		errno = rv;
		xo_err(1, "devinfo_init");
	}

	if ((root = devinfo_handle_to_device(DEVINFO_ROOT_DEVICE)) == NULL)
		xo_errx(1, "can't find root device");

	if (path) {
		xo_set_flags(NULL, XOF_DTRT);
		xo_open_container("device-path");
		print_path(root, path);
		xo_close_container("device-path");
	} else if (uflag) {
		/* print resource usage? */
		xo_set_flags(NULL, XOF_DTRT);
		xo_open_container("device-resources");
		devinfo_foreach_rman(print_rman, NULL);
		xo_close_container("device-resources");
	} else {
		/* print device hierarchy */
		xo_open_container("device-information");
		devinfo_foreach_device_child(root, print_device, (void *)0);
		xo_close_container("device-information");
	}

	if (xo_finish() < 0) {
		exit(1);
	}
	return(0);
}
