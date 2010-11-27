/***********************license start***************
 *  Copyright (c) 2003-2008 Cavium Networks (support@cavium.com). All rights
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





/**
 * @file
 *
 * Interface to the Trace buffer hardware.
 *
 * <hr>$Revision: 30644 $<hr>
 */
#include "cvmx.h"
#include "cvmx-tra.h"

static const char *TYPE_ARRAY[] = {
    "DWB - Don't write back",
    "PL2 - Prefetch into L2",
    "PSL1 - Dcache fill, skip L2",
    "LDD - Dcache fill",
    "LDI - Icache/IO fill",
    "LDT - Icache/IO fill, skip L2",
    "STF - Store full",
    "STC - Store conditional",
    "STP - Store partial",
    "STT - Store full, skip L2",
    "IOBLD8 - IOB 8bit load",
    "IOBLD16 - IOB 16bit load",
    "IOBLD32 - IOB 32bit load",
    "IOBLD64 - IOB 64bit load",
    "IOBST - IOB store",
    "IOBDMA - Async IOB",
    "SAA - Store atomic add",
    "RSVD17",
    "RSVD18",
    "RSVD19",
    "RSVD20",
    "RSVD21",
    "RSVD22",
    "RSVD23",
    "RSVD24",
    "RSVD25",
    "RSVD26",
    "RSVD27",
    "RSVD28",
    "RSVD29",
    "RSVD30",
    "RSVD31"
};

static const char *SOURCE_ARRAY[] = {
    "PP0",
    "PP1",
    "PP2",
    "PP3",
    "PP4",
    "PP5",
    "PP6",
    "PP7",
    "PP8",
    "PP9",
    "PP10",
    "PP11",
    "PP12",
    "PP13",
    "PP14",
    "PP15",
    "PIP/IPD",
    "PKO-R",
    "FPA/TIM/DFA/PCI/ZIP/POW/PKO-W",
    "DWB",
    "RSVD20",
    "RSVD21",
    "RSVD22",
    "RSVD23",
    "RSVD24",
    "RSVD25",
    "RSVD26",
    "RSVD27",
    "RSVD28",
    "RSVD29",
    "RSVD30",
    "RSVD31"
};

static const char *DEST_ARRAY[] = {
    "CIU/GPIO",
    "RSVD1",
    "RSVD2",
    "PCI/PCIe",
    "KEY",
    "FPA",
    "DFA",
    "ZIP",
    "RNG",
    "IPD",
    "PKO",
    "RSVD11",
    "POW",
    "RSVD13",
    "RSVD14",
    "RSVD15",
    "RSVD16",
    "RSVD17",
    "RSVD18",
    "RSVD19",
    "RSVD20",
    "RSVD21",
    "RSVD22",
    "RSVD23",
    "RSVD24",
    "RSVD25",
    "RSVD26",
    "RSVD27",
    "RSVD28",
    "RSVD29",
    "RSVD30",
    "RSVD31"
};

/**
 * Setup the TRA buffer for use
 *
 * @param control TRA control setup
 * @param filter  Which events to log
 * @param source_filter
 *                Source match
 * @param dest_filter
 *                Destination match
 * @param address Address compare
 * @param address_mask
 *                Address mask
 */
void cvmx_tra_setup(cvmx_tra_ctl_t control, cvmx_tra_filt_cmd_t filter,
                    cvmx_tra_filt_sid_t source_filter, cvmx_tra_filt_did_t dest_filter,
                    uint64_t address, uint64_t address_mask)
{
    cvmx_write_csr(CVMX_TRA_CTL,            control.u64);
    cvmx_write_csr(CVMX_TRA_FILT_CMD,       filter.u64);
    cvmx_write_csr(CVMX_TRA_FILT_SID,       source_filter.u64);
    cvmx_write_csr(CVMX_TRA_FILT_DID,       dest_filter.u64);
    cvmx_write_csr(CVMX_TRA_FILT_ADR_ADR,   address);
    cvmx_write_csr(CVMX_TRA_FILT_ADR_MSK,   address_mask);
}


/**
 * Setup a TRA trigger. How the triggers are used should be
 * setup using cvmx_tra_setup.
 *
 * @param trigger Trigger to setup (0 or 1)
 * @param filter  Which types of events to trigger on
 * @param source_filter
 *                Source trigger match
 * @param dest_filter
 *                Destination trigger match
 * @param address Trigger address compare
 * @param address_mask
 *                Trigger address mask
 */
void cvmx_tra_trig_setup(uint64_t trigger, cvmx_tra_filt_cmd_t filter,
                         cvmx_tra_filt_sid_t source_filter, cvmx_tra_trig0_did_t dest_filter,
                         uint64_t address, uint64_t address_mask)
{
    cvmx_write_csr(CVMX_TRA_TRIG0_CMD + trigger * 64,       filter.u64);
    cvmx_write_csr(CVMX_TRA_TRIG0_SID + trigger * 64,       source_filter.u64);
    cvmx_write_csr(CVMX_TRA_TRIG0_DID + trigger * 64,       dest_filter.u64);
    cvmx_write_csr(CVMX_TRA_TRIG0_ADR_ADR + trigger * 64,   address);
    cvmx_write_csr(CVMX_TRA_TRIG0_ADR_MSK + trigger * 64,   address_mask);
}


