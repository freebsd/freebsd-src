/*
 *    Chassis LCD/LED driver for HP-PARISC workstations
 *
 *      (c) Copyright 2000 Red Hat Software
 *      (c) Copyright 2000 Helge Deller <hdeller@redhat.com>
 *      (c) Copyright 2001-2002 Helge Deller <deller@gmx.de>
 *      (c) Copyright 2001 Randolph Chung <tausq@debian.org>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 * TODO:
 *	- speed-up calculations with inlined assembler
 *	- interface to write to second row of LCD from /proc
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/stddef.h>	/* for offsetof() */
#include <linux/init.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/bitops.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/reboot.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <asm/io.h>
#include <asm/gsc.h>
#include <asm/processor.h>
#include <asm/hardware.h>
#include <asm/param.h>		/* HZ */
#include <asm/led.h>
#include <asm/pdc.h>
#include <asm/uaccess.h>

/* The control of the LEDs and LCDs on PARISC-machines have to be done 
   completely in software. The necessary calculations are done in a tasklet
   which is scheduled at every timer interrupt and since the calculations 
   may consume relatively much CPU-time some of the calculations can be 
   turned off with the following variables (controlled via procfs) */

static int led_type = -1;
static int led_heartbeat = 1;
static int led_diskio = 1;
static int led_lanrxtx = 1;
static char lcd_text[32];

#if 0
#define DPRINTK(x)	printk x
#else
#define DPRINTK(x)
#endif


#define CALC_ADD(val, comp, add) \
 (val<=(comp/8) ? add/16 : val<=(comp/4) ? add/8 : val<=(comp/2) ? add/4 : add)


struct lcd_block {
	unsigned char command;	/* stores the command byte      */
	unsigned char on;	/* value for turning LED on     */
	unsigned char off;	/* value for turning LED off    */
};

/* Structure returned by PDC_RETURN_CHASSIS_INFO */
/* NOTE: we use unsigned long:16 two times, since the following member 
   lcd_cmd_reg_addr needs to be 64bit aligned on 64bit PA2.0-machines */
struct pdc_chassis_lcd_info_ret_block {
	unsigned long model:16;		/* DISPLAY_MODEL_XXXX */
	unsigned long lcd_width:16;	/* width of the LCD in chars (DISPLAY_MODEL_LCD only) */
	char *lcd_cmd_reg_addr;		/* ptr to LCD cmd-register & data ptr for LED */
	char *lcd_data_reg_addr;	/* ptr to LCD data-register (LCD only) */
	unsigned int min_cmd_delay;	/* delay in uS after cmd-write (LCD only) */
	unsigned char reset_cmd1;	/* command #1 for writing LCD string (LCD only) */
	unsigned char reset_cmd2;	/* command #2 for writing LCD string (LCD only) */
	unsigned char act_enable;	/* 0 = no activity (LCD only) */
	struct lcd_block heartbeat;
	struct lcd_block disk_io;
	struct lcd_block lan_rcv;
	struct lcd_block lan_tx;
	char _pad;
};


/* LCD_CMD and LCD_DATA for KittyHawk machines */
#define KITTYHAWK_LCD_CMD  (0xfffffffff0190000UL) /* 64bit-ready */
#define KITTYHAWK_LCD_DATA (KITTYHAWK_LCD_CMD+1)

/* lcd_info is pre-initialized to the values needed to program KittyHawk LCD's 
 * HP seems to have used Sharp/Hitachi HD44780 LCDs most of the time. */
static struct pdc_chassis_lcd_info_ret_block
lcd_info __attribute__((aligned(8))) =
{
      model:		DISPLAY_MODEL_LCD,
      lcd_width:	16,
      lcd_cmd_reg_addr:	(char *) KITTYHAWK_LCD_CMD,
      lcd_data_reg_addr:(char *) KITTYHAWK_LCD_DATA,
      min_cmd_delay:	40,
      reset_cmd1:	0x80,
      reset_cmd2:	0xc0,
};


/* direct access to some of the lcd_info variables */
#define LCD_CMD_REG	lcd_info.lcd_cmd_reg_addr	 
#define LCD_DATA_REG	lcd_info.lcd_data_reg_addr	 
#define LED_DATA_REG	lcd_info.lcd_cmd_reg_addr	/* LASI & ASP only */


