/*
 * Copyright (c) 1993 Daniel Boulet
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 *
 * Command line interface for IP firewall facility
 */

#define IPFIREWALL
#include <kvm.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <machine/endian.h>  
#include <nlist.h>
#include <paths.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_fw.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>


struct nlist nl[]={
#define N_BCHAIN 	0
	{ "_ip_fw_blk_chain" },
#define N_FCHAIN 	1
	{ "_ip_fw_fwd_chain" },
#define N_POLICY 	2
	{ "_ip_fw_policy" },
	"" ,
};

typedef enum {
    IPF_BLOCKING,
    IPF_FORWARDING
} ipf_kind;

int do_resolv=1;

show_usage()
{
fprintf(stderr,"ipfw: [-n] <command>\n");
}


static
void
print_ip(xaddr)
struct in_addr xaddr;
{
    u_long addr = ntohl(xaddr.s_addr);
    printf("%d.%d.%d.%d",(addr>>24) & 0xff,(addr>>16)&0xff,(addr>>8)&0xff,addr&0xff);
}

void
show_firewall_chain(chain)
struct ip_firewall *chain;
{
	    char *comma;
	    u_long adrt;
	    struct hostent *he;
	    int i;

	    if ( chain->flags & IP_FIREWALL_ACCEPT ) {
		printf("accept ");
	    } else {
		printf("deny   ");
	    }

	    switch ( chain->flags & IP_FIREWALL_KIND ) {
	    case IP_FIREWALL_ICMP: printf("icmp "); break;
	    case IP_FIREWALL_TCP: printf("tcp  "); break;
	    case IP_FIREWALL_UDP: printf("udp  "); break;
	    case IP_FIREWALL_UNIVERSAL: printf("all  "); break;
	    default: break;
	    }
	    printf("from ");

	    adrt=ntohl(chain->src_mask.s_addr);
	    if (adrt==0xFFFFFFFFl && do_resolv)
		{
		  adrt=(chain->src.s_addr);
	          he=gethostbyaddr((char *)&adrt,sizeof(u_long),AF_INET);
		  if (he==NULL)
			{
	    		 print_ip(chain->src); printf(":");
	    		 print_ip(chain->src_mask);
			}
		  else
			printf("%s",he->h_name);
		}
	    else
		{
	          print_ip(chain->src); printf(":");
	          print_ip(chain->src_mask);
		}

	    comma = " ";
	    for ( i = 0; i < chain->num_src_ports; i += 1 ) {
		printf("%s%d",comma,chain->ports[i]);
		if ( i == 0 && (chain->flags & IP_FIREWALL_SRC_RANGE) ) {
		    comma = ":";
		} else {
		    comma = ",";
		}
	    }

	    printf(" to ");
	    adrt=ntohl(chain->dst_mask.s_addr);
	    if (adrt==0xFFFFFFFFl && do_resolv)
		{
		  adrt=(chain->dst.s_addr);
	          he=gethostbyaddr((char *)&adrt,sizeof(u_long),AF_INET);
		  if (he==NULL)
			{
	    		 print_ip(chain->dst); printf(":");
	    		 print_ip(chain->dst_mask);
			}
		  else
			printf("%s",he->h_name);
		}
	    else
		{
	          print_ip(chain->dst); printf(":");
	          print_ip(chain->dst_mask);
		}

	    comma = " ";
	    for ( i = 0; i < chain->num_dst_ports; i += 1 ) {
		printf("%s%d",comma,chain->ports[chain->num_src_ports+i]);
		if ( i == chain->num_src_ports && (chain->flags & IP_FIREWALL_DST_RANGE) ) {
		    comma = ":";
		} else {
		    comma = ",";
		}
	    }
	    printf("\n");
	   /* chain = chain->next; */
}


