/**************************************************************************
**
**  $Id: ncr.c,v 1.37.4.5 1996/01/15 06:14:12 jkh Exp $
**
**  Device driver for the   NCR 53C810   PCI-SCSI-Controller.
**
**  FreeBSD / NetBSD
**
**-------------------------------------------------------------------------
**
**  Written for 386bsd and FreeBSD by
**	Wolfgang Stanglmeier	<wolf@cologne.de>
**	Stefan Esser		<se@mi.Uni-Koeln.de>
**
**  Ported to NetBSD by
**	Charles M. Hannum	<mycroft@gnu.ai.mit.edu>
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

#define NCR_DATE "pl23 95/09/07"

#define NCR_VERSION	(2)
#define	MAX_UNITS	(16)

#define NCR_GETCC_WITHMSG

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
**    The maximal synchronous frequency in kHz.
**    (0=asynchronous)
*/

#ifndef SCSI_NCR_MAX_SYNC
#define SCSI_NCR_MAX_SYNC   (10000)
#endif /* SCSI_NCR_MAX_SYNC */

/*
**    The maximal bus with (in log2 byte)
**    (0=8 bit, 1=16 bit)
*/

#ifndef SCSI_NCR_MAX_WIDE
#define SCSI_NCR_MAX_WIDE   (1)
#endif /* SCSI_NCR_MAX_WIDE */

/*
**    The maximum number of tags per logic unit.
**    Used only for disk devices that support tags.
*/

#ifndef SCSI_NCR_MAX_TAGS
#define SCSI_NCR_MAX_TAGS    (4)
#endif /* SCSI_NCR_MAX_TAGS */

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
#define MAX_LUN     (1)
#endif	/* MAX_LUN */

/*
**    The maximum number of jobs scheduled for starting.
**    There should be one slot per target, and one slot
**    for each tag of each target in use.
**    The calculation below is actually quite silly ...
*/

#define MAX_START   (MAX_TARGET + 7 * SCSI_NCR_MAX_TAGS)

/*
**    The maximum number of segments a transfer is split into.
*/

#define MAX_SCATTER (33)

/*
**    The maximum transfer length (should be >= 64k).
**    MUST NOT be greater than (MAX_SCATTER-1) * NBPG.
*/

#define MAX_SIZE  ((MAX_SCATTER-1) * (long) NBPG)

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

#ifdef __NetBSD__
#ifdef _KERNEL
#define KERNEL
#endif
#endif
#include <stddef.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>

#ifdef KERNEL
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#ifndef __NetBSD__
#include <machine/clock.h>
#include <machine/cpu.h> /* bootverbose */
#else
#define bootverbose	1
#endif
#include <vm/vm.h>
#include <vm/vm_extern.h>
#endif /* KERNEL */


#ifndef __NetBSD__
#include <sys/devconf.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/ncrreg.h>
extern PRINT_ADDR();
#else
#include <sys/device.h>
#include <dev/pci/ncr_reg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#define DELAY(x)	delay(x)
#endif /* __NetBSD */

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#ifndef __NetBSD__
#include <machine/clock.h>
#endif /* __NetBSD */


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

#ifdef SCSI_DEBUG_FLAGS
	#define DEBUG_FLAGS ncr_debug
#else /* SCSI_DEBUG_FLAGS */
	#define SCSI_DEBUG_FLAGS	0
	#define DEBUG_FLAGS	0
#endif /* SCSI_DEBUG_FLAGS */



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

#define	assert(expression) { \
	if (!(expression)) { \
		(void)printf(\
			"assertion \"%s\" failed: file \"%s\", line %d\n", \
			#expression, \
			__FILE__, __LINE__); \
	} \
}

/*==========================================================
**
**	Access to the controller chip.
**
**==========================================================
*/

#ifdef NCR_IOMAPPED

#define	INB(r) inb (np->port + offsetof(struct ncr_reg, r))
#define	INW(r) inw (np->port + offsetof(struct ncr_reg, r))
#define	INL(r) inl (np->port + offsetof(struct ncr_reg, r))

#define	OUTB(r, val) outb (np->port+offsetof(struct ncr_reg,r),(val))
#define	OUTW(r, val) outw (np->port+offsetof(struct ncr_reg,r),(val))
#define	OUTL(r, val) outl (np->port+offsetof(struct ncr_reg,r),(val))

#else

#define INB(r) (np->reg->r)
#define INW(r) (np->reg->r)
#define INL(r) (np->reg->r)

#define OUTB(r, val) np->reg->r = val
#define OUTW(r, val) np->reg->r = val
#define OUTL(r, val) np->reg->r = val

#endif

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

#define NS_SYNC		(1)
#define NS_WIDE		(2)

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
#define	QUIRK_UPDATE	(0x80)

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
#define	MAX_TAGS	(16)		/* hard limit */

/*==========================================================
**
**	OS dependencies.
**
**==========================================================
*/

#ifdef __NetBSD__
	#define INT32     int
	#define U_INT32   u_int
	#define TIMEOUT   (void*)
#else  /*__NetBSD__*/
	#define INT32     int32
	#define U_INT32   u_int32
	#define TIMEOUT   (timeout_func_t)
#endif /*__NetBSD__*/
#define PRINT_ADDR(xp) sc_print_addr(xp->sc_link)

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
	u_long	l_cmd;
	u_long	l_paddr;
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

struct tstamp {
	struct timeval	start;
	struct timeval	end;
	struct timeval	select;
	struct timeval	command;
	struct timeval	data;
	struct timeval	status;
	struct timeval	disconnect;
	struct timeval	reselect;
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
	**	@(first ccb of this lun)
	*/

	struct link   jump_lcb;

	/*
	**	pointer to interrupted getcc ccb
	*/

	ccb_p   hold_cp;

	/*
	**	statistical data
	*/

	u_long	transfers;
	u_long	bytes;

	/*
	**	user settable limits for sync transfer
	**	and tagged commands.
	*/

	u_char	usrsync;
	u_char	usrtags;
	u_char	usrwide;
	u_char	usrflag;

	/*
	**	negotiation of wide and synch transfer.
	**	device quirks.
	*/

/*0*/	u_char	minsync;
/*1*/	u_char	sval;
/*2*/	u_short	period;
/*0*/	u_char	maxoffs;

/*1*/	u_char	quirks;

/*2*/	u_char	widedone;
/*3*/	u_char	wval;
	/*
	**	inquire data
	*/
#define MAX_INQUIRE 36
	u_char	inqdata[MAX_INQUIRE];

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
	**	now look for the right ccb.
	**
	**	JUMP
	**	@(first ccb of this lun)
	*/

	struct link	jump_ccb;

	/*
	**	start of the ccb chain
	*/

	ccb_p	next_ccb;

	/*
	**	Control of tagged queueing
	*/

	u_char		reqccbs;
	u_char		actccbs;
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
**	This substructure is copied from the ccb to a
**	global address after selection (or reselection)
**	and copied back before disconnect.
**
**	These fields are accessible to the script processor.
**
**----------------------------------------------------------
*/

struct head {
	/*
	**	Execution of a ccb starts at this point.
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

	u_long		savep;
	u_long		lastp;
	u_long		goalp;

	/*
	**	The virtual address of the ccb
	**	containing this header.
	*/

	ccb_p	cp;

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
#define  scsi_status   phys.header.status[2]
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
**	During execution of a ccb by the script processor,
**	the DSA (data structure address) register points
**	to this substructure of the ccb.
**	This substructure contains the header with
**	the script-processor-changable data and then
**	data blocks for the indirect move commands.
**
**----------------------------------------------------------
*/


struct ccb {
	/*
	**	during reselection the ncr jumps to this point.
	**	If a "SIMPLE_TAG" message was received,
	**	then SFBR is set to the tag.
	**	else SFBR is set to 0
	**	If looking for another tag, jump to the next ccb.
	**
	**	JUMP  IF (SFBR != #TAG#)
	**	@(next ccb of this lun)
	*/

	struct link		jump_ccb;

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

	u_long			patch[8];

	/*
	**	The general SCSI driver provides a
	**	pointer to a control block.
	*/

	struct scsi_xfer	*xfer;

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
	**	Lock this ccb.
	**	Flag is used while looking for a free ccb.
	*/

	u_long		magic;

	/*
	**	Physical address of this instance of ccb
	*/

	u_long		p_ccb;

	/*
	**	Completion time out for this job.
	**	It's set to time of start + allowed number of seconds.
	*/

	u_long		tlimit;

	/*
	**	All ccbs of one hostadapter are chained.
	*/

	ccb_p		link_ccb;

	/*
	**	All ccbs of one target/lun are chained.
	*/

	ccb_p		next_ccb;

	/*
	**	Sense command
	*/

	u_char		sensecmd[6];

	/*
	**	Tag for this transfer.
	**	It's patched into jump_ccb.
	**	If it's not zero, a SIMPLE_TAG
	**	message is included in smsg.
	*/

	u_char			tag;
};

#define CCB_PHYS(cp,lbl)	(cp->p_ccb + offsetof(struct ccb, lbl))

/*==========================================================
**
**      Declaration of structs:     NCR device descriptor
**
**==========================================================
*/

struct ncb {
#ifdef __NetBSD__
	struct device sc_dev;
	void *sc_ih;
#else /* !__NetBSD__ */
	int	unit;
#endif /* __NetBSD__ */

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
	vm_offset_t     vaddr;
	vm_offset_t     paddr;

	/*
	**	pointer to the chip's registers.
	*/
	volatile
	struct ncr_reg* reg;

	/*
	**	A copy of the script, relocated for this ncb.
	*/
	struct script	*script;

	/*
	**	Physical address of this instance of ncb->script
	*/
	u_long		p_script;

	/*
	**	The SCSI address of the host adapter.
	*/
	u_char	  myaddr;

	/*
	**	timing parameters
	*/
	u_char		ns_async;
	u_char		ns_sync;
	u_char		rv_scntl3;

	/*-----------------------------------------------
	**	Link to the generic SCSI driver
	**-----------------------------------------------
	*/

	struct scsi_link	sc_link;

	/*-----------------------------------------------
	**	Job control
	**-----------------------------------------------
	**
	**	Commands from user
	*/
	struct usrcmd	user;
	u_char		order;

	/*
	**	Target data
	*/
	struct tcb	target[MAX_TARGET];

	/*
	**	Start queue.
	*/
	u_long		squeue [MAX_START];
	u_short		squeueput;
	u_short		actccbs;

	/*
	**	Timeout handler
	*/
	u_long		heartbeat;
	u_short		ticks;
	u_short		latetime;
	u_long		lasttime;

	/*-----------------------------------------------
	**	Debug and profiling
	**-----------------------------------------------
	**
	**	register dump
	*/
	struct ncr_reg	regdump;
	struct timeval	regtime;

	/*
	**	Profiling data
	*/
	struct profile	profile;
	u_long		disc_phys;
	u_long		disc_ref;

	/*
	**	The global header.
	**	Accessible to both the host and the
	**	script-processor.
	*/
	struct head     header;

	/*
	**	The global control block.
	**	It's used only during the configuration phase.
	**	A target control block will be created
	**	after the first successful transfer.
	*/
	struct ccb      ccb;

	/*
	**	message buffers.
	**	Should be longword aligned,
	**	because they're written with a
	**	COPY script command.
	*/
	u_char		msgout[8];
	u_char		msgin [8];
	u_long		lastmsg;

	/*
	**	Buffer for STATUS_IN phase.
	*/
	u_char		scratch;

	/*
	**	controller chip dependent maximal transfer width.
	*/
	u_char		maxwide;

