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
 * Simple allocate only memory allocator.  Used to allocate memory at application
 * start time.
 *
 * <hr>$Revision: 41586 $<hr>
 *
 */

#include "cvmx.h"
#include "cvmx-spinlock.h"
#include "cvmx-bootmem.h"


//#define DEBUG


#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#define ALIGN_ADDR_UP(addr, align)     (((addr) + (~(align))) & (align))

static CVMX_SHARED cvmx_bootmem_desc_t *cvmx_bootmem_desc = NULL;

/* See header file for descriptions of functions */

/* Wrapper functions are provided for reading/writing the size and next block
** values as these may not be directly addressible (in 32 bit applications, for instance.)
*/
/* Offsets of data elements in bootmem list, must match cvmx_bootmem_block_header_t */
#define NEXT_OFFSET 0
#define SIZE_OFFSET 8
static void cvmx_bootmem_phy_set_size(uint64_t addr, uint64_t size)
{
    cvmx_write64_uint64((addr + SIZE_OFFSET) | (1ull << 63), size);
}
static void cvmx_bootmem_phy_set_next(uint64_t addr, uint64_t next)
{
    cvmx_write64_uint64((addr + NEXT_OFFSET) | (1ull << 63), next);
}
static uint64_t cvmx_bootmem_phy_get_size(uint64_t addr)
{
    return(cvmx_read64_uint64((addr + SIZE_OFFSET) | (1ull << 63)));
}
static uint64_t cvmx_bootmem_phy_get_next(uint64_t addr)
{
    return(cvmx_read64_uint64((addr + NEXT_OFFSET) | (1ull << 63)));
}


/* This functions takes an address range and adjusts it as necessary to
** match the ABI that is currently being used.  This is required to ensure
** that bootmem_alloc* functions only return valid pointers for 32 bit ABIs */
static int __cvmx_validate_mem_range(uint64_t *min_addr_ptr, uint64_t *max_addr_ptr)
{

#if defined(__linux__) && defined(CVMX_ABI_N32)
    {
        extern uint64_t linux_mem32_min;
        extern uint64_t linux_mem32_max;
        /* For 32 bit Linux apps, we need to restrict the allocations to the range
        ** of memory configured for access from userspace.  Also, we need to add mappings
        ** for the data structures that we access.*/

        /* Narrow range requests to be bounded by the 32 bit limits.  octeon_phy_mem_block_alloc()
        ** will reject inconsistent req_size/range requests, so we don't repeat those checks here.
        ** If max unspecified, set to 32 bit maximum. */
        *min_addr_ptr = MIN(MAX(*min_addr_ptr, linux_mem32_min), linux_mem32_max);
        if (!*max_addr_ptr)
            *max_addr_ptr = linux_mem32_max;
        else
            *max_addr_ptr = MAX(MIN(*max_addr_ptr, linux_mem32_max), linux_mem32_min);
    }
#elif defined(CVMX_ABI_N32)
    {
        uint32_t max_phys = 0x0FFFFFFF;  /* Max physical address when 1-1 mappings not used */
#if CVMX_USE_1_TO_1_TLB_MAPPINGS
        max_phys = 0x7FFFFFFF;
#endif
        /* We are are running standalone simple executive, so we need to limit the range
        ** that we allocate from */

        /* Narrow range requests to be bounded by the 32 bit limits.  octeon_phy_mem_block_alloc()
        ** will reject inconsistent req_size/range requests, so we don't repeat those checks here.
        ** If max unspecified, set to 32 bit maximum. */
        *min_addr_ptr = MIN(MAX(*min_addr_ptr, 0x0), max_phys);
        if (!*max_addr_ptr)
            *max_addr_ptr = max_phys;
        else
            *max_addr_ptr = MAX(MIN(*max_addr_ptr, max_phys), 0x0);
    }
#endif

    return 0;
}


void *cvmx_bootmem_alloc_range(uint64_t size, uint64_t alignment, uint64_t min_addr, uint64_t max_addr)
{
    int64_t address;

    __cvmx_validate_mem_range(&min_addr, &max_addr);
    address = cvmx_bootmem_phy_alloc(size, min_addr, max_addr, alignment, 0);

    if (address > 0)
        return cvmx_phys_to_ptr(address);
    else
        return NULL;
}

void *cvmx_bootmem_alloc_address(uint64_t size, uint64_t address, uint64_t alignment)
{
    return cvmx_bootmem_alloc_range(size, alignment, address, address + size);
}


void *cvmx_bootmem_alloc(uint64_t size, uint64_t alignment)
{
    return cvmx_bootmem_alloc_range(size, alignment, 0, 0);
}

