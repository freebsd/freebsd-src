/*
 * PC9801 SCSI I/F (PC-9801-55)
 * modified for PC9801 by A.Kojima
 *			Kyoto University Microcomputer Club (KMC)
 */

/* I/O address */

/* WD33C93 */
#define	SCSI_ADR_REG	0xcc0	/* write       Address Register */
#define	SCSI_AUX_REG	0xcc0	/* read        Aux. Status Register */
#define	SCSI_CTL_REG	0xcc2	/* read/write  Control Registers */

/* Port */
#define SCSI_STAT_RD	0xcc4	/* read        Status Read */
#define SCSI_CMD_WRT	0xcc4	/* write       Command Write */

#if 0 /* H98 extended mode */
/* WD33C93 */
#define	SCSI_ADR_REG	0xee0	/* write       Address Register */
#define	SCSI_AUX_REG	0xee0	/* read        Control Register */
#define	SCSI_CTL_REG	0xee2	/* read/write  Registers */

/* Port */
#define SCSI_STAT_RD	0xee4	/* read        Status Read */
#define SCSI_CMD_WRT	0xee4	/* write       Command Write */
#endif

/****************************************************************/

/* WD33C93 Registers */
#define	REG_OWN_ID		0x00	/* Own ID */
#define	REG_CONTROL		0x01	/* Control */
#define REG_TIMEOUT_PERIOD	0x02	/* Timeout Period */
#define REG_TOTAL_SECTORS	0x03	/* Total Sectors */
#define REG_TOTAL_HEADS		0x04	/* Total Heads */
#define REG_TOTAL_CYL_H		0x05	/* Total Cylinders (MSB) */
#define REG_TOTAL_CYL_L		0x06	/* Total Cylinders (LSB) */
#define REG_LOG_SECTOR_HH	0x07	/* Logical Address (MSB) */
#define REG_LOG_SECTOR_HL	0x08	/* Logical Address       */
#define REG_LOG_SECTOR_LH	0x09	/* Logical Address       */
#define REG_LOG_SECTOR_LL	0x0a	/* Logical Address (LSB) */
#define REG_SECTOR_NUMBER	0x0b	/* Sector Number */
#define REG_HEAD_NUMBER		0x0c	/* Head Number */
#define REG_CYL_NUMBER_H	0x0d	/* Cylinder Number (MSB) */
#define REG_CYL_NUMBER_L	0x0e	/* Cylinder Number (LSB) */
#define REG_TARGET_LUN		0x0f	/* Target LUN */
#define REG_CMD_PHASE		0x10	/* Command Phase */
#define REG_SYNC_TFR		0x11	/* Synchronous Transfer */
#define REG_TFR_COUNT_H		0x12	/* Transfer Count (MSB) */
#define REG_TFR_COUNT_M		0x13	/* Transfer Count      */
#define REG_TFR_COUNT_L		0x14	/* Transfer Count (LSB) */
#define REG_DST_ID		0x15	/* Destination ID */
#define REG_SRC_ID		0x16	/* Source ID */
#define REG_SCSI_STATUS		0x17	/* SCSI Status (Read Only) */
#define REG_COMMAND		0x18	/* Command */
#define REG_DATA		0x19	/* Data */

/* PC98 only */
#define REG_MEM_BANK		0x30	/* Memory Bank */
#define REG_MEM_WIN		0x31	/* Memery Window */
#define REG_RESERVED1		0x32	/* NEC Reserved 1 */
#define REG_RESET_INT		0x33	/* Reset/Int */
#define REG_RESERVED2		0x34	/* NEC Reserved 2 */

/****************************************************************/

/* WD33C93 Commands */
#define	CMD_RESET		0x00	/* Reset */
#define	CMD_ABORT		0x01	/* Abort */
#define	CMD_ASSERT_ATN		0x02	/* Assert ATN */
#define	CMD_NEGATE_ATN		0x03	/* Negate ATN */
#define	CMD_DISCONNECT		0x04	/* Disconnect */
#define	CMD_RESELECT		0x05	/* Reselect */
#define	CMD_SELECT_ATN		0x06	/* Select with ATN */
#define	CMD_SELECT_NO_ATN	0x07	/* Select without ATN */
#define	CMD_SELECT_ATN_TFR	0x08	/* Select with ATN and Transfer */ 
#define	CMD_SELECT_NO_ATN_TFR	0x09	/* Select without ATN and Transfer */ 
#define	CMD_RESELECT_RCV_DATA	0x0a	/* Reselect and Recieve Data */
#define	CMD_RESELECT_SEND_DATA	0x0b	/* Reselect and Send Data */
#define	CMD_WAIT_SELECT_RCV	0x0c	/* Wait for Select and Recieve */
#define	CMD_RCV_CMD		0x10	/* Recieve Command */
#define	CMD_RCV_DATA		0x11	/* Recieve Data */
#define	CMD_RCV_MSG_OUT		0x12	/* Recieve Message Info Out*/
#define	CMD_RCV_UNSP_INFO_OUT	0x13	/* Recieve Unspecified Info Out */
#define	CMD_SEND_STATUS		0x14	/* Send Status */
#define	CMD_SEND_DATA		0x15	/* Send Data */
#define	CMD_SEND_MSG_IN		0x16	/* Send Message In */
#define	CMD_SEND_UNSP_INFO_IN	0x17	/* Send Unspecified Info In */
#define	CMD_TRANSLATE_ADDRESS	0x18	/* Translate Address */
#define	CMD_TFR_INFO		0x20	/* Transfer Info */
#define	CMD_TFR_PAD		0x21	/* Transfer Pad */
#define CMD_SBT_SFX		0x80	/* single byte suffix */

/* WD33C93 bus status register (lower nibble) */
#define STAT_DATAOUT	0x08		/* Data out phase */
#define STAT_DATAIN	0x09		/* Data in phase */
#define STAT_CMDOUT	0x0a		/* Command out phase */
#define STAT_STATIN	0x0b		/* Status in phase */
#define STAT_MSGOUT	0x0e		/* Message out phase */
#define STAT_MSGIN	0x0f		/* Message in phase */

/* SCSI Status byte */
#define SS_GOOD		0x00		/* Good status */
#define SS_CHKCOND	0x02
#define SS_MET		0x04
#define SS_BUSY		0x08
#define SS_INTERGOOD	0x10
#define SS_INTERMET	0x14
#define SS_CONFLICT	0x18

/* SCSI message system */
#define MSG_COMPLETE	0x00		/* Command complete message */
#define MSG_EXTEND	0x01		/* Extend message */
#define MSG_SAVEPTR	0x02		/* Save data pointer message */
#define MSG_RESTORE	0x03		/* Restore data pointer message */
#define MSG_DISCON	0x04		/* Disconnect message */
#define MSG_INIERROR	0x05
#define MSG_ABORT	0x06
#define MSG_REJECT	0x07
#define MSG_NOP		0x08
#define MSG_PARERROR	0x09
#define MSG_LCOMPLETE	0x0a
#define MSG_LCOMPLETEF	0x0b
#define MSG_DEVRESET	0x0c
#define MSG_IDENTIFY	0x80		/* Identify message */

