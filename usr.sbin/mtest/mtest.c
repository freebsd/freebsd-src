/*
 * Program to test new [sg]etsockopts and ioctls for manipulating IP and
 * Ethernet multicast address filters.
 *
 * Written by Steve Deering, Stanford University, February 1989.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

main( argc, argv )
    int argc;
    char **argv;
  {
    int so;
    char line[80];
    char *lineptr;
    struct ip_mreq imr;
    struct ifreq ifr;
    int n, f;
    unsigned i1, i2, i3, i4, g1, g2, g3, g4;
    unsigned e1, e2, e3, e4, e5, e6;

    if( (so = socket( AF_INET, SOCK_DGRAM, 0 )) == -1)
      {
	perror( "can't open socket" );
	exit( 1 );
      }

    printf( "multicast membership test program; " );
    printf( "enter ? for list of commands\n" );

    while( fgets( line, 79, stdin ) != NULL )
      {
	lineptr = line;
	while( *lineptr == ' ' || *lineptr == '\t' ) ++lineptr;
	switch( *lineptr )
	  {
	    case '?':
	      {
		printf( "%s%s%s%s%s%s%s",
		" j g.g.g.g i.i.i.i      - join  IP  multicast group     \n",
		" l g.g.g.g i.i.i.i      - leave IP  multicast group     \n",
		" a ifname e.e.e.e.e.e   - add ether multicast address   \n",
		" d ifname e.e.e.e.e.e   - del ether multicast address   \n",
		" m ifname 1/0           - set/clear ether allmulti flag \n",
		" p ifname 1/0           - set/clear ether promisc flag  \n",
		" q                      - quit                      \n\n" );
		break;
	      }

	    case 'j':
	      {
		++lineptr;
		while( *lineptr == ' ' || *lineptr == '\t' ) ++lineptr;
		if( (n = sscanf( lineptr, "%u.%u.%u.%u %u.%u.%u.%u %u",
		    &g1, &g2, &g3, &g4, &i1, &i2, &i3, &i4 )) != 8 )
		  {
		    printf( "bad args\n" );
		    break;
		  }
		imr.imr_multiaddr.s_addr = (g1<<24) | (g2<<16) | (g3<<8) | g4;
		imr.imr_multiaddr.s_addr = htonl(imr.imr_multiaddr.s_addr);
		imr.imr_interface.s_addr = (i1<<24) | (i2<<16) | (i3<<8) | i4;
		imr.imr_interface.s_addr = htonl(imr.imr_interface.s_addr);
		if( setsockopt( so, IPPROTO_IP, IP_ADD_MEMBERSHIP,
				&imr, sizeof(struct ip_mreq) ) == -1 )
		     perror( "can't join group" );
		else printf( "group joined\n" );
		break;
	      }	    

	    case 'l':
	      {
		++lineptr;
		while( *lineptr == ' ' || *lineptr == '\t' ) ++lineptr;
		if( (n = sscanf( lineptr, "%u.%u.%u.%u %u.%u.%u.%u %u",
		    &g1, &g2, &g3, &g4, &i1, &i2, &i3, &i4 )) != 8 )
		  {
		    printf( "bad args\n" );
		    break;
		  }
		imr.imr_multiaddr.s_addr = (g1<<24) | (g2<<16) | (g3<<8) | g4;
		imr.imr_multiaddr.s_addr = htonl(imr.imr_multiaddr.s_addr);
		imr.imr_interface.s_addr = (i1<<24) | (i2<<16) | (i3<<8) | i4;
		imr.imr_interface.s_addr = htonl(imr.imr_interface.s_addr);
		if( setsockopt( so, IPPROTO_IP, IP_DROP_MEMBERSHIP,
				&imr, sizeof(struct ip_mreq) ) == -1 )
		     perror( "can't leave group" );
		else printf( "group left\n" );
		break;
	      }	    

	    case 'a':
	      {
		++lineptr;
		while( *lineptr == ' ' || *lineptr == '\t' ) ++lineptr;
		if( (n = sscanf( lineptr, "%s %x.%x.%x.%x.%x.%x",
			ifr.ifr_name, &e1, &e2, &e3, &e4, &e5, &e6 )) != 7 )
		  {
		    printf( "bad args\n" );
		    break;
		  }
		ifr.ifr_addr.sa_family = AF_UNSPEC;
		ifr.ifr_addr.sa_data[0] = e1;
		ifr.ifr_addr.sa_data[1] = e2;
		ifr.ifr_addr.sa_data[2] = e3;
		ifr.ifr_addr.sa_data[3] = e4;
		ifr.ifr_addr.sa_data[4] = e5;
		ifr.ifr_addr.sa_data[5] = e6;
		if( ioctl( so, SIOCADDMULTI, &ifr ) == -1 )
		     perror( "can't add ether adress" );
		else printf( "ether address added\n" );
		break;
	      }	    

	    case 'd':
	      {
		++lineptr;
		while( *lineptr == ' ' || *lineptr == '\t' ) ++lineptr;
		if( (n = sscanf( lineptr, "%s %x.%x.%x.%x.%x.%x",
			ifr.ifr_name, &e1, &e2, &e3, &e4, &e5, &e6 )) != 7 )
		  {
		    printf( "bad args\n" );
		    break;
		  }
		ifr.ifr_addr.sa_family = AF_UNSPEC;
		ifr.ifr_addr.sa_data[0] = e1;
		ifr.ifr_addr.sa_data[1] = e2;
		ifr.ifr_addr.sa_data[2] = e3;
		ifr.ifr_addr.sa_data[3] = e4;
		ifr.ifr_addr.sa_data[4] = e5;
		ifr.ifr_addr.sa_data[5] = e6;
		if( ioctl( so, SIOCDELMULTI, &ifr ) == -1 )
		     perror( "can't delete ether adress" );
		else printf( "ether address deleted\n" );
		break;
	      }	    

	    case 'm':
	      {
		++lineptr;
		while( *lineptr == ' ' || *lineptr == '\t' ) ++lineptr;
		if( (n = sscanf( lineptr, "%s %u", ifr.ifr_name, &f )) != 2 )
		  {
		    printf( "bad args\n" );
		    break;
		  }
		if( ioctl( so, SIOCGIFFLAGS, &ifr ) == -1 )
		  {
		    perror( "can't get interface flags" );
		    break;
		  }
		printf( "interface flags %x, ", ifr.ifr_flags );
		fflush( stdout );
		if( f ) ifr.ifr_flags |=  IFF_ALLMULTI;
		else    ifr.ifr_flags &= ~IFF_ALLMULTI;
		if( ioctl( so, SIOCSIFFLAGS, &ifr ) == -1 )
		     perror( "can't set" );
		else printf( "changed to %x\n", ifr.ifr_flags );
		break;
	      }	    

	    case 'p':
	      {
		++lineptr;
		while( *lineptr == ' ' || *lineptr == '\t' ) ++lineptr;
		if( (n = sscanf( lineptr, "%s %u", ifr.ifr_name, &f )) != 2 )
		  {
		    printf( "bad args\n" );
		    break;
		  }
		if( ioctl( so, SIOCGIFFLAGS, &ifr ) == -1 )
		  {
		    perror( "can't get interface flags" );
		    break;
		  }
		printf( "interface flags %x, ", ifr.ifr_flags );
		fflush( stdout );
		if( f ) ifr.ifr_flags |=  IFF_PROMISC;
		else    ifr.ifr_flags &= ~IFF_PROMISC;
		if( ioctl( so, SIOCSIFFLAGS, &ifr ) == -1 )
		     perror( "can't set" );
		else printf( "changed to %x\n", ifr.ifr_flags );
		break;
	      }	    

	    case 'q': exit( 0 );

	    case 0:
	    case '\n': break;

	    default:
	      {
		printf( "bad command\n" );
		break;
	      }
	  }
      }
  }
