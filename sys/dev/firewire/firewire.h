/*
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetoshi Shimokawa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD$
 *
 */

#ifndef _FIREWIRE_H
#define _FIREWIRE_H 1

#define	DEV_DEF  0
#define	DEV_DV   2

struct dv_data{
	u_int32_t n_write;
	u_int32_t a_write;
	u_int32_t k_write;
	u_int32_t write_done;
	u_int32_t write_len[16];
	u_int32_t write_off[16];
	u_int32_t n_read;
	u_int32_t a_read;
	u_int32_t k_read;
	u_int32_t read_done;
	u_int32_t read_len[16];
	u_int32_t read_off[16];
};

struct dv_data_req_t {
	unsigned long index;
	unsigned long len;
	unsigned long off;
};

struct fw_isochreq {
	unsigned char	ch:6,
			tag:2;
};

struct fw_isobufreq {
	struct {
		unsigned int nchunk;
		unsigned int npacket;
		unsigned int psize;
	} tx, rx;
};

struct fw_addr{
	unsigned long hi;
	unsigned long lo;
};

struct fw_asybindreq {
	struct fw_addr start;
	unsigned long len;
};

struct fw_reg_req_t{
	unsigned long addr;
	unsigned long data;
};

#define FWPMAX_S400 (2048 + 20)	/* MAXREC plus space for control data */
#define FWMAXQUEUE 128

#define	FWLOCALBUS	0xffc0

#define FWTCODE_WREQQ	0
#define FWTCODE_WREQB	1
#define FWTCODE_WRES	2
#define FWTCODE_RREQQ	4
#define FWTCODE_RREQB	5
#define FWTCODE_RRESQ	6
#define FWTCODE_RRESB	7
#define FWTCODE_CYCS	8
#define FWTCODE_LREQ	9
#define FWTCODE_STREAM	0xa
#define FWTCODE_LRES	0xb
#define FWTCODE_PHY	0xe

#define	FWRETRY_1	0
#define	FWRETRY_X	1
#define	FWRETRY_A	2
#define	FWRETRY_B	3

#define FWRCODE_COMPLETE	0
#define FWRCODE_ER_CONFL	4
#define FWRCODE_ER_DATA		5
#define FWRCODE_ER_TYPE		6
#define FWRCODE_ER_ADDR		7

#define FWSPD_S100	0
#define FWSPD_S200	1
#define FWSPD_S400	2

#define	FWP_TL_VALID (1 << 7)

struct fw_isohdr{
	u_int32_t hdr[1];
};
struct fw_asyhdr{
	u_int32_t hdr[4];
};
#define FWPHYSIDSUBS(SID) (((SID) >> 23) & 1)
#define FWPHYSIDNODE(SID) (((SID) >> 24) & 0x3f)
#define FWPHYSIDLINK(SID) (((SID) >> 22) & 1)
#define FWPHYSIDGAP(SID) (((SID) >> 16) & 0x3f)
#define FWPHYSIDSPD(SID) (((SID) >> 14) & 0x3)
#define FWPHYSIDDEL(SID) (((SID) >> 12) & 0x3)
#define FWPHYSIDCON(SID) (((SID) >> 11) & 1)
#define FWPHYSIDPWR(SID) (((SID) >> 8) & 0x7)
#define FWPHYSIDP0(SID) (((SID) >> 6) & 0x3)
#define FWPHYSIDP1(SID) (((SID) >> 4) & 0x3)
#define FWPHYSIDP2(SID) (((SID) >> 2) & 0x3)
#define FWPHYSIDIR(SID) (((SID) >> 1) & 1)
#define FWPHYSIDMORE(SID) ((SID) & 1)
#define FWPHYSIDSEQ(SID) (((SID) >> 20) & 0x7)
#define FWPHYSIDPA(SID) (((SID) >> 16) & 0x3)
#define FWPHYSIDPB(SID) (((SID) >> 14) & 0x3)
#define FWPHYSIDPC(SID) (((SID) >> 12) & 0x3)
#define FWPHYSIDPD(SID) (((SID) >> 10) & 0x3)
#define FWPHYSIDPE(SID) (((SID) >> 8) & 0x3)
#define FWPHYSIDPF(SID) (((SID) >> 6) & 0x3)
#define FWPHYSIDPG(SID) (((SID) >> 4) & 0x3)
#define FWPHYSIDPH(SID) (((SID) >> 2) & 0x3)
struct fw_pkt{
#if BYTE_ORDER == LITTLE_ENDIAN
	union{
		u_int32_t ld[0];
		struct {
			u_int32_t :28,
				  tcode:4;
		}common;
		struct {
			u_int16_t len;
			u_int8_t chtag;
			u_int8_t sy:4,
				 tcode:4;
			u_int32_t payload[0];
		}stream;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
		}hdr;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
			u_int16_t dest_hi;
			u_int32_t dest_lo;
		}rreqq;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
			u_int16_t res1:4,
				  rtcode:4,
				  res2:8;
			u_int32_t res3;
		}wres;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
			u_int16_t dest_hi;
			u_int32_t dest_lo;
			u_int16_t len;
			u_int16_t extcode:16;
		}rreqb;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
			u_int16_t dest_hi:16;
			u_int32_t dest_lo;
			u_int32_t data;
		}wreqq;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
			u_int16_t dest_hi;
			u_int32_t dest_lo;
			u_int32_t data;
		}cyc;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
			u_int16_t res1:4,
				  rtcode:4,
				  res2:8;
			u_int32_t res3;
			u_int32_t data;
		}rresq;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
			u_int16_t dest_hi;
			u_int32_t dest_lo;
			u_int16_t len;
			u_int16_t extcode;
			u_int32_t payload[0];
		}wreqb;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
			u_int16_t dest_hi;
			u_int32_t dest_lo;
			u_int16_t len;
			u_int16_t extcode;
