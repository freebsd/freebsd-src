/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_IOERROR_H
#define _ASM_IA64_SN_IOERROR_H

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <asm/sn/types.h>

/*
 * Macros defining the various Errors to be handled as part of
 * IO Error handling.
 */

/*
 * List of errors to be handled by each subsystem.
 * "error_code" field will take one of these values.
 * The error code is built up of single bits expressing
 * our confidence that the error was that type; note
 * that it is possible to have a PIO or DMA error where
 * we don't know whether it was a READ or a WRITE, or
 * even a READ or WRITE error that we're not sure whether
 * to call a PIO or DMA.
 *
 * It is also possible to set both PIO and DMA, and possible
 * to set both READ and WRITE; the first may be nonsensical
 * but the second *could* be used to designate an access
 * that is known to be a read-modify-write cycle. It is
 * quite possible that nobody will ever use PIO|DMA or
 * READ|WRITE ... but being flexible is good.
 */
#define	IOECODE_UNSPEC		0
#define	IOECODE_READ		1
#define	IOECODE_WRITE		2
#define	IOECODE_PIO		4
#define	IOECODE_DMA		8

#define	IOECODE_PIO_READ	(IOECODE_PIO|IOECODE_READ)
#define	IOECODE_PIO_WRITE	(IOECODE_PIO|IOECODE_WRITE)
#define	IOECODE_DMA_READ	(IOECODE_DMA|IOECODE_READ)
#define	IOECODE_DMA_WRITE	(IOECODE_DMA|IOECODE_WRITE)

/* support older names, but try to move everything
 * to using new names that identify which package
 * controls their values ...
 */
#define	PIO_READ_ERROR		IOECODE_PIO_READ
#define	PIO_WRITE_ERROR		IOECODE_PIO_WRITE
#define	DMA_READ_ERROR		IOECODE_DMA_READ
#define	DMA_WRITE_ERROR		IOECODE_DMA_WRITE

/*
 * List of error numbers returned by error handling sub-system.
 */

#define	IOERROR_HANDLED		0	/* Error Properly handled.        */
#define	IOERROR_NODEV		0x1	/* No such device attached        */
#define	IOERROR_BADHANDLE	0x2	/* Received bad handle            */
#define	IOERROR_BADWIDGETNUM	0x3	/* Bad widget number              */
#define	IOERROR_BADERRORCODE	0x4	/* Bad error code passed in       */
#define	IOERROR_INVALIDADDR	0x5	/* Invalid address specified      */

#define	IOERROR_WIDGETLEVEL	0x6	/* Some failure at widget level    */
#define	IOERROR_XTALKLEVEL	0x7

#define	IOERROR_HWGRAPH_LOOKUP	0x8	/* hwgraph lookup failed for path  */
#define	IOERROR_UNHANDLED	0x9	/* handler rejected error          */

#define	IOERROR_PANIC		0xA	/* subsidiary handler has already
					 * started decode: continue error
					 * data dump, and panic from top
					 * caller in error chain.
					 */

/*
 * IO errors at the bus/device driver level
 */

#define	IOERROR_DEV_NOTFOUND	0x10	/* Device matching bus addr not found */
#define	IOERROR_DEV_SHUTDOWN	0x11	/* Device has been shutdown        */

/*
 * Type of address.
 * Indicates the direction of transfer that caused the error.
 */
#define	IOERROR_ADDR_PIO	1	/* Error Address generated due to PIO */
#define	IOERROR_ADDR_DMA	2	/* Error address generated due to DMA */

/*
 * IO error structure.
 *
 * This structure would expand to hold the information retrieved from
 * all IO related error registers.
 *
 * This structure is defined to hold all system specific
 * information related to a single error.
 *
 * This serves a couple of purpose.
 *      - Error handling often involves translating one form of address to other
 *        form. So, instead of having different data structures at each level,
 *        we have a single structure, and the appropriate fields get filled in
 *        at each layer.
 *      - This provides a way to dump all error related information in any layer
 *        of erorr handling (debugging aid).
 *
 * A second possibility is to allow each layer to define its own error
 * data structure, and fill in the proper fields. This has the advantage
 * of isolating the layers.
 * A big concern is the potential stack usage (and overflow), if each layer
 * defines these structures on stack (assuming we don't want to do kmalloc.
 *
 * Any layer wishing to pass extra information to a layer next to it in
 * error handling hierarchy, can do so as a separate parameter.
 */

