/*
 * Copyright (c) 2002-2003 Luigi Rizzo
 * Copyright (c) 1996 Alex Nash, Paul Traina, Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
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
 * NEW command line interface for IP firewall facility
 *
 * $FreeBSD$
 *
 * dummynet support
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
/* XXX there are several sysctl leftover here */
#include <sys/sysctl.h>

#include "ipfw2.h"

#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <net/if.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>
#include <arpa/inet.h>	/* inet_ntoa */

static struct _s_x dummynet_params[] = {
	{ "plr",		TOK_PLR },
	{ "noerror",		TOK_NOERROR },
	{ "buckets",		TOK_BUCKETS },
	{ "dst-ip",		TOK_DSTIP },
	{ "src-ip",		TOK_SRCIP },
	{ "dst-port",		TOK_DSTPORT },
	{ "src-port",		TOK_SRCPORT },
	{ "proto",		TOK_PROTO },
	{ "weight",		TOK_WEIGHT },
	{ "all",		TOK_ALL },
	{ "mask",		TOK_MASK },
	{ "droptail",		TOK_DROPTAIL },
	{ "red",		TOK_RED },
	{ "gred",		TOK_GRED },
	{ "bw",			TOK_BW },
	{ "bandwidth",		TOK_BW },
	{ "delay",		TOK_DELAY },
	{ "pipe",		TOK_PIPE },
	{ "queue",		TOK_QUEUE },
	{ "flow-id",		TOK_FLOWID},
	{ "dst-ipv6",		TOK_DSTIP6},
	{ "dst-ip6",		TOK_DSTIP6},
	{ "src-ipv6",		TOK_SRCIP6},
	{ "src-ip6",		TOK_SRCIP6},
	{ "dummynet-params",	TOK_NULL },
	{ NULL, 0 }	/* terminator */
};

static int
sort_q(const void *pa, const void *pb)
{
	int rev = (co.do_sort < 0);
	int field = rev ? -co.do_sort : co.do_sort;
	long long res = 0;
	const struct dn_flow_queue *a = pa;
	const struct dn_flow_queue *b = pb;

	switch (field) {
	case 1: /* pkts */
		res = a->len - b->len;
		break;
	case 2: /* bytes */
		res = a->len_bytes - b->len_bytes;
		break;

	case 3: /* tot pkts */
		res = a->tot_pkts - b->tot_pkts;
		break;

	case 4: /* tot bytes */
		res = a->tot_bytes - b->tot_bytes;
		break;
	}
	if (res < 0)
		res = -1;
	if (res > 0)
		res = 1;
	return (int)(rev ? res : -res);
}

