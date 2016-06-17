/*
 * Device driver for the SYMBIOS/LSILOGIC 53C8XX and 53C1010 family 
 * of PCI-SCSI IO processors.
 *
 * Copyright (C) 1999-2001  Gerard Roudier <groudier@free.fr>
 *
 * This driver is derived from the Linux sym53c8xx driver.
 * Copyright (C) 1998-2000  Gerard Roudier
 *
 * The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 * a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 * The original ncr driver has been written for 386bsd and FreeBSD by
 *         Wolfgang Stanglmeier        <wolf@cologne.de>
 *         Stefan Esser                <se@mi.Uni-Koeln.de>
 * Copyright (C) 1994  Wolfgang Stanglmeier
 *
 * Other major contributions:
 *
 * NVRAM detection and reading.
 * Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under the terms of the GPL, the terms
 * and conditions of this License will apply in addition to those of the
 * GPL with the exception of any terms or conditions of this License that
 * conflict with, or are expressly prohibited by, the GPL.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef SYM_HIPD_H
#define SYM_HIPD_H

/*
 *  Generic driver options.
 *
 *  They may be defined in platform specific headers, if they 
 *  are useful.
 *
 *    SYM_OPT_NO_BUS_MEMORY_MAPPING
 *        When this option is set, the driver will not load the 
 *        on-chip RAM using MMIO, but let the SCRIPTS processor 
 *        do the work using MOVE MEMORY instructions.
 *        (set for Linux/PPC)
 *
 *    SYM_OPT_HANDLE_DIR_UNKNOWN
 *        When this option is set, the SCRIPTS used by the driver 
 *        are able to handle SCSI transfers with direction not 
 *        supplied by user.
 *        (set for Linux-2.0.X)
 *
 *    SYM_OPT_HANDLE_DEVICE_QUEUEING
 *        When this option is set, the driver will use a queue per 
 *        device and handle QUEUE FULL status requeuing internally.
 *
 *    SYM_OPT_BUS_DMA_ABSTRACTION
 *        When this option is set, the driver allocator is responsible 
 *        of maintaining bus physical addresses and so provides virtual 
 *        to bus physical address translation of driver data structures.
 *        (set for FreeBSD-4 and Linux 2.3)
 *
 *    SYM_OPT_SNIFF_INQUIRY
 *        When this option is set, the driver sniff out successful 
 *        INQUIRY response and performs negotiations accordingly.
 *        (set for Linux)
 *
 *    SYM_OPT_LIMIT_COMMAND_REORDERING
 *        When this option is set, the driver tries to limit tagged 
 *        command reordering to some reasonnable value.
 *        (set for Linux)
 */
#if 0
#define SYM_OPT_NO_BUS_MEMORY_MAPPING
#define SYM_OPT_HANDLE_DIR_UNKNOWN
#define SYM_OPT_HANDLE_DEVICE_QUEUEING
#define SYM_OPT_BUS_DMA_ABSTRACTION
#define SYM_OPT_SNIFF_INQUIRY
#define SYM_OPT_LIMIT_COMMAND_REORDERING
#endif

/*
 *  Active debugging tags and verbosity.
 *  Both DEBUG_FLAGS and sym_verbose can be redefined 
 *  by the platform specific code to something else.
 */
#define DEBUG_ALLOC	(0x0001)
#define DEBUG_PHASE	(0x0002)
#define DEBUG_POLL	(0x0004)
#define DEBUG_QUEUE	(0x0008)
#define DEBUG_RESULT	(0x0010)
#define DEBUG_SCATTER	(0x0020)
#define DEBUG_SCRIPT	(0x0040)
#define DEBUG_TINY	(0x0080)
#define DEBUG_TIMING	(0x0100)
#define DEBUG_NEGO	(0x0200)
#define DEBUG_TAGS	(0x0400)
#define DEBUG_POINTER	(0x0800)

#ifndef DEBUG_FLAGS
#define DEBUG_FLAGS	(0x0000)
#endif

#ifndef sym_verbose
#define sym_verbose	(np->verbose)
#endif

/*
 *  These ones should have been already defined.
 */
#ifndef offsetof
#define offsetof(t, m)	((size_t) (&((t *)0)->m))
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef assert
#define	assert(expression) { \
	if (!(expression)) { \
		(void)panic( \
			"assertion \"%s\" failed: file \"%s\", line %d\n", \
			#expression, \
			__FILE__, __LINE__); \
	} \
}
#endif

/*
 *  Number of tasks per device we want to handle.
 */
#if	SYM_CONF_MAX_TAG_ORDER > 8
#error	"more than 256 tags per logical unit not allowed."
#endif
#define	SYM_CONF_MAX_TASK	(1<<SYM_CONF_MAX_TAG_ORDER)

/*
 *  Donnot use more tasks that we can handle.
 */
#ifndef	SYM_CONF_MAX_TAG
#define	SYM_CONF_MAX_TAG	SYM_CONF_MAX_TASK
#endif
#if	SYM_CONF_MAX_TAG > SYM_CONF_MAX_TASK
#undef	SYM_CONF_MAX_TAG
#define	SYM_CONF_MAX_TAG	SYM_CONF_MAX_TASK
#endif

/*
 *    This one means 'NO TAG for this job'
 */
#define NO_TAG	(256)

/*
 *  Number of SCSI targets.
 */
#if	SYM_CONF_MAX_TARGET > 16
#error	"more than 16 targets not allowed."
#endif

/*
 *  Number of logical units per target.
 */
#if	SYM_CONF_MAX_LUN > 64
#error	"more than 64 logical units per target not allowed."
#endif

/*
 *    Asynchronous pre-scaler (ns). Shall be 40 for 
 *    the SCSI timings to be compliant.
 */
#define	SYM_CONF_MIN_ASYNC (40)

/*
 *  Number of entries in the START and DONE queues.
 *
 *  We limit to 1 PAGE in order to succeed allocation of 
 *  these queues. Each entry is 8 bytes long (2 DWORDS).
 */
#ifdef	SYM_CONF_MAX_START
#define	SYM_CONF_MAX_QUEUE (SYM_CONF_MAX_START+2)
#else
#define	SYM_CONF_MAX_QUEUE (7*SYM_CONF_MAX_TASK+2)
#define	SYM_CONF_MAX_START (SYM_CONF_MAX_QUEUE-2)
#endif

#if	SYM_CONF_MAX_QUEUE > SYM_MEM_CLUSTER_SIZE/8
#undef	SYM_CONF_MAX_QUEUE
#define	SYM_CONF_MAX_QUEUE (SYM_MEM_CLUSTER_SIZE/8)
#undef	SYM_CONF_MAX_START
#define	SYM_CONF_MAX_START (SYM_CONF_MAX_QUEUE-2)
#endif

