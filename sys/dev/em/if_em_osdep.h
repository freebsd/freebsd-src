/**************************************************************************
**************************************************************************

Copyright (c) 2001 Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms of the Software, with or
without modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code of the Software may retain the above
    copyright notice, this list of conditions and the following disclaimer.

 2. Redistributions in binary form of the Software may reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors shall be used to endorse or promote products derived from
    this Software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR ITS CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

***************************************************************************
***************************************************************************/
/*$FreeBSD$*/

#ifndef _FREEBSD_OS_H_
#define _FREEBSD_OS_H_

#include <sys/types.h>

#define ASSERT(x) if(!(x)) panic("EM: x")

/* The happy-fun DELAY macro is defined in /usr/src/sys/i386/include/clock.h */
#define DelayInMicroseconds(x) DELAY(x)
#define DelayInMilliseconds(x) DELAY(1000*(x))

typedef u_int8_t   u8;
typedef u_int16_t  u16;
typedef u_int32_t  u32;
typedef struct _E1000_64_BIT_PHYSICAL_ADDRESS {
    u32 Lo32;
    u32 Hi32;
} E1000_64_BIT_PHYSICAL_ADDRESS, *PE1000_64_BIT_PHYSICAL_ADDRESS;

#define IN
#define OUT
#define STATIC static

#define MSGOUT(S, A, B)     printf(S "\n", A, B)
#define DEBUGFUNC(F)        DEBUGOUT(F);
#if DBG
    #define DEBUGOUT(S)         printf(S "\n")
    #define DEBUGOUT1(S,A)      printf(S "\n",A)
    #define DEBUGOUT2(S,A,B)    printf(S "\n",A,B)
    #define DEBUGOUT3(S,A,B,C)  printf(S "\n",A,B,C)
    #define DEBUGOUT7(S,A,B,C,D,E,F,G)  printf(S "\n",A,B,C,D,E,F,G)
#else
    #define DEBUGOUT(S)
    #define DEBUGOUT1(S,A)
    #define DEBUGOUT2(S,A,B)
    #define DEBUGOUT3(S,A,B,C)
    #define DEBUGOUT7(S,A,B,C,D,E,F,G)
#endif


#define E1000_READ_REG(reg)  \
        bus_space_read_4(Adapter->bus_space_tag, Adapter->bus_space_handle, \
        (Adapter->MacType >= MAC_LIVENGOOD)?offsetof(E1000_REGISTERS, reg): \
        offsetof(OLD_REGISTERS, reg))

#define E1000_WRITE_REG(reg, value)  \
        bus_space_write_4(Adapter->bus_space_tag, Adapter->bus_space_handle, \
        (Adapter->MacType >= MAC_LIVENGOOD)?offsetof(E1000_REGISTERS, reg): \
        offsetof(OLD_REGISTERS, reg), value)

#define WritePciConfigWord(Reg, PValue)  pci_write_config(Adapter->dev, Reg, *PValue, 2);


#include <dev/em/if_em.h>

#endif  /* _FREEBSD_OS_H_ */

