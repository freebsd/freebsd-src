/*
 * natd - Network Address Translation Daemon for FreeBSD.
 *
 * This software is provided free of charge, with no 
 * warranty of any kind, either expressed or implied.
 * Use at your own risk.
 * 
 * You may copy, modify and distribute this software (natd.h) freely.
 *
 * Ari Suutari <suutari@iki.fi>
 *
 * $FreeBSD: src/sbin/natd/natd.h,v 1.5.30.1 2010/02/10 00:26:20 kensmith Exp $
 */

#define PIDFILE	"/var/run/natd.pid"
#define	INPUT		1
#define	OUTPUT		2
#define	DONT_KNOW	3

extern void Quit (const char* msg);
extern void Warn (const char* msg);
extern int SendNeedFragIcmp (int sock, struct ip* failedDgram, int mtu);
extern struct libalias *mla;

