/*
 * Copyright (c) 1998 Michael Smith (msmith@freebsd.org)
 * Copyright (c) 1997 Kazutaka YOKOTA (yokota@zodiac.mech.utsunomiya-u.ac.jp)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	From Id: probe_keyboard.c,v 1.13 1997/06/09 05:10:55 bde Exp
 *
 * $FreeBSD$
 */

#include <stand.h>
#include <bootstrap.h>
#include <btxv86.h>
#include <machine/psl.h>
#ifdef PC98
#include <machine/cpufunc.h>
#endif
#include "libi386.h"

#if KEYBOARD_PROBE
#include <machine/cpufunc.h>

static int	probe_keyboard(void);
#endif
static void	vidc_probe(struct console *cp);
static int	vidc_init(int arg);
static void	vidc_putchar(int c);
static int	vidc_getchar(void);
static int	vidc_ischar(void);

static int	vidc_started;

#ifdef TERM_EMU
void		end_term(void);
void		bail_out(int c);
void		vidc_term_emu(int c);
void		get_pos(void);
void		curs_move(int x, int y);
void		write_char(int c, int fg, int bg);
void		scroll_up(int rows, int fg, int bg);
int		pow10(int i);
void		AB(void);
void		AF(void);
void		CD(void);
void		CM(void);
void		HO(void);
void		ME(void);

static int	args[2],argc,br;
static int	fg,bg,dig;
static int	fg_c,bg_c,curx,cury;
static int	esc;
#endif

#ifdef PC98
static unsigned short *crtat, *Crtat;
static int row = 25, col = 80;
#ifdef TERM_EMU
unsigned int at2pc98(unsigned int fg_at, unsigned int bg_at);
#endif
#endif

struct console vidconsole = {
    "vidconsole",
    "internal video/keyboard",
    0,
    vidc_probe,
    vidc_init,
    vidc_putchar,
    vidc_getchar,
    vidc_ischar
};

static void
vidc_probe(struct console *cp)
{
    
    /* look for a keyboard */
#if KEYBOARD_PROBE
    if (probe_keyboard())
#endif
    {
	
	cp->c_flags |= C_PRESENTIN;
    }

    /* XXX for now, always assume we can do BIOS screen output */
    cp->c_flags |= C_PRESENTOUT;
}

static int
vidc_init(int arg)
{
    int		i;
#ifdef PC98
    int		hw_cursor;
#endif

    if (vidc_started && arg == 0)
	return(0);
    vidc_started = 1;
#ifdef PC98
    Crtat = (unsigned short *)PTOV(0xA0000);
    while((inb(0x60) & 0x04) == 0);
    outb(0x62, 0xe0);
    while((inb(0x60) & 0x01) == 0);
    hw_cursor = inb(0x62);
    hw_cursor |= (inb(0x62) << 8);
    inb(0x62);
    inb(0x62);
    inb(0x62);
    crtat = Crtat + hw_cursor;
#endif
#ifdef TERM_EMU
    /* Init terminal emulator */
    end_term();
    get_pos();
    curs_move(curx,cury);
    fg_c=7;
    bg_c=0;
#endif
    for(i = 0; i < 10 && vidc_ischar(); i++)
	  (void)vidc_getchar();
    return(0);	/* XXX reinit? */
}

#ifdef PC98
static void
beep(void)
{
	outb(0x37, 6);
	delay(40000);
	outb(0x37, 7);
}
#endif