/*
 *  For this one, we want a short name :-)
 */
#define MAX_QUEUE	SYM_CONF_MAX_QUEUE

/*
 *  Union of supported NVRAM formats.
 */
struct sym_nvram {
	int type;
#define	SYM_SYMBIOS_NVRAM	(1)
#define	SYM_TEKRAM_NVRAM	(2)
#if SYM_CONF_NVRAM_SUPPORT
	union {
		Symbios_nvram Symbios;
		Tekram_nvram Tekram;
	} data;
#endif
};

/*
 *  Common definitions for both bus space based and legacy IO methods.
 */
#define INB(r)		INB_OFF(offsetof(struct sym_reg,r))
#define INW(r)		INW_OFF(offsetof(struct sym_reg,r))
#define INL(r)		INL_OFF(offsetof(struct sym_reg,r))

#define OUTB(r, v)	OUTB_OFF(offsetof(struct sym_reg,r), (v))
#define OUTW(r, v)	OUTW_OFF(offsetof(struct sym_reg,r), (v))
#define OUTL(r, v)	OUTL_OFF(offsetof(struct sym_reg,r), (v))

#define OUTONB(r, m)	OUTB(r, INB(r) | (m))
#define OUTOFFB(r, m)	OUTB(r, INB(r) & ~(m))
#define OUTONW(r, m)	OUTW(r, INW(r) | (m))
#define OUTOFFW(r, m)	OUTW(r, INW(r) & ~(m))
#define OUTONL(r, m)	OUTL(r, INL(r) | (m))
#define OUTOFFL(r, m)	OUTL(r, INL(r) & ~(m))

/*
 *  We normally want the chip to have a consistent view
 *  of driver internal data structures when we restart it.
 *  Thus these macros.
 */
#define OUTL_DSP(v)				\
	do {					\
		MEMORY_WRITE_BARRIER();		\
		OUTL (nc_dsp, (v));		\
	} while (0)

#define OUTONB_STD()				\
	do {					\
		MEMORY_WRITE_BARRIER();		\
		OUTONB (nc_dcntl, (STD|NOCOM));	\
	} while (0)

/*
 *  Command control block states.
 */
#define HS_IDLE		(0)
#define HS_BUSY		(1)
#define HS_NEGOTIATE	(2)	/* sync/wide data transfer*/
#define HS_DISCONNECT	(3)	/* Disconnected by target */
#define HS_WAIT		(4)	/* waiting for resource	  */

#define HS_DONEMASK	(0x80)
#define HS_COMPLETE	(4|HS_DONEMASK)
#define HS_SEL_TIMEOUT	(5|HS_DONEMASK)	/* Selection timeout      */
#define HS_UNEXPECTED	(6|HS_DONEMASK)	/* Unexpected disconnect  */
#define HS_COMP_ERR	(7|HS_DONEMASK)	/* Completed with error	  */

/*
 *  Software Interrupt Codes
 */
#define	SIR_BAD_SCSI_STATUS	(1)
#define	SIR_SEL_ATN_NO_MSG_OUT	(2)
#define	SIR_MSG_RECEIVED	(3)
#define	SIR_MSG_WEIRD		(4)
#define	SIR_NEGO_FAILED		(5)
#define	SIR_NEGO_PROTO		(6)
#define	SIR_SCRIPT_STOPPED	(7)
#define	SIR_REJECT_TO_SEND	(8)
#define	SIR_SWIDE_OVERRUN	(9)
#define	SIR_SODL_UNDERRUN	(10)
#define	SIR_RESEL_NO_MSG_IN	(11)
#define	SIR_RESEL_NO_IDENTIFY	(12)
#define	SIR_RESEL_BAD_LUN	(13)
#define	SIR_TARGET_SELECTED	(14)
#define	SIR_RESEL_BAD_I_T_L	(15)
#define	SIR_RESEL_BAD_I_T_L_Q	(16)
#define	SIR_ABORT_SENT		(17)
#define	SIR_RESEL_ABORTED	(18)
#define	SIR_MSG_OUT_DONE	(19)
#define	SIR_COMPLETE_ERROR	(20)
#define	SIR_DATA_OVERRUN	(21)
#define	SIR_BAD_PHASE		(22)
#if	SYM_CONF_DMA_ADDRESSING_MODE == 2
#define	SIR_DMAP_DIRTY		(23)
#define	SIR_MAX			(23)
#else
#define	SIR_MAX			(22)
#endif

/*
 *  Extended error bit codes.
 *  xerr_status field of struct sym_ccb.
 */
#define	XE_EXTRA_DATA	(1)	/* unexpected data phase	 */
#define	XE_BAD_PHASE	(1<<1)	/* illegal phase (4/5)		 */
#define	XE_PARITY_ERR	(1<<2)	/* unrecovered SCSI parity error */
#define	XE_SODL_UNRUN	(1<<3)	/* ODD transfer in DATA OUT phase */
#define	XE_SWIDE_OVRUN	(1<<4)	/* ODD transfer in DATA IN phase */

/*
 *  Negotiation status.
 *  nego_status field of struct sym_ccb.
 */
#define NS_SYNC		(1)
#define NS_WIDE		(2)
#define NS_PPR		(3)

/*
 *  A CCB hashed table is used to retrieve CCB address 
 *  from DSA value.
 */
#define CCB_HASH_SHIFT		8
#define CCB_HASH_SIZE		(1UL << CCB_HASH_SHIFT)
#define CCB_HASH_MASK		(CCB_HASH_SIZE-1)
#if 1
#define CCB_HASH_CODE(dsa)	\
	(((dsa) >> (_LGRU16_(sizeof(struct sym_ccb)))) & CCB_HASH_MASK)
#else
#define CCB_HASH_CODE(dsa)	(((dsa) >> 9) & CCB_HASH_MASK)
#endif

#if	SYM_CONF_DMA_ADDRESSING_MODE == 2
/*
 *  We may want to use segment registers for 64 bit DMA.
 *  16 segments registers -> up to 64 GB addressable.
 */
#define SYM_DMAP_SHIFT	(4)
#define SYM_DMAP_SIZE	(1u<<SYM_DMAP_SHIFT)
#define SYM_DMAP_MASK	(SYM_DMAP_SIZE-1)
#endif

/*
 *  Device flags.
 */
#define SYM_DISC_ENABLED	(1)
#define SYM_TAGS_ENABLED	(1<<1)
#define SYM_SCAN_BOOT_DISABLED	(1<<2)
#define SYM_SCAN_LUNS_DISABLED	(1<<3)

/*
 *  Host adapter miscellaneous flags.
 */
