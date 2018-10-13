/******************************************************************************

  Copyright (c) 2013-2018, Intel Corporation
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

******************************************************************************/
/*$FreeBSD$*/


#ifndef _IXLV_H_
#define _IXLV_H_

#include "ixl.h"

#define IXLV_AQ_MAX_ERR		200
#define IXLV_MAX_FILTERS	128
#define IXLV_MAX_QUEUES		16
#define IXLV_AQ_TIMEOUT		(1 * hz)

#define IXLV_FLAG_AQ_ENABLE_QUEUES            (u32)(1 << 0)
#define IXLV_FLAG_AQ_DISABLE_QUEUES           (u32)(1 << 1)
#define IXLV_FLAG_AQ_ADD_MAC_FILTER           (u32)(1 << 2)
#define IXLV_FLAG_AQ_ADD_VLAN_FILTER          (u32)(1 << 3)
#define IXLV_FLAG_AQ_DEL_MAC_FILTER           (u32)(1 << 4)
#define IXLV_FLAG_AQ_DEL_VLAN_FILTER          (u32)(1 << 5)
#define IXLV_FLAG_AQ_CONFIGURE_QUEUES         (u32)(1 << 6)
#define IXLV_FLAG_AQ_MAP_VECTORS              (u32)(1 << 7)
#define IXLV_FLAG_AQ_HANDLE_RESET             (u32)(1 << 8)
#define IXLV_FLAG_AQ_CONFIGURE_PROMISC        (u32)(1 << 9)
#define IXLV_FLAG_AQ_GET_STATS                (u32)(1 << 10)
#define IXLV_FLAG_AQ_CONFIG_RSS_KEY           (u32)(1 << 11)
#define IXLV_FLAG_AQ_SET_RSS_HENA             (u32)(1 << 12)
#define IXLV_FLAG_AQ_GET_RSS_HENA_CAPS        (u32)(1 << 13)
#define IXLV_FLAG_AQ_CONFIG_RSS_LUT           (u32)(1 << 14)

/* printf %b flag args */
#define IXLV_FLAGS \
    "\20\1ENABLE_QUEUES\2DISABLE_QUEUES\3ADD_MAC_FILTER" \
    "\4ADD_VLAN_FILTER\5DEL_MAC_FILTER\6DEL_VLAN_FILTER" \
    "\7CONFIGURE_QUEUES\10MAP_VECTORS\11HANDLE_RESET" \
    "\12CONFIGURE_PROMISC\13GET_STATS\14CONFIG_RSS_KEY" \
    "\15SET_RSS_HENA\16GET_RSS_HENA_CAPS\17CONFIG_RSS_LUT"
#define IXLV_PRINTF_VF_OFFLOAD_FLAGS \
    "\20\1L2" \
    "\2IWARP" \
    "\3RSVD" \
    "\4RSS_AQ" \
    "\5RSS_REG" \
    "\6WB_ON_ITR" \
    "\7REQ_QUEUES" \
    "\21VLAN" \
    "\22RX_POLLING" \
    "\23RSS_PCTYPE_V2" \
    "\24RSS_PF" \
    "\25ENCAP" \
    "\26ENCAP_CSUM" \
    "\27RX_ENCAP_CSUM"

MALLOC_DECLARE(M_IXLV);

/* Driver state */
enum ixlv_state_t {
	IXLV_RESET_REQUIRED,
	IXLV_RESET_PENDING,
	IXLV_INIT_READY,
	IXLV_RUNNING,
};

/* Structs */

struct ixlv_mac_filter {
	SLIST_ENTRY(ixlv_mac_filter)  next;
	u8      macaddr[ETHER_ADDR_LEN];
	u16     flags;
};
SLIST_HEAD(mac_list, ixlv_mac_filter);

struct ixlv_vlan_filter {
	SLIST_ENTRY(ixlv_vlan_filter)  next;
	u16     vlan;
	u16     flags;
};
SLIST_HEAD(vlan_list, ixlv_vlan_filter);

/* Software controller structure */
struct ixlv_sc {
	struct ixl_vsi		vsi;

	struct i40e_hw		hw;
	struct i40e_osdep	osdep;
	device_t		dev;

	struct resource		*pci_mem;

	enum ixlv_state_t	init_state;

	struct ifmedia		media;
	struct virtchnl_version_info	version;
	enum ixl_dbg_mask	dbg_mask;
	u16			promisc_flags;

