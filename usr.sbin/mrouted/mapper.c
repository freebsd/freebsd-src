/* Mapper for connections between MRouteD multicast routers.
 * Written by Pavel Curtis <Pavel@PARC.Xerox.Com>
 *
 * $FreeBSD$
 */

/*
 * Copyright (c) Xerox Corporation 1992. All rights reserved.
 *  
 * License is granted to copy, to use, and to make and to use derivative
 * works for research and evaluation purposes, provided that Xerox is
 * acknowledged in all documentation pertaining to any such copy or derivative
 * work. Xerox grants no other licenses expressed or implied. The Xerox trade
 * name should not be used in any advertising without its written permission.
 *  
 * XEROX CORPORATION MAKES NO REPRESENTATIONS CONCERNING EITHER THE
 * MERCHANTABILITY OF THIS SOFTWARE OR THE SUITABILITY OF THIS SOFTWARE
 * FOR ANY PARTICULAR PURPOSE.  The software is provided "as is" without
 * express or implied warranty of any kind.
 *  
 * These notices must be retained in any copies of any part of this software.
 */

#include <string.h>
#include <netdb.h>
#include <sys/time.h>
#include "defs.h"
#include <arpa/inet.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#define DEFAULT_TIMEOUT	2	/* How long to wait before retrying requests */
#define DEFAULT_RETRIES 1	/* How many times to ask each router */


/* All IP addresses are stored in the data structure in NET order. */

typedef struct neighbor {
    struct neighbor    *next;
    u_int32		addr;		/* IP address in NET order */
    u_char		metric;		/* TTL cost of forwarding */
    u_char		threshold;	/* TTL threshold to forward */
    u_short		flags;		/* flags on connection */
#define NF_PRESENT 0x8000	/* True if flags are meaningful */
} Neighbor;

typedef struct interface {
    struct interface *next;
    u_int32	addr;		/* IP address of the interface in NET order */
    Neighbor   *neighbors;	/* List of neighbors' IP addresses */
} Interface;

typedef struct node {
    u_int32	addr;		/* IP address of this entry in NET order */
    u_int32	version;	/* which mrouted version is running */
    int		tries;		/* How many requests sent?  -1 for aliases */
    union {
	struct node *alias;		/* If alias, to what? */
	struct interface *interfaces;	/* Else, neighbor data */
    } u;
    struct node *left, *right;
} Node;


Node   *routers = 0;
u_int32	our_addr, target_addr = 0;		/* in NET order */
int	debug = 0;
int	retries = DEFAULT_RETRIES;
int	timeout = DEFAULT_TIMEOUT;
int	show_names = TRUE;
vifi_t  numvifs;		/* to keep loader happy */
				/* (see COPY_TABLES macro called in kern.c) */

Node *			find_node __P((u_int32 addr, Node **ptr));
Interface *		find_interface __P((u_int32 addr, Node *node));
Neighbor *		find_neighbor __P((u_int32 addr, Node *node));
int			main __P((int argc, char *argv[]));
void			ask __P((u_int32 dst));
void			ask2 __P((u_int32 dst));
int			retry_requests __P((Node *node));
char *			inet_name __P((u_int32 addr));
void			print_map __P((Node *node));
char *			graph_name __P((u_int32 addr, char *buf));
void			graph_edges __P((Node *node));
void			elide_aliases __P((Node *node));
void			graph_map __P((void));
int			get_number __P((int *var, int deflt, char ***pargv,
						int *pargc));
u_int32			host_addr __P((char *name));


Node *find_node(addr, ptr)
    u_int32 addr;
    Node **ptr;
{
    Node *n = *ptr;

    if (!n) {
	*ptr = n = (Node *) malloc(sizeof(Node));
	n->addr = addr;
	n->version = 0;
	n->tries = 0;
	n->u.interfaces = 0;
	n->left = n->right = 0;
	return n;
    } else if (addr == n->addr)
	return n;
    else if (addr < n->addr)
	return find_node(addr, &(n->left));
    else
	return find_node(addr, &(n->right));
}


Interface *find_interface(addr, node)
    u_int32 addr;
    Node *node;
{
    Interface *ifc;

    for (ifc = node->u.interfaces; ifc; ifc = ifc->next)
	if (ifc->addr == addr)
	    return ifc;

    ifc = (Interface *) malloc(sizeof(Interface));
    ifc->addr = addr;
    ifc->next = node->u.interfaces;
    node->u.interfaces = ifc;
    ifc->neighbors = 0;

    return ifc;
}


