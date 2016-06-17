/*
 * i8k.c -- Linux driver for accessing the SMM BIOS on Dell laptops.
 *	    See http://www.debian.org/~dz/i8k/ for more information
 *	    and for latest version of this driver.
 *
 * Copyright (C) 2001  Massimo Dal Zotto <dz@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/apm_bios.h>
#include <linux/kbd_kern.h>
#include <linux/kbd_ll.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/i8k.h>

#define I8K_VERSION		"1.13 14/05/2002"

#define I8K_SMM_FN_STATUS	0x0025
#define I8K_SMM_POWER_STATUS	0x0069
#define I8K_SMM_SET_FAN		0x01a3
#define I8K_SMM_GET_FAN		0x00a3
#define I8K_SMM_GET_SPEED	0x02a3
#define I8K_SMM_GET_TEMP	0x10a3
#define I8K_SMM_GET_DELL_SIG	0xffa3
#define I8K_SMM_BIOS_VERSION	0x00a6

#define I8K_FAN_MULT		30
#define I8K_MAX_TEMP		127

#define I8K_FN_NONE		0x00
#define I8K_FN_UP		0x01
#define I8K_FN_DOWN		0x02
#define I8K_FN_MUTE		0x04
#define I8K_FN_MASK		0x07
#define I8K_FN_SHIFT		8

#define I8K_POWER_AC		0x05
#define I8K_POWER_BATTERY	0x01

#define I8K_TEMPERATURE_BUG	1

#define DELL_SIGNATURE		"Dell Computer"

/* Interval between polling of keys, in jiffies. */
#define I8K_POLL_INTERVAL	(HZ/20)
#define I8K_REPEAT_DELAY	250	/* 250 ms */
#define I8K_REPEAT_RATE		10

/*
 * (To be escaped) Scancodes for the keys.  These were chosen to match other
 * "Internet" keyboards.
 */
#define I8K_KEYS_UP_SCANCODE	0x30
#define I8K_KEYS_DOWN_SCANCODE	0x2e
#define I8K_KEYS_MUTE_SCANCODE	0x20

static char *supported_models[] = {
    "Inspiron",
    "Latitude",
    NULL
};

static char system_vendor[48] = "?";
static char product_name [48] = "?";
static char bios_version [4]  = "?";
static char serial_number[16] = "?";

int force = 0;
int restricted = 0;
int handle_buttons = 0;
int repeat_delay = I8K_REPEAT_DELAY;
int repeat_rate = I8K_REPEAT_RATE;
int power_status = 0;

static struct timer_list  i8k_keys_timer;

MODULE_AUTHOR("Massimo Dal Zotto (dz@debian.org)");
MODULE_DESCRIPTION("Driver for accessing SMM BIOS on Dell laptops");
MODULE_LICENSE("GPL");
MODULE_PARM(force, "i");
MODULE_PARM(restricted, "i");
MODULE_PARM(handle_buttons, "i");
MODULE_PARM(repeat_delay, "i");
MODULE_PARM(repeat_rate, "i");
MODULE_PARM(power_status, "i");
MODULE_PARM_DESC(force, "Force loading without checking for supported models");
MODULE_PARM_DESC(restricted, "Allow fan control if SYS_ADMIN capability set");
MODULE_PARM_DESC(handle_buttons, "Generate keyboard events for i8k buttons");
MODULE_PARM_DESC(repeat_delay, "I8k buttons repeat delay (ms)");
MODULE_PARM_DESC(repeat_rate, "I8k buttons repeat rate");
MODULE_PARM_DESC(power_status, "Report power status in /proc/i8k");

static ssize_t i8k_read(struct file *, char *, size_t, loff_t *);
static int i8k_ioctl(struct inode *, struct file *, unsigned int,
		     unsigned long);
static void i8k_keys_set_timer(void);

static struct file_operations i8k_fops = {
    read:	i8k_read,
    ioctl:	i8k_ioctl,
};

typedef struct {
    unsigned int eax;
    unsigned int ebx __attribute__ ((packed));
    unsigned int ecx __attribute__ ((packed));
    unsigned int edx __attribute__ ((packed));
    unsigned int esi __attribute__ ((packed));
    unsigned int edi __attribute__ ((packed));
} SMMRegisters;