#if 0
static void
vidc_biosputchar(int c)
{
#ifdef PC98
    unsigned short *cp;
    int i, pos;

#ifdef TERM_EMU
    *crtat = (c == 0x5c ? 0xfc : c);
    *(crtat + 0x1000) = at2pc98(fg, bg);
#else
    switch(c) {
    case '\b':
        crtat--;
	break;
    case '\r':
        crtat -= (crtat - Crtat) % col;
	break;
    case '\n':
        crtat += col;
	break;
    default:
        *crtat = (c == 0x5c ? 0xfc : c);
	*(crtat++ + 0x1000) = 0xe1;
	break;
    }

    if (crtat >= Crtat + col * row) {
        cp = Crtat;
	for (i = 1; i < row; i++) {
	    bcopy((void *)(cp+col), (void *)cp, col*2);
	    cp += col;
	}
	for (i = 0; i < col; i++) {
	    *cp++ = ' ';
	}
	crtat -= col;
    }
    pos = crtat - Crtat;
    while((inb(0x60) & 0x04) == 0) {}
    outb(0x62, 0x49);
    outb(0x60, pos & 0xff);
    outb(0x60, pos >> 8);
#endif
#else
    v86.ctl = 0;
    v86.addr = 0x10;
    v86.eax = 0xe00 | (c & 0xff);
    v86.ebx = 0x7;
    v86int();
#endif
}
#endif

static void
vidc_rawputchar(int c)
{
    int		i;

    if(c == '\t')
	/* lame tab expansion */
	for (i = 0; i < 8; i++)
	    vidc_rawputchar(' ');
    else {
#if !defined(TERM_EMU) && !defined(PC98)
        vidc_biosputchar(c);
#else
	/* Emulate AH=0eh (teletype output) */
	switch(c) {
	case '\a':
#ifdef PC98
		beep();
#else
		vidc_biosputchar(c);
#endif
		return;
	case '\r':
		curx=0;
		curs_move(curx,cury);
		return;
	case '\n':
		cury++;
		if(cury>24) {
			scroll_up(1,fg_c,bg_c);
			cury--;
		} else {
			curs_move(curx,cury);
		}
		return;
	case '\b':
		if(curx>0) {
			curx--;
			curs_move(curx,cury);
			/* write_char(' ',fg_c,bg_c); XXX destructive(!) */
			return;
		}
		return;
	default:
		write_char(c,fg_c,bg_c);
		curx++;
		if(curx>79) {
			curx=0;
			cury++;
		}
		if(cury>24) {
			curx=0;
			scroll_up(1,fg_c,bg_c);
			cury--;
		}
	}
	curs_move(curx,cury);
#endif
    }
}

#ifdef TERM_EMU

/* Get cursor position on the screen. Result is in edx. Sets
 * curx and cury appropriately.
 */
void
get_pos(void)
{
#ifdef PC98
    int pos = crtat - Crtat;
    curx = pos % col;
    cury = pos / col;
#else
    v86.ctl = 0;
    v86.addr = 0x10;
    v86.eax = 0x0300;
    v86.ebx = 0x0;
    v86int();
    curx=v86.edx & 0x00ff;
    cury=(v86.edx & 0xff00)>>8;
#endif
}

/* Move cursor to x rows and y cols (0-based). */
void
curs_move(int x, int y)
{
#ifdef PC98
    int pos;
    pos = x + y*col;
    crtat = Crtat + pos;
    pos = crtat - Crtat;
    while((inb(0x60) & 0x04) == 0) {}
    outb(0x62, 0x49);
    outb(0x60, pos & 0xff);
    outb(0x60, pos >> 8);
    curx=x;
    cury=y;
#define isvisible(c)	(((c)>32) && ((c)<255))
    if(!isvisible(*crtat & 0x00ff)) {
	write_char(' ',fg_c,bg_c);
    }
#else
    v86.ctl = 0;
    v86.addr = 0x10;
    v86.eax = 0x0200;
    v86.ebx = 0x0;
    v86.edx = ((0x00ff & y)<<8)+(0x00ff & x);
    v86int();
    curx=x;
    cury=y;
    /* If there is ctrl char at this position, cursor would be invisible.
     * Make it a space instead.
     */
    v86.ctl=0;
    v86.addr = 0x10;
    v86.eax = 0x0800;
    v86.ebx= 0x0;
    v86int();
#define isvisible(c)	(((c)>32) && ((c)<255))
    if(!isvisible(v86.eax & 0x00ff)) {
	write_char(' ',fg_c,bg_c);
    }
#endif
}

/* Scroll up the whole window by a number of rows. If rows==0,
 * clear the window. fg and bg are attributes for the new lines
 * inserted in the window.
 */
