/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley Software
 * Design, Inc. by Mark Linoman.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI int14.c,v 2.2 1996/04/08 19:32:45 bostic Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>

#include "doscmd.h"
#include "AsyncIO.h"
#include "com.h"

#define N_BYTES	1024

struct com_data_struct {
	int		fd;		/* BSD/386 file descriptor */
	char		*path;		/* BSD/386 pathname */
	int		addr;		/* ISA I/O address */
	unsigned char	irq;		/* ISA IRQ */
	unsigned char	inbuf[N_BYTES];	/* input buffer */
	unsigned char	outbuf[N_BYTES];/* output buffer */
	int		ids;            /* input data size */
	int		ods;            /* output data size */
	int		emptyint;
	struct termios	tty;
	unsigned char	div_latch[2];	/* mirror of 16550 R0':R1'
					   read/write */
	unsigned char	int_enable;	/* mirror of 16550 R1 read/write */
	unsigned char	fifo_ctrl;	/* mirror of 16550 R2 write only */
	unsigned char	line_ctrl;	/* mirror of 16550 R3 read/write */
	unsigned char	modem_ctrl;	/* mirror of 16550 R4 read/write */
	unsigned char	modem_stat;	/* mirror of 16550 R6 read/write */
	unsigned char	uart_spare;	/* mirror of 16550 R7 read/write */
};

#define	DIV_LATCH_LOW	0
#define	DIV_LATCH_HIGH	1

struct com_data_struct com_data[N_COMS_MAX];

static unsigned char	com_port_in(int port);
static void		com_port_out(int port, unsigned char val);
static void		com_set_line(struct com_data_struct *cdsp,
				     unsigned char port, unsigned char param);
 
static void
manage_int(struct com_data_struct *cdsp)
{
 	if ((cdsp->int_enable & IE_RCV_DATA) && cdsp->ids > 0) {
 		hardint(cdsp->irq);
  		debug(D_PORT, "manage_int: hardint rd\n");
 		return;
 	}
 	if ((cdsp->int_enable & IE_TRANS_HLD) && cdsp->emptyint) {
 		hardint(cdsp->irq);
  		debug(D_PORT, "manage_int: hardint wr\n");
 		return;
 	}
 	unpend (cdsp->irq);
}
 
static int
has_enough_data(struct com_data_struct *cdsp)
{
 	switch (cdsp->fifo_ctrl & (FC_FIFO_EN | FC_FIFO_SZ_MASK)) {
	case FC_FIFO_EN | FC_FIFO_4B:
		return cdsp->ids >= 4;
	case FC_FIFO_EN | FC_FIFO_8B:
		return cdsp->ids >= 8;
	case FC_FIFO_EN | FC_FIFO_14B:
		return cdsp->ids >= 14;
 	}
 	return cdsp->ids;
}
 
static void
input(struct com_data_struct *cdsp, int force_read)
{
 	int nbytes;
 
 	if (cdsp->ids < N_BYTES && (force_read || !has_enough_data(cdsp))) {
 		nbytes = read(cdsp->fd, &cdsp->inbuf[cdsp->ids],
		    N_BYTES - cdsp->ids);
 		debug(D_PORT, "read of fd %d on '%s' returned %d (%s)\n",
		    cdsp->fd, cdsp->path, nbytes,
		    nbytes == -1 ? strerror(errno) : "");
 		if (nbytes != -1)
 			cdsp->ids += nbytes;
 	}
}
 
static void
output(struct com_data_struct *cdsp)
{
 	int nbytes;
 
 	if (cdsp->ods > 0) {
 		nbytes = write(cdsp->fd, &cdsp->outbuf[0], cdsp->ods);
 		debug(D_PORT, "write of fd %d on '%s' returned %d (%s)\n",
		    cdsp->fd, cdsp->path, nbytes,
		    nbytes == -1 ? strerror(errno) : "");
 		if (nbytes != -1) {
 			cdsp->ods -= nbytes;
 			memmove (&cdsp->outbuf[0],
			    &cdsp->outbuf[nbytes], cdsp->ods);
 			if ((cdsp->int_enable & IE_TRANS_HLD)
			    && cdsp->ods == 0)
 				cdsp->emptyint = 1;
 		}
 	}
}
 
static void
flush_out(void* arg)
{
 	struct com_data_struct *cdsp = (struct com_data_struct*)arg;
 	output(cdsp);
 	manage_int(cdsp);
}
 
/*
 *  We postponed flush till the end of interrupt processing
 *   (see int.c).
 */
