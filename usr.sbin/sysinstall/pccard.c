/*
 * PC Card support for sysinstall
 *
 * $FreeBSD$
 *
 * Copyright (c) 1997-1999
 *	Tatsumi Hosokawa <hosokawa@jp.FreeBSD.org>.  All rights reserved.
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its
 * use.
 */

#include "sysinstall.h"
#include <sys/fcntl.h>
#include <sys/time.h>
#include <pccard/cardinfo.h>

int	pccard_mode = 0;

/*
 * Set up defines for pccardd interrupt selection.
 */
#ifdef PC98
#define IRQ_COUNT	11
#else
#define IRQ_COUNT	9
#endif /* PC98 */
#define IRQ_10		0x00001
#define IRQ_11		0x00002
#define IRQ_03		0x00004
#define IRQ_09		0x00008
#define IRQ_04		0x00010
#define IRQ_07		0x00020
#define IRQ_05		0x00040
#define IRQ_06		0x00080
#define IRQ_15		0x00100
#ifdef PC98
#define IRQ_12		0x00200
#define IRQ_13		0x00400
#endif /* PC98 */

unsigned int CardIrq;

typedef struct _irq {
    char *my_name;
    char *my_flag;
    unsigned int my_mask;
    unsigned int my_bit;
} Irq;

/* Fill in with potential free IRQs for pccardd */
static Irq IrqTable[] = {
    { "irq_03", "-i 3",  ~IRQ_03, IRQ_03 },
    { "irq_04", "-i 4",  ~IRQ_04, IRQ_04 },
    { "irq_05", "-i 5",  ~IRQ_05, IRQ_05 },
    { "irq_06", "-i 6",  ~IRQ_06, IRQ_06 },
    { "irq_07", "-i 7",  ~IRQ_07, IRQ_07 },
    { "irq_09", "-i 9",  ~IRQ_09, IRQ_09 },
    { "irq_10", "-i 10", ~IRQ_10, IRQ_10 },
    { "irq_11", "-i 11", ~IRQ_11, IRQ_11 },
    { "irq_15", "-i 15", ~IRQ_15, IRQ_15 },
#ifdef PC98
    { "irq_12", "-i 12", ~IRQ_12, IRQ_12 },
    { "irq_13", "-i 13", ~IRQ_13, IRQ_13 },
#endif /* PC98 */
    {NULL},
};

int
pccardIrqReset(dialogMenuItem *self)
{
    CardIrq = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
checkTrue(dialogMenuItem *item)
{
    return TRUE;
}

DMenu MenuPCICMem = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select free address area used by PC Card controller",
    "PC Card controller uses memory area to get card information.\n"
    "Please specify an address that is not used by other devices.\n"
    "If you're uncertain of detailed specification of your hardware,\n"
#ifdef PC98
    "leave it untouched (default == 0xd0000).\n"
    "If you use PC-9801 P, NS/A, NX/C, NL/R or PC-9821 Ne please \n"
    "select [DA] here.",
#else
    "leave it untouched (default == 0xd0000).",
#endif /* PC98 */
    "Press F1 for more HELP",
    "pccard",
    { { "Default",  "I/O address 0xd0000 - 0xd3fff",
	NULL, dmenuSetVariable, NULL, "_pcicmem=0"},
      { "D4", "I/O address 0xd4000 - 0xd7fff",
	NULL, dmenuSetVariable, NULL, "_pcicmem=1"},
      { "D8", "I/O address 0xd8000 - 0xdbfff",
	NULL,  dmenuSetVariable, NULL, "_pcicmem=2"},
      { "DC", "I/O address 0xdc000 - 0xdffff",
	NULL,  dmenuSetVariable, NULL, "_pcicmem=3"},
#ifdef PC98
      { "DA", "I/O address 0xda000 - 0xdbfff",
	NULL,  dmenuSetVariable, NULL, "_pcicmem=4"},
#endif /* PC98 */
      { NULL } },
};

