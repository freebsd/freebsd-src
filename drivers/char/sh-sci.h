/* $Id: sh-sci.h,v 1.1.1.1.2.5 2003/09/03 08:37:41 kkojima Exp $
 *
 *  linux/drivers/char/sh-sci.h
 *
 *  SuperH on-chip serial module support.  (SCI with no FIFO / with FIFO)
 *  Copyright (C) 1999, 2000  Niibe Yutaka
 *  Copyright (C) 2000  Greg Banks
 *  Modified to support multiple serial ports. Stuart Menefy (May 2000).
 *  Modified to support SH7300(SH-Mobile) SCIF. Takashi Kusuda (Jun 2003).
 *
 */
#include <linux/config.h>

/* Values for sci_port->type */
#define PORT_SCI  0
#define PORT_SCIF 1
#define PORT_IRDA 1		/* XXX: temporary assignment */

/* Offsets into the sci_port->irqs array */
#define SCIx_ERI_IRQ 0
#define SCIx_RXI_IRQ 1
#define SCIx_TXI_IRQ 2

/*                     ERI, RXI, TXI, BRI */
#define SCI_IRQS      { 23,  24,  25,   0 }
#define SH3_SCIF_IRQS { 56,  57,  59,  58 }
#define SH3_IRDA_IRQS { 52,  53,  55,  54 }
#define SH4_SCIF_IRQS { 40,  41,  43,  42 }
#define STB1_SCIF1_IRQS {23, 24,  26,  25 }
#define SH5_SCIF_IRQS  { 39, 40, 42 }
#define SH7300_SCIF0_IRQS {80,  80,  80,  80 }

#if defined(CONFIG_CPU_SUBTYPE_SH7708)
# define SCI_NPORTS 1
# define SCI_INIT { \
  { {}, PORT_SCI,  0xfffffe80, SCI_IRQS,      sci_init_pins_sci  } \
}
# define SCSPTR 0xffffff7c /* 8 bit */
# define SCSCR_INIT(port)          0x30 /* TIE=0,RIE=0,TE=1,RE=1 */
# define SCI_ONLY
#elif defined(CONFIG_CPU_SUBTYPE_SH7300)
# define SCI_NPORTS 1
# define SCI_INIT { \
  { {}, PORT_SCIF,  0xA4430000, SH7300_SCIF0_IRQS, sci_init_pins_scif }, \
}
# define SCPCR  0xA4050116        /* 16 bit SCIF */
# define SCPDR  0xA4050136        /* 16 bit SCIF */
# define SCSCR_INIT(port)  0x0030 /* TIE=0,RIE=0,TE=1,RE=1 */
# define SCIF_ONLY

#elif defined(CONFIG_CPU_SUBTYPE_SH7707) || defined(CONFIG_CPU_SUBTYPE_SH7709)
# define SCI_NPORTS 3
# define SCI_INIT { \
  { {}, PORT_SCI,  0xfffffe80, SCI_IRQS,      sci_init_pins_sci  }, \
  { {}, PORT_SCIF, 0xA4000150, SH3_SCIF_IRQS, sci_init_pins_scif }, \
  { {}, PORT_SCIF, 0xA4000140, SH3_IRDA_IRQS, sci_init_pins_irda }  \
}
# define SCPCR  0xA4000116 /* 16 bit SCI and SCIF */
# define SCPDR  0xA4000136 /* 8  bit SCI and SCIF */
# define SCSCR_INIT(port)          0x30 /* TIE=0,RIE=0,TE=1,RE=1 */
# define SCI_AND_SCIF
#elif defined(CONFIG_CPU_SUBTYPE_SH7750) || defined(CONFIG_CPU_SUBTYPE_SH7751)
# define SCI_NPORTS 2
# define SCI_INIT { \
  { {}, PORT_SCI,  0xffe00000, SCI_IRQS,      sci_init_pins_sci  }, \
  { {}, PORT_SCIF, 0xFFE80000, SH4_SCIF_IRQS, sci_init_pins_scif }  \
}
# define SCSPTR1 0xffe0001c /* 8  bit SCI */
# define SCSPTR2 0xFFE80020 /* 16 bit SCIF */
# define SCIF_ORER 0x0001   /* overrun error bit */
# define SCSCR_INIT(port) (((port)->type == PORT_SCI) ? \
	0x30 /* TIE=0,RIE=0,TE=1,RE=1 */ : \
	0x38 /* TIE=0,RIE=0,TE=1,RE=1,REIE=1 */ )
