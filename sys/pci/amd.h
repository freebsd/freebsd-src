/*
 *********************************************************************
 *	FILE NAME  : amd.h
 *	     BY    : C.L. Huang 	(ching@tekram.com.tw)
 *		     Erich Chen     (erich@tekram.com.tw)
 *	Description: Device Driver for the amd53c974 PCI Bus Master
 *		     SCSI Host adapter found on cards such as
 *		     the Tekram DC-390(T).
 * (C)Copyright 1995-1999 Tekram Technology Co., Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************
 * $FreeBSD: src/sys/pci/amd.h,v 1.1.4.1 2000/04/14 13:16:54 nyan Exp $
 */

#ifndef AMD_H
#define AMD_H

#define AMD_TRANS_CUR		0x01	/* Modify current neogtiation status */
#define AMD_TRANS_ACTIVE	0x03	/* Assume this is the active target */
#define AMD_TRANS_GOAL		0x04	/* Modify negotiation goal */
#define AMD_TRANS_USER		0x08	/* Modify user negotiation settings */

/*
 * Per target transfer parameters.
 */
struct amd_transinfo {
	u_int8_t period;
	u_int8_t offset;
};

struct amd_target_info {
	/*
	 * Records the currently active and user/default settings for
	 * tagged queueing and disconnection for each target.
	 */
	u_int8_t disc_tag;
#define		AMD_CUR_DISCENB	0x01
#define		AMD_CUR_TAGENB	0x02
#define		AMD_USR_DISCENB	0x04
#define		AMD_USR_TAGENB	0x08
	u_int8_t   CtrlR1;
	u_int8_t   CtrlR3;
	u_int8_t   CtrlR4;
	u_int8_t   sync_period_reg;
	u_int8_t   sync_offset_reg;

	/*
	 * Currently active transfer settings.
	 */
	struct amd_transinfo current;
	/*
	 * Transfer settings we wish to achieve
	 * through negotiation.
	 */
	struct amd_transinfo goal;
	/*
	 * User defined or default transfer settings.
	 */
	struct amd_transinfo user;
};

/*
 * Scatter/Gather Segment entry.
 */
struct amd_sg {
	u_int32_t   SGXLen;
	u_int32_t   SGXPtr;
};

/*
 * Chipset feature limits
 */
#define MAX_SCSI_ID		8
#define AMD_MAX_SYNC_OFFSET	15
#define AMD_TARGET_MAX	7
#define AMD_LUN_MAX		7
#define AMD_NSEG		(btoc(MAXPHYS) + 1)
#define AMD_MAXTRANSFER_SIZE	0xFFFFFF /* restricted by 24 bit counter */
#define MAX_DEVICES		10
#define MAX_TAGS_CMD_QUEUE	256
#define MAX_CMD_PER_LUN		6
#define MAX_SRB_CNT		256
#define MAX_START_JOB		256

/*
 * BIT position to integer mapping.
 */
#define BIT(N) (0x01 << N)

/*
 * EEPROM storage offsets and data structures.
 */
typedef struct _EEprom {
	u_int8_t   EE_MODE1;
	u_int8_t   EE_SPEED;
	u_int8_t   xx1;
	u_int8_t   xx2;
}       EEprom, *PEEprom;

#define EE_ADAPT_SCSI_ID	64
#define EE_MODE2		65
#define EE_DELAY		66
#define EE_TAG_CMD_NUM		67
#define EE_DATA_SIZE		128
#define EE_CHECKSUM		0x1234

/*
 * EE_MODE1 bits definition
 */
#define PARITY_CHK   	  	BIT(0)
#define SYNC_NEGO      		BIT(1)
#define EN_DISCONNECT  		BIT(2)
#define SEND_START     		BIT(3)
#define TAG_QUEUING    		BIT(4)

/*
 * EE_MODE2 bits definition
 */
#define MORE2_DRV		BIT(0)
#define GREATER_1G		BIT(1)
#define RST_SCSI_BUS		BIT(2)
#define ACTIVE_NEGATION		BIT(3)
#define NO_SEEK			BIT(4)
#define LUN_CHECK		BIT(5)

#define ENABLE_CE		1
#define DISABLE_CE		0
#define EEPROM_READ		0x80

