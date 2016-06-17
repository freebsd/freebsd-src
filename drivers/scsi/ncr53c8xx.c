/******************************************************************************
**  Device driver for the PCI-SCSI NCR538XX controller family.
**
**  Copyright (C) 1994  Wolfgang Stanglmeier
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
**
**  This driver has been ported to Linux from the FreeBSD NCR53C8XX driver
**  and is currently maintained by
**
**          Gerard Roudier              <groudier@free.fr>
**
**  Being given that this driver originates from the FreeBSD version, and
**  in order to keep synergy on both, any suggested enhancements and corrections
**  received on Linux are automatically a potential candidate for the FreeBSD 
**  version.
**
**  The original driver has been written for 386bsd and FreeBSD by
**          Wolfgang Stanglmeier        <wolf@cologne.de>
**          Stefan Esser                <se@mi.Uni-Koeln.de>
**
**  And has been ported to NetBSD by
**          Charles M. Hannum           <mycroft@gnu.ai.mit.edu>
**
**-----------------------------------------------------------------------------
**
**                     Brief history
**
**  December 10 1995 by Gerard Roudier:
**     Initial port to Linux.
**
**  June 23 1996 by Gerard Roudier:
**     Support for 64 bits architectures (Alpha).
**
**  November 30 1996 by Gerard Roudier:
**     Support for Fast-20 scsi.
**     Support for large DMA fifo and 128 dwords bursting.
**
**  February 27 1997 by Gerard Roudier:
**     Support for Fast-40 scsi.
**     Support for on-Board RAM.
**
**  May 3 1997 by Gerard Roudier:
**     Full support for scsi scripts instructions pre-fetching.
**
**  May 19 1997 by Richard Waltham <dormouse@farsrobt.demon.co.uk>:
**     Support for NvRAM detection and reading.
**
**  August 18 1997 by Cort <cort@cs.nmt.edu>:
**     Support for Power/PC (Big Endian).
**
**  June 20 1998 by Gerard Roudier
**     Support for up to 64 tags per lun.
**     O(1) everywhere (C and SCRIPTS) for normal cases.
**     Low PCI traffic for command handling when on-chip RAM is present.
**     Aggressive SCSI SCRIPTS optimizations.
**
*******************************************************************************
*/

/*
**	Supported SCSI-II features:
**	    Synchronous negotiation
**	    Wide negotiation        (depends on the NCR Chip)
**	    Enable disconnection
**	    Tagged command queuing
**	    Parity checking
**	    Etc...
**
**	Supported NCR/SYMBIOS chips:
**		53C810		(8 bits, Fast SCSI-2, no rom BIOS) 
**		53C815		(8 bits, Fast SCSI-2, on board rom BIOS)
**		53C820		(Wide,   Fast SCSI-2, no rom BIOS)
**		53C825		(Wide,   Fast SCSI-2, on board rom BIOS)
**		53C860		(8 bits, Fast 20,     no rom BIOS)
**		53C875		(Wide,   Fast 20,     on board rom BIOS)
**		53C895		(Wide,   Fast 40,     on board rom BIOS)
**		53C895A		(Wide,   Fast 40,     on board rom BIOS)
**		53C896		(Wide,   Fast 40,     on board rom BIOS)
**		53C897		(Wide,   Fast 40,     on board rom BIOS)
**		53C1510D	(Wide,   Fast 40,     on board rom BIOS)
**
**	Other features:
**		Memory mapped IO (linux-1.3.X and above only)
**		Module
**		Shared IRQ (since linux-1.3.72)
*/

/*
**	Name and version of the driver
*/
#define SCSI_NCR_DRIVER_NAME	"ncr53c8xx-3.4.3b-20010512"

#define SCSI_NCR_DEBUG_FLAGS	(0)

/*==========================================================
**
**      Include files
**
**==========================================================
*/

#define LinuxVersionCode(v, p, s) (((v)<<16)+((p)<<8)+(s))

#include <linux/module.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,17)
#include <linux/spinlock.h>
#elif LINUX_VERSION_CODE >= LinuxVersionCode(2,1,93)
#include <asm/spinlock.h>
#endif
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/stat.h>

#include <linux/version.h>
#include <linux/blk.h>

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,1,35)
#include <linux/init.h>
#endif

#ifndef	__init
#define	__init
#endif
#ifndef	__initdata
#define	__initdata
#endif

#if LINUX_VERSION_CODE <= LinuxVersionCode(2,1,92)
#include <linux/bios32.h>
#endif

#include "scsi.h"
#include "hosts.h"
#include "constants.h"
#include "sd.h"

#include <linux/types.h>

/*
**	Define BITS_PER_LONG for earlier linux versions.
*/
#ifndef	BITS_PER_LONG
#if (~0UL) == 0xffffffffUL
#define	BITS_PER_LONG	32
#else
#define	BITS_PER_LONG	64
#endif
#endif

/*
**	Define the BSD style u_int32 and u_int64 type.
**	Are in fact u_int32_t and u_int64_t :-)
*/
typedef u32 u_int32;
typedef u64 u_int64;
typedef	u_long		vm_offset_t;
#include "ncr53c8xx.h"

/*
**	Donnot compile integrity checking code for Linux-2.3.0 
**	and above since SCSI data structures are not ready yet.
*/
/* #if LINUX_VERSION_CODE < LinuxVersionCode(2,3,0) */
#if 0
#define	SCSI_NCR_INTEGRITY_CHECKING
#endif

#define NAME53C			"ncr53c"
#define NAME53C8XX		"ncr53c8xx"
#define DRIVER_SMP_LOCK		ncr53c8xx_lock

#include "sym53c8xx_comm.h"

/*==========================================================
**
**	The CCB done queue uses an array of CCB virtual 
**	addresses. Empty entries are flagged using the bogus 
**	virtual address 0xffffffff.
**
**	Since PCI ensures that only aligned DWORDs are accessed 
**	atomically, 64 bit little-endian architecture requires 
**	to test the high order DWORD of the entry to determine 
**	if it is empty or valid.
**
**	BTW, I will make things differently as soon as I will 
**	have a better idea, but this is simple and should work.
**
**==========================================================
*/
 
#define SCSI_NCR_CCB_DONE_SUPPORT
#ifdef  SCSI_NCR_CCB_DONE_SUPPORT

#define MAX_DONE 24
#define CCB_DONE_EMPTY 0xffffffffUL

/* All 32 bit architectures */
#if BITS_PER_LONG == 32
#define CCB_DONE_VALID(cp)  (((u_long) cp) != CCB_DONE_EMPTY)

/* All > 32 bit (64 bit) architectures regardless endian-ness */
#else
#define CCB_DONE_VALID(cp)  \
	((((u_long) cp) & 0xffffffff00000000ul) && 	\
	 (((u_long) cp) & 0xfffffffful) != CCB_DONE_EMPTY)
#endif

#endif /* SCSI_NCR_CCB_DONE_SUPPORT */

/*==========================================================
**
**	Configuration and Debugging
**
**==========================================================
*/

/*
**    SCSI address of this device.
**    The boot routines should have set it.
**    If not, use this.
*/

#ifndef SCSI_NCR_MYADDR
#define SCSI_NCR_MYADDR      (7)
#endif

/*
**    The maximum number of tags per logic unit.
**    Used only for disk devices that support tags.
*/

#ifndef SCSI_NCR_MAX_TAGS
#define SCSI_NCR_MAX_TAGS    (8)
#endif

/*
**    TAGS are actually limited to 64 tags/lun.
**    We need to deal with power of 2, for alignment constraints.
*/
#if	SCSI_NCR_MAX_TAGS > 64
#define	MAX_TAGS (64)
#else
#define	MAX_TAGS SCSI_NCR_MAX_TAGS
#endif

#define NO_TAG	(255)

/*
**	Choose appropriate type for tag bitmap.
*/
#if	MAX_TAGS > 32
typedef u_int64 tagmap_t;
#else
typedef u_int32 tagmap_t;
#endif

/*
**    Number of targets supported by the driver.
**    n permits target numbers 0..n-1.
**    Default is 16, meaning targets #0..#15.
**    #7 .. is myself.
*/

#ifdef SCSI_NCR_MAX_TARGET
#define MAX_TARGET  (SCSI_NCR_MAX_TARGET)
#else
#define MAX_TARGET  (16)
#endif

/*
**    Number of logic units supported by the driver.
**    n enables logic unit numbers 0..n-1.
**    The common SCSI devices require only
**    one lun, so take 1 as the default.
*/

#ifdef SCSI_NCR_MAX_LUN
#define MAX_LUN    SCSI_NCR_MAX_LUN
#else
#define MAX_LUN    (1)
#endif

/*
**    Asynchronous pre-scaler (ns). Shall be 40
*/
 
#ifndef SCSI_NCR_MIN_ASYNC
#define SCSI_NCR_MIN_ASYNC (40)
#endif

/*
**    The maximum number of jobs scheduled for starting.
**    There should be one slot per target, and one slot
**    for each tag of each target in use.
**    The calculation below is actually quite silly ...
*/

#ifdef SCSI_NCR_CAN_QUEUE
#define MAX_START   (SCSI_NCR_CAN_QUEUE + 4)
#else
#define MAX_START   (MAX_TARGET + 7 * MAX_TAGS)
#endif

/*
**   We limit the max number of pending IO to 250.
**   since we donnot want to allocate more than 1 
**   PAGE for 'scripth'.
*/
#if	MAX_START > 250
#undef	MAX_START
#define	MAX_START 250
#endif

/*
**    The maximum number of segments a transfer is split into.
**    We support up to 127 segments for both read and write.
**    The data scripts are broken into 2 sub-scripts.
**    80 (MAX_SCATTERL) segments are moved from a sub-script 
**    in on-chip RAM. This makes data transfers shorter than 
**    80k (assuming 1k fs) as fast as possible.
*/

#define MAX_SCATTER (SCSI_NCR_MAX_SCATTER)

#if (MAX_SCATTER > 80)
#define MAX_SCATTERL	80
#define	MAX_SCATTERH	(MAX_SCATTER - MAX_SCATTERL)
#else
#define MAX_SCATTERL	(MAX_SCATTER-1)
#define	MAX_SCATTERH	1
#endif

/*
**	other
*/

#define NCR_SNOOP_TIMEOUT (1000000)

/*
**	Head of list of NCR boards
**
**	For kernel version < 1.3.70, host is retrieved by its irq level.
**	For later kernels, the internal host control block address 
**	(struct ncb) is used as device id parameter of the irq stuff.
*/

static struct Scsi_Host		*first_host	= NULL;
static Scsi_Host_Template	*the_template	= NULL;	

/*
**	Other definitions
*/

#define ScsiResult(host_code, scsi_code) (((host_code) << 16) + ((scsi_code) & 0x7f))

static void ncr53c8xx_select_queue_depths(
	struct Scsi_Host *host, struct scsi_device *devlist);
static void ncr53c8xx_intr(int irq, void *dev_id, struct pt_regs * regs);
static void ncr53c8xx_timeout(unsigned long np);

#define initverbose (driver_setup.verbose)
#define bootverbose (np->verbose)

#ifdef SCSI_NCR_NVRAM_SUPPORT
static u_char Tekram_sync[16] __initdata =
	{25,31,37,43, 50,62,75,125, 12,15,18,21, 6,7,9,10};
#endif /* SCSI_NCR_NVRAM_SUPPORT */

/*==========================================================
**
**	Command control block states.
**
**==========================================================
*/

#define HS_IDLE		(0)
#define HS_BUSY		(1)
#define HS_NEGOTIATE	(2)	/* sync/wide data transfer*/
#define HS_DISCONNECT	(3)	/* Disconnected by target */

#define HS_DONEMASK	(0x80)
#define HS_COMPLETE	(4|HS_DONEMASK)
#define HS_SEL_TIMEOUT	(5|HS_DONEMASK)	/* Selection timeout      */
#define HS_RESET	(6|HS_DONEMASK)	/* SCSI reset	          */
#define HS_ABORTED	(7|HS_DONEMASK)	/* Transfer aborted       */
#define HS_TIMEOUT	(8|HS_DONEMASK)	/* Software timeout       */
#define HS_FAIL		(9|HS_DONEMASK)	/* SCSI or PCI bus errors */
#define HS_UNEXPECTED	(10|HS_DONEMASK)/* Unexpected disconnect  */

/*
**	Invalid host status values used by the SCRIPTS processor 
**	when the nexus is not fully identified.
**	Shall never appear in a CCB.
*/

#define HS_INVALMASK	(0x40)
#define	HS_SELECTING	(0|HS_INVALMASK)
#define	HS_IN_RESELECT	(1|HS_INVALMASK)
#define	HS_STARTING	(2|HS_INVALMASK)

/*
**	Flags set by the SCRIPT processor for commands 
**	that have been skipped.
*/
#define HS_SKIPMASK	(0x20)

/*==========================================================
**
**	Software Interrupt Codes
**
**==========================================================
*/

#define	SIR_BAD_STATUS		(1)
#define	SIR_XXXXXXXXXX		(2)
#define	SIR_NEGO_SYNC		(3)
#define	SIR_NEGO_WIDE		(4)
#define	SIR_NEGO_FAILED		(5)
#define	SIR_NEGO_PROTO		(6)
#define	SIR_REJECT_RECEIVED	(7)
#define	SIR_REJECT_SENT		(8)
#define	SIR_IGN_RESIDUE		(9)
#define	SIR_MISSING_SAVE	(10)
#define	SIR_RESEL_NO_MSG_IN	(11)
#define	SIR_RESEL_NO_IDENTIFY	(12)
#define	SIR_RESEL_BAD_LUN	(13)
#define	SIR_RESEL_BAD_TARGET	(14)
#define	SIR_RESEL_BAD_I_T_L	(15)
#define	SIR_RESEL_BAD_I_T_L_Q	(16)
#define	SIR_DONE_OVERFLOW	(17)
#define	SIR_MAX			(17)

/*==========================================================
**
**	Extended error codes.
**	xerr_status field of struct ccb.
**
**==========================================================
*/

#define	XE_OK		(0)
#define	XE_EXTRA_DATA	(1)	/* unexpected data phase */
#define	XE_BAD_PHASE	(2)	/* illegal phase (4/5)   */

/*==========================================================
**
**	Negotiation status.
**	nego_status field	of struct ccb.
**
**==========================================================
*/

#define NS_NOCHANGE	(0)
#define NS_SYNC		(1)
#define NS_WIDE		(2)
#define NS_PPR		(4)

/*==========================================================
**
**	"Special features" of targets.
**	quirks field		of struct tcb.
**	actualquirks field	of struct ccb.
**
**==========================================================
*/

#define	QUIRK_AUTOSAVE	(0x01)
#define	QUIRK_NOMSG	(0x02)
#define QUIRK_NOSYNC	(0x10)
#define QUIRK_NOWIDE16	(0x20)

/*==========================================================
**
**	Capability bits in Inquire response byte 7.
**
**==========================================================
*/

#define	INQ7_QUEUE	(0x02)
#define	INQ7_SYNC	(0x10)
#define	INQ7_WIDE16	(0x20)

/*==========================================================
**
**	Misc.
**
**==========================================================
*/

#define CCB_MAGIC	(0xf2691ad2)

/*==========================================================
**
**	Declaration of structs.
**
**==========================================================
*/

struct tcb;
struct lcb;
struct ccb;
struct ncb;
struct script;

typedef struct ncb * ncb_p;
typedef struct tcb * tcb_p;
typedef struct lcb * lcb_p;
typedef struct ccb * ccb_p;

struct link {
	ncrcmd	l_cmd;
	ncrcmd	l_paddr;
};

struct	usrcmd {
	u_long	target;
	u_long	lun;
	u_long	data;
	u_long	cmd;
};

#define UC_SETSYNC      10
#define UC_SETTAGS	11
#define UC_SETDEBUG	12
#define UC_SETORDER	13
#define UC_SETWIDE	14
#define UC_SETFLAG	15
#define UC_SETVERBOSE	17

#define	UF_TRACE	(0x01)
#define	UF_NODISC	(0x02)
#define	UF_NOSCAN	(0x04)

/*========================================================================
**
**	Declaration of structs:		target control block
**
**========================================================================
*/
struct tcb {
	/*----------------------------------------------------------------
	**	During reselection the ncr jumps to this point with SFBR 
	**	set to the encoded target number with bit 7 set.
	**	if it's not this target, jump to the next.
	**
	**	JUMP  IF (SFBR != #target#), @(next tcb)
	**----------------------------------------------------------------
	*/
	struct link   jump_tcb;

	/*----------------------------------------------------------------
	**	Load the actual values for the sxfer and the scntl3
	**	register (sync/wide mode).
	**
	**	SCR_COPY (1), @(sval field of this tcb), @(sxfer  register)
	**	SCR_COPY (1), @(wval field of this tcb), @(scntl3 register)
	**----------------------------------------------------------------
	*/
	ncrcmd	getscr[6];

	/*----------------------------------------------------------------
	**	Get the IDENTIFY message and load the LUN to SFBR.
	**
	**	CALL, <RESEL_LUN>
	**----------------------------------------------------------------
	*/
	struct link   call_lun;

	/*----------------------------------------------------------------
	**	Now look for the right lun.
	**
	**	For i = 0 to 3
	**		SCR_JUMP ^ IFTRUE(MASK(i, 3)), @(first lcb mod. i)
	**
	**	Recent chips will prefetch the 4 JUMPS using only 1 burst.
	**	It is kind of hashcoding.
	**----------------------------------------------------------------
	*/
	struct link     jump_lcb[4];	/* JUMPs for reselection	*/
	lcb_p		lp[MAX_LUN];	/* The lcb's of this tcb	*/
	u_char		inq_done;	/* Target capabilities received	*/
	u_char		inq_byte7;	/* Contains these capabilities	*/

	/*----------------------------------------------------------------
	**	Pointer to the ccb used for negotiation.
	**	Prevent from starting a negotiation for all queued commands 
	**	when tagged command queuing is enabled.
	**----------------------------------------------------------------
	*/
	ccb_p   nego_cp;

	/*----------------------------------------------------------------
	**	statistical data
	**----------------------------------------------------------------
	*/
	u_long	transfers;
	u_long	bytes;

	/*----------------------------------------------------------------
	**	negotiation of wide and synch transfer and device quirks.
	**----------------------------------------------------------------
	*/
/*0*/	u_char	minsync;
/*1*/	u_char	sval;
/*2*/	u_short	period;
/*0*/	u_char	maxoffs;
/*1*/	u_char	quirks;
/*2*/	u_char	widedone;
/*3*/	u_char	wval;

#ifdef SCSI_NCR_INTEGRITY_CHECKING
	u_char 	ic_min_sync;
	u_char 	ic_max_width;
	u_char 	ic_maximums_set;
	u_char 	ic_done;
#endif

	/*----------------------------------------------------------------
	**	User settable limits and options.
	**	These limits are read from the NVRAM if present.
	**----------------------------------------------------------------
	*/
	u_char	usrsync;
	u_char	usrwide;
	u_char	usrtags;
	u_char	usrflag;
};

/*========================================================================
**
**	Declaration of structs:		lun control block
**
**========================================================================
*/
struct lcb {
	/*----------------------------------------------------------------
	**	During reselection the ncr jumps to this point
	**	with SFBR set to the "Identify" message.
	**	if it's not this lun, jump to the next.
	**
	**	JUMP  IF (SFBR != #lun#), @(next lcb of this target)
	**
	**	It is this lun. Load TEMP with the nexus jumps table 
	**	address and jump to RESEL_TAG (or RESEL_NOTAG).
	**
	**		SCR_COPY (4), p_jump_ccb, TEMP,
	**		SCR_JUMP, <RESEL_TAG>
	**----------------------------------------------------------------
	*/
	struct link	jump_lcb;
	ncrcmd		load_jump_ccb[3];
	struct link	jump_tag;
	ncrcmd		p_jump_ccb;	/* Jump table bus address	*/

	/*----------------------------------------------------------------
	**	Jump table used by the script processor to directly jump 
	**	to the CCB corresponding to the reselected nexus.
	**	Address is allocated on 256 bytes boundary in order to 
	**	allow 8 bit calculation of the tag jump entry for up to 
	**	64 possible tags.
	**----------------------------------------------------------------
	*/
	u_int32		jump_ccb_0;	/* Default table if no tags	*/
	u_int32		*jump_ccb;	/* Virtual address		*/

	/*----------------------------------------------------------------
	**	CCB queue management.
	**----------------------------------------------------------------
	*/
	XPT_QUEHEAD	free_ccbq;	/* Queue of available CCBs	*/
	XPT_QUEHEAD	busy_ccbq;	/* Queue of busy CCBs		*/
	XPT_QUEHEAD	wait_ccbq;	/* Queue of waiting for IO CCBs	*/
	XPT_QUEHEAD	skip_ccbq;	/* Queue of skipped CCBs	*/
	u_char		actccbs;	/* Number of allocated CCBs	*/
	u_char		busyccbs;	/* CCBs busy for this lun	*/
	u_char		queuedccbs;	/* CCBs queued to the controller*/
	u_char		queuedepth;	/* Queue depth for this lun	*/
	u_char		scdev_depth;	/* SCSI device queue depth	*/
	u_char		maxnxs;		/* Max possible nexuses		*/

	/*----------------------------------------------------------------
	**	Control of tagged command queuing.
	**	Tags allocation is performed using a circular buffer.
	**	This avoids using a loop for tag allocation.
	**----------------------------------------------------------------
	*/
	u_char		ia_tag;		/* Allocation index		*/
	u_char		if_tag;		/* Freeing index		*/
	u_char cb_tags[MAX_TAGS];	/* Circular tags buffer	*/
	u_char		usetags;	/* Command queuing is active	*/
	u_char		maxtags;	/* Max nr of tags asked by user	*/
	u_char		numtags;	/* Current number of tags	*/
	u_char		inq_byte7;	/* Store unit CmdQ capabitility	*/

	/*----------------------------------------------------------------
	**	QUEUE FULL control and ORDERED tag control.
	**----------------------------------------------------------------
	*/
	/*----------------------------------------------------------------
	**	QUEUE FULL and ORDERED tag control.
	**----------------------------------------------------------------
	*/
	u_short		num_good;	/* Nr of GOOD since QUEUE FULL	*/
	tagmap_t	tags_umap;	/* Used tags bitmap		*/
	tagmap_t	tags_smap;	/* Tags in use at 'tag_stime'	*/
	u_long		tags_stime;	/* Last time we set smap=umap	*/
	ccb_p		held_ccb;	/* CCB held for QUEUE FULL	*/
};

/*========================================================================
**
**      Declaration of structs:     the launch script.
**
**========================================================================
**
**	It is part of the CCB and is called by the scripts processor to 
**	start or restart the data structure (nexus).
**	This 6 DWORDs mini script makes use of prefetching.
**
**------------------------------------------------------------------------
*/
struct launch {
	/*----------------------------------------------------------------
	**	SCR_COPY(4),	@(p_phys), @(dsa register)
	**	SCR_JUMP,	@(scheduler_point)
	**----------------------------------------------------------------
	*/
	ncrcmd		setup_dsa[3];	/* Copy 'phys' address to dsa	*/
	struct link	schedule;	/* Jump to scheduler point	*/
	ncrcmd		p_phys;		/* 'phys' header bus address	*/
};

/*========================================================================
**
**      Declaration of structs:     global HEADER.
**
**========================================================================
**
**	This substructure is copied from the ccb to a global address after 
**	selection (or reselection) and copied back before disconnect.
**
**	These fields are accessible to the script processor.
**
**------------------------------------------------------------------------
*/

struct head {
	/*----------------------------------------------------------------
	**	Saved data pointer.
	**	Points to the position in the script responsible for the
	**	actual transfer transfer of data.
	**	It's written after reception of a SAVE_DATA_POINTER message.
	**	The goalpointer points after the last transfer command.
	**----------------------------------------------------------------
	*/
	u_int32		savep;
	u_int32		lastp;
	u_int32		goalp;

	/*----------------------------------------------------------------
	**	Alternate data pointer.
	**	They are copied back to savep/lastp/goalp by the SCRIPTS 
	**	when the direction is unknown and the device claims data out.
	**----------------------------------------------------------------
	*/
	u_int32		wlastp;
	u_int32		wgoalp;

	/*----------------------------------------------------------------
	**	The virtual address of the ccb containing this header.
	**----------------------------------------------------------------
	*/
	ccb_p	cp;

	/*----------------------------------------------------------------
	**	Status fields.
	**----------------------------------------------------------------
	*/
	u_char		scr_st[4];	/* script status		*/
	u_char		status[4];	/* host status. must be the 	*/
					/*  last DWORD of the header.	*/
};

/*
**	The status bytes are used by the host and the script processor.
**
**	The byte corresponding to the host_status must be stored in the 
**	last DWORD of the CCB header since it is used for command 
**	completion (ncr_wakeup()). Doing so, we are sure that the header 
**	has been entirely copied back to the CCB when the host_status is 
**	seen complete by the CPU.
**
**	The last four bytes (status[4]) are copied to the scratchb register
**	(declared as scr0..scr3 in ncr_reg.h) just after the select/reselect,
**	and copied back just after disconnecting.
**	Inside the script the XX_REG are used.
**
**	The first four bytes (scr_st[4]) are used inside the script by 
**	"COPY" commands.
**	Because source and destination must have the same alignment
**	in a DWORD, the fields HAVE to be at the choosen offsets.
**		xerr_st		0	(0x34)	scratcha
**		sync_st		1	(0x05)	sxfer
**		wide_st		3	(0x03)	scntl3
*/

/*
**	Last four bytes (script)
*/
#define  QU_REG	scr0
#define  HS_REG	scr1
#define  HS_PRT	nc_scr1
#define  SS_REG	scr2
#define  SS_PRT	nc_scr2
#define  PS_REG	scr3

/*
**	Last four bytes (host)
*/
#define  actualquirks  phys.header.status[0]
#define  host_status   phys.header.status[1]
#define  scsi_status   phys.header.status[2]
#define  parity_status phys.header.status[3]

/*
**	First four bytes (script)
*/
#define  xerr_st       header.scr_st[0]
#define  sync_st       header.scr_st[1]
#define  nego_st       header.scr_st[2]
#define  wide_st       header.scr_st[3]

/*
**	First four bytes (host)
*/
#define  xerr_status   phys.xerr_st
#define  nego_status   phys.nego_st

#if 0
#define  sync_status   phys.sync_st
#define  wide_status   phys.wide_st
#endif

/*==========================================================
**
**      Declaration of structs:     Data structure block
**
**==========================================================
**
**	During execution of a ccb by the script processor,
**	the DSA (data structure address) register points
**	to this substructure of the ccb.
**	This substructure contains the header with
**	the script-processor-changable data and
**	data blocks for the indirect move commands.
**
**----------------------------------------------------------
*/

struct dsb {

	/*
	**	Header.
	*/

	struct head	header;

	/*
	**	Table data for Script
	*/

	struct scr_tblsel  select;
	struct scr_tblmove smsg  ;
	struct scr_tblmove cmd   ;
	struct scr_tblmove sense ;
	struct scr_tblmove data [MAX_SCATTER];
};


/*========================================================================
**
**      Declaration of structs:     Command control block.
**
**========================================================================
*/
struct ccb {
	/*----------------------------------------------------------------
	**	This is the data structure which is pointed by the DSA 
	**	register when it is executed by the script processor.
	**	It must be the first entry because it contains the header 
	**	as first entry that must be cache line aligned.
	**----------------------------------------------------------------
	*/
	struct dsb	phys;

	/*----------------------------------------------------------------
	**	Mini-script used at CCB execution start-up.
	**	Load the DSA with the data structure address (phys) and 
	**	jump to SELECT. Jump to CANCEL if CCB is to be canceled.
	**----------------------------------------------------------------
	*/
	struct launch	start;

	/*----------------------------------------------------------------
	**	Mini-script used at CCB relection to restart the nexus.
	**	Load the DSA with the data structure address (phys) and 
	**	jump to RESEL_DSA. Jump to ABORT if CCB is to be aborted.
	**----------------------------------------------------------------
	*/
	struct launch	restart;

	/*----------------------------------------------------------------
	**	If a data transfer phase is terminated too early
	**	(after reception of a message (i.e. DISCONNECT)),
	**	we have to prepare a mini script to transfer
	**	the rest of the data.
	**----------------------------------------------------------------
	*/
	ncrcmd		patch[8];

	/*----------------------------------------------------------------
	**	The general SCSI driver provides a
	**	pointer to a control block.
	**----------------------------------------------------------------
	*/
	Scsi_Cmnd	*cmd;		/* SCSI command 		*/
	u_char		cdb_buf[16];	/* Copy of CDB			*/
	u_char		sense_buf[64];
	int		data_len;	/* Total data length		*/

	/*----------------------------------------------------------------
	**	Message areas.
	**	We prepare a message to be sent after selection.
	**	We may use a second one if the command is rescheduled 
	**	due to GETCC or QFULL.
	**      Contents are IDENTIFY and SIMPLE_TAG.
	**	While negotiating sync or wide transfer,
	**	a SDTR or WDTR message is appended.
	**----------------------------------------------------------------
	*/
	u_char		scsi_smsg [8];
	u_char		scsi_smsg2[8];

	/*----------------------------------------------------------------
	**	Other fields.
	**----------------------------------------------------------------
	*/
	u_long		p_ccb;		/* BUS address of this CCB	*/
	u_char		sensecmd[6];	/* Sense command		*/
	u_char		tag;		/* Tag for this transfer	*/
					/*  255 means no tag		*/
	u_char		target;
	u_char		lun;
	u_char		queued;
	u_char		auto_sense;
	ccb_p		link_ccb;	/* Host adapter CCB chain	*/
	XPT_QUEHEAD	link_ccbq;	/* Link to unit CCB queue	*/
	u_int32		startp;		/* Initial data pointer		*/
	u_long		magic;		/* Free / busy  CCB flag	*/
};

#define CCB_PHYS(cp,lbl)	(cp->p_ccb + offsetof(struct ccb, lbl))


/*========================================================================
**
**      Declaration of structs:     NCR device descriptor
**
**========================================================================
*/
struct ncb {
	/*----------------------------------------------------------------
	**	The global header.
	**	It is accessible to both the host and the script processor.
	**	Must be cache line size aligned (32 for x86) in order to 
	**	allow cache line bursting when it is copied to/from CCB.
	**----------------------------------------------------------------
	*/
	struct head     header;

	/*----------------------------------------------------------------
	**	CCBs management queues.
	**----------------------------------------------------------------
	*/
	Scsi_Cmnd	*waiting_list;	/* Commands waiting for a CCB	*/
					/*  when lcb is not allocated.	*/
	Scsi_Cmnd	*done_list;	/* Commands waiting for done()  */
					/* callback to be invoked.      */ 
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,1,93)
	spinlock_t	smp_lock;	/* Lock for SMP threading       */
#endif

	/*----------------------------------------------------------------
	**	Chip and controller indentification.
	**----------------------------------------------------------------
	*/
	int		unit;		/* Unit number			*/
	char		chip_name[8];	/* Chip name			*/
	char		inst_name[16];	/* ncb instance name		*/

	/*----------------------------------------------------------------
	**	Initial value of some IO register bits.
	**	These values are assumed to have been set by BIOS, and may 
	**	be used for probing adapter implementation differences.
	**----------------------------------------------------------------
	*/
	u_char	sv_scntl0, sv_scntl3, sv_dmode, sv_dcntl, sv_ctest3, sv_ctest4,
		sv_ctest5, sv_gpcntl, sv_stest2, sv_stest4;

	/*----------------------------------------------------------------
	**	Actual initial value of IO register bits used by the 
	**	driver. They are loaded at initialisation according to  
	**	features that are to be enabled.
	**----------------------------------------------------------------
	*/
	u_char	rv_scntl0, rv_scntl3, rv_dmode, rv_dcntl, rv_ctest3, rv_ctest4, 
		rv_ctest5, rv_stest2;

	/*----------------------------------------------------------------
	**	Targets management.
	**	During reselection the ncr jumps to jump_tcb.
	**	The SFBR register is loaded with the encoded target id.
	**	For i = 0 to 3
	**		SCR_JUMP ^ IFTRUE(MASK(i, 3)), @(next tcb mod. i)
	**
	**	Recent chips will prefetch the 4 JUMPS using only 1 burst.
	**	It is kind of hashcoding.
	**----------------------------------------------------------------
	*/
	struct link     jump_tcb[4];	/* JUMPs for reselection	*/
	struct tcb  target[MAX_TARGET];	/* Target data			*/

	/*----------------------------------------------------------------
	**	Virtual and physical bus addresses of the chip.
	**----------------------------------------------------------------
	*/
	vm_offset_t	vaddr;		/* Virtual and bus address of	*/
	vm_offset_t     paddr;		/*  chip's IO registers.	*/
	vm_offset_t     paddr2;		/* On-chip RAM bus address.	*/
	volatile			/* Pointer to volatile for 	*/
	struct ncr_reg	*reg;		/*  memory mapped IO.		*/

	/*----------------------------------------------------------------
	**	SCRIPTS virtual and physical bus addresses.
	**	'script'  is loaded in the on-chip RAM if present.
	**	'scripth' stays in main memory.
	**----------------------------------------------------------------
	*/
	struct script	*script0;	/* Copies of script and scripth	*/
	struct scripth	*scripth0;	/*  relocated for this ncb.	*/
	struct scripth	*scripth;	/* Actual scripth virt. address	*/
	u_long		p_script;	/* Actual script and scripth	*/
	u_long		p_scripth;	/*  bus addresses.		*/

	/*----------------------------------------------------------------
	**	General controller parameters and configuration.
	**----------------------------------------------------------------
	*/
	pcidev_t	pdev;
	u_short		device_id;	/* PCI device id		*/
	u_char		revision_id;	/* PCI device revision id	*/
	u_char		bus;		/* PCI BUS number		*/
	u_char		device_fn;	/* PCI BUS device and function	*/
	u_long		base_io;	/* IO space base address	*/
	u_int		irq;		/* IRQ level			*/
	u_int		features;	/* Chip features map		*/
	u_char		myaddr;		/* SCSI id of the adapter	*/
	u_char		maxburst;	/* log base 2 of dwords burst	*/
	u_char		maxwide;	/* Maximum transfer width	*/
	u_char		minsync;	/* Minimum sync period factor	*/
	u_char		maxsync;	/* Maximum sync period factor	*/
	u_char		maxoffs;	/* Max scsi offset		*/
	u_char		multiplier;	/* Clock multiplier (1,2,4)	*/
	u_char		clock_divn;	/* Number of clock divisors	*/
	u_long		clock_khz;	/* SCSI clock frequency in KHz	*/

	/*----------------------------------------------------------------
	**	Start queue management.
	**	It is filled up by the host processor and accessed by the 
	**	SCRIPTS processor in order to start SCSI commands.
	**----------------------------------------------------------------
	*/
	u_short		squeueput;	/* Next free slot of the queue	*/
	u_short		actccbs;	/* Number of allocated CCBs	*/
	u_short		queuedccbs;	/* Number of CCBs in start queue*/
	u_short		queuedepth;	/* Start queue depth		*/

	/*----------------------------------------------------------------
	**	Timeout handler.
	**----------------------------------------------------------------
	*/
	struct timer_list timer;	/* Timer handler link header	*/
	u_long		lasttime;
	u_long		settle_time;	/* Resetting the SCSI BUS	*/

	/*----------------------------------------------------------------
	**	Debugging and profiling.
	**----------------------------------------------------------------
	*/
	struct ncr_reg	regdump;	/* Register dump		*/
	u_long		regtime;	/* Time it has been done	*/

	/*----------------------------------------------------------------
	**	Miscellaneous buffers accessed by the scripts-processor.
	**	They shall be DWORD aligned, because they may be read or 
	**	written with a SCR_COPY script command.
	**----------------------------------------------------------------
	*/
	u_char		msgout[8];	/* Buffer for MESSAGE OUT 	*/
	u_char		msgin [8];	/* Buffer for MESSAGE IN	*/
	u_int32		lastmsg;	/* Last SCSI message sent	*/
	u_char		scratch;	/* Scratch for SCSI receive	*/

	/*----------------------------------------------------------------
	**	Miscellaneous configuration and status parameters.
	**----------------------------------------------------------------
	*/
	u_char		disc;		/* Diconnection allowed		*/
	u_char		scsi_mode;	/* Current SCSI BUS mode	*/
	u_char		order;		/* Tag order to use		*/
	u_char		verbose;	/* Verbosity for this controller*/
	int		ncr_cache;	/* Used for cache test at init.	*/
	u_long		p_ncb;		/* BUS address of this NCB	*/

	/*----------------------------------------------------------------
	**	Command completion handling.
	**----------------------------------------------------------------
	*/
