/*
 * Aic7xxx register and scratch ram definitions.
 *
 * Copyright (c) 1994, 1995 Justin T. Gibbs.
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
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Justin T. Gibbs.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 *	$Id$
 */

/*
 * This header should be shared by the sequencer code and the kernel
 * level driver.  Unfortuanetly I haven't mangled the sequencer assembler
 * into using cpp yet. Someday...
 *
 * All page numbers refer to the Adaptec AIC-7770 Data Book availible from
 * Adaptec's Technical Documents Department 1-800-934-2766
 */

/*
 * SCSI Sequence Control (p. 3-11).
 * Each bit, when set starts a specific SCSI sequence on the bus
 */
#define SCSISEQ			0x000
#define		TEMODEO		0x80
#define		ENSELO		0x40
#define		ENSELI		0x20
#define		ENRSELI		0x10
#define		ENAUTOATNO	0x08
#define		ENAUTOATNI	0x04
#define		ENAUTOATNP	0x02
#define		SCSIRSTO	0x01

/*
 * SCSI Transfer Control 0 Register (pp. 3-13).
 * Controls the SCSI module data path.
 */
#define	SXFRCTL0		0x001
#define		DFON		0x80
#define		DFPEXP		0x40
#define		ULTRAEN		0x20
#define		CLRSTCNT	0x10
#define		SPIOEN		0x08
#define		SCAMEN		0x04
#define		CLRCHN		0x02
/*  UNUSED			0x01 */

/*
 * SCSI Transfer Control 1 Register (pp. 3-14,15).
 * Controls the SCSI module data path.
 */
#define	SXFRCTL1		0x002
#define		BITBUCKET	0x80
#define		SWRAPEN		0x40
#define		ENSPCHK		0x20
#define		STIMESEL	0x18
#define		ENSTIMER	0x04
#define		ACTNEGEN	0x02
#define		STPWEN		0x01	/* Powered Termination */

/*
 * SCSI Interrrupt Mode 1 (pp. 3-28,29).
 * Set bits in this register enable the corresponding
 * interrupt source.
 */
#define	SIMODE1			0x011
#define		ENSELTIMO	0x80
#define		ENATNTARG	0x40
#define		ENSCSIRST	0x20
#define		ENPHASEMIS	0x10
#define		ENBUSFREE	0x08
#define		ENSCSIPERR	0x04
#define		ENPHASECHG	0x02
#define		ENREQINIT	0x01

/*
 * SCSI Control Signal Read Register (p. 3-15).
 * Reads the actual state of the SCSI bus pins
 */
#define SCSISIGI		0x003
#define		CDI		0x80
#define		IOI		0x40
#define		MSGI		0x20
#define		ATNI		0x10
#define		SELI		0x08
#define		BSYI		0x04
#define		REQI		0x02
#define		ACKI		0x01

/*
 * SCSI Contol Signal Write Register (p. 3-16).
 * Writing to this register modifies the control signals on the bus.  Only
 * those signals that are allowed in the current mode (Initiator/Target) are
 * asserted.
 */
#define SCSISIGO		0x003
#define		CDO		0x80
#define		IOO		0x40
#define		MSGO		0x20
#define		ATNO		0x10
#define		SELO		0x08
#define		BSYO		0x04
#define		REQO		0x02
#define		ACKO		0x01

/* 
 * SCSI Rate Control (p. 3-17).
 * Contents of this register determine the Synchronous SCSI data transfer
 * rate and the maximum synchronous Req/Ack offset.  An offset of 0 in the
 * SOFS (3:0) bits disables synchronous data transfers.  Any offset value
 * greater than 0 enables synchronous transfers.
 */
#define SCSIRATE		0x004
#define WIDEXFER		0x80		/* Wide transfer control */
#define SXFR			0x70		/* Sync transfer rate */
#define SOFS			0x0f		/* Sync offset */

/*
 * SCSI ID (p. 3-18).
 * Contains the ID of the board and the current target on the
 * selected channel
 */
#define SCSIID			0x005
#define		TID		0xf0		/* Target ID mask */
#define		OID		0x0f		/* Our ID mask */