/**
 * Read an entry from the TRA buffer
 *
 * @return Value return. High bit will be zero if there wasn't any data
 */
cvmx_tra_data_t cvmx_tra_read(void)
{
    cvmx_tra_data_t result;
    result.u64 = cvmx_read_csr(CVMX_TRA_READ_DAT);
    return result;
}


/**
 * Decode a TRA entry into human readable output
 *
 * @param tra_ctl Trace control setup
 * @param data    Data to decode
 */
void cvmx_tra_decode_text(cvmx_tra_ctl_t tra_ctl, cvmx_tra_data_t data)
{
    /* The type is a five bit field for some entries and 4 for other. The four
        bit entries can be mis-typed if the top is set */
    int type = data.cmn.type;
    if (type >= 0x1a)
        type &= 0xf;
    switch (type)
    {
        case CVMX_TRA_DATA_DWB:
        case CVMX_TRA_DATA_PL2:
        case CVMX_TRA_DATA_PSL1:
        case CVMX_TRA_DATA_LDD:
        case CVMX_TRA_DATA_LDI:
        case CVMX_TRA_DATA_LDT:
            cvmx_dprintf("0x%016llx %c%+10d %s %s 0x%016llx\n",
                   (unsigned long long)data.u64,
                   (data.cmn.discontinuity) ? 'D' : ' ',
                   data.cmn.timestamp << (tra_ctl.s.time_grn*3),
                   TYPE_ARRAY[type],
                   SOURCE_ARRAY[data.cmn.source],
                   (unsigned long long)data.cmn.address);
            break;
        case CVMX_TRA_DATA_STC:
        case CVMX_TRA_DATA_STF:
        case CVMX_TRA_DATA_STP:
        case CVMX_TRA_DATA_STT:
        case CVMX_TRA_DATA_SAA:
            cvmx_dprintf("0x%016llx %c%+10d %s %s mask=0x%02x 0x%016llx\n",
                   (unsigned long long)data.u64,
                   (data.cmn.discontinuity) ? 'D' : ' ',
                   data.cmn.timestamp << (tra_ctl.s.time_grn*3),
                   TYPE_ARRAY[type],
                   SOURCE_ARRAY[data.store.source],
                   (unsigned int)data.store.mask,
                   (unsigned long long)data.store.address << 3);
            break;
        case CVMX_TRA_DATA_IOBLD8:
        case CVMX_TRA_DATA_IOBLD16:
        case CVMX_TRA_DATA_IOBLD32:
        case CVMX_TRA_DATA_IOBLD64:
        case CVMX_TRA_DATA_IOBST:
            cvmx_dprintf("0x%016llx %c%+10d %s %s->%s subdid=0x%x 0x%016llx\n",
                   (unsigned long long)data.u64,
                   (data.cmn.discontinuity) ? 'D' : ' ',
                   data.cmn.timestamp << (tra_ctl.s.time_grn*3),
                   TYPE_ARRAY[type],
                   SOURCE_ARRAY[data.iobld.source],
                   DEST_ARRAY[data.iobld.dest],
                   (unsigned int)data.iobld.subid,
                   (unsigned long long)data.iobld.address);
            break;
        case CVMX_TRA_DATA_IOBDMA:
            cvmx_dprintf("0x%016llx %c%+10d %s %s->%s len=0x%x 0x%016llx\n",
                   (unsigned long long)data.u64,
                   (data.cmn.discontinuity) ? 'D' : ' ',
                   data.cmn.timestamp << (tra_ctl.s.time_grn*3),
                   TYPE_ARRAY[type],
                   SOURCE_ARRAY[data.iob.source],
                   DEST_ARRAY[data.iob.dest],
                   (unsigned int)data.iob.mask,
                   (unsigned long long)data.iob.address << 3);
            break;
        default:
            cvmx_dprintf("0x%016llx %c%+10d Unknown format\n",
                   (unsigned long long)data.u64,
                   (data.cmn.discontinuity) ? 'D' : ' ',
                   data.cmn.timestamp << (tra_ctl.s.time_grn*3));
            break;
    }
}


/**
 * Display the entire trace buffer. It is advised that you
 * disable the trace buffer before calling this routine
 * otherwise it could infinitely loop displaying trace data
 * that it created.
 */
void cvmx_tra_display(void)
{
    cvmx_tra_ctl_t tra_ctl;
    cvmx_tra_data_t data;

    tra_ctl.u64 = cvmx_read_csr(CVMX_TRA_CTL);

    do
    {
        data = cvmx_tra_read();
        if (data.cmn.valid)
            cvmx_tra_decode_text(tra_ctl, data);
    } while (data.cmn.valid);
}