#define SYM_AVOID_BUS_RESET	(1)
#define SYM_SCAN_TARGETS_HILO	(1<<1)

/*
 *  Misc.
 */
#define SYM_SNOOP_TIMEOUT (10000000)
#define BUS_8_BIT	0
#define BUS_16_BIT	1

/*
 *  Gather negotiable parameters value
 */
struct sym_trans {
	u8 scsi_version;
	u8 spi_version;
	u8 period;
	u8 offset;
	u8 width;
	u8 options;	/* PPR options */
};

struct sym_tinfo {
	struct sym_trans curr;
	struct sym_trans goal;
	struct sym_trans user;
#ifdef	SYM_OPT_ANNOUNCE_TRANSFER_RATE
	struct sym_trans prev;
#endif
};

/*
 *  Global TCB HEADER.
 *
 *  Due to lack of indirect addressing on earlier NCR chips,
 *  this substructure is copied from the TCB to a global 
 *  address after selection.
 *  For SYMBIOS chips that support LOAD/STORE this copy is 
 *  not needed and thus not performed.
 */
struct sym_tcbh {
	/*
	 *  Scripts bus addresses of LUN table accessed from scripts.
	 *  LUN #0 is a special case, since multi-lun devices are rare, 
	 *  and we we want to speed-up the general case and not waste 
	 *  resources.
	 */
	u32	luntbl_sa;	/* bus address of this table	*/
	u32	lun0_sa;	/* bus address of LCB #0	*/
	/*
	 *  Actual SYNC/WIDE IO registers value for this target.
	 *  'sval', 'wval' and 'uval' are read from SCRIPTS and 
	 *  so have alignment constraints.
	 */
/*0*/	u_char	uval;		/* -> SCNTL4 register		*/
/*1*/	u_char	sval;		/* -> SXFER  io register	*/
/*2*/	u_char	filler1;
/*3*/	u_char	wval;		/* -> SCNTL3 io register	*/
};

/*
 *  Target Control Block
 */
struct sym_tcb {
	/*
	 *  TCB header.
	 *  Assumed at offset 0.
	 */
/*0*/	struct sym_tcbh head;

	/*
	 *  LUN table used by the SCRIPTS processor.
	 *  An array of bus addresses is used on reselection.
	 */
	u32	*luntbl;	/* LCBs bus address table	*/

	/*
	 *  LUN table used by the C code.
	 */
	lcb_p	lun0p;		/* LCB of LUN #0 (usual case)	*/
#if SYM_CONF_MAX_LUN > 1
	lcb_p	*lunmp;		/* Other LCBs [1..MAX_LUN]	*/
#endif

	/*
	 *  Bitmap that tells about LUNs that succeeded at least 
	 *  1 IO and therefore assumed to be a real device.
	 *  Avoid useless allocation of the LCB structure.
	 */
	u32	lun_map[(SYM_CONF_MAX_LUN+31)/32];

	/*
	 *  Bitmap that tells about LUNs that haven't yet an LCB 
	 *  allocated (not discovered or LCB allocation failed).
	 */
	u32	busy0_map[(SYM_CONF_MAX_LUN+31)/32];

#ifdef	SYM_HAVE_STCB
	/*
	 *  O/S specific data structure.
	 */
	struct sym_stcb s;
#endif

	/*
	 *  Transfer capabilities (SIP)
	 */
	struct sym_tinfo tinfo;

	/*
	 * Keep track of the CCB used for the negotiation in order
	 * to ensure that only 1 negotiation is queued at a time.
	 */
	ccb_p   nego_cp;	/* CCB used for the nego		*/

	/*
	 *  Set when we want to reset the device.
	 */
	u_char	to_reset;

	/*
	 *  Other user settable limits and options.
	 *  These limits are read from the NVRAM if present.
	 */
	u_char	usrflags;
	u_short	usrtags;

#ifdef	SYM_OPT_SNIFF_INQUIRY
	/*
	 *  Some minimal information from INQUIRY response.
	 */
	u32	cmdq_map[(SYM_CONF_MAX_LUN+31)/32];
	u_char	inq_version;
	u_char	inq_byte7;
	u_char	inq_byte56;
	u_char	inq_byte7_valid;
#endif

};

/*
 *  Global LCB HEADER.
 *
 *  Due to lack of indirect addressing on earlier NCR chips,
 *  this substructure is copied from the LCB to a global 
 *  address after selection.
 *  For SYMBIOS chips that support LOAD/STORE this copy is 
 *  not needed and thus not performed.
 */
struct sym_lcbh {
	/*
	 *  SCRIPTS address jumped by SCRIPTS on reselection.
	 *  For not probed logical units, this address points to 
	 *  SCRIPTS that deal with bad LU handling (must be at 
	 *  offset zero of the LCB for that reason).
	 */
/*0*/	u32	resel_sa;

	/*
	 *  Task (bus address of a CCB) read from SCRIPTS that points 
	 *  to the unique ITL nexus allowed to be disconnected.
	 */
	u32	itl_task_sa;

	/*
	 *  Task table bus address (read from SCRIPTS).
	 */
	u32	itlq_tbl_sa;
};

/*
 *  Logical Unit Control Block
 */
struct sym_lcb {
	/*
	 *  TCB header.
	 *  Assumed at offset 0.
	 */
/*0*/	struct sym_lcbh head;

	/*
	 *  Task table read from SCRIPTS that contains pointers to 
	 *  ITLQ nexuses. The bus address read from SCRIPTS is 
	 *  inside the header.
	 */
	u32	*itlq_tbl;	/* Kernel virtual address	*/

	/*
	 *  Busy CCBs management.
	 */
	u_short	busy_itlq;	/* Number of busy tagged CCBs	*/
	u_short	busy_itl;	/* Number of busy untagged CCBs	*/

	/*
	 *  Circular tag allocation buffer.
	 */
	u_short	ia_tag;		/* Tag allocation index		*/
	u_short	if_tag;		/* Tag release index		*/
	u_char	*cb_tags;	/* Circular tags buffer		*/

	/*
	 *  O/S specific data structure.
	 */
#ifdef	SYM_HAVE_SLCB
	struct sym_slcb s;
#endif

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	/*
	 *  Optionnaly the driver can handle device queueing, 
	 *  and requeues internally command to redo.
	 */
	SYM_QUEHEAD
		waiting_ccbq;
	SYM_QUEHEAD
		started_ccbq;
	int	num_sgood;
	u_short	started_tags;
	u_short	started_no_tag;
	u_short	started_max;
	u_short	started_limit;
#endif

#ifdef SYM_OPT_LIMIT_COMMAND_REORDERING
	/*
	 *  Optionnaly the driver can try to prevent SCSI 
	 *  IOs from being too much reordering.
	 */
	u_char		tags_si;	/* Current index to tags sum	*/
	u_short		tags_sum[2];	/* Tags sum counters		*/
	u_short		tags_since;	/* # of tags since last switch	*/
#endif

