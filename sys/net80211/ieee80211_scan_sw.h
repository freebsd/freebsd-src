/*-
 * Copyright (c) 2015 Adrian Chadd <adrian@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef	__NET80211_IEEE80211_SCAN_SW_H__
#define	__NET80211_IEEE80211_SCAN_SW_H__

extern	void ieee80211_swscan_attach(struct ieee80211com *ic);
extern	void ieee80211_swscan_detach(struct ieee80211com *ic);

extern	void ieee80211_swscan_vattach(struct ieee80211vap *vap);
extern	void ieee80211_swscan_vdetach(struct ieee80211vap *vap);

extern	int ieee80211_swscan_start_scan(const struct ieee80211_scanner *scan,
	    struct ieee80211vap *vap, int flags,
	    u_int duration, u_int mindwell, u_int maxdwell,
	    u_int nssid, const struct ieee80211_scan_ssid ssids[]);
extern	void ieee80211_swscan_set_scan_duration(struct ieee80211vap *vap,
	    u_int duration);
extern	void ieee80211_swscan_run_scan_task(struct ieee80211vap *vap);
extern	int ieee80211_swscan_check_scan(const struct ieee80211_scanner *scan,
	    struct ieee80211vap *vap, int flags,
	    u_int duration, u_int mindwell, u_int maxdwell,
	    u_int nssid, const struct ieee80211_scan_ssid ssids[]);
extern	int ieee80211_swscan_bg_scan(const struct ieee80211_scanner *scan,
	    struct ieee80211vap *vap, int flags);
extern	void ieee80211_swscan_cancel_scan(struct ieee80211vap *vap);
extern	void ieee80211_swscan_cancel_anyscan(struct ieee80211vap *vap);
extern	void ieee80211_swscan_scan_next(struct ieee80211vap *vap);
extern	void ieee80211_swscan_scan_done(struct ieee80211vap *vap);
extern	void ieee80211_swscan_probe_curchan(struct ieee80211vap *vap,
	    int force);
extern	void ieee80211_swscan_add_scan(struct ieee80211vap *vap,
	    struct ieee80211_channel *curchan,
	    const struct ieee80211_scanparams *sp,
	    const struct ieee80211_frame *wh,
	    int subtype, int rssi, int noise);

#endif	/* __NET80211_IEEE80211_SCAN_SW_H__ */
