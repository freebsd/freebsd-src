/*
 * Definitions for low level routines and data structures
 * for the Advanced Systems Inc. SCSI controllers chips.
 *
 * Copyright (c) 1996 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 *
 *      $FreeBSD$
 */
/*
 * Ported from:
 * advansys.c - Linux Host Driver for AdvanSys SCSI Adapters
 *     
 * Copyright (c) 1995-1996 Advanced System Products, Inc.
 * All Rights Reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

typedef u_int8_t target_bit_vector;
#define	TARGET_BIT_VECTOR_SET -1
#define ADV_SCSI_ID_BITS  3
#define ADV_MAX_TID	  7

/* Enumeration of board types */
typedef enum {
	ADV_NONE	= 0x000,
        ADV_ISA		= 0x001,
        ADV_ISAPNP	= 0x003,
        ADV_VL		= 0x004,
        ADV_EISA	= 0x008,
        ADV_PCI		= 0x010
}adv_btype;

typedef enum {
	ADV_STATE_NONE = 0x000
}adv_state;

#define ADV_SYN_XFER_NO			8
#define ADV_SYN_MAX_OFFSET		0x0F
#define ADV_DEF_SDTR_OFFSET		0x0F
#define ADV_DEF_SDTR_INDEX		0x00
#define ADV_OVERRUN_BSIZE		0x00000048
#define ADV_MAX_CDB_LEN			12
#define ADV_MAX_SENSE_LEN		32
#define ADV_MIN_SENSE_LEN		14

#define ADV_TIDLUN_TO_IX(tid, lun) ((tid) | ((lun) << ADV_SCSI_ID_BITS) )
#define ADV_TID_TO_TARGET_ID(tid)  (0x01 << (tid))
#define ADV_TIX_TO_TARGET_ID(tix)  (0x01 << ((tix) & ADV_MAX_TID))
#define ADV_TIX_TO_TID(tix)  ((tix) & ADV_MAX_TID)
#define ADV_TID_TO_TIX(tid)  ((tid) & ADV_MAX_TID)
#define ADV_TIX_TO_LUN(tix)  (((tix) >> ADV_SCSI_ID_BITS) & ADV_MAX_LUN )

#define ADV_INB(adv, offset)			\
	inb((adv)->iobase + (offset))
#define ADV_INW(adv, offset)			\
	inw((adv)->iobase + (offset))
#define ADV_INSB(adv, offset, valp, size)	\
	insb((adv)->iobase + (offset), (valp), (size))
#define ADV_INSW(adv, offset, valp, size)	\
	insw((adv)->iobase + (offset), (valp), (size))
#define ADV_INSL(adv, offset, valp, size)	\
	insl((adv)->iobase + (offset), (valp), (size))		
#define ADV_OUTB(adv, offset, val)		\
	outb((adv)->iobase + (offset), (val))
#define ADV_OUTW(adv, offset, val)		\
	outw((adv)->iobase + (offset), (val))
#define ADV_OUTSB(adv, offset, valp, size)	\
	outsb((adv)->iobase + (offset), (valp), (size))
#define ADV_OUTSW(adv, offset, valp, size)	\
	outsw((adv)->iobase + (offset), (valp), (size))	
#define ADV_OUTSL(adv, offset, valp, size)	\
	outsl((adv)->iobase + (offset), (valp), (size))

/*
 * XXX
 * PnP port addresses
 * I believe that these are standard PnP address and should be replaced
 * by the values in a central ISA PnP header file when we get one.
 */
#define ADV_ISA_PNP_PORT_ADDR  (0x279)
#define ADV_ISA_PNP_PORT_WRITE (ADV_ISA_PNP_PORT_ADDR+0x800)
	
/*
 * Board Signatures
 */
#define ADV_SIGNATURE_WORD		0x0000
#define		 ADV_1000_ID0W		0x04C1
#define		 ADV_1000_ID0W_FIX	0x00C1

