/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * User utilities
 * --------------
 *
 * Download (pre)processed microcode into Fore Series-200 host adapter
 * Interact with i960 uart on Fore Series-200 host adapter
 *
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <net/if.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
#include <dev/hfa/fore.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>

#include <ctype.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if (defined(BSD) && (BSD >= 199103))
#include <termios.h>
#else
#include <termio.h>
#endif	/* !BSD */
#include <unistd.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

extern u_char pca200e_microcode[];
extern int pca200e_microcode_size;

#ifdef sun
#define	DEV_NAME "/dev/sbus%d"
#endif	/* sun */
#if (defined(BSD) && (BSD >= 199103))
#define	DEV_NAME _PATH_KMEM
#endif	/* BSD */

#define	MAX_CHECK	60

int	comm_mode = 0;
char	*progname;

int	tty;
cc_t	vmin, vtime;
#if (defined(BSD) && (BSD >= 199103))
struct termios sgtty;
#define	TCSETA	TIOCSETA
#define	TCGETA	TIOCGETA
#else
struct termio sgtty;
#endif	/* !BSD */

int	endian = 0;
int	verbose = 0;
int	reset = 0;

char	line[132];
int	lineptr = 0;

Mon960 *Uart;

void
delay(cnt)
	int	cnt;
{
	usleep(cnt);
}

unsigned long
CP_READ ( val )
unsigned long val;
{
	if ( endian )
		return ( ntohl ( val ) );
	else
		return ( val );
}

unsigned long
CP_WRITE ( val )
unsigned long val;
{
	if ( endian )
		return ( htonl ( val ) );
	else
		return ( val );
}

/*
 * Print an error message and exit.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 */
void
error ( msg )
char *msg;
{
	printf ( "%s\n", msg );
	exit (1);
}

/*
 * Get a byte for the uart and if printing, display it.
 *
 * Arguments:
 *	prn				Are we displaying characters
 *
 * Returns:
 *	c				Character from uart
 */
char
getbyte ( prn )
int prn;
{
	int	c;

	while ( ! ( CP_READ(Uart->mon_xmithost) & UART_VALID ) )
		delay(10);

	c = ( CP_READ(Uart->mon_xmithost) & UART_DATAMASK );
	Uart->mon_xmithost = CP_WRITE(UART_READY);

	/*
	 * We need to introduce a delay in here or things tend to hang...
	 */
	delay(10000);

	if ( lineptr >= sizeof(line) )
		lineptr = 0;

	/*
	 * Save character into line
	 */
	line[lineptr++] = c;

	if (verbose) {
		if (isprint(c) || (c == '\n') || (c == '\r'))
			putc(c, stdout);
	}
	return ( c & 0xff );
}

/*
 * Loop getting characters from uart into static string until eol. If printing,
 * display the line retrieved.
 *
 * Arguments:
 *	prn				Are we displaying characters
 *
 * Returns:
 *	none				Line in global string 'line[]'
 */
void
getline ( prn )
int prn;
{
	char	c = '\0';
	int	i = 0;

	while ( c != '>' && c != '\n' && c != '\r' )
	{
		c = getbyte(0);
		if ( ++i >= sizeof(line) )
		{
			if ( prn )
				printf ( "%s", line );
			i = 0;
		}
	}

	/*
	 * Terminate line
	 */
	line[lineptr] = 0;
	lineptr = 0;

}

/*
 * Send a byte to the i960
 *
 * Arguments:
 *	c				Character to send
 *
 * Returns:
 *	none
 */
void
xmit_byte ( c, dn )
unsigned char c;
int dn;
{
	int	val;

	while ( CP_READ(Uart->mon_xmitmon) != UART_READY )
	{
		if ( CP_READ(Uart->mon_xmithost) & UART_VALID )
			getbyte ( 0 );
		if ( !dn ) delay ( 10000 );
	}
	val = ( c | UART_VALID );
	Uart->mon_xmitmon = CP_WRITE( val );
	if ( !dn ) delay ( 10000 );
	if ( CP_READ(Uart->mon_xmithost) & UART_VALID )
		getbyte ( 0 );

}