Neighbor *find_neighbor(addr, node)
    u_int32 addr;
    Node *node;
{
    Interface *ifc;

    for (ifc = node->u.interfaces; ifc; ifc = ifc->next) {
	Neighbor *nb;

	for (nb = ifc->neighbors; nb; nb = nb->next)
	    if (nb->addr == addr)
		return nb;
    }

    return 0;
}


/*
 * Log errors and other messages to stderr, according to the severity of the
 * message and the current debug level.  For errors of severity LOG_ERR or
 * worse, terminate the program.
 */
#ifdef __STDC__
void
log(int severity, int syserr, char *format, ...)
{
	va_list ap;
	char    fmt[100];

	va_start(ap, format);
#else
/*VARARGS3*/
void 
log(severity, syserr, format, va_alist)
	int     severity, syserr;
	char   *format;
	va_dcl
{
	va_list ap;
	char    fmt[100];

	va_start(ap);
#endif

    switch (debug) {
	case 0: if (severity > LOG_WARNING) return;
	case 1: if (severity > LOG_NOTICE ) return;
	case 2: if (severity > LOG_INFO   ) return;
	default:
	    fmt[0] = '\0';
	    if (severity == LOG_WARNING)
		strcat(fmt, "warning - ");
	    strncat(fmt, format, 80);
	    vfprintf(stderr, fmt, ap);
	    if (syserr == 0)
		fprintf(stderr, "\n");
	    else if (syserr < sys_nerr)
		fprintf(stderr, ": %s\n", sys_errlist[syserr]);
	    else
		fprintf(stderr, ": errno %d\n", syserr);
    }

    if (severity <= LOG_ERR)
	exit(-1);
}


/*
 * Send a neighbors-list request.
 */
void ask(dst)
    u_int32 dst;
{
    send_igmp(our_addr, dst, IGMP_DVMRP, DVMRP_ASK_NEIGHBORS,
		htonl(MROUTED_LEVEL), 0);
}

void ask2(dst)
    u_int32 dst;
{
    send_igmp(our_addr, dst, IGMP_DVMRP, DVMRP_ASK_NEIGHBORS2,
		htonl(MROUTED_LEVEL), 0);
}


/*
 * Process an incoming group membership report.
 */
void accept_group_report(src, dst, group, r_type)
    u_int32 src, dst, group;
    int r_type;
{
    log(LOG_INFO, 0, "ignoring IGMP group membership report from %s to %s",
	inet_fmt(src, s1), inet_fmt(dst, s2));
}


/*
 * Process an incoming neighbor probe message.
 */
void accept_probe(src, dst, p, datalen, level)
    u_int32 src, dst, level;
    char *p;
    int datalen;
{
    log(LOG_INFO, 0, "ignoring DVMRP probe from %s to %s",
	inet_fmt(src, s1), inet_fmt(dst, s2));
}


/*
 * Process an incoming route report message.
 */
void accept_report(src, dst, p, datalen, level)
    u_int32 src, dst, level;
    char *p;
    int datalen;
{
    log(LOG_INFO, 0, "ignoring DVMRP routing report from %s to %s",
	inet_fmt(src, s1), inet_fmt(dst, s2));
}


/*
 * Process an incoming neighbor-list request message.
 */
void accept_neighbor_request(src, dst)
    u_int32 src, dst;
{
    if (src != our_addr)
	log(LOG_INFO, 0,
	    "ignoring spurious DVMRP neighbor request from %s to %s",
	    inet_fmt(src, s1), inet_fmt(dst, s2));
}

void accept_neighbor_request2(src, dst)
    u_int32 src, dst;
{
    if (src != our_addr)
	log(LOG_INFO, 0,
	    "ignoring spurious DVMRP neighbor request2 from %s to %s",
	    inet_fmt(src, s1), inet_fmt(dst, s2));
}


/*
 * Process an incoming neighbor-list message.
 */
void accept_neighbors(src, dst, p, datalen, level)
    u_int32 src, dst, level;
    u_char *p;
    int datalen;
{
    Node       *node = find_node(src, &routers);

    if (node->tries == 0)	/* Never heard of 'em; must have hit them at */
	node->tries = 1;	/* least once, though...*/
    else if (node->tries == -1)	/* follow alias link */
	node = node->u.alias;

#define GET_ADDR(a) (a = ((u_int32)*p++ << 24), a += ((u_int32)*p++ << 16),\
		     a += ((u_int32)*p++ << 8), a += *p++)

    /* if node is running a recent mrouted, ask for additional info */
    if (level != 0) {
	node->version = level;
	node->tries = 1;
	ask2(src);
	return;
    }

    if (debug > 3) {
	int i;

	fprintf(stderr, "    datalen = %d\n", datalen);
	for (i = 0; i < datalen; i++) {
	    if ((i & 0xF) == 0)
		fprintf(stderr, "   ");
	    fprintf(stderr, " %02x", p[i]);
	    if ((i & 0xF) == 0xF)
		fprintf(stderr, "\n");
	}
	if ((datalen & 0xF) != 0xF)
	    fprintf(stderr, "\n");
    }

    while (datalen > 0) {	/* loop through interfaces */
	u_int32		ifc_addr;
	u_char		metric, threshold, ncount;
	Node   	       *ifc_node;
	Interface      *ifc;
	Neighbor       *old_neighbors;

	if (datalen < 4 + 3) {
	    log(LOG_WARNING, 0, "received truncated interface record from %s",
		inet_fmt(src, s1));
	    return;
	}

	GET_ADDR(ifc_addr);
	ifc_addr = htonl(ifc_addr);
	metric = *p++;
	threshold = *p++;
	ncount = *p++;
	datalen -= 4 + 3;

	/* Fix up any alias information */
	ifc_node = find_node(ifc_addr, &routers);
	if (ifc_node->tries == 0) { /* new node */
	    ifc_node->tries = -1;
	    ifc_node->u.alias = node;
	} else if (ifc_node != node
		   && (ifc_node->tries > 0  ||  ifc_node->u.alias != node)) {
	    /* must merge two hosts' nodes */
	    Interface  *ifc_i, *next_ifc_i;

	    if (ifc_node->tries == -1) {
		Node *tmp = ifc_node->u.alias;

		ifc_node->u.alias = node;
		ifc_node = tmp;
	    }

	    /* Merge ifc_node (foo_i) into node (foo_n) */

	    if (ifc_node->tries > node->tries)
		node->tries = ifc_node->tries;

	    for (ifc_i = ifc_node->u.interfaces; ifc_i; ifc_i = next_ifc_i) {
		Neighbor *nb_i, *next_nb_i, *nb_n;
		Interface *ifc_n = find_interface(ifc_i->addr, node);

		old_neighbors = ifc_n->neighbors;
		for (nb_i = ifc_i->neighbors; nb_i; nb_i = next_nb_i) {
		    next_nb_i = nb_i->next;
		    for (nb_n = old_neighbors; nb_n; nb_n = nb_n->next)
			if (nb_i->addr == nb_n->addr) {
			    if (nb_i->metric != nb_n->metric
				|| nb_i->threshold != nb_n->threshold)
				log(LOG_WARNING, 0,
				    "inconsistent %s for neighbor %s of %s",
				    "metric/threshold",
				    inet_fmt(nb_i->addr, s1),
				    inet_fmt(node->addr, s2));
			    free(nb_i);
			    break;
			}
		    if (!nb_n) { /* no match for this neighbor yet */
			nb_i->next = ifc_n->neighbors;
			ifc_n->neighbors = nb_i;
		    }
		}

		next_ifc_i = ifc_i->next;
		free(ifc_i);
	    }

	    ifc_node->tries = -1;
	    ifc_node->u.alias = node;
	}
	
	ifc = find_interface(ifc_addr, node);
	old_neighbors = ifc->neighbors;
	
	/* Add the neighbors for this interface */
	while (ncount--) {
	    u_int32 	neighbor;
	    Neighbor   *nb;
	    Node       *n_node;

	    if (datalen < 4) {
		log(LOG_WARNING, 0, "received truncated neighbor list from %s",
		    inet_fmt(src, s1));
		return;
	    }

	    GET_ADDR(neighbor);
	    neighbor = htonl(neighbor);
	    datalen -= 4;

	    for (nb = old_neighbors; nb; nb = nb->next)
		if (nb->addr == neighbor) {
		    if (metric != nb->metric || threshold != nb->threshold)
			log(LOG_WARNING, 0,
			    "inconsistent %s for neighbor %s of %s",
			    "metric/threshold",
			    inet_fmt(nb->addr, s1), inet_fmt(node->addr, s2));
		    goto next_neighbor;
		}

	    nb = (Neighbor *) malloc(sizeof(Neighbor));
	    nb->next = ifc->neighbors;
	    ifc->neighbors = nb;
	    nb->addr = neighbor;
	    nb->metric = metric;
	    nb->threshold = threshold;
	    nb->flags = 0;

	    n_node = find_node(neighbor, &routers);
	    if (n_node->tries == 0  &&  !target_addr) { /* it's a new router */
		ask(neighbor);
		n_node->tries = 1;
	    }

	  next_neighbor: ;
	}
    }
}