#define	ADV_SIGNATURE_BYTE		0x0001
#define		 ADV_1000_ID1B		0x25	

/*
 * Chip Revision Number
 */
#define	ADV_NONEISA_CHIP_REVISION		0x0003
#define		ADV_CHIP_MIN_VER_VL	 	0x01
#define		ADV_CHIP_MAX_VER_VL	 	0x07

#define		ADV_CHIP_MIN_VER_PCI	 	0x09
#define		ADV_CHIP_MAX_VER_PCI	 	0x0F
#define		ADV_CHIP_VER_PCI_BIT	  	0x08

#define		ADV_CHIP_MIN_VER_ISA		0x11
#define		ADV_CHIP_MIN_VER_ISA_PNP	0x21
#define		ADV_CHIP_MAX_VER_ISA     	0x27
#define		ADV_CHIP_VER_ISA_BIT     	0x30
#define		ADV_CHIP_VER_ISAPNP_BIT  	0x20

#define		ADV_CHIP_VER_ASYN_BUG	 	0x21

#define		ADV_CHIP_MIN_VER_EISA 	 	0x41
#define		ADV_CHIP_MAX_VER_EISA	 	0x47
#define		ADV_CHIP_VER_EISA_BIT		0x40

#define ADV_HALTCODE_W				0x0040
#define ADV_STOP_CODE_B				0x0034
#define		ADV_STOP_REQ_RISC_STOP		0x01
#define		ADV_STOP_ACK_RISC_STOP		0x03

#define	ADV_CHIP_CTRL				0x000F
#define		ADV_CC_CHIP_RESET		0x80
#define		ADV_CC_SCSI_RESET		0x40
#define		ADV_CC_HALT			0x20
#define		ADV_CC_SINGLE_STEP		0x10	
#define		ADV_CC_TEST			0x04
#define		ADV_CC_BANK_ONE			0x02	
#define		ADV_CC_DIAG			0x01
	
#define ADV_CHIP_STATUS				0x000E
#define 	ADV_CSW_TEST1			0x8000
#define 	ADV_CSW_AUTO_CONFIG		0x4000
#define 	ADV_CSW_RESERVED1		0x2000
#define 	ADV_CSW_IRQ_WRITTEN		0x1000
#define 	ADV_CSW_33MHZ_SELECTED		0x0800
#define 	ADV_CSW_TEST2			0x0400
#define 	ADV_CSW_TEST3			0x0200
#define 	ADV_CSW_RESERVED2		0x0100
#define 	ADV_CSW_DMA_DONE		0x0080
#define		ADV_CSW_FIFO_RDY		0x0040
#define 	ADV_CSW_EEP_READ_DONE		0x0020
#define		ADV_CSW_HALTED			0x0010
#define		ADV_CSW_SCSI_RESET_ACTIVE	0x0008
#define		ADV_CSW_PARITY_ERR		0x0004
#define		ADV_CSW_SCSI_RESET_LATCH	0x0002
#define		ADV_CSW_INT_PENDING		0x0001
/*
 * XXX I don't understand the relevence of the naming
 * convention change here.  What does CIW stand for?
 * Perhaps this is to differentiate read and write
 * values?
 */
#define		ADV_CIW_INT_ACK			0x0100
#define		ADV_CIW_TEST1			0x0200
#define		ADV_CIW_TEST2			0x0400
#define		ADV_CIW_SEL_33MHZ		0x0800
#define		ADV_CIW_IRQ_ACT			0x1000
	
#define	ADV_REG_IH				0x0002
#define		ADV_INS_HALTINT			0x6281
#define		ADV_INS_HALT			0x6280
#define		ADV_INS_SINT			0x6200
#define		ADV_INS_RFLAG_WTM		0x7380


#define ADV_REG_SC				0x0009
	
#define	ADV_REG_PROG_COUNTER			0x000C
#define		ADV_MCODE_START_ADDR		0x0080