/*
 * Transmit a line to the i960. Eol must be included as part of text to transmit.
 *
 * Arguments:
 *	line			Character string to transmit
 *	len			len of string. This allows us to include NULL's
 *					in the string/block to be transmitted.
 *
 * Returns:
 *	none
 */
void
xmit_to_i960 ( line, len, dn )
char *line;
int len;
int dn;
{
	int	i;

        for ( i = 0; i < len; i++ )
		xmit_byte ( line[i], dn );
}

/*
 * Send autobaud sequence to i960 monitor
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 */
void
autobaud()
{
	if ( strncmp ( line, "Mon960", 6 ) == 0 )
		xmit_to_i960 ( "\r\n\r\n\r\n\r\n", 8, 0 );
}

/*
 * Reset tty to initial state
 *
 * Arguments:
 *	ret		error code for exit()
 *
 * Returns:
 *	none
 *
 */
void
finish ( ret )
{
	sgtty.c_lflag |= ( ICANON | ECHO );
	sgtty.c_cc[VMIN] = vmin;
	sgtty.c_cc[VTIME] = vtime;
	ioctl ( tty, TCSETA, &sgtty );
	exit ( ret );
}

/*
 * Utility to strip off any leading path information from a filename
 *
 * Arguments:
 *	path		pathname to strip
 *
 * Returns:
 *	fname		striped filename
 *
 */
char *
basename ( path )
	char *path;
{
	char *fname;

	if ( ( fname = strrchr ( path, '/' ) ) != NULL )
		fname++;
	else
		fname = path;

	return ( fname );
}

/*
 * ASCII constants
 */
#define		SOH		001
#define		STX		002
#define		ETX		003
#define		EOT		004
#define		ENQ		005
#define		ACK		006
#define		LF		012
#define		CR		015
#define		NAK		025
#define		SYN		026
#define		CAN		030
#define		ESC		033

#define		NAKMAX		2
#define		ERRORMAX	10
#define		RETRYMAX	5

#define		CRCCHR		'C'
#define		CTRLZ		032

#define		BUFSIZE		128

#define		W		16
#define		B		8

/*
 * crctab - CRC-16 constant array...
 *     from Usenet contribution by Mark G. Mendel, Network Systems Corp.
 *     (ihnp4!umn-cs!hyper!mark)
 */
unsigned short crctab[1<<B] = {
    0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
    0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
    0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
    0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
    0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
    0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
    0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
    0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
    0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
    0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
    0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
    0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
    0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
    0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
    0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
    0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
    0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
    0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
    0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
    0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
    0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
    0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
    0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
    0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
    0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
    0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
    0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
    0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
    0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
    0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
    0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
    0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0
    };

/*
 * Hacked up xmodem protocol. Transmits the file 'filename' down to the i960
 * using the xmodem protocol.
 *
 * Arguments:
 *	filename			name of file to transmit
 *
 * Returns:
 *	0				file transmitted
 *	-1				unable to send file
 */