void accept_neighbors2(src, dst, p, datalen, level)
    u_int32 src, dst, level;
    u_char *p;
    int datalen;
{
    Node       *node = find_node(src, &routers);
    u_int broken_cisco = ((level & 0xffff) == 0x020a); /* 10.2 */
    /* well, only possibly_broken_cisco, but that's too long to type. */

    if (node->tries == 0)	/* Never heard of 'em; must have hit them at */
	node->tries = 1;	/* least once, though...*/
    else if (node->tries == -1)	/* follow alias link */
	node = node->u.alias;

    while (datalen > 0) {	/* loop through interfaces */
	u_int32		ifc_addr;
	u_char		metric, threshold, ncount, flags;
	Node   	       *ifc_node;
	Interface      *ifc;
	Neighbor       *old_neighbors;

	if (datalen < 4 + 4) {
	    log(LOG_WARNING, 0, "received truncated interface record from %s",
		inet_fmt(src, s1));
	    return;
	}

	ifc_addr = *(u_int32*)p;
	p += 4;
	metric = *p++;
	threshold = *p++;
	flags = *p++;
	ncount = *p++;
	datalen -= 4 + 4;

	if (broken_cisco && ncount == 0)	/* dumb Ciscos */
		ncount = 1;
	if (broken_cisco && ncount > 15)	/* dumb Ciscos */
		ncount = ncount & 0xf;

	/* Fix up any alias information */
	ifc_node = find_node(ifc_addr, &routers);
	if (ifc_node->tries == 0) { /* new node */
	    ifc_node->tries = -1;
	    ifc_node->u.alias = node;
	} else if (ifc_node != node
		   && (ifc_node->tries > 0  ||  ifc_node->u.alias != node)) {
	    /* must merge two hosts' nodes */
	    Interface  *ifc_i, *next_ifc_i;

	    if (ifc_node->tries == -1) {
		Node *tmp = ifc_node->u.alias;

		ifc_node->u.alias = node;
		ifc_node = tmp;
	    }

	    /* Merge ifc_node (foo_i) into node (foo_n) */

	    if (ifc_node->tries > node->tries)
		node->tries = ifc_node->tries;

	    for (ifc_i = ifc_node->u.interfaces; ifc_i; ifc_i = next_ifc_i) {
		Neighbor *nb_i, *next_nb_i, *nb_n;
		Interface *ifc_n = find_interface(ifc_i->addr, node);

		old_neighbors = ifc_n->neighbors;
		for (nb_i = ifc_i->neighbors; nb_i; nb_i = next_nb_i) {
		    next_nb_i = nb_i->next;
		    for (nb_n = old_neighbors; nb_n; nb_n = nb_n->next)
			if (nb_i->addr == nb_n->addr) {
			    if (nb_i->metric != nb_n->metric
				|| nb_i->threshold != nb_i->threshold)
				log(LOG_WARNING, 0,
				    "inconsistent %s for neighbor %s of %s",
				    "metric/threshold",
				    inet_fmt(nb_i->addr, s1),
				    inet_fmt(node->addr, s2));
			    free(nb_i);
			    break;
			}
		    if (!nb_n) { /* no match for this neighbor yet */
			nb_i->next = ifc_n->neighbors;
			ifc_n->neighbors = nb_i;
		    }
		}

		next_ifc_i = ifc_i->next;
		free(ifc_i);
	    }

	    ifc_node->tries = -1;
	    ifc_node->u.alias = node;
	}
	
	ifc = find_interface(ifc_addr, node);
	old_neighbors = ifc->neighbors;
	
	/* Add the neighbors for this interface */
	while (ncount-- && datalen > 0) {
	    u_int32 	neighbor;
	    Neighbor   *nb;
	    Node       *n_node;

	    if (datalen < 4) {
		log(LOG_WARNING, 0, "received truncated neighbor list from %s",
		    inet_fmt(src, s1));
		return;
	    }

	    neighbor = *(u_int32*)p;
	    p += 4;
	    datalen -= 4;
	    if (neighbor == 0)
		/* make leaf nets point to themselves */
		neighbor = ifc_addr;

	    for (nb = old_neighbors; nb; nb = nb->next)
		if (nb->addr == neighbor) {
		    if (metric != nb->metric || threshold != nb->threshold)
			log(LOG_WARNING, 0,
			    "inconsistent %s for neighbor %s of %s",
			    "metric/threshold",
			    inet_fmt(nb->addr, s1), inet_fmt(node->addr, s2));
		    goto next_neighbor;
		}

	    nb = (Neighbor *) malloc(sizeof(Neighbor));
	    nb->next = ifc->neighbors;
	    ifc->neighbors = nb;
	    nb->addr = neighbor;
	    nb->metric = metric;
	    nb->threshold = threshold;
	    nb->flags = flags | NF_PRESENT;

	    n_node = find_node(neighbor, &routers);
	    if (n_node->tries == 0  &&  !target_addr) { /* it's a new router */
		ask(neighbor);
		n_node->tries = 1;
	    }

	  next_neighbor: ;
	}
    }
}