void
scroll_up(int rows, int fgcol, int bgcol)
{
#ifdef PC98
        unsigned short *cp;
	int i;

	if(rows==0) rows=25;
	cp = Crtat;
	for (i = rows ; i < row; i++) {
	    bcopy((void *)(cp+col), (void *)cp, col*2);
	    cp += col;
	}
	for (i = 0; i < col; i++) {
	    *(cp + 0x1000) = at2pc98(fgcol, bgcol);
	    *cp++ = ' ';
	}
#else
	if(rows==0) rows=25;
	v86.ctl = 0;
	v86.addr = 0x10;
	v86.eax = 0x0600+(0x00ff & rows);
	v86.ebx = (bgcol<<12)+(fgcol<<8);
	v86.ecx = 0x0;
	v86.edx = 0x184f;
	v86int();
#endif
}

/* Write character and attribute at cursor position. */
void
write_char(int c, int fgcol, int bgcol)
{
#ifdef PC98
	*crtat = (c == 0x5c ? 0xfc : c);
	*(crtat + 0x1000) = at2pc98(fgcol, bgcol);
#else
	v86.ctl=0;
    	v86.addr = 0x10;
    	v86.eax = 0x0900+(0x00ff & c);
	v86.ebx = (bgcol<<4)+fgcol;
    	v86.ecx = 0x1;
    	v86int();
#endif
}

/* Calculate power of 10 */
int
pow10(int i)
{
	int res=1;

	while(i-->0) {
		res*=10;
	}
	return res;
}

/**************************************************************/
/*
 * Screen manipulation functions. They use accumulated data in
 * args[] and argc variables.
 *
 */

/* Set background color */
void
AB(void){
	bg_c=args[0];
	end_term();
}

/* Set foreground color */
void
AF(void)
{
	fg_c=args[0];
	end_term();
}

/* Clear display from current position to end of screen */
void
CD(void)
{
    get_pos();
#ifdef PC98
    for(;crtat <= Crtat + col*row; crtat++){
	*crtat = ' ';
	*(crtat + 0x1000) = at2pc98(fg_c, bg_c);
    }
#else
    v86.ctl = 0;
    v86.addr = 0x10;
    v86.eax = 0x0600;
    v86.ebx = (bg_c<<4)+fg_c;
    v86.ecx = v86.edx;
    v86.edx = 0x184f;
    v86int();
#endif
    curx=0;
    curs_move(curx,cury);
    end_term();
}

/* Absolute cursor move to args[0] rows and args[1] columns
 * (the coordinates are 1-based).
 */
void
CM(void)
{
    if(args[0]>0) args[0]--;
    if(args[1]>0) args[1]--;
    curs_move(args[1],args[0]);
    end_term();
}

/* Home cursor (left top corner) */
void
HO(void)
{
	argc=1;
	args[0]=args[1]=1;
	CM();
}

/* Exit attribute mode (reset fore/back-ground colors to defaults) */
void
ME(void)
{
	fg_c=7;
	bg_c=0;
	end_term();
}

/* Clear internal state of the terminal emulation code */
void
end_term(void)
{
	esc=0;
	argc=-1;
	fg=bg=br=0;
	args[0]=args[1]=0;
	dig=0;
}

/* Gracefully exit ESC-sequence processing in case of misunderstanding */
void
bail_out(int c)
{
	char buf[6],*ch;

	if(esc) vidc_rawputchar('\033');
	if(br) vidc_rawputchar('[');
	if(argc>-1) {
		sprintf(buf,"%d",args[0]);
		ch=buf;
		while(*ch) vidc_rawputchar(*ch++);
		
		if(argc>0) {
			vidc_rawputchar(';');
			sprintf(buf,"%d",args[1]);
			ch=buf;
			while(*ch) vidc_rawputchar(*ch++);
		}
	}
	vidc_rawputchar(c);
	end_term();
}

