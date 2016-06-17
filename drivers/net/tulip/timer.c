/*
	drivers/net/tulip/timer.c

	Maintained by Jeff Garzik <jgarzik@pobox.com>
	Copyright 2000,2001  The Linux Kernel Team
	Written/copyright 1994-2001 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	Please refer to Documentation/DocBook/tulip.{pdf,ps,html}
	for more information on this driver, or visit the project
	Web page at http://sourceforge.net/projects/tulip/

*/

#include "tulip.h"


void tulip_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct tulip_private *tp = (struct tulip_private *)dev->priv;
	long ioaddr = dev->base_addr;
	u32 csr12 = inl(ioaddr + CSR12);
	int next_tick = 2*HZ;

	if (tulip_debug > 2) {
		printk(KERN_DEBUG "%s: Media selection tick, %s, status %8.8x mode"
			   " %8.8x SIA %8.8x %8.8x %8.8x %8.8x.\n",
			   dev->name, medianame[dev->if_port], inl(ioaddr + CSR5),
			   inl(ioaddr + CSR6), csr12, inl(ioaddr + CSR13),
			   inl(ioaddr + CSR14), inl(ioaddr + CSR15));
	}
	switch (tp->chip_id) {
	case DC21040:
		if (!tp->medialock  &&  csr12 & 0x0002) { /* Network error */
			printk(KERN_INFO "%s: No link beat found.\n",
				   dev->name);
			dev->if_port = (dev->if_port == 2 ? 0 : 2);
			tulip_select_media(dev, 0);
			dev->trans_start = jiffies;
		}
		break;
	case DC21041:
		if (tulip_debug > 2)
			printk(KERN_DEBUG "%s: 21041 media tick  CSR12 %8.8x.\n",
				   dev->name, csr12);
		if (tp->medialock) break;
		switch (dev->if_port) {
		case 0: case 3: case 4:
		  if (csr12 & 0x0004) { /*LnkFail */
			/* 10baseT is dead.  Check for activity on alternate port. */
			tp->mediasense = 1;
			if (csr12 & 0x0200)
				dev->if_port = 2;
			else
				dev->if_port = 1;
			printk(KERN_INFO "%s: No 21041 10baseT link beat, Media switched to %s.\n",
				   dev->name, medianame[dev->if_port]);
			outl(0, ioaddr + CSR13); /* Reset */
			outl(t21041_csr14[dev->if_port], ioaddr + CSR14);
			outl(t21041_csr15[dev->if_port], ioaddr + CSR15);
			outl(t21041_csr13[dev->if_port], ioaddr + CSR13);
			next_tick = 10*HZ;			/* 2.4 sec. */
		  } else
			next_tick = 30*HZ;
		  break;
		case 1:					/* 10base2 */
		case 2:					/* AUI */
			if (csr12 & 0x0100) {
				next_tick = (30*HZ);			/* 30 sec. */
				tp->mediasense = 0;
			} else if ((csr12 & 0x0004) == 0) {
				printk(KERN_INFO "%s: 21041 media switched to 10baseT.\n",
					   dev->name);
				dev->if_port = 0;
				tulip_select_media(dev, 0);
				next_tick = (24*HZ)/10;				/* 2.4 sec. */
			} else if (tp->mediasense || (csr12 & 0x0002)) {
				dev->if_port = 3 - dev->if_port; /* Swap ports. */
				tulip_select_media(dev, 0);
				next_tick = 20*HZ;
			} else {
				next_tick = 20*HZ;
			}
			break;
		}
		break;
	case DC21140:
	case DC21142:
	case MX98713:
	case COMPEX9881:
	case DM910X:
	default: {
		struct medialeaf *mleaf;
		unsigned char *p;
		if (tp->mtable == NULL) {	/* No EEPROM info, use generic code. */
			/* Not much that can be done.
			   Assume this a generic MII or SYM transceiver. */
			next_tick = 60*HZ;
			if (tulip_debug > 2)
				printk(KERN_DEBUG "%s: network media monitor CSR6 %8.8x "
					   "CSR12 0x%2.2x.\n",
					   dev->name, inl(ioaddr + CSR6), csr12 & 0xff);
			break;
		}
		mleaf = &tp->mtable->mleaf[tp->cur_index];
		p = mleaf->leafdata;
		switch (mleaf->type) {
		case 0: case 4: {
			/* Type 0 serial or 4 SYM transceiver.  Check the link beat bit. */
			int offset = mleaf->type == 4 ? 5 : 2;
			s8 bitnum = p[offset];
			if (p[offset+1] & 0x80) {
				if (tulip_debug > 1)
					printk(KERN_DEBUG"%s: Transceiver monitor tick "
						   "CSR12=%#2.2x, no media sense.\n",
						   dev->name, csr12);
				if (mleaf->type == 4) {
					if (mleaf->media == 3 && (csr12 & 0x02))
						goto select_next_media;
				}
				break;
			}
			if (tulip_debug > 2)
				printk(KERN_DEBUG "%s: Transceiver monitor tick: CSR12=%#2.2x"
					   " bit %d is %d, expecting %d.\n",
					   dev->name, csr12, (bitnum >> 1) & 7,
					   (csr12 & (1 << ((bitnum >> 1) & 7))) != 0,
					   (bitnum >= 0));
			/* Check that the specified bit has the proper value. */
			if ((bitnum < 0) !=
				((csr12 & (1 << ((bitnum >> 1) & 7))) != 0)) {
				if (tulip_debug > 2)
					printk(KERN_DEBUG "%s: Link beat detected for %s.\n", dev->name,
					       medianame[mleaf->media & MEDIA_MASK]);
				if ((p[2] & 0x61) == 0x01)	/* Bogus Znyx board. */
					goto actually_mii;
				/* netif_carrier_on(dev); */
				break;
			}
			/* netif_carrier_off(dev); */
			if (tp->medialock)
				break;
	  select_next_media:
			if (--tp->cur_index < 0) {
				/* We start again, but should instead look for default. */
				tp->cur_index = tp->mtable->leafcount - 1;
			}
			dev->if_port = tp->mtable->mleaf[tp->cur_index].media;
			if (tulip_media_cap[dev->if_port] & MediaIsFD)
				goto select_next_media; /* Skip FD entries. */
			if (tulip_debug > 1)
				printk(KERN_DEBUG "%s: No link beat on media %s,"
				       " trying transceiver type %s.\n",
				       dev->name, medianame[mleaf->media & MEDIA_MASK],
				       medianame[tp->mtable->mleaf[tp->cur_index].media]);
			tulip_select_media(dev, 0);
			/* Restart the transmit process. */
			tulip_restart_rxtx(tp);
			next_tick = (24*HZ)/10;
			break;
		}
		case 1:  case 3:		/* 21140, 21142 MII */
		actually_mii:
			if (tulip_check_duplex(dev) < 0)
				{ /* netif_carrier_off(dev); */ }
			else
				{ /* netif_carrier_on(dev); */ }
			next_tick = 60*HZ;
			break;
		case 2:					/* 21142 serial block has no link beat. */
		default:
			break;
		}
	}
	break;
	}
	/* mod_timer synchronizes us with potential add_timer calls
	 * from interrupts.
	 */
	mod_timer(&tp->timer, RUN_AT(next_tick));
}


void mxic_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct tulip_private *tp = (struct tulip_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int next_tick = 60*HZ;

	if (tulip_debug > 3) {
		printk(KERN_INFO"%s: MXIC negotiation status %8.8x.\n", dev->name,
			   inl(ioaddr + CSR12));
	}
	if (next_tick) {
		mod_timer(&tp->timer, RUN_AT(next_tick));
	}
}


void comet_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct tulip_private *tp = (struct tulip_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int next_tick = 60*HZ;

	if (tulip_debug > 1)
		printk(KERN_DEBUG "%s: Comet link status %4.4x partner capability "
			   "%4.4x.\n",
			   dev->name, inl(ioaddr + 0xB8), inl(ioaddr + 0xC8));
	/* mod_timer synchronizes us with potential add_timer calls
	 * from interrupts.
	 */
	mod_timer(&tp->timer, RUN_AT(next_tick));
}

