/*-
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
#include <errno.h>
#include <libutil.h>
#include <inttypes.h>

#include <libgeom.h>
#include <dialog.h>
#include <dlg_keys.h>

#include "partedit.h"

#define GPART_FLAGS "x" /* Do not commit changes by default */

static void
gpart_show_error(const char *title, const char *explanation, const char *errstr)
{
	char *errmsg;
	char message[512];
	int error;

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

	dialog_msgbox(title, message, 0, 0, TRUE);
}

static int
scheme_supports_labels(const char *scheme)
{
	if (strcmp(scheme, "APM") == 0)
		return (1);
	if (strcmp(scheme, "GPT") == 0)
		return (1);
	if (strcmp(scheme, "PC98") == 0)
		return (1);

	return (0);
}

static void
newfs_command(const char *fstype, char *command, int use_default)
{
	if (strcmp(fstype, "freebsd-ufs") == 0) {
		int i;
		DIALOG_LISTITEM items[] = {
			{"UFS1", "UFS Version 1",
			    "Use version 1 of the UFS file system instead "
			    "of version 2 (not recommended)", 0 },
			{"SU", "Softupdates",
			    "Enable softupdates (default)", 1 },
			{"SUJ", "Softupdates journaling",
			    "Enable file system journaling (default - "
			    "turn off for SSDs)", 1 },
			{"TRIM", "Enable SSD TRIM support",
			    "Enable TRIM support, useful on solid-state drives",
			    0 },
		};

		if (!use_default) {
			int choice;
			choice = dlg_checklist("UFS Options", "", 0, 0, 0,
			    sizeof(items)/sizeof(items[0]), items, NULL,
			    FLAG_CHECK, &i);
			if (choice == 1) /* Cancel */
				return;
		}

		strcpy(command, "newfs ");
		for (i = 0; i < (int)(sizeof(items)/sizeof(items[0])); i++) {
			if (items[i].state == 0)
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
	} else if (strcmp(fstype, "fat32") == 0 || strcmp(fstype, "efi") == 0) {
		int i;
		DIALOG_LISTITEM items[] = {
			{"FAT32", "FAT Type 32",
			    "Create a FAT32 filesystem (default)", 1 },
			{"FAT16", "FAT Type 16",
			    "Create a FAT16 filesystem", 0 },
			{"FAT12", "FAT Type 12",
			    "Create a FAT12 filesystem", 0 },
		};

		if (!use_default) {
			int choice;
			choice = dlg_checklist("FAT Options", "", 0, 0, 0,
			    sizeof(items)/sizeof(items[0]), items, NULL,
			    FLAG_RADIO, &i);
			if (choice == 1) /* Cancel */
				return;
		}

		strcpy(command, "newfs_msdos ");
		for (i = 0; i < (int)(sizeof(items)/sizeof(items[0])); i++) {
			if (items[i].state == 0)
				continue;
			if (strcmp(items[i].name, "FAT32") == 0)
				strcat(command, "-F 32 ");
			else if (strcmp(items[i].name, "FAT16") == 0)
				strcat(command, "-F 16 ");
			else if (strcmp(items[i].name, "SUJ") == 0)
				strcat(command, "-F 12 ");
		}
	} else {
		if (!use_default)
			dialog_msgbox("Error", "No configurable options exist "
			    "for this filesystem.", 0, 0, TRUE);
		command[0] = '\0';
	}
}

int
gpart_partition(const char *lg_name, const char *scheme)
{
	int cancel, choice;
	struct gctl_req *r;
	const char *errstr;

	DIALOG_LISTITEM items[] = {
		{"APM", "Apple Partition Map",
		    "Bootable on PowerPC Apple Hardware", 0 },
		{"BSD", "BSD Labels",
		    "Bootable on most x86 systems", 0 },
		{"GPT", "GUID Partition Table",
		    "Bootable on most x86 systems", 0 },
		{"MBR", "DOS Partitions",
		    "Bootable on most x86 systems", 0 },
		{"PC98", "NEC PC9801 Partition Table",
		    "Bootable on NEC PC9801 systems", 0 },
		{"VTOC8", "Sun VTOC8 Partition Table",
		    "Bootable on Sun SPARC systems", 0 },
	};

schememenu:
	if (scheme == NULL) {
		dialog_vars.default_item = __DECONST(char *, default_scheme());
		cancel = dlg_menu("Partition Scheme",
		    "Select a partition scheme for this volume:", 0, 0, 0,
		    sizeof(items) / sizeof(items[0]), items, &choice, NULL);
		dialog_vars.default_item = NULL;

		if (cancel)
			return (-1);

		if (!is_scheme_bootable(items[choice].name)) {
			char message[512];
			sprintf(message, "This partition scheme (%s) is not "
			    "bootable on this platform. Are you sure you want "
			    "to proceed?", items[choice].name);
			dialog_vars.defaultno = TRUE;
			cancel = dialog_yesno("Warning", message, 0, 0);
			dialog_vars.defaultno = FALSE;
			if (cancel) /* cancel */
				goto schememenu;
		}

		scheme = items[choice].name;
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

	if (strcmp(scheme, "MBR") == 0 || strcmp(scheme, "EBR") == 0 ||
	    strcmp(scheme, "PC98") == 0)
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
	if (bootfd <= 0) {
		dialog_msgbox("Bootcode Error", strerror(errno), 0, 0,
		    TRUE);
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
gpart_partcode(struct gprovider *pp)
{
	struct gconfig *gc;
	const char *scheme;
	const char *indexstr;
	char message[255], command[255];

	LIST_FOREACH(gc, &pp->lg_geom->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "scheme") == 0) {
			scheme = gc->lg_val;
			break;
		}
	}

	/* Make sure this partition scheme needs partcode on this platform */
	if (partcode_path(scheme) == NULL)
		return;

	LIST_FOREACH(gc, &pp->lg_config, lg_config) {
		if (strcmp(gc->lg_name, "index") == 0) {
			indexstr = gc->lg_val;
			break;
		}
	}

	/* Shell out to gpart for partcode for now */
	sprintf(command, "gpart bootcode -p %s -i %s %s",
	    partcode_path(scheme), indexstr, pp->lg_geom->lg_name);
	if (system(command) != 0) {
		sprintf(message, "Error installing partcode on partition %s",
		    pp->lg_name);
		dialog_msgbox("Error", message, 0, 0, TRUE);
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
	if (errstr != NULL && errstr[0] != '\0') 
		gpart_show_error("Error", NULL, errstr);
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
	char newfs[64];
	intmax_t idx;
	int hadlabel, choice, junk, nitems;
	unsigned i;

	DIALOG_FORMITEM items[] = {
		{0, "Type:", 5, 0, 0, FALSE, "", 11, 0, 12, 15, 0,
		    FALSE, "Filesystem type (e.g. freebsd-ufs, freebsd-swap)",
		    FALSE},
		{0, "Size:", 5, 1, 0, FALSE, "", 11, 1, 12, 0, 0,
		    FALSE, "Partition size. Append K, M, G for kilobytes, "
		    "megabytes or gigabytes.", FALSE},
		{0, "Mountpoint:", 11, 2, 0, FALSE, "", 11, 2, 12, 15, 0,
		    FALSE, "Path at which to mount this partition (leave blank "
		    "for swap, set to / for root filesystem)", FALSE},
		{0, "Label:", 7, 3, 0, FALSE, "", 11, 3, 12, 15, 0, FALSE,
		    "Partition name. Not all partition schemes support this.",
		    FALSE},
	};

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
			items[0].text = gc->lg_val;
		}
		if (strcmp(gc->lg_name, "label") == 0 && gc->lg_val != NULL) {
			hadlabel = 1;
			items[3].text = gc->lg_val;
		}
		if (strcmp(gc->lg_name, "index") == 0)
			idx = atoi(gc->lg_val);
	}

	TAILQ_FOREACH(md, &part_metadata, metadata) {
		if (md->name != NULL && strcmp(md->name, pp->lg_name) == 0) {
			if (md->fstab != NULL)
				items[2].text = md->fstab->fs_file;
			break;
		}
	}

	humanize_number(sizestr, 7, pp->lg_mediasize, "B", HN_AUTOSCALE,
	    HN_NOSPACE | HN_DECIMAL);
	items[1].text = sizestr;

editpart:
	choice = dlg_form("Edit Partition", "", 0, 0, 0, nitems, items, &junk);

	if (choice) /* Cancel pressed */
		return;

	/* Check if the label has a / in it */
	if (strchr(items[3].text, '/') != NULL) {
		dialog_msgbox("Error", "Label contains a /, which is not an "
		    "allowed character.", 0, 0, TRUE);
		goto editpart;
	}

	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, geom->lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "verb", -1, "modify");
	gctl_ro_param(r, "index", sizeof(idx), &idx);
	if (hadlabel || items[3].text[0] != '\0')
		gctl_ro_param(r, "label", -1, items[3].text);
	gctl_ro_param(r, "type", -1, items[0].text);
	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
		goto editpart;
	}
	gctl_free(r);

	newfs_command(items[0].text, newfs, 1);
	set_default_part_metadata(pp->lg_name, scheme, items[0].text,
	    items[2].text, (strcmp(oldtype, items[0].text) != 0) ?
	    newfs : NULL);

	for (i = 0; i < (sizeof(items) / sizeof(items[0])); i++)
		if (items[i].text_free)
			free(items[i].text);
}

