/*******************************************************************************

  Copyright (c) 2001-2007, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/
/* $FreeBSD$ */


#ifndef _E1000_REGS_H_
#define _E1000_REGS_H_

#define E1000_CTRL     0x00000  /* Device Control - RW */
#define E1000_CTRL_DUP 0x00004  /* Device Control Duplicate (Shadow) - RW */
#define E1000_STATUS   0x00008  /* Device Status - RO */
#define E1000_EECD     0x00010  /* EEPROM/Flash Control - RW */
#define E1000_EERD     0x00014  /* EEPROM Read - RW */
#define E1000_CTRL_EXT 0x00018  /* Extended Device Control - RW */
#define E1000_FLA      0x0001C  /* Flash Access - RW */
#define E1000_MDIC     0x00020  /* MDI Control - RW */
#define E1000_SCTL     0x00024  /* SerDes Control - RW */
#define E1000_FCAL     0x00028  /* Flow Control Address Low - RW */
#define E1000_FCAH     0x0002C  /* Flow Control Address High -RW */
#define E1000_FEXTNVM  0x00028  /* Future Extended NVM - RW */
#define E1000_FCT      0x00030  /* Flow Control Type - RW */
#define E1000_CONNSW   0x00034  /* Copper/Fiber switch control - RW */
#define E1000_VET      0x00038  /* VLAN Ether Type - RW */
#define E1000_ICR      0x000C0  /* Interrupt Cause Read - R/clr */
#define E1000_ITR      0x000C4  /* Interrupt Throttling Rate - RW */
#define E1000_ICS      0x000C8  /* Interrupt Cause Set - WO */
#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */
#define E1000_IMC      0x000D8  /* Interrupt Mask Clear - WO */
#define E1000_IAM      0x000E0  /* Interrupt Acknowledge Auto Mask */
#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_FCTTV    0x00170  /* Flow Control Transmit Timer Value - RW */
#define E1000_TXCW     0x00178  /* TX Configuration Word - RW */
#define E1000_RXCW     0x00180  /* RX Configuration Word - RO */
#define E1000_EICR     0x01580  /* Ext. Interrupt Cause Read - R/clr */
#define E1000_EITR(_n) (0x01680 + (0x4 * (_n)))
#define E1000_EICS     0x01520  /* Ext. Interrupt Cause Set - W0 */
#define E1000_EIMS     0x01524  /* Ext. Interrupt Mask Set/Read - RW */
#define E1000_EIMC     0x01528  /* Ext. Interrupt Mask Clear - WO */
#define E1000_EIAC     0x0152C  /* Ext. Interrupt Auto Clear - RW */
#define E1000_EIAM     0x01530  /* Ext. Interrupt Ack Auto Clear Mask - RW */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TCTL_EXT 0x00404  /* Extended TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_TBT      0x00448  /* TX Burst Timer - RW */
#define E1000_AIT      0x00458  /* Adaptive Interframe Spacing Throttle - RW */
#define E1000_LEDCTL   0x00E00  /* LED Control - RW */
#define E1000_EXTCNF_CTRL  0x00F00  /* Extended Configuration Control */
#define E1000_EXTCNF_SIZE  0x00F08  /* Extended Configuration Size */
#define E1000_PHY_CTRL     0x00F10  /* PHY Control Register in CSR */
#define E1000_PBA      0x01000  /* Packet Buffer Allocation - RW */
#define E1000_PBS      0x01008  /* Packet Buffer Size */
#define E1000_EEMNGCTL 0x01010  /* MNG EEprom Control */
#define E1000_EEARBC   0x01024  /* EEPROM Auto Read Bus Control */
#define E1000_FLASHT   0x01028  /* FLASH Timer Register */
#define E1000_EEWR     0x0102C  /* EEPROM Write Register - RW */
#define E1000_FLSWCTL  0x01030  /* FLASH control register */
#define E1000_FLSWDATA 0x01034  /* FLASH data register */
#define E1000_FLSWCNT  0x01038  /* FLASH Access Counter */
#define E1000_FLOP     0x0103C  /* FLASH Opcode Register */
#define E1000_I2CCMD   0x01028  /* SFPI2C Command Register - RW */
#define E1000_I2CPARAMS 0x0102C /* SFPI2C Parameters Register - RW */
#define E1000_WDSTP    0x01040  /* Watchdog Setup - RW */
#define E1000_SWDSTS   0x01044  /* SW Device Status - RW */
#define E1000_FRTIMER  0x01048  /* Free Running Timer - RW */
#define E1000_TCPTIMER 0x0104C  /* TCP Timer - RW */
#define E1000_ERT      0x02008  /* Early Rx Threshold - RW */
#define E1000_FCRTL    0x02160  /* Flow Control Receive Threshold Low - RW */
#define E1000_FCRTH    0x02168  /* Flow Control Receive Threshold High - RW */
#define E1000_PSRCTL   0x02170  /* Packet Split Receive Control - RW */
#define E1000_RDFPCQ(_n)  (0x02430 + (0x4 * (_n)))
#define E1000_PBRTH    0x02458  /* PB RX Arbitration Threshold - RW */
#define E1000_FCRTV    0x02460  /* Flow Control Refresh Timer Value - RW */
/* Split and Replication RX Control - RW */
#define E1000_RDPUMB   0x025CC  /* DMA RX Descriptor uC Mailbox - RW */
#define E1000_RDPUAD   0x025D0  /* DMA RX Descriptor uC Addr Command - RW */
#define E1000_RDPUWD   0x025D4  /* DMA RX Descriptor uC Data Write - RW */
#define E1000_RDPURD   0x025D8  /* DMA RX Descriptor uC Data Read - RW */
#define E1000_RDPUCTL  0x025DC  /* DMA RX Descriptor uC Control - RW */
#define E1000_RDTR     0x02820  /* RX Delay Timer - RW */
#define E1000_RADV     0x0282C  /* RX Interrupt Absolute Delay Timer - RW */
/*
 * Convenience macros
 *
 * Note: "_n" is the queue number of the register to be written to.
 *
 * Example usage:
 * E1000_RDBAL_REG(current_rx_queue)
 */