/*
 * SCSI Transfer Count (pp. 3-19,20)
 * These registers count down the number of bytes transfered
 * across the SCSI bus.  The counter is decremented only once
 * the data has been safely transfered.  SDONE in SSTAT0 is
 * set when STCNT goes to 0
 */ 
#define STCNT			0x008

/*
 * SCSI Status 0 (p. 3-21)
 * Contains one set of SCSI Interrupt codes
 * These are most likely of interest to the sequencer
 */
#define SSTAT0			0x00b
#define		TARGET		0x80		/* Board is a target */
#define		SELDO		0x40		/* Selection Done */
#define		SELDI		0x20		/* Board has been selected */
#define		SELINGO		0x10		/* Selection In Progress */
#define		SWRAP		0x08		/* 24bit counter wrap */
#define		SDONE		0x04		/* STCNT = 0x000000 */
#define		SPIORDY		0x02		/* SCSI PIO Ready */
#define		DMADONE		0x01		/* DMA transfer completed */

/*
 * Clear SCSI Interrupt 1 (p. 3-23)
 * Writing a 1 to a bit clears the associated SCSI Interrupt in SSTAT1.
 */
#define CLRSINT1		0x00c
#define		CLRSELTIMEO	0x80
#define		CLRATNO		0x40
#define		CLRSCSIRSTI	0x20
/*  UNUSED			0x10 */
#define		CLRBUSFREE	0x08
#define		CLRSCSIPERR	0x04
#define		CLRPHASECHG	0x02
#define		CLRREQINIT	0x01

/*
 * SCSI Status 1 (p. 3-24)
 * These interrupt bits are of interest to the kernel driver
 */
#define SSTAT1			0x00c
#define		SELTO		0x80
#define		ATNTARG 	0x40
#define		SCSIRSTI	0x20
#define		PHASEMIS	0x10
#define		BUSFREE		0x08
#define		SCSIPERR	0x04
#define		PHASECHG	0x02
#define		REQINIT		0x01

/*
 * SCSI/Host Address (p. 3-30)
 * These registers hold the host address for the byte about to be
 * transfered on the SCSI bus.  They are counted up in the same
 * manner as STCNT is counted down.  SHADDR should always be used
 * to determine the address of the last byte transfered since HADDR
 * can be squewed by write ahead.
 */
#define	SHADDR			0x014

/*
 * Selection/Reselection ID (p. 3-31)
 * Upper four bits are the device id.  The ONEBIT is set when the re/selecting
 * device did not set its own ID.
 */
#define SELID			0x019
#define		SELID_MASK	0xf0
#define		ONEBIT		0x08
/*  UNUSED			0x07 */

/*
 * SCSI Block Control (p. 3-32)
 * Controls Bus type and channel selection.  In a twin channel configuration
 * addresses 0x00-0x1e are gated to the appropriate channel based on this
 * register.  SELWIDE allows for the coexistence of 8bit and 16bit devices
 * on a wide bus.
 */
#define SBLKCTL			0x01f
/*  UNUSED			0xc0 */
#define		AUTOFLUSHDIS	0x20
/*  UNUSED			0x10 */
#define		SELBUSB		0x08
/*  UNUSED			0x04 */
#define		SELWIDE		0x02
/*  UNUSED			0x01 */

/*
 * Sequencer Control (p. 3-33)
 * Error detection mode and speed configuration
 */
#define SEQCTL			0x060
#define		PERRORDIS	0x80
#define		PAUSEDIS	0x40
#define		FAILDIS		0x20
#define 	FASTMODE	0x10
#define		BRKADRINTEN	0x08
#define		STEP		0x04
#define		SEQRESET	0x02
#define		LOADRAM		0x01

/*
 * Sequencer RAM Data (p. 3-34)
 * Single byte window into the Scratch Ram area starting at the address
 * specified by SEQADDR0 and SEQADDR1.  To write a full word, simply write
 * four bytes in sucessesion.  The SEQADDRs will increment after the most
 * significant byte is written
 */
#define SEQRAM			0x061

