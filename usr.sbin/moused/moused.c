/**
 ** Copyright (c) 1995 Michael Smith, All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer as
 **    the first lines of this file unmodified.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. All advertising materials mentioning features or use of this software
 **    must display the following acknowledgment:
 **      This product includes software developed by Michael Smith.
 ** 4. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 **
 ** THIS SOFTWARE IS PROVIDED BY Michael Smith ``AS IS'' AND ANY
 ** EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 ** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 ** PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Michael Smith BE LIABLE FOR
 ** ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 ** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 ** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 ** BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 ** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 ** OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 ** EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **
 **
 **      $Id$
 **/

/**
 ** MOUSED.C
 **
 ** Mouse daemon : listens to serial port for mouse data stream,
 ** interprets same and passes ioctls off to the console driver.
 **
 ** The mouse interface functions are derived closely from the mouse
 ** handler in the XFree86 X server.  Many thanks to the XFree86 people
 ** for their great work!
 ** 
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <machine/console.h>

#define debug(fmt,args...) \
	if (debug&&nodaemon) fprintf(stderr,"%s: " fmt "\n", progname, ##args)

char	*progname;
int	debug = 0;
int	nodaemon = 0;

void	usage(void);

#define	R_UNKNOWN	0
#define R_MICROSOFT	1
#define R_MOUSESYS	2
#define R_MMSERIES	3
#define R_LOGITECH	4
#define R_BUSMOUSE	5
#define R_LOGIMAN	6
#define R_PS_2		7

char	*rnames[] = {
    "xxx",
    "microsoft",
    "mousesystems",
    "mmseries",
    "logitech",
    "busmouse",
    "mouseman",
    "ps/2",
    NULL
};

unsigned short rodentcflags[] =
{
    0,							/* nomouse */
    (CS7	           | CREAD | CLOCAL | HUPCL ),	/* MicroSoft */
    (CS8 | CSTOPB	   | CREAD | CLOCAL | HUPCL ),	/* MouseSystems */
    (CS8 | PARENB | PARODD | CREAD | CLOCAL | HUPCL ),	/* MMSeries */
    (CS8 | CSTOPB	   | CREAD | CLOCAL | HUPCL ),	/* Logitech */
    0,							/* BusMouse */
    (CS7		   | CREAD | CLOCAL | HUPCL ),	/* MouseMan */
    0							/* PS/2 */
};

    
typedef struct
{
    int
	dx,dy,
	buttons;
} ACTIVITY;
    

struct rodentparam
{
    int
	baudrate,
	samplerate,
	flags,
	rtype,
	lastbuttons,
	buttons,
	mfd;

    char
	*portname;
    
} rodent = { baudrate : 1200, 
	     samplerate : 0, 
             flags : 0, 
             rtype : R_UNKNOWN,
	     lastbuttons : 0,
	     buttons : 0,
	     mfd : -1,
	     portname : NULL };

#define	ChordMiddle	1
	
void		r_init(void);
ACTIVITY	*r_protocol(u_char b);

