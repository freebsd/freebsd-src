/*************************************************************************
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
**************************************************************************/

#ifndef _EM_FXHW_H_
#define _EM_FXHW_H_

/*
* Workfile: fxhw.h 
* Date: 9/25/01 2:40p 
* Revision: 43 
*/

#define _FXHW_

struct adapter;
struct _E1000_TRANSMIT_DESCRIPTOR;
struct _E1000_RECEIVE_DESCRIPTOR;
struct E1000_REGISTERS;

typedef enum _MAC_TYPE {
        MAC_WISEMAN_2_0 = 0,
        MAC_WISEMAN_2_1,
        MAC_LIVENGOOD,
        MAC_WAINWRIGHT,
        MAC_CORDOVA,
        NUM_MACS
} MAC_TYPE, *PMAC_TYPE;

typedef enum _GIGABIT_MEDIA_TYPE {
        MEDIA_TYPE_COPPER = 0,
        MEDIA_TYPE_FIBER = 1,
        NUM_MEDIA_TYPES
} GIGABIT_MEDIA_TYPE, *PGIGABIT_MEDIA_TYPE;

typedef enum _SPEED_DUPLEX_TYPE {
        HALF_10 = 0,
        FULL_10 = 1,
        HALF_100 = 2,
        FULL_100 = 3
} SPEED_DUPLEX_TYPE, *PSPEED_DUPLEX_TYPE;

typedef enum _FLOW_CONTROL_TYPE {
        FLOW_CONTROL_NONE = 0,
        FLOW_CONTROL_RECEIVE_PAUSE = 1,
        FLOW_CONTROL_TRANSMIT_PAUSE = 2,
        FLOW_CONTROL_FULL = 3,
        FLOW_CONTROL_HW_DEFAULT = 0xFF
} FLOW_CONTROL_TYPE, *PFLOW_CONTROL_TYPE;

typedef enum {
        E1000_BUS_TYPE_UNKNOWN = 0,
        E1000_BUS_TYPE_PCI,
        E1000_BUS_TYPE_PCIX
} E1000_BUS_TYPE_ENUM;

typedef enum {
        E1000_BUS_SPEED_UNKNOWN = 0,
        E1000_BUS_SPEED_PCI_33MHZ,
        E1000_BUS_SPEED_PCI_66MHZ,
        E1000_BUS_SPEED_PCIX_50_66MHZ,
        E1000_BUS_SPEED_PCIX_66_100MHZ,
        E1000_BUS_SPEED_PCIX_100_133MHZ,
        E1000_BUS_SPEED_PCIX_RESERVED
} E1000_BUS_SPEED_ENUM;

typedef enum {
        E1000_BUS_WIDTH_UNKNOWN = 0,
        E1000_BUS_WIDTH_32_BIT,
        E1000_BUS_WIDTH_64_BIT
} E1000_BUS_WIDTH_ENUM;

#include <dev/em/if_em_osdep.h>

void em_adapter_stop(struct adapter *Adapter);
u8 em_initialize_hardware(struct adapter *Adapter);
void em_init_rx_addresses(struct adapter *Adapter);

void em_multicast_address_list_update(struct adapter *Adapter,
                                      u8 * MulticastAddressList,
                                      u32 MulticastAddressCount,

                                      u32 Padding);
u32 em_hash_multicast_address(struct adapter *Adapter,

                              u8 * MulticastAddress);
void em_mta_set(struct adapter *Adapter, u32 HashValue);
void em_rar_set(struct adapter *Adapter,

                u8 * MulticastAddress, u32 RarIndex);
void em_write_vfta(struct adapter *Adapter, u32 Offset, u32 Value);
void em_clear_vfta(struct adapter *Adapter);

u8 em_setup_flow_control_and_link(struct adapter *Adapter);
u8 em_setup_pcs_link(struct adapter *Adapter, u32 DeviceControlReg);
void em_config_flow_control_after_link_up(struct adapter *Adapter);
void em_force_mac_flow_control_setting(struct adapter *Adapter);
void em_check_for_link(struct adapter *Adapter);
void em_get_speed_and_duplex(struct adapter *Adapter,

                             u16 * Speed, u16 * Duplex);

void em_cleanup_eeprom(struct adapter *Adapter);
void em_clock_eeprom(struct adapter *Adapter);
void em_setup_eeprom(struct adapter *Adapter);
void em_standby_eeprom(struct adapter *Adapter);
u16 em_read_eeprom_word(struct adapter *Adapter, u16 Reg);
u8 em_validate_eeprom_checksum(struct adapter *Adapter);
void em_update_eeprom_checksum(struct adapter *Adapter);
u8 em_write_eeprom_word(struct adapter *Adapter, u16 reg, u16 data);

void em_clear_hw_stats_counters(struct adapter *Adapter);
u8 em_read_part_number(struct adapter *Adapter, u32 * PartNumber);
void em_id_led_on(struct adapter *Adapter);
void em_id_led_off(struct adapter *Adapter);
void em_set_id_led_for_pc_ix(struct adapter *Adapter);
u8 em_is_low_profile(struct adapter *Adapter);
void em_get_bus_type_speed_width(struct adapter *Adapter);

#define MAC_DECODE_SIZE (128 * 1024)

#define WISEMAN_2_0_REV_ID               2
#define WISEMAN_2_1_REV_ID               3

#define SPEED_10                        10
#define SPEED_100                      100
#define SPEED_1000                    1000
#define HALF_DUPLEX                      1
#define FULL_DUPLEX                      2

#define ENET_HEADER_SIZE                14
#define MAXIMUM_ETHERNET_PACKET_SIZE  1514
#define MINIMUM_ETHERNET_PACKET_SIZE    60
#define CRC_LENGTH                       4

#define MAX_JUMBO_FRAME_SIZE    (0x3F00)

#define ISL_CRC_LENGTH                         4

#define MAXIMUM_VLAN_ETHERNET_PACKET_SIZE   1514
#define MINIMUM_VLAN_ETHERNET_PACKET_SIZE     60
#define VLAN_TAG_SIZE                          4

#define ETHERNET_IEEE_VLAN_TYPE     0x8100
#define ETHERNET_IP_TYPE            0x0800
#define ETHERNET_IPX_TYPE           0x8037
#define ETHERNET_IPX_OLD_TYPE       0x8137
#define MAX_802_3_LEN_FIELD         0x05DC

#define ETHERNET_ARP_TYPE           0x0806
#define ETHERNET_XNS_TYPE           0x0600
#define ETHERNET_X25_TYPE           0x0805
#define ETHERNET_BANYAN_TYPE        0x0BAD
#define ETHERNET_DECNET_TYPE        0x6003
#define ETHERNET_APPLETALK_TYPE     0x809B
#define ETHERNET_SNA_TYPE           0x80D5
#define ETHERNET_SNMP_TYPE          0x814C

