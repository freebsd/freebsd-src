#ifndef __MV64340_ETH_H__
#define __MV64340_ETH_H__

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/spinlock.h>

#include <asm/mv64340.h>

#define	BIT0	0x00000001
#define	BIT1	0x00000002
#define	BIT2	0x00000004
#define	BIT3	0x00000008
#define	BIT4	0x00000010
#define	BIT5	0x00000020
#define	BIT6	0x00000040
#define	BIT7	0x00000080
#define	BIT8	0x00000100
#define	BIT9	0x00000200
#define	BIT10	0x00000400
#define	BIT11	0x00000800
#define	BIT12	0x00001000
#define	BIT13	0x00002000
#define	BIT14	0x00004000
#define	BIT15	0x00008000
#define	BIT16	0x00010000
#define	BIT17	0x00020000
#define	BIT18	0x00040000
#define	BIT19	0x00080000
#define	BIT20	0x00100000
#define	BIT21	0x00200000
#define	BIT22	0x00400000
#define	BIT23	0x00800000
#define	BIT24	0x01000000
#define	BIT25	0x02000000
#define	BIT26	0x04000000
#define	BIT27	0x08000000
#define	BIT28	0x10000000
#define	BIT29	0x20000000
#define	BIT30	0x40000000
#define	BIT31	0x80000000

/*************************************************************************
**************************************************************************
**************************************************************************
*  The first part is the high level driver of the gigE ethernet ports.   *
**************************************************************************
**************************************************************************
*************************************************************************/

#define ETH_PORT0_IRQ_NUM 48			/* main high register, bit0 */
#define ETH_PORT1_IRQ_NUM ETH_PORT0_IRQ_NUM+1	/* main high register, bit1 */
#define ETH_PORT2_IRQ_NUM ETH_PORT0_IRQ_NUM+2	/* main high register, bit1 */

/* Checksum offload for Tx works */
#define  MV64340_CHECKSUM_OFFLOAD_TX	1
#define	 MV64340_NAPI			1
#define	 MV64340_TX_FAST_REFILL		1
#undef	 MV64340_COAL

/* 
 * Number of RX / TX descriptors on RX / TX rings.
 * Note that allocating RX descriptors is done by allocating the RX
 * ring AND a preallocated RX buffers (skb's) for each descriptor.
 * The TX descriptors only allocates the TX descriptors ring,
 * with no pre allocated TX buffers (skb's are allocated by higher layers.
 */

/* Default TX ring size is 1000 descriptors */
#define MV64340_TX_QUEUE_SIZE 1000

/* Default RX ring size is 400 descriptors */
#define MV64340_RX_QUEUE_SIZE 400

#define MV64340_TX_COAL 100
#ifdef MV64340_COAL
#define MV64340_RX_COAL 100
#endif

/* Private data structure used for ethernet device */
struct mv64340_eth_priv {
	unsigned int port_num;
	struct net_device_stats stats;
	spinlock_t lock;
	/* Size of Tx Ring per queue */
	unsigned int tx_ring_size;
	/* Ammont of SKBs outstanding on Tx queue */
	unsigned int tx_ring_skbs;
	/* Size of Rx Ring per queue */
	unsigned int rx_ring_size;
	/* Ammount of SKBs allocated to Rx Ring per queue */
	unsigned int rx_ring_skbs;

	/*
	 * rx_task used to fill RX ring out of bottom half context 
	 */
	struct tq_struct rx_task;

	/* 
	 * Used in case RX Ring is empty, which can be caused when 
	 * system does not have resources (skb's) 
	 */
	struct timer_list timeout;
	long rx_task_busy __attribute__ ((aligned(SMP_CACHE_BYTES)));
	unsigned rx_timer_flag;

	u32 rx_int_coal;
	u32 tx_int_coal;
};


/*************************************************************************
**************************************************************************
**************************************************************************
*  The second part is the low level driver of the gigE ethernet ports.   *
**************************************************************************
**************************************************************************
*************************************************************************/


/********************************************************************************
 * Header File for : MV-643xx network interface header 
 *
 * DESCRIPTION:
 *       This header file contains macros typedefs and function declaration for
 *       the Marvell Gig Bit Ethernet Controller. 
 *
 * DEPENDENCIES:
 *       None.
 *
 *******************************************************************************/

