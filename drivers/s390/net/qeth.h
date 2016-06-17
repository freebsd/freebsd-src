/*
 * linux/drivers/s390/net/qeth.h
 *
 * Linux on zSeries OSA Express and HiperSockets support
 *
 * Copyright 2000,2003 IBM Corporation
 * Author(s): Utz Bacher <utz.bacher@de.ibm.com>
 *
 */

#ifndef __QETH_H__
#define __QETH_H__

#include <asm/qdio.h>

#define QETH_NAME " qeth"

#define VERSION_QETH_H "$Revision: 1.113 $"

/******************** CONFIG STUFF ***********************/
//#define QETH_DBF_LIKE_HELL

#ifdef CONFIG_QETH_IPV6
#define QETH_IPV6
#define QETH_VERSION_IPV6 ":IPv6"
#else
#define QETH_VERSION_IPV6 ""
#endif /* CONFIG_QETH_IPV6 */

#ifdef CONFIG_QETH_VLAN
#define QETH_VLAN
#define QETH_VERSION_VLAN ":VLAN"
#else
#define QETH_VERSION_VLAN ""
#endif /* CONFIG_QETH_VLAN */

/* these values match CHECKSUM_* in include/linux/skbuff.h */
#define SW_CHECKSUMMING 0
#define HW_CHECKSUMMING 1
#define NO_CHECKSUMMING 2

#define QETH_CHECKSUM_DEFAULT NO_CHECKSUMMING

#define QETH_DEFAULT_QUEUE 2

/******************** CONFIG STUFF END ***********************/
/********************* TUNING STUFF **************************/
#define HIGH_WATERMARK_PACK		5
#define LOW_WATERMARK_PACK		2
#define WATERMARK_FUZZ			2

#define QETH_MAX_INPUT_THRESHOLD 500
#define QETH_MAX_OUTPUT_THRESHOLD 300 /* ? */

/* only the MAX values are used */
#define QETH_MIN_INPUT_THRESHOLD 1
#define QETH_MIN_OUTPUT_THRESHOLD 1

#define QETH_REQUEUE_THRESHOLD (card->options.inbound_buffer_count/4)

#ifdef CONFIG_QETH_PERF_STATS
#define QETH_PERFORMANCE_STATS
#endif /* CONFIG_QETH_PERF_STATS */

#define QETH_VERBOSE_LEVEL 7

#define PCI_THRESHOLD_A (card->options.inbound_buffer_count+1) /* buffers we have to be behind
			     before we get a PCI */
#define PCI_THRESHOLD_B 0 /* enqueued free buffers left before we get a PCI */
#define PCI_TIMER_VALUE 3 /* not used, unless the microcode gets patched */

#define DEFAULT_SPARE_BUFFERS 0
#define MAX_SPARE_BUFFERS 1024
#define SPAREBUF_MASK 65536
#define MAX_PORTNO 15

#define QETH_PROCFILE_NAME "qeth"
#define QETH_PERF_PROCFILE_NAME "qeth_perf"
#define QETH_IPA_PROCFILE_NAME "qeth_ipa_takeover"

#define SEND_RETRIES_ALLOWED 5
#define QETH_ROUTING_ATTEMPTS 2

#define QETH_HARDSETUP_LAPS 5
#define QETH_HARDSETUP_CLEAR_LAPS 3
#define QETH_RECOVERY_HARDSETUP_RETRY 2

/************************* DEBUG FACILITY STUFF *********************/

