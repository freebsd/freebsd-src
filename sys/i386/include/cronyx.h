/*
 * Defines for Cronyx-Sigma adapter driver.
 *
 * Copyright (C) 1994 Cronyx Ltd.
 * Author: Serge Vakulenko, <vak@zebub.msk.su>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organizations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * Version 1.9, Wed Oct  4 18:58:15 MSK 1995
 *
 * $FreeBSD$
 */
/*
 * Asynchronous channel mode -------------------------------------------------
 */

/* Parity */
#define	PAR_EVEN	0	/* even parity */
#define	PAR_ODD		1	/* odd parity */

/* Parity mode */
#define	PARM_NOPAR	0	/* no parity */
#define	PARM_FORCE	1	/* force parity (odd = force 1, even = 0) */
#define	PARM_NORMAL	2	/* normal parity */

/* Flow control transparency mode */
#define	FLOWCC_PASS	0	/* pass flow ctl chars as exceptions */
#define FLOWCC_NOTPASS  1       /* don't pass flow ctl chars to the host */

/* Stop bit length */
#define	STOPB_1		2	/* 1 stop bit */
#define	STOPB_15	3	/* 1.5 stop bits */
#define	STOPB_2		4	/* 2 stop bits */

/* Action on break condition */
#define	BRK_INTR	0	/* generate an exception interrupt */
#define	BRK_NULL	1	/* translate to a NULL character */
#define	BRK_RESERVED	2	/* reserved */
#define	BRK_DISCARD	3	/* discard character */

/* Parity/framing error actions */
#define	PERR_INTR	0	/* generate an exception interrupt */
#define	PERR_NULL	1	/* translate to a NULL character */
#define	PERR_IGNORE	2	/* ignore error; char passed as good data */
#define	PERR_DISCARD	3	/* discard error character */
#define	PERR_FFNULL	5	/* translate to FF NULL char */

typedef struct {		/* async channel option register 1 */
	unsigned charlen : 4;	/* character length, 5..8 */
	unsigned ignpar : 1;	/* ignore parity */
	unsigned parmode : 2;	/* parity mode */
	unsigned parity : 1;	/* parity */
} cx_cor1_async_t;

typedef struct {		/* async channel option register 2 */
	unsigned dsrae : 1;	/* DSR automatic enable */
	unsigned ctsae : 1;	/* CTS automatic enable */
	unsigned rtsao : 1;	/* RTS automatic output enable */
	unsigned rlm : 1;	/* remote loopback mode enable */
	unsigned zero : 1;
	unsigned etc : 1;	/* embedded transmitter cmd enable */
	unsigned ixon : 1;	/* in-band XON/XOFF enable */
	unsigned ixany : 1;	/* XON on any character */
} cx_cor2_async_t;

typedef struct {		/* async channel option register 3 */
	unsigned stopb : 3;	/* stop bit length */
	unsigned zero : 1;
	unsigned scde : 1;	/* special char detection enable */
	unsigned flowct : 1;	/* flow control transparency mode */
	unsigned rngde : 1;	/* range detect enable */
	unsigned escde : 1;	/* extended spec. char detect enable */
} cx_cor3_async_t;

typedef struct {		/* async channel option register 6 */
	unsigned parerr : 3;	/* parity/framing error actions */
	unsigned brk : 2;	/* action on break condition */
	unsigned inlcr : 1;	/* translate NL to CR on input */
	unsigned icrnl : 1;	/* translate CR to NL on input */
	unsigned igncr : 1;	/* discard CR on input */
} cx_cor6_async_t;

typedef struct {		/* async channel option register 7 */
	unsigned ocrnl : 1;	/* translate CR to NL on output */
	unsigned onlcr : 1;	/* translate NL to CR on output */
	unsigned zero : 3;
	unsigned fcerr : 1;	/* process flow ctl err chars enable */
	unsigned lnext : 1;	/* LNext option enable */
	unsigned istrip : 1;	/* strip 8-bit on input */
} cx_cor7_async_t;

typedef struct {		/* async channel options */
	cx_cor1_async_t cor1;   /* channel option register 1 */
	cx_cor2_async_t cor2;   /* channel option register 2 */
	cx_cor3_async_t cor3;   /* option register 3 */
	cx_cor6_async_t cor6;   /* channel option register 6 */
	cx_cor7_async_t cor7;   /* channel option register 7 */
	unsigned char schr1;	/* special character register 1 (XON) */
	unsigned char schr2;	/* special character register 2 (XOFF) */
	unsigned char schr3;	/* special character register 3 */
	unsigned char schr4;	/* special character register 4 */
	unsigned char scrl;	/* special character range low */
	unsigned char scrh;	/* special character range high */
	unsigned char lnxt;	/* LNext character */
} cx_opt_async_t;

