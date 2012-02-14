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
 * <hr>$Revision: 49448 $<hr>
 */
#ifndef __CVMX_INTERRUPT_H__
#define __CVMX_INTERRUPT_H__

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * Enumeration of Interrupt numbers
 */
typedef enum
{
    /* 0 - 7 represent the 8 MIPS standard interrupt sources */
    CVMX_IRQ_SW0        = 0,
    CVMX_IRQ_SW1        = 1,
    CVMX_IRQ_CIU0       = 2,
    CVMX_IRQ_CIU1       = 3,
    CVMX_IRQ_4          = 4,
    CVMX_IRQ_5          = 5,
    CVMX_IRQ_6          = 6,
    CVMX_IRQ_7          = 7,

    /* 8 - 71 represent the sources in CIU_INTX_EN0 */
    CVMX_IRQ_WORKQ0     = 8,
    CVMX_IRQ_WORKQ1     = 9,
    CVMX_IRQ_WORKQ2     = 10,
    CVMX_IRQ_WORKQ3     = 11,
    CVMX_IRQ_WORKQ4     = 12,
    CVMX_IRQ_WORKQ5     = 13,
    CVMX_IRQ_WORKQ6     = 14,
    CVMX_IRQ_WORKQ7     = 15,
    CVMX_IRQ_WORKQ8     = 16,
    CVMX_IRQ_WORKQ9     = 17,
    CVMX_IRQ_WORKQ10    = 18,
    CVMX_IRQ_WORKQ11    = 19,
    CVMX_IRQ_WORKQ12    = 20,
    CVMX_IRQ_WORKQ13    = 21,
    CVMX_IRQ_WORKQ14    = 22,
    CVMX_IRQ_WORKQ15    = 23,
    CVMX_IRQ_GPIO0      = 24,
    CVMX_IRQ_GPIO1      = 25,
    CVMX_IRQ_GPIO2      = 26,
    CVMX_IRQ_GPIO3      = 27,
    CVMX_IRQ_GPIO4      = 28,
    CVMX_IRQ_GPIO5      = 29,
    CVMX_IRQ_GPIO6      = 30,
    CVMX_IRQ_GPIO7      = 31,
    CVMX_IRQ_GPIO8      = 32,
    CVMX_IRQ_GPIO9      = 33,
    CVMX_IRQ_GPIO10     = 34,
    CVMX_IRQ_GPIO11     = 35,
    CVMX_IRQ_GPIO12     = 36,
    CVMX_IRQ_GPIO13     = 37,
    CVMX_IRQ_GPIO14     = 38,
    CVMX_IRQ_GPIO15     = 39,
    CVMX_IRQ_MBOX0      = 40,
    CVMX_IRQ_MBOX1      = 41,
    CVMX_IRQ_UART0      = 42,
    CVMX_IRQ_UART1      = 43,
    CVMX_IRQ_PCI_INT0   = 44,
    CVMX_IRQ_PCI_INT1   = 45,
    CVMX_IRQ_PCI_INT2   = 46,
    CVMX_IRQ_PCI_INT3   = 47,
    CVMX_IRQ_PCI_MSI0   = 48,
    CVMX_IRQ_PCI_MSI1   = 49,
    CVMX_IRQ_PCI_MSI2   = 50,
    CVMX_IRQ_PCI_MSI3   = 51,
    CVMX_IRQ_RESERVED44 = 52,
    CVMX_IRQ_TWSI       = 53,
    CVMX_IRQ_RML        = 54,
    CVMX_IRQ_TRACE      = 55,
    CVMX_IRQ_GMX_DRP0   = 56,
    CVMX_IRQ_GMX_DRP1   = 57,   /* Doesn't apply on CN52XX or CN63XX */
    CVMX_IRQ_IPD_DRP    = 58,
    CVMX_IRQ_KEY_ZERO   = 59,   /* Doesn't apply on CN52XX or CN63XX */
    CVMX_IRQ_TIMER0     = 60,
    CVMX_IRQ_TIMER1     = 61,
    CVMX_IRQ_TIMER2     = 62,
    CVMX_IRQ_TIMER3     = 63,
    CVMX_IRQ_USB0       = 64,   /* Doesn't apply on CN38XX or CN58XX */
    CVMX_IRQ_PCM        = 65,   /* Doesn't apply on CN52XX or CN63XX */
    CVMX_IRQ_MPI        = 66,   /* Doesn't apply on CN52XX or CN63XX */
    CVMX_IRQ_TWSI2      = 67,   /* Added in CN56XX */
    CVMX_IRQ_POWIQ      = 68,   /* Added in CN56XX */
    CVMX_IRQ_IPDPPTHR   = 69,   /* Added in CN56XX */
    CVMX_IRQ_MII        = 70,   /* Added in CN56XX */
    CVMX_IRQ_BOOTDMA    = 71,   /* Added in CN56XX */

    /* 72 - 135 represent the sources in CIU_INTX_EN1 */
    CVMX_IRQ_WDOG0 = 72,
    CVMX_IRQ_WDOG1 = 73,
    CVMX_IRQ_WDOG2 = 74,
    CVMX_IRQ_WDOG3 = 75,
    CVMX_IRQ_WDOG4 = 76,
    CVMX_IRQ_WDOG5 = 77,
    CVMX_IRQ_WDOG6 = 78,
    CVMX_IRQ_WDOG7 = 79,
    CVMX_IRQ_WDOG8 = 80,
    CVMX_IRQ_WDOG9 = 81,
    CVMX_IRQ_WDOG10= 82,
    CVMX_IRQ_WDOG11= 83,
    CVMX_IRQ_WDOG12= 84,
    CVMX_IRQ_WDOG13= 85,
    CVMX_IRQ_WDOG14= 86,
    CVMX_IRQ_WDOG15= 87,
    CVMX_IRQ_UART2 = 88,           /* Added in CN52XX */
    CVMX_IRQ_USB1  = 89,           /* Added in CN52XX */
    CVMX_IRQ_MII1  = 90,           /* Added in CN52XX */
    CVMX_IRQ_NAND  = 91,           /* Added in CN52XX */
    CVMX_IRQ_MIO   = 92,           /* Added in CN63XX */
    CVMX_IRQ_IOB   = 93,           /* Added in CN63XX */
    CVMX_IRQ_FPA   = 94,           /* Added in CN63XX */
    CVMX_IRQ_POW   = 95,           /* Added in CN63XX */
    CVMX_IRQ_L2C   = 96,           /* Added in CN63XX */
    CVMX_IRQ_IPD   = 97,           /* Added in CN63XX */
    CVMX_IRQ_PIP   = 98,           /* Added in CN63XX */
    CVMX_IRQ_PKO   = 99,           /* Added in CN63XX */
    CVMX_IRQ_ZIP   = 100,          /* Added in CN63XX */
    CVMX_IRQ_TIM   = 101,          /* Added in CN63XX */
    CVMX_IRQ_RAD   = 102,          /* Added in CN63XX */
    CVMX_IRQ_KEY   = 103,          /* Added in CN63XX */
    CVMX_IRQ_DFA   = 104,          /* Added in CN63XX */
    CVMX_IRQ_USB   = 105,          /* Added in CN63XX */
    CVMX_IRQ_SLI   = 106,          /* Added in CN63XX */
    CVMX_IRQ_DPI   = 107,          /* Added in CN63XX */
    CVMX_IRQ_AGX0  = 108,          /* Added in CN63XX */
    /* 109 - 117 are reserved */
    CVMX_IRQ_AGL   = 118,          /* Added in CN63XX */
    CVMX_IRQ_PTP   = 119,          /* Added in CN63XX */
    CVMX_IRQ_PEM0  = 120,          /* Added in CN63XX */
    CVMX_IRQ_PEM1  = 121,          /* Added in CN63XX */
    CVMX_IRQ_SRIO0 = 122,          /* Added in CN63XX */
    CVMX_IRQ_SRIO1 = 123,          /* Added in CN63XX */
    CVMX_IRQ_LMC0  = 124,          /* Added in CN63XX */
    /* Interrupts 125 - 127 are reserved */
    CVMX_IRQ_DFM   = 128,          /* Added in CN63XX */
    /* Interrupts 129 - 135 are reserved */
} cvmx_irq_t;

