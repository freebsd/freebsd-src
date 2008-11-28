/*
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2008 Atheros Communications, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: ah_osdep.c,v 1.3 2008/11/10 04:08:05 sam Exp $
 */
#include "opt_ah.h"

#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/sysctl.h>
#include <linux/proc_fs.h>

#include <asm/io.h>

#include "ah.h"

#ifndef __MOD_INC_USE_COUNT
#define	AH_MOD_INC_USE_COUNT(_m)					\
	if (!try_module_get(_m)) {					\
		printk(KERN_WARNING "try_module_get failed\n");		\
		return NULL;						\
	}
#define	AH_MOD_DEC_USE_COUNT(_m)	module_put(_m)
#else
#define	AH_MOD_INC_USE_COUNT(_m)	MOD_INC_USE_COUNT
#define	AH_MOD_DEC_USE_COUNT(_m)	MOD_DEC_USE_COUNT
#endif

#ifdef AH_DEBUG
static	int ath_hal_debug = 0;
#endif

int	ath_hal_dma_beacon_response_time = 2;	/* in TU's */
int	ath_hal_sw_beacon_response_time = 10;	/* in TU's */
int	ath_hal_additional_swba_backoff = 0;	/* in TU's */

struct ath_hal *
_ath_hal_attach(uint16_t devid, HAL_SOFTC sc,
		HAL_BUS_TAG t, HAL_BUS_HANDLE h, void* s)
{
	HAL_STATUS status;
	struct ath_hal *ah = ath_hal_attach(devid, sc, t, h, &status);

	*(HAL_STATUS *)s = status;
	if (ah)
		AH_MOD_INC_USE_COUNT(THIS_MODULE);
	return ah;
}

void
ath_hal_detach(struct ath_hal *ah)
{
	(*ah->ah_detach)(ah);
	AH_MOD_DEC_USE_COUNT(THIS_MODULE);
}

/*
 * Print/log message support.
 */

void __ahdecl
ath_hal_vprintf(struct ath_hal *ah, const char* fmt, va_list ap)
{
	char buf[1024];					/* XXX */
	vsnprintf(buf, sizeof(buf), fmt, ap);
	printk("%s", buf);
}

void __ahdecl
ath_hal_printf(struct ath_hal *ah, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ath_hal_vprintf(ah, fmt, ap);
	va_end(ap);
}
EXPORT_SYMBOL(ath_hal_printf);

/*
 * Format an Ethernet MAC for printing.
 */
const char* __ahdecl
ath_hal_ether_sprintf(const uint8_t *mac)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return etherbuf;
}

#ifdef AH_ASSERT
void __ahdecl
ath_hal_assert_failed(const char* filename, int lineno, const char *msg)
{
	printk("Atheros HAL assertion failure: %s: line %u: %s\n",
		filename, lineno, msg);
	panic("ath_hal_assert");
}
#endif /* AH_ASSERT */

#ifdef AH_DEBUG_ALQ
/*
 * ALQ register tracing support.
 *
 * Setting hw.ath.hal.alq=1 enables tracing of all register reads and
 * writes to the file /tmp/ath_hal.log.  The file format is a simple
 * fixed-size array of records.  When done logging set hw.ath.hal.alq=0
 * and then decode the file with the ardecode program (that is part of the
 * HAL).  If you start+stop tracing the data will be appended to an
 * existing file.
 *
 * NB: doesn't handle multiple devices properly; only one DEVICE record
 *     is emitted and the different devices are not identified.
 */
#include "alq/alq.h"
#include "ah_decode.h"

static	struct alq *ath_hal_alq;
static	int ath_hal_alq_emitdev;	/* need to emit DEVICE record */
static	u_int ath_hal_alq_lost;		/* count of lost records */
static	const char *ath_hal_logfile = "/tmp/ath_hal.log";
static	u_int ath_hal_alq_qsize = 8*1024;