/*
 * HDLC channel mode ---------------------------------------------------------
 */
/* Address field length option */
#define	AFLO_1OCT	0	/* address field is 1 octet in length */
#define	AFLO_2OCT	1	/* address field is 2 octet in length */

/* Clear detect for X.21 data transfer phase */
#define	CLRDET_DISABLE	0	/* clear detect disabled */
#define	CLRDET_ENABLE	1	/* clear detect enabled */

/* Addressing mode */
#define	ADMODE_NOADDR	0	/* no address */
#define	ADMODE_4_1	1	/* 4 * 1 byte */
#define	ADMODE_2_2	2	/* 2 * 2 byte */

/* FCS append */
#define	FCS_NOTPASS	0	/* receive CRC is not passed to the host */
#define	FCS_PASS	1	/* receive CRC is passed to the host */

/* CRC modes */
#define	CRC_INVERT	0	/* CRC is transmitted inverted (CRC V.41) */
#define	CRC_DONT_INVERT	1	/* CRC is not transmitted inverted (CRC-16) */

/* Send sync pattern */
#define	SYNC_00		0	/* send 00h as pad char (NRZI encoding) */
#define	SYNC_AA		1	/* send AAh (Manchester/NRZ encoding) */

/* FCS preset */
#define	FCSP_ONES	0	/* FCS is preset to all ones (CRC V.41) */
#define	FCSP_ZEROS	1	/* FCS is preset to all zeros (CRC-16) */

/* idle mode */
#define	IDLE_FLAG	0	/* idle in flag */
#define	IDLE_MARK	1	/* idle in mark */

/* CRC polynomial select */
#define	POLY_V41	0	/* x^16+x^12+x^5+1 (HDLC, preset to 1) */
#define	POLY_16		1	/* x^16+x^15+x^2+1 (bisync, preset to 0) */

typedef struct {		/* hdlc channel option register 1 */
	unsigned ifflags : 4;	/* number of inter-frame flags sent */
	unsigned admode : 2;	/* addressing mode */
	unsigned clrdet : 1;	/* clear detect for X.21 data transfer phase */
	unsigned aflo : 1;	/* address field length option */
} cx_cor1_hdlc_t;

typedef struct {		/* hdlc channel option register 2 */
	unsigned dsrae : 1;	/* DSR automatic enable */
	unsigned ctsae : 1;	/* CTS automatic enable */
	unsigned rtsao : 1;	/* RTS automatic output enable */
	unsigned zero1 : 1;
	unsigned crcninv : 1;	/* CRC inversion option */
	unsigned zero2 : 1;
	unsigned fcsapd : 1;	/* FCS append */
	unsigned zero3 : 1;
} cx_cor2_hdlc_t;

typedef struct {		/* hdlc channel option register 3 */
	unsigned padcnt : 3;	/* pad character count */
	unsigned idle : 1;	/* idle mode */
	unsigned nofcs : 1;	/* FCS disable */
	unsigned fcspre : 1;	/* FCS preset */
	unsigned syncpat : 1;	/* send sync pattern */
	unsigned sndpad : 1;	/* send pad characters before flag enable */
} cx_cor3_hdlc_t;

typedef struct {		/* hdlc channel options */
	cx_cor1_hdlc_t cor1;    /* hdlc channel option register 1 */
	cx_cor2_hdlc_t cor2;    /* hdlc channel option register 2 */
	cx_cor3_hdlc_t cor3;    /* hdlc channel option register 3 */
	unsigned char rfar1;	/* receive frame address register 1 */
	unsigned char rfar2;	/* receive frame address register 2 */
	unsigned char rfar3;	/* receive frame address register 3 */
	unsigned char rfar4;	/* receive frame address register 4 */
	unsigned char cpsr;	/* CRC polynomial select */
} cx_opt_hdlc_t;

/*
 * BISYNC channel mode -------------------------------------------------------
 */

/* Longitudinal redundancy check */
#define	BCC_CRC16	0	/* CRC16 is used for BCC */
#define	BCC_LRC		1	/* LRC is used for BCC */

/* Send pad pattern */
#define	PAD_AA		0	/* send AAh as pad character */
#define	PAD_55		1	/* send 55h as pad character */

