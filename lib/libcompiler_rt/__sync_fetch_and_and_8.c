/* $FreeBSD$ */
#define	NAME		__sync_fetch_and_and_8
#define	TYPE		int64_t
#define	CMPSET		atomic_cmpset_64
#define	EXPRESSION	t & value

#include "__sync_fetch_and_op_n.h"