list_kernel_data()
{
kvm_t *kd;
static char errb[_POSIX2_LINE_MAX];
struct ip_firewall b,*btmp;

 if ( (kd=kvm_openfiles(NULL,NULL,NULL,O_RDONLY,errb)) == NULL)
    {
     printf("kvm_openfiles: %s\n",kvm_geterr(kd));
     exit(1);
    }
 if (kvm_nlist(kd,nl) < 0 || nl[0].n_type == 0)
    {
      printf("kvm_nlist: no namelist in %s\n",getbootfile());
      exit(1);
    }

kvm_read(kd,(u_long)nl[N_BCHAIN].n_value,&b,sizeof(struct ip_firewall));
printf("Blocking chain entries:\n");
while(b.next!=NULL)
{
 btmp=b.next;
 kvm_read(kd,(u_long)btmp,&b,sizeof(struct ip_firewall));
 show_firewall_chain(&b);
}

kvm_read(kd,(u_long)nl[N_FCHAIN].n_value,&b,sizeof(struct ip_firewall));
printf("Forwarding chain entries:\n");
while(b.next!=NULL)
{
 btmp=b.next;
 kvm_read(kd,(u_long)btmp,&b,sizeof(struct ip_firewall));
 show_firewall_chain(&b);
}
}



static
char *
fmtip(u_long uaddr)
{
    static char tbuf[100];

    sprintf(tbuf,"%d.%d.%d.%d",
    ((char *)&uaddr)[0] & 0xff,
    ((char *)&uaddr)[1] & 0xff,
    ((char *)&uaddr)[2] & 0xff,
    ((char *)&uaddr)[3] & 0xff);

    return( &tbuf[0] );
}

static
void
print_ports(int cnt, int range, u_short *ports)
{
    int ix;
    char *pad;

    if ( range ) {
	if ( cnt < 2 ) {
	    fprintf(stderr,"ipfw:  range flag set but only %d ports\n",cnt);
	    abort();
	}
	printf("%d:%d",ports[0],ports[1]);
	ix = 2;
	pad = " ";
    } else {
	ix = 0;
	pad = "";
    }

    while ( ix < cnt ) {
	printf("%s%d",pad,ports[ix]);
	pad = " ";
	ix += 1;
    }
}

int
do_setsockopt( int fd, int proto, int cmd, void *data, int datalen, int ok_errno )
{

    switch ( cmd ) {
    case IP_FW_FLUSH:  break;
    case IP_FW_POLICY: break;
    case IP_FW_CHK_BLK:  break;
    case IP_FW_CHK_FWD:  break;
    case IP_FW_ADD_BLK:  break;
    case IP_FW_ADD_FWD:  break;
    case IP_FW_DEL_BLK:  break;
    case IP_FW_DEL_FWD:  break;
    default:
	fprintf(stderr,"ipfw: unknown command (%d) passed to setsockopt\n",cmd);
	exit(1);
    }
	if ( setsockopt(fd, proto, cmd, data, datalen) < 0 ) {
	    if ( errno == ok_errno ) {
		return(errno);
	    }
	    perror("ipfw: setsockopt");
	    exit(1);
           }
    return(0);
}

void
show_parms(char **argv)
{
    while ( *argv ) {
	printf("%s ",*argv++);
    }
}

int
get_protocol(char *arg,void (*cmd_usage)(ipf_kind),ipf_kind kind)
{
    if ( arg == NULL ) {
	fprintf(stderr,"ipfw: missing protocol name\n");
    } else if ( strcmp(arg, "tcp") == 0 ) {
	return( IP_FIREWALL_TCP );
    } else if ( strcmp(arg, "udp") == 0 ) {
	return( IP_FIREWALL_UDP );
    } else if ( strcmp(arg, "icmp") == 0 ) {
	return( IP_FIREWALL_ICMP );
    } else if ( strcmp(arg, "all") == 0 ) {
	return( IP_FIREWALL_UNIVERSAL );
    } else {
	fprintf(stderr,"illegal protocol name \"%s\"\n",arg);
    }
    exit(1);
    return(0);
}

