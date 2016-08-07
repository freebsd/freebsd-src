/******************************************************************************

  Copyright (c) 2013-2015, Intel Corporation 
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


#ifndef _IXL_PF_H_
#define _IXL_PF_H_

#include "ixl.h"
#include "ixl_pf_qmgr.h"

#define	VF_FLAG_ENABLED			0x01
#define	VF_FLAG_SET_MAC_CAP		0x02
#define	VF_FLAG_VLAN_CAP		0x04
#define	VF_FLAG_PROMISC_CAP		0x08
#define	VF_FLAG_MAC_ANTI_SPOOF		0x10

#define IXL_PF_STATE_EMPR_RESETTING	(1 << 0)

struct ixl_vf {
	struct ixl_vsi		vsi;
	uint32_t		vf_flags;

	uint8_t			mac[ETHER_ADDR_LEN];
	uint16_t		vf_num;
	uint32_t		version;

	struct ixl_pf_qtag	qtag;
	struct sysctl_ctx_list	ctx;
};

/* Physical controller structure */
struct ixl_pf {
	struct i40e_hw		hw;
	struct i40e_osdep	osdep;
	struct device		*dev;
	struct ixl_vsi		vsi;

	struct resource		*pci_mem;
	struct resource		*msix_mem;

	/*
	 * Interrupt resources: this set is
	 * either used for legacy, or for Link
	 * when doing MSIX
	 */
	void			*tag;
	struct resource 	*res;

	struct callout		timer;
	int			msix;
	int			if_flags;
	int			state;

	struct ixl_pf_qmgr	qmgr;
	struct ixl_pf_qtag	qtag;

	/* Tunable values */
	bool			enable_msix;
	int			max_queues;
	int			ringsz;
	bool			enable_tx_fc_filter;
	int			dynamic_rx_itr;
	int			dynamic_tx_itr;
	int			tx_itr;
	int			rx_itr;

	struct mtx		pf_mtx;

	u32			qbase;
	u32 			admvec;
	struct task     	adminq;
	struct taskqueue	*tq;

	bool			link_up;
	u32			link_speed;
	int			advertised_speed;
	int			fc; /* link flow ctrl setting */
	enum ixl_dbg_mask	dbg_mask;

	/* Misc stats maintained by the driver */
	u64			watchdog_events;
	u64			admin_irq;

	/* Statistics from hw */
	struct i40e_hw_port_stats 	stats;
	struct i40e_hw_port_stats	stats_offsets;
	bool 				stat_offsets_loaded;

	/* SR-IOV */
	struct ixl_vf		*vfs;
	int			num_vfs;
	uint16_t		veb_seid;
	struct task		vflr_task;
	int			vc_debug_lvl;
};

/*
 * Defines used for NVM update ioctls.
 * This value is used in the Solaris tool, too.
 */
#define I40E_NVM_ACCESS \
     (((((((('E' << 4) + '1') << 4) + 'K') << 4) + 'G') << 4) | 5)

#define IXL_DEFAULT_PHY_INT_MASK \
     ((~(I40E_AQ_EVENT_LINK_UPDOWN | I40E_AQ_EVENT_MODULE_QUAL_FAIL \
      | I40E_AQ_EVENT_MEDIA_NA)) & 0x3FF)

/*** Sysctl help messages; displayed with "sysctl -d" ***/

#define IXL_SYSCTL_HELP_SET_ADVERTISE	\
"\nControl advertised link speed.\n"	\
"Flags:\n"				\
"\t 0x1 - advertise 100M\n"		\
"\t 0x2 - advertise 1G\n"		\
"\t 0x4 - advertise 10G\n"		\
"\t 0x8 - advertise 20G\n"		\
"\t0x10 - advertise 40G\n\n"		\
"Set to 0 to disable link."

#define IXL_SYSCTL_HELP_FC				\
"\nSet flow control mode using the values below.\n" 	\
"\t0 - off\n" 						\
"\t1 - rx pause\n" 					\
"\t2 - tx pause\n"					\
"\t3 - tx and rx pause"

#define IXL_SYSCTL_HELP_LINK_STATUS					\
"\nExecutes a \"Get Link Status\" command on the Admin Queue, and displays" \
" the response."			\

static char *ixl_fc_string[6] = {
	"None",
	"Rx",
	"Tx",
	"Full",
	"Priority",
	"Default"
};

static MALLOC_DEFINE(M_IXL, "ixl", "ixl driver allocations");

