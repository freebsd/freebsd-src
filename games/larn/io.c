/* io.c			 Larn is copyrighted 1986 by Noah Morgan.
 * $FreeBSD: src/games/larn/io.c,v 1.7 1999/11/16 02:57:22 billf Exp $
 *
 *	Below are the functions in this file:
 *
 *	setupvt100() 	Subroutine to set up terminal in correct mode for game
 *	clearvt100()  	Subroutine to clean up terminal when the game is over
 *	getchar() 		Routine to read in one character from the terminal
 *	scbr()			Function to set cbreak -echo for the terminal
 *	sncbr()			Function to set -cbreak echo for the terminal
 *	newgame() 		Subroutine to save the initial time and seed rnd()
 *
 *	FILE OUTPUT ROUTINES
 *
 *	lprintf(format,args . . .)	printf to the output buffer
 *	lprint(integer)				send binary integer to output buffer
 *	lwrite(buf,len)				write a buffer to the output buffer
 *	lprcat(str)					sent string to output buffer
 *
 *	FILE OUTPUT MACROS (in header.h)
 *
 *	lprc(character)				put the character into the output buffer
 *
 *	FILE INPUT ROUTINES
 *
 *	long lgetc()				read one character from input buffer
 *	long lrint()				read one integer from input buffer
 *	lrfill(address,number)		put input bytes into a buffer
 *	char *lgetw()				get a whitespace ended word from input
 *	char *lgetl()				get a \n or EOF ended line from input
 *
 *	FILE OPEN / CLOSE ROUTINES
 *
 *	lcreat(filename)			create a new file for write
 *	lopen(filename)				open a file for read
 *	lappend(filename)			open for append to an existing file
 *	lrclose()					close the input file
 *	lwclose()					close output file
 *	lflush()					flush the output buffer
 *
 *	Other Routines
 *
 *	cursor(x,y)					position cursor at [x,y]
 *	cursors()					position cursor at [1,24] (saves memory)
 *  cl_line(x,y)         		Clear line at [1,y] and leave cursor at [x,y]
 *  cl_up(x,y)    				Clear screen from [x,1] to current line.
 *  cl_dn(x,y) 					Clear screen from [1,y] to end of display.
 *  standout(str)	 			Print the string in standout mode.
 *  set_score_output() 			Called when output should be literally printed.
 ** putchar(ch)					Print one character in decoded output buffer.
 ** flush_buf()					Flush buffer with decoded output.
 ** init_term()					Terminal initialization -- setup termcap info
 **	char *tmcapcnv(sd,ss)  		Routine to convert VT100 \33's to termcap format
 *	beep()		Routine to emit a beep if enabled (see no-beep in .larnopts)
 *
 * Note: ** entries are available only in termcap mode.
 */

#include "header.h"

#ifdef SYSV	/* system III or system V */
#include <termio.h>
#define sgttyb termio
#define stty(_a,_b) ioctl(_a,TCSETA,_b)
#define gtty(_a,_b) ioctl(_a,TCGETA,_b)
static int rawflg = 0;
static char saveeof,saveeol;
#define doraw(_a) if(!rawflg){++rawflg;saveeof=_a.c_cc[VMIN];saveeol=_a.c_cc[VTIME];}\
    _a.c_cc[VMIN]=1;_a.c_cc[VTIME]=1;_a.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL)
#define unraw(_a) _a.c_cc[VMIN]=saveeof;_a.c_cc[VTIME]=saveeol;_a.c_lflag |= ICANON|ECHO|ECHOE|ECHOK|ECHONL

#else not SYSV

#ifndef BSD
#define CBREAK RAW		/* V7 has no CBREAK */
#endif

#define doraw(_a) (_a.sg_flags |= CBREAK,_a.sg_flags &= ~ECHO)
#define unraw(_a) (_a.sg_flags &= ~CBREAK,_a.sg_flags |= ECHO)
#include <sgtty.h>
#endif not SYSV

#ifndef NOVARARGS	/* if we have varargs */
#include <varargs.h>
#else NOVARARGS	/* if we don't have varargs */
typedef char *va_list;
#define va_dcl int va_alist;
#define va_start(plist) plist = (char *) &va_alist
#define va_end(plist)
#define va_arg(plist,mode) ((mode *)(plist += sizeof(mode)))[-1]
#endif NOVARARGS