	/*
	 *  Set when we want to clear all tasks.
	 */
	u_char to_clear;

	/*
	 *  Capabilities.
	 */
	u_char	user_flags;
	u_char	curr_flags;
};

/*
 *  Action from SCRIPTS on a task.
 *  Is part of the CCB, but is also used separately to plug 
 *  error handling action to perform from SCRIPTS.
 */
struct sym_actscr {
	u32	start;		/* Jumped by SCRIPTS after selection	*/
	u32	restart;	/* Jumped by SCRIPTS on relection	*/
};

/*
 *  Phase mismatch context.
 *
 *  It is part of the CCB and is used as parameters for the 
 *  DATA pointer. We need two contexts to handle correctly the 
 *  SAVED DATA POINTER.
 */
struct sym_pmc {
	struct	sym_tblmove sg;	/* Updated interrupted SG block	*/
	u32	ret;		/* SCRIPT return address	*/
};

/*
 *  LUN control block lookup.
 *  We use a direct pointer for LUN #0, and a table of 
 *  pointers which is only allocated for devices that support 
 *  LUN(s) > 0.
 */
#if SYM_CONF_MAX_LUN <= 1
#define sym_lp(np, tp, lun) (!lun) ? (tp)->lun0p : 0
#else
#define sym_lp(np, tp, lun) \
	(!lun) ? (tp)->lun0p : (tp)->lunmp ? (tp)->lunmp[(lun)] : 0
#endif

/*
 *  Status are used by the host and the script processor.
 *
 *  The last four bytes (status[4]) are copied to the 
 *  scratchb register (declared as scr0..scr3) just after the 
 *  select/reselect, and copied back just after disconnecting.
 *  Inside the script the XX_REG are used.
 */

/*
 *  Last four bytes (script)
 */
#define  HX_REG	scr0
#define  HX_PRT	nc_scr0
#define  HS_REG	scr1
#define  HS_PRT	nc_scr1
#define  SS_REG	scr2
#define  SS_PRT	nc_scr2
#define  HF_REG	scr3
#define  HF_PRT	nc_scr3

/*
 *  Last four bytes (host)
 */
#define  host_xflags   phys.head.status[0]
#define  host_status   phys.head.status[1]
#define  ssss_status   phys.head.status[2]
#define  host_flags    phys.head.status[3]

/*
 *  Host flags
 */
#define HF_IN_PM0	1u
#define HF_IN_PM1	(1u<<1)
#define HF_ACT_PM	(1u<<2)
#define HF_DP_SAVED	(1u<<3)
#define HF_SENSE	(1u<<4)
#define HF_EXT_ERR	(1u<<5)
#define HF_DATA_IN	(1u<<6)
#ifdef SYM_CONF_IARB_SUPPORT
#define HF_HINT_IARB	(1u<<7)
#endif

/*
 *  More host flags
 */
#if	SYM_CONF_DMA_ADDRESSING_MODE == 2
#define	HX_DMAP_DIRTY	(1u<<7)
#endif

/*
 *  Global CCB HEADER.
 *
 *  Due to lack of indirect addressing on earlier NCR chips,
 *  this substructure is copied from the ccb to a global 
 *  address after selection (or reselection) and copied back 
 *  before disconnect.
 *  For SYMBIOS chips that support LOAD/STORE this copy is 
 *  not needed and thus not performed.
 */

struct sym_ccbh {
	/*
	 *  Start and restart SCRIPTS addresses (must be at 0).
	 */
/*0*/	struct sym_actscr go;

	/*
	 *  SCRIPTS jump address that deal with data pointers.
	 *  'savep' points to the position in the script responsible 
	 *  for the actual transfer of data.
	 *  It's written on reception of a SAVE_DATA_POINTER message.
	 */
	u32	savep;		/* Jump address to saved data pointer	*/
	u32	lastp;		/* SCRIPTS address at end of data	*/
#ifdef	SYM_OPT_HANDLE_DIR_UNKNOWN
	u32	wlastp;
#endif

	/*
	 *  Status fields.
	 */
	u8	status[4];
};

/*
 *  GET/SET the value of the data pointer used by SCRIPTS.
 *
 *  We must distinguish between the LOAD/STORE-based SCRIPTS 
 *  that use directly the header in the CCB, and the NCR-GENERIC 
 *  SCRIPTS that use the copy of the header in the HCB.
 */
#if	SYM_CONF_GENERIC_SUPPORT
#define sym_set_script_dp(np, cp, dp)				\
	do {							\
		if (np->features & FE_LDSTR)			\
			cp->phys.head.lastp = cpu_to_scr(dp);	\
		else						\
			np->ccb_head.lastp = cpu_to_scr(dp);	\
	} while (0)
#define sym_get_script_dp(np, cp) 				\
	scr_to_cpu((np->features & FE_LDSTR) ?			\
		cp->phys.head.lastp : np->ccb_head.lastp)
#else
#define sym_set_script_dp(np, cp, dp)				\
	do {							\
		cp->phys.head.lastp = cpu_to_scr(dp);		\
	} while (0)

#define sym_get_script_dp(np, cp) (cp->phys.head.lastp)
#endif

/*
 *  Data Structure Block
 *
 *  During execution of a ccb by the script processor, the 
 *  DSA (data structure address) register points to this 
 *  substructure of the ccb.
 */
struct sym_dsb {
	/*
	 *  CCB header.
	 *  Also assumed at offset 0 of the sym_ccb structure.
	 */
/*0*/	struct sym_ccbh head;

	/*
	 *  Phase mismatch contexts.
	 *  We need two to handle correctly the SAVED DATA POINTER.
	 *  MUST BOTH BE AT OFFSET < 256, due to using 8 bit arithmetic 
	 *  for address calculation from SCRIPTS.
	 */
	struct sym_pmc pm0;
	struct sym_pmc pm1;

	/*
	 *  Table data for Script
	 */
	struct sym_tblsel  select;
	struct sym_tblmove smsg;
	struct sym_tblmove smsg_ext;
	struct sym_tblmove cmd;
	struct sym_tblmove sense;
	struct sym_tblmove wresid;
	struct sym_tblmove data [SYM_CONF_MAX_SG];
};

/*
 *  Our Command Control Block
 */
struct sym_ccb {
	/*
	 *  This is the data structure which is pointed by the DSA 
	 *  register when it is executed by the script processor.
	 *  It must be the first entry.
	 */
	struct sym_dsb phys;