void
get_ipaddr(char *arg,struct in_addr *addr,struct in_addr *mask,void(*usage)(ipf_kind),ipf_kind kind)
{
    char *p, *tbuf;
    int period_cnt, non_digit;
    struct hostent *hptr;

    if ( arg == NULL ) {
	fprintf(stderr,"ipfw: missing ip address\n");
	exit(1);
    }

    period_cnt = 0;
    non_digit = 0;
    for ( p = arg; *p != '\0' && *p != '/' && *p != ':'; p += 1 ) {
	if ( *p == '.' ) {
	    if ( p > arg && *(p-1) == '.' ) {
		fprintf(stderr,"ipfw: two periods in a row in ip address (%s)\n",arg);
		exit(1);
	    }
	    period_cnt += 1;
	} else if ( !isdigit(*p) ) {
	    non_digit = 1;
	}
    }

    tbuf = malloc(p - arg + 1);
    strncpy(tbuf,arg,p-arg);
    tbuf[p-arg] = '\0';

    if ( non_digit  ) 
	{
	if (do_resolv)
         {
	hptr = gethostbyname(tbuf);
	if ( hptr == NULL ) {
	    fprintf(stderr,"ipfw:  unknown host \"%s\"\n",tbuf);
	    exit(1);
	 }
         }
	else
	 {
	   fprintf(stderr,"ipfw: bad IP \"%s\"\n",tbuf);
	   exit(1);
	 }

	if ( hptr->h_length != sizeof(struct in_addr) ) {
	    fprintf(stderr,"ipfe: hostentry addr length = %d, expected 4\n",
	    hptr->h_length,sizeof(struct in_addr));
	    exit(1);
	}

	bcopy( hptr->h_addr, addr, sizeof(struct in_addr) );

    } else {

	if ( period_cnt == 3 ) {

	    int a1, a2, a3, a4;

		sscanf(tbuf,"%d.%d.%d.%d",&a1,&a2,&a3,&a4);

	    if ( a1 > 255 || a2 > 255 || a3 > 255 || a4 > 255 ) {
		fprintf(stderr,"ipfw: number too large in ip address (%s)\n",arg);
		exit(1);
	    }

	    ((char *)addr)[0] = a1;
	    ((char *)addr)[1] = a2;
	    ((char *)addr)[2] = a3;
	    ((char *)addr)[3] = a4;

	} else if ( strcmp(tbuf,"0") == 0 ) {

	    ((char *)addr)[0] = 0;
	    ((char *)addr)[1] = 0;
	    ((char *)addr)[2] = 0;
	    ((char *)addr)[3] = 0;

	} else {

	    fprintf(stderr,"ipfw:  incorrect ip address format \"%s\"\n",tbuf);
	    exit(1);

	}

    }

    free(tbuf);

    if ( mask == NULL ) {

	if ( *p != '\0' ) {
	    fprintf(stderr,"ipfw: ip netmask no allowed here (%s)\n",addr);
	    exit(1);
	}

    } else {

	if ( *p == ':' ) {

	    get_ipaddr(p+1,mask,NULL,usage,kind);

	} else if ( *p == '/' ) {

	    int bits;
	    char *end;

	    p += 1;
	    if ( *p == '\0' ) {
		fprintf(stderr,"ipfw: missing mask value (%s)\n",arg);
		exit(1);
	    } else if ( !isdigit(*p) ) {
		fprintf(stderr,"ipfw: non-numeric mask value (%s)\n",arg);
		exit(1);
	    }

	    bits = strtol(p,&end,10);
	    if ( *end != '\0' ) {
		fprintf(stderr,"ipfw: junk after mask (%s)\n",arg);
		exit(1);
	    }

	    if ( bits < 0 || bits > sizeof(u_long) * 8 ) {
		fprintf(stderr,"ipfw: mask length value out of range (%s)\n",arg);
		exit(1);
	    }

	    if ( bits == 0 ) {	/* left shifts of 32 aren't defined */
		mask->s_addr = 0;
	    } else {
		((char *)mask)[0] = (-1 << (32 - bits)) >> 24;
		((char *)mask)[1] = (-1 << (32 - bits)) >> 16;
		((char *)mask)[2] = (-1 << (32 - bits)) >>  8;
		((char *)mask)[3] = (-1 << (32 - bits)) >>  0;
	    }

	} else if ( *p == '\0' ) {

	    mask->s_addr = 0xffffffff;

	} else {

	    fprintf(stderr,"ipfw: junk after ip address (%s)\n",arg);
	    exit(1);

	}

	/*
	 * Mask off any bits in the address that are zero in the mask.
	 * This allows the user to describe a network by specifying
	 * any host on the network masked with the network's netmask.
	 */

	addr->s_addr &= mask->s_addr;

    }

}

u_short
get_one_port(char *arg,void (*usage)(ipf_kind),ipf_kind kind,const char *proto_name)
{
    int slen = strlen(arg);

    if ( slen > 0 && strspn(arg,"0123456789") == slen ) {
	int port;
	char *end;

	port = strtol(arg,&end,10);
	if ( *end != '\0' ) {
	    fprintf(stderr,"ipfw: illegal port number (%s)\n",arg);
	    exit(1);
	}

	if ( port <= 0 || port > 65535 ) {
	    fprintf(stderr,"ipfw: port number out of range (%d)\n",port);
	    exit(1);
	}

	return( port );

    } else {

	struct servent *sptr;

	sptr = getservbyname(arg,proto_name);

	if ( sptr == NULL ) {
	    fprintf(stderr,"ipfw: unknown %s service \"%s\"\n",proto_name,arg);
	    exit(1);
	}

	return( ntohs(sptr->s_port) );

    }

}

