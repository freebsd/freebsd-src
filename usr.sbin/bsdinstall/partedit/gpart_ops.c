/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <bsddialog.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libgeom.h>

#include "partedit.h"

#define GPART_FLAGS "x" /* Do not commit changes by default */

static void
gpart_show_error(const char *title, const char *explanation, const char *errstr)
{
	char *errmsg;
	char message[512];
	int error;
	struct bsddialog_conf conf;

	if (explanation == NULL)
		explanation = "";

	error = strtol(errstr, &errmsg, 0);
	if (errmsg != errstr) {
		while (errmsg[0] == ' ')
			errmsg++;
		if (errmsg[0] != '\0')
			sprintf(message, "%s%s. %s", explanation,
			    strerror(error), errmsg);
		else
			sprintf(message, "%s%s", explanation, strerror(error));
	} else {
		sprintf(message, "%s%s", explanation, errmsg);
	}

	bsddialog_initconf(&conf);
	conf.title = title;
	bsddialog_msgbox(&conf, message, 0, 0);
}

static int
scheme_supports_labels(const char *scheme)
{
	if (strcmp(scheme, "APM") == 0)
		return (1);
	if (strcmp(scheme, "GPT") == 0)
		return (1);

	return (0);
}

static void
newfs_command(const char *fstype, char *command, int use_default)
{
	struct bsddialog_conf conf;

	bsddialog_initconf(&conf);

	if (strcmp(fstype, "freebsd-ufs") == 0) {
		int i;
		struct bsddialog_menuitem items[] = {
			{"", false, 0, "UFS1", "UFS Version 1",
			    "Use version 1 of the UFS file system instead "
			    "of version 2 (not recommended)"},
			{"", true, 0, "SU", "Softupdates",
			    "Enable softupdates (default)"},
			{"", true, 0, "SUJ", "Softupdates journaling",
			    "Enable file system journaling (default - "
			    "turn off for SSDs)"},
			{"", false, 0, "TRIM", "Enable SSD TRIM support",
			    "Enable TRIM support, useful on solid-state "
			    "drives" },
		};

		if (!use_default) {
			int choice;
			conf.title = "UFS Options";
			choice = bsddialog_checklist(&conf, "", 0, 0, 0,
			    nitems(items), items, NULL);
			if (choice == BSDDIALOG_CANCEL)
				return;
		}

		strcpy(command, "newfs ");
		for (i = 0; i < (int)nitems(items); i++) {
			if (items[i].on == false)
				continue;
			if (strcmp(items[i].name, "UFS1") == 0)
				strcat(command, "-O1 ");
			else if (strcmp(items[i].name, "SU") == 0)
				strcat(command, "-U ");
			else if (strcmp(items[i].name, "SUJ") == 0)
				strcat(command, "-j ");
			else if (strcmp(items[i].name, "TRIM") == 0)
				strcat(command, "-t ");
		}
	} else if (strcmp(fstype, "freebsd-zfs") == 0) {
		int i;
		struct bsddialog_menuitem items[] = {
			{"", 0, true, "fletcher4", "checksum algorithm: fletcher4",
			    "Use fletcher4 for data integrity checking. "
			    "(default)"},
			{"", 0, false, "fletcher2", "checksum algorithm: fletcher2",
			    "Use fletcher2 for data integrity checking. "
			    "(not recommended)"},
			{"", 0, false, "sha256", "checksum algorithm: sha256",
			    "Use sha256 for data integrity checking. "
			    "(not recommended)"},
			{"", 0, false, "atime", "Update atimes for files",
			    "Disable atime update"},
		};

		if (!use_default) {
			int choice;
			conf.title = "ZFS Options";
			choice = bsddialog_checklist(&conf, "", 0, 0, 0,
			    nitems(items), items, NULL);
			if (choice == BSDDIALOG_CANCEL)
				return;
		}

		strcpy(command, "zpool create -f -m none ");
		if (getenv("BSDINSTALL_TMPBOOT") != NULL) {
			char zfsboot_path[MAXPATHLEN];
			snprintf(zfsboot_path, sizeof(zfsboot_path), "%s/zfs",
			    getenv("BSDINSTALL_TMPBOOT"));
			mkdir(zfsboot_path, S_IRWXU | S_IRGRP | S_IXGRP |
			    S_IROTH | S_IXOTH);
			sprintf(command, "%s -o cachefile=%s/zpool.cache ",
			    command, zfsboot_path);
		}
		for (i = 0; i < (int)nitems(items); i++) {
			if (items[i].on == false)
				continue;
			if (strcmp(items[i].name, "fletcher4") == 0)
				strcat(command, "-O checksum=fletcher4 ");
			else if (strcmp(items[i].name, "fletcher2") == 0)
				strcat(command, "-O checksum=fletcher2 ");
			else if (strcmp(items[i].name, "sha256") == 0)
				strcat(command, "-O checksum=sha256 ");
			else if (strcmp(items[i].name, "atime") == 0)
				strcat(command, "-O atime=off ");
		}
	} else if (strcmp(fstype, "fat32") == 0 || strcmp(fstype, "efi") == 0 ||
	     strcmp(fstype, "ms-basic-data") == 0) {
		int i;
		struct bsddialog_menuitem items[] = {
			{"", 0, true, "FAT32", "FAT Type 32",
			    "Create a FAT32 filesystem (default)"},
			{"", 0, false, "FAT16", "FAT Type 16",
			    "Create a FAT16 filesystem"},
			{"", 0, false, "FAT12", "FAT Type 12",
			    "Create a FAT12 filesystem"},
		};

		if (!use_default) {
			int choice;
			conf.title = "FAT Options";
			choice = bsddialog_radiolist(&conf, "", 0, 0, 0,
			    nitems(items), items, NULL);
			if (choice == BSDDIALOG_CANCEL)
				return;
		}

		strcpy(command, "newfs_msdos ");
		for (i = 0; i < (int)nitems(items); i++) {
			if (items[i].on == false)
				continue;
			if (strcmp(items[i].name, "FAT32") == 0)
				strcat(command, "-F 32 -c 1");
			else if (strcmp(items[i].name, "FAT16") == 0)
				strcat(command, "-F 16 ");
			else if (strcmp(items[i].name, "FAT12") == 0)
				strcat(command, "-F 12 ");
		}
	} else {
		if (!use_default) {
			conf.title = "Error";
			bsddialog_msgbox(&conf, "No configurable options exist "
			    "for this filesystem.", 0, 0);
		}
		command[0] = '\0';
	}
}

