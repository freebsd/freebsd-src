/*
 * Aic7xxx register and scratch ram definitions.
 *
 * Copyright (c) 1994, 1995, 1996 Justin T. Gibbs.
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
 *	$Id: aic7xxx_reg.h,v 1.16 1996/11/11 05:16:41 gibbs Exp $
 */

/*
 * This header is shared by the sequencer code and the kernel level driver.
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
 * Possible phases in SCSISIGI
 */
#define		PHASE_MASK	0xe0
#define		P_DATAOUT	0x00
#define		P_DATAIN	0x40
#define		P_COMMAND	0x80
#define		P_MESGOUT	0xa0
#define		P_STATUS	0xc0
#define		P_MESGIN	0xe0
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
#define		WIDEXFER	0x80		/* Wide transfer control */
#define		SXFR		0x70		/* Sync transfer rate */
#define		SOFS		0x0f		/* Sync offset */

/*
 * SCSI ID (p. 3-18).
 * Contains the ID of the board and the current target on the
 * selected channel.
 */
#define SCSIID			0x005
#define		TID		0xf0		/* Target ID mask */
#define		OID		0x0f		/* Our ID mask */

/*
 * SCSI Latched Data (p. 3-19).
 * Read/Write latchs used to transfer data on the SCSI bus during
 * Automatic or Manual PIO mode.  SCSIDATH can be used for the
 * upper byte of a 16bit wide asyncronouse data phase transfer.
 */
#define SCSIDATL		0x006
#define SCSIDATH		0x007

/*
 * SCSI Transfer Count (pp. 3-19,20)
 * These registers count down the number of bytes transfered
 * across the SCSI bus.  The counter is decremented only once
 * the data has been safely transfered.  SDONE in SSTAT0 is
 * set when STCNT goes to 0
 */ 
#define STCNT			0x008
#define STCNT0			0x008
#define STCNT1			0x009
#define STCNT2			0x00a

/*
 * Clear SCSI Interrupt 0 (p. 3-20)
 * Writing a 1 to a bit clears the associated SCSI Interrupt in SSTAT0.
 */
#define	CLRSINT0		0x00b
#define		CLRSELDO	0x40
#define		CLRSELDI	0x20
#define		CLRSELINGO	0x10
#define		CLRSWRAP	0x08
/*  UNUSED			0x04 */
#define		CLRSPIORDY	0x02
/*  UNUSED			0x01 */

/*
 * SCSI Status 0 (p. 3-21)
 * Contains one set of SCSI Interrupt codes
 * These are most likely of interest to the sequencer
 */
#define SSTAT0			0x00b
#define		TARGET		0x80		/* Board acting as target */
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
 * SCSI Interrupt Mode 1 (pp. 3-28,29)
 * Setting any bit will enable the corresponding function
 * in SIMODE1 to interrupt via the IRQ pin.
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
 * SCSI Data Bus (High) (p. 3-29)
 * This register reads data on the SCSI Data bus directly.
 */
#define	SCSIBUSL		0x012
#define	SCSIBUSH		0x013

/*
 * SCSI/Host Address (p. 3-30)
 * These registers hold the host address for the byte about to be
 * transfered on the SCSI bus.  They are counted up in the same
 * manner as STCNT is counted down.  SHADDR should always be used
 * to determine the address of the last byte transfered since HADDR
 * can be squewed by write ahead.
 */
#define	SHADDR			0x014
#define	SHADDR0			0x014
#define	SHADDR1			0x015
#define	SHADDR2			0x016
#define	SHADDR3			0x017

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
#define		DIAGLEDEN	0x80	/* Aic78X0 only */
#define		DIAGLEDON	0x40	/* Aic78X0 only */
#define		AUTOFLUSHDIS	0x20
/*  UNUSED			0x10 */
#define		SELBUS_MASK	0x0a
#define		SELBUSB		0x08
/*  UNUSED			0x04 */
#define		SELWIDE		0x02
/*  UNUSED			0x01 */
#define		SELNARROW	0x00

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
#define DINDEX			0x066
#define ALLZEROS		0x06a
#define NONE			0x06a
#define SINDIR			0x06c
#define DINDIR			0x06d
#define FUNCTION1		0x06e