/* ptr to LCD/LED-specific function */
static void (*led_func_ptr) (unsigned char);

#define LED_HASLCD 1
#define LED_NOLCD  0
#ifdef CONFIG_PROC_FS
static int led_proc_read(char *page, char **start, off_t off, int count, 
	int *eof, void *data)
{
	char *out = page;
	int len;

	switch ((long)data)
	{
	case LED_NOLCD:
		out += sprintf(out, "Heartbeat: %d\n", led_heartbeat);
		out += sprintf(out, "Disk IO: %d\n", led_diskio);
		out += sprintf(out, "LAN Rx/Tx: %d\n", led_lanrxtx);
		break;
	case LED_HASLCD:
		out += sprintf(out, "%s\n", lcd_text);
		break;
	default:
		*eof = 1;
		return 0;
	}

	len = out - page - off;
	if (len < count) {
		*eof = 1;
		if (len <= 0) return 0;
	} else {
		len = count;
	}
	*start = page + off;
	return len;
}

static int led_proc_write(struct file *file, const char *buf, 
	unsigned long count, void *data)
{
	char *cur, lbuf[count];
	int d;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	memset(lbuf, 0, count);

	copy_from_user(lbuf, buf, count);
	cur = lbuf;

	/* skip initial spaces */
	while (*cur && isspace(*cur))
	{
		cur++;
	}

	switch ((long)data)
	{
	case LED_NOLCD:
		d = *cur++ - '0';
		if (d != 0 && d != 1) goto parse_error;
		led_heartbeat = d;

		if (*cur++ != ' ') goto parse_error;

		d = *cur++ - '0';
		if (d != 0 && d != 1) goto parse_error;
		led_diskio = d;

		if (*cur++ != ' ') goto parse_error;

		d = *cur++ - '0';
		if (d != 0 && d != 1) goto parse_error;
		led_lanrxtx = d;

		break;
	case LED_HASLCD:
		if (*cur == 0) 
		{
			/* reset to default */
			lcd_print("Linux " UTS_RELEASE);
		}
		else
		{
			/* chop off trailing \n.. if the user gives multiple
			 * \n then it's all their fault.. */
			if (*cur && cur[strlen(cur)-1] == '\n')
				cur[strlen(cur)-1] = 0;
			lcd_print(cur);
		}
		break;
	default:
		return 0;
	}
	
	return count;

parse_error:
	if ((long)data == LED_NOLCD)
		printk(KERN_CRIT "Parse error: expect \"n n n\" (n == 0 or 1) for heartbeat,\ndisk io and lan tx/rx indicators\n");
	return -EINVAL;
}

static int __init led_create_procfs(void)
{
	struct proc_dir_entry *proc_pdc_root = NULL;
	struct proc_dir_entry *ent;

	if (led_type == -1) return -1;

	proc_pdc_root = proc_mkdir("pdc", 0);
	if (!proc_pdc_root) return -1;
	proc_pdc_root->owner = THIS_MODULE;
	ent = create_proc_entry("led", S_IFREG|S_IRUGO|S_IWUSR, proc_pdc_root);
	if (!ent) return -1;
	ent->nlink = 1;
	ent->data = (void *)LED_NOLCD; /* LED */
	ent->read_proc = led_proc_read;
	ent->write_proc = led_proc_write;
	ent->owner = THIS_MODULE;

	if (led_type == LED_HASLCD)
	{
		ent = create_proc_entry("lcd", S_IFREG|S_IRUGO|S_IWUSR, proc_pdc_root);
		if (!ent) return -1;
		ent->nlink = 1;
		ent->data = (void *)LED_HASLCD; /* LCD */
		ent->read_proc = led_proc_read;
		ent->write_proc = led_proc_write;
		ent->owner = THIS_MODULE;
	}

	return 0;
}
#endif

/*
   ** 
   ** led_ASP_driver()
   ** 
 */
