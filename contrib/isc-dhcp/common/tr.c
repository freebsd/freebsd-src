/*
 * packet_tr.c - token ring interface code, contributed in May of 1999
 * by Andrew Chittenden
 */

#include "dhcpd.h"

#if defined (HAVE_TR_SUPPORT) && \
	(defined (PACKET_ASSEMBLY) || defined (PACKET_DECODING))
#include "includes/netinet/ip.h"
#include "includes/netinet/udp.h"
#include "includes/netinet/if_ether.h"
#include "netinet/if_tr.h"
#include <sys/time.h>

/*
 * token ring device handling subroutines.  These are required as token-ring
 * does not have a simple on-the-wire header but requires the use of
 * source routing
 */

static int insert_source_routing PROTO ((struct trh_hdr *trh, struct interface_info* interface));
static void save_source_routing PROTO ((struct trh_hdr *trh, struct interface_info* interface));
static void expire_routes PROTO ((void));

/*
 * As we keep a list of interesting routing information only, a singly
 * linked list is all we need
 */
struct routing_entry {
        struct routing_entry *next;
        unsigned char addr[TR_ALEN];
        unsigned char iface[5];
        __u16 rcf;                      /* route control field */
        __u16 rseg[8];                  /* routing registers */
        unsigned long access_time;      /* time we last used this entry */
};

static struct routing_entry *routing_info = NULL;

static int routing_timeout = 10;
static struct timeval routing_timer;

void assemble_tr_header (interface, buf, bufix, to)
	struct interface_info *interface;
	unsigned char *buf;
	int *bufix;
	struct hardware *to;
{
        struct trh_hdr *trh;
        int hdr_len;
        struct trllc *llc;


        /* set up the token header */
        trh = (struct trh_hdr *) &buf[*bufix];
        if (interface -> hw_address.hlen == sizeof (trh->saddr))
                memcpy (trh->saddr, interface -> hw_address.haddr,
                                    sizeof (trh->saddr));
        else
                memset (trh->saddr, 0x00, sizeof (trh->saddr));

        if (to && to -> hlen == 6) /* XXX */
                memcpy (trh->daddr, to -> haddr, sizeof trh->daddr);
        else
                memset (trh->daddr, 0xff, sizeof (trh->daddr));

	hdr_len = insert_source_routing (trh, interface);

        trh->ac = AC;
        trh->fc = LLC_FRAME;

        /* set up the llc header for snap encoding after the tr header */
        llc = (struct trllc *)(buf + *bufix + hdr_len);
        llc->dsap = EXTENDED_SAP;
        llc->ssap = EXTENDED_SAP;
        llc->llc = UI_CMD;
        llc->protid[0] = 0;
        llc->protid[1] = 0;
        llc->protid[2] = 0;
        llc->ethertype = htons(ETHERTYPE_IP);

        hdr_len += sizeof(struct trllc);

        *bufix += hdr_len;
}


static unsigned char tr_broadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*
 * decoding the token header is a bit complex as you can see here. It is
 * further complicated by the linux kernel stripping off some valuable
 * information (see comment below) even though we've asked for the raw
 * packets.
 */
ssize_t decode_tr_header (interface, buf, bufix, from)
        struct interface_info *interface;
        unsigned char *buf;
        int bufix;
        struct hardware *from;
{
        struct trh_hdr *trh = (struct trh_hdr *) buf + bufix;
        struct trllc *llc;
	struct ip *ip;
	struct udphdr *udp;
        unsigned int route_len = 0;
        ssize_t hdr_len;
        struct timeval now;

        /* see whether any source routing information has expired */
        gettimeofday(&now, NULL);

	if (routing_timer.tv_sec == 0)
                routing_timer.tv_sec = now.tv_sec + routing_timeout;
        else if ((now.tv_sec - routing_timer.tv_sec) > 0)
                expire_routes();

        /* the kernel might have stripped off the source
         * routing bit. We try a heuristic to determine whether
         * this is the case and put it back on if so
         */
        route_len = (ntohs(trh->rcf) & TR_RCF_LEN_MASK) >> 8;
        llc = (struct trllc *)(buf + bufix + sizeof(struct trh_hdr)-TR_MAXRIFLEN+route_len);
        if (llc->dsap == EXTENDED_SAP
                        && llc->ssap == EXTENDED_SAP
                        && llc->llc == UI_CMD
                        && llc->protid[0] == 0
                        && llc->protid[1] == 0
                        && llc->protid[2] == 0) {
                /* say there is source routing information present */
                trh->saddr[0] |= TR_RII;	
        }

	if (trh->saddr[0] & TR_RII)
                route_len = (ntohs(trh->rcf) & TR_RCF_LEN_MASK) >> 8;
        else
                route_len = 0;

        hdr_len = sizeof (struct trh_hdr) - TR_MAXRIFLEN + route_len;

        /* now filter out unwanted packets: this is based on the packet
         * filter code in bpf.c */
        llc = (struct trllc *)(buf + bufix + hdr_len);
        ip = (struct ip *) (llc + 1);
        udp = (struct udphdr *) ((unsigned char*) ip + ip->ip_hl * 4);

        /* make sure it is a snap encoded, IP, UDP, unfragmented packet sent
         * to our port */
        if (llc->dsap != EXTENDED_SAP
                        || ntohs(llc->ethertype) != ETHERTYPE_IP
                        || ip->ip_p != IPPROTO_UDP
                        || (ip->ip_off & IP_OFFMASK) != 0
                        || udp->uh_dport != local_port)
                return -1;

