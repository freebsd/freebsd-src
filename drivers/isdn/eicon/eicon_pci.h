/* $Id: eicon_pci.h,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * ISDN low-level module for Eicon active ISDN-Cards (PCI part).
 *
 * Copyright 1998-2000 by Armin Schindler (mac@melware.de)
 * Copyright 1999,2000 Cytronics & Melware (info@melware.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef eicon_pci_h
#define eicon_pci_h

#ifdef __KERNEL__

/*
 * card's description
 */
typedef struct {
	int   		  irq;	    /* IRQ		          */
	int		  channels; /* No. of supported channels  */
        void*             card;
        unsigned char     type;     /* card type                  */
        unsigned char     master;   /* Flag: Card is Quadro 1/4   */
} eicon_pci_card;

extern int eicon_pci_find_card(char *ID);

#endif  /* __KERNEL__ */

#endif	/* eicon_pci_h */

