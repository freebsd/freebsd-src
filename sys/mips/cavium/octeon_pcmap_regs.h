/*
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors."
 */

/* $FreeBSD$ */

#ifndef __OCTEON_PCMAP_REGS_H__
#define __OCTEON_PCMAP_REGS_H__

#include "opt_cputype.h" 

#define OCTEON_CACHE_LINE_SIZE	0x80	/* 128 bytes cache line size */
#define IS_OCTEON_ALIGNED(p)	(!((u_long)(p) & 0x7f))
#define OCTEON_ALIGN(p)		(((u_long)(p) + ((OCTEON_CACHE_LINE_SIZE) - 1)) & ~((OCTEON_CACHE_LINE_SIZE) - 1))

#ifndef LOCORE

/* XXXimp: From Cavium's include/pcpu.h, need to port that over */
#ifndef OCTEON_SMP
#define OCTEON_CORE_ID 0
#else
extern struct pcpu *cpuid_to_pcpu[];
#define OCTEON_CORE_ID (mips_rd_coreid())
#endif

/*
 * Utility inlines & macros
 */

/* turn the variable name into a string */
#define OCTEON_TMP_STR(x) OCTEON_TMP_STR2(x)
#define OCTEON_TMP_STR2(x) #x

#define OCTEON_PREFETCH_PREF0(address, offset) \
	__asm __volatile (	".set mips64\n"	     \
			".set noreorder\n"   \
			"pref 0, " OCTEON_TMP_STR(offset) "(%0)\n" \
			".set reorder\n"     \
			".set mips0\n"	     \
			 : \
			 : "r" (address) );

#define OCTEON_PREFETCH(address, offset) OCTEON_PREFETCH_PREF0(address,offset)

#define OCTEON_PREFETCH0(address) OCTEON_PREFETCH(address, 0)
#define OCTEON_PREFETCH128(address) OCTEON_PREFETCH(address, 128)

#define OCTEON_SYNCIOBDMA __asm __volatile (".word 0x8f" : : :"memory")

#define OCTEON_SYNCW	__asm __volatile (".word  0x10f" : : )
#define OCTEON_SYNCW	__asm __volatile (".word  0x10f" : : )
#define OCTEON_SYNCWS	__asm __volatile (".word  0x14f" : : )

#if defined(__mips_n32) || defined(__mips_n64)

static inline void oct_write64 (uint64_t csr_addr, uint64_t val64)
{
    uint64_t *ptr = (uint64_t *) csr_addr;
    *ptr = val64;
}

static inline void oct_write64_int64 (uint64_t csr_addr, int64_t val64i)
{
    int64_t *ptr = (int64_t *) csr_addr;
    *ptr = val64i;
}

static inline void oct_write8_x8 (uint64_t csr_addr, uint8_t val8)
{
    uint64_t *ptr = (uint64_t *) csr_addr;
    *ptr = (uint64_t) val8;
}

static inline void oct_write8 (uint64_t csr_addr, uint8_t val8)
{
    oct_write64(csr_addr, (uint64_t) val8);
}

static inline void oct_write16 (uint64_t csr_addr, uint16_t val16)
{
    oct_write64(csr_addr, (uint64_t) val16);
}

static inline void oct_write32 (uint64_t csr_addr, uint32_t val32)
{
    oct_write64(csr_addr, (uint64_t) val32);
}

static inline uint8_t oct_read8 (uint64_t csr_addr)
{
    uint8_t *ptr = (uint8_t *) csr_addr;
    return (*ptr);
}

static inline uint8_t oct_read16 (uint64_t csr_addr)
{
    uint16_t *ptr = (uint16_t *) csr_addr;
    return (*ptr);
}


static inline uint32_t oct_read32 (uint64_t csr_addr)
{
    uint32_t *ptr = (uint32_t *) csr_addr;
    return (*ptr);
}

static inline uint64_t oct_read64 (uint64_t csr_addr)
{
    uint64_t *ptr = (uint64_t *) csr_addr;
    return (*ptr);
}

static inline int32_t oct_readint32 (uint64_t csr_addr)
{
    int32_t *ptr = (int32_t *) csr_addr;
    return (*ptr);
}



#else


/*  ABI o32 */


/*
 * Read/write functions
 */
static inline void oct_write64 (uint64_t csr_addr, uint64_t val64)
{
    uint32_t csr_addrh = csr_addr >> 32;
    uint32_t csr_addrl = csr_addr;
    uint32_t valh = (uint64_t)val64 >> 32;
    uint32_t vall = val64;
    uint32_t tmp1;
    uint32_t tmp2;
    uint32_t tmp3;

    __asm __volatile (
            ".set mips64\n"
            "dsll   %0, %3, 32\n"
            "dsll   %1, %5, 32\n"
            "dsll   %2, %4, 32\n"
            "dsrl   %2, %2, 32\n"
            "or     %0, %0, %2\n"
            "dsll   %2, %6, 32\n"
            "dsrl   %2, %2, 32\n"
            "or     %1, %1, %2\n"
            "sd     %0, 0(%1)\n"
            ".set mips0\n"
            : "=&r" (tmp1), "=&r" (tmp2), "=&r" (tmp3)
            : "r" (valh), "r" (vall),
              "r" (csr_addrh), "r" (csr_addrl)
        );
}