typedef enum _bool { false, true } bool;

/* defines  */

/* Default port configuration value */
#define PORT_CONFIG_VALUE                       \
             ETH_UNICAST_NORMAL_MODE		|   \
             ETH_DEFAULT_RX_QUEUE_0		|   \
             ETH_DEFAULT_RX_ARP_QUEUE_0		|   \
             ETH_RECEIVE_BC_IF_NOT_IP_OR_ARP	|   \
             ETH_RECEIVE_BC_IF_IP		|   \
             ETH_RECEIVE_BC_IF_ARP 		|   \
             ETH_CAPTURE_TCP_FRAMES_DIS		|   \
             ETH_CAPTURE_UDP_FRAMES_DIS		|   \
             ETH_DEFAULT_RX_TCP_QUEUE_0		|   \
             ETH_DEFAULT_RX_UDP_QUEUE_0		|   \
             ETH_DEFAULT_RX_BPDU_QUEUE_0

/* Default port extend configuration value */
#define PORT_CONFIG_EXTEND_VALUE		\
             ETH_SPAN_BPDU_PACKETS_AS_NORMAL	|   \
             ETH_PARTITION_DISABLE


/* Default sdma control value */
#define PORT_SDMA_CONFIG_VALUE			\
			 ETH_RX_BURST_SIZE_16_64BIT 	|	\
			 GT_ETH_IPG_INT_RX(0) 		|	\
			 ETH_TX_BURST_SIZE_16_64BIT;

#define GT_ETH_IPG_INT_RX(value)                \
            ((value & 0x3fff) << 8)

/* Default port serial control value */
#define PORT_SERIAL_CONTROL_VALUE		\
			ETH_FORCE_LINK_PASS 			|	\
			ETH_ENABLE_AUTO_NEG_FOR_DUPLX		|	\
			ETH_DISABLE_AUTO_NEG_FOR_FLOW_CTRL 	|	\
			ETH_ADV_SYMMETRIC_FLOW_CTRL 		|	\
			ETH_FORCE_FC_MODE_NO_PAUSE_DIS_TX 	|	\
			ETH_FORCE_BP_MODE_NO_JAM 		|	\
			BIT9 					|	\
			ETH_DO_NOT_FORCE_LINK_FAIL 		|	\
			ETH_RETRANSMIT_16_ATTEMPTS 		|	\
			ETH_ENABLE_AUTO_NEG_SPEED_GMII	 	|	\
			ETH_DTE_ADV_0 				|	\
			ETH_DISABLE_AUTO_NEG_BYPASS		|	\
			ETH_AUTO_NEG_NO_CHANGE 			|	\
			ETH_MAX_RX_PACKET_9700BYTE 		|	\
			ETH_CLR_EXT_LOOPBACK 			|	\
			ETH_SET_FULL_DUPLEX_MODE 		|	\
			ETH_ENABLE_FLOW_CTRL_TX_RX_IN_FULL_DUPLEX

#define RX_BUFFER_MAX_SIZE  0x4000000
#define TX_BUFFER_MAX_SIZE  0x4000000

/* MAC accepet/reject macros */
#define ACCEPT_MAC_ADDR	    0
#define REJECT_MAC_ADDR	    1

/* Buffer offset from buffer pointer */
#define RX_BUF_OFFSET				0x2

/* Gigabit Ethernet Unit Global Registers */

