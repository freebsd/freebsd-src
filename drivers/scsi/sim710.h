#ifndef _SIM710_H
#define _SIM710_H

/*
 * sim710.h - Copyright (C) 1999 Richard Hirst
 */

#include <linux/types.h>

int sim710_detect(Scsi_Host_Template *);
int sim710_command(Scsi_Cmnd *);
int sim710_queuecommand(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int sim710_abort(Scsi_Cmnd * SCpnt);
int sim710_bus_reset(Scsi_Cmnd * SCpnt);
int sim710_dev_reset(Scsi_Cmnd * SCpnt);
int sim710_host_reset(Scsi_Cmnd * SCpnt);
#ifdef MODULE
int sim710_release(struct Scsi_Host *);
#else
#define sim710_release	NULL
#endif

#include <scsi/scsicam.h>

#define SIM710_SCSI { proc_name:		"sim710",		\
		      name:			"53c710",	 	\
		      detect:			sim710_detect,		\
		      release:			sim710_release,		\
		      queuecommand:		sim710_queuecommand,	\
		      eh_abort_handler:		sim710_abort,		\
		      eh_device_reset_handler:	sim710_dev_reset,	\
		      eh_bus_reset_handler:	sim710_bus_reset,	\
		      eh_host_reset_handler:	sim710_host_reset,	\
		      bios_param:		scsicam_bios_param,	\
		      can_queue:		8,		 	\
		      this_id:			7, 			\
		      sg_tablesize:		128,		 	\
		      cmd_per_lun:		1,		 	\
		      use_clustering:		DISABLE_CLUSTERING,	\
		      use_new_eh_code:		1}

#ifndef HOSTS_C

#ifdef __BIG_ENDIAN
#define	bE	3	/* 0 for little endian, 3 for big endian */
#else
#define bE	0
#endif

/* SCSI control 0 rw, default = 0xc0 */
#define SCNTL0_REG 		(0x00^bE)
#define SCNTL0_ARB1		0x80	/* 0 0 = simple arbitration */
#define SCNTL0_ARB2		0x40	/* 1 1 = full arbitration */
#define SCNTL0_STRT		0x20	/* Start Sequence */
#define SCNTL0_WATN		0x10	/* Select with ATN */
#define SCNTL0_EPC		0x08	/* Enable parity checking */
/* Bit 2 is reserved on 800 series chips */
#define SCNTL0_EPG_700		0x04	/* Enable parity generation */
#define SCNTL0_AAP		0x02	/*  ATN/ on parity error */
#define SCNTL0_TRG		0x01	/* Target mode */

/* SCSI control 1 rw, default = 0x00 */

#define SCNTL1_REG 		(0x01^bE)
#define SCNTL1_EXC		0x80	/* Extra Clock Cycle of Data setup */
#define SCNTL1_ADB		0x40	/*  contents of SODL on bus */
#define SCNTL1_ESR_700		0x20	/* Enable SIOP response to selection
					   and reselection */
#define SCNTL1_CON		0x10	/* Connected */
#define SCNTL1_RST		0x08	/* SCSI RST/ */
#define SCNTL1_AESP		0x04	/* Force bad parity */
#define SCNTL1_SND_700		0x02	/* Start SCSI send */
#define SCNTL1_IARB_800		0x02	/* Immediate Arbitration, start
					   arbitration immediately after
					   busfree is detected */
#define SCNTL1_RCV_700		0x01	/* Start SCSI receive */
#define SCNTL1_SST_800		0x01	/* Start SCSI transfer */

/* SCSI control 2 rw, */

#define SCNTL2_REG_800		(0x02^bE)
#define SCNTL2_800_SDU		0x80	/* SCSI disconnect unexpected */

/* SCSI control 3 rw */

#define SCNTL3_REG_800 		(0x03^bE)
#define SCNTL3_800_SCF_SHIFT	4
#define SCNTL3_800_SCF_MASK	0x70
#define SCNTL3_800_SCF2		0x40	/* Synchronous divisor */
#define SCNTL3_800_SCF1		0x20	/* 0x00 = SCLK/3 */
#define SCNTL3_800_SCF0		0x10	/* 0x10 = SCLK/1 */
					/* 0x20 = SCLK/1.5
					   0x30 = SCLK/2
					   0x40 = SCLK/3 */

#define SCNTL3_800_CCF_SHIFT	0
#define SCNTL3_800_CCF_MASK	0x07
#define SCNTL3_800_CCF2		0x04	/* 0x00 50.01 to 66 */
#define SCNTL3_800_CCF1		0x02	/* 0x01 16.67 to 25 */
#define SCNTL3_800_CCF0		0x01	/* 0x02	25.01 - 37.5
					   0x03	37.51 - 50
					   0x04 50.01 - 66 */

/*
 * SCSI destination ID rw - the appropriate bit is set for the selected
 * target ID.  This is written by the SCSI SCRIPTS processor.
 * default = 0x00
 */
#define SDID_REG_700  		(0x02^bE)
#define SDID_REG_800  		(0x06^bE)

#define GP_REG_800		(0x07^bE) /* General purpose IO */
#define GP_800_IO1		0x02
#define GP_800_IO2		0x01

