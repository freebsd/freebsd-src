/**************************************************************************
**
**  $Id: ncr.c,v 2.0.0.12 94/08/18 23:02:22 wolf Exp $
**
**  Device driver for the   NCR 53C810   PCI-SCSI-Controller.
**
**  386bsd / FreeBSD / NetBSD
**
**-------------------------------------------------------------------------
**
**  Written for 386bsd and FreeBSD by
**	wolf@dentaro.gun.de	Wolfgang Stanglmeier
**	se@mi.Uni-Koeln.de	Stefan Esser
**
**  Ported to NetBSD by
**	mycroft@gnu.ai.mit.edu
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
**-------------------------------------------------------------------------
**
**  $Log:	ncr.c,v $
**  Revision 2.0.0.12  94/08/18  23:02:22  wolf
**  ATN cleared after send of multibyte message.
**  ncr_msgout moved into struct ncb field lastmsg.
**  
**  Revision 2.0.0.11  94/08/11  19:01:46  wolf
**  port to NetBSD.
**  script: start0-label introduced.
**          start sequence changed.
**          badgetcc: extended.
**  chip: stest2.ext bit used.
**  ncr_int_sir(): case 2 debugged: usage of target.
**  
**  Revision 2.0.0.10  94/08/08  22:28:55  wolf
**  debug messages use sc_print_addr.
**  ncr_int_sir() changed.
**  
**  Revision 2.0.0.9  94/08/08  19:45:09  wolf
**  struct script left outside struct ncb.
**  (must fit in one physical page)
**  
**  Revision 2.0.0.7  94/08/04  18:29:07  mycroft
**  Adaption to NetBSD.
**  ncr_unit changed to ncr_name.
**  
**  Revision 2.0.0.6  94/08/01  20:37:34  wolf
**  Tiny cleanup.
**  
**  Revision 2.0.0.5  94/08/01  18:50:40  wolf
**  Write MAX_TARGET and revision before scanning targets.
**  ncr_int_ma: extended comments.
**  
**  Revision 2.0.0.4  94/07/25  18:24:39  wolf
**  Overwrites bogus xp->opennings value of /sys/scsi/cd.c.
**  Annoying "constant overflow" done away.
**  
**  Revision 2.0.0.3  94/07/24  09:02:42  wolf
**  sstat0 used to calculate residue in int_ma.
**  log messages extended.
**  
**  Revision 2.0.0.2  94/07/22  19:04:26  wolf
**  ncr_int_ma: byte count corrected with dfifo.
**  script: dispatch and no_data changed.
**  
**  Revision 2.0.0.1  94/07/19  21:42:02  wolf
**  New debug value: DEBUG_FREEZE
**  M_REJECT log entry includes rejected message.
**  Phase change in command/status/msg phase logged.
**  Timeout exception handler locked.
**  
**  Revision 2.0  94/07/10  15:53:22  wolf
**  FreeBSD release.
**  
**  Revision 1.0  94/06/07  20:02:16  wolf
**  Beta release.
**
***************************************************************************
*/

#ifndef __NetBSD__
#ifdef KERNEL
#include <ncr.h>
#else /* KERNEL */
#define NNCR 1
#endif /* KERNEL */
#endif /* !__NetBSD__ */

#define NCR_VERSION	(2)


/*==========================================================
**
**	Configuration and Debugging
**
**	May be overwritten in <i386/conf/XXXXX>
**
**==========================================================
*/

/*
**    Enable/Disable debug messages.
**    Can be changed at runtime too.
*/

#ifndef SCSI_NCR_DEBUG
#define SCSI_NCR_DEBUG   (0)
#endif /* SCSI_NCR_DEBUG */

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
#define SCSI_NCR_MAX_SYNC   (0)
#endif /* SCSI_NCR_MAX_SYNC */

/*
**    The maximum number of tags per logic unit.
**    Used only for disk devices that support tags.
*/

#ifndef SCSI_NCR_MAX_TAGS
#define SCSI_NCR_MAX_TAGS    (8)
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

#define MAX_TARGET  (7)

/*
**    Number of logic units supported by the driver.
**    n enables logic unit numbers 0..n-1.
**    The common SCSI devices require only
**    one lun, so take 1 as the default.
*/

#define MAX_LUN     (1)

/*
**    The maximum number of jobs scheduled for starting.
**    There should be one slot per target, and one slot
**    for each tag of each target.
*/

#define MAX_START   (20)

/*
**    The maximum number of segments a transfer is split into.
*/

#define MAX_SCATTER (33)

/*
**    The maximum transfer length (should be >= 64k).
**    MUST NOT be greater than (MAX_SCATTER-1) * NBPG.
*/

#define MAX_SIZE  ((MAX_SCATTER-1) * NBPG)

/*
**    Enable some processor/os dependent functions.
*/

#define DIRTY 1

/*
**    Write disk status information to dkstat ?
*/

#define DK  1

/*==========================================================
**
**      Include files
**
**==========================================================
*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>

#ifdef KERNEL
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#ifdef DK
#include <sys/dkstat.h>
#endif /* DK */
#include <vm/vm.h>
#endif /* KERNEL */

#include <i386/pci/ncr_reg.h>

#ifdef __NetBSD__
#include <sys/device.h>
#include <i386/pci/pcivar.h>
#include <i386/pci/pcireg.h>
#else
#include <i386/pci/pci.h>
#include <i386/pci/pci_device.h>
#endif

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>


/*==========================================================
**
**	Debugging tags
**
**==========================================================
*/

#ifdef SCSI_NCR_DEBUG

#define DEBUG_ALLOC    (0x0001)
#define DEBUG_PHASE    (0x0002)
#define DEBUG_POLL     (0x0004)
#define DEBUG_QUEUE    (0x0008)
#define DEBUG_RESULT   (0x0010)
#define DEBUG_SCATTER  (0x0020)
#define DEBUG_SCRIPT   (0x0040)
#define DEBUG_TINY     (0x0080)
#define DEBUG_TIMING   (0x0100)
#define DEBUG_SDTR     (0x0200)
#define DEBUG_TAGS     (0x0400)
#define DEBUG_FREEZE   (0x0800)
#define DEBUG_NODUMP   (0x1000)

int ncr_debug = SCSI_NCR_DEBUG;

#else /* SCSI_NCR_DEBUG */
int ncr_debug = 0;
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

#define INB(r) (np->reg->r)
#define INW(r) (np->reg->r)
#define INL(r) (np->reg->r)

#define OUTB(r, val) np->reg->r = val
#define OUTW(r, val) np->reg->r = val
#define OUTL(r, val) np->reg->r = val

/*==========================================================
**
**	Command control block states.
**
**==========================================================
*/

#define HS_IDLE         (0)
#define HS_BUSY         (1)
#define HS_NEGOTIATE    (2)	/* sync. data transfer    */
#define HS_DISCONNECT   (3)	/* Disconnected by target */

#define HS_COMPLETE     (4)
#define HS_SEL_TIMEOUT  (5)	/* Selection timeout      */
#define HS_RESET        (6)	/* SCSI reset             */
#define HS_ABORTED      (7)	/* Transfer aborted       */
#define HS_TIMEOUT      (8)	/* Software timeout       */
#define HS_FAIL         (9)	/* SCSI or PCI bus errors */
#define HS_UNEXPECTED  (10)	/* Unexpected disconnect  */

/*==========================================================
**
**	Misc.
**
**==========================================================
*/

#define ILLEGAL_ADDR	(0xefffffff)
#define CCB_MAGIC	(0xf2691ad2)

/*==========================================================
**
**	Capability bits in Inquire response byte #7.
**
**==========================================================
*/

#define	INQ7_SYNC	(0x10)
#define	INQ7_QUEUE	(0x02)

/*==========================================================
**
**	OS dependencies.
**
**==========================================================
*/

#ifndef __FreeBSD__
#ifndef __NetBSD__
	#define	ANCIENT
#endif /*__NetBSD__*/
#endif /*__FreeBSD__*/

#ifdef ANCIENT
	#define LUN       lu
	#define TARGET    targ
	#define PRINT_ADDR(xp) printf ("ncr0: targ %d lun %d ",xp->targ,xp->lu)
	#define INT32     int
	#define U_INT32   long
	#define TIMEOUT
#else /* !ANCIENT */
	#define LUN       sc_link->lun
	#define TARGET    sc_link->target
	#define PRINT_ADDR(xp) sc_print_addr(xp->sc_link)
#ifdef __NetBSD__
	#define INT32     int
	#define U_INT32   u_int
	#define TIMEOUT   (void*)
#else  /*__NetBSD__*/
	#define INT32     int32
	#define U_INT32   u_int32
	#define TIMEOUT   (timeout_func_t)
#endif /*__NetBSD__*/
#endif /* ANCIENT */

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

/*==========================================================
**
**	Access to fields of structs.
**
**==========================================================
*/

#define	offsetof(type, member)	((size_t)(&((type *)0)->member))

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
**      Declaration of structs:		TARGET control block
**
**==========================================================
*/

struct tcb {
	/*
	**	during reselection the ncr jumps to this point
	**	with SFBR set to the encoded TARGET number
	**	with bit 7 set.
	**	if it's not this target, jump to the next.
	**
	**	JUMP  IF (SFBR != #TARGET#)
	**	@(next tcb)
	*/

	struct link   jump_tcb;

	/*
	**	load the actual synchronous mode
	**	for this target to the sxfer register
	**
	**	SCR_COPY (1);
	**	@(sval field of this tcb)
	**	@(sxfer register)
	*/

	ncrcmd	getscr[3];

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

	/*
	**	negotiation of synch transfer and tagged commands
	*/

	u_short	period;
	u_char	_1;
	u_char	sval;
	u_char	minsync;
	u_char	maxoffs;

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
**      Declaration of structs:		LUN control block
**
**==========================================================
*/

struct lcb {
	/*
	**	during reselection the ncr jumps to this point
	**	with SFBR set to the "Identify" message.
	**	if it's not this lun, jump to the next.
	**
	**	JUMP  IF (SFBR == #LUN#)
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
	*/

