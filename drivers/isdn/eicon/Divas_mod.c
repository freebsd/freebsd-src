/*
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/fs.h>
#undef N_DATA

#include <linux/kernel.h>

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include "adapter.h"
#include "uxio.h"


MODULE_DESCRIPTION("ISDN4Linux: Driver for Eicon Diva Server cards");
MODULE_AUTHOR("Armin Schindler");
MODULE_LICENSE("GPL");

#ifdef MODULE
#include "idi.h"
void DIVA_DIDD_Write(DESCRIPTOR *, int);
EXPORT_SYMBOL_NOVERS(DIVA_DIDD_Read);
EXPORT_SYMBOL_NOVERS(DIVA_DIDD_Write);
EXPORT_SYMBOL_NOVERS(DivasPrintf);
#endif

int DivasCardsDiscover(void);

static int __init
divas_init(void)
{
	printk(KERN_DEBUG "DIVA Server Driver - initialising\n");
	
	printk(KERN_DEBUG "DIVA Server Driver - Version 2.0.16\n");

#if !defined(CONFIG_PCI)
	printk(KERN_WARNING "CONFIG_PCI is not defined!\n");
	return -ENODEV;
#endif

	if (pci_present())
	{
		if (DivasCardsDiscover() < 0)
		{
			printk(KERN_WARNING "Divas: Not loaded\n");
			return -ENODEV;
		}
	}
	else
	{
		printk(KERN_WARNING "Divas: No PCI bus present\n");
		return -ENODEV;
	}

    return 0;
}

static void __exit
divas_exit(void)
{
	card_t *pCard;
	word wCardIndex;
	extern int Divas_major;

	printk(KERN_DEBUG "DIVA Server Driver - unloading\n");

	pCard = DivasCards;
	for (wCardIndex = 0; wCardIndex < MAX_CARDS; wCardIndex++)
	{
		if ((pCard->hw) && (pCard->hw->in_use))
		{

			(*pCard->card_reset)(pCard);
			
			UxIsrRemove(pCard->hw, pCard);
			UxCardHandleFree(pCard->hw);

			if(pCard->e_tbl != NULL)
			{
				kfree(pCard->e_tbl);
			}

			
			if(pCard->hw->card_type == DIA_CARD_TYPE_DIVA_SERVER_B)
			{	
				release_region(pCard->hw->io_base,0x20);		
				release_region(pCard->hw->reset_base,0x80);		
			}

			// If this is a 4BRI ...
			if (pCard->hw->card_type == DIA_CARD_TYPE_DIVA_SERVER_Q)
			{
				// Skip over the next 3 virtual adapters
				wCardIndex += 3;

				// But free their handles 
				pCard++;
				UxCardHandleFree(pCard->hw);
			
				if(pCard->e_tbl != NULL)
				{
					kfree(pCard->e_tbl);
				}
				
				pCard++;
				UxCardHandleFree(pCard->hw);
				
				if(pCard->e_tbl != NULL)
				{
					kfree(pCard->e_tbl);
				}
				
				pCard++;
				UxCardHandleFree(pCard->hw);
				
				if(pCard->e_tbl != NULL)
				{
					kfree(pCard->e_tbl);
				}
			}
		}
		pCard++;
	}

	unregister_chrdev(Divas_major, "Divas");
}

module_init(divas_init);
module_exit(divas_exit);