#define QETH_DBF_HEX(ex,name,level,addr,len) \
        do { \
        if (ex) \
                debug_exception(qeth_dbf_##name,level,(void*)addr,len); \
        else \
                debug_event(qeth_dbf_##name,level,(void*)addr,len); \
        } while (0)
#define QETH_DBF_TEXT(ex,name,level,text) \
        do { \
        if (ex) \
                debug_text_exception(qeth_dbf_##name,level,text); \
        else \
                debug_text_event(qeth_dbf_##name,level,text); \
        } while (0)

#define QETH_DBF_HEX0(ex,name,addr,len) QETH_DBF_HEX(ex,name,0,addr,len)
#define QETH_DBF_HEX1(ex,name,addr,len) QETH_DBF_HEX(ex,name,1,addr,len)
#define QETH_DBF_HEX2(ex,name,addr,len) QETH_DBF_HEX(ex,name,2,addr,len)
#define QETH_DBF_HEX3(ex,name,addr,len) QETH_DBF_HEX(ex,name,3,addr,len)
#define QETH_DBF_HEX4(ex,name,addr,len) QETH_DBF_HEX(ex,name,4,addr,len)
#define QETH_DBF_HEX5(ex,name,addr,len) QETH_DBF_HEX(ex,name,5,addr,len)
#define QETH_DBF_HEX6(ex,name,addr,len) QETH_DBF_HEX(ex,name,6,addr,len)
#ifdef QETH_DBF_LIKE_HELL
#endif /* QETH_DBF_LIKE_HELL */

#define QETH_DBF_TEXT0(ex,name,text) QETH_DBF_TEXT(ex,name,0,text)
#define QETH_DBF_TEXT1(ex,name,text) QETH_DBF_TEXT(ex,name,1,text)
#define QETH_DBF_TEXT2(ex,name,text) QETH_DBF_TEXT(ex,name,2,text)
#define QETH_DBF_TEXT3(ex,name,text) QETH_DBF_TEXT(ex,name,3,text)
#define QETH_DBF_TEXT4(ex,name,text) QETH_DBF_TEXT(ex,name,4,text)
#define QETH_DBF_TEXT5(ex,name,text) QETH_DBF_TEXT(ex,name,5,text)
#define QETH_DBF_TEXT6(ex,name,text) QETH_DBF_TEXT(ex,name,6,text)
#ifdef QETH_DBF_LIKE_HELL
#endif /* QETH_DBF_LIKE_HELL */

#define QETH_DBF_SETUP_NAME "qeth_setup"
#define QETH_DBF_SETUP_LEN 8
#define QETH_DBF_SETUP_INDEX 3
#define QETH_DBF_SETUP_NR_AREAS 1
#ifdef QETH_DBF_LIKE_HELL
#define QETH_DBF_SETUP_LEVEL 6
#else /* QETH_DBF_LIKE_HELL */
#define QETH_DBF_SETUP_LEVEL 3
#endif /* QETH_DBF_LIKE_HELL */

#define QETH_DBF_MISC_NAME "qeth_misc"
#define QETH_DBF_MISC_LEN 128
#define QETH_DBF_MISC_INDEX 1
#define QETH_DBF_MISC_NR_AREAS 1
#ifdef QETH_DBF_LIKE_HELL
#define QETH_DBF_MISC_LEVEL 6
#else /* QETH_DBF_LIKE_HELL */
#define QETH_DBF_MISC_LEVEL 2
#endif /* QETH_DBF_LIKE_HELL */

#define QETH_DBF_DATA_NAME "qeth_data"
#define QETH_DBF_DATA_LEN 96
#define QETH_DBF_DATA_INDEX 3
#define QETH_DBF_DATA_NR_AREAS 1
#ifdef QETH_DBF_LIKE_HELL
#define QETH_DBF_DATA_LEVEL 6
#else /* QETH_DBF_LIKE_HELL */
#define QETH_DBF_DATA_LEVEL 2
#endif /* QETH_DBF_LIKE_HELL */

#define QETH_DBF_CONTROL_NAME "qeth_control"
/* buffers are 255 bytes long, but no prob */
#define QETH_DBF_CONTROL_LEN 256
#define QETH_DBF_CONTROL_INDEX 3
#define QETH_DBF_CONTROL_NR_AREAS 2
#ifdef QETH_DBF_LIKE_HELL
#define QETH_DBF_CONTROL_LEVEL 6
#else /* QETH_DBF_LIKE_HELL */
#define QETH_DBF_CONTROL_LEVEL 2
#endif /* QETH_DBF_LIKE_HELL */

#define QETH_DBF_TRACE_NAME "qeth_trace"
#define QETH_DBF_TRACE_LEN 8
#ifdef QETH_DBF_LIKE_HELL
#define QETH_DBF_TRACE_INDEX 3
#else /* QETH_DBF_LIKE_HELL */
#define QETH_DBF_TRACE_INDEX 2
#endif /* QETH_DBF_LIKE_HELL */
#define QETH_DBF_TRACE_NR_AREAS 2
#ifdef QETH_DBF_LIKE_HELL
#define QETH_DBF_TRACE_LEVEL 6
#else /* QETH_DBF_LIKE_HELL */
#define QETH_DBF_TRACE_LEVEL 2
#endif /* QETH_DBF_LIKE_HELL */

#define QETH_DBF_SENSE_NAME "qeth_sense"
#define QETH_DBF_SENSE_LEN 64
#define QETH_DBF_SENSE_INDEX 1
#define QETH_DBF_SENSE_NR_AREAS 1
#ifdef QETH_DBF_LIKE_HELL
#define QETH_DBF_SENSE_LEVEL 6
#else /* QETH_DBF_LIKE_HELL */
#define QETH_DBF_SENSE_LEVEL 2
#endif /* QETH_DBF_LIKE_HELL */


#define QETH_DBF_QERR_NAME "qeth_qerr"
#define QETH_DBF_QERR_LEN 8
#define QETH_DBF_QERR_INDEX 1
#define QETH_DBF_QERR_NR_AREAS 2
#ifdef QETH_DBF_LIKE_HELL
#define QETH_DBF_QERR_LEVEL 6
#else /* QETH_DBF_LIKE_HELL */
#define QETH_DBF_QERR_LEVEL 2
#endif /* QETH_DBF_LIKE_HELL */
/****************** END OF DEBUG FACILITY STUFF *********************/

/********************* CARD DATA STUFF **************************/

#define QETH_MAX_PARAMS 150

#define QETH_CARD_TYPE_UNKNOWN	0
#define QETH_CARD_TYPE_OSAE	10
#define QETH_CARD_TYPE_IQD	1234

#define QETH_IDX_FUNC_LEVEL_OSAE_ENA_IPAT 0x0101
#define QETH_IDX_FUNC_LEVEL_OSAE_DIS_IPAT 0x0101
/* as soon as steve is ready:
#define QETH_IDX_FUNC_LEVEL_OSAE_ENA_IPAT 0x4101
#define QETH_IDX_FUNC_LEVEL_OSAE_DIS_IPAT 0x5101
*/
#define QETH_IDX_FUNC_LEVEL_IQD_ENA_IPAT 0x4108
#define QETH_IDX_FUNC_LEVEL_IQD_DIS_IPAT 0x5108

#define QETH_MAX_QUEUES 4

#define UNIQUE_ID_IF_CREATE_ADDR_FAILED 0xfffe
#define UNIQUE_ID_NOT_BY_CARD 0x10000

/* CU type & model, Dev type & model, card_type, odd_even_restriction, func level, no of queues, multicast is different (multicast-queue_no + 0x100) */
#define QETH_MODELLIST_ARRAY \
	{{0x1731,0x01,0x1732,0x01,QETH_CARD_TYPE_OSAE,1, \
	  QETH_IDX_FUNC_LEVEL_OSAE_ENA_IPAT, \
	  QETH_IDX_FUNC_LEVEL_OSAE_DIS_IPAT, \
	  QETH_MAX_QUEUES,0}, \
	 {0x1731,0x05,0x1732,0x05,QETH_CARD_TYPE_IQD,0, \
	  QETH_IDX_FUNC_LEVEL_IQD_ENA_IPAT, \
	  QETH_IDX_FUNC_LEVEL_IQD_DIS_IPAT, \
	  QETH_MAX_QUEUES,0x103}, \
	 {0,0,0,0,0,0,0,0,0}}

#define QETH_MPC_DIFINFO_LEN_INDICATES_LINK_TYPE 0x18
 /* only the first two bytes are looked at in qeth_get_cardname_short */
#define QETH_MPC_LINK_TYPE_FAST_ETHERNET 0x01
#define QETH_MPC_LINK_TYPE_HSTR 0x02
#define QETH_MPC_LINK_TYPE_GIGABIT_ETHERNET 0x03
#define QETH_MPC_LINK_TYPE_LANE_ETH100 0x81
#define QETH_MPC_LINK_TYPE_LANE_TR 0x82
#define QETH_MPC_LINK_TYPE_LANE_ETH1000 0x83
#define QETH_MPC_LINK_TYPE_LANE 0x88

#define DEFAULT_ADD_HHLEN 0
#define MAX_ADD_HHLEN 1024

#define QETH_HEADER_SIZE	32
#define QETH_IP_HEADER_SIZE	40
#define QETH_HEADER_LEN_POS	8
/* flags for the header: */
#define QETH_HEADER_PASSTHRU	0x10
#define QETH_HEADER_IPV6	0x80

#define QETH_CAST_FLAGS		0x07
#define QETH_CAST_UNICAST	6
#define QETH_CAST_MULTICAST	4
#define QETH_CAST_BROADCAST	5
#define QETH_CAST_ANYCAST	7
#define QETH_CAST_NOCAST	0

/* VLAN defines */
#define QETH_EXT_HEADER_VLAN_FRAME	  0x01
#define QETH_EXT_HEADER_TOKEN_ID	  0x02
#define QETH_EXT_HEADER_INCLUDE_VLAN_TAG  0x04

#define QETH_EXT_HEADER_SRC_MAC_ADDRESS   0x08
#define QETH_EXT_HEADER_CSUM_HDR_REQ      0x10
#define QETH_EXT_HEADER_CSUM_TRANSP_REQ   0x20
#define QETH_EXT_HEADER_CSUM_TRANSP_FRAME_TYPE   0x40

#define QETH_UDP_CSUM_OFFSET	6
#define QETH_TCP_CSUM_OFFSET	16

#define QETH_VERIFY_IS_REAL_DEV               1
#define QETH_VERIFY_IS_VLAN_DEV               2

inline static unsigned int qeth_get_ipa_timeout(int cardtype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_IQD: return 2000;
	default: return 20000;
	}
}

inline static unsigned short qeth_get_additional_dev_flags(int cardtype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_IQD: return IFF_NOARP;
#ifdef QETH_IPV6
	default: return 0;
#else /* QETH_IPV6 */
	default: return IFF_NOARP;
#endif /* QETH_IPV6 */
	}
}

inline static int qeth_get_hlen(__u8 link_type)
{
#ifdef QETH_IPV6
	switch (link_type) {
	case QETH_MPC_LINK_TYPE_HSTR:
	case QETH_MPC_LINK_TYPE_LANE_TR:
		return QETH_HEADER_SIZE+TR_HLEN;
	default:
#ifdef QETH_VLAN
		return QETH_HEADER_SIZE+VLAN_ETH_HLEN;
#else
		return QETH_HEADER_SIZE+ETH_HLEN;
#endif
	}
#else /* QETH_IPV6 */
#ifdef QETH_VLAN
	return QETH_HEADER_SIZE+VLAN_HLEN;
#else
	return QETH_HEADER_SIZE;
#endif

#endif /* QETH_IPV6 */
}

int (*qeth_my_eth_header)(struct sk_buff *,struct net_device *,
	unsigned short,void *,void *,unsigned);
int (*qeth_my_tr_header)(struct sk_buff *,struct net_device *,
	unsigned short,void *,void *,unsigned);
int (*qeth_my_eth_rebuild_header)(struct sk_buff *);
int (*qeth_my_tr_rebuild_header)(struct sk_buff *);
int (*qeth_my_eth_header_cache)(struct neighbour *,struct hh_cache *);
void (*qeth_my_eth_header_cache_update)(struct hh_cache *,struct net_device *,
	 unsigned char *);

#ifdef QETH_IPV6
typedef int (*__qeth_temp1)(struct sk_buff *,struct net_device *,
	unsigned short,void *,void *,unsigned);
inline static __qeth_temp1 qeth_get_hard_header(__u8 link_type)
{
	switch (link_type) {
#ifdef CONFIG_TR
	case QETH_MPC_LINK_TYPE_HSTR:
	case QETH_MPC_LINK_TYPE_LANE_TR:
		return qeth_my_tr_header;
#endif /* CONFIG_TR */
	default:
		return qeth_my_eth_header;
	}
}

typedef int (*__qeth_temp2)(struct sk_buff *);
inline static __qeth_temp2 qeth_get_rebuild_header(__u8 link_type)
{
	switch (link_type) {
#ifdef CONFIG_TR
	case QETH_MPC_LINK_TYPE_HSTR:
	case QETH_MPC_LINK_TYPE_LANE_TR:
		return qeth_my_tr_rebuild_header;
#endif /* CONFIG_TR */
	default:
		return qeth_my_eth_rebuild_header;
	}
}

typedef int (*__qeth_temp3)(struct neighbour *,struct hh_cache *);
inline static __qeth_temp3 qeth_get_hard_header_cache(__u8 link_type)
{
	switch (link_type) {
	case QETH_MPC_LINK_TYPE_HSTR:
	case QETH_MPC_LINK_TYPE_LANE_TR:
		return NULL;
	default:
		return qeth_my_eth_header_cache;
	}
}

typedef void (*__qeth_temp4)(struct hh_cache *,struct net_device *,
	 unsigned char *);
inline static __qeth_temp4 qeth_get_header_cache_update(__u8 link_type)
{
	switch (link_type) {
	case QETH_MPC_LINK_TYPE_HSTR:
	case QETH_MPC_LINK_TYPE_LANE_TR:
		return NULL;
	default:
		return qeth_my_eth_header_cache_update;
	}
}

static unsigned short qeth_eth_type_trans(struct sk_buff *skb,
					  struct net_device *dev)
{
	struct ethhdr *eth;
	
	skb->mac.raw=skb->data;
	skb_pull(skb,ETH_ALEN*2+sizeof(short));
	eth=skb->mac.ethernet;
	
	if(*eth->h_dest&1) {
		if(memcmp(eth->h_dest,dev->broadcast, ETH_ALEN)==0)
			skb->pkt_type=PACKET_BROADCAST;
		else
			skb->pkt_type=PACKET_MULTICAST;
   	} else {
		skb->pkt_type=PACKET_OTHERHOST;
	}
	if (ntohs(eth->h_proto)>=1536) return eth->h_proto;
	if (*(unsigned short *)(skb->data) == 0xFFFF)
		return htons(ETH_P_802_3);
	return htons(ETH_P_802_2);
}

typedef unsigned short (*__qeth_temp5)(struct sk_buff *,struct net_device *);
inline static __qeth_temp5 qeth_get_type_trans(__u8 link_type)
{
	switch (link_type) {
	case QETH_MPC_LINK_TYPE_HSTR:
	case QETH_MPC_LINK_TYPE_LANE_TR:
		return tr_type_trans;
	default:
		return qeth_eth_type_trans;
	}
}
#endif /* QETH_IPV6 */

inline static const char *qeth_get_link_type_name(int cardtype,__u8 linktype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_UNKNOWN: return "unknown";
	case QETH_CARD_TYPE_OSAE:
		switch (linktype) {
		case QETH_MPC_LINK_TYPE_FAST_ETHERNET: return "Fast Eth";
		case QETH_MPC_LINK_TYPE_HSTR: return "HSTR";
		case QETH_MPC_LINK_TYPE_GIGABIT_ETHERNET: return "Gigabit Eth";
		case QETH_MPC_LINK_TYPE_LANE_ETH100: return "LANE Eth100";
		case QETH_MPC_LINK_TYPE_LANE_TR: return "LANE TR";
		case QETH_MPC_LINK_TYPE_LANE_ETH1000: return "LANE Eth1000";
		default: return "unknown";
		}
	case QETH_CARD_TYPE_IQD: return "magic";
	default: return "unknown";
	}
}

inline static const char* qeth_get_dev_basename(int cardtype,__u8 link_type)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_UNKNOWN: return "eth";
	case QETH_CARD_TYPE_OSAE: switch (link_type) {
				  case QETH_MPC_LINK_TYPE_LANE_TR:
					  /* fallthrough */
				  case QETH_MPC_LINK_TYPE_HSTR: return "tr";
				  default: return "eth";
				  }
	case QETH_CARD_TYPE_IQD: return "hsi";
	default: return "eth";
	}
}

