/*
 *  linux/drivers/message/fusion/mptbase.h
 *      High performance SCSI + LAN / Fibre Channel device drivers.
 *      For use with PCI chip/adapter(s):
 *          LSIFC9xx/LSI409xx Fibre Channel
 *      running LSI Logic Fusion MPT (Message Passing Technology) firmware.
 *
 *  Credits:
 *     (see mptbase.c)
 *
 *  Copyright (c) 1999-2002 LSI Logic Corporation
 *  Originally By: Steven J. Ralston
 *  (mailto:sjralston1@netscape.net)
 *  (mailto:mpt_linux_developer@lsil.com)
 *
 *  $Id: mptbase.h,v 1.149 2003/05/07 14:08:31 pdelaney Exp $
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    NO WARRANTY
    THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
    solely responsible for determining the appropriateness of using and
    distributing the Program and assumes all risks associated with its
    exercise of rights under this Agreement, including but not limited to
    the risks and costs of program errors, damage to or loss of data,
    programs or equipment, and unavailability or interruption of operations.

    DISCLAIMER OF LIABILITY
    NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
    HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef MPTBASE_H_INCLUDED
#define MPTBASE_H_INCLUDED
/*{-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include "linux_compat.h"	/* linux-2.2.x (vs. -2.4.x) tweaks */
#include "scsi3.h"		/* SCSI defines */

#include "lsi/mpi_type.h"
#include "lsi/mpi.h"		/* Fusion MPI(nterface) basic defs */
#include "lsi/mpi_ioc.h"	/* Fusion MPT IOC(ontroller) defs */
#include "lsi/mpi_cnfg.h"	/* IOC configuration support */
#include "lsi/mpi_init.h"	/* SCSI Host (initiator) protocol support */
#include "lsi/mpi_lan.h"	/* LAN over FC protocol support */
#include "lsi/mpi_raid.h"	/* Integrated Mirroring support */

#include "lsi/mpi_fc.h"		/* Fibre Channel (lowlevel) support */
#include "lsi/mpi_targ.h"	/* SCSI/FCP Target protcol support */
#include "lsi/fc_log.h"

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef MODULEAUTHOR
#define MODULEAUTHOR	"LSI Logic Corporation"
#endif

#ifndef COPYRIGHT
#define COPYRIGHT	"Copyright (c) 1999-2003 " MODULEAUTHOR
#endif

#define MPT_LINUX_VERSION_COMMON	"2.05.11.03"
#define MPT_LINUX_PACKAGE_NAME		"@(#)mptlinux-2.05.11.03"
#define WHAT_MAGIC_STRING		"@" "(" "#" ")"

#define show_mptmod_ver(s,ver)  \
	printk(KERN_INFO "%s %s\n", s, ver);

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Fusion MPT(linux) driver configurable stuff...
 */
#define MPT_MAX_ADAPTERS		18
#define MPT_MAX_PROTOCOL_DRIVERS	16
#define MPT_MAX_BUS			1	/* Do not change */
#define MPT_MAX_FC_DEVICES		255
#define MPT_MAX_SCSI_DEVICES		16
#define MPT_LAST_LUN			255
#define MPT_SENSE_BUFFER_ALLOC		64
	/* allow for 256 max sense alloc, but only 255 max request */
#if MPT_SENSE_BUFFER_ALLOC >= 256
#	undef MPT_SENSE_BUFFER_ALLOC
#	define MPT_SENSE_BUFFER_ALLOC	256
#	define MPT_SENSE_BUFFER_SIZE	255
#else
#	define MPT_SENSE_BUFFER_SIZE	MPT_SENSE_BUFFER_ALLOC
#endif

#define MPT_NAME_LENGTH			32

#define MPT_PROCFS_MPTBASEDIR		"mpt"
						/* chg it to "driver/fusion" ? */
#define MPT_PROCFS_SUMMARY_ALL_NODE		MPT_PROCFS_MPTBASEDIR "/summary"
#define MPT_PROCFS_SUMMARY_ALL_PATHNAME		"/proc/" MPT_PROCFS_SUMMARY_ALL_NODE
#define MPT_FW_REV_MAGIC_ID_STRING		"FwRev="

#define  MPT_MAX_REQ_DEPTH		1023
#define  MPT_DEFAULT_REQ_DEPTH		256
#define  MPT_MIN_REQ_DEPTH		128

#define  MPT_MAX_REPLY_DEPTH		MPT_MAX_REQ_DEPTH
#define  MPT_DEFAULT_REPLY_DEPTH	128
#define  MPT_MIN_REPLY_DEPTH		8
#define  MPT_MAX_REPLIES_PER_ISR	32

#define  MPT_MAX_FRAME_SIZE		128
#define  MPT_DEFAULT_FRAME_SIZE		128

#define  MPT_REPLY_FRAME_SIZE		0x40  /* Must be a multiple of 8 */

#define  MPT_SG_REQ_128_SCALE		1
#define  MPT_SG_REQ_96_SCALE		2
#define  MPT_SG_REQ_64_SCALE		4

#define	 CAN_SLEEP			1
#define  NO_SLEEP			0

#define MPT_COALESCING_TIMEOUT		0x10

/*
 * SCSI transfer rate defines.
 */
#define MPT_ULTRA320			0x08
#define MPT_ULTRA160			0x09
#define MPT_ULTRA2			0x0A
#define MPT_ULTRA			0x0C
#define MPT_FAST			0x19
#define MPT_SCSI			0x32
#define MPT_ASYNC			0xFF

#define MPT_NARROW			0
#define MPT_WIDE			1

#ifdef __KERNEL__	/* { */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <linux/proc_fs.h>

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 * Attempt semi-consistent error & warning msgs across
 * MPT drivers.  NOTE: Users of these macro defs must
 * themselves define their own MYNAM.
 */
