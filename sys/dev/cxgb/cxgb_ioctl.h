/**************************************************************************

Copyright (c) 2007-2008, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
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

$FreeBSD$

***************************************************************************/
#ifndef __CHIOCTL_H__
#define __CHIOCTL_H__

/*
 * Ioctl commands specific to this driver.
 */
enum {
	CH_SETREG = 0x40,
	CH_GETREG,
	CH_SETTPI,
	CH_GETTPI,
	CH_DEVUP,
	CH_GETMTUTAB,
	CH_SETMTUTAB,
	CH_GETMTU,
	CH_SET_PM,
	CH_GET_PM,
	CH_GET_TCAM,
	CH_SET_TCAM,
	CH_GET_TCB,
	CH_READ_TCAM_WORD,
	CH_GET_MEM,
	CH_GET_SGE_CONTEXT,
	CH_GET_SGE_DESC,
	CH_LOAD_FW,
	CH_GET_PROTO,
	CH_SET_PROTO,
	CH_SET_TRACE_FILTER,
	CH_SET_QSET_PARAMS,
	CH_GET_QSET_PARAMS,
	CH_SET_QSET_NUM,
	CH_GET_QSET_NUM,
	CH_SET_PKTSCHED,
	CH_IFCONF_GETREGS,
	CH_GETMIIREGS,
	CH_SETMIIREGS,
	CH_SET_FILTER,
	CH_SET_HW_SCHED,
	CH_DEL_FILTER,
};

struct ch_reg {
	uint32_t addr;
	uint32_t val;
};

struct ch_cntxt {
	uint32_t cntxt_type;
	uint32_t cntxt_id;
	uint32_t data[4];
};

/* context types */
enum { CNTXT_TYPE_EGRESS, CNTXT_TYPE_FL, CNTXT_TYPE_RSP, CNTXT_TYPE_CQ };

struct ch_desc {
	uint32_t cmd;
	uint32_t queue_num;
	uint32_t idx;
	uint32_t size;
	uint8_t  data[128];
};

struct ch_mem_range {
	uint32_t cmd;
	uint32_t mem_id;
	uint32_t addr;
	uint32_t len;
	uint32_t version;
	uint8_t  *buf;
};

struct ch_qset_params {
	uint32_t 	qset_idx;
	int32_t		txq_size[3];
	int32_t		rspq_size;
	int32_t		fl_size[2];
	int32_t		intr_lat;
	int32_t		polling;
	int32_t		lro;
	int32_t		cong_thres;
	int32_t		vector;
	int32_t		qnum;
};

struct ch_pktsched_params {
	uint32_t cmd;
	uint8_t  sched;
	uint8_t  idx;
	uint8_t  min;
	uint8_t  max;
	uint8_t  binding;
};

struct ch_hw_sched {
	uint32_t cmd;
	uint8_t  sched;
	int8_t   mode;
	int8_t   channel;
	int32_t  kbps;        /* rate in Kbps */
	int32_t  class_ipg;   /* tenths of nanoseconds */
	uint32_t flow_ipg;    /* usec */
};

struct ch_filter_tuple {
	uint32_t sip;
	uint32_t dip;
	uint16_t sport;
	uint16_t dport;
	uint16_t vlan:12;
	uint16_t vlan_prio:3;
};

struct ch_filter {
	uint32_t cmd;
	uint32_t filter_id;
	struct ch_filter_tuple val;
	struct ch_filter_tuple mask;
	uint16_t mac_addr_idx;
	uint8_t mac_hit:1;
	uint8_t proto:2;

	uint8_t want_filter_id:1; /* report filter TID instead of RSS hash */
	uint8_t pass:1;           /* whether to pass or drop packets */
	uint8_t rss:1;            /* use RSS or specified qset */
	uint8_t qset;
};

#ifndef TCB_SIZE
# define TCB_SIZE   128
#endif

/* TCB size in 32-bit words */
#define TCB_WORDS (TCB_SIZE / 4)

enum { MEM_CM, MEM_PMRX, MEM_PMTX };   /* ch_mem_range.mem_id values */

struct ch_mtus {
	uint32_t cmd;
	uint32_t nmtus;
	uint16_t mtus[NMTUS];
};