/* SCSI interrupt enable rw, default = 0x00 */
#define SIEN_REG_700		(0x03^bE)
#define SIEN0_REG_800		(0x40^bE)
#define SIEN_MA			0x80	/* Phase mismatch (ini) or ATN (tgt) */
#define SIEN_FC			0x40	/* Function complete */
#define SIEN_700_STO		0x20	/* Selection or reselection timeout */
#define SIEN_800_SEL		0x20	/* Selected */
#define SIEN_700_SEL		0x10	/* Selected or reselected */
#define SIEN_800_RESEL		0x10	/* Reselected */
#define SIEN_SGE		0x08	/* SCSI gross error */
#define SIEN_UDC		0x04	/* Unexpected disconnect */
#define SIEN_RST		0x02	/* SCSI RST/ received */
#define SIEN_PAR		0x01	/* Parity error */

/*
 * SCSI chip ID rw
 * NCR53c700 :
 * 	When arbitrating, the highest bit is used, when reselection or selection
 * 	occurs, the chip responds to all IDs for which a bit is set.
 * 	default = 0x00
 */
#define SCID_REG		(0x04^bE)
/* Bit 7 is reserved on 800 series chips */
#define SCID_800_RRE		0x40	/* Enable response to reselection */
#define SCID_800_SRE		0x20	/* Enable response to selection */
/* Bits four and three are reserved on 800 series chips */
#define SCID_800_ENC_MASK	0x07	/* Encoded SCSI ID */

/* SCSI transfer rw, default = 0x00 */
#define SXFER_REG		(0x05^bE)
#define SXFER_DHP		0x80	/* Disable halt on parity */

#define SXFER_TP2		0x40	/* Transfer period msb */
#define SXFER_TP1		0x20
#define SXFER_TP0		0x10	/* lsb */
#define SXFER_TP_MASK		0x70
/* FIXME : SXFER_TP_SHIFT == 5 is right for '8xx chips */
#define SXFER_TP_SHIFT		5
#define SXFER_TP_4		0x00	/* Divisors */
#define SXFER_TP_5		0x10<<1
#define SXFER_TP_6		0x20<<1
#define SXFER_TP_7		0x30<<1
#define SXFER_TP_8		0x40<<1
#define SXFER_TP_9		0x50<<1
#define SXFER_TP_10		0x60<<1
#define SXFER_TP_11		0x70<<1

#define SXFER_MO3		0x08	/* Max offset msb */
#define SXFER_MO2		0x04
#define SXFER_MO1		0x02
#define SXFER_MO0		0x01	/* lsb */
#define SXFER_MO_MASK		0x0f
#define SXFER_MO_SHIFT		0

/*
 * SCSI output data latch rw
 * The contents of this register are driven onto the SCSI bus when
 * the Assert Data Bus bit of the SCNTL1 register is set and
 * the CD, IO, and MSG bits of the SOCL register match the SCSI phase
 */
#define SODL_REG_700		(0x06^bE)
#define SODL_REG_800		(0x54^bE)


/*
 * SCSI output control latch rw, default = 0
 * Note that when the chip is being manually programmed as an initiator,
 * the MSG, CD, and IO bits must be set correctly for the phase the target
 * is driving the bus in.  Otherwise no data transfer will occur due to
 * phase mismatch.
 */

#define SOCL_REG		(0x07^bE)
#define SOCL_REQ		0x80	/*  REQ */
#define SOCL_ACK		0x40	/*  ACK */
#define SOCL_BSY		0x20	/*  BSY */
#define SOCL_SEL		0x10	/*  SEL */
#define SOCL_ATN		0x08	/*  ATN */
#define SOCL_MSG		0x04	/*  MSG */
#define SOCL_CD			0x02	/*  C/D */
#define SOCL_IO			0x01	/*  I/O */

/*
 * SCSI first byte received latch ro
 * This register contains the first byte received during a block MOVE
 * SCSI SCRIPTS instruction, including
 *
 * Initiator mode	Target mode
 * Message in		Command
 * Status		Message out
 * Data in		Data out
 *
 * It also contains the selecting or reselecting device's ID and our
 * ID.
 *
 * Note that this is the register the various IF conditionals can
 * operate on.
 */
#define SFBR_REG		(0x08^bE)

/*
 * SCSI input data latch ro
 * In initiator mode, data is latched into this register on the rising
 * edge of REQ/. In target mode, data is latched on the rising edge of
 * ACK/
 */
#define SIDL_REG_700		(0x09^bE)
#define SIDL_REG_800		(0x50^bE)

/*
 * SCSI bus data lines ro
 * This register reflects the instantaneous status of the SCSI data
 * lines.  Note that SCNTL0 must be set to disable parity checking,
 * otherwise reading this register will latch new parity.
 */
#define SBDL_REG_700		(0x0a^bE)
#define SBDL_REG_800		(0x58^bE)

#define SSID_REG_800		(0x0a^bE)
#define SSID_800_VAL		0x80	/* Exactly two bits asserted at sel */
#define SSID_800_ENCID_MASK	0x07	/* Device which performed operation */



/*
 * SCSI bus control lines rw,
 * instantaneous readout of control lines
 */
