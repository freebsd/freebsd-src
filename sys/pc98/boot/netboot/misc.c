/**************************************************************************
MISC Support Routines
**************************************************************************/

#include "netboot.h"
#ifdef	PC98
#include "../../pc98/pc98.h"
void putchar(int c);
void putc(int c);
#endif
#define NO_SWITCH		/* saves space */

/**************************************************************************
TWIDDLE
**************************************************************************/
twiddle()
{
	static int count=0;
	char tiddles[]="-\\|/";
	putchar(tiddles[(count++)&3]);
	putchar('\b');
}

/**************************************************************************
BCOPY
**************************************************************************/
bcopy(s,d,n)
	char *s, *d;
	int n;
{
	while ((n--) > 0) {
		*(d++) = *(s++);
	}
}

/**************************************************************************
BZERO
**************************************************************************/
bzero(d,n)
	char *d;
	int n;
{
	while ((n--) > 0) {
		*(d++) = 0;
	}
}

/**************************************************************************
BCOMPARE
**************************************************************************/
bcompare(d,s,n)
	char *d,*s;
	int n;
{
	while ((n--) > 0) {
		if (*(d++) != *(s++)) return(0);
	}
	return(1);
}

/**************************************************************************
SUBSTR (slightly wacky but functional)
**************************************************************************/
char *substr(a,b)
char *a,*b;
{
char *loc1;
char *loc2;

        while (*a != '\0') {
                loc1 = a;
                loc2 = b;
                while (*loc1 == *loc2++) {
                        if (*loc1 == '\0') return (0);
                        loc1++;
                        if (*loc2 == '\0') return (loc1);
                }
        a++;
        }
        return (0);
}

/**************************************************************************
PRINTF and friends

	Formats:
		%X	- 4 byte ASCII (8 hex digits)
		%x	- 2 byte ASCII (4 hex digits)
		%b	- 1 byte ASCII (2 hex digits)
		%d	- decimal
		%c	- ASCII char
		%s	- ASCII string
		%I	- Internet address in x.x.x.x notation
		%L	- Binary long
		%S	- String (multiple of 4 bytes) preceded with 4 byte
			  binary length
		%M	- Copy memory.  Takes two args, len and ptr
**************************************************************************/
static char hex[]="0123456789ABCDEF";
char *do_printf(buf, fmt, dp)
	char *buf, *fmt;
	int  *dp;
{
	register char *p;
	char tmp[16];
	while (*fmt) {
		if (*fmt == '%') {	/* switch() uses more space */
			fmt++;
			if (*fmt == 'L') {
				register int h = *(dp++);
				*(buf++) = h>>24;
				*(buf++) = h>>16;
				*(buf++) = h>>8;
				*(buf++) = h;
			}
			if (*fmt == 'S') {
				register int len = 0;
				char *lenptr = buf;
				p = (char *)*dp++;
				buf += 4;
				while (*p) {
					*(buf++) = *p++;
					len ++;
				}
				*(lenptr++) = len>>24;
				*(lenptr++) = len>>16;
				*(lenptr++) = len>>8;
				*lenptr = len;
				while (len & 3) {
					*(buf++) = 0;
					len ++;
				}
			}
			if (*fmt == 'M') {
				register int len = *(dp++);
				bcopy((char *)*dp++, buf, len);
				buf += len;
			}
			if (*fmt == 'X') {
				register int h = *(dp++);
				*(buf++) = hex[(h>>28)& 0x0F];
				*(buf++) = hex[(h>>24)& 0x0F];
				*(buf++) = hex[(h>>20)& 0x0F];
				*(buf++) = hex[(h>>16)& 0x0F];
				*(buf++) = hex[(h>>12)& 0x0F];
				*(buf++) = hex[(h>>8)& 0x0F];
				*(buf++) = hex[(h>>4)& 0x0F];
				*(buf++) = hex[h& 0x0F];
			}
			if (*fmt == 'x') {
				register int h = *(dp++);
				*(buf++) = hex[(h>>12)& 0x0F];
				*(buf++) = hex[(h>>8)& 0x0F];
				*(buf++) = hex[(h>>4)& 0x0F];
				*(buf++) = hex[h& 0x0F];
			}
			if (*fmt == 'b') {
				register int h = *(dp++);
				*(buf++) = hex[(h>>4)& 0x0F];
				*(buf++) = hex[h& 0x0F];
			}
			if (*fmt == 'd') {
				register int dec = *(dp++);
				p = tmp;
				if (dec < 0) {
					*(buf++) = '-';
					dec = -dec;
				}
				do {
					*(p++) = '0' + (dec%10);
					dec = dec/10;
				} while(dec);
				while ((--p) >= tmp) *(buf++) = *p;
			}
			if (*fmt == 'I') {
				buf = sprintf(buf,"%d.%d.%d.%d",
					(*(dp)>>24) & 0x00FF,
					(*(dp)>>16) & 0x00FF,
					(*(dp)>>8) & 0x00FF,
					*dp & 0x00FF);
				dp++;
			}
			if (*fmt == 'c')
				*(buf++) = *(dp++);
			if (*fmt == 's') {
				p = (char *)*dp++;
				while (*p) *(buf++) = *p++;
			}
		} else *(buf++) = *fmt;
		fmt++;
	}
	*buf = 0;
	return(buf);
}

