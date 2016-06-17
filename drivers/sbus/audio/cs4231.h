/* $Id: cs4231.h,v 1.13 1999/09/21 14:37:27 davem Exp $
 * drivers/sbus/audio/cs4231.h
 *
 * Copyright (C) 1996 Thomas K. Dyas (tdyas@noc.rutgers.edu)
 * Copyright (C) 1997 Derrick J. Brashear (shadow@dementia.org)
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 */

#ifndef _CS4231_H_
#define _CS4231_H_

#include <linux/types.h>

/* According to the CS4231A data provided on CS web site and sun's includes */
#define IAR	0x00UL	/* Index Address Register */
#define IDR	0x04UL	/* Index Data Register */
#define STAT	0x08UL	/* Status Register */
#define PIOD	0x0cUL	/* PIO Data Register */
#define APCCSR	0x10UL	/* APC DMA CSR */
#define APCCVA	0x20UL	/* APC Capture DMA Address */
#define APCCC	0x24UL	/* APC Capture Count */
#define APCCNVA	0x28UL	/* APC Capture DMA Next Address */
#define APCCNC	0x2cUL	/* APC Capture Next Count */
#define APCPVA	0x30UL	/* APC Play DMA Address */
#define APCPC	0x34UL	/* APC Play Count */
#define APCPNVA	0x38UL	/* APC Play DMA Next Address */
#define APCPNC	0x3cUL	/* APC Play Next Count */

/* EBUS DMA Registers */
#define EBDMA_CSR	0x00UL	/* Control/Status */
#define EBDMA_ADDR	0x04UL	/* DMA Address */
#define EBDMA_COUNT	0x08UL	/* DMA Count */

/* Our structure for each chip */
struct cs4231_chip {
	unsigned long regs;
	unsigned long eb2c;
	unsigned long eb2p;
	struct audio_info perchip_info;
	unsigned int playlen, reclen;
	int irq, irq2, nirqs;
	unsigned long regs_size;
  
	/* Keep track of various info */
	volatile unsigned int status;
  
	/* Current buffer that the driver is playing. */
	volatile __u8 * output_ptr;
	volatile __u32 output_size;
	volatile __u32 output_dma_handle, output_next_dma_handle;
	volatile __u32 output_dma_size, output_next_dma_size;

	/* Current record buffer. */
	volatile __u8 * input_ptr;
	volatile __u32 input_size;
	volatile __u32 input_dma_handle, input_next_dma_handle;
	volatile __u32 input_dma_size, input_next_dma_size;

	/* Number of buffers in the pipe. */
	volatile __u32 playing_count;
	volatile __u32 recording_count;
};

#ifdef EB4231_SUPPORT
#define CS4231_READ32(__C, __REG)		\
	(((__C)->status & CS_STATUS_IS_EBUS) ?	\
	readl((__REG)) :			\
	sbus_readl((__REG)))
#define CS4231_READ8(__C, __REG) \
	(((__C)->status & CS_STATUS_IS_EBUS) ?	\
	readb((__REG)) :			\
	sbus_readb((__REG)))
#define CS4231_WRITE32(__C, __REG, __VAL)	\
	(((__C)->status & CS_STATUS_IS_EBUS) ?	\
         writel((__VAL), (__REG)) :		\
         sbus_writel((__VAL), (__REG)))
#define CS4231_WRITE8(__C, __REG, __VAL)	\
	(((__C)->status & CS_STATUS_IS_EBUS) ?	\
         writeb((__VAL), (__REG)) :		\
         sbus_writeb((__VAL), (__REG)))
#else
/* We can assume all is SBUS in this case. */
#define CS4231_READ32(__C, __REG) sbus_readl((__REG))
#define CS4231_READ8(__C, __REG) sbus_readb((__REG))
#define CS4231_WRITE32(__C, __REG, __VAL) sbus_writel((__VAL), (__REG))
#define CS4231_WRITE8(__C, __REG, __VAL) sbus_writeb((__VAL), (__REG))
#endif

/* Local status bits */
#define CS_STATUS_NEED_INIT 0x01
#define CS_STATUS_INIT_ON_CLOSE 0x02
#define CS_STATUS_REV_A 0x04
#define CS_STATUS_INTS_ON 0x08
#define CS_STATUS_IS_ULTRA 0x10
#define CS_STATUS_IS_EBUS 0x20

#define CS_TIMEOUT      9000000

#define GAIN_SET(var, gain)     ((var & ~(0x3f)) | gain)
#define RECGAIN_SET(var, gain)  ((var & ~(0x1f)) | gain)

/* bits 0-3 set address of register accessed by idr register */
/* bit 4 allows access to idr registers 16-31 in mode 2 only */
/* bit 5 if set causes dma transfers to cease if the int bit of status set */
#define IAR_AUTOCAL_BEGIN       0x40    /* MCE */
#define IAR_NOT_READY           0x80    /* INIT */

