/*
 * Copyright (c) 2002-2003,2010 Luigi Rizzo
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
 * $FreeBSD$
 *
 * dummynet support
 */

#include <sys/types.h>
#include <sys/socket.h>
/* XXX there are several sysctl leftover here */
#include <sys/sysctl.h>

#include "ipfw2.h"

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

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
	{ "lmax",		TOK_LMAX },
	{ "maxlen",		TOK_LMAX },
	{ "all",		TOK_ALL },
	{ "mask",		TOK_MASK }, /* alias for both */
	{ "sched_mask",		TOK_SCHED_MASK },
	{ "flow_mask",		TOK_FLOW_MASK },
	{ "droptail",		TOK_DROPTAIL },
	{ "red",		TOK_RED },
	{ "gred",		TOK_GRED },
	{ "bw",			TOK_BW },
	{ "bandwidth",		TOK_BW },
	{ "delay",		TOK_DELAY },
	{ "link",		TOK_LINK },
	{ "pipe",		TOK_PIPE },
	{ "queue",		TOK_QUEUE },
	{ "flowset",		TOK_FLOWSET },
	{ "sched",		TOK_SCHED },
	{ "pri",		TOK_PRI },
	{ "priority",		TOK_PRI },
	{ "type",		TOK_TYPE },
	{ "flow-id",		TOK_FLOWID},
	{ "dst-ipv6",		TOK_DSTIP6},
	{ "dst-ip6",		TOK_DSTIP6},
	{ "src-ipv6",		TOK_SRCIP6},
	{ "src-ip6",		TOK_SRCIP6},
	{ "profile",		TOK_PROFILE},
	{ "burst",		TOK_BURST},
	{ "dummynet-params",	TOK_NULL },
	{ NULL, 0 }	/* terminator */
};

#define O_NEXT(p, len) ((void *)((char *)p + len))

static void
oid_fill(struct dn_id *oid, int len, int type, uintptr_t id)
{
	oid->len = len;
	oid->type = type;
	oid->subtype = 0;
	oid->id = id;
}

/* make room in the buffer and move the pointer forward */
static void *
o_next(struct dn_id **o, int len, int type)
{
	struct dn_id *ret = *o;
	oid_fill(ret, len, type, 0);
	*o = O_NEXT(*o, len);
	return ret;
}

#if 0
static int
sort_q(void *arg, const void *pa, const void *pb)
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
#endif

/* print a mask and header for the subsequent list of flows */
static void
print_mask(struct ipfw_flow_id *id)
{
	if (!IS_IP6_FLOW_ID(id)) {
		printf("    "
		    "mask: %s 0x%02x 0x%08x/0x%04x -> 0x%08x/0x%04x\n",
		    id->extra ? "queue," : "",
		    id->proto,
		    id->src_ip, id->src_port,
		    id->dst_ip, id->dst_port);
	} else {
		char buf[255];
		printf("\n        mask: %sproto: 0x%02x, flow_id: 0x%08x,  ",
		    id->extra ? "queue," : "",
		    id->proto, id->flow_id6);
		inet_ntop(AF_INET6, &(id->src_ip6), buf, sizeof(buf));
		printf("%s/0x%04x -> ", buf, id->src_port);
		inet_ntop(AF_INET6, &(id->dst_ip6), buf, sizeof(buf));
		printf("%s/0x%04x\n", buf, id->dst_port);
	}
}

static void
print_header(struct ipfw_flow_id *id)
{
	if (!IS_IP6_FLOW_ID(id))
		printf("BKT Prot ___Source IP/port____ "
		    "____Dest. IP/port____ "
		    "Tot_pkt/bytes Pkt/Byte Drp\n");
	else
		printf("BKT ___Prot___ _flow-id_ "
		    "______________Source IPv6/port_______________ "
		    "_______________Dest. IPv6/port_______________ "
		    "Tot_pkt/bytes Pkt/Byte Drp\n");
}

static void
list_flow(struct dn_flow *ni, int *print)
{
	char buff[255];
	struct protoent *pe = NULL;
	struct in_addr ina;
	struct ipfw_flow_id *id = &ni->fid;

	if (*print) {
		print_header(&ni->fid);
		*print = 0;
	}
	pe = getprotobynumber(id->proto);
		/* XXX: Should check for IPv4 flows */
	printf("%3u%c", (ni->oid.id) & 0xff,
		id->extra ? '*' : ' ');
	if (!IS_IP6_FLOW_ID(id)) {
		if (pe)
			printf("%-4s ", pe->p_name);
		else
			printf("%4u ", id->proto);
		ina.s_addr = htonl(id->src_ip);
		printf("%15s/%-5d ",
		    inet_ntoa(ina), id->src_port);
		ina.s_addr = htonl(id->dst_ip);
		printf("%15s/%-5d ",
		    inet_ntoa(ina), id->dst_port);
	} else {
		/* Print IPv6 flows */
		if (pe != NULL)
			printf("%9s ", pe->p_name);
		else
			printf("%9u ", id->proto);
		printf("%7d  %39s/%-5d ", id->flow_id6,
		    inet_ntop(AF_INET6, &(id->src_ip6), buff, sizeof(buff)),
		    id->src_port);
		printf(" %39s/%-5d ",
		    inet_ntop(AF_INET6, &(id->dst_ip6), buff, sizeof(buff)),
		    id->dst_port);
	}
	pr_u64(&ni->tot_pkts, 4);
	pr_u64(&ni->tot_bytes, 8);
	printf("%2u %4u %3u\n",
	    ni->length, ni->len_bytes, ni->drops);
}