static inline void oct_write64_int64 (uint64_t csr_addr, int64_t val64i)
{
    uint32_t csr_addrh = csr_addr >> 32;
    uint32_t csr_addrl = csr_addr;
    int32_t valh = (uint64_t)val64i >> 32;
    int32_t vall = val64i;
    uint32_t tmp1;
    uint32_t tmp2;
    uint32_t tmp3;

    __asm __volatile (
            ".set mips64\n"
            "dsll   %0, %3, 32\n"
            "dsll   %1, %5, 32\n"
            "dsll   %2, %4, 32\n"
            "dsrl   %2, %2, 32\n"
            "or     %0, %0, %2\n"
            "dsll   %2, %6, 32\n"
            "dsrl   %2, %2, 32\n"
            "or     %1, %1, %2\n"
            "sd     %0, 0(%1)\n"
            ".set mips0\n"
            : "=&r" (tmp1), "=&r" (tmp2), "=&r" (tmp3)
            : "r" (valh), "r" (vall),
              "r" (csr_addrh), "r" (csr_addrl)
        );
}


/*
 * oct_write8_x8
 *
 * 8 bit data write into IO Space. Written using an 8 bit bus io transaction
 */
static inline void oct_write8_x8 (uint64_t csr_addr, uint8_t val8)
{
    uint32_t csr_addrh = csr_addr>>32;
    uint32_t csr_addrl = csr_addr;
    uint32_t tmp1;
    uint32_t tmp2;

    __asm __volatile (
        ".set mips64\n"
        "dsll   %0, %3, 32\n"
        "dsll   %1, %4, 32\n"
        "dsrl   %1, %1, 32\n"
        "or     %0, %0, %1\n"
        "sb     %2, 0(%0)\n"
	".set mips0\n"
        : "=&r" (tmp1), "=&r" (tmp2)
        : "r" (val8), "r" (csr_addrh), "r" (csr_addrl) );
}

/*
 * oct_write8
 *
 * 8 bit data write into IO Space. Written using a 64 bit bus io transaction
 */
static inline void oct_write8 (uint64_t csr_addr, uint8_t val8)
{
#if 1
    oct_write64(csr_addr, (uint64_t) val8);
#else

    uint32_t csr_addrh = csr_addr>>32;
    uint32_t csr_addrl = csr_addr;
    uint32_t tmp1;
    uint32_t tmp2;

    __asm __volatile (
        ".set mips64\n"
        "dsll   %0, %3, 32\n"
        "dsll   %1, %4, 32\n"
        "dsrl   %1, %1, 32\n"
        "or     %0, %0, %1\n"
        "sb     %2, 0(%0)\n"
        ".set mips0\n"
        : "=&r" (tmp1), "=&r" (tmp2)
        : "r" (val8), "r" (csr_addrh), "r" (csr_addrl) );
#endif
}

static inline void oct_write16 (uint64_t csr_addr, uint16_t val16)
{
#if 1
    oct_write64(csr_addr, (uint64_t) val16);

#else
    uint32_t csr_addrh = csr_addr>>32;
    uint32_t csr_addrl = csr_addr;
    uint32_t tmp1;
    uint32_t tmp2;

    __asm __volatile (
        ".set mips64\n"
        "dsll   %0, %3, 32\n"
        "dsll   %1, %4, 32\n"
        "dsrl   %1, %1, 32\n"
        "or     %0, %0, %1\n"
        "sh     %2, 0(%0)\n"
	".set mips0\n"
        : "=&r" (tmp1), "=&r" (tmp2)
        : "r" (val16), "r" (csr_addrh), "r" (csr_addrl) );
#endif
}

static inline void oct_write32 (uint64_t csr_addr, uint32_t val32)
{
#if 1
    oct_write64(csr_addr, (uint64_t) val32);
#else

    uint32_t csr_addrh = csr_addr>>32;
    uint32_t csr_addrl = csr_addr;
    uint32_t tmp1;
    uint32_t tmp2;

    __asm __volatile (
        ".set mips64\n"
        "dsll   %0, %3, 32\n"
        "dsll   %1, %4, 32\n"
        "dsrl   %1, %1, 32\n"
        "or     %0, %0, %1\n"
        "sw     %2, 0(%0)\n"
	".set mips0\n"
        : "=&r" (tmp1), "=&r" (tmp2)
        : "r" (val32), "r" (csr_addrh), "r" (csr_addrl) );
#endif
}



static inline uint8_t oct_read8 (uint64_t csr_addr)
{
    uint32_t csr_addrh = csr_addr>>32;
    uint32_t csr_addrl = csr_addr;
    uint32_t tmp1, tmp2;

    __asm __volatile (
        ".set mips64\n"
        "dsll   %1, %2, 32\n"
        "dsll   %0, %3, 32\n"
        "dsrl   %0, %0, 32\n"
        "or     %1, %1, %0\n"
        "lb     %1, 0(%1)\n"
        ".set mips0\n"
        : "=&r" (tmp1), "=&r" (tmp2)
        : "r" (csr_addrh), "r" (csr_addrl) );
    return ((uint8_t) tmp2);
}

