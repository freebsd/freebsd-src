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
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <ctype.h>
#include <libdisk.h>

/* how much to bias minor number for a given /dev/<ct#><un#>s<s#> slice */
#define SLICE_DELTA	(0x10000)

static Device *Devices[DEV_MAX];
static int numDevs;

#define	DEVICE_ENTRY(type, name, descr, max)    { type, name, descr, max }

#define	DISK(name, descr, max)                                          \
	DEVICE_ENTRY(DEVICE_TYPE_DISK, name, descr, max)

static struct _devname {
    DeviceType type;
    char *name;
    char *description;
    int max;
} device_names[] = {
    DISK("da%d",	"SCSI disk device",		16),
    DISK("ad%d",	"ATA/IDE disk device",		16),
    DISK("ar%d",	"ATA/IDE RAID device",		16),
    DISK("afd%d",	"ATAPI/IDE floppy device",	4),
    DISK("mlxd%d",	"Mylex RAID disk",		4),
    DISK("amrd%d",	"AMI MegaRAID drive",		4),
    DISK("idad%d",	"Compaq RAID array",		4),
    DISK("twed%d",	"3ware ATA RAID array",		4),
    DISK("aacd%d",	"Adaptec FSA RAID array",	4),
    DISK("ipsd%d",	"IBM ServeRAID RAID array",	4),
    DISK("mfid%d",	"LSI MegaRAID SAS array",	4),
    { 0, NULL, NULL, 0 },
};

Device *
new_device(char *name)
{
    Device *dev;

    dev = safe_malloc(sizeof(Device));
    bzero(dev, sizeof(Device));
    if (name)
	SAFE_STRCPY(dev->name, name);
    return dev;
}

/* Stubs for unimplemented strategy routines */
Boolean
dummyInit(Device *dev)
{
    return TRUE;
}

FILE *
dummyGet(Device *dev, char *dist, Boolean probe)
{
    return NULL;
}

void
dummyShutdown(Device *dev)
{
    return;
}

static int
deviceTry(struct _devname dev, char *try, int i)
{
    int fd;
    char unit[80];

    snprintf(unit, sizeof unit, dev.name, i);
    snprintf(try, FILENAME_MAX, "/dev/%s", unit);
    if (isDebug())
	msgDebug("deviceTry: attempting to open %s\n", try);
    fd = open(try, O_RDONLY);
    if (fd >= 0) {
	if (isDebug())
	    msgDebug("deviceTry: open of %s succeeded.\n", try);
    }
    return fd;
}

/* Register a new device in the devices array */
Device *
deviceRegister(char *name, char *desc, char *devname, DeviceType type, Boolean enabled,
	       Boolean (*init)(Device *), FILE * (*get)(Device *, char *, Boolean),
	       void (*shutdown)(Device *), void *private)
{
    Device *newdev = NULL;

    if (numDevs == DEV_MAX)
	msgFatal("Too many devices found!");
    else {
	newdev = new_device(name);
	newdev->description = desc;
	newdev->devname = devname;
	newdev->type = type;
	newdev->enabled = enabled;
	newdev->init = init ? init : dummyInit;
	newdev->get = get ? get : dummyGet;
	newdev->shutdown = shutdown ? shutdown : dummyShutdown;
	newdev->private = private;
	Devices[numDevs] = newdev;
	Devices[++numDevs] = NULL;
    }
    return newdev;
}

/* Reset the registered device chain */
void
deviceReset(void)
{
    int i;

    for (i = 0; i < numDevs; i++) {
	DEVICE_SHUTDOWN(Devices[i]);

	/* XXX this potentially leaks Devices[i]->private if it's being
	 * used to point to something dynamic, but you're not supposed
	 * to call this routine at such times that some open instance
	 * has its private ptr pointing somewhere anyway. XXX
	 */
	free(Devices[i]);
    }
    Devices[numDevs = 0] = NULL;
}