void
set_default_part_metadata(const char *name, const char *scheme,
    const char *type, const char *mountpoint, const char *newfs)
{
	struct partition_metadata *md;

	/* Set part metadata */
	md = get_part_metadata(name, 1);

	if (newfs) {
		if (md->newfs != NULL) {
			free(md->newfs);
			md->newfs = NULL;
		}

		if (newfs != NULL && newfs[0] != '\0') {
			md->newfs = malloc(strlen(newfs) + strlen(" /dev/") +
			    strlen(name) + 1);
			sprintf(md->newfs, "%s /dev/%s", newfs, name);
		}
	}

	if (strcmp(type, "freebsd-swap") == 0)
		mountpoint = "none";
	if (strcmp(type, "freebsd-boot") == 0)
		md->bootcode = 1;

	/* VTOC8 needs partcode in UFS partitions */
	if (strcmp(scheme, "VTOC8") == 0 && strcmp(type, "freebsd-ufs") == 0)
		md->bootcode = 1;

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
		md->fstab->fs_spec = malloc(strlen(name) + 6);
		sprintf(md->fstab->fs_spec, "/dev/%s", name);
		md->fstab->fs_file = strdup(mountpoint);
		/* Get VFS from text after freebsd-, if possible */
		if (strncmp("freebsd-", type, 8) == 0)
			md->fstab->fs_vfstype = strdup(&type[8]);
		else if (strcmp("fat32", type) == 0 || strcmp("efi", type) == 0)
			md->fstab->fs_vfstype = strdup("msdosfs");
		else
			md->fstab->fs_vfstype = strdup(type); /* Guess */
		if (strcmp(type, "freebsd-swap") == 0) {
			md->fstab->fs_type = strdup(FSTAB_SW);
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
		maxsize = end - lastend - 1;
		maxstart = lastend + 1;
	}

	pp = LIST_FIRST(&geom->lg_consumer)->lg_provider;

	/* Compute beginning of new partition and maximum available space */
	if (pp->lg_stripesize > 0 &&
	    (maxstart*pp->lg_sectorsize % pp->lg_stripesize) != 0) {
		intmax_t offset = (pp->lg_stripesize -
		    ((maxstart*pp->lg_sectorsize) % pp->lg_stripesize)) /
		    pp->lg_sectorsize;
		maxstart += offset;
		maxsize -= offset;
	}

	if (npartstart != NULL)
		*npartstart = maxstart;

	return (maxsize);
}

