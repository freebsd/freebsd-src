/**************************************************************************
**************************************************************************

Copyright (c) 2001 Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms of the Software, with or
without modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code of the Software may retain the above
    copyright notice, this list of conditions and the following disclaimer.

 2. Redistributions in binary form of the Software may reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors shall be used to endorse or promote products derived from
    this Software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR ITS CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

$FreeBSD$
***************************************************************************
***************************************************************************/

#ifndef _EM_H_DEFINED_
#define _EM_H_DEFINED_


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <sys/sockio.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/clock.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include "opt_bdg.h"

#include <dev/em/if_em_fxhw.h>
#include <dev/em/if_em_phy.h>

/* Tunables */
#define MAX_TXD                         256
#define MAX_RXD                         256
#define TX_CLEANUP_THRESHOLD            MAX_TXD / 8
#define TIDV                            128     
#define RIDV                            28      
#define DO_AUTO_NEG                     1       
#define WAIT_FOR_AUTO_NEG_DEFAULT       1       
#define AUTONEG_ADV_DEFAULT             (ADVERTISE_10_HALF | ADVERTISE_10_FULL | \
                                         ADVERTISE_100_HALF | ADVERTISE_100_FULL | \
                                         ADVERTISE_1000_FULL)
#define EM_ENABLE_RXCSUM_OFFLOAD        1
#define EM_REPORT_TX_EARLY              2
#define EM_CHECKSUM_FEATURES            (CSUM_TCP | CSUM_UDP)
#define EM_MAX_INTR                     3
#define EM_TX_TIMEOUT                   5    /* set to 5 seconds */
#define EM_JUMBO_ENABLE_DEFAULT         0


#define EM_VENDOR_ID                    0x8086
#define EM_MMBA                         0x0010 /* Mem base address */
#define EM_ROUNDUP(size, unit) (((size) + (unit) - 1) & ~((unit) - 1))
#define EM_JUMBO_PBA                    0x00000028
#define EM_DEFAULT_PBA                  0x00000030

#define IOCTL_CMD_TYPE                  u_long
#define ETH_LENGTH_OF_ADDRESS           ETHER_ADDR_LEN
#define PCI_COMMAND_REGISTER            PCIR_COMMAND
#define MAX_NUM_MULTICAST_ADDRESSES     128
#define PCI_ANY_ID                      (~0U)
#define ETHER_ALIGN                     2
#define CMD_MEM_WRT_INVALIDATE          0x0010

/* Defines for printing debug information */
#define DEBUG_INIT  0
#define DEBUG_IOCTL 0
#define DEBUG_HW    0
#define DEBUG_TXRX  0
#define DEBUG_RXCSUM 0
#define DEBUG_TXCSUM 0

#define INIT_DEBUGOUT(S)            if (DEBUG_INIT)  printf(S "\n")
#define INIT_DEBUGOUT1(S, A)        if (DEBUG_INIT)  printf(S "\n", A)
#define INIT_DEBUGOUT2(S, A, B)     if (DEBUG_INIT)  printf(S "\n", A, B)
#define IOCTL_DEBUGOUT(S)           if (DEBUG_IOCTL) printf(S "\n")
#define IOCTL_DEBUGOUT1(S, A)       if (DEBUG_IOCTL) printf(S "\n", A)
#define IOCTL_DEBUGOUT2(S, A, B)    if (DEBUG_IOCTL) printf(S "\n", A, B)
#define HW_DEBUGOUT(S)              if (DEBUG_HW) printf(S "\n")
#define HW_DEBUGOUT1(S, A)          if (DEBUG_HW) printf(S "\n", A)
#define HW_DEBUGOUT2(S, A, B)       if (DEBUG_HW) printf(S "\n", A, B)
#define TXRX_DEBUGOUT(S)              if (DEBUG_TXRX) printf(S "\n")
#define TXRX_DEBUGOUT1(S, A)          if (DEBUG_TXRX) printf(S "\n", A)
#define TXRX_DEBUGOUT2(S, A, B)       if (DEBUG_TXRX) printf(S "\n", A, B)
#define RXCSUM_DEBUGOUT(S)              if (DEBUG_RXCSUM) printf(S "\n")
#define RXCSUM_DEBUGOUT1(S, A)          if (DEBUG_RXCSUM) printf(S "\n", A)
#define RXCSUM_DEBUGOUT2(S, A, B)       if (DEBUG_RXCSUM) printf(S "\n", A, B)
#define TXCSUM_DEBUGOUT(S)              if (DEBUG_TXCSUM) printf(S "\n")
#define TXCSUM_DEBUGOUT1(S, A)          if (DEBUG_TXCSUM) printf(S "\n", A)
#define TXCSUM_DEBUGOUT2(S, A, B)       if (DEBUG_TXCSUM) printf(S "\n", A, B)