int
xmitfile ( filename )
char *filename;
{
	int	fd;
	int	numsect;
	int	sectnum;
	struct stat stb;
	char	c;
	char	sendresp;
	int	crcmode = 0;
	int	attempts = 0;
	int	errors;
	int	sendfin;
	int	extrachr;
	char	buf[BUFSIZE + 6];
	char	blockbuf[BUFSIZE + 6];
	int	bufcntr;
	int	bbufcntr;
	int	bufsize = BUFSIZE;
	int	checksum;

	/*
	 * Try opening file
	 */
	if ( ( fd = open ( filename, O_RDONLY ) ) < 0 )
	{
		return -1;
	}
	stat ( filename, &stb );

	/*
	 * Determine number of 128 bytes sectors to transmit
	 */
	numsect = ( stb.st_size / 128 ) + 1;

	if ( verbose )
		fprintf ( stderr, "Downloading %d sectors from %s\n",
			numsect, filename );

	/*
	 * Send DO'wnload' command to i960
	 */
	xmit_to_i960 ( "do\r\n", 4, 0 );
	/*
	 * Wait for response from i960 indicating download in progress
	 */
	while ( strncmp ( line, "Downloading", 11 ) != 0 )
		getline ( verbose );
	

	/*
	 * Get startup character from i960
	 */
	do {
		while ( ( c = getbyte(0) ) != NAK && c != CRCCHR )
			if ( ++attempts > NAKMAX )
				error ( "Remote system not responding" );

		if ( c == CRCCHR )
			crcmode = 1;

	} while ( c != NAK && c != CRCCHR );

	sectnum = 1;
	attempts = errors = sendfin = extrachr = 0;

	/*
	 * Loop over each sector to be sent
	 */
	do {
		if ( extrachr >= 128 )
		{
			extrachr = 0;
			numsect++;
		}

		if ( sectnum > 0 )
		{
			/*
			 * Read a sectors worth of data from the file into
			 * an internal buffer.
			 */
			for ( bufcntr = 0; bufcntr < bufsize; )
			{
				int n;
				/*
				 * Check for EOF
				 */
				if ( ( n = read ( fd, &c, 1 ) ) == 0 )
				{
					sendfin = 1;
					if ( !bufcntr )
						break;
					buf[bufcntr++] = CTRLZ;
					continue;
				}
				buf[bufcntr++] = c;
			}
			if ( !bufcntr )
				break;
		}

		/*
		 * Fill in xmodem protocol values. Block size and sector number
		 */
		bbufcntr = 0;
		blockbuf[bbufcntr++] = (bufsize == 1024) ? STX : SOH;
		blockbuf[bbufcntr++] = sectnum;
		blockbuf[bbufcntr++] = ~sectnum;

		checksum = 0;

		/*
		 * Loop over the internal buffer computing the checksum of the
		 * sector
		 */
		for ( bufcntr = 0; bufcntr < bufsize; bufcntr++ )
		{
			blockbuf[bbufcntr++] = buf[bufcntr];

			if ( crcmode )
				checksum = (checksum<<B) ^ crctab[(checksum>>(W-B)) ^ buf[bufcntr]];
			else
				checksum = ((checksum + buf[bufcntr]) & 0xff);

		}

		/*
		 * Place the checksum at the end of the transmit buffer
		 */
		if ( crcmode )
		{
			checksum &= 0xffff;
			blockbuf[bbufcntr++] = ((checksum >> 8) & 0xff);
			blockbuf[bbufcntr++] = (checksum & 0xff);
		} else
			blockbuf[bbufcntr++] = checksum;

		attempts = 0;

		/*
		 * Make several attempts to send the data to the i960
		 */
		do
		{
			/*
			 * Transmit the sector + protocol to the i960
			 */
			xmit_to_i960 ( blockbuf, bbufcntr, 1 );

			/*
			 * Inform user where we're at
			 */
			if ( verbose )
				printf ( "Sector %3d %3dk\r",
				    sectnum, (sectnum * bufsize) / 1024 );

			attempts++;
			/*
			 * Get response from i960
			 */
			sendresp = getbyte(0);

			/*
			 * If i960 didn't like the sector
			 */
			if ( sendresp != ACK )
			{
				errors++;

				/*
				 * Are we supposed to cancel the transfer?
				 */
				if ( ( sendresp & 0x7f ) == CAN )
					if ( getbyte(0) == CAN )
						error ( "Send canceled at user's request" );
			}

		} while ( ( sendresp != ACK ) && ( attempts < RETRYMAX ) && ( errors < ERRORMAX ) );

		/*
		 * Next sector
		 */
		sectnum++;

	} while ( !sendfin && ( attempts < RETRYMAX ) && ( errors < ERRORMAX ) );

	/*
	 * Did we expire all our allows attempts?
	 */
	if ( attempts >= RETRYMAX )
	{
		xmit_byte ( CAN, 1 ), xmit_byte ( CAN, 1 ), xmit_byte ( CAN, 1 );
		error ( "Remote system not responding" );
	}

	/*
	 * Check for too many transmission errors
	 */
	if ( errors >= ERRORMAX )
	{
		xmit_byte ( CAN, 1 ), xmit_byte ( CAN, 1 ), xmit_byte ( CAN, 1 );
		error ( "Too many errors in transmission" );
	}

	attempts = 0;

	/*
	 * Indicate the transfer is complete
	 */
	xmit_byte ( EOT, 1 );

	/*
	 * Wait until i960 acknowledges us
	 */
	while ( ( c = getbyte(0) ) != ACK && ( ++attempts < RETRYMAX ) )
		xmit_byte ( EOT, 1 );

	if ( attempts >= RETRYMAX )
		error ( "Remote system not responding on completion" );

	/*
	 * After download, we'll see a few more command 
	 * prompts as the CP does its stuff. Ignore them.
	 */
	while ( strncmp ( line, "=>", 2 ) != 0 )
		getline ( verbose );

	while ( strncmp ( line, "=>", 2 ) != 0 )
		getline ( verbose );

	while ( strncmp ( line, "=>", 2 ) != 0 )
		getline ( verbose );

	/*
	 * Tell the i960 to start executing the downloaded code
	 */
	xmit_to_i960 ( "go\r\n", 4, 0 );

	/*
	 * Get the messages the CP will spit out
	 * after the GO command.
	 */
	getline ( verbose );
	getline ( verbose );

	close ( fd );

	return ( 0 );
}


