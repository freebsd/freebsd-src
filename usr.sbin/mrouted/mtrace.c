#include <netdb.h>
#include <sys/time.h>
#include "defs.h"

#define DEFAULT_TIMEOUT	10	/* How long to wait before retrying requests */
#define JAN_1970	2208988800	/* 1970 - 1900 in seconds */

int     timeout = DEFAULT_TIMEOUT;

vifi_t  numvifs;		/* to keep loader happy */
				/* (see COPY_TABLES macro called in kern.c) */


char   *
inet_name(addr)
	u_long  addr;
{
	struct hostent *e;

	e = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);

	return e ? e->h_name : "?";
}

u_long
host_addr(name)
	char   *name;
{
	struct hostent *e = gethostbyname(name);
	int     addr;

	if (e)
		memcpy(&addr, e->h_addr_list[0], e->h_length);
	else {
		addr = inet_addr(name);
		if (addr == -1)
			addr = 0;
	}

	return addr;
}
char *
proto_type(type)
    u_char type;
{
    switch (type) {
      case PROTO_DVMRP:
	return ("PROTO_DVMRP");
      case PROTO_MOSPF:
	return ("PROTO_MOSPF");
      case PROTO_PIM:
	return ("PROTO_PIM");
      case PROTO_CBT:
	return ("PROTO_CBT");
      default:
	return ("PROTO_UNKNOWN");
    }
}

char *
flag_type(type)
    u_char type;
{
    switch (type) {
      case TR_NO_ERR:
	return ("NO_ERR");
      case TR_WRONG_IF:
	return ("WRONG_IF");
      case TR_PRUNED:
	return ("PRUNED");
      case TR_SCOPED:
	return ("SCOPED");
      case TR_NO_RTE:
	return ("NO_RTE");
      default:
	return ("INVALID ERR");
    }
}

int
t_diff(a, b)
    u_long a, b;
{
    int d = a - b;

    return ((d * 125) >> 13);
}