#define MYIOC_s_INFO_FMT		KERN_INFO MYNAM ": %s: "
#define MYIOC_s_NOTE_FMT		KERN_NOTICE MYNAM ": %s: "
#define MYIOC_s_WARN_FMT		KERN_WARNING MYNAM ": %s: WARNING - "
#define MYIOC_s_ERR_FMT			KERN_ERR MYNAM ": %s: ERROR - "

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  MPT protocol driver defs...
 */
typedef enum {
	MPTBASE_DRIVER,		/* MPT base class */
	MPTCTL_DRIVER,		/* MPT ioctl class */
	MPTSCSIH_DRIVER,	/* MPT SCSI host (initiator) class */
	MPTLAN_DRIVER,		/* MPT LAN class */
	MPTSTM_DRIVER,		/* MPT SCSI target mode class */
	MPTDMP_DRIVER,		/* MPT Dynamic Multi-pathing class */
	MPTUNKNOWN_DRIVER
} MPT_DRIVER_CLASS;

/*
 *  MPT adapter / port / bus / device info structures...
 */

typedef union _MPT_FRAME_TRACKER {
	struct {
		struct _MPT_FRAME_HDR	*forw;
		struct _MPT_FRAME_HDR	*back;
		u32			 arg1;
		u32			 pad;
		void			*argp1;
#ifndef MPT_SCSI_USE_NEW_EH
		void			*argp2;
#endif
	} linkage;
	/*
	 * NOTE: When request frames are free, on the linkage structure
	 * contets are valid.  All other values are invalid.
	 * In particular, do NOT reply on offset [2]
	 * (in words) being the * message context.
	 * The message context must be reset (computed via base address
	 * + an offset) prior to issuing any command.
	 *
	 * NOTE2: On non-32-bit systems, where pointers are LARGE,
	 * using the linkage pointers destroys our sacred MsgContext
	 * field contents.  But we don't care anymore because these
	 * are now reset in mpt_put_msg_frame() just prior to sending
	 * a request off to the IOC.
	 */
	struct {
		u32 __hdr[2];
		/*
		 * The following _MUST_ match the location of the
		 * MsgContext field in the MPT message headers.
		 */
		union {
			u32		 MsgContext;
			struct {
				u16	 req_idx;	/* Request index */
				u8	 cb_idx;	/* callback function index */
				u8	 rsvd;
			} fld;
		} msgctxu;
	} hwhdr;
	/*
	 * Remark: 32 bit identifier:
	 *  31-24: reserved
	 *  23-16: call back index
	 *  15-0 : request index
	 */
} MPT_FRAME_TRACKER;

/*
 *  We might want to view/access a frame as:
 *    1) generic request header
 *    2) SCSIIORequest
 *    3) SCSIIOReply
 *    4) MPIDefaultReply
 *    5) frame tracker
 */
typedef struct _MPT_FRAME_HDR {
	union {
		MPIHeader_t		hdr;
		SCSIIORequest_t		scsireq;
		SCSIIOReply_t		sreply;
		ConfigReply_t		configreply;
		MPIDefaultReply_t	reply;
		MPT_FRAME_TRACKER	frame;
	} u;
} MPT_FRAME_HDR;

#define MPT_REQ_MSGFLAGS_DROPME		0x80

/* Used for tracking the free request frames
 * and free reply frames.
 */
typedef struct _MPT_Q_TRACKER {
	MPT_FRAME_HDR	*head;
	MPT_FRAME_HDR	*tail;
} MPT_Q_TRACKER;


typedef struct _MPT_SGL_HDR {
	SGESimple32_t	 sge[1];
} MPT_SGL_HDR;

typedef struct _MPT_SGL64_HDR {
	SGESimple64_t	 sge[1];
} MPT_SGL64_HDR;


typedef struct _Q_ITEM {
	struct _Q_ITEM	*forw;
	struct _Q_ITEM	*back;
} Q_ITEM;

typedef struct _Q_TRACKER {
	struct _Q_ITEM	*head;
	struct _Q_ITEM	*tail;
} Q_TRACKER;

typedef struct _MPT_DONE_Q {
	struct _MPT_DONE_Q	*forw;
	struct _MPT_DONE_Q	*back;
	void			*argp;
} MPT_DONE_Q;

typedef struct _DONE_Q_TRACKER {
	MPT_DONE_Q	*head;
	MPT_DONE_Q	*tail;
} DONE_Q_TRACKER;

/*
 *  Chip-specific stuff... FC929 delineates break between
 *  FC and Parallel SCSI parts. Do NOT re-order.
 */

typedef enum {
	FC919X = 0x0819,
	FC929X = 0x0829,
	FC909 = 0x0909,
	FC919 = 0x0919,
	FC929 = 0x0929,
	C1030 = 0x1030,
	C1035 = 0x1035,
	FCUNK = 0xFBAD
} CHIP_TYPE;

/*
 *  System interface register set
 */

typedef struct _SYSIF_REGS
{
	u32	Doorbell;	/* 00     System<->IOC Doorbell reg  */
	u32	WriteSequence;	/* 04     Write Sequence register    */
	u32	Diagnostic;	/* 08     Diagnostic register        */
	u32	TestBase;	/* 0C     Test Base Address          */
	u32	DiagRwData;	/* 10     Read Write Data (fw download)   */
	u32	DiagRwAddress;	/* 14     Read Write Address (fw download)*/
	u32	Reserved1[6];	/* 18-2F  reserved for future use    */
	u32	IntStatus;	/* 30     Interrupt Status           */
	u32	IntMask;	/* 34     Interrupt Mask             */
	u32	Reserved2[2];	/* 38-3F  reserved for future use    */
	u32	RequestFifo;	/* 40     Request Post/Free FIFO     */
	u32	ReplyFifo;	/* 44     Reply   Post/Free FIFO     */
	u32	Reserved3[2];	/* 48-4F  reserved for future use    */
	u32	HostIndex;	/* 50     Host Index register        */
	u32	Reserved4[15];	/* 54-8F                             */
	u32	Fubar;		/* 90     For Fubar usage            */
	u32	Reserved5[27];	/* 94-FF                             */
} SYSIF_REGS;

