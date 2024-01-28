/*
 * Copyright (c) 2005-2020 Rich Felker, et al.
 *
 * SPDX-License-Identifier: MIT
 *
 * Note: From the musl project, stripped down and repackaged with HOST_/host_ prepended
 */

struct host_termios {
	host_tcflag_t c_iflag;
	host_tcflag_t c_oflag;
	host_tcflag_t c_cflag;
	host_tcflag_t c_lflag;
	host_cc_t c_line;
	host_cc_t c_cc[HOST_NCCS];
	host_speed_t __c_ispeed;
	host_speed_t __c_ospeed;
};

#define HOST_VINTR     0
#define HOST_VQUIT     1
#define HOST_VERASE    2
#define HOST_VKILL     3
#define HOST_VEOF      4
#define HOST_VTIME     5
#define HOST_VMIN      6
#define HOST_VSWTC     7
#define HOST_VSTART    8
#define HOST_VSTOP     9
#define HOST_VSUSP    10
#define HOST_VEOL     11
#define HOST_VREPRINT 12
#define HOST_VDISCARD 13
#define HOST_VWERASE  14
#define HOST_VLNEXT   15
#define HOST_VEOL2    16

#define HOST_IGNBRK  0000001
#define HOST_BRKINT  0000002
#define HOST_IGNPAR  0000004
#define HOST_PARMRK  0000010
#define HOST_INPCK   0000020
#define HOST_ISTRIP  0000040
#define HOST_INLCR   0000100
#define HOST_IGNCR   0000200
#define HOST_ICRNL   0000400
#define HOST_IUCLC   0001000
#define HOST_IXON    0002000
#define HOST_IXANY   0004000
#define HOST_IXOFF   0010000
#define HOST_IMAXBEL 0020000
#define HOST_IUTF8   0040000

#define HOST_OPOST  0000001
#define HOST_OLCUC  0000002
#define HOST_ONLCR  0000004
#define HOST_OCRNL  0000010
#define HOST_ONOCR  0000020
#define HOST_ONLRET 0000040
#define HOST_OFILL  0000100
#define HOST_OFDEL  0000200
#if defined(_GNU_SOURCE) || defined(_BSD_SOURCE) || defined(_XOPEN_SOURCE)
#define HOST_NLDLY  0000400
#define HOST_NL0    0000000
#define HOST_NL1    0000400
#define HOST_CRDLY  0003000
#define HOST_CR0    0000000
#define HOST_CR1    0001000
#define HOST_CR2    0002000
#define HOST_CR3    0003000
#define HOST_TABDLY 0014000
#define HOST_TAB0   0000000
#define HOST_TAB1   0004000
#define HOST_TAB2   0010000
#define HOST_TAB3   0014000
#define HOST_BSDLY  0020000
#define HOST_BS0    0000000
#define HOST_BS1    0020000
#define HOST_FFDLY  0100000
#define HOST_FF0    0000000
#define HOST_FF1    0100000
#endif

#define HOST_VTDLY  0040000
#define HOST_VT0    0000000
#define HOST_VT1    0040000

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

#define HOST_B57600   0010001
#define HOST_B115200  0010002
#define HOST_B230400  0010003
#define HOST_B460800  0010004
#define HOST_B500000  0010005
#define HOST_B576000  0010006
#define HOST_B921600  0010007
#define HOST_B1000000 0010010
#define HOST_B1152000 0010011
#define HOST_B1500000 0010012
#define HOST_B2000000 0010013
#define HOST_B2500000 0010014
#define HOST_B3000000 0010015
#define HOST_B3500000 0010016
#define HOST_B4000000 0010017

#define HOST_CSIZE  0000060
#define HOST_CS5    0000000
#define HOST_CS6    0000020
#define HOST_CS7    0000040
#define HOST_CS8    0000060
#define HOST_CSTOPB 0000100
#define HOST_CREAD  0000200
#define HOST_PARENB 0000400
#define HOST_PARODD 0001000
#define HOST_HUPCL  0002000
#define HOST_CLOCAL 0004000

#define HOST_ISIG   0000001
#define HOST_ICANON 0000002
#define HOST_ECHO   0000010
#define HOST_ECHOE  0000020
#define HOST_ECHOK  0000040
#define HOST_ECHONL 0000100
#define HOST_NOFLSH 0000200
#define HOST_TOSTOP 0000400
#define HOST_IEXTEN 0100000

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
#define HOST_CBAUD   0010017
#define HOST_CBAUDEX 0010000
#define HOST_CIBAUD  002003600000
#define HOST_CMSPAR  010000000000
#define HOST_CRTSCTS 020000000000

#define HOST_XCASE   0000004
#define HOST_ECHOCTL 0001000
#define HOST_ECHOPRT 0002000
#define HOST_ECHOKE  0004000
#define HOST_FLUSHO  0010000
#define HOST_PENDIN  0040000
#define HOST_EXTPROC 0200000

#define HOST_XTABS  0014000

#define HOST_TCGETS		0x5401
#define HOST_TCSETS		0x5402
#define HOST_TCSETSW		0x5403
#define HOST_TCSETSF		0x5404