/* Device ID defines */
#define PCI_DEVICE_ID_82542            0x1000
#define PCI_DEVICE_ID_82543GC_FIBER    0x1001
#define PCI_DEVICE_ID_82543GC_COPPER   0x1004
#define PCI_DEVICE_ID_82544EI_FIBER    0x1009
#define PCI_DEVICE_ID_82544EI_COPPER   0x1008
#define PCI_DEVICE_ID_82544GC_STRG     0x100C
#define PCI_DEVICE_ID_82544GC_COPPER   0x100D

/* Supported RX Buffer Sizes */
#define EM_RXBUFFER_2048        2048
#define EM_RXBUFFER_4096        4096
#define EM_RXBUFFER_8192        8192
#define EM_RXBUFFER_16384      16384


/* Jumbo Frame */
#define EM_JSLOTS                   384
#define EM_JUMBO_FRAMELEN          9018
#define EM_JUMBO_MTU               (EM_JUMBO_FRAMELEN - ETHER_HDR_LEN - ETHER_CRC_LEN)
#define EM_JRAWLEN (EM_JUMBO_FRAMELEN + ETHER_ALIGN + sizeof(u_int64_t))
#define EM_JLEN (EM_JRAWLEN + (sizeof(u_int64_t) - \
                              (EM_JRAWLEN % sizeof(u_int64_t))))
#define EM_JPAGESZ PAGE_SIZE
#define EM_RESID (EM_JPAGESZ - (EM_JLEN * EM_JSLOTS) % EM_JPAGESZ)
#define EM_JMEM ((EM_JLEN * EM_JSLOTS) + EM_RESID)

struct em_jslot {
        caddr_t                 em_buf;
        int                     em_inuse;
};

struct em_jpool_entry {
        int                             slot;
        SLIST_ENTRY(em_jpool_entry)     em_jpool_entries;
};



/* ******************************************************************************
 * vendor_info_array
 *
 * This array contains the list of Subvendor/Subdevice IDs on which the driver
 * should load.
 *
 * ******************************************************************************/
typedef struct _em_vendor_info_t
{
        unsigned int vendor_id;
        unsigned int device_id;
        unsigned int subvendor_id;
        unsigned int subdevice_id;
        unsigned int index;
} em_vendor_info_t;


struct em_tx_buffer {
        STAILQ_ENTRY(em_tx_buffer) em_tx_entry;
        struct mbuf    *Packet;
        u_int32_t       NumTxDescriptorsUsed;
};


/* ******************************************************************************
 * This structure stores information about the 2k aligned receive buffer
 * into which the E1000 DMA's frames. 
 * ******************************************************************************/
struct em_rx_buffer {
        STAILQ_ENTRY(em_rx_buffer) em_rx_entry;
        struct mbuf    *Packet;
        u_int32_t       LowPhysicalAddress;
        u_int32_t       HighPhysicalAddress;
};

typedef enum _XSUM_CONTEXT_T {
        OFFLOAD_NONE,
        OFFLOAD_TCP_IP,
        OFFLOAD_UDP_IP
} XSUM_CONTEXT_T;

