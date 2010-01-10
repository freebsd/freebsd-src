/*------------------------------------------------------------------
 * octeon_rgmx.h      RGMII Ethernet Interfaces
 *
 *------------------------------------------------------------------
 */


#ifndef ___OCTEON_RGMX__H___
#define ___OCTEON_RGMX__H___



#define OCTEON_FPA_PACKET_POOL                  0
#define OCTEON_FPA_WQE_RX_POOL                  1
#define OCTEON_FPA_OUTPUT_BUFFER_POOL           2
#define OCTEON_FPA_WQE_POOL_SIZE                (1 *  OCTEON_CACHE_LINE_SIZE)
#define OCTEON_FPA_OUTPUT_BUFFER_POOL_SIZE      (8 *  OCTEON_CACHE_LINE_SIZE)
#define OCTEON_FPA_PACKET_POOL_SIZE             (16 * OCTEON_CACHE_LINE_SIZE)

#define OCTEON_POW_WORK_REQUEST(wait)   	(0x8001600000000000ull | (wait<<3))

typedef union
{
    void*           ptr;
    uint64_t        word64;
    struct
    {
        uint64_t    i    : 1;
        uint64_t    back : 4;
        uint64_t    pool : 3;
        uint64_t    size :16;
        uint64_t    addr :40;
    } bits;
} octeon_buf_ptr_t;

/**
 * Work queue entry format
 */
typedef struct
{
    uint16_t                   hw_chksum;
    uint8_t                    unused;
    uint64_t                   next_ptr      : 40;
    uint64_t                   len           :16;
    uint64_t                   ipprt         : 6;
    uint64_t                   qos           : 3;
    uint64_t                   grp           : 4;
    uint64_t                   tag_type      : 3;
    uint64_t                   tag           :32;
    union
    {
        uint64_t               word64;
        struct
        {
            uint64_t           bufs          : 8;
            uint64_t           ip_offset     : 8;
            uint64_t           vlan_valid    : 1;
            uint64_t           unassigned    : 2;
            uint64_t           vlan_cfi      : 1;
            uint64_t           vlan_id       :12;
            uint64_t           unassigned2   :12;
            uint64_t           dec_ipcomp    : 1;
            uint64_t           tcp_or_udp    : 1;
            uint64_t           dec_ipsec     : 1;
            uint64_t           is_v6         : 1;
            uint64_t           software      : 1;
            uint64_t           L4_error      : 1;
            uint64_t           is_frag       : 1;
            uint64_t           IP_exc        : 1;
            uint64_t           is_bcast      : 1;
            uint64_t           is_mcast      : 1;
            uint64_t           not_IP        : 1;
            uint64_t           rcv_error     : 1;
            uint64_t           err_code      : 8;
        } bits;
 struct
        {
            uint64_t           bufs          : 8;
            uint64_t           unused        : 8;
            uint64_t           vlan_valid    : 1;
            uint64_t           unassigned    : 2;
            uint64_t           vlan_cfi      : 1;
            uint64_t           vlan_id       :12;
            uint64_t           unassigned2   :16;
            uint64_t           software      : 1;
            uint64_t           unassigned3   : 1;
            uint64_t           is_rarp       : 1;
            uint64_t           is_arp        : 1;
            uint64_t           is_bcast      : 1;
            uint64_t           is_mcast      : 1;
            uint64_t           not_IP        : 1;
            uint64_t           rcv_error     : 1;
            uint64_t           err_code      : 8;
        } snoip;
    } word2;
    octeon_buf_ptr_t           packet_ptr;
    uint8_t packet_data[96];
} octeon_wqe_t;

typedef union {
    uint64_t         word64;

    struct {
        uint64_t                scraddr : 8;    /**< the (64-bit word) location in scratchpad to write to (if len != 0) */
        uint64_t                len     : 8;    /**< the number of words in the response (0 => no response) */
        uint64_t                did     : 8;    /**< the ID of the device on the non-coherent bus */
        uint64_t                unused  :36;
        uint64_t                wait    : 1;    /**< if set, don't return load response until work is available */
        uint64_t                unused2 : 3;
    } bits;

} octeon_pow_iobdma_store_t;


