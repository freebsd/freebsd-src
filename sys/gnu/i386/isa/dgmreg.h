/*-
 *  dgmreg.h $FreeBSD$
 *
 *  Digiboard driver.
 *
 *  Stage 1. "Better than nothing".
 *
 *  Based on sio driver by Bruce Evans and on Linux driver by Troy 
 *  De Jongh <troyd@digibd.com> or <troyd@skypoint.com> 
 *  which is under GNU General Public License version 2 so this driver 
 *  is forced to be under GPL 2 too.
 *
 *  Written by Serge Babkin,
 *      Joint Stock Commercial Bank "Chelindbank"
 *      (Chelyabinsk, Russia)
 *      babkin@hq.icb.chel.su
 */

#define MAX_DGM_PORTS 64

/* digi.h */
/*          Definitions for DigiBoard ditty(1) command.                 */

#if !defined(TIOCMODG)
#define	TIOCMODG	('d'<<8) | 250		/* get modem ctrl state	*/
#define	TIOCMODS	('d'<<8) | 251		/* set modem ctrl state	*/
#endif

#if !defined(TIOCMSET)
#define	TIOCMSET	('d'<<8) | 252		/* set modem ctrl state	*/
#define	TIOCMGET	('d'<<8) | 253		/* set modem ctrl state	*/
#endif

#if !defined(TIOCMBIC)
#define	TIOCMBIC	('d'<<8) | 254		/* set modem ctrl state */
#define	TIOCMBIS	('d'<<8) | 255		/* set modem ctrl state */
#endif

#if !defined(TIOCSDTR)
#define	TIOCSDTR	('e'<<8) | 0		/* set DTR		*/
#define	TIOCCDTR	('e'<<8) | 1		/* clear DTR		*/
#endif

/************************************************************************
 * Ioctl command arguments for DIGI parameters.
 ************************************************************************/
#define DIGI_GETA	('e'<<8) | 94		/* Read params		*/

#define DIGI_SETA	('e'<<8) | 95		/* Set params		*/
#define DIGI_SETAW	('e'<<8) | 96		/* Drain & set params	*/
#define DIGI_SETAF	('e'<<8) | 97		/* Drain, flush & set params */

#define	DIGI_GETFLOW	('e'<<8) | 99		/* Get startc/stopc flow */
						/* control characters 	 */
#define	DIGI_SETFLOW	('e'<<8) | 100		/* Set startc/stopc flow */
						/* control characters	 */
#define	DIGI_GETAFLOW	('e'<<8) | 101		/* Get Aux. startc/stopc */
						/* flow control chars 	 */
#define	DIGI_SETAFLOW	('e'<<8) | 102		/* Set Aux. startc/stopc */
						/* flow control chars	 */

struct	digiflow_struct {
	unsigned char	startc;				/* flow cntl start char	*/
	unsigned char	stopc;				/* flow cntl stop char	*/
};

typedef struct digiflow_struct digiflow_t;


/************************************************************************
 * Values for digi_flags 
 ************************************************************************/
#define DIGI_IXON	0x0001		/* Handle IXON in the FEP	*/
#define DIGI_FAST	0x0002		/* Fast baud rates		*/
#define RTSPACE		0x0004		/* RTS input flow control	*/
#define CTSPACE		0x0008		/* CTS output flow control	*/
#define DSRPACE		0x0010		/* DSR output flow control	*/
#define DCDPACE		0x0020		/* DCD output flow control	*/
#define DTRPACE		0x0040		/* DTR input flow control	*/
#define DIGI_FORCEDCD	0x0100		/* Force carrier		*/
#define	DIGI_ALTPIN	0x0200		/* Alternate RJ-45 pin config	*/
#define	DIGI_AIXON	0x0400		/* Aux flow control in fep	*/


/************************************************************************
 * Structure used with ioctl commands for DIGI parameters.
 ************************************************************************/
struct digi_struct {
	unsigned short	digi_flags;		/* Flags (see above)	*/
};

typedef struct digi_struct digi_t;

/* fep.h */

#define FEP_CSTART       0x400L
#define FEP_CMAX         0x800L
#define FEP_ISTART       0x800L
#define FEP_IMAX         0xC00L
#define FEP_CIN          0xD10L
#define FEP_GLOBAL       0xD10L
#define FEP_EIN          0xD18L
#define FEPSTAT      0xD20L
#define CHANSTRUCT   0x1000L
#define RXTXBUF      0x4000L


struct global_data {
	volatile ushort cin;
	volatile ushort cout;
	volatile ushort cstart;
	volatile ushort cmax;
	volatile ushort ein;
	volatile ushort eout;
	volatile ushort istart;
	volatile ushort imax;
};