const char *
choose_part_type(const char *def_scheme)
{
	int button, choice, i;
	const char *scheme = NULL;
	struct bsddialog_conf conf;

	struct bsddialog_menuitem items[] = {
		{"", false, 0, "APM", "Apple Partition Map",
		    "Bootable on PowerPC Apple Hardware" },
		{"", false, 0, "BSD", "BSD Labels",
		    "Bootable on most x86 systems" },
		{"", false, 0, "GPT", "GUID Partition Table",
		    "Bootable on most x86 systems and EFI aware ARM64" },
		{"", false, 0, "MBR", "DOS Partitions",
		    "Bootable on most x86 systems" },
	};

	for (i = 0; i < (int)nitems(items); i++)
		if (strcmp(items[i].name, def_scheme) == 0)
			choice = i;

	bsddialog_initconf(&conf);

parttypemenu:
	conf.title = "Partition Scheme";
	button = bsddialog_menu(&conf,
	    "Select a partition scheme for this volume:", 0, 0, 0,
	    nitems(items), items, &choice);

	if (button == BSDDIALOG_CANCEL)
		return NULL;

	if (!is_scheme_bootable(items[choice].name)) {
		char message[512];
		sprintf(message, "This partition scheme (%s) is not "
		    "bootable on this platform. Are you sure you want "
		    "to proceed?", items[choice].name);
		conf.button.default_cancel = true;
		conf.title = "Warning";
		button = bsddialog_yesno(&conf, message, 0, 0);
		conf.button.default_cancel = false;
		if (button == BSDDIALOG_NO)
			goto parttypemenu;
	}

	scheme = items[choice].name;

	return scheme;
}

int
gpart_partition(const char *lg_name, const char *scheme)
{
	int button;
	struct gctl_req *r;
	const char *errstr;
	struct bsddialog_conf conf;

	bsddialog_initconf(&conf);

schememenu:
	if (scheme == NULL) {
		scheme = choose_part_type(default_scheme());

		if (scheme == NULL)
			return (-1);

		if (!is_scheme_bootable(scheme)) {
			char message[512];
			sprintf(message, "This partition scheme (%s) is not "
			    "bootable on this platform. Are you sure you want "
			    "to proceed?", scheme);
			conf.button.default_cancel = true;
			conf.title = "Warning";
			button = bsddialog_yesno(&conf, message, 0, 0);
			conf.button.default_cancel = false;
			if (button == BSDDIALOG_NO) {
				/* Reset scheme so user can choose another */
				scheme = NULL;
				goto schememenu;
			}
		}
	}

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "scheme", -1, scheme);
	gctl_ro_param(r, "verb", -1, "create");

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
		scheme = NULL;
		goto schememenu;
	}
	gctl_free(r);

	if (bootcode_path(scheme) != NULL)
		get_part_metadata(lg_name, 1)->bootcode = 1;
	return (0);
}

static void
gpart_activate(struct gprovider *pp)
{
	struct gconfig *gc;
	struct gctl_req *r;
	const char *errstr, *scheme;
	const char *attribute = NULL;
	intmax_t idx;

	/*
	 * Some partition schemes need this partition to be marked 'active'
	 * for it to be bootable.
	 */
	LIST_FOREACH(gc, &pp->lg_geom->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "scheme") == 0) {
			scheme = gc->lg_val;
			break;
		}
	}

	if (strcmp(scheme, "MBR") == 0 || strcmp(scheme, "EBR") == 0)
		attribute = "active";
	else
		return;

	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "index") == 0) {
			idx = atoi(gc->lg_val);
			break;
		}
	}

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, pp->lg_geom->lg_name);
	gctl_ro_param(r, "verb", -1, "set");
	gctl_ro_param(r, "attrib", -1, attribute);
	gctl_ro_param(r, "index", sizeof(idx), &idx);

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') 
		gpart_show_error("Error", "Error marking partition active:",
		    errstr);
	gctl_free(r);
}

void
gpart_set_root(const char *lg_name, const char *attribute)
{
	struct gctl_req *r;
	const char *errstr;

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, lg_name);
	gctl_ro_param(r, "flags", -1, "C");
	gctl_ro_param(r, "verb", -1, "set");
	gctl_ro_param(r, "attrib", -1, attribute);

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') 
		gpart_show_error("Error", "Error setting parameter on disk:",
		    errstr);
	gctl_free(r);
}

static void
gpart_bootcode(struct ggeom *gp)
{
	const char *bootcode;
	struct gconfig *gc;
	struct gctl_req *r;
	const char *errstr, *scheme;
	uint8_t *boot;
	size_t bootsize, bytes;
	int bootfd;
	struct bsddialog_conf conf;

	/*
	 * Write default bootcode to the newly partitioned disk, if that
	 * applies on this platform.
	 */
	LIST_FOREACH(gc, &gp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "scheme") == 0) {
			scheme = gc->lg_val;
			break;
		}
	}

	bootcode = bootcode_path(scheme);
	if (bootcode == NULL) 
		return;

	bootfd = open(bootcode, O_RDONLY);
	if (bootfd < 0) {
		bsddialog_initconf(&conf);
		conf.title = "Bootcode Error";
		bsddialog_msgbox(&conf, strerror(errno), 0, 0);
		return;
	}

	bootsize = lseek(bootfd, 0, SEEK_END);
	boot = malloc(bootsize);
	lseek(bootfd, 0, SEEK_SET);
	bytes = 0;
	while (bytes < bootsize)
		bytes += read(bootfd, boot + bytes, bootsize - bytes);
	close(bootfd);

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, gp->lg_name);
	gctl_ro_param(r, "verb", -1, "bootcode");
	gctl_ro_param(r, "bootcode", bootsize, boot);

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') 
		gpart_show_error("Bootcode Error", NULL, errstr);
	gctl_free(r);
	free(boot);
}

