
/*
 * Author: Martin Peschke <mpeschke@de.ibm.com>
 * Copyright (C) 2001 IBM Entwicklung GmbH, IBM Corporation
 */

#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <asm/semaphore.h>
#include <asm/ebcdic.h>
#include "hwc_rw.h"
#include "hwc.h"

#define CPI_RETRIES		3
#define CPI_SLEEP_TICKS		50

#define CPI_LENGTH_SYSTEM_TYPE	8
#define CPI_LENGTH_SYSTEM_NAME	8
#define CPI_LENGTH_SYSPLEX_NAME	8

typedef struct {
	_EBUF_HEADER
	u8 id_format;
	u8 reserved0;
	u8 system_type[CPI_LENGTH_SYSTEM_TYPE];
	u64 reserved1;
	u8 system_name[CPI_LENGTH_SYSTEM_NAME];
	u64 reserved2;
	u64 system_level;
	u64 reserved3;
	u8 sysplex_name[CPI_LENGTH_SYSPLEX_NAME];
	u8 reserved4[16];
} __attribute__ ((packed)) 

cpi_evbuf_t;

typedef struct _cpi_hwcb_t {
	_HWCB_HEADER
	cpi_evbuf_t cpi_evbuf;
} __attribute__ ((packed)) 

cpi_hwcb_t;

cpi_hwcb_t *cpi_hwcb;

static int __init cpi_module_init (void);
static void __exit cpi_module_exit (void);

module_init (cpi_module_init);
module_exit (cpi_module_exit);

MODULE_AUTHOR (
		      "Martin Peschke, IBM Deutschland Entwicklung GmbH "
		      "<mpeschke@de.ibm.com>");

MODULE_DESCRIPTION (
  "identify this operating system instance to the S/390 or zSeries hardware");

static char *system_name = NULL;
MODULE_PARM (system_name, "s");
MODULE_PARM_DESC (system_name, "e.g. hostname - max. 8 characters");

static char *sysplex_name = NULL;
#ifdef ALLOW_SYSPLEX_NAME
MODULE_PARM (sysplex_name, "s");
MODULE_PARM_DESC (sysplex_name, "if applicable - max. 8 characters");
#endif

static char *system_type = "LINUX";

hwc_request_t cpi_request =
{};

hwc_callback_t cpi_callback;

static DECLARE_MUTEX_LOCKED (sem);

static int __init 
cpi_module_init (void)
{
	int retval;
	int system_type_length;
	int system_name_length;
	int sysplex_name_length = 0;
	int retries;

	if (!MACHINE_HAS_HWC) {
		printk ("cpi: bug: hardware console not present\n");
		retval = -EINVAL;
		goto out;
	}
	if (!system_type) {
		printk ("cpi: bug: no system type specified\n");
		retval = -EINVAL;
		goto out;
	}
	system_type_length = strlen (system_type);
	if (system_type_length > CPI_LENGTH_SYSTEM_NAME) {
		printk ("cpi: bug: system type has length of %i characters - "
			"only %i characters supported\n",
			system_type_length,
			CPI_LENGTH_SYSTEM_TYPE);
		retval = -EINVAL;
		goto out;
	}
	if (!system_name) {
		printk ("cpi: no system name specified\n");
		retval = -EINVAL;
		goto out;
	}
	system_name_length = strlen (system_name);
	if (system_name_length > CPI_LENGTH_SYSTEM_NAME) {
		printk ("cpi: system name has length of %i characters - "
			"only %i characters supported\n",
			system_name_length,
			CPI_LENGTH_SYSTEM_NAME);
		retval = -EINVAL;
		goto out;
	}
	if (sysplex_name) {
		sysplex_name_length = strlen (sysplex_name);
		if (sysplex_name_length > CPI_LENGTH_SYSPLEX_NAME) {
			printk ("cpi: sysplex name has length of %i characters - "
				"only %i characters supported\n",
				sysplex_name_length,
				CPI_LENGTH_SYSPLEX_NAME);
			retval = -EINVAL;
			goto out;
		}
	}
	cpi_hwcb = kmalloc (sizeof (cpi_hwcb_t), GFP_KERNEL);
	if (!cpi_hwcb) {
		printk ("cpi: no storage to fulfill request\n");
		retval = -ENOMEM;
		goto out;
	}
	memset (cpi_hwcb, 0, sizeof (cpi_hwcb_t));

	cpi_hwcb->length = sizeof (cpi_hwcb_t);
	cpi_hwcb->cpi_evbuf.length = sizeof (cpi_evbuf_t);
	cpi_hwcb->cpi_evbuf.type = 0x0B;

	memset (cpi_hwcb->cpi_evbuf.system_type, ' ', CPI_LENGTH_SYSTEM_TYPE);
	memcpy (cpi_hwcb->cpi_evbuf.system_type, system_type, system_type_length);
	HWC_ASCEBC_STR (cpi_hwcb->cpi_evbuf.system_type, CPI_LENGTH_SYSTEM_TYPE);
	EBC_TOUPPER (cpi_hwcb->cpi_evbuf.system_type, CPI_LENGTH_SYSTEM_TYPE);

	memset (cpi_hwcb->cpi_evbuf.system_name, ' ', CPI_LENGTH_SYSTEM_NAME);
	memcpy (cpi_hwcb->cpi_evbuf.system_name, system_name, system_name_length);
	HWC_ASCEBC_STR (cpi_hwcb->cpi_evbuf.system_name, CPI_LENGTH_SYSTEM_NAME);
	EBC_TOUPPER (cpi_hwcb->cpi_evbuf.system_name, CPI_LENGTH_SYSTEM_NAME);

	cpi_hwcb->cpi_evbuf.system_level = LINUX_VERSION_CODE;

	if (sysplex_name) {
		memset (cpi_hwcb->cpi_evbuf.sysplex_name, ' ', CPI_LENGTH_SYSPLEX_NAME);
		memcpy (cpi_hwcb->cpi_evbuf.sysplex_name, sysplex_name, sysplex_name_length);
		HWC_ASCEBC_STR (cpi_hwcb->cpi_evbuf.sysplex_name, CPI_LENGTH_SYSPLEX_NAME);
		EBC_TOUPPER (cpi_hwcb->cpi_evbuf.sysplex_name, CPI_LENGTH_SYSPLEX_NAME);
	}
	cpi_request.block = cpi_hwcb;
	cpi_request.word = HWC_CMDW_WRITEDATA;
	cpi_request.callback = cpi_callback;

	for (retries = CPI_RETRIES; retries; retries--) {
		retval = hwc_send (&cpi_request);
		if (retval) {

			set_current_state (TASK_INTERRUPTIBLE);
			schedule_timeout (CPI_SLEEP_TICKS);
		} else {

			down (&sem);

			switch (cpi_hwcb->response_code) {
			case 0x0020:
				printk ("cpi: succeeded\n");
				break;
			default:
				printk ("cpi: failed with response code 0x%x\n",
					cpi_hwcb->response_code);
			}
			goto free;
		}
	}

	printk ("cpi: failed (%i)\n", retval);

      free:
	kfree (cpi_hwcb);

      out:
	return retval;
}

static void __exit 
cpi_module_exit (void)
{
	printk ("cpi: exit\n");
}

void 
cpi_callback (hwc_request_t * req)
{
	up (&sem);
}