#define ADV_CONFIG_LSW				0x0002
#define		ADV_CFG_LSW_HOST_INT_ON		0x0020
#define		ADV_CFG_LSW_BIOS_ON		0x0040
#define		ADV_CFG_LSW_VERA_BURST_ON	0x0080
#define		ADV_CFG_LSW_SCSI_PARITY_ON	0x0800

#define ADV_CONFIG_MSW				0x0004
#define		ADV_CFG_MSW_SCSI_TARGET_ON	0x0080
#define		ADV_CFG_MSW__LRAM_8BITS_ON	0x0800
#define		ADV_CFG_MSW_CLR_MASK		0xF0C0


#define	ADV_EEPROM_DATA				0x0006

#define ADV_EEPROM_CMD				0x0007
#define		ADV_EEPROM_CMD_READ		0x80
#define		ADV_EEPROM_CMD_WRITE		0x40
#define		ADV_EEPROM_CMD_WRITE_ENABLE	0x30
#define		ADV_EEPROM_CMD_WRITE_DISABLE	0x00	

/*
 * EEPROM routine constants
 * XXX What about wide controllers?
 * Surely they have space for 8 more targets.
 */	
#define	ADV_EEPROM_CFG_BEG_VL		2
#define	ADV_EEPROM_MAX_ADDR_VL		15
#define	ADV_EEPROM_CFG_BEG		32
#define	ADV_EEPROM_MAX_ADDR		45
#define	ADV_EEPROM_MAX_RETRY		20
	
struct adv_eeprom_config {
	u_int16_t	cfg_lsw;
	
	u_int16_t	cfg_msw;

	u_int8_t	init_sdtr;
	u_int8_t	disc_enable;

	u_int8_t	use_cmd_qng;
	u_int8_t        start_motor;
	
	u_int8_t	max_total_qng;
	u_int8_t	max_tag_qng;
	
	u_int8_t	bios_scan;
	u_int8_t	power_up_wait;

	u_int8_t	no_scam;
	u_int8_t        scsi_id_dma_speed;
#define		EEPROM_SCSI_ID_MASK	0x0F
#define		EEPROM_DMA_SPEED_MASK	0xF0
#define		EEPROM_DMA_SPEED(ep) (((ep).scsi_id_dma_speed & EEPROM_DMA_SPEED_MASK) >> 8)
#define		EEPROM_SCSIID(ep) ((ep).scsi_id_dma_speed & EEPROM_SCSI_ID_MASK)
#define		EEPROM_SET_SCSIID(ep, id) ((ep).scsi_id_dma_speed &= EEPROM_SCSI_ID_MASK; \
					   (ep).scsi_id_dma_speed |= ((id) & EEPROM_SCSI_ID_MASK))
	/* XXX What about wide controllers??? */
	u_int8_t	sdtr_data[8];
	u_int8_t	adapter_info[6];

	u_int16_t	cntl;

	u_int16_t	chksum;
};

/*
 * Instruction data and code segment addresses,
 * and transaction address translation (queues).
 * All addresses refer to on board LRAM.
 */
#define ADV_DATA_SEC_BEG		0x0080
#define ADV_DATA_SEC_END		0x0080
#define ADV_CODE_SEC_BEG		0x0080
#define ADV_CODE_SEC_END		0x0080
#define ADV_QADR_BEG			0x4000
#define ADV_QADR_END			0x7FFF
#define ADV_QLAST_ADR			0x7FC0
#define ADV_QBLK_SIZE			0x40
#define ADV_BIOS_DATA_QBEG		0xF8
#define ADV_MAX_QNO			0xF8
#define ADV_QADR_USED (ADV_MAX_QNO * 64)
#define ADV_QNO_TO_QADDR(q_no) ((ADV_QADR_BEG) + ((u_int16_t)(q_no) << 6))

#define ADV_MIN_ACTIVE_QNO		0x01
#define ADV_QLINK_END			0xFF

#define ADV_MAX_SG_QUEUE		5
#define ADV_SG_LIST_PER_Q		7
#define ADV_MAX_SG_LIST			(1 + ((ADV_SG_LIST_PER_Q) * (ADV_MAX_SG_QUEUE)))

