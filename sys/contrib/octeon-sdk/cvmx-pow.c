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
 * Interface to the hardware Packet Order / Work unit.
 *
 * <hr>$Revision: 29727 $<hr>
 */

#include "cvmx.h"
#include "cvmx-pow.h"

/**
 * @INTERNAL
 * This structure stores the internal POW state captured by
 * cvmx_pow_capture(). It is purposely not exposed to the user
 * since the format may change without notice.
 */
typedef struct
{
    cvmx_pow_tag_load_resp_t sstatus[16][8];
    cvmx_pow_tag_load_resp_t smemload[2048][3];
    cvmx_pow_tag_load_resp_t sindexload[16][4];
} __cvmx_pow_dump_t;

typedef enum
{
    CVMX_POW_LIST_UNKNOWN=0,
    CVMX_POW_LIST_FREE=1,
    CVMX_POW_LIST_INPUT=2,
    CVMX_POW_LIST_CORE=CVMX_POW_LIST_INPUT+8,
    CVMX_POW_LIST_DESCHED=CVMX_POW_LIST_CORE+16,
    CVMX_POW_LIST_NOSCHED=CVMX_POW_LIST_DESCHED+16,
} __cvmx_pow_list_types_t;

static const char *__cvmx_pow_list_names[] = {
    "Unknown",
    "Free List",
    "Queue 0", "Queue 1", "Queue 2", "Queue 3",
    "Queue 4", "Queue 5", "Queue 6", "Queue 7",
    "Core 0", "Core 1", "Core 2", "Core 3",
    "Core 4", "Core 5", "Core 6", "Core 7",
    "Core 8", "Core 9", "Core 10", "Core 11",
    "Core 12", "Core 13", "Core 14", "Core 15",
    "Desched 0", "Desched 1", "Desched 2", "Desched 3",
    "Desched 4", "Desched 5", "Desched 6", "Desched 7",
    "Desched 8", "Desched 9", "Desched 10", "Desched 11",
    "Desched 12", "Desched 13", "Desched 14", "Desched 15",
    "Nosched 0", "Nosched 1", "Nosched 2", "Nosched 3",
    "Nosched 4", "Nosched 5", "Nosched 6", "Nosched 7",
    "Nosched 8", "Nosched 9", "Nosched 10", "Nosched 11",
    "Nosched 12", "Nosched 13", "Nosched 14", "Nosched 15"
};


/**
 * Return the number of POW entries supported by this chip
 *
 * @return Number of POW entries
 */
int cvmx_pow_get_num_entries(void)
{
    if (OCTEON_IS_MODEL(OCTEON_CN30XX))
        return 64;
    else if (OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN50XX))
        return 256;
    else if (OCTEON_IS_MODEL(OCTEON_CN52XX))
        return 512;
    else if (OCTEON_IS_MODEL(OCTEON_CN63XX))
	return 1024;
    else
        return 2048;
}


/**
 * Store the current POW internal state into the supplied
 * buffer. It is recommended that you pass a buffer of at least
 * 128KB. The format of the capture may change based on SDK
 * version and Octeon chip.
 *
 * @param buffer Buffer to store capture into
 * @param buffer_size
 *               The size of the supplied buffer
 *
 * @return Zero on sucess, negative on failure
 */
