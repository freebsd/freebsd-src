// SPDX-License-Identifier: GPL-2.0
//#define DISABLE_BRANCH_PROFILING
//#define pr_fmt(fmt) "kasan: " fmt

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/cprng.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/filedesc.h>
#include <sys/proc.h>

#include <sys/kasan.h>
#include <uvm/uvm.h>
#include <amd64/pmap.h>
#include <amd64/vmparam.h>


struct seg_details {
        unsigned long vaddr;
        unsigned long size;
};

static struct seg_details kmap[4];

void DumpSegments(void);
extern struct bootspace bootspace;

void
DumpSegments(void)
{
	size_t i;

        /*
         * Copy the addresses and sizes of the various regions in the 
         * bootspace structure into our kernel structure
         */
	for (i = 0; i < BTSPACE_NSEGS; i++) {
		if (bootspace.segs[i].type == BTSEG_NONE) {
			continue;
		}
	        kmap[bootspace.segs[i].type].vaddr = bootspace.segs[i].va;
                kmap[bootspace.segs[i].type].size = bootspace.segs[i].sz;
	}
}

unsigned long text_start;
unsigned long text_end;

/* Need to get a different offset */
#define CPU_ENTRY_AREA_BASE 0xff0000 //temp value
#define CPU_ENTRY_AREA_END 0xff0000 //temp value
#define MAX_BITS 46
#define MAX_MEM (1UL << MAX_BITS)


/*
 *  
 */
void
kasan_early_init(void)
{
    return ;
}

/* 
 * kasan_init is supposed to be called after the pmap(9) and uvm(9) bootstraps
 * are done - since we have opted to uses high level allocator function -
 * uvm_km_alloc to get the shadow region mapped. This is done with the idea
 * that the area we are allocating is a unused hole.
 */
void
kasan_init(void)
{
        struct pmap *kernmap;
        struct vm_map shadow_map;
        // void *shadow_begin, *shadow_end;

        /* clearing page table entries for the shadow region */
        kernmap = pmap_kernel();
        pmap_remove(kernmap, KASAN_SHADOW_START, KASAN_SHADOW_END);
        pmap_update(kernmap);


	/*  Text Section and main shadow offsets */
	DumpSegments();
	text_start = kmap[1].vaddr;
	text_end = kmap[1].vaddr + kmap[1].size;

        /* Initialize the kernel map for the unallocated region

	Alternate way to set up the map. 
        uvm_map_setup(&shadow_map, (vaddr_t)KASAN_SHADOW_START,
            (vaddr_t)KASAN_SHADOW_END, VM_MAP_PAGEABLE);
        shadow_map.pmap = pmap_kernel(); //Not sure about this

         Might need to add a check to see if everything worked properly

	error = uvm_map_prepare(&shadow_map,
	    kmembase, kmemsize,
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_NONE,
	    		UVM_ADV_RANDOM, UVM_FLAG_FIXED), NULL);
	if (!error) {
		kernel_kmem_mapent_store.flags =
		    UVM_MAP_KERNEL | UVM_MAP_STATIC | UVM_MAP_NOMERGE;
	}

	*/

	uvm_map(&shadow_map, (vaddr_t *)KASAN_SHADOW_START, 
                (size_t)(KASAN_SHADOW_END - KASAN_SHADOW_START), NULL, 
                UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL,
                    UVM_INH_NONE, UVM_ADV_RANDOM, UVM_FLAG_FIXED));

        /*
         * Map is ready - now we can allocate the shadow buffer
	 * Allocate zero pages for the shadow region 
         * We wll do the sections as in Linux 
         */

        /* User space */
        vm_map_setmin(&shadow_map, KASAN_SHADOW_START);
        uvm_km_alloc(&shadow_map, ((vsize_t)(kasan_mem_to_shadow(L4_BASE))
                    - KASAN_SHADOW_START), 0, UVM_KMF_ZERO);

        /* Second region is L4_BASE+MAX_MEM to start of the cpu entry region */
        vm_map_setmin(&shadow_map, (unsigned long)kasan_mem_to_shadow(L4_BASE
                    + MAX_MEM));
        uvm_km_alloc(&shadow_map, (vsize_t)kasan_mem_to_shadow((void *)(L4_BASE
                        + MAX_MEM)) - (vsize_t)kasan_mem_to_shadow((void *)
                        CPU_ENTRY_AREA_BASE), 0, UVM_KMF_ZERO);

        /* The cpu entry region - nid as 0*/
        uvm_km_alloc(&shadow_map, (vsize_t)kasan_mem_to_shadow((void *)
                    CPU_ENTRY_AREA_END) - (vsize_t)kasan_mem_to_shadow((void *)
                        CPU_ENTRY_AREA_BASE), 0, UVM_KMF_ZERO);

        /* Cpu end region to start of kernel map (KERNBASE)*/
        uvm_km_alloc(&shadow_map, (vsize_t)kasan_mem_to_shadow((void *)
                    CPU_ENTRY_AREA_END) - (vsize_t)kasan_mem_to_shadow((void *)
                        KERNBASE), 0, UVM_KMF_ZERO);

        /* The text section - nid as something */
        vm_map_setmin(&shadow_map, text_start);
        uvm_km_alloc(&shadow_map, (vsize_t)kasan_mem_to_shadow((void *)
                    text_end) - (vsize_t)kasan_mem_to_shadow((void *)
                        text_start), 0, UVM_KMF_ZERO);

        /* Avoiding the Module map for now */
}