typedef struct {
    u8	type;
    u8	length;
    u16	handle;
} DMIHeader;

/*
 * Call the System Management Mode BIOS. Code provided by Jonathan Buzzard.
 */
static int i8k_smm(SMMRegisters *regs)
{
    int rc;
    int eax = regs->eax;

    asm("pushl %%eax\n\t" \
	"movl 0(%%eax),%%edx\n\t" \
	"push %%edx\n\t" \
	"movl 4(%%eax),%%ebx\n\t" \
	"movl 8(%%eax),%%ecx\n\t" \
	"movl 12(%%eax),%%edx\n\t" \
	"movl 16(%%eax),%%esi\n\t" \
	"movl 20(%%eax),%%edi\n\t" \
	"popl %%eax\n\t" \
	"out %%al,$0xb2\n\t" \
	"out %%al,$0x84\n\t" \
	"xchgl %%eax,(%%esp)\n\t"
	"movl %%ebx,4(%%eax)\n\t" \
	"movl %%ecx,8(%%eax)\n\t" \
	"movl %%edx,12(%%eax)\n\t" \
	"movl %%esi,16(%%eax)\n\t" \
	"movl %%edi,20(%%eax)\n\t" \
	"popl %%edx\n\t" \
	"movl %%edx,0(%%eax)\n\t" \
	"lahf\n\t" \
	"shrl $8,%%eax\n\t" \
	"andl $1,%%eax\n" \
	: "=a" (rc)
	: "a" (regs)
	: "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory");

    if ((rc != 0) || ((regs->eax & 0xffff) == 0xffff) || (regs->eax == eax)) {
	return -EINVAL;
    }

    return 0;
}

/*
 * Read the bios version. Return the version as an integer corresponding
 * to the ascii value, for example "A17" is returned as 0x00413137.
 */
static int i8k_get_bios_version(void)
{
    SMMRegisters regs = { 0, 0, 0, 0, 0, 0 };
    int rc;

    regs.eax = I8K_SMM_BIOS_VERSION;
    if ((rc=i8k_smm(&regs)) < 0) {
	return rc;
    }

    return regs.eax;
}

/*
 * Read the machine id.
 */
static int i8k_get_serial_number(unsigned char *buff)
{
    strncpy(buff, serial_number, 16);
    return 0;
}

/*
 * Read the Fn key status.
 */
static int i8k_get_fn_status(void)
{
    SMMRegisters regs = { 0, 0, 0, 0, 0, 0 };
    int rc;

    regs.eax = I8K_SMM_FN_STATUS;
    if ((rc=i8k_smm(&regs)) < 0) {
	return rc;
    }

    switch ((regs.eax >> I8K_FN_SHIFT) & I8K_FN_MASK) {
    case I8K_FN_UP:
	return I8K_VOL_UP;
    case I8K_FN_DOWN:
	return I8K_VOL_DOWN;
    case I8K_FN_MUTE:
	return I8K_VOL_MUTE;
    default:
	return 0;
    }
}

/*
 * Read the power status.
 */
static int i8k_get_power_status(void)
{
    SMMRegisters regs = { 0, 0, 0, 0, 0, 0 };
    int rc;

    regs.eax = I8K_SMM_POWER_STATUS;
    if ((rc=i8k_smm(&regs)) < 0) {
	return rc;
    }

    switch (regs.eax & 0xff) {
    case I8K_POWER_AC:
	return I8K_AC;
    default:
	return I8K_BATTERY;
    }
}

/*
 * Read the fan status.
 */
static int i8k_get_fan_status(int fan)
{
    SMMRegisters regs = { 0, 0, 0, 0, 0, 0 };
    int rc;

    regs.eax = I8K_SMM_GET_FAN;
    regs.ebx = fan & 0xff;
    if ((rc=i8k_smm(&regs)) < 0) {
	return rc;
    }

    return (regs.eax & 0xff);
}

/*
 * Read the fan speed in RPM.
 */