static inline uint8_t oct_read16 (uint64_t csr_addr)
{
    uint32_t csr_addrh = csr_addr>>32;
    uint32_t csr_addrl = csr_addr;
    uint32_t tmp1, tmp2;

    __asm __volatile (
        ".set mips64\n"
        "dsll   %1, %2, 32\n"
        "dsll   %0, %3, 32\n"
        "dsrl   %0, %0, 32\n"
        "or     %1, %1, %0\n"
        "lh     %1, 0(%1)\n"
        ".set mips0\n"
        : "=&r" (tmp1), "=&r" (tmp2)
        : "r" (csr_addrh), "r" (csr_addrl) );
    return ((uint16_t) tmp2);
}


static inline uint32_t oct_read32 (uint64_t csr_addr)
{
    uint32_t csr_addrh = csr_addr>>32;
    uint32_t csr_addrl = csr_addr;
    uint32_t val32;
    uint32_t tmp;

    __asm __volatile (
        ".set mips64\n"
        "dsll   %0, %2, 32\n"
        "dsll   %1, %3, 32\n"
        "dsrl   %1, %1, 32\n"
        "or     %0, %0, %1\n"
        "lw    %0, 0(%0)\n"
        ".set mips0\n"
        : "=&r" (val32), "=&r" (tmp)
        : "r" (csr_addrh), "r" (csr_addrl) );
    return (val32);
}


static inline uint64_t oct_read64 (uint64_t csr_addr)
{
    uint32_t csr_addrh = csr_addr >> 32;
    uint32_t csr_addrl = csr_addr;
    uint32_t valh;
    uint32_t vall;

    __asm __volatile (
        ".set mips64\n"
        "dsll   %0, %2, 32\n"
        "dsll   %1, %3, 32\n"
        "dsrl   %1, %1, 32\n"
        "or     %0, %0, %1\n"
        "ld     %1, 0(%0)\n"
        "dsrl   %0, %1, 32\n"
        "dsll   %1, %1, 32\n"
        "dsrl   %1, %1, 32\n"
        ".set mips0\n"
        : "=&r" (valh), "=&r" (vall)
        : "r" (csr_addrh), "r" (csr_addrl)
        );
    return ((uint64_t)valh << 32) | vall;
}


static inline int32_t oct_readint32 (uint64_t csr_addr)
{
    uint32_t csr_addrh = csr_addr>>32;
    uint32_t csr_addrl = csr_addr;
    int32_t val32;
    uint32_t tmp;

    __asm __volatile (
        ".set mips64\n"
        "dsll   %0, %2, 32\n"
        "dsll   %1, %3, 32\n"
        "dsrl   %1, %1, 32\n"
        "or     %0, %0, %1\n"
        "lw    %0, 0(%0)\n"
        : "=&r" (val32), "=&r" (tmp)
        : "r" (csr_addrh), "r" (csr_addrl) );
    return (val32);
}


#endif


#define OCTEON_HW_BASE		((volatile uint64_t *) 0L)
#define OCTEON_REG_OFFSET	(-4 * 1024ll)  /* local scratchpad reg base */
#define OCTEON_SCRATCH_BASE	((volatile uint8_t *)(OCTEON_HW_BASE +	\
                                                      OCTEON_REG_OFFSET))

#define OCTEON_SCR_SCRATCH   8
#define OCTEON_SCRATCH_0   16
#define OCTEON_SCRATCH_1   24
#define OCTEON_SCRATCH_2   32


static inline uint64_t oct_mf_chord (void)
{
    uint64_t dest;

    __asm __volatile (	".set push\n"
                        ".set noreorder\n"
                        ".set noat\n"
                        ".set mips64\n"
			"dmfc2 $1, 0x400\n"
                        "move %0, $1\n"
        		".set pop\n"
 			: "=r" (dest) :  : "$1");
    return dest;
}


#define MIPS64_DMFCz(cop,regnum,cp0reg,select)  \
        .word   (0x40200000 | (cop << 25) | (regnum << 16) | (cp0reg << 11) | select)


#define mips64_getcpz_xstr(s) mips64_getcpz_str(s)
#define mips64_getcpz_str(s) #s

#define mips64_dgetcpz(cop,cpzreg,sel,val_ptr) \
    ({ __asm __volatile( \
            ".set push\n" \
            ".set mips3\n" \
            ".set noreorder\n" \
            ".set noat\n" \
            mips64_getcpz_xstr(MIPS64_DMFCz(cop,1,cpzreg,sel)) "\n" \
            "nop\n" \
            "nop\n" \
            "nop\n" \
            "nop\n" \
            "sd $1,0(%0)\n" \
            ".set pop" \
            : /* no outputs */ : "r" (val_ptr) : "$1"); \
    })


#define mips64_dgetcp2(cp2reg,sel,retval_ptr) \
    mips64_dgetcpz(2,cp2reg,sel,retval_ptr)