/*
 * Host Address (p. 3-48)
 * This register contains the address of the byte about
 * to be transfered across the host bus.
 */
#define HADDR			0x088
#define HADDR0			0x088
#define HADDR1			0x089
#define HADDR2			0x08a
#define HADDR3			0x08b

#define HCNT			0x08c
#define HCNT0			0x08c
#define HCNT1			0x08d
#define HCNT2			0x08e
/*
 * SCB Pointer (p. 3-49)
 * Gate one of the four SCBs into the SCBARRAY window.
 */
#define SCBPTR			0x090

/*
 * Board Control (p. 3-43)
 */
#define BCTL			0x084
/*   RSVD			0xf0 */
#define		ACE		0x08	/* Support for external processors */
/*   RSVD			0x06 */
#define		ENABLE		0x01

/*
 * On the aic78X0 chips, Board Control is replaced by the DSCommand
 * register (p. 4-64)
 */
#define	DSCOMMAND		0x084
#define		CACHETHEN	0x80	/* Cache Threshold enable */
#define		DPARCKEN	0x40	/* Data Parity Check Enable */
#define		MPARCKEN	0x20	/* Memory Parity Check Enable */
#define		EXTREQLCK	0x10	/* External Request Lock */

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
#define		DFTHRSH_100	0xc0

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
#define		CHIPRSTACK	0x01

/*
 * Interrupt Status (p. 3-50)
 * Status for system interrupts
 */
#define INTSTAT			0x091
#define		SEQINT_MASK	0xf1		/* SEQINT Status Codes */
#define			BAD_PHASE	0x01	/* unknown scsi bus phase */
#define			SEND_REJECT	0x11	/* sending a message reject */
#define			NO_IDENT	0x21	/* no IDENTIFY after reconnect*/
#define			NO_MATCH	0x31	/* no cmd match for reconnect */
#define			EXTENDED_MSG	0x41	/* Extended message received */
#define			NO_MATCH_BUSY	0x51	/* Couldn't find BUSY SCB */
#define			REJECT_MSG	0x61	/* Reject message received */
#define			BAD_STATUS	0x71	/* Bad status from target */
#define			RESIDUAL	0x81	/* Residual byte count != 0 */
#define			ABORT_TAG	0x91	/* Sent an ABORT_TAG message */
#define			AWAITING_MSG	0xa1	/*
						 * Kernel requested to specify
                                                 * a message to this target
                                                 * (command was null), so tell
                                                 * it that it can fill the
                                                 * message buffer.
                                                 */
#define			IMMEDDONE	0xb1	/*
						 * An immediate command has
						 * completed
						 */
#define			MSG_BUFFER_BUSY	0xc1	/*
						 * Sequencer wants to use the
						 * message buffer, but it
						 * already contains a message
						 */
#define			MSGIN_PHASEMIS	0xd1	/*
						 * Target changed phase on us
						 * when we were expecting
						 * another msgin byte.
						 */
#define			DATA_OVERRUN	0xe1	/*
						 * Target attempted to write
						 * beyond the bounds of its
						 * command.
						 */

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

#define	DFCNTRL			0x093
#define		WIDEODD		0x40
#define		SCSIEN		0x20
#define		SDMAEN		0x10
#define		SDMAENACK	0x10
#define		HDMAEN		0x08
#define		HDMAENACK	0x08
#define		DIRECTION	0x04
#define		FIFOFLUSH	0x02
#define		FIFORESET	0x01

#define	DFSTATUS		0x094
#define		HDONE		0x08
#define		FIFOEMP		0x01

#define	DFDAT			0x099

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

/*
 * SCB Definition (p. 5-4)
 * The two reserved bytes at SCBARRAY+1[23] are expected to be set to
 * zero. Bit 3 in SCBARRAY+0 is used as an internal flag to indicate
 * whether or not to DMA an SCB from host ram. This flag prevents the
 * "re-fetching" of transactions that are requed because the target is
 * busy with another command. We also use bits 6 & 7 to indicate whether
 * or not to initiate SDTR or WDTR repectively when starting this command.
 */
