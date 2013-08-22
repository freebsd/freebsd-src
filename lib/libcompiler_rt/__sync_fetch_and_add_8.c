/* $FreeBSD$ */
#define	NAME		__sync_fetch_and_add_8
#define	TYPE		int64_t
#define	FETCHADD(x, y)	atomic_fetchadd_64(x, y)

#include "__sync_fetch_and_op_n.h"
