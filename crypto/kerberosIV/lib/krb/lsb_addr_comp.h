/*
 * $Id: lsb_addr_comp.h,v 1.6 1996/10/05 00:18:02 joda Exp $
 *
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Comparison macros to emulate LSBFIRST comparison results of network
 * byte-order quantities
 */

#ifndef LSB_ADDR_COMP_DEFS
#define LSB_ADDR_COMP_DEFS

/* Compare x and y in VAX byte order, result is -1, 0 or 1. */

#define krb_lsb_antinet_ulong_less(x, y) (((x) == (y)) ? 0 :  krb_lsb_antinet_ulong_cmp(x, y))

#define krb_lsb_antinet_ushort_less(x, y) (((x) == (y)) ? 0 : krb_lsb_antinet_ushort_cmp(x, y))

int krb_lsb_antinet_ulong_cmp(u_int32_t x, u_int32_t y);
int krb_lsb_antinet_ushort_cmp(u_int16_t x, u_int16_t y);
u_int32_t lsb_time(time_t t, struct sockaddr_in *src, struct sockaddr_in *dst);

#endif /*  LSB_ADDR_COMP_DEFS */
