/*
 * Machine-independent glue to integrate David Gay's gdtoa
 * package into libc.
 *
 * $FreeBSD$
 */

#include "spinlock.h"

spinlock_t __gdtoa_locks[2];