#define E1000_RDBAL(_n)   ((_n) < 4 ? (0x02800 + ((_n) * 0x100)) : (0x0C000 + ((_n) * 0x40)))
#define E1000_RDBAH(_n)   ((_n) < 4 ? (0x02804 + ((_n) * 0x100)) : (0x0C004 + ((_n) * 0x40)))
#define E1000_RDLEN(_n)   ((_n) < 4 ? (0x02808 + ((_n) * 0x100)) : (0x0C008 + ((_n) * 0x40)))
#define E1000_SRRCTL(_n)  ((_n) < 4 ? (0x0280C + ((_n) * 0x100)) : (0x0C00C + ((_n) * 0x40)))
#define E1000_RDH(_n)     ((_n) < 4 ? (0x02810 + ((_n) * 0x100)) : (0x0C010 + ((_n) * 0x40)))
#define E1000_RDT(_n)     ((_n) < 4 ? (0x02818 + ((_n) * 0x100)) : (0x0C018 + ((_n) * 0x40)))
#define E1000_RXDCTL(_n)  ((_n) < 4 ? (0x02828 + ((_n) * 0x100)) : (0x0C028 + ((_n) * 0x40)))
#define E1000_TDBAL(_n)   ((_n) < 4 ? (0x03800 + ((_n) * 0x100)) : (0x0E000 + ((_n) * 0x40)))
#define E1000_TDBAH(_n)   ((_n) < 4 ? (0x03804 + ((_n) * 0x100)) : (0x0E004 + ((_n) * 0x40)))
#define E1000_TDLEN(_n)   ((_n) < 4 ? (0x03808 + ((_n) * 0x100)) : (0x0E008 + ((_n) * 0x40)))
#define E1000_TDH(_n)     ((_n) < 4 ? (0x03810 + ((_n) * 0x100)) : (0x0E010 + ((_n) * 0x40)))
#define E1000_TDT(_n)     ((_n) < 4 ? (0x03818 + ((_n) * 0x100)) : (0x0E018 + ((_n) * 0x40)))
#define E1000_TXDCTL(_n)  ((_n) < 4 ? (0x03828 + ((_n) * 0x100)) : (0x0E028 + ((_n) * 0x40)))
#define E1000_TARC(_n)    (0x03840 + (_n << 8))
#define E1000_DCA_TXCTRL(_n) (0x03814 + (_n << 8))
#define E1000_DCA_RXCTRL(_n) (0x02814 + (_n << 8))
#define E1000_TDWBAL(_n)  ((_n) < 4 ? (0x03838 + ((_n) * 0x100)) : (0x0E038 + ((_n) * 0x40)))
#define E1000_TDWBAH(_n)  ((_n) < 4 ? (0x0383C + ((_n) * 0x100)) : (0x0E03C + ((_n) * 0x40)))
#define E1000_RSRPD    0x02C00  /* RX Small Packet Detect - RW */
#define E1000_RAID     0x02C08  /* Receive Ack Interrupt Delay - RW */
#define E1000_TXDMAC   0x03000  /* TX DMA Control - RW */
#define E1000_KABGTXD  0x03004  /* AFE Band Gap Transmit Ref Data */
#define E1000_TDFH     0x03410  /* TX Data FIFO Head - RW */
#define E1000_TDFT     0x03418  /* TX Data FIFO Tail - RW */
#define E1000_TDFHS    0x03420  /* TX Data FIFO Head Saved - RW */
#define E1000_TDFTS    0x03428  /* TX Data FIFO Tail Saved - RW */
#define E1000_TDFPC    0x03430  /* TX Data FIFO Packet Count - RW */
#define E1000_TDPUMB   0x0357C  /* DMA TX Descriptor uC Mail Box - RW */
#define E1000_TDPUAD   0x03580  /* DMA TX Descriptor uC Addr Command - RW */
#define E1000_TDPUWD   0x03584  /* DMA TX Descriptor uC Data Write - RW */
#define E1000_TDPURD   0x03588  /* DMA TX Descriptor uC Data  Read  - RW */
#define E1000_TDPUCTL  0x0358C  /* DMA TX Descriptor uC Control - RW */
#define E1000_DTXCTL   0x03590  /* DMA TX Control - RW */
#define E1000_TIDV     0x03820  /* TX Interrupt Delay Value - RW */
#define E1000_TADV     0x0382C  /* TX Interrupt Absolute Delay Val - RW */
#define E1000_TSPMT    0x03830  /* TCP Segmentation PAD & Min Threshold - RW */
#define E1000_CRCERRS  0x04000  /* CRC Error Count - R/clr */
#define E1000_ALGNERRC 0x04004  /* Alignment Error Count - R/clr */
#define E1000_SYMERRS  0x04008  /* Symbol Error Count - R/clr */
#define E1000_RXERRC   0x0400C  /* Receive Error Count - R/clr */
#define E1000_MPC      0x04010  /* Missed Packet Count - R/clr */
#define E1000_SCC      0x04014  /* Single Collision Count - R/clr */
#define E1000_ECOL     0x04018  /* Excessive Collision Count - R/clr */
#define E1000_MCC      0x0401C  /* Multiple Collision Count - R/clr */
#define E1000_LATECOL  0x04020  /* Late Collision Count - R/clr */
#define E1000_COLC     0x04028  /* Collision Count - R/clr */
#define E1000_DC       0x04030  /* Defer Count - R/clr */
#define E1000_TNCRS    0x04034  /* TX-No CRS - R/clr */
#define E1000_SEC      0x04038  /* Sequence Error Count - R/clr */
#define E1000_CEXTERR  0x0403C  /* Carrier Extension Error Count - R/clr */
#define E1000_RLEC     0x04040  /* Receive Length Error Count - R/clr */
#define E1000_XONRXC   0x04048  /* XON RX Count - R/clr */
#define E1000_XONTXC   0x0404C  /* XON TX Count - R/clr */
#define E1000_XOFFRXC  0x04050  /* XOFF RX Count - R/clr */
#define E1000_XOFFTXC  0x04054  /* XOFF TX Count - R/clr */
#define E1000_FCRUC    0x04058  /* Flow Control RX Unsupported Count- R/clr */
#define E1000_PRC64    0x0405C  /* Packets RX (64 bytes) - R/clr */
#define E1000_PRC127   0x04060  /* Packets RX (65-127 bytes) - R/clr */
#define E1000_PRC255   0x04064  /* Packets RX (128-255 bytes) - R/clr */
#define E1000_PRC511   0x04068  /* Packets RX (255-511 bytes) - R/clr */
#define E1000_PRC1023  0x0406C  /* Packets RX (512-1023 bytes) - R/clr */
#define E1000_PRC1522  0x04070  /* Packets RX (1024-1522 bytes) - R/clr */
#define E1000_GPRC     0x04074  /* Good Packets RX Count - R/clr */
#define E1000_BPRC     0x04078  /* Broadcast Packets RX Count - R/clr */
#define E1000_MPRC     0x0407C  /* Multicast Packets RX Count - R/clr */
#define E1000_GPTC     0x04080  /* Good Packets TX Count - R/clr */
#define E1000_GORCL    0x04088  /* Good Octets RX Count Low - R/clr */
#define E1000_GORCH    0x0408C  /* Good Octets RX Count High - R/clr */
#define E1000_GOTCL    0x04090  /* Good Octets TX Count Low - R/clr */
#define E1000_GOTCH    0x04094  /* Good Octets TX Count High - R/clr */
#define E1000_RNBC     0x040A0  /* RX No Buffers Count - R/clr */
#define E1000_RUC      0x040A4  /* RX Undersize Count - R/clr */
#define E1000_RFC      0x040A8  /* RX Fragment Count - R/clr */
#define E1000_ROC      0x040AC  /* RX Oversize Count - R/clr */
#define E1000_RJC      0x040B0  /* RX Jabber Count - R/clr */
#define E1000_MGTPRC   0x040B4  /* Management Packets RX Count - R/clr */
#define E1000_MGTPDC   0x040B8  /* Management Packets Dropped Count - R/clr */
#define E1000_MGTPTC   0x040BC  /* Management Packets TX Count - R/clr */
#define E1000_TORL     0x040C0  /* Total Octets RX Low - R/clr */
#define E1000_TORH     0x040C4  /* Total Octets RX High - R/clr */
#define E1000_TOTL     0x040C8  /* Total Octets TX Low - R/clr */
#define E1000_TOTH     0x040CC  /* Total Octets TX High - R/clr */
#define E1000_TPR      0x040D0  /* Total Packets RX - R/clr */
#define E1000_TPT      0x040D4  /* Total Packets TX - R/clr */
#define E1000_PTC64    0x040D8  /* Packets TX (64 bytes) - R/clr */
#define E1000_PTC127   0x040DC  /* Packets TX (65-127 bytes) - R/clr */
#define E1000_PTC255   0x040E0  /* Packets TX (128-255 bytes) - R/clr */
#define E1000_PTC511   0x040E4  /* Packets TX (256-511 bytes) - R/clr */
#define E1000_PTC1023  0x040E8  /* Packets TX (512-1023 bytes) - R/clr */
#define E1000_PTC1522  0x040EC  /* Packets TX (1024-1522 Bytes) - R/clr */
#define E1000_MPTC     0x040F0  /* Multicast Packets TX Count - R/clr */
#define E1000_BPTC     0x040F4  /* Broadcast Packets TX Count - R/clr */
#define E1000_TSCTC    0x040F8  /* TCP Segmentation Context TX - R/clr */
#define E1000_TSCTFC   0x040FC  /* TCP Segmentation Context TX Fail - R/clr */
#define E1000_IAC      0x04100  /* Interrupt Assertion Count */
#define E1000_ICRXPTC  0x04104  /* Interrupt Cause Rx Packet Timer Expire Count */
#define E1000_ICRXATC  0x04108  /* Interrupt Cause Rx Absolute Timer Expire Count */
#define E1000_ICTXPTC  0x0410C  /* Interrupt Cause Tx Packet Timer Expire Count */
#define E1000_ICTXATC  0x04110  /* Interrupt Cause Tx Absolute Timer Expire Count */
#define E1000_ICTXQEC  0x04118  /* Interrupt Cause Tx Queue Empty Count */
#define E1000_ICTXQMTC 0x0411C  /* Interrupt Cause Tx Queue Minimum Threshold Count */
#define E1000_ICRXDMTC 0x04120  /* Interrupt Cause Rx Descriptor Minimum Threshold Count */
#define E1000_ICRXOC   0x04124  /* Interrupt Cause Receiver Overrun Count */
#define E1000_PCS_CFG0    0x04200  /* PCS Configuration 0 - RW */
#define E1000_PCS_LCTL    0x04208  /* PCS Link Control - RW */
#define E1000_PCS_LSTAT   0x0420C  /* PCS Link Status - RO */
#define E1000_CBTMPC      0x0402C  /* Circuit Breaker TX Packet Count */
#define E1000_HTDPMC      0x0403C  /* Host Transmit Discarded Packets */
#define E1000_CBRDPC      0x04044  /* Circuit Breaker RX Dropped Count */
#define E1000_CBRMPC      0x040FC  /* Circuit Breaker RX Packet Count */
#define E1000_RPTHC       0x04104  /* Rx Packets To Host */
#define E1000_HGPTC       0x04118  /* Host Good Packets TX Count */
#define E1000_HTCBDPC     0x04124  /* Host TX Circuit Breaker Dropped Count */
#define E1000_HGORCL      0x04128  /* Host Good Octets Received Count Low */
#define E1000_HGORCH      0x0412C  /* Host Good Octets Received Count High */
#define E1000_HGOTCL      0x04130  /* Host Good Octets Transmit Count Low */
#define E1000_HGOTCH      0x04134  /* Host Good Octets Transmit Count High */
#define E1000_LENERRS     0x04138  /* Length Errors Count */
#define E1000_SCVPC       0x04228  /* SerDes/SGMII Code Violation Pkt Count */
#define E1000_HRMPC       0x0A018  /* Header Redirection Missed Packet Count */
#define E1000_PCS_ANADV   0x04218  /* AN advertisement - RW */
#define E1000_PCS_LPAB    0x0421C  /* Link Partner Ability - RW */
#define E1000_PCS_NPTX    0x04220  /* AN Next Page Transmit - RW */
#define E1000_PCS_LPABNP  0x04224  /* Link Partner Ability Next Page - RW */
#define E1000_1GSTAT_RCV  0x04228  /* 1GSTAT Code Violation Packet Count - RW */
#define E1000_RXCSUM   0x05000  /* RX Checksum Control - RW */
#define E1000_RLPML    0x05004  /* RX Long Packet Max Length */
#define E1000_RFCTL    0x05008  /* Receive Filter Control*/
#define E1000_MTA      0x05200  /* Multicast Table Array - RW Array */
#define E1000_RA       0x05400  /* Receive Address - RW Array */
#define E1000_PSRTYPE  0x05480  /* Packet Split Receive Type - RW */
#define E1000_VFTA     0x05600  /* VLAN Filter Table Array - RW Array */
#define E1000_VMD_CTL  0x0581C  /* VMDq Control - RW */
#define E1000_VFQA0    0x0B000  /* VLAN Filter Queue Array 0 - RW Array */
#define E1000_VFQA1    0x0B200  /* VLAN Filter Queue Array 1 - RW Array */
#define E1000_WUC      0x05800  /* Wakeup Control - RW */
#define E1000_WUFC     0x05808  /* Wakeup Filter Control - RW */
#define E1000_WUS      0x05810  /* Wakeup Status - RO */
#define E1000_MANC     0x05820  /* Management Control - RW */
#define E1000_IPAV     0x05838  /* IP Address Valid - RW */
#define E1000_IP4AT    0x05840  /* IPv4 Address Table - RW Array */
#define E1000_IP6AT    0x05880  /* IPv6 Address Table - RW Array */
#define E1000_WUPL     0x05900  /* Wakeup Packet Length - RW */
#define E1000_WUPM     0x05A00  /* Wakeup Packet Memory - RO A */
#define E1000_PBACL    0x05B68  /* MSIx PBA Clear - Read/Write 1's to clear */
#define E1000_FFLT     0x05F00  /* Flexible Filter Length Table - RW Array */
#define E1000_HOST_IF  0x08800  /* Host Interface */
#define E1000_FFMT     0x09000  /* Flexible Filter Mask Table - RW Array */
#define E1000_FFVT     0x09800  /* Flexible Filter Value Table - RW Array */