static void
gpart_partcode(struct gprovider *pp, const char *fstype)
{
	struct gconfig *gc;
	const char *scheme;
	const char *indexstr;
	char message[255], command[255];
	struct bsddialog_conf conf;

	LIST_FOREACH(gc, &pp->lg_geom->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "scheme") == 0) {
			scheme = gc->lg_val;
			break;
		}
	}

	/* Make sure this partition scheme needs partcode on this platform */
	if (partcode_path(scheme, fstype) == NULL)
		return;

	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "index") == 0) {
			indexstr = gc->lg_val;
			break;
		}
	}

	/* Shell out to gpart for partcode for now */
	sprintf(command, "gpart bootcode -p %s -i %s %s",
	    partcode_path(scheme, fstype), indexstr, pp->lg_geom->lg_name);
	if (system(command) != 0) {
		sprintf(message, "Error installing partcode on partition %s",
		    pp->lg_name);
		bsddialog_initconf(&conf);
		conf.title = "Error";
		bsddialog_msgbox(&conf, message, 0, 0);
	}
}

void
gpart_destroy(struct ggeom *lg_geom)
{
	struct gctl_req *r;
	struct gprovider *pp;
	const char *errstr;
	int force = 1;

	/* Delete all child metadata */
	LIST_FOREACH(pp, &lg_geom->lg_provider, lg_provider)
		gpart_delete(pp);

	/* Revert any local changes to get this geom into a pristine state */
	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, lg_geom->lg_name);
	gctl_ro_param(r, "verb", -1, "undo");
	gctl_issue(r); /* Ignore errors -- these are non-fatal */
	gctl_free(r);

	/* Now destroy the geom itself */
	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, lg_geom->lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "force", sizeof(force), &force);
	gctl_ro_param(r, "verb", -1, "destroy");
	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		/*
		 * Check if we reverted away the existence of the geom
		 * altogether. Show all other errors to the user.
		 */
		if (strtol(errstr, NULL, 0) != EINVAL)
			gpart_show_error("Error", NULL, errstr);
	}
	gctl_free(r);

	/* And any metadata associated with the partition scheme itself */
	delete_part_metadata(lg_geom->lg_name);
}

void
gpart_edit(struct gprovider *pp)
{
	struct gctl_req *r;
	struct gconfig *gc;
	struct gconsumer *cp;
	struct ggeom *geom;
	const char *errstr, *oldtype, *scheme;
	struct partition_metadata *md;
	char sizestr[32];
	char newfs[255];
	intmax_t idx;
	int hadlabel, choice, nitems;
	unsigned i;
	struct bsddialog_conf conf;

	struct bsddialog_formitem items[] = {
		{ "Type:", 1, 1, "", 1, 12, 12, 15, NULL, 0,
		    "Filesystem type (e.g. freebsd-ufs, freebsd-zfs, "
		    "freebsd-swap)"},
		{ "Size:", 2, 1, "", 2, 12, 12, 15, NULL, 0,
		    "Partition size. Append K, M, G for kilobytes, "
		    "megabytes or gigabytes."},
		{ "Mountpoint:", 3, 1, "", 3, 12, 12, 15, NULL, 0,
		    "Path at which to mount this partition (leave blank "
		    "for swap, set to / for root filesystem)"},
		{ "Label:", 4, 1, "", 4, 12, 12, 15, NULL, 0,
		    "Partition name. Not all partition schemes support this."},
	};

	bsddialog_initconf(&conf);

	/*
	 * Find the PART geom we are manipulating. This may be a consumer of
	 * this provider, or its parent. Check the consumer case first.
	 */
	geom = NULL;
	LIST_FOREACH(cp, &pp->lg_consumers, lg_consumers)
		if (strcmp(cp->lg_geom->lg_class->lg_name, "PART") == 0) {
			/* Check for zombie geoms, treating them as blank */
			scheme = NULL;
			LIST_FOREACH(gc, &cp->lg_geom->lg_config, lg_config) {
				if (strcmp(gc->lg_name, "scheme") == 0) {
					scheme = gc->lg_val;
					break;
				}
			}
			if (scheme == NULL || strcmp(scheme, "(none)") == 0) {
				gpart_partition(cp->lg_geom->lg_name, NULL);
				return;
			}

			/* If this is a nested partition, edit as usual */
			if (strcmp(pp->lg_geom->lg_class->lg_name, "PART") == 0)
				break;

			/* Destroy the geom and all sub-partitions */
			gpart_destroy(cp->lg_geom);

			/* Now re-partition and return */
			gpart_partition(cp->lg_geom->lg_name, NULL);
			return;
		}

	if (geom == NULL && strcmp(pp->lg_geom->lg_class->lg_name, "PART") == 0)
		geom = pp->lg_geom;

	if (geom == NULL) {
		/* Disk not partitioned, so partition it */
		gpart_partition(pp->lg_name, NULL);
		return;
	}

	LIST_FOREACH(gc, &geom->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "scheme") == 0) {
			scheme = gc->lg_val;
			break;
		}
	}

	nitems = scheme_supports_labels(scheme) ? 4 : 3;

	/* Edit editable parameters of a partition */
	hadlabel = 0;
	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "type") == 0) {
			oldtype = gc->lg_val;
			items[0].init = gc->lg_val;
		}
		if (strcmp(gc->lg_name, "label") == 0 && gc->lg_val != NULL) {
			hadlabel = 1;
			items[3].init = gc->lg_val;
		}
		if (strcmp(gc->lg_name, "index") == 0)
			idx = atoi(gc->lg_val);
	}

	TAILQ_FOREACH(md, &part_metadata, metadata) {
		if (md->name != NULL && strcmp(md->name, pp->lg_name) == 0) {
			if (md->fstab != NULL)
				items[2].init = md->fstab->fs_file;
			break;
		}
	}

	humanize_number(sizestr, 7, pp->lg_mediasize, "B", HN_AUTOSCALE,
	    HN_NOSPACE | HN_DECIMAL);
	items[1].init = sizestr;

