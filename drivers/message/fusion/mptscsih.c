/*
 *  linux/drivers/message/fusion/mptscsih.c
 *      High performance SCSI / Fibre Channel SCSI Host device driver.
 *      For use with PCI chip/adapter(s):
 *          LSIFC9xx/LSI409xx Fibre Channel
 *      running LSI Logic Fusion MPT (Message Passing Technology) firmware.
 *
 *  Credits:
 *      This driver would not exist if not for Alan Cox's development
 *      of the linux i2o driver.
 *
 *      A special thanks to Pamela Delaney (LSI Logic) for tons of work
 *      and countless enhancements while adding support for the 1030
 *      chip family.  Pam has been instrumental in the development of
 *      of the 2.xx.xx series fusion drivers, and her contributions are
 *      far too numerous to hope to list in one place.
 *
 *      A huge debt of gratitude is owed to David S. Miller (DaveM)
 *      for fixing much of the stupid and broken stuff in the early
 *      driver while porting to sparc64 platform.  THANK YOU!
 *
 *      (see mptbase.c)
 *
 *  Copyright (c) 1999-2002 LSI Logic Corporation
 *  Original author: Steven J. Ralston
 *  (mailto:sjralston1@netscape.net)
 *  (mailto:mpt_linux_developer@lsil.com)
 *
 *  $Id: mptscsih.c,v 1.1.2.4 2003/05/07 14:08:34 pdelaney Exp $
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
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/blk.h>		/* for io_request_lock (spinlock) decl */
#include <linux/delay.h>	/* for mdelay */
#include <linux/interrupt.h>	/* needed for in_interrupt() proto */
#include <linux/reboot.h>	/* notifier code */
#include "../../scsi/scsi.h"
#include "../../scsi/hosts.h"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,45)
#include "../../scsi/sd.h"
#endif

#include "mptbase.h"
#include "mptscsih.h"
#include "isense.h"

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#define my_NAME		"Fusion MPT SCSI Host driver"
#define my_VERSION	MPT_LINUX_VERSION_COMMON
#define MYNAM		"mptscsih"

MODULE_AUTHOR(MODULEAUTHOR);
MODULE_DESCRIPTION(my_NAME);
MODULE_LICENSE("GPL");

/* Set string for command line args from insmod */
#ifdef MODULE
char *mptscsih = 0;
MODULE_PARM(mptscsih, "s");
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

typedef struct _BIG_SENSE_BUF {
	u8		data[MPT_SENSE_BUFFER_ALLOC];
} BIG_SENSE_BUF;

#define MPT_SCANDV_GOOD			(0x00000000) /* must be 0 */
#define MPT_SCANDV_DID_RESET		(0x00000001)
#define MPT_SCANDV_SENSE		(0x00000002)
#define MPT_SCANDV_SOME_ERROR		(0x00000004)
#define MPT_SCANDV_SELECTION_TIMEOUT	(0x00000008)
#define MPT_SCANDV_ISSUE_SENSE		(0x00000010)

#define MPT_SCANDV_MAX_RETRIES		(10)

#define MPT_ICFLAG_BUF_CAP	0x01	/* ReadBuffer Read Capacity format */
#define MPT_ICFLAG_ECHO		0x02	/* ReadBuffer Echo buffer format */
#define MPT_ICFLAG_PHYS_DISK	0x04	/* Any SCSI IO but do Phys Disk Format */
#define MPT_ICFLAG_TAGGED_CMD	0x08	/* Do tagged IO */
#define MPT_ICFLAG_DID_RESET	0x20	/* Bus Reset occured with this command */
#define MPT_ICFLAG_RESERVED	0x40	/* Reserved has been issued */

typedef struct _internal_cmd {
	char		*data;		/* data pointer */
	dma_addr_t	data_dma;	/* data dma address */
	int		size;		/* transfer size */
	u8		cmd;		/* SCSI Op Code */
	u8		bus;		/* bus number */
	u8		id;		/* SCSI ID (virtual) */
	u8		lun;
	u8		flags;		/* Bit Field - See above */
	u8		physDiskNum;	/* Phys disk number, -1 else */
	u8		rsvd2;
	u8		rsvd;
} INTERNAL_CMD;

typedef struct _negoparms {
	u8 width;
	u8 offset;
	u8 factor;
	u8 flags;
} NEGOPARMS;

typedef struct _dv_parameters {
	NEGOPARMS	 max;
	NEGOPARMS	 now;
	u8		 cmd;
	u8		 id;
	u16		 pad1;
} DVPARAMETERS;


/*
 *  Other private/forward protos...
 */
static int	mptscsih_io_done(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf, MPT_FRAME_HDR *r);
static void	mptscsih_report_queue_full(Scsi_Cmnd *sc, SCSIIOReply_t *pScsiReply, SCSIIORequest_t *pScsiReq);
static int	mptscsih_taskmgmt_complete(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf, MPT_FRAME_HDR *r);

static int	mptscsih_AddSGE(MPT_SCSI_HOST *hd, Scsi_Cmnd *SCpnt,
				 SCSIIORequest_t *pReq, int req_idx);
static void	mptscsih_freeChainBuffers(MPT_SCSI_HOST *hd, int req_idx);
static int	mptscsih_initChainBuffers (MPT_SCSI_HOST *hd, int init);
static void	copy_sense_data(Scsi_Cmnd *sc, MPT_SCSI_HOST *hd, MPT_FRAME_HDR *mf, SCSIIOReply_t *pScsiReply);
#ifndef MPT_SCSI_USE_NEW_EH
static void	search_taskQ_for_cmd(Scsi_Cmnd *sc, MPT_SCSI_HOST *hd);
#else
static int	mptscsih_tm_pending_wait(MPT_SCSI_HOST * hd);
#endif
static u32	SCPNT_TO_LOOKUP_IDX(Scsi_Cmnd *sc);
static MPT_FRAME_HDR *mptscsih_search_pendingQ(MPT_SCSI_HOST *hd, int scpnt_idx);
static void	post_pendingQ_commands(MPT_SCSI_HOST *hd);

static int	mptscsih_TMHandler(MPT_SCSI_HOST *hd, u8 type, u8 channel, u8 target, u8 lun, int ctx2abort, int sleepFlag);
static int	mptscsih_IssueTaskMgmt(MPT_SCSI_HOST *hd, u8 type, u8 channel, u8 target, u8 lun, int ctx2abort, int sleepFlag);

static int	mptscsih_ioc_reset(MPT_ADAPTER *ioc, int post_reset);
static int	mptscsih_event_process(MPT_ADAPTER *ioc, EventNotificationReply_t *pEvReply);

static void	mptscsih_initTarget(MPT_SCSI_HOST *hd, int bus_id, int target_id, u8 lun, char *data, int dlen);
void		mptscsih_setTargetNegoParms(MPT_SCSI_HOST *hd, VirtDevice *target, char byte56);
static void	mptscsih_set_dvflags(MPT_SCSI_HOST *hd, SCSIIORequest_t *pReq);
static void	mptscsih_setDevicePage1Flags (u8 width, u8 factor, u8 offset, int *requestedPtr, int *configurationPtr, u8 flags);
static void	mptscsih_no_negotiate(MPT_SCSI_HOST *hd, int target_id);
static int	mptscsih_writeSDP1(MPT_SCSI_HOST *hd, int portnum, int target, int flags);
static int	mptscsih_writeIOCPage4(MPT_SCSI_HOST *hd, int target_id, int bus);
static int	mptscsih_scandv_complete(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf, MPT_FRAME_HDR *r);
static void	mptscsih_timer_expired(unsigned long data);
static void	mptscsih_taskmgmt_timeout(unsigned long data);
static int	mptscsih_do_cmd(MPT_SCSI_HOST *hd, INTERNAL_CMD *iocmd);
static int	mptscsih_synchronize_cache(MPT_SCSI_HOST *hd, int portnum);

#ifndef MPTSCSIH_DISABLE_DOMAIN_VALIDATION
static int	mptscsih_do_raid(MPT_SCSI_HOST *hd, u8 action, INTERNAL_CMD *io);
static void	mptscsih_domainValidation(void *hd);
static int	mptscsih_is_phys_disk(MPT_ADAPTER *ioc, int id);
static void	mptscsih_qas_check(MPT_SCSI_HOST *hd, int id);
static int	mptscsih_doDv(MPT_SCSI_HOST *hd, int channel, int target);
static void	mptscsih_dv_parms(MPT_SCSI_HOST *hd, DVPARAMETERS *dv,void *pPage);
static void	mptscsih_fillbuf(char *buffer, int size, int index, int width);
#endif
static int	mptscsih_setup(char *str);
static int	mptscsih_halt(struct notifier_block *nb, ulong event, void *buf);

/*
 *	Reboot Notification
 */
static struct notifier_block mptscsih_notifier = {
	mptscsih_halt, NULL, 0
};

/*
 *	Private data...
 */

static int	mpt_scsi_hosts = 0;
static atomic_t	queue_depth;

static int	ScsiDoneCtx = -1;
static int	ScsiTaskCtx = -1;
static int	ScsiScanDvCtx = -1; /* Used only for bus scan and dv */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,28)
static struct proc_dir_entry proc_mpt_scsihost =
{
	.low_ino =	PROC_SCSI_MPT,
	.namelen =	8,
	.name =		"mptscsih",
	.mode =		S_IFDIR | S_IRUGO | S_IXUGO,
	.nlink =	2,
};
#endif

#define SNS_LEN(scp)	sizeof((scp)->sense_buffer)

#ifndef MPT_SCSI_USE_NEW_EH
/*
 *  Stuff to handle single-threading SCSI TaskMgmt
 *  (abort/reset) requests...
 */
static spinlock_t mytaskQ_lock = SPIN_LOCK_UNLOCKED;
static int mytaskQ_bh_active = 0;
static struct mpt_work_struct	mptscsih_ptaskfoo;
static atomic_t	mpt_taskQdepth;
#endif

#ifndef MPTSCSIH_DISABLE_DOMAIN_VALIDATION
/*
 * Domain Validation task structure
 */
static spinlock_t dvtaskQ_lock = SPIN_LOCK_UNLOCKED;
static int dvtaskQ_active = 0;
static int dvtaskQ_release = 0;
static struct mpt_work_struct	mptscsih_dvTask;
#endif

/*
 * Wait Queue setup
 */
static DECLARE_WAIT_QUEUE_HEAD (scandv_waitq);
static int scandv_wait_done = 1;

/* Driver default setup
 */
static struct mptscsih_driver_setup
	driver_setup = MPTSCSIH_DRIVER_SETUP;

#ifdef MPTSCSIH_DBG_TIMEOUT
static Scsi_Cmnd *foo_to[8];
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Private inline routines...
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/* 19991030 -sralston
 *  Return absolute SCSI data direction:
 *     1 = _DATA_OUT
 *     0 = _DIR_NONE
 *    -1 = _DATA_IN
 *
 * Changed: 3-20-2002 pdelaney to use the default data
 * direction and the defines set up in the
 * 2.4 kernel series
 *     1 = _DATA_OUT	changed to SCSI_DATA_WRITE (1)
 *     0 = _DIR_NONE	changed to SCSI_DATA_NONE (3)
 *    -1 = _DATA_IN	changed to SCSI_DATA_READ (2)
 * If the direction is unknown, fall through to original code.
 *
 * Mid-layer bug fix(): sg interface generates the wrong data
 * direction in some cases. Set the direction the hard way for
 * the most common commands.
 */
static inline int
mptscsih_io_direction(Scsi_Cmnd *cmd)
{
	switch (cmd->cmnd[0]) {
	case WRITE_6:		
	case WRITE_10:		
		return SCSI_DATA_WRITE;
		break;
	case READ_6:		
	case READ_10:		
		return SCSI_DATA_READ;
		break;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	if (cmd->sc_data_direction != SCSI_DATA_UNKNOWN)
		return cmd->sc_data_direction;
#endif
	switch (cmd->cmnd[0]) {
	/*  _DATA_OUT commands	*/
	case WRITE_6:		case WRITE_10:		case WRITE_12:
	case WRITE_LONG:	case WRITE_SAME:	case WRITE_BUFFER:
	case WRITE_VERIFY:	case WRITE_VERIFY_12:
	case COMPARE:		case COPY:		case COPY_VERIFY:
	case SEARCH_EQUAL:	case SEARCH_HIGH:	case SEARCH_LOW:
	case SEARCH_EQUAL_12:	case SEARCH_HIGH_12:	case SEARCH_LOW_12:
	case MODE_SELECT:	case MODE_SELECT_10:	case LOG_SELECT:
	case SEND_DIAGNOSTIC:	case CHANGE_DEFINITION: case UPDATE_BLOCK:
	case SET_WINDOW:	case MEDIUM_SCAN:	case SEND_VOLUME_TAG:
	case REASSIGN_BLOCKS:
	case PERSISTENT_RESERVE_OUT:
	case 0xea:
	case 0xa3:
		return SCSI_DATA_WRITE;

	/*  No data transfer commands  */
	case SEEK_6:		case SEEK_10:
	case RESERVE:		case RELEASE:
	case TEST_UNIT_READY:
	case START_STOP:
	case ALLOW_MEDIUM_REMOVAL:
		return SCSI_DATA_NONE;

	/*  Conditional data transfer commands	*/
	case FORMAT_UNIT:
		if (cmd->cmnd[1] & 0x10)	/* FmtData (data out phase)? */
			return SCSI_DATA_WRITE;
		else
			return SCSI_DATA_NONE;

	case VERIFY:
		if (cmd->cmnd[1] & 0x02)	/* VERIFY:BYTCHK (data out phase)? */
			return SCSI_DATA_WRITE;
		else
			return SCSI_DATA_NONE;

	case RESERVE_10:
		if (cmd->cmnd[1] & 0x03)	/* RESERVE:{LongID|Extent} (data out phase)? */
			return SCSI_DATA_WRITE;
		else
			return SCSI_DATA_NONE;

	/*  Must be data _IN!  */
	default:
		return SCSI_DATA_READ;
	}
} /* mptscsih_io_direction() */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_add_sge - Place a simple SGE at address pAddr.
 *	@pAddr: virtual address for SGE
 *	@flagslength: SGE flags and data transfer length
 *	@dma_addr: Physical address
 *
 *	This routine places a MPT request frame back on the MPT adapter's
 *	FreeQ.
 */
static inline void
mptscsih_add_sge(char *pAddr, u32 flagslength, dma_addr_t dma_addr)
{
	if (sizeof(dma_addr_t) == sizeof(u64)) {
		SGESimple64_t *pSge = (SGESimple64_t *) pAddr;
		u32 tmp = dma_addr & 0xFFFFFFFF;

		pSge->FlagsLength = cpu_to_le32(flagslength);
		pSge->Address.Low = cpu_to_le32(tmp);
		tmp = (u32) ((u64)dma_addr >> 32);
		pSge->Address.High = cpu_to_le32(tmp);

	} else {
		SGESimple32_t *pSge = (SGESimple32_t *) pAddr;
		pSge->FlagsLength = cpu_to_le32(flagslength);
		pSge->Address = cpu_to_le32(dma_addr);
	}
} /* mptscsih_add_sge() */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_add_chain - Place a chain SGE at address pAddr.
 *	@pAddr: virtual address for SGE
 *	@next: nextChainOffset value (u32's)
 *	@length: length of next SGL segment
 *	@dma_addr: Physical address
 *
 *	This routine places a MPT request frame back on the MPT adapter's
 *	FreeQ.
 */
static inline void
mptscsih_add_chain(char *pAddr, u8 next, u16 length, dma_addr_t dma_addr)
{
	if (sizeof(dma_addr_t) == sizeof(u64)) {
		SGEChain64_t *pChain = (SGEChain64_t *) pAddr;
		u32 tmp = dma_addr & 0xFFFFFFFF;

		pChain->Length = cpu_to_le16(length);
		pChain->Flags = MPI_SGE_FLAGS_CHAIN_ELEMENT | mpt_addr_size();

		pChain->NextChainOffset = next;

		pChain->Address.Low = cpu_to_le32(tmp);
		tmp = (u32) ((u64)dma_addr >> 32);
		pChain->Address.High = cpu_to_le32(tmp);
	} else {
		SGEChain32_t *pChain = (SGEChain32_t *) pAddr;
		pChain->Length = cpu_to_le16(length);
		pChain->Flags = MPI_SGE_FLAGS_CHAIN_ELEMENT | mpt_addr_size();
		pChain->NextChainOffset = next;
		pChain->Address = cpu_to_le32(dma_addr);
	}
} /* mptscsih_add_chain() */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_getFreeChainBuffes - Function to get a free chain
 *	from the MPT_SCSI_HOST FreeChainQ.
 *	@hd: Pointer to the MPT_SCSI_HOST instance
 *	@req_idx: Index of the SCSI IO request frame. (output)
 *
 *	return SUCCESS or FAILED
 */
static inline int
mptscsih_getFreeChainBuffer(MPT_SCSI_HOST *hd, int *retIndex)
{
	MPT_FRAME_HDR *chainBuf;
	unsigned long flags;
	int rc;
	int chain_idx;

	spin_lock_irqsave(&hd->ioc->FreeQlock, flags);
	if (!Q_IS_EMPTY(&hd->FreeChainQ)) {

		int offset;

		chainBuf = hd->FreeChainQ.head;
		Q_DEL_ITEM(&chainBuf->u.frame.linkage);
		offset = (u8 *)chainBuf - (u8 *)hd->ChainBuffer;
		chain_idx = offset / hd->ioc->req_sz;
		rc = SUCCESS;
	}
	else {
		rc = FAILED;
		chain_idx = MPT_HOST_NO_CHAIN;
	}
	spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);


	*retIndex = chain_idx;

	dsgprintk((MYIOC_s_INFO_FMT "getFreeChainBuffer (index %d), got buf=%p\n",
			hd->ioc->name, *retIndex, chainBuf));

	return rc;
} /* mptscsih_getFreeChainBuffer() */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_AddSGE - Add a SGE (plus chain buffers) to the
 *	SCSIIORequest_t Message Frame.
 *	@hd: Pointer to MPT_SCSI_HOST structure
 *	@SCpnt: Pointer to Scsi_Cmnd structure
 *	@pReq: Pointer to SCSIIORequest_t structure
 *
 *	Returns ...
 */
static int
mptscsih_AddSGE(MPT_SCSI_HOST *hd, Scsi_Cmnd *SCpnt,
		SCSIIORequest_t *pReq, int req_idx)
{
	char 	*psge;
	char	*chainSge;
	struct scatterlist *sg;
	int	 frm_sz;
	int	 sges_left, sg_done;
	int	 chain_idx = MPT_HOST_NO_CHAIN;
	int	 sgeOffset;
	int	 numSgeSlots, numSgeThisFrame;
	u32	 sgflags, sgdir, thisxfer = 0;
	int	 chain_dma_off = 0;
	int	 newIndex;
	int	 ii;
	dma_addr_t v2;

	sgdir = le32_to_cpu(pReq->Control) & MPI_SCSIIO_CONTROL_DATADIRECTION_MASK;
	if (sgdir == MPI_SCSIIO_CONTROL_WRITE)  {
		sgdir = MPT_TRANSFER_HOST_TO_IOC;
	} else {
		sgdir = MPT_TRANSFER_IOC_TO_HOST;
	}

	psge = (char *) &pReq->SGL;
	frm_sz = hd->ioc->req_sz;

	/* Map the data portion, if any.
	 * sges_left  = 0 if no data transfer.
	 */
	if ( (sges_left = SCpnt->use_sg) ) {
		if ( (sges_left = pci_map_sg(hd->ioc->pcidev,
			       (struct scatterlist *) SCpnt->request_buffer,
			       SCpnt->use_sg,
			       scsi_to_pci_dma_dir(SCpnt->sc_data_direction)))
			== 0 )
				return FAILED;
	} else if (SCpnt->request_bufflen) {
		dma_addr_t	 buf_dma_addr;
		scPrivate	*my_priv;

		buf_dma_addr = pci_map_single(hd->ioc->pcidev,
				      SCpnt->request_buffer,
				      SCpnt->request_bufflen,
				      scsi_to_pci_dma_dir(SCpnt->sc_data_direction));

		/* We hide it here for later unmap. */
		my_priv = (scPrivate *) &SCpnt->SCp;
		my_priv->p1 = (void *)(ulong) buf_dma_addr;

		dsgprintk((MYIOC_s_INFO_FMT "SG: non-SG for %p, len=%d\n",
				hd->ioc->name, SCpnt, SCpnt->request_bufflen));

		mptscsih_add_sge((char *) &pReq->SGL,
			0xD1000000|MPT_SGE_FLAGS_ADDRESSING|sgdir|SCpnt->request_bufflen,
			buf_dma_addr);

		return SUCCESS;
	}

	/* Handle the SG case.
	 */
	sg = (struct scatterlist *) SCpnt->request_buffer;
	sg_done  = 0;
	sgeOffset = sizeof(SCSIIORequest_t) - sizeof(SGE_IO_UNION);
	chainSge = NULL;

	/* Prior to entering this loop - the following must be set
	 * current MF:  sgeOffset (bytes)
	 *              chainSge (Null if original MF is not a chain buffer)
	 *              sg_done (num SGE done for this MF)
	 */

nextSGEset:
	numSgeSlots = ((frm_sz - sgeOffset) / (sizeof(u32) + sizeof(dma_addr_t)) );
	numSgeThisFrame = (sges_left < numSgeSlots) ? sges_left : numSgeSlots;

	sgflags = MPT_SGE_FLAGS_SIMPLE_ELEMENT | MPT_SGE_FLAGS_ADDRESSING | sgdir;

	/* Get first (num - 1) SG elements
	 * Skip any SG entries with a length of 0
	 * NOTE: at finish, sg and psge pointed to NEXT data/location positions
	 */
	for (ii=0; ii < (numSgeThisFrame-1); ii++) {
		thisxfer = sg_dma_len(sg);
		if (thisxfer == 0) {
			sg ++; /* Get next SG element from the OS */
			sg_done++;
			continue;
		}

		v2 = sg_dma_address(sg);
		mptscsih_add_sge(psge, sgflags | thisxfer, v2);

		sg++;		/* Get next SG element from the OS */
		psge += (sizeof(u32) + sizeof(dma_addr_t));
		sgeOffset += (sizeof(u32) + sizeof(dma_addr_t));
		sg_done++;
	}

	if (numSgeThisFrame == sges_left) {
		/* Add last element, end of buffer and end of list flags.
		 */
		sgflags |= MPT_SGE_FLAGS_LAST_ELEMENT |
				MPT_SGE_FLAGS_END_OF_BUFFER |
				MPT_SGE_FLAGS_END_OF_LIST;

		/* Add last SGE and set termination flags.
		 * Note: Last SGE may have a length of 0 - which should be ok.
		 */
		thisxfer = sg_dma_len(sg);

		v2 = sg_dma_address(sg);
		mptscsih_add_sge(psge, sgflags | thisxfer, v2);
		/*
		sg++;
		psge += (sizeof(u32) + sizeof(dma_addr_t));
		*/
		sgeOffset += (sizeof(u32) + sizeof(dma_addr_t));
		sg_done++;

		if (chainSge) {
			/* The current buffer is a chain buffer,
			 * but there is not another one.
			 * Update the chain element
			 * Offset and Length fields.
			 */
			mptscsih_add_chain((char *)chainSge, 0, sgeOffset, hd->ChainBufferDMA + chain_dma_off);
		} else {
			/* The current buffer is the original MF
			 * and there is no Chain buffer.
			 */
			pReq->ChainOffset = 0;
		}
	} else {
		/* At least one chain buffer is needed.
		 * Complete the first MF
		 *  - last SGE element, set the LastElement bit
		 *  - set ChainOffset (words) for orig MF
		 *             (OR finish previous MF chain buffer)
		 *  - update MFStructPtr ChainIndex
		 *  - Populate chain element
		 * Also
		 * Loop until done.
		 */

		dsgprintk((MYIOC_s_INFO_FMT "SG: Chain Required! sg done %d\n",
				hd->ioc->name, sg_done));

		/* Set LAST_ELEMENT flag for last non-chain element
		 * in the buffer. Since psge points at the NEXT
		 * SGE element, go back one SGE element, update the flags
		 * and reset the pointer. (Note: sgflags & thisxfer are already
		 * set properly).
		 */
		if (sg_done) {
			u32 *ptmp = (u32 *) (psge - (sizeof(u32) + sizeof(dma_addr_t)));
			sgflags = le32_to_cpu(*ptmp);
			sgflags |= MPT_SGE_FLAGS_LAST_ELEMENT;
			*ptmp = cpu_to_le32(sgflags);
		}

		if (chainSge) {
			/* The current buffer is a chain buffer.
			 * chainSge points to the previous Chain Element.
			 * Update its chain element Offset and Length (must
			 * include chain element size) fields.
			 * Old chain element is now complete.
			 */
			u8 nextChain = (u8) (sgeOffset >> 2);
			sgeOffset += (sizeof(u32) + sizeof(dma_addr_t));
			mptscsih_add_chain((char *)chainSge, nextChain, sgeOffset, hd->ChainBufferDMA + chain_dma_off);
		} else {
			/* The original MF buffer requires a chain buffer -
			 * set the offset.
			 * Last element in this MF is a chain element.
			 */
			pReq->ChainOffset = (u8) (sgeOffset >> 2);
		}

		sges_left -= sg_done;


		/* NOTE: psge points to the beginning of the chain element
		 * in current buffer. Get a chain buffer.
		 */
		if ((mptscsih_getFreeChainBuffer(hd, &newIndex)) == FAILED)
			return FAILED;

		/* Update the tracking arrays.
		 * If chainSge == NULL, update ReqToChain, else ChainToChain
		 */
		if (chainSge) {
			hd->ChainToChain[chain_idx] = newIndex;
		} else {
			hd->ReqToChain[req_idx] = newIndex;
		}
		chain_idx = newIndex;
		chain_dma_off = hd->ioc->req_sz * chain_idx;

		/* Populate the chainSGE for the current buffer.
		 * - Set chain buffer pointer to psge and fill
		 *   out the Address and Flags fields.
		 */
		chainSge = (char *) psge;
		dsgprintk((KERN_INFO "  Current buff @ %p (index 0x%x)",
				psge, req_idx));

		/* Start the SGE for the next buffer
		 */
		psge = (char *) (hd->ChainBuffer + chain_dma_off);
		sgeOffset = 0;
		sg_done = 0;

		dsgprintk((KERN_INFO "  Chain buff @ %p (index 0x%x)\n",
				psge, chain_idx));

		/* Start the SGE for the next buffer
		 */

		goto nextSGEset;
	}

	return SUCCESS;
} /* mptscsih_AddSGE() */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_io_done - Main SCSI IO callback routine registered to
 *	Fusion MPT (base) driver
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@mf: Pointer to original MPT request frame
 *	@r: Pointer to MPT reply frame (NULL if TurboReply)
 *
 *	This routine is called from mpt.c::mpt_interrupt() at the completion
 *	of any SCSI IO request.
 *	This routine is registered with the Fusion MPT (base) driver at driver
 *	load/init time via the mpt_register() API call.
 *
 *	Returns 1 indicating alloc'd request frame ptr should be freed.
 */
static int
mptscsih_io_done(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf, MPT_FRAME_HDR *mr)
{
	Scsi_Cmnd	*sc;
	MPT_SCSI_HOST	*hd;
	SCSIIORequest_t	*pScsiReq;
	SCSIIOReply_t	*pScsiReply;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
	unsigned long	 flags;
#endif
	u16		 req_idx;

	hd = (MPT_SCSI_HOST *) ioc->sh->hostdata;

	req_idx = le16_to_cpu(mf->u.frame.hwhdr.msgctxu.fld.req_idx);
	sc = hd->ScsiLookup[req_idx];
	if (sc == NULL) {
		MPIHeader_t *hdr = (MPIHeader_t *)mf;

		atomic_dec(&queue_depth);

		/* Remark: writeSDP1 will use the ScsiDoneCtx
		 * If a SCSI I/O cmd, device disabled by OS and
		 * completion done. Cannot touch sc struct. Just free mem.
		 */
		if (hdr->Function == MPI_FUNCTION_SCSI_IO_REQUEST)
			printk(MYIOC_s_ERR_FMT "NULL ScsiCmd ptr!\n", ioc->name);

		mptscsih_freeChainBuffers(hd, req_idx);
		return 1;
	}

	dmfprintk((MYIOC_s_INFO_FMT "ScsiDone (mf=%p,mr=%p,sc=%p,idx=%d)\n",
			ioc->name, mf, mr, sc, req_idx));

	atomic_dec(&queue_depth);

	sc->result = DID_OK << 16;		/* Set default reply as OK */
	pScsiReq = (SCSIIORequest_t *) mf;
	pScsiReply = (SCSIIOReply_t *) mr;

#ifdef MPTSCSIH_DBG_TIMEOUT
	if (ioc->timeout_cnt > 0) {
		int ii, left = 0;

		for (ii=0; ii < 8; ii++) {
			if (sc == foo_to[ii]) {
				printk(MYIOC_s_INFO_FMT "complete (%p, %ld)\n",
					ioc->name, sc, jiffies);
				foo_to[ii] = NULL;
			}
			if (foo_to[ii] != NULL)
				left++;
		}

		if (left == 0) {
			ioc->timeout_maxcnt = 0;
			ioc->timeout_cnt = 0;
		}
	}
#endif
	if (pScsiReply == NULL) {
		/* special context reply handling */

		/* If regular Inquiry cmd - save inquiry data
		 */
		if (pScsiReq->CDB[0] == INQUIRY && !(pScsiReq->CDB[1] & 0x3)) {
			int	 dlen;

			dlen = le32_to_cpu(pScsiReq->DataLength);
			if (dlen >= SCSI_STD_INQUIRY_BYTES) {
				mptscsih_initTarget(hd,
						sc->channel,
						sc->target,
						pScsiReq->LUN[1],
						sc->buffer,
						dlen);
			}
		}
	} else {
		u32	 xfer_cnt;
		u16	 status;
		u8	 scsi_state;

		status = le16_to_cpu(pScsiReply->IOCStatus) & MPI_IOCSTATUS_MASK;
		scsi_state = pScsiReply->SCSIState;

		dprintk((KERN_NOTICE "  Uh-Oh! (%d:%d:%d) mf=%p, mr=%p, sc=%p\n",
				ioc->id, pScsiReq->TargetID, pScsiReq->LUN[1],
				mf, mr, sc));
		dprintk((KERN_NOTICE "  IOCStatus=%04xh, SCSIState=%02xh"
				", SCSIStatus=%02xh, IOCLogInfo=%08xh\n",
				status, scsi_state, pScsiReply->SCSIStatus,
				le32_to_cpu(pScsiReply->IOCLogInfo)));

		if (scsi_state & MPI_SCSI_STATE_AUTOSENSE_VALID)
			copy_sense_data(sc, hd, mf, pScsiReply);

		/*
		 *  Look for + dump FCP ResponseInfo[]!
		 */
		if (scsi_state & MPI_SCSI_STATE_RESPONSE_INFO_VALID) {
			dprintk((KERN_NOTICE "  FCP_ResponseInfo=%08xh\n",
					     le32_to_cpu(pScsiReply->ResponseInfo)));
		}

		switch(status) {
		case MPI_IOCSTATUS_BUSY:			/* 0x0002 */
			/* CHECKME!
			 * Maybe: DRIVER_BUSY | SUGGEST_RETRY | DID_SOFT_ERROR (retry)
			 * But not: DID_BUS_BUSY lest one risk
			 * killing interrupt handler:-(
			 */
			sc->result = STS_BUSY;
			break;

		case MPI_IOCSTATUS_SCSI_INVALID_BUS:		/* 0x0041 */
		case MPI_IOCSTATUS_SCSI_INVALID_TARGETID:	/* 0x0042 */
			sc->result = DID_BAD_TARGET << 16;
			break;

		case MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE:	/* 0x0043 */
			/* Spoof to SCSI Selection Timeout! */
			sc->result = DID_NO_CONNECT << 16;

			if (hd->sel_timeout[pScsiReq->TargetID] < 0xFFFF)
				hd->sel_timeout[pScsiReq->TargetID]++;
			break;

		case MPI_IOCSTATUS_SCSI_TASK_TERMINATED:	/* 0x0048 */
#ifndef MPT_SCSI_USE_NEW_EH
			search_taskQ_for_cmd(sc, hd);
#endif
			/* Linux handles an unsolicited DID_RESET better
			 * than an unsolicited DID_ABORT.
			 */
			sc->result = DID_RESET << 16;

			/* GEM Workaround. */
			if (hd->is_spi)
				mptscsih_no_negotiate(hd, sc->target);
			break;

		case MPI_IOCSTATUS_SCSI_IOC_TERMINATED:		/* 0x004B */
		case MPI_IOCSTATUS_SCSI_EXT_TERMINATED:		/* 0x004C */
#ifndef MPT_SCSI_USE_NEW_EH
			search_taskQ_for_cmd(sc, hd);
#endif
			sc->result = DID_RESET << 16;

			/* GEM Workaround. */
			if (hd->is_spi)
				mptscsih_no_negotiate(hd, sc->target);
			break;

		case MPI_IOCSTATUS_SCSI_DATA_UNDERRUN:		/* 0x0045 */
			/*
			 *  YIKES!  I just discovered that SCSI IO which
			 *  returns check condition, SenseKey=05 (ILLEGAL REQUEST)
			 *  and ASC/ASCQ=94/01 (LSI Logic RAID vendor specific),
			 *  comes down this path!
			 *  Do upfront check for valid SenseData and give it
			 *  precedence!
			 */
			sc->result = (DID_OK << 16) | pScsiReply->SCSIStatus;
			if (scsi_state == 0) {
				;
			} else if (scsi_state & MPI_SCSI_STATE_AUTOSENSE_VALID) {
				/* Have already saved the status and sense data
				 */
				;
			} else if (scsi_state & (MPI_SCSI_STATE_AUTOSENSE_FAILED | MPI_SCSI_STATE_NO_SCSI_STATUS)) {
				/* What to do?
				 */
				sc->result = DID_SOFT_ERROR << 16;
			}
			else if (scsi_state & MPI_SCSI_STATE_TERMINATED) {
				/*  Not real sure here either...  */
				sc->result = DID_RESET << 16;
			}

			/* Give report and update residual count.
			 */
			xfer_cnt = le32_to_cpu(pScsiReply->TransferCount);
			dprintk((KERN_NOTICE "  sc->underflow={report ERR if < %02xh bytes xfer'd}\n",
					sc->underflow));
			dprintk((KERN_NOTICE "  ActBytesXferd=%02xh\n", xfer_cnt));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0)
			sc->resid = sc->request_bufflen - xfer_cnt;
			dprintk((KERN_NOTICE "  SET sc->resid=%02xh\n", sc->resid));
#endif
			if((xfer_cnt == 0 ) || (sc->underflow > xfer_cnt)) {
				sc->result = DID_SOFT_ERROR << 16;
			}

			/* Report Queue Full
			 */
			if (sc->result == MPI_SCSI_STATUS_TASK_SET_FULL)
				mptscsih_report_queue_full(sc, pScsiReply, pScsiReq);

			/* If regular Inquiry cmd and some data was transferred,
			 * save inquiry data
			 */
			if (    pScsiReq->CDB[0] == INQUIRY
			     && !(pScsiReq->CDB[1] & 0x3)
			     && xfer_cnt >= SCSI_STD_INQUIRY_BYTES
			   ) {
				mptscsih_initTarget(hd,
						sc->channel,
						sc->target,
						pScsiReq->LUN[1],
						sc->buffer,
						xfer_cnt);
			}
			break;

		case MPI_IOCSTATUS_SCSI_RECOVERED_ERROR:	/* 0x0040 */
		case MPI_IOCSTATUS_SUCCESS:			/* 0x0000 */
			sc->result = (DID_OK << 16) | pScsiReply->SCSIStatus;
			if (scsi_state == 0) {
				;
			} else if (scsi_state & MPI_SCSI_STATE_AUTOSENSE_VALID) {
				/*
				 * If running against circa 200003dd 909 MPT f/w,
				 * may get this (AUTOSENSE_VALID) for actual TASK_SET_FULL
				 * (QUEUE_FULL) returned from device! --> get 0x0000?128
				 * and with SenseBytes set to 0.
				 */
				if (pScsiReply->SCSIStatus == MPI_SCSI_STATUS_TASK_SET_FULL)
					mptscsih_report_queue_full(sc, pScsiReply, pScsiReq);

#ifndef MPT_SCSI_USE_NEW_EH
				/* ADDED 20011120 -sralston
				 * Scsi mid-layer (old_eh) doesn't seem to like it
				 * when RAID returns SCSIStatus=02 (CHECK CONDITION),
				 * SenseKey=01 (RECOVERED ERROR), ASC/ASCQ=95/01.
				 * Seems to be * treating this as a IO error:-(
				 *
				 * So just lie about it altogether here.
				 *
				 * NOTE: It still gets reported to syslog via
				 * mpt_ScsiHost_ErrorReport from copy_sense_data
				 * call far above.
				 */
				if (    pScsiReply->SCSIStatus == STS_CHECK_CONDITION
				     && SD_Sense_Key(sc->sense_buffer) == SK_RECOVERED_ERROR
				   ) {
					sc->result = 0;
				}
#endif

			}
			else if (scsi_state &
			         (MPI_SCSI_STATE_AUTOSENSE_FAILED | MPI_SCSI_STATE_NO_SCSI_STATUS)
			   ) {
				/*
				 * What to do?
				 */
				sc->result = DID_SOFT_ERROR << 16;
			}
			else if (scsi_state & MPI_SCSI_STATE_TERMINATED) {
				/*  Not real sure here either...  */
				sc->result = DID_RESET << 16;
			}
			else if (scsi_state & MPI_SCSI_STATE_QUEUE_TAG_REJECTED) {
				/* Device Inq. data indicates that it supports
				 * QTags, but rejects QTag messages.
				 * This command completed OK.
				 *
				 * Not real sure here either so do nothing...  */
			}

			if (sc->result == MPI_SCSI_STATUS_TASK_SET_FULL)
				mptscsih_report_queue_full(sc, pScsiReply, pScsiReq);

			/* Add handling of:
			 * Reservation Conflict, Busy,
			 * Command Terminated, CHECK
			 */

			/* If regular Inquiry cmd - save inquiry data
			 */
			xfer_cnt = le32_to_cpu(pScsiReply->TransferCount);
			if (    sc->result == (DID_OK << 16)
			     && pScsiReq->CDB[0] == INQUIRY
			     && !(pScsiReq->CDB[1] & 0x3)
			     && xfer_cnt >= SCSI_STD_INQUIRY_BYTES
			   ) {
				mptscsih_initTarget(hd,
						sc->channel,
						sc->target,
						pScsiReq->LUN[1],
						sc->buffer,
						xfer_cnt);
			}
			break;

		case MPI_IOCSTATUS_SCSI_PROTOCOL_ERROR:		/* 0x0047 */
			if (pScsiReply->SCSIState & MPI_SCSI_STATE_TERMINATED) {
				/*  Not real sure here either...  */
				sc->result = DID_RESET << 16;
			} else
				sc->result = DID_SOFT_ERROR << 16;
			break;

		case MPI_IOCSTATUS_INVALID_FUNCTION:		/* 0x0001 */
		case MPI_IOCSTATUS_INVALID_SGL:			/* 0x0003 */
		case MPI_IOCSTATUS_INTERNAL_ERROR:		/* 0x0004 */
		case MPI_IOCSTATUS_RESERVED:			/* 0x0005 */
		case MPI_IOCSTATUS_INSUFFICIENT_RESOURCES:	/* 0x0006 */
		case MPI_IOCSTATUS_INVALID_FIELD:		/* 0x0007 */
		case MPI_IOCSTATUS_INVALID_STATE:		/* 0x0008 */
		case MPI_IOCSTATUS_SCSI_DATA_OVERRUN:		/* 0x0044 */
		case MPI_IOCSTATUS_SCSI_IO_DATA_ERROR:		/* 0x0046 */
		case MPI_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:	/* 0x0049 */
		case MPI_IOCSTATUS_SCSI_TASK_MGMT_FAILED:	/* 0x004A */
		default:
			/*
			 * What to do?
			 */
			sc->result = DID_SOFT_ERROR << 16;
			break;

		}	/* switch(status) */

		dprintk((KERN_NOTICE "  sc->result set to %08xh\n", sc->result));
	} /* end of address reply case */

	/* Unmap the DMA buffers, if any. */
	if (sc->use_sg) {
		pci_unmap_sg(ioc->pcidev, (struct scatterlist *) sc->request_buffer,
			    sc->use_sg, scsi_to_pci_dma_dir(sc->sc_data_direction));
	} else if (sc->request_bufflen) {
		scPrivate	*my_priv;

		my_priv = (scPrivate *) &sc->SCp;
		pci_unmap_single(ioc->pcidev, (dma_addr_t)(ulong)my_priv->p1,
			   sc->request_bufflen,
			   scsi_to_pci_dma_dir(sc->sc_data_direction));
	}

	hd->ScsiLookup[req_idx] = NULL;

#ifndef MPT_SCSI_USE_NEW_EH
	sc->host_scribble = NULL;
#endif

        MPT_HOST_LOCK(flags);
	sc->scsi_done(sc);		/* Issue the command callback */
        MPT_HOST_UNLOCK(flags);

	/* Free Chain buffers */
	mptscsih_freeChainBuffers(hd, req_idx);
	return 1;
}

