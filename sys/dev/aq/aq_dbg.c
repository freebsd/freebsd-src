/**
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3) The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file aq_dbg.c
 * Debugging stuff.
 * @date 2017.12.13  @author roman.agafonov@aquantia.com
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include "aq_common.h"
#include "aq_dbg.h"


const aq_debug_level dbg_level_ = lvl_detail;
const u32 dbg_categories_ = dbg_init | dbg_config | dbg_fw;



#define DESCR_FIELD(DESCR, BIT_BEGIN, BIT_END) \
	((DESCR >> BIT_END) &\
		(BIT(BIT_BEGIN - BIT_END + 1) -1))

#define __field(TYPE, VAR) TYPE VAR;
void trace_aq_tx_descr(int ring_idx, unsigned int pointer, volatile u64 descr[2])
{
#if AQ_CFG_DEBUG_LVL > 2
	struct __entry{
		__field(unsigned int, ring_idx)
		__field(unsigned int, pointer)
		/* Tx Descriptor */
		__field(u64, data_buf_addr)
		__field(u32, pay_len)
		__field(u8, ct_en)
		__field(u8, ct_idx)
		__field(u16, rsvd2)
		__field(u8, tx_cmd)
		__field(u8, eop)
		__field(u8, dd)
		__field(u16, buf_len)
		__field(u8, rsvd1)
		__field(u8, des_typ)
	} entry;

	entry.ring_idx = ring_idx;
	entry.pointer = pointer;
	entry.data_buf_addr = descr[0];
	entry.pay_len = DESCR_FIELD(descr[1], 63, 46);
	entry.ct_en =  DESCR_FIELD(descr[1], 45, 45);
	entry.ct_idx = DESCR_FIELD(descr[1], 44, 44);
	entry.rsvd2 = DESCR_FIELD(descr[1], 43, 30);
	entry.tx_cmd = DESCR_FIELD(descr[1], 29, 22);
	entry.eop = DESCR_FIELD(descr[1], 21, 21);
	entry.dd = DESCR_FIELD(descr[1], 20, 20);
	entry.buf_len = DESCR_FIELD(descr[1], 19, 4);
	entry.rsvd1 = DESCR_FIELD(descr[1], 3, 3);
	entry.des_typ = DESCR_FIELD(descr[1], 2, 0);


	aq_log_detail("trace_aq_tx_descr ring=%d descr=%u pay_len=%u ct_en=%u ct_idx=%u rsvd2=0x%x tx_cmd=0x%x eop=%u dd=%u buf_len=%u rsvd1=%u des_typ=0x%x",
		  entry.ring_idx, entry.pointer, entry.pay_len,
		  entry.ct_en, entry.ct_idx, entry.rsvd2,
		  entry.tx_cmd, entry.eop, entry.dd, entry.buf_len,
		  entry.rsvd1, entry.des_typ);
#endif
}

void trace_aq_rx_descr(int ring_idx, unsigned int pointer, volatile u64 descr[2])
{
#if AQ_CFG_DEBUG_LVL > 2
	u8 dd;
	u8 eop;
	u8 rx_stat;
	u8 rx_estat;
	u8 rsc_cnt;
	u16 pkt_len;
	u16 next_desp;
	u16 vlan_tag;

	u8 rss_type;
	u8 pkt_type;
	u8 rdm_err;
	u8 avb_ts;
	u8 rsvd;
	u8 rx_cntl;
	u8 sph;
	u16 hdr_len;
	u32 rss_hash;

	rss_hash = DESCR_FIELD(descr[0], 63, 32);
	hdr_len =  DESCR_FIELD(descr[0], 31, 22);
	sph = DESCR_FIELD(descr[0], 21, 21);
	rx_cntl = DESCR_FIELD(descr[0], 20, 19);
	rsvd = DESCR_FIELD(descr[0], 18, 14);
	avb_ts = DESCR_FIELD(descr[0], 13, 13);
	rdm_err = DESCR_FIELD(descr[0], 12, 12);
	pkt_type = DESCR_FIELD(descr[0], 11, 4);
	rss_type = DESCR_FIELD(descr[0], 3, 0);

	vlan_tag = DESCR_FIELD(descr[1], 63, 48);
	next_desp = DESCR_FIELD(descr[1], 47, 32);
	pkt_len = DESCR_FIELD(descr[1], 31, 16);
	rsc_cnt = DESCR_FIELD(descr[1], 15, 12);
	rx_estat = DESCR_FIELD(descr[1], 11, 6);
	rx_stat = DESCR_FIELD(descr[1], 5, 2);
	eop = DESCR_FIELD(descr[1], 1, 1);
	dd = DESCR_FIELD(descr[1], 0, 0);

	printf("trace_aq_rx_descr ring=%d descr=%u rss_hash=0x%x hdr_len=%u sph=%u rx_cntl=%u rsvd=0x%x avb_ts=%u rdm_err=%u pkt_type=%u rss_type=%u vlan_tag=%u next_desp=%u pkt_len=%u rsc_cnt=%u rx_estat=0x%x rx_stat=0x%x eop=%u dd=%u\n",
		  ring_idx, pointer, rss_hash,
		  hdr_len, sph, rx_cntl,
		  rsvd, avb_ts, rdm_err,
		  pkt_type, rss_type, vlan_tag,
		  next_desp, pkt_len, rsc_cnt,
		  rx_estat, rx_stat, eop, dd);
#endif
}

