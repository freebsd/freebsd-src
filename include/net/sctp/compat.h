/* SCTP kernel reference Implementation
 *
 * (C) Copyright IBM Corp. 2004
 * Copyright (c) 2003 Hewlett-Packard Company
 * 
 * This file is part of the SCTP kernel reference Implementation
 * 
 * This header represents the structures and constants needed to backport
 * lksctp from Linux kernel 2.5 to 2.4 This file also has some code that
 * has been taken from the source base of Linux kernel 2.5 
 * 
 * The SCTP reference implementation is free software; 
 * you can redistribute it and/or modify it under the terms of 
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * The SCTP reference implementation is distributed in the hope that it 
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  
 * 
 */

#ifndef __net_sctp_compat_h__
#define __net_sctp_compat_h__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/seq_file.h>

/*
 * The following defines are for compatibility with 2.6
 */
/*
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define DEFINE_SNMP_STAT(type, name)	\
	type name[NR_CPUS * 2]
#define DECLARE_SNMP_STAT(type, name)	\
	extern type name[]
#define SNMP_DEC_STATS(mib, field) ((mib)[2*smp_processor_id()+!in_softirq()].field--)

#define inet_sk(__sk) (&(((struct sock *)__sk)->protinfo.af_inet))
#define inet6_sk(__sk) (&(((struct sock *)__sk)->net_pinfo.af_inet6))

#define virt_addr_valid(x)	VALID_PAGE(virt_to_page((x)))
#define sock_owned_by_user(sk)  ((sk)->lock.users)
#define sk_set_owner(x, y)
#define __unsafe(x) MOD_INC_USE_COUNT
#define dst_pmtu(x) ((x)->pmtu)

#define sk_family family
#define sk_state state
#define sk_type type
#define sk_socket socket
#define sk_prot prot
#define sk_rcvbuf rcvbuf
#define sk_sndbuf sndbuf
#define sk_ack_backlog ack_backlog
#define sk_max_ack_backlog max_ack_backlog
#define sk_write_space write_space
#define sk_use_write_queue use_write_queue
#define sk_err err
#define sk_err_soft err_soft
#define sk_error_report error_report
#define sk_error_queue error_queue
#define sk_shutdown shutdown
#define sk_state_change state_change
#define sk_receive_queue receive_queue
#define sk_data_ready data_ready
#define sk_no_check no_check
#define sk_reuse reuse
#define sk_destruct destruct
#define sk_zapped zapped
#define sk_protocol protocol
#define sk_backlog_rcv backlog_rcv
#define sk_allocation allocation
#define sk_lingertime lingertime
#define sk_sleep sleep
#define sk_wmem_queued wmem_queued
#define sk_bound_dev_if bound_dev_if

/*
 * find last bit set.
 */
static __inline__ int fls(int x)
{
	int r = 32;
	
	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

#endif /* __net_sctp_compat_h__ */
