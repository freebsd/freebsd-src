/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Cavium Networks nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/







/**
 * @file
 *
 * Interface to the Mips interrupts.
 *
 * <hr>$Revision: 52004 $<hr>
 */
#ifndef __U_BOOT__
#if __GNUC__ >= 4
/* Backtrace is only available with the new toolchain.  */
#include <execinfo.h>
#endif
#endif  /* __U_BOOT__ */
#include "cvmx-config.h"
#include "cvmx.h"
#include "cvmx-interrupt.h"
#include "cvmx-sysinfo.h"
#include "cvmx-uart.h"
#include "cvmx-pow.h"
#include "cvmx-ebt3000.h"
#include "cvmx-coremask.h"
#include "cvmx-spinlock.h"
#include "cvmx-app-init.h"
#include "cvmx-error.h"
#include "../../bootloader/u-boot/include/octeon_mem_map.h"

EXTERN_ASM void cvmx_interrupt_stage1(void);
EXTERN_ASM void cvmx_debug_handler_stage1(void);
EXTERN_ASM void cvmx_interrupt_cache_error(void);

int cvmx_interrupt_in_isr = 0;

/**
 * Internal status the interrupt registration
 */
typedef struct
{
    cvmx_interrupt_func_t handlers[256];  /**< One function to call per interrupt */
    void *                data[256];      /**< User data per interrupt */
    cvmx_interrupt_exception_t exception_handler;
} cvmx_interrupt_state_t;

/**
 * Internal state the interrupt registration
 */
#ifndef __U_BOOT__
static CVMX_SHARED cvmx_interrupt_state_t cvmx_interrupt_state;
static CVMX_SHARED cvmx_spinlock_t cvmx_interrupt_default_lock;
#endif  /* __U_BOOT__ */

#define ULL unsigned long long

#define HI32(data64)	((uint32_t)(data64 >> 32))
#define LO32(data64)	((uint32_t)(data64 & 0xFFFFFFFF))

static const char reg_names[][32] = { "r0","at","v0","v1","a0","a1","a2","a3",
                                      "t0","t1","t2","t3","t4","t5","t6","t7",
                                      "s0","s1","s2","s3","s4","s5", "s6","s7",
                                      "t8","t9", "k0","k1","gp","sp","s8","ra" };

/**
 * version of printf that works better in exception context.
 *
 * @param format
 */
void cvmx_safe_printf(const char *format, ...)
{
    char buffer[256];
    char *ptr = buffer;
    int count;
    va_list args;

    va_start(args, format);
#ifndef __U_BOOT__
    count = vsnprintf(buffer, sizeof(buffer), format, args);
#else
    count = vsprintf(buffer, format, args);
#endif
    va_end(args);

    while (count-- > 0)
    {
        cvmx_uart_lsr_t lsrval;

        /* Spin until there is room */
        do
        {
            lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(0));
#if !defined(CONFIG_OCTEON_SIM_SPEED)
            if (lsrval.s.temt == 0)
                cvmx_wait(10000);   /* Just to reduce the load on the system */
#endif
        }
        while (lsrval.s.temt == 0);

        if (*ptr == '\n')
            cvmx_write_csr(CVMX_MIO_UARTX_THR(0), '\r');
        cvmx_write_csr(CVMX_MIO_UARTX_THR(0), *ptr++);
    }
}

/* Textual descriptions of cause codes */
static const char cause_names[][128] = {
        /*  0 */ "Interrupt",
        /*  1 */ "TLB modification",
        /*  2 */ "tlb load/fetch",
        /*  3 */ "tlb store",
        /*  4 */ "address exc, load/fetch",
        /*  5 */ "address exc, store",
        /*  6 */ "bus error, instruction fetch",
        /*  7 */ "bus error, load/store",
        /*  8 */ "syscall",
        /*  9 */ "breakpoint",
        /* 10 */ "reserved instruction",
        /* 11 */ "cop unusable",
        /* 12 */ "arithmetic overflow",
        /* 13 */ "trap",
        /* 14 */ "",
        /* 15 */ "floating point exc",
        /* 16 */ "",
        /* 17 */ "",
        /* 18 */ "cop2 exception",
        /* 19 */ "",
        /* 20 */ "",
        /* 21 */ "",
        /* 22 */ "mdmx unusable",
        /* 23 */ "watch",
        /* 24 */ "machine check",
        /* 25 */ "",
        /* 26 */ "",
        /* 27 */ "",
        /* 28 */ "",
        /* 29 */ "",
        /* 30 */ "cache error",
        /* 31 */ ""
};

/**
 * @INTERNAL
 * print_reg64
 * @param name   Name of the value to print
 * @param reg    Value to print
 */
