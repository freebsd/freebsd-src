/**
 ** Copyright (c) 1995
 **      Michael Smith, msmith@atrad.adelaide.edu.au.  All rights reserved.
 **
 ** This code replaces a module marked :

 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 Jordan K. Hubbard
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 *
 * Many additional changes by Bruce Evans
 *
 * This code is derived from software contributed by the
 * University of California Berkeley, Jordan K. Hubbard,
 * David Greenman and Bruce Evans.

 ** As such, it may contain code subject to the above copyrights.
 **
 ** 
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer as
 **    the first lines of this file unmodified.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. All advertising materials mentioning features or use of this software
 **    must display the following acknowledgment:
 **      This product includes software developed by Michael Smith.
 ** 4. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY MICHAEL SMITH ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL MICHAEL SMITH BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **
 **      $Id$
 **/

/**
 ** USERCONF2
 **
 ** Fullscreen kernel boot-time configuration manipulation tool for FreeBSD.
 **
 **   msmith@atrad.adelaide.edu.au
 **
 ** Look for "EDIT THIS LIST" to add to the list of known devices
 ** 
 **
 ** There are a number of assumptions made in this code.
 ** 
 ** - That the console supports a minimal set of ANSI escape sequences
 **   (See the screen manipulation section for a summary)
 **   and has at least 24 rows.
 ** - That values less than or equal to zero for any of the device
 **   parameters indicate that the driver does not use the parameter.
 ** - That the only tunable parameter for PCI devices are their flags.
 ** - That flags are _always_ editable.
 **
 ** Devices marked as disabled are imported as such.  It is possible to move
 ** a PCI device onto the inactive list, but it is not possible to actually
 ** prevent the device from being probed.  The ability to move is considered
 ** desirable in that people will complain otherwise 8)
 ** 
 ** For this tool to be useful, the list of devices below _MUST_ be updated 
 ** when a new driver is brought into the kernel.  It is not possible to 
 ** extract this information from the drivers in the kernel, as the devconf
 ** structure for the device is not registered until the device is probed,
 ** which is too late.
 **
 ** XXX - TODO:
 ** 
 ** - Display _what_ a device conflicts with.
 ** - Find out how to put syscons back into low-intensity mode so that the
 **   !b escape is useful on the console.
 ** - The min and max values used for editing parameters are probably 
 **   very bogus - fix?
 **
 ** - ?? Smarter redraw() to reduce screen flicker whenever something is
 **   edited.
 **
 **/

#ifdef KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <machine/md_var.h>
#include <i386/i386/cons.h>
#include <i386/include/clock.h>
#include <i386/isa/isa_device.h>
#include <sys/kernel.h>
#include <pci/pcivar.h>

static struct isa_device *devtabs[] = { isa_devtab_bio, isa_devtab_tty, isa_devtab_net,
				     isa_devtab_null, NULL };
#define putchar(x)	cnputc(x)
#define getchar()	cngetc()

#else

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>

static void fakedevs(void);			    
#endif

#ifndef FALSE
#define FALSE	(0)
#define TRUE	(!FALSE)
#endif


typedef struct
{
    char	dev[16];		/* device basename */
    char	name[60];		/* long name */
    int		attrib;			/* things to do with the device */
    int		class;			/* device classification */
} DEV_INFO;

#define FLG_INVISIBLE	(1<<0)		/* device should not be shown */
#define FLG_MANDATORY	(1<<1)		/* device can be edited but not disabled */
#define FLG_FIXIRQ	(1<<2)		/* device IRQ cannot be changed */
#define FLG_FIXIOBASE	(1<<3)		/* device iobase cannot be changed */
#define FLG_FIXMADDR	(1<<4)		/* device maddr cannot be changed */
#define FLG_FIXMSIZE	(1<<5)		/* device msize cannot be changed */
#define FLG_FIXDRQ	(1<<6)		/* device DRQ cannot be changed */
#define FLG_FIXED	(FLG_FIXIRQ|FLG_FIXIOBASE|FLG_FIXMADDR|FLG_FIXMSIZE|FLG_FIXDRQ)
#define FLG_IMMUTABLE	(FLG_FIXED|FLG_MANDATORY)

#define CLS_STORAGE	1		/* storage devices */
#define CLS_NETWORK	2		/* network interfaces */
#define CLS_COMMS	3		/* serial, parallel ports */
#define CLS_INPUT	4		/* user input : mice, keyboards, joysticks etc */
#define CLS_MMEDIA	5		/* "multimedia" devices (sound, video, etc) */
#define CLS_MISC	255		/* none of the above */


typedef struct 
{
    char	name[60];
    int		number;
} DEVCLASS_INFO;

static DEVCLASS_INFO devclass_names[] = {
{	"======= Storage ======",	CLS_STORAGE},
{	"======= Network ======",	CLS_NETWORK},
{	"=== Communications ===",	CLS_COMMS},
{	"======== Input =======",	CLS_INPUT},
{	"===== Multimedia =====",	CLS_MMEDIA},
{	"==== Miscellaneous ===",	CLS_MISC},
{	"",0}};


/********************* EDIT THIS LIST **********************/

/** Notes :
 ** 
 ** - PCI devices should be marked FLG_FIXED, not FLG_IMMUTABLE.  Whilst
 **   it's impossible to disable them, it should be possible to move them
 **   from one list to another for peace of mind.
 ** - Devices that shouldn't be seen or removed should be marked FLG_INVISIBLE.
 ** - XXX The list below should be reviewed by the driver authors to verify
 **   that the correct flags have been set for each driver, and that the
 **   descriptions are accurate.
 **/