/* Get all device information for devices we have attached */
void
deviceGetAll(void)
{
    int i, j, fd;
    char **names;

    msgNotify("Probing devices, please wait (this can take a while)...");

    /* Next, try to find all the types of devices one might need
     * during the second stage of the installation.
     */
    for (i = 0; device_names[i].name; i++) {
	for (j = 0; j < device_names[i].max; j++) {
	    char try[FILENAME_MAX];

	    switch(device_names[i].type) {
	    case DEVICE_TYPE_DISK:
		fd = deviceTry(device_names[i], try, j);
		break;

	    default:
		break;
	    }
	}
    }

    /* Finally, go get the disks and look for DOS partitions to register */
    if ((names = Disk_Names()) != NULL) {
	int i;

	for (i = 0; names[i]; i++) {
	    Disk *d;

	    /* Ignore memory disks */
	    if (!strncmp(names[i], "md", 2))
		continue;

	    /*
	     * XXX 
	     *  Due to unknown reasons, Disk_Names() returns SCSI CDROM as a
	     * valid disk. This is main reason why sysinstall presents SCSI
	     * CDROM to available disks in Fdisk/Label menu. In addition,
	     * adding a blank SCSI CDROM to the menu generates floating point
	     * exception in sparc64. Disk_Names() just extracts sysctl
	     * "kern.disks". Why GEOM treats SCSI CDROM as a disk is beyond
	     * me and that should be investigated.
	     * For temporary workaround, ignore SCSI CDROM device.
	     */
	    if (!strncmp(names[i], "cd", 2))
		continue;

	    d = Open_Disk(names[i]);
	    if (!d) {
		msgDebug("Unable to open disk %s\n", names[i]);
		continue;
	    }

	    deviceRegister(names[i], names[i], d->name, DEVICE_TYPE_DISK, FALSE,
			   dummyInit, dummyGet, dummyShutdown, d);
	    if (isDebug())
		msgDebug("Found a disk device named %s\n", names[i]);

#if 0
	    /* Look for existing DOS partitions to register as "DOS media devices" */
	    for (c1 = d->chunks->part; c1; c1 = c1->next) {
		if (c1->type == fat || c1->type == efi || c1->type == extended) {
		    Device *dev;
		    char devname[80];

		    /* Got one! */
		    snprintf(devname, sizeof devname, "/dev/%s", c1->name);
		    dev = deviceRegister(c1->name, c1->name, strdup(devname), DEVICE_TYPE_DOS, TRUE,
			mediaInitDOS, mediaGetDOS, mediaShutdownDOS, NULL);
		    dev->private = c1;
		    if (isDebug())
			msgDebug("Found a DOS partition %s on drive %s\n", c1->name, d->name);
		}
	    }
#endif
	}
	free(names);
    }
    dialog_clear_norefresh();
}

/* Rescan all devices, after closing previous set - convenience function */
void
deviceRescan(void)
{
    deviceReset();
    deviceGetAll();
}

/*
 * Find all devices that match the criteria, allowing "wildcarding" as well
 * by allowing NULL or ANY values to match all.  The array returned is static
 * and may be used until the next invocation of deviceFind().
 */
Device **
deviceFind(char *name, DeviceType class)
{
    static Device *found[DEV_MAX];
    int i, j;

    j = 0;
    for (i = 0; i < numDevs; i++) {
	if ((!name || !strcmp(Devices[i]->name, name))
	    && (class == DEVICE_TYPE_ANY || class == Devices[i]->type))
	    found[j++] = Devices[i];
    }
    found[j] = NULL;
    return j ? found : NULL;
}

Device **
deviceFindDescr(char *name, char *desc, DeviceType class)
{
    static Device *found[DEV_MAX];
    int i, j;

    j = 0;
    for (i = 0; i < numDevs; i++) {
	if ((!name || !strcmp(Devices[i]->name, name)) &&
	    (!desc || !strcmp(Devices[i]->description, desc)) &&
	    (class == DEVICE_TYPE_ANY || class == Devices[i]->type))
	    found[j++] = Devices[i];
    }
    found[j] = NULL;
    return j ? found : NULL;
}

int
deviceCount(Device **devs)
{
    int i;

    if (!devs)
	return 0;
    for (i = 0; devs[i]; i++);
    return i;
}

/*
 * Create a menu listing all the devices of a certain type in the system.
 * The passed-in menu is expected to be a "prototype" from which the new
 * menu is cloned.
 */
DMenu *
deviceCreateMenu(DMenu *menu, DeviceType type, int (*hook)(dialogMenuItem *d), int (*check)(dialogMenuItem *d))
{
    Device **devs;
    int numdevs;
    DMenu *tmp = NULL;
    int i, j;

    devs = deviceFind(NULL, type);
    numdevs = deviceCount(devs);
    if (!numdevs)
	return NULL;
    tmp = (DMenu *)safe_malloc(sizeof(DMenu) + (sizeof(dialogMenuItem) * (numdevs + 1)));
    bcopy(menu, tmp, sizeof(DMenu));
    for (i = 0; devs[i]; i++) {
	tmp->items[i].prompt = devs[i]->name;
	for (j = 0; j < numDevs; j++) {
	    if (devs[i] == Devices[j]) {
		tmp->items[i].title = Devices[j]->description;
		break;
	    }
	}
	if (j == numDevs)
	    tmp->items[i].title = "<unknown device type>";
	tmp->items[i].fire = hook;
	tmp->items[i].checked = check;
    }
    tmp->items[i].title = NULL;
    return tmp;
}
