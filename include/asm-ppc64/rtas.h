#ifndef _PPC64_RTAS_H
#define _PPC64_RTAS_H

#include <linux/spinlock.h>
#include <asm/page.h>

/*
 * Definitions for talking to the RTAS on CHRP machines.
 *
 * Copyright (C) 2001 Peter Bergner
 * Copyright (C) 2001 PPC 64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define RTAS_UNKNOWN_SERVICE (-1)
#define RTAS_INSTANTIATE_MAX (1UL<<30) /* Don't instantiate rtas at/above this value */

/* Error inject defines */
#define ERRINJCT_TOKEN_LEN 24 /* Max length of an error inject token */
#define MAX_ERRINJCT_TOKENS 8 /* Max # tokens. */
#define WORKSPACE_SIZE 1024 

/*
 * In general to call RTAS use rtas_token("string") to lookup
 * an RTAS token for the given string (e.g. "event-scan").
 * To actually perform the call use
 *    ret = rtas_call(token, n_in, n_out, ...)
 * Where n_in is the number of input parameters and
 *       n_out is the number of output parameters
 *
 * If the "string" is invalid on this system, RTAS_UNKOWN_SERVICE
 * will be returned as a token.  rtas_call() does look for this
 * token and error out gracefully so rtas_call(rtas_token("str"), ...)
 * may be safely used for one-shot calls to RTAS.
 *
 */

typedef u32 rtas_arg_t;

struct rtas_args {
	u32 token;
	u32 nargs;
	u32 nret; 
	rtas_arg_t args[16];
#if 0
	spinlock_t lock;
#endif
	rtas_arg_t *rets;     /* Pointer to return values in args[]. */
};  

struct rtas_t {
	unsigned long entry;		/* physical address pointer */
	unsigned long base;		/* physical address pointer */
	unsigned long size;
	spinlock_t lock;

	struct device_node *dev;	/* virtual address pointer */
};

/* Event classes */
#define RTAS_INTERNAL_ERROR		0x80000000 /* set bit 0 */
#define RTAS_EPOW_WARNING		0x40000000 /* set bit 1 */
#define RTAS_POWERMGM_EVENTS		0x20000000 /* set bit 2 */
#define RTAS_HOTPLUG_EVENTS		0x10000000 /* set bit 3 */
#define RTAS_EVENT_SCAN_ALL_EVENTS	0xf0000000

/* event-scan returns */
#define SEVERITY_FATAL		0x5
#define SEVERITY_ERROR		0x4
#define SEVERITY_ERROR_SYNC	0x3
#define SEVERITY_WARNING	0x2
#define SEVERITY_EVENT		0x1
#define SEVERITY_NO_ERROR	0x0
#define DISP_FULLY_RECOVERED	0x0
#define DISP_LIMITED_RECOVERY	0x1
#define DISP_NOT_RECOVERED	0x2
#define PART_PRESENT		0x0
#define PART_NOT_PRESENT	0x1
#define INITIATOR_UNKNOWN	0x0
#define INITIATOR_CPU		0x1
#define INITIATOR_PCI		0x2
#define INITIATOR_ISA		0x3
#define INITIATOR_MEMORY	0x4
#define INITIATOR_POWERMGM	0x5
#define TARGET_UNKNOWN		0x0
#define TARGET_CPU		0x1
#define TARGET_PCI		0x2
#define TARGET_ISA		0x3
#define TARGET_MEMORY		0x4
#define TARGET_POWERMGM		0x5
#define TYPE_RETRY		0x01
#define TYPE_TCE_ERR		0x02
#define TYPE_INTERN_DEV_FAIL	0x03
#define TYPE_TIMEOUT		0x04
#define TYPE_DATA_PARITY	0x05
#define TYPE_ADDR_PARITY	0x06
#define TYPE_CACHE_PARITY	0x07
#define TYPE_ADDR_INVALID	0x08
#define TYPE_ECC_UNCORR		0x09
#define TYPE_ECC_CORR		0x0a
#define TYPE_EPOW		0x40
/* I don't add PowerMGM events right now, this is a different topic */ 
#define TYPE_PMGM_POWER_SW_ON	0x60
#define TYPE_PMGM_POWER_SW_OFF	0x61
#define TYPE_PMGM_LID_OPEN	0x62
#define TYPE_PMGM_LID_CLOSE	0x63
#define TYPE_PMGM_SLEEP_BTN	0x64
#define TYPE_PMGM_WAKE_BTN	0x65
#define TYPE_PMGM_BATTERY_WARN	0x66
#define TYPE_PMGM_BATTERY_CRIT	0x67
#define TYPE_PMGM_SWITCH_TO_BAT	0x68
#define TYPE_PMGM_SWITCH_TO_AC	0x69
#define TYPE_PMGM_KBD_OR_MOUSE	0x6a
#define TYPE_PMGM_ENCLOS_OPEN	0x6b
#define TYPE_PMGM_ENCLOS_CLOSED	0x6c
#define TYPE_PMGM_RING_INDICATE	0x6d
#define TYPE_PMGM_LAN_ATTENTION	0x6e
#define TYPE_PMGM_TIME_ALARM	0x6f
#define TYPE_PMGM_CONFIG_CHANGE	0x70
#define TYPE_PMGM_SERVICE_PROC	0x71