void check_vif_state()
{
    log(LOG_NOTICE, 0, "network marked down...");
}


int retry_requests(node)
    Node *node;
{
    int	result;

    if (node) {
	result = retry_requests(node->left);
	if (node->tries > 0  &&  node->tries < retries) {
	    if (node->version)
		ask2(node->addr);
	    else
		ask(node->addr);
	    node->tries++;
	    result = 1;
	}
	return retry_requests(node->right) || result;
    } else
	return 0;
}


char *inet_name(addr)
    u_int32 addr;
{
    struct hostent *e;

    e = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);

    return e ? e->h_name : 0;
}


void print_map(node)
    Node *node;
{
    if (node) {
	char *name, *addr;
	
	print_map(node->left);

	addr = inet_fmt(node->addr, s1);
	if (!target_addr
	    || (node->tries >= 0 && node->u.interfaces)
	    || (node->tries == -1
		&& node->u.alias->tries >= 0
		&& node->u.alias->u.interfaces)) {
	    if (show_names && (name = inet_name(node->addr)))
		printf("%s (%s):", addr, name);
	    else
		printf("%s:", addr);
	    if (node->tries < 0)
		printf(" alias for %s\n\n", inet_fmt(node->u.alias->addr, s1));
	    else if (!node->u.interfaces)
		printf(" no response to query\n\n");
	    else {
		Interface *ifc;

		if (node->version)
		    printf(" <v%d.%d>", node->version & 0xff,
					(node->version >> 8) & 0xff);
		printf("\n");
		for (ifc = node->u.interfaces; ifc; ifc = ifc->next) {
		    Neighbor *nb;
		    char *ifc_name = inet_fmt(ifc->addr, s1);
		    int ifc_len = strlen(ifc_name);
		    int count = 0;

		    printf("    %s:", ifc_name);
		    for (nb = ifc->neighbors; nb; nb = nb->next) {
			if (count > 0)
			    printf("%*s", ifc_len + 5, "");
			printf("  %s", inet_fmt(nb->addr, s1));
			if (show_names  &&  (name = inet_name(nb->addr)))
			    printf(" (%s)", name);
			printf(" [%d/%d", nb->metric, nb->threshold);
			if (nb->flags) {
			    u_short flags = nb->flags;
			    if (flags & DVMRP_NF_TUNNEL)
				    printf("/tunnel");
			    if (flags & DVMRP_NF_SRCRT)
				    printf("/srcrt");
			    if (flags & DVMRP_NF_QUERIER)
				    printf("/querier");
			    if (flags & DVMRP_NF_DISABLED)
				    printf("/disabled");
			    if (flags & DVMRP_NF_DOWN)
				    printf("/down");
			}
                        printf("]\n");
			count++;
		    }
		}
		printf("\n");
	    }
	}
	print_map(node->right);
    }
}