/*
 * NOTE: Use MPI_{DOORBELL,WRITESEQ,DIAG}_xxx defs in lsi/mpi.h
 * in conjunction with SYSIF_REGS accesses!
 */


/*
 *	Dynamic Multi-Pathing specific stuff...
 */
#define DMP_MAX_PATHS	8

typedef struct _PathInfo {
	u8		 ioc;
	u8		 target;
	u8		 pad;
	u8		 pflags;
} PathInfo;

#define PATHINFO_FLAGS_OWNED		0x01
#define PATHINFO_FLAGS_EXISTS		0x02
#define PATHINFO_FLAGS_AVAILABLE	0x04
#define PATHINFO_FLAGS_SECONDARY	0x08

#define PFLAGS_EXISTS_AND_AVAIL		(PATHINFO_FLAGS_EXISTS|PATHINFO_FLAGS_AVAILABLE)
#define PFLAGS_AVAIL_AND_OWNED		(PATHINFO_FLAGS_AVAILABLE|PATHINFO_FLAGS_OWNED)

typedef struct _ScsiCmndTracker {
	void			*head;
	void			*tail;
} ScsiCmndTracker;


/*
 *	VirtDevice - FC LUN device or SCSI target device
 *	(used to be FCSCSI_TARGET)
 */
typedef struct _VirtDevice {
	struct _VirtDevice	*forw;
	struct _VirtDevice	*back;
	struct scsi_device	*device;
	rwlock_t		 VdevLock;
	int			 ref_cnt;
	u8			 tflags;
	u8			 ioc_id;
	u8			 target_id;
	u8			 bus_id;
	u8			 minSyncFactor;	/* 0xFF is async */
	u8			 maxOffset;	/* 0 if async */
	u8			 maxWidth;	/* 0 if narrow, 1 if wide*/
	u8			 negoFlags;	/* bit field, 0 if WDTR/SDTR/QAS allowed */
	u8			 raidVolume;	/* set, if RAID Volume */
	u8			 type;		/* byte 0 of Inquiry data */
	u8			 cflags;	/* controller flags */
	u8			 rsvd1raid;
	int			 npaths;
	u16			 fc_phys_lun;
	u16			 fc_xlat_lun;
	int			 stall_detected;
	PathInfo		 path[DMP_MAX_PATHS];
	struct timer_list	 stall_timer;
	struct timer_list	 retry_timer;
	struct timer_list	 gone_timer;
	ScsiCmndTracker		 WaitQ;
	ScsiCmndTracker		 SentQ;
	ScsiCmndTracker		 DoneQ;
	u32			 num_luns;
	u32			 luns[8];		/* Max LUNs is 256 */
	u8			 pad[4];
	u8			 inq_data[8];
		/* IEEE Registered Extended Identifier
		   obtained via INQUIRY VPD page 0x83 */
		/* NOTE: Do not separate uniq_prepad and uniq_data
		   as they are treateed as a single entity in the code */
	u8			 uniq_prepad[8];
	u8			 uniq_data[20];
	u8			 pad2[4];
} VirtDevice;

/*
 *  Fibre Channel (SCSI) target device and associated defines...
 */
#define MPT_TARGET_DEFAULT_DV_STATUS	0
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,55)
#define MPT_TARGET_FLAGS_CONFIGURED	0x02
#define MPT_TARGET_FLAGS_Q_YES		0x08
#else
#define MPT_TARGET_FLAGS_VALID_NEGO	0x01
#define MPT_TARGET_FLAGS_VALID_INQUIRY	0x02
#define MPT_TARGET_FLAGS_Q_YES		0x08
#define MPT_TARGET_FLAGS_VALID_56	0x10
#define MPT_TARGET_FLAGS_SAF_TE_ISSUED	0x20
#endif

#define MPT_TARGET_NO_NEGO_WIDE		0x01
#define MPT_TARGET_NO_NEGO_SYNC		0x02
#define MPT_TARGET_NO_NEGO_QAS		0x04

typedef struct _VirtDevTracker {
	struct _VirtDevice	*head;
	struct _VirtDevice	*tail;
	rwlock_t		 VlistLock;
	int			 pad;
} VirtDevTracker;


/*
 *	/proc/mpt interface
 */
typedef struct {
	const char	*name;
	mode_t		 mode;
	int		 pad;
	read_proc_t	*read_proc;
	write_proc_t	*write_proc;
} mpt_proc_entry_t;

#define MPT_PROC_READ_RETURN(buf,start,offset,request,eof,len) \
do { \
	len -= offset;			\
	if (len < request) {		\
		*eof = 1;		\
		if (len <= 0)		\
			return 0;	\
	} else				\
		len = request;		\
	*start = buf + offset;		\
	return len;			\
} while (0)


/*
 *	IOCTL structure and associated defines
 */

#define MPT_IOCTL_STATUS_DID_IOCRESET	0x01	/* IOC Reset occurred on the current*/
#define MPT_IOCTL_STATUS_RF_VALID	0x02	/* The Reply Frame is VALID */
#define MPT_IOCTL_STATUS_TIMER_ACTIVE	0x04	/* The timer is running */
#define MPT_IOCTL_STATUS_SENSE_VALID	0x08	/* Sense data is valid */
#define MPT_IOCTL_STATUS_COMMAND_GOOD	0x10	/* Command Status GOOD */
#define MPT_IOCTL_STATUS_TMTIMER_ACTIVE	0x20	/* The TM timer is running */
#define MPT_IOCTL_STATUS_TM_FAILED	0x40	/* User TM request failed */

