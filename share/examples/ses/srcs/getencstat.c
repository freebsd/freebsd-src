/* $FreeBSD: src/share/examples/ses/srcs/getencstat.c,v 1.1 2000/02/29 05:44:17 mjacob Exp $ */
/*
 * Copyright (c) 2000 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * the GNU Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * Matthew Jacob
 * Feral Software
 * mjacob@feral.com
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include SESINC

extern char *geteltnm __P((int));
extern char *stat2ascii __P((int, u_char *));

int
main(a, v)
	int a;
	char **v;
{
	ses_object *objp;
	ses_objstat ob;
	int fd, nobj, f, i, verbose, quiet, errors;
	u_char estat;

	if (a < 2) {
		fprintf(stderr, "usage: %s [ -v ] device [ device ... ]\n", *v);
		return (1);
	}
	errors = quiet = verbose = 0;
	if (strcmp(v[1], "-V") == 0) {
		verbose = 2;
		v++;
	} else if (strcmp(v[1], "-v") == 0) {
		verbose = 1;
		v++;
	} else if (strcmp(v[1], "-q") == 0) {
		quiet = 1;
		verbose = 0;
		v++;
	}
	while (*++v) {
			
		fd = open(*v, O_RDONLY);
		if (fd < 0) {
			perror(*v);
			continue;
		}
		if (ioctl(fd, SESIOC_GETNOBJ, (caddr_t) &nobj) < 0) {
			perror("SESIOC_GETNOBJ");
			(void) close(fd);
			continue;
		}
		if (ioctl(fd, SESIOC_GETENCSTAT, (caddr_t) &estat) < 0) {
			perror("SESIOC_GETENCSTAT");
			(void) close(fd);
			continue;
		}
		if ((verbose == 0 || quiet == 1) && estat == 0) {
			if (quiet == 0)
				fprintf(stdout, "%s: Enclosure OK\n", *v);
			(void) close(fd);
			continue;
		}
		fprintf(stdout, "%s: Enclosure Status ", *v);
		if (estat == 0) {
			fprintf(stdout, "<OK");
		} else {
			errors++;
			f = '<';
			if (estat & SES_ENCSTAT_INFO) {
				fprintf(stdout, "%cINFO", f);
				f = ',';
			}
			if (estat & SES_ENCSTAT_NONCRITICAL) {
				fprintf(stdout, "%cNONCRITICAL", f);
				f = ',';
			}
			if (estat & SES_ENCSTAT_CRITICAL) {
				fprintf(stdout, "%cCRITICAL", f);
				f = ',';
			}
			if (estat & SES_ENCSTAT_UNRECOV) {
				fprintf(stdout, "%cUNRECOV", f);
				f = ',';
			}
		}
		fprintf(stdout, ">\n");
		objp = calloc(nobj, sizeof (ses_object));
		if (objp == NULL) {
			perror("calloc");
			(void) close(fd);
			continue;
		}
                if (ioctl(fd, SESIOC_GETOBJMAP, (caddr_t) objp) < 0) {
                        perror("SESIOC_GETOBJMAP");
                        (void) close(fd);
                        continue;
                }
		for (i = 0; i < nobj; i++) {
			ob.obj_id = objp[i].obj_id;
			if (ioctl(fd, SESIOC_GETOBJSTAT, (caddr_t) &ob) < 0) {
				perror("SESIOC_GETOBJSTAT");
				(void) close(fd);
				break;
			}
			if ((ob.cstat[0] & 0xf) == SES_OBJSTAT_OK) {
				if (verbose) {
					fprintf(stdout,
					    "Element 0x%x: %s OK (%s)\n",
					    ob.obj_id,
					    geteltnm(objp[i].object_type),
					    stat2ascii(objp[i].object_type,
					    ob.cstat));
				}
				continue;
			}
			fprintf(stdout, "Element 0x%x: %s, %s\n",
			    ob.obj_id, geteltnm(objp[i].object_type),
			    stat2ascii(objp[i].object_type, ob.cstat));
		}
		free(objp);
		(void) close(fd);
	}
	return (errors);
}