char *sprintf(buf, fmt, data)
	char *fmt, *buf;
	int data;
{
	return(do_printf(buf,fmt, &data));
}

printf(fmt,data)
	char *fmt;
	int data;
{
	char buf[1024],*p;
	p = buf;
	do_printf(buf,fmt,&data);
	while (*p) {
		if (*p=='\n') putchar('\r');
		putchar(*p++);
	}
}

/**************************************************************************
SETIP - Convert an ascii x.x.x.x to binary form
**************************************************************************/
setip(p, i)
	char *p;
	unsigned *i;
{
	unsigned ip = 0;
	int val;
	if (((val = getdec(&p)) < 0) || (val > 255)) return(0);
	if (*p != '.') return(0);
	p++;
	ip = val;
	if (((val = getdec(&p)) < 0) || (val > 255)) return(0);
	if (*p != '.') return(0);
	p++;
	ip = (ip << 8) | val;
	if (((val = getdec(&p)) < 0) || (val > 255)) return(0);
	if (*p != '.') return(0);
	p++;
	ip = (ip << 8) | val;
	if (((val = getdec(&p)) < 0) || (val > 255)) return(0);
	*i = (ip << 8) | val;
	return(1);
}

getdec(ptr)
	char **ptr;
{
	char *p = *ptr;
	int ret=0;
	if ((*p < '0') || (*p > '9')) return(-1);
	while ((*p >= '0') && (*p <= '9')) {
		ret = ret*10 + (*p - '0');
		p++;
	}
	*ptr = p;
	return(ret);
}


#define K_RDWR 		0x60		/* keyboard data & cmds (read/write) */
#define K_STATUS 	0x64		/* keyboard status */
#define K_CMD	 	0x64		/* keybd ctlr command (write-only) */

#define K_OBUF_FUL 	0x01		/* output buffer full */
#define K_IBUF_FUL 	0x02		/* input buffer full */

#define KC_CMD_WIN	0xd0		/* read  output port */
#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_A20		0x9f		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   disable clock line */

/*
 * Gate A20 for high memory
 */
unsigned char	x_20 = KB_A20;
gateA20()
{
#ifdef PC98
	outb(0xf2, 0x00);
	outb(0xf6, 0x02);
#else
#ifdef	IBM_L40
	outb(0x92, 0x2);
#else	IBM_L40
	while (inb(K_STATUS) & K_IBUF_FUL);
	while (inb(K_STATUS) & K_OBUF_FUL)
		(void)inb(K_RDWR);

	outb(K_CMD, KC_CMD_WOUT);
	while (inb(K_STATUS) & K_IBUF_FUL);
	outb(K_RDWR, x_20);
	while (inb(K_STATUS) & K_IBUF_FUL);
#endif	IBM_L40
#endif
}

#ifdef	PC98
void
putchar(int c)
{
	if (c == '\n')
		putc('\r');
	putc(c);
}

static unsigned short *Crtat = (unsigned short *)0;
static int row;
static int col;

