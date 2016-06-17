/*
 *  linux/drivers/hil/hilkbd.c
 *
 *  Copyright (C) 1998 Philip Blundell <philb@gnu.org>
 *  Copyright (C) 1999 Matthew Wilcox <willy@bofh.ai>
 *  Copyright (C) 1999-2002 Helge Deller <deller@gmx.de>
 *
 *  Very basic HP Human Interface Loop (HIL) driver.
 *  This driver handles the keyboard on HP300 (m68k) and on some 
 *  HP700 (parisc) series machines.
 *
 * 
 * This file is subject to the terms and conditions of the GNU General Public
 * License version 2.  See the file COPYING in the main directory of this
 * archive for more details.
 */

#include <linux/pci_ids.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/hil.h>


MODULE_AUTHOR("Philip Blundell, Matthew Wilcox, Helge Deller");
MODULE_DESCRIPTION("HIL driver (basic functionality)");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;


#if defined(CONFIG_PARISC)

 #include <asm/gsc.h>
 #include <asm/hardware.h>
 static unsigned long hil_base;	/* HPA for the HIL device */
 static unsigned int hil_irq;
 #define HILBASE		hil_base /* HPPA (parisc) port address */
 #define HIL_DATA		0x800
 #define HIL_CMD		0x801
 #define HIL_IRQ		hil_irq
 #define hil_readb(p)		gsc_readb(p)
 #define hil_writeb(v,p)	gsc_writeb((v),(p))

#elif defined(CONFIG_HP300)

 #define HILBASE		0xf0428000 /* HP300 (m86k) port address */
 #define HIL_DATA		0x1
 #define HIL_CMD		0x3
 #define HIL_IRQ		2
 #define hil_readb(p)		readb(p)
 #define hil_writeb(v,p)	writeb((v),(p))

#else
#error "HIL is not supported on this platform"
#endif


 
/* HIL helper functions */
 
#define hil_busy()              (hil_readb(HILBASE + HIL_CMD) & HIL_BUSY)
#define hil_data_available()    (hil_readb(HILBASE + HIL_CMD) & HIL_DATA_RDY)
#define hil_status()            (hil_readb(HILBASE + HIL_CMD))
#define hil_command(x)          do { hil_writeb((x), HILBASE + HIL_CMD); } while (0)
#define hil_read_data()         (hil_readb(HILBASE + HIL_DATA))
#define hil_write_data(x)       do { hil_writeb((x), HILBASE + HIL_DATA); } while (0)

/* HIL constants */
 
#define	HIL_BUSY		0x02
#define	HIL_DATA_RDY		0x01

#define	HIL_SETARD		0xA0		/* set auto-repeat delay */
#define	HIL_SETARR		0xA2		/* set auto-repeat rate */
#define	HIL_SETTONE		0xA3		/* set tone generator */
#define	HIL_CNMT		0xB2		/* clear nmi */
#define	HIL_INTON		0x5C		/* Turn on interrupts. */
#define	HIL_INTOFF		0x5D		/* Turn off interrupts. */
#define	HIL_TRIGGER		0xC5		/* trigger command */
#define	HIL_STARTCMD		0xE0		/* start loop command */
#define	HIL_TIMEOUT		0xFE		/* timeout */
#define	HIL_READTIME		0x13		/* Read real time register */

#define	HIL_READBUSY		0x02		/* internal "busy" register */
#define	HIL_READKBDLANG		0x12		/* read keyboard language code */
#define	HIL_READKBDSADR	 	0xF9
#define	HIL_WRITEKBDSADR 	0xE9
#define	HIL_READLPSTAT  	0xFA
#define	HIL_WRITELPSTAT 	0xEA
#define	HIL_READLPCTRL  	0xFB
#define	HIL_WRITELPCTRL 	0xEB