#define	LED_DATA	0x01	/* data to shift (0:on 1:off) */
#define	LED_STROBE	0x02	/* strobe to clock data */
static void led_ASP_driver(unsigned char leds)
{
	int i;

	leds = ~leds;
	for (i = 0; i < 8; i++) {
		unsigned char value;
		value = (leds & 0x80) >> 7;
		gsc_writeb( value,		 LED_DATA_REG );
		gsc_writeb( value | LED_STROBE,	 LED_DATA_REG );
		leds <<= 1;
	}
}


/*
   ** 
   ** led_LASI_driver()
   ** 
 */
static void led_LASI_driver(unsigned char leds)
{
	leds = ~leds;
	gsc_writeb( leds, LED_DATA_REG );
}


/*
   ** 
   ** led_LCD_driver()
   ** 
   ** The logic of the LCD driver is, that we write at every scheduled call
   ** only to one of LCD_CMD_REG _or_ LCD_DATA_REG - registers.
   ** That way we don't need to let this tasklet busywait for min_cmd_delay
   ** milliseconds.
   **
   ** TODO: check the value of "min_cmd_delay" against the value of HZ.
   **   
 */
static void led_LCD_driver(unsigned char leds)
{
	static int last_index;	/* 0:heartbeat, 1:disk, 2:lan_in, 3:lan_out */
	static int last_was_cmd;/* 0: CMD was written last, 1: DATA was last */
	struct lcd_block *block_ptr;
	int value;

	switch (last_index) {
	    case 0:	block_ptr = &lcd_info.heartbeat;
			value = leds & LED_HEARTBEAT;
			break;
	    case 1:	block_ptr = &lcd_info.disk_io;
			value = leds & LED_DISK_IO;
			break;					
	    case 2:	block_ptr = &lcd_info.lan_rcv;
			value = leds & LED_LAN_RCV;
			break;					
	    case 3:	block_ptr = &lcd_info.lan_tx;
			value = leds & LED_LAN_TX;
			break;
	    default:	/* should never happen: */
			return;
	}

	if (last_was_cmd) {
	    /* write the value to the LCD data port */
    	    gsc_writeb( value ? block_ptr->on : block_ptr->off, LCD_DATA_REG );
	} else {
	    /* write the command-byte to the LCD command register */
    	    gsc_writeb( block_ptr->command, LCD_CMD_REG );
	}    
	
	/* now update the vars for the next interrupt iteration */ 
	if (++last_was_cmd == 2) { /* switch between cmd & data */
	    last_was_cmd = 0;
	    if (++last_index == 4) 
		last_index = 0;	 /* switch back to heartbeat index */
	}
}


/*
   ** 
   ** led_get_net_stats()
   ** 
   ** calculate the TX- & RX-troughput on the network interfaces in
   ** the system for usage in the LED code
   **
   ** (analog to dev_get_info() from net/core/dev.c)
   **   
 */
static unsigned long led_net_rx_counter, led_net_tx_counter;

static void led_get_net_stats(int addvalue)
{ 
#ifdef CONFIG_NET
	static unsigned long rx_total_last, tx_total_last;
	unsigned long rx_total, tx_total;
	struct net_device *dev;
	struct net_device_stats *stats;

	rx_total = tx_total = 0;
	
	/* we are running as a tasklet, so locking dev_base 
	 * for reading should be OK */
	read_lock(&dev_base_lock);
	for (dev = dev_base; dev != NULL; dev = dev->next) {
	    if (dev->get_stats) { 
	        stats = dev->get_stats(dev);
		rx_total += stats->rx_packets;
		tx_total += stats->tx_packets;
	    }
	}
	read_unlock(&dev_base_lock);

	rx_total -= rx_total_last;
	tx_total -= tx_total_last;
	
	if (rx_total)
	    led_net_rx_counter += CALC_ADD(rx_total, tx_total, addvalue);

	if (tx_total)
	    led_net_tx_counter += CALC_ADD(tx_total, rx_total, addvalue);
        
	rx_total_last += rx_total;
        tx_total_last += tx_total;
#endif
}


/*
   ** 
   ** led_get_diskio_stats()
   ** 
   ** calculate the disk-io througput in the system
   ** (analog to linux/fs/proc/proc_misc.c)
   **   
 */
static unsigned long led_diskio_counter;

