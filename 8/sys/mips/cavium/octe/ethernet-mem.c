/*************************************************************************
Copyright (c) 2003-2007  Cavium Networks (support@cavium.com). All rights
reserved.


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Cavium Networks nor the names of
      its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

This Software, including technical data, may be subject to U.S. export  control laws, including the U.S. Export Administration Act and its  associated regulations, and may be subject to export or import  regulations in other countries.

TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.

*************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/ethernet.h>
#include <net/if.h>

#include "wrapper-cvmx-includes.h"
#include "ethernet-headers.h"

/**
 * Fill the supplied hardware pool with mbufs
 *
 * @param pool     Pool to allocate an mbuf for
 * @param size     Size of the buffer needed for the pool
 * @param elements Number of buffers to allocate
 */
static int cvm_oct_fill_hw_mbuf(int pool, int size, int elements)
{
	int freed = elements;
	while (freed) {
		KASSERT(size <= MCLBYTES - 128, ("mbuf clusters are too small"));

		struct mbuf *m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (__predict_false(m == NULL)) {
			printf("Failed to allocate mbuf for hardware pool %d\n", pool);
			break;
		}

		m->m_data += 128 - (((uintptr_t)m->m_data) & 0x7f);
		*(struct mbuf **)(m->m_data - sizeof(void *)) = m;
		cvmx_fpa_free(m->m_data, pool, DONT_WRITEBACK(size/128));
		freed--;
	}
	return (elements - freed);
}


/**
 * Free the supplied hardware pool of mbufs
 *
 * @param pool     Pool to allocate an mbuf for
 * @param size     Size of the buffer needed for the pool
 * @param elements Number of buffers to allocate
 */
static void cvm_oct_free_hw_mbuf(int pool, int size, int elements)
{
	char *memory;

	do {
		memory = cvmx_fpa_alloc(pool);
		if (memory) {
			struct mbuf *m = *(struct mbuf **)(memory - sizeof(void *));
			elements--;
			m_freem(m);
		}
	} while (memory);

	if (elements < 0)
		printf("Warning: Freeing of pool %u had too many mbufs (%d)\n", pool, elements);
	else if (elements > 0)
		printf("Warning: Freeing of pool %u is missing %d mbufs\n", pool, elements);
}


/**
 * This function fills a hardware pool with memory. Depending
 * on the config defines, this memory might come from the
 * kernel or global 32bit memory allocated with
 * cvmx_bootmem_alloc.
 *
 * @param pool     Pool to populate
 * @param size     Size of each buffer in the pool
 * @param elements Number of buffers to allocate
 */
static int cvm_oct_fill_hw_memory(int pool, int size, int elements)
{
	char *memory;
	int freed = elements;

	if (USE_32BIT_SHARED) {
#if 0
		extern uint64_t octeon_reserve32_memory;

		memory = cvmx_bootmem_alloc_range(elements*size, 128, octeon_reserve32_memory,
						octeon_reserve32_memory + (CONFIG_CAVIUM_RESERVE32<<20) - 1);
		if (memory == NULL)
			panic("Unable to allocate %u bytes for FPA pool %d\n", elements*size, pool);

		printf("Memory range %p - %p reserved for hardware\n", memory, memory + elements*size - 1);

		while (freed) {
			cvmx_fpa_free(memory, pool, 0);
			memory += size;
			freed--;
		}
#else
		panic("%s: may need to implement using shared memory.", __func__);
#endif
	} else {
		while (freed) {
			/* We need to force alignment to 128 bytes here */
#if 0
			memory = kmalloc(size + 127, GFP_ATOMIC);
#else
			panic("%s: not yet implemented.", __func__);
#endif
			if (__predict_false(memory == NULL)) {
				printf("Unable to allocate %u bytes for FPA pool %d\n", elements*size, pool);
				break;
			}
			memory = (char *)(((unsigned long)memory+127) & -128);
			cvmx_fpa_free(memory, pool, 0);
			freed--;
		}
	}
	return (elements - freed);
}


/**
 * Free memory previously allocated with cvm_oct_fill_hw_memory
 *
 * @param pool     FPA pool to free
 * @param size     Size of each buffer in the pool
 * @param elements Number of buffers that should be in the pool
 */
static void cvm_oct_free_hw_memory(int pool, int size, int elements)
{
	if (USE_32BIT_SHARED) {
		printf("Warning: 32 shared memory is not freeable\n");
	} else {
		char *memory;
		do {
			memory = cvmx_fpa_alloc(pool);
			if (memory) {
				elements--;
#if 0
				kfree(phys_to_virt(cvmx_ptr_to_phys(memory)));
#else
				panic("%s: not yet implemented.", __func__);
#endif
			}
		} while (memory);

		if (elements < 0)
			printf("Freeing of pool %u had too many buffers (%d)\n", pool, elements);
		else if (elements > 0)
			printf("Warning: Freeing of pool %u is missing %d buffers\n", pool, elements);
	}
}


int cvm_oct_mem_fill_fpa(int pool, int size, int elements)
{
	int freed;
	if (USE_MBUFS_IN_HW)
		freed = cvm_oct_fill_hw_mbuf(pool, size, elements);
	else
		freed = cvm_oct_fill_hw_memory(pool, size, elements);
	return (freed);
}

void cvm_oct_mem_empty_fpa(int pool, int size, int elements)
{
	if (USE_MBUFS_IN_HW)
		cvm_oct_free_hw_mbuf(pool, size, elements);
	else
		cvm_oct_free_hw_memory(pool, size, elements);
}