	u_long		savep;

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

#define  host_status   phys.header.status[0]
#define  scsi_status   phys.header.status[1]
#define  scs2_status   phys.header.status[2]
#define  sync_status   phys.header.status[3]
#define  parity_errs   phys.header.status[4]
};

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

	struct head        header;

	/*
	**	Table data for Script
	*/

	struct scr_tblsel  select;
	struct scr_tblmove smsg  ;
	struct scr_tblmove smsg2 ;
	struct scr_tblmove cmd   ;
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

	struct scsi_xfer        *xfer;

#ifdef ANCIENT
	/*
	**	We copy the SCSI command, because it
	**	may be volatile (on the stack).
	**
	*/
	struct scsi_generic	cmd;
#endif /* ANCIENT */

	/*
	**	We prepare a message to be sent after selection,
	**	and a second one to be sent after getcc selection.
	**      Contents are IDENTIFY and SIMPLE_TAG.
	**	And sdtr .. (if negotiating sync transfers)
	*/

	u_char			scsi_smsg [8];
	u_char			scsi_smsg2[8];

	/*
	**	Lock this ccb.
	**	Flag is used while looking for a free ccb.
	*/

	u_long			magic;
	u_long			tlimit;

	/*
	**	All ccbs of one hostadapter are linked.
	*/

	ccb_p		link_ccb;

	/*
	**	All ccbs of one target/lun are linked.
	*/

	ccb_p		next_ccb;

	/*
	**	Tag for this transfer.
	**	It's patched into jump_ccb.
	**	If it's not zero, a SIMPLE_TAG
	**	message is included in smsg.
	*/

	u_char			tag;
};

/*==========================================================
**
**      Declaration of structs:     NCR device descriptor
**
**==========================================================
*/

struct ncb {
#ifdef __NetBSD__
	struct device sc_dev;
	struct intrhand sc_ih;
#endif

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
	u_long		p_script;

	/*
	**	The SCSI address of the host adapter.
	*/
	u_char          myaddr;

	/*
	**	timing parameters
	*/
	u_char		ns_async;
	u_char		ns_sync;
	u_char		rv_scntl3;

#ifndef ANCIENT
	/*-----------------------------------------------
	**	Link to the generic SCSI driver
	**-----------------------------------------------
	*/

	struct scsi_link        sc_link;
#endif /* ANCIENT */

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
#ifndef __NetBSD__
	u_short		imask;
	u_short		mcount;
#endif

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

	/*-----------------------------------------------
	**	Working areas
	**-----------------------------------------------
	**
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

	u_char          msgout[8];
	u_char          msgin [8];
	u_long		lastmsg;

	/*
	**	Buffer for STATUS_IN phase.
	*/

	u_char		scratch;
	u_char		lock; /* @DEBUG@ */
};

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
	ncrcmd	loadpos		[ 24];
	ncrcmd	prepare2	[ 20];
	ncrcmd	setmsg		[  5];
	ncrcmd  clrack		[  6];
	ncrcmd  dispatch	[ 22];
	ncrcmd	no_data		[ 19];
	ncrcmd  checkatn        [ 16];
	ncrcmd  command		[ 15];
	ncrcmd  status		[ 25];
	ncrcmd  msg_in		[ 22];
	ncrcmd  msg_bad		[  6];
	ncrcmd  msg_parity	[ 12];
	ncrcmd	msg_reject	[  6];
	ncrcmd  msg_extended	[ 34];
	ncrcmd	msg_sdtr	[ 41];
	ncrcmd  complete	[  6];
	ncrcmd	cleanup		[ 12];
	ncrcmd	savepos		[ 11];
	ncrcmd	signal		[ 10];
	ncrcmd  save_dp         [  5];
	ncrcmd  restore_dp	[  5];
	ncrcmd  disconnect	[ 21];
	ncrcmd	msg_out		[  9];
	ncrcmd	msg_out_done	[  7];
	ncrcmd	msg_out_abort	[ 10];
	ncrcmd  getcc		[  4];
	ncrcmd  getcc1		[  5];
	ncrcmd	getcc2		[ 35];
	ncrcmd  badgetcc	[  6];
	ncrcmd	reselect	[ 12];
	ncrcmd	reselect2	[  6];
	ncrcmd	resel_tmp	[  5];
	ncrcmd  resel_lun	[ 18];
	ncrcmd	resel_tag	[ 24];
	ncrcmd  data_in		[MAX_SCATTER * 4 + 7];
	ncrcmd  data_out	[MAX_SCATTER * 4 + 7];
	ncrcmd	aborttag	[  4];
	ncrcmd	abort		[ 20];
};

/*==========================================================
**
**
**      Function Headers.
**
**
**==========================================================
*/

#ifdef KERNEL
#ifdef ANCIENT
extern	int	splbio(void);
extern	void	splx(int level);
extern	int	wakeup(void* channel);
extern	int	tsleep();
extern	int	DELAY();
extern	int	scsi_attachdevs();
extern	void	timeout();
extern	void	untimeout();
#endif /* ANCIENT */

static	void	ncr_alloc_ccb	(ncb_p np, struct scsi_xfer * xp);
static	void	ncr_complete	(ncb_p np, ccb_p cp);
static	int	ncr_delta	(struct timeval * from, struct timeval * to);
static	void	ncr_exception	(ncb_p np);
static	void	ncr_free_ccb	(ncb_p np, ccb_p cp, int flags);
static	void	ncr_getclock	(ncb_p np);
static	ccb_p ncr_get_ccb	(ncb_p np, u_long flags, u_long t,u_long l);
static  U_INT32 ncr_info	(int unit);
static	void	ncr_init	(ncb_p np, char * msg, u_long code);
static	void	ncr_int_ma	(ncb_p np);
static	void	ncr_int_sir	(ncb_p np);
static  void    ncr_int_sto     (ncb_p np);
static	void	ncr_min_phys	(struct buf *bp);
static	void	ncr_opennings	(ncb_p np, lcb_p lp, struct scsi_xfer * xp);
static	void	ncb_profile	(ncb_p np, ccb_p cp);
static	void	ncr_script_copy_and_bind
				(struct script * script, ncb_p np);
static  void    ncr_script_fill (struct script * scr);
static	u_long	ncr_scatter	(struct dsb* phys,u_long vaddr,u_long datalen);
static	void	ncr_setmaxtags	(tcb_p tp, u_long usrtags);
static	void	ncr_setsync	(ncb_p np, ccb_p cp, u_char sxfer);
static	void	ncr_settags	(tcb_p tp, lcb_p lp);
static	INT32	ncr_start	(struct scsi_xfer *xp);
static	void	ncr_timeout	(ncb_p np);
static	void	ncr_usercmd	(ncb_p np);
static  void    ncr_wakeup      (ncb_p np, u_long code);

#ifdef __NetBSD__
static	int	ncb_probe	(struct device *, struct device *, void *);
static	void	ncr_attach	(struct device *, struct device *, void *);
static	int	ncr_intr	(ncb_p np);
#else
static  int     ncb_probe       (pcici_t config_id);
static	int	ncr_attach	(pcici_t config_id);
static  int	ncr_intr        (int dev);
#endif

/*==========================================================
**
**
**	Access to processor ports.
**
**
**==========================================================
*/

#ifdef DIRTY

#ifdef __NetBSD__
#include <i386/include/cpufunc.h>
#include <i386/include/pio.h>
#include <i386/isa/isareg.h>
#define	DELAY(x)	delay(x)
#else /* !__NetBSD__ */

#include <i386/isa/isa.h>
#ifdef ANCIENT
/*
**	Doch das ist alles nur geklaut ..
**	aus:  386bsd:/sys/i386/include/pio.h
**
** Mach Operating System
** Copyright (c) 1990 Carnegie-Mellon University
** All rights reserved.  The CMU software License Agreement specifies
** the terms and conditions for use and redistribution.
*/

#undef inb
#define inb(port) \
({ unsigned char data; \
	__asm __volatile("inb %1, %0": "=a" (data): "d" ((u_short)(port))); \
	data; })

#undef outb
#define outb(port, data) \
{__asm __volatile("outb %0, %1"::"a" ((u_char)(data)), "d" ((u_short)(port)));}

#define disable_intr() \
{__asm __volatile("cli");}

#define enable_intr() \
{__asm __volatile("sti");}
#endif /* ANCIENT */

/*------------------------------------------------------------------
**
**	getirr: get a bit vector of the pending interrupts.
**
**	NOTE: this is HIGHLY hardware dependant :-(
**
**------------------------------------------------------------------
*/


static	u_long	getirr (void)
{
	u_long	mask;

	disable_intr();

	outb (IO_ICU2, 0x0a);
	mask = inb (IO_ICU2);
	outb (IO_ICU2, 0x0b);

	mask <<= 8;

	outb (IO_ICU1, 0x0a);
	mask|= inb (IO_ICU1);
	outb (IO_ICU1, 0x0b);

	enable_intr();

	return (mask);
}

#endif /* __NetBSD__ */
#else /* DIRTY */
	#define getirr()  (0)
#endif /* DIRTY */
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
	"\n$Id: ncr.c,v 2.0.0.12 94/08/18 23:02:22 wolf Exp $\n"
	"Copyright (c) 1994, Wolfgang Stanglmeier\n";

u_long	ncr_version = NCR_VERSION
	+ (u_long) sizeof (struct ncb)
	* (u_long) sizeof (struct ccb)
	* (u_long) sizeof (struct lcb)
	* (u_long) sizeof (struct tcb);

#ifdef KERNEL

#ifndef __NetBSD__
u_long		ncr_units;
u_long		nncr=NNCR;
ncb_p		ncrp [NNCR];
#endif

/*
**	SCSI cmd to get the SCSI sense data
*/

static u_char rs_cmd  [6] =
	{ 0x03, 0, 0, 0, sizeof (struct scsi_sense_data), 0 };

/*==========================================================
**
**
**      Global static data:	auto configure
**
**
**==========================================================
*/

#ifdef __NetBSD__

struct	cfdriver ncrcd = {
	NULL, "ncr", ncb_probe, ncr_attach, DV_DISK, sizeof(struct ncb)
};

#else /* !__NetBSD__ */