/* inbound: */
#define DEFAULT_BUFFER_SIZE 65536
#define DEFAULT_BUFFER_COUNT 128
#define BUFCNT_MIN 8
#define BUFCNT_MAX 128
#define BUFFER_SIZE (card->inbound_buffer_size)
#define BUFFER_MAX_ELEMENTS (BUFFER_SIZE>>12)
	/* 8k for each pair header-buffer: */

inline static int qeth_sbal_packing_on_card(int cardtype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_IQD: return 0;
	default: return 1;
	}
}

/* do it this way round -> __MODULE_STRING needs with */
/* QETH_PRIO_NICE_LEVELS a single number */
#define QETH_MAX_PRIO_QUEUES QETH_PRIO_NICE_LEVELS+1

static inline int qeth_sbalf15_in_retrieable_range(int sbalf15)
{
	return ( (sbalf15>=15) && (sbalf15<=31) );
}

#define INBOUND_BUFFER_POS(card,bufno,sbale) \
	( (bufno&SPAREBUF_MASK)? \
	  ( \
	    (sparebufs[bufno&(~SPAREBUF_MASK)].buf+ \
	     PAGE_SIZE*sbale) \
	  ):( \
	      (card->inbound_buffer_pool_entry[card-> \
	       inbound_buffer_entry_no[bufno]][sbale]) \
	    ) )