#define LINBUFSIZE 128		/* size of the lgetw() and lgetl() buffer		*/
int lfd;					/*  output file numbers							*/
int fd;						/*  input file numbers							*/
static struct sgttyb ttx;	/* storage for the tty modes					*/
static int ipoint=MAXIBUF,iepoint=MAXIBUF;	/*  input buffering pointers    */
static char lgetwbuf[LINBUFSIZE];	/* get line (word) buffer				*/

/*
 *	setupvt100() 		Subroutine to set up terminal in correct mode for game
 *
 *	Attributes off, clear screen, set scrolling region, set tty mode
 */
setupvt100()
	{
	clear();  setscroll();  scbr(); /* system("stty cbreak -echo"); */
	}

/*
 *	clearvt100() 	 	Subroutine to clean up terminal when the game is over
 *
 *	Attributes off, clear screen, unset scrolling region, restore tty mode
 */
clearvt100()
	{
	resetscroll();  clear();  sncbr(); /* system("stty -cbreak echo"); */
	}

/*
 *	getchar() 		Routine to read in one character from the terminal
 */
getchar()
	{
	char byt;
#ifdef EXTRA
	c[BYTESIN]++;
#endif
	lflush();		/* be sure output buffer is flushed */
	read(0,&byt,1); 	/* get byte from terminal */
	return(byt);
	}

/*
 *	scbr()		Function to set cbreak -echo for the terminal
 *
 *	like: system("stty cbreak -echo")
 */
scbr()
	{
	gtty(0,&ttx);		doraw(ttx);		stty(0,&ttx);
	}

/*
 *	sncbr()		Function to set -cbreak echo for the terminal
 *
 *	like: system("stty -cbreak echo")
 */
sncbr()
	{
	gtty(0,&ttx);		unraw(ttx);		stty(0,&ttx);
	}

/*
 *	newgame() 		Subroutine to save the initial time and seed rnd()
 */
newgame()
	{
	long *p,*pe;
	for (p=c,pe=c+100; p<pe; *p++ =0);
	time(&initialtime);             srandomdev();
	lcreat((char*)0);	/* open buffering for output to terminal */
	}

/*
 *	lprintf(format,args . . .)		printf to the output buffer
 *		char *format;
 *		??? args . . .
 *
 *	Enter with the format string in "format", as per printf() usage
 *		and any needed arguments following it
 *	Note: lprintf() only supports %s, %c and %d, with width modifier and left
 *		or right justification.
 *	No correct checking for output buffer overflow is done, but flushes
 *		are done beforehand if needed.
 *	Returns nothing of value.
 */
#ifdef lint
/*VARARGS*/
lprintf(str)
	char *str;
	{
	char *str2;
	str2 = str;
	str = str2; /* to make lint happy */
	}
/*VARARGS*/
sprintf(str)
	char *str;
	{
	char *str2;
	str2 = str;
	str = str2; /* to make lint happy */
	}
