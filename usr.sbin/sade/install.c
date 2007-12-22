/*
 * $FreeBSD$
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sade.h"
#include <ctype.h>
#include <sys/consio.h>
#include <sys/disklabel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/param.h>
#define MSDOSFS
#include <sys/mount.h>
#include <ufs/ufs/ufsmount.h>
#include <fs/msdosfs/msdosfsmount.h>
#undef MSDOSFS
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <libdisk.h>
#include <limits.h>
#include <unistd.h>
#include <termios.h>

#define TERMCAP_FILE	"/usr/share/misc/termcap"

Boolean
checkLabels(Boolean whinge)
{
    Boolean status;

    /* Don't allow whinging if noWarn is set */
    if (variable_get(VAR_NO_WARN))
	whinge = FALSE;

    status = TRUE;
    HomeChunk = RootChunk = SwapChunk = NULL;
    TmpChunk = UsrChunk = VarChunk = NULL;
#ifdef __ia64__
    EfiChunk = NULL;
#endif

    /* We don't need to worry about root/usr/swap if we're already multiuser */
    return status;
}

#define	QUEUE_YES	1
#define	QUEUE_NO	0
static int
performNewfs(PartInfo *pi, char *dname, int queue)
{
	char buffer[LINE_MAX];

	if (pi->do_newfs) {
		switch(pi->newfs_type) {
		case NEWFS_UFS:
			snprintf(buffer, LINE_MAX, "%s %s %s %s %s",
			    NEWFS_UFS_CMD,
			    pi->newfs_data.newfs_ufs.softupdates ?  "-U" : "",
			    pi->newfs_data.newfs_ufs.ufs1 ? "-O1" : "-O2",
			    pi->newfs_data.newfs_ufs.user_options,
			    dname);
			break;

		case NEWFS_MSDOS:
			snprintf(buffer, LINE_MAX, "%s %s", NEWFS_MSDOS_CMD,
			    dname);
			break;

		case NEWFS_CUSTOM:
			snprintf(buffer, LINE_MAX, "%s %s",
			    pi->newfs_data.newfs_custom.command, dname);
			break;
		}

		if (queue == QUEUE_YES) {
			command_shell_add(pi->mountpoint, buffer);
			return (0);
		} else
			return (vsystem(buffer));
	}
	return (0);
}

/* Go newfs and/or mount all the filesystems we've been asked to */
int
installFilesystems(dialogMenuItem *self)
{
    int i;
    Disk *disk;
    Chunk *c1, *c2;
    Device **devs;
    PartInfo *root;
    char dname[80];
    Boolean upgrade = FALSE;

    /* If we've already done this, bail out */
    if (!variable_cmp(DISK_LABELLED, "written"))
	return DITEM_SUCCESS;

    upgrade = !variable_cmp(SYSTEM_STATE, "upgrade");
    if (!checkLabels(TRUE))
	return DITEM_FAILURE;

    root = (RootChunk != NULL) ? (PartInfo *)RootChunk->private_data : NULL;

    command_clear();

    /* Now buzz through the rest of the partitions and mount them too */
    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;

	disk = (Disk *)devs[i]->private;
	if (!disk->chunks) {
	    msgConfirm("No chunk list found for %s!", disk->name);
	    return DITEM_FAILURE | DITEM_RESTORE;
	}
	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
#ifdef __ia64__
	if (c1->type == part) {
		c2 = c1;
		{
#elif defined(__powerpc__)
	    if (c1->type == apple) {
		for (c2 = c1->part; c2; c2 = c2->next) {
#else
	    if (c1->type == freebsd) {
		for (c2 = c1->part; c2; c2 = c2->next) {
#endif
		    if (c2->type == part && c2->subtype != FS_SWAP && c2->private_data) {
			PartInfo *tmp = (PartInfo *)c2->private_data;

			/* Already did root */
			if (c2 == RootChunk)
			    continue;

			sprintf(dname, "/dev/%s", c2->name);

			if (tmp->do_newfs && (!upgrade ||
			    !msgNoYes("You are upgrading - are you SURE you"
			    " want to newfs /dev/%s?", c2->name)))
				performNewfs(tmp, dname, QUEUE_YES);
			else
			    command_shell_add(tmp->mountpoint,
				"fsck_ffs -y /dev/%s", c2->name);
			command_func_add(tmp->mountpoint, Mount, c2->name);
		    }
		    else if (c2->type == part && c2->subtype == FS_SWAP) {
			char fname[80];
			int i;

			if (c2 == SwapChunk)
			    continue;
			sprintf(fname, "/dev/%s", c2->name);
			i = (Fake || swapon(fname));
			if (!i) {
			    dialog_clear_norefresh();
			    msgNotify("Added %s as an additional swap device", fname);
			}
			else {
			    msgConfirm("Unable to add %s as a swap device: %s", fname, strerror(errno));
			}
		    }
		}
	    }
	    else if (c1->type == fat && c1->private_data &&
		(root->do_newfs || upgrade)) {
		char name[FILENAME_MAX];

		sprintf(name, "/%s", ((PartInfo *)c1->private_data)->mountpoint);
		Mkdir(name);
	    }
#if defined(__ia64__)
	    else if (c1->type == efi && c1->private_data) {
		PartInfo *pi = (PartInfo *)c1->private_data;

		sprintf(dname, "/dev/%s", c1->name);

		if (pi->do_newfs && (!upgrade ||
		    !msgNoYes("You are upgrading - are you SURE you want to "
		    "newfs /dev/%s?", c1->name)))
			performNewfs(pi, dname, QUEUE_YES);
	    }
#endif
	}
    }

    command_sort();
    command_execute();
    dialog_clear_norefresh();
    return DITEM_SUCCESS | DITEM_RESTORE;
}

static char *
getRelname(void)
{
    static char buf[64];
    size_t sz = (sizeof buf) - 1;

    if (sysctlbyname("kern.osrelease", buf, &sz, NULL, 0) != -1) {
	buf[sz] = '\0';
	return buf;
    }
    else
	return "<unknown>";
}

/* Initialize various user-settable values to their defaults */
int
installVarDefaults(dialogMenuItem *self)
{

    /* Set default startup options */
    variable_set2(VAR_RELNAME,			getRelname(), 0);
    variable_set2(SYSTEM_STATE,		"update", 0);
    variable_set2(VAR_NEWFS_ARGS,		"-b 16384 -f 2048", 0);
    variable_set2(VAR_CONSTERM,                 "NO", 0);
    return DITEM_SUCCESS;
}

/* Load the environment up from various system configuration files */
void
installEnvironment(void)
{
}

