/**************************************************************************
**
** $FreeBSD$
**
**  Device driver for the   NCR 53C8XX   PCI-SCSI-Controller Family.
**
**-------------------------------------------------------------------------
**
**  Written for 386bsd and FreeBSD by
**	Wolfgang Stanglmeier	<wolf@cologne.de>
**	Stefan Esser		<se@mi.Uni-Koeln.de>
**
**-------------------------------------------------------------------------
**
** Copyright (c) 1994 Wolfgang Stanglmeier.  All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
***************************************************************************
*/

#define NCR_DATE "pl30 98/1/1"

#define NCR_VERSION	(2)
#define	MAX_UNITS	(16)

#define NCR_GETCC_WITHMSG

#if defined (__FreeBSD__) && defined(_KERNEL)
#include "opt_ncr.h"
#endif

/*==========================================================
**
**	Configuration and Debugging
**
**	May be overwritten in <arch/conf/xxxx>
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
#endif /* SCSI_NCR_MYADDR */

/*
**    The default synchronous period factor
**    (0=asynchronous)
**    If maximum synchronous frequency is defined, use it instead.
*/

#ifndef	SCSI_NCR_MAX_SYNC

#ifndef SCSI_NCR_DFLT_SYNC
#define SCSI_NCR_DFLT_SYNC   (12)
#endif /* SCSI_NCR_DFLT_SYNC */

#else

#if	SCSI_NCR_MAX_SYNC == 0
#define	SCSI_NCR_DFLT_SYNC 0
#else
#define	SCSI_NCR_DFLT_SYNC (250000 / SCSI_NCR_MAX_SYNC)
#endif

#endif

/*
**    The minimal asynchronous pre-scaler period (ns)
**    Shall be 40.
*/

#ifndef SCSI_NCR_MIN_ASYNC
#define SCSI_NCR_MIN_ASYNC   (40)
#endif /* SCSI_NCR_MIN_ASYNC */

/*
**    The maximal bus with (in log2 byte)
**    (0=8 bit, 1=16 bit)
*/

#ifndef SCSI_NCR_MAX_WIDE
#define SCSI_NCR_MAX_WIDE   (1)
#endif /* SCSI_NCR_MAX_WIDE */

/*==========================================================
**
**      Configuration and Debugging
**
**==========================================================
*/

/*
**    Number of targets supported by the driver.
**    n permits target numbers 0..n-1.
**    Default is 7, meaning targets #0..#6.
**    #7 .. is myself.
*/

#define MAX_TARGET  (16)

/*
**    Number of logic units supported by the driver.
**    n enables logic unit numbers 0..n-1.
**    The common SCSI devices require only
**    one lun, so take 1 as the default.
*/

#ifndef	MAX_LUN
#define MAX_LUN     (8)
#endif	/* MAX_LUN */

/*
**    The maximum number of jobs scheduled for starting.
**    There should be one slot per target, and one slot
**    for each tag of each target in use.
*/

#define MAX_START   (256)

/*
**    The maximum number of segments a transfer is split into.
*/

#define MAX_SCATTER (33)

/*
**    The maximum transfer length (should be >= 64k).
**    MUST NOT be greater than (MAX_SCATTER-1) * PAGE_SIZE.
*/

#define MAX_SIZE  ((MAX_SCATTER-1) * (long) PAGE_SIZE)

/*
**	other
*/

#define NCR_SNOOP_TIMEOUT (1000000)

/*==========================================================
**
**      Include files
**
**==========================================================
*/

#include <sys/param.h>
#include <sys/time.h>

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <machine/md_var.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#endif

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/ncrreg.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

/*==========================================================
**
**	Debugging tags
**
**==========================================================
*/

#define DEBUG_ALLOC    (0x0001)
#define DEBUG_PHASE    (0x0002)
#define DEBUG_POLL     (0x0004)
#define DEBUG_QUEUE    (0x0008)
#define DEBUG_RESULT   (0x0010)
#define DEBUG_SCATTER  (0x0020)
#define DEBUG_SCRIPT   (0x0040)
#define DEBUG_TINY     (0x0080)
#define DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x0200)
#define DEBUG_TAGS     (0x0400)
#define DEBUG_FREEZE   (0x0800)
#define DEBUG_RESTART  (0x1000)

/*
**    Enable/Disable debug messages.
**    Can be changed at runtime too.
*/
#ifdef SCSI_NCR_DEBUG
	#define DEBUG_FLAGS ncr_debug
#else /* SCSI_NCR_DEBUG */
	#define SCSI_NCR_DEBUG	0
	#define DEBUG_FLAGS	0
#endif /* SCSI_NCR_DEBUG */



/*==========================================================
**
**	assert ()
**
**==========================================================
**
**	modified copy from 386bsd:/usr/include/sys/assert.h
**
**----------------------------------------------------------
*/

#ifdef DIAGNOSTIC
#define	assert(expression) {					\
	if (!(expression)) {					\
		(void)printf("assertion \"%s\" failed: "	\
			     "file \"%s\", line %d\n",		\
			     #expression, __FILE__, __LINE__);	\
	     Debugger("");					\
	}							\
}
#else
#define	assert(expression) {					\
	if (!(expression)) {					\
		(void)printf("assertion \"%s\" failed: "	\
			     "file \"%s\", line %d\n",		\
			     #expression, __FILE__, __LINE__);	\
	}							\
}
#endif

/*==========================================================
**
**	Access to the controller chip.
**
**==========================================================
*/

#ifdef __alpha__
/* XXX */
#undef vtophys
#define	vtophys(va)	alpha_XXX_dmamap((vm_offset_t)va)
#endif

#define	INB(r) bus_space_read_1(np->bst, np->bsh, offsetof(struct ncr_reg, r))
#define	INW(r) bus_space_read_2(np->bst, np->bsh, offsetof(struct ncr_reg, r))
#define	INL(r) bus_space_read_4(np->bst, np->bsh, offsetof(struct ncr_reg, r))

#define	OUTB(r, val) bus_space_write_1(np->bst, np->bsh, \
				       offsetof(struct ncr_reg, r), val)
#define	OUTW(r, val) bus_space_write_2(np->bst, np->bsh, \
				       offsetof(struct ncr_reg, r), val)
#define	OUTL(r, val) bus_space_write_4(np->bst, np->bsh, \
				       offsetof(struct ncr_reg, r), val)
#define	OUTL_OFF(o, val) bus_space_write_4(np->bst, np->bsh, o, val)

#define	INB_OFF(o) bus_space_read_1(np->bst, np->bsh, o)
#define	INW_OFF(o) bus_space_read_2(np->bst, np->bsh, o)
#define	INL_OFF(o) bus_space_read_4(np->bst, np->bsh, o)

#define	READSCRIPT_OFF(base, off)					\
    (base ? *((volatile u_int32_t *)((volatile char *)base + (off))) :	\
    bus_space_read_4(np->bst2, np->bsh2, off))

#define	WRITESCRIPT_OFF(base, off, val)					\
    do {								\
    	if (base)							\
    		*((volatile u_int32_t *)				\
			((volatile char *)base + (off))) = (val);	\
    	else								\
		bus_space_write_4(np->bst2, np->bsh2, off, val);	\
    } while (0)

#define	READSCRIPT(r) \
    READSCRIPT_OFF(np->script, offsetof(struct script, r))

#define	WRITESCRIPT(r, val) \
    WRITESCRIPT_OFF(np->script, offsetof(struct script, r), val)

/*
**	Set bit field ON, OFF 
*/

#define OUTONB(r, m)	OUTB(r, INB(r) | (m))
#define OUTOFFB(r, m)	OUTB(r, INB(r) & ~(m))
#define OUTONW(r, m)	OUTW(r, INW(r) | (m))
#define OUTOFFW(r, m)	OUTW(r, INW(r) & ~(m))
#define OUTONL(r, m)	OUTL(r, INL(r) | (m))
#define OUTOFFL(r, m)	OUTL(r, INL(r) & ~(m))

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

#define HS_COMPLETE	(4)
#define HS_SEL_TIMEOUT	(5)	/* Selection timeout      */
#define HS_RESET	(6)	/* SCSI reset	     */
#define HS_ABORTED	(7)	/* Transfer aborted       */
#define HS_TIMEOUT	(8)	/* Software timeout       */
#define HS_FAIL		(9)	/* SCSI or PCI bus errors */
#define HS_UNEXPECTED	(10)	/* Unexpected disconnect  */
#define HS_STALL	(11)	/* QUEUE FULL or BUSY	  */

#define HS_DONEMASK	(0xfc)

/*==========================================================
**
**	Software Interrupt Codes
**
**==========================================================
*/

#define	SIR_SENSE_RESTART	(1)
#define	SIR_SENSE_FAILED	(2)
#define	SIR_STALL_RESTART	(3)
#define	SIR_STALL_QUEUE		(4)
#define	SIR_NEGO_SYNC		(5)
#define	SIR_NEGO_WIDE		(6)
#define	SIR_NEGO_FAILED		(7)
#define	SIR_NEGO_PROTO		(8)
#define	SIR_REJECT_RECEIVED	(9)
#define	SIR_REJECT_SENT		(10)
#define	SIR_IGN_RESIDUE		(11)
#define	SIR_MISSING_SAVE	(12)
#define	SIR_MAX			(12)

/*==========================================================
**
**	Extended error codes.
**	xerr_status field of struct nccb.
**
**==========================================================
*/

#define	XE_OK		(0)
#define	XE_EXTRA_DATA	(1)	/* unexpected data phase */
#define	XE_BAD_PHASE	(2)	/* illegal phase (4/5)   */

/*==========================================================
**
**	Negotiation status.
**	nego_status field	of struct nccb.
**
**==========================================================
*/

#define NS_SYNC		(1)
#define NS_WIDE		(2)

/*==========================================================
**
**	XXX These are no longer used.  Remove once the
**	    script is updated.
**	"Special features" of targets.
**	quirks field of struct tcb.
**	actualquirks field of struct nccb.
**
**==========================================================
*/

#define	QUIRK_AUTOSAVE	(0x01)
#define	QUIRK_NOMSG	(0x02)
#define	QUIRK_NOSYNC	(0x10)
#define	QUIRK_NOWIDE16	(0x20)
#define	QUIRK_NOTAGS	(0x40)
#define	QUIRK_UPDATE	(0x80)

/*==========================================================
**
**	Misc.
**
**==========================================================
*/

#define CCB_MAGIC	(0xf2691ad2)
#define	MAX_TAGS	(32)		/* hard limit */

/*==========================================================
**
**	OS dependencies.
**
**==========================================================
*/

#define PRINT_ADDR(ccb) xpt_print_path((ccb)->ccb_h.path)

/*==========================================================
**
**	Declaration of structs.
**
**==========================================================
*/

struct tcb;
struct lcb;
struct nccb;
struct ncb;
struct script;

typedef struct ncb * ncb_p;
typedef struct tcb * tcb_p;
typedef struct lcb * lcb_p;
typedef struct nccb * nccb_p;

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

#define	UF_TRACE	(0x01)

/*---------------------------------------
**
**	Timestamps for profiling
**
**---------------------------------------
*/

/* Type of the kernel variable `ticks'.  XXX should be declared with the var. */
typedef int ticks_t;

struct tstamp {
	ticks_t	start;
	ticks_t	end;
	ticks_t	select;
	ticks_t	command;
	ticks_t	data;
	ticks_t	status;
	ticks_t	disconnect;
};

/*
**	profiling data (per device)
*/

struct profile {
	u_long	num_trans;
	u_long	num_bytes;
	u_long	num_disc;
	u_long	num_break;
	u_long	num_int;
	u_long	num_fly;
	u_long	ms_setup;
	u_long	ms_data;
	u_long	ms_disc;
	u_long	ms_post;
};

/*==========================================================
**
**	Declaration of structs:		target control block
**
**==========================================================
*/

#define NCR_TRANS_CUR		0x01	/* Modify current neogtiation status */
#define NCR_TRANS_ACTIVE	0x03	/* Assume this is the active target */
#define NCR_TRANS_GOAL		0x04	/* Modify negotiation goal */
#define NCR_TRANS_USER		0x08	/* Modify user negotiation settings */

struct ncr_transinfo {
	u_int8_t width;
	u_int8_t period;
	u_int8_t offset;
};

struct ncr_target_tinfo {
	/* Hardware version of our sync settings */
	u_int8_t disc_tag;
#define		NCR_CUR_DISCENB	0x01
#define		NCR_CUR_TAGENB	0x02
#define		NCR_USR_DISCENB	0x04
#define		NCR_USR_TAGENB	0x08
	u_int8_t sval;
        struct	 ncr_transinfo current;
        struct	 ncr_transinfo goal;
        struct	 ncr_transinfo user;
	/* Hardware version of our wide settings */
	u_int8_t wval;
};

struct tcb {
	/*
	**	during reselection the ncr jumps to this point
	**	with SFBR set to the encoded target number
	**	with bit 7 set.
	**	if it's not this target, jump to the next.
	**
	**	JUMP  IF (SFBR != #target#)
	**	@(next tcb)
	*/

	struct link   jump_tcb;

	/*
	**	load the actual values for the sxfer and the scntl3
	**	register (sync/wide mode).
	**
	**	SCR_COPY (1);
	**	@(sval field of this tcb)
	**	@(sxfer register)
	**	SCR_COPY (1);
	**	@(wval field of this tcb)
	**	@(scntl3 register)
	*/

	ncrcmd	getscr[6];

	/*
	**	if next message is "identify"
	**	then load the message to SFBR,
	**	else load 0 to SFBR.
	**
	**	CALL
	**	<RESEL_LUN>
	*/

	struct link   call_lun;

	/*
	**	now look for the right lun.
	**
	**	JUMP
	**	@(first nccb of this lun)
	*/

	struct link   jump_lcb;

	/*
	**	pointer to interrupted getcc nccb
	*/

	nccb_p   hold_cp;

	/*
	**	pointer to nccb used for negotiating.
	**	Avoid to start a nego for all queued commands 
	**	when tagged command queuing is enabled.
	*/

	nccb_p   nego_cp;

	/*
	**	statistical data
	*/

	u_long	transfers;
	u_long	bytes;

	/*
	**	user settable limits for sync transfer
	**	and tagged commands.
	*/

	struct	 ncr_target_tinfo tinfo;

	/*
	**	the lcb's of this tcb
	*/

	lcb_p   lp[MAX_LUN];
};

/*==========================================================
**
**	Declaration of structs:		lun control block
**
**==========================================================
*/

struct lcb {
	/*
	**	during reselection the ncr jumps to this point
	**	with SFBR set to the "Identify" message.
	**	if it's not this lun, jump to the next.
	**
	**	JUMP  IF (SFBR != #lun#)
	**	@(next lcb of this target)
	*/

	struct link	jump_lcb;

	/*
	**	if next message is "simple tag",
	**	then load the tag to SFBR,
	**	else load 0 to SFBR.
	**
	**	CALL
	**	<RESEL_TAG>
	*/

	struct link	call_tag;

	/*
	**	now look for the right nccb.
	**
	**	JUMP
	**	@(first nccb of this lun)
	*/

	struct link	jump_nccb;

	/*
	**	start of the nccb chain
	*/

	nccb_p	next_nccb;

	/*
	**	Control of tagged queueing
	*/

	u_char		reqnccbs;
	u_char		reqlink;
	u_char		actlink;
	u_char		usetags;
	u_char		lasttag;
};

/*==========================================================
**
**      Declaration of structs:     COMMAND control block
**
**==========================================================
**
**	This substructure is copied from the nccb to a
**	global address after selection (or reselection)
**	and copied back before disconnect.
**
**	These fields are accessible to the script processor.
**
**----------------------------------------------------------
*/

struct head {
	/*
	**	Execution of a nccb starts at this point.
	**	It's a jump to the "SELECT" label
	**	of the script.
	**
	**	After successful selection the script
	**	processor overwrites it with a jump to
	**	the IDLE label of the script.
	*/

	struct link	launch;

	/*
	**	Saved data pointer.
	**	Points to the position in the script
	**	responsible for the actual transfer
	**	of data.
	**	It's written after reception of a
	**	"SAVE_DATA_POINTER" message.
	**	The goalpointer points after
	**	the last transfer command.
	*/

	u_int32_t	savep;
	u_int32_t	lastp;
	u_int32_t	goalp;

	/*
	**	The virtual address of the nccb
	**	containing this header.
	*/

	nccb_p	cp;

	/*
	**	space for some timestamps to gather
	**	profiling data about devices and this driver.
	*/

	struct tstamp	stamp;

	/*
	**	status fields.
	*/

	u_char		status[8];
};

/*
**	The status bytes are used by the host and the script processor.
**
**	The first four byte are copied to the scratchb register
**	(declared as scr0..scr3 in ncr_reg.h) just after the select/reselect,
**	and copied back just after disconnecting.
**	Inside the script the XX_REG are used.
**
**	The last four bytes are used inside the script by "COPY" commands.
**	Because source and destination must have the same alignment
**	in a longword, the fields HAVE to be at the choosen offsets.
**		xerr_st	(4)	0	(0x34)	scratcha
**		sync_st	(5)	1	(0x05)	sxfer
**		wide_st	(7)	3	(0x03)	scntl3
*/

/*
**	First four bytes (script)
*/
#define  QU_REG	scr0
#define  HS_REG	scr1
#define  HS_PRT	nc_scr1
#define  SS_REG	scr2
#define  PS_REG	scr3

/*
**	First four bytes (host)
*/
#define  actualquirks  phys.header.status[0]
#define  host_status   phys.header.status[1]
#define  s_status      phys.header.status[2]
#define  parity_status phys.header.status[3]

/*
**	Last four bytes (script)
*/
#define  xerr_st       header.status[4]	/* MUST be ==0 mod 4 */
#define  sync_st       header.status[5]	/* MUST be ==1 mod 4 */
#define  nego_st       header.status[6]
#define  wide_st       header.status[7]	/* MUST be ==3 mod 4 */

/*
**	Last four bytes (host)
*/
#define  xerr_status   phys.xerr_st
#define  sync_status   phys.sync_st
#define  nego_status   phys.nego_st
#define  wide_status   phys.wide_st

/*==========================================================
**
**      Declaration of structs:     Data structure block
**
**==========================================================
**
**	During execution of a nccb by the script processor,
**	the DSA (data structure address) register points
**	to this substructure of the nccb.
**	This substructure contains the header with
**	the script-processor-changable data and
**	data blocks for the indirect move commands.
**
**----------------------------------------------------------
*/

struct dsb {

	/*
	**	Header.
	**	Has to be the first entry,
	**	because it's jumped to by the
	**	script processor
	*/

	struct head	header;

	/*
	**	Table data for Script
	*/

	struct scr_tblsel  select;
	struct scr_tblmove smsg  ;
	struct scr_tblmove smsg2 ;
	struct scr_tblmove cmd   ;
	struct scr_tblmove scmd  ;
	struct scr_tblmove sense ;
	struct scr_tblmove data [MAX_SCATTER];
};

/*==========================================================
**
**      Declaration of structs:     Command control block.
**
**==========================================================
**
**	During execution of a nccb by the script processor,
**	the DSA (data structure address) register points
**	to this substructure of the nccb.
**	This substructure contains the header with
**	the script-processor-changable data and then
**	data blocks for the indirect move commands.
**
**----------------------------------------------------------
*/


struct nccb {
	/*
	**	This filler ensures that the global header is 
	**	cache line size aligned.
	*/
	ncrcmd	filler[4];

	/*
	**	during reselection the ncr jumps to this point.
	**	If a "SIMPLE_TAG" message was received,
	**	then SFBR is set to the tag.
	**	else SFBR is set to 0
	**	If looking for another tag, jump to the next nccb.
	**
	**	JUMP  IF (SFBR != #TAG#)
	**	@(next nccb of this lun)
	*/

	struct link		jump_nccb;

	/*
	**	After execution of this call, the return address
	**	(in  the TEMP register) points to the following
	**	data structure block.
	**	So copy it to the DSA register, and start
	**	processing of this data structure.
	**
	**	CALL
	**	<RESEL_TMP>
	*/

	struct link		call_tmp;

	/*
	**	This is the data structure which is
	**	to be executed by the script processor.
	*/

	struct dsb		phys;

	/*
	**	If a data transfer phase is terminated too early
	**	(after reception of a message (i.e. DISCONNECT)),
	**	we have to prepare a mini script to transfer
	**	the rest of the data.
	*/

	ncrcmd			patch[8];

	/*
	**	The general SCSI driver provides a
	**	pointer to a control block.
	*/

	union	ccb *ccb;

	/*
	**	We prepare a message to be sent after selection,
	**	and a second one to be sent after getcc selection.
	**      Contents are IDENTIFY and SIMPLE_TAG.
	**	While negotiating sync or wide transfer,
	**	a SDTM or WDTM message is appended.
	*/

	u_char			scsi_smsg [8];
	u_char			scsi_smsg2[8];

	/*
	**	Lock this nccb.
	**	Flag is used while looking for a free nccb.
	*/

	u_long		magic;

	/*
	**	Physical address of this instance of nccb
	*/

	u_long		p_nccb;

	/*
	**	Completion time out for this job.
	**	It's set to time of start + allowed number of seconds.
	*/

	time_t		tlimit;

	/*
	**	All nccbs of one hostadapter are chained.
	*/