/* MIB Counters register definitions */
#define ETH_MIB_GOOD_OCTETS_RECEIVED_LOW   0x0
#define ETH_MIB_GOOD_OCTETS_RECEIVED_HIGH  0x4
#define ETH_MIB_BAD_OCTETS_RECEIVED        0x8
#define ETH_MIB_INTERNAL_MAC_TRANSMIT_ERR  0xc
#define ETH_MIB_GOOD_FRAMES_RECEIVED       0x10
#define ETH_MIB_BAD_FRAMES_RECEIVED        0x14
#define ETH_MIB_BROADCAST_FRAMES_RECEIVED  0x18
#define ETH_MIB_MULTICAST_FRAMES_RECEIVED  0x1c
#define ETH_MIB_FRAMES_64_OCTETS           0x20
#define ETH_MIB_FRAMES_65_TO_127_OCTETS    0x24
#define ETH_MIB_FRAMES_128_TO_255_OCTETS   0x28
#define ETH_MIB_FRAMES_256_TO_511_OCTETS   0x2c
#define ETH_MIB_FRAMES_512_TO_1023_OCTETS  0x30
#define ETH_MIB_FRAMES_1024_TO_MAX_OCTETS  0x34
#define ETH_MIB_GOOD_OCTETS_SENT_LOW       0x38
#define ETH_MIB_GOOD_OCTETS_SENT_HIGH      0x3c
#define ETH_MIB_GOOD_FRAMES_SENT           0x40
#define ETH_MIB_EXCESSIVE_COLLISION        0x44
#define ETH_MIB_MULTICAST_FRAMES_SENT      0x48
#define ETH_MIB_BROADCAST_FRAMES_SENT      0x4c
#define ETH_MIB_UNREC_MAC_CONTROL_RECEIVED 0x50
#define ETH_MIB_FC_SENT                    0x54
#define ETH_MIB_GOOD_FC_RECEIVED           0x58
#define ETH_MIB_BAD_FC_RECEIVED            0x5c
#define ETH_MIB_UNDERSIZE_RECEIVED         0x60
#define ETH_MIB_FRAGMENTS_RECEIVED         0x64
#define ETH_MIB_OVERSIZE_RECEIVED          0x68
#define ETH_MIB_JABBER_RECEIVED            0x6c
#define ETH_MIB_MAC_RECEIVE_ERROR          0x70
#define ETH_MIB_BAD_CRC_EVENT              0x74
#define ETH_MIB_COLLISION                  0x78
#define ETH_MIB_LATE_COLLISION             0x7c

/* Port serial status reg (PSR) */
#define ETH_INTERFACE_GMII_MII                          0
#define ETH_INTERFACE_PCM                               BIT0
#define ETH_LINK_IS_DOWN                                0
#define ETH_LINK_IS_UP                                  BIT1
#define ETH_PORT_AT_HALF_DUPLEX                         0
#define ETH_PORT_AT_FULL_DUPLEX                         BIT2
#define ETH_RX_FLOW_CTRL_DISABLED                       0
#define ETH_RX_FLOW_CTRL_ENBALED                        BIT3
#define ETH_GMII_SPEED_100_10                           0
#define ETH_GMII_SPEED_1000                             BIT4
#define ETH_MII_SPEED_10                                0
#define ETH_MII_SPEED_100                               BIT5
#define ETH_NO_TX                                       0
#define ETH_TX_IN_PROGRESS                              BIT7
#define ETH_BYPASS_NO_ACTIVE                            0
#define ETH_BYPASS_ACTIVE                               BIT8
#define ETH_PORT_NOT_AT_PARTITION_STATE                 0
#define ETH_PORT_AT_PARTITION_STATE                     BIT9
#define ETH_PORT_TX_FIFO_NOT_EMPTY                      0
#define ETH_PORT_TX_FIFO_EMPTY                          BIT10


