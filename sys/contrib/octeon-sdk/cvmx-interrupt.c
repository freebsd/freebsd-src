/***********************license start***************
 *  Copyright (c) 2003-2008 Cavium Networks (support@cavium.com). All rights
 *  reserved.
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Cavium Networks nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 *  TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *  AND WITH ALL FAULTS AND CAVIUM NETWORKS MAKES NO PROMISES, REPRESENTATIONS
 *  OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *  RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
 *  REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
 *  DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
 *  PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
 *  POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
 *  OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 *
 *
 *  For any questions regarding licensing please contact marketing@caviumnetworks.com
 *
 ***********************license end**************************************/






/**
 * @file
 *
 * Interface to the Mips interrupts.
 *
 * <hr>$Revision: 42264 $<hr>
 */
#if __GNUC__ >= 4
/* Backtrace is only available with the new toolchain.  */
#include <execinfo.h>
#endif
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

EXTERN_ASM void cvmx_interrupt_stage1(void);
EXTERN_ASM void cvmx_interrupt_cache_error(void);

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
static CVMX_SHARED cvmx_interrupt_state_t cvmx_interrupt_state;
static CVMX_SHARED cvmx_spinlock_t cvmx_interrupt_default_lock;

#define COP0_CAUSE      "$13,0"
#define COP0_STATUS     "$12,0"
#define COP0_BADVADDR   "$8,0"
#define COP0_EPC        "$14,0"
#define READ_COP0(dest, R) asm volatile ("dmfc0 %[rt]," R : [rt] "=r" (dest))
#define ULL unsigned long long



/**
 * @INTERNAL
 * Dump all useful registers to the console
 *
 * @param registers CPU register to dump
 */
static void __cvmx_interrupt_dump_registers(uint64_t registers[32])
{
    static const char *name[32] = {"r0","at","v0","v1","a0","a1","a2","a3",
        "t0","t1","t2","t3","t4","t5","t6","t7","s0","s1","s2","s3","s4","s5",
        "s6","s7", "t8","t9", "k0","k1","gp","sp","s8","ra"};
    uint64_t reg;
    for (reg=0; reg<16; reg++)
    {
        cvmx_safe_printf("%3s ($%02d): 0x%016llx \t %3s ($%02d): 0x%016llx\n",
               name[reg], (int)reg, (ULL)registers[reg], name[reg+16], (int)reg+16, (ULL)registers[reg+16]);
    }
    READ_COP0(reg, COP0_CAUSE);
    cvmx_safe_printf("%16s: 0x%016llx\n", "COP0_CAUSE", (ULL)reg);
    READ_COP0(reg, COP0_STATUS);
    cvmx_safe_printf("%16s: 0x%016llx\n", "COP0_STATUS", (ULL)reg);
    READ_COP0(reg, COP0_BADVADDR);
    cvmx_safe_printf("%16s: 0x%016llx\n", "COP0_BADVADDR", (ULL)reg);
    READ_COP0(reg, COP0_EPC);
    cvmx_safe_printf("%16s: 0x%016llx\n", "COP0_EPC", (ULL)reg);
}


/**
 * @INTERNAL
 * Default exception handler. Prints out the exception
 * cause decode and all relevant registers.
 *
 * @param registers Registers at time of the exception
 */
static void __cvmx_interrupt_default_exception_handler(uint64_t registers[32])
{
    uint64_t trap_print_cause;

    ebt3000_str_write("Trap");
    cvmx_spinlock_lock(&cvmx_interrupt_default_lock);
    cvmx_safe_printf("******************************************************************\n");
    cvmx_safe_printf("Core %d: Unhandled Exception. Cause register decodes to:\n", (int)cvmx_get_core_num());
    READ_COP0(trap_print_cause, COP0_CAUSE);
    switch ((trap_print_cause >> 2) & 0x1f)
    {
        case 0x0:
            cvmx_safe_printf("Interrupt\n");
            break;
        case 0x1:
            cvmx_safe_printf("TLB Mod\n");
            break;
        case 0x2:
            cvmx_safe_printf("tlb load/fetch\n");
            break;
        case 0x3:
            cvmx_safe_printf("tlb store\n");
            break;
        case 0x4:
            cvmx_safe_printf("address exc, load/fetch\n");
            break;
        case 0x5:
            cvmx_safe_printf("address exc, store\n");
            break;
        case 0x6:
            cvmx_safe_printf("bus error, inst. fetch\n");
            break;
        case 0x7:
            cvmx_safe_printf("bus error, load/store\n");
            break;
        case 0x8:
            cvmx_safe_printf("syscall\n");
            break;
        case 0x9:
            cvmx_safe_printf("breakpoint \n");
            break;
        case 0xa:
            cvmx_safe_printf("reserved instruction\n");
            break;
        case 0xb:
            cvmx_safe_printf("cop unusable\n");
            break;
        case 0xc:
            cvmx_safe_printf("arithmetic overflow\n");
            break;
        case 0xd:
            cvmx_safe_printf("trap\n");
            break;
        case 0xf:
            cvmx_safe_printf("floating point exc\n");
            break;
        case 0x12:
            cvmx_safe_printf("cop2 exception\n");
            break;
        case 0x16:
            cvmx_safe_printf("mdmx unusable\n");
            break;
        case 0x17:
            cvmx_safe_printf("watch\n");
            break;
        case 0x18:
            cvmx_safe_printf("machine check\n");
            break;
        case 0x1e:
            cvmx_safe_printf("cache error\n");
            break;
        default:
            cvmx_safe_printf("Reserved exception cause.\n");
            break;

    }

    cvmx_safe_printf("******************************************************************\n");
    __cvmx_interrupt_dump_registers(registers);
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
}


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
    cvmx_interrupt_rsl_decode();
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
        return;
    }

    /* Convert the cause into an active mask */
    mask = ((cause & status) >> 8) & 0xff;
    if (mask == 0)
        return; /* Spurious interrupt */

    for (i=0; i<8; i++)
    {
        if (mask & (1<<i))
        {
            cvmx_interrupt_state.handlers[i](i, registers, cvmx_interrupt_state.data[i]);
            return;
        }
    }

    /* We should never get here */
    __cvmx_interrupt_default_exception_handler(registers);
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

        cvmx_interrupt_rsl_enable();
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


/**
 * version of printf that works better in exception context.
 *
 * @param format
 */
void cvmx_safe_printf(const char *format, ...)
{
    static char buffer[256];
    va_list args;
    va_start(args, format);
    int count = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    char *ptr = buffer;
    while (count-- > 0)
    {
        cvmx_uart_lsr_t lsrval;

        /* Spin until there is room */
        do
        {
            lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(0));
            if (lsrval.s.temt == 0)
                cvmx_wait(10000);   /* Just to reduce the load on the system */
        }
        while (lsrval.s.temt == 0);

        if (*ptr == '\n')
            cvmx_write_csr(CVMX_MIO_UARTX_THR(0), '\r');
        cvmx_write_csr(CVMX_MIO_UARTX_THR(0), *ptr++);
    }
}