#define ADV_MIN_REMAIN_Q		0x02
#define ADV_DEF_MAX_TOTAL_QNG		0x40
#define ADV_MIN_TAG_Q_PER_DVC		0x04
#define ADV_DEF_TAG_Q_PER_DVC		0x04
#define ADV_MIN_FREE_Q			ADV_MIN_REMAIN_Q
#define ADV_MIN_TOTAL_QNG		((ADV_MAX_SG_QUEUE)+(ADV_MIN_FREE_Q))
#define ADV_MAX_TOTAL_QNG		240
#define ADV_MAX_INRAM_TAG_QNG		16
#define ADV_MAX_PCI_INRAM_TOTAL_QNG	20

#define ADV_DEF_IRQ_NO			10
#define ADV_MAX_IRQ_NO			15
#define ADV_MIN_IRQ_NO			10

#define ADV_SCSIQ_CPY_BEG		4
#define ADV_SCSIQ_SGHD_CPY_BEG		2

#define ADV_SCSIQ_B_FWD			0
#define ADV_SCSIQ_B_BWD			1

#define ADV_SCSIQ_B_STATUS		2
#define ADV_SCSIQ_B_QNO			3

#define ADV_SCSIQ_B_CNTL		4
#define ADV_SCSIQ_B_SG_QUEUE_CNT	5

#define	ADV_LRAM_ADDR			0x000A
#define	ADV_LRAM_DATA			0x0008

#define ADV_SYN_OFFSET			0x000B

/* LRAM Offsets */
#define ADVV_MSGOUT_BEG			0x0000
#define ADVV_MSGOUT_SDTR_PERIOD		(ADVV_MSGOUT_BEG+3)
#define ADVV_MSGOUT_SDTR_OFFSET		(ADVV_MSGOUT_BEG+4)

#define ADVV_MSGIN_BEG			(ADVV_MSGOUT_BEG+8)
#define ADVV_MSGIN_SDTR_PERIOD		(ADVV_MSGIN_BEG+3)
#define ADVV_MSGIN_SDTR_OFFSET		(ADVV_MSGIN_BEG+4)

#define ADVV_SDTR_DATA_BEG		(ADVV_MSGIN_BEG+8)
#define ADVV_SDTR_DONE_BEG		(ADVV_SDTR_DATA_BEG+8)
#define ADVV_MAX_DVC_QNG_BEG		0x0020

#define ADVV_ASCDVC_ERR_CODE_W		0x0030
#define ADVV_MCODE_CHKSUM_W		0x0032
#define ADVV_MCODE_SIZE_W		0x0034
#define ADVV_STOP_CODE_B		0x0036
#define ADVV_DVC_ERR_CODE_B		0x0037

#define ADVV_OVERRUN_PADDR_D		0x0038
#define ADVV_OVERRUN_BSIZE_D		0x003C

#define ADVV_HALTCODE_W			0x0040
#define		ADV_HALT_EXTMSG_IN      0x8000
#define		ADV_HALT_CHK_CONDITION  0x8100
#define		ADV_HALT_SS_QUEUE_FULL  0x8200
#define		ADV_HALT_SDTR_REJECTED	0x4000

#define ADVV_CHKSUM_W			0x0042
#define ADVV_MC_DATE_W			0x0044
#define ADVV_MC_VER_W			0x0046
#define ADVV_NEXTRDY_B			0x0048
#define ADVV_DONENEXT_B			0x0049
#define ADVV_USE_TAGGED_QNG_B		0x004A
#define ADVV_SCSIBUSY_B			0x004B
#define ADVV_CDBCNT_B			0x004C
#define ADVV_CURCDB_B			0x004D
#define ADVV_RCLUN_B			0x004E
#define ADVV_BUSY_QHEAD_B		0x004F
#define ADVV_DISC1_QHEAD_B		0x0050