static DEV_INFO device_info[] = {
/*---Name-----   ---Description---------------------------------------------- */
{"bt",          "Buslogic SCSI controller",		0,		CLS_STORAGE},
{"ahc",         "Adaptec 274x/284x/294x SCSI controller",	0,	CLS_STORAGE},
{"ahb",         "Adaptec 174x SCSI controller",		0,		CLS_STORAGE},
{"aha",         "Adaptec 154x SCSI controller",		0,		CLS_STORAGE},
{"uha",         "Ultrastor 14F/34F SCSI controller",	0,		CLS_STORAGE},
{"aic",         "Adaptec 152x SCSI and compatible sound cards",	0,      CLS_STORAGE},
{"nca",         "ProAudio Spectrum SCSI and comaptibles",	0,	CLS_STORAGE},
{"sea",         "Seagate ST01/ST02 SCSI and compatibles",	0,	CLS_STORAGE},
{"wds",         "Western Digitial WD7000 SCSI controller",	0,	CLS_STORAGE},
{"ncr",         "NCR 53C810 SCSI controller",		FLG_FIXED,	CLS_STORAGE},
{"wdc",         "IDE/ESDI/MFM disk controller",		0,		CLS_STORAGE},
{"fdc",         "Floppy disk controller",		FLG_FIXED,	CLS_STORAGE},
{"mcd",         "Mitsumi CD-ROM",			0,		CLS_STORAGE},
{"scd",         "Sony CD-ROM",				0,		CLS_STORAGE},
{"matcd",       "Matsushita/Panasonic/Creative CDROM",	0,		CLS_STORAGE},
{"wt",          "Wangtek/Archive QIC-02 Tape drive",	0,		CLS_STORAGE},

{"ed",          "NE1000,NE2000,3C503,WD/SMC80xx Ethernet adapters",0,	CLS_NETWORK},
{"el",          "3C501 Ethernet adapter",		0,		CLS_NETWORK},
{"ep",          "3C509 Ethernet adapter",		0,		CLS_NETWORK},
{"fe",          "Fujitsu MD86960A/MB869685A Ethernet adapters",	0,	CLS_NETWORK},
{"fea",         "DEC DEFEA EISA FDDI adapter",		0,		CLS_NETWORK},
{"ie",          "AT&T Starlan 10 and EN100, 3C507, NI5210 Ethernet adapters",0,CLS_NETWORK},
{"ix",          "Intel EtherExpress Ethernet adapter",	0,		CLS_NETWORK},
{"le",          "DEC Etherworks 2 and 3 Ethernet adapters",	0,	CLS_NETWORK},
{"lnc",         "Isolan, Novell NE2100/NE32-VL Ethernet adapters",	0,CLS_NETWORK},
{"ze",          "IBM/National Semiconductor PCMCIA Ethernet adapter",0,	CLS_NETWORK},
{"zp",          "3COM PCMCIA Etherlink III Ethernet adapter",	0,	CLS_NETWORK},
{"de",          "DEC DC21040 Ethernet adapter",		FLG_FIXED,	CLS_NETWORK},
{"fpa",         "DEC DEFPA PCI FDDI adapter",		FLG_FIXED,	CLS_NETWORK},

{"sio",         "8250/16450/16550 Serial port",		0,		CLS_COMMS},
{"cx",          "Cronyx/Sigma multiport sync/async adapter",0,		CLS_COMMS},
{"rc",          "RISCom/8 multiport async adapter",	0,		CLS_COMMS},
{"cy",          "Cyclades high-speed multiport async adapter",	0,	CLS_COMMS},
{"lpt",         "Parallel printer port",		0,		CLS_COMMS},
{"nic",         "ISDN driver",				0,		CLS_COMMS},
{"nnic",        "ISDN driver",				0,		CLS_COMMS},
{"gp",          "National Instruments AT-GPIB/TNT driver",	0,	CLS_COMMS},

{"mse",         "Microsoft Bus Mouse",			0,		CLS_INPUT},
{"psm",         "PS/2 Mouse",				0,		CLS_INPUT},
{"joy",         "Joystick",				FLG_FIXED,	CLS_INPUT},
{"vt",          "PCVT console driver",			FLG_INVISIBLE,	CLS_INPUT},
{"sc",          "Syscons console driver",		FLG_INVISIBLE,	CLS_INPUT},

{"sb",          "Soundblaster PCM (SB, SBPro, SB16, ProAudio Spectrum)",0,CLS_MMEDIA},
{"sbxvi",       "Soundblaster 16",			0,		CLS_MMEDIA},
{"sbmidi",      "Soundblaster MIDI interface",		0,		CLS_MMEDIA},
{"pas",         "ProAudio Spectrum PCM and MIDI",	0,		CLS_MMEDIA},
{"gus",         "Gravis Ultrasound, Ultrasound 16 and Ultrasound MAX",0,CLS_MMEDIA},
{"gusxvi",      "Gravis Ultrasound 16-bit PCM",		0,		CLS_MMEDIA},
{"gusmax",      "Gravis Ultrasound MAX",		0,		CLS_MMEDIA},
{"mss",         "Microsoft Sound System",		0,		CLS_MMEDIA},
{"opl",         "OPL-2/3 FM, Soundblaster, SBPro, SB16, ProAudio Spectrum",0,CLS_MMEDIA},
{"mpu",         "Roland MPU401 MIDI",			0,		CLS_MMEDIA},
{"uart",        "6850 MIDI UART",			0,		CLS_MMEDIA},
{"pca",         "PC speaker PCM audio driver",		FLG_FIXED,	CLS_MMEDIA},
{"ctx",         "Coretex-I frame grabber",		0,		CLS_MMEDIA},
{"spigot",      "Creative Labs Video Spigot video capture",	0,	CLS_MMEDIA},
{"gsc",         "Genius GS-4500 hand scanner",		0,		CLS_MMEDIA},

{"apm",         "Advanced Power Management",		FLG_FIXED,	CLS_MISC},
{"labpc",       "National Instruments Lab-PC/Lab-PC+",	0,		CLS_MISC},
{"npx",	        "Math coprocessor",			FLG_INVISIBLE,	CLS_MISC},
{"lkm",		"Loadable PCI driver support",		FLG_INVISIBLE,	CLS_MISC},
{"vga",		"Catchall PCI VGA driver",		FLG_INVISIBLE,	CLS_MISC},
{"chip",	"PCI chipset support",			FLG_INVISIBLE,	CLS_MISC},
{"","",0,0}};


typedef struct _devlist_struct
{
    char	name[80];
    int		attrib;			/* flag values as per the FLG_* defines above */
    int		class;			/* disk, etc as per the CLS_* defines above */
    char	dev[16];
    int		iobase,irq,drq,maddr,msize,unit,flags,conflict_ok,id;
    int		comment;		/* 0 = device, 1 = comment, 2 = collapsed comment */
    int		conflicts;		/* set/reset by findconflict, count of conflicts */
    int		changed;		/* nonzero if the device has been edited */
    struct isa_device	*device;
    struct _devlist_struct *prev,*next;
} DEV_LIST;


#define DEV_DEVICE	0
#define DEV_COMMENT	1
#define DEV_ZOOMED	2

#define	LIST_CURRENT	(1<<0)
#define LIST_SELECTED	(1<<1)

#define KEY_EXIT	0	/* return codes from dolist() and friends */
#define KEY_DO		1
#define KEY_DEL		2
#define KEY_TAB		3
#define KEY_REDRAW	4

#define KEY_UP		5	/* these only returned from editval() */
#define KEY_DOWN	6
#define KEY_LEFT	7
#define KEY_RIGHT	8
#define KEY_NULL	9	/* this allows us to spin & redraw */

static void redraw(void);
static void insdev(DEV_LIST *dev, DEV_LIST *list);
static int  devinfo(DEV_LIST *dev);

static DEV_LIST	*active = NULL,*inactive = NULL;	/* driver lists */
static DEV_LIST	*alist,*ilist;				/* visible heads of the driver lists */
static DEV_LIST	scratch;				/* scratch record */
static int	conflicts;				/* total conflict count */


static char lines[] = "--------------------------------------------------------------------------------";
static char spaces[] = "                                                                                     ";


#ifdef KERNEL
/**
 ** Device manipulation stuff : find, describe, configure.
 **/

/**
 ** setdev
 **
 ** Sets the device referenced by (*dev) to the parameters in the struct,
 ** and the enable flag according to (enabled)
 **/
static void 
setdev(DEV_LIST *dev, int enabled)
{
    if (!dev->device)							/* PCI device */
	return;
    dev->device->id_iobase = dev->iobase;				/* copy happy */
    dev->device->id_irq = (u_short)(dev->irq < 16 ? 1<<dev->irq : 0);	/* IRQ is bitfield */
    dev->device->id_drq = (short)dev->drq;
    dev->device->id_maddr = (caddr_t)dev->maddr;
    dev->device->id_msize = dev->msize;
    dev->device->id_enabled = enabled;
}


/**
 ** getdevs
 **
 ** Walk the kernel device tables and build the active and inactive lists
 **
 **/
static void 
getdevs(void)
{
    int 		i,j;
    struct isa_device	*ap;

    for (j = 0; devtabs[j]; j++)			/* ISA devices */
    {
	ap = devtabs[j];				/* pointer to array of devices */
	for (i = 0; ap[i].id_id; i++)			/* for each device in this table */
	{
	    scratch.unit = ap[i].id_unit;		/* device parameters */
	    strcpy(scratch.dev,ap[i].id_driver->name);
	    scratch.iobase = ap[i].id_iobase;
	    scratch.irq = ffs(ap[i].id_irq)-1;
	    scratch.drq = ap[i].id_drq;
	    scratch.maddr = (int)ap[i].id_maddr;
	    scratch.msize = ap[i].id_msize;
	    scratch.conflict_ok = ap[i].id_conflicts;

	    scratch.comment = DEV_DEVICE;		/* admin stuff */
	    scratch.conflicts = 0;
	    scratch.device = &ap[i];			/* save pointer for later reference */
	    scratch.changed = 0;
	    if (!devinfo(&scratch))			/* get more info on the device */
		insdev(&scratch,ap[i].id_enabled?active:inactive);
	}
    }
#if NPCI > 0
    for (i = 0; i < pcidevice_set.ls_length; i++)
    {
	if (pcidevice_set.ls_items[i])
	{
	    if (((struct pci_device *)pcidevice_set.ls_items[i])->pd_name)
	    {
		strcpy(scratch.dev,((struct pci_device *)pcidevice_set.ls_items[i])->pd_name);
		scratch.iobase = -2;			/* mark as PCI for future reference */
		scratch.irq = -2;
		scratch.drq = -2;
		scratch.maddr = -2;
		scratch.msize = -2;
		scratch.conflict_ok = 0;			/* shouldn't conflict */
		scratch.comment = DEV_DEVICE;			/* is a device */
		scratch.unit = 0;				/* arbitrary number of them */
		scratch.conflicts = 0;
		scratch.device = NULL;	
		scratch.changed = 0;

		if (!devinfo(&scratch))
		    insdev(&scratch,active);		/* always active */
	    }
	}
    }
#endif
}