static inline void print_reg64(const char *name, uint64_t reg)
{
    cvmx_safe_printf("%16s: 0x%08x%08x\n", name, (unsigned int)HI32(reg),(unsigned int)LO32(reg));
}

/**
 * @INTERNAL
 * Dump all useful registers to the console
 *
 * @param registers CPU register to dump
 */
static void __cvmx_interrupt_dump_registers(uint64_t registers[32])
{
    uint64_t r1, r2;
    int reg;
    for (reg=0; reg<16; reg++)
    {
        r1 = registers[reg]; r2 = registers[reg+16];
        cvmx_safe_printf("%3s ($%02d): 0x%08x%08x \t %3s ($%02d): 0x%08x%08x\n",
                           reg_names[reg], reg, (unsigned int)HI32(r1), (unsigned int)LO32(r1),
                           reg_names[reg+16], reg+16, (unsigned int)HI32(r2), (unsigned int)LO32(r2));
    }
    CVMX_MF_COP0 (r1, COP0_CAUSE);
    print_reg64 ("COP0_CAUSE", r1);
    CVMX_MF_COP0 (r2, COP0_STATUS);
    print_reg64 ("COP0_STATUS", r2);
    CVMX_MF_COP0 (r1, COP0_BADVADDR);
    print_reg64 ("COP0_BADVADDR", r1);
    CVMX_MF_COP0 (r2, COP0_EPC);
    print_reg64 ("COP0_EPC", r2);
}

/**
 * @INTERNAL
 * Default exception handler. Prints out the exception
 * cause decode and all relevant registers.
 *
 * @param registers Registers at time of the exception
 */
#ifndef __U_BOOT__
static
#endif  /* __U_BOOT__ */
void __cvmx_interrupt_default_exception_handler(uint64_t registers[32])
{
    uint64_t trap_print_cause;
    const char *str;

#ifndef __U_BOOT__
    ebt3000_str_write("Trap");
    cvmx_spinlock_lock(&cvmx_interrupt_default_lock);
#endif
    CVMX_MF_COP0 (trap_print_cause, COP0_CAUSE);
    str = cause_names [(trap_print_cause >> 2) & 0x1f];
    cvmx_safe_printf("Core %d: Unhandled Exception. Cause register decodes to:\n%s\n", (int)cvmx_get_core_num(), str && *str ? str : "Reserved exception cause");
    cvmx_safe_printf("******************************************************************\n");
    __cvmx_interrupt_dump_registers(registers);

#ifndef __U_BOOT__

    cvmx_safe_printf("******************************************************************\n");
#if __GNUC__ >= 4 && !defined(OCTEON_DISABLE_BACKTRACE)
    cvmx_safe_printf("Backtrace:\n\n");
    __octeon_print_backtrace_func ((__octeon_backtrace_printf_t)cvmx_safe_printf);
    cvmx_safe_printf("******************************************************************\n");
#endif

    cvmx_spinlock_unlock(&cvmx_interrupt_default_lock);

    if (cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_SIM)
	CVMX_BREAK;

    while (1)
    {
	/* Interrupts are suppressed when we are in the exception
	   handler (because of SR[EXL]).  Spin and poll the uart
	   status and see if the debugger is trying to stop us. */
	cvmx_uart_lsr_t lsrval;
	lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(cvmx_debug_uart));
	if (lsrval.s.dr)
	{
	    uint64_t tmp;
	    /* Pulse the MCD0 signal. */
	    asm volatile (
	    ".set push\n"
	    ".set noreorder\n"
	    ".set mips64\n"
	    "dmfc0 %0, $22\n"
	    "ori   %0, %0, 0x10\n"
	    "dmtc0 %0, $22\n"
	    ".set pop\n"
	    : "=r" (tmp));
	}
    }
#endif /* __U_BOOT__ */
}

#ifndef __U_BOOT__
/**
 * @INTERNAL
 * Default interrupt handler if the user doesn't register one.
 *
 * @param irq_number IRQ that caused this interrupt
 * @param registers  Register at the time of the interrupt
 * @param user_arg   Unused optional user data
 */
static void __cvmx_interrupt_default(int irq_number, uint64_t registers[32], void *user_arg)
{
    cvmx_safe_printf("cvmx_interrupt_default: Received interrupt %d\n", irq_number);
    __cvmx_interrupt_dump_registers(registers);
}


/**
 * @INTERNAL
 * Handler for interrupt lines 2 and 3. These are directly tied
 * to the CIU. The handler queres the status of the CIU and
 * calls the secondary handler for the CIU interrupt that
 * occurred.
 *
 * @param irq_number Interrupt number that fired (2 or 3)
 * @param registers  Registers at the time of the interrupt
 * @param user_arg   Unused user argument
 */