#define IP_OFF_MF_BIT               0x0002
#define IP_OFF_OFFSET_MASK          0xFFF8
#define IP_PROTOCOL_ICMP                 1
#define IP_PROTOCOL_IGMP                 2
#define IP_PROTOCOL_TCP                  6
#define IP_PROTOCOL_UDP               0x11
#define IP_PROTOCOL_IPRAW             0xFF

#define POLL_IMS_ENABLE_MASK (E1000_IMS_RXDMT0 | E1000_IMS_RXSEQ)

#define IMS_ENABLE_MASK (E1000_IMS_RXT0 | E1000_IMS_TXDW | E1000_IMS_RXDMT0 | E1000_IMS_RXSEQ | E1000_IMS_LSC)

#define E1000_RAR_ENTRIES 16

typedef struct _E1000_RECEIVE_DESCRIPTOR {
        E1000_64_BIT_PHYSICAL_ADDRESS BufferAddress;

        u16 Length;
        u16 Csum;
        u8 ReceiveStatus;
        u8 Errors;
        u16 Special;

} E1000_RECEIVE_DESCRIPTOR, *PE1000_RECEIVE_DESCRIPTOR;

#define MIN_NUMBER_OF_DESCRIPTORS (8)
#define MAX_NUMBER_OF_DESCRIPTORS (0xFFF8)

#define E1000_RXD_STAT_DD        (0x01)
#define E1000_RXD_STAT_EOP       (0x02)

#define E1000_RXD_STAT_ISL       (0x04)
#define E1000_RXD_STAT_IXSM      (0x04)
#define E1000_RXD_STAT_VP        (0x08)
#define E1000_RXD_STAT_BPDU      (0x10)
#define E1000_RXD_STAT_TCPCS     (0x20)
#define E1000_RXD_STAT_IPCS      (0x40)

#define E1000_RXD_STAT_PIF       (0x80)

#define E1000_RXD_ERR_CE         (0x01)
#define E1000_RXD_ERR_SE         (0x02)
#define E1000_RXD_ERR_SEQ        (0x04)

#define E1000_RXD_ERR_ICE        (0x08)

#define E1000_RXD_ERR_CXE        (0x10)

#define E1000_RXD_ERR_TCPE       (0x20)
#define E1000_RXD_ERR_IPE        (0x40)

#define E1000_RXD_ERR_RXE        (0x80)

#define E1000_RXD_ERR_FRAME_ERR_MASK (E1000_RXD_ERR_CE | E1000_RXD_ERR_SE | E1000_RXD_ERR_SEQ | E1000_RXD_ERR_CXE | E1000_RXD_ERR_RXE)

#define E1000_RXD_SPC_VLAN_MASK  (0x0FFF)
#define E1000_RXD_SPC_PRI_MASK   (0xE000)
#define E1000_RXD_SPC_PRI_SHIFT  (0x000D)
#define E1000_RXD_SPC_CFI_MASK   (0x1000)
#define E1000_RXD_SPC_CFI_SHIFT  (0x000C)

#define E1000_TXD_DTYP_D        (0x00100000)
#define E1000_TXD_DTYP_C        (0x00000000)
#define E1000_TXD_POPTS_IXSM    (0x01)
#define E1000_TXD_POPTS_TXSM    (0x02)

typedef struct _E1000_TRANSMIT_DESCRIPTOR {
        E1000_64_BIT_PHYSICAL_ADDRESS BufferAddress;

        union {
                u32 DwordData;
                struct _TXD_FLAGS {
                        u16 Length;
                        u8 Cso;
                        u8 Cmd;
                } Flags;
        } Lower;

        union {
                u32 DwordData;
                struct _TXD_FIELDS {
                        u8 TransmitStatus;
                        u8 Css;
                        u16 Special;
                } Fields;
        } Upper;

} E1000_TRANSMIT_DESCRIPTOR, *PE1000_TRANSMIT_DESCRIPTOR;

typedef struct _E1000_TCPIP_CONTEXT_TRANSMIT_DESCRIPTOR {
        union {
                u32 IpXsumConfig;
                struct _IP_XSUM_FIELDS {
                        u8 Ipcss;
                        u8 Ipcso;
                        u16 Ipcse;
                } IpFields;
        } LowerXsumSetup;

        union {
                u32 TcpXsumConfig;
                struct _TCP_XSUM_FIELDS {
                        u8 Tucss;
                        u8 Tucso;
                        u16 Tucse;
                } TcpFields;
        } UpperXsumSetup;

        u32 CmdAndLength;

        union {
                u32 DwordData;
                struct _TCP_SEG_FIELDS {
                        u8 Status;
                        u8 HdrLen;
                        u16 Mss;
                } Fields;
        } TcpSegSetup;

} E1000_TCPIP_CONTEXT_TRANSMIT_DESCRIPTOR,

    *PE1000_TCPIP_CONTEXT_TRANSMIT_DESCRIPTOR;

typedef struct _E1000_TCPIP_DATA_TRANSMIT_DESCRIPTOR {
        E1000_64_BIT_PHYSICAL_ADDRESS BufferAddress;

        union {
                u32 DwordData;
                struct _TXD_OD_FLAGS {
                        u16 Length;
                        u8 TypLenExt;
                        u8 Cmd;
                } Flags;
        } Lower;

        union {
                u32 DwordData;
                struct _TXD_OD_FIELDS {
                        u8 TransmitStatus;
                        u8 Popts;
                        u16 Special;
                } Fields;
        } Upper;

} E1000_TCPIP_DATA_TRANSMIT_DESCRIPTOR,

    *PE1000_TCPIP_DATA_TRANSMIT_DESCRIPTOR;

#define E1000_TXD_CMD_EOP   (0x01000000)
#define E1000_TXD_CMD_IFCS  (0x02000000)

#define E1000_TXD_CMD_IC    (0x04000000)

#define E1000_TXD_CMD_RS    (0x08000000)
#define E1000_TXD_CMD_RPS   (0x10000000)

#define E1000_TXD_CMD_DEXT  (0x20000000)
#define E1000_TXD_CMD_ISLVE (0x40000000)

#define E1000_TXD_CMD_IDE   (0x80000000)

#define E1000_TXD_STAT_DD   (0x00000001)
#define E1000_TXD_STAT_EC   (0x00000002)
#define E1000_TXD_STAT_LC   (0x00000004)
#define E1000_TXD_STAT_TU   (0x00000008)

#define E1000_TXD_CMD_TCP   (0x01000000)
#define E1000_TXD_CMD_IP    (0x02000000)
#define E1000_TXD_CMD_TSE   (0x04000000)