int
loadmicrocode ( ucode, size, ram )
u_char *ucode;
int size;
u_char *ram;
{
	struct {
		u_long	Id;
		u_long	fver;
		u_long	start;
		u_long	entry;
	} binhdr;
#ifdef sun
	union {
		u_long	w;
		char	c[4];
	} w1, w2;
	int	n;
#endif
	u_char	*bufp;
	u_long	*lp;


	/*
	 * Check that we understand this header
	 */
	memcpy(&binhdr, ucode, sizeof(binhdr));
	if ( strncmp ( (caddr_t)&binhdr.Id, "fore", 4 ) != 0 ) {
		fprintf ( stderr, "Unrecognized format in micorcode file." );
		return ( -1 );
	}

#ifdef	sun
	/*
	 * We always swap the SunOS microcode file...
	 */
	endian = 1;

	/*
	 * We need to swap the header start/entry words...
	 */
	w1.w = binhdr.start;
	for ( n = 0; n < sizeof(u_long); n++ )
		w2.c[3-n] = w1.c[n];
	binhdr.start = w2.w;
	w1.w = binhdr.entry;
	for ( n = 0; n < sizeof(u_long); n++ )
		w2.c[3-n] = w1.c[n];
	binhdr.entry = w2.w;
#endif	/* sun */

	/*
	 * Set pointer to RAM load location
	 */
	bufp = (ram + binhdr.start);

	/*
	 * Load file
	 */
	if ( endian ) {
		int	i;

		lp = (u_long *) ucode;
		/* Swap buffer */
		for ( i = 0; i < size / sizeof(long); i++ )
#ifndef	sun
			lp[i] = CP_WRITE(lp[i]);
#else
		{
			int	j;

			w1.w = lp[i];
			for ( j = 0; j < 4; j++ )
				w2.c[3-j] = w1.c[j];
			lp[i] = w2.w;
		}
#endif
	}
	bcopy ( (caddr_t)ucode, bufp, size );

	/*
	 * With .bin extension, we need to specify start address on 'go'
	 * command.
	 */
	{
		char	cmd[80];

		sprintf ( cmd, "go %lx\r\n", binhdr.entry );

		xmit_to_i960 ( cmd, strlen ( cmd ), 0 );

		while ( strncmp ( line, cmd, strlen(cmd) - 3 ) != 0 ) 
			getline ( verbose );

		if ( verbose )
			printf("\n");
	}
	return ( 0 );
}

