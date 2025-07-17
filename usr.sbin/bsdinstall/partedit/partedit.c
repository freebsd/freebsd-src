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

#include <bsddialog.h>
#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <inttypes.h>
#include <libgeom.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "diskmenu.h"
#include "partedit.h"

struct pmetadata_head part_metadata;
static int sade_mode = 0;

static int apply_changes(struct gmesh *mesh);
static void apply_workaround(struct gmesh *mesh);
static struct partedit_item *read_geom_mesh(struct gmesh *mesh, int *nitems);
static void add_geom_children(struct ggeom *gp, int recurse,
    struct partedit_item **items, int *nitems);
static void init_fstab_metadata(void);
static void get_mount_points(struct partedit_item *items, int nitems);
static int validate_setup(void);

static void
sigint_handler(int sig)
{
	struct gmesh mesh;

	/* Revert all changes and exit dialog-mode cleanly on SIGINT */
	if (geom_gettree(&mesh) == 0) {
		gpart_revert_all(&mesh);
		geom_deletetree(&mesh);
	}

	bsddialog_end();

	exit(1);
}

int
main(int argc, const char **argv)
{
	struct partition_metadata *md;
	const char *progname, *prompt;
	struct partedit_item *items = NULL;
	struct gmesh mesh;
	int i, op, nitems;
	int error;
	struct bsddialog_conf conf;

	progname = getprogname();
	if (strcmp(progname, "sade") == 0)
		sade_mode = 1;

	TAILQ_INIT(&part_metadata);

	init_fstab_metadata();

	if (bsddialog_init() == BSDDIALOG_ERROR)
		err(1, "%s", bsddialog_geterror());
	bsddialog_initconf(&conf);
	if (!sade_mode)
		bsddialog_backtitle(&conf, OSNAME " Installer");
	i = 0;

	/* Revert changes on SIGINT */
	signal(SIGINT, sigint_handler);

	if (strcmp(progname, "autopart") == 0) { /* Guided */
		prompt = "Please review the disk setup. When complete, press "
		    "the Finish button.";
		/* Experimental ZFS autopartition support */
		if (argc > 1 && strcmp(argv[1], "zfs") == 0) {
			part_wizard("zfs");
		} else {
			part_wizard("ufs");
		}
	} else if (strcmp(progname, "scriptedpart") == 0) {
		error = scripted_editor(argc, argv);
		prompt = NULL;
		if (error != 0) {
			bsddialog_end();
			return (error);
		}
	} else {
		prompt = "Create partitions for " OSNAME ", F1 for help.\n"
		    "No changes will be made until you select Finish.";
	}

	/* Show the part editor either immediately, or to confirm wizard */
	while (prompt != NULL) {
		bsddialog_clear(0);
		if (!sade_mode)
			bsddialog_backtitle(&conf, OSNAME " Installer");

		error = geom_gettree(&mesh);
		if (error == 0)
			items = read_geom_mesh(&mesh, &nitems);
		if (error || items == NULL) {
			conf.title = "Error";
			bsddialog_msgbox(&conf, "No disks found. If you need "
			    "to install a kernel driver, choose Shell at the "
			    "installation menu.", 0, 0);
			break;
		}

		get_mount_points(items, nitems);

		if (i >= nitems)
			i = nitems - 1;
		op = diskmenu_show("Partition Editor", prompt, items, nitems,
		    &i);

		switch (op) {
		case BUTTON_CREATE:
			gpart_create((struct gprovider *)(items[i].cookie),
			    NULL, NULL, NULL, NULL, 1);
			break;
		case BUTTON_DELETE:
			gpart_delete((struct gprovider *)(items[i].cookie));
			break;
		case BUTTON_MODIFY:
			gpart_edit((struct gprovider *)(items[i].cookie));
			break;
		case BUTTON_REVERT:
			gpart_revert_all(&mesh);
			while ((md = TAILQ_FIRST(&part_metadata)) != NULL) {
				if (md->fstab != NULL) {
					free(md->fstab->fs_spec);
					free(md->fstab->fs_file);
					free(md->fstab->fs_vfstype);
					free(md->fstab->fs_mntops);
					free(md->fstab->fs_type);
					free(md->fstab);
				}
				if (md->newfs != NULL)
					free(md->newfs);
				free(md->name);

				TAILQ_REMOVE(&part_metadata, md, metadata);
				free(md);
			}
			init_fstab_metadata();
			break;
		case BUTTON_AUTO:
			part_wizard("ufs");
			break;
		}

		error = 0;
		if (op == BUTTON_FINISH) {
			conf.button.ok_label = "Commit";
			conf.button.with_extra = true;
			conf.button.extra_label = "Revert & Exit";
			conf.button.cancel_label = "Back";
			conf.title = "Confirmation";
			op = bsddialog_yesno(&conf, "Your changes will now be "
			    "written to disk. If you have chosen to overwrite "
			    "existing data, it will be PERMANENTLY ERASED. Are "
			    "you sure you want to commit your changes?", 0, 0);
			conf.button.ok_label = NULL;
			conf.button.with_extra = false;
			conf.button.extra_label = NULL;
			conf.button.cancel_label = NULL;

			if (op == BSDDIALOG_OK && validate_setup()) { /* Save */
				error = apply_changes(&mesh);
				if (!error)
					apply_workaround(&mesh);
				break;
			} else if (op == BSDDIALOG_EXTRA) { /* Quit */
				gpart_revert_all(&mesh);
				error =	-1;
				break;
			}
		}

		geom_deletetree(&mesh);
		free(items);
	}
	
	if (prompt == NULL) {
		error = geom_gettree(&mesh);
		if (error == 0) {
			if (validate_setup()) {
				error = apply_changes(&mesh);
			} else {
				gpart_revert_all(&mesh);
				error = -1;
			}
			geom_deletetree(&mesh);
		}
	}

	bsddialog_end();

	return (error);
}

