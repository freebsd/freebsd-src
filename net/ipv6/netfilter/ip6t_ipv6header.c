/* ipv6header match - matches IPv6 packets based
on whether they contain certain headers */

/* Original idea: Brad Chapman 
 * Rewritten by: Andras Kis-Szabo <kisza@sch.bme.hu> */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ipv6.h>
#include <linux/types.h>
#include <net/checksum.h>
#include <net/ipv6.h>

#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_ipv6header.h>

EXPORT_NO_SYMBOLS;
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IPv6 headers match");
MODULE_AUTHOR("Andras Kis-Szabo <kisza@sch.bme.hu>");

static int
ipv6header_match(const struct sk_buff *skb,
		 const struct net_device *in,
		 const struct net_device *out,
		 const void *matchinfo,
		 int offset,
		 const void *protohdr,
		 u_int16_t datalen,
		 int *hotdrop)
{
	const struct ip6t_ipv6header_info *info = matchinfo;
	unsigned int temp;
	int len;
	u8 nexthdr;
	unsigned int ptr;

	/* Make sure this isn't an evil packet */

	/* type of the 1st exthdr */
	nexthdr = skb->nh.ipv6h->nexthdr;
	/* pointer to the 1st exthdr */
	ptr = sizeof(struct ipv6hdr);
	/* available length */
	len = skb->len - ptr;
	temp = 0;

        while (ip6t_ext_hdr(nexthdr)) {
        	struct ipv6_opt_hdr *hdr;
        	int hdrlen;

		/* Is there enough space for the next ext header? */
                if (len < (int)sizeof(struct ipv6_opt_hdr))
                        return 0;
		/* No more exthdr -> evaluate */
                if (nexthdr == NEXTHDR_NONE) {
			temp |= MASK_NONE;
			break;
		}
		/* ESP -> evaluate */
                if (nexthdr == NEXTHDR_ESP) {
			temp |= MASK_ESP;
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

		/* set the flag */
		switch (nexthdr){
			case NEXTHDR_HOP:
				temp |= MASK_HOPOPTS;
				break;
			case NEXTHDR_ROUTING:
				temp |= MASK_ROUTING;
				break;
			case NEXTHDR_FRAGMENT:
				temp |= MASK_FRAGMENT;
				break;
			case NEXTHDR_AUTH:
				temp |= MASK_AH;
				break;
			case NEXTHDR_DEST:
				temp |= MASK_DSTOPTS;
				break;
			default:
				return 0;
				break;
		}

                nexthdr = hdr->nexthdr;
                len -= hdrlen;
                ptr += hdrlen;
		if ( ptr > skb->len ) {
			break;
		}
        }

	if ( (nexthdr != NEXTHDR_NONE ) && (nexthdr != NEXTHDR_ESP) )
		temp |= MASK_PROTO;

	if (info->modeflag)
		return (!( (temp & info->matchflags)
			^ info->matchflags) ^ info->invflags);
	else
		return (!( temp ^ info->matchflags) ^ info->invflags);
}

static int
ipv6header_checkentry(const char *tablename,
		      const struct ip6t_ip6 *ip,
		      void *matchinfo,
		      unsigned int matchsize,
		      unsigned int hook_mask)
{
	/* Check for obvious errors */
	/* This match is valid in all hooks! */
	if (matchsize != IP6T_ALIGN(sizeof(struct ip6t_ipv6header_info))) {
		return 0;
	}

	return 1;
}

static struct ip6t_match
ip6t_ipv6header_match = {
	{ NULL, NULL },
	"ipv6header",
	&ipv6header_match,
	&ipv6header_checkentry,
	NULL,
	THIS_MODULE
};

static int  __init ipv6header_init(void)
{
	return ip6t_register_match(&ip6t_ipv6header_match);
}

static void __exit ipv6header_exit(void)
{
	ip6t_unregister_match(&ip6t_ipv6header_match);
}

module_init(ipv6header_init);
module_exit(ipv6header_exit);

