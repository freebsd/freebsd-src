/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/boot/arc/lib/arch/alpha/rpb.c,v 1.2 1999/08/28 00:39:40 peter Exp $
 */

#include <stand.h>
#include <machine/rpb.h>
#include "arctypes.h"
#include "arcfuncs.h"

struct rpb RPB = {
    0,				/* rpb_phys */
    {"HWRPB"},			/* rpb_magic */
    HWRPB_DSRDB_MINVERS,	/* rpb_version */
    sizeof(struct rpb),		/* rpb_size */
    0,				/* rpb_primary_cpu_id */
    8192,			/* rpb_page_size */
    43,				/* rpb_phys_addr_size */
    0,				/* rpb_max_asn */
    {0},			/* rpb_ssn */
    ST_EB164,			/* rpb_type */
    SV_ST_ALPHAPC164LX_533,	/* rpb_variation */
    {"0000"},			/* rpb_revision */
    1024*4096,			/* rpb_intr_freq */
    533*1024*1024,		/* rpb_cc_freq */
    0,				/* rpb_vptb */
    0,				/* rpb_reserved_arch */
    0,				/* rpb_tbhint_off */
    0,				/* rpb_pcs_cnt */
    0,				/* rpb_pcs_size */
    0,				/* rpb_pcs_off */
    0,				/* rpb_ctb_cnt */
    0,				/* rpb_ctb_size */
    0,				/* rpb_ctb_off */
    0,				/* rpb_crb_off */
    0,				/* rpb_memdat_off */
    0,				/* rpb_condat_off */
    0,				/* rpb_fru_off */
    0,				/* rpb_save_term */
    0,				/* rpb_save_term_val */
    0,				/* rpb_rest_term */
    0,				/* rpb_rest_term_val */
    0,				/* rpb_restart */
    0,				/* rpb_restart_val */
    0,				/* rpb_reserve_os */
    0,				/* rpb_reserve_hw */
    0,				/* rpb_checksum */
    0,				/* rpb_rxrdy */
    0,				/* rpb_txrdy */
    0,				/* rpb_dsrdb_off */
    {0},			/* rpb_rpb_tbhint */
};

#define ROUNDUP(x)	(((x) + sizeof(u_int64_t) - 1) \
			 & ~(sizeof(u_int64_t) - 1))

u_int64_t
checksum(void *p, size_t size)
{
    u_int64_t sum = 0;
    u_int64_t *lp = (u_int64_t *)p;
    int i;

    printf("checksum(%p, %d)\n", p, size);
    size = ROUNDUP(size) / sizeof(u_int64_t);
    for (i = 0; i < size; i++)
	sum += lp[i];
}

size_t
size_mddt()
{
    int count = 0;
    MEMORY_DESCRIPTOR *desc;

    for (desc = GetMemoryDescriptor(NULL); desc;
	 desc = GetMemoryDescriptor(desc)) {
	count++;
    }

    return ROUNDUP(sizeof(struct mddt)
		   + (count - 1) * sizeof(struct mddt_cluster));
}

void
write_mddt(struct mddt *mddt, size_t size)
{
    int count = 0, i;
    MEMORY_DESCRIPTOR *desc;
    u_int64_t *p;

    memset(mddt, 0, sizeof(struct mddt));
    for (desc = GetMemoryDescriptor(NULL); desc;
	 desc = GetMemoryDescriptor(desc)) {
	struct mddt_cluster *mc;
	mc = &mddt->mddt_clusters[count];
	mc->mddt_pfn = desc->BasePage;
	mc->mddt_pg_cnt = desc->PageCount;
	mc->mddt_pg_test = 0;
	mc->mddt_v_bitaddr = 0;
	mc->mddt_p_bitaddr = 0;
	mc->mddt_bit_cksum = 0;

	/*
	 * Not sure about the FirmwareTemporary bit but my 164LX has
	 * about 60Mb marked this way.
	 */
	if (desc->Type == MemoryFree || desc->Type == MemoryFirmwareTemporary)
	    mc->mddt_usage = MDDT_SYSTEM;
	else if (desc->Type == MemorySpecialMemory)
	    mc->mddt_usage = MDDT_NONVOLATILE; /* ?? */
	else
	    mc->mddt_usage = MDDT_PALCODE;
	count++;
    }
    mddt->mddt_cluster_cnt = count;
    mddt->mddt_cksum = checksum(mddt, size);
}

size_t
size_rpb()
{
    return sizeof(struct rpb) + size_mddt();
}

void
write_rpb(struct rpb *rpb)
{
    EXTENDED_SYSTEM_INFORMATION	sysinfo;
    SYSTEM_ID *sysid;

    ReturnExtendedSystemInformation(&sysinfo);

    memset(rpb, 0, sizeof(struct rpb));
    rpb->rpb_phys = 0;			/* XXX */
    strcpy(rpb->rpb_magic, "HWRPB");
    rpb->rpb_version = HWRPB_DSRDB_MINVERS;
    rpb->rpb_size = sizeof(struct rpb);
    rpb->rpb_primary_cpu_id = 0;	/* XXX */
    rpb->rpb_page_size = sysinfo.ProcessorPageSize;
    rpb->rpb_phys_addr_size = sysinfo.NumberOfPhysicalAddressBits;
    rpb->rpb_max_asn = sysinfo.MaximumAddressSpaceNumber;
    rpb->rpb_type = ST_EB164;		/* XXX */
    rpb->rpb_variation = SV_ST_ALPHAPC164LX_533; /* XXX */
    rpb->rpb_intr_freq = 1024*4096;	/* XXX */
    rpb->rpb_cc_freq = 533000000;	/* XXX */
    rpb->rpb_memdat_off = sizeof(struct rpb);
    write_mddt((struct mddt *)((caddr_t) rpb + rpb->rpb_memdat_off),
	       size_mddt());
    rpb->rpb_checksum = checksum(rpb, 280); /* only sum first 280 bytes */
}

struct rpb *
make_rpb()
{
    EXTENDED_SYSTEM_INFORMATION	sysinfo;
    struct rpb *rpb;

    ReturnExtendedSystemInformation(&sysinfo);
    printf("sysinfo.ProcessorId = %x\n", sysinfo.ProcessorId);
    printf("sysinfo.ProcessorRevision = %d\n", sysinfo.ProcessorRevision);
    printf("sysinfo.ProcessorPageSize = %d\n", sysinfo.ProcessorPageSize);
    printf("sysinfo.NumberOfPhysicalAddressBits = %d\n", sysinfo.NumberOfPhysicalAddressBits);
    printf("sysinfo.MaximumAddressSpaceNumber = %d\n", sysinfo.MaximumAddressSpaceNumber);
    printf("sysinfo.ProcessorCycleCounterPeriod = %d\n", sysinfo.ProcessorCycleCounterPeriod);
    printf("sysinfo.SystemRevision = %d\n", sysinfo.SystemRevision);
    printf("sysinfo.SystemSerialNumber = %s\n", sysinfo.SystemSerialNumber);
    printf("sysinfo.FirmwareVersion = %s\n", sysinfo.FirmwareVersion);

    rpb = malloc(size_rpb());
    write_rpb(rpb);
    return rpb;
}
