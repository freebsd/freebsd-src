/*
 * $Id: common.h,v 1.1.1.1 1998/08/21 03:17:42 msmith Exp $
 * From: $NetBSD: common.h,v 1.2 1998/01/05 07:02:48 perry Exp $	
 */

int prom_open __P((char*, int));
void OSFpal __P((void));
void halt __P((void));
u_int64_t prom_dispatch __P((int, ...));
int cpu_number __P((void));
void switch_palcode __P((void));