#define FW_LREQ_MSKSWAP	1
#define FW_LREQ_CMPSWAP	2
#define FW_LREQ_FTADD	3
#define FW_LREQ_LTADD	4
#define FW_LREQ_BDADD	5
#define FW_LREQ_WRADD	6
			u_int32_t payload[0];
		}lreq;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
			u_int16_t res1:4,
				  rtcode:4,
				  res2:8;
			u_int32_t res3;
			u_int16_t len;
			u_int16_t extcode;
			u_int32_t payload[0];
		}rresb;
		struct {
			u_int16_t dst;
			u_int8_t tlrt;
			u_int8_t pri:4,
				 tcode:4;
			u_int16_t src;
			u_int16_t res1:4,
				  rtcode:4,
				  res2:8;
			u_int32_t res3;
			u_int16_t len;
			u_int16_t extcode;
			u_int32_t payload[0];
		}lres;
	}mode;
#else
	union{
		u_int32_t ld[0];
		struct {
			u_int32_t :4,
				  tcode:4,
				  :24;
		}common;
		struct {
			u_int8_t sy:4,
				 tcode:4;
			u_int8_t chtag;
			u_int16_t len;
			u_int32_t payload[0];
		}stream;
		struct {
			u_int32_t pri:4,
				  tcode:4,
				  tlrt:8,
				  dst:16;
			u_int32_t :16,
				  src:16;
		}hdr;
		struct {
			u_int8_t pri:4,
				 tcode:4;
			u_int8_t tlrt;
			u_int16_t dst;
			u_int16_t dest_hi;
			u_int16_t src;
			u_int32_t dest_lo;
		}rreqq;
		struct {
			u_int8_t pri:4,
				 tcode:4;
			u_int8_t tlrt;
			u_int16_t dst;
			u_int16_t res1:12,
				  rtcode:4;
			u_int16_t src;
			u_int32_t res3;
		}wres;
		struct {
			u_int8_t pri:4,
				 tcode:4;
			u_int8_t tlrt;
			u_int16_t dst;
			u_int16_t dest_hi;
			u_int16_t src;
			u_int32_t dest_lo;
			u_int16_t extcode:16;
			u_int16_t len;
		}rreqb;
		struct {
			u_int8_t pri:4,
				 tcode:4;
			u_int8_t tlrt;
			u_int16_t dst;
			u_int16_t dest_hi:16;
			u_int16_t src;
			u_int32_t dest_lo;
			u_int32_t data;
		}wreqq;
		struct {
			u_int8_t pri:4,
				 tcode:4;
			u_int8_t tlrt;
			u_int16_t dst;
			u_int16_t dest_hi;
			u_int16_t src;
			u_int32_t dest_lo;
			u_int32_t data;
		}cyc;
		struct {
			u_int8_t pri:4,
				 tcode:4;
			u_int8_t tlrt;
			u_int16_t dst;
			u_int16_t res1:12,
				  rtcode:4;
			u_int16_t src;
			u_int32_t res3;
			u_int32_t data;
		}rresq;
		struct {
			u_int8_t pri:4,
				 tcode:4;
			u_int8_t tlrt;
			u_int16_t dst;
			u_int16_t dest_hi;
			u_int16_t src;
			u_int32_t dest_lo;
			u_int16_t extcode;
			u_int16_t len;
			u_int32_t payload[0];
		}wreqb;
		struct {
			u_int8_t pri:4,
				 tcode:4;
			u_int8_t tlrt;
			u_int16_t dst;
			u_int16_t dest_hi;
			u_int16_t src;
			u_int32_t dest_lo;
			u_int16_t extcode;
			u_int16_t len;
#define FW_LREQ_MSKSWAP	1
#define FW_LREQ_CMPSWAP	2
#define FW_LREQ_FTADD	3
#define FW_LREQ_LTADD	4
#define FW_LREQ_BDADD	5
#define FW_LREQ_WRADD	6
			u_int32_t payload[0];
		}lreq;
		struct {
			u_int8_t pri:4,
				 tcode:4;
			u_int8_t tlrt;
			u_int16_t dst;
			u_int16_t res1:12,
				  rtcode:4;
			u_int16_t src;
			u_int32_t res3;
			u_int16_t extcode;
			u_int16_t len;
			u_int32_t payload[0];
		}rresb;
		struct {
			u_int8_t pri:4,
				 tcode:4;
			u_int8_t tlrt;
			u_int16_t dst;
			u_int16_t res1:12,
				  rtcode:4;
			u_int16_t src;
			u_int32_t res3;
			u_int16_t extcode;
			u_int16_t len;
			u_int32_t payload[0];
		}lres;
	}mode;