int
get_ports(char ***argv_ptr,u_short *ports,int min_ports,int max_ports,void (*usage)(ipf_kind),ipf_kind kind,const char *proto_name)
{
    int ix;
    char *arg;
    int sign;

    ix = 0;
    sign = 1;
    while ( (arg = **argv_ptr) != NULL && strcmp(arg,"from") != 0 && strcmp(arg,"to") != 0 ) {

	char *p;

	/*
	 * Check that we havn't found too many port numbers.
	 * We do this here instead of with another condition on the while loop
	 * so that the caller can assume that the next parameter is NOT a port number.
	 */

	if ( ix >= max_ports ) {
	    fprintf(stderr,"ipfw: too many port numbers (max %d\n",max_ports);
	    exit(1);
	}

	if ( (p = strchr(arg,':')) == NULL ) {

	    ports[ix++] = get_one_port(arg,usage,kind,proto_name);

	} else {

	    if ( ix > 0 ) {

		fprintf(stderr,"ipfw: port ranges are only allowed for the first port value pair (%s)\n",arg);
		exit(1);

	    }

	    if ( max_ports > 1 ) {

		char *tbuf;

		tbuf = malloc( (p - arg) + 1 );
		strncpy(tbuf,arg,p-arg);
		tbuf[p-arg] = '\0';

		ports[ix++] = get_one_port(tbuf,usage,kind,proto_name);
		ports[ix++] = get_one_port(p+1,usage,kind,proto_name);
		sign = -1;

	    } else {

		fprintf(stderr,"ipfw:  port range not allowed here (%s)\n",arg);
		exit(1);

	    }
	}

	*argv_ptr += 1;
    }

    if ( ix < min_ports ) {
	if ( min_ports == 1 ) {
	    fprintf(stderr,"ipfw:  missing port number%s\n",max_ports == 1 ? "" : "(s)" );
	} else {
	    fprintf(stderr,"ipfw: not enough port numbers (expected %d)\n",min_ports);
	}
	exit(1);
    }

    return( sign * ix );

}

void
check_usage(ipf_kind kind)
{
    fprintf(stderr,"usage: ipfw check%s <expression>\n",
    kind == IPF_BLOCKING ? "blocking" : "forwarding");
}

void
check(ipf_kind kind, int socket_fd, char **argv)
{
    int protocol;
    struct ip *packet;
    char *proto_name;

    packet = (struct ip *)malloc( sizeof(struct ip) + sizeof(struct tcphdr) );
    packet->ip_v = IPVERSION;
    packet->ip_hl = sizeof(struct ip) / sizeof(int);


    proto_name = *argv++;
    protocol = get_protocol(proto_name,check_usage,kind);
    switch ( protocol ) {
    case IP_FIREWALL_TCP: packet->ip_p = IPPROTO_TCP; break;
    case IP_FIREWALL_UDP: packet->ip_p = IPPROTO_UDP; break;
    default:
	fprintf(stderr,"ipfw:  can only check TCP or UDP packets\n");
	break;
    }

    if ( *argv == NULL ) {
	fprintf(stderr,"ipfw:  missing \"from\" from keyword\n");
	exit(1);
    }
    if ( strcmp(*argv,"from") == 0 ) {
	argv += 1;
	get_ipaddr(*argv++,&packet->ip_src,NULL,check_usage,kind);
	if ( protocol == IP_FIREWALL_TCP || protocol == IP_FIREWALL_UDP ) {
	    get_ports(&argv,&((struct tcphdr *)(&packet[1]))->th_sport,1,1,check_usage,kind,proto_name);
	    ((struct tcphdr *)(&packet[1]))->th_sport = htons(
		((struct tcphdr *)(&packet[1]))->th_sport
	    );
	}
    } else {
	fprintf(stderr,"ipfw: expected \"from\" keyword, got \"%s\"\n",*argv);
	exit(1);
    }

    if ( *argv == NULL ) {
	fprintf(stderr,"ipfw: missing \"to\" from keyword\n");
	exit(1);
    }
    if ( strcmp(*argv,"to") == 0 ) {
	argv += 1;
	get_ipaddr(*argv++,&packet->ip_dst,NULL,check_usage,kind);
	if ( protocol == IP_FIREWALL_TCP || protocol == IP_FIREWALL_UDP ) {
	    get_ports(&argv,&((struct tcphdr *)(&packet[1]))->th_dport,1,1,check_usage,kind,proto_name);
	    ((struct tcphdr *)(&packet[1]))->th_dport = htons(
		((struct tcphdr *)(&packet[1]))->th_dport
	    );
	}
    } else {
	fprintf(stderr,"ipfw: expected \"to\" keyword, got \"%s\"\n",*argv);
	exit(1);
    }

    if ( *argv == NULL ) {


	if ( do_setsockopt( 
		socket_fd, IPPROTO_IP,
		kind == IPF_BLOCKING ? IP_FW_CHK_BLK : IP_FW_CHK_FWD,
		packet,
		sizeof(struct ip) + sizeof(struct tcphdr),
		EACCES
	    ) == 0
	) {
	    printf("packet accepted by %s firewall\n",
	    kind == IPF_BLOCKING ? "blocking" : "forwarding");
	} else {
	    printf("packet rejected by %s firewall\n",
	    kind == IPF_BLOCKING ? "blocking" : "forwarding");
	}

	return;

    } else {
	fprintf(stderr,"ipfw: extra parameters at end of command (");
	show_parms(argv);
	fprintf(stderr,")\n");
	exit(1);
    }
}

