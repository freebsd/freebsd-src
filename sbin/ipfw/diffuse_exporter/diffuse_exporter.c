/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by Lawrence Stewart under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Description:
 * Rule/flow exporter.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/tree.h>

#include <arpa/inet.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#define	WITH_DIP_INFO
#include <netinet/ip_diffuse_export.h>
#include <netinet/sctp.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "../diffuse_proto.h"
#include "../diffuse_ui.h"

#define	MAX_ERRORS_BEFORE_IGNORE	12
#define	DEFAULT_RULE_TIMEOUT_SECS	300
#define	DIP_REQSTATE_PKTSIZE		27

#define	STRLEN_LITERAL(s) (sizeof((s)) - 1)

/*
 * Length of a flowrule template set based on the default flowrule template
 * defined in <netinet/ip_diffuse_export.h>.
 * The set consists of the following parts (in order):
 * - struct dip_set_header
 * - struct dip_templ_header
 * - A uint16_t ID field for each information element (IE)
 * - A uint16_t length field for variable length IEs (there are currently 4)
 */
#define	DIP_DEFAULT_FLOWRULE_SETLEN (sizeof(struct dip_set_header) + \
    sizeof(struct dip_templ_header) + \
    (N_DEFAULT_FLOWRULE_TEMPLATE_ITEMS * sizeof(uint16_t)) + \
    4 * sizeof(uint16_t))

static const char *usage = "Usage: diffuse_exporter [-hv] [-c <host:port>] "
    "[-a <prot://host:port>,...]";

/*
 * Could remove templ_id since we also have template, but we save a bit of
 * memory since for each action node we only need a list of IDs.
 */
struct templ_id {
	uint16_t		id;
	RB_ENTRY(templ_id)	node;
};

static inline int
templ_id_compare(struct templ_id *a, struct templ_id *b)
{

	return ((a->id != b->id) ? (a->id < b->id ? -1 : 1) : 0);
}

RB_HEAD(templ_id_head, templ_id);
static RB_PROTOTYPE(templ_id_head, templ_id, node, templ_id_compare);
static RB_GENERATE(templ_id_head, templ_id, node, templ_id_compare);

struct rule {
	struct templ_id		*tplid;
	char			*flowrule_dataset;
	uint32_t		to;
	uint8_t			set_len;
	TAILQ_ENTRY(rule)	next;
};

TAILQ_HEAD(rule_head, rule);

/* Represents an action node we will be exporting rules to. */
struct action_node {
	struct sockaddr_storage		an_details;	/* v4 or v6 details. */
	int				proto;
	uint32_t			seq_no;
	int				to_sock;
	int				from_sock;
	int				closed;
	int				errors;
	struct templ_id_head		templ_ids;
	struct rule_head		rules;
	SLIST_ENTRY(action_node)	next;
};

/* List of action nodes. */
static SLIST_HEAD(action_node_head, action_node) anodes =
    SLIST_HEAD_INITIALIZER(action_node_head);

/* Template list, templates received from classifier. */
RB_GENERATE(di_template_head, di_template, node, template_compare);
static struct di_template_head templ_list;

/* Global exit flag. */
static int stop;

/* Sets the flag to terminate the main loop on receipt of a signal. */
static void
sigint_handler(int i)
{

	stop = 1;
}