static int
ath_hal_setlogging(int enable)
{
	int error;

	if (enable) {
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		error = alq_open(&ath_hal_alq, ath_hal_logfile,
				sizeof (struct athregrec), ath_hal_alq_qsize);
		ath_hal_alq_lost = 0;
		ath_hal_alq_emitdev = 1;
		printk("ath_hal: logging to %s %s\n", ath_hal_logfile,
			error == 0 ? "enabled" : "could not be setup");
	} else {
		if (ath_hal_alq)
			alq_close(ath_hal_alq);
		ath_hal_alq = NULL;
		printk("ath_hal: logging disabled\n");
		error = 0;
	}
	return error;
}

/*
 * Deal with the sysctl handler api changing.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8)
#define	AH_SYSCTL_ARGS_DECL \
	ctl_table *ctl, int write, struct file *filp, void *buffer, \
		size_t *lenp
#define	AH_SYSCTL_ARGS		ctl, write, filp, buffer, lenp
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8) */
#define	AH_SYSCTL_ARGS_DECL \
	ctl_table *ctl, int write, struct file *filp, void *buffer,\
		size_t *lenp, loff_t *ppos
#define	AH_SYSCTL_ARGS		ctl, write, filp, buffer, lenp, ppos
#endif

static int
sysctl_hw_ath_hal_log(AH_SYSCTL_ARGS_DECL)
{
	int error, enable;

	ctl->data = &enable;
	ctl->maxlen = sizeof(enable);
	enable = (ath_hal_alq != NULL);
        error = proc_dointvec(AH_SYSCTL_ARGS);
        if (error || !write)
                return error;
	else
		return ath_hal_setlogging(enable);
}

static struct ale *
ath_hal_alq_get(struct ath_hal *ah)
{
	struct ale *ale;

	if (ath_hal_alq_emitdev) {
		ale = alq_get(ath_hal_alq, ALQ_NOWAIT);
		if (ale) {
			struct athregrec *r =
				(struct athregrec *) ale->ae_data;
			r->op = OP_DEVICE;
			r->reg = 0;
			r->val = ah->ah_devid;
			alq_post(ath_hal_alq, ale);
			ath_hal_alq_emitdev = 0;
		} else
			ath_hal_alq_lost++;
	}
	ale = alq_get(ath_hal_alq, ALQ_NOWAIT);
	if (!ale)
		ath_hal_alq_lost++;
	return ale;
}

void __ahdecl
ath_hal_reg_write(struct ath_hal *ah, uint32_t reg, uint32_t val)
{
	if (ath_hal_alq) {
		unsigned long flags;
		struct ale *ale;

		local_irq_save(flags);
		ale = ath_hal_alq_get(ah);
		if (ale) {
			struct athregrec *r = (struct athregrec *) ale->ae_data;
			r->op = OP_WRITE;
			r->reg = reg;
			r->val = val;
			alq_post(ath_hal_alq, ale);
		}
		local_irq_restore(flags);
	}
	_OS_REG_WRITE(ah, reg, val);
}
EXPORT_SYMBOL(ath_hal_reg_write);

uint32_t __ahdecl
ath_hal_reg_read(struct ath_hal *ah, uint32_t reg)
{
	uint32_t val;

	val = _OS_REG_READ(ah, reg);
	if (ath_hal_alq) {
		unsigned long flags;
		struct ale *ale;

		local_irq_save(flags);
		ale = ath_hal_alq_get(ah);
		if (ale) {
			struct athregrec *r = (struct athregrec *) ale->ae_data;
			r->op = OP_READ;
			r->reg = reg;
			r->val = val;
			alq_post(ath_hal_alq, ale);
		}
		local_irq_restore(flags);
	}
	return val;
}
EXPORT_SYMBOL(ath_hal_reg_read);

void __ahdecl
OS_MARK(struct ath_hal *ah, u_int id, uint32_t v)
{
	if (ath_hal_alq) {
		unsigned long flags;
		struct ale *ale;

		local_irq_save(flags);
		ale = ath_hal_alq_get(ah);
		if (ale) {
			struct athregrec *r = (struct athregrec *) ale->ae_data;
			r->op = OP_MARK;
			r->reg = id;
			r->val = v;
			alq_post(ath_hal_alq, ale);
		}
		local_irq_restore(flags);
	}
}
EXPORT_SYMBOL(OS_MARK);
#elif defined(AH_DEBUG) || defined(AH_REGOPS_FUNC)
/*
 * Memory-mapped device register read/write.  These are here
 * as routines when debugging support is enabled and/or when
 * explicitly configured to use function calls.  The latter is
 * for architectures that might need to do something before
 * referencing memory (e.g. remap an i/o window).
 *
 * NB: see the comments in ah_osdep.h about byte-swapping register
 *     reads and writes to understand what's going on below.
 */
