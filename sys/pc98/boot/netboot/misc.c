/**************************************************************************
MISC Support Routines
**************************************************************************/

#include "netboot.h"

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