typedef struct {		/* channel option register 1 */
	unsigned charlen : 4;	/* character length, 5..8 */
	unsigned ignpar : 1;	/* ignore parity */
	unsigned parmode : 2;	/* parity mode */
	unsigned parity : 1;	/* parity */
} cx_cor1_bisync_t;

typedef struct {		/* channel option register 2 */
	unsigned syns : 4;	/* number of extra SYN chars before a frame */
	unsigned crcninv : 1;	/* CRC inversion option */
	unsigned ebcdic : 1;	/* use EBCDIC as char set (instead of ASCII) */
	unsigned bcc : 1;	/* BCC append enable */
	unsigned lrc : 1;       /* longitudinal redundancy check */
} cx_cor2_bisync_t;

typedef struct {		/* channel option register 3 */
	unsigned padcnt : 3;	/* pad character count */
	unsigned idle : 1;	/* idle mode */
	unsigned nofcs : 1;	/* FCS disable */
	unsigned fcspre : 1;	/* FCS preset */
	unsigned padpat : 1;	/* send pad pattern */
	unsigned sndpad : 1;	/* send pad characters before SYN enable */
} cx_cor3_bisync_t;

typedef struct {		/* channel option register 6 */
	unsigned char specterm;	/* special termination character */
} cx_cor6_bisync_t;

typedef struct {		/* bisync channel options */
	cx_cor1_bisync_t cor1;  /* channel option register 1 */
	cx_cor2_bisync_t cor2;  /* channel option register 2 */
	cx_cor3_bisync_t cor3;  /* channel option register 3 */
	cx_cor6_bisync_t cor6;  /* channel option register 6 */
	unsigned char cpsr;	/* CRC polynomial select */
} cx_opt_bisync_t;

/*
 * X.21 channel mode ---------------------------------------------------------
 */

/* The number of SYN chars on receive */
#define	X21SYN_2	0	/* two SYN characters are required */
#define	X21SYN_1	1	/* one SYN character is required */

typedef struct {		/* channel option register 1 */
	unsigned charlen : 4;	/* character length, 5..8 */
	unsigned ignpar : 1;	/* ignore parity */
	unsigned parmode : 2;	/* parity mode */
	unsigned parity : 1;	/* parity */
} cx_cor1_x21_t;

typedef struct {		/* channel option register 2 */
	unsigned zero1 : 5;
	unsigned etc : 1;	/* embedded transmitter command enable */
	unsigned zero2 : 2;
} cx_cor2_x21_t;

typedef struct {		/* channel option register 3 */
	unsigned zero : 4;
	unsigned scde : 1;	/* special character detect enable */
	unsigned stripsyn : 1;	/* treat SYN chars as special condition */
	unsigned ssde : 1;	/* steady state detect enable */
	unsigned syn : 1;	/* the number of SYN chars on receive */
} cx_cor3_x21_t;

typedef struct {		/* channel option register 6 */
	unsigned char synchar;	/* syn character */
} cx_cor6_x21_t;

typedef struct {		/* x21 channel options */
	cx_cor1_x21_t cor1;     /* channel option register 1 */
	cx_cor2_x21_t cor2;     /* channel option register 2 */
	cx_cor3_x21_t cor3;     /* channel option register 3 */
	cx_cor6_x21_t cor6;     /* channel option register 6 */
	unsigned char schr1;	/* special character register 1 */
	unsigned char schr2;	/* special character register 2 */
	unsigned char schr3;	/* special character register 3 */
} cx_opt_x21_t;

/*
 * CD2400 channel state structure --------------------------------------------
 */

/* Signal encoding */
#define ENCOD_NRZ        0      /* NRZ mode */
#define ENCOD_NRZI       1      /* NRZI mode */
#define ENCOD_MANCHESTER 2      /* Manchester mode */

/* Clock source */
#define CLK_0           0      /* clock 0 */
#define CLK_1           1      /* clock 1 */
#define CLK_2           2      /* clock 2 */
#define CLK_3           3      /* clock 3 */
#define CLK_4           4      /* clock 4 */
#define CLK_EXT         6      /* external clock */
#define CLK_RCV         7      /* receive clock */