#define AMD_TAG_WILDCARD ((u_int)(~0))

/*
 * SCSI Request Block
 */
struct amd_srb {
	TAILQ_ENTRY(amd_srb) links;
	u_int8_t	 CmdBlock[12];
	union		 ccb *pccb;
	bus_dmamap_t	 dmamap;
	struct		 amd_sg *pSGlist;

	u_int32_t	 TotalXferredLen;
	u_int32_t	 SGPhysAddr;	/* a segment starting address */
	u_int32_t	 SGToBeXferLen;	/* to be xfer length */
	u_int32_t	 Segment0[2];
	u_int32_t	 Segment1[2];

	struct		 amd_sg SGsegment[AMD_NSEG];
	struct		 amd_sg Segmentx;/* a one entry of S/G list table */
	u_int8_t	*pMsgPtr;
	u_int16_t	 SRBState;

	u_int8_t	 AdaptStatus;
	u_int8_t	 TargetStatus;
	u_int8_t	 MsgCnt;
	u_int8_t	 EndMessage;
	u_int8_t	 TagNumber;
	u_int8_t	 SGcount;
	u_int8_t	 SGIndex;
	u_int8_t	 IORBFlag;	/* ;81h-Reset, 2-retry */

	u_int8_t	 SRBStatus;
	u_int8_t	 SRBFlag;
	/* ; b0-AutoReqSense,b6-Read,b7-write */
	/* ; b4-settimeout,b5-Residual valid */
	u_int8_t	 ScsiCmdLen;
};

TAILQ_HEAD(srb_queue, amd_srb);

/*
 * Per-adapter, software configuration.
 */
struct amd_softc {
	device_t		dev;
	bus_space_tag_t		tag;
	bus_space_handle_t	bsh;
	bus_dma_tag_t		buffer_dmat;   /* dmat for buffer I/O */  
	int			unit;

	int	   last_phase;
	int	   cur_target;
	int	   cur_lun;
	struct	   amd_srb *active_srb;
	struct	   amd_srb *untagged_srbs[AMD_TARGET_MAX+1][AMD_LUN_MAX+1];
	struct	   amd_target_info tinfo[AMD_TARGET_MAX+1];
	u_int16_t  disc_count[AMD_TARGET_MAX+1][AMD_LUN_MAX+1];

	struct	   srb_queue free_srbs;
	struct	   srb_queue waiting_srbs;
	struct	   srb_queue running_srbs;

	struct	   amd_srb *pTmpSRB;

	u_int16_t  SRBCount;

	u_int16_t  max_id;
	u_int16_t  max_lun;

	/* Hooks into the CAM XPT */
	struct	   cam_sim *psim;
	struct	   cam_path *ppath;

	u_int8_t   msgin_buf[6];
	u_int8_t   msgout_buf[6];
	u_int	   msgin_index;
	u_int	   msgout_index;
	u_int	   msgout_len;

	u_int8_t   status;
	u_int8_t   AdaptSCSIID;		/* ; Adapter SCSI Target ID */
	u_int8_t   AdaptSCSILUN;	/* ; Adapter SCSI LUN */

	u_int8_t   ACBFlag;

	u_int8_t   Gmode2;

	u_int8_t   HostID_Bit;

	u_int8_t   InitDCB_flag[8][8];	/* flag of initDCB for device */
	struct	   amd_srb SRB_array[MAX_SRB_CNT]; /* +45Ch, Len=	 */
	struct	   amd_srb TmpSRB;
	/* Setup data stored in an 93c46 serial eeprom */
	u_int8_t   eepromBuf[EE_DATA_SIZE];
};

/*
 *   ----SRB State machine definition
 */
#define SRB_FREE        	0
#define SRB_READY       	BIT(1)
#define SRB_MSGOUT      	BIT(2)	/* ;arbitration+msg_out 1st byte */
#define SRB_MSGIN       	BIT(3)
#define SRB_MSGIN_MULTI		BIT(4)
#define SRB_COMMAND     	BIT(5)
#define SRB_START	     	BIT(6)	/* ;arbitration+msg_out+command_out */
#define SRB_DISCONNECT   	BIT(7)
#define SRB_DATA_XFER    	BIT(8)
#define SRB_XFERPAD     	BIT(9)
#define SRB_STATUS      	BIT(10)
#define SRB_COMPLETED    	BIT(11)
#define SRB_ABORT_SENT   	BIT(12)
#define DO_SYNC_NEGO    	BIT(13)
#define SRB_UNEXPECT_RESEL	BIT(14)

