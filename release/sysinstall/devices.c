/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
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

#include "sysinstall.h"
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <ctype.h>

static Device *Devices[DEV_MAX];
static int numDevs;

static struct {
    DeviceType type;
    char *name;
    char *description;
} device_names[] = {
    { DEVICE_TYPE_CDROM,	"cd0a",		"SCSI CDROM drive"					},
    { DEVICE_TYPE_CDROM,	"mcd0a",	"Mitsumi (old model) CDROM drive"			},
    { DEVICE_TYPE_CDROM,	"scd0a",	"Sony CDROM drive - CDU31/33A type",			},
    { DEVICE_TYPE_CDROM,	"matcd0a",	"Matsushita CDROM ('sound blaster' type)"		},
    { DEVICE_TYPE_CDROM,	"wcd0c",	"ATAPI IDE CDROM"					},
    { DEVICE_TYPE_TAPE, 	"rst0",		"SCSI tape drive"					},
    { DEVICE_TYPE_TAPE, 	"rft0",		"Floppy tape drive (QIC-02)"				},
    { DEVICE_TYPE_TAPE, 	"rwt0",		"Wangtek tape drive"					},
    { DEVICE_TYPE_DISK, 	"sd",		"SCSI disk device"					},
    { DEVICE_TYPE_DISK, 	"wd",		"IDE/ESDI/MFM/ST506 disk device"			},
    { DEVICE_TYPE_DISK, 	"od",		"SCSI optical disk device"				},
    { DEVICE_TYPE_FLOPPY,	"fd0",		"floppy drive unit A"					},
    { DEVICE_TYPE_FLOPPY,	"fd1",		"floppy drive unit B"					},
    { DEVICE_TYPE_FLOPPY,	"od0",		"SCSI optical disk/floppy format"			},
    { DEVICE_TYPE_NETWORK,	"cuaa0",	"%s on serial port 0 (COM1)"				},
    { DEVICE_TYPE_NETWORK,	"cuaa1",	"%s on serial port 1 (COM2)"				},
    { DEVICE_TYPE_NETWORK,	"cuaa2",	"%s on serial port 2 (COM3)"				},
    { DEVICE_TYPE_NETWORK,	"cuaa3",	"%s on serial port 3 (COM4)"				},
    { DEVICE_TYPE_NETWORK,	"lp0",		"Parallel Port IP (PLIP) using laplink cable"		},
    { DEVICE_TYPE_NETWORK,	"lo",		"Loop-back (local) network interface"			},
    { DEVICE_TYPE_NETWORK,	"de",		"DEC DE435 PCI NIC or other DC21040-AA based card"	},
    { DEVICE_TYPE_NETWORK,	"fxp",		"Intel EtherExpress Pro/100B PCI Fast Ethernet card"	},
    { DEVICE_TYPE_NETWORK,	"ed",		"WD/SMC 80xx; Novell NE1000/2000; 3Com 3C503 card"	},
    { DEVICE_TYPE_NETWORK,	"ep",		"3Com 3C509 ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"el",		"3Com 3C501 ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"ex",		"Intel EtherExpress Pro/10 ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"fe",		"Fujitsu MB86960A/MB86965A ethernet card"		},
    { DEVICE_TYPE_NETWORK,	"ie",		"AT&T StarLAN 10 and EN100; 3Com 3C507; NI5210"		},
    { DEVICE_TYPE_NETWORK,	"ix",		"Intel Etherexpress ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"le",		"DEC EtherWorks 2 or 3 ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"lnc",		"Lance/PCnet (Isolan/Novell NE2100/NE32-VL) ethernet"	},
    { DEVICE_TYPE_NETWORK,	"vx",		"3COM 3c590 / 3c595 / 3c9xx ethernet card"	},
    { DEVICE_TYPE_NETWORK,	"ze",		"IBM/National Semiconductor PCMCIA ethernet card"	},
    { DEVICE_TYPE_NETWORK,	"zp",		"3Com Etherlink III PCMCIA ethernet card"		},
    { NULL },
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
deviceTry(char *name, char *try)
{
    int fd;

    snprintf(try, FILENAME_MAX, "/dev/%s", name);
    fd = open(try, O_RDONLY);
    if (fd > 0)
	return fd;
    if (errno == EBUSY)
	return -1;
    snprintf(try, FILENAME_MAX, "/mnt/dev/%s", name);
    fd = open(try, O_RDONLY);
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

/* Get all device information for devices we have attached */
void
deviceGetAll(void)
{
    int i, fd, s;
    struct ifconf ifc;
    struct ifreq *ifptr, *end;
    int ifflags;
    char buffer[INTERFACE_MAX * sizeof(struct ifreq)];
    char **names;

    /* Try and get the disks first */
    if ((names = Disk_Names()) != NULL) {
	int i;

	for (i = 0; names[i]; i++) {
	    Chunk *c1;
	    Disk *d;

	    d = Open_Disk(names[i]);
	    if (!d)
		msgFatal("Unable to open disk %s", names[i]);

	    (void)deviceRegister(names[i], names[i], d->name, DEVICE_TYPE_DISK, FALSE, NULL, NULL, NULL, d);
	    msgDebug("Found a disk device named %s\n", names[i]);

	    /* Look for existing DOS partitions to register */
	    for (c1 = d->chunks->part; c1; c1 = c1->next) {
		if (c1->type == fat || c1->type == extended) {
		    Device *dev;
		    char devname[80];

		    /* Got one! */
		    sprintf(devname, "/dev/%s", c1->name);
		    dev = deviceRegister(c1->name, c1->name, strdup(devname), DEVICE_TYPE_DOS, TRUE,
					 mediaInitDOS, mediaGetDOS, mediaShutdownDOS, NULL);
		    dev->private = c1;
		    msgDebug("Found a DOS partition %s on drive %s\n", c1->name, d->name);
		}
	    }
	}
	free(names);
    }

    /* Now go for the network interfaces.  Stolen shamelessly from ifconfig! */
    ifc.ifc_len = sizeof(buffer);
    ifc.ifc_buf = buffer;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	msgConfirm("ifconfig: socket");
	goto skipif;	/* Jump over network iface probing */
    }
    if (ioctl(s, SIOCGIFCONF, (char *) &ifc) < 0) {
	msgConfirm("ifconfig (SIOCGIFCONF)");
	goto skipif;	/* Jump over network iface probing */
    }
    ifflags = ifc.ifc_req->ifr_flags;
    end = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifptr = ifc.ifc_req; ifptr < end; ifptr++) {
	char *descr;

	/* If it's not a link entry, forget it */
	if (ifptr->ifr_ifru.ifru_addr.sa_family != AF_LINK)
	    continue;

	/* Eliminate network devices that don't make sense */
	if (!strncmp(ifptr->ifr_name, "lo0", 3))
	    continue;

	/* If we have a slip device, don't register it */
	if (!strncmp(ifptr->ifr_name, "sl", 2)) {
	    continue;
	}
	/* And the same for ppp */
	if (!strncmp(ifptr->ifr_name, "tun", 3) || !strncmp(ifptr->ifr_name, "ppp", 3)) {
	    continue;
	}
	/* Try and find its description */
	for (i = 0, descr = NULL; device_names[i].name; i++) {
	    int len = strlen(device_names[i].name);

	    if (!strncmp(ifptr->ifr_name, device_names[i].name, len)) {
		descr = device_names[i].description;
		break;
	    }
	}
	if (!descr)
	    descr = "<unknown network interface type>";

	deviceRegister(ifptr->ifr_name, descr, strdup(ifptr->ifr_name), DEVICE_TYPE_NETWORK, TRUE,
		       mediaInitNetwork, NULL, mediaShutdownNetwork, NULL);
	msgDebug("Found a network device named %s\n", ifptr->ifr_name);
	close(s);
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    msgConfirm("ifconfig: socket");
	    continue;
	}
	if (ifptr->ifr_addr.sa_len)	/* I'm not sure why this is here - it's inherited */
	    ifptr = (struct ifreq *)((caddr_t)ifptr + ifptr->ifr_addr.sa_len - sizeof(struct sockaddr));
    }

skipif:
    /* Finally, try to find all the types of devices one might need
     * during the second stage of the installation.
     */
    for (i = 0; device_names[i].name; i++) {
	char try[FILENAME_MAX];

	switch(device_names[i].type) {
	case DEVICE_TYPE_CDROM:
	    fd = deviceTry(device_names[i].name, try);
	    msgDebug("Try for %s returns errno %d\n", device_names[i].name, errno);
	    if (fd >= 0 || errno == EBUSY) {	/* EBUSY if already mounted */
		if (fd >= 0) close(fd);
		(void)deviceRegister(device_names[i].name, device_names[i].description, strdup(try),
				     DEVICE_TYPE_CDROM, TRUE, mediaInitCDROM, mediaGetCDROM,
				     mediaShutdownCDROM, NULL);
		msgDebug("Found a CDROM device named %s\n", device_names[i].name);
	    }
	    break;

	case DEVICE_TYPE_TAPE:
	    fd = deviceTry(device_names[i].name, try);
	    if (fd >= 0) {
		close(fd);
		deviceRegister(device_names[i].name, device_names[i].description, strdup(try),
			       DEVICE_TYPE_TAPE, TRUE, mediaInitTape, mediaGetTape, mediaShutdownTape, NULL);
		msgDebug("Found a TAPE device named %s\n", device_names[i].name);
	    }
	    break;

	case DEVICE_TYPE_FLOPPY:
	    fd = deviceTry(device_names[i].name, try);
	    if (fd >= 0) {
		close(fd);
		deviceRegister(device_names[i].name, device_names[i].description, strdup(try),
			       DEVICE_TYPE_FLOPPY, TRUE, mediaInitFloppy, mediaGetFloppy,
			       mediaShutdownFloppy, NULL);
		msgDebug("Found a floppy device named %s\n", device_names[i].name);
	    }
	    break;

	case DEVICE_TYPE_NETWORK:
	    fd = deviceTry(device_names[i].name, try);
	    /* The only network devices that you can open this way are serial ones */
	    if (fd >= 0) {
		char *newdesc, *cp;

		close(fd);
		cp = device_names[i].description;
		/* Serial devices get a slip and ppp device each, if supported */
		newdesc = safe_malloc(strlen(cp) + 40);
		sprintf(newdesc, cp, "SLIP interface");
		deviceRegister("sl0", newdesc, strdup(try), DEVICE_TYPE_NETWORK, TRUE, mediaInitNetwork,
				NULL, mediaShutdownNetwork, NULL);
		msgDebug("Add mapping for %s on %s to sl0\n", device_names[i].name, try);
		newdesc = safe_malloc(strlen(cp) + 50);
		sprintf(newdesc, cp, "PPP interface");
		deviceRegister("ppp0", newdesc, strdup(try), DEVICE_TYPE_NETWORK, TRUE, mediaInitNetwork,
				NULL, mediaShutdownNetwork, NULL);
		msgDebug("Add mapping for %s on %s to ppp0\n", device_names[i].name, try);
	    }
	    break;

	default:
	    break;
	}
    }
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