void
add_usage(ipf_kind kind)
{
    fprintf(stderr,"usage: ipfw add%s [accept|deny] <expression>\n",
    kind == IPF_BLOCKING ? "blocking" : "forwarding");
}

void
add(ipf_kind kind, int socket_fd, char **argv)
{
    int protocol, accept_firewall, src_range, dst_range;
    struct ip_firewall firewall;
    char *proto_name;


    if ( *argv == NULL ) {
	add_usage(kind);
	exit(1);
    }

    if ( strcmp(*argv,"deny") == 0 ) {
	accept_firewall = 0;
    } else if ( strcmp(*argv,"accept") == 0 ) {
	accept_firewall = IP_FIREWALL_ACCEPT;
    } else {
	add_usage(kind);
	exit(1);
    }

    argv += 1;
    proto_name = *argv++;
    protocol = get_protocol(proto_name,add_usage,kind);

    if ( *argv == NULL ) {
	fprintf(stderr,"ipfw: missing \"from\" keyword\n");
	exit(1);
    }
    if ( strcmp(*argv,"from") == 0 ) {
	argv++;
	get_ipaddr(*argv++,&firewall.src,&firewall.src_mask,add_usage,kind);
	if ( protocol == IP_FIREWALL_TCP || protocol == IP_FIREWALL_UDP ) {
	    int cnt;
	    cnt = get_ports(&argv,&firewall.ports[0],0,IP_FIREWALL_MAX_PORTS,add_usage,kind,proto_name);
	    if ( cnt < 0 ) {
		src_range = IP_FIREWALL_SRC_RANGE;
		cnt = -cnt;
	    } else {
		src_range = 0;
	    }
	    firewall.num_src_ports = cnt;
	} else {
	    firewall.num_src_ports = 0;
	    src_range = 0;
	}
    } else {
	fprintf(stderr,"ipfw: expected \"from\", got \"%s\"\n",*argv);
	exit(1);
    }

    if ( *argv == NULL ) {
	fprintf(stderr,"ipfw: missing \"to\" keyword\n");
	exit(1);
    }
    if ( strcmp(*argv,"to") == 0 ) {
	argv++;
	get_ipaddr(*argv++,&firewall.dst,&firewall.dst_mask,add_usage,kind);
	if ( protocol == IP_FIREWALL_TCP || protocol == IP_FIREWALL_UDP ) {
	    int cnt;
	    cnt = get_ports(&argv,&firewall.ports[firewall.num_src_ports],0,IP_FIREWALL_MAX_PORTS-firewall.num_src_ports,add_usage,kind,proto_name);
	    if ( cnt < 0 ) {
		dst_range = IP_FIREWALL_DST_RANGE;
		cnt = -cnt;
	    } else {
		dst_range = 0;
	    }
	    firewall.num_dst_ports = cnt;
	} else {
	    firewall.num_dst_ports = 0;
	    dst_range = 0;
	}
    } else {
	fprintf(stderr,"ipfw:  expected \"to\", got \"%s\"\n",*argv);
	exit(1);
    }

    if ( *argv == NULL ) {

	firewall.flags = protocol | accept_firewall | src_range | dst_range;
	(void)do_setsockopt( 
	    socket_fd, IPPROTO_IP,
	    kind == IPF_BLOCKING ? IP_FW_ADD_BLK : IP_FW_ADD_FWD,
	    &firewall,
	    sizeof(firewall),
	    0
	);

    } else {
	fprintf(stderr,"ipfw: extra parameters at end of command (");
	show_parms(argv);
	fprintf(stderr,")\n");
	exit(1);
    }
}