int cvmx_pow_capture(void *buffer, int buffer_size)
{
    __cvmx_pow_dump_t *dump = (__cvmx_pow_dump_t*)buffer;
    int num_cores;
    int num_pow_entries = cvmx_pow_get_num_entries();
    int core;
    int index;
    int bits;

    if (buffer_size < (int)sizeof(__cvmx_pow_dump_t))
    {
        cvmx_dprintf("cvmx_pow_capture: Buffer too small\n");
        return -1;
    }

    num_cores = cvmx_octeon_num_cores();

    /* Read all core related state */
    for (core=0; core<num_cores; core++)
    {
        cvmx_pow_load_addr_t load_addr;
        load_addr.u64 = 0;
        load_addr.sstatus.mem_region = CVMX_IO_SEG;
        load_addr.sstatus.is_io = 1;
        load_addr.sstatus.did = CVMX_OCT_DID_TAG_TAG1;
        load_addr.sstatus.coreid = core;
        for (bits=0; bits<8; bits++)
        {
            load_addr.sstatus.get_rev = (bits & 1) != 0;
            load_addr.sstatus.get_cur = (bits & 2) != 0;
            load_addr.sstatus.get_wqp = (bits & 4) != 0;
            if ((load_addr.sstatus.get_cur == 0) && load_addr.sstatus.get_rev)
                dump->sstatus[core][bits].u64 = -1;
            else
                dump->sstatus[core][bits].u64 = cvmx_read_csr(load_addr.u64);
        }
    }

    /* Read all internal POW entries */
    for (index=0; index<num_pow_entries; index++)
    {
        cvmx_pow_load_addr_t load_addr;
        load_addr.u64 = 0;
        load_addr.smemload.mem_region = CVMX_IO_SEG;
        load_addr.smemload.is_io = 1;
        load_addr.smemload.did = CVMX_OCT_DID_TAG_TAG2;
        load_addr.smemload.index = index;
        for (bits=0; bits<3; bits++)
        {
            load_addr.smemload.get_des = (bits & 1) != 0;
            load_addr.smemload.get_wqp = (bits & 2) != 0;
            dump->smemload[index][bits].u64 = cvmx_read_csr(load_addr.u64);
        }
    }

    /* Read all group and queue pointers */
    for (index=0; index<16; index++)
    {
        cvmx_pow_load_addr_t load_addr;
        load_addr.u64 = 0;
        load_addr.sindexload.mem_region = CVMX_IO_SEG;
        load_addr.sindexload.is_io = 1;
        load_addr.sindexload.did = CVMX_OCT_DID_TAG_TAG3;
        load_addr.sindexload.qosgrp = index;
        for (bits=0; bits<4; bits++)
        {
            load_addr.sindexload.get_rmt =  (bits & 1) != 0;
            load_addr.sindexload.get_des_get_tail =  (bits & 2) != 0;
            /* The first pass only has 8 valid index values */
            if ((load_addr.sindexload.get_rmt == 0) &&
                (load_addr.sindexload.get_des_get_tail == 0) &&
                (index >= 8))
                dump->sindexload[index][bits].u64 = -1;
            else
                dump->sindexload[index][bits].u64 = cvmx_read_csr(load_addr.u64);
        }
    }
    return 0;
}


/**
 * Function to display a POW internal queue to the user
 *
 * @param name       User visible name for the queue
 * @param name_param Parameter for printf in creating the name
 * @param valid      Set if the queue contains any elements
 * @param has_one    Set if the queue contains exactly one element
 * @param head       The head pointer
 * @param tail       The tail pointer
 */
static void __cvmx_pow_display_list(const char *name, int name_param, int valid, int has_one, uint64_t head, uint64_t tail)
{
    printf(name, name_param);
    printf(": ");
    if (valid)
    {
        if (has_one)
            printf("One element index=%llu(0x%llx)\n", CAST64(head), CAST64(head));
        else
            printf("Multiple elements head=%llu(0x%llx) tail=%llu(0x%llx)\n", CAST64(head), CAST64(head), CAST64(tail), CAST64(tail));
    }
    else
        printf("Empty\n");
}


/**
 * Mark which list a POW entry is on. Print a warning message if the
 * entry is already on a list. This happens if the POW changed while
 * the capture was running.
 *
 * @param entry_num  Entry number to mark
 * @param entry_type List type
 * @param entry_list Array to store marks
 *
 * @return Zero on success, negative if already on a list
 */
static int __cvmx_pow_entry_mark_list(int entry_num, __cvmx_pow_list_types_t entry_type, uint8_t entry_list[])
{
    if (entry_list[entry_num] == 0)
    {
        entry_list[entry_num] = entry_type;
        return 0;
    }
    else
    {
        printf("\nWARNING: Entry %d already on list %s, but we tried to add it to %s\n",
               entry_num, __cvmx_pow_list_names[entry_list[entry_num]], __cvmx_pow_list_names[entry_type]);
        return -1;
    }
}


/**
 * Display a list and mark all elements on the list as belonging to
 * the list.
 *
 * @param entry_type Type of the list to display and mark
 * @param dump       POW capture data
 * @param entry_list Array to store marks in
 * @param valid      Set if the queue contains any elements
 * @param has_one    Set if the queue contains exactly one element
 * @param head       The head pointer
 * @param tail       The tail pointer
 */
static void __cvmx_pow_display_list_and_walk(__cvmx_pow_list_types_t entry_type,
                                             __cvmx_pow_dump_t *dump, uint8_t entry_list[],
                                             int valid, int has_one, uint64_t head, uint64_t tail)
{
    __cvmx_pow_display_list(__cvmx_pow_list_names[entry_type], 0, valid, has_one, head, tail);
    if (valid)
    {
        if (has_one)
            __cvmx_pow_entry_mark_list(head, entry_type, entry_list);
        else
        {
            while (head != tail)
            {
                if (__cvmx_pow_entry_mark_list(head, entry_type, entry_list))
                    break;
                head = dump->smemload[head][0].s_smemload0.next_index;
            }
            __cvmx_pow_entry_mark_list(tail, entry_type, entry_list);
        }
    }
}