static int
write_char(struct com_data_struct *cdsp, char c)
{
 	int r = 0;
 	cdsp->emptyint = 0;
 	if (cdsp->ods >= N_BYTES)
 		output(cdsp);
 	if (cdsp->ods < N_BYTES) {
 		cdsp->outbuf[cdsp->ods ++] = c;
 		if (!isinhardint(cdsp->irq))
 			output(cdsp);
 		r = 1;
 	}
 	manage_int(cdsp);
 	return r;
}
 
static int
read_char(struct com_data_struct *cdsp)
{
 	int c = -1;
 
 	input(cdsp, 0);
 
 	if (cdsp->ids > 0) {
 		c = cdsp->inbuf[0];
 		cdsp->ids --;
 		memmove(&cdsp->inbuf[0], &cdsp->inbuf[1], cdsp->ids);
 	}
 
 	manage_int(cdsp);
 
 	debug(D_PORT, "read_char: %x\n", c);
 	return c;
}
 
static void
new_ii(struct com_data_struct *cdsp)
{
 	if ((cdsp->int_enable & IE_TRANS_HLD) && cdsp->ods == 0)
 		cdsp->emptyint = 1;
 	manage_int(cdsp);
}
 
static unsigned char
get_status(struct com_data_struct *cdsp)
{
 	unsigned char s = (LS_X_DATA_E | LS_X_HOLD_E);
 	if (cdsp->ids > 0)
 		s |= LS_RCV_DATA_RD;
 	if (cdsp->ods > 0) {
 		s &= ~LS_X_DATA_E;
 		if (cdsp->ods >= N_BYTES)
 			s &= ~LS_X_HOLD_E;
 	}
 	debug(D_PORT, "get_status: %x\n", (unsigned)s);
 	return s;
}
 
static unsigned char
get_int_id(struct com_data_struct *cdsp)
{
 	unsigned char s = II_PEND_INT;
 	if (cdsp->fifo_ctrl & FC_FIFO_EN)
 		s |= II_FIFOS_EN;
 	if ((cdsp->int_enable & IE_RCV_DATA) && cdsp->ids > 0) {
 		if (has_enough_data(cdsp))
 			s = (s & ~II_PEND_INT) | II_RCV_DATA;
 		else
 			s = (s & ~II_PEND_INT) | II_TO;
 	} else
 	if ((cdsp->int_enable & IE_TRANS_HLD) && cdsp->emptyint) {
 		cdsp->emptyint = 0;
 		s = (s & ~II_PEND_INT) | II_TRANS_HLD;
 	}
 	debug(D_PORT, "get_int_id: %x\n", (unsigned)s);
 	return s;
}
 
static void
com_async(int fd, int cond, void *arg, regcontext_t *REGS)
{
 	struct com_data_struct *cdsp = (struct com_data_struct*) arg;
 
 	debug(D_PORT, "com_async: %X.\n", cond);
 
 	if (cond & AS_RD)
 		input(cdsp, 1);
 	if (cond & AS_WR)
 		output(cdsp);
 	manage_int(cdsp);
}

void
int14(regcontext_t *REGS)
{
    struct com_data_struct *cdsp;
    int i;

    debug(D_PORT, "int14: dl = 0x%02X, al = 0x%02X.\n", R_DL, R_AL);
    if (R_DL >= N_COMS_MAX) {
	if (vflag)
	    dump_regs(REGS);
	fatal ("int14: illegal com port COM%d", R_DL + 1);
    }
    cdsp = &(com_data[R_DL]);

    switch (R_AH) {
    case 0x00:	/* Initialize Serial Port */
	com_set_line(cdsp, R_DL + 1, R_AL);
	R_AH = get_status(cdsp);
	R_AL = 0;
	break;

    case 0x01:	/* Write Character */
    	if (write_char(cdsp, R_AL)) {
		R_AH = get_status(cdsp);
		R_AL = 0;
	} else {
		debug(D_PORT, "int14: lost output character 0x%02x\n", R_AL);
		R_AH = LS_SW_TIME_OUT;
		R_AL = 0;
	}
	break;

    case 0x02:	/* Read Character */
	i = read_char(cdsp);
	if (i != -1) {
		R_AH = get_status(cdsp);
		R_AL = (char)i;
	} else {
		R_AH = LS_SW_TIME_OUT;
		R_AL = 0x60;
	}
	break;

    case 0x03:	/* Status Request */
	R_AH = get_status(cdsp);
	R_AL = 0;
	break;

    case 0x04:	/* Extended Initialization */
	R_AX = (LS_SW_TIME_OUT) << 8;
	break;

    case 0x05:	/* Modem Control Register operations */
	switch (R_AH) {
	case 0x00:	/* Read Modem Control Register */
		R_AX = (LS_SW_TIME_OUT) << 8;
		break;

	case 0x01:	/* Write Modem Control Register */
		R_AX = (LS_SW_TIME_OUT) << 8;
		break;

	default:
		unknown_int3(0x14, 0x05, R_AL, REGS);
		break;
	}
	break;
    default:
	unknown_int2(0x14, R_AH, REGS);
	break;
    }
}