	/*
	**	option for M_IDENTIFY message: enables disconnecting
	*/
	u_char		disc;

#ifdef NCR_IOMAPPED
	/*
	**	address of the ncr control registers in io space
	*/
	u_short		port;
#endif
};

#define NCB_SCRIPT_PHYS(np,lbl)	(np->p_script + offsetof (struct script, lbl))

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

struct script {
	ncrcmd	start		[  7];
	ncrcmd	start0		[  2];
	ncrcmd	start1		[  3];
	ncrcmd  startpos	[  1];
	ncrcmd  tryloop		[MAX_START*5+2];
	ncrcmd  trysel		[  8];
	ncrcmd	skip		[  8];
	ncrcmd	skip2		[  3];
	ncrcmd  idle		[  2];
	ncrcmd	select		[ 24];
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
	ncrcmd  msg_parity	[  6];
	ncrcmd	msg_reject	[  8];
	ncrcmd	msg_ign_residue	[ 32];
	ncrcmd  msg_extended	[ 18];
	ncrcmd  msg_ext_2	[ 18];
	ncrcmd	msg_wdtr	[ 27];
	ncrcmd  msg_ext_3	[ 18];
	ncrcmd	msg_sdtr	[ 27];
	ncrcmd  complete	[ 13];
	ncrcmd	cleanup		[ 12];
	ncrcmd	cleanup0	[ 11];
	ncrcmd	signal		[ 10];
	ncrcmd  save_dp		[  5];
	ncrcmd  restore_dp	[  5];
	ncrcmd  disconnect	[ 12];
	ncrcmd  disconnect0	[  5];
	ncrcmd  disconnect1	[ 23];
	ncrcmd	msg_out		[  9];
	ncrcmd	msg_out_done	[  7];
	ncrcmd	msg_out_abort	[ 10];
	ncrcmd  getcc		[  4];
	ncrcmd  getcc1		[  5];
#ifdef NCR_GETCC_WITHMSG
	ncrcmd	getcc2		[ 33];
#else
	ncrcmd	getcc2		[ 14];
#endif
	ncrcmd	getcc3		[ 10];
	ncrcmd  badgetcc	[  6];
	ncrcmd	reselect	[ 12];
	ncrcmd	reselect2	[  6];
	ncrcmd	resel_tmp	[  5];
	ncrcmd  resel_lun	[ 18];
	ncrcmd	resel_tag	[ 24];
	ncrcmd  data_in		[MAX_SCATTER * 4 + 7];
	ncrcmd  data_out	[MAX_SCATTER * 4 + 7];
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

#ifdef KERNEL
static	void	ncr_alloc_ccb	(ncb_p np, struct scsi_xfer * xp);
static	void	ncr_complete	(ncb_p np, ccb_p cp);
static	int	ncr_delta	(struct timeval * from, struct timeval * to);
static	void	ncr_exception	(ncb_p np);
static	void	ncr_free_ccb	(ncb_p np, ccb_p cp, int flags);
static	void	ncr_getclock	(ncb_p np);
static	ccb_p	ncr_get_ccb	(ncb_p np, u_long flags, u_long t,u_long l);
static  U_INT32 ncr_info	(int unit);
static	void	ncr_init	(ncb_p np, char * msg, u_long code);
#ifdef __NetBSD__
static	int     ncr_intr        (void *);
#else	/* !__NetBSD__ */
static	int	ncr_intr	(ncb_p np);
#endif	/* __NetBSD__ */	
static	void	ncr_int_ma	(ncb_p np);
static	void	ncr_int_sir	(ncb_p np);
static  void    ncr_int_sto     (ncb_p np);
#ifndef NEW_SCSICONF
static	u_long	ncr_lookup	(char* id);
#endif /* NEW_SCSICONF */
static	void	ncr_min_phys	(struct buf *bp);
static	void	ncr_negotiate	(struct ncb* np, struct tcb* tp);
static	void	ncr_opennings	(ncb_p np, lcb_p lp, struct scsi_xfer * xp);
static	void	ncb_profile	(ncb_p np, ccb_p cp);
static	void	ncr_script_copy_and_bind
				(struct script * script, ncb_p np);
static  void    ncr_script_fill (struct script * scr);
static	int	ncr_scatter	(struct dsb* phys,u_long vaddr,u_long datalen);
static	void	ncr_setmaxtags	(tcb_p tp, u_long usrtags);
static	void	ncr_setsync	(ncb_p np, ccb_p cp, u_char sxfer);
static	void	ncr_settags     (tcb_p tp, lcb_p lp);
static	void	ncr_setwide	(ncb_p np, ccb_p cp, u_char wide);
static	int	ncr_show_msg	(u_char * msg);
static	int	ncr_snooptest	(ncb_p np);
static	INT32	ncr_start       (struct scsi_xfer *xp);
static	void	ncr_timeout	(ncb_p np);
static	void	ncr_usercmd	(ncb_p np);
static  void    ncr_wakeup      (ncb_p np, u_long code);

#ifdef __NetBSD__
static	int	ncr_probe	(struct device *, void *, void *);
static	void	ncr_attach	(struct device *, struct device *, void *);
#else /* !__NetBSD */
static  char*	ncr_probe       (pcici_t tag, pcidi_t type);
static	void	ncr_attach	(pcici_t tag, int unit);
#endif /* __NetBSD__ */

#endif /* KERNEL */

/*==========================================================
**
**
**      Global static data.
**
**
**==========================================================
*/


static char ident[] =
	"\n$Id: ncr.c,v 1.37.4.5 1996/01/15 06:14:12 jkh Exp $\n";

u_long	ncr_version = NCR_VERSION	* 11
	+ (u_long) sizeof (struct ncb)	*  7
	+ (u_long) sizeof (struct ccb)	*  5
	+ (u_long) sizeof (struct lcb)	*  3
	+ (u_long) sizeof (struct tcb)	*  2;

#ifdef KERNEL

#ifndef __NetBSD__
u_long		nncr=MAX_UNITS;
ncb_p		ncrp [MAX_UNITS];
#endif /* !__NetBSD__ */

static int ncr_debug = SCSI_DEBUG_FLAGS;

int ncr_cache; /* to be aligned _NOT_ static */

/*==========================================================
**
**
**      Global static data:	auto configure
**
**
**==========================================================
*/

#define	NCR_810_ID	(0x00011000ul)
#define	NCR_810AP_ID	(0x00051000ul)
#define	NCR_815_ID	(0x00041000ul)
#define	NCR_825_ID	(0x00031000ul)
#define	NCR_860_ID	(0x00061000ul)
#define	NCR_875_ID	(0x000f1000ul)

#ifdef __NetBSD__

struct	cfdriver ncrcd = {
	NULL, "ncr", ncr_probe, ncr_attach, DV_DISK, sizeof(struct ncb)
};

#else /* !__NetBSD__ */

static u_long ncr_count;

struct	pci_device ncr_device = {
	"ncr",
	ncr_probe,
	ncr_attach,
	&ncr_count,
	NULL
};

DATA_SET (pcidevice_set, ncr_device);

#endif /* !__NetBSD__ */

struct scsi_adapter ncr_switch =
{
	ncr_start,
	ncr_min_phys,
	0,
	0,
#ifndef __NetBSD__
	ncr_info,
	"ncr",
#endif /* !__NetBSD__ */
};

struct scsi_device ncr_dev =
{
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
#ifndef __NetBSD__
	"ncr",
#endif /* !__NetBSD__ */
};

#ifdef __NetBSD__

#define	ncr_name(np)	(np->sc_dev.dv_xname)

#else /* !__NetBSD__ */

static char *ncr_name (ncb_p np)
{
	static char name[10];
	sprintf(name, "ncr%d", np->unit);
	return (name);
}
#endif

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
#define	RELOC_MASK	0xf0000000

#define	NADDR(label)	(RELOC_SOFTC | offsetof(struct ncb, label))
#define PADDR(label)    (RELOC_LABEL | offsetof(struct script, label))
#define	RADDR(label)	(RELOC_REGISTER | REG(label))
#define	FADDR(label,ofs)(RELOC_REGISTER | ((REG(label))+(ofs)))

static	struct script script0 = {
/*--------------------------< START >-----------------------*/ {
	/*
	**	Claim to be still alive ...
	*/
	SCR_COPY (sizeof (((struct ncb *)0)->heartbeat)),
		(ncrcmd) &time.tv_sec,
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
		PADDR(tryloop),
}/*-------------------------< TRYLOOP >---------------------*/,{
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
**		PADDR(tryloop),
**
**-----------------------------------------------------------
*/
0

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
	**	the address of the phys-part of a ccb.
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
	SCR_COPY (4),
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
	**	Save target id to ctest0 register
	*/

	SCR_FROM_REG (sdid),
		0,
	SCR_TO_REG (ctest0),
		0,
	/*
	**	Send the IDENTIFY and SIMPLE_TAG messages
	**	(and the M_X_SYNC_REQ message)
	*/
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg),
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		-16,
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
	SCR_COPY (4),
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
	**      Mark this ccb as not scheduled.
	*/
	SCR_COPY (8),
		PADDR (idle),
		NADDR (header.launch),
	/*
	**      Set a time stamp for this selection
	*/
	SCR_COPY (sizeof (struct timeval)),
		(ncrcmd) &time,
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
	SCR_LOAD_REG (scratcha, M_NOOP),
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
	SCR_JUMP ^ IFTRUE (DATA (M_EXTENDED)),
		PADDR (msg_in),
	SCR_JUMP ^ IFTRUE (DATA (M_REJECT)),
		PADDR (msg_reject),
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
	**	Prepare a M_ID_ERROR message
	**	(initiator detected error).
	**	The target should retry the transfer.
	*/
	SCR_LOAD_REG (scratcha, M_ID_ERROR),
		0,
	SCR_JUMP,
		PADDR (setmsg),

}/*-------------------------< COMMAND >--------------------*/,{
	/*
	**	If this is not a GETCC transfer ...
	*/
	SCR_FROM_REG (SS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFTRUE (DATA (S_CHECK_COND)),
		28,
	/*
	**	... set a timestamp ...
	*/
	SCR_COPY (sizeof (struct timeval)),
		(ncrcmd) &time,
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
	SCR_COPY (sizeof (struct timeval)),
		(ncrcmd) &time,
		NADDR (header.stamp.status),
	/*
	**	If this is a GETCC transfer,
	*/
	SCR_FROM_REG (SS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (DATA (S_CHECK_COND)),
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
	SCR_REG_REG (SS_REG, SCR_OR, S_SENSE),
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
	SCR_JUMP ^ IFTRUE (DATA (S_CHECK_COND)),
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
		PADDR (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	/*
	**	Parity was ok, handle this message.
	*/
	SCR_JUMP ^ IFTRUE (DATA (M_COMPLETE)),
		PADDR (complete),
	SCR_JUMP ^ IFTRUE (DATA (M_SAVE_DP)),
		PADDR (save_dp),
	SCR_JUMP ^ IFTRUE (DATA (M_RESTORE_DP)),
		PADDR (restore_dp),
	SCR_JUMP ^ IFTRUE (DATA (M_DISCONNECT)),
		PADDR (disconnect),
	SCR_JUMP ^ IFTRUE (DATA (M_EXTENDED)),
		PADDR (msg_extended),
	SCR_JUMP ^ IFTRUE (DATA (M_NOOP)),
		PADDR (clrack),
	SCR_JUMP ^ IFTRUE (DATA (M_REJECT)),
		PADDR (msg_reject),
	SCR_JUMP ^ IFTRUE (DATA (M_IGN_RESIDUE)),
		PADDR (msg_ign_residue),
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
	SCR_JUMP,
		PADDR (setmsg),

}/*-------------------------< MSG_PARITY >---------------*/,{
	/*
	**	count it
	*/
	SCR_REG_REG (PS_REG, SCR_ADD, 0x01),
		0,
	/*
	**	send a "message parity error" message.
	*/
	SCR_LOAD_REG (scratcha, M_PARITY),
		0,
	SCR_JUMP,
		PADDR (setmsg),
}/*-------------------------< MSG_REJECT >---------------*/,{
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
		PADDR (msg_parity),
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
		PADDR (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	/*
	*/
	SCR_JUMP ^ IFTRUE (DATA (3)),
		PADDR (msg_ext_3),
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
		PADDR (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	SCR_JUMP ^ IFTRUE (DATA (M_X_WIDE_REQ)),
		PADDR (msg_wdtr),
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
		PADDR (msg_parity),
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
	**	Send the M_X_WIDE_REQ
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
		PADDR (msg_parity),
	SCR_FROM_REG (scratcha),
		0,
	SCR_JUMP ^ IFTRUE (DATA (M_X_SYNC_REQ)),
		PADDR (msg_sdtr),
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
		PADDR (msg_parity),
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
	**	Send the M_X_SYNC_REQ
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

}/*-------------------------< COMPLETE >-----------------*/,{
	/*
	**	Complete message.
	**
	**	If it's not the get condition code,
	**	copy TEMP register to LASTP in header.
	*/
	SCR_FROM_REG (SS_REG),
		0,
/*<<<*/	SCR_JUMPR ^ IFTRUE (MASK (S_SENSE, S_SENSE)),
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
	**      dsa:    Pointer to ccb
	**	      or xxxxxxFF (no ccb)
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
	**	and copy back the header to the ccb.
	*/
	SCR_COPY (4),
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
	SCR_JUMP ^ IFTRUE (DATA (S_CHECK_COND)),
		PADDR(getcc2),
	/*
	**	And make the DSA register invalid.
	*/
/*>>>*/	SCR_LOAD_REG (dsa, 0xff), /* invalid */
		0,
}/*-------------------------< SIGNAL >----------------------*/,{
	/*
	**	if status = queue full,
	**	reinsert in startqueue and stall queue.
	*/
	SCR_FROM_REG (SS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (S_QUEUE_FULL)),
		SIR_STALL_QUEUE,
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
	SCR_COPY (1),
		NADDR (header.savep),
		PADDR (disconnect0),
}/*-------------------------< DISCONNECT0 >--------------*/,{
/*<<<*/	SCR_JUMPR ^ IFTRUE (DATA (1)),
		20,
	/*
	**	neither this
	*/
	SCR_COPY (1),
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
	SCR_COPY (sizeof (struct timeval)),
		(ncrcmd) &time,
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
	SCR_JUMP ^ IFTRUE (DATA (M_ABORT)),
		PADDR (msg_out_abort),
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
	SCR_COPY (4),
		RADDR (dsa),
		PADDR (getcc1),
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
		PADDR(getcc3),
	/*
	**	Then try to connect to the target.
	**	If we are reselected, special treatment
	**	of the current job is required before
	**	accepting the reselection.
	*/
	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR(badgetcc),
	/*
	**	save target id.
	*/
	SCR_FROM_REG (sdid),
		0,
	SCR_TO_REG (ctest0),
		0,
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
	**	save target id.
	*/
	SCR_FROM_REG (sdid),
		0,
	SCR_TO_REG (ctest0),
		0,
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

}/*------------------------< BADGETCC >---------------------*/,{
	/*
	**	If SIGP was set, clear it and try again.
	*/
	SCR_FROM_REG (ctest2),
		0,
	SCR_JUMP ^ IFTRUE (MASK (CSIGP,CSIGP)),
		PADDR (getcc2),
	SCR_INT,
		SIR_SENSE_FAILED,
}/*-------------------------< RESELECT >--------------------*/,{
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
	SCR_REG_SFBR (ssid, SCR_AND, 0x87),
		0,
	SCR_TO_REG (ctest0),
		0,
	SCR_JUMP,
		NADDR (jump_tcb),
}/*-------------------------< RESELECT2 >-------------------*/,{
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
/*<<<*/	SCR_JUMPR ^ IFFALSE (MASK (M_IDENTIFY, 0x98)),
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
/*<<<*/	SCR_JUMPR ^ IFFALSE (DATA (M_SIMPLE_TAG)),
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
**	SCR_COPY (sizeof (struct timeval)),
**		(ncrcmd) &time,
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
**	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN)),
**		PADDR (no_data),
**	SCR_COPY (sizeof (struct timeval)),
**		(ncrcmd) &time,
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
(u_long)&ident

}/*-------------------------< ABORTTAG >-------------------*/,{
	/*
	**      Abort a bad reselection.
	**	Set the message to ABORT vs. ABORT_TAG
	*/
	SCR_LOAD_REG (scratcha, M_ABORT_TAG),
		0,
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
}/*-------------------------< ABORT >----------------------*/,{
	SCR_LOAD_REG (scratcha, M_ABORT),
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
		(ncrcmd) &ncr_cache,
		RADDR (scratcha),
	/*
	**	Write the variable.
	*/
	SCR_COPY (4),
		RADDR (temp),
		(ncrcmd) &ncr_cache,
	/*
	**	Read back the variable.
	*/
	SCR_COPY (4),
		(ncrcmd) &ncr_cache,
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

void ncr_script_fill (struct script * scr)
{
	int	i;
	ncrcmd	*p;

	p = scr->tryloop;
	for (i=0; i<MAX_START; i++) {
		*p++ =SCR_COPY (4);
		*p++ =NADDR (squeue[i]);
		*p++ =RADDR (dsa);
		*p++ =SCR_CALL;
		*p++ =PADDR (trysel);
	};
	*p++ =SCR_JUMP;
	*p++ =PADDR(tryloop);

	assert ((u_long)p == (u_long)&scr->tryloop + sizeof (scr->tryloop));

	p = scr->data_in;

	*p++ =SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN));
	*p++ =PADDR (no_data);
	*p++ =SCR_COPY (sizeof (struct timeval));
	*p++ =(ncrcmd) &time;
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

	assert ((u_long)p == (u_long)&scr->data_in + sizeof (scr->data_in));

	p = scr->data_out;

	*p++ =SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_OUT));
	*p++ =PADDR (no_data);
	*p++ =SCR_COPY (sizeof (struct timeval));
	*p++ =(ncrcmd) &time;
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

static void ncr_script_copy_and_bind (struct script *script, ncb_p np)
{
	ncrcmd  opcode, new, old;
	ncrcmd	*src, *dst, *start, *end;
	int relocs;

#ifndef __NetBSD__
	np->script = (struct script*) vm_page_alloc_contig 
	(round_page(sizeof (struct script)), 0x100000, 0xffffffff, PAGE_SIZE);
#else  /* !__NetBSD___ */
	np->script = (struct script *)
		malloc (sizeof (struct script), M_DEVBUF, M_WAITOK);
#endif /* __NetBSD__ */

	np->p_script = vtophys(np->script);

	src = script->start;
	dst = np->script->start;

	start = src;
	end = src + (sizeof (struct script) / 4);

	while (src < end) {

		*dst++ = opcode = *src++;

		/*
		**	If we forget to change the length
		**	in struct script, a field will be
		**	padded with 0. This is an illegal
		**	command.
		*/

		if (opcode == 0) {
			printf ("%s: ERROR0 IN SCRIPT at %d.\n",
				ncr_name(np), src-start-1);
			DELAY (1000000);
		};

		if (DEBUG_FLAGS & DEBUG_SCRIPT)
			printf ("%x:  <%x>\n",
				(unsigned)(src-1), (unsigned)opcode);

		/*
		**	We don't have to decode ALL commands
		*/
		switch (opcode >> 28) {

		case 0xc:
			/*
			**	COPY has TWO arguments.
			*/
			relocs = 2;
			if ((src[0] ^ src[1]) & 3) {
				printf ("%s: ERROR1 IN SCRIPT at %d.\n",
					ncr_name(np), src-start-1);
				DELAY (1000000);
			};
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
				case RELOC_SOFTC:
					new = (old & ~RELOC_MASK) + vtophys(np);
					break;
				case 0:
					/* Don't relocate a 0 address. */
					if (old == 0) {
						new = old;
						break;
					}
					/* fall through */
				default:
					new = vtophys(old);
					break;
				}

				*dst++ = new;
			}
		} else
			*dst++ = *src++;

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

/*----------------------------------------------------------
**
**	Maximal number of outstanding requests per target.
**
**----------------------------------------------------------
*/

U_INT32 ncr_info (int unit)
{
	return (1);   /* may be changed later */
}

/*----------------------------------------------------------
**
**	Probe the hostadapter.
**
**----------------------------------------------------------
*/

#ifdef __NetBSD__

int
ncr_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pci_attach_args *pa = aux;

#ifdef 0
	if (!pci_targmatch(cf, pa))
		return 0;
#endif
	if (pa->pa_id != NCR_810_ID &&
	    pa->pa_id != NCR_810AP_ID &&
	    pa->pa_id != NCR_815_ID &&
	    pa->pa_id != NCR_825_ID &&
	    pa->pa_id != NCR_860_ID &&
	    pa->pa_id != NCR_875_ID)
		return 0;

	return 1;
}

#else /* !__NetBSD__ */


static	char* ncr_probe (pcici_t tag, pcidi_t type)
{
	switch (type) {

	case NCR_810_ID:
		return ("ncr 53c810 scsi");

	case NCR_810AP_ID:
		return ("ncr 53c810ap scsi");

	case NCR_815_ID:
		return ("ncr 53c815 scsi");

	case NCR_825_ID:
		return ("ncr 53c825 wide scsi");

	case NCR_860_ID:
		return ("ncr 53c860 scsi");

	case NCR_875_ID:
		return ("ncr 53c875 wide scsi");
	}
	return (NULL);
}

#endif /* !__NetBSD__ */


/*==========================================================
**
**
**      Auto configuration:  attach and init a host adapter.
**
**
**==========================================================
*/

#define	MIN_ASYNC_PD	40
#define	MIN_SYNC_PD	20

#ifdef __NetBSD__

int
ncr_print()
{
}

void
ncr_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	int retval;
	ncb_p np = (void *)self;