#define MPTCTL_RESET_OK			0x01	/* Issue Bus Reset */

typedef struct _MPT_IOCTL {
	struct _MPT_ADAPTER	*ioc;
	struct timer_list	 timer;		/* timer function for this adapter */
	u8			 ReplyFrame[MPT_DEFAULT_FRAME_SIZE];	/* reply frame data */
	u8			 sense[MPT_SENSE_BUFFER_ALLOC];
	int			 wait_done;	/* wake-up value for this ioc */
	u8			 rsvd;
	u8			 status;	/* current command status */
	u8			 reset;		/* 1 if bus reset allowed */
	u8			 target;	/* target for reset */
	void 			*tmPtr;
	struct timer_list	 TMtimer;	/* timer function for this adapter */
} MPT_IOCTL;

/*
 *  Event Structure and define
 */
#define MPTCTL_EVENT_LOG_SIZE		(0x0000000A)
typedef struct _mpt_ioctl_events {
	u32	event;		/* Specified by define above */
	u32	eventContext;	/* Index or counter */
	int	data[2];	/* First 8 bytes of Event Data */
} MPT_IOCTL_EVENTS;

/*
 * CONFIGPARM status  defines
 */
#define MPT_CONFIG_GOOD		MPI_IOCSTATUS_SUCCESS
#define MPT_CONFIG_ERROR	0x002F

/*
 *	Substructure to store SCSI specific configuration page data
 */
						/* dvStatus defines: */
#define MPT_SCSICFG_NEGOTIATE		0x01	/* Negotiate on next IO */
#define MPT_SCSICFG_NEED_DV		0x02	/* Schedule DV */
#define MPT_SCSICFG_DV_PENDING		0x04	/* DV on this physical id pending */
#define MPT_SCSICFG_DV_NOT_DONE		0x08	/* DV has not been performed */
#define MPT_SCSICFG_BLK_NEGO		0x10	/* WriteSDP1 with WDTR and SDTR disabled */
#define MPT_SCSICFG_RELOAD_IOC_PG3	0x20	/* IOC Pg 3 data is obsolete */
						/* Args passed to writeSDP1: */
#define MPT_SCSICFG_USE_NVRAM		0x01	/* WriteSDP1 using NVRAM */
#define MPT_SCSICFG_ALL_IDS		0x02	/* WriteSDP1 to all IDS */
/* #define MPT_SCSICFG_BLK_NEGO		0x10	   WriteSDP1 with WDTR and SDTR disabled */

typedef	struct _ScsiCfgData {
	u32		 PortFlags;
	int		*nvram;			/* table of device NVRAM values */
	IOCPage3_t	*pIocPg3;		/* table of physical disks */
	IOCPage4_t	*pIocPg4;		/* SEP devices addressing */
	dma_addr_t	 IocPg4_dma;		/* Phys Addr of IOCPage4 data */
	int		 IocPg4Sz;		/* IOCPage4 size */
	u8		 dvStatus[MPT_MAX_SCSI_DEVICES];
	int		 isRaid;		/* bit field, 1 if RAID */
	u8		 minSyncFactor;		/* 0xFF if async */
	u8		 maxSyncOffset;		/* 0 if async */
	u8		 maxBusWidth;		/* 0 if narrow, 1 if wide */
	u8		 busType;		/* SE, LVD, HD */
	u8		 sdp1version;		/* SDP1 version */
	u8		 sdp1length;		/* SDP1 length  */
	u8		 sdp0version;		/* SDP0 version */
	u8		 sdp0length;		/* SDP0 length  */
	u8		 dvScheduled;		/* 1 if scheduled */
	u8		 forceDv;		/* 1 to force DV scheduling */
	u8		 noQas;			/* Disable QAS for this adapter */
	u8		 Saf_Te;		/* 1 to force all Processors as SAF-TE if Inquiry data length is too short to check for SAF-TE */
	u8		 rsvd[1];
} ScsiCfgData;

typedef struct _fw_image {
	char		*fw;
	dma_addr_t	 fw_dma;
	u32		 size;
	u32		 rsvd;
} fw_image_t;

/*
 *  Adapter Structure - pci_dev specific. Maximum: MPT_MAX_ADAPTERS
 */