void
main(int argc, char *argv[])
{
    int			c,i,cfd;
    u_char		b;
    ACTIVITY		*act;
    struct termios	t;
    struct mouse_info 	mouse;
    int			saved_buttons = 0;

    progname = argv[0];

    while((c = getopt(argc,argv,"cdfsp:t:h?")) != EOF)
	switch(c)
	{
	case 'c':
	    rodent.flags |= ChordMiddle;
	    break;

	case 'd':
	    debug = 1;
	    break;

	case 'f':
	    nodaemon = 1;
	    break;

	case 'p':
	    rodent.portname = optarg;
	    break;

	case 's':
	    rodent.baudrate = 9600;
	    usage();

	case 't':
	    for (i = 0; rnames[i]; i++)
		if (!strcmp(optarg,rnames[i]))
		{
		    debug("rodent is %s",rnames[i]);
		    rodent.rtype = i;
		    break;
		}
	    if (rnames[i])
		break;
	    fprintf(stderr,"no such mouse type `%s'\n",optarg);
	    usage();

	case 'h':
	case '?':
	default:
	    usage();
	}

    switch(rodent.rtype)
    {
    case R_BUSMOUSE:
	if (!rodent.portname)
	    rodent.portname = "/dev/mse0";
	break;
    case R_PS_2:
	if (!rodent.portname)
	    rodent.portname = "/dev/psm0";
	break;
    default:
	if (rodent.portname)
	    break;
	fprintf(stderr,"No port name specified\n");
	usage();
    }

    if ((rodent.mfd = open(rodent.portname, O_RDWR, 0)) == -1)
    {
	fprintf(stderr,"Can't open %s : %s\n",rodent.portname,strerror(errno));
	usage();
    }
    if ((rodent.rtype != R_PS_2) && (rodent.rtype != R_BUSMOUSE))
    {
	tcgetattr(rodent.mfd,&t);
	cfsetspeed(&t,rodent.baudrate);
	t.c_cflag = rodentcflags[rodent.rtype];
	if (tcsetattr(rodent.mfd,TCSAFLUSH,&t))
	{
	    fprintf(stderr,
		"%s: tcsetattr() failed : %s\n",progname,strerror(errno));
	    exit(1);
	}
    }   
    r_init();				/* call init function */

    if ((cfd = open("/dev/console", O_RDWR, 0)) == -1)
	fprintf(stderr, "error on /dev/console\n");

    if (!nodaemon)
	if (daemon(0,0))
	{
	    fprintf(stderr,
		"%s: daemon() failed : %s\n",progname,strerror(errno));
	    exit(1);
	}

    for(;;)
    {
	i = read(rodent.mfd,&b,1);	/* get a byte */
	if (i != 1)			/* read returned or error; goodbye */
	{
	    debug("read returned %d : %s exiting",i,strerror(errno));
	    close(rodent.mfd);
	    exit(1);
	}
	act = r_protocol(b);		/* pass byte to handler */
	if (act)			/* handler detected action */
	{
	    if (act->buttons != saved_buttons) {
		if (act->buttons == 0x04) {
			mouse.operation = MOUSE_CUT_START;
	    		ioctl(cfd, CONS_MOUSECTL, &mouse);
		}
		if (act->buttons == 0x00) {
			mouse.operation = MOUSE_CUT_END;
	    		ioctl(cfd, CONS_MOUSECTL, &mouse);
		}
		if (act->buttons == 0x01) {
			mouse.operation = MOUSE_RETURN_CUTBUFFER;
	    		ioctl(cfd, CONS_MOUSECTL, &mouse);
		}
		saved_buttons = act->buttons;
	    }
	    mouse.operation = MOUSE_MOVEREL;
	    mouse.x = act->dx;
	    mouse.y = act->dy;
	    ioctl(cfd, CONS_MOUSECTL, &mouse);
	    debug("Activity : buttons 0x%02x  dx %d  dy %d",
		    act->buttons,act->dx,act->dy);
	}
    }
}	    	


/**
 ** usage
 **
 ** Complain, and free the CPU for more worthy tasks
 **/
void
usage(void)
{
    fprintf(stderr,
	    " Usage is %s [options] -p <port> -t <mousetype>\n"
	    "  Options are   -s   Select 9600 baud mouse.\n"
	    "                -f   Don't become a daemon\n"
	    "                -d   Enable debugging messages\n"
	    "	             -c   Enable ChordMiddle option\n"
	    "  <mousetype> should be one of :\n"
	    "                microsoft\n"
	    "                mousesystems\n"
	    "                mmseries\n"
	    "                logitech\n"
	    "                busmouse\n"
	    "                mouseman\n"
	    "                ps/2\n"
	    "                mmhittab\n"
	    ,progname);
 
    exit(1);
}


/**
 ** Mouse interface code, courtesy of XFree86 3.1.2.
 **
 ** Note: Various bits have been trimmed, and in my shortsighted enthusiasm
 ** to clean, reformat and rationalise naming, it's quite possible that
 ** some things in here have been broken.
 **
 ** I hope not 8)
 **
 ** The following code is derived from a module marked :
 **/

/* $XConsortium: xf86_Mouse.c,v 1.2 94/10/12 20:33:21 kaleb Exp $ */
/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86_Mouse.c,v 3.2 1995/01/28
 17:03:40 dawes Exp $ */
