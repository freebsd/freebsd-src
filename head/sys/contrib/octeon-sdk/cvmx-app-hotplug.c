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
 * Provides APIs for applications to register for hotplug. It also provides 
 * APIs for requesting shutdown of a running target application. 
 *
 * <hr>$Revision: $<hr>
 */

#include "cvmx-app-hotplug.h"
#include "cvmx-spinlock.h"

//#define DEBUG 1

#ifndef CVMX_BUILD_FOR_LINUX_USER

static CVMX_SHARED cvmx_spinlock_t cvmx_app_hotplug_sync_lock = { CVMX_SPINLOCK_UNLOCKED_VAL };
static CVMX_SHARED cvmx_spinlock_t cvmx_app_hotplug_lock = { CVMX_SPINLOCK_UNLOCKED_VAL };
static CVMX_SHARED cvmx_app_hotplug_info_t *cvmx_app_hotplug_info_ptr = NULL;

static void __cvmx_app_hotplug_shutdown(int irq_number, uint64_t registers[32], void *user_arg);
static void __cvmx_app_hotplug_sync(void);
static void __cvmx_app_hotplug_reset(void);

/**
 * This routine registers an application for hotplug. It installs a handler for
 * any incoming shutdown request. It also registers a callback routine from the
 * application. This callback is invoked when the application receives a 
 * shutdown notification. 
 *
 * This routine only needs to be called once per application. 
 *
 * @param fn      Callback routine from the application. 
 * @param arg     Argument to the application callback routine. 
 * @return        Return 0 on success, -1 on failure
 *
 */
int cvmx_app_hotplug_register(void(*fn)(void*), void* arg)
{
    /* Find the list of applications launched by bootoct utility. */

    if (!(cvmx_app_hotplug_info_ptr = cvmx_app_hotplug_get_info(cvmx_sysinfo_get()->core_mask)))
    {
        /* Application not launched by bootoct? */
        printf("ERROR: cmvx_app_hotplug_register() failed\n");
        return -1;
    }

    /* Register the callback */
    cvmx_app_hotplug_info_ptr->data = CAST64(arg);
    cvmx_app_hotplug_info_ptr->shutdown_callback = CAST64(fn);

#ifdef DEBUG
    cvmx_dprintf("cvmx_app_hotplug_register(): coremask 0x%x valid %d\n", 
                  cvmx_app_hotplug_info_ptr->coremask, cvmx_app_hotplug_info_ptr->valid);
#endif

    cvmx_interrupt_register(CVMX_IRQ_MBOX0, __cvmx_app_hotplug_shutdown, NULL);

    return 0;
}

/**
 * Activate the current application core for receiving hotplug shutdown requests. 
 *
 * This routine makes sure that each core belonging to the application is enabled 
 * to receive the shutdown notification and also provides a barrier sync to make
 * sure that all cores are ready. 
 */
int cvmx_app_hotplug_activate(void)
{
    /* Make sure all application cores are activating */
    __cvmx_app_hotplug_sync();

    cvmx_spinlock_lock(&cvmx_app_hotplug_lock);

    if (!cvmx_app_hotplug_info_ptr)
    {
        cvmx_spinlock_unlock(&cvmx_app_hotplug_lock);
        printf("ERROR: This application is not registered for hotplug\n");
	return -1;
    }

    /* Enable the interrupt before we mark the core as activated */
    cvmx_interrupt_unmask_irq(CVMX_IRQ_MBOX0);

    cvmx_app_hotplug_info_ptr->hotplug_activated_coremask |= (1<<cvmx_get_core_num());

#ifdef DEBUG
    cvmx_dprintf("cvmx_app_hotplug_activate(): coremask 0x%x valid %d sizeof %d\n", 
                 cvmx_app_hotplug_info_ptr->coremask, cvmx_app_hotplug_info_ptr->valid, 
                 sizeof(*cvmx_app_hotplug_info_ptr));
#endif

    cvmx_spinlock_unlock(&cvmx_app_hotplug_lock);

    return 0;
}

/**
 * This routine is only required if cvmx_app_hotplug_shutdown_request() was called
 * with wait=0. This routine waits for the application shutdown to complete. 
 *
 * @param coremask     Coremask the application is running on. 
 * @return             0 on success, -1 on error
 *
 */