	/*
	 *  Pointer to CAM ccb and related stuff.
	 */
	cam_ccb_p cam_ccb;	/* CAM scsiio ccb		*/
	u8	cdb_buf[16];	/* Copy of CDB			*/
	u8	*sns_bbuf;	/* Bounce buffer for sense data	*/
#ifndef	SYM_SNS_BBUF_LEN
#define	SYM_SNS_BBUF_LEN (32)
#endif	
	int	data_len;	/* Total data length		*/
	int	segments;	/* Number of SG segments	*/

	u8	order;		/* Tag type (if tagged command)	*/

	/*
	 *  Miscellaneous status'.
	 */
	u_char	nego_status;	/* Negotiation status		*/
	u_char	xerr_status;	/* Extended error flags		*/
	u32	extra_bytes;	/* Extraneous bytes transferred	*/

	/*
	 *  Message areas.
	 *  We prepare a message to be sent after selection.
	 *  We may use a second one if the command is rescheduled 
	 *  due to CHECK_CONDITION or COMMAND TERMINATED.
	 *  Contents are IDENTIFY and SIMPLE_TAG.
	 *  While negotiating sync or wide transfer,
	 *  a SDTR or WDTR message is appended.
	 */
	u_char	scsi_smsg [12];
	u_char	scsi_smsg2[12];

	/*
	 *  Auto request sense related fields.
	 */
	u_char	sensecmd[6];	/* Request Sense command	*/
	u_char	sv_scsi_status;	/* Saved SCSI status 		*/
	u_char	sv_xerr_status;	/* Saved extended status	*/
	int	sv_resid;	/* Saved residual		*/

	/*
	 *  O/S specific data structure.
	 */
#ifdef	SYM_HAVE_SCCB
	struct sym_sccb s;
#endif
	/*
	 *  Other fields.
	 */
#ifdef	SYM_OPT_HANDLE_IO_TIMEOUT
	SYM_QUEHEAD tmo_linkq;	/* Optional timeout handling	*/
	u_int	tmo_clock;	/* (link and dealine value)	*/
#endif
	u32	ccb_ba;		/* BUS address of this CCB	*/
	u_short	tag;		/* Tag for this transfer	*/
				/*  NO_TAG means no tag		*/
	u_char	target;
	u_char	lun;
	ccb_p	link_ccbh;	/* Host adapter CCB hash chain	*/
	SYM_QUEHEAD
		link_ccbq;	/* Link to free/busy CCB queue	*/
	u32	startp;		/* Initial data pointer		*/
	u32	goalp;		/* Expected last data pointer	*/
#ifdef	SYM_OPT_HANDLE_DIR_UNKNOWN
	u32	wgoalp;
#endif
	int	ext_sg;		/* Extreme data pointer, used	*/
	int	ext_ofs;	/*  to calculate the residual.	*/
#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	SYM_QUEHEAD
		link2_ccbq;	/* Link for device queueing	*/
	u_char	started;	/* CCB queued to the squeue	*/
#endif
	u_char	to_abort;	/* Want this IO to be aborted	*/
#ifdef SYM_OPT_LIMIT_COMMAND_REORDERING
	u_char	tags_si;	/* Lun tags sum index (0,1)	*/
#endif
};

#define CCB_BA(cp,lbl)	(cp->ccb_ba + offsetof(struct sym_ccb, lbl))

#ifdef	SYM_OPT_HANDLE_DIR_UNKNOWN
#define	sym_goalp(cp) ((cp->host_flags & HF_DATA_IN) ? cp->goalp : cp->wgoalp)
#else
#define	sym_goalp(cp) (cp->goalp)
#endif

/*
 *  Host Control Block
 */
struct sym_hcb {
	/*
	 *  Global headers.
	 *  Due to poorness of addressing capabilities, earlier 
	 *  chips (810, 815, 825) copy part of the data structures 
	 *  (CCB, TCB and LCB) in fixed areas.
	 */
#if	SYM_CONF_GENERIC_SUPPORT
	struct sym_ccbh	ccb_head;
	struct sym_tcbh	tcb_head;
	struct sym_lcbh	lcb_head;
#endif
	/*
	 *  Idle task and invalid task actions and 
	 *  their bus addresses.
	 */
	struct sym_actscr idletask, notask, bad_itl, bad_itlq;
	u32 idletask_ba, notask_ba, bad_itl_ba, bad_itlq_ba;

	/*
	 *  Dummy lun table to protect us against target 
	 *  returning bad lun number on reselection.
	 */
	u32	*badluntbl;	/* Table physical address	*/
	u32	badlun_sa;	/* SCRIPT handler BUS address	*/

	/*
	 *  Bus address of this host control block.
	 */
	u32	hcb_ba;

	/*
	 *  Bit 32-63 of the on-chip RAM bus address in LE format.
	 *  The START_RAM64 script loads the MMRS and MMWS from this 
	 *  field.
	 */
	u32	scr_ram_seg;

	/*
	 *  Initial value of some IO register bits.
	 *  These values are assumed to have been set by BIOS, and may 
	 *  be used to probe adapter implementation differences.
	 */
	u_char	sv_scntl0, sv_scntl3, sv_dmode, sv_dcntl, sv_ctest3, sv_ctest4,
		sv_ctest5, sv_gpcntl, sv_stest2, sv_stest4, sv_scntl4,
		sv_stest1;

	/*
	 *  Actual initial value of IO register bits used by the 
	 *  driver. They are loaded at initialisation according to  
	 *  features that are to be enabled/disabled.
	 */
	u_char	rv_scntl0, rv_scntl3, rv_dmode, rv_dcntl, rv_ctest3, rv_ctest4, 
		rv_ctest5, rv_stest2, rv_ccntl0, rv_ccntl1, rv_scntl4;

	/*
	 *  Target data.
	 */
	struct sym_tcb	target[SYM_CONF_MAX_TARGET];

	/*
	 *  Target control block bus address array used by the SCRIPT 
	 *  on reselection.
	 */
	u32		*targtbl;
	u32		targtbl_ba;

	/*
	 *  DMA pool handle for this HBA.
	 */
#ifdef	SYM_OPT_BUS_DMA_ABSTRACTION
	m_pool_ident_t	bus_dmat;
#endif

	/*
	 *  O/S specific data structure
	 */
	struct sym_shcb s;

	/*
	 *  Physical bus addresses of the chip.
	 */
	u32		mmio_ba;	/* MMIO 32 bit BUS address	*/
	int		mmio_ws;	/* MMIO Window size		*/

	u32		ram_ba;		/* RAM 32 bit BUS address	*/
	int		ram_ws;		/* RAM window size		*/

