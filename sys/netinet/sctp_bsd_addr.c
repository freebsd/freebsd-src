/*-
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $KAME: sctp_output.c,v 1.46 2005/03/06 16:04:17 itojun Exp $	 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <netinet/sctp_os.h>
#include <netinet/sctp_var.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_header.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_bsd_addr.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_indata.h>
#include <sys/unistd.h>

/* Declare all of our malloc named types */

/* Note to Michael/Peter for mac-os,
 * I think mac has this too since I
 * do see the M_PCB type, so I
 * will also put in the mac file the
 * MALLOC_DECLARE. If this does not
 * work for mac uncomment the defines for
 * the strings that we use in Panda, I put
 * them in comments in the mac-os file.
 */
MALLOC_DEFINE(SCTP_M_MAP, "sctp_map", "sctp asoc map descriptor");
MALLOC_DEFINE(SCTP_M_STRMI, "sctp_stri", "sctp stream in array");
MALLOC_DEFINE(SCTP_M_STRMO, "sctp_stro", "sctp stream out array");
MALLOC_DEFINE(SCTP_M_ASC_ADDR, "sctp_aadr", "sctp asconf address");
MALLOC_DEFINE(SCTP_M_ASC_IT, "sctp_a_it", "sctp asconf iterator");
MALLOC_DEFINE(SCTP_M_AUTH_CL, "sctp_atcl", "sctp auth chunklist");
MALLOC_DEFINE(SCTP_M_AUTH_KY, "sctp_atky", "sctp auth key");
MALLOC_DEFINE(SCTP_M_AUTH_HL, "sctp_athm", "sctp auth hmac list");
MALLOC_DEFINE(SCTP_M_AUTH_IF, "sctp_athi", "sctp auth info");
MALLOC_DEFINE(SCTP_M_STRESET, "sctp_stre", "sctp stream reset");
MALLOC_DEFINE(SCTP_M_CMSG, "sctp_cmsg", "sctp CMSG buffer");
MALLOC_DEFINE(SCTP_M_COPYAL, "sctp_cpal", "sctp copy all");
MALLOC_DEFINE(SCTP_M_VRF, "sctp_vrf", "sctp vrf struct");
MALLOC_DEFINE(SCTP_M_IFA, "sctp_ifa", "sctp ifa struct");
MALLOC_DEFINE(SCTP_M_IFN, "sctp_ifn", "sctp ifn struct");
MALLOC_DEFINE(SCTP_M_TIMW, "sctp_timw", "sctp time block");
MALLOC_DEFINE(SCTP_M_MVRF, "sctp_mvrf", "sctp mvrf pcb list");
MALLOC_DEFINE(SCTP_M_ITER, "sctp_iter", "sctp iterator control");
MALLOC_DEFINE(SCTP_M_SOCKOPT, "sctp_socko", "sctp socket option");

#if defined(SCTP_USE_THREAD_BASED_ITERATOR)
void
sctp_wakeup_iterator(void)
{
	wakeup(&SCTP_BASE_INFO(iterator_running));
}

static void
sctp_iterator_thread(void *v)
{
	SCTP_IPI_ITERATOR_WQ_LOCK();
	SCTP_BASE_INFO(iterator_running) = 0;
	while (1) {
		msleep(&SCTP_BASE_INFO(iterator_running),
		    &SCTP_BASE_INFO(ipi_iterator_wq_mtx),
		    0, "waiting_for_work", 0);
		if (SCTP_BASE_INFO(threads_must_exit)) {
			kthread_exit();
		}
		sctp_iterator_worker();
	}
}

void
sctp_startup_iterator(void)
{
	int ret;

	ret = kproc_create(sctp_iterator_thread,
	    (void *)NULL,
	    &SCTP_BASE_INFO(thread_proc),
	    RFPROC,
	    SCTP_KTHREAD_PAGES,
	    SCTP_KTRHEAD_NAME);
}

#endif

#ifdef INET6