	bool				link_up;
	enum virtchnl_link_speed	link_speed;

	/* Tunable settings */
	int			tx_itr;
	int			rx_itr;
	int			dynamic_tx_itr;
	int			dynamic_rx_itr;

	/* Filter lists */
	struct mac_list		*mac_filters;
	struct vlan_list	*vlan_filters;

	/* Virtual comm channel */
	struct virtchnl_vf_resource *vf_res;
	struct virtchnl_vsi_resource *vsi_res;

	/* Misc stats maintained by the driver */
	u64			admin_irq;

	/* Buffer used for reading AQ responses */
	u8			aq_buffer[IXL_AQ_BUF_SZ];

	/* State flag used in init/stop */
	u32			queues_enabled;
	u8			enable_queues_chan;
	u8			disable_queues_chan;
};

/*
** This checks for a zero mac addr, something that will be likely
** unless the Admin on the Host has created one.
*/
static inline bool
ixlv_check_ether_addr(u8 *addr)
{
	bool status = TRUE;

	if ((addr[0] == 0 && addr[1]== 0 && addr[2] == 0 &&
	    addr[3] == 0 && addr[4]== 0 && addr[5] == 0))
		status = FALSE;
	return (status);
}

/* Debug printing */
#define ixlv_dbg(sc, m, s, ...)		ixl_debug_core(sc->dev, sc->dbg_mask, m, s, ##__VA_ARGS__)
#define ixlv_dbg_init(sc, s, ...)	ixl_debug_core(sc->dev, sc->dbg_mask, IXLV_DBG_INIT, s, ##__VA_ARGS__)
#define ixlv_dbg_info(sc, s, ...)	ixl_debug_core(sc->dev, sc->dbg_mask, IXLV_DBG_INFO, s, ##__VA_ARGS__)
#define ixlv_dbg_vc(sc, s, ...)		ixl_debug_core(sc->dev, sc->dbg_mask, IXLV_DBG_VC, s, ##__VA_ARGS__)
#define ixlv_dbg_filter(sc, s, ...)	ixl_debug_core(sc->dev, sc->dbg_mask, IXLV_DBG_FILTER, s, ##__VA_ARGS__)

/*
** VF Common function prototypes
*/
void	ixlv_if_init(if_ctx_t ctx);

int	ixlv_send_api_ver(struct ixlv_sc *);
int	ixlv_verify_api_ver(struct ixlv_sc *);
int	ixlv_send_vf_config_msg(struct ixlv_sc *);
int	ixlv_get_vf_config(struct ixlv_sc *);
void	ixlv_init(void *);
int	ixlv_reinit_locked(struct ixlv_sc *);
int	ixlv_configure_queues(struct ixlv_sc *);
int	ixlv_enable_queues(struct ixlv_sc *);
int	ixlv_disable_queues(struct ixlv_sc *);
int	ixlv_map_queues(struct ixlv_sc *);
void	ixlv_enable_intr(struct ixl_vsi *);
void	ixlv_disable_intr(struct ixl_vsi *);
int	ixlv_add_ether_filters(struct ixlv_sc *);
int	ixlv_del_ether_filters(struct ixlv_sc *);
int	ixlv_request_stats(struct ixlv_sc *);
int	ixlv_request_reset(struct ixlv_sc *);
void	ixlv_vc_completion(struct ixlv_sc *,
	enum virtchnl_ops, enum virtchnl_status_code,
	u8 *, u16);
int	ixlv_add_ether_filter(struct ixlv_sc *);
int	ixlv_add_vlans(struct ixlv_sc *);
int	ixlv_del_vlans(struct ixlv_sc *);
void	ixlv_update_stats_counters(struct ixlv_sc *,
		    struct i40e_eth_stats *);
void	ixlv_update_link_status(struct ixlv_sc *);
int	ixlv_get_default_rss_key(u32 *, bool);
int	ixlv_config_rss_key(struct ixlv_sc *);
int	ixlv_set_rss_hena(struct ixlv_sc *);
int	ixlv_config_rss_lut(struct ixlv_sc *);
int	ixlv_config_promisc_mode(struct ixlv_sc *);

int	ixl_vc_send_cmd(struct ixlv_sc *sc, uint32_t request);
char	*ixlv_vc_speed_to_string(enum virtchnl_link_speed link_speed);
void 	*ixl_vc_get_op_chan(struct ixlv_sc *sc, uint32_t request);
#endif /* _IXLV_H_ */