#define IAR_AUTOCAL_END         ~(IAR_AUTOCAL_BEGIN) /* MCD */

/* Registers 1-15 modes 1 and 2. Registers 16-31 mode 2 only */
/* Registers assumed to be same in both modes unless noted */

/* 0 - Left Input Control */
/* 1 - Right Input Control */
#define MIC_ENABLE(var)         ((var & 0x2f) | 0x80)
#define LINE_ENABLE(var)        (var & 0x2f)
#define CDROM_ENABLE(var)       ((var & 0x2f) | 0x40)
#define OUTPUTLOOP_ENABLE(var)  ((var & 0x2f) | 0xC0)
#define INPUTCR_AUX1            0x40

/* 2 - Left Aux 1 Input Control */
/* 3 - Right Aux 1 Input Control */
/* 4 - Left Aux 2 Input Control */
/* 5 - Right Aux 2 Input Control */

/* 6 - Left Output Control */
/* 7 - Right Output Control */
#define OUTCR_MUTE              0x80
#define OUTCR_UNMUTE            ~0x80

/* 8 - Playback Data Format (Mode 2) */
#define CHANGE_DFR(var, val)            ((var & ~(0xF)) | val)
#define CHANGE_ENCODING(var, val)       ((var & ~(0xe0)) | val)
#define DEFAULT_DATA_FMAT               CS4231_DFR_ULAW
#define CS4231_DFR_5512                 0x01
#define CS4231_DFR_6615                 0x0f
#define CS4231_DFR_8000                 0x00
#define CS4231_DFR_9600                 0x0e
#define CS4231_DFR_11025                0x03
#define CS4231_DFR_16000                0x02
#define CS4231_DFR_18900                0x05
#define CS4231_DFR_22050                0x07
#define CS4231_DFR_27429                0x04
#define CS4231_DFR_32000                0x06
#define CS4231_DFR_33075                0x0d
#define CS4231_DFR_37800                0x09
#define CS4231_DFR_44100                0x0b
#define CS4231_DFR_48000                0x0c
#define CS4231_DFR_LINEAR8              0x00
#define CS4231_DFR_ULAW                 0x20
#define CS4231_DFR_LINEARLE             0x40
#define CS4231_DFR_ALAW                 0x60
#define CS4231_DFR_ADPCM                0xa0 /* N/A in mode 1 */
#define CS4231_DFR_LINEARBE             0xc0 /* N/A in mode 1 */
#define CS4231_STEREO_ON(val)           (val | 0x10)
#define CS4231_MONO_ON(val)             (val & ~0x10)

/* 9 - Interface Config. Register */
#define PEN_ENABLE              (0x01) /* Playback Enable */
#define PEN_DISABLE             (~0x01)
#define CEN_ENABLE              (0x02) /* Capture Enable */
#define CEN_DISABLE             (~0x02)
#define SDC_ENABLE              (0x04) /* Turn on single DMA Channel mode */
#define ACAL_CONV               0x08   /* Turn on converter autocal */
#define ACAL_DISABLE            (~0x08) 
#define ACAL_DAC                0x10  /* Turn on DAC autocal */
#define ACAL_FULL               (ACAL_DAC|ACAL_CONV) /* Turn on full autocal */
#define PPIO                    0x20 /* do playback via PIO rather than DMA */
#define CPIO                    0x40 /* do capture via PIO rather than DMA */
#define ICR_AUTOCAL_INIT        0x01

/* 10 - Pin Control Register */
#define INTR_ON                 0x82
#define INTR_OFF                0x80
#define PINCR_LINE_MUTE         0x40
#define PINCR_HDPH_MUTE         0x80

/* 11 - Test/Initialization */
#define DRQ_STAT                0x10
#define AUTOCAL_IN_PROGRESS      0x20

/* 12 - Misc Information */
#define MISC_IR_MODE2           0x40

/* 13 - Loopback Control */
#define LOOPB_ON                0x01
#define LOOPB_OFF               0x00

/* 14 - shared play/capture upper (mode 1) */
/* 15 - shared play/capture lower (mode 1) */

/* 14 - Playback Upper (mode 2) */
/* 15 - Playback Lower (mode 2) */

/* The rest are mode 2 only */

/* 16 - Alternate Feature 1 Enable */
#define DAC_ZERO                0x01
#define PLAY_MCE                0x10
#define CAPTURE_MCE             0x20
#define TIMER_ENABLE            0x40
#define OLB_ENABLE              0x80 /* go to 2.88 vpp analog output */

/* 17 - Alternate Feature 2 Enable */
#define HPF_ON                  0x01 /* High Pass Filter */
#define XTALE_ON                0x02 /* Enable both crystals */
#define APAR_OFF                0x04 /* ADPCM playback accum reset */

/* 18 - Left Line Input Gain */
/* 19 - Right Line Input Gain */

/* 20 - Timer High */
/* 21 - Timer Low */

/* 22 - unused */

/* 23 - Alt. Fea. Ena 3 */
#define ACF 0x01