/* Parse URL for a single action node. */
static void
parse_anode(char *s, struct action_node *an)
{
	struct addrinfo *ai, *curai;
	struct sockaddr_in *v4sockaddr;
	char *errptr, *ip, *p;
	int ret;
	uint16_t port;

	p = strstr(s, "://");
	if (p == NULL) {
		errx(EX_USAGE,
		    "target must be specified as <proto>://<host>:<port>");
	}

	/* Parse protocol. */
	if (strncmp(s, "udp", STRLEN_LITERAL("udp")) == 0)
		an->proto = IPPROTO_UDP;
	else if (strncmp(s, "tcp", STRLEN_LITERAL("tcp")) == 0)
		an->proto = IPPROTO_TCP;
	else if (strncmp(s, "sctp", STRLEN_LITERAL("sctp")) == 0)
		an->proto = IPPROTO_SCTP;
	else
		errx(EX_USAGE, "only udp/tcp/sctp are supported");

	ip = p + 3;
	p = strstr(ip, ":");
	if (p != NULL) {
		*p = '\0';
		p++;
		port = strtonum(p, 1, 65535, (const char **)&errptr);
		if (errptr != NULL)
			errx(EX_USAGE, "error target port '%s': %s", p, errptr);
	} else {
		port = DI_COLLECTOR_DEFAULT_LISTEN_PORT;
	}

	ret = getaddrinfo(ip, NULL, NULL, &ai);

	if (ret != 0)
		errx(EX_OSERR, "%s", gai_strerror(ret));

	curai = ai;
	while (curai != NULL && curai->ai_family != AF_INET)
		curai = curai->ai_next;

	if (curai != NULL) {
		v4sockaddr = (struct sockaddr_in *)&an->an_details;
		v4sockaddr->sin_family = curai->ai_family;
		v4sockaddr->sin_port = port;
		v4sockaddr->sin_addr.s_addr =
		    ((struct sockaddr_in *)curai->ai_addr)->sin_addr.s_addr;
	} else {
		errx(EX_USAGE, "getaddrinfo() returned non IPv4 details");
	}

	freeaddrinfo(ai);
	an->to_sock = -1;
	an->from_sock = -1;
	an->closed = 0;
	an->errors = 0;
	an->seq_no = 0;
	RB_INIT(&an->templ_ids);
}

static void
parse_anodes(char *optarg)
{
	struct action_node *tmp_anode;
	char *anode_str, *x;
	char *sep = ",";

	x = strdup(optarg);

	for (anode_str = strtok(x, sep); anode_str;
	    anode_str = strtok(NULL, sep)) {
		printf("%s\n", anode_str);
		tmp_anode = malloc(sizeof(struct action_node));
		if (tmp_anode == NULL)
			err(EX_OSERR, NULL);
		parse_anode(anode_str, tmp_anode);
		SLIST_INSERT_HEAD(&anodes, tmp_anode, next);
	}
	free(x);
}

static void
parse_class(char *optarg, uint32_t *class_ip, uint16_t *class_port)
{
	struct addrinfo *ai, *curai;
	char *errptr, *p;
	int ret;

	p = strstr(optarg, ":");

	if (p != NULL) {
		*p = '\0';
		p++;
		*class_port = strtonum(p, 1, 65535, (const char **)&errptr);
		if (errptr)
			errx(EX_USAGE, "parse error port '%s': %s", p, errptr);
	}

	ret = getaddrinfo(optarg, NULL, NULL, &ai);

	if (ret != 0)
		errx(EX_OSERR, "%s", gai_strerror(ret));

	curai = ai;
	while (curai != NULL && curai->ai_family != AF_INET)
		curai = curai->ai_next;

	if (curai != NULL) {
		*class_ip =
		    ((struct sockaddr_in *)curai->ai_addr)->sin_addr.s_addr;
	} else {
		errx(EX_USAGE, "getaddrinfo() returned non IPv4 details");
	}

	freeaddrinfo(ai);
}

/* Close socket for action node. */
static void
close_anode_socket(struct action_node *an)
{
	struct sctp_sndrcvinfo sinfo;

	if (an->proto == IPPROTO_SCTP) {
		sinfo.sinfo_flags = SCTP_EOF;
		sctp_send(an->to_sock, NULL, 0, &sinfo, 0);
	}

	close(an->to_sock);
}