/* These macros describes the Port configuration reg (Px_cR) bits */
#define ETH_UNICAST_NORMAL_MODE                         0
#define ETH_UNICAST_PROMISCUOUS_MODE                    BIT0
#define ETH_DEFAULT_RX_QUEUE_0                          0
#define ETH_DEFAULT_RX_QUEUE_1                          BIT1
#define ETH_DEFAULT_RX_QUEUE_2                          BIT2
#define ETH_DEFAULT_RX_QUEUE_3                          (BIT2 | BIT1)
#define ETH_DEFAULT_RX_QUEUE_4                          BIT3
#define ETH_DEFAULT_RX_QUEUE_5                          (BIT3 | BIT1)
#define ETH_DEFAULT_RX_QUEUE_6                          (BIT3 | BIT2)
#define ETH_DEFAULT_RX_QUEUE_7                          (BIT3 | BIT2 | BIT1)
#define ETH_DEFAULT_RX_ARP_QUEUE_0                      0
#define ETH_DEFAULT_RX_ARP_QUEUE_1                      BIT4
#define ETH_DEFAULT_RX_ARP_QUEUE_2                      BIT5
#define ETH_DEFAULT_RX_ARP_QUEUE_3                      (BIT5 | BIT4)
#define ETH_DEFAULT_RX_ARP_QUEUE_4                      BIT6
#define ETH_DEFAULT_RX_ARP_QUEUE_5                      (BIT6 | BIT4)
#define ETH_DEFAULT_RX_ARP_QUEUE_6                      (BIT6 | BIT5)
#define ETH_DEFAULT_RX_ARP_QUEUE_7                      (BIT6 | BIT5 | BIT4)
#define ETH_RECEIVE_BC_IF_NOT_IP_OR_ARP                 0
#define ETH_REJECT_BC_IF_NOT_IP_OR_ARP                  BIT7
#define ETH_RECEIVE_BC_IF_IP                            0
#define ETH_REJECT_BC_IF_IP                             BIT8
#define ETH_RECEIVE_BC_IF_ARP                           0
#define ETH_REJECT_BC_IF_ARP                            BIT9
#define ETH_TX_AM_NO_UPDATE_ERROR_SUMMARY               BIT12
#define ETH_CAPTURE_TCP_FRAMES_DIS                      0
#define ETH_CAPTURE_TCP_FRAMES_EN                       BIT14
#define ETH_CAPTURE_UDP_FRAMES_DIS                      0
#define ETH_CAPTURE_UDP_FRAMES_EN                       BIT15
#define ETH_DEFAULT_RX_TCP_QUEUE_0                      0
#define ETH_DEFAULT_RX_TCP_QUEUE_1                      BIT16
#define ETH_DEFAULT_RX_TCP_QUEUE_2                      BIT17
#define ETH_DEFAULT_RX_TCP_QUEUE_3                      (BIT17 | BIT16)
#define ETH_DEFAULT_RX_TCP_QUEUE_4                      BIT18
#define ETH_DEFAULT_RX_TCP_QUEUE_5                      (BIT18 | BIT16)
#define ETH_DEFAULT_RX_TCP_QUEUE_6                      (BIT18 | BIT17)
#define ETH_DEFAULT_RX_TCP_QUEUE_7                      (BIT18 | BIT17 | BIT16)
#define ETH_DEFAULT_RX_UDP_QUEUE_0                      0
#define ETH_DEFAULT_RX_UDP_QUEUE_1                      BIT19
#define ETH_DEFAULT_RX_UDP_QUEUE_2                      BIT20
#define ETH_DEFAULT_RX_UDP_QUEUE_3                      (BIT20 | BIT19)
#define ETH_DEFAULT_RX_UDP_QUEUE_4                      (BIT21
#define ETH_DEFAULT_RX_UDP_QUEUE_5                      (BIT21 | BIT19)
#define ETH_DEFAULT_RX_UDP_QUEUE_6                      (BIT21 | BIT20)
#define ETH_DEFAULT_RX_UDP_QUEUE_7                      (BIT21 | BIT20 | BIT19)
#define ETH_DEFAULT_RX_BPDU_QUEUE_0                      0
#define ETH_DEFAULT_RX_BPDU_QUEUE_1                     BIT22
#define ETH_DEFAULT_RX_BPDU_QUEUE_2                     BIT23
#define ETH_DEFAULT_RX_BPDU_QUEUE_3                     (BIT23 | BIT22)
#define ETH_DEFAULT_RX_BPDU_QUEUE_4                     BIT24
#define ETH_DEFAULT_RX_BPDU_QUEUE_5                     (BIT24 | BIT22)
#define ETH_DEFAULT_RX_BPDU_QUEUE_6                     (BIT24 | BIT23)
#define ETH_DEFAULT_RX_BPDU_QUEUE_7                     (BIT24 | BIT23 | BIT22)


/* These macros describes the Port configuration extend reg (Px_cXR) bits*/
#define ETH_CLASSIFY_EN                                 BIT0
#define ETH_SPAN_BPDU_PACKETS_AS_NORMAL                 0
#define ETH_SPAN_BPDU_PACKETS_TO_RX_QUEUE_7             BIT1
#define ETH_PARTITION_DISABLE                           0
#define ETH_PARTITION_ENABLE                            BIT2