#define ADVV_DISC_ENABLE_B		0x0052
#define ADVV_CAN_TAGGED_QNG_B		0x0053
#define ADVV_HOSTSCSI_ID_B		0x0055
#define ADVV_MCODE_CNTL_B		0x0056
#define ADVV_NULL_TARGET_B		0x0057

#define ADVV_FREE_Q_HEAD_W		0x0058
#define ADVV_DONE_Q_TAIL_W		0x005A
#define ADVV_FREE_Q_HEAD_B		(ADVV_FREE_Q_HEAD_W+1)
#define ADVV_DONE_Q_TAIL_B		(ADVV_DONE_Q_TAIL_W+1)

#define ADVV_HOST_FLAG_B		0x005D
#define		ADV_HOST_FLAG_IN_ISR	0x01
#define		ADV_HOST_FLAG_ACK_INT	0x02


#define ADVV_TOTAL_READY_Q_B		  0x0064
#define ADVV_VER_SERIAL_B		  0x0065
#define ADVV_HALTCODE_SAVED_W		  0x0066
#define ADVV_WTM_FLAG_B			  0x0068
#define ADVV_RISC_FLAG_B		  0x006A
#define		ADV_RISC_FLAG_GEN_INT     0x01
#define		ADV_RISC_FLAG_REQ_SG_LIST 0x02

#define ADVV_REQ_SG_LIST_QP		0x006B
struct adv_softc
{
	/* The overrun buffer must be quad word aligned */
	u_int8_t		overrun_buf[ADV_OVERRUN_BSIZE];
	int			unit;
	u_int32_t		iobase;
	adv_btype		type;
	target_bit_vector	needs_async_bug_fix;
	target_bit_vector	initiate_sdtr;
	target_bit_vector	sdtr_done;
	target_bit_vector	cmd_qng_enabled;
	target_bit_vector	unit_not_ready;
	target_bit_vector	start_motor;
	target_bit_vector	no_scam;
	target_bit_vector	disc_enable;
	u_int16_t		control;
#define		ADV_CNTL_INITIATOR		0x0001
#define		ADV_CNTL_BIOS_GT_1GB		0x0002
#define		ADV_CNTL_BIOS_GT_2_DISK		0x0004
#define		ADV_CNTL_BIOS_REMOVABLE		0x0008
#define		ADV_CNTL_NO_SCAM		0x0010
#define		ADV_CNTL_NO_PCI_FIX_ASYN_XFER	0x0020
#define		ADV_CNTL_INT_MULTI_Q		0x0080
#define		ADV_CNTL_NO_LUN_SUPPORT		0x0040
#define		ADV_CNTL_NO_VERIFY_COPY		0x0100
#define		ADV_CNTL_RESET_SCSI		0x0200
#define		ADV_CNTL_INIT_INQUIRY		0x0400
#define		ADV_CNTL_INIT_VERBOSE		0x0800
#define		ADV_CNTL_SCSI_PARITY		0x1000
#define		ADV_CNTL_BURST_MODE		0x2000
#define		ADV_CNTL_USE_8_IOP_BASE		0x4000
	
	u_int16_t		bug_fix_control;
#define		ADV_BUG_FIX_ADD_ONE_BYTE	0x0001
	
	adv_state		state;
	u_int32_t		max_dma_count;
	u_int8_t		isa_dma_speed;
	u_int8_t		scsi_id;
	u_int8_t		chip_version;
	u_int8_t		max_tags_per_target;
	u_int8_t		max_openings;
	u_int8_t		cur_active;
	u_int8_t		openings_needed;
	u_int8_t		sdtr_data[16];
	struct scsi_sense_data	*sense_buffers;
};

/*
 * Structures for talking to the RISC engine.
 */
struct adv_scsiq_1 {
	u_int8_t		status;
#define		QS_FREE        0x00
#define		QS_READY       0x01
#define		QS_DISC1       0x02
#define		QS_DISC2       0x04
#define		QS_BUSY        0x08
#define		QS_ABORTED     0x40
#define		QS_DONE        0x80
	