	nccb_p		link_nccb;

	/*
	**	All nccbs of one target/lun are chained.
	*/

	nccb_p		next_nccb;

	/*
	**	Sense command
	*/

	u_char		sensecmd[6];

	/*
	**	Tag for this transfer.
	**	It's patched into jump_nccb.
	**	If it's not zero, a SIMPLE_TAG
	**	message is included in smsg.
	*/

	u_char			tag;
};

#define CCB_PHYS(cp,lbl)	(cp->p_nccb + offsetof(struct nccb, lbl))

/*==========================================================
**
**      Declaration of structs:     NCR device descriptor
**
**==========================================================
*/

struct ncb {
	/*
	**	The global header.
	**	Accessible to both the host and the
	**	script-processor.
	**	We assume it is cache line size aligned.
	*/
	struct head     header;

	int	unit;

	/*-----------------------------------------------
	**	Scripts ..
	**-----------------------------------------------
	**
	**	During reselection the ncr jumps to this point.
	**	The SFBR register is loaded with the encoded target id.
	**
	**	Jump to the first target.
	**
	**	JUMP
	**	@(next tcb)
	*/
	struct link     jump_tcb;

	/*-----------------------------------------------
	**	Configuration ..
	**-----------------------------------------------
	**
	**	virtual and physical addresses
	**	of the 53c810 chip.
	*/
	int		reg_rid;
	struct resource *reg_res;
	bus_space_tag_t	bst;
	bus_space_handle_t bsh;

	int		sram_rid;
	struct resource *sram_res;
	bus_space_tag_t	bst2;
	bus_space_handle_t bsh2;

	struct resource *irq_res;
	void		*irq_handle;

	/*
	**	Scripts instance virtual address.
	*/
	struct script	*script;
	struct scripth	*scripth;

	/*
	**	Scripts instance physical address.
	*/
	u_long		p_script;
	u_long		p_scripth;

	/*
	**	The SCSI address of the host adapter.
	*/
	u_char		myaddr;

	/*
	**	timing parameters
	*/
	u_char		minsync;	/* Minimum sync period factor	*/
	u_char		maxsync;	/* Maximum sync period factor	*/
	u_char		maxoffs;	/* Max scsi offset		*/
	u_char		clock_divn;	/* Number of clock divisors	*/
	u_long		clock_khz;	/* SCSI clock frequency in KHz	*/
	u_long		features;	/* Chip features map		*/
	u_char		multiplier;	/* Clock multiplier (1,2,4)	*/

	u_char		maxburst;	/* log base 2 of dwords burst	*/

	/*
	**	BIOS supplied PCI bus options
	*/
	u_char		rv_scntl3;
	u_char		rv_dcntl;
	u_char		rv_dmode;
	u_char		rv_ctest3;
	u_char		rv_ctest4;
	u_char		rv_ctest5;
	u_char		rv_gpcntl;
	u_char		rv_stest2;

	/*-----------------------------------------------
	**	CAM SIM information for this instance
	**-----------------------------------------------
	*/

	struct		cam_sim  *sim;
	struct		cam_path *path;

	/*-----------------------------------------------
	**	Job control
	**-----------------------------------------------
	**
	**	Commands from user
	*/
	struct usrcmd	user;

	/*
	**	Target data
	*/
	struct tcb	target[MAX_TARGET];

	/*
	**	Start queue.
	*/
	u_int32_t	squeue [MAX_START];
	u_short		squeueput;

	/*
	**	Timeout handler
	*/
	time_t		heartbeat;
	u_short		ticks;
	u_short		latetime;
	time_t		lasttime;
	struct		callout_handle timeout_ch;

	/*-----------------------------------------------
	**	Debug and profiling
	**-----------------------------------------------
	**
	**	register dump
	*/
	struct ncr_reg	regdump;
	time_t		regtime;

	/*
	**	Profiling data
	*/
	struct profile	profile;
	u_long		disc_phys;
	u_long		disc_ref;

	/*
	**	Head of list of all nccbs for this controller.
	*/
	nccb_p		link_nccb;
	
	/*
	**	message buffers.
	**	Should be longword aligned,
	**	because they're written with a
	**	COPY script command.
	*/
	u_char		msgout[8];
	u_char		msgin [8];
	u_int32_t	lastmsg;

	/*
	**	Buffer for STATUS_IN phase.
	*/
	u_char		scratch;

	/*
	**	controller chip dependent maximal transfer width.
	*/
	u_char		maxwide;

#ifdef NCR_IOMAPPED
	/*
	**	address of the ncr control registers in io space
	*/
	pci_port_t	port;
#endif
};

#define NCB_SCRIPT_PHYS(np,lbl)	(np->p_script + offsetof (struct script, lbl))
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
**	Script fragments which are loaded into the on-board RAM 
**	of 825A, 875 and 895 chips.
*/
struct script {
	ncrcmd	start		[  7];
	ncrcmd	start0		[  2];
	ncrcmd	start1		[  3];
	ncrcmd  startpos	[  1];
	ncrcmd  trysel		[  8];
	ncrcmd	skip		[  8];
	ncrcmd	skip2		[  3];
	ncrcmd  idle		[  2];
	ncrcmd	select		[ 18];
	ncrcmd	prepare		[  4];
	ncrcmd	loadpos		[ 14];
	ncrcmd	prepare2	[ 24];
	ncrcmd	setmsg		[  5];
	ncrcmd  clrack		[  2];
	ncrcmd  dispatch	[ 33];
	ncrcmd	no_data		[ 17];
	ncrcmd  checkatn	[ 10];
	ncrcmd  command		[ 15];
	ncrcmd  status		[ 27];
	ncrcmd  msg_in		[ 26];
	ncrcmd  msg_bad		[  6];
	ncrcmd  complete	[ 13];
	ncrcmd	cleanup		[ 12];
	ncrcmd	cleanup0	[  9];
	ncrcmd	signal		[ 12];
	ncrcmd  save_dp		[  5];
	ncrcmd  restore_dp	[  5];
	ncrcmd  disconnect	[ 12];
	ncrcmd  disconnect0	[  5];
	ncrcmd  disconnect1	[ 23];
	ncrcmd	msg_out		[  9];
	ncrcmd	msg_out_done	[  7];
	ncrcmd  badgetcc	[  6];
	ncrcmd	reselect	[  8];
	ncrcmd	reselect1	[  8];
	ncrcmd	reselect2	[  8];
	ncrcmd	resel_tmp	[  5];
	ncrcmd  resel_lun	[ 18];
	ncrcmd	resel_tag	[ 24];
	ncrcmd  data_in		[MAX_SCATTER * 4 + 7];
	ncrcmd  data_out	[MAX_SCATTER * 4 + 7];
};

/*
**	Script fragments which stay in main memory for all chips.
*/
struct scripth {
	ncrcmd  tryloop		[MAX_START*5+2];
	ncrcmd  msg_parity	[  6];
	ncrcmd	msg_reject	[  8];
	ncrcmd	msg_ign_residue	[ 32];
	ncrcmd  msg_extended	[ 18];
	ncrcmd  msg_ext_2	[ 18];
	ncrcmd	msg_wdtr	[ 27];
	ncrcmd  msg_ext_3	[ 18];
	ncrcmd	msg_sdtr	[ 27];
	ncrcmd	msg_out_abort	[ 10];
	ncrcmd  getcc		[  4];
	ncrcmd  getcc1		[  5];
#ifdef NCR_GETCC_WITHMSG
	ncrcmd	getcc2		[ 29];
#else
	ncrcmd	getcc2		[ 14];
#endif
	ncrcmd	getcc3		[  6];
	ncrcmd	aborttag	[  4];
	ncrcmd	abort		[ 22];
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

#ifdef _KERNEL
static	nccb_p	ncr_alloc_nccb	(ncb_p np, u_long target, u_long lun);
static	void	ncr_complete	(ncb_p np, nccb_p cp);
static	int	ncr_delta	(int * from, int * to);
static	void	ncr_exception	(ncb_p np);
static	void	ncr_free_nccb	(ncb_p np, nccb_p cp);
static	void	ncr_freeze_devq (ncb_p np, struct cam_path *path);
static	void	ncr_selectclock	(ncb_p np, u_char scntl3);
static	void	ncr_getclock	(ncb_p np, u_char multiplier);
static	nccb_p	ncr_get_nccb	(ncb_p np, u_long t,u_long l);
#if 0
static  u_int32_t ncr_info	(int unit);
#endif
static	void	ncr_init	(ncb_p np, char * msg, u_long code);
static	void	ncr_intr	(void *vnp);
static	void	ncr_int_ma	(ncb_p np, u_char dstat);
static	void	ncr_int_sir	(ncb_p np);
static  void    ncr_int_sto     (ncb_p np);
#if 0
static	void	ncr_min_phys	(struct buf *bp);
#endif
static	void	ncr_poll	(struct cam_sim *sim);
static	void	ncb_profile	(ncb_p np, nccb_p cp);
static	void	ncr_script_copy_and_bind
				(ncb_p np, ncrcmd *src, ncrcmd *dst, int len);
static  void    ncr_script_fill (struct script * scr, struct scripth *scrh);
static	int	ncr_scatter	(struct dsb* phys, vm_offset_t vaddr,
				 vm_size_t datalen);
static	void	ncr_getsync	(ncb_p np, u_char sfac, u_char *fakp,
				 u_char *scntl3p);
static	void	ncr_setsync	(ncb_p np, nccb_p cp,u_char scntl3,u_char sxfer,
				 u_char period);
static	void	ncr_setwide	(ncb_p np, nccb_p cp, u_char wide, u_char ack);
static	int	ncr_show_msg	(u_char * msg);
static	int	ncr_snooptest	(ncb_p np);
static	void	ncr_action	(struct cam_sim *sim, union ccb *ccb);
static	void	ncr_timeout	(void *arg);
static  void    ncr_wakeup	(ncb_p np, u_long code);

static  int	ncr_probe	(device_t dev);
static	int	ncr_attach	(device_t dev);

#endif /* _KERNEL */

/*==========================================================
**
**
**      Global static data.
**
**
**==========================================================
*/


#if !defined(lint)
static const char ident[] =
	"\n$FreeBSD$\n";
#endif

static const u_long	ncr_version = NCR_VERSION	* 11
	+ (u_long) sizeof (struct ncb)	*  7
	+ (u_long) sizeof (struct nccb)	*  5
	+ (u_long) sizeof (struct lcb)	*  3
	+ (u_long) sizeof (struct tcb)	*  2;

#ifdef _KERNEL

static int ncr_debug = SCSI_NCR_DEBUG;
SYSCTL_INT(_debug, OID_AUTO, ncr_debug, CTLFLAG_RW, &ncr_debug, 0, "");

static int ncr_cache; /* to be aligned _NOT_ static */

/*==========================================================
**
**
**      Global static data:	auto configure
**
**
**==========================================================
*/

#define	NCR_810_ID	(0x00011000ul)
#define	NCR_815_ID	(0x00041000ul)
#define	NCR_820_ID	(0x00021000ul)
#define	NCR_825_ID	(0x00031000ul)
#define	NCR_860_ID	(0x00061000ul)
#define	NCR_875_ID	(0x000f1000ul)
#define	NCR_875_ID2	(0x008f1000ul)
#define	NCR_885_ID	(0x000d1000ul)
#define	NCR_895_ID	(0x000c1000ul)
#define	NCR_896_ID	(0x000b1000ul)
#define	NCR_895A_ID	(0x00121000ul)
#define	NCR_1510D_ID	(0x000a1000ul)


static char *ncr_name (ncb_p np)
{
	static char name[10];
	snprintf(name, sizeof(name), "ncr%d", np->unit);
	return (name);
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
#define	RELOC_KVAR	0x70000000
#define	RELOC_LABELH	0x80000000
#define	RELOC_MASK	0xf0000000

#define	NADDR(label)	(RELOC_SOFTC | offsetof(struct ncb, label))
#define PADDR(label)    (RELOC_LABEL | offsetof(struct script, label))
#define PADDRH(label)   (RELOC_LABELH | offsetof(struct scripth, label))
#define	RADDR(label)	(RELOC_REGISTER | REG(label))
#define	FADDR(label,ofs)(RELOC_REGISTER | ((REG(label))+(ofs)))
#define	KVAR(which)	(RELOC_KVAR | (which))

#define KVAR_SECOND			(0)
#define KVAR_TICKS			(1)
#define KVAR_NCR_CACHE			(2)

#define	SCRIPT_KVAR_FIRST		(0)
#define	SCRIPT_KVAR_LAST		(3)

/*
 * Kernel variables referenced in the scripts.
 * THESE MUST ALL BE ALIGNED TO A 4-BYTE BOUNDARY.
 */
static void *script_kvars[] =
	{ &time_second, &ticks, &ncr_cache };

static	struct script script0 = {
/*--------------------------< START >-----------------------*/ {
	/*
	**	Claim to be still alive ...
	*/
	SCR_COPY (sizeof (((struct ncb *)0)->heartbeat)),
		KVAR (KVAR_SECOND),
		NADDR (heartbeat),
	/*
	**      Make data structure address invalid.
	**      clear SIGP.
	*/
	SCR_LOAD_REG (dsa, 0xff),
		0,
	SCR_FROM_REG (ctest2),
		0,
}/*-------------------------< START0 >----------------------*/,{
	/*
	**	Hook for interrupted GetConditionCode.
	**	Will be patched to ... IFTRUE by
	**	the interrupt handler.
	*/
	SCR_INT ^ IFFALSE (0),
		SIR_SENSE_RESTART,

}/*-------------------------< START1 >----------------------*/,{
	/*
	**	Hook for stalled start queue.
	**	Will be patched to IFTRUE by the interrupt handler.
	*/
	SCR_INT ^ IFFALSE (0),
		SIR_STALL_RESTART,
	/*
	**	Then jump to a certain point in tryloop.
	**	Due to the lack of indirect addressing the code
	**	is self modifying here.
	*/
	SCR_JUMP,
}/*-------------------------< STARTPOS >--------------------*/,{
		PADDRH(tryloop),

}/*-------------------------< TRYSEL >----------------------*/,{
	/*
	**	Now:
	**	DSA: Address of a Data Structure
	**	or   Address of the IDLE-Label.
	**
	**	TEMP:	Address of a script, which tries to
	**		start the NEXT entry.
	**
	**	Save the TEMP register into the SCRATCHA register.
	**	Then copy the DSA to TEMP and RETURN.
	**	This is kind of an indirect jump.
	**	(The script processor has NO stack, so the
	**	CALL is actually a jump and link, and the
	**	RETURN is an indirect jump.)
	**
	**	If the slot was empty, DSA contains the address
	**	of the IDLE part of this script. The processor
	**	jumps to IDLE and waits for a reselect.
	**	It will wake up and try the same slot again
	**	after the SIGP bit becomes set by the host.
	**
	**	If the slot was not empty, DSA contains
	**	the address of the phys-part of a nccb.
	**	The processor jumps to this address.
	**	phys starts with head,
	**	head starts with launch,
	**	so actually the processor jumps to
	**	the lauch part.
	**	If the entry is scheduled for execution,
	**	then launch contains a jump to SELECT.
	**	If it's not scheduled, it contains a jump to IDLE.
	*/
	SCR_COPY (4),
		RADDR (temp),
		RADDR (scratcha),
	SCR_COPY (4),
		RADDR (dsa),
		RADDR (temp),
	SCR_RETURN,
		0

}/*-------------------------< SKIP >------------------------*/,{
	/*
	**	This entry has been canceled.
	**	Next time use the next slot.
	*/
	SCR_COPY (4),
		RADDR (scratcha),
		PADDR (startpos),
	/*
	**	patch the launch field.
	**	should look like an idle process.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (skip2),
	SCR_COPY (8),
		PADDR (idle),
}/*-------------------------< SKIP2 >-----------------------*/,{
		0,
	SCR_JUMP,
		PADDR(start),
}/*-------------------------< IDLE >------------------------*/,{
	/*
	**	Nothing to do?
	**	Wait for reselect.
	*/
	SCR_JUMP,
		PADDR(reselect),

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
	SCR_LOAD_REG (HS_REG, 0xff),
		0,

	/*
	**      And try to select this target.
	*/
	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR (reselect),

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
	**	(3) The ncr completes the selection.
	**	Then it will execute the next statement.
	**
	**	(4) There is a selection timeout.
	**	Then the ncr should interrupt the host and stop.
	**	Unfortunately, it seems to continue execution
	**	of the script. But it will fail with an
	**	IID-interrupt on the next WHEN.
	*/

	SCR_JUMPR ^ IFTRUE (WHEN (SCR_MSG_IN)),
		0,

	/*
	**	Send the IDENTIFY and SIMPLE_TAG messages
	**	(and the MSG_EXT_SDTR message)
	*/
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg),
#ifdef undef /* XXX better fail than try to deal with this ... */
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		-16,
#endif
	SCR_CLR (SCR_ATN),
		0,
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	/*
	**	Selection complete.
	**	Next time use the next slot.
	*/
	SCR_COPY (4),
		RADDR (scratcha),
		PADDR (startpos),
}/*-------------------------< PREPARE >----------------------*/,{
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
	**      Mark this nccb as not scheduled.
	*/
	SCR_COPY (8),
		PADDR (idle),
		NADDR (header.launch),
	/*
	**      Set a time stamp for this selection
	*/
	SCR_COPY (sizeof (ticks)),
		KVAR (KVAR_TICKS),
		NADDR (header.stamp.select),
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
	**      Load the synchronous mode register
	*/
	SCR_COPY (1),
		NADDR (sync_st),
		RADDR (sxfer),
	/*
	**      Load the wide mode and timing register
	*/
	SCR_COPY (1),
		NADDR (wide_st),
		RADDR (scntl3),
	/*
	**	Initialize the msgout buffer with a NOOP message.
	*/
	SCR_LOAD_REG (scratcha, MSG_NOOP),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgin),
	/*
	**	Message in phase ?
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	Extended or reject message ?
	*/
	SCR_FROM_REG (sbdl),
		0,
	SCR_JUMP ^ IFTRUE (DATA (MSG_EXTENDED)),
		PADDR (msg_in),
	SCR_JUMP ^ IFTRUE (DATA (MSG_MESSAGE_REJECT)),
		PADDRH (msg_reject),
	/*
	**	normal processing
	*/
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< SETMSG >----------------------*/,{
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_SET (SCR_ATN),
		0,
}/*-------------------------< CLRACK >----------------------*/,{
	/*
	**	Terminate possible pending message phase.
	*/
	SCR_CLR (SCR_ACK),
		0,

}/*-----------------------< DISPATCH >----------------------*/,{
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,
	/*
	**	remove bogus output signals
	*/
	SCR_REG_REG (socl, SCR_AND, CACK|CATN),
		0,
	SCR_RETURN ^ IFTRUE (WHEN (SCR_DATA_OUT)),
		0,
	SCR_RETURN ^ IFTRUE (IF (SCR_DATA_IN)),
		0,
	SCR_JUMP ^ IFTRUE (IF (SCR_MSG_OUT)),
		PADDR (msg_out),
	SCR_JUMP ^ IFTRUE (IF (SCR_MSG_IN)),
		PADDR (msg_in),
	SCR_JUMP ^ IFTRUE (IF (SCR_COMMAND)),
		PADDR (command),
	SCR_JUMP ^ IFTRUE (IF (SCR_STATUS)),
		PADDR (status),
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
}/*-------------------------< CHECKATN >--------------------*/,{
	/*
	**	If AAP (bit 1 of scntl0 register) is set
	**	and a parity error is detected,
	**	the script processor asserts ATN.
	**
	**	The target should switch to a MSG_OUT phase
	**	to get the message.
	*/
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFFALSE (MASK (CATN, CATN)),
		PADDR (dispatch),
	/*
	**	count it
	*/
	SCR_REG_REG (PS_REG, SCR_ADD, 1),
		0,
	/*
	**	Prepare a MSG_INITIATOR_DET_ERR message
	**	(initiator detected error).
	**	The target should retry the transfer.
	*/
	SCR_LOAD_REG (scratcha, MSG_INITIATOR_DET_ERR),
		0,
	SCR_JUMP,
		PADDR (setmsg),

}/*-------------------------< COMMAND >--------------------*/,{
	/*
	**	If this is not a GETCC transfer ...
	*/
	SCR_FROM_REG (SS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFTRUE (DATA (SCSI_STATUS_CHECK_COND)),
		28,
	/*
	**	... set a timestamp ...
	*/
	SCR_COPY (sizeof (ticks)),
		KVAR (KVAR_TICKS),
		NADDR (header.stamp.command),
	/*
	**	... and send the command
	*/
	SCR_MOVE_TBL ^ SCR_COMMAND,
		offsetof (struct dsb, cmd),
	SCR_JUMP,
		PADDR (dispatch),
	/*
	**	Send the GETCC command
	*/
/*>>>*/	SCR_MOVE_TBL ^ SCR_COMMAND,
		offsetof (struct dsb, scmd),
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< STATUS >--------------------*/,{
	/*
	**	set the timestamp.
	*/
	SCR_COPY (sizeof (ticks)),
		KVAR (KVAR_TICKS),
		NADDR (header.stamp.status),
	/*
	**	If this is a GETCC transfer,
	*/
	SCR_FROM_REG (SS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (DATA (SCSI_STATUS_CHECK_COND)),
		40,
	/*
	**	get the status
	*/
	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
	/*
	**	Save status to scsi_status.
	**	Mark as complete.
	**	And wait for disconnect.
	*/
	SCR_TO_REG (SS_REG),
		0,
	SCR_REG_REG (SS_REG, SCR_OR, SCSI_STATUS_SENSE),
		0,
	SCR_LOAD_REG (HS_REG, HS_COMPLETE),
		0,
	SCR_JUMP,
		PADDR (checkatn),
	/*
	**	If it was no GETCC transfer,
	**	save the status to scsi_status.
	*/
/*>>>*/	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
	SCR_TO_REG (SS_REG),
		0,
	/*
	**	if it was no check condition ...
	*/
	SCR_JUMP ^ IFTRUE (DATA (SCSI_STATUS_CHECK_COND)),
		PADDR (checkatn),
	/*
	**	... mark as complete.
	*/
	SCR_LOAD_REG (HS_REG, HS_COMPLETE),
		0,
	SCR_JUMP,
		PADDR (checkatn),

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
	/*
	**	Check for message parity error.
	*/
	SCR_TO_REG (scratcha),
		0,
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	/*
	**	Parity was ok, handle this message.
	*/
	SCR_JUMP ^ IFTRUE (DATA (MSG_CMDCOMPLETE)),
		PADDR (complete),
	SCR_JUMP ^ IFTRUE (DATA (MSG_SAVEDATAPOINTER)),
		PADDR (save_dp),
	SCR_JUMP ^ IFTRUE (DATA (MSG_RESTOREPOINTERS)),
		PADDR (restore_dp),
	SCR_JUMP ^ IFTRUE (DATA (MSG_DISCONNECT)),
		PADDR (disconnect),
	SCR_JUMP ^ IFTRUE (DATA (MSG_EXTENDED)),
		PADDRH (msg_extended),
	SCR_JUMP ^ IFTRUE (DATA (MSG_NOOP)),
		PADDR (clrack),
	SCR_JUMP ^ IFTRUE (DATA (MSG_MESSAGE_REJECT)),
		PADDRH (msg_reject),
	SCR_JUMP ^ IFTRUE (DATA (MSG_IGN_WIDE_RESIDUE)),
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
	SCR_LOAD_REG (scratcha, MSG_MESSAGE_REJECT),
		0,
	SCR_JUMP,
		PADDR (setmsg),

}/*-------------------------< COMPLETE >-----------------*/,{
	/*
	**	Complete message.
	**
	**	If it's not the get condition code,
	**	copy TEMP register to LASTP in header.
	*/
	SCR_FROM_REG (SS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFTRUE (MASK (SCSI_STATUS_SENSE, SCSI_STATUS_SENSE)),
		12,
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.lastp),
/*>>>*/	/*
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
}/*-------------------------< CLEANUP >-------------------*/,{
	/*
	**      dsa:    Pointer to nccb
	**	      or xxxxxxFF (no nccb)
	**
	**      HS_REG:   Host-Status (<>0!)
	*/
	SCR_FROM_REG (dsa),
		0,
	SCR_JUMP ^ IFTRUE (DATA (0xff)),
		PADDR (signal),
	/*
	**      dsa is valid.
	**	save the status registers
	*/
	SCR_COPY (4),
		RADDR (scr0),
		NADDR (header.status),
	/*
	**	and copy back the header to the nccb.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (cleanup0),
	SCR_COPY (sizeof (struct head)),
		NADDR (header),
}/*-------------------------< CLEANUP0 >--------------------*/,{
		0,

	/*
	**	If command resulted in "check condition"
	**	status and is not yet completed,
	**	try to get the condition code.
	*/
	SCR_FROM_REG (HS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (MASK (0, HS_DONEMASK)),
		16,
	SCR_FROM_REG (SS_REG),
		0,
	SCR_JUMP ^ IFTRUE (DATA (SCSI_STATUS_CHECK_COND)),
		PADDRH(getcc2),
}/*-------------------------< SIGNAL >----------------------*/,{
	/*
	**	if status = queue full,
	**	reinsert in startqueue and stall queue.
	*/
/*>>>*/	SCR_FROM_REG (SS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (SCSI_STATUS_QUEUE_FULL)),
		SIR_STALL_QUEUE,
  	/*
	**	And make the DSA register invalid.
	*/
	SCR_LOAD_REG (dsa, 0xff), /* invalid */
		0,
	/*
	**	if job completed ...
	*/
	SCR_FROM_REG (HS_REG),
		0,
	/*
	**	... signal completion to the host
	*/
	SCR_INT_FLY ^ IFFALSE (MASK (0, HS_DONEMASK)),
		0,
	/*
	**	Auf zu neuen Schandtaten!
	*/
	SCR_JUMP,
		PADDR(start),

}/*-------------------------< SAVE_DP >------------------*/,{
	/*
	**	SAVE_DP message:
	**	Copy TEMP register to SAVEP in header.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.savep),
	SCR_JUMP,
		PADDR (clrack),
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
	**	If QUIRK_AUTOSAVE is set,
	**	do an "save pointer" operation.
	*/
	SCR_FROM_REG (QU_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (MASK (QUIRK_AUTOSAVE, QUIRK_AUTOSAVE)),
		12,
	/*
	**	like SAVE_DP message:
	**	Copy TEMP register to SAVEP in header.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.savep),
/*>>>*/	/*
	**	Check if temp==savep or temp==goalp:
	**	if not, log a missing save pointer message.
	**	In fact, it's a comparison mod 256.
	**
	**	Hmmm, I hadn't thought that I would be urged to
	**	write this kind of ugly self modifying code.
	**
	**	It's unbelievable, but the ncr53c8xx isn't able
	**	to subtract one register from another.
	*/
	SCR_FROM_REG (temp),
		0,
	/*
	**	You are not expected to understand this ..
	**
	**	CAUTION: only little endian architectures supported! XXX
	*/
	SCR_COPY_F (1),
		NADDR (header.savep),
		PADDR (disconnect0),
}/*-------------------------< DISCONNECT0 >--------------*/,{
/*<<<*/	SCR_JUMPR ^ IFTRUE (DATA (1)),
		20,
	/*
	**	neither this
	*/
	SCR_COPY_F (1),
		NADDR (header.goalp),
		PADDR (disconnect1),
}/*-------------------------< DISCONNECT1 >--------------*/,{
	SCR_INT ^ IFFALSE (DATA (1)),
		SIR_MISSING_SAVE,
/*>>>*/

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
	**	Profiling:
	**	Set a time stamp,
	**	and count the disconnects.
	*/
	SCR_COPY (sizeof (ticks)),
		KVAR (KVAR_TICKS),
		NADDR (header.stamp.disconnect),
	SCR_COPY (4),
		NADDR (disc_phys),
		RADDR (temp),
	SCR_REG_REG (temp, SCR_ADD, 0x01),
		0,
	SCR_COPY (4),
		RADDR (temp),
		NADDR (disc_phys),
	/*
	**	Status is: DISCONNECTED.
	*/
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	SCR_JUMP,
		PADDR (cleanup),

}/*-------------------------< MSG_OUT >-------------------*/,{
	/*
	**	The target requests a message.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	/*
	**	If it was no ABORT message ...
	*/
	SCR_JUMP ^ IFTRUE (DATA (MSG_ABORT)),
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
	SCR_LOAD_REG (scratcha, MSG_NOOP),
		0,
	SCR_COPY (4),
		RADDR (scratcha),
		NADDR (msgout),
	/*
	**	... and process the next phase
	*/
	SCR_JUMP,
		PADDR (dispatch),

}/*------------------------< BADGETCC >---------------------*/,{
	/*
	**	If SIGP was set, clear it and try again.
	*/
	SCR_FROM_REG (ctest2),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CSIGP,CSIGP)),
		PADDRH (getcc2),
	SCR_INT,
		SIR_SENSE_FAILED,
}/*-------------------------< RESELECT >--------------------*/,{
	/*
	**	This NOP will be patched with LED OFF
	**	SCR_REG_REG (gpreg, SCR_OR, 0x01)
	*/
	SCR_NO_OP,
		0,

	/*
	**	make the DSA invalid.
	*/
	SCR_LOAD_REG (dsa, 0xff),
		0,
	SCR_CLR (SCR_TRG),
		0,
	/*
	**	Sleep waiting for a reselection.
	**	If SIGP is set, special treatment.
	**
	**	Zu allem bereit ..
	*/
	SCR_WAIT_RESEL,
		PADDR(reselect2),
}/*-------------------------< RESELECT1 >--------------------*/,{
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
	**	- struct nccb
	**	to understand what's going on.
	*/
	SCR_REG_SFBR (ssid, SCR_AND, 0x8F),
		0,
	SCR_TO_REG (sdid),
		0,
	SCR_JUMP,
		NADDR (jump_tcb),
}/*-------------------------< RESELECT2 >-------------------*/,{
	/*
	**	This NOP will be patched with LED ON
	**	SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	*/
	SCR_NO_OP,
		0,
	/*
	**	If it's not connected :(
	**	-> interrupted by SIGP bit.
	**	Jump to start.
	*/
	SCR_FROM_REG (ctest2),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CSIGP,CSIGP)),
		PADDR (start),
	SCR_JUMP,
		PADDR (reselect),

}/*-------------------------< RESEL_TMP >-------------------*/,{
	/*
	**	The return address in TEMP
	**	is in fact the data structure address,
	**	so copy it to the DSA register.
	*/
	SCR_COPY (4),
		RADDR (temp),
		RADDR (dsa),
	SCR_JUMP,
		PADDR (prepare),

}/*-------------------------< RESEL_LUN >-------------------*/,{
	/*
	**	come back to this point
	**	to get an IDENTIFY message
	**	Wait for a msg_in phase.
	*/
/*<<<*/	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_IN)),
		48,
	/*
	**	message phase
	**	It's not a sony, it's a trick:
	**	read the data without acknowledging it.
	*/
	SCR_FROM_REG (sbdl),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (MASK (MSG_IDENTIFYFLAG, 0x98)),
		32,
	/*
	**	It WAS an Identify message.
	**	get it and ack it!
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_CLR (SCR_ACK),
		0,
	/*
	**	Mask out the lun.
	*/
	SCR_REG_REG (sfbr, SCR_AND, 0x07),
		0,
	SCR_RETURN,
		0,
	/*
	**	No message phase or no IDENTIFY message:
	**	return 0.
	*/
/*>>>*/	SCR_LOAD_SFBR (0),
		0,
	SCR_RETURN,
		0,

}/*-------------------------< RESEL_TAG >-------------------*/,{
	/*
	**	come back to this point
	**	to get a SIMPLE_TAG message
	**	Wait for a MSG_IN phase.
	*/
/*<<<*/	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_IN)),
		64,
	/*
	**	message phase
	**	It's a trick - read the data
	**	without acknowledging it.
	*/
	SCR_FROM_REG (sbdl),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (DATA (MSG_SIMPLE_Q_TAG)),
		48,
	/*
	**	It WAS a SIMPLE_TAG message.
	**	get it and ack it!
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_CLR (SCR_ACK),
		0,
	/*
	**	Wait for the second byte (the tag)
	*/
/*<<<*/	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_IN)),
		24,
	/*
	**	Get it and ack it!
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_CLR (SCR_ACK|SCR_CARRY),
		0,
	SCR_RETURN,
		0,
	/*
	**	No message phase or no SIMPLE_TAG message
	**	or no second byte: return 0.
	*/
/*>>>*/	SCR_LOAD_SFBR (0),
		0,
	SCR_SET (SCR_CARRY),
		0,
	SCR_RETURN,
		0,

}/*-------------------------< DATA_IN >--------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTER parameter,
**	it is filled in at runtime.
**
**	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN)),
**		PADDR (no_data),
**	SCR_COPY (sizeof (ticks)),
**		KVAR (KVAR_TICKS),
**		NADDR (header.stamp.data),
**	SCR_MOVE_TBL ^ SCR_DATA_IN,
**		offsetof (struct dsb, data[ 0]),
**
**  ##===========< i=1; i<MAX_SCATTER >=========
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN)),
**  ||		PADDR (checkatn),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_IN,
**  ||		offsetof (struct dsb, data[ i]),
**  ##==========================================
**
**	SCR_CALL,
**		PADDR (checkatn),
**	SCR_JUMP,
**		PADDR (no_data),
*/
0
}/*-------------------------< DATA_OUT >-------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTER parameter,
**	it is filled in at runtime.
**
**	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_OUT)),
**		PADDR (no_data),
**	SCR_COPY (sizeof (ticks)),
**		KVAR (KVAR_TICKS),
**		NADDR (header.stamp.data),
**	SCR_MOVE_TBL ^ SCR_DATA_OUT,
**		offsetof (struct dsb, data[ 0]),
**
**  ##===========< i=1; i<MAX_SCATTER >=========
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_OUT,
**  ||		offsetof (struct dsb, data[ i]),
**  ##==========================================
**
**	SCR_CALL,
**		PADDR (dispatch),
**	SCR_JUMP,
**		PADDR (no_data),
**
**---------------------------------------------------------
*/
(u_long)0

}/*--------------------------------------------------------*/
};