editpart:
	conf.form.value_without_ok = true;
	conf.title = "Edit Partition";
	choice = bsddialog_form(&conf, "", 0, 0, 0, nitems, items);

	if (choice == BSDDIALOG_CANCEL)
		goto endedit;

	/* If this is the root partition, check that this fs is bootable */
	if (strcmp(items[2].value, "/") == 0 && !is_fs_bootable(scheme,
	    items[0].value)) {
		char message[512];
		sprintf(message, "This file system (%s) is not bootable "
		    "on this system. Are you sure you want to proceed?",
		    items[0].value);
		conf.button.default_cancel = true;
		conf.title = "Warning";
		choice = bsddialog_yesno(&conf, message, 0, 0);
		conf.button.default_cancel = false;
		if (choice == BSDDIALOG_CANCEL)
			goto editpart;
	}

	/* Check if the label has a / in it */
	if (items[3].value != NULL && strchr(items[3].value, '/') != NULL) {
		conf.title = "Error";
		bsddialog_msgbox(&conf, "Label contains a /, which is not an "
		    "allowed character.", 0, 0);
		goto editpart;
	}

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, geom->lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "verb", -1, "modify");
	gctl_ro_param(r, "index", sizeof(idx), &idx);
	if (items[3].value != NULL && (hadlabel || items[3].value[0] != '\0'))
		gctl_ro_param(r, "label", -1, items[3].value);
	gctl_ro_param(r, "type", -1, items[0].value);
	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
		goto editpart;
	}
	gctl_free(r);

	newfs_command(items[0].value, newfs, 1);
	set_default_part_metadata(pp->lg_name, scheme, items[0].value,
	    items[2].value, (strcmp(oldtype, items[0].value) != 0) ?
	    newfs : NULL);

endedit:
	if (strcmp(oldtype, items[0].value) != 0 && cp != NULL)
		gpart_destroy(cp->lg_geom);
	if (strcmp(oldtype, items[0].value) != 0 && strcmp(items[0].value,
	    "freebsd") == 0)
		gpart_partition(pp->lg_name, "BSD");

	for (i = 0; i < nitems(items); i++)
		if (items[i].value != NULL)
			free(items[i].value);
}

void
set_default_part_metadata(const char *name, const char *scheme,
    const char *type, const char *mountpoint, const char *newfs)
{
	struct partition_metadata *md;
	char *zpool_name = NULL;
	const char *default_bootmount = NULL;
	int i;

	/* Set part metadata */
	md = get_part_metadata(name, 1);

	if (newfs) {
		if (md->newfs != NULL) {
			free(md->newfs);
			md->newfs = NULL;
		}

		if (newfs != NULL && newfs[0] != '\0') {
			md->newfs = malloc(strlen(newfs) + strlen(" /dev/") +
			    strlen(mountpoint) + 5 + strlen(name) + 1);
			if (strcmp("freebsd-zfs", type) == 0) {
				zpool_name = strdup((strlen(mountpoint) == 1) ?
				    "root" : &mountpoint[1]);
				for (i = 0; zpool_name[i] != 0; i++)
					if (!isalnum(zpool_name[i]))
						zpool_name[i] = '_';
				sprintf(md->newfs, "%s %s /dev/%s", newfs,
				    zpool_name, name);
			} else {
				sprintf(md->newfs, "%s /dev/%s", newfs, name);
			}
		}
	}

	if (strcmp(type, "freebsd-swap") == 0)
		mountpoint = "none";
	if (strcmp(type, bootpart_type(scheme, &default_bootmount)) == 0) {
		if (default_bootmount == NULL)
			md->bootcode = 1;
		else if (mountpoint == NULL || strlen(mountpoint) == 0)
			mountpoint = default_bootmount;
	}

	if (mountpoint == NULL || mountpoint[0] == '\0') {
		if (md->fstab != NULL) {
			free(md->fstab->fs_spec);
			free(md->fstab->fs_file);
			free(md->fstab->fs_vfstype);
			free(md->fstab->fs_mntops);
			free(md->fstab->fs_type);
			free(md->fstab);
			md->fstab = NULL;
		}
	} else {
		if (md->fstab == NULL) {
			md->fstab = malloc(sizeof(struct fstab));
		} else {
			free(md->fstab->fs_spec);
			free(md->fstab->fs_file);
			free(md->fstab->fs_vfstype);
			free(md->fstab->fs_mntops);
			free(md->fstab->fs_type);
		}
		if (strcmp("freebsd-zfs", type) == 0) {
			md->fstab->fs_spec = strdup(zpool_name);
		} else {
			md->fstab->fs_spec = malloc(strlen(name) +
			    strlen("/dev/") + 1);
			sprintf(md->fstab->fs_spec, "/dev/%s", name);
		}
		md->fstab->fs_file = strdup(mountpoint);
		/* Get VFS from text after freebsd-, if possible */
		if (strncmp("freebsd-", type, 8) == 0)
			md->fstab->fs_vfstype = strdup(&type[8]);
		else if (strcmp("fat32", type) == 0 || strcmp("efi", type) == 0
	     	    || strcmp("ms-basic-data", type) == 0)
			md->fstab->fs_vfstype = strdup("msdosfs");
		else
			md->fstab->fs_vfstype = strdup(type); /* Guess */
		if (strcmp(type, "freebsd-swap") == 0) {
			md->fstab->fs_type = strdup(FSTAB_SW);
			md->fstab->fs_freq = 0;
			md->fstab->fs_passno = 0;
		} else if (strcmp(type, "freebsd-zfs") == 0) {
			md->fstab->fs_type = strdup(FSTAB_RW);
			md->fstab->fs_freq = 0;
			md->fstab->fs_passno = 0;
		} else {
			md->fstab->fs_type = strdup(FSTAB_RW);
			if (strcmp(mountpoint, "/") == 0) {
				md->fstab->fs_freq = 1;
				md->fstab->fs_passno = 1;
			} else {
				md->fstab->fs_freq = 2;
				md->fstab->fs_passno = 2;
			}
		}
		md->fstab->fs_mntops = strdup(md->fstab->fs_type);
	}

	if (zpool_name != NULL)
		free(zpool_name);
}