static int i8k_get_fan_speed(int fan)
{
    SMMRegisters regs = { 0, 0, 0, 0, 0, 0 };
    int rc;

    regs.eax = I8K_SMM_GET_SPEED;
    regs.ebx = fan & 0xff;
    if ((rc=i8k_smm(&regs)) < 0) {
	return rc;
    }

    return (regs.eax & 0xffff) * I8K_FAN_MULT;
}

/*
 * Set the fan speed (off, low, high). Returns the new fan status.
 */
static int i8k_set_fan(int fan, int speed)
{
    SMMRegisters regs = { 0, 0, 0, 0, 0, 0 };
    int rc;

    speed = (speed < 0) ? 0 : ((speed > I8K_FAN_MAX) ? I8K_FAN_MAX : speed);

    regs.eax = I8K_SMM_SET_FAN;
    regs.ebx = (fan & 0xff) | (speed << 8);
    if ((rc=i8k_smm(&regs)) < 0) {
	return rc;
    }

    return (i8k_get_fan_status(fan));
}

/*
 * Read the cpu temperature.
 */
static int i8k_get_cpu_temp(void)
{
    SMMRegisters regs = { 0, 0, 0, 0, 0, 0 };
    int rc;
    int temp;

#ifdef I8K_TEMPERATURE_BUG
    static int prev = 0;
#endif

    regs.eax = I8K_SMM_GET_TEMP;
    if ((rc=i8k_smm(&regs)) < 0) {
	return rc;
    }
    temp = regs.eax & 0xff;

#ifdef I8K_TEMPERATURE_BUG
    /*
     * Sometimes the temperature sensor returns 0x99, which is out of range.
     * In this case we return (once) the previous cached value. For example:
     # 1003655137 00000058 00005a4b
     # 1003655138 00000099 00003a80 <--- 0x99 = 153 degrees
     # 1003655139 00000054 00005c52
     */
    if (temp > I8K_MAX_TEMP) {
	temp = prev;
	prev = I8K_MAX_TEMP;
    } else {
	prev = temp;
    }
#endif

    return temp;
}

static int i8k_get_dell_signature(void)
{
    SMMRegisters regs = { 0, 0, 0, 0, 0, 0 };
    int rc;

    regs.eax = I8K_SMM_GET_DELL_SIG;
    if ((rc=i8k_smm(&regs)) < 0) {
	return rc;
    }

    if ((regs.eax == 1145651527) && (regs.edx == 1145392204)) {
	return 0;
    } else {
	return -1;
    }
}

static int i8k_ioctl(struct inode *ip, struct file *fp, unsigned int cmd,
		     unsigned long arg)
{
    int val;
    int speed;
    unsigned char buff[16];

    if (!arg) {
	return -EINVAL;
    }

    switch (cmd) {
    case I8K_BIOS_VERSION:
	val = i8k_get_bios_version();
	break;

    case I8K_MACHINE_ID:
	memset(buff, 0, 16);
	val = i8k_get_serial_number(buff);
	break;

    case I8K_FN_STATUS:
	val = i8k_get_fn_status();
	break;

    case I8K_POWER_STATUS:
	val = i8k_get_power_status();
	break;

    case I8K_GET_TEMP:
	val = i8k_get_cpu_temp();
	break;

    case I8K_GET_SPEED:
	if (copy_from_user(&val, (int *)arg, sizeof(int))) {
	    return -EFAULT;
	}
	val = i8k_get_fan_speed(val);
	break;

    case I8K_GET_FAN:
	if (copy_from_user(&val, (int *)arg, sizeof(int))) {
	    return -EFAULT;
	}
	val = i8k_get_fan_status(val);
	break;

    case I8K_SET_FAN:
	if (restricted && !capable(CAP_SYS_ADMIN)) {
	    return -EPERM;
	}
	if (copy_from_user(&val, (int *)arg, sizeof(int))) {
	    return -EFAULT;
	}
	if (copy_from_user(&speed, (int *)arg+1, sizeof(int))) {
	    return -EFAULT;
	}
	val = i8k_set_fan(val, speed);
	break;

    default:
	return -EINVAL;
    }

    if (val < 0) {
	return val;
    }

    switch (cmd) {
    case I8K_BIOS_VERSION:
	if (copy_to_user((int *)arg, &val, 4)) {
	    return -EFAULT;
	}
	break;
    case I8K_MACHINE_ID:
	if (copy_to_user((int *)arg, buff, 16)) {
	    return -EFAULT;
	}
	break;
    default:
	if (copy_to_user((int *)arg, &val, sizeof(int))) {
	    return -EFAULT;
	}
	break;
    }

    return 0;
}

