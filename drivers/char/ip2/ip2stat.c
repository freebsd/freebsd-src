/*******************************************************************************
*
*   (c) 1998 by Computone Corporation
*
********************************************************************************
*
*
*   PACKAGE:     Linux tty Device Driver for IntelliPort family of multiport
*                serial I/O controllers.
*
*   DESCRIPTION: Status display utility
*
*******************************************************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <linux/timer.h>
#include <linux/termios.h>

#include "i2ellis.h"
#include "i2lib.h"

i2eBordStr Board[2];
i2ChanStr  Port[2];

struct driver_stats
{
	ULONG ref_count;
	ULONG irq_counter;
	ULONG bh_counter;
} Driver;

char	devname[20];

int main (int argc, char *argv[])
{
   int   fd;
   int   dev, i;
	i2eBordStrPtr pB  = Board;
	i2ChanStrPtr  pCh = Port;

	if ( argc != 2 ) 
	{
		printf ( "Usage: %s <port>\n", argv[0] );
		exit(1);
	}
	i = sscanf ( argv[1], "/dev/ttyF%d", &dev );

	if ( i != 1 ) exit(1);

	//printf("%s: board %d, port %d\n", argv[1], dev / 64, dev % 64 );

	sprintf ( devname, "/dev/ip2stat%d", dev / 64 );
	if( 0 > ( fd = open ( devname, O_RDONLY ) ) ) {
		// Conventional name failed - try devfs name
		sprintf ( devname, "/dev/ip2/stat%d", dev / 64 );
		if( 0 > ( fd = open ( devname, O_RDONLY ) ) ) {
			// Where is our board???
			printf( "Unable to open board %d to retrieve stats\n",
				dev / 64 );
			exit( 255 );
		}
	}

	ioctl ( fd,  64, &Driver );
	ioctl ( fd,  65, Board );
	ioctl ( fd,  dev % 64, Port );

	printf ( "Driver statistics:-\n" );
	printf ( " Reference Count:  %d\n", Driver.ref_count );
	printf ( " Interrupts to date:   %ld\n", Driver.irq_counter );
	printf ( " Bottom half to date:  %ld\n", Driver.bh_counter );

	printf ( "Board statistics(%d):-\n",dev/64 );
	printf ( "FIFO: remains = %d%s\n", pB->i2eFifoRemains, 
				pB->i2eWaitingForEmptyFifo ? ", busy" : "" );
	printf ( "Mail: out mail = %02x\n", pB->i2eOutMailWaiting ); 
	printf ( "  Input interrupts : %d\n", pB->i2eFifoInInts );
	printf ( "  Output interrupts: %d\n", pB->i2eFifoOutInts );
	printf ( "  Flow queued      : %ld\n", pB->debugFlowQueued );
	printf ( "  Bypass queued    : %ld\n", pB->debugBypassQueued );
	printf ( "  Inline queued    : %ld\n", pB->debugInlineQueued );
	printf ( "  Data queued      : %ld\n", pB->debugDataQueued );
	printf ( "  Flow packets     : %ld\n", pB->debugFlowCount );
	printf ( "  Bypass packets   : %ld\n", pB->debugBypassCount );
	printf ( "  Inline packets   : %ld\n", pB->debugInlineCount );
	printf ( "  Mail status      : %x\n",  pB->i2eStatus );
	printf ( "  Output mail      : %x\n",  pB->i2eOutMailWaiting );
	printf ( "  Fatal flag       : %d\n",  pB->i2eFatal );

	printf ( "Channel statistics(%s:%d):-\n",argv[1],dev%64 );
	printf ( "ibuf: stuff = %d strip = %d\n", pCh->Ibuf_stuff, pCh->Ibuf_strip );
	printf ( "obuf: stuff = %d strip = %d\n", pCh->Obuf_stuff, pCh->Obuf_strip );
	printf ( "pbuf: stuff = %d\n", pCh->Pbuf_stuff );
	printf ( "cbuf: stuff = %d strip = %d\n", pCh->Cbuf_stuff, pCh->Cbuf_strip );
	printf ( "infl: count = %d room = %d\n", pCh->infl.asof, pCh->infl.room );
	printf ( "outfl: count = %d room = %d\n", pCh->outfl.asof, pCh->outfl.room );
	printf ( "throttled = %d ",pCh->throttled);
	printf ( "bookmarks = %d ",pCh->bookMarks);
	printf ( "flush_flags = %x\n",pCh->flush_flags);
	printf ( "needs: ");
	if (pCh->channelNeeds & NEED_FLOW)   printf("FLOW ");
	if (pCh->channelNeeds & NEED_INLINE) printf("INLINE ");
	if (pCh->channelNeeds & NEED_BYPASS) printf("BYPASS ");
	if (pCh->channelNeeds & NEED_CREDIT) printf("CREDIT ");
	printf ( "\n");
	printf ( "dss: in = %x, out = %x\n",pCh->dataSetIn,pCh->dataSetOut);
	
}
