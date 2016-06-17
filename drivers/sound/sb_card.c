/*
 * sound/sb_card.c
 *
 * Detection routine for the Sound Blaster cards.
 *
 *
 * Copyright (C) by Hannu Savolainen 1993-1997
 *
 * OSS/Free for Linux is distributed under the GNU GENERAL PUBLIC LICENSE (GPL)
 * Version 2 (June 1991). See the "COPYING" file distributed with this software
 * for more info.
 *
 * 26-11-1999 Patched to compile without ISA PnP support in the
 * kernel - Daniel Stone (tamriel@ductape.net) 
 *
 * 06-01-2000 Refined and bugfixed ISA PnP support, added
 *  CMI 8330 support - Alessandro Zummo <azummo@ita.flashnet.it>
 *
 * 18-01-2000 Separated sb_card and sb_common
 *  Jeff Garzik <jgarzik@pobox.com>
 *
 * 04-02-2000 Added Soundblaster AWE 64 PnP support, isapnpjump
 *  Alessandro Zummo <azummo@ita.flashnet.it>
 *
 * 11-02-2000 Added Soundblaster AWE 32 PnP support, refined PnP code
 *  Alessandro Zummo <azummo@ita.flashnet.it>
 *
 * 13-02-2000 Hopefully fixed awe/sb16 related bugs, code cleanup
 *  Alessandro Zummo <azummo@ita.flashnet.it>
 *
 * 13-03-2000 Added some more cards, thanks to Torsten Werner.
 *  Removed joystick and wavetable code, there are better places for them.
 *  Code cleanup plus some fixes. 
 *  Alessandro Zummo <azummo@ita.flashnet.it>
 * 
 * 26-03-2000 Fixed acer, esstype and sm_games module options.
 *  Alessandro Zummo <azummo@ita.flashnet.it>
 *
 * 12-04-2000 ISAPnP cleanup, reorg, fixes, and multiple card support.
 *  Thanks to Gaël Quéri and Alessandro Zummo for testing and fixes.
 *  Paul E. Laufer <pelaufer@csupomona.edu>
 *
 * 06-05-2000 added another card. Daniel M. Newman <dmnewman@pobox.com>
 *
 * 25-05-2000 Added Creative SB AWE64 Gold (CTL00B2). 
 * 	Pål-Kristian Engstad <engstad@att.net>
 *
 * 12-08-2000 Added Creative SB32 PnP (CTL009F).
 * 	Kasatenko Ivan Alex. <skywriter@rnc.ru>
 *
 * 21-09-2000 Got rid of attach_sbmpu
 * 	Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * 28-10-2000 Added pnplegacy support
 * 	Daniel Church <dchurch@mbhs.edu>
 *
 * 01-10-2001 Added a new flavor of Creative SB AWE64 PnP (CTL00E9).
 *      Jerome Cornet <jcornet@free.fr>
 */

#include <linux/config.h>
#include <linux/mca.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/isapnp.h>

#include "sound_config.h"

#include "sb_mixer.h"
#include "sb.h"

#if defined CONFIG_ISAPNP || defined CONFIG_ISAPNP_MODULE
#define SB_CARDS_MAX 5
#else
#define SB_CARDS_MAX 1
#endif

static int sbmpu[SB_CARDS_MAX] = {0};
static int sb_cards_num = 0;

extern void *smw_free;

/*
 *    Note DMA2 of -1 has the right meaning in the SB16 driver as well
 *    as here. It will cause either an error if it is needed or a fallback
 *    to the 8bit channel.
 */

static int __initdata mpu_io	= 0;
static int __initdata io	= -1;
static int __initdata irq	= -1;
static int __initdata dma	= -1;
static int __initdata dma16	= -1;   /* Set this for modules that need it */
static int __initdata type	= 0;    /* Can set this to a specific card type */
static int __initdata esstype   = 0;	/* ESS chip type */
static int __initdata acer 	= 0;	/* Do acer notebook init? */
static int __initdata sm_games 	= 0;	/* Logitech soundman games? */