/**
 * Function prototype for the exception handler
 */
typedef void (*cvmx_interrupt_exception_t)(uint64_t registers[32]);

/**
 * Function prototype for interrupt handlers
 */
typedef void (*cvmx_interrupt_func_t)(int irq_number, uint64_t registers[32], void *user_arg);

/**
 * Register an interrupt handler for the specified interrupt number.
 *
 * @param irq_number Interrupt number to register for (0-135)
 * @param func       Function to call on interrupt.
 * @param user_arg   User data to pass to the interrupt handler
 */
void cvmx_interrupt_register(cvmx_irq_t irq_number, cvmx_interrupt_func_t func, void *user_arg);

/**
 * Set the exception handler for all non interrupt sources.
 *
 * @param handler New exception handler
 * @return Old exception handler
 */
cvmx_interrupt_exception_t cvmx_interrupt_set_exception(cvmx_interrupt_exception_t handler);

/**
 * Masks a given interrupt number.
 * EN0 sources are masked on IP2
 * EN1 sources are masked on IP3
 *
 * @param irq_number interrupt number to mask (0-135)
 */
static inline void cvmx_interrupt_mask_irq(int irq_number)
{
    if (irq_number<8)
    {
        uint32_t mask;
        asm volatile ("mfc0 %0,$12,0" : "=r" (mask));
        mask &= ~(1<< (8 + irq_number));
        asm volatile ("mtc0 %0,$12,0" : : "r" (mask));
    }
    else if (irq_number < 8 + 64)
    {
        int ciu_bit = (irq_number - 8) & 63;
        int ciu_offset = cvmx_get_core_num() * 2;
        uint64_t mask = cvmx_read_csr(CVMX_CIU_INTX_EN0(ciu_offset));
        mask &= ~(1ull << ciu_bit);
        cvmx_write_csr(CVMX_CIU_INTX_EN0(ciu_offset), mask);
    }
    else
    {
        int ciu_bit = (irq_number - 8) & 63;
        int ciu_offset = cvmx_get_core_num() * 2 + 1;
        uint64_t mask = cvmx_read_csr(CVMX_CIU_INTX_EN1(ciu_offset));
        mask &= ~(1ull << ciu_bit);
        cvmx_write_csr(CVMX_CIU_INTX_EN1(ciu_offset), mask);
    }
}