static void led_get_diskio_stats(int addvalue)
{	
	static unsigned int diskio_total_last, diskio_max;
	int major, disk, total;
	
	total = 0;
	for (major = 0; major < DK_MAX_MAJOR; major++) {
	    for (disk = 0; disk < DK_MAX_DISK; disk++)
		total += kstat.dk_drive[major][disk];
	}
	total -= diskio_total_last;
	
	if (total) {
	    if (total >= diskio_max) {
		led_diskio_counter += addvalue;
	        diskio_max = total; /* new maximum value found */ 
	    } else
		led_diskio_counter += CALC_ADD(total, diskio_max, addvalue);
	}
	
	diskio_total_last += total; 
}



/*
   ** led_tasklet_func()
   ** 
   ** is scheduled at every timer interrupt from time.c and
   ** updates the chassis LCD/LED 

    TODO:
    - display load average (older machines like 715/64 have 4 "free" LED's for that)
    - optimizations
 */

static unsigned char currentleds;	/* stores current value of the LEDs */

#define HEARTBEAT_LEN (HZ*6/100)
#define HEARTBEAT_2ND_RANGE_START (HZ*22/100)
#define HEARTBEAT_2ND_RANGE_END   (HEARTBEAT_2ND_RANGE_START + HEARTBEAT_LEN)

static void led_tasklet_func(unsigned long unused)
{
	static unsigned int count, count_HZ;
	static unsigned char lastleds;

	/* exit if not initialized */
	if (!led_func_ptr)
	    return;

	/* increment the local counters */
	++count;
	if (++count_HZ == HZ)
	    count_HZ = 0;

	if (led_heartbeat)
	{
		/* flash heartbeat-LED like a real heart (2 x short then a long delay) */
		if (count_HZ<HEARTBEAT_LEN || 
		    (count_HZ>=HEARTBEAT_2ND_RANGE_START && count_HZ<HEARTBEAT_2ND_RANGE_END)) 
		    currentleds |= LED_HEARTBEAT;
		else
		    currentleds &= ~LED_HEARTBEAT;
	}

	/* gather network and diskio statistics and flash LEDs respectively */

	if (led_lanrxtx)
	{
		if ((count & 31) == 0)
			led_get_net_stats(30);

		if (led_net_rx_counter) {
			led_net_rx_counter--;
			currentleds |= LED_LAN_RCV;
		}
		else    
			currentleds &= ~LED_LAN_RCV;

		if (led_net_tx_counter) {
			led_net_tx_counter--;
			currentleds |= LED_LAN_TX;
		}
		else    
			currentleds &= ~LED_LAN_TX;
	}

	if (led_diskio)
	{
		/* avoid to calculate diskio-stats at same irq as netio-stats ! */
		if ((count & 31) == 15) 
			led_get_diskio_stats(30);

		if (led_diskio_counter) {
			led_diskio_counter--;
			currentleds |= LED_DISK_IO;
		}
		else    
			currentleds &= ~LED_DISK_IO;
	}

	/* update the LCD/LEDs */
	if (currentleds != lastleds) {
	    led_func_ptr(currentleds);
	    lastleds = currentleds;
	}
}

/* main led tasklet struct (scheduled from time.c) */
DECLARE_TASKLET_DISABLED(led_tasklet, led_tasklet_func, 0);


/*
   ** led_halt()
   ** 
   ** called by the reboot notifier chain at shutdown and stops all
   ** LED/LCD activities.
   ** 
 */

static int led_halt(struct notifier_block *, unsigned long, void *);

static struct notifier_block led_notifier = {
	notifier_call: led_halt,
};

static int led_halt(struct notifier_block *nb, unsigned long event, void *buf) 
{
	char *txt;
	
	switch (event) {
	case SYS_RESTART:	txt = "SYSTEM RESTART";
				break;
	case SYS_HALT:		txt = "SYSTEM HALT";
				break;
	case SYS_POWER_OFF:	txt = "SYSTEM POWER OFF";
				break;
	default:		return NOTIFY_DONE;
	}
	
	/* completely stop the LED/LCD tasklet */
	tasklet_disable(&led_tasklet);

	if (lcd_info.model == DISPLAY_MODEL_LCD)
		lcd_print(txt);
	else
		if (led_func_ptr)
			led_func_ptr(0xff); /* turn all LEDs ON */
	
	unregister_reboot_notifier(&led_notifier);
	return NOTIFY_OK;
}