static void __init attach_sb_card(struct address_info *hw_config)
{
	if(!sb_dsp_init(hw_config, THIS_MODULE))
		hw_config->slots[0] = -1;
}

static int __init probe_sb(struct address_info *hw_config)
{
	struct sb_module_options sbmo;

	if (hw_config->io_base == -1 || hw_config->dma == -1 || hw_config->irq == -1)
	{
		printk(KERN_ERR "sb: I/O, IRQ, and DMA are mandatory\n");
		return -EINVAL;
	}

#ifdef CONFIG_MCA
	/* MCA code added by ZP Gu (zpg@castle.net) */
	if (MCA_bus) {               /* no multiple REPLY card probing */
		int slot;
		u8 pos2, pos3, pos4;

		slot = mca_find_adapter( 0x5138, 0 );
		if( slot == MCA_NOTFOUND ) 
		{
			slot = mca_find_adapter( 0x5137, 0 );

			if (slot != MCA_NOTFOUND)
				mca_set_adapter_name( slot, "REPLY SB16 & SCSI Adapter" );
		}
		else
		{
			mca_set_adapter_name( slot, "REPLY SB16 Adapter" );
		}

		if (slot != MCA_NOTFOUND) 
		{
			mca_mark_as_used(slot);
			pos2 = mca_read_stored_pos( slot, 2 );
			pos3 = mca_read_stored_pos( slot, 3 );
			pos4 = mca_read_stored_pos( slot, 4 );

			if (pos2 & 0x4) 
			{
				/* enabled? */
				static unsigned short irq[] = { 0, 5, 7, 10 };
				/*
				static unsigned short midiaddr[] = {0, 0x330, 0, 0x300 };
       				*/

				hw_config->io_base = 0x220 + 0x20 * (pos2 >> 6);
				hw_config->irq = irq[(pos4 >> 5) & 0x3];
				hw_config->dma = pos3 & 0xf;
				/* Reply ADF wrong on High DMA, pos[1] should start w/ 00 */
				hw_config->dma2 = (pos3 >> 4) & 0x3;
				if (hw_config->dma2 == 0)
					hw_config->dma2 = hw_config->dma;
				else
					hw_config->dma2 += 4;
				/*
					hw_config->driver_use_2 = midiaddr[(pos2 >> 3) & 0x3];
				*/
	
				printk(KERN_INFO "sb: Reply MCA SB at slot=%d \
iobase=0x%x irq=%d lo_dma=%d hi_dma=%d\n",
						slot+1,
				        	hw_config->io_base, hw_config->irq,
	        				hw_config->dma, hw_config->dma2);
			}
			else
			{
				printk (KERN_INFO "sb: Reply SB Base I/O address disabled\n");
			}
		}
	}
#endif

	/* Setup extra module options */

	sbmo.acer 	= acer;
	sbmo.sm_games	= sm_games;
	sbmo.esstype	= esstype;

	return sb_dsp_detect(hw_config, 0, 0, &sbmo);
}

static void __exit unload_sb(struct address_info *hw_config, int card)
{
	if(hw_config->slots[0]!=-1)
		sb_dsp_unload(hw_config, sbmpu[card]);
}

static struct address_info cfg[SB_CARDS_MAX];
static struct address_info cfg_mpu[SB_CARDS_MAX];

struct pci_dev 	*sb_dev[SB_CARDS_MAX] 	= {NULL}, 
		*mpu_dev[SB_CARDS_MAX]	= {NULL},
		*opl_dev[SB_CARDS_MAX]	= {NULL};


#if defined CONFIG_ISAPNP || defined CONFIG_ISAPNP_MODULE
static int isapnp	= 1;
static int isapnpjump	= 0;
static int multiple	= 1;
static int pnplegacy	= 0;
static int reverse	= 0;
static int uart401	= 0;

static int audio_activated[SB_CARDS_MAX] = {0};
static int mpu_activated[SB_CARDS_MAX]   = {0};
static int opl_activated[SB_CARDS_MAX]   = {0};
#else
static int isapnp	= 0;
static int multiple	= 0;
static int pnplegacy	= 0;
#endif