#else lint
/*VARARGS*/
lprintf(va_alist)
va_dcl
    {
	va_list ap;	/* pointer for variable argument list */
	char *fmt;
	char *outb,*tmpb;
	long wide,left,cont,n;		/* data for lprintf	*/
	char db[12];			/* %d buffer in lprintf	*/

	va_start(ap);	/* initialize the var args pointer */
	fmt = va_arg(ap, char *);	/* pointer to format string */
	if (lpnt >= lpend) lflush();
	outb = lpnt;
	for ( ; ; )
		{
		while (*fmt != '%')
			if (*fmt) *outb++ = *fmt++;  else { lpnt=outb;  return; }
		wide = 0;	left = 1;	cont=1;
		while (cont)
		  switch(*(++fmt))
			{
			case 'd':	n = va_arg(ap, long);
						if (n<0) { n = -n;  *outb++ = '-';  if (wide) --wide; }
						tmpb = db+11;	*tmpb = (char)(n % 10 + '0');
						while (n>9)  *(--tmpb) = (char)((n /= 10) % 10 + '0');
						if (wide==0)  while (tmpb < db+12) *outb++ = *tmpb++;
						else
							{
							wide -= db-tmpb+12;
							if (left)  while (wide-- > 0) *outb++ = ' ';
							while (tmpb < db+12) *outb++ = *tmpb++;
							if (left==0)  while (wide-- > 0) *outb++ = ' ';
							}
						cont=0;	break;

			case 's':	tmpb = va_arg(ap, char *);
						if (wide==0)  { while (*outb++ = *tmpb++);  --outb; }
						else
							{
							n = wide - strlen(tmpb);
							if (left)  while (n-- > 0) *outb++ = ' ';
							while (*outb++ = *tmpb++);  --outb;
							if (left==0)  while (n-- > 0) *outb++ = ' ';
							}
						cont=0;	break;

			case 'c':	*outb++ = va_arg(ap, int);	cont=0;  break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':	wide = 10*wide + *fmt - '0';	break;

			case '-':	left = 0;	break;

			default:	*outb++ = *fmt;  cont=0;	break;
			};
		fmt++;
		}
	va_end(ap);
	}
#endif lint

/*
 *	lprint(long-integer)				send binary integer to output buffer
 *		long integer;
 *
 *		+---------+---------+---------+---------+
 *		|	high  |			|		  |	  low	|
 *		|  order  |			|		  |  order	|
 *		|   byte  |			|		  |	  byte	|
 *		+---------+---------+---------+---------+
 *	   31  ---  24 23 --- 16 15 ---  8 7  ---   0
 *
 *	The save order is low order first, to high order (4 bytes total)
 *		and is written to be system independent.
 *	No checking for output buffer overflow is done, but flushes if needed!
 *	Returns nothing of value.
 */
lprint(x)
	long x;
	{
	if (lpnt >= lpend) lflush();
	*lpnt++ =  255 & x;			*lpnt++ =  255 & (x>>8);
	*lpnt++ =  255 & (x>>16);	*lpnt++ =  255 & (x>>24);
	}

/*
 *	lwrite(buf,len)					write a buffer to the output buffer
 *		char *buf;
 *		int len;
 *
 *	Enter with the address and number of bytes to write out
 *	Returns nothing of value
 */
lwrite(buf,len)
    char *buf;
    int len;
    {
	char *str;
	int num2;
	if (len > 399)  /* don't copy data if can just write it */
		{
#ifdef EXTRA
		c[BYTESOUT] += len;
#endif

#ifndef VT100
		for (str=buf;  len>0; --len)
			lprc(*str++);
#else VT100
		lflush();
		write(lfd,buf,len);
#endif VT100
		}
	else while (len)
		{
		if (lpnt >= lpend) lflush();	/* if buffer is full flush it	*/
		num2 = lpbuf+BUFBIG-lpnt;	/*	# bytes left in output buffer	*/
		if (num2 > len) num2=len;
		str = lpnt;  len -= num2;
		while (num2--)  *str++ = *buf++;	/* copy in the bytes */
		lpnt = str;
		}
    }

/*
 *	long lgetc()				Read one character from input buffer
 *
 *  Returns 0 if EOF, otherwise the character
 */
long lgetc()
    {
    int i;
    if (ipoint != iepoint)  return(inbuffer[ipoint++]);
    if (iepoint!=MAXIBUF)   return(0);
    if ((i=read(fd,inbuffer,MAXIBUF))<=0)
        {
        if (i!=0)  write(1,"error reading from input file\n",30);
		iepoint = ipoint = 0;		return(0);
        }
    ipoint=1;  iepoint=i;  return(*inbuffer);
    }

/*
 *	long lrint()			Read one integer from input buffer
 *
 *		+---------+---------+---------+---------+
 *		|	high  |			|		  |	  low	|
 *		|  order  |			|		  |  order	|
 *		|   byte  |			|		  |	  byte	|
 *		+---------+---------+---------+---------+
 *	   31  ---  24 23 --- 16 15 ---  8 7  ---   0
 *
 *	The save order is low order first, to high order (4 bytes total)
 *	Returns the int read
 */