typedef struct _MPT_ADAPTER
{
	struct _MPT_ADAPTER	*forw;
	struct _MPT_ADAPTER	*back;
	int			 id;		/* Unique adapter id N {0,1,2,...} */
	int			 pci_irq;	/* This irq           */
	char			 name[MPT_NAME_LENGTH];	/* "iocN"             */
	char			*prod_name;	/* "LSIFC9x9"         */
	volatile SYSIF_REGS	*chip;		/* == c8817000 (mmap) */
	volatile SYSIF_REGS	*pio_chip;	/* Programmed IO (downloadboot) */
	u32			 mem_phys;	/* == f4020000 (mmap) */
	u32			 pio_mem_phys;	/* Programmed IO (downloadboot) */
	int			 mem_size;	/* mmap memory size */
	int			 alloc_total;
	u32			 last_state;
	int			 active;
	u8			*reply_alloc;	/* Reply frames alloc ptr */
	dma_addr_t		 reply_alloc_dma;
	MPT_FRAME_HDR		*reply_frames;	/* Reply msg frames - rounded up! */
	dma_addr_t		 reply_frames_dma;
	u32			 reply_frames_low_dma;
	int			 reply_depth;	/* Num Allocated reply frames */
	int			 reply_sz;	/* Reply frame size */
	CHIP_TYPE		 chip_type;
		/* We (host driver) get to manage our own RequestQueue! */
	u8			*req_alloc;	/* Request frames alloc ptr */
	dma_addr_t		 req_alloc_dma;
	MPT_FRAME_HDR		*req_frames;	/* Request msg frames - rounded up! */
	dma_addr_t		 req_frames_dma;
	u32			 req_frames_low_dma;
	int			 req_depth;	/* Number of request frames */
	int			 req_sz;	/* Request frame size (bytes) */
	spinlock_t		 FreeQlock;
	MPT_Q_TRACKER		 FreeQ;
		/* Pool of SCSI sense buffers for commands coming from
		 * the SCSI mid-layer.  We have one 256 byte sense buffer
		 * for each REQ entry.
		 */
	u8			*sense_buf_pool;
	dma_addr_t		 sense_buf_pool_dma;
	u32			 sense_buf_low_dma;
	int			 mtrr_reg;
	struct pci_dev		*pcidev;	/* struct pci_dev pointer */
	u8			*memmap;	/* mmap address */
	struct Scsi_Host	*sh;		/* Scsi Host pointer */
	ScsiCfgData		spi_data;	/* Scsi config. data */
	MPT_IOCTL		*ioctl;		/* ioctl data pointer */
	struct proc_dir_entry	*ioc_dentry;
	struct _MPT_ADAPTER	*alt_ioc;	/* ptr to 929 bound adapter port */
	spinlock_t		 diagLock;	/* diagnostic reset lock */
	int			 diagPending;
	u32			 biosVersion;	/* BIOS version from IO Unit Page 2 */
	int			 eventTypes;	/* Event logging parameters */
	int			 eventContext;	/* Next event context */
	int			 eventLogSize;	/* Max number of cached events */
#ifdef MPTSCSIH_DBG_TIMEOUT
	int			timeout_hard;
	int			timeout_delta;
	int			timeout_cnt;
	int			timeout_maxcnt;
#endif
	struct _mpt_ioctl_events *events;	/* pointer to event log */
	fw_image_t		**cached_fw;	/* Pointer to FW SG List */
	Q_TRACKER		 configQ;	/* linked list of config. requests */
	int			 num_fw_frags;	/* Number of SGE in FW SG List */
	int			 hs_reply_idx;
#ifndef MFCNT
	u32			 pad0;
#else
	u32			 mfcnt;
#endif
	u32			 hs_req[MPT_MAX_FRAME_SIZE/sizeof(u32)];
	u16			 hs_reply[MPT_MAX_FRAME_SIZE/sizeof(u16)];
	IOCFactsReply_t		 facts;
	PortFactsReply_t	 pfacts[2];
	FCPortPage0_t		 fc_port_page0[2];
	LANPage0_t		 lan_cnfg_page0;
	LANPage1_t		 lan_cnfg_page1;
	u8			 FirstWhoInit;
	u8			 upload_fw;	/* If set, do a fw upload */
	u8			 reload_fw;	/* Force a FW Reload on next reset */
	u8			 pad1[5];
} MPT_ADAPTER;


typedef struct _MPT_ADAPTER_TRACKER {
	MPT_ADAPTER	*head;
	MPT_ADAPTER	*tail;
} MPT_ADAPTER_TRACKER;

/*
 *  New return value convention:
 *    1 = Ok to free associated request frame
 *    0 = not Ok ...
 */
typedef int (*MPT_CALLBACK)(MPT_ADAPTER *ioc, MPT_FRAME_HDR *req, MPT_FRAME_HDR *reply);
typedef int (*MPT_EVHANDLER)(MPT_ADAPTER *ioc, EventNotificationReply_t *evReply);
typedef int (*MPT_RESETHANDLER)(MPT_ADAPTER *ioc, int reset_phase);
/* reset_phase defs */
#define MPT_IOC_PRE_RESET		0
#define MPT_IOC_POST_RESET		1
#define MPT_IOC_SETUP_RESET		2

/*
 * Invent MPT host event (super-set of MPI Events)
 * Fitted to 1030's 64-byte [max] request frame size
 */
typedef struct _MPT_HOST_EVENT {
	EventNotificationReply_t	 MpiEvent;	/* 8 32-bit words! */
	u32				 pad[6];
	void				*next;
} MPT_HOST_EVENT;

#define MPT_HOSTEVENT_IOC_BRINGUP	0x91
#define MPT_HOSTEVENT_IOC_RECOVER	0x92

/* Define the generic types based on the size
 * of the dma_addr_t type.
 */
typedef struct _mpt_sge {
	u32		FlagsLength;
	dma_addr_t	Address;
} MptSge_t;

#define mpt_addr_size() \
	((sizeof(dma_addr_t) == sizeof(u64)) ? MPI_SGE_FLAGS_64_BIT_ADDRESSING : \
		MPI_SGE_FLAGS_32_BIT_ADDRESSING)

#define mpt_msg_flags() \
	((sizeof(dma_addr_t) == sizeof(u64)) ? MPI_SCSIIO_MSGFLGS_SENSE_WIDTH_64 : \
		MPI_SCSIIO_MSGFLGS_SENSE_WIDTH_32)

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Funky (private) macros...
 */
#ifdef MPT_DEBUG
#define dprintk(x)  printk x
#else
#define dprintk(x)
#endif

#ifdef MPT_DEBUG_HANDSHAKE
#define dhsprintk(x)  printk x
#else
#define dhsprintk(x)
#endif

//#if defined(MPT_DEBUG) || defined(MPT_DEBUG_MSG_FRAME)
#if defined(MPT_DEBUG_MSG_FRAME)
#define dmfprintk(x)  printk x
#else
#define dmfprintk(x)
#endif

#ifdef MPT_DEBUG_IRQ
#define dirqprintk(x)  printk x
#else
#define dirqprintk(x)
#endif

#ifdef MPT_DEBUG_SG
#define dsgprintk(x)  printk x
#else
#define dsgprintk(x)
#endif

#if defined(MPT_DEBUG_DL) || defined(MPT_DEBUG)
#define ddlprintk(x)  printk x
#else
#define ddlprintk(x)
#endif