# define SCI_AND_SCIF
#elif defined(CONFIG_CPU_SUBTYPE_SH4_202)
# define SCI_NPORTS 1
# define SCI_INIT { \
  { {}, PORT_SCIF, 0xFFE80000, SH4_SCIF_IRQS, sci_init_pins_scif }  \
}
# define SCSPTR2 0xFFE80020 /* 16 bit SCIF */
# define SCIF_ORER 0x0001   /* overrun error bit */
# define SCSCR_INIT(port) 0x38 /* TIE=0,RIE=0,TE=1,RE=1,REIE=1 */
# define SCIF_ONLY
#elif defined(CONFIG_CPU_SUBTYPE_ST40)
# define SCI_NPORTS 2
# define SCI_INIT { \
  { {}, PORT_SCIF, 0xffe00000, STB1_SCIF1_IRQS, sci_init_pins_scif }, \
  { {}, PORT_SCIF, 0xffe80000, SH4_SCIF_IRQS,   sci_init_pins_scif }  \
}
# define SCSPTR1 0xffe00020 /* 16 bit SCIF */
# define SCSPTR2 0xffe80020 /* 16 bit SCIF */
# define SCIF_ORER 0x0001   /* overrun error bit */
# define SCSCR_INIT(port)          0x38 /* TIE=0,RIE=0,TE=1,RE=1,REIE=1 */
# define SCIF_ONLY

#elif defined(CONFIG_CPU_SUBTYPE_SH5_101) || defined(CONFIG_CPU_SUBTYPE_SH5_103)
# include <asm/hardware.h>
# define SCIF_BASE_ADDR    0x01030000 
# define SCIF_ADDR_SH5     PHYS_PERIPHERAL_BLOCK+SCIF_BASE_ADDR
# define SCIF_PTR2_OFFS    0x0000020
# define SCIF_LSR2_OFFS    0x0000024
# define SCI_NPORTS 1
# define SCI_INIT { \
  { {}, PORT_SCIF, 0, \
     SH5_SCIF_IRQS, sci_init_pins_scif }  \
}
# define SCSPTR2           (SCIF_ADDR_SH5+SCIF_PTR2_OFFS) /* 16 bit SCIF */
# define SCLSR2            (SCIF_ADDR_SH5+SCIF_LSR2_OFFS) /* 16 bit SCIF */
# define SCSCR_INIT(port)  0x38                           /* TIE=0,RIE=0,
							     TE=1,RE=1,REIE=1 */
# define SCIF_ONLY


#else
# error CPU subtype not defined
#endif

/* SCSCR */
#define SCI_CTRL_FLAGS_TIE  0x80 /* all */
#define SCI_CTRL_FLAGS_RIE  0x40 /* all */
#define SCI_CTRL_FLAGS_TE   0x20 /* all */
#define SCI_CTRL_FLAGS_RE   0x10 /* all */
/*      SCI_CTRL_FLAGS_REIE 0x08  * 7750 SCIF */
/*      SCI_CTRL_FLAGS_MPIE 0x08  * 7707 SCI, 7708 SCI, 7709 SCI, 7750 SCI */
/*      SCI_CTRL_FLAGS_TEIE 0x04  * 7707 SCI, 7708 SCI, 7709 SCI, 7750 SCI */
/*      SCI_CTRL_FLAGS_CKE1 0x02  * all */
/*      SCI_CTRL_FLAGS_CKE0 0x01  * 7707 SCI/SCIF, 7708 SCI, 7709 SCI/SCIF, 7750 SCI */