/**
 * Wait flag values for pow functions.
 */
typedef enum
{
    OCTEON_POW_WAIT = 1,
    OCTEON_POW_NO_WAIT = 0,
} octeon_pow_wait_t;



static inline void * phys_to_virt (unsigned long address)
{
        return (void *)(address + 0x80000000UL);
}

// decode within DMA space
typedef enum {
   OCTEON_ADD_WIN_DMA_ADD = 0L,     // add store data to the write buffer entry, allocating it if necessary
   OCTEON_ADD_WIN_DMA_SENDMEM = 1L, // send out the write buffer entry to DRAM
                                     // store data must be normal DRAM memory space address in this case
   OCTEON_ADD_WIN_DMA_SENDDMA = 2L, // send out the write buffer entry as an IOBDMA command
                                     // see OCTEON_ADD_WIN_DMA_SEND_DEC for data contents
   OCTEON_ADD_WIN_DMA_SENDIO = 3L,  // send out the write buffer entry as an IO write
                                     // store data must be normal IO space address in this case
   OCTEON_ADD_WIN_DMA_SENDSINGLE = 4L, // send out a single-tick command on the NCB bus
                                        // no write buffer data needed/used
} octeon_add_win_dma_dec_t;


#define OCTEON_OCT_DID_FPA	5ULL
#define OCTEON_OCT_DID_TAG	12ULL
#define OCTEON_OCT_DID_TAG_SWTAG OCTEON_ADDR_FULL_DID(OCTEON_OCT_DID_TAG, 0ULL)


#define OCTEON_IOBDMA_OFFSET            (-3*1024ll)
#define OCTEON_IOBDMA_SEP               16 
#define OCTEON_IOBDMA_SENDSINGLE        (OCTEON_IOBDMA_OFFSET +         \
                                        (OCTEON_ADD_WIN_DMA_SENDSINGLE *\
                                        OCTEON_IOBDMA_SEP))

static inline void octeon_send_single (uint64_t data)
{
    oct_write64((uint64_t)(OCTEON_IOBDMA_SENDSINGLE * (long long)8), data);
}


static inline void octeon_pow_work_request_async_nocheck (int scratch_addr,
                                                          octeon_pow_wait_t wait)
{
    octeon_pow_iobdma_store_t data;

    /* scratch_addr must be 8 byte aligned */
    data.bits.scraddr = scratch_addr >> 3;
    data.bits.len = 1;
    data.bits.did = OCTEON_OCT_DID_TAG_SWTAG;
    data.bits.wait = wait;
    octeon_send_single(data.word64);
}



/**
 * octeon_gmx_inf_mode
 *
 * GMX_INF_MODE = Interface Mode
 *
 */
typedef union
{       
    uint64_t word64;
    struct gmxx_inf_mode_s
    {       
        uint64_t reserved_3_63           : 61;
        uint64_t p0mii                   : 1;       /**< Port 0 Interface Mode
                                                         0: Port 0 is RGMII
                                                         1: Port 0 is MII */
        uint64_t en                      : 1;       /**< Interface Enable */
        uint64_t type                    : 1;       /**< Interface Mode
                                                         0: RGMII Mode
                                                         1: Spi4 Mode */
    } bits;
    struct gmxx_inf_mode_cn3020
    {               
        uint64_t reserved_2_63           : 62;
        uint64_t en                      : 1;       /**< Interface Enable */
        uint64_t type                    : 1;       /**< Interface Mode
                                                         0: All three ports are RGMII ports
                                                         1: prt0 is RGMII, prt1 is GMII, and prt2 is unused */
    } cn3020;
    struct gmxx_inf_mode_s          cn30xx;
    struct gmxx_inf_mode_cn3020     cn31xx;
    struct gmxx_inf_mode_cn3020     cn36xx;
    struct gmxx_inf_mode_cn3020     cn38xx;
    struct gmxx_inf_mode_cn3020     cn38xxp2;
    struct gmxx_inf_mode_cn3020     cn56xx;
    struct gmxx_inf_mode_cn3020     cn58xx;
} octeon_gmxx_inf_mode_t;




typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved	: 60;      /* Reserved */
        uint64_t slottime	: 1;       /* Slot Time for Half-Duplex */
        /* operation - 0 = 512 bitimes (10/100Mbs operation) */
        /* - 1 = 4096 bitimes (1000Mbs operation) */
        uint64_t duplex		: 1;       /* Duplex - 0 = Half Duplex */
        /* (collisions/extentions/bursts)            - 1 = Full Duplex */
        uint64_t speed		: 1;       /* Link Speed - 0 = 10/100Mbs */
	/* operation - 1 = 1000Mbs operation */
        uint64_t en		: 1;       /* Link Enable */
    } bits;
} octeon_rgmx_prtx_cfg_t;


/*
 * GMX_RX_INBND = RGMX InBand Link Status
 *
 */
typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved	: 60;      /* Reserved */
        uint64_t duplex		: 1;       /* 0 = Half, 1 = Full */
        uint64_t speed		: 2;       /* Inbound Link Speed */
					   /* 00 = 2.5Mhz, 01 = 25Mhz */
        				   /* 10 = 125MHz, 11 = Reserved */
        uint64_t status		: 1;       /* Inbound Status Up/Down */
    } bits;
} octeon_rgmx_rxx_rx_inbnd_t;



typedef union
{
    uint64_t word64;
    struct {
        uint64_t all_drop                    : 32;
        uint64_t slow_drop                   : 32;
    } bits;
} octeon_rgmx_ipd_queue_red_marks_t;


typedef union
{
    uint64_t word64;
    struct {
        uint64_t reserved	         : 15;
        uint64_t use_pagecount           : 1;
        uint64_t new_con                 : 8;
        uint64_t avg_con                 : 8;
        uint64_t prb_con                 : 32;
    } bits;
} octeon_rgmx_ipd_red_q_param_t;



typedef union
{
    uint64_t word64;
    struct {
        uint64_t reserved          	 : 46;
        uint64_t bp_enable               : 1;
        uint64_t page_count              : 17;
    } bits;
} octeon_ipd_port_bp_page_count_t;


typedef union
{
    uint64_t word64;
    struct {
        uint64_t prb_dly                 : 14;
        uint64_t avg_dly                 : 14;
        uint64_t port_enable             : 36;
    } bits;
} octeon_ipd_red_port_enable_t;


/**
 * Tag type definitions
 */ 
typedef enum
{
    OCTEON_POW_TAG_TYPE_ORDERED   = 0L,   /**< Tag ordering is maintained */
    OCTEON_POW_TAG_TYPE_ATOMIC    = 1L,   /**< Tag ordering is maintained, and at most one PP has the tag */
    OCTEON_POW_TAG_TYPE_NULL      = 2L,   /**< The work queue entry from the order
                                            - NEVER tag switch from NULL to NULL */
    OCTEON_POW_TAG_TYPE_NULL_NULL = 3L    /**< A tag switch to NULL, and there is no space reserved in POW
                                            - NEVER tag switch to NULL_NULL
                                            - NEVER tag switch from NULL_NULL
                                            - NULL_NULL is entered at the beginning of time and on a deschedule.
                                            - NULL_NULL can be exited by a new work request. A NULL_SWITCH load can also switch the state to NULL */
} octeon_pow_tag_type_t ;

/**
 * This structure defines the response to a load/SENDSINGLE to POW (except CSR reads)
 */
typedef union {
    uint64_t         word64;

    octeon_wqe_t *wqp;

    // response to new work request loads
    struct {
        uint64_t       no_work : 1;   // set when no new work queue entry was returned
        // If there was de-scheduled work, the HW will definitely
        // return it. When this bit is set, it could mean
        // either mean:
        //   - There was no work, or
        //   - There was no work that the HW could find. This
        //     case can happen, regardless of the wait bit value
        //     in the original request, when there is work
        //     in the IQ's that is too deep down the list.
        uint64_t       unused  : 23;
        uint64_t       addr    : 40;  // 36 in O1 -- the work queue pointer
    } s_work;

    // response to NULL_RD request loads
    struct {
        uint64_t       unused  : 62;
        uint64_t       state    : 2;  // of type octeon_pow_tag_type_t
        // state is one of the following:
        //       OCTEON_POW_TAG_TYPE_ORDERED
        //       OCTEON_POW_TAG_TYPE_ATOMIC
        //       OCTEON_POW_TAG_TYPE_NULL
        //       OCTEON_POW_TAG_TYPE_NULL_NULL
    } s_null_rd;

} octeon_pow_tag_load_resp_t;