long lrint()
	{
	unsigned long i;
	i  = 255 & lgetc();				i |= (255 & lgetc()) << 8;
	i |= (255 & lgetc()) << 16;		i |= (255 & lgetc()) << 24;
	return(i);
	}

/*
 *	lrfill(address,number)			put input bytes into a buffer
 *		char *address;
 *		int number;
 *
 *	Reads "number" bytes into the buffer pointed to by "address".
 *	Returns nothing of value
 */
lrfill(adr,num)
	char *adr;
	int num;
	{
	char *pnt;
	int num2;
	while (num)
		{
		if (iepoint == ipoint)
			{
			if (num>5) /* fast way */
				{
				if (read(fd,adr,num) != num)
					write(2,"error reading from input file\n",30);
				num=0;
				}
			else { *adr++ = lgetc();  --num; }
			}
		else
			{
			num2 = iepoint-ipoint;	/*	# of bytes left in the buffer	*/
			if (num2 > num) num2=num;
			pnt = inbuffer+ipoint;	num -= num2;  ipoint += num2;
			while (num2--)  *adr++ = *pnt++;
			}
		}
	}

/*
 *	char *lgetw()			Get a whitespace ended word from input
 *
 *	Returns pointer to a buffer that contains word.  If EOF, returns a NULL
 */
char *lgetw()
	{
	char *lgp,cc;
	int n=LINBUFSIZE,quote=0;
	lgp = lgetwbuf;
	do cc=lgetc();  while ((cc <= 32) && (cc > NULL));  /* eat whitespace */
	for ( ; ; --n,cc=lgetc())
		{
		if ((cc==NULL) && (lgp==lgetwbuf))  return(NULL);	/* EOF */
		if ((n<=1) || ((cc<=32) && (quote==0))) { *lgp=NULL; return(lgetwbuf); }
		if (cc != '"') *lgp++ = cc;   else quote ^= 1;
		}
	}

/*
 *	char *lgetl()		Function to read in a line ended by newline or EOF
 *
 *	Returns pointer to a buffer that contains the line.  If EOF, returns NULL
 */
char *lgetl()
	{
	int i=LINBUFSIZE,ch;
	char *str=lgetwbuf;
	for ( ; ; --i)
		{
		if ((*str++ = ch = lgetc()) == NULL)
			{
			if (str == lgetwbuf+1)  return(NULL); /* EOF */
		ot:	*str = NULL;	return(lgetwbuf);	/* line ended by EOF */
			}
		if ((ch=='\n') || (i<=1))  goto ot; /* line ended by \n */
		}
	}

/*
 *	lcreat(filename)			Create a new file for write
 *		char *filename;
 *
 *	lcreat((char*)0); means to the terminal
 *	Returns -1 if error, otherwise the file descriptor opened.
 */
lcreat(str)
	char *str;
	{
	lpnt = lpbuf;	lpend = lpbuf+BUFBIG;
	if (str==NULL) return(lfd=1);
	if ((lfd=creat(str,0644)) < 0)
		{
		lfd=1; lprintf("error creating file <%s>\n",str); lflush(); return(-1);
		}
	return(lfd);
	}

/*
 *	lopen(filename)			Open a file for read
 *		char *filename;
 *
 *	lopen(0) means from the terminal
 *	Returns -1 if error, otherwise the file descriptor opened.
 */
lopen(str)
	char *str;
	{
	ipoint = iepoint = MAXIBUF;
	if (str==NULL) return(fd=0);
	if ((fd=open(str,0)) < 0)
		{
		lwclose(); lfd=1; lpnt=lpbuf; return(-1);
		}
	return(fd);
	}

/*
 *	lappend(filename)		Open for append to an existing file
 *		char *filename;
 *
 *	lappend(0) means to the terminal
 *	Returns -1 if error, otherwise the file descriptor opened.
 */
lappend(str)
	char *str;
	{
	lpnt = lpbuf;	lpend = lpbuf+BUFBIG;
	if (str==NULL) return(lfd=1);
	if ((lfd=open(str,2)) < 0)
		{
		lfd=1; return(-1);
		}
	lseek(lfd,0,2);	/* seek to end of file */
	return(lfd);
	}

/*
 *	lrclose()						close the input file
 *
 *	Returns nothing of value.
 */