struct	pci_driver ncrdevice = {
	ncb_probe,
	ncr_attach,
	0x00011000ul,
	"ncr",
	"ncr 53c810 scsi",
	ncr_intr
};

#endif /* !__NetBSD__ */


#ifndef ANCIENT
struct scsi_adapter ncr_switch =
{
	ncr_start,
	ncr_min_phys,
	0,
	0,
	ncr_info,
	"ncr",
};

struct scsi_device ncr_dev =
{
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
	"ncr",
};
#else /* ANCIENT */
struct scsi_switch ncr_switch =
{
	ncr_start,
	ncr_min_phys,
	0,
	0,
	ncr_info,
	0,0,0
};
#endif /* ANCIENT */

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
**
**
**	PADDR generates a reference to another part of the script.
**	REG   generates a reference to a script processor register.
**
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
		1,
}/*-------------------------< START1 >----------------------*/,{
	/*
	**	Hook for stalled start queue.
	**	Will be patched to IFTRUE by the interrupt handler.
	*/
	SCR_INT ^ IFFALSE (0),
		7,
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
	**	If the entry is scheduled to be executed,
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
	**	(Target mode is left as an exercise for the student)
	*/

	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (scr0, 0xff),
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
	**	(and the M_X_SDTR message)
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
	/*
	**	Set carry according to host_status
	*/
	SCR_CLR (SCR_CARRY),
		0,
	SCR_FROM_REG (scr0),
		0,
	SCR_JUMPR ^ IFFALSE (DATA (HS_NEGOTIATE)),
		16,
	SCR_LOAD_REG (scr0, HS_BUSY),
		0,
	SCR_SET (SCR_CARRY),
		0,

}/*-------------------------< PREPARE2 >---------------------*/,{
	/*
	**	<Carry set iff SDTM message sent>
	**
	**      Load the synchronous mode register
	*/
	SCR_FROM_REG (scr3),
		0,
	SCR_TO_REG (sxfer),
		0,
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
	**	If M_X_SDTR sent, but no MSG_IN phase, ...
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR (msg_in),
	SCR_JUMP ^ IFFALSE (CARRYSET),
		PADDR (dispatch),
	/*
	**	no answer is an answer, too.
	*/
	SCR_INT,
		3,
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
	**	Terminate sdtr mode.
	**	Terminate possible pending message phase.
	*/
	SCR_FROM_REG (scr0),
		0,
	SCR_LOAD_REG (scr0, HS_BUSY),
		0,
	SCR_CLR (SCR_ACK | SCR_CARRY),
		0,

}/*-----------------------< DISPATCH >----------------------*/,{
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
	**      Prepare an abort message and set ATN.
	*/
	SCR_LOAD_REG (scratcha, M_ABORT),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_SET (SCR_ATN),
		0,
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
	SCR_COPY (1),
		NADDR (header.status[4]),
		RADDR (scratcha),
	SCR_REG_REG (scratcha, SCR_ADD, 0x01),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (header.status[4]),
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
	SCR_FROM_REG (scr1),
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
/*>>>*/	SCR_MOVE_ABS (6) ^ SCR_COMMAND,
		(ncrcmd) &rs_cmd,
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
	SCR_FROM_REG (scr1),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (DATA (S_CHECK_COND)),
		32,
	/*
	**	get the status
	*/
	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
	/*
	**	Save status to scs2_status.
	**	Mark as complete.
	**	And wait for disconnect.
	*/
	SCR_TO_REG (scr2),
		0,
	SCR_LOAD_REG (scr0, HS_COMPLETE),
		0,
	SCR_JUMP,
		PADDR (checkatn),
	/*
	**	If it was no GETCC transfer,
	**	save the status to scsi_status.
	*/
/*>>>*/	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
	SCR_TO_REG (scr1),
		0,
	/*
	**	if it was no check condition ...
	*/
	SCR_JUMP ^ IFTRUE (DATA (S_CHECK_COND)),
		PADDR (checkatn),
	/*
	**	... mark as complete.
	*/
	SCR_LOAD_REG (scr0, HS_COMPLETE),
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
	SCR_JUMP ^ IFTRUE (DATA (M_REJECT)),
		PADDR (msg_reject),
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
		6,
	SCR_LOAD_REG (scratcha, M_REJECT),
		0,
	SCR_JUMP,
		PADDR (setmsg),

}/*-------------------------< MSG_PARITY >---------------*/,{
	/*
	**	count it
	*/
	SCR_COPY (1),
		NADDR (header.status[4]),
		RADDR (scratcha),
	SCR_REG_REG (scratcha, SCR_ADD, 0x01),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (header.status[4]),
	/*
	**	send a "message parity error" message.
	*/
	SCR_LOAD_REG (scratcha, M_PARITY),
		0,
	SCR_JUMP,
		PADDR (setmsg),
}/*-------------------------< MSG_REJECT >---------------*/,{
	/*
	**	If a M_X_SDTR message was sent,
	**	negotiate synchronous mode.
	*/
	SCR_INT ^ IFTRUE (CARRYSET),
		3,
	/*
	**	make host log this message
	*/
	SCR_INT ^ IFFALSE (CARRYSET),
		5,
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
	SCR_JUMP ^ IFFALSE (DATA (3)),
		PADDR (msg_bad),
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
	SCR_JUMP ^ IFTRUE (DATA (M_X_SDTR)),
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
	SCR_INT ^ IFTRUE (CARRYSET),
		3,
	SCR_INT ^ IFFALSE (CARRYSET),
		4,
	/*
	**	let the target fetch our answer.
	*/
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,

/*<<<*/	SCR_JUMPR ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		16,
	SCR_INT,
		4,
	SCR_JUMP,
		PADDR (dispatch),
	/*
	**	Sent the M_X_SDTR
	*/
/*>>>*/	SCR_MOVE_ABS (5) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		-16,
	SCR_CLR (SCR_ATN),
		0,
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	/*
	**	If rejected, cancel sync transfer.
	*/
	SCR_JUMP ^ IFFALSE (IF (SCR_MSG_IN)),
		PADDR (msg_out_done),
	SCR_FROM_REG (sbdl),
		0,
	SCR_INT ^ IFTRUE (DATA (M_REJECT)),
		4,
	SCR_JUMP,
		PADDR (msg_out_done),

}/*-------------------------< COMPLETE >-----------------*/,{
	/*
	**	Complete message.
	**
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
	SCR_CLR (SCR_ACK),
		0,
	/*
	**	... and wait for the disconnect.
	*/
	SCR_WAIT_DISC,
		0,
}/*-------------------------< CLEANUP >-------------------*/,{
	/*
	**      dsa:    Pointer to ccb
	**              or xxxxxxFF (no ccb)
	**
	**      scr0:   Host-Status (<>0!)
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
		PADDR (savepos),
	SCR_COPY (sizeof (struct head)),
		NADDR (header),
}/*-------------------------< SAVEPOS >---------------------*/,{
		0,

	/*
	**	If command resulted in "check condition"
	**	status and is not yet completed,
	**	try to get the condition code.
	*/
	SCR_FROM_REG (scr0),
		0,
/*<<<*/	SCR_JUMPR ^ IFFALSE (MASK (0x00, 0xfc)),
		16,
	SCR_FROM_REG (scr1),
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
	SCR_FROM_REG (scr1),
		0,
	SCR_INT ^ IFTRUE (DATA (S_QUEUE_FULL)),
		8,
	/*
	**	if job completed ...
	*/
	SCR_FROM_REG (scr0),
		0,
	/*
	**	... signal completion to the host
	*/
	SCR_INT_FLY ^ IFFALSE (MASK (0x00, 0xfc)),
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
	**	Disable the "unexpected disconnect" feature.
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK),
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
	SCR_LOAD_REG (scr0, HS_DISCONNECT),
		0,
	SCR_JUMP,
		PADDR (cleanup),

}/*-------------------------< MSG_OUT >-------------------*/,{
	/*
	**	The target requests a message.
	**	First remove ATN so the target will
	**	not continue fetching messages.
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
	SCR_CLR (SCR_ACK),
		0,
	SCR_WAIT_DISC,
		0,
	/*
	**	... and set the status to "ABORTED"
	*/
	SCR_LOAD_REG (scr0, HS_ABORTED),
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
	/*
	**	Then try to connect to the target.
	**	If we are reselected, special treatment
	**	of the current job is required before
	**	accepting the reselection.
	*/
	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR(badgetcc),
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_MSG_IN)),
		0,
	SCR_FROM_REG (sdid),
		0,
	SCR_TO_REG (ctest0),
		0,
	/*
	**	and send the IDENTIFY and a SDTM message.
	*/
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg2),
	SCR_JUMPR ^ IFTRUE ( WHEN (SCR_MSG_OUT) ),
		-16,
	SCR_CLR (SCR_ATN),
		0,
	SCR_COPY (1),
		RADDR (sfbr),
		NADDR (lastmsg),
	/*
	**	Handle synch negotiation.
	*/
	SCR_SET (SCR_CARRY),
		0,
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
		2,
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
	**	Mask out the LUN.
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
	SCR_WAIT_DISC,
		0,
	SCR_JUMP,
		PADDR (start),
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
**	Bind the script to its physical address.
**
**
**==========================================================
*/

#ifdef __NetBSD__

#define	ncr_name(np)	(np->sc_dev.dv_xname)

#else /* !__NetBSD__ */

static char *ncr_name (ncb_p np)
{
	static char name[10];
	int idx;

	for (idx = 0; idx < NNCR; idx++)
		if (ncrp[idx] == np) {
			sprintf(name, "ncr%d", idx);
			return (name);
		}
	return ("ncr?");
}