static unsigned char hil_kbd_set1[128] = {
   KEY_5,		KEY_RESERVED,	KEY_RIGHTALT,	KEY_LEFTALT, 
   KEY_RIGHTSHIFT,	KEY_LEFTSHIFT,	KEY_LEFTCTRL,	KEY_SYSRQ,
   KEY_KP4,		KEY_KP8,	KEY_KP5,	KEY_KP9,
   KEY_KP6,		KEY_KP7,	KEY_KPCOMMA,	KEY_KPENTER,
   KEY_KP1,		KEY_KPSLASH,	KEY_KP2,	KEY_KPPLUS,
   KEY_KP3,		KEY_KPASTERISK,	KEY_KP0,	KEY_KPMINUS,
   KEY_B,		KEY_V,		KEY_C,		KEY_X,
   KEY_Z,		KEY_UNKNOWN,	KEY_RESERVED,   KEY_ESC,
   KEY_6,		KEY_F10,	KEY_3,		KEY_F11,
   KEY_KPDOT,		KEY_F9,		KEY_TAB /*KP*/,	KEY_F12,
   KEY_H,		KEY_G,		KEY_F,		KEY_D,
   KEY_S,		KEY_A,		KEY_RESERVED,	KEY_CAPSLOCK,
   KEY_U,		KEY_Y,		KEY_T,		KEY_R,
   KEY_E,		KEY_W,		KEY_Q,		KEY_TAB,
   KEY_7,		KEY_6,		KEY_5,		KEY_4,
   KEY_3,		KEY_2,		KEY_1,		KEY_GRAVE,
   KEY_INTL1,		KEY_INTL2,	KEY_INTL3,	KEY_INTL4, /*Buttons*/
   KEY_INTL5,		KEY_INTL6,	KEY_INTL7,	KEY_INTL8,
   KEY_MENU,		KEY_F4,		KEY_F3,		KEY_F2,
   KEY_F1,		KEY_VOLUMEUP,	KEY_STOP,	KEY_SENDFILE/*Enter/Print*/, 
   KEY_SYSRQ,		KEY_F5,		KEY_F6,		KEY_F7,
   KEY_F8,		KEY_VOLUMEDOWN,	KEY_CUT /*CLEAR_LINE*/, KEY_REFRESH /*CLEAR_DISPLAY*/,
   KEY_8,		KEY_9,		KEY_0,		KEY_MINUS,
   KEY_EQUAL,		KEY_BACKSPACE,	KEY_INSERT/*KPINSERT_LINE*/, KEY_DELETE /*KPDELETE_LINE*/,
   KEY_I,		KEY_O,		KEY_P,		KEY_LEFTBRACE,
   KEY_RIGHTBRACE,	KEY_BACKSLASH,	KEY_INSERT,	KEY_DELETE,
   KEY_J,		KEY_K,		KEY_L,		KEY_SEMICOLON,
   KEY_APOSTROPHE,	KEY_ENTER,	KEY_HOME,	KEY_SCROLLUP,
   KEY_M,		KEY_COMMA,	KEY_DOT,	KEY_SLASH,
   KEY_RESERVED,	KEY_OPEN/*Select*/,KEY_RESERVED,KEY_SCROLLDOWN/*KPNEXT*/,
   KEY_N,		KEY_SPACE,	KEY_SCROLLDOWN/*Next*/, KEY_UNKNOWN,
   KEY_LEFT,		KEY_DOWN,	KEY_UP,		KEY_RIGHT
};


/* HIL structure */
static struct {
	struct input_dev dev;

	unsigned int curdev;
	
	unsigned char s;
	unsigned char c;
	int valid;
	
	unsigned char data[16];
	unsigned int ptr;

	void *dev_id;	/* native bus device */
} hil_dev;


static void poll_finished(void)
{
	int down;
	int key;
	unsigned char scode;
	
	switch (hil_dev.data[0]) {
	case 0x40:
		down = (hil_dev.data[1] & 1) == 0;
		scode = hil_dev.data[1] >> 1;
		key = hil_kbd_set1[scode & 0x7f];
		input_report_key(&hil_dev.dev, key, down);
		break;
	}
	hil_dev.curdev = 0;
}

static inline void handle_status(unsigned char s, unsigned char c)
{
	if (c & 0x8) {
		/* End of block */
		if (c & 0x10)
			poll_finished();
	} else {
		if (c & 0x10) {
			if (hil_dev.curdev)
				poll_finished();  /* just in case */
			hil_dev.curdev = c & 7;
			hil_dev.ptr = 0;
		}
	}
}

static inline void handle_data(unsigned char s, unsigned char c)
{
	if (hil_dev.curdev) {
		hil_dev.data[hil_dev.ptr++] = c;
		hil_dev.ptr &= 15;
	}
}


/* 
 * Handle HIL interrupts.
 */
static void hil_interrupt(int irq, void *handle, struct pt_regs *regs)
{
	unsigned char s, c;
	
	s = hil_status();
	c = hil_read_data();

	switch (s >> 4) {
	case 0x5:
		handle_status(s, c);
		break;
	case 0x6:
		handle_data(s, c);
		break;
	case 0x4:
		hil_dev.s = s;
		hil_dev.c = c;
		mb();
		hil_dev.valid = 1;
		break;
	}
}

/*
 * Send a command to the HIL
 */