#define SCBARRAY		0x0a0
#define	SCB_CONTROL		0x0a0
#define		MK_MESSAGE      0x80
#define		DISCENB         0x40
#define		TAG_ENB		0x20
#define		ABORT_SCB	0x08
#define		DISCONNECTED	0x04
#define		SCB_TAG_TYPE	0x03
#define	SCB_TCL			0x0a1
#define	SCB_TARGET_STATUS	0x0a2
#define	SCB_SGCOUNT		0x0a3
#define	SCB_SGPTR		0x0a4
#define		SCB_SGPTR0	0x0a4
#define		SCB_SGPTR1	0x0a5
#define		SCB_SGPTR2	0x0a6
#define		SCB_SGPTR3	0x0a7
#define	SCB_RESID_SGCNT		0x0a8
#define SCB_RESID_DCNT		0x0a9
#define		SCB_RESID_DCNT0	0x0a9
#define		SCB_RESID_DCNT1	0x0aa
#define		SCB_RESID_DCNT2	0x0ab
#define SCB_DATAPTR		0x0ac
#define		SCB_DATAPTR0	0x0ac
#define		SCB_DATAPTR1	0x0ad
#define		SCB_DATAPTR2	0x0ae
#define		SCB_DATAPTR3	0x0af
#define	SCB_DATACNT		0x0b0
#define		SCB_DATACNT0	0x0b0
#define		SCB_DATACNT1	0x0b1
#define		SCB_DATACNT2	0x0b2
#define	SCB_LINKED_NEXT		0x0b3
#define SCB_CMDPTR		0x0b4
#define		SCB_CMDPTR0	0x0b4
#define		SCB_CMDPTR1	0x0b5
#define		SCB_CMDPTR2	0x0b6
#define		SCB_CMDPTR3	0x0b7
#define	SCB_CMDLEN		0x0b8
#define SCB_TAG			0x0b9
#define	SCB_NEXT		0x0ba
#define	SCB_PREV		0x0bb
#define	SCB_ACTIVE0		0x0bc
#define	SCB_ACTIVE1		0x0bd
#define	SCB_ACTIVE2		0x0be
#define	SCB_ACTIVE3		0x0bf

#ifdef __linux__
#define	SG_SIZEOF		0x0c		/* sizeof(struct scatterlist) */
#else
#define	SG_SIZEOF		0x08		/* sizeof(struct ahc_dma) */
#endif

/* --------------------- AHA-2840-only definitions -------------------- */

#define	SEECTL_2840		0x0c0
/*	UNUSED			0xf8 */
#define		CS_2840		0x04
#define		CK_2840		0x02
#define		DO_2840		0x01

#define	STATUS_2840		0x0c1
#define		EEPROM_TF	0x80
#define		BIOS_SEL	0x60
#define		ADSEL		0x1e
#define		DI_2840		0x01

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
 * BIOS or are specified by the sequencer code.
 *
 * The host adapter card (at least the BIOS) uses 20-2f for SCSI
 * device information, 32-33 and 5a-5f as well. As it turns out, the
 * BIOS trashes 20-2f, writing the synchronous negotiation results
 * on top of the BIOS values, so we re-use those for our per-target
 * scratchspace (actually a value that can be copied directly into
 * SCSIRATE).  The kernel driver will enable synchronous negotiation
 * for all targets that have a value other than 0 in the lower four
 * bits of the target scratch space.  This should work regardless of
 * whether the bios has been installed.
 */

/*
 * 1 byte per target starting at this address for configuration values
 */
#define TARG_SCRATCH		0x020

/*
 * The sequencer will stick the frist byte of any rejected message here so
 * we can see what is getting thrown away.
 */
#define REJBYTE			0x030
/*
 * Since the sequencer cannot read QOUTCNT, we use this memory location
 * to make sure that we don't overflow the QOUTFIFO when doing SCB Paging.
 */
#define	QOUTQCNT		0x031

/*
 * Bit vector of targets that have disconnection disabled.
 */
#define	DISC_DSB		0x032
#define		DISC_DSB_A	0x032
#define		DISC_DSB_B	0x033

/*
 * Length of pending message
 */
#define MSG_LEN			0x034