void *cvmx_bootmem_alloc_named_range(uint64_t size, uint64_t min_addr, uint64_t max_addr, uint64_t align, char *name)
{
    int64_t addr;

    __cvmx_validate_mem_range(&min_addr, &max_addr);
    addr = cvmx_bootmem_phy_named_block_alloc(size, min_addr, max_addr, align, name, 0);
    if (addr >= 0)
        return cvmx_phys_to_ptr(addr);
    else
        return NULL;

}
void *cvmx_bootmem_alloc_named_address(uint64_t size, uint64_t address, char *name)
{
    return(cvmx_bootmem_alloc_named_range(size, address, address + size, 0, name));
}
void *cvmx_bootmem_alloc_named(uint64_t size, uint64_t alignment, char *name)
{
    return(cvmx_bootmem_alloc_named_range(size, 0, 0, alignment, name));
}

int cvmx_bootmem_free_named(char *name)
{
    return(cvmx_bootmem_phy_named_block_free(name, 0));
}

cvmx_bootmem_named_block_desc_t * cvmx_bootmem_find_named_block(char *name)
{
    return(cvmx_bootmem_phy_named_block_find(name, 0));
}

void cvmx_bootmem_print_named(void)
{
    cvmx_bootmem_phy_named_block_print();
}

#if defined(__linux__) && defined(CVMX_ABI_N32)
cvmx_bootmem_named_block_desc_t *linux32_named_block_array_ptr;
#endif

int cvmx_bootmem_init(void *mem_desc_ptr)
{
    /* Verify that the size of cvmx_spinlock_t meets our assumptions */
    if (sizeof(cvmx_spinlock_t) != 4)
    {
        cvmx_dprintf("ERROR: Unexpected size of cvmx_spinlock_t\n");
        return(-1);
    }

    /* Here we set the global pointer to the bootmem descriptor block.  This pointer will
    ** be used directly, so we will set it up to be directly usable by the application.
    ** It is set up as follows for the various runtime/ABI combinations:
    ** Linux 64 bit: Set XKPHYS bit
    ** Linux 32 bit: use mmap to create mapping, use virtual address
    ** CVMX 64 bit:  use physical address directly
    ** CVMX 32 bit:  use physical address directly
    ** Note that the CVMX environment assumes the use of 1-1 TLB mappings so that the physical addresses
    ** can be used directly
    */
    if (!cvmx_bootmem_desc)
    {
#if defined(CVMX_BUILD_FOR_LINUX_USER) && defined(CVMX_ABI_N32)
        void *base_ptr;
        /* For 32 bit, we need to use mmap to create a mapping for the bootmem descriptor */
        int dm_fd = open("/dev/mem", O_RDWR);
        if (dm_fd < 0)
        {
            cvmx_dprintf("ERROR opening /dev/mem for boot descriptor mapping\n");
            return(-1);
        }

        base_ptr = mmap(NULL,
                        sizeof(cvmx_bootmem_desc_t) + sysconf(_SC_PAGESIZE),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        dm_fd,
                        ((off_t)mem_desc_ptr) & ~(sysconf(_SC_PAGESIZE) - 1));

        if (MAP_FAILED == base_ptr)
        {
            cvmx_dprintf("Error mapping bootmem descriptor!\n");
            close(dm_fd);
            return(-1);
        }

        /* Adjust pointer to point to bootmem_descriptor, rather than start of page it is in */
        cvmx_bootmem_desc =  (cvmx_bootmem_desc_t*)((char*)base_ptr + (((off_t)mem_desc_ptr) & (sysconf(_SC_PAGESIZE) - 1)));

        /* Also setup mapping for named memory block desc. while we are at it.  Here we must keep another
        ** pointer around, as the value in the bootmem descriptor is shared with other applications. */
        base_ptr = mmap(NULL,
                        sizeof(cvmx_bootmem_named_block_desc_t) * cvmx_bootmem_desc->named_block_num_blocks + sysconf(_SC_PAGESIZE),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        dm_fd,
                        ((off_t)cvmx_bootmem_desc->named_block_array_addr) & ~(sysconf(_SC_PAGESIZE) - 1));

        close(dm_fd);

        if (MAP_FAILED == base_ptr)
        {
            cvmx_dprintf("Error mapping named block descriptor!\n");
            return(-1);
        }

        /* Adjust pointer to point to named block array, rather than start of page it is in */
        linux32_named_block_array_ptr = (cvmx_bootmem_named_block_desc_t*)((char*)base_ptr + (((off_t)cvmx_bootmem_desc->named_block_array_addr) & (sysconf(_SC_PAGESIZE) - 1)));

#elif (defined(CVMX_BUILD_FOR_LINUX_KERNEL) || defined(CVMX_BUILD_FOR_LINUX_USER)) && defined(CVMX_ABI_64)
        /* Set XKPHYS bit */
        cvmx_bootmem_desc = cvmx_phys_to_ptr(CAST64(mem_desc_ptr));
#else
        cvmx_bootmem_desc = (cvmx_bootmem_desc_t*)mem_desc_ptr;
#endif
    }


    return(0);
}