char *graph_name(addr, buf)
    u_int32 addr;
    char *buf;
{
    char *name;

    if (show_names  &&  (name = inet_name(addr)))
	strcpy(buf, name);
    else
	inet_fmt(addr, buf);

    return buf;
}


void graph_edges(node)
    Node *node;
{
    Interface *ifc;
    Neighbor *nb;
    char name[100];

    if (node) {
	graph_edges(node->left);
	if (node->tries >= 0) {
	    printf("  %d {$ NP %d0 %d0 $} \"%s%s\" \n",
		   (int) node->addr,
		   node->addr & 0xFF, (node->addr >> 8) & 0xFF,
		   graph_name(node->addr, name),
		   node->u.interfaces ? "" : "*");
	    for (ifc = node->u.interfaces; ifc; ifc = ifc->next)
		for (nb = ifc->neighbors; nb; nb = nb->next) {
		    Node *nb_node = find_node(nb->addr, &routers);
		    Neighbor *nb2;

		    if (nb_node->tries < 0)
			nb_node = nb_node->u.alias;

		    if (node != nb_node &&
			(!(nb2 = find_neighbor(node->addr, nb_node))
			 || node->addr < nb_node->addr)) {
			printf("    %d \"%d/%d",
			       nb_node->addr, nb->metric, nb->threshold);
			if (nb2 && (nb2->metric != nb->metric
				    || nb2->threshold != nb->threshold))
			    printf(",%d/%d", nb2->metric, nb2->threshold);
			if (nb->flags & NF_PRESENT)
			    printf("%s%s",
				   nb->flags & DVMRP_NF_SRCRT ? "" :
				   nb->flags & DVMRP_NF_TUNNEL ? "E" : "P",
				   nb->flags & DVMRP_NF_DOWN ? "D" : "");
			printf("\"\n");
		    }
		}
	    printf("    ;\n");
	}
	graph_edges(node->right);
    }
}