/* called when doscmd initializes a single line */
static void
com_set_line(struct com_data_struct *cdsp, unsigned char port, unsigned char param)
{
    struct stat stat_buf;
    int mode = 0;		/* read | write */
    int reg_num, ret_val, spd, speed;
    u_int8_t div_hi, div_lo;
    
    debug(D_PORT, "com_set_line: cdsp = %8p, port = 0x%04x,"
		   "param = 0x%04X.\n", cdsp, port, param);
    if (cdsp->fd > 0) {
	debug(D_PORT, "Re-initialize serial port com%d\n", port);
	_RegisterIO(cdsp->fd, 0, 0, 0);
	(void)close(cdsp->fd);
    } else {
	debug(D_PORT, "Initialize serial port com%d\n", port);
    }
    
    stat(cdsp->path, &stat_buf);
    if (!S_ISCHR(stat_buf.st_mode) ||
	((cdsp->fd = open(cdsp->path, O_RDWR | O_NONBLOCK, 0666)) == -1)) {
	
	debug(D_PORT,
	      "Could not initialize serial port com%d on path '%s'\n",
	      port, cdsp->path);
	return;
    }
    
    cdsp->ids = cdsp->ods = cdsp->emptyint = 0;
    cdsp->int_enable = 0;
    cdsp->fifo_ctrl = 0;
    cdsp->modem_ctrl = 0;
    cdsp->modem_stat = 0;
    cdsp->uart_spare = 0;
    
    if ((param & PARITY_EVEN) == PARITY_NONE)
	cdsp->tty.c_iflag = IGNBRK | IGNPAR /* | IXON | IXOFF | IXANY */;
    else
	cdsp->tty.c_iflag = IGNBRK /* | IXON | IXOFF | IXANY */;
    cdsp->tty.c_oflag = 0;
    cdsp->tty.c_lflag = 0;
    cdsp->tty.c_cc[VTIME] = 0; 
    cdsp->tty.c_cc[VMIN] = 0;
    cdsp->tty.c_cflag = CREAD | CLOCAL | HUPCL;
    /* MCL WHY CLOCAL ??????; but, gets errno EIO on writes, else */
    if ((param & TXLEN_8BITS) == TXLEN_8BITS) {
	cdsp->tty.c_cflag |= CS8;
	cdsp->line_ctrl |= 3;
    } else {
	cdsp->tty.c_cflag |= CS7;
	cdsp->line_ctrl |= 2;
    }
    if ((param & STOPBIT_2) == STOPBIT_2) {
	cdsp->tty.c_cflag |= CSTOPB;
	cdsp->line_ctrl |= LC_STOP_B;
    } else {
	cdsp->tty.c_cflag &= ~CSTOPB;
	cdsp->line_ctrl &= ~LC_STOP_B;
    }    
    switch (param & PARITY_EVEN) {
    case PARITY_ODD:
	cdsp->tty.c_cflag |= (PARENB | PARODD);
	cdsp->line_ctrl &= ~LC_EVEN_P;
	cdsp->line_ctrl |= LC_PAR_E;
	break;
    case PARITY_EVEN:
	cdsp->tty.c_cflag |= PARENB;
	cdsp->line_ctrl |= LC_EVEN_P | LC_PAR_E;
	break;
    case PARITY_NONE:
	cdsp->line_ctrl &= ~LC_PAR_E;
    default:
	break;
    }
    switch (param & BITRATE_9600) {
    case BITRATE_110:
	speed = B110;
	spd = 110;
	break;
    case BITRATE_150:
	speed = B150;
	spd = 150;
	break;
    case BITRATE_300:
	speed = B300;
	spd = 300;
	break;
    case BITRATE_600:
	speed = B600;
	spd = 600;
	break;
    case BITRATE_1200:
	speed = B1200;
	spd = 1200;
	break;
    case BITRATE_2400:
	speed = B2400;
	spd = 2400;
	break;
    case BITRATE_4800:
	speed = B4800;
	spd = 4800;
	break;
    case BITRATE_9600:
	speed = B9600;
	spd = 9600;
	break;
    }
    debug(D_PORT,
	"com_set_line: going with cflag 0x%X iflag 0x%X speed %d.\n",
	cdsp->tty.c_cflag, cdsp->tty.c_iflag, speed);
    div_lo = (115200 / spd) & 0x00ff;
    div_hi = (115200 / spd) & 0xff00;
    cdsp->div_latch[DIV_LATCH_LOW] = div_lo;
    cdsp->div_latch[DIV_LATCH_HIGH] = div_hi;
    errno = 0;
    ret_val = cfsetispeed(&cdsp->tty, speed);
    debug(D_PORT, "com_set_line: cfsetispeed returned 0x%X.\n", ret_val);
    errno = 0;
    ret_val = cfsetospeed(&cdsp->tty, speed);
    debug(D_PORT, "com_set_line: cfsetospeed returned 0x%X.\n", ret_val);
    errno = 0;
    ret_val = tcsetattr(cdsp->fd, 0, &cdsp->tty);
    debug(D_PORT, "com_set_line: tcsetattr returned 0x%X (%s).\n",
	ret_val, ret_val == -1 ? strerror(errno) : "");
    errno = 0;
    ret_val = fcntl(cdsp->fd, F_SETFL, O_NDELAY);
    debug(D_PORT, "fcntl of 0x%X, 0x%X to fd %d returned %d errno %d\n",
	F_SETFL, O_NDELAY, cdsp->fd, ret_val, errno);
    errno = 0;
    ret_val = ioctl(cdsp->fd, TIOCFLUSH, &mode);
    debug(D_PORT, "ioctl of 0x%02lx to fd %d on 0x%X returned %d errno %d\n",
	TIOCFLUSH, cdsp->fd, mode, ret_val, errno);
    for (reg_num = 0; reg_num < N_OF_COM_REGS; reg_num++) {
	define_input_port_handler(cdsp->addr + reg_num, com_port_in);
	define_output_port_handler(cdsp->addr + reg_num, com_port_out);
    }
    debug(D_PORT, "com%d: attached '%s' at addr 0x%04x irq %d\n",
	port, cdsp->path, cdsp->addr, cdsp->irq);

    set_eoir(cdsp->irq, flush_out, cdsp);
    _RegisterIO(cdsp->fd, com_async, cdsp, 0);
}