#define SBCL_REG		(0x0b^bE)
#define SBCL_REQ		0x80	/*  REQ ro */
#define SBCL_ACK		0x40	/*  ACK ro */
#define SBCL_BSY		0x20	/*  BSY ro */
#define SBCL_SEL		0x10	/*  SEL ro */
#define SBCL_ATN		0x08	/*  ATN ro */
#define SBCL_MSG		0x04	/*  MSG ro */
#define SBCL_CD			0x02	/*  C/D ro */
#define SBCL_IO			0x01	/*  I/O ro */
#define SBCL_PHASE_CMDOUT	SBCL_CD
#define SBCL_PHASE_DATAIN	SBCL_IO
#define SBCL_PHASE_DATAOUT	0
#define SBCL_PHASE_MSGIN	(SBCL_CD|SBCL_IO|SBCL_MSG)
#define SBCL_PHASE_MSGOUT	(SBCL_CD|SBCL_MSG)
#define SBCL_PHASE_STATIN	(SBCL_CD|SBCL_IO)
#define SBCL_PHASE_MASK		(SBCL_CD|SBCL_IO|SBCL_MSG)
/*
 * Synchronous SCSI Clock Control bits
 * 0 - set by DCNTL
 * 1 - SCLK / 1.0
 * 2 - SCLK / 1.5
 * 3 - SCLK / 2.0
 */
#define SBCL_SSCF1		0x02	/* wo, -66 only */
#define SBCL_SSCF0		0x01	/* wo, -66 only */
#define SBCL_SSCF_MASK		0x03

/*
 * XXX note : when reading the DSTAT and STAT registers to clear interrupts,
 * insure that 10 clocks elapse between the two
 */
/* DMA status ro */
#define DSTAT_REG		(0x0c^bE)
#define DSTAT_DFE		0x80	/* DMA FIFO empty */
#define DSTAT_800_MDPE		0x40	/* Master Data Parity Error */
#define DSTAT_BF		0x20	/* Bus Fault */
#define DSTAT_ABRT		0x10	/* Aborted - set on error */
#define DSTAT_SSI		0x08	/* SCRIPTS single step interrupt */
#define DSTAT_SIR		0x04	/* SCRIPTS interrupt received -
					   set when INT instruction is
					   executed */
#define DSTAT_WTD		0x02	/* Watchdog timeout detected */
#define DSTAT_OPC		0x01	/* Illegal instruction */
#define DSTAT_IID		0x01	/* Same thing, different name */


#define SSTAT0_REG		(0x0d^bE)	/* SCSI status 0 ro */
#define SIST0_REG_800		(0x42^bE)	/* SCSI status 0 ro */
#define SSTAT0_MA		0x80	/* ini : phase mismatch,
					 * tgt : ATN/ asserted
					 */
#define SSTAT0_CMP		0x40	/* function complete */
#define SSTAT0_700_STO		0x20	/* Selection or reselection timeout */
#define SSTAT0_800_SEL		0x20	/* Selected */
#define SSTAT0_700_SEL		0x10	/* Selected or reselected */
#define SIST0_800_RSL		0x10	/* Reselected */
#define SSTAT0_SGE		0x08	/* SCSI gross error */
#define SSTAT0_UDC		0x04	/* Unexpected disconnect */
#define SSTAT0_RST		0x02	/* SCSI RST/ received */
#define SSTAT0_PAR		0x01	/* Parity error */

#define SSTAT1_REG		(0x0e^bE)	/* SCSI status 1 ro */
#define SSTAT1_ILF		0x80	/* SIDL full */
#define SSTAT1_ORF		0x40	/* SODR full */
#define SSTAT1_OLF		0x20	/* SODL full */
#define SSTAT1_AIP		0x10	/* Arbitration in progress */
#define SSTAT1_LOA		0x08	/* Lost arbitration */
#define SSTAT1_WOA		0x04	/* Won arbitration */
#define SSTAT1_RST		0x02	/* Instant readout of RST/ */
#define SSTAT1_SDP		0x01	/* Instant readout of SDP/ */

#define SSTAT2_REG		(0x0f^bE)	/* SCSI status 2 ro */
#define SSTAT2_FF3		0x80 	/* number of bytes in synchronous */
#define SSTAT2_FF2		0x40	/* data FIFO */
#define SSTAT2_FF1		0x20
#define SSTAT2_FF0		0x10
#define SSTAT2_FF_MASK		0xf0
#define SSTAT2_FF_SHIFT		4

/*
 * Latched signals, latched on the leading edge of REQ/ for initiators,
 * ACK/ for targets.
 */
#define SSTAT2_SDP		0x08	/* SDP */
#define SSTAT2_MSG		0x04	/* MSG */
#define SSTAT2_CD		0x02	/* C/D */
#define SSTAT2_IO		0x01	/* I/O */
#define SSTAT2_PHASE_CMDOUT	SSTAT2_CD
#define SSTAT2_PHASE_DATAIN	SSTAT2_IO
#define SSTAT2_PHASE_DATAOUT	0
#define SSTAT2_PHASE_MSGIN	(SSTAT2_CD|SSTAT2_IO|SSTAT2_MSG)
#define SSTAT2_PHASE_MSGOUT	(SSTAT2_CD|SSTAT2_MSG)
#define SSTAT2_PHASE_STATIN	(SSTAT2_CD|SSTAT2_IO)
#define SSTAT2_PHASE_MASK	(SSTAT2_CD|SSTAT2_IO|SSTAT2_MSG)


