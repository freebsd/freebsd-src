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
PRINTF
**************************************************************************/
printf(fmt, data)
	char *fmt;
	int  data;
{
	char *p;
	int *dp = &data;
	while (*fmt) {
		if (*fmt == '%') {	/* switch() uses more space */
			fmt++;
			if (*fmt == 'X')
				printhl(*dp++);
			if (*fmt == 'x')
				printhw(*dp++);
			if (*fmt == 'd')
				printdec(*dp++);
			if (*fmt == 'I')
				printip(*dp++);
			if (*fmt == 'c')
				putchar(*(dp++) & 0x00FF);
			if (*fmt == 's') {
				p = (char *)*dp++;
				while (*p) putchar(*p++);
			}
		} else putchar(*fmt);
		fmt++;
	}
	return(0);
}


/**************************************************************************
PRINTDEC - Print a number in decimal
**************************************************************************/
printdec(n)
	int n;
{
	char buf[16], *p;
	p = buf;
	if (n < 0) {
		putchar('-');
		n = -n;
	}
	do {
		*(p++) = '0' + (n%10);
		n = n/10;
	} while(n);
	while ((--p) >= buf) putchar(*p);
}

char *putdec(p, n)
	char *p;
	int n;
{
	n = n & 0x00FF;
	if (n/100) *(p++) = '0' + (n/100);
	if (n/10) *(p++) = '0' + ((n/10) % 10);
	*(p++) = '0' + (n%10);
	return(p);
}

/**************************************************************************
PRINTDEC - Print a number in decimal
**************************************************************************/
static char hex[]="0123456789ABCDEF";
printhl(h) {
	printhw(h>>16);
	printhw(h);
}
printhw(h) {
	printhb(h>>8);
	printhb(h);
}
printhb(h) {
	putchar(hex[(h>>4)& 0x0F]);
	putchar(hex[h &0x0F]);
}

/**************************************************************************
PRINTIP - Print an IP address in x.x.x.x notation
**************************************************************************/
printip(i)
	unsigned i;
{
	printdec((i>>24) & 0x0FF);
	putchar('.');
	printdec((i>>16) & 0x0FF);
	putchar('.');
	printdec((i>>8) & 0x0FF);
	putchar('.');
	printdec(i & 0x0FF);
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
}