	u_int8_t		q_no;		/*
						 * Queue ID of the first queue
						 * used in this transaction.
						 */
	u_int8_t		cntl;
#define		QC_NO_CALLBACK   0x01
#define		QC_SG_SWAP_QUEUE 0x02
#define		QC_SG_HEAD       0x04
#define		QC_DATA_IN       0x08
#define		QC_DATA_OUT      0x10
#define		QC_URGENT        0x20
#define		QC_MSG_OUT       0x40
#define		QC_REQ_SENSE     0x80
	
	u_int8_t		sg_queue_cnt;	/* Number of SG entries */

	u_int8_t		target_id;	/* target id as a bit vector */
	u_int8_t		target_lun;	/* LUN - taken from our xs */

	u_int32_t		data_addr;	/*
						 * physical addres of first
						 * (possibly only) segment
						 * to transfer.
						 */
	u_int32_t		data_cnt;	/*
						 * byte count of the first
						 * (possibly only) segment
						 * to transfer.
						 */
	u_int32_t		sense_addr;	/*
						 * physical address of the sense
						 * buffer.
						 */
	u_int8_t		sense_len;	/* length of sense buffer */
	u_int8_t		user_def;
};

struct adv_scsiq_2 {
	u_int32_t		xs_ptr;		/* Pointer to our scsi_xfer */
	u_int8_t		target_ix;      /* Combined TID and LUN */

	u_int8_t		flag;
	u_int8_t		cdb_len;	/*
						 * Number of bytes in the SCSI
						 * command to execute.
						 */
	u_int8_t		tag_code;	/*
						 * Tag type for this transaction
						 * (SIMPLE, ORDERED, HEAD )
						 */
#define		ADV_TAG_FLAG_ADD_ONE_BYTE     0x10
#define		ADV_TAG_FLAG_ISAPNP_ADD_BYTES 0x40
	
	u_int16_t		vm_id;
};

struct adv_scsiq_3 {
	u_int8_t		done_stat;
#define		QD_IN_PROGRESS			0x00
#define		QD_NO_ERROR			0x01
#define		QD_ABORTED_BY_HOST		0x02
#define		QD_WITH_ERROR			0x04
#define		QD_INVALID_REQUEST		0x80
#define		QD_INVALID_HOST_NUM		0x81
#define		QD_INVALID_DEVICE		0x82
#define		QD_ERR_INTERNAL			0xFF
	
	u_int8_t		host_stat;
#define		QHSTA_NO_ERROR			0x00
#define		QHSTA_M_SEL_TIMEOUT		0x11
#define		QHSTA_M_DATA_OVER_RUN		0x12
#define		QHSTA_M_DATA_UNDER_RUN		0x12
#define		QHSTA_M_UNEXPECTED_BUS_FREE	0x13
#define		QHSTA_M_BAD_BUS_PHASE_SEQ	0x14

#define		QHSTA_D_QDONE_SG_LIST_CORRUPTED	0x21
#define		QHSTA_D_ASC_DVC_ERROR_CODE_SET	0x22
#define		QHSTA_D_HOST_ABORT_FAILED	0x23
#define		QHSTA_D_EXE_SCSI_Q_FAILED	0x24
#define		QHSTA_D_EXE_SCSI_Q_BUSY_TIMEOUT	0x25
#define		QHSTA_D_ASPI_NO_BUF_POOL	0x26

#define		QHSTA_M_WTM_TIMEOUT		0x41
#define		QHSTA_M_BAD_CMPL_STATUS_IN	0x42
#define		QHSTA_M_NO_AUTO_REQ_SENSE	0x43
#define		QHSTA_M_AUTO_REQ_SENSE_FAIL	0x44
#define		QHSTA_M_TARGET_STATUS_BUSY	0x45
#define		QHSTA_M_BAD_TAG_CODE		0x46

#define		QHSTA_M_BAD_QUEUE_FULL_OR_BUSY	0x47

#define		QHSTA_D_LRAM_CMP_ERROR		0x81
	
#define		QHSTA_M_MICRO_CODE_ERROR_HALT	0xA1
	
