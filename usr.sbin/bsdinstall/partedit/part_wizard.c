/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 Nathan Whitehorn
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

#include <sys/param.h>

#include <errno.h>
#include <inttypes.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgeom.h>
#include <bsddialog.h>

#include "partedit.h"

#define MIN_FREE_SPACE		(1024*1024*1024) /* 1 GB */
#define SWAP_SIZE(available)	MIN(available/20, 4*1024*1024*1024LL)

static char *wizard_partition(struct gmesh *mesh, const char *disk);

int
part_wizard(const char *fsreq)
{
	char *disk, *schemeroot;
	const char *fstype;
	struct gmesh mesh;
	int error;
	struct bsddialog_conf conf;

	bsddialog_initconf(&conf);

	if (fsreq != NULL)
		fstype = fsreq;
	else
		fstype = "ufs";

startwizard:
	error = geom_gettree(&mesh);
	if (error != 0)
		return (1);

	bsddialog_backtitle(&conf, OSNAME " Installer");
	disk = boot_disk_select(&mesh);
	if (disk == NULL) {
		geom_deletetree(&mesh);
		return (1);
	}

	bsddialog_clear(0);
	bsddialog_backtitle(&conf, OSNAME " Installer");
	schemeroot = wizard_partition(&mesh, disk);
	free(disk);
	geom_deletetree(&mesh);
	if (schemeroot == NULL)
		return (1);

	bsddialog_clear(0);
	bsddialog_backtitle(&conf, OSNAME " Installer");
	error = geom_gettree(&mesh);
	if (error != 0) {
		free(schemeroot);
		return (1);
	}

	error = wizard_makeparts(&mesh, schemeroot, fstype, 1);
	free(schemeroot);
	geom_deletetree(&mesh);
	if (error)
		goto startwizard;

	return (0);
}

char *
boot_disk_select(struct gmesh *mesh)
{
	struct gclass *classp;
	struct gconfig *gc;
	struct ggeom *gp;
	struct gprovider *pp;
	struct bsddialog_menuitem *disks = NULL;
	const char *type, *desc;
	char diskdesc[512];
	char *chosen;
	int i, button, fd, selected, n = 0;
	struct bsddialog_conf conf;

	bsddialog_initconf(&conf);

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, "DISK") != 0 &&
		    strcmp(classp->lg_name, "RAID") != 0 &&
		    strcmp(classp->lg_name, "MD") != 0)
			continue;

		LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
			if (LIST_EMPTY(&gp->lg_provider))
				continue;

			LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
				desc = type = NULL;
				LIST_FOREACH(gc, &pp->lg_config, lg_config) {
					if (strcmp(gc->lg_name, "type") == 0)
						type = gc->lg_val;
					if (strcmp(gc->lg_name, "descr") == 0)
						desc = gc->lg_val;
				}

				/* Skip swap-backed md and WORM devices */
				if (strcmp(classp->lg_name, "MD") == 0 &&
				    type != NULL && strcmp(type, "swap") == 0)
					continue;
				if (strncmp(pp->lg_name, "cd", 2) == 0)
					continue;
				/*
				 * Check if the disk is available to be opened for
				 * write operations, it helps prevent the USB
				 * stick used to boot from being listed as an option
				 */
				fd = g_open(pp->lg_name, 1);
				if (fd == -1) {
					continue;
				}
				g_close(fd);

				disks = realloc(disks, (++n)*sizeof(disks[0]));
				disks[n-1].name = pp->lg_name;
				humanize_number(diskdesc, 7, pp->lg_mediasize,
				    "B", HN_AUTOSCALE, HN_DECIMAL);
				if (strncmp(pp->lg_name, "ad", 2) == 0)
					strcat(diskdesc, " ATA Hard Disk");
				else if (strncmp(pp->lg_name, "md", 2) == 0)
					strcat(diskdesc, " Memory Disk");
				else
					strcat(diskdesc, " Disk");

				if (desc != NULL)
					snprintf(diskdesc, sizeof(diskdesc),
					    "%s <%s>", diskdesc, desc);

				disks[n-1].prefix = "";
				disks[n-1].on = false;
				disks[n-1].depth = 0;
				disks[n-1].desc = strdup(diskdesc);
				disks[n-1].bottomdesc = "";
			}
		}
	}

	if (n > 1) {
		conf.title = "Partitioning";
		button = bsddialog_menu(&conf,
		    "Select the disk on which to install " OSNAME ".", 0, 0, 0,
		    n, disks, &selected);

		chosen = (button == BSDDIALOG_OK) ?
		    strdup(disks[selected].name) : NULL;
	} else if (n == 1) {
		chosen = strdup(disks[0].name);
	} else {
		chosen = NULL;
	}

	for (i = 0; i < n; i++)
		free((char*)disks[i].desc);

	return (chosen);
}