/*
 *
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 * Copyright 1993 by David Dawes <dawes@physics.su.oz.au>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Thomas Roell and David Dawes not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Thomas Roell
 * and David Dawes makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * THOMAS ROELL AND DAVID DAWES DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THOMAS ROELL OR DAVID DAWES BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


void
r_init(void)
{
    /**
     ** This comment is a little out of context here, but it contains 
     ** some useful information...
     ********************************************************************
     **
     ** The following lines take care of the Logitech MouseMan protocols.
     **
     ** NOTE: There are different versions of both MouseMan and TrackMan!
     **       Hence I add another protocol P_LOGIMAN, which the user can
     **       specify as MouseMan in his XF86Config file. This entry was
     **       formerly handled as a special case of P_MS. However, people
     **       who don't have the middle button problem, can still specify
     **       Microsoft and use P_MS.
     **
     ** By default, these mice should use a 3 byte Microsoft protocol
     ** plus a 4th byte for the middle button. However, the mouse might
     ** have switched to a different protocol before we use it, so I send
     ** the proper sequence just in case.
     **
     ** NOTE: - all commands to (at least the European) MouseMan have to
     **         be sent at 1200 Baud.
     **       - each command starts with a '*'.
     **       - whenever the MouseMan receives a '*', it will switch back
     **	 to 1200 Baud. Hence I have to select the desired protocol
     **	 first, then select the baud rate.
     **
     ** The protocols supported by the (European) MouseMan are:
     **   -  5 byte packed binary protocol, as with the Mouse Systems
     **      mouse. Selected by sequence "*U".
     **   -  2 button 3 byte MicroSoft compatible protocol. Selected
     **      by sequence "*V".
     **   -  3 button 3+1 byte MicroSoft compatible protocol (default).
     **      Selected by sequence "*X".
     **
     ** The following baud rates are supported:
     **   -  1200 Baud (default). Selected by sequence "*n".
     **   -  9600 Baud. Selected by sequence "*q".
     **
     ** Selecting a sample rate is no longer supported with the MouseMan!
     ** Some additional lines in xf86Config.c take care of ill configured
     ** baud rates and sample rates. (The user will get an error.)
     */

  
    if (rodent.rtype == R_LOGIMAN)
    {
	write(rodent.mfd, "*X", 2);
    }
    if ((rodent.rtype != R_BUSMOUSE) && (rodent.rtype != R_PS_2))
    {
	if (rodent.rtype == R_LOGITECH)
	    write(rodent.mfd, "S", 1);
	/** Support for the Hitachi PUMA Plus tablet not brought through */
	if      (rodent.samplerate <= 0)   write(rodent.mfd, "O", 1);
	else if (rodent.samplerate <= 15)  write(rodent.mfd, "J", 1);
	else if (rodent.samplerate <= 27)  write(rodent.mfd, "K", 1);
	else if (rodent.samplerate <= 42)  write(rodent.mfd, "L", 1);
	else if (rodent.samplerate <= 60)  write(rodent.mfd, "R", 1);
	else if (rodent.samplerate <= 85)  write(rodent.mfd, "M", 1);
	else if (rodent.samplerate <= 125) write(rodent.mfd, "Q", 1);
	else				   write(rodent.mfd, "N", 1);
    }

#ifdef CLEARDTR_SUPPORT		/* XXX find someone to tell me about this */
    if (xf86Info.mseType == P_MSC && (xf86Info.mouseFlags & MF_CLEAR_DTR))
    {
	int val = TIOCM_DTR;
	ioctl(xf86Info.mseFd, TIOCMBIC, &val);
    }
    if (xf86Info.mseType == P_MSC && (xf86Info.mouseFlags & MF_CLEAR_RTS))
    {
	int val = TIOCM_RTS;
	ioctl(xf86Info.mseFd, TIOCMBIC, &val);
    }
#endif
}