/**
 ** Devinfo
 **
 ** Fill in (dev->name), (dev->attrib) and (dev->type) from the device_info array.
 ** If the device is unknown, put it in the CLS_MISC class, with no flags.
 **
 ** If the device is marked "invisible", return nonzero; the caller should
 ** not insert any such device into either list.
 **
 **/
static int
devinfo(DEV_LIST *dev)
{
    int		i;

    for (i = 0; device_info[i].class; i++)
    {
	if (!strcmp(dev->dev,device_info[i].dev))
	{
	    if (device_info[i].attrib & FLG_INVISIBLE)
		return(1);
	    strcpy(dev->name,device_info[i].name);
	    dev->attrib = device_info[i].attrib;
	    dev->class = device_info[i].class;
	    return(0);
	}
    }
    strcpy(dev->name,"Unknown device");
    dev->attrib = 0;
    dev->class = CLS_MISC;
    return(0);
}
    
#endif


/**
 ** List manipulation stuff : add, move, initialise, free, traverse
 **
 ** Note that there are assumptions throughout this code that
 ** the first entry in a list will never move. (assumed to be
 ** a comment).
 **/


/**
 ** Adddev
 ** 
 ** appends a copy of (dev) to the end of (*list)
 **/
static void 
addev(DEV_LIST *dev, DEV_LIST **list)
{

    DEV_LIST	*lp,*ap;

#ifdef KERNEL
    lp = (DEV_LIST *)malloc(sizeof(DEV_LIST),M_DEVL,M_WAITOK);
#else
    lp = (DEV_LIST *)malloc(sizeof(DEV_LIST));
#endif
    bcopy(dev,lp,sizeof(DEV_LIST));			/* create copied record */

    if (*list)						/* list exists */
    {
	ap = *list;
	while(ap->next)
	    ap = ap->next;				/* scoot to end of list */
	lp->prev = ap;
	lp->next = NULL;
	ap->next = lp;
    }else{						/* list does not yet exist */
	*list = lp;
	lp->prev = lp->next = NULL;			/* list now exists */
    }
}


/**
 ** Findspot
 **
 ** Finds the 'appropriate' place for (dev) in (list)
 **
 ** 'Appropriate' means in numeric order with other devices of the same type,
 ** or in alphabetic order following a comment of the appropriate type.
 ** or at the end of the list if an appropriate comment is not found. (this should
 ** never happen)
 ** (Note that the appropriate point is never the top, but may be the bottom)
 **/
static DEV_LIST	*
findspot(DEV_LIST *dev, DEV_LIST *list)
{
    DEV_LIST	*ap;

    for (ap = list; ap; ap = ap->next)
    {
	if (ap->comment != DEV_DEVICE)			/* ignore comments */
	    continue;
	if (!strcmp(dev->dev,ap->dev))			/* same base device */
	{
	    if ((dev->unit <= ap->unit)			/* belongs before (equal is bad) */
		|| !ap->next)				/* or end of list */
	    {
		ap = ap->prev;				/* back up one */
		break;					/* done here */
	    }
	    if (ap->next)				/* if the next item exists */
	    {
		if (ap->next->comment != DEV_DEVICE)	/* next is a comment */
		    break;
		if (strcmp(dev->dev,ap->next->dev))		/* next is a different device */
		    break;
	    }
	}
    }

    if (!ap)
    {
	for (ap = list; ap; ap = ap->next)
	{
	    if (ap->comment != DEV_DEVICE)		/* look for simlar devices */
		continue;
	    if (dev->class != ap->class)			/* of same class too 8) */
		continue;
	    if (strcmp(dev->dev,ap->dev) < 0)		/* belongs before the current entry */
	    {
		ap = ap->prev;				/* back up one */
		break;					/* done here */
	    }
	    if (ap->next)				/* if the next item exists */
		if (ap->next->comment != DEV_DEVICE)	/* next is a comment, go here */
		    break;
	}
    }

    if (!ap)						/* didn't find a match */
    {
	for (ap = list; ap->next; ap = ap->next)	/* try for a matching comment */
	    if ((ap->comment != DEV_DEVICE) 
		&& (ap->class == dev->class))		/* appropriate place? */
		break;
    }							/* or just put up with last */

    return(ap);
}


/**
 ** Insdev
 **
 ** Inserts a copy of (dev) at the appropriate point in (list)
 **/
static void 
insdev(DEV_LIST *dev, DEV_LIST *list)
{
    DEV_LIST	*lp,*ap;

#ifdef KERNEL
    lp = (DEV_LIST *)malloc(sizeof(DEV_LIST),M_DEVL,M_WAITOK);
#else
    lp = (DEV_LIST *)malloc(sizeof(DEV_LIST));
#endif
    bcopy(dev,lp,sizeof(DEV_LIST));			/* create copied record */

    ap = findspot(lp,list);				/* find appropriate spot */
    lp->next = ap->next;				/* point to next */
    if (ap->next)
	ap->next->prev = lp;				/* point next to new */
    lp->prev = ap;					/* point new to current */
    ap->next = lp;					/* and current to new */
}


/**
 ** Movedev
 **
 ** Moves (dev) from its current list to an appropriate place in (list)
 ** (dev) may not come from the top of a list, but it may from the bottom.
 **/
static void 
movedev(DEV_LIST *dev, DEV_LIST *list)
{
    DEV_LIST	*ap;

    ap = findspot(dev,list);
    dev->prev->next = dev->next;			/* remove from old list */
    if (dev->next)
	dev->next->prev = dev->prev;
    
    dev->next = ap->next;				/* insert in new list */
    if (ap->next)
	ap->next->prev = dev;				/* point next to new */
    dev->prev = ap;					/* point new to current */
    ap->next = dev;					/* and current to new */
}
	    

/**
 ** Initlist
 **
 ** Initialises (*list) with the basic headings
 **
 **/
static void 
initlist(DEV_LIST **list)
{
    int		i;

    for(i = 0; devclass_names[i].name[0]; i++)		/* for each devtype name */
    {
	strcpy(scratch.name,devclass_names[i].name);
	scratch.comment = DEV_ZOOMED;
	scratch.class = devclass_names[i].number;
	scratch.attrib = FLG_MANDATORY;			/* can't be moved */
	addev(&scratch,list);				/* add to the list */
    }
}


/**
 ** savelist
 **
 ** Walks (list) and saves the settings of any entry marked as changed.
 **
 ** The device's active field is set according to (active).
 **/
static void
savelist(DEV_LIST *list, int active)
{
    while (list)
    {
#ifdef KERNEL
	if ((list->comment == DEV_DEVICE) && list->changed)
	    setdev(list,active);
#endif
	list = list->next;
    }
}


/**
 ** nukelist
 **
 ** Frees all storage in use by a (list).
 **/
static void 
nukelist(DEV_LIST *list)
{
    DEV_LIST	*dp;

    if (!list)
	return;
    while(list->prev)					/* walk to head of list */
	list = list->prev;

    while(list)
    {
	dp = list;
	list = list->next;
#ifdef KERNEL
	free(dp,M_DEVL);
#else
	free(dp);
#endif
    }
}