struct partition_metadata *
get_part_metadata(const char *name, int create)
{
	struct partition_metadata *md;

	TAILQ_FOREACH(md, &part_metadata, metadata) 
		if (md->name != NULL && strcmp(md->name, name) == 0)
			break;

	if (md == NULL && create) {
		md = calloc(1, sizeof(*md));
		md->name = strdup(name);
		TAILQ_INSERT_TAIL(&part_metadata, md, metadata);
	}

	return (md);
}
	
void
delete_part_metadata(const char *name)
{
	struct partition_metadata *md;

	TAILQ_FOREACH(md, &part_metadata, metadata) {
		if (md->name != NULL && strcmp(md->name, name) == 0) {
			if (md->fstab != NULL) {
				free(md->fstab->fs_spec);
				free(md->fstab->fs_file);
				free(md->fstab->fs_vfstype);
				free(md->fstab->fs_mntops);
				free(md->fstab->fs_type);
				free(md->fstab);
			}
			if (md->newfs != NULL)
				free(md->newfs);
			free(md->name);

			TAILQ_REMOVE(&part_metadata, md, metadata);
			free(md);
			break;
		}
	}
}

static int
validate_setup(void)
{
	struct partition_metadata *md, *root = NULL;
	int button;
	struct bsddialog_conf conf;

	TAILQ_FOREACH(md, &part_metadata, metadata) {
		if (md->fstab != NULL && strcmp(md->fstab->fs_file, "/") == 0)
			root = md;

		/* XXX: Check for duplicate mountpoints */
	}

	bsddialog_initconf(&conf);

	if (root == NULL) {
		conf.title = "Error";
		bsddialog_msgbox(&conf, "No root partition was found. "
		    "The root " OSNAME " partition must have a mountpoint "
		    "of '/'.", 0, 0);
		return (false);
	}

	/*
	 * Check for root partitions that we aren't formatting, which is 
	 * usually a mistake
	 */
	if (root->newfs == NULL && !sade_mode) {
		conf.button.default_cancel = true;
		conf.title = "Warning";
		button = bsddialog_yesno(&conf, "The chosen root partition "
		    "has a preexisting filesystem. If it contains an existing "
		    OSNAME " system, please update it with freebsd-update "
		    "instead of installing a new system on it. The partition "
		    "can also be erased by pressing \"No\" and then deleting "
		    "and recreating it. Are you sure you want to proceed?",
		    0, 0);
		if (button == BSDDIALOG_CANCEL)
			return (false);
	}

	return (true);
}

static int
mountpoint_sorter(const void *xa, const void *xb)
{
	struct partition_metadata *a = *(struct partition_metadata **)xa;
	struct partition_metadata *b = *(struct partition_metadata **)xb;

	if (a->fstab == NULL && b->fstab == NULL)
		return 0;
	if (a->fstab == NULL)
		return 1;
	if (b->fstab == NULL)
		return -1;

	return strcmp(a->fstab->fs_file, b->fstab->fs_file);
}