ACTIVITY *
r_protocol(u_char rBuf)
{
    static int           pBufP = 0;
    static unsigned char pBuf[8];
    static ACTIVITY	 act;

    static unsigned char proto[9][5] = {
    /*  hd_mask hd_id   dp_mask dp_id   nobytes */
    {	0,	0,	0,	0,	0	},  /* nomouse */
    { 	0x40,	0x40,	0x40,	0x00,	3 	},  /* MicroSoft */
    {	0xf8,	0x80,	0x00,	0x00,	5	},  /* MouseSystems */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* MMSeries */
    {	0xe0,	0x80,	0x80,	0x00,	3	},  /* Logitech */
    {	0xf8,	0x80,	0x00,	0x00,	5	},  /* BusMouse */
    { 	0x40,	0x40,	0x40,	0x00,	3 	},  /* MouseMan */
    {	0xc0,	0x00,	0x00,	0x00,	3	},  /* PS/2 mouse */
    };
  
    debug("received char 0x%x",(int)rBuf);

    /*
     * Hack for resyncing: We check here for a package that is:
     *  a) illegal (detected by wrong data-package header)
     *  b) invalid (0x80 == -128 and that might be wrong for MouseSystems)
     *  c) bad header-package
     *
     * NOTE: b) is a voilation of the MouseSystems-Protocol, since values of
     *       -128 are allowed, but since they are very seldom we can easily
     *       use them as package-header with no button pressed.
     * NOTE/2: On a PS/2 mouse any byte is valid as a data byte. Furthermore,
     *         0x80 is not valid as a header byte. For a PS/2 mouse we skip
     *         checking data bytes.
     *         For resyncing a PS/2 mouse we require the two most significant
     *         bits in the header byte to be 0. These are the overflow bits,
     *         and in case of an overflow we actually lose sync. Overflows
     *         are very rare, however, and we quickly gain sync again after
     *         an overflow condition. This is the best we can do. (Actually,
     *         we could use bit 0x08 in the header byte for resyncing, since
     *         that bit is supposed to be always on, but nobody told
     *         Microsoft...)
     */

    if (pBufP != 0 && rodent.rtype != R_PS_2 &&
	((rBuf & proto[rodent.rtype][2]) != proto[rodent.rtype][3]
	 || rBuf == 0x80))
    {
	pBufP = 0;		/* skip package */
    }
    
    if (pBufP == 0 &&
	(rBuf & proto[rodent.rtype][0]) != proto[rodent.rtype][1])
    {
	/*
	 * Hack for Logitech MouseMan Mouse - Middle button
	 *
	 * Unfortunately this mouse has variable length packets: the standard
	 * Microsoft 3 byte packet plus an optional 4th byte whenever the
	 * middle button status changes.
	 *
	 * We have already processed the standard packet with the movement
	 * and button info.  Now post an event message with the old status
	 * of the left and right buttons and the updated middle button.
	 */

	/*
	 * Even worse, different MouseMen and TrackMen differ in the 4th
	 * byte: some will send 0x00/0x20, others 0x01/0x21, or even
	 * 0x02/0x22, so I have to strip off the lower bits.
	 */
	if ((rodent.rtype == R_MICROSOFT || rodent.rtype == R_LOGIMAN)
	    && (char)(rBuf & ~0x23) == 0)
	{
	    act.buttons = ((int)(rBuf & 0x20) >> 4)
		| (rodent.lastbuttons & 0x05);
	    rodent.lastbuttons = act.buttons;	/* save new button state */
	    return(&act);
	}

	return(NULL);				/* skip package */
    }
        
    pBuf[pBufP++] = rBuf;
    if (pBufP != proto[rodent.rtype][4]) return(NULL);
    
    /*
     * assembly full package
     */

    debug("Assembled full packet (len %d) %x,%x,%x,%x,%x",
	proto[rodent.rtype][4], pBuf[0],pBuf[1],pBuf[2],pBuf[3],pBuf[4]);

    switch(rodent.rtype) 
    {
    case R_LOGIMAN:		/* MouseMan / TrackMan */
    case R_MICROSOFT:		/* Microsoft */
	if (rodent.flags & ChordMiddle)
	    act.buttons = (((int) pBuf[0] & 0x30) == 0x30) ? 2 :
		           ((int)(pBuf[0]&0x20)>>3) | ((int)(pBuf[0]&0x10)>>4);
	else
	    act.buttons =   (rodent.lastbuttons & 2)
		          | ((int)(pBuf[0] & 0x20) >> 3)
		          | ((int)(pBuf[0] & 0x10) >> 4);
	act.dx = (char)(((pBuf[0] & 0x03) << 6) | (pBuf[1] & 0x3F));
	act.dy = (char)(((pBuf[0] & 0x0C) << 4) | (pBuf[2] & 0x3F));
	break;
      
    case R_MOUSESYS:		/* Mouse Systems Corp */
	act.buttons = (~pBuf[0]) & 0x07;
	act.dx =    (char)(pBuf[1]) + (char)(pBuf[3]);
	act.dy = - ((char)(pBuf[2]) + (char)(pBuf[4]));
	break;
      
    case R_MMSERIES:		/* MM Series */
    case R_LOGITECH:		/* Logitech Mice */
	act.buttons = pBuf[0] & 0x07;
	act.dx = (pBuf[0] & 0x10) ?   pBuf[1] : - pBuf[1];
	act.dy = (pBuf[0] & 0x08) ? - pBuf[2] :   pBuf[2];
	break;
      
    case R_BUSMOUSE:		/* BusMouse */
	act.buttons = (~pBuf[0]) & 0x07;
	act.dx =   (char)pBuf[1];
	act.dy = - (char)pBuf[2];
	break;

    case R_PS_2:		/* PS/2 mouse */
	act.buttons = (pBuf[0] & 0x04) >> 1 | /* Middle */
	              (pBuf[0] & 0x02) >> 1 | /* Right */
		      (pBuf[0] & 0x01) << 2; /* Left */
	act.dx = (pBuf[0] & 0x10) ?    pBuf[1]-256  :  pBuf[1];
	act.dy = (pBuf[0] & 0x20) ?  -(pBuf[2]-256) : -pBuf[2];
	break;
    }
    pBufP = 0;
    return(&act);
}
