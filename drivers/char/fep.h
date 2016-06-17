
#define CSTART       0x400L
#define CMAX         0x800L
#define ISTART       0x800L
#define IMAX         0xC00L
#define CIN          0xD10L
#define GLOBAL       0xD10L
#define EIN          0xD18L
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
	volatile unchar *dev;
	
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
	
	volatile unchar num;
	volatile unchar ract;
	volatile unchar bstat;
	volatile unchar tbusy;
	volatile unchar iempty;
	volatile unchar ilow;
	volatile unchar idata;
	volatile unchar eflag;
	
	volatile unchar tflag;
	volatile unchar rflag;
	volatile unchar xmask;
	volatile unchar xval;
	volatile unchar mstat;
	volatile unchar mchange;
	volatile unchar mint;
	volatile unchar lstat;

	volatile unchar mtran;
	volatile unchar orun;
	volatile unchar startca;
	volatile unchar stopca;
	volatile unchar startc;
	volatile unchar stopc;
	volatile unchar vnext;
	volatile unchar hflow;

	volatile unchar fillc;
	volatile unchar ochar;
	volatile unchar omask;

	unchar filler7;
	unchar filler8[28];
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


#define RTS   0x02
#define CD    0x08
#define DSR   0x10
#define CTS   0x20
#define RI    0x40
#define DTR   0x80

	/* These are termios bits as the FEP understands them */

/* c_cflag bit meaning */
#define FEP_CBAUD	0000017
#define  FEP_B0		0000000		/* hang up */
#define  FEP_B50	0000001
#define  FEP_B75	0000002
#define  FEP_B110	0000003
#define  FEP_B134	0000004
#define  FEP_B150	0000005
#define  FEP_B200	0000006
#define  FEP_B300	0000007
#define  FEP_B600	0000010
#define  FEP_B1200	0000011
#define  FEP_B1800	0000012
#define  FEP_B2400	0000013
#define  FEP_B4800	0000014
#define  FEP_B9600	0000015
#define  FEP_B19200	0000016
#define  FEP_B38400	0000017
#define FEP_EXTA FEP_B19200
#define FEP_EXTB FEP_B38400
#define FEP_CSIZE	0000060
#define   FEP_CS5	0000000
#define   FEP_CS6	0000020
#define   FEP_CS7	0000040
#define   FEP_CS8	0000060
#define FEP_CSTOPB	0000100
#define FEP_CREAD	0000200
#define FEP_PARENB	0000400
#define FEP_PARODD	0001000
#define FEP_HUPCL	0002000
#define FEP_CLOCAL	0004000
#define FEP_CIBAUD	03600000		/* input baud rate (not used) */
#define FEP_CRTSCTS	  020000000000		/* flow control */