uint64_t cvmx_bootmem_available_mem(uint64_t min_block_size)
{
    return(cvmx_bootmem_phy_available_mem(min_block_size));
}





/*********************************************************************
** The cvmx_bootmem_phy* functions below return 64 bit physical addresses,
** and expose more features that the cvmx_bootmem_functions above.  These are
** required for full memory space access in 32 bit applications, as well as for
** using some advance features.
** Most applications should not need to use these.
**
**/


int64_t cvmx_bootmem_phy_alloc(uint64_t req_size, uint64_t address_min, uint64_t address_max, uint64_t alignment, uint32_t flags)
{

    uint64_t head_addr;
    uint64_t ent_addr;
    uint64_t prev_addr = 0;  /* points to previous list entry, NULL current entry is head of list */
    uint64_t new_ent_addr = 0;
    uint64_t desired_min_addr;
    uint64_t alignment_mask = ~(alignment - 1);

#ifdef DEBUG
    cvmx_dprintf("cvmx_bootmem_phy_alloc: req_size: 0x%llx, min_addr: 0x%llx, max_addr: 0x%llx, align: 0x%llx\n",
           (unsigned long long)req_size, (unsigned long long)address_min, (unsigned long long)address_max, (unsigned long long)alignment);
#endif

    if (cvmx_bootmem_desc->major_version > 3)
    {
        cvmx_dprintf("ERROR: Incompatible bootmem descriptor version: %d.%d at addr: %p\n",
               (int)cvmx_bootmem_desc->major_version, (int)cvmx_bootmem_desc->minor_version, cvmx_bootmem_desc);
        goto error_out;
    }

    /* Do a variety of checks to validate the arguments.  The allocator code will later assume
    ** that these checks have been made.  We validate that the requested constraints are not
    ** self-contradictory before we look through the list of available memory
    */

    /* 0 is not a valid req_size for this allocator */
    if (!req_size)
        goto error_out;

    /* Round req_size up to mult of minimum alignment bytes */
    req_size = (req_size + (CVMX_BOOTMEM_ALIGNMENT_SIZE - 1)) & ~(CVMX_BOOTMEM_ALIGNMENT_SIZE - 1);

    /* Convert !0 address_min and 0 address_max to special case of range that specifies an exact
    ** memory block to allocate.  Do this before other checks and adjustments so that this tranformation will be validated */
    if (address_min && !address_max)
        address_max = address_min + req_size;
    else if (!address_min && !address_max)
        address_max = ~0ull;   /* If no limits given, use max limits */




    /* Enforce minimum alignment (this also keeps the minimum free block
    ** req_size the same as the alignment req_size */
    if (alignment < CVMX_BOOTMEM_ALIGNMENT_SIZE)
    {
        alignment = CVMX_BOOTMEM_ALIGNMENT_SIZE;
    }
    alignment_mask = ~(alignment - 1);

    /* Adjust address minimum based on requested alignment (round up to meet alignment).  Do this here so we can
    ** reject impossible requests up front. (NOP for address_min == 0) */
    if (alignment)
        address_min = (address_min + (alignment - 1)) & ~(alignment - 1);


    /* Reject inconsistent args.  We have adjusted these, so this may fail due to our internal changes
    ** even if this check would pass for the values the user supplied. */
    if (req_size > address_max - address_min)
        goto error_out;

    /* Walk through the list entries - first fit found is returned */

    if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
        cvmx_spinlock_lock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
    head_addr = cvmx_bootmem_desc->head_addr;
    ent_addr = head_addr;
    while (ent_addr)
    {
        uint64_t usable_base, usable_max;
        uint64_t ent_size = cvmx_bootmem_phy_get_size(ent_addr);

        if (cvmx_bootmem_phy_get_next(ent_addr) && ent_addr > cvmx_bootmem_phy_get_next(ent_addr))
        {
            cvmx_dprintf("Internal bootmem_alloc() error: ent: 0x%llx, next: 0x%llx\n",
                   (unsigned long long)ent_addr, (unsigned long long)cvmx_bootmem_phy_get_next(ent_addr));
            goto error_out;
        }

        /* Determine if this is an entry that can satisify the request */
        /* Check to make sure entry is large enough to satisfy request */
        usable_base = ALIGN_ADDR_UP(MAX(address_min, ent_addr), alignment_mask);
        usable_max = MIN(address_max, ent_addr + ent_size);
        /* We should be able to allocate block at address usable_base */

        desired_min_addr = usable_base;

        /* Determine if request can be satisfied from the current entry */
        if ((((ent_addr + ent_size) > usable_base && ent_addr < address_max))
            && req_size <= usable_max - usable_base)
        {
            /* We have found an entry that has room to satisfy the request, so allocate it from this entry */

            /* If end CVMX_BOOTMEM_FLAG_END_ALLOC set, then allocate from the end of this block
            ** rather than the beginning */
            if (flags & CVMX_BOOTMEM_FLAG_END_ALLOC)
            {
                desired_min_addr = usable_max - req_size;
                /* Align desired address down to required alignment */
                desired_min_addr &= alignment_mask;
            }

            /* Match at start of entry */
            if (desired_min_addr == ent_addr)
            {
                if (req_size < ent_size)
                {
                    /* big enough to create a new block from top portion of block */
                    new_ent_addr = ent_addr + req_size;
                    cvmx_bootmem_phy_set_next(new_ent_addr, cvmx_bootmem_phy_get_next(ent_addr));
                    cvmx_bootmem_phy_set_size(new_ent_addr, ent_size - req_size);

                    /* Adjust next pointer as following code uses this */
                    cvmx_bootmem_phy_set_next(ent_addr, new_ent_addr);
                }

                /* adjust prev ptr or head to remove this entry from list */
                if (prev_addr)
                {
                    cvmx_bootmem_phy_set_next(prev_addr, cvmx_bootmem_phy_get_next(ent_addr));
                }
                else
                {
                    /* head of list being returned, so update head ptr */
                    cvmx_bootmem_desc->head_addr = cvmx_bootmem_phy_get_next(ent_addr);
                }
                if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
                    cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
                return(desired_min_addr);
            }


            /* block returned doesn't start at beginning of entry, so we know
            ** that we will be splitting a block off the front of this one.  Create a new block
            ** from the beginning, add to list, and go to top of loop again.
            **
            ** create new block from high portion of block, so that top block
            ** starts at desired addr
            **/
            new_ent_addr = desired_min_addr;
            cvmx_bootmem_phy_set_next(new_ent_addr, cvmx_bootmem_phy_get_next(ent_addr));
            cvmx_bootmem_phy_set_size(new_ent_addr, cvmx_bootmem_phy_get_size(ent_addr) - (desired_min_addr - ent_addr));
            cvmx_bootmem_phy_set_size(ent_addr, desired_min_addr - ent_addr);
            cvmx_bootmem_phy_set_next(ent_addr, new_ent_addr);
            /* Loop again to handle actual alloc from new block */
        }

        prev_addr = ent_addr;
        ent_addr = cvmx_bootmem_phy_get_next(ent_addr);
    }
error_out:
    /* We didn't find anything, so return error */
    if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
        cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
    return(-1);
}