#define E1000_TXD_STAT_TC   (0x00000004)

#define E1000_NUM_UNICAST          (16)
#define E1000_MC_TBL_SIZE          (128)

#define E1000_VLAN_FILTER_TBL_SIZE (128)

typedef struct {
        volatile u32 Low;
        volatile u32 High;
} RECEIVE_ADDRESS_REGISTER_PAIR;

#define E1000_NUM_MTA_REGISTERS 128

typedef struct {
        volatile u32 IpAddress;
        volatile u32 Reserved;
} IPAT_ENTRY;

#define E1000_WAKEUP_IP_ADDRESS_COUNT_MAX   (4)
#define E1000_IPAT_SIZE                     E1000_WAKEUP_IP_ADDRESS_COUNT_MAX

typedef struct {
        volatile u32 Length;
        volatile u32 Reserved;
} FFLT_ENTRY;

typedef struct {
        volatile u32 Mask;
        volatile u32 Reserved;
} FFMT_ENTRY;

typedef struct {
        volatile u32 Value;
        volatile u32 Reserved;
} FFVT_ENTRY;

#define E1000_FLEXIBLE_FILTER_COUNT_MAX     (4)

#define E1000_FLEXIBLE_FILTER_SIZE_MAX      (128)

#define E1000_FFLT_SIZE                     E1000_FLEXIBLE_FILTER_COUNT_MAX
#define E1000_FFMT_SIZE                     E1000_FLEXIBLE_FILTER_SIZE_MAX
#define E1000_FFVT_SIZE                     E1000_FLEXIBLE_FILTER_SIZE_MAX

typedef struct _E1000_REGISTERS {

        volatile u32 Ctrl;
        volatile u32 Pad1;
        volatile u32 Status;
        volatile u32 Pad2;
        volatile u32 Eecd;
        volatile u32 Pad3;
        volatile u32 Exct;
        volatile u32 Pad4;
        volatile u32 Mdic;
        volatile u32 Pad5;
        volatile u32 Fcal;
        volatile u32 Fcah;
        volatile u32 Fct;
        volatile u32 Pad6;

        volatile u32 Vet;
        volatile u32 Pad7;

        RECEIVE_ADDRESS_REGISTER_PAIR Rar[16];

        volatile u32 Icr;
        volatile u32 Pad8;
        volatile u32 Ics;
        volatile u32 Pad9;
        volatile u32 Ims;
        volatile u32 Pad10;
        volatile u32 Imc;
        volatile u8 Pad11[0x24];

        volatile u32 Rctl;
        volatile u32 Pad12;
        volatile u32 PadRdtr0;
        volatile u32 Pad13;
        volatile u32 PadRdbal0;
        volatile u32 PadRdbah0;
        volatile u32 PadRdlen0;
        volatile u32 Pad14;
        volatile u32 PadRdh0;
        volatile u32 Pad15;
        volatile u32 PadRdt0;
        volatile u32 Pad16;
        volatile u32 Rdtr1;
        volatile u32 Pad17;
        volatile u32 Rdbal1;
        volatile u32 Rdbah1;
        volatile u32 Rdlen1;
        volatile u32 Pad18;
        volatile u32 Rdh1;
        volatile u32 Pad19;
        volatile u32 Rdt1;
        volatile u8 Pad20[0x0C];
        volatile u32 PadFcrth;
        volatile u32 Pad21;
        volatile u32 PadFcrtl;
        volatile u32 Pad22;
        volatile u32 Fcttv;
        volatile u32 Pad23;
        volatile u32 Txcw;
        volatile u32 Pad24;
        volatile u32 Rxcw;
        volatile u8 Pad25[0x7C];
        volatile u32 Mta[(128)];

        volatile u32 Tctl;
        volatile u32 Pad26;
        volatile u32 Tqsal;
        volatile u32 Tqsah;
        volatile u32 Tipg;
        volatile u32 Pad27;
        volatile u32 Tqc;
        volatile u32 Pad28;
        volatile u32 PadTdbal;
        volatile u32 PadTdbah;
        volatile u32 PadTdl;
        volatile u32 Pad29;
        volatile u32 PadTdh;
        volatile u32 Pad30;
        volatile u32 PadTdt;
        volatile u32 Pad31;
        volatile u32 PadTidv;
        volatile u32 Pad32;
        volatile u32 Tbt;
        volatile u8 Pad33[0x0C];

        volatile u32 Ait;
        volatile u8 Pad34[0xA4];

        volatile u32 Ftr[8];
        volatile u32 Fcr;
        volatile u32 Pad35;
        volatile u32 Trcr;

        volatile u8 Pad36[0xD4];

        volatile u32 Vfta[(128)];
        volatile u8 Pad37[0x700];
        volatile u32 Circ;
        volatile u8 Pad37a[0xFC];

        volatile u32 Pba;
        volatile u8 Pad38[0xFFC];

        volatile u8 Pad39[0x8];
        volatile u32 Ert;
        volatile u8 Pad40[0xf4];

        volatile u8 Pad41[0x60];
        volatile u32 Fcrtl;
        volatile u32 Pad42;
        volatile u32 Fcrth;
        volatile u8 Pad43[0x294];

        volatile u8 Pad44[0x10];
        volatile u32 Rdfh;
        volatile u32 Pad45;
        volatile u32 Rdft;
        volatile u32 Pad45a;
        volatile u32 Rdfhs;
        volatile u32 Pad45b;
        volatile u32 Rdfts;
        volatile u32 Pad45c;
        volatile u32 Rdfpc;
        volatile u8 Pad46[0x3cc];

        volatile u32 Rdbal0;
        volatile u32 Rdbah0;
        volatile u32 Rdlen0;
        volatile u32 Pad47;
        volatile u32 Rdh0;
        volatile u32 Pad48;
        volatile u32 Rdt0;
        volatile u32 Pad49;
        volatile u32 Rdtr0;
        volatile u32 Pad50;
        volatile u32 Rxdctl;
        volatile u32 Pad51;
        volatile u32 Rddh0;
        volatile u32 Pad52;
        volatile u32 Rddt0;
        volatile u8 Pad53[0x7C4];

        volatile u32 Txdmac;
        volatile u32 Pad54;
        volatile u32 Ett;
        volatile u8 Pad55[0x3f4];

        volatile u8 Pad56[0x10];
        volatile u32 Tdfh;
        volatile u32 Pad57;
        volatile u32 Tdft;
        volatile u32 Pad57a;
        volatile u32 Tdfhs;
        volatile u32 Pad57b;
        volatile u32 Tdfts;
        volatile u32 Pad57c;
        volatile u32 Tdfpc;
        volatile u8 Pad58[0x3cc];

        volatile u32 Tdbal;
        volatile u32 Tdbah;
        volatile u32 Tdl;
        volatile u32 Pad59;
        volatile u32 Tdh;
        volatile u32 Pad60;
        volatile u32 Tdt;
        volatile u32 Pad61;
        volatile u32 Tidv;
        volatile u32 Pad62;
        volatile u32 Txdctl;
        volatile u32 Pad63;
        volatile u32 Tddh;
        volatile u32 Pad64;
        volatile u32 Tddt;
        volatile u8 Pad65[0x7C4];

        volatile u32 Crcerrs;
        volatile u32 Algnerrc;
        volatile u32 Symerrs;
        volatile u32 Rxerrc;
        volatile u32 Mpc;
        volatile u32 Scc;
        volatile u32 Ecol;
        volatile u32 Mcc;
        volatile u32 Latecol;
        volatile u32 Pad66;
        volatile u32 Colc;
        volatile u32 Tuc;
        volatile u32 Dc;
        volatile u32 Tncrs;
        volatile u32 Sec;
        volatile u32 Cexterr;
        volatile u32 Rlec;
        volatile u32 Rutec;
        volatile u32 Xonrxc;
        volatile u32 Xontxc;
        volatile u32 Xoffrxc;
        volatile u32 Xofftxc;
        volatile u32 Fcruc;
        volatile u32 Prc64;
        volatile u32 Prc127;
        volatile u32 Prc255;
        volatile u32 Prc511;
        volatile u32 Prc1023;
        volatile u32 Prc1522;
        volatile u32 Gprc;
        volatile u32 Bprc;
        volatile u32 Mprc;
        volatile u32 Gptc;
        volatile u32 Pad67;
        volatile u32 Gorl;
        volatile u32 Gorh;
        volatile u32 Gotl;
        volatile u32 Goth;
        volatile u8 Pad68[8];
        volatile u32 Rnbc;
        volatile u32 Ruc;
        volatile u32 Rfc;
        volatile u32 Roc;
        volatile u32 Rjc;
        volatile u8 Pad69[0xC];
        volatile u32 Torl;
        volatile u32 Torh;
        volatile u32 Totl;
        volatile u32 Toth;
        volatile u32 Tpr;
        volatile u32 Tpt;
        volatile u32 Ptc64;
        volatile u32 Ptc127;
        volatile u32 Ptc255;
        volatile u32 Ptc511;
        volatile u32 Ptc1023;
        volatile u32 Ptc1522;
        volatile u32 Mptc;
        volatile u32 Bptc;

        volatile u32 Tsctc;
        volatile u32 Tsctfc;
        volatile u8 Pad70[0x0F00];

        volatile u32 Rxcsum;
        volatile u8 Pad71[0x07FC];

        volatile u32 Wuc;
        volatile u32 Pad72;
        volatile u32 Wufc;
        volatile u32 Pad73;
        volatile u32 Wus;
        volatile u8 Pad74[0x24];
        volatile u32 Ipav;
        volatile u32 Pad75;
        IPAT_ENTRY Ipat[(4)];
        volatile u8 Pad76[0xA0];
        volatile u32 Wupl;
        volatile u8 Pad77[0xFC];
        volatile u8 Wupm[0x80];
        volatile u8 Pad78[0x480];
        FFLT_ENTRY Fflt[(4)];
        volatile u8 Pad79[0x20E0];

        volatile u32 PadRdfh;
        volatile u32 Pad80;
        volatile u32 PadRdft;
        volatile u32 Pad81;
        volatile u32 PadTdfh;
        volatile u32 Pad82;
        volatile u32 PadTdft;
        volatile u8 Pad83[0xFE4];

        FFMT_ENTRY Ffmt[(128)];
        volatile u8 Pad84[0x0400];
        FFVT_ENTRY Ffvt[(128)];

        volatile u8 Pad85[0x6400];

        volatile u32 Pbm[0x4000];

} E1000_REGISTERS, *PE1000_REGISTERS;

