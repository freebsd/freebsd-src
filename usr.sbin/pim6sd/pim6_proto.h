/*
 *  Copyright (c) 1998 by the University of Southern California.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Southern
 *  California and/or Information Sciences Institute.
 *  The name of the University of Southern California may not
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  THE UNIVERSITY OF SOUTHERN CALIFORNIA DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL USC, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.        
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.        
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 * $FreeBSD: src/usr.sbin/pim6sd/pim6_proto.h,v 1.1.2.1 2000/07/15 07:36:37 kris Exp $
 */


#ifndef PIM6_PROTO_H
#define PIM6_PROTO_H
#include "defs.h"
#include "vif.h"
#include "mrt.h"

extern build_jp_message_t *build_jp_message_pool;
extern int               build_jp_message_pool_counter;
extern struct sockaddr_in6 sockaddr6_any;
extern struct sockaddr_in6 sockaddr6_d;

extern int receive_pim6_hello         __P((struct sockaddr_in6 *src,
                       char *pim_message, int datalen));

extern int send_pim6_hello            __P((struct uvif *v, u_int16 holdtime));
extern void delete_pim6_nbr           __P((pim_nbr_entry_t *nbr_delete));

extern int  receive_pim6_register    __P((struct sockaddr_in6 *src, struct sockaddr_in6 *dst,
                         char *pim_message, int datalen));
extern int  send_pim6_null_register  __P((mrtentry_t *r));
extern int  receive_pim6_register_stop __P((struct sockaddr_in6 *src, struct sockaddr_in6 *dst,
                           char *pim_message,
                           int datalen));
extern int  send_pim6_register   __P((char *pkt));
extern int  receive_pim6_join_prune  __P((struct sockaddr_in6 *src, struct sockaddr_in6 *dst,
                         char *pim_message, int datalen));
extern int  join_or_prune       __P((mrtentry_t *mrtentry_ptr,  
                         pim_nbr_entry_t *upstream_router));
extern int  receive_pim6_assert  __P((struct sockaddr_in6 *src, struct sockaddr_in6 *dst,
                         char *pim_message, int datalen));
extern int  send_pim6_assert     __P((struct sockaddr_in6 *source, struct sockaddr_in6 *group,
                         vifi_t vifi,
                         mrtentry_t *mrtentry_ptr));
extern int  send_periodic_pim6_join_prune __P((vifi_t vifi, 
                          pim_nbr_entry_t *pim_nbr,
                          u_int16 holdtime));
extern int  add_jp_entry        __P((pim_nbr_entry_t *pim_nbr,
                         u_int16 holdtime, struct sockaddr_in6 *group,
                         u_int8 grp_msklen, struct sockaddr_in6 *source,  
                         u_int8 src_msklen,
                         u_int16 addr_flags,  
                         u_int8 join_prune));
extern void pack_and_send_jp6_message __P((pim_nbr_entry_t *pim_nbr));
extern int  receive_pim6_cand_rp_adv __P((struct sockaddr_in6 *src, struct sockaddr_in6 *dst,
                         char *pim_message, int datalen));
extern int  receive_pim6_bootstrap   __P((struct sockaddr_in6 *src, struct sockaddr_in6 *dst,
                         char *pim_message, int datalen));
extern int  send_pim6_cand_rp_adv    __P((void));
extern void send_pim6_bootstrap  __P((void));


#endif