#define SPAREBUF_UNAVAIL 0
#define SPAREBUF_FREE 1
#define SPAREBUF_USED 2

typedef struct sparebufs_t {
	char *buf;
	atomic_t status;
} sparebufs_t;

#define SEND_STATE_INACTIVE		0
#define SEND_STATE_DONT_PACK		1
#define SEND_STATE_PACK			2

#define QETH_LOCK_UNLOCKED 0
#define QETH_LOCK_NORMAL 1
#define QETH_LOCK_FLUSH 2

#define QETH_MAX_DEVICES 16
	/* DEPENDENCY ON QETH_MAX_DEVICES.
	 *__MOUDLE_STRING expects simple literals */
#define QETH_MAX_DEVICES_TIMES_4 64
#define QETH_MAX_DEVNAMES 16
#define QETH_DEVNAME "eth"

#define QETH_TX_TIMEOUT 100*HZ /* 100 seconds */

#define QETH_REMOVE_WAIT_TIME 200
#define QETH_WAIT_FOR_THREAD_TIME 20
#define QETH_IDLE_WAIT_TIME 10
#define QETH_WAIT_BEFORE_2ND_DOIO 1000

#define QETH_MAX_PARM_LEN 128

#define QETH_FAKE_LL_LEN ETH_HLEN /* 14 */
#define QETH_FAKE_LL_PROT_LEN 2
#define QETH_FAKE_LL_ADDR_LEN ETH_ALEN /* 6 */
#define QETH_FAKE_LL_DEST_MAC_POS 0
#define QETH_FAKE_LL_SRC_MAC_POS 6
#define QETH_FAKE_LL_SRC_MAC_POS_IN_QDIO_HDR 6
#define QETH_FAKE_LL_PROT_POS 12 
#define QETH_FAKE_LL_V4_ADDR_POS 16
#define QETH_FAKE_LL_V6_ADDR_POS 24

#define DEV_NAME_LEN 16
#define IOCTL_MAX_TRANSFER_SIZE 65535

#define IP_TOS_LOWDELAY 0x10
#define IP_TOS_HIGHTHROUGHPUT 0x08
#define IP_TOS_HIGHRELIABILITY 0x04
#define IP_TOS_NOTIMPORTANT 0x02

#define QETH_RCD_LENGTH 128

