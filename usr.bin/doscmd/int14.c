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
 *
 * $FreeBSD$
 */

#include "doscmd.h"
#include <sys/ioctl.h>
#include <termios.h>
#include "com.h"

struct com_data_struct com_data[N_COMS_MAX];

struct queue *create_queue() { return(0); }
int get_char_q() {}
int queue_not_empty() {}
int reset_irq_request() {}
int set_irq_request() {}
int test_irq_request() {}
int write_div_latches() {}

void
int14(regcontext_t *REGS)
{
    int reg_num;
    struct com_data_struct *cdsp;
    int i;
    int nbytes;
    char c;

    debug (D_PORT, "int14: dl = 0x%02X, al = 0x%02X.\n", R_DL, R_AL);
    if (R_DL >= N_COMS_MAX) {
	if (vflag)
	    dump_regs(REGS);
	fatal ("int14: illegal com port COM%d", R_DL + 1);
    }
    cdsp = &(com_data[R_DL]);

    switch (R_AH) {
    case 0x00:	/* Initialize Serial Port */
#if	0	/* hold off: try to defeat stupid DOS defaults */
	com_set_line(cdsp, R_DL + 1, R_AL);
	R_AH = LS_X_SHFT_E | LS_X_HOLD_E;
	R_AL = 0;
#endif	0
	break;

    case 0x01:	/* Write Character */
	errno = 0;
	c = R_AL;
	nbytes = write(cdsp->fd, &c, 1);
	debug (D_PORT, "write of 0x%02x to fd %d on '%s' returned %d %s\n",
		R_AL, cdsp->fd, cdsp->path, nbytes, strerror(errno));
	if (nbytes == 1) {
		R_AH = LS_X_SHFT_E | LS_X_HOLD_E;
		R_AL = 0;
	} else {
		debug(D_PORT, "int14: lost output character 0x%02x\n",
			R_AL);
		R_AH = LS_SW_TIME_OUT;
		R_AL = 0;
	}
	break;

    case 0x02:	/* Read Character */
	errno = 0;
	nbytes = read(cdsp->fd, &c, 1);
	debug (D_PORT, "read of fd %d on '%s' returned %d byte 0x%02x %s\n",
		    cdsp->fd, cdsp->path, nbytes, c,
		    errno ? strerror(errno) : "");
	if (nbytes == 1) {
		R_AH = LS_X_SHFT_E | LS_X_HOLD_E;
		R_AL = c;
	} else {
		R_AH = LS_SW_TIME_OUT;
		R_AL = 0x60;
	}
	break;

    case 0x03:	/* Status Request */
	R_AX = (LS_X_SHFT_E | LS_X_HOLD_E) << 8;
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
void
com_set_line(struct com_data_struct *cdsp, unsigned char port, unsigned char param)
{
    struct termios tty;
    struct stat stat_buf;
    int mode = 0;		/* read|write */
    int speed;
    int reg_num;
    int ret_val;

    debug (D_PORT, "com_set_line: cdsp = 0x%08X, port = 0x%04x,"
		   "param = 0x%04X.\n", cdsp, port, param);
    if (cdsp->fd > 0) {
	    debug (D_PORT, "Re-initialize serial port com%d\n", port);
	    (void)close(cdsp->fd);
    } else {
	    debug (D_PORT, "Initialize serial port com%d\n", port);
    }

    stat(cdsp->path, &stat_buf);
    if (!S_ISCHR(stat_buf.st_mode) ||
	((cdsp->fd = open(cdsp->path, O_RDWR | O_NONBLOCK, 0666)) == -1)) {

	debug (D_PORT,
		"Could not initialize serial port com%d on path '%s'\n",
			port, cdsp->path);
	return;
    }

    cdsp->flags = 0x00;
    cdsp->last_char_read = 0x00;
#if 0
    if ((param & PARITY_EVEN) == PARITY_NONE)
	    tty.c_iflag = IGNBRK | IGNPAR | IXON | IXOFF /* | IXANY */;
    else
	    tty.c_iflag = IGNBRK | IXON | IXOFF /* | IXANY */;
    tty.c_oflag = 0;           
    tty.c_lflag = 0;
    tty.c_cc[VTIME] = 0; 
    tty.c_cc[VMIN] = 1;
    tty.c_cflag = CREAD | CLOCAL | HUPCL;
    /* MCL WHY CLOCAL ??????; but, gets errno EIO on writes, else */
    if ((param & TXLEN_8BITS) == TXLEN_8BITS)
	    tty.c_cflag |= CS8;
    else
	    tty.c_cflag |= CS7;
    if ((param & STOPBIT_2) == STOPBIT_2)
	    tty.c_cflag |= CSTOPB;
    switch (param & PARITY_EVEN) {
	    case (PARITY_ODD):
		    tty.c_cflag |= (PARENB | PARODD);
		    break;
	    case (PARITY_EVEN):
		    tty.c_cflag |= PARENB;
		    break;
	    case (PARITY_NONE):
	    default:
		    break;
    }
    switch (param & BITRATE_9600) {
    case (BITRATE_110):
	speed = B110;
	break;
    case (BITRATE_150):
	speed = B150;
	break;
    case (BITRATE_300):
	speed = B300;
	break;
    case (BITRATE_600):
	speed = B600;
	break;
    case (BITRATE_1200):
	speed = B1200;
	break;
    case (BITRATE_2400):
	speed = B2400;
	break;
    case (BITRATE_4800):
	speed = B4800;
	break;
    case (BITRATE_9600):
	speed = B9600;
	break;
    }
    debug (D_PORT, "com_set_line: going with cflag 0x%X iflag 0x%X speed %d.\n",
    tty.c_cflag, tty.c_iflag, speed);
    errno = 0;
    ret_val = cfsetispeed(&tty, speed);
    debug (D_PORT, "com_set_line: cfsetispeed returned 0x%X.\n", ret_val);
    errno = 0;
    ret_val = cfsetospeed(&tty, speed);
    debug (D_PORT, "com_set_line: cfsetospeed returned 0x%X.\n", ret_val);
    errno = 0;
    ret_val = tcsetattr(cdsp->fd, 0, &tty);
    debug (D_PORT, "com_set_line: tcsetattr returned 0x%X.\n", ret_val);

    errno = 0;
    ret_val = fcntl(cdsp->fd, F_SETFL, O_NDELAY);
    debug (D_PORT, "fcntl of 0x%X, 0x%X to fd %d returned %d errno %d\n",
    F_SETFL, O_NDELAY, cdsp->fd, ret_val, errno);
    errno = 0;
    ret_val = ioctl(cdsp->fd, TIOCFLUSH, &mode);
    debug (D_PORT, "ioctl of 0x%02x to fd %d on 0x%X returned %d errno %d\n",
    TIOCFLUSH, cdsp->fd, mode, ret_val, errno);
#endif
    for (reg_num = 0; reg_num < N_OF_COM_REGS; reg_num++) {
	define_input_port_handler(cdsp->addr + reg_num, 
				  com_port_in);
	define_output_port_handler(cdsp->addr + reg_num,
				   com_port_out);
    }
    cdsp->com_queue = create_queue(cdsp->irq);
    debug(D_PORT, "com%d: attached '%s' at addr 0x%04x irq %d\n",
	    port, cdsp->path, cdsp->addr, cdsp->irq);
}


/* called when config.c initializes a single line */
void
init_com(int port, char *path, int addr, unsigned char irq)
{
    struct com_data_struct *cdsp;
	
    debug (D_PORT, "init_com: port = 0x%04x, addr = 0x%04X, irq = %d.\n",
	   port, addr, irq);
    cdsp = &(com_data[port]);
    cdsp->path = path;	/* XXX DEBUG strcpy? */
    cdsp->addr = addr;
    cdsp->irq = irq;
    cdsp->fd = -1;
    com_set_line(cdsp, port + 1, TXLEN_8BITS | BITRATE_9600);
}


/* called when DOS wants to read directly from a physical port */
unsigned char
com_port_in(int port)
{
    struct com_data_struct *cdsp;
    unsigned char rs;
    unsigned char i;
    int nbytes;

    /* search for a valid COM ???or MOUSE??? port */
    for (i = 0; i < N_COMS_MAX; i++) {
	if (com_data[i].addr == ((unsigned short)port & 0xfff8)) {
	    cdsp = &(com_data[i]);
	    break;
	}
    }
    if (i == N_COMS_MAX) {
	debug (D_PORT, "com port 0x%04x not found\n", port);
	return 0xff;
    }

    switch (port - cdsp->addr) {
	/* 0x03F8 - (receive buffer) or (divisor latch LO) */
    case 0:
	if (cdsp->line_ctrl & LC_DIV_ACC)
	    rs = cdsp->div_latch[DIV_LATCH_LOW];
	else {
#if	0
	    if (queue_not_empty(cdsp->com_queue)) {
		rs = get_char_q(cdsp->com_queue);
		cdsp->last_char_read = rs;
		if (queue_not_empty(cdsp->com_queue) &&
		    (cdsp->int_enable & IE_RCV_DATA) != 0) {
		    debug(D_PORT,
			  "com_port_in: setting irq %d because bytes yet to be read.\n",
			  cdsp->irq);
		    set_irq_request(cdsp->irq);
		}
	    } else
#else
		errno = 0;
	    nbytes = read(cdsp->fd, &rs, 1);
	    debug (D_PORT, "read of fd %d on '%s' returned %d byte 0x%02x errno %d\n",
		   cdsp->fd, cdsp->path, nbytes, rs, errno);
	    if (nbytes != 1)
#endif
		rs = cdsp->last_char_read;
	}
	break;

	/* 0x03F9 - (interrupt enable) or (divisor latch HI) */
    case 1:    
	if (cdsp->line_ctrl & LC_DIV_ACC)
	    rs = cdsp->div_latch[DIV_LATCH_HIGH];
	else
	    rs = cdsp->int_enable;

	/* 0x03FA - interrupt identification register */
    case 2:
	/* rs = cdsp->int_id;	* XXX DEBUG not initialized */
	rs = 0;
	if ((queue_not_empty(cdsp->com_queue))
	    && (test_irq_request(cdsp->irq) != 0))
	    rs |= II_PEND_INT | II_RCV_DATA;
	if ((cdsp->fifo_ctrl & FC_FIFO_EN) == FC_FIFO_EN)
	    rs |= II_FIFOS_EN;
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
	rs = LS_X_SHFT_E | LS_X_HOLD_E;
	/* if (queue_not_empty(cdsp->com_queue)) */
	ioctl(cdsp->fd, FIONREAD, &nbytes);
	if (nbytes > 0);
	rs |= LS_RCV_DATA_RD;
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
    int nbytes;
    int i;

    /* search for a valid COM ???or MOUSE??? port */
    for (i = 0; i < N_COMS_MAX; i++) {
	if (com_data[i].addr == ((unsigned short)port & 0xfff8)) {
	    cdsp = &(com_data[i]);
	    break;
	}
    }
    if (i == N_COMS_MAX) {
	debug (D_PORT, "com port 0x%04x not found\n", port);
	return;
    }

    switch (port - cdsp->addr) {
	/* 0x03F8 - (transmit buffer) or (divisor latch LO) */
    case 0:
	if (cdsp->line_ctrl & LC_DIV_ACC) {
	    cdsp->div_latch[DIV_LATCH_LOW] = val;
	    cdsp->flags |= DIV_LATCH_LOW_WRITTEN;
	    write_div_latches(cdsp);
	} else {
	    errno = 0;
	    nbytes = write(cdsp->fd, val, 1);
	    debug (D_PORT, "write of 0x%02x to fd %d on '%s' returned %d errno %d\n",
		   val, cdsp->fd, cdsp->path, nbytes, errno);
	    if (nbytes != 1)
		debug(D_PORT,
		      "int14: lost output character 0x%02x\n",
		      val);
	}
	break;

	/* 0x03F9 - (interrupt enable) or (divisor latch HI) */
    case 1:
	if (cdsp->line_ctrl & LC_DIV_ACC) {
	    cdsp->div_latch[DIV_LATCH_HIGH] = val;
	    cdsp->flags |= DIV_LATCH_HIGH_WRITTEN;
	    write_div_latches(cdsp);
	} else {
	    cdsp->int_enable = val;
	    if ((val & IE_RCV_DATA) == 0) {
		reset_irq_request(cdsp->irq);
	    } else {
		if (queue_not_empty(cdsp->com_queue)) {
		    set_irq_request(cdsp->irq);
		}
	    }
	}
	break;
	
	/* 0x03FA - FIFO control register */
    case 2:
	cdsp->fifo_ctrl = val;
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
	cdsp->line_stat = val;
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

#if	0
/*
 * called when BSD has bytes ready (as discovered via select) for DOS
 */
static void do_com_input(int fd)
{
    struct com_data_struct *cdsp;
    unsigned char buffer[BUFSIZE];
    int i, nbytes;
    
    dp = search_com_device_by_fd(fd);
    if (dp == NULL)
	return;
    do {
	nbytes = read(cdsp->fd, buffer, BUFSIZE);
	if (nbytes > 0) {
	    debug(D_PORT, "do_com_input: read %d bytes from fd %d (aka %d): ",
		  nbytes, fd, cdsp->fd);
	    for (i = 0; i < nbytes; i++) {
		put_char_q(cdsp->com_queue, buffer[i]);
		if (cdsp->int_enable & IE_RCV_DATA) {
		    debug(D_PORT, "\n");
		    debug(D_PORT, "do_com_input: setting irq %d because %d bytes read.\n",
			  dp->irq, nbytes);
		    debug(D_PORT, "do_com_input: ");
		    set_irq_request(dp->irq);
		}
		debug(D_PORT, "%02x ", buffer[i]);
	    }
	    debug(D_PORT, "\n");
	}
    } while (nbytes == BUFSIZE);
}
#endif	0
