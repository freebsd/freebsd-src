/* $FreeBSD$ */
/*	$NetBSD: $	*/

/*-
 * Copyright (c) 2003, 2004 David Young.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID YOUNG ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DAVID
 * YOUNG BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
#ifndef _NET_IF_IEEE80211RADIOTAP_H_
#define _NET_IF_IEEE80211RADIOTAP_H_

/* A generic radio capture format is desirable. There is one for
 * Linux, but it is neither rigidly defined (there were not even
 * units given for some fields) nor easily extensible.
 *
 * I suggest the following extensible radio capture format. It is
 * based on a bitmap indicating which fields are present.
 *
 * I am trying to describe precisely what the application programmer
 * should expect in the following, and for that reason I tell the
 * units and origin of each measurement (where it applies), or else I
 * use sufficiently weaselly language ("is a monotonically nondecreasing
 * function of...") that I cannot set false expectations for lawyerly
 * readers.
 */
#ifndef DLT_IEEE802_11_RADIO
#define	DLT_IEEE802_11_RADIO	127	/* 802.11 plus WLAN header */
#endif

/* The radio capture header precedes the 802.11 header. */
struct ieee80211_radiotap_header {
	u_int8_t	it_version;	/* Version 0. Only increases
					 * for drastic changes,
					 * introduction of compatible
					 * new LTVs does not count.
					 */
	u_int8_t	it_pad;
	u_int16_t       it_len;         /* length of the whole
					 * header in bytes, including
					 * it_version, it_pad,
					 * it_len, and LTVs.
					 */
	u_int32_t       it_present;     /* A bitmap telling which
					 * fields are present. Set bit 31
					 * (0x80000000) to extend the
					 * bitmap by another 32 bits.
					 * Additional extensions are made
					 * by setting bit 31.
					 */
};

/* Name                                 Data type       Units
 * ----                                 ---------       -----
 *
 * IEEE80211_RADIOTAP_CHANNEL           u_int8_t	MHz
 *
 *      Tx/Rx channel number (for DSSS/OFDM/PBCC PHY)
 *
 * IEEE80211_RADIOTAP_FHSS              u_int16_t       see below
 *
 *      For frequency-hopping radios, the hop set (first byte)
 *      and pattern (second byte).
 *
 * IEEE80211_RADIOTAP_RATE              u_int8_t	.5Mb/s
 *
 *      IEEE Tx/Rx data rate cookie (including basic rate flag)
 *
 * IEEE80211_RADIOTAP_DB_ANTSIGNAL      int16_t         decibel (dB)
 *              
 *      RF signal power at the antenna, measured from a fixed,
 *      arbitrary reference point.
 *
 * IEEE80211_RADIOTAP_DB_ANTNOISE       int16_t         decibel (dB)
 *
 *      RF noise power at the antenna in decibels from an arbitrary,
 *      fixed reference point.
 *
 * IEEE80211_RADIOTAP_BARKER_CODE_LOCK  u_int16_t       unitless
 *
 *      Quality of Barker code lock. Unitless. Monotonically
 *      nondecreasing with "better" lock strength. Called "Signal
 *      Quality" in datasheets.  (Is there a standard way to measure
 *      this?)
 *
 * IEEE80211_RADIOTAP_TX_ATTENUATION    u_int16_t       unitless
 *
 *      Transmit power expressed as unitless distance from max
 *      power set at factory calibration.  0 is max power.
 *      Monotonically nondecreasing with lower power levels.
 *
 * IEEE80211_RADIOTAP_DB_TX_ATTENUATION u_int16_t       decibels (dB)
 *
 *      Transmit power expressed as decibel distance from max power
 *      set at factory calibration.  0 is max power.  Monotonically
 *      nondecreasing with lower power levels.
 *
 * IEEE80211_RADIOTAP_DBM_TX_POWER      u_int16_t       decibels from
 *                                                      milliwatt (dBm)
 *
 *      Transmit power expressed as dBm (decibels from a 1 milliwatt
 *      reference). This is the absolute power level measured at
 *      the antenna port.
 *
 * IEEE80211_RADIOTAP_TSFT              u_int64_t       microseconds
 *
 *      Value in microseconds of the MAC's 64-bit 802.11 Time
 *      Synchronization Function timer when the first bit of the
 *      MPDU arrived at the MAC. For received frames, only.
 *      Little-endian byte order.
 *
 * IEEE80211_RADIOTAP_FLAGS             u_int16_t       bitmap
 *
 *      Properties of transmitted and received frames. See flags
 *      defined below.
 *
 * IEEE80211_RADIOTAP_TIME              u_int32_t       microseconds
 *
 *      For radios that provide it: packet arrival time in
 *      microseconds. Prism hardware will provide this, but it is
 *      not known whether it marks the first or the last bit of
 *      the frame. Nor is it known where that bit has arrived
 *      (antenna, modem, MAC?) when the time is measured.
 *
 * IEEE80211_RADIOTAP_ANTENNA           u_int16_t       antenna index
 *
 *      Unitless indication of the Rx/Tx antenna for this packet.
 *      The first antenna is antenna 0.
 */
enum ieee80211_radiotap_type {
	IEEE80211_RADIOTAP_PAD = 0,
	IEEE80211_RADIOTAP_FLAGS = 1,
	IEEE80211_RADIOTAP_RATE = 2,
	IEEE80211_RADIOTAP_CHANNEL = 3,
	IEEE80211_RADIOTAP_FHSS = 4,
	IEEE80211_RADIOTAP_DB_ANTSIGNAL = 5,
	IEEE80211_RADIOTAP_DB_ANTNOISE = 6,
	IEEE80211_RADIOTAP_LOCK_QUALITY = 7,
	IEEE80211_RADIOTAP_TX_ATTENUATION = 8,
	IEEE80211_RADIOTAP_DB_TX_ATTENUATION = 9,
	IEEE80211_RADIOTAP_DBM_TX_POWER = 10,
	IEEE80211_RADIOTAP_ANTENNA = 11,
	IEEE80211_RADIOTAP_TSFT = 12,
	IEEE80211_RADIOTAP_EXT = 31,
	IEEE80211_RADIOTAP_TIME = 32
};

/* For IEEE80211_RADIOTAP_FLAGS */
#define	IEEE80211_RADIOTAP_F_CFP	0x0001  /* sent/received
						 * during CFP
						 */
#define	IEEE80211_RADIOTAP_F_SHORTPRE	0x0002  /* sent/received
						 * with short
						 * preamble
						 */
#define	IEEE80211_RADIOTAP_F_WEP	0x0004  /* sent/received
						 * with WEP encryption
						 */
#define	IEEE80211_RADIOTAP_F_FRAG	0x0008  /* sent/received
						 * with fragmentation
						 */

#endif /* _NET_IF_IEEE80211RADIOTAP_H_ */