/* Open socket for action node. */
static int
open_anode_socket(struct action_node *an)
{
	struct sctp_initmsg initmsg;
	struct sctp_status status;
	socklen_t len;
	int type;

	if (an->proto == IPPROTO_UDP)
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;

	if ((an->to_sock = socket(AF_INET, type, an->proto)) < 0) {
		errx(EX_OSERR, "create action node socket: %s",
		    strerror(errno));
	}

	if (an->proto == IPPROTO_SCTP) {
		/* Must have two streams. */
		memset(&initmsg, 0, sizeof(initmsg));
		initmsg.sinit_max_instreams = 2;
		initmsg.sinit_num_ostreams = 2;

		if (setsockopt(an->to_sock, IPPROTO_SCTP, SCTP_INITMSG, &initmsg,
		    sizeof(initmsg))) {
			errx(EX_OSERR, "set sock option initmsg");
		}
	}

	if (connect(an->to_sock, (struct sockaddr *)&an->an_details,
	    sizeof(struct sockaddr_in)) < 0) {
		errx(EX_OSERR, "connect action node socket: %s",
		    strerror(errno));
	}

	if (an->proto == IPPROTO_SCTP) {
		memset(&status, 0, sizeof(status));
		len = sizeof(status);

		if (getsockopt(an->to_sock, IPPROTO_SCTP, SCTP_STATUS, &status,
		    &len) == -1) {
			errx(EX_OSERR,"get sock option status: %s",
			    strerror(errno));
		}
		if (status.sstat_instrms < 2 || status.sstat_outstrms < 2)
			errx(EX_OSERR,"can't get two streams");
	}

	/* XXX: Use one-to-many association for SCTP. */

	return (an->to_sock);
}

static int
send_anode_pkt(struct action_node *anode, char *colpkt, int tplindex,
    int tpllen, int dataindex, int datalen)
{
	struct dip_header *hdr;
	struct sctp_sndrcvinfo sinfo;
	int ret;

	ret = 0;

	if (anode->proto == IPPROTO_SCTP) {
		/* Send templates and data over different streams. */
		if (tpllen > sizeof(struct dip_header)) {
			hdr = (struct dip_header *)colpkt + tplindex;
			hdr->msg_len = htons(tpllen);
			hdr->seq_no = htonl(anode->seq_no++);
			bzero(&sinfo, sizeof(sinfo));
			sinfo.sinfo_stream = 0;
			/* XXX: Handle partial sends. */
			ret = sctp_send(anode->to_sock, colpkt + tplindex,
			    tpllen, &sinfo, 0);
		}

		if (!ret && datalen > sizeof(struct dip_header)) {
			hdr = (struct dip_header *)colpkt + dataindex;
			hdr->msg_len = htons(datalen);
			hdr->seq_no = htonl(anode->seq_no++);
			bzero(&sinfo, sizeof(sinfo));
			sinfo.sinfo_stream = 1;
#ifdef __FREEBSD__
			/* Use SCTP PR if possible (man sctp_send). */
			sinfo.sinfo_flags |= SCTP_PR_SCTP_TTL;
			/* Drop if can't send for this many ms. */
			sinfo.sinfo_timetolive = 200;
#endif
			/* XXX: Handle partial sends. */
			ret = sctp_send(anode->to_sock, colpkt + dataindex,
			    datalen, &sinfo, 0);
		}
	} else {
		/* TCP or UDP. */
		hdr = (struct dip_header *)colpkt;
		hdr->msg_len = htons(tpllen + datalen);
		hdr->seq_no = htonl(anode->seq_no++);

		/*
		 * XXX: We don't do PMTUD yet so if the length of what we're
		 * sending is > MTU along the path to the action node (likely in
		 * the common case where diffuse_exporter runs on the classifier
		 * node and kernel sends packets via loopback which has a
		 * default MTU of 16384) and we're using UDP for transport, we
		 * will cause datagrams to be fragmented.
		 */
		/* XXX: Handle partial sends. */
		ret = send(anode->to_sock, colpkt, tpllen + datalen, 0);
	}

	return (ret);
}