#define DSA_REG			0x10	/* DATA structure address */

#define CTEST0_REG_700		(0x14^bE)	/* Chip test 0 ro */
#define CTEST0_REG_800		(0x18^bE)	/* Chip test 0 ro */
/* 0x80 - 0x04 are reserved */
#define CTEST0_700_RTRG		0x02	/* Real target mode */
#define CTEST0_700_DDIR		0x01	/* Data direction, 1 =
					 * SCSI bus to host, 0  =
					 * host to SCSI.
					 */

#define CTEST1_REG_700		(0x15^bE)	/* Chip test 1 ro */
#define CTEST1_REG_800		(0x19^bE)	/* Chip test 1 ro */
#define CTEST1_FMT3		0x80	/* Identify which byte lanes are empty */
#define CTEST1_FMT2		0x40 	/* in the DMA FIFO */
#define CTEST1_FMT1		0x20
#define CTEST1_FMT0		0x10

#define CTEST1_FFL3		0x08	/* Identify which bytes lanes are full */
#define CTEST1_FFL2		0x04	/* in the DMA FIFO */
#define CTEST1_FFL1		0x02
#define CTEST1_FFL0		0x01

#define CTEST2_REG_700		(0x16^bE)	/* Chip test 2 ro */
#define CTEST2_REG_800		(0x1a^bE)	/* Chip test 2 ro */

#define CTEST2_800_DDIR		0x80	/* 1 = SCSI->host */
#define CTEST2_800_SIGP		0x40	/* A copy of SIGP in ISTAT.
					   Reading this register clears */
#define CTEST2_800_CIO		0x20	/* Configured as IO */.
#define CTEST2_800_CM		0x10	/* Configured as memory */

/* 0x80 - 0x40 are reserved on 700 series chips */
#define CTEST2_700_SOFF		0x20	/* SCSI Offset Compare,
					 * As an initiator, this bit is
					 * one when the synchronous offset
					 * is zero, as a target this bit
					 * is one when the synchronous
					 * offset is at the maximum
					 * defined in SXFER
					 */
#define CTEST2_700_SFP		0x10	/* SCSI FIFO parity bit,
					 * reading CTEST3 unloads a byte
					 * from the FIFO and sets this
					 */
#define CTEST2_700_DFP		0x08	/* DMA FIFO parity bit,
					 * reading CTEST6 unloads a byte
					 * from the FIFO and sets this
					 */
#define CTEST2_TEOP		0x04	/* SCSI true end of process,
					 * indicates a totally finished
					 * transfer
					 */
#define CTEST2_DREQ		0x02	/* Data request signal */
/* 0x01 is reserved on 700 series chips */
#define CTEST2_800_DACK		0x01

/*
 * Chip test 3 ro
 * Unloads the bottom byte of the eight deep SCSI synchronous FIFO,
 * check SSTAT2 FIFO full bits to determine size.  Note that a GROSS
 * error results if a read is attempted on this register.  Also note
 * that 16 and 32 bit reads of this register will cause corruption.
 */
#define CTEST3_REG_700		(0x17^bE)
/*  Chip test 3 rw */
#define CTEST3_REG_800		(0x1b^bE)
#define CTEST3_800_V3		0x80	/* Chip revision */
#define CTEST3_800_V2		0x40
#define CTEST3_800_V1		0x20
#define CTEST3_800_V0		0x10
#define CTEST3_800_FLF		0x08	/* Flush DMA FIFO */
#define CTEST3_800_CLF		0x04	/* Clear DMA FIFO */
#define CTEST3_800_FM		0x02	/* Fetch mode pin */
/* bit 0 is reserved on 800 series chips */

#define CTEST4_REG_700		(0x18^bE)	/* Chip test 4 rw */
#define CTEST4_REG_800		(0x21^bE)	/* Chip test 4 rw */
/* 0x80 is reserved on 700 series chips */
#define CTEST4_800_BDIS		0x80	/* Burst mode disable */
#define CTEST4_ZMOD		0x40	/* High impedance mode */
#define CTEST4_SZM		0x20	/* SCSI bus high impedance */
#define CTEST4_700_SLBE		0x10	/* SCSI loopback enabled */
#define CTEST4_800_SRTM		0x10	/* Shadow Register Test Mode */
#define CTEST4_700_SFWR		0x08	/* SCSI FIFO write enable,
					 * redirects writes from SODL
					 * to the SCSI FIFO.
					 */
#define CTEST4_800_MPEE		0x08	/* Enable parity checking
					   during master cycles on PCI
					   bus */

/*
 * These bits send the contents of the CTEST6 register to the appropriate
 * byte lane of the 32 bit DMA FIFO.  Normal operation is zero, otherwise
 * the high bit means the low two bits select the byte lane.
 */