#ifdef SCSI_NCR_CCB_DONE_SUPPORT
	struct ccb	*(ccb_done[MAX_DONE]);
	int		ccb_done_ic;
#endif
	/*----------------------------------------------------------------
	**	Fields that should be removed or changed.
	**----------------------------------------------------------------
	*/
	struct ccb	*ccb;		/* Global CCB			*/
	struct usrcmd	user;		/* Command from user		*/
	u_char		release_stage;	/* Synchronisation stage on release  */

#ifdef SCSI_NCR_INTEGRITY_CHECKING
	/*----------------------------------------------------------------
	**	Fields that are used for integrity check
	**----------------------------------------------------------------
	*/
	unsigned char check_integrity; /* Enable midlayer integ.check on
					* bus scan. */
	unsigned char check_integ_par;  /* Set if par or Init. Det. error
					 * used only during integ check */
#endif
};

#define NCB_SCRIPT_PHYS(np,lbl)	 (np->p_script  + offsetof (struct script, lbl))
#define NCB_SCRIPTH_PHYS(np,lbl) (np->p_scripth + offsetof (struct scripth,lbl))

/*==========================================================
**
**
**      Script for NCR-Processor.
**
**	Use ncr_script_fill() to create the variable parts.
**	Use ncr_script_copy_and_bind() to make a copy and
**	bind to physical addresses.
**
**
**==========================================================
**
**	We have to know the offsets of all labels before
**	we reach them (for forward jumps).
**	Therefore we declare a struct here.
**	If you make changes inside the script,
**	DONT FORGET TO CHANGE THE LENGTHS HERE!
**
**----------------------------------------------------------
*/

/*
**	Script fragments which are loaded into the on-chip RAM 
**	of 825A, 875 and 895 chips.
*/
struct script {
	ncrcmd	start		[  5];
	ncrcmd  startpos	[  1];
	ncrcmd	select		[  6];
	ncrcmd	select2		[  9];
	ncrcmd	loadpos		[  4];
	ncrcmd	send_ident	[  9];
	ncrcmd	prepare		[  6];
	ncrcmd	prepare2	[  7];
	ncrcmd  command		[  6];
	ncrcmd  dispatch	[ 32];
	ncrcmd  clrack		[  4];
	ncrcmd	no_data		[ 17];
	ncrcmd  status		[  8];
	ncrcmd  msg_in		[  2];
	ncrcmd  msg_in2		[ 16];
	ncrcmd  msg_bad		[  4];
	ncrcmd	setmsg		[  7];
	ncrcmd	cleanup		[  6];
	ncrcmd  complete	[  9];
	ncrcmd	cleanup_ok	[  8];
	ncrcmd	cleanup0	[  1];
#ifndef SCSI_NCR_CCB_DONE_SUPPORT
	ncrcmd	signal		[ 12];
#else
	ncrcmd	signal		[  9];
	ncrcmd	done_pos	[  1];
	ncrcmd	done_plug	[  2];
	ncrcmd	done_end	[  7];
#endif
	ncrcmd  save_dp		[  7];
	ncrcmd  restore_dp	[  5];
	ncrcmd  disconnect	[ 17];
	ncrcmd	msg_out		[  9];
	ncrcmd	msg_out_done	[  7];
	ncrcmd  idle		[  2];
	ncrcmd	reselect	[  8];
	ncrcmd	reselected	[  8];
	ncrcmd	resel_dsa	[  6];
	ncrcmd	loadpos1	[  4];
	ncrcmd  resel_lun	[  6];
	ncrcmd	resel_tag	[  6];
	ncrcmd	jump_to_nexus	[  4];
	ncrcmd	nexus_indirect	[  4];
	ncrcmd	resel_notag	[  4];
	ncrcmd  data_in		[MAX_SCATTERL * 4];
	ncrcmd  data_in2	[  4];
	ncrcmd  data_out	[MAX_SCATTERL * 4];
	ncrcmd  data_out2	[  4];
};

/*
**	Script fragments which stay in main memory for all chips.
*/
struct scripth {
	ncrcmd  tryloop		[MAX_START*2];
	ncrcmd  tryloop2	[  2];
#ifdef SCSI_NCR_CCB_DONE_SUPPORT
	ncrcmd  done_queue	[MAX_DONE*5];
	ncrcmd  done_queue2	[  2];
#endif
	ncrcmd	select_no_atn	[  8];
	ncrcmd	cancel		[  4];
	ncrcmd	skip		[  9];
	ncrcmd	skip2		[ 19];
	ncrcmd	par_err_data_in	[  6];
	ncrcmd	par_err_other	[  4];
	ncrcmd	msg_reject	[  8];
	ncrcmd	msg_ign_residue	[ 24];
	ncrcmd  msg_extended	[ 10];
	ncrcmd  msg_ext_2	[ 10];
	ncrcmd	msg_wdtr	[ 14];
	ncrcmd	send_wdtr	[  7];
	ncrcmd  msg_ext_3	[ 10];
	ncrcmd	msg_sdtr	[ 14];
	ncrcmd	send_sdtr	[  7];
	ncrcmd	nego_bad_phase	[  4];
	ncrcmd	msg_out_abort	[ 10];
	ncrcmd  hdata_in	[MAX_SCATTERH * 4];
	ncrcmd  hdata_in2	[  2];
	ncrcmd  hdata_out	[MAX_SCATTERH * 4];
	ncrcmd  hdata_out2	[  2];
	ncrcmd	reset		[  4];
	ncrcmd	aborttag	[  4];
	ncrcmd	abort		[  2];
	ncrcmd	abort_resel	[ 20];
	ncrcmd	resend_ident	[  4];
	ncrcmd	clratn_go_on	[  3];
	ncrcmd	nxtdsp_go_on	[  1];
	ncrcmd	sdata_in	[  8];
	ncrcmd  data_io		[ 18];
	ncrcmd	bad_identify	[ 12];
	ncrcmd	bad_i_t_l	[  4];
	ncrcmd	bad_i_t_l_q	[  4];
	ncrcmd	bad_target	[  8];
	ncrcmd	bad_status	[  8];
	ncrcmd	start_ram	[  4];
	ncrcmd	start_ram0	[  4];
	ncrcmd	sto_restart	[  5];
	ncrcmd	snooptest	[  9];
	ncrcmd	snoopend	[  2];
};

/*==========================================================
**
**
**      Function headers.
**
**
**==========================================================
*/

static	void	ncr_alloc_ccb	(ncb_p np, u_char tn, u_char ln);
static	void	ncr_complete	(ncb_p np, ccb_p cp);
static	void	ncr_exception	(ncb_p np);
static	void	ncr_free_ccb	(ncb_p np, ccb_p cp);
static	void	ncr_init_ccb	(ncb_p np, ccb_p cp);
static	void	ncr_init_tcb	(ncb_p np, u_char tn);
static	lcb_p	ncr_alloc_lcb	(ncb_p np, u_char tn, u_char ln);
static	lcb_p	ncr_setup_lcb	(ncb_p np, u_char tn, u_char ln,
				 u_char *inq_data);
static	void	ncr_getclock	(ncb_p np, int mult);
static	void	ncr_selectclock	(ncb_p np, u_char scntl3);
static	ccb_p	ncr_get_ccb	(ncb_p np, u_char tn, u_char ln);
static	void	ncr_init	(ncb_p np, int reset, char * msg, u_long code);
static	int	ncr_int_sbmc	(ncb_p np);
static	int	ncr_int_par	(ncb_p np);
static	void	ncr_int_ma	(ncb_p np);
static	void	ncr_int_sir	(ncb_p np);
static  void    ncr_int_sto     (ncb_p np);
static	u_long	ncr_lookup	(char* id);
static	void	ncr_negotiate	(struct ncb* np, struct tcb* tp);
static	int	ncr_prepare_nego(ncb_p np, ccb_p cp, u_char *msgptr);
#ifdef SCSI_NCR_INTEGRITY_CHECKING
static	int	ncr_ic_nego(ncb_p np, ccb_p cp, Scsi_Cmnd *cmd, u_char *msgptr);
#endif

static	void	ncr_script_copy_and_bind
				(ncb_p np, ncrcmd *src, ncrcmd *dst, int len);
static  void    ncr_script_fill (struct script * scr, struct scripth * scripth);
static	int	ncr_scatter	(ncb_p np, ccb_p cp, Scsi_Cmnd *cmd);
static	void	ncr_getsync	(ncb_p np, u_char sfac, u_char *fakp, u_char *scntl3p);
static	void	ncr_setsync	(ncb_p np, ccb_p cp, u_char scntl3, u_char sxfer);
static	void	ncr_setup_tags	(ncb_p np, u_char tn, u_char ln);
static	void	ncr_setwide	(ncb_p np, ccb_p cp, u_char wide, u_char ack);
static	int	ncr_show_msg	(u_char * msg);
static  void    ncr_print_msg   (ccb_p cp, char *label, u_char *msg);
static	int	ncr_snooptest	(ncb_p np);
static	void	ncr_timeout	(ncb_p np);
static  void    ncr_wakeup      (ncb_p np, u_long code);
static  void    ncr_wakeup_done (ncb_p np);
static	void	ncr_start_next_ccb (ncb_p np, lcb_p lp, int maxn);
static	void	ncr_put_start_queue(ncb_p np, ccb_p cp);
static	void	ncr_start_reset	(ncb_p np);
static	int	ncr_reset_scsi_bus (ncb_p np, int enab_int, int settle_delay);

#ifdef SCSI_NCR_USER_COMMAND_SUPPORT
static	void	ncr_usercmd	(ncb_p np);
#endif

static int ncr_attach (Scsi_Host_Template *tpnt, int unit, ncr_device *device);

static void insert_into_waiting_list(ncb_p np, Scsi_Cmnd *cmd);
static Scsi_Cmnd *retrieve_from_waiting_list(int to_remove, ncb_p np, Scsi_Cmnd *cmd);
static void process_waiting_list(ncb_p np, int sts);

#define remove_from_waiting_list(np, cmd) \
		retrieve_from_waiting_list(1, (np), (cmd))
#define requeue_waiting_list(np) process_waiting_list((np), DID_OK)
#define reset_waiting_list(np) process_waiting_list((np), DID_RESET)

static inline char *ncr_name (ncb_p np)
{
	return np->inst_name;
}


/*==========================================================
**
**
**      Scripts for NCR-Processor.
**
**      Use ncr_script_bind for binding to physical addresses.
**
**
**==========================================================
**
**	NADDR generates a reference to a field of the controller data.
**	PADDR generates a reference to another part of the script.
**	RADDR generates a reference to a script processor register.
**	FADDR generates a reference to a script processor register
**		with offset.
**
**----------------------------------------------------------
*/

#define	RELOC_SOFTC	0x40000000
#define	RELOC_LABEL	0x50000000
#define	RELOC_REGISTER	0x60000000
#if 0
#define	RELOC_KVAR	0x70000000
#endif
#define	RELOC_LABELH	0x80000000
#define	RELOC_MASK	0xf0000000

#define	NADDR(label)	(RELOC_SOFTC | offsetof(struct ncb, label))
#define PADDR(label)    (RELOC_LABEL | offsetof(struct script, label))
#define PADDRH(label)   (RELOC_LABELH | offsetof(struct scripth, label))
#define	RADDR(label)	(RELOC_REGISTER | REG(label))
#define	FADDR(label,ofs)(RELOC_REGISTER | ((REG(label))+(ofs)))
#if 0
#define	KVAR(which)	(RELOC_KVAR | (which))
#endif

#if 0
#define	SCRIPT_KVAR_JIFFIES	(0)
#define	SCRIPT_KVAR_FIRST		SCRIPT_KVAR_JIFFIES
#define	SCRIPT_KVAR_LAST		SCRIPT_KVAR_JIFFIES
/*
 * Kernel variables referenced in the scripts.
 * THESE MUST ALL BE ALIGNED TO A 4-BYTE BOUNDARY.
 */
static void *script_kvars[] __initdata =
	{ (void *)&jiffies };
#endif