struct ch_pm {
	uint32_t cmd;
	uint32_t tx_pg_sz;
	uint32_t tx_num_pg;
	uint32_t rx_pg_sz;
	uint32_t rx_num_pg;
	uint32_t pm_total;
};

struct ch_tcam {
	uint32_t cmd;
	uint32_t tcam_size;
	uint32_t nservers;
	uint32_t nroutes;
	uint32_t nfilters;
};

struct ch_tcb {
	uint32_t cmd;
	uint32_t tcb_index;
	uint32_t tcb_data[TCB_WORDS];
};

struct ch_tcam_word {
	uint32_t cmd;
	uint32_t addr;
	uint32_t buf[3];
};

struct ch_trace {
	uint32_t cmd;
	uint32_t sip;
	uint32_t sip_mask;
	uint32_t dip;
	uint32_t dip_mask;
	uint16_t sport;
	uint16_t sport_mask;
	uint16_t dport;
	uint16_t dport_mask;
	uint32_t vlan:12,
		vlan_mask:12,
		intf:4,
		intf_mask:4;
	uint8_t  proto;
	uint8_t  proto_mask;
	uint8_t  invert_match:1,
		config_tx:1,
		config_rx:1,
		trace_tx:1,
		trace_rx:1;
};

#define REGDUMP_SIZE  (4 * 1024)

struct ifconf_regs {
	uint32_t  version;
	uint32_t  len; /* bytes */
	uint8_t   *data;
};

struct mii_data {
	uint32_t phy_id;
	uint32_t reg_num;
	uint32_t val_in;
	uint32_t val_out;
};

#define CHELSIO_SETREG              _IOW('f', CH_SETREG, struct ch_reg)
#define CHELSIO_GETREG              _IOWR('f', CH_GETREG, struct ch_reg)
#define CHELSIO_READ_TCAM_WORD      _IOR('f', CH_READ_TCAM_WORD, struct ch_tcam)
#define CHELSIO_GET_MEM             _IOWR('f', CH_GET_MEM, struct ch_mem_range)
#define CHELSIO_GET_SGE_CONTEXT     _IOWR('f', CH_GET_SGE_CONTEXT, struct ch_cntxt)
#define CHELSIO_GET_SGE_DESC        _IOWR('f', CH_GET_SGE_DESC, struct ch_desc)
#define CHELSIO_GET_QSET_PARAMS     _IOWR('f', CH_GET_QSET_PARAMS, struct ch_qset_params)
#define CHELSIO_SET_QSET_PARAMS     _IOW('f', CH_SET_QSET_PARAMS, struct ch_qset_params)
#define CHELSIO_GET_QSET_NUM        _IOWR('f', CH_GET_QSET_NUM, struct ch_reg)
#define CHELSIO_SET_QSET_NUM        _IOW('f', CH_SET_QSET_NUM, struct ch_reg)
#define CHELSIO_GETMTUTAB           _IOR('f', CH_GET_QSET_NUM, struct ch_mtus)
#define CHELSIO_SETMTUTAB           _IOW('f', CH_SET_QSET_NUM, struct ch_mtus)


#define CHELSIO_SET_TRACE_FILTER    _IOW('f', CH_SET_TRACE_FILTER, struct ch_trace)
#define CHELSIO_SET_PKTSCHED        _IOW('f', CH_SET_PKTSCHED, struct ch_pktsched_params)
#define CHELSIO_IFCONF_GETREGS      _IOWR('f', CH_IFCONF_GETREGS, struct ifconf_regs)
#define SIOCGMIIREG                 _IOWR('f', CH_GETMIIREGS, struct mii_data)
#define SIOCSMIIREG                 _IOWR('f', CH_SETMIIREGS, struct mii_data)
#define CHELSIO_SET_HW_SCHED        _IOWR('f', CH_SET_HW_SCHED, struct ch_hw_sched)
#define CHELSIO_SET_FILTER          _IOW('f', CH_SET_FILTER, struct ch_filter)
#define CHELSIO_DEL_FILTER          _IOW('f', CH_DEL_FILTER, struct ch_filter)
#define CHELSIO_DEVUP               _IO('f', CH_DEVUP)

#define CHELSIO_GET_TCB             _IOWR('f', CH_GET_TCB, struct ch_tcb)
#endif