/* Tx/Rx queue command reg (RQCR/TQCR)*/
#define ETH_QUEUE_0_ENABLE                              BIT0
#define ETH_QUEUE_1_ENABLE                              BIT1
#define ETH_QUEUE_2_ENABLE                              BIT2
#define ETH_QUEUE_3_ENABLE                              BIT3
#define ETH_QUEUE_4_ENABLE                              BIT4
#define ETH_QUEUE_5_ENABLE                              BIT5
#define ETH_QUEUE_6_ENABLE                              BIT6
#define ETH_QUEUE_7_ENABLE                              BIT7
#define ETH_QUEUE_0_DISABLE                             BIT8
#define ETH_QUEUE_1_DISABLE                             BIT9
#define ETH_QUEUE_2_DISABLE                             BIT10
#define ETH_QUEUE_3_DISABLE                             BIT11
#define ETH_QUEUE_4_DISABLE                             BIT12
#define ETH_QUEUE_5_DISABLE                             BIT13
#define ETH_QUEUE_6_DISABLE                             BIT14
#define ETH_QUEUE_7_DISABLE                             BIT15


/* These macros describes the Port Sdma configuration reg (SDCR) bits */
#define ETH_RIFB                                        BIT0
#define ETH_RX_BURST_SIZE_1_64BIT                       0
#define ETH_RX_BURST_SIZE_2_64BIT                       BIT1
#define ETH_RX_BURST_SIZE_4_64BIT                       BIT2
#define ETH_RX_BURST_SIZE_8_64BIT                       (BIT2 | BIT1)
#define ETH_RX_BURST_SIZE_16_64BIT                      BIT3
#define ETH_BLM_RX_NO_SWAP                              BIT4
#define ETH_BLM_RX_BYTE_SWAP                            0
#define ETH_BLM_TX_NO_SWAP                              BIT5
#define ETH_BLM_TX_BYTE_SWAP                            0
#define ETH_DESCRIPTORS_BYTE_SWAP                       BIT6
#define ETH_DESCRIPTORS_NO_SWAP                         0
#define ETH_TX_BURST_SIZE_1_64BIT                       0
#define ETH_TX_BURST_SIZE_2_64BIT                       BIT22
#define ETH_TX_BURST_SIZE_4_64BIT                       BIT23
#define ETH_TX_BURST_SIZE_8_64BIT                       (BIT23 | BIT22)
#define ETH_TX_BURST_SIZE_16_64BIT                      BIT24



/* These macros describes the Port serial control reg (PSCR) bits */
#define ETH_SERIAL_PORT_DISABLE                         0
#define ETH_SERIAL_PORT_ENABLE                          BIT0
#define ETH_FORCE_LINK_PASS                             BIT1
#define ETH_DO_NOT_FORCE_LINK_PASS                      0
#define ETH_ENABLE_AUTO_NEG_FOR_DUPLX                   0
#define ETH_DISABLE_AUTO_NEG_FOR_DUPLX                  BIT2
#define ETH_ENABLE_AUTO_NEG_FOR_FLOW_CTRL               0
#define ETH_DISABLE_AUTO_NEG_FOR_FLOW_CTRL              BIT3
#define ETH_ADV_NO_FLOW_CTRL                            0
#define ETH_ADV_SYMMETRIC_FLOW_CTRL                     BIT4
#define ETH_FORCE_FC_MODE_NO_PAUSE_DIS_TX               0
#define ETH_FORCE_FC_MODE_TX_PAUSE_DIS                  BIT5
#define ETH_FORCE_BP_MODE_NO_JAM                        0
#define ETH_FORCE_BP_MODE_JAM_TX                        BIT7
#define ETH_FORCE_BP_MODE_JAM_TX_ON_RX_ERR              BIT8
#define ETH_FORCE_LINK_FAIL                             0
#define ETH_DO_NOT_FORCE_LINK_FAIL                      BIT10
#define ETH_RETRANSMIT_16_ATTEMPTS                      0
#define ETH_RETRANSMIT_FOREVER                          BIT11
#define ETH_DISABLE_AUTO_NEG_SPEED_GMII                 BIT13
#define ETH_ENABLE_AUTO_NEG_SPEED_GMII                  0
#define ETH_DTE_ADV_0                                   0
#define ETH_DTE_ADV_1                                   BIT14
#define ETH_DISABLE_AUTO_NEG_BYPASS                     0
#define ETH_ENABLE_AUTO_NEG_BYPASS                      BIT15
#define ETH_AUTO_NEG_NO_CHANGE                          0
#define ETH_RESTART_AUTO_NEG                            BIT16
#define ETH_MAX_RX_PACKET_1518BYTE                      0
#define ETH_MAX_RX_PACKET_1522BYTE                      BIT17
#define ETH_MAX_RX_PACKET_1552BYTE                      BIT18
#define ETH_MAX_RX_PACKET_9022BYTE                      (BIT18 | BIT17)
#define ETH_MAX_RX_PACKET_9192BYTE                      BIT19
#define ETH_MAX_RX_PACKET_9700BYTE                      (BIT19 | BIT17)
#define ETH_SET_EXT_LOOPBACK                            BIT20
#define ETH_CLR_EXT_LOOPBACK                            0
#define ETH_SET_FULL_DUPLEX_MODE                        BIT21
#define ETH_SET_HALF_DUPLEX_MODE                        0
#define ETH_ENABLE_FLOW_CTRL_TX_RX_IN_FULL_DUPLEX       BIT22
#define ETH_DISABLE_FLOW_CTRL_TX_RX_IN_FULL_DUPLEX      0
#define ETH_SET_GMII_SPEED_TO_10_100                    0
#define ETH_SET_GMII_SPEED_TO_1000                      BIT23
#define ETH_SET_MII_SPEED_TO_10                         0
#define ETH_SET_MII_SPEED_TO_100                        BIT24


