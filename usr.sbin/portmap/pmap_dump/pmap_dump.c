 /*
  * pmap_dump - dump portmapper table in format readable by pmap_set
  *
  * Author: Wietse Venema (wietse@wzv.win.tue.nl), dept. of Mathematics and
  * Computing Science, Eindhoven University of Technology, The Netherlands.
  */

#ifndef lint
#if 0
static char sccsid[] = "@(#) pmap_dump.c 1.1 92/06/11 22:53:15";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/portmap/pmap_dump/pmap_dump.c,v 1.6 2000/01/15 23:08:30 brian Exp $";
#endif

#include <stdio.h>
#include <sys/types.h>
#ifdef SYSV40
#include <netinet/in.h>
#include <rpc/rpcent.h>
#else
#include <netdb.h>
#endif
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

static const char *protoname __P((u_long));

int
main(argc, argv)
    int argc;
    char **argv;
{
    struct sockaddr_in addr;
    register struct pmaplist *list;
    register struct rpcent *rpc;

    get_myaddress(&addr);

    for (list = pmap_getmaps(&addr); list; list = list->pml_next) {
	rpc = getrpcbynumber((int) list->pml_map.pm_prog);
	printf("%10lu %4lu %5s %6lu  %s\n",
	       list->pml_map.pm_prog,
	       list->pml_map.pm_vers,
	       protoname(list->pml_map.pm_prot),
	       list->pml_map.pm_port,
	       rpc ? rpc->r_name : "");
    }
#undef perror
    return (fclose(stdout) ? (perror(argv[0]), 1) : 0);
}

static const char *
protoname(proto)
    u_long proto;
{
    static char buf[BUFSIZ];

    switch (proto) {
    case IPPROTO_UDP:
	return ("udp");
    case IPPROTO_TCP:
	return ("tcp");
    default:
	sprintf(buf, "%lu", proto);
	return (buf);
    }
}
