/* $FreeBSD$ */
#define	NAME		__sync_fetch_and_sub_4
#define	TYPE		uint32_t
#define	FETCHADD(x, y)	atomic_fetchadd_32(x, -(y))

#include "__sync_fetch_and_op_n.h"