/* SMI reg */
#define ETH_SMI_BUSY        	BIT28	/* 0 - Write, 1 - Read          */
#define ETH_SMI_READ_VALID  	BIT27	/* 0 - Write, 1 - Read          */
#define ETH_SMI_OPCODE_WRITE	0	/* Completion of Read operation */
#define ETH_SMI_OPCODE_READ 	BIT26	/* Operation is in progress             */

/* SDMA command status fields macros */

/* Tx & Rx descriptors status */
#define ETH_ERROR_SUMMARY                   (BIT0)

/* Tx & Rx descriptors command */
#define ETH_BUFFER_OWNED_BY_DMA             (BIT31)

/* Tx descriptors status */
#define ETH_LC_ERROR                        (0	  )
#define ETH_UR_ERROR                        (BIT1 )
#define ETH_RL_ERROR                        (BIT2 )
#define ETH_LLC_SNAP_FORMAT                 (BIT9 )

/* Rx descriptors status */
#define ETH_CRC_ERROR                       (0	  )
#define ETH_OVERRUN_ERROR                   (BIT1 )
#define ETH_MAX_FRAME_LENGTH_ERROR          (BIT2 )
#define ETH_RESOURCE_ERROR                  ((BIT2 | BIT1))
#define ETH_VLAN_TAGGED                     (BIT19)
#define ETH_BPDU_FRAME                      (BIT20)
#define ETH_TCP_FRAME_OVER_IP_V_4           (0    )
#define ETH_UDP_FRAME_OVER_IP_V_4           (BIT21)
#define ETH_OTHER_FRAME_TYPE                (BIT22)
#define ETH_LAYER_2_IS_ETH_V_2              (BIT23)
#define ETH_FRAME_TYPE_IP_V_4               (BIT24)
#define ETH_FRAME_HEADER_OK                 (BIT25)
#define ETH_RX_LAST_DESC                    (BIT26)
#define ETH_RX_FIRST_DESC                   (BIT27)
#define ETH_UNKNOWN_DESTINATION_ADDR        (BIT28)
#define ETH_RX_ENABLE_INTERRUPT             (BIT29)
#define ETH_LAYER_4_CHECKSUM_OK             (BIT30)

/* Rx descriptors byte count */
#define ETH_FRAME_FRAGMENTED                (BIT2)