int __cvmx_bootmem_phy_free(uint64_t phy_addr, uint64_t size, uint32_t flags)
{
    uint64_t cur_addr;
    uint64_t prev_addr = 0;  /* zero is invalid */
    int retval = 0;

#ifdef DEBUG
    cvmx_dprintf("__cvmx_bootmem_phy_free addr: 0x%llx, size: 0x%llx\n", (unsigned long long)phy_addr, (unsigned long long)size);
#endif
    if (cvmx_bootmem_desc->major_version > 3)
    {
        cvmx_dprintf("ERROR: Incompatible bootmem descriptor version: %d.%d at addr: %p\n",
               (int)cvmx_bootmem_desc->major_version, (int)cvmx_bootmem_desc->minor_version, cvmx_bootmem_desc);
        return(0);
    }

    /* 0 is not a valid size for this allocator */
    if (!size)
        return(0);


    if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
        cvmx_spinlock_lock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
    cur_addr = cvmx_bootmem_desc->head_addr;
    if (cur_addr == 0 || phy_addr < cur_addr)
    {
        /* add at front of list - special case with changing head ptr */
        if (cur_addr && phy_addr + size > cur_addr)
            goto bootmem_free_done; /* error, overlapping section */
        else if (phy_addr + size == cur_addr)
        {
            /* Add to front of existing first block */
            cvmx_bootmem_phy_set_next(phy_addr, cvmx_bootmem_phy_get_next(cur_addr));
            cvmx_bootmem_phy_set_size(phy_addr, cvmx_bootmem_phy_get_size(cur_addr) + size);
            cvmx_bootmem_desc->head_addr = phy_addr;

        }
        else
        {
            /* New block before first block */
            cvmx_bootmem_phy_set_next(phy_addr, cur_addr);  /* OK if cur_addr is 0 */
            cvmx_bootmem_phy_set_size(phy_addr, size);
            cvmx_bootmem_desc->head_addr = phy_addr;
        }
        retval = 1;
        goto bootmem_free_done;
    }

    /* Find place in list to add block */
    while (cur_addr && phy_addr > cur_addr)
    {
        prev_addr = cur_addr;
        cur_addr = cvmx_bootmem_phy_get_next(cur_addr);
    }

    if (!cur_addr)
    {
        /* We have reached the end of the list, add on to end, checking
        ** to see if we need to combine with last block
        **/
        if (prev_addr +  cvmx_bootmem_phy_get_size(prev_addr) == phy_addr)
        {
            cvmx_bootmem_phy_set_size(prev_addr, cvmx_bootmem_phy_get_size(prev_addr) + size);
        }
        else
        {
            cvmx_bootmem_phy_set_next(prev_addr, phy_addr);
            cvmx_bootmem_phy_set_size(phy_addr, size);
            cvmx_bootmem_phy_set_next(phy_addr, 0);
        }
        retval = 1;
        goto bootmem_free_done;
    }
    else
    {
        /* insert between prev and cur nodes, checking for merge with either/both */

        if (prev_addr +  cvmx_bootmem_phy_get_size(prev_addr) == phy_addr)
        {
            /* Merge with previous */
            cvmx_bootmem_phy_set_size(prev_addr, cvmx_bootmem_phy_get_size(prev_addr) + size);
            if (phy_addr + size == cur_addr)
            {
                /* Also merge with current */
                cvmx_bootmem_phy_set_size(prev_addr, cvmx_bootmem_phy_get_size(cur_addr) + cvmx_bootmem_phy_get_size(prev_addr));
                cvmx_bootmem_phy_set_next(prev_addr, cvmx_bootmem_phy_get_next(cur_addr));
            }
            retval = 1;
            goto bootmem_free_done;
        }
        else if (phy_addr + size == cur_addr)
        {
            /* Merge with current */
            cvmx_bootmem_phy_set_size(phy_addr, cvmx_bootmem_phy_get_size(cur_addr) + size);
            cvmx_bootmem_phy_set_next(phy_addr, cvmx_bootmem_phy_get_next(cur_addr));
            cvmx_bootmem_phy_set_next(prev_addr, phy_addr);
            retval = 1;
            goto bootmem_free_done;
        }

        /* It is a standalone block, add in between prev and cur */
        cvmx_bootmem_phy_set_size(phy_addr, size);
        cvmx_bootmem_phy_set_next(phy_addr, cur_addr);
        cvmx_bootmem_phy_set_next(prev_addr, phy_addr);


    }
    retval = 1;

bootmem_free_done:
    if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
        cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
    return(retval);

}