#ifdef MPT_DEBUG_DV
#define ddvprintk(x)  printk x
#else
#define ddvprintk(x)
#endif

#if defined(MPT_DEBUG_DV) || defined(MPT_DEBUG_DV_TINY)
#define ddvtprintk(x)  printk x
#else
#define ddvtprintk(x)
#endif

#ifdef MPT_DEBUG_IOCTL
#define dctlprintk(x) printk x
#else
#define dctlprintk(x)
#endif

#ifdef MPT_DEBUG_RESET
#define dtmprintk(x) printk x
#else
#define dtmprintk(x)
#endif

#ifdef MPT_DEBUG_NEH
#define nehprintk(x) printk x
#else
#define nehprintk(x)
#endif

#if defined(MPT_DEBUG_CONFIG) || defined(MPT_DEBUG)
#define dcprintk(x) printk x
#else
#define dcprintk(x)
#endif

#if defined(MPT_DEBUG_SCSI) || defined(MPT_DEBUG) || defined(MPT_DEBUG_MSG_FRAME)
#define dsprintk(x) printk x
#else
#define dsprintk(x)
#endif


#define MPT_INDEX_2_MFPTR(ioc,idx) \
	(MPT_FRAME_HDR*)( (u8*)(ioc)->req_frames + (ioc)->req_sz * (idx) )

#define MFPTR_2_MPT_INDEX(ioc,mf) \
	(int)( ((u8*)mf - (u8*)(ioc)->req_frames) / (ioc)->req_sz )

#define MPT_INDEX_2_RFPTR(ioc,idx) \
	(MPT_FRAME_HDR*)( (u8*)(ioc)->reply_frames + (ioc)->req_sz * (idx) )

#define Q_INIT(q,type)  (q)->head = (q)->tail = (type*)(q)
#define Q_IS_EMPTY(q)   ((Q_ITEM*)(q)->head == (Q_ITEM*)(q))

#define Q_ADD_TAIL(qt,i,type) { \
	Q_TRACKER	*_qt = (Q_TRACKER*)(qt); \
	Q_ITEM		*oldTail = _qt->tail; \
	(i)->forw = (type*)_qt; \
	(i)->back = (type*)oldTail; \
	oldTail->forw = (Q_ITEM*)(i); \
	_qt->tail = (Q_ITEM*)(i); \
}

#define Q_ADD_HEAD(qt,i,type) { \
	Q_TRACKER	*_qt = (Q_TRACKER*)(qt); \
	Q_ITEM		*oldHead = _qt->head; \
	(i)->forw = (type*)oldHead; \
	(i)->back = (type*)_qt; \
	oldHead->back = (Q_ITEM*)(i); \
	_qt->head = (Q_ITEM*)(i); \
}

#define Q_DEL_ITEM(i) { \
	Q_ITEM  *_forw = (Q_ITEM*)(i)->forw; \
	Q_ITEM  *_back = (Q_ITEM*)(i)->back; \
	_back->forw = _forw; \
	_forw->back = _back; \
}

#define SWAB4(value) \
	(u32)(   (((value) & 0x000000ff) << 24) \
	       | (((value) & 0x0000ff00) << 8)  \
	       | (((value) & 0x00ff0000) >> 8)  \
	       | (((value) & 0xff000000) >> 24) )


#if defined(MPT_DEBUG) || defined(MPT_DEBUG_MSG_FRAME)
#define DBG_DUMP_REPLY_FRAME(mfp) \
	{	u32 *m = (u32 *)(mfp);					\
		int  i, n = (le32_to_cpu(m[0]) & 0x00FF0000) >> 16;	\
		printk(KERN_INFO " ");					\
		for (i=0; i<n; i++)					\
			printk(" %08x", le32_to_cpu(m[i]));		\
		printk("\n");						\
	}
#define DBG_DUMP_REQUEST_FRAME_HDR(mfp) \
	{	int  i, n = 3;						\
		u32 *m = (u32 *)(mfp);					\
		printk(KERN_INFO " ");					\
		for (i=0; i<n; i++)					\
			printk(" %08x", le32_to_cpu(m[i]));		\
		printk("\n");						\
	}
#else
#define DBG_DUMP_REPLY_FRAME(mfp)
#define DBG_DUMP_REQUEST_FRAME_HDR(mfp)
#endif


/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

/*
 * MPT_SCSI_HOST defines - Used by the IOCTL and the SCSI drivers
 * Private to the driver.
 */
/* LOCAL structure and fields used when processing
 * internally generated commands. These include:
 * bus scan, dv and config requests.
 */
typedef struct _MPT_LOCAL_REPLY {
	ConfigPageHeader_t header;
	int	completion;
	u8	sense[SCSI_STD_SENSE_BYTES];
	u8	scsiStatus;
	u8	skip;
	u32	pad;
} MPT_LOCAL_REPLY;

#define MPT_HOST_BUS_UNKNOWN		(0xFF)
#define MPT_HOST_TOO_MANY_TM		(0x05)
#define MPT_HOST_NVRAM_INVALID		(0xFFFFFFFF)
#define MPT_HOST_NO_CHAIN		(0xFFFFFFFF)
#define MPT_NVRAM_MASK_TIMEOUT		(0x000000FF)
#define MPT_NVRAM_SYNC_MASK		(0x0000FF00)
#define MPT_NVRAM_SYNC_SHIFT		(8)
#define MPT_NVRAM_DISCONNECT_ENABLE	(0x00010000)
#define MPT_NVRAM_ID_SCAN_ENABLE	(0x00020000)
#define MPT_NVRAM_LUN_SCAN_ENABLE	(0x00040000)
#define MPT_NVRAM_TAG_QUEUE_ENABLE	(0x00080000)
#define MPT_NVRAM_WIDE_DISABLE		(0x00100000)
#define MPT_NVRAM_BOOT_CHOICE		(0x00200000)

