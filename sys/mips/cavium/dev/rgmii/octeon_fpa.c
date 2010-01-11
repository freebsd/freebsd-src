/*------------------------------------------------------------------
 * octeon_fpa.c        Free Pool Allocator
 *
 *------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/pmap.h>


#include <mips/cavium/octeon_pcmap_regs.h>
#include "octeon_fpa.h"


//#define FPA_DEBUG 1

/*
 * octeon_dump_fpa
 *
 */
void octeon_dump_fpa (void)
{
    int i;
    octeon_fpa_ctl_status_t status;
    octeon_fpa_queue_available_t q_avail;

    status.word64 = oct_read64(OCTEON_FPA_CTL_STATUS);
    if (!status.bits.enb) {
        printf("\n  FPA Disabled");
        /*
         * No dumping if disabled
         */
        return;
    }
    printf(" FPA  Ctrl-Status-reg 0x%llX := 0x%llX  EN %X  M1_E %X  M0_E %X\n",
           OCTEON_FPA_CTL_STATUS, (unsigned long long)status.word64,
           status.bits.enb, status.bits.mem1_err, status.bits.mem0_err);
    for (i = 0; i < OCTEON_FPA_QUEUES; i++) {
        printf("   Pool: %d\n", i);

        q_avail.word64 = oct_read64((OCTEON_FPA_QUEUE_AVAILABLE + (i)*8ull));
        printf("   Avail-reg 0x%llX :=   Size: 0x%X\n",
               (OCTEON_FPA_QUEUE_AVAILABLE + (i)*8ull), q_avail.bits.queue_size);
    }
}

void octeon_dump_fpa_pool (u_int pool)
{
    octeon_fpa_ctl_status_t status;
    octeon_fpa_queue_available_t q_avail;

    status.word64 = oct_read64(OCTEON_FPA_CTL_STATUS);
    if (!status.bits.enb) {
        printf("\n  FPA Disabled");
        /*
         * No dumping if disabled
         */
        return;
    }
    printf(" FPA  Ctrl-Status-reg 0x%llX := 0x%llX  EN %X  M1_E %X  M0_E %X\n",
           OCTEON_FPA_CTL_STATUS, (unsigned long long)status.word64,
           status.bits.enb, status.bits.mem1_err, status.bits.mem0_err);
    q_avail.word64 = oct_read64((OCTEON_FPA_QUEUE_AVAILABLE + (pool)*8ull));
    printf("   FPA Pool: %u   Avail-reg 0x%llX :=   Size: 0x%X\n", pool,
           (OCTEON_FPA_QUEUE_AVAILABLE + (pool)*8ull), q_avail.bits.queue_size);
}


u_int octeon_fpa_pool_size (u_int pool)
{
    octeon_fpa_queue_available_t q_avail;
    u_int size = 0;

    if (pool < 7) {
            q_avail.word64 = oct_read64((OCTEON_FPA_QUEUE_AVAILABLE + (pool)*8ull));
            size = q_avail.bits.queue_size;
    }
    return (size);
}


/*
 * octeon_enable_fpa
 *
 * configure fpa with defaults and then mark it enabled.
 */
void octeon_enable_fpa (void)
{
    int i;
    octeon_fpa_ctl_status_t status;
    octeon_fpa_fpf_marks_t marks;

    for (i = 0; i < OCTEON_FPA_QUEUES; i++) {
        marks.word64 = oct_read64((OCTEON_FPA_FPF_MARKS + (i)*8ull));

        marks.bits.fpf_wr = 0xe0;
        oct_write64((OCTEON_FPA_FPF_MARKS + (i)*8ull), marks.word64);
    }

    /* Enforce a 10 cycle delay between config and enable */
    octeon_wait(10);

    status.word64 = 0;
    status.bits.enb = 1;
    oct_write64(OCTEON_FPA_CTL_STATUS, status.word64);
}


//#define FPA_DEBUG_TERSE 1

/*
 * octeon_fpa_fill_pool_mem
 *
 * Fill the specified FPA pool with elem_num number of
 * elements of size elem_size_words * 8
 */
void octeon_fpa_fill_pool_mem (u_int pool, u_int elem_size_words, u_int elem_num)
{
    void *memory;
    u_int bytes, elem_size_bytes;
    u_int block_size;

#ifdef FPA_DEBUG
    u_int elems = elem_num;
    printf(" FPA fill: Pool %u  elem_size_words %u   Num: %u\n", pool, elem_size_words, elem_num);
#endif
    elem_size_bytes = elem_size_words * sizeof(uint64_t);
    block_size = OCTEON_ALIGN(elem_size_bytes);

//    block_size = ((elem_size_bytes / OCTEON_FPA_POOL_ALIGNMENT) + 1) * OCTEON_FPA_POOL_ALIGNMENT;

    bytes = (elem_num * block_size);

#ifdef FPA_DEBUG
    printf(" elem_size_bytes = words * 8 = %u;  block_size %u\n", elem_size_bytes, block_size);
#endif


#ifdef FPA_DEBUG
    int block = 0;

    printf(" %% Filling Pool %u  with %u blocks of %u bytes  %u words\n",
           pool, elem_num, elem_size_bytes, elem_size_words);
#endif

//    memory = malloc(bytes, M_DEVBUF, M_NOWAIT | M_ZERO);
    memory = contigmalloc(bytes, M_DEVBUF, M_NOWAIT | M_ZERO,
                          0, 0x20000000,
                          OCTEON_FPA_POOL_ALIGNMENT, 0);

    if (memory == NULL) {
        printf(" %% FPA pool %u could not be filled with %u bytes\n",
               pool, bytes);
        return;
    }

    /*
     * Forward Align allocated mem to needed alignment. Don't worry about growth, we
     * already preallocated extra
     */
#ifdef FPA_DEBUG
    printf(" %% Huge MemBlock  0x%X   Bytes %u\n", memory, bytes);
#endif

    memory = (void *) OCTEON_ALIGN(memory);

#ifdef FPA_DEBUG_TERSE
    printf("FPA fill: %u  Count: %u  SizeBytes: %u  SizeBytesAligned: %u  1st: 0x%X = %p\n",
           pool, elem_num, elem_size_bytes, block_size, memory, (void *)OCTEON_PTR2PHYS(memory));
#endif

//    memory = (void *) ((((u_int) memory / OCTEON_FPA_POOL_ALIGNMENT) + 1) * OCTEON_FPA_POOL_ALIGNMENT);

    while (elem_num--) {
#ifdef FPA_DEBUG
        if (((elems - elem_num) < 4) || (elem_num < 4))
        printf(" %% Block %d:  0x%X  Phys 0x%X   Bytes %u\n", block, memory, OCTEON_PTR2PHYS(memory), elem_size_bytes);
        block++;
#endif
        octeon_fpa_free(memory, pool, 0);
        memory = (void *) (((u_long) memory) + block_size);
    }
}

