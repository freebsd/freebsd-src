/*-
 * Copyright (c) 2020-2021 The FreeBSD Foundation
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

#ifndef _IWL_DEBUG_H
#define	_IWL_DEBUG_H

#if defined(__FreeBSD__)
#ifdef CONFIG_IWLWIFI_DEBUG
#include <sys/types.h>
#endif
#endif

#include <linux/device.h>

enum iwl_dl {
	IWL_DL_ASSOC		= 0x00000001,
	IWL_DL_CALIB		= 0x00000002,
	IWL_DL_COEX		= 0x00000004,
	IWL_DL_WOWLAN		= 0x00000008,
	IWL_DL_DROP		= 0x00000010,
	IWL_DL_EEPROM		= 0x00000020,
	IWL_DL_FW		= 0x00000040,
	/*			= 0x00000080, */
	IWL_DL_HC		= 0x00000100,
	IWL_DL_HT		= 0x00000200,
	IWL_DL_INFO		= 0x00000400,
	IWL_DL_ISR		= 0x00000800,
	IWL_DL_LAR		= 0x00001000,
	IWL_DL_MAC80211		= 0x00002000,
	IWL_DL_POWER		= 0x00004000,
	IWL_DL_QUOTA		= 0x00008000,
	IWL_DL_RADIO		= 0x00010000,
	IWL_DL_RATE		= 0x00020000,
	IWL_DL_RF_KILL		= 0x00040000,
	IWL_DL_RX		= 0x00080000,
	IWL_DL_SCAN		= 0x00100000,
	IWL_DL_STATS		= 0x00200000,
	/*			= 0x00400000, */
	IWL_DL_TDLS		= 0x00800000,
	IWL_DL_TE		= 0x01000000,
	IWL_DL_TEMP		= 0x02000000,
	IWL_DL_TPT		= 0x04000000,
	IWL_DL_TX		= 0x08000000,
	IWL_DL_TX_QUEUES	= 0x10000000,
	IWL_DL_TX_REPLY		= 0x20000000,
	IWL_DL_WEP		= 0x40000000,

	IWL_DL_PCI_RW		= 0x80000000,

	IWL_DL_ANY		= 0x7fffffff,
};

enum iwl_err_mode {
	IWL_ERR_MODE_RATELIMIT,
	IWL_ERR_MODE_REGULAR,
	IWL_ERR_MODE_RFKILL,	/* XXX we do not pass that from anywhere? */
};

void __iwl_crit(struct device *, const char *, ...);
void __iwl_info(struct device *, const char *, ...);
void __iwl_warn(struct device *, const char *, ...);
void __iwl_err(struct device *, enum iwl_err_mode, const char *, ...);