void
sctp_gather_internal_ifa_flags(struct sctp_ifa *ifa)
{
	struct in6_ifaddr *ifa6;

	ifa6 = (struct in6_ifaddr *)ifa->ifa;
	ifa->flags = ifa6->ia6_flags;
	if (!MODULE_GLOBAL(ip6_use_deprecated)) {
		if (ifa->flags &
		    IN6_IFF_DEPRECATED) {
			ifa->localifa_flags |= SCTP_ADDR_IFA_UNUSEABLE;
		} else {
			ifa->localifa_flags &= ~SCTP_ADDR_IFA_UNUSEABLE;
		}
	} else {
		ifa->localifa_flags &= ~SCTP_ADDR_IFA_UNUSEABLE;
	}
	if (ifa->flags &
	    (IN6_IFF_DETACHED |
	    IN6_IFF_ANYCAST |
	    IN6_IFF_NOTREADY)) {
		ifa->localifa_flags |= SCTP_ADDR_IFA_UNUSEABLE;
	} else {
		ifa->localifa_flags &= ~SCTP_ADDR_IFA_UNUSEABLE;
	}
}

#endif				/* INET6 */


static uint32_t
sctp_is_desired_interface_type(struct ifaddr *ifa)
{
	int result;

	/* check the interface type to see if it's one we care about */
	switch (ifa->ifa_ifp->if_type) {
	case IFT_ETHER:
	case IFT_ISO88023:
	case IFT_ISO88024:
	case IFT_ISO88025:
	case IFT_ISO88026:
	case IFT_STARLAN:
	case IFT_P10:
	case IFT_P80:
	case IFT_HY:
	case IFT_FDDI:
	case IFT_XETHER:
	case IFT_ISDNBASIC:
	case IFT_ISDNPRIMARY:
	case IFT_PTPSERIAL:
	case IFT_OTHER:
	case IFT_PPP:
	case IFT_LOOP:
	case IFT_SLIP:
	case IFT_GIF:
	case IFT_L2VLAN:
	case IFT_IP:
	case IFT_IPOVERCDLC:
	case IFT_IPOVERCLAW:
	case IFT_VIRTUALIPADDRESS:
		result = 1;
		break;
	default:
		result = 0;
	}

	return (result);
}




static void
sctp_init_ifns_for_vrf(int vrfid)
{
	/*
	 * Here we must apply ANY locks needed by the IFN we access and also
	 * make sure we lock any IFA that exists as we float through the
	 * list of IFA's
	 */
	struct ifnet *ifn;
	struct ifaddr *ifa;
	struct in6_ifaddr *ifa6;
	struct sctp_ifa *sctp_ifa;
	uint32_t ifa_flags;

	TAILQ_FOREACH(ifn, &MODULE_GLOBAL(ifnet), if_list) {
		IF_ADDR_LOCK(ifn);
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			if (ifa->ifa_addr == NULL) {
				continue;
			}
			if ((ifa->ifa_addr->sa_family != AF_INET) && (ifa->ifa_addr->sa_family != AF_INET6)) {
				/* non inet/inet6 skip */
				continue;
			}
			if (ifa->ifa_addr->sa_family == AF_INET6) {
				if (IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr)) {
					/* skip unspecifed addresses */
					continue;
				}
			} else {
				if (((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == 0) {
					continue;
				}
			}
			if (sctp_is_desired_interface_type(ifa) == 0) {
				/* non desired type */
				continue;
			}
			if (ifa->ifa_addr->sa_family == AF_INET6) {
				ifa6 = (struct in6_ifaddr *)ifa;
				ifa_flags = ifa6->ia6_flags;
			} else {
				ifa_flags = 0;
			}
			sctp_ifa = sctp_add_addr_to_vrf(vrfid,
			    (void *)ifn,
			    ifn->if_index,
			    ifn->if_type,
			    ifn->if_xname,
			    (void *)ifa,
			    ifa->ifa_addr,
			    ifa_flags,
			    0);
			if (sctp_ifa) {
				sctp_ifa->localifa_flags &= ~SCTP_ADDR_DEFER_USE;
			}
		}
		IF_ADDR_UNLOCK(ifn);
	}
}

void
sctp_init_vrf_list(int vrfid)
{
	if (vrfid > SCTP_MAX_VRF_ID)
		/* can't do that */
		return;

	/* Don't care about return here */
	(void)sctp_allocate_vrf(vrfid);

	/*
	 * Now we need to build all the ifn's for this vrf and there
	 * addresses
	 */
	sctp_init_ifns_for_vrf(vrfid);
}

