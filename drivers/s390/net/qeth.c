/*
 *
 * linux/drivers/s390/net/qeth.c ($Revision: 1.337 $)
 *
 * Linux on zSeries OSA Express and HiperSockets support
 *
 * Copyright 2000,2003 IBM Corporation
 *
 * Author(s): Utz Bacher <utz.bacher@de.ibm.com>
 *            Cornelia Huck <cohuck@de.ibm.com> (chandev stuff,
 *                                               numerous bugfixes)
 *            Frank Pavlic <pavlic@de.ibm.com>  (query/purge ARP, SNMP, fixes)
 *            Andreas Herrmann <aherrman@de.ibm.com> (bugfixes)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * The driver supports in general all QDIO driven network devices on the
 * Hydra card.
 *
 * For all devices, three channels must be available to the driver. One
 * channel is the read channel, one is the write channel and the third
 * one is the channel used to control QDIO.
 *
 * There are several stages from the channel recognition to the running
 * network device:
 * - The channels are scanned and ordered due to the parameters (see
 *   MODULE_PARM_DESC)
 * - The card is hardsetup: this means, that the communication channels
 *   are prepared
 * - The card is softsetup: this means, that commands are issued
 *   to activate the network parameters
 * - After that, data can flow through the card (transported by QDIO)
 *
 *IPA Takeover:
 * /proc/qeth_ipa_takeover provides the possibility to add and remove
 * certain ranges of IP addresses to the driver. As soon as these
 * addresses have to be set by the driver, the driver uses the OSA
 * Address Takeover mechanism.
 * reading out of the proc-file displays the registered addresses;
 * writing into it changes the information. Only one command at one
 * time must be written into the file. Subsequent commands are ignored.
 * The following commands are available:
 * inv4
 * inv6
 * add4 <ADDR>/<mask bits>[:<interface>]
 * add6 <ADDR>/<mask bits>[:<interface>]
 * del4 <ADDR>/<mask bits>[:<interface>]
 * del6 <ADDR>/<mask bits>[:<interface>]
 * inv4 and inv6 toggle the IPA takeover behaviour for all interfaces:
 * when inv4 was input once, all addresses specified with add4 are not
 * set using the takeover mechanism, but all other IPv4 addresses are set so.
 *
 * add# adds an address range, del# deletes an address range. # corresponds
 * to the IP version (4 or 6).
 * <ADDR> is a 8 or 32byte hexadecimal view of the IP address.
 * <mask bits> specifies the number of bits which are set in the network mask.
 * <interface> is optional and specifies the interface name to which the
 * address range is bound.
 * E. g.
 *   add4 C0a80100/24
 * activates all addresses in the 192.168.10 subnet for address takeover.
 * Note, that the address is not taken over before an according ifconfig
 * is executed.
 *
 *VIPA:
 * add_vipa4 <ADDR>:<interface>
 * add_vipa6 <ADDR>:<interface>
 * del_vipa4 <ADDR>:<interface>
 * del_vipa6 <ADDR>:<interface>
 *
 * the specified address is set/unset as VIPA on the specified interface.
 * use the src_vipa package to exploit this out of arbitrary applications.
 *
 *Proxy ARP:
 *
 * add_rxip4 <ADDR>:<interface>
 * add_rxip6 <ADDR>:<interface>
 * del_rxip4 <ADDR>:<interface>
 * del_rxip6 <ADDR>:<interface>
 *
 * the specified address is set/unset as "do not fail a gratuitous ARP"
 * on the specified interface. this can be used to act as a proxy ARP.
 */

void volatile qeth_eyecatcher(void)
{
	return;
}

#include <linux/config.h>

#ifndef CONFIG_CHANDEV
#error "qeth can only be compiled with chandev support"
#endif /* CONFIG_CHANDEV */

#include <linux/module.h>

#include <linux/string.h>
#include <linux/errno.h>
#include <linux/mm.h>

#include <linux/version.h>

#include <asm/io.h>
#include <asm/ebcdic.h>
#include <linux/ctype.h>
#include <asm/semaphore.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <linux/skbuff.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif /* CONFIG_PROC_FS */
#include <net/route.h>
#include <net/arp.h>
#include <linux/in.h>
#include <linux/igmp.h>
#include <net/ip.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <net/ipv6.h>
#include <linux/in6.h>
#include <net/if_inet6.h>
#include <net/addrconf.h>
#include <linux/if_tr.h>
#include <linux/trdevice.h>
#include <linux/etherdevice.h>
#include <linux/reboot.h>

#include <linux/if_vlan.h>
#include <asm/chandev.h>

#include <asm/irq.h>
#include <asm/s390dyn.h>
#include <asm/debug.h>

#include <asm/qdio.h>

#include "qeth_mpc.h"
#include "qeth.h"

/****************** MODULE PARAMETER VARIABLES ********************/
static int qeth_sparebufs=0;
MODULE_PARM(qeth_sparebufs,"i");
MODULE_PARM_DESC(qeth_sparebufs,"the number of pre-allocated spare buffers " \
		 "reserved for low memory situations");

static int global_stay_in_mem=0;

/****************** MODULE STUFF **********************************/
#define VERSION_QETH_C "$Revision: 1.337 $"
static const char *version="qeth S/390 OSA-Express driver (" \
	VERSION_QETH_C "/" VERSION_QETH_H "/" VERSION_QETH_MPC_H
	QETH_VERSION_IPV6 QETH_VERSION_VLAN ")";

MODULE_AUTHOR("Utz Bacher <utz.bacher@de.ibm.com>");
MODULE_DESCRIPTION("Linux on zSeries OSA Express and HiperSockets support\n" \
		   "Copyright 2000,2003 IBM Corporation\n");
MODULE_LICENSE("GPL");

/******************** HERE WE GO ***********************************/

#define PROCFILE_SLEEP_SEM_MAX_VALUE 0
#define PROCFILE_IOCTL_SEM_MAX_VALUE 3
static struct semaphore qeth_procfile_ioctl_lock;
static struct semaphore qeth_procfile_ioctl_sem;
static qeth_card_t *firstcard=NULL;

static sparebufs_t sparebufs[MAX_SPARE_BUFFERS];
static int sparebuffer_count;

static unsigned int known_devices[][10]=QETH_MODELLIST_ARRAY;

static spinlock_t setup_lock;
static rwlock_t list_lock=RW_LOCK_UNLOCKED;

static debug_info_t *qeth_dbf_setup=NULL;
static debug_info_t *qeth_dbf_data=NULL;
static debug_info_t *qeth_dbf_misc=NULL;
static debug_info_t *qeth_dbf_control=NULL;
static debug_info_t *qeth_dbf_trace=NULL;
static debug_info_t *qeth_dbf_sense=NULL;
static debug_info_t *qeth_dbf_qerr=NULL;

static int proc_file_registration;
#ifdef QETH_PERFORMANCE_STATS
static int proc_perf_file_registration;
#define NOW qeth_get_micros()
#endif /* QETH_PERFORMANCE_STATS */
static int proc_ipato_file_registration;

static int ipato_inv4=0,ipato_inv6=0;
static ipato_entry_t *ipato_entries=NULL;
static spinlock_t ipato_list_lock;

typedef struct {
	char *data;
	int len;
} tempinfo_t;

/* thought I could get along without forward declarations...
 * just lazyness here */
static int qeth_reinit_thread(void*);
static void qeth_schedule_recovery(qeth_card_t *card);

inline static int QETH_IP_VERSION(struct sk_buff *skb)
{
        switch (skb->protocol) {
        case ETH_P_IPV6: return 6;
        case ETH_P_IP: return 4;
        default: return 0;
        }
}
/* not a macro, as one of the arguments is atomic_read */
static inline int qeth_min(int a,int b)
{
	if (a<b)
		return a;
	else
		return b;
}

static inline unsigned int qeth_get_millis(void)
{
	__u64 time;

	asm volatile ("STCK %0" : "=m" (time));
	return (int) (time>>22); /* time>>12 is microseconds, we divide it
				    by 1024 */
}

#ifdef QETH_PERFORMANCE_STATS
static inline unsigned int qeth_get_micros(void)
{
	__u64 time;

	asm volatile ("STCK %0" : "=m" (time));
	return (int) (time>>12);
}
#endif /* QETH_PERFORMANCE_STATS */

static void qeth_delay_millis(unsigned long msecs)
{
	unsigned int start;

	start=qeth_get_millis();
	while (qeth_get_millis()-start<msecs)
		;
}

static void qeth_wait_nonbusy(unsigned int timeout)
{
	unsigned int start;
	char dbf_text[15];

	sprintf(dbf_text,"wtnb%4x",timeout);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	start=qeth_get_millis();
        for (;;) {
                set_task_state(current,TASK_INTERRUPTIBLE);
                if (qeth_get_millis()-start>timeout) {
                        goto out;
                }
                schedule_timeout(((start+timeout-qeth_get_millis())>>10)*HZ);
        }
 out:
	set_task_state(current,TASK_RUNNING);
}

static void qeth_get_mac_for_ipm(__u32 ipm,char *mac,struct net_device *dev) {
	if (dev->type==ARPHRD_IEEE802_TR)
		ip_tr_mc_map(ipm,mac);
	else
		ip_eth_mc_map(ipm,mac);
}

#define HEXDUMP16(importance,header,ptr) \
PRINT_##importance(header "%02x %02x %02x %02x  %02x %02x %02x %02x  " \
		   "%02x %02x %02x %02x  %02x %02x %02x %02x\n", \
		   *(((char*)ptr)),*(((char*)ptr)+1),*(((char*)ptr)+2), \
		   *(((char*)ptr)+3),*(((char*)ptr)+4),*(((char*)ptr)+5), \
		   *(((char*)ptr)+6),*(((char*)ptr)+7),*(((char*)ptr)+8), \
		   *(((char*)ptr)+9),*(((char*)ptr)+10),*(((char*)ptr)+11), \
		   *(((char*)ptr)+12),*(((char*)ptr)+13), \
		   *(((char*)ptr)+14),*(((char*)ptr)+15)); \
PRINT_##importance(header "%02x %02x %02x %02x  %02x %02x %02x %02x  " \
		   "%02x %02x %02x %02x  %02x %02x %02x %02x\n", \
		   *(((char*)ptr)+16),*(((char*)ptr)+17), \
		   *(((char*)ptr)+18),*(((char*)ptr)+19), \
		   *(((char*)ptr)+20),*(((char*)ptr)+21), \
		   *(((char*)ptr)+22),*(((char*)ptr)+23), \
		   *(((char*)ptr)+24),*(((char*)ptr)+25), \
		   *(((char*)ptr)+26),*(((char*)ptr)+27), \
		   *(((char*)ptr)+28),*(((char*)ptr)+29), \
		   *(((char*)ptr)+30),*(((char*)ptr)+31));

#define atomic_swap(a,b) xchg((int*)a.counter,b)

#ifdef QETH_DBF_LIKE_HELL

#define my_read_lock(x) do { \
	void *ptr=x; \
	QETH_DBF_TEXT6(0,trace,"rd_lck"); \
	QETH_DBF_HEX6(0,trace,&ptr,sizeof(void*)); \
	read_lock(x); \
} while (0)
#define my_read_unlock(x) do { \
	void *ptr=x; \
	QETH_DBF_TEXT6(0,trace,"rd_unlck"); \
	QETH_DBF_HEX6(0,trace,&ptr,sizeof(void*)); \
	read_unlock(x); \
} while (0)
#define my_write_lock(x) do { \
	void *ptr=x; \
	QETH_DBF_TEXT6(0,trace,"wr_lck"); \
	QETH_DBF_HEX6(0,trace,&ptr,sizeof(void*)); \
	write_lock(x); \
} while (0)
#define my_write_unlock(x) do { \
	void *ptr=x; \
	QETH_DBF_TEXT6(0,trace,"wr_unlck"); \
	QETH_DBF_HEX6(0,trace,&ptr,sizeof(void*)); \
	write_unlock(x); \
} while (0)

#define my_spin_lock(x) do { \
	void *ptr=x; \
	QETH_DBF_TEXT6(0,trace,"sp_lck"); \
	QETH_DBF_HEX6(0,trace,&ptr,sizeof(void*)); \
	spin_lock(x); \
} while (0)
#define my_spin_unlock(x) do { \
	void *ptr=x; \
	QETH_DBF_TEXT6(0,trace,"sp_unlck"); \
	QETH_DBF_HEX6(0,trace,&ptr,sizeof(void*)); \
	spin_unlock(x); \
} while (0)
#define my_spin_lock_irqsave(x,y) do { \
	void *ptr=x; \
	QETH_DBF_TEXT6(0,trace,"sp_lck_i"); \
	QETH_DBF_HEX6(0,trace,&ptr,sizeof(void*)); \
	spin_lock_irqsave(x,y); \
} while (0)
#define my_spin_unlock_irqrestore(x,y) do { \
	void *ptr=x; \
	QETH_DBF_TEXT6(0,trace,"sp_nlk_i"); \
	QETH_DBF_HEX6(0,trace,&ptr,sizeof(void*)); \
	spin_unlock_irqrestore(x,y); \
} while (0)

#else /* QETH_DBF_LIKE_HELL */

#define my_read_lock(x) read_lock(x)
#define my_write_lock(x) write_lock(x)
#define my_read_unlock(x) read_unlock(x)
#define my_write_unlock(x) write_unlock(x)

#define my_spin_lock(x) spin_lock(x)
#define my_spin_unlock(x) spin_unlock(x)
#define my_spin_lock_irqsave(x,y) spin_lock_irqsave(x,y)
#define my_spin_unlock_irqrestore(x,y) spin_unlock_irqrestore(x,y)

#endif /* QETH_DBF_LIKE_HELL */

static int inline my_spin_lock_nonbusy(qeth_card_t *card,spinlock_t *lock)
{
	for (;;) {
		if (card) {
			if (atomic_read(&card->shutdown_phase)) return -1;
		}
		if (spin_trylock(lock)) return 0;
		qeth_wait_nonbusy(QETH_IDLE_WAIT_TIME);
	}
}

#ifdef CONFIG_ARCH_S390X
#define QETH_GET_ADDR(x) ((__u32)(unsigned long)x)
#else /* CONFIG_ARCH_S390X */
#define QETH_GET_ADDR(x) ((__u32)x)
#endif /* CONFIG_ARCH_S390X */

static int qeth_does_card_exist(qeth_card_t *card)
{
	qeth_card_t *c=firstcard;
	int rc=0;

	my_read_lock(&list_lock);
	while (c) {
		if (c==card) {
			rc=1;
			break;
		}
		c=c->next;
	}
	my_read_unlock(&list_lock);
	return rc;
}

static inline qeth_card_t *qeth_get_card_by_irq(int irq)
{
        qeth_card_t *card;

	my_read_lock(&list_lock);
        card=firstcard;
        while (card) {
                if ((card->irq0==irq)&&
		    (atomic_read(&card->shutdown_phase)!=
		     QETH_REMOVE_CARD_QUICK)) break;
                if ((card->irq1==irq)&&
		    (atomic_read(&card->shutdown_phase)!=
		     QETH_REMOVE_CARD_QUICK)) break;
                if ((card->irq2==irq)&&
		    (atomic_read(&card->shutdown_phase)!=
		     QETH_REMOVE_CARD_QUICK)) break;
                card=card->next;
        }
	my_read_unlock(&list_lock);

        return card;
}

static int qeth_getxdigit(char c)
{
        if ((c>='0') && (c<='9')) return c-'0';
        if ((c>='a') && (c<='f')) return c+10-'a';
        if ((c>='A') && (c<='F')) return c+10-'A';
        return -1;
}

static qeth_card_t *qeth_get_card_by_name(char *name)
{
        qeth_card_t *card;

	my_read_lock(&list_lock);
        card=firstcard;
        while (card) {
		if (!strncmp(name,card->dev_name,DEV_NAME_LEN)) break;
                card=card->next;
        }
	my_read_unlock(&list_lock);

        return card;
}

static void qeth_convert_addr_to_text(int version,__u8 *addr,char *text)
{
	if (version==4) {
		sprintf(text,"%02x%02x%02x%02x",
			addr[0],addr[1],addr[2],addr[3]);
	} else {
		sprintf(text,"%02x%02x%02x%02x%02x%02x%02x%02x" \
			"%02x%02x%02x%02x%02x%02x%02x%02x",
			addr[0],addr[1],addr[2],addr[3],
			addr[4],addr[5],addr[6],addr[7],
			addr[8],addr[9],addr[10],addr[11],
			addr[12],addr[13],addr[14],addr[15]);
	}
}

static int qeth_convert_text_to_addr(int version,char *text,__u8 *addr)
{
	int olen=(version==4)?4:16;

	while (olen--) {
		if ( (!isxdigit(*text)) || (!isxdigit(*(text+1))) )
			return -EINVAL;
		*addr=(qeth_getxdigit(*text)<<4)+qeth_getxdigit(*(text+1));
		addr++;
		text+=2;
	}
	return 0;
}

static void qeth_add_ipato_entry(int version,__u8 *addr,int mask_bits,
				 char *dev_name)
{
	ipato_entry_t *entry,*e;
	int len=(version==4)?4:16;

	entry=(ipato_entry_t*)kmalloc(sizeof(ipato_entry_t),GFP_KERNEL);
	if (!entry) {
		PRINT_ERR("not enough memory for ipato allocation\n");
		return;
	}
	entry->version=version;
	memcpy(entry->addr,addr,len);
	if (dev_name) {
		strncpy(entry->dev_name,dev_name,DEV_NAME_LEN);
		if (qeth_get_card_by_name(dev_name)->options.ena_ipat!=
		    ENABLE_TAKEOVER)
			PRINT_WARN("IP takeover is not enabled on %s! " \
				   "Ignoring line\n",dev_name);
	} else
		memset(entry->dev_name,0,DEV_NAME_LEN);
	entry->mask_bits=mask_bits;
	entry->next=NULL;

	my_spin_lock(&ipato_list_lock);
	if (ipato_entries) {
		e=ipato_entries;
		while (e) {
			if ( (e->version==version) &&
			     (e->mask_bits==mask_bits) &&
			     ( ((dev_name)&&!strncmp(e->dev_name,dev_name,
						     DEV_NAME_LEN)) ||
			       (!dev_name) ) &&
			     (!memcmp(e->addr,addr,len)) ) {
				PRINT_INFO("ipato to be added does already " \
					   "exist\n");
				kfree(entry);
				goto out;
			     }
			if (e->next) e=e->next; else break;
		}
		e->next=entry;
	} else
		ipato_entries=entry;
out:
	my_spin_unlock(&ipato_list_lock);
}

static void qeth_del_ipato_entry(int version,__u8 *addr,int mask_bits,
				 char *dev_name)
{
	ipato_entry_t *e,*e_before;
	int len=(version==4)?4:16;
	int found=0;
	
	my_spin_lock(&ipato_list_lock);
	e=ipato_entries;
	if ( (e->version==version) &&
	     (e->mask_bits==mask_bits) &&
	     (!memcmp(e->addr,addr,len)) ) {
		ipato_entries=e->next;
		kfree(e);
	} else while (e) {
		e_before=e;
		e=e->next;
		if (!e) break;
		if ( (e->version==version) &&
		     (e->mask_bits==mask_bits) &&
		     ( ((dev_name)&&!strncmp(e->dev_name,dev_name,
					     DEV_NAME_LEN)) ||
		       (!dev_name) ) &&
		     (!memcmp(e->addr,addr,len)) ) {
			e_before->next=e->next;
			kfree(e);
			found=1;
			break;
		}
	}
	if (!found)
		PRINT_INFO("ipato to be deleted does not exist\n");
	my_spin_unlock(&ipato_list_lock);
}

static void qeth_convert_addr_to_bits(__u8 *addr,char *bits,int len)
{
	int i,j;
	__u8 octet;
	
	for (i=0;i<len;i++) {
		octet=addr[i];
		for (j=7;j>=0;j--) {
			bits[i*8+j]=(octet&1)?1:0;
			octet>>=1;
		}
	}
}

static int qeth_is_ipa_covered_by_ipato_entries(int version,__u8 *addr,
						qeth_card_t *card)
{
	char *memarea,*addr_bits,*entry_bits;
	int len=(version==4)?4:16;
	int invert=(version==4)?ipato_inv4:ipato_inv6;
	int result=0;
	ipato_entry_t *e;

	if (card->options.ena_ipat!=ENABLE_TAKEOVER) {
		return 0;
	}

	memarea=kmalloc(256,GFP_KERNEL);
	if (!memarea) {
		PRINT_ERR("not enough memory to check out whether to " \
			  "use ipato\n");
		return 0;
	}
	addr_bits=memarea;
	entry_bits=memarea+128;
	qeth_convert_addr_to_bits(addr,addr_bits,len);
	e=ipato_entries;
	while (e) {
		qeth_convert_addr_to_bits(e->addr,entry_bits,len);
		if ( (!memcmp(addr_bits,entry_bits,
			      __min(len*8,e->mask_bits))) &&
		     ( (e->dev_name[0]&&
			(!strncmp(e->dev_name,card->dev_name,DEV_NAME_LEN))) ||
		       (!e->dev_name[0]) ) ) {
			result=1;
			break;
		}
		e=e->next;
	}

	kfree(memarea);
	if (invert)
		return !result;
	else
		return result;
}

static void qeth_set_dev_flag_running(qeth_card_t *card)
{
	if (card) {
		card->dev->flags|=IFF_RUNNING;
/*
		clear_bit(__LINK_STATE_DOWN,&dev->flags);
*/
	}
}

static void qeth_set_dev_flag_norunning(qeth_card_t *card)
{
	if (card) {
		card->dev->flags&=~IFF_RUNNING;
/*
		set_bit(__LINK_STATE_DOWN,&dev->flags);
*/
	}
}

static void qeth_restore_dev_flag_state(qeth_card_t *card)
{
	if (card) {
		if (card->saved_dev_flags&IFF_RUNNING)
			card->dev->flags|=IFF_RUNNING;
		else
			card->dev->flags&=~IFF_RUNNING;
/*
		if (card->saved_dev_flags&__LINK_STATE_DOWN)
			set_bit(__LINK_STATE_DOWN,&card->dev->flags);
		else
			clear_bit(__LINK_STATE_DOWN,&card->dev->flags);
*/
	}
}

static void qeth_save_dev_flag_state(qeth_card_t *card)
{
	if (card) {
		card->saved_dev_flags=card->dev->flags&IFF_RUNNING;
/*
		card->saved_dev_flags=card->dev->flags&__LINK_STATE_DOWN;
*/
	}
}

static inline int netif_is_busy(struct net_device *dev)
{
        return(test_bit(__LINK_STATE_XOFF,&dev->flags));
}

static int qeth_open(struct net_device *dev)
{
	char dbf_text[15];
	qeth_card_t *card;

	card=(qeth_card_t *)dev->priv;
	sprintf(dbf_text,"open%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	QETH_DBF_TEXT2(0,setup,dbf_text);

	qeth_save_dev_flag_state(card);

	netif_start_queue(dev);
        if (!atomic_swap(&((qeth_card_t*)dev->priv)->is_open,1)) {
                MOD_INC_USE_COUNT;
        }
	return 0;
}

static int qeth_set_config(struct net_device *dev,struct ifmap *map)
{
	qeth_card_t *card=(qeth_card_t*)dev->priv;
	char dbf_text[15];

	sprintf(dbf_text,"nscf%04x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	return -EOPNOTSUPP;
}

static int qeth_is_multicast_skb_at_all(struct sk_buff *skb,int version)
{
	int i;
	if (skb->dst && skb->dst->neighbour) {
		i=skb->dst->neighbour->type;
		return ((i==RTN_BROADCAST)||
			(i==RTN_MULTICAST)||
			(i==RTN_ANYCAST))?i:0;
	}
	/* ok, we've to try it somehow else */
	if (version==4) {
		return ((skb->nh.raw[16]&0xf0)==0xe0)?RTN_MULTICAST:0;
	} else if (version==6) {
		return (skb->nh.raw[24]==0xff)?RTN_MULTICAST:0;
	}
	return 0;
}

static int qeth_get_prioqueue(qeth_card_t *card,struct sk_buff *skb,
			      int multicast,int version)
{
	if (!version) return QETH_DEFAULT_QUEUE;
        switch (card->no_queues) {
        case 1:
                return 0;
        case 4:
		if ( (card->can_do_async_iqd) &&
		     (card->options.async_iqd==ASYNC_IQD) ) {
			return card->no_queues-1;
		}
		if (card->is_multicast_different) {
			if (multicast) {
			    return card->is_multicast_different&
				    (card->no_queues-1);
			} else {
				return 0;
			}
		}
		if (card->options.do_prio_queueing) {
			if (version==4) {
				if (card->options.do_prio_queueing==
				    PRIO_QUEUEING_TOS) {
					if (skb->nh.iph->tos&
					    IP_TOS_NOTIMPORTANT) {
						return 3;
					}
					if (skb->nh.iph->tos&
					    IP_TOS_LOWDELAY) {
						return 0;
					}
					if (skb->nh.iph->tos&
					    IP_TOS_HIGHTHROUGHPUT) {
						return 1;
					}
					if (skb->nh.iph->tos&
					    IP_TOS_HIGHRELIABILITY) {
						return 2;
					}
					return QETH_DEFAULT_QUEUE;
				}
				if (card->options.do_prio_queueing==
				    PRIO_QUEUEING_PREC) {
					return 3-(skb->nh.iph->tos>>6);
				}
			} else if (version==6) {
/********************
 ********************
TODO: IPv6!!!
********************/
			}
                        return card->options.default_queue;
                } else return card->options.default_queue;
        default:
                return 0;
        }
}

static void qeth_wakeup(qeth_card_t *card) {
	char dbf_text[15];

	sprintf(dbf_text,"wkup%4x",card->irq0);
	QETH_DBF_TEXT5(0,trace,dbf_text);

        atomic_set(&card->data_has_arrived,1);
	spin_lock(&card->wait_q_lock);
	if (atomic_read(&card->wait_q_active)) {
		wake_up(&card->wait_q);
	}
	spin_unlock(&card->wait_q_lock);
}

static int qeth_check_idx_response(unsigned char *buffer)
{
	if (!buffer)
		return 0;
        if ((buffer[2]&0xc0)==0xc0) {
                return -EIO;
        }
        return 0;
}

static int qeth_get_cards_problem(qeth_card_t *card,unsigned char *buffer,
				  int irq,int dstat,int cstat,int rqparam,
				  char *irb,char *sense)
{
	char dbf_text[15];
	int problem=0;

	if (atomic_read(&card->shutdown_phase)) return 0;
	if (dstat&DEV_STAT_UNIT_CHECK) {
		if (irq==card->irq2) {
			sprintf(dbf_text,"ACHK%04x",card->irq0);
			QETH_DBF_TEXT1(0,trace,dbf_text);
			problem=PROBLEM_ACTIVATE_CHECK_CONDITION;
			goto out;
		}
		if (sense[SENSE_RESETTING_EVENT_BYTE]&
		    SENSE_RESETTING_EVENT_FLAG) {
			sprintf(dbf_text,"REVN%04x",card->irq0);
			QETH_DBF_TEXT1(0,trace,dbf_text);
			problem=PROBLEM_RESETTING_EVENT_INDICATOR;
			goto out;
		}
		if (sense[SENSE_COMMAND_REJECT_BYTE]&
		    SENSE_COMMAND_REJECT_FLAG) {
			sprintf(dbf_text,"CREJ%04x",card->irq0);
			QETH_DBF_TEXT1(0,trace,dbf_text);
			problem=PROBLEM_COMMAND_REJECT;
			goto out;
		}
		if ( (sense[2]==0xaf)&&(sense[3]==0xfe) ) {
			sprintf(dbf_text,"AFFE%04x",card->irq0);
			QETH_DBF_TEXT1(0,trace,dbf_text);
			problem=PROBLEM_AFFE;
			goto out;
		}
		if ( (!sense[0]) && (!sense[1]) &&
		     (!sense[2]) && (!sense[3]) ) {
			sprintf(dbf_text,"ZSNS%04x",card->irq0);
			QETH_DBF_TEXT1(0,trace,dbf_text);
			problem=PROBLEM_ZERO_SENSE_DATA;
			goto out;
		}
		sprintf(dbf_text,"GCHK%04x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		problem=PROBLEM_GENERAL_CHECK;
		goto out;
	}
	if (cstat& (SCHN_STAT_CHN_CTRL_CHK|SCHN_STAT_INTF_CTRL_CHK|
		    SCHN_STAT_CHN_DATA_CHK|SCHN_STAT_CHAIN_CHECK|
		    SCHN_STAT_PROT_CHECK|SCHN_STAT_PROG_CHECK) ) {
		sprintf(dbf_text,"GCHK%04x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_HEX1(0,misc,irb,__max(QETH_DBF_MISC_LEN,64));
		PRINT_WARN("check on irq x%x, dstat=x%x, cstat=x%x, " \
			   "rqparam=x%x\n",irq,dstat,cstat,rqparam);
		HEXDUMP16(WARN,"irb: ",irb);
		HEXDUMP16(WARN,"irb: ",((char*)irb)+32);
		problem=PROBLEM_GENERAL_CHECK;
		goto out;
	}
        if (qeth_check_idx_response(buffer)) {
                PRINT_WARN("received an IDX TERMINATE on irq 0x%X/0x%X " \
			   "with cause code 0x%02x%s\n",
			   card->irq0,card->irq1,buffer[4],
			   (buffer[4]==0x22)?" -- try another portname":"");
		sprintf(dbf_text,"RTRM%04x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
                problem=PROBLEM_RECEIVED_IDX_TERMINATE;
		goto out;
        }
        if (IS_IPA(buffer) && !IS_IPA_REPLY(buffer)) {
                if ( *(PDU_ENCAPSULATION(buffer))==IPA_CMD_STOPLAN ) {
			atomic_set(&card->is_startlaned,0);
			/* we don't do a  netif_stop_queue(card->dev);
			   we better discard all packets --
			   the outage could take longer */
			PRINT_WARN("Link failure on %s (CHPID 0x%X) -- " \
				   "there is a network problem or someone " \
				   "pulled the cable or disabled the port."
				   "Discarding outgoing packets.\n",
				   card->dev_name,card->chpid);
			sprintf(dbf_text,"CBOT%04x",card->irq0);
			QETH_DBF_TEXT1(0,trace,dbf_text);
			qeth_set_dev_flag_norunning(card);
                        problem=0;
			goto out;
                }
		/* we checked for buffer!=0 in IS_IPA */
                if ( *(PDU_ENCAPSULATION(buffer))==IPA_CMD_STARTLAN ) {
			if (!atomic_read(&card->is_startlaned)) {
				atomic_set(&card->is_startlaned,1);
				problem=PROBLEM_CARD_HAS_STARTLANED;
			}
			goto out;
                }
		if ( *(PDU_ENCAPSULATION(buffer))==
		     IPA_CMD_REGISTER_LOCAL_ADDR ) {
			sprintf(dbf_text,"irla%04x",card->irq0);
			QETH_DBF_TEXT3(0,trace,dbf_text);
		}
		if ( *(PDU_ENCAPSULATION(buffer))==
		     IPA_CMD_UNREGISTER_LOCAL_ADDR ) {
			sprintf(dbf_text,"irla%04x",card->irq0);
			QETH_DBF_TEXT3(0,trace,dbf_text);
		}
                PRINT_WARN("probably a problem on %s: received data is " \
			   "IPA, but not a reply: command=0x%x\n",
			   card->dev_name,*(PDU_ENCAPSULATION(buffer)+1));
		sprintf(dbf_text,"INRP%04x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		goto out;
        }
        /* no probs */
out:
	if (problem) {
		sprintf(dbf_text,"gcpr%4x",card->irq0);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		sprintf(dbf_text,"%2x%2x%4x",dstat,cstat,problem);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		sprintf(dbf_text,"%8x",rqparam);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		if (buffer)
			QETH_DBF_HEX3(0,trace,&buffer,sizeof(void*));
		QETH_DBF_HEX3(0,trace,&irb,sizeof(void*));
		QETH_DBF_HEX3(0,trace,&sense,sizeof(void*));
	}
	atomic_set(&card->problem,problem);
	return problem;
}


static void qeth_issue_next_read(qeth_card_t *card)
{
        int result,result2;
	char dbf_text[15];

	sprintf(dbf_text,"isnr%04x",card->irq0);
	QETH_DBF_TEXT5(0,trace,dbf_text);

        /* set up next read ccw */
        memcpy(&card->dma_stuff->read_ccw,READ_CCW,sizeof(ccw1_t));
        card->dma_stuff->read_ccw.count=QETH_BUFSIZE;
        /* recbuf is not yet used by read channel program */
        card->dma_stuff->read_ccw.cda=QETH_GET_ADDR(card->dma_stuff->recbuf);

	/* we don't s390irq_spin_lock_irqsave(card->irq0,flags), as
	   we are only called in the interrupt handler */
        result=do_IO(card->irq0,&card->dma_stuff->read_ccw,
		     MPC_SETUP_STATE,0,0);
        if (result) {
		qeth_delay_millis(QETH_WAIT_BEFORE_2ND_DOIO);
                result2=do_IO(card->irq0,&card->dma_stuff->read_ccw,
			      MPC_SETUP_STATE,0,0);
                PRINT_WARN("read handler on irq x%x, read: do_IO " \
                           "returned %i, next try returns %i\n",
                           card->irq0,result,result2);
		sprintf(dbf_text,"IsNR%04x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		sprintf(dbf_text,"%04x%04x",(__s16)result,(__s16)result2);
		QETH_DBF_TEXT1(0,trace,dbf_text);
        }
}

static int qeth_is_to_recover(qeth_card_t *card,int problem)
{
        switch (problem) {
        case PROBLEM_CARD_HAS_STARTLANED:
                return 1;
        case PROBLEM_RECEIVED_IDX_TERMINATE:
                if (atomic_read(&card->in_recovery)) {
			return 1;
		} else {
			qeth_set_dev_flag_norunning(card);
			return 0;
		}
	case PROBLEM_ACTIVATE_CHECK_CONDITION:
		return 1;
	case PROBLEM_RESETTING_EVENT_INDICATOR:
		return 1;
	case PROBLEM_COMMAND_REJECT:
		return 0;
	case PROBLEM_ZERO_SENSE_DATA:
		return 0;
	case PROBLEM_GENERAL_CHECK:
		return 1;
	case PROBLEM_BAD_SIGA_RESULT:
		return 1;
	case PROBLEM_USER_TRIGGERED_RECOVERY:
		return 1;
	case PROBLEM_AFFE:
		return 1;
	case PROBLEM_MACHINE_CHECK:
		return 1;
	case PROBLEM_TX_TIMEOUT:
		return 1;
        }
        return 0;
}

static int qeth_wait_for_event(atomic_t *var,unsigned int timeout)
{
        unsigned int start;
	int retval;
	char dbf_text[15];

	QETH_DBF_TEXT5(0,trace,"wait4evn");
	sprintf(dbf_text,"%08x",timeout);
	QETH_DBF_TEXT5(0,trace,dbf_text);
	QETH_DBF_HEX5(0,trace,&var,sizeof(void*));

        start=qeth_get_millis();
        for (;;) {
                set_task_state(current,TASK_INTERRUPTIBLE);
                if (atomic_read(var)) {
			retval=0;
			goto out;
		}
                if (qeth_get_millis()-start>timeout) {
			retval=-ETIME;
			goto out;
		}
                schedule_timeout(((start+timeout-qeth_get_millis())>>10)*HZ);
        }
 out:
	set_task_state(current,TASK_RUNNING);

	return retval;
}

static int qeth_get_spare_buf(void)
{
	int i=0;
	char dbf_text[15];

	while (i<sparebuffer_count) {
		if (!atomic_compare_and_swap(SPAREBUF_FREE,SPAREBUF_USED,
					     &sparebufs[i].status)) {
			sprintf(dbf_text,"gtspb%3x",i);
			QETH_DBF_TEXT4(0,trace,dbf_text);
			return i;
		}
		i++;
	}
	QETH_DBF_TEXT3(0,trace,"nospbuf");

	return -1;
}

static void qeth_put_spare_buf(int no)
{
	char dbf_text[15];
	
	sprintf(dbf_text,"ptspb%3x",no);
	QETH_DBF_TEXT4(0,trace,dbf_text);
	atomic_set(&sparebufs[no].status, SPAREBUF_FREE);
}

static inline void qeth_put_buffer_pool_entry(qeth_card_t *card,int entry_no)
{
	if (entry_no&SPAREBUF_MASK)
		qeth_put_spare_buf(entry_no&(~SPAREBUF_MASK));
	else
		card->inbound_buffer_pool_entry_used[entry_no]=BUFFER_UNUSED;
}

static inline int qeth_get_empty_buffer_pool_entry(qeth_card_t *card)
{
	int i;
	int max_buffers=card->options.inbound_buffer_count;
	
	for (i=0;i<max_buffers;i++) {
		if (xchg((int*)&card->inbound_buffer_pool_entry_used[i],
			 BUFFER_USED)==BUFFER_UNUSED) return i;
	}
	return -1;
}

static inline void qeth_clear_input_buffer(qeth_card_t *card,int bufno)
{
	qdio_buffer_t *buffer;
	int i;
	int elements,el_m_1;
	void *ptr;
#ifdef QETH_DBF_LIKE_HELL
	char dbf_text[15];

	sprintf(dbf_text,"clib%4x",card->irq0);
	QETH_DBF_TEXT6(0,trace,dbf_text);
	sprintf(dbf_text,"bufno%3x",bufno);
	QETH_DBF_TEXT6(0,trace,dbf_text);
#endif /* QETH_DBF_LIKE_HELL */

	buffer=&card->inbound_qdio_buffers[bufno];
	elements=BUFFER_MAX_ELEMENTS;
	el_m_1=elements-1;

	for (i=0;i<elements;i++) {
		if (i==el_m_1)
			buffer->element[i].flags=SBAL_FLAGS_LAST_ENTRY;
		else
			buffer->element[i].flags=0;

		buffer->element[i].length=PAGE_SIZE;
		ptr=INBOUND_BUFFER_POS(card,bufno,i);
		if (card->do_pfix) {
			/* we assume here, that ptr&(PAGE_SIZE-1)==0 */
			buffer->element[i].addr=(void *)pfix_get_page_addr(ptr);
			card->real_inb_buffer_addr[bufno][i]=ptr;
		} else {
			buffer->element[i].addr=ptr;
		}
	}
}

static void qeth_queue_input_buffer(qeth_card_t *card,int bufno,
				    unsigned int under_int)
{
	int count=0,start=0,stop=0,pos;
	int result;
	int cnt1,cnt2=0;
	int wrapped=0;
	int i;
	int requeue_counter;
	char dbf_text[15];
	int no;

#ifdef QETH_DBF_LIKE_HELL
	sprintf(dbf_text,"qibf%4x",card->irq0);
	QETH_DBF_TEXT5(0,trace,dbf_text);
	sprintf(dbf_text,"%4x%4x",under_int,bufno);
	QETH_DBF_TEXT5(0,trace,dbf_text);
#endif /* QETH_DBF_LIKE_HELL */
	atomic_inc(&card->requeue_counter);
	if (atomic_read(&card->requeue_counter) > QETH_REQUEUE_THRESHOLD) {
		if (!spin_trylock(&card->requeue_input_lock)) {
#ifndef QETH_DBF_LIKE_HELL
			sprintf(dbf_text,"qibl%4x",card->irq0);
			QETH_DBF_TEXT5(0,trace,dbf_text);
#endif /* QETH_DBF_LIKE_HELL */
			return;
		}
		requeue_counter=atomic_read(&card->requeue_counter);
		pos=atomic_read(&card->requeue_position);

		start=pos;
		/* omit the situation with 128 simultaneously
		   enqueued buffers, as then we can't benefit from PCI
		   avoidance anymore -- therefore we let count not grow as
		   big as requeue_counter */
		while ( (!atomic_read(&card->inbound_buffer_refcnt[pos])) &&
			(count<requeue_counter-1) ) {
			no=qeth_get_empty_buffer_pool_entry(card);
			if (no==-1) {
				if (count) break;
				no=qeth_get_spare_buf();
				if (no==-1) {
					PRINT_ERR("%s: no more input "\
						  "buffers available! " \
						  "Inbound traffic could " \
						  "be lost! Try to spend " \
						  "more memory for qeth\n",
						  card->dev_name);
					sprintf(dbf_text,"QINB%4x",card->irq0);
					QETH_DBF_TEXT2(1,trace,dbf_text);
					goto out;
				}
				card->inbound_buffer_entry_no[pos]=
					no|SPAREBUF_MASK;
			}
			card->inbound_buffer_entry_no[pos]=no;
			atomic_set(&card->inbound_buffer_refcnt[pos],1);
			count++;
			if (pos>=QDIO_MAX_BUFFERS_PER_Q-1) {
				pos=0;
				wrapped=1;
			} else pos++;
		}
		/* stop points to the position after the last element */
		stop=pos;

#ifdef QETH_DBF_LIKE_HELL
		sprintf(dbf_text,"qibi%4x",card->irq0);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		sprintf(dbf_text,"%4x",requeue_counter);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		sprintf(dbf_text,"%4x%4x",start,stop);
		QETH_DBF_TEXT3(0,trace,dbf_text);
#endif /* QETH_DBF_LIKE_HELL */
		if (wrapped) {
			cnt1=QDIO_MAX_BUFFERS_PER_Q-start;
			cnt2=stop;
		} else {
			cnt1=count;
			/* cnt2 is already set to 0 */
		}

		atomic_sub(count,&card->requeue_counter);
		/* this is the only place where card->requeue_position is
		   written to, so that's ok (as it is in a lock) */
		atomic_set(&card->requeue_position,
			   (atomic_read(&card->requeue_position)+count)
			   &(QDIO_MAX_BUFFERS_PER_Q-1));

		if (cnt1) {
			for (i=start;i<start+cnt1;i++) {
				qeth_clear_input_buffer(card,i);
			}
			result=do_QDIO(card->irq2,
				       QDIO_FLAG_SYNC_INPUT|under_int,
				       0,start,cnt1,
				       NULL);
			if (result) {
				PRINT_WARN("qeth_queue_input_buffer's " \
			    		   "do_QDIO returnd %i " \
		    			   "(irq 0x%x)\n",
	    				   result,card->irq2);
				sprintf(dbf_text,"QIDQ%4x",card->irq0);
				QETH_DBF_TEXT1(0,trace,dbf_text);
				sprintf(dbf_text,"%4x%4x",result,
					requeue_counter);
				QETH_DBF_TEXT1(0,trace,dbf_text);
				sprintf(dbf_text,"%4x%4x",start,cnt1);
				QETH_DBF_TEXT1(1,trace,dbf_text);
			}
		}
		if (cnt2) {
			for (i=0;i<cnt2;i++) {
				qeth_clear_input_buffer(card,i);
			}
			result=do_QDIO(card->irq2,
				       QDIO_FLAG_SYNC_INPUT|under_int,0,
				       0,cnt2,
				       NULL);
			if (result) {
				PRINT_WARN("qeth_queue_input_buffer's " \
			    		   "do_QDIO returnd %i " \
		    			   "(irq 0x%x)\n",
	    				   result,card->irq2);
				sprintf(dbf_text,"QIDQ%4x",card->irq0);
				QETH_DBF_TEXT1(0,trace,dbf_text);
				sprintf(dbf_text,"%4x%4x",result,
					requeue_counter);
				QETH_DBF_TEXT1(0,trace,dbf_text);
				sprintf(dbf_text,"%4x%4x",0,cnt2);
				QETH_DBF_TEXT1(1,trace,dbf_text);
			}
		}
out:
		my_spin_unlock(&card->requeue_input_lock);
	}

}

static inline struct sk_buff *qeth_get_skb(unsigned int len)
{
	struct sk_buff *skb;

#ifdef QETH_VLAN
	skb=dev_alloc_skb(len+VLAN_HLEN);
	if (skb) skb_reserve(skb,VLAN_HLEN);
#else /* QETH_VLAN */
	skb=dev_alloc_skb(len);
#endif /* QETH_VLAN */
	return skb;
}

static struct sk_buff *qeth_get_next_skb(qeth_card_t *card,
	       				 int *element_ptr,int *pos_in_el_ptr,
			       		 void **hdr_ptr,
					 qdio_buffer_t *buffer)
{
	int length;
	char *data_ptr;
	int step,len_togo,element,pos_in_el;
	int curr_len;
	int max_elements;
	struct sk_buff *skb;
	char dbf_text[15];

	max_elements=BUFFER_MAX_ELEMENTS;

#define SBALE_LEN(x) ((x>=max_elements)?0:(buffer->element[x].length))
#define SBALE_ADDR(x) (buffer->element[x].addr)

	element=*element_ptr;

	if (element>=max_elements) {
		PRINT_WARN("irq 0x%x: error in interpreting buffer (data " \
			   "too long), %i elements.\n",card->irq0,element);
		sprintf(dbf_text,"IEDL%4x",card->irq0);
		QETH_DBF_TEXT0(0,trace,dbf_text);
		sprintf(dbf_text,"%4x%4x",*element_ptr,*pos_in_el_ptr);
		QETH_DBF_TEXT0(1,trace,dbf_text);
		QETH_DBF_HEX0(0,misc,buffer,QETH_DBF_MISC_LEN);
		QETH_DBF_HEX0(0,misc,buffer+QETH_DBF_MISC_LEN,
			      QETH_DBF_MISC_LEN);
		return NULL;
	}

	pos_in_el=*pos_in_el_ptr;

	curr_len=SBALE_LEN(element);
	if (curr_len>PAGE_SIZE) {
		PRINT_WARN("irq 0x%x: bad element length in element %i: " \
			   "0x%x\n",card->irq0,element,curr_len);
		sprintf(dbf_text,"BELN%4x",card->irq0);
		QETH_DBF_TEXT0(0,trace,dbf_text);
		sprintf(dbf_text,"%4x",curr_len);
		QETH_DBF_TEXT0(0,trace,dbf_text);
		sprintf(dbf_text,"%4x%4x",*element_ptr,*pos_in_el_ptr);
		QETH_DBF_TEXT0(1,trace,dbf_text);
		QETH_DBF_HEX0(0,misc,buffer,QETH_DBF_MISC_LEN);
		QETH_DBF_HEX0(0,misc,buffer+QETH_DBF_MISC_LEN,
			      QETH_DBF_MISC_LEN);
		return NULL;
	}
	/* header fits in current element? */
	if (curr_len<pos_in_el+QETH_HEADER_SIZE) {
		if (!pos_in_el) {
#ifdef QETH_DBF_LIKE_HELL
			sprintf(dbf_text,"gnmh%4x",card->irq0);
			QETH_DBF_TEXT6(0,trace,dbf_text);
#endif /* QETH_DBF_LIKE_HELL */
			return NULL; /* no more data in buffer */
		}
		/* set hdr to next element */
		element++;
		pos_in_el=0;
		curr_len=SBALE_LEN(element);
		/* does it fit in there? */
		if (curr_len<QETH_HEADER_SIZE) {
#ifdef QETH_DBF_LIKE_HELL
			sprintf(dbf_text,"gdnf%4x",card->irq0);
			QETH_DBF_TEXT6(0,trace,dbf_text);
#endif /* QETH_DBF_LIKE_HELL */
			return NULL;
		}
	}

	*hdr_ptr=SBALE_ADDR(element)+pos_in_el;

	length=*(__u16*)((char*)(*hdr_ptr)+QETH_HEADER_LEN_POS);

#ifdef QETH_DBF_LIKE_HELL
	sprintf(dbf_text,"gnHd%4x",card->irq0);
	QETH_DBF_TEXT6(0,trace,dbf_text);
	QETH_DBF_HEX6(0,trace,hdr_ptr,sizeof(void*));
#endif /* QETH_DBF_LIKE_HELL */

	pos_in_el+=QETH_HEADER_SIZE;
	if (curr_len<=pos_in_el) {
		/* switch to next element for data */
		pos_in_el=0;
		element++;
		curr_len=SBALE_LEN(element);
		if (!curr_len) {
			PRINT_WARN("irq 0x%x: inb. buffer with more headers " \
				   "than data areas (%i elements).\n",
				   card->irq0,element);
			sprintf(dbf_text,"IEMH%4x",card->irq0);
			QETH_DBF_TEXT0(0,trace,dbf_text);
			sprintf(dbf_text,"%2x%2x%4x",element,*element_ptr,
				*pos_in_el_ptr);
			QETH_DBF_TEXT0(1,trace,dbf_text);
			QETH_DBF_HEX0(0,misc,buffer,QETH_DBF_MISC_LEN);
			QETH_DBF_HEX0(0,misc,buffer+QETH_DBF_MISC_LEN,
				      QETH_DBF_MISC_LEN);
			return NULL;
		}
	}

	data_ptr=SBALE_ADDR(element)+pos_in_el;

	if (card->options.fake_ll==FAKE_LL) {
		skb=qeth_get_skb(length+QETH_FAKE_LL_LEN);
		if (!skb) goto nomem;
		skb_pull(skb,QETH_FAKE_LL_LEN);
		if (!skb) {
			dev_kfree_skb_irq(skb);
			goto nomem;
		}
	} else {
		skb=qeth_get_skb(length);
		if (!skb) goto nomem;
	}

	if (card->easy_copy_cap)
		memcpy(skb_put(skb,length),data_ptr,length);

#ifdef QETH_DBF_LIKE_HELL
	QETH_DBF_HEX6(0,trace,&data_ptr,sizeof(void*));
	QETH_DBF_HEX6(0,trace,&skb,sizeof(void*));
#endif /* QETH_DBF_LIKE_HELL */

	len_togo=length;
	while (1) {
		step=qeth_min(len_togo,curr_len-pos_in_el);
		if (!step) {
			PRINT_WARN("irq 0x%x: unexpected end of buffer, " \
				   "length of element %i is 0. Discarding " \
				   "packet.\n",card->irq0,element);
			sprintf(dbf_text,"IEUE%4x",card->irq0);
			QETH_DBF_TEXT0(0,trace,dbf_text);
			sprintf(dbf_text,"%2x%2x%4x",element,*element_ptr,
				*pos_in_el_ptr);
			QETH_DBF_TEXT0(0,trace,dbf_text);
			sprintf(dbf_text,"%4x%4x",len_togo,step);
			QETH_DBF_TEXT0(0,trace,dbf_text);
			sprintf(dbf_text,"%4x%4x",curr_len,pos_in_el);
			QETH_DBF_TEXT0(1,trace,dbf_text);
			QETH_DBF_HEX0(0,misc,buffer,QETH_DBF_MISC_LEN);
			QETH_DBF_HEX0(0,misc,buffer+QETH_DBF_MISC_LEN,
				      QETH_DBF_MISC_LEN);
			dev_kfree_skb_irq(skb);
			return NULL;
		}
		if (!card->easy_copy_cap)
			memcpy(skb_put(skb,step),data_ptr,step);
		len_togo-=step;
		if (len_togo) {
			pos_in_el=0;
			element++;
			curr_len=SBALE_LEN(element);
			data_ptr=SBALE_ADDR(element);
		} else {
#ifdef QETH_INBOUND_PACKING_1_PACKET_PER_SBALE
			element++;
			/* we don't need to calculate curr_len */
			pos_in_el=0;
#else /* QETH_INBOUND_PACKING_1_PACKET_PER_SBALE */
			pos_in_el+=step;
#endif /* QETH_INBOUND_PACKING_1_PACKET_PER_SBALE */
			break;
		}
	}

#ifdef QETH_DBF_LIKE_HELL
	sprintf(dbf_text,"%4x%4x",element,pos_in_el);
	QETH_DBF_TEXT6(0,trace,dbf_text);
#endif /* QETH_DBF_LIKE_HELL */

	*element_ptr=element;
	*pos_in_el_ptr=pos_in_el;

	return skb;

nomem:
	if (net_ratelimit()) {
		PRINT_WARN("no memory for packet from %s\n",card->dev_name);
	}
	sprintf(dbf_text,"NOMM%4x",card->irq0);
	QETH_DBF_TEXT0(0,trace,dbf_text);
	return NULL;
}

static void qeth_transform_outbound_addrs(qeth_card_t *card,
					  qdio_buffer_t *buffer)
{
	int i;
	void *ptr;

	if (card->do_pfix) {
		for (i=0;i<QDIO_MAX_ELEMENTS_PER_BUFFER;i++) {
			ptr=buffer->element[i].addr;
			buffer->element[i].addr=(void *)pfix_get_addr(ptr);
		}
	}
}
static void qeth_get_linux_addrs_for_buffer(qeth_card_t *card,int buffer_no)
{
	int i;
	void *ptr;

	if (card->do_pfix) {
		for (i=0;i<QDIO_MAX_ELEMENTS_PER_BUFFER;i++) {
			ptr=card->inbound_qdio_buffers[buffer_no].
				element[i].addr;
			card->inbound_qdio_buffers[buffer_no].element[i].addr=
				card->real_inb_buffer_addr[buffer_no][i]+
				((unsigned long)ptr&(PAGE_SIZE-1));
		}
	}
}

static void qeth_read_in_buffer(qeth_card_t *card,int buffer_no)
{
        struct sk_buff *skb;
        void *hdr_ptr;
	int element=0,pos_in_el=0;
	int version;
	qdio_buffer_t *buffer;
	unsigned short cast_type;
#ifdef QETH_VLAN
	__u16 *vlan_tag;
#endif
	int i;
	int max_elements;
	char dbf_text[15];
	struct net_device *dev;

	dev=card->dev;
	max_elements=BUFFER_MAX_ELEMENTS;

	buffer=&card->inbound_qdio_buffers[buffer_no];

	/* inform about errors */
	if (buffer->element[15].flags&0xff) {
		PRINT_WARN("on irq 0x%x: incoming SBALF 15 on buffer " \
			   "0x%x are 0x%x\n",card->irq0,buffer_no,
			   buffer->element[15].flags&0xff);
		sprintf(dbf_text,"SF##%2x%2x",buffer_no,
			buffer->element[15].flags&0xff);
		*((__u16*)(&dbf_text[2]))=(__u16)card->irq0;
		QETH_DBF_HEX1(1,trace,dbf_text,QETH_DBF_TRACE_LEN);
	}

	for (i=0;i<max_elements-1;i++) {
		if (buffer->element[i].flags&SBAL_FLAGS_LAST_ENTRY) {
			buffer->element[i+1].length=0;
			break;
		}
	}
#ifdef QETH_PERFORMANCE_STATS
	card->perf_stats.bufs_rec++;
#endif /* QETH_PERFORMANCE_STATS */

#ifdef QETH_DBF_LIKE_HELL
	sprintf(dbf_text,"ribX%4x",card->irq0);
	dbf_text[3]=buffer_no;
	QETH_DBF_HEX6(0,trace,dbf_text,QETH_DBF_TRACE_LEN);
#endif /* QETH_DBF_LIKE_HELL */

        while ((skb=qeth_get_next_skb(card,&element,&pos_in_el,
				      &hdr_ptr,buffer))) {

#ifdef QETH_PERFORMANCE_STATS
		card->perf_stats.skbs_rec++;
#endif /* QETH_PERFORMANCE_STATS */

		if (skb) {
			skb->dev=dev;

#ifdef QETH_IPV6
			if ( (*(__u16 *)(hdr_ptr))&(QETH_HEADER_PASSTHRU) ) {
				skb->protocol=card->type_trans(skb,dev);
			} else
#endif /* QETH_IPV6 */
			{
				version=((*(__u16 *)(hdr_ptr))&
					 (QETH_HEADER_IPV6))?6:4;
				skb->protocol=htons((version==4)?ETH_P_IP:
						    (version==6)?ETH_P_IPV6:
						    ETH_P_ALL);
				cast_type=(*(__u16 *)(hdr_ptr))&
					(QETH_CAST_FLAGS);
				if (cast_type==QETH_CAST_UNICAST) {
					skb->pkt_type=PACKET_HOST;
				} else if (cast_type==QETH_CAST_MULTICAST) {
					skb->pkt_type=PACKET_MULTICAST;
				} else if (cast_type==QETH_CAST_BROADCAST) {
					skb->pkt_type=PACKET_BROADCAST;
				} else if ( (cast_type==QETH_CAST_ANYCAST) ||
					    (cast_type==QETH_CAST_NOCAST) ) {
					sprintf(dbf_text,"ribf%4x",card->irq0);
					QETH_DBF_TEXT2(0,trace,dbf_text);
					sprintf(dbf_text,"castan%2x",cast_type);
					QETH_DBF_TEXT2(1,trace,dbf_text);
					skb->pkt_type=PACKET_HOST;
				} else {
					PRINT_WARN("adapter is using an " \
						   "unknown casting value " \
						   "of 0x%x. Using " \
						   "unicasting instead.\n",
						   cast_type);
					skb->pkt_type=PACKET_HOST;
					sprintf(dbf_text,"ribf%4x",card->irq0);
					QETH_DBF_TEXT2(0,trace,dbf_text);
					sprintf(dbf_text,"castun%2x",cast_type);
					QETH_DBF_TEXT2(1,trace,dbf_text);
				}

		if (card->options.fake_ll==FAKE_LL) {
			skb->mac.raw=skb->data-QETH_FAKE_LL_LEN;
			if (skb->pkt_type==PACKET_MULTICAST) {
				switch (skb->protocol) {
#ifdef QETH_IPV6
				case __constant_htons(ETH_P_IPV6):
					ndisc_mc_map((struct in6_addr *)
					skb->data+QETH_FAKE_LL_V6_ADDR_POS,
						skb->mac.raw+
				   		QETH_FAKE_LL_DEST_MAC_POS,
			   			card->dev,0);
					break;
#endif /* QETH_IPV6 */
				case __constant_htons(ETH_P_IP):
					qeth_get_mac_for_ipm(*(__u32*)
					skb->data+QETH_FAKE_LL_V4_ADDR_POS,
						skb->mac.raw+
				   		QETH_FAKE_LL_DEST_MAC_POS,
			   			card->dev);
					break;
				default:
					memcpy(skb->mac.raw+
					       QETH_FAKE_LL_DEST_MAC_POS,
					       card->dev->dev_addr,
					       QETH_FAKE_LL_ADDR_LEN);
				}
			} else if (skb->pkt_type==PACKET_BROADCAST) {
				memset(skb->mac.raw+
				       QETH_FAKE_LL_DEST_MAC_POS,0xff,
				       QETH_FAKE_LL_ADDR_LEN);
			} else {
				memcpy(skb->mac.raw+
				       QETH_FAKE_LL_DEST_MAC_POS,
				       card->dev->dev_addr,
				       QETH_FAKE_LL_ADDR_LEN);
			}
			if (*(__u8*)(hdr_ptr+11)&
			    QETH_EXT_HEADER_SRC_MAC_ADDRESS) {
				memcpy(skb->mac.raw+
				       QETH_FAKE_LL_SRC_MAC_POS,
			       hdr_ptr+QETH_FAKE_LL_SRC_MAC_POS_IN_QDIO_HDR,
				       QETH_FAKE_LL_ADDR_LEN);
			} else {
				/* clear source MAC for security reasons */
				memset(skb->mac.raw+
				       QETH_FAKE_LL_DEST_MAC_POS,0,
				       QETH_FAKE_LL_ADDR_LEN);
			}
			memcpy(skb->mac.raw+
			       QETH_FAKE_LL_PROT_POS,
			       &skb->protocol,
			       QETH_FAKE_LL_PROT_LEN);
		} else {
			skb->mac.raw=skb->data;
		}

		skb->ip_summed=card->options.checksum_type;
		if (card->options.checksum_type==HW_CHECKSUMMING) {
			/* do we have a checksummed packet? */
			if (*(__u8*)(hdr_ptr+11)&
			    QETH_EXT_HEADER_CSUM_TRANSP_REQ) {
				/* skb->ip_summed is set already */

				/* vlan is not an issue here, it's still in
				 * the QDIO header, not pushed in the
				 * skb yet */
				int ip_len=(skb->data[0]&0x0f)<<2;

				if (*(__u8*)(hdr_ptr+11)&
				    QETH_EXT_HEADER_CSUM_TRANSP_FRAME_TYPE) {
					/* get the UDP checksum */
					skb->csum=*(__u16*)
						(&skb->data[ip_len+
						 QETH_UDP_CSUM_OFFSET]);
				} else {
					/* get the TCP checksum */
					skb->csum=*(__u16*)
						(&skb->data[ip_len+
						 QETH_TCP_CSUM_OFFSET]);
				}
			} else {
				/* make the stack check it */
				skb->ip_summed=SW_CHECKSUMMING;
			}
		}

#ifdef QETH_VLAN
				if (*(__u8*)(hdr_ptr+11)&
				    QETH_EXT_HEADER_VLAN_FRAME) {
					vlan_tag=(__u16 *)skb_push(skb,
								   VLAN_HLEN);
					/*
					 if (*(__u8*)(hdr_ptr+11) & 
					     QETH_EXT_HEADER_INCLUDE_VLAN_TAG) {
				   	   *vlan_tag = *(__u16*)(hdr_ptr+28);
			  		   *(vlan_tag+1)= *(__u16*)(hdr_ptr+30);
			 		 } else {
					 */
				    	*vlan_tag = *(__u16*)(hdr_ptr+12);
			 		*(vlan_tag+1) = skb->protocol;
					/*
					   }
					 */
					skb->protocol=
						__constant_htons(ETH_P_8021Q);
				}
#endif				
			}

#ifdef QETH_PERFORMANCE_STATS
			card->perf_stats.inbound_time+=
				NOW-card->perf_stats.inbound_start_time;
			card->perf_stats.inbound_cnt++;
#endif /* QETH_PERFORMANCE_STATS */

#ifdef QETH_DBF_LIKE_HELL
			sprintf(dbf_text,"rxpk%4x",card->irq0);
			QETH_DBF_TEXT6(0,trace,dbf_text);
#endif /* QETH_DBF_LIKE_HELL */

			netif_rx(skb);

			card->stats->rx_packets++;
			card->stats->rx_bytes+=skb->len;
		} else {
			PRINT_WARN("%s: dropped packet, no buffers " \
				   "available.\n",card->dev_name);
			sprintf(dbf_text,"DROP%4x",card->irq0);
			QETH_DBF_TEXT2(1,trace,dbf_text);
			card->stats->rx_dropped++;
		}
        }
	atomic_set(&card->inbound_buffer_refcnt[buffer_no],0);
	qeth_put_buffer_pool_entry(card,card->inbound_buffer_entry_no[
				   buffer_no]);
}

static void qeth_fill_header(qeth_hdr_t *hdr,struct sk_buff *skb,
	       		     int version,int multicast)
{
#ifdef QETH_DBF_LIKE_HELL
	char dbf_text[15];
#endif /* QETH_DBF_LIKE_HELL */
#ifdef QETH_VLAN
	qeth_card_t *card;
#endif

	hdr->id=1;
	hdr->ext_flags=0;

#ifdef QETH_VLAN
        /* before we're going to overwrite 
	   this location with next hop ip 
	*/
	card = (qeth_card_t *)skb->dev->priv;
	if ((card->vlangrp != NULL) && 
	    (version == 4)          &&
	    vlan_tx_tag_present(skb)) 
	{
		hdr->ext_flags = QETH_EXT_HEADER_VLAN_FRAME;
		hdr->vlan_id  = vlan_tx_tag_get(skb);
	}
#endif

	hdr->length=skb->len-QETH_HEADER_SIZE; /* as skb->len includes
						  the header now */

	/* yes, I know this is doubled code, but a small little bit
	   faster maybe */
	if (version==4) { /* IPv4 */
		if (multicast==RTN_MULTICAST) {
			hdr->flags=QETH_CAST_MULTICAST;
		} else if (multicast==RTN_BROADCAST) {
			hdr->flags=QETH_CAST_BROADCAST;
		} else {
			hdr->flags=QETH_CAST_UNICAST;
		}
		*((__u32*)(&hdr->dest_addr[0]))=0;
		*((__u32*)(&hdr->dest_addr[4]))=0;
		*((__u32*)(&hdr->dest_addr[8]))=0;
		if ((skb->dst) && (skb->dst->neighbour)) {
		*((__u32*)(&hdr->dest_addr[12]))=
			       *((__u32*)skb->dst->neighbour->primary_key);
		} else {
			/* fill in destination address used
			 * in ip header */
			*((__u32*)(&hdr->dest_addr[12]))=
				skb->nh.iph->daddr;
		}
	} else if (version==6) { /* IPv6 or passthru */
		if (multicast==RTN_MULTICAST) {
			hdr->flags=QETH_CAST_MULTICAST|
				QETH_HEADER_PASSTHRU|
				QETH_HEADER_IPV6;
		} else if (multicast==RTN_ANYCAST) {
			hdr->flags=QETH_CAST_ANYCAST|
				QETH_HEADER_PASSTHRU|
				QETH_HEADER_IPV6;
		} else if (multicast==RTN_BROADCAST) {
			hdr->flags=QETH_CAST_BROADCAST|
				QETH_HEADER_PASSTHRU|
				QETH_HEADER_IPV6;
		} else { /* default: RTN_UNICAST */
			hdr->flags=QETH_CAST_UNICAST|
				QETH_HEADER_PASSTHRU|
				QETH_HEADER_IPV6;
		}

		if ((skb->dst) && (skb->dst->neighbour)) {
			memcpy(hdr->dest_addr,
			       skb->dst->neighbour->primary_key,16);
		} else {
			/* fill in destination address used
			 * in ip header */
			memcpy(hdr->dest_addr,
			       &skb->nh.ipv6h->daddr,16);
		}
	} else { /* passthrough */
		if (!memcmp(skb->data+QETH_HEADER_SIZE,
			    skb->dev->broadcast,6)) { /* broadcast? */
			hdr->flags=QETH_CAST_BROADCAST|QETH_HEADER_PASSTHRU;
		} else {
			hdr->flags=QETH_CAST_UNICAST|QETH_HEADER_PASSTHRU;
		}
	}
#ifdef QETH_DBF_LIKE_HELL
	sprintf(dbf_text,"filhdr%2x",version);
	QETH_DBF_TEXT6(0,trace,dbf_text);
	sprintf(dbf_text,"%2x",multicast);
	QETH_DBF_TEXT6(0,trace,dbf_text);
	QETH_DBF_HEX6(0,trace,&skb,sizeof(void*));
	QETH_DBF_HEX6(0,trace,&skb->data,sizeof(void*));
	QETH_DBF_HEX6(0,misc,hdr,__max(QETH_HEADER_SIZE,QETH_DBF_MISC_LEN));
	QETH_DBF_HEX6(0,data,skb->data,
		      __max(QETH_DBF_DATA_LEN,QETH_DBF_DATA_LEN));
#endif /* QETH_DBF_LIKE_HELL */
}

static int inline qeth_fill_buffer(qdio_buffer_t *buffer,char *dataptr,
		       		   int length,int element)
{
	int length_here;
	int first_lap=1;
#ifdef QETH_DBF_LIKE_HELL
	char dbf_text[15];
#endif /* QETH_DBF_LIKE_HELL */
	int first_element=element;

	while (length>0) {
		/* length_here is the remaining amount of data in this page */
		length_here=PAGE_SIZE-((unsigned long)dataptr&(PAGE_SIZE-1));
		if (length<length_here) length_here=length;

		buffer->element[element].addr=dataptr;
		buffer->element[element].length=length_here;
		length-=length_here;
		if (!length) {
			if (first_lap) {
				buffer->element[element].flags=0;
			} else {
				buffer->element[element].flags=
					SBAL_FLAGS_LAST_FRAG;
			}
		} else {
			if (first_lap) {
				buffer->element[element].flags=
					SBAL_FLAGS_FIRST_FRAG;
			} else {
				buffer->element[element].flags=
					SBAL_FLAGS_MIDDLE_FRAG;
			}
		}
		dataptr=dataptr+length_here;
		element++;
		if (element>QDIO_MAX_ELEMENTS_PER_BUFFER) {
			PRINT_ERR("qeth_fill_buffer: IP packet too big!\n");
			QETH_DBF_TEXT1(0,trace,"IPpktobg");
			QETH_DBF_HEX1(1,trace,&dataptr,sizeof(void*));
			buffer->element[first_element].length=0;
			break;
		}
		first_lap=0;
	}
#ifdef QETH_DBF_LIKE_HELL
	sprintf(dbf_text,"filbuf%2x",element);
	QETH_DBF_TEXT6(0,trace,dbf_text);
	QETH_DBF_HEX3(0,misc,buffer,QETH_DBF_MISC_LEN);
	QETH_DBF_HEX3(0,misc,buffer+QETH_DBF_MISC_LEN,QETH_DBF_MISC_LEN);
#endif /* QETH_DBF_LIKE_HELL */

	return element;
}

static void qeth_flush_packed_packets(qeth_card_t *card,int queue,
				      int under_int)
{
	qdio_buffer_t *buffer;
	int result;
	int position;
	int position_for_do_qdio;
	char dbf_text[15];
	int last_pci;

	position=card->outbound_first_free_buffer[queue];
	/* can happen, when in the time between deciding to pack and sending
	   the next packet the lower mark was reached: */
	if (!card->outbound_ringbuffer[queue]->ringbuf_element[position].
	    next_element_to_fill)
		return;

	buffer=&card->outbound_ringbuffer[queue]->buffer[position];
	buffer->element[card->outbound_ringbuffer[queue]->
		       ringbuf_element[position].
		       next_element_to_fill-1].flags|=SBAL_FLAGS_LAST_ENTRY;

	card->dev->trans_start=jiffies;
	
#ifdef QETH_PERFORMANCE_STATS
	if (card->outbound_buffer_send_state[queue][position]==
	    SEND_STATE_DONT_PACK) {
		card->perf_stats.bufs_sent_dont_pack++;
	} else if (card->outbound_buffer_send_state[queue][position]==
		   SEND_STATE_PACK) {
		card->perf_stats.bufs_sent_pack++;
	}
	card->perf_stats.bufs_sent++;
#endif /* QETH_PERFORMANCE_STATS */

	position_for_do_qdio=position;

	position=(position+1)&(QDIO_MAX_BUFFERS_PER_Q-1);
	card->outbound_first_free_buffer[queue]=position;

	card->outbound_bytes_in_buffer[queue]=0;
	/* we can override that, as we have at most 127 buffers enqueued */
	card->outbound_ringbuffer[queue]->ringbuf_element[position].
		next_element_to_fill=0;

	atomic_inc(&card->outbound_used_buffers[queue]);

#ifdef QETH_DBF_LIKE_HELL
	sprintf(dbf_text,"flsp%4x",card->irq0);
	QETH_DBF_TEXT5(0,trace,dbf_text);
	sprintf(dbf_text,"%4x%2x%2x",position_for_do_qdio,under_int,queue);
	QETH_DBF_TEXT5(0,trace,dbf_text);
	QETH_DBF_HEX5(0,misc,buffer,QETH_DBF_MISC_LEN);
	QETH_DBF_HEX5(0,misc,buffer+QETH_DBF_MISC_LEN,QETH_DBF_MISC_LEN);
#endif /* QETH_DBF_LIKE_HELL */

	/* we always set the outbound pci flag, don't care, whether the
	 * adapter honors it or not */
	switch (card->send_state[queue]) {
 	case SEND_STATE_DONT_PACK:
		if (atomic_read(&card->outbound_used_buffers[queue])
		    <HIGH_WATERMARK_PACK-WATERMARK_FUZZ) break;
		/* set the PCI bit */
		card->outbound_ringbuffer[queue]->
			buffer[position_for_do_qdio].element[0].flags|=0x40;
		atomic_set(&card->last_pci_pos[queue],position_for_do_qdio);
		break;
	case SEND_STATE_PACK:
		last_pci=atomic_read(&card->last_pci_pos[queue]);
		if (position_for_do_qdio<last_pci)
			last_pci-=QDIO_MAX_BUFFERS_PER_Q;
		/* so:
		 * last_pci is the position of the last pci we've set
		 * position_for_do_qdio is the position we will send out now
		 * outbound_used_buffers is the number of buffers used (means
		 *   all buffers hydra has, inclusive position_for_do_qdio)
		 *
		 * we have to request a pci, if we have got the buffer of the
		 * last_pci position back.
		 *
		 * position_for_do_qdio-outbound_used_buffers is the newest
		 *   buffer that we got back from hydra
		 *
		 * if this is greater or equal than the last_pci position,
		 * we should request a pci, as no pci request is
		 * outstanding anymore
		 */
		if (position_for_do_qdio-
		    atomic_read(&card->outbound_used_buffers[queue])>=
		    last_pci) {
			/* set the PCI bit */
			card->outbound_ringbuffer[queue]->
				buffer[position_for_do_qdio].
				element[0].flags|=0x40;
			atomic_set(&card->last_pci_pos[queue],
				   position_for_do_qdio);
		}
	}

	qeth_transform_outbound_addrs(card,
				      &card->outbound_ringbuffer[queue]->
				      buffer[position_for_do_qdio]);
	/* this has to be at the end, otherwise a buffer could be flushed
	   twice (see coment in qeth_do_send_packet) */
	result=do_QDIO(card->irq2,QDIO_FLAG_SYNC_OUTPUT|under_int,queue,
		       position_for_do_qdio,1,
				       NULL);

	if (result) {
		PRINT_WARN("Outbound do_QDIO returned %i " \
	    		   "(irq 0x%x)\n",result,card->irq2);
		sprintf(dbf_text,"FLSP%4x",card->irq0);
		QETH_DBF_TEXT5(0,trace,dbf_text);
		sprintf(dbf_text,"odoQ%4x",result);
		QETH_DBF_TEXT5(0,trace,dbf_text);
		sprintf(dbf_text,"%4x%2x%2x",position_for_do_qdio,
			under_int,queue);
		QETH_DBF_TEXT5(0,trace,dbf_text);
		QETH_DBF_HEX5(0,misc,buffer,QETH_DBF_MISC_LEN);
		QETH_DBF_HEX5(0,misc,buffer+QETH_DBF_MISC_LEN,
			      QETH_DBF_MISC_LEN);
	}
}

#define ERROR_NONE 0
#define ERROR_RETRY 1
#define ERROR_LINK_FAILURE 2
#define ERROR_KICK_THAT_PUPPY 3
static inline int qeth_determine_send_error(int cc,int qdio_error,int sbalf15)
{
	char dbf_text[15];

	switch (cc&3) {
	case 0:
		if (qdio_error)
			return ERROR_LINK_FAILURE;
		return ERROR_NONE;
	case 2:
		if (cc&QDIO_SIGA_ERROR_B_BIT_SET) {
			QETH_DBF_TEXT3(0,trace,"sigacc2b");
			return ERROR_KICK_THAT_PUPPY;
		}
		if (qeth_sbalf15_in_retrieable_range(sbalf15))
			return ERROR_RETRY;
		return ERROR_LINK_FAILURE;
		/* look at qdio_error and sbalf 15 */
	case 1:
		PRINT_WARN("siga returned cc 1! cc=0x%x, " \
			   "qdio_error=0x%x, sbalf15=0x%x\n",
			   cc,qdio_error,sbalf15);

		QETH_DBF_TEXT3(0,trace,"siga-cc1");
		QETH_DBF_TEXT2(0,qerr,"siga-cc1");
		sprintf(dbf_text,"%1x%2x%2x",cc,qdio_error,sbalf15);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		QETH_DBF_TEXT2(0,qerr,dbf_text);
		return ERROR_LINK_FAILURE;
	case 3:
		QETH_DBF_TEXT3(0,trace,"siga-cc3");
		return ERROR_KICK_THAT_PUPPY;
	}
	return ERROR_LINK_FAILURE; /* should never happen */
}

static void qeth_free_buffer(qeth_card_t *card,int queue,int bufno,
	       		     int qdio_error,int siga_error)
{
	struct sk_buff *skb;
	int error;
	int retries;
	int sbalf15;
	char dbf_text[15];
	qdio_buffer_t *buffer;

	switch (card->outbound_buffer_send_state[queue][bufno]) {
	case SEND_STATE_DONT_PACK: /* fallthrough */
	case SEND_STATE_PACK:
#ifdef QETH_DBF_LIKE_HELL
		sprintf(dbf_text,"frbf%4x",card->irq0);
		QETH_DBF_TEXT5(0,trace,dbf_text);
		sprintf(dbf_text,"%2x%2x%4x",queue,bufno,
			card->outbound_buffer_send_state[queue][bufno]);
		QETH_DBF_TEXT5(0,trace,dbf_text);
#endif /* QETH_DBF_LIKE_HELL */

		buffer=&card->outbound_ringbuffer[queue]->buffer[bufno];
		sbalf15=buffer->element[15].flags&0xff;
		error=qeth_determine_send_error(siga_error,qdio_error,sbalf15);
		if (error==ERROR_KICK_THAT_PUPPY) {
			sprintf(dbf_text,"KP%4x%2x",card->irq0,queue);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			QETH_DBF_TEXT2(0,qerr,dbf_text);
			QETH_DBF_TEXT2(1,setup,dbf_text);
			sprintf(dbf_text,"%2x%2x%2x%2x",bufno,
				siga_error,qdio_error,sbalf15);
			QETH_DBF_TEXT2(1,trace,dbf_text);
			QETH_DBF_TEXT2(1,qerr,dbf_text);
			PRINT_ERR("Outbound queue x%x on irq x%x (%s); " \
				  "errs: siga: x%x, qdio: x%x, flags15: " \
				  "x%x. The device will be taken down.\n",
				  queue,card->irq0,card->dev_name,
				  siga_error,qdio_error,sbalf15);
			netif_stop_queue(card->dev);
			qeth_set_dev_flag_norunning(card);
			atomic_set(&card->problem,PROBLEM_BAD_SIGA_RESULT);
			qeth_schedule_recovery(card);
		} else if (error==ERROR_RETRY) {
			/* analyze, how many retries we did so far */
			retries=card->send_retries[queue][bufno];

			sprintf(dbf_text,"Rt%4x%2x",card->irq0,queue);
			QETH_DBF_TEXT4(0,trace,dbf_text);
			sprintf(dbf_text,"b%2x:%2x%2x",bufno,
				sbalf15,retries);
			QETH_DBF_TEXT4(0,trace,dbf_text);

			if (++retries>SEND_RETRIES_ALLOWED) {
				error=ERROR_LINK_FAILURE;
				QETH_DBF_TEXT4(1,trace,"ndegelnd");
			}
			/* else error stays RETRY for the switch statemnet */
		} else if (error==ERROR_LINK_FAILURE) {
			/* we don't want to log failures resulting from
			 * too many retries */
			sprintf(dbf_text,"Fail%4x",card->irq0);
			QETH_DBF_TEXT3(1,trace,dbf_text);
			QETH_DBF_HEX3(0,misc,buffer,QETH_DBF_MISC_LEN);
			QETH_DBF_HEX3(0,misc,buffer+QETH_DBF_MISC_LEN,
				      QETH_DBF_MISC_LEN);
		}

		while ((skb=skb_dequeue(&card->outbound_ringbuffer[queue]->
					ringbuf_element[bufno].skb_list))) {
			switch (error) {
			case ERROR_NONE:
				atomic_dec(&skb->users);
				dev_kfree_skb_irq(skb);
				break;
			case ERROR_RETRY:
				QETH_DBF_TEXT3(0,qerr,"RETRY!!!");
				QETH_DBF_TEXT4(0,trace,"RETRY!!!");
				/* retry packet async (quickly) ... */
				atomic_dec(&skb->users);
				dev_kfree_skb_irq(skb);
				break;
			case ERROR_LINK_FAILURE:
			case ERROR_KICK_THAT_PUPPY:
				QETH_DBF_TEXT4(0,trace,"endeglnd");
				dst_link_failure(skb);
				atomic_dec(&skb->users);
				dev_kfree_skb_irq(skb);
				break;
			}
		}
		break;
	default:
		PRINT_WARN("oops... wrong send_state on %s. " \
			   "shouldn't happen " \
			   "(line %i). q=%i, bufno=x%x, state=%i\n",
			   card->dev_name,__LINE__,queue,bufno,
			   card->outbound_buffer_send_state[queue][bufno]);
		sprintf(dbf_text,"UPSf%4x",card->irq0);
		QETH_DBF_TEXT0(1,trace,dbf_text);
		QETH_DBF_TEXT0(1,qerr,dbf_text);
		sprintf(dbf_text,"%2x%2x%4x",queue,bufno,
			card->outbound_buffer_send_state[queue][bufno]);
		QETH_DBF_TEXT0(1,trace,dbf_text);
		QETH_DBF_TEXT0(1,qerr,dbf_text);
	}
	card->outbound_buffer_send_state[queue][bufno]=SEND_STATE_INACTIVE;
	card->send_retries[queue][bufno]=0;
}

static void qeth_free_all_skbs(qeth_card_t *card)
{
	int q,b;

	for (q=0;q<card->no_queues;q++)
		for (b=0;b<QDIO_MAX_BUFFERS_PER_Q;b++)
			if (card->outbound_buffer_send_state[q][b]!=
			    SEND_STATE_INACTIVE)
				qeth_free_buffer(card,q,b,0,0);
}

static inline void qeth_flush_buffer(qeth_card_t *card,int queue,
				     int under_int)
{
#ifdef QETH_DBF_LIKE_HELL
	char dbf_text[15];
	sprintf(dbf_text,"flsb%4x",card->irq0);
	QETH_DBF_TEXT5(0,trace,dbf_text);
	sprintf(dbf_text,"%2x%2x%2x",queue,under_int,
		card->outbound_buffer_send_state[queue][
		card->outbound_first_free_buffer[queue] ]);
	QETH_DBF_TEXT5(0,trace,dbf_text);
#endif /* QETH_DBF_LIKE_HELL */

	switch (card->outbound_buffer_send_state[queue][
		card->outbound_first_free_buffer[queue] ]) {
	case SEND_STATE_DONT_PACK: break;
	case SEND_STATE_PACK:
		qeth_flush_packed_packets(card,queue,under_int);
		break;
	default:break;
	}
}
#ifdef QETH_VLAN

void qeth_insert_ipv6_vlan_tag(struct sk_buff *__skb)
{

	/* Move the mac addresses to the beginning of the new header.
         * We are using three memcpys instead of one memmove to save
         * cycles.
         */
#define TMP_CPYSIZE 4
        __u16 *tag;
        tag = (__u16*)skb_push(__skb, VLAN_HLEN);
        memcpy(__skb->data,
	       __skb->data+TMP_CPYSIZE,TMP_CPYSIZE);
        memcpy(__skb->data+TMP_CPYSIZE,
               __skb->data+(2*TMP_CPYSIZE),TMP_CPYSIZE);
        memcpy(__skb->data+(2*TMP_CPYSIZE),
	       __skb->data+(3*TMP_CPYSIZE),TMP_CPYSIZE);
        tag = (__u16*)(__skb->data+(3*TMP_CPYSIZE));

        /*first two bytes  = ETH_P_8021Q (0x8100)
         *second two bytes = VLANID
         */

	*tag =  __constant_htons(ETH_P_8021Q);
	*(tag+1) = vlan_tx_tag_get(__skb);
        *(tag+1)=htons(*(tag+1));
#undef TMP_CPYSIZE
}
#endif



static void qeth_send_packet_fast(qeth_card_t *card,struct sk_buff *skb,
				  struct net_device *dev,
				  int queue,int version,int multicast)
{
	qeth_ringbuffer_element_t *mybuffer;
	int position;
	qeth_hdr_t *hdr;
	char *dataptr;
	char dbf_text[15];
	struct sk_buff *nskb;

	position=card->outbound_first_free_buffer[queue];

	card->outbound_buffer_send_state[queue][position]=SEND_STATE_DONT_PACK;

	mybuffer=&card->outbound_ringbuffer[queue]->ringbuf_element[position];
	if (skb_headroom(skb)<QETH_HEADER_SIZE) {
		if ((version)&&(!card->realloc_message)) {
			card->realloc_message=1;
			PRINT_WARN("%s: not enough headroom in skb. " \
				   "Try increasing the " \
				   "add_hhlen parameter by %i.\n",
				   card->dev_name,
				   QETH_HEADER_SIZE-skb_headroom(skb));
		}
		PRINT_STUPID("%s: not enough headroom in skb (missing: %i)\n",
			     card->dev_name,QETH_HEADER_SIZE-skb_headroom(skb));
		sprintf(dbf_text,"NHRf%4x",card->irq0);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		sprintf(dbf_text,"%2x%2x%2x%2x",skb_headroom(skb),
			version,multicast,queue);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		QETH_DBF_HEX3(0,trace,&skb->head,sizeof(void*));
		QETH_DBF_HEX3(0,trace,&skb->data,sizeof(void*));
		nskb=skb_realloc_headroom(skb,QETH_HEADER_SIZE);
		if (!nskb) {
			PRINT_WARN("%s: could not realloc headroom\n",
				   card->dev_name);
			sprintf(dbf_text,"CNRf%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			dev_kfree_skb_irq(skb);
			return;
		}
		dev_kfree_skb_irq(skb);
		skb=nskb;
	}
#ifdef QETH_VLAN
	if ( (card->vlangrp != NULL) &&
             vlan_tx_tag_present(skb) &&
             (version==6)) {
                qeth_insert_ipv6_vlan_tag(skb);
        }
#endif
		hdr=(qeth_hdr_t*)(skb_push(skb,QETH_HEADER_SIZE));
	/* sanity check, the Linux memory allocation scheme should
	   never present us cases like this one (the 32bytes header plus
	   the first 40 bytes of the paket cross a 4k boundary) */
	dataptr=(char*)hdr;
	if ( (((unsigned long)dataptr)&(~(PAGE_SIZE-1))) !=
	     ( ((unsigned long)dataptr+QETH_HEADER_SIZE+QETH_IP_HEADER_SIZE)&
	       (~(PAGE_SIZE-1)) ) ) {
		PRINT_ERR("%s: packet misaligned -- the first %i bytes " \
			  "are not in the same page. Discarding packet!\n",
			  card->dev_name,
			  QETH_HEADER_SIZE+QETH_IP_HEADER_SIZE);
		PRINT_ERR("head=%p, data=%p\n",skb->head,skb->data);
		sprintf(dbf_text,"PMAf%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		sprintf(dbf_text,"%2x%2x%2x%2x",skb_headroom(skb),
			version,multicast,queue);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_HEX1(0,trace,&skb->head,sizeof(void*));
		QETH_DBF_HEX1(1,trace,&skb->data,sizeof(void*));
		dev_kfree_skb_irq(skb);
		return;
	}

	atomic_inc(&skb->users);
	skb_queue_tail(&mybuffer->skb_list,skb);
	qeth_fill_header(hdr,skb,version,multicast);
	/* we need to write to next_element_to_fill as
	   qeth_flush_packed_packets checks it */
	card->outbound_ringbuffer[queue]->ringbuf_element[position].
		next_element_to_fill=
		qeth_fill_buffer(&card->outbound_ringbuffer[queue]->
				 buffer[position],(char *)hdr,
				 skb->len,0);

#ifdef QETH_PERFORMANCE_STATS
	card->perf_stats.skbs_sent_dont_pack++;
#endif /* QETH_PERFORMANCE_STATS */

	qeth_flush_packed_packets(card,queue,0);
}

/* no checks, if all elements are used, as then we would not be here (at most
   127 buffers are enqueued) */
static void qeth_send_packet_packed(qeth_card_t *card,struct sk_buff *skb,
       				    struct net_device *dev,
       				    int queue,int version,int multicast)
{
	qeth_ringbuffer_element_t *mybuffer;
	int elements_needed;
	int element_to_fill;
	int buffer_no;
	int length;
	char *dataptr;
	qeth_hdr_t *hdr;
	char dbf_text[15];
	struct sk_buff *nskb;

	/* sanity check, dev->hard_header_len should prevent this */
	if (skb_headroom(skb)<QETH_HEADER_SIZE) {
		if ((version)&&(!card->realloc_message)) {
			card->realloc_message=1;
			PRINT_WARN("%s: not enough headroom in skb. " \
				   "Try increasing the " \
				   "add_hhlen parameter by %i.\n",
				   card->dev_name,
				   QETH_HEADER_SIZE-skb_headroom(skb));
		}
		PRINT_STUPID("%s: not enough headroom in skb (missing: %i)\n",
			     card->dev_name,QETH_HEADER_SIZE-skb_headroom(skb));
		sprintf(dbf_text,"NHRp%4x",card->irq0);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		sprintf(dbf_text,"%2x%2x%2x%2x",skb_headroom(skb),
			version,multicast,queue);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		QETH_DBF_HEX3(0,trace,&skb->head,sizeof(void*));
		QETH_DBF_HEX3(0,trace,&skb->data,sizeof(void*));
		nskb=skb_realloc_headroom(skb,QETH_HEADER_SIZE);
		if (!nskb) {
			PRINT_WARN("%s: could not realloc headroom\n",
				   card->dev_name);
			sprintf(dbf_text,"CNRp%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			dev_kfree_skb_irq(skb);
			return;
		}
		dev_kfree_skb_irq(skb);
		skb=nskb;
	}
#ifdef QETH_VLAN
        if ( (card->vlangrp != NULL) &&
             vlan_tx_tag_present(skb) &&
             (version==6)) {
                qeth_insert_ipv6_vlan_tag(skb);
        }

#endif
	hdr=(qeth_hdr_t*)(skb_push(skb,QETH_HEADER_SIZE));

	length=skb->len;

	/* sanity check, the Linux memory allocation scheme should
	   never present us cases like this one (the 32bytes header plus
	   the first 40 bytes of the paket cross a 4k boundary) */
	dataptr=(char*)hdr;
	if ( (((unsigned long)dataptr)&(~(PAGE_SIZE-1))) !=
	     ( ((unsigned long)dataptr+QETH_HEADER_SIZE+QETH_IP_HEADER_SIZE)&
	       (~(PAGE_SIZE-1)) ) ) {
		PRINT_ERR("%s: packet misaligned -- the first %i bytes " \
			  "are not in the same page. Discarding packet!\n",
			  card->dev_name,
			  QETH_HEADER_SIZE+QETH_IP_HEADER_SIZE);
		sprintf(dbf_text,"PMAp%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		sprintf(dbf_text,"%2x%2x%2x%2x",skb_headroom(skb),
			version,multicast,queue);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_HEX1(0,trace,&skb->head,sizeof(void*));
		QETH_DBF_HEX1(1,trace,&skb->data,sizeof(void*));
		dev_kfree_skb_irq(skb);
		return;
	}

	buffer_no=card->outbound_first_free_buffer[queue];

	element_to_fill=card->outbound_ringbuffer[queue]->
		ringbuf_element[buffer_no].
		next_element_to_fill;

	elements_needed=1+( ( (((unsigned long)dataptr)&(PAGE_SIZE-1))+
			      length ) >>PAGE_SHIFT);
	if ( (elements_needed>(QDIO_MAX_ELEMENTS_PER_BUFFER-element_to_fill)) ||
	     ( (elements_needed==
		(QDIO_MAX_ELEMENTS_PER_BUFFER-element_to_fill)) &&
		((element_to_fill>>PAGE_SHIFT)==
		 card->outbound_bytes_in_buffer[queue]) ) ) {
		qeth_flush_packed_packets(card,queue,0);
		element_to_fill=0;
		card->outbound_bytes_in_buffer[queue]=0;
		buffer_no=(buffer_no+1)&(QDIO_MAX_BUFFERS_PER_Q-1);
	}

	if (!element_to_fill)
 		card->outbound_buffer_send_state[queue][buffer_no]
			=SEND_STATE_PACK;

#ifdef QETH_PERFORMANCE_STATS
	card->perf_stats.skbs_sent_pack++;
#endif /* QETH_PERFORMANCE_STATS */

	mybuffer=&card->outbound_ringbuffer[queue]->ringbuf_element[buffer_no];
	atomic_inc(&skb->users);
	skb_queue_tail(&mybuffer->skb_list,skb);
	qeth_fill_header(hdr,skb,version,multicast);
	card->outbound_bytes_in_buffer[queue]+=length+QETH_HEADER_SIZE;
	card->outbound_ringbuffer[queue]->ringbuf_element[buffer_no].
		next_element_to_fill=
		qeth_fill_buffer(&card->outbound_ringbuffer[queue]->
				 buffer[buffer_no],
				 dataptr,length,element_to_fill);
}

static void qeth_alloc_spare_bufs(void)
{
	int i;
	int dont_alloc_more=0;
	char dbf_text[15];

	sparebuffer_count=0;
	for (i=0;i<qeth_sparebufs;i++) {
		if (!dont_alloc_more) {
			sparebufs[i].buf=(char*)
				kmalloc(DEFAULT_BUFFER_SIZE,GFP_KERNEL);
			if (sparebufs[i].buf)
				sparebuffer_count++;
			else
				dont_alloc_more=1;
		}
		atomic_set(&sparebufs[i].status,(dont_alloc_more)?
			   SPAREBUF_UNAVAIL:SPAREBUF_FREE);
	}
	sprintf(dbf_text,"alspb%3x",sparebuffer_count);
	QETH_DBF_TEXT2(0,trace,dbf_text);

	PRINT_INFO("allocated %i spare buffers\n",sparebuffer_count);
}

static void qeth_free_all_spare_bufs(void)
{
	int i;

	QETH_DBF_TEXT2(0,trace,"frealspb");

	for (i=0;i<qeth_sparebufs;i++)
		if (atomic_read(&sparebufs[i].status)!=SPAREBUF_UNAVAIL) {
			kfree(sparebufs[i].buf);
			atomic_set(&sparebufs[i].status,SPAREBUF_UNAVAIL);
		}
}

static __inline__ int atomic_return_sub(int i, atomic_t *v)
{
	int old_val, new_val;
	__CS_LOOP(old_val, new_val, v, i, "sr");
	return old_val;
}

static int qeth_do_send_packet(qeth_card_t *card,struct sk_buff *skb,
       			       struct net_device *dev)
{
        int queue,result=0;
	int multicast,version;
	char dbf_text[15];
	char dbf_text2[15]="stchupXX";

	version=QETH_IP_VERSION(skb);
	multicast=qeth_is_multicast_skb_at_all(skb,version);
        queue=qeth_get_prioqueue(card,skb,multicast,version);

#ifdef QETH_DBF_LIKE_HELL
	sprintf(dbf_text,"dsp:%4x",card->irq0);
	QETH_DBF_TEXT6(0,trace,dbf_text);
	sprintf(dbf_text,"%c %c%4x",(version==4)?'4':((version==6)?'6':'0'),
		(multicast)?'m':'_',queue);
	QETH_DBF_TEXT6(0,trace,dbf_text);
	sprintf(dbf_text,"%4x%4x",
		card->outbound_first_free_buffer[queue],
		atomic_read(&card->outbound_used_buffers[queue]));
	QETH_DBF_TEXT6(0,trace,dbf_text);
	if (qeth_sbal_packing_on_card(card->type)) {
		switch (card->send_state[queue]) {
		case SEND_STATE_DONT_PACK:
			QETH_DBF_TEXT6(0,trace,"usngfast");
			break;
		case SEND_STATE_PACK:
			QETH_DBF_TEXT6(0,trace,"usngpack");
			break;
		}
	} else {
		QETH_DBF_TEXT6(0,trace,"usngfast");
	}
#endif /* QETH_DBF_LIKE_HELL */

	if (atomic_read(&card->outbound_used_buffers[queue])
	    >=QDIO_MAX_BUFFERS_PER_Q-1) {
		sprintf(dbf_text,"cdbs%4x",card->irq0);
		QETH_DBF_TEXT2(1,trace,dbf_text);
		netif_stop_queue(dev);
		return -EBUSY;
	}
	
	/* we are not called under int, so we just spin */
	/* happens around once a second under heavy traffic. takes a little
	 * bit less than 10usec in avg. on a z900 */
	if (atomic_compare_and_swap(QETH_LOCK_UNLOCKED,QETH_LOCK_NORMAL,
			    	    &card->outbound_ringbuffer_lock[queue])) {
		sprintf(dbf_text,"SPIN%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		while (atomic_compare_and_swap
		       (QETH_LOCK_UNLOCKED,QETH_LOCK_NORMAL,
    			&card->outbound_ringbuffer_lock[queue]))
			;
		sprintf(dbf_text,"spin%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
	}

#ifdef QETH_PERFORMANCE_STATS
	card->perf_stats.skbs_sent++;
#endif /* QETH_PERFORMANCE_STATS */
	
	if (qeth_sbal_packing_on_card(card->type)) {
		switch (card->send_state[queue]) {
		case SEND_STATE_DONT_PACK:
			qeth_send_packet_fast(card,skb,dev,queue,
					      version,multicast);
			if (atomic_read(&card->outbound_used_buffers[queue])
			    >=HIGH_WATERMARK_PACK) {
				card->send_state[queue]=SEND_STATE_PACK;
				*((__u16*)(&dbf_text2[6]))=card->irq0;
				QETH_DBF_HEX3(0,trace,dbf_text2,
					      QETH_DBF_TRACE_LEN);
#ifdef QETH_PERFORMANCE_STATS
				card->perf_stats.sc_dp_p++;
#endif /* QETH_PERFORMANCE_STATS */
			}
			break;
		case SEND_STATE_PACK:
			qeth_send_packet_packed(card,skb,dev,queue,
						version,multicast);
			break;
		default:
			result=-EBUSY;
			sprintf(dbf_text,"UPSs%4x",card->irq0);
			QETH_DBF_TEXT0(1,trace,dbf_text);
			PRINT_ALL("oops... shouldn't happen (line %i:%i).\n",
				  __LINE__,card->send_state[queue]);
		}
	} else {
		qeth_send_packet_fast(card,skb,dev,queue,
				      version,multicast);
	}

again:
	/* ATOMIC: (NORMAL->UNLOCKED, FLUSH->NORMAL) */
	if (atomic_dec_return(&card->outbound_ringbuffer_lock[queue])) {
		qeth_flush_buffer(card,queue,0);
		card->send_state[queue]=SEND_STATE_DONT_PACK;
		goto again;
	}

#ifdef QETH_PERFORMANCE_STATS
	card->perf_stats.outbound_time+=
		NOW-card->perf_stats.outbound_start_time;
	card->perf_stats.outbound_cnt++;
#endif /* QETH_PERFORMANCE_STATS */

	card->stats->tx_packets++;
	card->stats->tx_bytes+=skb->len;

	return result;
}

static int qeth_hard_start_xmit(struct sk_buff *skb,struct net_device *dev)
{
        qeth_card_t *card;
	char dbf_text[15];
	int result;

        card=(qeth_card_t*)(dev->priv);

        if (skb==NULL)
		return 0;

#ifdef QETH_DBF_LIKE_HELL
	QETH_DBF_HEX4(0,data,skb->data,__max(QETH_DBF_DATA_LEN,skb->len));
#endif /* QETH_DBF_LIKE_HELL */

	netif_stop_queue(dev);

	if (!card) {
		QETH_DBF_TEXT2(0,trace,"XMNSNOCD");
		dst_link_failure(skb);
		dev_kfree_skb_irq(skb);
		return 0;
	}

#ifdef QETH_PERFORMANCE_STATS
	card->perf_stats.outbound_start_time=NOW;
#endif /* QETH_PERFORMANCE_STATS */

	if (!atomic_read(&card->is_startlaned)) {
		card->stats->tx_carrier_errors++;
		sprintf(dbf_text,"XMNS%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		dst_link_failure(skb);
		dev_kfree_skb_irq(skb);
		return 0;
	}

	result=qeth_do_send_packet(card,skb,dev);

	if (!result)
		netif_wake_queue(card->dev);

	return result;
}

static struct net_device_stats* qeth_get_stats(struct net_device *dev)
{
        qeth_card_t *card;
	char dbf_text[15];

        card=(qeth_card_t*)(dev->priv);

	sprintf(dbf_text,"gtst%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	return card->stats;
}

static int qeth_change_mtu(struct net_device *dev,int new_mtu)
{
        qeth_card_t *card;
	char dbf_text[15];

        card=(qeth_card_t*)(dev->priv);

	sprintf(dbf_text,"mtu %4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	sprintf(dbf_text,"%8x",new_mtu);
	QETH_DBF_TEXT2(0,trace,dbf_text);

        if (new_mtu<64) return -EINVAL;
        if (new_mtu>65535) return -EINVAL;
        if ((!qeth_is_supported(IPA_IP_FRAGMENTATION)) &&
            (!qeth_mtu_is_valid(card,new_mtu)))
	    return -EINVAL;
	dev->mtu=new_mtu;
	return 0;
}

static void qeth_start_softsetup_thread(qeth_card_t *card)
{
	char dbf_text[15];
	if (!atomic_read(&card->shutdown_phase)) {
		sprintf(dbf_text,"stss%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		up(&card->softsetup_thread_sem);
	}
}

static int qeth_sleepon(qeth_card_t *card,int timeout)
{
	unsigned long flags;
        unsigned long start;
	int retval;
	char dbf_text[15];

        DECLARE_WAITQUEUE (current_wait_q,current);

	sprintf(dbf_text,"slpn%4x",card->irq0);
	QETH_DBF_TEXT5(0,trace,dbf_text);
	sprintf(dbf_text,"%08x",timeout);
	QETH_DBF_TEXT5(0,trace,dbf_text);

	add_wait_queue(&card->wait_q,&current_wait_q);
	atomic_set(&card->wait_q_active,1);
        start=qeth_get_millis();
        for (;;) {
                set_task_state(current,TASK_INTERRUPTIBLE);
                if (atomic_read(&card->data_has_arrived)) {
			atomic_set(&card->data_has_arrived,0);
			retval=0;
			goto out;
                }
                if (qeth_get_millis()-start>timeout) {
			retval=-ETIME;
			goto out;
		}
                schedule_timeout(((start+timeout-qeth_get_millis())>>10)*HZ);
        }
 out:
	spin_lock_irqsave(&card->wait_q_lock,flags);
	atomic_set(&card->wait_q_active,0);
	spin_unlock_irqrestore(&card->wait_q_lock,flags);

	/* we've got to check once again to close the window */
	if (atomic_read(&card->data_has_arrived)) {
		atomic_set(&card->data_has_arrived,0);
		retval=0;
	}

	set_task_state(current,TASK_RUNNING);
	remove_wait_queue(&card->wait_q,&current_wait_q);
	
	return retval;
}

static void qeth_wakeup_ioctl(qeth_card_t *card) {
	char dbf_text[15];

	sprintf(dbf_text,"wkup%4x",card->irq0);
	QETH_DBF_TEXT5(0,trace,dbf_text);

        atomic_set(&card->ioctl_data_has_arrived,1);
	spin_lock(&card->ioctl_wait_q_lock);
	if (atomic_read(&card->ioctl_wait_q_active)) {
		wake_up(&card->ioctl_wait_q);
	}
	spin_unlock(&card->ioctl_wait_q_lock);
}

static int qeth_sleepon_ioctl(qeth_card_t *card,int timeout)
{
	unsigned long flags;
        unsigned long start;
	int retval;
	char dbf_text[15];

        DECLARE_WAITQUEUE (current_wait_q,current);

	sprintf(dbf_text,"ioctlslpn%4x",card->irq0);
	QETH_DBF_TEXT5(0,trace,dbf_text);
	sprintf(dbf_text,"%08x",timeout);
	QETH_DBF_TEXT5(0,trace,dbf_text);

	save_flags(flags); 
	add_wait_queue(&card->ioctl_wait_q,&current_wait_q);
	atomic_set(&card->ioctl_wait_q_active,1);
        start=qeth_get_millis();
        for (;;) {
                set_task_state(current,TASK_INTERRUPTIBLE);
                if (atomic_read(&card->ioctl_data_has_arrived)) {
			atomic_set(&card->ioctl_data_has_arrived,0);
			retval=0;
			goto out;
                }
                if (qeth_get_millis()-start>timeout) {
			retval=-ETIME;
			goto out;
		}
                schedule_timeout(((start+timeout-qeth_get_millis())>>10)*HZ);
        }
 out:
	spin_lock_irqsave(&card->ioctl_wait_q_lock,flags);
	atomic_set(&card->ioctl_wait_q_active,0);
	spin_unlock_irqrestore(&card->ioctl_wait_q_lock,flags);

	/* we've got to check once again to close the window */
	if (atomic_read(&card->ioctl_data_has_arrived)) {
		atomic_set(&card->ioctl_data_has_arrived,0);
		retval=0;
	}

	set_task_state(current,TASK_RUNNING);
	remove_wait_queue(&card->ioctl_wait_q,&current_wait_q);
	
	return retval;
}

static void qeth_wakeup_procfile(void) 
{
	QETH_DBF_TEXT5(0,trace,"procwkup");
	if (atomic_read(&qeth_procfile_ioctl_sem.count)<
	    PROCFILE_SLEEP_SEM_MAX_VALUE)
  		up(&qeth_procfile_ioctl_sem);
}


static int qeth_sleepon_procfile(void)
{
	QETH_DBF_TEXT5(0,trace,"procslp");
	if (down_interruptible(&qeth_procfile_ioctl_sem)) {
      		up(&qeth_procfile_ioctl_sem);
      		return -ERESTARTSYS;
	}
	return 0;
}

static char* qeth_send_control_data(qeth_card_t *card,unsigned char *buffer,
				    int len,unsigned long intparam)
{
        unsigned long flags;
        int result,result2;
	char dbf_text[15];
	unsigned char *rec_buf;
	int setip=(intparam&IPA_SETIP_FLAG)?1:0;

again:
	if (atomic_read(&card->shutdown_phase)==
	    QETH_REMOVE_CARD_QUICK) return NULL;
	if (atomic_read(&card->escape_softsetup))
		return NULL;

	/* we lock very early to synchronize access to seqnos */
	if (atomic_swap(&card->write_busy,1)) {
		qeth_wait_nonbusy(QETH_IDLE_WAIT_TIME);
		sprintf(dbf_text,"LSCD%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		goto again;
	}
	memcpy(card->dma_stuff->sendbuf,card->send_buf,QETH_BUFSIZE);

        memcpy(QETH_TRANSPORT_HEADER_SEQ_NO(buffer),
               &card->seqno.trans_hdr,QETH_SEQ_NO_LENGTH);
        card->seqno.trans_hdr++;

        memcpy(QETH_PDU_HEADER_SEQ_NO(buffer),
               &card->seqno.pdu_hdr,QETH_SEQ_NO_LENGTH);
        card->seqno.pdu_hdr++;
        memcpy(QETH_PDU_HEADER_ACK_SEQ_NO(buffer),
               &card->seqno.pdu_hdr_ack,QETH_SEQ_NO_LENGTH);

        /* there is noone doing this except sleep and this function */
	atomic_set(&card->data_has_arrived,0);

        memcpy(&card->dma_stuff->write_ccw,WRITE_CCW,sizeof(ccw1_t));
        card->dma_stuff->write_ccw.count=len;
        card->dma_stuff->write_ccw.cda=
		QETH_GET_ADDR(card->dma_stuff->sendbuf);

	sprintf(dbf_text,"scdw%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	sprintf(dbf_text,"%8x",len);
	QETH_DBF_TEXT4(0,trace,dbf_text);
	QETH_DBF_HEX4(0,trace,&intparam,QETH_DBF_TRACE_LEN);
	QETH_DBF_HEX2(0,control,buffer,QETH_DBF_CONTROL_LEN);

        s390irq_spin_lock_irqsave(card->irq1,flags);
        result=do_IO(card->irq1,&card->dma_stuff->write_ccw,intparam,0,0);
        if (result) {
		qeth_delay_millis(QETH_WAIT_BEFORE_2ND_DOIO);
                result2=do_IO(card->irq1,&card->dma_stuff->write_ccw,
			      intparam,0,0);
		if (result2!=-ENODEV)
			PRINT_WARN("qeth_send_control_data: do_IO " \
				   "returned %i, next try returns %i\n",
				   result,result2);
		result=result2;
        }
        s390irq_spin_unlock_irqrestore(card->irq1,flags);

	if (result) {
		QETH_DBF_TEXT2(0,trace,"scd:doio");
		sprintf(dbf_text,"%4x",(__s16)result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		return NULL;
	}

	if (intparam==IPA_IOCTL_STATE) {
		if (qeth_sleepon_ioctl(card,QETH_IPA_TIMEOUT)) {
			QETH_DBF_TEXT2(0,trace,"scd:ioctime");
			/* re-enable qeth_send_control_data again */
			atomic_set(&card->write_busy,0);
			return NULL;
		}
		rec_buf=card->ipa_buf;
		sprintf(dbf_text,"scro%4x",card->irq0);
	} else {
		if (qeth_sleepon(card,(setip)?QETH_IPA_TIMEOUT:
				 QETH_MPC_TIMEOUT)) {
			QETH_DBF_TEXT2(0,trace,"scd:time");
			/* re-enable qeth_send_control_data again */
			atomic_set(&card->write_busy,0);
			return NULL;
		}
		rec_buf=card->ipa_buf;
		sprintf(dbf_text,"scri%4x",card->irq0);
	}
	QETH_DBF_TEXT2(0,trace,dbf_text);
	QETH_DBF_HEX2(0,control,rec_buf,QETH_DBF_CONTROL_LEN);

        memcpy(&card->seqno.pdu_hdr_ack,
               QETH_PDU_HEADER_SEQ_NO(rec_buf),
               QETH_SEQ_NO_LENGTH);

        return rec_buf;
}

static int qeth_send_ipa_cmd(qeth_card_t *card,ipa_cmd_t *cmd,int update_cmd,
			     int ipatype)
{
        unsigned char *buffer;
        ipa_cmd_t *reply;
        int ipa_cmd;
        int result;

	/* don't muck around with ipv6 if there's no use to do so */
	if ( (cmd->prot_version==6) &&
	     (!qeth_is_supported(IPA_IPv6)) ) return 0;

        ipa_cmd=cmd->command;

        memcpy(card->send_buf,IPA_PDU_HEADER,
	       IPA_PDU_HEADER_SIZE);

        memcpy(QETH_IPA_CMD_DEST_ADDR(card->send_buf),
               &card->token.ulp_connection_r,QETH_MPC_TOKEN_LENGTH);

        memcpy(card->send_buf+IPA_PDU_HEADER_SIZE,
	       cmd,sizeof(ipa_cmd_t));

        buffer=qeth_send_control_data(card,card->send_buf,
                                      IPA_PDU_HEADER_SIZE+sizeof(ipa_cmd_t),
                                      ipatype);

	if (!buffer) {
		if (atomic_read(&card->escape_softsetup)) result=0;
		else result=-1;
	} else {
		reply=(ipa_cmd_t*)PDU_ENCAPSULATION(buffer);
		if ((update_cmd)&&(reply)) memcpy(cmd,reply,sizeof(ipa_cmd_t));
		result=reply->return_code;

		if ((ipa_cmd==IPA_CMD_SETASSPARMS)&&(result==0)) {
			result=reply->data.setassparms.return_code;
		}
		if ((ipa_cmd==IPA_CMD_SETADAPTERPARMS)&&(result==0)) {
			result=reply->data.setadapterparms.return_code;
		}
	}
        return result;
}

static void qeth_fill_ipa_cmd(qeth_card_t *card,ipa_cmd_t *cmd,
			      __u8 command,int ip_vers)
{
        memset(cmd,0,sizeof(ipa_cmd_t));
        cmd->command=command;
        cmd->initiator=INITIATOR_HOST;
        cmd->seq_no=card->seqno.ipa++;
        cmd->adapter_type=qeth_get_adapter_type_for_ipa(card->link_type);
        cmd->rel_adapter_no=(__u8)card->options.portno;
        cmd->prim_version_no=1;
        cmd->param_count=1;
        cmd->prot_version=ip_vers;
        cmd->ipa_supported=0;
        cmd->ipa_enabled=0;
}

static int qeth_send_startstoplan(qeth_card_t *card,__u8 ipacmd,__u16 ip_vers)
{
        ipa_cmd_t cmd;
	int result;

        qeth_fill_ipa_cmd(card,&cmd,ipacmd,0);
        cmd.param_count=0;
        cmd.prot_version=ip_vers;
        cmd.ipa_supported=0;
        cmd.ipa_enabled=0;

        result=qeth_send_ipa_cmd(card,&cmd,0,IPA_CMD_STATE);
	return result;
}

static int qeth_send_startlan(qeth_card_t *card,__u16 ip_vers)
{
	int result;
	char dbf_text[15];

	sprintf(dbf_text,"stln%4x",card->irq0);
	QETH_DBF_TEXT4(0,trace,dbf_text);

        result=qeth_send_startstoplan(card,IPA_CMD_STARTLAN,ip_vers);
	if (!result) atomic_set(&card->is_startlaned,1);

	if (result) {
		QETH_DBF_TEXT2(0,trace,"STRTLNFL");
		sprintf(dbf_text,"%4x%4x",card->irq0,result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
	}

	return result;
}

static int qeth_send_stoplan(qeth_card_t *card)
{
#ifdef QETH_SEND_STOPLAN_ON_SHUTDOWN
	int result;
	char dbf_text[15];

	atomic_set(&card->is_startlaned,0);

	sprintf(dbf_text,"spln%4x",card->irq0);
	QETH_DBF_TEXT4(0,trace,dbf_text);

        result=qeth_send_startstoplan(card,IPA_CMD_STOPLAN,4);

	if (result) {
		QETH_DBF_TEXT2(0,trace,"STPLNFLD");
		sprintf(dbf_text,"%4x%4x",card->irq0,result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
	}

	return result;
#else /* QETH_SEND_STOPLAN_ON_SHUTDOWN */
	return 0;
#endif /* QETH_SEND_STOPLAN_ON_SHUTDOWN */
}

static int qeth_send_qipassist(qeth_card_t *card,short ip_vers)
{
        ipa_cmd_t cmd;
	int result;

        qeth_fill_ipa_cmd(card,&cmd,IPA_CMD_QIPASSIST,ip_vers);

	result=qeth_send_ipa_cmd(card,&cmd,1,IPA_CMD_STATE);

	if (!result) {
		if (ip_vers==4) {
			card->ipa_supported=cmd.ipa_supported;
			card->ipa_enabled=cmd.ipa_enabled;
		} else {
			card->ipa6_supported=cmd.ipa_supported;
			card->ipa6_enabled=cmd.ipa_enabled;
		}
	}

        return result;
}

static int qeth_send_ipa_arpcmd(qeth_card_t *card,arp_cmd_t *cmd,
				int update_cmd,int ipatype,__u32 req_size)
{
        unsigned char *buffer;
        int ipa_cmd;
        int result;
	__u16 s1,s2;

	/* don't muck around with ipv6 if there's no use to do so */
	if ( (cmd->prot_version==6) &&
	     (!qeth_is_supported(IPA_IPv6)) ) return 0;
	result = 0;
        ipa_cmd=cmd->command;

        memcpy(card->send_buf,IPA_PDU_HEADER,
	       IPA_PDU_HEADER_SIZE);
        memcpy(QETH_IPA_CMD_DEST_ADDR(card->send_buf),
               &card->token.ulp_connection_r,QETH_MPC_TOKEN_LENGTH);
        memcpy(card->send_buf+IPA_PDU_HEADER_SIZE,
	       cmd,sizeof(arp_cmd_t));

	if (req_size) {
		/* adjust sizes for big requests */
		s1=(__u32)IPA_PDU_HEADER_SIZE+SNMP_BASE_CMDLENGTH+req_size;
		s2=(__u32)SNMP_BASE_CMDLENGTH+req_size;
		memcpy(QETH_IPA_PDU_LEN_TOTAL(card->send_buf),&s1,2);
		memcpy(QETH_IPA_PDU_LEN_PDU1(card->send_buf),&s2,2);
		memcpy(QETH_IPA_PDU_LEN_PDU2(card->send_buf),&s2,2);
		memcpy(QETH_IPA_PDU_LEN_PDU3(card->send_buf),&s2,2);
	}

        buffer=qeth_send_control_data(card,card->send_buf,
                                      IPA_PDU_HEADER_SIZE+sizeof(arp_cmd_t),
                                      ipatype);
	if (!buffer) 
		result = -ENODATA;
	else 
		result = card->ioctl_returncode;
        return result;
}

static int qeth_ioctl_handle_snmp_data(qeth_card_t *card,arp_cmd_t *reply)
{
	__u16 data_len;

#define SNMP_HEADER_SIZE_WITH_TOKEN 36
	
	data_len = *((__u16*)QETH_IPA_PDU_LEN_PDU1(card->dma_stuff->recbuf));

	if (reply->data.setadapterparms.frame_seq_no == 1) {
		data_len = data_len - 
		    (__u16)((char*)reply->data.setadapterparms.
					  data.snmp_subcommand.
					  snmp_data - (char*)reply); 
	} else {
		data_len = data_len - 
		 	(__u16)((char*)&reply->data.setadapterparms.data.
                                       	snmp_subcommand.
					snmp_request - (char*)reply);
	}
	if (reply->data.setadapterparms.frame_seq_no == 1) {
		if (card->ioctl_buffersize <= (SNMP_HEADER_SIZE_WITH_TOKEN + 
		    reply->data.setadapterparms.frames_used_total *
		    ARP_DATA_SIZE)) {
			card->ioctl_returncode = ARP_RETURNCODE_ERROR;
			reply->data.setadapterparms.data.
				snmp_subcommand.snmp_returncode = -ENOMEM;
		} else {
			card->ioctl_returncode = ARP_RETURNCODE_SUCCESS;
			card->number_of_entries = 0;
			memcpy(((char *)card->ioctl_data_buffer),
	                         reply->data.setadapterparms.snmp_token,
		                 SNMP_HEADER_SIZE_WITH_TOKEN);
			card->ioctl_buffer_pointer = card->ioctl_data_buffer+
		  				     SNMP_HEADER_SIZE_WITH_TOKEN;
		}
	}

	if (card->ioctl_returncode != ARP_RETURNCODE_ERROR &&
	    reply->data.setadapterparms.frame_seq_no <=
	    reply->data.setadapterparms.frames_used_total) {
		if (reply->data.setadapterparms.return_code==
		    IPA_REPLY_SUCCESS) {
			if (reply->data.setadapterparms.frame_seq_no == 1) {
				memcpy(card->ioctl_buffer_pointer,
			       		reply->data.setadapterparms.data.
			       		snmp_subcommand.snmp_data,data_len);
			} else {
				memcpy(card->ioctl_buffer_pointer,
			       		(char*)&reply->data.setadapterparms.
						data.snmp_subcommand.
						snmp_request,data_len);
			}
			card->ioctl_buffer_pointer =
				card->ioctl_buffer_pointer + data_len;
			card->ioctl_returncode = ARP_RETURNCODE_SUCCESS;

			if (reply->data.setadapterparms.frame_seq_no == 
			    reply->data.setadapterparms.frames_used_total) {
				card->ioctl_returncode =
					ARP_RETURNCODE_LASTREPLY;
			}
		} else {
			card->ioctl_returncode = ARP_RETURNCODE_ERROR;
			memset(card->ioctl_data_buffer,0,
			       card->ioctl_buffersize);
			reply->data.setadapterparms.data.
				snmp_subcommand.snmp_returncode = 
				reply->data.setadapterparms.return_code;
		}
	}
#undef  SNMP_HEADER_SIZE_WITH_TOKEN

	return card->ioctl_returncode;
}

static int qeth_ioctl_handle_arp_data(qeth_card_t *card, arp_cmd_t *reply) 
{
	if (reply->data.setassparms.seq_no == 1) {
		if (card->ioctl_buffersize <= 
		    (sizeof(__u16) + sizeof(int) + reply->data.
		     setassparms.number_of_replies * ARP_DATA_SIZE)) {
			card->ioctl_returncode = ARP_RETURNCODE_ERROR;
		} else {
			card->ioctl_returncode = ARP_RETURNCODE_SUCCESS;
			card->number_of_entries = 0;
			card->ioctl_buffer_pointer = card->ioctl_data_buffer+
				sizeof(__u16) + sizeof(int);
		}
	}

	if (card->ioctl_returncode != ARP_RETURNCODE_ERROR &&
	    reply->data.setassparms.seq_no <=
	    reply->data.setassparms.number_of_replies) {
		
		if (reply->data.setassparms.return_code==IPA_REPLY_SUCCESS) {
		
			card->number_of_entries = card->number_of_entries + 
				                  reply->data.setassparms.
						  data.queryarp_data.
						  number_of_entries;
			memcpy(card->ioctl_buffer_pointer,
			       reply->data.setassparms.data.queryarp_data.
			       arp_data,ARP_DATA_SIZE);
			card->ioctl_buffer_pointer = card->
				ioctl_buffer_pointer + ARP_DATA_SIZE;
			card->ioctl_returncode = ARP_RETURNCODE_SUCCESS;
			if (reply->data.setassparms.seq_no ==
			    reply->data.setassparms.number_of_replies) {
				memcpy(card->ioctl_data_buffer,
				       &reply->data.setassparms.data.
				       queryarp_data.osa_setbitmask,
				       sizeof(__u16));
				card->ioctl_returncode=
					ARP_RETURNCODE_LASTREPLY;
			}
		} else {
			card->ioctl_returncode = ARP_RETURNCODE_ERROR;
			memset(card->ioctl_data_buffer,0,
			       card->ioctl_buffersize);
		}
	}
	return card->ioctl_returncode;
}

static int qeth_look_for_arp_data(qeth_card_t *card) 
{
	arp_cmd_t *reply;
	int result;

	
	reply=(arp_cmd_t*)PDU_ENCAPSULATION(card->dma_stuff->recbuf);

	if ( (reply->command==IPA_CMD_SETASSPARMS) &&
	     (reply->data.setassparms.assist_no==IPA_ARP_PROCESSING) &&
	     (reply->data.setassparms.command_code==
	      IPA_CMD_ASS_ARP_FLUSH_CACHE) ) {
		result=ARP_FLUSH;
	} else if ( (reply->command == IPA_CMD_SETASSPARMS) &&
	     (reply->data.setassparms.assist_no == IPA_ARP_PROCESSING) &&
	     (reply->data.setassparms.command_code == 
	      IPA_CMD_ASS_ARP_QUERY_INFO) &&
	     (card->ioctl_returncode == ARP_RETURNCODE_SUCCESS)) {
		result = qeth_ioctl_handle_arp_data(card,reply);
	} else if ( (reply->command == IPA_CMD_SETADAPTERPARMS)	&&
	            (reply->data.setadapterparms.command_code == 
		     IPA_SETADP_SET_SNMP_CONTROL) &&
		    (card->ioctl_returncode == ARP_RETURNCODE_SUCCESS) ){
		result = qeth_ioctl_handle_snmp_data(card,reply);
	} else
		result = ARP_RETURNCODE_NOARPDATA;		
	
	return result;
}



static int qeth_queryarp(qeth_card_t *card,struct ifreq *req,int version,
			 __u32 assist_no, __u16 command_code,char *c_data,
			 __u16 len)
{
	int data_size;
	arp_cmd_t *cmd;
	int result;
        

	cmd = (arp_cmd_t *) kmalloc(sizeof(arp_cmd_t),GFP_KERNEL);
	if (!cmd) {
		return IPA_REPLY_FAILED;
	}

	memcpy(&data_size,c_data,sizeof(int));

	qeth_fill_ipa_cmd(card,(ipa_cmd_t*)cmd,IPA_CMD_SETASSPARMS,version);

	cmd->data.setassparms.assist_no=assist_no;
	cmd->data.setassparms.length=8+len;
        cmd->data.setassparms.command_code=command_code;
        cmd->data.setassparms.return_code=0;
	cmd->data.setassparms.seq_no=0;

	card->ioctl_buffersize = data_size;
	card->ioctl_data_buffer = (char *) vmalloc(data_size);
	if (!card->ioctl_data_buffer) {
		kfree(cmd);
		return IPA_REPLY_FAILED;
	}

	card->ioctl_returncode = ARP_RETURNCODE_SUCCESS;

	result=qeth_send_ipa_arpcmd(card,cmd,1,IPA_IOCTL_STATE,0);
	
	if ((result == ARP_RETURNCODE_ERROR) || 
	    (result == -ENODATA)) {
		result = IPA_REPLY_FAILED;
	}
	else {
		result = IPA_REPLY_SUCCESS;
		memcpy(((char *)(card->ioctl_data_buffer)) + sizeof(__u16),
		       &(card->number_of_entries),sizeof(int));
		copy_to_user(req->ifr_ifru.ifru_data,
			     card->ioctl_data_buffer,data_size);		
	}
	card->ioctl_buffer_pointer = NULL;
	vfree(card->ioctl_data_buffer);
	kfree(cmd);
	card->number_of_entries = 0;
	card->ioctl_buffersize = 0;

	return result;
}

static int snmp_set_setadapterparms_command(qeth_card_t *card,
					    arp_cmd_t *cmd,struct ifreq *req, 
					    char *data,__u16 len,
					    __u16 command_code,int req_size)
{
	__u32 data_size;

	memcpy(&data_size,data,sizeof(__u32));

	card->ioctl_buffersize = data_size;
	card->ioctl_data_buffer = (char *) vmalloc(data_size);
	if (!card->ioctl_data_buffer) {
		return -ENOMEM;
	}
	card->ioctl_returncode = ARP_RETURNCODE_SUCCESS;

	memcpy(cmd->data.setadapterparms.snmp_token,
	       data+SNMP_REQUEST_DATA_OFFSET,req_size); 

	cmd->data.setadapterparms.cmdlength=SNMP_SETADP_CMDLENGTH+req_size;
	cmd->data.setadapterparms.command_code = command_code;
	cmd->data.setadapterparms.frames_used_total=1;
	cmd->data.setadapterparms.frame_seq_no=1;

	return 0;
}

static int qeth_send_snmp_control(qeth_card_t *card,struct ifreq *req,
				  __u32 command,__u16 command_code,
				  char *c_data,__u16 len)
{
	arp_cmd_t *cmd;
	__u32 result,req_size;

        cmd = (arp_cmd_t *) kmalloc(sizeof(arp_cmd_t),GFP_KERNEL);
	if (!cmd) {
		return IPA_REPLY_FAILED;
	}

	qeth_fill_ipa_cmd(card,(ipa_cmd_t*)cmd,command,4);

	memcpy(&req_size,((char*)c_data)+sizeof(__u32),sizeof(__u32));

	if (snmp_set_setadapterparms_command(card,cmd,req,c_data,
					     len,command_code,req_size))
	{
		kfree(cmd);
		return IPA_REPLY_FAILED;
	}

	result=qeth_send_ipa_arpcmd(card,cmd,1,IPA_IOCTL_STATE,req_size);

	if (result == -ENODATA) {
		result = IPA_REPLY_FAILED;
		goto snmp_out;
	}
	if (result == ARP_RETURNCODE_ERROR ) {
		copy_to_user(req->ifr_ifru.ifru_data+SNMP_REQUEST_DATA_OFFSET,
			     card->ioctl_data_buffer,card->ioctl_buffersize);
		result = IPA_REPLY_FAILED;
	}
	else {
		copy_to_user(req->ifr_ifru.ifru_data+SNMP_REQUEST_DATA_OFFSET,
			     card->ioctl_data_buffer,card->ioctl_buffersize);
		result = IPA_REPLY_SUCCESS;
	}
snmp_out:
	card->number_of_entries = 0;
	card->ioctl_buffersize = 0;
	card->ioctl_buffer_pointer = NULL;
	vfree(card->ioctl_data_buffer);
	kfree(cmd);

	return result;
}

static int qeth_send_setassparms(qeth_card_t *card,int version,
				 __u32 assist_no,__u16 command_code,
				 long data,__u16 len)
{
        ipa_cmd_t cmd;
        int result;

        qeth_fill_ipa_cmd(card,&cmd,IPA_CMD_SETASSPARMS,version);

        cmd.data.setassparms.assist_no=assist_no;
        cmd.data.setassparms.length=8+len;
        cmd.data.setassparms.command_code=command_code;
        cmd.data.setassparms.return_code=0;
        cmd.data.setassparms.seq_no=0;

	if (len<=sizeof(__u32))
		cmd.data.setassparms.data.flags_32bit=(__u32)data;
	else if (len>sizeof(__u32))
		memcpy(&cmd.data.setassparms.data,(void*)data,
		       /* limit here to a page or so */
		       qeth_min(len,PAGE_SIZE));
	if (command_code != IPA_CMD_ASS_START) {
		result=qeth_send_ipa_cmd(card,&cmd,0,
			 ((assist_no==IPA_ARP_PROCESSING)&&
			  (command_code!=IPA_CMD_ASS_ARP_FLUSH_CACHE))?
					 IPA_IOCTL_STATE:IPA_CMD_STATE);
 
	} else
		result=qeth_send_ipa_cmd(card,&cmd,0,IPA_CMD_STATE);
		
	return result;
}

static int qeth_send_setadapterparms_query(qeth_card_t *card)
{
        ipa_cmd_t cmd;
	int result;

	qeth_fill_ipa_cmd(card,&cmd,IPA_CMD_SETADAPTERPARMS,
			  IPA_SETADAPTERPARMS_IP_VERSION);
	cmd.data.setadapterparms.cmdlength=sizeof(struct ipa_setadp_cmd);
	cmd.data.setadapterparms.command_code=
		IPA_SETADP_QUERY_COMMANDS_SUPPORTED;
	cmd.data.setadapterparms.frames_used_total=1;
	cmd.data.setadapterparms.frame_seq_no=1;
	result=qeth_send_ipa_cmd(card,&cmd,1,IPA_CMD_STATE);

	if (cmd.data.setadapterparms.data.query_cmds_supp.lan_type&0x7f)
		card->link_type=cmd.data.setadapterparms.data.
			query_cmds_supp.lan_type;

	card->adp_supported=
		cmd.data.setadapterparms.data.query_cmds_supp.supported_cmds;

	return result;
}

static int qeth_send_setadapterparms_mode(qeth_card_t *card,__u32 command,
					  __u32 mode)
{
        ipa_cmd_t cmd;
	int result;

	qeth_fill_ipa_cmd(card,&cmd,IPA_CMD_SETADAPTERPARMS,
			  IPA_SETADAPTERPARMS_IP_VERSION);
	cmd.data.setadapterparms.cmdlength=sizeof(struct ipa_setadp_cmd);
	cmd.data.setadapterparms.command_code=command;
	cmd.data.setadapterparms.frames_used_total=1;
	cmd.data.setadapterparms.frame_seq_no=1;
	cmd.data.setadapterparms.data.mode=mode;
	result=qeth_send_ipa_cmd(card,&cmd,0,IPA_CMD_STATE);

	return result;
}

static int qeth_send_setadapterparms_change_addr(qeth_card_t *card,
						 __u32 command,
						 __u32 subcmd,__u8 *mac_addr,
						 int addr_len)
{
        ipa_cmd_t cmd;
	int result;

	qeth_fill_ipa_cmd(card,&cmd,IPA_CMD_SETADAPTERPARMS,
			  IPA_SETADAPTERPARMS_IP_VERSION);
	cmd.data.setadapterparms.cmdlength=sizeof(struct ipa_setadp_cmd);
	cmd.data.setadapterparms.command_code=command;
	cmd.data.setadapterparms.frames_used_total=1;
	cmd.data.setadapterparms.frame_seq_no=1;
	cmd.data.setadapterparms.data.change_addr.cmd=subcmd;
	cmd.data.setadapterparms.data.change_addr.addr_size=addr_len;
	memcpy(&cmd.data.setadapterparms.data.change_addr.addr,
	       mac_addr,addr_len);

	result=qeth_send_ipa_cmd(card,&cmd,1,IPA_CMD_STATE);

	memcpy(mac_addr,&cmd.data.setadapterparms.data.change_addr.addr,
	       addr_len);

	return result;
}

static int qeth_send_setassparms_simple_with_data(qeth_card_t *card,
						  __u32 assist_no,
						  __u16 command_code,
						  long data)
{
        return qeth_send_setassparms(card,4,assist_no,command_code,data,4);
}

static int qeth_send_setassparms_simple_without_data(qeth_card_t *card,
						     __u32 assist_no,
						     __u16 command_code)
{
        return qeth_send_setassparms(card,4,assist_no,command_code,0,0);
}

static int qeth_send_setassparms_simple_without_data6(qeth_card_t *card,
						      __u32 assist_no,
						      __u16 command_code)
{
 	return qeth_send_setassparms(card,6,assist_no,command_code,0,0);
}

static int qeth_send_setdelip(qeth_card_t *card,__u8 *ip,__u8 *netmask,
			      int ipacmd,short ip_vers,unsigned int flags)
{
        ipa_cmd_t cmd;
        int ip_len=(ip_vers==6)?16:4;

        qeth_fill_ipa_cmd(card,&cmd,ipacmd,ip_vers);
        if (ip_vers==6) {
                memcpy(&cmd.data.setdelip6.ip,ip,ip_len);
                memcpy(&cmd.data.setdelip6.netmask,netmask,ip_len);
                cmd.data.setdelip6.flags=flags;
        } else {
                memcpy(&cmd.data.setdelip4.ip,ip,ip_len);
                memcpy(&cmd.data.setdelip4.netmask,netmask,ip_len);
                cmd.data.setdelip4.flags=flags;
        }

        return qeth_send_ipa_cmd(card,&cmd,0,IPA_CMD_STATE|
				 ((ipacmd==IPA_CMD_SETIP)?IPA_SETIP_FLAG:0));
}

static int qeth_send_setdelipm(qeth_card_t *card,__u8 *ip,__u8 *mac,
			       int ipacmd,short ip_vers)
{
        ipa_cmd_t cmd;
        int ip_len=(ip_vers==6)?16:4;

        qeth_fill_ipa_cmd(card,&cmd,ipacmd,ip_vers);
        memcpy(&cmd.data.setdelipm.mac,mac,6);
        if (ip_vers==6) {
                memcpy(&cmd.data.setdelipm.ip6,ip,ip_len);
        } else {
                memcpy(&cmd.data.setdelipm.ip4_6,ip,ip_len);
        }

        return qeth_send_ipa_cmd(card,&cmd,0,IPA_CMD_STATE|
				 ((ipacmd==IPA_CMD_SETIPM)?IPA_SETIP_FLAG:0));
}

#define PRINT_SETIP_ERROR(x) \
	if (result) \
		PRINT_ERR("setip%c: return code 0x%x (%s)\n",x,result, \
			  (result==0xe002)?"invalid mtu size": \
	       		  (result==0xe005)?"duplicate ip address": \
	       		  (result==0xe0a5)?"duplicate ip address": \
       			  (result==0xe006)?"ip table full": \
			  (result==0xe008)?"startlan not received": \
			  (result==0xe009)?"setip already received": \
			  (result==0xe00a)?"dup network ip address": \
			  (result==0xe00b)?"mblk no free main task entry": \
			  (result==0xe00d)?"invalid ip version": \
			  (result==0xe00e)?"unsupported arp assist cmd": \
			  (result==0xe00f)?"arp assist not enabled": \
			  (result==0xe080)?"startlan disabled": \
			  (result==-1)?"IPA communication timeout": \
			  "unknown return code")

static inline int qeth_send_setip(qeth_card_t *card,__u8 *ip,
				  __u8 *netmask,short ip_vers,int use_retries)
{
	int result;
	int retries;
	char dbf_text[15];
	int takeover=0;

	retries=(use_retries)?QETH_SETIP_RETRIES:1;
	if (qeth_is_ipa_covered_by_ipato_entries(ip_vers,ip,card)) {
		sprintf(dbf_text,"ipto%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		if (ip_vers==4) {
			*((__u32*)(&dbf_text[0]))=*((__u32*)ip);
			*((__u32*)(&dbf_text[4]))=*((__u32*)netmask);
			QETH_DBF_HEX2(0,trace,dbf_text,QETH_DBF_TRACE_LEN);
		} else {
			QETH_DBF_HEX2(0,trace,ip,QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX2(0,trace,ip+QETH_DBF_TRACE_LEN,
				      QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX2(0,trace,netmask,QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX2(0,trace,netmask+QETH_DBF_TRACE_LEN,
				      QETH_DBF_TRACE_LEN);
		}
		takeover=1;
	}
retry:
        result=qeth_send_setdelip(card,ip,netmask,IPA_CMD_SETIP,ip_vers,
				  (takeover)?IPA_SETIP_TAKEOVER_FLAGS:
				  IPA_SETIP_FLAGS);
	PRINT_SETIP_ERROR(' ');

	if (result) {
		QETH_DBF_TEXT2(0,trace,"SETIPFLD");
		sprintf(dbf_text,"%4x%4x",card->irq0,result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
	}

	if (((result==-1)||(result==0xe080))&&(retries--)) {
		sprintf(dbf_text,"sipr%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		if (ip_vers==4) {
			*((__u32*)(&dbf_text[0]))=*((__u32*)ip);
			*((__u32*)(&dbf_text[4]))=*((__u32*)netmask);
			QETH_DBF_HEX2(0,trace,dbf_text,QETH_DBF_TRACE_LEN);
		} else {
			QETH_DBF_HEX2(0,trace,ip,QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX2(0,trace,ip+QETH_DBF_TRACE_LEN,
				      QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX2(0,trace,netmask,QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX2(0,trace,netmask+QETH_DBF_TRACE_LEN,
				      QETH_DBF_TRACE_LEN);
		}
		PRINT_WARN("trying again...\n");
		goto retry;
	}

	return result;
}

static inline int qeth_send_delip(qeth_card_t *card,__u8 *ip,
				  __u8 *netmask,short ip_vers)
{
        return qeth_send_setdelip(card,ip,netmask,IPA_CMD_DELIP,ip_vers,
				  IPA_DELIP_FLAGS);
}

static inline int qeth_send_setipm(qeth_card_t *card,__u8 *ip,
				   __u8 *mac,short ip_vers,int use_retries)
{
	int result;
	int retries;
	char dbf_text[15];
	
	retries=(use_retries)?QETH_SETIP_RETRIES:1;
	if (qeth_is_ipa_covered_by_ipato_entries(ip_vers,ip,card)) {
		sprintf(dbf_text,"imto%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		if (ip_vers==4) {
			*((__u32*)(&dbf_text[0]))=*((__u32*)ip);
			QETH_DBF_HEX2(0,trace,dbf_text,QETH_DBF_TRACE_LEN);
		} else {
			QETH_DBF_HEX2(0,trace,ip,QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX2(0,trace,ip+QETH_DBF_TRACE_LEN,
				      QETH_DBF_TRACE_LEN);
		}
	}

retry:
        result=qeth_send_setdelipm(card,ip,mac,IPA_CMD_SETIPM,ip_vers);
	PRINT_SETIP_ERROR('m');

	if (result) {
		QETH_DBF_TEXT2(0,trace,"SETIMFLD");
		sprintf(dbf_text,"%4x%4x",card->irq0,result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
	}

	if ((result==-1)&&(retries--)) {
		sprintf(dbf_text,"simr%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		if (ip_vers==4) {
			sprintf(dbf_text,"%08x",*((__u32*)ip));
			QETH_DBF_TEXT2(0,trace,dbf_text);
		} else {
			QETH_DBF_HEX2(0,trace,ip,QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX2(0,trace,ip+QETH_DBF_TRACE_LEN,
				      QETH_DBF_TRACE_LEN);
		}
		QETH_DBF_HEX2(0,trace,mac,OSA_ADDR_LEN);
		PRINT_WARN("trying again...\n");
		goto retry;
	}

	return result;
}

static inline int qeth_send_delipm(qeth_card_t *card,__u8 *ip,
				   __u8 *mac,short ip_vers)
{
        return qeth_send_setdelipm(card,ip,mac,IPA_CMD_DELIPM,ip_vers);
}

static int qeth_add_vipa_entry(qeth_card_t *card,int version,__u8 *addr,
			       int flag)
{
	qeth_vipa_entry_t *entry,*e;
	int result=0;

	entry=(qeth_vipa_entry_t*)kmalloc(sizeof(qeth_vipa_entry_t),
					  GFP_KERNEL);
	if (!entry) {
		PRINT_ERR("not enough memory for vipa handling\n");
		return -ENOMEM;
	}
	entry->version=version;
	entry->flag=flag;
	memcpy(entry->ip,addr,16);
	entry->state=VIPA_2_B_ADDED;

	my_write_lock(&card->vipa_list_lock);
	e=card->vipa_list;
	while (e) {
		if (e->version!=version) goto next;
		if (memcmp(e->ip,addr,(version==4)?4:16)) goto next;
		if (flag==IPA_SETIP_VIPA_FLAGS) {
			PRINT_ERR("vipa already set\n");
		} else {
			PRINT_ERR("rxip already set\n");
		}
		kfree(entry);
		result=-EALREADY;
		goto out;
next:
		e=e->next;
	}
	entry->next=card->vipa_list;
	card->vipa_list=entry;
out:
	my_write_unlock(&card->vipa_list_lock);
	return result;
}

static int qeth_del_vipa_entry(qeth_card_t *card,int version,__u8 *addr,
			       int flag)
{
	qeth_vipa_entry_t *e;
	int result=0;

	my_write_lock(&card->vipa_list_lock);
	e=card->vipa_list;
	while (e) {
		if (e->version!=version) goto next;
		if (e->flag!=flag) goto next;
		if (memcmp(e->ip,addr,(version==4)?4:16)) goto next;
		e->state=VIPA_2_B_REMOVED;
		goto out;
next:
		e=e->next;
	}
	if (flag==IPA_SETIP_VIPA_FLAGS) {
		PRINT_ERR("vipa not found\n");
	} else {
		PRINT_ERR("rxip not found\n");
	}
	result=-ENOENT;
out:
	my_write_unlock(&card->vipa_list_lock);
	return result;
}

static void qeth_set_vipas(qeth_card_t *card,int set_only)
{
	qeth_vipa_entry_t *e,*le=NULL,*ne; /* ne stands for new entry,
     					      le is last entry */
	char dbf_text[15];
	int result;
	__u8 netmask[16]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
	qeth_vipa_entry_t *priv_add_list=NULL;
	qeth_vipa_entry_t *priv_del_list=NULL;

	my_write_lock(&card->vipa_list_lock);
	e=card->vipa_list;
	while (e) {
		switch (e->state) {
		case VIPA_2_B_ADDED:
			if (!set_only) break;
			if (!atomic_read(&card->is_open)) break;
			/* we don't want to hold the lock for a long time...
			 * so we clone the entry */
			ne=(qeth_vipa_entry_t*)
				kmalloc(sizeof(qeth_vipa_entry_t),
					GFP_KERNEL);
			if (ne) {
				ne->version=e->version;
				memcpy(ne->ip,e->ip,16);
				ne->next=priv_add_list;
				priv_add_list=ne;

				e->state=VIPA_ESTABLISHED;
			} else {
				PRINT_ERR("not enough for internal vipa " \
					  "handling... trying to set " \
					  "vipa next time.\n");
				qeth_start_softsetup_thread(card);
			}
			break;
		case VIPA_2_B_REMOVED:
			if (set_only) break;
			if (le)
				le->next=e->next;
			else card->vipa_list=e->next;
			ne=e->next;
			e->next=priv_del_list;
			priv_del_list=e;
			e=ne;
			continue;
		case VIPA_ESTABLISHED:
			if (atomic_read(&card->is_open)) break;
			/* we don't want to hold the lock for a long time...
			 * so we clone the entry */
			ne=(qeth_vipa_entry_t*)
				kmalloc(sizeof(qeth_vipa_entry_t),
					GFP_KERNEL);
			if (ne) {
				ne->version=e->version;
				memcpy(ne->ip,e->ip,16);
				ne->next=priv_del_list;
				priv_del_list=ne;

				e->state=VIPA_2_B_ADDED;
			} else {
				PRINT_ERR("not enough for internal vipa " \
					  "handling... VIPA/RXIP remains set " \
					  "although device is stopped.\n");
				qeth_start_softsetup_thread(card);
			}
			break;
		default:
			break;
		}
		le=e;
		e=e->next;
	}
	my_write_unlock(&card->vipa_list_lock);

	while (priv_add_list) {
		result=qeth_send_setdelip(card,priv_add_list->ip,netmask,
					  IPA_CMD_SETIP,priv_add_list->version,
					  priv_add_list->flag);
		PRINT_SETIP_ERROR('s');

		if (result) {
			QETH_DBF_TEXT2(0,trace,"SETSVFLD");
			sprintf(dbf_text,"%4x%4x",card->irq0,result);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			if (priv_add_list->version==4) {
				PRINT_ERR("going to leave vipa/rxip %08x" \
					  "unset...\n",
					  *((__u32*)&priv_add_list->ip[0]));
				sprintf(dbf_text,"%08x",
					  *((__u32*)&priv_add_list->ip[0]));
				QETH_DBF_TEXT2(0,trace,dbf_text);
			} else {
				PRINT_ERR("going to leave vipa/rxip " \
					  "%08x%08x%08x%08x unset...\n",
					  *((__u32*)&priv_add_list->ip[0]),
					  *((__u32*)&priv_add_list->ip[4]),
					  *((__u32*)&priv_add_list->ip[8]),
					  *((__u32*)&priv_add_list->ip[12]));
				QETH_DBF_HEX2(0,trace,&priv_add_list->ip[0],
					      QETH_DBF_TRACE_LEN);
				QETH_DBF_HEX2(0,trace,&priv_add_list->ip[8],
					      QETH_DBF_TRACE_LEN);
			}
		}
		e=priv_add_list;
		priv_add_list=priv_add_list->next;
		kfree(e);
	}

	while (priv_del_list) {
		result=qeth_send_setdelip(card,priv_del_list->ip,netmask,
					  IPA_CMD_DELIP,priv_del_list->version,
					  priv_del_list->flag);
		if (result) {
			QETH_DBF_TEXT2(0,trace,"DELSVFLD");
			sprintf(dbf_text,"%4x%4x",card->irq0,result);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			if (priv_del_list->version==4) {
				PRINT_ERR("could not delete vipa/rxip " \
					  "%08x...\n",
					  *((__u32*)&priv_del_list->ip[0]));
				sprintf(dbf_text,"%08x",
					  *((__u32*)&priv_del_list->ip[0]));
				QETH_DBF_TEXT2(0,trace,dbf_text);
			} else {
				PRINT_ERR("could not delete vipa/rxip " \
					  "%08x%08x%08x%08x...\n",
					  *((__u32*)&priv_del_list->ip[0]),
					  *((__u32*)&priv_del_list->ip[4]),
					  *((__u32*)&priv_del_list->ip[8]),
					  *((__u32*)&priv_del_list->ip[12]));
				QETH_DBF_HEX2(0,trace,&priv_del_list->ip[0],
					      QETH_DBF_TRACE_LEN);
				QETH_DBF_HEX2(0,trace,&priv_del_list->ip[8],
					      QETH_DBF_TRACE_LEN);
			}
		}
		e=priv_del_list;
		priv_del_list=priv_del_list->next;
		kfree(e);
	}
}

static void qeth_refresh_vipa_states(qeth_card_t *card)
{
	qeth_vipa_entry_t *e;

	my_write_lock(&card->vipa_list_lock);
	e=card->vipa_list;
	while (e) {
		if (e->state==VIPA_ESTABLISHED) e->state=VIPA_2_B_ADDED;
		e=e->next;
	}
	my_write_unlock(&card->vipa_list_lock);
}

static inline int qeth_send_setrtg(qeth_card_t *card,int routing_type,
				   short ip_vers)
{
        ipa_cmd_t cmd;

        qeth_fill_ipa_cmd(card,&cmd,IPA_CMD_SETRTG,ip_vers);
	/* strip off RESET_ROUTING_FLAG */
        cmd.data.setrtg.type=(routing_type)&(ROUTER_MASK);

        return qeth_send_ipa_cmd(card,&cmd,0,IPA_CMD_STATE);
}

static int qeth_is_ipa_in_list(struct in_ifaddr *ip,struct in_ifaddr *list)
{
	while (list) {
		if (ip->ifa_address==list->ifa_address) return 1;
		list=list->ifa_next;
	}
	return 0;
}

#ifdef QETH_IPV6
static int qeth_is_ipa_in_list6(struct inet6_ifaddr *ip,
				struct inet6_ifaddr *list)
{
	while (list) {
		if (!memcmp(&ip->addr.s6_addr,&list->addr.s6_addr,16))
			return 1;
		list=list->if_next;
	}
	return 0;
}

static int qeth_add_ifa6_to_list(struct inet6_ifaddr **list,
       				 struct inet6_ifaddr *ifa)
{
	struct inet6_ifaddr *i;

	if (*list==NULL) {
		*list=ifa;
	} else {
		if (qeth_is_ipa_in_list6(ifa,*list))
			return -EALREADY;
		i=*list;
		while (i->if_next) {
			i=i->if_next;
		}
		i->if_next=ifa;
	}
	ifa->if_next=NULL;
	return 0;
}
#endif /* QETH_IPV6 */

static int qeth_add_ifa_to_list(struct in_ifaddr **list,struct in_ifaddr *ifa)
{
	struct in_ifaddr *i;

	if (*list==NULL) {
		*list=ifa;
	} else {
		if (qeth_is_ipa_in_list(ifa,*list))
			return -EALREADY;
		i=*list;
		while (i->ifa_next) {
			i=i->ifa_next;
		}
		i->ifa_next=ifa;
	}
	ifa->ifa_next=NULL;
	return 0;
}

static int qeth_setips(qeth_card_t *card,int use_setip_retries)
{
	struct in_ifaddr *addr;
	int result;
	char dbf_text[15];
#ifdef QETH_IPV6
	struct inet6_ifaddr *addr6;
	__u8 netmask[16];
#endif /* QETH_IPV6 */

	sprintf(dbf_text,"stip%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	addr=card->ip_current_state.ip_ifa;
	while (addr) {
		if (!qeth_is_ipa_in_list(addr,card->ip_new_state.ip_ifa)) {
			QETH_DBF_TEXT3(0,trace,"setipdel");
			*((__u32*)(&dbf_text[0]))=*((__u32*)&addr->ifa_address);
			*((__u32*)(&dbf_text[4]))=*((__u32*)&addr->ifa_mask);
			QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN);
			result=qeth_send_delip(card,(__u8*)&addr->ifa_address,
					       (__u8*)&addr->ifa_mask,4);
			if (result) {
				PRINT_ERR("was not able to delete ip " \
			    		  "%08x/%08x on irq x%x " \
					  "(result: 0x%x), " \
		    			  "trying to continue\n",
	    				  addr->ifa_address,
    					  addr->ifa_mask,card->irq0,result);
				sprintf(dbf_text,"stdl%4x",result);
				QETH_DBF_TEXT3(0,trace,dbf_text);
			}
		}
		addr=addr->ifa_next;
	}

	addr=card->ip_new_state.ip_ifa;
	while (addr) {
		if (!qeth_is_ipa_in_list(addr,
					 card->ip_current_state.ip_ifa)) {
			QETH_DBF_TEXT3(0,trace,"setipset");
			*((__u32*)(&dbf_text[0]))=
				*((__u32*)&addr->ifa_address);
			*((__u32*)(&dbf_text[4]))=
				*((__u32*)&addr->ifa_mask);
			QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN);
			result=qeth_send_setip(card,(__u8*)&addr->ifa_address,
					       (__u8*)&addr->ifa_mask,4,
					       use_setip_retries);
			if (result) {
				PRINT_ERR("was not able to set ip " \
					  "%08x/%08x on irq x%x, trying to " \
					  "continue\n",
					  addr->ifa_address,
					  addr->ifa_mask,card->irq0);
				sprintf(dbf_text,"stst%4x",result);
				QETH_DBF_TEXT3(0,trace,dbf_text);
			}
		}
		addr=addr->ifa_next;
	}

#ifdef QETH_IPV6
#define FILL_NETMASK(len) { \
	int i,j; \
	for (i=0;i<16;i++) { \
		j=(len)-(i*8); \
		if (j>=8) netmask[i]=0xff; else \
			if (j<=0) netmask[i]=0x0; else \
				netmask[i]=(__u8)(0xFF00>>j); \
	} \
}
	/* here we go with IPv6 */
	addr6=card->ip_current_state.ip6_ifa;
	while (addr6) {
		if (!qeth_is_ipa_in_list6(addr6,card->ip_new_state.ip6_ifa)) {
			QETH_DBF_TEXT3(0,trace,"setipdl6");
			QETH_DBF_HEX3(0,trace,&addr6->addr.s6_addr,
				      QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX3(0,trace,
				      ((char *)(&addr6->addr.s6_addr))+
				      QETH_DBF_TRACE_LEN,QETH_DBF_TRACE_LEN);
			sprintf(dbf_text,"nmsk%4u",addr6->prefix_len);
			QETH_DBF_TEXT3(0,trace,dbf_text);
			FILL_NETMASK(addr6->prefix_len);
			result=qeth_send_delip(card,
					       (__u8*)&addr6->addr.s6_addr,
					       (__u8*)&netmask,6);
			if (result) {
				PRINT_ERR("was not able to delete ip " \
			    		  "%04x:%04x:%04x:%04x:%04x:%04x:" \
					  "%04x:%04x/%u on irq x%x " \
					  "(result: 0x%x), " \
		    			  "trying to continue\n",
					  addr6->addr.s6_addr16[0],
					  addr6->addr.s6_addr16[1],
					  addr6->addr.s6_addr16[2],
					  addr6->addr.s6_addr16[3],
					  addr6->addr.s6_addr16[4],
					  addr6->addr.s6_addr16[5],
					  addr6->addr.s6_addr16[6],
					  addr6->addr.s6_addr16[7],
					  addr6->prefix_len,
					  card->irq0,result);
				sprintf(dbf_text,"std6%4x",result);
				QETH_DBF_TEXT3(0,trace,dbf_text);
			}
		}
		addr6=addr6->if_next;
	}

	addr6=card->ip_new_state.ip6_ifa;
	while (addr6) {
		if (!qeth_is_ipa_in_list6(addr6,
					  card->ip_current_state.ip6_ifa)) {
			QETH_DBF_TEXT3(0,trace,"setipst6");
			QETH_DBF_HEX3(0,trace,&addr6->addr.s6_addr,
				      QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX3(0,trace,
				      ((char *)(&addr6->addr.s6_addr))+
				      QETH_DBF_TRACE_LEN,QETH_DBF_TRACE_LEN);
			sprintf(dbf_text,"nmsk%4u",addr6->prefix_len);
			QETH_DBF_TEXT3(0,trace,dbf_text);
			FILL_NETMASK(addr6->prefix_len);
			result=qeth_send_setip(card,
					       (__u8*)&addr6->addr.s6_addr,
					       (__u8*)&netmask,6,
					       use_setip_retries);
			if (result) {
				PRINT_ERR("was not able to set ip " \
			    		  "%04x:%04x:%04x:%04x:%04x:%04x:" \
					  "%04x:%04x/%u on irq x%x " \
					  "(result: 0x%x), " \
		    			  "trying to continue\n",
					  addr6->addr.s6_addr16[0],
					  addr6->addr.s6_addr16[1],
					  addr6->addr.s6_addr16[2],
					  addr6->addr.s6_addr16[3],
					  addr6->addr.s6_addr16[4],
					  addr6->addr.s6_addr16[5],
					  addr6->addr.s6_addr16[6],
					  addr6->addr.s6_addr16[7],
					  addr6->prefix_len,
					  card->irq0,result);
				sprintf(dbf_text,"sts6%4x",result);
				QETH_DBF_TEXT3(0,trace,dbf_text);
			}
		}
		addr6=addr6->if_next;
	}
#endif /* QETH_IPV6 */

        return 0;
}

static int qeth_is_ipma_in_list(struct qeth_ipm_mac *ipma,
				struct qeth_ipm_mac *list)
{
	while (list) {
		if ( (!memcmp(ipma->ip,list->ip,16)) &&
		     (!memcmp(ipma->mac,list->mac,6)) ) return 1;
		list=list->next;
	}
	return 0;
}

static void qeth_remove_mc_ifa_from_list(struct qeth_ipm_mac **list,
       					 struct qeth_ipm_mac *ipma)
{
	struct qeth_ipm_mac *i,*li=NULL;

	if ((!(*list)) || (!ipma)) return;

	if (*list==ipma) {
		*list=ipma->next;
	} else {
		i=*list;
		while (i) {
			if (i==ipma) {
				li->next=i->next;
			} else {
				li=i;
			}
			i=i->next;
		}
	}
}

static int qeth_add_mc_ifa_to_list(struct qeth_ipm_mac **list,
       				   struct qeth_ipm_mac *ipma)
{
	struct qeth_ipm_mac *i;

	if (qeth_is_ipma_in_list(ipma,*list))
		return -EALREADY;

	if (*list==NULL) {
		*list=ipma;
	} else {
		i=*list;
		while (i->next) {
			i=i->next;
		}
		i->next=ipma;
	}
	ipma->next=NULL;
	return 0;
}

static int qeth_setipms(qeth_card_t *card,int use_setipm_retries)
{
	struct qeth_ipm_mac *addr;
	int result;
	char dbf_text[15];

	sprintf(dbf_text,"stim%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

        if (qeth_is_supported(IPA_MULTICASTING)) {
		addr=card->ip_mc_current_state.ipm_ifa;
		while (addr) {
			if (!qeth_is_ipma_in_list(addr,card->
						  ip_mc_new_state.ipm_ifa)) {
				QETH_DBF_TEXT3(0,trace,"setimdel");
				sprintf(dbf_text,"%08x",
					*((__u32*)&addr->ip[0]));
				QETH_DBF_TEXT3(0,trace,dbf_text);
				*((__u32*)(&dbf_text[0]))=
					*((__u32*)&addr->mac);
				*((__u32*)(&dbf_text[4]))=
					*(((__u32*)&addr->mac)+1);
				QETH_DBF_HEX3(0,trace,dbf_text,
					      QETH_DBF_TRACE_LEN);
				result=qeth_send_delipm(
					card,(__u8*)&addr->ip[0],
					(__u8*)addr->mac,4);
				if (result) {
					PRINT_ERR("was not able to delete " \
						  "multicast ip %08x/" \
						  "%02x%02x%02x%02x%02x%02x " \
						  "on irq x%x " \
						  "(result: 0x%x), " \
						  "trying to continue\n",
						  *((__u32*)&addr->ip[0]),
						  addr->mac[0],addr->mac[1],
						  addr->mac[2],addr->mac[3],
						  addr->mac[4],addr->mac[5],
						  card->irq0,result);
					sprintf(dbf_text,"smdl%4x",result);
					QETH_DBF_TEXT3(0,trace,dbf_text);
				}
			}
			addr=addr->next;
		}
		
		addr=card->ip_mc_new_state.ipm_ifa;
		while (addr) {
			if (!qeth_is_ipma_in_list(addr,card->
						  ip_mc_current_state.
						  ipm_ifa)) {
				QETH_DBF_TEXT3(0,trace,"setimset");
				sprintf(dbf_text,"%08x",
					*((__u32*)&addr->ip[0]));
				QETH_DBF_TEXT3(0,trace,dbf_text);
				*((__u32*)(&dbf_text[0]))=
					*((__u32*)&addr->mac);
				*((__u32*)(&dbf_text[4]))=
					*(((__u32*)&addr->mac)+1);
				QETH_DBF_HEX3(0,trace,dbf_text,
					      QETH_DBF_TRACE_LEN);
				result=qeth_send_setipm(
					card,(__u8*)&addr->ip[0],
					(__u8*)addr->mac,4,
					use_setipm_retries);
				if (result) {
					PRINT_ERR("was not able to set " \
						  "multicast ip %08x/" \
						  "%02x%02x%02x%02x%02x%02x " \
						  "on irq x%x " \
						  "(result: 0x%x), " \
						  "trying to continue\n",
						  *((__u32*)&addr->ip[0]),
						  addr->mac[0],addr->mac[1],
						  addr->mac[2],addr->mac[3],
						  addr->mac[4],addr->mac[5],
						  card->irq0,result);
					sprintf(dbf_text,"smst%4x",result);
					QETH_DBF_TEXT3(0,trace,dbf_text);
					qeth_remove_mc_ifa_from_list(
						&card->ip_mc_current_state.
						ipm_ifa,addr);
				}
			}
			addr=addr->next;
		}

#ifdef QETH_IPV6
		/* here we go with IPv6 */
		addr=card->ip_mc_current_state.ipm6_ifa;
		while (addr) {
			if (!qeth_is_ipma_in_list(addr,card->
						  ip_mc_new_state.ipm6_ifa)) {
				QETH_DBF_TEXT3(0,trace,"setimdl6");
				QETH_DBF_HEX3(0,trace,&addr->ip[0],
					      QETH_DBF_TRACE_LEN);
				QETH_DBF_HEX3(0,trace,(&addr->ip[0])+
					      QETH_DBF_TRACE_LEN,
					      QETH_DBF_TRACE_LEN);
				QETH_DBF_HEX3(0,trace,&addr->mac,
					      QETH_DBF_TRACE_LEN);
				result=qeth_send_delipm(
					card,(__u8*)&addr->ip[0],
					(__u8*)addr->mac,6);
				if (result) {
					PRINT_ERR("was not able to delete " \
						  "multicast ip %04x:%04x:" \
						  "%04x:%04x:%04x:%04x:" \
						  "%04x:%04x/" \
						  "%02x%02x%02x%02x%02x%02x " \
						  "on irq x%x " \
						  "(result: 0x%x), " \
						  "trying to continue\n",
						  *((__u16*)&addr->ip[0]),
						  *((__u16*)&addr->ip[2]),
						  *((__u16*)&addr->ip[4]),
						  *((__u16*)&addr->ip[6]),
						  *((__u16*)&addr->ip[8]),
						  *((__u16*)&addr->ip[10]),
						  *((__u16*)&addr->ip[12]),
						  *((__u16*)&addr->ip[14]),
						  addr->mac[0],addr->mac[1],
						  addr->mac[2],addr->mac[3],
						  addr->mac[4],addr->mac[5],
						  card->irq0,result);
					sprintf(dbf_text,"smd6%4x",result);
					QETH_DBF_TEXT3(0,trace,dbf_text);
				}
			}
			addr=addr->next;
		}
		
		addr=card->ip_mc_new_state.ipm6_ifa;
		while (addr) {
			if (!qeth_is_ipma_in_list(addr,card->
						  ip_mc_current_state.
						  ipm6_ifa)) {
				QETH_DBF_TEXT3(0,trace,"setimst6");
				QETH_DBF_HEX3(0,trace,&addr->ip[0],
					      QETH_DBF_TRACE_LEN);
				QETH_DBF_HEX3(0,trace,(&addr->ip[0])+
					      QETH_DBF_TRACE_LEN,
					      QETH_DBF_TRACE_LEN);
				QETH_DBF_HEX3(0,trace,&addr->mac,
					      QETH_DBF_TRACE_LEN);
				result=qeth_send_setipm(
					card,(__u8*)&addr->ip[0],
					(__u8*)addr->mac,6,
					use_setipm_retries);
				if (result) {
					PRINT_ERR("was not able to set " \
						  "multicast ip %04x:%04x:" \
						  "%04x:%04x:%04x:%04x:" \
						  "%04x:%04x/" \
						  "%02x%02x%02x%02x%02x%02x " \
						  "on irq x%x " \
						  "(result: 0x%x), " \
						  "trying to continue\n",
						  *((__u16*)&addr->ip[0]),
						  *((__u16*)&addr->ip[2]),
						  *((__u16*)&addr->ip[4]),
						  *((__u16*)&addr->ip[6]),
						  *((__u16*)&addr->ip[8]),
						  *((__u16*)&addr->ip[10]),
						  *((__u16*)&addr->ip[12]),
						  *((__u16*)&addr->ip[14]),
						  addr->mac[0],addr->mac[1],
						  addr->mac[2],addr->mac[3],
						  addr->mac[4],addr->mac[5],
						  card->irq0,result);
					sprintf(dbf_text,"sms6%4x",result);
					QETH_DBF_TEXT3(0,trace,dbf_text);
					qeth_remove_mc_ifa_from_list(
						&card->ip_mc_current_state.
						ipm6_ifa,addr);
				}
			}
			addr=addr->next;
		}
#endif /* QETH_IPV6 */
                return 0;
        } else return 0;
}

static void qeth_clone_ifa(struct in_ifaddr *src,struct in_ifaddr *dest)
{
	memcpy(dest,src,sizeof(struct in_ifaddr));
	dest->ifa_next=NULL;
}

#ifdef QETH_IPV6
static void qeth_clone_ifa6(struct inet6_ifaddr *src,
			    struct inet6_ifaddr *dest)
{
	memcpy(dest,src,sizeof(struct inet6_ifaddr));
	dest->if_next=NULL;
}
#endif /* QETH_IPV6 */

#define QETH_STANDARD_RETVALS \
		ret_val=-EIO; \
		if (result==IPA_REPLY_SUCCESS) ret_val=0; \
		if (result==IPA_REPLY_FAILED) ret_val=-EIO; \
		if (result==IPA_REPLY_OPNOTSUPP) ret_val=-EOPNOTSUPP

static int qeth_do_ioctl(struct net_device *dev,struct ifreq *rq,int cmd)
{
	char *data;
	int result,i,ret_val;
	int version=4;
	qeth_card_t *card;
	char dbf_text[15];
	char buff[100];

	card=(qeth_card_t*)dev->priv;

	sprintf(dbf_text,"ioct%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	sprintf(dbf_text,"cmd=%4x",cmd);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	QETH_DBF_HEX2(0,trace,&rq,sizeof(void*));

	if ((cmd<SIOCDEVPRIVATE) || (cmd>SIOCDEVPRIVATE+5))
		return -EOPNOTSUPP;
	copy_from_user(buff,rq->ifr_ifru.ifru_data,sizeof(buff));
	data=buff;

	if ( (!atomic_read(&card->is_registered))||
	     (!atomic_read(&card->is_hardsetup))||
	     (atomic_read(&card->is_gone)) ) return -ENODEV;

	if (atomic_read(&card->shutdown_phase)) return -ENODEV;

	my_spin_lock(&card->ioctl_lock);

	if (atomic_read(&card->shutdown_phase)) return -ENODEV;
	if ( (!atomic_read(&card->is_registered))||
	     (!atomic_read(&card->is_hardsetup))||
	     (atomic_read(&card->is_gone)) ) {
		ret_val=-ENODEV;
		goto out;
	}

	switch (cmd) {
	case SIOCDEVPRIVATE+0:
		if (!capable(CAP_NET_ADMIN)) {
			ret_val=-EPERM;
			break;
		}
		result=qeth_send_setassparms(card,version,IPA_ARP_PROCESSING,
					     IPA_CMD_ASS_ARP_SET_NO_ENTRIES,
					     rq->ifr_ifru.ifru_ivalue,4);
		QETH_STANDARD_RETVALS;
		if (result==3) ret_val=-EINVAL;
		break;
	case SIOCDEVPRIVATE+1:
		if (!capable(CAP_NET_ADMIN)) {
			ret_val=-EPERM;
			break;
		}
		result = qeth_queryarp(card,rq,version,IPA_ARP_PROCESSING,
				       IPA_CMD_ASS_ARP_QUERY_INFO,data,4);

		QETH_STANDARD_RETVALS;
		break;
	case SIOCDEVPRIVATE+2:
		if (!capable(CAP_NET_ADMIN)) {
			ret_val=-EPERM;
			break;
		}
		for (i=12;i<24;i++) if (data[i]) version=6;
		result=qeth_send_setassparms(card,version,IPA_ARP_PROCESSING,
					     IPA_CMD_ASS_ARP_ADD_ENTRY,
					     (long)data,56);
		QETH_STANDARD_RETVALS;
		break;
	case SIOCDEVPRIVATE+3:
		if (!capable(CAP_NET_ADMIN)) {
			ret_val=-EPERM;
			break;
		}
		for (i=4;i<12;i++) if (data[i]) version=6;
		result=qeth_send_setassparms(card,version,IPA_ARP_PROCESSING,
					     IPA_CMD_ASS_ARP_REMOVE_ENTRY,
					     (long)data,16);
		QETH_STANDARD_RETVALS;
		break;
	case SIOCDEVPRIVATE+4:
		if (!capable(CAP_NET_ADMIN)) {
			ret_val=-EPERM;
			break;
		}
		result=qeth_send_setassparms(card,version,IPA_ARP_PROCESSING,
					     IPA_CMD_ASS_ARP_FLUSH_CACHE,
					     0,0);
		QETH_STANDARD_RETVALS;
		break;
	case SIOCDEVPRIVATE+5:
		result=qeth_send_snmp_control(card,rq,IPA_CMD_SETADAPTERPARMS,
					      IPA_SETADP_SET_SNMP_CONTROL,
					      data,4);
		QETH_STANDARD_RETVALS;
		break;

	default:
		return -EOPNOTSUPP;
	}
out:
	my_spin_unlock(&card->ioctl_lock);

	sprintf(dbf_text,"ret=%4x",ret_val);
	QETH_DBF_TEXT2(0,trace,dbf_text);

	return ret_val;
}

static void qeth_clear_ifamc_list(struct qeth_ipm_mac **ifa_list)
{
	struct qeth_ipm_mac *ifa;
	while (*ifa_list) {
		ifa=*ifa_list;
		*ifa_list=ifa->next;
		kfree(ifa);
	}
}

#ifdef QETH_IPV6
static void qeth_clear_ifa6_list(struct inet6_ifaddr **ifa_list)
{
	struct inet6_ifaddr *ifa;
	while (*ifa_list) {
		ifa=*ifa_list;
		*ifa_list=ifa->if_next;
		kfree(ifa);
	}
}

static void qeth_takeover_ip_ipms6(qeth_card_t *card)
{
	struct inet6_ifaddr *ifa,*ifanew;
	char dbf_text[15];
	int remove;
#ifdef QETH_VLAN
        struct vlan_group *card_group;
	int i;
#endif

	struct qeth_ipm_mac *ipmanew;
        struct ifmcaddr6 *im6;
        struct inet6_dev *in6_dev;
#ifdef QETH_VLAN
        struct inet6_dev *in6_vdev;
#endif
	char buf[MAX_ADDR_LEN];

	sprintf(dbf_text,"tip6%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);
	/* unicast */
	/* clear ip_current_state */
	qeth_clear_ifa6_list(&card->ip_current_state.ip6_ifa);
	/* take it over */
	card->ip_current_state.ip6_ifa=card->ip_new_state.ip6_ifa;
	card->ip_new_state.ip6_ifa=NULL;

	/* multicast */
	/* clear ip_mc_current_state */
	qeth_clear_ifamc_list(&card->ip_mc_current_state.ipm6_ifa);
	/* take it over */
	card->ip_mc_current_state.ipm6_ifa=card->ip_mc_new_state.ipm6_ifa;
	/* get new one, we try to have the same order as ifa_list in device
	   structure, for what reason ever*/
	card->ip_mc_new_state.ipm6_ifa=NULL;

        if((in6_dev=in6_dev_get(card->dev))==NULL) {
		sprintf(dbf_text,"id16%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
                goto out;
	}
        read_lock(&in6_dev->lock);

	/* get new one, we try to have the same order as ifa_list in device
	   structure, for what reason ever*/
	QETH_DBF_TEXT4(0,trace,"to-ip6s");
	if ( (atomic_read(&card->is_open)) && (card->dev->ip6_ptr) &&
	     (((struct inet6_dev*)card->dev->ip6_ptr)->addr_list) ) {
		ifa=((struct inet6_dev*)card->dev->ip6_ptr)->addr_list;

		while (ifa) {
			ifanew=kmalloc(sizeof(struct inet6_ifaddr),GFP_KERNEL);
			if (!ifanew) {
				PRINT_WARN("No memory for IP address " \
					   "handling. Some of the IPs " \
					   "will not be set on %s.\n",
					   card->dev_name);
				QETH_DBF_TEXT2(0,trace,"TOIPNMEM");
			} else {
				qeth_clone_ifa6(ifa,ifanew);
				remove=qeth_add_ifa6_to_list(
					&card->ip_new_state.ip6_ifa,ifanew);
				QETH_DBF_HEX4(0,trace,&ifanew->addr.s6_addr,
					      QETH_DBF_TRACE_LEN);
				QETH_DBF_HEX4(0,trace,&ifanew->addr.s6_addr+
					      QETH_DBF_TRACE_LEN,
					      QETH_DBF_TRACE_LEN);
				sprintf(dbf_text,"pref%4u",ifanew->prefix_len);
				QETH_DBF_TEXT4(0,trace,dbf_text);
				if (remove) {
					kfree(ifanew);
					QETH_DBF_TEXT4(0,trace,"alrdy6rm");
				}
			}
			ifa=ifa->if_next;
		}
	}
#ifdef QETH_VLAN
/*append all known VLAN IP Addresses corresponding 
  to the real device card->dev->ifindex 
*/
	QETH_DBF_TEXT4(0,trace,"to-vip6s");
	if ( (qeth_is_supported(IPA_FULL_VLAN)) &&
	     (atomic_read(&card->is_open)) ) {
		card_group = (struct vlan_group *) card->vlangrp;
		if (card_group) for (i=0;i<VLAN_GROUP_ARRAY_LEN;i++) {
			if ( (card_group->vlan_devices[i]) &&
			     (card_group->vlan_devices[i]->flags&IFF_UP)&&
			     ((struct inet6_dev *) card_group->
			      vlan_devices[i]->ip6_ptr) ) {
				ifa=((struct inet6_dev *) 
				     card_group->vlan_devices[i]->ip6_ptr)->
					addr_list;
				
		while (ifa) {
			ifanew=kmalloc(sizeof(struct inet6_ifaddr),GFP_KERNEL);
			if (!ifanew) {
				PRINT_WARN("No memory for IP address " \
					   "handling. Some of the IPs " \
					   "will not be set on %s.\n",
					   card->dev_name);
				QETH_DBF_TEXT2(0,trace,"TOIPNMEM");
			} else {
				qeth_clone_ifa6(ifa,ifanew);
				remove=qeth_add_ifa6_to_list
					(&card->ip_new_state.ip6_ifa,ifanew);
				QETH_DBF_HEX4(0,trace,&ifanew->addr.s6_addr,
					      QETH_DBF_TRACE_LEN);
				QETH_DBF_HEX4(0,trace,&ifanew->addr.s6_addr+
					      QETH_DBF_TRACE_LEN,
					      QETH_DBF_TRACE_LEN);
				sprintf(dbf_text,"pref%4u",ifanew->prefix_len);
				QETH_DBF_TEXT4(0,trace,dbf_text);
				if (remove) {
					kfree(ifanew);
					QETH_DBF_TEXT4(0,trace,"alrdv6rm");
				}
			}
			ifa=ifa->if_next;
		}
			}
		}
	}
#endif
	
	QETH_DBF_TEXT4(0,trace,"to-ipm6s");
	if (atomic_read(&card->is_open))
	for (im6=in6_dev->mc_list;im6;im6=im6->next) {
		ndisc_mc_map(&im6->mca_addr,buf,card->dev,0);
		ipmanew=(struct qeth_ipm_mac*)kmalloc(
			sizeof(struct qeth_ipm_mac),GFP_KERNEL);
		if (!ipmanew) {
			PRINT_WARN("No memory for IPM address " \
				   "handling. Multicast IP " \
				   "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x" \
				   "will not be set on %s.\n",
				   im6->mca_addr.s6_addr16[0],
				   im6->mca_addr.s6_addr16[1],
				   im6->mca_addr.s6_addr16[2],
				   im6->mca_addr.s6_addr16[3],
				   im6->mca_addr.s6_addr16[4],
				   im6->mca_addr.s6_addr16[5],
				   im6->mca_addr.s6_addr16[6],
				   im6->mca_addr.s6_addr16[7],
				   card->dev_name);
			QETH_DBF_TEXT2(0,trace,"TOIPMNMM");
		} else {
			memset(ipmanew,0,sizeof(struct qeth_ipm_mac));
			memcpy(ipmanew->mac,buf,OSA_ADDR_LEN);
			memcpy(ipmanew->ip,im6->mca_addr.s6_addr,16);
			ipmanew->next=NULL;
			remove=qeth_add_mc_ifa_to_list(
				&card->ip_mc_new_state.ipm6_ifa,ipmanew);
			QETH_DBF_HEX4(0,trace,&ipmanew->ip,
				      QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX4(0,trace,&ipmanew->ip+
				      QETH_DBF_TRACE_LEN,
				      QETH_DBF_TRACE_LEN);
			QETH_DBF_HEX4(0,trace,&ipmanew->mac,
				      QETH_DBF_TRACE_LEN);
			if (remove) {
				QETH_DBF_TEXT4(0,trace,"mlrdy6rm");
				kfree(ipmanew);
			}
		}
	}
#ifdef QETH_VLAN
	QETH_DBF_TEXT4(0,trace,"tovipm6s");
	if ( (qeth_is_supported(IPA_FULL_VLAN)) &&
	     (atomic_read(&card->is_open)) ) {
		card_group = (struct vlan_group *) card->vlangrp;
		if (card_group) for (i=0;i<VLAN_GROUP_ARRAY_LEN;i++) {
			if ((card_group->vlan_devices[i])&&
			    (card_group->vlan_devices[i]->flags&IFF_UP)) {
				in6_vdev=in6_dev_get(card_group->
						     vlan_devices[i]);
				if(!(in6_vdev==NULL)) {
					read_lock(&in6_vdev->lock);
		for (im6=in6_vdev->mc_list;
		     im6;im6=im6->next) {
			ndisc_mc_map(&im6->mca_addr,
				     buf,card_group->vlan_devices[i],
				     0);
			ipmanew=(struct qeth_ipm_mac*)
				kmalloc(sizeof(struct qeth_ipm_mac),
					GFP_KERNEL);
			if (!ipmanew) {
				PRINT_WARN("No memory for IPM address " \
					   "handling. Multicast IP " \
					   "%04x:%04x:%04x:%04x:" \
					   "%04x:%04x:%04x:%04x" \
					   "will not be set on %s.\n",
					   im6->mca_addr.s6_addr16[0],
					   im6->mca_addr.s6_addr16[1],
					   im6->mca_addr.s6_addr16[2],
					   im6->mca_addr.s6_addr16[3],
					   im6->mca_addr.s6_addr16[4],
					   im6->mca_addr.s6_addr16[5],
					   im6->mca_addr.s6_addr16[6],
					   im6->mca_addr.s6_addr16[7],
					   card->dev_name);
				QETH_DBF_TEXT2(0,trace,"TOIPMNMM");
			} else {
				memset(ipmanew,0,
				       sizeof(struct qeth_ipm_mac));
				memcpy(ipmanew->mac,buf,OSA_ADDR_LEN);
				memcpy(ipmanew->ip,
				       im6->mca_addr.s6_addr,16);
				ipmanew->next=NULL;
				remove=qeth_add_mc_ifa_to_list
					(&card->ip_mc_new_state.ipm6_ifa,
					 ipmanew);
				QETH_DBF_HEX4(0,trace,&ipmanew->ip,
					      QETH_DBF_TRACE_LEN);
				QETH_DBF_HEX4(0,trace,&ipmanew->ip+
					      QETH_DBF_TRACE_LEN,
					      QETH_DBF_TRACE_LEN);
				QETH_DBF_HEX4(0,trace,&ipmanew->mac,
					      QETH_DBF_TRACE_LEN);
				
				if (remove) {
					QETH_DBF_TEXT4(0,trace,"mlrdv6rm");
					kfree(ipmanew);
				}
			}
		}
					read_unlock(&in6_vdev->lock);
					in6_dev_put(in6_vdev);
				} else {
					sprintf(dbf_text,"id26%4x",card->irq0);
					QETH_DBF_TEXT2(0,trace,dbf_text);
				}
			}
		}
	}
#endif
        read_unlock(&in6_dev->lock);
        in6_dev_put(in6_dev);
 out:
	;
}
#endif /* QETH_IPV6 */

static void qeth_clear_ifa4_list(struct in_ifaddr **ifa_list)
{
	struct in_ifaddr *ifa;
	while (*ifa_list) {
		ifa=*ifa_list;
		*ifa_list=ifa->ifa_next;
		kfree(ifa);
	}
}

static void qeth_takeover_ip_ipms(qeth_card_t *card)
{
	struct in_ifaddr *ifa,*ifanew;
	char dbf_text[15];
	int remove;
#ifdef QETH_VLAN
	struct vlan_group *card_group;
	int i;
        struct in_device        *vin4_dev;
#endif

	struct qeth_ipm_mac *ipmanew;
        struct ip_mc_list       *im4;
        struct in_device        *in4_dev;
	char buf[MAX_ADDR_LEN];
	__u32 maddr;

	sprintf(dbf_text,"tips%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);
	/* unicast */
	/* clear ip_current_state */
	qeth_clear_ifa4_list(&card->ip_current_state.ip_ifa);
	/* take it over */
	card->ip_current_state.ip_ifa=card->ip_new_state.ip_ifa;
	card->ip_new_state.ip_ifa=NULL;

	/* multicast */
	/* clear ip_mc_current_state */
	qeth_clear_ifamc_list(&card->ip_mc_current_state.ipm_ifa);
	/* take it over */
	card->ip_mc_current_state.ipm_ifa=card->ip_mc_new_state.ipm_ifa;
	/* get new one, we try to have the same order as ifa_list in device
	   structure, for what reason ever*/
	card->ip_mc_new_state.ipm_ifa=NULL;

        if((in4_dev=in_dev_get(card->dev))==NULL) {
		QETH_DBF_TEXT2(0,trace,"nodvhol1");
		QETH_DBF_TEXT2(0,trace,card->dev_name);
                return;
	}
        read_lock(&in4_dev->lock);

	/* get new one, we try to have the same order as ifa_list in device
	   structure, for what reason ever*/
	QETH_DBF_TEXT4(0,trace,"to-ips");
	if ( (atomic_read(&card->is_open)) && (card->dev->ip_ptr) &&
	     (((struct in_device*)card->dev->ip_ptr)->ifa_list) ) {
		ifa=((struct in_device*)card->dev->ip_ptr)->ifa_list;

		while (ifa) {
			ifanew=kmalloc(sizeof(struct in_ifaddr),GFP_KERNEL);
			if (!ifanew) {
				PRINT_WARN("No memory for IP address " \
					   "handling. Some of the IPs " \
					   "will not be set on %s.\n",
					   card->dev_name);
				QETH_DBF_TEXT2(0,trace,"TOIPNMEM");
			} else {
				qeth_clone_ifa(ifa,ifanew);
				remove=qeth_add_ifa_to_list(
					&card->ip_new_state.ip_ifa,ifanew);
				*((__u32*)(&dbf_text[0]))=
					*((__u32*)&ifanew->ifa_address);
				*((__u32*)(&dbf_text[4]))=
					*((__u32*)&ifanew->ifa_mask);
				QETH_DBF_TEXT4(0,trace,dbf_text);
				if (remove) {
					kfree(ifanew);
					QETH_DBF_TEXT4(0,trace,"alrdy4rm");
				}
			}
			
			ifa=ifa->ifa_next;
		}
	}

#ifdef QETH_VLAN
	/* append all known VLAN IP Addresses corresponding to the
	 * real device card->dev->ifindex */
	QETH_DBF_TEXT4(0,trace,"to-vips");
	if ( (qeth_is_supported(IPA_FULL_VLAN)) &&
	     (atomic_read(&card->is_open)) ) {
		card_group = (struct vlan_group *) card->vlangrp;
                if (card_group) {
		    for (i=0;i<VLAN_GROUP_ARRAY_LEN;i++) {
			if ((vin4_dev=in_dev_get(card->dev))) {
				read_lock(&vin4_dev->lock);
			if ((card_group->vlan_devices[i])&&
			    (card_group->vlan_devices[i]->flags&IFF_UP)) {
			    ifa=((struct in_device*)
				 card_group->vlan_devices[i]->ip_ptr)->
				    ifa_list;
			    while (ifa) {
				ifanew=kmalloc(sizeof(struct in_ifaddr),
					       GFP_KERNEL);
				if (!ifanew) {
				    PRINT_WARN("No memory for IP address " \
					       "handling. Some of the IPs " \
					       "will not be set on %s.\n",
					       card->dev_name);
				    QETH_DBF_TEXT2(0,trace,"TOIPNMEM");
				} else {
					qeth_clone_ifa(ifa,ifanew);
					remove=qeth_add_ifa_to_list(
						&card->ip_new_state.ip_ifa,
						ifanew);
					*((__u32*)(&dbf_text[0]))=
						*((__u32*)&ifanew->
						  ifa_address);
					*((__u32*)(&dbf_text[4]))=
						*((__u32*)&ifanew->ifa_mask);
					QETH_DBF_TEXT4(0,trace,dbf_text);
					if (remove) {
						kfree(ifanew);
						QETH_DBF_TEXT4(0,trace,
							       "alrdv4rm");
					}
				}
				ifa=ifa->ifa_next;
			    }
			}
			read_unlock(&vin4_dev->lock);
			in_dev_put(vin4_dev);
			} else {
				QETH_DBF_TEXT2(0,trace,"nodvhol2");
				QETH_DBF_TEXT2(0,trace,card->dev_name);
			}
		    }
		}
	}
#endif /* QETH_VLAN */

	QETH_DBF_TEXT4(0,trace,"to-ipms");
	if (atomic_read(&card->is_open))
	for (im4=in4_dev->mc_list;im4;im4=im4->next) {
		qeth_get_mac_for_ipm(im4->multiaddr,buf,in4_dev->dev);
		ipmanew=(struct qeth_ipm_mac*)kmalloc(
			sizeof(struct qeth_ipm_mac),GFP_KERNEL);
		if (!ipmanew) {
			PRINT_WARN("No memory for IPM address " \
				   "handling. Multicast IP %08x" \
				   "will not be set on %s.\n",
				   (__u32)im4->multiaddr,
				   card->dev_name);
			QETH_DBF_TEXT2(0,trace,"TOIPMNMM");
		} else {
			memset(ipmanew,0,sizeof(struct qeth_ipm_mac));
			memcpy(ipmanew->mac,buf,OSA_ADDR_LEN);
			maddr=im4->multiaddr;
			memcpy(&(ipmanew->ip[0]),&maddr,4);
			memset(&(ipmanew->ip[4]),0xff,12);
			ipmanew->next=NULL;
			remove=qeth_add_mc_ifa_to_list(
				&card->ip_mc_new_state.ipm_ifa,ipmanew);
			sprintf(dbf_text,"%08x",*((__u32*)&ipmanew->ip));
			QETH_DBF_TEXT4(0,trace,dbf_text);
			QETH_DBF_HEX4(0,trace,&ipmanew->mac,
				      QETH_DBF_TRACE_LEN);
			if (remove) {
				QETH_DBF_TEXT4(0,trace,"mlrdy4rm");
				kfree(ipmanew);
			}
		}
	}

#ifdef QETH_VLAN
	QETH_DBF_TEXT4(0,trace,"to-vipms");
	if ( (qeth_is_supported(IPA_FULL_VLAN)) &&
	     (atomic_read(&card->is_open)) ) {
		card_group = (struct vlan_group *) card->vlangrp;
                if (card_group) for (i=0;i<VLAN_GROUP_ARRAY_LEN;i++) {
			if ((card_group->vlan_devices[i])&&
			    (card_group->vlan_devices[i]->flags&IFF_UP)) {
				if ((vin4_dev=in_dev_get(card_group->
				 			 vlan_devices[i]))) {
					read_lock(&vin4_dev->lock);
		for (im4=vin4_dev->mc_list;im4;im4=im4->next) {
			qeth_get_mac_for_ipm(im4->multiaddr,buf,vin4_dev->dev);
			ipmanew=(struct qeth_ipm_mac*)kmalloc(
				sizeof(struct qeth_ipm_mac),GFP_KERNEL);
			if (!ipmanew) {
				PRINT_WARN("No memory for IPM address " \
					   "handling. Multicast VLAN IP %08x" \
					   "will not be set on %s.\n",
					   (__u32)im4->multiaddr,
					   card->dev_name);
				QETH_DBF_TEXT2(0,trace,"TOIPMNMM");
			} else {
				memset(ipmanew,0,sizeof(struct qeth_ipm_mac));
				memcpy(ipmanew->mac,buf,OSA_ADDR_LEN);
				maddr=im4->multiaddr;
				memcpy(&(ipmanew->ip[0]),&maddr,4);
				memset(&(ipmanew->ip[4]),0xff,12);
				ipmanew->next=NULL;
				remove=qeth_add_mc_ifa_to_list(
				       &card->ip_mc_new_state.ipm_ifa,
				       ipmanew);
				sprintf(dbf_text,"%08x",
					*((__u32*)&ipmanew->ip));
				QETH_DBF_TEXT4(0,trace,dbf_text);
				QETH_DBF_HEX4(0,trace,&ipmanew->mac,
					      QETH_DBF_TRACE_LEN);
				if (remove) {
					QETH_DBF_TEXT4(0,trace,"mlrdv4rm");
					kfree(ipmanew);
				}
			}
		}
					read_unlock(&vin4_dev->lock);
					in_dev_put(vin4_dev);
				} else {
					QETH_DBF_TEXT2(0,trace,"novdhol3");
					QETH_DBF_TEXT2(0,trace,card->dev_name);
					QETH_DBF_TEXT2(0,trace,card_group->
						       vlan_devices[i]->name);
				}
			    }
			}
	}
#endif /* QETH_VLAN */

        read_unlock(&in4_dev->lock);
        in_dev_put(in4_dev);
}

static void qeth_get_unique_id(qeth_card_t *card)
{
#ifdef QETH_IPV6
#ifdef CONFIG_SHARED_IPV6_CARDS
        ipa_cmd_t cmd;
	int result;
	char dbf_text[15];

	if (!qeth_is_supported(IPA_IPv6)) {
		card->unique_id=UNIQUE_ID_IF_CREATE_ADDR_FAILED|
			UNIQUE_ID_NOT_BY_CARD;
		return;
	}
        qeth_fill_ipa_cmd(card,&cmd,IPA_CMD_CREATE_ADDR,6);

	*((__u16*)&cmd.data.create_destroy_addr.unique_id[6])=card->unique_id;

	result=qeth_send_ipa_cmd(card,&cmd,1,IPA_CMD_STATE);

	if (result) {
		card->unique_id=UNIQUE_ID_IF_CREATE_ADDR_FAILED|
			UNIQUE_ID_NOT_BY_CARD;
		PRINT_WARN("couldn't get a unique id from the card on irq " \
			   "x%x (result=x%x), using default id. ipv6 " \
			   "autoconfig on other lpars may lead to duplicate " \
			   "ip addresses. please use manually " \
			   "configured ones.\n",card->irq0,result);
		QETH_DBF_TEXT2(0,trace,"unid fld");
		sprintf(dbf_text,"%4x%4x",card->irq0,result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
	} else { 
		card->unique_id=
			*((__u16*)&cmd.data.create_destroy_addr.unique_id[6]);
		QETH_DBF_TEXT2(0,setup,"uniqueid");
		sprintf(dbf_text,"%4x%4x",card->irq0,card->unique_id);
		QETH_DBF_TEXT2(0,setup,dbf_text);
	}
#else /* CONFIG_SHARED_IPV6_CARDS */
	card->unique_id=UNIQUE_ID_IF_CREATE_ADDR_FAILED|UNIQUE_ID_NOT_BY_CARD;
#endif /* CONFIG_SHARED_IPV6_CARDS */
#else /* QETH_IPV6 */
	card->unique_id=UNIQUE_ID_IF_CREATE_ADDR_FAILED|UNIQUE_ID_NOT_BY_CARD;
#endif /* QETH_IPV6 */
}

static void qeth_put_unique_id(qeth_card_t *card)
{
#ifdef QETH_IPV6
#ifdef CONFIG_SHARED_IPV6_CARDS
        ipa_cmd_t cmd;
	int result;
	char dbf_text[15];

	/* is also true, if ipv6 is not supported on the card */
	if ((card->unique_id&UNIQUE_ID_NOT_BY_CARD)==UNIQUE_ID_NOT_BY_CARD)
		return;

        qeth_fill_ipa_cmd(card,&cmd,IPA_CMD_DESTROY_ADDR,6);
	*((__u16*)&cmd.data.create_destroy_addr.unique_id[6])=card->unique_id;
	memcpy(&cmd.data.create_destroy_addr.unique_id[0],
	       card->dev->dev_addr,OSA_ADDR_LEN);

	result=qeth_send_ipa_cmd(card,&cmd,1,IPA_CMD_STATE);

	if (result) {
		QETH_DBF_TEXT2(0,trace,"unibkfld");
		sprintf(dbf_text,"%4x%4x",card->irq0,result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
	}
#else /* CONFIG_SHARED_IPV6_CARDS */
	card->unique_id=UNIQUE_ID_IF_CREATE_ADDR_FAILED|UNIQUE_ID_NOT_BY_CARD;
#endif /* CONFIG_SHARED_IPV6_CARDS */
#else /* QETH_IPV6 */
	card->unique_id=UNIQUE_ID_IF_CREATE_ADDR_FAILED|UNIQUE_ID_NOT_BY_CARD;
#endif /* QETH_IPV6 */
}

static void qeth_do_setadapterparms_stuff(qeth_card_t *card)
{
	int result;
	char dbf_text[15];

	if (!qeth_is_supported(IPA_SETADAPTERPARMS)) {
		return;
	}

	sprintf(dbf_text,"stap%4x",card->irq0);
	QETH_DBF_TEXT4(0,trace,dbf_text);

	result=qeth_send_setadapterparms_query(card);

	if (result) {
		PRINT_WARN("couldn't set adapter parameters on irq 0x%x: " \
			   "x%x\n",card->irq0,result);
		QETH_DBF_TEXT1(0,trace,"SETADPFL");
		sprintf(dbf_text,"%4x%4x",card->irq0,result);
		QETH_DBF_TEXT1(1,trace,dbf_text);
		return;
	}

	sprintf(dbf_text,"spap%4x",card->adp_supported);
	QETH_DBF_TEXT2(0,trace,dbf_text);

	if (qeth_is_adp_supported(IPA_SETADP_ALTER_MAC_ADDRESS)) {
		sprintf(dbf_text,"rdmc%4x",card->irq0);
		QETH_DBF_TEXT3(0,trace,dbf_text);
		QETH_DBF_TEXT2(0,setup,dbf_text);

		result=qeth_send_setadapterparms_change_addr(card,
			IPA_SETADP_ALTER_MAC_ADDRESS,
			CHANGE_ADDR_READ_MAC,card->dev->dev_addr,
			OSA_ADDR_LEN);
		if (result) {
			PRINT_WARN("couldn't get MAC address on " \
				   "irq 0x%x: x%x\n",card->irq0,result);
			QETH_DBF_TEXT1(0,trace,"NOMACADD");
			sprintf(dbf_text,"%4x%4x",card->irq0,result);
			QETH_DBF_TEXT1(1,trace,dbf_text);
		} else {
			QETH_DBF_HEX2(0,setup,card->dev->dev_addr,
				      __max(OSA_ADDR_LEN,QETH_DBF_SETUP_LEN));
			QETH_DBF_HEX3(0,trace,card->dev->dev_addr,
				      __max(OSA_ADDR_LEN,QETH_DBF_TRACE_LEN));
		}
	}

	if ( (card->link_type==QETH_MPC_LINK_TYPE_HSTR) ||
	     (card->link_type==QETH_MPC_LINK_TYPE_LANE_TR) ) {
		sprintf(dbf_text,"hstr%4x",card->irq0);
		QETH_DBF_TEXT3(0,trace,dbf_text);

		if (qeth_is_adp_supported(IPA_SETADP_SET_BROADCAST_MODE)) {
			result=qeth_send_setadapterparms_mode(card,
				IPA_SETADP_SET_BROADCAST_MODE,
				card->options.broadcast_mode);
			if (result) {
				PRINT_WARN("couldn't set broadcast mode on " \
					   "irq 0x%x: x%x\n",
					   card->irq0,result);
				QETH_DBF_TEXT1(0,trace,"STBRDCST");
				sprintf(dbf_text,"%4x%4x",card->irq0,result);
				QETH_DBF_TEXT1(1,trace,dbf_text);
			}
		} else if (card->options.broadcast_mode) {
			PRINT_WARN("set adapter parameters not available " \
				   "to set broadcast mode, using ALLRINGS " \
				   "on irq 0x%x:\n",card->irq0);
			sprintf(dbf_text,"NOBC%4x",card->irq0);
			QETH_DBF_TEXT1(0,trace,dbf_text);
		}

		if (qeth_is_adp_supported(IPA_SETADP_SET_BROADCAST_MODE)) {
			result=qeth_send_setadapterparms_mode(card,
				IPA_SETADP_ALTER_MAC_ADDRESS,
				card->options.macaddr_mode);
			if (result) {
				PRINT_WARN("couldn't set macaddr mode on " \
					   "irq 0x%x: x%x\n",
					   card->irq0,result);
				QETH_DBF_TEXT1(0,trace,"STMACMOD");
				sprintf(dbf_text,"%4x%4x",card->irq0,result);
				QETH_DBF_TEXT1(1,trace,dbf_text);
			}
		} else if (card->options.macaddr_mode) {
			PRINT_WARN("set adapter parameters not available " \
				   "to set macaddr mode, using NONCANONICAL " \
				   "on irq 0x%x:\n",card->irq0);
			sprintf(dbf_text,"NOMA%4x",card->irq0);
			QETH_DBF_TEXT1(0,trace,dbf_text);
		}
	}
}

static int qeth_softsetup_card(qeth_card_t *card,int wait_for_lock)
{
        int result;
	int use_setip_retries=1;
	char dbf_text[15];
	int do_a_startlan6=0;

	if (wait_for_lock==QETH_WAIT_FOR_LOCK) {
		my_spin_lock(&card->softsetup_lock);
	} else if (wait_for_lock==QETH_DONT_WAIT_FOR_LOCK) {
		if (!spin_trylock(&card->softsetup_lock)) {
			return -EAGAIN;
		}
	} else if (wait_for_lock==QETH_LOCK_ALREADY_HELD) {
		use_setip_retries=0; /* we are in recovery and don't want
					to repeat setting ips on and on */
	} else {
		return -EINVAL;
	}

	qeth_save_dev_flag_state(card);

	sprintf(dbf_text,"ssc%c%4x",wait_for_lock?'w':'n',card->irq0);
	QETH_DBF_TEXT1(0,trace,dbf_text);

        if (!atomic_read(&card->is_softsetup)) {
		atomic_set(&card->enable_routing_attempts4,
			   QETH_ROUTING_ATTEMPTS);
#ifdef QETH_IPV6
		atomic_set(&card->enable_routing_attempts6,
			   QETH_ROUTING_ATTEMPTS);
#endif /* QETH_IPV6 */
		if ( (!atomic_read(&card->is_startlaned)) &&
		     (atomic_read(&card->startlan_attempts)) ) {
			atomic_dec(&card->startlan_attempts);
			QETH_DBF_TEXT2(0,trace,"startlan");
			netif_stop_queue(card->dev);
			result=qeth_send_startlan(card,4);
			if (result) {
				PRINT_WARN("couldn't send STARTLAN on %s " \
					   "(CHPID 0x%X): 0x%x (%s)\n",
					   card->dev_name,card->chpid,result,
					   (result==0xe080)?
					   "startlan disabled (link " \
					   "failure -- please check the " \
					   "network, plug in the cable or " \
					   "enable the OSA port":
					   "unknown return code");
				sprintf(dbf_text,"stln%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				atomic_set(&card->is_softsetup,0);
				atomic_set(&card->is_startlaned,0);
				/* do not return an error */
				if (result==0xe080) {
					result=0;
				}
				goto out;
			}
			do_a_startlan6=1;
		}
		netif_wake_queue(card->dev);

		qeth_do_setadapterparms_stuff(card);

                if (!qeth_is_supported(IPA_ARP_PROCESSING)) {
                        PRINT_WARN("oops... ARP processing not supported " \
				   "on %s!\n",card->dev_name);
			QETH_DBF_TEXT1(0,trace,"NOarpPRC");
                } else {
			QETH_DBF_TEXT2(0,trace,"enaARPpr");
                        result=qeth_send_setassparms_simple_without_data(
                                card,IPA_ARP_PROCESSING,IPA_CMD_ASS_START);
                        if (result) {
                                PRINT_WARN("Could not start ARP processing " \
                                           "assist on %s: 0x%x\n",
					   card->dev_name,result);
				sprintf(dbf_text,"ARPp%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				atomic_set(&card->is_softsetup,0);
                                goto out;
                        }
                }

                if (qeth_is_supported(IPA_IP_FRAGMENTATION)) {
                        PRINT_INFO("IP fragmentation supported on " \
				   "%s... :-)\n",card->dev_name);
			QETH_DBF_TEXT2(0,trace,"enaipfrg");
                        result=qeth_send_setassparms_simple_without_data(
                                card,IPA_IP_FRAGMENTATION,IPA_CMD_ASS_START);
                        if (result) {
                                PRINT_WARN("Could not start IP fragmenting " \
                                           "assist on %s: 0x%x, continuing\n",
					   card->dev_name,result);
				sprintf(dbf_text,"IFRG%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				/* go on */
                        }
                }

		if (card->options.fake_ll==FAKE_LL) {
			if (qeth_is_supported(IPA_SOURCE_MAC_AVAIL)) {
				QETH_DBF_TEXT2(0,trace,"enainsrc");
			result=qeth_send_setassparms_simple_without_data(
		     		 card,IPA_SOURCE_MAC_AVAIL,IPA_CMD_ASS_START);
				if (result) {
					PRINT_WARN("Could not start " \
						   "inbound source " \
						   "assist on %s: 0x%x, " \
						   "continuing\n",
						   card->dev_name,result);
					sprintf(dbf_text,"INSR%4x",result);
					QETH_DBF_TEXT2(0,trace,dbf_text);
					/* go on */
				}
			} else {
				PRINT_INFO("Inbound source addresses not " \
					   "supported on %s\n",card->dev_name);
			}
		}
#ifdef QETH_VLAN
                if (!qeth_is_supported(IPA_FULL_VLAN)) {
                        PRINT_WARN("VLAN not supported on %s\n",
				   card->dev_name);
			QETH_DBF_TEXT2(0,trace,"vlnotsup");
                } else {
                        result=qeth_send_setassparms_simple_without_data(
                                card,IPA_VLAN_PRIO,IPA_CMD_ASS_START);
			QETH_DBF_TEXT2(0,trace,"enavlan");
                        if (result) {
                                PRINT_WARN("Could not start vlan "
					   "assist on %s: 0x%x, continuing\n",
					   card->dev_name,result);
				sprintf(dbf_text,"VLAN%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				/* go on*/
			}
			else {
		               card->dev->features |=
				       NETIF_F_HW_VLAN_TX |
				       NETIF_F_HW_VLAN_RX;

			}
                }
#endif /* QETH_VLAN */

                if (!qeth_is_supported(IPA_MULTICASTING)) {
                        PRINT_WARN("multicasting not supported on %s\n",
				   card->dev_name);
			QETH_DBF_TEXT2(0,trace,"mcnotsup");
                } else {
                        result=qeth_send_setassparms_simple_without_data(
                                card,IPA_MULTICASTING,IPA_CMD_ASS_START);
			QETH_DBF_TEXT2(0,trace,"enamcass");
                        if (result) {
                                PRINT_WARN("Could not start multicast "
					   "assist on %s: 0x%x, continuing\n",
					   card->dev_name,result);
				sprintf(dbf_text,"MCAS%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				/* go on*/
                        } else {
				card->dev->flags|=IFF_MULTICAST;
			}
                }

                if (!qeth_is_supported(IPA_IPv6)) {
			QETH_DBF_TEXT2(0,trace,"ipv6ntsp");
                        PRINT_WARN("IPv6 not supported on %s\n",
				   card->dev_name);
                } else {
			if (do_a_startlan6) {
				QETH_DBF_TEXT2(0,trace,"startln6");
				netif_stop_queue(card->dev);
				result=qeth_send_startlan(card,6);
				if (result) {
					sprintf(dbf_text,"stl6%4x",result);
					QETH_DBF_TEXT2(0,trace,dbf_text);
					atomic_set(&card->is_softsetup,0);
					/* do not return an error */
					if (result==0xe080) {
						result=0;
					}
					goto out;
				}
			}
			netif_wake_queue(card->dev);
			QETH_DBF_TEXT2(0,trace,"qipassi6");
			result=qeth_send_qipassist(card,6);
			if (result) {
				PRINT_WARN("couldn't send QIPASSIST6 on %s: " \
					   "0x%x\n",card->dev_name,result);
				sprintf(dbf_text,"QIP6%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				atomic_set(&card->is_softsetup,0);
				goto out;
			}

			sprintf(dbf_text,"%4x%4x",card->ipa6_supported,
				card->ipa6_enabled);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			QETH_DBF_TEXT2(0,trace,"enaipv46");
                        result=qeth_send_setassparms_simple_with_data(
                                card,IPA_IPv6,IPA_CMD_ASS_START,3);
                        if (result) {
                                PRINT_WARN("Could not enable IPv4&6 assist " \
					   "on %s: " \
                                           "0x%x, continuing\n",
					   card->dev_name,result);
					sprintf(dbf_text,"I46A%4x",result);
					QETH_DBF_TEXT2(0,trace,dbf_text);
				/* go on */
			}
			QETH_DBF_TEXT2(0,trace,"enaipv6");
                        result=qeth_send_setassparms_simple_without_data6(
                                card,IPA_IPv6,IPA_CMD_ASS_START);
                        if (result) {
                                PRINT_WARN("Could not start IPv6 assist " \
					   "on %s: " \
                                           "0x%x, continuing\n",
					   card->dev_name,result);
				sprintf(dbf_text,"I6AS%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				/* go on */
                        }
			QETH_DBF_TEXT2(0,trace,"enapstr6");
                        result=qeth_send_setassparms_simple_without_data6(
                                card,IPA_PASSTHRU,IPA_CMD_ASS_START);
                        if (result) {
                                PRINT_WARN("Could not enable passthrough " \
					   "on %s: " \
                                           "0x%x, continuing\n",
					   card->dev_name,result);
				sprintf(dbf_text,"PSTR%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				/* go on */
                        }
                }

		card->broadcast_capable=0;
                if (!qeth_is_supported(IPA_FILTERING)) {
			QETH_DBF_TEXT2(0,trace,"filtntsp");
                        PRINT_WARN("Broadcasting not supported on %s\n",
				   card->dev_name);
                } else {
			QETH_DBF_TEXT2(0,trace,"enafiltr");
                        result=qeth_send_setassparms_simple_without_data(
                                card,IPA_FILTERING,IPA_CMD_ASS_START);
                        if (result) {
                                PRINT_WARN("Could not enable broadcast " \
					   "filtering on %s: " \
                                           "0x%x, continuing\n",
					   card->dev_name,result);
				sprintf(dbf_text,"FLT1%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				goto go_on_filt;
                        }
			result=qeth_send_setassparms_simple_with_data(
                                card,IPA_FILTERING,IPA_CMD_ASS_CONFIGURE,1);
			if (result) {
                                PRINT_WARN("Could not set up broadcast " \
					   "filtering on %s: " \
                                           "0x%x, continuing\n",
					   card->dev_name,result);
				sprintf(dbf_text,"FLT2%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				goto go_on_filt;
			}
			card->dev->flags|=IFF_BROADCAST;
			card->broadcast_capable=1;

		}
go_on_filt:
		if (card->options.checksum_type==HW_CHECKSUMMING) {
			if (!qeth_is_supported(IPA_INBOUND_CHECKSUM)) {
				PRINT_WARN("Inbound HW checksumming not " \
					   "supported on %s, continuing " \
					   "using inbound sw checksumming\n",
					   card->dev_name);
				QETH_DBF_TEXT2(0,trace,"ibckntsp");
				card->options.checksum_type=SW_CHECKSUMMING;
			} else {
				QETH_DBF_TEXT2(0,trace,"ibcksupp");
			result=qeth_send_setassparms_simple_without_data(
					card,IPA_INBOUND_CHECKSUM,
					IPA_CMD_ASS_START);
				if (result) {
					PRINT_WARN("Could not start inbound " \
						   "checksumming on %s: " \
						   "0x%x, " \
						   "continuing using " \
						   "inbound sw checksumming\n",
						   card->dev_name,result);
					sprintf(dbf_text,"SIBC%4x",result);
					QETH_DBF_TEXT2(0,trace,dbf_text);
					card->options.checksum_type=
						SW_CHECKSUMMING;
					goto go_on_checksum;
				}
				result=qeth_send_setassparms_simple_with_data(
					card,IPA_INBOUND_CHECKSUM,
					IPA_CMD_ASS_ENABLE,
					IPA_CHECKSUM_ENABLE_MASK);
				if (result) {
					PRINT_WARN("Could not enable inbound " \
						   "checksumming on %s: " \
						   "0x%x, " \
						   "continuing using " \
						   "inbound sw checksumming\n",
						   card->dev_name,result);
					sprintf(dbf_text,"EIBC%4x",result);
					QETH_DBF_TEXT2(0,trace,dbf_text);
					card->options.checksum_type=
						SW_CHECKSUMMING;
					goto go_on_checksum;
				}
			}
		}
go_on_checksum:

                atomic_set(&card->is_softsetup,1);
        }

	if (atomic_read(&card->enable_routing_attempts4)) {
		if (card->options.routing_type4) {
			sprintf(dbf_text,"strtg4%2x",
				card->options.routing_type4);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			result=qeth_send_setrtg(card,card->options.
						routing_type4,4);
			if (result) {
				if (atomic_dec_return(&card->
					enable_routing_attempts4)) {
				PRINT_WARN("couldn't set up v4 routing type " \
					   "on %s: 0x%x (%s).\nWill try " \
					   "next time again.\n",
					   card->dev_name,result,
					   ((result==0xe010)||
					    (result==0xe008))?
					   "primary already defined":
					   ((result==0xe011)||
					    (result==0xe009))?
					   "secondary already defined":
					   (result==0xe012)?
					   "invalid indicator":
					   "unknown return code");
				sprintf(dbf_text,"sRT4%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				atomic_set(&card->rt4fld,1);
				} else {
				PRINT_WARN("couldn't set up v4 routing type " \
					   "on %s: 0x%x (%s).\nTrying to " \
					   "continue without routing.\n",
					   card->dev_name,result,
					   ((result==0xe010)||
					    (result==0xe008))?
					   "primary already defined":
					   ((result==0xe011)||
					    (result==0xe009))?
					   "secondary already defined":
					   (result==0xe012)?
					   "invalid indicator":
					   "unknown return code");
				sprintf(dbf_text,"SRT4%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				atomic_set(&card->rt4fld,1);
				}
			} else { /* routing set correctly */
				atomic_set(&card->enable_routing_attempts4,0);
				atomic_set(&card->rt4fld,0);
			}
		} else {
			atomic_set(&card->enable_routing_attempts4,0);
			atomic_set(&card->rt4fld,0);
		}
	}

#ifdef QETH_IPV6
	if (atomic_read(&card->enable_routing_attempts6)) {
		if (card->options.routing_type6) {
			sprintf(dbf_text,"strtg6%2x",
				card->options.routing_type6);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			result=qeth_send_setrtg(card,card->options.
						routing_type6,6);
			if (result) {
				if (atomic_dec_return(&card->
					enable_routing_attempts6)) {
				PRINT_WARN("couldn't set up v6 routing type " \
					   "on %s: 0x%x (%s).\nWill try " \
					   "next time again.\n",
					   card->dev_name,result,
					   ((result==0xe010)||
					    (result==0xe008))?
					   "primary already defined":
					   ((result==0xe011)||
					    (result==0xe009))?
					   "secondary already defined":
					   (result==0xe012)?
					   "invalid indicator":
					   "unknown return code");
				sprintf(dbf_text,"sRT6%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				atomic_set(&card->rt6fld,1);
				} else {
				PRINT_WARN("couldn't set up v6 routing type " \
					   "on %s: 0x%x (%s).\nTrying to " \
					   "continue without routing.\n",
					   card->dev_name,result,
					   ((result==0xe010)||
					    (result==0xe008))?
					   "primary already defined":
					   ((result==0xe011)||
					    (result==0xe009))?
					   "secondary already defined":
					   (result==0xe012)?
					   "invalid indicator":
					   "unknown return code");
				sprintf(dbf_text,"SRT6%4x",result);
				QETH_DBF_TEXT2(0,trace,dbf_text);
				atomic_set(&card->rt6fld,1);
				}
			} else { /* routing set correctly */
				atomic_set(&card->enable_routing_attempts6,0);
				atomic_set(&card->rt6fld,0);
			}
		} else {
			atomic_set(&card->enable_routing_attempts6,0);
			atomic_set(&card->rt6fld,0);
		}
	}
#endif /* QETH_IPV6 */

	QETH_DBF_TEXT2(0,trace,"delvipa");
	qeth_set_vipas(card,0);
	QETH_DBF_TEXT2(0,trace,"toip/ms");
	qeth_takeover_ip_ipms(card);
#ifdef QETH_IPV6
	qeth_takeover_ip_ipms6(card);
#endif /* QETH_IPV6 */
	QETH_DBF_TEXT2(0,trace,"setvipa");
	qeth_set_vipas(card,1);

        result=qeth_setips(card,use_setip_retries);
        if (result) { /* by now, qeth_setips does not return errors */
                PRINT_WARN("couldn't set up IPs on %s: 0x%x\n",
			   card->dev_name,result);
		sprintf(dbf_text,"SSIP%4x",result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		atomic_set(&card->is_softsetup,0);
                goto out;
        }

        result=qeth_setipms(card,use_setip_retries);
        if (result) { /* by now, qeth_setipms does not return errors */
                PRINT_WARN("couldn't set up multicast IPs on %s: 0x%x\n",
			   card->dev_name,result);
		sprintf(dbf_text,"ssim%4x",result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		atomic_set(&card->is_softsetup,0);
                goto out;
        }
 out:
	if (!result) {
		netif_wake_queue(card->dev);
	}
	if (wait_for_lock!=QETH_LOCK_ALREADY_HELD)
		my_spin_unlock(&card->softsetup_lock);
        return result;
}

static int qeth_softsetup_thread(void *param)
{
	char dbf_text[15];
	char name[15];
	qeth_card_t *card=(qeth_card_t*)param;

	daemonize();

	/* set a nice name ... */
	sprintf(name, "qethsoftd%04x", card->irq0);
	strcpy(current->comm, name);

	sprintf(dbf_text,"ssth%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);

	atomic_set(&card->softsetup_thread_is_running,1);
	for (;;) {
		if (atomic_read(&card->shutdown_phase)) goto out;
		down_interruptible(&card->softsetup_thread_sem);
		sprintf(dbf_text,"ssst%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		if (atomic_read(&card->shutdown_phase)) goto out;
		while (qeth_softsetup_card(card,QETH_DONT_WAIT_FOR_LOCK)
		       ==-EAGAIN) {
			if (atomic_read(&card->shutdown_phase)) goto out;
			qeth_wait_nonbusy(QETH_IDLE_WAIT_TIME);
		}
		sprintf(dbf_text,"sstd%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		netif_wake_queue(card->dev);
	}
 out:
	atomic_set(&card->softsetup_thread_is_running,0);

	sprintf(dbf_text,"lsst%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);

	return 0;
}

static void qeth_softsetup_thread_starter(void *data)
{
	char dbf_text[15];
	qeth_card_t *card=(qeth_card_t *)data;

	sprintf(dbf_text,"ssts%4x",card->irq0);
	QETH_DBF_TEXT4(0,trace,dbf_text);
	sema_init(&card->softsetup_thread_sem,0);
	kernel_thread(qeth_softsetup_thread,card,SIGCHLD);
}

static void qeth_start_reinit_thread(qeth_card_t *card)
{
	char dbf_text[15];

	/* we allow max 2 reinit threads, one could be just about to
	 * finish and the next would be waiting. another waiting
	 * reinit_thread is not necessary. */
	if (atomic_read(&card->reinit_counter)<2) {
		atomic_inc(&card->reinit_counter);
		if (atomic_read(&card->shutdown_phase)) {
			atomic_dec(&card->reinit_counter);
			return;
		}
		sprintf(dbf_text,"stri%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		PRINT_STUPID("starting reinit-thread\n");
		kernel_thread(qeth_reinit_thread,card,SIGCHLD);
	}
}

static void qeth_recover(void *data)
{
        qeth_card_t *card;
        int i;
	char dbf_text[15];

        card=(qeth_card_t*)data;

	sprintf(dbf_text,"recv%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);

	if (atomic_compare_and_swap(0,1,&card->in_recovery))
		return;

        i=atomic_read(&card->problem);

	sprintf(dbf_text,"PROB%4x",i);
	QETH_DBF_TEXT2(0,trace,dbf_text);

        PRINT_WARN("recovery was scheduled on irq 0x%x (%s) with " \
		   "problem 0x%x\n",
                   card->irq0,card->dev_name,i);
        switch (i) {
        case PROBLEM_RECEIVED_IDX_TERMINATE:
		if (atomic_read(&card->in_recovery))
			atomic_set(&card->break_out,QETH_BREAKOUT_AGAIN);
		break;
        case PROBLEM_CARD_HAS_STARTLANED:
		PRINT_WARN("You are lucky! Somebody either fixed the " \
			   "network problem, plugged the cable back in " \
			   "or enabled the OSA port on %s (CHPID 0x%X). " \
			   "The link has come up.\n",
			   card->dev_name,card->chpid);
		sprintf(dbf_text,"CBIN%4x",i);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		atomic_set(&card->is_softsetup,0);
		qeth_set_dev_flag_running(card);
		atomic_set(&card->enable_routing_attempts4,
			   QETH_ROUTING_ATTEMPTS);
		qeth_clear_ifa4_list(&card->ip_new_state.ip_ifa);
#ifdef QETH_IPV6
		atomic_set(&card->enable_routing_attempts6,
			   QETH_ROUTING_ATTEMPTS);
		qeth_clear_ifa6_list(&card->ip_new_state.ip6_ifa);
#endif /* QETH_IPV6 */
		qeth_clear_ifamc_list(&card->ip_mc_new_state.ipm_ifa);
#ifdef QETH_IPV6
		qeth_clear_ifamc_list(&card->ip_mc_new_state.ipm6_ifa);
#endif /* QETH_IPV6 */
		qeth_refresh_vipa_states(card);
		qeth_start_softsetup_thread(card);
		atomic_set(&card->in_recovery,0);
		break;
	case PROBLEM_RESETTING_EVENT_INDICATOR:
		/* we do nothing here */
		break;
	case PROBLEM_ACTIVATE_CHECK_CONDITION:
	case PROBLEM_GENERAL_CHECK:
	case PROBLEM_USER_TRIGGERED_RECOVERY:
	case PROBLEM_AFFE:
	case PROBLEM_MACHINE_CHECK:
	case PROBLEM_BAD_SIGA_RESULT:
	case PROBLEM_TX_TIMEOUT:
		qeth_start_reinit_thread(card);
		break;
        }
}

static void qeth_schedule_recovery(qeth_card_t *card)
{
	if (card) {
   		INIT_LIST_HEAD(&card->tqueue.list);
		card->tqueue.routine=qeth_recover;
		card->tqueue.data=card;
		card->tqueue.sync=0;
		schedule_task(&card->tqueue);
	} else {
		QETH_DBF_TEXT2(1,trace,"scdnocrd");
		PRINT_WARN("recovery requested to be scheduled " \
			   "with no card!\n");
	}
}

static void qeth_qdio_input_handler(int irq,unsigned int status,
			     unsigned int qdio_error,unsigned int siga_error,
			     unsigned int queue,
			     int first_element,int count,
			     unsigned long card_ptr)
{
        struct net_device *dev;
        qeth_card_t *card;
	int problem;
	int sbalf15;
	char dbf_text[15]="qinbhnXX";

	*((__u16*)(&dbf_text[6]))=(__u16)irq;
	QETH_DBF_HEX6(0,trace,dbf_text,QETH_DBF_TRACE_LEN);

	card=(qeth_card_t *)card_ptr;

#ifdef QETH_PERFORMANCE_STATS
	card->perf_stats.inbound_start_time=NOW;
#endif /* QETH_PERFORMANCE_STATS */
	dev=card->dev;

	if (status&QDIO_STATUS_LOOK_FOR_ERROR) {
		if (status&QDIO_STATUS_ACTIVATE_CHECK_CONDITION) {
			problem=PROBLEM_ACTIVATE_CHECK_CONDITION;
			PRINT_WARN("activate queues on irq 0x%x: " \
				   "dstat=0x%x, cstat=0x%x\n",irq,
				   card->devstat2->dstat,
				   card->devstat2->cstat);
			atomic_set(&card->problem,problem);
			QETH_DBF_TEXT1(0,trace,"IHACTQCK");
			sprintf(dbf_text,"%4x%4x",first_element,count);
			QETH_DBF_TEXT1(0,trace,dbf_text);
			sprintf(dbf_text,"%4x%4x",queue,status);
			QETH_DBF_TEXT1(0,trace,dbf_text);
			sprintf(dbf_text,"qscd%4x",card->irq0);
			QETH_DBF_TEXT1(1,trace,dbf_text);
			qeth_schedule_recovery(card);
			return;
		}
		sbalf15=(card->inbound_qdio_buffers[(first_element+count-1)&
			 QDIO_MAX_BUFFERS_PER_Q].
			 element[15].flags)&&0xff;
		PRINT_STUPID("inbound qdio transfer error on irq 0x%04x. " \
			     "qdio_error=0x%x (more than one: %c), " \
			     "siga_error=0x%x (more than one: %c), " \
			     "sbalf15=x%x, bufno=x%x\n",
			     irq,qdio_error,
			     (status&QDIO_STATUS_MORE_THAN_ONE_QDIO_ERROR)?
			     'y':'n',siga_error,
			     (status&QDIO_STATUS_MORE_THAN_ONE_SIGA_ERROR)?
			     'y':'n',
			     sbalf15,first_element);
		sprintf(dbf_text,"IQTI%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_TEXT1(0,qerr,dbf_text);
		sprintf(dbf_text,"%4x%4x",first_element,count);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_TEXT1(0,qerr,dbf_text);
		sprintf(dbf_text,"%2x%4x%2x",queue,status,sbalf15);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_TEXT1(0,qerr,dbf_text);
		sprintf(dbf_text,"%4x%4x",qdio_error,siga_error);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_TEXT1(0,qerr,dbf_text);
		/* we inform about error more detailed in
		 * qeth_read_in_buffer() */
	}

	for (;;) {
		qeth_get_linux_addrs_for_buffer(card,first_element);
		qeth_read_in_buffer(card,first_element);
		qeth_queue_input_buffer(card,first_element,
					QDIO_FLAG_UNDER_INTERRUPT);
		count--;
		if (count)
			first_element=(first_element+1)&
				(QDIO_MAX_BUFFERS_PER_Q-1);
		else break;
	}
}

static void qeth_qdio_output_handler(int irq,unsigned int status,
			      unsigned int qdio_error,unsigned int siga_error,
			      unsigned int queue,
			      int first_element,int count,
			      unsigned long card_ptr)
{
	qeth_card_t *card;
	int mycnt,problem,buffers_used;
	int sbalf15;
	char dbf_text[15]="qouthnXX";
	char dbf_text2[15]="stchdwXX";
	int last_pci_hit=0,switch_state;
	int last_pci;

	*((__u16*)(&dbf_text[6]))=(__u16)irq;
	QETH_DBF_HEX6(0,trace,dbf_text,QETH_DBF_TRACE_LEN);

	mycnt=count;
	card=(qeth_card_t *)card_ptr;

	if (status&QDIO_STATUS_LOOK_FOR_ERROR) {
		if (status&QDIO_STATUS_ACTIVATE_CHECK_CONDITION) {
			problem=PROBLEM_ACTIVATE_CHECK_CONDITION;
			PRINT_WARN("activate queues on irq 0x%x: " \
				   "dstat=0x%x, cstat=0x%x\n",irq,
				   card->devstat2->dstat,
				   card->devstat2->cstat);
			atomic_set(&card->problem,problem);
			QETH_DBF_TEXT1(0,trace,"OHACTQCK");
			sprintf(dbf_text,"%4x%4x",first_element,count);
			QETH_DBF_TEXT1(0,trace,dbf_text);
			sprintf(dbf_text,"%4x%4x",queue,status);
			QETH_DBF_TEXT1(0,trace,dbf_text);
			sprintf(dbf_text,"qscd%4x",card->irq0);
			QETH_DBF_TEXT1(1,trace,dbf_text);
			qeth_schedule_recovery(card);
			goto out;
		}
		sbalf15=(card->outbound_ringbuffer[queue]->buffer[
			 (first_element+count-1)&
			 QDIO_MAX_BUFFERS_PER_Q].element[15].flags)&0xff;
		PRINT_STUPID("outbound qdio transfer error on irq %04x, " \
			     "queue=%i. qdio_error=0x%x (more than one: %c)," \
			     " siga_error=0x%x (more than one: %c), " \
			     "sbalf15=x%x, bufno=x%x\n",
			     irq,queue,qdio_error,status&
			     QDIO_STATUS_MORE_THAN_ONE_QDIO_ERROR?'y':'n',
			     siga_error,status&
			     QDIO_STATUS_MORE_THAN_ONE_SIGA_ERROR?'y':'n',
			     sbalf15,first_element);
		sprintf(dbf_text,"IQTO%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_TEXT1(0,qerr,dbf_text);
		sprintf(dbf_text,"%4x%4x",first_element,count);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_TEXT1(0,qerr,dbf_text);
		sprintf(dbf_text,"%2x%4x%2x",queue,status,sbalf15);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_TEXT1(0,qerr,dbf_text);
		sprintf(dbf_text,"%4x%4x",qdio_error,siga_error);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_TEXT1(0,qerr,dbf_text);
		/* we maybe do recovery or dst_link_failures
		 * in qeth_free_buffer */
	}

	if (mycnt) {
		last_pci=atomic_read(&card->last_pci_pos[queue]);
		for (;;) {
			qeth_free_buffer(card,queue,first_element,
					 qdio_error,siga_error);
			if (first_element==last_pci) last_pci_hit=1;
			mycnt--;
			if (mycnt>0)
				first_element=(first_element+1)&
					(QDIO_MAX_BUFFERS_PER_Q-1);
			else break;
		}
	}

	buffers_used=atomic_return_sub(count,
				       &card->outbound_used_buffers[queue]);

	switch (card->send_state[queue]) {
	case SEND_STATE_PACK:
		switch_state=(atomic_read
			      (&card->outbound_used_buffers[queue])<=
			      LOW_WATERMARK_PACK);
		/* first_element is the last buffer that we got back
		 * from hydra */
		if (switch_state||last_pci_hit) {
			*((__u16*)(&dbf_text2[6]))=card->irq0;
			QETH_DBF_HEX3(0,trace,dbf_text2,QETH_DBF_TRACE_LEN);
			if (atomic_swap(&card->outbound_ringbuffer_lock
		       			[queue],QETH_LOCK_FLUSH)
			    ==QETH_LOCK_UNLOCKED) {
				/* we stop the queue as we try to not run
				 * onto the outbound_ringbuffer_lock --
				 * this will not prevent it totally, but
				 * reduce it. in high traffic situations,
				 * it saves around 20us per second, hopefully
				 * this is amortized by calling netif_... */
				netif_stop_queue(card->dev);
				qeth_flush_packed_packets
					(card,queue,
					QDIO_FLAG_UNDER_INTERRUPT);
				/* only switch state to non-packing, if
				 * the amount of used buffers decreased */
				if (switch_state)
					card->send_state[queue]=
						SEND_STATE_DONT_PACK;
				netif_wake_queue(card->dev);
				atomic_set(&card->outbound_ringbuffer_lock[
					   queue],QETH_LOCK_UNLOCKED);
			} /* if the lock was UNLOCKED, we flush ourselves,
			     otherwise this is done in do_send_packet when
			     the lock is released */
#ifdef QETH_PERFORMANCE_STATS
			card->perf_stats.sc_p_dp++;
#endif /* QETH_PERFORMANCE_STATS */
		}
		break;
	default: break;
	}

	/* we don't have to start the queue, if it was started already */
	if (buffers_used<QDIO_MAX_BUFFERS_PER_Q-1)
		return;

 out:
	netif_wake_queue(card->dev);
}

static void qeth_interrupt_handler_read(int irq,void *devstat,
					struct pt_regs *p)
{
        devstat_t *stat;
        int cstat,dstat;
        int problem;
        qeth_card_t *card;
        int rqparam;
	char dbf_text[15];
	int result;

        stat=(devstat_t *)devstat;
        cstat=stat->cstat;
        dstat=stat->dstat;
        rqparam=stat->intparm;

	sprintf(dbf_text,"rint%4x",irq);
	QETH_DBF_TEXT4(0,trace,dbf_text);
	sprintf(dbf_text,"%4x%4x",cstat,dstat);
	QETH_DBF_TEXT4(0,trace,dbf_text);
	sprintf(dbf_text,"%4x",rqparam);
	QETH_DBF_TEXT4(0,trace,dbf_text);

	if (!rqparam) {
		PRINT_STUPID("got unsolicited interrupt in read handler, " \
			     "irq 0x%x\n",irq);
		return;
	}

        card=qeth_get_card_by_irq(irq);
	if (!card) return;

	if ((rqparam==CLEAR_STATE) || (stat->flag&DEVSTAT_CLEAR_FUNCTION)) {
		atomic_set(&card->clear_succeeded0,1);
		return;
	}

	if ((dstat==0)&&(cstat==0)) return;

	if (card->devstat0->flag&DEVSTAT_FLAG_SENSE_AVAIL) {
		PRINT_WARN("sense data available on read channel.\n");
		HEXDUMP16(WARN,"irb: ",&card->devstat0->ii.irb);
		HEXDUMP16(WARN,"sense data: ",&card->devstat0->ii.
			  sense.data[0]);
		sprintf(dbf_text,"RSNS%4x",irq);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_HEX0(0,sense,&card->devstat0->ii.irb,
			      QETH_DBF_SENSE_LEN);
	}

        if (cstat!=0) {
                PRINT_WARN("got nonzero-nonpci channel status in read_" \
                           "handler (irq 0x%x, devstat 0x%02x, schstat " \
                           "0x%02x, rqparam 0x%x)\n",irq,dstat,cstat,rqparam);
        }

        problem=qeth_get_cards_problem(card,card->dma_stuff->recbuf,
				       irq,dstat,cstat,rqparam,
				       (char*)&card->devstat0->ii.irb,
				       (char*)&card->devstat0->
				       ii.sense.data[0]);

        /* detect errors in dstat here */
        if ( (dstat&DEV_STAT_UNIT_EXCEP) || (dstat&DEV_STAT_UNIT_CHECK) ) {
                PRINT_WARN("unit check/exception in read_handler " \
                           "(irq 0x%x, devstat 0x%02x, schstat 0x%02x, " \
			   "rqparam 0x%x)\n",irq,dstat,cstat,rqparam);
		if (!atomic_read(&card->is_hardsetup)) {
			if ((problem)&&(qeth_is_to_recover(card,problem)))
				atomic_set(&card->break_out,
					   QETH_BREAKOUT_AGAIN);
			else
				atomic_set(&card->break_out,
					   QETH_BREAKOUT_LEAVE);
			goto wakeup_out;
		} else goto recover;
        }
	
	if (!(dstat&DEV_STAT_CHN_END)) {
                PRINT_WARN("didn't get device end in read_handler " \
                           "(irq 0x%x, devstat 0x%02x, schstat 0x%02x, " \
			   "rqparam 0x%x)\n",irq,dstat,cstat,rqparam);
                goto wakeup_out;
        }

        if ( (rqparam==IDX_ACTIVATE_WRITE_STATE) ||
	     (rqparam==NOP_STATE) ) {
		goto wakeup_out;
	}

        /* at this point, (maybe channel end and) device end has appeared */

	/* we don't start the next read until we have examined the buffer. */
	if ( (rqparam!=IDX_ACTIVATE_READ_STATE) &&
     	     (rqparam!=IDX_ACTIVATE_WRITE_STATE) )
		qeth_issue_next_read(card);

recover:
        if (qeth_is_to_recover(card,problem)) {
		sprintf(dbf_text,"rscd%4x",card->irq0);
		QETH_DBF_TEXT2(1,trace,dbf_text);
		qeth_schedule_recovery(card);
                goto wakeup_out;
        }

        if (!IS_IPA(card->dma_stuff->recbuf)||
            IS_IPA_REPLY(card->dma_stuff->recbuf)) {
                /* setup or unknown data */
		result = qeth_look_for_arp_data(card);
		switch (result) {
			case ARP_RETURNCODE_ERROR:
			case ARP_RETURNCODE_LASTREPLY:
				qeth_wakeup_ioctl(card);
				return;
			default:
				break;
		}
        }

 wakeup_out:
	memcpy(card->ipa_buf,card->dma_stuff->recbuf,QETH_BUFSIZE);
	qeth_wakeup(card);
}

static void qeth_interrupt_handler_write(int irq,void *devstat,
					 struct pt_regs *p)
{
        devstat_t *stat;
        int cstat,dstat,rqparam;
        qeth_card_t *card;
	int problem;
	char dbf_text[15];

        stat = (devstat_t *)devstat;
        cstat = stat->cstat;
        dstat = stat->dstat;
        rqparam=stat->intparm;

	sprintf(dbf_text,"wint%4x",irq);
	QETH_DBF_TEXT4(0,trace,dbf_text);
	sprintf(dbf_text,"%4x%4x",cstat,dstat);
	QETH_DBF_TEXT4(0,trace,dbf_text);
	sprintf(dbf_text,"%4x",rqparam);
	QETH_DBF_TEXT4(0,trace,dbf_text);

	if (!rqparam) {
		PRINT_STUPID("got unsolicited interrupt in write handler, " \
			     "irq 0x%x\n",irq);
		return;
	}

        card=qeth_get_card_by_irq(irq);
	if (!card) return;

	if ((rqparam==CLEAR_STATE) || (stat->flag&DEVSTAT_CLEAR_FUNCTION)) {
		atomic_set(&card->clear_succeeded1,1);
		goto out;
	}

	if ((dstat==0)&&(cstat==0)) goto out;

	if (card->devstat1->flag&DEVSTAT_FLAG_SENSE_AVAIL) {
		PRINT_WARN("sense data available on write channel.\n");
		HEXDUMP16(WARN,"irb: ",&card->devstat1->ii.irb);
		HEXDUMP16(WARN,"sense data: ",&card->devstat1->ii.
			  sense.data[0]);
		sprintf(dbf_text,"WSNS%4x",irq);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_HEX0(0,sense,&card->devstat1->ii.irb,
			      QETH_DBF_SENSE_LEN);
	}

        if (cstat != 0) {
                PRINT_WARN("got nonzero channel status in write_handler " \
                           "(irq 0x%x, devstat 0x%02x, schstat 0x%02x, " \
			   "rqparam 0x%x)\n",irq,dstat,cstat,rqparam);
        }

        problem=qeth_get_cards_problem(card,NULL,
				       irq,dstat,cstat,rqparam,
				       (char*)&card->devstat1->ii.irb,
				       (char*)&card->devstat1->
				       ii.sense.data[0]);

        /* detect errors in dstat here */
        if ( (dstat&DEV_STAT_UNIT_EXCEP) || (dstat&DEV_STAT_UNIT_CHECK) ) {
                PRINT_WARN("unit check/exception in write_handler " \
                           "(irq 0x%x, devstat 0x%02x, schstat 0x%02x, " \
			   "rqparam 0x%x)\n",irq,dstat,cstat,rqparam);
		if (!atomic_read(&card->is_hardsetup)) {
			if (problem==PROBLEM_RESETTING_EVENT_INDICATOR) {
				atomic_set(&card->break_out,
					   QETH_BREAKOUT_AGAIN);
				qeth_wakeup(card);
				goto out;
			}
			atomic_set(&card->break_out,QETH_BREAKOUT_LEAVE);
			goto out;
		} else goto recover;
        }
	
	if (dstat==DEV_STAT_DEV_END) goto out;

	if (!(dstat&DEV_STAT_CHN_END)) {
                PRINT_WARN("didn't get device end in write_handler " \
                           "(irq 0x%x, devstat 0x%02x, schstat 0x%02x, " \
			   "rqparam 0x%x)\n",irq,dstat,cstat,rqparam);
                goto out;
        }

recover:
        if (qeth_is_to_recover(card,problem)) {
		sprintf(dbf_text,"wscd%4x",card->irq1);
		QETH_DBF_TEXT2(1,trace,dbf_text);
		qeth_schedule_recovery(card);
                goto out;
        }

        /* at this point, (maybe channel end and) device end has appeared */
        if ( (rqparam==IDX_ACTIVATE_READ_STATE) ||
	     (rqparam==IDX_ACTIVATE_WRITE_STATE) ||
	     (rqparam==NOP_STATE) ) {
                qeth_wakeup(card);
                goto out;
        }

        /* well, a write has been done successfully. */

 out:
	/* all statuses are final statuses on the write channel */
	atomic_set(&card->write_busy,0);
}

static void qeth_interrupt_handler_qdio(int irq,void *devstat,
					struct pt_regs *p)
{
        devstat_t *stat;
        int cstat,dstat,rqparam;
	char dbf_text[15];

        qeth_card_t *card;

        stat = (devstat_t *)devstat;
        cstat = stat->cstat;
        dstat = stat->dstat;
        rqparam=stat->intparm;

	sprintf(dbf_text,"qint%4x",irq);
	QETH_DBF_TEXT4(0,trace,dbf_text);
	sprintf(dbf_text,"%4x%4x",cstat,dstat);
	QETH_DBF_TEXT4(0,trace,dbf_text);
	sprintf(dbf_text,"%4x",rqparam);
	QETH_DBF_TEXT4(0,trace,dbf_text);


	if (!rqparam) {
		PRINT_STUPID("got unsolicited interrupt in qdio handler, " \
			     "irq 0x%x\n",irq);
		return;
	}

        card=qeth_get_card_by_irq(irq);
	if (!card) return;

	if ((rqparam==CLEAR_STATE) || (stat->flag&DEVSTAT_CLEAR_FUNCTION)) {
		atomic_set(&card->clear_succeeded2,1);
		return;
	}

	if ((dstat==0)&&(cstat==0)) return;

	if (card->devstat2->flag&DEVSTAT_FLAG_SENSE_AVAIL) {
		PRINT_WARN("sense data available on qdio channel.\n");
		HEXDUMP16(WARN,"irb: ",&card->devstat2->ii.irb);
		HEXDUMP16(WARN,"sense data: ",&card->devstat2->ii.
			  sense.data[0]);
		sprintf(dbf_text,"QSNS%4x",irq);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		QETH_DBF_HEX0(0,sense,&card->devstat2->ii.irb,
			      QETH_DBF_SENSE_LEN);
	}

        if ( (rqparam==READ_CONF_DATA_STATE) ||
	     (rqparam==NOP_STATE) ) {
                qeth_wakeup(card);
		return;
        }

        if (cstat != 0) {
		sprintf(dbf_text,"qchk%4x",irq);
		QETH_DBF_TEXT2(0,trace,dbf_text); 
		sprintf(dbf_text,"%4x%4x",cstat,dstat);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		sprintf(dbf_text,"%4x",rqparam);
		QETH_DBF_TEXT2(0,trace,dbf_text);
                PRINT_WARN("got nonzero channel status in qdio_handler " \
                           "(irq 0x%x, devstat 0x%02x, schstat 0x%02x)\n",
                           irq,dstat,cstat);
        }

        if (dstat&~(DEV_STAT_CHN_END|DEV_STAT_DEV_END)) {
                PRINT_WARN("got the following dstat on the qdio channel: " \
                           "irq 0x%x, dstat 0x%02x, cstat 0x%02x, " \
			   "rqparam=%i\n",
                           irq,dstat,cstat,rqparam);
        }

}

static int qeth_register_netdev(qeth_card_t *card)
{
	int result;
	char dbf_text[15];

	sprintf(dbf_text,"rgnd%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	result=register_netdev(card->dev);

	return result;
}

static void qeth_unregister_netdev(qeth_card_t *card)
{
	char dbf_text[15];

	sprintf(dbf_text,"nrgn%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	unregister_netdev(card->dev);
}

static int qeth_stop(struct net_device *dev)
{
	char dbf_text[15];
	qeth_card_t *card;

	card=(qeth_card_t *)dev->priv;
	sprintf(dbf_text,"stop%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	QETH_DBF_TEXT2(0,setup,dbf_text);

	qeth_save_dev_flag_state(card);

	netif_stop_queue(dev);
        if (atomic_swap(&((qeth_card_t*)dev->priv)->is_open,0)) {
                MOD_DEC_USE_COUNT;
        }

	return 0;
}

static void qeth_softshutdown(qeth_card_t *card)
{
	char dbf_text[15];

	sprintf(dbf_text,"ssht%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

        qeth_send_stoplan(card);
}

static void qeth_clear_card(qeth_card_t *card,int qdio_clean,int use_halt)
{
	unsigned long flags0,flags1,flags2;
	char dbf_text[15];

	sprintf(dbf_text,"clr%c%4x",(qdio_clean)?'q':' ',card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);
	QETH_DBF_TEXT1(0,setup,dbf_text);

	atomic_set(&card->write_busy,0);
	if (qdio_clean)
		qdio_cleanup(card->irq2,
			     (card->type==QETH_CARD_TYPE_IQD)?
			     QDIO_FLAG_CLEANUP_USING_HALT:
			     QDIO_FLAG_CLEANUP_USING_CLEAR);

	if (use_halt) {
		atomic_set(&card->clear_succeeded0,0);
		atomic_set(&card->clear_succeeded1,0);
		atomic_set(&card->clear_succeeded2,0);
		
		s390irq_spin_lock_irqsave(card->irq0,flags0);
		halt_IO(card->irq0,CLEAR_STATE,0);
		s390irq_spin_unlock_irqrestore(card->irq0,flags0);
		
		s390irq_spin_lock_irqsave(card->irq1,flags1);
		halt_IO(card->irq1,CLEAR_STATE,0);
		s390irq_spin_unlock_irqrestore(card->irq1,flags1);

		s390irq_spin_lock_irqsave(card->irq2,flags2);
		halt_IO(card->irq2,CLEAR_STATE,0);
		s390irq_spin_unlock_irqrestore(card->irq2,flags2);

		if (qeth_wait_for_event(&card->clear_succeeded0,
					QETH_CLEAR_TIMEOUT)==-ETIME) {
			if (atomic_read(&card->shutdown_phase)!=
			    QETH_REMOVE_CARD_QUICK) {
				PRINT_ERR("Did not get interrupt on halt_IO " \
					  "on irq 0x%x.\n",card->irq0);
			}
		}
		if (qeth_wait_for_event(&card->clear_succeeded1,
					QETH_CLEAR_TIMEOUT)==-ETIME) {
			if (atomic_read(&card->shutdown_phase)!=
			    QETH_REMOVE_CARD_QUICK) {
				PRINT_ERR("Did not get interrupt on halt_IO " \
					  "on irq 0x%x.\n",card->irq1);
			}
		}
		if (qeth_wait_for_event(&card->clear_succeeded2,
					QETH_CLEAR_TIMEOUT)==-ETIME) {
			if (atomic_read(&card->shutdown_phase)!=
			    QETH_REMOVE_CARD_QUICK) {
				PRINT_ERR("Did not get interrupt on halt_IO " \
					  "on irq 0x%x.\n",card->irq2);
			}
		}
	}
		
	atomic_set(&card->clear_succeeded0,0);
	atomic_set(&card->clear_succeeded1,0);
	atomic_set(&card->clear_succeeded2,0);

	s390irq_spin_lock_irqsave(card->irq0,flags0);
        clear_IO(card->irq0,CLEAR_STATE,0);
	s390irq_spin_unlock_irqrestore(card->irq0,flags0);

	s390irq_spin_lock_irqsave(card->irq1,flags1);
        clear_IO(card->irq1,CLEAR_STATE,0);
	s390irq_spin_unlock_irqrestore(card->irq1,flags1);

	s390irq_spin_lock_irqsave(card->irq2,flags2);
        clear_IO(card->irq2,CLEAR_STATE,0);
	s390irq_spin_unlock_irqrestore(card->irq2,flags2);

	if (qeth_wait_for_event(&card->clear_succeeded0,
				QETH_CLEAR_TIMEOUT)==-ETIME) {
		if (atomic_read(&card->shutdown_phase)!=
		    QETH_REMOVE_CARD_QUICK) {
			PRINT_ERR("Did not get interrupt on clear_IO on " \
				  "irq 0x%x.\n",card->irq0);
		}
	}
	if (qeth_wait_for_event(&card->clear_succeeded1,
				QETH_CLEAR_TIMEOUT)==-ETIME) {
		if (atomic_read(&card->shutdown_phase)!=
		    QETH_REMOVE_CARD_QUICK) {
			PRINT_ERR("Did not get interrupt on clear_IO on " \
				  "irq 0x%x.\n",card->irq1);
		}
	}
	if (qeth_wait_for_event(&card->clear_succeeded2,
				QETH_CLEAR_TIMEOUT)==-ETIME) {
		if (atomic_read(&card->shutdown_phase)!=
		    QETH_REMOVE_CARD_QUICK) {
			PRINT_ERR("Did not get interrupt on clear_IO on " \
				  "irq 0x%x.\n",card->irq2);
		}
	}
}

static void qeth_free_card(qeth_card_t *card)
{
	int i,j;
	char dbf_text[15];
	int element_count;
	qeth_vipa_entry_t *e,*e2;

        if (!card) return;

	sprintf(dbf_text,"free%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);
	QETH_DBF_TEXT1(0,setup,dbf_text);

	my_write_lock(&card->vipa_list_lock);
	e=card->vipa_list;
	while (e) {
		e2=e->next;
		kfree(e);
		e=e2;
	}
	my_write_unlock(&card->vipa_list_lock);

	element_count=(card->options.memusage==MEMUSAGE_DISCONTIG)?
		BUFFER_MAX_ELEMENTS:1;
	for (i=0;i<card->options.inbound_buffer_count;i++) {
		for (j=0;j<element_count;j++) {
			if (card->inbound_buffer_pool_entry[i][j]) {
				kfree(card->inbound_buffer_pool_entry[i][j]);
				card->inbound_buffer_pool_entry[i][j]=NULL;
			}
		}
	}
	for (i=0;i<card->no_queues;i++)
		if (card->outbound_ringbuffer[i])
			vfree(card->outbound_ringbuffer[i]);

        if (card->stats) kfree(card->stats);
        if (card->dma_stuff) kfree(card->dma_stuff);
        if (card->dev) kfree(card->dev);
        if (card->devstat2) kfree(card->devstat2);
        if (card->devstat1) kfree(card->devstat1);
        if (card->devstat0) kfree(card->devstat0);
        vfree(card); /* we checked against NULL already */
}

/* also locked from outside (setup_lock) */
static void qeth_remove_card_from_list(qeth_card_t *card)
{
        qeth_card_t *cn;
	unsigned long flags0,flags1,flags2;
	char dbf_text[15];

	my_write_lock(&list_lock);
	if (!card) {
		QETH_DBF_TEXT2(0,trace,"RMCWNOCD");
		PRINT_WARN("qeth_remove_card_from_list call with no card!\n");
		return;
	}

	sprintf(dbf_text,"rmcl%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

        /* check first, if card is in list */
        if (!firstcard) {
		QETH_DBF_TEXT2(0,trace,"NOCRDINL");
                PRINT_WARN("qeth_remove_card_from_list called on " \
			   "empty card list!!\n");
                return;
        }

	s390irq_spin_lock_irqsave(card->irq0,flags0);
	s390irq_spin_lock_irqsave(card->irq1,flags1);
	s390irq_spin_lock_irqsave(card->irq2,flags2);

	if (firstcard==card)
		firstcard=card->next;
	else {
		cn=firstcard;
		while (cn->next) {
			if (cn->next==card) {
				cn->next=card->next;
				card->next=NULL;
				break;
			}
			cn = cn->next;
		}
	}

	s390irq_spin_unlock_irqrestore(card->irq2,flags2);
	s390irq_spin_unlock_irqrestore(card->irq1,flags1);
	s390irq_spin_unlock_irqrestore(card->irq0,flags0);
	

	my_write_unlock(&list_lock);

}

static void qeth_delete_all_ips(qeth_card_t *card)
{
	qeth_vipa_entry_t *e;

	if (atomic_read(&card->is_softsetup)) {
		qeth_clear_ifa4_list(&card->ip_new_state.ip_ifa);
		qeth_clear_ifamc_list(&card->ip_mc_new_state.ipm_ifa);

#ifdef QETH_IPV6
		qeth_clear_ifa6_list(&card->ip_new_state.ip6_ifa);
		qeth_clear_ifamc_list(&card->ip_mc_new_state.ipm6_ifa);
#endif /* QETH_IPV6 */

		my_write_lock(&card->vipa_list_lock);
		e=card->vipa_list;
		while (e) {
			e->state=VIPA_2_B_REMOVED;
			e=e->next;
		}
		my_write_unlock(&card->vipa_list_lock);
		qeth_start_softsetup_thread(card);
	}
}

static void qeth_remove_card(qeth_card_t *card,int method)
{
	char dbf_text[15];

	if (!card) {
		return;
	}
	sprintf(dbf_text,"rmcd%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	QETH_DBF_TEXT1(0,setup,dbf_text);

	if (method==QETH_REMOVE_CARD_PROPER) {
		atomic_set(&card->shutdown_phase,QETH_REMOVE_CARD_PROPER);
		if (atomic_read(&card->is_open)) {
			qeth_stop(card->dev);
			qeth_wait_nonbusy(QETH_REMOVE_WAIT_TIME);
		}
		qeth_delete_all_ips(card);
	} else {
		atomic_set(&card->shutdown_phase,QETH_REMOVE_CARD_QUICK);
	}
	atomic_set(&card->write_busy,0);

	QETH_DBF_TEXT4(0,trace,"freeskbs");
	qeth_free_all_skbs(card);

	QETH_DBF_TEXT2(0,trace,"upthrsem");

	up(&card->softsetup_thread_sem);
	up(&card->reinit_thread_sem);
	while ( (atomic_read(&card->softsetup_thread_is_running)) ||
   		(atomic_read(&card->reinit_counter)) ) {
		qeth_wait_nonbusy(QETH_WAIT_FOR_THREAD_TIME);
	}

	if (method==QETH_REMOVE_CARD_PROPER) {
		QETH_DBF_TEXT4(0,trace,"softshut");
		qeth_softshutdown(card);
		qeth_wait_nonbusy(QETH_REMOVE_WAIT_TIME);
	}

	atomic_set(&card->is_startlaned,0); /* paranoia, qeth_stop
					       should prevent
					       further calls of
					       hard_start_xmit */

	if (atomic_read(&card->is_registered)) {
		QETH_DBF_TEXT2(0,trace,"unregdev");
                qeth_unregister_netdev(card);
		qeth_wait_nonbusy(QETH_REMOVE_WAIT_TIME);
		atomic_set(&card->is_registered,0);
	}

	qeth_put_unique_id(card);

	QETH_DBF_TEXT2(0,trace,"clrcard");
	if (atomic_read(&card->is_hardsetup)) {
		PRINT_STUPID("clearing card %s\n",card->dev_name);
		qeth_clear_card(card,1,0);
	}

	atomic_set(&card->is_hardsetup,0);
	atomic_set(&card->is_softsetup,0);


	if (card->has_irq>=3) { 
		QETH_DBF_TEXT2(0,trace,"freeirq2");
		chandev_free_irq(card->irq2,card->devstat2);
	}
	if (card->has_irq>=2) {
		QETH_DBF_TEXT2(0,trace,"freeirq1");
		chandev_free_irq(card->irq1,card->devstat1);
	}
	if (card->has_irq>=1) {
		QETH_DBF_TEXT2(0,trace,"freeirq0");
		chandev_free_irq(card->irq0,card->devstat0);
	}
}

static void qeth_destructor(struct net_device *dev)
{
	char dbf_text[15];
	qeth_card_t *card;

	card=(qeth_card_t *)(dev->priv);
	sprintf(dbf_text,"dstr%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
}

static void qeth_set_multicast_list(struct net_device *dev)
{
	char dbf_text[15];
	qeth_card_t *card=dev->priv;

	sprintf(dbf_text,"smcl%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);

	qeth_start_softsetup_thread(card);
}

static int qeth_set_mac_address(struct net_device *dev,void *addr)
{
	char dbf_text[15];

	sprintf(dbf_text,"stmc%4x",((qeth_card_t *)dev->priv)->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	return -EOPNOTSUPP;
}

static int qeth_neigh_setup(struct net_device *dev,struct neigh_parms *np)
{
	char dbf_text[15];

	sprintf(dbf_text,"ngst%4x",((qeth_card_t *)dev->priv)->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
        return 0;
}

static void qeth_generate_tokens(qeth_card_t *card)
{
        card->token.issuer_rm_w=0x00010103UL;
        card->token.cm_filter_w=0x00010108UL;
        card->token.cm_connection_w=0x0001010aUL;
        card->token.ulp_filter_w=0x0001010bUL;
        card->token.ulp_connection_w=0x0001010dUL;
}

static int qeth_peer_func_level(int level)
{
	if ((level&0xff)==8) return (level&0xff)+0x400;
        if ( ((level>>8)&3)==1) return (level&0xff)+0x200;
        return level; /* hmmm... don't know what to do with that level. */
}

static int qeth_idx_activate_read(qeth_card_t *card)
{
        int result,result2;
        __u16 temp;
        unsigned long flags;
	char dbf_text[15];

        result=result2=0;

        memcpy(&card->dma_stuff->write_ccw,WRITE_CCW,sizeof(ccw1_t));
        card->dma_stuff->write_ccw.count=IDX_ACTIVATE_SIZE;
        card->dma_stuff->write_ccw.cda=
		QETH_GET_ADDR(card->dma_stuff->sendbuf);

        memcpy(card->dma_stuff->sendbuf,IDX_ACTIVATE_READ,
	       IDX_ACTIVATE_SIZE);
        memcpy(QETH_TRANSPORT_HEADER_SEQ_NO(card->dma_stuff->sendbuf),
               &card->seqno.trans_hdr,QETH_SEQ_NO_LENGTH);

        memcpy(QETH_IDX_ACT_ISSUER_RM_TOKEN(card->dma_stuff->sendbuf),
               &card->token.issuer_rm_w,QETH_MPC_TOKEN_LENGTH);
        memcpy(QETH_IDX_ACT_FUNC_LEVEL(card->dma_stuff->sendbuf),
               &card->func_level,2);

        temp=card->devno2;
        memcpy(QETH_IDX_ACT_QDIO_DEV_CUA(card->dma_stuff->sendbuf),
	       &temp,2);
        temp=(card->cula<<8)+card->unit_addr2;
        memcpy(QETH_IDX_ACT_QDIO_DEV_REALADDR(card->dma_stuff->sendbuf),
               &temp,2);

	sprintf(dbf_text,"iarw%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	QETH_DBF_HEX2(0,control,card->dma_stuff->sendbuf,
		      QETH_DBF_CONTROL_LEN);

        s390irq_spin_lock_irqsave(card->irq0,flags);
        result=do_IO(card->irq0,&card->dma_stuff->write_ccw,
		     IDX_ACTIVATE_WRITE_STATE,0,0);
        if (result) {
		qeth_delay_millis(QETH_WAIT_BEFORE_2ND_DOIO);
                result2=do_IO(card->irq0,&card->dma_stuff->write_ccw,
			      IDX_ACTIVATE_WRITE_STATE,0,0);
		sprintf(dbf_text,"IRW1%4x",result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		sprintf(dbf_text,"IRW2%4x",result2);
		QETH_DBF_TEXT2(0,trace,dbf_text);
                PRINT_WARN("qeth_idx_activate_read (write): do_IO returned " \
                           "%i, next try returns %i\n",result,result2);
        }
        s390irq_spin_unlock_irqrestore(card->irq0,flags);

	if (atomic_read(&card->break_out)) {
		QETH_DBF_TEXT3(0,trace,"IARWBRKO");
		result=-EIO;
		goto exit;
	}

        if (qeth_sleepon(card,QETH_MPC_TIMEOUT)) {
		sprintf(dbf_text,"IRWT%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
                PRINT_ERR("IDX_ACTIVATE(wr) on read channel irq 0x%x: " \
			  "timeout\n",card->irq0);
                result=-EIO;
                goto exit;
        }

	/* start reading on read channel, card->read_ccw is not yet used */
        memcpy(&card->dma_stuff->read_ccw,READ_CCW,sizeof(ccw1_t));
        card->dma_stuff->read_ccw.count=QETH_BUFSIZE;
        card->dma_stuff->read_ccw.cda=QETH_GET_ADDR(card->dma_stuff->recbuf);

        s390irq_spin_lock_irqsave(card->irq0,flags);
	result2=0;
        result=do_IO(card->irq0,&card->dma_stuff->read_ccw,
		     IDX_ACTIVATE_READ_STATE,0,0);
        if (result) {
		qeth_delay_millis(QETH_WAIT_BEFORE_2ND_DOIO);
                result2=do_IO(card->irq0,&card->dma_stuff->read_ccw,
			      IDX_ACTIVATE_READ_STATE,0,0);
		sprintf(dbf_text,"IRR1%4x",result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		sprintf(dbf_text,"IRR2%4x",result2);
		QETH_DBF_TEXT2(0,trace,dbf_text);
                PRINT_WARN("qeth_idx_activate_read (read): do_IO " \
                           "returned %i, next try returns %i\n",
                           result,result2);
        }
        s390irq_spin_unlock_irqrestore(card->irq0,flags);

        if (result2) {
		result=result2;
		if (result) goto exit;
	}

        if (qeth_sleepon(card,QETH_MPC_TIMEOUT)) {
		sprintf(dbf_text,"IRRT%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
                PRINT_ERR("IDX_ACTIVATE(rd) on read channel irq 0x%x: " \
			  "timeout\n",card->irq0);
                result=-EIO;
                goto exit;
        }
	sprintf(dbf_text,"iarr%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	QETH_DBF_HEX2(0,control,card->dma_stuff->recbuf,
		      QETH_DBF_CONTROL_LEN);

        if (!(QETH_IS_IDX_ACT_POS_REPLY(card->dma_stuff->recbuf))) {
		sprintf(dbf_text,"IRNR%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
                PRINT_ERR("IDX_ACTIVATE on read channel irq 0x%x: negative " \
                          "reply\n",card->irq0);
                result=-EIO;
                goto exit;
        }

	card->portname_required=
		((!QETH_IDX_NO_PORTNAME_REQUIRED(card->dma_stuff->recbuf))&&
		 (card->type==QETH_CARD_TYPE_OSAE));

        memcpy(&temp,QETH_IDX_ACT_FUNC_LEVEL(card->dma_stuff->recbuf),2);
        if (temp!=qeth_peer_func_level(card->func_level)) {
		sprintf(dbf_text,"IRFL%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		sprintf(dbf_text,"%4x%4x",card->func_level,temp);
		QETH_DBF_TEXT1(0,trace,dbf_text);
                PRINT_WARN("IDX_ACTIVATE on read channel irq 0x%x: " \
			   "function level mismatch (sent: 0x%x, " \
			   "received: 0x%x)\n",
                           card->irq0,card->func_level,temp);
		result=-EIO;
	}

        memcpy(&card->token.issuer_rm_r,
               QETH_IDX_ACT_ISSUER_RM_TOKEN(card->dma_stuff->recbuf),
               QETH_MPC_TOKEN_LENGTH);

	memcpy(&card->level[0],
	       QETH_IDX_REPLY_LEVEL(card->dma_stuff->recbuf),
	       QETH_MCL_LENGTH);
 exit:
        return result;
}

static int qeth_idx_activate_write(qeth_card_t *card)
{
        int result,result2;
        __u16 temp;
        unsigned long flags;
	char dbf_text[15];

        result=result2=0;

        memcpy(&card->dma_stuff->write_ccw,WRITE_CCW,sizeof(ccw1_t));
        card->dma_stuff->write_ccw.count=IDX_ACTIVATE_SIZE;
        card->dma_stuff->write_ccw.cda=
		QETH_GET_ADDR(card->dma_stuff->sendbuf);

        memcpy(card->dma_stuff->sendbuf,IDX_ACTIVATE_WRITE,
	       IDX_ACTIVATE_SIZE);
        memcpy(QETH_TRANSPORT_HEADER_SEQ_NO(card->dma_stuff->sendbuf),
               &card->seqno.trans_hdr,QETH_SEQ_NO_LENGTH);
        card->seqno.trans_hdr++;

        memcpy(QETH_IDX_ACT_ISSUER_RM_TOKEN(card->dma_stuff->sendbuf),
               &card->token.issuer_rm_w,QETH_MPC_TOKEN_LENGTH);
        memcpy(QETH_IDX_ACT_FUNC_LEVEL(card->dma_stuff->sendbuf),
               &card->func_level,2);
        
        temp=card->devno2;
        memcpy(QETH_IDX_ACT_QDIO_DEV_CUA(card->dma_stuff->sendbuf),
	       &temp,2);
        temp=(card->cula<<8)+card->unit_addr2;
        memcpy(QETH_IDX_ACT_QDIO_DEV_REALADDR(card->dma_stuff->sendbuf),
               &temp,2);
       
	sprintf(dbf_text,"iaww%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	QETH_DBF_HEX2(0,control,card->dma_stuff->sendbuf,
		      QETH_DBF_CONTROL_LEN);

        s390irq_spin_lock_irqsave(card->irq1,flags);
        result=do_IO(card->irq1,&card->dma_stuff->write_ccw,
		     IDX_ACTIVATE_WRITE_STATE,0,0);
        if (result) {
		qeth_delay_millis(QETH_WAIT_BEFORE_2ND_DOIO);
                result2=do_IO(card->irq1,&card->dma_stuff->write_ccw,
			      IDX_ACTIVATE_WRITE_STATE,0,0);
		sprintf(dbf_text,"IWW1%4x",result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		sprintf(dbf_text,"IWW2%4x",result2);
		QETH_DBF_TEXT2(0,trace,dbf_text);
                PRINT_WARN("qeth_idx_activate_write (write): do_IO " \
                           "returned %i, next try returns %i\n",
                           result,result2);
        }
        s390irq_spin_unlock_irqrestore(card->irq1,flags);

	if (atomic_read(&card->break_out)) {
		QETH_DBF_TEXT3(0,trace,"IAWWBRKO");
		result=-EIO;
		goto exit;
	}

        if (qeth_sleepon(card,QETH_MPC_TIMEOUT)) {
		sprintf(dbf_text,"IWWT%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
                PRINT_ERR("IDX_ACTIVATE(wr) on write channel irq 0x%x: " \
			  "timeout\n",card->irq1);
                result=-EIO;
                goto exit;
        }

	QETH_DBF_TEXT3(0,trace,"idxawrrd");

	/* start one read on write channel */
        memcpy(&card->dma_stuff->read_ccw,READ_CCW,sizeof(ccw1_t));
        card->dma_stuff->read_ccw.count=QETH_BUFSIZE;

        /* recbuf and card->read_ccw is not yet used by any other
	   read channel program */
        card->dma_stuff->read_ccw.cda=QETH_GET_ADDR(card->dma_stuff->recbuf);

        s390irq_spin_lock_irqsave(card->irq1,flags);
	result2=0;
        result=do_IO(card->irq1,&card->dma_stuff->read_ccw,
		     IDX_ACTIVATE_READ_STATE,0,0);
        if (result) {
		qeth_delay_millis(QETH_WAIT_BEFORE_2ND_DOIO);
                result2=do_IO(card->irq1,&card->dma_stuff->read_ccw,
			      IDX_ACTIVATE_READ_STATE,0,0);
		sprintf(dbf_text,"IWR1%4x",result);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		sprintf(dbf_text,"IWR2%4x",result2);
		QETH_DBF_TEXT2(0,trace,dbf_text);
                PRINT_WARN("qeth_idx_activate_write (read): do_IO returned " \
                           "%i, next try returns %i\n",result,result2);
        }

        s390irq_spin_unlock_irqrestore(card->irq1,flags);

        if (result2) {
		result=result2;
		if (result) goto exit;
	}

        if (qeth_sleepon(card,QETH_MPC_TIMEOUT)) {
		sprintf(dbf_text,"IWRT%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
                PRINT_ERR("IDX_ACTIVATE(rd) on write channel irq 0x%x: " \
			  "timeout\n",card->irq1);
                result=-EIO;
                goto exit;
        }
	sprintf(dbf_text,"iawr%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	QETH_DBF_HEX2(0,control,card->dma_stuff->recbuf,
		      QETH_DBF_CONTROL_LEN);

        if (!(QETH_IS_IDX_ACT_POS_REPLY(card->dma_stuff->recbuf))) {
		sprintf(dbf_text,"IWNR%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
                PRINT_ERR("IDX_ACTIVATE on write channel irq 0x%x: " \
			  "negative reply\n",card->irq1);
                result=-EIO;
                goto exit;
        }

        memcpy(&temp,QETH_IDX_ACT_FUNC_LEVEL(card->dma_stuff->recbuf),2);
        if ((temp&~0x0100)!=qeth_peer_func_level(card->func_level)) {
		sprintf(dbf_text,"IWFM%4x",card->irq0);
		QETH_DBF_TEXT1(0,trace,dbf_text);
		sprintf(dbf_text,"%4x%4x",card->func_level,temp);
		QETH_DBF_TEXT1(0,trace,dbf_text);
                PRINT_WARN("IDX_ACTIVATE on write channel irq 0x%x: " \
			   "function level mismatch (sent: 0x%x, " \
			   "received: 0x%x)\n",
                           card->irq1,card->func_level,temp);
		result=-EIO;
	}

 exit:
        return result;
}

static int qeth_cm_enable(qeth_card_t *card)
{
        unsigned char *buffer;
	int result;
	char dbf_text[15];

        memcpy(card->send_buf,CM_ENABLE,CM_ENABLE_SIZE);

        memcpy(QETH_CM_ENABLE_ISSUER_RM_TOKEN(card->send_buf),
               &card->token.issuer_rm_r,QETH_MPC_TOKEN_LENGTH);
        memcpy(QETH_CM_ENABLE_FILTER_TOKEN(card->send_buf),
               &card->token.cm_filter_w,QETH_MPC_TOKEN_LENGTH);

        buffer=qeth_send_control_data(card,card->send_buf,
                                      CM_ENABLE_SIZE,MPC_SETUP_STATE);

	if (!buffer) {
		QETH_DBF_TEXT2(0,trace,"CME:NOBF");
		return -EIO;
	}

        memcpy(&card->token.cm_filter_r,
               QETH_CM_ENABLE_RESP_FILTER_TOKEN(buffer),
               QETH_MPC_TOKEN_LENGTH);

	result=qeth_check_idx_response(buffer);

	sprintf(dbf_text,"cme=%4x",result);
	QETH_DBF_TEXT3(0,trace,dbf_text);

        return result;
}

static int qeth_cm_setup(qeth_card_t *card)
{
        unsigned char *buffer;
	int result;
	char dbf_text[15];

        memcpy(card->send_buf,CM_SETUP,CM_SETUP_SIZE);

        memcpy(QETH_CM_SETUP_DEST_ADDR(card->send_buf),
               &card->token.issuer_rm_r,QETH_MPC_TOKEN_LENGTH);
        memcpy(QETH_CM_SETUP_CONNECTION_TOKEN(card->send_buf),
               &card->token.cm_connection_w,QETH_MPC_TOKEN_LENGTH);
        memcpy(QETH_CM_SETUP_FILTER_TOKEN(card->send_buf),
               &card->token.cm_filter_r,QETH_MPC_TOKEN_LENGTH);

        buffer=qeth_send_control_data(card,card->send_buf,
                                      CM_SETUP_SIZE,MPC_SETUP_STATE);

	if (!buffer) {
		QETH_DBF_TEXT2(0,trace,"CMS:NOBF");
		return -EIO;
	}

        memcpy(&card->token.cm_connection_r,
               QETH_CM_SETUP_RESP_DEST_ADDR(buffer),
               QETH_MPC_TOKEN_LENGTH);

	result=qeth_check_idx_response(buffer);

	sprintf(dbf_text,"cms=%4x",result);
	QETH_DBF_TEXT3(0,trace,dbf_text);

        return result;
}

static int qeth_ulp_enable(qeth_card_t *card)
{
        unsigned char *buffer;
	__u16 mtu,framesize;
	__u16 len;
	__u8 link_type;
	int result;
	char dbf_text[15];

        memcpy(card->send_buf,ULP_ENABLE,ULP_ENABLE_SIZE);

	*(QETH_ULP_ENABLE_LINKNUM(card->send_buf))=(__u8)card->options.portno;

        memcpy(QETH_ULP_ENABLE_DEST_ADDR(card->send_buf),
               &card->token.cm_connection_r,QETH_MPC_TOKEN_LENGTH);
        memcpy(QETH_ULP_ENABLE_FILTER_TOKEN(card->send_buf),
               &card->token.ulp_filter_w,QETH_MPC_TOKEN_LENGTH);

        memcpy(QETH_ULP_ENABLE_PORTNAME_AND_LL(card->send_buf),
               card->options.portname,9);

        buffer=qeth_send_control_data(card,card->send_buf,
                                      ULP_ENABLE_SIZE,MPC_SETUP_STATE);

	if (!buffer) {
		QETH_DBF_TEXT2(0,trace,"ULE:NOBF");
		return -EIO;
	}

        memcpy(&card->token.ulp_filter_r,
               QETH_ULP_ENABLE_RESP_FILTER_TOKEN(buffer),
               QETH_MPC_TOKEN_LENGTH);

	/* to be done before qeth_init_ringbuffers and qeth_init_dev */
	if (qeth_get_mtu_out_of_mpc(card->type)) {
		memcpy(&framesize,QETH_ULP_ENABLE_RESP_MAX_MTU(buffer),2);
		mtu=qeth_get_mtu_outof_framesize(framesize);

		sprintf(dbf_text,"ule:%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		sprintf(dbf_text,"mtu=%4x",mtu);
		QETH_DBF_TEXT2(0,trace,dbf_text);

		if (!mtu) return -EINVAL;

		card->max_mtu=mtu;
		card->initial_mtu=mtu;
		card->inbound_buffer_size=mtu+2*PAGE_SIZE;
	} else {
		card->initial_mtu=qeth_get_initial_mtu_for_card(card);
		card->max_mtu=qeth_get_max_mtu_for_card(card->type);
		card->inbound_buffer_size=DEFAULT_BUFFER_SIZE;
	}

	memcpy(&len,QETH_ULP_ENABLE_RESP_DIFINFO_LEN(buffer),2);
	if (len>=QETH_MPC_DIFINFO_LEN_INDICATES_LINK_TYPE) {
		memcpy(&link_type,QETH_ULP_ENABLE_RESP_LINK_TYPE(buffer),1);
		card->link_type=link_type;
		sprintf(dbf_text,"link=%2x",link_type);
		QETH_DBF_TEXT2(0,trace,dbf_text);
	} else card->link_type=0;

	result=qeth_check_idx_response(buffer);

	sprintf(dbf_text,"ule=%4x",result);
	QETH_DBF_TEXT3(0,trace,dbf_text);

        return result;
}

static int qeth_ulp_setup(qeth_card_t *card)
{
        unsigned char *buffer;
        __u16 temp;
	int result;
	char dbf_text[15];

        memcpy(card->send_buf,ULP_SETUP,ULP_SETUP_SIZE);

        memcpy(QETH_ULP_SETUP_DEST_ADDR(card->send_buf),
               &card->token.cm_connection_r,QETH_MPC_TOKEN_LENGTH);
        memcpy(QETH_ULP_SETUP_CONNECTION_TOKEN(card->send_buf),
               &card->token.ulp_connection_w,QETH_MPC_TOKEN_LENGTH);
        memcpy(QETH_ULP_SETUP_FILTER_TOKEN(card->send_buf),
               &card->token.ulp_filter_r,QETH_MPC_TOKEN_LENGTH);

        temp=card->devno2;
        memcpy(QETH_ULP_SETUP_CUA(card->send_buf),
               &temp,2);
        temp=(card->cula<<8)+card->unit_addr2;
        memcpy(QETH_ULP_SETUP_REAL_DEVADDR(card->send_buf),
               &temp,2);

        buffer=qeth_send_control_data(card,card->send_buf,
                                      ULP_SETUP_SIZE,MPC_SETUP_STATE);

	if (!buffer) {
		QETH_DBF_TEXT2(0,trace,"ULS:NOBF");
		return -EIO;
	}

        memcpy(&card->token.ulp_connection_r,
               QETH_ULP_SETUP_RESP_CONNECTION_TOKEN(buffer),
               QETH_MPC_TOKEN_LENGTH);

        result=qeth_check_idx_response(buffer);

	sprintf(dbf_text,"uls=%4x",result);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	return result;
}

static int qeth_qdio_establish(qeth_card_t *card)
{
	int result;
	char adapter_area[15];
	char dbf_text[15];
	void **input_array,**output_array,**ptr;
	int i,j;
	qdio_initialize_t init_data;

	adapter_area[0]=_ascebc['P'];
	adapter_area[1]=_ascebc['C'];
	adapter_area[2]=_ascebc['I'];
	adapter_area[3]=_ascebc['T'];
	*((unsigned int*)(&adapter_area[4]))=PCI_THRESHOLD_A;
	*((unsigned int*)(&adapter_area[8]))=PCI_THRESHOLD_B;
	*((unsigned int*)(&adapter_area[12]))=PCI_TIMER_VALUE;

	input_array=vmalloc(QDIO_MAX_BUFFERS_PER_Q*sizeof(void*));
	if (!input_array) return -ENOMEM;
	ptr=input_array;
	for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
		*ptr=(void*)virt_to_phys
			(&card->inbound_qdio_buffers[j]);
		ptr++;
	}
	
	output_array=vmalloc(QDIO_MAX_BUFFERS_PER_Q*sizeof(void*)*
			     card->no_queues);
	if (!output_array) {
		vfree(input_array);
		return -ENOMEM;
	}
	ptr=output_array;
	for (i=0;i<card->no_queues;i++)
		for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
			*ptr=(void*)virt_to_phys
				(&card->outbound_ringbuffer[i]->
				 buffer[j]);
			ptr++;
		}
	
	init_data.irq=card->irq2;
	init_data.q_format=qeth_get_q_format(card->type);
	init_data.qib_param_field_format=0;
	init_data.qib_param_field=adapter_area;
	init_data.input_slib_elements=NULL;
	init_data.output_slib_elements=NULL;
	init_data.min_input_threshold=card->options.polltime;
	init_data.max_input_threshold=card->options.polltime;
	init_data.min_output_threshold=QETH_MIN_OUTPUT_THRESHOLD;
	init_data.max_output_threshold=QETH_MAX_OUTPUT_THRESHOLD;
	init_data.no_input_qs=1;
	init_data.no_output_qs=card->no_queues;
	init_data.input_handler=qeth_qdio_input_handler;
	init_data.output_handler=qeth_qdio_output_handler;
	init_data.int_parm=(unsigned long)card;
	init_data.flags=QDIO_INBOUND_0COPY_SBALS|
			QDIO_OUTBOUND_0COPY_SBALS|
			QDIO_USE_OUTBOUND_PCIS;
	if (card->do_pfix)
		init_data.flags |= QDIO_PFIX;
	init_data.input_sbal_addr_array=input_array;
	init_data.output_sbal_addr_array=output_array;
	
	result=qdio_initialize(&init_data);
	
	vfree(input_array);
	vfree(output_array);

	sprintf(dbf_text,"qde=%4i",result);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	return result;
}

static int qeth_qdio_activate(qeth_card_t *card)
{
	int result;
	char dbf_text[15];

	result=qdio_activate(card->irq2,0);

	sprintf(dbf_text,"qda=%4x",result);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	return result;
}

static int qeth_dm_act(qeth_card_t *card)
{
        unsigned char *buffer;
	int result;
	char dbf_text[15];

        memcpy(card->send_buf,DM_ACT,DM_ACT_SIZE);

        memcpy(QETH_DM_ACT_DEST_ADDR(card->send_buf),
               &card->token.cm_connection_r,QETH_MPC_TOKEN_LENGTH);
        memcpy(QETH_DM_ACT_CONNECTION_TOKEN(card->send_buf),
               &card->token.ulp_connection_r,QETH_MPC_TOKEN_LENGTH);

        buffer=qeth_send_control_data(card,card->send_buf,
                                      DM_ACT_SIZE,MPC_SETUP_STATE);

	if (!buffer) {
		QETH_DBF_TEXT2(0,trace,"DMA:NOBF");
		return -EIO;
	}

        result=qeth_check_idx_response(buffer);

	sprintf(dbf_text,"dma=%4x",result);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	return result;
}

#if defined(QETH_VLAN)||defined(QETH_IPV6)
static int qeth_verify_dev(struct net_device *dev)
{
	qeth_card_t *tmp;
        int result=0;
#ifdef QETH_VLAN
        struct vlan_group *vlan_grp;
        int i;
#endif

	my_read_lock(&list_lock);
	tmp=firstcard;
	for (;tmp&&(!result);tmp=tmp->next) {
		if (atomic_read(&tmp->shutdown_phase))
			continue;
		if (dev==tmp->dev) {
			result=QETH_VERIFY_IS_REAL_DEV;
		}
#ifdef QETH_VLAN
		/* check all vlan devices */
		vlan_grp = (struct vlan_group *) tmp->vlangrp;
		if (vlan_grp) {
			for (i=0;i<VLAN_GROUP_ARRAY_LEN;i++) {
				if (vlan_grp->vlan_devices[i]==dev) {
					result=QETH_VERIFY_IS_VLAN_DEV;
				}
			}
		}

#endif
	}
        my_read_unlock(&list_lock);
	return result;
}
#endif /* defined(QETH_VLAN)||defined(QETH_IPV6) */

static int qeth_verify_card(qeth_card_t *card)
{
  	qeth_card_t *tmp;
	int result=0;

	my_read_lock(&list_lock);
  	tmp=firstcard;
    	while (tmp) {
      		if ((card==tmp) && (!atomic_read(&card->shutdown_phase))) {
			result=1;
			break;
		}
		tmp=tmp->next;
	}
	my_read_unlock(&list_lock);
	return result;
}

#ifdef QETH_IPV6
extern struct neigh_table arp_tbl;
int (*qeth_old_arp_constructor)(struct neighbour *);
static struct neigh_ops arp_direct_ops_template =
{
	family:			AF_INET,
	destructor:		NULL,
	solicit:		NULL,
	error_report:		NULL,
	output:        		dev_queue_xmit,
	connected_output:	dev_queue_xmit,
	hh_output:		dev_queue_xmit,
	queue_xmit:		dev_queue_xmit
};
/* as we have neighbour structures point to this structure, even
 * after our life time, this will stay in memory as a leak */
static struct neigh_ops *arp_direct_ops;

static int qeth_arp_constructor(struct neighbour *neigh)
{
        char dbf_text[15];
        struct net_device *dev = neigh->dev;
        struct in_device *in_dev = in_dev_get(dev);

        if (in_dev == NULL)
                return -EINVAL;

        QETH_DBF_TEXT4(0,trace,"arpconst");
        if (!qeth_verify_dev(dev)) {
	    	
                in_dev_put(in_dev);
                return qeth_old_arp_constructor(neigh);
        }

        neigh->type = inet_addr_type(*(u32*)neigh->primary_key);
        if (in_dev->arp_parms)
                neigh->parms = in_dev->arp_parms;

        in_dev_put(in_dev);

        sprintf(dbf_text,"%08x",ntohl(*((__u32*)(neigh->primary_key))));
        QETH_DBF_TEXT4(0,trace,dbf_text);
        QETH_DBF_HEX4(0,trace,&neigh,sizeof(void*));

        neigh->nud_state = NUD_NOARP;
        neigh->ops = arp_direct_ops;
        neigh->output = neigh->ops->queue_xmit;
        return 0;
}

static int qeth_hard_header(struct sk_buff *skb,struct net_device *dev,
			    unsigned short type,void *daddr,void *saddr,
			    unsigned len)
{
	qeth_card_t *card;

	QETH_DBF_TEXT5(0,trace,"hardhdr");

#ifdef QETH_VLAN
	if (qeth_verify_dev(dev)==QETH_VERIFY_IS_VLAN_DEV)
		card = (qeth_card_t *)VLAN_DEV_INFO(dev)->real_dev->priv;
        else
#endif
	card=(qeth_card_t*)dev->priv;

	return card->hard_header(skb,dev,type,daddr,saddr,len);
}

static void qeth_header_cache_update(struct hh_cache *hh,
                                     struct net_device *dev,
                                     unsigned char *haddr)
{
	qeth_card_t *card;

	card=(qeth_card_t*)dev->priv;
	QETH_DBF_TEXT5(0,trace,"hdrcheup");
        return card->header_cache_update(hh,dev,haddr);
}

static int qeth_rebuild_header(struct sk_buff *skb)
{
	qeth_card_t *card;
	QETH_DBF_TEXT5(0,trace,"rebldhdr");
	if (skb->protocol==__constant_htons(ETH_P_IP)) return 0;

#ifdef QETH_VLAN
	if (qeth_verify_dev(skb->dev)==QETH_VERIFY_IS_VLAN_DEV)
		card = (qeth_card_t *)VLAN_DEV_INFO(skb->dev)->real_dev->priv;
        else
#endif
	card=(qeth_card_t*)skb->dev->priv;

	return card->rebuild_header(skb);
}

static void qeth_ipv6_init_card(qeth_card_t *card)
{
	card->hard_header=qeth_get_hard_header(card->link_type);
	card->rebuild_header=qeth_get_rebuild_header(card->link_type);
	card->hard_header_cache=qeth_get_hard_header_cache(card->link_type);
	card->header_cache_update=
		qeth_get_header_cache_update(card->link_type);
	card->type_trans=qeth_get_type_trans(card->link_type);
}
#endif /* QETH_IPV6 */

#ifdef QETH_VLAN
static void qeth_vlan_rx_register(struct net_device *dev, 
				  struct vlan_group *grp)
{
        qeth_card_t *card;
        card = (qeth_card_t *)dev->priv;
        spin_lock_irq(&card->vlan_lock);
        card->vlangrp = grp;
        spin_unlock_irq(&card->vlan_lock);
}
static void qeth_vlan_rx_kill_vid(struct net_device *dev, 
				  unsigned short vid)
{
        qeth_card_t *card;
        card = (qeth_card_t *)dev->priv;
        spin_lock_irq(&card->vlan_lock);
	if (card->vlangrp)
                card->vlangrp->vlan_devices[vid] = NULL;
        spin_unlock_irq(&card->vlan_lock);
}

#endif /*QETH_VLAN*/


static void qeth_tx_timeout(struct net_device *dev)
{
	char dbf_text[15];
	qeth_card_t *card;

	card=(qeth_card_t *)dev->priv;
	sprintf(dbf_text,"XMTO%4x",card->irq0);
	QETH_DBF_TEXT2(1,trace,dbf_text);
	card->stats->tx_errors++;
	atomic_set(&card->problem,PROBLEM_TX_TIMEOUT);
	qeth_schedule_recovery(card);
}

static int qeth_init_dev(struct net_device *dev)
{
	qeth_card_t *card;
	char dbf_text[15];

	card=(qeth_card_t *)dev->priv;

	sprintf(dbf_text,"inid%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	dev->tx_timeout=&qeth_tx_timeout;
	dev->watchdog_timeo=QETH_TX_TIMEOUT;
	dev->open=qeth_open;
	dev->stop=qeth_stop;
	dev->set_config=qeth_set_config;
	dev->hard_start_xmit=qeth_hard_start_xmit;
	dev->do_ioctl=qeth_do_ioctl;
	dev->get_stats=qeth_get_stats;
	dev->change_mtu=qeth_change_mtu;
#ifdef QETH_VLAN
	dev->vlan_rx_register = qeth_vlan_rx_register;
        dev->vlan_rx_kill_vid = qeth_vlan_rx_kill_vid;
#endif

	dev->rebuild_header=
#ifdef QETH_IPV6
		(!(qeth_get_additional_dev_flags(card->type)&IFF_NOARP))?
		(qeth_get_rebuild_header(card->link_type)?
		 qeth_rebuild_header:NULL):
#endif /* QETH_IPV6 */
		NULL;
	dev->hard_header=
#ifdef QETH_IPV6
		(!(qeth_get_additional_dev_flags(card->type)&IFF_NOARP))?
		(qeth_get_hard_header(card->link_type)?
		 qeth_hard_header:NULL):
#endif /* QETH_IPV6 */
		NULL;
	dev->header_cache_update=
#ifdef QETH_IPV6
		(!(qeth_get_additional_dev_flags(card->type)&IFF_NOARP))?
		(qeth_get_header_cache_update(card->link_type)?
		 qeth_header_cache_update:NULL):
#endif /* QETH_IPV6 */
		NULL;
	dev->hard_header_cache=
#ifdef QETH_IPV6
		(!(qeth_get_additional_dev_flags(card->type)&IFF_NOARP))?
		qeth_get_hard_header_cache(card->link_type):
#endif /* QETH_IPV6 */
		NULL;
        dev->hard_header_parse=NULL;
        dev->destructor=qeth_destructor;
        dev->set_multicast_list=qeth_set_multicast_list;
        dev->set_mac_address=qeth_set_mac_address;
        dev->neigh_setup=qeth_neigh_setup;

	dev->flags|=qeth_get_additional_dev_flags(card->type);

	dev->flags|=(
		     (card->options.fake_broadcast==FAKE_BROADCAST)||
		     (card->broadcast_capable)
		     )?
		IFF_BROADCAST:0;

	/* is done in hardsetup_card... see comment below
	qeth_send_qipassist(card,4);*/

	/* that was the old place. one id. we need to make sure, that
	 * hydra knows about us going to use the same id again, so we
	 * do that in hardsetup_card every time
	qeth_get_unique_id(card);*/

#ifdef CONFIG_SHARED_IPV6_CARDS
	dev->features=(card->unique_id&UNIQUE_ID_NOT_BY_CARD)?
		0:NETIF_F_SHARED_IPV6;
	dev->dev_id=card->unique_id&0xffff;
#endif /* CONFIG_SHARED_IPV6_CARDS */

	dev->tx_queue_len=qeth_get_device_tx_q_len(card->type);
	dev->hard_header_len=qeth_get_hlen(card->link_type)+
		card->options.add_hhlen;
	dev->addr_len=OSA_ADDR_LEN; /* is ok for eth, tr, atm lane */
	netif_start_queue(dev);

        dev->mtu=card->initial_mtu;

#ifdef QETH_IPV6
	qeth_ipv6_init_card(card);
#endif /* QETH_IPV6 */

	dev_init_buffers(dev);
 
	return 0;
}

static int qeth_get_unitaddr(qeth_card_t *card)
{
	char *prcd;
        int result=0;
	char dbf_text[15];
	int length;

	sprintf(dbf_text,"gtua%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);

	result = read_conf_data(card->irq2, (void **)&prcd, &length, 0);
	if (result) {
		sprintf(dbf_text, "rcd%4x", result);
		QETH_DBF_TEXT3(0, trace, dbf_text);
		PRINT_ERR("read_conf_data for sch %x returned %i\n",
			  card->irq2, result);
		goto exit;
	}

	card->chpid = prcd[30];
	card->unit_addr2 = prcd[31];
	card->cula = prcd[63];
	/* Don't build queues with diag98 for VM guest lan. */
	card->do_pfix = (MACHINE_HAS_PFIX) ? ((prcd[0x10]!=_ascebc['V']) ||
					      (prcd[0x11]!=_ascebc['M'])):0;

	sprintf(dbf_text,"chpid:%02x",card->chpid);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	sprintf(dbf_text,"unad2:%02x",card->unit_addr2);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	sprintf(dbf_text,"cula:%02x",card->cula);
	QETH_DBF_TEXT2(0,trace,dbf_text);

        result=0;

 exit:
        return result;
}

static int qeth_send_nops(qeth_card_t *card)
{
	int result,result2;
	char dbf_text[15];
	int irq;
	unsigned long saveflags;

	card->dma_stuff->write_ccw.cmd_code=CCW_NOP_CMD;
	card->dma_stuff->write_ccw.flags=CCW_FLAG_SLI;
	card->dma_stuff->write_ccw.count=CCW_NOP_COUNT;
	card->dma_stuff->write_ccw.cda=(unsigned long)NULL;

#define DO_SEND_NOP(subchannel) \
do { \
	irq=subchannel; \
	sprintf(dbf_text,"snnp%4x",irq); \
	QETH_DBF_TEXT3(0,trace,dbf_text); \
\
	s390irq_spin_lock_irqsave(irq,saveflags); \
        result=do_IO(irq,&card->dma_stuff->write_ccw,NOP_STATE,0,0); \
        if (result) { \
		qeth_delay_millis(QETH_WAIT_BEFORE_2ND_DOIO); \
                result2=do_IO(irq,&card->dma_stuff->write_ccw, \
			      NOP_STATE,0,0); \
                PRINT_WARN("qeth_send_nops on irq 0x%x: do_IO returned %i, " \
                           "next try returns %i\n", \
                           irq,result,result2); \
		result=result2; \
        } \
        s390irq_spin_unlock_irqrestore(irq,saveflags); \
\
	if (result) goto exit; \
\
        if (qeth_sleepon(card,QETH_NOP_TIMEOUT)) { \
		QETH_DBF_TEXT2(0,trace,"snnp:tme"); \
		result=-EIO; \
		goto exit; \
        } \
} while (0)

	DO_SEND_NOP(card->irq0);
	DO_SEND_NOP(card->irq1);
	DO_SEND_NOP(card->irq2);

exit:
	return result;
}

static void qeth_clear_card_structures(qeth_card_t *card)
{
	int i,j;
	char dbf_text[15];

	if (!card) {
		QETH_DBF_TEXT2(0,trace,"clrCRDnc");
		return;
	}

	sprintf(dbf_text,"clcs%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);
	
	atomic_set(&card->is_startlaned,0);
					   
	for (i=0;i<QETH_MAX_QUEUES;i++) {
		card->send_state[i]=SEND_STATE_DONT_PACK;
		card->outbound_first_free_buffer[i]=0;
		atomic_set(&card->outbound_used_buffers[i],0);
		atomic_set(&card->outbound_ringbuffer_lock[i],0);
		
		for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++) {
			card->outbound_buffer_send_state[i][j]=
				SEND_STATE_DONT_PACK;
			card->send_retries[i][j]=0;
			
			if (i<card->no_queues) {
				card->outbound_ringbuffer[i]->
					ringbuf_element[j].
					next_element_to_fill=0;
				card->outbound_bytes_in_buffer[i]=0;
				skb_queue_head_init(&card->
						    outbound_ringbuffer[i]->
						    ringbuf_element[j].
						    skb_list);
			}
		}
	}

	for (i=0;i<card->options.inbound_buffer_count;i++) {
		xchg((int*)&card->inbound_buffer_pool_entry_used[i],
			 BUFFER_UNUSED);
	}

	spin_lock_init(&card->requeue_input_lock);
	atomic_set(&card->requeue_position,0);
	atomic_set(&card->requeue_counter,0);
	
	card->seqno.trans_hdr=0;
	card->seqno.pdu_hdr=0;
	card->seqno.pdu_hdr_ack=0;
	card->seqno.ipa=0;
	
	qeth_clear_ifa4_list(&card->ip_current_state.ip_ifa);
	qeth_clear_ifa4_list(&card->ip_new_state.ip_ifa);
	qeth_clear_ifamc_list(&card->ip_mc_current_state.ipm_ifa);
	qeth_clear_ifamc_list(&card->ip_mc_new_state.ipm_ifa);

#ifdef QETH_IPV6
	qeth_clear_ifa6_list(&card->ip_current_state.ip6_ifa);
	qeth_clear_ifa6_list(&card->ip_new_state.ip6_ifa);
	qeth_clear_ifamc_list(&card->ip_mc_current_state.ipm6_ifa);
	qeth_clear_ifamc_list(&card->ip_mc_new_state.ipm6_ifa);
#endif /* QETH_IPV6 */
}

static void qeth_init_input_buffers(qeth_card_t *card)
{
	int i;

	/* slowly, slowly (we don't want to enqueue all buffers
	 * at one time) */
	for (i=0;i<QDIO_MAX_BUFFERS_PER_Q;i++) {
		atomic_set(&card->inbound_buffer_refcnt[i],1);
	}
	for (i=0;i<QDIO_MAX_BUFFERS_PER_Q;i++) {
	   	atomic_set(&card->inbound_buffer_refcnt[i],0);
		/* only try to queue as many buffers as we have at all */
		if (i<card->options.inbound_buffer_count) {
			qeth_queue_input_buffer(card,i,0);
		}
	}
	qdio_synchronize(card->irq2,QDIO_FLAG_SYNC_INPUT,0);
}

/* initializes all the structures for a card */
static int qeth_hardsetup_card(qeth_card_t *card,int in_recovery)
{
        int result,q,breakout;
	unsigned long flags;
	int laps=QETH_HARDSETUP_LAPS;
	int clear_laps;
	int cleanup_qdio;
	char dbf_text[15];
	int i,r;

	/* setup name and so on */
	atomic_set(&card->shutdown_phase,0);

        if (atomic_read(&card->is_hardsetup)) {
		sprintf(dbf_text,"hscd%4x",card->irq0);
		QETH_DBF_TEXT2(1,trace,dbf_text);
                PRINT_ALL("card is already hardsetup.\n");
                return 0;
        }

	cleanup_qdio=in_recovery; /* if we are in recovery, we clean
				     the qdio stuff up */

	my_spin_lock(&card->hardsetup_lock);
	atomic_set(&card->write_busy,0);

	do {
		if (in_recovery) {
			PRINT_STUPID("qeth: recovery: quiescing %s...\n",
				     card->dev_name);
			sprintf(dbf_text,"Rqsc%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			qeth_wait_nonbusy(QETH_QUIESCE_WAIT_BEFORE_CLEAR);
		}
		clear_laps=QETH_HARDSETUP_CLEAR_LAPS;
		do {
			if (in_recovery)
				PRINT_STUPID("clearing card %s\n",
					     card->dev_name);
			qeth_clear_card(card,cleanup_qdio,
					(card->type==QETH_CARD_TYPE_OSAE));
			result=qeth_send_nops(card);
			breakout=atomic_read(&card->break_out);
		} while ( (--clear_laps) && (result) );
		if (result) {
			goto exit;
		}

		if (in_recovery) {
			PRINT_STUPID("qeth: recovery: still quiescing %s...\n",
				     card->dev_name);
			sprintf(dbf_text,"RQsc%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			qeth_wait_nonbusy(QETH_QUIESCE_WAIT_AFTER_CLEAR);
		} else {
			atomic_set(&card->shutdown_phase,0);
		}

		cleanup_qdio=0; /* qdio was cleaned now, if necessary */

		result=qeth_get_unitaddr(card);
		if (result) goto exit;

		qeth_generate_tokens(card);

#define PRINT_TOKENS do { \
		sprintf(dbf_text,"stra    "); \
		memcpy(&dbf_text[4],&card->seqno.trans_hdr,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"spdu    "); \
		memcpy(&dbf_text[4],&card->seqno.pdu_hdr,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"spda    "); \
		memcpy(&dbf_text[4],&card->seqno.pdu_hdr_ack,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"sipa    "); \
		memcpy(&dbf_text[4],&card->seqno.ipa,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"tisw    "); \
		memcpy(&dbf_text[4],&card->token.issuer_rm_w,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"tisr    "); \
		memcpy(&dbf_text[4],&card->token.issuer_rm_r,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"tcfw    "); \
		memcpy(&dbf_text[4],&card->token.cm_filter_w,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"tcfr    "); \
		memcpy(&dbf_text[4],&card->token.cm_filter_r,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"tccw    "); \
		memcpy(&dbf_text[4],&card->token.cm_connection_w,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"tccr    "); \
		memcpy(&dbf_text[4],&card->token.cm_connection_r,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"tufw    "); \
		memcpy(&dbf_text[4],&card->token.ulp_filter_w,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"tufr    "); \
		memcpy(&dbf_text[4],&card->token.ulp_filter_r,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"tucw    "); \
		memcpy(&dbf_text[4],&card->token.ulp_connection_w,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
		sprintf(dbf_text,"tucr    "); \
		memcpy(&dbf_text[4],&card->token.ulp_connection_r,4); \
		QETH_DBF_HEX3(0,trace,dbf_text,QETH_DBF_TRACE_LEN); \
	} while (0)
		PRINT_TOKENS;

		/* card->break_out and problem will be set here to 0
		 * (in each lap) (there can't be a problem at this
		 * early time) */
		atomic_set(&card->problem,0);
		atomic_set(&card->break_out,0);

#define CHECK_ERRORS \
		breakout=atomic_read(&card->break_out); \
		if (breakout==QETH_BREAKOUT_AGAIN) \
			continue; \
		else if (breakout==QETH_BREAKOUT_LEAVE) { \
			result=-EIO; \
			goto exit; \
		} \
		if (result) goto exit

		QETH_DBF_TEXT2(0,trace,"hsidxard");
		result=qeth_idx_activate_read(card);
		CHECK_ERRORS;

		PRINT_TOKENS;
		QETH_DBF_TEXT2(0,trace,"hsidxawr");
		result=qeth_idx_activate_write(card);
		CHECK_ERRORS;

		QETH_DBF_TEXT2(0,trace,"hsissurd");
		/* from here, there will always be an outstanding read */
		s390irq_spin_lock_irqsave(card->irq0,flags);
		qeth_issue_next_read(card);
		s390irq_spin_unlock_irqrestore(card->irq0,flags);

		PRINT_TOKENS;
		QETH_DBF_TEXT2(0,trace,"hscmenab");
		result=qeth_cm_enable(card);
		CHECK_ERRORS;

		PRINT_TOKENS;
		QETH_DBF_TEXT2(0,trace,"hscmsetu");
		result=qeth_cm_setup(card);
		CHECK_ERRORS;

		PRINT_TOKENS;
		QETH_DBF_TEXT2(0,trace,"hsulpena");
		result=qeth_ulp_enable(card);
		CHECK_ERRORS;

		PRINT_TOKENS;
		QETH_DBF_TEXT2(0,trace,"hsulpset");
		result=qeth_ulp_setup(card);
		CHECK_ERRORS;

		cleanup_qdio=1;

		QETH_DBF_TEXT2(0,trace,"hsqdioes");
		result=qeth_qdio_establish(card);
		CHECK_ERRORS;

		PRINT_TOKENS;
		QETH_DBF_TEXT2(0,trace,"hsqdioac");
		result=qeth_qdio_activate(card);
		CHECK_ERRORS;

		PRINT_TOKENS;
		QETH_DBF_TEXT2(0,trace,"hsdmact");
		result=qeth_dm_act(card);
		CHECK_ERRORS;
	} while ( (laps--) && (breakout==QETH_BREAKOUT_AGAIN) );
	if (breakout==QETH_BREAKOUT_AGAIN) {
		sprintf(dbf_text,"hsnr%4x",card->irq0);
		QETH_DBF_TEXT2(0,trace,dbf_text);
		printk("qeth: recovery not successful on device " \
		       "0x%X/0x%X/0x%X; giving up.\n",
		       card->devno0,card->devno1,card->devno2);
		result=-EIO;
		goto exit;
	}

	qeth_clear_ifa4_list(&card->ip_current_state.ip_ifa);
	qeth_clear_ifa4_list(&card->ip_new_state.ip_ifa);
	qeth_clear_ifamc_list(&card->ip_mc_current_state.ipm_ifa);
	qeth_clear_ifamc_list(&card->ip_mc_new_state.ipm_ifa);

#ifdef QETH_IPV6
	qeth_clear_ifa6_list(&card->ip_current_state.ip6_ifa);
	qeth_clear_ifa6_list(&card->ip_new_state.ip6_ifa);
	qeth_clear_ifamc_list(&card->ip_mc_current_state.ipm6_ifa);
	qeth_clear_ifamc_list(&card->ip_mc_new_state.ipm6_ifa);
#endif /* QETH_IPV6 */

	if (!atomic_read(&card->is_registered)) {
		card->dev->dev_addr[0]=0; /* we don't know the mac addr yet */
		card->dev->dev_addr[1]=0;
		card->dev->dev_addr[2]=0;
		card->dev->dev_addr[3]=0;
		card->dev->dev_addr[4]=0;
		card->dev->dev_addr[5]=0;
		card->dev->broadcast[0]=card->dev->broadcast[1]=0xff;
		card->dev->broadcast[2]=card->dev->broadcast[3]=0xff;
		card->dev->broadcast[4]=card->dev->broadcast[5]=0xff;
		
		card->dev->type=qeth_get_arphrd_type(card->type,
						     card->link_type);

		card->dev->init=qeth_init_dev;

		if (card->options.memusage==MEMUSAGE_CONTIG) {
			card->easy_copy_cap=
				qeth_determine_easy_copy_cap(card->type);
		} else card->easy_copy_cap=0;
		card->ipa_timeout=qeth_get_ipa_timeout(card->type);
	}

	atomic_set(&card->is_hardsetup,1);
	atomic_set(&card->is_softsetup,0);
	atomic_set(&card->startlan_attempts,1);

	for (q=0;q<card->no_queues;q++)
		card->send_state[q]=SEND_STATE_DONT_PACK;

	/* here we need to know, whether we should include a value
	 * into eui-64 address generation */
	QETH_DBF_TEXT2(0,trace,"qipassi4");
	r=qeth_send_qipassist(card,4);
	if (r) {
		PRINT_WARN("couldn't send QIPASSIST4 on %s: " \
			   "0x%x\n",card->dev_name,r);
		sprintf(dbf_text,"QIP4%4x",r);
		QETH_DBF_TEXT2(0,trace,dbf_text);
	}

	sprintf(dbf_text,"%4x%4x",card->ipa_supported,card->ipa_enabled);
	QETH_DBF_TEXT2(0,trace,dbf_text);

	qeth_get_unique_id(card);

	/* print out status */
	if (in_recovery) {
		qeth_clear_card_structures(card);
		qeth_init_input_buffers(card);
		QETH_DBF_TEXT1(0,trace,"RECOVSUC");
		printk("qeth: recovered device 0x%X/0x%X/0x%X (%s) " \
		       "successfully.\n",
		       card->devno0,card->devno1,card->devno2,
		       card->dev_name);
	} else {
		QETH_DBF_TEXT2(0,trace,"hrdsetok");

		switch (card->type) {
		case QETH_CARD_TYPE_OSAE:
			/* VM will use a non-zero first character to indicate
			 * a HiperSockets like reporting of the level
			 * OSA sets the first character to zero */
			if (!card->level[0]) {
				sprintf(card->level,"%02x%02x",card->level[2],
					card->level[3]);
				card->level[QETH_MCL_LENGTH]=0;
				break;
			} else {
				/* fallthrough */
			}
		case QETH_CARD_TYPE_IQD:
			card->level[0]=(char)_ebcasc[(__u8)card->level[0]];
			card->level[1]=(char)_ebcasc[(__u8)card->level[1]];
			card->level[2]=(char)_ebcasc[(__u8)card->level[2]];
			card->level[3]=(char)_ebcasc[(__u8)card->level[3]];
			card->level[QETH_MCL_LENGTH]=0;
			break;
		default:
			memset(&card->level[0],0,QETH_MCL_LENGTH+1);
		}

		sprintf(dbf_text,"lvl:%s",card->level);
		QETH_DBF_TEXT2(0,setup,dbf_text);

		if (card->portname_required) {
			sprintf(dbf_text,"%s",card->options.portname+1);
			for (i=0;i<8;i++)
				dbf_text[i]=(char)_ebcasc[(__u8)dbf_text[i]];
			dbf_text[8]=0;
			printk("qeth: Device 0x%X/0x%X/0x%X is a%s " \
			       "card%s%s%s\n" \
			       "with link type %s (portname: %s)\n",
			       card->devno0,card->devno1,card->devno2,
			       qeth_get_cardname(card->type),
			       (card->level[0])?" (level: ":"",
			       (card->level[0])?card->level:"",
			       (card->level[0])?")":"",
			       qeth_get_link_type_name(card->type,
						       card->link_type),
			       dbf_text);
		} else {
			printk("qeth: Device 0x%X/0x%X/0x%X is a%s " \
			       "card%s%s%s\nwith link type %s " \
			       "(no portname needed by interface)\n",
			       card->devno0,card->devno1,card->devno2,
			       qeth_get_cardname(card->type),
			       (card->level[0])?" (level: ":"",
			       (card->level[0])?card->level:"",
			       (card->level[0])?")":"",
			       qeth_get_link_type_name(card->type,
						       card->link_type));
		}
	}
        
 exit:
	my_spin_unlock(&card->hardsetup_lock);
        return result;
}

static int qeth_reinit_thread(void *param)
{
	qeth_card_t *card=(qeth_card_t*)param;
	int already_registered;
	int already_hardsetup;
	int retry=QETH_RECOVERY_HARDSETUP_RETRY;
	int result;
	char dbf_text[15];
	char name[15];

	sprintf(dbf_text,"RINI%4x",card->irq0);
	QETH_DBF_TEXT1(0,trace,dbf_text);

	daemonize();

	/* set a nice name ... */
	sprintf(name, "qethrinid%04x", card->irq0);
	strcpy(current->comm, name);
	
	if (atomic_read(&card->shutdown_phase)) goto out_wakeup;
	down_interruptible(&card->reinit_thread_sem);
	if (atomic_read(&card->shutdown_phase)) goto out_wakeup;

	QETH_DBF_TEXT1(0,trace,"ri-gotin");
	PRINT_STUPID("entering recovery (reinit) thread for device %s\n",
		     card->dev_name);

	atomic_set(&card->is_startlaned,0);
	atomic_set(&card->is_softsetup,0);

	my_read_lock(&list_lock);
	if (!qeth_verify_card(card)) goto out;
	QETH_DBF_TEXT1(0,trace,"ri-vrfd");

	atomic_set(&card->write_busy,0);
	qeth_set_dev_flag_norunning(card);
	already_hardsetup=atomic_read(&card->is_hardsetup);
	already_registered=atomic_read(&card->is_registered);
	if (already_hardsetup) {
		atomic_set(&card->is_hardsetup,0);

		if (-1==my_spin_lock_nonbusy(card,&setup_lock)) goto out;
		if (atomic_read(&card->shutdown_phase)) goto out_wakeup;

		atomic_set(&card->escape_softsetup,1);
		if (-1==my_spin_lock_nonbusy(card,&card->softsetup_lock)) {
			atomic_set(&card->escape_softsetup,0);
			goto out;
		}
		atomic_set(&card->escape_softsetup,0);
		if (atomic_read(&card->shutdown_phase)) {
			my_spin_unlock(&card->softsetup_lock);
			goto out_wakeup;
		}
		if (!qeth_verify_card(card)) goto out;

		if (already_registered)
			netif_stop_queue(card->dev);

		qeth_wait_nonbusy(QETH_QUIESCE_NETDEV_TIME);

		atomic_set(&card->is_startlaned,0);

		QETH_DBF_TEXT1(0,trace,"ri-frskb");
		qeth_free_all_skbs(card);
		do {
			QETH_DBF_TEXT1(0,trace,"ri-hrdst");
			result=qeth_hardsetup_card(card,1);
		} while (result&&(retry--));

		/* tries to remove old ips, that's paranoid, but ok */
		qeth_clear_ifa4_list(&card->ip_new_state.ip_ifa);
		qeth_clear_ifamc_list(&card->ip_mc_new_state.ipm_ifa);

#ifdef QETH_IPV6
		qeth_clear_ifa6_list(&card->ip_new_state.ip6_ifa);
		qeth_clear_ifamc_list(&card->ip_mc_new_state.ipm6_ifa);
#endif /* QETH_IPV6 */

		if (result) {
			QETH_DBF_TEXT1(0,trace,"ri-nosuc");
			printk("qeth: RECOVERY WAS NOT SUCCESSFUL ON %s " \
		     	       "(devnos 0x%X/0x%X/0x%X), GIVING UP, " \
	     		       "OUTGOING PACKETS WILL BE DISCARDED!\n",
     			       card->dev_name,card->devno0,
			       card->devno1,card->devno2);
			/* early leave hard_start_xmit! */
			atomic_set(&card->is_startlaned,0);
			/* show status in /proc/qeth */
			atomic_set(&card->is_gone,1);
			qeth_wakeup_procfile();
		} else {
			QETH_DBF_TEXT1(0,trace,"ri-sftst");
			qeth_softsetup_card(card,QETH_LOCK_ALREADY_HELD);
			my_spin_unlock(&card->softsetup_lock);

			if (!already_registered) {
				QETH_DBF_TEXT1(0,trace,"ri-regcd");
				qeth_register_netdev(card);
			}
			qeth_restore_dev_flag_state(card);
			atomic_set(&card->is_gone,0);
			netif_wake_queue(card->dev);
			qeth_wakeup_procfile();
		}
		my_spin_unlock(&setup_lock);
	}
out:
	atomic_set(&card->in_recovery,0);
	my_read_unlock(&list_lock);
	QETH_DBF_TEXT1(0,trace,"ri-leave");
out_wakeup:
	up(&card->reinit_thread_sem);
	atomic_dec(&card->reinit_counter);

	return 0;
}

static void qeth_fill_qeth_card_options(qeth_card_t *card)
{
        int i;

	card->options.portname[0]=0;
	for (i=1;i<9;i++)
		card->options.portname[i]=_ascebc[' '];
	strcpy(card->options.devname," ");
	card->options.routing_type4=NO_ROUTER;
#ifdef QETH_IPV6
	card->options.routing_type6=NO_ROUTER;
#endif /* QETH_IPV6 */
	card->options.portno=0;
	card->options.checksum_type=QETH_CHECKSUM_DEFAULT;
	card->options.do_prio_queueing=0;
	card->options.default_queue=QETH_DEFAULT_QUEUE;
	card->options.inbound_buffer_count=DEFAULT_BUFFER_COUNT;
	card->options.polltime=QETH_MAX_INPUT_THRESHOLD;
	card->options.memusage=MEMUSAGE_DISCONTIG;
	card->options.macaddr_mode=MACADDR_NONCANONICAL;
	card->options.broadcast_mode=BROADCAST_ALLRINGS;
	card->options.fake_broadcast=DONT_FAKE_BROADCAST;
	card->options.ena_ipat=ENABLE_TAKEOVER;
	card->options.add_hhlen=DEFAULT_ADD_HHLEN;
	card->options.fake_ll=DONT_FAKE_LL;
	card->options.async_iqd=SYNC_IQD;
}

static qeth_card_t *qeth_alloc_card(void)
{
        qeth_card_t *card;

	QETH_DBF_TEXT3(0,trace,"alloccrd");
	card=(qeth_card_t *)vmalloc(sizeof(qeth_card_t));
	if (!card) goto exit_card;
	memset(card,0,sizeof(qeth_card_t));
	init_waitqueue_head(&card->wait_q);
	init_waitqueue_head(&card->ioctl_wait_q);

	qeth_fill_qeth_card_options(card);

	card->dma_stuff=(qeth_dma_stuff_t*)kmalloc(sizeof(qeth_dma_stuff_t),
						   GFP_DMA);
	if (!card->dma_stuff) goto exit_dma;
	memset(card->dma_stuff,0,sizeof(qeth_dma_stuff_t));

	card->dma_stuff->recbuf=(char*)kmalloc(QETH_BUFSIZE,GFP_DMA);
	if (!card->dma_stuff->recbuf) goto exit_dma1;
	memset(card->dma_stuff->recbuf,0,QETH_BUFSIZE);

	card->dma_stuff->sendbuf=(char*)kmalloc(QETH_BUFSIZE,GFP_DMA);
	if (!card->dma_stuff->sendbuf) goto exit_dma2;
	memset(card->dma_stuff->sendbuf,0,QETH_BUFSIZE);

	card->dev=(struct net_device *)kmalloc(sizeof(struct net_device),
                                              GFP_KERNEL);
	if (!card->dev) goto exit_dev;
	memset(card->dev,0,sizeof(struct net_device));

	card->stats=(struct net_device_stats *)kmalloc(
                sizeof(struct net_device_stats),GFP_KERNEL);
	if (!card->stats) goto exit_stats;
	memset(card->stats,0,sizeof(struct net_device_stats));

	card->devstat0=(devstat_t *)kmalloc(sizeof(devstat_t),GFP_KERNEL);
	if (!card->devstat0) goto exit_stats;
	memset(card->devstat0,0,sizeof(devstat_t));

	card->devstat1=(devstat_t *)kmalloc(sizeof(devstat_t),GFP_KERNEL);
	if (!card->devstat1) {
		kfree(card->devstat0);
		goto exit_stats;
	}
	memset(card->devstat1,0,sizeof(devstat_t));

	card->devstat2=(devstat_t *)kmalloc(sizeof(devstat_t),GFP_KERNEL);
	if (!card->devstat2) {
		kfree(card->devstat1);
		kfree(card->devstat0);
		goto exit_stats;
	}
	memset(card->devstat2,0,sizeof(devstat_t));

	spin_lock_init(&card->wait_q_lock);
	spin_lock_init(&card->softsetup_lock);
	spin_lock_init(&card->hardsetup_lock);
	spin_lock_init(&card->ioctl_lock);
#ifdef QETH_VLAN
	spin_lock_init(&card->vlan_lock);
        card->vlangrp = NULL;
#endif
	card->unique_id=0;
	sema_init(&card->reinit_thread_sem,0);
	up(&card->reinit_thread_sem);

        /* setup card stuff */
        card->ip_current_state.ip_ifa=NULL;
        card->ip_new_state.ip_ifa=NULL;
        card->ip_mc_current_state.ipm_ifa=NULL;
	card->ip_mc_new_state.ipm_ifa=NULL;

#ifdef QETH_IPV6
	card->ip_current_state.ip6_ifa=NULL;
	card->ip_new_state.ip6_ifa=NULL;
	card->ip_mc_current_state.ipm6_ifa=NULL;
	card->ip_mc_new_state.ipm6_ifa=NULL;
#endif /* QETH_IPV6 */

        /* setup net_device stuff */
	card->dev->priv=card;

	strncpy(card->dev->name,card->dev_name,IFNAMSIZ);

        /* setup net_device_stats stuff */
        /* =nothing yet */

        /* and return to the sender */
        return card;

        /* these are quick exits in case of failures of the kmallocs */
exit_stats:
        kfree(card->dev);
exit_dev:
	kfree(card->dma_stuff->sendbuf);
exit_dma2:
	kfree(card->dma_stuff->recbuf);
exit_dma1:
	kfree(card->dma_stuff);
exit_dma:
        kfree(card);
exit_card:
        return NULL;
}

static int qeth_init_ringbuffers1(qeth_card_t *card)
{
	int i,j;
	char dbf_text[15];

	sprintf(dbf_text,"irb1%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	for (i=0;i<card->no_queues;i++) {
		card->outbound_ringbuffer[i]=
			vmalloc(sizeof(qeth_ringbuffer_t));
		if (!card->outbound_ringbuffer[i]) {
			for (j=i-1;j>=0;j--) {
				vfree(card->outbound_ringbuffer[j]);
				card->outbound_ringbuffer[j]=NULL;
			}
			return -ENOMEM;
		}
		memset(card->outbound_ringbuffer[i],0,
		       sizeof(qeth_ringbuffer_t));
		for (j=0;j<QDIO_MAX_BUFFERS_PER_Q;j++)
			skb_queue_head_init(&card->outbound_ringbuffer[i]->
					    ringbuf_element[j].skb_list);
	}

	return 0;
}

static int qeth_init_ringbuffers2(qeth_card_t *card)
{
	int i,j;
	int failed=0;
	char dbf_text[15];
	int discont_mem,element_count;
	long alloc_size;

	sprintf(dbf_text,"irb2%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	discont_mem=(card->options.memusage==MEMUSAGE_DISCONTIG);
	element_count=(discont_mem)?BUFFER_MAX_ELEMENTS:1;
	alloc_size=(discont_mem)?PAGE_SIZE:BUFFER_SIZE;
	if (discont_mem) {
		for (i=0;i<card->options.inbound_buffer_count;i++) {
			for (j=0;j<element_count;j++) {
				card->inbound_buffer_pool_entry[i][j]=
					kmalloc(alloc_size,GFP_KERNEL);
				if (!card->inbound_buffer_pool_entry[i][j]) {
					failed=1;
					goto out;
				}
			}
			card->inbound_buffer_pool_entry_used[i]=BUFFER_UNUSED;
		}
	} else {
		for (i=0;i<card->options.inbound_buffer_count;i++) {
			card->inbound_buffer_pool_entry[i][0]=
				kmalloc(alloc_size,GFP_KERNEL);
			if (!card->inbound_buffer_pool_entry[i][0]) failed=1;
			for (j=1;j<element_count;j++)
				card->inbound_buffer_pool_entry[i][j]=
					card->inbound_buffer_pool_entry[i][0]+
					PAGE_SIZE*j;
			card->inbound_buffer_pool_entry_used[i]=BUFFER_UNUSED;
		}
	}
		
out:
	if (failed) {
		for (i=0;i<card->options.inbound_buffer_count;i++) {
			for (j=0;j<QDIO_MAX_ELEMENTS_PER_BUFFER;j++) {
				if (card->inbound_buffer_pool_entry[i][j]) {
					if (j<element_count) kfree(card->
						inbound_buffer_pool_entry
						[i][j]);
					card->inbound_buffer_pool_entry
						[i][j]=NULL;
				}
			}
		}
		for (i=0;i<card->no_queues;i++) {
			vfree(card->outbound_ringbuffer[i]);
			card->outbound_ringbuffer[i]=NULL;
		}
		return -ENOMEM;
	}

	spin_lock_init(&card->requeue_input_lock);

	return 0;
}

/* also locked from outside (setup_lock) */
static void qeth_insert_card_into_list(qeth_card_t *card)
{
	char dbf_text[15];

	sprintf(dbf_text,"icil%4x",card->irq0);
	QETH_DBF_TEXT3(0,trace,dbf_text);

	my_write_lock(&list_lock);
        card->next=firstcard;
        firstcard=card;
	my_write_unlock(&list_lock);
}

static int qeth_determine_card_type(qeth_card_t *card)
{
        int i=0;
	char dbf_text[15];

        while (known_devices[i][4]) {
                if ( (card->dev_type==known_devices[i][2]) &&
                     (card->dev_model==known_devices[i][3]) ) {
                        card->type=known_devices[i][4];
			if (card->options.ena_ipat==ENABLE_TAKEOVER)
				card->func_level=known_devices[i][6];
			else
				card->func_level=known_devices[i][7];
                        card->no_queues=known_devices[i][8];
			card->is_multicast_different=known_devices[i][9];
			sprintf(dbf_text,"irq %4x",card->irq0);
			QETH_DBF_TEXT2(0,setup,dbf_text);
			sprintf(dbf_text,"ctyp%4x",card->type);
			QETH_DBF_TEXT2(0,setup,dbf_text);
                        return 0;
                }
                i++;
        }
        card->type=QETH_CARD_TYPE_UNKNOWN;
	sprintf(dbf_text,"irq %4x",card->irq0);
	QETH_DBF_TEXT2(0,setup,dbf_text);
	sprintf(dbf_text,"ctypUNKN");
	QETH_DBF_TEXT2(0,setup,dbf_text);
	PRINT_ERR("unknown card type on irq x%x\n",card->irq0);
	return -ENOENT;
}

static int qeth_getint(char *s,int longint)
{
        int cnt;
        int hex;
        int result;
        char c;
        
        if (!s) return -1;
        hex=((s[0]=='0')&&( (s[1]=='x') || (s[1]=='X') ))?1:0;
        cnt=(hex)?2:0; /* start from the first real digit */
        if (!(s[cnt])) return -1;
        result=0;
        while ((c=s[cnt++])) {
                if (hex) {
                        if (isxdigit(c)) result=result*16+qeth_getxdigit(c);
                        else return -1;
                } else {
                        if (isdigit(c)) result=result*10+c-'0';
			else return -1;
                }
                /* prevent overflow, 0xffff is enough for us */
		if (longint) {
			if (result>0xfffffff) return -1;
		} else {
			if (result>0xffff) return -1;
		}
        }
        return result;
}

static int qeth_setvalue_if_possible(qeth_card_t *card,char *option,
				     int parse_category,
				     int *variable,int value)
{
	int *a_p=0; /* to shut the compiler up */
	int routing_already_set_conflict=0;

	a_p= &card->options.already_parsed[parse_category];

	/* reject a general router statement, if router4 or router6
	 * have already been set */
	if (parse_category==PARSE_ROUTING_TYPE) {
		if ( (card->options.already_parsed[PARSE_ROUTING_TYPE4]) ||
		     (card->options.already_parsed[PARSE_ROUTING_TYPE6]) )
			routing_already_set_conflict=1;
	}

	/* reject a router4 statement, if a general router statement
	 * has already been set */
	if (parse_category==PARSE_ROUTING_TYPE4) {
		if ( (card->options.already_parsed[PARSE_ROUTING_TYPE4]) &&
		     (card->options.already_parsed[PARSE_ROUTING_TYPE6]) )
			routing_already_set_conflict=1;
	}

	/* reject a router6 statement, if a general router statement
	 * has already been set */
	if (parse_category==PARSE_ROUTING_TYPE6) {
		if ( (card->options.already_parsed[PARSE_ROUTING_TYPE4]) &&
		     (card->options.already_parsed[PARSE_ROUTING_TYPE6]) )
			routing_already_set_conflict=1;
	}

	if ( (routing_already_set_conflict) ||
	     ((parse_category!=-1) && (*a_p)) ) {
                PRINT_ERR("parameter %s does not fit to previous " \
			  "parameters\n",option);
                return 0;
        }
        *a_p=1;
        *variable=value;
        return 1;
}

/* um... */
#define doit1(a,b,c,d) \
   if (!strcmp(option,a)) \
     return qeth_setvalue_if_possible(card,option,b,((int*)&c),d)

#define doit2(a,b,c,d,e,f) \
   doit1(a,b,c,d) | qeth_setvalue_if_possible(card,option,-1,((int*)&e),f)

/* returns 0, if option could notr be parsed alright */
static int qeth_parse_option(qeth_card_t *card,char *option)
{
	int i;
	int *a_p;

#ifdef QETH_IPV6
	doit2("no_router",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,NO_ROUTER,
	      card->options.routing_type6,NO_ROUTER);
	doit2("primary_router",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,PRIMARY_ROUTER,
	      card->options.routing_type6,PRIMARY_ROUTER);
	doit2("secondary_router",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,SECONDARY_ROUTER,
	      card->options.routing_type6,SECONDARY_ROUTER);
	doit2("multicast_router",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,MULTICAST_ROUTER,
	      card->options.routing_type6,MULTICAST_ROUTER);
	doit2("primary_connector",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,PRIMARY_CONNECTOR,
	      card->options.routing_type6,PRIMARY_CONNECTOR);
	doit2("secondary_connector",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,SECONDARY_CONNECTOR,
	      card->options.routing_type6,SECONDARY_CONNECTOR);
#else /* QETH_IPV6 */
	doit1("no_router",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,NO_ROUTER);
	doit1("primary_router",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,PRIMARY_ROUTER);
	doit1("secondary_router",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,SECONDARY_ROUTER);
	doit1("multicast_router",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,MULTICAST_ROUTER);
	doit1("primary_connector",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,PRIMARY_CONNECTOR);
	doit1("secondary_connector",PARSE_ROUTING_TYPE,
	      card->options.routing_type4,SECONDARY_CONNECTOR);
#endif /* QETH_IPV6 */
	
	doit1("no_router4",PARSE_ROUTING_TYPE4,
	      card->options.routing_type4,NO_ROUTER);
	doit1("primary_router4",PARSE_ROUTING_TYPE4,
	      card->options.routing_type4,PRIMARY_ROUTER);
	doit1("secondary_router4",PARSE_ROUTING_TYPE4,
	      card->options.routing_type4,SECONDARY_ROUTER);
	doit1("multicast_router4",PARSE_ROUTING_TYPE4,
	      card->options.routing_type4,MULTICAST_ROUTER);
	doit1("primary_connector4",PARSE_ROUTING_TYPE4,
	      card->options.routing_type4,PRIMARY_CONNECTOR);
	doit1("secondary_connector4",PARSE_ROUTING_TYPE4,
	      card->options.routing_type4,SECONDARY_CONNECTOR);
	
#ifdef QETH_IPV6
	doit1("no_router6",PARSE_ROUTING_TYPE6,
	      card->options.routing_type6,NO_ROUTER);
	doit1("primary_router6",PARSE_ROUTING_TYPE6,
	      card->options.routing_type6,PRIMARY_ROUTER);
	doit1("secondary_router6",PARSE_ROUTING_TYPE6,
	      card->options.routing_type6,SECONDARY_ROUTER);
	doit1("multicast_router6",PARSE_ROUTING_TYPE6,
	      card->options.routing_type6,MULTICAST_ROUTER);
	doit1("primary_connector6",PARSE_ROUTING_TYPE6,
	      card->options.routing_type6,PRIMARY_CONNECTOR);
	doit1("secondary_connector6",PARSE_ROUTING_TYPE6,
	      card->options.routing_type6,SECONDARY_CONNECTOR);
#endif /* QETH_IPV6 */
	
	doit1("sw_checksumming",PARSE_CHECKSUMMING,
	      card->options.checksum_type,SW_CHECKSUMMING);
	doit1("hw_checksumming",PARSE_CHECKSUMMING,
	      card->options.checksum_type,HW_CHECKSUMMING);
	doit1("no_checksumming",PARSE_CHECKSUMMING,
	      card->options.checksum_type,NO_CHECKSUMMING);
	
	doit1("prio_queueing_prec",PARSE_PRIO_QUEUEING,
	      card->options.do_prio_queueing,PRIO_QUEUEING_PREC);
	doit1("prio_queueing_tos",PARSE_PRIO_QUEUEING,
	      card->options.do_prio_queueing,PRIO_QUEUEING_TOS);
	
	doit1("mem_discontig",PARSE_MEMUSAGE,
	      card->options.memusage,MEMUSAGE_DISCONTIG);
	doit1("mem_contig",PARSE_MEMUSAGE,
	      card->options.memusage,MEMUSAGE_CONTIG);
	
	doit1("broadcast_allrings",PARSE_BROADCAST_MODE,
	      card->options.broadcast_mode,BROADCAST_ALLRINGS);
	doit1("broadcast_local",PARSE_BROADCAST_MODE,
	      card->options.broadcast_mode,BROADCAST_LOCAL);
	
	doit1("macaddr_noncanon",PARSE_MACADDR_MODE,
	      card->options.macaddr_mode,MACADDR_NONCANONICAL);
	doit1("macaddr_canon",PARSE_MACADDR_MODE,
	      card->options.macaddr_mode,MACADDR_CANONICAL);
	
	doit1("enable_takeover",PARSE_ENA_IPAT,
	      card->options.ena_ipat,ENABLE_TAKEOVER);
	doit1("disable_takeover",PARSE_ENA_IPAT,
	      card->options.ena_ipat,DISABLE_TAKEOVER);
	
	doit1("fake_broadcast",PARSE_FAKE_BROADCAST,
	      card->options.fake_broadcast,FAKE_BROADCAST);
	doit1("dont_fake_broadcast",PARSE_FAKE_BROADCAST,
	      card->options.fake_broadcast,DONT_FAKE_BROADCAST);
	
	doit1("fake_ll",PARSE_FAKE_LL,
	      card->options.fake_ll,FAKE_LL);
	doit1("dont_fake_ll",PARSE_FAKE_LL,
	      card->options.fake_ll,DONT_FAKE_LL);
	
	doit1("sync_hsi",PARSE_ASYNC_IQD,
	      card->options.async_iqd,SYNC_IQD);
	doit1("async_hsi",PARSE_ASYNC_IQD,
	      card->options.async_iqd,ASYNC_IQD);
	
	doit2("no_prio_queueing:0",PARSE_PRIO_QUEUEING,
	      card->options.do_prio_queueing,NO_PRIO_QUEUEING,
	      card->options.default_queue,0);
	doit2("no_prio_queueing:1",PARSE_PRIO_QUEUEING,
	      card->options.do_prio_queueing,NO_PRIO_QUEUEING,
	      card->options.default_queue,1);
	doit2("no_prio_queueing:2",PARSE_PRIO_QUEUEING,
	      card->options.do_prio_queueing,NO_PRIO_QUEUEING,
	      card->options.default_queue,2);
	doit2("no_prio_queueing:3",PARSE_PRIO_QUEUEING,
	      card->options.do_prio_queueing,NO_PRIO_QUEUEING,
	      card->options.default_queue,3);
	doit2("no_prio_queueing",PARSE_PRIO_QUEUEING,
	      card->options.do_prio_queueing,NO_PRIO_QUEUEING,
	      card->options.default_queue,QETH_DEFAULT_QUEUE);
	
	if (!strncmp(option,"polltime:",9)) {
		i=qeth_getint(option+9,1);
		if (i==-1) {
			PRINT_ERR("parameter %s -- does not contain " \
				  "a valid number.\n",option);
			return 0;
		}
		return qeth_setvalue_if_possible(card,option,PARSE_POLLTIME,
						 &card->options.polltime,i);
	}
	
	if (!strncmp(option,"port:",5)) {
		i=qeth_getint(option+5,0);
		if (i==-1) {
			PRINT_ERR("parameter %s -- does not contain " \
				  "a valid number.\n",option);
			return 0;
		}
		if ( (i<0) || (i>MAX_PORTNO) ) {
			PRINT_ERR("parameter %s -- out of range\n",
				  option);
			return 0;
		}
		return qeth_setvalue_if_possible(card,option,PARSE_PORTNO,
						 &card->options.portno,i);
	}
	
	if (!strncmp(option,"add_hhlen:",10)) {
		i=qeth_getint(option+10,0);
		if (i==-1) {
			PRINT_ERR("parameter %s -- does not contain " \
				  "a valid number.\n",option);
			return 0;
		}
		if ( (i<0) || (i>MAX_ADD_HHLEN) ) {
			PRINT_ERR("parameter %s -- out of range\n",
				  option);
			return 0;
		}
		return qeth_setvalue_if_possible(card,option,PARSE_ADD_HHLEN,
						 &card->options.add_hhlen,i);
	}
	
	if (!strncmp(option,"portname:",9)) {
		a_p=&card->options.already_parsed[PARSE_PORTNAME];
		if (*a_p) {
			PRINT_ERR("parameter %s does not fit to " \
				  "previous parameters\n",option);
			return 0;
		}
		if ((strlen(option)-9)>8) {
			PRINT_ERR("parameter %s -- too long\n",option);
			return 0;
		}
		*a_p=1;
		card->options.portname[0]=strlen(option)-9;
		/* for beauty reasons: */
		for (i=1;i<9;i++)
			card->options.portname[i]=' ';
		strcpy(card->options.portname+1,option+9);
		for (i=1;i<9;i++)
			card->options.portname[i]=
				_ascebc[(unsigned char)
					card->options.portname[i]];
		return 1;
	}

	PRINT_ERR("unknown parameter: %s\n",option);

        return 0;
}

static void qeth_detach_handler(int irq,int status)
{
        qeth_card_t *card;
	int remove_method;
	char dbf_text[15];

	if (irq>=NR_IRQS) {
		irq-=NR_IRQS;
		remove_method=QETH_REMOVE_CARD_PROPER;
	} else {
		remove_method=QETH_REMOVE_CARD_QUICK;
	}

	QETH_DBF_TEXT1(0,trace,"detchhnd");
	sprintf(dbf_text,"%4x%4x",irq,status);
	QETH_DBF_TEXT1(0,trace,dbf_text);

	/* try to get the lock, if we didn't get it (hold by recovery),
	 * give up initiative to enable others to release the lock */
        my_spin_lock_nonbusy(NULL,&setup_lock);

	if ((card=qeth_get_card_by_irq(irq))) {
		qeth_remove_card(card,remove_method);
        }
	qeth_wakeup_procfile();
        my_spin_unlock(&setup_lock);
}

static void qeth_chandev_do_unregister_device(struct net_device *dev,
					      int quick) 
{
	qeth_card_t *card;
	
	card=(qeth_card_t *)dev->priv;
	
	if (quick) {
		qeth_detach_handler(card->irq0,0x1234);
	} else {
		qeth_detach_handler(NR_IRQS+card->irq0,0x1234);
	}
}

static void qeth_chandev_unregister_device(struct net_device *dev)
{
	qeth_chandev_do_unregister_device(dev, 0);
}

static void qeth_chandev_parse_options(qeth_card_t *card,char *parmstring)
{
	/* parse options coming from the chandev layer */
	char *optionstr;
	char *tmp;

	QETH_DBF_HEX2(0,misc,parmstring,
		      qeth_min(QETH_DBF_MISC_LEN,strlen(parmstring)));
	tmp = kmalloc ((strlen(parmstring) + 1) * sizeof (char), GFP_KERNEL);
	if (tmp) {
		memset (tmp, 0, strlen(parmstring) + 1);
		memcpy (tmp, parmstring, strlen(parmstring) + 1);
		do {
			char *end;
			int len;
			end = strchr (tmp, ',');
			if (end == NULL) {
				len = strlen (tmp) + 1;
			} else {
				len = (long) end - (long) tmp + 1;
			*end = '\0';
			end++;
			}

			if (len>1) {
				optionstr = kmalloc (len * sizeof (char),
						     GFP_KERNEL);
				if (optionstr) {
					memset(optionstr,0,len*sizeof(char));
					memcpy(optionstr,tmp,len*sizeof(char));
					
					qeth_parse_option(card,optionstr);
					kfree(optionstr);
					tmp = end;
				} else {
					PRINT_ERR("Cannot allocate memory " \
						  "for option parsing!\n");
					break;
				}
			}
		} while (tmp != NULL && *tmp != '\0');
	} else {
		PRINT_ERR("Cannot allocate memory " \
			  "for option parsing!\n");
	}
}

static int qeth_get_bufcnt_from_memamnt(qeth_card_t *card,__s32 *memamnt) 
{
	int cnt = 0;
	int bufsize = BUFFER_SIZE;

	if (bufsize == 24576)
		bufsize = 32768;
	if (bufsize == 40960) 
		bufsize = 65536;
	cnt = (*memamnt)*1024 / bufsize;
	cnt = (cnt<BUFCNT_MIN)?BUFCNT_MIN:((cnt>BUFCNT_MAX)?BUFCNT_MAX:cnt);
	(*memamnt) = cnt * bufsize/1024;

	return cnt;
}

static void qeth_correct_routing_status(qeth_card_t *card)
{
	if (card->type==QETH_CARD_TYPE_IQD) {
		/* if it's not a mc router, it's no router */
		if ( (card->options.routing_type4 == PRIMARY_ROUTER) ||
		     (card->options.routing_type4 == SECONDARY_ROUTER) 
#ifdef QETH_IPV6
		     ||
		     (card->options.routing_type6 == PRIMARY_ROUTER) ||
		     (card->options.routing_type6 == SECONDARY_ROUTER) 
#endif /* QETH_IPV6 */
		     ) {
			PRINT_WARN("routing not applicable, reset " \
				   "routing status.\n");
			card->options.routing_type4=NO_ROUTER;
#ifdef QETH_IPV6
			card->options.routing_type6=NO_ROUTER;
#endif /* QETH_IPV6 */
		}
		card->options.do_prio_queueing=NO_PRIO_QUEUEING;
	} else {
		/* if it's a mc router, it's no router */
		if ( (card->options.routing_type4 == MULTICAST_ROUTER) ||
		     (card->options.routing_type4 == PRIMARY_CONNECTOR) ||
		     (card->options.routing_type4 == SECONDARY_CONNECTOR) 
#ifdef QETH_IPV6
		     ||
		     (card->options.routing_type6 == MULTICAST_ROUTER) ||
		     (card->options.routing_type6 == PRIMARY_CONNECTOR) ||
		     (card->options.routing_type6 == SECONDARY_CONNECTOR) 
#endif /* QETH_IPV6 */
		     ) {
			PRINT_WARN("routing not applicable, reset " \
				   "routing status. (Did you mean " \
				   "primary_router or secondary_router?)\n");
			card->options.routing_type4=NO_ROUTER;
#ifdef QETH_IPV6
			card->options.routing_type6=NO_ROUTER;
#endif /* QETH_IPV6 */
		}
	}
}

static int qeth_attach_handler(int irq_to_scan,chandev_probeinfo *probeinfo)
{
	int result = 0;
        int irq1,irq2;
        unsigned int irq;
	int nr;
	__u8 mask;
	int read_chpid=-1,write_chpid=-2,data_chpid=-3; /* so that it will
							   fail, if getting
							   the chpids fails */
        qeth_card_t *card;
        int success = 0;
	char dbf_text[15];
	chandev_subchannel_info temp;

	QETH_DBF_TEXT2(0,trace,"athandlr");
	sprintf(dbf_text,"irq:%4x",irq_to_scan);
	QETH_DBF_TEXT2(0,trace,dbf_text);

        my_spin_lock_nonbusy(NULL,&setup_lock);

	for (nr=0; nr<8; nr++) {
		mask = 0x80 >> nr;
		if (probeinfo->read.pim & mask) {
			read_chpid=probeinfo->read.chpid[nr];
			/* we take the first chpid -- well, there's
			 * usually only one... */
			break;
		}
	}
	for (nr=0; nr<8; nr++) {
		mask = 0x80 >> nr;
		if (probeinfo->write.pim & mask) {
			write_chpid=probeinfo->write.chpid[nr];
			/* we take the first chpid -- well, there's
			 * usually only one... */
			break;
		}
	}
	for (nr=0; nr<8; nr++) {
		mask = 0x80 >> nr;
		if (probeinfo->data.pim & mask) {
			data_chpid=probeinfo->data.chpid[nr];
			/* we take the first chpid -- well, there's
			 * usually only one... */
			break;
		}
	}
	if ((read_chpid!=write_chpid)||(read_chpid!=data_chpid)) {
		PRINT_ERR("devices are not on the same CHPID!\n");
		goto endloop;
	}

	/* Try to reorder the devices, if neccessary */
	if (probeinfo->read.dev_model == 0x05)
		/* No odd/even restr. for IQD */
		goto correct_order;
	if ((probeinfo->read.devno %2 == 0) &&
	    (probeinfo->write.devno == probeinfo->read.devno + 1))
		goto correct_order;
	if ((probeinfo->write.devno %2 == 0) &&
	    (probeinfo->data.devno == probeinfo->write.devno + 1)) {
		temp = probeinfo->read;
		probeinfo->read = probeinfo->write;
		probeinfo->write = probeinfo->data;
		probeinfo->data = temp;
		goto correct_order;
	}
	if ((probeinfo->write.devno %2 == 0) &&
	    (probeinfo->read.devno == probeinfo->write.devno + 1)) {
		temp = probeinfo->read;
		probeinfo->read = probeinfo->write;
		probeinfo->write = temp;
		goto correct_order;
	}
	if ((probeinfo->read.devno %2 == 0) &&
	    (probeinfo->data.devno == probeinfo->read.devno + 1)) {
		temp = probeinfo->write;
		probeinfo->write = probeinfo->data;
		probeinfo->data = temp;
		goto correct_order;
	}
	if ((probeinfo->data.devno %2 == 0) &&
	    (probeinfo->write.devno == probeinfo->data.devno + 1)) {
		temp = probeinfo->read;
		probeinfo->read = probeinfo->data;
		probeinfo->data = temp;
		goto correct_order;
	}
	if ((probeinfo->data.devno %2 == 0) &&
	    (probeinfo->read.devno == probeinfo->data.devno + 1)) {
		temp = probeinfo->read;
		probeinfo->read = probeinfo->data;
		probeinfo->data = probeinfo->write;
		probeinfo->write = temp;
		goto correct_order;
	}
	PRINT_ERR("Failed to reorder devices %04x,%04x,%04x; "
		  "please check your configuration\n",
		  probeinfo->read.devno, probeinfo->write.devno,
		  probeinfo->data.devno);
	goto endloop;
	
correct_order:
	irq = probeinfo->read.irq;
	irq1 = probeinfo->write.irq;
	irq2 = probeinfo->data.irq;	

	card=qeth_alloc_card();
	if (card==NULL) {
		QETH_DBF_TEXT2(0,trace,"nocrdmem");
		PRINT_ERR("memory structures could not be allocated\n");
		goto endloop;
	}
	card->chpid=read_chpid;

	if (probeinfo->port_protocol_no != -1 )
		card->options.portno = probeinfo->port_protocol_no;
	else
		card->options.portno = 0;

	qeth_chandev_parse_options(card,probeinfo->parmstr);

	card->has_irq=0;
	card->irq0=irq;
	card->irq1=irq1;
	card->irq2=irq2;
	card->devno0=probeinfo->read.devno;
	card->devno1=probeinfo->write.devno;
	card->devno2=probeinfo->data.devno;
	card->dev_type=probeinfo->read.dev_type;
	card->dev_model=probeinfo->read.dev_model;
	atomic_set(&card->is_gone,0);
	atomic_set(&card->rt4fld,0);
#ifdef QETH_IPV6
	atomic_set(&card->rt6fld,0);
#endif /* QETH_IPV6 */
	
	sprintf(dbf_text,"atch%4x",card->irq0);
	QETH_DBF_TEXT1(0,setup,dbf_text);
	QETH_DBF_HEX1(0,setup,&card,sizeof(void*));
	QETH_DBF_HEX1(0,setup,&card->dev,sizeof(void*));
	QETH_DBF_HEX1(0,setup,&card->stats,sizeof(void*));
	QETH_DBF_HEX1(0,setup,&card->devstat0,sizeof(void*));
	QETH_DBF_HEX1(0,setup,&card->devstat1,sizeof(void*));
	QETH_DBF_HEX1(0,setup,&card->devstat2,sizeof(void*));
	
	QETH_DBF_HEX2(0,misc,&card->options,QETH_DBF_MISC_LEN);

	if (qeth_determine_card_type(card)) {
		qeth_free_card(card);
		goto endloop;
	}
	
	qeth_correct_routing_status(card);
	qeth_insert_card_into_list(card);
	
	QETH_DBF_TEXT3(0,trace,"request0");
	/* 0 is irqflags. what is SA_SAMPLE_RANDOM? */
	result=chandev_request_irq(irq,(void*)
				   qeth_interrupt_handler_read,
				   0,QETH_NAME,card->devstat0);
	if (result) goto attach_error;
	card->has_irq++;

	QETH_DBF_TEXT3(0,trace,"request1");
	result=chandev_request_irq(irq1,(void*)
				   qeth_interrupt_handler_write,
				   0,QETH_NAME,card->devstat1);
	if (result) goto attach_error;
	card->has_irq++;

	QETH_DBF_TEXT3(0,trace,"request2");
	result=chandev_request_irq(irq2,(void*)
				   qeth_interrupt_handler_qdio,
				   0,QETH_NAME,card->devstat2);
	if (result) goto attach_error;
	card->has_irq++;
	
	printk("qeth: Trying to use card with devnos 0x%X/0x%X/0x%X\n",
	       card->devno0,card->devno1,card->devno2);
	
	result=qeth_init_ringbuffers1(card);
	if (result) goto attach_error;
	
	result=qeth_hardsetup_card(card,0);
	if (result) goto attach_error;
	if (probeinfo->memory_usage_in_k != 0) {
		card->options.inbound_buffer_count=
			qeth_get_bufcnt_from_memamnt
			(card,&probeinfo->memory_usage_in_k);
		card->options.memory_usage_in_k=
			probeinfo->memory_usage_in_k;
	} else {
		/* sick... */
		probeinfo->memory_usage_in_k=
			-card->options.inbound_buffer_count*
			BUFFER_SIZE/1024;
	}
	result=qeth_init_ringbuffers2(card);
	if (result) goto attach_error;
	
	success = 1;
	goto endloop;

 attach_error:
	sprintf(dbf_text,"ATER%4x",card->irq0);
	QETH_DBF_TEXT2(0,trace,dbf_text);
	switch (result) {
	case 0:
		break;
	case -EINVAL:
		PRINT_ERR("oops... invalid parameter.\n");
                        break;
	case -EBUSY:
		PRINT_WARN("Device is busy!\n");
		break;
	case -ENODEV:
		PRINT_WARN("Device became not operational.\n");
		break;
	case -ENOMEM:
		PRINT_ERR("Not enough kernel memory for operation.\n");
		break;
	case -EIO:
		PRINT_ERR("There were problems in hard-setting up " \
			  "the card.\n");
		break;
	case -ETIME:
		PRINT_WARN("Timeout on initializing the card.\n");
		break;
	default:
		PRINT_ERR("Unknown error %d in attach_handler.\n",
			  result);
	}
	if (result) {
		qeth_remove_card(card,QETH_REMOVE_CARD_PROPER);
		
		qeth_remove_card_from_list(card);
		QETH_DBF_TEXT4(0,trace,"freecard");
		qeth_free_card(card);
	}
 endloop:

	QETH_DBF_TEXT3(0,trace,"leftloop");
	
        if (!success) {
		QETH_DBF_TEXT2(0,trace,"noaddcrd");

		/* we want to return which problem we had */
		result=result?result:-ENODEV;
                goto exit;
        }

    
 exit:
        my_spin_unlock(&setup_lock);
	
        return result;
}

static void qeth_chandev_msck_notfunc(struct net_device *device, 
       				      int msck_irq, 
       				      chandev_msck_status prevstatus,
				      chandev_msck_status newstatus )
{
 
	if (!(device->priv))
		return;
	
	if ((prevstatus != chandev_status_good) ||
	    (prevstatus != chandev_status_all_chans_good)) {
		if ((newstatus == chandev_status_good) ||
		    (newstatus == chandev_status_all_chans_good)) {
			qeth_card_t *card = (qeth_card_t *)device->priv;

			atomic_set(&card->problem,PROBLEM_MACHINE_CHECK);
			atomic_set(&card->write_busy,0);
			qeth_schedule_recovery(card);
		}
	}
	if ((newstatus == chandev_status_gone) ||
	    (newstatus == chandev_status_no_path) ||
	    (newstatus == chandev_status_not_oper)) {
		qeth_card_t *card = (qeth_card_t *)device->priv;

		my_read_lock(&list_lock);
		if (qeth_verify_card(card)) {
			atomic_set(&card->is_startlaned,0);
			qeth_set_dev_flag_norunning(card);
			/*
			 * Unfortunately, the chandev layer does not provide 
			 * a possibility to unregister a single device. So
			 * we mark the card as "gone" to avoid internal
			 * mishandling.
			 */
			atomic_set(&card->is_gone,1);
			/* means, we prevent looping in
			 * qeth_send_control_data */
			atomic_set(&card->write_busy,0);
			qeth_wakeup_procfile();
		}
		my_read_unlock(&list_lock);
	}
}

struct net_device *qeth_chandev_init_netdev(struct net_device *dev,
					   int sizeof_priv)
{
	
	qeth_card_t *card = NULL;
	int result; 
	char dbf_text[15];	
	
	if (!dev) {
		PRINT_ERR("qeth_chandev_init_netdev called with no device!\n");
		goto out;
	}

	card = (qeth_card_t *)dev->priv;
	strcpy(card->dev_name,dev->name);
	result=qeth_register_netdev(card);
	if (result) {
		PRINT_ALL("         register_netdev %s -- rc=%i\n",
			  ((qeth_card_t*)firstcard->dev->priv)->
			  dev_name,result);
		sprintf(dbf_text,"rgnd%4x",(__u16)result);
		QETH_DBF_TEXT2(1,trace,dbf_text);
		atomic_set(&card->is_registered,0);
		goto out;
	}
	strcpy(card->dev_name,dev->name);
	atomic_set(&card->write_busy,0);
        atomic_set(&card->is_registered,1);

        result=qeth_softsetup_card(card,QETH_WAIT_FOR_LOCK);

	if (!result) {
		qeth_init_input_buffers(card);
	} else {
		QETH_DBF_TEXT2(0,trace,"SSFAILED");
		PRINT_WARN("soft-setup of card failed!\n");
	}

	INIT_LIST_HEAD(&card->tqueue_sst.list);
	card->tqueue_sst.routine=qeth_softsetup_thread_starter;
	card->tqueue_sst.data=card;
	card->tqueue_sst.sync=0;
	schedule_task(&card->tqueue_sst);
 out:
	qeth_wakeup_procfile();
	return dev;

}

static int qeth_probe(chandev_probeinfo *probeinfo)
{
	int result;
	struct net_device *pdevice = NULL;
	int sizeof_priv;
	qeth_card_t *card;
	char *basename = NULL;

	result = qeth_attach_handler(probeinfo->read.irq, probeinfo);
	if (result)
		return result;

	sizeof_priv = sizeof(qeth_card_t);
	card = qeth_get_card_by_irq(probeinfo->read.irq); 

	pdevice = card->dev; 
	basename = (char *)qeth_get_dev_basename(card->type, card->link_type);

	pdevice->irq=card->irq0;

	if (probeinfo->memory_usage_in_k>=0)
		probeinfo->memory_usage_in_k=
			-card->options.memory_usage_in_k;
	pdevice = chandev_initnetdevice(probeinfo,
					card->options.portno,
					pdevice,
					sizeof_priv,
					basename,
					qeth_chandev_init_netdev,
					qeth_chandev_unregister_device);
	if (pdevice) {
		return 0;
	} else {
		qeth_remove_card(card,QETH_REMOVE_CARD_PROPER);
		qeth_remove_card_from_list(card);
		QETH_DBF_TEXT4(0,trace,"freecard");
		qeth_free_card(card);
		return -ENODEV;
	}
			
}

static int qeth_dev_event(struct notifier_block *this,
			  unsigned long event,void *ptr)
{
	qeth_card_t *card;
	struct net_device *dev = (struct net_device *)ptr;
	char dbf_text[15];

	sprintf(dbf_text,"devevent");
	QETH_DBF_TEXT3(0,trace,dbf_text);
	QETH_DBF_HEX3(0,trace,&event,sizeof(unsigned long));
	QETH_DBF_HEX3(0,trace,&dev,sizeof(void*));

#ifdef QETH_VLAN
	if (qeth_verify_dev(dev)==QETH_VERIFY_IS_VLAN_DEV)
		card = (qeth_card_t *)VLAN_DEV_INFO(dev)->real_dev->priv;
        else
#endif
	card=(qeth_card_t *)dev->priv;
        if (qeth_does_card_exist(card)) {
		qeth_save_dev_flag_state(card);
		switch (event) {
		default:
			qeth_start_softsetup_thread(card);
			break;
		}
	}

	return NOTIFY_DONE;
}

static int qeth_ip_event(struct notifier_block *this,
			 unsigned long event,void *ptr)
{
	qeth_card_t *card;
	struct in_ifaddr *ifa=(struct in_ifaddr *)ptr;
	struct net_device *dev = ifa->ifa_dev->dev;
	char dbf_text[15];

	sprintf(dbf_text,"ipevent");
	QETH_DBF_TEXT3(0,trace,dbf_text);
	QETH_DBF_HEX3(0,trace,&event,sizeof(unsigned long));
	QETH_DBF_HEX3(0,trace,&dev,sizeof(void*));
	sprintf(dbf_text,"%08x",ifa->ifa_address);
	QETH_DBF_TEXT3(0,trace,dbf_text);
	sprintf(dbf_text,"%08x",ifa->ifa_mask);
	QETH_DBF_TEXT3(0,trace,dbf_text);

#ifdef QETH_VLAN
	if (qeth_verify_dev(dev)==QETH_VERIFY_IS_VLAN_DEV)
		card = (qeth_card_t *)VLAN_DEV_INFO(dev)->real_dev->priv;
        else
#endif
	card=(qeth_card_t *)dev->priv;
        if (qeth_does_card_exist(card)) {
		QETH_DBF_HEX3(0,trace,&card,sizeof(void*));
		qeth_save_dev_flag_state(card);
		qeth_start_softsetup_thread(card);
	}

	return NOTIFY_DONE;
}

#ifdef QETH_IPV6
static int qeth_ip6_event(struct notifier_block *this,
		 	  unsigned long event,void *ptr)
{
	qeth_card_t *card;
	struct inet6_ifaddr *ifa=(struct inet6_ifaddr *)ptr;
	struct net_device *dev = ifa->idev->dev;
	char dbf_text[15];

	sprintf(dbf_text,"ip6event");
	QETH_DBF_TEXT3(0,trace,dbf_text);
	QETH_DBF_HEX3(0,trace,&event,sizeof(unsigned long));
	QETH_DBF_HEX3(0,trace,&dev,sizeof(void*));
	QETH_DBF_HEX3(0,trace,ifa->addr.s6_addr,QETH_DBF_TRACE_LEN);
	QETH_DBF_HEX3(0,trace,ifa->addr.s6_addr+QETH_DBF_TRACE_LEN,
		      QETH_DBF_TRACE_LEN);

#ifdef QETH_VLAN
	if (qeth_verify_dev(dev)==QETH_VERIFY_IS_VLAN_DEV)
		card = (qeth_card_t *)VLAN_DEV_INFO(dev)->real_dev->priv;
        else
#endif
	card=(qeth_card_t *)dev->priv;

        if (qeth_does_card_exist(card)) {
		QETH_DBF_HEX3(0,trace,&card,sizeof(void*));
		qeth_save_dev_flag_state(card);
		qeth_start_softsetup_thread(card);
	}

	return NOTIFY_DONE;
}
#endif /* QETH_IPV6 */

static int qeth_reboot_event(struct notifier_block *this,
   			     unsigned long event,void *ptr)
{
	qeth_card_t *card;

	my_read_lock(&list_lock);
	if (firstcard) {
		card = firstcard;
clear_another_one:
		if (card->has_irq) {
			if (card->type==QETH_CARD_TYPE_IQD) {
				halt_IO(card->irq2,0,0);
				clear_IO(card->irq0,0,0);
				clear_IO(card->irq1,0,0);
				clear_IO(card->irq2,0,0);
			} else {
				clear_IO(card->irq2,0,0);
				clear_IO(card->irq0,0,0);
				clear_IO(card->irq1,0,0);
			}
		}
		if (card->next) {
			card = card->next;
			goto clear_another_one;
		}
	} 
	my_read_unlock(&list_lock);

	return 0;
}

static struct notifier_block qeth_dev_notifier = {
        qeth_dev_event,
        0
};

static struct notifier_block qeth_ip_notifier = {
        qeth_ip_event,
        0
};

#ifdef QETH_IPV6
static struct notifier_block qeth_ip6_notifier = {
        qeth_ip6_event,
        0
};
#endif /* QETH_IPV6 */

static struct notifier_block qeth_reboot_notifier = {
        qeth_reboot_event,
        0
};

static void qeth_register_notifiers(void)
{
	int r;

	QETH_DBF_TEXT5(0,trace,"regnotif");
        /* register to be notified on events */
	r=register_netdevice_notifier(&qeth_dev_notifier);
        
	r=register_inetaddr_notifier(&qeth_ip_notifier);
#ifdef QETH_IPV6
	r=register_inet6addr_notifier(&qeth_ip6_notifier);
#endif /* QETH_IPV6 */
	r=register_reboot_notifier(&qeth_reboot_notifier);
}

#ifdef MODULE
static void qeth_unregister_notifiers(void)
{
	int r;

	QETH_DBF_TEXT5(0,trace,"unregnot");
	r=unregister_netdevice_notifier(&qeth_dev_notifier);
	r=unregister_inetaddr_notifier(&qeth_ip_notifier);
#ifdef QETH_IPV6
	r=unregister_inet6addr_notifier(&qeth_ip6_notifier);
#endif /* QETH_IPV6 */
	r=unregister_reboot_notifier(&qeth_reboot_notifier);
}
#endif /* MODULE */

static int qeth_procfile_open(struct inode *inode, struct file *file)
{
	int length=0;
	qeth_card_t *card;
	char checksum_str[5],queueing_str[14],router_str[8],bufsize_str[4];
	char *buffer;
	int rc=0;
	int size;
	tempinfo_t *info;

	info = (tempinfo_t *) vmalloc (sizeof (tempinfo_t));
	if (info == NULL) {
		PRINT_WARN("No memory available for data\n");
		return -ENOMEM;
	} else {
		file->private_data = (void *) info;
	}

	/* lock all the stuff */
	my_read_lock(&list_lock);
	card=firstcard;
	size=200; /* 2 lines plus some sanity space */
	while (card) {
		size+=90; /* if device name is > 10 chars, (should never
			     happen...), we'll need that */
		card=card->next;
	}

	buffer=info->data = (char *) vmalloc (size);
	if (info->data == NULL) {
		PRINT_WARN("No memory available for data\n");
		vfree (info);
		rc=-ENOMEM;
		goto out;
	}
	
	QETH_DBF_TEXT2(0,trace,"procread");
	length+=sprintf(buffer+length,
			"devnos (hex) CHPID     " \
			"device     cardtype port chksum prio-q'ing " \
			"rtr fsz C cnt\n");
	length+=sprintf(buffer+length,
			"-------------- --- ----" \
			"------ -------------- --     -- ---------- " \
			"--- --- - ---\n");
	card=firstcard;
	while (card) {
		strcpy(checksum_str,
		       (card->options.checksum_type==SW_CHECKSUMMING)?"SW":
		       (card->options.checksum_type==HW_CHECKSUMMING)?"HW":
		       "no");
		if (card->options.do_prio_queueing==NO_PRIO_QUEUEING) {
			sprintf(queueing_str,"always_q_%i",
				card->options.default_queue);
		} else {
			strcpy(queueing_str,(card->options.do_prio_queueing
					     ==PRIO_QUEUEING_PREC)?"by_prec.":
			       "by_ToS");
		}

#ifdef QETH_IPV6
		if (atomic_read(&card->rt4fld) &&
		    atomic_read(&card->rt6fld))
			strcpy(router_str, "no");
		else if (atomic_read(&card->rt4fld) ||
			 atomic_read(&card->rt6fld))
			strcpy(router_str, "mix");
#else /* QETH_IPV6 */
		if (atomic_read(&card->rt4fld))
			strcpy(router_str, "no");
#endif /* QETH_IPV6 */		
		else if ( ((card->options.routing_type4&ROUTER_MASK)==
		      PRIMARY_ROUTER) 
#ifdef QETH_IPV6
		     &&
		     ((card->options.routing_type6&ROUTER_MASK)==
		      PRIMARY_ROUTER) 
#endif /* QETH_IPV6 */
		     ) {
			strcpy(router_str,"pri");
		} else
		if ( ((card->options.routing_type4&ROUTER_MASK)==
		      SECONDARY_ROUTER) 
#ifdef QETH_IPV6
		     &&
		     ((card->options.routing_type6&ROUTER_MASK)==
		      SECONDARY_ROUTER) 
#endif /* QETH_IPV6 */
		     ) {
			strcpy(router_str,"sec");
		} else
		if ( ((card->options.routing_type4&ROUTER_MASK)==
		      MULTICAST_ROUTER) 
#ifdef QETH_IPV6
		     &&
		     ((card->options.routing_type6&ROUTER_MASK)==
		      MULTICAST_ROUTER) 
#endif /* QETH_IPV6 */
		     ) {
			strcpy(router_str,"mc");
		} else
		if ( ((card->options.routing_type4&ROUTER_MASK)==
		      PRIMARY_CONNECTOR) 
#ifdef QETH_IPV6
		     &&
		     ((card->options.routing_type6&ROUTER_MASK)==
		      PRIMARY_CONNECTOR) 
#endif /* QETH_IPV6 */
		     ) {
			strcpy(router_str,"p.c");
		} else
		if ( ((card->options.routing_type4&ROUTER_MASK)==
		      SECONDARY_CONNECTOR)
#ifdef QETH_IPV6
		     &&
		     ((card->options.routing_type6&ROUTER_MASK)==
		      SECONDARY_CONNECTOR) 
#endif /* QETH_IPV6 */
		     ) {
			strcpy(router_str,"s.c");
		} else
		if ( ((card->options.routing_type4&ROUTER_MASK)==
		      NO_ROUTER) 
#ifdef QETH_IPV6
		     &&
		     ((card->options.routing_type6&ROUTER_MASK)==
		      NO_ROUTER) 
#endif /* QETH_IPV6 */
		     ) {
			strcpy(router_str,"no");
		} else {
			strcpy(router_str,"mix");
		}
		strcpy(bufsize_str,
		       (BUFFER_SIZE==16384)?"16k":
		       (BUFFER_SIZE==24576)?"24k":
		       (BUFFER_SIZE==32768)?"32k":
		       (BUFFER_SIZE==40960)?"40k":
		       "64k");
		if (atomic_read(&card->is_gone)) {
			length+=sprintf(buffer+length,
					"%04X/%04X/%04X x%02X %10s %14s %2i" 
					"  +++ CARD IS GONE +++\n",
					card->devno0,card->devno1,card->devno2,
					card->chpid,
					card->dev_name,
					qeth_get_cardname_short
					(card->type,card->link_type),
					card->options.portno);
		} else if (!atomic_read(&card->is_startlaned)) {
			length+=sprintf(buffer+length,
					"%04X/%04X/%04X x%02X %10s %14s %2i" 
					"  +++ CABLE PULLED +++\n",
					card->devno0,card->devno1,card->devno2,
					card->chpid,
					card->dev_name,
					qeth_get_cardname_short
					(card->type,card->link_type),
					card->options.portno);
		} else {
			length+=sprintf(buffer+length,
					"%04X/%04X/%04X x%02X %10s %14s %2i" \
					"     %2s %10s %3s %3s %c %3i\n",
					card->devno0,card->devno1,card->devno2,
					card->chpid,card->dev_name,
					qeth_get_cardname_short
					(card->type,card->link_type),
					card->options.portno,
					checksum_str,
					queueing_str,router_str,bufsize_str,
					(card->options.memusage==
					 MEMUSAGE_CONTIG)?'c':' ',
					card->options.inbound_buffer_count);
		}
		card=card->next;
	}

out:
	info->len=length;
	/* unlock all the stuff */
	my_read_unlock(&list_lock);
	return rc;
}

static qeth_card_t *qeth_find_card(char *buffer,int len)
{
        qeth_card_t *card;
	int devnamelen;

	my_read_lock(&list_lock);
        card=firstcard;
        while (card) {
		devnamelen=0;
		while ( (devnamelen<DEV_NAME_LEN) &&
			( (card->dev_name[devnamelen]!=' ') &&
			  (card->dev_name[devnamelen]!=0) &&
			  (card->dev_name[devnamelen]!='\n') ) ) {
			devnamelen++;
		}
                if ((!strncmp(card->dev_name,buffer,
			      qeth_min(len,DEV_NAME_LEN)))&&
		    (devnamelen==len) &&
		    (!atomic_read(&card->shutdown_phase))) break;
                card=card->next;
        }
	my_read_unlock(&list_lock);

	return card;
}

static int qeth_get_next_token(char *buffer,int *token_len,
			       int *pos,int *end_pos,int count)
{
	*token_len=0;
	*end_pos=*pos;
	if (*end_pos>=count) return 0;
	if (!buffer[*end_pos]) return 0;
	for (;;) {
		if ( (buffer[*end_pos]!=' ') &&
		     (buffer[*end_pos]!='\t') &&
		     (buffer[*end_pos]!='\n') &&
		     (buffer[*end_pos]!='\r') &&
		     (buffer[*end_pos]!=0) &&
		     (*end_pos<count) ) {
			*end_pos=(*end_pos)+1;
			*token_len=(*token_len)+1;
		}
		else break;
	}
	return 1;
}

static void qeth_skip_whitespace(char *buffer,int *pos,int count)
{
	for (;;) {
		if (*pos>=count) return;
		if ((buffer[*pos]==' ')||
		    (buffer[*pos]=='\t')||
		    (buffer[*pos]=='\n')||
		    (buffer[*pos]=='\r')) *pos=(*pos)+1;
		else return;
	}
}

#define CHECK_MISSING_PARAMETER do { \
	pos=end_pos; \
	if (!qeth_get_next_token(buffer,&token_len,&pos,&end_pos,user_len)) { \
		PRINT_WARN("paramter missing on procfile input, " \
			   "ignoring input!\n"); \
		goto out; \
	} \
	card=qeth_find_card(buffer+pos,token_len); \
	if (!card) { \
		PRINT_WARN("paramter invalid on procfile input, " \
			   "ignoring input!\n"); \
		goto out; \
	} \
} while (0)

static ssize_t qeth_procfile_write(struct file *file,
	  			   const char *user_buffer,
  				   size_t user_len,loff_t *offset)
{
	qeth_card_t *card;
	char *buffer;
	int token_len;
	int pos=0,end_pos;
	char dbf_text[15];

	if (*offset>0) return user_len;
	buffer=vmalloc(__max(user_len+1,QETH_DBF_MISC_LEN));
	if (buffer == NULL)
		return -ENOMEM;
	memset(buffer,0,user_len+1);

	if (copy_from_user(buffer, user_buffer, user_len)) {
		vfree (buffer);
		return -EFAULT;
	}
		
	QETH_DBF_TEXT2(0,trace,"procwrit");
	QETH_DBF_TEXT2(0,misc,buffer);

	if (!qeth_get_next_token(buffer,&token_len,&pos,&end_pos,user_len))
		goto out;
	qeth_skip_whitespace(buffer,&end_pos,user_len);

	if (!strncmp(buffer+pos,"recover",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"UTRC%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			atomic_set(&card->problem,
				   PROBLEM_USER_TRIGGERED_RECOVERY);
			qeth_schedule_recovery(card);

	} else if (!strncmp(buffer+pos,"remember",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"remb%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			card->save_state_flag=1;

	} else if (!strncmp(buffer+pos,"forget",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"forg%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);
			card->save_state_flag=0;
	} else if (!strncmp(buffer+pos,"primary_router",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"prir%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=PRIMARY_ROUTER|
				RESET_ROUTING_FLAG;
#ifdef QETH_IPV6
			card->options.routing_type6=PRIMARY_ROUTER|
				RESET_ROUTING_FLAG;
#endif /* QETH_IPV6 */
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
#ifdef QETH_IPV6
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
#endif /* QETH_IPV6 */
			qeth_start_softsetup_thread(card);
	} else if (!strncmp(buffer+pos,"primary_router4",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"pri4%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=PRIMARY_ROUTER|
				RESET_ROUTING_FLAG;
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#ifdef QETH_IPV6
	} else if (!strncmp(buffer+pos,"primary_router6",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"pri6%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type6=PRIMARY_ROUTER|
				RESET_ROUTING_FLAG;
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#endif /* QETH_IPV6 */
	} else if (!strncmp(buffer+pos,"secondary_router",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"secr%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=SECONDARY_ROUTER|
				RESET_ROUTING_FLAG;
#ifdef QETH_IPV6
			card->options.routing_type6=SECONDARY_ROUTER|
				RESET_ROUTING_FLAG;
#endif /* QETH_IPV6 */
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
#ifdef QETH_IPV6
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
#endif /* QETH_IPV6 */
			qeth_start_softsetup_thread(card);
	} else if (!strncmp(buffer+pos,"secondary_router4",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"sec4%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=SECONDARY_ROUTER|
				RESET_ROUTING_FLAG;
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#ifdef QETH_IPV6
	} else if (!strncmp(buffer+pos,"secondary_router6",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"sec6%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type6=SECONDARY_ROUTER|
				RESET_ROUTING_FLAG;
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#endif /* QETH_IPV6 */
	} else if (!strncmp(buffer+pos,"multicast_router",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"mcr %4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=MULTICAST_ROUTER|
				RESET_ROUTING_FLAG;
#ifdef QETH_IPV6
			card->options.routing_type6=MULTICAST_ROUTER|
				RESET_ROUTING_FLAG;
#endif /* QETH_IPV6 */
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
#ifdef QETH_IPV6
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
#endif /* QETH_IPV6 */
			qeth_start_softsetup_thread(card);
	} else if (!strncmp(buffer+pos,"multicast_router4",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"mcr4%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=MULTICAST_ROUTER|
				RESET_ROUTING_FLAG;
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#ifdef QETH_IPV6
	} else if (!strncmp(buffer+pos,"multicast_router6",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"mcr6%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type6=MULTICAST_ROUTER|
				RESET_ROUTING_FLAG;
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#endif /* QETH_IPV6 */
	} else if (!strncmp(buffer+pos,"primary_connector",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"prc %4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=PRIMARY_CONNECTOR|
				RESET_ROUTING_FLAG;
#ifdef QETH_IPV6
			card->options.routing_type6=PRIMARY_CONNECTOR|
				RESET_ROUTING_FLAG;
#endif /* QETH_IPV6 */
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
#ifdef QETH_IPV6
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
#endif /* QETH_IPV6 */
			qeth_start_softsetup_thread(card);
	} else if (!strncmp(buffer+pos,"primary_connector4",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"prc4%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=PRIMARY_CONNECTOR|
				RESET_ROUTING_FLAG;
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#ifdef QETH_IPV6
	} else if (!strncmp(buffer+pos,"primary_connector6",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"prc6%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type6=PRIMARY_CONNECTOR|
				RESET_ROUTING_FLAG;
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#endif /* QETH_IPV6 */
	} else if (!strncmp(buffer+pos,"secondary_connector",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"scc %4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=SECONDARY_CONNECTOR|
				RESET_ROUTING_FLAG;
#ifdef QETH_IPV6
			card->options.routing_type6=SECONDARY_CONNECTOR|
				RESET_ROUTING_FLAG;
#endif /* QETH_IPV6 */
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
#ifdef QETH_IPV6
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
#endif /* QETH_IPV6 */
			qeth_start_softsetup_thread(card);
	} else if (!strncmp(buffer+pos,"secondary_connector4",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"scc4%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=SECONDARY_CONNECTOR|
				RESET_ROUTING_FLAG;
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#ifdef QETH_IPV6
	} else if (!strncmp(buffer+pos,"secondary_connector6",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"scc6%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type6=SECONDARY_CONNECTOR|
				RESET_ROUTING_FLAG;
			qeth_correct_routing_status(card);
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#endif /* QETH_IPV6 */
	} else if (!strncmp(buffer+pos,"no_router",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"nor %4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=NO_ROUTER|
				RESET_ROUTING_FLAG;
#ifdef QETH_IPV6
			card->options.routing_type6=NO_ROUTER|
				RESET_ROUTING_FLAG;
#endif /* QETH_IPV6 */
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
#ifdef QETH_IPV6
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
#endif /* QETH_IPV6 */
			qeth_start_softsetup_thread(card);
	} else if (!strncmp(buffer+pos,"no_router4",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"nor4%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type4=NO_ROUTER|
				RESET_ROUTING_FLAG;
			atomic_set(&card->enable_routing_attempts4,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#ifdef QETH_IPV6
	} else if (!strncmp(buffer+pos,"no_router6",token_len)) {
			CHECK_MISSING_PARAMETER;
			sprintf(dbf_text,"nor6%4x",card->irq0);
			QETH_DBF_TEXT2(0,trace,dbf_text);

			card->options.routing_type6=NO_ROUTER|
				RESET_ROUTING_FLAG;
			atomic_set(&card->enable_routing_attempts6,
				   QETH_ROUTING_ATTEMPTS);
			qeth_start_softsetup_thread(card);
#endif /* QETH_IPV6 */
	} else {
		PRINT_WARN("unknown command input in procfile\n");
	}
#undef CHECK_MISSING_PARAMETER
out:

	*offset = *offset + user_len;
	vfree(buffer);

	return user_len;
}

#define _OUTP_IT(x...) c+=sprintf(buffer+c,x)

#ifdef QETH_PERFORMANCE_STATS
static int qeth_perf_procfile_read(char *buffer,char **buffer_location,
		    		   off_t offset,int buffer_length,int *eof,
				   void *data)
{
	int c=0;
	qeth_card_t *card;
	/* we are always called with buffer_length=4k, so we all
	   deliver on the first read */
	if (offset>0) return 0;

	QETH_DBF_TEXT2(0,trace,"perfpfrd");

	card=firstcard;

	while (card) {
		_OUTP_IT("For card with devnos 0x%X/0x%X/0x%X (%s):\n",
			 card->devno0,card->devno1,card->devno2,
			 card->dev_name);
		_OUTP_IT("  Skb's/buffers received                 : %i/%i\n",
			 card->perf_stats.skbs_rec,
			 card->perf_stats.bufs_rec);
		_OUTP_IT("  Skb's/buffers sent                     : %i/%i\n",
			 card->perf_stats.skbs_sent,
			 card->perf_stats.bufs_sent);
		_OUTP_IT("\n");
		_OUTP_IT("  Skb's/buffers sent without packing     : %i/%i\n",
			 card->perf_stats.skbs_sent_dont_pack,
			 card->perf_stats.bufs_sent_dont_pack);
		_OUTP_IT("  Skb's/buffers sent with packing        : %i/%i\n",
			 card->perf_stats.skbs_sent_pack,
			 card->perf_stats.bufs_sent_pack);
		_OUTP_IT("\n");
		_OUTP_IT("  Packing state changes no pkg.->packing : %i/%i\n",
			 card->perf_stats.sc_dp_p,
			 card->perf_stats.sc_p_dp);
		_OUTP_IT("  Current buffer usage (outbound q's)    : " \
			 "%i/%i/%i/%i\n",
			 atomic_read(&card->outbound_used_buffers[0]),
			 atomic_read(&card->outbound_used_buffers[1]),
			 atomic_read(&card->outbound_used_buffers[2]),
			 atomic_read(&card->outbound_used_buffers[3]));
		_OUTP_IT("\n");
		_OUTP_IT("  Inbound time (in us)                   : %i\n",
			 card->perf_stats.inbound_time);
		_OUTP_IT("  Inbound cnt                            : %i\n",
			 card->perf_stats.inbound_cnt);
		_OUTP_IT("  Outbound time (in us, incl QDIO)       : %i\n",
			 card->perf_stats.outbound_time);
		_OUTP_IT("  Outbound cnt                           : %i\n",
			 card->perf_stats.outbound_cnt);
		_OUTP_IT("  Watermarks: L/H=%i/%i\n",
			 LOW_WATERMARK_PACK,HIGH_WATERMARK_PACK);
		_OUTP_IT("\n");

		card=card->next;
	}

	return c;
}

static struct proc_dir_entry *qeth_perf_proc_file;

#endif /* QETH_PERFORMANCE_STATS */

static int qeth_ipato_procfile_open(struct inode *inode, struct file *file)
{
	char text[33];
	ipato_entry_t *ipato_entry;
	qeth_card_t *card;
	qeth_vipa_entry_t *vipa_entry;
	int rc=0;
	tempinfo_t *info;
	int size;
	char entry_type[5];

	info = (tempinfo_t *) vmalloc (sizeof (tempinfo_t));
	if (info == NULL) {
		PRINT_WARN("No memory available for data\n");
		return -ENOMEM;
	} else {
		file->private_data = (void *) info;
	}
	info->len=0;

	QETH_DBF_TEXT2(0,trace,"ipatorea");
	/* lock all the stuff */
	my_spin_lock(&ipato_list_lock);
	my_read_lock(&list_lock);

	size=64; /* for inv4/6 etc. */

	ipato_entry=ipato_entries;
	while (ipato_entry) {
		ipato_entry=ipato_entry->next;
		size+=64;
	}
	card=firstcard;
	while (card) {
		my_read_lock(&card->vipa_list_lock);
		vipa_entry=card->vipa_list;
		while (vipa_entry) {
			vipa_entry=vipa_entry->next;
			size+=64;
		}
		/*my_read_unlock(&card->vipa_list_lock); don't unlock it here*/
		card=card->next;
	}
	info->data = (char *) vmalloc (size);
	if (info->data == NULL) {
		PRINT_WARN("No memory available for data\n");
		vfree (info);
		rc=-ENOMEM;
		goto out;
	}

#define _IOUTP_IT(x...) info->len+=sprintf(info->data+info->len,x)
	if (ipato_inv4) 
		_IOUTP_IT("inv4\n");
	ipato_entry=ipato_entries;
	text[8]=0;
	while (ipato_entry) {
		if (ipato_entry->version==4) {
			qeth_convert_addr_to_text(4,ipato_entry->addr,text);
			_IOUTP_IT("add4 %s/%i%s%s\n",text,
				 ipato_entry->mask_bits,
				 ipato_entry->dev_name[0]?":":"",
				 ipato_entry->dev_name[0]?
				 ipato_entry->dev_name:"");
		}
		ipato_entry=ipato_entry->next;
	}
	
	if (ipato_inv6) 
		_IOUTP_IT("inv6\n");
	ipato_entry=ipato_entries;
	text[32]=0;
	while (ipato_entry) {
		if (ipato_entry->version==6) {
			qeth_convert_addr_to_text(6,ipato_entry->addr,text);
			_IOUTP_IT("add6 %s/%i%s%s\n",text,
				 ipato_entry->mask_bits,
				 ipato_entry->dev_name[0]?":":"",
				 ipato_entry->dev_name[0]?
				 ipato_entry->dev_name:"");
		}
		ipato_entry=ipato_entry->next;
	}
	card=firstcard;
	while (card) {
		vipa_entry=card->vipa_list;
		while (vipa_entry) {
			strcpy(entry_type,(vipa_entry->flag==
					   IPA_SETIP_VIPA_FLAGS)?
			       "vipa":"rxip");
			if (vipa_entry->version==4) {
				_IOUTP_IT("add_%s4 %02x%02x%02x%02x:%s\n",
					  entry_type,
					  vipa_entry->ip[0],
					  vipa_entry->ip[1],
					  vipa_entry->ip[2],
					  vipa_entry->ip[3],
					  card->dev_name);
			} else {
				_IOUTP_IT("add_%s6 %02x%02x%02x%02x" \
					 "%02x%02x%02x%02x" \
					 "%02x%02x%02x%02x" \
					 "%02x%02x%02x%02x:%s\n",
					  entry_type,
					  vipa_entry->ip[0],
					  vipa_entry->ip[1],
					  vipa_entry->ip[2],
					  vipa_entry->ip[3],
					  vipa_entry->ip[4],
					  vipa_entry->ip[5],
					  vipa_entry->ip[6],
					  vipa_entry->ip[7],
					  vipa_entry->ip[8],
					  vipa_entry->ip[9],
					  vipa_entry->ip[10],
					  vipa_entry->ip[11],
					  vipa_entry->ip[12],
					  vipa_entry->ip[13],
					  vipa_entry->ip[14],
					  vipa_entry->ip[15],
					  card->dev_name);
			}
			vipa_entry=vipa_entry->next;
		}
		card=card->next;
	}
out:
	/* unlock all the stuff */
	card=firstcard;
	while (card) {
		/*my_read_lock(&card->vipa_list_lock); don't lock it here */
		my_read_unlock(&card->vipa_list_lock);
		card=card->next;
	}
	my_read_unlock(&list_lock);
	my_spin_unlock(&ipato_list_lock);
	
	return rc;
}

static ssize_t qeth_procfile_read(struct file *file,char *user_buf,
				  size_t user_len,loff_t * offset)
{
	loff_t len;
	tempinfo_t *p_info = (tempinfo_t *) file->private_data;
	
	if (*offset >= p_info->len) {
		return 0;
	} else {
		len = __min(user_len, (p_info->len - *offset));
		if (copy_to_user (user_buf, &(p_info->data[*offset]), len))
			return -EFAULT;
		(*offset) += len;
		return len;
	}
}

/* ATT: this is also the procfile release function for the ipato
 * procfs entry */
static int qeth_procfile_release(struct inode *inode,struct file *file)
{
   	tempinfo_t *p_info = (tempinfo_t *) file->private_data;
	
	if (p_info) {
		if (p_info->data)
			vfree (p_info->data);
		vfree (p_info);
	}
	
	return 0;
}

static ssize_t qeth_ipato_procfile_write(struct file *file,
					 const char *user_buffer,
					 size_t user_len,loff_t *offset)
{
	int add,version;
	char text[33];
	__u8 addr[16];
	int len,i,flag;
	int mask_bits;
	char *buffer;
	int dev_name_there;
	char *dev_name_ptr;
	qeth_card_t *card;
#define BUFFER_LEN (10+32+1+5+1+DEV_NAME_LEN+1)

	if (*offset>0) return user_len;
	buffer=vmalloc(__max(__max(user_len+1,BUFFER_LEN),QETH_DBF_MISC_LEN));

	if (buffer == NULL)
		return -ENOMEM;
	/* BUFFER_LEN=command incl. blank+addr+slash+mask_bits+
	 * colon+DEV_NAME_LEN+zero */
	memset(buffer,0,BUFFER_LEN);

	if (copy_from_user(buffer, user_buffer, user_len)) {
		vfree (buffer);
		return -EFAULT;
	}
		
	QETH_DBF_TEXT2(0,trace,"ipatowri");
	QETH_DBF_TEXT2(0,misc,buffer);
	if (!strncmp(buffer,"inv4",4)) {
		ipato_inv4=1-ipato_inv4;
		goto out;
	}
	if (!strncmp(buffer,"inv6",4)) {
		ipato_inv6=1-ipato_inv6;
		goto out;
	}
	if ( (!strncmp(buffer,"add4 ",5)) ||
	     (!strncmp(buffer,"add6 ",5)) ||
	     (!strncmp(buffer,"del4 ",5)) ||
	     (!strncmp(buffer,"del6 ",5)) ) {
		text[8]=0;
		text[32]=0;
		add=!strncmp(buffer,"add",3);
		version=(buffer[3]=='4')?4:6;
		len=(version==4)?8:32;
		strncpy(text,buffer+5,len);
		if (qeth_convert_text_to_addr(version,text,addr)) {
			PRINT_ERR("error in parsing ipato information " \
				  "(addr)\n");
			goto out;
		}
		strncpy(text,buffer+5+len+1,10);
		/* we prepare mask_bits for qeth_getints */
		dev_name_there=0;
		for (i=5+len+1;i<BUFFER_LEN;i++) {
			if (*(buffer+i)=='\n') {
				*(buffer+i)=0;
				break;
			}
			if (*(buffer+i)==':') {
				*(buffer+i)=0; /* so that qeth_getint works */
				dev_name_there=i;
				break;
			}
			if (*(buffer+i)==0)
				break;
		}
		mask_bits=qeth_getint(buffer+5+len+1,0);
		if ((mask_bits<0)||(mask_bits>((version==4)?32:128))) {
			PRINT_ERR("error in parsing ipato information " \
				  "(mask bits)\n");
			goto out;
		}
		if (dev_name_there) {
			dev_name_ptr=buffer+dev_name_there+1;
			/* wipe out the linefeed */
			for (i=dev_name_there+1;
			     i<dev_name_there+1+DEV_NAME_LEN+1;i++)
				if (*(buffer+i)=='\n') *(buffer+i)=0;
		} else
			dev_name_ptr=NULL;

		if (add)
			qeth_add_ipato_entry(version,addr,mask_bits,
					     dev_name_ptr);
		else
			qeth_del_ipato_entry(version,addr,mask_bits,
					     dev_name_ptr);
		goto out;
	}
	if ( (!strncmp(buffer,"add_vipa4 ",10)) ||
	     (!strncmp(buffer,"add_rxip4 ",10)) ||
	     (!strncmp(buffer,"add_vipa6 ",10)) ||
	     (!strncmp(buffer,"add_rxip6 ",10)) ||
	     (!strncmp(buffer,"del_vipa4 ",10)) ||
	     (!strncmp(buffer,"del_rxip4 ",10)) ||
	     (!strncmp(buffer,"del_vipa6 ",10)) ||
	     (!strncmp(buffer,"del_rxip6 ",10)) ) {
		text[8]=0;
		text[32]=0;
		add=!strncmp(buffer,"add",3);
		flag=(!strncmp(buffer+4,"vipa",4))?IPA_SETIP_VIPA_FLAGS:
			IPA_SETIP_TAKEOVER_FLAGS;
		version=(buffer[8]=='4')?4:6;
		len=(version==4)?8:32;
		strncpy(text,buffer+10,len);
		if (qeth_convert_text_to_addr(version,text,addr)) {
			PRINT_ERR("error in parsing vipa/rxip information " \
				  "(addr)\n");
			goto out;
		}
		if (*(buffer+10+len)!=':') {
			PRINT_ERR("error in parsing vipa/rxip information " \
				  "(no interface)\n");
			goto out;
		}
		/* interface name is at buffer+10+len+1 */
		/* wipe out the \n */
		for (i=10+len+1;i<10+len+1+DEV_NAME_LEN+1;i++)
			if (*(buffer+i)=='\n') *(buffer+i)=0;
		card=qeth_get_card_by_name(buffer+10+len+1);
		if (!card) {
			PRINT_ERR("error in parsing vipa/rxip information " \
				  "(unknown interface)\n");
			goto out;
		}
		if (add)
			i=qeth_add_vipa_entry(card,version,addr,flag);
		else
			i=qeth_del_vipa_entry(card,version,addr,flag);
		if (!i) qeth_start_softsetup_thread(card);
		goto out;
	}
	PRINT_ERR("unknown ipato information command\n");
out:
	vfree(buffer);
	*offset = *offset + user_len;
#undef BUFFER_LEN
	return user_len;
}

static int qeth_procfile_getinterfaces(unsigned long arg) 
{
	qeth_card_t *card;

	char parms[16];
	char *buffer;
	char *buffer_pointer;
	__u32 version,valid_fields,qeth_version,number_of_devices,if_index;
	__u32 data_size,data_len;
	unsigned long ioctl_flags;
	int result=0;

	/* the struct of version 0 is:
typedef struct dev_list
{
  char device_name[IFNAME_MAXLEN]; // OSA-Exp device name (e.g. eth0)
  __u32 if_index;                  // interface index from kernel
  __u32 flags;             	   // device charateristics
} __attribute__((packed)) DEV_LIST;

typedef struct osaexp_dev_ver0
{
  __u32 version;                // structure version
  __u32 valid_fields;           // bitmask of fields that are really filled
  __u32 qeth_version;           // qeth driver version
  __u32 number_of_devices;      // number of OSA Express devices
  struct dev_list devices[0]; // list of OSA Express devices
} __attribute__((packed)) OSAEXP_DEV_VER0;
	*/

	version = 0;
	valid_fields = 0;
	qeth_version = 0;
	number_of_devices= 0;

	copy_from_user((void*)parms,(void*)arg,sizeof(parms));
	memcpy(&data_size,parms,sizeof(__u32));

	if ( !(data_size > 0) ) 
	     return -EFAULT;
	if ( data_size > IOCTL_MAX_TRANSFER_SIZE ) 
	     return -EFAULT;
	if ( !access_ok(VERIFY_WRITE, (void *)arg, data_size) )
	     return -EFAULT;

	my_read_lock(&list_lock);
	card = firstcard;
#define IOCTL_USER_STRUCT_SIZE (DEV_NAME_LEN*sizeof(char)) + \
	sizeof(__u32) + sizeof(__u32)
	while (card) {
	     if (card->type == QETH_CARD_TYPE_OSAE)
		  number_of_devices=number_of_devices + IOCTL_USER_STRUCT_SIZE;
	     card = card->next;
	}
#undef IOCTL_USER_STRUCT_SIZE
	if ((number_of_devices + 4*sizeof(__u32)) >= data_size) {
		result=-ENOMEM;
		goto out;
	}

	number_of_devices=0;
	card = firstcard;
	buffer = (char *)vmalloc(data_size);
	if (!buffer) {
		result=-EFAULT;
		goto out;
	}
	buffer_pointer = ((char *)(buffer)) + (4*sizeof(__u32)) ; 
	while (card) {
	     if ((card->type == QETH_CARD_TYPE_OSAE)&&
		 (!atomic_read(&card->is_gone))&&
		 (atomic_read(&card->is_hardsetup))&&
		 (atomic_read(&card->is_registered))) {

		  memcpy(buffer_pointer,card->dev_name,DEV_NAME_LEN);
		  buffer_pointer = buffer_pointer + DEV_NAME_LEN;
		  if_index=card->dev->ifindex;
		  memcpy(buffer_pointer,&if_index,sizeof(__u32));
		  buffer_pointer = buffer_pointer + sizeof(__u32);
		  memcpy(buffer_pointer,&ioctl_flags,sizeof(__u32));
		  buffer_pointer = buffer_pointer + sizeof(__u32);
		  number_of_devices=number_of_devices+1;
	     }
	     card = card->next;
	}

	/* we copy the real size */
	data_len=buffer_pointer-buffer;

	buffer_pointer = buffer;
	/* copy the header information at the beginning of the buffer */
	memcpy(buffer_pointer,&version,sizeof(__u32));
	memcpy(((char *)buffer_pointer)+sizeof(__u32),&valid_fields,
	       sizeof(__u32));
	memcpy(((char *)buffer_pointer)+(2*sizeof(__u32)),&qeth_version,
	       sizeof(__u32));
	memcpy(((char *)buffer_pointer)+(3*sizeof(__u32)),&number_of_devices,
	       sizeof(__u32));
	copy_to_user((char *)arg,buffer,data_len);
	vfree(buffer);
out:
	my_read_unlock(&list_lock);
	return result;

#undef PARMS_BUFFERLENGTH

};

static int qeth_procfile_interfacechanges(unsigned long arg) 
{
	return qeth_sleepon_procfile();

} 

static int qeth_procfile_ioctl(struct inode *inode, struct file *file,
			       unsigned int cmd, unsigned long arg) 
{

     int result;
     down_interruptible(&qeth_procfile_ioctl_lock);
     switch (cmd) {

     case QETH_IOCPROC_OSAEINTERFACES:
	     result = qeth_procfile_getinterfaces(arg);
	     break;
     case QETH_IOCPROC_INTERFACECHANGES:
	     result = qeth_procfile_interfacechanges(arg);
	     break;
     default:
	     result = -EOPNOTSUPP;	     
     }
     up(&qeth_procfile_ioctl_lock);
     return result;
};

static struct file_operations qeth_procfile_fops =
{
	ioctl:qeth_procfile_ioctl,
	read:qeth_procfile_read,
	write:qeth_procfile_write,
	open:qeth_procfile_open,
	release:qeth_procfile_release,
};

static struct proc_dir_entry *qeth_proc_file;

static struct file_operations qeth_ipato_procfile_fops =
{
	read:qeth_procfile_read, /* same as above! */
	write:qeth_ipato_procfile_write,
	open:qeth_ipato_procfile_open,
	release:qeth_procfile_release /* same as above! */
};

static struct proc_dir_entry *qeth_ipato_proc_file;

static void qeth_add_procfs_entries(void)
{
	proc_file_registration=0;
	qeth_proc_file=create_proc_entry(QETH_PROCFILE_NAME,
					 S_IFREG|0644,&proc_root);
	if (qeth_proc_file) {
		qeth_proc_file->proc_fops = &qeth_procfile_fops;
		sema_init(&qeth_procfile_ioctl_sem,
			  PROCFILE_SLEEP_SEM_MAX_VALUE);
		sema_init(&qeth_procfile_ioctl_lock,
			  PROCFILE_IOCTL_SEM_MAX_VALUE);
	} else proc_file_registration=-1;

	if (proc_file_registration)
		PRINT_WARN("was not able to register proc-file (%i).\n",
			   proc_file_registration);
	proc_ipato_file_registration=0;
	qeth_ipato_proc_file=create_proc_entry(QETH_IPA_PROCFILE_NAME,
		  			       S_IFREG|0644,&proc_root);
	if (qeth_ipato_proc_file) {
		qeth_ipato_proc_file->proc_fops = &qeth_ipato_procfile_fops;
	} else proc_ipato_file_registration=-1;

	if (proc_ipato_file_registration)
		PRINT_WARN("was not able to register ipato-proc-file (%i).\n",
			   proc_ipato_file_registration);

#ifdef QETH_PERFORMANCE_STATS
	proc_perf_file_registration=0;
	qeth_perf_proc_file=create_proc_entry(QETH_PERF_PROCFILE_NAME,
					      S_IFREG|0444,&proc_root);
	if (qeth_perf_proc_file) {
		qeth_perf_proc_file->read_proc=&qeth_perf_procfile_read;
	} else proc_perf_file_registration=-1;

	if (proc_perf_file_registration)
		PRINT_WARN("was not able to register perf. proc-file (%i).\n",
			   proc_perf_file_registration);
#endif /* QETH_PERFORMANCE_STATS */
}

#ifdef MODULE
static void qeth_remove_procfs_entries(void)
{
	if (!proc_file_registration) /* means if it went ok earlier */
		remove_proc_entry(QETH_PROCFILE_NAME,&proc_root);

	if (!proc_ipato_file_registration) /* means if it went ok earlier */
		remove_proc_entry(QETH_IPA_PROCFILE_NAME,&proc_root);

#ifdef QETH_PERFORMANCE_STATS
	if (!proc_perf_file_registration) /* means if it went ok earlier */
		remove_proc_entry(QETH_PERF_PROCFILE_NAME,&proc_root);
#endif /* QETH_PERFORMANCE_STATS */
}
#endif /* MODULE */

static void qeth_unregister_dbf_views(void)
{
	if (qeth_dbf_setup)
		debug_unregister(qeth_dbf_setup);
	if (qeth_dbf_qerr)
		debug_unregister(qeth_dbf_qerr);
	if (qeth_dbf_sense)
		debug_unregister(qeth_dbf_sense);
	if (qeth_dbf_misc)
		debug_unregister(qeth_dbf_misc);
	if (qeth_dbf_data)
		debug_unregister(qeth_dbf_data);
	if (qeth_dbf_control)
		debug_unregister(qeth_dbf_control);
	if (qeth_dbf_trace)
		debug_unregister(qeth_dbf_trace);
}

static int qeth_chandev_shutdown(struct net_device *dev)
{

	qeth_card_t* card;

	card = (qeth_card_t *)dev->priv;

	my_spin_lock(&setup_lock);

        qeth_remove_card_from_list(card);
	QETH_DBF_TEXT4(0,trace,"freecard");
        qeth_free_card(card);

	my_spin_unlock(&setup_lock);
	
	return 0;

}

#ifdef QETH_IPV6
static int qeth_ipv6_init(void)
{
	qeth_old_arp_constructor=arp_tbl.constructor;
	write_lock(&arp_tbl.lock);
	arp_tbl.constructor=qeth_arp_constructor;
	write_unlock(&arp_tbl.lock);

	/* generate the memory leak here */
	arp_direct_ops=(struct neigh_ops*)
		kmalloc(sizeof(struct neigh_ops),GFP_KERNEL);
	if (!arp_direct_ops)
		return -ENOMEM;

	memcpy(arp_direct_ops,&arp_direct_ops_template,
	       sizeof(struct neigh_ops));
	return 0;
}

static void qeth_ipv6_uninit(void)
{
	write_lock(&arp_tbl.lock);
	arp_tbl.constructor=qeth_old_arp_constructor;
	write_unlock(&arp_tbl.lock);
}
#endif /* QETH_IPV6 */

static void qeth_get_internal_functions(void)
{
	struct net_device dev;
	ether_setup(&dev);
	qeth_my_eth_header=dev.hard_header;
	qeth_my_eth_rebuild_header=dev.rebuild_header;
	qeth_my_eth_header_cache=dev.hard_header_cache;
	qeth_my_eth_header_cache_update=dev.header_cache_update;
	qeth_my_eth_header=dev.hard_header;
#ifdef CONFIG_TR
	tr_setup(&dev);
	qeth_my_tr_header=dev.hard_header;
	qeth_my_tr_rebuild_header=dev.rebuild_header;
#endif /* CONFIG_TR */
}

#ifdef MODULE
int init_module(void)
#else /* MODULE */
static int __init qeth_init(void)
#endif /* MODULE */
{
	int result;
#ifdef MODULE
	void *ptr;
#endif /* MODULE */

	int unregister_from_chandev=0;
	int cards_found;

	qeth_eyecatcher();

  	printk("qeth: loading %s\n",version);

#ifdef MODULE
	global_stay_in_mem = chandev_persist(chandev_type_qeth);
#endif /* MODULE */

        spin_lock_init(&setup_lock);

	spin_lock_init(&ipato_list_lock);

	qeth_get_internal_functions();

	qeth_alloc_spare_bufs();

#ifdef QETH_IPV6
	if (qeth_ipv6_init()) goto oom;
#endif /* QETH_IPV6 */

        qeth_dbf_setup=debug_register(QETH_DBF_SETUP_NAME,
                                      QETH_DBF_SETUP_INDEX,
                                      QETH_DBF_SETUP_NR_AREAS,
                                      QETH_DBF_SETUP_LEN);
        if (!qeth_dbf_setup) goto oom;

        debug_register_view(qeth_dbf_setup,&debug_hex_ascii_view);
	debug_set_level(qeth_dbf_setup,QETH_DBF_SETUP_LEVEL);

        qeth_dbf_misc=debug_register(QETH_DBF_MISC_NAME,
				     QETH_DBF_MISC_INDEX,
				     QETH_DBF_MISC_NR_AREAS,
				     QETH_DBF_MISC_LEN);
        if (!qeth_dbf_misc) goto oom;

        debug_register_view(qeth_dbf_misc,&debug_hex_ascii_view);
	debug_set_level(qeth_dbf_misc,QETH_DBF_MISC_LEVEL);

        qeth_dbf_data=debug_register(QETH_DBF_DATA_NAME,
				     QETH_DBF_DATA_INDEX,
				     QETH_DBF_DATA_NR_AREAS,
				     QETH_DBF_DATA_LEN);
        if (!qeth_dbf_data) goto oom;

        debug_register_view(qeth_dbf_data,&debug_hex_ascii_view);
	debug_set_level(qeth_dbf_data,QETH_DBF_DATA_LEVEL);

        qeth_dbf_control=debug_register(QETH_DBF_CONTROL_NAME,
					QETH_DBF_CONTROL_INDEX,
					QETH_DBF_CONTROL_NR_AREAS,
					QETH_DBF_CONTROL_LEN);
        if (!qeth_dbf_control) goto oom;

        debug_register_view(qeth_dbf_control,&debug_hex_ascii_view);
	debug_set_level(qeth_dbf_control,QETH_DBF_CONTROL_LEVEL);

        qeth_dbf_sense=debug_register(QETH_DBF_SENSE_NAME,
                                      QETH_DBF_SENSE_INDEX,
                                      QETH_DBF_SENSE_NR_AREAS,
                                      QETH_DBF_SENSE_LEN);
        if (!qeth_dbf_sense) goto oom;

        debug_register_view(qeth_dbf_sense,&debug_hex_ascii_view);
	debug_set_level(qeth_dbf_sense,QETH_DBF_SENSE_LEVEL);

        qeth_dbf_qerr=debug_register(QETH_DBF_QERR_NAME,
       				     QETH_DBF_QERR_INDEX,
      				     QETH_DBF_QERR_NR_AREAS,
     				     QETH_DBF_QERR_LEN);
        if (!qeth_dbf_qerr) goto oom;

        debug_register_view(qeth_dbf_qerr,&debug_hex_ascii_view);
	debug_set_level(qeth_dbf_qerr,QETH_DBF_QERR_LEVEL);

        qeth_dbf_trace=debug_register(QETH_DBF_TRACE_NAME,
                                      QETH_DBF_TRACE_INDEX,
                                      QETH_DBF_TRACE_NR_AREAS,
                                      QETH_DBF_TRACE_LEN);
        if (!qeth_dbf_trace) goto oom;

        debug_register_view(qeth_dbf_trace,&debug_hex_ascii_view);
	debug_set_level(qeth_dbf_trace,QETH_DBF_TRACE_LEVEL);
	
	cards_found = chandev_register_and_probe
		(qeth_probe,(chandev_shutdownfunc)qeth_chandev_shutdown,
		 (chandev_msck_notification_func)qeth_chandev_msck_notfunc,
		 chandev_type_qeth);
	if (cards_found>0)
		result=0;
	else if (cards_found<0) {
		result=cards_found;
		global_stay_in_mem=0;
	} else result=-ENODEV;

	unregister_from_chandev=(cards_found>=0);
	if ((result)&&(global_stay_in_mem)) {
		result=0;
        }

#ifdef MODULE
	QETH_DBF_TEXT0(0,setup,"initmodl");
	ptr=&init_module;
	QETH_DBF_HEX0(0,setup,&ptr,sizeof(void*));
#endif /* MODULE */

	if (!result) {
                qeth_register_notifiers();
		qeth_add_procfs_entries();
	} else {
		/* don't call it with shutdown -- there was not device
		 * initialized or an internal problem in chandev, better
		 * have him not try to call us */
		if (unregister_from_chandev)
			chandev_unregister(qeth_probe,0);
		qeth_unregister_dbf_views();
#ifdef QETH_IPV6
		qeth_ipv6_uninit();
#endif /* QETH_IPV6 */
		qeth_free_all_spare_bufs();
	}

	return result;
oom:
	PRINT_ERR("not enough memory for dbf. Will not load module.\n");
	result=-ENOMEM;
#ifdef QETH_IPV6
	qeth_ipv6_uninit();
#endif /* QETH_IPV6 */
	qeth_unregister_dbf_views();
	qeth_free_all_spare_bufs();
	return result;
}

#ifdef MODULE
void cleanup_module(void)
{
#ifdef QETH_IPV6
	qeth_ipv6_uninit();
#endif /* QETH_IPV6 */
        qeth_unregister_notifiers();

	qeth_remove_procfs_entries();

	QETH_DBF_TEXT1(0,trace,"cleanup.");

	chandev_unregister(qeth_probe, 1 );

	qeth_free_all_spare_bufs();

	qeth_unregister_dbf_views();

	printk("qeth: %s: module removed\n",version);
}
EXPORT_SYMBOL(qeth_eyecatcher); /* yeah, i know, could be outside of
				   the ifdef */
#else /* MODULE */
__initcall(qeth_init);
#endif /* MODULE */