static void hil_do(unsigned char cmd, unsigned char *data, unsigned int len)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	while (hil_busy())
		/* wait */;
	hil_command(cmd);
	while (len--) {
		while (hil_busy())
			/* wait */;
		hil_write_data(*(data++));
	}
	restore_flags(flags);
}


/*
 * Initialise HIL. 
 */

static int __init
hil_keyb_init(void)
{
	unsigned char c;
	unsigned int i, kbid, n = 0;

	if (hil_dev.dev.idbus) {
		printk("HIL: already initialized\n");
		return -ENODEV;
	}
	
#if defined(CONFIG_HP300)
	if (!hwreg_present((void *)(HILBASE + HIL_DATA)))
		return -ENODEV;
	
	request_region(HILBASE+HIL_DATA, 2, "hil");
#endif
	
	request_irq(HIL_IRQ, hil_interrupt, 0, "hil", hil_dev.dev_id);

	/* Turn on interrupts */
	hil_do(HIL_INTON, NULL, 0);

	/* Look for keyboards */
	hil_dev.valid = 0;	/* clear any pending data */
	hil_do(HIL_READKBDSADR, NULL, 0);
	while (!hil_dev.valid) {
		if (n++ > 100000) {
			printk(KERN_DEBUG "HIL: timed out, assuming no keyboard present.\n");
			break;
		}
		mb();
	}

	c = hil_dev.c; 
	hil_dev.valid = 0;
	if (c == 0) {
		kbid = -1;
		printk(KERN_WARNING "HIL: no keyboard present.\n");
	} else {
		kbid = ffz(~c);
		printk(KERN_INFO "HIL: keyboard found at id %d\n", kbid);
	}

	/* set it to raw mode */
	c = 0;
	hil_do(HIL_WRITEKBDSADR, &c, 1);
	

	/* register input interface */
	hil_dev.dev.name 	= "HIL keyboard";
	hil_dev.dev.idbus	= BUS_HIL;
	hil_dev.dev.idvendor	= PCI_VENDOR_ID_HP;
	hil_dev.dev.idproduct	= 0x0001;
	hil_dev.dev.idversion	= 0x0100;

	hil_dev.dev.evbit[0] |= BIT(EV_KEY);
	for (i = 0; i < 128; i++)
		set_bit(hil_kbd_set1[i], hil_dev.dev.keybit);
	clear_bit(0, hil_dev.dev.keybit);

#if 1
	/* XXX: HACK !!!
	 * remove this call if hp_psaux.c/hp_keyb.c is converted
	 * to the input layer... */
	register_ps2_keybfuncs();
#endif
	
	input_register_device(&hil_dev.dev);
	printk(KERN_INFO "input%d: %s on hil%d (id %d)\n",
		hil_dev.dev.number, hil_dev.dev.name, 0, kbid);

	/* HIL keyboards don't have a numlock key,
	 * simulate a up-down sequence of numlock to 
	 * make the keypad work at expected. */
	input_report_key(&hil_dev.dev, KEY_NUMLOCK, 1);

	return 0;
}

#if defined(CONFIG_PARISC)
static int __init
hil_init_chip(struct parisc_device *dev)
{
	if (!dev->irq) {
		printk(KERN_WARNING "HIL: IRQ not found for HIL at 0x%lx\n", dev->hpa);
		return -ENODEV;
	}

	hil_base = dev->hpa;
	hil_irq  = dev->irq;
	hil_dev.dev_id = dev;
	
	printk(KERN_INFO "Found HIL at 0x%lx, IRQ %d\n", hil_base, hil_irq);

	return hil_keyb_init();
}

static struct parisc_device_id hil_tbl[] = {
	{ HPHW_FIO, HVERSION_REV_ANY_ID, HVERSION_ANY_ID, 0x00073 },
	{ 0, }
};

MODULE_DEVICE_TABLE(parisc, hil_tbl);

static struct parisc_driver hil_driver = {
	.name =		"HIL",
	.id_table =	hil_tbl,
	.probe =	hil_init_chip,
};
#endif /* CONFIG_PARISC */





static int __init hil_init(void)
{
#if defined(CONFIG_PARISC)
	return register_parisc_driver(&hil_driver);
#else
	return hil_keyb_init();
#endif
}


static void __exit hil_exit(void)
{
	if (HIL_IRQ) {
		disable_irq(HIL_IRQ);
		free_irq(HIL_IRQ, hil_dev.dev_id);
	}

	input_unregister_device(&hil_dev.dev);

#if defined(CONFIG_PARISC)
	unregister_parisc_driver(&hil_driver);
#else
	release_region(HILBASE+HIL_DATA, 2);
#endif
}

module_init(hil_init);
module_exit(hil_exit);