DMenu MenuCardIRQ = {
    DMENU_CHECKLIST_TYPE | DMENU_SELECTION_RETURNS,
    "Please specify the IRQs that may be used by PC Cards",
    "(NOTE: remove any cards that will NOT be used for installation).\n"
    "The IRQs that you choose must be free (unshared), or you risk \n"
    "subpar performance and/or a complete system lockup (choose wisely).\n"
    "One way to determine which IRQs are available is to \"cheat\" and\n"
    "use the Windows 9x/2000 Device Manager as a reference prior to the\n"
    "installation.\n",
    "Select Free IRQ for pccardd",
    NULL,
    { { "X Exit",    "Exit this menu",
	checkTrue, dmenuExit, NULL, NULL, '<', '<', '<' }, 
    { "Reset",     "Reset selected IRQ list",
	NULL, pccardIrqReset, NULL, NULL, ' ', ' ', ' ' },
#ifdef PC98
    { "3 IRQ 3",    "(INT 0) is 2nd serial port, internal modem",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_03 },
    { "4 IRQ 5",    "(INT 1) is Infrared communication, SCSI I/F, (2nd serial)",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_05 },
    { "5 IRQ 6",    "(INT 2) is PC Card Controller",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_06 },
    { "6 IRQ 9",    "(INT 3) is IDE disk Controller",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_09 },
    { "7 IRQ 10",   "(INT 41) is often free",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_10 },
    { "8 IRQ 12",   "(INT 5) is Internal sound",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_12 },
    { "9 IRQ 13",   "(INT 6) is Bus Mouse",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_13 },
#else
    { "3 IRQ 10",   "IRQ 10 is often free (verify in BIOS)",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_10 },
    { "4 IRQ 11",   "Verify IRQ 11 is not being used as PCI shared interrupt",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_11 },
    { "5 IRQ 3",   "IRQ 3 is often free in most laptops",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_03 },
    { "6 IRQ 9",   "IRQ 9 may be used by video or sound or USB",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_09 },
    { "7 IRQ 4",   "IRQ 4, usually COM1 (disable in BIOS to make free)",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_04 },
    { "8 IRQ 7",   "IRQ 7, usually LPT1 (disable in BIOS to make free)",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_07 },
    { "9 IRQ 5",   "IRQ 5, usually ISA Audio (disable in BIOS to make free)", 
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_05 },
    { "10 IRQ 15", "IRQ 15, usually Secondary IDE channel",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_15 },
    { "11 IRQ 6",  "IRQ 6 will be free if laptop only has USB floppy drive",
	dmenuFlagCheck, dmenuSetFlag, NULL, &CardIrq, '[', 'X', ']', IRQ_06 },
#endif /* PC98 */
    { NULL } },
};

void
pccardInitialize(void)
{
    int fd;
    int t;
    int i;
    int	pcic_mem = 0xd0000;
    int beep_newstat;
    char card_device[16];
    char card_irq[256] = "";
    char temp[256];
    char *spcic_mem;
    char pccardd_cmd[256];
    WINDOW *w;
    
    pccard_mode = 1;
    
    if (!RunningAsInit && !Fake) {
	/* It's not my job... */
	return;
    }

    sprintf(card_device, CARD_DEVICE, 0);

    if ((fd = open(card_device, O_RDWR)) < 0) {
	msgDebug("Can't open PC Card controller %s.\n", card_device);
	return;
    }
    else if (msgYesNo("Found PC Card slot(s).\n"
		      "Use PC Card device as installation media?\n")) {
	return;
    }
    close(fd);

    variable_set2("_pccard_install", "YES", 0);

    dmenuOpenSimple(&MenuPCICMem, FALSE);
    spcic_mem = variable_get("_pcicmem");
    dmenuOpenSimple(&MenuCardIRQ, FALSE);

    sscanf(spcic_mem, "%d", &t);
    switch (t) {
      case 0:
	pcic_mem = 0xd0000;
	variable_set2("pccard_mem", "DEFAULT", 1);
	break;
      case 1:
	pcic_mem = 0xd4000;
	variable_set2("pccard_mem", "0xd4000", 1);
	break;
      case 2:
	pcic_mem = 0xd8000;
	variable_set2("pccard_mem", "0xd8000", 1);
	break;
      case 3:
	pcic_mem = 0xdc000;
	variable_set2("pccard_mem", "0xdc000", 1);
	break;
#ifdef PC98
      case 4:
	pcic_mem = 0xda000;
	variable_set2("pccard_mem", "0xda000", 1);
	break;
#endif /* PC98 */
    }

    /* get card_irq out of CardIrq somehow */
    if (CardIrq) {
        for (i = 0; i < IRQ_COUNT; i++) {
            if ((CardIrq & IrqTable[i].my_bit) != 0) {
                sprintf(temp, "%s %s", card_irq, IrqTable[i].my_flag);
                strcpy(card_irq, temp);
            }
        } 
    }

    w = savescr();
    dialog_clear_norefresh();
    msgConfirm("Now we start initializing PC Card controller and cards.\n"
	       "If you've executed this installer from a PC Card floppy\n"
	       "drive, this is the last chance to replace it with\n"
	       "installation media (PC Card Ethernet, CD, DVD, etc.).\n"
	       "Please insert installation media and press [Enter].\n"
	       "If you've not plugged the PC Card installation media\n"
	       "in yet, please plug it in now and press [Enter].\n"
	       "Otherwise, just press [Enter] to proceed."); 

    dialog_clear();
    msgNotify("Initializing PC Card controller....");

    if (!Fake) {
	if ((fd = open(card_device, O_RDWR)) < 1) {
	    msgNotify("Can't open PC Card controller %s.\n", card_device);
	    restorescr(w);
	    return;
	}

	if (ioctl(fd, PIOCRWMEM, &pcic_mem) < 0) {
	    msgNotify("ioctl %s failed.\n", card_device);
	    restorescr(w);
	    return;
	}
	beep_newstat = 2;
	if (ioctl(fd, PIOCSBEEP, &beep_newstat) < 0) {
	    msgNotify("Warning: unable to set pccard insertion beep type for %s",
		card_device);
	    restorescr(w);
	    return;
	}

    }

    strcpy(pccardd_cmd, "/stand/pccardd ");
    strcat(pccardd_cmd, card_irq);
    strcat(pccardd_cmd, " -z");

    variable_set2("pccardd_flags", card_irq, 1);
    variable_set2("pccard_enable", "YES", 1);

    vsystem("%s", pccardd_cmd);
    restorescr(w);
}
