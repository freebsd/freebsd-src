/*
 * Copyright (c) 1994, Garrett A. Wollman.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: devconf.h,v 1.6 1995/04/13 20:37:51 wollman Exp $
 */

/*
 * devconf.h - device configuration table
 *
 * Garrett A. Wollman, October 1994.
 */

#ifndef _SYS_DEVCONF_H_
#define _SYS_DEVCONF_H_ 1

#include <machine/devconf.h>

#define	MAXDEVNAME	32
#define MAXDEVDESCR	64

enum dc_state {
	DC_UNKNOWN = 0,		/* don't know the state or driver doesn't support */
	DC_UNCONFIGURED,	/* driver is present but not configured */
	DC_IDLE,		/* driver supports state and is not in use */
	DC_BUSY			/* driver supports state and is currently busy */
};

#define DC_STATENAMES \
	{ \
	    "unknown", "unconfigured", "idle", "busy" \
	}

enum dc_class {
	DC_CLS_UNKNOWN = 0,	/* old drivers don't set class */
	DC_CLS_CPU = 1,		/* CPU devices */
	DC_CLS_BUS = 2,		/* busses */
	DC_CLS_DISK = 4,	/* disks */
	DC_CLS_TAPE = 8,	/* tapes */
	DC_CLS_RDISK = 16,	/* read-only disks */
	DC_CLS_DISPLAY = 32,	/* display devices */
	DC_CLS_SERIAL = 64,	/* serial I/O devices */
	DC_CLS_PARALLEL = 128,	/* parallel I/O devices */
	DC_CLS_NETIF = 256,	/* network interfaces */
	DC_CLS_MISC = 512	/* anything else */
};

#define DC_CLASSNAMES \
	{ \
	    "unknown", "cpu", "bus", "disk", "tape", "rodisk", \
	    "display", "serial", "parallel", "netif", \
	    "misc" \
	}

struct devconf {
	char dc_name[MAXDEVNAME]; 	/* name */
	char dc_descr[MAXDEVDESCR];	/* description */
	int dc_unit;			/* unit number */
	int dc_number;			/* unique id */
	char dc_pname[MAXDEVNAME]; 	/* name of the parent device */
	int dc_punit;			/* unit number of the parent */
	int dc_pnumber;			/* unique id of the parent */
	struct machdep_devconf dc_md;	/* machine-dependent stuff */
	enum dc_state dc_state;		/* state of the device (see above) */
	enum dc_class dc_class;		/* type of device (see above) */
	size_t dc_datalen;		/* length of data */
	char dc_data[1];		/* variable-length data */
};

#ifdef KERNEL

struct kern_devconf;		/* forward declaration */

/*
 * These four routines are called from the generic configuration
 * table code to allow devices to provide their information in a
 * more useful form.
 *
 * EXTERNALIZE: convert internal representation to external and copy out
 * into user space.
 */
struct sysctl_req;
typedef int (*kdc_externalize_t)(struct kern_devconf *, struct sysctl_req *);
/*
 * INTERNALIZE: copy in from user space, convert to internal representation,
 * validate, and set configuration.
 */
typedef int (*kdc_internalize_t)(struct kern_devconf *, struct sysctl_req *);
/*
 * GOAWAY: shut the device down, if possible, and prepare to exit.
 */
typedef int (*kdc_shutdown_t)(struct kern_devconf *, int);

struct kern_devconf {
	struct kern_devconf *kdc_next;		/* filled in by kern_devconf */
	struct kern_devconf **kdc_rlink; 	/* filled in by kern_devconf */
	int kdc_number;				/* filled in by kern_devconf */
	const char *kdc_name;			/* filled in by driver */
	int kdc_unit;				/* filled in by driver */
	struct machdep_kdevconf kdc_md;		/* filled in by driver */
	kdc_externalize_t kdc_externalize;	/* filled in by driver */
	kdc_internalize_t kdc_internalize; 	/* filled in by driver */
	kdc_shutdown_t kdc_shutdown;		/* filled in by driver */
	size_t kdc_datalen;			/* filled in by driver */
	struct kern_devconf *kdc_parent;	/* filled in by driver */
	void *kdc_parentdata;			/* filled in by driver */
	enum dc_state kdc_state; 		/* filled in by driver dynamically */
	const char *kdc_description; 		/* filled in by driver; maybe dyn. */
	enum dc_class kdc_class; 		/* filled in by driver */
};

int dev_attach(struct kern_devconf *);
int dev_detach(struct kern_devconf *);
int dev_shutdownall(int);

#endif /* KERNEL */

/*
 * HW_DEVCONF sysctl(3) identifiers
 */
#define DEVCONF_NUMBER		0	/* get number of devices */
#define DEVCONF_MAXID		1	/* number of items (not really) */

#define HW_DEVCONF_NAMES { \
	{ "number", CTLTYPE_INT }, \
}

#endif /* _SYS_DEVCONF_H_ */