static
int part_compare(const void *xa, const void *xb)
{
	struct gprovider **a = (struct gprovider **)xa;
	struct gprovider **b = (struct gprovider **)xb;
	intmax_t astart, bstart;
	struct gconfig *gc;
	
	astart = bstart = 0;
	LIST_FOREACH(gc, &(*a)->lg_config, lg_config)
		if (strcmp(gc->lg_name, "start") == 0) {
			astart = strtoimax(gc->lg_val, NULL, 0);
			break;
		}
	LIST_FOREACH(gc, &(*b)->lg_config, lg_config)
		if (strcmp(gc->lg_name, "start") == 0) {
			bstart = strtoimax(gc->lg_val, NULL, 0);
			break;
		}

	if (astart < bstart)
		return -1;
	else if (astart > bstart)
		return 1;
	else
		return 0;
}

intmax_t
gpart_max_free(struct ggeom *geom, intmax_t *npartstart)
{
	struct gconfig *gc;
	struct gprovider *pp, **providers;
	intmax_t sectorsize, stripesize, offset;
	intmax_t lastend;
	intmax_t start, end;
	intmax_t maxsize, maxstart;
	intmax_t partstart, partend;
	int i, nparts;

	/* Now get the maximum free size and free start */
	start = end = 0;
	LIST_FOREACH(gc, &geom->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "first") == 0)
			start = strtoimax(gc->lg_val, NULL, 0);
		if (strcmp(gc->lg_name, "last") == 0)
			end = strtoimax(gc->lg_val, NULL, 0);
	}

	i = nparts = 0;
	LIST_FOREACH(pp, &geom->lg_provider, lg_provider)
		nparts++;
	providers = calloc(nparts, sizeof(providers[0]));
	LIST_FOREACH(pp, &geom->lg_provider, lg_provider)
		providers[i++] = pp;
	qsort(providers, nparts, sizeof(providers[0]), part_compare);

	lastend = start - 1;
	maxsize = 0;
	for (i = 0; i < nparts; i++) {
		pp = providers[i];

		LIST_FOREACH(gc, &pp->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "start") == 0)
				partstart = strtoimax(gc->lg_val, NULL, 0);
			if (strcmp(gc->lg_name, "end") == 0)
				partend = strtoimax(gc->lg_val, NULL, 0);
		}

		if (partstart - lastend > maxsize) {
			maxsize = partstart - lastend - 1;
			maxstart = lastend + 1;
		}

		lastend = partend;
	}

	if (end - lastend > maxsize) {
		maxsize = end - lastend;
		maxstart = lastend + 1;
	}

	pp = LIST_FIRST(&geom->lg_consumer)->lg_provider;

	/*
	 * Round the start and size of the largest available space up to
	 * the nearest multiple of the adjusted stripe size.
	 *
	 * The adjusted stripe size is the least common multiple of the
	 * actual stripe size, or the sector size if no stripe size was
	 * reported, and 4096.  The reason for this is that contemporary
	 * disks often have 4096-byte physical sectors but report 512
	 * bytes instead for compatibility with older / broken operating
	 * systems and BIOSes.  For the same reasons, virtualized storage
	 * may also report a 512-byte stripe size, or none at all.
	 */
	sectorsize = pp->lg_sectorsize;
	if ((stripesize = pp->lg_stripesize) == 0)
		stripesize = sectorsize;
	while (stripesize % 4096 != 0)
		stripesize *= 2;
	if ((offset = maxstart * sectorsize % stripesize) != 0) {
		offset = (stripesize - offset) / sectorsize;
		maxstart += offset;
		maxsize -= offset;
	}

	if (npartstart != NULL)
		*npartstart = maxstart;

	return (maxsize);
}

static size_t
add_boot_partition(struct ggeom *geom, struct gprovider *pp,
    const char *scheme, int interactive)
{
	struct gconfig *gc;
	struct gprovider *ppi;
	int choice;
	struct bsddialog_conf conf;

	/* Check for existing freebsd-boot partition */
	LIST_FOREACH(ppi, &geom->lg_provider, lg_provider) {
		struct partition_metadata *md;
		const char *bootmount = NULL;

		LIST_FOREACH(gc, &ppi->lg_config, lg_config)
			if (strcmp(gc->lg_name, "type") == 0)
				break;
		if (gc == NULL)
			continue;
		if (strcmp(gc->lg_val, bootpart_type(scheme, &bootmount)) != 0)
			continue;

		/*
		 * If the boot partition is not mountable and needs partcode,
		 * but doesn't have it, it doesn't satisfy our requirements.
		 */
		md = get_part_metadata(ppi->lg_name, 0);
		if (bootmount == NULL && (md == NULL || !md->bootcode))
			continue;

		/* If it is mountable, but mounted somewhere else, remount */
		if (bootmount != NULL && md != NULL && md->fstab != NULL
		    && strlen(md->fstab->fs_file) > 0
		    && strcmp(md->fstab->fs_file, bootmount) != 0)
			continue;

		/* If it is mountable, but mountpoint is not set, mount it */
		if (bootmount != NULL && md == NULL)
			set_default_part_metadata(ppi->lg_name, scheme,
			    gc->lg_val, bootmount, NULL);
		
		/* Looks good at this point, no added data needed */
		return (0);
	}

	if (interactive) {
		bsddialog_initconf(&conf);
		conf.title = "Boot Partition";
		choice = bsddialog_yesno(&conf,
		    "This partition scheme requires a boot partition "
		    "for the disk to be bootable. Would you like to "
		    "make one now?", 0, 0);
	} else {
		choice = BSDDIALOG_YES;
	}

	if (choice == BSDDIALOG_YES) {
		struct partition_metadata *md;
		const char *bootmount = NULL;
		char *bootpartname = NULL;
		char sizestr[7];

		humanize_number(sizestr, 7,
		    bootpart_size(scheme), "B", HN_AUTOSCALE,
		    HN_NOSPACE | HN_DECIMAL);

		gpart_create(pp, bootpart_type(scheme, &bootmount),
		    sizestr, bootmount, &bootpartname, 0);

		if (bootpartname == NULL) /* Error reported to user already */
			return 0;

		/* If the part is not mountable, make sure newfs isn't set */
		if (bootmount == NULL) {
			md = get_part_metadata(bootpartname, 0);
			if (md != NULL && md->newfs != NULL) {
				free(md->newfs);
				md->newfs = NULL;
			}
		}

		free(bootpartname);

		return (bootpart_size(scheme));
	}
	
	return (0);
}