void cvmx_bootmem_phy_list_print(void)
{
    uint64_t addr;

    addr = cvmx_bootmem_desc->head_addr;
    cvmx_dprintf("\n\n\nPrinting bootmem block list, descriptor: %p,  head is 0x%llx\n",
           cvmx_bootmem_desc, (unsigned long long)addr);
    cvmx_dprintf("Descriptor version: %d.%d\n", (int)cvmx_bootmem_desc->major_version, (int)cvmx_bootmem_desc->minor_version);
    if (cvmx_bootmem_desc->major_version > 3)
    {
        cvmx_dprintf("Warning: Bootmem descriptor version is newer than expected\n");
    }
    if (!addr)
    {
        cvmx_dprintf("mem list is empty!\n");
    }
    while (addr)
    {
        cvmx_dprintf("Block address: 0x%08qx, size: 0x%08qx, next: 0x%08qx\n",
               (unsigned long long)addr,
               (unsigned long long)cvmx_bootmem_phy_get_size(addr),
               (unsigned long long)cvmx_bootmem_phy_get_next(addr));
        addr = cvmx_bootmem_phy_get_next(addr);
    }
    cvmx_dprintf("\n\n");

}


uint64_t cvmx_bootmem_phy_available_mem(uint64_t min_block_size)
{
    uint64_t addr;

    uint64_t available_mem = 0;

    cvmx_spinlock_lock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
    addr = cvmx_bootmem_desc->head_addr;
    while (addr)
    {
        if (cvmx_bootmem_phy_get_size(addr) >= min_block_size)
            available_mem += cvmx_bootmem_phy_get_size(addr);
        addr = cvmx_bootmem_phy_get_next(addr);
    }
    cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
    return(available_mem);

}



cvmx_bootmem_named_block_desc_t * cvmx_bootmem_phy_named_block_find(char *name, uint32_t flags)
{
    unsigned int i;
    cvmx_bootmem_named_block_desc_t *named_block_array_ptr;


#ifdef DEBUG
    cvmx_dprintf("cvmx_bootmem_phy_named_block_find: %s\n", name);
#endif
    /* Lock the structure to make sure that it is not being changed while we are
    ** examining it.
    */
    if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
        cvmx_spinlock_lock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));