static void __cvmx_interrupt_ciu(int irq_number, uint64_t registers[32], void *user_arg)
{
    int ciu_offset = cvmx_get_core_num() * 2 + irq_number - 2;
    uint64_t irq_mask = cvmx_read_csr(CVMX_CIU_INTX_SUM0(ciu_offset)) & cvmx_read_csr(CVMX_CIU_INTX_EN0(ciu_offset));
    int irq = 8;

    /* Handle EN0 sources */
    while (irq_mask)
    {
        if (irq_mask&1)
        {
            cvmx_interrupt_state.handlers[irq](irq, registers, cvmx_interrupt_state.data[irq]);
            return;
        }
        irq_mask = irq_mask >> 1;
        irq++;
    }

    /* Handle EN1 sources */
    irq_mask = cvmx_read_csr(CVMX_CIU_INT_SUM1) & cvmx_read_csr(CVMX_CIU_INTX_EN1(ciu_offset));
    irq = 8 + 64;
    while (irq_mask)
    {
        if (irq_mask&1)
        {
            cvmx_interrupt_state.handlers[irq](irq, registers, cvmx_interrupt_state.data[irq]);
            return;
        }
        irq_mask = irq_mask >> 1;
        irq++;
    }
}


/**
 * @INTERNAL
 * Called for all RML interrupts. This is usually an ECC error
 *
 * @param irq_number Interrupt number that we're being called for
 * @param registers  Registers at the time of the interrupt
 * @param user_arg   Unused user argument
 */
static void __cvmx_interrupt_ecc(int irq_number, uint64_t registers[32], void *user_arg)
{
    cvmx_error_poll();
}


/**
 * Process an interrupt request
 *
 * @param registers Registers at time of interrupt / exception
 * Registers 0-31 are standard MIPS, others specific to this routine
 * @return
 */
EXTERN_ASM void cvmx_interrupt_do_irq(uint64_t registers[35]);
void cvmx_interrupt_do_irq(uint64_t registers[35])
{
    uint64_t        mask;
    uint64_t        cause;
    uint64_t        status;
    uint64_t        cache_err;
    int             i;
    uint32_t exc_vec;
    /* Determine the cause of the interrupt */
    asm volatile ("dmfc0 %0,$13,0" : "=r" (cause));
    asm volatile ("dmfc0 %0,$12,0" : "=r" (status));
    /* In case of exception, clear all interrupts to avoid recursive interrupts.
       Also clear EXL bit to display the correct PC value. */
    if ((cause & 0x7c) == 0)
    {
        asm volatile ("dmtc0 %0, $12, 0" : : "r" (status & ~(0xff02)));
    }
    /* The assembly stub at each exception vector saves its address in k1 when
    ** it calls the stage 2 handler.  We use this to compute the exception vector
    ** that brought us here */
    exc_vec = (uint32_t)(registers[27] & 0x780);  /* Mask off bits we need to ignore */

    /* Check for cache errors.  The cache errors go to a separate exception vector,
    ** so we will only check these if we got here from a cache error exception, and
    ** the ERL (error level) bit is set. */
    if (exc_vec == 0x100 && (status & 0x4))
    {
        i = cvmx_get_core_num();
        CVMX_MF_CACHE_ERR(cache_err);

        /* Use copy of DCACHE_ERR register that early exception stub read */
        if (registers[34] & 0x1)
        {
            cvmx_safe_printf("Dcache error detected: core: %d, set: %d, va 6:3: 0x%x\n", i, (int)(cache_err >> 3) & 0x3, (int)(cache_err >> 3) & 0xf);
            uint64_t dcache_err = 0;
            CVMX_MT_DCACHE_ERR(dcache_err);
        }
        else if (cache_err & 0x1)
        {
            cvmx_safe_printf("Icache error detected: core: %d, set: %d, way : %d\n", i, (int)(cache_err >> 5) & 0x3f, (int)(cache_err >> 7) & 0x3);
            cache_err = 0;
            CVMX_MT_CACHE_ERR(cache_err);
        }
        else
            cvmx_safe_printf("Cache error exception: core %d\n", i);
    }

    if ((cause & 0x7c) != 0)
    {
        cvmx_interrupt_state.exception_handler(registers);
        goto return_from_interrupt;
    }

    /* Convert the cause into an active mask */
    mask = ((cause & status) >> 8) & 0xff;
    if (mask == 0)
    {
        goto return_from_interrupt; /* Spurious interrupt */
    }

    for (i=0; i<8; i++)
    {
        if (mask & (1<<i))
        {
            cvmx_interrupt_state.handlers[i](i, registers, cvmx_interrupt_state.data[i]);
            goto return_from_interrupt;
        }
    }

    /* We should never get here */
    __cvmx_interrupt_default_exception_handler(registers);

return_from_interrupt:
    /* Restore Status register before returning from exception. */
    asm volatile ("dmtc0 %0, $12, 0" : : "r" (status));
}