/*
 * Print the information for /proc/i8k.
 */
static int i8k_get_info(char *buffer, char **start, off_t fpos, int length)
{
    int n, fn_key, cpu_temp, ac_power;
    int left_fan, right_fan, left_speed, right_speed;

    cpu_temp     = i8k_get_cpu_temp();			/* 11100 탎 */
    left_fan     = i8k_get_fan_status(I8K_FAN_LEFT);	/*   580 탎 */
    right_fan    = i8k_get_fan_status(I8K_FAN_RIGHT);	/*   580 탎 */
    left_speed   = i8k_get_fan_speed(I8K_FAN_LEFT);	/*   580 탎 */
    right_speed  = i8k_get_fan_speed(I8K_FAN_RIGHT);	/*   580 탎 */
    fn_key       = i8k_get_fn_status();			/*   750 탎 */
    if (power_status) {
	ac_power = i8k_get_power_status();		/* 14700 탎 */
    } else {
	ac_power = -1;
    }

    /*
     * Info:
     *
     * 1)  Format version (this will change if format changes)
     * 2)  BIOS version
     * 3)  BIOS machine ID
     * 4)  Cpu temperature
     * 5)  Left fan status
     * 6)  Right fan status
     * 7)  Left fan speed
     * 8)  Right fan speed
     * 9)  AC power
     * 10) Fn Key status
     */
    n = sprintf(buffer, "%s %s %s %d %d %d %d %d %d %d\n",
		I8K_PROC_FMT,
		bios_version,
		serial_number,
		cpu_temp,
		left_fan,
		right_fan,
		left_speed,
		right_speed,
		ac_power,
		fn_key);

    return n;
}

static ssize_t i8k_read(struct file *f, char *buffer, size_t len, loff_t *fpos)
{
    int n;
    char info[128];

    n = i8k_get_info(info, NULL, 0, 128);
    if (n <= 0) {
	return n;
    }

    if (*fpos >= n) {
	return 0;
    }

    if ((*fpos + len) >= n) {
	len = n - *fpos;
    }

    if (copy_to_user(buffer, info, len) != 0) {
	return -EFAULT;
    }

    *fpos += len;
    return len;
}

/*
 * i8k_keys stuff. Thanks to David Bustos <bustos@caltech.edu>
 */

static unsigned char i8k_keys_make_scancode(int x) {
    switch (x) {
    case I8K_FN_UP:	return I8K_KEYS_UP_SCANCODE;
    case I8K_FN_DOWN:	return I8K_KEYS_DOWN_SCANCODE;
    case I8K_FN_MUTE:	return I8K_KEYS_MUTE_SCANCODE;
    }

    return 0;
}

static void i8k_keys_poll(unsigned long data) {
    static int last = 0;
    static int repeat = 0;

    int  curr;

    curr = i8k_get_fn_status();
    if (curr >= 0) {
	if (curr != last) {
	    repeat = jiffies + (HZ * repeat_delay / 1000);

	    if (last != 0) {
		handle_scancode(0xe0, 0);
		handle_scancode(i8k_keys_make_scancode(last), 0);
	    }

	    if (curr != 0) {
		handle_scancode(0xe0, 1);
		handle_scancode(i8k_keys_make_scancode(curr), 1);
	    }
	} else {
	    /* Generate keyboard repeat events with current scancode -- dz */
	    if ((curr) && (repeat_rate > 0) && (jiffies >= repeat)) {
		repeat = jiffies + (HZ / repeat_rate);
		handle_scancode(0xe0, 1);
		handle_scancode(i8k_keys_make_scancode(curr), 1);
	    }
	}

	last = curr;
    }

    /* Reset the timer. */
    i8k_keys_set_timer();
}

static void i8k_keys_set_timer() {
    i8k_keys_timer.expires = jiffies + I8K_POLL_INTERVAL;
    add_timer(&i8k_keys_timer);
}