static struct gprovider *
provider_for_name(struct gmesh *mesh, const char *name)
{
	struct gclass *classp;
	struct gprovider *pp = NULL;
	struct ggeom *gp;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
			if (LIST_EMPTY(&gp->lg_provider))
				continue;

			LIST_FOREACH(pp, &gp->lg_provider, lg_provider)
				if (strcmp(pp->lg_name, name) == 0)
					break;

			if (pp != NULL) break;
		}

		if (pp != NULL) break;
	}

	return (pp);
}

static char *
wizard_partition(struct gmesh *mesh, const char *disk)
{
	struct gclass *classp;
	struct ggeom *gpart = NULL;
	struct gconfig *gc;
	char *retval = NULL;
	const char *scheme = NULL;
	char message[512];
	int choice;
	struct bsddialog_conf conf;

	bsddialog_initconf(&conf);

	LIST_FOREACH(classp, &mesh->lg_class, lg_class)
		if (strcmp(classp->lg_name, "PART") == 0)
			break;

	if (classp != NULL) {
		LIST_FOREACH(gpart, &classp->lg_geom, lg_geom)
			if (strcmp(gpart->lg_name, disk) == 0)
				break;
	}

	if (gpart != NULL) {
		LIST_FOREACH(gc, &gpart->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "scheme") == 0) {
				scheme = gc->lg_val;
				break;
			}
		}
	}

	/* Treat uncommitted scheme deletions as no scheme */
	if (scheme != NULL && strcmp(scheme, "(none)") == 0)
		scheme = NULL;

query:
	conf.button.ok_label = "Entire Disk";
	conf.button.cancel_label = "Partition";
	if (gpart != NULL)
		conf.button.default_cancel = true;

	snprintf(message, sizeof(message), "Would you like to use this entire "
	    "disk (%s) for " OSNAME " or partition it to share it with other "
	    "operating systems? Using the entire disk will erase any data "
	    "currently stored there.", disk);
	conf.title = "Partition";
	choice = bsddialog_yesno(&conf, message, 9, 45);

	conf.button.ok_label = NULL;
	conf.button.cancel_label = NULL;
	conf.button.default_cancel = false;

	if (choice == BSDDIALOG_NO && scheme != NULL && !is_scheme_bootable(scheme)) {
		char warning[512];
		int subchoice;

		snprintf(warning, sizeof(warning),
		    "The existing partition scheme on this "
		    "disk (%s) is not bootable on this platform. To install "
		    OSNAME ", it must be repartitioned. This will destroy all "
		    "data on the disk. Are you sure you want to proceed?",
		    scheme);
		conf.title = "Non-bootable Disk";
		subchoice = bsddialog_yesno(&conf, warning, 0, 0);
		if (subchoice != BSDDIALOG_YES)
			goto query;

		gpart_destroy(gpart);
		scheme = choose_part_type(default_scheme());
		if (scheme == NULL)
			return NULL;
		gpart_partition(disk, scheme);
	}

	if (scheme == NULL || choice == 0) {
		if (gpart != NULL && scheme != NULL) {
			/* Erase partitioned disk */
			conf.title = "Confirmation";
			choice = bsddialog_yesno(&conf, "This will erase "
			   "the disk. Are you sure you want to proceed?", 0, 0);
			if (choice != BSDDIALOG_YES)
				goto query;

			gpart_destroy(gpart);
		}

		scheme = choose_part_type(default_scheme());
		if (scheme == NULL)
			return NULL;
		gpart_partition(disk, scheme);
	}

	if (strcmp(scheme, "MBR") == 0) {
		struct gmesh submesh;

		if (geom_gettree(&submesh) == 0) {
			gpart_create(provider_for_name(&submesh, disk),
			    "freebsd", NULL, NULL, &retval,
			    choice /* Non-interactive for "Entire Disk" */);
			geom_deletetree(&submesh);
		}
	} else {
		retval = strdup(disk);
	}

	return (retval);
}