/* Emulate basic capabilities of cons25 terminal */
void
vidc_term_emu(int c)
{

    if(!esc) {
	if(c=='\033') {
	    esc=1;
	} else {
	    vidc_rawputchar(c);
	}
	return;
    }

    /* Do ESC sequences processing */
    switch(c) {
    case '\033':
	/* ESC in ESC sequence - error */
	bail_out(c);
	break;
    case '[':
	/* Check if it's first char after ESC */
        if(argc<0) {
            br=1;
        } else {
	    bail_out(c);
        }
	break;
    case 'H':
	/* Emulate \E[H (cursor home) and 
	 * \E%d;%dH (cursor absolute move) */
	if(br) {
	    switch(argc) {
	    case -1:
		HO();
		break;
	    case 1:
		if(fg) args[0]+=pow10(dig)*3;
		if(bg) args[0]+=pow10(dig)*4;
		CM();
		break;
	    default:
		bail_out(c);
	    }
	} else bail_out(c);
	break;
    case 'J':
	/* Emulate \EJ (clear to end of screen) */
	if(br && argc<0) {
	    CD();
	} else bail_out(c);
	break;
    case ';':
	/* perhaps args separator */
	if(br && (argc>-1)) {
	    argc++;
	} else bail_out(c);
	break;
    case 'm':
	/* Change char attributes */
	if(br) {
	    switch(argc) {
	    case -1:
		ME();
		break;
	    case 0:
		if(fg) AF();
		else AB();
		break;
	    default:
		bail_out(c);
	    }
	} else bail_out(c);
	break;
    default:
	if(isdigit(c)) {
	    /* Carefully collect numeric arguments */
	    /* XXX this is ugly. */
	    if(br) {
	        if(argc==-1) {
	     	    argc=0;
		    args[argc]=0;
		    dig=0;
		    /* in case we're in error... */
		    if(c=='3') {
			fg=1;
			return;
		    }
		    if(c=='4') {
			bg=1;
			return;
		    }
	     	    args[argc]=(int)(c-'0');
		    dig=1;
	     	    args[argc+1]=0;
	    	} else {
		    args[argc]=args[argc]*10+(int)(c-'0');
		    if(argc==0) dig++;
	    	}
	    } else bail_out(c);
	} else bail_out(c);
	break;
    }
}
#endif

static void
vidc_putchar(int c)
{
#ifdef TERM_EMU
    vidc_term_emu(c);
#else
    vidc_rawputchar(c);
#endif
}

static int
vidc_getchar(void)
{
    if (vidc_ischar()) {
	v86.ctl = 0;
#ifdef PC98
	v86.addr = 0x18;
#else
	v86.addr = 0x16;
#endif
	v86.eax = 0x0;
	v86int();
	return(v86.eax & 0xff);
    } else {
	return(-1);
    }
}

static int
vidc_ischar(void)
{
#ifdef PC98
    v86.ctl = 0;
    v86.addr = 0x18;
    v86.eax = 0x100;
    v86int();
    return((v86.ebx >> 8) & 0x1);
#else
    v86.ctl = V86_FLAGS;
    v86.addr = 0x16;
    v86.eax = 0x100;
    v86int();
    return(!(v86.efl & PSL_Z));
#endif
}

#if KEYBOARD_PROBE

#ifdef PC98
static int
probe_keyboard(void)
{
    return (*(u_char *)PTOV(0xA1481) & 0x48);
}
#else   /* PC98 */
#define PROBE_MAXRETRY	5
#define PROBE_MAXWAIT	400
#define IO_DUMMY	0x84
#define IO_KBD		0x060		/* 8042 Keyboard */

/* selected defines from kbdio.h */
#define KBD_STATUS_PORT 	4	/* status port, read */
#define KBD_DATA_PORT		0	/* data port, read/write 
					 * also used as keyboard command
					 * and mouse command port 
					 */
#define KBDC_ECHO		0x00ee
#define KBDS_ANY_BUFFER_FULL	0x0001
#define KBDS_INPUT_BUFFER_FULL	0x0002
#define KBD_ECHO		0x00ee

