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
 * $FreeBSD: src/usr.sbin/pim6sd/rp.h,v 1.1.2.1 2000/07/15 07:36:37 kris Exp $
 */


#ifndef RP_H
#define RP_H

#include "defs.h"
#include "mrt.h"

extern cand_rp_t				*cand_rp_list;
extern grp_mask_t       *grp_mask_list;
extern cand_rp_t        *segmented_cand_rp_list;
extern grp_mask_t       *segmented_grp_mask_list; 

extern u_int8					cand_rp_flag;
extern u_int8					cand_bsr_flag;
extern u_int8					my_cand_rp_priority;
extern u_int8					my_bsr_priority;
extern u_int16					my_cand_rp_adv_period;
extern u_int16					my_bsr_period;
extern u_int16					my_cand_rp_holdtime;
extern struct sockaddr_in6		my_cand_rp_address;
extern struct sockaddr_in6		my_bsr_address;
extern struct in6_addr			my_bsr_hash_mask;
extern struct in6_addr			curr_bsr_hash_mask;
extern struct sockaddr_in6		curr_bsr_address;
extern u_int16          curr_bsr_fragment_tag;
extern u_int8					curr_bsr_priority;
extern u_int16          pim_bootstrap_timer;
extern u_int16          pim_cand_rp_adv_timer;

extern struct cand_rp_adv_message_ {
	u_int8					*buffer;
	u_int8					*insert_data_ptr;
	u_int8					*prefix_cnt_ptr;
	u_int16					message_size;
} cand_rp_adv_message;


extern void      init_rp6_and_bsr6         __P((void));
void delete_rp_list( cand_rp_t **used_cand_rp_list , grp_mask_t **used_grp_mask_list );
u_int16 bootstrap_initial_delay __P((void));
extern rpentry_t *rp_match      __P((struct sockaddr_in6 *group));
extern rp_grp_entry_t *rp_grp_match __P((struct sockaddr_in6 *group));
extern int  create_pim6_bootstrap_message __P((char *send_buff));

extern rp_grp_entry_t *add_rp_grp_entry __P((cand_rp_t  **used_cand_rp_list,
                         grp_mask_t **used_grp_mask_list,
                         struct sockaddr_in6 *rp_addr,
                         u_int8  rp_priority,
                         u_int16 rp_holdtime,
                         struct sockaddr_in6 *group_addr,
                         struct in6_addr group_mask,
                         struct in6_addr bsr_hash_mask,
                         u_int16 fragment_tag));
extern void delete_rp_grp_entry __P((cand_rp_t  **used_cand_rp_list,
                         grp_mask_t **used_grp_mask_list,
                         rp_grp_entry_t *rp_grp_entry_delete));
extern void delete_grp_mask     __P((cand_rp_t  **used_cand_rp_list, 
                         grp_mask_t **used_grp_mask_list,  
                         struct sockaddr_in6 *group_addr,
                         struct in6_addr group_mask));
extern void delete_rp       __P((cand_rp_t  **used_cand_rp_list,
                         grp_mask_t **used_grp_mask_list,
                         struct sockaddr_in6 *rp_addr));
extern void delete_rp_list      __P((cand_rp_t  **used_cand_rp_list,
                         grp_mask_t **used_grp_mask_list));
extern rpentry_t *rp_match      __P((struct sockaddr_in6 *group)); 
extern rp_grp_entry_t *rp_grp_match __P((struct sockaddr_in6 *group));
extern rpentry_t *rp_find       __P((struct sockaddr_in6 *rp_address));
extern int  remap_grpentry      __P((grpentry_t *grpentry_ptr));
extern int  check_mrtentry_rp   __P((mrtentry_t *mrtentry_ptr,
                         struct sockaddr_in6 *rp_addr));




#endif
