/* lookup.h */
/* $FreeBSD: src/libexec/bootpd/lookup.h,v 1.2 2002/05/28 18:31:41 alfred Exp $ */

#include "bptypes.h"	/* for int32, u_int32 */

extern u_char *lookup_hwa(char *hostname, int htype);
extern int lookup_ipa(char *hostname, u_int32 *addr);
extern int lookup_netmask(u_int32 addr, u_int32 *mask);