void
gpart_create(struct gprovider *pp, char *default_type, char *default_size,
     char *default_mountpoint, char **partname, int interactive)
{
	struct gctl_req *r;
	struct gconfig *gc;
	struct gconsumer *cp;
	struct ggeom *geom;
	const char *errstr, *scheme;
	char sizestr[32], startstr[32], output[64];
	char newfs[64], options_fstype[64];
	intmax_t maxsize, size, sector, firstfree, stripe;
	uint64_t bytes;
	int nitems, choice, junk;
	unsigned i;

	DIALOG_FORMITEM items[] = {
		{0, "Type:", 5, 0, 0, FALSE, "freebsd-ufs", 11, 0, 12, 15, 0,
		    FALSE, "Filesystem type (e.g. freebsd-ufs, freebsd-swap)",
		    FALSE},
		{0, "Size:", 5, 1, 0, FALSE, "", 11, 1, 12, 15, 0,
		    FALSE, "Partition size. Append K, M, G for kilobytes, "
		    "megabytes or gigabytes.", FALSE},
		{0, "Mountpoint:", 11, 2, 0, FALSE, "", 11, 2, 12, 15, 0,
		    FALSE, "Path at which to mount partition (blank for "
		    "swap, / for root filesystem)", FALSE},
		{0, "Label:", 7, 3, 0, FALSE, "", 11, 3, 12, 15, 0, FALSE,
		    "Partition name. Not all partition schemes support this.",
		    FALSE},
	};

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
		if (gpart_partition(pp->lg_name, NULL) == 0)
			dialog_msgbox("",
			    "The partition table has been successfully created."
			    " Please press Create again to create partitions.",
			    0, 0, TRUE);

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
		dialog_msgbox("Error", "No free space left on device.", 0, 0,
		    TRUE);
		return;
	}

	humanize_number(sizestr, 7, size*sector, "B", HN_AUTOSCALE,
	    HN_NOSPACE | HN_DECIMAL);
	items[1].text = sizestr;

	/* Special-case the MBR default type for nested partitions */
	if (strcmp(scheme, "MBR") == 0 || strcmp(scheme, "PC98") == 0) {
		items[0].text = "freebsd";
		items[0].help = "Filesystem type (e.g. freebsd, fat32)";
	}

	nitems = scheme_supports_labels(scheme) ? 4 : 3;

	if (default_type != NULL)
		items[0].text = default_type;
	if (default_size != NULL)
		items[1].text = default_size;
	if (default_mountpoint != NULL)
		items[2].text = default_mountpoint;

	/* Default options */
	strncpy(options_fstype, items[0].text,
	    sizeof(options_fstype));
	newfs_command(options_fstype, newfs, 1);