/* Forward message to action node. */
static int
fwd_anode(struct action_node *an, char *dikrnlpkt, int dikrnlpktlen)
{
	struct dip_header *hdr;
	struct dip_set_header *shdr;
	struct dip_templ_header *thdr;
	struct rule *anrule;
	struct templ_id *r, s;
	struct timeval curtime;
	char *databuf, *dstbuf, *templbuf;
	char colpkt[dikrnlpktlen * 2];
	int databuf_index, newtpl, offs, ret, templbuf_index;

	if (an->closed || an->errors > MAX_ERRORS_BEFORE_IGNORE)
		return (0);

	hdr = (struct dip_header *)dikrnlpkt;
	templbuf = colpkt;
	databuf = colpkt + (sizeof(colpkt) / 2);
	gettimeofday(&curtime, NULL);

	/*
	 * For TCP and UDP, we interleave the templates and data all into a
	 * single buffer (colpkt). For SCTP, we split the templates and data so
	 * that they can be sent over separate SCTP streams.
	 */
	if (an->proto == IPPROTO_SCTP) {
		memcpy(templbuf, dikrnlpkt, sizeof(struct dip_header));
		memcpy(databuf, dikrnlpkt, sizeof(struct dip_header));
		templbuf_index = databuf_index = sizeof(struct dip_header);
	} else {
		memcpy(colpkt, dikrnlpkt, sizeof(struct dip_header));
		databuf_index = sizeof(struct dip_header);
		templbuf_index = 0;
	}

	offs = sizeof(struct dip_header);

	while (offs < ntohs(hdr->msg_len)) {
		/*
		 * Templates are always paried with a proceeding data set, so
		 * parse both as a pair, storing copies of the template and data
		 * set in the action node to allow state requests from action
		 * nodes to be fullfilled. For classifiers we send to via SCTP
		 * or TCP, we only send templates once, whereas for UDP we
		 * always send them.
		 */
		shdr = (struct dip_set_header *)(dikrnlpkt + offs);
		assert(ntohs(shdr->set_id) <= DIP_SET_ID_FLOWRULE_TPL);

		/* Template first. */
		thdr = (struct dip_templ_header *)(dikrnlpkt + offs +
		    sizeof(struct dip_set_header));
		s.id = ntohs(thdr->templ_id);
		r = RB_FIND(templ_id_head, &an->templ_ids, &s);
		if (r == NULL) {
			r = malloc(sizeof(struct templ_id));
			if (r == NULL)
				continue; /* XXX: Or break? */
			r->id = s.id;
			RB_INSERT(templ_id_head, &an->templ_ids, r);
			newtpl = 1;
		} else {
			newtpl = 0;
		}

		if (newtpl || an->proto == IPPROTO_UDP) {
			if (an->proto == IPPROTO_SCTP)
				dstbuf = &templbuf[templbuf_index];
			else
				dstbuf = &colpkt[templbuf_index + databuf_index];

			memcpy(dstbuf, &dikrnlpkt[offs], ntohs(shdr->set_len));
			templbuf_index += ntohs(shdr->set_len);
		}

		/* Move to the data set header. */
		offs += ntohs(shdr->set_len);
		shdr = (struct dip_set_header *)(dikrnlpkt + offs);
		assert(ntohs(shdr->set_id) <= DIP_SET_ID_FLOWRULE_TPL);

		/* Data set second. */
		if (an->proto == IPPROTO_SCTP)
			dstbuf = &databuf[databuf_index];
		else
			dstbuf = &colpkt[templbuf_index + databuf_index];

		memcpy(dstbuf, &dikrnlpkt[offs], ntohs(shdr->set_len));

		anrule = malloc(sizeof(struct rule) + ntohs(shdr->set_len));
		if (anrule == NULL)
			continue; /* XXX: Or something else? */

		anrule->flowrule_dataset = ((char *)anrule) +
		    sizeof(struct rule);
		anrule->set_len = ntohs(shdr->set_len);
		anrule->tplid = r;
		anrule->to = curtime.tv_sec + DEFAULT_RULE_TIMEOUT_SECS;

		/* Cache the flow rule data set. */
		memcpy(anrule->flowrule_dataset, &dikrnlpkt[offs],
		    ntohs(shdr->set_len));

		TAILQ_INSERT_TAIL(&an->rules, anrule, next);
		databuf_index += ntohs(shdr->set_len);
		offs += ntohs(shdr->set_len);
	}

	ret = send_anode_pkt(an, colpkt, templbuf - colpkt, templbuf_index,
	    databuf - colpkt, databuf_index);

	if (ret < 0)
		an->errors++;
	else
		an->errors = 0;

	return (ret);
}