static	struct script script0 __initdata = {
/*--------------------------< START >-----------------------*/ {
	/*
	**	This NOP will be patched with LED ON
	**	SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	*/
	SCR_NO_OP,
		0,
	/*
	**      Clear SIGP.
	*/
	SCR_FROM_REG (ctest2),
		0,
	/*
	**	Then jump to a certain point in tryloop.
	**	Due to the lack of indirect addressing the code
	**	is self modifying here.
	*/
	SCR_JUMP,
}/*-------------------------< STARTPOS >--------------------*/,{
		PADDRH(tryloop),

}/*-------------------------< SELECT >----------------------*/,{
	/*
	**	DSA	contains the address of a scheduled
	**		data structure.
	**
	**	SCRATCHA contains the address of the script,
	**		which starts the next entry.
	**
	**	Set Initiator mode.
	**
	**	(Target mode is left as an exercise for the reader)
	*/

	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (HS_REG, HS_SELECTING),
		0,

	/*
	**      And try to select this target.
	*/
	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR (reselect),

}/*-------------------------< SELECT2 >----------------------*/,{
	/*
	**	Now there are 4 possibilities:
	**
	**	(1) The ncr looses arbitration.
	**	This is ok, because it will try again,
	**	when the bus becomes idle.
	**	(But beware of the timeout function!)
	**
	**	(2) The ncr is reselected.
	**	Then the script processor takes the jump
	**	to the RESELECT label.
	**
	**	(3) The ncr wins arbitration.
	**	Then it will execute SCRIPTS instruction until 
	**	the next instruction that checks SCSI phase.
	**	Then will stop and wait for selection to be 
	**	complete or selection time-out to occur.
	**	As a result the SCRIPTS instructions until 
	**	LOADPOS + 2 should be executed in parallel with 
	**	the SCSI core performing selection.
	*/

	/*
	**	The M_REJECT problem seems to be due to a selection 
	**	timing problem.
	**	Wait immediately for the selection to complete. 
	**	(2.5x behaves so)
	*/
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		0,

	/*
	**	Next time use the next slot.
	*/
	SCR_COPY (4),
		RADDR (temp),
		PADDR (startpos),
	/*
	**      The ncr doesn't have an indirect load
	**	or store command. So we have to
	**	copy part of the control block to a
	**	fixed place, where we can access it.
	**
	**	We patch the address part of a
	**	COPY command with the DSA-register.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (loadpos),
	/*
	**	then we do the actual copy.
	*/
	SCR_COPY (sizeof (struct head)),
	/*
	**	continued after the next label ...
	*/
}/*-------------------------< LOADPOS >---------------------*/,{
		0,
		NADDR (header),
	/*
	**	Wait for the next phase or the selection
	**	to complete or time-out.
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDR (prepare),

}/*-------------------------< SEND_IDENT >----------------------*/,{
	/*
	**	Selection complete.
	**	Send the IDENTIFY and SIMPLE_TAG messages
	**	(and the M_X_SYNC_REQ message)
	*/
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg),
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDRH (resend_ident),
	SCR_LOAD_REG (scratcha, 0x80),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (lastmsg),
}/*-------------------------< PREPARE >----------------------*/,{
	/*
	**      load the savep (saved pointer) into
	**      the TEMP register (actual pointer)
	*/
	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	/*
	**      Initialize the status registers
	*/
	SCR_COPY (4),
		NADDR (header.status),
		RADDR (scr0),
}/*-------------------------< PREPARE2 >---------------------*/,{
	/*
	**	Initialize the msgout buffer with a NOOP message.
	*/
	SCR_LOAD_REG (scratcha, M_NOOP),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
#if 0
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgin),
#endif
	/*
	**	Anticipate the COMMAND phase.
	**	This is the normal case for initial selection.
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_COMMAND)),
		PADDR (dispatch),

}/*-------------------------< COMMAND >--------------------*/,{
	/*
	**	... and send the command
	*/
	SCR_MOVE_TBL ^ SCR_COMMAND,
		offsetof (struct dsb, cmd),
	/*
	**	If status is still HS_NEGOTIATE, negotiation failed.
	**	We check this here, since we want to do that 
	**	only once.
	*/
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,

}/*-----------------------< DISPATCH >----------------------*/,{
	/*
	**	MSG_IN is the only phase that shall be 
	**	entered at least once for each (re)selection.
	**	So we test it first.
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR (msg_in),

	SCR_RETURN ^ IFTRUE (IF (SCR_DATA_OUT)),
		0,
	/*
	**	DEL 397 - 53C875 Rev 3 - Part Number 609-0392410 - ITEM 4.
	**	Possible data corruption during Memory Write and Invalidate.
	**	This work-around resets the addressing logic prior to the 
	**	start of the first MOVE of a DATA IN phase.
	**	(See README.ncr53c8xx for more information)
	*/
	SCR_JUMPR ^ IFFALSE (IF (SCR_DATA_IN)),
		20,
	SCR_COPY (4),
		RADDR (scratcha),
		RADDR (scratcha),
	SCR_RETURN,
 		0,
	SCR_JUMP ^ IFTRUE (IF (SCR_STATUS)),
		PADDR (status),
	SCR_JUMP ^ IFTRUE (IF (SCR_COMMAND)),
		PADDR (command),
	SCR_JUMP ^ IFTRUE (IF (SCR_MSG_OUT)),
		PADDR (msg_out),
	/*
	**      Discard one illegal phase byte, if required.
	*/
	SCR_LOAD_REG (scratcha, XE_BAD_PHASE),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (xerr_st),
	SCR_JUMPR ^ IFFALSE (IF (SCR_ILG_OUT)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_ILG_OUT,
		NADDR (scratch),
	SCR_JUMPR ^ IFFALSE (IF (SCR_ILG_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_ILG_IN,
		NADDR (scratch),
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< CLRACK >----------------------*/,{
	/*
	**	Terminate possible pending message phase.
	*/
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< NO_DATA >--------------------*/,{
	/*
	**	The target wants to tranfer too much data
	**	or in the wrong direction.
	**      Remember that in extended error.
	*/
	SCR_LOAD_REG (scratcha, XE_EXTRA_DATA),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (xerr_st),
	/*
	**      Discard one data byte, if required.
	*/
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_OUT)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_DATA_OUT,
		NADDR (scratch),
	SCR_JUMPR ^ IFFALSE (IF (SCR_DATA_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_DATA_IN,
		NADDR (scratch),
	/*
	**      .. and repeat as required.
	*/
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),

}/*-------------------------< STATUS >--------------------*/,{
	/*
	**	get the status
	*/
	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
	/*
	**	save status to scsi_status.
	**	mark as complete.
	*/
	SCR_TO_REG (SS_REG),
		0,
	SCR_LOAD_REG (HS_REG, HS_COMPLETE),
		0,
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< MSG_IN >--------------------*/,{
	/*
	**	Get the first byte of the message
	**	and save it to SCRATCHA.
	**
	**	The script processor doesn't negate the
	**	ACK signal after this transfer.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[0]),
}/*-------------------------< MSG_IN2 >--------------------*/,{
	/*
	**	Handle this message.
	*/
	SCR_JUMP ^ IFTRUE (DATA (M_COMPLETE)),
		PADDR (complete),
	SCR_JUMP ^ IFTRUE (DATA (M_DISCONNECT)),
		PADDR (disconnect),
	SCR_JUMP ^ IFTRUE (DATA (M_SAVE_DP)),
		PADDR (save_dp),
	SCR_JUMP ^ IFTRUE (DATA (M_RESTORE_DP)),
		PADDR (restore_dp),
	SCR_JUMP ^ IFTRUE (DATA (M_EXTENDED)),
		PADDRH (msg_extended),
	SCR_JUMP ^ IFTRUE (DATA (M_NOOP)),
		PADDR (clrack),
	SCR_JUMP ^ IFTRUE (DATA (M_REJECT)),
		PADDRH (msg_reject),
	SCR_JUMP ^ IFTRUE (DATA (M_IGN_RESIDUE)),
		PADDRH (msg_ign_residue),
	/*
	**	Rest of the messages left as
	**	an exercise ...
	**
	**	Unimplemented messages:
	**	fall through to MSG_BAD.
	*/
}/*-------------------------< MSG_BAD >------------------*/,{
	/*
	**	unimplemented message - reject it.
	*/
	SCR_INT,
		SIR_REJECT_SENT,
	SCR_LOAD_REG (scratcha, M_REJECT),
		0,
}/*-------------------------< SETMSG >----------------------*/,{
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_SET (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR (clrack),
}/*-------------------------< CLEANUP >-------------------*/,{
	/*
	**      dsa:    Pointer to ccb
	**	      or xxxxxxFF (no ccb)
	**
	**      HS_REG:   Host-Status (<>0!)
	*/
	SCR_FROM_REG (dsa),
		0,
	SCR_JUMP ^ IFTRUE (DATA (0xff)),
		PADDR (start),
	/*
	**      dsa is valid.
	**	complete the cleanup.
	*/
	SCR_JUMP,
		PADDR (cleanup_ok),

}/*-------------------------< COMPLETE >-----------------*/,{
	/*
	**	Complete message.
	**
	**	Copy TEMP register to LASTP in header.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.lastp),
	/*
	**	When we terminate the cycle by clearing ACK,
	**	the target may disconnect immediately.
	**
	**	We don't want to be told of an
	**	"unexpected disconnect",
	**	so we disable this feature.
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	/*
	**	Terminate cycle ...
	*/
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	/*
	**	... and wait for the disconnect.
	*/
	SCR_WAIT_DISC,
		0,
}/*-------------------------< CLEANUP_OK >----------------*/,{
	/*
	**	Save host status to header.
	*/
	SCR_COPY (4),
		RADDR (scr0),
		NADDR (header.status),
	/*
	**	and copy back the header to the ccb.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (cleanup0),
	SCR_COPY (sizeof (struct head)),
		NADDR (header),
}/*-------------------------< CLEANUP0 >--------------------*/,{
		0,
}/*-------------------------< SIGNAL >----------------------*/,{
	/*
	**	if job not completed ...
	*/
	SCR_FROM_REG (HS_REG),
		0,
	/*
	**	... start the next command.
	*/
	SCR_JUMP ^ IFTRUE (MASK (0, (HS_DONEMASK|HS_SKIPMASK))),
		PADDR(start),
	/*
	**	If command resulted in not GOOD status,
	**	call the C code if needed.
	*/
	SCR_FROM_REG (SS_REG),
		0,
	SCR_CALL ^ IFFALSE (DATA (S_GOOD)),
		PADDRH (bad_status),

#ifndef	SCSI_NCR_CCB_DONE_SUPPORT

	/*
	**	... signal completion to the host
	*/
	SCR_INT_FLY,
		0,
	/*
	**	Auf zu neuen Schandtaten!
	*/
	SCR_JUMP,
		PADDR(start),

#else	/* defined SCSI_NCR_CCB_DONE_SUPPORT */

	/*
	**	... signal completion to the host
	*/
	SCR_JUMP,
}/*------------------------< DONE_POS >---------------------*/,{
		PADDRH (done_queue),
}/*------------------------< DONE_PLUG >--------------------*/,{
	SCR_INT,
		SIR_DONE_OVERFLOW,
}/*------------------------< DONE_END >---------------------*/,{
	SCR_INT_FLY,
		0,
	SCR_COPY (4),
		RADDR (temp),
		PADDR (done_pos),
	SCR_JUMP,
		PADDR (start),

#endif	/* SCSI_NCR_CCB_DONE_SUPPORT */

}/*-------------------------< SAVE_DP >------------------*/,{
	/*
	**	SAVE_DP message:
	**	Copy TEMP register to SAVEP in header.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.savep),
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< RESTORE_DP >---------------*/,{
	/*
	**	RESTORE_DP message:
	**	Copy SAVEP in header to TEMP register.
	*/
	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< DISCONNECT >---------------*/,{
	/*
	**	DISCONNECTing  ...
	**
	**	disable the "unexpected disconnect" feature,
	**	and remove the ACK signal.
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	/*
	**	Wait for the disconnect.
	*/
	SCR_WAIT_DISC,
		0,
	/*
	**	Status is: DISCONNECTED.
	*/
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	/*
	**	If QUIRK_AUTOSAVE is set,
	**	do an "save pointer" operation.
	*/
	SCR_FROM_REG (QU_REG),
		0,
	SCR_JUMP ^ IFFALSE (MASK (QUIRK_AUTOSAVE, QUIRK_AUTOSAVE)),
		PADDR (cleanup_ok),
	/*
	**	like SAVE_DP message:
	**	Copy TEMP register to SAVEP in header.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.savep),
	SCR_JUMP,
		PADDR (cleanup_ok),

}/*-------------------------< MSG_OUT >-------------------*/,{
	/*
	**	The target requests a message.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	/*
	**	If it was no ABORT message ...
	*/
	SCR_JUMP ^ IFTRUE (DATA (M_ABORT)),
		PADDRH (msg_out_abort),
	/*
	**	... wait for the next phase
	**	if it's a message out, send it again, ...
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDR (msg_out),
}/*-------------------------< MSG_OUT_DONE >--------------*/,{
	/*
	**	... else clear the message ...
	*/
	SCR_LOAD_REG (scratcha, M_NOOP),
		0,
	SCR_COPY (4),
		RADDR (scratcha),
		NADDR (msgout),
	/*
	**	... and process the next phase
	*/
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< IDLE >------------------------*/,{
	/*
	**	Nothing to do?
	**	Wait for reselect.
	**	This NOP will be patched with LED OFF
	**	SCR_REG_REG (gpreg, SCR_OR, 0x01)
	*/
	SCR_NO_OP,
		0,
}/*-------------------------< RESELECT >--------------------*/,{
	/*
	**	make the DSA invalid.
	*/
	SCR_LOAD_REG (dsa, 0xff),
		0,
	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (HS_REG, HS_IN_RESELECT),
		0,
	/*
	**	Sleep waiting for a reselection.
	**	If SIGP is set, special treatment.
	**
	**	Zu allem bereit ..
	*/
	SCR_WAIT_RESEL,
		PADDR(start),
}/*-------------------------< RESELECTED >------------------*/,{
	/*
	**	This NOP will be patched with LED ON
	**	SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	*/
	SCR_NO_OP,
		0,
	/*
	**	... zu nichts zu gebrauchen ?
	**
	**      load the target id into the SFBR
	**	and jump to the control block.
	**
	**	Look at the declarations of
	**	- struct ncb
	**	- struct tcb
	**	- struct lcb
	**	- struct ccb
	**	to understand what's going on.
	*/
	SCR_REG_SFBR (ssid, SCR_AND, 0x8F),
		0,
	SCR_TO_REG (sdid),
		0,
	SCR_JUMP,
		NADDR (jump_tcb),

}/*-------------------------< RESEL_DSA >-------------------*/,{
	/*
	**	Ack the IDENTIFY or TAG previously received.
	*/
	SCR_CLR (SCR_ACK),
		0,
	/*
	**      The ncr doesn't have an indirect load
	**	or store command. So we have to
	**	copy part of the control block to a
	**	fixed place, where we can access it.
	**
	**	We patch the address part of a
	**	COPY command with the DSA-register.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (loadpos1),
	/*
	**	then we do the actual copy.
	*/
	SCR_COPY (sizeof (struct head)),
	/*
	**	continued after the next label ...
	*/

}/*-------------------------< LOADPOS1 >-------------------*/,{
		0,
		NADDR (header),
	/*
	**	The DSA contains the data structure address.
	*/
	SCR_JUMP,
		PADDR (prepare),

}/*-------------------------< RESEL_LUN >-------------------*/,{
	/*
	**	come back to this point
	**	to get an IDENTIFY message
	**	Wait for a msg_in phase.
	*/
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_IN)),
		SIR_RESEL_NO_MSG_IN,
	/*
	**	message phase.
	**	Read the data directly from the BUS DATA lines.
	**	This helps to support very old SCSI devices that 
	**	may reselect without sending an IDENTIFY.
	*/
	SCR_FROM_REG (sbdl),
		0,
	/*
	**	It should be an Identify message.
	*/
	SCR_RETURN,
		0,
}/*-------------------------< RESEL_TAG >-------------------*/,{
	/*
	**	Read IDENTIFY + SIMPLE + TAG using a single MOVE.
	**	Agressive optimization, is'nt it?
	**	No need to test the SIMPLE TAG message, since the 
	**	driver only supports conformant devices for tags. ;-)
	*/
	SCR_MOVE_ABS (3) ^ SCR_MSG_IN,
		NADDR (msgin),
	/*
	**	Read the TAG from the SIDL.
	**	Still an aggressive optimization. ;-)
	**	Compute the CCB indirect jump address which 
	**	is (#TAG*2 & 0xfc) due to tag numbering using 
	**	1,3,5..MAXTAGS*2+1 actual values.
	*/
	SCR_REG_SFBR (sidl, SCR_SHL, 0),
		0,
	SCR_SFBR_REG (temp, SCR_AND, 0xfc),
		0,
}/*-------------------------< JUMP_TO_NEXUS >-------------------*/,{
	SCR_COPY_F (4),
		RADDR (temp),
		PADDR (nexus_indirect),
	SCR_COPY (4),
}/*-------------------------< NEXUS_INDIRECT >-------------------*/,{
		0,
		RADDR (temp),
	SCR_RETURN,
		0,
}/*-------------------------< RESEL_NOTAG >-------------------*/,{
	/*
	**	No tag expected.
	**	Read an throw away the IDENTIFY.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_JUMP,
		PADDR (jump_to_nexus),
}/*-------------------------< DATA_IN >--------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTERL parameter,
**	it is filled in at runtime.
**
**  ##===========< i=0; i<MAX_SCATTERL >=========
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_IN,
**  ||		offsetof (struct dsb, data[ i]),
**  ##==========================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< DATA_IN2 >-------------------*/,{
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
}/*-------------------------< DATA_OUT >--------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTERL parameter,
**	it is filled in at runtime.
**
**  ##===========< i=0; i<MAX_SCATTERL >=========
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_OUT,
**  ||		offsetof (struct dsb, data[ i]),
**  ##==========================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< DATA_OUT2 >-------------------*/,{
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
}/*--------------------------------------------------------*/
};

static	struct scripth scripth0 __initdata = {
/*-------------------------< TRYLOOP >---------------------*/{
/*
**	Start the next entry.
**	Called addresses point to the launch script in the CCB.
**	They are patched by the main processor.
**
**	Because the size depends on the
**	#define MAX_START parameter, it is filled
**	in at runtime.
**
**-----------------------------------------------------------
**
**  ##===========< I=0; i<MAX_START >===========
**  ||	SCR_CALL,
**  ||		PADDR (idle),
**  ##==========================================
**
**-----------------------------------------------------------
*/
0
}/*------------------------< TRYLOOP2 >---------------------*/,{
	SCR_JUMP,
		PADDRH(tryloop),

#ifdef SCSI_NCR_CCB_DONE_SUPPORT

}/*------------------------< DONE_QUEUE >-------------------*/,{
/*
**	Copy the CCB address to the next done entry.
**	Because the size depends on the
**	#define MAX_DONE parameter, it is filled
**	in at runtime.
**
**-----------------------------------------------------------
**
**  ##===========< I=0; i<MAX_DONE >===========
**  ||	SCR_COPY (sizeof(ccb_p)),
**  ||		NADDR (header.cp),
**  ||		NADDR (ccb_done[i]),
**  ||	SCR_CALL,
**  ||		PADDR (done_end),
**  ##==========================================
**
**-----------------------------------------------------------
*/
0
}/*------------------------< DONE_QUEUE2 >------------------*/,{
	SCR_JUMP,
		PADDRH (done_queue),

#endif /* SCSI_NCR_CCB_DONE_SUPPORT */
}/*------------------------< SELECT_NO_ATN >-----------------*/,{
	/*
	**	Set Initiator mode.
	**      And try to select this target without ATN.
	*/

	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (HS_REG, HS_SELECTING),
		0,
	SCR_SEL_TBL ^ offsetof (struct dsb, select),
		PADDR (reselect),
	SCR_JUMP,
		PADDR (select2),

}/*-------------------------< CANCEL >------------------------*/,{

	SCR_LOAD_REG (scratcha, HS_ABORTED),
		0,
	SCR_JUMPR,
		8,
}/*-------------------------< SKIP >------------------------*/,{
	SCR_LOAD_REG (scratcha, 0),
		0,
	/*
	**	This entry has been canceled.
	**	Next time use the next slot.
	*/
	SCR_COPY (4),
		RADDR (temp),
		PADDR (startpos),
	/*
	**      The ncr doesn't have an indirect load
	**	or store command. So we have to
	**	copy part of the control block to a
	**	fixed place, where we can access it.
	**
	**	We patch the address part of a
	**	COPY command with the DSA-register.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDRH (skip2),
	/*
	**	then we do the actual copy.
	*/
	SCR_COPY (sizeof (struct head)),
	/*
	**	continued after the next label ...
	*/
}/*-------------------------< SKIP2 >---------------------*/,{
		0,
		NADDR (header),
	/*
	**      Initialize the status registers
	*/
	SCR_COPY (4),
		NADDR (header.status),
		RADDR (scr0),
	/*
	**	Force host status.
	*/
	SCR_FROM_REG (scratcha),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (0, HS_DONEMASK)),
		16,
	SCR_REG_REG (HS_REG, SCR_OR, HS_SKIPMASK),
		0,
	SCR_JUMPR,
		8,
	SCR_TO_REG (HS_REG),
		0,
	SCR_LOAD_REG (SS_REG, S_GOOD),
		0,
	SCR_JUMP,
		PADDR (cleanup_ok),

},/*-------------------------< PAR_ERR_DATA_IN >---------------*/{
	/*
	**	Ignore all data in byte, until next phase
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN)),
		PADDRH (par_err_other),
	SCR_MOVE_ABS (1) ^ SCR_DATA_IN,
		NADDR (scratch),
	SCR_JUMPR,
		-24,
},/*-------------------------< PAR_ERR_OTHER >------------------*/{
	/*
	**	count it.
	*/
	SCR_REG_REG (PS_REG, SCR_ADD, 0x01),
		0,
	/*
	**	jump to dispatcher.
	*/
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< MSG_REJECT >---------------*/,{
	/*
	**	If a negotiation was in progress,
	**	negotiation failed.
	**	Otherwise, let the C code print 
	**	some message.
	*/
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFFALSE (DATA (HS_NEGOTIATE)),
		SIR_REJECT_RECEIVED,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< MSG_IGN_RESIDUE >----------*/,{
	/*
	**	Terminate cycle
	*/
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get residue size.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	/*
	**	Size is 0 .. ignore message.
	*/
	SCR_JUMP ^ IFTRUE (DATA (0)),
		PADDR (clrack),
	/*
	**	Size is not 1 .. have to interrupt.
	*/
	SCR_JUMPR ^ IFFALSE (DATA (1)),
		40,
	/*
	**	Check for residue byte in swide register
	*/
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (WSR, WSR)),
		16,
	/*
	**	There IS data in the swide register.
	**	Discard it.
	*/
	SCR_REG_REG (scntl2, SCR_OR, WSR),
		0,
	SCR_JUMP,
		PADDR (clrack),
	/*
	**	Load again the size to the sfbr register.
	*/
	SCR_FROM_REG (scratcha),
		0,
	SCR_INT,
		SIR_IGN_RESIDUE,
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< MSG_EXTENDED >-------------*/,{
	/*
	**	Terminate cycle
	*/
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get length.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	/*
	*/
	SCR_JUMP ^ IFTRUE (DATA (3)),
		PADDRH (msg_ext_3),
	SCR_JUMP ^ IFFALSE (DATA (2)),
		PADDR (msg_bad),
}/*-------------------------< MSG_EXT_2 >----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get extended message code.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[2]),
	SCR_JUMP ^ IFTRUE (DATA (M_X_WIDE_REQ)),
		PADDRH (msg_wdtr),
	/*
	**	unknown extended message
	*/
	SCR_JUMP,
		PADDR (msg_bad)
}/*-------------------------< MSG_WDTR >-----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get data bus width
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[3]),
	/*
	**	let the host do the real work.
	*/
	SCR_INT,
		SIR_NEGO_WIDE,
	/*
	**	let the target fetch our answer.
	*/
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),

}/*-------------------------< SEND_WDTR >----------------*/,{
	/*
	**	Send the M_X_WIDE_REQ
	*/
	SCR_MOVE_ABS (4) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (msg_out_done),

}/*-------------------------< MSG_EXT_3 >----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get extended message code.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[2]),
	SCR_JUMP ^ IFTRUE (DATA (M_X_SYNC_REQ)),
		PADDRH (msg_sdtr),
	/*
	**	unknown extended message
	*/
	SCR_JUMP,
		PADDR (msg_bad)

}/*-------------------------< MSG_SDTR >-----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get period and offset
	*/
	SCR_MOVE_ABS (2) ^ SCR_MSG_IN,
		NADDR (msgin[3]),
	/*
	**	let the host do the real work.
	*/
	SCR_INT,
		SIR_NEGO_SYNC,
	/*
	**	let the target fetch our answer.
	*/
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),

}/*-------------------------< SEND_SDTR >-------------*/,{
	/*
	**	Send the M_X_SYNC_REQ
	*/
	SCR_MOVE_ABS (5) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (msg_out_done),

}/*-------------------------< NEGO_BAD_PHASE >------------*/,{
	SCR_INT,
		SIR_NEGO_PROTO,
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< MSG_OUT_ABORT >-------------*/,{
	/*
	**	After ABORT message,
	**
	**	expect an immediate disconnect, ...
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	/*
	**	... and set the status to "ABORTED"
	*/
	SCR_LOAD_REG (HS_REG, HS_ABORTED),
		0,
	SCR_JUMP,
		PADDR (cleanup),

}/*-------------------------< HDATA_IN >-------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTERH parameter,
**	it is filled in at runtime.
**
**  ##==< i=MAX_SCATTERL; i<MAX_SCATTERL+MAX_SCATTERH >==
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_IN,
**  ||		offsetof (struct dsb, data[ i]),
**  ##===================================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< HDATA_IN2 >------------------*/,{
	SCR_JUMP,
		PADDR (data_in),

}/*-------------------------< HDATA_OUT >-------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTERH parameter,
**	it is filled in at runtime.
**
**  ##==< i=MAX_SCATTERL; i<MAX_SCATTERL+MAX_SCATTERH >==
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_OUT,
**  ||		offsetof (struct dsb, data[ i]),
**  ##===================================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< HDATA_OUT2 >------------------*/,{
	SCR_JUMP,
		PADDR (data_out),

}/*-------------------------< RESET >----------------------*/,{
	/*
	**      Send a M_RESET message if bad IDENTIFY 
	**	received on reselection.
	*/
	SCR_LOAD_REG (scratcha, M_ABORT_TAG),
		0,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< ABORTTAG >-------------------*/,{
	/*
	**      Abort a wrong tag received on reselection.
	*/
	SCR_LOAD_REG (scratcha, M_ABORT_TAG),
		0,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< ABORT >----------------------*/,{
	/*
	**      Abort a reselection when no active CCB.
	*/
	SCR_LOAD_REG (scratcha, M_ABORT),
		0,
}/*-------------------------< ABORT_RESEL >----------------*/,{
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	/*
	**	and send it.
	**	we expect an immediate disconnect
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< RESEND_IDENT >-------------------*/,{
	/*
	**	The target stays in MSG OUT phase after having acked 
	**	Identify [+ Tag [+ Extended message ]]. Targets shall
	**	behave this way on parity error.
	**	We must send it again all the messages.
	*/
	SCR_SET (SCR_ATN), /* Shall be asserted 2 deskew delays before the  */
		0,         /* 1rst ACK = 90 ns. Hope the NCR is'nt too fast */
	SCR_JUMP,
		PADDR (send_ident),
}/*-------------------------< CLRATN_GO_ON >-------------------*/,{
	SCR_CLR (SCR_ATN),
		0,
	SCR_JUMP,
}/*-------------------------< NXTDSP_GO_ON >-------------------*/,{
		0,
}/*-------------------------< SDATA_IN >-------------------*/,{
	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN)),
		PADDR (dispatch),
	SCR_MOVE_TBL ^ SCR_DATA_IN,
		offsetof (struct dsb, sense),
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
}/*-------------------------< DATA_IO >--------------------*/,{
	/*
	**	We jump here if the data direction was unknown at the 
	**	time we had to queue the command to the scripts processor.
	**	Pointers had been set as follow in this situation:
	**	  savep   -->   DATA_IO
	**	  lastp   -->   start pointer when DATA_IN
	**	  goalp   -->   goal  pointer when DATA_IN
	**	  wlastp  -->   start pointer when DATA_OUT
	**	  wgoalp  -->   goal  pointer when DATA_OUT
	**	This script sets savep/lastp/goalp according to the 
	**	direction chosen by the target.
	*/
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_DATA_OUT)),
		32,
	/*
	**	Direction is DATA IN.
	**	Warning: we jump here, even when phase is DATA OUT.
	*/
	SCR_COPY (4),
		NADDR (header.lastp),
		NADDR (header.savep),

	/*
	**	Jump to the SCRIPTS according to actual direction.
	*/
	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	SCR_RETURN,
		0,
	/*
	**	Direction is DATA OUT.
	*/
	SCR_COPY (4),
		NADDR (header.wlastp),
		NADDR (header.lastp),
	SCR_COPY (4),
		NADDR (header.wgoalp),
		NADDR (header.goalp),
	SCR_JUMPR,
		-64,
}/*-------------------------< BAD_IDENTIFY >---------------*/,{
	/*
	**	If message phase but not an IDENTIFY,
	**	get some help from the C code.
	**	Old SCSI device may behave so.
	*/
	SCR_JUMPR ^ IFTRUE (MASK (0x80, 0x80)),
		16,
	SCR_INT,
		SIR_RESEL_NO_IDENTIFY,
	SCR_JUMP,
		PADDRH (reset),
	/*
	**	Message is an IDENTIFY, but lun is unknown.
	**	Read the message, since we got it directly 
	**	from the SCSI BUS data lines.
	**	Signal problem to C code for logging the event.
	**	Send a M_ABORT to clear all pending tasks.
	*/
	SCR_INT,
		SIR_RESEL_BAD_LUN,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_JUMP,
		PADDRH (abort),
}/*-------------------------< BAD_I_T_L >------------------*/,{
	/*
	**	We donnot have a task for that I_T_L.
	**	Signal problem to C code for logging the event.
	**	Send a M_ABORT message.
	*/
	SCR_INT,
		SIR_RESEL_BAD_I_T_L,
	SCR_JUMP,
		PADDRH (abort),
}/*-------------------------< BAD_I_T_L_Q >----------------*/,{
	/*
	**	We donnot have a task that matches the tag.
	**	Signal problem to C code for logging the event.
	**	Send a M_ABORTTAG message.
	*/
	SCR_INT,
		SIR_RESEL_BAD_I_T_L_Q,
	SCR_JUMP,
		PADDRH (aborttag),
}/*-------------------------< BAD_TARGET >-----------------*/,{
	/*
	**	We donnot know the target that reselected us.
	**	Grab the first message if any (IDENTIFY).
	**	Signal problem to C code for logging the event.
	**	M_RESET message.
	*/
	SCR_INT,
		SIR_RESEL_BAD_TARGET,
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_JUMP,
		PADDRH (reset),
}/*-------------------------< BAD_STATUS >-----------------*/,{
	/*
	**	If command resulted in either QUEUE FULL,
	**	CHECK CONDITION or COMMAND TERMINATED,
	**	call the C code.
	*/
	SCR_INT ^ IFTRUE (DATA (S_QUEUE_FULL)),
		SIR_BAD_STATUS,
	SCR_INT ^ IFTRUE (DATA (S_CHECK_COND)),
		SIR_BAD_STATUS,
	SCR_INT ^ IFTRUE (DATA (S_TERMINATED)),
		SIR_BAD_STATUS,
	SCR_RETURN,
		0,
}/*-------------------------< START_RAM >-------------------*/,{
	/*
	**	Load the script into on-chip RAM, 
	**	and jump to start point.
	*/
	SCR_COPY_F (4),
		RADDR (scratcha),
		PADDRH (start_ram0),
	SCR_COPY (sizeof (struct script)),
}/*-------------------------< START_RAM0 >--------------------*/,{
		0,
		PADDR (start),
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< STO_RESTART >-------------------*/,{
	/*
	**
	**	Repair start queue (e.g. next time use the next slot) 
	**	and jump to start point.
	*/
	SCR_COPY (4),
		RADDR (temp),
		PADDR (startpos),
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< SNOOPTEST >-------------------*/,{
	/*
	**	Read the variable.
	*/
	SCR_COPY (4),
		NADDR(ncr_cache),
		RADDR (scratcha),
	/*
	**	Write the variable.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR(ncr_cache),
	/*
	**	Read back the variable.
	*/
	SCR_COPY (4),
		NADDR(ncr_cache),
		RADDR (temp),
}/*-------------------------< SNOOPEND >-------------------*/,{
	/*
	**	And stop.
	*/
	SCR_INT,
		99,
}/*--------------------------------------------------------*/
};

/*==========================================================
**
**
**	Fill in #define dependent parts of the script
**
**
**==========================================================
*/

void __init ncr_script_fill (struct script * scr, struct scripth * scrh)
{
	int	i;
	ncrcmd	*p;

	p = scrh->tryloop;
	for (i=0; i<MAX_START; i++) {
		*p++ =SCR_CALL;
		*p++ =PADDR (idle);
	};

	assert ((u_long)p == (u_long)&scrh->tryloop + sizeof (scrh->tryloop));

#ifdef SCSI_NCR_CCB_DONE_SUPPORT

	p = scrh->done_queue;
	for (i = 0; i<MAX_DONE; i++) {
		*p++ =SCR_COPY (sizeof(ccb_p));
		*p++ =NADDR (header.cp);
		*p++ =NADDR (ccb_done[i]);
		*p++ =SCR_CALL;
		*p++ =PADDR (done_end);
	}

	assert ((u_long)p ==(u_long)&scrh->done_queue+sizeof(scrh->done_queue));

#endif /* SCSI_NCR_CCB_DONE_SUPPORT */

	p = scrh->hdata_in;
	for (i=0; i<MAX_SCATTERH; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_IN;
		*p++ =offsetof (struct dsb, data[i]);
	};
	assert ((u_long)p == (u_long)&scrh->hdata_in + sizeof (scrh->hdata_in));

	p = scr->data_in;
	for (i=MAX_SCATTERH; i<MAX_SCATTERH+MAX_SCATTERL; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_IN;
		*p++ =offsetof (struct dsb, data[i]);
	};
	assert ((u_long)p == (u_long)&scr->data_in + sizeof (scr->data_in));

	p = scrh->hdata_out;
	for (i=0; i<MAX_SCATTERH; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_OUT;
		*p++ =offsetof (struct dsb, data[i]);
	};
	assert ((u_long)p==(u_long)&scrh->hdata_out + sizeof (scrh->hdata_out));

	p = scr->data_out;
	for (i=MAX_SCATTERH; i<MAX_SCATTERH+MAX_SCATTERL; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_OUT;
		*p++ =offsetof (struct dsb, data[i]);
	};

	assert ((u_long)p == (u_long)&scr->data_out + sizeof (scr->data_out));
}

/*==========================================================
**
**
**	Copy and rebind a script.
**
**
**==========================================================
*/

static void __init 
ncr_script_copy_and_bind (ncb_p np, ncrcmd *src, ncrcmd *dst, int len)
{
	ncrcmd  opcode, new, old, tmp1, tmp2;
	ncrcmd	*start, *end;
	int relocs;
	int opchanged = 0;

	start = src;
	end = src + len/4;

	while (src < end) {

		opcode = *src++;
		*dst++ = cpu_to_scr(opcode);

		/*
		**	If we forget to change the length
		**	in struct script, a field will be
		**	padded with 0. This is an illegal
		**	command.
		*/

		if (opcode == 0) {
			printk (KERN_ERR "%s: ERROR0 IN SCRIPT at %d.\n",
				ncr_name(np), (int) (src-start-1));
			MDELAY (1000);
		};

		if (DEBUG_FLAGS & DEBUG_SCRIPT)
			printk (KERN_DEBUG "%p:  <%x>\n",
				(src-1), (unsigned)opcode);

		/*
		**	We don't have to decode ALL commands
		*/
		switch (opcode >> 28) {

		case 0xc:
			/*
			**	COPY has TWO arguments.
			*/
			relocs = 2;
			tmp1 = src[0];
#ifdef	RELOC_KVAR
			if ((tmp1 & RELOC_MASK) == RELOC_KVAR)
				tmp1 = 0;
#endif
			tmp2 = src[1];
#ifdef	RELOC_KVAR
			if ((tmp2 & RELOC_MASK) == RELOC_KVAR)
				tmp2 = 0;
#endif
			if ((tmp1 ^ tmp2) & 3) {
				printk (KERN_ERR"%s: ERROR1 IN SCRIPT at %d.\n",
					ncr_name(np), (int) (src-start-1));
				MDELAY (1000);
			}
			/*
			**	If PREFETCH feature not enabled, remove 
			**	the NO FLUSH bit if present.
			*/
			if ((opcode & SCR_NO_FLUSH) && !(np->features & FE_PFEN)) {
				dst[-1] = cpu_to_scr(opcode & ~SCR_NO_FLUSH);
				++opchanged;
			}
			break;

		case 0x0:
			/*
			**	MOVE (absolute address)
			*/
			relocs = 1;
			break;

		case 0x8:
			/*
			**	JUMP / CALL
			**	dont't relocate if relative :-)
			*/
			if (opcode & 0x00800000)
				relocs = 0;
			else
				relocs = 1;
			break;

		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
			relocs = 1;
			break;

		default:
			relocs = 0;
			break;
		};

		if (relocs) {
			while (relocs--) {
				old = *src++;

				switch (old & RELOC_MASK) {
				case RELOC_REGISTER:
					new = (old & ~RELOC_MASK) + np->paddr;
					break;
				case RELOC_LABEL:
					new = (old & ~RELOC_MASK) + np->p_script;
					break;
				case RELOC_LABELH:
					new = (old & ~RELOC_MASK) + np->p_scripth;
					break;
				case RELOC_SOFTC:
					new = (old & ~RELOC_MASK) + np->p_ncb;
					break;
#ifdef	RELOC_KVAR
				case RELOC_KVAR:
					if (((old & ~RELOC_MASK) <
					     SCRIPT_KVAR_FIRST) ||
					    ((old & ~RELOC_MASK) >
					     SCRIPT_KVAR_LAST))
						panic("ncr KVAR out of range");
					new = vtophys(script_kvars[old &
					    ~RELOC_MASK]);
					break;
#endif
				case 0:
					/* Don't relocate a 0 address. */
					if (old == 0) {
						new = old;
						break;
					}
					/* fall through */
				default:
					panic("ncr_script_copy_and_bind: weird relocation %x\n", old);
					break;
				}

				*dst++ = cpu_to_scr(new);
			}
		} else
			*dst++ = cpu_to_scr(*src++);

	};
}

/*==========================================================
**
**
**      Auto configuration:  attach and init a host adapter.
**
**
**==========================================================
*/

/*
**	Linux host data structure
**
**	The script area is allocated in the host data structure
**	because kmalloc() returns NULL during scsi initialisations
**	with Linux 1.2.X
*/

struct host_data {
     struct ncb *ncb;
};

/*
**	Print something which allows to retrieve the controller type, unit,
**	target, lun concerned by a kernel message.
*/

static void PRINT_TARGET(ncb_p np, int target)
{
	printk(KERN_INFO "%s-<%d,*>: ", ncr_name(np), target);
}

static void PRINT_LUN(ncb_p np, int target, int lun)
{
	printk(KERN_INFO "%s-<%d,%d>: ", ncr_name(np), target, lun);
}

static void PRINT_ADDR(Scsi_Cmnd *cmd)
{
	struct host_data *host_data = (struct host_data *) cmd->host->hostdata;
	PRINT_LUN(host_data->ncb, cmd->target, cmd->lun);
}

/*==========================================================
**
**	NCR chip clock divisor table.
**	Divisors are multiplied by 10,000,000 in order to make 
**	calculations more simple.
**
**==========================================================
*/

#define _5M 5000000
static u_long div_10M[] =
	{2*_5M, 3*_5M, 4*_5M, 6*_5M, 8*_5M, 12*_5M, 16*_5M};


/*===============================================================
**
**	Prepare io register values used by ncr_init() according 
**	to selected and supported features.
**
**	NCR chips allow burst lengths of 2, 4, 8, 16, 32, 64, 128 
**	transfers. 32,64,128 are only supported by 875 and 895 chips.
**	We use log base 2 (burst length) as internal code, with 
**	value 0 meaning "burst disabled".
**
**===============================================================
*/

/*
 *	Burst length from burst code.
 */
#define burst_length(bc) (!(bc))? 0 : 1 << (bc)

/*
 *	Burst code from io register bits.
 */
#define burst_code(dmode, ctest4, ctest5) \
	(ctest4) & 0x80? 0 : (((dmode) & 0xc0) >> 6) + ((ctest5) & 0x04) + 1

/*
 *	Set initial io register bits from burst code.
 */
static inline void ncr_init_burst(ncb_p np, u_char bc)
{
	np->rv_ctest4	&= ~0x80;
	np->rv_dmode	&= ~(0x3 << 6);
	np->rv_ctest5	&= ~0x4;

	if (!bc) {
		np->rv_ctest4	|= 0x80;
	}
	else {
		--bc;
		np->rv_dmode	|= ((bc & 0x3) << 6);
		np->rv_ctest5	|= (bc & 0x4);
	}
}

#ifdef SCSI_NCR_NVRAM_SUPPORT

/*
**	Get target set-up from Symbios format NVRAM.
*/

static void __init 
ncr_Symbios_setup_target(ncb_p np, int target, Symbios_nvram *nvram)
{
	tcb_p tp = &np->target[target];
	Symbios_target *tn = &nvram->target[target];

	tp->usrsync = tn->sync_period ? (tn->sync_period + 3) / 4 : 255;
	tp->usrwide = tn->bus_width == 0x10 ? 1 : 0;
	tp->usrtags =
		(tn->flags & SYMBIOS_QUEUE_TAGS_ENABLED)? MAX_TAGS : 0;

	if (!(tn->flags & SYMBIOS_DISCONNECT_ENABLE))
		tp->usrflag |= UF_NODISC;
	if (!(tn->flags & SYMBIOS_SCAN_AT_BOOT_TIME))
		tp->usrflag |= UF_NOSCAN;
}

/*
**	Get target set-up from Tekram format NVRAM.
*/

static void __init 
ncr_Tekram_setup_target(ncb_p np, int target, Tekram_nvram *nvram)
{
	tcb_p tp = &np->target[target];
	struct Tekram_target *tn = &nvram->target[target];
	int i;

	if (tn->flags & TEKRAM_SYNC_NEGO) {
		i = tn->sync_index & 0xf;
		tp->usrsync = Tekram_sync[i];
	}

	tp->usrwide = (tn->flags & TEKRAM_WIDE_NEGO) ? 1 : 0;

	if (tn->flags & TEKRAM_TAGGED_COMMANDS) {
		tp->usrtags = 2 << nvram->max_tags_index;
	}

	if (!(tn->flags & TEKRAM_DISCONNECT_ENABLE))
		tp->usrflag = UF_NODISC;
 
	/* If any device does not support parity, we will not use this option */
	if (!(tn->flags & TEKRAM_PARITY_CHECK))
		np->rv_scntl0  &= ~0x0a; /* SCSI parity checking disabled */
}
#endif /* SCSI_NCR_NVRAM_SUPPORT */

static int __init ncr_prepare_setting(ncb_p np, ncr_nvram *nvram)
{
	u_char	burst_max;
	u_long	period;
	int i;

	/*
	**	Save assumed BIOS setting
	*/

	np->sv_scntl0	= INB(nc_scntl0) & 0x0a;
	np->sv_scntl3	= INB(nc_scntl3) & 0x07;
	np->sv_dmode	= INB(nc_dmode)  & 0xce;
	np->sv_dcntl	= INB(nc_dcntl)  & 0xa8;
	np->sv_ctest3	= INB(nc_ctest3) & 0x01;
	np->sv_ctest4	= INB(nc_ctest4) & 0x80;
	np->sv_ctest5	= INB(nc_ctest5) & 0x24;
	np->sv_gpcntl	= INB(nc_gpcntl);
	np->sv_stest2	= INB(nc_stest2) & 0x20;
	np->sv_stest4	= INB(nc_stest4);

	/*
	**	Wide ?
	*/

	np->maxwide	= (np->features & FE_WIDE)? 1 : 0;

 	/*
	 *  Guess the frequency of the chip's clock.
	 */
	if	(np->features & (FE_ULTRA3 | FE_ULTRA2))
		np->clock_khz = 160000;
	else if	(np->features & FE_ULTRA)
		np->clock_khz = 80000;
	else
		np->clock_khz = 40000;

	/*
	 *  Get the clock multiplier factor.
 	 */
	if	(np->features & FE_QUAD)
		np->multiplier	= 4;
	else if	(np->features & FE_DBLR)
		np->multiplier	= 2;
	else
		np->multiplier	= 1;

	/*
	 *  Measure SCSI clock frequency for chips 
	 *  it may vary from assumed one.
	 */
	if (np->features & FE_VARCLK)
		ncr_getclock(np, np->multiplier);

	/*
	 * Divisor to be used for async (timer pre-scaler).
	 */
	i = np->clock_divn - 1;
	while (--i >= 0) {
		if (10ul * SCSI_NCR_MIN_ASYNC * np->clock_khz > div_10M[i]) {
			++i;
			break;
		}
	}
	np->rv_scntl3 = i+1;

	/*
	 * Minimum synchronous period factor supported by the chip.
	 * Btw, 'period' is in tenths of nanoseconds.
	 */

	period = (4 * div_10M[0] + np->clock_khz - 1) / np->clock_khz;
	if	(period <= 250)		np->minsync = 10;
	else if	(period <= 303)		np->minsync = 11;
	else if	(period <= 500)		np->minsync = 12;
	else				np->minsync = (period + 40 - 1) / 40;

	/*
	 * Check against chip SCSI standard support (SCSI-2,ULTRA,ULTRA2).
	 */

	if	(np->minsync < 25 && !(np->features & (FE_ULTRA|FE_ULTRA2)))
		np->minsync = 25;
	else if	(np->minsync < 12 && !(np->features & FE_ULTRA2))
		np->minsync = 12;

	/*
	 * Maximum synchronous period factor supported by the chip.
	 */

	period = (11 * div_10M[np->clock_divn - 1]) / (4 * np->clock_khz);
	np->maxsync = period > 2540 ? 254 : period / 10;

	/*
	**	Prepare initial value of other IO registers
	*/
#if defined SCSI_NCR_TRUST_BIOS_SETTING
	np->rv_scntl0	= np->sv_scntl0;
	np->rv_dmode	= np->sv_dmode;
	np->rv_dcntl	= np->sv_dcntl;
	np->rv_ctest3	= np->sv_ctest3;
	np->rv_ctest4	= np->sv_ctest4;
	np->rv_ctest5	= np->sv_ctest5;
	burst_max	= burst_code(np->sv_dmode, np->sv_ctest4, np->sv_ctest5);
#else

	/*
	**	Select burst length (dwords)
	*/
	burst_max	= driver_setup.burst_max;
	if (burst_max == 255)
		burst_max = burst_code(np->sv_dmode, np->sv_ctest4, np->sv_ctest5);
	if (burst_max > 7)
		burst_max = 7;
	if (burst_max > np->maxburst)
		burst_max = np->maxburst;

	/*
	**	Select all supported special features
	*/
	if (np->features & FE_ERL)
		np->rv_dmode	|= ERL;		/* Enable Read Line */
	if (np->features & FE_BOF)
		np->rv_dmode	|= BOF;		/* Burst Opcode Fetch */
	if (np->features & FE_ERMP)
		np->rv_dmode	|= ERMP;	/* Enable Read Multiple */
	if (np->features & FE_PFEN)
		np->rv_dcntl	|= PFEN;	/* Prefetch Enable */
	if (np->features & FE_CLSE)
		np->rv_dcntl	|= CLSE;	/* Cache Line Size Enable */
	if (np->features & FE_WRIE)
		np->rv_ctest3	|= WRIE;	/* Write and Invalidate */
	if (np->features & FE_DFS)
		np->rv_ctest5	|= DFS;		/* Dma Fifo Size */

	/*
	**	Select some other
	*/
	if (driver_setup.master_parity)
		np->rv_ctest4	|= MPEE;	/* Master parity checking */
	if (driver_setup.scsi_parity)
		np->rv_scntl0	|= 0x0a;	/*  full arb., ena parity, par->ATN  */

#ifdef SCSI_NCR_NVRAM_SUPPORT
	/*
	**	Get parity checking, host ID and verbose mode from NVRAM
	**/
	if (nvram) {
		switch(nvram->type) {
		case SCSI_NCR_TEKRAM_NVRAM:
			np->myaddr = nvram->data.Tekram.host_id & 0x0f;
			break;
		case SCSI_NCR_SYMBIOS_NVRAM:
			if (!(nvram->data.Symbios.flags & SYMBIOS_PARITY_ENABLE))
				np->rv_scntl0  &= ~0x0a;
			np->myaddr = nvram->data.Symbios.host_id & 0x0f;
			if (nvram->data.Symbios.flags & SYMBIOS_VERBOSE_MSGS)
				np->verbose += 1;
			break;
		}
	}
#endif
	/*
	**  Get SCSI addr of host adapter (set by bios?).
	*/
	if (np->myaddr == 255) {
		np->myaddr = INB(nc_scid) & 0x07;
		if (!np->myaddr)
			np->myaddr = SCSI_NCR_MYADDR;
	}

#endif /* SCSI_NCR_TRUST_BIOS_SETTING */

	/*
	 *	Prepare initial io register bits for burst length
	 */
	ncr_init_burst(np, burst_max);

	/*
	**	Set SCSI BUS mode.
	**
	**	- ULTRA2 chips (895/895A/896) report the current 
	**	  BUS mode through the STEST4 IO register.
	**	- For previous generation chips (825/825A/875), 
	**	  user has to tell us how to check against HVD, 
	**	  since a 100% safe algorithm is not possible.
	*/
	np->scsi_mode = SMODE_SE;
	if	(np->features & FE_ULTRA2)
		np->scsi_mode = (np->sv_stest4 & SMODE);
	else if	(np->features & FE_DIFF) {
		switch(driver_setup.diff_support) {
		case 4:	/* Trust previous settings if present, then GPIO3 */
			if (np->sv_scntl3) {
				if (np->sv_stest2 & 0x20)
					np->scsi_mode = SMODE_HVD;
				break;
			}
		case 3:	/* SYMBIOS controllers report HVD through GPIO3 */
			if (nvram && nvram->type != SCSI_NCR_SYMBIOS_NVRAM)
				break;
			if (INB(nc_gpreg) & 0x08)
				break;
		case 2:	/* Set HVD unconditionally */
			np->scsi_mode = SMODE_HVD;
		case 1:	/* Trust previous settings for HVD */
			if (np->sv_stest2 & 0x20)
				np->scsi_mode = SMODE_HVD;
			break;
		default:/* Don't care about HVD */	
			break;
		}
	}
	if (np->scsi_mode == SMODE_HVD)
		np->rv_stest2 |= 0x20;

	/*
	**	Set LED support from SCRIPTS.
	**	Ignore this feature for boards known to use a 
	**	specific GPIO wiring and for the 895A or 896 
	**	that drive the LED directly.
	**	Also probe initial setting of GPIO0 as output.
	*/
	if ((driver_setup.led_pin ||
	     (nvram && nvram->type == SCSI_NCR_SYMBIOS_NVRAM)) &&
	    !(np->features & FE_LEDC) && !(np->sv_gpcntl & 0x01))
		np->features |= FE_LED0;

	/*
	**	Set irq mode.
	*/
	switch(driver_setup.irqm & 3) {
	case 2:
		np->rv_dcntl	|= IRQM;
		break;
	case 1:
		np->rv_dcntl	|= (np->sv_dcntl & IRQM);
		break;
	default:
		break;
	}

	/*
	**	Configure targets according to driver setup.
	**	If NVRAM present get targets setup from NVRAM.
	**	Allow to override sync, wide and NOSCAN from 
	**	boot command line.
	*/
	for (i = 0 ; i < MAX_TARGET ; i++) {
		tcb_p tp = &np->target[i];

		tp->usrsync = 255;
#ifdef SCSI_NCR_NVRAM_SUPPORT
		if (nvram) {
			switch(nvram->type) {
			case SCSI_NCR_TEKRAM_NVRAM:
				ncr_Tekram_setup_target(np, i, &nvram->data.Tekram);
				break;
			case SCSI_NCR_SYMBIOS_NVRAM:
				ncr_Symbios_setup_target(np, i, &nvram->data.Symbios);
				break;
			}
			if (driver_setup.use_nvram & 0x2)
				tp->usrsync = driver_setup.default_sync;
			if (driver_setup.use_nvram & 0x4)
				tp->usrwide = driver_setup.max_wide;
			if (driver_setup.use_nvram & 0x8)
				tp->usrflag &= ~UF_NOSCAN;
		}
		else {
#else
		if (1) {
#endif
			tp->usrsync = driver_setup.default_sync;
			tp->usrwide = driver_setup.max_wide;
			tp->usrtags = MAX_TAGS;
			if (!driver_setup.disconnection)
				np->target[i].usrflag = UF_NODISC;
		}
	}

	/*
	**	Announce all that stuff to user.
	*/

	i = nvram ? nvram->type : 0;
	printk(KERN_INFO "%s: %sID %d, Fast-%d%s%s\n", ncr_name(np),
		i  == SCSI_NCR_SYMBIOS_NVRAM ? "Symbios format NVRAM, " :
		(i == SCSI_NCR_TEKRAM_NVRAM  ? "Tekram format NVRAM, " : ""),
		np->myaddr,
		np->minsync < 12 ? 40 : (np->minsync < 25 ? 20 : 10),
		(np->rv_scntl0 & 0xa)	? ", Parity Checking"	: ", NO Parity",
		(np->rv_stest2 & 0x20)	? ", Differential"	: "");

	if (bootverbose > 1) {
		printk (KERN_INFO "%s: initial SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			ncr_name(np), np->sv_scntl3, np->sv_dmode, np->sv_dcntl,
			np->sv_ctest3, np->sv_ctest4, np->sv_ctest5);

		printk (KERN_INFO "%s: final   SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			ncr_name(np), np->rv_scntl3, np->rv_dmode, np->rv_dcntl,
			np->rv_ctest3, np->rv_ctest4, np->rv_ctest5);
	}

	if (bootverbose && np->paddr2)
		printk (KERN_INFO "%s: on-chip RAM at 0x%lx\n",
			ncr_name(np), np->paddr2);

	return 0;
}

/*
**	Host attach and initialisations.
**
**	Allocate host data and ncb structure.
**	Request IO region and remap MMIO region.
**	Do chip initialization.
**	If all is OK, install interrupt handling and
**	start the timer daemon.
*/

static int __init 
ncr_attach (Scsi_Host_Template *tpnt, int unit, ncr_device *device)
{
        struct host_data *host_data;
	ncb_p np = 0;
        struct Scsi_Host *instance = 0;
	u_long flags = 0;
	ncr_nvram *nvram = device->nvram;
	int i;

	printk(KERN_INFO "ncr53c%s-%d: rev 0x%x on pci bus %d device %d function %d "
#ifdef __sparc__
		"irq %s\n",
#else
		"irq %d\n",
#endif
		device->chip.name, unit, device->chip.revision_id,
		device->slot.bus, (device->slot.device_fn & 0xf8) >> 3,
		device->slot.device_fn & 7,
#ifdef __sparc__
		__irq_itoa(device->slot.irq));
#else
		device->slot.irq);
#endif

	/*
	**	Allocate host_data structure
	*/
        if (!(instance = scsi_register(tpnt, sizeof(*host_data))))
	        goto attach_error;
	host_data = (struct host_data *) instance->hostdata;

	/*
	**	Allocate the host control block.
	*/
	np = __m_calloc_dma(device->pdev, sizeof(struct ncb), "NCB");
	if (!np)
		goto attach_error;
	NCR_INIT_LOCK_NCB(np);
	np->pdev  = device->pdev;
	np->p_ncb = vtobus(np);
	host_data->ncb = np;

	/*
	**	Allocate the default CCB.
	*/
	np->ccb = (ccb_p) m_calloc_dma(sizeof(struct ccb), "CCB");
	if (!np->ccb)
		goto attach_error;

	/*
	**	Store input informations in the host data structure.
	*/
	strncpy(np->chip_name, device->chip.name, sizeof(np->chip_name) - 1);
	np->unit	= unit;
	np->verbose	= driver_setup.verbose;
	sprintf(np->inst_name, "ncr53c%s-%d", np->chip_name, np->unit);
	np->device_id	= device->chip.device_id;
	np->revision_id	= device->chip.revision_id;
	np->bus		= device->slot.bus;
	np->device_fn	= device->slot.device_fn;
	np->features	= device->chip.features;
	np->clock_divn	= device->chip.nr_divisor;
	np->maxoffs	= device->chip.offset_max;
	np->maxburst	= device->chip.burst_max;
	np->myaddr	= device->host_id;

	/*
	**	Allocate SCRIPTS areas.
	*/
	np->script0  = (struct script *)
			m_calloc_dma(sizeof(struct script), "SCRIPT");
	if (!np->script0)
		goto attach_error;
	np->scripth0  = (struct scripth *)
			m_calloc_dma(sizeof(struct scripth), "SCRIPTH");
	if (!np->scripth0)
		goto attach_error;

	/*
	**    Initialize timer structure
        **
        */
	init_timer(&np->timer);
	np->timer.data     = (unsigned long) np;
	np->timer.function = ncr53c8xx_timeout;

	/*
	**	Try to map the controller chip to
	**	virtual and physical memory.
	*/

	np->paddr	= device->slot.base;
	np->paddr2	= (np->features & FE_RAM)? device->slot.base_2 : 0;

#ifndef SCSI_NCR_IOMAPPED
	np->vaddr = remap_pci_mem(device->slot.base_c, (u_long) 128);
	if (!np->vaddr) {
		printk(KERN_ERR
			"%s: can't map memory mapped IO region\n",ncr_name(np));
		goto attach_error;
	}
	else
		if (bootverbose > 1)
			printk(KERN_INFO
				"%s: using memory mapped IO at virtual address 0x%lx\n", ncr_name(np), (u_long) np->vaddr);

	/*
	**	Make the controller's registers available.
	**	Now the INB INW INL OUTB OUTW OUTL macros
	**	can be used safely.
	*/

	np->reg = (struct ncr_reg*) np->vaddr;

#endif /* !defined SCSI_NCR_IOMAPPED */

	/*
	**	Try to map the controller chip into iospace.
	*/

	request_region(device->slot.io_port, 128, "ncr53c8xx");
	np->base_io = device->slot.io_port;

#ifdef SCSI_NCR_NVRAM_SUPPORT
	if (nvram) {
		switch(nvram->type) {
		case SCSI_NCR_SYMBIOS_NVRAM:
#ifdef SCSI_NCR_DEBUG_NVRAM
			ncr_display_Symbios_nvram(&nvram->data.Symbios);
#endif
			break;
		case SCSI_NCR_TEKRAM_NVRAM:
#ifdef SCSI_NCR_DEBUG_NVRAM
			ncr_display_Tekram_nvram(&nvram->data.Tekram);
#endif
			break;
		default:
			nvram = 0;
#ifdef SCSI_NCR_DEBUG_NVRAM
			printk(KERN_DEBUG "%s: NVRAM: None or invalid data.\n", ncr_name(np));
#endif
		}
	}
#endif

	/*
	**	Do chip dependent initialization.
	*/
	(void)ncr_prepare_setting(np, nvram);

	if (np->paddr2 && sizeof(struct script) > 4096) {
		np->paddr2 = 0;
		printk(KERN_WARNING "%s: script too large, NOT using on chip RAM.\n",
			ncr_name(np));
	}

	/*
	**	Fill Linux host instance structure
	*/
	instance->max_channel	= 0;
	instance->this_id       = np->myaddr;
	instance->max_id	= np->maxwide ? 16 : 8;
	instance->max_lun	= SCSI_NCR_MAX_LUN;
#ifndef SCSI_NCR_IOMAPPED
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,29)
	instance->base		= (unsigned long) np->reg;
#else
	instance->base		= (char *) np->reg;
#endif
#endif
	instance->irq		= device->slot.irq;
	instance->unique_id	= device->slot.io_port;
	instance->io_port	= device->slot.io_port;
	instance->n_io_port	= 128;
	instance->dma_channel	= 0;
	instance->cmd_per_lun	= MAX_TAGS;
	instance->can_queue	= (MAX_START-4);
	instance->select_queue_depths = ncr53c8xx_select_queue_depths;
	scsi_set_pci_device(instance, device->pdev);

#ifdef SCSI_NCR_INTEGRITY_CHECKING
	np->check_integrity	  = 0;
	instance->check_integrity = 0;

#ifdef SCSI_NCR_ENABLE_INTEGRITY_CHECK
	if ( !(driver_setup.bus_check & 0x04) ) {
		np->check_integrity	  = 1;
		instance->check_integrity = 1;
	}
#endif
#endif
	/*
	**	Patch script to physical addresses
	*/
	ncr_script_fill (&script0, &scripth0);

	np->scripth	= np->scripth0;
	np->p_scripth	= vtobus(np->scripth);

	np->p_script	= (np->paddr2) ?  np->paddr2 : vtobus(np->script0);

	ncr_script_copy_and_bind (np, (ncrcmd *) &script0, (ncrcmd *) np->script0, sizeof(struct script));
	ncr_script_copy_and_bind (np, (ncrcmd *) &scripth0, (ncrcmd *) np->scripth0, sizeof(struct scripth));
	np->ccb->p_ccb		= vtobus (np->ccb);

	/*
	**    Patch the script for LED support.
	*/

	if (np->features & FE_LED0) {
		np->script0->idle[0]  =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_OR,  0x01));
		np->script0->reselected[0] =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_AND, 0xfe));
		np->script0->start[0] =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_AND, 0xfe));
	}

	/*
	**	Look for the target control block of this nexus.
	**	For i = 0 to 3
	**		JUMP ^ IFTRUE (MASK (i, 3)), @(next_lcb)
	*/
	for (i = 0 ; i < 4 ; i++) {
		np->jump_tcb[i].l_cmd   =
				cpu_to_scr((SCR_JUMP ^ IFTRUE (MASK (i, 3))));
		np->jump_tcb[i].l_paddr =
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, bad_target));
	}

	/*
	**	Reset chip.
	*/

	OUTB (nc_istat,  SRST);
	UDELAY (100);
	OUTB (nc_istat,  0   );

	/*
	**	Now check the cache handling of the pci chipset.
	*/

	if (ncr_snooptest (np)) {
		printk (KERN_ERR "CACHE INCORRECTLY CONFIGURED.\n");
		goto attach_error;
	};

	/*
	**	Install the interrupt handler.
	*/

	if (request_irq(device->slot.irq, ncr53c8xx_intr,
			((driver_setup.irqm & 0x10) ? 0 : SA_SHIRQ) |
#if LINUX_VERSION_CODE < LinuxVersionCode(2,2,0)
			((driver_setup.irqm & 0x20) ? 0 : SA_INTERRUPT),
#else
			0,
#endif
			"ncr53c8xx", np)) {
#ifdef __sparc__
		printk(KERN_ERR "%s: request irq %s failure\n",
			ncr_name(np), __irq_itoa(device->slot.irq));
#else
		printk(KERN_ERR "%s: request irq %d failure\n",
			ncr_name(np), device->slot.irq);
#endif
		goto attach_error;
	}

	np->irq = device->slot.irq;

	/*
	**	Initialize the fixed part of the default ccb.
	*/
	ncr_init_ccb(np, np->ccb);

	/*
	**	After SCSI devices have been opened, we cannot
	**	reset the bus safely, so we do it here.
	**	Interrupt handler does the real work.
	**	Process the reset exception,
	**	if interrupts are not enabled yet.
	**	Then enable disconnects.
	*/
	NCR_LOCK_NCB(np, flags);
	if (ncr_reset_scsi_bus(np, 0, driver_setup.settle_delay) != 0) {
		printk(KERN_ERR "%s: FATAL ERROR: CHECK SCSI BUS - CABLES, TERMINATION, DEVICE POWER etc.!\n", ncr_name(np));

		NCR_UNLOCK_NCB(np, flags);
		goto attach_error;
	}
	ncr_exception (np);

	np->disc = 1;

	/*
	**	The middle-level SCSI driver does not
	**	wait for devices to settle.
	**	Wait synchronously if more than 2 seconds.
	*/
	if (driver_setup.settle_delay > 2) {
		printk(KERN_INFO "%s: waiting %d seconds for scsi devices to settle...\n",
			ncr_name(np), driver_setup.settle_delay);
		MDELAY (1000 * driver_setup.settle_delay);
	}

	/*
	**	Now let the generic SCSI driver
	**	look for the SCSI devices on the bus ..
	*/

	/*
	**	start the timeout daemon
	*/
	np->lasttime=0;
	ncr_timeout (np);

	/*
	**  use SIMPLE TAG messages by default
	*/
#ifdef SCSI_NCR_ALWAYS_SIMPLE_TAG
	np->order = M_SIMPLE_TAG;
#endif

	/*
	**  Done.
	*/
        if (!the_template) {
        	the_template = instance->hostt;
        	first_host = instance;
	}

	NCR_UNLOCK_NCB(np, flags);

	return 0;

attach_error:
	if (!instance) return -1;
	printk(KERN_INFO "%s: detaching...\n", ncr_name(np));
	if (!np)
		goto unregister;
#ifndef SCSI_NCR_IOMAPPED
	if (np->vaddr) {
#ifdef DEBUG_NCR53C8XX
		printk(KERN_DEBUG "%s: releasing memory mapped IO region %lx[%d]\n", ncr_name(np), (u_long) np->vaddr, 128);
#endif
		unmap_pci_mem((vm_offset_t) np->vaddr, (u_long) 128);
	}
#endif /* !SCSI_NCR_IOMAPPED */
	if (np->base_io) {
#ifdef DEBUG_NCR53C8XX
		printk(KERN_DEBUG "%s: releasing IO region %x[%d]\n", ncr_name(np), np->base_io, 128);
#endif
		release_region(np->base_io, 128);
	}
	if (np->irq) {
#ifdef DEBUG_NCR53C8XX
#ifdef __sparc__
	printk(KERN_INFO "%s: freeing irq %s\n", ncr_name(np),
	       __irq_itoa(np->irq));
#else
	printk(KERN_INFO "%s: freeing irq %d\n", ncr_name(np), np->irq);
#endif
#endif
		free_irq(np->irq, np);
	}
	if (np->scripth0)
		m_free_dma(np->scripth0, sizeof(struct scripth), "SCRIPTH");
	if (np->script0)
		m_free_dma(np->script0, sizeof(struct script), "SCRIPT");
	if (np->ccb)
		m_free_dma(np->ccb, sizeof(struct ccb), "CCB");
	m_free_dma(np, sizeof(struct ncb), "NCB");

unregister:
	scsi_unregister(instance);

        return -1;
 }


/*==========================================================
**
**
**	Done SCSI commands list management.
**
**	We donnot enter the scsi_done() callback immediately 
**	after a command has been seen as completed but we 
**	insert it into a list which is flushed outside any kind 
**	of driver critical section.
**	This allows to do minimal stuff under interrupt and 
**	inside critical sections and to also avoid locking up 
**	on recursive calls to driver entry points under SMP.
**	In fact, the only kernel point which is entered by the 
**	driver with a driver lock set is kmalloc(GFP_ATOMIC) 
**	that shall not reenter the driver under any circumstances,
**	AFAIK.
**
**==========================================================
*/
static inline void ncr_queue_done_cmd(ncb_p np, Scsi_Cmnd *cmd)
{
	unmap_scsi_data(np, cmd);
	cmd->host_scribble = (char *) np->done_list;
	np->done_list = cmd;
}

static inline void ncr_flush_done_cmds(Scsi_Cmnd *lcmd)
{
	Scsi_Cmnd *cmd;

	while (lcmd) {
		cmd = lcmd;
		lcmd = (Scsi_Cmnd *) cmd->host_scribble;
		cmd->scsi_done(cmd);
	}
}

/*==========================================================
**
**
**	Prepare the next negotiation message for integrity check,
**	if needed.
**
**	Fill in the part of message buffer that contains the 
**	negotiation and the nego_status field of the CCB.
**	Returns the size of the message in bytes.
**
**
**==========================================================
*/

#ifdef SCSI_NCR_INTEGRITY_CHECKING
static int ncr_ic_nego(ncb_p np, ccb_p cp, Scsi_Cmnd *cmd, u_char *msgptr)
{
	tcb_p tp = &np->target[cp->target];
	int msglen = 0;
	int nego = 0;
	u_char no_increase;

	if (tp->inq_done) {

		if (!tp->ic_maximums_set) {
			tp->ic_maximums_set = 1;

			/* check target and host adapter capabilities */
			if ( (tp->inq_byte7 & INQ7_WIDE16) && 
					np->maxwide && tp->usrwide ) 
				tp->ic_max_width = 1;
			else
				tp->ic_max_width = 0;

			if ((tp->inq_byte7 & INQ7_SYNC) && tp->maxoffs) {
				tp->ic_min_sync   = (tp->minsync < np->minsync) ?
							np->minsync : tp->minsync;
			}
			else 
				tp->ic_min_sync   = 255;
			
			tp->period   = 1;
			tp->widedone = 1;
		}

		if (DEBUG_FLAGS & DEBUG_IC) {
			printk("%s: cmd->ic_nego %d, 1st byte 0x%2X\n",
				ncr_name(np), cmd->ic_nego, cmd->cmnd[0]);
		}

		/* First command from integrity check routine will request
		 * a PPR message.  Disable.
		 */
		if ((cmd->ic_nego & NS_PPR) == NS_PPR)
			cmd->ic_nego &= ~NS_PPR;
		/* Previous command recorded a parity or an initiator
		 * detected error condition. Force bus to narrow for this
		 * target. Clear flag. Negotation on request sense.
		 * Note: kernel forces 2 bus resets :o( but clears itself out.
		 * Minor bug? in scsi_obsolete.c (ugly)
		 */
		if (np->check_integ_par) {
			printk("%s: Parity Error. Target set to narrow.\n",
				ncr_name(np));
			tp->ic_max_width = 0;
			tp->widedone = tp->period = 0;
		}
		
		/* In case of a bus reset, ncr_negotiate will reset 
                 * the flags tp->widedone and tp->period to 0, forcing
		 * a new negotiation. 
		 */
		no_increase = 0;
		if (tp->widedone == 0) {
			cmd->ic_nego = NS_WIDE;
			tp->widedone = 1;
			no_increase = 1;
		}
		else if (tp->period == 0) {
			cmd->ic_nego = NS_SYNC;
			tp->period = 1;
			no_increase = 1;
		}
			
		switch (cmd->ic_nego) {
		case NS_WIDE:
			/*
			**	negotiate wide transfers ?
			**	Do NOT negotiate if device only supports
			**	narrow.	
			*/
			if (tp->ic_max_width | np->check_integ_par) {
				nego = NS_WIDE;

				msgptr[msglen++] = M_EXTENDED;
				msgptr[msglen++] = 2;
				msgptr[msglen++] = M_X_WIDE_REQ;
				msgptr[msglen++] = cmd->ic_nego_width & tp->ic_max_width;
			}
			else
				cmd->ic_nego_width &= tp->ic_max_width;
              
			break;

		case NS_SYNC:
			/*
			**	negotiate synchronous transfers?
			**	Target must support sync transfers.
			**
			**	If period becomes longer than max, reset to async
			*/

			if (tp->inq_byte7 & INQ7_SYNC) {

				nego = NS_SYNC;

				msgptr[msglen++] = M_EXTENDED;
				msgptr[msglen++] = 3;
				msgptr[msglen++] = M_X_SYNC_REQ;

				switch (cmd->ic_nego_sync) {
				case 2: /* increase the period */
					if (!no_increase) {
					  if (tp->ic_min_sync <= 0x0A)
					      tp->ic_min_sync = 0x0C;
					  else if (tp->ic_min_sync <= 0x0C)
					      tp->ic_min_sync = 0x19;
					  else if (tp->ic_min_sync <= 0x19)
					      tp->ic_min_sync *= 2;
					  else {
						tp->ic_min_sync = 255;
						cmd->ic_nego_sync = 0;
						tp->maxoffs = 0;
					   }
					}
					msgptr[msglen++] = tp->maxoffs?tp->ic_min_sync:0;
					msgptr[msglen++] = tp->maxoffs;
					break;

				case 1: /* nego. to maximum */
					msgptr[msglen++] = tp->maxoffs?tp->ic_min_sync:0;
					msgptr[msglen++] = tp->maxoffs;
					break;

				case 0:	/* nego to async */
				default:
					msgptr[msglen++] = 0;
					msgptr[msglen++] = 0;
					break;
				};
			}
			else
				cmd->ic_nego_sync = 0;
			break;

		case NS_NOCHANGE:
		default:
			break;
		};
	};

	cp->nego_status = nego;
	np->check_integ_par = 0;

	if (nego) {
		tp->nego_cp = cp;
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			ncr_print_msg(cp, nego == NS_WIDE ?
			  "wide/narrow msgout": "sync/async msgout", msgptr);
		};
	};

	return msglen;
}
#endif /* SCSI_NCR_INTEGRITY_CHECKING */

/*==========================================================
**
**
**	Prepare the next negotiation message if needed.
**
**	Fill in the part of message buffer that contains the 
**	negotiation and the nego_status field of the CCB.
**	Returns the size of the message in bytes.
**
**
**==========================================================
*/


static int ncr_prepare_nego(ncb_p np, ccb_p cp, u_char *msgptr)
{
	tcb_p tp = &np->target[cp->target];
	int msglen = 0;
	int nego = 0;

	if (tp->inq_done) {

		/*
		**	negotiate wide transfers ?
		*/

		if (!tp->widedone) {
			if (tp->inq_byte7 & INQ7_WIDE16) {
				nego = NS_WIDE;
#ifdef SCSI_NCR_INTEGRITY_CHECKING
				if (tp->ic_done)
		       			 tp->usrwide &= tp->ic_max_width;
#endif
			} else
				tp->widedone=1;

		};

		/*
		**	negotiate synchronous transfers?
		*/

		if (!nego && !tp->period) {
			if (tp->inq_byte7 & INQ7_SYNC) {
				nego = NS_SYNC;
#ifdef SCSI_NCR_INTEGRITY_CHECKING
				if ((tp->ic_done) &&
				 	      (tp->minsync < tp->ic_min_sync))
		       			 tp->minsync = tp->ic_min_sync;
#endif
			} else {
				tp->period  =0xffff;
				PRINT_TARGET(np, cp->target);
				printk ("target did not report SYNC.\n");
			};
		};
	};

	switch (nego) {
	case NS_SYNC:
		msgptr[msglen++] = M_EXTENDED;
		msgptr[msglen++] = 3;
		msgptr[msglen++] = M_X_SYNC_REQ;
		msgptr[msglen++] = tp->maxoffs ? tp->minsync : 0;
		msgptr[msglen++] = tp->maxoffs;
		break;
	case NS_WIDE:
		msgptr[msglen++] = M_EXTENDED;
		msgptr[msglen++] = 2;
		msgptr[msglen++] = M_X_WIDE_REQ;
		msgptr[msglen++] = tp->usrwide;
		break;
	};

	cp->nego_status = nego;

	if (nego) {
		tp->nego_cp = cp;
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			ncr_print_msg(cp, nego == NS_WIDE ?
					  "wide msgout":"sync_msgout", msgptr);
		};
	};

