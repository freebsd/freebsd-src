/*-
 * Copyright (c) 2006 IronPort Systems
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/blist.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/bus.h>
#include <sys/pciio.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <net/if.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/swap_pager.h>

#include <machine/bus.h>

#include <compat/linux/linux_ioctl.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_util.h>
#include <fs/pseudofs/pseudofs.h>

struct scsi_host_queue {
	TAILQ_ENTRY(scsi_host_queue) scsi_host_next;
	char *path;
	char *name;
};

TAILQ_HEAD(,scsi_host_queue) scsi_host_q;

static int host_number = 0;

static int
atoi(const char *str)
{
	return (int)strtol(str, (char **)NULL, 10);
}

/*
 * Filler function for proc_name
 */
static int
linsysfs_scsiname(PFS_FILL_ARGS)
{
	struct scsi_host_queue *scsi_host;
	int index;

	if (strncmp(pn->pn_parent->pn_name, "host", 4) == 0) {
		index = atoi(&pn->pn_parent->pn_name[4]);
	} else {
		sbuf_printf(sb, "unknown\n");
		return (0);
	}
	TAILQ_FOREACH(scsi_host, &scsi_host_q, scsi_host_next) {
		if (index-- == 0) {
			sbuf_printf(sb, "%s\n", scsi_host->name);
			return (0);
		}
	}
	sbuf_printf(sb, "unknown\n");
	return (0);
}

/*
 * Filler function for device sym-link
 */
static int
linsysfs_link_scsi_host(PFS_FILL_ARGS)
{
	struct scsi_host_queue *scsi_host;
	int index;

	if (strncmp(pn->pn_parent->pn_name, "host", 4) == 0) {
		index = atoi(&pn->pn_parent->pn_name[4]);
	} else {
		sbuf_printf(sb, "unknown\n");
		return (0);
	}
	TAILQ_FOREACH(scsi_host, &scsi_host_q, scsi_host_next) {
		if (index-- == 0) {
			sbuf_printf(sb, "../../../devices%s", scsi_host->path);
			return(0);
		}
	}
	sbuf_printf(sb, "unknown\n");
	return (0);
}

#define PCI_DEV "pci"
static int
linsysfs_run_bus(device_t dev, struct pfs_node *dir, struct pfs_node *scsi, char *path,
   char *prefix)
{
	struct scsi_host_queue *scsi_host;
	struct pfs_node *sub_dir;
	int i, nchildren;
	device_t *children, parent;
	devclass_t devclass;
	const char *name = NULL;
	struct pci_devinfo *dinfo;
	char *device, *host, *new_path = path;

	parent = device_get_parent(dev);
	if (parent) {
		devclass = device_get_devclass(parent);
		if (devclass != NULL)
			name = devclass_get_name(devclass);
		if (name && strcmp(name, PCI_DEV) == 0) {
			dinfo = device_get_ivars(dev);
			if (dinfo) {
				device = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
				new_path = malloc(MAXPATHLEN, M_TEMP,
				    M_WAITOK);
				new_path[0] = '\000';
				strcpy(new_path, path);
				host = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
				device[0] = '\000';
				sprintf(device, "%s:%02x:%02x.%x",
				    prefix,
				    dinfo->cfg.bus,
				    dinfo->cfg.slot,
				    dinfo->cfg.func);
				strcat(new_path, "/");
				strcat(new_path, device);
				dir = pfs_create_dir(dir, device,
				    NULL, NULL, NULL, 0);

				if (dinfo->cfg.baseclass == PCIC_STORAGE) {
					/* DJA only make this if needed */
					sprintf(host, "host%d", host_number++);
					strcat(new_path, "/");
					strcat(new_path, host);
					pfs_create_dir(dir, host,
					    NULL, NULL, NULL, 0);
					scsi_host = malloc(sizeof(
					    struct scsi_host_queue),
					    M_DEVBUF, M_NOWAIT);
					scsi_host->path = malloc(
					    strlen(new_path) + 1,
					    M_DEVBUF, M_NOWAIT);
					scsi_host->path[0] = '\000';
					bcopy(new_path, scsi_host->path,
					    strlen(new_path) + 1);
					scsi_host->name = "unknown";

					sub_dir = pfs_create_dir(scsi, host,
					    NULL, NULL, NULL, 0);
					pfs_create_link(sub_dir, "device",
					    &linsysfs_link_scsi_host,
					    NULL, NULL, NULL, 0);
					pfs_create_file(sub_dir, "proc_name",
					    &linsysfs_scsiname,
					    NULL, NULL, NULL, PFS_RD);
					scsi_host->name
					    = linux_driver_get_name_dev(dev);
					TAILQ_INSERT_TAIL(&scsi_host_q,
					    scsi_host, scsi_host_next);
				}
				free(device, M_TEMP);
				free(host, M_TEMP);
			}
		}
	}

	device_get_children(dev, &children, &nchildren);
	for (i = 0; i < nchildren; i++) {
		if (children[i])
			linsysfs_run_bus(children[i], dir, scsi, new_path, prefix);
	}
	if (new_path != path)
		free(new_path, M_TEMP);

	return (1);
}

/*
 * Constructor
 */
static int
linsysfs_init(PFS_INIT_ARGS)
{
	struct pfs_node *root;
	struct pfs_node *dir;
	struct pfs_node *pci;
	struct pfs_node *scsi;
	devclass_t devclass;
	device_t dev;

	TAILQ_INIT(&scsi_host_q);

	root = pi->pi_root;

	/* /sys/class/... */
	scsi = pfs_create_dir(root, "class", NULL, NULL, NULL, 0);
	scsi = pfs_create_dir(scsi, "scsi_host", NULL, NULL, NULL, 0);

	/* /sys/device */
	dir = pfs_create_dir(root, "devices", NULL, NULL, NULL, 0);

	/* /sys/device/pci0000:00 */
	pci = pfs_create_dir(dir, "pci0000:00", NULL, NULL, NULL, 0);

	devclass = devclass_find("root");
	if (devclass == NULL) {
		return (0);
	}

	dev = devclass_get_device(devclass, 0);
	linsysfs_run_bus(dev, pci, scsi, "/pci0000:00", "0000");
	return (0);
}

/*
 * Destructor
 */
static int
linsysfs_uninit(PFS_INIT_ARGS)
{
	struct scsi_host_queue *scsi_host, *scsi_host_tmp;

	TAILQ_FOREACH_SAFE(scsi_host, &scsi_host_q, scsi_host_next,
	    scsi_host_tmp) {
		TAILQ_REMOVE(&scsi_host_q, scsi_host, scsi_host_next);
		free(scsi_host->path, M_TEMP);
		free(scsi_host, M_TEMP);
	}

	return (0);
}

PSEUDOFS(linsysfs, 1, PR_ALLOW_MOUNT_LINSYSFS);
#if defined(__amd64__)
MODULE_DEPEND(linsysfs, linux_common, 1, 1, 1);
#else
MODULE_DEPEND(linsysfs, linux, 1, 1, 1);
#endif
