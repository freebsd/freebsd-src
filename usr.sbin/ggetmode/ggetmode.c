/*-
 * Copyright (c) 2011 Lev Serebryakov
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <sys/time.h>
#include <sysexits.h>
#include <paths.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <libgeom.h>

static char pathdev[] = _PATH_DEV;

int
main(int argc, char *argv[])
{
	struct gmesh mesh;
	struct gclass *mp;
	struct ggeom *gp;
	struct gprovider *pp;
	int error;
	char mode[16] = "";
	char *device;

	if (argc != 2)
		errx(EX_USAGE, "Syntax: %s <device name>", basename(argv[0]));

	device = argv[1];
	if (!strncmp(device, pathdev, sizeof(pathdev) - 1))
		device += sizeof(pathdev) - 1;

	error = geom_gettree(&mesh);
	if (error != 0)
		errc(EX_UNAVAILABLE, error, "Can not get GEOM configuration: ");

	LIST_FOREACH(mp, &mesh.lg_class, lg_class) {
		LIST_FOREACH(gp, &mp->lg_geom, lg_geom) {
			LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
				if (!strcmp(pp->lg_name, device)) {
					strlcpy(mode, pp->lg_mode, sizeof(mode) - 1);
					goto end;
				}
			}
		}
	}
end:
	geom_deletetree(&mesh);
	if (mode[0])
		printf("%s\n", mode);
	else
		errx(EX_DATAERR, "%s not found", argv[1]);

	return (EX_OK);
}
