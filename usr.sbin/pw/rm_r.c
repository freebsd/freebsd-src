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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <dirent.h>

#include "pwupd.h"

void
rm_r(char const * dir, uid_t uid)
{
	DIR            *d = opendir(dir);

	if (d != NULL) {
		struct dirent  *e;
		struct stat     st;
		char            file[MAXPATHLEN];

		while ((e = readdir(d)) != NULL) {
			if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0) {
				snprintf(file, sizeof(file), "%s/%s", dir, e->d_name);
				if (lstat(file, &st) == 0) {	/* Need symlinks, not
								 * linked file */
					if (S_ISDIR(st.st_mode))	/* Directory - recurse */
						rm_r(file, uid);
					else {
						if (S_ISLNK(st.st_mode) || st.st_uid == uid)
							remove(file);
					}
				}
			}
		}
		closedir(d);
		if (lstat(dir, &st) == 0) {
			if (S_ISLNK(st.st_mode))
				remove(dir);
			else if (st.st_uid == uid)
				rmdir(dir);
		}
	}
}