#define OCTEON_MF_CHORD(dest)  mips64_dgetcp2(0x400, 0, &dest)



#define OCTEON_RDHWR(result, regstr) \
	__asm __volatile (		\
        		".set mips3\n"		\
			"rdhwr %0,$" OCTEON_TMP_STR(regstr) "\n"	\
        		".set mips\n"		\
			 : "=d" (result));

#define CVMX_MF_CHORD(dest)         OCTEON_RDHWR(dest, 30)

#define OCTEON_CHORD_HEX(dest_ptr)  \
    ({ __asm __volatile( \
            ".set push\n" \
            ".set mips3\n" \
            ".set noreorder\n" \
            ".set noat\n" \
	    ".word 0x7c02f03b \n"\
            "nop\n" \
            "nop\n" \
            "nop\n" \
            "nop\n" \
            "sd $2,0(%0)\n" \
            ".set pop" \
            : /* no outputs */ : "r" (dest_ptr) : "$2"); \
    })



#define OCTEON_MF_CHORD_BAD(dest)	\
         __asm __volatile (		\
        		".set mips3\n"		\
			"dmfc2 %0, 0x400\n"	\
        		".set mips0\n"		\
 			: "=&r" (dest) : )

static inline uint64_t oct_scratch_read64 (uint64_t address)
{
    return(*((volatile uint64_t *)(OCTEON_SCRATCH_BASE + address)));
}

static inline void oct_scratch_write64 (uint64_t address, uint64_t value)
{
    *((volatile uint64_t *)(OCTEON_SCRATCH_BASE + address)) = value;
}


#define OCTEON_READ_CSR32(addr, val) \
	addr_ptr = addr; \
	oct_read_32_ptr(&addr_ptr, &val);

#define OCTEON_WRITE_CSR32(addr, val, val_dummy) \
	addr_ptr = addr; \
	oct_write_32_ptr(&addr_ptr, &val); \
	oct_read64(OCTEON_MIO_BOOT_BIST_STAT);



/*
 * Octeon Address Space Definitions
 */
typedef enum {
   OCTEON_MIPS_SPACE_XKSEG = 3LL,
   OCTEON_MIPS_SPACE_XKPHYS = 2LL,
   OCTEON_MIPS_SPACE_XSSEG = 1LL,
   OCTEON_MIPS_SPACE_XUSEG = 0LL
} octeon_mips_space_t;

typedef enum {
   OCTEON_MIPS_XKSEG_SPACE_KSEG0 = 0LL,
   OCTEON_MIPS_XKSEG_SPACE_KSEG1 = 1LL,
   OCTEON_MIPS_XKSEG_SPACE_SSEG = 2LL,
   OCTEON_MIPS_XKSEG_SPACE_KSEG3 = 3LL
} octeon_mips_xkseg_space_t;


/*
***********************************************************************
 * 32 bit mode alert
 * The kseg0 calc below might fail in xkphys.
 */

/*
 * We limit the allocated device physical blocks to low mem. So use Kseg0
 */

/*
 * Need to go back to kernel to find v->p mappings & vice-versa
 * We are getting non 1-1 mappings.
 * #define OCTEON_PTR2PHYS(addr)  ((unsigned long) addr & 0x7fffffff)
 */
#define OCTEON_PTR2PHYS(addr) octeon_ptr_to_phys(addr)



/*  PTR_SIZE == sizeof(uint32_t)  */

#ifdef ISA_MIPS32
#define mipsx_addr_size				uint32_t	// u_int64
#define MIPSX_ADDR_SIZE_KSEGX_BIT_SHIFT		30		// 62
#define MIPSX_ADDR_SIZE_KSEGX_MASK_REMOVED	0x1fffffff	// 0x1fffffff
#else
#define mipsx_addr_size				uint64_t
#define MIPSX_ADDR_SIZE_KSEGX_BIT_SHIFT		62
#define MIPSX_ADDR_SIZE_KSEGX_MASK_REMOVED	0x1fffffffffffffff
#endif


#define octeon_ptr_to_phys(ptr)                                           \
    ((((mipsx_addr_size) ptr) >> MIPSX_ADDR_SIZE_KSEGX_BIT_SHIFT) == 2) ? \
    	((mipsx_addr_size) ptr & MIPSX_ADDR_SIZE_KSEGX_MASK_REMOVED)  :   \
    	(vtophys(ptr))


#ifdef CODE_FOR_64_BIT_NEEDED
static inline mipsx_addr_size octeon_ptr_to_phys (void *ptr)
{
    if ((((mipsx_addr_size) ptr) >> MIPSX_ADDR_SIZE_KSEGX_BIT_SHIFT) == 2) {
        /*
         * KSEG0 based address ?
         */
        return ((mipsx_addr_size) ptr & MIPSX_ADDR_SIZE_KSEGX_MASK_REMOVED);
    } else {
        /*
         * Ask kernel/vm to give us the phys translation.
         */
        return (vtophys(ptr));
    }
}
#endif

#define OCTEON_IO_SEG OCTEON_MIPS_SPACE_XKPHYS