typedef struct _OLD_REGISTERS {

        volatile u32 Ctrl;
        volatile u32 Pad1;
        volatile u32 Status;
        volatile u32 Pad2;
        volatile u32 Eecd;
        volatile u32 Pad3;
        volatile u32 Exct;
        volatile u32 Pad4;
        volatile u32 Mdic;
        volatile u32 Pad5;
        volatile u32 Fcal;
        volatile u32 Fcah;
        volatile u32 Fct;
        volatile u32 Pad6;

        volatile u32 Vet;
        volatile u32 Pad7;

        RECEIVE_ADDRESS_REGISTER_PAIR Rar[16];

        volatile u32 Icr;
        volatile u32 Pad8;
        volatile u32 Ics;
        volatile u32 Pad9;
        volatile u32 Ims;
        volatile u32 Pad10;
        volatile u32 Imc;
        volatile u8 Pad11[0x24];

        volatile u32 Rctl;
        volatile u32 Pad12;
        volatile u32 Rdtr0;
        volatile u32 Pad13;
        volatile u32 Rdbal0;
        volatile u32 Rdbah0;
        volatile u32 Rdlen0;
        volatile u32 Pad14;
        volatile u32 Rdh0;
        volatile u32 Pad15;
        volatile u32 Rdt0;
        volatile u32 Pad16;
        volatile u32 Rdtr1;
        volatile u32 Pad17;
        volatile u32 Rdbal1;
        volatile u32 Rdbah1;
        volatile u32 Rdlen1;
        volatile u32 Pad18;
        volatile u32 Rdh1;
        volatile u32 Pad19;
        volatile u32 Rdt1;
        volatile u8 Pad20[0x0C];
        volatile u32 Fcrth;
        volatile u32 Pad21;
        volatile u32 Fcrtl;
        volatile u32 Pad22;
        volatile u32 Fcttv;
        volatile u32 Pad23;
        volatile u32 Txcw;
        volatile u32 Pad24;
        volatile u32 Rxcw;
        volatile u8 Pad25[0x7C];
        volatile u32 Mta[(128)];

        volatile u32 Tctl;
        volatile u32 Pad26;
        volatile u32 Tqsal;
        volatile u32 Tqsah;
        volatile u32 Tipg;
        volatile u32 Pad27;
        volatile u32 Tqc;
        volatile u32 Pad28;
        volatile u32 Tdbal;
        volatile u32 Tdbah;
        volatile u32 Tdl;
        volatile u32 Pad29;
        volatile u32 Tdh;
        volatile u32 Pad30;
        volatile u32 Tdt;
        volatile u32 Pad31;
        volatile u32 Tidv;
        volatile u32 Pad32;
        volatile u32 Tbt;
        volatile u8 Pad33[0x0C];

        volatile u32 Ait;
        volatile u8 Pad34[0xA4];

        volatile u32 Ftr[8];
        volatile u32 Fcr;
        volatile u32 Pad35;
        volatile u32 Trcr;

        volatile u8 Pad36[0xD4];

        volatile u32 Vfta[(128)];
        volatile u8 Pad37[0x700];
        volatile u32 Circ;
        volatile u8 Pad37a[0xFC];

        volatile u32 Pba;
        volatile u8 Pad38[0xFFC];

        volatile u8 Pad39[0x8];
        volatile u32 Ert;
        volatile u8 Pad40[0x1C];
        volatile u32 Rxdctl;
        volatile u8 Pad41[0xFD4];

        volatile u32 Txdmac;
        volatile u32 Pad42;
        volatile u32 Ett;
        volatile u8 Pad43[0x1C];
        volatile u32 Txdctl;
        volatile u8 Pad44[0xFD4];

        volatile u32 Crcerrs;
        volatile u32 Algnerrc;
        volatile u32 Symerrs;
        volatile u32 Rxerrc;
        volatile u32 Mpc;
        volatile u32 Scc;
        volatile u32 Ecol;
        volatile u32 Mcc;
        volatile u32 Latecol;
        volatile u32 Pad45;
        volatile u32 Colc;
        volatile u32 Tuc;
        volatile u32 Dc;
        volatile u32 Tncrs;
        volatile u32 Sec;
        volatile u32 Cexterr;
        volatile u32 Rlec;
        volatile u32 Rutec;
        volatile u32 Xonrxc;
        volatile u32 Xontxc;
        volatile u32 Xoffrxc;
        volatile u32 Xofftxc;
        volatile u32 Fcruc;
        volatile u32 Prc64;
        volatile u32 Prc127;
        volatile u32 Prc255;
        volatile u32 Prc511;
        volatile u32 Prc1023;
        volatile u32 Prc1522;
        volatile u32 Gprc;
        volatile u32 Bprc;
        volatile u32 Mprc;
        volatile u32 Gptc;
        volatile u32 Pad46;
        volatile u32 Gorl;
        volatile u32 Gorh;
        volatile u32 Gotl;
        volatile u32 Goth;
        volatile u8 Pad47[8];
        volatile u32 Rnbc;
        volatile u32 Ruc;
        volatile u32 Rfc;
        volatile u32 Roc;
        volatile u32 Rjc;
        volatile u8 Pad48[0xC];
        volatile u32 Torl;
        volatile u32 Torh;
        volatile u32 Totl;
        volatile u32 Toth;
        volatile u32 Tpr;
        volatile u32 Tpt;
        volatile u32 Ptc64;
        volatile u32 Ptc127;
        volatile u32 Ptc255;
        volatile u32 Ptc511;
        volatile u32 Ptc1023;
        volatile u32 Ptc1522;
        volatile u32 Mptc;
        volatile u32 Bptc;

        volatile u32 Tsctc;
        volatile u32 Tsctfc;
        volatile u8 Pad49[0x0F00];

        volatile u32 Rxcsum;
        volatile u8 Pad50[0x07FC];

        volatile u32 Wuc;
        volatile u32 Pad51;
        volatile u32 Wufc;
        volatile u32 Pad52;
        volatile u32 Wus;
        volatile u8 Pad53[0x24];
        volatile u32 Ipav;
        volatile u32 Pad54;
        IPAT_ENTRY Ipat[(4)];
        volatile u8 Pad55[0xA0];
        volatile u32 Wupl;
        volatile u8 Pad56[0xFC];
        volatile u8 Wupm[0x80];
        volatile u8 Pad57[0x480];
        FFLT_ENTRY Fflt[(4)];
        volatile u8 Pad58[0x20E0];

        volatile u32 Rdfh;
        volatile u32 Pad59;
        volatile u32 Rdft;
        volatile u32 Pad60;
        volatile u32 Tdfh;
        volatile u32 Pad61;
        volatile u32 Tdft;
        volatile u32 Pad62;
        volatile u32 Tdfhs;
        volatile u32 Pad63;
        volatile u32 Tdfts;
        volatile u32 Pad64;
        volatile u32 Tdfpc;
        volatile u8 Pad65[0x0FCC];

        FFMT_ENTRY Ffmt[(128)];
        volatile u8 Pad66[0x0400];
        FFVT_ENTRY Ffvt[(128)];

        volatile u8 Pad67[0x6400];

        volatile u32 Pbm[0x4000];

} OLD_REGISTERS, *POLD_REGISTERS;