void
gpart_create(struct gprovider *pp, const char *default_type,
    const char *default_size, const char *default_mountpoint,
    char **partname, int interactive)
{
	struct gctl_req *r;
	struct gconfig *gc;
	struct gconsumer *cp;
	struct ggeom *geom;
	const char *errstr, *scheme;
	char sizestr[32], startstr[32], output[64], *newpartname;
	char newfs[255], options_fstype[64];
	intmax_t maxsize, size, sector, firstfree, stripe;
	uint64_t bytes;
	int nitems, choice, junk;
	unsigned i;
	bool init_allocated;
	struct bsddialog_conf conf;

	struct bsddialog_formitem items[] = {
		{"Type:", 1, 1, "freebsd-ufs", 1, 12, 12, 15, NULL, 0,
		    "Filesystem type (e.g. freebsd-ufs, freebsd-zfs, "
		    "freebsd-swap)"},
		{"Size:", 2, 1, "", 2, 12, 12, 15, NULL, 0,
		    "Partition size. Append K, M, G for kilobytes, "
		    "megabytes or gigabytes."},
		{"Mountpoint:", 3, 1, "", 3, 12, 12, 15, NULL, 0,
		    "Path at which to mount partition (blank for "
		    "swap, / for root filesystem)"},
		{"Label:", 4, 1, "", 4, 12, 12, 15, NULL, 0,
		    "Partition name. Not all partition schemes support this."},
	};

	bsddialog_initconf(&conf);

	if (partname != NULL)
		*partname = NULL;

	/* Record sector and stripe sizes */
	sector = pp->lg_sectorsize;
	stripe = pp->lg_stripesize;

	/*
	 * Find the PART geom we are manipulating. This may be a consumer of
	 * this provider, or its parent. Check the consumer case first.
	 */
	geom = NULL;
	LIST_FOREACH(cp, &pp->lg_consumers, lg_consumers)
		if (strcmp(cp->lg_geom->lg_class->lg_name, "PART") == 0) {
			geom = cp->lg_geom;
			break;
		}

	if (geom == NULL && strcmp(pp->lg_geom->lg_class->lg_name, "PART") == 0)
		geom = pp->lg_geom;

	/* Now get the partition scheme */
	scheme = NULL;
	if (geom != NULL) {
		LIST_FOREACH(gc, &geom->lg_config, lg_config) 
			if (strcmp(gc->lg_name, "scheme") == 0)
				scheme = gc->lg_val;
	}

	if (geom == NULL || scheme == NULL || strcmp(scheme, "(none)") == 0) {
		if (gpart_partition(pp->lg_name, NULL) == 0) {
			bsddialog_msgbox(&conf,
			    "The partition table has been successfully created."
			    " Please press Create again to create partitions.",
			    0, 0);
		}

		return;
	}

	/*
	 * If we still don't have a geom, either the user has
	 * canceled partitioning or there has been an error which has already
	 * been displayed, so bail.
	 */
	if (geom == NULL)
		return;

	maxsize = size = gpart_max_free(geom, &firstfree);
	if (size <= 0) {
		conf .title = "Error";
		bsddialog_msgbox(&conf, "No free space left on device.", 0, 0);
		return;
	}

	humanize_number(sizestr, 7, size*sector, "B", HN_AUTOSCALE,
	    HN_NOSPACE | HN_DECIMAL);
	items[1].init = sizestr;

	/* Special-case the MBR default type for nested partitions */
	if (strcmp(scheme, "MBR") == 0) {
		items[0].init = "freebsd";
		items[0].bottomdesc = "Filesystem type (e.g. freebsd, fat32)";
	}

	nitems = scheme_supports_labels(scheme) ? 4 : 3;

	if (default_type != NULL)
		items[0].init = (char *)default_type;
	if (default_size != NULL)
		items[1].init = (char *)default_size;
	if (default_mountpoint != NULL)
		items[2].init = (char *)default_mountpoint;

	/* Default options */
	strncpy(options_fstype, items[0].init,
	    sizeof(options_fstype));
	newfs_command(options_fstype, newfs, 1);

	init_allocated = false;
addpartform:
	if (interactive) {
		conf.button.with_extra = true;
		conf.button.extra_label = "Options";
		conf.form.value_without_ok = true;
		conf.title = "Add Partition";
		choice = bsddialog_form(&conf, "", 0, 0, 0, nitems, items);
		conf.button.with_extra = false;
		conf.button.extra_label = NULL;
		conf.form.value_without_ok = false;
		switch (choice) {
		case BSDDIALOG_OK:
			break;
		case BSDDIALOG_CANCEL:
			return;
		case BSDDIALOG_EXTRA: /* Options */
			strncpy(options_fstype, items[0].value,
			    sizeof(options_fstype));
			newfs_command(options_fstype, newfs, 0);
			for (i = 0; i < nitems(items); i++) {
				if (init_allocated)
					free((char*)items[i].init);
				items[i].init = items[i].value;
			}
			init_allocated = true;
			goto addpartform;
		}
	} else { /* auto partitioning */
		items[0].value = strdup(items[0].init);
		items[1].value = strdup(items[1].init);
		items[2].value = strdup(items[2].init);
		if (nitems > 3)
			items[3].value = strdup(items[3].init);
	}

	/*
	 * If the user changed the fs type after specifying options, undo
	 * their choices in favor of the new filesystem's defaults.
	 */
	if (strcmp(options_fstype, items[0].value) != 0) {
		strncpy(options_fstype, items[0].value, sizeof(options_fstype));
		newfs_command(options_fstype, newfs, 1);
	}

	size = maxsize;
	if (strlen(items[1].value) > 0) {
		if (expand_number(items[1].value, &bytes) != 0) {
			char error[512];

			sprintf(error, "Invalid size: %s\n", strerror(errno));
			conf.title = "Error";
			bsddialog_msgbox(&conf, error, 0, 0);
			goto addpartform;
		}
		size = MIN((intmax_t)(bytes/sector), maxsize);
	}

	/* Check if the label has a / in it */
	if (items[3].value != NULL && strchr(items[3].value, '/') != NULL) {
		conf.title = "Error";
		bsddialog_msgbox(&conf, "Label contains a /, which is not an "
		    "allowed character.", 0, 0);
		goto addpartform;
	}

	/* Warn if no mountpoint set */
	if (strcmp(items[0].value, "freebsd-ufs") == 0 &&
	    items[2].value[0] != '/') {
		choice = 0;
		if (interactive) {
			conf.button.default_cancel = true;
			conf.title = "Warning";
			choice = bsddialog_yesno(&conf,
			    "This partition does not have a valid mountpoint "
			    "(for the partition from which you intend to boot the "
			    "operating system, the mountpoint should be /). Are you "
			    "sure you want to continue?"
			, 0, 0);
			conf.button.default_cancel = false;
		}
		if (choice == BSDDIALOG_CANCEL)
			goto addpartform;
	}

	/*
	 * Error if this scheme needs nested partitions, this is one, and
	 * a mountpoint was set.
	 */
	if (strcmp(items[0].value, "freebsd") == 0 &&
	    strlen(items[2].value) > 0) {
		conf.title = "Error";
		bsddialog_msgbox(&conf, "Partitions of type \"freebsd\" are "
		    "nested BSD-type partition schemes and cannot have "
		    "mountpoints. After creating one, select it and press "
		    "Create again to add the actual file systems.", 0, 0);
		goto addpartform;
	}

	/* If this is the root partition, check that this scheme is bootable */
	if (strcmp(items[2].value, "/") == 0 && !is_scheme_bootable(scheme)) {
		char message[512];
		sprintf(message, "This partition scheme (%s) is not bootable "
		    "on this platform. Are you sure you want to proceed?",
		    scheme);
		conf.button.default_cancel = true;
		conf.title = "Warning";
		choice = bsddialog_yesno(&conf, message, 0, 0);
		conf.button.default_cancel = false;
		if (choice == BSDDIALOG_CANCEL)
			goto addpartform;
	}

	/* If this is the root partition, check that this fs is bootable */
	if (strcmp(items[2].value, "/") == 0 && !is_fs_bootable(scheme,
	    items[0].value)) {
		char message[512];
		sprintf(message, "This file system (%s) is not bootable "
		    "on this system. Are you sure you want to proceed?",
		    items[0].value);
		conf.button.default_cancel = true;
		conf.title = "Warning";
		choice = bsddialog_yesno(&conf, message, 0, 0);
		conf.button.default_cancel = false;
		if (choice == BSDDIALOG_CANCEL)
			goto addpartform;
	}

	/*
	 * If this is the root partition, and we need a boot partition, ask
	 * the user to add one.
	 */

	if ((strcmp(items[0].value, "freebsd") == 0 ||
	    strcmp(items[2].value, "/") == 0) && bootpart_size(scheme) > 0) {
		size_t bytes = add_boot_partition(geom, pp, scheme,
		    interactive);

		/* Now adjust the part we are really adding forward */
		if (bytes > 0) {
			firstfree += bytes / sector;
			size -= (bytes + stripe)/sector;
			if (stripe > 0 && (firstfree*sector % stripe) != 0) 
				firstfree += (stripe - ((firstfree*sector) %
				    stripe)) / sector;
		}
	}

	output[0] = '\0';

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, geom->lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "verb", -1, "add");

	gctl_ro_param(r, "type", -1, items[0].value);
	snprintf(sizestr, sizeof(sizestr), "%jd", size);
	gctl_ro_param(r, "size", -1, sizestr);
	snprintf(startstr, sizeof(startstr), "%jd", firstfree);
	gctl_ro_param(r, "start", -1, startstr);
	if (items[3].value != NULL && items[3].value[0] != '\0')
		gctl_ro_param(r, "label", -1, items[3].value);
	gctl_add_param(r, "output", sizeof(output), output,
	    GCTL_PARAM_WR | GCTL_PARAM_ASCII);
	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
		goto addpartform;
	}
	newpartname = strtok(output, " ");
	gctl_free(r);

	/*
	 * Try to destroy any geom that gpart picked up already here from
	 * dirty blocks.
	 */
	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, newpartname);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	junk = 1;
	gctl_ro_param(r, "force", sizeof(junk), &junk);
	gctl_ro_param(r, "verb", -1, "destroy");
	gctl_issue(r); /* Error usually expected and non-fatal */
	gctl_free(r);


	if (strcmp(items[0].value, "freebsd") == 0)
		gpart_partition(newpartname, "BSD");
	else
		set_default_part_metadata(newpartname, scheme,
		    items[0].value, items[2].value, newfs);

	for (i = 0; i < nitems(items); i++) {
		if (items[i].value != NULL) {
			free(items[i].value);
			if (init_allocated && items[i].init != NULL)
				free((char*)items[i].init);
		}
	}

	if (partname != NULL)
		*partname = strdup(newpartname);
}
	
