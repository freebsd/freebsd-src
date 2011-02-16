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
 * $FreeBSD: src/sbin/natd/natd.h,v 1.6.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $
 */

#define PIDFILE	"/var/run/natd.pid"
#define	INPUT		1
#define	OUTPUT		2
#define	DONT_KNOW	3

#define EXIT_DELAY	10000
#define MAX_EXIT_DELAY	999999

extern void Quit (const char* msg);
extern void Warn (const char* msg);
extern int SendNeedFragIcmp (int sock, struct ip* failedDgram, int mtu);
extern struct libalias *mla;