#define E1000_EEPROM_SWDPIN0       (0x00000001)
#define E1000_EEPROM_LED_LOGIC     (0x0020)

#define E1000_CTRL_FD              (0x00000001)
#define E1000_CTRL_BEM             (0x00000002)
#define E1000_CTRL_PRIOR           (0x00000004)
#define E1000_CTRL_LRST            (0x00000008)
#define E1000_CTRL_TME             (0x00000010)
#define E1000_CTRL_SLE             (0x00000020)
#define E1000_CTRL_ASDE            (0x00000020)
#define E1000_CTRL_SLU             (0x00000040)

#define E1000_CTRL_ILOS            (0x00000080)
#define E1000_CTRL_SPD_SEL         (0x00000300)
#define E1000_CTRL_SPD_10          (0x00000000)
#define E1000_CTRL_SPD_100         (0x00000100)
#define E1000_CTRL_SPD_1000        (0x00000200)
#define E1000_CTRL_BEM32           (0x00000400)
#define E1000_CTRL_FRCSPD          (0x00000800)
#define E1000_CTRL_FRCDPX          (0x00001000)

#define E1000_CTRL_SWDPIN0         (0x00040000)
#define E1000_CTRL_SWDPIN1         (0x00080000)
#define E1000_CTRL_SWDPIN2         (0x00100000)
#define E1000_CTRL_SWDPIN3         (0x00200000)
#define E1000_CTRL_SWDPIO0         (0x00400000)
#define E1000_CTRL_SWDPIO1         (0x00800000)
#define E1000_CTRL_SWDPIO2         (0x01000000)
#define E1000_CTRL_SWDPIO3         (0x02000000)
#define E1000_CTRL_RST             (0x04000000)
#define E1000_CTRL_RFCE            (0x08000000)
#define E1000_CTRL_TFCE            (0x10000000)

#define E1000_CTRL_RTE             (0x20000000)
#define E1000_CTRL_VME             (0x40000000)

#define E1000_CTRL_PHY_RST         (0x80000000)