MODULE_DESCRIPTION("Soundblaster driver");
MODULE_LICENSE("GPL");

MODULE_PARM(io,		"i");
MODULE_PARM(irq,	"i");
MODULE_PARM(dma,	"i");
MODULE_PARM(dma16,	"i");
MODULE_PARM(mpu_io,	"i");
MODULE_PARM(type,	"i");
MODULE_PARM(sm_games,	"i");
MODULE_PARM(esstype,	"i");
MODULE_PARM(acer,	"i");

#if defined CONFIG_ISAPNP || defined CONFIG_ISAPNP_MODULE
MODULE_PARM(isapnp,	"i");
MODULE_PARM(isapnpjump,	"i");
MODULE_PARM(multiple,	"i");
MODULE_PARM(pnplegacy,	"i");
MODULE_PARM(reverse,	"i");
MODULE_PARM(uart401,	"i");
MODULE_PARM_DESC(isapnp,	"When set to 0, Plug & Play support will be disabled");
MODULE_PARM_DESC(isapnpjump,	"Jumps to a specific slot in the driver's PnP table. Use the source, Luke.");
MODULE_PARM_DESC(multiple,	"When set to 0, will not search for multiple cards");
MODULE_PARM_DESC(pnplegacy,	"When set to 1, will search for a legacy SB card along with any PnP cards.");
MODULE_PARM_DESC(reverse,	"When set to 1, will reverse ISAPnP search order");
MODULE_PARM_DESC(uart401,	"When set to 1, will attempt to detect and enable the mpu on some clones");
#endif

MODULE_PARM_DESC(io,		"Soundblaster i/o base address (0x220,0x240,0x260,0x280)");
MODULE_PARM_DESC(irq,		"IRQ (5,7,9,10)");
MODULE_PARM_DESC(dma,		"8-bit DMA channel (0,1,3)");
MODULE_PARM_DESC(dma16,		"16-bit DMA channel (5,6,7)");
MODULE_PARM_DESC(mpu_io,	"Mpu base address");
MODULE_PARM_DESC(type,		"You can set this to specific card type");
MODULE_PARM_DESC(sm_games,	"Enable support for Logitech soundman games");
MODULE_PARM_DESC(esstype,	"ESS chip type");
MODULE_PARM_DESC(acer,		"Set this to detect cards in some ACER notebooks");

#if defined CONFIG_ISAPNP || defined CONFIG_ISAPNP_MODULE

