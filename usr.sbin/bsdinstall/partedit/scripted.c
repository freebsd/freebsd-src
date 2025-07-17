/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Nathan Whitehorn
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

#include <ctype.h>
#include <libgeom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "partedit.h"

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

static int
part_config(char *disk, const char *scheme, char *config)
{
	char *partition, *ap, *size = NULL, *type = NULL, *mount = NULL;
	struct gclass *classp;
	struct gmesh mesh;
	struct ggeom *gpart = NULL;
	int error;

	if (scheme == NULL)
		scheme = default_scheme();

	error = geom_gettree(&mesh);
	if (error != 0)
		return (-1);
	if (provider_for_name(&mesh, disk) == NULL) {
		fprintf(stderr, "GEOM provider %s not found\n", disk);
		geom_deletetree(&mesh);
		return (-1);
	}

	/* Remove any existing partitioning and create new scheme */
	LIST_FOREACH(classp, &mesh.lg_class, lg_class)
		if (strcmp(classp->lg_name, "PART") == 0)
			break;
	if (classp != NULL) {
		LIST_FOREACH(gpart, &classp->lg_geom, lg_geom)
		if (strcmp(gpart->lg_name, disk) == 0)
			break;
	}
	if (gpart != NULL)
		gpart_destroy(gpart);
	gpart_partition(disk, scheme);

	if (strcmp(scheme, "MBR") == 0) {
		struct gmesh submesh;

		if (geom_gettree(&submesh) == 0) {
			gpart_create(provider_for_name(&submesh, disk),
			    "freebsd", NULL, NULL, &disk, 0);
			geom_deletetree(&submesh);
		}
	} else {
		disk = strdup(disk);
	}

	geom_deletetree(&mesh);
	error = geom_gettree(&mesh);
	if (error != 0) {
		free(disk);
		return (-1);
	}

	/* Create partitions */
	if (config == NULL) {
		wizard_makeparts(&mesh, disk, "ufs", 0);
		goto finished;
	}

	while ((partition = strsep(&config, ",")) != NULL) {
		while ((ap = strsep(&partition, " \t\n")) != NULL) {
			if (*ap == '\0')
				continue;
			if (size == NULL)
				size = ap;
			else if (type == NULL)
				type = ap;
			else if (mount == NULL)
				mount = ap;
		}
		if (size == NULL)
			continue;
		if (strcmp(size, "auto") == 0)
			size = NULL;
		gpart_create(provider_for_name(&mesh, disk), type, size, mount,
		    NULL, 0);
		geom_deletetree(&mesh);
		error = geom_gettree(&mesh);
		if (error != 0) {
			free(disk);
			return (-1);
		}
		size = type = mount = NULL;
	}

finished:
	geom_deletetree(&mesh);
	free(disk);

	return (0);
}

static int
parse_disk_config(char *input)
{
	char *ap;
	char *disk = NULL, *scheme = NULL, *partconfig = NULL;

	while (input != NULL && *input != 0) {
		if (isspace(*input)) {
			input++;
			continue;
		}

		switch(*input) {
		case '{':
			input++;
			partconfig = strchr(input, '}');
			if (partconfig == NULL) {
				fprintf(stderr, "Malformed partition setup "
				    "string: %s\n", input);
				return (1);
			}
			*partconfig = '\0';
			ap = partconfig+1;
			partconfig = input;
			input = ap;
			break;
		default:
			if (disk == NULL)
				disk = strsep(&input, " \t\n");
			else if (scheme == NULL)
				scheme = strsep(&input, " \t\n");
			else {
				fprintf(stderr, "Unknown directive: %s\n",
				    strsep(&input, " \t\n"));
				return (1);
			}
		}
	} while (input != NULL && *input != 0);

	if (disk == NULL || strcmp(disk, "DEFAULT") == 0) {
		struct gmesh mesh;

		if (geom_gettree(&mesh) == 0) {
			disk = boot_disk_select(&mesh);
			geom_deletetree(&mesh);
		}
	}

	return (part_config(disk, scheme, partconfig));
}

int
scripted_editor(int argc, const char **argv)
{
	FILE *fp;
	char *input, *token;
	size_t len;
	int i, error = 0;

	fp = open_memstream(&input, &len);
	fputs(argv[1], fp);
	for (i = 2; i < argc; i++) {
		fprintf(fp, " %s", argv[i]);
	}
	fclose(fp);

	while ((token = strsep(&input, ";")) != NULL) {
		error = parse_disk_config(token);
		if (error != 0) {
			free(input);
			return (error);
		}
	}
	free(input);

	return (0);
}

