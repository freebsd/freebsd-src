#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <net/checksum.h>

#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <net/profile.h>

#ifdef CONFIG_NET_PROFILE

atomic_t net_profile_active;
struct timeval net_profile_adjust;

NET_PROFILE_DEFINE(total);

struct net_profile_slot *net_profile_chain = &net_prof_total;

#ifdef __alpha__
__u32 alpha_lo;
long alpha_hi;

static void alpha_tick(unsigned long);

static struct timer_list alpha_timer =
	{ NULL, NULL, 0, 0L, alpha_tick };

void alpha_tick(unsigned long dummy)
{
	struct timeval dummy_stamp;
	net_profile_stamp(&dummy_stamp);
	alpha_timer.expires = jiffies + 4*HZ;
	add_timer(&alpha_timer);
}

#endif

void net_profile_irq_adjust(struct timeval *entered, struct timeval* leaved)
{
	struct net_profile_slot *s;

	net_profile_sub(entered, leaved);
	for (s = net_profile_chain; s; s = s->next) {
		if (s->active)
			net_profile_add(leaved, &s->irq);
	}
}


#ifdef CONFIG_PROC_FS
static int profile_read_proc(char *buffer, char **start, off_t offset,
			     int length, int *eof, void *data)
{
	off_t pos=0;
	off_t begin=0;
	int len=0;
	struct net_profile_slot *s;

	len+= sprintf(buffer, "Slot            Hits       Hi         Lo         OnIrqHi    OnIrqLo    Ufl\n");

	if (offset == 0) {
		cli();
		net_prof_total.active = 1;
		atomic_inc(&net_profile_active);
		NET_PROFILE_LEAVE(total);
		sti();
	}
	for (s = net_profile_chain; s; s = s->next) {
		struct net_profile_slot tmp;

		cli();
		tmp = *s;

		/* Wrong, but pretty close to truth */

		s->accumulator.tv_sec = 0;
		s->accumulator.tv_usec = 0;
		s->irq.tv_sec = 0;
		s->irq.tv_usec = 0;
		s->hits = 0;
		s->underflow = 0;
		/* Repair active count, it is possible, only if code has a bug */
		if (s->active) {
			s->active = 0;
			atomic_dec(&net_profile_active);
		}
		sti();

		net_profile_sub(&tmp.irq, &tmp.accumulator);

		len += sprintf(buffer+len,"%-15s %-10d %-10ld %-10lu %-10lu %-10lu %d/%d",
			       tmp.id,
			       tmp.hits,
			       tmp.accumulator.tv_sec,
			       tmp.accumulator.tv_usec,
			       tmp.irq.tv_sec,
			       tmp.irq.tv_usec,
			       tmp.underflow, tmp.active);

			buffer[len++]='\n';
		
			pos=begin+len;
			if(pos<offset) {
				len=0;
				begin=pos;
			}
			if(pos>offset+length)
				goto done;
	}
	*eof = 1;

done:
	*start=buffer+(offset-begin);
	len-=(offset-begin);
	if(len>length)
		len=length;
	if (len < 0)
		len = 0;
	if (offset == 0) {
		cli();
		net_prof_total.active = 0;
		net_prof_total.hits = 0;
		net_profile_stamp(&net_prof_total.entered);
		sti();
	}
	return len;
}
#endif

struct iphdr whitehole_iph;
int whitehole_count;

static int whitehole_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device_stats *stats;

	stats = (struct net_device_stats *)dev->priv;
	stats->tx_packets++;
	stats->tx_bytes+=skb->len;

	dev_kfree_skb(skb);
	return 0;
}

static void whitehole_inject(unsigned long);
int whitehole_init(struct net_device *dev);

static struct timer_list whitehole_timer =
	{ NULL, NULL, 0, 0L, whitehole_inject };

static struct net_device whitehole_dev = {
	"whitehole", 0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 0, NULL, whitehole_init, };