/* Destroy all action nodes. */
static void
free_anodes()
{
	struct action_node *tmp_anode;
	struct rule *tmpruledel, *tmprulenext;
	struct templ_id *n, *r;

	while (!SLIST_EMPTY(&anodes)) {
		tmp_anode = SLIST_FIRST(&anodes);
		SLIST_REMOVE_HEAD(&anodes, next);
		close_anode_socket(tmp_anode);
		for (r = RB_MIN(templ_id_head, &tmp_anode->templ_ids); r != NULL;
		    r = n) {
			n = RB_NEXT(templ_id_head, &tmp_anode->templ_ids, r);
			RB_REMOVE(templ_id_head, &tmp_anode->templ_ids, r);
			free(r);
		}

		tmpruledel = TAILQ_FIRST(&tmp_anode->rules);
		while (tmpruledel != NULL) {
			tmprulenext = TAILQ_NEXT(tmpruledel, next);
			/*
			 * Freeing the rule also frees the memory used by the
			 * flowrule_dataset member.
			 */
			free(tmpruledel);
			tmpruledel = tmprulenext;
		}

		free(tmp_anode);
	}
}

/* Free templates received from classifier. */
static void
free_templates()
{
	struct di_template *n, *r;

	for (r = RB_MIN(di_template_head, &templ_list); r != NULL; r = n) {
		n = RB_NEXT(di_template_head, &templ_list, r);
		RB_REMOVE(di_template_head, &templ_list, r);
		free(r);
	}
}