#ifndef MPT_SCSI_USE_NEW_EH	/* { */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	search_taskQ - Search SCSI task mgmt request queue for specific
 *	request type.
 *	@remove: (Boolean) Should request be removed if found?
 *	@sc: Pointer to Scsi_Cmnd structure
 *	@task_type: Task type to search for
 *
 *	Returns pointer to MPT request frame if found, or %NULL if request
 *	was not found.
 */
static MPT_FRAME_HDR *
search_taskQ(int remove, Scsi_Cmnd *sc, MPT_SCSI_HOST *hd, u8 task_type)
{
	MPT_FRAME_HDR *mf = NULL;
	unsigned long flags;
	int count = 0;
	int list_sz;

	dprintk((KERN_INFO MYNAM ": search_taskQ(%d,sc=%p,%d) called\n",
			remove, sc, task_type));
	spin_lock_irqsave(&hd->ioc->FreeQlock, flags);
	list_sz = hd->taskQcnt;
	if (! Q_IS_EMPTY(&hd->taskQ)) {
		mf = hd->taskQ.head;
		do {
			count++;
			if (mf->u.frame.linkage.argp1 == sc &&
			    mf->u.frame.linkage.arg1 == task_type) {
				if (remove) {
					Q_DEL_ITEM(&mf->u.frame.linkage);
					hd->taskQcnt--;
					atomic_dec(&mpt_taskQdepth);

					/* Don't save mf into nextmf because
					 * exit after command has been deleted.
					 */

					/* Place the MF back on the FreeQ */
					Q_ADD_TAIL(&hd->ioc->FreeQ,
						&mf->u.frame.linkage,
						MPT_FRAME_HDR);
#ifdef MFCNT
					hd->ioc->mfcnt--;
#endif
				}
				break;
			}
		} while ((mf = mf->u.frame.linkage.forw) != (MPT_FRAME_HDR*)&hd->taskQ);
		if (mf == (MPT_FRAME_HDR*)&hd->taskQ) {
			mf = NULL;
		}
	}
	spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);

	if (list_sz) {
		dprintk((KERN_INFO "  Results=%p (%sFOUND%s)!\n",
				   mf,
				   mf ? "" : "NOT_",
				   (mf && remove) ? "+REMOVED" : "" ));
		dprintk((KERN_INFO "  (searched thru %d of %d items on taskQ)\n",
				   count,
				   list_sz ));
	}

	return mf;
}

/*
 *	clean_taskQ - Clean the  SCSI task mgmt request for
 *			this SCSI host instance.
 *	@hd: MPT_SCSI_HOST pointer
 *
 *	Returns: None.
 */
static void
clean_taskQ(MPT_SCSI_HOST *hd)
{
	MPT_FRAME_HDR *mf;
	MPT_FRAME_HDR *nextmf;
	MPT_ADAPTER *ioc = hd->ioc;
	unsigned long flags;

	dprintk((KERN_INFO MYNAM ": clean_taskQ called\n"));

	spin_lock_irqsave(&ioc->FreeQlock, flags);
	if (! Q_IS_EMPTY(&hd->taskQ)) {
		mf = hd->taskQ.head;
		do {
			Q_DEL_ITEM(&mf->u.frame.linkage);
			hd->taskQcnt--;
			atomic_dec(&mpt_taskQdepth);

			nextmf = mf->u.frame.linkage.forw;

			/* Place the MF back on the FreeQ */
			Q_ADD_TAIL(&ioc->FreeQ, &mf->u.frame.linkage,
				MPT_FRAME_HDR);
#ifdef MFCNT
			hd->ioc->mfcnt--;
#endif
		} while ((mf = nextmf) != (MPT_FRAME_HDR*)&hd->taskQ);
	}
	spin_unlock_irqrestore(&ioc->FreeQlock, flags);

	return;
}

/*
 *	search_taskQ_for_cmd - Search the  SCSI task mgmt request queue for
 *			the specified command. If found, delete
 *	@hd: MPT_SCSI_HOST pointer
 *
 *	Returns: None.
 */
static void
search_taskQ_for_cmd(Scsi_Cmnd *sc, MPT_SCSI_HOST *hd)
{
	MPT_FRAME_HDR *mf;
	unsigned long flags;
	int count = 0;

	dprintk((KERN_INFO MYNAM ": search_taskQ_for_cmd(sc=%p) called\n", sc));
	spin_lock_irqsave(&hd->ioc->FreeQlock, flags);
	if (! Q_IS_EMPTY(&hd->taskQ)) {
		mf = hd->taskQ.head;
		do {
			count++;
			if (mf->u.frame.linkage.argp1 == sc) {
				Q_DEL_ITEM(&mf->u.frame.linkage);
				hd->taskQcnt--;
				atomic_dec(&mpt_taskQdepth);
				dprintk((KERN_INFO MYNAM
					": Cmd %p found! Deleting.\n", sc));

				/* Don't save mf into nextmf because
				 * exit after command has been deleted.
				 */

				/* Place the MF back on the FreeQ */
				Q_ADD_TAIL(&hd->ioc->FreeQ,
					&mf->u.frame.linkage,
					MPT_FRAME_HDR);
#ifdef MFCNT
				hd->ioc->mfcnt--;
#endif
				break;
			}
		} while ((mf = mf->u.frame.linkage.forw) != (MPT_FRAME_HDR*)&hd->taskQ);
	}
	spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);

	return;
}

#endif		/* } MPT_SCSI_USE_NEW_EH */


/*
 * Flush all commands on the doneQ.
 * Lock Q when deleting/adding members
 * Lock io_request_lock for OS callback.
 */
static void
flush_doneQ(MPT_SCSI_HOST *hd)
{
	MPT_DONE_Q	*buffer;
	Scsi_Cmnd	*SCpnt;
	unsigned long	 flags;

	/* Flush the doneQ.
	 */
	dtmprintk((KERN_INFO MYNAM ": flush_doneQ called\n"));
	while (1) {
		spin_lock_irqsave(&hd->freedoneQlock, flags);
		if (Q_IS_EMPTY(&hd->doneQ)) {
			spin_unlock_irqrestore(&hd->freedoneQlock, flags);
			break;
		}

		buffer = hd->doneQ.head;
		/* Delete from Q
		 */
		Q_DEL_ITEM(buffer);

		/* Set the Scsi_Cmnd pointer
		 */
		SCpnt = (Scsi_Cmnd *) buffer->argp;
		buffer->argp = NULL;

		/* Add to the freeQ
		 */
		Q_ADD_TAIL(&hd->freeQ.head, buffer, MPT_DONE_Q);
		spin_unlock_irqrestore(&hd->freedoneQlock, flags);

		/* Do the OS callback.
		 */
                MPT_HOST_LOCK(flags);
		SCpnt->scsi_done(SCpnt);
                MPT_HOST_UNLOCK(flags);
	}

	return;
}

/*
 * Search the doneQ for a specific command. If found, delete from Q.
 * Calling function will finish processing.
 */
static void
search_doneQ_for_cmd(MPT_SCSI_HOST *hd, Scsi_Cmnd *SCpnt)
{
	unsigned long	 flags;
	MPT_DONE_Q	*buffer;

	spin_lock_irqsave(&hd->freedoneQlock, flags);
	if (!Q_IS_EMPTY(&hd->doneQ)) {
		buffer = hd->doneQ.head;
		do {
			Scsi_Cmnd *sc = (Scsi_Cmnd *) buffer->argp;
			if (SCpnt == sc) {
				Q_DEL_ITEM(buffer);
				SCpnt->result = sc->result;

				/* Set the Scsi_Cmnd pointer
				 */
				buffer->argp = NULL;

				/* Add to the freeQ
				 */
				Q_ADD_TAIL(&hd->freeQ.head, buffer, MPT_DONE_Q);
				break;
			}
		} while ((buffer = buffer->forw) != (MPT_DONE_Q *) &hd->doneQ);
	}
	spin_unlock_irqrestore(&hd->freedoneQlock, flags);
	return;
}

static void
mptscsih_reset_timeouts (MPT_SCSI_HOST *hd)
{
	Scsi_Cmnd	*SCpnt;
	int		 ii;
	int		 max = hd->ioc->req_depth;

	for (ii= 0; ii < max; ii++) {
		if ((SCpnt = hd->ScsiLookup[ii]) != NULL) {
			mod_timer(&SCpnt->eh_timeout, jiffies + (HZ * 60));
			dtmprintk((MYIOC_s_WARN_FMT "resetting SCpnt=%p timeout + 60HZ",
				(hd && hd->ioc) ? hd->ioc->name : "ioc?", SCpnt));
		}
	}
}

/*
 *	mptscsih_flush_running_cmds - For each command found, search
 *		Scsi_Host instance taskQ and reply to OS.
 *		Called only if recovering from a FW reload.
 *	@hd: Pointer to a SCSI HOST structure
 *
 *	Returns: None.
 *
 *	Must be called while new I/Os are being queued.
 */
static void
mptscsih_flush_running_cmds(MPT_SCSI_HOST *hd)
{
	Scsi_Cmnd	*SCpnt;
	MPT_FRAME_HDR	*mf;
	MPT_DONE_Q	*buffer;
	int		 ii;
	int		 max = hd->ioc->req_depth;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
	unsigned long	 flags;
#endif

	dprintk((KERN_INFO MYNAM ": flush_ScsiLookup called\n"));
	for (ii= 0; ii < max; ii++) {
		if ((SCpnt = hd->ScsiLookup[ii]) != NULL) {

			/* Command found.
			 */
#ifndef MPT_SCSI_USE_NEW_EH
			/* Search taskQ, if found, delete.
			 */
			search_taskQ_for_cmd(SCpnt, hd);
#endif

			/* Search pendingQ, if found,
			 * delete from Q. If found, do not decrement
			 * queue_depth, command never posted.
			 */
			if (mptscsih_search_pendingQ(hd, ii) == NULL)
				atomic_dec(&queue_depth);

			/* Null ScsiLookup index
			 */
			hd->ScsiLookup[ii] = NULL;

			mf = MPT_INDEX_2_MFPTR(hd->ioc, ii);
			dmfprintk(( "flush: ScsiDone (mf=%p,sc=%p)\n", mf, SCpnt));

			/* Set status, free OS resources (SG DMA buffers)
			 * Free driver resources (chain, msg buffers)
			 */
			if (SCpnt->use_sg) {
				pci_unmap_sg(hd->ioc->pcidev, (struct scatterlist *) SCpnt->request_buffer,
					    SCpnt->use_sg, scsi_to_pci_dma_dir(SCpnt->sc_data_direction));
			} else if (SCpnt->request_bufflen) {
				scPrivate	*my_priv;
		
				my_priv = (scPrivate *) &SCpnt->SCp;
				pci_unmap_single(hd->ioc->pcidev, (dma_addr_t)(ulong)my_priv->p1,
					   SCpnt->request_bufflen,
					   scsi_to_pci_dma_dir(SCpnt->sc_data_direction));
			}
			SCpnt->result = DID_RESET << 16;
			SCpnt->host_scribble = NULL;

			/* Free Chain buffers */
			mptscsih_freeChainBuffers(hd, ii);

			/* Free Message frames */
			mpt_free_msg_frame(ScsiDoneCtx, hd->ioc->id, mf);

#if 1
			/* Post to doneQ, do not reply until POST phase
			 * of reset handler....prevents new commands from
			 * being queued.
			 */
			spin_lock_irqsave(&hd->freedoneQlock, flags);
			if (!Q_IS_EMPTY(&hd->freeQ)) {
				buffer = hd->freeQ.head;
				Q_DEL_ITEM(buffer);

				/* Set the Scsi_Cmnd pointer
				 */
				buffer->argp = (void *)SCpnt;

				/* Add to the doneQ
				 */
				Q_ADD_TAIL(&hd->doneQ.head, buffer, MPT_DONE_Q);
				spin_unlock_irqrestore(&hd->freedoneQlock, flags);
			} else {
				spin_unlock_irqrestore(&hd->freedoneQlock, flags);
				SCpnt->scsi_done(SCpnt);
			}
#else
                        MPT_HOST_LOCK(flags);
			SCpnt->scsi_done(SCpnt);	/* Issue the command callback */
                        MPT_HOST_UNLOCK(flags);
#endif
		}
	}

	return;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_initChainBuffers - Allocate memory for and initialize
 *	chain buffers, chain buffer control arrays and spinlock.
 *	@hd: Pointer to MPT_SCSI_HOST structure
 *	@init: If set, initialize the spin lock.
 */
static int
mptscsih_initChainBuffers (MPT_SCSI_HOST *hd, int init)
{
	MPT_FRAME_HDR	*chain;
	u8		*mem;
	unsigned long	flags;
	int		sz, ii, num_chain;
	int 		scale, num_sge;

	/* ReqToChain size must equal the req_depth
	 * index = req_idx
	 */
	if (hd->ReqToChain == NULL) {
		sz = hd->ioc->req_depth * sizeof(int);
		mem = kmalloc(sz, GFP_ATOMIC);
		if (mem == NULL)
			return -1;

		hd->ReqToChain = (int *) mem;
	}
	for (ii = 0; ii < hd->ioc->req_depth; ii++)
		hd->ReqToChain[ii] = MPT_HOST_NO_CHAIN;

	/* ChainToChain size must equal the total number
	 * of chain buffers to be allocated.
	 * index = chain_idx
	 *
	 * Calculate the number of chain buffers needed(plus 1) per I/O
	 * then multiply the the maximum number of simultaneous cmds
	 *
	 * num_sge = num sge in request frame + last chain buffer
	 * scale = num sge per chain buffer if no chain element
	 */
	scale = hd->ioc->req_sz/(sizeof(dma_addr_t) + sizeof(u32));
	if (sizeof(dma_addr_t) == sizeof(u64))
		num_sge =  scale + (hd->ioc->req_sz - 60) / (sizeof(dma_addr_t) + sizeof(u32));
	else
		num_sge =  1+ scale + (hd->ioc->req_sz - 64) / (sizeof(dma_addr_t) + sizeof(u32));

	num_chain = 1;
	while (hd->max_sge - num_sge > 0) {
		num_chain++;
		num_sge += (scale - 1);
	}
	num_chain++;

	if ((int) hd->ioc->chip_type > (int) FC929)
		num_chain *= MPT_SCSI_CAN_QUEUE;
	else
		num_chain *= MPT_FC_CAN_QUEUE;

	hd->num_chain = num_chain;

	sz = num_chain * sizeof(int);
	if (hd->ChainToChain == NULL) {
		mem = kmalloc(sz, GFP_ATOMIC);
		if (mem == NULL)
			return -1;

		hd->ChainToChain = (int *) mem;
	} else {
		mem = (u8 *) hd->ChainToChain;
	}
	memset(mem, 0xFF, sz);

	sz = num_chain * hd->ioc->req_sz;
	if (hd->ChainBuffer == NULL) {
		/* Allocate free chain buffer pool
		 */
		mem = pci_alloc_consistent(hd->ioc->pcidev, sz, &hd->ChainBufferDMA);
		if (mem == NULL)
			return -1;

		hd->ChainBuffer = (u8*)mem;
	} else {
		mem = (u8 *) hd->ChainBuffer;
	}
	memset(mem, 0, sz);

	dprintk((KERN_INFO "  ChainBuffer    @ %p(%p), sz=%d\n",
		 hd->ChainBuffer, (void *)(ulong)hd->ChainBufferDMA, sz));

	/* Initialize the free chain Q.
	 */
	if (init) {
		spin_lock_init(&hd->FreeChainQlock);
	}

	spin_lock_irqsave (&hd->FreeChainQlock, flags);
	Q_INIT(&hd->FreeChainQ, MPT_FRAME_HDR);

	/* Post the chain buffers to the FreeChainQ.
	 */
	mem = (u8 *)hd->ChainBuffer;
	for (ii=0; ii < num_chain; ii++) {
		chain = (MPT_FRAME_HDR *) mem;
		Q_ADD_TAIL(&hd->FreeChainQ.head, &chain->u.frame.linkage, MPT_FRAME_HDR);
		mem += hd->ioc->req_sz;
	}
	spin_unlock_irqrestore(&hd->FreeChainQlock, flags);

	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Hack! It might be nice to report if a device is returning QUEUE_FULL
 *  but maybe not each and every time...
 */
static long last_queue_full = 0;

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_report_queue_full - Report QUEUE_FULL status returned
 *	from a SCSI target device.
 *	@sc: Pointer to Scsi_Cmnd structure
 *	@pScsiReply: Pointer to SCSIIOReply_t
 *	@pScsiReq: Pointer to original SCSI request
 *
 *	This routine periodically reports QUEUE_FULL status returned from a
 *	SCSI target device.  It reports this to the console via kernel
 *	printk() API call, not more than once every 10 seconds.
 */
static void
mptscsih_report_queue_full(Scsi_Cmnd *sc, SCSIIOReply_t *pScsiReply, SCSIIORequest_t *pScsiReq)
{
	long time = jiffies;

	if (time - last_queue_full > 10 * HZ) {
		char *ioc_str = "ioc?";

		if (sc->host != NULL && sc->host->hostdata != NULL)
			ioc_str = ((MPT_SCSI_HOST *)sc->host->hostdata)->ioc->name;
		printk(MYIOC_s_WARN_FMT "Device (%d:%d:%d) reported QUEUE_FULL!\n",
				ioc_str, 0, sc->target, sc->lun);
		last_queue_full = time;
	}
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
static int BeenHereDoneThat = 0;
static char *info_kbuf = NULL;

/*  SCSI host fops start here...  */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_detect - Register MPT adapter(s) as SCSI host(s) with
 *	linux scsi mid-layer.
 *	@tpnt: Pointer to Scsi_Host_Template structure
 *
 *	(linux Scsi_Host_Template.detect routine)
 *
 *	Returns number of SCSI host adapters that were successfully
 *	registered with the linux scsi mid-layer via the scsi_register()
 *	API call.
 */
int
mptscsih_detect(Scsi_Host_Template *tpnt)
{
	struct Scsi_Host	*sh;
	MPT_SCSI_HOST		*hd;
	MPT_ADAPTER		*ioc;
	MPT_DONE_Q		*freedoneQ;
	unsigned long		 flags;
	int			 sz, ii;
	int			 numSGE = 0;
	int			 scale;
	int			 ioc_cap;
	u8			*mem;

	if (! BeenHereDoneThat++) {
		show_mptmod_ver(my_NAME, my_VERSION);

		ScsiDoneCtx = mpt_register(mptscsih_io_done, MPTSCSIH_DRIVER);
		ScsiTaskCtx = mpt_register(mptscsih_taskmgmt_complete, MPTSCSIH_DRIVER);
		ScsiScanDvCtx = mpt_register(mptscsih_scandv_complete, MPTSCSIH_DRIVER);

#ifndef MPT_SCSI_USE_NEW_EH
		spin_lock_init(&mytaskQ_lock);
#endif

		if (mpt_event_register(ScsiDoneCtx, mptscsih_event_process) == 0) {
			dprintk((KERN_INFO MYNAM ": Registered for IOC event notifications\n"));
		} else {
			/* FIXME! */
		}

		if (mpt_reset_register(ScsiDoneCtx, mptscsih_ioc_reset) == 0) {
			dprintk((KERN_INFO MYNAM ": Registered for IOC reset notifications\n"));
		} else {
			/* FIXME! */
		}
	}
	dprintk((KERN_INFO MYNAM ": mpt_scsih_detect()\n"));

#ifdef MODULE
	/* Evaluate the command line arguments, if any */
	if (mptscsih)
		mptscsih_setup(mptscsih);
#endif
#ifndef MPT_SCSI_USE_NEW_EH
	atomic_set(&mpt_taskQdepth, 0);
#endif

	for (ioc = mpt_adapter_find_first(); ioc != NULL; ioc = mpt_adapter_find_next(ioc)) {
		/* 20010202 -sralston
		 *  Added sanity check on readiness of the MPT adapter.
		 */
		if (ioc->last_state != MPI_IOC_STATE_OPERATIONAL) {
			printk(MYIOC_s_WARN_FMT "Skipping because it's not operational!\n",
					ioc->name);
			continue;
		}

		if (!ioc->active) {
			printk(MYIOC_s_WARN_FMT "Skipping because it's disabled!\n",
					ioc->name);
			continue;
		}


		/*  Sanity check - ensure at least 1 port is INITIATOR capable
		 */
		ioc_cap = 0;
		for (ii=0; ii < ioc->facts.NumberOfPorts; ii++) {
			if (ioc->pfacts[ii].ProtocolFlags & MPI_PORTFACTS_PROTOCOL_INITIATOR)
				ioc_cap ++;
		}

		if (!ioc_cap) {
			printk(MYIOC_s_WARN_FMT "Skipping ioc=%p because SCSI Initiator mode is NOT enabled!\n",
					ioc->name, ioc);
			continue;
		}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
		tpnt->proc_dir = &proc_mpt_scsihost;
#endif
		tpnt->proc_info = mptscsih_proc_info;
		sh = scsi_register(tpnt, sizeof(MPT_SCSI_HOST));
		if (sh != NULL) {
			spin_lock_irqsave(&ioc->FreeQlock, flags);
			sh->io_port = 0;
			sh->n_io_port = 0;
			sh->irq = 0;

			/* Yikes!  This is important!
			 * Otherwise, by default, linux
			 * only scans target IDs 0-7!
			 * pfactsN->MaxDevices unreliable
			 * (not supported in early
			 *	versions of the FW).
			 * max_id = 1 + actual max id,
			 * max_lun = 1 + actual last lun,
			 *	see hosts.h :o(
			 */
			if ((int)ioc->chip_type > (int)FC929)
				sh->max_id = MPT_MAX_SCSI_DEVICES;
			else {
				/* For FC, increase the queue depth
				 * from MPT_SCSI_CAN_QUEUE (31)
				 * to MPT_FC_CAN_QUEUE (63).
				 */
				sh->can_queue = MPT_FC_CAN_QUEUE;
				sh->max_id = MPT_MAX_FC_DEVICES<256 ? MPT_MAX_FC_DEVICES : 255;
			}
			sh->max_lun = MPT_LAST_LUN + 1;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,7)
			sh->max_sectors = MPT_SCSI_MAX_SECTORS;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20) || defined CONFIG_HIGHIO
			sh->highmem_io = 1;
#endif
			/* MPI uses {port, bus, id, lun}, but logically maps
			 * devices on different ports to different buses, i.e.,
			 * bus 1 may be the 2nd bus on port 0 or the 1st bus on port 1.
			 * Map bus to channel, ignore port number in SCSI....
			 *	hd->port = 0;
			 * If max_channel > 0, need to adjust mem alloc, free, DV
			 * and all access to VirtDev
			 */
			sh->max_channel = 0;
			sh->this_id = ioc->pfacts[0].PortSCSIID;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,44)
			/* OS entry to allow host drivers to force
			 * a queue depth on a per device basis.
			 */
			sh->select_queue_depths = mptscsih_select_queue_depths;
#endif
			/* Required entry.
			 */
			sh->unique_id = ioc->id;

			/* Verify that we won't exceed the maximum
			 * number of chain buffers
			 * We can optimize:  ZZ = req_sz/sizeof(SGE)
			 * For 32bit SGE's:
			 *  numSGE = 1 + (ZZ-1)*(maxChain -1) + ZZ
			 *               + (req_sz - 64)/sizeof(SGE)
			 * A slightly different algorithm is required for
			 * 64bit SGEs.
			 */
			scale = ioc->req_sz/(sizeof(dma_addr_t) + sizeof(u32));
			if (sizeof(dma_addr_t) == sizeof(u64)) {
				numSGE = (scale - 1) * (ioc->facts.MaxChainDepth-1) + scale +
					(ioc->req_sz - 60) / (sizeof(dma_addr_t) + sizeof(u32));
			} else {
				numSGE = 1 + (scale - 1) * (ioc->facts.MaxChainDepth-1) + scale +
					(ioc->req_sz - 64) / (sizeof(dma_addr_t) + sizeof(u32));
			}

			if (numSGE < sh->sg_tablesize) {
				/* Reset this value */
				dprintk((MYIOC_s_INFO_FMT
					 "Resetting sg_tablesize to %d from %d\n",
					 ioc->name, numSGE, sh->sg_tablesize));
				sh->sg_tablesize = numSGE;
			}

			/* Set the pci device pointer in Scsi_Host structure.
			 */
			scsi_set_pci_device(sh, ioc->pcidev);

			spin_unlock_irqrestore(&ioc->FreeQlock, flags);

			hd = (MPT_SCSI_HOST *) sh->hostdata;
			hd->ioc = ioc;
			hd->max_sge = sh->sg_tablesize;

			if ((int)ioc->chip_type > (int)FC929)
				hd->is_spi = 1;

			if (DmpService &&
			    (ioc->chip_type == FC919 || ioc->chip_type == FC929))
				hd->is_multipath = 1;

			/* SCSI needs Scsi_Cmnd lookup table!
			 * (with size equal to req_depth*PtrSz!)
			 */
			sz = hd->ioc->req_depth * sizeof(void *);
			mem = kmalloc(sz, GFP_ATOMIC);
			if (mem == NULL)
				goto done;

			memset(mem, 0, sz);
			hd->ScsiLookup = (struct scsi_cmnd **) mem;

			dprintk((MYIOC_s_INFO_FMT "ScsiLookup @ %p, sz=%d\n",
				 ioc->name, hd->ScsiLookup, sz));

			if (mptscsih_initChainBuffers(hd, 1) < 0)
				goto done;

			/* Allocate memory for free and doneQ's
			 */
			sz = sh->can_queue * sizeof(MPT_DONE_Q);
			mem = kmalloc(sz, GFP_ATOMIC);
			if (mem == NULL)
				goto done;

			memset(mem, 0xFF, sz);
			hd->memQ = mem;

			/* Initialize the free, done and pending Qs.
			 */
			Q_INIT(&hd->freeQ, MPT_DONE_Q);
			Q_INIT(&hd->doneQ, MPT_DONE_Q);
			Q_INIT(&hd->pendingQ, MPT_DONE_Q);
			spin_lock_init(&hd->freedoneQlock);

			mem = hd->memQ;
			for (ii=0; ii < sh->can_queue; ii++) {
				freedoneQ = (MPT_DONE_Q *) mem;
				Q_ADD_TAIL(&hd->freeQ.head, freedoneQ, MPT_DONE_Q);
				mem += sizeof(MPT_DONE_Q);
			}

			/* Initialize this Scsi_Host
			 * internal task Q.
			 */
			Q_INIT(&hd->taskQ, MPT_FRAME_HDR);
			hd->taskQcnt = 0;

			/* Allocate memory for the device structures.
			 * A non-Null pointer at an offset
			 * indicates a device exists.
			 * max_id = 1 + maximum id (hosts.h)
			 */
			sz = sh->max_id * sizeof(void *);
			mem = kmalloc(sz, GFP_ATOMIC);
			if (mem == NULL)
				goto done;

			memset(mem, 0, sz);
			hd->Targets = (VirtDevice **) mem;

			dprintk((KERN_INFO "  Targets @ %p, sz=%d\n", hd->Targets, sz));


			/* Clear the TM flags
			 */
			hd->tmPending = 0;
#ifdef MPT_SCSI_USE_NEW_EH
			hd->tmState = TM_STATE_NONE;
#endif
			hd->resetPending = 0;
			hd->abortSCpnt = NULL;
			hd->tmPtr = NULL;
			hd->numTMrequests = 0;

			/* Clear the pointer used to store
			 * single-threaded commands, i.e., those
			 * issued during a bus scan, dv and
			 * configuration pages.
			 */
			hd->cmdPtr = NULL;

			/* Attach the SCSI Host to the IOC structure
			 */
			ioc->sh = sh;

			/* Initialize this SCSI Hosts' timers
			 * To use, set the timer expires field
			 * and add_timer
			 */
			init_timer(&hd->timer);
			hd->timer.data = (unsigned long) hd;
			hd->timer.function = mptscsih_timer_expired;

			init_timer(&hd->TMtimer);
			hd->TMtimer.data = (unsigned long) hd;
			hd->TMtimer.function = mptscsih_taskmgmt_timeout;
			hd->qtag_tick = jiffies;

			/* Moved Earlier Pam D */
			/* ioc->sh = sh;	*/

#ifdef MPTSCSIH_DBG_TIMEOUT
			hd->ioc->timeout_hard = 0;
			hd->ioc->timeout_delta = 30 * HZ;
			hd->ioc->timeout_maxcnt = 0;
			hd->ioc->timeout_cnt = 0;
			for (ii=0; ii < 8; ii++)
				foo_to[ii] = NULL;
#endif

			if (hd->is_spi) {
				/* Update with the driver setup
				 * values.
				 */
				if (hd->ioc->spi_data.maxBusWidth > driver_setup.max_width)
					hd->ioc->spi_data.maxBusWidth = driver_setup.max_width;
				if (hd->ioc->spi_data.minSyncFactor < driver_setup.min_sync_fac)
					hd->ioc->spi_data.minSyncFactor = driver_setup.min_sync_fac;

				if (hd->ioc->spi_data.minSyncFactor == MPT_ASYNC)
					hd->ioc->spi_data.maxSyncOffset = 0;

				hd->ioc->spi_data.Saf_Te = driver_setup.saf_te;

				hd->negoNvram = 0;
#ifdef MPTSCSIH_DISABLE_DOMAIN_VALIDATION
				hd->negoNvram = MPT_SCSICFG_USE_NVRAM;
#endif
				if (driver_setup.dv == 0)
					hd->negoNvram = MPT_SCSICFG_USE_NVRAM;

				hd->ioc->spi_data.forceDv = 0;
				for (ii=0; ii < MPT_MAX_SCSI_DEVICES; ii++)
					hd->ioc->spi_data.dvStatus[ii] = MPT_SCSICFG_NEGOTIATE;
	
				if (hd->negoNvram == 0) {
					for (ii=0; ii < MPT_MAX_SCSI_DEVICES; ii++)
						hd->ioc->spi_data.dvStatus[ii] |= MPT_SCSICFG_DV_NOT_DONE;
				}

				ddvprintk((MYIOC_s_INFO_FMT
					"dv %x width %x factor %x saf_te %x\n",
					hd->ioc->name, driver_setup.dv,
					driver_setup.max_width,
					driver_setup.min_sync_fac,
					driver_setup.saf_te));
			}

			mpt_scsi_hosts++;
		}
	}

done:
	if (mpt_scsi_hosts > 0)
		register_reboot_notifier(&mptscsih_notifier);
	else {
		mpt_reset_deregister(ScsiDoneCtx);
		dprintk((KERN_INFO MYNAM ": Deregistered for IOC reset notifications\n"));

		mpt_event_deregister(ScsiDoneCtx);
		dprintk((KERN_INFO MYNAM ": Deregistered for IOC event notifications\n"));

		mpt_deregister(ScsiScanDvCtx);
		mpt_deregister(ScsiTaskCtx);
		mpt_deregister(ScsiDoneCtx);

		if (info_kbuf != NULL)
			kfree(info_kbuf);
	}

	return mpt_scsi_hosts;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_release - Unregister SCSI host from linux scsi mid-layer
 *	@host: Pointer to Scsi_Host structure
 *
 *	(linux Scsi_Host_Template.release routine)
 *	This routine releases all resources associated with the SCSI host
 *	adapter.
 *
 *	Returns 0 for success.
 */
int
mptscsih_release(struct Scsi_Host *host)
{
	MPT_SCSI_HOST	*hd;
	int 		 count;
	unsigned long	 flags;

	hd = (MPT_SCSI_HOST *) host->hostdata;

#ifndef MPT_SCSI_USE_NEW_EH
#ifndef MPTSCSIH_DISABLE_DOMAIN_VALIDATION
	spin_lock_irqsave(&dvtaskQ_lock, flags);
	dvtaskQ_release = 1;
	spin_unlock_irqrestore(&dvtaskQ_lock, flags);
#endif

	count = 10 * HZ;
	spin_lock_irqsave(&mytaskQ_lock, flags);
	if (mytaskQ_bh_active) {
		spin_unlock_irqrestore(&mytaskQ_lock, flags);
		dprintk((KERN_INFO MYNAM ": Info: Zapping TaskMgmt thread!\n"));
		clean_taskQ(hd);

		while(mytaskQ_bh_active && --count) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(1);
		}
	} else {
		spin_unlock_irqrestore(&mytaskQ_lock, flags);
	}
	if (!count)
		printk(KERN_ERR MYNAM ": ERROR - TaskMgmt thread still active!\n");

#endif

#ifndef MPTSCSIH_DISABLE_DOMAIN_VALIDATION
	/* Check DV thread active */
	count = 10 * HZ;
	spin_lock_irqsave(&dvtaskQ_lock, flags);
	if (dvtaskQ_active) {
		spin_unlock_irqrestore(&dvtaskQ_lock, flags);
		while(dvtaskQ_active && --count) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(1);
		}
	} else {
		spin_unlock_irqrestore(&dvtaskQ_lock, flags);
	}
	if (!count)
		printk(KERN_ERR MYNAM ": ERROR - DV thread still active!\n");