void elide_aliases(node)
    Node *node;
{
    if (node) {
	elide_aliases(node->left);
	if (node->tries >= 0) {
	    Interface *ifc;

	    for (ifc = node->u.interfaces; ifc; ifc = ifc->next) {
		Neighbor *nb;

		for (nb = ifc->neighbors; nb; nb = nb->next) {
		    Node *nb_node = find_node(nb->addr, &routers);

		    if (nb_node->tries < 0)
			nb->addr = nb_node->u.alias->addr;
		}
	    }
	}
	elide_aliases(node->right);
    }
}

void graph_map()
{
    time_t now = time(0);
    char *nowstr = ctime(&now);

    nowstr[24] = '\0';		/* Kill the newline at the end */
    elide_aliases(routers);
    printf("GRAPH \"Multicast Router Connectivity: %s\" = UNDIRECTED\n",
	   nowstr);
    graph_edges(routers);
    printf("END\n");
}


int get_number(var, deflt, pargv, pargc)
    int *var, *pargc, deflt;
    char ***pargv;
{
    if ((*pargv)[0][2] == '\0') { /* Get the value from the next argument */
	if (*pargc > 1  &&  isdigit((*pargv)[1][0])) {
	    (*pargv)++, (*pargc)--;
	    *var = atoi((*pargv)[0]);
	    return 1;
	} else if (deflt >= 0) {
	    *var = deflt;
	    return 1;
	} else
	    return 0;
    } else {			/* Get value from the rest of this argument */
	if (isdigit((*pargv)[0][2])) {
	    *var = atoi((*pargv)[0] + 2);
	    return 1;
	} else {
	    return 0;
	}
    }
}


u_int32 host_addr(name)
    char *name;
{
    struct hostent *e = gethostbyname(name);
    int addr;

    if (e)
	memcpy(&addr, e->h_addr_list[0], e->h_length);
    else {
	addr = inet_addr(name);
	if (addr == -1)
	    addr = 0;
    }

    return addr;
}


