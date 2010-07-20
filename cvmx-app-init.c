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






#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cvmx-config.h"
#include "cvmx.h"
#include "cvmx-spinlock.h"
#include <octeon-app-init.h>
#include "cvmx-sysinfo.h"
#include "cvmx-bootmem.h"
#include "cvmx-uart.h"
#include "cvmx-ciu.h"
#include "cvmx-coremask.h"
#include "cvmx-core.h"
#include "cvmx-interrupt.h"
#include "cvmx-ebt3000.h"
#include "../../bootloader/u-boot/include/octeon_mem_map.h"

int cvmx_debug_uart;

/**
 * @file
 *
 * Main entry point for all simple executive based programs.
 */


extern void cvmx_interrupt_initialize(void);



/**
 * Main entry point for all simple executive based programs.
 * This is the first C function called. It completes
 * initialization, calls main, and performs C level cleanup.
 *
 * @param app_desc_addr
 *               Address of the application description structure passed
 *               brom the boot loader.
 */
EXTERN_ASM void __cvmx_app_init(uint64_t app_desc_addr);


/**
 * Set up sysinfo structure from boot descriptor versions 6 and higher.
 * In these versions, the interesting data in not in the boot info structure
 * defined by the toolchain, but is in the cvmx_bootinfo structure defined in
 * the simple exec.
 *
 * @param app_desc_ptr
 *               pointer to boot descriptor block
 *
 * @param sys_info_ptr
 *               pointer to sysinfo structure to fill in
 */