#define OCTEON_ADD_SEG(segment, add)	((((uint64_t)segment) << 62) | (add))

#define OCTEON_ADD_IO_SEG(add)		OCTEON_ADD_SEG(OCTEON_IO_SEG, (add))
#define OCTEON_ADDR_DID(did)		(OCTEON_ADDR_DIDSPACE(did) << 40)
#define OCTEON_ADDR_DIDSPACE(did)	(((OCTEON_IO_SEG) << 22) | ((1ULL) << 8) | (did))
#define OCTEON_ADDR_FULL_DID(did,subdid)	(((did) << 3) | (subdid))


#define OCTEON_CIU_PP_RST	OCTEON_ADD_IO_SEG(0x0001070000000700ull)
#define OCTEON_OCTEON_DID_TAG	12ULL




/*
 * octeon_addr_t
 */
typedef union {
   uint64_t         word64;

   struct {
       octeon_mips_space_t          R   : 2;
       uint64_t               offset :62;
   } sva; // mapped or unmapped virtual address

   struct {
       uint64_t               zeroes :33;
       uint64_t               offset :31;
   } suseg; // mapped USEG virtual addresses (typically)

   struct {
       uint64_t                ones  :33;
       octeon_mips_xkseg_space_t   sp   : 2;
       uint64_t               offset :29;
   } sxkseg; // mapped or unmapped virtual address

   struct {
       octeon_mips_space_t         R    :2; // CVMX_MIPS_SPACE_XKPHYS in this case
       uint64_t                 cca  : 3; // ignored by octeon
       uint64_t                 mbz  :10;
       uint64_t                  pa  :49; // physical address
   } sxkphys; // physical address accessed through xkphys unmapped virtual address

   struct {
       uint64_t                 mbz  :15;
       uint64_t                is_io : 1; // if set, the address is uncached and resides on MCB bus
       uint64_t                 did  : 8; // the hardware ignores this field when is_io==0, else device ID
       uint64_t                unaddr: 4; // the hardware ignores <39:36> in Octeon I
       uint64_t               offset :36;
   } sphys; // physical address

    struct {
        uint64_t               zeroes :24; // techically, <47:40> are dont-cares
        uint64_t                unaddr: 4; // the hardware ignores <39:36> in Octeon I
        uint64_t               offset :36;
    } smem; // physical mem address

    struct {
        uint64_t                 mem_region  :2;
        uint64_t                 mbz  :13;
        uint64_t                is_io : 1; // 1 in this case
        uint64_t                 did  : 8; // the hardware ignores this field when is_io==0, else device ID
        uint64_t                unaddr: 4; // the hardware ignores <39:36> in Octeon I
        uint64_t               offset :36;
    } sio; // physical IO address

    struct {
        uint64_t                didspace : 24;
        uint64_t                unused   : 40;
    } sfilldidspace;

} octeon_addr_t;


typedef union {
    uint64_t	    word64;
    struct {
        uint32_t    word32hi;
        uint32_t    word32lo;
    } bits;
} octeon_word_t;




/*
 * octeon_build_io_address
 *
 * Builds a memory address for I/O based on the Major 5bits and Sub DID 3bits
 */
static inline uint64_t octeon_build_io_address (uint64_t major_did,
                                                uint64_t sub_did)
{
    return ((0x1ull << 48) | (major_did << 43) | (sub_did << 40));
}

/*
 * octeon_build_mask
 *
 * Builds a bit mask given the required size in bits.
 *
 * @param bits   Number of bits in the mask
 * @return The mask
 */
static inline uint64_t octeon_build_mask (uint64_t bits)
{
    return ~((~0x0ull) << bits);
}

/*
 * octeon_build_bits
 *
 * Perform mask and shift to place the supplied value into
 * the supplied bit rage.
 *
 * Example: octeon_build_bits(39,24,value)
 * <pre>
 * 6       5       4       3       3       2       1
 * 3       5       7       9       1       3       5       7      0
 * +-------+-------+-------+-------+-------+-------+-------+------+
 * 000000000000000000000000___________value000000000000000000000000
 * </pre>
 *
 * @param high_bit Highest bit value can occupy (inclusive) 0-63
 * @param low_bit  Lowest bit value can occupy inclusive 0-high_bit
 * @param value    Value to use
 * @return Value masked and shifted
 */
static inline uint64_t octeon_build_bits (uint64_t high_bit, uint64_t low_bit,
                                          uint64_t value)
{
    return ((value & octeon_build_mask(high_bit - low_bit + 1)) << low_bit);
}


/**********************  simple spinlocks ***************/
typedef struct {
    volatile uint32_t value;
} octeon_spinlock_t;

// note - macros not expanded in inline ASM, so values hardcoded
#define  OCTEON_SPINLOCK_UNLOCKED_VAL  0
#define  OCTEON_SPINLOCK_LOCKED_VAL    1

/**
 * Initialize a spinlock
 *
 * @param lock   Lock to initialize
 */
static inline void octeon_spinlock_init(octeon_spinlock_t *lock)
{
    lock->value = OCTEON_SPINLOCK_UNLOCKED_VAL;
}
/**
 * Releases lock
 *
 * @param lock   pointer to lock structure
 */