/* We reserve 6bytes to store outgoing messages */
#define MSG0			0x035
#define		COMP_MSG0	0xcb      /* 2's complement of MSG0 */
#define MSG1			0x036
#define MSG2			0x037
#define MSG3			0x038
#define MSG4			0x039
#define MSG5			0x03a

#define LASTPHASE		0x03b
#define		P_BUSFREE	0x01

#define ARG_1			0x03c
#define RETURN_1		0x03c
#define		SEND_MSG	0x80
#define		SEND_SENSE	0x40
#define		SEND_REJ	0x20
#define		SCB_PAGEDIN	0x10

#define DMAPARAMS		0x03d	/* Parameters for DMA Logic */

#define	SCBCOUNT		0x03e	/*
					 * Number of SCBs supported by
					 * this card.
					 */
#define	COMP_SCBCOUNT		0x03f	/*
					 * Two's compliment of SCBCOUNT
					 */
#define QCNTMASK		0x040	/*
					 * Mask of bits to test against
					 * when looking at the Queue Count
					 * registers.  Works around a bug
					 * on aic7850 chips. 
					 */
#define FLAGS			0x041
#define		SINGLE_BUS	0x00
#define		TWIN_BUS	0x01
#define		WIDE_BUS	0x02
#define		PAGESCBS	0x04
#define		SCB_LISTED	0x08
#define		DPHASE		0x10
#define		TAGGED_SCB	0x20
#define		IDENTIFY_SEEN	0x40
#define		RESELECTED	0x80

#define	SAVED_TCL		0x042	/*
					 * Temporary storage for the
					 * target/channel/lun of a
					 * reconnecting target
					 */
#define	SG_COUNT		0x043
#define	SG_NEXT			0x044	/* working value of SG pointer */
#define		SG_NEXT0	0x044
#define		SG_NEXT1	0x045
#define		SG_NEXT2	0x046
#define		SG_NEXT3	0x047

#define WAITING_SCBH		0x048	/*
					 * head of list of SCBs awaiting
					 * selection
					 */
#define SAVED_LINKPTR		0x049
#define SAVED_SCBPTR		0x04a
#define ULTRA_ENB		0x04b
#define ULTRA_ENB_B		0x04c

#define MSGIN_EXT_LEN		0x04d
#define MSGIN_EXT_OPCODE	0x04e
#define MSGIN_EXT_BYTE0		0x04f
#define MSGIN_EXT_BYTE1		0x050
#define MSGIN_EXT_BYTE2		0x051	/*
					 * This location, stores the last
					 * byte of an extended message if
					 * it passes the two bytes of space
					 * we allow now.  This byte isn't
					 * used for anything, it just makes
					 * the code shorter for tossing
					 * extra bytes.
					 */
#define	MSGIN_EXT_LASTBYTE	0x052	/* Used as the address for range
					 * checking, not used for storage.
					 */

#define DISCONNECTED_SCBH	0x052	/*
					 * head of list of SCBs that are
					 * disconnected.  Used for SCB
					 * paging.
					 */
#define FREE_SCBH		0x053	/*
					 * head of list of SCBs that are
					 * not in use.  Used for SCB paging.
					 */


#define HSCB_ADDR0		0x054
#define HSCB_ADDR1		0x055
#define HSCB_ADDR2		0x056
#define HSCB_ADDR3		0x057

#define	CUR_SCBID		0x058
#define QFULLCNT		0x059

#define		SCB_LIST_NULL	0xff

/*
 * These are offsets into the card's scratch ram.  Some of the values are
 * specified in the AHA2742 technical reference manual and are initialized
 * by the BIOS at boot time.
 */
#define SCSICONF		0x05a
#define		RESET_SCSI	0x40

#define HOSTCONF		0x05d

#define HA_274_BIOSCTRL		0x05f
#define BIOSMODE		0x30
#define BIOSDISABLED		0x30
#define CHANNEL_B_PRIMARY	0x08

/* WDTR Message values */
#define	BUS_8_BIT		0x00
#define BUS_16_BIT		0x01
#define BUS_32_BIT		0x02

#define MAX_OFFSET_8BIT		0x0f
#define MAX_OFFSET_16BIT	0x08