#endif
};
struct fw_eui64 {
	u_int32_t hi, lo;
};
struct fw_asyreq {
	struct fw_asyreq_t{
		unsigned char sped;
		unsigned int type;
#define FWASREQNODE	0
#define FWASREQEUI	1
#define FWASRESTL	2
#define FWASREQSTREAM	3
		unsigned short len;
		union {
			struct fw_eui64 eui;
		}dst;
	}req;
	struct fw_pkt pkt;
	u_int32_t data[512];
};
struct fw_devlstreq {
	int n;
	struct fw_eui64 eui[64];
	u_int16_t dst[64];
	u_int16_t status[64];
};
#define FW_SELF_ID_PORT_CONNECTED_TO_CHILD 3
#define FW_SELF_ID_PORT_CONNECTED_TO_PARENT 2
#define FW_SELF_ID_PORT_NOT_CONNECTED 1
#define FW_SELF_ID_PORT_NOT_EXISTS 0
union fw_self_id {
	struct {
		u_int32_t more_packets:1,
			  initiated_reset:1,
			  port2:2,
			  port1:2,
			  port0:2,
			  power_class:3,
			  contender:1,
			  phy_delay:2,
			  phy_speed:2,
			  gap_count:6,
			  link_active:1,
			  sequel:1,
			  phy_id:6,
			  id:2;
	} p0;
	struct {
		u_int32_t more_packets:1,
			  reserved1:1,
			  porth:2,
			  portg:2,
			  portf:2,
			  porte:2,
			  portd:2,
			  portc:2,
			  portb:2,
			  porta:2,
			  reserved2:2,
			  sequence_num:3,
			  sequel:1,
			  phy_id:6,
			  id:2;
	} p1;
};
struct fw_topology_map {
	u_int32_t crc:16,
		  crc_len:16;
	u_int32_t generation;
	u_int32_t self_id_count:16,
		  node_count:16;
	union fw_self_id self_id[4*64];
};
struct fw_speed_map {
	u_int32_t crc:16,
		  crc_len:16;
	u_int32_t generation;
	u_int8_t  speed[64][64];
};
struct fw_map_buf {
	int len;
	void *ptr;
};
struct fw_crom_buf {
	struct fw_eui64 eui;
	int len;
	void *ptr;
};
#define FWSTMAXCHUNK 16
/*
 * FireWire specific system requests.
 */
#define	FW_SSTDV	_IOWR('S', 85, unsigned int)
#define	FW_SSTBUF	_IOWR('S', 86, struct fw_isobufreq)
#define	FW_GSTBUF	_IOWR('S', 87, struct fw_isobufreq)
#define	FW_SRSTREAM	_IOWR('S', 88, struct fw_isochreq)
#define	FW_GRSTREAM	_IOWR('S', 89, struct fw_isochreq)
#define	FW_STSTREAM	_IOWR('S', 90, struct fw_isochreq)
#define	FW_GTSTREAM	_IOWR('S', 91, struct fw_isochreq)

#define	FW_ASYREQ	_IOWR('S', 92, struct fw_asyreq)
#define FW_IBUSRST	_IOR('S', 1, unsigned int)
#define FW_GDEVLST	_IOWR('S', 2, struct fw_devlstreq)
#define	FW_SBINDADDR	_IOWR('S', 3, struct fw_asybindreq)
#define	FW_CBINDADDR	_IOWR('S', 4, struct fw_asybindreq)
#define	FW_GTPMAP	_IOR('S', 5, struct fw_topology_map)
#define	FW_GSPMAP	_IOW('S', 6, struct fw_speed_map *)
#define	FW_GCROM	_IOWR('S', 7, struct fw_crom_buf)

#define FWOHCI_RDREG	_IOWR('S', 80, struct fw_reg_req_t)
#define FWOHCI_WRREG	_IOWR('S', 81, struct fw_reg_req_t)

#define DUMPDMA		_IOWR('S', 82, u_int32_t)

#ifdef _KERNEL

#define FWMAXNDMA 0x100 /* 8 bits DMA channel id. in device No. */

#if __FreeBSD_version < 500000
#define dev2unit(x)	((minor(x) & 0xff) | (minor(x) >> 8))
#define unit2minor(x)	(((x) & 0xff) | (((x) << 8) & ~0xffff))
#endif

#define UNIT2MIN(x)	(((x) & 0xff) << 8)
#define DEV2UNIT(x)	((dev2unit(x) & 0xff00) >> 8)
#define DEV2DMACH(x)	(dev2unit(x) & 0xff)

#define FWMEM_FLAG	0x10000
#define DEV_FWMEM(x)	(dev2unit(x) & FWMEM_FLAG)
#endif
#endif
