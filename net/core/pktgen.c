/* -*-linux-c-*-
 * $Id: pktgen.c,v 1.8 2002/07/15 19:30:17 robert Exp $
 * pktgen.c: Packet Generator for performance evaluation.
 *
 * Copyright 2001, 2002 by Robert Olsson <robert.olsson@its.uu.se>
 *                                 Uppsala University, Sweden
 *
 * A tool for loading the network with preconfigurated packets.
 * The tool is implemented as a linux module.  Parameters are output 
 * device, IPG (interpacket gap), number of packets, and whether
 * to use multiple SKBs or just the same one.
 * pktgen uses the installed interface's output routine.
 *
 * Additional hacking by:
 *
 * Jens.Laas@data.slu.se
 * Improved by ANK. 010120.
 * Improved by ANK even more. 010212.
 * MAC address typo fixed. 010417 --ro
 * Integrated.  020301 --DaveM
 * Added multiskb option 020301 --DaveM
 * Scaling of results. 020417--sigurdur@linpro.no
 * Significant re-work of the module:
 *   *  Updated to support generation over multiple interfaces at once
 *       by creating 32 /proc/net/pg* files.  Each file can be manipulated
 *       individually.
 *   *  Converted many counters to __u64 to allow longer runs.
 *   *  Allow configuration of ranges, like min/max IP address, MACs,
 *       and UDP-ports, for both source and destination, and can
 *       set to use a random distribution or sequentially walk the range.
 *   *  Can now change some values after starting.
 *   *  Place 12-byte packet in UDP payload with magic number,
 *       sequence number, and timestamp.  Will write receiver next.
 *   *  The new changes seem to have a performance impact of around 1%,
 *       as far as I can tell.
 *   --Ben Greear <greearb@candelatech.com>
 *
 * Renamed multiskb to clone_skb and cleaned up sending core for two distinct 
 * skb modes. A clone_skb=0 mode for Ben "ranges" work and a clone_skb != 0 
 * as a "fastpath" with a configurable number of clones after alloc's.
 *
 * clone_skb=0 means all packets are allocated this also means ranges time 
 * stamps etc can be used. clone_skb=100 means 1 malloc is followed by 100 
 * clones.
 *
 * Also moved to /proc/net/pktgen/ 
 * --ro 
 *
 * Fix refcount off by one if first packet fails, potential null deref, 
 * memleak 030710- KJP
 *
 * See Documentation/networking/pktgen.txt for how to use this.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/inet.h>
#include <asm/byteorder.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/uaccess.h>

#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <net/checksum.h>
#include <asm/timex.h>

#define cycles()	((u32)get_cycles())


#define VERSION "pktgen version 1.3"
static char version[] __initdata = 
  "pktgen.c: v1.3: Packet Generator for packet performance testing.\n";

/* Used to help with determining the pkts on receive */

#define PKTGEN_MAGIC 0xbe9be955


/* Keep information per interface */
struct pktgen_info {
        /* Parameters */