static void
print_flowset_parms(struct dn_fs *fs, char *prefix)
{
	int l;
	char qs[30];
	char plr[30];
	char red[90];	/* Display RED parameters */

	l = fs->qsize;
	if (fs->flags & DN_QSIZE_BYTES) {
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

	if (fs->flags & DN_IS_RED)	/* RED parameters */
		sprintf(red,
		    "\n\t %cRED w_q %f min_th %d max_th %d max_p %f",
		    (fs->flags & DN_IS_GENTLE_RED) ? 'G' : ' ',
		    1.0 * fs->w_q / (double)(1 << SCALE_RED),
		    fs->min_th,
		    fs->max_th,
		    1.0 * fs->max_p / (double)(1 << SCALE_RED));
	else
		sprintf(red, "droptail");

	if (prefix[0]) {
	    printf("%s %s%s %d queues (%d buckets) %s\n",
		prefix, qs, plr, fs->oid.id, fs->buckets, red);
	    prefix[0] = '\0';
	} else {
	    printf("q%05d %s%s %d flows (%d buckets) sched %d "
			"weight %d lmax %d pri %d %s\n",
		fs->fs_nr, qs, plr, fs->oid.id, fs->buckets,
		fs->sched_nr, fs->par[0], fs->par[1], fs->par[2], red);
	    if (fs->flags & DN_HAVE_MASK)
		print_mask(&fs->flow_mask);
	}
}

static void
print_extra_delay_parms(struct dn_profile *p)
{
	double loss;
	if (p->samples_no <= 0)
		return;

	loss = p->loss_level;
	loss /= p->samples_no;
	printf("\t profile: name \"%s\" loss %f samples %d\n",
		p->name, loss, p->samples_no);
}

static void
flush_buf(char *buf)
{
	if (buf[0])
		printf("%s\n", buf);
	buf[0] = '\0';
}
	
/*
 * generic list routine. We expect objects in a specific order, i.e.
 * PIPES AND SCHEDULERS:
 *	link; scheduler; internal flowset if any; instances
 * we can tell a pipe from the number.
 *
 * FLOWSETS:
 *	flowset; queues;
 * link i (int queue); scheduler i; si(i) { flowsets() : queues }
 */
static void
list_pipes(struct dn_id *oid, struct dn_id *end)
{
    char buf[160];	/* pending buffer */
    int toPrint = 1;	/* print header */

    buf[0] = '\0';
    for (; oid != end; oid = O_NEXT(oid, oid->len)) {
	if (oid->len < sizeof(*oid))
		errx(1, "invalid oid len %d\n", oid->len);

	switch (oid->type) {
	default:
	    flush_buf(buf);
	    printf("unrecognized object %d size %d\n", oid->type, oid->len);
	    break;
	case DN_TEXT: /* list of attached flowsets */
	    {
		int i, l;
		struct {
			struct dn_id id;
			uint32_t p[0];
		} *d = (void *)oid;
		l = (oid->len - sizeof(*oid))/sizeof(d->p[0]);
		if (l == 0)
		    break;
		printf("   Children flowsets: ");
		for (i = 0; i < l; i++)
			printf("%u ", d->p[i]);
		printf("\n");
		break;
	    }
	case DN_CMD_GET:
	    if (co.verbose)
		printf("answer for cmd %d, len %d\n", oid->type, oid->id);
	    break;
	case DN_SCH: {
	    struct dn_sch *s = (struct dn_sch *)oid;
	    flush_buf(buf);
	    printf(" sched %d type %s flags 0x%x %d buckets %d active\n",
			s->sched_nr,
			s->name, s->flags, s->buckets, s->oid.id);
	    if (s->flags & DN_HAVE_MASK)
		print_mask(&s->sched_mask);
	    }
	    break;

	case DN_FLOW:
	    list_flow((struct dn_flow *)oid, &toPrint);
	    break;

	case DN_LINK: {
	    struct dn_link *p = (struct dn_link *)oid;
	    double b = p->bandwidth;
	    char bwbuf[30];
	    char burst[5 + 7];

	    /* This starts a new object so flush buffer */
	    flush_buf(buf);
	    /* data rate */
	    if (b == 0)
		sprintf(bwbuf, "unlimited     ");
	    else if (b >= 1000000)
		sprintf(bwbuf, "%7.3f Mbit/s", b/1000000);
	    else if (b >= 1000)
		sprintf(bwbuf, "%7.3f Kbit/s", b/1000);
	    else
		sprintf(bwbuf, "%7.3f bit/s ", b);

	    if (humanize_number(burst, sizeof(burst), p->burst,
		    "", HN_AUTOSCALE, 0) < 0 || co.verbose)
		sprintf(burst, "%d", (int)p->burst);
	    sprintf(buf, "%05d: %s %4d ms burst %s",
		p->link_nr % DN_MAX_ID, bwbuf, p->delay, burst);
	    }
	    break;

	case DN_FS:
	    print_flowset_parms((struct dn_fs *)oid, buf);
	    break;
	case DN_PROFILE:
	    flush_buf(buf);
	    print_extra_delay_parms((struct dn_profile *)oid);
	}
	flush_buf(buf); // XXX does it really go here ?
    }
}

/*
 * Delete pipe, queue or scheduler i
 */
int
ipfw_delete_pipe(int do_pipe, int i)
{
	struct {
		struct dn_id oid;
		uintptr_t a[1];	/* add more if we want a list */
	} cmd;
	oid_fill((void *)&cmd, sizeof(cmd), DN_CMD_DELETE, DN_API_VERSION);
	cmd.oid.subtype = (do_pipe == 1) ? DN_LINK :
		( (do_pipe == 2) ? DN_FS : DN_SCH);
	cmd.a[0] = i;
	i = do_cmd(IP_DUMMYNET3, &cmd, cmd.oid.len);
	if (i) {
		i = 1;
		warn("rule %u: setsockopt(IP_DUMMYNET_DEL)", i);
	}
	return i;
}

/*
 * Code to parse delay profiles.
 *
 * Some link types introduce extra delays in the transmission
 * of a packet, e.g. because of MAC level framing, contention on
 * the use of the channel, MAC level retransmissions and so on.
 * From our point of view, the channel is effectively unavailable
 * for this extra time, which is constant or variable depending
 * on the link type. Additionally, packets may be dropped after this
 * time (e.g. on a wireless link after too many retransmissions).
 * We can model the additional delay with an empirical curve
 * that represents its distribution.
 *
 *	cumulative probability
 *	1.0 ^
 *	    |
 *	L   +-- loss-level          x
 *	    |                 ******
 *	    |                *
 *	    |           *****
 *	    |          *
 *	    |        **
 *	    |       *                         
 *	    +-------*------------------->
 *			delay
 *
 * The empirical curve may have both vertical and horizontal lines.
 * Vertical lines represent constant delay for a range of
 * probabilities; horizontal lines correspond to a discontinuty
 * in the delay distribution: the link will use the largest delay
 * for a given probability.
 * 
 * To pass the curve to dummynet, we must store the parameters
 * in a file as described below, and issue the command
 *
 *      ipfw pipe <n> config ... bw XXX profile <filename> ...
 *
 * The file format is the following, with whitespace acting as
 * a separator and '#' indicating the beginning a comment:
 *
 *	samples N
 *		the number of samples used in the internal
 *		representation (2..1024; default 100);
 *
 *	loss-level L 
 *		The probability above which packets are lost.
 *               (0.0 <= L <= 1.0, default 1.0 i.e. no loss);
 *
 *	name identifier
 *		Optional a name (listed by "ipfw pipe show")
 *		to identify the distribution;
 *
 *	"delay prob" | "prob delay"
 *		One of these two lines is mandatory and defines
 *		the format of the following lines with data points.
 *
 *	XXX YYY
 *		2 or more lines representing points in the curve,
 *		with either delay or probability first, according
 *		to the chosen format.
 *		The unit for delay is milliseconds.
 *
 * Data points does not need to be ordered or equal to the number
 * specified in the "samples" line. ipfw will sort and interpolate
 * the curve as needed.
 *
 * Example of a profile file:
 
        name    bla_bla_bla
        samples 100
        loss-level    0.86
        prob    delay
        0       200	# minimum overhead is 200ms
        0.5     200
        0.5     300
        0.8     1000
        0.9     1300
        1       1300
 
 * Internally, we will convert the curve to a fixed number of
 * samples, and when it is time to transmit a packet we will
 * model the extra delay as extra bits in the packet.
 *
 */

#define ED_MAX_LINE_LEN	256+ED_MAX_NAME_LEN
#define ED_TOK_SAMPLES	"samples"
#define ED_TOK_LOSS	"loss-level"
#define ED_TOK_NAME	"name"
#define ED_TOK_DELAY	"delay"
#define ED_TOK_PROB	"prob"
#define ED_TOK_BW	"bw"
#define ED_SEPARATORS	" \t\n"
#define ED_MIN_SAMPLES_NO	2

/*
 * returns 1 if s is a non-negative number, with at least one '.'
 */
static int
is_valid_number(const char *s)
{
	int i, dots_found = 0;
	int len = strlen(s);

	for (i = 0; i<len; ++i)
		if (!isdigit(s[i]) && (s[i] !='.' || ++dots_found > 1))
			return 0;
	return 1;
}

/*
 * Take as input a string describing a bandwidth value
 * and return the numeric bandwidth value.
 * set clocking interface or bandwidth value
 */
static void
read_bandwidth(char *arg, int *bandwidth, char *if_name, int namelen)
{
	if (*bandwidth != -1)
		warnx("duplicate token, override bandwidth value!");

	if (arg[0] >= 'a' && arg[0] <= 'z') {
		if (!if_name) {
			errx(1, "no if support");
		}
		if (namelen >= IFNAMSIZ)
			warn("interface name truncated");
		namelen--;
		/* interface name */
		strncpy(if_name, arg, namelen);
		if_name[namelen] = '\0';
		*bandwidth = 0;
	} else {	/* read bandwidth value */
		int bw;
		char *end = NULL;

		bw = strtoul(arg, &end, 0);
		if (*end == 'K' || *end == 'k') {
			end++;
			bw *= 1000;
		} else if (*end == 'M' || *end == 'm') {
			end++;
			bw *= 1000000;
		}
		if ((*end == 'B' &&
			_substrcmp2(end, "Bi", "Bit/s") != 0) ||
		    _substrcmp2(end, "by", "bytes") == 0)
			bw *= 8;

		if (bw < 0)
			errx(EX_DATAERR, "bandwidth too large");

		*bandwidth = bw;
		if (if_name)
			if_name[0] = '\0';
	}
}

struct point {
	double prob;
	double delay;
};

static int
compare_points(const void *vp1, const void *vp2)
{
	const struct point *p1 = vp1;
	const struct point *p2 = vp2;
	double res = 0;

	res = p1->prob - p2->prob;
	if (res == 0)
		res = p1->delay - p2->delay;
	if (res < 0)
		return -1;
	else if (res > 0)
		return 1;
	else
		return 0;
}

#define ED_EFMT(s) EX_DATAERR,"error in %s at line %d: "#s,filename,lineno

static void
load_extra_delays(const char *filename, struct dn_profile *p,
	struct dn_link *link)
{
	char    line[ED_MAX_LINE_LEN];
	FILE    *f;
	int     lineno = 0;
	int     i;

	int     samples = -1;
	double  loss = -1.0;
	char    profile_name[ED_MAX_NAME_LEN];
	int     delay_first = -1;
	int     do_points = 0;
	struct point    points[ED_MAX_SAMPLES_NO];
	int     points_no = 0;

	/* XXX link never NULL? */
	p->link_nr = link->link_nr;

	profile_name[0] = '\0';
	f = fopen(filename, "r");
	if (f == NULL)
		err(EX_UNAVAILABLE, "fopen: %s", filename);

	while (fgets(line, ED_MAX_LINE_LEN, f)) {         /* read commands */
		char *s, *cur = line, *name = NULL, *arg = NULL;

		++lineno;

		/* parse the line */
		while (cur) {
			s = strsep(&cur, ED_SEPARATORS);
			if (s == NULL || *s == '#')
				break;
			if (*s == '\0')
				continue;
			if (arg)
				errx(ED_EFMT("too many arguments"));
			if (name == NULL)
				name = s;
			else
				arg = s;
		}
		if (name == NULL)	/* empty line */
			continue;
		if (arg == NULL)
			errx(ED_EFMT("missing arg for %s"), name);

		if (!strcasecmp(name, ED_TOK_SAMPLES)) {
		    if (samples > 0)
			errx(ED_EFMT("duplicate ``samples'' line"));
		    if (atoi(arg) <=0)
			errx(ED_EFMT("invalid number of samples"));
		    samples = atoi(arg);
		    if (samples>ED_MAX_SAMPLES_NO)
			    errx(ED_EFMT("too many samples, maximum is %d"),
				ED_MAX_SAMPLES_NO);
		    do_points = 0;
		} else if (!strcasecmp(name, ED_TOK_BW)) {
		    char buf[IFNAMSIZ];
		    read_bandwidth(arg, &link->bandwidth, buf, sizeof(buf));
		} else if (!strcasecmp(name, ED_TOK_LOSS)) {
		    if (loss != -1.0)
			errx(ED_EFMT("duplicated token: %s"), name);
		    if (!is_valid_number(arg))
			errx(ED_EFMT("invalid %s"), arg);
		    loss = atof(arg);
		    if (loss > 1)
			errx(ED_EFMT("%s greater than 1.0"), name);
		    do_points = 0;
		} else if (!strcasecmp(name, ED_TOK_NAME)) {
		    if (profile_name[0] != '\0')
			errx(ED_EFMT("duplicated token: %s"), name);
		    strncpy(profile_name, arg, sizeof(profile_name) - 1);
		    profile_name[sizeof(profile_name)-1] = '\0';
		    do_points = 0;
		} else if (!strcasecmp(name, ED_TOK_DELAY)) {
		    if (do_points)
			errx(ED_EFMT("duplicated token: %s"), name);
		    delay_first = 1;
		    do_points = 1;
		} else if (!strcasecmp(name, ED_TOK_PROB)) {
		    if (do_points)
			errx(ED_EFMT("duplicated token: %s"), name);
		    delay_first = 0;
		    do_points = 1;
		} else if (do_points) {
		    if (!is_valid_number(name) || !is_valid_number(arg))
			errx(ED_EFMT("invalid point found"));
		    if (delay_first) {
			points[points_no].delay = atof(name);
			points[points_no].prob = atof(arg);
		    } else {
			points[points_no].delay = atof(arg);
			points[points_no].prob = atof(name);
		    }
		    if (points[points_no].prob > 1.0)
			errx(ED_EFMT("probability greater than 1.0"));
		    ++points_no;
		} else {
		    errx(ED_EFMT("unrecognised command '%s'"), name);
		}
	}

	fclose (f);

	if (samples == -1) {
	    warnx("'%s' not found, assuming 100", ED_TOK_SAMPLES);
	    samples = 100;
	}

	if (loss == -1.0) {
	    warnx("'%s' not found, assuming no loss", ED_TOK_LOSS);
	    loss = 1;
	}

	/* make sure that there are enough points. */
	if (points_no < ED_MIN_SAMPLES_NO)
	    errx(ED_EFMT("too few samples, need at least %d"),
		ED_MIN_SAMPLES_NO);

	qsort(points, points_no, sizeof(struct point), compare_points);

	/* interpolation */
	for (i = 0; i<points_no-1; ++i) {
	    double y1 = points[i].prob * samples;
	    double x1 = points[i].delay;
	    double y2 = points[i+1].prob * samples;
	    double x2 = points[i+1].delay;

	    int ix = y1;
	    int stop = y2;

	    if (x1 == x2) {
		for (; ix<stop; ++ix)
		    p->samples[ix] = x1;
	    } else {
		double m = (y2-y1)/(x2-x1);
		double c = y1 - m*x1;
		for (; ix<stop ; ++ix)
		    p->samples[ix] = (ix - c)/m;
	    }
	}
	p->samples_no = samples;
	p->loss_level = loss * samples;
	strncpy(p->name, profile_name, sizeof(p->name));
}

/*
 * configuration of pipes, schedulers, flowsets.
 * When we configure a new scheduler, an empty pipe is created, so:
 * 
 * do_pipe = 1 -> "pipe N config ..." only for backward compatibility
 *	sched N+Delta type fifo sched_mask ...
 *	pipe N+Delta <parameters>
 *	flowset N+Delta pipe N+Delta (no parameters)
 *	sched N type wf2q+ sched_mask ...
 *	pipe N <parameters>
 *
 * do_pipe = 2 -> flowset N config
 *	flowset N parameters
 *
 * do_pipe = 3 -> sched N config
 *	sched N parameters (default no pipe)
 *	optional Pipe N config ...
 * pipe ==>
 */
void
ipfw_config_pipe(int ac, char **av)
{
	int i, j;
	char *end;
	void *par = NULL;
	struct dn_id *buf, *base;
	struct dn_sch *sch = NULL;
	struct dn_link *p = NULL;
	struct dn_fs *fs = NULL;
	struct dn_profile *pf = NULL;
	struct ipfw_flow_id *mask = NULL;
	int lmax;
	uint32_t _foo = 0, *flags = &_foo , *buckets = &_foo;

	/*
	 * allocate space for 1 header,
	 * 1 scheduler, 1 link, 1 flowset, 1 profile
	 */
	lmax = sizeof(struct dn_id);	/* command header */
	lmax += sizeof(struct dn_sch) + sizeof(struct dn_link) +
		sizeof(struct dn_fs) + sizeof(struct dn_profile);

	av++; ac--;
	/* Pipe number */
	if (ac && isdigit(**av)) {
		i = atoi(*av); av++; ac--;
	} else
		i = -1;
	if (i <= 0)
		errx(EX_USAGE, "need a pipe/flowset/sched number");
	base = buf = safe_calloc(1, lmax);
	/* all commands start with a 'CONFIGURE' and a version */
	o_next(&buf, sizeof(struct dn_id), DN_CMD_CONFIG);
	base->id = DN_API_VERSION;

	switch (co.do_pipe) {
	case 1: /* "pipe N config ..." */
		/* Allocate space for the WF2Q+ scheduler, its link
		 * and the FIFO flowset. Set the number, but leave
		 * the scheduler subtype and other parameters to 0
		 * so the kernel will use appropriate defaults.
		 * XXX todo: add a flag to record if a parameter
		 * is actually configured.
		 * If we do a 'pipe config' mask -> sched_mask.
		 * The FIFO scheduler and link are derived from the
		 * WF2Q+ one in the kernel.
		 */
		sch = o_next(&buf, sizeof(*sch), DN_SCH);
		p = o_next(&buf, sizeof(*p), DN_LINK);
		fs = o_next(&buf, sizeof(*fs), DN_FS);

		sch->sched_nr = i;
		sch->oid.subtype = 0;	/* defaults to WF2Q+ */
		mask = &sch->sched_mask;
		flags = &sch->flags;
		buckets = &sch->buckets;
		*flags |= DN_PIPE_CMD;

		p->link_nr = i;

		/* This flowset is only for the FIFO scheduler */
		fs->fs_nr = i + 2*DN_MAX_ID;
		fs->sched_nr = i + DN_MAX_ID;
		break;

	case 2: /* "queue N config ... " */
		fs = o_next(&buf, sizeof(*fs), DN_FS);
		fs->fs_nr = i;
		mask = &fs->flow_mask;
		flags = &fs->flags;
		buckets = &fs->buckets;
		break;

	case 3: /* "sched N config ..." */
		sch = o_next(&buf, sizeof(*sch), DN_SCH);
		fs = o_next(&buf, sizeof(*fs), DN_FS);
		sch->sched_nr = i;
		mask = &sch->sched_mask;
		flags = &sch->flags;
		buckets = &sch->buckets;
		/* fs is used only with !MULTIQUEUE schedulers */
		fs->fs_nr = i + DN_MAX_ID;
		fs->sched_nr = i;
		break;
	}
	/* set to -1 those fields for which we want to reuse existing
	 * values from the kernel.
	 * Also, *_nr and subtype = 0 mean reuse the value from the kernel.
	 * XXX todo: support reuse of the mask.
	 */
	if (p)
		p->bandwidth = -1;
	for (j = 0; j < sizeof(fs->par)/sizeof(fs->par[0]); j++)
		fs->par[j] = -1;
	while (ac > 0) {
		double d;
		int tok = match_token(dummynet_params, *av);
		ac--; av++;

		switch(tok) {
		case TOK_NOERROR:
			NEED(fs, "noerror is only for pipes");
			fs->flags |= DN_NOERROR;
			break;

		case TOK_PLR:
			NEED(fs, "plr is only for pipes");
			NEED1("plr needs argument 0..1\n");
			d = strtod(av[0], NULL);
			if (d > 1)
				d = 1;
			else if (d < 0)
				d = 0;
			fs->plr = (int)(d*0x7fffffff);
			ac--; av++;
			break;

		case TOK_QUEUE:
			NEED(fs, "queue is only for pipes or flowsets");
			NEED1("queue needs queue size\n");
			end = NULL;
			fs->qsize = strtoul(av[0], &end, 0);
			if (*end == 'K' || *end == 'k') {
				fs->flags |= DN_QSIZE_BYTES;
				fs->qsize *= 1024;
			} else if (*end == 'B' ||
			    _substrcmp2(end, "by", "bytes") == 0) {
				fs->flags |= DN_QSIZE_BYTES;
			}
			ac--; av++;
			break;

		case TOK_BUCKETS:
			NEED(fs, "buckets is only for pipes or flowsets");
			NEED1("buckets needs argument\n");
			*buckets = strtoul(av[0], NULL, 0);
			ac--; av++;
			break;

		case TOK_FLOW_MASK:
		case TOK_SCHED_MASK:
		case TOK_MASK:
			NEED(mask, "tok_mask");
			NEED1("mask needs mask specifier\n");
			/*
			 * per-flow queue, mask is dst_ip, dst_port,
			 * src_ip, src_port, proto measured in bits
			 */
			par = NULL;

			bzero(mask, sizeof(*mask));
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
				     * except 'extra' (the queue number)
				     */
				    mask->dst_ip = ~0;
				    mask->src_ip = ~0;
				    mask->dst_port = ~0;
				    mask->src_port = ~0;
				    mask->proto = ~0;
				    n2mask(&mask->dst_ip6, 128);
				    n2mask(&mask->src_ip6, 128);
				    mask->flow_id6 = ~0;
				    *flags |= DN_HAVE_MASK;
				    goto end_mask;

			    case TOK_QUEUE:
				    mask->extra = ~0;
				    *flags |= DN_HAVE_MASK;
				    goto end_mask;

			    case TOK_DSTIP:
				    mask->addr_type = 4;
				    p32 = &mask->dst_ip;
				    break;

			    case TOK_SRCIP:
				    mask->addr_type = 4;
				    p32 = &mask->src_ip;
				    break;

			    case TOK_DSTIP6:
				    mask->addr_type = 6;
				    pa6 = &mask->dst_ip6;
				    break;
			    
			    case TOK_SRCIP6:
				    mask->addr_type = 6;
				    pa6 = &mask->src_ip6;
				    break;

			    case TOK_FLOWID:
				    mask->addr_type = 6;
				    p20 = &mask->flow_id6;
				    break;

			    case TOK_DSTPORT:
				    p16 = &mask->dst_port;
				    break;

			    case TOK_SRCPORT:
				    p16 = &mask->src_port;
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
				    mask->proto = (uint8_t)a;
			    }
			    if (a != 0)
				    *flags |= DN_HAVE_MASK;
			    ac--; av++;
			} /* end while, config masks */
end_mask:
			break;

		case TOK_RED:
		case TOK_GRED:
			NEED1("red/gred needs w_q/min_th/max_th/max_p\n");
			fs->flags |= DN_IS_RED;
			if (tok == TOK_GRED)
				fs->flags |= DN_IS_GENTLE_RED;
			/*
			 * the format for parameters is w_q/min_th/max_th/max_p
			 */
			if ((end = strsep(&av[0], "/"))) {
			    double w_q = strtod(end, NULL);
			    if (w_q > 1 || w_q <= 0)
				errx(EX_DATAERR, "0 < w_q <= 1");
			    fs->w_q = (int) (w_q * (1 << SCALE_RED));
			}
			if ((end = strsep(&av[0], "/"))) {
			    fs->min_th = strtoul(end, &end, 0);
			    if (*end == 'K' || *end == 'k')
				fs->min_th *= 1024;
			}
			if ((end = strsep(&av[0], "/"))) {
			    fs->max_th = strtoul(end, &end, 0);
			    if (*end == 'K' || *end == 'k')
				fs->max_th *= 1024;
			}
			if ((end = strsep(&av[0], "/"))) {
			    double max_p = strtod(end, NULL);
			    if (max_p > 1 || max_p <= 0)
				errx(EX_DATAERR, "0 < max_p <= 1");
			    fs->max_p = (int)(max_p * (1 << SCALE_RED));
			}
			ac--; av++;
			break;

		case TOK_DROPTAIL:
			NEED(fs, "droptail is only for flowsets");
			fs->flags &= ~(DN_IS_RED|DN_IS_GENTLE_RED);
			break;

		case TOK_BW:
			NEED(p, "bw is only for links");
			NEED1("bw needs bandwidth or interface\n");
			read_bandwidth(av[0], &p->bandwidth, NULL, 0);
			ac--; av++;
			break;

		case TOK_DELAY:
			NEED(p, "delay is only for links");
			NEED1("delay needs argument 0..10000ms\n");
			p->delay = strtoul(av[0], NULL, 0);
			ac--; av++;
			break;

		case TOK_TYPE: {
			int l;
			NEED(sch, "type is only for schedulers");
			NEED1("type needs a string");
			l = strlen(av[0]);
			if (l == 0 || l > 15)
				errx(1, "type %s too long\n", av[0]);
			strcpy(sch->name, av[0]);
			sch->oid.subtype = 0; /* use string */
			ac--; av++;
			break;
		    }

		case TOK_WEIGHT:
			NEED(fs, "weight is only for flowsets");
			NEED1("weight needs argument\n");
			fs->par[0] = strtol(av[0], &end, 0);
			ac--; av++;
			break;

		case TOK_LMAX:
			NEED(fs, "lmax is only for flowsets");
			NEED1("lmax needs argument\n");
			fs->par[1] = strtol(av[0], &end, 0);
			ac--; av++;
			break;

		case TOK_PRI:
			NEED(fs, "priority is only for flowsets");
			NEED1("priority needs argument\n");
			fs->par[2] = strtol(av[0], &end, 0);
			ac--; av++;
			break;

		case TOK_SCHED:
		case TOK_PIPE:
			NEED(fs, "pipe/sched");
			NEED1("pipe/link/sched needs number\n");
			fs->sched_nr = strtoul(av[0], &end, 0);
			ac--; av++;
			break;

		case TOK_PROFILE:
			NEED((!pf), "profile already set");
			NEED(p, "profile");
		    {
			NEED1("extra delay needs the file name\n");
			pf = o_next(&buf, sizeof(*pf), DN_PROFILE);
			load_extra_delays(av[0], pf, p); //XXX can't fail?
			--ac; ++av;
		    }
			break;

		case TOK_BURST:
			NEED(p, "burst");
			NEED1("burst needs argument\n");
			errno = 0;
			if (expand_number(av[0], &p->burst) < 0)
				if (errno != ERANGE)
					errx(EX_DATAERR,
					    "burst: invalid argument");
			if (errno || p->burst > (1ULL << 48) - 1)
				errx(EX_DATAERR,
				    "burst: out of range (0..2^48-1)");
			ac--; av++;
			break;

		default:
			errx(EX_DATAERR, "unrecognised option ``%s''", av[-1]);
		}
	}

	/* check validity of parameters */
	if (p) {
		if (p->delay > 10000)
			errx(EX_DATAERR, "delay must be < 10000");
		if (p->bandwidth == -1)
			p->bandwidth = 0;
	}
	if (fs) {
		/* XXX accept a 0 scheduler to keep the default */
	    if (fs->flags & DN_QSIZE_BYTES) {
		size_t len;
		long limit;

		len = sizeof(limit);
		if (sysctlbyname("net.inet.ip.dummynet.pipe_byte_limit",
			&limit, &len, NULL, 0) == -1)
			limit = 1024*1024;
		if (fs->qsize > limit)
			errx(EX_DATAERR, "queue size must be < %ldB", limit);
	    } else {
		size_t len;
		long limit;

		len = sizeof(limit);
		if (sysctlbyname("net.inet.ip.dummynet.pipe_slot_limit",
			&limit, &len, NULL, 0) == -1)
			limit = 100;
		if (fs->qsize > limit)
			errx(EX_DATAERR, "2 <= queue size <= %ld", limit);
	    }

	    if (fs->flags & DN_IS_RED) {
		size_t len;
		int lookup_depth, avg_pkt_size;
		double w_q;

		if (fs->min_th >= fs->max_th)
		    errx(EX_DATAERR, "min_th %d must be < than max_th %d",
			fs->min_th, fs->max_th);
		if (fs->max_th == 0)
		    errx(EX_DATAERR, "max_th must be > 0");

		len = sizeof(int);
		if (sysctlbyname("net.inet.ip.dummynet.red_lookup_depth",
			&lookup_depth, &len, NULL, 0) == -1)
			lookup_depth = 256;
		if (lookup_depth == 0)
		    errx(EX_DATAERR, "net.inet.ip.dummynet.red_lookup_depth"
			" must be greater than zero");

		len = sizeof(int);
		if (sysctlbyname("net.inet.ip.dummynet.red_avg_pkt_size",
			&avg_pkt_size, &len, NULL, 0) == -1)
			avg_pkt_size = 512;

		if (avg_pkt_size == 0)
			errx(EX_DATAERR,
			    "net.inet.ip.dummynet.red_avg_pkt_size must"
			    " be greater than zero");

		/*
		 * Ticks needed for sending a medium-sized packet.
		 * Unfortunately, when we are configuring a WF2Q+ queue, we
		 * do not have bandwidth information, because that is stored
		 * in the parent pipe, and also we have multiple queues
		 * competing for it. So we set s=0, which is not very
		 * correct. But on the other hand, why do we want RED with
		 * WF2Q+ ?
		 */
#if 0
		if (p.bandwidth==0) /* this is a WF2Q+ queue */
			s = 0;
		else
			s = (double)ck.hz * avg_pkt_size * 8 / p.bandwidth;
#endif
		/*
		 * max idle time (in ticks) before avg queue size becomes 0.
		 * NOTA:  (3/w_q) is approx the value x so that
		 * (1-w_q)^x < 10^-3.
		 */
		w_q = ((double)fs->w_q) / (1 << SCALE_RED);
#if 0 // go in kernel
		idle = s * 3. / w_q;
		fs->lookup_step = (int)idle / lookup_depth;
		if (!fs->lookup_step)
			fs->lookup_step = 1;
		weight = 1 - w_q;
		for (t = fs->lookup_step; t > 1; --t)
			weight *= 1 - w_q;
		fs->lookup_weight = (int)(weight * (1 << SCALE_RED));
#endif
	    }
	}

	i = do_cmd(IP_DUMMYNET3, base, (char *)buf - (char *)base);

	if (i)
		err(1, "setsockopt(%s)", "IP_DUMMYNET_CONFIGURE");
}