	/*
	 *  SCRIPTS virtual and physical bus addresses.
	 *  'script'  is loaded in the on-chip RAM if present.
	 *  'scripth' stays in main memory for all chips except the 
	 *  53C895A, 53C896 and 53C1010 that provide 8K on-chip RAM.
	 */
	u_char		*scripta0;	/* Copy of scripts A, B, Z	*/
	u_char		*scriptb0;
	u_char		*scriptz0;
	u32		scripta_ba;	/* Actual scripts A, B, Z	*/
	u32		scriptb_ba;	/* 32 bit bus addresses.	*/
	u32		scriptz_ba;
	u_short		scripta_sz;	/* Actual size of script A, B, Z*/
	u_short		scriptb_sz;
	u_short		scriptz_sz;

	/*
	 *  Bus addresses, setup and patch methods for 
	 *  the selected firmware.
	 */
	struct sym_fwa_ba fwa_bas;	/* Useful SCRIPTA bus addresses	*/
	struct sym_fwb_ba fwb_bas;	/* Useful SCRIPTB bus addresses	*/
	struct sym_fwz_ba fwz_bas;	/* Useful SCRIPTZ bus addresses	*/
	void		(*fw_setup)(hcb_p np, struct sym_fw *fw);
	void		(*fw_patch)(hcb_p np);
	char		*fw_name;

	/*
	 *  General controller parameters and configuration.
	 */
	u_short	device_id;	/* PCI device id		*/
	u_char	revision_id;	/* PCI device revision id	*/
	u_int	features;	/* Chip features map		*/
	u_char	myaddr;		/* SCSI id of the adapter	*/
	u_char	maxburst;	/* log base 2 of dwords burst	*/
	u_char	maxwide;	/* Maximum transfer width	*/
	u_char	minsync;	/* Min sync period factor (ST)	*/
	u_char	maxsync;	/* Max sync period factor (ST)	*/
	u_char	maxoffs;	/* Max scsi offset        (ST)	*/
	u_char	minsync_dt;	/* Min sync period factor (DT)	*/
	u_char	maxsync_dt;	/* Max sync period factor (DT)	*/
	u_char	maxoffs_dt;	/* Max scsi offset        (DT)	*/
	u_char	multiplier;	/* Clock multiplier (1,2,4)	*/
	u_char	clock_divn;	/* Number of clock divisors	*/
	u32	clock_khz;	/* SCSI clock frequency in KHz	*/
	u32	pciclk_khz;	/* Estimated PCI clock  in KHz	*/
	/*
	 *  Start queue management.
	 *  It is filled up by the host processor and accessed by the 
	 *  SCRIPTS processor in order to start SCSI commands.
	 */
	volatile		/* Prevent code optimizations	*/
	u32	*squeue;	/* Start queue virtual address	*/
	u32	squeue_ba;	/* Start queue BUS address	*/
	u_short	squeueput;	/* Next free slot of the queue	*/
	u_short	actccbs;	/* Number of allocated CCBs	*/

	/*
	 *  Command completion queue.
	 *  It is the same size as the start queue to avoid overflow.
	 */
	u_short	dqueueget;	/* Next position to scan	*/
	volatile		/* Prevent code optimizations	*/
	u32	*dqueue;	/* Completion (done) queue	*/
	u32	dqueue_ba;	/* Done queue BUS address	*/

	/*
	 *  Miscellaneous buffers accessed by the scripts-processor.
	 *  They shall be DWORD aligned, because they may be read or 
	 *  written with a script command.
	 */
	u_char		msgout[8];	/* Buffer for MESSAGE OUT 	*/
	u_char		msgin [8];	/* Buffer for MESSAGE IN	*/
	u32		lastmsg;	/* Last SCSI message sent	*/
	u32		scratch;	/* Scratch for SCSI receive	*/
					/* Also used for cache test 	*/
	/*
	 *  Miscellaneous configuration and status parameters.
	 */
	u_char		usrflags;	/* Miscellaneous user flags	*/
	u_char		scsi_mode;	/* Current SCSI BUS mode	*/
	u_char		verbose;	/* Verbosity for this controller*/

	/*
	 *  CCB lists and queue.
	 */
	ccb_p *ccbh;			/* CCBs hashed by DSA value	*/
					/* CCB_HASH_SIZE lists of CCBs	*/
	SYM_QUEHEAD	free_ccbq;	/* Queue of available CCBs	*/
	SYM_QUEHEAD	busy_ccbq;	/* Queue of busy CCBs		*/

	/*
	 *  During error handling and/or recovery,
	 *  active CCBs that are to be completed with 
	 *  error or requeued are moved from the busy_ccbq
	 *  to the comp_ccbq prior to completion.
	 */
	SYM_QUEHEAD	comp_ccbq;

#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
	SYM_QUEHEAD	dummy_ccbq;
#endif
	/*
	 *  Optional handling of IO timeouts.
	 */
#ifdef	SYM_OPT_HANDLE_IO_TIMEOUT
	SYM_QUEHEAD tmo0_ccbq;
	SYM_QUEHEAD *tmo_ccbq;	/* [2*SYM_TIMEOUT_ORDER_MAX] */
	u_int	tmo_clock;
	u_int	tmo_actq;
#endif

	/*
	 *  IMMEDIATE ARBITRATION (IARB) control.
	 *
	 *  We keep track in 'last_cp' of the last CCB that has been 
	 *  queued to the SCRIPTS processor and clear 'last_cp' when 
	 *  this CCB completes. If last_cp is not zero at the moment 
	 *  we queue a new CCB, we set a flag in 'last_cp' that is 
	 *  used by the SCRIPTS as a hint for setting IARB.
	 *  We donnot set more than 'iarb_max' consecutive hints for 
	 *  IARB in order to leave devices a chance to reselect.
	 *  By the way, any non zero value of 'iarb_max' is unfair. :)
	 */
#ifdef SYM_CONF_IARB_SUPPORT
	u_short		iarb_max;	/* Max. # consecutive IARB hints*/
	u_short		iarb_count;	/* Actual # of these hints	*/
	ccb_p		last_cp;
#endif

	/*
	 *  Command abort handling.
	 *  We need to synchronize tightly with the SCRIPTS 
	 *  processor in order to handle things correctly.
	 */
	u_char		abrt_msg[4];	/* Message to send buffer	*/
	struct sym_tblmove abrt_tbl;	/* Table for the MOV of it 	*/
	struct sym_tblsel  abrt_sel;	/* Sync params for selection	*/
	u_char		istat_sem;	/* Tells the chip to stop (SEM)	*/