#define E1000_STATUS_FD            (0x00000001)
#define E1000_STATUS_LU            (0x00000002)
#define E1000_STATUS_TCKOK         (0x00000004)
#define E1000_STATUS_RBCOK         (0x00000008)
#define E1000_STATUS_TXOFF         (0x00000010)
#define E1000_STATUS_TBIMODE       (0x00000020)
#define E1000_STATUS_SPEED_10      (0x00000000)
#define E1000_STATUS_SPEED_100     (0x00000040)
#define E1000_STATUS_SPEED_1000    (0x00000080)
#define E1000_STATUS_ASDV          (0x00000300)
#define E1000_STATUS_MTXCKOK       (0x00000400)
#define E1000_STATUS_PCI66         (0x00000800)
#define E1000_STATUS_BUS64         (0x00001000)
#define E1000_STATUS_PCIX_MODE     (0x00002000)
#define E1000_STATUS_PCIX_SPEED    (0x0000C000)

#define E1000_STATUS_PCIX_SPEED_66  (0x00000000)
#define E1000_STATUS_PCIX_SPEED_100 (0x00004000)
#define E1000_STATUS_PCIX_SPEED_133 (0x00008000)

#define E1000_EESK                 (0x00000001)
#define E1000_EECS                 (0x00000002)
#define E1000_EEDI                 (0x00000004)
#define E1000_EEDO                 (0x00000008)
#define E1000_FLASH_WRITE_DIS      (0x00000010)
#define E1000_FLASH_WRITE_EN       (0x00000020)

#define E1000_EXCTRL_GPI_EN0       (0x00000001)
#define E1000_EXCTRL_GPI_EN1       (0x00000002)
#define E1000_EXCTRL_GPI_EN2       (0x00000004)
#define E1000_EXCTRL_GPI_EN3       (0x00000008)
#define E1000_EXCTRL_SWDPIN4       (0x00000010)
#define E1000_EXCTRL_SWDPIN5       (0x00000020)
#define E1000_EXCTRL_SWDPIN6       (0x00000040)
#define E1000_EXCTRL_SWDPIN7       (0x00000080)
#define E1000_EXCTRL_SWDPIO4       (0x00000100)
#define E1000_EXCTRL_SWDPIO5       (0x00000200)
#define E1000_EXCTRL_SWDPIO6       (0x00000400)
#define E1000_EXCTRL_SWDPIO7       (0x00000800)
#define E1000_EXCTRL_ASDCHK        (0x00001000)
#define E1000_EXCTRL_EE_RST        (0x00002000)
#define E1000_EXCTRL_IPS           (0x00004000)
#define E1000_EXCTRL_SPD_BYPS      (0x00008000)

#define E1000_MDI_WRITE            (0x04000000)
#define E1000_MDI_READ             (0x08000000)
#define E1000_MDI_READY            (0x10000000)
#define E1000_MDI_INT              (0x20000000)
#define E1000_MDI_ERR              (0x40000000)

#define E1000_RAH_RDR              (0x40000000)
#define E1000_RAH_AV               (0x80000000)

#define E1000_ICR_TXDW             (0x00000001)
#define E1000_ICR_TXQE             (0x00000002)
#define E1000_ICR_LSC              (0x00000004)
#define E1000_ICR_RXSEQ            (0x00000008)
#define E1000_ICR_RXDMT0           (0x00000010)
#define E1000_ICR_RXDMT1           (0x00000020)
#define E1000_ICR_RXO              (0x00000040)
#define E1000_ICR_RXT0             (0x00000080)
#define E1000_ICR_RXT1             (0x00000100)
#define E1000_ICR_MDAC             (0x00000200)
#define E1000_ICR_RXCFG            (0x00000400)
#define E1000_ICR_GPI_EN0          (0x00000800)
#define E1000_ICR_GPI_EN1          (0x00001000)
#define E1000_ICR_GPI_EN2          (0x00002000)
#define E1000_ICR_GPI_EN3          (0x00004000)

#define E1000_ICS_TXDW             E1000_ICR_TXDW
#define E1000_ICS_TXQE             E1000_ICR_TXQE
#define E1000_ICS_LSC              E1000_ICR_LSC
#define E1000_ICS_RXSEQ            E1000_ICR_RXSEQ
#define E1000_ICS_RXDMT0           E1000_ICR_RXDMT0
#define E1000_ICS_RXDMT1           E1000_ICR_RXDMT1
#define E1000_ICS_RXO              E1000_ICR_RXO
#define E1000_ICS_RXT0             E1000_ICR_RXT0
#define E1000_ICS_RXT1             E1000_ICR_RXT1
#define E1000_ICS_MDAC             E1000_ICR_MDAC
#define E1000_ICS_RXCFG            E1000_ICR_RXCFG
#define E1000_ICS_GPI_EN0          E1000_ICR_GPI_EN0
#define E1000_ICS_GPI_EN1          E1000_ICR_GPI_EN1
#define E1000_ICS_GPI_EN2          E1000_ICR_GPI_EN2
#define E1000_ICS_GPI_EN3          E1000_ICR_GPI_EN3

#define E1000_IMS_TXDW             E1000_ICR_TXDW
#define E1000_IMS_TXQE             E1000_ICR_TXQE
#define E1000_IMS_LSC              E1000_ICR_LSC
#define E1000_IMS_RXSEQ            E1000_ICR_RXSEQ
#define E1000_IMS_RXDMT0           E1000_ICR_RXDMT0
#define E1000_IMS_RXDMT1           E1000_ICR_RXDMT1
#define E1000_IMS_RXO              E1000_ICR_RXO
#define E1000_IMS_RXT0             E1000_ICR_RXT0
#define E1000_IMS_RXT1             E1000_ICR_RXT1
#define E1000_IMS_MDAC             E1000_ICR_MDAC
#define E1000_IMS_RXCFG            E1000_ICR_RXCFG
#define E1000_IMS_GPI_EN0          E1000_ICR_GPI_EN0
#define E1000_IMS_GPI_EN1          E1000_ICR_GPI_EN1
#define E1000_IMS_GPI_EN2          E1000_ICR_GPI_EN2
#define E1000_IMS_GPI_EN3          E1000_ICR_GPI_EN3

#define E1000_IMC_TXDW             E1000_ICR_TXDW
#define E1000_IMC_TXQE             E1000_ICR_TXQE
#define E1000_IMC_LSC              E1000_ICR_LSC
#define E1000_IMC_RXSEQ            E1000_ICR_RXSEQ
#define E1000_IMC_RXDMT0           E1000_ICR_RXDMT0
#define E1000_IMC_RXDMT1           E1000_ICR_RXDMT1
#define E1000_IMC_RXO              E1000_ICR_RXO
#define E1000_IMC_RXT0             E1000_ICR_RXT0
#define E1000_IMC_RXT1             E1000_ICR_RXT1
#define E1000_IMC_MDAC             E1000_ICR_MDAC
#define E1000_IMC_RXCFG            E1000_ICR_RXCFG
#define E1000_IMC_GPI_EN0          E1000_ICR_GPI_EN0
#define E1000_IMC_GPI_EN1          E1000_ICR_GPI_EN1
#define E1000_IMC_GPI_EN2          E1000_ICR_GPI_EN2
#define E1000_IMC_GPI_EN3          E1000_ICR_GPI_EN3