#endif

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

	np->script = (struct script *)
	    malloc (sizeof (struct script), M_DEVBUF, M_WAITOK);
	np->p_script = vtophys(np->script);

	src = script->start;
	dst = np->script->start;

	start = src;
	end = src + (sizeof(struct script) / 4);
	
	while (src < end) {

		*dst++ = opcode = *src++;

		/*
		**	If we forget to change the length
		**	in struct script, a field will be
		**	padded with 0. This is an illegal
		**	command.
		*/

		if (opcode == 0)
			printf ("%s: ERROR0 IN SCRIPT at %d.\n",
				ncr_name(np), src-start-1);

#ifdef SCSI_NCR_DEBUG
		if (ncr_debug & DEBUG_SCRIPT)
			printf ("%x:  <%x>\n",
				(u_long)(src-1), opcode);
#endif /* SCSI_NCR_DEBUG */

		/*
		**	We don't have to decode ALL commands
		*/
		switch (opcode >> 28) {

		case 0xc:
			/*
			**	COPY has TWO arguments.
			*/
			relocs = 2;
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
					new = (old & ~RELOC_MASK) + vtophys(np->script);
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
	if (bp->b_bcount > MAX_SIZE) bp->b_bcount = MAX_SIZE;
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
ncb_probe(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cfdata *cf = self->dv_cfdata;
	struct pci_attach_args *pa = aux;

	if (!pci_targmatch(cf, pa))
		return 0;
	if (pa->pa_id != 0x00011000)
		return 0;

	return 1;
}

#else /* !__NetBSD__ */

static	int ncb_probe(pcici_t config_id)
{
	if (ncr_units >= NNCR) return (-1);
	return (ncr_units);
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
	 * XXX
	 * Perhaps try to figure what which model chip it is and print that
	 * out.
	 */
	printf("\n");

	/*
	**	Try to map the controller chip to
	**	virtual and physical memory.
	*/

	retval = pci_map_mem(pa->pa_tag, 0x14, &np->vaddr, &np->paddr);
	if (retval)
		return;

	np->sc_ih.ih_fun = ncr_intr;
	np->sc_ih.ih_arg = np;
	np->sc_ih.ih_level = IPL_BIO;

	retval = pci_map_int(pa->pa_tag, &np->sc_ih);
	if (retval)
		return;

#else /* !__NetBSD__ */

static	int ncr_attach (pcici_t config_id)
{
	int retval;
	ncb_p np = ncrp[ncr_units];

	/*
	**	allocate structure
	*/

	if (!np) {
		np = (ncb_p) malloc (sizeof (struct ncb),
				M_DEVBUF, M_NOWAIT);
		if (!np) return (0);
		ncrp[ncr_units]=np;
	}

	/*
	**	initialize structure.
	*/

	bzero (np, sizeof (*np));

	/*
	**	Try to map the controller chip to
	**	virtual and physical memory.
	*/

	retval = pci_map_mem (config_id, 0x14, &np->vaddr, &np->paddr);

	if (retval) {
		printf ("%s: pci_map_mem failed.\n", ncr_name (np));
		return (retval);
	};

#endif /* !__NetBSD__ */

	/*
	**	Patch script to physical addresses
	*/

	ncr_script_fill (&script0);
	ncr_script_copy_and_bind (&script0, np);

	/*
	**	init data structure
	*/

	np -> jump_tcb.l_cmd   = SCR_JUMP ;
	np -> jump_tcb.l_paddr = vtophys (&np->script->abort);

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
	OUTB (nc_istat,  0   );

	/*
	**	After SCSI devices have been opened, we cannot
	**	reset the bus safely, so we do it here.
	**	Interrupt handler does the real work.
	*/

	OUTB (nc_scntl1, CRST);

	/*
	**	process the reset exception,
	**	if interrupts are not enabled yet.
	*/
	ncr_exception (np);

#ifdef ANCIENT
	printf ("%s: waiting for scsi devices to settle\n",
		ncr_name (np));
	DELAY (1000000);
#endif
	printf ("%s scanning for targets 0..%d ($Revision: 2.0.0.12 $%x$)\n",
		ncr_name (np), MAX_TARGET-1, SCSI_NCR_DEBUG);

	/*
	**	Now let the generic SCSI driver
	**	look for the SCSI devices on the bus ..
	*/

#ifndef ANCIENT
#ifdef __NetBSD__
	np->sc_link.adapter_softc = np;
#else /* !__NetBSD__ */
	np->sc_link.adapter_unit = ncr_units;
#endif /* !__NetBSD__ */
	np->sc_link.adapter_targ = np->myaddr;
	np->sc_link.adapter      = &ncr_switch;
	np->sc_link.device       = &ncr_dev;

#ifdef __NetBSD__
	config_found(self, &np->sc_link, ncr_print);
#else /* !__NetBSD__ */
	scsi_attachdevs (&np->sc_link);
#endif /* !__NetBSD__ */
#else /* ANCIENT */
	scsi_attachdevs (ncr_units, np->myaddr, &ncr_switch);
#endif /* ANCIENT */

	/*
	**	start the timeout daemon
	*/
	ncr_timeout (np);
	np->lasttime=0;

	/*
	**  Done.
	*/

#ifndef __NetBSD__
	ncr_units++;
	return(1);
#endif
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
ncr_intr(np)
	ncb_p np;
{
	int n = 0;

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_TINY) printf ("[");
#endif /* SCSI_NCR_DEBUG */

#else /* !__NetBSD__ */

static int ncr_intr (int dev)
{
	ncb_p np;
	int n=0;

	/*
	**	Sanity check
	*/

	if (dev >= ncr_units) return (0);

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_TINY) printf ("[");
#endif /* SCSI_NCR_DEBUG */

	assert (dev<NNCR);

	/*
	**	Repeat until no outstanding ints
	*/

	np = ncrp[dev];

#endif /* !__NetBSD__ */

	while (INB(nc_istat) & (INTF|SIP|DIP)) {
		ncr_exception (np);
		n=1;
	};

	/*
	**	Switch timeout function to slow.
	*/

	if (n) np->ticks = 100;

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_TINY) printf ("]\n");
#endif /* SCSI_NCR_DEBUG */

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
#ifndef ANCIENT
#ifdef __NetBSD__
	ncb_p np  = xp->sc_link->adapter_softc;
#else /*__NetBSD__*/
	ncb_p np  = ncrp[xp->sc_link->adapter_unit];
#endif/*__NetBSD__*/
#else /* ANCIENT */
	ncb_p np  = ncrp[xp->adapter];
#endif /* ANCIENT */

	struct scsi_generic * cmd = xp->cmd;
	ccb_p cp;
	lcb_p lp;
	tcb_p tp;

	int	i, oldspl, flags = xp->flags;
	u_char	ptr, startcode, idmsg;
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
		return(COMPLETE);
	};

	/*---------------------------------------------
	**
	**      Some shortcuts ...
	**
	**---------------------------------------------
	*/

	if ((xp->TARGET == np->myaddr    ) ||
		(xp->TARGET >= MAX_TARGET) ||
		(xp->LUN    >= MAX_LUN   ) ||
		(flags    & SCSI_DATA_UIO)) {
		xp->error = XS_DRIVER_STUFFUP;
		return(HAD_ERROR);
	};

#ifdef ANCIENT
	/*---------------------------------------------
	**   Ancient version of <sys/scsi/sd.c>
	**   doesn't set the DATA_IN/DATA_OUT bits.
	**   So we have to fix it ..
	**---------------------------------------------
	*/

	switch (cmd->opcode) {
	case 0x1a:  /* MODE_SENSE    */
	case 0x25:  /* READ_CAPACITY */
	case 0x28:  /* READ_BIG (10) */
		xp->flags |= SCSI_DATA_IN;
		break;
	case 0x2a:  /* WRITE_BIG(10) */
		xp->flags |= SCSI_DATA_OUT;
		break;
	};
#endif /* ANCIENT */

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_TINY) {
		PRINT_ADDR(xp);
		printf ("CMD=%x F=%x L=%x ", cmd->opcode,
			xp->flags, xp->datalen);
	}
#endif /* SCSI_NCR_DEBUG */

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
	**	Assign a ccb
	**
	**----------------------------------------------------
	*/

	if (!(cp=ncr_get_ccb (np, flags, xp->TARGET, xp->LUN))) {
		printf ("%s: no ccb.\n", ncr_name (np));
		xp->error = XS_DRIVER_STUFFUP;
		return(TRY_AGAIN_LATER);
	};

	/*---------------------------------------------------
	**
	**	timestamp
	**
	**----------------------------------------------------
	*/

	bzero (&cp->phys.header.stamp, sizeof (struct tstamp));
	cp->phys.header.stamp.start = time;

	/*---------------------------------------------------
	**
	**	sync negotiation required?
	**
	**----------------------------------------------------
	*/

	tp = &np->target[xp->TARGET];

	if  ((cmd->opcode!=0x12) && (tp->period)) {

		startcode = HS_BUSY;

	} else if (!(tp->inqdata[7] & INQ7_SYNC)) {

		tp->minsync = 255;
		tp->maxoffs =  8 ;
		tp->period  =0xffff;
		startcode = HS_BUSY;

	} else {
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
		
		startcode = HS_NEGOTIATE;
	};

	/*---------------------------------------------------
	**
	**	choose a new tag ...
	**
	**----------------------------------------------------
	*/

	if ((lp = tp->lp[xp->LUN]) && (lp->usetags)) {
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
			PRINT_ADDR(xp);
			printf ("using tag #%d.\n", cp->tag);
		};
	} else {
		cp->tag=0;
#if !defined(ANCIENT) && !defined(__NetBSD__)
		/*
		** @GENSCSI@	Bug in "/sys/scsi/cd.c"
		**
		**	/sys/scsi/cd.c initializes opennings with 2.
		**	Our info value of 1 is not respected.
		*/
		if (xp->sc_link && xp->sc_link->opennings) {
			PRINT_ADDR(xp);
			printf ("opennings set to 0.\n");
			xp->sc_link->opennings = 0;
		};
#endif
	};

	/*----------------------------------------------------
	**
	**	Build the identify / tag / sdtr message
	**
	**----------------------------------------------------
	*/

	idmsg = (cp==&np->ccb ? 0x80 : 0xc0) | xp->LUN;

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
		**	can be overwritten by ncrstat
		*/
		switch (np->order) {

		case M_SIMPLE_TAG:
			cp -> scsi_smsg [msglen] = M_SIMPLE_TAG;
			break;

		case M_ORDERED_TAG:
			cp -> scsi_smsg [msglen] = M_ORDERED_TAG;
			break;
		};
		msglen++;

		cp -> scsi_smsg [msglen++] = cp -> tag;
	}
	if (startcode==HS_NEGOTIATE) {
		cp -> scsi_smsg [msglen++] = M_EXTENDED;
		cp -> scsi_smsg [msglen++] = 3;
		cp -> scsi_smsg [msglen++] = M_X_SDTR;
		cp -> scsi_smsg [msglen++] = np->target[xp->TARGET].minsync;
		cp -> scsi_smsg [msglen++] = np->target[xp->TARGET].maxoffs;
	};

	/*----------------------------------------------------
	**
	**	Build the identify / sdtr message for getcc
	**
	**----------------------------------------------------
	*/

	cp -> scsi_smsg2 [0] = idmsg;
	msglen2 = 1;
	if (np->target[xp->TARGET].inqdata[7]&INQ7_SYNC) {
		cp -> scsi_smsg2 [1] = M_EXTENDED;
		cp -> scsi_smsg2 [2] = 3;
		cp -> scsi_smsg2 [3] = M_X_SDTR;
		cp -> scsi_smsg2 [4] = np->target[xp->TARGET].minsync;
		cp -> scsi_smsg2 [5] = np->target[xp->TARGET].maxoffs;
		msglen2 = 6;
	};

	/*----------------------------------------------------
	**
	**	Build the data descriptors
	**
	**----------------------------------------------------
	*/

	if (ncr_scatter (&cp->phys,
			(vm_offset_t) xp->data,
			(vm_size_t) xp->datalen)) {
		xp->error = XS_DRIVER_STUFFUP;
		ncr_free_ccb(np, cp, flags);
		return(HAD_ERROR);
	};

	/*----------------------------------------------------
	**
	**	Set the SAVED_POINTER.
	**
	**----------------------------------------------------
	*/

	if (flags & SCSI_DATA_IN) {
		cp->phys.header.savep	= vtophys (&np->script->data_in);
	} else if (flags & SCSI_DATA_OUT) {
		cp->phys.header.savep	= vtophys (&np->script->data_out);
	} else {
		cp->phys.header.savep	= vtophys (&np->script->no_data);
	};

	/*----------------------------------------------------
	**
	**	fill ccb
	**
	**----------------------------------------------------
	*/

	/*
	**	physical -> virtual translation
	*/
	cp->phys.header.cp		= cp;
	/*
	**	Generic SCSI command
	*/
	cp->xfer			= xp;
	/*
	**	Startqueue
	*/
	cp->phys.header.launch.l_paddr	= vtophys (&np->script->select);
	cp->phys.header.launch.l_cmd	= SCR_JUMP;
	/*
	**	select
	*/
	cp->phys.select.sel_id		= xp->TARGET;
	cp->phys.select.sel_scntl3	= np->rv_scntl3;
	cp->phys.select.sel_sxfer	= np->target[xp->TARGET].sval;
	/*
	**	message
	*/
	cp->phys.smsg.addr		= vtophys (&cp->scsi_smsg );
	cp->phys.smsg.size		= msglen;
	cp->phys.smsg2.addr		= vtophys (&cp->scsi_smsg2);
	cp->phys.smsg2.size		= msglen2;
	/*
	**	command
	*/