/*
 *   ---ACB Flag
 */
#define RESET_DEV       	BIT(0)
#define RESET_DETECT    	BIT(1)
#define RESET_DONE      	BIT(2)

/*
 *   ---DCB Flag
 */
#define ABORT_DEV_      	BIT(0)

/*
 *   ---SRB status
 */
#define SRB_OK	        	BIT(0)
#define ABORTION        	BIT(1)
#define OVER_RUN        	BIT(2)
#define UNDER_RUN       	BIT(3)
#define PARITY_ERROR    	BIT(4)
#define SRB_ERROR       	BIT(5)

/*
 *   ---SRB Flags
 */
#define DATAOUT         	BIT(7)
#define DATAIN	        	BIT(6)
#define RESIDUAL_VALID   	BIT(5)
#define ENABLE_TIMER    	BIT(4)
#define RESET_DEV0      	BIT(2)
#define ABORT_DEV       	BIT(1)
#define AUTO_REQSENSE    	BIT(0)

/*
 *   ---Adapter status
 */
#define H_STATUS_GOOD		0
#define H_SEL_TIMEOUT		0x11
#define H_OVER_UNDER_RUN	0x12
#define H_UNEXP_BUS_FREE	0x13
#define H_TARGET_PHASE_F	0x14
#define H_INVALID_CCB_OP	0x16
#define H_LINK_CCB_BAD		0x17
#define H_BAD_TARGET_DIR	0x18
#define H_DUPLICATE_CCB		0x19
#define H_BAD_CCB_OR_SG		0x1A
#define H_ABORT			0x0FF

/*
 * AMD specific "status" codes returned in the SCSI status byte.
 */
#define AMD_SCSI_STAT_UNEXP_BUS_F    	0xFD	/* ;  Unexpect Bus Free */
#define AMD_SCSI_STAT_BUS_RST_DETECT	0xFE	/* ;  Scsi Bus Reset detected */
#define AMD_SCSI_STAT_SEL_TIMEOUT   	0xFF	/* ;  Selection Time out */

/*
 *   ---Sync_Mode
 */
#define SYNC_DISABLE	    0
#define SYNC_ENABLE 	    BIT(0)
#define SYNC_NEGO_DONE	    BIT(1)
#define WIDE_ENABLE 	    BIT(2)
#define WIDE_NEGO_DONE	    BIT(3)
#define EN_TAG_QUEUING	    BIT(4)
#define EN_ATN_STOP         BIT(5)

#define SYNC_NEGO_OFFSET    15

/*
 *    ---SCSI bus phase
 */
#define SCSI_DATA_OUT		0
#define SCSI_DATA_IN		1
#define SCSI_COMMAND		2
#define SCSI_STATUS		3
#define SCSI_NOP0		4
#define SCSI_ARBITRATING	5
#define SCSI_MSG_OUT		6
#define SCSI_MSG_IN		7
#define SCSI_BUS_FREE		8

/*
 *==========================================================
 *      	AMD 53C974 Registers bit Definition
 *==========================================================
 */

/*
 *      ------SCSI Register-------
 *      Command Reg.(+0CH)
 */
#define DMA_COMMAND   	    	BIT(7)
#define NOP_CMD 	       	0
#define CLEAR_FIFO_CMD	    	1
#define RST_DEVICE_CMD	    	2
#define RST_SCSI_BUS_CMD    	3
#define INFO_XFER_CMD	    	0x10
#define INITIATOR_CMD_CMPLTE	0x11
#define MSG_ACCEPTED_CMD    	0x12
#define XFER_PAD_BYTE	     	0x18
#define SET_ATN_CMD	       	0x1A
#define RESET_ATN_CMD    	0x1B
#define SEL_W_ATN		0x42
#define SEL_W_ATN_STOP	    	0x43
#define EN_SEL_RESEL	    	0x44
#define SEL_W_ATN2	       	0x46
#define DATA_XFER_CMD	    	INFO_XFER_CMD


