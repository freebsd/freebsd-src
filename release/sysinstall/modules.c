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
#include <fcntl.h>
#include <fnmatch.h>

/* Prototypes */
static int		kldModuleFire(dialogMenuItem *self);

#define MODULESDIR "/modules"
#define DISTMOUNT "/dist"

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
		if (kldload(module) < 0 && errno != EEXIST) {
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

int
kldBrowser(dialogMenuItem *self)
{
    DMenu	*menu;
    int		i, what = DITEM_SUCCESS, msize, count;
    DIR		*dir;
    struct dirent *de;
    char	*err;
    
    err = NULL;
    
    if (DITEM_STATUS(mediaSetFloppy(NULL)) == DITEM_FAILURE) {
	msgConfirm("Unable to set media device to floppy.");
	what |= DITEM_FAILURE;
	mediaClose();
	return what;
    }

    if (!DEVICE_INIT(mediaDevice)) {
	msgConfirm("Unable to mount floppy filesystem.");
	what |= DITEM_FAILURE;
	mediaClose();
	return what;
    }

    msize = sizeof(DMenu) + (sizeof(dialogMenuItem) * 2);
    count = 0;
    if ((menu = malloc(msize)) == NULL) {
	err = "Failed to allocate memory for menu";
	goto errout;
    }

    bcopy(&MenuKLD, menu, sizeof(DMenu));
	
    bzero(&menu->items[count], sizeof(menu->items[0]));
    menu->items[count].prompt = strdup("X Exit");
    menu->items[count].title = strdup("Exit this menu (returning to previous)");
    menu->items[count].fire = dmenuExit;
    count++;
	
    if ((dir = opendir(DISTMOUNT)) == NULL) {
	err = "Couldn't open directory";
	goto errout;
    }
    
    while ((de = readdir(dir)) != NULL) {
	if (fnmatch("*.ko", de->d_name, FNM_CASEFOLD))
	    continue;
	
	msize += sizeof(dialogMenuItem);
	if ((menu = realloc(menu, msize)) == NULL) {
	    err = "Failed to allocate memory for menu item";
	    goto errout;
	}
	    
	bzero(&menu->items[count], sizeof(menu->items[0]));
	menu->items[count].fire = kldModuleFire;

	menu->items[count].prompt = strdup(de->d_name);
	menu->items[count].title = menu->items[count].prompt;
	    
	count++;
    }

    closedir(dir);

    menu->items[count].prompt = NULL;
    menu->items[count].title = NULL;
    
    dmenuOpenSimple(menu, FALSE);
    
    mediaClose();

    deviceRescan();
    
  errout:    
    for (i = 0; i < count; i++)
	free(menu->items[i].prompt);
    
    free(menu);

    if (err != NULL) {
	what |= DITEM_FAILURE;
	if (!variable_get(VAR_NO_ERROR))
	    msgConfirm(err);
    }
    
    return (what);
}

static int
kldModuleFire(dialogMenuItem *self) {
    char	fname[256];

    bzero(fname, sizeof(fname));
    snprintf(fname, sizeof(fname), "%s/%s", DISTMOUNT, self->prompt);

    if (kldload(fname) < 0 && errno != EEXIST) {
	if (!variable_get(VAR_NO_ERROR))
	    msgConfirm("Loading module %s failed\n", fname);
    } else {
	if (!variable_get(VAR_NO_ERROR))
	    msgConfirm("Loaded module %s OK", fname);
    }

    return(0);
 }