void trace_aq_tx_context_descr(int ring_idx, unsigned int pointer, volatile u64 descr[2])
{
#if AQ_CFG_DEBUG_LVL > 2
	struct __entry_s{
		__field(unsigned int, ring_idx)
		__field(unsigned int, pointer)
		/* Tx Context Descriptor */
		__field(u16, out_len)
		__field(u8, tun_len)
		__field(u64, resvd3)
		__field(u16, mss_len)
		__field(u8, l4_len)
		__field(u8, l3_len)
		__field(u8, l2_len)
		__field(u8, ct_cmd)
		__field(u16, vlan_tag)
		__field(u8, ct_idx)
		__field(u8, des_typ)
	} entry;
	struct __entry_s *__entry = &entry;
	__entry->ring_idx = ring_idx;
	__entry->pointer = pointer;
	__entry->out_len = DESCR_FIELD(descr[0], 63, 48);
	__entry->tun_len = DESCR_FIELD(descr[0], 47, 40);
	__entry->resvd3 = DESCR_FIELD(descr[0], 39, 0);
	__entry->mss_len = DESCR_FIELD(descr[1], 63, 48);
	__entry->l4_len = DESCR_FIELD(descr[1], 47, 40);
	__entry->l3_len = DESCR_FIELD(descr[1], 39, 31);
	__entry->l2_len = DESCR_FIELD(descr[1], 30, 24);
	__entry->ct_cmd = DESCR_FIELD(descr[1], 23, 20);
	__entry->vlan_tag = DESCR_FIELD(descr[1], 19, 4);
	__entry->ct_idx = DESCR_FIELD(descr[1], 3, 3);
	__entry->des_typ = DESCR_FIELD(descr[1], 2, 0);

	printf("trace_aq_tx_context_descr ring=%d descr=%u out_len=%u tun_len=%u resvd3=%lu mss_len=%u l4_len=%u l3_len=%u l2_len=%d ct_cmd=%u vlan_tag=%u ct_idx=%u des_typ=0x%x\n",
		  __entry->ring_idx, __entry->pointer, __entry->out_len,
		  __entry->tun_len, __entry->resvd3, __entry->mss_len,
		  __entry->l4_len, __entry->l3_len, __entry->l2_len,
		  __entry->ct_cmd, __entry->vlan_tag, __entry->ct_idx,
		  __entry->des_typ);
#endif
}

void DumpHex(const void* data, size_t size) {
#if AQ_CFG_DEBUG_LVL > 3
	char ascii[17];
	size_t i, j;
	char line[256];
	char buf[256];

	ascii[16] = '\0';
	line[0] = '\0';
	printf("packet at %p\n", data);

	for (i = 0; i < size; ++i) {
		sprintf(buf, "%02X ", ((const unsigned char*)data)[i]);
		strcat(line, buf);
		if (((const unsigned char*)data)[i] >= ' ' && ((const unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((const unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			strcat(line, " ");
			if ((i+1) % 16 == 0) {
				sprintf(buf, "|  %s \n", ascii);
				strcat(line, buf);
				printf("%s", line);
				line[0] = '\0';
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					strcat(line, " ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					strcat(line, "   ");
				}
				sprintf(buf, "|  %s \n", ascii);
				strcat(line, buf);
				printf("%s", line);
				line[0] = '\0';
			}
		}
	}
#endif
}