lrclose()
	{
	if (fd > 0) close(fd);
	}

/*
 *	lwclose()						close output file flushing if needed
 *
 *	Returns nothing of value.
 */
lwclose()
	{
	lflush();	if (lfd > 2) close(lfd);
	}

/*
 *	lprcat(string)					append a string to the output buffer
 *								    avoids calls to lprintf (time consuming)
 */
lprcat(str)
    char *str;
    {
	char *str2;
	if (lpnt >= lpend) lflush();
	str2 = lpnt;
	while (*str2++ = *str++);
	lpnt = str2 - 1;
    }

#ifdef VT100
/*
 *	cursor(x,y) 		Subroutine to set the cursor position
 *
 *	x and y are the cursor coordinates, and lpbuff is the output buffer where
 *	escape sequence will be placed.
 */
static char *y_num[]= { "\33[","\33[","\33[2","\33[3","\33[4","\33[5","\33[6",
	"\33[7","\33[8","\33[9","\33[10","\33[11","\33[12","\33[13","\33[14",
	"\33[15","\33[16","\33[17","\33[18","\33[19","\33[20","\33[21","\33[22",
	"\33[23","\33[24" };

static char *x_num[]= { "H","H",";2H",";3H",";4H",";5H",";6H",";7H",";8H",";9H",
	";10H",";11H",";12H",";13H",";14H",";15H",";16H",";17H",";18H",";19H",
	";20H",";21H",";22H",";23H",";24H",";25H",";26H",";27H",";28H",";29H",
	";30H",";31H",";32H",";33H",";34H",";35H",";36H",";37H",";38H",";39H",
	";40H",";41H",";42H",";43H",";44H",";45H",";46H",";47H",";48H",";49H",
	";50H",";51H",";52H",";53H",";54H",";55H",";56H",";57H",";58H",";59H",
	";60H",";61H",";62H",";63H",";64H",";65H",";66H",";67H",";68H",";69H",
	";70H",";71H",";72H",";73H",";74H",";75H",";76H",";77H",";78H",";79H",
	";80H" };

cursor(x,y)
	int x,y;
	{
	char *p;
	if (lpnt >= lpend) lflush();

	p = y_num[y];	/* get the string to print */
	while (*p) *lpnt++ = *p++;	/* print the string */

	p = x_num[x];	/* get the string to print */
	while (*p) *lpnt++ = *p++;	/* print the string */
	}
#else VT100
/*
 * cursor(x,y)	  Put cursor at specified coordinates staring at [1,1] (termcap)
 */
cursor (x,y)
	int x,y;
	{
	if (lpnt >= lpend) lflush ();

	*lpnt++ = CURSOR;		*lpnt++ = x;		*lpnt++ = y;
	}
#endif VT100

/*
 *	Routine to position cursor at beginning of 24th line
 */
cursors()
	{
	cursor(1,24);
	}

#ifndef VT100
/*
 * Warning: ringing the bell is control code 7. Don't use in defines.
 * Don't change the order of these defines.
 * Also used in helpfiles. Codes used in helpfiles should be \E[1 to \E[7 with
 * obvious meanings.
 */

static char cap[256];
char *CM, *CE, *CD, *CL, *SO, *SE, *AL, *DL;/* Termcap capabilities */
static char *outbuf=0;	/* translated output buffer */

int putchar ();

/*
 * init_term()		Terminal initialization -- setup termcap info
 */
