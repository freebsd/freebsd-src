/*-
 * Copyright (c) 2001 FreeBSD Inc.
 * All rights reserved.
 *
 * These routines are for converting time_t to fixed-bit representations
 * for use in protocols or storage.  When converting time to a larger
 * representation of time_t these routines are expected to assume temporal
 * locality and use the 50-year rule to properly set the msb bits.  XXX
 *
 * Redistribution and use under the terms of the COPYRIGHT file at the
 * base of the source tree.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/time.h>

/*
 * Convert a 32 bit representation of time_t into time_t.  XXX needs to
 * implement the 50-year rule to handle post-2038 conversions.
 */
time_t
time32_to_time(__int32_t t32)
{
    return((time_t)t32);
}

/*
 * Convert time_t to a 32 bit representation.  If time_t is 64 bits we can
 * simply chop it down.   The resulting 32 bit representation can be 
 * converted back to a temporally local 64 bit time_t using time32_to_time.
 */
__int32_t
time_to_time32(time_t t)
{
    return((__int32_t)t);
}

/*
 * Convert a 64 bit representation of time_t into time_t.  If time_t is
 * represented as 32 bits we can simply chop it and not support times
 * past 2038.
 */
time_t
time64_to_time(__int64_t t64)
{
    return((time_t)t64);
}

/*
 * Convert time_t to a 64 bit representation.  If time_t is represented
 * as 32 bits we simply sign-extend and do not support times past 2038.
 */
__int64_t
time_to_time64(time_t t)
{
    return((__int64_t)t);
}