int
sendbinfile ( fname, ram )
char *fname;
u_char *ram;
{
	struct {
		u_long	Id;
		u_long	fver;
		u_long	start;
		u_long	entry;
	} binhdr;
#ifdef sun
	union {
		u_long	w;
		char	c[4];
	} w1, w2;
#endif
	int	fd;
	int	n;
	int	cnt = 0;
	u_char	*bufp;
	long	buffer[1024];

	/*
	 * Try opening file
	 */
	if ( ( fd = open ( fname, O_RDONLY ) ) < 0 )
		return ( -1 );

	/*
	 * Read the .bin header from the file
	 */
	if ( ( read ( fd, &binhdr, sizeof(binhdr) ) ) != sizeof(binhdr) )
	{
		close ( fd );
		return ( -1 );
	}

	/*
	 * Check that we understand this header
	 */
	if ( strncmp ( (caddr_t)&binhdr.Id, "fore", 4 ) != 0 ) {
		fprintf ( stderr, "Unrecognized format in micorcode file." );
		close ( fd );
		return ( -1 );
	}

#ifdef	sun
	/*
	 * We always swap the SunOS microcode file...
	 */
	endian = 1;

	/*
	 * We need to swap the header start/entry words...
	 */
	w1.w = binhdr.start;
	for ( n = 0; n < sizeof(u_long); n++ )
		w2.c[3-n] = w1.c[n];
	binhdr.start = w2.w;
	w1.w = binhdr.entry;
	for ( n = 0; n < sizeof(u_long); n++ )
		w2.c[3-n] = w1.c[n];
	binhdr.entry = w2.w;
#endif	/* sun */

	/*
	 * Rewind the file
	 */
	lseek ( fd, 0, 0 );

	/*
	 * Set pointer to RAM load location
	 */
	bufp = (ram + binhdr.start);

	/*
	 * Load file
	 */
	if ( endian ) {
		/*
		 * Need to swap longs - copy file into temp buffer
		 */
		while ( ( n = read ( fd, (char *)buffer, sizeof(buffer))) > 0 )
		{
			int	i;

			/* Swap buffer */
			for ( i = 0; i < sizeof(buffer) / sizeof(long); i++ )
#ifndef	sun
				buffer[i] = CP_WRITE(buffer[i]);
#else
			{
				int	j;

				w1.w = buffer[i];
				for ( j = 0; j < 4; j++ )
					w2.c[3-j] = w1.c[j];
				buffer[i] = w2.w;
			}
#endif

			/*
			 * Copy swapped buffer into CP RAM
			 */
			cnt++;
			bcopy ( (caddr_t)buffer, bufp, n );
			if ( verbose )
				printf ( "%d\r", cnt );
			bufp += n;
		}
	} else {
	    while ( ( n = read ( fd, bufp, 128 ) ) > 0 )
	    {
		cnt++;
		if ( verbose )
			printf ( "%d\r", cnt );
		bufp += n;
	    }
	}

	/*
	 * With .bin extension, we need to specify start address on 'go'
	 * command.
	 */
	{
		char	cmd[80];

		sprintf ( cmd, "go %lx\r\n", binhdr.entry );

		xmit_to_i960 ( cmd, strlen ( cmd ), 0 );

		while ( strncmp ( line, cmd, strlen(cmd) - 3 ) != 0 )
			getline ( verbose );

		if ( verbose )
			printf("\n");
	}

	close ( fd );
	return ( 0 );
}


/*
 * Program to download previously processed microcode to series-200 host adapter
 */
