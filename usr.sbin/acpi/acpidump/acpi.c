/*-
 * Copyright (c) 1998 Doug Rabson
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: acpi.c,v 1.4 2000/08/09 14:47:52 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "acpidump.h"

#include "aml/aml_env.h"
#include "aml/aml_common.h"

#define BEGIN_COMMENT	"/*\n"
#define END_COMMENT	" */\n"

struct ACPIsdt	dsdt_header = {
	"DSDT", 0, 1, 0, "OEMID", "OEMTBLID", 0x12345678, "CRTR", 0x12345678
};

static void
acpi_trim_string(char *s, size_t length)
{

	/* Trim trailing spaces and NULLs */
	while (length > 0 && (s[length - 1] == ' ' || s[length - 1] == '\0'))
		s[length-- - 1] = '\0';
}

static void
acpi_print_dsdt_definition(void)
{
	char	oemid[6 + 1];
	char	oemtblid[8 + 1];

	acpi_trim_string(dsdt_header.oemid, 6);
	acpi_trim_string(dsdt_header.oemtblid, 8);
	strncpy(oemid, dsdt_header.oemid, 6);
	oemid[6] = '\0';
	strncpy(oemtblid, dsdt_header.oemtblid, 8);
	oemtblid[8] = '\0';

	printf("DefinitionBlock (
    \"acpi_dsdt.aml\",	//Output filename
    \"DSDT\",		//Signature
    0x%x,		//DSDT Revision
    \"%s\",		//OEMID
    \"%s\",		//TABLE ID
    0x%x		//OEM Revision\n)\n",
	dsdt_header.rev, oemid, oemtblid, dsdt_header.oemrev);
}

static void
acpi_print_string(char *s, size_t length)
{
	int	c;

	/* Trim trailing spaces and NULLs */
	while (length > 0 && (s[length - 1] == ' ' || s[length - 1] == '\0'))
		length--;

	while (length--) {
		c = *s++;
		putchar(c);
	}
}

static void
acpi_handle_dsdt(struct ACPIsdt *dsdp)
{
	u_int8_t       *dp;
	u_int8_t       *end;

	acpi_print_dsdt(dsdp);
	dp = (u_int8_t *)dsdp->body;
	end = (u_int8_t *)dsdp + dsdp->len;

	acpi_dump_dsdt(dp, end);
}

static void
acpi_handle_facp(struct FACPbody *facp)
{
	struct	ACPIsdt *dsdp;

	acpi_print_facp(facp);
	dsdp = (struct ACPIsdt *) acpi_map_sdt(facp->dsdt_ptr);
	if (acpi_checksum(dsdp, dsdp->len))
		errx(1, "DSDT is corrupt\n");
	acpi_handle_dsdt(dsdp);
	aml_dump(dsdp);
}

static void
init_namespace()
{
	struct	aml_environ env;
	struct	aml_name *newname;

	aml_new_name_group(AML_NAME_GROUP_OS_DEFINED);
	env.curname = aml_get_rootname();
	newname = aml_create_name(&env, "\\_OS_");
	newname->property = aml_alloc_object(aml_t_string, NULL);
	newname->property->str.needfree = 0;
	newname->property->str.string = "Microsoft Windows NT";
}

/*
 * Public interfaces
 */

void
acpi_dump_dsdt(u_int8_t *dp, u_int8_t *end)
{
	extern struct aml_environ	asl_env;

	acpi_print_dsdt_definition();

	/* 1st stage: parse only w/o printing */
	init_namespace();
	aml_new_name_group((int)dp);
	bzero(&asl_env, sizeof(asl_env));

	asl_env.dp = dp;
	asl_env.end = end;
	asl_env.curname = aml_get_rootname();

	aml_local_stack_push(aml_local_stack_create());
	aml_parse_objectlist(&asl_env, 0);
	aml_local_stack_delete(aml_local_stack_pop());

	assert(asl_env.dp == asl_env.end);
	asl_env.dp = dp;

	/* 2nd stage: dump whole object list */
	printf("\n{\n");
	asl_dump_objectlist(&dp, end, 0);
	printf("\n}\n");
	assert(dp == end);
}
void
acpi_print_sdt(struct ACPIsdt *sdp)
{

	printf(BEGIN_COMMENT);
	acpi_print_string(sdp->signature, 4);
	printf(": Length=%d, Revision=%d, Checksum=%d,\n",
	       sdp->len, sdp->rev, sdp->check);
	printf("\tOEMID=");
	acpi_print_string(sdp->oemid, 6);
	printf(", OEM Table ID=");
	acpi_print_string(sdp->oemtblid, 8);
	printf(", OEM Revision=0x%x,\n", sdp->oemrev);
	printf("\tCreator ID=");
	acpi_print_string(sdp->creator, 4);
	printf(", Creator Revision=0x%x\n", sdp->crerev);
	printf(END_COMMENT);
	if (!memcmp(sdp->signature, "DSDT", 4)) {
		memcpy(&dsdt_header, sdp, sizeof(dsdt_header));
	}
}