/* 7 microsec delay necessary for some keyboard controllers */
static void
delay7(void)
{
    /* 
     * I know this is broken, but no timer is available yet at this stage...
     * See also comments in `delay1ms()'.
     */
    inb(IO_DUMMY); inb(IO_DUMMY);
    inb(IO_DUMMY); inb(IO_DUMMY);
    inb(IO_DUMMY); inb(IO_DUMMY);
}

/*
 * This routine uses an inb to an unused port, the time to execute that
 * inb is approximately 1.25uS.  This value is pretty constant across
 * all CPU's and all buses, with the exception of some PCI implentations
 * that do not forward this I/O adress to the ISA bus as they know it
 * is not a valid ISA bus address, those machines execute this inb in
 * 60 nS :-(.
 *
 */
static void
delay1ms(void)
{
    int i = 800;
    while (--i >= 0)
	(void)inb(0x84);
}

/* 
 * We use the presence/absence of a keyboard to determine whether the internal
 * console can be used for input.
 *
 * Perform a simple test on the keyboard; issue the ECHO command and see
 * if the right answer is returned. We don't do anything as drastic as
 * full keyboard reset; it will be too troublesome and take too much time.
 */
static int
probe_keyboard(void)
{
    int retry = PROBE_MAXRETRY;
    int wait;
    int i;

    while (--retry >= 0) {
	/* flush any noise */
	while (inb(IO_KBD + KBD_STATUS_PORT) & KBDS_ANY_BUFFER_FULL) {
	    delay7();
	    inb(IO_KBD + KBD_DATA_PORT);
	    delay1ms();
	}

	/* wait until the controller can accept a command */
	for (wait = PROBE_MAXWAIT; wait > 0; --wait) {
	    if (((i = inb(IO_KBD + KBD_STATUS_PORT)) 
                & (KBDS_INPUT_BUFFER_FULL | KBDS_ANY_BUFFER_FULL)) == 0)
		break;
	    if (i & KBDS_ANY_BUFFER_FULL) {
		delay7();
	        inb(IO_KBD + KBD_DATA_PORT);
	    }
	    delay1ms();
	}
	if (wait <= 0)
	    continue;

	/* send the ECHO command */
	outb(IO_KBD + KBD_DATA_PORT, KBDC_ECHO);

	/* wait for a response */
	for (wait = PROBE_MAXWAIT; wait > 0; --wait) {
	     if (inb(IO_KBD + KBD_STATUS_PORT) & KBDS_ANY_BUFFER_FULL)
		 break;
	     delay1ms();
	}
	if (wait <= 0)
	    continue;

	delay7();
	i = inb(IO_KBD + KBD_DATA_PORT);
#ifdef PROBE_KBD_BEBUG
        printf("probe_keyboard: got 0x%x.\n", i);
#endif
	if (i == KBD_ECHO) {
	    /* got the right answer */
	    return (0);
	}
    }

    return (1);
}
#endif
#endif /* KEYBOARD_PROBE */

#ifdef TERM_EMU
#ifdef PC98
static u_char	ibmpc_to_pc98[16] =
 { 0x01,0x21,0x81,0xa1,0x41,0x61,0xc1,0xe1, 0x09,0x29,0x89,0xa9,0x49,0x69,0xc9,0xe9 };
static u_char	ibmpc_to_pc98rev[16] = 
 { 0x05,0x25,0x85,0xa5,0x45,0x65,0xc5,0xe5, 0x0d,0x2d,0x8d,0xad,0x4d,0x6d,0xcd,0xed };

unsigned int
at2pc98(unsigned int fg_at, unsigned int bg_at)
{
    unsigned int at;

    if (bg_at) {
	if (bg_at & 0x80) {
	    if (bg_at & 0x70) {
		/* reverse & blink */
		at = ibmpc_to_pc98rev[bg_at >> 4] | 0x02;
	    } else {
		/* normal & blink */
		at = ibmpc_to_pc98[fg_at] | 0x02;
	    }
	} else {
	    /* reverse */
	    at = ibmpc_to_pc98rev[bg_at >> 4];
	}
    } else {
	/* normal */
	at = ibmpc_to_pc98[fg_at];
    }
    at |= ((fg_at|bg_at) << 8);
    return (at);
}
#endif
#endif