static void process_boot_desc_ver_6(octeon_boot_descriptor_t *app_desc_ptr, cvmx_sysinfo_t *sys_info_ptr)
{
    cvmx_bootinfo_t *cvmx_bootinfo_ptr = CASTPTR(cvmx_bootinfo_t, app_desc_ptr->cvmx_desc_vaddr);

    /* copy application information for simple exec use */
    /* Populate the sys_info structure from the boot descriptor block created by the bootloader.
    ** The boot descriptor block is put in the top of the heap, so it will be overwritten when the
    ** heap is fully used.  Information that is to be used must be copied before that.
    ** Applications should only use the sys_info structure, not the boot descriptor
    */
    if (cvmx_bootinfo_ptr->major_version == 1)
    {
        sys_info_ptr->core_mask = cvmx_bootinfo_ptr->core_mask;
        sys_info_ptr->heap_base = cvmx_bootinfo_ptr->heap_base;
        sys_info_ptr->heap_size = cvmx_bootinfo_ptr->heap_end - cvmx_bootinfo_ptr->heap_base;
        sys_info_ptr->stack_top = cvmx_bootinfo_ptr->stack_top;
        sys_info_ptr->stack_size = cvmx_bootinfo_ptr->stack_size;
        sys_info_ptr->init_core = cvmx_get_core_num();
        sys_info_ptr->phy_mem_desc_ptr = CASTPTR(void, CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0, cvmx_bootinfo_ptr->phy_mem_desc_addr));
        sys_info_ptr->exception_base_addr = cvmx_bootinfo_ptr->exception_base_addr;
        sys_info_ptr->cpu_clock_hz  = cvmx_bootinfo_ptr->eclock_hz;
        sys_info_ptr->dram_data_rate_hz  = cvmx_bootinfo_ptr->dclock_hz * 2;

        sys_info_ptr->board_type = cvmx_bootinfo_ptr->board_type;
        sys_info_ptr->board_rev_major = cvmx_bootinfo_ptr->board_rev_major;
        sys_info_ptr->board_rev_minor = cvmx_bootinfo_ptr->board_rev_minor;
        memcpy(sys_info_ptr->mac_addr_base, cvmx_bootinfo_ptr->mac_addr_base, 6);
        sys_info_ptr->mac_addr_count = cvmx_bootinfo_ptr->mac_addr_count;
        memcpy(sys_info_ptr->board_serial_number, cvmx_bootinfo_ptr->board_serial_number, CVMX_BOOTINFO_OCTEON_SERIAL_LEN);
        sys_info_ptr->console_uart_num = 0;
        if (cvmx_bootinfo_ptr->flags & OCTEON_BL_FLAG_CONSOLE_UART1)
            sys_info_ptr->console_uart_num = 1;

        if (cvmx_bootinfo_ptr->dram_size > 16*1024*1024)
            sys_info_ptr->system_dram_size = (uint64_t)cvmx_bootinfo_ptr->dram_size;  /* older bootloaders incorrectly gave this in bytes, so don't convert */
        else
            sys_info_ptr->system_dram_size = (uint64_t)cvmx_bootinfo_ptr->dram_size * 1024 * 1024;  /* convert from Megabytes to bytes */
        if (cvmx_bootinfo_ptr->minor_version >= 1)
        {
            sys_info_ptr->compact_flash_common_base_addr = cvmx_bootinfo_ptr->compact_flash_common_base_addr;
            sys_info_ptr->compact_flash_attribute_base_addr = cvmx_bootinfo_ptr->compact_flash_attribute_base_addr;
            sys_info_ptr->led_display_base_addr = cvmx_bootinfo_ptr->led_display_base_addr;
        }
        else if (sys_info_ptr->board_type == CVMX_BOARD_TYPE_EBT3000 ||
                 sys_info_ptr->board_type == CVMX_BOARD_TYPE_EBT5800)
        {
            /* Default these variables so that users of structure can be the same no
            ** matter what version fo boot info block the bootloader passes */
            sys_info_ptr->compact_flash_common_base_addr = 0x1d000000 + 0x800;
            sys_info_ptr->compact_flash_attribute_base_addr = 0x1d010000;
            if (sys_info_ptr->board_rev_major == 1)
                sys_info_ptr->led_display_base_addr = 0x1d020000;
            else
                sys_info_ptr->led_display_base_addr = 0x1d020000 + 0xf8;
        }
        else
        {
            sys_info_ptr->compact_flash_common_base_addr = 0;
            sys_info_ptr->compact_flash_attribute_base_addr = 0;
            sys_info_ptr->led_display_base_addr = 0;
        }

        if (cvmx_bootinfo_ptr->minor_version >= 2)
        {
            sys_info_ptr->dfa_ref_clock_hz = cvmx_bootinfo_ptr->dfa_ref_clock_hz;
            sys_info_ptr->bootloader_config_flags = cvmx_bootinfo_ptr->config_flags;
        }
        else
        {
            sys_info_ptr->dfa_ref_clock_hz = 0;
            sys_info_ptr->bootloader_config_flags = 0;
            if (app_desc_ptr->flags & OCTEON_BL_FLAG_DEBUG)
                sys_info_ptr->bootloader_config_flags |= CVMX_BOOTINFO_CFG_FLAG_DEBUG;
            if (app_desc_ptr->flags & OCTEON_BL_FLAG_NO_MAGIC)
                sys_info_ptr->bootloader_config_flags |= CVMX_BOOTINFO_CFG_FLAG_NO_MAGIC;
        }

    }
    else
    {
        printf("ERROR: Incompatible CVMX descriptor passed by bootloader: %d.%d\n",
               (int)cvmx_bootinfo_ptr->major_version, (int)cvmx_bootinfo_ptr->minor_version);
        while (1);
    }
}


/**
 * Interrupt handler for debugger Control-C interrupts.
 *
 * @param irq_number IRQ interrupt number
 * @param registers  CPU registers at the time of the interrupt
 * @param user_arg   Unused user argument
 */
static void process_debug_interrupt(int irq_number, uint64_t registers[32], void *user_arg)
{
    int uart = irq_number - CVMX_IRQ_UART0;
    cvmx_uart_lsr_t lsrval;

    /* Check for a Control-C interrupt from the debugger. This loop will eat
        all input received on the uart */
    lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(uart));
    while (lsrval.s.dr)
    {
        int c = cvmx_read_csr(CVMX_MIO_UARTX_RBR(uart));
        if (c == '\003')
        {
            register uint64_t tmp;
            fflush(stderr);
            fflush(stdout);
            /* Pulse MCD0 signal on Ctrl-C to stop all the cores. Also
                set the MCD0 to be not masked by this core so we know
                the signal is received by someone */
            asm volatile (
                "dmfc0 %0, $22\n"
                "ori   %0, %0, 0x1110\n"
                "dmtc0 %0, $22\n"
                : "=r" (tmp));
        }
        lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(uart));
    }
}