static void
try_set_speed(struct com_data_struct *cdsp)
{
    unsigned divisor, speed;
    int ret_val;
 
    divisor = (unsigned) cdsp->div_latch[DIV_LATCH_HIGH] << 8
	| (unsigned) cdsp->div_latch[DIV_LATCH_LOW];
 
    debug(D_PORT, "try_set_speed: divisor = %u\n", divisor);
 
    if (divisor == 0)
	return;
 
    speed = 115200 / divisor;
    if (speed == 0)
	return;
 
    errno = 0;
    ret_val = cfsetispeed(&cdsp->tty, speed);
    debug(D_PORT, "try_set_speed: cfsetispeed returned 0x%X.\n", ret_val);
 
    errno = 0;
    ret_val = cfsetospeed(&cdsp->tty, speed);
    debug(D_PORT, "try_set_speed: cfsetospeed returned 0x%X.\n", ret_val);
 
    errno = 0;
    ret_val = tcsetattr(cdsp->fd, 0, &cdsp->tty);
    debug(D_PORT, "try_set_speed: tcsetattr returned 0x%X (%s).\n", ret_val,
	  ret_val == -1 ? strerror (errno) : "");
}

/* called when config.c initializes a single line */
void
init_com(int port, char *path, int addr, unsigned char irq)
{
    struct com_data_struct *cdsp;
	
    debug(D_PORT, "init_com: port = 0x%04x, addr = 0x%04X, irq = %d.\n",
	  port, addr, irq);
    cdsp = &(com_data[port]);
    cdsp->path = path;	/* XXX DEBUG strcpy? */
    cdsp->addr = addr;
    cdsp->irq = irq;
    cdsp->fd = -1;
    com_set_line(cdsp, port + 1, TXLEN_8BITS | BITRATE_9600);

    /* update BIOS variables */
    nserial++;
    *(u_int16_t *)&BIOSDATA[0x00 + 2 * port] = (u_int16_t)addr;
}