/*** Functions / Macros ***/
#define	I40E_VC_DEBUG(pf, level, ...) \
	do { \
		if ((pf)->vc_debug_lvl >= (level)) \
			device_printf((pf)->dev, __VA_ARGS__); \
	} while (0)

#define	i40e_send_vf_nack(pf, vf, op, st) \
	ixl_send_vf_nack_msg((pf), (vf), (op), (st), __FILE__, __LINE__)

#define IXL_PF_LOCK_INIT(_sc, _name) \
        mtx_init(&(_sc)->pf_mtx, _name, "IXL PF Lock", MTX_DEF)
#define IXL_PF_LOCK(_sc)              mtx_lock(&(_sc)->pf_mtx)
#define IXL_PF_UNLOCK(_sc)            mtx_unlock(&(_sc)->pf_mtx)
#define IXL_PF_LOCK_DESTROY(_sc)      mtx_destroy(&(_sc)->pf_mtx)
#define IXL_PF_LOCK_ASSERT(_sc)       mtx_assert(&(_sc)->pf_mtx, MA_OWNED)

/* For stats sysctl naming */
#define QUEUE_NAME_LEN 32

/*
 * PF-only function declarations
 */

void	ixl_set_busmaster(device_t);
int	ixl_setup_interface(device_t, struct ixl_vsi *);
void	ixl_print_nvm_cmd(device_t, struct i40e_nvm_access *);

void	ixl_handle_que(void *context, int pending);

void	ixl_init(void *);
void	ixl_local_timer(void *);
void	ixl_register_vlan(void *, struct ifnet *, u16);
void	ixl_unregister_vlan(void *, struct ifnet *, u16);
void	ixl_intr(void *);
void	ixl_msix_que(void *);
void	ixl_msix_adminq(void *);
void	ixl_do_adminq(void *, int);

int	ixl_res_alloc_cmp(const void *, const void *);
char *	ixl_switch_res_type_string(u8);
char *	ixl_switch_element_string(struct sbuf *,
	    struct i40e_aqc_switch_config_element_resp *);
void	ixl_add_sysctls_mac_stats(struct sysctl_ctx_list *,
		    struct sysctl_oid_list *, struct i40e_hw_port_stats *);
void	ixl_add_sysctls_eth_stats(struct sysctl_ctx_list *,
		    struct sysctl_oid_list *,
		    struct i40e_eth_stats *);

void    ixl_media_status(struct ifnet *, struct ifmediareq *);
int     ixl_media_change(struct ifnet *);
int     ixl_ioctl(struct ifnet *, u_long, caddr_t);

void    ixl_enable_adminq(struct i40e_hw *);
void	ixl_get_bus_info(struct i40e_hw *, device_t);
void	ixl_disable_adminq(struct i40e_hw *);
void	ixl_enable_queue(struct i40e_hw *, int);
void	ixl_disable_queue(struct i40e_hw *, int);
void	ixl_enable_legacy(struct i40e_hw *);
void	ixl_disable_legacy(struct i40e_hw *);
void	ixl_nvm_version_str(struct i40e_hw *hw, struct sbuf *buf);
void	ixl_stat_update48(struct i40e_hw *, u32, u32, bool,
		    u64 *, u64 *);
void	ixl_stat_update32(struct i40e_hw *, u32, bool,
		    u64 *, u64 *);

