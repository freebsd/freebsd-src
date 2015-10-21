/*****************************************************************************
 * x86/xen/xen-os.h
 *
 * Random collection of macros and definition
 *
 * Copyright (c) 2003, 2004 Keir Fraser (on behalf of the Xen team)
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_X86_XEN_XEN_OS_H_
#define _MACHINE_X86_XEN_XEN_OS_H_

#ifdef PAE
#define CONFIG_X86_PAE
#endif

/* Everything below this point is not included by assembler (.S) files. */
#ifndef __ASSEMBLY__

/* REP NOP (PAUSE) is a good thing to insert into busy-wait loops. */
static inline void rep_nop(void)
{
    __asm__ __volatile__ ( "rep;nop" : : : "memory" );
}
#define cpu_relax() rep_nop()

/* This is a barrier for the compiler only, NOT the processor! */
#define barrier() __asm__ __volatile__("": : :"memory")

#define LOCK_PREFIX ""
#define LOCK ""
#define ADDR (*(volatile long *) addr)

/**
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static __inline int test_and_clear_bit(int nr, volatile void * addr)
{
        int oldbit;

        __asm__ __volatile__( LOCK_PREFIX
                "btrl %2,%1\n\tsbbl %0,%0"
                :"=r" (oldbit),"=m" (ADDR)
                :"Ir" (nr) : "memory");
        return oldbit;
}

static __inline int constant_test_bit(int nr, const volatile void * addr)
{
    return ((1UL << (nr & 31)) & (((const volatile unsigned int *) addr)[nr >> 5])) != 0;
}

static __inline int variable_test_bit(int nr, volatile void * addr)
{
    int oldbit;

    __asm__ __volatile__(
        "btl %2,%1\n\tsbbl %0,%0"
        :"=r" (oldbit)
        :"m" (ADDR),"Ir" (nr));
    return oldbit;
}

#define test_bit(nr,addr) \
(__builtin_constant_p(nr) ? \
 constant_test_bit((nr),(addr)) : \
 variable_test_bit((nr),(addr)))

/**
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static __inline__ void set_bit(int nr, volatile void * addr)
{
        __asm__ __volatile__( LOCK_PREFIX
                "btsl %1,%0"
                :"=m" (ADDR)
                :"Ir" (nr));
}

/**
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_clear_bit() and/or smp_mb__after_clear_bit()
 * in order to ensure changes are visible on other processors.
 */
static __inline__ void clear_bit(int nr, volatile void * addr)
{
        __asm__ __volatile__( LOCK_PREFIX
                "btrl %1,%0"
                :"=m" (ADDR)
                :"Ir" (nr));
}

#endif /* !__ASSEMBLY__ */

#endif /* _MACHINE_X86_XEN_XEN_OS_H_ */