void
acpi_print_rsdt(struct ACPIsdt *rsdp)
{
	int	i, entries;

	acpi_print_sdt(rsdp);
	entries = (rsdp->len - SIZEOF_SDT_HDR) / sizeof(u_int32_t);
	printf(BEGIN_COMMENT);
	printf("\tEntries={ ");
	for (i = 0; i < entries; i++) {
		if (i > 0)
			printf(", ");
		printf("0x%08x", rsdp->body[i]);
	}
	printf(" }\n");
	printf(END_COMMENT);
}

void
acpi_print_facp(struct FACPbody *facp)
{
	char	sep;

	printf(BEGIN_COMMENT);
	printf("\tDSDT=0x%x\n", facp->dsdt_ptr);
	printf("\tINT_MODEL=%s\n", facp->int_model ? "APIC" : "PIC");
	printf("\tSCI_INT=%d\n", facp->sci_int);
	printf("\tSMI_CMD=0x%x, ", facp->smi_cmd);
	printf("ACPI_ENABLE=0x%x, ", facp->acpi_enable);
	printf("ACPI_DISABLE=0x%x, ", facp->acpi_disable);
	printf("S4BIOS_REQ=0x%x\n", facp->s4biosreq);
	if (facp->pm1a_evt_blk)
		printf("\tPM1a_EVT_BLK=0x%x-0x%x\n",
		       facp->pm1a_evt_blk,
		       facp->pm1a_evt_blk + facp->pm1_evt_len - 1);
	if (facp->pm1b_evt_blk)
		printf("\tPM1b_EVT_BLK=0x%x-0x%x\n",
		       facp->pm1b_evt_blk,
		       facp->pm1b_evt_blk + facp->pm1_evt_len - 1);
	if (facp->pm1a_cnt_blk)
		printf("\tPM1a_CNT_BLK=0x%x-0x%x\n",
		       facp->pm1a_cnt_blk,
		       facp->pm1a_cnt_blk + facp->pm1_cnt_len - 1);
	if (facp->pm1b_cnt_blk)
		printf("\tPM1b_CNT_BLK=0x%x-0x%x\n",
		       facp->pm1b_cnt_blk,
		       facp->pm1b_cnt_blk + facp->pm1_cnt_len - 1);
	if (facp->pm2_cnt_blk)
		printf("\tPM2_CNT_BLK=0x%x-0x%x\n",
		       facp->pm2_cnt_blk,
		       facp->pm2_cnt_blk + facp->pm2_cnt_len - 1);
	if (facp->pm_tmr_blk)
		printf("\tPM2_TMR_BLK=0x%x-0x%x\n",
		       facp->pm_tmr_blk,
		       facp->pm_tmr_blk + facp->pm_tmr_len - 1);
	if (facp->gpe0_blk)
		printf("\tPM2_GPE0_BLK=0x%x-0x%x\n",
		       facp->gpe0_blk,
		       facp->gpe0_blk + facp->gpe0_len - 1);
	if (facp->gpe1_blk)
		printf("\tPM2_GPE1_BLK=0x%x-0x%x, GPE1_BASE=%d\n",
		       facp->gpe1_blk,
		       facp->gpe1_blk + facp->gpe1_len - 1,
		       facp->gpe1_base);
	printf("\tP_LVL2_LAT=%dms, P_LVL3_LAT=%dms\n",
	       facp->p_lvl2_lat, facp->p_lvl3_lat);
	printf("\tFLUSH_SIZE=%d, FLUSH_STRIDE=%d\n",
	       facp->flush_size, facp->flush_stride);
	printf("\tDUTY_OFFSET=%d, DUTY_WIDTH=%d\n",
	       facp->duty_off, facp->duty_width);
	printf("\tDAY_ALRM=%d, MON_ALRM=%d, CENTURY=%d\n",
	       facp->day_alrm, facp->mon_alrm, facp->century);
	printf("\tFlags=");
	sep = '{';

#define PRINTFLAG(xx) do {					\
	if (facp->flags & ACPI_FACP_FLAG_## xx) {		\
		printf("%c%s", sep, #xx); sep = ',';		\
	}							\
} while (0)

	PRINTFLAG(WBINVD);
	PRINTFLAG(WBINVD_FLUSH);
	PRINTFLAG(PROC_C1);
	PRINTFLAG(P_LVL2_UP);
	PRINTFLAG(PWR_BUTTON);
	PRINTFLAG(SLP_BUTTON);
	PRINTFLAG(FIX_RTC);
	PRINTFLAG(RTC_S4);
	PRINTFLAG(TMR_VAL_EXT);
	PRINTFLAG(DCK_CAP);

#undef PRINTFLAG

	printf("}\n");
	printf(END_COMMENT);
}

