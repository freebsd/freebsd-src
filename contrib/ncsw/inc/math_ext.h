/* Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MATH_EXT_H
#define __MATH_EXT_H


#if defined(NCSW_LINUX) && defined(__KERNEL__)
#include <linux/math.h>

#elif defined(__MWERKS__)
#define LOW(x) ( sizeof(x)==8 ? *(1+(int32_t*)&x) : (*(int32_t*)&x))
#define HIGH(x) (*(int32_t*)&x)
#define ULOW(x) ( sizeof(x)==8 ? *(1+(uint32_t*)&x) : (*(uint32_t*)&x))
#define UHIGH(x) (*(uint32_t*)&x)

static const double big = 1.0e300;

/* Macro for checking if a number is a power of 2 */
static __inline__ double ceil(double x)
{
    int32_t i0,i1,j0; /*- cc 020130 -*/
    uint32_t i,j; /*- cc 020130 -*/
    i0 =  HIGH(x);
    i1 =  LOW(x);
    j0 = ((i0>>20)&0x7ff)-0x3ff;
    if(j0<20) {
        if(j0<0) {     /* raise inexact if x != 0 */
        if(big+x>0.0) {/* return 0*sign(x) if |x|<1 */
            if(i0<0) {i0=0x80000000;i1=0;}
            else if((i0|i1)!=0) { i0=0x3ff00000;i1=0;}
        }
        } else {
        i = (uint32_t)(0x000fffff)>>j0;
        if(((i0&i)|i1)==0) return x; /* x is integral */
        if(big+x>0.0) {    /* raise inexact flag */
            if(i0>0) i0 += (0x00100000)>>j0;
            i0 &= (~i); i1=0;
        }
        }
    } else if (j0>51) {
        if(j0==0x400) return x+x;    /* inf or NaN */
        else return x;        /* x is integral */
    } else {
        i = ((uint32_t)(0xffffffff))>>(j0-20); /*- cc 020130 -*/
        if((i1&i)==0) return x;    /* x is integral */
        if(big+x>0.0) {         /* raise inexact flag */
        if(i0>0) {
            if(j0==20) i0+=1;
            else {
            j = (uint32_t)(i1 + (1<<(52-j0)));
            if(j<i1) i0+=1;    /* got a carry */
            i1 = (int32_t)j;
            }
        }
        i1 &= (~i);
        }
    }
    HIGH(x) = i0;
    LOW(x) = i1;
    return x;
}

#else
#include <math.h>
#endif /* defined(NCSW_LINUX) && defined(__KERNEL__) */


#endif /* __MATH_EXT_H */