/* SCxSR SCI */
#define SCI_TDRE  0x80 /* 7707 SCI, 7708 SCI, 7709 SCI, 7750 SCI */
#define SCI_RDRF  0x40 /* 7707 SCI, 7708 SCI, 7709 SCI, 7750 SCI */
#define SCI_ORER  0x20 /* 7707 SCI, 7708 SCI, 7709 SCI, 7750 SCI */
#define SCI_FER   0x10 /* 7707 SCI, 7708 SCI, 7709 SCI, 7750 SCI */
#define SCI_PER   0x08 /* 7707 SCI, 7708 SCI, 7709 SCI, 7750 SCI */
#define SCI_TEND  0x04 /* 7707 SCI, 7708 SCI, 7709 SCI, 7750 SCI */
/*      SCI_MPB   0x02  * 7707 SCI, 7708 SCI, 7709 SCI, 7750 SCI */
/*      SCI_MPBT  0x01  * 7707 SCI, 7708 SCI, 7709 SCI, 7750 SCI */

#define SCI_ERRORS ( SCI_PER | SCI_FER | SCI_ORER)

/* SCxSR SCIF */
#define SCIF_ER    0x0080 /* 7707 SCIF, 7709 SCIF, 7750 SCIF */
#define SCIF_TEND  0x0040 /* 7707 SCIF, 7709 SCIF, 7750 SCIF */
#define SCIF_TDFE  0x0020 /* 7707 SCIF, 7709 SCIF, 7750 SCIF */
#define SCIF_BRK   0x0010 /* 7707 SCIF, 7709 SCIF, 7750 SCIF */
#define SCIF_FER   0x0008 /* 7707 SCIF, 7709 SCIF, 7750 SCIF */
#define SCIF_PER   0x0004 /* 7707 SCIF, 7709 SCIF, 7750 SCIF */
#define SCIF_RDF   0x0002 /* 7707 SCIF, 7709 SCIF, 7750 SCIF */
#define SCIF_DR    0x0001 /* 7707 SCIF, 7709 SCIF, 7750 SCIF */

#if defined(CONFIG_CPU_SUBTYPE_SH7300)
#define SCIF_ORER    0x0200
#define SCIF_ERRORS ( SCIF_PER | SCIF_FER | SCIF_ER | SCIF_BRK | SCIF_ORER)
#else
#define SCIF_ERRORS ( SCIF_PER | SCIF_FER | SCIF_ER | SCIF_BRK)
#endif

#if defined(SCI_ONLY)
# define SCxSR_TEND(port)		SCI_TEND
# define SCxSR_ERRORS(port)		SCI_ERRORS
# define SCxSR_RDxF(port)               SCI_RDRF
# define SCxSR_TDxE(port)               SCI_TDRE
# define SCxSR_ORER(port)		SCI_ORER
# define SCxSR_FER(port)		SCI_FER
# define SCxSR_PER(port)		SCI_PER
# define SCxSR_BRK(port)		0x00
# define SCxSR_RDxF_CLEAR(port)		0xbc
# define SCxSR_ERROR_CLEAR(port)	0xc4
# define SCxSR_TDxE_CLEAR(port)		0x78
# define SCxSR_BREAK_CLEAR(port)   	0xc4
#elif defined(SCIF_ONLY) 
# define SCxSR_TEND(port)		SCIF_TEND
# define SCxSR_ERRORS(port)		SCIF_ERRORS
# define SCxSR_RDxF(port)               SCIF_RDF
# define SCxSR_TDxE(port)               SCIF_TDFE
#if defined(CONFIG_CPU_SUBTYPE_SH7300)
# define SCxSR_ORER(port)		SCIF_ORER
#else
# define SCxSR_ORER(port)		0x0000
#endif
# define SCxSR_FER(port)		SCIF_FER
# define SCxSR_PER(port)		SCIF_PER
# define SCxSR_BRK(port)		SCIF_BRK
#if defined(CONFIG_CPU_SUBTYPE_SH7300)
# define SCxSR_RDxF_CLEAR(port)         (sci_in(port,SCxSR)&0xfffc)
# define SCxSR_ERROR_CLEAR(port)        (sci_in(port,SCxSR)&0xfd73)
# define SCxSR_TDxE_CLEAR(port)         (sci_in(port,SCxSR)&0xffdf)
# define SCxSR_BREAK_CLEAR(port)        (sci_in(port,SCxSR)&0xffe3)
#else
# define SCxSR_RDxF_CLEAR(port)		0x00fc
# define SCxSR_ERROR_CLEAR(port)	0x0073
# define SCxSR_TDxE_CLEAR(port)		0x00df
# define SCxSR_BREAK_CLEAR(port)   	0x00e3
#endif
#else
# define SCxSR_TEND(port)	 (((port)->type == PORT_SCI) ? SCI_TEND   : SCIF_TEND)
# define SCxSR_ERRORS(port)	 (((port)->type == PORT_SCI) ? SCI_ERRORS : SCIF_ERRORS)
# define SCxSR_RDxF(port)        (((port)->type == PORT_SCI) ? SCI_RDRF   : SCIF_RDF)
# define SCxSR_TDxE(port)        (((port)->type == PORT_SCI) ? SCI_TDRE   : SCIF_TDFE)
# define SCxSR_ORER(port)        (((port)->type == PORT_SCI) ? SCI_ORER   : 0x0000)
# define SCxSR_FER(port)         (((port)->type == PORT_SCI) ? SCI_FER    : SCIF_FER)
# define SCxSR_PER(port)         (((port)->type == PORT_SCI) ? SCI_PER    : SCIF_PER)
# define SCxSR_BRK(port)         (((port)->type == PORT_SCI) ? 0x00       : SCIF_BRK)
# define SCxSR_RDxF_CLEAR(port)	 (((port)->type == PORT_SCI) ? 0xbc : 0x00fc)
# define SCxSR_ERROR_CLEAR(port) (((port)->type == PORT_SCI) ? 0xc4 : 0x0073)
# define SCxSR_TDxE_CLEAR(port)  (((port)->type == PORT_SCI) ? 0x78 : 0x00df)
# define SCxSR_BREAK_CLEAR(port) (((port)->type == PORT_SCI) ? 0xc4 : 0x00e3)
#endif