void
sctp_addr_change(struct ifaddr *ifa, int cmd)
{
	struct sctp_ifa *ifap = NULL;
	uint32_t ifa_flags = 0;

	/*
	 * BSD only has one VRF, if this changes we will need to hook in the
	 * right things here to get the id to pass to the address managment
	 * routine.
	 */
	if (SCTP_BASE_VAR(first_time) == 0) {
		/* Special test to see if my ::1 will showup with this */
		SCTP_BASE_VAR(first_time) = 1;
		sctp_init_ifns_for_vrf(SCTP_DEFAULT_VRFID);
	}
	if ((cmd != RTM_ADD) && (cmd != RTM_DELETE)) {
		/* don't know what to do with this */
		return;
	}
	if (ifa->ifa_addr == NULL) {
		return;
	}
	if ((ifa->ifa_addr->sa_family != AF_INET) && (ifa->ifa_addr->sa_family != AF_INET6)) {
		/* non inet/inet6 skip */
		return;
	}
	if (ifa->ifa_addr->sa_family == AF_INET6) {
		ifa_flags = ((struct in6_ifaddr *)ifa)->ia6_flags;
		if (IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr)) {
			/* skip unspecifed addresses */
			return;
		}
	} else {
		if (((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == 0) {
			return;
		}
	}

	if (sctp_is_desired_interface_type(ifa) == 0) {
		/* non desired type */
		return;
	}
	if (cmd == RTM_ADD) {
		ifap = sctp_add_addr_to_vrf(SCTP_DEFAULT_VRFID, (void *)ifa->ifa_ifp,
		    ifa->ifa_ifp->if_index, ifa->ifa_ifp->if_type,
		    ifa->ifa_ifp->if_xname,
		    (void *)ifa, ifa->ifa_addr, ifa_flags, 1);
	} else {

		sctp_del_addr_from_vrf(SCTP_DEFAULT_VRFID, ifa->ifa_addr,
		    ifa->ifa_ifp->if_index,
		    ifa->ifa_ifp->if_xname
		    );
		/*
		 * We don't bump refcount here so when it completes the
		 * final delete will happen.
		 */
	}
}

void
     sctp_add_or_del_interfaces(int (*pred) (struct ifnet *), int add){
	struct ifnet *ifn;
	struct ifaddr *ifa;

	TAILQ_FOREACH(ifn, &MODULE_GLOBAL(ifnet), if_list) {
		if (!(*pred) (ifn)) {
			continue;
		}
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			sctp_addr_change(ifa, add ? RTM_ADD : RTM_DELETE);
		}
	}
}

struct mbuf *
sctp_get_mbuf_for_msg(unsigned int space_needed, int want_header,
    int how, int allonebuf, int type)
{
	struct mbuf *m = NULL;

	m = m_getm2(NULL, space_needed, how, type, want_header ? M_PKTHDR : 0);
	if (m == NULL) {
		/* bad, no memory */
		return (m);
	}
	if (allonebuf) {
		int siz;

		if (SCTP_BUF_IS_EXTENDED(m)) {
			siz = SCTP_BUF_EXTEND_SIZE(m);
		} else {
			if (want_header)
				siz = MHLEN;
			else
				siz = MLEN;
		}
		if (siz < space_needed) {
			m_freem(m);
			return (NULL);
		}
	}
	if (SCTP_BUF_NEXT(m)) {
		sctp_m_freem(SCTP_BUF_NEXT(m));
		SCTP_BUF_NEXT(m) = NULL;
	}
#ifdef SCTP_MBUF_LOGGING
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_MBUF_LOGGING_ENABLE) {
		if (SCTP_BUF_IS_EXTENDED(m)) {
			sctp_log_mb(m, SCTP_MBUF_IALLOC);
		}
	}
#endif
	return (m);
}