/*
 * Sequencer Address Registers (p. 3-35)
 * Only the first bit of SEQADDR1 holds addressing information
 */
#define SEQADDR0		0x062
#define SEQADDR1		0x063
#define 	SEQADDR1_MASK	0x01

/*
 * Accumulator
 * We cheat by passing arguments in the Accumulator up to the kernel driver
 */
#define ACCUM			0x064

#define SINDEX			0x065

/*
 * Board Control (p. 3-43)
 */
#define BCTL			0x084
/*   RSVD			0xf0 */
#define		ACE		0x08	/* Support for external processors */
/*   RSVD			0x06 */
#define		ENABLE		0x01

/*
 * Bus On/Off Time (p. 3-44)
 */
#define BUSTIME			0x085
#define		BOFF		0xf0
#define		BON		0x0f

/*
 * Bus Speed (p. 3-45)
 */
#define	BUSSPD			0x086
#define		DFTHRSH		0xc0
#define		STBOFF		0x38
#define		STBON		0x07

/*
 * Host Control (p. 3-47) R/W
 * Overal host control of the device.
 */
#define HCNTRL			0x087
/*    UNUSED			0x80 */
#define		POWRDN		0x40
/*    UNUSED			0x20 */
#define		SWINT		0x10
#define		IRQMS		0x08
#define		PAUSE		0x04
#define		INTEN		0x02
#define		CHIPRST		0x01

/*
 * Host Address (p. 3-48)
 * This register contains the address of the byte about
 * to be transfered across the host bus.
 */
#define HADDR			0x088
/*
 * SCB Pointer (p. 3-49)
 * Gate one of the four SCBs into the SCBARRAY window.
 */
#define SCBPTR			0x090

/*
 * Interrupt Status (p. 3-50)
 * Status for system interrupts
 */
#define INTSTAT			0x091
#define		SEQINT_MASK	0xf0		/* SEQINT Status Codes */
#define			BAD_PHASE	0x00
#define			SEND_REJECT	0x10
#define			NO_IDENT	0x20
#define			NO_MATCH	0x30
#define			MSG_SDTR	0x40
#define			MSG_WDTR	0x50
#define			MSG_REJECT	0x60
#define			BAD_STATUS	0x70
#define			RESIDUAL	0x80
#define			ABORT_TAG	0x90
#define			AWAITING_MSG	0xa0
#define			IMMEDDONE	0xb0
#define 	BRKADRINT 0x08
#define		SCSIINT	  0x04
#define		CMDCMPLT  0x02
#define		SEQINT    0x01
#define		INT_PEND  (BRKADRINT | SEQINT | SCSIINT | CMDCMPLT)

/*
 * Hard Error (p. 3-53)
 * Reporting of catastrophic errors.  You usually cannot recover from
 * these without a full board reset.
 */
#define ERROR			0x092
/*    UNUSED			0xf0 */
#define		PARERR		0x08
#define		ILLOPCODE	0x04
#define		ILLSADDR	0x02
#define		ILLHADDR	0x01

/*
 * Clear Interrupt Status (p. 3-52)
 */
#define CLRINT			0x092
#define		CLRBRKADRINT	0x08
#define		CLRSCSIINT      0x04
#define		CLRCMDINT 	0x02
#define		CLRSEQINT 	0x01

/*
 * SCB Auto Increment (p. 3-59)
 * Byte offset into the SCB Array and an optional bit to allow auto
 * incrementing of the address during download and upload operations
 */
#define SCBCNT			0x09a
#define		SCBAUTO		0x80
#define		SCBCNT_MASK	0x1f

/*
 * Queue In FIFO (p. 3-60)
 * Input queue for queued SCBs (commands that the seqencer has yet to start)
 */
#define QINFIFO			0x09b

/*
 * Queue In Count (p. 3-60)
 * Number of queued SCBs
 */
#define QINCNT			0x09c

/*
 * Queue Out FIFO (p. 3-61)
 * Queue of SCBs that have completed and await the host
 */
#define QOUTFIFO		0x09d

/*
 * Queue Out Count (p. 3-61)
 * Number of queued SCBs in the Out FIFO
 */