/* Please add new entries at the end of the table */
static struct {
	char *name; 
	unsigned short	card_vendor, card_device, 
			audio_vendor, audio_function,
			mpu_vendor, mpu_function,
			opl_vendor, opl_function;
	short dma, dma2, mpu_io, mpu_irq; /* see sb_init() */
} sb_isapnp_list[] __initdata = {
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0024),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0025),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0026), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0027), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0028), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0029), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x002a),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x002b), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x002c), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x002c), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00ed), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0086), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster 16", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0086), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster Vibra16S", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0051), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0001),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster Vibra16C", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0070), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0001),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster Vibra16CL", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0080), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster Vibra16X", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00F0), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0043),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 32", 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0039), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 32",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0042), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 32",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0043), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 32",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0044),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
        {"Sound Blaster AWE 32",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0045),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 32",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0046),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 32",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0047),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 32",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0048), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 32",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0054), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 32",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x009C), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041),
		0,0,0,0,
		0,1,1,-1},
	{"Creative SB32 PnP",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x009F),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 64",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x009D), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0042),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 64 Gold",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x009E), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0044),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 64 Gold",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00B2),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0044),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 64",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00C1), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0042),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 64",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00C3), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0045),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 64",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00C5), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0045),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 64",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00C7), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0045),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 64",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00E4), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0045),
		0,0,0,0,
		0,1,1,-1},
	{"Sound Blaster AWE 64",
		ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00E9), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0045),
		0,0,0,0,
		0,1,1,-1},
	{"ESS 1688",
		ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x0968), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x0968),
		0,0,0,0,
		0,1,2,-1},
	{"ESS 1868",
		ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x1868), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x1868),
		0,0,0,0,
		0,1,2,-1},
	{"ESS 1868",
		ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x1868), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x8611),
		0,0,0,0,
		0,1,2,-1},
	{"ESS 1869 PnP AudioDrive", 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x0003), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x1869),
		0,0,0,0,
		0,1,2,-1},
	{"ESS 1869",
		ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x1869), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x1869),
		0,0,0,0,
		0,1,2,-1},
	{"ESS 1878",
		ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x1878), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x1878),
		0,0,0,0,
		0,1,2,-1},
	{"ESS 1879",
		ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x1879), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x1879),
		0,0,0,0,
		0,1,2,-1},
	{"CMI 8330 SoundPRO",
		ISAPNP_VENDOR('C','M','I'), ISAPNP_DEVICE(0x0001), 
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x0001),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x0001),
		0,1,0,-1},
	{"Diamond DT0197H",
		ISAPNP_VENDOR('R','W','B'), ISAPNP_DEVICE(0x1688), 
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x0001),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x0001),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001),
		0,-1,0,0},
	{"ALS007",
		ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0007),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x0001),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x0001),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001),
		0,-1,0,0},
	{"ALS100",
		ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0001), 
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x0001),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x0001),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001),
		1,0,0,0},
	{"ALS110",
		ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0110),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x1001),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x1001),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001),
		1,0,0,0},
	{"ALS120",
		ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0120),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x2001),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x2001),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001),
		1,0,0,0},
	{"ALS200",
		ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0200),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x0020),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x0020),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001),
		1,0,0,0},
	{"RTL3000",
		ISAPNP_VENDOR('R','T','L'), ISAPNP_DEVICE(0x3000),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x2001),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x2001),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001),
		1,0,0,0},
	{0}
};

static struct isapnp_device_id id_table[] __devinitdata = {
	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0024),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0025),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0026), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0027), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0028), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0029), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x002a),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x002b), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x002c), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x002c), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00ed), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0086), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0086), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0051), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0070), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0080), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00F0), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0043), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0039), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0042), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0043), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0044),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0045),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0048), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x0054), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0031), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x009C), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x009F),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0041), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x009D), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0042), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x009E), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0044), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00B2),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0044), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00C1), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0042), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00C3), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0045), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00C5), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0045), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00C7), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0045), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00E4), 
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0045), 0 },

	{	ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x00E9),
		ISAPNP_VENDOR('C','T','L'), ISAPNP_FUNCTION(0x0045), 0 },

	{	ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x0968), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x0968), 0 },

	{	ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x1868), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x1868), 0 },

	{	ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x1868), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x8611), 0 },

	{	ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x0003), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x1869), 0 },

	{	ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x1869), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x1869), 0 },

	{	ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x1878), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x1878), 0 },

	{	ISAPNP_VENDOR('E','S','S'), ISAPNP_DEVICE(0x1879), 
		ISAPNP_VENDOR('E','S','S'), ISAPNP_FUNCTION(0x1879), 0 },

	{	ISAPNP_VENDOR('C','M','I'), ISAPNP_DEVICE(0x0001), 
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('C','M','I'), ISAPNP_DEVICE(0x0001), 
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('C','M','I'), ISAPNP_DEVICE(0x0001), 
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('R','W','B'), ISAPNP_DEVICE(0x1688), 
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('R','W','B'), ISAPNP_DEVICE(0x1688), 
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('R','W','B'), ISAPNP_DEVICE(0x1688), 
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0007),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0007),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0007),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0001), 
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0001), 
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0001), 
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0110),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x1001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0110),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x1001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0110),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0120),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x2001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0120),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x2001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0120),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0200),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x0020), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0200),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x0020), 0 },

	{	ISAPNP_VENDOR('A','L','S'), ISAPNP_DEVICE(0x0200),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001), 0 },

	{	ISAPNP_VENDOR('R','T','L'), ISAPNP_DEVICE(0x3000),
		ISAPNP_VENDOR('@','@','@'), ISAPNP_FUNCTION(0x2001), 0 },

	{	ISAPNP_VENDOR('R','T','L'), ISAPNP_DEVICE(0x3000),
		ISAPNP_VENDOR('@','X','@'), ISAPNP_FUNCTION(0x2001), 0 },

	{	ISAPNP_VENDOR('R','T','L'), ISAPNP_DEVICE(0x3000),
		ISAPNP_VENDOR('@','H','@'), ISAPNP_FUNCTION(0x0001), 0 },
	{0}
};

