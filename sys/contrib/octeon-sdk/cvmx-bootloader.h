/***********************license start***************
 *  Copyright (c) 2008 Cavium Networks (support@cavium.com). All rights
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



#ifndef __CVMX_BOOTLOADER__
#define __CVMX_BOOTLOADER__



/**
 * @file
 *
 * Bootloader definitions that are shared with other programs
 *
 * <hr>$Revision: 41586 $<hr>
 */


/* The bootloader_header_t structure defines the header that is present
** at the start of binary u-boot images.  This header is used to locate the bootloader
** image in NAND, and also to allow verification of images for normal NOR booting.
** This structure is placed at the beginning of a bootloader binary image, and remains
** in the executable code.
*/
#define BOOTLOADER_HEADER_MAGIC 0x424f4f54  /* "BOOT" in ASCII */

#define BOOTLOADER_HEADER_COMMENT_LEN  64
#define BOOTLOADER_HEADER_VERSION_LEN  64
#define BOOTLOADER_HEADER_MAX_SIZE      0x200 /* limited by the space to the next exception handler */

#define BOOTLOADER_HEADER_CURRENT_MAJOR_REV 1
#define BOOTLOADER_HEADER_CURRENT_MINOR_REV 1 

/* offsets to struct bootloader_header fields for assembly use */
#define MAGIC_OFFST     8
#define HCRC_OFFST      12
#define HLEN_OFFST      16
#define DLEN_OFFST      24
#define DCRC_OFFST      28
#define GOT_OFFST       48

#define LOOKUP_STEP 8192

#ifndef __ASSEMBLY__
typedef struct bootloader_header
{
    uint32_t    jump_instr; /* Jump to executable code following the
                            ** header.  This allows this header to
                            ** be (and remain) part of the executable image)
                            */
    uint32_t    nop_instr;  /* Must be 0x0 */
    uint32_t    magic; /* Magic number to identify header */
    uint32_t    hcrc;  /* CRC of all of header excluding this field */ 

    uint16_t    hlen;  /* Length of header in bytes */
    uint16_t    maj_rev;  /* Major revision */
    uint16_t    min_rev;  /* Minor revision */
    uint16_t    board_type;  /* Board type that the image is for */

    uint32_t    dlen;  /* Length of data (immediately following header) in bytes */
    uint32_t    dcrc;  /* CRC of data */
    uint64_t    address;  /* Mips virtual address */
    uint32_t    flags;
    uint16_t    image_type;  /* Defined in bootloader_image_t enum */
    uint16_t    resv0;       /* pad */
 
    /* The next 4 fields are placed in compile-time, not by the utility */
    uint32_t    got_address;   /* compiled got address position in the image */
    uint32_t    got_num_entries; /* number of got entries */
    uint32_t    compiled_start;  /* compaled start of the image address */
    uint32_t    image_start;     /* relocated start of image address */

    char        comment_string[BOOTLOADER_HEADER_COMMENT_LEN];  /* Optional, for descriptive purposes */
    char        version_string[BOOTLOADER_HEADER_VERSION_LEN];  /* Optional, for descriptive purposes */
} __attribute__((packed)) bootloader_header_t;



/* Defines for flag field */
#define BL_HEADER_FLAG_FAILSAFE         (1)


typedef enum
{
    BL_HEADER_IMAGE_UKNOWN = 0x0,
    BL_HEADER_IMAGE_STAGE2,  /* Binary bootloader stage2 image (NAND boot) */
    BL_HEADER_IMAGE_STAGE3,  /* Binary bootloader stage3 image (NAND boot)*/
    BL_HEADER_IMAGE_NOR,     /* Binary bootloader for NOR boot */
    BL_HEADER_IMAGE_PCIBOOT,     /* Binary bootloader for PCI boot */
    BL_HEADER_IMAGE_UBOOT_ENV,  /* Environment for u-boot */
    BL_HEADER_IMAGE_MAX,
    /* Range for customer private use.  Will not be used by Cavium Networks */
    BL_HEADER_IMAGE_CUST_RESERVED_MIN = 0x1000,
    BL_HEADER_IMAGE_CUST_RESERVED_MAX = 0x1fff,
} bootloader_image_t;

#endif /* __ASSEMBLY__ */

/* Maximum address searched for NAND boot images and environments.  This is used
** by stage1 and stage2. */
#define MAX_NAND_SEARCH_ADDR   0x400000


/* Defines for RAM based environment set by the host or the previous bootloader
** in a chain boot configuration. */

#define U_BOOT_RAM_ENV_ADDR     (0x1000)
#define U_BOOT_RAM_ENV_SIZE     (0x1000)
#define U_BOOT_RAM_ENV_CRC_SIZE (0x4)

#endif /* __CVMX_BOOTLOADER__ */