static int
apply_changes(struct gmesh *mesh)
{
	struct partition_metadata *md;
	char message[512];
	int i, nitems, error, *miniperc;
	const char **minilabel;
	const char *fstab_path;
	FILE *fstab;
	char *command;
	struct bsddialog_conf conf;

	nitems = 1; /* Partition table changes */
	TAILQ_FOREACH(md, &part_metadata, metadata) {
		if (md->newfs != NULL)
			nitems++;
	}
	minilabel = calloc(nitems, sizeof(const char *));
	miniperc  = calloc(nitems, sizeof(int));
	minilabel[0] = "Writing partition tables";
	miniperc[0]  = BSDDIALOG_MG_INPROGRESS;
	i = 1;
	TAILQ_FOREACH(md, &part_metadata, metadata) {
		if (md->newfs != NULL) {
			char *item;

			asprintf(&item, "Initializing %s", md->name);
			minilabel[i] = item;
			miniperc[i]  = BSDDIALOG_MG_PENDING;
			i++;
		}
	}

	i = 0;
	bsddialog_initconf(&conf);
	conf.title = "Initializing";
	bsddialog_mixedgauge(&conf,
	    "Initializing file systems. Please wait.", 0, 0, i * 100 / nitems,
	    nitems, minilabel, miniperc);
	gpart_commit(mesh);
	miniperc[i] = BSDDIALOG_MG_COMPLETED;
	i++;

	if (getenv("BSDINSTALL_LOG") == NULL) 
		setenv("BSDINSTALL_LOG", "/dev/null", 1);

	TAILQ_FOREACH(md, &part_metadata, metadata) {
		if (md->newfs != NULL) {
			miniperc[i] = BSDDIALOG_MG_INPROGRESS;
			bsddialog_mixedgauge(&conf,
			    "Initializing file systems. Please wait.", 0, 0,
			    i * 100 / nitems, nitems, minilabel, miniperc);
			asprintf(&command, "(echo %s; %s) >>%s 2>>%s",
			    md->newfs, md->newfs, getenv("BSDINSTALL_LOG"),
			    getenv("BSDINSTALL_LOG"));
			error = system(command);
			free(command);
			miniperc[i] = (error == 0) ?
			    BSDDIALOG_MG_COMPLETED : BSDDIALOG_MG_FAILED;
			i++;
		}
	}
	bsddialog_mixedgauge(&conf, "Initializing file systems. Please wait.",
	    0, 0, i * 100 / nitems, nitems, minilabel, miniperc);

	for (i = 1; i < nitems; i++)
		free(__DECONST(char *, minilabel[i]));

	free(minilabel);
	free(miniperc);

	/* Sort filesystems for fstab so that mountpoints are ordered */
	{
		struct partition_metadata **tobesorted;
		struct partition_metadata *tmp;
		int nparts = 0;
		TAILQ_FOREACH(md, &part_metadata, metadata)
			nparts++;
		tobesorted = malloc(sizeof(struct partition_metadata *)*nparts);
		nparts = 0;
		TAILQ_FOREACH_SAFE(md, &part_metadata, metadata, tmp) {
			tobesorted[nparts++] = md;
			TAILQ_REMOVE(&part_metadata, md, metadata);
		}
		qsort(tobesorted, nparts, sizeof(tobesorted[0]),
		    mountpoint_sorter);

		/* Now re-add everything */
		while (nparts-- > 0)
			TAILQ_INSERT_HEAD(&part_metadata,
			    tobesorted[nparts], metadata);
		free(tobesorted);
	}

	if (getenv("PATH_FSTAB") != NULL)
		fstab_path = getenv("PATH_FSTAB");
	else
		fstab_path = "/etc/fstab";
	fstab = fopen(fstab_path, "w+");
	if (fstab == NULL) {
		snprintf(message, sizeof(message),
		    "Cannot open fstab file %s for writing (%s)\n",
		    getenv("PATH_FSTAB"), strerror(errno));
		conf.title = "Error";
		bsddialog_msgbox(&conf, message, 0, 0);
		return (-1);
	}
	fprintf(fstab, "# Device\tMountpoint\tFStype\tOptions\tDump\tPass#\n");
	TAILQ_FOREACH(md, &part_metadata, metadata) {
		if (md->fstab != NULL)
			fprintf(fstab, "%s\t%s\t\t%s\t%s\t%d\t%d\n",
			    md->fstab->fs_spec, md->fstab->fs_file,
			    md->fstab->fs_vfstype, md->fstab->fs_mntops,
			    md->fstab->fs_freq, md->fstab->fs_passno);
	}
	fclose(fstab);

	return (0);
}

