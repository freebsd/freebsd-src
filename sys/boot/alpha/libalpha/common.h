/*
 * $FreeBSD$
 * From: $NetBSD: common.h,v 1.2 1998/01/05 07:02:48 perry Exp $	
 */

int prom_open(char*, int);
void OSFpal(void);
void halt(void);
u_int64_t prom_dispatch(int, ...);
int cpu_number(void);
void switch_palcode(void);