        /* only save source routing information for packets from valued hosts */
        save_source_routing(trh, interface);

        return hdr_len + sizeof (struct trllc);
}

/* insert_source_routing inserts source route information into the token ring
 * header
 */
static int insert_source_routing (trh, interface)
        struct trh_hdr *trh;
        struct interface_info* interface;
{
	struct routing_entry *rover;
        struct timeval now;
        unsigned int route_len = 0;

        gettimeofday(&now, NULL);

	/* single route broadcasts as per rfc 1042 */
	if (memcmp(trh->daddr, tr_broadcast,TR_ALEN) == 0) {
		trh->saddr[0] |= TR_RII;
		trh->rcf = ((sizeof(trh->rcf)) << 8) & TR_RCF_LEN_MASK;  
                trh->rcf |= (TR_RCF_FRAME2K | TR_RCF_LIMITED_BROADCAST);
		trh->rcf = htons(trh->rcf);
	} else {
		/* look for a routing entry */
                for (rover = routing_info; rover != NULL; rover = rover->next) {
                        if (memcmp(rover->addr, trh->daddr, TR_ALEN) == 0)
                                break;
                }

		if (rover != NULL) {
                        /* success: route that frame */
                        if ((rover->rcf & TR_RCF_LEN_MASK) >> 8) {
                                __u16 rcf = rover->rcf;
				memcpy(trh->rseg,rover->rseg,sizeof(trh->rseg));
				rcf ^= TR_RCF_DIR_BIT;	
				rcf &= ~TR_RCF_BROADCAST_MASK;
                                trh->rcf = htons(rcf);
				trh->saddr[0] |= TR_RII;
			}
			rover->access_time = now.tv_sec;
		} else {
                        /* we don't have any routing information so send a
                         * limited broadcast */
                        trh->saddr[0] |= TR_RII;
                        trh->rcf = ((sizeof(trh->rcf)) << 8) & TR_RCF_LEN_MASK;  
                        trh->rcf |= (TR_RCF_FRAME2K | TR_RCF_LIMITED_BROADCAST);
                        trh->rcf = htons(trh->rcf);
		}
	}

	/* return how much of the header we've actually used */
	if (trh->saddr[0] & TR_RII)
                route_len = (ntohs(trh->rcf) & TR_RCF_LEN_MASK) >> 8;
        else
                route_len = 0;

        return sizeof (struct trh_hdr) - TR_MAXRIFLEN + route_len;
}

/*
 * save any source routing information
 */
static void save_source_routing(trh, interface)
        struct trh_hdr *trh;
        struct interface_info *interface;
{
        struct routing_entry *rover;
        struct timeval now;
        unsigned char saddr[TR_ALEN];
        __u16 rcf = 0;

        gettimeofday(&now, NULL);

        memcpy(saddr, trh->saddr, sizeof(saddr));
        saddr[0] &= 0x7f;   /* strip off source routing present flag */

        /* scan our table to see if we've got it */
        for (rover = routing_info; rover != NULL; rover = rover->next) {
                if (memcmp(&rover->addr[0], &saddr[0], TR_ALEN) == 0)
                        break;
        }

        /* found an entry so update it with fresh information */
        if (rover != NULL) {
                if ((trh->saddr[0] & TR_RII)
		    && (((ntohs(trh->rcf) & TR_RCF_LEN_MASK) >> 8) > 2)) {
                        rcf = ntohs(trh->rcf);
                        rcf &= ~TR_RCF_BROADCAST_MASK;
                        memcpy(rover->rseg, trh->rseg, sizeof(rover->rseg));
                }
                rover->rcf = rcf;
                rover->access_time = now.tv_sec;
                return;     /* that's all folks */
        }

        /* no entry found, so create one */
        rover = malloc(sizeof(struct routing_entry));
        if (rover == NULL) {
                fprintf(stderr,
			"%s: unable to save source routing information\n",
			__FILE__);
                return;
        }

        memcpy(rover->addr, saddr, sizeof(rover->addr));
        memcpy(rover->iface, interface->name, 5);
        rover->access_time = now.tv_sec;
        if (trh->saddr[0] & TR_RII) {
                if (((ntohs(trh->rcf) & TR_RCF_LEN_MASK) >> 8) > 2) {
                        rcf = ntohs(trh->rcf);
                        rcf &= ~TR_RCF_BROADCAST_MASK;
                        memcpy(rover->rseg, trh->rseg, sizeof(rover->rseg));
                }
                rover->rcf = rcf;
        }

        /* insert into list */
        rover->next = routing_info;
        routing_info = rover;

        return;
}

/*
 * get rid of old routes
 */
static void expire_routes()
{
        struct routing_entry *rover;
        struct routing_entry **prover = &routing_info;
        struct timeval now;

        gettimeofday(&now, NULL);

        while((rover = *prover) != NULL) {
                if ((now.tv_sec - rover->access_time) > routing_timeout) {
                        *prover = rover->next;
                        free(rover);
                } else
                        prover = &rover->next;
        }

        /* Reset the timer */
        routing_timer.tv_sec = now.tv_sec + routing_timeout;
        routing_timer.tv_usec = now.tv_usec;
}

#endif