static void
apply_workaround(struct gmesh *mesh)
{
	struct gclass *classp;
	struct ggeom *gp;
	struct gconfig *gc;
	const char *scheme = NULL, *modified = NULL;
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
		LIST_FOREACH(gc, &gp->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "scheme") == 0) {
				scheme = gc->lg_val;
			} else if (strcmp(gc->lg_name, "modified") == 0) {
				modified = gc->lg_val;
			}
		}

		if (scheme && strcmp(scheme, "GPT") == 0 &&
		    modified && strcmp(modified, "true") == 0) {
			if (getenv("WORKAROUND_LENOVO"))
				gpart_set_root(gp->lg_name, "lenovofix");
			if (getenv("WORKAROUND_GPTACTIVE"))
				gpart_set_root(gp->lg_name, "active");
		}
	}
}

static struct partedit_item *
read_geom_mesh(struct gmesh *mesh, int *nitems)
{
	struct gclass *classp;
	struct ggeom *gp;
	struct partedit_item *items;

	*nitems = 0;
	items = NULL;

	/*
	 * Build the device table. First add all disks (and CDs).
	 */
	
	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, "DISK") != 0 &&
		    strcmp(classp->lg_name, "MD") != 0)
			continue;

		/* Now recurse into all children */
		LIST_FOREACH(gp, &classp->lg_geom, lg_geom) 
			add_geom_children(gp, 0, &items, nitems);
	}

	return (items);
}

static void
add_geom_children(struct ggeom *gp, int recurse, struct partedit_item **items,
    int *nitems)
{
	struct gconsumer *cp;
	struct gprovider *pp;
	struct gconfig *gc;

	if (strcmp(gp->lg_class->lg_name, "PART") == 0 &&
	    !LIST_EMPTY(&gp->lg_config)) {
		LIST_FOREACH(gc, &gp->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "scheme") == 0)
				(*items)[*nitems-1].type = gc->lg_val;
		}
	}

	if (LIST_EMPTY(&gp->lg_provider)) 
		return;

	LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
		if (strcmp(gp->lg_class->lg_name, "LABEL") == 0)
			continue;

		/* Skip WORM media */
		if (strncmp(pp->lg_name, "cd", 2) == 0)
			continue;

		*items = realloc(*items,
		    (*nitems+1)*sizeof(struct partedit_item));
		(*items)[*nitems].indentation = recurse;
		(*items)[*nitems].name = pp->lg_name;
		(*items)[*nitems].size = pp->lg_mediasize;
		(*items)[*nitems].mountpoint = NULL;
		(*items)[*nitems].type = "";
		(*items)[*nitems].cookie = pp;

		LIST_FOREACH(gc, &pp->lg_config, lg_config) {
			if (strcmp(gc->lg_name, "type") == 0)
				(*items)[*nitems].type = gc->lg_val;
		}

		/* Skip swap-backed MD devices */
		if (strcmp(gp->lg_class->lg_name, "MD") == 0 &&
		    strcmp((*items)[*nitems].type, "swap") == 0)
			continue;

		(*nitems)++;

		LIST_FOREACH(cp, &pp->lg_consumers, lg_consumers)
			add_geom_children(cp->lg_geom, recurse+1, items,
			    nitems);

		/* Only use first provider for acd */
		if (strcmp(gp->lg_class->lg_name, "ACD") == 0)
			break;
	}
}

static void
init_fstab_metadata(void)
{
	struct fstab *fstab;
	struct partition_metadata *md;

	setfsent();
	while ((fstab = getfsent()) != NULL) {
		md = calloc(1, sizeof(struct partition_metadata));

		md->name = NULL;
		if (strncmp(fstab->fs_spec, "/dev/", 5) == 0)
			md->name = strdup(&fstab->fs_spec[5]);

		md->fstab = malloc(sizeof(struct fstab));
		md->fstab->fs_spec = strdup(fstab->fs_spec);
		md->fstab->fs_file = strdup(fstab->fs_file);
		md->fstab->fs_vfstype = strdup(fstab->fs_vfstype);
		md->fstab->fs_mntops = strdup(fstab->fs_mntops);
		md->fstab->fs_type = strdup(fstab->fs_type);
		md->fstab->fs_freq = fstab->fs_freq;
		md->fstab->fs_passno = fstab->fs_passno;

		md->newfs = NULL;
		
		TAILQ_INSERT_TAIL(&part_metadata, md, metadata);
	}
}

static void
get_mount_points(struct partedit_item *items, int nitems)
{
	struct partition_metadata *md;
	int i;
	
	for (i = 0; i < nitems; i++) {
		TAILQ_FOREACH(md, &part_metadata, metadata) {
			if (md->name != NULL && md->fstab != NULL &&
			    strcmp(md->name, items[i].name) == 0) {
				items[i].mountpoint = md->fstab->fs_file;
				break;
			}
		}
	}
}
