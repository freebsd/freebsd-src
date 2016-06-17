/* Kernel module to match FRAG parameters. */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ipv6.h>
#include <linux/types.h>
#include <net/checksum.h>
#include <net/ipv6.h>

#include <asm/byteorder.h>

#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_frag.h>

EXPORT_NO_SYMBOLS;
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IPv6 FRAG match");
MODULE_AUTHOR("Andras Kis-Szabo <kisza@sch.bme.hu>");

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#if 0
#if     BYTE_ORDER == BIG_ENDIAN
#define IP6F_OFF_MASK       0xfff8  /* mask out offset from _offlg */
#define IP6F_RESERVED_MASK  0x0006  /* reserved bits in ip6f_offlg */
#define IP6F_MORE_FRAG      0x0001  /* more-fragments flag */
#else   /* BYTE_ORDER == LITTLE_ENDIAN */
#define IP6F_OFF_MASK       0xf8ff  /* mask out offset from _offlg */
#define IP6F_RESERVED_MASK  0x0600  /* reserved bits in ip6f_offlg */
#define IP6F_MORE_FRAG      0x0100  /* more-fragments flag */
#endif
#endif

#define IP6F_OFF_MASK       0xf8ff  /* mask out offset from _offlg */
#define IP6F_RESERVED_MASK  0x0600  /* reserved bits in ip6f_offlg */
#define IP6F_MORE_FRAG      0x0100  /* more-fragments flag */

struct fraghdr {
       __u8    nexthdr;
       __u8    hdrlen;
       __u16   info;
       __u32   id;
};

/* Returns 1 if the id is matched by the range, 0 otherwise */
static inline int
id_match(u_int32_t min, u_int32_t max, u_int32_t id, int invert)
{
       int r=0;
       DEBUGP("frag id_match:%c 0x%x <= 0x%x <= 0x%x",invert? '!':' ',
              min,id,max);
       r=(id >= min && id <= max) ^ invert;
       DEBUGP(" result %s\n",r? "PASS" : "FAILED");
       return r;
}