static inline void octeon_spinlock_unlock(octeon_spinlock_t *lock)
{
    OCTEON_SYNCWS;

    lock->value = 0;
    OCTEON_SYNCWS;
}

/**
 * Gets lock, spins until lock is taken
 *
 * @param lock   pointer to lock structure
 */
static inline void octeon_spinlock_lock(octeon_spinlock_t *lock)
{
    unsigned int tmp;
    __asm __volatile(
    ".set noreorder         \n"
    "1: ll   %1, %0  \n"
    "   bnez %1, 1b     \n"
    "   li   %1, 1      \n"
    "   sc   %1, %0 \n"
    "   beqz %1, 1b     \n"
    "   nop                \n"
    ".set reorder           \n"
    :  "+m" (lock->value), "=&r" (tmp )
    :
    : "memory");
}

/********************** end simple spinlocks ***************/



/* ------------------------------------------------------------------- *
 *                      octeon_get_chipid()                               *
 * ------------------------------------------------------------------- */
#define OCTEON_CN31XX_CHIP  0x000d0100
#define OCTEON_CN30XX_CHIP  0x000d0200
#define OCTEON_CN3020_CHIP  0x000d0112
#define OCTEON_CN5020_CHIP  0x000d0601

static inline uint32_t octeon_get_chipid(void)
{
    uint32_t id;

    __asm __volatile ("mfc0 %0, $15,0" : "=r" (id));

    return (id);
}


static inline uint32_t octeon_get_except_base_reg (void)
{
    uint32_t tmp;

    __asm volatile (
    "    .set mips64r2            \n"
    "    .set noreorder         \n"
    "    mfc0   %0, $15, 1  \n"
    "    .set reorder           \n"
     :  "=&r" (tmp) : );

    return(tmp);
}




static inline unsigned int get_coremask (void)
{
    return(~(oct_read64(OCTEON_CIU_PP_RST)) & 0xffff);
}


static inline uint32_t octeon_get_core_num (void)
{

    return (0x3FF & octeon_get_except_base_reg());
}


static inline uint64_t octeon_get_cycle(void)
{

/*  ABI == 32 */

    uint32_t tmp_low, tmp_hi;

    __asm __volatile (
               "   .set push                  \n"
               "   .set mips64r2                \n"
               "   .set noreorder               \n"
               "   rdhwr %[tmpl], $31           \n"
               "   dadd  %[tmph], %[tmpl], $0   \n"
               "   dsrl  %[tmph], 32            \n"
               "   dsll  %[tmpl], 32            \n"
               "   dsrl  %[tmpl], 32            \n"
               "   .set pop                 \n"
                  : [tmpl] "=&r" (tmp_low), [tmph] "=&r" (tmp_hi) : );

    return(((uint64_t)tmp_hi << 32) + tmp_low);
}


/**
 * Wait for the specified number of cycle
 *
 * @param cycles
 */
static inline void octeon_wait (uint64_t cycles)
{
    uint64_t done = octeon_get_cycle() + cycles;

    while (octeon_get_cycle() < done)
    {
        /* Spin */
    }
}



/*
 * octeon_machdep.c
 *
 * Direct to Board Support level.
 */
extern void octeon_led_write_char(int char_position, char val);
extern void octeon_led_write_hexchar(int char_position, char hexval);
extern void octeon_led_write_hex(uint32_t wl);
extern void octeon_led_write_string(const char *str);
extern void octeon_reset(void);
extern void octeon_uart_write_byte(int uart_index, uint8_t ch);
extern void octeon_uart_write_string(int uart_index, const char *str);
extern void octeon_uart_write_hex(uint32_t wl);
extern void octeon_uart_write_hex2(uint32_t wl, uint32_t wh);
extern void octeon_wait_uart_flush(int uart_index, uint8_t ch);
extern void octeon_uart_write_byte0(uint8_t ch);
extern void octeon_led_write_char0(char val);
extern void octeon_led_run_wheel(int *pos, int led_position);
extern void octeon_debug_symbol(void);
extern void mips_disable_interrupt_controls(void);
extern uint32_t octeon_cpu_clock;
extern uint64_t octeon_dram;
extern uint32_t octeon_bd_ver, octeon_board_rev_major, octeon_board_rev_minor, octeon_board_type;
extern uint8_t octeon_mac_addr[6];
extern int octeon_core_mask, octeon_mac_addr_count, octeon_chip_rev_major, octeon_chip_rev_minor, octeon_chip_type;
extern void bzero_64(void *str, size_t len);
extern void bzero_32(void *str, size_t len);
extern void bzero_16(void *str, size_t len);
extern void bzero_old(void *str, size_t len);
extern void octeon_ciu_reset(void);
extern void ciu_disable_intr(int core_num, int intx, int enx);
extern void ciu_enable_interrupts (int core_num, int intx, int enx, uint64_t set_these_interrupt_bits, int ciu_ip);
extern void ciu_clear_int_summary(int core_num, int intx, int enx, uint64_t write_bits);
extern uint64_t ciu_get_int_summary(int core_num, int intx, int enx);
extern void octeon_ciu_start_gtimer(int timer, u_int one_shot, uint64_t time_cycles);
extern void octeon_ciu_stop_gtimer(int timer);
extern int octeon_board_real(void);
extern unsigned long octeon_get_clock_rate(void);