/*
 * This structure describes the address to load stuff from POW
 */
typedef union {
    uint64_t word64;

    // address for new work request loads (did<2:0> == 0)
    struct {
        uint64_t                mem_region  :2;
        uint64_t                mbz  :13;
        uint64_t                is_io  : 1;    // must be one
        uint64_t                did    : 8;    // the ID of POW -- did<2:0> == 0 in this case
        uint64_t                unaddr : 4;
        uint64_t                unused :32;
        uint64_t                wait   : 1;    // if set, don't return load response until work is available
        uint64_t                mbzl   : 3;    // must be zero
    } swork; // physical address


    // address for NULL_RD request (did<2:0> == 4)
    // when this is read, HW attempts to change the state to NULL if it is NULL_NULL
    // (the hardware cannot switch from NULL_NULL to NULL if a POW entry is not available -
    // software may need to recover by finishing another piece of work before a POW
    // entry can ever become available.)
    struct {
        uint64_t                mem_region  :2;
        uint64_t                mbz  :13;
        uint64_t                is_io  : 1;    // must be one
        uint64_t                did    : 8;    // the ID of POW -- did<2:0> == 4 in this case
        uint64_t                unaddr : 4;
        uint64_t                unused :33;
        uint64_t                mbzl   : 3;    // must be zero
    } snull_rd; // physical address

    // address for CSR accesses
    struct {
        uint64_t                mem_region  :2;
        uint64_t                mbz  :13;
        uint64_t                is_io  : 1;    // must be one
        uint64_t                did    : 8;    // the ID of POW -- did<2:0> == 7 in this case
        uint64_t                unaddr : 4;
        uint64_t                csraddr:36;    // only 36 bits in O1, addr<2:0> must be zero
    } stagcsr; // physical address

} octeon_pow_load_addr_t;


static inline void octeon_pow_tag_switch_wait (void)
{
    uint64_t switch_complete;

    do
    {
        OCTEON_CHORD_HEX(&switch_complete);
    } while (!switch_complete);

    return;
}


static inline octeon_wqe_t *octeon_pow_work_request_sync_nocheck (octeon_pow_wait_t wait)
{
    octeon_pow_load_addr_t ptr;
    octeon_pow_tag_load_resp_t result;

    ptr.word64 = 0;
    ptr.swork.mem_region = OCTEON_IO_SEG;
    ptr.swork.is_io = 1;
    ptr.swork.did = OCTEON_OCT_DID_TAG_SWTAG;
    ptr.swork.wait = wait;

    result.word64 = oct_read64(ptr.word64);

    if (result.s_work.no_work || !result.s_work.addr) {
        return NULL;
    }
    return (octeon_wqe_t *) MIPS_PHYS_TO_KSEG0(result.s_work.addr);
}

static inline octeon_wqe_t *octeon_pow_work_request_sync_nocheck_debug (octeon_pow_wait_t wait)
{
    octeon_pow_load_addr_t ptr;
    octeon_pow_tag_load_resp_t result;

    ptr.word64 = 0;
    ptr.swork.mem_region = OCTEON_IO_SEG;
    ptr.swork.is_io = 1;
    ptr.swork.did = OCTEON_OCT_DID_TAG_SWTAG;
    ptr.swork.wait = wait;

    result.word64 = oct_read64(ptr.word64);

    printf("WQE Result: 0x%llX  No-work %X   Addr %llX  Ptr: %p\n",
	(unsigned long long)result.word64,  result.s_work.no_work,
	(unsigned long long)result.s_work.addr,
	(void *)MIPS_PHYS_TO_KSEG0(result.s_work.addr));

    if (result.s_work.no_work || !result.s_work.addr) {
        return NULL;
    }
    return (octeon_wqe_t *) MIPS_PHYS_TO_KSEG0(result.s_work.addr);
}