static void
list_queues(struct dn_flow_set *fs, struct dn_flow_queue *q)
{
	int l;
	int index_printed, indexes = 0;
	char buff[255];
	struct protoent *pe;

	if (fs->rq_elements == 0)
		return;

	if (co.do_sort != 0)
		heapsort(q, fs->rq_elements, sizeof *q, sort_q);

	/* Print IPv4 flows */
	index_printed = 0;
	for (l = 0; l < fs->rq_elements; l++) {
		struct in_addr ina;

		/* XXX: Should check for IPv4 flows */
		if (IS_IP6_FLOW_ID(&(q[l].id)))
			continue;

		if (!index_printed) {
			index_printed = 1;
			if (indexes > 0)	/* currently a no-op */
				printf("\n");
			indexes++;
			printf("    "
			    "mask: 0x%02x 0x%08x/0x%04x -> 0x%08x/0x%04x\n",
			    fs->flow_mask.proto,
			    fs->flow_mask.src_ip, fs->flow_mask.src_port,
			    fs->flow_mask.dst_ip, fs->flow_mask.dst_port);

			printf("BKT Prot ___Source IP/port____ "
			    "____Dest. IP/port____ "
			    "Tot_pkt/bytes Pkt/Byte Drp\n");
		}

		printf("%3d ", q[l].hash_slot);
		pe = getprotobynumber(q[l].id.proto);
		if (pe)
			printf("%-4s ", pe->p_name);
		else
			printf("%4u ", q[l].id.proto);
		ina.s_addr = htonl(q[l].id.src_ip);
		printf("%15s/%-5d ",
		    inet_ntoa(ina), q[l].id.src_port);
		ina.s_addr = htonl(q[l].id.dst_ip);
		printf("%15s/%-5d ",
		    inet_ntoa(ina), q[l].id.dst_port);
		printf("%4llu %8llu %2u %4u %3u\n",
		    align_uint64(&q[l].tot_pkts),
		    align_uint64(&q[l].tot_bytes),
		    q[l].len, q[l].len_bytes, q[l].drops);
		if (co.verbose)
			printf("   S %20llu  F %20llu\n",
			    align_uint64(&q[l].S), align_uint64(&q[l].F));
	}

	/* Print IPv6 flows */
	index_printed = 0;
	for (l = 0; l < fs->rq_elements; l++) {
		if (!IS_IP6_FLOW_ID(&(q[l].id)))
			continue;

		if (!index_printed) {
			index_printed = 1;
			if (indexes > 0)
				printf("\n");
			indexes++;
			printf("\n        mask: proto: 0x%02x, flow_id: 0x%08x,  ",
			    fs->flow_mask.proto, fs->flow_mask.flow_id6);
			inet_ntop(AF_INET6, &(fs->flow_mask.src_ip6),
			    buff, sizeof(buff));
			printf("%s/0x%04x -> ", buff, fs->flow_mask.src_port);
			inet_ntop( AF_INET6, &(fs->flow_mask.dst_ip6),
			    buff, sizeof(buff) );
			printf("%s/0x%04x\n", buff, fs->flow_mask.dst_port);

			printf("BKT ___Prot___ _flow-id_ "
			    "______________Source IPv6/port_______________ "
			    "_______________Dest. IPv6/port_______________ "
			    "Tot_pkt/bytes Pkt/Byte Drp\n");
		}
		printf("%3d ", q[l].hash_slot);
		pe = getprotobynumber(q[l].id.proto);
		if (pe != NULL)
			printf("%9s ", pe->p_name);
		else
			printf("%9u ", q[l].id.proto);
		printf("%7d  %39s/%-5d ", q[l].id.flow_id6,
		    inet_ntop(AF_INET6, &(q[l].id.src_ip6), buff, sizeof(buff)),
		    q[l].id.src_port);
		printf(" %39s/%-5d ",
		    inet_ntop(AF_INET6, &(q[l].id.dst_ip6), buff, sizeof(buff)),
		    q[l].id.dst_port);
		printf(" %4llu %8llu %2u %4u %3u\n",
		    align_uint64(&q[l].tot_pkts),
		    align_uint64(&q[l].tot_bytes),
		    q[l].len, q[l].len_bytes, q[l].drops);
		if (co.verbose)
			printf("   S %20llu  F %20llu\n",
			    align_uint64(&q[l].S),
			    align_uint64(&q[l].F));
	}
}

static void
print_flowset_parms(struct dn_flow_set *fs, char *prefix)
{
	int l;
	char qs[30];
	char plr[30];
	char red[90];	/* Display RED parameters */

	l = fs->qsize;
	if (fs->flags_fs & DN_QSIZE_IS_BYTES) {
		if (l >= 8192)
			sprintf(qs, "%d KB", l / 1024);
		else
			sprintf(qs, "%d B", l);
	} else
		sprintf(qs, "%3d sl.", l);
	if (fs->plr)
		sprintf(plr, "plr %f", 1.0 * fs->plr / (double)(0x7fffffff));
	else
		plr[0] = '\0';
	if (fs->flags_fs & DN_IS_RED)	/* RED parameters */
		sprintf(red,
		    "\n\t  %cRED w_q %f min_th %d max_th %d max_p %f",
		    (fs->flags_fs & DN_IS_GENTLE_RED) ? 'G' : ' ',
		    1.0 * fs->w_q / (double)(1 << SCALE_RED),
		    SCALE_VAL(fs->min_th),
		    SCALE_VAL(fs->max_th),
		    1.0 * fs->max_p / (double)(1 << SCALE_RED));
	else
		sprintf(red, "droptail");

	printf("%s %s%s %d queues (%d buckets) %s\n",
	    prefix, qs, plr, fs->rq_elements, fs->rq_size, red);
}