MODULE_DEVICE_TABLE(isapnp, id_table);

static struct pci_dev *activate_dev(char *devname, char *resname, struct pci_dev *dev)
{
	int err;

	/* Device already active? Let's use it */
	if(dev->active)
		return(dev);
	
	if((err = dev->activate(dev)) < 0) {
		printk(KERN_ERR "sb: %s %s config failed (out of resources?)[%d]\n", devname, resname, err);

		dev->deactivate(dev);

		return(NULL);
	}
	return(dev);
}

static struct pci_dev *sb_init(struct pci_bus *bus, struct address_info *hw_config, struct address_info *mpu_config, int slot, int card)
{

	/* Configure Audio device */
	if((sb_dev[card] = isapnp_find_dev(bus, sb_isapnp_list[slot].audio_vendor, sb_isapnp_list[slot].audio_function, NULL)))
	{
		int ret;
		ret = sb_dev[card]->prepare(sb_dev[card]);
		/* If device is active, assume configured with /proc/isapnp
		 * and use anyway. Some other way to check this? */
		if(ret && ret != -EBUSY) {
			printk(KERN_ERR "sb: ISAPnP found device that could not be autoconfigured.\n");
			return(NULL);
		}
		if(ret == -EBUSY)
			audio_activated[card] = 1;
		
		if((sb_dev[card] = activate_dev(sb_isapnp_list[slot].name, "sb", sb_dev[card])))
		{
			hw_config->io_base 	= sb_dev[card]->resource[0].start;
			hw_config->irq 		= sb_dev[card]->irq_resource[0].start;
			hw_config->dma 		= sb_dev[card]->dma_resource[sb_isapnp_list[slot].dma].start;
			if(sb_isapnp_list[slot].dma2 != -1)
				hw_config->dma2 = sb_dev[card]->dma_resource[sb_isapnp_list[slot].dma2].start;
			else
				hw_config->dma2 = -1;
		} else
			return(NULL);
	} else
		return(NULL);

	/* Cards with separate OPL3 device (ALS, CMI, etc.)
	 * This is just to activate the device so the OPL module can use it */
	if(sb_isapnp_list[slot].opl_vendor || sb_isapnp_list[slot].opl_function) {
		if((opl_dev[card] = isapnp_find_dev(bus, sb_isapnp_list[slot].opl_vendor, sb_isapnp_list[slot].opl_function, NULL))) {
			int ret = opl_dev[card]->prepare(opl_dev[card]);
			/* If device is active, assume configured with
			 * /proc/isapnp and use anyway */
			if(ret && ret != -EBUSY) {
				printk(KERN_ERR "sb: OPL device could not be autoconfigured.\n");
				return(sb_dev[card]);
			}
			if(ret == -EBUSY)
				opl_activated[card] = 1;

			/* Some have irq and dma for opl. the opl3 driver wont
			 * use 'em so don't configure 'em and hope it works -PEL */
			opl_dev[card]->irq_resource[0].flags = 0;
			opl_dev[card]->dma_resource[0].flags = 0;

			opl_dev[card] = activate_dev(sb_isapnp_list[slot].name, "opl3", opl_dev[card]);
		} else
			printk(KERN_ERR "sb: %s isapnp panic: opl3 device not found\n", sb_isapnp_list[slot].name);
	}