int
wizard_makeparts(struct gmesh *mesh, const char *disk, const char *fstype,
    int interactive)
{
	struct gclass *classp;
	struct ggeom *gp;
	struct gprovider *pp;
	char *fsnames[] = {"freebsd-ufs", "freebsd-zfs"};
	char *fsname;
	struct gmesh submesh;
	char swapsizestr[10], rootsizestr[10];
	intmax_t swapsize, available;
	int error, retval;
	struct bsddialog_conf conf;

	if (strcmp(fstype, "zfs") == 0) {
		fsname = fsnames[1];
	} else {
		/* default to UFS */
		fsname = fsnames[0];
	}

	LIST_FOREACH(classp, &mesh->lg_class, lg_class)
		if (strcmp(classp->lg_name, "PART") == 0)
			break;

	LIST_FOREACH(gp, &classp->lg_geom, lg_geom)
		if (strcmp(gp->lg_name, disk) == 0)
			break;

	pp = provider_for_name(mesh, disk);

	available = gpart_max_free(gp, NULL)*pp->lg_sectorsize;
	if (interactive && available < MIN_FREE_SPACE) {
		char availablestr[10], neededstr[10], message[512];
		humanize_number(availablestr, 7, available, "B", HN_AUTOSCALE,
		    HN_DECIMAL);
		humanize_number(neededstr, 7, MIN_FREE_SPACE, "B", HN_AUTOSCALE,
		    HN_DECIMAL);
		snprintf(message, sizeof(message),
		    "There is not enough free space on %s to "
		    "install " OSNAME " (%s free, %s required). Would you like "
		    "to choose another disk or to open the partition editor?",
		    disk, availablestr, neededstr);

		bsddialog_initconf(&conf);
		conf.button.ok_label = "Another Disk";
		conf.button.cancel_label = "Editor";
		conf.title = "Warning";
		retval = bsddialog_yesno(&conf, message, 0, 0);

		return (!retval); /* Editor -> return 0 */
	}

	swapsize = SWAP_SIZE(available);
	humanize_number(swapsizestr, 7, swapsize, "B", HN_AUTOSCALE,
	    HN_NOSPACE | HN_DECIMAL);
	humanize_number(rootsizestr, 7, available - swapsize - 1024*1024,
	    "B", HN_AUTOSCALE, HN_NOSPACE | HN_DECIMAL);

	error = geom_gettree(&submesh);
	if (error != 0)
		return (error);
	pp = provider_for_name(&submesh, disk);
	gpart_create(pp, fsname, rootsizestr, "/", NULL, 0);
	geom_deletetree(&submesh);

	error = geom_gettree(&submesh);
	if (error != 0)
		return (error);
	pp = provider_for_name(&submesh, disk);
	gpart_create(pp, "freebsd-swap", swapsizestr, NULL, NULL, 0);
	geom_deletetree(&submesh);

	return (0);
}