static int
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const void *matchinfo,
      int offset,
      const void *protohdr,
      u_int16_t datalen,
      int *hotdrop)
{
       struct fraghdr *frag = NULL;
       const struct ip6t_frag *fraginfo = matchinfo;
       unsigned int temp;
       int len;
       u8 nexthdr;
       unsigned int ptr;
       unsigned int hdrlen = 0;

       /* type of the 1st exthdr */
       nexthdr = skb->nh.ipv6h->nexthdr;
       /* pointer to the 1st exthdr */
       ptr = sizeof(struct ipv6hdr);
       /* available length */
       len = skb->len - ptr;
       temp = 0;

        while (ip6t_ext_hdr(nexthdr)) {
               struct ipv6_opt_hdr *hdr;

              DEBUGP("ipv6_frag header iteration \n");

              /* Is there enough space for the next ext header? */
                if (len < (int)sizeof(struct ipv6_opt_hdr))
                        return 0;
              /* No more exthdr -> evaluate */
                if (nexthdr == NEXTHDR_NONE) {
                     break;
              }
              /* ESP -> evaluate */
                if (nexthdr == NEXTHDR_ESP) {
                     break;
              }

              hdr=(struct ipv6_opt_hdr *)skb->data+ptr;

              /* Calculate the header length */
                if (nexthdr == NEXTHDR_FRAGMENT) {
                        hdrlen = 8;
                } else if (nexthdr == NEXTHDR_AUTH)
                        hdrlen = (hdr->hdrlen+2)<<2;
                else
                        hdrlen = ipv6_optlen(hdr);

              /* FRAG -> evaluate */
                if (nexthdr == NEXTHDR_FRAGMENT) {
                     temp |= MASK_FRAGMENT;
                     break;
              }


              /* set the flag */
              switch (nexthdr){
                     case NEXTHDR_HOP:
                     case NEXTHDR_ROUTING:
                     case NEXTHDR_FRAGMENT:
                     case NEXTHDR_AUTH:
                     case NEXTHDR_DEST:
                            break;
                     default:
                            DEBUGP("ipv6_frag match: unknown nextheader %u\n",nexthdr);
                            return 0;
                            break;
              }

                nexthdr = hdr->nexthdr;
                len -= hdrlen;
                ptr += hdrlen;
		if ( ptr > skb->len ) {
			DEBUGP("ipv6_frag: new pointer too large! \n");
			break;
		}
        }

       /* FRAG header not found */
       if ( temp != MASK_FRAGMENT ) return 0;

       if (len < (int)sizeof(struct fraghdr)){
	       *hotdrop = 1;
       		return 0;
       }

       frag = (struct fraghdr *) (skb->data + ptr);

       DEBUGP("IPv6 FRAG LEN %u %u ", hdrlen, frag->hdrlen);
       DEBUGP("INFO %04X ", frag->info);
       DEBUGP("OFFSET %04X ", frag->info & IP6F_OFF_MASK);
       DEBUGP("RES %04X ", frag->info & IP6F_RESERVED_MASK);
       DEBUGP("MF %04X ", frag->info & IP6F_MORE_FRAG);
       DEBUGP("ID %u %08X\n", ntohl(frag->id), ntohl(frag->id));

       DEBUGP("IPv6 FRAG id %02X ",
       		(id_match(fraginfo->ids[0], fraginfo->ids[1],
                           ntohl(frag->id),
                           !!(fraginfo->invflags & IP6T_FRAG_INV_IDS))));
       DEBUGP("len %02X %04X %02X ",
       		fraginfo->hdrlen, hdrlen,
       		(!fraginfo->hdrlen ||
                           (fraginfo->hdrlen == hdrlen) ^
                           !!(fraginfo->invflags & IP6T_FRAG_INV_LEN)));
       DEBUGP("res %02X %02X %02X ", 
       		(fraginfo->flags & IP6T_FRAG_RES), frag->info & IP6F_RESERVED_MASK,
       		!((fraginfo->flags & IP6T_FRAG_RES) && (frag->info & IP6F_RESERVED_MASK)));
       DEBUGP("first %02X %02X %02X ", 
       		(fraginfo->flags & IP6T_FRAG_FST), frag->info & IP6F_OFF_MASK,
       		!((fraginfo->flags & IP6T_FRAG_FST) && (frag->info & IP6F_OFF_MASK)));
       DEBUGP("mf %02X %02X %02X ", 
       		(fraginfo->flags & IP6T_FRAG_MF), frag->info & IP6F_MORE_FRAG,
       		!((fraginfo->flags & IP6T_FRAG_MF) && !((frag->info & IP6F_MORE_FRAG))));
       DEBUGP("last %02X %02X %02X\n", 
       		(fraginfo->flags & IP6T_FRAG_NMF), frag->info & IP6F_MORE_FRAG,
       		!((fraginfo->flags & IP6T_FRAG_NMF) && (frag->info & IP6F_MORE_FRAG)));

       return (frag != NULL)
       		&&
       		(id_match(fraginfo->ids[0], fraginfo->ids[1],
                           ntohl(frag->id),
                           !!(fraginfo->invflags & IP6T_FRAG_INV_IDS)))
		&&
	      	(!fraginfo->hdrlen ||
                           (fraginfo->hdrlen == hdrlen) ^
                           !!(fraginfo->invflags & IP6T_FRAG_INV_LEN))
		&&
		!((fraginfo->flags & IP6T_FRAG_RES) && (frag->info & IP6F_RESERVED_MASK))
		&&
		!((fraginfo->flags & IP6T_FRAG_FST) && (frag->info & IP6F_OFF_MASK))
		&&
		!((fraginfo->flags & IP6T_FRAG_MF) && !((frag->info & IP6F_MORE_FRAG)))
		&&
		!((fraginfo->flags & IP6T_FRAG_NMF) && (frag->info & IP6F_MORE_FRAG));
}

/* Called when user tries to insert an entry of this type. */
static int
checkentry(const char *tablename,
          const struct ip6t_ip6 *ip,
          void *matchinfo,
          unsigned int matchinfosize,
          unsigned int hook_mask)
{
       const struct ip6t_frag *fraginfo = matchinfo;

       if (matchinfosize != IP6T_ALIGN(sizeof(struct ip6t_frag))) {
              DEBUGP("ip6t_frag: matchsize %u != %u\n",
                      matchinfosize, IP6T_ALIGN(sizeof(struct ip6t_frag)));
              return 0;
       }
       if (fraginfo->invflags & ~IP6T_FRAG_INV_MASK) {
              DEBUGP("ip6t_frag: unknown flags %X\n",
                      fraginfo->invflags);
              return 0;
       }

       return 1;
}

static struct ip6t_match frag_match
= { { NULL, NULL }, "frag", &match, &checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
       return ip6t_register_match(&frag_match);
}

static void __exit cleanup(void)
{
       ip6t_unregister_match(&frag_match);
}

module_init(init);
module_exit(cleanup);
