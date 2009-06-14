/*------------------------------------------------------------------
 * octeon_fau.h        Fetch & Add Unit
 *
 *------------------------------------------------------------------
 */


#ifndef ___OCTEON_FAU__H___
#define ___OCTEON_FAU__H___




typedef enum {
   OCTEON_FAU_OP_SIZE_8  = 0,
   OCTEON_FAU_OP_SIZE_16 = 1,
   OCTEON_FAU_OP_SIZE_32 = 2,
   OCTEON_FAU_OP_SIZE_64 = 3
} octeon_fau_op_size_t;



#define OCTEON_FAU_LOAD_IO_ADDRESS    octeon_build_io_address(0x1e, 0)
#define OCTEON_FAU_BITS_SCRADDR       63,56
#define OCTEON_FAU_BITS_LEN           55,48
#define OCTEON_FAU_BITS_INEVAL        35,14
#define OCTEON_FAU_BITS_TAGWAIT       13,13
#define OCTEON_FAU_BITS_NOADD         13,13
#define OCTEON_FAU_BITS_SIZE          12,11
#define OCTEON_FAU_BITS_REGISTER      10,0

#define OCTEON_FAU_REG_64_ADDR(x) ((x <<3) + OCTEON_FAU_REG_64_START)
typedef enum
{
    OCTEON_FAU_REG_64_START		= 0, 
    OCTEON_FAU_REG_OQ_ADDR_INDEX 	= OCTEON_FAU_REG_64_ADDR(0),
    OCTEON_FAU_REG_OQ_ADDR_END 		= OCTEON_FAU_REG_64_ADDR(31),
    OCTEON_FAU_REG_64_END		= OCTEON_FAU_REG_64_ADDR(39),
} octeon_fau_reg_64_t;

#define OCTEON_FAU_REG_32_ADDR(x) ((x <<2) + OCTEON_FAU_REG_32_START)
typedef enum
{
    OCTEON_FAU_REG_32_START          = OCTEON_FAU_REG_64_END,
    OCTEON_FAU_REG_32_END            = OCTEON_FAU_REG_32_ADDR(0),
} octeon_fau_reg_32_t;



/*
 * octeon_fau_atomic_address
 *
 * Builds a I/O address for accessing the FAU
 *
 * @param tagwait Should the atomic add wait for the current tag switch
 *                operation to complete.
 *                - 0 = Don't wait
 *                - 1 = Wait for tag switch to complete
 * @param reg     FAU atomic register to access. 0 <= reg < 4096.
 *                - Step by 2 for 16 bit access.
 *                - Step by 4 for 32 bit access.
 *                - Step by 8 for 64 bit access.
 * @param value   Signed value to add.
 *                Note: When performing 32 and 64 bit access, only the low
 *                22 bits are available.
 * @return Address to read from for atomic update
 */
static inline uint64_t octeon_fau_atomic_address (uint64_t tagwait, uint64_t reg,
                                               int64_t value)
{
    return (OCTEON_ADD_IO_SEG(OCTEON_FAU_LOAD_IO_ADDRESS) |
            octeon_build_bits(OCTEON_FAU_BITS_INEVAL, value) |
            octeon_build_bits(OCTEON_FAU_BITS_TAGWAIT, tagwait) |
            octeon_build_bits(OCTEON_FAU_BITS_REGISTER, reg));
}


/*
 * octeon_fau_store_address
 *
 * Builds a store I/O address for writing to the FAU
 *
 * noadd  0 = Store value is atomically added to the current value
 *               1 = Store value is atomically written over the current value
 * reg    FAU atomic register to access. 0 <= reg < 4096.
 *               - Step by 2 for 16 bit access.
 *               - Step by 4 for 32 bit access.
 *               - Step by 8 for 64 bit access.
 * Returns Address to store for atomic update
 */
static inline uint64_t octeon_fau_store_address (uint64_t noadd, uint64_t reg)
{
    return (OCTEON_ADD_IO_SEG(OCTEON_FAU_LOAD_IO_ADDRESS) |
            octeon_build_bits(OCTEON_FAU_BITS_NOADD, noadd) |
            octeon_build_bits(OCTEON_FAU_BITS_REGISTER, reg));
}


/*
 * octeon_fau_atomic_add32
 *
 * Perform an atomic 32 bit add
 *
 * @param reg     FAU atomic register to access. 0 <= reg < 4096.
 *                - Step by 4 for 32 bit access.
 * @param value   Signed value to add.
 */
static inline void octeon_fau_atomic_add32 (octeon_fau_reg_32_t reg, int32_t value)
{
    oct_write32(octeon_fau_store_address(0, reg), value);
}

/*
 * octeon_fau_fetch_and_add
 *
 * reg     FAU atomic register to access. 0 <= reg < 4096.
 *                - Step by 8 for 64 bit access.
 * value   Signed value to add.
 *                Note: Only the low 22 bits are available.
 * returns Value of the register before the update
 */
static inline int64_t octeon_fau_fetch_and_add64 (octeon_fau_reg_64_t reg,
                                               int64_t  val64)
{

    return (oct_read64(octeon_fau_atomic_address(0, reg, val64)));
}

/*
 * octeon_fau_fetch_and_add32
 *
 * reg     FAU atomic register to access. 0 <= reg < 4096.
 *                - Step by 8 for 64 bit access.
 * value   Signed value to add.
 *                Note: Only the low 22 bits are available.
 * returns Value of the register before the update
 */
static inline int32_t octeon_fau_fetch_and_add32 (octeon_fau_reg_64_t reg,
                                               int32_t val32)
{
    return (oct_read32(octeon_fau_atomic_address(0, reg, val32)));
}

/*
 * octeon_fau_atomic_write32
 *
 * Perform an atomic 32 bit write
 *
 * @param reg     FAU atomic register to access. 0 <= reg < 4096.
 *                - Step by 4 for 32 bit access.
 * @param value   Signed value to write.
 */
static inline void octeon_fau_atomic_write32(octeon_fau_reg_32_t reg, int32_t value)
{
    oct_write32(octeon_fau_store_address(1, reg), value);
}


/*
 * octeon_fau_atomic_write64
 *
 * Perform an atomic 32 bit write
 *
 * reg    FAU atomic register to access. 0 <= reg < 4096.
 *                - Step by 8 for 64 bit access.
 * value   Signed value to write.
 */
static inline void octeon_fau_atomic_write64 (octeon_fau_reg_64_t reg, int64_t value)
{
    oct_write64(octeon_fau_store_address(1, reg), value);
}


static inline void octeon_fau_atomic_add64 (octeon_fau_reg_64_t reg, int64_t value)
{
    oct_write64_int64(octeon_fau_store_address(0, reg), value);
}


extern void octeon_fau_init(void);
extern void octeon_fau_enable(void);
extern void octeon_fau_disable(void);


#endif  /* ___OCTEON_FAU__H___ */