void
acpi_print_dsdt(struct ACPIsdt *dsdp)
{

	acpi_print_sdt(dsdp);
}

int
acpi_checksum(void *p, size_t length)
{
	u_int8_t	*bp;
	u_int8_t	sum;

	bp = p;
	sum = 0;
	while (length--)
		sum += *bp++;

	return (sum);
}

struct ACPIsdt *
acpi_map_sdt(vm_offset_t pa)
{
	struct	ACPIsdt *sp;

	sp = acpi_map_physical(pa, sizeof(struct ACPIsdt));
	sp = acpi_map_physical(pa, sp->len);
	return (sp);
}

void
acpi_print_rsd_ptr(struct ACPIrsdp *rp)
{

	printf(BEGIN_COMMENT);
	printf("RSD PTR: Checksum=%d, OEMID=", rp->sum);
	acpi_print_string(rp->oem, 6);
	printf(", RsdtAddress=0x%08x\n", rp->rsdt_addr);
	printf(END_COMMENT);
}

void
acpi_handle_rsdt(struct ACPIsdt *rsdp)
{
	int	i;
	int	entries;
	struct	ACPIsdt *sdp;

	entries = (rsdp->len - SIZEOF_SDT_HDR) / sizeof(u_int32_t);
	acpi_print_rsdt(rsdp);
	for (i = 0; i < entries; i++) {
		sdp = (struct ACPIsdt *) acpi_map_sdt(rsdp->body[i]);
		if (acpi_checksum(sdp, sdp->len))
			errx(1, "RSDT entry %d is corrupt\n", i);
		if (!memcmp(sdp->signature, "FACP", 4)) {
			acpi_handle_facp((struct FACPbody *) sdp->body);
		} else {
			acpi_print_sdt(sdp);
		}
	}
}

/*
 *	Dummy functions
 */

void
aml_dbgr(struct aml_environ *env1, struct aml_environ *env2)
{
	/* do nothing */
}

int
aml_region_read_simple(struct aml_region_handle *h, vm_offset_t offset,
    u_int32_t *valuep)
{
	return (0);
}

int
aml_region_write_simple(struct aml_region_handle *h, vm_offset_t offset,
    u_int32_t value)
{
	return (0);
}

u_int32_t
aml_region_prompt_read(struct aml_region_handle *h, u_int32_t value)
{
	return (0);
}

u_int32_t
aml_region_prompt_write(struct aml_region_handle *h, u_int32_t value)
{
	return (0);
}

int
aml_region_prompt_update_value(u_int32_t orgval, u_int32_t value,
    struct aml_region_handle *h)
{
	return (0);
}

u_int32_t
aml_region_read(struct aml_environ *env, int regtype, u_int32_t flags,
    u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen)
{
	return (0);
}

int
aml_region_write(struct aml_environ *env, int regtype, u_int32_t flags,
    u_int32_t value, u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen)
{
	return (0);
}

int
aml_region_write_from_buffer(struct aml_environ *env, int regtype,
    u_int32_t flags, u_int8_t *buffer, u_int32_t addr, u_int32_t bitoffset,
    u_int32_t bitlen)
{
	return (0);
}

int
aml_region_bcopy(struct aml_environ *env, int regtype, u_int32_t flags,
    u_int32_t addr, u_int32_t bitoffset, u_int32_t bitlen,
    u_int32_t dflags, u_int32_t daddr,
    u_int32_t dbitoffset, u_int32_t dbitlen)
{
	return (0);
}

int
aml_region_read_into_buffer(struct aml_environ *env, int regtype,
    u_int32_t flags, u_int32_t addr, u_int32_t bitoffset,
    u_int32_t bitlen, u_int8_t *buffer)
{
	return (0);
}