static	struct scripth scripth0 = {
/*-------------------------< TRYLOOP >---------------------*/{
/*
**	Load an entry of the start queue into dsa
**	and try to start it by jumping to TRYSEL.
**
**	Because the size depends on the
**	#define MAX_START parameter, it is filled
**	in at runtime.
**
**-----------------------------------------------------------
**
**  ##===========< I=0; i<MAX_START >===========
**  ||	SCR_COPY (4),
**  ||		NADDR (squeue[i]),
**  ||		RADDR (dsa),
**  ||	SCR_CALL,
**  ||		PADDR (trysel),
**  ##==========================================
**
**	SCR_JUMP,
**		PADDRH(tryloop),
**
**-----------------------------------------------------------
*/
0
}/*-------------------------< MSG_PARITY >---------------*/,{
	/*
	**	count it
	*/
	SCR_REG_REG (PS_REG, SCR_ADD, 0x01),
		0,
	/*
	**	send a "message parity error" message.
	*/
	SCR_LOAD_REG (scratcha, MSG_PARITY_ERROR),
		0,
	SCR_JUMP,
		PADDR (setmsg),
}/*-------------------------< MSG_MESSAGE_REJECT >---------------*/,{
	/*
	**	If a negotiation was in progress,
	**	negotiation failed.
	*/
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,
	/*
	**	else make host log this message
	*/
	SCR_INT ^ IFFALSE (DATA (HS_NEGOTIATE)),
		SIR_REJECT_RECEIVED,
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
	**	Check for message parity error.
	*/
	SCR_TO_REG (scratcha),
		0,
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	/*
	**	Size is 0 .. ignore message.
	*/
	SCR_JUMP ^ IFTRUE (DATA (0)),
		PADDR (clrack),
	/*
	**	Size is not 1 .. have to interrupt.
	*/
/*<<<*/	SCR_JUMPR ^ IFFALSE (DATA (1)),
		40,
	/*
	**	Check for residue byte in swide register
	*/
	SCR_FROM_REG (scntl2),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (MASK (WSR, WSR)),
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
/*>>>*/	SCR_FROM_REG (scratcha),
		0,
/*>>>*/	SCR_INT,
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
	**	Check for message parity error.
	*/
	SCR_TO_REG (scratcha),
		0,
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
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
	/*
	**	Check for message parity error.
	*/
	SCR_TO_REG (scratcha),
		0,
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	SCR_JUMP ^ IFTRUE (DATA (MSG_EXT_WDTR)),
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
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
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

	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		SIR_NEGO_PROTO,
	/*
	**	Send the MSG_EXT_WDTR
	*/
	SCR_MOVE_ABS (4) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_CLR (SCR_ATN),
		0,
	SCR_COPY (1),
		RADDR (sfbr),
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
	/*
	**	Check for message parity error.
	*/
	SCR_TO_REG (scratcha),
		0,
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	SCR_JUMP ^ IFTRUE (DATA (MSG_EXT_SDTR)),
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
	SCR_FROM_REG (socl),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CATN, CATN)),
		PADDRH (msg_parity),
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

	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		SIR_NEGO_PROTO,
	/*
	**	Send the MSG_EXT_SDTR
	*/
	SCR_MOVE_ABS (5) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_CLR (SCR_ATN),
		0,
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (msg_out_done),

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

}/*-------------------------< GETCC >-----------------------*/,{
	/*
	**	The ncr doesn't have an indirect load
	**	or store command. So we have to
	**	copy part of the control block to a
	**	fixed place, where we can modify it.
	**
	**	We patch the address part of a COPY command
	**	with the address of the dsa register ...
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDRH (getcc1),
	/*
	**	... then we do the actual copy.
	*/
	SCR_COPY (sizeof (struct head)),
}/*-------------------------< GETCC1 >----------------------*/,{
		0,
		NADDR (header),
	/*
	**	Initialize the status registers
	*/
	SCR_COPY (4),
		NADDR (header.status),
		RADDR (scr0),
}/*-------------------------< GETCC2 >----------------------*/,{
	/*
	**	Get the condition code from a target.
	**
	**	DSA points to a data structure.
	**	Set TEMP to the script location
	**	that receives the condition code.
	**
	**	Because there is no script command
	**	to load a longword into a register,
	**	we use a CALL command.
	*/
/*<<<*/	SCR_CALLR,
		24,
	/*
	**	Get the condition code.
	*/
	SCR_MOVE_TBL ^ SCR_DATA_IN,
		offsetof (struct dsb, sense),
	/*
	**	No data phase may follow!
	*/
	SCR_CALL,
		PADDR (checkatn),
	SCR_JUMP,
		PADDR (no_data),
/*>>>*/

	/*
	**	The CALL jumps to this point.
	**	Prepare for a RESTORE_POINTER message.
	**	Save the TEMP register into the saved pointer.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.savep),
	/*
	**	Load scratcha, because in case of a selection timeout,
	**	the host will expect a new value for startpos in
	**	the scratcha register.
	*/
	SCR_COPY (4),
		PADDR (startpos),
		RADDR (scratcha),
#ifdef NCR_GETCC_WITHMSG
	/*
	**	If QUIRK_NOMSG is set, select without ATN.
	**	and don't send a message.
	*/
	SCR_FROM_REG (QU_REG),
		0,
	SCR_JUMP ^ IFTRUE (MASK (QUIRK_NOMSG, QUIRK_NOMSG)),
		PADDRH(getcc3),
	/*
	**	Then try to connect to the target.
	**	If we are reselected, special treatment
	**	of the current job is required before
	**	accepting the reselection.
	*/
	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR(badgetcc),
	/*
	**	Send the IDENTIFY message.
	**	In case of short transfer, remove ATN.
	*/
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg2),
	SCR_CLR (SCR_ATN),
		0,
	/*
	**	save the first byte of the message.
	*/
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (prepare2),

#endif
}/*-------------------------< GETCC3 >----------------------*/,{
	/*
	**	Try to connect to the target.
	**	If we are reselected, special treatment
	**	of the current job is required before
	**	accepting the reselection.
	**
	**	Silly target won't accept a message.
	**	Select without ATN.
	*/
	SCR_SEL_TBL ^ offsetof (struct dsb, select),
		PADDR(badgetcc),
	/*
	**	Force error if selection timeout
	*/
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_MSG_IN)),
		0,
	/*
	**	don't negotiate.
	*/
	SCR_JUMP,
		PADDR (prepare2),
}/*-------------------------< ABORTTAG >-------------------*/,{
	/*
	**      Abort a bad reselection.
	**	Set the message to ABORT vs. ABORT_TAG
	*/
	SCR_LOAD_REG (scratcha, MSG_ABORT_TAG),
		0,
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
}/*-------------------------< ABORT >----------------------*/,{
	SCR_LOAD_REG (scratcha, MSG_ABORT),
		0,
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
		RADDR (sfbr),
		NADDR (lastmsg),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< SNOOPTEST >-------------------*/,{
	/*
	**	Read the variable.
	*/
	SCR_COPY (4),
		KVAR (KVAR_NCR_CACHE),
		RADDR (scratcha),
	/*
	**	Write the variable.
	*/
	SCR_COPY (4),
		RADDR (temp),
		KVAR (KVAR_NCR_CACHE),
	/*
	**	Read back the variable.
	*/
	SCR_COPY (4),
		KVAR (KVAR_NCR_CACHE),
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

void ncr_script_fill (struct script * scr, struct scripth * scrh)
{
	int	i;
	ncrcmd	*p;

	p = scrh->tryloop;
	for (i=0; i<MAX_START; i++) {
		*p++ =SCR_COPY (4);
		*p++ =NADDR (squeue[i]);
		*p++ =RADDR (dsa);
		*p++ =SCR_CALL;
		*p++ =PADDR (trysel);
	};
	*p++ =SCR_JUMP;
	*p++ =PADDRH(tryloop);

	assert ((char *)p == (char *)&scrh->tryloop + sizeof (scrh->tryloop));

	p = scr->data_in;

	*p++ =SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN));
	*p++ =PADDR (no_data);
	*p++ =SCR_COPY (sizeof (ticks));
	*p++ =(ncrcmd) KVAR (KVAR_TICKS);
	*p++ =NADDR (header.stamp.data);
	*p++ =SCR_MOVE_TBL ^ SCR_DATA_IN;
	*p++ =offsetof (struct dsb, data[ 0]);

	for (i=1; i<MAX_SCATTER; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN));
		*p++ =PADDR (checkatn);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_IN;
		*p++ =offsetof (struct dsb, data[i]);
	};

	*p++ =SCR_CALL;
	*p++ =PADDR (checkatn);
	*p++ =SCR_JUMP;
	*p++ =PADDR (no_data);

	assert ((char *)p == (char *)&scr->data_in + sizeof (scr->data_in));

	p = scr->data_out;

	*p++ =SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_OUT));
	*p++ =PADDR (no_data);
	*p++ =SCR_COPY (sizeof (ticks));
	*p++ =(ncrcmd) KVAR (KVAR_TICKS);
	*p++ =NADDR (header.stamp.data);
	*p++ =SCR_MOVE_TBL ^ SCR_DATA_OUT;
	*p++ =offsetof (struct dsb, data[ 0]);

	for (i=1; i<MAX_SCATTER; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_OUT;
		*p++ =offsetof (struct dsb, data[i]);
	};

	*p++ =SCR_CALL;
	*p++ =PADDR (dispatch);
	*p++ =SCR_JUMP;
	*p++ =PADDR (no_data);

	assert ((char *)p == (char *)&scr->data_out + sizeof (scr->data_out));
}