int cvmx_app_hotplug_shutdown_complete(uint32_t coremask)
{
    cvmx_app_hotplug_info_t *hotplug_info_ptr;

    if (!(hotplug_info_ptr = cvmx_app_hotplug_get_info(coremask)))
    {
        printf("\nERROR: Failed to get hotplug info for coremask: 0x%x\n", (unsigned int)coremask);
        return -1;
    }

    while(!hotplug_info_ptr->shutdown_done);

    /* Clean up the hotplug info region for this app */
    bzero(hotplug_info_ptr, sizeof(*hotplug_info_ptr));

    return 0;
}

/**
 * Disable recognition of any incoming shutdown request. 
 */

void cvmx_app_hotplug_shutdown_disable(void)
{
    cvmx_interrupt_mask_irq(CVMX_IRQ_MBOX0);
}

/**
 * Re-enable recognition of incoming shutdown requests.
 */

void cvmx_app_hotplug_shutdown_enable(void)
{
    cvmx_interrupt_unmask_irq(CVMX_IRQ_MBOX0);
}

/*
 * ISR for the incoming shutdown request interrupt. 
 */
static void __cvmx_app_hotplug_shutdown(int irq_number, uint64_t registers[32], void *user_arg)
{
    cvmx_sysinfo_t *sys_info_ptr = cvmx_sysinfo_get();
    uint32_t flags;

    cvmx_interrupt_mask_irq(CVMX_IRQ_MBOX0);

    /* Clear the interrupt */
    cvmx_write_csr(CVMX_CIU_MBOX_CLRX(cvmx_get_core_num()), 1);

    /* Make sure the write above completes */
    cvmx_read_csr(CVMX_CIU_MBOX_CLRX(cvmx_get_core_num()));

    if (!cvmx_app_hotplug_info_ptr)
    {
        printf("ERROR: Application is not registered for hotplug!\n");
        return;
    }

    if (cvmx_app_hotplug_info_ptr->hotplug_activated_coremask != sys_info_ptr->core_mask)
    {
        printf("ERROR: Shutdown requested when not all app cores have activated hotplug\n"
	       "Application coremask: 0x%x Hotplug coremask: 0x%x\n", (unsigned int)sys_info_ptr->core_mask, 
	       (unsigned int)cvmx_app_hotplug_info_ptr->hotplug_activated_coremask);
	return;
    }

    /* Call the application's own callback function */
    ((void(*)(void*))(long)cvmx_app_hotplug_info_ptr->shutdown_callback)(CASTPTR(void *, cvmx_app_hotplug_info_ptr->data));

    __cvmx_app_hotplug_sync();

    if (cvmx_coremask_first_core(sys_info_ptr->core_mask))
    {
        bzero(cvmx_app_hotplug_info_ptr, sizeof(*cvmx_app_hotplug_info_ptr));
#ifdef DEBUG
        cvmx_dprintf("__cvmx_app_hotplug_shutdown(): setting shutdown done! \n");
#endif
        cvmx_app_hotplug_info_ptr->shutdown_done = 1;
    }

    flags = cvmx_interrupt_disable_save();

    __cvmx_app_hotplug_sync();

    /* Reset the core */
    __cvmx_app_hotplug_reset();
}

/*
 * Reset the core. We just jump back to the reset vector for now. 
 */
void __cvmx_app_hotplug_reset(void)
{
    /* Code from SecondaryCoreLoop from bootloader, sleep until we recieve
       a NMI. */
    __asm__ volatile (
        ".set noreorder      \n"
	"\tsync               \n"
	"\tnop               \n"
        "1:\twait            \n"
        "\tb 1b              \n"
	"\tnop               \n"             
	".set reorder        \n"
	:: 
    );
}

/* 
 * We need a separate sync operation from cvmx_coremask_barrier_sync() to
 * avoid a deadlock on state.lock, since the application itself maybe doing a
 * cvmx_coremask_barrier_sync(). 
 */
static void __cvmx_app_hotplug_sync(void)
{
    static CVMX_SHARED volatile uint32_t sync_coremask = 0;
    cvmx_sysinfo_t *sys_info_ptr = cvmx_sysinfo_get();

    cvmx_spinlock_lock(&cvmx_app_hotplug_sync_lock);
    
    sync_coremask |= cvmx_coremask_core(cvmx_get_core_num());

    cvmx_spinlock_unlock(&cvmx_app_hotplug_sync_lock);

    while (sync_coremask != sys_info_ptr->core_mask);
}

