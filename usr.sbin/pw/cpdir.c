/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/param.h>
#include <errno.h>

#include "pwupd.h"

void
copymkdir(char const * dir, char const * skel, mode_t mode, uid_t uid, gid_t gid)
{
	int             rc = 0;
	char            src[MAXPATHLEN];
	char            dst[MAXPATHLEN];

	if (mkdir(dir, mode) != 0 && errno != EEXIST) {
		sprintf(src, "mkdir(%s)", dir);
		perror(src);
	} else {
		int             infd, outfd;
		struct stat     st;

		static char     counter = 0;
		static char    *copybuf = NULL;

		++counter;
		chown(dir, uid, gid);
		if (skel == NULL || *skel == '\0')
			rc = 1;
		else {
			DIR            *d = opendir(skel);

			if (d != NULL) {
				struct dirent  *e;

				while ((e = readdir(d)) != NULL) {
					char           *p = e->d_name;

					sprintf(src, "%s/%s", skel, p);
					if (stat(src, &st) == 0) {
						if (strncmp(p, "dot.", 4) == 0)	/* Conversion */
							p += 3;
						sprintf(dst, "%s/%s", dir, p);
						if (S_ISDIR(st.st_mode)) {	/* Recurse for this */
							if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
								copymkdir(dst, src, (st.st_mode & 0777), uid, gid);
							/*
							 * Note: don't propogate 'special' attributes
							 */
						} else if (S_ISREG(st.st_mode) && (outfd = open(dst, O_RDWR | O_CREAT | O_EXCL, st.st_mode)) != -1) {
							if ((infd = open(src, O_RDONLY)) == -1) {
								close(outfd);
								remove(dst);
							} else {
								int             b;

								/*
								 * Allocate our copy buffer if we need to
								 */
								if (copybuf == NULL)
									copybuf = malloc(4096);
								while ((b = read(infd, copybuf, 4096)) > 0)
									write(outfd, copybuf, b);
								close(infd);
								close(outfd);
								chown(dst, uid, gid);
							}
						}
					}
				}
				closedir(d);
			}
		}
		if (--counter == 0 && copybuf != NULL) {
			free(copybuf);
			copybuf = NULL;
		}
	}
}