/* Our adapter structure */
struct adapter {
        struct arpcom   interface_data;
        struct adapter *next;
        struct adapter *prev;

        /* FreeBSD operating-system-specific structures */
        bus_space_tag_t bus_space_tag;
        bus_space_handle_t bus_space_handle;
        struct device   *dev;
        struct resource *res_memory;
        struct resource *res_interrupt;
        void            *int_handler_tag;
        struct ifmedia  media;
        struct callout_handle timer_handle;
        u_int8_t        unit;

        /* PCI Info */
        u_int16_t       VendorId;
        u_int16_t       DeviceId;
        u_int8_t        RevId;
        u_int16_t       SubVendorId;
        u_int16_t       SubSystemId;
        u_int16_t       PciCommandWord;

        /* PCI Bus Info */
        E1000_BUS_TYPE_ENUM BusType;
        E1000_BUS_SPEED_ENUM BusSpeed;
        E1000_BUS_WIDTH_ENUM BusWidth;

        /* Info about the board itself */
        u_int8_t        MacType;
        u_int8_t        MediaType;
        u_int32_t       PhyId;
        u_int32_t       PhyAddress;
        uint8_t         CurrentNetAddress[ETH_LENGTH_OF_ADDRESS];
        uint8_t         PermNetAddress[ETH_LENGTH_OF_ADDRESS];
        u_int32_t       PartNumber;

        u_int8_t        AdapterStopped;
        u_int8_t        DmaFairness;
        u_int8_t        ReportTxEarly;
        u_int32_t       MulticastFilterType;
        u_int32_t       NumberOfMcAddresses;
        u_int8_t        MulticastAddressList[MAX_NUM_MULTICAST_ADDRESSES][ETH_LENGTH_OF_ADDRESS];

        u_int8_t        GetLinkStatus;
        u_int8_t        LinkStatusChanged;
        u_int8_t        LinkIsActive;
        u_int32_t       AutoNegFailed;
        u_int8_t        AutoNeg;
        u_int16_t       AutoNegAdvertised;
        u_int8_t        WaitAutoNegComplete;
        u_int8_t        ForcedSpeedDuplex;
        u_int16_t       LineSpeed;
        u_int16_t       FullDuplex;
        u_int8_t        TbiCompatibilityEnable;
        u_int8_t        TbiCompatibilityOn;
        u_int32_t       TxcwRegValue;
        u_int32_t       OriginalFlowControl;
        u_int32_t       FlowControl;
        u_int16_t       FlowControlHighWatermark;
        u_int16_t       FlowControlLowWatermark;
        u_int16_t       FlowControlPauseTime;
        u_int8_t        FlowControlSendXon;

        u_int32_t       MaxFrameSize;
        u_int32_t       TxIntDelay;
        u_int32_t       RxIntDelay;

        u_int8_t        RxChecksum;
        XSUM_CONTEXT_T  ActiveChecksumContext;

        u_int8_t        MdiX;
        u_int8_t        DisablePolarityCorrection;

        /* Transmit definitions */
        struct _E1000_TRANSMIT_DESCRIPTOR *FirstTxDescriptor;
        struct _E1000_TRANSMIT_DESCRIPTOR *LastTxDescriptor;
        struct _E1000_TRANSMIT_DESCRIPTOR *NextAvailTxDescriptor;
        struct _E1000_TRANSMIT_DESCRIPTOR *OldestUsedTxDescriptor;
        struct _E1000_TRANSMIT_DESCRIPTOR *TxDescBase;
        volatile u_int16_t NumTxDescriptorsAvail;
        u_int16_t       NumTxDescriptors;
        u_int32_t       TxdCmd;
        struct em_tx_buffer   *tx_buffer_area;
        STAILQ_HEAD(__em_tx_buffer_free, em_tx_buffer)  FreeSwTxPacketList;
        STAILQ_HEAD(__em_tx_buffer_used, em_tx_buffer)  UsedSwTxPacketList;