#define QOUTCNT			0x09e

#define SCBARRAY		0x0a0

/* --------------------- AIC-7870-only definitions -------------------- */

#define DSPCISTATUS		0x086

/*
 * Serial EEPROM Control (p. 4-92 in 7870 Databook)
 * Controls the reading and writing of an external serial 1-bit
 * EEPROM Device.  In order to access the serial EEPROM, you must
 * first set the SEEMS bit that generates a request to the memory
 * port for access to the serial EEPROM device.  When the memory
 * port is not busy servicing another request, it reconfigures
 * to allow access to the serial EEPROM.  When this happens, SEERDY
 * gets set high to verify that the memory port access has been
 * granted.  
 *
 * After successful arbitration for the memory port, the SEECS bit of 
 * the SEECTL register is connected to the chip select.  The SEECK, 
 * SEEDO, and SEEDI are connected to the clock, data out, and data in 
 * lines respectively.  The SEERDY bit of SEECTL is useful in that it 
 * gives us an 800 nsec timer.  After a write to the SEECTL register, 
 * the SEERDY goes high 800 nsec later.  The one exception to this is 
 * when we first request access to the memory port.  The SEERDY goes 
 * high to signify that access has been granted and, for this case, has 
 * no implied timing.
 *
 * See 93cx6.c for detailed information on the protocol necessary to 
 * read the serial EEPROM.
 */
#define SEECTL			0x01e
#define		EXTARBACK	0x80
#define		EXTARBREQ	0x40
#define		SEEMS		0x20
#define		SEERDY		0x10
#define		SEECS		0x08
#define		SEECK		0x04
#define		SEEDO		0x02
#define		SEEDI		0x01

/* ---------------------- Scratch RAM Offsets ------------------------- */
/* These offsets are either to values that are initialized by the board's
 * BIOS or are specified by the Linux sequencer code.  If I can figure out
 * how to read the EISA configuration info at probe time, the cards could
 * be run without BIOS support installed
 */

/*
 * 1 byte per target starting at this address for configuration values
 */
#define HA_TARG_SCRATCH		0x020

/*
 * The sequencer will stick the frist byte of any rejected message here so
 * we can see what is getting thrown away.
 */
#define HA_REJBYTE		0x031

/*
 * Bit vector of targets that have disconnection disabled.
 */
#define	HA_DISC_DSB		0x032

/*
 * Length of pending message
 */
#define HA_MSG_LEN		0x034

/*
 * message body
 */
#define HA_MSG_START		0x035	/* outgoing message body */

/*
 * These are offsets into the card's scratch ram.  Some of the values are
 * specified in the AHA2742 technical reference manual and are initialized
 * by the BIOS at boot time.
 */
#define HA_ARG_1		0x04a
#define HA_RETURN_1		0x04a
#define		SEND_SENSE	0x80
#define		SEND_WDTR	0x80
#define		SEND_SDTR	0x80
#define		SEND_REJ	0x40

#define	SG_COUNT		0x04d
#define	SG_NEXT			0x04e
#define HA_SIGSTATE		0x04b

#define HA_SCBCOUNT		0x052
#define HA_FLAGS		0x053
#define		SINGLE_BUS	0x00
#define		TWIN_BUS	0x01
#define		WIDE_BUS	0x02
#define		ACTIVE_MSG	0x20
#define		IDENTIFY_SEEN	0x40
#define		RESELECTING	0x80

#define	HA_ACTIVE0		0x054
#define	HA_ACTIVE1		0x055
#define	SAVED_TCL		0x056
#define WAITING_SCBH		0x057
#define WAITING_SCBT		0x058

#define HA_SCSICONF		0x05a
#define HA_HOSTCONF		0x05d

#define HA_274_BIOSCTRL		0x05f
#define BIOSMODE		0x30
#define BIOSDISABLED		0x30

#define MSG_ABORT		0x06
#define MSG_BUS_DEVICE_RESET	0x0c
#define	BUS_8_BIT		0x00
#define BUS_16_BIT		0x01
#define BUS_32_BIT		0x02