void
ipfw_list_pipes(void *data, uint nbytes, int ac, char *av[])
{
	int rulenum;
	void *next = data;
	struct dn_pipe *p = (struct dn_pipe *) data;
	struct dn_flow_set *fs;
	struct dn_flow_queue *q;
	int l;

	if (ac > 0)
		rulenum = strtoul(*av++, NULL, 10);
	else
		rulenum = 0;
	for (; nbytes >= sizeof *p; p = (struct dn_pipe *)next) {
		double b = p->bandwidth;
		char buf[30];
		char prefix[80];

		if (SLIST_NEXT(p, next) != (struct dn_pipe *)DN_IS_PIPE)
			break;	/* done with pipes, now queues */

		/*
		 * compute length, as pipe have variable size
		 */
		l = sizeof(*p) + p->fs.rq_elements * sizeof(*q);
		next = (char *)p + l;
		nbytes -= l;

		if ((rulenum != 0 && rulenum != p->pipe_nr) || co.do_pipe == 2)
			continue;

		/*
		 * Print rate (or clocking interface)
		 */
		if (p->if_name[0] != '\0')
			sprintf(buf, "%s", p->if_name);
		else if (b == 0)
			sprintf(buf, "unlimited");
		else if (b >= 1000000)
			sprintf(buf, "%7.3f Mbit/s", b/1000000);
		else if (b >= 1000)
			sprintf(buf, "%7.3f Kbit/s", b/1000);
		else
			sprintf(buf, "%7.3f bit/s ", b);

		sprintf(prefix, "%05d: %s %4d ms ",
		    p->pipe_nr, buf, p->delay);
		print_flowset_parms(&(p->fs), prefix);
		if (co.verbose)
			printf("   V %20llu\n", align_uint64(&p->V) >> MY_M);

		q = (struct dn_flow_queue *)(p+1);
		list_queues(&(p->fs), q);
	}
	for (fs = next; nbytes >= sizeof *fs; fs = next) {
		char prefix[80];

		if (SLIST_NEXT(fs, next) != (struct dn_flow_set *)DN_IS_QUEUE)
			break;
		l = sizeof(*fs) + fs->rq_elements * sizeof(*q);
		next = (char *)fs + l;
		nbytes -= l;

		if (rulenum != 0 && ((rulenum != fs->fs_nr && co.do_pipe == 2) ||
		    (rulenum != fs->parent_nr && co.do_pipe == 1))) {
			continue;
		}

		q = (struct dn_flow_queue *)(fs+1);
		sprintf(prefix, "q%05d: weight %d pipe %d ",
		    fs->fs_nr, fs->weight, fs->parent_nr);
		print_flowset_parms(fs, prefix);
		list_queues(fs, q);
	}
}

/*
 * Delete pipe or queue i
 */
int
ipfw_delete_pipe(int pipe_or_queue, int i)
{
	struct dn_pipe p;

	memset(&p, 0, sizeof p);
	if (pipe_or_queue == 1)
		p.pipe_nr = i;		/* pipe */
	else
		p.fs.fs_nr = i;		/* queue */
	i = do_cmd(IP_DUMMYNET_DEL, &p, sizeof p);
	if (i) {
		i = 1;
		warn("rule %u: setsockopt(IP_DUMMYNET_DEL)", i);
	}
	return i;
}