	u_int8_t		scsi_stat;
	u_int8_t		scsi_msg;
};

struct adv_scsiq_4 {
	u_int8_t		cdb[ADV_MAX_CDB_LEN];
	u_int8_t		y_first_sg_list_qp;
	u_int8_t		y_working_sg_qp;
	u_int8_t		y_working_sg_ix;
	u_int8_t		y_cntl;
	u_int16_t		x_req_count;
	u_int16_t		x_reconnect_rtn;
	u_int32_t		x_saved_data_addr;
	u_int32_t		x_saved_data_cnt;
};

struct adv_q_done_info {
	struct adv_scsiq_2	d2;
	struct adv_scsiq_3	d3;
	u_int8_t		q_status;
	u_int8_t		q_no;
	u_int8_t		cntl;
	u_int8_t		sense_len;
	u_int8_t		user_def;
	u_int8_t		res;
	u_int32_t		remain_bytes;
};

struct adv_sg_entry {
	u_int32_t		addr;
	u_int32_t		bytes;
};

struct adv_sg_head {
	u_int8_t		entry_cnt;	/* Number of SG entries in this list */

	u_int8_t		queue_cnt;	/*
						 * Number of queues required to store
						 * entry_cnt SG entries.
						 */

	u_int8_t		entry_to_copy;	/*
						 * Number of SG entries to copy to the
						 * board.
						 */
	u_int8_t		res;
	struct adv_sg_entry	sg_list[ADV_MAX_SG_LIST];
};

#define ADV_MIN_SG_LIST   2

struct adv_min_sg_head {
	u_int8_t		entry_cnt;

	u_int8_t		queue_cnt;

	u_int8_t		entry_to_copy;
	u_int8_t		res;
	struct adv_sg_entry	sg_list[ADV_MIN_SG_LIST];
};

#define QCX_SORT        (0x0001)
#define QCX_COALEASE    (0x0002)

#if CC_LINK_BUSY_Q
struct asc_ext_scsi_q {
	u_int32_t		lba;
	u_int16_t		lba_len;
	struct adv_scsi_q	*next;
	struct adv_scsi_q	*join;
	u_int16_t		cntl;
	u_int16_t		buffer_id;
	u_int8_t		q_required;
	u_int8_t		res;
};
#endif

struct adv_scsi_q {
	struct adv_scsiq_1	q1;
	struct adv_scsiq_2	q2;
	struct scsi_generic	*cdbptr;	/*
						 * Pointer to the SCSI command
						 * to execute.
						 */

	struct adv_sg_head	*sg_head;	/*
						 * Pointer to possible SG list
						 */
#if CC_LINK_BUSY_Q
	struct adv_ext_scsi_q	 ext;
#endif

};
#define ADV_SCSIQ_D_DATA_ADDR		8
#define ADV_SCSIQ_D_DATA_CNT		12
#define ADV_SCSIQ_B_SENSE_LEN		20
#define ADV_SCSIQ_DONE_INFO_BEG		22
#define ADV_SCSIQ_D_SRBPTR		22
#define ADV_SCSIQ_B_TARGET_IX		26
#define ADV_SCSIQ_B_CDB_LEN		28
#define ADV_SCSIQ_B_TAG_CODE		29
#define ADV_SCSIQ_W_VM_ID		30
#define ADV_SCSIQ_DONE_STATUS		32
#define ADV_SCSIQ_HOST_STATUS		33
#define ADV_SCSIQ_SCSI_STATUS		34
#define ADV_SCSIQ_CDB_BEG		36
#define ADV_SCSIQ_DW_REMAIN_XFER_ADDR	56
#define ADV_SCSIQ_DW_REMAIN_XFER_CNT	60
#define ADV_SCSIQ_B_SG_WK_QP		49
#define ADV_SCSIQ_B_SG_WK_IX		50
#define ADV_SCSIQ_W_REQ_COUNT		52
#define ADV_SCSIQ_B_LIST_CNT		6
#define ADV_SCSIQ_B_CUR_LIST_CNT	7