	/*
	** XXX NetBSD
	** Perhaps try to figure what which model chip it is and print that
	** out.
	*/
	printf("\n");

	/*
	**	Try to map the controller chip to
	**	virtual and physical memory.
	*/

	retval = pci_map_mem(pa->pa_tag, 0x14, &np->vaddr, &np->paddr);
	if (retval)
		return;

	np->sc_ih = pci_map_int(pa->pa_tag, PCI_IPL_BIO, ncr_intr, np);
	if (np->sc_ih == NULL)
		return;


#else /* !__NetBSD__ */

static	void ncr_attach (pcici_t config_id, int unit)
{
	ncb_p np = (struct ncb*) 0;
#if ! (__FreeBSD__ >= 2)
	extern unsigned bio_imask;
#endif

#if (__FreeBSD__ >= 2)
	struct scsibus_data *scbus;
#endif

	/*
	**	allocate structure
	*/

	if (!np) {
		np = (ncb_p) malloc (sizeof (struct ncb), M_DEVBUF, M_WAITOK);
		if (!np) return;
		ncrp[unit]=np;
	}

	/*
	**	initialize structure.
	*/

	bzero (np, sizeof (*np));
	np->unit = unit;

	/*
	**	Try to map the controller chip to
	**	virtual and physical memory.
	*/

	if (!pci_map_mem (config_id, 0x14, &np->vaddr, &np->paddr))
		return;

#ifdef NCR_IOMAPPED
	/*
	**	Try to map the controller chip into iospace.
	*/

	if (!pci_map_port (config_id, 0x10, &np->port))
		return;
#endif

#endif /* !__NetBSD__ */

	/*
	**	Do chip dependent initialization.
	*/

#ifdef __NetBSD__
	switch (pa->pa_id) {
#else /* !__NetBSD__ */
	switch (pci_conf_read (config_id, PCI_ID_REG)) {
#endif /* __NetBSD__ */
	case NCR_825_ID:
	case NCR_875_ID:
		np->maxwide = 1;
		break;
	default:
		np->maxwide = 0;
		break;
	}

	/*
	**	Patch script to physical addresses
	*/

	ncr_script_fill (&script0);
	ncr_script_copy_and_bind (&script0, np);
	np->ccb.p_ccb		= vtophys (&np->ccb);

	/*
	**	init data structure
	*/

	np->jump_tcb.l_cmd	= SCR_JUMP;
	np->jump_tcb.l_paddr	= NCB_SCRIPT_PHYS (np, abort);

	/*
	**	Make the controller's registers available.
	**	Now the INB INW INL OUTB OUTW OUTL macros
	**	can be used safely.
	*/

	np->reg = (struct ncr_reg*) np->vaddr;

	/*
	**  Get SCSI addr of host adapter (set by bios?).
	*/

	np->myaddr = INB(nc_scid) & 0x07;
	if (!np->myaddr) np->myaddr = SCSI_NCR_MYADDR;

	/*
	**	Get the value of the chip's clock.
	**	Find the right value for scntl3.
	*/

	ncr_getclock (np);

	/*
	**	Reset chip.
	*/

	OUTB (nc_istat,  SRST);
	DELAY (1000);
	OUTB (nc_istat,  0   );

#ifdef NCR_DUMP_REG
	/*
	**	Log the initial register contents
	*/
	{
		int reg;
#ifdef __NetBSD__
		u_long config_id = pa->pa_tag;
#endif /* __NetBSD__ */
		for (reg=0; reg<256; reg+=4) {
			if (reg%16==0) printf ("reg[%2x]", reg);
			printf (" %08x", (int)pci_conf_read (config_id, reg));
			if (reg%16==12) printf ("\n");
		}
	}

	/*
	**	Reset chip, once again.
	*/

	OUTB (nc_istat,  SRST);
	DELAY (1000);
	OUTB (nc_istat,  0   );

#endif /* NCR_DUMP_REG */

	/*
	**	Now check the cache handling of the pci chipset.
	*/

	if (ncr_snooptest (np)) {
		printf ("CACHE INCORRECTLY CONFIGURED.\n");
		return;
	};

#ifndef __NetBSD__
	/*
	**	Install the interrupt handler.
	*/

	if (!pci_map_int (config_id, ncr_intr, np, &bio_imask))
		printf ("\tinterruptless mode: reduced performance.\n");
#endif /* __NetBSD__ */

	/*
	**	After SCSI devices have been opened, we cannot
	**	reset the bus safely, so we do it here.
	**	Interrupt handler does the real work.
	*/

	OUTB (nc_scntl1, CRST);
	DELAY (1000);

	/*
	**	Process the reset exception,
	**	if interrupts are not enabled yet.
	**	Then enable disconnects.
	*/
	ncr_exception (np);
	np->disc = 1;

	/*
	**	Now let the generic SCSI driver
	**	look for the SCSI devices on the bus ..
	*/

#ifdef __NetBSD__
	np->sc_link.adapter_softc = np;
	np->sc_link.adapter_target = np->myaddr;
	np->sc_link.openings = 1;
#else /* !__NetBSD__ */
	np->sc_link.adapter_unit = unit;
	np->sc_link.adapter_targ = np->myaddr;
	np->sc_link.fordriver	 = 0;
#endif /* !__NetBSD__ */
	np->sc_link.adapter      = &ncr_switch;
	np->sc_link.device       = &ncr_dev;
	np->sc_link.flags	 = 0;

#ifdef __NetBSD__
	config_found(self, &np->sc_link, ncr_print);
#else /* !__NetBSD__ */
#if (__FreeBSD__ >= 2)
	scbus = scsi_alloc_bus();
	if(!scbus)
		return;
	scbus->adapter_link = &np->sc_link;

	if(np->maxwide)
		scbus->maxtarg = 15;

	if (bootverbose) {
		unsigned t_from = 0;
		unsigned t_to   = scbus->maxtarg;
		unsigned myaddr = np->myaddr;

		char *txt_and = "";
		printf ("%s scanning for targets ", ncr_name (np));
		if (t_from < myaddr) {
			printf ("%d..%d ", t_from, myaddr -1);
			txt_and = "and ";
		}
		if (myaddr < t_to)
			printf ("%s%d..%d ", txt_and, myaddr +1, t_to);
		printf ("(V%d " NCR_DATE ")\n", NCR_VERSION);
	}
		
	scsi_attachdevs (scbus);
	scbus = NULL;   /* Upper-level SCSI code owns this now */
#else
	scsi_attachdevs (&np->sc_link);
#endif /* !__FreeBSD__ >= 2 */
#endif /* !__NetBSD__ */

	/*
	**	start the timeout daemon
	*/
	ncr_timeout (np);
	np->lasttime=0;

	/*
	**  Done.
	*/

	return;
}

/*==========================================================
**
**
**	Process pending device interrupts.
**
**
**==========================================================
*/

#ifdef __NetBSD__
int
ncr_intr(arg)
        void *arg;
{               
        ncb_p np = arg;
#else /* !__NetBSD__ */
int
ncr_intr(np)
	ncb_p np;
{
#endif /* __NetBSD__ */
	int n = 0;
	int oldspl = splbio();

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("[");

	if (INB(nc_istat) & (INTF|SIP|DIP)) {
		/*
		**	Repeat until no outstanding ints
		*/
		do {
			ncr_exception (np);
		} while (INB(nc_istat) & (INTF|SIP|DIP));

		n=1;
		np->ticks = 100;
	};

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("]\n");

	splx (oldspl);
	return (n);
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

static INT32 ncr_start (struct scsi_xfer * xp)
{
#ifdef __NetBSD__
	ncb_p np  = xp->sc_link->adapter_softc;
#else /*__NetBSD__*/
	ncb_p np  = ncrp[xp->sc_link->adapter_unit];
#endif/*__NetBSD__*/

	struct scsi_generic * cmd = xp->cmd;
	ccb_p cp;
	lcb_p lp;
	tcb_p tp = &np->target[xp->sc_link->target];

	int	i, oldspl, segments, flags = xp->flags;
	u_char	ptr, nego, idmsg;
	u_long  msglen, msglen2;



	/*---------------------------------------------
	**
	**   Reset SCSI bus
	**
	**	Interrupt handler does the real work.
	**
	**---------------------------------------------
	*/

	if (flags & SCSI_RESET) {
		OUTB (nc_scntl1, CRST);
		DELAY (1000);
		return(COMPLETE);
	};

	/*---------------------------------------------
	**
	**      Some shortcuts ...
	**
	**---------------------------------------------
	*/

	if ((xp->sc_link->target == np->myaddr	  ) ||
		(xp->sc_link->target >= MAX_TARGET) ||
		(xp->sc_link->lun    >= MAX_LUN   ) ||
		(flags    & SCSI_DATA_UIO)) {
		xp->error = XS_DRIVER_STUFFUP;
		return(COMPLETE);
	};

	/*---------------------------------------------
	**
	**      Diskaccess to partial blocks?
	**
	**---------------------------------------------
	*/

	if ((xp->datalen & 0x1ff) && !(tp->inqdata[0] & 0x1f)) {
		switch (cmd->opcode) {
		case 0x28:  /* READ_BIG  (10) */
		case 0xa8:  /* READ_HUGE (12) */
		case 0x2a:  /* WRITE_BIG (10) */
		case 0xaa:  /* WRITE_HUGE(12) */
			PRINT_ADDR(xp);
			printf ("access to partial disk block refused.\n");
			xp->error = XS_DRIVER_STUFFUP;
			return(COMPLETE);
		};
	};

	if (DEBUG_FLAGS & DEBUG_TINY) {
		PRINT_ADDR(xp);
		printf ("CMD=%x F=%x L=%x ", cmd->opcode,
			(unsigned)xp->flags, (unsigned) xp->datalen);
	}

	/*--------------------------------------------
	**
	**   Sanity checks ...
	**	copied from Elischer's Adaptec driver.
	**
	**--------------------------------------------
	*/

	flags = xp->flags;
	if (!(flags & INUSE)) {
		printf("%s: ?INUSE?\n", ncr_name (np));
		xp->flags |= INUSE;
	};

	if(flags & ITSDONE) {
		printf("%s: ?ITSDONE?\n", ncr_name (np));
		xp->flags &= ~ITSDONE;
	};

	if (xp->bp)
		flags |= (SCSI_NOSLEEP); /* just to be sure */

	/*---------------------------------------------------
	**
	**	Assign a ccb / bind xp
	**
	**----------------------------------------------------
	*/

	oldspl = splbio();

	if (!(cp=ncr_get_ccb (np, flags, xp->sc_link->target, xp->sc_link->lun))) {
		printf ("%s: no ccb.\n", ncr_name (np));
		xp->error = XS_DRIVER_STUFFUP;
		splx(oldspl);
		return(TRY_AGAIN_LATER);
	};
	cp->xfer = xp;

	/*---------------------------------------------------
	**
	**	timestamp
	**
	**----------------------------------------------------
	*/

	bzero (&cp->phys.header.stamp, sizeof (struct tstamp));
	cp->phys.header.stamp.start = time;

	/*----------------------------------------------------
	**
	**	Get device quirks from a speciality table.
	**
	**	@GENSCSI@
	**	This should be a part of the device table
	**	in "scsi_conf.c".
	**
	**----------------------------------------------------
	*/

	if (tp->quirks & QUIRK_UPDATE) {
#ifdef NEW_SCSICONF
		tp->quirks = xp->sc_link->quirks;
#else
		tp->quirks = ncr_lookup ((char*) &tp->inqdata[0]);
#endif
#ifndef NCR_GETCC_WITHMSG
		if (tp->quirks) {
			PRINT_ADDR(xp);
			printf ("quirks=%x.\n", tp->quirks);
		};
#endif
	};

	/*---------------------------------------------------
	**
	**	negotiation required?
	**
	**----------------------------------------------------
	*/

	nego = 0;

	if (tp->inqdata[7]) {
		/*
		**	negotiate wide transfers ?
		*/

		if (!tp->widedone) {
			if (tp->inqdata[7] & INQ7_WIDE16) {
				nego = NS_WIDE;
			} else
				tp->widedone=1;
		};

		/*
		**	negotiate synchronous transfers?
		*/

		if (!nego && !tp->period) {
			if (SCSI_NCR_MAX_SYNC 
#if defined (CDROM_ASYNC)
			    && ((tp->inqdata[0] & 0x1f) != 5)
#endif
			    && (tp->inqdata[7] & INQ7_SYNC)) {
				nego = NS_SYNC;
			} else {
				tp->period  =0xffff;
				tp->sval = 0xe0;
				PRINT_ADDR(xp);
				printf ("asynchronous.\n");
			};
		};
	};

	/*---------------------------------------------------
	**
	**	choose a new tag ...
	**
	**----------------------------------------------------
	*/

	if ((lp = tp->lp[xp->sc_link->lun]) && (lp->usetags)) {
		/*
		**	assign a tag to this ccb!
		*/
		while (!cp->tag) {
			ccb_p cp2 = lp->next_ccb;
			lp->lasttag = lp->lasttag % 255 + 1;
			while (cp2 && cp2->tag != lp->lasttag)
				cp2 = cp2->next_ccb;
			if (cp2) continue;
			cp->tag=lp->lasttag;
			if (DEBUG_FLAGS & DEBUG_TAGS) {
				PRINT_ADDR(xp);
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

	idmsg = M_IDENTIFY | xp->sc_link->lun;
#ifndef NCR_NO_DISCONNECT
	/*---------------------------------------------------------------------
	** Some users have problems with this driver.
	** I assume that the current problems relate to a conflict between
	** a disconnect and an immediately following reconnect operation.
	** With this option one can prevent the driver from using disconnects.
	** Without disconnects the performance will be severely degraded.
	** But it may help to trace down the core problem.
	**---------------------------------------------------------------------
	*/
	if ((cp!=&np->ccb) && (np->disc))
		idmsg |= 0x40;
#endif

	cp -> scsi_smsg [0] = idmsg;
	msglen=1;

	if (cp->tag) {

		/*
		**	Ordered write ops, unordered read ops.
		*/
		switch (cmd->opcode) {
		case 0x08:  /* READ_SMALL (6) */
		case 0x28:  /* READ_BIG  (10) */
		case 0xa8:  /* READ_HUGE (12) */
			cp -> scsi_smsg [msglen] = M_SIMPLE_TAG;
			break;
		default:
			cp -> scsi_smsg [msglen] = M_ORDERED_TAG;
		}

		/*
		**	can be overwritten by ncrcontrol
		*/
		switch (np->order) {
		case M_SIMPLE_TAG:
		case M_ORDERED_TAG:
			cp -> scsi_smsg [msglen] = np->order;
		};
		msglen++;
		cp -> scsi_smsg [msglen++] = cp -> tag;
	}

	switch (nego) {
	case NS_SYNC:
		cp -> scsi_smsg [msglen++] = M_EXTENDED;
		cp -> scsi_smsg [msglen++] = 3;
		cp -> scsi_smsg [msglen++] = M_X_SYNC_REQ;
		cp -> scsi_smsg [msglen++] = tp->minsync;
		cp -> scsi_smsg [msglen++] = tp->maxoffs;
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("sync msgout: ");
			ncr_show_msg (&cp->scsi_smsg [msglen-5]);
			printf (".\n");
		};
		break;
	case NS_WIDE:
		cp -> scsi_smsg [msglen++] = M_EXTENDED;
		cp -> scsi_smsg [msglen++] = 2;
		cp -> scsi_smsg [msglen++] = M_X_WIDE_REQ;
		cp -> scsi_smsg [msglen++] = tp->usrwide;
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
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

	cp -> scsi_smsg2 [0] = idmsg;
	msglen2 = 1;

	/*----------------------------------------------------
	**
	**	Build the data descriptors
	**
	**----------------------------------------------------
	*/

	segments = ncr_scatter (&cp->phys, (vm_offset_t) xp->data,
					(vm_size_t) xp->datalen);

	if (segments < 0) {
		xp->error = XS_DRIVER_STUFFUP;
		ncr_free_ccb(np, cp, flags);
		splx(oldspl);
		return(COMPLETE);
	};

	/*----------------------------------------------------
	**
	**	Set the SAVED_POINTER.
	**
	**----------------------------------------------------
	*/

	if (flags & SCSI_DATA_IN) {
		cp->phys.header.savep = NCB_SCRIPT_PHYS (np, data_in);
		cp->phys.header.goalp = cp->phys.header.savep +20 +segments*16;
	} else if (flags & SCSI_DATA_OUT) {
		cp->phys.header.savep = NCB_SCRIPT_PHYS (np, data_out);
		cp->phys.header.goalp = cp->phys.header.savep +20 +segments*16;
	} else {
		cp->phys.header.savep = NCB_SCRIPT_PHYS (np, no_data);
		cp->phys.header.goalp = cp->phys.header.savep;
	};
	cp->phys.header.lastp = cp->phys.header.savep;


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
	cp->phys.header.cp		= cp;
	/*
	**	Startqueue
	*/
	cp->phys.header.launch.l_paddr	= NCB_SCRIPT_PHYS (np, select);
	cp->phys.header.launch.l_cmd	= SCR_JUMP;
	/*
	**	select
	*/
	cp->phys.select.sel_id		= xp->sc_link->target;
	cp->phys.select.sel_scntl3	= tp->wval;
	cp->phys.select.sel_sxfer	= tp->sval;
	/*
	**	message
	*/
/*	cp->phys.smsg.addr		= cp->p_scsi_smsg;*/
	cp->phys.smsg.addr		= CCB_PHYS (cp, scsi_smsg);
	cp->phys.smsg.size		= msglen;

/*	cp->phys.smsg2.addr		= cp->p_scsi_smsg2;*/
	cp->phys.smsg2.addr		= CCB_PHYS (cp, scsi_smsg2);
	cp->phys.smsg2.size		= msglen2;
	/*
	**	command
	*/
	cp->phys.cmd.addr		= vtophys (cmd);
	cp->phys.cmd.size		= xp->cmdlen;
	/*
	**	sense command
	*/
/*	cp->phys.scmd.addr		= cp->p_sensecmd;*/
	cp->phys.scmd.addr		= CCB_PHYS (cp, sensecmd);
	cp->phys.scmd.size		= 6;
	/*
	**	patch requested size into sense command
	*/
	cp->sensecmd[0]			= 0x03;
	cp->sensecmd[1]			= xp->sc_link->lun << 5;
	cp->sensecmd[4]			= sizeof(struct scsi_sense_data);
	if (xp->req_sense_length)
		cp->sensecmd[4]		= xp->req_sense_length;
	/*
	**	sense data
	*/
	cp->phys.sense.addr		= vtophys (&cp->xfer->sense);
	cp->phys.sense.size		= sizeof(struct scsi_sense_data);
	/*
	**	status
	*/
	cp->actualquirks		= tp->quirks;
	cp->host_status			= nego ? HS_NEGOTIATE : HS_BUSY;
	cp->scsi_status			= S_ILLEGAL;
	cp->parity_status		= 0;

	cp->xerr_status			= XE_OK;
	cp->sync_status			= tp->sval;
	cp->nego_status			= nego;
	cp->wide_status			= tp->wval;

	/*----------------------------------------------------
	**
	**	Critical region: start this job.
	**
	**----------------------------------------------------
	*/

	/*
	**	reselect pattern and activate this job.
	*/

	cp->jump_ccb.l_cmd	= (SCR_JUMP ^ IFFALSE (DATA (cp->tag)));
	cp->tlimit		= time.tv_sec + xp->timeout / 1000 + 2;
	cp->magic		= CCB_MAGIC;

	/*
	**	insert into start queue.
	*/

	ptr = np->squeueput + 1;
	if (ptr >= MAX_START) ptr=0;
	np->squeue [ptr	  ] = NCB_SCRIPT_PHYS (np, idle);
	np->squeue [np->squeueput] = CCB_PHYS (cp, phys);
	np->squeueput = ptr;

	if(DEBUG_FLAGS & DEBUG_QUEUE)
		printf ("%s: queuepos=%d tryoffset=%d.\n", ncr_name (np),
		np->squeueput,
		(unsigned)(np->script->startpos[0]- 
			   (NCB_SCRIPT_PHYS (np, tryloop))));

	/*
	**	Script processor may be waiting for reselect.
	**	Wake it up.
	*/
	OUTB (nc_istat, SIGP);

	/*
	**	and reenable interrupts
	*/
	splx (oldspl);

	/*
	**	If interrupts are enabled, return now.
	**	Command is successfully queued.
	*/

#ifdef __NetBSD__
        if (!(flags & SCSI_POLL)) {
#else /* !__NetBSD__ */ 
	if (!(flags & SCSI_NOMASK)) {
#endif /* __NetBSD__ */
		if (np->lasttime) {
			if(DEBUG_FLAGS & DEBUG_TINY) printf ("Q");
			return(SUCCESSFULLY_QUEUED);
		};
	};

	/*----------------------------------------------------
	**
	**	Interrupts not yet enabled - have to poll.
	**
	**----------------------------------------------------
	*/

	if (DEBUG_FLAGS & DEBUG_POLL) printf("P");

	for (i=xp->timeout; i && !(xp->flags & ITSDONE);i--) {
		if ((DEBUG_FLAGS & DEBUG_POLL) && (cp->host_status))
			printf ("%c", (cp->host_status & 0xf) + '0');
		DELAY (1000);
		ncr_exception (np);
	};

	/*
	**	Abort if command not done.
	*/
	if (!(xp->flags & ITSDONE)) {
		printf ("%s: aborting job ...\n", ncr_name (np));
		OUTB (nc_istat, CABRT);
		DELAY (100000);
		OUTB (nc_istat, SIGP);
		ncr_exception (np);
	};

	if (!(xp->flags & ITSDONE)) {
		printf ("%s: abortion failed at %x.\n",
			ncr_name (np), (unsigned) INL(nc_dsp));
		ncr_init (np, "timeout", HS_TIMEOUT);
	};

	if (!(xp->flags & ITSDONE)) {
		cp-> host_status = HS_SEL_TIMEOUT;
		ncr_complete (np, cp);
	};

	if (DEBUG_FLAGS & DEBUG_RESULT) {
		printf ("%s: result: %x %x.\n",
			ncr_name (np), cp->host_status, cp->scsi_status);
	};
#ifdef __NetBSD__
        if (!(flags & SCSI_POLL)) 
#else /* !__NetBSD__ */ 
	if (!(flags & SCSI_NOMASK))
#endif /* __NetBSD__ */
		return (SUCCESSFULLY_QUEUED);
	switch (xp->error) {
	case  0     : return (COMPLETE);
	case XS_BUSY: return (TRY_AGAIN_LATER);
	};
	return (COMPLETE);
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

void ncr_complete (ncb_p np, ccb_p cp)
{
	struct scsi_xfer * xp;
	tcb_p tp;
	lcb_p lp;

	/*
	**	Sanity check
	*/

	if (!cp || (cp->magic!=CCB_MAGIC) || !cp->xfer) return;
	cp->magic = 1;
	cp->tlimit= 0;

	/*
	**	No Reselect anymore.
	*/
	cp->jump_ccb.l_cmd = (SCR_JUMP);

	/*
	**	No starting.
	*/
	cp->phys.header.launch.l_paddr= NCB_SCRIPT_PHYS (np, idle);

	/*
	**	timestamp
	*/
	ncb_profile (np, cp);

	if (DEBUG_FLAGS & DEBUG_TINY)
		printf ("CCB=%x STAT=%x/%x\n", (unsigned)cp & 0xfff,
			cp->host_status,cp->scsi_status);

	xp = cp->xfer;
	cp->xfer = NULL;
	tp = &np->target[xp->sc_link->target];
	lp = tp->lp[xp->sc_link->lun];

	/*
	**	Check for parity errors.
	*/

	if (cp->parity_status) {
		PRINT_ADDR(xp);
		printf ("%d parity error(s), fallback.\n", cp->parity_status);
		/*
		**	fallback to asynch transfer.
		*/
		tp->usrsync=255;
		tp->period =  0;
	};

	/*
	**	Check for extended errors.
	*/

	if (cp->xerr_status != XE_OK) {
		PRINT_ADDR(xp);
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
#ifdef __NetBSD__
	if (xp->error != XS_NOERROR) { 
                                
                /*              
                **      Don't override the error value.
                */
	} else                        
#endif /* __NetBSD__ */
	if (   (cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_GOOD)) {

		/*
		**	All went well.
		*/

		xp->resid = 0;

		/*
		** if (cp->phys.header.lastp != cp->phys.header.goalp)...
		**
		**	@RESID@
		**	Could dig out the correct value for resid,
		**	but it would be quite complicated.
		**
		**	The ah1542.c driver sets it to 0 too ...
		*/

		/*
		**	Try to assign a ccb to this nexus
		*/
		ncr_alloc_ccb (np, xp);

		/*
		**	On inquire cmd (0x12) save some data.
		*/
		if (xp->cmd->opcode == 0x12) {
			bcopy (	xp->data,
				&tp->inqdata,
				sizeof (tp->inqdata));

			/*
			**	set number of tags
			*/
			ncr_setmaxtags (tp, tp->usrtags);

			/*
			**	prepare negotiation of synch and wide.
			*/
			ncr_negotiate (np, tp);

			/*
			**	force quirks update before next command start
			*/
			tp->quirks |= QUIRK_UPDATE;
		};

		/*
		**	Announce changes to the generic driver
		*/
		if (lp) {
			ncr_settags (tp, lp);
			if (lp->reqlink != lp->actlink)
				ncr_opennings (np, lp, xp);
		};

		tp->bytes     += xp->datalen;
		tp->transfers ++;
#ifndef __NetBSD__
	} else if (xp->flags & SCSI_ERR_OK) {

		/*
		**   Not correct, but errors expected.
		*/
		xp->resid = 0;
#endif /* !__NetBSD__ */
	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == (S_SENSE|S_GOOD))) {

		/*
		**   Check condition code
		*/
		xp->error = XS_SENSE;

		if (DEBUG_FLAGS & (DEBUG_RESULT|DEBUG_TINY)) {
			u_char * p = (u_char*) & xp->sense;
			int i;
			printf ("\n%s: sense data:", ncr_name (np));
			for (i=0; i<14; i++) printf (" %x", *p++);
			printf (".\n");
		};

	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_BUSY)) {

		/*
		**   Target is busy.
		*/
		xp->error = XS_BUSY;

	} else if ((cp->host_status == HS_SEL_TIMEOUT)
		|| (cp->host_status == HS_TIMEOUT)) {

		/*
		**   No response
		*/
		xp->error = XS_TIMEOUT;

	} else {

		/*
		**  Other protocol messes
		*/
		PRINT_ADDR(xp);
		printf ("COMMAND FAILED (%x %x) @%x.\n",
			cp->host_status, cp->scsi_status, (unsigned)cp);

		xp->error = XS_TIMEOUT;
	}

	xp->flags |= ITSDONE;

	/*
	**	trace output
	*/

	if (tp->usrflag & UF_TRACE) {
		u_char * p;
		int i;
		PRINT_ADDR(xp);
		printf (" CMD:");
		p = (u_char*) &xp->cmd->opcode;
		for (i=0; i<xp->cmdlen; i++) printf (" %x", *p++);

		if (cp->host_status==HS_COMPLETE) {
			switch (cp->scsi_status) {
			case S_GOOD:
				printf ("  GOOD");
				break;
			case S_CHECK_COND:
				printf ("  SENSE:");
				p = (u_char*) &xp->sense;
				for (i=0; i<xp->req_sense_length; i++)
					printf (" %x", *p++);
				break;
			default:
				printf ("  STAT: %x\n", cp->scsi_status);
				break;
			};
		} else printf ("  HOSTERROR: %x", cp->host_status);
		printf ("\n");
	};

	/*
	**	Free this ccb
	*/
	ncr_free_ccb (np, cp, xp->flags);

	/*
	**	signal completion to generic driver.
	*/
	scsi_done (xp);
}

/*==========================================================
**
**
**	Signal all (or one) control block done.
**
**
**==========================================================
*/

void ncr_wakeup (ncb_p np, u_long code)
{
	/*
	**	Starting at the default ccb and following
	**	the links, complete all jobs with a
	**	host_status greater than "disconnect".
	**
	**	If the "code" parameter is not zero,
	**	complete all jobs that are not IDLE.
	*/

	ccb_p cp = &np->ccb;
	while (cp) {
		switch (cp->host_status) {

		case HS_IDLE:
			break;

		case HS_DISCONNECT:
			if(DEBUG_FLAGS & DEBUG_TINY) printf ("D");
			/* fall through */

		case HS_BUSY:
		case HS_NEGOTIATE:
			if (!code) break;
			cp->host_status = code;

			/* fall through */

		default:
			ncr_complete (np, cp);
			break;
		};
		cp = cp -> link_ccb;
	};
}

/*==========================================================
**
**
**	Start NCR chip.
**
**
**==========================================================
*/

void ncr_init (ncb_p np, char * msg, u_long code)
{
	int	i;
	u_long	usrsync;
	u_char	usrwide;
	u_char	burstlen;

	/*
	**	Reset chip.
	*/

	OUTB (nc_istat,  SRST);
	DELAY (1000);

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
	np->script->startpos[0] = NCB_SCRIPT_PHYS (np, tryloop);
	np->script->start0  [0] = SCR_INT ^ IFFALSE (0);

	/*
	**	Wakeup all pending jobs.
	*/

	ncr_wakeup (np, code);

	/*
	**	Init chip.
	*/

#ifndef __NetBSD__
	if (pci_max_burst_len < 4) {
		static u_char tbl[4]={0,0,0x40,0x80};
		burstlen = tbl[pci_max_burst_len];
	} else burstlen = 0xc0;
#else /* !__NetBSD__ */
	burstlen = 0xc0;
#endif /* __NetBSD__ */

	OUTB (nc_istat,  0      );      /*  Remove Reset, abort ...	     */
	OUTB (nc_scntl0, 0xca   );      /*  full arb., ena parity, par->ATN  */
	OUTB (nc_scntl1, 0x00	);	/*  odd parity, and remove CRST!!    */
	OUTB (nc_scntl3, np->rv_scntl3);/*  timing prescaler		     */
	OUTB (nc_scid  , RRE|np->myaddr);/*  host adapter SCSI address       */
	OUTW (nc_respid, 1ul<<np->myaddr);/*  id to respond to		     */
	OUTB (nc_istat , SIGP	);	/*  Signal Process		     */
	OUTB (nc_dmode , burstlen);	/*  Burst length = 2 .. 16 transfers */
	OUTB (nc_dcntl , NOCOM  );      /*  no single step mode, protect SFBR*/
	OUTB (nc_ctest4, 0x08	);	/*  enable master parity checking    */
	OUTB (nc_stest2, EXT    );	/*  Extended Sreq/Sack filtering     */
	OUTB (nc_stest3, TE     );	/*  TolerANT enable		     */
	OUTB (nc_stime0, 0xfb	);	/*  HTH = 1.6sec  STO = 0.1 sec.     */

	/*
	**	Reinitialize usrsync.
	**	Have to renegotiate synch mode.
	*/

	usrsync = 255;
	if (SCSI_NCR_MAX_SYNC) {
		u_long period;
		period =1000000/SCSI_NCR_MAX_SYNC; /* ns = 10e6 / kHz */
		if (period <= 11 * np->ns_sync) {
			if (period < 4 * np->ns_sync)
				usrsync = np->ns_sync;
			else
				usrsync = period / 4;
		};
	};

	/*
	**	Reinitialize usrwide.
	**	Have to renegotiate wide mode.
	*/

	usrwide = (SCSI_NCR_MAX_WIDE);
	if (usrwide > np->maxwide) usrwide=np->maxwide;

	/*
	**	Disable disconnects.
	*/

	np->disc = 0;

	/*
	**	Fill in target structure.
	*/

	for (i=0;i<MAX_TARGET;i++) {
		tcb_p tp = &np->target[i];

		tp->sval    = 0;
		tp->wval    = np->rv_scntl3;

		tp->usrsync = usrsync;
		tp->usrwide = usrwide;

		ncr_negotiate (np, tp);
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

	if (minsync < 25) minsync=25;

	/*
	**	if not scsi 2
	**	don't believe FAST!
	*/

	if ((minsync < 50) && (tp->inqdata[2] & 0x0f) < 2)
		minsync=50;

	/*
	**	our limit ..
	*/

	if (minsync < np->ns_sync)
		minsync = np->ns_sync;

	/*
	**	divider limit
	*/

	if (minsync > (np->ns_sync * 11) / 4)
		minsync = 255;

	tp->minsync = minsync;
	tp->maxoffs = (minsync<255 ? 8 : 0);

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
**	Switch sync mode for current job and it's target
**
**==========================================================
*/

static void ncr_setsync (ncb_p np, ccb_p cp, u_char sxfer)
{
	struct scsi_xfer *xp;
	tcb_p tp;
	u_char target = INB (nc_ctest0)&7;

	assert (cp);
	if (!cp) return;

	xp = cp->xfer;
	assert (xp);
	if (!xp) return;
	assert (target == xp->sc_link->target & 7);

	tp = &np->target[target];
	tp->period= sxfer&0xf ? ((sxfer>>5)+4) * np->ns_sync : 0xffff;

	if (tp->sval == sxfer) return;
	tp->sval = sxfer;

	/*
	**	Bells and whistles   ;-)
	*/
	PRINT_ADDR(xp);
	if (sxfer & 0x0f) {
		/*
		**  Disable extended Sreq/Sack filtering
		*/
		if (tp->period <= 200) OUTB (nc_stest2, 0);
		printf ("%s%dns (%d Mb/sec) offset %d.\n",
			tp->period<200 ? "FAST SCSI-2 ":"",
			tp->period, (1000+tp->period/2)/tp->period,
			sxfer & 0x0f);
	} else  printf ("asynchronous.\n");

	/*
	**	set actual value and sync_status
	*/
	OUTB (nc_sxfer, sxfer);
	np->sync_st = sxfer;

	/*
	**	patch ALL ccbs of this target.
	*/
	for (cp = &np->ccb; cp; cp = cp->link_ccb) {
		if (!cp->xfer) continue;
		if (cp->xfer->sc_link->target != target) continue;
		cp->sync_status = sxfer;
	};
}

/*==========================================================
**
**	Switch wide mode for current job and it's target
**
**==========================================================
*/

static void ncr_setwide (ncb_p np, ccb_p cp, u_char wide)
{
	struct scsi_xfer *xp;
	u_short target = INB (nc_ctest0)&7;
	tcb_p tp;
	u_char	scntl3 = np->rv_scntl3 | (wide ? EWS : 0);

	assert (cp);
	if (!cp) return;

	xp = cp->xfer;
	assert (xp);
	if (!xp) return;
	assert (target == xp->sc_link->target & 7);

	tp = &np->target[target];
	tp->widedone  =  wide+1;
	if (tp->wval == scntl3) return;
	tp->wval = scntl3;

	/*
	**	Bells and whistles   ;-)
	*/
	PRINT_ADDR(xp);
	if (scntl3 & EWS)
		printf ("WIDE SCSI (16 bit) enabled.\n");
	else
		printf ("WIDE SCSI disabled.\n");

	/*
	**	set actual value and sync_status
	*/
	OUTB (nc_scntl3, scntl3);
	np->wide_st = scntl3;

	/*
	**	patch ALL ccbs of this target.
	*/
	for (cp = &np->ccb; cp; cp = cp->link_ccb) {
		if (!cp->xfer) continue;
		if (cp->xfer->sc_link->target != target) continue;
		cp->wide_status = scntl3;
	};
}

/*==========================================================
**
**	Switch tagged mode for a target.
**
**==========================================================
*/

static void ncr_setmaxtags (tcb_p tp, u_long usrtags)
{
	int l;
	tp->usrtags = usrtags;
	for (l=0; l<MAX_LUN; l++) {
		lcb_p lp;
		if (!tp) break;
		lp=tp->lp[l];
		if (!lp) continue;
		ncr_settags (tp, lp);
	};
}

static void ncr_settags (tcb_p tp, lcb_p lp)
{
	u_char reqtags, tmp;

	if ((!tp) || (!lp)) return;

	/*
	**	only devices capable of tagges commands
	**	only disk devices
	**	only if enabled by user ..
	*/
	if ((  tp->inqdata[7] & INQ7_QUEUE) && ((tp->inqdata[0] & 0x1f)==0x00)
		&& tp->usrtags) {
		reqtags = tp->usrtags;
		if (lp->actlink <= 1)
			lp->usetags=reqtags;
	} else {
		reqtags = 1;
		if (lp->actlink <= 1)
			lp->usetags=0;
	};

	/*
	**	don't announce more than available.
	*/
	tmp = lp->actccbs;
	if (tmp > reqtags) tmp = reqtags;
	lp->reqlink = tmp;

	/*
	**	don't discard if announced.
	*/
	tmp = lp->actlink;
	if (tmp < reqtags) tmp = reqtags;
	lp->reqccbs = tmp;
}

/*----------------------------------------------------
**
**	handle user commands
**
**----------------------------------------------------
*/

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
		if (np->user.data > MAX_TAGS)
			break;
		for (t=0; t<MAX_TARGET; t++) {
			if (!((np->user.target>>t)&1)) continue;
			ncr_setmaxtags (&np->target[t], np->user.data);
		};
		break;

	case UC_SETDEBUG:
		ncr_debug = np->user.data;
		break;

	case UC_SETORDER:
		np->order = np->user.data;
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
	u_long	thistime = time.tv_sec;
	u_long	step  = np->ticks;
	u_long	count = 0;
	long signed   t;
	ccb_p cp;

	if (np->lasttime != thistime) {
		/*
		**	block ncr interrupts
		*/
		int oldspl = splbio();
		np->lasttime = thistime;

		ncr_usercmd (np);

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
			**      Let's wake it up.
			*/
			OUTB (nc_istat, SIGP);
		};

		if (np->latetime>4) {
			/*
			**	Although we tried to wake it up,
			**	the script processor didn't respond.
			**
			**	May be a target is hanging,
			**	or another initator lets a tape device
			**	rewind with disconnect disabled :-(
			**
			**	We won't accept that.
			*/
			if (INB (nc_sbcl) & CBSY)
				OUTB (nc_scntl1, CRST);
			DELAY (1000);
			ncr_init (np, "ncr dead ?", HS_TIMEOUT);
			np->heartbeat = thistime;
		};

		/*----------------------------------------------------
		**
		**	handle ccb timeouts
		**
		**----------------------------------------------------
		*/

		for (cp=&np->ccb; cp; cp=cp->link_ccb) {
			/*
			**	look for timed out ccbs.
			*/
			if (!cp->host_status) continue;
			count++;
			if (cp->tlimit > thistime) continue;

			/*
			**	Disable reselect.
			**      Remove it from startqueue.
			*/
			cp->jump_ccb.l_cmd = (SCR_JUMP);
			if (cp->phys.header.launch.l_paddr ==
				NCB_SCRIPT_PHYS (np, select)) {
				printf ("%s: timeout ccb=%x (skip)\n",
					ncr_name (np), (unsigned)cp);
				cp->phys.header.launch.l_paddr
				= NCB_SCRIPT_PHYS (np, skip);
			};

			switch (cp->host_status) {

			case HS_BUSY:
			case HS_NEGOTIATE:
				/*
				** still in start queue ?
				*/
				if (cp->phys.header.launch.l_paddr ==
					NCB_SCRIPT_PHYS (np, skip))
					continue;

				/* fall through */
			case HS_DISCONNECT:
				cp->host_status=HS_TIMEOUT;
			};
			cp->tag = 0;

			/*
			**	wakeup this ccb.
			*/
			ncr_complete (np, cp);
		};
		splx (oldspl);
	}

	timeout (TIMEOUT ncr_timeout, (caddr_t) np, step ? step : 1);

	if (INB(nc_istat) & (INTF|SIP|DIP)) {

		/*
		**	Process pending interrupts.
		*/

		int	oldspl	= splbio ();
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("{");
		ncr_exception (np);
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("}");
		splx (oldspl);
	};
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
	u_long	dsp, dsa;
	int	i, script_ofs;

	/*
	**	interrupt on the fly ?
	*/
	while ((istat = INB (nc_istat)) & INTF) {
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("F");
		OUTB (nc_istat, INTF);
		np->profile.num_fly++;
		ncr_wakeup (np, 0);
	};

	if (!(istat & (SIP|DIP))) return;

	/*
	**	Steinbach's Guideline for Systems Programming:
	**	Never test for an error condition you don't know how to handle.
	*/

	dstat = INB (nc_dstat);
	sist  = INW (nc_sist) ;
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
		ncr_int_ma (np);
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
	**	do the register dump
	**========================================
	*/

	if (time.tv_sec - np->regtime.tv_sec>10) {
		int i;
		np->regtime = time;
		for (i=0; i<sizeof(np->regdump); i++)
			((char*)&np->regdump)[i] = ((char*)np->reg)[i];
		np->regdump.nc_dstat = dstat;
		np->regdump.nc_sist  = sist;
	};

	/*=========================================
	**	log message for real hard errors
	**=========================================

	"ncr0 targ 0?: ERROR (ds:si) (so-si-sd) (sxfer/scntl3) @ (dsp:dbc)."
	"	      reg: r0 r1 r2 r3 r4 r5 r6 ..... rf."

	exception register:
		ds:	dstat
		si:	sist

	SCSI bus lines:
		so:	control lines as driver by NCR.
		si:	control lines as seen by NCR.
		sd:	scsi data lines as seen by NCR.

	wide/fastmode:
		sxfer:	(see the manual)
		scntl3:	(see the manual)

	current script command:
		dsp:	script adress (relative to start of script).
		dbc:	first word of script command.

	First 16 register of the chip:
		r0..rf

	=============================================
	*/

	dsp = (unsigned) INL (nc_dsp);
	dsa = (unsigned) INL (nc_dsa);

	script_ofs = dsp - np->p_script;

	printf ("%s:%d: ERROR (%x:%x) (%x-%x-%x) (%x/%x) @ (%x:%08x).\n",
		ncr_name (np), INB (nc_ctest0)&0x0f, dstat, sist,
		INB (nc_socl), INB (nc_sbcl), INB (nc_sbdl),
		INB (nc_sxfer),INB (nc_scntl3), script_ofs,
		(unsigned) INL (nc_dbc));

	if (((script_ofs & 3) == 0) &&
	    (unsigned)script_ofs < sizeof(struct script)) {
		printf ("\tscript cmd = %08x\n", 
			*(ncrcmd *)((char*)np->script +script_ofs));
	}

	printf ("\treg:\t");
	for (i=0; i<16;i++)
		printf (" %02x", ((u_char*)np->reg)[i]);
	printf (".\n");

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
		OUTB (nc_ctest3, CLF);		/* clear dma fifo  */
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
			OUTB (nc_dcntl, (STD|NOCOM));
			/*
			**	info message
			*/
			printf ("%s: INFO: LDSC while IID.\n",
				ncr_name (np));
			return;
		};
		printf ("%s: target %d doesn't release the bus.\n",
			ncr_name (np), INB (nc_ctest0)&0x0f);
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
		OUTB (nc_dcntl, (STD|NOCOM));
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
		OUTB (nc_ctest3, CLF);		/* clear scsi offsets */
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
			val = ((unsigned char*) np->vaddr) [i];
			printf (" %x%x", val/16, val%16);
			if (i%16==15) printf (".\n");
		};

		untimeout (TIMEOUT ncr_timeout, (caddr_t) np);

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
	ccb_p cp;
	if (DEBUG_FLAGS & DEBUG_TINY) printf ("T");

	/*
	**	look for ccb and set the status.
	*/

	dsa = INL (nc_dsa);
	cp = &np->ccb;
	while (cp && (CCB_PHYS (cp, phys) != dsa))
		cp = cp->link_ccb;

	if (cp) {
		cp-> host_status = HS_SEL_TIMEOUT;
		ncr_complete (np, cp);
	};

	/*
	**	repair start queue
	*/

	scratcha = INL (nc_scratcha);
	diff = scratcha - NCB_SCRIPT_PHYS (np, tryloop);

/*	assert ((diff <= MAX_START * 20) && !(diff % 20));*/

	if ((diff <= MAX_START * 20) && !(diff % 20)) {
		np->script->startpos[0] = scratcha;
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

static void ncr_int_ma (ncb_p np)
{
	u_long	dbc;
	u_long	rest;
	u_long	dsa;
	u_long	dsp;
	u_long	nxtdsp;
	u_long	*vdsp;
	u_long	oadr, olen;
	u_long	*tblp, *newcmd;
	u_char	cmd, sbcl, delta, ss0, ss2;
	ccb_p	cp;

	dsp = INL (nc_dsp);
	dsa = INL (nc_dsa);
	dbc = INL (nc_dbc);
	ss0 = INB (nc_sstat0);
	ss2 = INB (nc_sstat2);
	sbcl= INB (nc_sbcl);

	cmd = dbc >> 24;
	rest= dbc & 0xffffff;
	delta=(INB (nc_dfifo) - rest) & 0x7f;

	/*
	**	The data in the dma fifo has not been transfered to
	**	the target -> add the amount to the rest
	**	and clear the data.
	**	Check the sstat2 register in case of wide transfer.
	*/

	if (! (INB(nc_dstat) & DFE)) rest += delta;
	if (ss0 & OLF) rest++;
	if (ss0 & ORF) rest++;
	if (INB(nc_scntl3) & EWS) {
		if (ss2 & OLF1) rest++;
		if (ss2 & ORF1) rest++;
	};
	OUTB (nc_ctest3, CLF   );	/* clear dma fifo  */
	OUTB (nc_stest3, TE|CSF);	/* clear scsi fifo */

	/*
	**	locate matching cp
	*/
	dsa = INL (nc_dsa);
	cp = &np->ccb;
	while (cp && (CCB_PHYS (cp, phys) != dsa))
		cp = cp->link_ccb;

	if (!cp) {
	    printf ("%s: SCSI phase error fixup: CCB already dequeued (0x%08lx)\n", 
		    ncr_name (np), (u_long) np->header.cp);
	    return;
	}
	if (cp != np->header.cp) {
	    printf ("%s: SCSI phase error fixup: CCB address mismatch (0x%08lx != 0x%08lx)\n", 
		    ncr_name (np), (u_long) cp, (u_long) np->header.cp);
	    return;
	}

	/*
	**	find the interrupted script command,
	**	and the address at which to continue.
	*/

	if (dsp == vtophys (&cp->patch[2])) {
		vdsp = &cp->patch[0];
		nxtdsp = vdsp[3];
	} else if (dsp == vtophys (&cp->patch[6])) {
		vdsp = &cp->patch[4];
		nxtdsp = vdsp[3];
	} else {
		vdsp = (u_long*) ((char*)np->script - np->p_script + dsp -8);
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
		printf ("\nCP=%x CP2=%x DSP=%x NXT=%x VDSP=%x CMD=%x ",
			(unsigned)cp, (unsigned)np->header.cp,
			(unsigned)dsp,
			(unsigned)nxtdsp, (unsigned)vdsp, cmd);
	};

	/*
	**	get old startaddress and old length.
	*/

	oadr = vdsp[1];

	if (cmd & 0x10) {	/* Table indirect */
		tblp = (u_long*) ((char*) &cp->phys + oadr);
		olen = tblp[0];
		oadr = tblp[1];
	} else {
		tblp = (u_long*) 0;
		olen = vdsp[0] & 0xffffff;
	};

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("OCMD=%x\nTBLP=%x OLEN=%x OADR=%x\n",
			(unsigned) (vdsp[0] >> 24),
			(unsigned) tblp,
			(unsigned) olen,
			(unsigned) oadr);
	};

	/*
	**	if old phase not dataphase, leave here.
	*/

	if (cmd != (vdsp[0] >> 24)) {
		PRINT_ADDR(cp->xfer);
		printf ("internal error: cmd=%02x != %02x=(vdsp[0] >> 24)\n",
			(unsigned)cmd, (unsigned)vdsp[0] >> 24);
		
		return;
	}
	if (cmd & 0x06) {
		PRINT_ADDR(cp->xfer);
		printf ("phase change %x-%x %d@%08x resid=%d.\n",
			cmd&7, sbcl&7, (unsigned)olen,
			(unsigned)oadr, (unsigned)rest);

		OUTB (nc_dcntl, (STD|NOCOM));
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
		PRINT_ADDR(cp->xfer);
		printf ("newcmd[%d] %x %x %x %x.\n",
			newcmd - cp->patch,
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
	OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, dispatch));
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
	if (*msg==M_EXTENDED) {
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
	u_char chg, ofs, per, fak, wide;
	u_char num = INB (nc_dsps);
	ccb_p	cp=0;
	u_long	dsa;
	u_char	target = INB (nc_ctest0) & 7;
	tcb_p	tp     = &np->target[target];
	int     i;
	if (DEBUG_FLAGS & DEBUG_TINY) printf ("I#%d", num);

	switch (num) {
	case SIR_SENSE_RESTART:
	case SIR_STALL_RESTART:
		break;

	default:
		/*
		**	lookup the ccb
		*/
		dsa = INL (nc_dsa);
		cp = &np->ccb;
		while (cp && (CCB_PHYS (cp, phys) != dsa))
			cp = cp->link_ccb;

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
		cp = (ccb_p) 0;
		for (i=0; i<MAX_TARGET; i++) {
			if (DEBUG_FLAGS & DEBUG_RESTART) printf (" t%d", i);
			tp = &np->target[i];
			if (DEBUG_FLAGS & DEBUG_RESTART) printf ("+");
			cp = tp->hold_cp;
			if (!cp) continue;
			if (DEBUG_FLAGS & DEBUG_RESTART) printf ("+");
			if ((cp->host_status==HS_BUSY) &&
				(cp->scsi_status==S_CHECK_COND))
				break;
			if (DEBUG_FLAGS & DEBUG_RESTART) printf ("- (remove)");
			tp->hold_cp = cp = (ccb_p) 0;
		};

		if (cp) {
			if (DEBUG_FLAGS & DEBUG_RESTART)
				printf ("+ restart job ..\n");
			OUTL (nc_dsa, CCB_PHYS (cp, phys));
			OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, getcc));
			return;
		};

		/*
		**	no job, resume normal processing
		*/
		if (DEBUG_FLAGS & DEBUG_RESTART) printf (" -- remove trap\n");
		np->script->start0[0] =  SCR_INT ^ IFFALSE (0);
		break;

	case SIR_SENSE_FAILED:
		/*-------------------------------------------
		**	While trying to select for
		**	getting the condition code,
		**	a target reselected us.
		**-------------------------------------------
		*/
		if (DEBUG_FLAGS & DEBUG_RESTART) {
			PRINT_ADDR(cp->xfer);
			printf ("in getcc reselect by t%d.\n",
				INB(nc_ssid) & 0x0f);
		}

		/*
		**	Mark this job
		*/
		cp->host_status = HS_BUSY;
		cp->scsi_status = S_CHECK_COND;
		np->target[cp->xfer->sc_link->target].hold_cp = cp;

		/*
		**	And patch code to restart it.
		*/
		np->script->start0[0] =  SCR_INT;
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
**	When we set the values, we adjust them in all ccbs belonging 
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
			PRINT_ADDR(cp->xfer);
			printf ("negotiation failed sir=%x status=%x.\n",
				num, cp->nego_status);
		};

		/*
		**	any error in negotiation:
		**	fall back to default mode.
		*/
		switch (cp->nego_status) {

		case NS_SYNC:
			ncr_setsync (np, cp, 0xe0);
			break;

		case NS_WIDE:
			ncr_setwide (np, cp, 0);
			break;

		};
		np->msgin [0] = M_NOOP;
		np->msgout[0] = M_NOOP;
		cp->nego_status = 0;
		OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, dispatch));
		break;

	case SIR_NEGO_SYNC:
		/*
		**	Synchronous request message received.
		*/

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
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
		**      if target sends SDTR message,
		**	      it CAN transfer synch.
		*/

		if (ofs)
			tp->inqdata[7] |= INQ7_SYNC;

		/*
		**	check values against driver limits.
		*/

		if (per < np->ns_sync)
			{chg = 1; per = np->ns_sync;}
		if (per < tp->minsync)
			{chg = 1; per = tp->minsync;}
		if (ofs > tp->maxoffs)
			{chg = 1; ofs = tp->maxoffs;}

		/*
		**	Check against controller limits.
		*/
		fak = (4ul * per - 1) / np->ns_sync - 3;
		if (ofs && (fak>7))   {chg = 1; ofs = 0;}
		if (!ofs) fak=7;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("sync: per=%d ofs=%d fak=%d chg=%d.\n",
				per, ofs, fak, chg);
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
					ncr_setsync (np, cp, 0xe0);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, msg_bad));
				} else {
					/*
					**	Answer is ok.
					*/
					ncr_setsync (np, cp, (fak<<5)|ofs);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, clrack));
				};
				return;