int
main( argc, argv )
int argc;
char *argv[];
{
	int	fd;			/* mmap for Uart */
	u_char	*ram;			/* pointer to RAM */
	Mon960	*Mon;			/* Uart */
	Aali	*aap;
	char	c;
	int	i, err;
	int	binary = 0;		/* Send binary file */
	caddr_t	buf;			/* Ioctl buffer */
	char	bus_dev[80];		/* Bus device to mmap on */
	struct atminfreq req;
	struct air_cfg_rsp *air;	/* Config info response structure */
	int	buf_len;		/* Size of ioctl buffer */
	char	*devname = "\0";	/* Device to download */
	char	*dirname = NULL;	/* Directory path to objd files */
	char	*objfile = NULL;	/* Command line object filename */
	u_char	*ucode = NULL;		/* Pointer to microcode */
	int	ucode_size = 0;		/* Length of microcode */
	char	*sndfile = NULL;	/* Object filename to download */
	char	filename[64];		/* Constructed object filename */
	char	base[64];		/* sba200/sba200e/pca200e basename */
	int	ext = 0;		/* 0 == bin 1 == objd */
	struct stat sbuf;		/* Used to find if .bin or .objd */
	extern char *optarg;

	progname = (char *)basename(argv[0]);
	comm_mode = strcmp ( progname, "fore_comm" ) == 0;

	while ( ( c = getopt ( argc, argv, "i:d:f:berv" ) ) != -1 )
	    switch ( c ) {
		case 'b':
			binary++;
			break;
		case 'd':
			dirname = (char *)strdup ( optarg );
			break;
		case 'e':
			endian++;
			break;
		case 'i':
			devname = (char *)strdup ( optarg );
			break;
		case 'f':
			objfile = (char *)strdup ( optarg );
			break;
		case 'v':
			verbose++;
			break;
		case 'r':
			reset++;
			break;
		case '?':
			printf ( "usage: %s [-v] [-i intf] [-d dirname] [-f objfile]\n", argv[0] );
			exit ( 2 );
	    }
	
	/*
	 * Unbuffer stdout
	 */
	setbuf ( stdout, NULL );
		
	if ( ( fd = socket ( AF_ATM, SOCK_DGRAM, 0 ) ) < 0 )
	{
		perror ( "Cannot create ATM socket" );
		exit ( 1 );
	}
	/*
	 * Over allocate memory for returned data. This allows
	 * space for IOCTL reply info as well as config info.
	 */
	buf_len = 4 * sizeof(struct air_cfg_rsp);
	if ( ( buf = (caddr_t)malloc(buf_len) ) == NULL )
	{
		perror ( "Cannot allocate memory for reply" );
		exit ( 1 );
	}
	/*
	 * Fill in request paramaters
	 */
	req.air_opcode = AIOCS_INF_CFG;
	req.air_buf_addr = buf;
	req.air_buf_len = buf_len;

	/*
	 * Copy interface name into ioctl request
	 */
	strcpy ( req.air_cfg_intf, devname );

	/*
	 * Issue ioctl
	 */
	if ( ( ioctl ( fd, AIOCINFO, (caddr_t)&req ) ) ) {
		perror ( "ioctl (AIOCSINFO)" );
		exit ( 1 );
	}
	/*
	 * Reset buffer pointer
	 */
	req.air_buf_addr = buf;

	/*
	 * Close socket
	 */
	close ( fd );

	/*
	 * Loop through all attached adapters
	 */
	for (; req.air_buf_len >= sizeof(struct air_cfg_rsp); 
			buf += sizeof(struct air_cfg_rsp),
			req.air_buf_len -= sizeof(struct air_cfg_rsp)) {

		/*
		 * Point to vendor info
		 */
		air = (struct air_cfg_rsp *)buf;

		if (air->acp_vendapi == VENDAPI_FORE_1 && air->acp_ram != 0)
		{
			/*
			 * Create /dev name
			 */
#ifdef sun
			sprintf ( bus_dev, DEV_NAME, air->acp_busslot );
#else
			sprintf ( bus_dev, DEV_NAME );
#endif

			/*
			 * Setup signal handlers
			 */
			signal ( SIGINT, SIG_IGN );
			signal ( SIGQUIT, SIG_IGN );
		
			/*
			 * If comm_mode, setup terminal for single char I/O
			 */
			if ( comm_mode ) {
				tty = open ( _PATH_TTY, O_RDWR );
				ioctl ( tty, TCGETA, &sgtty );
				sgtty.c_lflag &= ~( ICANON | ECHO );
				vmin = sgtty.c_cc[VMIN];
				vtime = sgtty.c_cc[VTIME];
				sgtty.c_cc[VMIN] = 0;
				sgtty.c_cc[VTIME] = 0;
				ioctl ( tty, TCSETA, &sgtty );
			}

			/*
			 * Open bus for memory access
			 */
			if ( ( fd = open ( bus_dev, O_RDWR ) ) < 0 )
			{
				perror ( "open bus_dev" );
				fprintf(stderr, "%s download failed (%s)\n",
					air->acp_intf, bus_dev);
				continue;
			}

			/*
			 * Map in the RAM memory to get access to the Uart
			 */
#ifdef __FreeBSD__ /*XXX*/
			ram = (u_char *) mmap(0, PCA200E_MMAP_SIZE,
#else
			ram = (u_char *) mmap(0, air->acp_ramsize,
#endif
				PROT_READ | PROT_WRITE, MAP_SHARED | MAP_HASSEMAPHORE,
				fd, air->acp_ram);
			if (ram == (u_char *)-1) {
				perror ( "mmap ram" );
				fprintf(stderr, "%s download failed\n",
					air->acp_intf);
				(void) close(fd);
				continue;
			}
			Mon = (Mon960 *)(ram + MON960_BASE);
			Uart = (Mon960 *)&(Mon->mon_xmitmon);

			/*
			 * Determine endianess
			 */
			switch ( Mon->mon_bstat ) {
			case BOOT_COLDSTART:
			case BOOT_MONREADY:
			case BOOT_FAILTEST:
			case BOOT_RUNNING:
				break;

			default:
				switch (ntohl(Mon->mon_bstat)) {
				case BOOT_COLDSTART:
				case BOOT_MONREADY:
				case BOOT_FAILTEST:
				case BOOT_RUNNING:
					endian++;
					break;

				default:
					fprintf(stderr, "%s unknown status\n",
						air->acp_intf);
					(void) close(fd);
					continue;
				}
				break;
			}

#ifdef __FreeBSD__
			if (reset) {
				u_int	*hcr = (u_int *)(ram + PCA200E_HCR_OFFSET);
				PCA200E_HCR_INIT(*hcr, PCA200E_RESET_BD);
				delay(10000);
				PCA200E_HCR_CLR(*hcr, PCA200E_RESET_BD);
				delay(10000);
			}
#endif

			if ( comm_mode ) {
			    static struct timeval timeout = { 0, 0 };
			    int	esc_seen = 0;

			    /*
			     * We want to talk with the i960 monitor
			     */

			    /*
			     * Loop forever accepting characters
			     */
			    for ( ; ; ) {
				fd_set	fdr;
				int	ns;

				/*
				 * Check for data from the terminal
				 */
				FD_ZERO ( &fdr );
				FD_SET ( fileno(stdin), &fdr );

				if ( ( ns = select ( FD_SETSIZE, &fdr, NULL, NULL,
					&timeout ) ) < 0 ) {
						perror ( "select" );
						finish( -1 );
				}

				if ( ns ) {
					int	c;
					int	nr;

					nr = read ( fileno(stdin), &c, 1 );
					c &= 0xff;
					if ( !esc_seen ) {
					    if ( c == 27 )
						esc_seen++;
					    else
						xmit_byte ( c, 0 );
					} else {
					    if ( c == 27 ) 
						finish( -1 );
					    else {
						xmit_byte ( 27, 0 );
						esc_seen = 0;
					    }
					    xmit_byte ( c, 0 );
					}
				}

				/*
				 * Check for data from the i960
				 */
				if ( CP_READ(Uart->mon_xmithost) & UART_VALID ) {
					c = getbyte(0);
					putchar ( c );
				}
				if ( strcmp ( line, "Mon960" )  == 0 )
					autobaud();

			    }
			} else {
			    /*
			     * Make sure the driver is loaded and that the CP
			     * is ready for commands
			     */
			    if ( CP_READ(Mon->mon_bstat) == BOOT_RUNNING )
			    {
				fprintf ( stderr, 
				"%s is up and running - no download allowed.\n",
					air->acp_intf );
				(void) close(fd);
				continue;
			    }
		
			    if ( CP_READ(Mon->mon_bstat) != BOOT_MONREADY )
			    {
				fprintf ( stderr, 
					"%s is not ready for downloading.\n", 
					air->acp_intf );
				(void) close(fd);
				continue;
			    }
		
			    /*
			     * Indicate who we're downloading
			     */
			    if ( verbose )
				printf ( "Downloading code for %s\n",
					air->acp_intf );

			    /*
			     * Look for the i960 monitor message. 
			     * We should see this after a board reset.
			     */
			    while ( strncmp ( line, "Mon960", 6 ) != 0 && 
				strncmp ( line, "=>", 2 ) != 0 )
				getline( verbose );	/* Verbose */
		
			    /*
			     * Autobaud fakery
			     */
			    if ( strncmp ( line, "Mon960", 6 ) == 0 ) {
				xmit_to_i960 ( "\r\n\r\n\r\n\r\n", 8, 0 );
				delay ( 10000 );
			    }

			    /*
			     * Keep reading until we get a command prompt
			     */
			    while ( strncmp ( line, "=>", 2 ) != 0 )
				getline( verbose );	/* Verbose */

			    /*
			     * Choose the correct microcode file based on the
			     * adapter type the card claims to be.
			     */
			    switch ( air->acp_device )
			    {
			    case DEV_FORE_SBA200:
				sprintf ( base, "sba200" );
				break;

			    case DEV_FORE_SBA200E:
				sprintf ( base, "sba200e" );
				break;

			    case DEV_FORE_PCA200E:
				sprintf ( base, "pca200e" );
				break;
 
			    default:
				err = 1;
				fprintf(stderr, "Unknown adapter type: %d\n", 
					air->acp_device );
			    }

			    sndfile = NULL;

			    if ( objfile == NULL ) {
				switch ( air->acp_device ) {
				case DEV_FORE_SBA200:
				case DEV_FORE_SBA200E:
				    sprintf ( filename, "%s.bin%d", base,
					air->acp_bustype );
				    if ( stat ( filename, &sbuf ) == -1 ) {
					sprintf ( filename, "%s/%s.bin%d",
					    dirname, base,
						air->acp_bustype );
					if ( stat ( filename, &sbuf ) == -1 ) {
					    ext = 1;
					    sprintf ( filename, "%s.objd%d",
						base, air->acp_bustype );
					    if ( stat(filename, &sbuf) == -1 ) {
						sprintf ( filename,
						    "%s/%s.objd%d", dirname,
							base,
							    air->acp_bustype );
						if ( stat ( filename, &sbuf ) != -1 )
						    sndfile = filename;
					    } else
						sndfile = filename;
					} else
					    sndfile = filename;
				    } else
					sndfile = filename;
				    break;
				case DEV_FORE_PCA200E:
				    /* Use compiled in microcode */
				    ucode = pca200e_microcode;
				    ucode_size = pca200e_microcode_size;
				    break;
				default:
				    break;
			        }
			    } else
				sndfile = objfile;

			    if ( ext && !binary )
				err = xmitfile ( sndfile );
			    else if (sndfile != NULL) 
				err = sendbinfile ( sndfile, ram );
			    else 
				err = loadmicrocode( ucode, ucode_size, ram );

			    if ( err ) {
				fprintf(stderr, "%s download failed\n",
					air->acp_intf);
				(void) close(fd);
				continue;
			    }

			    /*
			     * Download completed - wait around a while for
			     * the driver to initialize the adapter
			     */
			     aap = (Aali *)(ram + CP_READ(Mon->mon_appl));
			     for (i = 0; i < MAX_CHECK; i++, sleep(1)) {
				u_long	hb1, hb2, hb3;

				hb3 = CP_READ(Mon->mon_bstat);
				if (hb3 != BOOT_RUNNING) {
					if (verbose)
						printf("bstat %lx\n", hb3);
					continue;
				}

				hb1 = CP_READ(aap->aali_heartbeat);
				delay(1);
				hb2 = CP_READ(aap->aali_heartbeat);
				if (verbose)
					printf("hb %lx %lx\n", hb1, hb2);
				if (hb1 < hb2)
					break;
			     }
			}

			close ( fd );
		}
	}

	/*
	 * Exit
	 */
	exit (0);

}