struct adv_scsi_req_q {
	struct adv_scsiq_1	r1;
	struct adv_scsiq_2	r2;
	u_int8_t		*cdbptr;
	struct adv_sg_head	*sg_head;

#if CC_LINK_BUSY_Q
	struct adv_ext_scsi_q	ext;
#endif

	u_int8_t		*sense_ptr;

	struct adv_scsiq_3	r3;
	u_int8_t		cdb[ADV_MAX_CDB_LEN];
	u_int8_t		sense[ADV_MIN_SENSE_LEN];
};

struct adv_risc_q {
	u_int8_t		fwd;
	u_int8_t		bwd;
	struct adv_scsiq_1	i1;
	struct adv_scsiq_2	i2;
	struct adv_scsiq_3	i3;
	struct adv_scsiq_4	i4;
};

struct adv_sg_list_q {
	u_int8_t		seq_no;
	u_int8_t		q_no;
	u_int8_t		cntl;
#define		QCSG_SG_XFER_LIST  0x02
#define		QCSG_SG_XFER_MORE  0x04
#define		QCSG_SG_XFER_END   0x08
	
	u_int8_t		sg_head_qp;
	u_int8_t		sg_list_cnt;
	u_int8_t		sg_cur_list_cnt;
};
#define ADV_SGQ_B_SG_CNTL		4
#define ADV_SGQ_B_SG_HEAD_QP		5
#define ADV_SGQ_B_SG_LIST_CNT		6
#define ADV_SGQ_B_SG_CUR_LIST_CNT	7
#define ADV_SGQ_LIST_BEG		8

struct asc_risc_sg_list_q {
	u_int8_t		fwd;
	u_int8_t		bwd;
	struct adv_sg_list_q	sg;
	struct adv_sg_entry	sg_list[7];
};


/* LRAM routines */
u_int8_t  adv_read_lram_8 __P((struct adv_softc *adv, u_int16_t addr));
void	  adv_write_lram_8 __P((struct adv_softc *adv, u_int16_t addr,
				u_int8_t value));
u_int16_t adv_read_lram_16 __P((struct adv_softc *adv, u_int16_t addr));
void	  adv_write_lram_16 __P((struct adv_softc *adv,	u_int16_t addr,
				 u_int16_t value));

/* Intialization */
void	  adv_get_board_type __P((struct adv_softc *adv));
u_int16_t adv_get_eeprom_config __P((struct adv_softc *adv,
				     struct adv_eeprom_config *eeprom_config));
int	  adv_set_eeprom_config __P((struct adv_softc *adv,
				     struct adv_eeprom_config *eeprom_config));
int	  adv_reset_chip_and_scsi_bus __P((struct adv_softc *adv));
int	  adv_test_external_lram __P((struct adv_softc* adv));
int	  adv_init_lram_and_mcode __P((struct adv_softc *adv));
u_int8_t  adv_get_chip_irq __P((struct adv_softc *adv));
u_int8_t  adv_set_chip_irq __P((struct adv_softc *adv, u_int8_t irq_no));

/* Queue handling and execution */
int	  adv_execute_scsi_queue __P((struct adv_softc *adv, struct adv_scsi_q *scsiq));
u_int8_t  adv_copy_lram_doneq __P((struct adv_softc *adv, u_int16_t q_addr,
				   struct adv_q_done_info *scsiq, u_int32_t max_dma_count));

/* Chip Control */
int	  adv_stop_execution __P((struct adv_softc *adv));
int	  adv_is_chip_halted __P((struct adv_softc *adv));

/* Interrupt processing */
void	  adv_ack_interrupt __P((struct adv_softc *adv));
void	  adv_isr_chip_halted __P((struct adv_softc *adv));
