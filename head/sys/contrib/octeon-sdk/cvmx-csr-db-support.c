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







/**
 * @file
 *
 * Utility functions for working with the CSR database
 *
 * <hr>$Revision: 49507 $<hr>
 */
#ifdef CVMX_BUILD_FOR_LINUX_KERNEL
#define PRINTF printk
#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-csr-db.h>
#else
#define PRINTF printf
#include "cvmx.h"
#include "cvmx-csr-db.h"
#endif

/**
 * Figure out which database to use for this chip. The passed
 * identifier can be a processor ID or a PCI ID.
 *
 * @param identifier processor ID or a PCI ID
 *
 * @return index into the csr db
 */
int cvmx_db_get_chipindex(int identifier)
{
    /* First try and see if the identifier is a Processor ID */
    switch (identifier & 0xffff00)
    {
        case 0x000d0600: /* CN50XX */
            return 8;
        case 0x000d0400: /* CN56XX */
            return 7;
        case 0x000d0300: /* CN58XX */
            return 5;
        case 0x000d0000: /* CN38XX */
            return 3;
        case 0x000d0100: /* CN31XX */
            return 1;
        case 0x000d0200: /* CN3010 */
            return 2;
        case 0x000d0700: /* CN52XX */
            return 10;
        case 0x000d9000: /* CN63XX */
            return 11;
    }

    /* Next try PCI device IDs */
    switch (identifier)
    {
        case 0x0003177d: /* CN38XX Pass 1 */
            return 0;
        case 0x0004177d: /* CN38XX Pass 2 */
            return 0;
        case 0x0005177d: /* CN38XX Pass 3 */
            return 3;
        case 0x1001177d: /* Thunder */
            return 3;
        case 0x0020177d: /* CN31XX Pass 1 */
            return 1;
        case 0x0030177d: /* CN30XX Pass 1 */
            return 2;
        case 0x0040177d: /* CN58XX Pass 2 */
            return 5;
        case 0x0050177d: /* CN56XX Pass 2 */
            return 7;
        case 0x0070177d: /* CN50XX Pass 1 */
            return 8;
        case 0x0080177d: /* CN52XX Pass 2 */
            return 10;
        case 0x0090177d: /* CN63XX Pass 1 */
            return 11;
    }

    /* Default to Pass 3 if we don't know */
    return 3;
}


#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
/**
 * Get the CSR DB entry for the passed Octeon model and CSR name. The
 * model can either be specified as a processor id or PCI id.
 *
 * @param identifier Identifer to choose the CSR DB with
 * @param name       CSR name to lookup
 *
 * @return CSR DB entry or NULL on failure
 */
const CVMX_CSR_DB_ADDRESS_TYPE *cvmx_csr_db_get(int identifier, const char *name)
{
    int chip = cvmx_db_get_chipindex(identifier);
    int i=0;
    if (strncasecmp(name, "CVMX_", 5) == 0)
        name += 5;
    while (cvmx_csr_db_addresses[chip][i].name)
    {
        if (strcasecmp(name, cvmx_csr_db_addresses[chip][i].name) == 0)
            return &(cvmx_csr_db_addresses[chip][i]);
        i++;
    }
    return NULL;
}
#endif

static void __cvmx_csr_db_decode_csr(int chip, int index, uint64_t value)
{
    int field;
    int csr = cvmx_csr_db_addresses[chip][index].csroff;
    PRINTF("%s(0x%016llx) = 0x%016llx\n", cvmx_csr_db_addresses[chip][index].name, (unsigned long long)cvmx_csr_db_addresses[chip][index].address, (unsigned long long)value);
    for (field=cvmx_csr_db[chip][csr].fieldoff+cvmx_csr_db[chip][csr].numfields-1; field>=cvmx_csr_db[chip][csr].fieldoff; field--)
    {
        uint64_t v = (value >> cvmx_csr_db_fields[chip][field].startbit);
        if(cvmx_csr_db_fields[chip][field].sizebits < 64)
            v = v & ~((~0x0ull) << cvmx_csr_db_fields[chip][field].sizebits);
        if (cvmx_csr_db_fields[chip][field].sizebits == 1)
            PRINTF("  [   %2d] %-20s = %10llu (0x%llx)\n",
                cvmx_csr_db_fields[chip][field].startbit, cvmx_csr_db_fields[chip][field].name,
                (unsigned long long)v, (unsigned long long)v);
        else
            PRINTF("  [%2d:%2d] %-20s = %10llu (0x%llx)\n",
                cvmx_csr_db_fields[chip][field].startbit + cvmx_csr_db_fields[chip][field].sizebits - 1,
                cvmx_csr_db_fields[chip][field].startbit,
                cvmx_csr_db_fields[chip][field].name,
                (unsigned long long)v, (unsigned long long)v);
    }
}

/**
 * Decode a CSR value into named bitfields. The model can either
 * be specified as a processor id or PCI id.
 *
 * @param identifier Identifer to choose the CSR DB with
 * @param address    CSR address being decoded
 * @param value      Value to decode
 */
void cvmx_csr_db_decode(int identifier, uint64_t address, uint64_t value)
{
    int chip = cvmx_db_get_chipindex(identifier);
    int index=0;
    /* Strip off the upper 8 bits since they are normally mips addressing
        modes */
    address &= (1ull<<56)-1;
    while (cvmx_csr_db_addresses[chip][index].name)
    {
        if (cvmx_csr_db_addresses[chip][index].address == address)
            __cvmx_csr_db_decode_csr(chip, index, value);
        index++;
    }
}

/**
 * Decode a CSR value into named bitfields. The model can either
 * be specified as a processor id or PCI id.
 *
 * @param identifier Identifer to choose the CSR DB with
 * @param name       CSR name to decode
 * @param value      Value to decode
 */
void cvmx_csr_db_decode_by_name(int identifier, const char *name, uint64_t value)
{
    int chip = cvmx_db_get_chipindex(identifier);
    int index=0;
    while (cvmx_csr_db_addresses[chip][index].name)
    {
        if (strcasecmp(name, cvmx_csr_db_addresses[chip][index].name) == 0)
        {
            __cvmx_csr_db_decode_csr(chip, index, value);
            break;
        }
        index++;
    }
}


#ifndef CVMX_BUILD_FOR_LINUX_KERNEL
/**
 * Print a list of csrs begimning with a prefix. The
 * model can either be specified as a processor id or PCI id.
 *
 * @param identifier Identifer to choose the CSR DB with
 * @param prefix     Beginning prefix to look for
 */
void cvmx_csr_db_display_list(int identifier, const char *prefix)
{
    int i, len;
    int chip = cvmx_db_get_chipindex(identifier);
    if (prefix == NULL)
        prefix = "";
    if (strncasecmp(prefix, "CVMX_", 5) == 0)
        prefix += 5;
    len = strlen(prefix);

    i=0;
    while (cvmx_csr_db_addresses[chip][i].name)
    {
        if (strncasecmp(prefix, cvmx_csr_db_addresses[chip][i].name, len) == 0)
            PRINTF("%s\n", cvmx_csr_db_addresses[chip][i].name);
        i++;
    }
}
#endif
