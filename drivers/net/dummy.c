/* dummy.c: a dummy net driver

	The purpose of this driver is to provide a device to point a
	route through, but not to actually transmit packets.

	Why?  If you have a machine whose only connection is an occasional
	PPP/SLIP/PLIP link, you can only connect to your own hostname
	when the link is up.  Otherwise you have to use localhost.
	This isn't very consistent.

	One solution is to set up a dummy link using PPP/SLIP/PLIP,
	but this seems (to me) too much overhead for too little gain.
	This driver provides a small alternative. Thus you can do
	
	[when not running slip]
		ifconfig dummy slip.addr.ess.here up
	[to go to slip]
		ifconfig dummy down
		dip whatever

	This was written by looking at Donald Becker's skeleton driver
	and the loopback driver.  I then threw away anything that didn't
	apply!	Thanks to Alan Cox for the key clue on what to do with
	misguided packets.

			Nick Holloway, 27th May 1994
	[I tweaked this explanation a little but that's all]
			Alan Cox, 30th May 1994
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/init.h>

static int dummy_xmit(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats *dummy_get_stats(struct net_device *dev);

/* fake multicast ability */
static void set_multicast_list(struct net_device *dev)
{
}

#ifdef CONFIG_NET_FASTROUTE
static int dummy_accept_fastpath(struct net_device *dev, struct dst_entry *dst)
{
	return -1;
}
#endif

static int __init dummy_init(struct net_device *dev)
{
	/* Initialize the device structure. */

	dev->priv = kmalloc(sizeof(struct net_device_stats), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOMEM;
	memset(dev->priv, 0, sizeof(struct net_device_stats));

	dev->get_stats = dummy_get_stats;
	dev->hard_start_xmit = dummy_xmit;
	dev->set_multicast_list = set_multicast_list;
#ifdef CONFIG_NET_FASTROUTE
	dev->accept_fastpath = dummy_accept_fastpath;
#endif

	/* Fill in device structure with ethernet-generic values. */
	ether_setup(dev);
	dev->tx_queue_len = 0;
	dev->flags |= IFF_NOARP;
	dev->flags &= ~IFF_MULTICAST;

	return 0;
}

static int dummy_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device_stats *stats = dev->priv;

	stats->tx_packets++;
	stats->tx_bytes+=skb->len;

	dev_kfree_skb(skb);
	return 0;
}

static struct net_device_stats *dummy_get_stats(struct net_device *dev)
{
	return dev->priv;
}

static struct net_device dev_dummy;

static int __init dummy_init_module(void)
{
	int err;

	dev_dummy.init = dummy_init;
	SET_MODULE_OWNER(&dev_dummy);

	/* Find a name for this unit */
	err=dev_alloc_name(&dev_dummy,"dummy%d");
	if(err<0)
		return err;
	err = register_netdev(&dev_dummy);
	if (err<0)
		return err;
	return 0;
}

static void __exit dummy_cleanup_module(void)
{
	unregister_netdev(&dev_dummy);
	kfree(dev_dummy.priv);

	memset(&dev_dummy, 0, sizeof(dev_dummy));
	dev_dummy.init = dummy_init;
}

module_init(dummy_init_module);
module_exit(dummy_cleanup_module);
MODULE_LICENSE("GPL");