/*==========================================================
**
**
**	Copy and rebind a script.
**
**
**==========================================================
*/

static void ncr_script_copy_and_bind (ncb_p np, ncrcmd *src, ncrcmd *dst, int len)
{
	ncrcmd  opcode, new, old, tmp1, tmp2;
	ncrcmd	*start, *end;
	int relocs, offset;

	start = src;
	end = src + len/4;
	offset = 0;

	while (src < end) {

		opcode = *src++;
		WRITESCRIPT_OFF(dst, offset, opcode);
		offset += 4;

		/*
		**	If we forget to change the length
		**	in struct script, a field will be
		**	padded with 0. This is an illegal
		**	command.
		*/

		if (opcode == 0) {
			printf ("%s: ERROR0 IN SCRIPT at %d.\n",
				ncr_name(np), (int) (src-start-1));
			DELAY (1000000);
		};

		if (DEBUG_FLAGS & DEBUG_SCRIPT)
			printf ("%p:  <%x>\n",
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
			if ((tmp1 & RELOC_MASK) == RELOC_KVAR)
				tmp1 = 0;
			tmp2 = src[1];
			if ((tmp2 & RELOC_MASK) == RELOC_KVAR)
				tmp2 = 0;
			if ((tmp1 ^ tmp2) & 3) {
				printf ("%s: ERROR1 IN SCRIPT at %d.\n",
					ncr_name(np), (int) (src-start-1));
				DELAY (1000000);
			}
			/*
			**	If PREFETCH feature not enabled, remove 
			**	the NO FLUSH bit if present.
			*/
			if ((opcode & SCR_NO_FLUSH) && !(np->features&FE_PFEN))
				WRITESCRIPT_OFF(dst, offset - 4,
				    (opcode & ~SCR_NO_FLUSH));
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
					new = (old & ~RELOC_MASK) + rman_get_start(np->reg_res);
					break;
				case RELOC_LABEL:
					new = (old & ~RELOC_MASK) + np->p_script;
					break;
				case RELOC_LABELH:
					new = (old & ~RELOC_MASK) + np->p_scripth;
					break;
				case RELOC_SOFTC:
					new = (old & ~RELOC_MASK) + vtophys(np);
					break;
				case RELOC_KVAR:
					if (((old & ~RELOC_MASK) <
					     SCRIPT_KVAR_FIRST) ||
					    ((old & ~RELOC_MASK) >
					     SCRIPT_KVAR_LAST))
						panic("ncr KVAR out of range");
					new = vtophys(script_kvars[old &
					    ~RELOC_MASK]);
					break;
				case 0:
					/* Don't relocate a 0 address. */
					if (old == 0) {
						new = old;
						break;
					}
					/* FALLTHROUGH */
				default:
					panic("ncr_script_copy_and_bind: weird relocation %x @ %d\n", old, (int)(src - start));
					break;
				}

				WRITESCRIPT_OFF(dst, offset, new);
				offset += 4;
			}
		} else {
			WRITESCRIPT_OFF(dst, offset, *src++);
			offset += 4;
		}

	};
}

/*==========================================================
**
**
**      Auto configuration.
**
**
**==========================================================
*/

#if 0
/*----------------------------------------------------------
**
**	Reduce the transfer length to the max value
**	we can transfer safely.
**
**      Reading a block greater then MAX_SIZE from the
**	raw (character) device exercises a memory leak
**	in the vm subsystem. This is common to ALL devices.
**	We have submitted a description of this bug to
**	<FreeBSD-bugs@freefall.cdrom.com>.
**	It should be fixed in the current release.
**
**----------------------------------------------------------
*/

void ncr_min_phys (struct  buf *bp)
{
	if ((unsigned long)bp->b_bcount > MAX_SIZE) bp->b_bcount = MAX_SIZE;
}

#endif

#if 0
/*----------------------------------------------------------
**
**	Maximal number of outstanding requests per target.
**
**----------------------------------------------------------
*/

u_int32_t ncr_info (int unit)
{
	return (1);   /* may be changed later */
}

#endif

/*----------------------------------------------------------
**
**	NCR chip devices table and chip look up function.
**	Features bit are defined in ncrreg.h. Is it the 
**	right place?
**
**----------------------------------------------------------
*/
typedef struct {
	unsigned long	device_id;
	unsigned short	minrevid;
	char	       *name;
	unsigned char	maxburst;
	unsigned char	maxoffs;
	unsigned char	clock_divn;
	unsigned int	features;
} ncr_chip;