#define CTEST4_FBL2		0x04
#define CTEST4_FBL1		0x02
#define CTEST4_FBL0		0x01
#define CTEST4_FBL_MASK		0x07
#define CTEST4_FBL_0		0x04	/* Select DMA FIFO byte lane 0 */
#define CTEST4_FBL_1		0x05	/* Select DMA FIFO byte lane 1 */
#define CTEST4_FBL_2		0x06	/* Select DMA FIFO byte lane 2 */
#define CTEST4_FBL_3		0x07	/* Select DMA FIFO byte lane 3 */
#define CTEST4_800_SAVE		(CTEST4_800_BDIS)


#define CTEST5_REG_700		(0x19^bE)	/* Chip test 5 rw */
#define CTEST5_REG_800		(0x22^bE)	/* Chip test 5 rw */
/*
 * Clock Address Incrementor.  When set, it increments the
 * DNAD register to the next bus size boundary.  It automatically
 * resets itself when the operation is complete.
 */
#define CTEST5_ADCK		0x80
/*
 * Clock Byte Counter.  When set, it decrements the DBC register to
 * the next bus size boundary.
 */
#define CTEST5_BBCK		0x40
/*
 * Reset SCSI Offset.  Setting this bit to 1 clears the current offset
 * pointer in the SCSI synchronous offset counter (SSTAT).  This bit
 * is set to 1 if a SCSI Gross Error Condition occurs.  The offset should
 * be cleared when a synchronous transfer fails.  When written, it is
 * automatically cleared after the SCSI synchronous offset counter is
 * reset.
 */
/* Bit 5 is reserved on 800 series chips */
#define CTEST5_700_ROFF		0x20
/*
 * Master Control for Set or Reset pulses. When 1, causes the low
 * four bits of register to set when set, 0 causes the low bits to
 * clear when set.
 */
#define CTEST5_MASR 		0x10
#define CTEST5_DDIR		0x08	/* DMA direction */
/*
 * Bits 2-0 are reserved on 800 series chips
 */
#define CTEST5_700_EOP		0x04	/* End of process */
#define CTEST5_700_DREQ		0x02	/* Data request */
#define CTEST5_700_DACK		0x01	/* Data acknowledge */

/*
 * Chip test 6 rw - writing to this register writes to the byte
 * lane in the DMA FIFO as determined by the FBL bits in the CTEST4
 * register.
 */
#define CTEST6_REG_700		(0x1a^bE)
#define CTEST6_REG_800		(0x23^bE)

#define CTEST7_REG		(0x1b^bE)	/* Chip test 7 rw */
#define CTEST7_10_CDIS		0x80	/* Cache burst disable */
#define CTEST7_10_SC1		0x40	/* Snoop control bits */
#define CTEST7_10_SC0		0x20
#define CTEST7_10_SC_MASK	0x60
#define CTEST7_STD		0x10	/* Selection timeout disable */
#define CTEST7_DFP		0x08	/* DMA FIFO parity bit for CTEST6 */
#define CTEST7_EVP		0x04	/* 1 = host bus even parity, 0 = odd */
#define CTEST7_10_TT1		0x02	/* Transfer type */
#define CTEST7_DIFF		0x01	/* Differential mode */

#define CTEST7_SAVE ( CTEST7_EVP | CTEST7_DIFF )


#define TEMP_REG		0x1c	/* through 0x1f Temporary stack rw */

#define DFIFO_REG		(0x20^bE)	/* DMA FIFO rw */
/*
 * 0x80 is reserved on the NCR53c710, the CLF and FLF bits have been
 * moved into the CTEST8 register.
 */
#define DFIFO_BO6		0x40
#define DFIFO_BO5		0x20
#define DFIFO_BO4		0x10
#define DFIFO_BO3		0x08
#define DFIFO_BO2		0x04
#define DFIFO_BO1		0x02
#define DFIFO_BO0		0x01
#define DFIFO_10_BO_MASK	0x7f	/* 7 bit counter */

/*
 * Interrupt status rw
 * Note that this is the only register which can be read while SCSI
 * SCRIPTS are being executed.
 */
#define ISTAT_REG_700		(0x21^bE)
#define ISTAT_REG_800		(0x14^bE)
#define ISTAT_ABRT		0x80	/* Software abort, write
					 *1 to abort, wait for interrupt. */
#define ISTAT_10_SRST		0x40	/* software reset */
#define ISTAT_10_SIGP		0x20	/* signal script */
#define ISTAT_CON		0x08	/* 1 when connected */
#define ISTAT_800_INTF		0x04	/* Interrupt on the fly */
#define ISTAT_700_PRE		0x04	/* Pointer register empty.
					 * Set to 1 when DSPS and DSP
					 * registers are empty in pipeline
					 * mode, always set otherwise.
					 */
#define ISTAT_SIP		0x02	/* SCSI interrupt pending from
					 * SCSI portion of SIOP see
					 * SSTAT0
					 */
#define ISTAT_DIP		0x01	/* DMA interrupt pending
					 * see DSTAT
					 */

#define CTEST8_REG		(0x22^bE)	/* Chip test 8 rw */
#define CTEST8_10_V3		0x80	/* Chip revision */
#define CTEST8_10_V2		0x40
#define CTEST8_10_V1		0x20
#define CTEST8_10_V0		0x10
#define CTEST8_10_V_MASK	0xf0
#define CTEST8_10_FLF		0x08	/* Flush FIFOs */
#define CTEST8_10_CLF		0x04	/* Clear FIFOs */
#define CTEST8_10_FM		0x02	/* Fetch pin mode */
#define CTEST8_10_SM		0x01	/* Snoop pin mode */


