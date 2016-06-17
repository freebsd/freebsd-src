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
*   DESCRIPTION: Interpretive trace dump utility
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
#include "ip2trace.h"

unsigned long namebuf[100];

struct { 
	int wrap,
	size,
	o_strip,
	o_stuff,
	strip,
	stuff;
	unsigned long buf[1000];
} tbuf;

struct sigaction act;

typedef enum { kChar, kInt, kAddr, kHex } eFormat;

int active = 1;
void quit() { active = 0; }

int main (int argc, char *argv[])
{
   int   fd = open ( "/dev/ip2trace", O_RDONLY );
   int   cnt, i;
	unsigned long ts, td;
   struct timeval timeout;
   union ip2breadcrumb bc;
	eFormat fmt = kHex;

   if ( fd < 0 )
   {
      printf ( "Can't open device /dev/ip2trace\n" );
      exit ( 1 );
   }

   act.sa_handler = quit;
   /*act.sa_mask = 0;*/
	sigemptyset(&act.sa_mask);
   act.sa_flags = 0;
   act.sa_restorer = NULL;

   sigaction ( SIGTERM, &act, NULL );

	ioctl ( fd,  1, namebuf );

	printf ( "iiSendPendingMail %p\n",        namebuf[0] );
	printf ( "i2InitChannels %p\n",           namebuf[1] );
	printf ( "i2QueueNeeds %p\n",             namebuf[2] );
	printf ( "i2QueueCommands %p\n",          namebuf[3] );
	printf ( "i2GetStatus %p\n",              namebuf[4] );
	printf ( "i2Input %p\n",                  namebuf[5] );
	printf ( "i2InputFlush %p\n",             namebuf[6] );
	printf ( "i2Output %p\n",                 namebuf[7] );
	printf ( "i2FlushOutput %p\n",            namebuf[8] );
	printf ( "i2DrainWakeup %p\n",            namebuf[9] );
	printf ( "i2DrainOutput %p\n",            namebuf[10] );
	printf ( "i2OutputFree %p\n",             namebuf[11] );
	printf ( "i2StripFifo %p\n",              namebuf[12] );
	printf ( "i2StuffFifoBypass %p\n",        namebuf[13] );
	printf ( "i2StuffFifoFlow %p\n",          namebuf[14] );
	printf ( "i2StuffFifoInline %p\n",        namebuf[15] );
	printf ( "i2ServiceBoard %p\n",           namebuf[16] );
	printf ( "serviceOutgoingFifo %p\n",      namebuf[17] );
	printf ( "ip2_init %p\n",                 namebuf[18] ); 
	printf ( "ip2_init_board %p\n",           namebuf[19] ); 
	printf ( "find_eisa_board %p\n",          namebuf[20] );  
	printf ( "set_irq %p\n",                  namebuf[21] );  
	printf ( "ex_details %p\n",               namebuf[22] );  
	printf ( "ip2_interrupt %p\n",            namebuf[23] );  
	printf ( "ip2_poll %p\n",                 namebuf[24] );  
	printf ( "service_all_boards %p\n",        namebuf[25] );  
	printf ( "do_input %p\n",                 namebuf[27] );  
	printf ( "do_status %p\n",                namebuf[26] );  
	printf ( "open_sanity_check %p\n",        namebuf[27] );  
	printf ( "open_block_til_ready %p\n",     namebuf[28] );   
	printf ( "ip2_open %p\n",                 namebuf[29] );  
	printf ( "ip2_close %p\n",                namebuf[30] );  
	printf ( "ip2_hangup %p\n",               namebuf[31] );  
	printf ( "ip2_write %p\n",                namebuf[32] );  
	printf ( "ip2_putchar %p\n",              namebuf[33] );  
	printf ( "ip2_flush_chars %p\n",          namebuf[34] );  
	printf ( "ip2_write_room %p\n",           namebuf[35] );  
	printf ( "ip2_chars_in_buf %p\n",         namebuf[36] );  
	printf ( "ip2_flush_buffer %p\n",         namebuf[37] );  
	//printf ( "ip2_wait_until_sent %p\n",      namebuf[38] );  
	printf ( "ip2_throttle %p\n",             namebuf[39] );  
	printf ( "ip2_unthrottle %p\n",           namebuf[40] );  
	printf ( "ip2_ioctl %p\n",                namebuf[41] );  
	printf ( "get_modem_info %p\n",           namebuf[42] );  
	printf ( "set_modem_info %p\n",           namebuf[43] );  
	printf ( "get_serial_info %p\n",          namebuf[44] );  
	printf ( "set_serial_info %p\n",          namebuf[45] );  
	printf ( "ip2_set_termios %p\n",          namebuf[46] );  
	printf ( "ip2_set_line_discipline %p\n",  namebuf[47] );  
	printf ( "set_line_characteristics %p\n", namebuf[48] );  

	printf("\n-------------------------\n");
	printf("Start of trace\n");

   while ( active ) {
      cnt = read ( fd, &tbuf, sizeof tbuf );

      if ( cnt ) {
         if ( tbuf.wrap ) {
            printf ( "\nTrace buffer: wrap=%d, strip=%d, stuff=%d\n",
                     tbuf.wrap, tbuf.strip, tbuf.stuff );
         }
         for ( i = 0, bc.value = 0; i < cnt; ++i ) {
				if ( !bc.hdr.codes ) {
					td = tbuf.buf[i] - ts;
					ts = tbuf.buf[i++];
					bc.value = tbuf.buf[i];
	
					printf ( "\n(%d) Port %3d ", ts, bc.hdr.port );

					fmt = kHex;

					switch ( bc.hdr.cat )
					{
					case ITRC_INIT:
						printf ( "Init       %d: ", bc.hdr.label );
						break;

					case ITRC_OPEN:
						printf ( "Open       %d: ", bc.hdr.label );
						break;

					case ITRC_CLOSE:
						printf ( "Close      %d: ", bc.hdr.label );
						break;

					case ITRC_DRAIN:
						printf ( "Drain      %d: ", bc.hdr.label );
						fmt = kInt;
						break;

					case ITRC_IOCTL:
						printf ( "Ioctl      %d: ", bc.hdr.label );
						break;

					case ITRC_FLUSH:
						printf ( "Flush      %d: ", bc.hdr.label );
						break;

					case ITRC_STATUS:
						printf ( "GetS       %d: ", bc.hdr.label );
						break;

					case ITRC_HANGUP:
						printf ( "Hangup     %d: ", bc.hdr.label );
						break;

					case ITRC_INTR:
						printf ( "*Intr      %d: ", bc.hdr.label );
						break;

					case ITRC_SFLOW:
						printf ( "SFlow      %d: ", bc.hdr.label );
						fmt = kInt;
						break;

					case ITRC_SBCMD:
						printf ( "Bypass CMD %d: ", bc.hdr.label );
						fmt = kInt;
						break;

					case ITRC_SICMD:
						printf ( "Inline CMD %d: ", bc.hdr.label );
						fmt = kInt;
						break;

					case ITRC_MODEM:
						printf ( "Modem      %d: ", bc.hdr.label );
						break;

					case ITRC_INPUT:
						printf ( "Input      %d: ", bc.hdr.label );
						break;

					case ITRC_OUTPUT:
						printf ( "Output     %d: ", bc.hdr.label );
						fmt = kInt;
						break;

					case ITRC_PUTC:
						printf ( "Put char   %d: ", bc.hdr.label );
						fmt = kChar;
						break;

					case ITRC_QUEUE:
						printf ( "Queue CMD  %d: ", bc.hdr.label );
						fmt = kInt;
						break;

					case ITRC_STFLW:
						printf ( "Stat Flow  %d: ", bc.hdr.label );
						fmt = kInt;
						break;

					case ITRC_SFIFO:
						printf ( "SFifo      %d: ", bc.hdr.label );
						break;

					case ITRC_VERIFY:
						printf ( "Verfy      %d: ", bc.hdr.label );
						fmt = kHex;
						break;

					case ITRC_WRITE:
						printf ( "Write      %d: ", bc.hdr.label );
						fmt = kChar;
						break;

					case ITRC_ERROR:
						printf ( "ERROR      %d: ", bc.hdr.label );
						fmt = kInt;
						break;

					default:
						printf ( "%08x          ", tbuf.buf[i] );
						break;
					}
				}
				else 
				{
               --bc.hdr.codes;
					switch ( fmt )
					{
					case kChar:
						printf ( "%c (0x%02x) ", 
							isprint ( tbuf.buf[i] ) ? tbuf.buf[i] : '.', tbuf.buf[i] );
						break;
					case kInt:
						printf ( "%d ", tbuf.buf[i] );
						break;

					case kAddr:
					case kHex:
						printf ( "0x%x ", tbuf.buf[i] );
						break;
					}
				}
         }
      }
      fflush ( stdout );
      timeout.tv_sec = 0;
      timeout.tv_usec = 250;
      select ( 0, NULL, NULL, NULL, &timeout );

   }
	printf("\n-------------------------\n");
	printf("End of trace\n");

   close ( fd );
}

