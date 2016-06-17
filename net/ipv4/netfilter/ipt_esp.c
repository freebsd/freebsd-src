/* Kernel module to match ESP parameters. */
#include <linux/module.h>
#include <linux/skbuff.h>

#include <linux/netfilter_ipv4/ipt_esp.h>
#include <linux/netfilter_ipv4/ip_tables.h>

EXPORT_NO_SYMBOLS;
MODULE_LICENSE("GPL");

#ifdef DEBUG_CONNTRACK
#define duprintf(format, args...) printk(format , ## args)
#else
#define duprintf(format, args...)
#endif

struct esphdr {
	__u32   spi;
	__u32   seq_no;
};

/* Returns 1 if the spi is matched by the range, 0 otherwise */
static inline int
spi_match(u_int32_t min, u_int32_t max, u_int32_t spi, int invert)
{
	int r=0;
        duprintf("esp spi_match:%c 0x%x <= 0x%x <= 0x%x",invert? '!':' ',
        	min,spi,max);
	r=(spi >= min && spi <= max) ^ invert;
	duprintf(" result %s\n",r? "PASS" : "FAILED");
	return r;
}

static int
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const void *matchinfo,
      int offset,
      const void *hdr,
      u_int16_t datalen,
      int *hotdrop)
{
	const struct esphdr *esp = hdr;
	const struct ipt_esp *espinfo = matchinfo;

	if (offset == 0 && datalen < sizeof(struct esphdr)) {
		/* We've been asked to examine this packet, and we
		   can't.  Hence, no choice but to drop. */
		duprintf("Dropping evil ESP tinygram.\n");
		*hotdrop = 1;
		return 0;
	}

	/* Must not be a fragment. */
	return !offset
		&& spi_match(espinfo->spis[0], espinfo->spis[1],
			      ntohl(esp->spi),
			      !!(espinfo->invflags & IPT_ESP_INV_SPI));
}

/* Called when user tries to insert an entry of this type. */
static int
checkentry(const char *tablename,
	   const struct ipt_ip *ip,
	   void *matchinfo,
	   unsigned int matchinfosize,
	   unsigned int hook_mask)
{
	const struct ipt_esp *espinfo = matchinfo;

	/* Must specify proto == ESP, and no unknown invflags */
	if (ip->proto != IPPROTO_ESP || (ip->invflags & IPT_INV_PROTO)) {
		duprintf("ipt_esp: Protocol %u != %u\n", ip->proto,
			 IPPROTO_ESP);
		return 0;
	}
	if (matchinfosize != IPT_ALIGN(sizeof(struct ipt_esp))) {
		duprintf("ipt_esp: matchsize %u != %u\n",
			 matchinfosize, IPT_ALIGN(sizeof(struct ipt_esp)));
		return 0;
	}
	if (espinfo->invflags & ~IPT_ESP_INV_MASK) {
		duprintf("ipt_esp: unknown flags %X\n",
			 espinfo->invflags);
		return 0;
	}

	return 1;
}

static struct ipt_match esp_match
= { { NULL, NULL }, "esp", &match, &checkentry, NULL, THIS_MODULE };

static int __init init(void)
{
	return ipt_register_match(&esp_match);
}

static void __exit cleanup(void)
{
	ipt_unregister_match(&esp_match);
}

module_init(init);
module_exit(cleanup);