/*
 *     ------SCSI Register-------
 *     SCSI Status Reg.(+10H)
 */
#define INTERRUPT	    	BIT(7)
#define ILLEGAL_OP_ERR		BIT(6)
#define PARITY_ERR	    	BIT(5)
#define COUNT_2_ZERO		BIT(4)
#define GROUP_CODE_VALID	BIT(3)
#define SCSI_PHASE_MASK 	(BIT(2)+BIT(1)+BIT(0))

/*
 *     ------SCSI Register-------
 *     Interrupt Status Reg.(+14H)
 */
#define SCSI_RESET_	    	BIT(7)
#define INVALID_CMD	    	BIT(6)
#define DISCONNECTED		BIT(5)
#define SERVICE_REQUEST 	BIT(4)
#define SUCCESSFUL_OP		BIT(3)
#define RESELECTED	    	BIT(2)
#define SEL_ATTENTION		BIT(1)
#define SELECTED	    	BIT(0)

/*
 *     ------SCSI Register-------
 *    Internal State Reg.(+18H)
 */
#define SYNC_OFFSET_FLAG	BIT(3)
#define INTRN_STATE_MASK	(BIT(2)+BIT(1)+BIT(0))

/*
 *     ------SCSI Register-------
 *     Clock Factor Reg.(+24H)
 */
#define CLK_FREQ_40MHZ		0
#define CLK_FREQ_35MHZ		(BIT(2)+BIT(1)+BIT(0))
#define CLK_FREQ_30MHZ		(BIT(2)+BIT(1))
#define CLK_FREQ_25MHZ		(BIT(2)+BIT(0))
#define CLK_FREQ_20MHZ		BIT(2)
#define CLK_FREQ_15MHZ		(BIT(1)+BIT(0))
#define CLK_FREQ_10MHZ		BIT(1)

/*
 *     ------SCSI Register-------
 *     Control Reg. 1(+20H)
 */
#define EXTENDED_TIMING 	BIT(7)
#define DIS_INT_ON_SCSI_RST	BIT(6)
#define PARITY_ERR_REPO 	BIT(4)
#define SCSI_ID_ON_BUS		(BIT(2)+BIT(1)+BIT(0))

/*
 *     ------SCSI Register-------
 *     Control Reg. 2(+2CH)
 */
#define EN_FEATURE	    	BIT(6)
#define EN_SCSI2_CMD		BIT(3)

/*
 *     ------SCSI Register-------
 *     Control Reg. 3(+30H)
 */
#define ID_MSG_CHECK		BIT(7)
#define EN_QTAG_MSG	    	BIT(6)
#define EN_GRP2_CMD	    	BIT(5)
#define FAST_SCSI	    	BIT(4)	/* ;10MB/SEC */
#define FAST_CLK	    	BIT(3)	/* ;25 - 40 MHZ */

/*
 *     ------SCSI Register-------
 *     Control Reg. 4(+34H)
 */
#define EATER_12NS	    	0
#define EATER_25NS	    	BIT(7)
#define EATER_35NS	    	BIT(6)
#define EATER_0NS	    	(BIT(7)+BIT(6))
#define NEGATE_REQACKDATA	BIT(2)
#define NEGATE_REQACK		BIT(3)

/*
 *========================================
 *             DMA Register
 *========================================
 */

/*
 *        -------DMA Register--------
 *        DMA Command Reg.(+40H)
 */
#define READ_DIRECTION		BIT(7)
#define WRITE_DIRECTION 	0
#define EN_DMA_INT	    	BIT(6)
#define MAP_TO_MDL	    	BIT(5)
#define DMA_DIAGNOSTIC		BIT(4)
#define DMA_IDLE_CMD		0
#define DMA_BLAST_CMD		BIT(0)
#define DMA_ABORT_CMD		BIT(1)
#define DMA_START_CMD		(BIT(1)|BIT(0))

/*
 *        -------DMA Register--------
 *         DMA Status Reg.(+54H)
 */