void
gpart_delete(struct gprovider *pp)
{
	struct gconfig *gc;
	struct ggeom *geom;
	struct gconsumer *cp;
	struct gctl_req *r;
	const char *errstr;
	intmax_t idx;
	int is_partition;
	struct bsddialog_conf conf;

	/* Is it a partition? */
	is_partition = (strcmp(pp->lg_geom->lg_class->lg_name, "PART") == 0);

	/* Find out if this is the root of a gpart geom */
	geom = NULL;
	LIST_FOREACH(cp, &pp->lg_consumers, lg_consumers)
		if (strcmp(cp->lg_geom->lg_class->lg_name, "PART") == 0) {
			geom = cp->lg_geom;
			break;
		}

	/* If so, destroy all children */
	if (geom != NULL) {
		gpart_destroy(geom);

		/* If this is a partition, revert it, so it can be deleted */
		if (is_partition) {
			r = gctl_get_handle();
			gctl_ro_param(r, "class", -1, "PART");
			gctl_ro_param(r, "arg0", -1, geom->lg_name);
			gctl_ro_param(r, "verb", -1, "undo");
			gctl_issue(r); /* Ignore non-fatal errors */
			gctl_free(r);
		}
	}

	/*
	 * If this is not a partition, see if that is a problem, complain if
	 * necessary, and return always, since we need not do anything further,
	 * error or no.
	 */
	if (!is_partition) {
		if (geom == NULL) {
			bsddialog_initconf(&conf);
			conf.title = "Error";
			bsddialog_msgbox(&conf,
			    "Only partitions can be deleted.", 0, 0);
		}
		return;
	}

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, pp->lg_geom->lg_class->lg_name);
	gctl_ro_param(r, "arg0", -1, pp->lg_geom->lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "verb", -1, "delete");

	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "index") == 0) {
			idx = atoi(gc->lg_val);
			gctl_ro_param(r, "index", sizeof(idx), &idx);
			break;
		}
	}

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
		return;
	}

	gctl_free(r);

	delete_part_metadata(pp->lg_name);
}