#if defined(__linux__) && !defined(CONFIG_OCTEON_U_BOOT)
#ifdef CVMX_ABI_N32
    /* Need to use mmapped named block pointer in 32 bit linux apps */
extern cvmx_bootmem_named_block_desc_t *linux32_named_block_array_ptr;
    named_block_array_ptr = linux32_named_block_array_ptr;
#else
    /* Use XKPHYS for 64 bit linux */
    named_block_array_ptr = (cvmx_bootmem_named_block_desc_t *)cvmx_phys_to_ptr(cvmx_bootmem_desc->named_block_array_addr);
#endif
#else
    /* Simple executive case. (and u-boot)
    ** This could be in the low 1 meg of memory that is not 1-1 mapped, so we need use XKPHYS/KSEG0 addressing for it */
    named_block_array_ptr = CASTPTR(cvmx_bootmem_named_block_desc_t, CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0,cvmx_bootmem_desc->named_block_array_addr));
#endif

#ifdef DEBUG
    cvmx_dprintf("cvmx_bootmem_phy_named_block_find: named_block_array_ptr: %p\n", named_block_array_ptr);
#endif
    if (cvmx_bootmem_desc->major_version == 3)
    {
        for (i = 0; i < cvmx_bootmem_desc->named_block_num_blocks; i++)
        {
            if ((name && named_block_array_ptr[i].size && !strncmp(name, named_block_array_ptr[i].name, cvmx_bootmem_desc->named_block_name_len - 1))
                || (!name && !named_block_array_ptr[i].size))
            {
                if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
                    cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));

                return(&(named_block_array_ptr[i]));
            }
        }
    }
    else
    {
        cvmx_dprintf("ERROR: Incompatible bootmem descriptor version: %d.%d at addr: %p\n",
               (int)cvmx_bootmem_desc->major_version, (int)cvmx_bootmem_desc->minor_version, cvmx_bootmem_desc);
    }
    if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
        cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));

    return(NULL);
}

int cvmx_bootmem_phy_named_block_free(char *name, uint32_t flags)
{
    cvmx_bootmem_named_block_desc_t *named_block_ptr;

    if (cvmx_bootmem_desc->major_version != 3)
    {
        cvmx_dprintf("ERROR: Incompatible bootmem descriptor version: %d.%d at addr: %p\n",
               (int)cvmx_bootmem_desc->major_version, (int)cvmx_bootmem_desc->minor_version, cvmx_bootmem_desc);
        return(0);
    }
#ifdef DEBUG
    cvmx_dprintf("cvmx_bootmem_phy_named_block_free: %s\n", name);
#endif

    /* Take lock here, as name lookup/block free/name free need to be atomic */
    cvmx_spinlock_lock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));

    named_block_ptr = cvmx_bootmem_phy_named_block_find(name, CVMX_BOOTMEM_FLAG_NO_LOCKING);
    if (named_block_ptr)
    {
#ifdef DEBUG
        cvmx_dprintf("cvmx_bootmem_phy_named_block_free: %s, base: 0x%llx, size: 0x%llx\n", name, (unsigned long long)named_block_ptr->base_addr, (unsigned long long)named_block_ptr->size);
#endif
        __cvmx_bootmem_phy_free(named_block_ptr->base_addr, named_block_ptr->size, CVMX_BOOTMEM_FLAG_NO_LOCKING);
        named_block_ptr->size = 0;
        /* Set size to zero to indicate block not used. */
    }

    cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));

    return(!!named_block_ptr);  /* 0 on failure, 1 on success */
}