init_term()
	{
	char termbuf[1024];
	char *capptr = cap+10;
	char *term;
	struct sgttyb tt;

	switch (tgetent(termbuf, term = getenv("TERM")))
		{
		case -1:
			write(2, "Cannot open termcap file.\n", 26); exit(1);
		case 0:
			write(2, "Cannot find entry of ", 21);
			write(2, term, strlen (term));
			write(2, " in termcap\n", 12);
			exit(1);
		};

	if (gtty(0, &tt) == 0)
		/* ospeed = tt.sg_ospeed */ ;

	CM = tgetstr("cm", &capptr);  /* Cursor motion */
	CE = tgetstr("ce", &capptr);  /* Clear to eoln */
	CL = tgetstr("cl", &capptr);  /* Clear screen */

/* OPTIONAL */
	AL = tgetstr("al", &capptr);  /* Insert line */
	DL = tgetstr("dl", &capptr);  /* Delete line */
	SO = tgetstr("so", &capptr);  /* Begin standout mode */
	SE = tgetstr("se", &capptr);  /* End standout mode */
	CD = tgetstr("cd", &capptr);  /* Clear to end of display */

	if (!CM)	/* can't find cursor motion entry */
		{
		write(2, "Sorry, for a ",13);		write(2, term, strlen(term));
		write(2, ", I can't find the cursor motion entry in termcap\n",50);
		exit(1);
		}
	if (!CE)	/* can't find clear to end of line entry */
		{
		write(2, "Sorry, for a ",13);		write(2, term, strlen(term));
		write(2,", I can't find the clear to end of line entry in termcap\n",57);
		exit(1);
		}
	if (!CL)	/* can't find clear entire screen entry */
		{
		write(2, "Sorry, for a ",13);		write(2, term, strlen(term));
		write(2, ", I can't find the clear entire screen entry in termcap\n",56);
		exit(1);
		}
	if ((outbuf=malloc(BUFBIG+16))==0) /* get memory for decoded output buffer*/
		{
		write(2,"Error malloc'ing memory for decoded output buffer\n",50);
		died(-285);	/* malloc() failure */
		}
	}
#endif VT100

/*
 * cl_line(x,y)  Clear the whole line indicated by 'y' and leave cursor at [x,y]
 */
cl_line(x,y)
	int x,y;
	{
#ifdef VT100
	cursor(x,y);		lprcat("\33[2K");
#else VT100
	cursor(1,y);		*lpnt++ = CL_LINE;		cursor(x,y);
#endif VT100
	}

/*
 * cl_up(x,y) Clear screen from [x,1] to current position. Leave cursor at [x,y]
 */
cl_up(x,y)
	int x,y;
	{
#ifdef VT100
	cursor(x,y);  lprcat("\33[1J\33[2K");
#else VT100
	int i;
	cursor(1,1);
	for (i=1; i<=y; i++)   { *lpnt++ = CL_LINE;  *lpnt++ = '\n'; }
	cursor(x,y);
#endif VT100
	}

/*
 * cl_dn(x,y) 	Clear screen from [1,y] to end of display. Leave cursor at [x,y]
 */
cl_dn(x,y)
	int x,y;
	{
#ifdef VT100
	cursor(x,y); lprcat("\33[J\33[2K");
#else VT100
	int i;
	cursor(1,y);
	if (!CD)
		{
		*lpnt++ = CL_LINE;
		for (i=y; i<=24; i++) { *lpnt++ = CL_LINE;  if (i!=24) *lpnt++ = '\n'; }
		cursor(x,y);
		}
	else
		*lpnt++ = CL_DOWN;
	cursor(x,y);
#endif VT100
	}

/*
 * standout(str)	Print the argument string in inverse video (standout mode).
 */
standout(str)
	char *str;
	{
#ifdef VT100
	setbold();
	while (*str)
		*lpnt++ = *str++;
	resetbold();
#else VT100
	*lpnt++ = ST_START;
	while (*str)
		*lpnt++ = *str++;
	*lpnt++ = ST_END;
#endif VT100
	}

/*
 * set_score_output() 	Called when output should be literally printed.
 */
set_score_output()
	{
	enable_scroll = -1;
	}

/*
 *	lflush()						Flush the output buffer
 *
 *	Returns nothing of value.
 *	for termcap version: Flush output in output buffer according to output
 *							status as indicated by `enable_scroll'
 */