	/* Cards with MPU as part of Audio device (CTL and ESS) */
	if(!sb_isapnp_list[slot].mpu_vendor) {
		mpu_config->io_base	= sb_dev[card]->resource[sb_isapnp_list[slot].mpu_io].start;
		return(sb_dev[card]);
	}
	
	/* Cards with separate MPU device (ALS, CMI, etc.) */
	if(!uart401)
		return(sb_dev[card]);
	if((mpu_dev[card] = isapnp_find_dev(bus, sb_isapnp_list[slot].mpu_vendor, sb_isapnp_list[slot].mpu_function, NULL)))
	{
		int ret = mpu_dev[card]->prepare(mpu_dev[card]);
		/* If device is active, assume configured with /proc/isapnp
		 * and use anyway */
		if(ret && ret != -EBUSY) {
			printk(KERN_ERR "sb: MPU device could not be autoconfigured.\n");
			return(sb_dev[card]);
		}
		if(ret == -EBUSY)
			mpu_activated[card] = 1;
		
		/* Some cards ask for irq but don't need them - azummo */
		if(sb_isapnp_list[slot].mpu_irq == -1)
			mpu_dev[card]->irq_resource[0].flags = 0;
		
		if((mpu_dev[card] = activate_dev(sb_isapnp_list[slot].name, "mpu", mpu_dev[card]))) {
			mpu_config->io_base = mpu_dev[card]->resource[sb_isapnp_list[slot].mpu_io].start;
			if(sb_isapnp_list[slot].mpu_irq != -1)
				mpu_config->irq = mpu_dev[card]->irq_resource[sb_isapnp_list[slot].mpu_irq].start;
		}
	}
	else
		printk(KERN_ERR "sb: %s isapnp panic: mpu not found\n", sb_isapnp_list[slot].name);
	
	return(sb_dev[card]);
}

static int __init sb_isapnp_init(struct address_info *hw_config, struct address_info *mpu_config, struct pci_bus *bus, int slot, int card)
{
	char *busname = bus->name[0] ? bus->name : sb_isapnp_list[slot].name;

	printk(KERN_INFO "sb: %s detected\n", busname); 

	/* Initialize this baby. */

	if(sb_init(bus, hw_config, mpu_config, slot, card)) {
		/* We got it. */
		
		printk(KERN_NOTICE "sb: ISAPnP reports '%s' at i/o %#x, irq %d, dma %d, %d\n",
		       busname,
		       hw_config->io_base, hw_config->irq, hw_config->dma,
		       hw_config->dma2);
		return 1;
	}
	else
		printk(KERN_INFO "sb: Failed to initialize %s\n", busname);

	return 0;
}

static int __init sb_isapnp_probe(struct address_info *hw_config, struct address_info *mpu_config, int card)
{
	static int first = 1;
	int i;

	/* Count entries in sb_isapnp_list */
	for (i = 0; sb_isapnp_list[i].card_vendor != 0; i++);
	i--;

	/* Check and adjust isapnpjump */
	if( isapnpjump < 0 || isapnpjump > i) {
		isapnpjump = reverse ? i : 0;
		printk(KERN_ERR "sb: Valid range for isapnpjump is 0-%d. Adjusted to %d.\n", i, isapnpjump);
	}

	if(!first || !reverse)
		i = isapnpjump;
	first = 0;
	while(sb_isapnp_list[i].card_vendor != 0) {
		static struct pci_bus *bus = NULL;

		while ((bus = isapnp_find_card(
				sb_isapnp_list[i].card_vendor,
				sb_isapnp_list[i].card_device,
				bus))) {
	
			if(sb_isapnp_init(hw_config, mpu_config, bus, i, card)) {
				isapnpjump = i; /* start next search from here */
				return 0;
			}
		}
		i += reverse ? -1 : 1;
	}

	return -ENODEV;
}
#endif