/**
 ** prevent
 **
 ** Returns the previous entry in (list), skipping zoomed regions.  Returns NULL
 ** if there is no previous entry. (Only possible if list->prev == NULL given the
 ** premise that there is always a comment at the head of the list)
 **
 **/
static DEV_LIST *
prevent(DEV_LIST *list)
{
    DEV_LIST	*dp;

    if (!list)
	return(NULL);
    dp = list->prev;			/* start back one */
    while(dp)
    {
	if (dp->comment == DEV_ZOOMED)	/* previous section is zoomed */
	    return(dp);			/* so skip to comment */
	if (dp->comment == DEV_COMMENT)	/* not zoomed */
	    return(list->prev);		/* one back as normal */
	dp = dp->prev;			/* backpedal */
    }
    return(dp);				/* NULL, we can assume */
}


/**
 ** nextent
 **
 ** Returns the next entry in (list), skipping zoomed regions.  Returns NULL
 ** if there is no next entry. (Possible if the current entry is last, or
 ** if the current entry is the last heading and it's collapsed)
 **/
static DEV_LIST	*
nextent(DEV_LIST *list)
{
    DEV_LIST	*dp;

    if (!list)
	return(NULL);
    if (list->comment != DEV_ZOOMED)	/* no reason to skip */
	return(list->next);
    dp = list->next;
    while(dp)
    {
	if (dp->comment != DEV_DEVICE)	/* found another heading */
	    break;
	dp = dp->next;
    }
    return(dp);				/* back we go */
}
    

/**
 ** ofsent
 **
 ** Returns the (ofs)th entry down from (list), or NULL if it doesn't exist 
 **/
static DEV_LIST *
ofsent(int ofs, DEV_LIST *list)
{
    while (ofs-- && list)
	list = nextent(list);
    return(list);
}


/**
 ** findconflict
 **
 ** Scans every element of (list) and sets the conflict tags appropriately
 ** Returns the number of conflicts found.
 **/
static int
findconflict(DEV_LIST *list)
{
    int		count = 0;			/* number of conflicts found */
    DEV_LIST	*dp,*sp;

    for (dp = list; dp; dp = dp->next)		/* over the whole list */
    {
	if (dp->comment != DEV_DEVICE)		/* comments don't usually conflict */
	    continue;

	dp->conflicts = 0;			/* assume the best */
	for (sp = list; sp; sp = sp->next)	/* scan the entire list for conflicts */
	{
	    if (sp->comment != DEV_DEVICE)	/* likewise */
		continue;
	    if (sp == dp)			/* always conflict with itself */
		continue;
	    if (sp->conflict_ok && dp->conflict_ok)
		continue;			/* both allowed to conflict */

	    if ((dp->iobase > 0) &&		/* iobase conflict? */
		(dp->iobase == sp->iobase))
		dp->conflicts++;
	    if ((dp->irq > 0) &&		/* irq conflict? */
		(dp->irq == sp->irq))
		dp->conflicts++;
	    if ((dp->drq > 0) &&		/* drq conflict? */
		(dp->drq == sp->drq))
		dp->conflicts++;
	    if ((dp->maddr > 0) &&		/* maddr conflict? */
		(dp->maddr == sp->maddr))
		dp->conflicts++;
	    if ((dp->msize > 0) &&		/* msize conflict? */
		(dp->msize == sp->msize))
		dp->conflicts++;
	}
	count += dp->conflicts;			/* count conflicts */
    }
    return(count);
}


/**
 ** Screen-manipulation stuff
 **
 ** This is the basic screen layout :
 **
 **     0    5   10   15   20   25   30   35   40   45   50   55   60   67   70   75
 **     |....|....|....|....|....|....|....|....|....|....|....|....|....|....|....|....
 **    +--------------------------------------------------------------------------------+
 ** 0 -|---Active Drivers----------------------------------------------Dev---IRQ--Port--|
 ** 1 -| ........................                                    .......  ..  0x....|
 ** 2 -| ........................                                    .......  ..  0x....|
 ** 3 -| ........................                                    .......  ..  0x....|
 ** 4 -| ........................                                    .......  ..  0x....|
 ** 5 -| ........................                                    .......  ..  0x....|
 ** 6 -| ........................                                    .......  ..  0x....|
 ** 7 -| ........................                                    .......  ..  0x....|
 ** 8 -|------------------------------------------------------UP-DOWN-------------------|
 ** 9 -|---Inactive Drivers--------------------------------------------Dev--------------|
 ** 10-| ........................                                    .......            |
 ** 11-| ........................                                    .......            |
 ** 12-| ........................                                    .......            |
 ** 13-| ........................                                    .......            |
 ** 14-| ........................                                    .......            |
 ** 15-| ........................                                    .......            |
 ** 16-| ........................                                    .......            |
 ** 17-|------------------------------------------------------UP-DOWN-------------------|
 ** 18-| Relevant parameters for the current device                                     |
 ** 19-|                                                                                |
 ** 20-|                                                                                |
 ** 21-|--------------------------------------------------------------------------------|
 ** 22-| [Enter] edit device parameters  [DEL] disable device                           |
 ** 23-| [TAB]   change fields           [Q] save and exit                              |
 **    +--------------------------------------------------------------------------------+
 **
 **/


/**
 **
 ** The base-level screen primitives :
 **
 ** bold()	- enter bold mode 		\E[1m     (md)
 ** inverse()   - enter inverse mode 		\E[7m     (so)
 ** normal()	- clear bold/inverse mode 	\E[m      (se)
 ** clear()	- clear the screen 		\E[H\E[J  (ce)
 ** move(x,y)	- move the cursor to x,y 	\E[y;xH:  (cm)
 **/

static void 
bold(void)
{
    printf("\033[1m");
}

static void 
inverse(void)
{
    printf("\033[7m");
}

static void 
normal(void)
{
    printf("\033[m");
}

static void 
clear(void)
{
    normal();
    printf("\033[H\033[J");
}

static void 
move(int x, int y)
{
    printf("\033[%d;%dH",y+1,x+1);
}


/**
 **
 ** High-level screen primitives :
 ** 
 ** putxyl(x,y,str,len)	- put (len) bytes of (str) at (x,y), supports embedded formatting
 ** putxy(x,y,str)	- put (str) at (x,y), supports embedded formatting
 ** erase(x,y,w,h)	- clear the box (x,y,w,h)
 ** txtbox(x,y,w,y,str) - put (str) in a region at (x,y,w,h)
 ** putmsg(str)		- put (str) in the message area
 ** puthelp(str)	- put (str) in the upper helpline
 ** pad(str,len)	- pad (str) to (len) with spaces
 ** drawline(row,detail,list,inverse,*dhelp)
 **			- draws a line for (*list) at (row) onscreen.  If (detail) is 
 **			  nonzero, include port, IRQ and maddr, if (inverse) is nonzero,
 **			  draw the line in inverse video, and display (*dhelp) on the
 **                       helpline.
 ** drawlist(row,num,detail,list)
 **			- draw (num) entries from (list) at (row) onscreen, passile (detail)
 **			  through to drawline().
 ** showparams(dev)	- displays the relevant parameters for (dev) below the lists onscreen.
 ** yesno(str)		- displays (str) in the message area, and returns nonzero on 'y' or 'Y'
 ** redraw();		- Redraws the entire screen layout, including the 
 **			- two list panels.
 **/

/** 
 ** putxy
 **   writes (str) at x,y onscreen
 ** putxyl
 **   writes up to (len) of (str) at x,y onscreen.
 **
 ** Supports embedded formatting :
 ** !i - inverse mode.
 ** !b - bold mode.
 ** !n - normal mode.
 **
 **
 **/