#ifdef MPT_SCSI_USE_NEW_EH
/* The TM_STATE variable is used to provide strict single threading of TM
 * requests as well as communicate TM error conditions.
 */
#define TM_STATE_NONE          (0)
#define	TM_STATE_IN_PROGRESS   (1)
#define	TM_STATE_ERROR	       (2)
#endif

typedef struct _MPT_SCSI_HOST {
	MPT_ADAPTER		 *ioc;
	int			  port;
	u32			  pad0;
	struct scsi_cmnd	**ScsiLookup;
		/* Pool of buffers for chaining. ReqToChain
		 * and ChainToChain track index of chain buffers.
		 * ChainBuffer (DMA) virt/phys addresses.
		 * FreeChainQ (lock) locking mechanisms.
		 */
	int			 *ReqToChain;
	int			 *ChainToChain;
	u8			 *ChainBuffer;
	dma_addr_t		  ChainBufferDMA;
	MPT_Q_TRACKER		  FreeChainQ;
	spinlock_t		  FreeChainQlock;
	u32			  qtag_tick;
	VirtDevice		**Targets;
	MPT_LOCAL_REPLY		 *pLocal;		/* used for internal commands */
	struct timer_list	  timer;
	struct timer_list	  TMtimer;		/* Timer for TM commands ONLY */
		/* Pool of memory for holding SCpnts before doing
		 * OS callbacks. freeQ is the free pool.
		 */
	u8			 *memQ;
	DONE_Q_TRACKER		  freeQ;
	DONE_Q_TRACKER		  doneQ;		/* Holds Linux formmatted requests */
	DONE_Q_TRACKER		  pendingQ;		/* Holds MPI formmatted requests */
	MPT_Q_TRACKER		  taskQ;		/* TM request Q */
	spinlock_t		  freedoneQlock;
	int			  taskQcnt;
	int			  num_chain;		/* Number of chain buffers */
	int			  max_sge;		/* Max No of SGE*/
	u8			  numTMrequests;
	u8			  tmPending;
	u8			  resetPending;
	u8			  is_spi;		/* Parallel SCSI i/f */
	u8			  negoNvram;		/* DV disabled, nego NVRAM */
	u8			  is_multipath;		/* Multi-path compatible */
#ifdef MPT_SCSI_USE_NEW_EH
	u8                        tmState;
	u8			  rsvd[1];
#else
	u8			  rsvd[2];
#endif
	MPT_FRAME_HDR		 *tmPtr;		/* Ptr to TM request*/
	MPT_FRAME_HDR		 *cmdPtr;		/* Ptr to nonOS request */
	struct scsi_cmnd	 *abortSCpnt;
	MPT_LOCAL_REPLY		  localReply;		/* internal cmd reply struct */
	unsigned long		  hard_resets;		/* driver forced bus resets count */
	unsigned long		  soft_resets;		/* fw/external bus resets count */
	unsigned long		  timeouts;		/* cmd timeouts */
	ushort			  sel_timeout[MPT_MAX_FC_DEVICES];
} MPT_SCSI_HOST;

/*
 *	Structure for overlaying onto scsi_cmnd->SCp area
 *	NOTE: SCp area is 36 bytes min, 44 bytes max?
 */
typedef struct _scPrivate {
	struct scsi_cmnd	*forw;
	struct scsi_cmnd	*back;
	void			*p1;
	void			*p2;
	u8			 io_path_id;	/* DMP */
	u8			 pad[7];
} scPrivate;

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	More Dynamic Multi-Pathing stuff...
 */

/* Forward decl, a strange C thing, to prevent gcc compiler warnings */
struct scsi_cmnd;

/*
 *	DMP service layer structure / API interface
 */
typedef struct _DmpServices {
	VirtDevTracker	  VdevList;
	struct semaphore *Daemon;
	int		(*ScsiPathSelect)
				(struct scsi_cmnd *, MPT_SCSI_HOST **hd, int *target, int *lun);
	int		(*DmpIoDoneChk)
				(MPT_SCSI_HOST *, struct scsi_cmnd *,
				 SCSIIORequest_t *,
				 SCSIIOReply_t *);
	void		(*mptscsih_scanVlist)
				(MPT_SCSI_HOST *, int portnum);
	int		(*ScsiAbort)
				(struct scsi_cmnd *);
	int		(*ScsiBusReset)
				(struct scsi_cmnd *);
} DmpServices_t;

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 * Generic structure passed to the base mpt_config function.
 */
typedef struct _x_config_parms {
	Q_ITEM			 linkage;	/* linked list */
	struct timer_list	 timer;		/* timer function for this request  */
	ConfigPageHeader_t	*hdr;
	dma_addr_t		 physAddr;
	int			 wait_done;	/* wait for this request */
	u32			 pageAddr;	/* properly formatted */
	u8			 action;
	u8			 dir;
	u8			 timeout;	/* seconds */
	u8			 pad1;
	u16			 status;
	u16			 pad2;
} CONFIGPARMS;

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Public entry points...
 */
extern int	 mpt_register(MPT_CALLBACK cbfunc, MPT_DRIVER_CLASS dclass);
extern void	 mpt_deregister(int cb_idx);
extern int	 mpt_event_register(int cb_idx, MPT_EVHANDLER ev_cbfunc);
extern void	 mpt_event_deregister(int cb_idx);
extern int	 mpt_reset_register(int cb_idx, MPT_RESETHANDLER reset_func);
extern void	 mpt_reset_deregister(int cb_idx);
extern int	 mpt_register_ascqops_strings(void *ascqTable, int ascqtbl_sz, const char **opsTable);
extern void	 mpt_deregister_ascqops_strings(void);
extern MPT_FRAME_HDR	*mpt_get_msg_frame(int handle, int iocid);
extern void	 mpt_free_msg_frame(int handle, int iocid, MPT_FRAME_HDR *mf);
extern void	 mpt_put_msg_frame(int handle, int iocid, MPT_FRAME_HDR *mf);
extern void	 mpt_add_sge(char *pAddr, u32 flagslength, dma_addr_t dma_addr);
extern void	 mpt_add_chain(char *pAddr, u8 next, u16 length, dma_addr_t dma_addr);