static int whitehole_open(struct net_device *dev)
{
	whitehole_count = 100000;
	whitehole_timer.expires = jiffies + 5*HZ;
	add_timer(&whitehole_timer);
	return 0;
}

static int whitehole_close(struct net_device *dev)
{
	del_timer(&whitehole_timer);
	return 0;
}

static void whitehole_inject(unsigned long dummy)
{
	struct net_device_stats *stats = (struct net_device_stats *)whitehole_dev.priv;
	extern int netdev_dropping;

	do {
		struct iphdr *iph;
		struct sk_buff *skb = alloc_skb(128, GFP_ATOMIC);
		if (!skb)
			break;
		skb_reserve(skb, 32);
		iph = (struct iphdr*)skb_put(skb, sizeof(*iph));
		skb->mac.raw = ((u8*)iph) - 14;
		memcpy(iph, &whitehole_iph, sizeof(*iph));
		skb->protocol = __constant_htons(ETH_P_IP);
		skb->dev = &whitehole_dev;
		skb->pkt_type = PACKET_HOST;
		stats->rx_packets++;
		stats->rx_bytes += skb->len;
		netif_rx(skb);
		whitehole_count--;
	} while (netdev_dropping == 0 && whitehole_count>0);
	if (whitehole_count > 0) {
		whitehole_timer.expires = jiffies + 1;
		add_timer(&whitehole_timer);
	}
}

static struct net_device_stats *whitehole_get_stats(struct net_device *dev)
{
	struct net_device_stats *stats = (struct net_device_stats *) dev->priv;
	return stats;
}

int __init whitehole_init(struct net_device *dev)
{
	dev->priv = kmalloc(sizeof(struct net_device_stats), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOBUFS;
	memset(dev->priv, 0, sizeof(struct net_device_stats));
	dev->get_stats	= whitehole_get_stats;
	dev->hard_start_xmit = whitehole_xmit;
	dev->open = whitehole_open;
	dev->stop = whitehole_close;
	ether_setup(dev);
	dev->tx_queue_len = 0;
	dev->flags |= IFF_NOARP;
	dev->flags &= ~(IFF_BROADCAST|IFF_MULTICAST);
	dev->iflink = 0;
	whitehole_iph.ihl = 5;
	whitehole_iph.version = 4;
	whitehole_iph.ttl = 2;
	whitehole_iph.saddr = in_aton("193.233.7.21");
	whitehole_iph.daddr = in_aton("193.233.7.10");
	whitehole_iph.tot_len = htons(20);
	whitehole_iph.check = ip_compute_csum((void *)&whitehole_iph, 20);
	return 0;
}

int net_profile_register(struct net_profile_slot *slot)
{
	cli();
	slot->next = net_profile_chain;
	net_profile_chain = slot;
	sti();
	return 0;
}

int net_profile_unregister(struct net_profile_slot *slot)
{
	struct net_profile_slot **sp, *s;

	for (sp = &net_profile_chain; (s = *sp) != NULL; sp = &s->next) {
		if (s == slot) {
			cli();
			*sp = s->next;
			sti();
			return 0;
		}
	}
	return -ESRCH;
}


int __init net_profile_init(void)
{
	int i;

#ifdef CONFIG_PROC_FS
	create_proc_read_entry("net/profile", 0, 0, profile_read_proc, NULL);
#endif

	register_netdevice(&whitehole_dev);

	printk("Evaluating net profiler cost ...");
#ifdef __alpha__
	alpha_tick(0);
#endif
	for (i=0; i<1024; i++) {
		NET_PROFILE_ENTER(total);
		NET_PROFILE_LEAVE(total);
	}
	if (net_prof_total.accumulator.tv_sec) {
		printk(" too high!\n");
	} else {
		net_profile_adjust.tv_usec = net_prof_total.accumulator.tv_usec>>10;
		printk("%ld units\n", net_profile_adjust.tv_usec);
	}
	net_prof_total.hits = 0;
	net_profile_stamp(&net_prof_total.entered);
	return 0;
}

#endif
