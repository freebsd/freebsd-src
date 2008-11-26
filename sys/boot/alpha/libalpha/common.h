/*
 * $FreeBSD: src/sys/boot/alpha/libalpha/common.h,v 1.3.26.1 2008/10/02 02:57:24 kensmith Exp $
 * From: $NetBSD: common.h,v 1.2 1998/01/05 07:02:48 perry Exp $	
 */

int prom_open(char*, int);
void OSFpal(void);
void halt(void);
u_int64_t prom_dispatch(int, ...);
int cpu_number(void);
void switch_palcode(void);