/**
 * Dump a POW capture to the console in a human readable format.
 *
 * @param buffer POW capture from cvmx_pow_capture()
 * @param buffer_size
 *               Size of the buffer
 */
void cvmx_pow_display(void *buffer, int buffer_size)
{
    __cvmx_pow_dump_t *dump = (__cvmx_pow_dump_t*)buffer;
    int num_pow_entries = cvmx_pow_get_num_entries();
    int num_cores;
    int core;
    int index;
    uint8_t entry_list[2048];

    if (buffer_size < (int)sizeof(__cvmx_pow_dump_t))
    {
        cvmx_dprintf("cvmx_pow_dump: Buffer too small\n");
        return;
    }

    memset(entry_list, 0, sizeof(entry_list));
    num_cores = cvmx_octeon_num_cores();

    printf("POW Display Start\n");

    /* Print the free list info */
    __cvmx_pow_display_list_and_walk(CVMX_POW_LIST_FREE, dump, entry_list,
                                     dump->sindexload[0][0].sindexload0.free_val,
                                     dump->sindexload[0][0].sindexload0.free_one,
                                     dump->sindexload[0][0].sindexload0.free_head,
                                     dump->sindexload[0][0].sindexload0.free_tail);

    /* Print the core state */
    for (core=0; core<num_cores; core++)
    {
        const int bit_rev = 1;
        const int bit_cur = 2;
        const int bit_wqp = 4;
        printf("Core %d State:  tag=%s,0x%08x", core,
               OCT_TAG_TYPE_STRING(dump->sstatus[core][bit_cur].s_sstatus2.tag_type),
               dump->sstatus[core][bit_cur].s_sstatus2.tag);
        if (dump->sstatus[core][bit_cur].s_sstatus2.tag_type != CVMX_POW_TAG_TYPE_NULL_NULL)
        {
            __cvmx_pow_entry_mark_list(dump->sstatus[core][bit_cur].s_sstatus2.index, CVMX_POW_LIST_CORE + core, entry_list);
            printf(" grp=%d",                   dump->sstatus[core][bit_cur].s_sstatus2.grp);
            printf(" wqp=0x%016llx",            CAST64(dump->sstatus[core][bit_cur|bit_wqp].s_sstatus4.wqp));
            printf(" index=%d",                 dump->sstatus[core][bit_cur].s_sstatus2.index);
            if (dump->sstatus[core][bit_cur].s_sstatus2.head)
                printf(" head");
            else
                printf(" prev=%d", dump->sstatus[core][bit_cur|bit_rev].s_sstatus3.revlink_index);
            if (dump->sstatus[core][bit_cur].s_sstatus2.tail)
                printf(" tail");
            else
                printf(" next=%d", dump->sstatus[core][bit_cur].s_sstatus2.link_index);
        }

        if (dump->sstatus[core][0].s_sstatus0.pend_switch)
        {
            printf(" pend_switch=%d",           dump->sstatus[core][0].s_sstatus0.pend_switch);
            printf(" pend_switch_full=%d",      dump->sstatus[core][0].s_sstatus0.pend_switch_full);
            printf(" pend_switch_null=%d",      dump->sstatus[core][0].s_sstatus0.pend_switch_null);
        }

        if (dump->sstatus[core][0].s_sstatus0.pend_desched)
        {
            printf(" pend_desched=%d",          dump->sstatus[core][0].s_sstatus0.pend_desched);
            printf(" pend_desched_switch=%d",   dump->sstatus[core][0].s_sstatus0.pend_desched_switch);
            printf(" pend_nosched=%d",          dump->sstatus[core][0].s_sstatus0.pend_nosched);
            if (dump->sstatus[core][0].s_sstatus0.pend_desched_switch)
                printf(" pend_grp=%d",              dump->sstatus[core][0].s_sstatus0.pend_grp);
        }

        if (dump->sstatus[core][0].s_sstatus0.pend_new_work)
        {
            if (dump->sstatus[core][0].s_sstatus0.pend_new_work_wait)
                printf(" (Waiting for work)");
            else
                printf(" (Getting work)");
        }
        if (dump->sstatus[core][0].s_sstatus0.pend_null_rd)
            printf(" pend_null_rd=%d",          dump->sstatus[core][0].s_sstatus0.pend_null_rd);
        if (dump->sstatus[core][0].s_sstatus0.pend_nosched_clr)
        {
            printf(" pend_nosched_clr=%d",      dump->sstatus[core][0].s_sstatus0.pend_nosched_clr);
            printf(" pend_index=%d",            dump->sstatus[core][0].s_sstatus0.pend_index);
        }
        if (dump->sstatus[core][0].s_sstatus0.pend_switch ||
            (dump->sstatus[core][0].s_sstatus0.pend_desched &&
            dump->sstatus[core][0].s_sstatus0.pend_desched_switch))
        {
            printf(" pending tag=%s,0x%08x",
                   OCT_TAG_TYPE_STRING(dump->sstatus[core][0].s_sstatus0.pend_type),
                   dump->sstatus[core][0].s_sstatus0.pend_tag);
        }
        if (dump->sstatus[core][0].s_sstatus0.pend_nosched_clr)
            printf(" pend_wqp=0x%016llx\n",     CAST64(dump->sstatus[core][bit_wqp].s_sstatus1.pend_wqp));
        printf("\n");
    }

    /* Print out the state of the nosched list and the 16 deschedule lists. */
    __cvmx_pow_display_list_and_walk(CVMX_POW_LIST_NOSCHED, dump, entry_list,
                            dump->sindexload[0][2].sindexload1.nosched_val,
                            dump->sindexload[0][2].sindexload1.nosched_one,
                            dump->sindexload[0][2].sindexload1.nosched_head,
                            dump->sindexload[0][2].sindexload1.nosched_tail);
    for (index=0; index<16; index++)
    {
        __cvmx_pow_display_list_and_walk(CVMX_POW_LIST_DESCHED + index, dump, entry_list,
                                dump->sindexload[index][2].sindexload1.des_val,
                                dump->sindexload[index][2].sindexload1.des_one,
                                dump->sindexload[index][2].sindexload1.des_head,
                                dump->sindexload[index][2].sindexload1.des_tail);
    }

    /* Print out the state of the 8 internal input queues */
    for (index=0; index<8; index++)
    {
        __cvmx_pow_display_list_and_walk(CVMX_POW_LIST_INPUT + index, dump, entry_list,
                                dump->sindexload[index][0].sindexload0.loc_val,
                                dump->sindexload[index][0].sindexload0.loc_one,
                                dump->sindexload[index][0].sindexload0.loc_head,
                                dump->sindexload[index][0].sindexload0.loc_tail);
    }

    /* Print out the state of the 16 memory queues */
    for (index=0; index<8; index++)
    {
        const char *name;
        if (dump->sindexload[index][1].sindexload2.rmt_is_head)
            name = "Queue %da Memory (is head)";
        else
            name = "Queue %da Memory";
        __cvmx_pow_display_list(name, index,
                                dump->sindexload[index][1].sindexload2.rmt_val,
                                dump->sindexload[index][1].sindexload2.rmt_one,
                                dump->sindexload[index][1].sindexload2.rmt_head,
                                dump->sindexload[index][3].sindexload3.rmt_tail);
        if (dump->sindexload[index+8][1].sindexload2.rmt_is_head)
            name = "Queue %db Memory (is head)";
        else
            name = "Queue %db Memory";
        __cvmx_pow_display_list(name, index,
                                dump->sindexload[index+8][1].sindexload2.rmt_val,
                                dump->sindexload[index+8][1].sindexload2.rmt_one,
                                dump->sindexload[index+8][1].sindexload2.rmt_head,
                                dump->sindexload[index+8][3].sindexload3.rmt_tail);
    }

    /* Print out each of the internal POW entries. Each entry has a tag, group,
        wqe, and possibly a next pointer. The next pointer is only valid if this
        entry isn't make as a tail */
    for (index=0; index<num_pow_entries; index++)
    {
        printf("Entry %d(%-10s): tag=%s,0x%08x grp=%d wqp=0x%016llx", index,
               __cvmx_pow_list_names[entry_list[index]],
               OCT_TAG_TYPE_STRING(dump->smemload[index][0].s_smemload0.tag_type),
               dump->smemload[index][0].s_smemload0.tag,
               dump->smemload[index][0].s_smemload0.grp,
               CAST64(dump->smemload[index][2].s_smemload1.wqp));
        if (dump->smemload[index][0].s_smemload0.tail)
            printf(" tail");
        else
            printf(" next=%d", dump->smemload[index][0].s_smemload0.next_index);
        if (entry_list[index] >= CVMX_POW_LIST_DESCHED)
        {
            printf(" prev=%d", dump->smemload[index][1].s_smemload2.fwd_index);
            printf(" nosched=%d", dump->smemload[index][1].s_smemload2.nosched);
            if (dump->smemload[index][1].s_smemload2.pend_switch)
            {
                printf(" pending tag=%s,0x%08x",
                       OCT_TAG_TYPE_STRING(dump->smemload[index][1].s_smemload2.pend_type),
                       dump->smemload[index][1].s_smemload2.pend_tag);
            }
        }
        printf("\n");
    }

    printf("POW Display End\n");
}