			case NS_WIDE:
				ncr_setwide (np, cp, 0);
				break;
			};
		};

		/*
		**	It was a request. Set value and
		**      prepare an answer message
		*/

		ncr_setsync (np, cp, (fak<<5)|ofs);

		np->msgout[0] = M_EXTENDED;
		np->msgout[1] = 3;
		np->msgout[2] = M_X_SYNC_REQ;
		np->msgout[3] = per;
		np->msgout[4] = ofs;

		cp->nego_status = NS_SYNC;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("sync msgout: ");
			(void) ncr_show_msg (np->msgin);
			printf (".\n");
		}

		if (!ofs) {
			OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, msg_bad));
			return;
		}
		np->msgin [0] = M_NOOP;

		break;

	case SIR_NEGO_WIDE:
		/*
		**	Wide request message received.
		*/
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
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
		**      if target sends WDTR message,
		**	      it CAN transfer wide.
		*/

		if (wide)
			tp->inqdata[7] |= INQ7_WIDE16;

		/*
		**	check values against driver limits.
		*/

		if (wide > tp->usrwide)
			{chg = 1; wide = tp->usrwide;}

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
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
					ncr_setwide (np, cp, 0);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, msg_bad));
				} else {
					/*
					**	Answer is ok.
					*/
					ncr_setwide (np, cp, wide);
					OUTL (nc_dsp, NCB_SCRIPT_PHYS (np, clrack));
				};
				return;

			case NS_SYNC:
				ncr_setsync (np, cp, 0xe0);
				break;
			};
		};

		/*
		**	It was a request, set value and
		**      prepare an answer message
		*/

		ncr_setwide (np, cp, wide);

		np->msgout[0] = M_EXTENDED;
		np->msgout[1] = 2;
		np->msgout[2] = M_X_WIDE_REQ;
		np->msgout[3] = wide;

		np->msgin [0] = M_NOOP;

		cp->nego_status = NS_WIDE;

		if (DEBUG_FLAGS & DEBUG_NEGO) {
			PRINT_ADDR(cp->xfer);
			printf ("wide msgout: ");
			(void) ncr_show_msg (np->msgin);
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
		**	We received a M_REJECT message.
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->xfer);
		printf ("M_REJECT received (%x:%x).\n",
			(unsigned)np->lastmsg, np->msgout[0]);
		break;

	case SIR_REJECT_SENT:
		/*-----------------------------------------------
		**
		**	We received an unknown message
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->xfer);
		printf ("M_REJECT sent for ");
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

		PRINT_ADDR(cp->xfer);
		printf ("M_IGN_RESIDUE received, but not yet implemented.\n");
		break;

	case SIR_MISSING_SAVE:
		/*-----------------------------------------------
		**
		**	We received an DISCONNECT message,
		**	but the datapointer wasn't saved before.
		**
		**-----------------------------------------------
		*/

		PRINT_ADDR(cp->xfer);
		printf ("M_DISCONNECT received, but datapointer not saved:\n"
			"\tdata=%x save=%x goal=%x.\n",
			(unsigned) INL (nc_temp),
			(unsigned) np->header.savep,
			(unsigned) np->header.goalp);
		break;