int64_t cvmx_bootmem_phy_named_block_alloc(uint64_t size, uint64_t min_addr, uint64_t max_addr, uint64_t alignment, char *name, uint32_t flags)
{
    int64_t addr_allocated;
    cvmx_bootmem_named_block_desc_t *named_block_desc_ptr;

#ifdef DEBUG
    cvmx_dprintf("cvmx_bootmem_phy_named_block_alloc: size: 0x%llx, min: 0x%llx, max: 0x%llx, align: 0x%llx, name: %s\n",
                 (unsigned long long)size,
                 (unsigned long long)min_addr,
                 (unsigned long long)max_addr,
                 (unsigned long long)alignment,
                 name);
#endif
    if (cvmx_bootmem_desc->major_version != 3)
    {
        cvmx_dprintf("ERROR: Incompatible bootmem descriptor version: %d.%d at addr: %p\n",
               (int)cvmx_bootmem_desc->major_version, (int)cvmx_bootmem_desc->minor_version, cvmx_bootmem_desc);
        return(-1);
    }


    /* Take lock here, as name lookup/block alloc/name add need to be atomic */

    if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
        cvmx_spinlock_lock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));

    /* Get pointer to first available named block descriptor */
    named_block_desc_ptr = cvmx_bootmem_phy_named_block_find(NULL, flags | CVMX_BOOTMEM_FLAG_NO_LOCKING);

    /* Check to see if name already in use, return error if name
    ** not available or no more room for blocks.
    */
    if (cvmx_bootmem_phy_named_block_find(name, flags | CVMX_BOOTMEM_FLAG_NO_LOCKING) || !named_block_desc_ptr)
    {
        if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
            cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
        return(-1);
    }


    /* Round size up to mult of minimum alignment bytes
    ** We need the actual size allocated to allow for blocks to be coallesced
    ** when they are freed.  The alloc routine does the same rounding up
    ** on all allocations. */
    size = (size + (CVMX_BOOTMEM_ALIGNMENT_SIZE - 1)) & ~(CVMX_BOOTMEM_ALIGNMENT_SIZE - 1);

    addr_allocated = cvmx_bootmem_phy_alloc(size, min_addr, max_addr, alignment, flags | CVMX_BOOTMEM_FLAG_NO_LOCKING);
    if (addr_allocated >= 0)
    {
        named_block_desc_ptr->base_addr = addr_allocated;
        named_block_desc_ptr->size = size;
        strncpy(named_block_desc_ptr->name, name, cvmx_bootmem_desc->named_block_name_len);
        named_block_desc_ptr->name[cvmx_bootmem_desc->named_block_name_len - 1] = 0;
    }

    if (!(flags & CVMX_BOOTMEM_FLAG_NO_LOCKING))
        cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));

    return(addr_allocated);
}




void cvmx_bootmem_phy_named_block_print(void)
{
    unsigned int i;
    int printed = 0;

#if defined(__linux__) && !defined(CONFIG_OCTEON_U_BOOT)
#ifdef CVMX_ABI_N32
    /* Need to use mmapped named block pointer in 32 bit linux apps */
extern cvmx_bootmem_named_block_desc_t *linux32_named_block_array_ptr;
    cvmx_bootmem_named_block_desc_t *named_block_array_ptr = linux32_named_block_array_ptr;
#else
    /* Use XKPHYS for 64 bit linux */
    cvmx_bootmem_named_block_desc_t *named_block_array_ptr = (cvmx_bootmem_named_block_desc_t *)cvmx_phys_to_ptr(cvmx_bootmem_desc->named_block_array_addr);
#endif
#else
    /* Simple executive case. (and u-boot)
    ** This could be in the low 1 meg of memory that is not 1-1 mapped, so we need use XKPHYS/KSEG0 addressing for it */
    cvmx_bootmem_named_block_desc_t *named_block_array_ptr = CASTPTR(cvmx_bootmem_named_block_desc_t, CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0,cvmx_bootmem_desc->named_block_array_addr));
#endif
#ifdef DEBUG
    cvmx_dprintf("cvmx_bootmem_phy_named_block_print, desc addr: %p\n", cvmx_bootmem_desc);
#endif
    if (cvmx_bootmem_desc->major_version != 3)
    {
        cvmx_dprintf("ERROR: Incompatible bootmem descriptor version: %d.%d at addr: %p\n",
               (int)cvmx_bootmem_desc->major_version, (int)cvmx_bootmem_desc->minor_version, cvmx_bootmem_desc);
        return;
    }
    cvmx_dprintf("List of currently allocated named bootmem blocks:\n");
    for (i = 0; i < cvmx_bootmem_desc->named_block_num_blocks; i++)
    {
        if (named_block_array_ptr[i].size)
        {
            printed++;
            cvmx_dprintf("Name: %s, address: 0x%08qx, size: 0x%08qx, index: %d\n",
                   named_block_array_ptr[i].name,
                   (unsigned long long)named_block_array_ptr[i].base_addr,
                   (unsigned long long)named_block_array_ptr[i].size,
                   i);

        }
    }
    if (!printed)
    {
        cvmx_dprintf("No named bootmem blocks exist.\n");
    }

}