void
dummynet_flush(void)
{
	struct dn_id oid;
	oid_fill(&oid, sizeof(oid), DN_CMD_FLUSH, DN_API_VERSION);
	do_cmd(IP_DUMMYNET3, &oid, oid.len);
}

/* Parse input for 'ipfw [pipe|sched|queue] show [range list]'
 * Returns the number of ranges, and possibly stores them
 * in the array v of size len.
 */
static int
parse_range(int ac, char *av[], uint32_t *v, int len)
{
	int n = 0;
	char *endptr, *s;
	uint32_t base[2];

	if (v == NULL || len < 2) {
		v = base;
		len = 2;
	}

	for (s = *av; s != NULL; av++, ac--) {
		v[0] = strtoul(s, &endptr, 10);
		v[1] = (*endptr != '-') ? v[0] :
			 strtoul(endptr+1, &endptr, 10);
		if (*endptr == '\0') { /* prepare for next round */
			s = (ac > 0) ? *(av+1) : NULL;
		} else {
			if (*endptr != ',') {
				warn("invalid number: %s", s);
				s = ++endptr;
				continue;
			}
			/* continue processing from here */
			s = ++endptr;
			ac++;
			av--;
		}
		if (v[1] < v[0] ||
			v[1] < 0 || v[1] >= DN_MAX_ID-1 ||
			v[0] < 0 || v[1] >= DN_MAX_ID-1) {
			continue; /* invalid entry */
		}
		n++;
		/* translate if 'pipe list' */
		if (co.do_pipe == 1) {
			v[0] += DN_MAX_ID;
			v[1] += DN_MAX_ID;
		}
		v = (n*2 < len) ? v + 2 : base;
	}
	return n;
}

