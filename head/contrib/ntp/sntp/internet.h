/*  Copyright (C) 1996 N.M. Maclaren
    Copyright (C) 1996 The University of Cambridge

This includes all of the 'Internet' headers and definitions used across
modules, including those for handling timeouts.  No changes should be needed
for any version of Unix with Internet (IP version 5) addressing, but would be
for other addressing domains.  It needs <sys/socket.h> only because AF_INET is
needed by gethostbyaddr and is defined there rather than in <netdb.h>, for some
damn-fool reason. */



#include <setjmp.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>



/* It is most unclear whether these should be here or in kludges.h, as they are
kludges to keep 32-bit address dependencies out of the main body of internet.c,
to allow for the much heralded arrival of IP version 6.  It will be interesting
to see whether the universal availability of 64-bit integers arrives first. */

#define local_to_address(x,y) ((x)->s_addr = htonl((unsigned long)y))
#define network_to_address(x,y) ((x)->s_addr = (y))

#define NTP_PORT htons((unsigned short)123)    /* If not in /etc/services */
#define port_to_integer(x) (ntohs((unsigned short)(x)))


#if defined(_SS_MAXSIZE) || defined(_SS_SIZE)
#define HAVE_IPV6
#endif

/* Defined in internet.c */
#ifdef HAVE_IPV6
extern void find_address (struct sockaddr_storage *address,
    struct sockaddr_storage *anywhere,
    int *port, char *hostname, int timespan);
#else
extern void find_address (struct in_addr *address, struct in_addr *anywhere,
    int *port, char *hostname, int timespan);
#endif