typedef union {
    uint64_t word64;
    struct {
        uint64_t reserved             : 27;     /* Not used */
        uint64_t one_shot             : 1;      /* Oneshot ? */
        uint64_t len                  : 36;     /* len of timer in clock cycles - 1 */
    } bits;
} octeon_ciu_gentimer;



#endif	/* LOCORE */


/*
 * R4K Address space definitions
 */
#define ADRSPC_K0BASE    (0x80000000)
#define ADRSPC_K0SIZE    (0x20000000)
#define ADRSPC_K1BASE    (0xA0000000)
#define ADRSPC_K1SIZE    (0x20000000)
#define ADRSPC_KSBASE    (0xC0000000)
#define ADRSPC_KSSIZE    (0x20000000)
#define ADRSPC_K3BASE    (0xE0000000)
#define ADRSPC_K3SIZE    (0x20000000)
#define ADRSPC_KUBASE    (0x00000000)
#define ADRSPC_KUSIZE    (0x80000000)
#define KSEG_MSB_ADDR    0xFFFFFFFF



#define OCTEON_CLOCK_DEFAULT (500 * 1000 * 1000)


/*
 * Octeon Boot Bus BIST Status
 * Mostly used for dummy read to ensure all prev I/Os are write-complete.
 */
#define  OCTEON_MIO_BOOT_BIST_STAT      0x80011800000000F8ull

/*
 * Octeon UART unit
 */
#define  OCTEON_MIO_UART0               0x8001180000000800ull
#define  OCTEON_MIO_UART1               0x8001180000000C00ull
#define  OCTEON_MIO_UART0_THR           0x8001180000000840ull
#define  OCTEON_MIO_UART1_THR           0x8001180000000C40ull
#define  OCTEON_MIO_UART0_LSR           0x8001180000000828ull
#define  OCTEON_MIO_UART1_LSR           0x8001180000000C28ull
#define  OCTEON_MIO_UART0_RBR           0x8001180000000800ull
#define  OCTEON_MIO_UART1_RBR           0x8001180000000C00ull
#define  OCTEON_MIO_UART0_USR           0x8001180000000938ull
#define  OCTEON_MIO_UART1_USR           0x8001180000000D38ull
#define  OCTEON_MIO_ADDR_HI24           0x800118
#define  OCTEON_MIO_UART_SIZE           0x400ull


/*
 * EBT3000 LED Unit
 */
#define  OCTEON_CHAR_LED_BASE_ADDR	(0x1d020000 | (0x1ffffffffull << 31))

#define  OCTEON_FPA_QUEUES		8

/*
 * Octeon FPA I/O Registers
 */
#define  OCTEON_FPA_CTL_STATUS		0x8001180028000050ull
#define  OCTEON_FPA_FPF_SIZE		0x8001180028000058ull
#define  OCTEON_FPA_FPF_MARKS		0x8001180028000000ull
#define  OCTEON_FPA_INT_SUMMARY		0x8001180028000040ull
#define  OCTEON_FPA_INT_ENABLE		0x8001180028000048ull
#define  OCTEON_FPA_QUEUE_AVAILABLE	0x8001180028000098ull
#define  OCTEON_FPA_PAGE_INDEX		0x80011800280000f0ull

/*
 * Octeon PKO Unit
 */
#define	 OCTEON_PKO_REG_FLAGS		0x8001180050000000ull
#define	 OCTEON_PKO_REG_READ_IDX	0x8001180050000008ull
#define	 OCTEON_PKO_CMD_BUF		0x8001180050000010ull
#define	 OCTEON_PKO_GMX_PORT_MODE	0x8001180050000018ull
#define	 OCTEON_PKO_REG_CRC_ENABLE	0x8001180050000020ull
#define	 OCTEON_PKO_QUEUE_MODE		0x8001180050000048ull
#define  OCTEON_PKO_MEM_QUEUE_PTRS     	0x8001180050001000ull
#define	 OCTEON_PKO_MEM_COUNT0		0x8001180050001080ull
#define	 OCTEON_PKO_MEM_COUNT1		0x8001180050001088ull
#define	 OCTEON_PKO_MEM_DEBUG0		0x8001180050001100ull
#define	 OCTEON_PKO_MEM_DEBUG1		0x8001180050001108ull
#define	 OCTEON_PKO_MEM_DEBUG2		0x8001180050001110ull
#define	 OCTEON_PKO_MEM_DEBUG3		0x8001180050001118ull
#define	 OCTEON_PKO_MEM_DEBUG4		0x8001180050001120ull
#define	 OCTEON_PKO_MEM_DEBUG5		0x8001180050001128ull
#define	 OCTEON_PKO_MEM_DEBUG6		0x8001180050001130ull
#define	 OCTEON_PKO_MEM_DEBUG7		0x8001180050001138ull
#define	 OCTEON_PKO_MEM_DEBUG8		0x8001180050001140ull
#define	 OCTEON_PKO_MEM_DEBUG9		0x8001180050001148ull