/* called when DOS wants to read directly from a physical port */
unsigned char
com_port_in(int port)
{
    struct com_data_struct *cdsp;
    unsigned char rs;
    unsigned char i;
    int r;

    /* search for a valid COM ???or MOUSE??? port */
    for (i = 0; i < N_COMS_MAX; i++) {
	if (com_data[i].addr == ((unsigned short)port & 0xfff8)) {
	    cdsp = &(com_data[i]);
	    break;
	}
    }
    if (i == N_COMS_MAX) {
	debug(D_PORT, "com port 0x%04x not found\n", port);
	return 0xff;
    }

    switch (port - cdsp->addr) {
	/* 0x03F8 - (receive buffer) or (divisor latch LO) */
    case 0:
	if (cdsp->line_ctrl & LC_DIV_ACC)
	    rs = cdsp->div_latch[DIV_LATCH_LOW];
	else {
	    rs = 0x60;
	    r = read_char(cdsp);
	    if (r != -1)
		rs = (unsigned char)r;
	}
	break;

	/* 0x03F9 - (interrupt enable) or (divisor latch HI) */
    case 1:
	if (cdsp->line_ctrl & LC_DIV_ACC)
	    rs = cdsp->div_latch[DIV_LATCH_HIGH];
	else
	    rs = cdsp->int_enable;
	break;
	
	/* 0x03FA - interrupt identification register */
    case 2:
	rs = get_int_id(cdsp);
	break;

	/* 0x03FB - line control register */
    case 3:
	rs = cdsp->line_ctrl;
	break;

	/* 0x03FC - modem control register */
    case 4:
	rs = cdsp->modem_ctrl;
	break;

	/* 0x03FD - line status register */
    case 5:
	rs = get_status(cdsp);
	break;

	/* 0x03FE - modem status register */
    case 6:
	rs = cdsp->modem_stat | MS_DCD | MS_DSR | MS_CTS;
	break;

	/* 0x03FF - spare register */
    case 7:
	rs = cdsp->uart_spare;
	break;

    default:
	debug(D_PORT, "com_port_in: illegal port index 0x%04x - 0x%04x\n",
	      port, cdsp->addr);
	break;
    }
    return rs;
}

/* called when DOS wants to write directly to a physical port */
void
com_port_out(int port, unsigned char val)
{
    struct com_data_struct *cdsp;
    int i;

    /* search for a valid COM ???or MOUSE??? port */
    for (i = 0; i < N_COMS_MAX; i++) {
	if (com_data[i].addr == ((unsigned short)port & 0xfff8)) {
	    cdsp = &(com_data[i]);
	    break;
	}
    }
    if (i == N_COMS_MAX) {
	debug(D_PORT, "com port 0x%04x not found\n", port);
	return;
    }

    switch (port - cdsp->addr) {
	/* 0x03F8 - (transmit buffer) or (divisor latch LO) */
    case 0:
	if (cdsp->line_ctrl & LC_DIV_ACC) {
	    cdsp->div_latch[DIV_LATCH_LOW] = val;
	    try_set_speed(cdsp);
	} else {
	    write_char(cdsp, val);
	}
	break;

	/* 0x03F9 - (interrupt enable) or (divisor latch HI) */
    case 1:
	if (cdsp->line_ctrl & LC_DIV_ACC) {
	    cdsp->div_latch[DIV_LATCH_HIGH] = val;
	    try_set_speed(cdsp);
	} else {
	    cdsp->int_enable = val;
	    new_ii(cdsp);
	}
	break;
	
	/* 0x03FA - FIFO control register */
    case 2:
	cdsp->fifo_ctrl = val & (FC_FIFO_EN | FC_FIFO_SZ_MASK);
	if (val & FC_FIFO_CRV)
	    cdsp->ids = 0;
	if (val & FC_FIFO_CTR) {
	    cdsp->ods = 0;
	    cdsp->emptyint = 1;
	}
	input(cdsp, 1);
	manage_int(cdsp);
	break;
	
	/* 0x03FB - line control register */
    case 3:
	cdsp->line_ctrl = val;
	break;

	/* 0x03FC - modem control register */
    case 4:
	cdsp->modem_ctrl = val;
	break;

	/* 0x03FD - line status register */
    case 5:
	/* read-only */
	break;

	/* 0x03FE - modem status register */
    case 6:
	cdsp->modem_stat = val;
	break;

	/* 0x03FF - spare register */
    case 7:
	cdsp->uart_spare = val;
	break;

    default:
	debug(D_PORT, "com_port_out: illegal port index 0x%04x - 0x%04x\n",
	      port, cdsp->addr);
	break;
    }
}