        /* Receive definitions */
        struct _E1000_RECEIVE_DESCRIPTOR *FirstRxDescriptor;
        struct _E1000_RECEIVE_DESCRIPTOR *LastRxDescriptor;
        struct _E1000_RECEIVE_DESCRIPTOR *NextRxDescriptorToCheck;
        struct _E1000_RECEIVE_DESCRIPTOR *RxDescBase;
        u_int16_t       NumRxDescriptors;
        u_int16_t       NumRxDescriptorsEmpty;
        u_int16_t       NextRxDescriptorToFill;
        u_int32_t       RxBufferLen;
        struct em_rx_buffer   *rx_buffer_area;
        STAILQ_HEAD(__em_rx_buffer, em_rx_buffer)  RxSwPacketList;

        /* Jumbo frame */
        u_int8_t               JumboEnable;
        struct em_jslot        em_jslots[EM_JSLOTS];
        void                  *em_jumbo_buf;
        SLIST_HEAD(__em_jfreehead, em_jpool_entry)      em_jfree_listhead;
        SLIST_HEAD(__em_jinusehead, em_jpool_entry)     em_jinuse_listhead;


        /* Misc stats maintained by the driver */
        unsigned long   DroppedPackets;
        unsigned long   NoJumboBufAvail;
        unsigned long   JumboMbufFailed;
        unsigned long   JumboClusterFailed;
        unsigned long   StdMbufFailed;
        unsigned long   StdClusterFailed;
#ifdef DBG_STATS
        unsigned long   NoTxDescAvail;
        unsigned long   NoPacketsAvail;
        unsigned long   CleanTxInterrupts;
        unsigned long   NoTxBufferAvail1;
        unsigned long   NoTxBufferAvail2;
#endif

        /* Statistics registers present in the 82542 */
        unsigned long   Crcerrs;
        unsigned long   Symerrs;
        unsigned long   Mpc;
        unsigned long   Scc;
        unsigned long   Ecol;
        unsigned long   Mcc;
        unsigned long   Latecol;
        unsigned long   Colc;
        unsigned long   Dc;
        unsigned long   Sec;
        unsigned long   Rlec;
        unsigned long   Xonrxc;
        unsigned long   Xontxc;
        unsigned long   Xoffrxc;
        unsigned long   Xofftxc;
        unsigned long   Fcruc;
        unsigned long   Prc64;
        unsigned long   Prc127;
        unsigned long   Prc255;
        unsigned long   Prc511;
        unsigned long   Prc1023;
        unsigned long   Prc1522;
        unsigned long   Gprc;
        unsigned long   Bprc;
        unsigned long   Mprc;
        unsigned long   Gptc;
        unsigned long   Gorcl;
        unsigned long   Gorch;
        unsigned long   Gotcl;
        unsigned long   Gotch;
        unsigned long   Rnbc;
        unsigned long   Ruc;
        unsigned long   Rfc;
        unsigned long   Roc;
        unsigned long   Rjc;
        unsigned long   Torcl;
        unsigned long   Torch;
        unsigned long   Totcl;
        unsigned long   Totch;
        unsigned long   Tpr;
        unsigned long   Tpt;
        unsigned long   Ptc64;
        unsigned long   Ptc127;
        unsigned long   Ptc255;
        unsigned long   Ptc511;
        unsigned long   Ptc1023;
        unsigned long   Ptc1522;
        unsigned long   Mptc;
        unsigned long   Bptc;
        /* Statistics registers added in the 82543 */
        unsigned long   Algnerrc;
        unsigned long   Rxerrc;
        unsigned long   Tuc;
        unsigned long   Tncrs;
        unsigned long   Cexterr;
        unsigned long   Rutec;
        unsigned long   Tsctc;
        unsigned long   Tsctfc;

};

extern void em_adjust_tbi_accepted_stats(struct adapter * Adapter,
                                            u32 FrameLength, u8 * MacAddress);

#endif                                                  /* _EM_H_DEFINED_ */