static char* __init string_trim(char *s, int size)
{
    int len;
    char *p;

    if ((len = strlen(s)) > size) {
	len = size;
    }

    for (p=s+len-1; len && (*p==' '); len--,p--) {
	*p = '\0';
    }

    return s;
}

/* DMI code, stolen from arch/i386/kernel/dmi_scan.c */

/*
 * |<-- dmi->length -->|
 * |                   |
 * |dmi header    s=N  | string1,\0, ..., stringN,\0, ..., \0
 *                |                       |
 *                +-----------------------+
 */
static char* __init dmi_string(DMIHeader *dmi, u8 s)
{
    u8 *p;

    if (!s) {
	return "";
    }
    s--;

    p = (u8 *)dmi + dmi->length;
    while (s > 0) {
	p += strlen(p);
	p++;
	s--;
    }

    return p;
}

static void __init dmi_decode(DMIHeader *dmi)
{
    u8 *data = (u8 *) dmi;
    char *p;

#ifdef I8K_DEBUG
    int i;
    printk("%08x ", (int)data);
    for (i=0; i<data[1] && i<64; i++) {
	printk("%02x ", data[i]);
    }
    printk("\n");
#endif

    switch (dmi->type) {
    case  0:	/* BIOS Information */
	p = dmi_string(dmi,data[5]);
	if (*p) {
	    strncpy(bios_version, p, sizeof(bios_version));
	    string_trim(bios_version, sizeof(bios_version));
	}
	break;	
    case 1:	/* System Information */
	p = dmi_string(dmi,data[4]);
	if (*p) {
	    strncpy(system_vendor, p, sizeof(system_vendor));
	    string_trim(system_vendor, sizeof(system_vendor));
	}
	p = dmi_string(dmi,data[5]);
	if (*p) {
	    strncpy(product_name, p, sizeof(product_name));
	    string_trim(product_name, sizeof(product_name));
	}
	p = dmi_string(dmi,data[7]);
	if (*p) {
	    strncpy(serial_number, p, sizeof(serial_number));
	    string_trim(serial_number, sizeof(serial_number));
	}
	break;
    }
}

static int __init dmi_table(u32 base, int len, int num, void (*fn)(DMIHeader*))
{
    u8 *buf;
    u8 *data;
    DMIHeader *dmi;
    int i = 1;

    buf = ioremap(base, len);
    if (buf == NULL) {
	return -1;
    }
    data = buf;

    /*
     * Stop when we see al the items the table claimed to have
     * or we run off the end of the table (also happens)
     */
    while ((i<num) && ((data-buf) < len)) {
	dmi = (DMIHeader *)data;
	/*
	 * Avoid misparsing crud if the length of the last
	 * record is crap
	 */
	if ((data-buf+dmi->length) >= len) {
	    break;
	}
	fn(dmi);
	data += dmi->length;
	/*
	 * Don't go off the end of the data if there is
	 * stuff looking like string fill past the end
	 */
	while (((data-buf) < len) && (*data || data[1])) {
	    data++;
	}
	data += 2;
	i++;
    }
    iounmap(buf);

    return 0;
}

static int __init dmi_iterate(void (*decode)(DMIHeader *))
{
    unsigned char buf[20];
    long fp = 0x000e0000L;
    fp -= 16;

    while (fp < 0x000fffffL) {
	fp += 16;
	isa_memcpy_fromio(buf, fp, 20);
	if (memcmp(buf, "_DMI_", 5)==0) {
	    u16 num  = buf[13]<<8  | buf[12];
	    u16 len  = buf [7]<<8  | buf [6];
	    u32 base = buf[11]<<24 | buf[10]<<16 | buf[9]<<8 | buf[8];
#ifdef I8K_DEBUG
	    printk(KERN_INFO "DMI %d.%d present.\n",
		   buf[14]>>4, buf[14]&0x0F);
	    printk(KERN_INFO "%d structures occupying %d bytes.\n",
		   buf[13]<<8 | buf[12],
		   buf [7]<<8 | buf[6]);
	    printk(KERN_INFO "DMI table at 0x%08X.\n",
		   buf[11]<<24 | buf[10]<<16 | buf[9]<<8 | buf[8]);
#endif
	    if (dmi_table(base, len, num, decode)==0) {
		return 0;
	    }
	}
    }
    return -1;
}
/* end of DMI code */

