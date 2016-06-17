/*
 * File:	mca.c
 * Purpose:	SN specific MCA code.
 *
 * Copyright (C) 2001-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <asm/machvec.h>
#include <asm/mca.h>
#include <asm/sal.h>
#include <asm/sn/sn_sal.h>



/*
 * Interval for calling SAL to poll for errors that do NOT cause error
 * interrupts. SAL will raise a CPEI if any errors are present that
 * need to be logged.
 */
#define CPEI_INTERVAL	(5*HZ)


struct timer_list sn_cpei_timer;
void sn_init_cpei_timer(void);

/* Printing oemdata from mca uses data that is not passed through SAL, it is
 * global.  Only one user at a time.
 */
static DECLARE_MUTEX(sn_oemdata_mutex);
static u8 **sn_oemdata;
static u64 *sn_oemdata_size, sn_oemdata_bufsize;

/* Called as the "printk" routine via sn_platform_plat_specific_err_print,
 * ia64_sn_plat_specific_err_print through SAL to print_hook.  It only handles
 * a print of '"%s", buf'.  buf is appended to sn_oemdata, resizing as required.
 */
static int
sn_oemdata_print(const char *fmt, ...)
{
	va_list args;
	char *buf;
	int len;
	va_start(args, fmt);
	buf = va_arg(args, char *);
	va_end(args);
	len = strlen(buf);
	while (*sn_oemdata_size + len + 1 > sn_oemdata_bufsize) {
		u8 *newbuf = vmalloc(sn_oemdata_bufsize += 1000);
		if (!newbuf) {
			printk(KERN_ERR "%s: unable to extend sn_oemdata\n", __FUNCTION__);
			return 0;
		}
		memcpy(newbuf, *sn_oemdata, *sn_oemdata_size);
		vfree(*sn_oemdata);
		*sn_oemdata = newbuf;
	}
	memcpy(*sn_oemdata + *sn_oemdata_size, buf, len + 1);
	*sn_oemdata_size += len;
	return len;
}

/*
 * print_hook
 *
 * This function is the callback routine that SAL calls to log error
 * info for platform errors. 
 */
static int
print_hook(const char *fmt, ...)
{
	static int	newline=1;
	char		buf[400], *p;
	va_list		args;
	int		len=0;


	va_start(args, fmt);
	if (newline) {
		strcpy(buf, "+ ");
		len += 2;
	}
	len += vsnprintf(buf+len, sizeof(buf)-len, fmt, args);

	/* Prefix each line with "+ " to be consistent with mca.c. */
	p = buf;
	while ((p=strchr(p, '\n')) && *++p != '\0') {
		memmove(p+2, p, 1+strlen(p));
		strncpy(p, "+ ", 2);
		len += 2;
	}
	newline = (p != 0);

	va_end(args);
	sn_oemdata_print("%s", buf);	/* args must be '"%s", buf' */
	return len;
}


static void
sn_cpei_handler(int irq, void *devid, struct pt_regs *regs)
{
	/*
	 * this function's sole purpose is to call SAL when we receive
	 * a CE interrupt from SHUB or when the timer routine decides
	 * we need to call SAL to check for CEs.
	 */

	/* CALL SAL_LOG_CE */

	ia64_sn_plat_cpei_handler();
}


static void
sn_cpei_timer_handler(unsigned long dummy) {
	sn_cpei_handler(-1, NULL, NULL);
	mod_timer(&sn_cpei_timer, jiffies + CPEI_INTERVAL);
}

void
sn_init_cpei_timer() {
	sn_cpei_timer.expires = jiffies + CPEI_INTERVAL;
        sn_cpei_timer.function = sn_cpei_timer_handler;
        add_timer(&sn_cpei_timer);
}

static int
sn_platform_plat_specific_err_print(const u8 *sect_header, u8 **oemdata, u64 *oemdata_size)
{
	sal_log_plat_specific_err_info_t *psei = (sal_log_plat_specific_err_info_t *)sect_header;
	if (!psei->valid.oem_data)
		return 0;
	down(&sn_oemdata_mutex);
	sn_oemdata = oemdata;
	sn_oemdata_size = oemdata_size;
	ia64_sn_plat_specific_err_print(print_hook, (char *)psei);
	up(&sn_oemdata_mutex);
	return 0;
}

/* Callback when userspace salinfo wants to decode oem data via the platform
 * kernel and/or prom.
 */
int sn_salinfo_platform_oemdata(const u8 *sect_header, u8 **oemdata, u64 *oemdata_size)
{
	efi_guid_t guid = *(efi_guid_t *)sect_header;
	*oemdata_size = 0;
	sn_oemdata_bufsize = 0;
	vfree(*oemdata);
	*oemdata = NULL;
	if (efi_guidcmp(guid, SAL_PLAT_SPECIFIC_ERR_SECT_GUID) == 0)
		return sn_platform_plat_specific_err_print(sect_header, oemdata, oemdata_size);
	return 0;
}

static int __init sn_salinfo_init(void)
{
	salinfo_platform_oemdata = &sn_salinfo_platform_oemdata;
        return 0;
}

module_init(sn_salinfo_init)