#if defined(MPT_DEBUG_DV) || defined(MPT_DEBUG_DV_TINY)
	else
		printk(KERN_ERR MYNAM ": DV thread orig %d, count %d\n", 10 * HZ, count);
#endif
#endif

	unregister_reboot_notifier(&mptscsih_notifier);

	if (hd != NULL) {
		int sz1, sz2, sz3, sztarget=0;
		int szr2chain = 0;
		int szc2chain = 0;
		int szchain = 0;
		int szQ = 0;

		/* Synchronize disk caches
		 */
		(void) mptscsih_synchronize_cache(hd, 0);

		sz1 = sz2 = sz3 = 0;

		if (hd->ScsiLookup != NULL) {
			sz1 = hd->ioc->req_depth * sizeof(void *);
			kfree(hd->ScsiLookup);
			hd->ScsiLookup = NULL;
		}

		if (hd->ReqToChain != NULL) {
			szr2chain = hd->ioc->req_depth * sizeof(int);
			kfree(hd->ReqToChain);
			hd->ReqToChain = NULL;
		}

		if (hd->ChainToChain != NULL) {
			szc2chain = hd->num_chain * sizeof(int);
			kfree(hd->ChainToChain);
			hd->ChainToChain = NULL;
		}

		if (hd->ChainBuffer != NULL) {
			sz2 = hd->num_chain * hd->ioc->req_sz;
			szchain = szr2chain + szc2chain + sz2;

			pci_free_consistent(hd->ioc->pcidev, sz2,
				    hd->ChainBuffer, hd->ChainBufferDMA);
			hd->ChainBuffer = NULL;
		}

		if (hd->memQ != NULL) {
			szQ = host->can_queue * sizeof(MPT_DONE_Q);
			kfree(hd->memQ);
			hd->memQ = NULL;
		}

		if (hd->Targets != NULL) {
			int max, ii;

			/*
			 * Free any target structures that were allocated.
			 */
			if (hd->is_spi) {
				max = MPT_MAX_SCSI_DEVICES;
			} else {
				max = MPT_MAX_FC_DEVICES<256 ? MPT_MAX_FC_DEVICES : 255;
			}
			for (ii=0; ii < max; ii++) {
				if (hd->Targets[ii]) {
					kfree(hd->Targets[ii]);
					hd->Targets[ii] = NULL;
					sztarget += sizeof(VirtDevice);
				}
			}

			/*
			 * Free pointer array.
			 */
			sz3 = max * sizeof(void *);
			kfree(hd->Targets);
			hd->Targets = NULL;
		}

		dprintk((MYIOC_s_INFO_FMT "Free'd ScsiLookup (%d), chain (%d) and Target (%d+%d) memory\n",
				hd->ioc->name, sz1, szchain, sz3, sztarget));
		dprintk(("Free'd done and free Q (%d) memory\n", szQ));
	}
	/* NULL the Scsi_Host pointer
	 */
	hd->ioc->sh = NULL;
	scsi_unregister(host);

	if (mpt_scsi_hosts) {
		if (--mpt_scsi_hosts == 0) {
			mpt_reset_deregister(ScsiDoneCtx);
			dprintk((KERN_INFO MYNAM ": Deregistered for IOC reset notifications\n"));

			mpt_event_deregister(ScsiDoneCtx);
			dprintk((KERN_INFO MYNAM ": Deregistered for IOC event notifications\n"));

			mpt_deregister(ScsiScanDvCtx);
			mpt_deregister(ScsiTaskCtx);
			mpt_deregister(ScsiDoneCtx);

			if (info_kbuf != NULL)
				kfree(info_kbuf);
		}
	}

	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_halt - Process the reboot notification
 *	@nb: Pointer to a struct notifier_block (ignored)
 *	@event: event (SYS_HALT, SYS_RESTART, SYS_POWER_OFF)
 *	@buf: Pointer to a data buffer (ignored)
 *
 *	This routine called if a system shutdown or reboot is to occur.
 *
 *	Return NOTIFY_DONE if this is something other than a reboot message.
 *		NOTIFY_OK if this is a reboot message.
 */