#endif /* CVMX_BUILD_FOR_LINUX_USER */

/**
 * Return the hotplug info structure (cvmx_app_hotplug_info_t) pointer for the 
 * application running on the given coremask. 
 *
 * @param coremask     Coremask of application. 
 * @return             Returns hotplug info struct on success, NULL on failure
 *
 */
cvmx_app_hotplug_info_t* cvmx_app_hotplug_get_info(uint32_t coremask)
{
    const struct cvmx_bootmem_named_block_desc *block_desc;
    cvmx_app_hotplug_info_t *hip;
    cvmx_app_hotplug_global_t *hgp;
    int i;

    block_desc = cvmx_bootmem_find_named_block(CVMX_APP_HOTPLUG_INFO_REGION_NAME);

    if (!block_desc)
    {
        printf("ERROR: Hotplug info region is not setup\n");
        return NULL;
    }
    else

#ifdef CVMX_BUILD_FOR_LINUX_USER
    {
        size_t pg_sz = sysconf(_SC_PAGESIZE), size;
        off_t offset;
        char *vaddr;
        int fd;

        if ((fd = open("/dev/mem", O_RDWR)) == -1) {
            perror("open");
            return NULL;
        }

        /*
         * We need to mmap() this memory, since this was allocated from the 
         * kernel bootup code and does not reside in the RESERVE32 region.
         */
        size = CVMX_APP_HOTPLUG_INFO_REGION_SIZE + pg_sz-1;
        offset = block_desc->base_addr & ~(pg_sz-1);
        if ((vaddr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, offset)) == MAP_FAILED) 
        {
            perror("mmap");
            return NULL;
        }

        hgp = (cvmx_app_hotplug_global_t *)(vaddr + ( block_desc->base_addr & (pg_sz-1)));
    }
#else
    hgp = cvmx_phys_to_ptr(block_desc->base_addr);
#endif

    hip = hgp->hotplug_info_array;

#ifdef DEBUG
    cvmx_dprintf("cvmx_app_hotplug_get_info(): hotplug_info phy addr 0x%llx ptr %p\n", 
                  block_desc->base_addr, hgp);
#endif

    /* Look for the current app's info */

    for (i=0; i<CVMX_APP_HOTPLUG_MAX_APPS; i++)
    {
        if (hip[i].coremask == coremask)
	{
#ifdef DEBUG
	    cvmx_dprintf("cvmx_app_hotplug_get_info(): coremask match %d -- coremask 0x%x valid %d\n", 
	                 i, hip[i].coremask, hip[i].valid);
#endif

	    return &hip[i];
	}
    }

    return NULL;
}

/**
 * This routine sends a shutdown request to a running target application. 
 *
 * @param coremask     Coremask the application is running on. 
 * @param wait         1 - Wait for shutdown completion
 *                     0 - Do not wait
 * @return             0 on success, -1 on error
 *
 */

int cvmx_app_hotplug_shutdown_request(uint32_t coremask, int wait) 
{
    int i;
    cvmx_app_hotplug_info_t *hotplug_info_ptr;

    if (!(hotplug_info_ptr = cvmx_app_hotplug_get_info(coremask)))
    {
        printf("\nERROR: Failed to get hotplug info for coremask: 0x%x\n", (unsigned int)coremask);
        return -1;
    }

    if (!hotplug_info_ptr->shutdown_callback)
    {
        printf("\nERROR: Target application has not registered for hotplug!\n");
        return -1;
    }

    if (hotplug_info_ptr->hotplug_activated_coremask != coremask)
    {
        printf("\nERROR: Not all application cores have activated hotplug\n");
        return -1;
    }

    /* Send IPIs to all application cores to request shutdown */
    for (i=0; i<CVMX_MAX_CORES; i++) {
    	if (coremask & (1<<i))
		cvmx_write_csr(CVMX_CIU_MBOX_SETX(i), 1);
    }

    if (wait)
    {
        while (!hotplug_info_ptr->shutdown_done);    

        /* Clean up the hotplug info region for this application */
        bzero(hotplug_info_ptr, sizeof(*hotplug_info_ptr));
    }

    return 0;
}