/**
 * Unmasks a given interrupt number
 * EN0 sources are unmasked on IP2
 * EN1 sources are unmasked on IP3
 *
 * @param irq_number interrupt number to unmask (0-135)
 */
static inline void cvmx_interrupt_unmask_irq(int irq_number)
{
    if (irq_number<8)
    {
        uint32_t mask;
        asm volatile ("mfc0 %0,$12,0" : "=r" (mask));
        mask |= (1<< (8 + irq_number));
        asm volatile ("mtc0 %0,$12,0" : : "r" (mask));
    }
    else if (irq_number < 8 + 64)
    {
        int ciu_bit = (irq_number - 8) & 63;
        int ciu_offset = cvmx_get_core_num() * 2;
        uint64_t mask = cvmx_read_csr(CVMX_CIU_INTX_EN0(ciu_offset));
        mask |= (1ull << ciu_bit);
        cvmx_write_csr(CVMX_CIU_INTX_EN0(ciu_offset), mask);
    }
    else
    {
        int ciu_bit = (irq_number - 8) & 63;
        int ciu_offset = cvmx_get_core_num() * 2 + 1;
        uint64_t mask = cvmx_read_csr(CVMX_CIU_INTX_EN1(ciu_offset));
        mask |= (1ull << ciu_bit);
        cvmx_write_csr(CVMX_CIU_INTX_EN1(ciu_offset), mask);
    }
}


/* Disable interrupts by clearing bit 0 of the COP0 status register,
** and return the previous contents of the status register.
** Note: this is only used to track interrupt status. */
static inline uint32_t cvmx_interrupt_disable_save(void)
{
    uint32_t flags;
    asm volatile (
        "DI   %[flags]\n"
        : [flags]"=r" (flags));
    return(flags);
}

/* Restore the contents of the cop0 status register.  Used with
** cvmx_interrupt_disable_save to allow recursive interrupt disabling */
static inline void cvmx_interrupt_restore(uint32_t flags)
{
    /* If flags value indicates interrupts should be enabled, then enable them */
    if (flags & 1)
    {
        asm volatile (
            "EI     \n"
            ::);
    }
}

#define cvmx_local_irq_save(x) ({x = cvmx_interrupt_disable_save();})
#define cvmx_local_irq_restore(x) cvmx_interrupt_restore(x)

/**
 * Utility function to do interrupt safe printf
 */
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
	#define cvmx_safe_printf printk
#elif defined(CVMX_BUILD_FOR_LINUX_USER)
	#define cvmx_safe_printf printf
#else
	extern void cvmx_safe_printf(const char* format, ... ) __attribute__ ((format(printf, 1, 2)));
#endif

#define PRINT_ERROR(format, ...) cvmx_safe_printf("ERROR " format, ##__VA_ARGS__)

#ifdef	__cplusplus
}
#endif

#endif