	return msglen;
}



/*==========================================================
**
**
**	Start execution of a SCSI command.
**	This is called from the generic SCSI driver.
**
**
**==========================================================
*/
static int ncr_queue_command (ncb_p np, Scsi_Cmnd *cmd)
{
/*	Scsi_Device        *device    = cmd->device; */
	tcb_p tp                      = &np->target[cmd->target];
	lcb_p lp		      = tp->lp[cmd->lun];
	ccb_p cp;

	int	segments;
	u_char	idmsg, *msgptr;
	u_int  msglen;
	int	direction;
	u_int32	lastp, goalp;

	/*---------------------------------------------
	**
	**      Some shortcuts ...
	**
	**---------------------------------------------
	*/
	if ((cmd->target == np->myaddr	  ) ||
		(cmd->target >= MAX_TARGET) ||
		(cmd->lun    >= MAX_LUN   )) {
		return(DID_BAD_TARGET);
        }

	/*---------------------------------------------
	**
	**	Complete the 1st TEST UNIT READY command
	**	with error condition if the device is 
	**	flagged NOSCAN, in order to speed up 
	**	the boot.
	**
	**---------------------------------------------
	*/
	if ((cmd->cmnd[0] == 0 || cmd->cmnd[0] == 0x12) && 
	    (tp->usrflag & UF_NOSCAN)) {
		tp->usrflag &= ~UF_NOSCAN;
		return DID_BAD_TARGET;
	}

	if (DEBUG_FLAGS & DEBUG_TINY) {
		PRINT_ADDR(cmd);
		printk ("CMD=%x ", cmd->cmnd[0]);
	}

	/*---------------------------------------------------
	**
	**	Assign a ccb / bind cmd.
	**	If resetting, shorten settle_time if necessary
	**	in order to avoid spurious timeouts.
	**	If resetting or no free ccb,
	**	insert cmd into the waiting list.
	**
	**----------------------------------------------------
	*/
	if (np->settle_time && cmd->timeout_per_command >= HZ) {
		u_long tlimit = ktime_get(cmd->timeout_per_command - HZ);
		if (ktime_dif(np->settle_time, tlimit) > 0)
			np->settle_time = tlimit;
	}

        if (np->settle_time || !(cp=ncr_get_ccb (np, cmd->target, cmd->lun))) {
		insert_into_waiting_list(np, cmd);
		return(DID_OK);
	}
	cp->cmd = cmd;

	/*---------------------------------------------------
	**
	**	Enable tagged queue if asked by scsi ioctl
	**
	**----------------------------------------------------
	*/
#if 0	/* This stuff was only useful for linux-1.2.13 */
	if (lp && !lp->numtags && cmd->device && cmd->device->tagged_queue) {
		lp->numtags = tp->usrtags;
		ncr_setup_tags (np, cmd->target, cmd->lun);
	}
#endif

	/*----------------------------------------------------
	**
	**	Build the identify / tag / sdtr message
	**
	**----------------------------------------------------
	*/

	idmsg = M_IDENTIFY | cmd->lun;

	if (cp ->tag != NO_TAG ||
		(cp != np->ccb && np->disc && !(tp->usrflag & UF_NODISC)))
		idmsg |= 0x40;

	msgptr = cp->scsi_smsg;
	msglen = 0;
	msgptr[msglen++] = idmsg;

	if (cp->tag != NO_TAG) {
		char order = np->order;

		/*
		**	Force ordered tag if necessary to avoid timeouts 
		**	and to preserve interactivity.
		*/
		if (lp && ktime_exp(lp->tags_stime)) {
			if (lp->tags_smap) {
				order = M_ORDERED_TAG;
				if ((DEBUG_FLAGS & DEBUG_TAGS)||bootverbose>2){ 
					PRINT_ADDR(cmd);
					printk("ordered tag forced.\n");
				}
			}
			lp->tags_stime = ktime_get(3*HZ);
			lp->tags_smap = lp->tags_umap;
		}

		if (order == 0) {
			/*
			**	Ordered write ops, unordered read ops.
			*/
			switch (cmd->cmnd[0]) {
			case 0x08:  /* READ_SMALL (6) */
			case 0x28:  /* READ_BIG  (10) */
			case 0xa8:  /* READ_HUGE (12) */
				order = M_SIMPLE_TAG;
				break;
			default:
				order = M_ORDERED_TAG;
			}
		}
		msgptr[msglen++] = order;
		/*
		**	Actual tags are numbered 1,3,5,..2*MAXTAGS+1,
		**	since we may have to deal with devices that have 
		**	problems with #TAG 0 or too great #TAG numbers.
		*/
		msgptr[msglen++] = (cp->tag << 1) + 1;
	}

	/*----------------------------------------------------
	**
	**	Build the data descriptors
	**
	**----------------------------------------------------
	*/

	direction = scsi_data_direction(cmd);
	if (direction != SCSI_DATA_NONE) {
		segments = ncr_scatter (np, cp, cp->cmd);
		if (segments < 0) {
			ncr_free_ccb(np, cp);
			return(DID_ERROR);
		}
	}
	else {
		cp->data_len = 0;
		segments = 0;
	}

	/*---------------------------------------------------
	**
	**	negotiation required?
	**
	**	(nego_status is filled by ncr_prepare_nego())
	**
	**---------------------------------------------------
	*/

	cp->nego_status = 0;

#ifdef SCSI_NCR_INTEGRITY_CHECKING
	if ((np->check_integrity && tp->ic_done) || !np->check_integrity) {
		 if ((!tp->widedone || !tp->period) && !tp->nego_cp && lp) {
			msglen += ncr_prepare_nego (np, cp, msgptr + msglen);
		 }
	}
	else if (np->check_integrity && (cmd->ic_in_progress)) { 
		msglen += ncr_ic_nego (np, cp, cmd, msgptr + msglen);
        }
	else if (np->check_integrity && cmd->ic_complete) {
                /*
                 * Midlayer signal to the driver that all of the scsi commands
                 * for the integrity check have completed. Save the negotiated
                 * parameters (extracted from sval and wval). 
                 */ 

		{
			u_char idiv;
			idiv = (tp->wval>>4) & 0x07;
			if ((tp->sval&0x1f) && idiv )
				tp->period = (((tp->sval>>5)+4)  
						*div_10M[idiv-1])/np->clock_khz;
			else
				tp->period = 0xffff;
		}
		/*
		 * tp->period contains 10 times the transfer period, 
		 * which itself is 4 * the requested negotiation rate.
		 */
		if	(tp->period <= 250)	tp->ic_min_sync = 10;
		else if	(tp->period <= 303)	tp->ic_min_sync = 11;
		else if	(tp->period <= 500)	tp->ic_min_sync = 12;
		else				
				tp->ic_min_sync = (tp->period + 40 - 1) / 40;


		/*
                 * Negotiation for this target it complete.
                 */
		tp->ic_max_width =  (tp->wval & EWS) ? 1: 0;
		tp->ic_done = 1;
		tp->widedone = 1;

		printk("%s: Integrity Check Complete: \n", ncr_name(np)); 

		printk("%s: %s %s SCSI", ncr_name(np), 
				(tp->sval&0x1f)?"SYNC":"ASYNC",
				tp->ic_max_width?"WIDE":"NARROW");

		if (tp->sval&0x1f) {
                        u_long mbs = 10000 * (tp->ic_max_width + 1);

                        printk(" %d.%d  MB/s", 
                                (int) (mbs / tp->period), (int) (mbs % tp->period));

			printk(" (%d ns, %d offset)\n", 
				  tp->period/10, tp->sval&0x1f);
		}
		else
                        printk(" %d MB/s. \n ", (tp->ic_max_width+1)*5);
        }
#else
	if ((!tp->widedone || !tp->period) && !tp->nego_cp && lp) {
		msglen += ncr_prepare_nego (np, cp, msgptr + msglen);
	}
#endif /* SCSI_NCR_INTEGRITY_CHECKING */

	/*----------------------------------------------------
	**
	**	Determine xfer direction.
	**
	**----------------------------------------------------
	*/
	if (!cp->data_len)
		direction = SCSI_DATA_NONE;

	/*
	**	If data direction is UNKNOWN, speculate DATA_READ 
	**	but prepare alternate pointers for WRITE in case 
	**	of our speculation will be just wrong.
	**	SCRIPTS will swap values if needed.
	*/
	switch(direction) {
	case SCSI_DATA_UNKNOWN:
	case SCSI_DATA_WRITE:
		goalp = NCB_SCRIPT_PHYS (np, data_out2) + 8;
		if (segments <= MAX_SCATTERL)
			lastp = goalp - 8 - (segments * 16);
		else {
			lastp = NCB_SCRIPTH_PHYS (np, hdata_out2);
			lastp -= (segments - MAX_SCATTERL) * 16;
		}
		if (direction != SCSI_DATA_UNKNOWN)
			break;
		cp->phys.header.wgoalp	= cpu_to_scr(goalp);
		cp->phys.header.wlastp	= cpu_to_scr(lastp);
		/* fall through */
	case SCSI_DATA_READ:
		goalp = NCB_SCRIPT_PHYS (np, data_in2) + 8;
		if (segments <= MAX_SCATTERL)
			lastp = goalp - 8 - (segments * 16);
		else {
			lastp = NCB_SCRIPTH_PHYS (np, hdata_in2);
			lastp -= (segments - MAX_SCATTERL) * 16;
		}
		break;
	default:
	case SCSI_DATA_NONE:
		lastp = goalp = NCB_SCRIPT_PHYS (np, no_data);
		break;
	}

	/*
	**	Set all pointers values needed by SCRIPTS.
	**	If direction is unknown, start at data_io.
	*/
	cp->phys.header.lastp = cpu_to_scr(lastp);
	cp->phys.header.goalp = cpu_to_scr(goalp);

	if (direction == SCSI_DATA_UNKNOWN)
		cp->phys.header.savep = 
			cpu_to_scr(NCB_SCRIPTH_PHYS (np, data_io));
	else
		cp->phys.header.savep= cpu_to_scr(lastp);

	/*
	**	Save the initial data pointer in order to be able 
	**	to redo the command.
	*/
	cp->startp = cp->phys.header.savep;

	/*----------------------------------------------------
	**
	**	fill in ccb
	**
	**----------------------------------------------------
	**
	**
	**	physical -> virtual backlink
	**	Generic SCSI command
	*/

	/*
	**	Startqueue
	*/
	cp->start.schedule.l_paddr   = cpu_to_scr(NCB_SCRIPT_PHYS (np, select));
	cp->restart.schedule.l_paddr = cpu_to_scr(NCB_SCRIPT_PHYS (np, resel_dsa));
	/*
	**	select
	*/
	cp->phys.select.sel_id		= cmd->target;
	cp->phys.select.sel_scntl3	= tp->wval;
	cp->phys.select.sel_sxfer	= tp->sval;
	/*
	**	message
	*/
	cp->phys.smsg.addr		= cpu_to_scr(CCB_PHYS (cp, scsi_smsg));
	cp->phys.smsg.size		= cpu_to_scr(msglen);

	/*
	**	command
	*/
	memcpy(cp->cdb_buf, cmd->cmnd, MIN(cmd->cmd_len, sizeof(cp->cdb_buf)));
	cp->phys.cmd.addr		= cpu_to_scr(CCB_PHYS (cp, cdb_buf[0]));
	cp->phys.cmd.size		= cpu_to_scr(cmd->cmd_len);

	/*
	**	status
	*/
	cp->actualquirks		= tp->quirks;
	cp->host_status			= cp->nego_status ? HS_NEGOTIATE : HS_BUSY;
	cp->scsi_status			= S_ILLEGAL;
	cp->parity_status		= 0;

	cp->xerr_status			= XE_OK;
#if 0
	cp->sync_status			= tp->sval;
	cp->wide_status			= tp->wval;
#endif

	/*----------------------------------------------------
	**
	**	Critical region: start this job.
	**
	**----------------------------------------------------
	*/

	/*
	**	activate this job.
	*/
	cp->magic		= CCB_MAGIC;

	/*
	**	insert next CCBs into start queue.
	**	2 max at a time is enough to flush the CCB wait queue.
	*/
	cp->auto_sense = 0;
	if (lp)
		ncr_start_next_ccb(np, lp, 2);
	else
		ncr_put_start_queue(np, cp);

	/*
	**	Command is successfully queued.
	*/

	return(DID_OK);
}


/*==========================================================
**
**
**	Insert a CCB into the start queue and wake up the 
**	SCRIPTS processor.
**
**
**==========================================================
*/

static void ncr_start_next_ccb(ncb_p np, lcb_p lp, int maxn)
{
	XPT_QUEHEAD *qp;
	ccb_p cp;

	if (lp->held_ccb)
		return;

	while (maxn-- && lp->queuedccbs < lp->queuedepth) {
		qp = xpt_remque_head(&lp->wait_ccbq);
		if (!qp)
			break;
		++lp->queuedccbs;
		cp = xpt_que_entry(qp, struct ccb, link_ccbq);
		xpt_insque_tail(qp, &lp->busy_ccbq);
		lp->jump_ccb[cp->tag == NO_TAG ? 0 : cp->tag] =
			cpu_to_scr(CCB_PHYS (cp, restart));
		ncr_put_start_queue(np, cp);
	}
}