/* Tx descriptors command */
#define ETH_LAYER_4_CHECKSUM_FIRST_DESC		(BIT10)
#define ETH_FRAME_SET_TO_VLAN               (BIT15)
#define ETH_TCP_FRAME                       (0	  )
#define ETH_UDP_FRAME                       (BIT16)
#define ETH_GEN_TCP_UDP_CHECKSUM            (BIT17)
#define ETH_GEN_IP_V_4_CHECKSUM             (BIT18)
#define ETH_ZERO_PADDING                    (BIT19)
#define ETH_TX_LAST_DESC                    (BIT20)
#define ETH_TX_FIRST_DESC                   (BIT21)
#define ETH_GEN_CRC                         (BIT22)
#define ETH_TX_ENABLE_INTERRUPT             (BIT23)
#define ETH_AUTO_MODE                       (BIT30)

/* typedefs */

typedef enum _eth_port {
	ETH_0 = 0,
	ETH_1 = 1,
	ETH_2 = 2
} ETH_PORT;

typedef enum _eth_func_ret_status {
	ETH_OK,			/* Returned as expected.                    */
	ETH_ERROR,		/* Fundamental error.                       */
	ETH_RETRY,		/* Could not process request. Try later.    */
	ETH_END_OF_JOB,		/* Ring has nothing to process.             */
	ETH_QUEUE_FULL,		/* Ring resource error.                     */
	ETH_QUEUE_LAST_RESOURCE	/* Ring resources about to exhaust.         */
} ETH_FUNC_RET_STATUS;

typedef enum _eth_target {
	ETH_TARGET_DRAM,
	ETH_TARGET_DEVICE,
	ETH_TARGET_CBS,
	ETH_TARGET_PCI0,
	ETH_TARGET_PCI1
} ETH_TARGET;

/* These are for big-endian machines.  Little endian needs different
 * definitions.
 */
#if defined(__BIG_ENDIAN)
typedef struct _eth_rx_desc {
	u16	byte_cnt;	/* Descriptor buffer byte count     */
	u16	buf_size;	/* Buffer size                      */
	u32	cmd_sts;	/* Descriptor command status        */
	u32	next_desc_ptr;	/* Next descriptor pointer          */
	u32	buf_ptr;	/* Descriptor buffer pointer        */
} ETH_RX_DESC;

typedef struct _eth_tx_desc {
	u16	byte_cnt;	/* buffer byte count */
	u16	l4i_chk;	/* CPU provided TCP checksum */
	u32	cmd_sts;	/* Command/status field */
	u32	next_desc_ptr;	/* Pointer to next descriptor */
	u32	buf_ptr;	/* pointer to buffer for this descriptor */
} ETH_TX_DESC;

#elif defined(__LITTLE_ENDIAN)
typedef struct _eth_rx_desc {
	u32	cmd_sts;	/* Descriptor command status        */
	u16	buf_size;	/* Buffer size                      */
	u16	byte_cnt;	/* Descriptor buffer byte count     */
	u32	buf_ptr;	/* Descriptor buffer pointer        */
	u32	next_desc_ptr;	/* Next descriptor pointer          */
} ETH_RX_DESC;

typedef struct _eth_tx_desc {
	u32	cmd_sts;	/* Command/status field */
	u16	l4i_chk;	/* CPU provided TCP checksum */
	u16	byte_cnt;	/* buffer byte count */
	u32	buf_ptr;	/* pointer to buffer for this descriptor */
	u32	next_desc_ptr;	/* Pointer to next descriptor */
} ETH_TX_DESC;
#else
#error One of __BIG_ENDIAN or __LITTLE_ENDIAN must be defined
#endif

/* Unified struct for Rx and Tx operations. The user is not required to */
/* be familier with neither Tx nor Rx descriptors.                       */
typedef struct _pkt_info {
	unsigned short byte_cnt;	/* Descriptor buffer byte count     */
	unsigned short l4i_chk;	/* Tx CPU provided TCP Checksum     */
	unsigned int cmd_sts;	/* Descriptor command status        */
	unsigned int buf_ptr;	/* Descriptor buffer pointer        */
	struct sk_buff* return_info;	/* User resource return information */
} PKT_INFO;


/* Ethernet port specific infomation */