void
gpart_revert_all(struct gmesh *mesh)
{
	struct gclass *classp;
	struct gconfig *gc;
	struct ggeom *gp;
	struct gctl_req *r;
	const char *modified;
	struct bsddialog_conf conf;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, "PART") == 0)
			break;
	}

	if (strcmp(classp->lg_name, "PART") != 0) {
		bsddialog_initconf(&conf);
		conf.title = "Error";
		bsddialog_msgbox(&conf, "gpart not found!", 0, 0);
		return;
	}

	LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
		modified = "true"; /* XXX: If we don't know (kernel too old),
				    * assume there are modifications. */
		LIST_FOREACH(gc, &gp->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "modified") == 0) {
				modified = gc->lg_val;
				break;
			}
		}

		if (strcmp(modified, "false") == 0)
			continue;

		r = gctl_get_handle();
		gctl_ro_param(r, "class", -1, "PART");
		gctl_ro_param(r, "arg0", -1, gp->lg_name);
		gctl_ro_param(r, "verb", -1, "undo");

		gctl_issue(r);
		gctl_free(r);
	}
}

void
gpart_commit(struct gmesh *mesh)
{
	struct partition_metadata *md;
	struct gclass *classp;
	struct ggeom *gp;
	struct gconfig *gc;
	struct gconsumer *cp;
	struct gprovider *pp;
	struct gctl_req *r;
	const char *errstr;
	const char *modified;
	const char *rootfs;
	struct bsddialog_conf conf;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, "PART") == 0)
			break;
	}

	/* Figure out what filesystem / uses */
	rootfs = "ufs"; /* Assume ufs if nothing else present */
	TAILQ_FOREACH(md, &part_metadata, metadata) {
		if (md->fstab != NULL && strcmp(md->fstab->fs_file, "/") == 0) {
			rootfs = md->fstab->fs_vfstype;
			break;
		}
	}

	if (strcmp(classp->lg_name, "PART") != 0) {
		bsddialog_initconf(&conf);
		conf.title = "Error";
		bsddialog_msgbox(&conf, "gpart not found!", 0, 0);
		return;
	}

	LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
		modified = "true"; /* XXX: If we don't know (kernel too old),
				    * assume there are modifications. */
		LIST_FOREACH(gc, &gp->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "modified") == 0) {
				modified = gc->lg_val;
				break;
			}
		}

		if (strcmp(modified, "false") == 0)
			continue;

		/* Add bootcode if necessary, before the commit */
		md = get_part_metadata(gp->lg_name, 0);
		if (md != NULL && md->bootcode)
			gpart_bootcode(gp);

		/* Now install partcode on its partitions, if necessary */
		LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
			md = get_part_metadata(pp->lg_name, 0);
			if (md == NULL || !md->bootcode)
				continue;
		
			/* Mark this partition active if that's required */
			gpart_activate(pp);

			/* Check if the partition has sub-partitions */
			LIST_FOREACH(cp, &pp->lg_consumers, lg_consumers)
				if (strcmp(cp->lg_geom->lg_class->lg_name,
				    "PART") == 0)
					break;

			if (cp == NULL) /* No sub-partitions */
				gpart_partcode(pp, rootfs);
		}

		r = gctl_get_handle();
		gctl_ro_param(r, "class", -1, "PART");
		gctl_ro_param(r, "arg0", -1, gp->lg_name);
		gctl_ro_param(r, "verb", -1, "commit");

		errstr = gctl_issue(r);
		if (errstr != NULL && errstr[0] != '\0') 
			gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
	}
}

