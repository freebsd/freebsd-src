/* $FreeBSD$ */
#define	NAME		__sync_lock_test_and_set_8
#define	TYPE		int64_t
#define	CMPSET		atomic_cmpset_64
#define	EXPRESSION	value

#include "__sync_fetch_and_op_n.h"