/* Channel type */
#define T_NONE          0       /* no channel */
#define T_ASYNC         1       /* pure asynchronous RS-232 channel */
#define T_SYNC_RS232    2       /* pure synchronous RS-232 channel */
#define T_SYNC_V35      3       /* pure synchronous V.35 channel */
#define T_SYNC_RS449    4       /* pure synchronous RS-449 channel */
#define T_UNIV_RS232    5       /* sync/async RS-232 channel */
#define T_UNIV_RS449    6       /* sync/async RS-232/RS-449 channel */
#define T_UNIV_V35      7       /* sync/async RS-232/V.35 channel */

typedef enum {			/* channel mode */
	M_ASYNC,		/* asynchronous mode */
	M_HDLC,			/* HDLC mode */
	M_BISYNC,		/* BISYNC mode */
	M_X21			/* X.21 mode */
} cx_chan_mode_t;

typedef struct {		/* channel option register 4 */
	unsigned thr : 4;	/* FIFO threshold */
	unsigned zero : 1;
	unsigned cts_zd : 1;	/* detect 1 to 0 transition on the CTS */
	unsigned cd_zd : 1;	/* detect 1 to 0 transition on the CD */
	unsigned dsr_zd : 1;	/* detect 1 to 0 transition on the DSR */
} cx_cor4_t;

typedef struct {		/* channel option register 5 */
	unsigned rx_thr : 4;	/* receive flow control FIFO threshold */
	unsigned zero : 1;
	unsigned cts_od : 1;	/* detect 0 to 1 transition on the CTS */
	unsigned cd_od : 1;	/* detect 0 to 1 transition on the CD */
	unsigned dsr_od : 1;	/* detect 0 to 1 transition on the DSR */
} cx_cor5_t;

typedef struct {		/* receive clock option register */
	unsigned clk : 3;	/* receive clock source */
	unsigned encod : 2;     /* signal encoding NRZ/NRZI/Manchester */
	unsigned dpll : 1;      /* DPLL enable */
	unsigned zero : 1;
	unsigned tlval : 1;	/* transmit line value */
} cx_rcor_t;

typedef struct {		/* transmit clock option register */
	unsigned zero1 : 1;
	unsigned llm : 1;	/* local loopback mode */
	unsigned zero2 : 1;
	unsigned ext1x : 1;	/* external 1x clock mode */
	unsigned zero3 : 1;
	unsigned clk : 3;	/* transmit clock source */
} cx_tcor_t;

typedef struct {
	cx_cor4_t cor4;         /* channel option register 4 */
	cx_cor5_t cor5;         /* channel option register 5 */
	cx_rcor_t rcor;         /* receive clock option register */
	cx_tcor_t tcor;         /* transmit clock option register */
} cx_chan_opt_t;

typedef enum {                  /* line break mode */
	BRK_IDLE,               /* normal line mode */
	BRK_SEND,               /* start sending break */
	BRK_STOP                /* stop sending break */
} cx_break_t;

typedef struct {
	unsigned cisco : 1;     /* cisco mode */
	unsigned keepalive : 1; /* keepalive enable */
	unsigned ext : 1;       /* use external ppp implementation */
	unsigned lock : 1;      /* channel locked for use by driver */
	unsigned norts : 1;     /* disable automatic RTS control */
} cx_soft_opt_t;

#define NCHIP    4		/* the number of controllers per board */
#define NCHAN    16		/* the number of channels on the board */

typedef struct {
	unsigned char board;            /* adapter number, 0..2 */
	unsigned char channel;          /* channel number, 0..15 */
	unsigned char type;             /* channel type (read only) */
	unsigned char iftype;           /* chan0 interface RS-232/RS-449/V.35 */
	unsigned long rxbaud;		/* receiver speed */
	unsigned long txbaud;		/* transmitter speed */
	cx_chan_mode_t mode;            /* channel mode */
	cx_chan_opt_t opt;              /* common channel options */
	cx_opt_async_t aopt;            /* async mode options */
	cx_opt_hdlc_t hopt;             /* hdlc mode options */
	cx_opt_bisync_t bopt;           /* bisync mode options */
	cx_opt_x21_t xopt;              /* x.21 mode options */
	cx_soft_opt_t sopt;             /* software options and state flags */
	char master[16];                /* master interface name or \0 */
} cx_options_t;                         /* user settable options */