#define E1000_TINT_RINT_PCI        (E1000_TXDW|E1000_ICR_RXT0)
#define E1000_CAUSE_ERR            (E1000_ICR_RXSEQ|E1000_ICR_RXO)

#define E1000_RCTL_RST             (0x00000001)
#define E1000_RCTL_EN              (0x00000002)
#define E1000_RCTL_SBP             (0x00000004)
#define E1000_RCTL_UPE             (0x00000008)
#define E1000_RCTL_MPE             (0x00000010)
#define E1000_RCTL_LPE             (0x00000020)
#define E1000_RCTL_LBM_NO          (0x00000000)
#define E1000_RCTL_LBM_MAC         (0x00000040)
#define E1000_RCTL_LBM_SLP         (0x00000080)
#define E1000_RCTL_LBM_TCVR        (0x000000c0)
#define E1000_RCTL_RDMTS0_HALF     (0x00000000)
#define E1000_RCTL_RDMTS0_QUAT     (0x00000100)
#define E1000_RCTL_RDMTS0_EIGTH    (0x00000200)
#define E1000_RCTL_RDMTS1_HALF     (0x00000000)
#define E1000_RCTL_RDMTS1_QUAT     (0x00000400)
#define E1000_RCTL_RDMTS1_EIGTH    (0x00000800)
#define E1000_RCTL_MO_SHIFT        12

#define E1000_RCTL_MO_0            (0x00000000)
#define E1000_RCTL_MO_1            (0x00001000)
#define E1000_RCTL_MO_2            (0x00002000)
#define E1000_RCTL_MO_3            (0x00003000)

#define E1000_RCTL_MDR             (0x00004000)
#define E1000_RCTL_BAM             (0x00008000)

#define E1000_RCTL_SZ_2048         (0x00000000)
#define E1000_RCTL_SZ_1024         (0x00010000)
#define E1000_RCTL_SZ_512          (0x00020000)
#define E1000_RCTL_SZ_256          (0x00030000)

#define E1000_RCTL_SZ_16384        (0x00010000)
#define E1000_RCTL_SZ_8192         (0x00020000)
#define E1000_RCTL_SZ_4096         (0x00030000)

#define E1000_RCTL_VFE             (0x00040000)

#define E1000_RCTL_CFIEN           (0x00080000)
#define E1000_RCTL_CFI             (0x00100000)
#define E1000_RCTL_ISLE            (0x00200000)

#define E1000_RCTL_DPF             (0x00400000)
#define E1000_RCTL_PMCF            (0x00800000)

#define E1000_RCTL_SISLH           (0x01000000)

#define E1000_RCTL_BSEX            (0x02000000)
#define E1000_RDT0_DELAY           (0x0000ffff)
#define E1000_RDT0_FPDB            (0x80000000)

#define E1000_RDT1_DELAY           (0x0000ffff)
#define E1000_RDT1_FPDB            (0x80000000)

#define E1000_RDLEN0_LEN           (0x0007ff80)

#define E1000_RDLEN1_LEN           (0x0007ff80)

#define E1000_RDH0_RDH             (0x0000ffff)

#define E1000_RDH1_RDH             (0x0000ffff)

#define E1000_RDT0_RDT             (0x0000ffff)

#define E1000_FCRTH_RTH            (0x0000FFF8)
#define E1000_FCRTH_XFCE           (0x80000000)

#define E1000_FCRTL_RTL            (0x0000FFF8)
#define E1000_FCRTL_XONE           (0x80000000)

#define E1000_RXDCTL_PTHRESH       0x0000003F
#define E1000_RXDCTL_HTHRESH       0x00003F00
#define E1000_RXDCTL_WTHRESH       0x003F0000
#define E1000_RXDCTL_GRAN          0x01000000

#define E1000_TXDCTL_PTHRESH       0x000000FF
#define E1000_TXDCTL_HTHRESH       0x0000FF00
#define E1000_TXDCTL_WTHRESH       0x00FF0000
#define E1000_TXDCTL_GRAN          0x01000000

#define E1000_TXCW_FD              (0x00000020)
#define E1000_TXCW_HD              (0x00000040)
#define E1000_TXCW_PAUSE           (0x00000080)
#define E1000_TXCW_ASM_DIR         (0x00000100)
#define E1000_TXCW_PAUSE_MASK      (0x00000180)
#define E1000_TXCW_RF              (0x00003000)
#define E1000_TXCW_NP              (0x00008000)
#define E1000_TXCW_CW              (0x0000ffff)
#define E1000_TXCW_TXC             (0x40000000)
#define E1000_TXCW_ANE             (0x80000000)

#define E1000_RXCW_CW              (0x0000ffff)
#define E1000_RXCW_NC              (0x04000000)
#define E1000_RXCW_IV              (0x08000000)
#define E1000_RXCW_CC              (0x10000000)
#define E1000_RXCW_C               (0x20000000)
#define E1000_RXCW_SYNCH           (0x40000000)
#define E1000_RXCW_ANC             (0x80000000)

#define E1000_TCTL_RST             (0x00000001)
#define E1000_TCTL_EN              (0x00000002)
#define E1000_TCTL_BCE             (0x00000004)
#define E1000_TCTL_PSP             (0x00000008)
#define E1000_TCTL_CT              (0x00000ff0)
#define E1000_TCTL_COLD            (0x003ff000)
#define E1000_TCTL_SWXOFF          (0x00400000)
#define E1000_TCTL_PBE             (0x00800000)
#define E1000_TCTL_RTLC            (0x01000000)
#define E1000_TCTL_NRTU            (0x02000000)

#define E1000_TQSAL_TQSAL          (0xffffffc0)
#define E1000_TQSAH_TQSAH          (0xffffffff)

#define E1000_TQC_SQ               (0x00000001)
#define E1000_TQC_RQ               (0x00000002)

#define E1000_TDBAL_TDBAL          (0xfffff000)
#define E1000_TDBAH_TDBAH          (0xffffffff)

#define E1000_TDL_LEN              (0x0007ff80)

#define E1000_TDH_TDH              (0x0000ffff)

#define E1000_TDT_TDT              (0x0000ffff)