void putc(int c)
{
	static unsigned short *crtat;
	unsigned char sys_type;
	unsigned short *cp;
	int i, pos;

	if (Crtat == 0) {
		sys_type = *(unsigned char *)0x0a1501/*0x11501*/;
		if (sys_type & 0x08) {
			Crtat = (unsigned short *)0x0e0000/*0x50000*/;
			crtat = Crtat;
			row = 31;
			col = 80;
		} else {
			Crtat = (unsigned short *)0x0a0000/*0x10000*/;
			crtat = Crtat;
			row = 25;
			col = 80;
		}
	}

	switch(c) {
	case '\t':
		do {
			putc(' ');
		} while ((int)crtat % 16);
		break;
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
		for (i = 1; i < row; i++)
			bcopy(Crtat+col*i, Crtat+col*(i-1), col*2);
		for (i = 0, cp = Crtat + col * (row - 1); i < col*2; i++) {
			*cp++ = ' ';
		}
		crtat -= col;
	}
	pos = crtat - Crtat;
	while((inb(0x60) & 0x04) == 0) {}
	outb(0x62, 0x49);
	outb(0x60, pos & 0xff);
	outb(0x60, pos >> 8);
}

unsigned int bios98getdate();

unsigned int currticks() 
{
	unsigned int biostime = bios98getdate() >> 8;
	unsigned int time;
	static unsigned int oldtime;
	time = ((   (biostime >> 4)  & 0x0f)*10
		+   (biostime        & 0x0f))*3600	/*  hour    */
		+ (((biostime >> 12) & 0x0f)*10
		+  ((biostime >>  8) & 0x0f))*60	/*  minute  */
		+ (((biostime >> 20) & 0x0f)*10
		+  ((biostime >> 16) & 0x0f));		/*  second  */
	while(oldtime > time)
		time += 24*3600;
	oldtime = time;
	return time*20;
}

void machine_check(void)
{
	int	ret;
	int	i;
	int	data = 0;
	u_char epson_machine_id = *(unsigned char *)(0x0a1624/*0x11624*/);
	
	/* PC98_SYSTEM_PARAMETER(0x501) */
	ret = ((*(unsigned char*)0x0a1501/*0x11501*/) & 0x08) >> 3;

	/* wait V-SYNC */
	while (inb(0x60) & 0x20) {}
	while (!(inb(0x60) & 0x20)) {}

	/* ANK 'A' font */
	outb(0xa1, 0x00);
	outb(0xa3, 0x41);

	if (ret & M_NORMAL) {
		/* M_NORMAL, use CG window (all NEC OK)  */
		/* sum */
		for (i = 0; i < 4; i++) {
			data += *((unsigned long*)0x0a4000/*0x14000*/ + i);/* 0xa4000 */
		}
		if (data == 0x6efc58fc) { /* DA data */
			ret |= M_NEC_PC98;
		} else {
			ret |= M_EPSON_PC98;
		}
		ret |= (inb(0x42) & 0x20) ? M_8M : 0;
	} else {
		/* M_HIGHRESO, use CG window */
		/* sum */
		for (i = 0; i < 12; i++) {
			data += *((unsigned long*)0x0e4000/*0x54000*/ + i); /* 0xe4000 */
		}
		if ( data == 0x50154624) { /* XA data */
			ret |= M_NEC_PC98;
		} else {
			ret |= M_EPSON_PC98;
		}
		ret |= (inb(0x63) & 0x01) ? M_8M : 0;
	}

	/* PC98_SYSTEM_PARAMETER(0x400) */
	if ((*(unsigned char*)0xa1400/*0x11400*/) & 0x80) {
		ret |= M_NOTE;
	}
	if (ret & M_NEC_PC98) {
		/* PC98_SYSTEM_PARAMETER(0x458) */
		if ((*(unsigned char*)0x0a1458/*0x11458*/) & 0x80) {
			ret |= M_H98;
		} else {
			ret |= M_NOT_H98;
		}
	} else {
		ret |= M_NOT_H98;
		switch (epson_machine_id) {
		case 0x20:	/* note A */
		case 0x22:	/* note W */
		case 0x27:	/* note AE */
		case 0x2a:	/* note WR */
		/*case 0x2:	/* note AR */
			ret |= M_NOTE;
			break;
		default:
			    break;
		}
	}
	(*(unsigned long *)(0x0a1620/*0x11620*/)) = ret;
}
#endif