#define __max(a,b) ( ((a)>(b))?(a):(b) )
#define __min(a,b) ( ((a)<(b))?(a):(b) )
#define QETH_BUFSIZE __max(__max(IPA_PDU_HEADER_SIZE+ \
				 sizeof(arp_cmd_t),IPA_PDU_HEADER_SIZE+ \
				 sizeof(ipa_cmd_t)),QETH_RCD_LENGTH)

#define QETH_FINAL_STATUS_TIMEOUT 1500
#define QETH_CLEAR_TIMEOUT 1500
#define QETH_RCD_TIMEOUT 1500
#define QETH_NOP_TIMEOUT 1500
#define QETH_QUIESCE_NETDEV_TIME 300
#define QETH_QUIESCE_WAIT_BEFORE_CLEAR 4000
#define QETH_QUIESCE_WAIT_AFTER_CLEAR 4000

#define NOP_STATE 0x1001
#define READ_CONF_DATA_STATE 0x1002
#define IDX_ACTIVATE_READ_STATE 0x1003
#define IDX_ACTIVATE_WRITE_STATE 0x1004
#define MPC_SETUP_STATE 0x1005
#define CLEAR_STATE 0x1006
#define IPA_CMD_STATE 0x1007
#define IPA_IOCTL_STATE 0x1009
#define IPA_SETIP_FLAG 0x100000

#define QETH_REMOVE_CARD_PROPER 1
#define QETH_REMOVE_CARD_QUICK 2

#define PARSE_AUTO 0
#define PARSE_ROUTING_TYPE 1
#define PARSE_CHECKSUMMING 2
#define PARSE_PRIO_QUEUEING 3
#define PARSE_STAYINMEM 4
#define PARSE_BUFFERCOUNT 5
#define PARSE_PORTNAME 6
#define PARSE_POLLTIME 7
#define PARSE_SPARE_BUFFERCOUNT 8
#define PARSE_PORTNO 9
#define PARSE_BROADCAST_MODE 10
#define PARSE_MACADDR_MODE 11
#define PARSE_MEMUSAGE 12
#define PARSE_ENA_IPAT 13
#define PARSE_FAKE_BROADCAST 14
#define PARSE_ADD_HHLEN 15
#define PARSE_ROUTING_TYPE4 16
#define PARSE_ROUTING_TYPE6 17
#define PARSE_FAKE_LL 18
#define PARSE_ASYNC_IQD 19

#define PARSE_COUNT 20

#define NO_PRIO_QUEUEING 0
#define PRIO_QUEUEING_PREC 1
#define PRIO_QUEUEING_TOS 2
#define NO_ROUTER 0
#define PRIMARY_ROUTER 1
#define SECONDARY_ROUTER 2
#define MULTICAST_ROUTER 3
#define PRIMARY_CONNECTOR 4
#define SECONDARY_CONNECTOR 5
#define ROUTER_MASK 0xf /* used to remove SET_ROUTING_FLAG
			   from routing_type */
#define RESET_ROUTING_FLAG 0x10 /* used to indicate, that setting
				   the routing type is desired */
#define BROADCAST_ALLRINGS 0
#define BROADCAST_LOCAL 1
#define MACADDR_NONCANONICAL 0
#define MACADDR_CANONICAL 1
#define MEMUSAGE_DISCONTIG 0
#define MEMUSAGE_CONTIG 1
#define ENABLE_TAKEOVER 0
#define DISABLE_TAKEOVER 1
#define FAKE_BROADCAST 0
#define DONT_FAKE_BROADCAST 1
#define FAKE_LL 0
#define DONT_FAKE_LL 1
#define SYNC_IQD 0
#define ASYNC_IQD 1

#define QETH_BREAKOUT_LEAVE 1
#define QETH_BREAKOUT_AGAIN 2

#define QETH_WAIT_FOR_LOCK 0
#define QETH_DONT_WAIT_FOR_LOCK 1
#define QETH_LOCK_ALREADY_HELD 2

#define PROBLEM_CARD_HAS_STARTLANED 1
#define PROBLEM_RECEIVED_IDX_TERMINATE 2
#define PROBLEM_ACTIVATE_CHECK_CONDITION 3
#define PROBLEM_RESETTING_EVENT_INDICATOR 4
#define PROBLEM_COMMAND_REJECT 5
#define PROBLEM_ZERO_SENSE_DATA 6
#define PROBLEM_GENERAL_CHECK 7
#define PROBLEM_BAD_SIGA_RESULT 8
#define PROBLEM_USER_TRIGGERED_RECOVERY 9
#define PROBLEM_AFFE 10
#define PROBLEM_MACHINE_CHECK 11
#define PROBLEM_TX_TIMEOUT 12

#define SENSE_COMMAND_REJECT_BYTE 0
#define SENSE_COMMAND_REJECT_FLAG 0x80
#define SENSE_RESETTING_EVENT_BYTE 1
#define SENSE_RESETTING_EVENT_FLAG 0x80

#define DEFAULT_RCD_CMD 0x72
#define DEFAULT_RCD_COUNT 0x80

#define BUFFER_USED 1
#define BUFFER_UNUSED -1

/*typedef struct wait_queue* wait_queue_head_t; already typedefed in qdio.h */

typedef int (*reg_notifier_t)(struct notifier_block*);

typedef struct ipato_entry_t {
	int version;
	__u8 addr[16];
	int mask_bits;
	char dev_name[DEV_NAME_LEN];
	struct ipato_entry_t *next;
} ipato_entry_t;

typedef struct qeth_vipa_entry_t {
	int version;
	__u8 ip[16];
	int flag;
	volatile int state;
	struct qeth_vipa_entry_t *next;
} qeth_vipa_entry_t;

typedef struct ip_state_t {
	struct in_ifaddr *ip_ifa;	/* pointer to IPv4 adresses */
	struct inet6_ifaddr *ip6_ifa;
} ip_state_t;

struct qeth_ipm_mac {
	__u8 mac[ETH_ALEN];
	__u8 ip[16];
	struct qeth_ipm_mac *next;
};