static ncr_chip ncr_chip_table[] = {
 {NCR_810_ID, 0x00,	"ncr 53c810 fast10 scsi",		4,  8, 4,
 FE_ERL}
 ,
 {NCR_810_ID, 0x10,	"ncr 53c810a fast10 scsi",		4,  8, 4,
 FE_ERL|FE_LDSTR|FE_PFEN|FE_BOF}
 ,
 {NCR_815_ID, 0x00,	"ncr 53c815 fast10 scsi", 		4,  8, 4,
 FE_ERL|FE_BOF}
 ,
 {NCR_820_ID, 0x00,	"ncr 53c820 fast10 wide scsi", 		4,  8, 4,
 FE_WIDE|FE_ERL}
 ,
 {NCR_825_ID, 0x00,	"ncr 53c825 fast10 wide scsi",		4,  8, 4,
 FE_WIDE|FE_ERL|FE_BOF}
 ,
 {NCR_825_ID, 0x10,	"ncr 53c825a fast10 wide scsi",		7,  8, 4,
 FE_WIDE|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_860_ID, 0x00,	"ncr 53c860 fast20 scsi",		4,  8, 5,
 FE_ULTRA|FE_CLK80|FE_CACHE_SET|FE_LDSTR|FE_PFEN}
 ,
 {NCR_875_ID, 0x00,	"ncr 53c875 fast20 wide scsi",		7, 16, 5,
 FE_WIDE|FE_ULTRA|FE_CLK80|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_875_ID, 0x02,	"ncr 53c875 fast20 wide scsi",		7, 16, 5,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_875_ID2, 0x00,	"ncr 53c875j fast20 wide scsi",		7, 16, 5,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_885_ID, 0x00,	"ncr 53c885 fast20 wide scsi",		7, 16, 5,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_895_ID, 0x00,	"ncr 53c895 fast40 wide scsi",		7, 31, 7,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_896_ID, 0x00,	"ncr 53c896 fast40 wide scsi",		7, 31, 7,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_895A_ID, 0x00,	"ncr 53c895a fast40 wide scsi",		7, 31, 7,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
 ,
 {NCR_1510D_ID, 0x00,	"ncr 53c1510d fast40 wide scsi",	7, 31, 7,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM}
};

static int ncr_chip_lookup(u_long device_id, u_char revision_id)
{
	int i, found;
	
	found = -1;
	for (i = 0; i < sizeof(ncr_chip_table)/sizeof(ncr_chip_table[0]); i++) {
		if (device_id	== ncr_chip_table[i].device_id &&
		    ncr_chip_table[i].minrevid <= revision_id) {
			if (found < 0 || 
			    ncr_chip_table[found].minrevid 
			      < ncr_chip_table[i].minrevid) {
				found = i;
			}
		}
	}
	return found;
}

/*----------------------------------------------------------
**
**	Probe the hostadapter.
**
**----------------------------------------------------------
*/



static	int ncr_probe (device_t dev)
{
	int i;

	i = ncr_chip_lookup(pci_get_devid(dev), pci_get_revid(dev));
	if (i >= 0) {
		device_set_desc(dev, ncr_chip_table[i].name);
		return (0);
	}

	return (ENXIO);
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
static void
ncr_init_burst(ncb_p np, u_char bc)
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

/*==========================================================
**
**
**      Auto configuration:  attach and init a host adapter.
**
**
**==========================================================
*/


static int
ncr_attach (device_t dev)
{
	ncb_p np = (struct ncb*) device_get_softc(dev);
	u_char	 rev = 0;
	u_long	 period;
	int	 i, rid;
	u_int8_t usrsync;
	u_int8_t usrwide;
	struct cam_devq *devq;

	/*
	**	allocate and initialize structures.
	*/

	np->unit = device_get_unit(dev);

	/*
	**	Try to map the controller chip to
	**	virtual and physical memory.
	*/

	np->reg_rid = 0x14;
	np->reg_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &np->reg_rid,
					 0, ~0, 1, RF_ACTIVE);
	if (!np->reg_res) {
		device_printf(dev, "could not map memory\n");
		return ENXIO;
	}

	/*
	**	Make the controller's registers available.
	**	Now the INB INW INL OUTB OUTW OUTL macros
	**	can be used safely.
	*/

	np->bst = rman_get_bustag(np->reg_res);
	np->bsh = rman_get_bushandle(np->reg_res);


#ifdef NCR_IOMAPPED
	/*
	**	Try to map the controller chip into iospace.
	*/

	if (!pci_map_port (config_id, 0x10, &np->port))
		return;
#endif


	/*
	**	Save some controller register default values
	*/

	np->rv_scntl3	= INB(nc_scntl3) & 0x77;
	np->rv_dmode	= INB(nc_dmode)  & 0xce;
	np->rv_dcntl	= INB(nc_dcntl)  & 0xa9;
	np->rv_ctest3	= INB(nc_ctest3) & 0x01;
	np->rv_ctest4	= INB(nc_ctest4) & 0x88;
	np->rv_ctest5	= INB(nc_ctest5) & 0x24;
	np->rv_gpcntl	= INB(nc_gpcntl);
	np->rv_stest2	= INB(nc_stest2) & 0x20;

	if (bootverbose >= 2) {
		printf ("\tBIOS values:  SCNTL3:%02x DMODE:%02x  DCNTL:%02x\n",
			np->rv_scntl3, np->rv_dmode, np->rv_dcntl);
		printf ("\t              CTEST3:%02x CTEST4:%02x CTEST5:%02x\n",
			np->rv_ctest3, np->rv_ctest4, np->rv_ctest5);
	}

	np->rv_dcntl  |= NOCOM;

	/*
	**	Do chip dependent initialization.
	*/

	rev = pci_get_revid(dev);

	/*
	**	Get chip features from chips table.
	*/
	i = ncr_chip_lookup(pci_get_devid(dev), rev);

	if (i >= 0) {
		np->maxburst	= ncr_chip_table[i].maxburst;
		np->maxoffs	= ncr_chip_table[i].maxoffs;
		np->clock_divn	= ncr_chip_table[i].clock_divn;
		np->features	= ncr_chip_table[i].features;
	} else {	/* Should'nt happen if probe() is ok */
		np->maxburst	= 4;
		np->maxoffs	= 8;
		np->clock_divn	= 4;
		np->features	= FE_ERL;
	}

	np->maxwide	= np->features & FE_WIDE ? 1 : 0;
	np->clock_khz	= np->features & FE_CLK80 ? 80000 : 40000;
	if	(np->features & FE_QUAD)	np->multiplier = 4;
	else if	(np->features & FE_DBLR)	np->multiplier = 2;
	else					np->multiplier = 1;

	/*
	**	Get the frequency of the chip's clock.
	**	Find the right value for scntl3.
	*/
	if (np->features & (FE_ULTRA|FE_ULTRA2))
		ncr_getclock(np, np->multiplier);

#ifdef NCR_TEKRAM_EEPROM
	if (bootverbose) {
		printf ("%s: Tekram EEPROM read %s\n",
			ncr_name(np),
			read_tekram_eeprom (np, NULL) ?
			"succeeded" : "failed");
	}
#endif /* NCR_TEKRAM_EEPROM */

	/*
	 *	If scntl3 != 0, we assume BIOS is present.
	 */
	if (np->rv_scntl3)
		np->features |= FE_BIOS;

	/*
	 * Divisor to be used for async (timer pre-scaler).
	 */
	i = np->clock_divn - 1;
	while (i >= 0) {
		--i;
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
	 * Now, some features available with Symbios compatible boards.
	 * LED support through GPIO0 and DIFF support.
	 */

#ifdef	SCSI_NCR_SYMBIOS_COMPAT
	if (!(np->rv_gpcntl & 0x01))
		np->features |= FE_LED0;
#if 0	/* Not safe enough without NVRAM support or user settable option */
	if (!(INB(nc_gpreg) & 0x08))
		np->features |= FE_DIFF;
#endif
#endif	/* SCSI_NCR_SYMBIOS_COMPAT */

	/*
	 * Prepare initial IO registers settings.
	 * Trust BIOS only if we believe we have one and if we want to.
	 */
#ifdef	SCSI_NCR_TRUST_BIOS
	if (!(np->features & FE_BIOS)) {
#else
	if (1) {
#endif
		np->rv_dmode = 0;
		np->rv_dcntl = NOCOM;
		np->rv_ctest3 = 0;
		np->rv_ctest4 = MPEE;
		np->rv_ctest5 = 0;
		np->rv_stest2 = 0;

		if (np->features & FE_ERL)
			np->rv_dmode 	|= ERL;	  /* Enable Read Line */
		if (np->features & FE_BOF)
			np->rv_dmode 	|= BOF;	  /* Burst Opcode Fetch */
		if (np->features & FE_ERMP)
			np->rv_dmode	|= ERMP;  /* Enable Read Multiple */
		if (np->features & FE_CLSE)
			np->rv_dcntl	|= CLSE;  /* Cache Line Size Enable */
		if (np->features & FE_WRIE)
			np->rv_ctest3	|= WRIE;  /* Write and Invalidate */
		if (np->features & FE_PFEN)
			np->rv_dcntl	|= PFEN;  /* Prefetch Enable */
		if (np->features & FE_DFS)
			np->rv_ctest5	|= DFS;	  /* Dma Fifo Size */
		if (np->features & FE_DIFF)	
			np->rv_stest2	|= 0x20;  /* Differential mode */
		ncr_init_burst(np, np->maxburst); /* Max dwords burst length */
	} else {
		np->maxburst =
			burst_code(np->rv_dmode, np->rv_ctest4, np->rv_ctest5);
	}

	/*
	**	Get on-chip SRAM address, if supported
	*/
	if ((np->features & FE_RAM) && sizeof(struct script) <= 4096) {
		np->sram_rid = 0x18;
		np->sram_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
						  &np->sram_rid,
						  0, ~0, 1, RF_ACTIVE);
	}

	/*
	**	Allocate structure for script relocation.
	*/
	if (np->sram_res != NULL) {
		np->script = NULL;
		np->p_script = rman_get_start(np->sram_res);
		np->bst2 = rman_get_bustag(np->sram_res);
		np->bsh2 = rman_get_bushandle(np->sram_res);
	} else if (sizeof (struct script) > PAGE_SIZE) {
		np->script  = (struct script*) vm_page_alloc_contig 
			(round_page(sizeof (struct script)), 
			 0, 0xffffffff, PAGE_SIZE);
	} else {
		np->script  = (struct script *)
			malloc (sizeof (struct script), M_DEVBUF, M_WAITOK);
	}

	/* XXX JGibbs - Use contigmalloc */
	if (sizeof (struct scripth) > PAGE_SIZE) {
		np->scripth = (struct scripth*) vm_page_alloc_contig 
			(round_page(sizeof (struct scripth)), 
			 0, 0xffffffff, PAGE_SIZE);
	} else 
		{
		np->scripth = (struct scripth *)
			malloc (sizeof (struct scripth), M_DEVBUF, M_WAITOK);
	}

#ifdef SCSI_NCR_PCI_CONFIG_FIXUP
	/*
	**	If cache line size is enabled, check PCI config space and 
	**	try to fix it up if necessary.
	*/
#ifdef PCIR_CACHELNSZ	/* To be sure that new PCI stuff is present */
	{
		u_char cachelnsz = pci_read_config(dev, PCIR_CACHELNSZ, 1);
		u_short command  = pci_read_config(dev, PCIR_COMMAND, 2);

		if (!cachelnsz) {
			cachelnsz = 8;
			printf("%s: setting PCI cache line size register to %d.\n",
				ncr_name(np), (int)cachelnsz);
			pci_write_config(dev, PCIR_CACHELNSZ, cachelnsz, 1);
		}

		if (!(command & (1<<4))) {
			command |= (1<<4);
			printf("%s: setting PCI command write and invalidate.\n",
				ncr_name(np));
			pci_write_config(dev, PCIR_COMMAND, command, 2);
		}
	}
#endif /* PCIR_CACHELNSZ */

#endif /* SCSI_NCR_PCI_CONFIG_FIXUP */

	/* Initialize per-target user settings */
	usrsync = 0;
	if (SCSI_NCR_DFLT_SYNC) {
		usrsync = SCSI_NCR_DFLT_SYNC;
		if (usrsync > np->maxsync)
			usrsync = np->maxsync;
		if (usrsync < np->minsync)
			usrsync = np->minsync;
	};

	usrwide = (SCSI_NCR_MAX_WIDE);
	if (usrwide > np->maxwide) usrwide=np->maxwide;

	for (i=0;i<MAX_TARGET;i++) {
		tcb_p tp = &np->target[i];

		tp->tinfo.user.period = usrsync;
		tp->tinfo.user.offset = usrsync != 0 ? np->maxoffs : 0;
		tp->tinfo.user.width = usrwide;
		tp->tinfo.disc_tag = NCR_CUR_DISCENB
				   | NCR_CUR_TAGENB
				   | NCR_USR_DISCENB
				   | NCR_USR_TAGENB;
	}

	/*
	**	Bells and whistles   ;-)
	*/
	if (bootverbose)
		printf("%s: minsync=%d, maxsync=%d, maxoffs=%d, %d dwords burst, %s dma fifo\n",
		ncr_name(np), np->minsync, np->maxsync, np->maxoffs,
		burst_length(np->maxburst),
		(np->rv_ctest5 & DFS) ? "large" : "normal");

	/*
	**	Print some complementary information that can be helpfull.
	*/
	if (bootverbose)
		printf("%s: %s, %s IRQ driver%s\n",
			ncr_name(np),
			np->rv_stest2 & 0x20 ? "differential" : "single-ended",
			np->rv_dcntl & IRQM ? "totem pole" : "open drain",
			np->sram_res ? ", using on-chip SRAM" : "");
			
	/*
	**	Patch scripts to physical addresses
	*/
	ncr_script_fill (&script0, &scripth0);

	if (np->script)
		np->p_script	= vtophys(np->script);
	np->p_scripth	= vtophys(np->scripth);

	ncr_script_copy_and_bind (np, (ncrcmd *) &script0,
			(ncrcmd *) np->script, sizeof(struct script));

	ncr_script_copy_and_bind (np, (ncrcmd *) &scripth0,
		(ncrcmd *) np->scripth, sizeof(struct scripth));

	/*
	**    Patch the script for LED support.
	*/

	if (np->features & FE_LED0) {
		WRITESCRIPT(reselect[0],  SCR_REG_REG(gpreg, SCR_OR,  0x01));
		WRITESCRIPT(reselect1[0], SCR_REG_REG(gpreg, SCR_AND, 0xfe));
		WRITESCRIPT(reselect2[0], SCR_REG_REG(gpreg, SCR_AND, 0xfe));
	}

	/*
	**	init data structure
	*/

	np->jump_tcb.l_cmd	= SCR_JUMP;
	np->jump_tcb.l_paddr	= NCB_SCRIPTH_PHYS (np, abort);

	/*
	**  Get SCSI addr of host adapter (set by bios?).
	*/

	np->myaddr = INB(nc_scid) & 0x07;
	if (!np->myaddr) np->myaddr = SCSI_NCR_MYADDR;

#ifdef NCR_DUMP_REG
	/*
	**	Log the initial register contents
	*/
	{
		int reg;
		for (reg=0; reg<256; reg+=4) {
			if (reg%16==0) printf ("reg[%2x]", reg);
			printf (" %08x", (int)pci_conf_read (config_id, reg));
			if (reg%16==12) printf ("\n");
		}
	}
#endif /* NCR_DUMP_REG */

	/*
	**	Reset chip.
	*/

	OUTB (nc_istat,  SRST);
	DELAY (1000);
	OUTB (nc_istat,  0   );


	/*
	**	Now check the cache handling of the pci chipset.
	*/

	if (ncr_snooptest (np)) {
		printf ("CACHE INCORRECTLY CONFIGURED.\n");
		return EINVAL;
	};

	/*
	**	Install the interrupt handler.
	*/

	rid = 0;
	np->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
					 RF_SHAREABLE | RF_ACTIVE);
	if (np->irq_res == NULL) {
		device_printf(dev,
			      "interruptless mode: reduced performance.\n");
	} else {
		bus_setup_intr(dev, np->irq_res, INTR_TYPE_CAM | INTR_ENTROPY,
			       ncr_intr, np, &np->irq_handle);
	}

	/*
	** Create the device queue.  We only allow MAX_START-1 concurrent
	** transactions so we can be sure to have one element free in our
	** start queue to reset to the idle loop.
	*/
	devq = cam_simq_alloc(MAX_START - 1);
	if (devq == NULL)
		return ENOMEM;

	/*
	**	Now tell the generic SCSI layer
	**	about our bus.
	*/
	np->sim = cam_sim_alloc(ncr_action, ncr_poll, "ncr", np, np->unit,
				1, MAX_TAGS, devq);
	if (np->sim == NULL) {
		cam_simq_free(devq);
		return ENOMEM;
	}

	
	if (xpt_bus_register(np->sim, 0) != CAM_SUCCESS) {
		cam_sim_free(np->sim, /*free_devq*/ TRUE);
		return ENOMEM;
	}
	
	if (xpt_create_path(&np->path, /*periph*/NULL,
			    cam_sim_path(np->sim), CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(np->sim));
		cam_sim_free(np->sim, /*free_devq*/TRUE);
		return ENOMEM;
	}

	/*
	**	start the timeout daemon
	*/
	ncr_timeout (np);
	np->lasttime=0;

	return 0;
}

/*==========================================================
**
**
**	Process pending device interrupts.
**
**
**==========================================================
*/

static void
ncr_intr(vnp)
	void *vnp;
{
	ncb_p np = vnp;
	int oldspl = splcam();

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("[");

	if (INB(nc_istat) & (INTF|SIP|DIP)) {
		/*
		**	Repeat until no outstanding ints
		*/
		do {
			ncr_exception (np);
		} while (INB(nc_istat) & (INTF|SIP|DIP));

		np->ticks = 100;
	};

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("]\n");

	splx (oldspl);
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

static void
ncr_action (struct cam_sim *sim, union ccb *ccb)
{
	ncb_p np;

	np = (ncb_p) cam_sim_softc(sim);

	switch (ccb->ccb_h.func_code) {
	/* Common cases first */
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
	{
		nccb_p cp;
		lcb_p lp;
		tcb_p tp;
		int oldspl;
		struct ccb_scsiio *csio;
		u_int8_t *msgptr;
		u_int msglen;
		u_int msglen2;
		int segments;
		u_int8_t nego;
		u_int8_t idmsg;
		u_int8_t qidx;
		
		tp = &np->target[ccb->ccb_h.target_id];
		csio = &ccb->csio;

		oldspl = splcam();

		/*
		 * Last time we need to check if this CCB needs to
		 * be aborted.
		 */
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
			xpt_done(ccb);
			splx(oldspl);
			return;
		}
		ccb->ccb_h.status |= CAM_SIM_QUEUED;

		/*---------------------------------------------------
		**
		**	Assign an nccb / bind ccb
		**
		**----------------------------------------------------
		*/
		cp = ncr_get_nccb (np, ccb->ccb_h.target_id,
				   ccb->ccb_h.target_lun);
		if (cp == NULL) {
			/* XXX JGibbs - Freeze SIMQ */
			ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
			xpt_done(ccb);
			return;
		};
		
		cp->ccb = ccb;
		
		/*---------------------------------------------------
		**
		**	timestamp
		**
		**----------------------------------------------------
		*/
		/*
		** XXX JGibbs - Isn't this expensive
		**		enough to be conditionalized??
		*/

		bzero (&cp->phys.header.stamp, sizeof (struct tstamp));
		cp->phys.header.stamp.start = ticks;

		nego = 0;
		if (tp->nego_cp == NULL) {
			
			if (tp->tinfo.current.width
			 != tp->tinfo.goal.width) {
				tp->nego_cp = cp;
				nego = NS_WIDE;
			} else if ((tp->tinfo.current.period
				    != tp->tinfo.goal.period)
				|| (tp->tinfo.current.offset
				    != tp->tinfo.goal.offset)) {
				tp->nego_cp = cp;
				nego = NS_SYNC;
			};
		};

		/*---------------------------------------------------
		**
		**	choose a new tag ...
		**
		**----------------------------------------------------
		*/
		lp = tp->lp[ccb->ccb_h.target_lun];

		if ((ccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0
		 && (ccb->csio.tag_action != CAM_TAG_ACTION_NONE)
		 && (nego == 0)) {
			/*
			**	assign a tag to this nccb
			*/
			while (!cp->tag) {
				nccb_p cp2 = lp->next_nccb;
				lp->lasttag = lp->lasttag % 255 + 1;
				while (cp2 && cp2->tag != lp->lasttag)
					cp2 = cp2->next_nccb;
				if (cp2) continue;
				cp->tag=lp->lasttag;
				if (DEBUG_FLAGS & DEBUG_TAGS) {
					PRINT_ADDR(ccb);
					printf ("using tag #%d.\n", cp->tag);
				};
			};
		} else {
			cp->tag=0;
		};

		/*----------------------------------------------------
		**
		**	Build the identify / tag / sdtr message
		**
		**----------------------------------------------------
		*/
		idmsg = MSG_IDENTIFYFLAG | ccb->ccb_h.target_lun;
		if (tp->tinfo.disc_tag & NCR_CUR_DISCENB)
			idmsg |= MSG_IDENTIFY_DISCFLAG;

		msgptr = cp->scsi_smsg;
		msglen = 0;
		msgptr[msglen++] = idmsg;

		if (cp->tag) {
	    		msgptr[msglen++] = ccb->csio.tag_action;
			msgptr[msglen++] = cp->tag;
		}

		switch (nego) {
		case NS_SYNC:
			msgptr[msglen++] = MSG_EXTENDED;
			msgptr[msglen++] = MSG_EXT_SDTR_LEN;
			msgptr[msglen++] = MSG_EXT_SDTR;
			msgptr[msglen++] = tp->tinfo.goal.period;
			msgptr[msglen++] = tp->tinfo.goal.offset;;
			if (DEBUG_FLAGS & DEBUG_NEGO) {
				PRINT_ADDR(ccb);
				printf ("sync msgout: ");
				ncr_show_msg (&cp->scsi_smsg [msglen-5]);
				printf (".\n");
			};
			break;
		case NS_WIDE:
			msgptr[msglen++] = MSG_EXTENDED;
			msgptr[msglen++] = MSG_EXT_WDTR_LEN;
			msgptr[msglen++] = MSG_EXT_WDTR;
			msgptr[msglen++] = tp->tinfo.goal.width;
			if (DEBUG_FLAGS & DEBUG_NEGO) {
				PRINT_ADDR(ccb);
				printf ("wide msgout: ");
				ncr_show_msg (&cp->scsi_smsg [msglen-4]);
				printf (".\n");
			};
			break;
		};

		/*----------------------------------------------------
		**
		**	Build the identify message for getcc.
		**
		**----------------------------------------------------
		*/

		cp->scsi_smsg2 [0] = idmsg;
		msglen2 = 1;

		/*----------------------------------------------------
		**
		**	Build the data descriptors
		**
		**----------------------------------------------------
		*/

		/* XXX JGibbs - Handle other types of I/O */
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
			segments = ncr_scatter(&cp->phys,
					       (vm_offset_t)csio->data_ptr,
					       (vm_size_t)csio->dxfer_len);

			if (segments < 0) {
				ccb->ccb_h.status = CAM_REQ_TOO_BIG;
				ncr_free_nccb(np, cp);
				splx(oldspl);
				xpt_done(ccb);
				return;
			}
			if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
				cp->phys.header.savep = NCB_SCRIPT_PHYS (np, data_in);
				cp->phys.header.goalp = cp->phys.header.savep +20 +segments*16;
			} else { /* CAM_DIR_OUT */
				cp->phys.header.savep = NCB_SCRIPT_PHYS (np, data_out);
				cp->phys.header.goalp = cp->phys.header.savep +20 +segments*16;
			}
		} else {
			cp->phys.header.savep = NCB_SCRIPT_PHYS (np, no_data);
			cp->phys.header.goalp = cp->phys.header.savep;
		}

		cp->phys.header.lastp = cp->phys.header.savep;


		/*----------------------------------------------------
		**
		**	fill in nccb
		**
		**----------------------------------------------------
		**
		**
		**	physical -> virtual backlink
		**	Generic SCSI command
		*/
		cp->phys.header.cp		= cp;
		/*
		**	Startqueue
		*/
		cp->phys.header.launch.l_paddr	= NCB_SCRIPT_PHYS (np, select);
		cp->phys.header.launch.l_cmd	= SCR_JUMP;
		/*
		**	select
		*/
		cp->phys.select.sel_id		= ccb->ccb_h.target_id;
		cp->phys.select.sel_scntl3	= tp->tinfo.wval;
		cp->phys.select.sel_sxfer	= tp->tinfo.sval;
		/*
		**	message
		*/
		cp->phys.smsg.addr		= CCB_PHYS (cp, scsi_smsg);
		cp->phys.smsg.size		= msglen;
	
		cp->phys.smsg2.addr		= CCB_PHYS (cp, scsi_smsg2);
		cp->phys.smsg2.size		= msglen2;
		/*
		**	command
		*/
		/* XXX JGibbs - Support other command types */
		cp->phys.cmd.addr		= vtophys (csio->cdb_io.cdb_bytes);
		cp->phys.cmd.size		= csio->cdb_len;
		/*
		**	sense command
		*/
		cp->phys.scmd.addr		= CCB_PHYS (cp, sensecmd);
		cp->phys.scmd.size		= 6;
		/*
		**	patch requested size into sense command
		*/
		cp->sensecmd[0]			= 0x03;
		cp->sensecmd[1]			= ccb->ccb_h.target_lun << 5;
		cp->sensecmd[4]			= sizeof(struct scsi_sense_data);
		cp->sensecmd[4]			= csio->sense_len;
		/*
		**	sense data
		*/
		cp->phys.sense.addr		= vtophys (&csio->sense_data);
		cp->phys.sense.size		= csio->sense_len;
		/*
		**	status
		*/
		cp->actualquirks		= QUIRK_NOMSG;
		cp->host_status			= nego ? HS_NEGOTIATE : HS_BUSY;
		cp->s_status			= SCSI_STATUS_ILLEGAL;
		cp->parity_status		= 0;
	
		cp->xerr_status			= XE_OK;
		cp->sync_status			= tp->tinfo.sval;
		cp->nego_status			= nego;
		cp->wide_status			= tp->tinfo.wval;

		/*----------------------------------------------------
		**
		**	Critical region: start this job.
		**
		**----------------------------------------------------
		*/

		/*
		**	reselect pattern and activate this job.
		*/

		cp->jump_nccb.l_cmd	= (SCR_JUMP ^ IFFALSE (DATA (cp->tag)));
		cp->tlimit		= time_second
					+ ccb->ccb_h.timeout / 1000 + 2;
		cp->magic		= CCB_MAGIC;

		/*
		**	insert into start queue.
		*/

		qidx = np->squeueput + 1;
		if (qidx >= MAX_START) qidx=0;
		np->squeue [qidx	 ] = NCB_SCRIPT_PHYS (np, idle);
		np->squeue [np->squeueput] = CCB_PHYS (cp, phys);
		np->squeueput = qidx;

		if(DEBUG_FLAGS & DEBUG_QUEUE)
			printf("%s: queuepos=%d tryoffset=%d.\n",
			       ncr_name (np), np->squeueput,
			       (unsigned)(READSCRIPT(startpos[0]) - 
			       (NCB_SCRIPTH_PHYS (np, tryloop))));

		/*
		**	Script processor may be waiting for reselect.
		**	Wake it up.
		*/
		OUTB (nc_istat, SIGP);

		/*
		**	and reenable interrupts
		*/
		splx (oldspl);
		break;
	}
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_TARGET_IO:		/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_SET_TRAN_SETTINGS:
	{
		struct	ccb_trans_settings *cts;
		tcb_p	tp;
		u_int	update_type;
		int	s;

		cts = &ccb->cts;
		update_type = 0;
		if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0)
			update_type |= NCR_TRANS_GOAL;
		if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0)
			update_type |= NCR_TRANS_USER;
		
		s = splcam();
		tp = &np->target[ccb->ccb_h.target_id];
		/* Tag and disc enables */
		if ((cts->valid & CCB_TRANS_DISC_VALID) != 0) {
			if (update_type & NCR_TRANS_GOAL) {
				if ((cts->flags & CCB_TRANS_DISC_ENB) != 0)
					tp->tinfo.disc_tag |= NCR_CUR_DISCENB;
				else
					tp->tinfo.disc_tag &= ~NCR_CUR_DISCENB;
			}

			if (update_type & NCR_TRANS_USER) {
				if ((cts->flags & CCB_TRANS_DISC_ENB) != 0)
					tp->tinfo.disc_tag |= NCR_USR_DISCENB;
				else
					tp->tinfo.disc_tag &= ~NCR_USR_DISCENB;
			}

		}

		if ((cts->valid & CCB_TRANS_TQ_VALID) != 0) {
			if (update_type & NCR_TRANS_GOAL) {
				if ((cts->flags & CCB_TRANS_TAG_ENB) != 0)
					tp->tinfo.disc_tag |= NCR_CUR_TAGENB;
				else
					tp->tinfo.disc_tag &= ~NCR_CUR_TAGENB;
			}

			if (update_type & NCR_TRANS_USER) {
				if ((cts->flags & CCB_TRANS_TAG_ENB) != 0)
					tp->tinfo.disc_tag |= NCR_USR_TAGENB;
				else
					tp->tinfo.disc_tag &= ~NCR_USR_TAGENB;
			}	
		}

		/* Filter bus width and sync negotiation settings */
		if ((cts->valid & CCB_TRANS_BUS_WIDTH_VALID) != 0) {
			if (cts->bus_width > np->maxwide)
				cts->bus_width = np->maxwide;
		}

		if (((cts->valid & CCB_TRANS_SYNC_RATE_VALID) != 0)
		 || ((cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) != 0)) {
			if ((cts->valid & CCB_TRANS_SYNC_RATE_VALID) != 0) {
				if (cts->sync_period != 0
				 && (cts->sync_period < np->minsync))
					cts->sync_period = np->minsync;
			}
			if ((cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) != 0) {
				if (cts->sync_offset == 0)
					cts->sync_period = 0;
				if (cts->sync_offset > np->maxoffs)
					cts->sync_offset = np->maxoffs;
			}
		}
		if ((update_type & NCR_TRANS_USER) != 0) {
			if ((cts->valid & CCB_TRANS_SYNC_RATE_VALID) != 0)
				tp->tinfo.user.period = cts->sync_period;
			if ((cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) != 0)
				tp->tinfo.user.offset = cts->sync_offset;
			if ((cts->valid & CCB_TRANS_BUS_WIDTH_VALID) != 0)
				tp->tinfo.user.width = cts->bus_width;
		}
		if ((update_type & NCR_TRANS_GOAL) != 0) {
			if ((cts->valid & CCB_TRANS_SYNC_RATE_VALID) != 0)
				tp->tinfo.goal.period = cts->sync_period;

			if ((cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) != 0)
				tp->tinfo.goal.offset = cts->sync_offset;

			if ((cts->valid & CCB_TRANS_BUS_WIDTH_VALID) != 0)
				tp->tinfo.goal.width = cts->bus_width;
		}
		splx(s);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	/* Get default/user set transfer settings for the target */
	{
		struct	ccb_trans_settings *cts;
		struct	ncr_transinfo *tinfo;
		tcb_p	tp;		
		int	s;

		cts = &ccb->cts;
		tp = &np->target[ccb->ccb_h.target_id];
		
		s = splcam();
		if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0) {
			tinfo = &tp->tinfo.current;
			if (tp->tinfo.disc_tag & NCR_CUR_DISCENB)
				cts->flags |= CCB_TRANS_DISC_ENB;
			else
				cts->flags &= ~CCB_TRANS_DISC_ENB;

			if (tp->tinfo.disc_tag & NCR_CUR_TAGENB)
				cts->flags |= CCB_TRANS_TAG_ENB;
			else
				cts->flags &= ~CCB_TRANS_TAG_ENB;
		} else {
			tinfo = &tp->tinfo.user;
			if (tp->tinfo.disc_tag & NCR_USR_DISCENB)
				cts->flags |= CCB_TRANS_DISC_ENB;
			else
				cts->flags &= ~CCB_TRANS_DISC_ENB;

			if (tp->tinfo.disc_tag & NCR_USR_TAGENB)
				cts->flags |= CCB_TRANS_TAG_ENB;
			else
				cts->flags &= ~CCB_TRANS_TAG_ENB;
		}

		cts->sync_period = tinfo->period;
		cts->sync_offset = tinfo->offset;
		cts->bus_width = tinfo->width;

		splx(s);

		cts->valid = CCB_TRANS_SYNC_RATE_VALID
			   | CCB_TRANS_SYNC_OFFSET_VALID
			   | CCB_TRANS_BUS_WIDTH_VALID
			   | CCB_TRANS_DISC_VALID
			   | CCB_TRANS_TQ_VALID;

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;
		int	  extended;

		/* XXX JGibbs - I'm sure the NCR uses a different strategy,
		 *		but it should be able to deal with Adaptec
		 *		geometry too.
		 */
		extended = 1;
		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);
		
		if (size_mb > 1024 && extended) {
			ccg->heads = 255;
			ccg->secs_per_track = 63;
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
	{
		OUTB (nc_scntl1, CRST);
		ccb->ccb_h.status = CAM_REQ_CMP;
		DELAY(10000);	/* Wait until our interrupt handler sees it */ 
		xpt_done(ccb);
		break;
	}
	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;
		
		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE;
		if ((np->features & FE_WIDE) != 0)
			cpi->hba_inquiry |= PI_WIDE_16;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = (np->features & FE_WIDE) ? 15 : 7;
		cpi->max_lun = MAX_LUN - 1;
		cpi->initiator_id = np->myaddr;
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Symbios", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

/*==========================================================
**
**
**	Complete execution of a SCSI command.
**	Signal completion to the generic SCSI driver.
**
**
**==========================================================
*/

void
ncr_complete (ncb_p np, nccb_p cp)
{
	union ccb *ccb;
	tcb_p tp;
	lcb_p lp;

	/*
	**	Sanity check
	*/

	if (!cp || (cp->magic!=CCB_MAGIC) || !cp->ccb) return;
	cp->magic = 1;
	cp->tlimit= 0;

	/*
	**	No Reselect anymore.
	*/
	cp->jump_nccb.l_cmd = (SCR_JUMP);

	/*
	**	No starting.
	*/
	cp->phys.header.launch.l_paddr= NCB_SCRIPT_PHYS (np, idle);

	/*
	**	timestamp
	*/
	ncb_profile (np, cp);

	if (DEBUG_FLAGS & DEBUG_TINY)
		printf ("CCB=%x STAT=%x/%x\n", (int)(intptr_t)cp & 0xfff,
			cp->host_status,cp->s_status);

	ccb = cp->ccb;
	cp->ccb = NULL;
	tp = &np->target[ccb->ccb_h.target_id];
	lp = tp->lp[ccb->ccb_h.target_lun];

	/*
	**	We do not queue more than 1 nccb per target 
	**	with negotiation at any time. If this nccb was 
	**	used for negotiation, clear this info in the tcb.
	*/

	if (cp == tp->nego_cp)
		tp->nego_cp = NULL;

	/*
	**	Check for parity errors.
	*/
	/* XXX JGibbs - What about reporting them??? */

	if (cp->parity_status) {
		PRINT_ADDR(ccb);
		printf ("%d parity error(s), fallback.\n", cp->parity_status);
		/*
		**	fallback to asynch transfer.
		*/
		tp->tinfo.goal.period = 0;
		tp->tinfo.goal.offset = 0;
	};

	/*
	**	Check for extended errors.
	*/

	if (cp->xerr_status != XE_OK) {
		PRINT_ADDR(ccb);
		switch (cp->xerr_status) {
		case XE_EXTRA_DATA:
			printf ("extraneous data discarded.\n");
			break;
		case XE_BAD_PHASE:
			printf ("illegal scsi phase (4/5).\n");
			break;
		default:
			printf ("extended error %d.\n", cp->xerr_status);
			break;
		};
		if (cp->host_status==HS_COMPLETE)
			cp->host_status = HS_FAIL;
	};

	/*
	**	Check the status.
	*/
	if (cp->host_status == HS_COMPLETE) {

		if (cp->s_status == SCSI_STATUS_OK) {

			/*
			**	All went well.
			*/
			/* XXX JGibbs - Properly calculate residual */

			tp->bytes     += ccb->csio.dxfer_len;
			tp->transfers ++;

			ccb->ccb_h.status = CAM_REQ_CMP;
		} else if ((cp->s_status & SCSI_STATUS_SENSE) != 0) {

			/*
			 * XXX Could be TERMIO too.  Should record
			 * original status.
			 */
			ccb->csio.scsi_status = SCSI_STATUS_CHECK_COND;
			cp->s_status &= ~SCSI_STATUS_SENSE;
			if (cp->s_status == SCSI_STATUS_OK) {
				ccb->ccb_h.status =
				    CAM_AUTOSNS_VALID|CAM_SCSI_STATUS_ERROR;
			} else {
				ccb->ccb_h.status = CAM_AUTOSENSE_FAIL;
			}
		} else {
			ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;			
			ccb->csio.scsi_status = cp->s_status;
		}
		
		
	} else if (cp->host_status == HS_SEL_TIMEOUT) {

		/*
		**   Device failed selection
		*/
		ccb->ccb_h.status = CAM_SEL_TIMEOUT;

	} else if (cp->host_status == HS_TIMEOUT) {

		/*
		**   No response
		*/
		ccb->ccb_h.status = CAM_CMD_TIMEOUT;
	} else if (cp->host_status == HS_STALL) {
		ccb->ccb_h.status = CAM_REQUEUE_REQ;
	} else {

		/*
		**  Other protocol messes
		*/
		PRINT_ADDR(ccb);
		printf ("COMMAND FAILED (%x %x) @%p.\n",
			cp->host_status, cp->s_status, cp);

		ccb->ccb_h.status = CAM_CMD_TIMEOUT;
	}

	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
		ccb->ccb_h.status |= CAM_DEV_QFRZN;
	}

	/*
	**	Free this nccb
	*/
	ncr_free_nccb (np, cp);

	/*
	**	signal completion to generic driver.
	*/
	xpt_done (ccb);
}

