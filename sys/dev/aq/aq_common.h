/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3)The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AQ_COMMON_H_
#define _AQ_COMMON_H_

#include <sys/types.h>

#define s8       __int8_t
#define u8       __uint8_t
#define u16      __uint16_t
#define s16      __int16_t
#define u32      __uint32_t
#define u64      __uint64_t
#define s64      __int64_t
#define s32      int

#define ETIME ETIMEDOUT
#define EOK 0

#define BIT(nr) (1UL << (nr))

#define usec_delay(x) DELAY(x)

#ifndef msec_delay
#define msec_delay(x) DELAY(x*1000)
#define msec_delay_irq(x) DELAY(x*1000)
#endif

#define AQ_HW_WAIT_FOR(_B_, _US_, _N_) \
    do { \
        unsigned int i; \
        for (i = _N_; (!(_B_)) && i; --i) { \
            usec_delay(_US_); \
        } \
        if (!i) { \
            err = -1; \
        } \
    } while (0)


#define LOWORD(a) ((u16)(a))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define AQ_VER        "0.0.5"

#endif //_AQ_COMMON_H_