void
del_usage(ipf_kind kind)
{
    fprintf(stderr,"usage: ipfw del%s <expression>\n",
    kind == IPF_BLOCKING ? "blocking" : "forwarding");
}

void
del(ipf_kind kind, int socket_fd, char **argv)
{
    int protocol, accept_firewall, src_range, dst_range;
    struct ip_firewall firewall;
    char *proto_name;

    if ( *argv == NULL ) {
	fprintf(stderr,"ipfw: missing \"accept\" or \"deny\" keyword\n");
	exit(1);
    }

    if ( strcmp(*argv,"deny") == 0 ) {
	accept_firewall = 0;
    } else if ( strcmp(*argv,"accept") == 0 ) {
	accept_firewall = IP_FIREWALL_ACCEPT;
    } else {
	fprintf(stderr,"ipfw: expected \"accept\" or \"deny\", got \"%s\"\n",*argv);
	exit(1);
    }

    argv += 1;
    proto_name = *argv++;
    protocol = get_protocol(proto_name,del_usage,kind);

    if ( *argv == NULL ) {
	fprintf(stderr,"ipfw: missing \"from\" keyword\n");
	exit(1);
    }
    if ( strcmp(*argv,"from") == 0 ) {
	argv++;
	get_ipaddr(*argv++,&firewall.src,&firewall.src_mask,del_usage,kind);
	if ( protocol == IP_FIREWALL_TCP || protocol == IP_FIREWALL_UDP ) {
	    int cnt;
	    cnt = get_ports(&argv,&firewall.ports[0],0,IP_FIREWALL_MAX_PORTS,del_usage,kind,proto_name);
	    if ( cnt < 0 ) {
		src_range = IP_FIREWALL_SRC_RANGE;
		cnt = -cnt;
	    } else {
		src_range = 0;
	    }
	    firewall.num_src_ports = cnt;
	} else {
	    firewall.num_src_ports = 0;
	    src_range = 0;
	}
    } else {
	fprintf(stderr,"ipfw: expected \"from\", got \"%s\"\n",*argv);
	exit(1);
    }

    if ( *argv == NULL ) {
	fprintf(stderr,"ipfw: missing \"to\" keyword\n");
	exit(1);
    }
    if ( strcmp(*argv,"to") == 0 ) {
	argv++;
	get_ipaddr(*argv++,&firewall.dst,&firewall.dst_mask,del_usage,kind);
	if ( protocol == IP_FIREWALL_TCP || protocol == IP_FIREWALL_UDP ) {
	    int cnt;
	    cnt = get_ports(&argv,&firewall.ports[firewall.num_src_ports],0,IP_FIREWALL_MAX_PORTS-firewall.num_src_ports,del_usage,kind,proto_name);
	    if ( cnt < 0 ) {
		dst_range = IP_FIREWALL_DST_RANGE;
		cnt = -cnt;
	    } else {
		dst_range = 0;
	    }
	    firewall.num_dst_ports = cnt;
	} else {
	    firewall.num_dst_ports = 0;
	    dst_range = 0;
	}
    } else {
	fprintf(stderr,"ipfw: expected \"to\", got \"%s\"\n",*argv);
	exit(1);
    }

    if ( *argv == NULL ) {

	firewall.flags = protocol | accept_firewall | src_range | dst_range;
	(void)do_setsockopt(
	    socket_fd, IPPROTO_IP,
	    kind == IPF_BLOCKING ? IP_FW_DEL_BLK : IP_FW_DEL_FWD,
	    &firewall,
	    sizeof(firewall),
	    0
	);

    } else {
	fprintf(stderr,"ipfw: extra parameters at end of command (");
	show_parms(argv);
	fprintf(stderr,")\n");
        exit(1);
        }
}


