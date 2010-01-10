/*------------------------------------------------------------------
 * octeon_fpa.h      Free Pool Allocator
 *
 *------------------------------------------------------------------
 */


#ifndef ___OCTEON_FPA__H___
#define ___OCTEON_FPA__H___


#define OCTEON_FPA_FPA_OUTPUT_BUFFER_POOL           2    /* Same in octeon_rgmx.h */


/*
 * OCTEON_FPA_FPF_MARKS = FPA's Queue Free Page FIFO Read Write Marks
 *
 * The high and low watermark register that determines when we write and
 * read free pages from L2C for Queue.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved                : 42;      /* Must be zero */
        uint64_t fpf_wr                  : 11;      /* Write Hi Water mark */
        uint64_t fpf_rd                  : 11;      /* Read Lo Water mark */
    } bits;
} octeon_fpa_fpf_marks_t;


/*
 * OCTEON_FPA_CTL_STATUS = FPA's Control/Status Register
 *
 * The FPA's interrupt enable register.
 * - Use with the CVMX_FPA_CTL_STATUS CSR.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved                : 49;      /* Must be zero */
        uint64_t enb                     : 1;       /* Enable */
        uint64_t mem1_err                : 7;       /* ECC flip 1 */
        uint64_t mem0_err                : 7;       /* ECC flip 0 */
    } bits;
} octeon_fpa_ctl_status_t;


/*
 * OCTEON_FPA_FPF_SIZE = FPA's Queue N Free Page FIFO Size
 *
 * The number of page pointers that will be kept local to the FPA for
 *  this Queue. FPA Queues are assigned in order from Queue 0 to
 *  Queue 7, though only Queue 0 through Queue x can be used.
 * The sum of the 8 (0-7)OCTEON_FPA_FPF#_SIZE registers must be limited to 2048.
 * - Use with the CVMX_FPA_FPF0_SIZE CSR.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved                : 52;      /* Must be zero */
      /*
       * The number of entries assigned in the FPA FIFO (used to hold
       * page-pointers) for this Queue.
       * The value of this register must divisable by 2, and the FPA will
       * ignore bit [0] of this register.
       * The total of the FPF_SIZ field of the 8 (0-7)OCTEON_FPA_FPF#_MARKS
       * registers must not exceed 2048.
       * After writing this field the FPA will need 10 core clock cycles
       * to be ready for operation. The assignment of location in
       * the FPA FIFO must start with Queue 0, then 1, 2, etc.
       * The number of useable entries will be FPF_SIZ-2.
       */
        uint64_t fpf_siz                 : 12;
    } bits;
} octeon_fpa_fpf_size_t;

/*
 *OCTEON_FPA_INT_ENB = FPA's Interrupt Enable
 *
 * The FPA's interrupt enable register.
 * - Use with the CVMX_FPA_INT_ENB CSR.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved                : 60;  /* Must be zero */
        uint64_t fed1_dbe                : 1;   /* Int iff bit3 Int-Sum set */
        uint64_t fed1_sbe                : 1;   /* Int iff bit2 Int-Sum set */
        uint64_t fed0_dbe                : 1;   /* Int iff bit1 Int-Sum set */
        uint64_t fed0_sbe                : 1;   /* Int iff bit0 Int-Sum set */
    } bits;
} octeon_fpa_int_enb_t;

/**
 *OCTEON_FPA_INT_SUM = FPA's Interrupt Summary Register
 *
 * Contains the diffrent interrupt summary bits of the FPA.
 * - Use with the CVMX_FPA_INT_SUM CSR.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved                : 60;      /**< Must be zero */
        uint64_t fed1_dbe                : 1;
        uint64_t fed1_sbe                : 1;
        uint64_t fed0_dbe                : 1;
        uint64_t fed0_sbe                : 1;
    } bits;
} octeon_fpa_int_sum_t;


/*
 *OCTEON_FPA_QUEUE_PAGES_AVAILABLE = FPA's Queue 0-7 Free Page Available Register
 *
 * The number of page pointers that are available in the FPA and local DRAM.
 * - Use with the CVMX_FPA_QUEX_AVAILABLE(0..7) CSR.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved                : 38;      /* Must be zero */
        uint64_t queue_size              : 26;      /* free pages available */
    } bits;
} octeon_fpa_queue_available_t;


/*
 *OCTEON_FPA_QUEUE_PAGE_INDEX
 *
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved                : 39;      /* Must be zero */
        uint64_t page_index              : 25;      /* page_index */
    } bits;
} octeon_fpa_queue_page_index_t;


#define OCTEON_DID_FPA			5ULL

#define	OCTEON_FPA_POOL_ALIGNMENT	(OCTEON_CACHE_LINE_SIZE)


/*
 * Externs
 */
extern void octeon_dump_fpa(void);
extern void octeon_dump_fpa_pool(u_int pool);
extern u_int octeon_fpa_pool_size(u_int pool);
extern void octeon_enable_fpa(void);
extern void octeon_fpa_fill_pool_mem(u_int pool,
                                     u_int block_size_words,
                                     u_int block_num);

/*
 * octeon_fpa_free
 *
 * Free a mem-block to FPA pool.
 *
 * Takes away this 'buffer' from SW and passes it to FPA for management.
 *
 *  pool is FPA pool num, ptr is block ptr, num_cache_lines is number of
 *  cache lines to invalidate (not written back).
 */
static inline void octeon_fpa_free (void *ptr, u_int pool,
                                    u_int num_cache_lines)
{
    octeon_addr_t free_ptr;

    free_ptr.word64 = (uint64_t)OCTEON_PTR2PHYS(ptr);

    free_ptr.sfilldidspace.didspace = OCTEON_ADDR_DIDSPACE(
        OCTEON_ADDR_FULL_DID(OCTEON_DID_FPA, pool));

    /*
     * Do not 'sync'
     *     asm volatile ("sync\n");
     */
    oct_write64(free_ptr.word64, num_cache_lines);
}



/*
 * octeon_fpa_alloc
 *
 * Allocate a new block from the FPA
 *
 * Buffer passes away from FPA management to SW control
 */
static inline void *octeon_fpa_alloc (u_int pool)
{
    uint64_t address;

    address = oct_read64(OCTEON_ADDR_DID(OCTEON_ADDR_FULL_DID(OCTEON_DID_FPA,
                                                              pool)));
    if (address) {

/*
 * 32 bit FPA pointers only
 */
        /*
         * We only use 32 bit pointers at this time
         */
/*XXX mips64 issue */
        return ((void *) MIPS_PHYS_TO_KSEG0(address & 0xffffffff));
    }
    return (NULL);
}

static inline uint64_t octeon_fpa_alloc_phys (u_int pool)
{

    return (oct_read64(OCTEON_ADDR_DID(OCTEON_ADDR_FULL_DID(OCTEON_DID_FPA,
                                                            pool))));
}

#endif /* ___OCTEON_FPA__H___ */
