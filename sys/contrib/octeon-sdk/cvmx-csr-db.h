/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Cavium Networks nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/






#ifndef __CVMX_CSR_DB_H__
#define __CVMX_CSR_DB_H__

/**
 * @file
 * Interface for the Octeon CSR database.
 *
 *
 * <hr>$Revision: 49507 $<hr>
 *
 */
#if !defined(CVMX_BUILD_FOR_LINUX_KERNEL) && !defined(CVMX_BUILD_FOR_FREEBSD_KERNEL)
#include "cvmx-platform.h"
#endif

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
   CVMX_CSR_DB_TYPE_RSL,             /**< Octeon internal address, but indirect and slow (not used for addresses) */
   CVMX_CSR_DB_TYPE_NCB,             /**< Octeon internal address */
   CVMX_CSR_DB_TYPE_PCI_NCB,         /**< Can be accessed through PCI BAR0, also an NCB alias (not used for addresses) */
   CVMX_CSR_DB_TYPE_PCICONFIG,       /**< PCI Config, also an NCB alias */
   CVMX_CSR_DB_TYPE_PCI,             /**< PCI BAR0 (only) */
   CVMX_CSR_DB_TYPE_PEXP,            /**< PCIe BAR 0 address only */
   CVMX_CSR_DB_TYPE_PEXP_NCB,        /**< NCB-direct and PCIe BAR0 address */
   CVMX_CSR_DB_TYPE_PCICONFIGEP,     /**< PCIe config address (EP mode) + indirect through PESC*_CFG_RD/PESC*_CFG_WR */
   CVMX_CSR_DB_TYPE_PCICONFIGRC,     /**< PCICONFIGRC - PCIe config address (RC mode) + indirect through PESC*_CFG_RD/PESC*_CFG_WR */
   CVMX_CSR_DB_TYPE_SRIOMAINT        /**< SRIOMAINT - SRIO maintenance registers */
} CVMX_CSR_DB_TYPE_FIELD;

/**
 * the structure for the cvmx_csr_db_addresses[] array that
 * holds all possible Octeon CSR addresses
 */
typedef struct {
   char *   name;                   /**< CSR name at the supplied address */
   uint64_t address;                /**< Address = octeon internal, PCI BAR0 relative, PCI CONFIG relative */
   CVMX_CSR_DB_TYPE_FIELD type:8;   /**< the type */
   uint8_t  widthbits;              /**< the width of the CSR in bits */
   uint16_t csroff;                 /**< position of the CSR in cvmx_csr_db[] */
} __attribute__ ((packed)) CVMX_CSR_DB_ADDRESS_TYPE;

/**
 * the structure for the cvmx_csr_db_fields[] array that
 * holds all possible Octeon CSR fields
 */
typedef struct {
   char *   name;                   /**< name of the field */
   uint8_t  startbit;               /**< starting bit position of the field */
   uint8_t  sizebits;               /**< the size of the field in bits */
   uint16_t csroff;                 /**< position of the CSR containing the field in cvmx_csr_db[] (get alias from there) */
   char *   type;                   /**< the type of the field R/W, R/W1C, ... */
   uint8_t  rst_unp;                /**< set if the reset value is unknown */
   uint8_t  typ_unp;                /**< set if the typical value is unknown */
   uint64_t rst_val;                /**< the reset value of the field */
   uint64_t typ_val;                /**< the typical value of the field */
} __attribute__ ((packed)) CVMX_CSR_DB_FIELD_TYPE;

/**
 * the structure for the cvmx_csr_db[] array that holds all
 * possible Octeon CSR forms
 */
typedef struct {
   char *basename;                  /**< the base name of the CSR */
   CVMX_CSR_DB_TYPE_FIELD type:8;   /**< the type */
   uint8_t  widthbits;              /**< the width of the CSR in bits */
   uint16_t addoff;                 /**< the position of the first address in cvmx_csr_db_csr_addresses[] (numblocks*indexnum is #) */
   uint8_t  numfields;              /**< the number of fields in the CSR (and in cvmx_csr_db_csr_fields[]) */
   uint16_t fieldoff;               /**< the position of the first field in cvmx_csr_db_csr_fields[] */
} __attribute__ ((packed)) CVMX_CSR_DB_TYPE;


/**
 * This NULL terminated array contains the CVMX_CSR_DB_TYPE
 * arrays for each chip. Each array entry is another NULL
 * terminated array of CSRs.
 */
extern const CVMX_CSR_DB_TYPE *cvmx_csr_db[];

/**
 * This NULL terminated array contains the CVMX_CSR_DB_ADDRESS_TYPE
 * arrays for each chip. Each array entry is another NULL
 * terminated array of CSR addresses.
 */
extern const CVMX_CSR_DB_ADDRESS_TYPE *cvmx_csr_db_addresses[];

/**
 * This NULL terminated array contains the CVMX_CSR_DB_FIELD_TYPE
 * arrays for each chip. Each array entry is another NULL
 * terminated array of CSR fields.
 */
extern const CVMX_CSR_DB_FIELD_TYPE *cvmx_csr_db_fields[];

/**
 * Figure out which database to use for this chip. The passed
 * identifier can be a processor ID or a PCI ID.
 *
 * @param identifier processor ID or a PCI ID
 *
 * @return index into the csr db
 */
extern int cvmx_db_get_chipindex(int identifier);

/**
 * Get the CSR DB entry for the passed Octeon model and CSR name. The
 * model can either be specified as a processor id or PCI id.
 *
 * @param identifier Identifer to choose the CSR DB with
 * @param name       CSR name to lookup
 *
 * @return CSR DB entry or NULL on failure
 */
extern const CVMX_CSR_DB_ADDRESS_TYPE *cvmx_csr_db_get(int identifier, const char *name);

/**
 * Decode a CSR value into named bitfields. The model can either
 * be specified as a processor id or PCI id.
 *
 * @param identifier Identifer to choose the CSR DB with
 * @param address    CSR address being decoded
 * @param value      Value to decode
 */
extern void cvmx_csr_db_decode(int identifier, uint64_t address, uint64_t value);

/**
 * Decode a CSR value into named bitfields. The model can either
 * be specified as a processor id or PCI id.
 *
 * @param identifier Identifer to choose the CSR DB with
 * @param name       CSR name to decode
 * @param value      Value to decode
 */
extern void cvmx_csr_db_decode_by_name(int identifier, const char *name, uint64_t value);

/**
 * Print a list of csrs begimning with a prefix. The
 * model can either be specified as a processor id or PCI id.
 *
 * @param identifier Identifer to choose the CSR DB with
 * @param prefix     Beginning prefix to look for
 */
extern void cvmx_csr_db_display_list(int identifier, const char *prefix);

#ifdef  __cplusplus
}
#endif

#endif