struct board_chan {
	int filler1; 
	int filler2;
	volatile ushort tseg;
	volatile ushort tin;
	volatile ushort tout;
	volatile ushort tmax;
	
	volatile ushort rseg;
	volatile ushort rin;
	volatile ushort rout;
	volatile ushort rmax;
	
	volatile ushort tlow;
	volatile ushort rlow;
	volatile ushort rhigh;
	volatile ushort incr;
	
	volatile ushort etime;
	volatile ushort edelay;
	volatile u_char *dev;
	
	volatile ushort iflag;
	volatile ushort oflag;
	volatile ushort cflag;
	volatile ushort gmask;
	
	volatile ushort col;
	volatile ushort delay;
	volatile ushort imask;
	volatile ushort tflush;

	int filler3;
	int filler4;
	int filler5;
	int filler6;
	
	volatile u_char num;
	volatile u_char ract;
	volatile u_char bstat;
	volatile u_char tbusy;
	volatile u_char iempty;
	volatile u_char ilow;
	volatile u_char idata;
	volatile u_char eflag;
	
	volatile u_char tflag;
	volatile u_char rflag;
	volatile u_char xmask;
	volatile u_char xval;
	volatile u_char mstat;
	volatile u_char mchange;
	volatile u_char mint;
	volatile u_char lstat;

	volatile u_char mtran;
	volatile u_char orun;
	volatile u_char startca;
	volatile u_char stopca;
	volatile u_char startc;
	volatile u_char stopc;
	volatile u_char vnext;
	volatile u_char hflow;

	volatile u_char fillc;
	volatile u_char ochar;
	volatile u_char omask;

	u_char filler7;
	u_char filler8[28];
}; 


#define SRXLWATER      0xE0
#define SRXHWATER      0xE1
#define STOUT          0xE2
#define PAUSETX        0xE3
#define RESUMETX       0xE4
#define SAUXONOFFC     0xE6
#define SENDBREAK      0xE8
#define SETMODEM       0xE9
#define SETIFLAGS      0xEA
#define SONOFFC        0xEB
#define STXLWATER      0xEC
#define PAUSERX        0xEE
#define RESUMERX       0xEF
#define SETBUFFER      0xF2
#define SETCOOKED      0xF3
#define SETHFLOW       0xF4
#define SETCTRLFLAGS   0xF5
#define SETVNEXT       0xF6



#define BREAK_IND        0x01
#define LOWTX_IND        0x02
#define EMPTYTX_IND      0x04
#define DATA_IND         0x08
#define MODEMCHG_IND     0x20

#define ALL_IND	(BREAK_IND|LOWTX_IND|EMPTYTX_IND|DATA_IND|MODEMCHG_IND)

#define CD    0x80	
#define DSR   0x20
#define CTS   0x10
#define DTR   0x01
#define RTS   0x02
#define RI    0x40

#define FEPCODESEG  0x0200L
#define FEPCODE     0x2000L
#define BIOSCODE    0xf800L
#define BIOSOFFSET  0x1000L

#define MISCGLOBAL  0x0C00L
#define NPORT       0x0C02L
#define MBOX        0x0C40L
#define BOTWIN      0x100L
#define TOPWIN      0xFF00L

#define FEPCLR      0x00
#define FEPMEM      0x02
#define FEPRST      0x04
#define FEPINT      0x08
#define	FEPMASK     0x0e
#define	FEPWIN      0x80

#define PCXI    0
#define PCXE    1
#define	PCXEVE	2
#define	PCXEM	3

static char * const board_desc[] = {
	"PC/Xi (64K)",
	"PC/Xe (64K)",
	"PC/Xe (8K) ",
	"PC/Xem ",
};

#define STARTC      021
#define STOPC       023
#define IAIXON      0x2000


struct board_info	{
	u_char status;
	u_char type;
	u_char altpin;
	ushort numports;
	ushort port;
	u_long  membase;
};


#define TXSTOPPED   0x1
#define LOWWAIT		0x2
#define EMPTYWAIT	0x4

#define DISABLED   0
#define ENABLED    1
#define OFF        0
#define ON         1

#define FEPTIMEOUT 200000  
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2
#define PCXE_EVENT_HANGUP   1