#ifdef ANCIENT
	bcopy (cmd, &cp->cmd, sizeof (cp->cmd));
	cp->phys.cmd.addr		= vtophys (&cp->cmd);
#else /* ANCIENT */
	cp->phys.cmd.addr		= vtophys (cmd);
#endif /* ANCIENT */
	cp->phys.cmd.size		= xp->cmdlen;
	/*
	**	sense data
	*/
	cp->phys.sense.addr		= vtophys (&cp->xfer->sense);
	cp->phys.sense.size		= sizeof(struct scsi_sense_data);
	/*
	**	status
	*/
	cp->scs2_status			= S_ILLEGAL;
	cp->scsi_status			= S_ILLEGAL;
	cp->sync_status			= np->target[xp->TARGET].sval;
	cp->host_status			= startcode;
	cp->parity_errs			= 0;

	/*----------------------------------------------------
	**
	**	Critical region: starting this job.
	**
	**----------------------------------------------------
	*/

	oldspl = 0; /* for the sake of gcc */
	if (!(flags & SCSI_NOMASK)) oldspl = splbio();
	np->lock++;

	/*
	**	reselect pattern and activate this job.
	*/

	cp->jump_ccb.l_cmd	= (SCR_JUMP ^ IFFALSE (DATA (cp->tag)));
	cp->tlimit		= time.tv_sec + xp->timeout / 1000 + 2;
	cp->magic               = CCB_MAGIC;

	/*
	**	insert into startqueue.
	*/

	ptr = np->squeueput + 1;
	if (ptr >= MAX_START) ptr=0;
	np->squeue [ptr          ] = vtophys(&np->script->idle);
	np->squeue [np->squeueput] = vtophys(&cp->phys);
	np->squeueput = ptr;

#ifdef SCSI_NCR_DEBUG
	if(ncr_debug & DEBUG_QUEUE)
		printf ("%s: queuepos=%d tryoffset=%d.\n", ncr_name (np),
		np->squeueput, np->script->startpos[0]-(vtophys(&np->script->tryloop)));
#endif /* SCSI_NCR_DEBUG */

	/*
	**	Script processor may be waiting for reconnect.
	**	Wake it up.
	*/
	OUTB (nc_istat, SIGP);

	/*
	**	If interrupts are enabled, return now.
	**	Command is successfully queued.
	*/

	np->lock--;
	if (!(flags & SCSI_NOMASK)) {
		splx (oldspl);
		if (np->lasttime) {
#ifdef SCSI_NCR_DEBUG
			if(ncr_debug & DEBUG_TINY) printf ("Q");
#endif /* SCSI_NCR_DEBUG */
			return(SUCCESSFULLY_QUEUED);
		};
	};

	/*----------------------------------------------------
	**
	**	Interrupts not yet enabled - have to poll.
	**
	**----------------------------------------------------
	*/

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_POLL) printf("P");
#endif /* SCSI_NCR_DEBUG */

	for (i=xp->timeout; i && !(xp->flags & ITSDONE);i--) {
#ifdef SCSI_NCR_DEBUG
		if ((ncr_debug & DEBUG_POLL) && (cp->host_status))
			printf ("%c", (cp->host_status & 0xf) + '0');
#endif /* SCSI_NCR_DEBUG */
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
			ncr_name (np), INL(nc_dsp));
		ncr_init (np, "timeout", HS_TIMEOUT);
	};

	if (!(xp->flags & ITSDONE)) {
		cp-> host_status = HS_SEL_TIMEOUT;
		ncr_complete (np, cp);
	};

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_RESULT) {
		printf ("%s: result: %x %x %x.\n",
			ncr_name (np), cp->host_status,
			cp->scsi_status, cp->scs2_status);
	};
#endif /* SCSI_NCR_DEBUG */
	if (!(flags & SCSI_NOMASK))
		return (SUCCESSFULLY_QUEUED);
	switch (xp->error) {
	case  0     : return (COMPLETE);
	case XS_BUSY: return (TRY_AGAIN_LATER);
	};
	return (HAD_ERROR);
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

	if (!cp || !cp->magic || !cp->xfer) return;
	cp->magic = 1;
	cp->tlimit= 0;

	/*
	**	No Reselect anymore.
	*/
	cp->jump_ccb.l_cmd = (SCR_JUMP);

	/*
	**	No starting.
	*/
	cp->phys.header.launch.l_paddr= vtophys (&np->script->idle);

	/*
	**	timestamp
	*/
	ncb_profile (np, cp);

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_TINY)
		printf ("CCB=%x STAT=%x/%x/%x\n", (u_long)cp & 0xfff,
			cp->host_status,cp->scsi_status,cp->scs2_status);
