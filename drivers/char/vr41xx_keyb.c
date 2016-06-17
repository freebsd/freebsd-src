/*
 * FILE NAME
 *	drivers/char/vr41xx_keyb.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Keyboard driver for NEC VR4100 series Keyboard Interface Unit.
 *
 * Copyright (C) 1999 Bradley D. LaRonde
 * Copyright (C) 1999 Hiroshi Kawashima <kawashima@iname.com>
 * Copyright (C) 2000 Michael Klar <wyldfier@iname.com>
 * Copyright (C) 2002,2003 Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
/*
 * Changes:
 *  version 1.0
 *  Yoichi Yuasa <yuasa@hh.iij4u.or.jp>  Mon, 25 Mar 2002
 *  -  Rewrote extensively because of 2.4.18.
 *
 *  version 1.1
 *  Yoichi Yuasa <yuasa@hh.iij4u.or.jp>  Wed,  9 Sep 200
 *  -  Added NEC VRC4173 KIU support.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kbd_ll.h>
#ifdef CONFIG_PCI
#include <linux/pci.h>
#endif
#include <linux/pm.h>

#include <asm/addrspace.h>
#include <asm/cpu.h>
#include <asm/io.h>
#include <asm/param.h>
#include <asm/vr41xx/vr41xx.h>
#ifdef CONFIG_VRC4173
#include <asm/vr41xx/vrc4173.h>
#endif

#define KIU_BASE			KSEG1ADDR(0x0b000180)
#define MKIUINTREG			KSEG1ADDR(0x0b000092)

#define VRC4173_KIU_OFFSET		0x100
#define VRC4173_MKIUINTREG_OFFSET	0x072

#define KIUDAT0				0x00
#define KIUDAT1				0x02
#define KIUDAT2				0x04
#define KIUDAT3				0x06
#define KIUDAT4				0x08
#define KIUDAT5				0x0a
#define KIUDAT6				0x0c
#define KIUDAT7				0x0e
#define KIUSCANREP			0x10
 #define KIUSCANREP_KEYEN		0x8000
 #define KIUSCANREP_STPREP(x)		((x) << 4)
 #define KIUSCANREP_SCANSTP		0x0008
 #define KIUSCANREP_SCANSTART		0x0004
 #define KIUSCANREP_ATSTP		0x0002
 #define KIUSCANREP_ATSCAN		0x0001
#define KIUSCANS			0x12
 #define KIUSCANS_SCANNING		0x0003
 #define KIUSCANS_INTERVAL		0x0002
 #define KIUSCANS_WAITKEYIN		0x0001
 #define KIUSCANS_STOPPED		0x0000
#define KIUWKS				0x14
 #define KIUWKS_T3CNT			0x7c00
 #define KIUWKS_T3CNT_SHIFT		10
 #define KIUWKS_T2CNT			0x03e0
 #define KIUWKS_T2CNT_SHIFT		5
 #define KIUWKS_T1CNT			0x001f
 #define KIUWKS_T1CNT_SHIFT		0
 #define KIUWKS_CNT_USEC(x)		(((x) / 30) - 1)
#define KIUWKI				0x16
 #define KIUWKI_INTERVAL_USEC(x)	((x) / 30)
#define KIUINT				0x18
 #define KIUINT_KDATLOST		0x0004
 #define KIUINT_KDATRDY			0x0002
 #define KIUINT_SCANINT			0x0001
#define KIURST				0x1a
 #define KIURST_KIURST			0x0001
#define KIUGPEN				0x1c
 #define KIUGPEN_KGPEN(x)		((uint16_t)1 << (x))
#define SCANLINE			0x1e
 #define SCANLINE_DONTUSE		0x0003
 #define SCANLINE_8LINES		0x0002
 #define SCANLINE_10LINES		0x0001
 #define SCANLINE_12LINES		0x0000

static unsigned long kiu_base;
static unsigned long mkiuintreg;

#ifdef CONFIG_VRC4173
#define kiu_readw(offset)		vrc4173_inw(kiu_base + (offset))
#define kiu_writew(val, offset)		vrc4173_outw(val, kiu_base + (offset))
#define mkiuintreg_writew(val)		vrc4173_outw((val), mkiuintreg)
#else
#define kiu_readw(offset)		readw(kiu_base + (offset))
#define kiu_writew(val, offset)		writew(val, kiu_base + (offset))
#define mkiuintreg_writew(val)		writew((val), mkiuintreg)
#endif

#define KIU_CLOCK			0x0008

#ifdef CONFIG_VRC4173
#define KIU_IRQ				VRC4173_KIU_IRQ
#else
#define KIU_IRQ				SYSINT1_IRQ(7)
#endif

#define KEY_UP				0
#define KEY_DOWN			1

#define DEFAULT_KIUDAT_REGS		6
#define DEFAULT_DATA_NOT_REVERSED	0
#define DEFAULT_T3CNT			KIUWKS_CNT_USEC(200)
#define DEFAULT_T2CNT			KIUWKS_CNT_USEC(200)
#define DEFAULT_T1CNT			KIUWKS_CNT_USEC(200)
#define DEFAULT_SCAN_INTERVAL		KIUWKI_INTERVAL_USEC(30000)
#define DEFAULT_REPEAT_DELAY		HZ/4
#define DEFAULT_REPEAT_RATE		HZ/25

static char *kiu_driver_name = "Keyboard driver";
static char *kiu_driver_version = "1.1";
static char *kiu_driver_revdate = "2003-09-09";
static char *kiu_driver_device_name = "NEC VR4100 series KIU";

static unsigned char kiudat_regs = DEFAULT_KIUDAT_REGS;
static unsigned char data_reverse = DEFAULT_DATA_NOT_REVERSED;
static uint16_t scanlines = SCANLINE_12LINES;
static uint16_t t3cnt = DEFAULT_T3CNT;
static uint16_t t2cnt = DEFAULT_T2CNT;
static uint16_t t1cnt = DEFAULT_T1CNT;
static uint16_t scan_interval = DEFAULT_SCAN_INTERVAL;

static unsigned long repeat_delay = DEFAULT_REPEAT_DELAY;
static unsigned long repeat_rate = DEFAULT_REPEAT_RATE;

static int repeat_scancode = -1;
static unsigned long next_handle_time;

struct kiudat_t {
	uint32_t reg;
	uint16_t data;
};

static struct kiudat_t kiudat [8] = {
	{KIUDAT0, 0}, {KIUDAT1, 0},
	{KIUDAT2, 0}, {KIUDAT3, 0},
	{KIUDAT4, 0}, {KIUDAT5, 0},
	{KIUDAT6, 0}, {KIUDAT7, 0},
};

int kbd_setkeycode(unsigned int scancode, unsigned int keycode)
{
	return (scancode == keycode) ? 0 : -EINVAL;
}

int kbd_getkeycode(unsigned int scancode)
{
	return scancode;
}

int kbd_translate(unsigned char scancode, unsigned char *keycode, char raw_mode)
{
	*keycode = scancode;
	return 1;
}

char kbd_unexpected_up(unsigned char keycode)
{
	printk(KERN_WARNING "vr41xx_keyb: unexpected up, keycode 0x%02x\n", keycode);
	return 0x80;
}

void kbd_leds(unsigned char leds)
{
	return;
}

static inline void handle_kiudat(uint16_t data, uint16_t cmp_data, int scancode)
{
	uint16_t mask;
	int down, candidate_scancode = 0;

	for (mask = 0x0001; mask ; mask <<= 1) {
		if (cmp_data & mask) {
			down = data & mask ? KEY_DOWN : KEY_UP;
			if (down == KEY_DOWN) {
				repeat_scancode = scancode;
				next_handle_time = jiffies + repeat_delay;
			}
			else {
				if (repeat_scancode == scancode)
					repeat_scancode = -1;
			}
			handle_scancode(scancode, down);
		}
		if (data & mask) {
			candidate_scancode = scancode;
		}
		scancode++;
	}

	if ((repeat_scancode < 0) && (candidate_scancode > 0)) {
			repeat_scancode = candidate_scancode;
			next_handle_time = jiffies + repeat_delay;
	}
}

static inline void handle_kiu_event(void)
{
	struct kiudat_t *kiu = kiudat;
	uint16_t data, last_data, cmp_data;
	int i;

	for (i = 0; i < kiudat_regs; i++) {
		last_data = kiu->data;
		data = kiu_readw(kiu->reg);
		if (data_reverse)
			data = ~data;
		kiu->data = data;
		cmp_data = data ^ last_data;
		handle_kiudat(data, cmp_data, i * 16);
		kiu++;
	}

	if ((repeat_scancode >= 0) &&
	    (time_after_eq(jiffies, next_handle_time))) {
		handle_scancode(repeat_scancode, KEY_DOWN);
		next_handle_time = jiffies + repeat_rate;
	}
}

static void kiu_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	uint16_t status;

	mkiuintreg_writew(0);

	status = kiu_readw(KIUINT);
	kiu_writew(KIUINT_KDATLOST | KIUINT_KDATRDY | KIUINT_SCANINT, KIUINT);

	if (status & KIUINT_KDATRDY)
		handle_kiu_event();

	mkiuintreg_writew(KIUINT_KDATLOST | KIUINT_KDATRDY);
}

#ifdef CONFIG_PM

static int pm_kiu_request(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	switch (rqst) {
	case PM_SUSPEND:
		mkiuintreg_writew(KIUINT_SCANINT);
		break;
	case PM_RESUME:
		kiu_writew(KIUINT_KDATLOST | KIUINT_KDATRDY | KIUINT_SCANINT, KIUINT);
		mkiuintreg_writew(KIUINT_KDATLOST | KIUINT_KDATRDY, MKIUINTREG);
		break;
	}

	return 0;
}

#endif

void __devinit kbd_init_hw(void)
{
	uint16_t kiugpen = 0;
	int i;

	if (current_cpu_data.cputype == CPU_VR4111 ||
	    current_cpu_data.cputype == CPU_VR4121) {
		kiu_base = KIU_BASE;
		mkiuintreg = MKIUINTREG;
#if defined(CONFIG_PCI) && defined(CONFIG_VRC4173)
	} else if (current_cpu_data.cputype == CPU_VR4122 ||
	           current_cpu_data.cputype == CPU_VR4131) {
		struct pci_dev *dev;
		int found = 0;
		dev = pci_find_device(PCI_VENDOR_ID_NEC,
		                      PCI_DEVICE_ID_NEC_VRC4173, NULL);
		if (dev != NULL) {
			switch (scanlines) {
			case SCANLINE_8LINES:
				vrc4173_select_function(KIU8_SELECT);
				found = 1;
				break;
			case SCANLINE_10LINES:
				vrc4173_select_function(KIU10_SELECT);
				found = 1;
				break;
			case SCANLINE_12LINES:
				vrc4173_select_function(KIU12_SELECT);
				found = 1;
				break;
			default:
				break;
			}

			if (found != 0) {
				kiu_base = VRC4173_KIU_OFFSET;
				mkiuintreg = VRC4173_MKIUINTREG_OFFSET;
				vrc4173_clock_supply(VRC4173_KIU_CLOCK);
			}
		}
#endif
	}

	if (kiu_base == 0 || mkiuintreg == 0)
		return;

	printk(KERN_INFO "%s version %s (%s) for %s\n",
	       kiu_driver_name, kiu_driver_version,
	       kiu_driver_revdate, kiu_driver_device_name);

	mkiuintreg_writew(0);

	if (current_cpu_data.cputype == CPU_VR4111 ||
	    current_cpu_data.cputype == CPU_VR4121)
		vr41xx_clock_supply(KIU_CLOCK);

	kiu_writew(KIURST_KIURST, KIURST);

	for (i = 0; i < scanlines; i++)
		kiugpen &= ~(0x0001 << i);

	kiu_writew(kiugpen, KIUGPEN);
	kiu_writew(scanlines, SCANLINE);
	kiu_writew((t3cnt << KIUWKS_T3CNT_SHIFT) |
	           (t2cnt << KIUWKS_T2CNT_SHIFT) |
	           (t1cnt << KIUWKS_T1CNT_SHIFT), KIUWKS);
	kiu_writew(scan_interval, KIUWKI);
	kiu_writew(KIUINT_KDATLOST | KIUINT_KDATRDY | KIUINT_SCANINT, KIUINT);


	request_irq(KIU_IRQ, kiu_interrupt, 0, "keyboard", NULL);

	mkiuintreg_writew(KIUINT_KDATLOST | KIUINT_KDATRDY);
	kiu_writew(KIUSCANREP_KEYEN | KIUSCANREP_STPREP(1) |
	       KIUSCANREP_ATSTP | KIUSCANREP_ATSCAN, KIUSCANREP);

#ifdef CONFIG_PM
	pm_register(PM_SYS_DEV, PM_SYS_KBC, pm_kiu_request);
#endif
}

static int __devinit vr41xx_kbd_setup(char *options)
{
	char *this_opt;
	int num;

	if (!options || !*options)
		return 1;

	for (this_opt = strtok(options, ","); this_opt; this_opt = strtok(NULL, ",")) {
		if (!strncmp(this_opt, "regs:", 5)) {
			num = simple_strtoul(this_opt+5, NULL, 0);
			if (num == 6 || num == 8)
				kiudat_regs = num;
		} else if (!strncmp(this_opt, "lines:", 6)) {
			num = simple_strtoul(this_opt+6, NULL, 0);
			if (num == 8)
				scanlines = SCANLINE_8LINES;
			else if (num == 10)
				scanlines = SCANLINE_10LINES;
			else if (num == 12)
				scanlines = SCANLINE_12LINES;
		} else if (!strncmp(this_opt, "reverse:", 8)) {
			num = simple_strtoul(this_opt+8, NULL, 0);
			if (num == 0 || num == 1)
				data_reverse = num;
		} else if (!strncmp(this_opt, "t3cnt:", 6)) {
			num = simple_strtoul(this_opt+6, NULL, 0);
			if (num >= 60 && num <= 960)
				t3cnt = KIUWKS_CNT_USEC(num);
		} else if (!strncmp(this_opt, "t2cnt:", 6)) {
			num = simple_strtoul(this_opt+6, NULL, 0);
			if (num >= 60 && num <= 960)
				t2cnt = KIUWKS_CNT_USEC(num);
		} else if (!strncmp(this_opt, "t1cnt:", 6)) {
			num = simple_strtoul(this_opt+6, NULL, 0);
			if (num >= 60 && num <= 960)
				t1cnt = KIUWKS_CNT_USEC(num);
		} else if (!strncmp(this_opt, "interval:", 9)) {
			num = simple_strtoul(this_opt+9, NULL, 0);
			if (num >= 30 && num <= 30690)
				scan_interval = KIUWKI_INTERVAL_USEC(num);
		} else if (!strncmp(this_opt, "delay:", 6)) {
			num = simple_strtoul(this_opt+6, NULL, 0);
			if (num > 0 && num <= HZ)
				repeat_delay = num;
		} else if (!strncmp(this_opt, "rate:", 5)) {
			num = simple_strtoul(this_opt+5, NULL, 0);
			if (num > 0 && num <= HZ)
				repeat_rate = num;
		}
	}

	return 1;
}

__setup("vr41xx_kbd=", vr41xx_kbd_setup);