	/*
	 *  64 bit DMA handling.
	 */
#if	SYM_CONF_DMA_ADDRESSING_MODE != 0
	u_char	use_dac;		/* Use PCI DAC cycles		*/
#if	SYM_CONF_DMA_ADDRESSING_MODE == 2
	u_char	dmap_dirty;		/* Dma segments registers dirty	*/
	u32	dmap_bah[SYM_DMAP_SIZE];/* Segment registers map	*/
#endif
#endif
};

#define HCB_BA(np, lbl)	(np->hcb_ba + offsetof(struct sym_hcb, lbl))

/*
 *  NVRAM reading (sym_nvram.c).
 */
void sym_nvram_setup_host (hcb_p np, struct sym_nvram *nvram);
void sym_nvram_setup_target (hcb_p np, int target, struct sym_nvram *nvp);
int sym_read_nvram (sdev_p np, struct sym_nvram *nvp);

/*
 *  FIRMWARES (sym_fw.c)
 */
struct sym_fw * sym_find_firmware(struct sym_pci_chip *chip);
void sym_fw_bind_script (hcb_p np, u32 *start, int len);

/*
 *  Driver methods called from O/S specific code.
 */
char *sym_driver_name(void);
void sym_print_xerr(ccb_p cp, int x_status);
int sym_reset_scsi_bus(hcb_p np, int enab_int);
struct sym_pci_chip *
sym_lookup_pci_chip_table (u_short device_id, u_char revision);
void sym_put_start_queue(hcb_p np, ccb_p cp);
#ifdef SYM_OPT_HANDLE_DEVICE_QUEUEING
void sym_start_next_ccbs(hcb_p np, lcb_p lp, int maxn);
#endif
void sym_start_up (hcb_p np, int reason);
void sym_interrupt (hcb_p np);
void sym_flush_comp_queue(hcb_p np, int cam_status);
int sym_clear_tasks(hcb_p np, int cam_status, int target, int lun, int task);
ccb_p sym_get_ccb (hcb_p np, u_char tn, u_char ln, u_char tag_order);
void sym_free_ccb (hcb_p np, ccb_p cp);
lcb_p sym_alloc_lcb (hcb_p np, u_char tn, u_char ln);
int sym_queue_scsiio(hcb_p np, cam_scsiio_p csio, ccb_p cp);
int sym_abort_scsiio(hcb_p np, cam_ccb_p ccb, int timed_out);
int sym_abort_ccb(hcb_p np, ccb_p cp, int timed_out);
int sym_reset_scsi_target(hcb_p np, int target);
void sym_hcb_free(hcb_p np);

#ifdef SYM_OPT_NVRAM_PRE_READ
int sym_hcb_attach(hcb_p np, struct sym_fw *fw, struct sym_nvram *nvram);
#else
int sym_hcb_attach(hcb_p np, struct sym_fw *fw);
#endif

/*
 *  Optionnaly, the driver may handle IO timeouts.
 */
#ifdef	SYM_OPT_HANDLE_IO_TIMEOUT
int sym_abort_ccb(hcb_p np, ccb_p cp, int timed_out);
void sym_timeout_ccb(hcb_p np, ccb_p cp, u_int ticks);
static void __inline sym_untimeout_ccb(hcb_p np, ccb_p cp)
{
	sym_remque(&cp->tmo_linkq);
	sym_insque_head(&cp->tmo_linkq, &np->tmo0_ccbq);
}
void sym_clock(hcb_p np);
#endif	/* SYM_OPT_HANDLE_IO_TIMEOUT */

/*
 *  Optionnaly, the driver may provide a function
 *  to announce transfer rate changes.
 */
#ifdef	SYM_OPT_ANNOUNCE_TRANSFER_RATE
void sym_announce_transfer_rate(hcb_p np, int target);
#endif

/*
 *  Optionnaly, the driver may sniff inquiry data.
 */
#ifdef	SYM_OPT_SNIFF_INQUIRY
#define	INQ7_CMDQ	(0x02)
#define	INQ7_SYNC	(0x10)
#define	INQ7_WIDE16	(0x20)

#define INQ56_CLOCKING	(3<<2)
#define INQ56_ST_ONLY	(0<<2)
#define INQ56_DT_ONLY	(1<<2)
#define INQ56_ST_DT	(3<<2)

void sym_update_trans_settings(hcb_p np, tcb_p tp);
int  
__sym_sniff_inquiry(hcb_p np, u_char tn, u_char ln,
                    u_char *inq_data, int inq_len);
#endif


/*
 *  Build a scatter/gather entry.
 *
 *  For 64 bit systems, we use the 8 upper bits of the size field 
 *  to provide bus address bits 32-39 to the SCRIPTS processor.
 *  This allows the 895A, 896, 1010 to address up to 1 TB of memory.
 */

#if   SYM_CONF_DMA_ADDRESSING_MODE == 0
#define sym_build_sge(np, data, badd, len)	\
do {						\
	(data)->addr = cpu_to_scr(badd);	\
	(data)->size = cpu_to_scr(len);		\
} while (0)
#elif SYM_CONF_DMA_ADDRESSING_MODE == 1
#define sym_build_sge(np, data, badd, len)				\
do {									\
	(data)->addr = cpu_to_scr(badd);				\
	(data)->size = cpu_to_scr((((badd) >> 8) & 0xff000000) + len);	\
} while (0)
#elif SYM_CONF_DMA_ADDRESSING_MODE == 2
int sym_lookup_dmap(hcb_p np, u32 h, int s);
static __inline void 
sym_build_sge(hcb_p np, struct sym_tblmove *data, u64 badd, int len)
{
	u32 h = (badd>>32);
	int s = (h&SYM_DMAP_MASK);

	if (h != np->dmap_bah[s])
		goto bad;
good:
	(data)->addr = cpu_to_scr(badd);
	(data)->size = cpu_to_scr((s<<24) + len);
	return;
bad:
	s = sym_lookup_dmap(np, h, s);
	goto good;
}
#else
#error "Unsupported DMA addressing mode"
#endif

/*
 *  Set up data pointers used by SCRIPTS.
 *  Called from O/S specific code.
 */
