/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Jun/94

**************************************************************************/

#define TRUE		1
#define FALSE		0

#define ETH_MIN_PACKET	64
#define ETH_MAX_PACKET	1518

#define VENDOR_NONE	0
#define VENDOR_WD	1
#define VENDOR_NOVELL	2

#define FLAG_PIO	0x01
#define FLAG_16BIT	0x02
#define FLAG_790	0x04

#define MEM_8192	32
#define MEM_16384	64
#define MEM_32768	128

/**************************************************************************
Western Digital/SMC Board Definitions
**************************************************************************/
#define WD_LOW_BASE	0x200
#define WD_HIGH_BASE	0x3e0
#ifndef WD_DEFAULT_MEM
#define WD_DEFAULT_MEM	0xD0000
#endif
#define WD_NIC_ADDR	0x10

/**************************************************************************
Western Digital/SMC ASIC Addresses
**************************************************************************/
#define WD_MSR		0x00
#define WD_ICR		0x01
#define WD_IAR		0x02
#define WD_BIO		0x03
#define WD_IRR		0x04
#define WD_LAAR		0x05
#define WD_IJR		0x06
#define WD_GP2		0x07
#define WD_LAR		0x08
#define WD_BID		0x0E

#define WD_ICR_16BIT	0x01

#define WD_MSR_MENB	0x40

#define WD_LAAR_L16EN	0x40
#define WD_LAAR_M16EN	0x80

#define WD_SOFTCONFIG	0x20

/**************************************************************************
Western Digital/SMC Board Types
**************************************************************************/
#define TYPE_WD8003S	0x02
#define TYPE_WD8003E	0x03
#define TYPE_WD8013EBT	0x05
#define TYPE_WD8003W	0x24
#define TYPE_WD8003EB	0x25
#define TYPE_WD8013W	0x26
#define TYPE_WD8013EP	0x27
#define TYPE_WD8013WC	0x28
#define TYPE_WD8013EPC	0x29
#define TYPE_SMC8216T	0x2a
#define TYPE_SMC8216C	0x2b
#define TYPE_SMC8013EBP	0x2c

#ifdef INCLUDE_WD
struct wd_board {
	char *name;
	char id;
	char flags;
	char memsize;
} wd_boards[] = {
	{"WD8003S",	TYPE_WD8003S,	0,			MEM_8192},
	{"WD8003E",	TYPE_WD8003E,	0,			MEM_8192},
	{"WD8013EBT",	TYPE_WD8013EBT,	FLAG_16BIT,		MEM_16384},
	{"WD8003W",	TYPE_WD8003W,	0,			MEM_8192},
	{"WD8003EB",	TYPE_WD8003EB,	0,			MEM_8192},
	{"WD8013W",	TYPE_WD8013W,	FLAG_16BIT,		MEM_16384},
	{"WD8003EP/WD8013EP",
			TYPE_WD8013EP,	0,			MEM_8192},
	{"WD8013WC",	TYPE_WD8013WC,	FLAG_16BIT,		MEM_16384},
	{"WD8013EPC",	TYPE_WD8013EPC,	FLAG_16BIT,		MEM_16384},
	{"SMC8216T",	TYPE_SMC8216T,	FLAG_16BIT | FLAG_790,	MEM_16384},
	{"SMC8216C",	TYPE_SMC8216C,	FLAG_16BIT | FLAG_790,	MEM_16384},
	{"SMC8013EBP",	TYPE_SMC8013EBP,FLAG_16BIT,		MEM_16384},
	{NULL,		0,		0}
};
#endif

/**************************************************************************
NE1000/2000 definitions
**************************************************************************/
#ifndef NE_BASE
#define NE_BASE		0x320
#endif
#define NE_ASIC_OFFSET	0x10
#define NE_RESET	0x0F		/* Used to reset card */
#define NE_DATA		0x00		/* Used to read/write NIC mem */

/**************************************************************************
8390 Register Definitions
**************************************************************************/
#define D8390_P0_COMMAND	0x00
#define D8390_P0_PSTART		0x01
#define D8390_P0_PSTOP		0x02
#define D8390_P0_BOUND		0x03
#define D8390_P0_TSR		0x04
#define	D8390_P0_TPSR		0x04
#define D8390_P0_TBCR0		0x05
#define D8390_P0_TBCR1		0x06
#define D8390_P0_ISR		0x07
#define D8390_P0_RSAR0		0x08
#define D8390_P0_RSAR1		0x09
#define D8390_P0_RBCR0		0x0A
#define D8390_P0_RBCR1		0x0B
#define D8390_P0_RSR		0x0C
#define D8390_P0_RCR		0x0C
#define D8390_P0_TCR		0x0D
#define D8390_P0_DCR		0x0E
#define D8390_P0_IMR		0x0F
#define D8390_P1_COMMAND	0x00
#define D8390_P1_PAR0		0x01
#define D8390_P1_PAR1		0x02
#define D8390_P1_PAR2		0x03
#define D8390_P1_PAR3		0x04
#define D8390_P1_PAR4		0x05
#define D8390_P1_PAR5		0x06
#define D8390_P1_CURR		0x07
#define D8390_P1_MAR0		0x08

#define D8390_COMMAND_PS0	0x0		/* Page 0 select */
#define D8390_COMMAND_PS1	0x40		/* Page 1 select */
#define D8390_COMMAND_PS2	0x80		/* Page 2 select */
#define	D8390_COMMAND_RD2	0x20		/* Remote DMA control */
#define D8390_COMMAND_RD1	0x10
#define D8390_COMMAND_RD0	0x08
#define D8390_COMMAND_TXP	0x04		/* transmit packet */
#define D8390_COMMAND_STA	0x02		/* start */
#define D8390_COMMAND_STP	0x01		/* stop */

#define D8390_RCR_MON		0x20		/* monitor mode */

#define D8390_DCR_FT1		0x40
#define D8390_DCR_LS		0x08		/* Loopback select */
#define D8390_DCR_WTS		0x01		/* Word transfer select */

#define D8390_ISR_PRX		0x01		/* successful recv */
#define D8390_ISR_PTX		0x02		/* successful xmit */
#define D8390_ISR_RXE		0x04		/* receive error */
#define D8390_ISR_TXE		0x08		/* transmit error */
#define D8390_ISR_OVW		0x10		/* Overflow */
#define D8390_ISR_CNT		0x20		/* Counter overflow */
#define D8390_ISR_RDC		0x40		/* Remote DMA complete */
#define D8390_ISR_RST		0x80		/* reset */

#define D8390_RSTAT_PRX		0x01		/* successful recv */
#define D8390_RSTAT_CRC		0x02		/* CRC error */
#define D8390_RSTAT_FAE		0x04		/* Frame alignment error */
#define D8390_RSTAT_OVER	0x08		/* overflow */

#define D8390_TXBUF_SIZE	6
#define D8390_RXBUF_END		32

struct ringbuffer {
	unsigned char status;
	unsigned char bound;
	unsigned short len;
};