#endif /* SCSI_NCR_DEBUG */

	xp  = cp->xfer;
	cp->xfer = NULL;
	tp = &np->target[xp->TARGET];
	lp  = tp->lp[xp->LUN];

	/*
	** @PARITY@
	**	Check for parity errors.
	*/

	if (cp->parity_errs) {
		PRINT_ADDR(xp);
		printf ("%d parity error(s), fallback.\n", cp->parity_errs);
		/*
		**	fallback to asynch transfer.
		*/
		tp->usrsync=255;
		tp->period =  0;
	};

	/*
	**	Check the status.
	*/
	if (   (cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_GOOD)) {

		/*
		**   All went well.
		*/
		xp->resid = 0;

		/*
		**	Try to assign a ccb to this nexus
		*/
		ncr_alloc_ccb (np, xp);

		/*
		**	On inquire cmd (0x12) save some data.
		*/
#ifdef ANCIENT
		if (cp->cmd.opcode == 0x12) {
#else /* ANCIENT */
		if (xp->cmd->opcode == 0x12) {
#endif /* ANCIENT */
			bcopy (	xp->data,
				&tp->inqdata,
				sizeof (tp->inqdata));
			ncr_setmaxtags (tp, tp->usrtags);
			tp->period=0;
		};

		if (!tp->sval) {
			PRINT_ADDR(xp);
			printf ("asynchronous.\n");
			tp->sval = 0xe0;
		};

		/*
		**	Announce changes to the generic driver
		*/
		if (lp) {
			ncr_settags (tp, lp);
			if (lp->reqlink != lp->actlink)
				ncr_opennings (np, lp, xp);
		};

#ifdef DK
		dk_xfer[DK] ++;
		dk_wds [DK] += xp->datalen/64;
		dk_wpms[DK] =  1000000;
#endif /* DK */

		tp->bytes     += xp->datalen;
		tp->transfers ++;

	} else if (xp->flags & SCSI_ERR_OK) {

		/*
		**   Not correct, but errors expected.
		*/
		xp->resid = 0;

	} else if ((cp->host_status == HS_COMPLETE)
		&& (cp->scsi_status == S_CHECK_COND)
		&& (cp->scs2_status == S_GOOD)) {

		/*
		**   Check condition code
		*/
		xp->error = XS_SENSE;

#ifdef SCSI_NCR_DEBUG
		if (ncr_debug & (DEBUG_RESULT|DEBUG_TINY)) {
			u_char * p = (u_char*) & xp->sense;
			int i;
			printf ("\n%s: sense data:", ncr_name (np));
			for (i=0; i<14; i++) printf (" %x", *p++);
			printf (".\n");
		};
#endif /* SCSI_NCR_DEBUG */

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
		printf ("COMMAND FAILED (%x %x %x) @%x.\n",
			cp->host_status, cp->scsi_status, cp->scs2_status,
			cp);

		xp->error = XS_DRIVER_STUFFUP;
	}

	xp->flags |= ITSDONE;

	/*
	**	Free this ccb
	*/
	ncr_free_ccb (np, cp, xp->flags);

	/*
	**	signal completion to generic driver.
	*/
#ifdef ANCIENT
	if (xp->when_done)
		(*(xp->when_done))(xp->done_arg,xp->done_arg2);
#else /* ANCIENT */
	scsi_done (xp);
#endif /* ANCIENT */
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
#ifdef SCSI_NCR_DEBUG
			if(ncr_debug & DEBUG_TINY) printf ("D");
#endif /* SCSI_NCR_DEBUG */
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

	/*
	**	Reset chip.
	*/

	OUTB (nc_istat,  SRST	);

	/*
	**	Message.
	*/

	if (msg) printf ("%s: restart (%s).\n", ncr_name (np), msg);

	/*
	**	Clear Start Queue
	*/

	for (i=0;i<MAX_START;i++)
		np -> squeue [i] = vtophys (&np->script->idle);

	/*
	**	Start at first entry.
	*/

	np->squeueput = 0;
	np->script->startpos[0] = vtophys (&np->script->tryloop);
	np->script->start0  [0] = SCR_INT ^ IFFALSE (0);

	/*
	**	Wakeup all pending jobs.
	*/

	ncr_wakeup (np, code);

	/*
	**	Init chip.
	*/

	OUTB (nc_istat,  0	);	/*  Remove Reset, abort ...          */
	OUTB (nc_scntl0, 0xca   );      /*  full arb., ena parity, par->ATN  */
	OUTB (nc_scntl1, 0x00	);	/*  odd parity, and remove CRST!!    */
	OUTB (nc_scntl3, np->rv_scntl3);/*  timing prescaler                 */
	OUTB (nc_scid  , 0x40|np->myaddr); /*  host adapter SCSI address     */
	OUTB (nc_respid, 1<<np->myaddr);/*  id to respond to                 */
	OUTB (nc_istat , SIGP	);	/*  Signal Process                   */
	OUTB (nc_dmode , 0xc	);	/*  Burst length = 16 transfer       */
	OUTB (nc_dcntl , NOCOM	);	/*  no single step mode, protect SFBR*/
	OUTB (nc_ctest4, 0x08	);	/*  enable master parity checking    */
	OUTB (nc_stest2, EXT    );	/*  Extended Sreq/Sack filtering     */
	OUTB (nc_stest3, TE     );	/*  TolerANT enable                  */
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

	for (i=0;i<MAX_TARGET;i++) {
		tcb_p tp = &np->target[i];
		tp->period  = 0;
		tp->sval    = 0;
		tp->usrsync = usrsync;
	}

	/*
	**      enable ints
	*/

	OUTW (nc_sien , STO|HTH|MA|SGE|UDC|RST);
	OUTB (nc_dien , MDPE|BF|ABRT|SSI|SIR|IID);

	/*
	**    Start script processor.
	*/

	OUTL (nc_dsp, vtophys (&np->script->start));
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
	u_short target = INB (nc_ctest0)&7;
	tcb_p tp;

	assert (cp);
	if (!cp) return;

	xp = cp->xfer;
	assert (xp);
	if (!xp) return;
	assert (target == xp->TARGET & 7);

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
	} else {
		printf ("asynchronous.\n");
	}

	/*
	**	set actual value and sync_status
	*/
	OUTB (nc_scr3 , sxfer);
	OUTB (nc_sxfer, sxfer);

	/*
	**	patch ALL ccbs of this target.
	*/
	for (cp = &np->ccb; cp; cp = cp->link_ccb) {
		if (!cp->xfer) continue;
		if (cp->xfer->TARGET != target) continue;
		cp->sync_status = sxfer;
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
			tp->period  = 0;
		};
		break;

	case UC_SETTAGS:
		if (np->user.data > SCSI_NCR_MAX_TAGS)
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

		if (np->latetime>10) {
			/*
			**	Although we tried to wakeup it,
			**	the script processor didn't answer.
			**
			**	May be a target is hanging,
			**	or another initator lets a tape device
			**	rewind with disconnect disabled :-(
			**
			**	We won't accept that.
			*/
			printf ("%s: reset by timeout.\n", ncr_name (np));
			OUTB (nc_istat, SRST);
			OUTB (nc_istat, 0);
			if (INB (nc_sbcl) & CBSY)
				OUTB (nc_scntl1, CRST);
			ncr_init (np, NULL, HS_TIMEOUT);
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
				vtophys (&np->script->select)) {
				printf ("%s: timeout ccb=%x (skip)\n",
					ncr_name (np), cp);
				cp->phys.header.launch.l_paddr
				= vtophys (&np->script->skip);
			};

			switch (cp->host_status) {

			case HS_BUSY:
			case HS_NEGOTIATE:
				/*
				** still in start queue ?
				*/
				if (cp->phys.header.launch.l_paddr ==
					vtophys (&np->script->skip))
					continue;

				/* fall through */
			case HS_DISCONNECT:
				cp->host_status=HS_TIMEOUT;
			};
			cp->tag = 0;

			/*
			**	wakeup this ccb.
			*/
			{
				int oldspl = splbio();
				ncr_complete (np, cp);
				splx (oldspl);
			};
		};
	}

	timeout (TIMEOUT ncr_timeout, (caddr_t) np, step ? step : 1);

	if ((INB(nc_istat) & (INTF|SIP|DIP)) && !np->lock) {

		/*
		**	Process pending interrupts.
		*/

		int	oldspl	= splbio ();
#ifndef __NetBSD__
		u_long  imask	= getirr();
#endif
#ifdef SCSI_NCR_DEBUG
		if (ncr_debug & DEBUG_TINY) printf ("{");
#endif /* SCSI_NCR_DEBUG */
		ncr_exception (np);
#ifdef SCSI_NCR_DEBUG
		if (ncr_debug & DEBUG_TINY) printf ("}");
#endif /* SCSI_NCR_DEBUG */
#ifndef __NetBSD__
		imask &=~getirr();
		splx (oldspl);

		/*
		**	automagically find int vector.
		*/
		if (imask) {
			if ((imask != np->imask) && (np->mcount < 100))
				np->mcount = 0;
			np->imask = imask;
			np->mcount++;
		};

		/*
		**	a hint to the user :-)
		*/
		if (np->mcount == 100) {
			if (np->imask & (np->imask-1)) {
				printf ("%s: please configure intr mask %x.\n",
					ncr_name (np), np->imask);
			} else {
				printf ("%s: please configure intr %d.\n",
					ncr_name (np), ffs (np->imask)-1);
			};
			np->mcount++;
		};
#endif
	};
}

/*==========================================================
**
**
**	ncr chip exception handler.
**
**
**==========================================================
**
**	@RECOVER@ this function is not yet complete.
**
**	there should be better ways to handle
**	unexpected exceptions than to restart the
**	script processor.
**
**----------------------------------------------------------
*/

void ncr_exception (ncb_p np)
{
	u_char  istat, dstat;
	u_short sist;
	u_long	dsp;

	/*
	**	interrupt on the fly ?
	*/
	while ((istat = INB (nc_istat)) & INTF) {
#ifdef SCSI_NCR_DEBUG
		if (ncr_debug & DEBUG_TINY) printf ("F");
#endif /* SCSI_NCR_DEBUG */
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

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_TINY)
		printf ("<%d|%x:%x|%x:%x>",
			INB(nc_scr0),
			dstat,sist,
			INL(nc_dsp),INL(nc_dbc));
#endif /* SCSI_NCR_DEBUG */
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
		ncr_init (np, "scsi reset", HS_RESET);
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

	/*-------------------------------------------
	**	Programmed interrupt
	**-------------------------------------------
	*/

	if ((dstat & SIR) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|IID)) &&
		(INB(nc_dsps) <= 8)) {
		ncr_int_sir (np);
		return;
	};

	/*========================================
	**	do the register dump
	**========================================
	*/

#ifdef SCSI_NCR_DEBUG
	if (!(ncr_debug & DEBUG_NODUMP)) /* @DEBUG@ */