/*==========================================================
**
**
**	Signal all (or one) control block done.
**
**
**==========================================================
*/

void
ncr_wakeup (ncb_p np, u_long code)
{
	/*
	**	Starting at the default nccb and following
	**	the links, complete all jobs with a
	**	host_status greater than "disconnect".
	**
	**	If the "code" parameter is not zero,
	**	complete all jobs that are not IDLE.
	*/

	nccb_p cp = np->link_nccb;
	while (cp) {
		switch (cp->host_status) {

		case HS_IDLE:
			break;

		case HS_DISCONNECT:
			if(DEBUG_FLAGS & DEBUG_TINY) printf ("D");
			/* FALLTHROUGH */

		case HS_BUSY:
		case HS_NEGOTIATE:
			if (!code) break;
			cp->host_status = code;

			/* FALLTHROUGH */

		default:
			ncr_complete (np, cp);
			break;
		};
		cp = cp -> link_nccb;
	};
}

static void
ncr_freeze_devq (ncb_p np, struct cam_path *path)
{
	nccb_p	cp;
	int	i;
	int	count;
	int	firstskip;
	/*
	**	Starting at the first nccb and following
	**	the links, complete all jobs that match
	**	the passed in path and are in the start queue.
	*/

	cp = np->link_nccb;
	count = 0;
	firstskip = 0;
	while (cp) {
		switch (cp->host_status) {

		case HS_BUSY:
		case HS_NEGOTIATE:
			if ((cp->phys.header.launch.l_paddr
			    == NCB_SCRIPT_PHYS (np, select))
			 && (xpt_path_comp(path, cp->ccb->ccb_h.path) >= 0)) {

				/* Mark for removal from the start queue */
				for (i = 1; i < MAX_START; i++) {
					int idx;

					idx = np->squeueput - i;
				
					if (idx < 0)
						idx = MAX_START + idx;
					if (np->squeue[idx]
					 == CCB_PHYS(cp, phys)) {
						np->squeue[idx] =
						    NCB_SCRIPT_PHYS (np, skip);
						if (i > firstskip)
							firstskip = i;
						break;
					}
				}
				cp->host_status=HS_STALL;
				ncr_complete (np, cp);
				count++;
			}
			break;
		default:
			break;
		}
		cp = cp->link_nccb;
	}

	if (count > 0) {
		int j;
		int bidx;

		/* Compress the start queue */
		j = 0;
		bidx = np->squeueput;
		i = np->squeueput - firstskip;
		if (i < 0)
			i = MAX_START + i;
		for (;;) {

			bidx = i - j;
			if (bidx < 0)
				bidx = MAX_START + bidx;
			
			if (np->squeue[i] == NCB_SCRIPT_PHYS (np, skip)) {
				j++;
			} else if (j != 0) {
				np->squeue[bidx] = np->squeue[i];
				if (np->squeue[bidx]
				 == NCB_SCRIPT_PHYS(np, idle))
					break;
			}
			i = (i + 1) % MAX_START;
		}
		np->squeueput = bidx;
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

void
ncr_init(ncb_p np, char * msg, u_long code)
{
	int	i;

	/*
	**	Reset chip.
	*/

	OUTB (nc_istat,  SRST);
	DELAY (1000);
	OUTB (nc_istat, 0);

	/*
	**	Message.
	*/

	if (msg) printf ("%s: restart (%s).\n", ncr_name (np), msg);

	/*
	**	Clear Start Queue
	*/

	for (i=0;i<MAX_START;i++)
		np -> squeue [i] = NCB_SCRIPT_PHYS (np, idle);

	/*
	**	Start at first entry.
	*/

	np->squeueput = 0;
	WRITESCRIPT(startpos[0], NCB_SCRIPTH_PHYS (np, tryloop));
	WRITESCRIPT(start0  [0], SCR_INT ^ IFFALSE (0));

	/*
	**	Wakeup all pending jobs.
	*/

	ncr_wakeup (np, code);

	/*
	**	Init chip.
	*/

	OUTB (nc_istat,  0x00   );      /*  Remove Reset, abort ...	     */
	OUTB (nc_scntl0, 0xca   );      /*  full arb., ena parity, par->ATN  */
	OUTB (nc_scntl1, 0x00	);	/*  odd parity, and remove CRST!!    */
	ncr_selectclock(np, np->rv_scntl3); /* Select SCSI clock             */
	OUTB (nc_scid  , RRE|np->myaddr);/*  host adapter SCSI address       */
	OUTW (nc_respid, 1ul<<np->myaddr);/*  id to respond to		     */
	OUTB (nc_istat , SIGP	);	/*  Signal Process		     */
	OUTB (nc_dmode , np->rv_dmode);	/* XXX modify burstlen ??? */
	OUTB (nc_dcntl , np->rv_dcntl);
	OUTB (nc_ctest3, np->rv_ctest3);
	OUTB (nc_ctest5, np->rv_ctest5);
	OUTB (nc_ctest4, np->rv_ctest4);/*  enable master parity checking    */
	OUTB (nc_stest2, np->rv_stest2|EXT); /* Extended Sreq/Sack filtering */
	OUTB (nc_stest3, TE     );	/*  TolerANT enable		     */
	OUTB (nc_stime0, 0x0b	);	/*  HTH = disabled, STO = 0.1 sec.   */

	if (bootverbose >= 2) {
		printf ("\tACTUAL values:SCNTL3:%02x DMODE:%02x  DCNTL:%02x\n",
			np->rv_scntl3, np->rv_dmode, np->rv_dcntl);
		printf ("\t              CTEST3:%02x CTEST4:%02x CTEST5:%02x\n",
			np->rv_ctest3, np->rv_ctest4, np->rv_ctest5);
	}

	/*
	**    Enable GPIO0 pin for writing if LED support.
	*/

	if (np->features & FE_LED0) {
		OUTOFFB (nc_gpcntl, 0x01);
	}

	/*
	**	Fill in target structure.
	*/
	for (i=0;i<MAX_TARGET;i++) {
		tcb_p tp = &np->target[i];

		tp->tinfo.sval    = 0;
		tp->tinfo.wval    = np->rv_scntl3;

		tp->tinfo.current.period = 0;
		tp->tinfo.current.offset = 0;
		tp->tinfo.current.width = MSG_EXT_WDTR_BUS_8_BIT;
	}

	/*
	**      enable ints
	*/

	OUTW (nc_sien , STO|HTH|MA|SGE|UDC|RST);
	OUTB (nc_dien , MDPE|BF|ABRT|SSI|SIR|IID);

	/*
	**    Start script processor.
	*/

	OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, start));

	/*
	 * Notify the XPT of the event
	 */
	if (code == HS_RESET)
		xpt_async(AC_BUS_RESET, np->path, NULL);
}

static void
ncr_poll(struct cam_sim *sim)
{       
	ncr_intr(cam_sim_softc(sim));  
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
		if (kpc >= (div_10M[div] * 4)) break;

	/*
	**	Calculate the lowest clock factor that allows an output 
	**	speed not faster than the period.
	*/
	fak = (kpc - 1) / div_10M[div] + 1;

#if 0	/* You can #if 1 if you think this optimization is usefull */

	per = (fak * div_10M[div]) / clk;

	/*
	**	Why not to try the immediate lower divisor and to choose 
	**	the one that allows the fastest output speed ?
	**	We dont want input speed too much greater than output speed.
	*/
	if (div >= 1 && fak < 6) {
		u_long fak2, per2;
		fak2 = (kpc - 1) / div_10M[div-1] + 1;
		per2 = (fak2 * div_10M[div-1]) / clk;
		if (per2 < per && fak2 <= 6) {
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
**	Switch sync mode for current job and its target
**
**==========================================================
*/

static void
ncr_setsync(ncb_p np, nccb_p cp, u_char scntl3, u_char sxfer, u_char period)
{
	union	ccb *ccb;
	struct	ccb_trans_settings neg;	
	tcb_p	tp;
	int	div;
	u_int	target = INB (nc_sdid) & 0x0f;
	u_int	period_10ns;

	assert (cp);
	if (!cp) return;

	ccb = cp->ccb;
	assert (ccb);
	if (!ccb) return;
	assert (target == ccb->ccb_h.target_id);

	tp = &np->target[target];

	if (!scntl3 || !(sxfer & 0x1f))
		scntl3 = np->rv_scntl3;
	scntl3 = (scntl3 & 0xf0) | (tp->tinfo.wval & EWS)
	       | (np->rv_scntl3 & 0x07);

	/*
	**	Deduce the value of controller sync period from scntl3.
	**	period is in tenths of nano-seconds.
	*/

	div = ((scntl3 >> 4) & 0x7);
	if ((sxfer & 0x1f) && div)
		period_10ns =
		    (((sxfer>>5)+4)*div_10M[div-1])/np->clock_khz;
	else
		period_10ns = 0;

	tp->tinfo.goal.period = period;
	tp->tinfo.goal.offset = sxfer & 0x1f;
	tp->tinfo.current.period = period;
	tp->tinfo.current.offset = sxfer & 0x1f;

	/*
	**	 Stop there if sync parameters are unchanged
	*/
	if (tp->tinfo.sval == sxfer && tp->tinfo.wval == scntl3) return;
	tp->tinfo.sval = sxfer;
	tp->tinfo.wval = scntl3;

	if (sxfer & 0x1f) {
		/*
		**  Disable extended Sreq/Sack filtering
		*/
		if (period_10ns <= 2000) OUTOFFB (nc_stest2, EXT);
	}

	/*
	** Tell the SCSI layer about the
	** new transfer parameters.
	*/
	neg.sync_period = period;
	neg.sync_offset = sxfer & 0x1f;
	neg.valid = CCB_TRANS_SYNC_RATE_VALID
		| CCB_TRANS_SYNC_OFFSET_VALID;
	xpt_setup_ccb(&neg.ccb_h, ccb->ccb_h.path,
		      /*priority*/1);
	xpt_async(AC_TRANSFER_NEG, ccb->ccb_h.path, &neg);
	
	/*
	**	set actual value and sync_status
	*/
	OUTB (nc_sxfer, sxfer);
	np->sync_st = sxfer;
	OUTB (nc_scntl3, scntl3);
	np->wide_st = scntl3;

	/*
	**	patch ALL nccbs of this target.
	*/
	for (cp = np->link_nccb; cp; cp = cp->link_nccb) {
		if (!cp->ccb) continue;
		if (cp->ccb->ccb_h.target_id != target) continue;
		cp->sync_status = sxfer;
		cp->wide_status = scntl3;
	};
}

/*==========================================================
**
**	Switch wide mode for current job and its target
**	SCSI specs say: a SCSI device that accepts a WDTR 
**	message shall reset the synchronous agreement to 
**	asynchronous mode.
**
**==========================================================
*/

static void ncr_setwide (ncb_p np, nccb_p cp, u_char wide, u_char ack)
{
	union	ccb *ccb;
	struct	ccb_trans_settings neg;		
	u_int	target = INB (nc_sdid) & 0x0f;
	tcb_p	tp;
	u_char	scntl3;
	u_char	sxfer;

	assert (cp);
	if (!cp) return;

	ccb = cp->ccb;
	assert (ccb);
	if (!ccb) return;
	assert (target == ccb->ccb_h.target_id);

	tp = &np->target[target];
	tp->tinfo.current.width = wide;
	tp->tinfo.goal.width = wide;
	tp->tinfo.current.period = 0;
	tp->tinfo.current.offset = 0;

	scntl3 = (tp->tinfo.wval & (~EWS)) | (wide ? EWS : 0);

	sxfer = ack ? 0 : tp->tinfo.sval;

	/*
	**	 Stop there if sync/wide parameters are unchanged
	*/
	if (tp->tinfo.sval == sxfer && tp->tinfo.wval == scntl3) return;
	tp->tinfo.sval = sxfer;
	tp->tinfo.wval = scntl3;

	/* Tell the SCSI layer about the new transfer params */
	neg.bus_width = (scntl3 & EWS) ? MSG_EXT_WDTR_BUS_16_BIT
		                       : MSG_EXT_WDTR_BUS_8_BIT;
	neg.sync_period = 0;
	neg.sync_offset = 0;
	neg.valid = CCB_TRANS_BUS_WIDTH_VALID
		  | CCB_TRANS_SYNC_RATE_VALID
		  | CCB_TRANS_SYNC_OFFSET_VALID;
	xpt_setup_ccb(&neg.ccb_h, ccb->ccb_h.path,
		      /*priority*/1);
	xpt_async(AC_TRANSFER_NEG, ccb->ccb_h.path, &neg);	

	/*
	**	set actual value and sync_status
	*/
	OUTB (nc_sxfer, sxfer);
	np->sync_st = sxfer;
	OUTB (nc_scntl3, scntl3);
	np->wide_st = scntl3;

	/*
	**	patch ALL nccbs of this target.
	*/
	for (cp = np->link_nccb; cp; cp = cp->link_nccb) {
		if (!cp->ccb) continue;
		if (cp->ccb->ccb_h.target_id != target) continue;
		cp->sync_status = sxfer;
		cp->wide_status = scntl3;
	};
}

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

static void
ncr_timeout (void *arg)
{
	ncb_p	np = arg;
	time_t	thistime = time_second;
	ticks_t	step  = np->ticks;
	u_long	count = 0;
	long signed   t;
	nccb_p cp;

	if (np->lasttime != thistime) {
		/*
		**	block ncr interrupts
		*/
		int oldspl = splcam();
		np->lasttime = thistime;

		/*----------------------------------------------------
		**
		**	handle ncr chip timeouts
		**
		**	Assumption:
		**	We have a chance to arbitrate for the
		**	SCSI bus at least every 10 seconds.
		**
		**----------------------------------------------------
		*/

		t = thistime - np->heartbeat;

		if (t<2) np->latetime=0; else np->latetime++;

		if (np->latetime>2) {
			/*
			**      If there are no requests, the script
			**      processor will sleep on SEL_WAIT_RESEL.
			**      But we have to check whether it died.
			**      Let's try to wake it up.
			*/
			OUTB (nc_istat, SIGP);
		};

		/*----------------------------------------------------
		**
		**	handle nccb timeouts
		**
		**----------------------------------------------------
		*/

		for (cp=np->link_nccb; cp; cp=cp->link_nccb) {
			/*
			**	look for timed out nccbs.
			*/
			if (!cp->host_status) continue;
			count++;
			if (cp->tlimit > thistime) continue;

			/*
			**	Disable reselect.
			**      Remove it from startqueue.
			*/
			cp->jump_nccb.l_cmd = (SCR_JUMP);
			if (cp->phys.header.launch.l_paddr ==
				NCB_SCRIPT_PHYS (np, select)) {
				printf ("%s: timeout nccb=%p (skip)\n",
					ncr_name (np), cp);
				cp->phys.header.launch.l_paddr
				= NCB_SCRIPT_PHYS (np, skip);
			};

			switch (cp->host_status) {

			case HS_BUSY:
			case HS_NEGOTIATE:
				/* FALLTHROUGH */
			case HS_DISCONNECT:
				cp->host_status=HS_TIMEOUT;
			};
			cp->tag = 0;

			/*
			**	wakeup this nccb.
			*/
			ncr_complete (np, cp);
		};
		splx (oldspl);
	}

	np->timeout_ch =
		timeout (ncr_timeout, (caddr_t) np, step ? step : 1);

	if (INB(nc_istat) & (INTF|SIP|DIP)) {

		/*
		**	Process pending interrupts.
		*/

		int	oldspl	= splcam();
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("{");
		ncr_exception (np);
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("}");
		splx (oldspl);
	};
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
	u_int32_t dsp;
	int	script_ofs;
	int	script_size;
	char	*script_name;
	u_char	*script_base;
	int	i;

	dsp	= INL (nc_dsp);

	if (np->p_script < dsp && 
	    dsp <= np->p_script + sizeof(struct script)) {
		script_ofs	= dsp - np->p_script;
		script_size	= sizeof(struct script);
		script_base	= (u_char *) np->script;
		script_name	= "script";
	}
	else if (np->p_scripth < dsp && 
		 dsp <= np->p_scripth + sizeof(struct scripth)) {
		script_ofs	= dsp - np->p_scripth;
		script_size	= sizeof(struct scripth);
		script_base	= (u_char *) np->scripth;
		script_name	= "scripth";
	} else {
		script_ofs	= dsp;
		script_size	= 0;
		script_base	= 0;
		script_name	= "mem";
	}

	printf ("%s:%d: ERROR (%x:%x) (%x-%x-%x) (%x/%x) @ (%s %x:%08x).\n",
		ncr_name (np), (unsigned)INB (nc_sdid)&0x0f, dstat, sist,
		(unsigned)INB (nc_socl), (unsigned)INB (nc_sbcl), (unsigned)INB (nc_sbdl),
		(unsigned)INB (nc_sxfer),(unsigned)INB (nc_scntl3), script_name, script_ofs,
		(unsigned)INL (nc_dbc));

	if (((script_ofs & 3) == 0) &&
	    (unsigned)script_ofs < script_size) {
		printf ("%s: script cmd = %08x\n", ncr_name(np),
			(int)READSCRIPT_OFF(script_base, script_ofs));
	}

        printf ("%s: regdump:", ncr_name(np));
        for (i=0; i<16;i++)
            printf (" %02x", (unsigned)INB_OFF(i));
        printf (".\n");
}

/*==========================================================
**
**
**	ncr chip exception handler.
**
**
**==========================================================
*/

void ncr_exception (ncb_p np)
{
	u_char	istat, dstat;
	u_short	sist;

	/*
	**	interrupt on the fly ?
	*/
	while ((istat = INB (nc_istat)) & INTF) {
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("F ");
		OUTB (nc_istat, INTF);
		np->profile.num_fly++;
		ncr_wakeup (np, 0);
	};
	if (!(istat & (SIP|DIP))) {
		return;
	}

	/*
	**	Steinbach's Guideline for Systems Programming:
	**	Never test for an error condition you don't know how to handle.
	*/

	sist  = (istat & SIP) ? INW (nc_sist)  : 0;
	dstat = (istat & DIP) ? INB (nc_dstat) : 0;
	np->profile.num_int++;

	if (DEBUG_FLAGS & DEBUG_TINY)
		printf ("<%d|%x:%x|%x:%x>",
			INB(nc_scr0),
			dstat,sist,
			(unsigned)INL(nc_dsp),
			(unsigned)INL(nc_dbc));
	if ((dstat==DFE) && (sist==PAR)) return;

/*==========================================================
**
**	First the normal cases.
**
**==========================================================
*/
	/*-------------------------------------------
	**	SCSI reset
	**-------------------------------------------
	*/

	if (sist & RST) {
		ncr_init (np, bootverbose ? "scsi reset" : NULL, HS_RESET);
		return;
	};

	/*-------------------------------------------
	**	selection timeout
	**
	**	IID excluded from dstat mask!
	**	(chip bug)
	**-------------------------------------------
	*/

	if ((sist  & STO) &&
		!(sist  & (GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR))) {
		ncr_int_sto (np);
		return;
	};

	/*-------------------------------------------
	**      Phase mismatch.
	**-------------------------------------------
	*/

	if ((sist  & MA) &&
		!(sist  & (STO|GEN|HTH|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR|IID))) {
		ncr_int_ma (np, dstat);
		return;
	};

	/*----------------------------------------
	**	move command with length 0
	**----------------------------------------
	*/

	if ((dstat & IID) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR)) &&
		((INL(nc_dbc) & 0xf8000000) == SCR_MOVE_TBL)) {
		/*
		**      Target wants more data than available.
		**	The "no_data" script will do it.
		*/
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, no_data));
		return;
	};

	/*-------------------------------------------
	**	Programmed interrupt
	**-------------------------------------------
	*/

	if ((dstat & SIR) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|IID)) &&
		(INB(nc_dsps) <= SIR_MAX)) {
		ncr_int_sir (np);
		return;
	};

	/*========================================
	**	log message for real hard errors
	**========================================
	*/

	ncr_log_hard_error(np, sist, dstat);

	/*========================================
	**	do the register dump
	**========================================
	*/

	if (time_second - np->regtime > 10) {
		int i;
		np->regtime = time_second;
		for (i=0; i<sizeof(np->regdump); i++)
			((volatile char*)&np->regdump)[i] = INB_OFF(i);
		np->regdump.nc_dstat = dstat;
		np->regdump.nc_sist  = sist;
	};


	/*----------------------------------------
	**	clean up the dma fifo
	**----------------------------------------
	*/

	if ( (INB(nc_sstat0) & (ILF|ORF|OLF)   ) ||
	     (INB(nc_sstat1) & (FF3210)	) ||
	     (INB(nc_sstat2) & (ILF1|ORF1|OLF1)) ||	/* wide .. */
	     !(dstat & DFE)) {
		printf ("%s: have to clear fifos.\n", ncr_name (np));
		OUTB (nc_stest3, TE|CSF);	/* clear scsi fifo */
		OUTB (nc_ctest3, np->rv_ctest3 | CLF);
						/* clear dma fifo  */
	}

	/*----------------------------------------
	**	handshake timeout
	**----------------------------------------
	*/

	if (sist & HTH) {
		printf ("%s: handshake timeout\n", ncr_name(np));
		OUTB (nc_scntl1, CRST);
		DELAY (1000);
		OUTB (nc_scntl1, 0x00);
		OUTB (nc_scr0, HS_FAIL);
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, cleanup));
		return;
	}

	/*----------------------------------------
	**	unexpected disconnect
	**----------------------------------------
	*/

	if ((sist  & UDC) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR|IID))) {
		OUTB (nc_scr0, HS_UNEXPECTED);
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, cleanup));
		return;
	};

	/*----------------------------------------
	**	cannot disconnect
	**----------------------------------------
	*/

	if ((dstat & IID) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR)) &&
		((INL(nc_dbc) & 0xf8000000) == SCR_WAIT_DISC)) {
		/*
		**      Unexpected data cycle while waiting for disconnect.
		*/
		if (INB(nc_sstat2) & LDSC) {
			/*
			**	It's an early reconnect.
			**	Let's continue ...
			*/
			OUTB (nc_dcntl, np->rv_dcntl | STD);
			/*
			**	info message
			*/
			printf ("%s: INFO: LDSC while IID.\n",
				ncr_name (np));
			return;
		};
		printf ("%s: target %d doesn't release the bus.\n",
			ncr_name (np), INB (nc_sdid)&0x0f);
		/*
		**	return without restarting the NCR.
		**	timeout will do the real work.
		*/
		return;
	};

	/*----------------------------------------
	**	single step
	**----------------------------------------
	*/

	if ((dstat & SSI) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR|IID))) {
		OUTB (nc_dcntl, np->rv_dcntl | STD);
		return;
	};

