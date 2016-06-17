/* $Id: eicon_pci.c,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * ISDN low-level module for Eicon active ISDN-Cards.
 * Hardware-specific code for PCI cards.
 *
 * Copyright 1998-2000 by Armin Schindler (mac@melware.de)
 * Copyright 1999,2000 Cytronics & Melware (info@melware.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Thanks to	Eicon Networks for 
 *		documents, informations and hardware. 
 *
 */

#include <linux/config.h>
#include <linux/pci.h>

#include "eicon.h"
#include "eicon_pci.h"

#undef N_DATA
#include "adapter.h"
#include "uxio.h"

char *eicon_pci_revision = "$Revision: 1.1.4.1 $";

#if CONFIG_PCI	         /* intire stuff is only for PCI */
#ifdef CONFIG_ISDN_DRV_EICON_PCI

int eicon_pci_find_card(char *ID)
{
	int pci_cards = 0;
	int card_id = 0;
	int had_q = 0;
	int ctype = 0;
	char did[20];
	card_t *pCard;
	word wCardIndex;

	pCard = DivasCards;
	for (wCardIndex = 0; wCardIndex < MAX_CARDS; wCardIndex++)
	{
	if ((pCard->hw) && (pCard->hw->in_use))
		{
			switch(pCard->hw->card_type) {
				case DIA_CARD_TYPE_DIVA_SERVER:
					ctype = EICON_CTYPE_MAESTRAP;
					card_id++;
					had_q = 0;
					break;
				case DIA_CARD_TYPE_DIVA_SERVER_B:
					ctype = EICON_CTYPE_MAESTRA;
					card_id++;
					had_q = 0;
					break;
				case DIA_CARD_TYPE_DIVA_SERVER_Q:
					ctype = EICON_CTYPE_MAESTRAQ;
					if (!had_q)
						card_id++;
					if (++had_q >=4)
						had_q = 0;
					break;
				default:
					printk(KERN_ERR "eicon_pci: unknown card type %d !\n",
						pCard->hw->card_type);
					goto err;
			}
			sprintf(did, "%s%d", (strlen(ID) < 1) ? "eicon":ID, pci_cards);
			if ((!ctype) || (!(eicon_addcard(ctype, 0, pCard->hw->irq, did, card_id)))) {
				printk(KERN_ERR "eicon_pci: Card could not be added !\n");
			} else {
				pci_cards++;
				printk(KERN_INFO "%s: DriverID='%s' CardID=%d\n",
					eicon_ctype_name[ctype], did, card_id);
			}
err:;
		}
		pCard++;
	}
	return pci_cards;
}

void
eicon_pci_init_conf(eicon_card *card)
{
	int j;

	/* initializing some variables */
	card->ReadyInt = 0;

	for(j = 0; j < 256; j++)
		card->IdTable[j] = NULL;

	for(j = 0; j < (card->d->channels + 1); j++) {
		card->bch[j].e.busy = 0;
		card->bch[j].e.D3Id = 0;
		card->bch[j].e.B2Id = 0;
		card->bch[j].e.ref = 0;
		card->bch[j].e.Req = 0;
		card->bch[j].e.complete = 1;
		card->bch[j].fsm_state = EICON_STATE_NULL;
	}
}

#endif
#endif	/* CONFIG_PCI */