/*--------------------------------------------------------------------
**
**	Processing of a "S_QUEUE_FULL" status.
**
**	The current command has been rejected,
**	because there are too many in the command queue.
**	We have started too many commands for that target.
**
**	If possible, reinsert at head of queue.
**	Stall queue until there are no disconnected jobs
**	(ncr is REALLY idle). Then restart processing.
**
**	We should restart the current job after the controller
**	has become idle. But this is not yet implemented.
**
**--------------------------------------------------------------------
*/
	case SIR_STALL_QUEUE:
		/*-----------------------------------------------
		**
		**	Stall the start queue.
		**
		**-----------------------------------------------
		*/
		PRINT_ADDR(cp->xfer);
		printf ("queue full.\n");

		np->script->start1[0] =  SCR_INT;

		/*
		**	Try to disable tagged transfers.
		*/
		ncr_setmaxtags (&np->target[target], 0);

		/*
		** @QUEUE@
		**
		**	Should update the launch field of the
		**	current job to be able to restart it.
		**	Then prepend it to the start queue.
		*/

		/* fall through */

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
		cp = &np->ccb;
		while (cp && cp->host_status != HS_DISCONNECT)
			cp = cp->link_ccb;

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
		np->script->start1[0] =  SCR_INT ^ IFFALSE (0);
		break;
	};