static void 
putxyl(int x, int y, char *str, int len)
{
    move(x,y);
    normal();

    while((*str) && (len--))
    {
	if (*str == '!')		/* format escape? */
	{
	    switch(*(str+1))		/* depending on the next character */
	    {
	    case 'i':
		inverse();
		str +=2;		/* skip formatting */
		len++;			/* doesn't count for length */
		break;
		
	    case 'b':
		bold();
		str  +=2;		/* skip formatting */
		len++;			/* doesn't count for length */
		break;

	    case 'n':
		normal();
		str +=2;		/* skip formatting */
		len++;			/* doesn't count for length */
		break;
		
	    default:
		putchar(*str++);	/* not an escape */
	    }
	}else{
	    putchar(*str++);		/* emit the character */
	}
    }
}

#define putxy(x,y,str)	putxyl(x,y,str,-1)


/**
 ** erase
 **
 ** Erases the region (x,y,w,h)
 **/
static void 
erase(int x, int y, int w, int h)
{
    int		i;

    normal();
    for (i = 0; i < h; i++)
	putxyl(x,y++,spaces,w);
}


/** 
 ** txtbox
 **
 ** Writes (str) into the region (x,y,w,h), supports embedded formatting using
 ** putxy.  Lines are not wrapped, newlines must be forced with \n.
 **
 **
 **/
static void 
txtbox(int x, int y, int w, int h, char *str)
{
    int		i = 0;

    h--;
    while((str[i]) && h)
    {
	if (str[i] == '\n')			/* newline */
	{
	    putxyl(x,y,str,(i<w)?i:w);		/* write lesser of i or w */
	    y++;				/* move down */
	    h--;				/* room for one less */
	    str += (i+1);			/* skip first newline */
	    i = 0;				/* zero offset */
	}else{
	    i++;				/* next character */
	}
    }
    if (h)					/* end of string, not region */
	putxyl(x,y,str,w);
}


/**
 ** putmsg
 **
 ** writes (msg) in the helptext area
 **
 **/
static void 
putmsg(char *msg)
{
    erase(0,18,80,3);				/* clear area */
    txtbox(0,18,80,3,msg);
#ifndef KERNEL
    fflush(stdout);
#endif
}


/**
 ** puthelp
 **
 ** Writes (msg) in the helpline area
 **/
static void 
puthelp(char *msg)
{
    erase(0,22,80,1);
    putxy(0,22,msg);
}


/**
 ** pad 
 **
 ** space-pads a (str) to (len) characters
 ** 
 **/
static void 
pad(char *str, int len)
{
    int		i;

    for (i = 0; str[i]; i++)			/* find the end of the string */
	;
    if (i >= len)				/* no padding needed */
	return;
    while(i < len)				/* pad */
	str[i++] = ' ';
    str[i] = '\0';
}
						   

/**
 ** drawline
 **
 ** Displays entry (ofs) of (list) in region at (row) onscreen, optionally displaying
 ** the port and IRQ fields if (detail) is nonzero.  If (inverse), in inverse video.
 **
 ** The text (dhelp) is displayed if the item is a normal device, otherwise
 ** help is shown for normal or zoomed comments
 **
 **/
static void 
drawline(int row, int detail, DEV_LIST *list, int inverse, char *dhelp)
{
    char	lbuf[90],nb[70],db[20],ib[16],pb[16];
    
    strcpy(nb,list->name);
    if ((list->comment == DEV_ZOOMED) && (list->next))
	if (list->next->comment == DEV_DEVICE)	/* only mention if there's something hidden */
	    strcat(nb,"  (Collapsed)");
    pad(nb,60);
    if (list->conflicts)			/* device in conflict? */
	if (inverse)
	{
	    strcpy(nb+54," !nCONF!i ");		/* tag conflict, careful of length */
	}else{
	    strcpy(nb+54," !iCONF!n ");		/* tag conflict, careful of length */
	}

    if (list->comment == DEV_DEVICE)
    {
	sprintf(db,"%s%d",list->dev,list->unit);
	pad(db,8);
    }else{
	strcpy(db,"        ");
    }
    if ((list->irq > 0) && detail && (list->comment == DEV_DEVICE))
    {
	sprintf(ib," %d",list->irq);
	pad(ib,4);
    }else{
	strcpy(ib,"    ");
    }
    if ((list->iobase > 0) && detail && (list->comment == DEV_DEVICE))
    {
	sprintf(pb,"0x%x",list->iobase);
	pad(pb,7);
    }else{
	strcpy(pb,"       ");
    }

    sprintf(lbuf,"  %s%s%s%s%s",inverse?"!i":"",nb,db,ib,pb);

    putxyl(0,row,lbuf,80);
    switch(list->comment)
    {
    case DEV_DEVICE:			/* ordinary device */
	puthelp(dhelp);
	break;
    case DEV_COMMENT:
	puthelp("  [!bEnter!n] Collapse device list");
	break;
    case DEV_ZOOMED:
	puthelp("  [!bEnter!n] Expand device list");
	break;
    default:
	puthelp("  WARNING: This list entry corrupted!");
	break;
    }
    move(0,row);				/* put the cursor somewhere relevant */
}


/**
 ** drawlist
 **
 ** Displays (num) lines of the contents of (list) at (row), optionally displaying the
 ** port and IRQ fields as well if (detail) is nonzero
 **
 ** printf in the kernel is essentially useless, so we do most of the hard work ourselves here.
 **/
static void 
drawlist(int row, int num, int detail, DEV_LIST *list)
{
    int		ofs;

    for(ofs = 0; ofs < num; ofs++)
    {
	drawline(row+ofs,detail,list,0,"");
	list = nextent(list);			/* move down visible list */
	if (!list)
	    return;				/* nothing more to draw */
    }
}


/**
 ** redraw
 **
 ** Clear the screen and redraw the entire layout
 **
 **/
static void 
redraw(void)
{
    char	cbuf[16];

    clear();
    putxy(0,0,lines);
    putxy(3,0,"!bActive!n-!bDrivers");
    putxy(63,0,"!bDev!n---!bIRQ!n--!bPort");
    putxy(0,8,lines);
    if (conflicts)
    {
	sprintf(cbuf,"!i%d conflict%s",conflicts/2,(conflicts>2)?"s":"");
	putxy(25,8,cbuf);
    }
    putxy(0,9,lines);
    putxy(3,9,"!bInactive!n-!bDrivers");
    putxy(63,9,"!bDev");
    putxy(0,17,lines);
    putxy(0,21,lines);
    putxy(2,23,"[!bTAB!n]   change fields           [!bQ!n]   save and exit");

    drawlist(1,7,1,alist);			/* draw device lists */
    drawlist(10,7,0,ilist);
}


/**
 ** yesnocancel
 **
 ** Put (str) in the message area, and return 1 if the user hits 'y' or 'Y',
 ** 2 if they hit 'c' or 'C',  or 0 for 'n' or 'N'.
 **
 **/
static int
yesnocancel(char *str)
{

    putmsg(str);
    for(;;)
	switch(getchar())
	{
	case 'n':
	case 'N':
	    return(0);
	    
	case 'y':
	case 'Y':
	    return(1);
	    
	case 'c':
	case 'C':
	    return(2);
	}
}


/**
 ** showparams
 **
 ** Show device parameters in the region below the lists
 **
 **     0    5   10   15   20   25   30   35   40   45   50   55   60   67   70   75
 **     |....|....|....|....|....|....|....|....|....|....|....|....|....|....|....|....
 **    +--------------------------------------------------------------------------------+
 ** 17-|--------------------------------------------------------------------------------|
 ** 18-| Port address : 0x0000     Memory address : 0x00000   Conflict allowed          |
 ** 19-| IRQ number   : 00         Memory size    : 0x0000                              |
 ** 20-| Flags        : 0x0000     DRQ number     : 00                                  |
 ** 21-|--------------------------------------------------------------------------------|
 **/