static int
mptscsih_halt(struct notifier_block *nb, ulong event, void *buf)
{
	MPT_ADAPTER *ioc;
	MPT_SCSI_HOST *hd;

	/* Ignore all messages other than reboot message
	 */
	if ((event != SYS_RESTART) && (event != SYS_HALT)
		&& (event != SYS_POWER_OFF))
		return (NOTIFY_DONE);

	for (ioc = mpt_adapter_find_first(); ioc != NULL; ioc =	mpt_adapter_find_next(ioc)) {
		/* Flush the cache of this adapter
		 */
		if (ioc->sh) {
			hd = (MPT_SCSI_HOST *) ioc->sh->hostdata;
			if (hd) {
				mptscsih_synchronize_cache(hd, 0);
			}
		}
	}

	unregister_reboot_notifier(&mptscsih_notifier);
	return NOTIFY_OK;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_info - Return information about MPT adapter
 *	@SChost: Pointer to Scsi_Host structure
 *
 *	(linux Scsi_Host_Template.info routine)
 *
 *	Returns pointer to buffer where information was written.
 */
const char *
mptscsih_info(struct Scsi_Host *SChost)
{
	MPT_SCSI_HOST *h;
	int size = 0;

	if (info_kbuf == NULL)
		if ((info_kbuf = kmalloc(0x1000 /* 4Kb */, GFP_KERNEL)) == NULL)
			return info_kbuf;

	h = (MPT_SCSI_HOST *)SChost->hostdata;
	info_kbuf[0] = '\0';
	if (h) {
		mpt_print_ioc_summary(h->ioc, info_kbuf, &size, 0, 0);
		info_kbuf[size-1] = '\0';
	}

	return info_kbuf;
}

struct info_str {
	char *buffer;
	int   length;
	int   offset;
	int   pos;
};

static void copy_mem_info(struct info_str *info, char *data, int len)
{
	if (info->pos + len > info->length)
		len = info->length - info->pos;

	if (info->pos + len < info->offset) {
		info->pos += len;
		return;
	}

	if (info->pos < info->offset) {
	        data += (info->offset - info->pos);
	        len  -= (info->offset - info->pos);
	}

	if (len > 0) {
                memcpy(info->buffer + info->pos, data, len);
                info->pos += len;
	}
}

static int copy_info(struct info_str *info, char *fmt, ...)
{
	va_list args;
	char buf[81];
	int len;

	va_start(args, fmt);
	len = vsprintf(buf, fmt, args);
	va_end(args);

	copy_mem_info(info, buf, len);
	return len;
}

static int mptscsih_host_info(MPT_ADAPTER *ioc, char *pbuf, off_t offset, int len)
{
	struct info_str info;

	info.buffer	= pbuf;
	info.length	= len;
	info.offset	= offset;
	info.pos	= 0;

	copy_info(&info, "%s: %s, ", ioc->name, ioc->prod_name);
	copy_info(&info, "%s%08xh, ", MPT_FW_REV_MAGIC_ID_STRING, ioc->facts.FWVersion.Word);
	copy_info(&info, "Ports=%d, ", ioc->facts.NumberOfPorts);
	copy_info(&info, "MaxQ=%d\n", ioc->req_depth);

	return ((info.pos > info.offset) ? info.pos - info.offset : 0);
}

#ifndef MPTSCSIH_DBG_TIMEOUT
static int mptscsih_user_command(MPT_ADAPTER *ioc, char *pbuf, int len)
{
	/* Not yet implemented */
	return len;
}
#else
#define is_digit(c)	((c) >= '0' && (c) <= '9')
#define digit_to_bin(c)	((c) - '0')
#define is_space(c)	((c) == ' ' || (c) == '\t')

#define UC_DBG_TIMEOUT		0x01
#define UC_DBG_HARDRESET	0x02

static int skip_spaces(char *ptr, int len)
{
	int cnt, c;

	for (cnt = len; cnt > 0 && (c = *ptr++) && is_space(c); cnt --);

	return (len - cnt);
}

static int get_int_arg(char *ptr, int len, ulong *pv)
{
	int cnt, c;
	ulong	v;
	for (v =  0, cnt = len; cnt > 0 && (c=*ptr++) && is_digit(c); cnt --) {
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
	if ((arg_len = skip_spaces(ptr,len)) < (min_spaces))		\
		return -EINVAL;						\
	ptr += arg_len;							\
	len -= arg_len;

#define GET_INT_ARG(v)							\
	if (!(arg_len = get_int_arg(ptr,len, &(v))))			\
		return -EINVAL;						\
	ptr += arg_len;							\
	len -= arg_len;

static int mptscsih_user_command(MPT_ADAPTER *ioc, char *buffer, int length)
{
	char *ptr = buffer;
	char btmp[24];	/* REMOVE */
	int arg_len;
	int len	= length;
	int cmd;
	ulong number = 1;
	ulong delta = 10;

	if ((len > 0) && (ptr[len -1] == '\n'))
		--len;

	if (len < 22) {
		strncpy(btmp, buffer, len);
		btmp[len+1]='\0';
	} else {
		strncpy(btmp, buffer, 22);
		btmp[23]='\0';
	}
	printk("user_command:  ioc %d, buffer %s, length %d\n",
			ioc->id, btmp, length);

	if ((arg_len = is_keyword(ptr, len, "timeout")) != 0)
		cmd = UC_DBG_TIMEOUT;
	else if ((arg_len = is_keyword(ptr, len, "hardreset")) != 0)
		cmd = UC_DBG_HARDRESET;
	else
		return -EINVAL;

	ptr += arg_len;
	len -= arg_len;

	switch(cmd) {
		case UC_DBG_TIMEOUT:
			SKIP_SPACES(1);
			GET_INT_ARG(number);
			SKIP_SPACES(1);
			GET_INT_ARG(delta);
			break;
	}

	printk("user_command: cnt=%ld delta=%ld\n", number, delta);

	if (len)
		return -EINVAL;
	else {
		if (cmd == UC_DBG_HARDRESET) {
			ioc->timeout_hard = 1;
		} else if (cmd == UC_DBG_TIMEOUT) {
			/* process this command ...
			 */
			ioc->timeout_maxcnt = 0;
			ioc->timeout_delta = delta < 2 ? 2 : delta;
			ioc->timeout_cnt = 0;
			ioc->timeout_maxcnt = number < 8 ? number: 8;
		}
	}
	/* Not yet implemented */
	return length;
}
#endif


/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_proc_info - Return information about MPT adapter
 *
 *	(linux Scsi_Host_Template.info routine)
 *
 * 	buffer: if write, user data; if read, buffer for user
 * 	length: if write, return length;
 * 	offset: if write, 0; if read, the current offset into the buffer from
 * 		the previous read.
 * 	hostno: scsi host number
 *	func:   if write = 1; if read = 0
 */
int mptscsih_proc_info(char *buffer, char **start, off_t offset,
			int length, int hostno, int func)
{
	MPT_ADAPTER	*ioc;
	MPT_SCSI_HOST	*hd = NULL;
	int size = 0;

	dprintk(("Called mptscsih_proc_info: hostno=%d, func=%d\n", hostno, func));
	dprintk(("buffer %p, start=%p (%p) offset=%ld length = %d\n",
			buffer, start, *start, offset, length));

	for (ioc = mpt_adapter_find_first(); ioc != NULL; ioc = mpt_adapter_find_next(ioc)) {
		if ((ioc->sh) && (ioc->sh->host_no == hostno)) {
			hd = (MPT_SCSI_HOST *)ioc->sh->hostdata;
			break;
		}
	}
	if ((ioc == NULL) || (ioc->sh == NULL) || (hd == NULL))
		return 0;

	if (func) {
		size = mptscsih_user_command(ioc, buffer, length);
	} else {
		if (start)
			*start = buffer;

		size = mptscsih_host_info(ioc, buffer, offset, length);
	}

	return size;
}


/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
	static int max_qd = 1;
#if 0
static int index_log[128];
static int index_ent = 0;
static __inline__ void ADD_INDEX_LOG(int req_ent)
{
	int i = index_ent++;

	index_log[i & (128 - 1)] = req_ent;
}
#else
#define ADD_INDEX_LOG(req_ent)	do { } while(0)
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_put_msgframe - Wrapper routine to post message frame to F/W.
 *	@context: Call back context (ScsiDoneCtx, ScsiScanDvCtx)
 *	@id: IOC id number
 *	@mf: Pointer to message frame
 *
 *	Handles the call to mptbase for posting request and queue depth
 *	tracking.
 *
 *	Returns none.
 */
static inline void
mptscsih_put_msgframe(int context, int id, MPT_FRAME_HDR *mf)
{
	/* Main banana... */
	atomic_inc(&queue_depth);
	if (atomic_read(&queue_depth) > max_qd) {
		max_qd = atomic_read(&queue_depth);
		dprintk((KERN_INFO MYNAM ": Queue depth now %d.\n", max_qd));
	}

	mpt_put_msg_frame(context, id, mf);

	return;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_qcmd - Primary Fusion MPT SCSI initiator IO start routine.
 *	@SCpnt: Pointer to Scsi_Cmnd structure
 *	@done: Pointer SCSI mid-layer IO completion function
 *
 *	(linux Scsi_Host_Template.queuecommand routine)
 *	This is the primary SCSI IO start routine.  Create a MPI SCSIIORequest
 *	from a linux Scsi_Cmnd request and send it to the IOC.
 *
 *	Returns 0. (rtn value discarded by linux scsi mid-layer)
 */
int
mptscsih_qcmd(Scsi_Cmnd *SCpnt, void (*done)(Scsi_Cmnd *))
{
	MPT_SCSI_HOST		*hd;
	MPT_FRAME_HDR		*mf;
	SCSIIORequest_t		*pScsiReq;
	VirtDevice		*pTarget;
	MPT_DONE_Q		*buffer;
	unsigned long		 flags;
	int	 target;
	int	 lun;
	int	 datadir;
	u32	 datalen;
	u32	 scsictl;
	u32	 scsidir;
	u32	 cmd_len;
	int	 my_idx;
	int	 ii;
	int	 rc;
	int	 did_errcode;
	int	 issueCmd;

	did_errcode = 0;
	hd = (MPT_SCSI_HOST *) SCpnt->host->hostdata;
	target = SCpnt->target;
	lun = SCpnt->lun;
	SCpnt->scsi_done = done;

	pTarget = hd->Targets[target];

	dmfprintk((MYIOC_s_INFO_FMT "qcmd: SCpnt=%p, done()=%p\n",
			(hd && hd->ioc) ? hd->ioc->name : "ioc?", SCpnt, done));

	if (hd->resetPending) {
		/* Prevent new commands from being issued
		 * while reloading the FW. Reset timer to 60 seconds,
		 * as the FW can take some time to come ready.
		 * For New EH, cmds on doneQ posted to FW.
		 */
		did_errcode = 1;
		mod_timer(&SCpnt->eh_timeout, jiffies + (HZ * 60));
		dtmprintk((MYIOC_s_WARN_FMT "qcmd: SCpnt=%p timeout + 60HZ\n",
			(hd && hd->ioc) ? hd->ioc->name : "ioc?", SCpnt));
		goto did_error;
	}

	/*
	 *  Put together a MPT SCSI request...
	 */
	if ((mf = mpt_get_msg_frame(ScsiDoneCtx, hd->ioc->id)) == NULL) {
		dprintk((MYIOC_s_WARN_FMT "QueueCmd, no msg frames!!\n",
				hd->ioc->name));
		did_errcode = 2;
		goto did_error;
	}

	pScsiReq = (SCSIIORequest_t *) mf;

	my_idx = le16_to_cpu(mf->u.frame.hwhdr.msgctxu.fld.req_idx);

	ADD_INDEX_LOG(my_idx);

	/*
	 *  The scsi layer should be handling this stuff
	 *  (In 2.3.x it does -DaveM)
	 */

	/*  BUG FIX!  19991030 -sralston
	 *    TUR's being issued with scsictl=0x02000000 (DATA_IN)!
	 *    Seems we may receive a buffer (datalen>0) even when there
	 *    will be no data transfer!  GRRRRR...
	 */
	datadir = mptscsih_io_direction(SCpnt);
	if (datadir == SCSI_DATA_READ) {
		datalen = SCpnt->request_bufflen;
		scsidir = MPI_SCSIIO_CONTROL_READ;	/* DATA IN  (host<--ioc<--dev) */
	} else if (datadir == SCSI_DATA_WRITE) {
		datalen = SCpnt->request_bufflen;
		scsidir = MPI_SCSIIO_CONTROL_WRITE;	/* DATA OUT (host-->ioc-->dev) */
	} else {
		datalen = 0;
		scsidir = MPI_SCSIIO_CONTROL_NODATATRANSFER;
	}

	/* Default to untagged. Once a target structure has been allocated,
	 * use the Inquiry data to determine if device supports tagged.
	 */
	if (   pTarget
	    && (pTarget->tflags & MPT_TARGET_FLAGS_Q_YES)
	    && (SCpnt->device->tagged_supported)) {
		scsictl = scsidir | MPI_SCSIIO_CONTROL_SIMPLEQ;
	} else {
		scsictl = scsidir | MPI_SCSIIO_CONTROL_UNTAGGED;
	}

	/* Use the above information to set up the message frame
	 */
	pScsiReq->TargetID = (u8) target;
	pScsiReq->Bus = (u8) SCpnt->channel;
	pScsiReq->ChainOffset = 0;
	pScsiReq->Function = MPI_FUNCTION_SCSI_IO_REQUEST;
	pScsiReq->CDBLength = SCpnt->cmd_len;
	pScsiReq->SenseBufferLength = MPT_SENSE_BUFFER_SIZE;
	pScsiReq->Reserved = 0;
	pScsiReq->MsgFlags = mpt_msg_flags();
	pScsiReq->LUN[0] = 0;
	pScsiReq->LUN[1] = lun;
	pScsiReq->LUN[2] = 0;
	pScsiReq->LUN[3] = 0;
	pScsiReq->LUN[4] = 0;
	pScsiReq->LUN[5] = 0;
	pScsiReq->LUN[6] = 0;
	pScsiReq->LUN[7] = 0;
	pScsiReq->Control = cpu_to_le32(scsictl);

	/*
	 *  Write SCSI CDB into the message
	 *  Should write from cmd_len up to 16, but skip for performance reasons.
	 */
	cmd_len = SCpnt->cmd_len;
	for (ii=0; ii < cmd_len; ii++)
		pScsiReq->CDB[ii] = SCpnt->cmnd[ii];

	for (ii=cmd_len; ii < 16; ii++)
		pScsiReq->CDB[ii] = 0;

	/* DataLength */
	pScsiReq->DataLength = cpu_to_le32(datalen);

	/* SenseBuffer low address */
	pScsiReq->SenseBufferLowAddr = cpu_to_le32(hd->ioc->sense_buf_low_dma
					   + (my_idx * MPT_SENSE_BUFFER_ALLOC));

	/* Now add the SG list
	 * Always have a SGE even if null length.
	 */
	rc = SUCCESS;
	if (datalen == 0) {
		/* Add a NULL SGE */
		mptscsih_add_sge((char *)&pScsiReq->SGL, MPT_SGE_FLAGS_SSIMPLE_READ | 0,
			(dma_addr_t) -1);
	} else {
		/* Add a 32 or 64 bit SGE */
		rc = mptscsih_AddSGE(hd, SCpnt, pScsiReq, my_idx);
	}


	if (rc == SUCCESS) {
		hd->ScsiLookup[my_idx] = SCpnt;
		SCpnt->host_scribble = NULL;

		/* SCSI specific processing */
		issueCmd = 1;
		if (hd->is_spi) {
			int dvStatus = hd->ioc->spi_data.dvStatus[target];

			if (dvStatus || hd->ioc->spi_data.forceDv) {

				/* Write SDP1 on this I/O to this target */
				if (dvStatus & MPT_SCSICFG_NEGOTIATE) {
					mptscsih_writeSDP1(hd, 0, target, hd->negoNvram);
					dvStatus &= ~MPT_SCSICFG_NEGOTIATE;
					hd->ioc->spi_data.dvStatus[target] =  dvStatus;
				} else if (dvStatus & MPT_SCSICFG_BLK_NEGO) {
					mptscsih_writeSDP1(hd, 0, target, MPT_SCSICFG_BLK_NEGO);
					dvStatus &= ~MPT_SCSICFG_BLK_NEGO;
					hd->ioc->spi_data.dvStatus[target] =  dvStatus;
				}

#ifndef MPTSCSIH_DISABLE_DOMAIN_VALIDATION
				if ((dvStatus & MPT_SCSICFG_NEED_DV) ||
					(hd->ioc->spi_data.forceDv & MPT_SCSICFG_NEED_DV)) {
					unsigned long lflags;
					/* Schedule DV if necessary */
					spin_lock_irqsave(&dvtaskQ_lock, lflags);
					if (!dvtaskQ_active) {
						dvtaskQ_active = 1;
						spin_unlock_irqrestore(&dvtaskQ_lock, lflags);
						MPT_INIT_WORK(&mptscsih_dvTask, mptscsih_domainValidation, (void *) hd);

						SCHEDULE_TASK(&mptscsih_dvTask);
					} else {
						spin_unlock_irqrestore(&dvtaskQ_lock, lflags);
					}
					hd->ioc->spi_data.forceDv &= ~MPT_SCSICFG_NEED_DV;
				}

				/* Trying to do DV to this target, extend timeout.
				 * Wait to issue intil flag is clear
				 */
				if (dvStatus & MPT_SCSICFG_DV_PENDING) {
					mod_timer(&SCpnt->eh_timeout, jiffies + 40 * HZ);
					issueCmd = 0;
				}

				/* Set the DV flags.
				 */
				if (dvStatus & MPT_SCSICFG_DV_NOT_DONE)
					mptscsih_set_dvflags(hd, pScsiReq);
#endif
			}
		}

#ifdef MPTSCSIH_DBG_TIMEOUT
		if (hd->ioc->timeout_cnt < hd->ioc->timeout_maxcnt) {
			foo_to[hd->ioc->timeout_cnt] = SCpnt;
			hd->ioc->timeout_cnt++;
			//mod_timer(&SCpnt->eh_timeout, jiffies + hd->ioc->timeout_delta);
			issueCmd = 0;
			printk(MYIOC_s_WARN_FMT
				"to pendingQ: (sc=%p, mf=%p, time=%ld)\n",
				hd->ioc->name, SCpnt, mf, jiffies);
		}
#endif

		if (issueCmd) {
			mptscsih_put_msgframe(ScsiDoneCtx, hd->ioc->id, mf);
			dmfprintk((MYIOC_s_INFO_FMT "Issued SCSI cmd (%p) mf=%p idx=%d\n",
					hd->ioc->name, SCpnt, mf, my_idx));
		} else {
			ddvtprintk((MYIOC_s_INFO_FMT "Pending cmd=%p idx %d\n",
					hd->ioc->name, SCpnt, my_idx));
			/* Place this command on the pendingQ if possible */
			spin_lock_irqsave(&hd->freedoneQlock, flags);
			if (!Q_IS_EMPTY(&hd->freeQ)) {
				buffer = hd->freeQ.head;
				Q_DEL_ITEM(buffer);

				/* Save the mf pointer
				 */
				buffer->argp = (void *)mf;

				/* Add to the pendingQ
				 */
				Q_ADD_TAIL(&hd->pendingQ.head, buffer, MPT_DONE_Q);
				spin_unlock_irqrestore(&hd->freedoneQlock, flags);
			} else {
				spin_unlock_irqrestore(&hd->freedoneQlock, flags);
				SCpnt->result = (DID_BUS_BUSY << 16);
				SCpnt->scsi_done(SCpnt);
			}
		}
	} else {
		mptscsih_freeChainBuffers(hd, my_idx);
		mpt_free_msg_frame(ScsiDoneCtx, hd->ioc->id, mf);
		did_errcode = 3;
		goto did_error;
	}

	return 0;

did_error:
	dprintk((MYIOC_s_WARN_FMT "_qcmd did_errcode=%d (sc=%p)\n",
			hd->ioc->name, did_errcode, SCpnt));
	/* Just wish OS to issue a retry */
	SCpnt->result = (DID_BUS_BUSY << 16);
	spin_lock_irqsave(&hd->freedoneQlock, flags);
	if (!Q_IS_EMPTY(&hd->freeQ)) {
		dtmprintk((MYIOC_s_WARN_FMT "SCpnt=%p to doneQ\n",
			(hd && hd->ioc) ? hd->ioc->name : "ioc?", SCpnt));
		buffer = hd->freeQ.head;
		Q_DEL_ITEM(buffer);

		/* Set the Scsi_Cmnd pointer
		 */
		buffer->argp = (void *)SCpnt;

		/* Add to the doneQ
		 */
		Q_ADD_TAIL(&hd->doneQ.head, buffer, MPT_DONE_Q);
		spin_unlock_irqrestore(&hd->freedoneQlock, flags);
	} else {
		spin_unlock_irqrestore(&hd->freedoneQlock, flags);
		SCpnt->scsi_done(SCpnt);
	}

	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_freeChainBuffers - Function to free chain buffers associated
 *	with a SCSI IO request
 *	@hd: Pointer to the MPT_SCSI_HOST instance
 *	@req_idx: Index of the SCSI IO request frame.
 *
 *	Called if SG chain buffer allocation fails and mptscsih callbacks.
 *	No return.
 */
static void
mptscsih_freeChainBuffers(MPT_SCSI_HOST *hd, int req_idx)
{
	MPT_FRAME_HDR *chain;
	unsigned long flags;
	int chain_idx;
	int next;

	/* Get the first chain index and reset
	 * tracker state.
	 */
	chain_idx = hd->ReqToChain[req_idx];
	hd->ReqToChain[req_idx] = MPT_HOST_NO_CHAIN;

	while (chain_idx != MPT_HOST_NO_CHAIN) {

		/* Save the next chain buffer index */
		next = hd->ChainToChain[chain_idx];

		/* Free this chain buffer and reset
		 * tracker
		 */
		hd->ChainToChain[chain_idx] = MPT_HOST_NO_CHAIN;

		chain = (MPT_FRAME_HDR *) (hd->ChainBuffer
					+ (chain_idx * hd->ioc->req_sz));
		spin_lock_irqsave(&hd->ioc->FreeQlock, flags);
		Q_ADD_TAIL(&hd->FreeChainQ.head,
					&chain->u.frame.linkage, MPT_FRAME_HDR);
		spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);

		dmfprintk((MYIOC_s_INFO_FMT "FreeChainBuffers (index %d)\n",
				hd->ioc->name, chain_idx));

		/* handle next */
		chain_idx = next;
	}
	return;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	Reset Handling
 */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_TMHandler - Generic handler for SCSI Task Management.
 *	Fall through to mpt_HardResetHandler if: not operational, too many
 *	failed TM requests or handshake failure.
 *
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@type: Task Management type
 *	@target: Logical Target ID for reset (if appropriate)
 *	@lun: Logical Unit for reset (if appropriate)
 *	@ctx2abort: Context for the task to be aborted (if appropriate)
 *	@sleepFlag: If set, use udelay instead of schedule in handshake code.
 *
 *	Remark: Currently invoked from a non-interrupt thread (_bh).
 *
 *	Remark: With old EH code, at most 1 SCSI TaskMgmt function per IOC
 *	will be active.
 *
 *	Returns 0 for SUCCESS or -1 if FAILED.
 */
static int
mptscsih_TMHandler(MPT_SCSI_HOST *hd, u8 type, u8 channel, u8 target, u8 lun, int ctx2abort, int sleepFlag)
{
	MPT_ADAPTER	*ioc;
	int		 rc = -1;
	int		 doTask = 1;
	u32		 ioc_raw_state;
	unsigned long	 flags;

	/* If FW is being reloaded currently, return success to
	 * the calling function.
	 */
	if (hd == NULL)
		return 0;

	ioc = hd->ioc;
	if (ioc == NULL) {
		printk(KERN_ERR MYNAM " TMHandler" " NULL ioc!\n");
		return FAILED;
	}
	dtmprintk((MYIOC_s_INFO_FMT "TMHandler Entered!\n", ioc->name));

	// SJR - CHECKME - Can we avoid this here?
	// (mpt_HardResetHandler has this check...)
	spin_lock_irqsave(&ioc->diagLock, flags);
	if ((ioc->diagPending) || (ioc->alt_ioc && ioc->alt_ioc->diagPending)) {
		spin_unlock_irqrestore(&ioc->diagLock, flags);
		return FAILED;
	}
	spin_unlock_irqrestore(&ioc->diagLock, flags);

	/* Do not do a Task Management if there are
	 * too many failed TMs on this adapter.
	 */
	if (hd->numTMrequests > MPT_HOST_TOO_MANY_TM)
		doTask = 0;

#ifdef MPT_SCSI_USE_NEW_EH
	/*  Wait a fixed amount of time for the TM pending flag to be cleared.
	 *  If we time out and not bus reset, then we return a FAILED status to the caller.
	 *  The call to mptscsih_tm_pending_wait() will set the pending flag if we are
	 *  successful. Otherwise, reload the FW.
	 */
	if (mptscsih_tm_pending_wait(hd) == FAILED) {
		if (type == MPI_SCSITASKMGMT_TASKTYPE_ABORT_TASK) {
			nehprintk((KERN_WARNING MYNAM ": %s: TMHandler abort: "
			   "Timed out waiting for last TM (%d) to complete! \n",
			   hd->ioc->name, hd->tmPending));
			return FAILED;
		} else if (type == MPI_SCSITASKMGMT_TASKTYPE_TARGET_RESET) {
			nehprintk((KERN_WARNING MYNAM ": %s: TMHandler target reset: "
			   "Timed out waiting for last TM (%d) to complete! \n",
			   hd->ioc->name, hd->tmPending));
			return FAILED;
		} else if (type == MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS) {
			nehprintk((KERN_WARNING MYNAM ": %s: TMHandler bus reset: "
			   "Timed out waiting for last TM (%d) to complete! \n",
			   hd->ioc->name, hd->tmPending));
			if (hd->tmPending & (1 << MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS))
				return FAILED;
				
			doTask = 0;
		}
	} else {
		spin_lock_irqsave(&hd->ioc->FreeQlock, flags);
		hd->tmPending |=  (1 << type);
		spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);
	}
#endif

	/* Is operational?
	 */
	ioc_raw_state = mpt_GetIocState(hd->ioc, 0);

#ifdef MPT_DEBUG_RESET
	if ((ioc_raw_state & MPI_IOC_STATE_MASK) != MPI_IOC_STATE_OPERATIONAL) {
		printk(MYIOC_s_WARN_FMT
			"TM Handler: IOC Not operational(0x%x)!\n",
			hd->ioc->name, ioc_raw_state);
	}
#endif

	if (doTask && ((ioc_raw_state & MPI_IOC_STATE_MASK) == MPI_IOC_STATE_OPERATIONAL)
				&& !(ioc_raw_state & MPI_DOORBELL_ACTIVE)) {

		/* Isse the Task Mgmt request.
		 */
		if (hd->hard_resets < -1)
			hd->hard_resets++;
		rc = mptscsih_IssueTaskMgmt(hd, type, channel, target, lun, ctx2abort, sleepFlag);
		if (rc) {
			printk(MYIOC_s_INFO_FMT "Issue of TaskMgmt failed!\n", hd->ioc->name);
		} else {
			dtmprintk((MYIOC_s_INFO_FMT "Issue of TaskMgmt Successful!\n", hd->ioc->name));
		}
	}
#ifdef MPTSCSIH_DBG_TIMEOUT
	if (hd->ioc->timeout_hard)
		rc = 1;
#endif

	/* Only fall through to the HRH if this is a bus reset
	 */
	if ((type == MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS) && (rc ||
		ioc->reload_fw || (ioc->alt_ioc && ioc->alt_ioc->reload_fw))) {
		dtmprintk((MYIOC_s_INFO_FMT "Calling HardReset! \n",
			 hd->ioc->name));
		rc = mpt_HardResetHandler(hd->ioc, sleepFlag);
	}

	dtmprintk((MYIOC_s_INFO_FMT "TMHandler rc = %d!\n", hd->ioc->name, rc));
#ifndef MPT_SCSI_USE_NEW_EH
	dtmprintk((MYIOC_s_INFO_FMT "TMHandler: _bh_handler state (%d) taskQ count (%d)\n",
		ioc->name, mytaskQ_bh_active, hd->taskQcnt));
#endif

	return rc;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_IssueTaskMgmt - Generic send Task Management function.
 *	@hd: Pointer to MPT_SCSI_HOST structure
 *	@type: Task Management type
 *	@target: Logical Target ID for reset (if appropriate)
 *	@lun: Logical Unit for reset (if appropriate)
 *	@ctx2abort: Context for the task to be aborted (if appropriate)
 *	@sleepFlag: If set, use udelay instead of schedule in handshake code.
 *
 *	Remark: _HardResetHandler can be invoked from an interrupt thread (timer)
 *	or a non-interrupt thread.  In the former, must not call schedule().
 *
 *	Not all fields are meaningfull for all task types.
 *
 *	Returns 0 for SUCCESS, -999 for "no msg frames",
 *	else other non-zero value returned.
 */
static int
mptscsih_IssueTaskMgmt(MPT_SCSI_HOST *hd, u8 type, u8 channel, u8 target, u8 lun, int ctx2abort, int sleepFlag)
{
	MPT_FRAME_HDR	*mf;
	SCSITaskMgmt_t	*pScsiTm;
	int		 ii;
	int		 retval;

	/* Return Fail to calling function if no message frames available.
	 */
	if ((mf = mpt_get_msg_frame(ScsiTaskCtx, hd->ioc->id)) == NULL) {
		dtmprintk((MYIOC_s_WARN_FMT "IssueTaskMgmt, no msg frames!!\n",
				hd->ioc->name));
		//return FAILED;
		return -999;
	}
	dtmprintk((MYIOC_s_INFO_FMT "IssueTaskMgmt request @ %p\n",
			hd->ioc->name, mf));

	/* Format the Request
	 */
	pScsiTm = (SCSITaskMgmt_t *) mf;
	pScsiTm->TargetID = target;
	pScsiTm->Bus = channel;
	pScsiTm->ChainOffset = 0;
	pScsiTm->Function = MPI_FUNCTION_SCSI_TASK_MGMT;

	pScsiTm->Reserved = 0;
	pScsiTm->TaskType = type;
	pScsiTm->Reserved1 = 0;
	pScsiTm->MsgFlags = (type == MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS)
	                    ? MPI_SCSITASKMGMT_MSGFLAGS_LIPRESET_RESET_OPTION : 0;

	for (ii= 0; ii < 8; ii++) {
		pScsiTm->LUN[ii] = 0;
	}
	pScsiTm->LUN[1] = lun;

	for (ii=0; ii < 7; ii++)
		pScsiTm->Reserved2[ii] = 0;

	pScsiTm->TaskMsgContext = ctx2abort;
	dtmprintk((MYIOC_s_INFO_FMT "IssueTaskMgmt, ctx2abort (0x%08x), type (%d)\n",
			hd->ioc->name, ctx2abort, type));

	/* MPI v0.10 requires SCSITaskMgmt requests be sent via Doorbell/handshake
		mpt_put_msg_frame(hd->ioc->id, mf);
	* Save the MF pointer in case the request times out.
	*/
	hd->tmPtr = mf;
	hd->numTMrequests++;
	hd->TMtimer.expires = jiffies + HZ*20;  /* 20 seconds */
	add_timer(&hd->TMtimer);

	if ((retval = mpt_send_handshake_request(ScsiTaskCtx, hd->ioc->id,
				sizeof(SCSITaskMgmt_t), (u32*)pScsiTm, sleepFlag))
	!= 0) {
		dtmprintk((MYIOC_s_WARN_FMT "_send_handshake FAILED!"
			" (hd %p, ioc %p, mf %p) \n", hd->ioc->name, hd, hd->ioc, mf));
		hd->numTMrequests--;
		hd->tmPtr = NULL;
		del_timer(&hd->TMtimer);
		mpt_free_msg_frame(ScsiTaskCtx, hd->ioc->id, mf);
	}

	return retval;
}

#ifdef MPT_SCSI_USE_NEW_EH		/* { */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_abort - Abort linux Scsi_Cmnd routine, new_eh variant
 *	@SCpnt: Pointer to Scsi_Cmnd structure, IO to be aborted
 *
 *	(linux Scsi_Host_Template.eh_abort_handler routine)
 *
 *	Returns SUCCESS or FAILED.
 */
int
mptscsih_abort(Scsi_Cmnd * SCpnt)
{
	MPT_SCSI_HOST	*hd;
	MPT_FRAME_HDR	*mf;
	u32		 ctx2abort;
	int		 scpnt_idx;

	/* If we can't locate our host adapter structure, return FAILED status.
	 */
	if ((hd = (MPT_SCSI_HOST *) SCpnt->host->hostdata) == NULL) {
		SCpnt->result = DID_RESET << 16;
		SCpnt->scsi_done(SCpnt);
		nehprintk((KERN_WARNING MYNAM ": mptscsih_abort: "
			   "Can't locate host! (sc=%p)\n",
			   SCpnt));
		return FAILED;
	}

	if (hd->resetPending)
		return FAILED;

	printk(KERN_WARNING MYNAM ": %s: >> Attempting task abort! (sc=%p, numIOs=%d)\n",
	       hd->ioc->name, SCpnt, atomic_read(&queue_depth));

	if (hd->timeouts < -1)
		hd->timeouts++;

	/* Find this command
	 */
	if ((scpnt_idx = SCPNT_TO_LOOKUP_IDX(SCpnt)) < 0) {
		/* Cmd not found in ScsiLookup. If found in
		 * doneQ, delete from Q. Do OS callback.
		 */
		search_doneQ_for_cmd(hd, SCpnt);

		SCpnt->result = DID_RESET << 16;
		nehprintk((KERN_WARNING MYNAM ": %s: mptscsih_abort: "
			   "Command not in the active list! (sc=%p)\n",
			   hd->ioc->name, SCpnt));
		return SUCCESS;
	}

	/* If this command is pended, then timeout/hang occurred
	 * during DV. Post command and flush pending Q
	 * and then following up with the reset request.
	 */
	if ((mf = mptscsih_search_pendingQ(hd, scpnt_idx)) != NULL) {
		mptscsih_put_msgframe(ScsiDoneCtx, hd->ioc->id, mf);
		post_pendingQ_commands(hd);
		nehprintk((KERN_WARNING MYNAM ": %s: mptscsih_abort: "
			   "Posting pended cmd! (sc=%p)\n",
			   hd->ioc->name, SCpnt));
	}

	/* Most important!  Set TaskMsgContext to SCpnt's MsgContext!
	 * (the IO to be ABORT'd)
	 *
	 * NOTE: Since we do not byteswap MsgContext, we do not
	 *	 swap it here either.  It is an opaque cookie to
	 *	 the controller, so it does not matter. -DaveM
	 */
	mf = MPT_INDEX_2_MFPTR(hd->ioc, scpnt_idx);
	ctx2abort = mf->u.frame.hwhdr.msgctxu.MsgContext;

	hd->abortSCpnt = SCpnt;
	if (mptscsih_TMHandler(hd, MPI_SCSITASKMGMT_TASKTYPE_ABORT_TASK,
	                       SCpnt->channel, SCpnt->target, SCpnt->lun, ctx2abort, NO_SLEEP)
		< 0) {

		/* The TM request failed and the subsequent FW-reload failed!
		 * Fatal error case.
		 */
		printk(MYIOC_s_WARN_FMT "Error issuing abort task! (sc=%p)\n",
		       hd->ioc->name, SCpnt);

		/* We must clear our pending flag before clearing our state.
		 */
		hd->tmPending = 0;
		hd->tmState = TM_STATE_NONE;

		return FAILED;
	}
	return FAILED;

}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_dev_reset - Perform a SCSI TARGET_RESET!  new_eh variant
 *	@SCpnt: Pointer to Scsi_Cmnd structure, IO which reset is due to
 *
 *	(linux Scsi_Host_Template.eh_dev_reset_handler routine)
 *
 *	Returns SUCCESS or FAILED.
 */
int
mptscsih_dev_reset(Scsi_Cmnd * SCpnt)
{
	MPT_SCSI_HOST	*hd;

	/* If we can't locate our host adapter structure, return FAILED status.
	 */
	if ((hd = (MPT_SCSI_HOST *) SCpnt->host->hostdata) == NULL){
		nehprintk((KERN_WARNING MYNAM ": mptscsih_dev_reset: "
			   "Can't locate host! (sc=%p)\n",
			   SCpnt));
		return FAILED;
	}

	if (hd->resetPending)
		return FAILED;

	printk(KERN_WARNING MYNAM ": %s: >> Attempting target reset! (sc=%p, numIOs=%d)\n",
	       hd->ioc->name, SCpnt, atomic_read(&queue_depth));

	/* Unsupported for SCSI. Supported for FCP
	 */
	if (hd->is_spi)
		return FAILED;

	if (mptscsih_TMHandler(hd, MPI_SCSITASKMGMT_TASKTYPE_TARGET_RESET,
	                       SCpnt->channel, SCpnt->target, 0, 0, NO_SLEEP)
		< 0){
		/* The TM request failed and the subsequent FW-reload failed!
		 * Fatal error case.
		 */
		printk(MYIOC_s_WARN_FMT "Error processing TaskMgmt request (sc=%p)\n",
		 		hd->ioc->name, SCpnt);
		hd->tmPending = 0;
		hd->tmState = TM_STATE_NONE;
		return FAILED;
	}
	return SUCCESS;

}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_bus_reset - Perform a SCSI BUS_RESET!	new_eh variant
 *	@SCpnt: Pointer to Scsi_Cmnd structure, IO which reset is due to
 *
 *	(linux Scsi_Host_Template.eh_bus_reset_handler routine)
 *
 *	Returns SUCCESS or FAILED.
 */
int
mptscsih_bus_reset(Scsi_Cmnd * SCpnt)
{
	MPT_SCSI_HOST	*hd;

	/* If we can't locate our host adapter structure, return FAILED status.
	 */
	if ((hd = (MPT_SCSI_HOST *) SCpnt->host->hostdata) == NULL){
		nehprintk((KERN_WARNING MYNAM ": mptscsih_bus_reset: "
			   "Can't locate host! (sc=%p)\n",
			   SCpnt ) );
		return FAILED;
	}

	printk(KERN_WARNING MYNAM ": %s: >> Attempting bus reset! (sc=%p, numIOs=%d)\n",
	       hd->ioc->name, SCpnt, atomic_read(&queue_depth));

	if (hd->timeouts < -1)
		hd->timeouts++;

	/* We are now ready to execute the task management request. */
	if (mptscsih_TMHandler(hd, MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS,
	                       SCpnt->channel, 0, 0, 0, NO_SLEEP)
	    < 0){

		/* The TM request failed and the subsequent FW-reload failed!
		 * Fatal error case.
		 */
		printk(MYIOC_s_WARN_FMT
		       "Error processing TaskMgmt request (sc=%p)\n",
		       hd->ioc->name, SCpnt);
		hd->tmPending = 0;
		hd->tmState = TM_STATE_NONE;
		return FAILED;
	}

	return SUCCESS;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_host_reset - Perform a SCSI host adapter RESET!
 *	new_eh variant
 *	@SCpnt: Pointer to Scsi_Cmnd structure, IO which reset is due to
 *
 *	(linux Scsi_Host_Template.eh_host_reset_handler routine)
 *
 *	Returns SUCCESS or FAILED.
 */
int
mptscsih_host_reset(Scsi_Cmnd *SCpnt)
{
	MPT_SCSI_HOST *  hd;
	int              status = SUCCESS;

	/*  If we can't locate the host to reset, then we failed. */
	if ((hd = (MPT_SCSI_HOST *) SCpnt->host->hostdata) == NULL){
		nehprintk( ( KERN_WARNING MYNAM ": mptscsih_host_reset: "
			     "Can't locate host! (sc=%p)\n",
			     SCpnt ) );
		return FAILED;
	}

	printk(KERN_WARNING MYNAM ": %s: >> Attempting host reset! (sc=%p)\n",
	       hd->ioc->name, SCpnt);
	printk(KERN_WARNING MYNAM ": %s: IOs outstanding = %d\n",
	       hd->ioc->name, atomic_read(&queue_depth));

	/*  If our attempts to reset the host failed, then return a failed
	 *  status.  The host will be taken off line by the SCSI mid-layer.
	 */
	if (mpt_HardResetHandler(hd->ioc, NO_SLEEP) < 0){
		status = FAILED;
	} else {
		/*  Make sure TM pending is cleared and TM state is set to
		 *  NONE.
		 */
		hd->tmPending = 0;
		hd->tmState = TM_STATE_NONE;
	}


	nehprintk( ( KERN_WARNING MYNAM ": mptscsih_host_reset: "
		     "Status = %s\n",
		     (status == SUCCESS) ? "SUCCESS" : "FAILED" ) );

	return status;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_tm_pending_wait - wait for pending task management request to
 *		complete.
 *	@hd: Pointer to MPT host structure.
 *
 *	Returns {SUCCESS,FAILED}.
 */
static int
mptscsih_tm_pending_wait(MPT_SCSI_HOST * hd)
{
	unsigned long  flags;
	int            loop_count = 2 * 10 * 4;  /* Wait 2 seconds */
	int            status = FAILED;

	do {
		spin_lock_irqsave(&hd->ioc->FreeQlock, flags);
		if (hd->tmState == TM_STATE_NONE) {
			hd->tmState = TM_STATE_IN_PROGRESS;
			hd->tmPending = 1;
			spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);
			status = SUCCESS;
			break;
		}
		spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);
		//set_current_state(TASK_INTERRUPTIBLE);
		//schedule_timeout(HZ/4);
		mdelay(250);
	} while (--loop_count);

	return status;
}

#else		/* MPT_SCSI old EH stuff... */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_old_abort - Abort linux Scsi_Cmnd routine
 *	@SCpnt: Pointer to Scsi_Cmnd structure, IO to be aborted
 *
 *	(linux Scsi_Host_Template.abort routine)
 *
 *	Returns SCSI_ABORT_{SUCCESS,BUSY,PENDING}.
 */
int
mptscsih_old_abort(Scsi_Cmnd *SCpnt)
{
	MPT_SCSI_HOST		*hd;
	MPT_FRAME_HDR		*mf;
	struct mpt_work_struct	*ptaskfoo;
	unsigned long		 flags;
	int			 scpnt_idx;

	printk(KERN_WARNING MYNAM ": OldAbort scheduling ABORT SCSI IO (sc=%p)\n", (void *) SCpnt);
	printk(KERN_WARNING "  IOs outstanding = %d\n", atomic_read(&queue_depth));

	if ((hd = (MPT_SCSI_HOST *) SCpnt->host->hostdata) == NULL) {
		printk(KERN_WARNING "  WARNING - OldAbort, NULL hostdata ptr!!\n");
		SCpnt->result = DID_ERROR << 16;
		return SCSI_ABORT_NOT_RUNNING;
	}

	if (hd->timeouts < -1)
		hd->timeouts++;

	if ((scpnt_idx = SCPNT_TO_LOOKUP_IDX(SCpnt)) < 0) {
		/* Cmd not found in ScsiLookup.
		 * If found in doneQ, delete from Q.
		 * Do OS callback.
		 */
		search_doneQ_for_cmd(hd, SCpnt);

		SCpnt->result = DID_ABORT << 16;
		return SCSI_ABORT_SUCCESS;
	} else {
		/* If this command is pended, then timeout/hang occurred
		 * during DV. Force bus reset by posting command to F/W
		 * and then following up with the reset request.
		 */
#ifndef MPTSCSIH_DBG_TIMEOUT
		if ((mf = mptscsih_search_pendingQ(hd, scpnt_idx)) != NULL) {
			mptscsih_put_msgframe(ScsiDoneCtx, hd->ioc->id, mf);
			post_pendingQ_commands(hd);
		}
#endif
	}

	/*
	 *  Check to see if there's already an ABORT queued for this guy.
	 */
	mf = search_taskQ(0, SCpnt, hd, MPI_SCSITASKMGMT_TASKTYPE_ABORT_TASK);
	if (mf != NULL) {
		dtmprintk((MYIOC_s_INFO_FMT "OldAbort:Abort Task PENDING cmd (%p) taskQ depth (%d)\n",
			hd->ioc->name, SCpnt, hd->taskQcnt));
		return SCSI_ABORT_PENDING;
	}

	// SJR - CHECKME - Can we avoid this here?
	// (mpt_HardResetHandler has this check...)
	/* If IOC is reloading FW, return PENDING.
	 */
	spin_lock_irqsave(&hd->ioc->diagLock, flags);
	if (hd->ioc->diagPending) {
		spin_unlock_irqrestore(&hd->ioc->diagLock, flags);
		return SCSI_ABORT_PENDING;
	}
	spin_unlock_irqrestore(&hd->ioc->diagLock, flags);

	/* If there are no message frames what should we do?
	 */
	if ((mf = mpt_get_msg_frame(ScsiTaskCtx, hd->ioc->id)) == NULL) {
		printk((KERN_WARNING "  WARNING - OldAbort, no msg frames!!\n"));
		/* We are out of message frames!
		 * Call the reset handler to do a FW reload.
		 */
		printk((KERN_WARNING " Reloading Firmware!!\n"));
		if (mpt_HardResetHandler(hd->ioc, NO_SLEEP) < 0) {
			printk((KERN_WARNING " Firmware Reload FAILED!!\n"));
		}
		return SCSI_ABORT_PENDING;
	}

	/*
	 *  Add ourselves to (end of) taskQ .
	 *  Check to see if our _bh is running.  If NOT, schedule it.
	 */
	spin_lock_irqsave(&hd->ioc->FreeQlock, flags);
	Q_ADD_TAIL(&hd->taskQ, &mf->u.frame.linkage, MPT_FRAME_HDR);
	hd->taskQcnt++;
	atomic_inc(&mpt_taskQdepth);
	spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);

	spin_lock_irqsave(&mytaskQ_lock, flags);

	/* Save the original SCpnt mf pointer
	 */
	SCpnt->host_scribble = (u8 *) MPT_INDEX_2_MFPTR (hd->ioc, scpnt_idx);

	/* For the time being, force bus reset on any abort
	 * requests for the 1030/1035 FW.
	 */
	if (hd->is_spi)
		mf->u.frame.linkage.arg1 = MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS;
	else
		mf->u.frame.linkage.arg1 = MPI_SCSITASKMGMT_TASKTYPE_ABORT_TASK;

	mf->u.frame.linkage.argp1 = SCpnt;
	mf->u.frame.linkage.argp2 = (void *) hd;

	dtmprintk((MYIOC_s_INFO_FMT "OldAbort:_bh_handler state (%d) taskQ count (%d)\n",
		hd->ioc->name, mytaskQ_bh_active, hd->taskQcnt));

	if (! mytaskQ_bh_active) {
		mytaskQ_bh_active = 1;
		spin_unlock_irqrestore(&mytaskQ_lock, flags);

		ptaskfoo = (struct mpt_work_struct *) &mptscsih_ptaskfoo;
		MPT_INIT_WORK(&mptscsih_ptaskfoo, mptscsih_taskmgmt_bh, (void *) SCpnt);

		SCHEDULE_TASK(ptaskfoo);
	} else  {
		spin_unlock_irqrestore(&mytaskQ_lock, flags);
	}

	return SCSI_ABORT_PENDING;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_old_reset - Perform a SCSI BUS_RESET!
 *	@SCpnt: Pointer to Scsi_Cmnd structure, IO which reset is due to
 *	@reset_flags: (not used?)
 *
 *	(linux Scsi_Host_Template.reset routine)
 *
 *	Returns SCSI_RESET_{SUCCESS,PUNT,PENDING}.
 */
int
mptscsih_old_reset(Scsi_Cmnd *SCpnt, unsigned int reset_flags)
{
	MPT_SCSI_HOST		*hd;
	MPT_FRAME_HDR		*mf;
	struct mpt_work_struct	*ptaskfoo;
	unsigned long		 flags;
	int			 scpnt_idx;

	printk(KERN_WARNING MYNAM ": OldReset scheduling BUS_RESET (sc=%p)\n", (void *) SCpnt);
	printk(KERN_WARNING "  IOs outstanding = %d\n", atomic_read(&queue_depth));

	if ((hd = (MPT_SCSI_HOST *) SCpnt->host->hostdata) == NULL) {
		SCpnt->result = DID_ERROR << 16;
		return SCSI_RESET_SUCCESS;
	}

	if (hd->timeouts < -1)
		hd->timeouts++;

	if ((scpnt_idx = SCPNT_TO_LOOKUP_IDX(SCpnt)) < 0) {
		/* Cmd not found in ScsiLookup.
		 * If found in doneQ, delete from Q.
		 * Do OS callback.
		 */
		search_doneQ_for_cmd(hd, SCpnt);

		SCpnt->result = DID_RESET << 16;
		return SCSI_RESET_SUCCESS;
	} else {
		/* If this command is pended, then timeout/hang occurred
		 * during DV. Force bus reset by posting command to F/W
		 * and then following up with the reset request.
		 */
#ifndef MPTSCSIH_DBG_TIMEOUT
		if ((mf = mptscsih_search_pendingQ(hd, scpnt_idx)) != NULL) {
			mptscsih_put_msgframe(ScsiDoneCtx, hd->ioc->id, mf);
			post_pendingQ_commands(hd);
		}
#endif
	}

	/*
	 *  Check to see if there's an ABORT_TASK queued for this guy.
	 *  If so, delete.
	 */
	search_taskQ(1, SCpnt, hd, MPI_SCSITASKMGMT_TASKTYPE_ABORT_TASK);

	/*
	 *  Check to see if there's already a BUS_RESET queued for this guy.
	 */
	mf = search_taskQ(0, SCpnt, hd, MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS);
	if (mf != NULL) {
		dtmprintk((MYIOC_s_INFO_FMT "OldReset:Reset Task PENDING cmd (%p) taskQ depth (%d)\n",
			hd->ioc->name, SCpnt, hd->taskQcnt));
		return SCSI_RESET_PENDING;
	}

	// SJR - CHECKME - Can we avoid this here?
	// (mpt_HardResetHandler has this check...)
	/* If IOC is reloading FW, return PENDING.
	 */
	spin_lock_irqsave(&hd->ioc->diagLock, flags);
	if (hd->ioc->diagPending) {
		spin_unlock_irqrestore(&hd->ioc->diagLock, flags);
		return SCSI_RESET_PENDING;
	}
	spin_unlock_irqrestore(&hd->ioc->diagLock, flags);

	if ((mf = mpt_get_msg_frame(ScsiTaskCtx, hd->ioc->id)) == NULL) {
		/* We are out of message frames!
		 * Call the reset handler to do a FW reload.
		 */
		printk((KERN_WARNING " Reloading Firmware!!\n"));
		if (mpt_HardResetHandler(hd->ioc, NO_SLEEP) < 0) {
			printk((KERN_WARNING " Firmware Reload FAILED!!\n"));
		}
		return SCSI_RESET_PENDING;
	}

	/*
	 *  Add ourselves to (end of) taskQ.
	 *  Check to see if our _bh is running.  If NOT, schedule it.
	 */
	spin_lock_irqsave(&hd->ioc->FreeQlock, flags);
	Q_ADD_TAIL(&hd->taskQ, &mf->u.frame.linkage, MPT_FRAME_HDR);
	hd->taskQcnt++;
	atomic_inc(&mpt_taskQdepth);
	spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);


	dtmprintk((MYIOC_s_INFO_FMT "OldReset: _bh_handler state (%d) taskQ count (%d)\n",
		hd->ioc->name, mytaskQ_bh_active, hd->taskQcnt));

	spin_lock_irqsave(&mytaskQ_lock, flags);
	/* Save the original SCpnt mf pointer
	 */
	SCpnt->host_scribble = (u8 *) MPT_INDEX_2_MFPTR (hd->ioc, scpnt_idx);

	mf->u.frame.linkage.arg1 = MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS;
	mf->u.frame.linkage.argp1 = SCpnt;
	mf->u.frame.linkage.argp2 = (void *) hd;

	if (! mytaskQ_bh_active) {
		mytaskQ_bh_active = 1;
		spin_unlock_irqrestore(&mytaskQ_lock, flags);
		/*
		 *  Oh how cute, no alloc/free/mgmt needed if we use
		 *  (bottom/unused portion of) MPT request frame.
		 */
		ptaskfoo = (struct mpt_work_struct *) &mptscsih_ptaskfoo;
		MPT_INIT_WORK(&mptscsih_ptaskfoo, mptscsih_taskmgmt_bh, (void *) SCpnt);

		SCHEDULE_TASK(ptaskfoo);
	} else  {
		spin_unlock_irqrestore(&mytaskQ_lock, flags);
	}
	return SCSI_RESET_PENDING;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_taskmgmt_bh - SCSI task mgmt bottom half handler
 *	@sc: (unused)
 *
 *	This routine (thread) is active whenever there are any outstanding
 *	SCSI task management requests for a SCSI host adapter.
 *	IMPORTANT!  This routine is scheduled therefore should never be
 *	running in ISR context.  i.e., it's safe to sleep here.
 */
void
mptscsih_taskmgmt_bh(void *sc)
{
	MPT_ADAPTER	*ioc;
	Scsi_Cmnd	*SCpnt;
	MPT_FRAME_HDR	*mf = NULL;
	MPT_SCSI_HOST	*hd;
	u32		 ctx2abort = 0;
	unsigned long	 flags;
	int		 scpnt_idx;
	int		 did;
	u8		 task_type;

	spin_lock_irqsave(&mytaskQ_lock, flags);
	mytaskQ_bh_active = 1;
	spin_unlock_irqrestore(&mytaskQ_lock, flags);

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ/4);
		did = 0;

		for (ioc = mpt_adapter_find_first(); ioc != NULL; ioc = mpt_adapter_find_next(ioc)) {
			if (ioc->sh) {
				hd = (MPT_SCSI_HOST *) ioc->sh->hostdata;
				if (hd == NULL) {
					printk(KERN_ERR MYNAM
							": ERROR - TaskMgmt NULL SCSI Host!"
							"(ioc=%p, sh=%p hd=%p)\n",
							(void *) ioc, (void *) ioc->sh, (void *) hd);
					continue;
				}

#ifdef MPTSCSIH_DBG_TIMEOUT
				if (ioc->timeout_hard == 1) {
					mptscsih_TMHandler(hd,
						MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS,
						0, 0, 0, 0, CAN_SLEEP);

				}
#endif

				spin_lock_irqsave(&ioc->FreeQlock, flags);
				if (Q_IS_EMPTY(&hd->taskQ)) {
					spin_unlock_irqrestore(&ioc->FreeQlock, flags);
					continue;
				}

				/* If we ever find a non-empty queue,
				 * keep the handler alive
				 */
				did++;

				/* tmPending is SMP lock-protected */
				if (hd->tmPending || hd->tmPtr) {
					spin_unlock_irqrestore(&ioc->FreeQlock, flags);
					continue;
				}
				hd->tmPending = 1;

				/* Process this request
				 */
                                mf = hd->taskQ.head;
				Q_DEL_ITEM(&mf->u.frame.linkage);
				hd->taskQcnt--;
				atomic_dec(&mpt_taskQdepth);
				spin_unlock_irqrestore(&ioc->FreeQlock, flags);

				SCpnt = (Scsi_Cmnd*)mf->u.frame.linkage.argp1;
				if (SCpnt == NULL) {
					printk(KERN_ERR MYNAM ": ERROR - TaskMgmt has NULL SCpnt! (mf=%p:sc=%p)\n",
							(void *) mf, (void *) SCpnt);
					mpt_free_msg_frame(ScsiTaskCtx, hd->ioc->id, mf);
					spin_lock_irqsave(&ioc->FreeQlock, flags);
					hd->tmPending = 0;
					spin_unlock_irqrestore(&ioc->FreeQlock, flags);
					continue;
				}

				/* Get the ScsiLookup index pointer
				 * from the SC pointer.
				 */
				if (!SCpnt->host_scribble || ((MPT_SCSI_HOST *)SCpnt->host->hostdata != hd)) {
					/* The command associated with the
					 * abort/reset request must have
					 * completed and this is a stale
					 * request. We are done.
					 * Free the current MF and continue.
					 */
					mpt_free_msg_frame(ScsiTaskCtx, hd->ioc->id, mf);
					spin_lock_irqsave(&ioc->FreeQlock, flags);
					hd->tmPending = 0;
					spin_unlock_irqrestore(&ioc->FreeQlock, flags);
					continue;
				}

				scpnt_idx = MFPTR_2_MPT_INDEX(hd->ioc, SCpnt->host_scribble);
				if (scpnt_idx != SCPNT_TO_LOOKUP_IDX(SCpnt)) {
					/* Error! this should never happen!!
					 */
					mpt_free_msg_frame(ScsiTaskCtx, hd->ioc->id, mf);
					spin_lock_irqsave(&ioc->FreeQlock, flags);
					hd->tmPending = 0;
					spin_unlock_irqrestore(&ioc->FreeQlock, flags);
					continue;
				}

				task_type = mf->u.frame.linkage.arg1;
				ctx2abort = 0;
				if (task_type == MPI_SCSITASKMGMT_TASKTYPE_ABORT_TASK) {
					MPT_FRAME_HDR	*SCpntMf;

					/*
					 * Most important!  Set TaskMsgContext to SCpnt's MsgContext!
					 * (the IO to be ABORT'd)
					 *
					 * NOTE: Since we do not byteswap MsgContext, we do not
					 *	 swap it here either.  It is an opaque cookie to
					 *	 the controller, so it does not matter. -DaveM
					 */
					SCpntMf = (MPT_FRAME_HDR *) SCpnt->host_scribble;
					ctx2abort = SCpntMf->u.frame.hwhdr.msgctxu.MsgContext;

					hd->abortSCpnt = SCpnt;
					printk(KERN_WARNING MYNAM ": Attempting ABORT SCSI IO! (mf=%p:sc=%p)\n",
							(void *) mf, (void *) SCpnt);
				}

				/* The TM handler will allocate a new mf,
				 * so free the current mf.
				 */
				mpt_free_msg_frame(ScsiTaskCtx, hd->ioc->id, mf);
				mf = NULL;

#ifndef MPTSCSIH_DBG_TIMEOUT
				post_pendingQ_commands(hd);
#endif
				if (mptscsih_TMHandler(hd, task_type, SCpnt->channel,
						      SCpnt->target, SCpnt->lun,
						       ctx2abort, CAN_SLEEP) < 0) {

					/* The TM request failed and the subsequent FW-reload failed!
					 * Fatal error case.
					 */
					printk(KERN_WARNING MYNAM
						": WARNING[1] - IOC error processing TaskMgmt request (sc=%p)\n", (void *) SCpnt);

					if (hd->ScsiLookup[scpnt_idx] != NULL) {
						atomic_dec(&queue_depth);
						SCpnt->result = DID_SOFT_ERROR << 16;
                                                MPT_HOST_LOCK(flags);
						SCpnt->scsi_done(SCpnt);
                                                MPT_HOST_UNLOCK(flags);
						mpt_free_msg_frame(ScsiTaskCtx, hd->ioc->id, mf);
					}
					spin_lock_irqsave(&ioc->FreeQlock, flags);
					hd->tmPending = 0;
					spin_unlock_irqrestore(&ioc->FreeQlock, flags);
					hd->abortSCpnt = NULL;
				}
			}
		}
		if (atomic_read(&mpt_taskQdepth) > 0)
			did++;

	} while ( did );

	spin_lock_irqsave(&mytaskQ_lock, flags);
	mytaskQ_bh_active = 0;
	spin_unlock_irqrestore(&mytaskQ_lock, flags);

	return;
}
#endif		/* } */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_taskmgmt_complete - Registered with Fusion MPT base driver
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@mf: Pointer to SCSI task mgmt request frame
 *	@mr: Pointer to SCSI task mgmt reply frame
 *
 *	This routine is called from mptbase.c::mpt_interrupt() at the completion
 *	of any SCSI task management request.
 *	This routine is registered with the MPT (base) driver at driver
 *	load/init time via the mpt_register() API call.
 *
 *	Returns 1 indicating alloc'd request frame ptr should be freed.
 */
static int
mptscsih_taskmgmt_complete(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf, MPT_FRAME_HDR *mr)
{
	SCSITaskMgmtReply_t	*pScsiTmReply;
	SCSITaskMgmt_t		*pScsiTmReq;
	MPT_SCSI_HOST		*hd;
	unsigned long		 flags;
	u8			 tmType = 0;

	dtmprintk((MYIOC_s_INFO_FMT "SCSI TaskMgmt completed (mf=%p,r=%p)\n",
			ioc->name, mf, mr));
	if (ioc->sh) {
		/* Depending on the thread, a timer is activated for
		 * the TM request.  Delete this timer on completion of TM.
		 * Decrement count of outstanding TM requests.
		 */
		hd = (MPT_SCSI_HOST *)ioc->sh->hostdata;
		if (hd->tmPtr) {
			del_timer(&hd->TMtimer);
		}
		dtmprintk((MYIOC_s_INFO_FMT "taskQcnt (%d)\n",
			ioc->name, hd->taskQcnt));
	} else {
		dtmprintk((MYIOC_s_WARN_FMT "TaskMgmt Complete: NULL Scsi Host Ptr\n",
			ioc->name));
		return 1;
	}

	if (mr == NULL) {
		dtmprintk((MYIOC_s_WARN_FMT "ERROR! TaskMgmt Reply: NULL Request %p\n",
			ioc->name, mf));
		return 1;
	} else {
		pScsiTmReply = (SCSITaskMgmtReply_t*)mr;
		pScsiTmReq = (SCSITaskMgmt_t*)mf;

		/* Figure out if this was ABORT_TASK, TARGET_RESET, or BUS_RESET! */
		tmType = pScsiTmReq->TaskType;

		dtmprintk((KERN_INFO "  TaskType = %d, TerminationCount=%d\n",
				tmType, le32_to_cpu(pScsiTmReply->TerminationCount)));

		/* Error?  (anything non-zero?) */
		if (*(u32 *)&pScsiTmReply->Reserved2[0]) {
			u16	 iocstatus;

			iocstatus = le16_to_cpu(pScsiTmReply->IOCStatus) & MPI_IOCSTATUS_MASK;
			dtmprintk((KERN_INFO "  SCSI TaskMgmt (%d) - Oops!\n", tmType));
			dtmprintk((KERN_INFO "  IOCStatus = %04xh\n", iocstatus));
			dtmprintk((KERN_INFO "  IOCLogInfo = %08xh\n",
				 le32_to_cpu(pScsiTmReply->IOCLogInfo)));

			/* clear flags and continue.
			 */
			if (tmType == MPI_SCSITASKMGMT_TASKTYPE_ABORT_TASK)
				hd->abortSCpnt = NULL;

			/* If an internal command is present
			 * or the TM failed - reload the FW.
			 * FC FW may respond FAILED to an ABORT
			 */
			if (tmType == MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS) {
				if ((hd->cmdPtr) ||
				    (iocstatus == MPI_IOCSTATUS_SCSI_TASK_MGMT_FAILED)) {
					if (mpt_HardResetHandler(ioc, NO_SLEEP) < 0) {
						printk((KERN_WARNING
							" Firmware Reload FAILED!!\n"));
					}
				}
			}
		} else {
			dtmprintk((KERN_INFO "  SCSI TaskMgmt SUCCESS!\n"));

#ifndef MPT_SCSI_USE_NEW_EH
			if (tmType == MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS) {
				/* clean taskQ - remove tasks associated with
				 * completed commands.
				 */
				clean_taskQ(hd);
			} else if (tmType == MPI_SCSITASKMGMT_TASKTYPE_ABORT_TASK) {
				/* If taskQ contains another request
				 * for this SCpnt, delete this request.
				 */
				search_taskQ_for_cmd(hd->abortSCpnt, hd);
			}
#endif
			hd->numTMrequests--;
			hd->abortSCpnt = NULL;
			flush_doneQ(hd);

		}
	}

#ifndef MPT_SCSI_USE_NEW_EH
	/*
	 *  Signal to _bh thread that we finished.
	 *  This IOC can now process another TM command.
	 */
	dtmprintk((MYIOC_s_INFO_FMT "taskmgmt_complete: (=%p) done! Num Failed(%d) Task Count (%d)\n",
			ioc->name, mf, hd->numTMrequests, hd->taskQcnt));
#endif
	hd->tmPtr = NULL;
	spin_lock_irqsave(&ioc->FreeQlock, flags);
	hd->tmPending = 0;
	spin_unlock_irqrestore(&ioc->FreeQlock, flags);
#ifdef MPT_SCSI_USE_NEW_EH
	hd->tmState = TM_STATE_NONE;
#endif

	return 1;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	This is anyones guess quite frankly.
 */
int
mptscsih_bios_param(Disk * disk, kdev_t dev, int *ip)
{
	unsigned capacity = disk->capacity;
	int size;

	size = capacity;
	ip[0] = 64;				/* heads			*/
	ip[1] = 32;				/* sectors			*/
	if ((ip[2] = size >> 11) > 1024) {	/* cylinders, test for big disk */
		ip[0] = 255;			/* heads			*/
		ip[1] = 63;			/* sectors			*/
		ip[2] = size / (255 * 63);	/* cylinders			*/
	}
	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	OS entry point to adjust the queue_depths on a per-device basis.
 *	Called once per device the bus scan. Use it to force the queue_depth
 *	member to 1 if a device does not support Q tags.
 */
void
mptscsih_select_queue_depths(struct Scsi_Host *sh, Scsi_Device *sdList)
{
	struct scsi_device	*device;
	VirtDevice		*pTarget;
	MPT_SCSI_HOST		*hd;

	for (device = sdList; device != NULL; device = device->next) {

		if (device->host != sh)
			continue;

		hd = (MPT_SCSI_HOST *) sh->hostdata;
		if (hd == NULL)
			continue;

		if (hd->Targets != NULL) {
			pTarget = NULL;
			if (device->id > sh->max_id) {
				/* error case, should never happen */
				device->queue_depth = 1;
				continue;
			} else {
				pTarget = hd->Targets[device->id];
			}

			if (pTarget == NULL) {
				/* error case - don't know about this device */
				device->queue_depth = 1;
			} else if (pTarget->tflags & MPT_TARGET_FLAGS_VALID_INQUIRY) {
				if (!(pTarget->tflags & MPT_TARGET_FLAGS_Q_YES))
					device->queue_depth = 1;
				else if (((pTarget->inq_data[0] & 0x1f) == 0x00)
					 && (pTarget->minSyncFactor <= MPT_ULTRA160 || !hd->is_spi)){
					device->queue_depth = MPT_SCSI_CMD_PER_DEV_HIGH;
				} else
					device->queue_depth = MPT_SCSI_CMD_PER_DEV_LOW;

			} else {
				/* error case - No Inq. Data */
				device->queue_depth = 1;
			}

			if (pTarget != NULL) {
				dprintk((MYIOC_s_INFO_FMT
					 "scsi%d: Id=%d Lun=%d: Queue depth=%d\n",
					 hd->ioc->name, 
					 device->id, device->lun, device->queue_depth));

				dprintk((MYIOC_s_INFO_FMT
					 "Id = %d, sync factor = %x\n",
					 hd->ioc->name, pTarget->target_id,
					 pTarget->minSyncFactor));
			}
		}
	}
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Private routines...
 */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/* Utility function to copy sense data from the scsi_cmnd buffer
 * to the FC and SCSI target structures.
 *
 */
static void
copy_sense_data(Scsi_Cmnd *sc, MPT_SCSI_HOST *hd, MPT_FRAME_HDR *mf, SCSIIOReply_t *pScsiReply)
{
	VirtDevice	*target;
	SCSIIORequest_t	*pReq;
	u32		 sense_count = le32_to_cpu(pScsiReply->SenseCount);
	int		 index;
	char		 devFoo[96];
	IO_Info_t	 thisIo;

	/* Get target structure
	 */
	pReq = (SCSIIORequest_t *) mf;
	index = (int) pReq->TargetID;
	target = hd->Targets[index];
	if (hd->is_multipath && sc->device->hostdata)
		target = (VirtDevice *) sc->device->hostdata;

	if (sense_count) {
		u8 *sense_data;
		int req_index;

		/* Copy the sense received into the scsi command block. */
		req_index = le16_to_cpu(mf->u.frame.hwhdr.msgctxu.fld.req_idx);
		sense_data = ((u8 *)hd->ioc->sense_buf_pool + (req_index * MPT_SENSE_BUFFER_ALLOC));
		memcpy(sc->sense_buffer, sense_data, SNS_LEN(sc));

		/* Log SMART data (asc = 0x5D, non-IM case only) if required.
		 */
		if ((hd->ioc->events) && (hd->ioc->eventTypes & (1 << MPI_EVENT_SCSI_DEVICE_STATUS_CHANGE))) {
			if ((sense_data[12] == 0x5D) && (target->raidVolume == 0)) {
				int idx;
				MPT_ADAPTER *ioc = hd->ioc;

				idx = ioc->eventContext % ioc->eventLogSize;
				ioc->events[idx].event = MPI_EVENT_SCSI_DEVICE_STATUS_CHANGE;
				ioc->events[idx].eventContext = ioc->eventContext;

				ioc->events[idx].data[0] = (pReq->LUN[1] << 24) ||
					(MPI_EVENT_SCSI_DEV_STAT_RC_SMART_DATA << 16) ||
					(pReq->Bus << 8) || pReq->TargetID;

				ioc->events[idx].data[1] = (sense_data[13] << 8) || sense_data[12];

				ioc->eventContext++;
			}
		}

		/* Print an error report for the user.
		 */
		thisIo.cdbPtr = sc->cmnd;
		thisIo.sensePtr = sc->sense_buffer;
		thisIo.SCSIStatus = pScsiReply->SCSIStatus;
		thisIo.DoDisplay = 1;
		if (hd->is_multipath)
			sprintf(devFoo, "%d:%d:%d",
					hd->ioc->id,
					pReq->TargetID,
					pReq->LUN[1]);
		else
			sprintf(devFoo, "%d:%d:%d", hd->ioc->id, sc->target, sc->lun);
		thisIo.DevIDStr = devFoo;
/* fubar */
		thisIo.dataPtr = NULL;
		thisIo.inqPtr = NULL;
		if (sc->device) {
			thisIo.inqPtr = sc->device->vendor-8;	/* FIXME!!! */
		}
		(void) mpt_ScsiHost_ErrorReport(&thisIo);

	} else {
		dprintk((MYIOC_s_INFO_FMT "Hmmm... SenseData len=0! (?)\n",
				hd->ioc->name));
	}

	return;
}

static u32
SCPNT_TO_LOOKUP_IDX(Scsi_Cmnd *sc)
{
	MPT_SCSI_HOST *hd;
	int i;

	hd = (MPT_SCSI_HOST *) sc->host->hostdata;

	for (i = 0; i < hd->ioc->req_depth; i++) {
		if (hd->ScsiLookup[i] == sc) {
			return i;
		}
	}

	return -1;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

/* see mptscsih.h */

#ifdef MPT_SCSIHOST_NEED_ENTRY_EXIT_HOOKUPS
	static Scsi_Host_Template driver_template = MPT_SCSIHOST;
#	include "../../scsi/scsi_module.c"
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/* Search the pendingQ for a command with specific index.
 * If found, delete and return mf pointer
 * If not found, return NULL
 */
static MPT_FRAME_HDR *
mptscsih_search_pendingQ(MPT_SCSI_HOST *hd, int scpnt_idx)
{
	unsigned long	 flags;
	MPT_DONE_Q	*buffer;
	MPT_FRAME_HDR	*mf = NULL;
	MPT_FRAME_HDR	*cmdMfPtr;

	ddvtprintk((MYIOC_s_INFO_FMT ": search_pendingQ ...", hd->ioc->name));
	cmdMfPtr = MPT_INDEX_2_MFPTR(hd->ioc, scpnt_idx);
	spin_lock_irqsave(&hd->freedoneQlock, flags);
	if (!Q_IS_EMPTY(&hd->pendingQ)) {
		buffer = hd->pendingQ.head;
		do {
			mf = (MPT_FRAME_HDR *) buffer->argp;
			if (mf == cmdMfPtr) {
				Q_DEL_ITEM(buffer);

				/* clear the arg pointer
				 */
				buffer->argp = NULL;

				/* Add to the freeQ
				 */
				Q_ADD_TAIL(&hd->freeQ.head, buffer, MPT_DONE_Q);
				break;
			}
			mf = NULL;
		} while ((buffer = buffer->forw) != (MPT_DONE_Q *) &hd->pendingQ);
	}
	spin_unlock_irqrestore(&hd->freedoneQlock, flags);
	ddvtprintk((" ...return %p\n", mf));
	return mf;
}

/* Post all commands on the pendingQ to the FW.
 * Lock Q when deleting/adding members
 * Lock io_request_lock for OS callback.
 */
static void
post_pendingQ_commands(MPT_SCSI_HOST *hd)
{
	MPT_FRAME_HDR	*mf;
	MPT_DONE_Q	*buffer;
	unsigned long	 flags;

	/* Flush the pendingQ.
	 */
	ddvtprintk((MYIOC_s_INFO_FMT ": post_pendingQ_commands\n", hd->ioc->name));
	while (1) {
		spin_lock_irqsave(&hd->freedoneQlock, flags);
		if (Q_IS_EMPTY(&hd->pendingQ)) {
			spin_unlock_irqrestore(&hd->freedoneQlock, flags);
			break;
		}

		buffer = hd->pendingQ.head;
		/* Delete from Q
		 */
		Q_DEL_ITEM(buffer);

		mf = (MPT_FRAME_HDR *) buffer->argp;
		buffer->argp = NULL;

		/* Add to the freeQ
		 */
		Q_ADD_TAIL(&hd->freeQ.head, buffer, MPT_DONE_Q);
		spin_unlock_irqrestore(&hd->freedoneQlock, flags);

		if (!mf) {
			/* This should never happen */
			printk(MYIOC_s_WARN_FMT "post_pendingQ_commands: mf %p\n", hd->ioc->name, (void *) mf);
			continue;
		}

		mptscsih_put_msgframe(ScsiDoneCtx, hd->ioc->id, mf);

#if defined(MPT_DEBUG_DV) || defined(MPT_DEBUG_DV_TINY)
		{
			u16		 req_idx = le16_to_cpu(mf->u.frame.hwhdr.msgctxu.fld.req_idx);
			Scsi_Cmnd	*sc = hd->ScsiLookup[req_idx];
			printk(MYIOC_s_INFO_FMT "Issued SCSI cmd (sc=%p) idx=%d (mf=%p)\n",
					hd->ioc->name, sc, req_idx, mf);
		}
#endif
	}

	return;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
static int
mptscsih_ioc_reset(MPT_ADAPTER *ioc, int reset_phase)
{
	MPT_SCSI_HOST	*hd;
	unsigned long	 flags;

	dtmprintk((KERN_WARNING MYNAM
			": IOC %s_reset routed to SCSI host driver!\n",
			reset_phase==MPT_IOC_SETUP_RESET ? "setup" : (
			reset_phase==MPT_IOC_PRE_RESET ? "pre" : "post")));

	/* If a FW reload request arrives after base installed but
	 * before all scsi hosts have been attached, then an alt_ioc
	 * may have a NULL sh pointer.
	 */
	if ((ioc->sh == NULL) || (ioc->sh->hostdata == NULL))
		return 0;
	else
		hd = (MPT_SCSI_HOST *) ioc->sh->hostdata;

	if (reset_phase == MPT_IOC_SETUP_RESET) {
		dtmprintk((MYIOC_s_WARN_FMT "Setup-Diag Reset\n", ioc->name));
		/* Clean Up:
		 * 1. Set Hard Reset Pending Flag
		 * All new commands go to doneQ
		 */
		hd->resetPending = 1;

		/* 2. Reset timeouts on all running commands
		 */
		mptscsih_reset_timeouts (hd);

	} else if (reset_phase == MPT_IOC_PRE_RESET) {
		dtmprintk((MYIOC_s_WARN_FMT "Pre-Diag Reset\n", ioc->name));

		/* 2. Flush running commands
		 *	Clean ScsiLookup (and associated memory)
		 *	AND clean mytaskQ
		 */

		/* 2b. Reply to OS all known outstanding I/O commands.
		 */
		mptscsih_flush_running_cmds(hd);

		/* 2c. If there was an internal command that
		 * has not completed, configuration or io request,
		 * free these resources.
		 */
		if (hd->cmdPtr) {
			del_timer(&hd->timer);
			mpt_free_msg_frame(ScsiScanDvCtx, ioc->id, hd->cmdPtr);
			atomic_dec(&queue_depth);
		}

		/* 2d. If a task management has not completed,
		 * free resources associated with this request.
		 */
		if (hd->tmPtr) {
			del_timer(&hd->TMtimer);
			mpt_free_msg_frame(ScsiTaskCtx, ioc->id, hd->tmPtr);
		}

#ifndef MPT_SCSI_USE_NEW_EH
		/* 2e. Delete all commands on taskQ
		 * Should be superfluous - as this taskQ should
		 * be empty.
		 */
		clean_taskQ(hd);
#endif
		
#ifdef MPTSCSIH_DBG_TIMEOUT
		ioc->timeout_hard = 0;
#endif

		dtmprintk((MYIOC_s_WARN_FMT "Pre-Reset complete.\n", ioc->name));
	} else {
		ScsiCfgData	*pSpi;

		dtmprintk((MYIOC_s_WARN_FMT "Post-Diag Reset\n", ioc->name));

		/* Once a FW reload begins, all new OS commands are
		 * redirected to the doneQ w/ a reset status.
		 * Init all control structures.
		 */

		/* ScsiLookup initialization
		 */
		{
			int ii;
			for (ii=0; ii < hd->ioc->req_depth; ii++)
				hd->ScsiLookup[ii] = NULL;
		}

		/* 2. Chain Buffer initialization
		 */
		mptscsih_initChainBuffers(hd, 0);

		/* 3. tmPtr clear
		 */
		if (hd->tmPtr) {
			hd->tmPtr = NULL;
		}

		/* 4. Renegotiate to all devices, if SCSI
		 */
		if (hd->is_spi)
			mptscsih_writeSDP1(hd, 0, 0, MPT_SCSICFG_ALL_IDS | MPT_SCSICFG_USE_NVRAM);

		/* 5. Enable new commands to be posted
		 */
		spin_lock_irqsave(&ioc->FreeQlock, flags);
		hd->tmPending = 0;
		spin_unlock_irqrestore(&ioc->FreeQlock, flags);
		hd->resetPending = 0;
		hd->numTMrequests = 0;
#ifdef MPT_SCSI_USE_NEW_EH
		hd->tmState = TM_STATE_NONE;
#endif

		/* 6. If there was an internal command,
		 * wake this process up.
		 */
		if (hd->cmdPtr) {
			/*
			 * Wake up the original calling thread
			 */
			hd->pLocal = &hd->localReply;
			hd->pLocal->completion = MPT_SCANDV_DID_RESET;
			scandv_wait_done = 1;
			wake_up(&scandv_waitq);
			hd->cmdPtr = NULL;
		}

		/* 7. Flush doneQ
		 */
		flush_doneQ(hd);

		/* 8. Set flag to force DV and re-read IOC Page 3
		 */
		if (hd->is_spi) {
			pSpi = &ioc->spi_data;
			pSpi->forceDv = MPT_SCSICFG_NEED_DV | MPT_SCSICFG_RELOAD_IOC_PG3;
			ddvtprintk(("Set reload IOC Pg3 Flag\n"));
		}

		dtmprintk((MYIOC_s_WARN_FMT "Post-Reset complete.\n", ioc->name));
	}

	return 1;		/* currently means nothing really */
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
static int
mptscsih_event_process(MPT_ADAPTER *ioc, EventNotificationReply_t *pEvReply)
{
	MPT_SCSI_HOST *hd;
	u8 event = le32_to_cpu(pEvReply->Event) & 0xFF;

	dprintk((MYIOC_s_INFO_FMT "MPT event (=%02Xh) routed to SCSI host driver!\n",
			ioc->name, event));

	switch (event) {
	case MPI_EVENT_UNIT_ATTENTION:			/* 03 */
		/* FIXME! */
		break;
	case MPI_EVENT_IOC_BUS_RESET:			/* 04 */
	case MPI_EVENT_EXT_BUS_RESET:			/* 05 */
		hd = NULL;
		if (ioc->sh) {
			hd = (MPT_SCSI_HOST *) ioc->sh->hostdata;
			if (hd && (hd->is_spi) && (hd->soft_resets < -1))
				hd->soft_resets++;
		}
		break;
	case MPI_EVENT_LOGOUT:				/* 09 */
		/* FIXME! */
		break;

		/*
		 *  CHECKME! Don't think we need to do
		 *  anything for these, but...
		 */
	case MPI_EVENT_RESCAN:				/* 06 */
	case MPI_EVENT_LINK_STATUS_CHANGE:		/* 07 */
	case MPI_EVENT_LOOP_STATE_CHANGE:		/* 08 */
		/*
		 *  CHECKME!  Falling thru...
		 */
		break;

	case MPI_EVENT_INTEGRATED_RAID:			/* 0B */
#ifndef MPTSCSIH_DISABLE_DOMAIN_VALIDATION
		/* negoNvram set to 0 if DV enabled and to USE_NVRAM if
		 * if DV disabled. Need to check for target mode.
		 */
		hd = NULL;
		if (ioc->sh)
			hd = (MPT_SCSI_HOST *) ioc->sh->hostdata;

		if (hd && (hd->is_spi) && (hd->negoNvram == 0)) {
			ScsiCfgData	*pSpi;
			Ioc3PhysDisk_t	*pPDisk;
			int		 numPDisk;
			u8		 reason;
			u8		 physDiskNum;
			
			reason = (le32_to_cpu(pEvReply->Data[0]) & 0x00FF0000) >> 16;
			if (reason == MPI_EVENT_RAID_RC_DOMAIN_VAL_NEEDED) {
				/* New or replaced disk.
				 * Set DV flag and schedule DV.
				 */
				pSpi = &ioc->spi_data;
				physDiskNum = (le32_to_cpu(pEvReply->Data[0]) & 0xFF000000) >> 24;
				ddvtprintk(("DV requested for phys disk id %d\n", physDiskNum));
				if (pSpi->pIocPg3) {
					pPDisk =  pSpi->pIocPg3->PhysDisk;
					numPDisk =pSpi->pIocPg3->NumPhysDisks;

					while (numPDisk) {
						if (physDiskNum == pPDisk->PhysDiskNum) {
							pSpi->dvStatus[pPDisk->PhysDiskID] = (MPT_SCSICFG_NEED_DV | MPT_SCSICFG_DV_NOT_DONE);
							pSpi->forceDv = MPT_SCSICFG_NEED_DV;
							ddvtprintk(("NEED_DV set for phys disk id %d\n", pPDisk->PhysDiskID));
							break;
						}
						pPDisk++;
						numPDisk--;
					}

					if (numPDisk == 0) {
						/* The physical disk that needs DV was not found
						 * in the stored IOC Page 3. The driver must reload
						 * this page. DV routine will set the NEED_DV flag for
						 * all phys disks that have DV_NOT_DONE set.
						 */
						pSpi->forceDv = MPT_SCSICFG_NEED_DV | MPT_SCSICFG_RELOAD_IOC_PG3;
						ddvtprintk(("phys disk %d not found. Setting reload IOC Pg3 Flag\n", physDiskNum));
					}
				}
			}
		}
#endif

#if defined(MPT_DEBUG_DV) || defined(MPT_DEBUG_DV_TINY)
		printk("Raid Event RF: ");
		{
			u32 *m = (u32 *)pEvReply;
			int ii;
			int n = (int)pEvReply->MsgLength;
			for (ii=6; ii < n; ii++)
				printk(" %08x", le32_to_cpu(m[ii]));
			printk("\n");
		}
#endif
		break;

	case MPI_EVENT_NONE:				/* 00 */
	case MPI_EVENT_LOG_DATA:			/* 01 */
	case MPI_EVENT_STATE_CHANGE:			/* 02 */
	case MPI_EVENT_EVENT_CHANGE:			/* 0A */
	default:
		dprintk((KERN_INFO "  Ignoring event (=%02Xh)\n", event));
		break;
	}

	return 1;		/* currently means nothing really */
}

#if 0		/* { */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	scsiherr.c - Fusion MPT SCSI Host driver error handling/reporting.
 *
 *	drivers/message/fusion/scsiherr.c
 */

//extern const char	**mpt_ScsiOpcodesPtr;	/* needed by mptscsih.c */
//extern ASCQ_Table_t	 *mpt_ASCQ_TablePtr;
//extern int		  mpt_ASCQ_TableSz;

#define MYNAM	"mptscsih"

#endif		/* } */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Private data...
 */
static ASCQ_Table_t *mptscsih_ASCQ_TablePtr;

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/* old symsense.c stuff... */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 * Private data...
 * To protect ourselves against those that would pass us bogus pointers
 */
static u8 dummyInqData[SCSI_STD_INQUIRY_BYTES]
    = { 0x1F, 0x00, 0x00, 0x00,
	0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static u8 dummySenseData[SCSI_STD_SENSE_BYTES]
    = { 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00 };
static u8 dummyCDB[16]
    = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static u8 dummyScsiData[16]
    = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#if 0
static const char *PeripheralDeviceTypeString[32] = {
	"Direct-access",		/* 00h */
	"Sequential-access",		/* 01h */
	"Printer",			/* 02h */
	"Processor",			/* 03h */
			/*"Write-Once-Read-Multiple",*/	/* 04h */
	"WORM",				/* 04h */
	"CD-ROM",			/* 05h */
	"Scanner",			/* 06h */
	"Optical memory",		/* 07h */
	"Media Changer",		/* 08h */
	"Communications",		/* 09h */
	"(Graphics arts pre-press)",	/* 0Ah */
	"(Graphics arts pre-press)",	/* 0Bh */
	"Array controller",		/* 0Ch */
	"Enclosure services",		/* 0Dh */
	"Simplified direct-access",	/* 0Eh */
	"Reserved-0Fh",			/* 0Fh */
	"Reserved-10h",			/* 10h */
	"Reserved-11h",			/* 11h */
	"Reserved-12h",			/* 12h */
	"Reserved-13h",			/* 13h */
	"Reserved-14h",			/* 14h */
	"Reserved-15h",			/* 15h */
	"Reserved-16h",			/* 16h */
	"Reserved-17h",			/* 17h */
	"Reserved-18h",			/* 18h */
	"Reserved-19h",			/* 19h */
	"Reserved-1Ah",			/* 1Ah */
	"Reserved-1Bh",			/* 1Bh */
	"Reserved-1Ch",			/* 1Ch */
	"Reserved-1Dh",			/* 1Dh */
	"Reserved-1Eh",			/* 1Eh */
	"Unknown"			/* 1Fh */
};
#endif

static char *ScsiStatusString[] = {
	"GOOD",					/* 00h */
	NULL,					/* 01h */
	"CHECK CONDITION",			/* 02h */
	NULL,					/* 03h */
	"CONDITION MET",			/* 04h */
	NULL,					/* 05h */
	NULL,					/* 06h */
	NULL,					/* 07h */
	"BUSY",					/* 08h */
	NULL,					/* 09h */
	NULL,					/* 0Ah */
	NULL,					/* 0Bh */
	NULL,					/* 0Ch */
	NULL,					/* 0Dh */
	NULL,					/* 0Eh */
	NULL,					/* 0Fh */
	"INTERMEDIATE",				/* 10h */
	NULL,					/* 11h */
	NULL,					/* 12h */
	NULL,					/* 13h */
	"INTERMEDIATE-CONDITION MET",		/* 14h */
	NULL,					/* 15h */
	NULL,					/* 16h */
	NULL,					/* 17h */
	"RESERVATION CONFLICT",			/* 18h */
	NULL,					/* 19h */
	NULL,					/* 1Ah */
	NULL,					/* 1Bh */
	NULL,					/* 1Ch */
	NULL,					/* 1Dh */
	NULL,					/* 1Eh */
	NULL,					/* 1Fh */
	NULL,					/* 20h */
	NULL,					/* 21h */
	"COMMAND TERMINATED",			/* 22h */
	NULL,					/* 23h */
	NULL,					/* 24h */
	NULL,					/* 25h */
	NULL,					/* 26h */
	NULL,					/* 27h */
	"TASK SET FULL",			/* 28h */
	NULL,					/* 29h */
	NULL,					/* 2Ah */
	NULL,					/* 2Bh */
	NULL,					/* 2Ch */
	NULL,					/* 2Dh */
	NULL,					/* 2Eh */
	NULL,					/* 2Fh */
	"ACA ACTIVE",				/* 30h */
	NULL
};

static const char *ScsiCommonOpString[] = {
	"TEST UNIT READY",			/* 00h */
	"REZERO UNIT (REWIND)",			/* 01h */
	NULL,					/* 02h */
	"REQUEST_SENSE",			/* 03h */
	"FORMAT UNIT (MEDIUM)",			/* 04h */
	"READ BLOCK LIMITS",			/* 05h */
	NULL,					/* 06h */
	"REASSIGN BLOCKS",			/* 07h */
	"READ(6)",				/* 08h */
	NULL,					/* 09h */
	"WRITE(6)",				/* 0Ah */
	"SEEK(6)",				/* 0Bh */
	NULL,					/* 0Ch */
	NULL,					/* 0Dh */
	NULL,					/* 0Eh */
	"READ REVERSE",				/* 0Fh */
	"WRITE_FILEMARKS",			/* 10h */
	"SPACE(6)",				/* 11h */
	"INQUIRY",				/* 12h */
	NULL
};

static const char *SenseKeyString[] = {
	"NO SENSE",				/* 0h */
	"RECOVERED ERROR",			/* 1h */
	"NOT READY",				/* 2h */
	"MEDIUM ERROR",				/* 3h */
	"HARDWARE ERROR",			/* 4h */
	"ILLEGAL REQUEST",			/* 5h */
	"UNIT ATTENTION",			/* 6h */
	"DATA PROTECT",				/* 7h */
	"BLANK CHECK",				/* 8h */
	"VENDOR-SPECIFIC",			/* 9h */
	"ABORTED COPY",				/* Ah */
	"ABORTED COMMAND",			/* Bh */
	"EQUAL (obsolete)",			/* Ch */
	"VOLUME OVERFLOW",			/* Dh */
	"MISCOMPARE",				/* Eh */
	"RESERVED",				/* Fh */
	NULL
};

#define SPECIAL_ASCQ(c,q) \
	(((c) == 0x40 && (q) != 0x00) || ((c) == 0x4D) || ((c) == 0x70))

#if 0
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Sense_Key_Specific() - If Sense_Key_Specific_Valid bit is set,
 *			   then print additional information via
 *			   a call to SDMS_SystemAlert().
 */
static void Sense_Key_Specific(IO_Info_t *ioop, char *msg1)
{
	u8	*sd;
	u8	 BadValue;
	u8	 SenseKey;
	int	 Offset;
	int	 len = strlen(msg1);

	sd = ioop->sensePtr;
	if (SD_Additional_Sense_Length(sd) < 8)
		return;

	SenseKey = SD_Sense_Key(sd);

	if (SD_Sense_Key_Specific_Valid(sd)) {
		if (SenseKey == SK_ILLEGAL_REQUEST) {
			Offset = SD_Bad_Byte(sd);
			if (SD_Was_Illegal_Request(sd)) {
				BadValue = ioop->cdbPtr[Offset];
				len += sprintf(msg1+len, "\n  Illegal CDB value=%02Xh found at CDB ",
						BadValue);
		} else {
			BadValue = ioop->dataPtr[Offset];
			len += sprintf(msg1+len, "\n  Illegal DATA value=%02Xh found at DATA ",
					BadValue);
		}
		len += sprintf(msg1+len, "byte=%02Xh", Offset);
		if (SD_SKS_Bit_Pointer_Valid(sd))
			len += sprintf(msg1+len, "/bit=%1Xh", SD_SKS_Bit_Pointer(sd));
		} else if ((SenseKey == SK_RECOVERED_ERROR) ||
			   (SenseKey == SK_HARDWARE_ERROR) ||
			   (SenseKey == SK_MEDIUM_ERROR)) {
			len += sprintf(msg1+len, "\n  Recovery algorithm Actual_Retry_Count=%02Xh",
			SD_Actual_Retry_Count(sd));
		}
	}
}
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
static int dump_cdb(char *foo, unsigned char *cdb)
{
	int i, grpCode, cdbLen;
	int l = 0;

	grpCode = cdb[0] >> 5;
	if (grpCode < 1)
		cdbLen = 6;
	else if (grpCode < 3)
		cdbLen = 10;
	else if (grpCode == 5)
		cdbLen = 12;
	else
		cdbLen = 16;

	for (i=0; i < cdbLen; i++)
		l += sprintf(foo+l, " %02X", cdb[i]);

	return l;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#if 0
static int dump_sd(char *foo, unsigned char *sd)
{
	int snsLen = 8 + SD_Additional_Sense_Length(sd);
	int l = 0;
	int i;

	for (i=0; i < MIN(snsLen,18); i++)
		l += sprintf(foo+l, " %02X", sd[i]);
	l += sprintf(foo+l, "%s", snsLen>18 ? " ..." : "");

	return l;
}
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*  Do ASC/ASCQ lookup/grindage to English readable string(s)  */
static const char * ascq_set_strings_4max(
		u8 ASC, u8 ASCQ,
		const char **s1, const char **s2, const char **s3, const char **s4)
{
	static const char *asc_04_part1_string = "LOGICAL UNIT ";
	static const char *asc_04_part2a_string = "NOT READY, ";
	static const char *asc_04_part2b_string = "IS ";
	static const char *asc_04_ascq_NN_part3_strings[] = {	/* ASC ASCQ (hex) */
	  "CAUSE NOT REPORTABLE",				/* 04 00 */
	  "IN PROCESS OF BECOMING READY",			/* 04 01 */
	  "INITIALIZING CMD. REQUIRED",				/* 04 02 */
	  "MANUAL INTERVENTION REQUIRED",			/* 04 03 */
	  /* Add	" IN PROGRESS" to all the following... */
	  "FORMAT",						/* 04 04 */
	  "REBUILD",						/* 04 05 */
	  "RECALCULATION",					/* 04 06 */
	  "OPERATION",						/* 04 07 */
	  "LONG WRITE",						/* 04 08 */
	  "SELF-TEST",						/* 04 09 */
	  NULL
	};
	static char *asc_04_part4_string = " IN PROGRESS";

	static char *asc_29_ascq_NN_strings[] = {		/* ASC ASCQ (hex) */
	  "POWER ON, RESET, OR BUS DEVICE RESET OCCURRED",	/* 29 00 */
	  "POWER ON OCCURRED",					/* 29 01 */
	  "SCSI BUS RESET OCCURRED",				/* 29 02 */
	  "BUS DEVICE RESET FUNCTION OCCURRED",			/* 29 03 */
	  "DEVICE INTERNAL RESET",				/* 29 04 */
	  "TRANSCEIVER MODE CHANGED TO SINGLE-ENDED",		/* 29 05 */
	  "TRANSCEIVER MODE CHANGED TO LVD",			/* 29 06 */
	  NULL
	};
	static char *ascq_vendor_uniq = "(Vendor Unique)";
	static char *ascq_noone = "(no matching ASC/ASCQ description found)";
	int idx;

	*s1 = *s2 = *s3 = *s4 = "";		/* set'em all to the empty "" string */

	/* CHECKME! Need lock/sem?
	 *  Update and examine for isense module presense.
	 */
	mptscsih_ASCQ_TablePtr = (ASCQ_Table_t *)mpt_v_ASCQ_TablePtr;

	if (mptscsih_ASCQ_TablePtr == NULL) {
		/* 2nd chances... */
		if (ASC == 0x04 && (ASCQ < sizeof(asc_04_ascq_NN_part3_strings)/sizeof(char*)-1)) {
			*s1 = asc_04_part1_string;
			*s2 = (ASCQ == 0x01) ? asc_04_part2b_string : asc_04_part2a_string;
			*s3 = asc_04_ascq_NN_part3_strings[ASCQ];
			/* check for " IN PROGRESS" ones */
			if (ASCQ >= 0x04)
				*s4 = asc_04_part4_string;
		} else if (ASC == 0x29 && (ASCQ < sizeof(asc_29_ascq_NN_strings)/sizeof(char*)-1))
			*s1 = asc_29_ascq_NN_strings[ASCQ];
		/*
		 *	Else { leave all *s[1-4] values pointing to the empty "" string }
		 */
		return *s1;
	}

	/*
	 * Need to check ASC here; if it is "special," then
	 * the ASCQ is variable, and indicates failed component number.
	 * We must treat the ASCQ as a "don't care" while searching the
	 * mptscsih_ASCQ_Table[] by masking it off, and then restoring it later
	 * on when we actually need to identify the failed component.
	 */
	if (SPECIAL_ASCQ(ASC,ASCQ))
		ASCQ = 0xFF;

	/* OK, now search mptscsih_ASCQ_Table[] for a matching entry */
	for (idx = 0; mptscsih_ASCQ_TablePtr && idx < mpt_ASCQ_TableSz; idx++)
		if ((ASC == mptscsih_ASCQ_TablePtr[idx].ASC) && (ASCQ == mptscsih_ASCQ_TablePtr[idx].ASCQ)) {
			*s1 = mptscsih_ASCQ_TablePtr[idx].Description;
			return *s1;
		}

	if ((ASC >= 0x80) || (ASCQ >= 0x80))
		*s1 = ascq_vendor_uniq;
	else
		*s1 = ascq_noone;

	return *s1;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  SCSI Information Report; desired output format...
 *---
SCSI Error: (iocnum:target_id:LUN) Status=02h (CHECK CONDITION)
  Key=6h (UNIT ATTENTION); FRU=03h
  ASC/ASCQ=29h/00h, "POWER ON, RESET, OR BUS DEVICE RESET OCCURRED"
  CDB: 00 00 00 00 00 00 - TestUnitReady
 *---
 */
/*
 *  SCSI Error Report; desired output format...
 *---
SCSI Error Report =-=-=-=-=-=-=-=-=-=-=-=-=-= (ioc0,scsi0:0)
  SCSI_Status=02h (CHECK CONDITION)
  Original_CDB[]: 00 00 00 00 00 00 - TestUnitReady
  SenseData[12h]: 70 00 06 00 00 00 00 0A 00 00 00 00 29 00 03 00 00 00
  SenseKey=6h (UNIT ATTENTION); FRU=03h
  ASC/ASCQ=29h/00h, "POWER ON, RESET, OR BUS DEVICE RESET OCCURRED"
 *---
 */

int mpt_ScsiHost_ErrorReport(IO_Info_t *ioop)
{
	char		 foo[512];
	char		 buf2[32];
	char		*statstr;
	const char	*opstr;
	int		 sk		= SD_Sense_Key(ioop->sensePtr);
	const char	*skstr		= SenseKeyString[sk];
	unsigned char	 asc		= SD_ASC(ioop->sensePtr);
	unsigned char	 ascq		= SD_ASCQ(ioop->sensePtr);
	int		 l;

	/* Change the error logging to only report errors on
	 * read and write commands. Ignore errors on other commands.
	 * Should this be configurable via proc?
	 */
	switch (ioop->cdbPtr[0]) {
	case READ_6:
	case WRITE_6:
	case READ_10:
	case WRITE_10:
	case READ_12:
	case WRITE_12:
		break;
	default:
		return 0;
	}

	/*
	 *  More quiet mode.
	 *  Filter out common, repetitive, warning-type errors...  like:
	 *    POWER ON (06,29/00 or 06,29/01),
	 *    SPINNING UP (02,04/01),
	 *    LOGICAL UNIT NOT SUPPORTED (05,25/00), etc.
	 */
	if (sk == SK_NO_SENSE) {
		return 0;
	}

	if (	(sk==SK_UNIT_ATTENTION	&& asc==0x29 && (ascq==0x00 || ascq==0x01))
	     || (sk==SK_NOT_READY	&& asc==0x04 && (ascq==0x01 || ascq==0x02))
	     || (sk==SK_ILLEGAL_REQUEST && asc==0x25 && ascq==0x00)
	   )
	{
		/* Do nothing! */
		return 0;
	}

	/* Prevent the system from continually writing to the log
	 * if a medium is not found: 02 3A 00
	 * Changer issues: TUR, Read Capacity, Table of Contents continually
	 */
	if (sk==SK_NOT_READY && asc==0x3A) {
		if (ioop->cdbPtr == NULL) {
			return 0;
		} else if ((ioop->cdbPtr[0] == CMD_TestUnitReady) ||
			(ioop->cdbPtr[0] == CMD_ReadCapacity) ||
			(ioop->cdbPtr[0] == 0x43)) {
			return 0;
		}
	}
	if (sk==SK_UNIT_ATTENTION) {
		if (ioop->cdbPtr == NULL)
			return 0;
		else if (ioop->cdbPtr[0] == CMD_TestUnitReady)
			return 0;
	}

	/*
	 *  Protect ourselves...
	 */
	if (ioop->cdbPtr == NULL)
		ioop->cdbPtr = dummyCDB;
	if (ioop->sensePtr == NULL)
		ioop->sensePtr = dummySenseData;
	if (ioop->inqPtr == NULL)
		ioop->inqPtr = dummyInqData;
	if (ioop->dataPtr == NULL)
		ioop->dataPtr = dummyScsiData;

	statstr = NULL;
	if ((ioop->SCSIStatus >= sizeof(ScsiStatusString)/sizeof(char*)-1) ||
	    ((statstr = (char*)ScsiStatusString[ioop->SCSIStatus]) == NULL)) {
		(void) sprintf(buf2, "Bad-Reserved-%02Xh", ioop->SCSIStatus);
		statstr = buf2;
	}

	opstr = NULL;
	if (1+ioop->cdbPtr[0] <= sizeof(ScsiCommonOpString)/sizeof(char*))
		opstr = ScsiCommonOpString[ioop->cdbPtr[0]];
	else if (mpt_ScsiOpcodesPtr)
		opstr = mpt_ScsiOpcodesPtr[ioop->cdbPtr[0]];

	l = sprintf(foo, "SCSI Error: (%s) Status=%02Xh (%s)\n",
			  ioop->DevIDStr,
			  ioop->SCSIStatus,
			  statstr);
	l += sprintf(foo+l, " Key=%Xh (%s); FRU=%02Xh\n ASC/ASCQ=%02Xh/%02Xh",
		  sk, skstr, SD_FRU(ioop->sensePtr), asc, ascq );
	{
		const char	*x1, *x2, *x3, *x4;
		x1 = x2 = x3 = x4 = "";
		x1 = ascq_set_strings_4max(asc, ascq, &x1, &x2, &x3, &x4);
		if (x1 != NULL) {
			if (x1[0] != '(')
				l += sprintf(foo+l, " \"%s%s%s%s\"", x1,x2,x3,x4);
			else
				l += sprintf(foo+l, " %s%s%s%s", x1,x2,x3,x4);
		}
	}
	l += sprintf(foo+l, "\n CDB:");
	l += dump_cdb(foo+l, ioop->cdbPtr);
	if (opstr)
		l += sprintf(foo+l, " - \"%s\"", opstr);
	l += sprintf(foo+l, "\n");

	PrintF(("%s\n", foo));

	return l;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_initTarget - Target, LUN alloc/free functionality.
 *	@hd: Pointer to MPT_SCSI_HOST structure
 *	@bus_id: Bus number (?)
 *	@target_id: SCSI target id
 *	@lun: SCSI LUN id
 *	@data: Pointer to data
 *	@dlen: Number of INQUIRY bytes
 *
 *	NOTE: It's only SAFE to call this routine if data points to
 *	sane & valid STANDARD INQUIRY data!
 *
 *	Allocate and initialize memory for this target.
 *	Save inquiry data.
 *
 */
static void 
mptscsih_initTarget(MPT_SCSI_HOST *hd, int bus_id, int target_id, u8 lun, char *data, int dlen)
{
	int		indexed_lun, lun_index;
	VirtDevice	*vdev;

	dprintk((MYIOC_s_INFO_FMT "initTarget (%d,%d,%d) called, hd=%p\n",
			hd->ioc->name, bus_id, target_id, lun, hd));

	if ((vdev = hd->Targets[target_id]) == NULL) {
		if ((vdev = kmalloc(sizeof(VirtDevice), GFP_ATOMIC)) == NULL) {
			printk(MYIOC_s_ERR_FMT "initTarget kmalloc(%d) FAILED!\n",
					hd->ioc->name, (int)sizeof(VirtDevice));
			return;
		} else {
			memset(vdev, 0, sizeof(VirtDevice));
			rwlock_init(&vdev->VdevLock);
			Q_INIT(&vdev->WaitQ, void);
			Q_INIT(&vdev->SentQ, void);
			Q_INIT(&vdev->DoneQ, void);
			vdev->tflags = 0;
			vdev->ioc_id = hd->ioc->id;
			vdev->target_id = target_id;
			vdev->bus_id = bus_id;

			hd->Targets[target_id] = vdev;
			dprintk((KERN_INFO "  *NEW* Target structure (id %d) @ %p\n",
					target_id, vdev));
		}
	}

	vdev->raidVolume = 0;
	if (hd->is_spi) {
		if (hd->ioc->spi_data.isRaid & (1 << target_id)) {
			vdev->raidVolume = 1;
			ddvtprintk((KERN_INFO "RAID Volume @ id %d\n", target_id));
		}
	}

	if (!(vdev->tflags & MPT_TARGET_FLAGS_VALID_INQUIRY)) {
		/* Copy the inquiry data  - if we haven't yet.
		*/

		memcpy (vdev->inq_data, data, 8);

		if ( (data[0] == SCSI_TYPE_PROC) && 
			!(vdev->tflags & MPT_TARGET_FLAGS_SAF_TE_ISSUED )) {
			if ( dlen > 49 ) {
				vdev->tflags |= MPT_TARGET_FLAGS_VALID_INQUIRY;
				if ( data[44] == 'S' && 
				     data[45] == 'A' && 
				     data[46] == 'F' && 
				     data[47] == '-' && 
				     data[48] == 'T' && 
				     data[49] == 'E' ) {
					vdev->tflags |= MPT_TARGET_FLAGS_SAF_TE_ISSUED;
					mptscsih_writeIOCPage4(hd, target_id, bus_id);
				}
			} else {
				/* Treat all Processors as SAF-TE if 
				 * command line option is set */
				if ( hd->ioc->spi_data.Saf_Te ) {
					vdev->tflags |= MPT_TARGET_FLAGS_SAF_TE_ISSUED;
					mptscsih_writeIOCPage4(hd, target_id, bus_id);
				}
			}
		} else 
			vdev->tflags |= MPT_TARGET_FLAGS_VALID_INQUIRY;

		if ((dlen > 56) && (!(vdev->tflags & MPT_TARGET_FLAGS_VALID_56))) {
			/* Update the target capabilities
			 */
			if (dlen > 56) {
				mptscsih_setTargetNegoParms(hd, vdev, data[56]);
				vdev->tflags |= MPT_TARGET_FLAGS_VALID_56;
			} else
				mptscsih_setTargetNegoParms(hd, vdev, 0);

			/* If LUN 0, tape and have not done DV, set the DV flag.
			 */
			if (hd->is_spi && (lun == 0) && ((data[0] & 0x1F) == 0x01)) {
				ScsiCfgData *pSpi = &hd->ioc->spi_data;
				if (pSpi->dvStatus[target_id] & MPT_SCSICFG_DV_NOT_DONE)
					pSpi->dvStatus[target_id] |= MPT_SCSICFG_NEED_DV;
			}
		}
	}

	/* Is LUN supported? If so, upper 3 bits will be 0
	 * in first byte of inquiry data.
	 */
	if ((*data & 0xe0) == 0) {
		lun_index = (lun >> 5);  /* 32 luns per lun_index */
		indexed_lun = (lun % 32);
		vdev->luns[lun_index] |= (1 << indexed_lun);
	}

	dprintk((KERN_INFO "  target = %p\n", vdev));
	return;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Update the target negotiation parameters based on the
 *  the Inquiry data, adapter capabilities, and NVRAM settings.
 *
 */
void mptscsih_setTargetNegoParms(MPT_SCSI_HOST *hd, VirtDevice *target, char byte56)
{
	ScsiCfgData *pspi_data = &hd->ioc->spi_data;
	int  id = (int) target->target_id;
	int  nvram;
	char canQ = 0;
	VirtDevice	*vdev;
	int ii;
	u8 width = MPT_NARROW;
	u8 factor = MPT_ASYNC;
	u8 offset = 0;
	u8 version, nfactor;
	u8 noQas = 1;

	if (!hd->is_spi) {
		if (target->tflags & MPT_TARGET_FLAGS_VALID_INQUIRY) {
			if (target->inq_data[7] & 0x02)
				target->tflags |= MPT_TARGET_FLAGS_Q_YES;
		}
		return;
	}

	target->negoFlags = pspi_data->noQas;

	/* noQas == 0 => device supports QAS. Need byte 56 of Inq to determine
	 * support. If available, default QAS to off and allow enabling.
	 * If not available, default QAS to on, turn off for non-disks.
	 */

	/* Set flags based on Inquiry data
	 */
	if (target->tflags & MPT_TARGET_FLAGS_VALID_INQUIRY) {
		version = target->inq_data[2] & 0x07;
		if (version < 2) {
			width = 0;
			factor = MPT_ULTRA2;
			offset = pspi_data->maxSyncOffset;
		} else {
			if (target->inq_data[7] & 0x20) {
				width = 1;
			}

			if (target->inq_data[7] & 0x10) {
				/* bits 2 & 3 show DT support
				 */
				if ((byte56 & 0x04) == 0)
					factor = MPT_ULTRA2;
				else if ((byte56 & 0x03) == 0)
					factor = MPT_ULTRA160;
				else
					factor = MPT_ULTRA320;
				offset = pspi_data->maxSyncOffset;

				/* If RAID, never disable QAS
				 * else if non RAID, do not disable
				 *   QAS if bit 1 is set
				 * bit 1 QAS support, non-raid only
				 * bit 0 IU support
				 */
				if ((target->raidVolume == 1) || ((byte56 & 0x02) != 0))
					noQas = 0;
			} else {
				factor = MPT_ASYNC;
				offset = 0;
			}
		}

		if (target->inq_data[7] & 0x02) {
			canQ = 1;
		}

		/* Update tflags based on NVRAM settings. (SCSI only)
		 */
		if (pspi_data->nvram && (pspi_data->nvram[id] != MPT_HOST_NVRAM_INVALID)) {
			nvram = pspi_data->nvram[id];
			nfactor = (nvram & MPT_NVRAM_SYNC_MASK) >> 8;

			if (width)
				width = nvram & MPT_NVRAM_WIDE_DISABLE ? 0 : 1;

			if (offset > 0) {
				/* Ensure factor is set to the
				 * maximum of: adapter, nvram, inquiry
				 */
				if (nfactor) {
					if (nfactor < pspi_data->minSyncFactor )
						nfactor = pspi_data->minSyncFactor;

					factor = MAX (factor, nfactor);
					if (factor == MPT_ASYNC)
						offset = 0;
				} else {
					offset = 0;
					factor = MPT_ASYNC;
				}
			} else {
				factor = MPT_ASYNC;
			}
		}

		/* Make sure data is consistent
		 */
		if ((!width) && (factor < MPT_ULTRA2)) {
			factor = MPT_ULTRA2;
		}

		/* Save the data to the target structure.
		 */
		target->minSyncFactor = factor;
		target->maxOffset = offset;
		target->maxWidth = width;
		if (canQ) {
			target->tflags |= MPT_TARGET_FLAGS_Q_YES;
		}

		target->tflags |= MPT_TARGET_FLAGS_VALID_NEGO;

		/* Disable unused features.
		 */
		if (!width)
			target->negoFlags |= MPT_TARGET_NO_NEGO_WIDE;

		if (!offset)
			target->negoFlags |= MPT_TARGET_NO_NEGO_SYNC;

		/* GEM, processor WORKAROUND
		 */
		if (((target->inq_data[0] & 0x1F) == 0x03) || ((target->inq_data[0] & 0x1F) > 0x08)) {
			target->negoFlags |= (MPT_TARGET_NO_NEGO_WIDE | MPT_TARGET_NO_NEGO_SYNC);
			pspi_data->dvStatus[id] |= MPT_SCSICFG_BLK_NEGO;
		} else {
			if (noQas && (pspi_data->noQas == 0)) {
				pspi_data->noQas |= MPT_TARGET_NO_NEGO_QAS;
				target->negoFlags |= MPT_TARGET_NO_NEGO_QAS;

				/* Disable QAS in a mixed configuration case
		 		*/

//				ddvtprintk((KERN_INFO "Disabling QAS!\n"));
				for (ii = 0; ii < id; ii++) {
					if ( (vdev = hd->Targets[ii]) ) {
						vdev->negoFlags |= MPT_TARGET_NO_NEGO_QAS;
					}	
				}
			}
		}
	}

	return;
}

/* If DV disabled (negoNvram set to USE_NVARM) or if not LUN 0, return.
 * Else set the NEED_DV flag after Read Capacity Issued (disks)
 * or Mode Sense (cdroms).
 *
 * Tapes, initTarget will set this flag on completion of Inquiry command.
 * Called only if DV_NOT_DONE flag is set
 */
static void mptscsih_set_dvflags(MPT_SCSI_HOST *hd, SCSIIORequest_t *pReq)
{
	u8 cmd;
	
	if ((pReq->LUN[1] != 0) || (hd->negoNvram != 0))
		return;

	cmd = pReq->CDB[0];

	if ((cmd == READ_CAPACITY) || (cmd == MODE_SENSE)) {
		ScsiCfgData *pSpi = &hd->ioc->spi_data;
		if ((pSpi->isRaid & (1 << pReq->TargetID)) && pSpi->pIocPg3) {
			/* Set NEED_DV for all hidden disks
			 */
			Ioc3PhysDisk_t *pPDisk =  pSpi->pIocPg3->PhysDisk;
			int		numPDisk = pSpi->pIocPg3->NumPhysDisks;

			while (numPDisk) {
				pSpi->dvStatus[pPDisk->PhysDiskID] |= MPT_SCSICFG_NEED_DV;
				ddvtprintk(("NEED_DV set for phys disk id %d\n", pPDisk->PhysDiskID));
				pPDisk++;
				numPDisk--;
			}
		}
		pSpi->dvStatus[pReq->TargetID] |= MPT_SCSICFG_NEED_DV;
		ddvtprintk(("NEED_DV set for visible disk id %d\n", pReq->TargetID));
	}
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 * If no Target, bus reset on 1st I/O. Set the flag to
 * prevent any future negotiations to this device.
 */
static void mptscsih_no_negotiate(MPT_SCSI_HOST *hd, int target_id)
{

	if ((hd->Targets) && (hd->Targets[target_id] == NULL))
		hd->ioc->spi_data.dvStatus[target_id] |= MPT_SCSICFG_BLK_NEGO;

	return;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  SCSI Config Page functionality ...
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*	mptscsih_setDevicePage1Flags  - add Requested and Configuration fields flags
 *	based on width, factor and offset parameters.
 *	@width: bus width
 *	@factor: sync factor
 *	@offset: sync offset
 *	@requestedPtr: pointer to requested values (updated)
 *	@configurationPtr: pointer to configuration values (updated)
 *	@flags: flags to block WDTR or SDTR negotiation
 *
 *	Return: None.
 *
 *	Remark: Called by writeSDP1 and _dv_params
 */
static void
mptscsih_setDevicePage1Flags (u8 width, u8 factor, u8 offset, int *requestedPtr, int *configurationPtr, u8 flags)
{
	u8 nowide = flags & MPT_TARGET_NO_NEGO_WIDE;
	u8 nosync = flags & MPT_TARGET_NO_NEGO_SYNC;

	*configurationPtr = 0;
	*requestedPtr = width ? MPI_SCSIDEVPAGE1_RP_WIDE : 0;
	*requestedPtr |= (offset << 16) | (factor << 8);

	if (width && offset && !nowide && !nosync) {
		if (factor < MPT_ULTRA160) {
			*requestedPtr |= (MPI_SCSIDEVPAGE1_RP_IU + MPI_SCSIDEVPAGE1_RP_DT);
			if ((flags & MPT_TARGET_NO_NEGO_QAS) == 0)
				*requestedPtr |= MPI_SCSIDEVPAGE1_RP_QAS;
		} else if (factor < MPT_ULTRA2) {
			*requestedPtr |= MPI_SCSIDEVPAGE1_RP_DT;
		}
	}

	if (nowide)
		*configurationPtr |= MPI_SCSIDEVPAGE1_CONF_WDTR_DISALLOWED;

	if (nosync)
		*configurationPtr |= MPI_SCSIDEVPAGE1_CONF_SDTR_DISALLOWED;

	return;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*	mptscsih_writeSDP1  - write SCSI Device Page 1
 *	@hd: Pointer to a SCSI Host Strucutre
 *	@portnum: IOC port number
 *	@target_id: writeSDP1 for single ID
 *	@flags: MPT_SCSICFG_ALL_IDS, MPT_SCSICFG_USE_NVRAM, MPT_SCSICFG_BLK_NEGO
 *
 *	Return: -EFAULT if read of config page header fails
 *		or 0 if success.
 *
 *	Remark: If a target has been found, the settings from the
 *		target structure are used, else the device is set
 *		to async/narrow.
 *
 *	Remark: Called during init and after a FW reload.
 *	Remark: We do not wait for a return, write pages sequentially.
 */
static int
mptscsih_writeSDP1(MPT_SCSI_HOST *hd, int portnum, int target_id, int flags)
{
	MPT_ADAPTER		*ioc = hd->ioc;
	Config_t		*pReq;
	SCSIDevicePage1_t	*pData;
	VirtDevice		*pTarget;
	MPT_FRAME_HDR		*mf;
	dma_addr_t		 dataDma;
	u16			 req_idx;
	u32			 frameOffset;
	u32			 requested, configuration, flagsLength;
	int			 ii, nvram;
	int			 id = 0, maxid = 0;
	u8			 width;
	u8			 factor;
	u8			 offset;
	u8			 bus = 0;
	u8			 negoFlags;
	u8			 maxwidth, maxoffset, maxfactor;

	if (ioc->spi_data.sdp1length == 0)
		return 0;

	if (flags & MPT_SCSICFG_ALL_IDS) {
		id = 0;
		maxid = ioc->sh->max_id - 1;
	} else if (ioc->sh) {
		id = target_id;
		maxid = MIN(id, ioc->sh->max_id - 1);
	}

	for (; id <= maxid; id++) {

		if (id == ioc->pfacts[portnum].PortSCSIID)
			continue;

		/* Use NVRAM to get adapter and target maximums
		 * Data over-riden by target structure information, if present
		 */
		maxwidth = ioc->spi_data.maxBusWidth;
		maxoffset = ioc->spi_data.maxSyncOffset;
		maxfactor = ioc->spi_data.minSyncFactor;
		if (ioc->spi_data.nvram && (ioc->spi_data.nvram[id] != MPT_HOST_NVRAM_INVALID)) {
			nvram = ioc->spi_data.nvram[id];

			if (maxwidth)
				maxwidth = nvram & MPT_NVRAM_WIDE_DISABLE ? 0 : 1;

			if (maxoffset > 0) {
				maxfactor = (nvram & MPT_NVRAM_SYNC_MASK) >> 8;
				if (maxfactor == 0) {
					/* Key for async */
					maxfactor = MPT_ASYNC;
					maxoffset = 0;
				} else if (maxfactor < ioc->spi_data.minSyncFactor) {
					maxfactor = ioc->spi_data.minSyncFactor;
				}
			} else
				maxfactor = MPT_ASYNC;
		}

		/* Set the negotiation flags.
		 */
		negoFlags = ioc->spi_data.noQas;
		if (!maxwidth)
			negoFlags |= MPT_TARGET_NO_NEGO_WIDE;

		if (!maxoffset)
			negoFlags |= MPT_TARGET_NO_NEGO_SYNC;

		if (flags & MPT_SCSICFG_USE_NVRAM) {
			width = maxwidth;
			factor = maxfactor;
			offset = maxoffset;
		} else {
			width = 0;
			factor = MPT_ASYNC;
			offset = 0;
			//negoFlags = 0;
			//negoFlags = MPT_TARGET_NO_NEGO_SYNC;
		}

#ifndef MPTSCSIH_DISABLE_DOMAIN_VALIDATION
		/* Force to async and narrow if DV has not been executed
		 * for this ID
		 */
		if ((hd->ioc->spi_data.dvStatus[id] & MPT_SCSICFG_DV_NOT_DONE) != 0) {
			width = 0;
			factor = MPT_ASYNC;
			offset = 0;
		}
#endif

		/* If id is not a raid volume, get the updated
		 * transmission settings from the target structure.
		 */
		if (hd->Targets && (pTarget = hd->Targets[id]) && !pTarget->raidVolume) {
			width = pTarget->maxWidth;
			factor = pTarget->minSyncFactor;
			offset = pTarget->maxOffset;
			negoFlags = pTarget->negoFlags;
		}

		if (flags & MPT_SCSICFG_BLK_NEGO)
			negoFlags = MPT_TARGET_NO_NEGO_WIDE | MPT_TARGET_NO_NEGO_SYNC;

		mptscsih_setDevicePage1Flags(width, factor, offset,
					&requested, &configuration, negoFlags);

		/* Get a MF for this command.
		 */
		if ((mf = mpt_get_msg_frame(ScsiDoneCtx, ioc->id)) == NULL) {
			dprintk((MYIOC_s_WARN_FMT "write SDP1: no msg frames!\n",
						ioc->name));
			return -EAGAIN;
		}

		ddvprintk((MYIOC_s_INFO_FMT "WriteSDP1 (mf=%p, id=%d, req=0x%x, cfg=0x%x)\n",
			hd->ioc->name, mf, id, requested, configuration));


		/* Set the request and the data pointers.
		 * Request takes: 36 bytes (32 bit SGE)
		 * SCSI Device Page 1 requires 16 bytes
		 * 40 + 16 <= size of SCSI IO Request = 56 bytes
		 * and MF size >= 64 bytes.
		 * Place data at end of MF.
		 */
		pReq = (Config_t *)mf;

		req_idx = le16_to_cpu(mf->u.frame.hwhdr.msgctxu.fld.req_idx);
		frameOffset = ioc->req_sz - sizeof(SCSIDevicePage1_t);

		pData = (SCSIDevicePage1_t *)((u8 *) mf + frameOffset);
		dataDma = ioc->req_frames_dma + (req_idx * ioc->req_sz) + frameOffset;

		/* Complete the request frame (same for all requests).
		 */
		pReq->Action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
		pReq->Reserved = 0;
		pReq->ChainOffset = 0;
		pReq->Function = MPI_FUNCTION_CONFIG;
		pReq->Reserved1[0] = 0;
		pReq->Reserved1[1] = 0;
		pReq->Reserved1[2] = 0;
		pReq->MsgFlags = 0;
		for (ii=0; ii < 8; ii++) {
			pReq->Reserved2[ii] = 0;
		}
		pReq->Header.PageVersion = ioc->spi_data.sdp1version;
		pReq->Header.PageLength = ioc->spi_data.sdp1length;
		pReq->Header.PageNumber = 1;
		pReq->Header.PageType = MPI_CONFIG_PAGETYPE_SCSI_DEVICE;
		pReq->PageAddress = cpu_to_le32(id | (bus << 8 ));

		/* Add a SGE to the config request.
		 */
		flagsLength = MPT_SGE_FLAGS_SSIMPLE_WRITE | ioc->spi_data.sdp1length * 4;

		mpt_add_sge((char *)&pReq->PageBufferSGE, flagsLength, dataDma);

		/* Set up the common data portion
		 */
		pData->Header.PageVersion = pReq->Header.PageVersion;
		pData->Header.PageLength = pReq->Header.PageLength;
		pData->Header.PageNumber = pReq->Header.PageNumber;
		pData->Header.PageType = pReq->Header.PageType;
		pData->RequestedParameters = cpu_to_le32(requested);
		pData->Reserved = 0;
		pData->Configuration = cpu_to_le32(configuration);

		dprintk((MYIOC_s_INFO_FMT
			"write SDP1: id %d pgaddr 0x%x req 0x%x config 0x%x\n",
				ioc->name, id, (id | (bus<<8)),
				requested, configuration));

		mptscsih_put_msgframe(ScsiDoneCtx, ioc->id, mf);
	}

	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*	mptscsih_writeIOCPage4  - write IOC Page 4
 *	@hd: Pointer to a SCSI Host Structure
 *	@target_id: write IOC Page4 for this ID & Bus
 *
 *	Return: -EAGAIN if unable to obtain a Message Frame
 *		or 0 if success.
 *
 *	Remark: We do not wait for a return, write pages sequentially.
 */
static int
mptscsih_writeIOCPage4(MPT_SCSI_HOST *hd, int target_id, int bus)
{
	MPT_ADAPTER		*ioc = hd->ioc;
	Config_t		*pReq;
	IOCPage4_t		*IOCPage4Ptr;
	MPT_FRAME_HDR		*mf;
	dma_addr_t		 dataDma;
	u16			 req_idx;
	u32			 frameOffset;
	u32			 flagsLength;
	int			 ii;

	/* Get a MF for this command.
	 */
	if ((mf = mpt_get_msg_frame(ScsiDoneCtx, ioc->id)) == NULL) {
		dprintk((MYIOC_s_WARN_FMT "writeIOCPage4 : no msg frames!\n",
					ioc->name));
		return -EAGAIN;
	}

	ddvprintk((MYIOC_s_INFO_FMT "writeIOCPage4 (mf=%p, id=%d)\n",
		ioc->name, mf, target_id));

	/* Set the request and the data pointers.
	 * Place data at end of MF.
	 */
	pReq = (Config_t *)mf;

	req_idx = le16_to_cpu(mf->u.frame.hwhdr.msgctxu.fld.req_idx);
	frameOffset = ioc->req_sz - sizeof(IOCPage4_t);

	/* Complete the request frame (same for all requests).
	 */
	pReq->Action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	pReq->Reserved = 0;
	pReq->ChainOffset = 0;
	pReq->Function = MPI_FUNCTION_CONFIG;
	pReq->Reserved1[0] = 0;
	pReq->Reserved1[1] = 0;
	pReq->Reserved1[2] = 0;
	pReq->MsgFlags = 0;
	for (ii=0; ii < 8; ii++) {
		pReq->Reserved2[ii] = 0;
	}

       	IOCPage4Ptr = ioc->spi_data.pIocPg4;
       	dataDma = ioc->spi_data.IocPg4_dma;
       	ii = IOCPage4Ptr->ActiveSEP++;
       	IOCPage4Ptr->SEP[ii].SEPTargetID = target_id;
       	IOCPage4Ptr->SEP[ii].SEPBus = bus;
       	pReq->Header = IOCPage4Ptr->Header;
	pReq->PageAddress = cpu_to_le32(target_id | (bus << 8 ));

	/* Add a SGE to the config request.
	 */
	flagsLength = MPT_SGE_FLAGS_SSIMPLE_WRITE | 
		(IOCPage4Ptr->Header.PageLength + ii) * 4;

	mpt_add_sge((char *)&pReq->PageBufferSGE, flagsLength, dataDma);

	dsprintk((MYIOC_s_INFO_FMT
		"writeIOCPage4: pgaddr 0x%x\n",
			ioc->name, (target_id | (bus<<8))));

	mptscsih_put_msgframe(ScsiDoneCtx, ioc->id, mf);

	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*	mptscsih_taskmgmt_timeout - Call back for timeout on a
 *	task management request.
 *	@data: Pointer to MPT_SCSI_HOST recast as an unsigned long
 *
 */
static void mptscsih_taskmgmt_timeout(unsigned long data)
{
	MPT_SCSI_HOST *hd = (MPT_SCSI_HOST *) data;

	dtmprintk((KERN_WARNING MYNAM ": %s: mptscsih_taskmgmt_timeout: "
		   "TM request timed out!\n", hd->ioc->name));

	/* Delete the timer that triggered this callback.
	 * Remark: del_timer checks to make sure timer is active
	 * before deleting.
	 */
	del_timer(&hd->TMtimer);

	/* Call the reset handler. Already had a TM request
	 * timeout - so issue a diagnostic reset
	 */
	if (mpt_HardResetHandler(hd->ioc, NO_SLEEP) < 0) {
		printk((KERN_WARNING " Firmware Reload FAILED!!\n"));
	}
#ifdef MPT_SCSI_USE_NEW_EH
	else {
		/* Because we have reset the IOC, no TM requests can be
		 * pending.  So let's make sure the tmPending flag is reset.
		 */
		nehprintk((KERN_WARNING MYNAM
			   ": %s: mptscsih_taskmgmt_timeout\n",
			   hd->ioc->name));
		hd->tmPending = 0;
	}
#endif

	return;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Bus Scan and Domain Validation functionality ...
 */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *	mptscsih_scandv_complete - Scan and DV callback routine registered
 *	to Fustion MPT (base) driver.
 *
 *	@ioc: Pointer to MPT_ADAPTER structure
 *	@mf: Pointer to original MPT request frame
 *	@mr: Pointer to MPT reply frame (NULL if TurboReply)
 *
 *	This routine is called from mpt.c::mpt_interrupt() at the completion
 *	of any SCSI IO request.
 *	This routine is registered with the Fusion MPT (base) driver at driver
 *	load/init time via the mpt_register() API call.
 *
 *	Returns 1 indicating alloc'd request frame ptr should be freed.
 *
 *	Remark: Sets a completion code and (possibly) saves sense data
 *	in the IOC member localReply structure.
 *	Used ONLY for DV and other internal commands.
 */
static int
mptscsih_scandv_complete(MPT_ADAPTER *ioc, MPT_FRAME_HDR *mf, MPT_FRAME_HDR *mr)
{
	MPT_SCSI_HOST	*hd;
	SCSIIORequest_t *pReq;
	int		 completionCode;
	u16		 req_idx;

	if ((mf == NULL) ||
	    (mf >= MPT_INDEX_2_MFPTR(ioc, ioc->req_depth))) {
		printk(MYIOC_s_ERR_FMT
			"ScanDvComplete, %s req frame ptr! (=%p)\n",
				ioc->name, mf?"BAD":"NULL", (void *) mf);
		goto wakeup;
	}

	hd = (MPT_SCSI_HOST *) ioc->sh->hostdata;
	del_timer(&hd->timer);
	req_idx = le16_to_cpu(mf->u.frame.hwhdr.msgctxu.fld.req_idx);
	hd->ScsiLookup[req_idx] = NULL;
	pReq = (SCSIIORequest_t *) mf;

	if (mf != hd->cmdPtr) {
		printk(MYIOC_s_WARN_FMT "ScanDvComplete (mf=%p, cmdPtr=%p, idx=%d)\n",
				hd->ioc->name, (void *)mf, (void *) hd->cmdPtr, req_idx);
	}
	hd->cmdPtr = NULL;

	ddvprintk((MYIOC_s_INFO_FMT "ScanDvComplete (mf=%p,mr=%p,idx=%d)\n",
			hd->ioc->name, mf, mr, req_idx));

	atomic_dec(&queue_depth);

	hd->pLocal = &hd->localReply;
	hd->pLocal->scsiStatus = 0;

	/* If target struct exists, clear sense valid flag.
	 */
	if (mr == NULL) {
		completionCode = MPT_SCANDV_GOOD;
	} else {
		SCSIIOReply_t	*pReply;
		u16		 status;

		pReply = (SCSIIOReply_t *) mr;

		status = le16_to_cpu(pReply->IOCStatus) & MPI_IOCSTATUS_MASK;

		ddvtprintk((KERN_NOTICE "  IOCStatus=%04xh, SCSIState=%02xh, SCSIStatus=%02xh, IOCLogInfo=%08xh\n",
			     status, pReply->SCSIState, pReply->SCSIStatus,
			     le32_to_cpu(pReply->IOCLogInfo)));

		switch(status) {

		case MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE:	/* 0x0043 */
			completionCode = MPT_SCANDV_SELECTION_TIMEOUT;
			break;

		case MPI_IOCSTATUS_SCSI_IO_DATA_ERROR:		/* 0x0046 */
		case MPI_IOCSTATUS_SCSI_TASK_TERMINATED:	/* 0x0048 */
		case MPI_IOCSTATUS_SCSI_IOC_TERMINATED:		/* 0x004B */
		case MPI_IOCSTATUS_SCSI_EXT_TERMINATED:		/* 0x004C */
			completionCode = MPT_SCANDV_DID_RESET;
			break;

		case MPI_IOCSTATUS_SCSI_DATA_UNDERRUN:		/* 0x0045 */
		case MPI_IOCSTATUS_SCSI_RECOVERED_ERROR:	/* 0x0040 */
		case MPI_IOCSTATUS_SUCCESS:			/* 0x0000 */
			if (pReply->Function == MPI_FUNCTION_CONFIG) {
				ConfigReply_t *pr = (ConfigReply_t *)mr;
				completionCode = MPT_SCANDV_GOOD;
				hd->pLocal->header.PageVersion = pr->Header.PageVersion;
				hd->pLocal->header.PageLength = pr->Header.PageLength;
				hd->pLocal->header.PageNumber = pr->Header.PageNumber;
				hd->pLocal->header.PageType = pr->Header.PageType;

			} else if (pReply->Function == MPI_FUNCTION_RAID_ACTION) {
				/* If the RAID Volume request is successful,
				 * return GOOD, else indicate that
				 * some type of error occurred.
				 */
				MpiRaidActionReply_t	*pr = (MpiRaidActionReply_t *)mr;
				if (pr->ActionStatus == MPI_RAID_ACTION_ASTATUS_SUCCESS)
					completionCode = MPT_SCANDV_GOOD;
				else
					completionCode = MPT_SCANDV_SOME_ERROR;

			} else if (pReply->SCSIState & MPI_SCSI_STATE_AUTOSENSE_VALID) {
				u8		*sense_data;
				int		 sz;

				/* save sense data in global structure
				 */
				completionCode = MPT_SCANDV_SENSE;
				hd->pLocal->scsiStatus = pReply->SCSIStatus;
				sense_data = ((u8 *)hd->ioc->sense_buf_pool +
					(req_idx * MPT_SENSE_BUFFER_ALLOC));

				sz = MIN (pReq->SenseBufferLength,
							SCSI_STD_SENSE_BYTES);
				memcpy(hd->pLocal->sense, sense_data, sz);

				ddvprintk((KERN_NOTICE "  Check Condition, sense ptr %p\n",
						sense_data));
			} else if (pReply->SCSIState & MPI_SCSI_STATE_AUTOSENSE_FAILED) {
				if (pReq->CDB[0] == CMD_Inquiry)
					completionCode = MPT_SCANDV_ISSUE_SENSE;
				else
					completionCode = MPT_SCANDV_DID_RESET;
			}
			else if (pReply->SCSIState & MPI_SCSI_STATE_NO_SCSI_STATUS)
				completionCode = MPT_SCANDV_DID_RESET;
			else if (pReply->SCSIState & MPI_SCSI_STATE_TERMINATED)
				completionCode = MPT_SCANDV_DID_RESET;
			else {
				/* If no error, this will be equivalent
				 * to MPT_SCANDV_GOOD
				 */
				completionCode = MPT_SCANDV_GOOD;
				hd->pLocal->scsiStatus = pReply->SCSIStatus;
			}
			break;

		case MPI_IOCSTATUS_SCSI_PROTOCOL_ERROR:		/* 0x0047 */
			if (pReply->SCSIState & MPI_SCSI_STATE_TERMINATED)
				completionCode = MPT_SCANDV_DID_RESET;
			else
				completionCode = MPT_SCANDV_SOME_ERROR;
			break;

		default:
			completionCode = MPT_SCANDV_SOME_ERROR;
			break;

		}	/* switch(status) */

		ddvtprintk((KERN_NOTICE "  completionCode set to %08xh\n",
				completionCode));
	} /* end of address reply case */

	hd->pLocal->completion = completionCode;

	/* MF and RF are freed in mpt_interrupt
	 */
wakeup:
	/* Free Chain buffers (will never chain) in scan or dv */
	//mptscsih_freeChainBuffers(hd, req_idx);

	/*
	 * Wake up the original calling thread
	 */
	scandv_wait_done = 1;
	wake_up(&scandv_waitq);

	return 1;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*	mptscsih_timer_expired - Call back for timer process.
 *	Used only for dv functionality.
 *	@data: Pointer to MPT_SCSI_HOST recast as an unsigned long
 *
 */
static void mptscsih_timer_expired(unsigned long data)
{
	MPT_SCSI_HOST *hd = (MPT_SCSI_HOST *) data;
#ifndef MPT_SCSI_USE_NEW_EH
	unsigned long  flags;
#endif


	ddvprintk((MYIOC_s_WARN_FMT "Timer Expired! Cmd %p\n", hd->ioc->name, hd->cmdPtr));

	if (hd->cmdPtr) {
		MPIHeader_t *cmd = (MPIHeader_t *)hd->cmdPtr;

		if (cmd->Function == MPI_FUNCTION_SCSI_IO_REQUEST) {
			/* Desire to issue a task management request here.
			 * TM requests MUST be single threaded.
			 * If old eh code and no TM current, issue request.
			 * If new eh code, do nothing. Wait for OS cmd timeout
			 *	for bus reset.
			 */
#ifndef MPT_SCSI_USE_NEW_EH
			SCSIIORequest_t	*pReq = (SCSIIORequest_t *) hd->cmdPtr;

			spin_lock_irqsave(&hd->ioc->FreeQlock, flags);
			if (hd->tmPending) {
				spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);
				return;
			} else
				hd->tmPending = 1;
			spin_unlock_irqrestore(&hd->ioc->FreeQlock, flags);
			if (mptscsih_TMHandler(hd, MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS,
							pReq->Bus, 0, 0, 0, NO_SLEEP) < 0) {
				printk(MYIOC_s_WARN_FMT "TM FAILED!\n", hd->ioc->name);
			}
#else
			ddvtprintk((MYIOC_s_NOTE_FMT "DV Cmd Timeout: NoOp\n", hd->ioc->name));
#endif
		} else {
			/* Perform a FW reload */
			if (mpt_HardResetHandler(hd->ioc, NO_SLEEP) < 0) {
				printk(MYIOC_s_WARN_FMT "Firmware Reload FAILED!\n", hd->ioc->name);
			}
		}
	} else {
		/* This should NEVER happen */
		printk(MYIOC_s_WARN_FMT "Null cmdPtr!!!!\n", hd->ioc->name);
	}

	/* No more processing.
	 * TM call will generate an interrupt for SCSI TM Management.
	 * The FW will reply to all outstanding commands, callback will finish cleanup.
	 * Hard reset clean-up will free all resources.
	 */
	ddvprintk((MYIOC_s_WARN_FMT "Timer Expired Complete!\n", hd->ioc->name));

	return;
}

#ifndef MPTSCSIH_DISABLE_DOMAIN_VALIDATION
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*	mptscsih_do_raid - Format and Issue a RAID volume request message.
 *	@hd: Pointer to scsi host structure
 *	@action: What do be done.
 *	@id: Logical target id.
 *	@bus: Target locations bus.
 *
 *	Returns: < 0 on a fatal error
 *		0 on success
 *
 *	Remark: Wait to return until reply processed by the ISR.
 */
static int
mptscsih_do_raid(MPT_SCSI_HOST *hd, u8 action, INTERNAL_CMD *io)
{
	MpiRaidActionRequest_t	*pReq;
	MPT_FRAME_HDR		*mf;
	int			in_isr;

	in_isr = in_interrupt();
	if (in_isr) {
		dprintk((MYIOC_s_WARN_FMT "Internal raid request not allowed in ISR context!\n",
       				hd->ioc->name));
		return -EPERM;
	}

	/* Get and Populate a free Frame
	 */
	if ((mf = mpt_get_msg_frame(ScsiScanDvCtx, hd->ioc->id)) == NULL) {
		ddvprintk((MYIOC_s_WARN_FMT "_do_raid: no msg frames!\n",
					hd->ioc->name));
		return -EAGAIN;
	}
	pReq = (MpiRaidActionRequest_t *)mf;
	pReq->Action = action;
	pReq->Reserved1 = 0;
	pReq->ChainOffset = 0;
	pReq->Function = MPI_FUNCTION_RAID_ACTION;
	pReq->VolumeID = io->id;
	pReq->VolumeBus = io->bus;
	pReq->PhysDiskNum = io->physDiskNum;
	pReq->MsgFlags = 0;
	pReq->Reserved2 = 0;
	pReq->ActionDataWord = 0; /* Reserved for this action */
	//pReq->ActionDataSGE = 0;

	mpt_add_sge((char *)&pReq->ActionDataSGE,
		MPT_SGE_FLAGS_SSIMPLE_READ | 0, (dma_addr_t) -1);

	ddvprintk((MYIOC_s_INFO_FMT "RAID Volume action %x id %d\n",
			hd->ioc->name, action, io->id));

	hd->pLocal = NULL;
	hd->timer.expires = jiffies + HZ*2; /* 2 second timeout */
	scandv_wait_done = 0;

	/* Save cmd pointer, for resource free if timeout or
	 * FW reload occurs
	 */
	hd->cmdPtr = mf;

	add_timer(&hd->timer);
	mptscsih_put_msgframe(ScsiScanDvCtx, hd->ioc->id, mf);
	wait_event(scandv_waitq, scandv_wait_done);

	if ((hd->pLocal == NULL) || (hd->pLocal->completion != MPT_SCANDV_GOOD))
		return -1;

	return 0;
}
#endif /* ~MPTSCSIH_DISABLE_DOMAIN_VALIDATION */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_do_cmd - Do internal command.
 *	@hd: MPT_SCSI_HOST pointer
 *	@io: INTERNAL_CMD pointer.
 *
 *	Issue the specified internally generated command and do command
 *	specific cleanup. For bus scan / DV only.
 *	NOTES: If command is Inquiry and status is good,
 *	initialize a target structure, save the data
 *
 *	Remark: Single threaded access only.
 *
 *	Return:
 *		< 0 if an illegal command or no resources
 *
 *		   0 if good
 *
 *		 > 0 if command complete but some type of completion error.
 */
static int
mptscsih_do_cmd(MPT_SCSI_HOST *hd, INTERNAL_CMD *io)
{
	MPT_FRAME_HDR	*mf;
	SCSIIORequest_t	*pScsiReq;
	SCSIIORequest_t	 ReqCopy;
	int		 my_idx, ii, dir;
	int		 rc, cmdTimeout;
	int		in_isr;
	char		 cmdLen;
	char		 CDB[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	char		 cmd = io->cmd;

	in_isr = in_interrupt();
	if (in_isr) {
		dprintk((MYIOC_s_WARN_FMT "Internal SCSI IO request not allowed in ISR context!\n",
       				hd->ioc->name));
		return -EPERM;
	}


	/* Set command specific information
	 */
	switch (cmd) {
	case CMD_Inquiry:
		cmdLen = 6;
		dir = MPI_SCSIIO_CONTROL_READ;
		CDB[0] = cmd;
		CDB[4] = io->size;
		cmdTimeout = 10;
		break;

	case CMD_TestUnitReady:
		cmdLen = 6;
		dir = MPI_SCSIIO_CONTROL_READ;
		cmdTimeout = 10;
		break;

	case CMD_StartStopUnit:
		cmdLen = 6;
		dir = MPI_SCSIIO_CONTROL_READ;
		CDB[0] = cmd;
		CDB[4] = 1;	/*Spin up the disk */
		cmdTimeout = 15;
		break;

	case CMD_RequestSense:
		cmdLen = 6;
		CDB[0] = cmd;
		CDB[4] = io->size;
		dir = MPI_SCSIIO_CONTROL_READ;
		cmdTimeout = 10;
		break;

	case CMD_ReadBuffer:
		cmdLen = 10;
		dir = MPI_SCSIIO_CONTROL_READ;
		CDB[0] = cmd;
		if (io->flags & MPT_ICFLAG_ECHO) {
			CDB[1] = 0x0A;
		} else {
			CDB[1] = 0x02;
		}

		if (io->flags & MPT_ICFLAG_BUF_CAP) {
			CDB[1] |= 0x01;
		}
		CDB[6] = (io->size >> 16) & 0xFF;
		CDB[7] = (io->size >>  8) & 0xFF;
		CDB[8] = io->size & 0xFF;
		cmdTimeout = 10;
		break;

	case CMD_WriteBuffer:
		cmdLen = 10;
		dir = MPI_SCSIIO_CONTROL_WRITE;
		CDB[0] = cmd;
		if (io->flags & MPT_ICFLAG_ECHO) {
			CDB[1] = 0x0A;
		} else {
			CDB[1] = 0x02;
		}
		CDB[6] = (io->size >> 16) & 0xFF;
		CDB[7] = (io->size >>  8) & 0xFF;
		CDB[8] = io->size & 0xFF;
		cmdTimeout = 10;
		break;

	case CMD_Reserve6:
		cmdLen = 6;
		dir = MPI_SCSIIO_CONTROL_READ;
		CDB[0] = cmd;
		cmdTimeout = 10;
		break;

	case CMD_Release6:
		cmdLen = 6;
		dir = MPI_SCSIIO_CONTROL_READ;
		CDB[0] = cmd;
		cmdTimeout = 10;
		break;

	case CMD_SynchronizeCache:
		cmdLen = 10;
		dir = MPI_SCSIIO_CONTROL_READ;
		CDB[0] = cmd;
//		CDB[1] = 0x02;	/* set immediate bit */
		cmdTimeout = 10;
		break;

	default:
		/* Error Case */
		return -EFAULT;
	}

	/* Get and Populate a free Frame
	 */
	if ((mf = mpt_get_msg_frame(ScsiScanDvCtx, hd->ioc->id)) == NULL) {
		ddvprintk((MYIOC_s_WARN_FMT "No msg frames!\n",
					hd->ioc->name));
		return -EBUSY;
	}

	pScsiReq = (SCSIIORequest_t *) mf;

	/* Get the request index */
	my_idx = le16_to_cpu(mf->u.frame.hwhdr.msgctxu.fld.req_idx);
	ADD_INDEX_LOG(my_idx); /* for debug */

	if (io->flags & MPT_ICFLAG_PHYS_DISK) {
		pScsiReq->TargetID = io->physDiskNum;
		pScsiReq->Bus = 0;
		pScsiReq->ChainOffset = 0;
		pScsiReq->Function = MPI_FUNCTION_RAID_SCSI_IO_PASSTHROUGH;
	} else {
		pScsiReq->TargetID = io->id;
		pScsiReq->Bus = io->bus;
		pScsiReq->ChainOffset = 0;
		pScsiReq->Function = MPI_FUNCTION_SCSI_IO_REQUEST;
	}

	pScsiReq->CDBLength = cmdLen;
	pScsiReq->SenseBufferLength = MPT_SENSE_BUFFER_SIZE;

	pScsiReq->Reserved = 0;

	pScsiReq->MsgFlags = mpt_msg_flags();
	/* MsgContext set in mpt_get_msg_fram call  */

	for (ii=0; ii < 8; ii++)
		pScsiReq->LUN[ii] = 0;
	pScsiReq->LUN[1] = io->lun;

	if (io->flags & MPT_ICFLAG_TAGGED_CMD)
		pScsiReq->Control = cpu_to_le32(dir | MPI_SCSIIO_CONTROL_SIMPLEQ);
	else
		pScsiReq->Control = cpu_to_le32(dir | MPI_SCSIIO_CONTROL_UNTAGGED);

	if (cmd == CMD_RequestSense) {
		pScsiReq->Control = cpu_to_le32(dir | MPI_SCSIIO_CONTROL_UNTAGGED);
		ddvprintk((MYIOC_s_INFO_FMT "Untagged! 0x%2x\n",
			hd->ioc->name, cmd));
	}

	for (ii=0; ii < 16; ii++)
		pScsiReq->CDB[ii] = CDB[ii];

	pScsiReq->DataLength = cpu_to_le32(io->size);
	pScsiReq->SenseBufferLowAddr = cpu_to_le32(hd->ioc->sense_buf_low_dma
					   + (my_idx * MPT_SENSE_BUFFER_ALLOC));

	ddvprintk((MYIOC_s_INFO_FMT "Sending Command 0x%x for (%d:%d:%d)\n",
			hd->ioc->name, cmd, io->bus, io->id, io->lun));

	if (dir == MPI_SCSIIO_CONTROL_READ) {
		mpt_add_sge((char *) &pScsiReq->SGL,
			MPT_SGE_FLAGS_SSIMPLE_READ | io->size,
			io->data_dma);
	} else {
		mpt_add_sge((char *) &pScsiReq->SGL,
			MPT_SGE_FLAGS_SSIMPLE_WRITE | io->size,
			io->data_dma);
	}

	/* The ISR will free the request frame, but we need
	 * the information to initialize the target. Duplicate.
	 */
	memcpy(&ReqCopy, pScsiReq, sizeof(SCSIIORequest_t));

	/* Issue this command after:
	 *	finish init
	 *	add timer
	 * Wait until the reply has been received
	 *  ScsiScanDvCtx callback function will
	 *	set hd->pLocal;
	 *	set scandv_wait_done and call wake_up
	 */
	hd->pLocal = NULL;
	hd->timer.expires = jiffies + HZ*cmdTimeout;
	scandv_wait_done = 0;

	/* Save cmd pointer, for resource free if timeout or
	 * FW reload occurs
	 */
	hd->cmdPtr = mf;

	add_timer(&hd->timer);
	mptscsih_put_msgframe(ScsiScanDvCtx, hd->ioc->id, mf);
	wait_event(scandv_waitq, scandv_wait_done);

	if (hd->pLocal) {
		rc = hd->pLocal->completion;
		hd->pLocal->skip = 0;

		/* Always set fatal error codes in some cases.
		 */
		if (rc == MPT_SCANDV_SELECTION_TIMEOUT)
			rc = -ENXIO;
		else if (rc == MPT_SCANDV_SOME_ERROR)
			rc =  -rc;
	} else {
		rc = -EFAULT;
		/* This should never happen. */
		ddvprintk((MYIOC_s_INFO_FMT "_do_cmd: Null pLocal!!!\n",
				hd->ioc->name));
	}

	return rc;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_synchronize_cache - Send SYNCHRONIZE_CACHE to all disks.
 *	@hd: Pointer to MPT_SCSI_HOST structure
 *	@portnum: IOC port number
 *
 *	Uses the ISR, but with special processing.
 *	MUST be single-threaded.
 *
 *	Return: 0 on completion
 */
static int
mptscsih_synchronize_cache(MPT_SCSI_HOST *hd, int portnum)
{
	MPT_ADAPTER		*ioc= hd->ioc;
	VirtDevice		*pTarget;
	SCSIDevicePage1_t	*pcfg1Data = NULL;
	INTERNAL_CMD		 iocmd;
	CONFIGPARMS		 cfg;
	dma_addr_t		 cfg1_dma_addr = -1;
	ConfigPageHeader_t	 header1;
	int			 bus = 0;
	int			 id = 0;
	int			 lun;
	int			 indexed_lun, lun_index;
	int			 hostId = ioc->pfacts[portnum].PortSCSIID;
	int			 max_id;
	int			 requested, configuration, data;
	int			 doConfig = 0;
	u8			 flags, factor;

	max_id = ioc->sh->max_id - 1;

	/* Following parameters will not change
	 * in this routine.
	 */
	iocmd.cmd = CMD_SynchronizeCache;
	iocmd.flags = 0;
	iocmd.physDiskNum = -1;
	iocmd.data = NULL;
	iocmd.data_dma = -1;
	iocmd.size = 0;
	iocmd.rsvd = iocmd.rsvd2 = 0;

	/* No SCSI hosts
	 */
	if (hd->Targets == NULL)
		return 0;

	/* Skip the host
	 */
	if (id == hostId)
		id++;

	/* Write SDP1 for all SCSI devices
	 * Alloc memory and set up config buffer
	 */
	if (hd->is_spi) {
		if (ioc->spi_data.sdp1length > 0) {
			pcfg1Data = (SCSIDevicePage1_t *)pci_alloc_consistent(ioc->pcidev,
					 ioc->spi_data.sdp1length * 4, &cfg1_dma_addr);
	
			if (pcfg1Data != NULL) {
				doConfig = 1;
				header1.PageVersion = ioc->spi_data.sdp1version;
				header1.PageLength = ioc->spi_data.sdp1length;
				header1.PageNumber = 1;
				header1.PageType = MPI_CONFIG_PAGETYPE_SCSI_DEVICE;
				cfg.hdr = &header1;
				cfg.physAddr = cfg1_dma_addr;
				cfg.action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
				cfg.dir = 1;
				cfg.timeout = 0;
			}
		}
	}

	/* loop through all devices on this port
	 */
	while (bus < MPT_MAX_BUS) {
		iocmd.bus = bus;
		iocmd.id = id;
		pTarget = hd->Targets[(int)id];

		if (doConfig) {

			/* Set the negotiation flags */
			if (pTarget && (pTarget = hd->Targets[id]) && !pTarget->raidVolume) {
				flags = pTarget->negoFlags;
			} else {
				flags = hd->ioc->spi_data.noQas;
				if (hd->ioc->spi_data.nvram && (hd->ioc->spi_data.nvram[id] != MPT_HOST_NVRAM_INVALID)) {
					data = hd->ioc->spi_data.nvram[id];
	
					if (data & MPT_NVRAM_WIDE_DISABLE)
						flags |= MPT_TARGET_NO_NEGO_WIDE;

					factor = (data & MPT_NVRAM_SYNC_MASK) >> MPT_NVRAM_SYNC_SHIFT;
					if ((factor == 0) || (factor == MPT_ASYNC))
						flags |= MPT_TARGET_NO_NEGO_SYNC;
				}
			}
	
			/* Force to async, narrow */
			mptscsih_setDevicePage1Flags(0, MPT_ASYNC, 0, &requested,
					&configuration, flags);
			pcfg1Data->RequestedParameters = le32_to_cpu(requested);
			pcfg1Data->Reserved = 0;
			pcfg1Data->Configuration = le32_to_cpu(configuration);
			cfg.pageAddr = (bus<<8) | id;
			mpt_config(hd->ioc, &cfg);
		}

		/* If target Ptr NULL or if this target is NOT a disk, skip.
		 */
		if ((pTarget) && (pTarget->tflags & MPT_TARGET_FLAGS_Q_YES)){
			for (lun=0; lun <= MPT_LAST_LUN; lun++) {
				/* If LUN present, issue the command
				 */
				lun_index = (lun >> 5);  /* 32 luns per lun_index */
				indexed_lun = (lun % 32);
				if (pTarget->luns[lun_index] & (1<<indexed_lun)) {
					iocmd.lun = lun;
					(void) mptscsih_do_cmd(hd, &iocmd);
				}
			}
		}

		/* get next relevant device */
		id++;

		if (id == hostId)
			id++;

		if (id > max_id) {
			id = 0;
			bus++;
		}
	}

	if (pcfg1Data) {
		pci_free_consistent(ioc->pcidev, header1.PageLength * 4, pcfg1Data, cfg1_dma_addr);
	}

	return 0;
}

#ifndef MPTSCSIH_DISABLE_DOMAIN_VALIDATION
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_domainValidation - Top level handler for domain validation.
 *	@hd: Pointer to MPT_SCSI_HOST structure.
 *
 *	Uses the ISR, but with special processing.
 *	Called from schedule, should not be in interrupt mode.
 *	While thread alive, do dv for all devices needing dv
 *
 *	Return: None.
 */
static void
mptscsih_domainValidation(void *arg)
{
	MPT_SCSI_HOST		*hd;
	MPT_ADAPTER		*ioc;
	unsigned long		 flags;
	int 			 id, maxid, dvStatus, did;
	int			 ii, isPhysDisk;

	spin_lock_irqsave(&dvtaskQ_lock, flags);
	dvtaskQ_active = 1;
	if (dvtaskQ_release) {
		dvtaskQ_active = 0;
		spin_unlock_irqrestore(&dvtaskQ_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&dvtaskQ_lock, flags);

	/* For this ioc, loop through all devices and do dv to each device.
	 * When complete with this ioc, search through the ioc list, and
	 * for each scsi ioc found, do dv for all devices. Exit when no
	 * device needs dv.
	 */
	did = 1;
	while (did) {
		did = 0;
		for (ioc = mpt_adapter_find_first(); ioc != NULL; ioc = mpt_adapter_find_next(ioc)) {
			spin_lock_irqsave(&dvtaskQ_lock, flags);
			if (dvtaskQ_release) {
				dvtaskQ_active = 0;
				spin_unlock_irqrestore(&dvtaskQ_lock, flags);
				return;
			}
			spin_unlock_irqrestore(&dvtaskQ_lock, flags);

			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ/4);

			/* DV only to SCSI adapters */
			if ((int)ioc->chip_type <= (int)FC929)
				continue;
			
			/* Make sure everything looks ok */
			if (ioc->sh == NULL)
				continue;

			hd = (MPT_SCSI_HOST *) ioc->sh->hostdata;
			if (hd == NULL)
				continue;

			if ((ioc->spi_data.forceDv & MPT_SCSICFG_RELOAD_IOC_PG3) != 0) {
				mpt_read_ioc_pg_3(ioc);
				if (ioc->spi_data.pIocPg3) {
					Ioc3PhysDisk_t *pPDisk = ioc->spi_data.pIocPg3->PhysDisk;
					int		numPDisk = ioc->spi_data.pIocPg3->NumPhysDisks;

					while (numPDisk) {
						if (ioc->spi_data.dvStatus[pPDisk->PhysDiskID] & MPT_SCSICFG_DV_NOT_DONE)
							ioc->spi_data.dvStatus[pPDisk->PhysDiskID] |= MPT_SCSICFG_NEED_DV;

						pPDisk++;
						numPDisk--;
					}
				}
				ioc->spi_data.forceDv &= ~MPT_SCSICFG_RELOAD_IOC_PG3;
			}

			maxid = MIN (ioc->sh->max_id, MPT_MAX_SCSI_DEVICES);

			for (id = 0; id < maxid; id++) {
				spin_lock_irqsave(&dvtaskQ_lock, flags);
				if (dvtaskQ_release) {
					dvtaskQ_active = 0;
					spin_unlock_irqrestore(&dvtaskQ_lock, flags);
					return;
				}
				spin_unlock_irqrestore(&dvtaskQ_lock, flags);
				dvStatus = hd->ioc->spi_data.dvStatus[id];

				if (dvStatus & MPT_SCSICFG_NEED_DV) {
					did++;
					hd->ioc->spi_data.dvStatus[id] |= MPT_SCSICFG_DV_PENDING;
					hd->ioc->spi_data.dvStatus[id] &= ~MPT_SCSICFG_NEED_DV;

					set_current_state(TASK_INTERRUPTIBLE);
					schedule_timeout(HZ/4);

					/* If hidden phys disk, block IO's to all
					 *	raid volumes
					 * else, process normally
					 */
					isPhysDisk = mptscsih_is_phys_disk(ioc, id);
					if (isPhysDisk) {
						for (ii=0; ii < MPT_MAX_SCSI_DEVICES; ii++) {
							if (hd->ioc->spi_data.isRaid & (1 << ii)) {
								hd->ioc->spi_data.dvStatus[ii] |= MPT_SCSICFG_DV_PENDING;
							}
						}
					}

					if (mptscsih_doDv(hd, 0, id) == 1) {
						/* Untagged device was busy, try again
						 */
						hd->ioc->spi_data.dvStatus[id] |= MPT_SCSICFG_NEED_DV;
						hd->ioc->spi_data.dvStatus[id] &= ~MPT_SCSICFG_DV_PENDING;
					} else {
						/* DV is complete. Clear flags.
						 */
						hd->ioc->spi_data.dvStatus[id] &= ~(MPT_SCSICFG_DV_NOT_DONE | MPT_SCSICFG_DV_PENDING);
					}

					if (isPhysDisk) {
						for (ii=0; ii < MPT_MAX_SCSI_DEVICES; ii++) {
							if (hd->ioc->spi_data.isRaid & (1 << ii)) {
								hd->ioc->spi_data.dvStatus[ii] &= ~MPT_SCSICFG_DV_PENDING;
							}
						}
					}

					/* Post OS IOs that were pended while
					 * DV running.
					 */
					post_pendingQ_commands(hd);

					if (hd->ioc->spi_data.noQas)
						mptscsih_qas_check(hd, id);
				}
			}
		}
	}

	spin_lock_irqsave(&dvtaskQ_lock, flags);
	dvtaskQ_active = 0;
	spin_unlock_irqrestore(&dvtaskQ_lock, flags);

	return;
}

/* Search IOC page 3 to determine if this is hidden physical disk
 */
static int mptscsih_is_phys_disk(MPT_ADAPTER *ioc, int id)
{
	if (ioc->spi_data.pIocPg3) {
		Ioc3PhysDisk_t *pPDisk =  ioc->spi_data.pIocPg3->PhysDisk;
		int		numPDisk = ioc->spi_data.pIocPg3->NumPhysDisks;

		while (numPDisk) {
			if (pPDisk->PhysDiskID == id) {
				return 1;
			}
			pPDisk++;
			numPDisk--;
		}
	}
	return 0;
}

/* Write SDP1 if no QAS has been enabled
 */
static void mptscsih_qas_check(MPT_SCSI_HOST *hd, int id)
{
	VirtDevice *pTarget;
	int ii;

	if (hd->Targets == NULL)
		return;

	for (ii=0; ii < MPT_MAX_SCSI_DEVICES; ii++) {
		if (ii == id)
			continue;

		if ((hd->ioc->spi_data.dvStatus[ii] & MPT_SCSICFG_DV_NOT_DONE) != 0)
			continue;

		pTarget = hd->Targets[ii];

		if ((pTarget != NULL) && (!pTarget->raidVolume)) {
			if ((pTarget->negoFlags & hd->ioc->spi_data.noQas) == 0) {
				pTarget->negoFlags |= hd->ioc->spi_data.noQas;
				mptscsih_writeSDP1(hd, 0, ii, 0);
			}
		} else {
			if (mptscsih_is_phys_disk(hd->ioc, ii) == 1)
				mptscsih_writeSDP1(hd, 0, ii, MPT_SCSICFG_USE_NVRAM);
		}
	}
	return;
}



#define MPT_GET_NVRAM_VALS	0x01
#define MPT_UPDATE_MAX		0x02
#define MPT_SET_MAX		0x04
#define MPT_SET_MIN		0x08
#define MPT_FALLBACK		0x10
#define MPT_SAVE		0x20

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/**
 *	mptscsih_doDv - Perform domain validation to a target.
 *	@hd: Pointer to MPT_SCSI_HOST structure.
 *	@portnum: IOC port number.
 *	@target: Physical ID of this target
 *
 *	Uses the ISR, but with special processing.
 *	MUST be single-threaded.
 *	Test will exit if target is at async & narrow.
 *
 *	Return: None.
 */
static int
mptscsih_doDv(MPT_SCSI_HOST *hd, int bus_number, int id)
{
	MPT_ADAPTER		*ioc = hd->ioc;
	VirtDevice		*pTarget;
	SCSIDevicePage1_t	*pcfg1Data;
	SCSIDevicePage0_t	*pcfg0Data;
	u8			*pbuf1;
	u8			*pbuf2;
	u8			*pDvBuf;
	dma_addr_t		 dvbuf_dma = -1;
	dma_addr_t		 buf1_dma = -1;
	dma_addr_t		 buf2_dma = -1;
	dma_addr_t		 cfg1_dma_addr = -1;
	dma_addr_t		 cfg0_dma_addr = -1;
	ConfigPageHeader_t	 header1;
	ConfigPageHeader_t	 header0;
	DVPARAMETERS		 dv;
	INTERNAL_CMD		 iocmd;
	CONFIGPARMS		 cfg;
	int			 dv_alloc = 0;
	int			 rc, sz = 0;
	int			 bufsize = 0;
	int			 dataBufSize = 0;
	int			 echoBufSize = 0;
	int			 notDone;
	int			 patt;
	int			 repeat;
	int			 retcode = 0;
	int			 nfactor =  MPT_ULTRA320;
	char			 firstPass = 1;
	char			 doFallback = 0;
	char			 readPage0;
	char			 bus, lun;
	char			 inq0 = 0;

	if (ioc->spi_data.sdp1length == 0)
		return 0;

	if (ioc->spi_data.sdp0length == 0)
		return 0;

	/* If multiple buses are used, require that the initiator
	 * id be the same on all buses.
	 */
	if (id == ioc->pfacts[0].PortSCSIID)
		return 0;

	lun = 0;
	bus = (u8) bus_number;
	ddvtprintk((MYIOC_s_NOTE_FMT
			"DV started: numIOs %d bus=%d, id %d dv @ %p\n",
			ioc->name, atomic_read(&queue_depth), bus, id, &dv));

	/* Prep DV structure
	 */
	memset (&dv, 0, sizeof(DVPARAMETERS));
	dv.id = id;

	/* Populate tmax with the current maximum
	 * transfer parameters for this target.
	 * Exit if narrow and async.
	 */
	dv.cmd = MPT_GET_NVRAM_VALS;
	mptscsih_dv_parms(hd, &dv, NULL);
	if ((!dv.max.width) && (!dv.max.offset))
		return 0;

	/* Prep SCSI IO structure
	 */
	iocmd.id = id;
	iocmd.bus = bus;
	iocmd.lun = lun;
	iocmd.flags = 0;
	iocmd.physDiskNum = -1;
	iocmd.rsvd = iocmd.rsvd2 = 0;

	pTarget = hd->Targets[id];
	if (pTarget && (pTarget->tflags & MPT_TARGET_FLAGS_VALID_INQUIRY)) {
		/* Another GEM workaround. Check peripheral device type,
		 * if PROCESSOR, quit DV.
		 */
		if (((pTarget->inq_data[0] & 0x1F) == 0x03) || ((pTarget->inq_data[0] & 0x1F) > 0x08)) {
			pTarget->negoFlags |= (MPT_TARGET_NO_NEGO_WIDE | MPT_TARGET_NO_NEGO_SYNC);
			return 0;
		}
	}

	/* Use tagged commands if possible.
	 */
	if (pTarget) {
		if (pTarget->tflags & MPT_TARGET_FLAGS_Q_YES)
			iocmd.flags |= MPT_ICFLAG_TAGGED_CMD;
		else {
			if (hd->ioc->facts.FWVersion.Word < 0x01000600)
				return 0;

			if ((hd->ioc->facts.FWVersion.Word >= 0x01010000) &&
				(hd->ioc->facts.FWVersion.Word < 0x01010B00))
				return 0;
		}
	}

	/* Prep cfg structure
	 */
	cfg.pageAddr = (bus<<8) | id;
	cfg.hdr = NULL;

	/* Prep SDP0 header
	 */
	header0.PageVersion = ioc->spi_data.sdp0version;
	header0.PageLength = ioc->spi_data.sdp0length;
	header0.PageNumber = 0;
	header0.PageType = MPI_CONFIG_PAGETYPE_SCSI_DEVICE;

	/* Prep SDP1 header
	 */
	header1.PageVersion = ioc->spi_data.sdp1version;
	header1.PageLength = ioc->spi_data.sdp1length;
	header1.PageNumber = 1;
	header1.PageType = MPI_CONFIG_PAGETYPE_SCSI_DEVICE;

	if (header0.PageLength & 1)
		dv_alloc = (header0.PageLength * 4) + 4;

	dv_alloc +=  (2048 + (header1.PageLength * 4));

	pDvBuf = pci_alloc_consistent(ioc->pcidev, dv_alloc, &dvbuf_dma);
	if (pDvBuf == NULL)
		return 0;

	sz = 0;
	pbuf1 = (u8 *)pDvBuf;
	buf1_dma = dvbuf_dma;
	sz +=1024;

	pbuf2 = (u8 *) (pDvBuf + sz);
	buf2_dma = dvbuf_dma + sz;
	sz +=1024;

	pcfg0Data = (SCSIDevicePage0_t *) (pDvBuf + sz);
	cfg0_dma_addr = dvbuf_dma + sz;
	sz += header0.PageLength * 4;

	/* 8-byte alignment
	 */
	if (header0.PageLength & 1)
		sz += 4;

	pcfg1Data = (SCSIDevicePage1_t *) (pDvBuf + sz);
	cfg1_dma_addr = dvbuf_dma + sz;

	/* Skip this ID? Set cfg.hdr to force config page write
	 */
	{
		ScsiCfgData *pspi_data = &hd->ioc->spi_data;
		if (pspi_data->nvram && (pspi_data->nvram[id] != MPT_HOST_NVRAM_INVALID)) {
			/* Set the factor from nvram */
			nfactor = (pspi_data->nvram[id] & MPT_NVRAM_SYNC_MASK) >> 8;
			if (nfactor < pspi_data->minSyncFactor )
				nfactor = pspi_data->minSyncFactor;
	
			if (!(pspi_data->nvram[id] & MPT_NVRAM_ID_SCAN_ENABLE) ||
				(pspi_data->PortFlags == MPI_SCSIPORTPAGE2_PORT_FLAGS_OFF_DV) ) {
	
				ddvprintk((MYIOC_s_NOTE_FMT "DV Skipped: bus, id, lun (%d, %d, %d)\n",
					ioc->name, bus, id, lun));
	
				dv.cmd = MPT_SET_MAX;
				mptscsih_dv_parms(hd, &dv, (void *)pcfg1Data);
				cfg.hdr = &header1;
	
				/* Save the final negotiated settings to
				 * SCSI device page 1.
				 */
				cfg.physAddr = cfg1_dma_addr;
				cfg.action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
				cfg.dir = 1;
				mpt_config(hd->ioc, &cfg);
				goto target_done;
			}
		}
	}

	/* Finish iocmd inititialization - hidden or visible disk? */
	if (ioc->spi_data.pIocPg3) {
		/* Searc IOC page 3 for matching id
		 */
		Ioc3PhysDisk_t *pPDisk =  ioc->spi_data.pIocPg3->PhysDisk;
		int		numPDisk = ioc->spi_data.pIocPg3->NumPhysDisks;

		while (numPDisk) {
			if (pPDisk->PhysDiskID == id) {
				/* match */
				iocmd.flags |= MPT_ICFLAG_PHYS_DISK;
				iocmd.physDiskNum = pPDisk->PhysDiskNum;

				/* Quiesce the IM
				 */
				if (mptscsih_do_raid(hd, MPI_RAID_ACTION_QUIESCE_PHYS_IO, &iocmd) < 0) {
					ddvprintk((MYIOC_s_ERR_FMT "RAID Queisce FAILED!\n", ioc->name));
					goto target_done;
				}
				break;
			}
			pPDisk++;
			numPDisk--;
		}
	}

	/* RAID Volume ID's may double for a physical device. If RAID but
	 * not a physical ID as well, skip DV.
	 */
	if ((hd->ioc->spi_data.isRaid & (1 << id)) && !(iocmd.flags & MPT_ICFLAG_PHYS_DISK))
		goto target_done;


	/* Basic Test.
	 * Async & Narrow - Inquiry
	 * Async & Narrow - Inquiry
	 * Maximum transfer rate - Inquiry
	 * Compare buffers:
	 *	If compare, test complete.
	 *	If miscompare and first pass, repeat
	 *	If miscompare and not first pass, fall back and repeat
	 */
	hd->pLocal = NULL;
	readPage0 = 0;
	sz = SCSI_STD_INQUIRY_BYTES;
	rc = MPT_SCANDV_GOOD;
	while (1) {
		ddvprintk((MYIOC_s_NOTE_FMT "DV: Start Basic test.\n", ioc->name));
		retcode = 0;
		dv.cmd = MPT_SET_MIN;
		mptscsih_dv_parms(hd, &dv, (void *)pcfg1Data);

		cfg.hdr = &header1;
		cfg.physAddr = cfg1_dma_addr;
		cfg.action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
		cfg.dir = 1;
		if (mpt_config(hd->ioc, &cfg) != 0)
			goto target_done;

		/* Wide - narrow - wide workaround case
		 */
		if ((rc == MPT_SCANDV_ISSUE_SENSE) && dv.max.width) {
			/* Send an untagged command to reset disk Qs corrupted
			 * when a parity error occurs on a Request Sense.
			 */
			if ((hd->ioc->facts.FWVersion.Word >= 0x01000600) ||
				((hd->ioc->facts.FWVersion.Word >= 0x01010000) &&
				(hd->ioc->facts.FWVersion.Word < 0x01010B00)) ) {

				iocmd.cmd = CMD_RequestSense;
				iocmd.data_dma = buf1_dma;
				iocmd.data = pbuf1;
				iocmd.size = 0x12;
				if (mptscsih_do_cmd(hd, &iocmd) < 0)
					goto target_done;
				else {
					if (hd->pLocal == NULL)
						goto target_done;
					rc = hd->pLocal->completion;
					if ((rc == MPT_SCANDV_GOOD) || (rc == MPT_SCANDV_SENSE)) {
						dv.max.width = 0;
						doFallback = 0;
					} else
						goto target_done;
				}
			} else
				goto target_done;
		}

		iocmd.cmd = CMD_Inquiry;
		iocmd.data_dma = buf1_dma;
		iocmd.data = pbuf1;
		iocmd.size = sz;
		if (mptscsih_do_cmd(hd, &iocmd) < 0)
			goto target_done;
		else {
			if (hd->pLocal == NULL)
				goto target_done;
			rc = hd->pLocal->completion;
			if (rc == MPT_SCANDV_GOOD) {
				if (hd->pLocal->scsiStatus == STS_BUSY) {
					if ((iocmd.flags & MPT_ICFLAG_TAGGED_CMD) == 0)
						retcode = 1;
					else
						retcode = 0;

					goto target_done;
				}
			} else if  (rc == MPT_SCANDV_SENSE) {
				;
			} else {
				/* If first command doesn't complete
				 * with a good status or with a check condition,
				 * exit.
				 */
				goto target_done;
			}
		}

		/* Reset the size for disks
		 */
		inq0 = (*pbuf1) & 0x1F;
		if ((inq0 == 0) && pTarget && !pTarget->raidVolume) {
			sz = 0x40;
			iocmd.size = sz;
		}

		/* Another GEM workaround. Check peripheral device type,
		 * if PROCESSOR, quit DV.
		 */
		if (((pbuf1[0] & 0x1F) == 0x03) || ((pbuf1[0] & 0x1F) > 0x08))
			goto target_done;

		if (mptscsih_do_cmd(hd, &iocmd) < 0)
			goto target_done;

		if (sz == 0x40) {
			if ((pTarget->maxWidth == 1) && (pTarget->maxOffset) && (nfactor < 0x0A)
				&& (pTarget->minSyncFactor > 0x09)) {
				if ((pbuf1[56] & 0x04) == 0)
					;
				else if ((pbuf1[56] & 0x01) == 1) {
					pTarget->minSyncFactor = nfactor > MPT_ULTRA320 ? nfactor : MPT_ULTRA320;
				} else {
					pTarget->minSyncFactor = nfactor > MPT_ULTRA160 ? nfactor : MPT_ULTRA160;
				}

				dv.max.factor = pTarget->minSyncFactor;

				if ((pbuf1[56] & 0x02) == 0) {
					pTarget->negoFlags |= MPT_TARGET_NO_NEGO_QAS;
					hd->ioc->spi_data.noQas = MPT_TARGET_NO_NEGO_QAS;
				}
			}
		}

		if (doFallback)
			dv.cmd = MPT_FALLBACK;
		else
			dv.cmd = MPT_SET_MAX;

		mptscsih_dv_parms(hd, &dv, (void *)pcfg1Data);
		if (mpt_config(hd->ioc, &cfg) != 0)
			goto target_done;

		if ((!dv.now.width) && (!dv.now.offset))
			goto target_done;

		iocmd.cmd = CMD_Inquiry;
		iocmd.data_dma = buf2_dma;
		iocmd.data = pbuf2;
		iocmd.size = sz;
		if (mptscsih_do_cmd(hd, &iocmd) < 0)
			goto target_done;
		else if (hd->pLocal == NULL)
			goto target_done;
		else {
			/* Save the return code.
			 * If this is the first pass,
			 * read SCSI Device Page 0
			 * and update the target max parameters.
			 */
			rc = hd->pLocal->completion;
			doFallback = 0;
			if (rc == MPT_SCANDV_GOOD) {
				if (!readPage0) {
					u32 sdp0_info;
					u32 sdp0_nego;

					cfg.hdr = &header0;
					cfg.physAddr = cfg0_dma_addr;
					cfg.action = MPI_CONFIG_ACTION_PAGE_READ_CURRENT;
					cfg.dir = 0;

					if (mpt_config(hd->ioc, &cfg) != 0)
						goto target_done;

					sdp0_info = le32_to_cpu(pcfg0Data->Information) & 0x0E;
					sdp0_nego = (le32_to_cpu(pcfg0Data->NegotiatedParameters) & 0xFF00 ) >> 8;

					/* Quantum and Fujitsu workarounds.
					 * Quantum: PPR U320 -> PPR reply with Ultra2 and wide
					 * Fujitsu: PPR U320 -> Msg Reject and Ultra2 and wide
					 * Resetart with a request for U160.
					 */
					if ((dv.now.factor == MPT_ULTRA320) && (sdp0_nego == MPT_ULTRA2)) {
							doFallback = 1;
					} else {
						dv.cmd = MPT_UPDATE_MAX;
						mptscsih_dv_parms(hd, &dv, (void *)pcfg0Data);
						/* Update the SCSI device page 1 area
						 */
						pcfg1Data->RequestedParameters = pcfg0Data->NegotiatedParameters;
						readPage0 = 1;
					}
				}

				/* Quantum workaround. Restart this test will the fallback
				 * flag set.
				 */
				if (doFallback == 0) {
					if (memcmp(pbuf1, pbuf2, sz) != 0) {
						if (!firstPass)
							doFallback = 1;
					} else
						break;	/* test complete */
				}


			} else if (rc == MPT_SCANDV_ISSUE_SENSE)
				doFallback = 1;	/* set fallback flag */
			else if ((rc == MPT_SCANDV_DID_RESET) || (rc == MPT_SCANDV_SENSE))
				doFallback = 1;	/* set fallback flag */
			else
				goto target_done;

			firstPass = 0;
		}
	}
	ddvprintk((MYIOC_s_NOTE_FMT "DV: Basic test completed OK.\n", ioc->name));
	inq0 = (*pbuf1) & 0x1F;

	/* Continue only for disks
	 */
	if (inq0 != 0)
		goto target_done;

	if ( ioc->spi_data.PortFlags == MPI_SCSIPORTPAGE2_PORT_FLAGS_BASIC_DV_ONLY )
		goto target_done;

	/* Start the Enhanced Test.
	 * 0) issue TUR to clear out check conditions
	 * 1) read capacity of echo (regular) buffer
	 * 2) reserve device
	 * 3) do write-read-compare data pattern test
	 * 4) release
	 * 5) update nego parms to target struct
	 */
	cfg.hdr = &header1;
	cfg.physAddr = cfg1_dma_addr;
	cfg.action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	cfg.dir = 1;

	iocmd.cmd = CMD_TestUnitReady;
	iocmd.data_dma = -1;
	iocmd.data = NULL;
	iocmd.size = 0;
	notDone = 1;
	while (notDone) {
		if (mptscsih_do_cmd(hd, &iocmd) < 0)
			goto target_done;

		if (hd->pLocal == NULL)
			goto target_done;

		rc = hd->pLocal->completion;
		if (rc == MPT_SCANDV_GOOD)
			notDone = 0;
		else if (rc == MPT_SCANDV_SENSE) {
			u8 skey = hd->pLocal->sense[2] & 0x0F;
			u8 asc = hd->pLocal->sense[12];
			u8 ascq = hd->pLocal->sense[13];
			ddvprintk((MYIOC_s_INFO_FMT
				"SenseKey:ASC:ASCQ = (%x:%02x:%02x)\n",
				ioc->name, skey, asc, ascq));

			if (skey == SK_UNIT_ATTENTION)
				notDone++; /* repeat */
			else if ((skey == SK_NOT_READY) &&
					(asc == 0x04)&&(ascq == 0x01)) {
				/* wait then repeat */
				mdelay (2000);
				notDone++;
			} else if ((skey == SK_NOT_READY) && (asc == 0x3A)) {
				/* no medium, try read test anyway */
				notDone = 0;
			} else {
				/* All other errors are fatal.
				 */
				ddvprintk((MYIOC_s_INFO_FMT "DV: fatal error.",
						ioc->name));
				goto target_done;
			}
		} else
			goto target_done;
	}

	iocmd.cmd = CMD_ReadBuffer;
	iocmd.data_dma = buf1_dma;
	iocmd.data = pbuf1;
	iocmd.size = 4;
	iocmd.flags |= MPT_ICFLAG_BUF_CAP;

	dataBufSize = 0;
	echoBufSize = 0;
	for (patt = 0; patt < 2; patt++) {
		if (patt == 0)
			iocmd.flags |= MPT_ICFLAG_ECHO;
		else
			iocmd.flags &= ~MPT_ICFLAG_ECHO;

		notDone = 1;
		while (notDone) {
			bufsize = 0;

			/* If not ready after 8 trials,
			 * give up on this device.
			 */
			if (notDone > 8)
				goto target_done;

			if (mptscsih_do_cmd(hd, &iocmd) < 0)
				goto target_done;
			else if (hd->pLocal == NULL)
				goto target_done;
			else {
				rc = hd->pLocal->completion;
				ddvprintk(("ReadBuffer Comp Code %d", rc));
				ddvprintk(("  buff: %0x %0x %0x %0x\n",
					pbuf1[0], pbuf1[1], pbuf1[2], pbuf1[3]));

				if (rc == MPT_SCANDV_GOOD) {
					notDone = 0;
					if (iocmd.flags & MPT_ICFLAG_ECHO) {
						bufsize =  ((pbuf1[2] & 0x1F) <<8) | pbuf1[3];
					} else {
						bufsize =  pbuf1[1]<<16 | pbuf1[2]<<8 | pbuf1[3];
					}
				} else if (rc == MPT_SCANDV_SENSE) {
					u8 skey = hd->pLocal->sense[2] & 0x0F;
					u8 asc = hd->pLocal->sense[12];
					u8 ascq = hd->pLocal->sense[13];
					ddvprintk((MYIOC_s_INFO_FMT
						"SenseKey:ASC:ASCQ = (%x:%02x:%02x)\n",
						ioc->name, skey, asc, ascq));
					if (skey == SK_ILLEGAL_REQUEST) {
						notDone = 0;
					} else if (skey == SK_UNIT_ATTENTION) {
						notDone++; /* repeat */
					} else if ((skey == SK_NOT_READY) &&
						(asc == 0x04)&&(ascq == 0x01)) {
						/* wait then repeat */
						mdelay (2000);
						notDone++;
					} else {
						/* All other errors are fatal.
						 */
						ddvprintk((MYIOC_s_INFO_FMT "DV: fatal error.",
							ioc->name));
						goto target_done;
					}
				} else {
					/* All other errors are fatal
					 */
					goto target_done;
				}
			}
		}

		if (iocmd.flags & MPT_ICFLAG_ECHO)
			echoBufSize = bufsize;
		else
			dataBufSize = bufsize;
	}
	sz = 0;
	iocmd.flags &= ~MPT_ICFLAG_BUF_CAP;

	/* Use echo buffers if possible,
	 * Exit if both buffers are 0.
	 */
	if (echoBufSize > 0) {
		iocmd.flags |= MPT_ICFLAG_ECHO;
		if (dataBufSize > 0)
			bufsize = MIN(echoBufSize, dataBufSize);
		else
			bufsize = echoBufSize;
	} else if (dataBufSize == 0)
		goto target_done;

	ddvprintk((MYIOC_s_INFO_FMT "%s Buffer Capacity %d\n", ioc->name,
		(iocmd.flags & MPT_ICFLAG_ECHO) ? "Echo" : " ", bufsize));

	/* Data buffers for write-read-compare test max 1K.
	 */
	sz = MIN(bufsize, 1024);

	/* --- loop ----
	 * On first pass, always issue a reserve.
	 * On additional loops, only if a reset has occurred.
	 * iocmd.flags indicates if echo or regular buffer
	 */
	for (patt = 0; patt < 4; patt++) {
		ddvprintk(("Pattern %d\n", patt));
		if ((iocmd.flags & MPT_ICFLAG_RESERVED) && (iocmd.flags & MPT_ICFLAG_DID_RESET)) {
			iocmd.cmd = CMD_TestUnitReady;
			iocmd.data_dma = -1;
			iocmd.data = NULL;
			iocmd.size = 0;
			if (mptscsih_do_cmd(hd, &iocmd) < 0)
				goto target_done;

			iocmd.cmd = CMD_Release6;
			iocmd.data_dma = -1;
			iocmd.data = NULL;
			iocmd.size = 0;
			if (mptscsih_do_cmd(hd, &iocmd) < 0)
				goto target_done;
			else if (hd->pLocal == NULL)
				goto target_done;
			else {
				rc = hd->pLocal->completion;
				ddvprintk(("Release rc %d\n", rc));
				if (rc == MPT_SCANDV_GOOD)
					iocmd.flags &= ~MPT_ICFLAG_RESERVED;
				else
					goto target_done;
			}
			iocmd.flags &= ~MPT_ICFLAG_RESERVED;
		}
		iocmd.flags &= ~MPT_ICFLAG_DID_RESET;

		repeat = 5;
		while (repeat && (!(iocmd.flags & MPT_ICFLAG_RESERVED))) {
			iocmd.cmd = CMD_Reserve6;
			iocmd.data_dma = -1;
			iocmd.data = NULL;
			iocmd.size = 0;
			if (mptscsih_do_cmd(hd, &iocmd) < 0)
				goto target_done;
			else if (hd->pLocal == NULL)
				goto target_done;
			else {
				rc = hd->pLocal->completion;
				if (rc == MPT_SCANDV_GOOD) {
					iocmd.flags |= MPT_ICFLAG_RESERVED;
				} else if (rc == MPT_SCANDV_SENSE) {
					/* Wait if coming ready
					 */
					u8 skey = hd->pLocal->sense[2] & 0x0F;
					u8 asc = hd->pLocal->sense[12];
					u8 ascq = hd->pLocal->sense[13];
					ddvprintk((MYIOC_s_INFO_FMT
						"DV: Reserve Failed: ", ioc->name));
					ddvprintk(("SenseKey:ASC:ASCQ = (%x:%02x:%02x)\n",
							skey, asc, ascq));

					if ((skey == SK_NOT_READY) && (asc == 0x04)&&
									(ascq == 0x01)) {
						/* wait then repeat */
						mdelay (2000);
						notDone++;
					} else {
						ddvprintk((MYIOC_s_INFO_FMT
							"DV: Reserved Failed.", ioc->name));
						goto target_done;
					}
				} else {
					ddvprintk((MYIOC_s_INFO_FMT "DV: Reserved Failed.",
							 ioc->name));
					goto target_done;
				}
			}
		}

		mptscsih_fillbuf(pbuf1, sz, patt, 1);
		iocmd.cmd = CMD_WriteBuffer;
		iocmd.data_dma = buf1_dma;
		iocmd.data = pbuf1;
		iocmd.size = sz;
		if (mptscsih_do_cmd(hd, &iocmd) < 0)
			goto target_done;
		else if (hd->pLocal == NULL)
			goto target_done;
		else {
			rc = hd->pLocal->completion;
			if (rc == MPT_SCANDV_GOOD)
				;		/* Issue read buffer */
			else if (rc == MPT_SCANDV_DID_RESET) {
				/* If using echo buffers, reset to data buffers.
				 * Else do Fallback and restart
				 * this test (re-issue reserve
				 * because of bus reset).
				 */
				if ((iocmd.flags & MPT_ICFLAG_ECHO) && (dataBufSize >= bufsize)) {
					iocmd.flags &= ~MPT_ICFLAG_ECHO;
				} else {
					dv.cmd = MPT_FALLBACK;
					mptscsih_dv_parms(hd, &dv, (void *)pcfg1Data);

					if (mpt_config(hd->ioc, &cfg) != 0)
						goto target_done;

					if ((!dv.now.width) && (!dv.now.offset))
						goto target_done;
				}

				iocmd.flags |= MPT_ICFLAG_DID_RESET;
				patt = -1;
				continue;
			} else if (rc == MPT_SCANDV_SENSE) {
				/* Restart data test if UA, else quit.
				 */
				u8 skey = hd->pLocal->sense[2] & 0x0F;
				ddvprintk((MYIOC_s_INFO_FMT
					"SenseKey:ASC:ASCQ = (%x:%02x:%02x)\n", ioc->name, skey,
					hd->pLocal->sense[12], hd->pLocal->sense[13]));
				if (skey == SK_UNIT_ATTENTION) {
					patt = -1;
					continue;
				} else if (skey == SK_ILLEGAL_REQUEST) {
					if (iocmd.flags & MPT_ICFLAG_ECHO) {
						if (dataBufSize >= bufsize) {
							iocmd.flags &= ~MPT_ICFLAG_ECHO;
							patt = -1;
							continue;
						}
					}
					goto target_done;
				}
				else
					goto target_done;
			} else {
				/* fatal error */
				goto target_done;
			}
		}

		iocmd.cmd = CMD_ReadBuffer;
		iocmd.data_dma = buf2_dma;
		iocmd.data = pbuf2;
		iocmd.size = sz;
		if (mptscsih_do_cmd(hd, &iocmd) < 0)
			goto target_done;
		else if (hd->pLocal == NULL)
			goto target_done;
		else {
			rc = hd->pLocal->completion;
			if (rc == MPT_SCANDV_GOOD) {
				 /* If buffers compare,
				  * go to next pattern,
				  * else, do a fallback and restart
				  * data transfer test.
				  */
				if (memcmp (pbuf1, pbuf2, sz) == 0) {
					; /* goto next pattern */
				} else {
					/* Miscompare with Echo buffer, go to data buffer,
					 * if that buffer exists.
					 * Miscompare with Data buffer, check first 4 bytes,
					 * some devices return capacity. Exit in this case.
					 */
					if (iocmd.flags & MPT_ICFLAG_ECHO) {
						if (dataBufSize >= bufsize)
							iocmd.flags &= ~MPT_ICFLAG_ECHO;
						else
							goto target_done;
					} else {
						if (dataBufSize == (pbuf2[1]<<16 | pbuf2[2]<<8 | pbuf2[3])) {
							/* Argh. Device returning wrong data.
							 * Quit DV for this device.
							 */
							goto target_done;
						}

						/* Had an actual miscompare. Slow down.*/
						dv.cmd = MPT_FALLBACK;
						mptscsih_dv_parms(hd, &dv, (void *)pcfg1Data);

						if (mpt_config(hd->ioc, &cfg) != 0)
							goto target_done;

						if ((!dv.now.width) && (!dv.now.offset))
							goto target_done;
					}

					patt = -1;
					continue;
				}
			} else if (rc == MPT_SCANDV_DID_RESET) {
				/* Do Fallback and restart
				 * this test (re-issue reserve
				 * because of bus reset).
				 */
				dv.cmd = MPT_FALLBACK;
				mptscsih_dv_parms(hd, &dv, (void *)pcfg1Data);

				if (mpt_config(hd->ioc, &cfg) != 0)
					 goto target_done;

				if ((!dv.now.width) && (!dv.now.offset))
					goto target_done;

				iocmd.flags |= MPT_ICFLAG_DID_RESET;
				patt = -1;
				continue;
			} else if (rc == MPT_SCANDV_SENSE) {
				/* Restart data test if UA, else quit.
				 */
				u8 skey = hd->pLocal->sense[2] & 0x0F;
				ddvprintk((MYIOC_s_INFO_FMT
					"SenseKey:ASC:ASCQ = (%x:%02x:%02x)\n", ioc->name, skey,
					hd->pLocal->sense[12], hd->pLocal->sense[13]));
				if (skey == SK_UNIT_ATTENTION) {
					patt = -1;
					continue;
				}
				else
					goto target_done;
			} else {
				/* fatal error */
				goto target_done;
			}
		}

	} /* --- end of patt loop ---- */

target_done:
	if (iocmd.flags & MPT_ICFLAG_RESERVED) {
		iocmd.cmd = CMD_Release6;
		iocmd.data_dma = -1;
		iocmd.data = NULL;
		iocmd.size = 0;
		if (mptscsih_do_cmd(hd, &iocmd) < 0)
			printk(MYIOC_s_INFO_FMT "DV: Release failed. id %d",
					ioc->name, id);
		else if (hd->pLocal) {
			if (hd->pLocal->completion == MPT_SCANDV_GOOD)
				iocmd.flags &= ~MPT_ICFLAG_RESERVED;
		} else {
			printk(MYIOC_s_INFO_FMT "DV: Release failed. id %d",
						ioc->name, id);
		}
	}


	/* Set if cfg1_dma_addr contents is valid
	 */
	if ((cfg.hdr != NULL) && (retcode == 0)){
		/* If disk, not U320, disable QAS
		 */
		if ((inq0 == 0) && (dv.now.factor > MPT_ULTRA320))
			hd->ioc->spi_data.noQas = MPT_TARGET_NO_NEGO_QAS;

		dv.cmd = MPT_SAVE;
		mptscsih_dv_parms(hd, &dv, (void *)pcfg1Data);

		/* Double writes to SDP1 can cause problems,
		 * skip save of the final negotiated settings to
		 * SCSI device page 1.
		 *
		cfg.hdr = &header1;
		cfg.physAddr = cfg1_dma_addr;
		cfg.action = MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT;
		cfg.dir = 1;
		mpt_config(hd->ioc, &cfg);
		 */
	}

	/* If this is a RAID Passthrough, enable internal IOs
	 */
	if (iocmd.flags & MPT_ICFLAG_PHYS_DISK) {
		if (mptscsih_do_raid(hd, MPI_RAID_ACTION_ENABLE_PHYS_IO, &iocmd) < 0)
			ddvprintk((MYIOC_s_ERR_FMT "RAID Enable FAILED!\n", ioc->name));
	}

	/* Done with the DV scan of the current target
	 */
	if (pDvBuf)
		pci_free_consistent(ioc->pcidev, dv_alloc, pDvBuf, dvbuf_dma);

	ddvtprintk((MYIOC_s_INFO_FMT "DV Done. IOs outstanding = %d\n",
			ioc->name, atomic_read(&queue_depth)));

	return retcode;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*	mptscsih_dv_parms - perform a variety of operations on the
 *	parameters used for negotiation.
 *	@hd: Pointer to a SCSI host.
 *	@dv: Pointer to a structure that contains the maximum and current
 *		negotiated parameters.
 */
static void
mptscsih_dv_parms(MPT_SCSI_HOST *hd, DVPARAMETERS *dv,void *pPage)
{
	VirtDevice		*pTarget;
	SCSIDevicePage0_t	*pPage0;
	SCSIDevicePage1_t	*pPage1;
	int			val = 0, data, configuration;
	u8			width = 0;
	u8			offset = 0;
	u8			factor = 0;
	u8			negoFlags = 0;
	u8			cmd = dv->cmd;
	u8			id = dv->id;

	switch (cmd) {
	case MPT_GET_NVRAM_VALS:
		ddvprintk((MYIOC_s_NOTE_FMT "Getting NVRAM: ",
							 hd->ioc->name));
		/* Get the NVRAM values and save in tmax
		 * If not an LVD bus, the adapter minSyncFactor has been
		 * already throttled back.
		 */
		if ((hd->Targets)&&((pTarget = hd->Targets[(int)id]) != NULL) && !pTarget->raidVolume) {
			width = pTarget->maxWidth;
			offset = pTarget->maxOffset;
			factor = pTarget->minSyncFactor;
			negoFlags = pTarget->negoFlags;
		} else {
			if (hd->ioc->spi_data.nvram && (hd->ioc->spi_data.nvram[id] != MPT_HOST_NVRAM_INVALID)) {
				data = hd->ioc->spi_data.nvram[id];
				width = data & MPT_NVRAM_WIDE_DISABLE ? 0 : 1;
				if ((offset = hd->ioc->spi_data.maxSyncOffset) == 0)
					factor = MPT_ASYNC;
				else {
					factor = (data & MPT_NVRAM_SYNC_MASK) >> MPT_NVRAM_SYNC_SHIFT;
					if ((factor == 0) || (factor == MPT_ASYNC)){
						factor = MPT_ASYNC;
						offset = 0;
					}
				}
			} else {
				width = MPT_NARROW;
				offset = 0;
				factor = MPT_ASYNC;
			}

			/* Set the negotiation flags */
			negoFlags = hd->ioc->spi_data.noQas;
			if (!width)
				negoFlags |= MPT_TARGET_NO_NEGO_WIDE;

			if (!offset)
				negoFlags |= MPT_TARGET_NO_NEGO_SYNC;
		}

		/* limit by adapter capabilities */
		width = MIN(width, hd->ioc->spi_data.maxBusWidth);
		offset = MIN(offset, hd->ioc->spi_data.maxSyncOffset);
		factor = MAX(factor, hd->ioc->spi_data.minSyncFactor);

		/* Check Consistency */
		if (offset && (factor < MPT_ULTRA2) && !width)
			factor = MPT_ULTRA2;

		dv->max.width = width;
		dv->max.offset = offset;
		dv->max.factor = factor;
		dv->max.flags = negoFlags;
		ddvprintk((" width %d, factor %x, offset %x flags %x\n",
				width, factor, offset, negoFlags));
		break;

	case MPT_UPDATE_MAX:
		ddvprintk((MYIOC_s_NOTE_FMT
			"Updating with SDP0 Data: ", hd->ioc->name));
		/* Update tmax values with those from Device Page 0.*/
		pPage0 = (SCSIDevicePage0_t *) pPage;
		if (pPage0) {
			val = cpu_to_le32(pPage0->NegotiatedParameters);
			dv->max.width = val & MPI_SCSIDEVPAGE0_NP_WIDE ? 1 : 0;
			dv->max.offset = (val&MPI_SCSIDEVPAGE0_NP_NEG_SYNC_OFFSET_MASK) >> 16;
			dv->max.factor = (val&MPI_SCSIDEVPAGE0_NP_NEG_SYNC_PERIOD_MASK) >> 8;
		}

		dv->now.width = dv->max.width;
		dv->now.offset = dv->max.offset;
		dv->now.factor = dv->max.factor;
		ddvprintk(("width %d, factor %x, offset %x, flags %x\n",
				dv->now.width, dv->now.factor, dv->now.offset, dv->now.flags));
		break;

	case MPT_SET_MAX:
		ddvprintk((MYIOC_s_NOTE_FMT "Setting Max: ",
								hd->ioc->name));
		/* Set current to the max values. Update the config page.*/
		dv->now.width = dv->max.width;
		dv->now.offset = dv->max.offset;
		dv->now.factor = dv->max.factor;
		dv->now.flags = dv->max.flags;

		pPage1 = (SCSIDevicePage1_t *)pPage;
		if (pPage1) {
			mptscsih_setDevicePage1Flags (dv->now.width, dv->now.factor,
				dv->now.offset, &val, &configuration, dv->now.flags);
			pPage1->RequestedParameters = le32_to_cpu(val);
			pPage1->Reserved = 0;
			pPage1->Configuration = le32_to_cpu(configuration);

		}

		ddvprintk(("width %d, factor %x, offset %x request %x, config %x\n",
				dv->now.width, dv->now.factor, dv->now.offset, val, configuration));
		break;

	case MPT_SET_MIN:
		ddvprintk((MYIOC_s_NOTE_FMT "Setting Min: ",
								hd->ioc->name));
		/* Set page to asynchronous and narrow
		 * Do not update now, breaks fallback routine. */
		width = MPT_NARROW;
		offset = 0;
		factor = MPT_ASYNC;
		negoFlags = dv->max.flags;

		pPage1 = (SCSIDevicePage1_t *)pPage;
		if (pPage1) {
			mptscsih_setDevicePage1Flags (width, factor,
				offset, &val, &configuration, negoFlags);
			pPage1->RequestedParameters = le32_to_cpu(val);
			pPage1->Reserved = 0;
			pPage1->Configuration = le32_to_cpu(configuration);
		}
		ddvprintk(("width %d, factor %x, offset %x request %x config %x\n",
				width, factor, offset, val, configuration));
		break;

	case MPT_FALLBACK:
		ddvprintk((MYIOC_s_NOTE_FMT
			"Fallback: Start: offset %d, factor %x, width %d \n",
				hd->ioc->name, dv->now.offset,
				dv->now.factor, dv->now.width));
		width = dv->now.width;
		offset = dv->now.offset;
		factor = dv->now.factor;
		if ((offset) && (dv->max.width)) {
			if (factor < MPT_ULTRA160)
				factor = MPT_ULTRA160;
			else if (factor < MPT_ULTRA2) {
				factor = MPT_ULTRA2;
				width = MPT_WIDE;
			} else if ((factor == MPT_ULTRA2) && width) {
				factor = MPT_ULTRA2;
				width = MPT_NARROW;
			} else if (factor < MPT_ULTRA) {
				factor = MPT_ULTRA;
				width = MPT_WIDE;
			} else if ((factor == MPT_ULTRA) && width) {
				factor = MPT_ULTRA;
				width = MPT_NARROW;
			} else if (factor < MPT_FAST) {
				factor = MPT_FAST;
				width = MPT_WIDE;
			} else if ((factor == MPT_FAST) && width) {
				factor = MPT_FAST;
				width = MPT_NARROW;
			} else if (factor < MPT_SCSI) {
				factor = MPT_SCSI;
				width = MPT_WIDE;
			} else if ((factor == MPT_SCSI) && width) {
				factor = MPT_SCSI;
				width = MPT_NARROW;
			} else {
				factor = MPT_ASYNC;
				offset = 0;
			}

		} else if (offset) {
			width = MPT_NARROW;
			if (factor < MPT_ULTRA)
				factor = MPT_ULTRA;
			else if (factor < MPT_FAST)
				factor = MPT_FAST;
			else if (factor < MPT_SCSI)
				factor = MPT_SCSI;
			else {
				factor = MPT_ASYNC;
				offset = 0;
			}

		} else {
			width = MPT_NARROW;
			factor = MPT_ASYNC;
		}
		dv->max.flags |= MPT_TARGET_NO_NEGO_QAS;

		dv->now.width = width;
		dv->now.offset = offset;
		dv->now.factor = factor;
		dv->now.flags = dv->max.flags;

		pPage1 = (SCSIDevicePage1_t *)pPage;
		if (pPage1) {
			mptscsih_setDevicePage1Flags (width, factor, offset, &val,
						&configuration, dv->now.flags);

			pPage1->RequestedParameters = le32_to_cpu(val);
			pPage1->Reserved = 0;
			pPage1->Configuration = le32_to_cpu(configuration);
		}

		ddvprintk(("Finish: offset %d, factor %x, width %d, request %x config %x\n",
			     dv->now.offset, dv->now.factor, dv->now.width, val, configuration));
		break;

	case MPT_SAVE:
		ddvprintk((MYIOC_s_NOTE_FMT
			"Saving to Target structure: ", hd->ioc->name));
		ddvprintk(("offset %d, factor %x, width %d \n",
			     dv->now.offset, dv->now.factor, dv->now.width));

		/* Save these values to target structures
		 * or overwrite nvram (phys disks only).
		 */

		if ((hd->Targets)&&((pTarget = hd->Targets[(int)id]) != NULL) && !pTarget->raidVolume ) {
			pTarget->maxWidth = dv->now.width;
			pTarget->maxOffset = dv->now.offset;
			pTarget->minSyncFactor = dv->now.factor;
			pTarget->negoFlags = dv->now.flags;
		} else {
			/* Preserv all flags, use
			 * read-modify-write algorithm
			 */
			if (hd->ioc->spi_data.nvram) {
				data = hd->ioc->spi_data.nvram[id];

				if (dv->now.width)
					data &= ~MPT_NVRAM_WIDE_DISABLE;
				else
					data |= MPT_NVRAM_WIDE_DISABLE;

				if (!dv->now.offset)
					factor = MPT_ASYNC;

				data &= ~MPT_NVRAM_SYNC_MASK;
				data |= (dv->now.factor << MPT_NVRAM_SYNC_SHIFT) & MPT_NVRAM_SYNC_MASK;

				hd->ioc->spi_data.nvram[id] = data;
			}
		}
		break;
	}
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*	mptscsih_fillbuf - fill a buffer with a special data pattern
 *		cleanup. For bus scan only.
 *
 *	@buffer: Pointer to data buffer to be filled.
 *	@size: Number of bytes to fill
 *	@index: Pattern index
 *	@width: bus width, 0 (8 bits) or 1 (16 bits)
 */
static void
mptscsih_fillbuf(char *buffer, int size, int index, int width)
{
	char *ptr = buffer;
	int ii;
	char byte;
	short val;

	switch (index) {
	case 0:

		if (width) {
			/* Pattern:  0000 FFFF 0000 FFFF
			 */
			for (ii=0; ii < size; ii++, ptr++) {
				if (ii & 0x02)
					*ptr = 0xFF;
				else
					*ptr = 0x00;
			}
		} else {
			/* Pattern:  00 FF 00 FF
			 */
			for (ii=0; ii < size; ii++, ptr++) {
				if (ii & 0x01)
					*ptr = 0xFF;
				else
					*ptr = 0x00;
			}
		}
		break;

	case 1:
		if (width) {
			/* Pattern:  5555 AAAA 5555 AAAA 5555
			 */
			for (ii=0; ii < size; ii++, ptr++) {
				if (ii & 0x02)
					*ptr = 0xAA;
				else
					*ptr = 0x55;
			}
		} else {
			/* Pattern:  55 AA 55 AA 55
			 */
			for (ii=0; ii < size; ii++, ptr++) {
				if (ii & 0x01)
					*ptr = 0xAA;
				else
					*ptr = 0x55;
			}
		}
		break;

	case 2:
		/* Pattern:  00 01 02 03 04 05
		 * ... FE FF 00 01..
		 */
		for (ii=0; ii < size; ii++, ptr++)
			*ptr = (char) ii;
		break;

	case 3:
		if (width) {
			/* Wide Pattern:  FFFE 0001 FFFD 0002
			 * ...  4000 DFFF 8000 EFFF
			 */
			byte = 0;
			for (ii=0; ii < size/2; ii++) {
				/* Create the base pattern
				 */
				val = (1 << byte);
				/* every 64 (0x40) bytes flip the pattern
				 * since we fill 2 bytes / iteration,
				 * test for ii = 0x20
				 */
				if (ii & 0x20)
					val = ~(val);

				if (ii & 0x01) {
					*ptr = (char)( (val & 0xFF00) >> 8);
					ptr++;
					*ptr = (char)(val & 0xFF);
					byte++;
					byte &= 0x0F;
				} else {
					val = ~val;
					*ptr = (char)( (val & 0xFF00) >> 8);
					ptr++;
					*ptr = (char)(val & 0xFF);
				}

				ptr++;
			}
		} else {
			/* Narrow Pattern:  FE 01 FD 02 FB 04
			 * .. 7F 80 01 FE 02 FD ...  80 7F
			 */
			byte = 0;
			for (ii=0; ii < size; ii++, ptr++) {
				/* Base pattern - first 32 bytes
				 */
				if (ii & 0x01) {
					*ptr = (1 << byte);
					byte++;
					byte &= 0x07;
				} else {
					*ptr = (char) (~(1 << byte));
				}

				/* Flip the pattern every 32 bytes
				 */
				if (ii & 0x20)
					*ptr = ~(*ptr);
			}
		}
		break;
	}
}
#endif /* ~MPTSCSIH_DISABLE_DOMAIN_VALIDATION */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/* Commandline Parsing routines and defines.
 *
 * insmod format:
 *	insmod mptscsih mptscsih="width:1 dv:n factor:0x09 saf-te:1"
 *  boot format:
 *	mptscsih=width:1,dv:n,factor:0x8,saf-te:1
 *
 */
#ifdef MODULE
#define	ARG_SEP	' '
#else
#define	ARG_SEP	','
#endif

static char setup_token[] __initdata =
	"dv:"
	"width:"
	"factor:"
	"saf-te:"
       ;	/* DO NOT REMOVE THIS ';' */

#define OPT_DV			1
#define OPT_MAX_WIDTH		2
#define OPT_MIN_SYNC_FACTOR	3
#define OPT_SAF_TE		4

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
static int
__init get_setup_token(char *p)
{
	char *cur = setup_token;
	char *pc;
	int i = 0;

	while (cur != NULL && (pc = strchr(cur, ':')) != NULL) {
		++pc;
		++i;
		if (!strncmp(p, cur, pc - cur))
			return i;
		cur = pc;
	}
	return 0;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
static int
__init mptscsih_setup(char *str)
{
	char *cur = str;
	char *pc, *pv;
	unsigned long val;
	int  c;

	while (cur != NULL && (pc = strchr(cur, ':')) != NULL) {
		char *pe;

		val = 0;
		pv = pc;
		c = *++pv;

		if	(c == 'n')
			val = 0;
		else if	(c == 'y')
			val = 1;
		else
			val = (int) simple_strtoul(pv, &pe, 0);

		printk("Found Token: %s, value %x\n", cur, (int)val);
		switch (get_setup_token(cur)) {
		case OPT_DV:
			driver_setup.dv = val;
			break;

		case OPT_MAX_WIDTH:
			driver_setup.max_width = val;
			break;

		case OPT_MIN_SYNC_FACTOR:
			driver_setup.min_sync_fac = val;
			break;

		case OPT_SAF_TE:
			driver_setup.saf_te = val;
			break;

		default:
			printk("mptscsih_setup: unexpected boot option '%.*s' ignored\n", (int)(pc-cur+1), cur);
			break;
		}

		if ((cur = strchr(cur, ARG_SEP)) != NULL)
			++cur;
	}
	return 1;
}

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

