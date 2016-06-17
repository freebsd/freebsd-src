/* Kernel module to match AH parameters. */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ipv6.h>
#include <linux/types.h>
#include <net/checksum.h>
#include <net/ipv6.h>

#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_ah.h>

EXPORT_NO_SYMBOLS;
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IPv6 AH match");
MODULE_AUTHOR("Andras Kis-Szabo <kisza@sch.bme.hu>");

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

struct ahhdr {
       __u8    nexthdr;
       __u8    hdrlen;
       __u16   reserved;
       __u32   spi;
};

/* Returns 1 if the spi is matched by the range, 0 otherwise */
static inline int
spi_match(u_int32_t min, u_int32_t max, u_int32_t spi, int invert)
{
       int r=0;
       DEBUGP("ah spi_match:%c 0x%x <= 0x%x <= 0x%x",invert? '!':' ',
              min,spi,max);
       r=(spi >= min && spi <= max) ^ invert;
       DEBUGP(" result %s\n",r? "PASS\n" : "FAILED\n");
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
       struct ahhdr *ah = NULL;
       const struct ip6t_ah *ahinfo = matchinfo;
       unsigned int temp;
       int len;
       u8 nexthdr;
       unsigned int ptr;
       unsigned int hdrlen = 0;

       /*DEBUGP("IPv6 AH entered\n");*/
       /* if (opt->auth == 0) return 0;
       * It does not filled on output */

       /* type of the 1st exthdr */
       nexthdr = skb->nh.ipv6h->nexthdr;
       /* pointer to the 1st exthdr */
       ptr = sizeof(struct ipv6hdr);
       /* available length */
       len = skb->len - ptr;
       temp = 0;

        while (ip6t_ext_hdr(nexthdr)) {
               struct ipv6_opt_hdr *hdr;

              DEBUGP("ipv6_ah header iteration \n");

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

              /* AH -> evaluate */
                if (nexthdr == NEXTHDR_AUTH) {
                     temp |= MASK_AH;
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
                            DEBUGP("ipv6_ah match: unknown nextheader %u\n",nexthdr);
                            return 0;
                            break;
              }

                nexthdr = hdr->nexthdr;
                len -= hdrlen;
                ptr += hdrlen;
		if ( ptr > skb->len ) {
			DEBUGP("ipv6_ah: new pointer too large! \n");
			break;
		}
        }

       /* AH header not found */
       if ( temp != MASK_AH ) return 0;

       if (len < (int)sizeof(struct ahhdr)){
	       *hotdrop = 1;
       		return 0;
       }

       ah = (struct ahhdr *) (skb->data + ptr);

       DEBUGP("IPv6 AH LEN %u %u ", hdrlen, ah->hdrlen);
       DEBUGP("RES %04X ", ah->reserved);
       DEBUGP("SPI %u %08X\n", ntohl(ah->spi), ntohl(ah->spi));

       DEBUGP("IPv6 AH spi %02X ",
       		(spi_match(ahinfo->spis[0], ahinfo->spis[1],
                           ntohl(ah->spi),
                           !!(ahinfo->invflags & IP6T_AH_INV_SPI))));
       DEBUGP("len %02X %04X %02X ",
       		ahinfo->hdrlen, hdrlen,
       		(!ahinfo->hdrlen ||
                           (ahinfo->hdrlen == hdrlen) ^
                           !!(ahinfo->invflags & IP6T_AH_INV_LEN)));
       DEBUGP("res %02X %04X %02X\n", 
       		ahinfo->hdrres, ah->reserved,
       		!(ahinfo->hdrres && ah->reserved));

       return (ah != NULL)
              &&
              (spi_match(ahinfo->spis[0], ahinfo->spis[1],
                           ntohl(ah->spi),
                           !!(ahinfo->invflags & IP6T_AH_INV_SPI)))
              &&
              (!ahinfo->hdrlen ||
                           (ahinfo->hdrlen == hdrlen) ^
                           !!(ahinfo->invflags & IP6T_AH_INV_LEN))
              &&
              !(ahinfo->hdrres && ah->reserved);
}

/* Called when user tries to insert an entry of this type. */
static int
checkentry(const char *tablename,
          const struct ip6t_ip6 *ip,
          void *matchinfo,
          unsigned int matchinfosize,
          unsigned int hook_mask)
{
       const struct ip6t_ah *ahinfo = matchinfo;

       if (matchinfosize != IP6T_ALIGN(sizeof(struct ip6t_ah))) {
              DEBUGP("ip6t_ah: matchsize %u != %u\n",
                      matchinfosize, IP6T_ALIGN(sizeof(struct ip6t_ah)));
              return 0;
       }
       if (ahinfo->invflags & ~IP6T_AH_INV_MASK) {
              DEBUGP("ip6t_ah: unknown flags %X\n",
                      ahinfo->invflags);
              return 0;
       }

       return 1;
}

static struct ip6t_match ah_match
= { { NULL, NULL }, "ah", &match, &checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
       return ip6t_register_match(&ah_match);
}

static void __exit cleanup(void)
{
       ip6t_unregister_match(&ah_match);
}

module_init(init);
module_exit(cleanup);