#define E1000_KMRNCTRLSTA 0x00034 /* MAC-PHY interface - RW */
#define E1000_MDPHYA      0x0003C /* PHY address - RW */
#define E1000_MANC2H      0x05860 /* Management Control To Host - RW */
#define E1000_SW_FW_SYNC  0x05B5C /* Software-Firmware Synchronization - RW */
#define E1000_CCMCTL      0x05B48 /* CCM Control Register */
#define E1000_GIOCTL      0x05B44 /* GIO Analog Control Register */
#define E1000_SCCTL       0x05B4C /* PCIc PLL Configuration Register */
#define E1000_GCR         0x05B00 /* PCI-Ex Control */
#define E1000_GSCL_1    0x05B10 /* PCI-Ex Statistic Control #1 */
#define E1000_GSCL_2    0x05B14 /* PCI-Ex Statistic Control #2 */
#define E1000_GSCL_3    0x05B18 /* PCI-Ex Statistic Control #3 */
#define E1000_GSCL_4    0x05B1C /* PCI-Ex Statistic Control #4 */
#define E1000_FACTPS    0x05B30 /* Function Active and Power State to MNG */
#define E1000_SWSM      0x05B50 /* SW Semaphore */
#define E1000_FWSM      0x05B54 /* FW Semaphore */
#define E1000_DCA_ID    0x05B70 /* DCA Requester ID Information - RO */
#define E1000_DCA_CTRL  0x05B74 /* DCA Control - RW */
#define E1000_FFLT_DBG  0x05F04 /* Debug Register */
#define E1000_HICR      0x08F00 /* Host Inteface Control */