static void __inline 
sym_setup_data_pointers(hcb_p np, ccb_p cp, int dir)
{
	u32 lastp, goalp;

	/*
	 *  No segments means no data.
	 */
	if (!cp->segments)
		dir = CAM_DIR_NONE;

	/*
	 *  Set the data pointer.
	 */
	switch(dir) {
#ifdef	SYM_OPT_HANDLE_DIR_UNKNOWN
	case CAM_DIR_UNKNOWN:
#endif
	case CAM_DIR_OUT:
		goalp = SCRIPTA_BA (np, data_out2) + 8;
		lastp = goalp - 8 - (cp->segments * (2*4));
#ifdef	SYM_OPT_HANDLE_DIR_UNKNOWN
		cp->wgoalp = cpu_to_scr(goalp);
		if (dir != CAM_DIR_UNKNOWN)
			break;
		cp->phys.head.wlastp = cpu_to_scr(lastp);
		/* fall through */
#else
		break;
#endif
	case CAM_DIR_IN:
		cp->host_flags |= HF_DATA_IN;
		goalp = SCRIPTA_BA (np, data_in2) + 8;
		lastp = goalp - 8 - (cp->segments * (2*4));
		break;
	case CAM_DIR_NONE:
	default:
#ifdef	SYM_OPT_HANDLE_DIR_UNKNOWN
		cp->host_flags |= HF_DATA_IN;
#endif
		lastp = goalp = SCRIPTB_BA (np, no_data);
		break;
	}

	/*
	 *  Set all pointers values needed by SCRIPTS.
	 */
	cp->phys.head.lastp = cpu_to_scr(lastp);
	cp->phys.head.savep = cpu_to_scr(lastp);
	cp->startp	    = cp->phys.head.savep;
	cp->goalp	    = cpu_to_scr(goalp);

#ifdef	SYM_OPT_HANDLE_DIR_UNKNOWN
	/*
	 *  If direction is unknown, start at data_io.
	 */
	if (dir == CAM_DIR_UNKNOWN)
		cp->phys.head.savep = cpu_to_scr(SCRIPTB_BA (np, data_io));
#endif
}

/*
 *  MEMORY ALLOCATOR.
 */

/*
 *  Shortest memory chunk is (1<<SYM_MEM_SHIFT), currently 16.
 *  Actual allocations happen as SYM_MEM_CLUSTER_SIZE sized.
 *  (1 PAGE at a time is just fine).
 */
#define SYM_MEM_SHIFT	4
#define SYM_MEM_CLUSTER_SIZE	(1UL << SYM_MEM_CLUSTER_SHIFT)
#define SYM_MEM_CLUSTER_MASK	(SYM_MEM_CLUSTER_SIZE-1)

/*
 *  Link between free memory chunks of a given size.
 */
typedef struct sym_m_link {
	struct sym_m_link *next;
} *m_link_p;

/*
 *  Virtual to bus physical translation for a given cluster.
 *  Such a structure is only useful with DMA abstraction.
 */
#ifdef	SYM_OPT_BUS_DMA_ABSTRACTION
typedef struct sym_m_vtob {	/* Virtual to Bus address translation */
	struct sym_m_vtob *next;
#ifdef	SYM_HAVE_M_SVTOB
	struct sym_m_svtob s;	/* OS specific data structure */
#endif
	m_addr_t	vaddr;	/* Virtual address */
	m_addr_t	baddr;	/* Bus physical address */
} *m_vtob_p;

/* Hash this stuff a bit to speed up translations */
#define VTOB_HASH_SHIFT		5
#define VTOB_HASH_SIZE		(1UL << VTOB_HASH_SHIFT)
#define VTOB_HASH_MASK		(VTOB_HASH_SIZE-1)
#define VTOB_HASH_CODE(m)	\
	((((m_addr_t) (m)) >> SYM_MEM_CLUSTER_SHIFT) & VTOB_HASH_MASK)
#endif	/* SYM_OPT_BUS_DMA_ABSTRACTION */

/*
 *  Memory pool of a given kind.
 *  Ideally, we want to use:
 *  1) 1 pool for memory we donnot need to involve in DMA.
 *  2) The same pool for controllers that require same DMA 
 *     constraints and features.
 *     The OS specific m_pool_id_t thing and the sym_m_pool_match() 
 *     method are expected to tell the driver about.
 */
typedef struct sym_m_pool {
#ifdef	SYM_OPT_BUS_DMA_ABSTRACTION
	m_pool_ident_t	dev_dmat;	/* Identifies the pool (see above) */
	m_addr_t (*get_mem_cluster)(struct sym_m_pool *);
#ifdef	SYM_MEM_FREE_UNUSED
	void (*free_mem_cluster)(struct sym_m_pool *, m_addr_t);
#endif
#define M_GET_MEM_CLUSTER()		mp->get_mem_cluster(mp)
#define M_FREE_MEM_CLUSTER(p)		mp->free_mem_cluster(mp, p)
#ifdef	SYM_HAVE_M_SPOOL
	struct sym_m_spool	s;	/* OS specific data structure */
#endif
	int nump;
	m_vtob_p vtob[VTOB_HASH_SIZE];
	struct sym_m_pool *next;
#else
#define M_GET_MEM_CLUSTER()		sym_get_mem_cluster()
#define M_FREE_MEM_CLUSTER(p)		sym_free_mem_cluster(p)
#endif	/* SYM_OPT_BUS_DMA_ABSTRACTION */
	struct sym_m_link h[SYM_MEM_CLUSTER_SHIFT - SYM_MEM_SHIFT + 1];
} *m_pool_p;

/*
 *  Alloc and free non DMAable memory.
 */
void sym_mfree_unlocked(void *ptr, int size, char *name);
void *sym_calloc_unlocked(int size, char *name);

/*
 *  Alloc, free and translate addresses to bus physical 
 *  for DMAable memory.
 */
#ifdef	SYM_OPT_BUS_DMA_ABSTRACTION
void *__sym_calloc_dma_unlocked(m_pool_ident_t dev_dmat, int size, char *name);
void 
__sym_mfree_dma_unlocked(m_pool_ident_t dev_dmat, void *m,int size, char *name);
u32 __vtobus_unlocked(m_pool_ident_t dev_dmat, void *m);
#endif

/*
 * Verbs used by the driver code for DMAable memory handling.
 * The _uvptv_ macro avoids a nasty warning about pointer to volatile 
 * being discarded.
 */
#define _uvptv_(p) ((void *)((u_long)(p)))

#define _sym_calloc_dma(np, l, n)	__sym_calloc_dma(np->bus_dmat, l, n)
#define _sym_mfree_dma(np, p, l, n)	\
			__sym_mfree_dma(np->bus_dmat, _uvptv_(p), l, n)
#define sym_calloc_dma(l, n)		_sym_calloc_dma(np, l, n)
#define sym_mfree_dma(p, l, n)		_sym_mfree_dma(np, p, l, n)
#define _vtobus(np, p)			__vtobus(np->bus_dmat, _uvptv_(p))
#define vtobus(p)			_vtobus(np, p)

/*
 *  Override some function names.
 */
#define PRINT_ADDR	sym_print_addr
#define PRINT_TARGET	sym_print_target
#define PRINT_LUN	sym_print_lun
#define MDELAY		sym_mdelay
#define UDELAY		sym_udelay

#endif /* SYM_HIPD_H */