#define E1000_RXCSUM_PCSS          (0x000000ff)
#define E1000_RXCSUM_IPOFL         (0x00000100)
#define E1000_RXCSUM_TUOFL         (0x00000200)

#define E1000_WUC_APME             (0x00000001)
#define E1000_WUC_PME_EN           (0x00000002)
#define E1000_WUC_PME_STATUS       (0x00000004)
#define E1000_WUC_APMPME           (0x00000008)

#define E1000_WUFC_LNKC            (0x00000001)
#define E1000_WUFC_MAG             (0x00000002)
#define E1000_WUFC_EX              (0x00000004)
#define E1000_WUFC_MC              (0x00000008)
#define E1000_WUFC_BC              (0x00000010)
#define E1000_WUFC_ARP             (0x00000020)
#define E1000_WUFC_IP              (0x00000040)
#define E1000_WUFC_FLX0            (0x00010000)
#define E1000_WUFC_FLX1            (0x00020000)
#define E1000_WUFC_FLX2            (0x00040000)
#define E1000_WUFC_FLX3            (0x00080000)
#define E1000_WUFC_ALL_FILTERS     (0x000F007F)

#define E1000_WUFC_FLX_OFFSET      (16)
#define E1000_WUFC_FLX_FILTERS     (0x000F0000)

#define E1000_WUS_LNKC             (0x00000001)
#define E1000_WUS_MAG              (0x00000002)
#define E1000_WUS_EX               (0x00000004)
#define E1000_WUS_MC               (0x00000008)
#define E1000_WUS_BC               (0x00000010)
#define E1000_WUS_ARP              (0x00000020)
#define E1000_WUS_IP               (0x00000040)
#define E1000_WUS_FLX0             (0x00010000)
#define E1000_WUS_FLX1             (0x00020000)
#define E1000_WUS_FLX2             (0x00040000)
#define E1000_WUS_FLX3             (0x00080000)
#define E1000_WUS_FLX_FILTERS      (0x000F0000)

#define E1000_WUPL_LENGTH_MASK     (0x0FFF)

#define E1000_MDALIGN               (4096)

#define EEPROM_READ_OPCODE          (0x6)
#define EEPROM_WRITE_OPCODE         (0x5)
#define EEPROM_ERASE_OPCODE         (0x7)
#define EEPROM_EWEN_OPCODE          (0x13)
#define EEPROM_EWDS_OPCODE          (0x10)

#define EEPROM_INIT_CONTROL1_REG    (0x000A)
#define EEPROM_INIT_CONTROL2_REG    (0x000F)
#define EEPROM_CHECKSUM_REG         (0x003F)

#define EEPROM_WORD0A_ILOS          (0x0010)
#define EEPROM_WORD0A_SWDPIO        (0x01E0)
#define EEPROM_WORD0A_LRST          (0x0200)
#define EEPROM_WORD0A_FD            (0x0400)
#define EEPROM_WORD0A_66MHZ         (0x0800)

#define EEPROM_WORD0F_PAUSE_MASK    (0x3000)
#define EEPROM_WORD0F_PAUSE         (0x1000)
#define EEPROM_WORD0F_ASM_DIR       (0x2000)
#define EEPROM_WORD0F_ANE           (0x0800)
#define EEPROM_WORD0F_SWPDIO_EXT    (0x00F0)

#define EEPROM_SUM                  (0xBABA)

#define EEPROM_NODE_ADDRESS_BYTE_0  (0)
#define EEPROM_PBA_BYTE_1           (8)

#define EEPROM_WORD_SIZE            (64)

#define NODE_ADDRESS_SIZE           (6)
#define PBA_SIZE                    (4)

#define E1000_COLLISION_THRESHOLD   16
#define E1000_CT_SHIFT              4

#define E1000_FDX_COLLISION_DISTANCE 64
#define E1000_HDX_COLLISION_DISTANCE 64
#define E1000_GB_HDX_COLLISION_DISTANCE 512
#define E1000_COLD_SHIFT            12

#define REQ_TX_DESCRIPTOR_MULTIPLE  8
#define REQ_RX_DESCRIPTOR_MULTIPLE  8

#define DEFAULT_WSMN_TIPG_IPGT        10
#define DEFAULT_LVGD_TIPG_IPGT_FIBER  6
#define DEFAULT_LVGD_TIPG_IPGT_COPPER 8

#define E1000_TIPG_IPGT_MASK        0x000003FF
#define E1000_TIPG_IPGR1_MASK       0x000FFC00
#define E1000_TIPG_IPGR2_MASK       0x3FF00000

#define DEFAULT_WSMN_TIPG_IPGR1     2
#define DEFAULT_LVGD_TIPG_IPGR1     8
#define E1000_TIPG_IPGR1_SHIFT      10

#define DEFAULT_WSMN_TIPG_IPGR2     10
#define DEFAULT_LVGD_TIPG_IPGR2     6
#define E1000_TIPG_IPGR2_SHIFT      20

#define E1000_TXDMAC_DPP            0x00000001

#define E1000_PBA_16K               (0x0010)
#define E1000_PBA_24K               (0x0018)
#define E1000_PBA_40K               (0x0028)
#define E1000_PBA_48K               (0x0030)

#define FLOW_CONTROL_ADDRESS_LOW    (0x00C28001)
#define FLOW_CONTROL_ADDRESS_HIGH   (0x00000100)
#define FLOW_CONTROL_TYPE           (0x8808)

#define FC_DEFAULT_HI_THRESH        (0x8000)
#define FC_DEFAULT_LO_THRESH        (0x4000)
#define FC_DEFAULT_TX_TIMER         (0x100)

#define PAUSE_SHIFT 5

#define SWDPIO_SHIFT 17

#define SWDPIO__EXT_SHIFT 4

#define ILOS_SHIFT  3

#define MDI_REGADD_SHIFT 16

#define MDI_PHYADD_SHIFT 21

#define RECEIVE_BUFFER_ALIGN_SIZE  (256)

#define LINK_UP_TIMEOUT             500

#define E1000_TX_BUFFER_SIZE ((u32)1514)

#define E1000_MIN_SIZE_OF_RECEIVE_BUFFERS (2048)

#define CARRIER_EXTENSION   0x0F

#define TBI_ACCEPT(RxErrors, LastByteInFrame, HwFrameLength) (Adapter->TbiCompatibilityOn                                      && (((RxErrors) & E1000_RXD_ERR_FRAME_ERR_MASK) == E1000_RXD_ERR_CE)&& ((LastByteInFrame) == CARRIER_EXTENSION)                         && ((HwFrameLength) > 64)                                           && ((HwFrameLength) <= Adapter->MaxFrameSize+1))

#define E1000_WAIT_PERIOD           10

#endif /* _EM_FXHW_H_ */