/* SCFCR */
#define SCFCR_RFRST 0x0002
#define SCFCR_TFRST 0x0004
#define SCFCR_TCRST 0x4000
#define SCFCR_MCE   0x0008

#define SCI_MAJOR		204
#define SCI_MINOR_START		8

/* Generic serial flags */
#define SCI_RX_THROTTLE		0x0000001

#define SCI_MAGIC 0xbabeface

/*
 * Events are used to schedule things to happen at timer-interrupt
 * time, instead of at rs interrupt time.
 */
#define SCI_EVENT_WRITE_WAKEUP	0

struct sci_port {
	struct gs_port gs;
	int type;
	unsigned int base;
	unsigned char irqs[4]; /* ERI, RXI, TXI, BRI */
	void (*init_pins)(struct sci_port* port, unsigned int cflag);
	unsigned int old_cflag;
	struct async_icount icount;
	struct tq_struct tqueue;
	unsigned long event;
	int break_flag;
};

#define SCI_IN(size, offset)					\
  unsigned int addr = port->base + (offset);			\
  if ((size) == 8) { 						\
    return ctrl_inb(addr);					\
  } else {					 		\
    return ctrl_inw(addr);					\
  }
#define SCI_OUT(size, offset, value)				\
  unsigned int addr = port->base + (offset);			\
  if ((size) == 8) { 						\
    ctrl_outb(value, addr);					\
  } else {							\
    ctrl_outw(value, addr);					\
  }

#define CPU_SCIx_FNS(name, sci_offset, sci_size, scif_offset, scif_size)\
  static inline unsigned int sci_##name##_in(struct sci_port* port)	\
  {									\
    if (port->type == PORT_SCI) { 					\
      SCI_IN(sci_size, sci_offset)					\
    } else {								\
      SCI_IN(scif_size, scif_offset);		 			\
    }									\
  }									\
  static inline void sci_##name##_out(struct sci_port* port, unsigned int value) \
  {									\
    if (port->type == PORT_SCI) {					\
      SCI_OUT(sci_size, sci_offset, value)				\
    } else {								\
      SCI_OUT(scif_size, scif_offset, value);				\
    }									\
  }