static void 
showparams(DEV_LIST *dev)
{
    char	buf[80];

    erase(0,18,80,3);				/* clear area */
    if (!dev)
	return;
    if (dev->comment != DEV_DEVICE)
	return;


    if (dev->iobase > 0)
    {
	sprintf(buf,"Port address : 0x%x",dev->iobase);
	putxy(1,18,buf);
    }
    if (dev->irq > 0)
    {
	sprintf(buf,"IRQ number   : %d",dev->irq);
	putxy(1,19,buf);
    }
    sprintf(buf,"Flags        : 0x%x",dev->flags);
    putxy(1,20,buf);
    if (dev->maddr > 0)
    {
	sprintf(buf,"Memory address : 0x%x",dev->maddr);
	putxy(26,18,buf);
    }
    if (dev->msize > 0)
    {
	sprintf(buf,"Memory size    : 0x%x",dev->msize);
	putxy(26,19,buf);
    }

    if (dev->drq > 0)
    {
	sprintf(buf,"DRQ number     : %d",dev->drq);
	putxy(26,20,buf);
    }
    if (dev->conflict_ok)
	putxy(54,18,"Conflict allowed");
}


/**
 ** Editing functions for device parameters
 **
 ** editval(x,y,width,hex,min,max,val)	- Edit (*val) in a field (width) wide at (x,y)
 **					  onscreen.  Refuse values outsise (min) and (max).
 ** editparams(dev)			- Edit the parameters for (dev)
 **/


#define VetRet(code)							\
{									\
    if ((i >= min) && (i <= max))	/* legit? */			\
    {									\
	*val = i;							\
	sprintf(buf,hex?"0x%x":"%d",i);					\
	putxy(hex?x-2:x,y,buf);						\
	return(code);			/* all done and exit */		\
    }									\
    i = *val;				/* restore original value */	\
    delta = 1;				/* restore other stuff */	\
}


/**
 ** editval
 **
 ** Edit (*val) at (x,y) in (hex)?hex:decimal mode, allowing values between (min) and (max)
 ** in a field (width) wide. (Allow one space)
 ** If (ro) is set, we're in "readonly" mode, so disallow edits.
 **
 ** Return KEY_TAB on \t, KEY_EXIT on 'q'
 **/
static int 
editval(int x, int y, int width, int hex, int min, int max, int *val, int ro)
{
    int		i = *val;			/* work with copy of the value */
    char	buf[10],tc[8];			/* display buffer, text copy */
    int		xp = 0;				/* cursor offset into text copy */
    int		delta = 1;			/* force redraw first time in */
    int		c;
    int		extended = 0;			/* stage counter for extended key sequences */

    if (hex)					/* we presume there's a leading 0x onscreen */
	putxy(x-2,y,"!i0x");			/* coz there sure is now */
    	
    for (;;)
    {
	if (delta)				/* only update if necessary */
	{
	    sprintf(tc,hex?"%x":"%d",i);	/* make a text copy of the value */
	    sprintf(buf,"!i%s",tc);		/* format for printing */
	    erase(x,y,width,1);			/* clear the area */
	    putxy(x,y,buf);			/* write */
	    xp = strlen(tc);			/* cursor always at end */
	    move(x+xp,y);			/* position the cursor */
	}

	c = getchar();

	switch(extended)			/* escape handling */
	{
	case 0:
	    if (c == 0x1b)			/* esc? */
	    {
		extended = 1;			/* flag and spin */
		continue;
	    }
	    extended = 0;
	    break;				/* nope, drop through */
	
	case 1:					/* there was an escape prefix */
	    if (c == '[')			/* second character in sequence */
	    {
		extended = 2;
		continue;
	    }
	    if (c == 0x1b)
		return(KEY_EXIT);		/* double esc exits */
	    extended = 0;
	    break;				/* nup, not a sequence. */

	case 2:
	    extended = 0;
	    switch(c)				/* looks like the real McCoy */
	    {
	    case 'A':
		VetRet(KEY_UP);			/* leave if OK */
		continue;
	    case 'B':
		VetRet(KEY_DOWN);		/* leave if OK */
		continue;
	    case 'C':
		VetRet(KEY_RIGHT);		/* leave if OK */
		continue;
	    case 'D':
		VetRet(KEY_LEFT);		/* leave if OK */
		continue;
		
	    default:
		continue;
	    }
	}
    
	switch(c)
	{
	case '\t':				/* trying to tab off */
	    VetRet(KEY_TAB);			/* verify and maybe return */
	    break;

	case 'q':
	case 'Q':
	    VetRet(KEY_EXIT);
	    break;
	    
	case '\b':
	case '\177':				/* BS or DEL */
	    if (ro)				/* readonly? */
	    {
		puthelp(" !iThis value cannot be edited (Press ESC)");
		while(getchar() != 0x1b);	/* wait for key */
		return(KEY_NULL);		/* spin */
	    }
	    if (xp)				/* still something left to delete */
	    {
		i = i / (hex?0x10:10);		/* strip last digit */
		delta = 1;			/* force update */
	    }
	    break;

	case 588:
	    VetRet(KEY_UP);
	    break;

	case 596:
	    VetRet(KEY_DOWN);
	    break;

	case 591:
	    VetRet(KEY_LEFT);
	    break;

	case 593:
	    VetRet(KEY_RIGHT);
	    break;
		
	default:
	    if (ro)				/* readonly? */
	    {
		puthelp(" !iThis value cannot be edited (Press ESC)");
		while(getchar() != 0x1b);	/* wait for key */
		return(KEY_NULL);		/* spin */
	    }
	    if (xp >= width)			/* no room for more characters anyway */
		break;
	    if (hex)
	    {
		if ((c >= '0') && (c <= '9'))
		{
		    i = i*0x10 + (c-'0');	/* update value */
		    delta = 1;
		    break;
		}
		if ((c >= 'a') && (c <= 'f'))
		{
		    i = i*0x10 + (c-'a'+0xa);
		    delta = 1;
		    break;
		}
		if ((c >= 'A') && (c <= 'F'))
		{
		    i = i*0x10 + (c-'A'+0xa);
		    delta = 1;
		    break;
		}
	    }else{
		if ((c >= '0') && (c <= '9'))
		{
		    i = i*10 + (c-'0');		/* update value */
		    delta = 1;			/* force redraw */
		    break;
		}
	    }
	    break;
	}
    }
}


/**
 ** editparams
 **
 ** Edit the parameters for (dev)
 **
 ** Note that it's _always_ possible to edit the flags, otherwise it might be
 ** possible for this to spin in an endless loop...
 **     0    5   10   15   20   25   30   35   40   45   50   55   60   67   70   75
 **     |....|....|....|....|....|....|....|....|....|....|....|....|....|....|....|....
 **    +--------------------------------------------------------------------------------+
 ** 17-|--------------------------------------------------------------------------------|
 ** 18-| Port address : 0x0000     Memory address : 0x00000   Conflict allowed          |
 ** 19-| IRQ number   : 00         Memory size    : 0x0000                              |
 ** 20-| Flags        : 0x0000     DRQ number     : 00                                  |
 ** 21-|--------------------------------------------------------------------------------|
 **
 ** The "intelligence" in this function that hops around based on the directional
 ** returns from editval isn't very smart, and depends on the layout above.
 **/
