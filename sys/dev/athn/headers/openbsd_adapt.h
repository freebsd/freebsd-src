/*	$OpenBSD: time.h,v 1.63 2022/12/13 17:30:36 cheloha Exp $	*/
/*	$NetBSD: time.h,v 1.18 1996/04/23 10:29:33 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)time.h	8.2 (Berkeley) 7/10/94
 */

#ifndef _OPENBSD_ADAPT_H_
#define _OPENBSD_ADAPT_H_

/*
 * Set to 1 to 'uncomment' (enable)
 * parts of the code that depend on the USB OpenBSD API
 */
#define OpenBSD_IEEE80211_API 0

/*
 * Set to 1 to 'uncomment' (enable)
 * parts of the code that depend on the USB OpenBSD API
 */
#define OpenBSD_USB_API 0

/*
 * Code makred by this macro
 * needs a OpenBSD equivalent function or complete rework
 * Set to 1 to 'uncomment' (enable)
 */
#define OpenBSD_ONLY 0

#include <sys/endian.h>
// map OpenBSD endian conversion macro names to FreeBSD
#define betoh16 be16toh
#define betoh32 be32toh
#define betoh64 be64toh
#define letoh16 le16toh

// map OpenBSD flag name to FreeBSD
#define	IFF_RUNNING	IFF_DRV_RUNNING

// map 3-argument OpenBSD free function to 2-argument FreeBSD one
// if the number of arguments is 2 - do nothing
#define free(addr,type,...)	free(addr,type)

#define IEEE80211_HT_NUM_MCS            77
#define IEEE80211_DUR_DS_SHSLOT         9
#define MBUF_LIST_INITIALIZER() { NULL, NULL, 0 }


struct mbuf_list {
        struct mbuf             *ml_head;
        struct mbuf             *ml_tail;
        u_int                   ml_len;
};

enum ieee80211_edca_ac {
        EDCA_AC_BK  = 1,        /* Background */
        EDCA_AC_BE  = 0,        /* Best Effort */
        EDCA_AC_VI  = 2,        /* Video */
        EDCA_AC_VO  = 3         /* Voice */
};



// TODO: try remove these defines, use proper include instead

#define SIMPLEQ_ENTRY(type)                                             \
struct {                                                                \
         struct type *sqe_next;  /* next element */                     \
}

#define SIMPLEQ_FIRST(head)         ((head)->sqh_first)
#define SIMPLEQ_END(head)           NULL
#define SIMPLEQ_EMPTY(head)         (SIMPLEQ_FIRST(head) == SIMPLEQ_END(head))
#define SIMPLEQ_NEXT(elm, field)    ((elm)->field.sqe_next)


#define SIMPLEQ_HEAD(name, type)                                        \
struct name {                                                           \
        struct type *sqh_first; /* first element */                     \
        struct type **sqh_last; /* addr of last next element */         \
}

#define SIMPLEQ_REMOVE_HEAD(head, field) do {                   \
        if (((head)->sqh_first = (head)->sqh_first->field.sqe_next) == NULL) \
                (head)->sqh_last = &(head)->sqh_first;                  \
} while (0)

#define SIMPLEQ_INSERT_TAIL(head, elm, field) do {                      \
        (elm)->field.sqe_next = NULL;                                   \
        *(head)->sqh_last = (elm);                                      \
        (head)->sqh_last = &(elm)->field.sqe_next;                      \
} while (0)

#define SIMPLEQ_INIT(head) do {                                         \
        (head)->sqh_first = NULL;                                       \
        (head)->sqh_last = &(head)->sqh_first;                          \
} while (0)


static inline uint64_t
SEC_TO_NSEC(uint64_t seconds)
{
	if (seconds > UINT64_MAX / 1000000000ULL)
		return UINT64_MAX;
	return seconds * 1000000000ULL;
}

static inline uint64_t
MSEC_TO_NSEC(uint64_t milliseconds)
{
	if (milliseconds > UINT64_MAX / 1000000ULL)
		return UINT64_MAX;
	return milliseconds * 1000000ULL;
}

// ifq_oactive is not available in FreeBSD.
// Need to use additional variable to provide this functionality.
extern unsigned int ifq_oactive;

static inline void
ifq_clr_oactive()
{
	ifq_oactive = 0;
}

static inline unsigned int
ifq_is_oactive()
{
	return ifq_oactive;
}

static inline void
ifq_set_oactive()
{
	ifq_oactive = 1;
}

static __inline intrmask_t	splusb(void)		{ return 0; }

#endif /* _OPENBSD_ADAPT_H_ */
