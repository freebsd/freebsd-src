/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/alpha/include/bwx.h,v 1.4 2000/02/12 14:56:59 dfr Exp $
 */

#ifndef _MACHINE_BWX_H_
#define	_MACHINE_BWX_H_

/*
 * Byte/word accesses must be made with particular values for addr<37,38>
 */
#define BWX_EV56_INT8	(0L << 37)
#define BWX_EV56_INT4	(1L << 37)
#define BWX_EV56_INT2	(2L << 37)
#define BWX_EV56_INT1	(3L << 37)

static __inline u_int8_t
ldbu(vm_offset_t va)
{
    u_int64_t r;
    __asm__ __volatile__ ("ldbu %0,%1" : "=r"(r) : "m"(*(u_int8_t*)va));
    return r;
}

static __inline u_int16_t
ldwu(vm_offset_t va)
{
    u_int64_t r;
    __asm__ __volatile__ ("ldwu %0,%1" : "=r"(r) : "m"(*(u_int16_t*)va));
    return r;
}

static __inline u_int32_t
ldl(vm_offset_t va)
{
    return *(u_int32_t*) va;
}

static __inline void
stb(vm_offset_t va, u_int64_t r)
{
    __asm__ __volatile__ ("stb %1,%0" : "=m"(*(u_int8_t*)va) : "r"(r));
    __asm__ __volatile__ ("mb");
}

static __inline void
stw(vm_offset_t va, u_int64_t r)
{
    __asm__ __volatile__ ("stw %1,%0" : "=m"(*(u_int16_t*)va) : "r"(r));
    __asm__ __volatile__ ("mb");
}


static __inline void
stl(vm_offset_t va, u_int64_t r)
{
    __asm__ __volatile__ ("stl %1,%0" : "=m"(*(u_int32_t*)va) : "r"(r));
    __asm__ __volatile__ ("mb");
}

static __inline void
stb_nb(vm_offset_t va, u_int64_t r)
{
    __asm__ __volatile__ ("stb %1,%0" : "=m"(*(u_int8_t*)va) : "r"(r));
}

static __inline void
stw_nb(vm_offset_t va, u_int64_t r)
{
    __asm__ __volatile__ ("stw %1,%0" : "=m"(*(u_int16_t*)va) : "r"(r));
}


static __inline void
stl_nb(vm_offset_t va, u_int64_t r)
{
    __asm__ __volatile__ ("stl %1,%0" : "=m"(*(u_int32_t*)va) : "r"(r));
}

#endif /* !_MACHINE_BWX_H_ */
