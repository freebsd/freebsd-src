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
 * $FreeBSD: src/usr.sbin/pim6sd/kern.h,v 1.1.2.1 2000/07/15 07:36:36 kris Exp $
 */


#ifndef KERN_H 
#define KERN_H
#include "vif.h"
#include "mrt.h"

extern void     k_set_rcvbuf    __P((int socket, int bufsize, int minsize));
extern void     k_set_hlim       __P((int socket, int t));
extern void     k_set_loop      __P((int socket, int l));
extern void     k_set_if        __P((int socket, u_int ifindex));
extern void     k_join          __P((int socket, struct in6_addr *grp,
                     u_int ifindex));
extern void     k_leave         __P((int socket, struct in6_addr *grp,
                     u_int ifindex));
extern void     k_init_pim     __P((int));
extern void     k_stop_pim      __P((int));
extern int      k_del_mfc       __P((int socket, struct sockaddr_in6 *source,
                     struct sockaddr_in6 *group));
extern int      k_chg_mfc       __P((int socket, struct sockaddr_in6 *source,
                     struct sockaddr_in6 *group, vifi_t iif, 
                     if_set *oifs, struct sockaddr_in6 *rp_addr));
extern void     k_add_vif       __P((int socket, vifi_t vifi, struct uvif *v));
extern void     k_del_vif       __P((int socket, vifi_t vifi));
extern int      k_get_vif_count __P((vifi_t vifi, struct vif_count *retval));
extern int      k_get_sg_cnt    __P((int socket, struct sockaddr_in6 *source,
                     struct sockaddr_in6 *group,
                     struct sg_count *retval));



#endif