typedef struct _eth_port_ctrl {
	ETH_PORT port_num;		/* User Ethernet port number */
	u8	port_mac_addr[6];	/* User defined port MAC address. */
	u32	port_config;		/* User port configuration value */
	u32	port_config_extend;	/* User port config extend value */
	u32	port_sdma_config;	/* User port SDMA config value */
	u32	port_serial_control;	/* User port serial control value */
	u32	port_tx_queue_command;	/* Port active Tx queues summary */
	u32	port_rx_queue_command;	/* Port active Rx queues summary */

	/* User scratch pad for user specific data structures */
	void*	port_private;

	bool	rx_resource_err;	/* Rx ring resource error flag */
	bool	tx_resource_err;	/* Tx ring resource error flag */

	/* Tx/Rx rings managment indexes fields. For driver use */

	/* Next available and first returning Rx resource */
	int rx_curr_desc_q, rx_used_desc_q;

	/* Next available and first returning Tx resource */
	int tx_curr_desc_q, tx_used_desc_q;
#ifdef MV64340_CHECKSUM_OFFLOAD_TX
        int tx_first_desc_q;
#endif

#ifdef MV64340_TX_FAST_REFILL
	u32	tx_clean_threshold;
#endif

	/* Tx/Rx rings size and base variables fields. For driver use */
	volatile ETH_RX_DESC *p_rx_desc_area;
	unsigned int rx_desc_area_size;
	struct sk_buff* rx_skb[MV64340_RX_QUEUE_SIZE];

	volatile ETH_TX_DESC *p_tx_desc_area;
	unsigned int tx_desc_area_size;
	struct sk_buff* tx_skb[MV64340_TX_QUEUE_SIZE];
	struct tq_struct tx_timeout_task;
} ETH_PORT_INFO;


/* ethernet.h API list */

/* Port operation control routines */
static void eth_port_init(ETH_PORT_INFO * p_eth_port_ctrl);
static void eth_port_reset(ETH_PORT eth_port_num);
static bool eth_port_start(ETH_PORT_INFO * p_eth_port_ctrl);

static void ethernet_set_config_reg(ETH_PORT eth_port_num,
				    unsigned int value);
static unsigned int ethernet_get_config_reg(ETH_PORT eth_port_num);

/* Interrupt Coalesting functions */
static unsigned int eth_port_set_rx_coal(ETH_PORT, unsigned int,
					 unsigned int);
static unsigned int eth_port_set_tx_coal(ETH_PORT, unsigned int,
					 unsigned int);

/* Port MAC address routines */
static void eth_port_uc_addr_set(ETH_PORT eth_port_num,
				 unsigned char *p_addr);

/* PHY and MIB routines */
static bool ethernet_phy_reset(ETH_PORT eth_port_num);

static bool eth_port_write_smi_reg(ETH_PORT eth_port_num,
				   unsigned int phy_reg,
				   unsigned int value);

static bool eth_port_read_smi_reg(ETH_PORT eth_port_num,
				  unsigned int phy_reg,
				  unsigned int *value);

static void eth_clear_mib_counters(ETH_PORT eth_port_num);

/* Port data flow control routines */
static ETH_FUNC_RET_STATUS eth_port_send(ETH_PORT_INFO * p_eth_port_ctrl,
					 PKT_INFO * p_pkt_info);
static ETH_FUNC_RET_STATUS eth_tx_return_desc(ETH_PORT_INFO *
					      p_eth_port_ctrl,
					      PKT_INFO * p_pkt_info);
static ETH_FUNC_RET_STATUS eth_port_receive(ETH_PORT_INFO *
					    p_eth_port_ctrl,
					    PKT_INFO * p_pkt_info);
static ETH_FUNC_RET_STATUS eth_rx_return_buff(ETH_PORT_INFO *
					      p_eth_port_ctrl,
					      PKT_INFO * p_pkt_info);


static bool ether_init_tx_desc_ring(ETH_PORT_INFO * p_eth_port_ctrl,
				    int tx_desc_num,
				    unsigned long tx_desc_base_addr);

static bool ether_init_rx_desc_ring(ETH_PORT_INFO * p_eth_port_ctrl,
				    int rx_desc_num,
				    int rx_buff_size,
				    unsigned long rx_desc_base_addr,
				    unsigned long rx_buff_base_addr);

#endif				/* MV64340_ETH_ */