extern int	 mpt_send_handshake_request(int handle, int iocid, int reqBytes, u32 *req, int sleepFlag);
extern int	 mpt_handshake_req_reply_wait(MPT_ADAPTER *ioc, int reqBytes, u32 *req, int replyBytes, u16 *u16reply, int maxwait, int sleepFlag);
extern int	 mpt_verify_adapter(int iocid, MPT_ADAPTER **iocpp);
extern MPT_ADAPTER	*mpt_adapter_find_first(void);
extern MPT_ADAPTER	*mpt_adapter_find_next(MPT_ADAPTER *prev);
extern u32	 mpt_GetIocState(MPT_ADAPTER *ioc, int cooked);
extern void	 mpt_print_ioc_summary(MPT_ADAPTER *ioc, char *buf, int *size, int len, int showlan);
extern int	 mpt_HardResetHandler(MPT_ADAPTER *ioc, int sleepFlag);
extern int	 mpt_config(MPT_ADAPTER *ioc, CONFIGPARMS *cfg);
extern void	*mpt_alloc_fw_memory(MPT_ADAPTER *ioc, int size, int *frags, int *alloc_sz);
extern void	 mpt_free_fw_memory(MPT_ADAPTER *ioc, fw_image_t **alt_img);
extern int	 mpt_read_ioc_pg_3(MPT_ADAPTER *ioc);

/*
 *  Public data decl's...
 */
extern MPT_ADAPTER	 	*mpt_adapters[MPT_MAX_ADAPTERS];
extern struct proc_dir_entry	*mpt_proc_root_dir;
extern DmpServices_t		*DmpService;

extern int		  mpt_lan_index;	/* needed by mptlan.c */
extern int		  mpt_stm_index;	/* needed by mptstm.c */

extern void		 *mpt_v_ASCQ_TablePtr;
extern const char	**mpt_ScsiOpcodesPtr;
extern int		  mpt_ASCQ_TableSz;

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#endif		/* } __KERNEL__ */

/*
 *  More (public) macros...
 */
#ifndef MIN
#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))
#endif

#ifndef offsetof
#define offsetof(t, m)	((size_t) (&((t *)0)->m))
#endif

#if defined(__alpha__) || defined(__sparc_v9__) || defined(__ia64__) || defined(__x86_64__)
#define CAST_U32_TO_PTR(x)	((void *)(u64)x)
#define CAST_PTR_TO_U32(x)	((u32)(u64)x)
#else
#define CAST_U32_TO_PTR(x)	((void *)x)
#define CAST_PTR_TO_U32(x)	((u32)x)
#endif

#define MPT_PROTOCOL_FLAGS_c_c_c_c(pflags) \
	((pflags) & MPI_PORTFACTS_PROTOCOL_INITIATOR)	? 'I' : 'i',	\
	((pflags) & MPI_PORTFACTS_PROTOCOL_TARGET)	? 'T' : 't',	\
	((pflags) & MPI_PORTFACTS_PROTOCOL_LAN)		? 'L' : 'l',	\
	((pflags) & MPI_PORTFACTS_PROTOCOL_LOGBUSADDR)	? 'B' : 'b'

/*
 *  Shifted SGE Defines - Use in SGE with FlagsLength member.
 *  Otherwise, use MPI_xxx defines (refer to "lsi/mpi.h" header).
 *  Defaults: 32 bit SGE, SYSTEM_ADDRESS if direction bit is 0, read
 */
#define MPT_TRANSFER_IOC_TO_HOST		(0x00000000)
#define MPT_TRANSFER_HOST_TO_IOC		(0x04000000)
#define MPT_SGE_FLAGS_LAST_ELEMENT		(0x80000000)
#define MPT_SGE_FLAGS_END_OF_BUFFER		(0x40000000)
#define MPT_SGE_FLAGS_LOCAL_ADDRESS		(0x08000000)
#define MPT_SGE_FLAGS_DIRECTION			(0x04000000)
#define MPT_SGE_FLAGS_ADDRESSING		(mpt_addr_size() << MPI_SGE_FLAGS_SHIFT)
#define MPT_SGE_FLAGS_END_OF_LIST		(0x01000000)

#define MPT_SGE_FLAGS_TRANSACTION_ELEMENT	(0x00000000)
#define MPT_SGE_FLAGS_SIMPLE_ELEMENT		(0x10000000)
#define MPT_SGE_FLAGS_CHAIN_ELEMENT		(0x30000000)
#define MPT_SGE_FLAGS_ELEMENT_MASK		(0x30000000)

#define MPT_SGE_FLAGS_SSIMPLE_READ \
	(MPT_SGE_FLAGS_LAST_ELEMENT |	\
	 MPT_SGE_FLAGS_END_OF_BUFFER |	\
	 MPT_SGE_FLAGS_END_OF_LIST |	\
	 MPT_SGE_FLAGS_SIMPLE_ELEMENT |	\
	 MPT_SGE_FLAGS_ADDRESSING | \
	 MPT_TRANSFER_IOC_TO_HOST)
#define MPT_SGE_FLAGS_SSIMPLE_WRITE \
	(MPT_SGE_FLAGS_LAST_ELEMENT |	\
	 MPT_SGE_FLAGS_END_OF_BUFFER |	\
	 MPT_SGE_FLAGS_END_OF_LIST |	\
	 MPT_SGE_FLAGS_SIMPLE_ELEMENT |	\
	 MPT_SGE_FLAGS_ADDRESSING | \
	 MPT_TRANSFER_HOST_TO_IOC)

/*}-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#endif