int main(argc, argv)
    int argc;
    char *argv[];
{
    int flood = FALSE, graph = FALSE;
    
    if (geteuid() != 0) {
	fprintf(stderr, "map-mbone: must be root\n");
	exit(1);
    }

    init_igmp();
    setuid(getuid());

    setlinebuf(stderr);

    argv++, argc--;
    while (argc > 0 && argv[0][0] == '-') {
	switch (argv[0][1]) {
	  case 'd':
	    if (!get_number(&debug, DEFAULT_DEBUG, &argv, &argc))
		goto usage;
	    break;
	  case 'f':
	    flood = TRUE;
	    break;
	  case 'g':
	    graph = TRUE;
	    break;
	  case 'n':
	    show_names = FALSE;
	    break;
	  case 'r':
	    if (!get_number(&retries, -1, &argv, &argc))
		goto usage;
	    break;
	  case 't':
	    if (!get_number(&timeout, -1, &argv, &argc))
		goto usage;
	    break;
	  default:
	    goto usage;
	}
	argv++, argc--;
    }

    if (argc > 1) {
      usage:	
	fprintf(stderr,
		"Usage: map-mbone [-f] [-g] [-n] [-t timeout] %s\n\n",
		"[-r retries] [-d [debug-level]] [router]");
        fprintf(stderr, "\t-f  Flood the routing graph with queries\n");
        fprintf(stderr, "\t    (True by default unless `router' is given)\n");
        fprintf(stderr, "\t-g  Generate output in GraphEd format\n");
        fprintf(stderr, "\t-n  Don't look up DNS names for routers\n");
	exit(1);
    } else if (argc == 1 && !(target_addr = host_addr(argv[0]))) {
	fprintf(stderr, "Unknown host: %s\n", argv[0]);
	exit(2);
    }

    if (debug)
	fprintf(stderr, "Debug level %u\n", debug);

    {				/* Find a good local address for us. */
	int udp;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);

	addr.sin_family = AF_INET;
#if (defined(BSD) && (BSD >= 199103))
	addr.sin_len = sizeof addr;
#endif
	addr.sin_addr.s_addr = dvmrp_group;
	addr.sin_port = htons(2000); /* any port over 1024 will do... */
	if ((udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0
	    || connect(udp, (struct sockaddr *) &addr, sizeof(addr)) < 0
	    || getsockname(udp, (struct sockaddr *) &addr, &addrlen) < 0) {
	    perror("Determining local address");
	    exit(-1);
	}
	close(udp);
	our_addr = addr.sin_addr.s_addr;
    }

    /* Send initial seed message to all local routers */
    ask(target_addr ? target_addr : allhosts_group);

    if (target_addr) {
	Node *n = find_node(target_addr, &routers);

	n->tries = 1;

	if (flood)
	    target_addr = 0;
    }

    /* Main receive loop */
    for(;;) {
	fd_set		fds;
	struct timeval 	tv;
	int 		count, recvlen, dummy = 0;

	FD_ZERO(&fds);
	FD_SET(igmp_socket, &fds);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	count = select(igmp_socket + 1, &fds, 0, 0, &tv);

	if (count < 0) {
	    if (errno != EINTR)
		perror("select");
	    continue;
	} else if (count == 0) {
	    log(LOG_DEBUG, 0, "Timed out receiving neighbor lists");
	    if (retry_requests(routers))
		continue;
	    else
		break;
	}

	recvlen = recvfrom(igmp_socket, recv_buf, RECV_BUF_SIZE,
			   0, NULL, &dummy);
	if (recvlen >= 0)
	    accept_igmp(recvlen);
	else if (errno != EINTR)
	    perror("recvfrom");
    }

    printf("\n");

    if (graph)
	graph_map();
    else {
	if (!target_addr)
	    printf("Multicast Router Connectivity:\n\n");
	print_map(routers);
    }

    exit(0);
}

/* dummies */
void accept_prune(src, dst, p, datalen)
	u_int32 src, dst;
	char *p;
	int datalen;
{
}
void accept_graft(src, dst, p, datalen)
	u_int32 src, dst;
	char *p;
	int datalen;
{
}
void accept_g_ack(src, dst, p, datalen)
	u_int32 src, dst;
	char *p;
	int datalen;
{
}
void add_table_entry(origin, mcastgrp)
	u_int32 origin, mcastgrp;
{
}
void accept_leave_message(src, dst, group)
	u_int32 src, dst, group;
{
}
void accept_mtrace(src, dst, group, data, no, datalen)
	u_int32 src, dst, group;
	char *data;
	u_int no;
	int datalen;
{
}
void accept_membership_query(src, dst, group, tmo)
	u_int32 src, dst, group;
	int tmo;
{
}
void accept_info_request(src, dst, p, datalen)
	u_int32 src, dst;
	u_char *p;
	int datalen;
{
}
void accept_info_reply(src, dst, p, datalen)
	u_int32 src, dst;
	u_char *p;
	int datalen;
{
}