typedef struct ip_mc_state_t {
	struct qeth_ipm_mac *ipm_ifa;
	struct qeth_ipm_mac *ipm6_ifa;
} ip_mc_state_t;

struct qeth_card_options {
	char devname[DEV_NAME_LEN];
	volatile int routing_type4;
#ifdef QETH_IPV6
	volatile int routing_type6;
#endif /* QETH_IPV6 */
	int checksum_type;
	int do_prio_queueing;
	int default_queue;
	int already_parsed[PARSE_COUNT];
	int inbound_buffer_count;
	__s32 memory_usage_in_k;
	int polltime;
	char portname[9];
	int portno;
	int memusage;
	int broadcast_mode;
	int macaddr_mode;
	int ena_ipat;
	int fake_broadcast;
	int add_hhlen;
	int fake_ll;
	int async_iqd;
};

typedef struct qeth_hdr_t {
	__u8 id;
	__u8 flags;
	__u16 inbound_checksum;
	__u32 token;
	__u16 length;
	__u8 vlan_prio;
	__u8 ext_flags;
	__u16 vlan_id;
	__u16 frame_offset;
	__u8 dest_addr[16];
} qeth_hdr_t;

typedef struct qeth_ringbuffer_element_t {
	struct sk_buff_head skb_list;
	int next_element_to_fill;
} __attribute__ ((packed)) qeth_ringbuffer_element_t;

typedef struct qeth_ringbuffer_t {
	qdio_buffer_t buffer[QDIO_MAX_BUFFERS_PER_Q];
	qeth_ringbuffer_element_t ringbuf_element[QDIO_MAX_BUFFERS_PER_Q];
} qeth_ringbuffer_t __attribute__ ((packed,aligned(PAGE_SIZE)));

typedef struct qeth_dma_stuff_t {
	unsigned char *sendbuf;
	unsigned char *recbuf;
	ccw1_t read_ccw;
	ccw1_t write_ccw;
} qeth_dma_stuff_t __attribute__ ((packed,aligned(PAGE_SIZE)));

typedef struct qeth_perf_stats_t {
	unsigned int skbs_rec;
	unsigned int bufs_rec;

	unsigned int skbs_sent;
	unsigned int bufs_sent;

	unsigned int skbs_sent_dont_pack;
	unsigned int bufs_sent_dont_pack;
	unsigned int skbs_sent_pack;
	unsigned int bufs_sent_pack;
	unsigned int skbs_sent_pack_better;
	unsigned int bufs_sent_pack_better;

	unsigned int sc_dp_p;
	unsigned int sc_p_dp;

	__u64 inbound_start_time;
	unsigned int inbound_cnt;
	unsigned int inbound_time;
	__u64 outbound_start_time;
	unsigned int outbound_cnt;
	unsigned int outbound_time;
} qeth_perf_stats_t;