void
policy(int socket_fd, char **argv)
{
 int p;
 kvm_t *kd;
 static char errb[_POSIX2_LINE_MAX];
 int  b;

if (argv[0]==NULL || strlen(argv[0])<=0) 
 {
 if ( (kd=kvm_openfiles(NULL,NULL,NULL,O_RDONLY,errb)) == NULL)
    {
     printf("kvm_openfiles: %s\n",kvm_geterr(kd));
     exit(1);
    }
 if (kvm_nlist(kd,nl) < 0 || nl[0].n_type == 0)
    {
      printf("kvm_nlist: no namelist in %s\n",getbootfile());
      exit(1);
    }

kvm_read(kd,(u_long)nl[N_POLICY].n_value,&b,sizeof(int));

if (b==1)
	printf("Default policy: ACCEPT\n");
if (b==0) 
	printf("Default policy: DENY\n");
if (b!=0 && b!=1)
	printf("Wrong policy value\n");
exit(1);
}

 if (strncmp(argv[0],"deny",strlen(argv[0])))
	p=1;
 else
 if (strncmp(argv[0],"accept",strlen(argv[0])))
	p=0;
 else
	{
         fprintf(stderr,"usage: ipfw policy [deny|accept]\n");
	 exit(1);
	}

	(void)do_setsockopt(
	    socket_fd, IPPROTO_IP,
	    IP_FW_POLICY,
	    &p,
	    sizeof(p),
	    0
	);
}
     

main(argc,argv)
int argc;
char **argv;
{
    int socket_fd;
    struct ip_firewall *data,*fdata;
    char **str;

    socket_fd = socket( AF_INET, SOCK_RAW, IPPROTO_RAW );

    if ( socket_fd < 0 ) {
	printf("Can't open raw socket.Must be root to use this programm. \n");
        exit(1);
    }

    if ( argc == 1 ) {
	show_usage();
	exit(1);
    }

    if (!strcmp(argv[1],"-n"))
	{
	 str=&argv[2];
	 do_resolv=0;
	}
    else 
	str=&argv[1];

    if (str[0]==NULL)
	{
	show_usage();
	exit(1);
	}

    if ( strcmp(str[0],"list") == 0 ) {
      
      list_kernel_data();

    } else if ( strcmp(str[0],"flush") == 0 ) {

	(void)do_setsockopt( socket_fd, IPPROTO_IP, 
                             IP_FW_FLUSH, NULL, 0, 0 );

    } else if ( strlen(str[0]) >= strlen("checkb")
    && strncmp(str[0],"checkblocking",strlen(str[0])) == 0 ) {

	check(IPF_BLOCKING,socket_fd,&str[1]);

    } else if ( strlen(str[0]) >= strlen("checkf")
    && strncmp(str[0],"checkforwarding",strlen(str[0])) == 0 ) {

	check(IPF_FORWARDING,socket_fd,&str[1]);

    } else if ( strlen(str[0]) >= strlen("addb")
    && strncmp(str[0],"addblocking",strlen(str[0])) == 0 ) {

	add(IPF_BLOCKING,socket_fd,&str[1]);

    } else if ( strlen(str[0]) >= strlen("addf")
    && strncmp(str[0],"addforwarding",strlen(str[0])) == 0 ) {

	add(IPF_FORWARDING,socket_fd,&str[1]);

    } else if ( strlen(str[0]) >= strlen("delb")
    && strncmp(str[0],"delblocking",strlen(str[0])) == 0 ) {

	del(IPF_BLOCKING,socket_fd,&str[1]);

    } else if ( strlen(str[0]) >= strlen("delf")
    && strncmp(str[0],"delforwarding",strlen(str[0])) == 0 ) {

	del(IPF_FORWARDING,socket_fd,&str[1]);

    } else if ( strlen(str[0]) >= strlen("poli")
    && strncmp(str[0],"policy",strlen(str[0])) == 0 ) {

	policy(socket_fd,&str[1]);

    } else {

	fprintf(stderr,"ipfw: unknown command \"%s\"\n",str[1]);
	show_usage();
	exit(1);
    }

    exit(0);
}