void __ahdecl
ath_hal_reg_write(struct ath_hal *ah, u_int reg, uint32_t val)
{
#ifdef AH_DEBUG
	if (ath_hal_debug > 1)
		ath_hal_printf(ah, "WRITE 0x%x <= 0x%x\n", reg, val);
#endif
	_OS_REG_WRITE(ah, reg, val);
}
EXPORT_SYMBOL(ath_hal_reg_write);

uint32_t __ahdecl
ath_hal_reg_read(struct ath_hal *ah, u_int reg)
{
 	uint32_t val;

	val = _OS_REG_READ(ah, reg);
#ifdef AH_DEBUG
	if (ath_hal_debug > 1)
		ath_hal_printf(ah, "READ 0x%x => 0x%x\n", reg, val);
#endif
	return val;
}
EXPORT_SYMBOL(ath_hal_reg_read);
#endif /* AH_DEBUG || AH_REGOPS_FUNC */

#ifdef AH_DEBUG
void __ahdecl
HALDEBUG(struct ath_hal *ah, const char* fmt, ...)
{
	if (ath_hal_debug) {
		__va_list ap;
		va_start(ap, fmt);
		ath_hal_vprintf(ah, fmt, ap);
		va_end(ap);
	}
}


void __ahdecl
HALDEBUGn(struct ath_hal *ah, u_int level, const char* fmt, ...)
{
	if (ath_hal_debug >= level) {
		__va_list ap;
		va_start(ap, fmt);
		ath_hal_vprintf(ah, fmt, ap);
		va_end(ap);
	}
}
#endif /* AH_DEBUG */

/*
 * Delay n microseconds.
 */
void __ahdecl
ath_hal_delay(int n)
{
	udelay(n);
}

uint32_t __ahdecl
ath_hal_getuptime(struct ath_hal *ah)
{
	return ((jiffies / HZ) * 1000) + (jiffies % HZ) * (1000 / HZ);
}
EXPORT_SYMBOL(ath_hal_getuptime);

/*
 * Allocate/free memory.
 */

void * __ahdecl
ath_hal_malloc(size_t size)
{
	void *p;
	p = kmalloc(size, GFP_KERNEL);
	if (p)
		OS_MEMZERO(p, size);
	return p;
		
}

void __ahdecl
ath_hal_free(void* p)
{
	kfree(p);
}

void __ahdecl
ath_hal_memzero(void *dst, size_t n)
{
	memset(dst, 0, n);
}
EXPORT_SYMBOL(ath_hal_memzero);

void * __ahdecl
ath_hal_memcpy(void *dst, const void *src, size_t n)
{
	return memcpy(dst, src, n);
}
EXPORT_SYMBOL(ath_hal_memcpy);

int __ahdecl
ath_hal_memcmp(const void *a, const void *b, size_t n)
{
	return memcmp(a, b, n);
}
EXPORT_SYMBOL(ath_hal_memcmp);

#ifdef CONFIG_SYSCTL
enum {
	DEV_ATH		= 9,			/* XXX must match driver */
};

#define	CTL_AUTO	-2	/* cannot be CTL_ANY or CTL_NONE */

static ctl_table ath_hal_sysctls[] = {
#ifdef AH_DEBUG
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "debug",
	  .mode		= 0644,
	  .data		= &ath_hal_debug,
	  .maxlen	= sizeof(ath_hal_debug),
	  .proc_handler	= proc_dointvec
	},