struct rtas_error_log {
	unsigned long version:8;		/* Architectural version */
	unsigned long severity:3;		/* Severity level of error */
	unsigned long disposition:2;		/* Degree of recovery */
	unsigned long extended:1;		/* extended log present? */
	unsigned long /* reserved */ :2;	/* Reserved for future use */
	unsigned long initiator:4;		/* Initiator of event */
	unsigned long target:4;			/* Target of failed operation */
	unsigned long type:8;			/* General event or error*/
	unsigned long extended_log_length:32;	/* length in bytes */
	unsigned char buffer[1];		/* allocated by klimit bump */
};

struct errinjct_token {
    	char * name;
	int value;
};

struct flash_block {
	char *data;
	unsigned long length;
};

/* This struct is very similar but not identical to
 * that needed by the rtas flash update.
 * All we need to do for rtas is rewrite num_blocks
 * into a version/length and translate the pointers
 * to absolute.
 */
#define FLASH_BLOCKS_PER_NODE ((PAGE_SIZE - 16) / sizeof(struct flash_block))
struct flash_block_list {
	unsigned long num_blocks;
	struct flash_block_list *next;
	struct flash_block blocks[FLASH_BLOCKS_PER_NODE];
};
struct flash_block_list_header { /* just the header of flash_block_list */
	unsigned long num_blocks;
	struct flash_block_list *next;
};
extern struct flash_block_list_header rtas_firmware_flash_list;

extern struct rtas_t rtas;

extern void enter_rtas(struct rtas_args *);
extern int rtas_token(const char *service);
extern long rtas_call(int token, int, int, unsigned long *, ...);
extern void phys_call_rtas(int, int, int, ...);
extern void phys_call_rtas_display_status(char);
extern void call_rtas_display_status(char);
extern void rtas_restart(char *cmd);
extern void rtas_power_off(void);
extern void rtas_halt(void);
extern int rtas_errinjct_open(void);
extern int rtas_errinjct(unsigned int, char *, char *);
extern int rtas_errinjct_close(unsigned int);

extern struct proc_dir_entry *rtas_proc_dir;
extern struct errinjct_token ei_token_list[MAX_ERRINJCT_TOKENS];

extern void pSeries_log_error(char *buf, unsigned int err_type, int fatal);

/* Error types logged.  */
#define ERR_FLAG_ALREADY_LOGGED	0x0
#define ERR_FLAG_BOOT		0x1 	/* log was pulled from NVRAM on boot */
#define ERR_TYPE_RTAS_LOG	0x2	/* from rtas event-scan */
#define ERR_TYPE_KERNEL_PANIC	0x4	/* from panic() */

/* All the types and not flags */
#define ERR_TYPE_MASK	(ERR_TYPE_RTAS_LOG | ERR_TYPE_KERNEL_PANIC)

#define RTAS_ERR KERN_ERR "RTAS: "

#define RTAS_ERROR_LOG_MAX 1024


/* Event Scan Parameters */
#define EVENT_SCAN_ALL_EVENTS	0xf0000000
#define SURVEILLANCE_TOKEN	9000
#define LOG_NUMBER		64		/* must be a power of two */
#define LOG_NUMBER_MASK		(LOG_NUMBER-1)

/* Given an RTAS status code of 9900..9905 compute the hinted delay */
unsigned int rtas_extended_busy_delay_time(int status);
static inline int rtas_is_extended_busy(int status)
{
	return status >= 9900 && status <= 9909;
}

/* Some RTAS ops require a data buffer and that buffer must be < 4G.
 * Rather than having a memory allocator, just use this buffer
 * (get the lock first), make the RTAS call.  Copy the data instead
 * of holding the buffer for long.
 */
#define RTAS_DATA_BUF_SIZE 4096
extern spinlock_t rtas_data_buf_lock;
extern char rtas_data_buf[RTAS_DATA_BUF_SIZE];

#endif /* _PPC64_RTAS_H */