/*
 * Get DMI information.
 */
static int __init i8k_dmi_probe(void)
{
    char **p;

    if (dmi_iterate(dmi_decode) != 0) {
	printk(KERN_INFO "i8k: unable to get DMI information\n");
	return -ENODEV;
    }

    if (strncmp(system_vendor,DELL_SIGNATURE,strlen(DELL_SIGNATURE)) != 0) {
	printk(KERN_INFO "i8k: not running on a Dell system\n");
	return -ENODEV;
    }

    for (p=supported_models; ; p++) {
	if (!*p) {
	    printk(KERN_INFO "i8k: unsupported model: %s\n", product_name);
	    return -ENODEV;
	}
	if (strncmp(product_name,*p,strlen(*p)) == 0) {
	    break;
	}
    }

    return 0;
}

/*
 * Probe for the presence of a supported laptop.
 */
static int __init i8k_probe(void)
{
    char buff[4];
    int version;
    int smm_found = 0;

    /*
     * Get DMI information
     */
    if (i8k_dmi_probe() != 0) {
	printk(KERN_INFO "i8k: vendor=%s, model=%s, version=%s\n",
	       system_vendor, product_name, bios_version);
    }

    /*
     * Get SMM Dell signature
     */
    if (i8k_get_dell_signature() != 0) {
	printk(KERN_INFO "i8k: unable to get SMM Dell signature\n");
    } else {
	smm_found = 1;
    }

    /*
     * Get SMM BIOS version.
     */
    version = i8k_get_bios_version();
    if (version <= 0) {
	printk(KERN_INFO "i8k: unable to get SMM BIOS version\n");
    } else {
	smm_found = 1;
	buff[0] = (version >> 16) & 0xff;
	buff[1] = (version >>  8) & 0xff;
	buff[2] = (version)       & 0xff;
	buff[3] = '\0';
	/*
	 * If DMI BIOS version is unknown use SMM BIOS version.
	 */
	if (bios_version[0] == '?') {
	    strcpy(bios_version, buff);
	}
	/*
	 * Check if the two versions match.
	 */
	if (strncmp(buff,bios_version,sizeof(bios_version)) != 0) {
	    printk(KERN_INFO "i8k: BIOS version mismatch: %s != %s\n",
		   buff, bios_version);
	}
    }

    if (!smm_found && !force) {
	return -ENODEV;
    }

    return 0;
}

#ifdef MODULE
static
#endif
int __init i8k_init(void)
{
    struct proc_dir_entry *proc_i8k;

    /* Are we running on an supported laptop? */
    if (i8k_probe() != 0) {
	return -ENODEV;
    }

    /* Register the proc entry */
    proc_i8k = create_proc_info_entry("i8k", 0, NULL, i8k_get_info);
    if (!proc_i8k) {
	return -ENOENT;
    }
    proc_i8k->proc_fops = &i8k_fops;
    SET_MODULE_OWNER(proc_i8k);

    printk(KERN_INFO
	   "Dell laptop SMM driver v%s Massimo Dal Zotto (dz@debian.org)\n",
	   I8K_VERSION);

    /* Register the i8k_keys timer. */
    if (handle_buttons) {
	printk(KERN_INFO
	       "i8k: enabling buttons events, delay=%d, rate=%d\n",
	       repeat_delay, repeat_rate);
	init_timer(&i8k_keys_timer);
	i8k_keys_timer.function = i8k_keys_poll;
	i8k_keys_set_timer();
    }

    return 0;
}

#ifdef MODULE
int init_module(void)
{
    return i8k_init();
}

void cleanup_module(void)
{
    /* Remove the proc entry */
    remove_proc_entry("i8k", NULL);

    /* Unregister the i8k_keys timer. */
    while (handle_buttons && !del_timer(&i8k_keys_timer)) {
	schedule_timeout(I8K_POLL_INTERVAL);
    }

    printk(KERN_INFO "i8k: module unloaded\n");
}
#endif

/* end of file */