static inline octeon_wqe_t *octeon_pow_work_request_sync (octeon_pow_wait_t wait)
{
    octeon_pow_tag_switch_wait();
    return (octeon_pow_work_request_sync_nocheck(wait));
}


static inline octeon_wqe_t *octeon_pow_work_request_sync_debug (octeon_pow_wait_t wait)
{
    octeon_pow_tag_switch_wait();
    return (octeon_pow_work_request_sync_nocheck_debug(wait));
}
    


/**
 * Gets result of asynchronous work request.  Performs a IOBDMA sync
 * to wait for the response.
 *
 * @param scratch_addr Scratch memory address to get result from
 *                  Byte address, must be 8 byte aligned.
 * @return Returns the WQE from the scratch register, or NULL if no work was available.
 */
static inline octeon_wqe_t *octeon_pow_work_response_async(int scratch_addr)
{
    octeon_pow_tag_load_resp_t result;

    OCTEON_SYNCIOBDMA;
    result.word64 = oct_scratch_read64(scratch_addr);

    if (result.s_work.no_work) {
        return NULL;
    }
    return (octeon_wqe_t*) MIPS_PHYS_TO_KSEG0(result.s_work.addr);
}



/*
 * The address from POW is a physical address. Adjust for back ptr, as well as
 * make it accessible using  KSEG0.
 */
static inline void *octeon_pow_pktptr_to_kbuffer (octeon_buf_ptr_t pkt_ptr)
{
    return ((void *)MIPS_PHYS_TO_KSEG0(
	((pkt_ptr.bits.addr >> 7) - pkt_ptr.bits.back) << 7));
}

#define INTERFACE(port) (port >> 4) /* Ports 0-15 are interface 0, 16-31 are interface 1 */
#define INDEX(port) (port & 0xf)


#define  OCTEON_RGMX_PRTX_CFG(index,interface)	(0x8001180008000010ull+((index)*2048)+((interface)*0x8000000ull))
#define  OCTEON_RGMX_SMACX(offset,block_id)	(0x8001180008000230ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_RXX_ADR_CAM0(offset,block_id)	(0x8001180008000180ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_RXX_ADR_CAM1(offset,block_id)	(0x8001180008000188ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_RXX_ADR_CAM2(offset,block_id)	(0x8001180008000190ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_RXX_ADR_CAM3(offset,block_id)	(0x8001180008000198ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_RXX_ADR_CAM4(offset,block_id)	(0x80011800080001A0ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_RXX_ADR_CAM5(offset,block_id)	(0x80011800080001A8ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_RXX_ADR_CTL(offset,block_id)	(0x8001180008000100ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_RXX_ADR_CAM_EN(offset,block_id)	(0x8001180008000108ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_INF_MODE(block_id)		(0x80011800080007F8ull+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_TX_PRTS(block_id)		(0x8001180008000480ull+((block_id)*0x8000000ull))
#define  OCTEON_ASXX_RX_PRT_EN(block_id)	(0x80011800B0000000ull+((block_id)*0x8000000ull))
#define  OCTEON_ASXX_TX_PRT_EN(block_id)	(0x80011800B0000008ull+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_TXX_THRESH(offset,block_id)	(0x8001180008000210ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_ASXX_TX_HI_WATERX(offset,block_id)	(0x80011800B0000080ull+((offset)*8)+((block_id)*0x8000000ull))
#define  OCTEON_ASXX_RX_CLK_SETX(offset,block_id)	(0x80011800B0000020ull+((offset)*8)+((block_id)*0x8000000ull))
#define  OCTEON_ASXX_TX_CLK_SETX(offset,block_id)	(0x80011800B0000048ull+((offset)*8)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_RXX_RX_INBND(offset,block_id)	(0x8001180008000060ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_TXX_CLK(offset,block_id)	(0x8001180008000208ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_TXX_SLOT(offset,block_id)	(0x8001180008000220ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_RGMX_TXX_BURST(offset,block_id)	(0x8001180008000228ull+((offset)*2048)+((block_id)*0x8000000ull))
#define  OCTEON_PIP_GBL_CTL			(0x80011800A0000020ull)
#define  OCTEON_PIP_GBL_CFG			(0x80011800A0000028ull)
#define  OCTEON_PIP_PRT_CFGX(offset)		(0x80011800A0000200ull+((offset)*8))
#define  OCTEON_PIP_PRT_TAGX(offset)		(0x80011800A0000400ull+((offset)*8))