typedef struct io_error_s {
    /* Bit fields indicating which structure fields are valid */
    union {
	struct {
	    unsigned                ievb_errortype:1;
	    unsigned                ievb_widgetnum:1;
	    unsigned                ievb_widgetdev:1;
	    unsigned                ievb_srccpu:1;
	    unsigned                ievb_srcnode:1;
	    unsigned                ievb_errnode:1;
	    unsigned                ievb_sysioaddr:1;
	    unsigned                ievb_xtalkaddr:1;
	    unsigned                ievb_busspace:1;
	    unsigned                ievb_busaddr:1;
	    unsigned                ievb_vaddr:1;
	    unsigned                ievb_memaddr:1;
	    unsigned		    ievb_epc:1;
	    unsigned		    ievb_ef:1;
	    unsigned		    ievb_tnum:1;
	} iev_b;
	unsigned                iev_a;
    } ie_v;

    short                   ie_errortype;	/* error type: extra info about error */
    short                   ie_widgetnum;	/* Widget number that's in error */
    short                   ie_widgetdev;	/* Device within widget in error */
    cpuid_t                 ie_srccpu;	/* CPU on srcnode generating error */
    cnodeid_t               ie_srcnode;		/* Node which caused the error   */
    cnodeid_t               ie_errnode;		/* Node where error was noticed  */
    iopaddr_t               ie_sysioaddr;	/* Sys specific IO address       */
    iopaddr_t               ie_xtalkaddr;	/* Xtalk (48bit) addr of Error   */
    iopaddr_t               ie_busspace;	/* Bus specific address space    */
    iopaddr_t               ie_busaddr;		/* Bus specific address          */
    caddr_t                 ie_vaddr;	/* Virtual address of error      */
    paddr_t                 ie_memaddr;		/* Physical memory address       */
    caddr_t		    ie_epc;		/* pc when error reported	 */
    caddr_t		    ie_ef;		/* eframe when error reported	 */
    short		    ie_tnum;		/* Xtalk TNUM field */
} ioerror_t;

#define	IOERROR_INIT(e)		do { (e)->ie_v.iev_a = 0; } while (0)
#define	IOERROR_SETVALUE(e,f,v)	do { (e)->ie_ ## f = (v); (e)->ie_v.iev_b.ievb_ ## f = 1; } while (0)
#define	IOERROR_FIELDVALID(e,f)	((unsigned long long)((e)->ie_v.iev_b.ievb_ ## f) != (unsigned long long) 0)
#define	IOERROR_NOGETVALUE(e,f)	(ASSERT(IOERROR_FIELDVALID(e,f)), ((e)->ie_ ## f))
#define	IOERROR_GETVALUE(p,e,f)	ASSERT(IOERROR_FIELDVALID(e,f)); p=((e)->ie_ ## f)

/* hub code likes to call the SysAD address "hubaddr" ... */
#define	ie_hubaddr	ie_sysioaddr
#define	ievb_hubaddr	ievb_sysioaddr
#endif

/*
 * Error handling Modes.
 */
typedef enum {
    MODE_DEVPROBE,		/* Probing mode. Errors not fatal */
    MODE_DEVERROR,		/* Error while system is running */
    MODE_DEVUSERERROR,		/* Device Error created due to user mode access */
    MODE_DEVREENABLE		/* Reenable pass                */
} ioerror_mode_t;


typedef int             error_handler_f(void *, int, ioerror_mode_t, ioerror_t *);
typedef void           *error_handler_arg_t;

extern void             snia_ioerror_dump(char *, int, int, ioerror_t *);

#ifdef	ERROR_DEBUG
#define	IOERROR_DUMP(x, y, z, t)	snia_ioerror_dump((x), (y), (z), (t))
#define	IOERR_PRINTF(x)	(x)
#else
#define	IOERROR_DUMP(x, y, z, t)
#define	IOERR_PRINTF(x)
#endif				/* ERROR_DEBUG */

#endif /* _ASM_IA64_SN_IOERROR_H */
