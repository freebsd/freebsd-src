/*-
 * Copyright (c) 2000  "HOSOKAWA, Tatsumi" <hosokawa@FreeBSD.org>
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

#include "sysinstall.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <fcntl.h>
#include <dirent.h>

#define MODULESDIR "/stand/modules"

void
moduleInitialize(void)
{
    int fd, len;
    DIR *dirp;
    struct dirent *dp;
    char module[MAXPATHLEN], desc[MAXPATHLEN];
    char desc_str[BUFSIZ];

    if (!RunningAsInit && !Fake) {
	/* It's not my job... */
	return;
    }

    dirp = opendir(MODULESDIR);
    if (dirp) {
	while ((dp = readdir(dirp))) {
	    if (dp->d_namlen < (sizeof(".ko") - 1)) continue;
	    if (strcmp(dp->d_name + dp->d_namlen - (sizeof(".ko") - 1), ".ko") == 0) {
		strcpy(module, MODULESDIR);
		strcat(module, "/");
		strcat(module, dp->d_name);
		strcpy(desc, module);
		len = strlen(desc);
		strcpy(desc + (len - (sizeof(".ko") - 1)), ".dsc");
		fd = open(module, O_RDONLY);
		if (fd < 0) continue;
		close(fd);
		fd = open(desc, O_RDONLY);
		if (fd < 0) {
		    desc_str[0] = 0;
		}
		else {
		    len = read(fd, desc_str, BUFSIZ);
		    close(fd);
		    if (len < BUFSIZ) desc_str[len] = 0;
		}
		if (desc_str[0])
		    msgDebug("Loading module %s (%s)\n", dp->d_name, desc_str);
		else
		    msgDebug("Loading module %s\n", dp->d_name);
		if (kldload(module) < 0) {
		    if (desc_str[0])
			msgConfirm("Loading module %s failed\n%s", dp->d_name, desc_str);
		    else
			msgConfirm("Loading module %s failed", dp->d_name);
		}
	    }
	}
	closedir(dirp);
    }
}
