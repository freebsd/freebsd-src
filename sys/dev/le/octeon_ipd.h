/*------------------------------------------------------------------
 * octeon_ipd.h      Input Packet Unit
 *
 *------------------------------------------------------------------
 */


#ifndef ___OCTEON_IPD__H___
#define ___OCTEON_IPD__H___



typedef enum {
   OCTEON_IPD_OPC_MODE_STT = 0LL,   /* All blocks DRAM, not cached in L2 */
   OCTEON_IPD_OPC_MODE_STF = 1LL,   /* All blocks into  L2 */
   OCTEON_IPD_OPC_MODE_STF1_STT = 2LL,   /* 1st block L2, rest DRAM */
   OCTEON_IPD_OPC_MODE_STF2_STT = 3LL    /* 1st, 2nd blocks L2, rest DRAM */
} octeon_ipd_mode_t;




/*
 * IPD_CTL_STATUS = IPS'd Control Status Register
 *  The number of words in a MBUFF used for packet data store.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved	: 58;      /* Reserved */
        uint64_t pkt_lend       : 1;       /* Pkt Lil-Endian Writes to L2C */
        uint64_t wqe_lend       : 1;       /* WQE Lik-Endian Writes to L2C */
        uint64_t pbp_en         : 1;       /* Enable Back-Pressure */
        octeon_ipd_mode_t opc_mode : 2;       /* Pkt data in Mem/L2-cache ? */
        uint64_t ipd_en         : 1;       /* Enable IPD */
    } bits;
} octeon_ipd_ctl_status_t;


/*
 * IPD_1ST_NEXT_PTR_BACK = IPD First Next Pointer Back Values
 *
 * Contains the Back Field for use in creating the Next Pointer Header
 *    for the First MBUF
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved	: 60;      /* Must be zero */
        uint64_t back		: 4;       /* Used to find head of buffer from the nxt-hdr-ptr. */
    } bits;
} octeon_ipd_first_next_ptr_back_t;


/*
 * IPD_INTERRUPT_ENB = IPD Interrupt Enable Register
 *
 * Used to enable the various interrupting conditions of IPD
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved       : 59;      /* Must be zero */
        uint64_t bp_sub		: 1;       /* BP subtract is illegal val */
        uint64_t prc_par3       : 1;       /* PBM Bits [127:96] Parity Err */
        uint64_t prc_par2       : 1;       /* PBM Bits [ 95:64] Parity Err */
        uint64_t prc_par1       : 1;       /* PBM Bits [ 63:32] Parity Err */
        uint64_t prc_par0       : 1;       /* PBM Bits [ 31:0 ] Parity Err */
    } bits;
} octeon_ipd_int_enb_t;


/*
 * IPD_INTERRUPT_SUM = IPD Interrupt Summary Register
 *
 * Set when an interrupt condition occurs, write '1' to clear.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved	: 59;      /* Must be zero */
        uint64_t bp_sub         : 1;       /* BP subtract is illegal val */
        uint64_t prc_par3       : 1;       /* PBM Bits [127:96] Parity Err */
        uint64_t prc_par2       : 1;       /* PBM Bits [ 95:64] Parity Err */
        uint64_t prc_par1       : 1;       /* PBM Bits [ 63:32] Parity Err */
        uint64_t prc_par0       : 1;       /* PBM Bits [ 31:0 ] Parity Err */
    } bits;
} octeon_ipd_int_sum_t;


/**
 * IPD_1ST_MBUFF_SKIP = IPD First MBUFF Word Skip Size
 *
 * The number of words that the IPD will skip when writing the first MBUFF.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved	: 58;      /* Must be zero */
        uint64_t skip_sz        : 6;       /* 64bit words from the top of */
        				   /*  1st MBUFF that the IPD will */
					   /*  store the next-pointer. */
        				   /*  [0..32]  &&             */
                                           /*    (skip_sz + 16) <= IPD_PACKET_MBUFF_SIZE[MB_SIZE]. */
    } bits;
} octeon_ipd_mbuff_first_skip_t;


/*
 * IPD_PACKET_MBUFF_SIZE = IPD's PACKET MUBUF Size In Words
 *
 * The number of words in a MBUFF used for packet data store.
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved	: 52;      /* Must be zero */
        uint64_t mb_size        : 12;      /* 64bit words in a MBUF. */
        				   /* Must be [32..2048] */
					   /* Is also the size of the FPA's */
					   /*   Queue-0 Free-Page */
    } bits;
} octeon_ipd_mbuff_size_t;


/*
 * IPD_WQE_FPA_QUEUE = IPD Work-Queue-Entry FPA Page Size
 *
 * Which FPA Queue (0-7) to fetch page-pointers from for WQE's
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved	: 61;    /* Must be zero */
        uint64_t wqe_pool       : 3;     /* FPA Pool to fetch WQE Page-ptrs */
    } bits;
} octeon_ipd_wqe_fpa_pool_t;




/* End of Control and Status Register (CSR) definitions */

typedef octeon_ipd_mbuff_first_skip_t octeon_ipd_mbuff_not_first_skip_t;
typedef octeon_ipd_first_next_ptr_back_t octeon_ipd_second_next_ptr_back_t;


/*
 * Externs
 */
extern void octeon_ipd_enable(void);
extern void octeon_ipd_disable(void);
extern void octeon_ipd_config(u_int mbuff_size,
                              u_int first_mbuff_skip,
                              u_int not_first_mbuff_skip,
                              u_int first_back,
                              u_int second_back,
                              u_int wqe_fpa_pool,
                              octeon_ipd_mode_t cache_mode,
                              u_int back_pres_enable_flag);



#endif   /*  ___OCTEON_IPD__H___ */