static int __init init_sb(void)
{
	int card, max = (multiple && isapnp) ? SB_CARDS_MAX : 1;

	printk(KERN_INFO "Soundblaster audio driver Copyright (C) by Hannu Savolainen 1993-1996\n");
	
	for(card = 0; card < max; card++, sb_cards_num++) {
#if defined CONFIG_ISAPNP || defined CONFIG_ISAPNP_MODULE
		/* Please remember that even with CONFIG_ISAPNP defined one
		 * should still be able to disable PNP support for this 
		 * single driver! */
		if((!pnplegacy||card>0) && isapnp && (sb_isapnp_probe(&cfg[card], &cfg_mpu[card], card) < 0) ) {
			if(!sb_cards_num) {
				/* Found no ISAPnP cards, so check for a non-pnp
				 * card and set the detection loop for 1 cycle
				 */
				printk(KERN_NOTICE "sb: No ISAPnP cards found, trying standard ones...\n");
				isapnp = 0;
				max = 1;
			} else
				/* found all the ISAPnP cards so exit the
				 * detection loop. */
				break;
		}
#endif

		if(!isapnp || (pnplegacy&&card==0)) {
			cfg[card].io_base	= io;
			cfg[card].irq		= irq;
			cfg[card].dma		= dma;
			cfg[card].dma2		= dma16;
		}

		cfg[card].card_subtype = type;

		if (!probe_sb(&cfg[card])) {
			/* if one or more cards already registered, don't
			 * return an error but print a warning. Note, this
			 * should never really happen unless the hardware
			 * or ISAPnP screwed up. */
			if (sb_cards_num) {
				printk(KERN_WARNING "sb.c: There was a " \
				  "problem probing one of your SoundBlaster " \
				  "ISAPnP soundcards. Continuing.\n");
				card--;
				sb_cards_num--;
				continue;
			} else if(pnplegacy && isapnp) {
				printk(KERN_NOTICE "sb: No legacy SoundBlaster cards " \
				  "found.  Continuing with PnP detection.\n");
				pnplegacy=0;
				card--;
				continue;
			} else
				return -ENODEV;
		}
		attach_sb_card(&cfg[card]);

		if(cfg[card].slots[0]==-1) {
			if(card==0 && pnplegacy && isapnp) {
				printk(KERN_NOTICE "sb: No legacy SoundBlaster cards " \
				  "found.  Continuing with PnP detection.\n");
				pnplegacy=0;
				card--;
				continue;
			} else
				return -ENODEV;
		}
		
		if (!isapnp||(pnplegacy&&card==0))
			cfg_mpu[card].io_base = mpu_io;
		if (probe_sbmpu(&cfg_mpu[card], THIS_MODULE))
			sbmpu[card] = 1;
	}

	if(isapnp)
		printk(KERN_NOTICE "sb: %d Soundblaster PnP card(s) found.\n", sb_cards_num);

	return 0;
}

static void __exit cleanup_sb(void)
{
	int i;
	
	if (smw_free) {
		vfree(smw_free);
		smw_free = NULL;
	}

	for(i = 0; i < sb_cards_num; i++) {
		unload_sb(&cfg[i], i);
		if (sbmpu[i])
			unload_sbmpu(&cfg_mpu[i]);

#if defined CONFIG_ISAPNP || defined CONFIG_ISAPNP_MODULE
		if(!audio_activated[i] && sb_dev[i])
			sb_dev[i]->deactivate(sb_dev[i]);
		if(!mpu_activated[i] && mpu_dev[i])
			mpu_dev[i]->deactivate(mpu_dev[i]);
		if(!opl_activated[i] && opl_dev[i])
			opl_dev[i]->deactivate(opl_dev[i]);
#endif
	}
}

module_init(init_sb);
module_exit(cleanup_sb);

#ifndef MODULE
static int __init setup_sb(char *str)
{
	/* io, irq, dma, dma2 - just the basics */
	int ints[5];
	
	str = get_options(str, ARRAY_SIZE(ints), ints);
	
	io	= ints[1];
	irq	= ints[2];
	dma	= ints[3];
	dma16	= ints[4];

	return 1;
}
__setup("sb=", setup_sb);
#endif