static void ncr_put_start_queue(ncb_p np, ccb_p cp)
{
	u_short	qidx;

	/*
	**	insert into start queue.
	*/
	if (!np->squeueput) np->squeueput = 1;
	qidx = np->squeueput + 2;
	if (qidx >= MAX_START + MAX_START) qidx = 1;

	np->scripth->tryloop [qidx] = cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));
	MEMORY_BARRIER();
	np->scripth->tryloop [np->squeueput] = cpu_to_scr(CCB_PHYS (cp, start));

	np->squeueput = qidx;
	++np->queuedccbs;
	cp->queued = 1;

	if (DEBUG_FLAGS & DEBUG_QUEUE)
		printk ("%s: queuepos=%d.\n", ncr_name (np), np->squeueput);

	/*
	**	Script processor may be waiting for reselect.
	**	Wake it up.
	*/
	MEMORY_BARRIER();
	OUTB (nc_istat, SIGP);
}


/*==========================================================
**
**
**	Start reset process.
**	If reset in progress do nothing.
**	The interrupt handler will reinitialize the chip.
**	The timeout handler will wait for settle_time before 
**	clearing it and so resuming command processing.
**
**
**==========================================================
*/
static void ncr_start_reset(ncb_p np)
{
	if (!np->settle_time) {
		(void) ncr_reset_scsi_bus(np, 1, driver_setup.settle_delay);
 	}
 }
 
static int ncr_reset_scsi_bus(ncb_p np, int enab_int, int settle_delay)
{
	u_int32 term;
	int retv = 0;

	np->settle_time	= ktime_get(settle_delay * HZ);

	if (bootverbose > 1)
		printk("%s: resetting, "
			"command processing suspended for %d seconds\n",
			ncr_name(np), settle_delay);

	OUTB (nc_istat, SRST);
	UDELAY (100);
	OUTB (nc_istat, 0);
	UDELAY (2000);	/* The 895 needs time for the bus mode to settle */
	if (enab_int)
		OUTW (nc_sien, RST);
	/*
	**	Enable Tolerant, reset IRQD if present and 
	**	properly set IRQ mode, prior to resetting the bus.
	*/
	OUTB (nc_stest3, TE);
	OUTB (nc_dcntl, (np->rv_dcntl & IRQM));
	OUTB (nc_scntl1, CRST);
	UDELAY (200);

	if (!driver_setup.bus_check)
		goto out;
	/*
	**	Check for no terminators or SCSI bus shorts to ground.
	**	Read SCSI data bus, data parity bits and control signals.
	**	We are expecting RESET to be TRUE and other signals to be 
	**	FALSE.
	*/

	term =	INB(nc_sstat0);
	term =	((term & 2) << 7) + ((term & 1) << 17);	/* rst sdp0 */
	term |= ((INB(nc_sstat2) & 0x01) << 26) |	/* sdp1     */
		((INW(nc_sbdl) & 0xff)   << 9)  |	/* d7-0     */
		((INW(nc_sbdl) & 0xff00) << 10) |	/* d15-8    */
		INB(nc_sbcl);	/* req ack bsy sel atn msg cd io    */

	if (!(np->features & FE_WIDE))
		term &= 0x3ffff;

	if (term != (2<<7)) {
		printk("%s: suspicious SCSI data while resetting the BUS.\n",
			ncr_name(np));
		printk("%s: %sdp0,d7-0,rst,req,ack,bsy,sel,atn,msg,c/d,i/o = "
			"0x%lx, expecting 0x%lx\n",
			ncr_name(np),
			(np->features & FE_WIDE) ? "dp1,d15-8," : "",
			(u_long)term, (u_long)(2<<7));
		if (driver_setup.bus_check == 1)
			retv = 1;
	}
out:
	OUTB (nc_scntl1, 0);
	return retv;
}

/*==========================================================
**
**
**	Reset the SCSI BUS.
**	This is called from the generic SCSI driver.
**
**
**==========================================================
*/
static int ncr_reset_bus (ncb_p np, Scsi_Cmnd *cmd, int sync_reset)
{
/*	Scsi_Device        *device    = cmd->device; */
	ccb_p cp;
	int found;

/*
 * Return immediately if reset is in progress.
 */
	if (np->settle_time) {
		return SCSI_RESET_PUNT;
	}
/*
 * Start the reset process.
 * The script processor is then assumed to be stopped.
 * Commands will now be queued in the waiting list until a settle 
 * delay of 2 seconds will be completed.
 */
	ncr_start_reset(np);
/*
 * First, look in the wakeup list
 */
	for (found=0, cp=np->ccb; cp; cp=cp->link_ccb) {
		/*
		**	look for the ccb of this command.
		*/
		if (cp->host_status == HS_IDLE) continue;
		if (cp->cmd == cmd) {
			found = 1;
			break;
		}
	}
/*
 * Then, look in the waiting list
 */
	if (!found && retrieve_from_waiting_list(0, np, cmd))
		found = 1;
/*
 * Wake-up all awaiting commands with DID_RESET.
 */
	reset_waiting_list(np);
/*
 * Wake-up all pending commands with HS_RESET -> DID_RESET.
 */
	ncr_wakeup(np, HS_RESET);
/*
 * If the involved command was not in a driver queue, and the 
 * scsi driver told us reset is synchronous, and the command is not 
 * currently in the waiting list, complete it with DID_RESET status,
 * in order to keep it alive.
 */
	if (!found && sync_reset && !retrieve_from_waiting_list(0, np, cmd)) {
		cmd->result = ScsiResult(DID_RESET, 0);
		ncr_queue_done_cmd(np, cmd);
	}

	return SCSI_RESET_SUCCESS;
}

/*==========================================================
**
**
**	Abort an SCSI command.
**	This is called from the generic SCSI driver.
**
**
**==========================================================
*/
static int ncr_abort_command (ncb_p np, Scsi_Cmnd *cmd)
{
/*	Scsi_Device        *device    = cmd->device; */
	ccb_p cp;
	int found;
	int retv;

/*
 * First, look for the scsi command in the waiting list
 */
	if (remove_from_waiting_list(np, cmd)) {
		cmd->result = ScsiResult(DID_ABORT, 0);
		ncr_queue_done_cmd(np, cmd);
		return SCSI_ABORT_SUCCESS;
	}

/*
 * Then, look in the wakeup list
 */
	for (found=0, cp=np->ccb; cp; cp=cp->link_ccb) {
		/*
		**	look for the ccb of this command.
		*/
		if (cp->host_status == HS_IDLE) continue;
		if (cp->cmd == cmd) {
			found = 1;
			break;
		}
	}

	if (!found) {
		return SCSI_ABORT_NOT_RUNNING;
	}

	if (np->settle_time) {
		return SCSI_ABORT_SNOOZE;
	}

	/*
	**	If the CCB is active, patch schedule jumps for the 
	**	script to abort the command.
	*/

	switch(cp->host_status) {
	case HS_BUSY:
	case HS_NEGOTIATE:
		printk ("%s: abort ccb=%p (cancel)\n", ncr_name (np), cp);
			cp->start.schedule.l_paddr =
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, cancel));
		retv = SCSI_ABORT_PENDING;
		break;
	case HS_DISCONNECT:
		cp->restart.schedule.l_paddr =
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, abort));
		retv = SCSI_ABORT_PENDING;
		break;
	default:
		retv = SCSI_ABORT_NOT_RUNNING;
		break;

	}

	/*
	**      If there are no requests, the script
	**      processor will sleep on SEL_WAIT_RESEL.
	**      Let's wake it up, since it may have to work.
	*/
	OUTB (nc_istat, SIGP);

	return retv;
}

/*==========================================================
**
**	Linux release module stuff.
**
**	Called before unloading the module
**	Detach the host.
**	We have to free resources and halt the NCR chip
**
**==========================================================
*/

#ifdef MODULE
static int ncr_detach(ncb_p np)
{
	ccb_p cp;
	tcb_p tp;
	lcb_p lp;
	int target, lun;
	int i;

	printk("%s: releasing host resources\n", ncr_name(np));

/*
**	Stop the ncr_timeout process
**	Set release_stage to 1 and wait that ncr_timeout() set it to 2.
*/

#ifdef DEBUG_NCR53C8XX
	printk("%s: stopping the timer\n", ncr_name(np));
#endif
	np->release_stage = 1;
	for (i = 50 ; i && np->release_stage != 2 ; i--) MDELAY (100);
	if (np->release_stage != 2)
		printk("%s: the timer seems to be already stopped\n", ncr_name(np));
	else np->release_stage = 2;

/*
**	Disable chip interrupts
*/

#ifdef DEBUG_NCR53C8XX
	printk("%s: disabling chip interrupts\n", ncr_name(np));
#endif
	OUTW (nc_sien , 0);
	OUTB (nc_dien , 0);

/*
**	Free irq
*/

#ifdef DEBUG_NCR53C8XX
#ifdef __sparc__
	printk("%s: freeing irq %s\n", ncr_name(np), __irq_itoa(np->irq));
#else
	printk("%s: freeing irq %d\n", ncr_name(np), np->irq);
#endif
#endif
	free_irq(np->irq, np);

	/*
	**	Reset NCR chip
	**	Restore bios setting for automatic clock detection.
	*/

	printk("%s: resetting chip\n", ncr_name(np));
	OUTB (nc_istat,  SRST);
	UDELAY (100);
	OUTB (nc_istat,  0   );

	OUTB(nc_dmode,	np->sv_dmode);
	OUTB(nc_dcntl,	np->sv_dcntl);
	OUTB(nc_ctest3,	np->sv_ctest3);
	OUTB(nc_ctest4,	np->sv_ctest4);
	OUTB(nc_ctest5,	np->sv_ctest5);
	OUTB(nc_gpcntl,	np->sv_gpcntl);
	OUTB(nc_stest2,	np->sv_stest2);

	ncr_selectclock(np, np->sv_scntl3);

	/*
	**	Release Memory mapped IO region and IO mapped region
	*/

#ifndef SCSI_NCR_IOMAPPED
#ifdef DEBUG_NCR53C8XX
	printk("%s: releasing memory mapped IO region %lx[%d]\n", ncr_name(np), (u_long) np->vaddr, 128);
#endif
	unmap_pci_mem((vm_offset_t) np->vaddr, (u_long) 128);
#endif /* !SCSI_NCR_IOMAPPED */

#ifdef DEBUG_NCR53C8XX
	printk("%s: releasing IO region %x[%d]\n", ncr_name(np), np->base_io, 128);
#endif
	release_region(np->base_io, 128);

	/*
	**	Free allocated ccb(s)
	*/

	while ((cp=np->ccb->link_ccb) != NULL) {
		np->ccb->link_ccb = cp->link_ccb;
		if (cp->host_status) {
		printk("%s: shall free an active ccb (host_status=%d)\n",
			ncr_name(np), cp->host_status);
		}
#ifdef DEBUG_NCR53C8XX
	printk("%s: freeing ccb (%lx)\n", ncr_name(np), (u_long) cp);
#endif
		m_free_dma(cp, sizeof(*cp), "CCB");
	}

	/*
	**	Free allocated tp(s)
	*/

	for (target = 0; target < MAX_TARGET ; target++) {
		tp=&np->target[target];
		for (lun = 0 ; lun < MAX_LUN ; lun++) {
			lp = tp->lp[lun];
			if (lp) {
#ifdef DEBUG_NCR53C8XX
	printk("%s: freeing lp (%lx)\n", ncr_name(np), (u_long) lp);
#endif
				if (lp->jump_ccb != &lp->jump_ccb_0)
					m_free_dma(lp->jump_ccb,256,"JUMP_CCB");
				m_free_dma(lp, sizeof(*lp), "LCB");
			}
		}
	}

	if (np->scripth0)
		m_free_dma(np->scripth0, sizeof(struct scripth), "SCRIPTH");
	if (np->script0)
		m_free_dma(np->script0, sizeof(struct script), "SCRIPT");
	if (np->ccb)
		m_free_dma(np->ccb, sizeof(struct ccb), "CCB");
	m_free_dma(np, sizeof(struct ncb), "NCB");

	printk("%s: host resources successfully released\n", ncr_name(np));

	return 1;
}
#endif

/*==========================================================
**
**
**	Complete execution of a SCSI command.
**	Signal completion to the generic SCSI driver.
**
**
**==========================================================
*/

void ncr_complete (ncb_p np, ccb_p cp)
{
	Scsi_Cmnd *cmd;
	tcb_p tp;
	lcb_p lp;

	/*
	**	Sanity check
	*/

	if (!cp || cp->magic != CCB_MAGIC || !cp->cmd)
		return;

	/*
	**	Print minimal debug information.
	*/

	if (DEBUG_FLAGS & DEBUG_TINY)
		printk ("CCB=%lx STAT=%x/%x\n", (unsigned long)cp,
			cp->host_status,cp->scsi_status);

	/*
	**	Get command, target and lun pointers.
	*/

	cmd = cp->cmd;
	cp->cmd = NULL;
	tp = &np->target[cmd->target];
	lp = tp->lp[cmd->lun];

	/*
	**	We donnot queue more than 1 ccb per target 
	**	with negotiation at any time. If this ccb was 
	**	used for negotiation, clear this info in the tcb.
	*/

	if (cp == tp->nego_cp)
		tp->nego_cp = 0;

	/*
	**	If auto-sense performed, change scsi status.
	*/
	if (cp->auto_sense) {
		cp->scsi_status = cp->auto_sense;
	}

	/*
	**	If we were recovering from queue full or performing 
	**	auto-sense, requeue skipped CCBs to the wait queue.
	*/

	if (lp && lp->held_ccb) {
		if (cp == lp->held_ccb) {
			xpt_que_splice(&lp->skip_ccbq, &lp->wait_ccbq);
			xpt_que_init(&lp->skip_ccbq);
			lp->held_ccb = 0;
		}
	}

	/*
	**	Check for parity errors.
	*/

	if (cp->parity_status > 1) {
		PRINT_ADDR(cmd);
		printk ("%d parity error(s).\n",cp->parity_status);
	}

	/*
	**	Check for extended errors.
	*/

	if (cp->xerr_status != XE_OK) {
		PRINT_ADDR(cmd);
		switch (cp->xerr_status) {
		case XE_EXTRA_DATA:
			printk ("extraneous data discarded.\n");
			break;
		case XE_BAD_PHASE:
			printk ("illegal scsi phase (4/5).\n");
			break;
		default:
			printk ("extended error %d.\n", cp->xerr_status);
			break;
		}
		if (cp->host_status==HS_COMPLETE)
			cp->host_status = HS_FAIL;
	}

	/*
	**	Print out any error for debugging purpose.
	*/
	if (DEBUG_FLAGS & (DEBUG_RESULT|DEBUG_TINY)) {
		if (cp->host_status!=HS_COMPLETE || cp->scsi_status!=S_GOOD) {
			PRINT_ADDR(cmd);
			printk ("ERROR: cmd=%x host_status=%x scsi_status=%x\n",
				cmd->cmnd[0], cp->host_status, cp->scsi_status);
		}
	}

	/*
	**	Check the status.
	*/
	if (   (cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_GOOD ||
		    cp->scsi_status == S_COND_MET)) {
                /*
		**	All went well (GOOD status).
		**	CONDITION MET status is returned on 
                **	`Pre-Fetch' or `Search data' success.
                */
		cmd->result = ScsiResult(DID_OK, cp->scsi_status);

		/*
		**	@RESID@
		**	Could dig out the correct value for resid,
		**	but it would be quite complicated.
		*/
		/* if (cp->phys.header.lastp != cp->phys.header.goalp) */

		/*
		**	Allocate the lcb if not yet.
		*/
		if (!lp)
			ncr_alloc_lcb (np, cmd->target, cmd->lun);

		/*
		**	On standard INQUIRY response (EVPD and CmDt 
		**	not set), setup logical unit according to 
		**	announced capabilities (we need the 1rst 7 bytes).
		*/
		if (cmd->cmnd[0] == 0x12 && !(cmd->cmnd[1] & 0x3) &&
		    cmd->cmnd[4] >= 7 && !cmd->use_sg) {
			sync_scsi_data(np, cmd);	/* SYNC the data */
			ncr_setup_lcb (np, cmd->target, cmd->lun,
				       (char *) cmd->request_buffer);
		}

		tp->bytes     += cp->data_len;
		tp->transfers ++;

		/*
		**	If tags was reduced due to queue full,
		**	increase tags if 1000 good status received.
		*/
		if (lp && lp->usetags && lp->numtags < lp->maxtags) {
			++lp->num_good;
			if (lp->num_good >= 1000) {
				lp->num_good = 0;
				++lp->numtags;
				ncr_setup_tags (np, cmd->target, cmd->lun);
			}
		}
	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_CHECK_COND)) {
		/*
		**   Check condition code
		*/
		cmd->result = ScsiResult(DID_OK, S_CHECK_COND);

		/*
		**	Copy back sense data to caller's buffer.
		*/
		memcpy(cmd->sense_buffer, cp->sense_buf,
		       MIN(sizeof(cmd->sense_buffer), sizeof(cp->sense_buf)));

		if (DEBUG_FLAGS & (DEBUG_RESULT|DEBUG_TINY)) {
			u_char * p = (u_char*) & cmd->sense_buffer;
			int i;
			PRINT_ADDR(cmd);
			printk ("sense data:");
			for (i=0; i<14; i++) printk (" %x", *p++);
			printk (".\n");
		}
	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_CONFLICT)) {
		/*
		**   Reservation Conflict condition code
		*/
		cmd->result = ScsiResult(DID_OK, S_CONFLICT);
	
	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_BUSY ||
		    cp->scsi_status == S_QUEUE_FULL)) {

		/*
		**   Target is busy.
		*/
		cmd->result = ScsiResult(DID_OK, cp->scsi_status);

	} else if ((cp->host_status == HS_SEL_TIMEOUT)
		|| (cp->host_status == HS_TIMEOUT)) {

		/*
		**   No response
		*/
		cmd->result = ScsiResult(DID_TIME_OUT, cp->scsi_status);

	} else if (cp->host_status == HS_RESET) {

		/*
		**   SCSI bus reset
		*/
		cmd->result = ScsiResult(DID_RESET, cp->scsi_status);

	} else if (cp->host_status == HS_ABORTED) {

		/*
		**   Transfer aborted
		*/
		cmd->result = ScsiResult(DID_ABORT, cp->scsi_status);

	} else {

		/*
		**  Other protocol messes
		*/
		PRINT_ADDR(cmd);
		printk ("COMMAND FAILED (%x %x) @%p.\n",
			cp->host_status, cp->scsi_status, cp);

		cmd->result = ScsiResult(DID_ERROR, cp->scsi_status);
	}

	/*
	**	trace output
	*/

	if (tp->usrflag & UF_TRACE) {
		u_char * p;
		int i;
		PRINT_ADDR(cmd);
		printk (" CMD:");
		p = (u_char*) &cmd->cmnd[0];
		for (i=0; i<cmd->cmd_len; i++) printk (" %x", *p++);

		if (cp->host_status==HS_COMPLETE) {
			switch (cp->scsi_status) {
			case S_GOOD:
				printk ("  GOOD");
				break;
			case S_CHECK_COND:
				printk ("  SENSE:");
				p = (u_char*) &cmd->sense_buffer;
				for (i=0; i<14; i++)
					printk (" %x", *p++);
				break;
			default:
				printk ("  STAT: %x\n", cp->scsi_status);
				break;
			}
		} else printk ("  HOSTERROR: %x", cp->host_status);
		printk ("\n");
	}

	/*
	**	Free this ccb
	*/
	ncr_free_ccb (np, cp);

	/*
	**	requeue awaiting scsi commands for this lun.
	*/
	if (lp && lp->queuedccbs < lp->queuedepth &&
	    !xpt_que_empty(&lp->wait_ccbq))
		ncr_start_next_ccb(np, lp, 2);

	/*
	**	requeue awaiting scsi commands for this controller.
	*/
	if (np->waiting_list)
		requeue_waiting_list(np);

	/*
	**	signal completion to generic driver.
	*/
	ncr_queue_done_cmd(np, cmd);
}

/*==========================================================
**
**
**	Signal all (or one) control block done.
**
**
**==========================================================
*/

/*
**	This CCB has been skipped by the NCR.
**	Queue it in the correponding unit queue.
*/
static void ncr_ccb_skipped(ncb_p np, ccb_p cp)
{
	tcb_p tp = &np->target[cp->target];
	lcb_p lp = tp->lp[cp->lun];

	if (lp && cp != np->ccb) {
		cp->host_status &= ~HS_SKIPMASK;
		cp->start.schedule.l_paddr = 
			cpu_to_scr(NCB_SCRIPT_PHYS (np, select));
		xpt_remque(&cp->link_ccbq);
		xpt_insque_tail(&cp->link_ccbq, &lp->skip_ccbq);
		if (cp->queued) {
			--lp->queuedccbs;
		}
	}
	if (cp->queued) {
		--np->queuedccbs;
		cp->queued = 0;
	}
}

/*
**	The NCR has completed CCBs.
**	Look at the DONE QUEUE if enabled, otherwise scan all CCBs
*/
void ncr_wakeup_done (ncb_p np)
{
	ccb_p cp;
#ifdef SCSI_NCR_CCB_DONE_SUPPORT
	int i, j;

	i = np->ccb_done_ic;
	while (1) {
		j = i+1;
		if (j >= MAX_DONE)
			j = 0;

		cp = np->ccb_done[j];
		if (!CCB_DONE_VALID(cp))
			break;

		np->ccb_done[j] = (ccb_p) CCB_DONE_EMPTY;
		np->scripth->done_queue[5*j + 4] =
				cpu_to_scr(NCB_SCRIPT_PHYS (np, done_plug));
		MEMORY_BARRIER();
		np->scripth->done_queue[5*i + 4] =
				cpu_to_scr(NCB_SCRIPT_PHYS (np, done_end));

		if (cp->host_status & HS_DONEMASK)
			ncr_complete (np, cp);
		else if (cp->host_status & HS_SKIPMASK)
			ncr_ccb_skipped (np, cp);

		i = j;
	}
	np->ccb_done_ic = i;
#else
	cp = np->ccb;
	while (cp) {
		if (cp->host_status & HS_DONEMASK)
			ncr_complete (np, cp);
		else if (cp->host_status & HS_SKIPMASK)
			ncr_ccb_skipped (np, cp);
		cp = cp->link_ccb;
	}
#endif
}

/*
**	Complete all active CCBs.
*/
void ncr_wakeup (ncb_p np, u_long code)
{
	ccb_p cp = np->ccb;

	while (cp) {
		if (cp->host_status != HS_IDLE) {
			cp->host_status = code;
			ncr_complete (np, cp);
		}
		cp = cp->link_ccb;
	}
}

/*==========================================================
**
**
**	Start NCR chip.
**
**
**==========================================================
*/

void ncr_init (ncb_p np, int reset, char * msg, u_long code)
{
 	int	i;

 	/*
	**	Reset chip if asked, otherwise just clear fifos.
 	*/

	if (reset) {
		OUTB (nc_istat,  SRST);
		UDELAY (100);
	}
	else {
		OUTB (nc_stest3, TE|CSF);
		OUTONB (nc_ctest3, CLF);
	}
 
	/*
	**	Message.
	*/

	if (msg) printk (KERN_INFO "%s: restart (%s).\n", ncr_name (np), msg);

	/*
	**	Clear Start Queue
	*/
	np->queuedepth = MAX_START - 1;	/* 1 entry needed as end marker */
	for (i = 1; i < MAX_START + MAX_START; i += 2)
		np->scripth0->tryloop[i] =
				cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));

	/*
	**	Start at first entry.
	*/
	np->squeueput = 0;
	np->script0->startpos[0] = cpu_to_scr(NCB_SCRIPTH_PHYS (np, tryloop));

	/*
	**	Clear Done Queue
	*/
	for (i = 0; i < MAX_DONE; i++) {
		np->ccb_done[i] = (ccb_p) CCB_DONE_EMPTY;
		np->scripth0->done_queue[5*i + 4] =
			cpu_to_scr(NCB_SCRIPT_PHYS (np, done_end));
	}

	/*
	**	Start at first entry.
	*/
	np->script0->done_pos[0] = cpu_to_scr(NCB_SCRIPTH_PHYS (np,done_queue));
	np->ccb_done_ic = MAX_DONE-1;
	np->scripth0->done_queue[5*(MAX_DONE-1) + 4] =
			cpu_to_scr(NCB_SCRIPT_PHYS (np, done_plug));

	/*
	**	Wakeup all pending jobs.
	*/
	ncr_wakeup (np, code);

	/*
	**	Init chip.
	*/

	OUTB (nc_istat,  0x00   );	/*  Remove Reset, abort */
	UDELAY (2000);	/* The 895 needs time for the bus mode to settle */

	OUTB (nc_scntl0, np->rv_scntl0 | 0xc0);
					/*  full arb., ena parity, par->ATN  */
	OUTB (nc_scntl1, 0x00);		/*  odd parity, and remove CRST!! */

	ncr_selectclock(np, np->rv_scntl3);	/* Select SCSI clock */

	OUTB (nc_scid  , RRE|np->myaddr);	/* Adapter SCSI address */
	OUTW (nc_respid, 1ul<<np->myaddr);	/* Id to respond to */
	OUTB (nc_istat , SIGP	);		/*  Signal Process */
	OUTB (nc_dmode , np->rv_dmode);		/* Burst length, dma mode */
	OUTB (nc_ctest5, np->rv_ctest5);	/* Large fifo + large burst */

	OUTB (nc_dcntl , NOCOM|np->rv_dcntl);	/* Protect SFBR */
	OUTB (nc_ctest3, np->rv_ctest3);	/* Write and invalidate */
	OUTB (nc_ctest4, np->rv_ctest4);	/* Master parity checking */

	OUTB (nc_stest2, EXT|np->rv_stest2);	/* Extended Sreq/Sack filtering */
	OUTB (nc_stest3, TE);			/* TolerANT enable */
	OUTB (nc_stime0, 0x0c	);		/* HTH disabled  STO 0.25 sec */

	/*
	**	Disable disconnects.
	*/

	np->disc = 0;

	/*
	**    Enable GPIO0 pin for writing if LED support.
	*/

	if (np->features & FE_LED0) {
		OUTOFFB (nc_gpcntl, 0x01);
	}

	/*
	**      enable ints
	*/

	OUTW (nc_sien , STO|HTH|MA|SGE|UDC|RST|PAR);
	OUTB (nc_dien , MDPE|BF|ABRT|SSI|SIR|IID);

	/*
	**	For 895/6 enable SBMC interrupt and save current SCSI bus mode.
	*/
	if (np->features & FE_ULTRA2) {
		OUTONW (nc_sien, SBMC);
		np->scsi_mode = INB (nc_stest4) & SMODE;
	}

	/*
	**	DEL 441 - 53C876 Rev 5 - Part Number 609-0392787/2788 - ITEM 2.
	**	Disable overlapped arbitration.
	**	All 896 chips are also affected by this errata.
	*/
	if (np->device_id == PCI_DEVICE_ID_NCR_53C875)
		OUTB (nc_ctest0, (1<<5));
	else if (np->device_id == PCI_DEVICE_ID_NCR_53C896)
		OUTB (nc_ccntl0, DPR);

	/*
	**	Fill in target structure.
	**	Reinitialize usrsync.
	**	Reinitialize usrwide.
	**	Prepare sync negotiation according to actual SCSI bus mode.
	*/

	for (i=0;i<MAX_TARGET;i++) {
		tcb_p tp = &np->target[i];

		tp->sval    = 0;
		tp->wval    = np->rv_scntl3;

		if (tp->usrsync != 255) {
			if (tp->usrsync <= np->maxsync) {
				if (tp->usrsync < np->minsync) {
					tp->usrsync = np->minsync;
				}
			}
			else
				tp->usrsync = 255;
		};

		if (tp->usrwide > np->maxwide)
			tp->usrwide = np->maxwide;

		ncr_negotiate (np, tp);
	}

	/*
	**    Start script processor.
	*/
	if (np->paddr2) {
		if (bootverbose)
			printk ("%s: Downloading SCSI SCRIPTS.\n",
				ncr_name(np));
		OUTL (nc_scratcha, vtobus(np->script0));
		OUTL_DSP (NCB_SCRIPTH_PHYS (np, start_ram));
	}
	else
		OUTL_DSP (NCB_SCRIPT_PHYS (np, start));
}

/*==========================================================
**
**	Prepare the negotiation values for wide and
**	synchronous transfers.
**
**==========================================================
*/

static void ncr_negotiate (struct ncb* np, struct tcb* tp)
{
	/*
	**	minsync unit is 4ns !
	*/

	u_long minsync = tp->usrsync;

	/*
	**	SCSI bus mode limit
	*/

	if (np->scsi_mode && np->scsi_mode == SMODE_SE) {
		if (minsync < 12) minsync = 12;
	}

	/*
	**	our limit ..
	*/

	if (minsync < np->minsync)
		minsync = np->minsync;

	/*
	**	divider limit
	*/

	if (minsync > np->maxsync)
		minsync = 255;

	tp->minsync = minsync;
	tp->maxoffs = (minsync<255 ? np->maxoffs : 0);

	/*
	**	period=0: has to negotiate sync transfer
	*/

	tp->period=0;

	/*
	**	widedone=0: has to negotiate wide transfer
	*/
	tp->widedone=0;
}

/*==========================================================
**
**	Get clock factor and sync divisor for a given 
**	synchronous factor period.
**	Returns the clock factor (in sxfer) and scntl3 
**	synchronous divisor field.
**
**==========================================================
*/

static void ncr_getsync(ncb_p np, u_char sfac, u_char *fakp, u_char *scntl3p)
{
	u_long	clk = np->clock_khz;	/* SCSI clock frequency in kHz	*/
	int	div = np->clock_divn;	/* Number of divisors supported	*/
	u_long	fak;			/* Sync factor in sxfer		*/
	u_long	per;			/* Period in tenths of ns	*/
	u_long	kpc;			/* (per * clk)			*/

	/*
	**	Compute the synchronous period in tenths of nano-seconds
	*/
	if	(sfac <= 10)	per = 250;
	else if	(sfac == 11)	per = 303;
	else if	(sfac == 12)	per = 500;
	else			per = 40 * sfac;

	/*
	**	Look for the greatest clock divisor that allows an 
	**	input speed faster than the period.
	*/
	kpc = per * clk;
	while (--div >= 0)
		if (kpc >= (div_10M[div] << 2)) break;

	/*
	**	Calculate the lowest clock factor that allows an output 
	**	speed not faster than the period.
	*/
	fak = (kpc - 1) / div_10M[div] + 1;

#if 0	/* This optimization does not seem very useful */

	per = (fak * div_10M[div]) / clk;

	/*
	**	Why not to try the immediate lower divisor and to choose 
	**	the one that allows the fastest output speed ?
	**	We dont want input speed too much greater than output speed.
	*/
	if (div >= 1 && fak < 8) {
		u_long fak2, per2;
		fak2 = (kpc - 1) / div_10M[div-1] + 1;
		per2 = (fak2 * div_10M[div-1]) / clk;
		if (per2 < per && fak2 <= 8) {
			fak = fak2;
			per = per2;
			--div;
		}
	}
#endif

	if (fak < 4) fak = 4;	/* Should never happen, too bad ... */

	/*
	**	Compute and return sync parameters for the ncr
	*/
	*fakp		= fak - 4;
	*scntl3p	= ((div+1) << 4) + (sfac < 25 ? 0x80 : 0);
}


/*==========================================================
**
**	Set actual values, sync status and patch all ccbs of 
**	a target according to new sync/wide agreement.
**
**==========================================================
*/

static void ncr_set_sync_wide_status (ncb_p np, u_char target)
{
	ccb_p cp;
	tcb_p tp = &np->target[target];

	/*
	**	set actual value and sync_status
	*/
	OUTB (nc_sxfer, tp->sval);
	np->sync_st = tp->sval;
	OUTB (nc_scntl3, tp->wval);
	np->wide_st = tp->wval;

	/*
	**	patch ALL ccbs of this target.
	*/
	for (cp = np->ccb; cp; cp = cp->link_ccb) {
		if (!cp->cmd) continue;
		if (cp->cmd->target != target) continue;
#if 0
		cp->sync_status = tp->sval;
		cp->wide_status = tp->wval;
#endif
		cp->phys.select.sel_scntl3 = tp->wval;
		cp->phys.select.sel_sxfer  = tp->sval;
	};
}

/*==========================================================
**
**	Switch sync mode for current job and it's target
**
**==========================================================
*/

static void ncr_setsync (ncb_p np, ccb_p cp, u_char scntl3, u_char sxfer)
{
	Scsi_Cmnd *cmd;
	tcb_p tp;
	u_char target = INB (nc_sdid) & 0x0f;
	u_char idiv;

	assert (cp && cp->cmd);
	if (!cp) return;

	cmd = cp->cmd;
	if (!cmd) return;

	assert (target == (cmd->target & 0xf));

	tp = &np->target[target];

	if (!scntl3 || !(sxfer & 0x1f))
		scntl3 = np->rv_scntl3;
	scntl3 = (scntl3 & 0xf0) | (tp->wval & EWS) | (np->rv_scntl3 & 0x07);

	/*
	**	Deduce the value of controller sync period from scntl3.
	**	period is in tenths of nano-seconds.
	*/

	idiv = ((scntl3 >> 4) & 0x7);
	if ((sxfer & 0x1f) && idiv)
		tp->period = (((sxfer>>5)+4)*div_10M[idiv-1])/np->clock_khz;
	else
		tp->period = 0xffff;

	/*
	**	 Stop there if sync parameters are unchanged
	*/
	if (tp->sval == sxfer && tp->wval == scntl3) return;
	tp->sval = sxfer;
	tp->wval = scntl3;

	/*
	**	Bells and whistles   ;-)
	*/
	PRINT_TARGET(np, target);
	if (sxfer & 0x01f) {
		unsigned f10 = 100000 << (tp->widedone ? tp->widedone -1 : 0);
		unsigned mb10 = (f10 + tp->period/2) / tp->period;
		char *scsi;

		/*
		**  Disable extended Sreq/Sack filtering
		*/
		if (tp->period <= 2000) OUTOFFB (nc_stest2, EXT);

		/*
		**	Bells and whistles   ;-)
		*/
		if	(tp->period < 500)	scsi = "FAST-40";
		else if	(tp->period < 1000)	scsi = "FAST-20";
		else if	(tp->period < 2000)	scsi = "FAST-10";
		else				scsi = "FAST-5";

		printk ("%s %sSCSI %d.%d MB/s (%d ns, offset %d)\n", scsi,
			tp->widedone > 1 ? "WIDE " : "",
			mb10 / 10, mb10 % 10, tp->period / 10, sxfer & 0x1f);
	} else
		printk ("%sasynchronous.\n", tp->widedone > 1 ? "wide " : "");

	/*
	**	set actual value and sync_status
	**	patch ALL ccbs of this target.
	*/
	ncr_set_sync_wide_status(np, target);
}

