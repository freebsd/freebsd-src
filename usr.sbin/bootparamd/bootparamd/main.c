/*

This code is not copyright, and is placed in the public domain. Feel free to
use and modify. Please send modifications and/or suggestions + bug fixes to

        Klas Heggemann <klas@nada.kth.se>


	$FreeBSD$

*/

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <rpc/rpc.h>
#include "bootparam_prot.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>

int debug = 0;
int dolog = 0;
unsigned long route_addr = -1, inet_addr();
struct sockaddr_in my_addr;
char *progname;
char *bootpfile = "/etc/bootparams";

extern  void bootparamprog_1();

extern char *optarg;
extern int optind;

main(argc, argv)
int argc;
char **argv;
{
	SVCXPRT *transp;
	int i;
	struct hostent *he;
	struct stat buf;
	char *optstring;
	char c;

	progname = rindex(argv[0],'/');
	if ( progname ) progname++;
	else progname = argv[0];

	while ((c = getopt(argc, argv,"dsr:f:")) != EOF)
	  switch (c) {
	  case 'd':
	    debug = 1;
	    break;
	  case 'r':
	      if ( isdigit( *optarg)) {
		route_addr = inet_addr(optarg);
		break;
	      } else {
		he = gethostbyname(optarg);
		if (he) {
		   bcopy(he->h_addr, (char *)&route_addr, sizeof(route_addr));
		   break;
		 } else {
		   fprintf(stderr,"%s: No such host %s\n", progname, argv[i]);
		   exit(1);
		 }
	      }
	  case 'f':
	    bootpfile = optarg;
	    break;
	  case 's':
	    dolog = 1;
#ifndef LOG_DAEMON
	    openlog(progname, 0 , 0);
#else
	    openlog(progname, 0 , LOG_DAEMON);
	    setlogmask(LOG_UPTO(LOG_NOTICE));
#endif
	    break;
	  default:
	    fprintf(stderr,
		    "Usage: %s [-d ] [ -s ] [ -r router ] [ -f bootparmsfile ]\n", progname);
	    exit(1);
	  }

	if ( stat(bootpfile, &buf ) ) {
	  fprintf(stderr,"%s: ", progname);
	  perror(bootpfile);
	  exit(1);
	}


	if (route_addr == -1) {
	  get_myaddress(&my_addr);
	  bcopy(&my_addr.sin_addr.s_addr, &route_addr, sizeof (route_addr));
	}

	if (!debug) {
	  if (daemon(0,0)) {
	    perror("bootparamd: fork");
	    exit(1);
	  }
	}


	(void)pmap_unset(BOOTPARAMPROG, BOOTPARAMVERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		(void)fprintf(stderr, "cannot create udp service.\n");
		exit(1);
	}
	if (!svc_register(transp, BOOTPARAMPROG, BOOTPARAMVERS, bootparamprog_1, IPPROTO_UDP)) {
		(void)fprintf(stderr, "unable to register (BOOTPARAMPROG, BOOTPARAMVERS, udp).\n");
		exit(1);
	}

	svc_run();
	(void)fprintf(stderr, "svc_run returned\n");
	exit(1);
}