/* ugly. I know. */
typedef struct qeth_card_t {			/* pointed to by dev->priv */
	int easy_copy_cap;

	/* pointer to options (defaults + parameters) */
	struct qeth_card_options options;

 	atomic_t is_startlaned; /* card did not get a stoplan */
				/* also 0 when card is gone after a
				   machine check */

	__u8 link_type;

	int do_pfix; /* to avoid doing diag98 for vm guest lan devices */

	/* inbound buffer management */
	atomic_t inbound_buffer_refcnt[QDIO_MAX_BUFFERS_PER_Q];
	qdio_buffer_t inbound_qdio_buffers[QDIO_MAX_BUFFERS_PER_Q];
	void *real_inb_buffer_addr[QDIO_MAX_BUFFERS_PER_Q]
		[QDIO_MAX_ELEMENTS_PER_BUFFER];

	/* inbound data area */
	void *inbound_buffer_pool_entry[QDIO_MAX_BUFFERS_PER_Q]
		[QDIO_MAX_ELEMENTS_PER_BUFFER];
	volatile int inbound_buffer_pool_entry_used[QDIO_MAX_BUFFERS_PER_Q];
	int inbound_buffer_entry_no[QDIO_MAX_BUFFERS_PER_Q];

	/* for requeueing of buffers */
	spinlock_t requeue_input_lock;
	atomic_t requeue_position;
	atomic_t requeue_counter;

	/* outbound QDIO stuff */
	volatile int send_state[QETH_MAX_QUEUES];
	volatile int outbound_first_free_buffer[QETH_MAX_QUEUES];
	atomic_t outbound_used_buffers[QETH_MAX_QUEUES];
	int outbound_buffer_send_state[QETH_MAX_QUEUES]
		[QDIO_MAX_BUFFERS_PER_Q];
	int send_retries[QETH_MAX_QUEUES][QDIO_MAX_BUFFERS_PER_Q];
	volatile int outbound_bytes_in_buffer[QETH_MAX_QUEUES];
	qeth_ringbuffer_t *outbound_ringbuffer[QETH_MAX_QUEUES];
	atomic_t outbound_ringbuffer_lock[QETH_MAX_QUEUES];
	atomic_t last_pci_pos[QETH_MAX_QUEUES];

#ifdef QETH_IPV6
	int (*hard_header)(struct sk_buff *,struct net_device *,
			   unsigned short,void *,void *,unsigned);
	int (*rebuild_header)(struct sk_buff *);
	int (*hard_header_cache)(struct neighbour *,struct hh_cache *);
	void (*header_cache_update)(struct hh_cache *,struct net_device *,
				    unsigned char *);
	unsigned short (*type_trans)(struct sk_buff *,struct net_device *);
	int type_trans_correction;
#endif /* QETH_IPV6 */

#ifdef QETH_VLAN
        struct vlan_group *vlangrp;
        spinlock_t vlan_lock;

#endif
	char dev_name[DEV_NAME_LEN];		/* pointed to by dev->name */
	char dev_basename[DEV_NAME_LEN];
	struct net_device *dev;
	struct net_device_stats *stats;

	int no_queues;

#ifdef QETH_PERFORMANCE_STATS
	qeth_perf_stats_t perf_stats;
#endif /* QETH_PERFORMANCE_STATS */

	/* our state */
	atomic_t is_registered;		/* card registered as netdev? */
	atomic_t is_hardsetup;		/* card has gone through hardsetup */
	atomic_t is_softsetup;		/* card is setup by softsetup */
	atomic_t is_open;		/* card is in use */
	atomic_t is_gone;               /* after a msck */

	int has_irq;			/* once a request_irq was successful */

	/* prevents deadlocks :-O */
	spinlock_t softsetup_lock;
	spinlock_t hardsetup_lock;
	spinlock_t ioctl_lock;
	atomic_t softsetup_thread_is_running;
	struct semaphore softsetup_thread_sem;
	struct tq_struct tqueue_sst;

	atomic_t escape_softsetup;	/* active, when recovery has to
					   wait for softsetup */
	struct semaphore reinit_thread_sem;
	atomic_t in_recovery;
	atomic_t reinit_counter;

	/* problem management */
	atomic_t break_out;
	atomic_t problem;
	struct tq_struct tqueue;

	struct {
		__u32 trans_hdr;
		__u32 pdu_hdr;
		__u32 pdu_hdr_ack;
		__u32 ipa;
	} seqno;

	struct {
		__u32 issuer_rm_w;
		__u32 issuer_rm_r;
		__u32 cm_filter_w;
		__u32 cm_filter_r;
		__u32 cm_connection_w;
		__u32 cm_connection_r;
		__u32 ulp_filter_w;
		__u32 ulp_filter_r;
		__u32 ulp_connection_w;
		__u32 ulp_connection_r;
	} token;

	/* this is card-related */
	int type;
	__u16 func_level;
	int initial_mtu;
	int max_mtu;
	int inbound_buffer_size;

	int is_multicast_different; /* if multicast traffic is to be sent
				       on a different queue, this is the
				       queue+no_queues */
	int can_do_async_iqd; /* 1 only on IQD that provides async
		      		 unicast sigas */

	__u32 ipa_supported;
	__u32 ipa_enabled;
	__u32 ipa6_supported;
	__u32 ipa6_enabled;
	__u32 adp_supported;

	atomic_t startlan_attempts;
	atomic_t enable_routing_attempts4;
	atomic_t rt4fld;
#ifdef QETH_IPV6
	atomic_t enable_routing_attempts6;
	atomic_t rt6fld;
#endif /* QETH_IPV6 */
	int unique_id;

	/* device and I/O data */
	int devno0;
	int devno1;
	int devno2;
	int irq0;
	int irq1;
	int irq2;
	unsigned short unit_addr2;
	unsigned short cula;
	unsigned short dev_type;
	unsigned char dev_model;
	unsigned short chpid;
	devstat_t *devstat0;
	devstat_t *devstat1;
	devstat_t *devstat2;

	unsigned char ipa_buf[QETH_BUFSIZE];
	unsigned char send_buf[QETH_BUFSIZE];

/* IOCTL Stuff */
	unsigned char *ioctl_data_buffer;
	unsigned char *ioctl_buffer_pointer;
	int ioctl_returncode;
	int ioctl_buffersize;
	int number_of_entries;


	atomic_t ioctl_data_has_arrived;
	wait_queue_head_t ioctl_wait_q;
	atomic_t ioctl_wait_q_active;
	spinlock_t ioctl_wait_q_lock;

/* stuff under 2 gb */
	qeth_dma_stuff_t *dma_stuff;

	unsigned int ipa_timeout;

	atomic_t write_busy;

	int read_state; /* only modified and read in the int handler */

	/* vipa stuff */
	rwlock_t vipa_list_lock;
	qeth_vipa_entry_t *vipa_list;

	/* state information when doing I/O */
	atomic_t shutdown_phase;
	volatile int save_state_flag;
	atomic_t data_has_arrived;
	wait_queue_head_t wait_q;
	atomic_t wait_q_active;
	spinlock_t wait_q_lock; /* for wait_q_active and wait_q */

	atomic_t final_status0;
	atomic_t final_status1;
	atomic_t final_status2;
	atomic_t clear_succeeded0;
	atomic_t clear_succeeded1;
	atomic_t clear_succeeded2;

	/* bookkeeping of IP and multicast addresses */
	ip_state_t ip_current_state;
	ip_state_t ip_new_state;
	
#ifdef CONFIG_IP_MULTICAST
	ip_mc_state_t ip_mc_current_state;
	ip_mc_state_t ip_mc_new_state;
#endif /* CONFIG_IF_MULTICAST */

	int broadcast_capable;
	int portname_required;

	int realloc_message;

	char level[QETH_MCL_LENGTH+1];

	volatile int saved_dev_flags;

	/* for our linked list */
	struct qeth_card_t *next;
} qeth_card_t;

typedef struct mydevreg_t {
	devreg_t devreg;
	struct mydevreg_t *next;
	struct mydevreg_t *prev;
} mydevreg_t;

inline static int qeth_get_arphrd_type(int cardtype,int linktype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_OSAE: switch (linktype) {
				  case QETH_MPC_LINK_TYPE_LANE_TR:
					  /* fallthrough */
				  case QETH_MPC_LINK_TYPE_HSTR:
					  return ARPHRD_IEEE802;
				  default: return ARPHRD_ETHER;
				  }
	case QETH_CARD_TYPE_IQD: return ARPHRD_ETHER;
	default: return ARPHRD_ETHER;
	}
}

inline static int qeth_determine_easy_copy_cap(int cardtype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_UNKNOWN: return 0; /* better be cautious */
	case QETH_CARD_TYPE_OSAE: return 1;
	case QETH_CARD_TYPE_IQD: return 0;
	default: return 0; /* ?? */
	}
}
inline static __u8 qeth_get_adapter_type_for_ipa(int link_type)
{
	switch (link_type) {
	case QETH_MPC_LINK_TYPE_HSTR: return 2;
	default: return 1;
	}
}