#define LCRC_REG_10		(0x23^bE)

/*
 * 0x24 through 0x27 are the DMA byte counter register.  Instructions
 * write their high 8 bits into the DCMD register, the low 24 bits into
 * the DBC register.
 *
 * Function is dependent on the command type being executed.
 */


#define DBC_REG			0x24
/*
 * For Block Move Instructions, DBC is a 24 bit quantity representing
 *     the number of bytes to transfer.
 * For Transfer Control Instructions, DBC is bit fielded as follows :
 */
/* Bits 20 - 23 should be clear */
#define DBC_TCI_TRUE		(1 << 19) 	/* Jump when true */
#define DBC_TCI_COMPARE_DATA	(1 << 18)	/* Compare data */
#define DBC_TCI_COMPARE_PHASE	(1 << 17)	/* Compare phase with DCMD field */
#define DBC_TCI_WAIT_FOR_VALID	(1 << 16)	/* Wait for REQ */
/* Bits 8 - 15 are reserved on some implementations ? */
#define DBC_TCI_MASK_MASK	0xff00 		/* Mask for data compare */
#define DBC_TCI_MASK_SHIFT	8
#define DBC_TCI_DATA_MASK	0xff		/* Data to be compared */
#define DBC_TCI_DATA_SHIFT	0

#define DBC_RWRI_IMMEDIATE_MASK	0xff00		/* Immediate data */
#define DBC_RWRI_IMMEDIATE_SHIFT 8		/* Amount to shift */
#define DBC_RWRI_ADDRESS_MASK	0x3f0000	/* Register address */
#define DBC_RWRI_ADDRESS_SHIFT 	16


/*
 * DMA command r/w
 */
#define DCMD_REG		(0x27^bE)
#define DCMD_TYPE_MASK		0xc0	/* Masks off type */
#define DCMD_TYPE_BMI		0x00	/* Indicates a Block Move instruction */
#define DCMD_BMI_IO		0x01	/* I/O, CD, and MSG bits selecting   */
#define DCMD_BMI_CD		0x02	/* the phase for the block MOVE      */
#define DCMD_BMI_MSG		0x04	/* instruction 			     */

#define DCMD_BMI_OP_MASK	0x18	/* mask for opcode */
#define DCMD_BMI_OP_MOVE_T	0x00	/* MOVE */
#define DCMD_BMI_OP_MOVE_I	0x08	/* MOVE Initiator */

#define DCMD_BMI_INDIRECT	0x20	/*  Indirect addressing */

#define DCMD_TYPE_TCI		0x80	/* Indicates a Transfer Control
					   instruction */
#define DCMD_TCI_IO		0x01	/* I/O, CD, and MSG bits selecting   */
#define DCMD_TCI_CD		0x02	/* the phase for the block MOVE      */
#define DCMD_TCI_MSG		0x04	/* instruction 			     */
#define DCMD_TCI_OP_MASK	0x38	/* mask for opcode */
#define DCMD_TCI_OP_JUMP	0x00	/* JUMP */
#define DCMD_TCI_OP_CALL	0x08	/* CALL */
#define DCMD_TCI_OP_RETURN	0x10	/* RETURN */
#define DCMD_TCI_OP_INT		0x18	/* INT */

#define DCMD_TYPE_RWRI		0x40	/* Indicates I/O or register Read/Write
					   instruction */
#define DCMD_RWRI_OPC_MASK	0x38	/* Opcode mask */
#define DCMD_RWRI_OPC_WRITE	0x28	/* Write SFBR to register */
#define DCMD_RWRI_OPC_READ	0x30	/* Read register to SFBR */
#define DCMD_RWRI_OPC_MODIFY	0x38	/* Modify in place */

#define DCMD_RWRI_OP_MASK	0x07
#define DCMD_RWRI_OP_MOVE	0x00
#define DCMD_RWRI_OP_SHL	0x01
#define DCMD_RWRI_OP_OR		0x02
#define DCMD_RWRI_OP_XOR	0x03
#define DCMD_RWRI_OP_AND	0x04
#define DCMD_RWRI_OP_SHR	0x05
#define DCMD_RWRI_OP_ADD	0x06
#define DCMD_RWRI_OP_ADDC	0x07

#define DCMD_TYPE_MMI		0xc0	/* Indicates a Memory Move instruction
					   (three words) */


#define DNAD_REG		0x28	/* through 0x2b DMA next address for
					   data */
#define DSP_REG			0x2c	/* through 0x2f DMA SCRIPTS pointer rw */
#define DSPS_REG		0x30	/* through 0x33 DMA SCRIPTS pointer
					   save rw */
#define DMODE_BL1		0x80	/* Burst length bits */
#define DMODE_BL0		0x40
#define DMODE_BL_MASK		0xc0
/* Burst lengths (800) */
#define DMODE_BL_2	0x00	/* 2 transfer */
#define DMODE_BL_4	0x40	/* 4 transfers */
#define DMODE_BL_8	0x80	/* 8 transfers */
#define DMODE_BL_16	0xc0	/* 16 transfers */

