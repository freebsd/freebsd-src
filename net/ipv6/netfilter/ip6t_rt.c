/* Kernel module to match ROUTING parameters. */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ipv6.h>
#include <linux/types.h>
#include <net/checksum.h>
#include <net/ipv6.h>

#include <asm/byteorder.h>

#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_rt.h>

EXPORT_NO_SYMBOLS;
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IPv6 RT match");
MODULE_AUTHOR("Andras Kis-Szabo <kisza@sch.bme.hu>");

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

/* Returns 1 if the id is matched by the range, 0 otherwise */
static inline int
segsleft_match(u_int32_t min, u_int32_t max, u_int32_t id, int invert)
{
       int r=0;
       DEBUGP("rt segsleft_match:%c 0x%x <= 0x%x <= 0x%x",invert? '!':' ',
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
       struct ipv6_rt_hdr *route = NULL;
       const struct ip6t_rt *rtinfo = matchinfo;
       unsigned int temp;
       unsigned int len;
       u8 nexthdr;
       unsigned int ptr;
       unsigned int hdrlen = 0;
       unsigned int ret = 0;

       /* type of the 1st exthdr */
       nexthdr = skb->nh.ipv6h->nexthdr;
       /* pointer to the 1st exthdr */
       ptr = sizeof(struct ipv6hdr);
       /* available length */
       len = skb->len - ptr;
       temp = 0;

        while (ip6t_ext_hdr(nexthdr)) {
               struct ipv6_opt_hdr *hdr;

              DEBUGP("ipv6_rt header iteration \n");

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

              /* ROUTING -> evaluate */
                if (nexthdr == NEXTHDR_ROUTING) {
                     temp |= MASK_ROUTING;
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
                            DEBUGP("ipv6_rt match: unknown nextheader %u\n",nexthdr);
                            return 0;
                            break;
              }

                nexthdr = hdr->nexthdr;
                len -= hdrlen;
                ptr += hdrlen;
		if ( ptr > skb->len ) {
			DEBUGP("ipv6_rt: new pointer is too large! \n");
			break;
		}
        }

       /* ROUTING header not found */
       if ( temp != MASK_ROUTING ) return 0;

       if (len < (int)sizeof(struct ipv6_rt_hdr)){
	       *hotdrop = 1;
       		return 0;
       }

       if (len < hdrlen){
	       /* Pcket smaller than its length field */
       		return 0;
       }

       route = (struct ipv6_rt_hdr *) (skb->data + ptr);

       DEBUGP("IPv6 RT LEN %u %u ", hdrlen, route->hdrlen);
       DEBUGP("TYPE %04X ", route->type);
       DEBUGP("SGS_LEFT %u %08X\n", ntohl(route->segments_left), ntohl(route->segments_left));

       DEBUGP("IPv6 RT segsleft %02X ",
       		(segsleft_match(rtinfo->segsleft[0], rtinfo->segsleft[1],
                           ntohl(route->segments_left),
                           !!(rtinfo->invflags & IP6T_RT_INV_SGS))));
       DEBUGP("type %02X %02X %02X ",
       		rtinfo->rt_type, route->type, 
       		(!(rtinfo->flags & IP6T_RT_TYP) ||
                           ((rtinfo->rt_type == route->type) ^
                           !!(rtinfo->invflags & IP6T_RT_INV_TYP))));
       DEBUGP("len %02X %04X %02X ",
       		rtinfo->hdrlen, hdrlen,
       		(!(rtinfo->flags & IP6T_RT_LEN) ||
                           ((rtinfo->hdrlen == hdrlen) ^
                           !!(rtinfo->invflags & IP6T_RT_INV_LEN))));
       DEBUGP("res %02X %02X %02X ", 
       		(rtinfo->flags & IP6T_RT_RES), ((struct rt0_hdr *)route)->bitmap,
       		!((rtinfo->flags & IP6T_RT_RES) && (((struct rt0_hdr *)route)->bitmap)));

       ret = (route != NULL)
       		&&
       		(segsleft_match(rtinfo->segsleft[0], rtinfo->segsleft[1],
                           ntohl(route->segments_left),
                           !!(rtinfo->invflags & IP6T_RT_INV_SGS)))
		&&
	      	(!(rtinfo->flags & IP6T_RT_LEN) ||
                           ((rtinfo->hdrlen == hdrlen) ^
                           !!(rtinfo->invflags & IP6T_RT_INV_LEN)))
		&&
       		(!(rtinfo->flags & IP6T_RT_TYP) ||
                           ((rtinfo->rt_type == route->type) ^
                           !!(rtinfo->invflags & IP6T_RT_INV_TYP)))
		&&
       		!((rtinfo->flags & IP6T_RT_RES) && (((struct rt0_hdr *)route)->bitmap));

	DEBUGP("#%d ",rtinfo->addrnr);
       temp = len = ptr = 0;
       if ( !(rtinfo->flags & IP6T_RT_FST) ){
	       return ret;
	} else if (rtinfo->flags & IP6T_RT_FST_NSTRICT) {
		DEBUGP("Not strict ");
		if ( rtinfo->addrnr > (unsigned int)((hdrlen-8)/16) ){
			DEBUGP("There isn't enough space\n");
			return 0;
		} else {
			DEBUGP("#%d ",rtinfo->addrnr);
			ptr = 0;
			for(temp=0; temp<(unsigned int)((hdrlen-8)/16); temp++){
				len = 0;
				while ((u8)(((struct rt0_hdr *)route)->
						addr[temp].s6_addr[len]) ==
					(u8)(rtinfo->addrs[ptr].s6_addr[len])){
					DEBUGP("%02X?%02X ",
		(u8)(((struct rt0_hdr *)route)->addr[temp].s6_addr[len]),
					(u8)(rtinfo->addrs[ptr].s6_addr[len]));
					len++;
					if ( len == 16 ) break;
				}
				if (len==16) {
					DEBUGP("ptr=%d temp=%d;\n",ptr,temp);
					ptr++;
				} else {
					DEBUGP("%02X?%02X ",
		(u8)(((struct rt0_hdr *)route)->addr[temp].s6_addr[len]),
					(u8)(rtinfo->addrs[ptr].s6_addr[len]));
					DEBUGP("!ptr=%d temp=%d;\n",ptr,temp);
				}
				if (ptr==rtinfo->addrnr) break;
			}
			DEBUGP("ptr=%d len=%d #%d\n",ptr,len, rtinfo->addrnr);
			if ( (len == 16) && (ptr == rtinfo->addrnr))
				return ret;
			else return 0;
		}
	} else {
		DEBUGP("Strict ");
		if ( rtinfo->addrnr > (unsigned int)((hdrlen-8)/16) ){
			DEBUGP("There isn't enough space\n");
			return 0;
		} else {
			DEBUGP("#%d ",rtinfo->addrnr);
			for(temp=0; temp<rtinfo->addrnr; temp++){
				len = 0;
				while ((u8)(((struct rt0_hdr *)route)->
						addr[temp].s6_addr[len]) ==
					(u8)(rtinfo->addrs[temp].s6_addr[len])){
					DEBUGP("%02X?%02X ",
		(u8)(((struct rt0_hdr *)route)->addr[temp].s6_addr[len]),
					(u8)(rtinfo->addrs[temp].s6_addr[len]));
					len++;
					if ( len == 16 ) break;
				}
				if (len!=16) {
					DEBUGP("%02X?%02X ",
		(u8)(((struct rt0_hdr *)route)->addr[temp].s6_addr[len]),
					(u8)(rtinfo->addrs[temp].s6_addr[len]));
					DEBUGP("!len=%d temp=%d;\n",len,temp);
					break;
				}
			}
			DEBUGP("temp=%d len=%d #%d\n",temp,len,rtinfo->addrnr);
			if ( (len == 16) && (temp == rtinfo->addrnr) && (temp == (unsigned int)((hdrlen-8)/16)))
				return ret;
			else return 0;
		}
	}

	return 0;
}

/* Called when user tries to insert an entry of this type. */
static int
checkentry(const char *tablename,
          const struct ip6t_ip6 *ip,
          void *matchinfo,
          unsigned int matchinfosize,
          unsigned int hook_mask)
{
       const struct ip6t_rt *rtinfo = matchinfo;

       if (matchinfosize != IP6T_ALIGN(sizeof(struct ip6t_rt))) {
              DEBUGP("ip6t_rt: matchsize %u != %u\n",
                      matchinfosize, IP6T_ALIGN(sizeof(struct ip6t_rt)));
              return 0;
       }
       if (rtinfo->invflags & ~IP6T_RT_INV_MASK) {
              DEBUGP("ip6t_rt: unknown flags %X\n",
                      rtinfo->invflags);
              return 0;
       }
       if ( (rtinfo->flags & (IP6T_RT_RES|IP6T_RT_FST_MASK)) && 
		       (!(rtinfo->flags & IP6T_RT_TYP) || 
		       (rtinfo->rt_type != 0) || 
		       (rtinfo->invflags & IP6T_RT_INV_TYP)) ) {
	      DEBUGP("`--rt-type 0' required before `--rt-0-*'");
              return 0;
       }

       return 1;
}

static struct ip6t_match rt_match
= { { NULL, NULL }, "rt", &match, &checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
       return ip6t_register_match(&rt_match);
}

static void __exit cleanup(void)
{
       ip6t_unregister_match(&rt_match);
}

module_init(init);
module_exit(cleanup);