/*==========================================================
**
**	Switch wide mode for current job and it's target
**	SCSI specs say: a SCSI device that accepts a WDTR 
**	message shall reset the synchronous agreement to 
**	asynchronous mode.
**
**==========================================================
*/

static void ncr_setwide (ncb_p np, ccb_p cp, u_char wide, u_char ack)
{
	Scsi_Cmnd *cmd;
	u_short target = INB (nc_sdid) & 0x0f;
	tcb_p tp;
	u_char	scntl3;
	u_char	sxfer;

	assert (cp && cp->cmd);
	if (!cp) return;

	cmd = cp->cmd;
	if (!cmd) return;

	assert (target == (cmd->target & 0xf));

	tp = &np->target[target];
	tp->widedone  =  wide+1;
	scntl3 = (tp->wval & (~EWS)) | (wide ? EWS : 0);

	sxfer = ack ? 0 : tp->sval;

	/*
	**	 Stop there if sync/wide parameters are unchanged
	*/
	if (tp->sval == sxfer && tp->wval == scntl3) return;
	tp->sval = sxfer;
	tp->wval = scntl3;

	/*
	**	Bells and whistles   ;-)
	*/
	if (bootverbose >= 2) {
		PRINT_TARGET(np, target);
		if (scntl3 & EWS)
			printk ("WIDE SCSI (16 bit) enabled.\n");
		else
			printk ("WIDE SCSI disabled.\n");
	}

	/*
	**	set actual value and sync_status
	**	patch ALL ccbs of this target.
	*/
	ncr_set_sync_wide_status(np, target);
}

/*==========================================================
**
**	Switch tagged mode for a target.
**
**==========================================================
*/

static void ncr_setup_tags (ncb_p np, u_char tn, u_char ln)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = tp->lp[ln];
	u_char   reqtags, maxdepth;

	/*
	**	Just in case ...
	*/
	if ((!tp) || (!lp))
		return;

	/*
	**	If SCSI device queue depth is not yet set, leave here.
	*/
	if (!lp->scdev_depth)
		return;

	/*
	**	Donnot allow more tags than the SCSI driver can queue 
	**	for this device.
	**	Donnot allow more tags than we can handle.
	*/
	maxdepth = lp->scdev_depth;
	if (maxdepth > lp->maxnxs)	maxdepth    = lp->maxnxs;
	if (lp->maxtags > maxdepth)	lp->maxtags = maxdepth;
	if (lp->numtags > maxdepth)	lp->numtags = maxdepth;

	/*
	**	only devices conformant to ANSI Version >= 2
	**	only devices capable of tagged commands
	**	only if enabled by user ..
	*/
	if ((lp->inq_byte7 & INQ7_QUEUE) && lp->numtags > 1) {
		reqtags = lp->numtags;
	} else {
		reqtags = 1;
	};

	/*
	**	Update max number of tags
	*/
	lp->numtags = reqtags;
	if (lp->numtags > lp->maxtags)
		lp->maxtags = lp->numtags;

	/*
	**	If we want to switch tag mode, we must wait 
	**	for no CCB to be active.
	*/
	if	(reqtags > 1 && lp->usetags) {	 /* Stay in tagged mode    */
		if (lp->queuedepth == reqtags)	 /* Already announced	   */
			return;
		lp->queuedepth	= reqtags;
	}
	else if	(reqtags <= 1 && !lp->usetags) { /* Stay in untagged mode  */
		lp->queuedepth	= reqtags;
		return;
	}
	else {					 /* Want to switch tag mode */
		if (lp->busyccbs)		 /* If not yet safe, return */
			return;
		lp->queuedepth	= reqtags;
		lp->usetags	= reqtags > 1 ? 1 : 0;
	}

	/*
	**	Patch the lun mini-script, according to tag mode.
	*/
	lp->jump_tag.l_paddr = lp->usetags?
			cpu_to_scr(NCB_SCRIPT_PHYS(np, resel_tag)) :
			cpu_to_scr(NCB_SCRIPT_PHYS(np, resel_notag));

	/*
	**	Announce change to user.
	*/
	if (bootverbose) {
		PRINT_LUN(np, tn, ln);
		if (lp->usetags) {
			printk("tagged command queue depth set to %d\n", reqtags);
		}
		else {
			printk("tagged command queueing disabled\n");
		}
	}
}

/*----------------------------------------------------
**
**	handle user commands
**
**----------------------------------------------------
*/

#ifdef SCSI_NCR_USER_COMMAND_SUPPORT

static void ncr_usercmd (ncb_p np)
{
	u_char t;
	tcb_p tp;

	switch (np->user.cmd) {

	case 0: return;

	case UC_SETSYNC:
		for (t=0; t<MAX_TARGET; t++) {
			if (!((np->user.target>>t)&1)) continue;
			tp = &np->target[t];
			tp->usrsync = np->user.data;
			ncr_negotiate (np, tp);
		};
		break;

	case UC_SETTAGS:
		for (t=0; t<MAX_TARGET; t++) {
			int ln;
			if (!((np->user.target>>t)&1)) continue;
			np->target[t].usrtags = np->user.data;
			for (ln = 0; ln < MAX_LUN; ln++) {
				lcb_p lp = np->target[t].lp[ln];
				if (!lp)
					continue;
				lp->maxtags = lp->numtags = np->user.data;
				ncr_setup_tags (np, t, ln);
			}
 		};
		break;

	case UC_SETDEBUG:
#ifdef SCSI_NCR_DEBUG_INFO_SUPPORT
		ncr_debug = np->user.data;
#endif
		break;

	case UC_SETORDER:
		np->order = np->user.data;
		break;

	case UC_SETVERBOSE:
		np->verbose = np->user.data;
		break;

	case UC_SETWIDE:
		for (t=0; t<MAX_TARGET; t++) {
			u_long size;
			if (!((np->user.target>>t)&1)) continue;
			tp = &np->target[t];
			size = np->user.data;
			if (size > np->maxwide) size=np->maxwide;
			tp->usrwide = size;
			ncr_negotiate (np, tp);
		};
		break;

	case UC_SETFLAG:
		for (t=0; t<MAX_TARGET; t++) {
			if (!((np->user.target>>t)&1)) continue;
			tp = &np->target[t];
			tp->usrflag = np->user.data;
		};
		break;
	}
	np->user.cmd=0;
}
#endif

/*==========================================================
**
**
**	ncr timeout handler.
**
**
**==========================================================
**
**	Misused to keep the driver running when
**	interrupts are not configured correctly.
**
**----------------------------------------------------------
*/

static void ncr_timeout (ncb_p np)
{
	u_long	thistime = ktime_get(0);

	/*
	**	If release process in progress, let's go
	**	Set the release stage from 1 to 2 to synchronize
	**	with the release process.
	*/

	if (np->release_stage) {
		if (np->release_stage == 1) np->release_stage = 2;
		return;
	}

	np->timer.expires = ktime_get(SCSI_NCR_TIMER_INTERVAL);
	add_timer(&np->timer);

	/*
	**	If we are resetting the ncr, wait for settle_time before 
	**	clearing it. Then command processing will be resumed.
	*/
	if (np->settle_time) {
		if (np->settle_time <= thistime) {
			if (bootverbose > 1)
				printk("%s: command processing resumed\n", ncr_name(np));
			np->settle_time	= 0;
			np->disc	= 1;
			requeue_waiting_list(np);
		}
		return;
	}

	/*
	**	Since the generic scsi driver only allows us 0.5 second 
	**	to perform abort of a command, we must look at ccbs about 
	**	every 0.25 second.
	*/
	if (np->lasttime + 4*HZ < thistime) {
		/*
		**	block ncr interrupts
		*/
		np->lasttime = thistime;
	}

#ifdef SCSI_NCR_BROKEN_INTR
	if (INB(nc_istat) & (INTF|SIP|DIP)) {

		/*
		**	Process pending interrupts.
		*/
		if (DEBUG_FLAGS & DEBUG_TINY) printk ("{");
		ncr_exception (np);
		if (DEBUG_FLAGS & DEBUG_TINY) printk ("}");
	}
#endif /* SCSI_NCR_BROKEN_INTR */
}

/*==========================================================
**
**	log message for real hard errors
**
**	"ncr0 targ 0?: ERROR (ds:si) (so-si-sd) (sxfer/scntl3) @ name (dsp:dbc)."
**	"	      reg: r0 r1 r2 r3 r4 r5 r6 ..... rf."
**
**	exception register:
**		ds:	dstat
**		si:	sist
**
**	SCSI bus lines:
**		so:	control lines as driver by NCR.
**		si:	control lines as seen by NCR.
**		sd:	scsi data lines as seen by NCR.
**
**	wide/fastmode:
**		sxfer:	(see the manual)
**		scntl3:	(see the manual)
**
**	current script command:
**		dsp:	script address (relative to start of script).
**		dbc:	first word of script command.
**
**	First 16 register of the chip:
**		r0..rf
**
**==========================================================
*/

static void ncr_log_hard_error(ncb_p np, u_short sist, u_char dstat)
{
	u_int32	dsp;
	int	script_ofs;
	int	script_size;
	char	*script_name;
	u_char	*script_base;
	int	i;

	dsp	= INL (nc_dsp);

	if (dsp > np->p_script && dsp <= np->p_script + sizeof(struct script)) {
		script_ofs	= dsp - np->p_script;
		script_size	= sizeof(struct script);
		script_base	= (u_char *) np->script0;
		script_name	= "script";
	}
	else if (np->p_scripth < dsp && 
		 dsp <= np->p_scripth + sizeof(struct scripth)) {
		script_ofs	= dsp - np->p_scripth;
		script_size	= sizeof(struct scripth);
		script_base	= (u_char *) np->scripth0;
		script_name	= "scripth";
	} else {
		script_ofs	= dsp;
		script_size	= 0;
		script_base	= 0;
		script_name	= "mem";
	}

	printk ("%s:%d: ERROR (%x:%x) (%x-%x-%x) (%x/%x) @ (%s %x:%08x).\n",
		ncr_name (np), (unsigned)INB (nc_sdid)&0x0f, dstat, sist,
		(unsigned)INB (nc_socl), (unsigned)INB (nc_sbcl), (unsigned)INB (nc_sbdl),
		(unsigned)INB (nc_sxfer),(unsigned)INB (nc_scntl3), script_name, script_ofs,
		(unsigned)INL (nc_dbc));

	if (((script_ofs & 3) == 0) &&
	    (unsigned)script_ofs < script_size) {
		printk ("%s: script cmd = %08x\n", ncr_name(np),
			scr_to_cpu((int) *(ncrcmd *)(script_base + script_ofs)));
	}

        printk ("%s: regdump:", ncr_name(np));
        for (i=0; i<16;i++)
            printk (" %02x", (unsigned)INB_OFF(i));
        printk (".\n");
}

/*============================================================
**
**	ncr chip exception handler.
**
**============================================================
**
**	In normal cases, interrupt conditions occur one at a 
**	time. The ncr is able to stack in some extra registers 
**	other interrupts that will occurs after the first one.
**	But severall interrupts may occur at the same time.
**
**	We probably should only try to deal with the normal 
**	case, but it seems that multiple interrupts occur in 
**	some cases that are not abnormal at all.
**
**	The most frequent interrupt condition is Phase Mismatch.
**	We should want to service this interrupt quickly.
**	A SCSI parity error may be delivered at the same time.
**	The SIR interrupt is not very frequent in this driver, 
**	since the INTFLY is likely used for command completion 
**	signaling.
**	The Selection Timeout interrupt may be triggered with 
**	IID and/or UDC.
**	The SBMC interrupt (SCSI Bus Mode Change) may probably 
**	occur at any time.
**
**	This handler try to deal as cleverly as possible with all
**	the above.
**
**============================================================
*/

void ncr_exception (ncb_p np)
{
	u_char	istat, dstat;
	u_short	sist;
	int	i;

	/*
	**	interrupt on the fly ?
	**	Since the global header may be copied back to a CCB 
	**	using a posted PCI memory write, the last operation on 
	**	the istat register is a READ in order to flush posted 
	**	PCI write commands.
	*/
	istat = INB (nc_istat);
	if (istat & INTF) {
		OUTB (nc_istat, (istat & SIGP) | INTF);
		istat = INB (nc_istat);
		if (DEBUG_FLAGS & DEBUG_TINY) printk ("F ");
		ncr_wakeup_done (np);
	};

	if (!(istat & (SIP|DIP)))
		return;

	if (istat & CABRT)
		OUTB (nc_istat, CABRT);

	/*
	**	Steinbach's Guideline for Systems Programming:
	**	Never test for an error condition you don't know how to handle.
	*/

	sist  = (istat & SIP) ? INW (nc_sist)  : 0;
	dstat = (istat & DIP) ? INB (nc_dstat) : 0;

	if (DEBUG_FLAGS & DEBUG_TINY)
		printk ("<%d|%x:%x|%x:%x>",
			(int)INB(nc_scr0),
			dstat,sist,
			(unsigned)INL(nc_dsp),
			(unsigned)INL(nc_dbc));

	/*========================================================
	**	First, interrupts we want to service cleanly.
	**
	**	Phase mismatch is the most frequent interrupt, and 
	**	so we have to service it as quickly and as cleanly 
	**	as possible.
	**	Programmed interrupts are rarely used in this driver,
	**	but we must handle them cleanly anyway.
	**	We try to deal with PAR and SBMC combined with 
	**	some other interrupt(s).
	**=========================================================
	*/

	if (!(sist  & (STO|GEN|HTH|SGE|UDC|RST)) &&
	    !(dstat & (MDPE|BF|ABRT|IID))) {
		if ((sist & SBMC) && ncr_int_sbmc (np))
			return;
		if ((sist & PAR)  && ncr_int_par  (np))
			return;
		if (sist & MA) {
			ncr_int_ma (np);
			return;
		}
		if (dstat & SIR) {
			ncr_int_sir (np);
			return;
		}
		/*
		**  DEL 397 - 53C875 Rev 3 - Part Number 609-0392410 - ITEM 2.
		*/
		if (!(sist & (SBMC|PAR)) && !(dstat & SSI)) {
			printk(	"%s: unknown interrupt(s) ignored, "
				"ISTAT=%x DSTAT=%x SIST=%x\n",
				ncr_name(np), istat, dstat, sist);
			return;
		}
		OUTONB_STD ();
		return;
	};

	/*========================================================
	**	Now, interrupts that need some fixing up.
	**	Order and multiple interrupts is so less important.
	**
	**	If SRST has been asserted, we just reset the chip.
	**
	**	Selection is intirely handled by the chip. If the 
	**	chip says STO, we trust it. Seems some other 
	**	interrupts may occur at the same time (UDC, IID), so 
	**	we ignore them. In any case we do enough fix-up 
	**	in the service routine.
	**	We just exclude some fatal dma errors.
	**=========================================================
	*/

	if (sist & RST) {
		ncr_init (np, 1, bootverbose ? "scsi reset" : NULL, HS_RESET);
		return;
	};

	if ((sist & STO) &&
		!(dstat & (MDPE|BF|ABRT))) {
	/*
	**	DEL 397 - 53C875 Rev 3 - Part Number 609-0392410 - ITEM 1.
	*/
		OUTONB (nc_ctest3, CLF);

		ncr_int_sto (np);
		return;
	};

	/*=========================================================
	**	Now, interrupts we are not able to recover cleanly.
	**	(At least for the moment).
	**
	**	Do the register dump.
	**	Log message for real hard errors.
	**	Clear all fifos.
	**	For MDPE, BF, ABORT, IID, SGE and HTH we reset the 
	**	BUS and the chip.
	**	We are more soft for UDC.
	**=========================================================
	*/

	if (ktime_exp(np->regtime)) {
		np->regtime = ktime_get(10*HZ);
		for (i = 0; i<sizeof(np->regdump); i++)
			((char*)&np->regdump)[i] = INB_OFF(i);
		np->regdump.nc_dstat = dstat;
		np->regdump.nc_sist  = sist;
	};

	ncr_log_hard_error(np, sist, dstat);

	printk ("%s: have to clear fifos.\n", ncr_name (np));
	OUTB (nc_stest3, TE|CSF);
	OUTONB (nc_ctest3, CLF);

	if ((sist & (SGE)) ||
		(dstat & (MDPE|BF|ABRT|IID))) {
		ncr_start_reset(np);
		return;
	};

	if (sist & HTH) {
		printk ("%s: handshake timeout\n", ncr_name(np));
		ncr_start_reset(np);
		return;
	};

	if (sist & UDC) {
		printk ("%s: unexpected disconnect\n", ncr_name(np));
		OUTB (HS_PRT, HS_UNEXPECTED);
		OUTL_DSP (NCB_SCRIPT_PHYS (np, cleanup));
		return;
	};

	/*=========================================================
	**	We just miss the cause of the interrupt. :(
	**	Print a message. The timeout will do the real work.
	**=========================================================
	*/
	printk ("%s: unknown interrupt\n", ncr_name(np));
}

/*==========================================================
**
**	ncr chip exception handler for selection timeout
**
**==========================================================
**
**	There seems to be a bug in the 53c810.
**	Although a STO-Interrupt is pending,
**	it continues executing script commands.
**	But it will fail and interrupt (IID) on
**	the next instruction where it's looking
**	for a valid phase.
**
**----------------------------------------------------------
*/

void ncr_int_sto (ncb_p np)
{
	u_long dsa;
	ccb_p cp;
	if (DEBUG_FLAGS & DEBUG_TINY) printk ("T");

	/*
	**	look for ccb and set the status.
	*/

	dsa = INL (nc_dsa);
	cp = np->ccb;
	while (cp && (CCB_PHYS (cp, phys) != dsa))
		cp = cp->link_ccb;

	if (cp) {
		cp-> host_status = HS_SEL_TIMEOUT;
		ncr_complete (np, cp);
	};

	/*
	**	repair start queue and jump to start point.
	*/

	OUTL_DSP (NCB_SCRIPTH_PHYS (np, sto_restart));
	return;
}

/*==========================================================
**
**	ncr chip exception handler for SCSI bus mode change
**
**==========================================================
**
**	spi2-r12 11.2.3 says a transceiver mode change must 
**	generate a reset event and a device that detects a reset 
**	event shall initiate a hard reset. It says also that a
**	device that detects a mode change shall set data transfer 
**	mode to eight bit asynchronous, etc...
**	So, just resetting should be enough.
**	 
**
**----------------------------------------------------------
*/

static int ncr_int_sbmc (ncb_p np)
{
	u_char scsi_mode = INB (nc_stest4) & SMODE;

	if (scsi_mode != np->scsi_mode) {
		printk("%s: SCSI bus mode change from %x to %x.\n",
			ncr_name(np), np->scsi_mode, scsi_mode);

		np->scsi_mode = scsi_mode;


		/*
		**	Suspend command processing for 1 second and 
		**	reinitialize all except the chip.
		*/
		np->settle_time	= ktime_get(1*HZ);
		ncr_init (np, 0, bootverbose ? "scsi mode change" : NULL, HS_RESET);
		return 1;
	}
	return 0;
}

/*==========================================================
**
**	ncr chip exception handler for SCSI parity error.
**
**==========================================================
**
**
**----------------------------------------------------------
*/

static int ncr_int_par (ncb_p np)
{
	u_char	hsts	= INB (HS_PRT);
	u_int32	dbc	= INL (nc_dbc);
	u_char	sstat1	= INB (nc_sstat1);
	int phase	= -1;
	int msg		= -1;
	u_int32 jmp;

	printk("%s: SCSI parity error detected: SCR1=%d DBC=%x SSTAT1=%x\n",
		ncr_name(np), hsts, dbc, sstat1);

	/*
	 *	Ignore the interrupt if the NCR is not connected 
	 *	to the SCSI bus, since the right work should have  
	 *	been done on unexpected disconnection handling.
	 */
	if (!(INB (nc_scntl1) & ISCON))
		return 0;

	/*
	 *	If the nexus is not clearly identified, reset the bus.
	 *	We will try to do better later.
	 */
	if (hsts & HS_INVALMASK)
		goto reset_all;

	/*
	 *	If the SCSI parity error occurs in MSG IN phase, prepare a 
	 *	MSG PARITY message. Otherwise, prepare a INITIATOR DETECTED 
	 *	ERROR message and let the device decide to retry the command 
	 *	or to terminate with check condition. If we were in MSG IN 
	 *	phase waiting for the response of a negotiation, we will 
	 *	get SIR_NEGO_FAILED at dispatch.
	 */
	if (!(dbc & 0xc0000000))
		phase = (dbc >> 24) & 7;
	if (phase == 7)
		msg = M_PARITY;
	else
		msg = M_ID_ERROR;

#ifdef SCSI_NCR_INTEGRITY_CHECKING
	/*
	**      Save error message. For integrity check use only.
	*/
	if (np->check_integrity)
		np->check_integ_par = msg;
#endif

	/*
	 *	If the NCR stopped on a MOVE ^ DATA_IN, we jump to a 
	 *	script that will ignore all data in bytes until phase 
	 *	change, since we are not sure the chip will wait the phase 
	 *	change prior to delivering the interrupt.
	 */
	if (phase == 1)
		jmp = NCB_SCRIPTH_PHYS (np, par_err_data_in);
	else
		jmp = NCB_SCRIPTH_PHYS (np, par_err_other);

	OUTONB (nc_ctest3, CLF );	/* clear dma fifo  */
	OUTB (nc_stest3, TE|CSF);	/* clear scsi fifo */

	np->msgout[0] = msg;
	OUTL_DSP (jmp);
	return 1;

reset_all:
	ncr_start_reset(np);
	return 1;
}

/*==========================================================
**
**
**	ncr chip exception handler for phase errors.
**
**
**==========================================================
**
**	We have to construct a new transfer descriptor,
**	to transfer the rest of the current block.
**
**----------------------------------------------------------
*/

static void ncr_int_ma (ncb_p np)
{
	u_int32	dbc;
	u_int32	rest;
	u_int32	dsp;
	u_int32	dsa;
	u_int32	nxtdsp;
	u_int32	newtmp;
	u_int32	*vdsp;
	u_int32	oadr, olen;
	u_int32	*tblp;
        ncrcmd *newcmd;
	u_char	cmd, sbcl;
	ccb_p	cp;

	dsp	= INL (nc_dsp);
	dbc	= INL (nc_dbc);
	sbcl	= INB (nc_sbcl);

	cmd	= dbc >> 24;
	rest	= dbc & 0xffffff;

	/*
	**	Take into account dma fifo and various buffers and latches,
	**	only if the interrupted phase is an OUTPUT phase.
	*/

	if ((cmd & 1) == 0) {
		u_char	ctest5, ss0, ss2;
		u_short	delta;

		ctest5 = (np->rv_ctest5 & DFS) ? INB (nc_ctest5) : 0;
		if (ctest5 & DFS)
			delta=(((ctest5 << 8) | (INB (nc_dfifo) & 0xff)) - rest) & 0x3ff;
		else
			delta=(INB (nc_dfifo) - rest) & 0x7f;

		/*
		**	The data in the dma fifo has not been transferred to
		**	the target -> add the amount to the rest
		**	and clear the data.
		**	Check the sstat2 register in case of wide transfer.
		*/

		rest += delta;
		ss0  = INB (nc_sstat0);
		if (ss0 & OLF) rest++;
		if (ss0 & ORF) rest++;
		if (INB(nc_scntl3) & EWS) {
			ss2 = INB (nc_sstat2);
			if (ss2 & OLF1) rest++;
			if (ss2 & ORF1) rest++;
		};

		if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_PHASE))
			printk ("P%x%x RL=%d D=%d SS0=%x ", cmd&7, sbcl&7,
				(unsigned) rest, (unsigned) delta, ss0);

	} else	{
		if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_PHASE))
			printk ("P%x%x RL=%d ", cmd&7, sbcl&7, rest);
	}

	/*
	**	Clear fifos.
	*/
	OUTONB (nc_ctest3, CLF );	/* clear dma fifo  */
	OUTB (nc_stest3, TE|CSF);	/* clear scsi fifo */

	/*
	**	locate matching cp.
	**	if the interrupted phase is DATA IN or DATA OUT,
	**	trust the global header.
	*/
	dsa = INL (nc_dsa);
	if (!(cmd & 6)) {
		cp = np->header.cp;
		if (CCB_PHYS(cp, phys) != dsa)
			cp = 0;
	} else {
		cp  = np->ccb;
		while (cp && (CCB_PHYS (cp, phys) != dsa))
			cp = cp->link_ccb;
	}

	/*
	**	try to find the interrupted script command,
	**	and the address at which to continue.
	*/
	vdsp	= 0;
	nxtdsp	= 0;
	if	(dsp >  np->p_script &&
		 dsp <= np->p_script + sizeof(struct script)) {
		vdsp = (u_int32 *)((char*)np->script0 + (dsp-np->p_script-8));
		nxtdsp = dsp;
	}
	else if	(dsp >  np->p_scripth &&
		 dsp <= np->p_scripth + sizeof(struct scripth)) {
		vdsp = (u_int32 *)((char*)np->scripth0 + (dsp-np->p_scripth-8));
		nxtdsp = dsp;
	}
	else if (cp) {
		if	(dsp == CCB_PHYS (cp, patch[2])) {
			vdsp = &cp->patch[0];
			nxtdsp = scr_to_cpu(vdsp[3]);
		}
		else if (dsp == CCB_PHYS (cp, patch[6])) {
			vdsp = &cp->patch[4];
			nxtdsp = scr_to_cpu(vdsp[3]);
		}
	}

	/*
	**	log the information
	*/

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printk ("\nCP=%p CP2=%p DSP=%x NXT=%x VDSP=%p CMD=%x ",
			cp, np->header.cp,
			(unsigned)dsp,
			(unsigned)nxtdsp, vdsp, cmd);
	};

	/*
	**	cp=0 means that the DSA does not point to a valid control 
	**	block. This should not happen since we donnot use multi-byte 
	**	move while we are being reselected ot after command complete.
	**	We are not able to recover from such a phase error.
	*/
	if (!cp) {
		printk ("%s: SCSI phase error fixup: "
			"CCB already dequeued (0x%08lx)\n", 
			ncr_name (np), (u_long) np->header.cp);
		goto reset_all;
	}

	/*
	**	get old startaddress and old length.
	*/

	oadr = scr_to_cpu(vdsp[1]);

	if (cmd & 0x10) {	/* Table indirect */
		tblp = (u_int32 *) ((char*) &cp->phys + oadr);
		olen = scr_to_cpu(tblp[0]);
		oadr = scr_to_cpu(tblp[1]);
	} else {
		tblp = (u_int32 *) 0;
		olen = scr_to_cpu(vdsp[0]) & 0xffffff;
	};

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printk ("OCMD=%x\nTBLP=%p OLEN=%x OADR=%x\n",
			(unsigned) (scr_to_cpu(vdsp[0]) >> 24),
			tblp,
			(unsigned) olen,
			(unsigned) oadr);
	};

	/*
	**	check cmd against assumed interrupted script command.
	*/

	if (cmd != (scr_to_cpu(vdsp[0]) >> 24)) {
		PRINT_ADDR(cp->cmd);
		printk ("internal error: cmd=%02x != %02x=(vdsp[0] >> 24)\n",
			(unsigned)cmd, (unsigned)scr_to_cpu(vdsp[0]) >> 24);

		goto reset_all;
	}

	/*
	**	cp != np->header.cp means that the header of the CCB 
	**	currently being processed has not yet been copied to 
	**	the global header area. That may happen if the device did 
	**	not accept all our messages after having been selected.
	*/
	if (cp != np->header.cp) {
		printk ("%s: SCSI phase error fixup: "
			"CCB address mismatch (0x%08lx != 0x%08lx)\n", 
			ncr_name (np), (u_long) cp, (u_long) np->header.cp);
	}

	/*
	**	if old phase not dataphase, leave here.
	*/

	if (cmd & 0x06) {
		PRINT_ADDR(cp->cmd);
		printk ("phase change %x-%x %d@%08x resid=%d.\n",
			cmd&7, sbcl&7, (unsigned)olen,
			(unsigned)oadr, (unsigned)rest);
		goto unexpected_phase;
	};

	/*
	**	choose the correct patch area.
	**	if savep points to one, choose the other.
	*/

	newcmd = cp->patch;
	newtmp = CCB_PHYS (cp, patch);
	if (newtmp == scr_to_cpu(cp->phys.header.savep)) {
		newcmd = &cp->patch[4];
		newtmp = CCB_PHYS (cp, patch[4]);
	}

	/*
	**	fillin the commands
	*/

	newcmd[0] = cpu_to_scr(((cmd & 0x0f) << 24) | rest);
	newcmd[1] = cpu_to_scr(oadr + olen - rest);
	newcmd[2] = cpu_to_scr(SCR_JUMP);
	newcmd[3] = cpu_to_scr(nxtdsp);

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		PRINT_ADDR(cp->cmd);
		printk ("newcmd[%d] %x %x %x %x.\n",
			(int) (newcmd - cp->patch),
			(unsigned)scr_to_cpu(newcmd[0]),
			(unsigned)scr_to_cpu(newcmd[1]),
			(unsigned)scr_to_cpu(newcmd[2]),
			(unsigned)scr_to_cpu(newcmd[3]));
	}
	/*
	**	fake the return address (to the patch).
	**	and restart script processor at dispatcher.
	*/
	OUTL (nc_temp, newtmp);
	OUTL_DSP (NCB_SCRIPT_PHYS (np, dispatch));
	return;

	/*
	**	Unexpected phase changes that occurs when the current phase 
	**	is not a DATA IN or DATA OUT phase are due to error conditions.
	**	Such event may only happen when the SCRIPTS is using a 
	**	multibyte SCSI MOVE.
	**
	**	Phase change		Some possible cause
	**
	**	COMMAND  --> MSG IN	SCSI parity error detected by target.
	**	COMMAND  --> STATUS	Bad command or refused by target.
	**	MSG OUT  --> MSG IN     Message rejected by target.
	**	MSG OUT  --> COMMAND    Bogus target that discards extended
	**				negotiation messages.
	**
	**	The code below does not care of the new phase and so 
	**	trusts the target. Why to annoy it ?
	**	If the interrupted phase is COMMAND phase, we restart at
	**	dispatcher.
	**	If a target does not get all the messages after selection, 
	**	the code assumes blindly that the target discards extended 
	**	messages and clears the negotiation status.
	**	If the target does not want all our response to negotiation,
	**	we force a SIR_NEGO_PROTO interrupt (it is a hack that avoids 
	**	bloat for such a should_not_happen situation).
	**	In all other situation, we reset the BUS.
	**	Are these assumptions reasonnable ? (Wait and see ...)
	*/
unexpected_phase:
	dsp -= 8;
	nxtdsp = 0;

	switch (cmd & 7) {
	case 2:	/* COMMAND phase */
		nxtdsp = NCB_SCRIPT_PHYS (np, dispatch);
		break;
#if 0
	case 3:	/* STATUS  phase */
		nxtdsp = NCB_SCRIPT_PHYS (np, dispatch);
		break;
#endif
	case 6:	/* MSG OUT phase */
		np->scripth->nxtdsp_go_on[0] = cpu_to_scr(dsp + 8);
		if	(dsp == NCB_SCRIPT_PHYS (np, send_ident)) {
			cp->host_status = HS_BUSY;
			nxtdsp = NCB_SCRIPTH_PHYS (np, clratn_go_on);
		}
		else if	(dsp == NCB_SCRIPTH_PHYS (np, send_wdtr) ||
			 dsp == NCB_SCRIPTH_PHYS (np, send_sdtr)) {
			nxtdsp = NCB_SCRIPTH_PHYS (np, nego_bad_phase);
		}
		break;
#if 0
	case 7:	/* MSG IN  phase */
		nxtdsp = NCB_SCRIPT_PHYS (np, clrack);
		break;
#endif
	}

	if (nxtdsp) {
		OUTL_DSP (nxtdsp);
		return;
	}

reset_all:
	ncr_start_reset(np);
}


static void ncr_sir_to_redo(ncb_p np, int num, ccb_p cp)
{
	Scsi_Cmnd *cmd	= cp->cmd;
	tcb_p tp	= &np->target[cmd->target];
	lcb_p lp	= tp->lp[cmd->lun];
	XPT_QUEHEAD	*qp;
	ccb_p		cp2;
	int		disc_cnt = 0;
	int		busy_cnt = 0;
	u_int32		startp;
	u_char		s_status = INB (SS_PRT);

	/*
	**	Let the SCRIPTS processor skip all not yet started CCBs,
	**	and count disconnected CCBs. Since the busy queue is in 
	**	the same order as the chip start queue, disconnected CCBs 
	**	are before cp and busy ones after.
	*/
	if (lp) {
		qp = lp->busy_ccbq.blink;
		while (qp != &lp->busy_ccbq) {
			cp2 = xpt_que_entry(qp, struct ccb, link_ccbq);
			qp  = qp->blink;
			++busy_cnt;
			if (cp2 == cp)
				break;
			cp2->start.schedule.l_paddr =
			cpu_to_scr(NCB_SCRIPTH_PHYS (np, skip));
		}
		lp->held_ccb = cp;	/* Requeue when this one completes */
		disc_cnt = lp->queuedccbs - busy_cnt;
	}

	switch(s_status) {
	default:	/* Just for safety, should never happen */
	case S_QUEUE_FULL:
		/*
		**	Decrease number of tags to the number of 
		**	disconnected commands.
		*/
		if (!lp)
			goto out;
		if (bootverbose >= 1) {
			PRINT_ADDR(cmd);
			printk ("QUEUE FULL! %d busy, %d disconnected CCBs\n",
				busy_cnt, disc_cnt);
		}
		if (disc_cnt < lp->numtags) {
			lp->numtags	= disc_cnt > 2 ? disc_cnt : 2;
			lp->num_good	= 0;
			ncr_setup_tags (np, cmd->target, cmd->lun);
		}
		/*
		**	Requeue the command to the start queue.
		**	If any disconnected commands,
		**		Clear SIGP.
		**		Jump to reselect.
		*/
		cp->phys.header.savep = cp->startp;
		cp->host_status = HS_BUSY;
		cp->scsi_status = S_ILLEGAL;

		ncr_put_start_queue(np, cp);
		if (disc_cnt)
			INB (nc_ctest2);		/* Clear SIGP */
		OUTL_DSP (NCB_SCRIPT_PHYS (np, reselect));
		return;
	case S_TERMINATED:
	case S_CHECK_COND:
		/*
		**	If we were requesting sense, give up.
		*/
		if (cp->auto_sense)
			goto out;

		/*
		**	Device returned CHECK CONDITION status.
		**	Prepare all needed data strutures for getting 
		**	sense data.
		**
		**	identify message
		*/
		cp->scsi_smsg2[0]	= M_IDENTIFY | cmd->lun;
		cp->phys.smsg.addr	= cpu_to_scr(CCB_PHYS (cp, scsi_smsg2));
		cp->phys.smsg.size	= cpu_to_scr(1);

		/*
		**	sense command
		*/
		cp->phys.cmd.addr	= cpu_to_scr(CCB_PHYS (cp, sensecmd));
		cp->phys.cmd.size	= cpu_to_scr(6);

		/*
		**	patch requested size into sense command
		*/
		cp->sensecmd[0]		= 0x03;
		cp->sensecmd[1]		= cmd->lun << 5;
		cp->sensecmd[4]		= sizeof(cp->sense_buf);

		/*
		**	sense data
		*/
		bzero(cp->sense_buf, sizeof(cp->sense_buf));
		cp->phys.sense.addr	= cpu_to_scr(CCB_PHYS(cp,sense_buf[0]));
		cp->phys.sense.size	= cpu_to_scr(sizeof(cp->sense_buf));

		/*
		**	requeue the command.
		*/
		startp = cpu_to_scr(NCB_SCRIPTH_PHYS (np, sdata_in));

		cp->phys.header.savep	= startp;
		cp->phys.header.goalp	= startp + 24;
		cp->phys.header.lastp	= startp;
		cp->phys.header.wgoalp	= startp + 24;
		cp->phys.header.wlastp	= startp;

		cp->host_status = HS_BUSY;
		cp->scsi_status = S_ILLEGAL;
		cp->auto_sense	= s_status;

		cp->start.schedule.l_paddr =
			cpu_to_scr(NCB_SCRIPT_PHYS (np, select));

		/*
		**	Select without ATN for quirky devices.
		*/
		if (tp->quirks & QUIRK_NOMSG)
			cp->start.schedule.l_paddr =
			cpu_to_scr(NCB_SCRIPTH_PHYS (np, select_no_atn));

		ncr_put_start_queue(np, cp);

		OUTL_DSP (NCB_SCRIPT_PHYS (np, start));
		return;
	}

out:
	OUTONB_STD ();
	return;
}


