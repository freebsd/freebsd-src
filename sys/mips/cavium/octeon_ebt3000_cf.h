/*
 *  octeon_ebt3000_cf.h
 *
 */


#ifndef  __OCTEON_EBT3000_H__
#define  __OCTEON_EBT3000_H__



#define OCTEON_CF_COMMON_BASE_ADDR		(0x1d000000 | (1 << 11))
#define OCTEON_MIO_BOOT_REG_CFGX(offset)	(0x8001180000000000ull + ((offset) * 8))


typedef union
{   
    uint64_t	word64;
    struct
    {
        uint64_t reserved                : 27;      /**< Reserved */
        uint64_t sam                     : 1;       /**< Region 0 SAM */
        uint64_t we_ext                  : 2;       /**< Region 0 write enable count extension */
        uint64_t oe_ext                  : 2;       /**< Region 0 output enable count extension */
        uint64_t en                      : 1;       /**< Region 0 enable */
        uint64_t orbit                   : 1;       /**< No function for region 0 */
        uint64_t ale                     : 1;       /**< Region 0 ALE mode */
        uint64_t width                   : 1;       /**< Region 0 bus width */
        uint64_t size                    : 12;      /**< Region 0 size */
        uint64_t base                    : 16;      /**< Region 0 base address */
    } bits;
} octeon_mio_boot_reg_cfgx_t;


#endif  /* __OCTEON_EBT3000_H__ */