static void
handle_anode_state_request(struct action_node *anode, char *buf, int buflen)
{
	struct dip_set_header *shdr;
	struct dip_templ_header *thdr;
	struct rule *tmprule;
	char *databuf, *dstbuf, *templbuf;
	int databuf_index, i, offs, ret, templbuf_index;

	assert(buflen >= 3000);
	templbuf = buf;
	databuf = buf + (buflen / 2);

	/*
	 * For TCP and UDP, we interleave the templates and data all into a
	 * single buffer (buf). For SCTP, we split the templates and data so
	 * that they can be sent over separate SCTP streams.
	 */
	if (anode->proto == IPPROTO_SCTP) {
		templbuf_index = databuf_index = sizeof(struct dip_header);
	} else {
		templbuf_index = sizeof(struct dip_header);
		databuf_index = 0;
	}

	TAILQ_FOREACH(tmprule, &anode->rules, next) {
		if (anode->proto == IPPROTO_SCTP)
			dstbuf = templbuf + templbuf_index;
		else
			dstbuf = buf + templbuf_index + databuf_index;

		/*
		 * XXX: For SCTP or TCP action nodes, we don't need to send
		 * templates more than once.
		 */
		offs = 0;
		shdr = (struct dip_set_header *)dstbuf;
		shdr->set_id = DIP_SET_ID_FLOWRULE_TPL;
		shdr->set_len = DIP_DEFAULT_FLOWRULE_SETLEN;
		offs += sizeof(struct dip_set_header);

		thdr = (struct dip_templ_header *)(dstbuf + offs);
		thdr->templ_id = tmprule->tplid->id;
		thdr->flags = 0;
		offs += sizeof(struct dip_templ_header);

		/* Add the IE data for the default flowrule template. */
		for (i = 0; i < N_DEFAULT_FLOWRULE_TEMPLATE_ITEMS; i++) {
			*((uint16_t *)(dstbuf + offs)) =
			    htons(dip_info[def_flowrule_template[i]].id);
			offs += sizeof(uint16_t);
			if (def_flowrule_template[i] == DIP_IE_ACTION ||
			    def_flowrule_template[i] == DIP_IE_EXPORT_NAME ||
			    def_flowrule_template[i] == DIP_IE_CLASSIFIER_NAME) {
				*((uint16_t *)(dstbuf + offs)) =
				    htons((uint16_t)DI_MAX_NAME_STR_LEN);
				offs += sizeof(uint16_t);
			} else if (def_flowrule_template[i] ==
			    DIP_IE_ACTION_PARAMS) {
				*((uint16_t *)(dstbuf + offs)) =
				    htons((uint16_t)DI_MAX_PARAM_STR_LEN);
				offs += sizeof(uint16_t);
			}
		}
		templbuf_index += offs;

		if (anode->proto == IPPROTO_SCTP)
			dstbuf = databuf + databuf_index;
		else
			dstbuf = buf + templbuf_index + databuf_index;

		memcpy(dstbuf, tmprule->flowrule_dataset, tmprule->set_len);
		databuf_index += tmprule->set_len;

		/* XXX: Switch to a dynamically determined max packet len. */
		if (templbuf_index + databuf_index > 1400 ||
		    TAILQ_NEXT(tmprule, next) == NULL ||
		    (anode->proto == IPPROTO_SCTP && (templbuf_index > 1400 ||
		    databuf_index > 1400))) {
			ret = send_anode_pkt(anode, buf, templbuf - buf,
			    templbuf_index, databuf - buf, databuf_index);

			if (anode->proto == IPPROTO_SCTP) {
				templbuf_index = databuf_index =
				    sizeof(struct dip_header);
			} else {
				templbuf_index = sizeof(struct dip_header);
				databuf_index = 0;
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	fd_set rset, wset, _rset, _wset;
	socklen_t len;
	struct action_node *tmp_anode;
	struct rule *tmpruledel, *tmprulenext;
	struct sockaddr_in fromanode_addr, sin;
	struct timeval curtime, tv;
	char buf[IP_MAXPACKET];
	int ch, clsock, cnt, fromanode_sock, fromanodes, max_fd, nbytes;
	int verbose;
	uint32_t class_ip;
	uint16_t class_port;

	class_ip = INADDR_ANY;
	class_port = DI_COLLECTOR_DEFAULT_LISTEN_PORT;
	max_fd = verbose = 0;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	RB_INIT(&templ_list);

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	if (argc < 1) {
		printf("%s\n", usage);
		exit(-1);
	}

	while ((ch = getopt(argc, argv, "a:c:hv")) != EOF) {
		switch (ch) {
		case 'a':
			parse_anodes(optarg);
			break;

		case 'c':
			parse_class(optarg, &class_ip, &class_port);
			break;

		case 'h':
			printf("%s\n", usage);
			exit(0);

		case 'v':
			verbose++;
			break;

		default:
			printf("%s\n", usage);
			exit(-1);
		}
	}

	/* TCP listen socket for receiving action node messages. */
	if ((fromanodes = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		errx(EX_OSERR, "create fromanodes socket: %s", strerror(errno));

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port  = htons(DI_EXPORTER_DEFAULT_LISTEN_PORT);
	sin.sin_addr.s_addr = INADDR_ANY;

	if (bind(fromanodes, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		errx(EX_OSERR, "bind fromanodes socket: %s", strerror(errno));

	if (listen(fromanodes, 10) < 0)
		errx(EX_OSERR, "listen fromanodes socket: %s", strerror(errno));

	/* UDP socket to receive messages from the kernel exporter. */
	if ((clsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		errx(EX_OSERR, "create class socket: %s", strerror(errno));

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port  = htons(class_port);
	sin.sin_addr.s_addr = class_ip;

	if (bind(clsock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		errx(EX_OSERR, "bind class socket: %s", strerror(errno));

	printf("listening %s:%d\n", inet_ntoa(*((struct in_addr *)&class_ip)),
	    class_port);

	/* Open sockets to action nodes. */
	SLIST_FOREACH(tmp_anode, &anodes, next) {
		open_anode_socket(tmp_anode);
	}

	max_fd = clsock;
	SLIST_FOREACH(tmp_anode, &anodes, next) {
		if (tmp_anode->to_sock > max_fd)
			max_fd = tmp_anode->to_sock;
	}

	FD_ZERO(&rset);
	FD_ZERO(&wset);
	FD_SET(clsock, &rset);
	FD_SET(fromanodes, &rset);

	SLIST_FOREACH(tmp_anode, &anodes, next) {
		FD_SET(tmp_anode->to_sock, &rset);
	}

	/* Packet processing. */
	do {
		_rset = rset;
		_wset = wset;

		if ((cnt = select(max_fd + 1, &_rset, &_wset, NULL, &tv)) < 0) {
			if (errno != EINTR)
				errx(EX_OSERR, "select error");
		}

		if (cnt < 1)
			continue;

		if (FD_ISSET(clsock, &_rset)) {
			nbytes = recv(clsock, &buf, sizeof(buf), 0);
			if (nbytes < 0)
				errx(EX_OSERR, "class sock read error");

#ifdef DIFFUSE_DEBUG2
			printf("message %u\n", n);
			for (int j = 0; j < n; j++)
				printf("%u ", (uint8_t)buf[j]);
			printf("\n");
#endif

			if (verbose)
				diffuse_proto_print_msg(buf, &templ_list);

			SLIST_FOREACH(tmp_anode, &anodes, next) {
				fwd_anode(tmp_anode, buf, nbytes);
			}
		}

		if (FD_ISSET(fromanodes, &_rset)) {
			len = sizeof(fromanode_addr);
			fromanode_sock = accept(fromanodes,
			    (struct sockaddr *)&fromanode_addr, &len);
			/* XXX: Not v6 friendly. */
			SLIST_FOREACH(tmp_anode, &anodes, next) {
				if (bcmp(&fromanode_addr.sin_addr.s_addr,
				    &(((struct sockaddr_in *)&tmp_anode->
				    an_details)->sin_addr.s_addr),
				    sizeof(struct in_addr)) == 0 &&
				    tmp_anode->from_sock == -1) {
					tmp_anode->from_sock = fromanode_sock;
					FD_SET(fromanode_sock, &rset);
					if (fromanode_sock > max_fd)
						max_fd = fromanode_sock;
					break;
				}
			}
			/*
			 * If the incoming connection is not from a configured
			 * action node or we're already processing a connection
			 * from the action node, drop the new connection.
			 */
			if (tmp_anode == NULL)
				close(fromanode_sock);
		}

		SLIST_FOREACH(tmp_anode, &anodes, next) {
			if (FD_ISSET(tmp_anode->to_sock, &_rset)) {
				if (read(tmp_anode->to_sock, buf,
				    sizeof(buf)) == 0) {
					close(tmp_anode->to_sock);
					tmp_anode->closed = 1;
				}
				/* XXX: handle SCTP events? */
			} else if (FD_ISSET(tmp_anode->from_sock, &_rset)) {
				nbytes = read(tmp_anode->to_sock, buf,
				    sizeof(buf));
				/*
				 * XXX: This is a hack, but currently the only
				 * packet a collector will send to the exporter
				 * is a "request state" packet. If the size
				 * matches, don't bother parsing the packet and
				 * simply handle the request.
				 */
				if (nbytes == DIP_REQSTATE_PKTSIZE) {
					handle_anode_state_request(tmp_anode,
					    buf, sizeof(buf));
				}

				FD_CLR(tmp_anode->from_sock, &_rset);
				close(tmp_anode->from_sock);
				tmp_anode->from_sock = -1;
			}

			/* Flush old rules from the anode's rule cache. */
			gettimeofday(&curtime, NULL);
			tmpruledel = TAILQ_FIRST(&tmp_anode->rules);
			while (tmpruledel != NULL &&
			    curtime.tv_sec > tmpruledel->to) {
				tmprulenext = TAILQ_NEXT(tmpruledel, next);
				TAILQ_REMOVE(&tmp_anode->rules, tmpruledel,
				    next);
				/*
				 * Freeing the rule also frees the memory used
				 * by the flowrule_dataset member.
				 */
				free(tmpruledel);
				tmpruledel = tmprulenext;
			}
		}
	} while (!stop);

	close(clsock);
	free_anodes();
	free_templates();

	return (0);
}