/*==========================================================
**
**
**      ncr chip exception handler for programmed interrupts.
**
**
**==========================================================
*/

static int ncr_show_msg (u_char * msg)
{
	u_char i;
	printk ("%x",*msg);
	if (*msg==M_EXTENDED) {
		for (i=1;i<8;i++) {
			if (i-1>msg[1]) break;
			printk ("-%x",msg[i]);
		};
		return (i+1);
	} else if ((*msg & 0xf0) == 0x20) {
		printk ("-%x",msg[1]);
		return (2);
	};
	return (1);
}

static void ncr_print_msg ( ccb_p cp, char *label, u_char *msg)
{
	if (cp)
		PRINT_ADDR(cp->cmd);
	if (label)
		printk("%s: ", label);
	
	(void) ncr_show_msg (msg);
	printk(".\n");
}

void ncr_int_sir (ncb_p np)
{
	u_char scntl3;
	u_char chg, ofs, per, fak, wide;
	u_char num = INB (nc_dsps);
	ccb_p	cp=0;
	u_long	dsa    = INL (nc_dsa);
	u_char	target = INB (nc_sdid) & 0x0f;
	tcb_p	tp     = &np->target[target];

	if (DEBUG_FLAGS & DEBUG_TINY) printk ("I#%d", num);

	switch (num) {
	case SIR_RESEL_NO_MSG_IN:
	case SIR_RESEL_NO_IDENTIFY:
		/*
		**	If devices reselecting without sending an IDENTIFY 
		**	message still exist, this should help.
		**	We just assume lun=0, 1 CCB, no tag.
		*/
		if (tp->lp[0]) { 
			OUTL_DSP (scr_to_cpu(tp->lp[0]->jump_ccb[0]));
			return;
		}
	case SIR_RESEL_BAD_TARGET:	/* Will send a TARGET RESET message */
	case SIR_RESEL_BAD_LUN:		/* Will send a TARGET RESET message */
	case SIR_RESEL_BAD_I_T_L_Q:	/* Will send an ABORT TAG message   */
	case SIR_RESEL_BAD_I_T_L:	/* Will send an ABORT message	    */
		printk ("%s:%d: SIR %d, "
			"incorrect nexus identification on reselection\n",
			ncr_name (np), target, num);
		goto out;
	case SIR_DONE_OVERFLOW:
		printk ("%s:%d: SIR %d, "
			"CCB done queue overflow\n",
			ncr_name (np), target, num);
		goto out;
	case SIR_BAD_STATUS:
		cp = np->header.cp;
		if (!cp || CCB_PHYS (cp, phys) != dsa)
			goto out;
		ncr_sir_to_redo(np, num, cp);
		return;
	default:
		/*
		**	lookup the ccb
		*/
		cp = np->ccb;
		while (cp && (CCB_PHYS (cp, phys) != dsa))
			cp = cp->link_ccb;

		assert (cp && cp == np->header.cp);

		if (!cp || cp != np->header.cp)
			goto out;
	}

	switch (num) {
/*-----------------------------------------------------------------------------
**
**	Was Sie schon immer ueber transfermode negotiation wissen wollten ...
**
**	We try to negotiate sync and wide transfer only after
**	a successful inquire command. We look at byte 7 of the
**	inquire data to determine the capabilities of the target.
**
**	When we try to negotiate, we append the negotiation message
**	to the identify and (maybe) simple tag message.
**	The host status field is set to HS_NEGOTIATE to mark this
**	situation.
**
**	If the target doesn't answer this message immidiately
**	(as required by the standard), the SIR_NEGO_FAIL interrupt
**	will be raised eventually.
**	The handler removes the HS_NEGOTIATE status, and sets the
**	negotiated value to the default (async / nowide).
**
**	If we receive a matching answer immediately, we check it
**	for validity, and set the values.
**
**	If we receive a Reject message immediately, we assume the
**	negotiation has failed, and fall back to standard values.
**
**	If we receive a negotiation message while not in HS_NEGOTIATE
**	state, it's a target initiated negotiation. We prepare a
**	(hopefully) valid answer, set our parameters, and send back 
**	this answer to the target.
**
**	If the target doesn't fetch the answer (no message out phase),
**	we assume the negotiation has failed, and fall back to default
**	settings.
**
**	When we set the values, we adjust them in all ccbs belonging 
**	to this target, in the controller's register, and in the "phys"
**	field of the controller's struct ncb.
**
**	Possible cases:		   hs  sir   msg_in value  send   goto
**	We try to negotiate:
**	-> target doesn't msgin    NEG FAIL  noop   defa.  -      dispatch
**	-> target rejected our msg NEG FAIL  reject defa.  -      dispatch
**	-> target answered  (ok)   NEG SYNC  sdtr   set    -      clrack
**	-> target answered (!ok)   NEG SYNC  sdtr   defa.  REJ--->msg_bad
**	-> target answered  (ok)   NEG WIDE  wdtr   set    -      clrack
**	-> target answered (!ok)   NEG WIDE  wdtr   defa.  REJ--->msg_bad
**	-> any other msgin	   NEG FAIL  noop   defa.  -      dispatch
**
**	Target tries to negotiate:
**	-> incoming message	   --- SYNC  sdtr   set    SDTR   -
**	-> incoming message	   --- WIDE  wdtr   set    WDTR   -
**      We sent our answer:
**	-> target doesn't msgout   --- PROTO ?      defa.  -      dispatch
**
**-----------------------------------------------------------------------------
*/

	case SIR_NEGO_FAILED:
		/*-------------------------------------------------------
		**
		**	Negotiation failed.
		**	Target doesn't send an answer message,
		**	or target rejected our message.
		**
		**      Remove negotiation request.
		**
		**-------------------------------------------------------
		*/
		OUTB (HS_PRT, HS_BUSY);

		/* fall through */

	case SIR_NEGO_PROTO:
		/*-------------------------------------------------------
		**
		**	Negotiation failed.
		**	Target doesn't fetch the answer message.
		**
		**-------------------------------------------------------
		*/

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->cmd);
			printk ("negotiation failed sir=%x status=%x.\n",
				num, cp->nego_status);
		};

		/*
		**	any error in negotiation:
		**	fall back to default mode.
		*/
		switch (cp->nego_status) {

		case NS_SYNC:
			ncr_setsync (np, cp, 0, 0xe0);
			break;

		case NS_WIDE:
			ncr_setwide (np, cp, 0, 0);
			break;

		};
		np->msgin [0] = M_NOOP;
		np->msgout[0] = M_NOOP;
		cp->nego_status = 0;
		break;

	case SIR_NEGO_SYNC:
		/*
		**	Synchronous request message received.
		*/

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->cmd);
			printk ("sync msgin: ");
			(void) ncr_show_msg (np->msgin);
			printk (".\n");
		};

		/*
		**	get requested values.
		*/

		chg = 0;
		per = np->msgin[3];
		ofs = np->msgin[4];
		if (ofs==0) per=255;

		/*
		**      if target sends SDTR message,
		**	      it CAN transfer synch.
		*/

		if (ofs)
			tp->inq_byte7 |= INQ7_SYNC;

		/*
		**	check values against driver limits.
		*/

		if (per < np->minsync)
			{chg = 1; per = np->minsync;}
		if (per < tp->minsync)
			{chg = 1; per = tp->minsync;}
		if (ofs > tp->maxoffs)
			{chg = 1; ofs = tp->maxoffs;}

		/*
		**	Check against controller limits.
		*/
		fak	= 7;
		scntl3	= 0;
		if (ofs != 0) {
			ncr_getsync(np, per, &fak, &scntl3);
			if (fak > 7) {
				chg = 1;
				ofs = 0;
			}
		}
		if (ofs == 0) {
			fak	= 7;
			per	= 0;
			scntl3	= 0;
			tp->minsync = 0;
		}

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->cmd);
			printk ("sync: per=%d scntl3=0x%x ofs=%d fak=%d chg=%d.\n",
				per, scntl3, ofs, fak, chg);
		}

		if (INB (HS_PRT) == HS_NEGOTIATE) {
			OUTB (HS_PRT, HS_BUSY);
			switch (cp->nego_status) {

			case NS_SYNC:
				/*
				**      This was an answer message
				*/
				if (chg) {
					/*
					**	Answer wasn't acceptable.
					*/
					ncr_setsync (np, cp, 0, 0xe0);
					OUTL_DSP (NCB_SCRIPT_PHYS (np, msg_bad));
				} else {
					/*
					**	Answer is ok.
					*/
					ncr_setsync (np, cp, scntl3, (fak<<5)|ofs);
					OUTL_DSP (NCB_SCRIPT_PHYS (np, clrack));
				};
				return;

			case NS_WIDE:
				ncr_setwide (np, cp, 0, 0);
				break;
			};
		};

		/*
		**	It was a request. Set value and
		**      prepare an answer message
		*/

		ncr_setsync (np, cp, scntl3, (fak<<5)|ofs);

		np->msgout[0] = M_EXTENDED;
		np->msgout[1] = 3;
		np->msgout[2] = M_X_SYNC_REQ;
		np->msgout[3] = per;
		np->msgout[4] = ofs;

		cp->nego_status = NS_SYNC;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->cmd);
			printk ("sync msgout: ");
			(void) ncr_show_msg (np->msgout);
			printk (".\n");
		}

		if (!ofs) {
			OUTL_DSP (NCB_SCRIPT_PHYS (np, msg_bad));
			return;
		}
		np->msgin [0] = M_NOOP;

		break;

	case SIR_NEGO_WIDE:
		/*
		**	Wide request message received.
		*/
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->cmd);
			printk ("wide msgin: ");
			(void) ncr_show_msg (np->msgin);
			printk (".\n");
		};

		/*
		**	get requested values.
		*/

		chg  = 0;
		wide = np->msgin[3];

		/*
		**      if target sends WDTR message,
		**	      it CAN transfer wide.
		*/

		if (wide)
			tp->inq_byte7 |= INQ7_WIDE16;

		/*
		**	check values against driver limits.
		*/

		if (wide > tp->usrwide)
			{chg = 1; wide = tp->usrwide;}

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->cmd);
			printk ("wide: wide=%d chg=%d.\n", wide, chg);
		}

		if (INB (HS_PRT) == HS_NEGOTIATE) {
			OUTB (HS_PRT, HS_BUSY);
			switch (cp->nego_status) {

			case NS_WIDE:
				/*
				**      This was an answer message
				*/
				if (chg) {
					/*
					**	Answer wasn't acceptable.
					*/
					ncr_setwide (np, cp, 0, 1);
					OUTL_DSP (NCB_SCRIPT_PHYS (np, msg_bad));
				} else {
					/*
					**	Answer is ok.
					*/
					ncr_setwide (np, cp, wide, 1);
					OUTL_DSP (NCB_SCRIPT_PHYS (np, clrack));
				};
				return;

			case NS_SYNC:
				ncr_setsync (np, cp, 0, 0xe0);
				break;
			};
		};

		/*
		**	It was a request, set value and
		**      prepare an answer message
		*/

		ncr_setwide (np, cp, wide, 1);

		np->msgout[0] = M_EXTENDED;
		np->msgout[1] = 2;
		np->msgout[2] = M_X_WIDE_REQ;
		np->msgout[3] = wide;

		np->msgin [0] = M_NOOP;

		cp->nego_status = NS_WIDE;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->cmd);
			printk ("wide msgout: ");
			(void) ncr_show_msg (np->msgin);
			printk (".\n");
		}
		break;

/*--------------------------------------------------------------------
**
**	Processing of special messages
**
**--------------------------------------------------------------------
*/

	case SIR_REJECT_RECEIVED:
		/*-----------------------------------------------
		**
		**	We received a M_REJECT message.
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->cmd);
		printk ("M_REJECT received (%x:%x).\n",
			(unsigned)scr_to_cpu(np->lastmsg), np->msgout[0]);
		break;

	case SIR_REJECT_SENT:
		/*-----------------------------------------------
		**
		**	We received an unknown message
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->cmd);
		printk ("M_REJECT sent for ");
		(void) ncr_show_msg (np->msgin);
		printk (".\n");
		break;

/*--------------------------------------------------------------------
**
**	Processing of special messages
**
**--------------------------------------------------------------------
*/

	case SIR_IGN_RESIDUE:
		/*-----------------------------------------------
		**
		**	We received an IGNORE RESIDUE message,
		**	which couldn't be handled by the script.
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->cmd);
		printk ("M_IGN_RESIDUE received, but not yet implemented.\n");
		break;
#if 0
	case SIR_MISSING_SAVE:
		/*-----------------------------------------------
		**
		**	We received an DISCONNECT message,
		**	but the datapointer wasn't saved before.
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->cmd);
		printk ("M_DISCONNECT received, but datapointer not saved: "
			"data=%x save=%x goal=%x.\n",
			(unsigned) INL (nc_temp),
			(unsigned) scr_to_cpu(np->header.savep),
			(unsigned) scr_to_cpu(np->header.goalp));
		break;
#endif
	};

out:
	OUTONB_STD ();
}

/*==========================================================
**
**
**	Acquire a control block
**
**
**==========================================================
*/

static	ccb_p ncr_get_ccb (ncb_p np, u_char tn, u_char ln)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = tp->lp[ln];
	u_char tag = NO_TAG;
	ccb_p cp = (ccb_p) 0;

	/*
	**	Lun structure available ?
	*/
	if (lp) {
		XPT_QUEHEAD *qp;
		/*
		**	Keep from using more tags than we can handle.
		*/
		if (lp->usetags && lp->busyccbs >= lp->maxnxs)
			return (ccb_p) 0;

		/*
		**	Allocate a new CCB if needed.
		*/
		if (xpt_que_empty(&lp->free_ccbq))
			ncr_alloc_ccb(np, tn, ln);

		/*
		**	Tune tag mode if asked by user.
		*/
		if (lp->queuedepth != lp->numtags) {
			ncr_setup_tags(np, tn, ln);
		}
			
		/*
		**	Look for free CCB
		*/
		qp = xpt_remque_head(&lp->free_ccbq);
		if (qp) {
			cp = xpt_que_entry(qp, struct ccb, link_ccbq);
			if (cp->magic) {
				PRINT_LUN(np, tn, ln);
				printk ("ccb free list corrupted (@%p)\n", cp);
				cp = 0;
			}
			else {
				xpt_insque_tail(qp, &lp->wait_ccbq);
				++lp->busyccbs;
			}
		}

		/*
		**	If a CCB is available,
		**	Get a tag for this nexus if required.
		*/
		if (cp) {
			if (lp->usetags)
				tag = lp->cb_tags[lp->ia_tag];
		}
		else if (lp->actccbs > 0)
			return (ccb_p) 0;
	}

	/*
	**	if nothing available, take the default.
	*/
	if (!cp)
		cp = np->ccb;

	/*
	**	Wait until available.
	*/
#if 0
	while (cp->magic) {
		if (flags & SCSI_NOSLEEP) break;
		if (tsleep ((caddr_t)cp, PRIBIO|PCATCH, "ncr", 0))
			break;
	};
#endif

	if (cp->magic)
		return ((ccb_p) 0);

	cp->magic = 1;

	/*
	**	Move to next available tag if tag used.
	*/
	if (lp) {
		if (tag != NO_TAG) {
			++lp->ia_tag;
			if (lp->ia_tag == MAX_TAGS)
				lp->ia_tag = 0;
			lp->tags_umap |= (((tagmap_t) 1) << tag);
		}
	}

	/*
	**	Remember all informations needed to free this CCB.
	*/
	cp->tag	   = tag;
	cp->target = tn;
	cp->lun    = ln;

	if (DEBUG_FLAGS & DEBUG_TAGS) {
		PRINT_LUN(np, tn, ln);
		printk ("ccb @%p using tag %d.\n", cp, tag);
	}

	return cp;
}

/*==========================================================
**
**
**	Release one control block
**
**
**==========================================================
*/

static void ncr_free_ccb (ncb_p np, ccb_p cp)
{
	tcb_p tp = &np->target[cp->target];
	lcb_p lp = tp->lp[cp->lun];

	if (DEBUG_FLAGS & DEBUG_TAGS) {
		PRINT_LUN(np, cp->target, cp->lun);
		printk ("ccb @%p freeing tag %d.\n", cp, cp->tag);
	}

	/*
	**	If lun control block available,
	**	decrement active commands and increment credit, 
	**	free the tag if any and remove the JUMP for reselect.
	*/
	if (lp) {
		if (cp->tag != NO_TAG) {
			lp->cb_tags[lp->if_tag++] = cp->tag;
			if (lp->if_tag == MAX_TAGS)
				lp->if_tag = 0;
			lp->tags_umap &= ~(((tagmap_t) 1) << cp->tag);
			lp->tags_smap &= lp->tags_umap;
			lp->jump_ccb[cp->tag] =
				cpu_to_scr(NCB_SCRIPTH_PHYS(np, bad_i_t_l_q));
		} else {
			lp->jump_ccb[0] =
				cpu_to_scr(NCB_SCRIPTH_PHYS(np, bad_i_t_l));
		}
	}

	/*
	**	Make this CCB available.
	*/

	if (lp) {
		if (cp != np->ccb) {
			xpt_remque(&cp->link_ccbq);
			xpt_insque_head(&cp->link_ccbq, &lp->free_ccbq);
		}
		--lp->busyccbs;
		if (cp->queued) {
			--lp->queuedccbs;
		}
	}
	cp -> host_status = HS_IDLE;
	cp -> magic = 0;
	if (cp->queued) {
		--np->queuedccbs;
		cp->queued = 0;
	}

#if 0
	if (cp == np->ccb)
		wakeup ((caddr_t) cp);
#endif
}


#define ncr_reg_bus_addr(r) (np->paddr + offsetof (struct ncr_reg, r))

/*------------------------------------------------------------------------
**	Initialize the fixed part of a CCB structure.
**------------------------------------------------------------------------
**------------------------------------------------------------------------
*/
static void ncr_init_ccb(ncb_p np, ccb_p cp)
{
	ncrcmd copy_4 = np->features & FE_PFEN ? SCR_COPY(4) : SCR_COPY_F(4);

	/*
	**	Remember virtual and bus address of this ccb.
	*/
	cp->p_ccb 	   = vtobus(cp);
	cp->phys.header.cp = cp;

	/*
	**	This allows xpt_remque to work for the default ccb.
	*/
	xpt_que_init(&cp->link_ccbq);

	/*
	**	Initialyze the start and restart launch script.
	**
	**	COPY(4) @(...p_phys), @(dsa)
	**	JUMP @(sched_point)
	*/
	cp->start.setup_dsa[0]	 = cpu_to_scr(copy_4);
	cp->start.setup_dsa[1]	 = cpu_to_scr(CCB_PHYS(cp, start.p_phys));
	cp->start.setup_dsa[2]	 = cpu_to_scr(ncr_reg_bus_addr(nc_dsa));
	cp->start.schedule.l_cmd = cpu_to_scr(SCR_JUMP);
	cp->start.p_phys	 = cpu_to_scr(CCB_PHYS(cp, phys));

	bcopy(&cp->start, &cp->restart, sizeof(cp->restart));

	cp->start.schedule.l_paddr   = cpu_to_scr(NCB_SCRIPT_PHYS (np, idle));
	cp->restart.schedule.l_paddr = cpu_to_scr(NCB_SCRIPTH_PHYS (np, abort));
}


/*------------------------------------------------------------------------
**	Allocate a CCB and initialize its fixed part.
**------------------------------------------------------------------------
**------------------------------------------------------------------------
*/
static void ncr_alloc_ccb(ncb_p np, u_char tn, u_char ln)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = tp->lp[ln];
	ccb_p cp = 0;

	/*
	**	Allocate memory for this CCB.
	*/
	cp = m_calloc_dma(sizeof(struct ccb), "CCB");
	if (!cp)
		return;

	/*
	**	Count it and initialyze it.
	*/
	lp->actccbs++;
	np->actccbs++;
	bzero (cp, sizeof (*cp));
	ncr_init_ccb(np, cp);

	/*
	**	Chain into wakeup list and free ccb queue and take it 
	**	into account for tagged commands.
	*/
	cp->link_ccb      = np->ccb->link_ccb;
	np->ccb->link_ccb = cp;

	xpt_insque_head(&cp->link_ccbq, &lp->free_ccbq);
	ncr_setup_tags (np, tn, ln);
}

/*==========================================================
**
**
**      Allocation of resources for Targets/Luns/Tags.
**
**
**==========================================================
*/


/*------------------------------------------------------------------------
**	Target control block initialisation.
**------------------------------------------------------------------------
**	This data structure is fully initialized after a SCSI command 
**	has been successfully completed for this target.
**	It contains a SCRIPT that is called on target reselection.
**------------------------------------------------------------------------
*/
static void ncr_init_tcb (ncb_p np, u_char tn)
{
	tcb_p tp = &np->target[tn];
	ncrcmd copy_1 = np->features & FE_PFEN ? SCR_COPY(1) : SCR_COPY_F(1);
	int th = tn & 3;
	int i;

	/*
	**	Jump to next tcb if SFBR does not match this target.
	**	JUMP  IF (SFBR != #target#), @(next tcb)
	*/
	tp->jump_tcb.l_cmd   =
		cpu_to_scr((SCR_JUMP ^ IFFALSE (DATA (0x80 + tn))));
	tp->jump_tcb.l_paddr = np->jump_tcb[th].l_paddr;

	/*
	**	Load the synchronous transfer register.
	**	COPY @(tp->sval), @(sxfer)
	*/
	tp->getscr[0] =	cpu_to_scr(copy_1);
	tp->getscr[1] = cpu_to_scr(vtobus (&tp->sval));
	tp->getscr[2] = cpu_to_scr(ncr_reg_bus_addr(nc_sxfer));
  
	/*
	**	Load the timing register.
	**	COPY @(tp->wval), @(scntl3)
	*/
	tp->getscr[3] =	cpu_to_scr(copy_1);
	tp->getscr[4] = cpu_to_scr(vtobus (&tp->wval));
	tp->getscr[5] = cpu_to_scr(ncr_reg_bus_addr(nc_scntl3));

	/*
	**	Get the IDENTIFY message and the lun.
	**	CALL @script(resel_lun)
	*/
	tp->call_lun.l_cmd   = cpu_to_scr(SCR_CALL);
	tp->call_lun.l_paddr = cpu_to_scr(NCB_SCRIPT_PHYS (np, resel_lun));

	/*
	**	Look for the lun control block of this nexus.
	**	For i = 0 to 3
	**		JUMP ^ IFTRUE (MASK (i, 3)), @(next_lcb)
	*/
	for (i = 0 ; i < 4 ; i++) {
		tp->jump_lcb[i].l_cmd   =
				cpu_to_scr((SCR_JUMP ^ IFTRUE (MASK (i, 3))));
		tp->jump_lcb[i].l_paddr =
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, bad_identify));
	}

	/*
	**	Link this target control block to the JUMP chain.
	*/
	np->jump_tcb[th].l_paddr = cpu_to_scr(vtobus (&tp->jump_tcb));

	/*
	**	These assert's should be moved at driver initialisations.
	*/
	assert (( (offsetof(struct ncr_reg, nc_sxfer) ^
		offsetof(struct tcb    , sval    )) &3) == 0);
	assert (( (offsetof(struct ncr_reg, nc_scntl3) ^
		offsetof(struct tcb    , wval    )) &3) == 0);
}


/*------------------------------------------------------------------------
**	Lun control block allocation and initialization.
**------------------------------------------------------------------------
**	This data structure is allocated and initialized after a SCSI 
**	command has been successfully completed for this target/lun.
**------------------------------------------------------------------------
*/
static lcb_p ncr_alloc_lcb (ncb_p np, u_char tn, u_char ln)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = tp->lp[ln];
	ncrcmd copy_4 = np->features & FE_PFEN ? SCR_COPY(4) : SCR_COPY_F(4);
	int lh = ln & 3;

	/*
	**	Already done, return.
	*/
	if (lp)
		return lp;

	/*
	**	Allocate the lcb.
	*/
	lp = m_calloc_dma(sizeof(struct lcb), "LCB");
	if (!lp)
		goto fail;
	bzero(lp, sizeof(*lp));
	tp->lp[ln] = lp;

	/*
	**	Initialize the target control block if not yet.
	*/
	if (!tp->jump_tcb.l_cmd)
		ncr_init_tcb(np, tn);

	/*
	**	Initialize the CCB queue headers.
	*/
	xpt_que_init(&lp->free_ccbq);
	xpt_que_init(&lp->busy_ccbq);
	xpt_que_init(&lp->wait_ccbq);
	xpt_que_init(&lp->skip_ccbq);

	/*
	**	Set max CCBs to 1 and use the default 1 entry 
	**	jump table by default.
	*/
	lp->maxnxs	= 1;
	lp->jump_ccb	= &lp->jump_ccb_0;
	lp->p_jump_ccb	= cpu_to_scr(vtobus(lp->jump_ccb));

	/*
	**	Initilialyze the reselect script:
	**
	**	Jump to next lcb if SFBR does not match this lun.
	**	Load TEMP with the CCB direct jump table bus address.
	**	Get the SIMPLE TAG message and the tag.
	**
	**	JUMP  IF (SFBR != #lun#), @(next lcb)
	**	COPY @(lp->p_jump_ccb),	  @(temp)
	**	JUMP @script(resel_notag)
	*/
	lp->jump_lcb.l_cmd   =
		cpu_to_scr((SCR_JUMP ^ IFFALSE (MASK (0x80+ln, 0xff))));
	lp->jump_lcb.l_paddr = tp->jump_lcb[lh].l_paddr;

	lp->load_jump_ccb[0] = cpu_to_scr(copy_4);
	lp->load_jump_ccb[1] = cpu_to_scr(vtobus (&lp->p_jump_ccb));
	lp->load_jump_ccb[2] = cpu_to_scr(ncr_reg_bus_addr(nc_temp));

	lp->jump_tag.l_cmd   = cpu_to_scr(SCR_JUMP);
	lp->jump_tag.l_paddr = cpu_to_scr(NCB_SCRIPT_PHYS (np, resel_notag));

	/*
	**	Link this lun control block to the JUMP chain.
	*/
	tp->jump_lcb[lh].l_paddr = cpu_to_scr(vtobus (&lp->jump_lcb));

	/*
	**	Initialize command queuing control.
	*/
	lp->busyccbs	= 1;
	lp->queuedccbs	= 1;
	lp->queuedepth	= 1;
fail:
	return lp;
}


/*------------------------------------------------------------------------
**	Lun control block setup on INQUIRY data received.
**------------------------------------------------------------------------
**	We only support WIDE, SYNC for targets and CMDQ for logical units.
**	This setup is done on each INQUIRY since we are expecting user 
**	will play with CHANGE DEFINITION commands. :-)
**------------------------------------------------------------------------
*/
static lcb_p ncr_setup_lcb (ncb_p np, u_char tn, u_char ln, u_char *inq_data)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = tp->lp[ln];
	u_char inq_byte7;

	/*
	**	If no lcb, try to allocate it.
	*/
	if (!lp && !(lp = ncr_alloc_lcb(np, tn, ln)))
		goto fail;

	/*
	**	Get device quirks from a speciality table.
	*/
	tp->quirks = ncr_lookup (inq_data);
	if (tp->quirks && bootverbose) {
		PRINT_LUN(np, tn, ln);
		printk ("quirks=%x.\n", tp->quirks);
	}

	/*
	**	Evaluate trustable target/unit capabilities.
	**	We only believe device version >= SCSI-2 that 
	**	use appropriate response data format (2).
	**	But it seems that some CCS devices also 
	**	support SYNC and I donnot want to frustrate 
	**	anybody. ;-)
	*/
	inq_byte7 = 0;
	if	((inq_data[2] & 0x7) >= 2 && (inq_data[3] & 0xf) == 2)
		inq_byte7 = inq_data[7];
	else if ((inq_data[2] & 0x7) == 1 && (inq_data[3] & 0xf) == 1)
		inq_byte7 = INQ7_SYNC;

	/*
	**	Throw away announced LUN capabilities if we are told 
	**	that there is no real device supported by the logical unit.
	*/
	if ((inq_data[0] & 0xe0) > 0x20 || (inq_data[0] & 0x1f) == 0x1f)
		inq_byte7 &= (INQ7_SYNC | INQ7_WIDE16);

	/*
	**	If user is wanting SYNC, force this feature.
	*/
	if (driver_setup.force_sync_nego)
		inq_byte7 |= INQ7_SYNC;

	/*
	**	Prepare negotiation if SIP capabilities have changed.
	*/
	tp->inq_done = 1;
	if ((inq_byte7 ^ tp->inq_byte7) & (INQ7_SYNC | INQ7_WIDE16)) {
		tp->inq_byte7 = inq_byte7;
		ncr_negotiate(np, tp);
	}

	/*
	**	If unit supports tagged commands, allocate the 
	**	CCB JUMP table if not yet.
	*/
	if ((inq_byte7 & INQ7_QUEUE) && lp->jump_ccb == &lp->jump_ccb_0) {
		int i;
		lp->jump_ccb = m_calloc_dma(256, "JUMP_CCB");
		if (!lp->jump_ccb) {
			lp->jump_ccb = &lp->jump_ccb_0;
			goto fail;
		}
		lp->p_jump_ccb = cpu_to_scr(vtobus(lp->jump_ccb));
		for (i = 0 ; i < 64 ; i++)
			lp->jump_ccb[i] =
				cpu_to_scr(NCB_SCRIPTH_PHYS (np, bad_i_t_l_q));
		for (i = 0 ; i < MAX_TAGS ; i++)
			lp->cb_tags[i] = i;
		lp->maxnxs = MAX_TAGS;
		lp->tags_stime = ktime_get(3*HZ);
	}

	/*
	**	Adjust tagged queueing status if needed.
	*/
	if ((inq_byte7 ^ lp->inq_byte7) & INQ7_QUEUE) {
		lp->inq_byte7 = inq_byte7;
		lp->numtags   = lp->maxtags;
		ncr_setup_tags (np, tn, ln);
	}

fail:
	return lp;
}

/*==========================================================
**
**
**	Build Scatter Gather Block
**
**
**==========================================================
**
**	The transfer area may be scattered among
**	several non adjacent physical pages.
**
**	We may use MAX_SCATTER blocks.
**
**----------------------------------------------------------
*/

/*
**	We try to reduce the number of interrupts caused
**	by unexpected phase changes due to disconnects.
**	A typical harddisk may disconnect before ANY block.
**	If we wanted to avoid unexpected phase changes at all
**	we had to use a break point every 512 bytes.
**	Of course the number of scatter/gather blocks is
**	limited.
**	Under Linux, the scatter/gatter blocks are provided by 
**	the generic driver. We just have to copy addresses and 
**	sizes to the data segment array.
*/

static	int	ncr_scatter(ncb_p np, ccb_p cp, Scsi_Cmnd *cmd)
{
	struct scr_tblmove *data;
	int segment	= 0;
	int use_sg	= (int) cmd->use_sg;

	data		= cp->phys.data;
	cp->data_len	= 0;

	if (!use_sg) {
		if (cmd->request_bufflen) {
			u_long baddr = map_scsi_single_data(np, cmd);

			data = &data[MAX_SCATTER - 1];
			data[0].addr = cpu_to_scr(baddr);
			data[0].size = cpu_to_scr(cmd->request_bufflen);
			cp->data_len = cmd->request_bufflen;
			segment = 1;
		}
	}
	else if (use_sg <= MAX_SCATTER) {
		struct scatterlist *scatter = (struct scatterlist *)cmd->buffer;

		use_sg = map_scsi_sg_data(np, cmd);
		data = &data[MAX_SCATTER - use_sg];

		while (segment < use_sg) {
			u_long baddr = scsi_sg_dma_address(&scatter[segment]);
			unsigned int len = scsi_sg_dma_len(&scatter[segment]);

			data[segment].addr = cpu_to_scr(baddr);
			data[segment].size = cpu_to_scr(len);
			cp->data_len	  += len;
			++segment;
		}
	}
	else {
		return -1;
	}

	return segment;
}

/*==========================================================
**
**
**	Test the pci bus snoop logic :-(
**
**	Has to be called with interrupts disabled.
**
**
**==========================================================
*/