#endif
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "dma_beacon_response_time",
	  .data		= &ath_hal_dma_beacon_response_time,
	  .maxlen	= sizeof(ath_hal_dma_beacon_response_time),
	  .mode		= 0644,
	  .proc_handler	= proc_dointvec
	},
	{ .ctl_name	= CTL_AUTO,	
	  .procname	= "sw_beacon_response_time",
	  .mode		= 0644,
	  .data		= &ath_hal_sw_beacon_response_time,
	  .maxlen	= sizeof(ath_hal_sw_beacon_response_time),
	  .proc_handler	= proc_dointvec
	},
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "swba_backoff",
	  .mode		= 0644,
	  .data		= &ath_hal_additional_swba_backoff,
	  .maxlen	= sizeof(ath_hal_additional_swba_backoff),
	  .proc_handler	= proc_dointvec
	},
#ifdef AH_DEBUG_ALQ
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "alq",
	  .mode		= 0644,
	  .proc_handler	= sysctl_hw_ath_hal_log
	},
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "alq_size",
	  .mode		= 0644,
	  .data		= &ath_hal_alq_qsize,
	  .maxlen	= sizeof(ath_hal_alq_qsize),
	  .proc_handler	= proc_dointvec
	},
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "alq_lost",
	  .mode		= 0644,
	  .data		= &ath_hal_alq_lost,
	  .maxlen	= sizeof(ath_hal_alq_lost),
	  .proc_handler	= proc_dointvec
	},
#endif
	{ 0 }
};
static ctl_table ath_hal_table[] = {
	{ .ctl_name	= CTL_AUTO,
	  .procname	= "hal",
	  .mode		= 0555,
	  .child	= ath_hal_sysctls
	}, { 0 }
};
static ctl_table ath_ath_table[] = {
	{ .ctl_name	= DEV_ATH,
	  .procname	= "ath",
	  .mode		= 0555,
	  .child	= ath_hal_table
	}, { 0 }
};
static ctl_table ath_root_table[] = {
	{ .ctl_name	= CTL_DEV,
	  .procname	= "dev",
	  .mode		= 0555,
	  .child	= ath_ath_table
	}, { 0 }
};
static struct ctl_table_header *ath_hal_sysctl_header;

void
ath_hal_sysctl_register(void)
{
	static int initialized = 0;

	if (!initialized) {
		ath_hal_sysctl_header =
			register_sysctl_table(ath_root_table, 1);
		initialized = 1;
	}
}

void
ath_hal_sysctl_unregister(void)
{
	if (ath_hal_sysctl_header)
		unregister_sysctl_table(ath_hal_sysctl_header);
}
#endif /* CONFIG_SYSCTL */

/*
 * Module glue.
 */
static char *dev_info = "ath_hal";

MODULE_AUTHOR("Errno Consulting, Sam Leffler");
MODULE_DESCRIPTION("Atheros Hardware Access Layer (HAL)");
MODULE_SUPPORTED_DEVICE("Atheros WLAN devices");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Proprietary");
#endif

EXPORT_SYMBOL(ath_hal_probe);
EXPORT_SYMBOL(_ath_hal_attach);
EXPORT_SYMBOL(ath_hal_detach);
EXPORT_SYMBOL(ath_hal_init_channels);
EXPORT_SYMBOL(ath_hal_getwirelessmodes);
EXPORT_SYMBOL(ath_hal_computetxtime);
EXPORT_SYMBOL(ath_hal_mhz2ieee);
EXPORT_SYMBOL(ath_hal_process_noisefloor);

static int __init
init_ath_hal(void)
{
	const char *sep;
	int i;

	printk(KERN_INFO "%s: %s (", dev_info, ath_hal_version);
	sep = "";
	for (i = 0; ath_hal_buildopts[i] != NULL; i++) {
		printk("%s%s", sep, ath_hal_buildopts[i]);
		sep = ", ";
	}
	printk(")\n");
#ifdef CONFIG_SYSCTL
	ath_hal_sysctl_register();
#endif
	return (0);
}
module_init(init_ath_hal);

static void __exit
exit_ath_hal(void)
{
#ifdef CONFIG_SYSCTL
	ath_hal_sysctl_unregister();
#endif
	printk(KERN_INFO "%s: driver unloaded\n", dev_info);
}
module_exit(exit_ath_hal);
