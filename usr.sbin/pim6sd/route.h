/*
 * Copyright (C) 1999 LSIIT Laboratory.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  Questions concerning this software should be directed to
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
 * $FreeBSD: src/usr.sbin/pim6sd/route.h,v 1.1.2.1 2000/07/15 07:36:37 kris Exp $
 */


#ifndef ROUTE_H
#define ROUTE_H

#include "mrt.h"

extern u_int32 default_source_preference;
extern u_int32 default_source_metric;

int change_interfaces(	mrtentry_t *mrtentry_ptr,vifi_t new_iif,
						if_set *new_joined_oifs,if_set *new_pruned_oifs,if_set *new_leaves_ , if_set *asserted ,
						u_int16 flags);

extern void      process_kernel_call     __P((void));
extern int  set_incoming        __P((srcentry_t *srcentry_ptr,
                         int srctype));
extern vifi_t   get_iif         __P((struct sockaddr_in6 *source));
extern int  add_sg_oif      __P((mrtentry_t *mrtentry_ptr,
                         vifi_t vifi,
                         u_int16 holdtime,
                         int update_holdtime));
extern void add_leaf        __P((vifi_t vifi, struct sockaddr_in6 *source,
                         struct sockaddr_in6 *group));
extern void delete_leaf     __P((vifi_t vifi, struct sockaddr_in6 *source, 
                         struct sockaddr_in6 *group));




extern pim_nbr_entry_t *find_pim6_nbr __P((struct sockaddr_in6 *source));
extern void calc_oifs       __P((mrtentry_t *mrtentry_ptr,
                         if_set *oifs_ptr));
extern void process_kernel_call __P((void));
extern int  delete_vif_from_mrt __P((vifi_t vifi));
extern mrtentry_t *switch_shortest_path __P((struct sockaddr_in6 *source, struct sockaddr_in6 *group));


#endif
