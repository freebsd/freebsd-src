#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <alias.h>

#include "command.h"
#include "vars.h"

static int
StrToAddr (char *, struct in_addr* addr);

static int
StrToPort (char *, u_short *port, char *proto);

static int
StrToAddrAndPort (char *, struct in_addr *addr, u_short *port, char *proto);


int
AliasRedirectPort (struct cmdtab *list,
                   int argc,
                   char **argv,
                   void *param)
{
    if (argc == 3)
    {
        char proto_constant;
        char *proto;
        u_short local_port;
        u_short alias_port;
        int error;
        struct in_addr local_addr;
        struct in_addr null_addr;
        struct alias_link *link;

        proto = argv[0];
        if (strcmp(proto, "tcp") == 0)
        {
            proto_constant = IPPROTO_TCP;
        }
        else if (strcmp(proto, "udp") == 0)
        {
            proto_constant = IPPROTO_UDP;
        }
        else
        {
            printf("port redirect: protocol must be tcp or udp\n"); 
            printf("Usage: alias %s %s\n", list->name, list->syntax);
            return 1;
        }

        error = StrToAddrAndPort(argv[1], &local_addr, &local_port, proto);
        if (error)
        {
            printf("port redirect: error reading local addr:port\n");
            printf("Usage: alias %s %s\n", list->name, list->syntax);
            return 1;
        }

        error = StrToPort(argv[2], &alias_port, proto);
        if (error)
        {
            printf("port redirect: error reading alias port\n");
            printf("Usage: alias %s %s\n", list->name, list->syntax);
            return 1;
        }

        null_addr.s_addr = 0;

        link = PacketAliasRedirectPort(local_addr, local_port,
                                        null_addr,  0,
                                        null_addr,  alias_port,
                                        proto_constant);

        if (link == NULL)
            printf("port redirect: error returned by packed aliasing engine"
                   "(code=%d)\n", error);

        return 1;
    }

    printf("Usage: alias %s %s\n", list->name, list->syntax);
    return 1;
}


int
AliasRedirectAddr(struct cmdtab *list,
                  int argc,
                  char **argv,
                  void *param)
{
    if (argc == 2)
    {
        int error;
        struct in_addr local_addr;
        struct in_addr alias_addr;
        struct alias_link *link;

        error = StrToAddr(argv[0], &local_addr);
        if (error)
        {
            printf("address redirect: invalid local address\n");
            return 1;
        }

        error = StrToAddr(argv[1], &alias_addr);
        if (error)
        {
            printf("address redirect: invalid alias address\n");
            printf("Usage: alias %s %s\n", list->name, list->syntax);
            return 1;
        }

        link = PacketAliasRedirectAddr(local_addr, alias_addr);
        if (link == NULL)
        {
            printf("address redirect: packet aliasing engine error\n");
            printf("Usage: alias %s %s\n", list->name, list->syntax);
        }

        return 1;
    }

    printf("Usage: alias %s %s\n", list->name, list->syntax);
    return 1;
}


static int
StrToAddr (char* str,
           struct in_addr* addr)
{
    struct hostent* hp;

    if (inet_aton (str, addr))
        return 0;

    hp = gethostbyname (str);
    if (!hp)
    {
        fprintf (stderr, "Unknown host %s.\n", str);
        return -1;
    }

    *addr = *((struct in_addr *) hp->h_addr);
    return 0;
}


static int
StrToPort (char *str,
           u_short *port,
           char *proto)
{
    int iport;
    struct servent* sp;
    char* end;

    iport = strtol (str, &end, 10);
    if (end != str)
    {
        *port = htons(iport);
        return 0;
    }

    sp = getservbyname (str, proto);
    if (!sp)
    {
        fprintf (stderr, "Unknown port or service %s/%s.\n",
                          str, proto);
        return -1;
    }

    *port = sp->s_port;
    return 0;
}


int
StrToAddrAndPort (char* str,
                  struct in_addr* addr,
                  u_short *port,
                  char *proto)
{
    char *ptr;

    ptr = strchr (str, ':');
    if (!ptr)
    {
        fprintf (stderr, "%s is missing port number.\n", str);
        return -1;
    }

    *ptr = '\0';
    ++ptr;

    if (StrToAddr (str, addr) != 0)
        return -1;

    return StrToPort (ptr, port, proto);
}

