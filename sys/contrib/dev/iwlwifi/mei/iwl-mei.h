/*-
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _IWL_MEI_IWL_MEI_H
#define	_IWL_MEI_IWL_MEI_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>

enum mei_nvm_caps {
	MEI_NVM_CAPS_11AX_SUPPORT	= BIT(0),
	MEI_NVM_CAPS_LARI_SUPPORT	= BIT(1),
};

struct iwl_mei_nvm {
	uint8_t				n_hw_addrs;
	enum mei_nvm_caps		caps;
	uint32_t			nvm_version;
	uint32_t			radio_cfg;
	uint32_t			channels[110 /* IWL_NVM_NUM_CHANNELS_UHB */];
};

struct iwl_mei_conn_info {
	uint8_t				lp_state;
	uint8_t				band;
	uint8_t				channel;
	uint8_t				ssid_len;
	uint8_t				bssid[ETH_ALEN];
	uint8_t				ssid[IEEE80211_MAX_SSID_LEN];
};

struct iwl_mei_ops {
	void (*me_conn_status)(void *, const struct iwl_mei_conn_info *);
	void (*nic_stolen)(void *);
	void (*rfkill)(void *, bool);
	void (*roaming_forbidden)(void *, bool);
	void (*sap_connected)(void *);
};

#if IS_ENABLED(CONFIG_IWLMEI)
#error No MEI support in FreeBSD currently
#else

static __inline void
iwl_mei_device_down(void)
{
}

static __inline struct iwl_mei_nvm *
iwl_mei_get_nvm(void)
{
        return (NULL);
}

static __inline int
iwl_mei_get_ownership(void)
{
	return (0);
}

static __inline void
iwl_mei_host_disassociated(void)
{
}

static __inline bool
iwl_mei_is_connected(void)
{
        return (false);
}

static __inline void
iwl_mei_set_country_code(uint16_t mcc __unused)
{
}

static __inline void
iwl_mei_set_netdev(struct net_device *netdevice __unused)
{
}

static __inline void
iwl_mei_set_nic_info(const uint8_t *addr __unused, const uint8_t *hw_addr __unused)
{
}

static __inline void
iwl_mei_set_rfkill_state(bool rf_killed __unused, bool sw_rfkill __unused)
{
}

static __inline void
iwl_mei_tx_copy_to_csme(struct sk_buff *skb __unused, unsigned int ivlen __unused)
{
}

static __inline int
iwl_mei_register(void *mvm __unused, const struct iwl_mei_ops *ops __unused)
{
	return (0);
}

static __inline void
iwl_mei_start_unregister(void)
{
}

static __inline void
iwl_mei_unregister_complete(void)
{
}

static __inline void
iwl_mei_device_state(bool up __unused)
{
}

static __inline void
iwl_mei_alive_notif(bool x __unused)
{
}

static __inline bool
iwl_mei_pldr_req(void)
{
	return (false);
}
#endif

#endif /* _IWL_MEI_IWL_MEI_H */
