/*
 * PC-card support for sysinstall
 *
 * $Id: pccard.c,v 1.4 1999/07/04 15:54:14 hosokawa Exp $
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
#include "pccard_conf.h"
#include <sys/fcntl.h>
#include <sys/time.h>
#include <pccard/cardinfo.h>

#ifdef	PCCARD

int	pccard_mode = 0;

DMenu MenuPCICMem = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select free address area used by PC-card controller",
    "PC-card controller uses memory area to get card information.\n"
    "Please specify an address that is not used by other devices.\n"
    "If you're uncertain of detailed specification of your hardware,\n"
    "leave it untouched (default == 0xd0000).",
    "Press F1 for more HELP",
    "pccard",
    {	{ "Default",  "I/O address 0xd0000 - 0xd3fff",
	    NULL, dmenuSetVariable, NULL, "_pcicmem=0"},
	{ "D4", "I/O address 0xd4000 - 0xd7fff",
	    NULL, dmenuSetVariable, NULL, "_pcicmem=1"},
	{ "D8", "I/O address 0xd8000 - 0xdbfff",
	    NULL,  dmenuSetVariable, NULL, "_pcicmem=2"},
	{ "DC", "I/O address 0xdc000 - 0xdffff",
	    NULL,  dmenuSetVariable, NULL, "_pcicmem=3"},
	{ NULL } },
};

DMenu MenuCardIRQ = {
    DMENU_NORMAL_TYPE | DMENU_SELECTION_RETURNS,
    "Please select IRQs that can be used by PC-cards",
    "Please specify an IRQs that CANNOT be used by PC-card.\n"
    "For example, if you have a sound card that can't be probed by\n"
    "this installation floppy and it uses IRQ 10, you have to \n"
    "choose \"Option 1\" or \"Option 2\" at this menu.\n",
    "Press F1 for more HELP",
    "pccard",
    {	{ "Default",  "IRQ 10, 11",
	    NULL, dmenuSetVariable, NULL, "_cardirq=0"},
	{ "Option 1", "IRQ 5, 11 (ex. soundcard on IRQ 10)",
	    NULL, dmenuSetVariable, NULL, "_cardirq=1"},
	{ "Option 2", "IRQ 11 (ex. something on IRQ 5 and 10)",
	    NULL, dmenuSetVariable, NULL, "_cardirq=2"},
	{ NULL } },
};

void
pccardInitialize(void)
{
    int fd;
    int t;
    int	pcic_mem = 0xd0000;
    char card_device[16];
    char *card_irq = "";
    char *spcic_mem;
    char *scard_irq;
    char pccardd_flags[128];
    char pccardd_cmd[256];

    pccard_mode = 1;
    
    if (!RunningAsInit && !Fake) {
	/* It's not my job... */
	return;
    }

    dmenuOpenSimple(&MenuPCICMem, FALSE);
    spcic_mem = variable_get("_pcicmem");
    dmenuOpenSimple(&MenuCardIRQ, FALSE);
    scard_irq = variable_get("_cardirq");

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
    }

    sscanf(scard_irq, "%d", &t);

    switch (t) {
      case 0:
	card_irq = "-i 10 -i 11";
	break;
      case 1:
	card_irq = "-i 5 -i 11";
	break;
      case 2:
	card_irq = "-i 11";
	break;
    }

    sprintf(card_device, CARD_DEVICE, 0);
    
    dialog_clear();
    msgConfirm("Now starts initializing PC-card controller and cards.\n"
	       "If you've executed this installer from PC-card floppy\n"
	       "drive, this is the last chance to replace it with\n"
	       "installation media (PC-card Ethernet, CDROM, etc.).\n"
	       "Please insert installation media and press [Enter].\n"
	       "If you've not plugged the PC-card installation media\n"
	       "yet, please plug it now and press [Enter].\n"
	       "Otherwise, just press [Enter] to proceed."); 

    dialog_clear();
    msgNotify("Initializing PC-card controller....");
    
    if (!Fake) {
	if ((fd = open(card_device, O_RDWR)) < 1) {
	    msgNotify("Can't open PC-card controller %s.\n", 
		      card_device);
	    return;
	}

	if (ioctl(fd, PIOCRWMEM, &pcic_mem) < 0){
	    msgNotify("ioctl %s failed.\n", card_device);
	    return;
	}
    }

    strcpy(pccardd_cmd, "/stand/pccardd ");
    strcat(pccardd_cmd, card_irq);
    strcat(pccardd_cmd, " -z");

    strcpy(pccardd_flags, card_irq);
    variable_set2("pccardd_flags", card_irq, 1);

    vsystem(pccardd_cmd);
}

#endif	/* PCCARD */