/**
 * Interrupt handler for calling exit on Control-C interrupts.
 *
 * @param irq_number IRQ interrupt number
 * @param registers  CPU registers at the time of the interrupt
 * @param user_arg   Unused user argument
 */
static void process_break_interrupt(int irq_number, uint64_t registers[32], void *user_arg)
{
    /* Exclude new functionality when building with older toolchains */
#if OCTEON_APP_INIT_H_VERSION >= 3
    int uart = irq_number - CVMX_IRQ_UART0;
    cvmx_uart_lsr_t lsrval;

    /* Check for a Control-C interrupt from the console. This loop will eat
        all input received on the uart */
    lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(uart));
    while (lsrval.s.dr)
    {
        int c = cvmx_read_csr(CVMX_MIO_UARTX_RBR(uart));
        if (c == '\003')
        {
            register uint64_t tmp;

	    /* Wait for an another Control-C if right now we have no
	       access to the console.  After this point we hold the
	       lock and use a different lock to synchronize between
	       the memfile dumps from different cores.  As a
	       consequence regular printfs *don't* work after this
	       point! */
	    if (__octeon_uart_trylock () == 1)
		return;

            /* Pulse MCD0 signal on Ctrl-C to stop all the cores. Also
	       set the MCD0 to be not masked by this core so we know
	       the signal is received by someone */
            asm volatile (
                "dmfc0 %0, $22\n"
                "ori   %0, %0, 0x1110\n"
                "dmtc0 %0, $22\n"
                : "=r" (tmp));
        }
        lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(uart));
    }
#endif
}

/**
 * This is the debug exception handler with "break".  Before calling exit to
 * dump the profile-feedback output it releases the lock on the console.
 * This way if there is buffered data in stdout it can still be flushed.
 * stdio is required to flush all output during an fread.
 */

static void exit_on_break(void)
{
#if OCTEON_APP_INIT_H_VERSION >= 4
    unsigned int coremask = cvmx_sysinfo_get()->core_mask;

    cvmx_coremask_barrier_sync(coremask);
    if (cvmx_coremask_first_core(coremask))
      __octeon_uart_unlock();
#endif

    exit(0);
}

/* Add string signature to applications so that we can easily tell what
** Octeon revision they were compiled for. Don't make static to avoid unused
** variable warning. */
#define xstr(s) str(s)
#define str(s) #s

int octeon_model_version_check(uint32_t chip_id);

#define OMS xstr(OCTEON_MODEL)
char octeon_rev_signature[] =
#ifdef USE_RUNTIME_MODEL_CHECKS
    "Compiled for runtime Octeon model checking";
#else
    "Compiled for Octeon processor id: "OMS;
#endif