out:
	OUTB (nc_dcntl, (STD|NOCOM));
}

/*==========================================================
**
**
**	Aquire a control block
**
**
**==========================================================
*/

static	ccb_p ncr_get_ccb
	(ncb_p np, u_long flags, u_long target, u_long lun)
{
	lcb_p lp;
	ccb_p cp = (ccb_p) 0;

	/*
	**	Lun structure available ?
	*/

	lp = np->target[target].lp[lun];
	if (lp) {
		cp = lp->next_ccb;

		/*
		**	Look for free CCB
		*/

		while (cp && cp->magic) cp = cp->next_ccb;
	}

	/*
	**	if nothing available, take the default.
	*/

	if (!cp) cp = &np->ccb;

	/*
	**	Wait until available.
	*/

	while (cp->magic) {
		if (flags & SCSI_NOSLEEP) break;
		if (tsleep ((caddr_t)cp, PRIBIO|PCATCH, "ncr", 0))
			break;
	};

	if (cp->magic)
		return ((ccb_p) 0);

	cp->magic = 1;
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

void ncr_free_ccb (ncb_p np, ccb_p cp, int flags)
{
	/*
	**    sanity
	*/

	assert (cp != NULL);

	cp -> host_status = HS_IDLE;
	cp -> magic = 0;
	if (cp == &np->ccb)
		wakeup ((caddr_t) cp);
}

/*==========================================================
**
**
**      Allocation of resources for Targets/Luns/Tags.
**
**
**==========================================================
*/

static	void ncr_alloc_ccb (ncb_p np, struct scsi_xfer * xp)
{
	tcb_p tp;
	lcb_p lp;
	ccb_p cp;

	u_long	target;
	u_long	lun;

	assert (np != NULL);
	assert (xp != NULL);

	target = xp->sc_link->target;
	lun    = xp->sc_link->lun;

	if (target>=MAX_TARGET) return;
	if (lun   >=MAX_LUN   ) return;

	tp=&np->target[target];

	if (!tp->jump_tcb.l_cmd) {

		/*
		**	initialize it.
		*/
		tp->jump_tcb.l_cmd   = (SCR_JUMP^IFFALSE (DATA (0x80 + target)));
		tp->jump_tcb.l_paddr = np->jump_tcb.l_paddr;

		tp->getscr[0] = SCR_COPY (1);
		tp->getscr[1] = vtophys (&tp->sval);
		tp->getscr[2] = np->paddr + offsetof (struct ncr_reg, nc_sxfer);
		tp->getscr[3] = SCR_COPY (1);
		tp->getscr[4] = vtophys (&tp->wval);
		tp->getscr[5] = np->paddr + offsetof (struct ncr_reg, nc_scntl3);

		assert (( (offsetof(struct ncr_reg, nc_sxfer) ^
			offsetof(struct tcb    , sval    )) &3) == 0);
		assert (( (offsetof(struct ncr_reg, nc_scntl3) ^
			offsetof(struct tcb    , wval    )) &3) == 0);

		tp->call_lun.l_cmd   = (SCR_CALL);
		tp->call_lun.l_paddr = NCB_SCRIPT_PHYS (np, resel_lun);

		tp->jump_lcb.l_cmd   = (SCR_JUMP);
		tp->jump_lcb.l_paddr = NCB_SCRIPT_PHYS (np, abort);
		np->jump_tcb.l_paddr = vtophys (&tp->jump_tcb);

		ncr_setmaxtags (tp, SCSI_NCR_MAX_TAGS);
	}

	/*
	**	Logic unit control block
	*/
	lp = tp->lp[lun];
	if (!lp) {
		/*
		**	Allocate a lcb
		*/
		lp = (lcb_p) malloc (sizeof (struct lcb), M_DEVBUF, M_NOWAIT);
		if (!lp) return;

		/*
		**	Initialize it
		*/
		bzero (lp, sizeof (*lp));
		lp->jump_lcb.l_cmd   = (SCR_JUMP ^ IFFALSE (DATA (lun)));
		lp->jump_lcb.l_paddr = tp->jump_lcb.l_paddr;

		lp->call_tag.l_cmd   = (SCR_CALL);
		lp->call_tag.l_paddr = NCB_SCRIPT_PHYS (np, resel_tag);

		lp->jump_ccb.l_cmd   = (SCR_JUMP);
		lp->jump_ccb.l_paddr = NCB_SCRIPT_PHYS (np, aborttag);

		lp->actlink = 1;

		/*
		**   Chain into LUN list
		*/
		tp->jump_lcb.l_paddr = vtophys (&lp->jump_lcb);
		tp->lp[lun] = lp;

	}

	/*
	**	Limit possible number of ccbs.
	**
	**	If tagged command queueing is enabled,
	**	can use more than one ccb.
	*/

	if (np->actccbs >= MAX_START-2) return;
	if (lp->actccbs && (lp->actccbs >= lp->reqccbs))
		return;

	/*
	**	Allocate a ccb
	*/
	cp = (ccb_p) malloc (sizeof (struct ccb), M_DEVBUF, M_NOWAIT);

	if (!cp)
		return;

	if (DEBUG_FLAGS & DEBUG_ALLOC) {
		PRINT_ADDR(xp);
		printf ("new ccb @%x.\n", (unsigned) cp);
	}

	/*
	**	Count it
	*/
	lp->actccbs++;
	np->actccbs++;

	/*
	**	Initialize it
	*/
	bzero (cp, sizeof (*cp));

	/*
	**	Fill in physical addresses
	*/

	cp->p_ccb	     = vtophys (cp);

	/*
	**	Chain into reselect list
	*/
	cp->jump_ccb.l_cmd   = SCR_JUMP;
	cp->jump_ccb.l_paddr = lp->jump_ccb.l_paddr;
	lp->jump_ccb.l_paddr = CCB_PHYS (cp, jump_ccb);
	cp->call_tmp.l_cmd   = SCR_CALL;
	cp->call_tmp.l_paddr = NCB_SCRIPT_PHYS (np, resel_tmp);

	/*
	**	Chain into wakeup list
	*/
	cp->link_ccb      = np->ccb.link_ccb;
	np->ccb.link_ccb  = cp;

	/*
	**	Chain into CCB list
	*/
	cp->next_ccb	= lp->next_ccb;
	lp->next_ccb	= cp;
}

/*==========================================================
**
**
**	Announce the number of ccbs/tags to the scsi driver.
**
**
**==========================================================
*/

static void ncr_opennings (ncb_p np, lcb_p lp, struct scsi_xfer * xp)
{
	/*
	**	want to reduce the number ...
	*/
	if (lp->actlink > lp->reqlink) {

		/*
		**	Try to  reduce the count.
		**	We assume to run at splbio ..
		*/
		u_char diff = lp->actlink - lp->reqlink;

		if (!diff) return;

#ifdef __NetBSD__
		if (diff > xp->sc_link->openings)
			diff = xp->sc_link->openings;

		xp->sc_link->openings	-= diff;
#else /* !__NetBSD__ */
		if (diff > xp->sc_link->opennings)
			diff = xp->sc_link->opennings;

		xp->sc_link->opennings	-= diff;
#endif /* __NetBSD__ */
		lp->actlink		-= diff;
		if (DEBUG_FLAGS & DEBUG_TAGS)
			printf ("%s: actlink: diff=%d, new=%d, req=%d\n",
				ncr_name(np), diff, lp->actlink, lp->reqlink);
		return;
	};

	/*
	**	want to increase the number ?
	*/
	if (lp->reqlink > lp->actlink) {
		u_char diff = lp->reqlink - lp->actlink;

#ifdef __NetBSD__
		xp->sc_link->openings	+= diff;
#else /* !__NetBSD__ */
		xp->sc_link->opennings	+= diff;
#endif /* __NetBSD__ */
		lp->actlink		+= diff;
		wakeup ((caddr_t) xp->sc_link);
		if (DEBUG_FLAGS & DEBUG_TAGS)
			printf ("%s: actlink: diff=%d, new=%d, req=%d\n",
				ncr_name(np), diff, lp->actlink, lp->reqlink);
	};
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

	if (vaddr & (NBPG-1)) free -= datalen / NBPG;

	if (free>1)
		while ((chunk * free >= 2 * datalen) && (chunk>=1024))
			chunk /= 2;

	if(DEBUG_FLAGS & DEBUG_SCATTER)
		printf("ncr?:\tscattering virtual=0x%x size=%d chunk=%d.\n",
			(unsigned) vaddr, (unsigned) datalen, (unsigned) chunk);

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
			pnext = (paddr & (~(NBPG - 1))) + NBPG;

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
	register volatile u_long data, *addr;
	/*
	**	ncr registers may NOT be cached.
	**	write 0xffffffff to a read only register area,
	**	and try to read it back.
	*/
	addr = (u_long*) &np->reg->nc_dstat;
	data = 0xffffffff;
	*addr= data;
	data = *addr;
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
	u_long	ncr_rd, ncr_wr, ncr_bk, host_rd, host_wr, pc, err=0;
	int	i;
#ifndef NCR_IOMAPPED
	err |= ncr_regtest (np);
	if (err) return (err);
#endif
	/*
	**	init
	*/
	pc  = NCB_SCRIPT_PHYS (np, snooptest);
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
	if (pc != NCB_SCRIPT_PHYS (np, snoopend)+8) {
		printf ("CACHE TEST FAILED: script execution failed.\n");
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

static	int ncr_delta (struct timeval * from, struct timeval * to)
{
	if (!from->tv_sec) return (-1);
	if (!to  ->tv_sec) return (-2);
	return ( (to->tv_sec  - from->tv_sec  -       2)*1000+
		+(to->tv_usec - from->tv_usec + 2000000)/1000);
}

#define PROFILE  cp->phys.header.stamp
static	void ncb_profile (ncb_p np, ccb_p cp)
{
	int co, da, st, en, di, se, post,work,disc;
	u_long diff;

	PROFILE.end = time;

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
	if (cp->xfer)
	np->profile.num_bytes	+= cp->xfer->datalen;
	np->profile.num_disc	+= diff;
	np->profile.ms_setup	+= co;
	np->profile.ms_data	+= work;
	np->profile.ms_disc	+= disc;
	np->profile.ms_post	+= post;
}
#undef PROFILE

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

#ifndef NEW_SCSICONF

struct table_entry {
	char *	manufacturer;
	char *	model;
	char *	version;
	u_long	info;
};

static struct table_entry device_tab[] =
{
#ifdef NCR_GETCC_WITHMSG
	{"", "", "", QUIRK_NOMSG},
	{"SONY", "SDT-5000", "3.17", QUIRK_NOMSG},
	{"WangDAT", "Model 2600", "01.7", QUIRK_NOMSG},
	{"WangDAT", "Model 3200", "02.2", QUIRK_NOMSG},
	{"WangDAT", "Model 1300", "02.4", QUIRK_NOMSG},
#endif
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
#endif

/*==========================================================
**
**	Determine the ncr's clock frequency.
**	This is important for the negotiation
**	of the synchronous transfer rate.
**
**==========================================================
**
**	Note: we have to return the correct value.
**	THERE IS NO SAVE DEFAULT VALUE.
**
**	We assume that all NCR based boards are delivered
**	with a 40Mhz clock. Because we have to divide
**	by an integer value greater than 3, only clock
**	frequencies of 40Mhz (/4) or 50MHz (/5) permit
**	the FAST-SCSI rate of 10MHz.
**
**----------------------------------------------------------
*/

#ifndef NCR_CLOCK
#	define NCR_CLOCK 40
#endif /* NCR_CLOCK */


static void ncr_getclock (ncb_p np)
{
	u_char	tbl[5] = {6,2,3,4,6};
	u_char	f;
	u_char	ns_clock = (1000/NCR_CLOCK);

	/*
	**	Compute the best value for scntl3.
	*/

	f = (2 * MIN_SYNC_PD - 1) / ns_clock;
	if (!f ) f=1;
	if (f>4) f=4;
	np -> ns_sync = (ns_clock * tbl[f]) / 2;
	np -> rv_scntl3 = f<<4;

	f = (2 * MIN_ASYNC_PD - 1) / ns_clock;
	if (!f ) f=1;
	if (f>4) f=4;
	np -> ns_async = (ns_clock * tbl[f]) / 2;
	np -> rv_scntl3 |= f;
	if (DEBUG_FLAGS & DEBUG_TIMING)
		printf ("%s: sclk=%d async=%d sync=%d (ns) scntl3=0x%x\n",
		ncr_name (np), ns_clock, np->ns_async, np->ns_sync, np->rv_scntl3);
}

/*=========================================================================*/
#endif /* KERNEL */