inline static const char *qeth_get_cardname(int cardtype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_UNKNOWN: return "n unknown";
	case QETH_CARD_TYPE_OSAE: return "n OSD Express";
	case QETH_CARD_TYPE_IQD: return " HiperSockets";
	default: return " strange";
	}
}

/* max length to be returned: 14 */
inline static const char *qeth_get_cardname_short(int cardtype,__u8 link_type)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_UNKNOWN: return "unknown";
	case QETH_CARD_TYPE_OSAE: switch (link_type) {
			      	  case QETH_MPC_LINK_TYPE_FAST_ETHERNET:
		     			  return "OSD_100";
			      	  case QETH_MPC_LINK_TYPE_HSTR:
		     			  return "HSTR";
	     			  case QETH_MPC_LINK_TYPE_GIGABIT_ETHERNET:
     					  return "OSD_1000";
				  case QETH_MPC_LINK_TYPE_LANE_ETH100:
     					  return "OSD_FE_LANE";
				  case QETH_MPC_LINK_TYPE_LANE_TR:
     					  return "OSD_TR_LANE";
				  case QETH_MPC_LINK_TYPE_LANE_ETH1000:
     					  return "OSD_GbE_LANE";
				  case QETH_MPC_LINK_TYPE_LANE:
     					  return "OSD_ATM_LANE";
				  default: return "OSD_Express";
				  }
	case QETH_CARD_TYPE_IQD: return "HiperSockets";
	default: return " strange";
	}
}

inline static int qeth_mtu_is_valid(qeth_card_t *card,int mtu)
{
	switch (card->type) {
	case QETH_CARD_TYPE_UNKNOWN: return 1;
	case QETH_CARD_TYPE_OSAE: return ( (mtu>=576) && (mtu<=61440) );
	case QETH_CARD_TYPE_IQD: return ( (mtu>=576) &&
					    (mtu<=card->max_mtu+4096-32) );
	default: return 1;
	}
}

inline static int qeth_get_initial_mtu_for_card(qeth_card_t *card)
{
	switch (card->type) {
	case QETH_CARD_TYPE_UNKNOWN: return 1500;
	case QETH_CARD_TYPE_IQD: return card->max_mtu;
	case QETH_CARD_TYPE_OSAE:
		switch (card->link_type) {
		case QETH_MPC_LINK_TYPE_HSTR:
		case QETH_MPC_LINK_TYPE_LANE_TR:
			return 2000;
		default:
			return 1492;
		}
	default: return 1500;
	}
}

inline static int qeth_get_max_mtu_for_card(int cardtype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_UNKNOWN: return 61440;
	case QETH_CARD_TYPE_OSAE: return 61440;
	case QETH_CARD_TYPE_IQD: return 57344;
	default: return 1500;
	}
}

inline static int qeth_get_mtu_out_of_mpc(int cardtype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_IQD: return 1;
	default: return 0;
	}
}

inline static int qeth_get_mtu_outof_framesize(int framesize)
{
	switch (framesize) {
	case 0x4000: return 8192;
	case 0x6000: return 16384;
	case 0xa000: return 32768;
	case 0xffff: return 57344;
	default: return 0;
	}
}

inline static int qeth_get_buffersize_for_card(int cardtype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_UNKNOWN: return 65536;
	case QETH_CARD_TYPE_OSAE: return 65536;
	case QETH_CARD_TYPE_IQD: return 16384;
	default: return 65536;
	}
}

inline static int qeth_get_min_number_of_buffers(int cardtype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_UNKNOWN: return 32;
	case QETH_CARD_TYPE_OSAE: return 32;
	case QETH_CARD_TYPE_IQD: return 64;
	default: return 64;
	}
}

inline static int qeth_get_q_format(int cardtype)
{
	switch (cardtype) {
	case QETH_CARD_TYPE_IQD: return 2;
	default: return 0;
	}
}

inline static int qeth_get_device_tx_q_len(int cardtype)
{
	return 100;
}

inline static int qeth_get_max_number_of_buffers(int cardtype)
{
	return 127;
}

/******************** OUTPUT FACILITIES **************************/

#ifdef PRINT_INFO
#undef PRINTK_HEADER
#undef PRINT_STUPID
#undef PRINT_ALL
#undef PRINT_INFO
#undef PRINT_WARN
#undef PRINT_ERR
#undef PRINT_CRIT
#undef PRINT_ALERT
#undef PRINT_EMERG
#endif /* PRINT_INFO */

#define PRINTK_HEADER QETH_NAME ": "

#if QETH_VERBOSE_LEVEL>8
#define PRINT_STUPID(x...) printk( KERN_DEBUG PRINTK_HEADER x)
#else
#define PRINT_STUPID(x...)
#endif

#if QETH_VERBOSE_LEVEL>7
#define PRINT_ALL(x...) printk( KERN_DEBUG PRINTK_HEADER x)
#else
#define PRINT_ALL(x...)
#endif

#if QETH_VERBOSE_LEVEL>6
#define PRINT_INFO(x...) printk( KERN_INFO PRINTK_HEADER x)
#else
#define PRINT_INFO(x...)
#endif

#if QETH_VERBOSE_LEVEL>5
#define PRINT_WARN(x...) printk( KERN_WARNING PRINTK_HEADER x)
#else
#define PRINT_WARN(x...)
#endif

#if QETH_VERBOSE_LEVEL>4
#define PRINT_ERR(x...) printk( KERN_ERR PRINTK_HEADER x)
#else
#define PRINT_ERR(x...)
#endif

#if QETH_VERBOSE_LEVEL>3
#define PRINT_CRIT(x...) printk( KERN_CRIT PRINTK_HEADER x)
#else
#define PRINT_CRIT(x...)
#endif

#if QETH_VERBOSE_LEVEL>2
#define PRINT_ALERT(x...) printk( KERN_ALERT PRINTK_HEADER x)
#else
#define PRINT_ALERT(x...)
#endif

#if QETH_VERBOSE_LEVEL>1
#define PRINT_EMERG(x...) printk( KERN_EMERG PRINTK_HEADER x)
#else
#define PRINT_EMERG(x...)
#endif

#endif /* __QETH_H__ */