/* main entry point for dummynet list functions. co.do_pipe indicates
 * which function we want to support.
 * av may contain filtering arguments, either individual entries
 * or ranges, or lists (space or commas are valid separators).
 * Format for a range can be n1-n2 or n3 n4 n5 ...
 * In a range n1 must be <= n2, otherwise the range is ignored.
 * A number 'n4' is translate in a range 'n4-n4'
 * All number must be > 0 and < DN_MAX_ID-1
 */
void
dummynet_list(int ac, char *av[], int show_counters)
{
	struct dn_id *oid, *x = NULL;
	int ret, i, l;
	int n; 		/* # of ranges */
	int buflen;
	int max_size;	/* largest obj passed up */

	ac--;
	av++; 		/* skip 'list' | 'show' word */

	n = parse_range(ac, av, NULL, 0);	/* Count # of ranges. */

	/* Allocate space to store ranges */
	l = sizeof(*oid) + sizeof(uint32_t) * n * 2;
	oid = safe_calloc(1, l);
	oid_fill(oid, l, DN_CMD_GET, DN_API_VERSION);

	if (n > 0)	/* store ranges in idx */
		parse_range(ac, av, (uint32_t *)(oid + 1), n*2);
	/*
	 * Compute the size of the largest object returned. If the
	 * response leaves at least this much spare space in the
	 * buffer, then surely the response is complete; otherwise
	 * there might be a risk of truncation and we will need to
	 * retry with a larger buffer.
	 * XXX don't bother with smaller structs.
	 */
	max_size = sizeof(struct dn_fs);
	if (max_size < sizeof(struct dn_sch))
		max_size = sizeof(struct dn_sch);
	if (max_size < sizeof(struct dn_flow))
		max_size = sizeof(struct dn_flow);

	switch (co.do_pipe) {
	case 1:
		oid->subtype = DN_LINK;	/* list pipe */
		break;
	case 2:
		oid->subtype = DN_FS;	/* list queue */
		break;
	case 3:
		oid->subtype = DN_SCH;	/* list sched */
		break;
	}

	/*
	 * Ask the kernel an estimate of the required space (result
	 * in oid.id), unless we are requesting a subset of objects,
	 * in which case the kernel does not give an exact answer.
	 * In any case, space might grow in the meantime due to the
	 * creation of new queues, so we must be prepared to retry.
	 */
	if (n > 0) {
		buflen = 4*1024;
	} else {
		ret = do_cmd(-IP_DUMMYNET3, oid, (uintptr_t)&l);
		if (ret != 0 || oid->id <= sizeof(*oid))
			goto done;
		buflen = oid->id + max_size;
		oid->len = sizeof(*oid); /* restore */
	}
	/* Try a few times, until the buffer fits */
	for (i = 0; i < 20; i++) {
		l = buflen;
		x = safe_realloc(x, l);
		bcopy(oid, x, oid->len);
		ret = do_cmd(-IP_DUMMYNET3, x, (uintptr_t)&l);
		if (ret != 0 || x->id <= sizeof(*oid))
			goto done; /* no response */
		if (l + max_size <= buflen)
			break; /* ok */
		buflen *= 2;	 /* double for next attempt */
	}
	list_pipes(x, O_NEXT(x, l));
done:
	if (x)
		free(x);
	free(oid);
}