#define PCI_MS_ABORT		BIT(6)
#define BLAST_COMPLETE		BIT(5)
#define SCSI_INTERRUPT		BIT(4)
#define DMA_XFER_DONE		BIT(3)
#define DMA_XFER_ABORT		BIT(2)
#define DMA_XFER_ERROR		BIT(1)
#define POWER_DOWN	    	BIT(0)

/*
 *        -------DMA Register--------
 *        DMA SCSI Bus and Ctrl.(+70H)
 *        EN_INT_ON_PCI_ABORT
 */

/*
 *==========================================================
 *           SCSI Chip register address offset
 *==========================================================
 */
#define CTCREG_LOW   	0x00	/* (R)   current transfer count register low */
#define STCREG_LOW   	0x00	/* (W)   start transfer count register low */

#define CTCREG_MID   	0x04	/* (R)   current transfer count register
				 * middle */
#define STCREG_MID   	0x04	/* (W)   start transfer count register middle */

#define SCSIFIFOREG    	0x08	/* (R/W) SCSI FIFO register */

#define SCSICMDREG     	0x0C	/* (R/W) SCSI command register */

#define SCSISTATREG  	0x10	/* (R)   SCSI status register */
#define SCSIDESTIDREG  	0x10	/* (W)   SCSI destination ID register */

#define INTSTATREG   	0x14	/* (R)   interrupt status register */
#define SCSITIMEOUTREG 	0x14	/* (W)   SCSI timeout register */


#define INTERNSTATREG  	0x18	/* (R)   internal state register */
#define SYNCPERIOREG  	0x18	/* (W)   synchronous transfer period register */

#define CURRENTFIFOREG  0x1C	/* (R)   current FIFO/internal state register */
#define SYNCOFFREG 	    0x1C/* (W)   synchronous transfer period register */

#define CNTLREG1    	0x20	/* (R/W) control register 1 */
#define CLKFACTREG  	0x24	/* (W)   clock factor register */
#define CNTLREG2    	0x2C	/* (R/W) control register 2 */
#define CNTLREG3    	0x30	/* (R/W) control register 3 */
#define CNTLREG4    	0x34	/* (R/W) control register 4 */

#define CURTXTCNTREG  	0x38	/* (R)   current transfer count register
				 * high/part-unique ID code */
#define STCREG_HIGH  	0x38	/* (W)   Start current transfer count register
				 * high */

/*
 *********************************************************
 *
 *                 SCSI DMA register
 *
 *********************************************************
 */
#define DMA_Cmd     	0x40	/* (R/W) command register */
#define DMA_XferCnt  	0x44	/* (R/W) starting transfer count */
#define DMA_XferAddr	0x48	/* (R/W) starting Physical address */
#define DMA_Wk_ByteCntr 0x4C	/* ( R ) working byte counter */
#define DMA_Wk_AddrCntr 0x50	/* ( R ) working address counter */
#define DMA_Status   	0x54	/* ( R ) status register */
#define DMA_MDL_Addr	0x58	/* (R/W) starting memory descriptor list (MDL)
				 * address */
#define DMA_Wk_MDL_Cntr 0x5C	/* ( R ) working MDL counter */
#define DMA_ScsiBusCtrl 0x70	/* (bits R/W) SCSI BUS and control */

/* ******************************************************* */
#define am_target    	SCSISTATREG
#define am_timeout   	INTSTATREG
#define am_seq_step 	SYNCPERIOREG
#define am_fifo_count	SYNCOFFREG


#define amd_read8(amd, port)				\
	bus_space_read_1((amd)->tag, (amd)->bsh, port)

#define amd_read16(amd, port)				\
	bus_space_read_2((amd)->tag, (amd)->bsh, port)

#define amd_read32(amd, port)				\
	bus_space_read_4((amd)->tag, (amd)->bsh, port)

#define amd_write8(amd, port, value)			\
	bus_space_write_1((amd)->tag, (amd)->bsh, port, value)

#define amd_write8_multi(amd, port, ptr, len)		\
	bus_space_write_multi_1((amd)->tag, (amd)->bsh, port, ptr, len)

#define amd_write16(amd, port, value)			\
	bus_space_write_2((amd)->tag, (amd)->bsh, port, value)

#define amd_write32(amd, port, value)			\
	bus_space_write_4((amd)->tag, (amd)->bsh, port, value)

#endif /* AMD_H */