addpartform:
	if (interactive) {
		dialog_vars.extra_label = "Options";
		dialog_vars.extra_button = TRUE;
		choice = dlg_form("Add Partition", "", 0, 0, 0, nitems,
		    items, &junk);
		dialog_vars.extra_button = FALSE;
		switch (choice) {
		case 0: /* OK */
			break;
		case 1: /* Cancel */
			return;
		case 3: /* Options */
			strncpy(options_fstype, items[0].text,
			    sizeof(options_fstype));
			newfs_command(options_fstype, newfs, 0);
			goto addpartform;
		}
	}

	/*
	 * If the user changed the fs type after specifying options, undo
	 * their choices in favor of the new filesystem's defaults.
	 */
	if (strcmp(options_fstype, items[0].text) != 0) {
		strncpy(options_fstype, items[0].text, sizeof(options_fstype));
		newfs_command(options_fstype, newfs, 1);
	}

	size = maxsize;
	if (strlen(items[1].text) > 0) {
		if (expand_number(items[1].text, &bytes) != 0) {
			char error[512];

			sprintf(error, "Invalid size: %s\n", strerror(errno));
			dialog_msgbox("Error", error, 0, 0, TRUE);
			goto addpartform;
		}
		size = MIN((intmax_t)(bytes/sector), maxsize);
	}

	/* Check if the label has a / in it */
	if (strchr(items[3].text, '/') != NULL) {
		dialog_msgbox("Error", "Label contains a /, which is not an "
		    "allowed character.", 0, 0, TRUE);
		goto addpartform;
	}

	/* Warn if no mountpoint set */
	if (strcmp(items[0].text, "freebsd-ufs") == 0 &&
	    items[2].text[0] != '/') {
		dialog_vars.defaultno = TRUE;
		choice = dialog_yesno("Warning",
		    "This partition does not have a valid mountpoint "
		    "(for the partition from which you intend to boot the "
		    "operating system, the mountpoint should be /). Are you "
		    "sure you want to continue?"
		, 0, 0);
		dialog_vars.defaultno = FALSE;
		if (choice == 1) /* cancel */
			goto addpartform;
	}

	/*
	 * Error if this scheme needs nested partitions, this is one, and
	 * a mountpoint was set.
	 */
	if (strcmp(items[0].text, "freebsd") == 0 &&
	    strlen(items[2].text) > 0) {
		dialog_msgbox("Error", "Partitions of type \"freebsd\" are "
		    "nested BSD-type partition schemes and cannot have "
		    "mountpoints. After creating one, select it and press "
		    "Create again to add the actual file systems.", 0, 0, TRUE);
		goto addpartform;
	}

	/* If this is the root partition, check that this scheme is bootable */
	if (strcmp(items[2].text, "/") == 0 && !is_scheme_bootable(scheme)) {
		char message[512];
		sprintf(message, "This partition scheme (%s) is not bootable "
		    "on this platform. Are you sure you want to proceed?",
		    scheme);
		dialog_vars.defaultno = TRUE;
		choice = dialog_yesno("Warning", message, 0, 0);
		dialog_vars.defaultno = FALSE;
		if (choice == 1) /* cancel */
			goto addpartform;
	}

	/*
	 * If this is the root partition, and we need a boot partition, ask
	 * the user to add one.
	 */

	/* Check for existing freebsd-boot partition */
	LIST_FOREACH(pp, &geom->lg_provider, lg_provider) {
		struct partition_metadata *md;
		md = get_part_metadata(pp->lg_name, 0);
		if (md == NULL || !md->bootcode)
			continue;
		LIST_FOREACH(gc, &pp->lg_config, lg_config)
			if (strcmp(gc->lg_name, "type") == 0)
				break;
		if (gc != NULL && strcmp(gc->lg_val, "freebsd-boot") == 0)
			break;
	}

	/* If there isn't one, and we need one, ask */
	if (strcmp(items[2].text, "/") == 0 && bootpart_size(scheme) > 0 &&
	    pp == NULL) {
		if (interactive)
			choice = dialog_yesno("Boot Partition",
			    "This partition scheme requires a boot partition "
			    "for the disk to be bootable. Would you like to "
			    "make one now?", 0, 0);
		else
			choice = 0;

		if (choice == 0) { /* yes */
			r = gctl_get_handle();
			gctl_ro_param(r, "class", -1, "PART");
			gctl_ro_param(r, "arg0", -1, geom->lg_name);
			gctl_ro_param(r, "flags", -1, GPART_FLAGS);
			gctl_ro_param(r, "verb", -1, "add");
			gctl_ro_param(r, "type", -1, "freebsd-boot");
			snprintf(sizestr, sizeof(sizestr), "%jd",
			    bootpart_size(scheme) / sector);
			gctl_ro_param(r, "size", -1, sizestr);
			snprintf(startstr, sizeof(startstr), "%jd", firstfree);
			gctl_ro_param(r, "start", -1, startstr);
			gctl_rw_param(r, "output", sizeof(output), output);
			errstr = gctl_issue(r);
			if (errstr != NULL && errstr[0] != '\0') 
				gpart_show_error("Error", NULL, errstr);
			gctl_free(r);

			get_part_metadata(strtok(output, " "), 1)->bootcode = 1;

			/* Now adjust the part we are really adding forward */
			firstfree += bootpart_size(scheme) / sector;
			size -= (bootpart_size(scheme) + stripe)/sector;
			if (stripe > 0 && (firstfree*sector % stripe) != 0) 
				firstfree += (stripe - ((firstfree*sector) %
				    stripe)) / sector;
		}
	}
	
	r = gctl_get_handle();
	gctl_ro_param(r, "class", -1, "PART");
	gctl_ro_param(r, "arg0", -1, geom->lg_name);
	gctl_ro_param(r, "flags", -1, GPART_FLAGS);
	gctl_ro_param(r, "verb", -1, "add");

	gctl_ro_param(r, "type", -1, items[0].text);
	snprintf(sizestr, sizeof(sizestr), "%jd", size);
	gctl_ro_param(r, "size", -1, sizestr);
	snprintf(startstr, sizeof(startstr), "%jd", firstfree);
	gctl_ro_param(r, "start", -1, startstr);
	if (items[3].text[0] != '\0')
		gctl_ro_param(r, "label", -1, items[3].text);
	gctl_rw_param(r, "output", sizeof(output), output);

	errstr = gctl_issue(r);
	if (errstr != NULL && errstr[0] != '\0') {
		gpart_show_error("Error", NULL, errstr);
		gctl_free(r);
		goto addpartform;
	}

	if (strcmp(items[0].text, "freebsd-boot") == 0)
		get_part_metadata(strtok(output, " "), 1)->bootcode = 1;
	else if (strcmp(items[0].text, "freebsd") == 0)
		gpart_partition(strtok(output, " "), "BSD");
	else
		set_default_part_metadata(strtok(output, " "), scheme,
		    items[0].text, items[2].text, newfs);

	for (i = 0; i < (sizeof(items) / sizeof(items[0])); i++)
		if (items[i].text_free)
			free(items[i].text);
	gctl_free(r);

	if (partname != NULL)
		*partname = strdup(strtok(output, " "));
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
		if (geom == NULL)
			dialog_msgbox("Error",
			    "Only partitions can be deleted.", 0, 0, TRUE);
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
	const char *errstr;
	const char *modified;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, "PART") == 0)
			break;
	}

	if (strcmp(classp->lg_name, "PART") != 0) {
		dialog_msgbox("Error", "gpart not found!", 0, 0, TRUE);
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

		errstr = gctl_issue(r);
		if (errstr != NULL && errstr[0] != '\0') 
			gpart_show_error("Error", NULL, errstr);
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

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, "PART") == 0)
			break;
	}

	if (strcmp(classp->lg_name, "PART") != 0) {
		dialog_msgbox("Error", "gpart not found!", 0, 0, TRUE);
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
				gpart_partcode(pp);
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

