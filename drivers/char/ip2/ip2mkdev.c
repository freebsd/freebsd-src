#include <sys/types.h>
#include <sys/stat.h>
#include <linux/major.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "ip2.h"
#include "i2ellis.h"

char nm[256];
i2eBordStr Board[2];

static void ex_details(i2eBordStrPtr);

int main (int argc, char *argv[])
{
	int board, box, port;
	int fd;
	int dev;
	i2eBordStrPtr pB  = Board;

	// Remove all IP2 devices

	for ( board = 0; board < 4; ++board )
	{
		sprintf ( nm, "/dev/ip2ipl%d", board );
		unlink ( nm );
		sprintf ( nm, "/dev/ip2stat%d", board );
		unlink ( nm );
	}

	for ( port = 0; port < 256; ++port  )
	{
		sprintf ( nm, "/dev/ttyF%d", port );
		unlink ( nm );
		sprintf ( nm, "/dev/cuf%d", port );
		unlink ( nm );
	}

	// Now create management devices, and use the status device to determine how
	// port devices need to exist, and then create them.

	for ( board = 0; board < 4; ++board )
	{
		printf("Board %d: ", board );

		sprintf ( nm, "/dev/ip2ipl%d", board );
		mknod ( nm, S_IFCHR|0666, (IP2_IPL_MAJOR << 8) | board * 4 );
		sprintf ( nm, "/dev/ip2stat%d", board );
		mknod ( nm, S_IFCHR|0666, (IP2_IPL_MAJOR << 8) | board * 4 + 1 );

		fd = open ( nm, O_RDONLY );
		if ( !fd )
		{
			printf ( "Unable to open status device %s\n", nm );
			exit ( 1 );
		}
		if ( ioctl ( fd,  65, Board ) < 0 )
		{
			printf ( "not present\n" );
			close ( fd );
			unlink ( nm );
			sprintf ( nm, "/dev/ip2ipl%d", board );
			unlink ( nm );
		}
		else
		{
			switch( pB->i2ePom.e.porID & ~POR_ID_RESERVED ) 
			{
			case POR_ID_FIIEX: ex_details ( pB );       break;
			case POR_ID_II_4:  printf ( "ISA-4" );      break;
			case POR_ID_II_8:  printf ( "ISA-8 std" );  break;
			case POR_ID_II_8R: printf ( "ISA-8 RJ11" ); break;
		
			default:
				printf ( "Unknown board type, ID = %x", pB->i2ePom.e.porID );
			}

			for ( box = 0; box < ABS_MAX_BOXES; ++box )
			{
				for ( port = 0; port < ABS_BIGGEST_BOX; ++port )
				{
					if ( pB->i2eChannelMap[box] & ( 1 << port ) )
					{
						dev = port 
							 + box * ABS_BIGGEST_BOX 
							 + board * ABS_BIGGEST_BOX * ABS_MAX_BOXES;
	
						sprintf ( nm, "/dev/ttyF%d", dev );
						mknod ( nm, S_IFCHR|0666, (IP2_TTY_MAJOR << 8) | dev );
						sprintf ( nm, "/dev/cuf%d", dev );
						mknod ( nm, S_IFCHR|0666, (IP2_CALLOUT_MAJOR << 8) | dev );

						printf(".");
					}
				}
			}
			printf("\n");
		}
	}
}

static void ex_details ( i2eBordStrPtr pB )
{
	int            box;
	int            i;
	int            ports = 0;
	int            boxes = 0;

	for( box = 0; box < ABS_MAX_BOXES; ++box )
	{
		if( pB->i2eChannelMap[box] != 0 ) ++boxes;
		for( i = 0; i < ABS_BIGGEST_BOX; ++i ) 
		{
			if( pB->i2eChannelMap[box] & 1<< i ) ++ports;
		}
	}

	printf("EX bx=%d pt=%d %d bit", boxes, ports, pB->i2eDataWidth16 ? 16 : 8 );
}