void __cvmx_app_init(uint64_t app_desc_addr)
{
    /* App descriptor used by bootloader */
    octeon_boot_descriptor_t *app_desc_ptr = CASTPTR(octeon_boot_descriptor_t, app_desc_addr);

    /* app info structure used by the simple exec */
    cvmx_sysinfo_t *sys_info_ptr = cvmx_sysinfo_get();

    if (cvmx_coremask_first_core(app_desc_ptr->core_mask))
    {
        /* do once per application setup  */
        if (app_desc_ptr->desc_version < 6)
        {
            printf("Obsolete bootloader, can't run application\n");
            while (1)
                ;
        }
        else
        {
            /* Handle all newer versions here.... */
            if (app_desc_ptr->desc_version > 7)
            {
                printf("Warning: newer boot descripter version than expected\n");
            }
            process_boot_desc_ver_6(app_desc_ptr,sys_info_ptr);

        }
    }
    cvmx_coremask_barrier_sync(app_desc_ptr->core_mask);

    /* All cores need to enable MCD0 signals if the debugger flag is set */
    if (sys_info_ptr->bootloader_config_flags & CVMX_BOOTINFO_CFG_FLAG_DEBUG)
    {
        /* Set all cores to stop on MCD0 signals */
        uint64_t tmp;
        asm volatile(
            "dmfc0 %0, $22, 0\n"
            "or %0, %0, 0x1100\n"
            "dmtc0 %0, $22, 0\n" : "=r" (tmp));
    }

    cvmx_interrupt_initialize();

    if (cvmx_coremask_first_core(sys_info_ptr->core_mask))
    {
        /* Check to make sure the Chip version matches the configured version */
        uint32_t chip_id = cvmx_get_proc_id();
	int debugflag = sys_info_ptr->bootloader_config_flags & CVMX_BOOTINFO_CFG_FLAG_DEBUG;
	int breakflag = sys_info_ptr->bootloader_config_flags & CVMX_BOOTINFO_CFG_FLAG_BREAK;
	int uart;

        /* Intialize the bootmem allocator with the descriptor that was provided by
        ** the bootloader
        ** IMPORTANT:  All printfs must happen after this since PCI console uses named
        ** blocks.
        */
        cvmx_bootmem_init(sys_info_ptr->phy_mem_desc_ptr);

        /* Make sure we can properly run on this chip */
        octeon_model_version_check(chip_id);

	/* Default to the second uart port.  Set this even if debug was
	   not passed.  The idea is that if the program crashes one would
	   be able to break in on uart1 even without debug.  */
	cvmx_debug_uart = 1;
        /* If the debugger flag is set, setup the uart Control-C interrupt
            handler */
        if (debugflag)
        {
            /* Search through the arguments for a debug=X */
            unsigned int i;
            for (i=0; i<app_desc_ptr->argc; i++)
            {
                const char *argv = CASTPTR(const char, CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0, app_desc_ptr->argv[i]));
                if (strncmp(argv, "debug=", 6) == 0)
                {
                    /* Use the supplied uart as an override */
                    cvmx_debug_uart = atoi(argv+6);
                    break;
                }
            }
	    cvmx_interrupt_register(CVMX_IRQ_UART0+cvmx_debug_uart, process_debug_interrupt, NULL);
	    uart = cvmx_debug_uart;
	}
	else if (breakflag)
	{
            unsigned int i;
	    int32_t *trampoline = CASTPTR(int32_t, CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0, BOOTLOADER_DEBUG_TRAMPOLINE));
	    /* Default to the first uart port. */
	    uart = 0;

            /* Search through the arguments for a break=X */
            for (i = 0; i < app_desc_ptr->argc; i++)
            {
                const char *argv = CASTPTR(const char, CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0, app_desc_ptr->argv[i]));
                if (strncmp(argv, "break=", 6) == 0)
                {
                    /* Use the supplied uart as an override */
                    uart = atoi(argv+6);
                    break;
                }
            }

	    /* On debug exception, call exit_on_break from all cores. */
	    *trampoline = (int32_t)(long)&exit_on_break;
	    cvmx_interrupt_register(CVMX_IRQ_UART0 + uart, process_break_interrupt, NULL);
	}
	if (debugflag || breakflag)
	{
            /* Enable uart interrupts for debugger Control-C processing */
            cvmx_uart_ier_t ier;
            ier.u64 = cvmx_read_csr(CVMX_MIO_UARTX_IER(uart));
            ier.s.erbfi = 1;
            cvmx_write_csr(CVMX_MIO_UARTX_IER(uart), ier.u64);

            cvmx_interrupt_unmask_irq(CVMX_IRQ_UART0+uart);
        }
    }

    /* Clear BEV now that we have installed exception handlers. */
    uint64_t tmp;
    asm volatile (
               "   .set push                  \n"
               "   .set mips64                  \n"
               "   .set noreorder               \n"
               "   .set noat               \n"
               "   mfc0 %[tmp], $12, 0          \n"
               "   li   $at, 1 << 22            \n"
               "   not  $at, $at                \n"
               "   and  %[tmp], $at             \n"
               "   mtc0 %[tmp], $12, 0          \n"
               "   .set pop                  \n"
                  : [tmp] "=&r" (tmp) : );

    /* Set all cores to stop on MCD0 signals */
    asm volatile(
        "dmfc0 %0, $22, 0\n"
        "or %0, %0, 0x1100\n"
        "dmtc0 %0, $22, 0\n" : "=r" (tmp));

    CVMX_SYNC;
    /* Synchronise all cores at this point */
    cvmx_coremask_barrier_sync(app_desc_ptr->core_mask);

}