/* 24 - Alternate Feature Status */
#define CS_PU                   0x01 /* Underrun */
#define CS_PO                   0x02 /* Overrun */
#define CS_CU                   0x04 /* Underrun */
#define CS_CO                   0x08 /* Overrun */
#define CS_PI                   0x10 
#define CS_CI                   0x20 
#define CS_TI                   0x40 

/* 25 - Version */
#define CS4231A         0x20
#define CS4231CDE       0x80

/* 26 - Mono I/O Control */
#define CHANGE_MONO_GAIN(val)   ((val & ~(0xFF)) | val)
#define MONO_IOCR_BYPASS     0x20 
#define MONO_IOCR_MUTE       0x40
#define MONO_IOCR_INMUTE     0x80

/* 27 - Unused */

/* 28 - Capture Data Format */
/* see register 8 */

/* 29 - Unused */

/* 30 - Capture Upper */
/* 31 - Capture Lower */

/* Following are APC CSR register definitions for the Sparc */

#define APC_INT_PENDING 0x800000 /* Interrupt Pending */
#define APC_PLAY_INT    0x400000 /* Playback interrupt */
#define APC_CAPT_INT    0x200000 /* Capture interrupt */
#define APC_GENL_INT    0x100000 /* General interrupt */
#define APC_XINT_ENA    0x80000  /* General ext int. enable */
#define APC_XINT_PLAY   0x40000  /* Playback ext intr */
#define APC_XINT_CAPT   0x20000  /* Capture ext intr */
#define APC_XINT_GENL   0x10000  /* Error ext intr */
#define APC_XINT_EMPT   0x8000   /* Pipe empty interrupt (0 write to pva) */
#define APC_XINT_PEMP   0x4000   /* Play pipe empty (pva and pnva not set) */
#define APC_XINT_PNVA   0x2000   /* Playback NVA dirty */
#define APC_XINT_PENA   0x1000   /* play pipe empty Int enable */
#define APC_XINT_COVF   0x800    /* Cap data dropped on floor */
#define APC_XINT_CNVA   0x400    /* Capture NVA dirty */
#define APC_XINT_CEMP   0x200    /* Capture pipe empty (cva and cnva not set) */
#define APC_XINT_CENA   0x100    /* Cap. pipe empty int enable */
#define APC_PPAUSE      0x80     /* Pause the play DMA */
#define APC_CPAUSE      0x40     /* Pause the capture DMA */
#define APC_CDC_RESET   0x20     /* CODEC RESET */
#define APC_PDMA_READY  0x08     /* Play DMA Go */
#define APC_CDMA_READY  0x04     /* Capture DMA Go */
#define APC_CHIP_RESET  0x01     /* Reset the chip */

#define APC_INIT_SETUP  (APC_CDMA_READY | APC_PDMA_READY | APC_XINT_ENA | \
			 APC_XINT_PLAY | APC_XINT_GENL | APC_INT_PENDING | \
			 APC_PLAY_INT | APC_CAPT_INT | APC_GENL_INT) 

#define APC_PLAY_SETUP  (APC_GENL_INT | APC_PLAY_INT | APC_XINT_ENA | \
			 APC_XINT_PLAY | APC_XINT_EMPT | APC_XINT_GENL | \
			 APC_XINT_PENA | APC_PDMA_READY)

#define APC_CAPT_SETUP  (APC_GENL_INT | APC_CAPT_INT | APC_XINT_ENA | \
			 APC_XINT_CAPT | APC_XINT_CEMP | APC_XINT_GENL | \
			 APC_CDMA_READY)

/* Following are EB2 CSR register definitions for the Sparc */

/* asm/ebus.h has the base settings */

#define EB2_PLAY_SETUP (EBUS_DCSR_BURST_SZ_8 | EBUS_DCSR_INT_EN | EBUS_DCSR_EN_DMA | \
			EBUS_DCSR_EN_CNT | EBUS_DCSR_TC)
#define EB2_CAPT_SETUP (EBUS_DCSR_BURST_SZ_8 | EBUS_DCSR_INT_EN | EBUS_DCSR_EN_DMA| \
			EBUS_DCSR_EN_CNT | EBUS_DCSR_TC | EBUS_DCSR_WRITE)

#define CS4231_MIN_ATEN     (0)
#define CS4231_MAX_ATEN     (31)
#define CS4231_MAX_DEV_ATEN (63)

#define CS4231_MON_MIN_ATEN         (0)
#define CS4231_MON_MAX_ATEN         (63)

#define CS4231_DEFAULT_PLAYGAIN     (132)
#define CS4231_DEFAULT_RECGAIN      (126)

#define CS4231_MIN_GAIN     (0)
#define CS4231_MAX_GAIN     (15)

#define CS4231_PRECISION    (8)             /* # of bits/sample */
#define CS4231_CHANNELS     (1)             /* channels/sample */

#define CS4231_RATE   (8000)                /* default sample rate */

#endif /* _CS4231_H_ */