#endif
	if (time.tv_sec - np->regtime.tv_sec>10) {
		int i;
		np->regtime = time;
		for (i=0; i<sizeof(np->regdump); i++)
			((char*)&np->regdump)[i] = ((char*)np->reg)[i];
		np->regdump.nc_dstat = dstat;
		np->regdump.nc_sist  = sist;
	};

	printf ("%s targ %d?: ERROR (%x:%x:%x) (%x/%x) @ (%x:%x).\n",
		ncr_name (np), INB (nc_ctest0)&7, dstat, sist,
		INB (nc_sbcl),
		INB (nc_sxfer),INB (nc_scr3),
		dsp = INL (nc_dsp), INL (nc_dbc));

	/*----------------------------------------
	**	clean up the dma fifo
	**----------------------------------------
	*/

	if ((INB(nc_sstat0)&(ILF|ORF|OLF)) ||
            (INB(nc_sstat1)&0xf0) || !(dstat & DFE)) {
		printf ("%s: have to clear fifos.\n", ncr_name (np));
		OUTB (nc_stest3, TE|CSF);	/* clear scsi fifo */
		OUTB (nc_ctest3, CLF);		/* clear dma fifo  */
	}

	/*----------------------------------------
	**	unexpected disconnect
	**----------------------------------------
	*/

	if ((sist  & UDC) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR|IID))) {
		OUTB (nc_scr0, HS_UNEXPECTED);
		OUTL (nc_dsp, vtophys(&np->script->cleanup));
		return;
	};

	/*----------------------------------------
	**	cannot disconnect
	**----------------------------------------
	*/

	if ((dstat & IID) &&
		!(sist  & (STO|GEN|HTH|MA|SGE|UDC|RST|PAR)) &&
		!(dstat & (MDPE|BF|ABRT|SIR)) &&
		((INL(nc_dbc) & 0xf8000000) == 0x18000000)) {
		/*
		**      Data cycles while waiting for disconnect.
		**	Force disconnect.
		*/
		OUTB (nc_scntl1, 0);
		/*
		**      System may hang, but timeout will handle that.
		**	In fact, timeout can handle ALL problems :-)
		*/
		OUTB (nc_dcntl, (STD|NOCOM));
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
**	They may occur if there are problems with synch transfers,
**	or if targets are powerswitched while the driver is running.
*/

	if (sist & SGE) {
		OUTB (nc_ctest3, CLF);		/* clear scsi offsets */
	}

#ifdef SCSI_NCR_DEBUG
	/*
	**	Freeze controller to be able to read the messages.
	*/

	if (ncr_debug & DEBUG_FREEZE) {
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
#endif /* SCSI_NCR_DEBUG */

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
**	Although a STO-Interupt is pending,
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
#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_TINY) printf ("T");
#endif /* SCSI_NCR_DEBUG */

	/*
	**	look for ccb and set the status.
	*/

	dsa = INL (nc_dsa);
	cp = &np->ccb;
	while (cp && (vtophys(&cp->phys) != dsa))
		cp = cp->link_ccb;

	if (cp) {
		cp-> host_status = HS_SEL_TIMEOUT;
		ncr_complete (np, cp);
	};

	/*
	**	repair start queue
	*/

	scratcha = INL (nc_scratcha);
	diff = scratcha - vtophys(&np->script->tryloop);

	assert ((diff <= MAX_START * 20) && !(diff % 20));

	if ((diff <= MAX_START * 20) && !(diff % 20)) {
		np->script->startpos[0] = scratcha;
		OUTL (nc_dsp, vtophys (&np->script->start));
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
	u_long	oadr;
	u_long	olen;
	u_long	*tblp;
	u_long	*newcmd;
	u_char	cmd;
	u_char	sbcl;
	u_char	delta;
	u_char	ss0;
	ccb_p	cp;

	dsp = INL (nc_dsp);
	dsa = INL (nc_dsa);
	dbc = INL (nc_dbc);
	ss0 = INB (nc_sstat0);
	sbcl= INB (nc_sbcl);

	cmd = dbc >> 24;
	rest= dbc & 0xffffff;
	delta=(INB (nc_dfifo) - rest) & 0x7f;

	/*
	**	The data in the dma fifo has not been transfered to
	**	the target -> add the amount to the rest
	**	and clear the data.
	*/

	if (! (INB(nc_dstat) & DFE)) rest += delta;
	if (ss0 & OLF) rest++;
	if (ss0 & ORF) rest++;

	OUTB (nc_ctest3, CLF   );	/* clear dma fifo  */
	OUTB (nc_stest3, TE|CSF);	/* clear scsi fifo */

	/*
	**	verify cp
	*/
	dsa = INL (nc_dsa);
	cp = &np->ccb;
	while (cp && (vtophys(&cp->phys) != dsa))
		cp = cp->link_ccb;

	assert (cp == np->header.cp);
	assert (cp);
	if (!cp)
		return;

	/*
	**	find the interrupted script command,
	**	and the address at where to continue.
	*/

	if (dsp == vtophys (&cp->patch[2])) {
		vdsp = &cp->patch[0];
		nxtdsp = vdsp[3];
	} else if (dsp == vtophys (&cp->patch[6])) {
		vdsp = &cp->patch[4];
		nxtdsp = vdsp[3];
	} else {
		vdsp = (u_long*) ((char*)np->script - vtophys(np->script) + dsp -8);
		nxtdsp = dsp;
	};

#ifdef SCSI_NCR_DEBUG
	/*
	**	log the information
	*/
	if (ncr_debug & (DEBUG_TINY|DEBUG_PHASE)) {
		printf ("P%d%d ",cmd&7, sbcl&7);
		printf ("RL=%d D=%d SS0=%x ",rest,delta,ss0);
	};
	if (ncr_debug & DEBUG_PHASE) {
		printf ("\nCP=%x CP2=%x DSP=%x NXT=%x VDSP=%x CMD=%x ",
			cp, np->header.cp, dsp, nxtdsp, vdsp, cmd);
	};
#endif /* SCSI_NCR_DEBUG */

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

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_PHASE) {
		printf ("OCMD=%x\nTBLP=%x OLEN=%x OADR=%x\n",
			vdsp[0] >> 24, tblp, olen, oadr);
	};
#endif /* SCSI_NCR_DEBUG */

	/*
	**	if old phase not dataphase, leave here.
	*/

	assert (cmd == (vdsp[0] >> 24));
	if (cmd & 0x06) {
		PRINT_ADDR(cp->xfer);
		printf ("phase change %d-%d %d@%x resid=%d.\n",
			cmd&7, sbcl&7, olen, oadr, rest);

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

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_PHASE) {
		PRINT_ADDR(cp->xfer);
		printf ("newcmd[%d] %x %x %x %x.\n",
			newcmd - cp->patch,
			newcmd[0], newcmd[1], newcmd[2], newcmd[3]);
	}
#endif /* SCSI_NCR_DEBUG */
	/*
	**	fake the return address (to the patch).
	**	and restart script processor at dispatcher.
	*/
	np->profile.num_break++;
	OUTL (nc_temp, vtophys (newcmd));
	OUTL (nc_dsp, vtophys (&np->script->dispatch));
}

/*==========================================================
**
**
**      ncr chip exception handler for programmed interrupts.
**
**
**==========================================================
*/

static void ncr_show_msg (u_char * msg)
{
	u_char i;
	printf ("%x",*msg);
	if (*msg==M_EXTENDED) {
		for (i=1;i<8;i++) {
			if (i-1>msg[1]) break;
			printf ("-%x",msg[i]);
		};
	} else if ((*msg & 0xf0) == 0x20) {
		printf ("-%x",msg[1]);
	}
}

void ncr_int_sir (ncb_p np)
{
	u_char chg, ofs, per, fak;
	u_char num = INB (nc_dsps);
	ccb_p	cp=0;
	tcb_p	tp;
	u_long	dsa;
	u_char	target = INB (nc_ctest0) & 7;
	int	i;
#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_TINY) printf ("I#%d", num);
#endif /* SCSI_NCR_DEBUG */

	switch (num) {
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 8:
		/*
		**	lookup the ccb
		*/
		dsa = INL (nc_dsa);
		cp = &np->ccb;
		while (cp && (vtophys(&cp->phys) != dsa))
			cp = cp->link_ccb;

		assert (cp == np->header.cp);
		assert (cp);
		if (!cp)
			goto out;
	}

	switch (num) {

/*--------------------------------------------------------------------
**
**	Processing of interrupted getcc selects
**
**--------------------------------------------------------------------
*/

	case 1: /*
		**	Script processor is idle.
		**	Look for interrupted "check cond"
		*/

		printf ("%s: int#%d",ncr_name (np),num);
		cp = (ccb_p) 0;
		for (i=0; i<MAX_TARGET; i++) {
			printf (" t%d", i);
			tp = &np->target[i];
			printf ("+");
			cp = tp->hold_cp;
			if (!cp) continue;
			printf ("+");
			if ((cp->host_status==HS_BUSY) &&
				(cp->scsi_status==S_CHECK_COND) &&
				(cp->scs2_status==S_ILLEGAL))
				break;
			printf ("- (remove)");
			tp->hold_cp = cp = (ccb_p) 0;
		};

		if (cp) {
			printf ("+ restart job ..\n");
			OUTL (nc_dsa, vtophys (&cp->phys));
			OUTL (nc_dsp, vtophys (&np->script->getcc));
			return;
		};

		/*
		**	no job, resume normal processing
		*/
		printf (" -- remove trap\n");
		np->script->start0[0] =  SCR_INT ^ IFFALSE (0);
		break;

	case 2: /*
		**	While trying to reselect for
		**	getting the condition code,
		**	a target reselected us.
		*/
		PRINT_ADDR(cp->xfer);
		printf ("in getcc reselect by t%d.\n",
			INB(nc_ssid)&7);

		/*
		**	Mark this job
		*/
		cp->host_status = HS_BUSY;
		cp->scsi_status = S_CHECK_COND;
		cp->scs2_status = S_ILLEGAL;
		np->target[cp->xfer->TARGET].hold_cp = cp;

		/*
		**	And patch code to restart it.
		*/
		np->script->start0[0] =  SCR_INT;
		break;

/*--------------------------------------------------------------------
**
**	Negotiation of synch mode.
**
**	Possible cases:             int   msg_in[0] sxfer  send   goto
**	We try try to negotiate:
**	-> target doesnt't msgin    (3)   noop      ASYNC  -      -
**	-> target rejected our msg  (3)   reject    ASYNC  -      -
**	-> target answered  (ok)    (3)   sdtr      set    -      clrack
**	-> target answered (!ok)    (3)   sdtr      ASYNC  REJ--->msg_bad
**      -> any other msgin           -
**
**	Target tries to negotiate:
**	-> incoming message         (4)   sdtr      set    SDTR   -
**	We sent our answer:
**	-> target doesn't msgout    (4)   reject*   ASYNC  -      -
**	-> target rejected our msg  (4)   reject    ASYNC  -      -
**	-> target negotiates again  (4)   sdtr      set    SDTR   -
**
**--------------------------------------------------------------------
*/
	case 3:
	case 4:
		/*
		**	@CHECKOUT@
		*/

#ifdef SCSI_NCR_DEBUG
		if (ncr_debug & DEBUG_SDTR) {
			PRINT_ADDR(cp->xfer);
			printf ("sync msgin: ");
			ncr_show_msg (np->msgin);
			printf (".\n");
		};
#endif /* SCSI_NCR_DEBUG */

		tp = &np->target[target];

		/*
		**	any error in negotiation:
		**	fall back to asynch.
		*/

		if ((np->msgin[0]!=M_EXTENDED) ||
			(np->msgin[1]!=3) ||
			(np->msgin[2]!=M_X_SDTR)) {
			np->msgin [0] = M_NOOP;
			ncr_setsync (np, cp, 0xe0);
			break;
		}

		per = np->msgin[3];
		ofs = np->msgin[4];

		/*
		**	if target sends SDTR message,
		**		it CAN transfer synch.
		*/

		if (ofs)
			tp->inqdata[7] |= INQ7_SYNC;

		/*------------------------------------------------
		**	do actual computation.
		**------------------------------------------------
		*/
		chg = 0;

		if (ofs==0) per=255;
		if (per < np->ns_sync)	{chg = 1; per = np->ns_sync;}
		if (per < tp->minsync)
			{chg = 1; per = tp->minsync;}
		if (ofs > tp->maxoffs)
			{chg = 1; ofs = 8;}
		fak = (4ul * per - 1) / np->ns_sync - 3;

		if (ofs && (fak>7))   {chg = 1; ofs = 0;}

#ifdef	SCSI_NCR_DEBUG
		if (ncr_debug & DEBUG_SDTR) {
			PRINT_ADDR(cp->xfer);
			printf ("sync: per=%d ofs=%d fak=%d chg=%d.\n",
				per, ofs, fak, chg);
		}
#endif /* SCSI_NCR_DEBUG */

		/*
		**	if the answer had bad values,
		**	we will use asynch mode.
		*/

		if ((num == 3) && chg) ofs = 0;
		if (!ofs) fak=7;

		/*
		**      Set synchronous mode now.
		*/
		ncr_setsync (np, cp, (fak<<5)|ofs);

		if (num == 3) {
			if (chg) OUTL (nc_dsp,vtophys (&np->script->msg_bad));
			else     OUTL (nc_dsp,vtophys (&np->script->clrack));
			return;
		};

		/*------------------------------------------------
		**      prepare an answer message
		**------------------------------------------------
		*/

		np->msgout[0] = M_EXTENDED;
		np->msgout[1] = 3;
		np->msgout[2] = M_X_SDTR;
		np->msgout[3] = per;
		np->msgout[4] = ofs;

		np->msgin [0] = M_NOOP;

#ifdef SCSI_NCR_DEBUG
		if (ncr_debug & DEBUG_SDTR) {
			PRINT_ADDR(cp->xfer);
			printf ("sync msgout: ");
			ncr_show_msg (np->msgin);
			printf (".\n");
		}
#endif /* SCSI_NCR_DEBUG */
		break;


/*--------------------------------------------------------------------
**
**	Processing of special messages
**
**--------------------------------------------------------------------
*/

	case 5: /*
		**	We received a M_REJECT message.
		*/
		PRINT_ADDR(cp->xfer);
		printf ("M_REJECT received (%x:%x).\n",
			np->lastmsg, np->msgout[0]);
		break;

	case 6: /*
		**	We received an unknown message
		*/
		PRINT_ADDR(cp->xfer);
		printf ("M_REJECT sent for ");
		ncr_show_msg (np->msgin);
		printf (".\n");
		break;

/*--------------------------------------------------------------------
**
**	Processing of a "S_QUEUE_FULL" status.
**
**	The current command has been rejected.
**	We have started too many commands for that target.
**
**	If possible, reinsert at head of queue.
**	Stall queue until there are no disconnected jobs
**	(ncr REALLY idle). Then restart processing.
**
**--------------------------------------------------------------------
*/
	case 8:	/*
		**	Stall the start queue.
		*/
		PRINT_ADDR(cp->xfer);
		printf ("queue full.\n");

		np->script->start1[0] =  SCR_INT;

		/*
		**	Try to disable tagged transfers.
		*/
		ncr_setmaxtags (&np->target[target], 0);

		/*
		** @QUEUE@ reinsert current job in queue.
		*/

		/* fall through */

	case 7:	/*
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
			OUTL (nc_dsp, vtophys (&np->script->reselect));
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
	ccb_p cp = (ccb_p ) 0;

	/*
	**	Lun structure available ?
	*/

	lp = np->target[target].lp[lun];
	if (lp)
		cp = lp->next_ccb;

	/*
	**	Look for free CCB
	*/

	while (cp && cp->magic) cp = cp->next_ccb;

	/*
	**	if nothing available, take the default.
	*/

	if (!cp) cp = &np->ccb;

	/*
	**	Wait until available.
	*/

	while (cp->magic) {
		if (flags & SCSI_NOSLEEP) break;
		if (tsleep ((caddr_t)cp, PZERO|PCATCH, "ncr", 0))
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

	if (!cp) return;

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

	if (!np) return;
	if (!xp) return;

	target = xp->TARGET;
	lun    = xp->LUN;

	if (target>=MAX_TARGET) return;
	if (lun   >=MAX_LUN   ) return;

	/*
	**	target control block ?
	*/
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

		assert (( (offsetof(struct ncr_reg, nc_sxfer) ^ 
			    offsetof(struct tcb    , sval    )) &3) == 0);

		tp->call_lun.l_cmd   = (SCR_CALL);
		tp->call_lun.l_paddr = vtophys (&np->script->resel_lun);

		tp->jump_lcb.l_cmd   = (SCR_JUMP);
		tp->jump_lcb.l_paddr = vtophys (&np->script->abort);

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
		lp = (lcb_p) malloc (sizeof (struct lcb), M_DEVBUF, M_NOWAIT);
		if (!lp) return;

		/*
		**	Initialize it
		*/
		bzero (lp, sizeof (*lp));
		lp->jump_lcb.l_cmd   = (SCR_JUMP ^ IFFALSE (DATA (lun)));
		lp->jump_lcb.l_paddr = tp->jump_lcb.l_paddr;

		lp->call_tag.l_cmd   = (SCR_CALL);
		lp->call_tag.l_paddr = vtophys (&np->script->resel_tag);

		lp->jump_ccb.l_cmd   = (SCR_JUMP);
		lp->jump_ccb.l_paddr = vtophys (&np->script->aborttag);

		lp->actlink = 1;
		/*
		**   Link into Lun-Chain
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

#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_ALLOC) {
		PRINT_ADDR(xp);
		printf ("new ccb @%x.\n", cp);
	}
#endif /* SCSI_NCR_DEBUG */

	/*
	**	Count it
	*/
	lp->actccbs++;
	np->actccbs++;

	/*
	**	Initialize it.
	*/
	bzero (cp, sizeof (*cp));

	/*
	**	link in reselect chain.
	*/
	cp->jump_ccb.l_cmd   = SCR_JUMP;
	cp->jump_ccb.l_paddr = lp->jump_ccb.l_paddr;
	lp->jump_ccb.l_paddr = vtophys(&cp->jump_ccb);
	cp->call_tmp.l_cmd   = SCR_CALL;
	cp->call_tmp.l_paddr = vtophys(&np->script->resel_tmp);

	/*
	**	link in wakeup chain
	*/
	cp->link_ccb      = np->ccb.link_ccb;
	np->ccb.link_ccb  = cp;

	/*
	**	Link into CCB-Chain
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
#ifndef	ANCIENT
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

		if (diff > xp->sc_link->opennings)
			diff = xp->sc_link->opennings;

		/*
		**	reduce it.
		*/
		
		xp->sc_link->opennings	-= diff;
		lp->actlink		-= diff;
#ifdef SCSI_NCR_DEBUG
		if (ncr_debug & DEBUG_TAGS)
			printf ("%s: actlink: diff=%d, new=%d, req=%d\n",
				ncr_name(np), diff, lp->actlink, lp->reqlink);
#endif /* SCSI_NCR_DEBUG */
		return;
	};

	/*
	**	want to increase the number ?
	*/
	if (lp->reqlink > lp->actlink) {
		u_char diff = lp->reqlink - lp->actlink;

		xp->sc_link->opennings	+= diff;
		lp->actlink		+= diff;
		wakeup ((caddr_t) xp->sc_link);
#ifdef SCSI_NCR_DEBUG
		if (ncr_debug & DEBUG_TAGS)
			printf ("%s: actlink: diff=%d, new=%d, req=%d\n",
				ncr_name(np), diff, lp->actlink, lp->reqlink);
#endif
	};
#endif
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

static	u_long	ncr_scatter
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
	**	We try to reduce the number of interrupts due to
	**	unexpected phase changes due to disconnects.
	**	A typical harddisk may disconnect before ANY block.
	**	If we want to avoid unexpected phase changes at all
	**	we have to use a break point every 512 bytes.
	**	Of course the number of scatter/gather blocks is
	**	limited.
	*/

	free = MAX_SCATTER - 1;

	if (vaddr & (NBPG-1)) free -= datalen / NBPG;

	if (free>1)
		while ((chunk * free >= 2 * datalen) && (chunk>=1024))
			chunk /= 2;

#ifdef SCSI_NCR_DEBUG
	if(ncr_debug & DEBUG_SCATTER)
		printf("ncr?:\tscattering virtual=0x%x size=%d chunk=%d.\n",
			(u_long) vaddr, (u_long) datalen, chunk);
#endif /* SCSI_NCR_DEBUG */

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

			size = pnext - paddr;                /* page size */
			if (size > datalen) size = datalen;  /* data size */
			if (size > csize  ) size = csize  ;  /* chunksize */

			segsize += size;
			vaddr   += size;
			csize   -= size;
			datalen -= size;
			paddr    = vtophys (vaddr);
		};

#ifdef SCSI_NCR_DEBUG
		if(ncr_debug & DEBUG_SCATTER)
			printf ("\tseg #%d  addr=%x  size=%d  (rest=%d).\n",
			segment,  segaddr, segsize, datalen);
#endif /* SCSI_NCR_DEBUG */

		phys->data[segment].addr = segaddr;
		phys->data[segment].size = segsize;
		segment++;
	}

	if (datalen)
		printf("ncr?: scatter/gather failed (residue=%d).\n",
		datalen);

	return (datalen);
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
#ifdef SCSI_NCR_DEBUG
	if (ncr_debug & DEBUG_TIMING)
		printf ("%s: sclk=%d async=%d sync=%d (ns) scntl3=0x%x\n",
		ncr_name (np), ns_clock, np->ns_async, np->ns_sync, np->rv_scntl3);
#endif /* SCSI_NCR_DEBUG */
}

/*=========================================================================*/
#endif /* KERNEL */