main(argc, argv)
int argc;
char *argv[];
{
    struct timeval tq;
    struct timezone tzp;
    u_long querytime, resptime;

    int udp;
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    u_long  lcl_addr = 0;	/* in NET order */

    u_long qid  = ((u_long)random() >> 8);
    u_long qsrc = NULL;
    u_long qgrp = NULL;
    u_long qdst = NULL;
    u_char qno  = 0;
    u_long raddr = NULL;
    u_char qttl = 1;
    u_char rttl = 1;
    u_long dst = NULL;

    struct tr_query *query;

    struct tr_rlist *tr_rlist = NULL;

    char *p;
    int datalen = 0;

    int i;
    int done = 0;

    if (geteuid() != 0) {
	fprintf(stderr, "must be root\n");
	exit(1);
    }

    argv++, argc--;

    if (argc == 0) goto usage;

    while (argc > 0 && *argv[0] == '-') {
	switch (argv[0][1]) {
	  case 's':
	    if (argc > 1 && isdigit(*(argv + 1)[0])) {
		argv++, argc--;
		qsrc = host_addr(argv[0]);
		break;
	    } else
		goto usage;
	  case 'g':
	    if (argc > 1 && isdigit(*(argv + 1)[0])) {
		argv++, argc--;
		qgrp = host_addr(argv[0]);
		break;
	    } else
		goto usage;
	  case 'd':
	    if (argc > 1 && isdigit(*(argv + 1)[0])) {
		argv++, argc--;
		qdst = host_addr(argv[0]);
		break;
	    } else
		goto usage;
	  case 'x':
	    if (argc > 1 && isdigit(*(argv + 1)[0])) {
		argv++, argc--;
		dst = host_addr(argv[0]);
		break;
	    } else
		goto usage;
	  case 't':
	    if (argc > 1 && isdigit(*(argv + 1)[0])) {
		argv++, argc--;
		qttl = atoi(argv[0]);
		if (qttl < 1)
		    qttl = 1;
		break;
	    } else
		goto usage;
	  case 'n':
	    if (argc > 1 && isdigit(*(argv + 1)[0])) {
		argv++, argc--;
		qno = atoi(argv[0]);
		break;
	    } else
		goto usage;
	  case 'l':
	    if (argc > 1 && isdigit(*(argv + 1)[0])) {
		argv++, argc--;
		rttl = atoi(argv[0]);
		break;
	    } else
		goto usage;
	  case 'r':
	    if (argc > 1 && isdigit(*(argv + 1)[0])) {
		argv++, argc--;
		raddr = host_addr(argv[0]);
		break;
	    } else
		goto usage;
	  default:
	    goto usage;
	}
	argv++, argc--;
    }

    if (argc > 0) {
usage:	printf("usage: mtrace -s <src> -g <grp> -d <dst> -n <# reports> \n");
	printf("              -t <ttl> [-x <qdst>] [-r <rdst>] [-l <rttl>]\n");
	exit(1);
    }

    printf("Mtrace src %s grp %s dst %s #%d\n", inet_fmt(qsrc, s1),
	   inet_fmt(qgrp, s2), inet_fmt(qdst, s3), qno);
    printf("       resp ttl %d resp addr %s\n", rttl, inet_fmt(raddr, s1));

    init_igmp();

    /* Obtain the local address from which to send out packets */

    addr.sin_family = AF_INET;
    addr.sin_len = sizeof addr;
    addr.sin_addr.s_addr = qgrp;
    addr.sin_port = htons(2000);

    if (((udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) ||
	(connect(udp, (struct sockaddr *) &addr, sizeof(addr)) < 0) ||
	getsockname(udp, (struct sockaddr *) &addr, &addrlen) < 0) {
	perror("Determining local address");
	exit(-1);
    }
    close(udp);
    lcl_addr = addr.sin_addr.s_addr;

    /* Got the local address now */
    /* Now, make up the IGMP packet to send */

    query = (struct tr_query *)(send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN);

    query->tr_src   = qsrc;
    query->tr_dst   = qdst;
    query->tr_qid   = qid;
    if (raddr)
	query->tr_raddr = raddr;
    else
	query->tr_raddr  = lcl_addr;
    query->tr_rttl  = rttl;

    datalen += sizeof(struct tr_query);

    if (IN_MULTICAST(ntohl(qgrp)))
	k_set_ttl(qttl);
    else
	k_set_ttl(1);

    if (dst == NULL)
	dst = qgrp;

    /*
     * set timer to calculate delays & send query
     */
    gettimeofday(&tq, &tzp);
    querytime = ((tq.tv_sec + JAN_1970) << 16) + (tq.tv_usec << 10) / 15625;

    send_igmp(lcl_addr, dst, IGMP_MTRACE, qno,
	      qgrp, datalen);

    /*
     * If the response is to be a multicast address, make sure we
     * are listening on that multicast address.
     */
    if (IN_MULTICAST(ntohl(raddr)))
	k_join(raddr, lcl_addr);

    /* Wait for our reply now */
    while (!done) {
	fd_set  fds;
	struct timeval tv;
	struct timezone tzp;

	int     count, recvlen, dummy = 0;
	register u_long src, group, smask;
	struct ip *ip;
	struct igmp *igmp;
	struct tr_resp *resp;
	int ipdatalen, iphdrlen, igmpdatalen;
	int rno;

	FD_ZERO(&fds);
	FD_SET(igmp_socket, &fds);

	/* need to input timeout as optional argument */
	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	count = select(igmp_socket + 1, &fds, 0, 0, &tv);

	if (count < 0) {
	    if (errno != EINTR)
		perror("select");
	    continue;
	} else if (count == 0) {
	    printf("Timed out receiving responses\n");
	    exit(1);
	}

	gettimeofday(&tq, &tzp);
	resptime = ((tq.tv_sec + JAN_1970) << 16) + (tq.tv_usec << 10) / 15625;

	recvlen = recvfrom(igmp_socket, recv_buf, sizeof(recv_buf),
			   0, NULL, &dummy);

	if (recvlen <= 0) {
	    if (recvlen && errno != EINTR)
		perror("recvfrom");
	    continue;
	}

	if (recvlen < sizeof(struct ip)) {
	    log(LOG_WARNING, 0,
		"packet too short (%u bytes) for IP header",
		recvlen);
	    continue;
	}
	ip = (struct ip *) recv_buf;

	iphdrlen = ip->ip_hl << 2;
	ipdatalen = ip->ip_len;
	if (iphdrlen + ipdatalen != recvlen) {
	    printf("packet shorter (%u bytes) than hdr+data length (%u+%u)\n",
		   recvlen, iphdrlen, ipdatalen);
	    continue;
	}

	igmp = (struct igmp *) (recv_buf + iphdrlen);
	group = igmp->igmp_group.s_addr;
	igmpdatalen = ipdatalen - IGMP_MINLEN;
	if (igmpdatalen < 0) {
	    printf("IP data field too short (%u bytes) for IGMP, from %s\n",
		   ipdatalen, inet_fmt(src, s1));
	    continue;
	}

	if (igmp->igmp_type != IGMP_MTRACE &&
	    igmp->igmp_type != IGMP_MTRACE_RESP)
	    continue;

	if (igmpdatalen == QLEN)
	    continue;

	if ((igmpdatalen - QLEN)%RLEN) {
	    printf("packet with incorrect datalen\n");
	    continue;
	}

	query = (struct tr_query *)(igmp + 1);

	/* If this is query with a different id, ignore! */
	if (query->tr_qid != qid)
	    continue;

	/*
	 * Most of the sanity checking done at this point.
	 * This is the packet we have been waiting for all this time
	 */
	resp = (struct tr_resp *)(query + 1);

	rno = (igmpdatalen - QLEN)/RLEN;

	/*
	 * print the responses out in reverse order (from src to dst)
	 */
	printf("src: <%s>  grp: <%s>  dst: <%s>  rtt: %d ms\n\n",
	       inet_fmt(qsrc, s1), inet_fmt(qgrp, s2), inet_fmt(qdst, s3),
	       t_diff(resptime, querytime));

	VAL_TO_MASK(smask, (resp+rno-1)->tr_smask);

	if (((resp+rno-1)->tr_inaddr & smask) == (qsrc & smask))
	    printf("  %-15s \n", inet_fmt(qsrc, s1));
	else
	    printf("     * * *\n");

	while (rno--) {
	    struct tr_resp *r = resp + rno;

	    printf("          |          \n");
	    printf("  %-15s  ", inet_fmt(r->tr_inaddr, s1));
	    printf("ttl %d ", r->tr_fttl);
	    printf("cum: %d ms ", t_diff(r->tr_qarr, querytime));
	    printf("hop: %d ms ", t_diff(resptime, r->tr_qarr));
	    printf("%s ", proto_type(r->tr_rproto));
	    printf("%s\n", flag_type(r->tr_rflags));

	    printf("  %-15s  ", inet_fmt(r->tr_outaddr, s1));
	    printf("v_in: %ld ", r->tr_vifin);
	    printf("v_out: %ld ", r->tr_vifout);
	    printf("pkts: %ld\n", r->tr_pktcnt);

	    resptime = r->tr_qarr;
	}
	printf("          |          \n");
	printf("  %-15s \n", inet_fmt(qdst, s1));

	/*
	 * if the response was multicast back, leave the group
	 */
	if (IN_MULTICAST(ntohl(raddr)))
	    k_leave(raddr, lcl_addr);

	/* If I don't expect any more replies, exit here */
	exit(0);
    }
}
/* dummies */
void log()
{
}
void accept_probe()
{
}
void accept_group_report()
{
}
void accept_neighbors()
{
}
void accept_neighbors2()
{
}
void accept_neighbor_request2()
{
}
void accept_report()
{
}
void accept_neighbor_request()
{
}
void accept_prune()
{
}
void accept_graft()
{
}
void accept_g_ack()
{
}
void add_table_entry()
{
}
void check_vif_state()
{
}
void mtrace()
{
}
void leave_group_message()
{
}