/*
**	@RECOVER@ HTH, SGE, ABRT.
**
**	We should try to recover from these interrupts.
**	They may occur if there are problems with synch transfers, or 
**	if targets are switched on or off while the driver is running.
*/

	if (sist & SGE) {
		/* clear scsi offsets */
		OUTB (nc_ctest3, np->rv_ctest3 | CLF);
	}

	/*
	**	Freeze controller to be able to read the messages.
	*/

	if (DEBUG_FLAGS & DEBUG_FREEZE) {
		int i;
		unsigned char val;
		for (i=0; i<0x60; i++) {
			switch (i%16) {

			case 0:
				printf ("%s: reg[%d0]: ",
					ncr_name(np),i/16);
				break;
			case 4:
			case 8:
			case 12:
				printf (" ");
				break;
			};
			val = bus_space_read_1(np->bst, np->bsh, i);
			printf (" %x%x", val/16, val%16);
			if (i%16==15) printf (".\n");
		};

		untimeout (ncr_timeout, (caddr_t) np, np->timeout_ch);

		printf ("%s: halted!\n", ncr_name(np));
		/*
		**	don't restart controller ...
		*/
		OUTB (nc_istat,  SRST);
		return;
	};

#ifdef NCR_FREEZE
	/*
	**	Freeze system to be able to read the messages.
	*/
	printf ("ncr: fatal error: system halted - press reset to reboot ...");
	(void) splhigh();
	for (;;);
#endif

	/*
	**	sorry, have to kill ALL jobs ...
	*/

	ncr_init (np, "fatal error", HS_FAIL);
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
	u_long dsa, scratcha, diff;
	nccb_p cp;
	if (DEBUG_FLAGS & DEBUG_TINY) printf ("T");

	/*
	**	look for nccb and set the status.
	*/

	dsa = INL (nc_dsa);
	cp = np->link_nccb;
	while (cp && (CCB_PHYS (cp, phys) != dsa))
		cp = cp->link_nccb;

	if (cp) {
		cp-> host_status = HS_SEL_TIMEOUT;
		ncr_complete (np, cp);
	};

	/*
	**	repair start queue
	*/

	scratcha = INL (nc_scratcha);
	diff = scratcha - NCB_SCRIPTH_PHYS (np, tryloop);

/*	assert ((diff <= MAX_START * 20) && !(diff % 20));*/

	if ((diff <= MAX_START * 20) && !(diff % 20)) {
		WRITESCRIPT(startpos[0], scratcha);
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, start));
		return;
	};
	ncr_init (np, "selection timeout", HS_FAIL);
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

static void ncr_int_ma (ncb_p np, u_char dstat)
{
	u_int32_t	dbc;
	u_int32_t	rest;
	u_int32_t	dsa;
	u_int32_t	dsp;
	u_int32_t	nxtdsp;
	volatile void	*vdsp_base;
	size_t		vdsp_off;
	u_int32_t	oadr, olen;
	u_int32_t	*tblp, *newcmd;
	u_char	cmd, sbcl, ss0, ss2, ctest5;
	u_short	delta;
	nccb_p	cp;

	dsp = INL (nc_dsp);
	dsa = INL (nc_dsa);
	dbc = INL (nc_dbc);
	ss0 = INB (nc_sstat0);
	ss2 = INB (nc_sstat2);
	sbcl= INB (nc_sbcl);

	cmd = dbc >> 24;
	rest= dbc & 0xffffff;

	ctest5 = (np->rv_ctest5 & DFS) ? INB (nc_ctest5) : 0;
	if (ctest5 & DFS)
		delta=(((ctest5<<8) | (INB (nc_dfifo) & 0xff)) - rest) & 0x3ff;
	else
		delta=(INB (nc_dfifo) - rest) & 0x7f;


	/*
	**	The data in the dma fifo has not been transfered to
	**	the target -> add the amount to the rest
	**	and clear the data.
	**	Check the sstat2 register in case of wide transfer.
	*/

	if (!(dstat & DFE)) rest += delta;
	if (ss0 & OLF) rest++;
	if (ss0 & ORF) rest++;
	if (INB(nc_scntl3) & EWS) {
		if (ss2 & OLF1) rest++;
		if (ss2 & ORF1) rest++;
	};
	OUTB (nc_ctest3, np->rv_ctest3 | CLF);	/* clear dma fifo  */
	OUTB (nc_stest3, TE|CSF);		/* clear scsi fifo */

	/*
	**	locate matching cp
	*/
	cp = np->link_nccb;
	while (cp && (CCB_PHYS (cp, phys) != dsa))
		cp = cp->link_nccb;

	if (!cp) {
	    printf ("%s: SCSI phase error fixup: CCB already dequeued (%p)\n", 
		    ncr_name (np), (void *) np->header.cp);
	    return;
	}
	if (cp != np->header.cp) {
	    printf ("%s: SCSI phase error fixup: CCB address mismatch "
		    "(%p != %p) np->nccb = %p\n", 
		    ncr_name (np), (void *)cp, (void *)np->header.cp,
		    (void *)np->link_nccb);
/*	    return;*/
	}

	/*
	**	find the interrupted script command,
	**	and the address at which to continue.
	*/

	if (dsp == vtophys (&cp->patch[2])) {
		vdsp_base = cp;
		vdsp_off = offsetof(struct nccb, patch[0]);
		nxtdsp = READSCRIPT_OFF(vdsp_base, vdsp_off + 3*4);
	} else if (dsp == vtophys (&cp->patch[6])) {
		vdsp_base = cp;
		vdsp_off = offsetof(struct nccb, patch[4]);
		nxtdsp = READSCRIPT_OFF(vdsp_base, vdsp_off + 3*4);
	} else if (dsp > np->p_script &&
		   dsp <= np->p_script + sizeof(struct script)) {
		vdsp_base = np->script;
		vdsp_off = dsp - np->p_script - 8;
		nxtdsp = dsp;
	} else {
		vdsp_base = np->scripth;
		vdsp_off = dsp - np->p_scripth - 8;
		nxtdsp = dsp;
	};

	/*
	**	log the information
	*/
	if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_PHASE)) {
		printf ("P%x%x ",cmd&7, sbcl&7);
		printf ("RL=%d D=%d SS0=%x ",
			(unsigned) rest, (unsigned) delta, ss0);
	};
	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("\nCP=%p CP2=%p DSP=%x NXT=%x VDSP=%p CMD=%x ",
			cp, np->header.cp,
			dsp,
			nxtdsp, (volatile char*)vdsp_base+vdsp_off, cmd);
	};

	/*
	**	get old startaddress and old length.
	*/

	oadr = READSCRIPT_OFF(vdsp_base, vdsp_off + 1*4);

	if (cmd & 0x10) {	/* Table indirect */
		tblp = (u_int32_t *) ((char*) &cp->phys + oadr);
		olen = tblp[0];
		oadr = tblp[1];
	} else {
		tblp = (u_int32_t *) 0;
		olen = READSCRIPT_OFF(vdsp_base, vdsp_off) & 0xffffff;
	};

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("OCMD=%x\nTBLP=%p OLEN=%lx OADR=%lx\n",
			(unsigned) (READSCRIPT_OFF(vdsp_base, vdsp_off) >> 24),
			(void *) tblp,
			(u_long) olen,
			(u_long) oadr);
	};

	/*
	**	if old phase not dataphase, leave here.
	*/

	if (cmd != (READSCRIPT_OFF(vdsp_base, vdsp_off) >> 24)) {
		PRINT_ADDR(cp->ccb);
		printf ("internal error: cmd=%02x != %02x=(vdsp[0] >> 24)\n",
			(unsigned)cmd,
			(unsigned)READSCRIPT_OFF(vdsp_base, vdsp_off) >> 24);
		
		return;
	}
	if (cmd & 0x06) {
		PRINT_ADDR(cp->ccb);
		printf ("phase change %x-%x %d@%08x resid=%d.\n",
			cmd&7, sbcl&7, (unsigned)olen,
			(unsigned)oadr, (unsigned)rest);

		OUTB (nc_dcntl, np->rv_dcntl | STD);
		return;
	};

	/*
	**	choose the correct patch area.
	**	if savep points to one, choose the other.
	*/

	newcmd = cp->patch;
	if (cp->phys.header.savep == vtophys (newcmd)) newcmd+=4;

	/*
	**	fillin the commands
	*/

	newcmd[0] = ((cmd & 0x0f) << 24) | rest;
	newcmd[1] = oadr + olen - rest;
	newcmd[2] = SCR_JUMP;
	newcmd[3] = nxtdsp;

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		PRINT_ADDR(cp->ccb);
		printf ("newcmd[%d] %x %x %x %x.\n",
			(int)(newcmd - cp->patch),
			(unsigned)newcmd[0],
			(unsigned)newcmd[1],
			(unsigned)newcmd[2],
			(unsigned)newcmd[3]);
	}
	/*
	**	fake the return address (to the patch).
	**	and restart script processor at dispatcher.
	*/
	np->profile.num_break++;
	OUTL (nc_temp, vtophys (newcmd));
	if ((cmd & 7) == 0)
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, dispatch));
	else
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, checkatn));
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
	printf ("%x",*msg);
	if (*msg==MSG_EXTENDED) {
		for (i=1;i<8;i++) {
			if (i-1>msg[1]) break;
			printf ("-%x",msg[i]);
		};
		return (i+1);
	} else if ((*msg & 0xf0) == 0x20) {
		printf ("-%x",msg[1]);
		return (2);
	};
	return (1);
}

void ncr_int_sir (ncb_p np)
{
	u_char scntl3;
	u_char chg, ofs, per, fak, wide;
	u_char num = INB (nc_dsps);
	nccb_p	cp=0;
	u_long	dsa;
	u_int	target = INB (nc_sdid) & 0x0f;
	tcb_p	tp     = &np->target[target];
	int     i;
	if (DEBUG_FLAGS & DEBUG_TINY) printf ("I#%d", num);

	switch (num) {
	case SIR_SENSE_RESTART:
	case SIR_STALL_RESTART:
		break;

	default:
		/*
		**	lookup the nccb
		*/
		dsa = INL (nc_dsa);
		cp = np->link_nccb;
		while (cp && (CCB_PHYS (cp, phys) != dsa))
			cp = cp->link_nccb;

		assert (cp);
		if (!cp)
			goto out;
		assert (cp == np->header.cp);
		if (cp != np->header.cp)
			goto out;
	}

	switch (num) {

/*--------------------------------------------------------------------
**
**	Processing of interrupted getcc selects
**
**--------------------------------------------------------------------
*/

	case SIR_SENSE_RESTART:
		/*------------------------------------------
		**	Script processor is idle.
		**	Look for interrupted "check cond"
		**------------------------------------------
		*/

		if (DEBUG_FLAGS & DEBUG_RESTART)
			printf ("%s: int#%d",ncr_name (np),num);
		cp = (nccb_p) 0;
		for (i=0; i<MAX_TARGET; i++) {
			if (DEBUG_FLAGS & DEBUG_RESTART) printf (" t%d", i);
			tp = &np->target[i];
			if (DEBUG_FLAGS & DEBUG_RESTART) printf ("+");
			cp = tp->hold_cp;
			if (!cp) continue;
			if (DEBUG_FLAGS & DEBUG_RESTART) printf ("+");
			if ((cp->host_status==HS_BUSY) &&
				(cp->s_status==SCSI_STATUS_CHECK_COND))
				break;
			if (DEBUG_FLAGS & DEBUG_RESTART) printf ("- (remove)");
			tp->hold_cp = cp = (nccb_p) 0;
		};

		if (cp) {
			if (DEBUG_FLAGS & DEBUG_RESTART)
				printf ("+ restart job ..\n");
			OUTL (nc_dsa, CCB_PHYS (cp, phys));
			OUTL (nc_dsp, NCB_SCRIPTH_PHYS (np, getcc));
			return;
		};

		/*
		**	no job, resume normal processing
		*/
		if (DEBUG_FLAGS & DEBUG_RESTART) printf (" -- remove trap\n");
		WRITESCRIPT(start0[0], SCR_INT ^ IFFALSE (0));
		break;

	case SIR_SENSE_FAILED:
		/*-------------------------------------------
		**	While trying to select for
		**	getting the condition code,
		**	a target reselected us.
		**-------------------------------------------
		*/
		if (DEBUG_FLAGS & DEBUG_RESTART) {
			PRINT_ADDR(cp->ccb);
			printf ("in getcc reselect by t%d.\n",
				INB(nc_ssid) & 0x0f);
		}

		/*
		**	Mark this job
		*/
		cp->host_status = HS_BUSY;
		cp->s_status = SCSI_STATUS_CHECK_COND;
		np->target[cp->ccb->ccb_h.target_id].hold_cp = cp;

		/*
		**	And patch code to restart it.
		*/
		WRITESCRIPT(start0[0], SCR_INT);
		break;

/*-----------------------------------------------------------------------------
**
**	Was Sie schon immer ueber transfermode negotiation wissen wollten ...
**
**	We try to negotiate sync and wide transfer only after
**	a successfull inquire command. We look at byte 7 of the
**	inquire data to determine the capabilities if the target.
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
**	When we set the values, we adjust them in all nccbs belonging 
**	to this target, in the controller's register, and in the "phys"
**	field of the controller's struct ncb.
**
**	Possible cases:		   hs  sir   msg_in value  send   goto
**	We try try to negotiate:
**	-> target doesnt't msgin   NEG FAIL  noop   defa.  -      dispatch
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

		/* FALLTHROUGH */

	case SIR_NEGO_PROTO:
		/*-------------------------------------------------------
		**
		**	Negotiation failed.
		**	Target doesn't fetch the answer message.
		**
		**-------------------------------------------------------
		*/

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->ccb);
			printf ("negotiation failed sir=%x status=%x.\n",
				num, cp->nego_status);
		};

		/*
		**	any error in negotiation:
		**	fall back to default mode.
		*/
		switch (cp->nego_status) {

		case NS_SYNC:
			ncr_setsync (np, cp, 0, 0xe0, 0);
			break;

		case NS_WIDE:
			ncr_setwide (np, cp, 0, 0);
			break;

		};
		np->msgin [0] = MSG_NOOP;
		np->msgout[0] = MSG_NOOP;
		cp->nego_status = 0;
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, dispatch));
		break;

	case SIR_NEGO_SYNC:
		/*
		**	Synchronous request message received.
		*/

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->ccb);
			printf ("sync msgin: ");
			(void) ncr_show_msg (np->msgin);
			printf (".\n");
		};

		/*
		**	get requested values.
		*/

		chg = 0;
		per = np->msgin[3];
		ofs = np->msgin[4];
		if (ofs==0) per=255;

		/*
		**	check values against driver limits.
		*/
		if (per < np->minsync)
			{chg = 1; per = np->minsync;}
		if (per < tp->tinfo.user.period)
			{chg = 1; per = tp->tinfo.user.period;}
		if (ofs > tp->tinfo.user.offset)
			{chg = 1; ofs = tp->tinfo.user.offset;}

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
		}

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->ccb);
			printf ("sync: per=%d scntl3=0x%x ofs=%d fak=%d chg=%d.\n",
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
					ncr_setsync (np, cp, 0, 0xe0, 0);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, msg_bad));
				} else {
					/*
					**	Answer is ok.
					*/
					ncr_setsync (np,cp,scntl3,(fak<<5)|ofs, per);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, clrack));
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

		ncr_setsync (np, cp, scntl3, (fak<<5)|ofs, per);

		np->msgout[0] = MSG_EXTENDED;
		np->msgout[1] = 3;
		np->msgout[2] = MSG_EXT_SDTR;
		np->msgout[3] = per;
		np->msgout[4] = ofs;

		cp->nego_status = NS_SYNC;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->ccb);
			printf ("sync msgout: ");
			(void) ncr_show_msg (np->msgout);
			printf (".\n");
		}

		if (!ofs) {
			OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, msg_bad));
			return;
		}
		np->msgin [0] = MSG_NOOP;

		break;

	case SIR_NEGO_WIDE:
		/*
		**	Wide request message received.
		*/
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->ccb);
			printf ("wide msgin: ");
			(void) ncr_show_msg (np->msgin);
			printf (".\n");
		};

		/*
		**	get requested values.
		*/

		chg  = 0;
		wide = np->msgin[3];

		/*
		**	check values against driver limits.
		*/

		if (wide > tp->tinfo.user.width)
			{chg = 1; wide = tp->tinfo.user.width;}

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->ccb);
			printf ("wide: wide=%d chg=%d.\n", wide, chg);
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
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, msg_bad));
				} else {
					/*
					**	Answer is ok.
					*/
					ncr_setwide (np, cp, wide, 1);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, clrack));
				};
				return;

			case NS_SYNC:
				ncr_setsync (np, cp, 0, 0xe0, 0);
				break;
			};
		};

		/*
		**	It was a request, set value and
		**      prepare an answer message
		*/

		ncr_setwide (np, cp, wide, 1);

		np->msgout[0] = MSG_EXTENDED;
		np->msgout[1] = 2;
		np->msgout[2] = MSG_EXT_WDTR;
		np->msgout[3] = wide;

		np->msgin [0] = MSG_NOOP;

		cp->nego_status = NS_WIDE;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->ccb);
			printf ("wide msgout: ");
			(void) ncr_show_msg (np->msgout);
			printf (".\n");
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
		**	We received a MSG_MESSAGE_REJECT message.
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->ccb);
		printf ("MSG_MESSAGE_REJECT received (%x:%x).\n",
			(unsigned)np->lastmsg, np->msgout[0]);
		break;

	case SIR_REJECT_SENT:
		/*-----------------------------------------------
		**
		**	We received an unknown message
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->ccb);
		printf ("MSG_MESSAGE_REJECT sent for ");
		(void) ncr_show_msg (np->msgin);
		printf (".\n");
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

		PRINT_ADDR(cp->ccb);
		printf ("MSG_IGN_WIDE_RESIDUE received, but not yet implemented.\n");
		break;

	case SIR_MISSING_SAVE:
		/*-----------------------------------------------
		**
		**	We received an DISCONNECT message,
		**	but the datapointer wasn't saved before.
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->ccb);
		printf ("MSG_DISCONNECT received, but datapointer not saved:\n"
			"\tdata=%x save=%x goal=%x.\n",
			(unsigned) INL (nc_temp),
			(unsigned) np->header.savep,
			(unsigned) np->header.goalp);
		break;

/*--------------------------------------------------------------------
**
**	Processing of a "SCSI_STATUS_QUEUE_FULL" status.
**
**	XXX JGibbs - We should do the same thing for BUSY status.
**
**	The current command has been rejected,
**	because there are too many in the command queue.
**	We have started too many commands for that target.
**
**--------------------------------------------------------------------
*/
	case SIR_STALL_QUEUE:
		cp->xerr_status = XE_OK;
		cp->host_status = HS_COMPLETE;
		cp->s_status = SCSI_STATUS_QUEUE_FULL;
		ncr_freeze_devq(np, cp->ccb->ccb_h.path);
		ncr_complete(np, cp);

		/* FALLTHROUGH */

	case SIR_STALL_RESTART:
		/*-----------------------------------------------
		**
		**	Enable selecting again,
		**	if NO disconnected jobs.
		**
		**-----------------------------------------------
		*/
		/*
		**	Look for a disconnected job.
		*/
		cp = np->link_nccb;
		while (cp && cp->host_status != HS_DISCONNECT)
			cp = cp->link_nccb;

		/*
		**	if there is one, ...
		*/
		if (cp) {
			/*
			**	wait for reselection
			*/
			OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, reselect));
			return;
		};

		/*
		**	else remove the interrupt.
		*/

		printf ("%s: queue empty.\n", ncr_name (np));
		WRITESCRIPT(start1[0], SCR_INT ^ IFFALSE (0));
		break;
	};