static void 
editparams(DEV_LIST *dev)
{
    int		ret;
    char	buf[16];		/* needs to fit the device name */

    putxy(2,17,"!bParameters!n-!bfor!n-!bdevice!n-");
    sprintf(buf,"!b%s",dev->dev);
    putxy(24,17,buf);

    erase(1,22,80,1);
    for (;;)
    {
    ep_iobase:
	if (dev->iobase > 0)
	{
	    puthelp("  IO Port address (Hexadecimal, 0x1-0x2000)");
	    ret = editval(18,18,5,1,0x1,0x2000,&(dev->iobase),(dev->attrib & FLG_FIXIOBASE));
	    switch(ret)
	    {
	    case KEY_EXIT:
		goto ep_exit;

	    case KEY_RIGHT:
		if (dev->maddr > 0)
		    goto ep_maddr;
		break;

	    case KEY_TAB:
	    case KEY_DOWN:
		goto ep_irq;
	    }
	    goto ep_iobase;
	}
    ep_irq:
	if (dev->irq > 0)
	{
	    puthelp("  Interrupt number (Decimal, 1-15)");
	    ret = editval(16,19,3,0,1,15,&(dev->irq),(dev->attrib & FLG_FIXIRQ));
	    switch(ret)
	    {
	    case KEY_EXIT:
		goto ep_exit;

	    case KEY_RIGHT:
		if (dev->msize > 0)
		    goto ep_msize;
		break;

	    case KEY_UP:
		if (dev->iobase > 0)
		    goto ep_iobase;
		break;

	    case KEY_TAB:
	    case KEY_DOWN:
		goto ep_flags;
	    }
	    goto ep_irq;
	}
    ep_flags:
	puthelp("  Device-specific flag values.");
	ret = editval(18,20,5,1,0x0,0xffff,&(dev->flags),0);
	switch(ret)
	{
	case KEY_EXIT:
	    goto ep_exit;

	case KEY_RIGHT:
	    if (dev->drq > 0) 
		goto ep_drq;
	    break;

	case KEY_UP:
	    if (dev->irq > 0)
		goto ep_irq;
	    if (dev->iobase > 0)
		goto ep_iobase;
	    break;

	case KEY_DOWN:
	    if (dev->maddr > 0)
		goto ep_maddr;
	    if (dev->msize > 0)
		goto ep_msize;
	    if (dev->drq > 0)
		goto ep_drq;
	    break;

	case KEY_TAB:
	    goto ep_maddr;
	}
	goto ep_flags;
    ep_maddr:
	if (dev->maddr > 0)
	{
	    puthelp("  Device memory start address (Hexadecimal, 0x1-0xfffff)");
	    ret = editval(45,18,6,1,0x1,0xfffff,&(dev->maddr),(dev->attrib & FLG_FIXMADDR));
	    switch(ret)
	    {
	    case KEY_EXIT:
		goto ep_exit;

	    case KEY_LEFT:
		if (dev->iobase > 0)
		    goto ep_iobase;
		break;

	    case KEY_UP:
		goto ep_flags;

	    case KEY_DOWN:
		if (dev->msize > 0)
		    goto ep_msize;
		if (dev->drq > 0)
		    goto ep_drq;
		break;

	    case KEY_TAB:
		goto ep_msize;
	    }
	    goto ep_maddr;
	}
    ep_msize:
	if (dev->msize > 0)
	{
	    puthelp("  Device memory size (Hexadecimal, 0x1-0x10000)");
	    ret = editval(45,19,5,1,0x1,0x10000,&(dev->msize),(dev->attrib & FLG_FIXMSIZE));
	    switch(ret)
	    {
	    case KEY_EXIT:
		goto ep_exit;

	    case KEY_LEFT:
		if (dev->irq > 0)
		    goto ep_irq;
		break;

	    case KEY_UP:
		if (dev->maddr > 0)
		    goto ep_maddr;
		goto ep_flags;

	    case KEY_DOWN:
		if (dev->drq > 0)
		    goto ep_drq;
		break;

	    case KEY_TAB:
		goto ep_drq;
	    }
	    goto ep_msize;
	}
    ep_drq:
	if (dev->drq > 0)
	{
	    puthelp("  Device DMA request number (Decimal, 1-7)");
	    ret = editval(43,20,2,0,1,7,&(dev->drq),(dev->attrib & FLG_FIXDRQ));
	    switch(ret)
	    {
	    case KEY_EXIT:
		goto ep_exit;

	    case KEY_LEFT:
		goto ep_flags;

	    case KEY_UP:
		if (dev->msize > 0)
		    goto ep_msize;
		if (dev->maddr > 0)
		    goto ep_maddr;
		goto ep_flags;

	    case KEY_TAB:
		goto ep_iobase;
	    }
	    goto ep_drq;
	}
    }
    ep_exit:
#ifdef KERNEL
    dev->changed = 1;					/* mark as changed */
#endif
}


/** 
 ** High-level control functions
 **/


/**
 ** dolist
 **
 ** Handle user movement within (*list) in the region starting at (row) onscreen with
 ** (num) lines, starting at (*ofs) offset from row onscreen.
 ** Pass (detail) on to drawing routines.
 **
 ** If the user hits a key other than a cursor key, maybe return a code.
 **
 ** (*list) points to the device at the top line in the region, (*ofs) is the 
 ** position of the highlight within the region.  All routines below
 ** this take only a device and an absolute row : use ofsent() to find the 
 ** device, and add (*ofs) to (row) to find the absolute row.
 **
 **/
static int 
dolist(int row, int num, int detail, int *ofs, DEV_LIST **list, char *dhelp)
{
    int		extended = 0;
    int		c;
    DEV_LIST	*lp;
    int		delta = 1;
    
    for(;;)
    {
	if (delta)
	{
	    putxy(54,row+num,prevent(*list)?"UP":"--");			/* go up? */
	    putxy(57,row+num,ofsent(num,*list)?"DOWN":"----");		/* go down? */
	    showparams(ofsent(*ofs,*list));				/* show device parameters */
	    drawline(row+*ofs,detail,ofsent(*ofs,*list),1,dhelp);	/* highlight current line */
	}

	c = getchar();				/* get a character */
	if ((extended == 2) || (c==588) || (c==596))	/* console gives "alternative" codes */
	{
	    extended = 0;			/* no longer */
	    switch(c)
	    {
	    case 588:				/* syscons' idea of 'up' */
	    case 'A':				/* up */
		if (*ofs)			/* just a move onscreen */
		{
		    drawline(row+*ofs,detail,ofsent(*ofs,*list),0,dhelp);/* unhighlight current line */
		    (*ofs)--;			/* move up */
		}else{
		    lp = prevent(*list);	/* can we go up? */
		    if (!lp)			/* no */
			break;
		    *list = lp;			/* yes, move up list */
		    drawlist(row,num,detail,*list);
		}
		delta = 1;
		break;

	    case 596:				/* dooby-do */
	    case 'B':				/* down */
		lp = ofsent(*ofs,*list);	/* get current item */
		if (!nextent(lp))
		    break;			/* nothing more to move to */
		drawline(row+*ofs,detail,ofsent(*ofs,*list),0,dhelp);	/* unhighlight current line */
		if (*ofs < (num-1))		/* room to move onscreen? */
		{
		    (*ofs)++;		    
		}else{
		    *list = nextent(*list);	/* scroll region down */
		    drawlist(row,num,detail,*list);
		}		
		delta = 1;
		break;
	    }
	}else{
	    switch(c)
	    {
	    case '\033':
		extended=1;
		break;
		    
	    case '[':				/* cheat : always preceeds cursor move */
		if (extended==1)
		    extended=2;
		else
		    extended=0;
		break;
		
	    case 'Q':
	    case 'q':
		return(KEY_EXIT);		/* user requests exit */

	    case '\r':				
	    case '\n':
		return(KEY_DO);			/* "do" something */

	    case '\b':
	    case '\177':
	    case 599:
		return(KEY_DEL);		/* "delete" response */

	    case '\t':
		drawline(row+*ofs,detail,ofsent(*ofs,*list),0,dhelp);	/* unhighlight current line */
		return(KEY_TAB);				/* "move" response */
		
	    case '\014':			/* ^L, redraw */
		return(KEY_REDRAW);
	    }
	}
    }		
}


/**
 ** userconfig
 ** 
 ** Do the config thang
 **/