/*
   ** register_led_driver()
   ** 
   ** registers an external LED or LCD for usage by this driver.
   ** currently only LCD-, LASI- and ASP-style LCD/LED's are supported.
   ** 
 */

int __init register_led_driver(int model, char *cmd_reg, char *data_reg)
{
	static int initialized;
	
	if (initialized || !data_reg)
	    return 1;
	
	lcd_info.model = model;		/* store the values */
	LCD_CMD_REG = (cmd_reg == LED_CMD_REG_NONE) ? NULL : cmd_reg;

	switch (lcd_info.model) {
	case DISPLAY_MODEL_LCD:
		LCD_DATA_REG = data_reg;
		printk(KERN_INFO "LCD display at %p,%p registered\n", 
			LCD_CMD_REG , LCD_DATA_REG);
		led_func_ptr = led_LCD_driver;
		lcd_print( "Linux " UTS_RELEASE );
		led_type = LED_HASLCD;
		break;

	case DISPLAY_MODEL_LASI:
		LED_DATA_REG = data_reg;
		led_func_ptr = led_LASI_driver;
		printk(KERN_INFO "LED display at %p registered\n", LED_DATA_REG);
		led_type = LED_NOLCD;
		break;

	case DISPLAY_MODEL_OLD_ASP:
		LED_DATA_REG = data_reg;
		led_func_ptr = led_ASP_driver;
		printk(KERN_INFO "LED (ASP-style) display at %p registered\n", 
		    LED_DATA_REG);
		led_type = LED_NOLCD;
		break;

	default:
		printk(KERN_ERR "%s: Wrong LCD/LED model %d !\n",
		       __FUNCTION__, lcd_info.model);
		return 1;
	}
	
	/* mark the LCD/LED driver now as initialized and 
	 * register to the reboot notifier chain */
	initialized++;
	register_reboot_notifier(&led_notifier);

	/* start the led tasklet for the first time */
	tasklet_enable(&led_tasklet);
	
	return 0;
}

/*
   ** register_led_regions()
   ** 
   ** register_led_regions() registers the LCD/LED regions for /procfs.
   ** At bootup - where the initialisation of the LCD/LED normally happens - 
   ** not all internal structures of request_region() are properly set up,
   ** so that we delay the led-registration until after busdevices_init() 
   ** has been executed.
   **
 */

void __init register_led_regions(void)
{
	switch (lcd_info.model) {
	case DISPLAY_MODEL_LCD:
		request_mem_region((unsigned long)LCD_CMD_REG,  1, "lcd_cmd");
		request_mem_region((unsigned long)LCD_DATA_REG, 1, "lcd_data");
		break;
	case DISPLAY_MODEL_LASI:
	case DISPLAY_MODEL_OLD_ASP:
		request_mem_region((unsigned long)LED_DATA_REG, 1, "led_data");
		break;
	}
}


/*
   ** 
   ** lcd_print()
   ** 
   ** Displays the given string on the LCD-Display of newer machines.
   ** lcd_print() disables the timer-based led tasklet during its 
   ** execution and enables it afterwards again.
   **
 */
int lcd_print( char *str )
{
	int i;

	if (!led_func_ptr || lcd_info.model != DISPLAY_MODEL_LCD)
	    return 0;
	
	/* temporarily disable the led tasklet */
	tasklet_disable(&led_tasklet);

	/* copy display string to buffer for procfs */
	strncpy(lcd_text, str, sizeof(lcd_text)-1);
	
	/* Set LCD Cursor to 1st character */
	gsc_writeb(lcd_info.reset_cmd1, LCD_CMD_REG);
	udelay(lcd_info.min_cmd_delay);

	/* Print the string */
	for (i=0; i < lcd_info.lcd_width; i++) {
	    if (str && *str)
		gsc_writeb(*str++, LCD_DATA_REG);
	    else
		gsc_writeb(' ', LCD_DATA_REG);
	    udelay(lcd_info.min_cmd_delay);
	}
	
	/* re-enable the led tasklet */
	tasklet_enable(&led_tasklet);

	return lcd_info.lcd_width;
}