#ifndef SCSI_NCR_IOMAPPED
static int __init ncr_regtest (struct ncb* np)
{
	register volatile u_int32 data;
	/*
	**	ncr registers may NOT be cached.
	**	write 0xffffffff to a read only register area,
	**	and try to read it back.
	*/
	data = 0xffffffff;
	OUTL_OFF(offsetof(struct ncr_reg, nc_dstat), data);
	data = INL_OFF(offsetof(struct ncr_reg, nc_dstat));
#if 1
	if (data == 0xffffffff) {
#else
	if ((data & 0xe2f0fffd) != 0x02000080) {
#endif
		printk ("CACHE TEST FAILED: reg dstat-sstat2 readback %x.\n",
			(unsigned) data);
		return (0x10);
	};
	return (0);
}
#endif

static int __init ncr_snooptest (struct ncb* np)
{
	u_int32	ncr_rd, ncr_wr, ncr_bk, host_rd, host_wr, pc;
	int	i, err=0;
#ifndef SCSI_NCR_IOMAPPED
	if (np->reg) {
            err |= ncr_regtest (np);
            if (err) return (err);
	}
#endif
	/*
	**	init
	*/
	pc  = NCB_SCRIPTH_PHYS (np, snooptest);
	host_wr = 1;
	ncr_wr  = 2;
	/*
	**	Set memory and register.
	*/
	np->ncr_cache = cpu_to_scr(host_wr);
	OUTL (nc_temp, ncr_wr);
	/*
	**	Start script (exchange values)
	*/
	OUTL_DSP (pc);
	/*
	**	Wait 'til done (with timeout)
	*/
	for (i=0; i<NCR_SNOOP_TIMEOUT; i++)
		if (INB(nc_istat) & (INTF|SIP|DIP))
			break;
	/*
	**	Save termination position.
	*/
	pc = INL (nc_dsp);
	/*
	**	Read memory and register.
	*/
	host_rd = scr_to_cpu(np->ncr_cache);
	ncr_rd  = INL (nc_scratcha);
	ncr_bk  = INL (nc_temp);
	/*
	**	Reset ncr chip
	*/
	OUTB (nc_istat,  SRST);
	UDELAY (100);
	OUTB (nc_istat,  0   );
	/*
	**	check for timeout
	*/
	if (i>=NCR_SNOOP_TIMEOUT) {
		printk ("CACHE TEST FAILED: timeout.\n");
		return (0x20);
	};
	/*
	**	Check termination position.
	*/
	if (pc != NCB_SCRIPTH_PHYS (np, snoopend)+8) {
		printk ("CACHE TEST FAILED: script execution failed.\n");
		printk ("start=%08lx, pc=%08lx, end=%08lx\n", 
			(u_long) NCB_SCRIPTH_PHYS (np, snooptest), (u_long) pc,
			(u_long) NCB_SCRIPTH_PHYS (np, snoopend) +8);
		return (0x40);
	};
	/*
	**	Show results.
	*/
	if (host_wr != ncr_rd) {
		printk ("CACHE TEST FAILED: host wrote %d, ncr read %d.\n",
			(int) host_wr, (int) ncr_rd);
		err |= 1;
	};
	if (host_rd != ncr_wr) {
		printk ("CACHE TEST FAILED: ncr wrote %d, host read %d.\n",
			(int) ncr_wr, (int) host_rd);
		err |= 2;
	};
	if (ncr_bk != ncr_wr) {
		printk ("CACHE TEST FAILED: ncr wrote %d, read back %d.\n",
			(int) ncr_wr, (int) ncr_bk);
		err |= 4;
	};
	return (err);
}

/*==========================================================
**
**
**	Device lookup.
**
**	@GENSCSI@ should be integrated to scsiconf.c
**
**
**==========================================================
*/

struct table_entry {
	char *	manufacturer;
	char *	model;
	char *	version;
	u_long	info;
};

static struct table_entry device_tab[] =
{
#if 0
	{"", "", "", QUIRK_NOMSG},
#endif
	{"SONY", "SDT-5000", "3.17", QUIRK_NOMSG},
	{"WangDAT", "Model 2600", "01.7", QUIRK_NOMSG},
	{"WangDAT", "Model 3200", "02.2", QUIRK_NOMSG},
	{"WangDAT", "Model 1300", "02.4", QUIRK_NOMSG},
	{"", "", "", 0} /* catch all: must be last entry. */
};

static u_long ncr_lookup(char * id)
{
	struct table_entry * p = device_tab;
	char *d, *r, c;

	for (;;p++) {

		d = id+8;
		r = p->manufacturer;
		while ((c=*r++)) if (c!=*d++) break;
		if (c) continue;

		d = id+16;
		r = p->model;
		while ((c=*r++)) if (c!=*d++) break;
		if (c) continue;

		d = id+32;
		r = p->version;
		while ((c=*r++)) if (c!=*d++) break;
		if (c) continue;

		return (p->info);
	}
}

/*==========================================================
**
**	Determine the ncr's clock frequency.
**	This is essential for the negotiation
**	of the synchronous transfer rate.
**
**==========================================================
**
**	Note: we have to return the correct value.
**	THERE IS NO SAVE DEFAULT VALUE.
**
**	Most NCR/SYMBIOS boards are delivered with a 40 Mhz clock.
**	53C860 and 53C875 rev. 1 support fast20 transfers but 
**	do not have a clock doubler and so are provided with a 
**	80 MHz clock. All other fast20 boards incorporate a doubler 
**	and so should be delivered with a 40 MHz clock.
**	The future fast40 chips (895/895) use a 40 Mhz base clock 
**	and provide a clock quadrupler (160 Mhz). The code below 
**	tries to deal as cleverly as possible with all this stuff.
**
**----------------------------------------------------------
*/

/*
 *	Select NCR SCSI clock frequency
 */
static void ncr_selectclock(ncb_p np, u_char scntl3)
{
	if (np->multiplier < 2) {
		OUTB(nc_scntl3,	scntl3);
		return;
	}

	if (bootverbose >= 2)
		printk ("%s: enabling clock multiplier\n", ncr_name(np));

	OUTB(nc_stest1, DBLEN);	   /* Enable clock multiplier		  */
	if (np->multiplier > 2) {  /* Poll bit 5 of stest4 for quadrupler */
		int i = 20;
		while (!(INB(nc_stest4) & LCKFRQ) && --i > 0)
			UDELAY (20);
		if (!i)
			printk("%s: the chip cannot lock the frequency\n", ncr_name(np));
	} else			/* Wait 20 micro-seconds for doubler	*/
		UDELAY (20);
	OUTB(nc_stest3, HSC);		/* Halt the scsi clock		*/
	OUTB(nc_scntl3,	scntl3);
	OUTB(nc_stest1, (DBLEN|DBLSEL));/* Select clock multiplier	*/
	OUTB(nc_stest3, 0x00);		/* Restart scsi clock 		*/
}


/*
 *	calculate NCR SCSI clock frequency (in KHz)
 */
static unsigned __init ncrgetfreq (ncb_p np, int gen)
{
	unsigned ms = 0;
	char count = 0;

	/*
	 * Measure GEN timer delay in order 
	 * to calculate SCSI clock frequency
	 *
	 * This code will never execute too
	 * many loop iterations (if DELAY is 
	 * reasonably correct). It could get
	 * too low a delay (too high a freq.)
	 * if the CPU is slow executing the 
	 * loop for some reason (an NMI, for
	 * example). For this reason we will
	 * if multiple measurements are to be 
	 * performed trust the higher delay 
	 * (lower frequency returned).
	 */
	OUTB (nc_stest1, 0);	/* make sure clock doubler is OFF */
	OUTW (nc_sien , 0);	/* mask all scsi interrupts */
	(void) INW (nc_sist);	/* clear pending scsi interrupt */
	OUTB (nc_dien , 0);	/* mask all dma interrupts */
	(void) INW (nc_sist);	/* another one, just to be sure :) */
	OUTB (nc_scntl3, 4);	/* set pre-scaler to divide by 3 */
	OUTB (nc_stime1, 0);	/* disable general purpose timer */
	OUTB (nc_stime1, gen);	/* set to nominal delay of 1<<gen * 125us */
	while (!(INW(nc_sist) & GEN) && ms++ < 100000) {
		for (count = 0; count < 10; count ++)
			UDELAY (100);	/* count ms */
	}
	OUTB (nc_stime1, 0);	/* disable general purpose timer */
 	/*
 	 * set prescaler to divide by whatever 0 means
 	 * 0 ought to choose divide by 2, but appears
 	 * to set divide by 3.5 mode in my 53c810 ...
 	 */
 	OUTB (nc_scntl3, 0);

	if (bootverbose >= 2)
		printk ("%s: Delay (GEN=%d): %u msec\n", ncr_name(np), gen, ms);
  	/*
 	 * adjust for prescaler, and convert into KHz 
  	 */
	return ms ? ((1 << gen) * 4340) / ms : 0;
}

/*
 *	Get/probe NCR SCSI clock frequency
 */
static void __init ncr_getclock (ncb_p np, int mult)
{
	unsigned char scntl3 = INB(nc_scntl3);
	unsigned char stest1 = INB(nc_stest1);
	unsigned f1;

	np->multiplier = 1;
	f1 = 40000;

	/*
	**	True with 875 or 895 with clock multiplier selected
	*/
	if (mult > 1 && (stest1 & (DBLEN+DBLSEL)) == DBLEN+DBLSEL) {
		if (bootverbose >= 2)
			printk ("%s: clock multiplier found\n", ncr_name(np));
		np->multiplier = mult;
	}

	/*
	**	If multiplier not found or scntl3 not 7,5,3,
	**	reset chip and get frequency from general purpose timer.
	**	Otherwise trust scntl3 BIOS setting.
	*/
	if (np->multiplier != mult || (scntl3 & 7) < 3 || !(scntl3 & 1)) {
		unsigned f2;

		OUTB(nc_istat, SRST); UDELAY (5); OUTB(nc_istat, 0);

		(void) ncrgetfreq (np, 11);	/* throw away first result */
		f1 = ncrgetfreq (np, 11);
		f2 = ncrgetfreq (np, 11);

		if (bootverbose)
			printk ("%s: NCR clock is %uKHz, %uKHz\n", ncr_name(np), f1, f2);

		if (f1 > f2) f1 = f2;		/* trust lower result	*/

		if	(f1 <	45000)		f1 =  40000;
		else if (f1 <	55000)		f1 =  50000;
		else				f1 =  80000;

		if (f1 < 80000 && mult > 1) {
			if (bootverbose >= 2)
				printk ("%s: clock multiplier assumed\n", ncr_name(np));
			np->multiplier	= mult;
		}
	} else {
		if	((scntl3 & 7) == 3)	f1 =  40000;
		else if	((scntl3 & 7) == 5)	f1 =  80000;
		else 				f1 = 160000;

		f1 /= np->multiplier;
	}

	/*
	**	Compute controller synchronous parameters.
	*/
	f1		*= np->multiplier;
	np->clock_khz	= f1;
}

/*===================== LINUX ENTRY POINTS SECTION ==========================*/

/*
**   Linux select queue depths function
*/

static void ncr53c8xx_select_queue_depths(struct Scsi_Host *host, struct scsi_device *devlist)
{
	struct scsi_device *device;

	for (device = devlist; device; device = device->next) {
		ncb_p np;
		tcb_p tp;
		lcb_p lp;
		int numtags;

		if (device->host != host)
			continue;

		np = ((struct host_data *) host->hostdata)->ncb;
		tp = &np->target[device->id];
		lp = tp->lp[device->lun];

		/*
		**	Select queue depth from driver setup.
		**	Donnot use more than configured by user.
		**	Use at least 2.
		**	Donnot use more than our maximum.
		*/
		numtags = device_queue_depth(np->unit, device->id, device->lun);
		if (numtags > tp->usrtags)
			numtags = tp->usrtags;
		if (!device->tagged_supported)
			numtags = 1;
		device->queue_depth = numtags;
		if (device->queue_depth < 2)
			device->queue_depth = 2;
		if (device->queue_depth > MAX_TAGS)
			device->queue_depth = MAX_TAGS;

		/*
		**	Since the queue depth is not tunable under Linux,
		**	we need to know this value in order not to 
		**	announce stupid things to user.
		*/
		if (lp) {
			lp->numtags = lp->maxtags = numtags;
			lp->scdev_depth = device->queue_depth;
		}
		ncr_setup_tags (np, device->id, device->lun);

#ifdef DEBUG_NCR53C8XX
printk("ncr53c8xx_select_queue_depth: host=%d, id=%d, lun=%d, depth=%d\n",
	np->unit, device->id, device->lun, device->queue_depth);
#endif
	}
}

/*
**   Linux entry point of queuecommand() function
*/

int ncr53c8xx_queue_command (Scsi_Cmnd *cmd, void (* done)(Scsi_Cmnd *))
{
     ncb_p np = ((struct host_data *) cmd->host->hostdata)->ncb;
     unsigned long flags;
     int sts;

#ifdef DEBUG_NCR53C8XX
printk("ncr53c8xx_queue_command\n");
#endif

     cmd->scsi_done     = done;
     cmd->host_scribble = NULL;
#ifdef SCSI_NCR_DYNAMIC_DMA_MAPPING
     cmd->__data_mapped = 0;
     cmd->__data_mapping = 0;
#endif

     NCR_LOCK_NCB(np, flags);

     if ((sts = ncr_queue_command(np, cmd)) != DID_OK) {
	  cmd->result = ScsiResult(sts, 0);
#ifdef DEBUG_NCR53C8XX
printk("ncr53c8xx : command not queued - result=%d\n", sts);
#endif
     }
#ifdef DEBUG_NCR53C8XX
     else
printk("ncr53c8xx : command successfully queued\n");
#endif

     NCR_UNLOCK_NCB(np, flags);

     if (sts != DID_OK) {
          unmap_scsi_data(np, cmd);
          done(cmd);
     }

     return sts;
}

/*
**   Linux entry point of the interrupt handler.
**   Since linux versions > 1.3.70, we trust the kernel for 
**   passing the internal host descriptor as 'dev_id'.
**   Otherwise, we scan the host list and call the interrupt 
**   routine for each host that uses this IRQ.
*/

static void ncr53c8xx_intr(int irq, void *dev_id, struct pt_regs * regs)
{
     unsigned long flags;
     ncb_p np = (ncb_p) dev_id;
     Scsi_Cmnd *done_list;

#ifdef DEBUG_NCR53C8XX
     printk("ncr53c8xx : interrupt received\n");
#endif

     if (DEBUG_FLAGS & DEBUG_TINY) printk ("[");

     NCR_LOCK_NCB(np, flags);
     ncr_exception(np);
     done_list     = np->done_list;
     np->done_list = 0;
     NCR_UNLOCK_NCB(np, flags);

     if (DEBUG_FLAGS & DEBUG_TINY) printk ("]\n");

     if (done_list) {
          NCR_LOCK_SCSI_DONE(np, flags);
          ncr_flush_done_cmds(done_list);
          NCR_UNLOCK_SCSI_DONE(np, flags);
     }
}

/*
**   Linux entry point of the timer handler
*/

static void ncr53c8xx_timeout(unsigned long npref)
{
     ncb_p np = (ncb_p) npref;
     unsigned long flags;
     Scsi_Cmnd *done_list;

     NCR_LOCK_NCB(np, flags);
     ncr_timeout((ncb_p) np);
     done_list     = np->done_list;
     np->done_list = 0;
     NCR_UNLOCK_NCB(np, flags);

     if (done_list) {
          NCR_LOCK_SCSI_DONE(np, flags);
          ncr_flush_done_cmds(done_list);
          NCR_UNLOCK_SCSI_DONE(np, flags);
     }
}

/*
**   Linux entry point of reset() function
*/

#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
int ncr53c8xx_reset(Scsi_Cmnd *cmd, unsigned int reset_flags)
#else
int ncr53c8xx_reset(Scsi_Cmnd *cmd)
#endif
{
	ncb_p np = ((struct host_data *) cmd->host->hostdata)->ncb;
	int sts;
	unsigned long flags;
	Scsi_Cmnd *done_list;

#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
	printk("ncr53c8xx_reset: pid=%lu reset_flags=%x serial_number=%ld serial_number_at_timeout=%ld\n",
		cmd->pid, reset_flags, cmd->serial_number, cmd->serial_number_at_timeout);
#else
	printk("ncr53c8xx_reset: command pid %lu\n", cmd->pid);
#endif

	NCR_LOCK_NCB(np, flags);

	/*
	 * We have to just ignore reset requests in some situations.
	 */
#if defined SCSI_RESET_NOT_RUNNING
	if (cmd->serial_number != cmd->serial_number_at_timeout) {
		sts = SCSI_RESET_NOT_RUNNING;
		goto out;
	}
#endif
	/*
	 * If the mid-level driver told us reset is synchronous, it seems 
	 * that we must call the done() callback for the involved command, 
	 * even if this command was not queued to the low-level driver, 
	 * before returning SCSI_RESET_SUCCESS.
	 */

#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
	sts = ncr_reset_bus(np, cmd,
	(reset_flags & (SCSI_RESET_SYNCHRONOUS | SCSI_RESET_ASYNCHRONOUS)) == SCSI_RESET_SYNCHRONOUS);
#else
	sts = ncr_reset_bus(np, cmd, 0);
#endif

	/*
	 * Since we always reset the controller, when we return success, 
	 * we add this information to the return code.
	 */
#if defined SCSI_RESET_HOST_RESET
	if (sts == SCSI_RESET_SUCCESS)
		sts |= SCSI_RESET_HOST_RESET;
#endif

out:
	done_list     = np->done_list;
	np->done_list = 0;
	NCR_UNLOCK_NCB(np, flags);

	ncr_flush_done_cmds(done_list);

	return sts;
}

/*
**   Linux entry point of abort() function
*/

int ncr53c8xx_abort(Scsi_Cmnd *cmd)
{
	ncb_p np = ((struct host_data *) cmd->host->hostdata)->ncb;
	int sts;
	unsigned long flags;
	Scsi_Cmnd *done_list;

#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
	printk("ncr53c8xx_abort: pid=%lu serial_number=%ld serial_number_at_timeout=%ld\n",
		cmd->pid, cmd->serial_number, cmd->serial_number_at_timeout);
#else
	printk("ncr53c8xx_abort: command pid %lu\n", cmd->pid);
#endif

	NCR_LOCK_NCB(np, flags);

#if defined SCSI_RESET_SYNCHRONOUS && defined SCSI_RESET_ASYNCHRONOUS
	/*
	 * We have to just ignore abort requests in some situations.
	 */
	if (cmd->serial_number != cmd->serial_number_at_timeout) {
		sts = SCSI_ABORT_NOT_RUNNING;
		goto out;
	}
#endif

	sts = ncr_abort_command(np, cmd);
out:
	done_list     = np->done_list;
	np->done_list = 0;
	NCR_UNLOCK_NCB(np, flags);

	ncr_flush_done_cmds(done_list);

	return sts;
}


#ifdef MODULE
int ncr53c8xx_release(struct Scsi_Host *host)
{
#ifdef DEBUG_NCR53C8XX
printk("ncr53c8xx : release\n");
#endif
     ncr_detach(((struct host_data *) host->hostdata)->ncb);

     return 1;
}
#endif


/*
**	Scsi command waiting list management.
**
**	It may happen that we cannot insert a scsi command into the start queue,
**	in the following circumstances.
** 		Too few preallocated ccb(s), 
**		maxtags < cmd_per_lun of the Linux host control block,
**		etc...
**	Such scsi commands are inserted into a waiting list.
**	When a scsi command complete, we try to requeue the commands of the
**	waiting list.
*/

#define next_wcmd host_scribble

static void insert_into_waiting_list(ncb_p np, Scsi_Cmnd *cmd)
{
	Scsi_Cmnd *wcmd;

#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx inserted into waiting list\n", ncr_name(np), (u_long) cmd);
#endif
	cmd->next_wcmd = 0;
	if (!(wcmd = np->waiting_list)) np->waiting_list = cmd;
	else {
		while ((wcmd->next_wcmd) != 0)
			wcmd = (Scsi_Cmnd *) wcmd->next_wcmd;
		wcmd->next_wcmd = (char *) cmd;
	}
}

static Scsi_Cmnd *retrieve_from_waiting_list(int to_remove, ncb_p np, Scsi_Cmnd *cmd)
{
	Scsi_Cmnd **pcmd = &np->waiting_list;

	while (*pcmd) {
		if (cmd == *pcmd) {
			if (to_remove) {
				*pcmd = (Scsi_Cmnd *) cmd->next_wcmd;
				cmd->next_wcmd = 0;
			}
#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx retrieved from waiting list\n", ncr_name(np), (u_long) cmd);
#endif
			return cmd;
		}
		pcmd = (Scsi_Cmnd **) &(*pcmd)->next_wcmd;
	}
	return 0;
}

static void process_waiting_list(ncb_p np, int sts)
{
	Scsi_Cmnd *waiting_list, *wcmd;

	waiting_list = np->waiting_list;
	np->waiting_list = 0;

#ifdef DEBUG_WAITING_LIST
	if (waiting_list) printk("%s: waiting_list=%lx processing sts=%d\n", ncr_name(np), (u_long) waiting_list, sts);
#endif
	while ((wcmd = waiting_list) != 0) {
		waiting_list = (Scsi_Cmnd *) wcmd->next_wcmd;
		wcmd->next_wcmd = 0;
		if (sts == DID_OK) {
#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx trying to requeue\n", ncr_name(np), (u_long) wcmd);
#endif
			sts = ncr_queue_command(np, wcmd);
		}
		if (sts != DID_OK) {
#ifdef DEBUG_WAITING_LIST
	printk("%s: cmd %lx done forced sts=%d\n", ncr_name(np), (u_long) wcmd, sts);
#endif
			wcmd->result = ScsiResult(sts, 0);
			ncr_queue_done_cmd(np, wcmd);
		}
	}
}

#undef next_wcmd

#ifdef SCSI_NCR_PROC_INFO_SUPPORT

/*=========================================================================
**	Proc file system stuff
**
**	A read operation returns profile information.
**	A write operation is a control command.
**	The string is parsed in the driver code and the command is passed 
**	to the ncr_usercmd() function.
**=========================================================================
*/

#ifdef SCSI_NCR_USER_COMMAND_SUPPORT

#define is_digit(c)	((c) >= '0' && (c) <= '9')
#define digit_to_bin(c)	((c) - '0')
#define is_space(c)	((c) == ' ' || (c) == '\t')

static int skip_spaces(char *ptr, int len)
{
	int cnt, c;

	for (cnt = len; cnt > 0 && (c = *ptr++) && is_space(c); cnt--);

	return (len - cnt);
}

static int get_int_arg(char *ptr, int len, u_long *pv)
{
	int	cnt, c;
	u_long	v;

	for (v = 0, cnt = len; cnt > 0 && (c = *ptr++) && is_digit(c); cnt--) {
		v = (v * 10) + digit_to_bin(c);
	}

	if (pv)
		*pv = v;

	return (len - cnt);
}

static int is_keyword(char *ptr, int len, char *verb)
{
	int verb_len = strlen(verb);

	if (len >= strlen(verb) && !memcmp(verb, ptr, verb_len))
		return verb_len;
	else
		return 0;

}

#define SKIP_SPACES(min_spaces)						\
	if ((arg_len = skip_spaces(ptr, len)) < (min_spaces))		\
		return -EINVAL;						\
	ptr += arg_len; len -= arg_len;

#define GET_INT_ARG(v)							\
	if (!(arg_len = get_int_arg(ptr, len, &(v))))			\
		return -EINVAL;						\
	ptr += arg_len; len -= arg_len;


/*
**	Parse a control command
*/

static int ncr_user_command(ncb_p np, char *buffer, int length)
{
	char *ptr	= buffer;
	int len		= length;
	struct usrcmd	 *uc = &np->user;
	int		arg_len;
	u_long 		target;

	bzero(uc, sizeof(*uc));

	if (len > 0 && ptr[len-1] == '\n')
		--len;

	if	((arg_len = is_keyword(ptr, len, "setsync")) != 0)
		uc->cmd = UC_SETSYNC;
	else if	((arg_len = is_keyword(ptr, len, "settags")) != 0)
		uc->cmd = UC_SETTAGS;
	else if	((arg_len = is_keyword(ptr, len, "setorder")) != 0)
		uc->cmd = UC_SETORDER;
	else if	((arg_len = is_keyword(ptr, len, "setverbose")) != 0)
		uc->cmd = UC_SETVERBOSE;
	else if	((arg_len = is_keyword(ptr, len, "setwide")) != 0)
		uc->cmd = UC_SETWIDE;
	else if	((arg_len = is_keyword(ptr, len, "setdebug")) != 0)
		uc->cmd = UC_SETDEBUG;
	else if	((arg_len = is_keyword(ptr, len, "setflag")) != 0)
		uc->cmd = UC_SETFLAG;
	else
		arg_len = 0;

#ifdef DEBUG_PROC_INFO
printk("ncr_user_command: arg_len=%d, cmd=%ld\n", arg_len, uc->cmd);
#endif

	if (!arg_len)
		return -EINVAL;
	ptr += arg_len; len -= arg_len;

	switch(uc->cmd) {
	case UC_SETSYNC:
	case UC_SETTAGS:
	case UC_SETWIDE:
	case UC_SETFLAG:
		SKIP_SPACES(1);
		if ((arg_len = is_keyword(ptr, len, "all")) != 0) {
			ptr += arg_len; len -= arg_len;
			uc->target = ~0;
		} else {
			GET_INT_ARG(target);
			uc->target = (1<<target);
#ifdef DEBUG_PROC_INFO
printk("ncr_user_command: target=%ld\n", target);
#endif
		}
		break;
	}

	switch(uc->cmd) {
	case UC_SETVERBOSE:
	case UC_SETSYNC:
	case UC_SETTAGS:
	case UC_SETWIDE:
		SKIP_SPACES(1);
		GET_INT_ARG(uc->data);
#ifdef DEBUG_PROC_INFO
printk("ncr_user_command: data=%ld\n", uc->data);
#endif
		break;
	case UC_SETORDER:
		SKIP_SPACES(1);
		if	((arg_len = is_keyword(ptr, len, "simple")))
			uc->data = M_SIMPLE_TAG;
		else if	((arg_len = is_keyword(ptr, len, "ordered")))
			uc->data = M_ORDERED_TAG;
		else if	((arg_len = is_keyword(ptr, len, "default")))
			uc->data = 0;
		else
			return -EINVAL;
		break;
	case UC_SETDEBUG:
		while (len > 0) {
			SKIP_SPACES(1);
			if	((arg_len = is_keyword(ptr, len, "alloc")))
				uc->data |= DEBUG_ALLOC;
			else if	((arg_len = is_keyword(ptr, len, "phase")))
				uc->data |= DEBUG_PHASE;
			else if	((arg_len = is_keyword(ptr, len, "queue")))
				uc->data |= DEBUG_QUEUE;
			else if	((arg_len = is_keyword(ptr, len, "result")))
				uc->data |= DEBUG_RESULT;
			else if	((arg_len = is_keyword(ptr, len, "scatter")))
				uc->data |= DEBUG_SCATTER;
			else if	((arg_len = is_keyword(ptr, len, "script")))
				uc->data |= DEBUG_SCRIPT;
			else if	((arg_len = is_keyword(ptr, len, "tiny")))
				uc->data |= DEBUG_TINY;
			else if	((arg_len = is_keyword(ptr, len, "timing")))
				uc->data |= DEBUG_TIMING;
			else if	((arg_len = is_keyword(ptr, len, "nego")))
				uc->data |= DEBUG_NEGO;
			else if	((arg_len = is_keyword(ptr, len, "tags")))
				uc->data |= DEBUG_TAGS;
			else
				return -EINVAL;
			ptr += arg_len; len -= arg_len;
		}
#ifdef DEBUG_PROC_INFO
printk("ncr_user_command: data=%ld\n", uc->data);
#endif
		break;
	case UC_SETFLAG:
		while (len > 0) {
			SKIP_SPACES(1);
			if	((arg_len = is_keyword(ptr, len, "trace")))
				uc->data |= UF_TRACE;
			else if	((arg_len = is_keyword(ptr, len, "no_disc")))
				uc->data |= UF_NODISC;
			else
				return -EINVAL;
			ptr += arg_len; len -= arg_len;
		}
		break;
	default:
		break;
	}

	if (len)
		return -EINVAL;
	else {
		long flags;

		NCR_LOCK_NCB(np, flags);
		ncr_usercmd (np);
		NCR_UNLOCK_NCB(np, flags);
	}
	return length;
}

#endif	/* SCSI_NCR_USER_COMMAND_SUPPORT */


#ifdef SCSI_NCR_USER_INFO_SUPPORT
/*
**	Copy formatted information into the input buffer.
*/

static int ncr_host_info(ncb_p np, char *ptr, off_t offset, int len)
{
	struct info_str info;

	info.buffer	= ptr;
	info.length	= len;
	info.offset	= offset;
	info.pos	= 0;

	copy_info(&info, "  Chip NCR53C%s, device id 0x%x, "
			 "revision id 0x%x\n",
			 np->chip_name, np->device_id,	np->revision_id);
	copy_info(&info, "  On PCI bus %d, device %d, function %d, "
#ifdef __sparc__
		"IRQ %s\n",
#else
		"IRQ %d\n",
#endif
		np->bus, (np->device_fn & 0xf8) >> 3, np->device_fn & 7,
#ifdef __sparc__
		__irq_itoa(np->irq));
#else
		(int) np->irq);
#endif
	copy_info(&info, "  Synchronous period factor %d, "
			 "max commands per lun %d\n",
			 (int) np->minsync, MAX_TAGS);

	if (driver_setup.debug || driver_setup.verbose > 1) {
		copy_info(&info, "  Debug flags 0x%x, verbosity level %d\n",
			  driver_setup.debug, driver_setup.verbose);
	}

	return info.pos > info.offset? info.pos - info.offset : 0;
}

#endif /* SCSI_NCR_USER_INFO_SUPPORT */

/*
**	Entry point of the scsi proc fs of the driver.
**	- func = 0 means read  (returns profile data)
**	- func = 1 means write (parse user control command)
*/

static int ncr53c8xx_proc_info(char *buffer, char **start, off_t offset,
			int length, int hostno, int func)
{
	struct Scsi_Host *host;
	struct host_data *host_data;
	ncb_p ncb = 0;
	int retv;

#ifdef DEBUG_PROC_INFO
printk("ncr53c8xx_proc_info: hostno=%d, func=%d\n", hostno, func);
#endif

	for (host = first_host; host; host = host->next) {
		if (host->hostt == the_template && host->host_no == hostno) {
			host_data = (struct host_data *) host->hostdata;
			ncb = host_data->ncb;
			break;
		}
	}

	if (!ncb)
		return -EINVAL;

	if (func) {
#ifdef	SCSI_NCR_USER_COMMAND_SUPPORT
		retv = ncr_user_command(ncb, buffer, length);
#else
		retv = -EINVAL;
#endif
	}
	else {
		if (start)
			*start = buffer;
#ifdef SCSI_NCR_USER_INFO_SUPPORT
		retv = ncr_host_info(ncb, buffer, offset, length);
#else
		retv = -EINVAL;
#endif
	}

	return retv;
}

/*=========================================================================
**	End of proc file system stuff
**=========================================================================
*/
#endif


/*==========================================================
**
**	/proc directory entry.
**
**==========================================================
*/
#if LINUX_VERSION_CODE < LinuxVersionCode(2,3,27)
static struct proc_dir_entry proc_scsi_ncr53c8xx = {
    PROC_SCSI_NCR53C8XX, 9, NAME53C8XX,
    S_IFDIR | S_IRUGO | S_IXUGO, 2
};
#endif

/*==========================================================
**
**	Boot command line.
**
**==========================================================
*/
#ifdef	MODULE
char *ncr53c8xx = 0;	/* command line passed by insmod */
# if LINUX_VERSION_CODE >= LinuxVersionCode(2,1,30)
MODULE_PARM(ncr53c8xx, "s");
# endif
#endif

int __init ncr53c8xx_setup(char *str)
{
	return sym53c8xx__setup(str);
}

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,3,13)
#ifndef MODULE
__setup("ncr53c8xx=", ncr53c8xx_setup);
#endif
#endif

/*===================================================================
**
**   SYM53C8XX supported device list
**
**===================================================================
*/

static u_short	ncr_chip_ids[]   __initdata = {
	PCI_DEVICE_ID_NCR_53C810,
	PCI_DEVICE_ID_NCR_53C815,
	PCI_DEVICE_ID_NCR_53C820,
	PCI_DEVICE_ID_NCR_53C825,
	PCI_DEVICE_ID_NCR_53C860,
	PCI_DEVICE_ID_NCR_53C875,
	PCI_DEVICE_ID_NCR_53C875J,
	PCI_DEVICE_ID_NCR_53C885,
	PCI_DEVICE_ID_NCR_53C895,
	PCI_DEVICE_ID_NCR_53C896,
	PCI_DEVICE_ID_NCR_53C895A,
	PCI_DEVICE_ID_NCR_53C1510D
};

/*==========================================================
**
**	Chip detection entry point.
**
**==========================================================
*/
int __init ncr53c8xx_detect(Scsi_Host_Template *tpnt)
{
	/*
	**    Initialize driver general stuff.
	*/
#ifdef SCSI_NCR_PROC_INFO_SUPPORT
#if LINUX_VERSION_CODE < LinuxVersionCode(2,3,27)
     tpnt->proc_dir  = &proc_scsi_ncr53c8xx;
#else
     tpnt->proc_name = NAME53C8XX;
#endif
     tpnt->proc_info = ncr53c8xx_proc_info;
#endif

#if	defined(SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT) && defined(MODULE)
if (ncr53c8xx)
	ncr53c8xx_setup(ncr53c8xx);
#endif

	return sym53c8xx__detect(tpnt, ncr_chip_ids,
				 sizeof(ncr_chip_ids)/sizeof(ncr_chip_ids[0]));
}

/*==========================================================
**
**   Entry point for info() function
**
**==========================================================
*/
const char *ncr53c8xx_info (struct Scsi_Host *host)
{
	return SCSI_NCR_DRIVER_NAME;
}

/*
**	Module stuff
*/
MODULE_LICENSE("GPL");

#if LINUX_VERSION_CODE >= LinuxVersionCode(2,4,0)
static
#endif
#if LINUX_VERSION_CODE >= LinuxVersionCode(2,4,0) || defined(MODULE)
Scsi_Host_Template driver_template = NCR53C8XX;
#include "scsi_module.c"
#endif