#define CPU_SCIF_FNS(name, scif_offset, scif_size)				\
  static inline unsigned int sci_##name##_in(struct sci_port* port)	\
  {									\
    SCI_IN(scif_size, scif_offset);		 			\
  }									\
  static inline void sci_##name##_out(struct sci_port* port, unsigned int value) \
  {									\
    SCI_OUT(scif_size, scif_offset, value);				\
  }

#ifdef __sh3__
#if defined(CONFIG_CPU_SUBTYPE_SH7300)
#define SCIF_FNS(name, scif_offset, scif_size) \
  CPU_SCIF_FNS(name, scif_offset, scif_size)
#else
#define SCIx_FNS(name, sh3_sci_offset, sh3_sci_size, sh4_sci_offset, sh4_sci_size, \
		 sh3_scif_offset, sh3_scif_size, sh4_scif_offset, sh4_scif_size) \
  CPU_SCIx_FNS(name, sh3_sci_offset, sh3_sci_size, sh3_scif_offset, sh3_scif_size)
#define SCIF_FNS(name, sh3_scif_offset, sh3_scif_size, sh4_scif_offset, sh4_scif_size) \
  CPU_SCIF_FNS(name, sh3_scif_offset, sh3_scif_size)
#endif
#else
#define SCIx_FNS(name, sh3_sci_offset, sh3_sci_size, sh4_sci_offset, sh4_sci_size, \
		 sh3_scif_offset, sh3_scif_size, sh4_scif_offset, sh4_scif_size) \
  CPU_SCIx_FNS(name, sh4_sci_offset, sh4_sci_size, sh4_scif_offset, sh4_scif_size)
#define SCIF_FNS(name, sh3_scif_offset, sh3_scif_size, sh4_scif_offset, sh4_scif_size) \
  CPU_SCIF_FNS(name, sh4_scif_offset, sh4_scif_size)
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7300)
SCIF_FNS(SCSMR,  0x00, 16)
SCIF_FNS(SCBRR,  0x04,  8)
SCIF_FNS(SCSCR,  0x08, 16)
SCIF_FNS(SCTDSR, 0x0c,  8)
SCIF_FNS(SCFER,  0x10, 16)
SCIF_FNS(SCxSR,  0x14, 16)
SCIF_FNS(SCFCR,  0x18, 16)
SCIF_FNS(SCFDR,  0x1c, 16)
SCIF_FNS(SCxTDR, 0x20,  8)
SCIF_FNS(SCxRDR, 0x24,  8)
#else
/*      reg      SCI/SH3   SCI/SH4  SCIF/SH3   SCIF/SH4  */
/*      name     off  sz   off  sz   off  sz   off  sz   */
SCIx_FNS(SCSMR,  0x00,  8, 0x00,  8, 0x00,  8, 0x00, 16)
SCIx_FNS(SCBRR,  0x02,  8, 0x04,  8, 0x02,  8, 0x04,  8)
SCIx_FNS(SCSCR,  0x04,  8, 0x08,  8, 0x04,  8, 0x08, 16)
SCIx_FNS(SCxTDR, 0x06,  8, 0x0c,  8, 0x06,  8, 0x0C,  8)
SCIx_FNS(SCxSR,  0x08,  8, 0x10,  8, 0x08, 16, 0x10, 16)
SCIx_FNS(SCxRDR, 0x0a,  8, 0x14,  8, 0x0A,  8, 0x14,  8)
SCIF_FNS(SCFCR,                      0x0c,  8, 0x18, 16)
SCIF_FNS(SCFDR,                      0x0e, 16, 0x1C, 16)
SCIF_FNS(SCSPTR,			0,  0, 0x20, 16)
SCIF_FNS(SCLSR,                         0,  0, 0x24, 16)
#endif

#define sci_in(port, reg) sci_##reg##_in(port)
#define sci_out(port, reg, value) sci_##reg##_out(port, value)

