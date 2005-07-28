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
#include <libdisk.h>

/* how much to bias minor number for a given /dev/<ct#><un#>s<s#> slice */
#define SLICE_DELTA	(0x10000)

static Device *Devices[DEV_MAX];
static int numDevs;

static struct _devname {
    DeviceType type;
    char *name;
    char *description;
    int major, minor, delta, max;
} device_names[] = {
    { DEVICE_TYPE_CDROM,	"cd%d",		"SCSI CDROM drive",	15, 2, 8, 4 				},
    { DEVICE_TYPE_CDROM,	"mcd%d",	"Mitsumi (old model) CDROM drive",	29, 0, 8, 4	 	},
    { DEVICE_TYPE_CDROM,	"scd%d",	"Sony CDROM drive - CDU31/33A type",	45, 0, 8, 4 		},
#ifdef notdef
    { DEVICE_TYPE_CDROM,	"matcd%d",	"Matsushita CDROM ('sound blaster' type)", 46, 0, 8, 4 		},
#endif
    { DEVICE_TYPE_CDROM,	"acd%d",	"ATAPI/IDE CDROM",	117, 0, 8, 4				},
    { DEVICE_TYPE_TAPE, 	"sa%d",		"SCSI tape drive",	14, 0, 16, 4				},
    { DEVICE_TYPE_TAPE, 	"rwt%d",	"Wangtek tape drive",	10, 0, 1, 4				},
    { DEVICE_TYPE_DISK, 	"da%d",		"SCSI disk device",	13, 65538, 8, 16			},
    { DEVICE_TYPE_DISK, 	"ad%d",		"ATA/IDE disk device",	116, 65538, 8, 16			},
    { DEVICE_TYPE_DISK, 	"ar%d",		"ATA/IDE RAID device",	157, 65538, 8, 16			},
    { DEVICE_TYPE_DISK, 	"afd%d",	"ATAPI/IDE floppy device",	118, 65538, 8, 4		},
    { DEVICE_TYPE_DISK, 	"mlxd%d",	"Mylex RAID disk",	131, 65538, 8, 4			},
    { DEVICE_TYPE_DISK, 	"amrd%d",	"AMI MegaRAID drive",	133, 65538, 8, 4			},
    { DEVICE_TYPE_DISK, 	"idad%d",	"Compaq RAID array",	109, 65538, 8, 4			},
    { DEVICE_TYPE_DISK, 	"twed%d",	"3ware ATA RAID array",	147, 65538, 8, 4			},
    { DEVICE_TYPE_DISK, 	"aacd%d",	"Adaptec FSA RAID array", 151, 65538, 8, 4			},
    { DEVICE_TYPE_DISK, 	"ipsd%d",	"IBM ServeRAID RAID array", 176, 65538, 8, 4			},
    { DEVICE_TYPE_FLOPPY,	"fd%d",		"floppy drive unit A",	9, 0, 64, 4				},
    { DEVICE_TYPE_NETWORK,	"an",		"Aironet 4500/4800 802.11 wireless adapter"			},
    { DEVICE_TYPE_NETWORK,	"aue",		"ADMtek USB ethernet adapter"					},
    { DEVICE_TYPE_NETWORK,	"axe",		"ASIX Electronics USB ethernet adapter"					},
    { DEVICE_TYPE_NETWORK,	"bfe",		"Broadcom BCM440x PCI ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"bge",		"Broadcom BCM570x PCI gigabit ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"cue",		"CATC USB ethernet adapter"					},
    { DEVICE_TYPE_NETWORK,	"fpa",		"DEC DEFPA PCI FDDI card"					},
    { DEVICE_TYPE_NETWORK,	"sr",		"SDL T1/E1 sync serial PCI card"				},
    { DEVICE_TYPE_NETWORK,	"cc3i",		"SDL HSSI sync serial PCI card"					},
    { DEVICE_TYPE_NETWORK,	"en",		"Efficient Networks ATM PCI card"				},
    { DEVICE_TYPE_NETWORK,	"dc",		"DEC/Intel 21143 (and clones) PCI fast ethernet card"		},
    { DEVICE_TYPE_NETWORK,	"de",		"DEC DE435 PCI NIC or other DC21040-AA based card"		},
    { DEVICE_TYPE_NETWORK,	"fxp",		"Intel EtherExpress Pro/100B PCI Fast Ethernet card"		},
    { DEVICE_TYPE_NETWORK,	"ed",		"Novell NE1000/2000; 3C503; NE2000-compatible PCMCIA"		},
    { DEVICE_TYPE_NETWORK,	"ep",		"3Com 3C509 ethernet card/3C589 PCMCIA"				},
    { DEVICE_TYPE_NETWORK,	"el",		"3Com 3C501 ethernet card"					},
    { DEVICE_TYPE_NETWORK,	"em",		"Intel(R) PRO/1000 ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"ex",		"Intel EtherExpress Pro/10 ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"fe",		"Fujitsu MB86960A/MB86965A ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"gem",		"Apple/Sun GMAC ethernet adapter"				},
    { DEVICE_TYPE_NETWORK,	"ie",		"AT&T StarLAN 10 and EN100; 3Com 3C507; NI5210"			},
    { DEVICE_TYPE_NETWORK,	"ix",		"Intel Etherexpress ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"kue",		"Kawasaki LSI USB ethernet adapter"				},
    { DEVICE_TYPE_NETWORK,	"le",		"DEC EtherWorks 2 or 3 ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"lnc",		"Lance/PCnet (Isolan/Novell NE2100/NE32-VL) ethernet"		},
    { DEVICE_TYPE_NETWORK,	"lge",		"Level 1 LXT1001 gigabit ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"nge",		"NatSemi PCI gigabit ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"pcn",		"AMD Am79c79x PCI ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"ray",		"Raytheon Raylink 802.11 wireless adaptor"			},
    { DEVICE_TYPE_NETWORK,	"re",		"RealTek 8139C+/8169/8169S/8110S PCI ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"rl",		"RealTek 8129/8139 PCI ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"rue",		"RealTek USB ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"sf",		"Adaptec AIC-6915 PCI ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"sis",		"SiS 900/SiS 7016 PCI ethernet card"				},
#ifdef PC98
    { DEVICE_TYPE_NETWORK,	"snc",		"SONIC ethernet card"						},
#endif
    { DEVICE_TYPE_NETWORK,	"sn",		"SMC/Megahertz ethernet card"					},
    { DEVICE_TYPE_NETWORK,	"ste",		"Sundance ST201 PCI ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"sk",		"SysKonnect PCI gigabit ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"tx",		"SMC 9432TX ethernet card"					},
    { DEVICE_TYPE_NETWORK,	"txp",		"3Com 3cR990 ethernet card"					},
    { DEVICE_TYPE_NETWORK,	"ti",		"Alteon Networks PCI gigabit ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"tl",		"Texas Instruments ThunderLAN PCI ethernet card"		},
    { DEVICE_TYPE_NETWORK,	"vge",		"VIA VT612x PCI gigabit ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"vr",		"VIA VT3043/VT86C100A Rhine PCI ethernet card"			},
    { DEVICE_TYPE_NETWORK,	"vlan",		"IEEE 802.1Q VLAN network interface"				},
    { DEVICE_TYPE_NETWORK,	"vx",		"3COM 3c590 / 3c595 ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"wb",		"Winbond W89C840F PCI ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"wi",		"Lucent WaveLAN/IEEE 802.11 wireless adapter"			},
    { DEVICE_TYPE_NETWORK,	"wx",		"Intel Gigabit Ethernet (82452) card"			},
    { DEVICE_TYPE_NETWORK,	"xe",		"Xircom/Intel EtherExpress Pro100/16 ethernet card"		},
    { DEVICE_TYPE_NETWORK,	"xl",		"3COM 3c90x / 3c90xB PCI ethernet card"				},
    { DEVICE_TYPE_NETWORK,	"cuad%d",	"%s on device %s (COM%d)",	28, 128, 1, 16			},
    { DEVICE_TYPE_NETWORK,	"fwe",		"FireWire Ethernet emulation"					},
    { DEVICE_TYPE_NETWORK,	"lp",		"Parallel Port IP (PLIP) peer connection"			},
    { DEVICE_TYPE_NETWORK,	"lo",		"Loop-back (local) network interface"				},
    { DEVICE_TYPE_NETWORK,	"disc",		"Software discard network interface"				},
    { 0 },
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
    mode_t m;
    dev_t d;
    int fail;

    snprintf(unit, sizeof unit, dev.name, i);
    snprintf(try, FILENAME_MAX, "/dev/%s", unit);
    if (isDebug())
	msgDebug("deviceTry: attempting to open %s\n", try);
    fd = open(try, O_RDONLY);
    if (fd >= 0) {
	if (isDebug())
	    msgDebug("deviceTry: open of %s succeeded on first try.\n", try);
	return fd;
    }
    m = 0640 | S_IFCHR;
    d = makedev(dev.major, dev.minor + (i * dev.delta));
    if (isDebug())
	msgDebug("deviceTry: Making %s device for %s [%d, %d]\n", m & S_IFCHR ? "raw" : "block", try, dev.major, dev.minor + (i * dev.delta));
    fail = mknod(try, m, d);
    fd = open(try, O_RDONLY);
    if (fd >= 0) {
	if (isDebug())
	    msgDebug("deviceTry: open of %s succeeded on second try.\n", try);
	return fd;
    }
    else if (!fail)
	(void)unlink(try);
    /* Don't try a "make-under" here since we're using a fixit floppy in this case */
    snprintf(try, FILENAME_MAX, "/mnt/dev/%s", unit);
    fd = open(try, O_RDONLY);
    if (isDebug())
	msgDebug("deviceTry: final attempt for %s returns %d\n", try, fd);
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
    int i, j, fd, s;
    struct ifconf ifc;
    struct ifreq *ifptr, *end;
    int ifflags;
    char buffer[INTERFACE_MAX * sizeof(struct ifreq)];
    char **names;

    msgNotify("Probing devices, please wait (this can take a while)...");
    /* First go for the network interfaces.  Stolen shamelessly from ifconfig! */
    ifc.ifc_len = sizeof(buffer);
    ifc.ifc_buf = buffer;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
	goto skipif;	/* Jump over network iface probing */

    if (ioctl(s, SIOCGIFCONF, (char *) &ifc) < 0)
	goto skipif;	/* Jump over network iface probing */

    close(s);
    ifflags = ifc.ifc_req->ifr_flags;
    end = (struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifptr = ifc.ifc_req; ifptr < end; ifptr++) {
	char *descr;

	/* If it's not a link entry, forget it */
	if (ifptr->ifr_ifru.ifru_addr.sa_family != AF_LINK)
	    goto loopend;

	/* Eliminate network devices that don't make sense */
	if (!strncmp(ifptr->ifr_name, "lo", 2))
	    goto loopend;

	/* If we have a slip device, don't register it */
	if (!strncmp(ifptr->ifr_name, "sl", 2)) {
	    goto loopend;
	}
	/* And the same for ppp */
	if (!strncmp(ifptr->ifr_name, "tun", 3) || !strncmp(ifptr->ifr_name, "ppp", 3)) {
	    goto loopend;
	}
	/* Try and find its description */
	for (i = 0, descr = NULL; device_names[i].name; i++) {
	    int len = strlen(device_names[i].name);

	    if (!ifptr->ifr_name || !ifptr->ifr_name[0])
		continue;
	    else if (!strncmp(ifptr->ifr_name, device_names[i].name, len)) {
		descr = device_names[i].description;
		break;
	    }
	}
	if (!descr)
	    descr = "<unknown network interface type>";

	deviceRegister(ifptr->ifr_name, descr, strdup(ifptr->ifr_name), DEVICE_TYPE_NETWORK, TRUE,
		       mediaInitNetwork, NULL, mediaShutdownNetwork, NULL);
	if (isDebug())
	    msgDebug("Found a network device named %s\n", ifptr->ifr_name);
	close(s);
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	    continue;

loopend:
	if (ifptr->ifr_addr.sa_len)	/* I'm not sure why this is here - it's inherited */
	    ifptr = (struct ifreq *)((caddr_t)ifptr + ifptr->ifr_addr.sa_len - sizeof(struct sockaddr));
	close(s);
    }

skipif:
    /* Next, try to find all the types of devices one might need
     * during the second stage of the installation.
     */
    for (i = 0; device_names[i].name; i++) {
	for (j = 0; j < device_names[i].max; j++) {
	    char try[FILENAME_MAX];

	    switch(device_names[i].type) {
	    case DEVICE_TYPE_CDROM:
		fd = deviceTry(device_names[i], try, j);
		if (fd >= 0 || errno == EBUSY) {	/* EBUSY if already mounted */
		    char n[BUFSIZ];

		    if (fd >= 0) close(fd);
		    snprintf(n, sizeof n, device_names[i].name, j);
		    deviceRegister(strdup(n), device_names[i].description, strdup(try),
					 DEVICE_TYPE_CDROM, TRUE, mediaInitCDROM, mediaGetCDROM,
					 mediaShutdownCDROM, NULL);
		    if (isDebug())
			msgDebug("Found a CDROM device for %s\n", try);
		}
		break;

	    case DEVICE_TYPE_TAPE:
		fd = deviceTry(device_names[i], try, j);
		if (fd >= 0) {
		    char n[BUFSIZ];

		    close(fd);
		    snprintf(n, sizeof n, device_names[i].name, j);
		    deviceRegister(strdup(n), device_names[i].description, strdup(try),
				   DEVICE_TYPE_TAPE, TRUE, mediaInitTape, mediaGetTape, mediaShutdownTape, NULL);
		    if (isDebug())
			msgDebug("Found a TAPE device for %s\n", try);
		}
		break;

	    case DEVICE_TYPE_DISK:
		fd = deviceTry(device_names[i], try, j);
		if (fd >= 0 && RunningAsInit) {
		    dev_t d;
		    mode_t m;
		    int s, fail;
		    char unit[80], slice[80];

		    close(fd);
		    /* Make associated slice entries */
		    for (s = 1; s < 8; s++) {
			snprintf(unit, sizeof unit, device_names[i].name, j);
			snprintf(slice, sizeof slice, "/dev/%ss%d", unit, s);
			d = makedev(device_names[i].major, device_names[i].minor +
				    (j * device_names[i].delta) + (s * SLICE_DELTA));
			m = 0640 | S_IFCHR;
			fail = mknod(slice, m, d);
			fd = open(slice, O_RDONLY);
			if (fd >= 0)
			    close(fd);
			else if (!fail)
			    (void)unlink(slice);
		    }
		}
		break;

	    case DEVICE_TYPE_FLOPPY:
		fd = deviceTry(device_names[i], try, j);
		if (fd >= 0) {
		    char n[BUFSIZ];

		    close(fd);
		    snprintf(n, sizeof n, device_names[i].name, j);
		    deviceRegister(strdup(n), device_names[i].description, strdup(try),
				   DEVICE_TYPE_FLOPPY, TRUE, mediaInitFloppy, mediaGetFloppy,
				   mediaShutdownFloppy, NULL);
		    if (isDebug())
			msgDebug("Found a floppy device for %s\n", try);
		}
		break;

	    case DEVICE_TYPE_NETWORK:
		fd = deviceTry(device_names[i], try, j);
		/* The only network devices that you can open this way are serial ones */
		if (fd >= 0) {
		    char *newdesc, *cp;

		    close(fd);
		    cp = device_names[i].description;
		    /* Serial devices get a slip and ppp device each, if supported */
		    newdesc = safe_malloc(strlen(cp) + 40);
		    sprintf(newdesc, cp, "SLIP interface", try, j + 1);
		    deviceRegister("sl0", newdesc, strdup(try), DEVICE_TYPE_NETWORK, TRUE, mediaInitNetwork,
				   NULL, mediaShutdownNetwork, NULL);
		    msgDebug("Add mapping for %s to sl0\n", try);
		    newdesc = safe_malloc(strlen(cp) + 50);
		    sprintf(newdesc, cp, "PPP interface", try, j + 1);
		    deviceRegister("ppp0", newdesc, strdup(try), DEVICE_TYPE_NETWORK, TRUE, mediaInitNetwork,
				   NULL, mediaShutdownNetwork, NULL);
		    if (isDebug())
			msgDebug("Add mapping for %s to ppp0\n", try);
		}
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
	    Chunk *c1;
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