void 
userconfig(void)
{
    int	actofs = 0, inactofs = 0, mode = 0, ret = -1, i;
    DEV_LIST	*dp;
    
    initlist(&active);
    initlist(&inactive);
    alist = active;
    ilist = inactive;

#ifndef KERNEL
    fakedevs();
#else
    getdevs();
#endif

    conflicts = findconflict(active);		/* find conflicts in the active list only */

    redraw();

    for(;;)
    {
	switch(mode)
	{
	case 0:					/* active devices */
	    ret = dolist(1,7,1,&actofs,&alist,
			 "  [!bEnter!n] edit device parameters  [!bDEL!n] disable device");
	    switch(ret)
	    {
	    case KEY_TAB:
		mode = 1;			/* swap lists */
		break;

	    case KEY_REDRAW:
		redraw();
		break;

	    case KEY_DEL:

		dp = ofsent(actofs,active);	/* get current device */
		if (dp)				/* paranoia... */
		{
		    if (dp->attrib & FLG_MANDATORY)	/* can't be deleted */
			break;
		    if (dp == alist)		/* moving top item on list? */
		    {
			if (dp->next)
			{
			    alist = dp->next;	/* point list to non-moving item */
			}else{
			    alist = dp->prev;	/* end of list, go back instead */
			}
		    }else{
			if (!dp->next)		/* moving last item on list? */
			    actofs--;
		    }
		    dp->conflicts = 0;		/* no conflicts on the inactive list */
		    movedev(dp,inactive);	/* shift to inactive list */
		    conflicts = findconflict(active);	/* update conflict tags */
#ifdef KERNEL
		    dp->changed = 1;
#endif
		    redraw();			/* redraw */
		}
		break;
		
	    case KEY_DO:			/* edit device parameters */
		dp = ofsent(actofs,alist);	/* get current device */
		if (dp)				/* paranoia... */
		{
		    if (dp->comment == DEV_DEVICE)	/* can't edit comments, zoom? */
		    {
			editparams(dp);
			conflicts = findconflict(active);	/* update conflict tags */

		    }else{				/* DO on comment = zoom */
			switch(dp->comment)		/* Depends on current state */
			{
			case DEV_COMMENT:		/* not currently zoomed */
			    dp->comment = DEV_ZOOMED;
			    break;

			case DEV_ZOOMED:
			    dp->comment = DEV_COMMENT;
			    if (dp->next)
				if (dp->next->comment == DEV_DEVICE)
				{
				    actofs = 0;		/* put at top of region */
				    alist = dp;
				}
			    break;
			}
		    }
		    redraw();
		}
		break;
	    }
	    break;

	case 1:					/* inactive devices */
	    ret = dolist(10,7,0,&inactofs,&ilist,
			 "  [!bEnter!n] enable device                                   ");
	    switch(ret)
	    {
	    case KEY_TAB:
		mode = 0;
		break;

	    case KEY_REDRAW:
		redraw();
		break;

	    case KEY_DO:
		dp = ofsent(inactofs,ilist);	/* get current device */
		if (dp)				/* paranoia... */
		{
		    if (dp->comment == DEV_DEVICE)	/* can't move comments, zoom? */
		    {
			if (dp == ilist)		/* moving top of list? */
			{
			    if (dp->next)
			    {
				ilist = dp->next;	/* point list to non-moving item */
			    }else{
				ilist = dp->prev;	/* can't go down, go up instead */
			    }
			}else{
			    if (!dp->next)		/* last entry on list? */
				inactofs--;		/* shift cursor up one */
			}

			movedev(dp,active);		/* shift to active list */
			conflicts = findconflict(active);	/* update conflict tags */
#ifdef KERNEL
			dp->changed = 1;
#endif
			alist = dp;			/* put at top and current */
			actofs = 0;
			while(dp->comment == DEV_DEVICE)
			    dp = dp->prev;		/* forcibly unzoom section */
			dp ->comment = DEV_COMMENT;
			mode = 0;			/* and swap modes to follow it */

		    }else{				/* DO on comment = zoom */
			switch(dp->comment)		/* Depends on current state */
			{
			case DEV_COMMENT:		/* not currently zoomed */
			    dp->comment = DEV_ZOOMED;
			    break;

			case DEV_ZOOMED:
			    dp->comment = DEV_COMMENT;
			    if (dp->next)
				if (dp->next->comment == DEV_DEVICE)
				{
				    inactofs = 0;		/* put at top of region */
				    ilist = dp;
				}
			    break;
			}
		    }
		    redraw();			/* redraw */
		}
		break;

	    default:				/* nothing else relevant here */
		break;
	    }
	    break;
	default:
	    mode = 0;				/* shouldn't happen... */
	}
	if (ret == KEY_EXIT)
	{
	    i = yesnocancel(" Save these parameters and exit? (Yes/No/Cancel) ");
	    switch(i)
	    {
	    case 2:				/* cancel */
		redraw();
		break;
		
	    case 1:				/* save and exit */
		savelist(active,1);
		savelist(inactive,0);

	    case 0:				/* exit */
		nukelist(active);		/* clean up after ourselves */
		nukelist(inactive);
		normal();
		clear();
		return;
	    }
	}
    }
}


/** All code below is for testing as a usermode program.  XXX This should go away. **/

#ifndef KERNEL
void 
main(int argc, char *argv[])
{
    struct termios t,u;

    tcgetattr(fileno(stdin),&t);
    cfmakeraw(&u);
    tcsetattr(fileno(stdin),TCSANOW,&u);
    userconfig();
    tcsetattr(fileno(stdin),TCSANOW,&t);
}

void 
fakedevs(void)
{
    strcpy(scratch.name,"Half-phase blurglecruncheon detector");
    strcpy(scratch.dev,"bcd0");
    scratch.class = CLS_STORAGE;
    scratch.iobase = 0x100;
    scratch.irq = ffs(4);
    scratch.maddr = 0x1000;
    scratch.msize = 0x400;
    scratch.unit = 0;
    scratch.flags = 0;
    scratch.conflict_ok = 0;
    scratch.comment = 0;
    scratch.attrib = FLG_MANDATORY;
    insdev(&scratch,active);

    strcpy(scratch.name,"Half-phase blurglecruncheon detector");
    strcpy(scratch.dev,"bcd1");
    scratch.class = CLS_NETWORK;
    scratch.iobase = 0x100;
    scratch.irq = ffs(8);
    scratch.maddr = 0x1000;
    scratch.msize = 0x400;
    scratch.unit = 1;
    scratch.flags = 0;
    scratch.conflict_ok = 0;
    scratch.comment = 0;
    scratch.attrib = 0;
    insdev(&scratch,active);

    strcpy(scratch.name,"Half-phase blurglecruncheon detector");
    strcpy(scratch.dev,"bcd2");
    scratch.class = CLS_COMMS;
    scratch.iobase = 0x100;
    scratch.irq = ffs(16);
    scratch.maddr = 0x1000;
    scratch.msize = 0x400;
    scratch.unit = 2;
    scratch.flags = 0;
    scratch.conflict_ok = 0;
    scratch.comment = 0;
    scratch.attrib = FLG_IMMUTABLE;
    insdev(&scratch,active);

    strcpy(scratch.name,"Half-phase blurglecruncheon detector");
    strcpy(scratch.dev,"bcd3");
    scratch.class = CLS_INPUT;
    scratch.iobase = 0x100;
    scratch.irq = ffs(32);
    scratch.maddr = 0x1000;
    scratch.msize = 0x400;
    scratch.unit = 3;
    scratch.flags = 0;
    scratch.conflict_ok = 0;
    scratch.comment = 0;
    scratch.attrib = FLG_FIXED;
    insdev(&scratch,active);

    strcpy(scratch.name,"Half-phase blurglecruncheon detector");
    strcpy(scratch.dev,"bcd4");
    scratch.class = CLS_MMEDIA;
    scratch.iobase = 0x100;
    scratch.irq = ffs(64);
    scratch.maddr = 0x1000;
    scratch.msize = 0x400;
    scratch.unit = 4;
    scratch.flags = 0;
    scratch.conflict_ok = 0;
    scratch.comment = 0;
    scratch.attrib = 0;
    insdev(&scratch,active);

    strcpy(scratch.name,"Half-phase blurglecruncheon detector");
    strcpy(scratch.dev,"bcd5");
    scratch.class = CLS_MISC;
    scratch.iobase = 0x100;
    scratch.irq = ffs(128);
    scratch.maddr = 0x1000;
    scratch.msize = 0x400;
    scratch.unit = 5;
    scratch.flags = 0;
    scratch.conflict_ok = 0;
    scratch.comment = 0;
    scratch.attrib = 0;
    insdev(&scratch,active);
}

#endif