void
ipfw_config_pipe(int ac, char **av)
{
	struct dn_pipe p;
	int i;
	char *end;
	void *par = NULL;

	memset(&p, 0, sizeof p);

	av++; ac--;
	/* Pipe number */
	if (ac && isdigit(**av)) {
		i = atoi(*av); av++; ac--;
		if (co.do_pipe == 1)
			p.pipe_nr = i;
		else
			p.fs.fs_nr = i;
	}
	while (ac > 0) {
		double d;
		int tok = match_token(dummynet_params, *av);
		ac--; av++;

		switch(tok) {
		case TOK_NOERROR:
			p.fs.flags_fs |= DN_NOERROR;
			break;

		case TOK_PLR:
			NEED1("plr needs argument 0..1\n");
			d = strtod(av[0], NULL);
			if (d > 1)
				d = 1;
			else if (d < 0)
				d = 0;
			p.fs.plr = (int)(d*0x7fffffff);
			ac--; av++;
			break;

		case TOK_QUEUE:
			NEED1("queue needs queue size\n");
			end = NULL;
			p.fs.qsize = strtoul(av[0], &end, 0);
			if (*end == 'K' || *end == 'k') {
				p.fs.flags_fs |= DN_QSIZE_IS_BYTES;
				p.fs.qsize *= 1024;
			} else if (*end == 'B' ||
			    _substrcmp2(end, "by", "bytes") == 0) {
				p.fs.flags_fs |= DN_QSIZE_IS_BYTES;
			}
			ac--; av++;
			break;

		case TOK_BUCKETS:
			NEED1("buckets needs argument\n");
			p.fs.rq_size = strtoul(av[0], NULL, 0);
			ac--; av++;
			break;

		case TOK_MASK:
			NEED1("mask needs mask specifier\n");
			/*
			 * per-flow queue, mask is dst_ip, dst_port,
			 * src_ip, src_port, proto measured in bits
			 */
			par = NULL;

			bzero(&p.fs.flow_mask, sizeof(p.fs.flow_mask));
			end = NULL;

			while (ac >= 1) {
			    uint32_t *p32 = NULL;
			    uint16_t *p16 = NULL;
			    uint32_t *p20 = NULL;
			    struct in6_addr *pa6 = NULL;
			    uint32_t a;

			    tok = match_token(dummynet_params, *av);
			    ac--; av++;
			    switch(tok) {
			    case TOK_ALL:
				    /*
				     * special case, all bits significant
				     */
				    p.fs.flow_mask.dst_ip = ~0;
				    p.fs.flow_mask.src_ip = ~0;
				    p.fs.flow_mask.dst_port = ~0;
				    p.fs.flow_mask.src_port = ~0;
				    p.fs.flow_mask.proto = ~0;
				    n2mask(&(p.fs.flow_mask.dst_ip6), 128);
				    n2mask(&(p.fs.flow_mask.src_ip6), 128);
				    p.fs.flow_mask.flow_id6 = ~0;
				    p.fs.flags_fs |= DN_HAVE_FLOW_MASK;
				    goto end_mask;

			    case TOK_DSTIP:
				    p32 = &p.fs.flow_mask.dst_ip;
				    break;

			    case TOK_SRCIP:
				    p32 = &p.fs.flow_mask.src_ip;
				    break;

			    case TOK_DSTIP6:
				    pa6 = &(p.fs.flow_mask.dst_ip6);
				    break;
			    
			    case TOK_SRCIP6:
				    pa6 = &(p.fs.flow_mask.src_ip6);
				    break;

			    case TOK_FLOWID:
				    p20 = &p.fs.flow_mask.flow_id6;
				    break;

			    case TOK_DSTPORT:
				    p16 = &p.fs.flow_mask.dst_port;
				    break;

			    case TOK_SRCPORT:
				    p16 = &p.fs.flow_mask.src_port;
				    break;

			    case TOK_PROTO:
				    break;

			    default:
				    ac++; av--; /* backtrack */
				    goto end_mask;
			    }
			    if (ac < 1)
				    errx(EX_USAGE, "mask: value missing");
			    if (*av[0] == '/') {
				    a = strtoul(av[0]+1, &end, 0);
				    if (pa6 == NULL)
					    a = (a == 32) ? ~0 : (1 << a) - 1;
			    } else
				    a = strtoul(av[0], &end, 0);
			    if (p32 != NULL)
				    *p32 = a;
			    else if (p16 != NULL) {
				    if (a > 0xFFFF)
					    errx(EX_DATAERR,
						"port mask must be 16 bit");
				    *p16 = (uint16_t)a;
			    } else if (p20 != NULL) {
				    if (a > 0xfffff)
					errx(EX_DATAERR,
					    "flow_id mask must be 20 bit");
				    *p20 = (uint32_t)a;
			    } else if (pa6 != NULL) {
				    if (a > 128)
					errx(EX_DATAERR,
					    "in6addr invalid mask len");
				    else
					n2mask(pa6, a);
			    } else {
				    if (a > 0xFF)
					    errx(EX_DATAERR,
						"proto mask must be 8 bit");
				    p.fs.flow_mask.proto = (uint8_t)a;
			    }
			    if (a != 0)
				    p.fs.flags_fs |= DN_HAVE_FLOW_MASK;
			    ac--; av++;
			} /* end while, config masks */
end_mask:
			break;

		case TOK_RED:
		case TOK_GRED:
			NEED1("red/gred needs w_q/min_th/max_th/max_p\n");
			p.fs.flags_fs |= DN_IS_RED;
			if (tok == TOK_GRED)
				p.fs.flags_fs |= DN_IS_GENTLE_RED;
			/*
			 * the format for parameters is w_q/min_th/max_th/max_p
			 */
			if ((end = strsep(&av[0], "/"))) {
			    double w_q = strtod(end, NULL);
			    if (w_q > 1 || w_q <= 0)
				errx(EX_DATAERR, "0 < w_q <= 1");
			    p.fs.w_q = (int) (w_q * (1 << SCALE_RED));
			}
			if ((end = strsep(&av[0], "/"))) {
			    p.fs.min_th = strtoul(end, &end, 0);
			    if (*end == 'K' || *end == 'k')
				p.fs.min_th *= 1024;
			}
			if ((end = strsep(&av[0], "/"))) {
			    p.fs.max_th = strtoul(end, &end, 0);
			    if (*end == 'K' || *end == 'k')
				p.fs.max_th *= 1024;
			}
			if ((end = strsep(&av[0], "/"))) {
			    double max_p = strtod(end, NULL);
			    if (max_p > 1 || max_p <= 0)
				errx(EX_DATAERR, "0 < max_p <= 1");
			    p.fs.max_p = (int)(max_p * (1 << SCALE_RED));
			}
			ac--; av++;
			break;

		case TOK_DROPTAIL:
			p.fs.flags_fs &= ~(DN_IS_RED|DN_IS_GENTLE_RED);
			break;

		case TOK_BW:
			NEED1("bw needs bandwidth or interface\n");
			if (co.do_pipe != 1)
			    errx(EX_DATAERR, "bandwidth only valid for pipes");
			/*
			 * set clocking interface or bandwidth value
			 */
			if (av[0][0] >= 'a' && av[0][0] <= 'z') {
			    int l = sizeof(p.if_name)-1;
			    /* interface name */
			    strncpy(p.if_name, av[0], l);
			    p.if_name[l] = '\0';
			    p.bandwidth = 0;
			} else {
			    p.if_name[0] = '\0';
			    p.bandwidth = strtoul(av[0], &end, 0);
			    if (*end == 'K' || *end == 'k') {
				end++;
				p.bandwidth *= 1000;
			    } else if (*end == 'M') {
				end++;
				p.bandwidth *= 1000000;
			    }
			    if ((*end == 'B' &&
				  _substrcmp2(end, "Bi", "Bit/s") != 0) ||
			        _substrcmp2(end, "by", "bytes") == 0)
				p.bandwidth *= 8;
			    if (p.bandwidth < 0)
				errx(EX_DATAERR, "bandwidth too large");
			}
			ac--; av++;
			break;

		case TOK_DELAY:
			if (co.do_pipe != 1)
				errx(EX_DATAERR, "delay only valid for pipes");
			NEED1("delay needs argument 0..10000ms\n");
			p.delay = strtoul(av[0], NULL, 0);
			ac--; av++;
			break;

		case TOK_WEIGHT:
			if (co.do_pipe == 1)
				errx(EX_DATAERR,"weight only valid for queues");
			NEED1("weight needs argument 0..100\n");
			p.fs.weight = strtoul(av[0], &end, 0);
			ac--; av++;
			break;

		case TOK_PIPE:
			if (co.do_pipe == 1)
				errx(EX_DATAERR,"pipe only valid for queues");
			NEED1("pipe needs pipe_number\n");
			p.fs.parent_nr = strtoul(av[0], &end, 0);
			ac--; av++;
			break;

		default:
			errx(EX_DATAERR, "unrecognised option ``%s''", av[-1]);
		}
	}
	if (co.do_pipe == 1) {
		if (p.pipe_nr == 0)
			errx(EX_DATAERR, "pipe_nr must be > 0");
		if (p.delay > 10000)
			errx(EX_DATAERR, "delay must be < 10000");
	} else { /* co.do_pipe == 2, queue */
		if (p.fs.parent_nr == 0)
			errx(EX_DATAERR, "pipe must be > 0");
		if (p.fs.weight >100)
			errx(EX_DATAERR, "weight must be <= 100");
	}
	if (p.fs.flags_fs & DN_QSIZE_IS_BYTES) {
		size_t len;
		long limit;

		len = sizeof(limit);
		if (sysctlbyname("net.inet.ip.dummynet.pipe_byte_limit",
			&limit, &len, NULL, 0) == -1)
			limit = 1024*1024;
		if (p.fs.qsize > limit)
			errx(EX_DATAERR, "queue size must be < %ldB", limit);
	} else {
		size_t len;
		long limit;

		len = sizeof(limit);
		if (sysctlbyname("net.inet.ip.dummynet.pipe_slot_limit",
			&limit, &len, NULL, 0) == -1)
			limit = 100;
		if (p.fs.qsize > limit)
			errx(EX_DATAERR, "2 <= queue size <= %ld", limit);
	}
	if (p.fs.flags_fs & DN_IS_RED) {
		size_t len;
		int lookup_depth, avg_pkt_size;
		double s, idle, weight, w_q;
		struct clockinfo ck;
		int t;

		if (p.fs.min_th >= p.fs.max_th)
		    errx(EX_DATAERR, "min_th %d must be < than max_th %d",
			p.fs.min_th, p.fs.max_th);
		if (p.fs.max_th == 0)
		    errx(EX_DATAERR, "max_th must be > 0");

		len = sizeof(int);
		if (sysctlbyname("net.inet.ip.dummynet.red_lookup_depth",
			&lookup_depth, &len, NULL, 0) == -1)
		    errx(1, "sysctlbyname(\"%s\")",
			"net.inet.ip.dummynet.red_lookup_depth");
		if (lookup_depth == 0)
		    errx(EX_DATAERR, "net.inet.ip.dummynet.red_lookup_depth"
			" must be greater than zero");

		len = sizeof(int);
		if (sysctlbyname("net.inet.ip.dummynet.red_avg_pkt_size",
			&avg_pkt_size, &len, NULL, 0) == -1)

		    errx(1, "sysctlbyname(\"%s\")",
			"net.inet.ip.dummynet.red_avg_pkt_size");
		if (avg_pkt_size == 0)
			errx(EX_DATAERR,
			    "net.inet.ip.dummynet.red_avg_pkt_size must"
			    " be greater than zero");

		len = sizeof(struct clockinfo);
		if (sysctlbyname("kern.clockrate", &ck, &len, NULL, 0) == -1)
			errx(1, "sysctlbyname(\"%s\")", "kern.clockrate");

		/*
		 * Ticks needed for sending a medium-sized packet.
		 * Unfortunately, when we are configuring a WF2Q+ queue, we
		 * do not have bandwidth information, because that is stored
		 * in the parent pipe, and also we have multiple queues
		 * competing for it. So we set s=0, which is not very
		 * correct. But on the other hand, why do we want RED with
		 * WF2Q+ ?
		 */
		if (p.bandwidth==0) /* this is a WF2Q+ queue */
			s = 0;
		else
			s = (double)ck.hz * avg_pkt_size * 8 / p.bandwidth;

		/*
		 * max idle time (in ticks) before avg queue size becomes 0.
		 * NOTA:  (3/w_q) is approx the value x so that
		 * (1-w_q)^x < 10^-3.
		 */
		w_q = ((double)p.fs.w_q) / (1 << SCALE_RED);
		idle = s * 3. / w_q;
		p.fs.lookup_step = (int)idle / lookup_depth;
		if (!p.fs.lookup_step)
			p.fs.lookup_step = 1;
		weight = 1 - w_q;
		for (t = p.fs.lookup_step; t > 1; --t)
			weight *= 1 - w_q;
		p.fs.lookup_weight = (int)(weight * (1 << SCALE_RED));
	}
	i = do_cmd(IP_DUMMYNET_CONFIGURE, &p, sizeof p);
	if (i)
		err(1, "setsockopt(%s)", "IP_DUMMYNET_CONFIGURE");
}
