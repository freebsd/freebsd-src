#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/datalink.h>
#include <linux/mm.h>
#include <linux/in.h>

static void
pEII_datalink_header(struct datalink_proto *dl, 
		struct sk_buff *skb, unsigned char *dest_node)
{
	struct net_device	*dev = skb->dev;

	skb->protocol = htons (ETH_P_IPX);
	if(dev->hard_header)
		dev->hard_header(skb, dev, ETH_P_IPX, dest_node, NULL, skb->len);
}

struct datalink_proto *
make_EII_client(void)
{
	struct datalink_proto	*proto;

	proto = (struct datalink_proto *) kmalloc(sizeof(*proto), GFP_ATOMIC);
	if (proto != NULL) {
		proto->type_len = 0;
		proto->header_length = 0;
		proto->datalink_header = pEII_datalink_header;
		proto->string_name = "EtherII";
	}

	return proto;
}

void destroy_EII_client(struct datalink_proto *dl)
{
	if (dl)
		kfree(dl);
}