        /* If min != max, then we will either do a linear iteration, or
         * we will do a random selection from within the range.
         */
        __u32 flags;     

#define F_IPSRC_RND   (1<<0)  /* IP-Src Random  */
#define F_IPDST_RND   (1<<1)  /* IP-Dst Random  */
#define F_UDPSRC_RND  (1<<2)  /* UDP-Src Random */
#define F_UDPDST_RND  (1<<3)  /* UDP-Dst Random */
#define F_MACSRC_RND  (1<<4)  /* MAC-Src Random */
#define F_MACDST_RND  (1<<5)  /* MAC-Dst Random */
#define F_SET_SRCMAC  (1<<6)  /* Specify-Src-Mac 
				 (default is to use Interface's MAC Addr) */
#define F_SET_SRCIP   (1<<7)  /*  Specify-Src-IP
				  (default is to use Interface's IP Addr) */ 

        
        int pkt_size;    /* = ETH_ZLEN; */
        int nfrags;
        __u32 ipg;       /* Default Interpacket gap in nsec */
        __u64 count;     /* Default No packets to send */
        __u64 sofar;     /* How many pkts we've sent so far */
        __u64 errors;    /* Errors when trying to transmit, pkts will be re-sent */
        struct timeval started_at;
        struct timeval stopped_at;
        __u64 idle_acc;
        __u32 seq_num;
        
        int clone_skb;   /* Use multiple SKBs during packet gen.  If this number
                          * is greater than 1, then that many coppies of the same
                          * packet will be sent before a new packet is allocated.
                          * For instance, if you want to send 1024 identical packets
                          * before creating a new packet, set clone_skb to 1024.
                          */
        int busy;
        int do_run_run;   /* if this changes to false, the test will stop */
        
        char outdev[32];
        char dst_min[32];
        char dst_max[32];
        char src_min[32];
        char src_max[32];

        /* If we're doing ranges, random or incremental, then this
         * defines the min/max for those ranges.
         */
        __u32 saddr_min; /* inclusive, source IP address */
        __u32 saddr_max; /* exclusive, source IP address */
        __u32 daddr_min; /* inclusive, dest IP address */
        __u32 daddr_max; /* exclusive, dest IP address */

        __u16 udp_src_min; /* inclusive, source UDP port */
        __u16 udp_src_max; /* exclusive, source UDP port */
        __u16 udp_dst_min; /* inclusive, dest UDP port */
        __u16 udp_dst_max; /* exclusive, dest UDP port */

        __u32 src_mac_count; /* How many MACs to iterate through */
        __u32 dst_mac_count; /* How many MACs to iterate through */
        
        unsigned char dst_mac[6];
        unsigned char src_mac[6];
        
        __u32 cur_dst_mac_offset;
        __u32 cur_src_mac_offset;
        __u32 cur_saddr;
        __u32 cur_daddr;
        __u16 cur_udp_dst;
        __u16 cur_udp_src;
        
        __u8 hh[14];
        /* = { 
           0x00, 0x80, 0xC8, 0x79, 0xB3, 0xCB, 
           
           We fill in SRC address later
           0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x08, 0x00
           };
        */
        __u16 pad; /* pad out the hh struct to an even 16 bytes */
        char result[512];

        /* proc file names */
        char fname[80];
        char busy_fname[80];
        
        struct proc_dir_entry *proc_ent;
        struct proc_dir_entry *busy_proc_ent;
};

struct pktgen_hdr {
        __u32 pgh_magic;
        __u32 seq_num;
        struct timeval timestamp;
};

static int cpu_speed;
static int debug;

/* Module parameters, defaults. */
static int count_d = 100000;
static int ipg_d = 0;
static int clone_skb_d = 0;


#define MAX_PKTGEN 8
static struct pktgen_info pginfos[MAX_PKTGEN];


/** Convert to miliseconds */
inline __u64 tv_to_ms(const struct timeval* tv) {
        __u64 ms = tv->tv_usec / 1000;
        ms += (__u64)tv->tv_sec * (__u64)1000;
        return ms;
}

inline __u64 getCurMs(void) {
        struct timeval tv;
        do_gettimeofday(&tv);
        return tv_to_ms(&tv);
}

#define PG_PROC_DIR "pktgen"
static struct proc_dir_entry *proc_dir = 0;

static struct net_device *setup_inject(struct pktgen_info* info)
{
	struct net_device *odev;

	rtnl_lock();
	odev = __dev_get_by_name(info->outdev);
	if (!odev) {
		sprintf(info->result, "No such netdevice: \"%s\"", info->outdev);
		goto out_unlock;
	}

	if (odev->type != ARPHRD_ETHER) {
		sprintf(info->result, "Not ethernet device: \"%s\"", info->outdev);
		goto out_unlock;
	}

	if (!netif_running(odev)) {
		sprintf(info->result, "Device is down: \"%s\"", info->outdev);
		goto out_unlock;
	}

        /* Default to the interface's mac if not explicitly set. */
        if (!(info->flags & F_SET_SRCMAC)) {
                memcpy(&(info->hh[6]), odev->dev_addr, 6);
        }
        else {
                memcpy(&(info->hh[6]), info->src_mac, 6);
        }

        /* Set up Dest MAC */
        memcpy(&(info->hh[0]), info->dst_mac, 6);
        
	info->saddr_min = 0;
	info->saddr_max = 0;
        if (strlen(info->src_min) == 0) {
                if (odev->ip_ptr) {
                        struct in_device *in_dev = odev->ip_ptr;

                        if (in_dev->ifa_list) {
                                info->saddr_min = in_dev->ifa_list->ifa_address;
                                info->saddr_max = info->saddr_min;
                        }
                }
	}
        else {
                info->saddr_min = in_aton(info->src_min);
                info->saddr_max = in_aton(info->src_max);
        }

        info->daddr_min = in_aton(info->dst_min);
        info->daddr_max = in_aton(info->dst_max);

        /* Initialize current values. */
        info->cur_dst_mac_offset = 0;
        info->cur_src_mac_offset = 0;
        info->cur_saddr = info->saddr_min;
        info->cur_daddr = info->daddr_min;
        info->cur_udp_dst = info->udp_dst_min;
        info->cur_udp_src = info->udp_src_min;
        
	atomic_inc(&odev->refcnt);
	rtnl_unlock();

	return odev;

out_unlock:
	rtnl_unlock();
	return NULL;
}

static void nanospin(int ipg, struct pktgen_info* info)
{
	u32 idle_start, idle;

	idle_start = cycles();

	for (;;) {
		barrier();
		idle = cycles() - idle_start;
		if (idle * 1000 >= ipg * cpu_speed)
			break;
	}
	info->idle_acc += idle;
}

static int calc_mhz(void)
{
	struct timeval start, stop;
	u32 start_s, elapsed;

	do_gettimeofday(&start);
	start_s = cycles();
	do {
		barrier();
		elapsed = cycles() - start_s;
		if (elapsed == 0)
			return 0;
	} while (elapsed < 1000 * 50000);
	do_gettimeofday(&stop);
	return elapsed/(stop.tv_usec-start.tv_usec+1000000*(stop.tv_sec-start.tv_sec));
}

static void cycles_calibrate(void)
{
	int i;

	for (i = 0; i < 3; i++) {
		int res = calc_mhz();
		if (res > cpu_speed)
			cpu_speed = res;
	}
}


/* Increment/randomize headers according to flags and current values
 * for IP src/dest, UDP src/dst port, MAC-Addr src/dst
 */
static void mod_cur_headers(struct pktgen_info* info) {        
        __u32 imn;
        __u32 imx;
        
	/*  Deal with source MAC */
        if (info->src_mac_count > 1) {
                __u32 mc;
                __u32 tmp;
                if (info->flags & F_MACSRC_RND) {
                        mc = net_random() % (info->src_mac_count);
                }
                else {
                        mc = info->cur_src_mac_offset++;
                        if (info->cur_src_mac_offset > info->src_mac_count) {
                                info->cur_src_mac_offset = 0;
                        }
                }

                tmp = info->src_mac[5] + (mc & 0xFF);
                info->hh[11] = tmp;
                tmp = (info->src_mac[4] + ((mc >> 8) & 0xFF) + (tmp >> 8));
                info->hh[10] = tmp;
                tmp = (info->src_mac[3] + ((mc >> 16) & 0xFF) + (tmp >> 8));
                info->hh[9] = tmp;
                tmp = (info->src_mac[2] + ((mc >> 24) & 0xFF) + (tmp >> 8));
                info->hh[8] = tmp;
                tmp = (info->src_mac[1] + (tmp >> 8));
                info->hh[7] = tmp;        
        }

        /*  Deal with Destination MAC */
        if (info->dst_mac_count > 1) {
                __u32 mc;
                __u32 tmp;
                if (info->flags & F_MACDST_RND) {
                        mc = net_random() % (info->dst_mac_count);
                }
                else {
                        mc = info->cur_dst_mac_offset++;
                        if (info->cur_dst_mac_offset > info->dst_mac_count) {
                                info->cur_dst_mac_offset = 0;
                        }
                }

                tmp = info->dst_mac[5] + (mc & 0xFF);
                info->hh[5] = tmp;
                tmp = (info->dst_mac[4] + ((mc >> 8) & 0xFF) + (tmp >> 8));
                info->hh[4] = tmp;
                tmp = (info->dst_mac[3] + ((mc >> 16) & 0xFF) + (tmp >> 8));
                info->hh[3] = tmp;
                tmp = (info->dst_mac[2] + ((mc >> 24) & 0xFF) + (tmp >> 8));
                info->hh[2] = tmp;
                tmp = (info->dst_mac[1] + (tmp >> 8));
                info->hh[1] = tmp;        
        }

        if (info->udp_src_min < info->udp_src_max) {
                if (info->flags & F_UDPSRC_RND) {
                        info->cur_udp_src = ((net_random() % (info->udp_src_max - info->udp_src_min))
                                             + info->udp_src_min);
                }
                else {
                     info->cur_udp_src++;
                     if (info->cur_udp_src >= info->udp_src_max) {
                             info->cur_udp_src = info->udp_src_min;
                     }
                }
        }

        if (info->udp_dst_min < info->udp_dst_max) {
                if (info->flags & F_UDPDST_RND) {
                        info->cur_udp_dst = ((net_random() % (info->udp_dst_max - info->udp_dst_min))
                                             + info->udp_dst_min);
                }
                else {
                     info->cur_udp_dst++;
                     if (info->cur_udp_dst >= info->udp_dst_max) {
                             info->cur_udp_dst = info->udp_dst_min;
                     }
                }
        }

        if ((imn = ntohl(info->saddr_min)) < (imx = ntohl(info->saddr_max))) {
                __u32 t;
                if (info->flags & F_IPSRC_RND) {
                        t = ((net_random() % (imx - imn)) + imn);
                }
                else {
                     t = ntohl(info->cur_saddr);
                     t++;
                     if (t >= imx) {
                             t = imn;
                     }
                }
                info->cur_saddr = htonl(t);
        }

        if ((imn = ntohl(info->daddr_min)) < (imx = ntohl(info->daddr_max))) {
                __u32 t;
                if (info->flags & F_IPDST_RND) {
                        t = ((net_random() % (imx - imn)) + imn);
                }
                else {
                     t = ntohl(info->cur_daddr);
                     t++;
                     if (t >= imx) {
                             t = imn;
                     }
                }
                info->cur_daddr = htonl(t);
        }
}/* mod_cur_headers */


static struct sk_buff *fill_packet(struct net_device *odev, struct pktgen_info* info)
{
	struct sk_buff *skb = NULL;
	__u8 *eth;
	struct udphdr *udph;
	int datalen, iplen;
	struct iphdr *iph;
        struct pktgen_hdr *pgh = NULL;
        
	skb = alloc_skb(info->pkt_size + 64 + 16, GFP_ATOMIC);
	if (!skb) {
		sprintf(info->result, "No memory");
		return NULL;
	}

	skb_reserve(skb, 16);

	/*  Reserve for ethernet and IP header  */
	eth = (__u8 *) skb_push(skb, 14);
	iph = (struct iphdr *)skb_put(skb, sizeof(struct iphdr));
	udph = (struct udphdr *)skb_put(skb, sizeof(struct udphdr));

        /* Update any of the values, used when we're incrementing various
         * fields.
         */
        mod_cur_headers(info);

	memcpy(eth, info->hh, 14);
        
	datalen = info->pkt_size - 14 - 20 - 8; /* Eth + IPh + UDPh */
	if (datalen < sizeof(struct pktgen_hdr)) {
		datalen = sizeof(struct pktgen_hdr);
        }
        
	udph->source = htons(info->cur_udp_src);
	udph->dest = htons(info->cur_udp_dst);
	udph->len = htons(datalen + 8); /* DATA + udphdr */
	udph->check = 0;  /* No checksum */

	iph->ihl = 5;
	iph->version = 4;
	iph->ttl = 3;
	iph->tos = 0;
	iph->protocol = IPPROTO_UDP; /* UDP */
	iph->saddr = info->cur_saddr;
	iph->daddr = info->cur_daddr;
	iph->frag_off = 0;
	iplen = 20 + 8 + datalen;
	iph->tot_len = htons(iplen);
	iph->check = 0;
	iph->check = ip_fast_csum((void *) iph, iph->ihl);
	skb->protocol = __constant_htons(ETH_P_IP);
	skb->mac.raw = ((u8 *)iph) - 14;
	skb->dev = odev;
	skb->pkt_type = PACKET_HOST;

	if (info->nfrags <= 0) {
                pgh = (struct pktgen_hdr *)skb_put(skb, datalen);
	} else {
		int frags = info->nfrags;
		int i;

                /* TODO: Verify this is OK...it sure is ugly. --Ben */
                pgh = (struct pktgen_hdr*)(((char*)(udph)) + 8);
                
		if (frags > MAX_SKB_FRAGS)
			frags = MAX_SKB_FRAGS;
		if (datalen > frags*PAGE_SIZE) {
			skb_put(skb, datalen-frags*PAGE_SIZE);
			datalen = frags*PAGE_SIZE;
		}

		i = 0;
		while (datalen > 0) {
			struct page *page = alloc_pages(GFP_KERNEL, 0);
			skb_shinfo(skb)->frags[i].page = page;
			skb_shinfo(skb)->frags[i].page_offset = 0;
			skb_shinfo(skb)->frags[i].size =
				(datalen < PAGE_SIZE ? datalen : PAGE_SIZE);
			datalen -= skb_shinfo(skb)->frags[i].size;
			skb->len += skb_shinfo(skb)->frags[i].size;
			skb->data_len += skb_shinfo(skb)->frags[i].size;
			i++;
			skb_shinfo(skb)->nr_frags = i;
		}

		while (i < frags) {
			int rem;

			if (i == 0)
				break;

			rem = skb_shinfo(skb)->frags[i - 1].size / 2;
			if (rem == 0)
				break;

			skb_shinfo(skb)->frags[i - 1].size -= rem;

			skb_shinfo(skb)->frags[i] = skb_shinfo(skb)->frags[i - 1];
			get_page(skb_shinfo(skb)->frags[i].page);
			skb_shinfo(skb)->frags[i].page = skb_shinfo(skb)->frags[i - 1].page;
			skb_shinfo(skb)->frags[i].page_offset += skb_shinfo(skb)->frags[i - 1].size;
			skb_shinfo(skb)->frags[i].size = rem;
			i++;
			skb_shinfo(skb)->nr_frags = i;
		}
	}

        /* Stamp the time, and sequence number, convert them to network byte order */
        if (pgh) {
                pgh->pgh_magic = htonl(PKTGEN_MAGIC);
                do_gettimeofday(&(pgh->timestamp));
                pgh->timestamp.tv_usec = htonl(pgh->timestamp.tv_usec);
                pgh->timestamp.tv_sec = htonl(pgh->timestamp.tv_sec);
                pgh->seq_num = htonl(info->seq_num);
        }
        
	return skb;
}


static void inject(struct pktgen_info* info)
{
	struct net_device *odev = NULL;
	struct sk_buff *skb = NULL;
	__u64 total = 0;
        __u64 idle = 0;
	__u64 lcount = 0;
        int nr_frags = 0;
	int last_ok = 1;           /* Was last skb sent? 
	                            * Or a failed transmit of some sort?  This will keep
                                    * sequence numbers in order, for example.
                                    */
        __u64 fp = 0;
        __u32 fp_tmp = 0;

	odev = setup_inject(info);
	if (!odev)
		return;

        info->do_run_run = 1; /* Cranke yeself! */
	info->idle_acc = 0;
	info->sofar = 0;
	lcount = info->count;


        /* Build our initial pkt and place it as a re-try pkt. */
	skb = fill_packet(odev, info);
	if (skb == NULL) goto out_reldev;

	do_gettimeofday(&(info->started_at));

	while(info->do_run_run) {

                /* Set a time-stamp, so build a new pkt each time */

                if (last_ok) {
                        if (++fp_tmp >= info->clone_skb ) {
                                kfree_skb(skb);
                                skb = fill_packet(odev, info);
                                if (skb == NULL) {
					goto out_reldev;
                                }
                                fp++;
                                fp_tmp = 0; /* reset counter */
                        }
                }

                nr_frags = skb_shinfo(skb)->nr_frags;
                   
		spin_lock_bh(&odev->xmit_lock);
		if (!netif_queue_stopped(odev)) {

			atomic_inc(&skb->users);

			if (odev->hard_start_xmit(skb, odev)) {

				atomic_dec(&skb->users);
				if (net_ratelimit()) {
                                   printk(KERN_INFO "Hard xmit error\n");
                                }
                                info->errors++;
				last_ok = 0;
			}
                        else {
		           last_ok = 1;	
                           info->sofar++;
                           info->seq_num++;
                        }
		}
		else {
                        /* Re-try it next time */
			last_ok = 0;
                }
                

		spin_unlock_bh(&odev->xmit_lock);

		if (info->ipg) {
                        /* Try not to busy-spin if we have larger sleep times.
                         * TODO:  Investigate better ways to do this.
                         */
                        if (info->ipg < 10000) { /* 10 usecs or less */
                                nanospin(info->ipg, info);
                        }
                        else if (info->ipg < 10000000) { /* 10ms or less */
                                udelay(info->ipg / 1000);
                        }
                        else {
                                mdelay(info->ipg / 1000000);
                        }
                }
                
		if (signal_pending(current)) {
                        break;
                }

                /* If lcount is zero, then run forever */
		if ((lcount != 0) && (--lcount == 0)) {
			if (atomic_read(&skb->users) != 1) {
				u32 idle_start, idle;

				idle_start = cycles();
				while (atomic_read(&skb->users) != 1) {
					if (signal_pending(current)) {
                                                break;
                                        }
					schedule();
				}
				idle = cycles() - idle_start;
				info->idle_acc += idle;
			}
			break;
		}

		if (netif_queue_stopped(odev) || current->need_resched) {
			u32 idle_start, idle;

			idle_start = cycles();
			do {
				if (signal_pending(current)) {
                                        info->do_run_run = 0;
                                        break;
                                }
				if (!netif_running(odev)) {
                                        info->do_run_run = 0;
					break;
                                }
				if (current->need_resched)
					schedule();
				else
					do_softirq();
			} while (netif_queue_stopped(odev));
			idle = cycles() - idle_start;
			info->idle_acc += idle;
		}
	}/* while we should be running */

	do_gettimeofday(&(info->stopped_at));

	total = (info->stopped_at.tv_sec - info->started_at.tv_sec) * 1000000 +
		info->stopped_at.tv_usec - info->started_at.tv_usec;

	idle = (__u32)(info->idle_acc)/(__u32)(cpu_speed);

        {
		char *p = info->result;
                __u64 pps = (__u32)(info->sofar * 1000) / ((__u32)(total) / 1000);
                __u64 bps = pps * 8 * (info->pkt_size + 4); /* take 32bit ethernet CRC into account */
		p += sprintf(p, "OK: %llu(c%llu+d%llu) usec, %llu (%dbyte,%dfrags) %llupps %lluMb/sec (%llubps)  errors: %llu",
			     (unsigned long long) total,
			     (unsigned long long) (total - idle),
			     (unsigned long long) idle,
			     (unsigned long long) info->sofar,
                             skb->len + 4, /* Add 4 to account for the ethernet checksum */
                             nr_frags,
			     (unsigned long long) pps,
			     (unsigned long long) (bps / (u64) 1024 / (u64) 1024),
			     (unsigned long long) bps,
			     (unsigned long long) info->errors
			     );
	}

	kfree_skb(skb);

out_reldev:
        if (odev) {
                dev_put(odev);
                odev = NULL;
        }

	return;

}

/* proc/net/pktgen/pg */

static int proc_busy_read(char *buf , char **start, off_t offset,
			     int len, int *eof, void *data)
{
	char *p;
        int idx = (int)(long)(data);
        struct pktgen_info* info = NULL;
        
        if ((idx < 0) || (idx >= MAX_PKTGEN)) {
                printk("ERROR: idx: %i is out of range in proc_write\n", idx);
                return -EINVAL;
        }
        info = &(pginfos[idx]);
  
	p = buf;
	p += sprintf(p, "%d\n", info->busy);
	*eof = 1;
  
	return p-buf;
}

static int proc_read(char *buf , char **start, off_t offset,
			int len, int *eof, void *data)
{
	char *p;
	int i;
        int idx = (int)(long)(data);
        struct pktgen_info* info = NULL;
        __u64 sa;
        __u64 stopped;
        __u64 now = getCurMs();
        
        if ((idx < 0) || (idx >= MAX_PKTGEN)) {
                printk("ERROR: idx: %i is out of range in proc_write\n", idx);
                return -EINVAL;
        }
        info = &(pginfos[idx]);
  
	p = buf;
        p += sprintf(p, "%s\n", VERSION); /* Help with parsing compatibility */
	p += sprintf(p, "Params: count %llu  pkt_size: %u  frags: %d  ipg: %u  clone_skb: %d odev \"%s\"\n",
		     (unsigned long long) info->count,
		     info->pkt_size, info->nfrags, info->ipg,
                     info->clone_skb, info->outdev);
        p += sprintf(p, "     dst_min: %s  dst_max: %s  src_min: %s  src_max: %s\n",
                     info->dst_min, info->dst_max, info->src_min, info->src_max);
        p += sprintf(p, "     src_mac: ");
	for (i = 0; i < 6; i++) {
		p += sprintf(p, "%02X%s", info->src_mac[i], i == 5 ? "  " : ":");
        }
        p += sprintf(p, "dst_mac: ");
	for (i = 0; i < 6; i++) {
		p += sprintf(p, "%02X%s", info->dst_mac[i], i == 5 ? "\n" : ":");
        }
        p += sprintf(p, "     udp_src_min: %d  udp_src_max: %d  udp_dst_min: %d  udp_dst_max: %d\n",
                     info->udp_src_min, info->udp_src_max, info->udp_dst_min,
                     info->udp_dst_max);
        p += sprintf(p, "     src_mac_count: %d  dst_mac_count: %d\n     Flags: ",
                     info->src_mac_count, info->dst_mac_count);
        if (info->flags &  F_IPSRC_RND) {
                p += sprintf(p, "IPSRC_RND  ");
        }
        if (info->flags & F_IPDST_RND) {
                p += sprintf(p, "IPDST_RND  ");
        }
        if (info->flags & F_UDPSRC_RND) {
                p += sprintf(p, "UDPSRC_RND  ");
        }
        if (info->flags & F_UDPDST_RND) {
                p += sprintf(p, "UDPDST_RND  ");
        }
        if (info->flags & F_MACSRC_RND) {
                p += sprintf(p, "MACSRC_RND  ");
        }
        if (info->flags & F_MACDST_RND) {
                p += sprintf(p, "MACDST_RND  ");
        }
        p += sprintf(p, "\n");
        
        sa = tv_to_ms(&(info->started_at));
        stopped = tv_to_ms(&(info->stopped_at));
        if (info->do_run_run) {
                stopped = now; /* not really stopped, more like last-running-at */
        }
        p += sprintf(p, "Current:\n     pkts-sofar: %llu  errors: %llu\n     started: %llums  stopped: %llums  now: %llums  idle: %lluns\n",
                     (unsigned long long) info->sofar,
		     (unsigned long long) info->errors,
		     (unsigned long long) sa,
		     (unsigned long long) stopped,
		     (unsigned long long) now,
		     (unsigned long long) info->idle_acc);
        p += sprintf(p, "     seq_num: %d  cur_dst_mac_offset: %d  cur_src_mac_offset: %d\n",
                     info->seq_num, info->cur_dst_mac_offset, info->cur_src_mac_offset);
        p += sprintf(p, "     cur_saddr: 0x%x  cur_daddr: 0x%x  cur_udp_dst: %d  cur_udp_src: %d\n",
                     info->cur_saddr, info->cur_daddr, info->cur_udp_dst, info->cur_udp_src);
        
	if (info->result[0])
		p += sprintf(p, "Result: %s\n", info->result);
	else
		p += sprintf(p, "Result: Idle\n");
	*eof = 1;

	return p - buf;
}

static int count_trail_chars(const char *user_buffer, unsigned int maxlen)
{
	int i;

	for (i = 0; i < maxlen; i++) {
		char c;

		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		switch (c) {
		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
		case '=':
			break;
		default:
			goto done;
		};
	}
done:
	return i;
}

static unsigned long num_arg(const char *user_buffer, unsigned long maxlen,
			     unsigned long *num)
{
	int i = 0;

	*num = 0;
  
	for(; i < maxlen; i++) {
		char c;

		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		if ((c >= '0') && (c <= '9')) {
			*num *= 10;
			*num += c -'0';
		} else
			break;
	}
	return i;
}

static int strn_len(const char *user_buffer, unsigned int maxlen)
{
	int i = 0;

	for(; i < maxlen; i++) {
		char c;

		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		switch (c) {
		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
			goto done_str;
		default:
			break;
		};
	}
done_str:
	return i;
}

static int proc_write(struct file *file, const char *user_buffer,
			 unsigned long count, void *data)
{
	int i = 0, max, len;
	char name[16], valstr[32];
	unsigned long value = 0;
        int idx = (int)(long)(data);
        struct pktgen_info* info = NULL;
        char* result = NULL;
	int tmp;
        
        if ((idx < 0) || (idx >= MAX_PKTGEN)) {
                printk("ERROR: idx: %i is out of range in proc_write\n", idx);
                return -EINVAL;
        }
        info = &(pginfos[idx]);
        result = &(info->result[0]);
        
	if (count < 1) {
		sprintf(result, "Wrong command format");
		return -EINVAL;
	}
  
	max = count - i;
	tmp = count_trail_chars(&user_buffer[i], max);
	if (tmp < 0)
		return tmp;
	i += tmp;
  
	/* Read variable name */

	len = strn_len(&user_buffer[i], sizeof(name) - 1);
	if (len < 0)
		return len;
	memset(name, 0, sizeof(name));
	if (copy_from_user(name, &user_buffer[i], len))
		return -EFAULT;
	i += len;
  
	max = count -i;
	len = count_trail_chars(&user_buffer[i], max);
	if (len < 0)
		return len;
	i += len;

	if (debug)
		printk("pg: %s,%lu\n", name, count);

	if (!strcmp(name, "stop")) {
		if (info->do_run_run) {
			strcpy(result, "Stopping");
                }
                else {
                        strcpy(result, "Already stopped...\n");
                }
                info->do_run_run = 0;
		return count;
	}

	if (!strcmp(name, "pkt_size")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
		if (value < 14+20+8)
			value = 14+20+8;
		info->pkt_size = value;
		sprintf(result, "OK: pkt_size=%u", info->pkt_size);
		return count;
	}
	if (!strcmp(name, "frags")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
		info->nfrags = value;
		sprintf(result, "OK: frags=%u", info->nfrags);
		return count;
	}
	if (!strcmp(name, "ipg")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
		info->ipg = value;
		sprintf(result, "OK: ipg=%u", info->ipg);
		return count;
	}
 	if (!strcmp(name, "udp_src_min")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
	 	info->udp_src_min = value;
		sprintf(result, "OK: udp_src_min=%u", info->udp_src_min);
		return count;
	}
 	if (!strcmp(name, "udp_dst_min")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
	 	info->udp_dst_min = value;
		sprintf(result, "OK: udp_dst_min=%u", info->udp_dst_min);
		return count;
	}
 	if (!strcmp(name, "udp_src_max")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
	 	info->udp_src_max = value;
		sprintf(result, "OK: udp_src_max=%u", info->udp_src_max);
		return count;
	}
 	if (!strcmp(name, "udp_dst_max")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
	 	info->udp_dst_max = value;
		sprintf(result, "OK: udp_dst_max=%u", info->udp_dst_max);
		return count;
	}
	if (!strcmp(name, "clone_skb")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
                info->clone_skb = value;
	
		sprintf(result, "OK: clone_skb=%d", info->clone_skb);
		return count;
	}
	if (!strcmp(name, "count")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
		info->count = value;
		sprintf(result, "OK: count=%llu", (unsigned long long) info->count);
		return count;
	}
	if (!strcmp(name, "src_mac_count")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
		info->src_mac_count = value;
		sprintf(result, "OK: src_mac_count=%d", info->src_mac_count);
		return count;
	}
	if (!strcmp(name, "dst_mac_count")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;
		i += len;
		info->dst_mac_count = value;
		sprintf(result, "OK: dst_mac_count=%d", info->dst_mac_count);
		return count;
	}
	if (!strcmp(name, "odev")) {
		len = strn_len(&user_buffer[i], sizeof(info->outdev) - 1);
		if (len < 0)
			return len;
		memset(info->outdev, 0, sizeof(info->outdev));
		if (copy_from_user(info->outdev, &user_buffer[i], len))
			return -EFAULT;
		i += len;
		sprintf(result, "OK: odev=%s", info->outdev);
		return count;
	}
	if (!strcmp(name, "flag")) {
                char f[32];
		len = strn_len(&user_buffer[i], sizeof(f) - 1);
		if (len < 0)
			return len;
                memset(f, 0, 32);
		if (copy_from_user(f, &user_buffer[i], len))
			return -EFAULT;
		i += len;
                if (strcmp(f, "IPSRC_RND") == 0) {
                        info->flags |= F_IPSRC_RND;
                }
                else if (strcmp(f, "!IPSRC_RND") == 0) {
                        info->flags &= ~F_IPSRC_RND;
                }
                else if (strcmp(f, "IPDST_RND") == 0) {
                        info->flags |= F_IPDST_RND;
                }
                else if (strcmp(f, "!IPDST_RND") == 0) {
                        info->flags &= ~F_IPDST_RND;
                }
                else if (strcmp(f, "UDPSRC_RND") == 0) {
                        info->flags |= F_UDPSRC_RND;
                }
                else if (strcmp(f, "!UDPSRC_RND") == 0) {
                        info->flags &= ~F_UDPSRC_RND;
                }
                else if (strcmp(f, "UDPDST_RND") == 0) {
                        info->flags |= F_UDPDST_RND;
                }
                else if (strcmp(f, "!UDPDST_RND") == 0) {
                        info->flags &= ~F_UDPDST_RND;
                }
                else if (strcmp(f, "MACSRC_RND") == 0) {
                        info->flags |= F_MACSRC_RND;
                }
                else if (strcmp(f, "!MACSRC_RND") == 0) {
                        info->flags &= ~F_MACSRC_RND;
                }
                else if (strcmp(f, "MACDST_RND") == 0) {
                        info->flags |= F_MACDST_RND;
                }
                else if (strcmp(f, "!MACDST_RND") == 0) {
                        info->flags &= ~F_MACDST_RND;
                }
                else {
                        sprintf(result, "Flag -:%s:- unknown\nAvailable flags, (prepend ! to un-set flag):\n%s",
                                f,
                                "IPSRC_RND, IPDST_RND, UDPSRC_RND, UDPDST_RND, MACSRC_RND, MACDST_RND\n");
                        return count;
                }
		sprintf(result, "OK: flags=0x%x", info->flags);
		return count;
	}
	if (!strcmp(name, "dst_min") || !strcmp(name, "dst")) {
		len = strn_len(&user_buffer[i], sizeof(info->dst_min) - 1);
		if (len < 0)
			return len;
		memset(info->dst_min, 0, sizeof(info->dst_min));
		if (copy_from_user(info->dst_min, &user_buffer[i], len))
			return -EFAULT;
		if(debug)
			printk("pg: dst_min set to: %s\n", info->dst_min);
		i += len;
		sprintf(result, "OK: dst_min=%s", info->dst_min);
		return count;
	}
	if (!strcmp(name, "dst_max")) {
		len = strn_len(&user_buffer[i], sizeof(info->dst_max) - 1);
		if (len < 0)
			return len;
		memset(info->dst_max, 0, sizeof(info->dst_max));
		if (copy_from_user(info->dst_max, &user_buffer[i], len))
			return -EFAULT;
		if(debug)
			printk("pg: dst_max set to: %s\n", info->dst_max);
		i += len;
		sprintf(result, "OK: dst_max=%s", info->dst_max);
		return count;
	}
	if (!strcmp(name, "src_min")) {
		len = strn_len(&user_buffer[i], sizeof(info->src_min) - 1);
		if (len < 0)
			return len;
		memset(info->src_min, 0, sizeof(info->src_min));
		if (copy_from_user(info->src_min, &user_buffer[i], len))
			return -EFAULT;
		if(debug)
			printk("pg: src_min set to: %s\n", info->src_min);
		i += len;
		sprintf(result, "OK: src_min=%s", info->src_min);
		return count;
	}
	if (!strcmp(name, "src_max")) {
		len = strn_len(&user_buffer[i], sizeof(info->src_max) - 1);
		if (len < 0)
			return len;
		memset(info->src_max, 0, sizeof(info->src_max));
		if (copy_from_user(info->src_max, &user_buffer[i], len))
			return -EFAULT;
		if(debug)
			printk("pg: src_max set to: %s\n", info->src_max);
		i += len;
		sprintf(result, "OK: src_max=%s", info->src_max);
		return count;
	}
	if (!strcmp(name, "dstmac")) {
		char *v = valstr;
		unsigned char *m = info->dst_mac;

		len = strn_len(&user_buffer[i], sizeof(valstr) - 1);
		if (len < 0)
			return len;
		memset(valstr, 0, sizeof(valstr));
		if (copy_from_user(valstr, &user_buffer[i], len))
			return -EFAULT;
		i += len;

		for(*m = 0;*v && m < info->dst_mac + 6; v++) {
			if (*v >= '0' && *v <= '9') {
				*m *= 16;
				*m += *v - '0';
			}
			if (*v >= 'A' && *v <= 'F') {
				*m *= 16;
				*m += *v - 'A' + 10;
			}
			if (*v >= 'a' && *v <= 'f') {
				*m *= 16;
				*m += *v - 'a' + 10;
			}
			if (*v == ':') {
				m++;
				*m = 0;
			}
		}	  
		sprintf(result, "OK: dstmac");
		return count;
	}
	if (!strcmp(name, "srcmac")) {
		char *v = valstr;
		unsigned char *m = info->src_mac;

		len = strn_len(&user_buffer[i], sizeof(valstr) - 1);
		if (len < 0)
			return len;
		memset(valstr, 0, sizeof(valstr));
		if (copy_from_user(valstr, &user_buffer[i], len))
			return -EFAULT;
		i += len;

		for(*m = 0;*v && m < info->src_mac + 6; v++) {
			if (*v >= '0' && *v <= '9') {
				*m *= 16;
				*m += *v - '0';
			}
			if (*v >= 'A' && *v <= 'F') {
				*m *= 16;
				*m += *v - 'A' + 10;
			}
			if (*v >= 'a' && *v <= 'f') {
				*m *= 16;
				*m += *v - 'a' + 10;
			}
			if (*v == ':') {
				m++;
				*m = 0;
			}
		}	  
		sprintf(result, "OK: srcmac");
		return count;
	}

	if (!strcmp(name, "inject") || !strcmp(name, "start")) {
		MOD_INC_USE_COUNT;
                if (info->busy) {
                        strcpy(info->result, "Already running...\n");
                }
                else {
                        info->busy = 1;
                        strcpy(info->result, "Starting");
                        inject(info);
                        info->busy = 0;
                }
		MOD_DEC_USE_COUNT;
		return count;
	}

	sprintf(info->result, "No such parameter \"%s\"", name);
	return -EINVAL;
}


int create_proc_dir(void)
{
        int     len;
        /*  does proc_dir already exists */
        len = strlen(PG_PROC_DIR);

        for (proc_dir = proc_net->subdir; proc_dir;
             proc_dir=proc_dir->next) {
                if ((proc_dir->namelen == len) &&
                    (! memcmp(proc_dir->name, PG_PROC_DIR, len)))
                        break;
        }
        if (!proc_dir)
                proc_dir = create_proc_entry(PG_PROC_DIR, S_IFDIR, proc_net);
        if (!proc_dir) return -ENODEV;
        return 1;
}

int remove_proc_dir(void)
{
        remove_proc_entry(PG_PROC_DIR, proc_net);
        return 1;
}

static int __init init(void)
{
        int i;
	printk(version);
	cycles_calibrate();
	if (cpu_speed == 0) {
		printk("pktgen: Error: your machine does not have working cycle counter.\n");
		return -EINVAL;
	}

	create_proc_dir();

        for (i = 0; i<MAX_PKTGEN; i++) {
                memset(&(pginfos[i]), 0, sizeof(pginfos[i]));
                pginfos[i].pkt_size = ETH_ZLEN;
                pginfos[i].nfrags = 0;
                pginfos[i].clone_skb = clone_skb_d;
                pginfos[i].ipg = ipg_d;
                pginfos[i].count = count_d;
                pginfos[i].sofar = 0;
                pginfos[i].hh[12] = 0x08; /* fill in protocol.  Rest is filled in later. */
                pginfos[i].hh[13] = 0x00;
                pginfos[i].udp_src_min = 9; /* sink NULL */
                pginfos[i].udp_src_max = 9;
                pginfos[i].udp_dst_min = 9;
                pginfos[i].udp_dst_max = 9;
                
                sprintf(pginfos[i].fname, "net/%s/pg%i", PG_PROC_DIR, i);
                pginfos[i].proc_ent = create_proc_entry(pginfos[i].fname, 0600, 0);
                if (!pginfos[i].proc_ent) {
                        printk("pktgen: Error: cannot create net/%s/pg procfs entry.\n", PG_PROC_DIR);
                        goto cleanup_mem;
                }
                pginfos[i].proc_ent->read_proc = proc_read;
                pginfos[i].proc_ent->write_proc = proc_write;
                pginfos[i].proc_ent->data = (void*)(long)(i);

                sprintf(pginfos[i].busy_fname, "net/%s/pg_busy%i",  PG_PROC_DIR, i);
                pginfos[i].busy_proc_ent = create_proc_entry(pginfos[i].busy_fname, 0, 0);
                if (!pginfos[i].busy_proc_ent) {
                        printk("pktgen: Error: cannot create net/%s/pg_busy procfs entry.\n", PG_PROC_DIR);
                        goto cleanup_mem;
                }
                pginfos[i].busy_proc_ent->read_proc = proc_busy_read;
                pginfos[i].busy_proc_ent->data = (void*)(long)(i);
        }
        return 0;
        
cleanup_mem:
        for (i = 0; i<MAX_PKTGEN; i++) {
                if (strlen(pginfos[i].fname)) {
                        remove_proc_entry(pginfos[i].fname, NULL);
                }
                if (strlen(pginfos[i].busy_fname)) {
                        remove_proc_entry(pginfos[i].busy_fname, NULL);
                }
        }
	return -ENOMEM;
}


static void __exit cleanup(void)
{
        int i;
        for (i = 0; i<MAX_PKTGEN; i++) {
                if (strlen(pginfos[i].fname)) {
                        remove_proc_entry(pginfos[i].fname, NULL);
                }
                if (strlen(pginfos[i].busy_fname)) {
                        remove_proc_entry(pginfos[i].busy_fname, NULL);
                }
        }
	remove_proc_dir();
}

module_init(init);
module_exit(cleanup);

MODULE_AUTHOR("Robert Olsson <robert.olsson@its.uu.se");
MODULE_DESCRIPTION("Packet Generator tool");
MODULE_LICENSE("GPL");
MODULE_PARM(count_d, "i");
MODULE_PARM(ipg_d, "i");
MODULE_PARM(cpu_speed, "i");
MODULE_PARM(clone_skb_d, "i");



