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

static Device *Devices[DEV_MAX];
static int numDevs;

#define	DEVICE_ENTRY(type, name, descr, max)	{ type, name, descr, max }

#define	CDROM(name, descr, max)					\
	DEVICE_ENTRY(DEVICE_TYPE_CDROM, name, descr, max)
#define	TAPE(name, descr, max)						\
	DEVICE_ENTRY(DEVICE_TYPE_TAPE, name, descr, max)
#define	DISK(name, descr, max)						\
	DEVICE_ENTRY(DEVICE_TYPE_DISK, name, descr, max)
#define	FLOPPY(name, descr, max)					\
	DEVICE_ENTRY(DEVICE_TYPE_FLOPPY, name, descr, max)
#define	NETWORK(name, descr)						\
	DEVICE_ENTRY(DEVICE_TYPE_NETWORK, name, descr, 0)
#define	SERIAL(name, descr, max)					\
	DEVICE_ENTRY(DEVICE_TYPE_NETWORK, name, descr, max)

static struct _devname {
    DeviceType type;
    char *name;
    char *description;
    int max;
} device_names[] = {
    CDROM("cd%d",	"SCSI CDROM drive",			4),
    CDROM("mcd%d",	"Mitsumi (old model) CDROM drive",	4),
    CDROM("scd%d",	"Sony CDROM drive - CDU31/33A type",	4),
    CDROM("acd%d",	"ATAPI/IDE CDROM",			4),
    TAPE("sa%d",	"SCSI tape drive",			4),
    TAPE("rwt%d",	"Wangtek tape drive",			4),
    DISK("da%d",	"SCSI disk device",			16),
    DISK("ad%d",	"ATA/IDE disk device",			16),
    DISK("ar%d",	"ATA/IDE RAID device",			16),
    DISK("afd%d",	"ATAPI/IDE floppy device",		4),
    DISK("mlxd%d",	"Mylex RAID disk",			4),
    DISK("amrd%d",	"AMI MegaRAID drive",			4),
    DISK("idad%d",	"Compaq RAID array",			4),
    DISK("twed%d",	"3ware ATA RAID array",			4),
    DISK("aacd%d",	"Adaptec FSA RAID array",		4),
    DISK("ipsd%d",	"IBM ServeRAID RAID array",		4),
    DISK("mfid%d",	"LSI MegaRAID SAS array",		4),
    FLOPPY("fd%d",	"floppy drive unit A",			4),
    SERIAL("cuad%d",	"%s on device %s (COM%d)",		16),
    NETWORK("ae",	"Attansic/Atheros L2 Fast Ethernet"),
    NETWORK("age",	"Attansic/Atheros L1 Gigabit Ethernet"),
    NETWORK("alc",	"Atheros AR8131/AR8132 PCIe Ethernet"),
    NETWORK("ale",	"Atheros AR8121/AR8113/AR8114 PCIe Ethernet"),
    NETWORK("an",	"Aironet 4500/4800 802.11 wireless adapter"),
    NETWORK("ath",	"Atheros IEEE 802.11 wireless adapter"),
    NETWORK("aue",	"ADMtek USB Ethernet adapter"),
    NETWORK("axe",	"ASIX Electronics USB Ethernet adapter"),
    NETWORK("bce",	"Broadcom NetXtreme II Gigabit Ethernet card"),
    NETWORK("bfe",	"Broadcom BCM440x PCI Ethernet card"),
    NETWORK("bge",	"Broadcom BCM570x PCI Gigabit Ethernet card"),
    NETWORK("cas",	"Sun Cassini/Cassini+ or NS DP83065 Saturn Ethernet"),
    NETWORK("cue",	"CATC USB Ethernet adapter"),
    NETWORK("cxgb",	"Chelsio T3 10Gb Ethernet card"),
    NETWORK("fpa",	"DEC DEFPA PCI FDDI card"),
    NETWORK("sr",	"SDL T1/E1 sync serial PCI card"),
    NETWORK("cc3i",	"SDL HSSI sync serial PCI card"),
    NETWORK("en",	"Efficient Networks ATM PCI card"),
    NETWORK("dc",	"DEC/Intel 21143 (and clones) PCI Fast Ethernet card"),
    NETWORK("de",	"DEC DE435 PCI NIC or other DC21040-AA based card"),
    NETWORK("fxp",	"Intel EtherExpress Pro/100B PCI Fast Ethernet card"),
    NETWORK("ed",	"Novell NE1000/2000; 3C503; NE2000-compatible PCMCIA"),
    NETWORK("ep",	"3Com 3C509 Ethernet card/3C589 PCMCIA"),
    NETWORK("em",	"Intel(R) PRO/1000 Ethernet card"),
    NETWORK("et",	"Agere ET1310 based PCI Express Gigabit Ethernet card"),
    NETWORK("ex",	"Intel EtherExpress Pro/10 Ethernet card"),
    NETWORK("fe",	"Fujitsu MB86960A/MB86965A Ethernet card"),
    NETWORK("gem",	"Apple GMAC or Sun ERI/GEM Ethernet adapter"),
    NETWORK("hme",	"Sun HME (Happy Meal Ethernet) Ethernet adapter"),
    NETWORK("ie",	"AT&T StarLAN 10 and EN100; 3Com 3C507; NI5210"),
    NETWORK("igb",	"Intel(R) PRO/1000 PCI Express Gigabit Ethernet card"),
    NETWORK("ixgb",	"Intel(R) PRO/10Gb Ethernet card"),
    NETWORK("ixgbe",	"Intel(R) PRO/10Gb Ethernet card"),
    NETWORK("jme",	"JMicron JMC250 Gigabit/JMC260 Fast Ethernet"),
    NETWORK("kue",	"Kawasaki LSI USB Ethernet adapter"),
    NETWORK("le",	"AMD Am7900 LANCE or Am79C9xx PCnet Ethernet adapter"),
    NETWORK("lge",	"Level 1 LXT1001 Gigabit Ethernet card"),
    NETWORK("msk",	"Marvell/SysKonnect Yukon II Gigabit Ethernet"),
    NETWORK("mxge",	"Myricom Myri10GE 10Gb Ethernet card"),
    NETWORK("nfe",	"NVIDIA nForce MCP Ethernet"),
    NETWORK("nge",	"NatSemi PCI Gigabit Ethernet card"),
    NETWORK("nve",	"NVIDIA nForce MCP Ethernet"),
    NETWORK("nxge",	"Neterion Xframe 10GbE Server/Storage adapter"),
    NETWORK("pcn",	"AMD Am79c79x PCI Ethernet card"),
    NETWORK("ray",	"Raytheon Raylink 802.11 wireless adapter"),
    NETWORK("re",	"RealTek 8139C+/8169/8169S/8110S PCI Ethernet card"),
    NETWORK("rl",	"RealTek 8129/8139 PCI Ethernet card"),
    NETWORK("rue",	"RealTek USB Ethernet card"),
    NETWORK("sf",	"Adaptec AIC-6915 PCI Ethernet card"),
    NETWORK("sis",	"SiS 900/SiS 7016 PCI Ethernet card"),
#ifdef PC98
    NETWORK("snc",	"SONIC Ethernet card"),
#endif
    NETWORK("sn",	"SMC/Megahertz Ethernet card"),
    NETWORK("ste",	"Sundance ST201 PCI Ethernet card"),
    NETWORK("stge",	"Sundance/Tamarack TC9021 Gigabit Ethernet"),
    NETWORK("sk",	"SysKonnect PCI Gigabit Ethernet card"),
    NETWORK("tx",	"SMC 9432TX Ethernet card"),
    NETWORK("txp",	"3Com 3cR990 Ethernet card"),
    NETWORK("ti",	"Alteon Networks PCI Gigabit Ethernet card"),
    NETWORK("tl",	"Texas Instruments ThunderLAN PCI Ethernet card"),
    NETWORK("vge",	"VIA VT612x PCI Gigabit Ethernet card"),
    NETWORK("vr",	"VIA VT3043/VT86C100A Rhine PCI Ethernet card"),
    NETWORK("vlan",	"IEEE 802.1Q VLAN network interface"),
    NETWORK("vx",	"3COM 3c590 / 3c595 Ethernet card"),
    NETWORK("wb",	"Winbond W89C840F PCI Ethernet card"),
    NETWORK("wi",	"Lucent WaveLAN/IEEE 802.11 wireless adapter"),
    NETWORK("xe",	"Xircom/Intel EtherExpress Pro100/16 Ethernet card"),
    NETWORK("xl",	"3COM 3c90x / 3c90xB PCI Ethernet card"),
    NETWORK("fwe",	"FireWire Ethernet emulation"),
    NETWORK("fwip",	"IP over FireWire"),
    NETWORK("plip",	"Parallel Port IP (PLIP) peer connection"),
    NETWORK("lo",	"Loop-back (local) network interface"),
    NETWORK("disc",	"Software discard network interface"),
    { 0, NULL, NULL, 0 }
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
	    msgDebug("deviceTry: open of %s succeeded on first try.\n", try);
    } else {
	if (isDebug())
	    msgDebug("deviceTry: open of %s failed.\n", try);
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
		/* nothing to do */
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