/* Real physical addresses of memory regions */
#define OCTEON_DDR0_BASE    (0x0ULL)
#define OCTEON_DDR0_SIZE    (0x010000000ULL)
#define OCTEON_DDR1_BASE    (0x410000000ULL)
#define OCTEON_DDR1_SIZE    (0x010000000ULL)
#define OCTEON_DDR2_BASE    (0x020000000ULL)
#define OCTEON_DDR2_SIZE    (0x3e0000000ULL)
#define OCTEON_MAX_PHY_MEM_SIZE (16*1024*1024*1024ULL)
int64_t cvmx_bootmem_phy_mem_list_init(uint64_t mem_size, uint32_t low_reserved_bytes, cvmx_bootmem_desc_t *desc_buffer)
{
    uint64_t cur_block_addr;
    int64_t addr;

#ifdef DEBUG
    cvmx_dprintf("cvmx_bootmem_phy_mem_list_init (arg desc ptr: %p, cvmx_bootmem_desc: %p)\n", desc_buffer, cvmx_bootmem_desc);
#endif

    /* Descriptor buffer needs to be in 32 bit addressable space to be compatible with
    ** 32 bit applications */
    if (!desc_buffer)
    {
        cvmx_dprintf("ERROR: no memory for cvmx_bootmem descriptor provided\n");
        return 0;
    }

    if (mem_size > OCTEON_MAX_PHY_MEM_SIZE)
    {
        mem_size = OCTEON_MAX_PHY_MEM_SIZE;
        cvmx_dprintf("ERROR: requested memory size too large, truncating to maximum size\n");
    }

    if (cvmx_bootmem_desc)
        return 1;

    /* Initialize cvmx pointer to descriptor */
    cvmx_bootmem_init(desc_buffer);

    /* Set up global pointer to start of list, exclude low 64k for exception vectors, space for global descriptor */
    memset(cvmx_bootmem_desc, 0x0, sizeof(cvmx_bootmem_desc_t));
    /* Set version of bootmem descriptor */
    cvmx_bootmem_desc->major_version = CVMX_BOOTMEM_DESC_MAJ_VER;
    cvmx_bootmem_desc->minor_version = CVMX_BOOTMEM_DESC_MIN_VER;

    cur_block_addr = cvmx_bootmem_desc->head_addr = (OCTEON_DDR0_BASE + low_reserved_bytes);

    cvmx_bootmem_desc->head_addr = 0;

    if (mem_size <= OCTEON_DDR0_SIZE)
    {
        __cvmx_bootmem_phy_free(cur_block_addr, mem_size - low_reserved_bytes, 0);
        goto frees_done;
    }

    __cvmx_bootmem_phy_free(cur_block_addr, OCTEON_DDR0_SIZE - low_reserved_bytes, 0);

    mem_size -= OCTEON_DDR0_SIZE;

    /* Add DDR2 block next if present */
    if (mem_size > OCTEON_DDR1_SIZE)
    {
        __cvmx_bootmem_phy_free(OCTEON_DDR1_BASE, OCTEON_DDR1_SIZE, 0);
        __cvmx_bootmem_phy_free(OCTEON_DDR2_BASE, mem_size - OCTEON_DDR1_SIZE, 0);
    }
    else
    {
        __cvmx_bootmem_phy_free(OCTEON_DDR1_BASE, mem_size, 0);

    }
frees_done:

    /* Initialize the named block structure */
    cvmx_bootmem_desc->named_block_name_len = CVMX_BOOTMEM_NAME_LEN;
    cvmx_bootmem_desc->named_block_num_blocks = CVMX_BOOTMEM_NUM_NAMED_BLOCKS;
    cvmx_bootmem_desc->named_block_array_addr = 0;

    /* Allocate this near the top of the low 256 MBytes of memory */
    addr = cvmx_bootmem_phy_alloc(CVMX_BOOTMEM_NUM_NAMED_BLOCKS * sizeof(cvmx_bootmem_named_block_desc_t),0, 0x10000000, 0 ,CVMX_BOOTMEM_FLAG_END_ALLOC);
    if (addr >= 0)
        cvmx_bootmem_desc->named_block_array_addr = addr;

#ifdef DEBUG
    cvmx_dprintf("cvmx_bootmem_phy_mem_list_init: named_block_array_addr: 0x%llx)\n", (unsigned long long)cvmx_bootmem_desc->named_block_array_addr);
#endif
    if (!cvmx_bootmem_desc->named_block_array_addr)
    {
        cvmx_dprintf("FATAL ERROR: unable to allocate memory for bootmem descriptor!\n");
        return(0);
    }
    memset((void *)(unsigned long)cvmx_bootmem_desc->named_block_array_addr, 0x0, CVMX_BOOTMEM_NUM_NAMED_BLOCKS * sizeof(cvmx_bootmem_named_block_desc_t));

    return(1);
}


void cvmx_bootmem_lock(void)
{
    cvmx_spinlock_lock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
}

void cvmx_bootmem_unlock(void)
{
    cvmx_spinlock_unlock((cvmx_spinlock_t *)&(cvmx_bootmem_desc->lock));
}

void *__cvmx_bootmem_internal_get_desc_ptr(void)
{
    return(cvmx_bootmem_desc);
}