/**
 * Initialize the interrupt routine and copy the low level
 * stub into the correct interrupt vector. This is called
 * automatically during application startup.
 */
void cvmx_interrupt_initialize(void)
{
    void *low_level_loc;
    cvmx_sysinfo_t *sys_info_ptr = cvmx_sysinfo_get();
    int i;

    /* Disable all CIU interrupts by default */
    cvmx_write_csr(CVMX_CIU_INTX_EN0(cvmx_get_core_num()*2), 0);
    cvmx_write_csr(CVMX_CIU_INTX_EN0(cvmx_get_core_num()*2+1), 0);
    cvmx_write_csr(CVMX_CIU_INTX_EN1(cvmx_get_core_num()*2), 0);
    cvmx_write_csr(CVMX_CIU_INTX_EN1(cvmx_get_core_num()*2+1), 0);

    if (cvmx_coremask_first_core(sys_info_ptr->core_mask))
    {
        cvmx_interrupt_state.exception_handler = __cvmx_interrupt_default_exception_handler;

        for (i=0; i<256; i++)
        {
            cvmx_interrupt_state.handlers[i] = __cvmx_interrupt_default;
            cvmx_interrupt_state.data[i] = NULL;
        }

        low_level_loc = CASTPTR(void, CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0,sys_info_ptr->exception_base_addr));
        memcpy(low_level_loc + 0x80, (void*)cvmx_interrupt_stage1, 0x80);
        memcpy(low_level_loc + 0x100, (void*)cvmx_interrupt_cache_error, 0x80);
        memcpy(low_level_loc + 0x180, (void*)cvmx_interrupt_stage1, 0x80);
        memcpy(low_level_loc + 0x200, (void*)cvmx_interrupt_stage1, 0x80);

        /* Make sure the locations used to count Icache and Dcache exceptions
            starts out as zero */
        cvmx_write64_uint64(CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0, 8), 0);
        cvmx_write64_uint64(CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0, 16), 0);
        cvmx_write64_uint64(CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0, 24), 0);
        CVMX_SYNC;

        /* Add an interrupt handlers for chained CIU interrupts */
        cvmx_interrupt_register(CVMX_IRQ_CIU0, __cvmx_interrupt_ciu, NULL);
        cvmx_interrupt_register(CVMX_IRQ_CIU1, __cvmx_interrupt_ciu, NULL);

        /* Add an interrupt handler for ECC failures */
        cvmx_interrupt_register(CVMX_IRQ_RML, __cvmx_interrupt_ecc, NULL);

        if (cvmx_error_initialize(0 /* || CVMX_ERROR_FLAGS_ECC_SINGLE_BIT */))
            cvmx_warn("cvmx_error_initialize() failed\n");
        cvmx_interrupt_unmask_irq(CVMX_IRQ_RML);
    }

    cvmx_interrupt_unmask_irq(CVMX_IRQ_CIU0);
    cvmx_interrupt_unmask_irq(CVMX_IRQ_CIU1);
    CVMX_ICACHE_INVALIDATE;

    /* Enable interrupts for each core (bit0 of COP0 Status) */
    uint32_t mask;
    asm volatile (
        "mfc0   %0,$12,0\n"
        "ori    %0, %0, 1\n"
        "mtc0   %0,$12,0\n"
        : "=r" (mask));
}


/**
 * Register an interrupt handler for the specified interrupt number.
 *
 * @param irq_number Interrupt number to register for (0-135)  See
 *                   cvmx-interrupt.h for enumeration and description of sources.
 * @param func       Function to call on interrupt.
 * @param user_arg   User data to pass to the interrupt handler
 */
void cvmx_interrupt_register(cvmx_irq_t irq_number, cvmx_interrupt_func_t func, void *user_arg)
{
    cvmx_interrupt_state.handlers[irq_number] = func;
    cvmx_interrupt_state.data[irq_number] = user_arg;
    CVMX_SYNCWS;
}


/**
 * Set the exception handler for all non interrupt sources.
 *
 * @param handler New exception handler
 * @return Old exception handler
 */
cvmx_interrupt_exception_t cvmx_interrupt_set_exception(cvmx_interrupt_exception_t handler)
{
    cvmx_interrupt_exception_t result = cvmx_interrupt_state.exception_handler;
    cvmx_interrupt_state.exception_handler = handler;
    CVMX_SYNCWS;
    return result;
}
#endif /* !__U_BOOT__ */