#ifdef SCTP_PACKET_LOGGING
void
sctp_packet_log(struct mbuf *m, int length)
{
	int *lenat, thisone;
	void *copyto;
	uint32_t *tick_tock;
	int total_len;
	int grabbed_lock = 0;
	int value, newval, thisend, thisbegin;

	/*
	 * Buffer layout. -sizeof this entry (total_len) -previous end
	 * (value) -ticks of log      (ticks) o -ip packet o -as logged -
	 * where this started (thisbegin) x <--end points here
	 */
	total_len = SCTP_SIZE32((length + (4 * sizeof(int))));
	/* Log a packet to the buffer. */
	if (total_len > SCTP_PACKET_LOG_SIZE) {
		/* Can't log this packet I have not a buffer big enough */
		return;
	}
	if (length < (int)(SCTP_MIN_V4_OVERHEAD + sizeof(struct sctp_cookie_ack_chunk))) {
		return;
	}
	atomic_add_int(&SCTP_BASE_VAR(packet_log_writers), 1);
try_again:
	if (SCTP_BASE_VAR(packet_log_writers) > SCTP_PKTLOG_WRITERS_NEED_LOCK) {
		SCTP_IP_PKTLOG_LOCK();
		grabbed_lock = 1;
again_locked:
		value = SCTP_BASE_VAR(packet_log_end);
		newval = SCTP_BASE_VAR(packet_log_end) + total_len;
		if (newval >= SCTP_PACKET_LOG_SIZE) {
			/* we wrapped */
			thisbegin = 0;
			thisend = total_len;
		} else {
			thisbegin = SCTP_BASE_VAR(packet_log_end);
			thisend = newval;
		}
		if (!(atomic_cmpset_int(&SCTP_BASE_VAR(packet_log_end), value, thisend))) {
			goto again_locked;
		}
	} else {
		value = SCTP_BASE_VAR(packet_log_end);
		newval = SCTP_BASE_VAR(packet_log_end) + total_len;
		if (newval >= SCTP_PACKET_LOG_SIZE) {
			/* we wrapped */
			thisbegin = 0;
			thisend = total_len;
		} else {
			thisbegin = SCTP_BASE_VAR(packet_log_end);
			thisend = newval;
		}
		if (!(atomic_cmpset_int(&SCTP_BASE_VAR(packet_log_end), value, thisend))) {
			goto try_again;
		}
	}
	/* Sanity check */
	if (thisend >= SCTP_PACKET_LOG_SIZE) {
		printf("Insanity stops a log thisbegin:%d thisend:%d writers:%d lock:%d end:%d\n",
		    thisbegin,
		    thisend,
		    SCTP_BASE_VAR(packet_log_writers),
		    grabbed_lock,
		    SCTP_BASE_VAR(packet_log_end));
		SCTP_BASE_VAR(packet_log_end) = 0;
		goto no_log;

	}
	lenat = (int *)&SCTP_BASE_VAR(packet_log_buffer)[thisbegin];
	*lenat = total_len;
	lenat++;
	*lenat = value;
	lenat++;
	tick_tock = (uint32_t *) lenat;
	lenat++;
	*tick_tock = sctp_get_tick_count();
	copyto = (void *)lenat;
	thisone = thisend - sizeof(int);
	lenat = (int *)&SCTP_BASE_VAR(packet_log_buffer)[thisone];
	*lenat = thisbegin;
	if (grabbed_lock) {
		SCTP_IP_PKTLOG_UNLOCK();
		grabbed_lock = 0;
	}
	m_copydata(m, 0, length, (caddr_t)copyto);
no_log:
	if (grabbed_lock) {
		SCTP_IP_PKTLOG_UNLOCK();
	}
	atomic_subtract_int(&SCTP_BASE_VAR(packet_log_writers), 1);
}


int
sctp_copy_out_packet_log(uint8_t * target, int length)
{
	/*
	 * We wind through the packet log starting at start copying up to
	 * length bytes out. We return the number of bytes copied.
	 */
	int tocopy, this_copy;
	int *lenat;
	int did_delay = 0;

	tocopy = length;
	if (length < (int)(2 * sizeof(int))) {
		/* not enough room */
		return (0);
	}
	if (SCTP_PKTLOG_WRITERS_NEED_LOCK) {
		atomic_add_int(&SCTP_BASE_VAR(packet_log_writers), SCTP_PKTLOG_WRITERS_NEED_LOCK);
again:
		if ((did_delay == 0) && (SCTP_BASE_VAR(packet_log_writers) != SCTP_PKTLOG_WRITERS_NEED_LOCK)) {
			/*
			 * we delay here for just a moment hoping the
			 * writer(s) that were present when we entered will
			 * have left and we only have locking ones that will
			 * contend with us for the lock. This does not
			 * assure 100% access, but its good enough for a
			 * logging facility like this.
			 */
			did_delay = 1;
			DELAY(10);
			goto again;
		}
	}
	SCTP_IP_PKTLOG_LOCK();
	lenat = (int *)target;
	*lenat = SCTP_BASE_VAR(packet_log_end);
	lenat++;
	this_copy = min((length - sizeof(int)), SCTP_PACKET_LOG_SIZE);
	memcpy((void *)lenat, (void *)SCTP_BASE_VAR(packet_log_buffer), this_copy);
	if (SCTP_PKTLOG_WRITERS_NEED_LOCK) {
		atomic_subtract_int(&SCTP_BASE_VAR(packet_log_writers),
		    SCTP_PKTLOG_WRITERS_NEED_LOCK);
	}
	SCTP_IP_PKTLOG_UNLOCK();
	return (this_copy + sizeof(int));
}

#endif