#define	IWL_CRIT(_subsys, _fmt, ...)					\
	__iwl_crit((_subsys)->dev, _fmt, ##__VA_ARGS__)
#define	IWL_INFO(_subsys, _fmt, ...)					\
	__iwl_info((_subsys)->dev, _fmt, ##__VA_ARGS__)
#define	IWL_WARN(_subsys, _fmt, ...)					\
	__iwl_warn((_subsys)->dev, _fmt, ##__VA_ARGS__)
/* XXX Not sure what the two bools are good for if never passed. */
#define	__IWL_ERR_DEV(_dev, _mode, _fmt, ...)				\
	__iwl_err((_dev), IWL_ERR_MODE_REGULAR, _fmt, ##__VA_ARGS__)
#define	IWL_ERR_DEV(_dev, _fmt, ...)					\
	__IWL_ERR_DEV(_dev, IWL_ERR_MODE_REGULAR, _fmt, ##__VA_ARGS__)
#define	IWL_ERR(_subsys, _fmt, ...)					\
	IWL_ERR_DEV((_subsys)->dev, _fmt, ##__VA_ARGS__)
#define	IWL_ERR_LIMIT(_subsys, _fmt, ...)				\
	__IWL_ERR_DEV((_subsys)->dev, IWL_ERR_MODE_RATELIMIT,		\
	    _fmt, ##__VA_ARGS__)

#define	iwl_print_hex_error(_subsys, _pkt, _n)		/* XXX-BZ TODO */

#ifdef CONFIG_IWLWIFI_DEBUG
bool iwl_have_debug_level(enum iwl_dl);
void iwl_print_hex_dump(void *, enum iwl_dl, const char *, uint8_t *, size_t);
void __iwl_dbg(struct device *, u32, bool, const char *, const char *fmt, ...);

#define	IWL_DPRINTF_DEV_PREFIX(_dev, _e, _prefix, _fmt, ...)		\
	__iwl_dbg(_dev, _e, false, __func__, #_prefix " " _fmt, ##__VA_ARGS__)
#define	IWL_DPRINTF_DEV(_dev, _e, _fmt, ...)				\
	IWL_DPRINTF_DEV_PREFIX(_dev, _e, _e, _fmt, ##__VA_ARGS__)
#define	IWL_DPRINTF(_subsys, _e, _fmt, ...)				\
	IWL_DPRINTF_DEV((_subsys)->dev, _e, _fmt, ##__VA_ARGS__)
#define	IWL_DPRINTF_PREFIX(_subsys, _e, _prefix, _fmt, ...)		\
	IWL_DPRINTF_DEV_PREFIX((_subsys)->dev, _e, _prefix, _fmt, ##__VA_ARGS__)

#else /* !CONFIG_IWLWIFI_DEBUG */
#define	IWL_DPRINTF_DEV(_dev, _e, _fmt, ...)
#define	IWL_DPRINTF(_subsys, _e, _fmt, ...)
#define	IWL_DPRINTF_PREFIX(_subsys, _e, _prefix, _fmt, ...)
#endif /* CONFIG_IWLWIFI_DEBUG */

#define	IWL_DEBUG_ASSOC(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_ASSOC, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_CALIB(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_CALIB, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_COEX(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_COEX, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_DEV(_dev, _level, _fmt, ...)				\
	IWL_DPRINTF_DEV((_dev), (_level), _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_DROP(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_DROP, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_EEPROM(_dev, _fmt, ...)				\
	IWL_DPRINTF_DEV((_dev), IWL_DL_EEPROM, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_FW(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_FW, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_FW_INFO(_subsys, _fmt, ...)				\
	IWL_DPRINTF_PREFIX(_subsys, IWL_DL_FW | IWL_DL_INFO, IWL_DL_FW_INFO, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_HC(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_HC, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_HT(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_HT, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_INFO(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_INFO, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_ISR(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_ISR, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_LAR(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_LAR, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_MAC80211(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_MAC80211, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_POWER(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_POWER, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_QUOTA(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_QUOTA, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_RADIO(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_RADIO, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_RATE(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_RATE, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_RF_KILL(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_RF_KILL, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_RX(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_RX, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_SCAN(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_SCAN, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_STATS(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_STATS, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_STATS_LIMIT(_subsys, _fmt, ...)			\
	IWL_DPRINTF(_subsys, IWL_DL_STATS, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_TDLS(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_TDLS, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_TE(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_TE, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_TEMP(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_TEMP, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_TPT(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_TPT, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_TX(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_TX, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_TX_QUEUES(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_TX_QUEUES, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_TX_REPLY(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_TX_REPLY, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_WEP(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_WEP, _fmt, ##__VA_ARGS__)
#define	IWL_DEBUG_WOWLAN(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_WOWLAN, _fmt, ##__VA_ARGS__)

#define	IWL_DEBUG_PCI_RW(_subsys, _fmt, ...)				\
	IWL_DPRINTF(_subsys, IWL_DL_PCI_RW, _fmt, ##__VA_ARGS__)

#endif /* _IWL_DEBUG_H */
