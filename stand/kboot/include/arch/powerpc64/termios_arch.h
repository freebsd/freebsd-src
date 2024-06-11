/*
 * Copyright (c) 2005-2020 Rich Felker, et al.
 *
 * SPDX-License-Identifier: MIT
 *
 * Note: From the musl project, stripped down and repackaged with HOST_/host_ prepended
 */

#undef HOST_NCCS
#define HOST_NCCS 19
struct host_termios {
	host_tcflag_t c_iflag;
	host_tcflag_t c_oflag;
	host_tcflag_t c_cflag;
	host_tcflag_t c_lflag;
	host_cc_t c_cc[HOST_NCCS];
	host_cc_t c_line;
	host_speed_t __c_ispeed;
	host_speed_t __c_ospeed;
};

#define HOST_VINTR     0
#define HOST_VQUIT     1
#define HOST_VERASE    2
#define HOST_VKILL     3
#define HOST_VEOF      4
#define HOST_VMIN      5
#define HOST_VEOL      6
#define HOST_VTIME     7
#define HOST_VEOL2     8
#define HOST_VSWTC     9
#define HOST_VWERASE  10
#define HOST_VREPRINT 11
#define HOST_VSUSP    12
#define HOST_VSTART   13
#define HOST_VSTOP    14
#define HOST_VLNEXT   15
#define HOST_VDISCARD 16

#define HOST_IGNBRK  0000001
#define HOST_BRKINT  0000002
#define HOST_IGNPAR  0000004
#define HOST_PARMRK  0000010
#define HOST_INPCK   0000020
#define HOST_ISTRIP  0000040
#define HOST_INLCR   0000100
#define HOST_IGNCR   0000200
#define HOST_ICRNL   0000400
#define HOST_IXON    0001000
#define HOST_IXOFF   0002000
#define HOST_IXANY   0004000
#define HOST_IUCLC   0010000
#define HOST_IMAXBEL 0020000
#define HOST_IUTF8   0040000

#define HOST_OPOST  0000001
#define HOST_ONLCR  0000002
#define HOST_OLCUC  0000004
#define HOST_OCRNL  0000010
#define HOST_ONOCR  0000020
#define HOST_ONLRET 0000040
#define HOST_OFILL  0000100
#define HOST_OFDEL  0000200
#if defined(_GNU_SOURCE) || defined(_BSD_SOURCE) || defined(_XOPEN_SOURCE)
#define HOST_NLDLY  0001400
#define HOST_NL0    0000000
#define HOST_NL1    0000400
#define HOST_NL2    0001000
#define HOST_NL3    0001400
#define HOST_TABDLY 0006000
#define HOST_TAB0   0000000
#define HOST_TAB1   0002000
#define HOST_TAB2   0004000
#define HOST_TAB3   0006000
#define HOST_CRDLY  0030000
#define HOST_CR0    0000000
#define HOST_CR1    0010000
#define HOST_CR2    0020000
#define HOST_CR3    0030000
#define HOST_FFDLY  0040000
#define HOST_FF0    0000000
#define HOST_FF1    0040000
#define HOST_BSDLY  0100000
#define HOST_BS0    0000000
#define HOST_BS1    0100000
#endif

#define HOST_VTDLY  0200000
#define HOST_VT0    0000000
#define HOST_VT1    0200000

#define HOST_B0       0000000
#define HOST_B50      0000001
#define HOST_B75      0000002
#define HOST_B110     0000003
#define HOST_B134     0000004
#define HOST_B150     0000005
#define HOST_B200     0000006
#define HOST_B300     0000007
#define HOST_B600     0000010
#define HOST_B1200    0000011
#define HOST_B1800    0000012
#define HOST_B2400    0000013
#define HOST_B4800    0000014
#define HOST_B9600    0000015
#define HOST_B19200   0000016
#define HOST_B38400   0000017

#define HOST_B57600   00020
#define HOST_B115200  00021
#define HOST_B230400  00022
#define HOST_B460800  00023
#define HOST_B500000  00024
#define HOST_B576000  00025
#define HOST_B921600  00026
#define HOST_B1000000 00027
#define HOST_B1152000 00030
#define HOST_B1500000 00031
#define HOST_B2000000 00032
#define HOST_B2500000 00033
#define HOST_B3000000 00034
#define HOST_B3500000 00035
#define HOST_B4000000 00036

#define HOST_CSIZE  00001400
#define HOST_CS5    00000000
#define HOST_CS6    00000400
#define HOST_CS7    00001000
#define HOST_CS8    00001400
#define HOST_CSTOPB 00002000
#define HOST_CREAD  00004000
#define HOST_PARENB 00010000
#define HOST_PARODD 00020000
#define HOST_HUPCL  00040000
#define HOST_CLOCAL 00100000

#define HOST_ECHOE   0x00000002
#define HOST_ECHOK   0x00000004
#define HOST_ECHO    0x00000008
#define HOST_ECHONL  0x00000010
#define HOST_ISIG    0x00000080
#define HOST_ICANON  0x00000100
#define HOST_IEXTEN  0x00000400
#define HOST_TOSTOP  0x00400000
#define HOST_NOFLSH  0x80000000

#define HOST_TCOOFF 0
#define HOST_TCOON  1
#define HOST_TCIOFF 2
#define HOST_TCION  3

#define HOST_TCIFLUSH  0
#define HOST_TCOFLUSH  1
#define HOST_TCIOFLUSH 2

#define HOST_TCSANOW   0
#define HOST_TCSADRAIN 1
#define HOST_TCSAFLUSH 2

#define HOST_EXTA    0000016
#define HOST_EXTB    0000017
#define HOST_CBAUD   00377
#define HOST_CBAUDEX 0000020
#define HOST_CIBAUD  077600000
#define HOST_CMSPAR  010000000000
#define HOST_CRTSCTS 020000000000

#define HOST_XCASE   0x00004000
#define HOST_ECHOCTL 0x00000040
#define HOST_ECHOPRT 0x00000020
#define HOST_ECHOKE  0x00000001
#define HOST_FLUSHO  0x00800000
#define HOST_PENDIN  0x20000000
#define HOST_EXTPROC 0x10000000

#define HOST_XTABS   00006000
#define HOST_TIOCSER_TEMT 1

#define _IOC(a,b,c,d) ( ((a)<<29) | ((b)<<8) | (c) | ((d)<<16) )
#define _IOC_NONE  1U
#define _IOC_WRITE 4U
#define _IOC_READ  2U

#define _IO(a,b) _IOC(_IOC_NONE,(a),(b),0)
#define _IOW(a,b,c) _IOC(_IOC_WRITE,(a),(b),sizeof(c))
#define _IOR(a,b,c) _IOC(_IOC_READ,(a),(b),sizeof(c))
#define _IOWR(a,b,c) _IOC(_IOC_READ|_IOC_WRITE,(a),(b),sizeof(c))

#define HOST_TCGETS	_IOR('t', 19, char[44])
#define HOST_TCSETS	_IOW('t', 20, char[44])
#define HOST_TCSETSW	_IOW('t', 21, char[44])
#define HOST_TCSETSF	_IOW('t', 22, char[44])