#define DMODE_10_BL_1	0x00	/* 1 transfer */
#define DMODE_10_BL_2	0x40	/* 2 transfers */
#define DMODE_10_BL_4	0x80	/* 4 transfers */
#define DMODE_10_BL_8	0xc0	/* 8 transfers */
#define DMODE_10_FC2	0x20	/* Driven to FC2 pin */
#define DMODE_10_FC1	0x10	/* Driven to FC1 pin */
#define DMODE_710_PD	0x08	/* Program/data on FC0 pin */
#define DMODE_710_UO	0x02	/* User prog. output */

#define DMODE_MAN		0x01	/* Manual start mode,
					 * requires a 1 to be written
					 * to the start DMA bit in the DCNTL
					 * register to run scripts
					 */

/* NCR53c800 series only */
#define SCRATCHA_REG_800	0x34	/* through 0x37 Scratch A rw */
/* NCR53c710 only */
#define SCRATCHB_REG_10		0x34	/* through 0x37 scratch rw */

#define DMODE_REG	    	(0x38^bE)	/* DMA mode rw, NCR53c710 and newer */
#define DMODE_800_SIOM		0x20	/* Source IO = 1 */
#define DMODE_800_DIOM		0x10	/* Destination IO = 1 */
#define DMODE_800_ERL		0x08	/* Enable Read Line */

#define DIEN_REG		(0x39^bE)	/* DMA interrupt enable rw */
/* 0x80, 0x40, and 0x20 are reserved on 700-series chips */
#define DIEN_800_MDPE		0x40	/* Master data parity error */
#define DIEN_800_BF		0x20	/* BUS fault */
#define DIEN_700_BF		0x20	/* BUS fault */
#define DIEN_ABRT		0x10	/* Enable aborted interrupt */
#define DIEN_SSI		0x08	/* Enable single step interrupt */
#define DIEN_SIR		0x04	/* Enable SCRIPTS INT command
					 * interrupt
					 */
#define DIEN_700_WTD		0x02	/* Enable watchdog timeout interrupt */
#define DIEN_700_OPC		0x01	/* Enable illegal instruction
					 * interrupt
					 */
#define DIEN_800_IID		0x01	/*  Same meaning, different name */

/*
 * DMA watchdog timer rw
 * set in 16 CLK input periods.
 */
#define DWT_REG			(0x3a^bE)

/* DMA control rw */
#define DCNTL_REG		(0x3b^bE)
#define DCNTL_700_CF1		0x80	/* Clock divisor bits */
#define DCNTL_700_CF0		0x40
#define DCNTL_700_CF_MASK	0xc0
/* Clock divisors 			   Divisor SCLK range (MHZ) */
#define DCNTL_700_CF_2		0x00    /* 2.0	   37.51-50.00 */
#define DCNTL_700_CF_1_5	0x40	/* 1.5	   25.01-37.50 */
#define DCNTL_700_CF_1		0x80	/* 1.0     16.67-25.00 */
#define DCNTL_700_CF_3		0xc0	/* 3.0	   50.01-66.67 (53c700-66) */

#define DCNTL_700_S16		0x20	/* Load scripts 16 bits at a time */
#define DCNTL_SSM		0x10	/* Single step mode */
#define DCNTL_700_LLM		0x08	/* Low level mode, can only be set
					 * after selection */
#define DCNTL_800_IRQM		0x08	/* Totem pole IRQ pin */
#define DCNTL_STD		0x04	/* Start DMA / SCRIPTS */
/* 0x02 is reserved */
#define DCNTL_10_COM		0x01	/* 700 software compatibility mode */
#define DCNTL_10_EA		0x20	/* Enable Ack - needed for MVME16x */

#define SCRATCHB_REG_800	0x5c	/* through 0x5f scratch b rw */
/* NCR53c710 only */
#define ADDER_REG_10		0x3c	/* Adder, NCR53c710 only */

#define SIEN1_REG_800		(0x41^bE)
#define SIEN1_800_STO		0x04	/* selection/reselection timeout */
#define SIEN1_800_GEN		0x02	/* general purpose timer */
#define SIEN1_800_HTH		0x01	/* handshake to handshake */

#define SIST1_REG_800		(0x43^bE)
#define SIST1_800_STO		0x04	/* selection/reselection timeout */
#define SIST1_800_GEN		0x02	/* general purpose timer */
#define SIST1_800_HTH		0x01	/* handshake to handshake */

#define SLPAR_REG_800		(0x44^bE)	/* Parity */

#define MACNTL_REG_800		(0x46^bE)	/* Memory access control */
#define MACNTL_800_TYP3		0x80
#define MACNTL_800_TYP2		0x40
#define MACNTL_800_TYP1		0x20
#define MACNTL_800_TYP0		0x10
#define MACNTL_800_DWR		0x08
#define MACNTL_800_DRD		0x04
#define MACNTL_800_PSCPT	0x02
#define MACNTL_800_SCPTS	0x01

#define GPCNTL_REG_800		(0x47^bE)	/* General Purpose Pin Control */