struct channel {
	u_char unit;           /* board unit number */
	u_char omodem;         /* FEP output modem status     */
	u_char imodem;         /* FEP input modem status      */
	u_char modemfake;      /* Modem values to be forced   */
	u_char modem;          /* Force values                */
	u_char hflow;
	u_char dsr;
	u_char dcd;
	u_char stopc;
	u_char startc;
	u_char stopca;
	u_char startca;
	u_char fepstopc;
	u_char fepstartc;
	u_char fepstopca;
	u_char fepstartca;
	u_char txwin;
	u_char rxwin;
	ushort fepiflag;
	ushort fepcflag;
	ushort fepoflag;
	ushort txbufhead;
	ushort txbufsize;
	ushort rxbufhead;
	ushort rxbufsize;
	int close_delay;
	int count;
	int blocked_open;
	int event;
	int asyncflags;
	uint dev;
	long session;
	long pgrp;
	u_long statusflags;
	u_long c_iflag;
	u_long c_cflag;
	u_long c_lflag;
	u_long c_oflag;
	u_char *txptr;
	u_char *rxptr;
	struct board_info *board;
	struct board_chan *brdchan;
	struct digi_struct digiext;
	struct tty *tty;
	struct termios normal_termios;
	struct termios callout_termios;
	volatile struct global_data *mailbox;
};

/* flags for configuring */

#define	DGBFLAG_ALTPIN	0x0001	/* chande DCD and DCD */
#define DGBFLAG_NOWIN	0x0002	/* use windowed PC/Xe as non-windowed */

#define DB_RD	  0x0001
#define DB_WR	  0x0002
#define DB_WIN	  0x0004
#define DB_INFO   0x0008
#define DB_EXCEPT 0x0010
#define DB_OPEN   0x0100
#define DB_CLOSE  0x0200
#define DB_DATA   0x0400
#define DB_RXDATA 0x0401
#define DB_TXDATA 0x0402
#define DB_EVENT  0x0800
#define DB_MODEM  0x1000
#define DB_BREAK  0x2000
#define DB_PARAM  0x4000
#define DB_FEP    0x8000

/* debugging printout */

#ifdef DEBUG
#define DPRINT1(l,a1)			(dgmdebug&l ? printf(a1) : 0)
#define DPRINT2(l,a1,a2)		(dgmdebug&l ? printf(a1,a2) : 0)
#define DPRINT3(l,a1,a2,a3)		(dgmdebug&l ? printf(a1,a2,a3) : 0)
#define DPRINT4(l,a1,a2,a3,a4)		(dgmdebug&l ? printf(a1,a2,a3,a4) : 0)
#define DPRINT5(l,a1,a2,a3,a4,a5)	(dgmdebug&l ? printf(a1,a2,a3,a4,a5) : 0)
#define DPRINT6(l,a1,a2,a3,a4,a5,a6)	(dgmdebug&l ? printf(a1,a2,a3,a4,a5,a6) : 0)
#define DPRINT7(l,a1,a2,a3,a4,a5,a6,a7) (dgmdebug&l ? printf(a1,a2,a3,a4,a5,a6,a7) : 0)
#else
#define DPRINT1(l,a1)
#define DPRINT2(l,a1,a2)
#define DPRINT3(l,a1,a2,a3)
#define DPRINT4(l,a1,a2,a3,a4)
#define DPRINT5(l,a1,a2,a3,a4,a5)
#define DPRINT6(l,a1,a2,a3,a4,a5,a6)
#define DPRINT7(l,a1,a2,a3,a4,a5,a6,a7)
#endif


	/* These are termios bits as the FEP understands them */

/* c_cflag bits */
#define FEP_CBAUD	0x00000f
#define  FEP_B0		0x000000		/* hang up */
#define  FEP_B50	0x000001
#define  FEP_B75	0x000002
#define  FEP_B110	0x000003
#define  FEP_B134	0x000004
#define  FEP_B150	0x000005
#define  FEP_B200	0x000006
#define  FEP_B300	0x000007
#define  FEP_B600	0x000008
#define  FEP_B1200	0x000009
#define  FEP_B1800	0x00000a
#define  FEP_B2400	0x00000b
#define  FEP_B4800	0x00000c
#define  FEP_B9600	0x00000d
#define  FEP_B19200	0x00000e
#define  FEP_B38400	0x00000f
#define FEP_EXTA FEP_B19200
#define FEP_EXTB FEP_B38400
#define FEP_CSIZE	0x000030
#define   FEP_CS5	0x000000
#define   FEP_CS6	0x000010
#define   FEP_CS7	0x000020
#define   FEP_CS8	0x000030
#define FEP_CSTOPB	0x000040
#define FEP_CREAD	0x000080
#define FEP_PARENB	0x000100
#define FEP_PARODD	0x000200
#define FEP_CLOCAL	0x000800
#define FEP_FASTBAUD	0x000400
/* c_iflag bits */
#define FEP_IGNBRK	0000001
#define FEP_BRKINT	0000002
#define FEP_IGNPAR	0000004
#define FEP_PARMRK	0000010
#define FEP_INPCK	0000020
#define FEP_ISTRIP	0000040
#define FEP_IXON	0002000
#define FEP_IXANY	0004000
#define FEP_IXOFF	0010000