typedef struct _chan_t {
	unsigned char type;             /* channel type */
	unsigned char num;              /* channel number, 0..15 */
	struct _board_t *board;		/* board pointer */
	struct _chip_t *chip;		/* controller pointer */
	struct _stat_t *stat;           /* statistics */
	unsigned long rxbaud;		/* receiver speed */
	unsigned long txbaud;		/* transmitter speed */
	cx_chan_mode_t mode;            /* channel mode */
	cx_chan_opt_t opt;              /* common channel options */
	cx_opt_async_t aopt;            /* async mode options */
	cx_opt_hdlc_t hopt;             /* hdlc mode options */
	cx_opt_bisync_t bopt;           /* bisync mode options */
	cx_opt_x21_t xopt;              /* x.21 mode options */
	unsigned char *arbuf;           /* receiver A dma buffer */
	unsigned char *brbuf;           /* receiver B dma buffer */
	unsigned char *atbuf;           /* transmitter A dma buffer */
	unsigned char *btbuf;           /* transmitter B dma buffer */
	unsigned long arphys;           /* receiver A phys address */
	unsigned long brphys;           /* receiver B phys address */
	unsigned long atphys;           /* transmitter A phys address */
	unsigned long btphys;           /* transmitter B phys address */
	unsigned char dtr;              /* DTR signal value */
	unsigned char rts;              /* RTS signal value */
#ifdef _KERNEL
	struct tty *ttyp;               /* tty structure pointer */
	struct ifnet *ifp;              /* network interface data */
	struct ifnet *master;           /* master interface, or ==ifp */
	struct _chan_t *slaveq;         /* slave queue pointer, or NULL */
	cx_soft_opt_t sopt;             /* software options and state flags */
	cx_break_t brk;                 /* line break mode */
#ifdef __bsdi__
	struct ttydevice_tmp *ttydev;   /* tty statistics structure */
#endif
#endif
} cx_chan_t;

typedef struct _chip_t {
	unsigned short port;            /* base port address, or 0 if no chip */
	unsigned char num;              /* controller number, 0..3 */
	struct _board_t *board;		/* board pointer */
	unsigned long oscfreq;		/* oscillator frequency in Hz */
} cx_chip_t;

typedef struct _stat_t {
	unsigned char board;            /* adapter number, 0..2 */
	unsigned char channel;          /* channel number, 0..15 */
	unsigned long rintr;            /* receive interrupts */
	unsigned long tintr;            /* transmit interrupts */
	unsigned long mintr;            /* modem interrupts */
	unsigned long ibytes;           /* input bytes */
	unsigned long ipkts;            /* input packets */
	unsigned long ierrs;            /* input errors */
	unsigned long obytes;           /* output bytes */
	unsigned long opkts;            /* output packets */
	unsigned long oerrs;            /* output errors */
} cx_stat_t;

typedef struct _board_t {
	unsigned short port;	/* base board port, 0..3f0 */
	unsigned short num;     /* board number, 0..2 */
	unsigned char irq;      /* interrupt request {3 5 7 10 11 12 15} */
	unsigned char dma;      /* DMA request {5 6 7} */
	unsigned char if0type;  /* chan0 interface RS-232/RS-449/V.35 */
	unsigned char if8type;  /* chan8 interface RS-232/RS-449/V.35 */
	unsigned short bcr0;	/* BCR0 image */
	unsigned short bcr0b;	/* BCR0b image */
	unsigned short bcr1;	/* BCR1 image */
	unsigned short bcr1b;	/* BCR1b image */
	cx_chip_t chip[NCHIP];  /* controller structures */
	cx_chan_t chan[NCHAN];  /* channel structures */
	cx_stat_t stat[NCHAN];  /* channel statistics */
	char name[16];		/* board version name */
	unsigned char nuniv;	/* number of universal channels */
	unsigned char nsync;	/* number of sync. channels */
	unsigned char nasync;	/* number of async. channels */
} cx_board_t;

#define CX_SPEED_DFLT	9600

#ifdef _KERNEL
int cx_probe_board (int port);
void cx_init (cx_board_t *b, int num, int port, int irq, int dma);
void cx_setup_board (cx_board_t *b);
void cx_setup_chan (cx_chan_t *c);
void cx_chan_dtr (cx_chan_t *c, int on);
void cx_chan_rts (cx_chan_t *c, int on);
void cx_cmd (int base, int cmd);
int cx_chan_cd (cx_chan_t *c);
void cx_clock (long hz, long ba, int *clk, int *div);
#endif

#define CXIOCGETMODE _IOWR('x', 1, cx_options_t)   /* get channel options */
#define CXIOCSETMODE _IOW('x', 2, cx_options_t)    /* set channel options */
#define CXIOCGETSTAT _IOWR('x', 3, cx_stat_t)      /* get channel stats */