/* Timeouts are expressed such that 0=off, 1=100us, doubling after that */
#define STIME0_REG_800		(0x48^bE)	/* SCSI Timer Register 0 */
#define STIME0_800_HTH_MASK	0xf0	/* Handshake to Handshake timeout */
#define STIME0_800_HTH_SHIFT	4
#define STIME0_800_SEL_MASK	0x0f	/* Selection timeout */
#define STIME0_800_SEL_SHIFT	0

#define STIME1_REG_800		(0x49^bE)
#define STIME1_800_GEN_MASK	0x0f	/* General purpose timer */

#define RESPID_REG_800		(0x4a^bE)	/* Response ID, bit fielded.  8
					   bits on narrow chips, 16 on WIDE */

#define STEST0_REG_800		(0x4c^bE)
#define STEST0_800_SLT		0x08	/* Selection response logic test */
#define STEST0_800_ART		0x04	/* Arbitration priority encoder test */
#define STEST0_800_SOZ		0x02	/* Synchronous offset zero */
#define STEST0_800_SOM		0x01	/* Synchronous offset maximum */

#define STEST1_REG_800		(0x4d^bE)
#define STEST1_800_SCLK		0x80	/* Disable SCSI clock */

#define STEST2_REG_800		(0x4e^bE)
#define STEST2_800_SCE		0x80	/* Enable SOCL/SODL */
#define STEST2_800_ROF		0x40	/* Reset SCSI sync offset */
#define STEST2_800_SLB		0x10	/* Enable SCSI loopback mode */
#define STEST2_800_SZM		0x08	/* SCSI high impedance mode */
#define STEST2_800_EXT		0x02	/* Extend REQ/ACK filter 30 to 60ns */
#define STEST2_800_LOW		0x01	/* SCSI low level mode */

#define STEST3_REG_800		(0x4f^bE)
#define STEST3_800_TE		0x80	/* Enable active negation */
#define STEST3_800_STR		0x40	/* SCSI FIFO test read */
#define STEST3_800_HSC		0x20	/* Halt SCSI clock */
#define STEST3_800_DSI		0x10	/* Disable single initiator response */
#define STEST3_800_TTM		0x04	/* Time test mode */
#define STEST3_800_CSF		0x02	/* Clear SCSI FIFO */
#define STEST3_800_STW		0x01	/* SCSI FIFO test write */

#define ISTAT_REG	ISTAT_REG_700
#define SCRATCH_REG	SCRATCHB_REG_10
#define ADDER_REG	ADDER_REG_10
#define SIEN_REG	SIEN_REG_700
#define SDID_REG	SDID_REG_700
#define CTEST0_REG	CTEST0_REG_700
#define CTEST1_REG	CTEST1_REG_700
#define CTEST2_REG	CTEST2_REG_700
#define CTEST3_REG	CTEST3_REG_700
#define CTEST4_REG	CTEST4_REG_700
#define CTEST5_REG	CTEST5_REG_700
#define CTEST6_REG	CTEST6_REG_700
#define SODL_REG	SODL_REG_700
#define SBDL_REG	SBDL_REG_700
#define SIDL_REG	SIDL_REG_700
#define LCRC_REG	LCRC_REG_10

#ifdef MEM_MAPPED
#define NCR_read8(address) 					\
	(unsigned int)readb((u32)(host->base) + ((u32)(address)))

#define NCR_read32(address) 					\
	(unsigned int) readl((u32)(host->base) + (u32)(address))

#define NCR_write8(address,value) 				\
	{ DEB(DEB_REGS, printk("NCR: %02x => %08x\n", (u32)(value), ((u32)(host->base) + (u32)(address)))); \
	*(volatile unsigned char *)					\
		((u32)(host->base) + (u32)(address)) = (value); }

#define NCR_write32(address,value) 				\
	{ DEB(DEB_REGS, printk("NCR: %08x => %08x\n", (u32)(value), ((u32)(host->base) + (u32)(address)))); \
	*(volatile unsigned long *)					\
		((u32)(host->base) + (u32)(address)) = (value); }
#else
#define NCR_read8(address) 					\
	inb((u32)(host->base) + (address))

#define NCR_read32(address) 					\
	inl((u32)(host->base) + (address))

#define NCR_write8(address,value) 				\
	{ DEB(DEB_REGS, printk("NCR: %02x => %08x\n", (u32)(value), ((u32)(host->base) + (u32)(address)))); \
	outb((value), (u32)(host->base) + (u32)(address)); }

#define NCR_write32(address,value) 				\
	{ DEB(DEB_REGS, printk("NCR: %08x => %08x\n", (u32)(value), ((u32)(host->base) + (u32)(address)))); \
	outl((value), (u32)(host->base) + (u32)(address)); }
#endif

/* Patch arbitrary 32 bit words in the script */
#define patch_abs_32(script, offset, symbol, value)			\
    	for (i = 0; i < (sizeof (A_##symbol##_used) / sizeof 		\
    	    (u32)); ++i) {					\
	    (script)[A_##symbol##_used[i] - (offset)] += (value);	\
	      DEB(DEB_FIXUP, printk("scsi%d: %s reference %d at 0x%x in %s is now 0x%x\n",\
		host->host_no, #symbol, i, A_##symbol##_used[i] - 	\
		(int)(offset), #script, (script)[A_##symbol##_used[i] -	\
		(offset)]));						\
    	}

#endif
#endif
