/* $FreeBSD$ */
#define	NAME		__sync_fetch_and_and_4
#define	TYPE		int32_t
#define	CMPSET		atomic_cmpset_32
#define	EXPRESSION	t & value

#include "__sync_fetch_and_op_n.h"