#ifndef VT100
static int scrline=18; /* line # for wraparound instead of scrolling if no DL */
lflush ()
	{
	int lpoint;
	char *str;
	static int curx = 0;
	static int cury = 0;

	if ((lpoint = lpnt - lpbuf) > 0)
		{
#ifdef EXTRA
		c[BYTESOUT] += lpoint;
#endif
		if (enable_scroll <= -1)
			{
			flush_buf();
        	if (write(lfd,lpbuf,lpoint) != lpoint)
				write(2,"error writing to output file\n",29);
			lpnt = lpbuf;	/* point back to beginning of buffer */
			return;
			}
		for (str = lpbuf; str < lpnt; str++)
			{
			if (*str>=32)	{ putchar (*str); curx++; }
			else switch (*str)
				{
				case CLEAR:		tputs (CL, 0, putchar);		curx = cury = 0;
								break;

				case CL_LINE:	tputs (CE, 0, putchar);
								break;

				case CL_DOWN:	tputs (CD, 0, putchar);
								break;

				case ST_START:	tputs (SO, 0, putchar);
								break;

				case ST_END:	tputs (SE, 0, putchar);
								break;

				case CURSOR:	curx = *++str - 1;		cury = *++str - 1;
								tputs (tgoto (CM, curx, cury), 0, putchar);
								break;

				case '\n':		if ((cury == 23) && enable_scroll)
								  {
								  if (!DL || !AL) /* wraparound or scroll? */
									{
									if (++scrline > 23) scrline=19;

									if (++scrline > 23) scrline=19;
									tputs (tgoto (CM, 0, scrline), 0, putchar);
									tputs (CE, 0, putchar);

									if (--scrline < 19) scrline=23;
									tputs (tgoto (CM, 0, scrline), 0, putchar);
									tputs (CE, 0, putchar);
									}
								  else
									{
									tputs (tgoto (CM, 0, 19), 0, putchar);
									tputs (DL, 0, putchar);
									tputs (tgoto (CM, 0, 23), 0, putchar);
								/*	tputs (AL, 0, putchar); */
									}
								  }
								else
								  {
								  putchar ('\n');		cury++;
								  }
								curx = 0;
								break;

				default:		putchar (*str); curx++;
				};
			}
		}
	lpnt = lpbuf;
	flush_buf();	/* flush real output buffer now */
	}
#else VT100
/*
 *	lflush()						flush the output buffer
 *
 *	Returns nothing of value.
 */
lflush()
    {
	int lpoint;
	if ((lpoint = lpnt - lpbuf) > 0)
        {
#ifdef EXTRA
		c[BYTESOUT] += lpoint;
#endif
        if (write(lfd,lpbuf,lpoint) != lpoint)
			write(2,"error writing to output file\n",29);
        }
	lpnt = lpbuf;	/* point back to beginning of buffer */
    }
#endif VT100

#ifndef VT100
static int pindex=0;
/*
 * putchar(ch)		Print one character in decoded output buffer.
 */
int putchar(c)
int c;
	{
	outbuf[pindex++] = c;
	if (pindex >= BUFBIG)  flush_buf();
	}

/*
 * flush_buf()			Flush buffer with decoded output.
 */
flush_buf()
	{
	if (pindex) write(lfd, outbuf, pindex);
	pindex = 0;
	}

/*
 *	char *tmcapcnv(sd,ss)  Routine to convert VT100 escapes to termcap format
 *
 *	Processes only the \33[#m sequence (converts . files for termcap use
 */
char *tmcapcnv(sd,ss)
	char *sd,*ss;
	{
	int tmstate=0;	/* 0=normal, 1=\33 2=[ 3=# */
	char tmdigit=0;	/* the # in \33[#m */
	while (*ss)
		{
		switch(tmstate)
			{
			case 0:	if (*ss=='\33')  { tmstate++; break; }
			  ign:  *sd++ = *ss;
			  ign2: tmstate = 0;
					break;
			case 1: if (*ss!='[') goto ign;
					tmstate++;
					break;
			case 2: if (isdigit(*ss)) { tmdigit= *ss-'0'; tmstate++; break; }
					if (*ss == 'm') { *sd++ = ST_END; goto ign2; }
					goto ign;
			case 3: if (*ss == 'm')
						{
						if (tmdigit) *sd++ = ST_START;
							else *sd++ = ST_END;
						goto ign2;
						}
			default: goto ign;
			};
		ss++;
		}
	*sd=0; /* NULL terminator */
	return(sd);
	}
#endif VT100

/*
 *	beep()		Routine to emit a beep if enabled (see no-beep in .larnopts)
 */
beep()
	{
	if (!nobeep) *lpnt++ = '\7';
	}