#define OUR_CORE	0
#define IP2		0
#define IP3		1
#define CIU_TIMERS	4
#define OCTEON_POW_CORE_GROUP_MASK(core)  (0x8001670000000000ull + (8 * core))

#define	OCTEON_CIU_INT_EN0(CORE,IP)  (0x8001070000000200ull + (IP * 16) + \
					((CORE) * 32))
#define	OCTEON_CIU_INT_SUM0(CORE,IP) (0x8001070000000000ull + (IP * 8) + \
					((CORE) * 32))
#define OCTEON_CIU_TIMX(offset)	     (0x8001070000000480ull+((offset)*8))

#define	OCTEON_POW_WQ_INT_THRX(offset)  ((0x8001670000000080ull+((offset)*8)))
#define	OCTEON_POW_WQ_INT_CNTX(offset)  ((0x8001670000000100ull+((offset)*8)))
#define	OCTEON_POW_QOS_THRX(offset)     ((0x8001670000000180ull+((offset)*8)))
#define	OCTEON_POW_QOS_RNDX(offset)     ((0x80016700000001C0ull+((offset)*8)))
#define	OCTEON_POW_WQ_INT_PC            (0x8001670000000208ull)
#define	OCTEON_POW_NW_TIM               (0x8001670000000210ull)
#define	OCTEON_POW_ECC_ERR              (0x8001670000000218ull)
#define	OCTEON_POW_INT_CTL              (0x8001670000000220ull)
#define	OCTEON_POW_NOS_CNT              (0x8001670000000228ull)
#define	OCTEON_POW_WS_PCX(offset)       ((0x8001670000000280ull+((offset)*8)))
#define	OCTEON_POW_WA_PCX(offset)       ((0x8001670000000300ull+((offset)*8)))
#define	OCTEON_POW_IQ_CNTX(offset)      ((0x8001670000000340ull+((offset)*8)))
#define	OCTEON_POW_WA_COM_PC            (0x8001670000000380ull)
#define	OCTEON_POW_IQ_COM_CNT           (0x8001670000000388ull)
#define	OCTEON_POW_TS_PC                (0x8001670000000390ull)
#define	OCTEON_POW_DS_PC                (0x8001670000000398ull)
#define	OCTEON_POW_BIST_STAT            (0x80016700000003F8ull)


#define	OCTEON_POW_WQ_INT               (0x8001670000000200ull)

#define OCTEON_IPD_PORT_BP_COUNTERS_PAIRX(offset)  (0x80014F00000001B8ull+((offset)*8))

/*
 * Current Counts that triggered interrupt
 */
#define  OCTEON_POW_WQ_INT_CNTX(offset)	((0x8001670000000100ull+((offset)*8)))



#define OCTEON_RGMX_ADRCTL_CAM_MODE_REJECT_DMAC	0
#define OCTEON_RGMX_ADRCTL_ACCEPT_BROADCAST	1
#define OCTEON_RGMX_ADRCTL_REJECT_ALL_MULTICAST	2
#define OCTEON_RGMX_ADRCTL_ACCEPT_ALL_MULTICAST	4
#define OCTEON_RGMX_ADRCTL_CAM_MODE_ACCEPT_DMAC	8


#define	RGMX_LOCK_INIT(_sc, _name) \
	mtx_init(&(_sc)->mtx, _name, MTX_NETWORK_LOCK, MTX_DEF)
#define	RGMX_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->mtx)
#define	RGMX_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define	RGMX_UNLOCK(_sc)	mtx_unlock(&(_sc)->mtx)
#define	RGMX_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED)

#endif /* ___OCTEON_RGMX__H___ */