int cvmx_user_app_init(void)
{
    uint64_t bist_val;
    uint64_t mask;
    int bist_errors = 0;
    uint64_t tmp;
    uint64_t base_addr;


    /* Put message on LED display */
    if (cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_SIM)
      ebt3000_str_write("CVMX    ");

    /* Check BIST results for COP0 registers, some values only meaningful in pass 2 */
    CVMX_MF_CACHE_ERR(bist_val);
    mask = (1ULL<<32) | (1ULL<<33) | (1ULL<<34) | (1ULL<<35) | (1ULL<<36);
    bist_val &= mask;
    if (bist_val)
    {
        printf("BIST FAILURE: COP0_CACHE_ERR: 0x%llx\n", (unsigned long long)bist_val);
        bist_errors++;
    }
    /* Clear parity error bits */
    CVMX_MF_CACHE_ERR(bist_val);
    bist_val &= ~0x7ull;
    CVMX_MT_CACHE_ERR(bist_val);


    mask = 0xfc00000000000000ull;
    CVMX_MF_CVM_MEM_CTL(bist_val);
    bist_val &=  mask;
    if (bist_val)
    {
        printf("BIST FAILURE: COP0_CVM_MEM_CTL: 0x%llx\n", (unsigned long long)bist_val);
        bist_errors++;
    }

    /* Clear DCACHE parity error bit */
    bist_val = 0;
    CVMX_MF_DCACHE_ERR(bist_val);

    mask =     0x18ull;
    bist_val = cvmx_read_csr(CVMX_L2D_ERR);
    if (bist_val & mask)
    {
        printf("ERROR: ECC error detected in L2 Data, L2D_ERR: 0x%llx\n", (unsigned long long)bist_val);
        cvmx_write_csr(CVMX_L2D_ERR, bist_val); /* Clear error bits if set */
    }
    bist_val = cvmx_read_csr(CVMX_L2T_ERR);
    if (bist_val & mask)
    {
        printf("ERROR: ECC error detected in L2 Tags, L2T_ERR: 0x%llx\n", (unsigned long long)bist_val);
        cvmx_write_csr(CVMX_L2T_ERR, bist_val); /* Clear error bits if set */
    }


    /* Set up 4 cache lines of local memory, make available from Kernel space */
    CVMX_MF_CVM_MEM_CTL(tmp);
    tmp &= ~0x1ffull;
    tmp |= 0x104ull;
    CVMX_MT_CVM_MEM_CTL(tmp);


#if CVMX_USE_1_TO_1_TLB_MAPPINGS

    /* Check to see if the bootloader is indicating that the application is outside
    ** of the 0x10000000 0x20000000 range, in which case we can't use 1-1 mappings */
    if (cvmx_sysinfo_get()->bootloader_config_flags & CVMX_BOOTINFO_CFG_FLAG_OVERSIZE_TLB_MAPPING)
    {
        printf("ERROR: 1-1 TLB mappings configured and oversize application loaded.\n");
        printf("ERROR: Either 1-1 TLB mappings must be disabled or application size reduced.\n");
        while (1)
            ;
    }


    /* Create 1-1 Mappings for all DRAM up to 8 gigs, excluding the low 1 Megabyte.  This area
    ** is reserved for the bootloader and exception vectors.  By not mapping this area, NULL pointer
    ** dereferences will be caught with TLB exceptions.  Exception handlers should be written
    ** using XKPHYS or KSEG0 addresses. */
#if CVMX_NULL_POINTER_PROTECT
    /* Exclude low 1 MByte from mapping to detect NULL pointer accesses.
    ** The only down side of this is it uses more TLB mappings */
    cvmx_core_add_fixed_tlb_mapping_bits(0x0, 0x0, 0x100000  | TLB_DIRTY | TLB_VALID | TLB_GLOBAL, CVMX_TLB_PAGEMASK_1M);
    cvmx_core_add_fixed_tlb_mapping(0x200000, 0x200000, 0x300000, CVMX_TLB_PAGEMASK_1M);
    cvmx_core_add_fixed_tlb_mapping(0x400000, 0x400000, 0x500000, CVMX_TLB_PAGEMASK_1M);
    cvmx_core_add_fixed_tlb_mapping(0x600000, 0x600000, 0x700000, CVMX_TLB_PAGEMASK_1M);

    cvmx_core_add_fixed_tlb_mapping(0x800000,  0x800000,  0xC00000, CVMX_TLB_PAGEMASK_4M);
    cvmx_core_add_fixed_tlb_mapping(0x1000000, 0x1000000, 0x1400000, CVMX_TLB_PAGEMASK_4M);
    cvmx_core_add_fixed_tlb_mapping(0x1800000, 0x1800000, 0x1c00000, CVMX_TLB_PAGEMASK_4M);

    cvmx_core_add_fixed_tlb_mapping(0x2000000, 0x2000000, 0x3000000, CVMX_TLB_PAGEMASK_16M);
    cvmx_core_add_fixed_tlb_mapping(0x4000000, 0x4000000, 0x5000000, CVMX_TLB_PAGEMASK_16M);
    cvmx_core_add_fixed_tlb_mapping(0x6000000, 0x6000000, 0x7000000, CVMX_TLB_PAGEMASK_16M);
#else
    /* Map entire low 128 Megs, including 0x0 */
    cvmx_core_add_fixed_tlb_mapping(0x0, 0x0, 0x4000000ULL, CVMX_TLB_PAGEMASK_64M);
#endif
    cvmx_core_add_fixed_tlb_mapping(0x8000000ULL, 0x8000000ULL, 0xc000000ULL, CVMX_TLB_PAGEMASK_64M);

    /* Create 1-1 mapping for next 256 megs
    ** bottom page is not valid */
    cvmx_core_add_fixed_tlb_mapping_bits(0x400000000ULL, 0, 0x410000000ULL  | TLB_DIRTY | TLB_VALID | TLB_GLOBAL, CVMX_TLB_PAGEMASK_256M);

    /* Map from 0.5 up to the installed memory size in 512 MByte chunks.  If this loop runs out of memory,
    ** the NULL pointer detection can be disabled to free up more TLB entries. */
    if (cvmx_sysinfo_get()->system_dram_size > 0x20000000ULL)
    {
        for (base_addr = 0x20000000ULL; base_addr <= (cvmx_sysinfo_get()->system_dram_size - 0x20000000ULL); base_addr += 0x20000000ULL)
        {
            if (0 > cvmx_core_add_fixed_tlb_mapping(base_addr,  base_addr,  base_addr + 0x10000000ULL, CVMX_TLB_PAGEMASK_256M))
            {
                printf("ERROR adding 1-1 TLB mapping for address 0x%llx\n", (unsigned long long)base_addr);
                while (1);    /* Hang here, as expected memory mappings aren't set up if this fails */
            }
        }
    }


#endif


    cvmx_sysinfo_t *sys_info_ptr = cvmx_sysinfo_get();
    cvmx_bootmem_init(sys_info_ptr->phy_mem_desc_ptr);

    return(0);
}

void __cvmx_app_exit(void)
{
    if (cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_SIM)
    {
       uint64_t val;
       uint64_t mask, expected;
       int bist_errors = 0;

       mask =     0x1ull;
       expected = 0x0ull;
       CVMX_MF_DCACHE_ERR(val);
       val = (val & mask) ^ expected;
       if (val)
       {
           printf("DCACHE Parity error: 0x%llx\n", (unsigned long long)val);
           bist_errors++;
       }

       mask =     0x18ull;
       expected = 0x0ull;
       val = cvmx_read_csr(CVMX_L2D_ERR);
       val = (val & mask) ^ expected;
       if (val)
       {
           printf("L2 Parity error: 0x%llx\n", (unsigned long long)val);
           bist_errors++;
       }


       while (1)
           ;

    }
}