out:
	OUTB (nc_dcntl, np->rv_dcntl | STD);
}

/*==========================================================
**
**
**	Aquire a control block
**
**
**==========================================================
*/

static	nccb_p ncr_get_nccb
	(ncb_p np, u_long target, u_long lun)
{
	lcb_p lp;
	int s;
	nccb_p cp = NULL;

	/* Keep our timeout handler out */
	s = splsoftclock();
	
	/*
	**	Lun structure available ?
	*/

	lp = np->target[target].lp[lun];
	if (lp) {
		cp = lp->next_nccb;

		/*
		**	Look for free CCB
		*/

		while (cp && cp->magic) {
			cp = cp->next_nccb;
		}
	}

	/*
	**	if nothing available, create one.
	*/

	if (cp == NULL)
		cp = ncr_alloc_nccb(np, target, lun);

	if (cp != NULL) {
		if (cp->magic) {
			printf("%s: Bogus free cp found\n", ncr_name(np));
			splx(s);
			return (NULL);
		}
		cp->magic = 1;
	}
	splx(s);
	return (cp);
}

/*==========================================================
**
**
**	Release one control block
**
**
**==========================================================
*/

void ncr_free_nccb (ncb_p np, nccb_p cp)
{
	/*
	**    sanity
	*/

	assert (cp != NULL);

	cp -> host_status = HS_IDLE;
	cp -> magic = 0;
}

/*==========================================================
**
**
**      Allocation of resources for Targets/Luns/Tags.
**
**
**==========================================================
*/

static nccb_p
ncr_alloc_nccb (ncb_p np, u_long target, u_long lun)
{
	tcb_p tp;
	lcb_p lp;
	nccb_p cp;

	assert (np != NULL);

	if (target>=MAX_TARGET) return(NULL);
	if (lun   >=MAX_LUN   ) return(NULL);

	tp=&np->target[target];

	if (!tp->jump_tcb.l_cmd) {

		/*
		**	initialize it.
		*/
		tp->jump_tcb.l_cmd   = (SCR_JUMP^IFFALSE (DATA (0x80 + target)));
		tp->jump_tcb.l_paddr = np->jump_tcb.l_paddr;

		tp->getscr[0] =
			(np->features & FE_PFEN)? SCR_COPY(1) : SCR_COPY_F(1);
		tp->getscr[1] = vtophys (&tp->tinfo.sval);
		tp->getscr[2] = rman_get_start(np->reg_res) + offsetof (struct ncr_reg, nc_sxfer);
		tp->getscr[3] =
			(np->features & FE_PFEN)? SCR_COPY(1) : SCR_COPY_F(1);
		tp->getscr[4] = vtophys (&tp->tinfo.wval);
		tp->getscr[5] = rman_get_start(np->reg_res) + offsetof (struct ncr_reg, nc_scntl3);

		assert (((offsetof(struct ncr_reg, nc_sxfer) ^
			 (offsetof(struct tcb ,tinfo)
			+ offsetof(struct ncr_target_tinfo, sval))) & 3) == 0);
		assert (((offsetof(struct ncr_reg, nc_scntl3) ^
			 (offsetof(struct tcb, tinfo)
			+ offsetof(struct ncr_target_tinfo, wval))) &3) == 0);

		tp->call_lun.l_cmd   = (SCR_CALL);
		tp->call_lun.l_paddr = NCB_SCRIPT_PHYS (np, resel_lun);

		tp->jump_lcb.l_cmd   = (SCR_JUMP);
		tp->jump_lcb.l_paddr = NCB_SCRIPTH_PHYS (np, abort);
		np->jump_tcb.l_paddr = vtophys (&tp->jump_tcb);
	}

	/*
	**	Logic unit control block
	*/
	lp = tp->lp[lun];
	if (!lp) {
		/*
		**	Allocate a lcb
		*/
		lp = (lcb_p) malloc (sizeof (struct lcb), M_DEVBUF,
			M_NOWAIT | M_ZERO);
		if (!lp) return(NULL);

		/*
		**	Initialize it
		*/
		lp->jump_lcb.l_cmd   = (SCR_JUMP ^ IFFALSE (DATA (lun)));
		lp->jump_lcb.l_paddr = tp->jump_lcb.l_paddr;

		lp->call_tag.l_cmd   = (SCR_CALL);
		lp->call_tag.l_paddr = NCB_SCRIPT_PHYS (np, resel_tag);

		lp->jump_nccb.l_cmd   = (SCR_JUMP);
		lp->jump_nccb.l_paddr = NCB_SCRIPTH_PHYS (np, aborttag);

		lp->actlink = 1;

		/*
		**   Chain into LUN list
		*/
		tp->jump_lcb.l_paddr = vtophys (&lp->jump_lcb);
		tp->lp[lun] = lp;

	}

	/*
	**	Allocate a nccb
	*/
	cp = (nccb_p) malloc (sizeof (struct nccb), M_DEVBUF, M_NOWAIT|M_ZERO);

	if (!cp)
		return (NULL);

	if (DEBUG_FLAGS & DEBUG_ALLOC) { 
		printf ("new nccb @%p.\n", cp);
	}

	/*
	**	Fill in physical addresses
	*/

	cp->p_nccb	     = vtophys (cp);

	/*
	**	Chain into reselect list
	*/
	cp->jump_nccb.l_cmd   = SCR_JUMP;
	cp->jump_nccb.l_paddr = lp->jump_nccb.l_paddr;
	lp->jump_nccb.l_paddr = CCB_PHYS (cp, jump_nccb);
	cp->call_tmp.l_cmd   = SCR_CALL;
	cp->call_tmp.l_paddr = NCB_SCRIPT_PHYS (np, resel_tmp);

	/*
	**	Chain into wakeup list
	*/
	cp->link_nccb      = np->link_nccb;
	np->link_nccb	   = cp;

	/*
	**	Chain into CCB list
	*/
	cp->next_nccb	= lp->next_nccb;
	lp->next_nccb	= cp;

	return (cp);
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

static	int	ncr_scatter
	(struct dsb* phys, vm_offset_t vaddr, vm_size_t datalen)
{
	u_long	paddr, pnext;

	u_short	segment  = 0;
	u_long	segsize, segaddr;
	u_long	size, csize    = 0;
	u_long	chunk = MAX_SIZE;
	int	free;

	bzero (&phys->data, sizeof (phys->data));
	if (!datalen) return (0);

	paddr = vtophys (vaddr);

	/*
	**	insert extra break points at a distance of chunk.
	**	We try to reduce the number of interrupts caused
	**	by unexpected phase changes due to disconnects.
	**	A typical harddisk may disconnect before ANY block.
	**	If we wanted to avoid unexpected phase changes at all
	**	we had to use a break point every 512 bytes.
	**	Of course the number of scatter/gather blocks is
	**	limited.
	*/

	free = MAX_SCATTER - 1;

	if (vaddr & PAGE_MASK) free -= datalen / PAGE_SIZE;

	if (free>1)
		while ((chunk * free >= 2 * datalen) && (chunk>=1024))
			chunk /= 2;

	if(DEBUG_FLAGS & DEBUG_SCATTER)
		printf("ncr?:\tscattering virtual=%p size=%d chunk=%d.\n",
		       (void *) vaddr, (unsigned) datalen, (unsigned) chunk);

	/*
	**   Build data descriptors.
	*/
	while (datalen && (segment < MAX_SCATTER)) {

		/*
		**	this segment is empty
		*/
		segsize = 0;
		segaddr = paddr;
		pnext   = paddr;

		if (!csize) csize = chunk;

		while ((datalen) && (paddr == pnext) && (csize)) {

			/*
			**	continue this segment
			*/
			pnext = (paddr & (~PAGE_MASK)) + PAGE_SIZE;

			/*
			**	Compute max size
			*/

			size = pnext - paddr;		/* page size */
			if (size > datalen) size = datalen;  /* data size */
			if (size > csize  ) size = csize  ;  /* chunksize */

			segsize += size;
			vaddr   += size;
			csize   -= size;
			datalen -= size;
			paddr    = vtophys (vaddr);
		};

		if(DEBUG_FLAGS & DEBUG_SCATTER)
			printf ("\tseg #%d  addr=%x  size=%d  (rest=%d).\n",
			segment,
			(unsigned) segaddr,
			(unsigned) segsize,
			(unsigned) datalen);

		phys->data[segment].addr = segaddr;
		phys->data[segment].size = segsize;
		segment++;
	}

	if (datalen) {
		printf("ncr?: scatter/gather failed (residue=%d).\n",
			(unsigned) datalen);
		return (-1);
	};

	return (segment);
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

#ifndef NCR_IOMAPPED
static int ncr_regtest (struct ncb* np)
{
	register volatile u_int32_t data;
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
		printf ("CACHE TEST FAILED: reg dstat-sstat2 readback %x.\n",
			(unsigned) data);
		return (0x10);
	};
	return (0);
}
#endif

static int ncr_snooptest (struct ncb* np)
{
	u_int32_t ncr_rd, ncr_wr, ncr_bk, host_rd, host_wr, pc;
	int	i, err=0;
#ifndef NCR_IOMAPPED
	err |= ncr_regtest (np);
	if (err) return (err);
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
	ncr_cache = host_wr;
	OUTL (nc_temp, ncr_wr);
	/*
	**	Start script (exchange values)
	*/
	OUTL (nc_dsp, pc);
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
	host_rd = ncr_cache;
	ncr_rd  = INL (nc_scratcha);
	ncr_bk  = INL (nc_temp);
	/*
	**	Reset ncr chip
	*/
	OUTB (nc_istat,  SRST);
	DELAY (1000);
	OUTB (nc_istat,  0   );
	/*
	**	check for timeout
	*/
	if (i>=NCR_SNOOP_TIMEOUT) {
		printf ("CACHE TEST FAILED: timeout.\n");
		return (0x20);
	};
	/*
	**	Check termination position.
	*/
	if (pc != NCB_SCRIPTH_PHYS (np, snoopend)+8) {
		printf ("CACHE TEST FAILED: script execution failed.\n");
		printf ("start=%08lx, pc=%08lx, end=%08lx\n", 
			(u_long) NCB_SCRIPTH_PHYS (np, snooptest), (u_long) pc,
			(u_long) NCB_SCRIPTH_PHYS (np, snoopend) +8);
		return (0x40);
	};
	/*
	**	Show results.
	*/
	if (host_wr != ncr_rd) {
		printf ("CACHE TEST FAILED: host wrote %d, ncr read %d.\n",
			(int) host_wr, (int) ncr_rd);
		err |= 1;
	};
	if (host_rd != ncr_wr) {
		printf ("CACHE TEST FAILED: ncr wrote %d, host read %d.\n",
			(int) ncr_wr, (int) host_rd);
		err |= 2;
	};
	if (ncr_bk != ncr_wr) {
		printf ("CACHE TEST FAILED: ncr wrote %d, read back %d.\n",
			(int) ncr_wr, (int) ncr_bk);
		err |= 4;
	};
	return (err);
}

/*==========================================================
**
**
**	Profiling the drivers and targets performance.
**
**
**==========================================================
*/

/*
**	Compute the difference in milliseconds.
**/

static	int ncr_delta (int *from, int *to)
{
	if (!from) return (-1);
	if (!to)   return (-2);
	return ((to - from) * 1000 / hz);
}

#define PROFILE  cp->phys.header.stamp
static	void ncb_profile (ncb_p np, nccb_p cp)
{
	int co, da, st, en, di, se, post,work,disc;
	u_long diff;

	PROFILE.end = ticks;

	st = ncr_delta (&PROFILE.start,&PROFILE.status);
	if (st<0) return;	/* status  not reached  */

	da = ncr_delta (&PROFILE.start,&PROFILE.data);
	if (da<0) return;	/* No data transfer phase */

	co = ncr_delta (&PROFILE.start,&PROFILE.command);
	if (co<0) return;	/* command not executed */

	en = ncr_delta (&PROFILE.start,&PROFILE.end),
	di = ncr_delta (&PROFILE.start,&PROFILE.disconnect),
	se = ncr_delta (&PROFILE.start,&PROFILE.select);
	post = en - st;

	/*
	**	@PROFILE@  Disconnect time invalid if multiple disconnects
	*/

	if (di>=0) disc = se-di; else  disc = 0;

	work = (st - co) - disc;

	diff = (np->disc_phys - np->disc_ref) & 0xff;
	np->disc_ref += diff;

	np->profile.num_trans	+= 1;
	if (cp->ccb)
		np->profile.num_bytes	+= cp->ccb->csio.dxfer_len;
	np->profile.num_disc	+= diff;
	np->profile.ms_setup	+= co;
	np->profile.ms_data	+= work;
	np->profile.ms_disc	+= disc;
	np->profile.ms_post	+= post;
}
#undef PROFILE

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
		printf ("%s: enabling clock multiplier\n", ncr_name(np));

	OUTB(nc_stest1, DBLEN);	   /* Enable clock multiplier		  */
	if (np->multiplier > 2) {  /* Poll bit 5 of stest4 for quadrupler */
		int i = 20;
		while (!(INB(nc_stest4) & LCKFRQ) && --i > 0)
			DELAY(20);
		if (!i)
			printf("%s: the chip cannot lock the frequency\n", ncr_name(np));
	} else			/* Wait 20 micro-seconds for doubler	*/
		DELAY(20);
	OUTB(nc_stest3, HSC);		/* Halt the scsi clock		*/
	OUTB(nc_scntl3,	scntl3);
	OUTB(nc_stest1, (DBLEN|DBLSEL));/* Select clock multiplier	*/
	OUTB(nc_stest3, 0x00);		/* Restart scsi clock 		*/
}

/*
 *	calculate NCR SCSI clock frequency (in KHz)
 */
static unsigned
ncrgetfreq (ncb_p np, int gen)
{
	int ms = 0;
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
	OUTB (nc_stest1, 0);	/* make sure clock doubler is OFF	    */
	OUTW (nc_sien , 0);	/* mask all scsi interrupts		    */
	(void) INW (nc_sist);	/* clear pending scsi interrupt		    */
	OUTB (nc_dien , 0);	/* mask all dma interrupts		    */
	(void) INW (nc_sist);	/* another one, just to be sure :)	    */
	OUTB (nc_scntl3, 4);	/* set pre-scaler to divide by 3	    */
	OUTB (nc_stime1, 0);	/* disable general purpose timer	    */
	OUTB (nc_stime1, gen);	/* set to nominal delay of (1<<gen) * 125us */
	while (!(INW(nc_sist) & GEN) && ms++ < 1000)
		DELAY(1000);	/* count ms				    */
	OUTB (nc_stime1, 0);	/* disable general purpose timer	    */
	OUTB (nc_scntl3, 0);
	/*
	 * Set prescaler to divide by whatever "0" means.
	 * "0" ought to choose divide by 2, but appears
	 * to set divide by 3.5 mode in my 53c810 ...
	 */
	OUTB (nc_scntl3, 0);

	if (bootverbose >= 2)
	  	printf ("\tDelay (GEN=%d): %u msec\n", gen, ms);
	/*
	 * adjust for prescaler, and convert into KHz 
	 */
	return ms ? ((1 << gen) * 4440) / ms : 0;
}

static void ncr_getclock (ncb_p np, u_char multiplier)
{
	unsigned char scntl3;
	unsigned char stest1;
	scntl3 = INB(nc_scntl3);
	stest1 = INB(nc_stest1);
	  
	np->multiplier = 1;

	if (multiplier > 1) {
		np->multiplier	= multiplier;
		np->clock_khz	= 40000 * multiplier;
	} else {
		if ((scntl3 & 7) == 0) {
			unsigned f1, f2;
			/* throw away first result */
			(void) ncrgetfreq (np, 11);
			f1 = ncrgetfreq (np, 11);
			f2 = ncrgetfreq (np, 11);

			if (bootverbose >= 2)
			  printf ("\tNCR clock is %uKHz, %uKHz\n", f1, f2);
			if (f1 > f2) f1 = f2;	/* trust lower result	*/
			if (f1 > 45000) {
				scntl3 = 5;	/* >45Mhz: assume 80MHz	*/
			} else {
				scntl3 = 3;	/* <45Mhz: assume 40MHz	*/
			}
		}
		else if ((scntl3 & 7) == 5)
			np->clock_khz = 80000;	/* Probably a 875 rev. 1 ? */
	}
}

/*=========================================================================*/

#ifdef NCR_TEKRAM_EEPROM

struct tekram_eeprom_dev {
  u_char	devmode;
#define	TKR_PARCHK	0x01
#define	TKR_TRYSYNC	0x02
#define	TKR_ENDISC	0x04
#define	TKR_STARTUNIT	0x08
#define	TKR_USETAGS	0x10
#define	TKR_TRYWIDE	0x20
  u_char	syncparam;	/* max. sync transfer rate (table ?) */
  u_char	filler1;
  u_char	filler2;
};


struct tekram_eeprom {
  struct tekram_eeprom_dev 
		dev[16];
  u_char	adaptid;
  u_char	adaptmode;
#define	TKR_ADPT_GT2DRV	0x01
#define	TKR_ADPT_GT1GB	0x02
#define	TKR_ADPT_RSTBUS	0x04
#define	TKR_ADPT_ACTNEG	0x08
#define	TKR_ADPT_NOSEEK	0x10
#define	TKR_ADPT_MORLUN	0x20
  u_char	delay;		/* unit ? ( table ??? ) */
  u_char	tags;		/* use 4 times as many ... */
  u_char	filler[60];
};

static void
tekram_write_bit (ncb_p np, int bit)
{
	u_char val = 0x10 + ((bit & 1) << 1);

	DELAY(10);
	OUTB (nc_gpreg, val);
	DELAY(10);
	OUTB (nc_gpreg, val | 0x04);
	DELAY(10);
	OUTB (nc_gpreg, val);
	DELAY(10);
}

static int
tekram_read_bit (ncb_p np)
{
	OUTB (nc_gpreg, 0x10);
	DELAY(10);
	OUTB (nc_gpreg, 0x14);
	DELAY(10);
	return INB (nc_gpreg) & 1;
}

static u_short
read_tekram_eeprom_reg (ncb_p np, int reg)
{
	int bit;
	u_short result = 0;
	int cmd = 0x80 | reg;

	OUTB (nc_gpreg, 0x10);

	tekram_write_bit (np, 1);
	for (bit = 7; bit >= 0; bit--)
	{
		tekram_write_bit (np, cmd >> bit);
	}

	for (bit = 0; bit < 16; bit++)
	{
		result <<= 1;
		result |= tekram_read_bit (np);
	}

	OUTB (nc_gpreg, 0x00);
	return result;
}

static int 
read_tekram_eeprom(ncb_p np, struct tekram_eeprom *buffer)
{
	u_short *p = (u_short *) buffer;
	u_short sum = 0;
	int i;

	if (INB (nc_gpcntl) != 0x09)
	{
		return 0;
        }
	for (i = 0; i < 64; i++)
	{
		u_short val;
if((i&0x0f) == 0) printf ("%02x:", i*2);
		val = read_tekram_eeprom_reg (np, i);
		if (p)
			*p++ = val;
		sum += val;
if((i&0x01) == 0x00) printf (" ");
		printf ("%02x%02x", val & 0xff, (val >> 8) & 0xff);
if((i&0x0f) == 0x0f) printf ("\n");
	}
printf ("Sum = %04x\n", sum);
	return sum == 0x1234;
}
#endif /* NCR_TEKRAM_EEPROM */

static device_method_t ncr_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ncr_probe),
	DEVMETHOD(device_attach,	ncr_attach),

	{ 0, 0 }
};

static driver_t ncr_driver = {
	"ncr",
	ncr_methods,
	sizeof(struct ncb),
};

static devclass_t ncr_devclass;

DRIVER_MODULE(if_ncr, pci, ncr_driver, ncr_devclass, 0, 0);

/*=========================================================================*/
#endif /* _KERNEL */