#if defined(CONFIG_CPU_SUBTYPE_SH7708)
static inline int sci_rxd_in(struct sci_port *port)
{
	if (port->base == 0xfffffe80)
		return ctrl_inb(SCSPTR)&0x01 ? 1 : 0; /* SCI */
	return 1;
}
#elif defined(CONFIG_CPU_SUBTYPE_SH7300)
static inline int sci_rxd_in(struct sci_port *port)
{
        if (port->base == 0xa4430000)
                return ctrl_inb(SCPDR)&0x01 ? 1 : 0; /* SCIF0 */
        return 1;
}
#elif defined(CONFIG_CPU_SUBTYPE_SH7707) || defined(CONFIG_CPU_SUBTYPE_SH7709)
static inline int sci_rxd_in(struct sci_port *port)
{
	if (port->base == 0xfffffe80)
		return ctrl_inb(SCPDR)&0x01 ? 1 : 0; /* SCI */
	if (port->base == 0xa4000150)
		return ctrl_inb(SCPDR)&0x10 ? 1 : 0; /* SCIF */
	if (port->base == 0xa4000140)
		return ctrl_inb(SCPDR)&0x04 ? 1 : 0; /* IRDA */
	return 1;
}
#elif defined(CONFIG_CPU_SUBTYPE_SH7750) || defined(CONFIG_CPU_SUBTYPE_SH7751) || defined(CONFIG_CPU_SUBTYPE_SH4_202)
static inline int sci_rxd_in(struct sci_port *port)
{
#ifndef SCIF_ONLY
	if (port->base == 0xffe00000)
		return ctrl_inb(SCSPTR1)&0x01 ? 1 : 0; /* SCI */
#endif
#ifndef SCI_ONLY
	if (port->base == 0xffe80000)
		return ctrl_inw(SCSPTR2)&0x0001 ? 1 : 0; /* SCIF */
#endif
	return 1;
}
#elif defined(CONFIG_CPU_SUBTYPE_ST40)
static inline int sci_rxd_in(struct sci_port *port)
{
	if (port->base == 0xffe00000)
		return ctrl_inw(SCSPTR1)&0x0001 ? 1 : 0; /* SCIF */
	else
		return ctrl_inw(SCSPTR2)&0x0001 ? 1 : 0; /* SCIF */

}
#elif defined(CONFIG_CPU_SUBTYPE_SH5_101) || defined(CONFIG_CPU_SUBTYPE_SH5_103)
static inline int sci_rxd_in(struct sci_port *port)
{
         return sci_in(port, SCSPTR)&0x0001 ? 1 : 0; /* SCIF */
}

#endif

/*
 * Values for the BitRate Register (SCBRR)
 *
 * The values are actually divisors for a frequency which can
 * be internal to the SH3 (14.7456MHz) or derived from an external
 * clock source.  This driver assumes the internal clock is used;
 * to support using an external clock source, config options or
 * possibly command-line options would need to be added.
 *
 * Also, to support speeds below 2400 (why?) the lower 2 bits of
 * the SCSMR register would also need to be set to non-zero values.
 *
 * -- Greg Banks 27Feb2000
 *
 * Answer: The SCBRR register is only eight bits, and the value in
 * it gets larger with lower baud rates. At around 2400 (depending on
 * the peripherial module clock) you run out of bits. However the
 * lower two bits of SCSMR allow the module clock to be divided down,
 * scaling the value which is needed in SCBRR.
 *
 * -- Stuart Menefy - 23 May 2000
 *
 * I meant, why would anyone bother with bitrates below 2400.
 *
 * -- Greg Banks - 7Jul2000
 *
 * You "speedist"!  How will I use my 110bps ASR-33 teletype with paper
 * tape reader as a console!
 *
 * -- Mitch Davis - 15 Jul 2000
 */

#define PCLK           (current_cpu_data.module_clock)

#if defined(CONFIG_CPU_SUBTYPE_SH7300)
#define SCBRR_VALUE(bps) ((PCLK+16*bps)/(16*bps)-1)
#else
#define SCBRR_VALUE(bps) ((PCLK+16*bps)/(32*bps)-1)
#endif
#define BPS_2400       SCBRR_VALUE(2400)
#define BPS_4800       SCBRR_VALUE(4800)
#define BPS_9600       SCBRR_VALUE(9600)
#define BPS_19200      SCBRR_VALUE(19200)
#define BPS_38400      SCBRR_VALUE(38400)
#define BPS_57600      SCBRR_VALUE(57600)
#define BPS_115200     SCBRR_VALUE(115200)
#define BPS_230400     SCBRR_VALUE(230400)