/*
   ** led_init()
   ** 
   ** led_init() is called very early in the bootup-process from setup.c 
   ** and asks the PDC for an usable chassis LCD or LED.
   ** If the PDC doesn't return any info, then the LED
   ** is detected by lasi.c or asp.c and registered with the
   ** above functions lasi_led_init() or asp_led_init().
   ** KittyHawk machines have often a buggy PDC, so that
   ** we explicitly check for those machines here.
 */

int __init led_init(void)
{
	struct pdc_chassis_info chassis_info;
	int ret;

	/* Work around the buggy PDC of KittyHawk-machines */
	switch (CPU_HVERSION) {
	case 0x580:		/* KittyHawk DC2-100 (K100) */
	case 0x581:		/* KittyHawk DC3-120 (K210) */
	case 0x582:		/* KittyHawk DC3 100 (K400) */
	case 0x583:		/* KittyHawk DC3 120 (K410) */
	case 0x58B:		/* KittyHawk DC2 100 (K200) */
		printk(KERN_INFO "%s: KittyHawk-Machine (hversion 0x%x) found, "
				"LED detection skipped.\n", __FILE__, CPU_HVERSION);
		goto found;	/* use the preinitialized values of lcd_info */
	}

	/* initialize the struct, so that we can check for valid return values */
	lcd_info.model = DISPLAY_MODEL_NONE;
	chassis_info.actcnt = chassis_info.maxcnt = 0;

	if ((ret = pdc_chassis_info(&chassis_info, &lcd_info, sizeof(lcd_info))) == PDC_OK) {
		DPRINTK((KERN_INFO "%s: chassis info: model=%d (%s), "
			 "lcd_width=%d, cmd_delay=%u,\n"
			 "%s: sizecnt=%d, actcnt=%ld, maxcnt=%ld\n",
		         __FILE__, lcd_info.model,
			 (lcd_info.model==DISPLAY_MODEL_LCD) ? "LCD" :
			  (lcd_info.model==DISPLAY_MODEL_LASI) ? "LED" : "unknown",
			 lcd_info.lcd_width, lcd_info.min_cmd_delay,
			 __FILE__, sizeof(lcd_info), 
			 chassis_info.actcnt, chassis_info.maxcnt));
		DPRINTK((KERN_INFO "%s: cmd=%p, data=%p, reset1=%x, reset2=%x, act_enable=%d\n",
			__FILE__, lcd_info.lcd_cmd_reg_addr, 
			lcd_info.lcd_data_reg_addr, lcd_info.reset_cmd1,  
			lcd_info.reset_cmd2, lcd_info.act_enable ));
	
		/* check the results. Some machines have a buggy PDC */
		if (chassis_info.actcnt <= 0 || chassis_info.actcnt != chassis_info.maxcnt)
			goto not_found;

		switch (lcd_info.model) {
		case DISPLAY_MODEL_LCD:		/* LCD display */
			if (chassis_info.actcnt < 
				offsetof(struct pdc_chassis_lcd_info_ret_block, _pad)-1)
				goto not_found;
			if (!lcd_info.act_enable) {
				DPRINTK((KERN_INFO "PDC prohibited usage of the LCD.\n"));
				goto not_found;
			}
			break;

		case DISPLAY_MODEL_NONE:	/* no LED or LCD available */
			printk(KERN_INFO "PDC reported no LCD or LED.\n");
			goto not_found;

		case DISPLAY_MODEL_LASI:	/* Lasi style 8 bit LED display */
			if (chassis_info.actcnt != 8 && chassis_info.actcnt != 32)
				goto not_found;
			break;

		default:
			printk(KERN_WARNING "PDC reported unknown LCD/LED model %d\n",
			       lcd_info.model);
			goto not_found;
		} /* switch() */

found:
		/* register the LCD/LED driver */
		register_led_driver(lcd_info.model, LCD_CMD_REG, LCD_DATA_REG);
		return 0;

	} else { /* if() */
		DPRINTK((KERN_INFO "pdc_chassis_info call failed with retval = %d\n", ret));
	}

not_found:
	lcd_info.model = DISPLAY_MODEL_NONE;
	return 1;
}

#ifdef CONFIG_PROC_FS
module_init(led_create_procfs)
#endif