void	ixl_stop(struct ixl_pf *);
void	ixl_add_vsi_sysctls(struct ixl_pf *pf, struct ixl_vsi *vsi, struct sysctl_ctx_list *ctx, const char *sysctl_name);
int	ixl_get_hw_capabilities(struct ixl_pf *);
void    ixl_update_link_status(struct ixl_pf *);
int     ixl_allocate_pci_resources(struct ixl_pf *);
int	ixl_setup_stations(struct ixl_pf *);
int	ixl_switch_config(struct ixl_pf *);
void	ixl_stop_locked(struct ixl_pf *);
int	ixl_teardown_hw_structs(struct ixl_pf *);
int	ixl_reset(struct ixl_pf *);
void	ixl_init_locked(struct ixl_pf *);
void	ixl_set_rss_key(struct ixl_pf *);
void	ixl_set_rss_pctypes(struct ixl_pf *);
void	ixl_set_rss_hlut(struct ixl_pf *);
int	ixl_setup_adminq_msix(struct ixl_pf *);
int	ixl_setup_adminq_tq(struct ixl_pf *);
int	ixl_teardown_adminq_msix(struct ixl_pf *);
void	ixl_configure_intr0_msix(struct ixl_pf *);
void	ixl_configure_queue_intr_msix(struct ixl_pf *);
void	ixl_free_adminq_tq(struct ixl_pf *);
int	ixl_assign_vsi_legacy(struct ixl_pf *);
int	ixl_init_msix(struct ixl_pf *);
void	ixl_configure_itr(struct ixl_pf *);
void	ixl_configure_legacy(struct ixl_pf *);
void	ixl_free_pci_resources(struct ixl_pf *);
void	ixl_link_event(struct ixl_pf *, struct i40e_arq_event_info *);
void	ixl_config_rss(struct ixl_pf *);
int	ixl_set_advertised_speeds(struct ixl_pf *, int);
void	ixl_get_initial_advertised_speeds(struct ixl_pf *);
void	ixl_print_nvm_version(struct ixl_pf *pf);
void	ixl_add_device_sysctls(struct ixl_pf *);
void	ixl_handle_mdd_event(struct ixl_pf *);
void	ixl_add_hw_stats(struct ixl_pf *);
void	ixl_update_stats_counters(struct ixl_pf *);
void	ixl_pf_reset_stats(struct ixl_pf *);
void	ixl_dbg(struct ixl_pf *, enum ixl_dbg_mask, char *, ...);

int	ixl_handle_nvmupd_cmd(struct ixl_pf *, struct ifdrv *);
void	ixl_handle_empr_reset(struct ixl_pf *);
int	ixl_rebuild_hw_structs_after_reset(struct ixl_pf *);

void	ixl_set_queue_rx_itr(struct ixl_queue *);
void	ixl_set_queue_tx_itr(struct ixl_queue *);

void	ixl_add_filter(struct ixl_vsi *, u8 *, s16 vlan);
void	ixl_del_filter(struct ixl_vsi *, u8 *, s16 vlan);
void	ixl_reconfigure_filters(struct ixl_vsi *vsi);

int	ixl_disable_rings(struct ixl_vsi *);
int	ixl_disable_tx_ring(struct ixl_pf *, struct ixl_pf_qtag *, u16);
int	ixl_disable_rx_ring(struct ixl_pf *, struct ixl_pf_qtag *, u16);
int	ixl_disable_ring(struct ixl_pf *pf, struct ixl_pf_qtag *, u16);

int	ixl_enable_rings(struct ixl_vsi *);
int	ixl_enable_tx_ring(struct ixl_pf *, struct ixl_pf_qtag *, u16);
int	ixl_enable_rx_ring(struct ixl_pf *, struct ixl_pf_qtag *, u16);
int	ixl_enable_ring(struct ixl_pf *pf, struct ixl_pf_qtag *, u16);

void	ixl_update_eth_stats(struct ixl_vsi *);
void	ixl_disable_intr(struct ixl_vsi *);
void	ixl_cap_txcsum_tso(struct ixl_vsi *, struct ifnet *, int);
int	ixl_initialize_vsi(struct ixl_vsi *);
void	ixl_add_ifmedia(struct ixl_vsi *, u32);
int	ixl_setup_queue_msix(struct ixl_vsi *);
int	ixl_setup_queue_tqs(struct ixl_vsi *);
int	ixl_teardown_queue_msix(struct ixl_vsi *);
void	ixl_free_queue_tqs(struct ixl_vsi *);
void	ixl_enable_intr(struct ixl_vsi *);
void	ixl_disable_rings_intr(struct ixl_vsi *);
void	ixl_set_promisc(struct ixl_vsi *);
void	ixl_add_multi(struct ixl_vsi *);
void	ixl_del_multi(struct ixl_vsi *);
void	ixl_setup_vlan_filters(struct ixl_vsi *);
void	ixl_init_filters(struct ixl_vsi *);
void	ixl_add_hw_filters(struct ixl_vsi *, int, int);
void	ixl_del_hw_filters(struct ixl_vsi *, int);
struct ixl_mac_filter *
		ixl_find_filter(struct ixl_vsi *, u8 *, s16);
void	ixl_add_mc_filter(struct ixl_vsi *, u8 *);
void	ixl_free_mac_filters(struct ixl_vsi *vsi);
void	ixl_update_vsi_stats(struct ixl_vsi *);
void	ixl_vsi_reset_stats(struct ixl_vsi *);

#endif /* _IXL_PF_H_ */