/*
 * Octeon IPD Unit
 */
#define  OCTEON_IPD_1ST_MBUFF_SKIP		0x80014F0000000000ull
#define  OCTEON_IPD_NOT_1ST_MBUFF_SKIP		0x80014F0000000008ull
#define  OCTEON_IPD_PACKET_MBUFF_SIZE		0x80014F0000000010ull
#define  OCTEON_IPD_1ST_NEXT_PTR_BACK		0x80014F0000000150ull
#define  OCTEON_IPD_2ND_NEXT_PTR_BACK		0x80014F0000000158ull
#define  OCTEON_IPD_WQE_FPA_QUEUE		0x80014F0000000020ull
#define  OCTEON_IPD_CTL_STATUS			0x80014F0000000018ull
#define	 OCTEON_IPD_QOSX_RED_MARKS(queue)      (0x80014F0000000178ull + ((queue) * 8))
#define	 OCTEON_IPD_RED_Q_PARAM(queue)	       (0x80014F00000002E0ull + ((queue) * 8))
#define	 OCTEON_IPD_PORT_BP_PAGE_COUNT(port)   (0x80014F0000000028ull + ((port) * 8))
#define	 OCTEON_IPD_BP_PORT_RED_END		0x80014F0000000328ull
#define	 OCTEON_IPD_RED_PORT_ENABLE		0x80014F00000002D8ull

/*
 * Octeon CIU Unit
 */
#define OCTEON_CIU_ENABLE_BASE_ADDR	0x8001070000000200ull
#define OCTEON_CIU_SUMMARY_BASE_ADDR	0x8001070000000000ull
#define OCTEON_CIU_SUMMARY_INT1_ADDR	0x8001070000000108ull

#define OCTEON_CIU_MBOX_SETX(offset)    (0x8001070000000600ull+((offset)*8))
#define OCTEON_CIU_MBOX_CLRX(offset)    (0x8001070000000680ull+((offset)*8))
#define OCTEON_CIU_ENABLE_MBOX_INTR	 0x0000000300000000ull /* bits 32, 33 */

#define CIU_MIPS_IP2		0
#define CIU_MIPS_IP3		1

#define CIU_INT_0		CIU_MIPS_IP2
#define CIU_INT_1		CIU_MIPS_IP3

#define CIU_EN_0		0
#define CIU_EN_1		1

#define CIU_THIS_CORE		-1

#define CIU_UART_BITS_UART0		      (0x1ull << 34)		// Bit 34
#define CIU_UART_BITS_UART1		      (0x1ull << 35)		// Bit 35
#define CIU_GENTIMER_BITS_ENABLE(timer)	      (0x1ull << (52 + (timer)))	// Bit 52..55

#define CIU_GENTIMER_NUM_0			0
#define CIU_GENTIMER_NUM_1			1
#define CIU_GENTIMER_NUM_2			2
#define CIU_GENTIMER_NUM_3			3
#define OCTEON_GENTIMER_ONESHOT			1
#define OCTEON_GENTIMER_PERIODIC		0

#define OCTEON_CIU_GENTIMER_ADDR(timer)	     (0x8001070000000480ull + ((timer) * 0x8))


#define OCTEON_GENTIMER_LEN_1MS		(0x7a120ull)   /* Back of envelope. 500Mhz Octeon */ // FIXME IF WRONG
#define OCTEON_GENTIMER_LEN_1SEC	((OCTEON_GENTIMER_LEN_1MS) * 1000)

/*
 * Physical Memory Banks
 */
/* 1st BANK */
#define OCTEON_DRAM_FIRST_256_START	0x00000000ull
#define OCTEON_DRAM_FIRST_256_END	(0x10000000ull - 1ull)
#define OCTEON_DRAM_RESERVED_END        0X1FFF000ULL	/* 32 Meg Reserved for Mips Kernel MD Ops */
#define OCTEON_DRAM_FIRST_BANK_SIZE	(OCTEON_DRAM_FIRST_256_END - OCTEON_DRAM_FIRST_256_START + 1)

/* 2nd BANK */
#define OCTEON_DRAM_SECOND_256_START	(0x0000000410000000ull)
#define OCTEON_DRAM_SECOND_256_END	(0x0000000420000000ull - 1ull)  /* Requires 64 bit paddr */
#define OCTEON_DRAM_SECOND_BANK_SIZE	(OCTEON_DRAM_SECOND_256_END - OCTEON_DRAM_SECOND_256_START + 1ull)

/* 3rd BANK */
#define OCTEON_DRAM_ABOVE_512_START	0x20000000ull
#define OCTEON_DRAM_ABOVE_512_END	(0x0000000300000000ull - 1ull)  /* To be calculated as remaining */
#define OCTEON_DRAM_THIRD_BANK_SIZE	(OCTEON_DRAM_ABOVE_512_END - OCTEON_DRAM_ABOVE_512_START + 1ull)

#endif /* !OCTEON_PCMAP_REGS_H__ */