/* RSS registers */
#define E1000_CPUVEC    0x02C10 /* CPU Vector Register - RW */
#define E1000_MRQC      0x05818 /* Multiple Receive Control - RW */
#define E1000_IMIR(_i)      (0x05A80 + ((_i) * 4))  /* Immediate Interrupt */
#define E1000_IMIREXT(_i)   (0x05AA0 + ((_i) * 4))  /* Immediate Interrupt Ext*/
#define E1000_IMIRVP    0x05AC0 /* Immediate Interrupt RX VLAN Priority - RW */
#define E1000_MSIXBM(_i)    (0x01600 + ((_i) * 4)) /* MSI-X Allocation Register (_i) - RW */
#define E1000_MSIXTADD(_i)  (0x0C000 + ((_i) * 0x10)) /* MSI-X Table entry addr low reg 0 - RW */
#define E1000_MSIXTUADD(_i) (0x0C004 + ((_i) * 0x10)) /* MSI-X Table entry addr upper reg 0 - RW */
#define E1000_MSIXTMSG(_i)  (0x0C008 + ((_i) * 0x10)) /* MSI-X Table entry message reg 0 - RW */
#define E1000_MSIXVCTRL(_i) (0x0C00C + ((_i) * 0x10)) /* MSI-X Table entry vector ctrl reg 0 - RW */
#define E1000_MSIXPBA    0x0E000 /* MSI-X Pending bit array */
#define E1000_RETA(_i)  (0x05C00 + ((_i) * 4)) /* Redirection Table - RW Array */
#define E1000_RSSRK(_i) (0x05C80 + ((_i) * 4)) /* RSS Random Key - RW Array */
#define E1000_RSSIM     0x05864 /* RSS Interrupt Mask */
#define E1000_RSSIR     0x05868 /* RSS Interrupt Request */

#endif
