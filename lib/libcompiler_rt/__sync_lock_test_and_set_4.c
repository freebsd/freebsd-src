/* $FreeBSD$ */
#define	NAME		__sync_lock_test_and_set_4
#define	TYPE		int32_t
#define	CMPSET		atomic_cmpset_32
#define	EXPRESSION	value

#include "__sync_fetch_and_op_n.h"
