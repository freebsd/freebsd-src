#ifndef NTP_PSL_H
#define NTP_PSL_H


/*
 * Poll Skew List Item
 * u_in32 is large enough for sub and qty so long as NTP_MAXPOLL < 31
 */
#if NTP_MAXPOLL >= 31
#include "psl_item structure needs larger type"
#endif
typedef struct psl_item_tag {
	u_int32	sub;
	u_int32	qty;
	u_int32	msk;
} psl_item;

int get_pollskew(int, psl_item *);

#endif	/* !defined(NTP_PSL_H) */
