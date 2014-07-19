/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <libutil.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>

#include <machine/vmm.h>
#include <vmmapi.h>

#include "intel/vmcs.h"

#define	MB	(1UL << 20)
#define	GB	(1UL << 30)

#define	REQ_ARG		required_argument
#define	NO_ARG		no_argument
#define	OPT_ARG		optional_argument

static const char *progname;

static void
usage(void)
{

	(void)fprintf(stderr,
	"Usage: %s --vm=<vmname>\n"
	"       [--cpu=<vcpu_number>]\n"
	"       [--create]\n"
	"       [--destroy]\n"
	"       [--get-all]\n"
	"       [--get-stats]\n"
	"       [--set-desc-ds]\n"
	"       [--get-desc-ds]\n"
	"       [--set-desc-es]\n"
	"       [--get-desc-es]\n"
	"       [--set-desc-gs]\n"
	"       [--get-desc-gs]\n"
	"       [--set-desc-fs]\n"
	"       [--get-desc-fs]\n"
	"       [--set-desc-cs]\n"
	"       [--get-desc-cs]\n"
	"       [--set-desc-ss]\n"
	"       [--get-desc-ss]\n"
	"       [--set-desc-tr]\n"
	"       [--get-desc-tr]\n"
	"       [--set-desc-ldtr]\n"
	"       [--get-desc-ldtr]\n"
	"       [--set-desc-gdtr]\n"
	"       [--get-desc-gdtr]\n"
	"       [--set-desc-idtr]\n"
	"       [--get-desc-idtr]\n"
	"       [--run]\n"
	"       [--capname=<capname>]\n"
	"       [--getcap]\n"
	"       [--setcap=<0|1>]\n"
	"       [--desc-base=<BASE>]\n"
	"       [--desc-limit=<LIMIT>]\n"
	"       [--desc-access=<ACCESS>]\n"
	"       [--set-cr0=<CR0>]\n"
	"       [--get-cr0]\n"
	"       [--set-cr3=<CR3>]\n"
	"       [--get-cr3]\n"
	"       [--set-cr4=<CR4>]\n"
	"       [--get-cr4]\n"
	"       [--set-dr7=<DR7>]\n"
	"       [--get-dr7]\n"
	"       [--set-rsp=<RSP>]\n"
	"       [--get-rsp]\n"
	"       [--set-rip=<RIP>]\n"
	"       [--get-rip]\n"
	"       [--get-rax]\n"
	"       [--set-rax=<RAX>]\n"
	"       [--get-rbx]\n"
	"       [--get-rcx]\n"
	"       [--get-rdx]\n"
	"       [--get-rsi]\n"
	"       [--get-rdi]\n"
	"       [--get-rbp]\n"
	"       [--get-r8]\n"
	"       [--get-r9]\n"
	"       [--get-r10]\n"
	"       [--get-r11]\n"
	"       [--get-r12]\n"
	"       [--get-r13]\n"
	"       [--get-r14]\n"
	"       [--get-r15]\n"
	"       [--set-rflags=<RFLAGS>]\n"
	"       [--get-rflags]\n"
	"       [--set-cs]\n"
	"       [--get-cs]\n"
	"       [--set-ds]\n"
	"       [--get-ds]\n"
	"       [--set-es]\n"
	"       [--get-es]\n"
	"       [--set-fs]\n"
	"       [--get-fs]\n"
	"       [--set-gs]\n"
	"       [--get-gs]\n"
	"       [--set-ss]\n"
	"       [--get-ss]\n"
	"       [--get-tr]\n"
	"       [--get-ldtr]\n"
	"       [--get-vmcs-pinbased-ctls]\n"
	"       [--get-vmcs-procbased-ctls]\n"
	"       [--get-vmcs-procbased-ctls2]\n"
	"       [--get-vmcs-entry-interruption-info]\n"
	"       [--set-vmcs-entry-interruption-info=<info>]\n"
	"       [--get-vmcs-eptp]\n"
	"       [--get-vmcs-guest-physical-address\n"
	"       [--get-vmcs-guest-linear-address\n"
	"       [--set-vmcs-exception-bitmap]\n"
	"       [--get-vmcs-exception-bitmap]\n"
	"       [--get-vmcs-io-bitmap-address]\n"
	"       [--get-vmcs-tsc-offset]\n"
	"       [--get-vmcs-guest-pat]\n"
	"       [--get-vmcs-host-pat]\n"
	"       [--get-vmcs-host-cr0]\n"
	"       [--get-vmcs-host-cr3]\n"
	"       [--get-vmcs-host-cr4]\n"
	"       [--get-vmcs-host-rip]\n"
	"       [--get-vmcs-host-rsp]\n"
	"       [--get-vmcs-cr0-mask]\n"
	"       [--get-vmcs-cr0-shadow]\n"
	"       [--get-vmcs-cr4-mask]\n"
	"       [--get-vmcs-cr4-shadow]\n"
	"       [--get-vmcs-cr3-targets]\n"
	"       [--get-vmcs-apic-access-address]\n"
	"       [--get-vmcs-virtual-apic-address]\n"
	"       [--get-vmcs-tpr-threshold]\n"
	"       [--get-vmcs-msr-bitmap]\n"
	"       [--get-vmcs-msr-bitmap-address]\n"
	"       [--get-vmcs-vpid]\n"
	"       [--get-vmcs-ple-gap]\n"
	"       [--get-vmcs-ple-window]\n"
	"       [--get-vmcs-instruction-error]\n"
	"       [--get-vmcs-exit-ctls]\n"
	"       [--get-vmcs-entry-ctls]\n"
	"       [--get-vmcs-guest-sysenter]\n"
	"       [--get-vmcs-link]\n"
	"       [--get-vmcs-exit-reason]\n"
	"       [--get-vmcs-exit-qualification]\n"
	"       [--get-vmcs-exit-interruption-info]\n"
	"       [--get-vmcs-exit-interruption-error]\n"
	"       [--get-vmcs-interruptibility]\n"
	"       [--set-x2apic-state=<state>]\n"
	"       [--get-x2apic-state]\n"
	"       [--unassign-pptdev=<bus/slot/func>]\n"
	"       [--set-mem=<memory in units of MB>]\n"
	"       [--get-lowmem]\n"
	"       [--get-highmem]\n"
	"       [--get-gpa-pmap]\n"
	"       [--assert-lapic-lvt=<pin>]\n"
	"       [--inject-nmi]\n"
	"       [--force-reset]\n"
	"       [--force-poweroff]\n"
	"       [--get-active-cpus]\n"
	"       [--get-suspended-cpus]\n"
	"       [--get-intinfo]\n",
	progname);
	exit(1);
}

static int get_stats, getcap, setcap, capval, get_gpa_pmap;
static int inject_nmi, assert_lapic_lvt;
static int force_reset, force_poweroff;
static const char *capname;
static int create, destroy, get_lowmem, get_highmem;
static int get_intinfo;
static int get_active_cpus, get_suspended_cpus;
static uint64_t memsize;
static int set_cr0, get_cr0, set_cr3, get_cr3, set_cr4, get_cr4;
static int set_efer, get_efer;
static int set_dr7, get_dr7;
static int set_rsp, get_rsp, set_rip, get_rip, set_rflags, get_rflags;
static int set_rax, get_rax;
static int get_rbx, get_rcx, get_rdx, get_rsi, get_rdi, get_rbp;
static int get_r8, get_r9, get_r10, get_r11, get_r12, get_r13, get_r14, get_r15;
static int set_desc_ds, get_desc_ds;
static int set_desc_es, get_desc_es;
static int set_desc_fs, get_desc_fs;
static int set_desc_gs, get_desc_gs;
static int set_desc_cs, get_desc_cs;
static int set_desc_ss, get_desc_ss;
static int set_desc_gdtr, get_desc_gdtr;
static int set_desc_idtr, get_desc_idtr;
static int set_desc_tr, get_desc_tr;
static int set_desc_ldtr, get_desc_ldtr;
static int set_cs, set_ds, set_es, set_fs, set_gs, set_ss, set_tr, set_ldtr;
static int get_cs, get_ds, get_es, get_fs, get_gs, get_ss, get_tr, get_ldtr;
static int set_x2apic_state, get_x2apic_state;
enum x2apic_state x2apic_state;
static int unassign_pptdev, bus, slot, func;
static int run;

/*
 * VMCS-specific fields
 */
static int get_pinbased_ctls, get_procbased_ctls, get_procbased_ctls2;
static int get_eptp, get_io_bitmap, get_tsc_offset;
static int get_vmcs_entry_interruption_info, set_vmcs_entry_interruption_info;
static int get_vmcs_interruptibility;
uint32_t vmcs_entry_interruption_info;
static int get_vmcs_gpa, get_vmcs_gla;
static int get_exception_bitmap, set_exception_bitmap, exception_bitmap;
static int get_cr0_mask, get_cr0_shadow;
static int get_cr4_mask, get_cr4_shadow;
static int get_cr3_targets;
static int get_apic_access_addr, get_virtual_apic_addr, get_tpr_threshold;
static int get_msr_bitmap, get_msr_bitmap_address;
static int get_vpid, get_ple_gap, get_ple_window;
static int get_inst_err, get_exit_ctls, get_entry_ctls;
static int get_host_cr0, get_host_cr3, get_host_cr4;
static int get_host_rip, get_host_rsp;
static int get_guest_pat, get_host_pat;
static int get_guest_sysenter, get_vmcs_link;
static int get_vmcs_exit_reason, get_vmcs_exit_qualification;
static int get_vmcs_exit_interruption_info, get_vmcs_exit_interruption_error;

static uint64_t desc_base;
static uint32_t desc_limit, desc_access;

static int get_all;

static void
dump_vm_run_exitcode(struct vm_exit *vmexit, int vcpu)
{
	printf("vm exit[%d]\n", vcpu);
	printf("\trip\t\t0x%016lx\n", vmexit->rip);
	printf("\tinst_length\t%d\n", vmexit->inst_length);
	switch (vmexit->exitcode) {
	case VM_EXITCODE_INOUT:
		printf("\treason\t\tINOUT\n");
		printf("\tdirection\t%s\n", vmexit->u.inout.in ? "IN" : "OUT");
		printf("\tbytes\t\t%d\n", vmexit->u.inout.bytes);
		printf("\tflags\t\t%s%s\n",
			vmexit->u.inout.string ? "STRING " : "",
			vmexit->u.inout.rep ? "REP " : "");
		printf("\tport\t\t0x%04x\n", vmexit->u.inout.port);
		printf("\teax\t\t0x%08x\n", vmexit->u.inout.eax);
		break;
	case VM_EXITCODE_VMX:
		printf("\treason\t\tVMX\n");
		printf("\tstatus\t\t%d\n", vmexit->u.vmx.status);
		printf("\texit_reason\t0x%08x (%u)\n",
		    vmexit->u.vmx.exit_reason, vmexit->u.vmx.exit_reason);
		printf("\tqualification\t0x%016lx\n",
			vmexit->u.vmx.exit_qualification);
		printf("\tinst_type\t\t%d\n", vmexit->u.vmx.inst_type);
		printf("\tinst_error\t\t%d\n", vmexit->u.vmx.inst_error);
		break;
	default:
		printf("*** unknown vm run exitcode %d\n", vmexit->exitcode);
		break;
	}
}

static int
dump_vmcs_msr_bitmap(int vcpu, u_long addr)
{
	int error, fd, byte, bit, readable, writeable;
	u_int msr;
	const char *bitmap;

	error = -1;
	bitmap = MAP_FAILED;

	fd = open("/dev/mem", O_RDONLY, 0);
	if (fd < 0)
		goto done;

	bitmap = mmap(NULL, PAGE_SIZE, PROT_READ, 0, fd, addr);
	if (bitmap == MAP_FAILED)
		goto done;

	for (msr = 0; msr < 0x2000; msr++) {
		byte = msr / 8;
		bit = msr & 0x7;

		/* Look at MSRs in the range 0x00000000 to 0x00001FFF */
		readable = (bitmap[byte] & (1 << bit)) ? 0 : 1;
		writeable = (bitmap[2048 + byte] & (1 << bit)) ? 0 : 1;
		if (readable || writeable) {
			printf("msr 0x%08x[%d]\t\t%c%c\n", msr, vcpu,
				readable ? 'R' : '-',
				writeable ? 'W' : '-');
		}

		/* Look at MSRs in the range 0xC0000000 to 0xC0001FFF */
		byte += 1024;
		readable = (bitmap[byte] & (1 << bit)) ? 0 : 1;
		writeable = (bitmap[2048 + byte] & (1 << bit)) ? 0 : 1;
		if (readable || writeable) {
			printf("msr 0x%08x[%d]\t\t%c%c\n",
				0xc0000000 + msr, vcpu,
				readable ? 'R' : '-',
				writeable ? 'W' : '-');
		}
	}

	error = 0;
done:
	if (bitmap != MAP_FAILED)
		munmap((void *)bitmap, PAGE_SIZE);
	if (fd >= 0)
		close(fd);
	return (error);
}

static int
vm_get_vmcs_field(struct vmctx *ctx, int vcpu, int field, uint64_t *ret_val)
{

	return (vm_get_register(ctx, vcpu, VMCS_IDENT(field), ret_val));
}

static int
vm_set_vmcs_field(struct vmctx *ctx, int vcpu, int field, uint64_t val)
{

	return (vm_set_register(ctx, vcpu, VMCS_IDENT(field), val));
}

enum {
	VMNAME = 1000,	/* avoid collision with return values from getopt */
	VCPU,
	SET_MEM,
	SET_EFER,
	SET_CR0,
	SET_CR3,
	SET_CR4,
	SET_DR7,
	SET_RSP,
	SET_RIP,
	SET_RAX,
	SET_RFLAGS,
	DESC_BASE,
	DESC_LIMIT,
	DESC_ACCESS,
	SET_CS,
	SET_DS,
	SET_ES,
	SET_FS,
	SET_GS,
	SET_SS,
	SET_TR,
	SET_LDTR,
	SET_X2APIC_STATE,
	SET_VMCS_EXCEPTION_BITMAP,
	SET_VMCS_ENTRY_INTERRUPTION_INFO,
	SET_CAP,
	CAPNAME,
	UNASSIGN_PPTDEV,
	GET_GPA_PMAP,
	ASSERT_LAPIC_LVT,
};

static void
print_cpus(const char *banner, const cpuset_t *cpus)
{
	int i, first;

	first = 1;
	printf("%s:\t", banner);
	if (!CPU_EMPTY(cpus)) {
		for (i = 0; i < CPU_SETSIZE; i++) {
			if (CPU_ISSET(i, cpus)) {
				printf("%s%d", first ? " " : ", ", i);
				first = 0;
			}
		}
	} else
		printf(" (none)");
	printf("\n");
}

static void
print_intinfo(const char *banner, uint64_t info)
{
	int type;

	printf("%s:\t", banner);
	if (info & VM_INTINFO_VALID) {
		type = info & VM_INTINFO_TYPE;
		switch (type) {
		case VM_INTINFO_HWINTR:
			printf("extint");
			break;
		case VM_INTINFO_NMI:
			printf("nmi");
			break;
		case VM_INTINFO_SWINTR:
			printf("swint");
			break;
		default:
			printf("exception");
			break;
		}
		printf(" vector %d", (int)VM_INTINFO_VECTOR(info));
		if (info & VM_INTINFO_DEL_ERRCODE)
			printf(" errcode %#x", (u_int)(info >> 32));
	} else {
		printf("n/a");
	}
	printf("\n");
}

int
main(int argc, char *argv[])
{
	char *vmname;
	int error, ch, vcpu, ptenum;
	vm_paddr_t gpa, gpa_pmap;
	size_t len;
	struct vm_exit vmexit;
	uint64_t ctl, eptp, bm, addr, u64, pteval[4], *pte, info[2];
	struct vmctx *ctx;
	int wired;
	cpuset_t cpus;

	uint64_t cr0, cr3, cr4, dr7, rsp, rip, rflags, efer, pat;
	uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
	uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t cs, ds, es, fs, gs, ss, tr, ldtr;

	struct option opts[] = {
		{ "vm",		REQ_ARG,	0,	VMNAME },
		{ "cpu",	REQ_ARG,	0,	VCPU },
		{ "set-mem",	REQ_ARG,	0,	SET_MEM },
		{ "set-efer",	REQ_ARG,	0,	SET_EFER },
		{ "set-cr0",	REQ_ARG,	0,	SET_CR0 },
		{ "set-cr3",	REQ_ARG,	0,	SET_CR3 },
		{ "set-cr4",	REQ_ARG,	0,	SET_CR4 },
		{ "set-dr7",	REQ_ARG,	0,	SET_DR7 },
		{ "set-rsp",	REQ_ARG,	0,	SET_RSP },
		{ "set-rip",	REQ_ARG,	0,	SET_RIP },
		{ "set-rax",	REQ_ARG,	0,	SET_RAX },
		{ "set-rflags",	REQ_ARG,	0,	SET_RFLAGS },
		{ "desc-base",	REQ_ARG,	0,	DESC_BASE },
		{ "desc-limit",	REQ_ARG,	0,	DESC_LIMIT },
		{ "desc-access",REQ_ARG,	0,	DESC_ACCESS },
		{ "set-cs",	REQ_ARG,	0,	SET_CS },
		{ "set-ds",	REQ_ARG,	0,	SET_DS },
		{ "set-es",	REQ_ARG,	0,	SET_ES },
		{ "set-fs",	REQ_ARG,	0,	SET_FS },
		{ "set-gs",	REQ_ARG,	0,	SET_GS },
		{ "set-ss",	REQ_ARG,	0,	SET_SS },
		{ "set-tr",	REQ_ARG,	0,	SET_TR },
		{ "set-ldtr",	REQ_ARG,	0,	SET_LDTR },
		{ "set-x2apic-state",REQ_ARG,	0,	SET_X2APIC_STATE },
		{ "set-vmcs-exception-bitmap",
				REQ_ARG,	0, SET_VMCS_EXCEPTION_BITMAP },
		{ "set-vmcs-entry-interruption-info",
				REQ_ARG, 0, SET_VMCS_ENTRY_INTERRUPTION_INFO },
		{ "capname",	REQ_ARG,	0,	CAPNAME },
		{ "unassign-pptdev", REQ_ARG,	0,	UNASSIGN_PPTDEV },
		{ "setcap",	REQ_ARG,	0,	SET_CAP },
		{ "get-gpa-pmap", REQ_ARG,	0,	GET_GPA_PMAP },
		{ "assert-lapic-lvt", REQ_ARG,	0,	ASSERT_LAPIC_LVT },
		{ "getcap",	NO_ARG,		&getcap,	1 },
		{ "get-stats",	NO_ARG,		&get_stats,	1 },
		{ "get-desc-ds",NO_ARG,		&get_desc_ds,	1 },
		{ "set-desc-ds",NO_ARG,		&set_desc_ds,	1 },
		{ "get-desc-es",NO_ARG,		&get_desc_es,	1 },
		{ "set-desc-es",NO_ARG,		&set_desc_es,	1 },
		{ "get-desc-ss",NO_ARG,		&get_desc_ss,	1 },
		{ "set-desc-ss",NO_ARG,		&set_desc_ss,	1 },
		{ "get-desc-cs",NO_ARG,		&get_desc_cs,	1 },
		{ "set-desc-cs",NO_ARG,		&set_desc_cs,	1 },
		{ "get-desc-fs",NO_ARG,		&get_desc_fs,	1 },
		{ "set-desc-fs",NO_ARG,		&set_desc_fs,	1 },
		{ "get-desc-gs",NO_ARG,		&get_desc_gs,	1 },
		{ "set-desc-gs",NO_ARG,		&set_desc_gs,	1 },
		{ "get-desc-tr",NO_ARG,		&get_desc_tr,	1 },
		{ "set-desc-tr",NO_ARG,		&set_desc_tr,	1 },
		{ "set-desc-ldtr", NO_ARG,	&set_desc_ldtr,	1 },
		{ "get-desc-ldtr", NO_ARG,	&get_desc_ldtr,	1 },
		{ "set-desc-gdtr", NO_ARG,	&set_desc_gdtr, 1 },
		{ "get-desc-gdtr", NO_ARG,	&get_desc_gdtr, 1 },
		{ "set-desc-idtr", NO_ARG,	&set_desc_idtr, 1 },
		{ "get-desc-idtr", NO_ARG,	&get_desc_idtr, 1 },
		{ "get-lowmem", NO_ARG,		&get_lowmem,	1 },
		{ "get-highmem",NO_ARG,		&get_highmem,	1 },
		{ "get-efer",	NO_ARG,		&get_efer,	1 },
		{ "get-cr0",	NO_ARG,		&get_cr0,	1 },
		{ "get-cr3",	NO_ARG,		&get_cr3,	1 },
		{ "get-cr4",	NO_ARG,		&get_cr4,	1 },
		{ "get-dr7",	NO_ARG,		&get_dr7,	1 },
		{ "get-rsp",	NO_ARG,		&get_rsp,	1 },
		{ "get-rip",	NO_ARG,		&get_rip,	1 },
		{ "get-rax",	NO_ARG,		&get_rax,	1 },
		{ "get-rbx",	NO_ARG,		&get_rbx,	1 },
		{ "get-rcx",	NO_ARG,		&get_rcx,	1 },
		{ "get-rdx",	NO_ARG,		&get_rdx,	1 },
		{ "get-rsi",	NO_ARG,		&get_rsi,	1 },
		{ "get-rdi",	NO_ARG,		&get_rdi,	1 },
		{ "get-rbp",	NO_ARG,		&get_rbp,	1 },
		{ "get-r8",	NO_ARG,		&get_r8,	1 },
		{ "get-r9",	NO_ARG,		&get_r9,	1 },
		{ "get-r10",	NO_ARG,		&get_r10,	1 },
		{ "get-r11",	NO_ARG,		&get_r11,	1 },
		{ "get-r12",	NO_ARG,		&get_r12,	1 },
		{ "get-r13",	NO_ARG,		&get_r13,	1 },
		{ "get-r14",	NO_ARG,		&get_r14,	1 },
		{ "get-r15",	NO_ARG,		&get_r15,	1 },
		{ "get-rflags",	NO_ARG,		&get_rflags,	1 },
		{ "get-cs",	NO_ARG,		&get_cs,	1 },
		{ "get-ds",	NO_ARG,		&get_ds,	1 },
		{ "get-es",	NO_ARG,		&get_es,	1 },
		{ "get-fs",	NO_ARG,		&get_fs,	1 },
		{ "get-gs",	NO_ARG,		&get_gs,	1 },
		{ "get-ss",	NO_ARG,		&get_ss,	1 },
		{ "get-tr",	NO_ARG,		&get_tr,	1 },
		{ "get-ldtr",	NO_ARG,		&get_ldtr,	1 },
		{ "get-vmcs-pinbased-ctls",
				NO_ARG,		&get_pinbased_ctls, 1 },
		{ "get-vmcs-procbased-ctls",
				NO_ARG,		&get_procbased_ctls, 1 },
		{ "get-vmcs-procbased-ctls2",
				NO_ARG,		&get_procbased_ctls2, 1 },
		{ "get-vmcs-guest-linear-address",
				NO_ARG,		&get_vmcs_gla,	1 },
		{ "get-vmcs-guest-physical-address",
				NO_ARG,		&get_vmcs_gpa,	1 },
		{ "get-vmcs-entry-interruption-info",
				NO_ARG, &get_vmcs_entry_interruption_info, 1},
		{ "get-vmcs-eptp", NO_ARG,	&get_eptp,	1 },
		{ "get-vmcs-exception-bitmap",
				NO_ARG,		&get_exception_bitmap, 1 },
		{ "get-vmcs-io-bitmap-address",
				NO_ARG,		&get_io_bitmap,	1 },
		{ "get-vmcs-tsc-offset", NO_ARG,&get_tsc_offset, 1 },
		{ "get-vmcs-cr0-mask", NO_ARG,	&get_cr0_mask,	1 },
		{ "get-vmcs-cr0-shadow", NO_ARG,&get_cr0_shadow, 1 },
		{ "get-vmcs-cr4-mask", NO_ARG,	&get_cr4_mask,	1 },
		{ "get-vmcs-cr4-shadow", NO_ARG,&get_cr4_shadow, 1 },
		{ "get-vmcs-cr3-targets", NO_ARG, &get_cr3_targets, 1},
		{ "get-vmcs-apic-access-address",
				NO_ARG,		&get_apic_access_addr, 1},
		{ "get-vmcs-virtual-apic-address",
				NO_ARG,		&get_virtual_apic_addr, 1},
		{ "get-vmcs-tpr-threshold",
				NO_ARG,		&get_tpr_threshold, 1 },
		{ "get-vmcs-msr-bitmap",
				NO_ARG,		&get_msr_bitmap, 1 },
		{ "get-vmcs-msr-bitmap-address",
				NO_ARG,		&get_msr_bitmap_address, 1 },
		{ "get-vmcs-vpid", NO_ARG,	&get_vpid,	1 },
		{ "get-vmcs-ple-gap", NO_ARG,	&get_ple_gap,	1 },
		{ "get-vmcs-ple-window", NO_ARG,&get_ple_window,1 },
		{ "get-vmcs-instruction-error",
				NO_ARG,		&get_inst_err,	1 },
		{ "get-vmcs-exit-ctls", NO_ARG,	&get_exit_ctls,	1 },
		{ "get-vmcs-entry-ctls",
					NO_ARG,	&get_entry_ctls, 1 },
		{ "get-vmcs-guest-pat",	NO_ARG,	&get_guest_pat,	1 },
		{ "get-vmcs-host-pat",	NO_ARG,	&get_host_pat,	1 },
		{ "get-vmcs-host-cr0",
				NO_ARG,		&get_host_cr0,	1 },
		{ "get-vmcs-host-cr3",
				NO_ARG,		&get_host_cr3,	1 },
		{ "get-vmcs-host-cr4",
				NO_ARG,		&get_host_cr4,	1 },
		{ "get-vmcs-host-rip",
				NO_ARG,		&get_host_rip,	1 },
		{ "get-vmcs-host-rsp",
				NO_ARG,		&get_host_rsp,	1 },
		{ "get-vmcs-guest-sysenter",
				NO_ARG,		&get_guest_sysenter, 1 },
		{ "get-vmcs-link", NO_ARG,	&get_vmcs_link, 1 },
		{ "get-vmcs-exit-reason",
				NO_ARG,		&get_vmcs_exit_reason, 1 },
		{ "get-vmcs-exit-qualification",
			NO_ARG,		&get_vmcs_exit_qualification, 1 },
		{ "get-vmcs-exit-interruption-info",
				NO_ARG,	&get_vmcs_exit_interruption_info, 1},
		{ "get-vmcs-exit-interruption-error",
				NO_ARG,	&get_vmcs_exit_interruption_error, 1},
		{ "get-vmcs-interruptibility",
				NO_ARG, &get_vmcs_interruptibility, 1 },
		{ "get-x2apic-state",NO_ARG,	&get_x2apic_state, 1 },
		{ "get-all",	NO_ARG,		&get_all,	1 },
		{ "run",	NO_ARG,		&run,		1 },
		{ "create",	NO_ARG,		&create,	1 },
		{ "destroy",	NO_ARG,		&destroy,	1 },
		{ "inject-nmi",	NO_ARG,		&inject_nmi,	1 },
		{ "force-reset",	NO_ARG,	&force_reset,	1 },
		{ "force-poweroff", NO_ARG,	&force_poweroff, 1 },
		{ "get-active-cpus", NO_ARG,	&get_active_cpus, 1 },
		{ "get-suspended-cpus", NO_ARG,	&get_suspended_cpus, 1 },
		{ "get-intinfo", NO_ARG,	&get_intinfo,	1 },
		{ NULL,		0,		NULL,		0 }
	};

	vcpu = 0;
	vmname = NULL;
	assert_lapic_lvt = -1;
	progname = basename(argv[0]);

	while ((ch = getopt_long(argc, argv, "", opts, NULL)) != -1) {
		switch (ch) {
		case 0:
			break;
		case VMNAME:
			vmname = optarg;
			break;
		case VCPU:
			vcpu = atoi(optarg);
			break;
		case SET_MEM:
			memsize = atoi(optarg) * MB;
			memsize = roundup(memsize, 2 * MB);
			break;
		case SET_EFER:
			efer = strtoul(optarg, NULL, 0);
			set_efer = 1;
			break;
		case SET_CR0:
			cr0 = strtoul(optarg, NULL, 0);
			set_cr0 = 1;
			break;
		case SET_CR3:
			cr3 = strtoul(optarg, NULL, 0);
			set_cr3 = 1;
			break;
		case SET_CR4:
			cr4 = strtoul(optarg, NULL, 0);
			set_cr4 = 1;
			break;
		case SET_DR7:
			dr7 = strtoul(optarg, NULL, 0);
			set_dr7 = 1;
			break;
		case SET_RSP:
			rsp = strtoul(optarg, NULL, 0);
			set_rsp = 1;
			break;
		case SET_RIP:
			rip = strtoul(optarg, NULL, 0);
			set_rip = 1;
			break;
		case SET_RAX:
			rax = strtoul(optarg, NULL, 0);
			set_rax = 1;
			break;
		case SET_RFLAGS:
			rflags = strtoul(optarg, NULL, 0);
			set_rflags = 1;
			break;
		case DESC_BASE:
			desc_base = strtoul(optarg, NULL, 0);
			break;
		case DESC_LIMIT:
			desc_limit = strtoul(optarg, NULL, 0);
			break;
		case DESC_ACCESS:
			desc_access = strtoul(optarg, NULL, 0);
			break;
		case SET_CS:
			cs = strtoul(optarg, NULL, 0);
			set_cs = 1;
			break;
		case SET_DS:
			ds = strtoul(optarg, NULL, 0);
			set_ds = 1;
			break;
		case SET_ES:
			es = strtoul(optarg, NULL, 0);
			set_es = 1;
			break;
		case SET_FS:
			fs = strtoul(optarg, NULL, 0);
			set_fs = 1;
			break;
		case SET_GS:
			gs = strtoul(optarg, NULL, 0);
			set_gs = 1;
			break;
		case SET_SS:
			ss = strtoul(optarg, NULL, 0);
			set_ss = 1;
			break;
		case SET_TR:
			tr = strtoul(optarg, NULL, 0);
			set_tr = 1;
			break;
		case SET_LDTR:
			ldtr = strtoul(optarg, NULL, 0);
			set_ldtr = 1;
			break;
		case SET_X2APIC_STATE:
			x2apic_state = strtol(optarg, NULL, 0);
			set_x2apic_state = 1;
			break;
		case SET_VMCS_EXCEPTION_BITMAP:
			exception_bitmap = strtoul(optarg, NULL, 0);
			set_exception_bitmap = 1;
			break;
		case SET_VMCS_ENTRY_INTERRUPTION_INFO:
			vmcs_entry_interruption_info = strtoul(optarg, NULL, 0);
			set_vmcs_entry_interruption_info = 1;
			break;
		case SET_CAP:
			capval = strtoul(optarg, NULL, 0);
			setcap = 1;
			break;
		case GET_GPA_PMAP:
			gpa_pmap = strtoul(optarg, NULL, 0);
			get_gpa_pmap = 1;
			break;
		case CAPNAME:
			capname = optarg;
			break;
		case UNASSIGN_PPTDEV:
			unassign_pptdev = 1;
			if (sscanf(optarg, "%d/%d/%d", &bus, &slot, &func) != 3)
				usage();
			break;
		case ASSERT_LAPIC_LVT:
			assert_lapic_lvt = atoi(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (vmname == NULL)
		usage();

	error = 0;

	if (!error && create)
		error = vm_create(vmname);

	if (!error) {
		ctx = vm_open(vmname);
		if (ctx == NULL)
			error = -1;
	}

	if (!error && memsize)
		error = vm_setup_memory(ctx, memsize, VM_MMAP_NONE);

	if (!error && set_efer)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_EFER, efer);

	if (!error && set_cr0)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_CR0, cr0);

	if (!error && set_cr3)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_CR3, cr3);

	if (!error && set_cr4)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_CR4, cr4);

	if (!error && set_dr7)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_DR7, dr7);

	if (!error && set_rsp)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_RSP, rsp);

	if (!error && set_rip)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_RIP, rip);

	if (!error && set_rax)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_RAX, rax);

	if (!error && set_rflags) {
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_RFLAGS,
					rflags);
	}

	if (!error && set_desc_ds) {
		error = vm_set_desc(ctx, vcpu, VM_REG_GUEST_DS,
				    desc_base, desc_limit, desc_access);
	}

	if (!error && set_desc_es) {
		error = vm_set_desc(ctx, vcpu, VM_REG_GUEST_ES,
				    desc_base, desc_limit, desc_access);
	}

	if (!error && set_desc_ss) {
		error = vm_set_desc(ctx, vcpu, VM_REG_GUEST_SS,
				    desc_base, desc_limit, desc_access);
	}

	if (!error && set_desc_cs) {
		error = vm_set_desc(ctx, vcpu, VM_REG_GUEST_CS,
				    desc_base, desc_limit, desc_access);
	}

	if (!error && set_desc_fs) {
		error = vm_set_desc(ctx, vcpu, VM_REG_GUEST_FS,
				    desc_base, desc_limit, desc_access);
	}

	if (!error && set_desc_gs) {
		error = vm_set_desc(ctx, vcpu, VM_REG_GUEST_GS,
				    desc_base, desc_limit, desc_access);
	}

	if (!error && set_desc_tr) {
		error = vm_set_desc(ctx, vcpu, VM_REG_GUEST_TR,
				    desc_base, desc_limit, desc_access);
	}

	if (!error && set_desc_ldtr) {
		error = vm_set_desc(ctx, vcpu, VM_REG_GUEST_LDTR,
				    desc_base, desc_limit, desc_access);
	}

	if (!error && set_desc_gdtr) {
		error = vm_set_desc(ctx, vcpu, VM_REG_GUEST_GDTR,
				    desc_base, desc_limit, 0);
	}

	if (!error && set_desc_idtr) {
		error = vm_set_desc(ctx, vcpu, VM_REG_GUEST_IDTR,
				    desc_base, desc_limit, 0);
	}

	if (!error && set_cs)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_CS, cs);

	if (!error && set_ds)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_DS, ds);

	if (!error && set_es)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_ES, es);

	if (!error && set_fs)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_FS, fs);

	if (!error && set_gs)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_GS, gs);

	if (!error && set_ss)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_SS, ss);

	if (!error && set_tr)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_TR, tr);

	if (!error && set_ldtr)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_LDTR, ldtr);

	if (!error && set_x2apic_state)
		error = vm_set_x2apic_state(ctx, vcpu, x2apic_state);

	if (!error && unassign_pptdev)
		error = vm_unassign_pptdev(ctx, bus, slot, func);

	if (!error && set_exception_bitmap) {
		error = vm_set_vmcs_field(ctx, vcpu, VMCS_EXCEPTION_BITMAP,
					  exception_bitmap);
	}

	if (!error && set_vmcs_entry_interruption_info) {
		error = vm_set_vmcs_field(ctx, vcpu, VMCS_ENTRY_INTR_INFO,
					  vmcs_entry_interruption_info);
	}

	if (!error && inject_nmi) {
		error = vm_inject_nmi(ctx, vcpu);
	}

	if (!error && assert_lapic_lvt != -1) {
		error = vm_lapic_local_irq(ctx, vcpu, assert_lapic_lvt);
	}

	if (!error && (get_lowmem || get_all)) {
		gpa = 0;
		error = vm_get_memory_seg(ctx, gpa, &len, &wired);
		if (error == 0)
			printf("lowmem\t\t0x%016lx/%ld%s\n", gpa, len,
			    wired ? " wired" : "");
	}

	if (!error && (get_highmem || get_all)) {
		gpa = 4 * GB;
		error = vm_get_memory_seg(ctx, gpa, &len, &wired);
		if (error == 0)
			printf("highmem\t\t0x%016lx/%ld%s\n", gpa, len,
			    wired ? " wired" : "");
	}

	if (!error && (get_efer || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_EFER, &efer);
		if (error == 0)
			printf("efer[%d]\t\t0x%016lx\n", vcpu, efer);
	}

	if (!error && (get_cr0 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_CR0, &cr0);
		if (error == 0)
			printf("cr0[%d]\t\t0x%016lx\n", vcpu, cr0);
	}

	if (!error && (get_cr3 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_CR3, &cr3);
		if (error == 0)
			printf("cr3[%d]\t\t0x%016lx\n", vcpu, cr3);
	}

	if (!error && (get_cr4 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_CR4, &cr4);
		if (error == 0)
			printf("cr4[%d]\t\t0x%016lx\n", vcpu, cr4);
	}

	if (!error && (get_dr7 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_DR7, &dr7);
		if (error == 0)
			printf("dr7[%d]\t\t0x%016lx\n", vcpu, dr7);
	}

	if (!error && (get_rsp || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RSP, &rsp);
		if (error == 0)
			printf("rsp[%d]\t\t0x%016lx\n", vcpu, rsp);
	}

	if (!error && (get_rip || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RIP, &rip);
		if (error == 0)
			printf("rip[%d]\t\t0x%016lx\n", vcpu, rip);
	}

	if (!error && (get_rax || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RAX, &rax);
		if (error == 0)
			printf("rax[%d]\t\t0x%016lx\n", vcpu, rax);
	}

	if (!error && (get_rbx || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RBX, &rbx);
		if (error == 0)
			printf("rbx[%d]\t\t0x%016lx\n", vcpu, rbx);
	}

	if (!error && (get_rcx || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RCX, &rcx);
		if (error == 0)
			printf("rcx[%d]\t\t0x%016lx\n", vcpu, rcx);
	}

	if (!error && (get_rdx || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RDX, &rdx);
		if (error == 0)
			printf("rdx[%d]\t\t0x%016lx\n", vcpu, rdx);
	}

	if (!error && (get_rsi || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RSI, &rsi);
		if (error == 0)
			printf("rsi[%d]\t\t0x%016lx\n", vcpu, rsi);
	}

	if (!error && (get_rdi || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RDI, &rdi);
		if (error == 0)
			printf("rdi[%d]\t\t0x%016lx\n", vcpu, rdi);
	}

	if (!error && (get_rbp || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RBP, &rbp);
		if (error == 0)
			printf("rbp[%d]\t\t0x%016lx\n", vcpu, rbp);
	}

	if (!error && (get_r8 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_R8, &r8);
		if (error == 0)
			printf("r8[%d]\t\t0x%016lx\n", vcpu, r8);
	}

	if (!error && (get_r9 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_R9, &r9);
		if (error == 0)
			printf("r9[%d]\t\t0x%016lx\n", vcpu, r9);
	}

	if (!error && (get_r10 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_R10, &r10);
		if (error == 0)
			printf("r10[%d]\t\t0x%016lx\n", vcpu, r10);
	}

	if (!error && (get_r11 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_R11, &r11);
		if (error == 0)
			printf("r11[%d]\t\t0x%016lx\n", vcpu, r11);
	}

	if (!error && (get_r12 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_R12, &r12);
		if (error == 0)
			printf("r12[%d]\t\t0x%016lx\n", vcpu, r12);
	}

	if (!error && (get_r13 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_R13, &r13);
		if (error == 0)
			printf("r13[%d]\t\t0x%016lx\n", vcpu, r13);
	}

	if (!error && (get_r14 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_R14, &r14);
		if (error == 0)
			printf("r14[%d]\t\t0x%016lx\n", vcpu, r14);
	}

	if (!error && (get_r15 || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_R15, &r15);
		if (error == 0)
			printf("r15[%d]\t\t0x%016lx\n", vcpu, r15);
	}

	if (!error && (get_rflags || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RFLAGS,
					&rflags);
		if (error == 0)
			printf("rflags[%d]\t0x%016lx\n", vcpu, rflags);
	}

	if (!error && (get_stats || get_all)) {
		int i, num_stats;
		uint64_t *stats;
		struct timeval tv;
		const char *desc;

		stats = vm_get_stats(ctx, vcpu, &tv, &num_stats);
		if (stats != NULL) {
			printf("vcpu%d\n", vcpu);
			for (i = 0; i < num_stats; i++) {
				desc = vm_get_stat_desc(ctx, i);
				printf("%-40s\t%ld\n", desc, stats[i]);
			}
		}
	}

	if (!error && (get_desc_ds || get_all)) {
		error = vm_get_desc(ctx, vcpu, VM_REG_GUEST_DS,
				    &desc_base, &desc_limit, &desc_access);
		if (error == 0) {
			printf("ds desc[%d]\t0x%016lx/0x%08x/0x%08x\n",
			       vcpu, desc_base, desc_limit, desc_access);	
		}
	}

	if (!error && (get_desc_es || get_all)) {
		error = vm_get_desc(ctx, vcpu, VM_REG_GUEST_ES,
				    &desc_base, &desc_limit, &desc_access);
		if (error == 0) {
			printf("es desc[%d]\t0x%016lx/0x%08x/0x%08x\n",
			       vcpu, desc_base, desc_limit, desc_access);	
		}
	}

	if (!error && (get_desc_fs || get_all)) {
		error = vm_get_desc(ctx, vcpu, VM_REG_GUEST_FS,
				    &desc_base, &desc_limit, &desc_access);
		if (error == 0) {
			printf("fs desc[%d]\t0x%016lx/0x%08x/0x%08x\n",
			       vcpu, desc_base, desc_limit, desc_access);	
		}
	}

	if (!error && (get_desc_gs || get_all)) {
		error = vm_get_desc(ctx, vcpu, VM_REG_GUEST_GS,
				    &desc_base, &desc_limit, &desc_access);
		if (error == 0) {
			printf("gs desc[%d]\t0x%016lx/0x%08x/0x%08x\n",
			       vcpu, desc_base, desc_limit, desc_access);	
		}
	}

	if (!error && (get_desc_ss || get_all)) {
		error = vm_get_desc(ctx, vcpu, VM_REG_GUEST_SS,
				    &desc_base, &desc_limit, &desc_access);
		if (error == 0) {
			printf("ss desc[%d]\t0x%016lx/0x%08x/0x%08x\n",
			       vcpu, desc_base, desc_limit, desc_access);	
		}
	}

	if (!error && (get_desc_cs || get_all)) {
		error = vm_get_desc(ctx, vcpu, VM_REG_GUEST_CS,
				    &desc_base, &desc_limit, &desc_access);
		if (error == 0) {
			printf("cs desc[%d]\t0x%016lx/0x%08x/0x%08x\n",
			       vcpu, desc_base, desc_limit, desc_access);	
		}
	}

	if (!error && (get_desc_tr || get_all)) {
		error = vm_get_desc(ctx, vcpu, VM_REG_GUEST_TR,
				    &desc_base, &desc_limit, &desc_access);
		if (error == 0) {
			printf("tr desc[%d]\t0x%016lx/0x%08x/0x%08x\n",
			       vcpu, desc_base, desc_limit, desc_access);	
		}
	}

	if (!error && (get_desc_ldtr || get_all)) {
		error = vm_get_desc(ctx, vcpu, VM_REG_GUEST_LDTR,
				    &desc_base, &desc_limit, &desc_access);
		if (error == 0) {
			printf("ldtr desc[%d]\t0x%016lx/0x%08x/0x%08x\n",
			       vcpu, desc_base, desc_limit, desc_access);	
		}
	}

	if (!error && (get_desc_gdtr || get_all)) {
		error = vm_get_desc(ctx, vcpu, VM_REG_GUEST_GDTR,
				    &desc_base, &desc_limit, &desc_access);
		if (error == 0) {
			printf("gdtr[%d]\t\t0x%016lx/0x%08x\n",
			       vcpu, desc_base, desc_limit);	
		}
	}

	if (!error && (get_desc_idtr || get_all)) {
		error = vm_get_desc(ctx, vcpu, VM_REG_GUEST_IDTR,
				    &desc_base, &desc_limit, &desc_access);
		if (error == 0) {
			printf("idtr[%d]\t\t0x%016lx/0x%08x\n",
			       vcpu, desc_base, desc_limit);	
		}
	}

	if (!error && (get_cs || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_CS, &cs);
		if (error == 0)
			printf("cs[%d]\t\t0x%04lx\n", vcpu, cs);
	}

	if (!error && (get_ds || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_DS, &ds);
		if (error == 0)
			printf("ds[%d]\t\t0x%04lx\n", vcpu, ds);
	}

	if (!error && (get_es || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_ES, &es);
		if (error == 0)
			printf("es[%d]\t\t0x%04lx\n", vcpu, es);
	}

	if (!error && (get_fs || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_FS, &fs);
		if (error == 0)
			printf("fs[%d]\t\t0x%04lx\n", vcpu, fs);
	}

	if (!error && (get_gs || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_GS, &gs);
		if (error == 0)
			printf("gs[%d]\t\t0x%04lx\n", vcpu, gs);
	}

	if (!error && (get_ss || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_SS, &ss);
		if (error == 0)
			printf("ss[%d]\t\t0x%04lx\n", vcpu, ss);
	}

	if (!error && (get_tr || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_TR, &tr);
		if (error == 0)
			printf("tr[%d]\t\t0x%04lx\n", vcpu, tr);
	}

	if (!error && (get_ldtr || get_all)) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_LDTR, &ldtr);
		if (error == 0)
			printf("ldtr[%d]\t\t0x%04lx\n", vcpu, ldtr);
	}

	if (!error && (get_x2apic_state || get_all)) {
		error = vm_get_x2apic_state(ctx, vcpu, &x2apic_state);
		if (error == 0)
			printf("x2apic_state[%d]\t%d\n", vcpu, x2apic_state);
	}

	if (!error && (get_pinbased_ctls || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_PIN_BASED_CTLS, &ctl);
		if (error == 0)
			printf("pinbased_ctls[%d]\t0x%08lx\n", vcpu, ctl);
	}

	if (!error && (get_procbased_ctls || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu,
					  VMCS_PRI_PROC_BASED_CTLS, &ctl);
		if (error == 0)
			printf("procbased_ctls[%d]\t0x%08lx\n", vcpu, ctl);
	}

	if (!error && (get_procbased_ctls2 || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu,
					  VMCS_SEC_PROC_BASED_CTLS, &ctl);
		if (error == 0)
			printf("procbased_ctls2[%d]\t0x%08lx\n", vcpu, ctl);
	}

	if (!error && (get_vmcs_gla || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu,
					  VMCS_GUEST_LINEAR_ADDRESS, &u64);
		if (error == 0)
			printf("gla[%d]\t\t0x%016lx\n", vcpu, u64);
	}

	if (!error && (get_vmcs_gpa || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu,
					  VMCS_GUEST_PHYSICAL_ADDRESS, &u64);
		if (error == 0)
			printf("gpa[%d]\t\t0x%016lx\n", vcpu, u64);
	}

	if (!error && (get_vmcs_entry_interruption_info || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_ENTRY_INTR_INFO,&u64);
		if (error == 0) {
			printf("entry_interruption_info[%d]\t0x%08lx\n",
				vcpu, u64);
		}
	}

	if (!error && (get_eptp || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_EPTP, &eptp);
		if (error == 0)
			printf("eptp[%d]\t\t0x%016lx\n", vcpu, eptp);
	}

	if (!error && (get_exception_bitmap || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_EXCEPTION_BITMAP,
					  &bm);
		if (error == 0)
			printf("exception_bitmap[%d]\t0x%08lx\n", vcpu, bm);
	}

	if (!error && (get_io_bitmap || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_IO_BITMAP_A, &bm);
		if (error == 0)
			printf("io_bitmap_a[%d]\t0x%08lx\n", vcpu, bm);
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_IO_BITMAP_B, &bm);
		if (error == 0)
			printf("io_bitmap_b[%d]\t0x%08lx\n", vcpu, bm);
	}

	if (!error && (get_tsc_offset || get_all)) {
		uint64_t tscoff;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_TSC_OFFSET, &tscoff);
		if (error == 0)
			printf("tsc_offset[%d]\t0x%016lx\n", vcpu, tscoff);
	}

	if (!error && (get_cr0_mask || get_all)) {
		uint64_t cr0mask;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_CR0_MASK, &cr0mask);
		if (error == 0)
			printf("cr0_mask[%d]\t\t0x%016lx\n", vcpu, cr0mask);
	}

	if (!error && (get_cr0_shadow || get_all)) {
		uint64_t cr0shadow;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_CR0_SHADOW,
					  &cr0shadow);
		if (error == 0)
			printf("cr0_shadow[%d]\t\t0x%016lx\n", vcpu, cr0shadow);
	}

	if (!error && (get_cr4_mask || get_all)) {
		uint64_t cr4mask;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_CR4_MASK, &cr4mask);
		if (error == 0)
			printf("cr4_mask[%d]\t\t0x%016lx\n", vcpu, cr4mask);
	}

	if (!error && (get_cr4_shadow || get_all)) {
		uint64_t cr4shadow;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_CR4_SHADOW,
					  &cr4shadow);
		if (error == 0)
			printf("cr4_shadow[%d]\t\t0x%016lx\n", vcpu, cr4shadow);
	}
	
	if (!error && (get_cr3_targets || get_all)) {
		uint64_t target_count, target_addr;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_CR3_TARGET_COUNT,
					  &target_count);
		if (error == 0) {
			printf("cr3_target_count[%d]\t0x%08lx\n",
				vcpu, target_count);
		}

		error = vm_get_vmcs_field(ctx, vcpu, VMCS_CR3_TARGET0,
					  &target_addr);
		if (error == 0) {
			printf("cr3_target0[%d]\t\t0x%016lx\n",
				vcpu, target_addr);
		}

		error = vm_get_vmcs_field(ctx, vcpu, VMCS_CR3_TARGET1,
					  &target_addr);
		if (error == 0) {
			printf("cr3_target1[%d]\t\t0x%016lx\n",
				vcpu, target_addr);
		}

		error = vm_get_vmcs_field(ctx, vcpu, VMCS_CR3_TARGET2,
					  &target_addr);
		if (error == 0) {
			printf("cr3_target2[%d]\t\t0x%016lx\n",
				vcpu, target_addr);
		}

		error = vm_get_vmcs_field(ctx, vcpu, VMCS_CR3_TARGET3,
					  &target_addr);
		if (error == 0) {
			printf("cr3_target3[%d]\t\t0x%016lx\n",
				vcpu, target_addr);
		}
	}

	if (!error && (get_apic_access_addr || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_APIC_ACCESS, &addr);
		if (error == 0)
			printf("apic_access_addr[%d]\t0x%016lx\n", vcpu, addr);
	}

	if (!error && (get_virtual_apic_addr || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_VIRTUAL_APIC, &addr);
		if (error == 0)
			printf("virtual_apic_addr[%d]\t0x%016lx\n", vcpu, addr);
	}

	if (!error && (get_tpr_threshold || get_all)) {
		uint64_t threshold;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_TPR_THRESHOLD,
					  &threshold);
		if (error == 0)
			printf("tpr_threshold[%d]\t0x%08lx\n", vcpu, threshold);
	}

	if (!error && (get_msr_bitmap_address || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_MSR_BITMAP, &addr);
		if (error == 0)
			printf("msr_bitmap[%d]\t\t0x%016lx\n", vcpu, addr);
	}

	if (!error && (get_msr_bitmap || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_MSR_BITMAP, &addr);
		if (error == 0)
			error = dump_vmcs_msr_bitmap(vcpu, addr);
	}

	if (!error && (get_vpid || get_all)) {
		uint64_t vpid;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_VPID, &vpid);
		if (error == 0)
			printf("vpid[%d]\t\t0x%04lx\n", vcpu, vpid);
	}
	
	if (!error && (get_ple_window || get_all)) {
		uint64_t window;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_PLE_WINDOW, &window);
		if (error == 0)
			printf("ple_window[%d]\t\t0x%08lx\n", vcpu, window);
	}

	if (!error && (get_ple_gap || get_all)) {
		uint64_t gap;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_PLE_GAP, &gap);
		if (error == 0)
			printf("ple_gap[%d]\t\t0x%08lx\n", vcpu, gap);
	}

	if (!error && (get_inst_err || get_all)) {
		uint64_t insterr;
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_INSTRUCTION_ERROR,
					  &insterr);
		if (error == 0) {
			printf("instruction_error[%d]\t0x%08lx\n",
				vcpu, insterr);
		}
	}

	if (!error && (get_exit_ctls || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_EXIT_CTLS, &ctl);
		if (error == 0)
			printf("exit_ctls[%d]\t\t0x%08lx\n", vcpu, ctl);
	}

	if (!error && (get_entry_ctls || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_ENTRY_CTLS, &ctl);
		if (error == 0)
			printf("entry_ctls[%d]\t\t0x%08lx\n", vcpu, ctl);
	}

	if (!error && (get_host_pat || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_HOST_IA32_PAT, &pat);
		if (error == 0)
			printf("host_pat[%d]\t\t0x%016lx\n", vcpu, pat);
	}

	if (!error && (get_guest_pat || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_GUEST_IA32_PAT, &pat);
		if (error == 0)
			printf("guest_pat[%d]\t\t0x%016lx\n", vcpu, pat);
	}

	if (!error && (get_host_cr0 || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_HOST_CR0, &cr0);
		if (error == 0)
			printf("host_cr0[%d]\t\t0x%016lx\n", vcpu, cr0);
	}

	if (!error && (get_host_cr3 || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_HOST_CR3, &cr3);
		if (error == 0)
			printf("host_cr3[%d]\t\t0x%016lx\n", vcpu, cr3);
	}

	if (!error && (get_host_cr4 || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_HOST_CR4, &cr4);
		if (error == 0)
			printf("host_cr4[%d]\t\t0x%016lx\n", vcpu, cr4);
	}

	if (!error && (get_host_rip || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_HOST_RIP, &rip);
		if (error == 0)
			printf("host_rip[%d]\t\t0x%016lx\n", vcpu, rip);
	}

	if (!error && (get_host_rsp || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_HOST_RSP, &rsp);
		if (error == 0)
			printf("host_rsp[%d]\t\t0x%016lx\n", vcpu, rsp);
	}

	if (!error && (get_guest_sysenter || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu,
					  VMCS_GUEST_IA32_SYSENTER_CS, &cs);
		if (error == 0)
			printf("guest_sysenter_cs[%d]\t0x%08lx\n", vcpu, cs);

		error = vm_get_vmcs_field(ctx, vcpu,
					  VMCS_GUEST_IA32_SYSENTER_ESP, &rsp);
		if (error == 0)
			printf("guest_sysenter_sp[%d]\t0x%016lx\n", vcpu, rsp);
		error = vm_get_vmcs_field(ctx, vcpu,
					  VMCS_GUEST_IA32_SYSENTER_EIP, &rip);
		if (error == 0)
			printf("guest_sysenter_ip[%d]\t0x%016lx\n", vcpu, rip);
	}

	if (!error && (get_vmcs_link || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_LINK_POINTER, &addr);
		if (error == 0)
			printf("vmcs_pointer[%d]\t0x%016lx\n", vcpu, addr);
	}

	if (!error && (get_vmcs_exit_reason || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_EXIT_REASON, &u64);
		if (error == 0)
			printf("vmcs_exit_reason[%d]\t0x%016lx\n", vcpu, u64);
	}

	if (!error && (get_vmcs_exit_qualification || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_EXIT_QUALIFICATION,
					  &u64);
		if (error == 0)
			printf("vmcs_exit_qualification[%d]\t0x%016lx\n",
				vcpu, u64);
	}

	if (!error && (get_vmcs_exit_interruption_info || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_EXIT_INTR_INFO, &u64);
		if (error == 0) {
			printf("vmcs_exit_interruption_info[%d]\t0x%08lx\n",
				vcpu, u64);
		}
	}

	if (!error && (get_vmcs_exit_interruption_error || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu, VMCS_EXIT_INTR_ERRCODE,
		    &u64);
		if (error == 0) {
			printf("vmcs_exit_interruption_error[%d]\t0x%08lx\n",
				vcpu, u64);
		}
	}

	if (!error && (get_vmcs_interruptibility || get_all)) {
		error = vm_get_vmcs_field(ctx, vcpu,
					  VMCS_GUEST_INTERRUPTIBILITY, &u64);
		if (error == 0) {
			printf("vmcs_guest_interruptibility[%d]\t0x%08lx\n",
				vcpu, u64);
		}
	}

	if (!error && setcap) {
		int captype;
		captype = vm_capability_name2type(capname);
		error = vm_set_capability(ctx, vcpu, captype, capval);
		if (error != 0 && errno == ENOENT)
			printf("Capability \"%s\" is not available\n", capname);
	}

	if (!error && get_gpa_pmap) {
		error = vm_get_gpa_pmap(ctx, gpa_pmap, pteval, &ptenum);
		if (error == 0) {
			printf("gpa %#lx:", gpa_pmap);
			pte = &pteval[0];
			while (ptenum-- > 0)
				printf(" %#lx", *pte++);
			printf("\n");
		}
	}

	if (!error && (getcap || get_all)) {
		int captype, val, getcaptype;

		if (getcap && capname)
			getcaptype = vm_capability_name2type(capname);
		else
			getcaptype = -1;

		for (captype = 0; captype < VM_CAP_MAX; captype++) {
			if (getcaptype >= 0 && captype != getcaptype)
				continue;
			error = vm_get_capability(ctx, vcpu, captype, &val);
			if (error == 0) {
				printf("Capability \"%s\" is %s on vcpu %d\n",
					vm_capability_type2name(captype),
					val ? "set" : "not set", vcpu);
			} else if (errno == ENOENT) {
				error = 0;
				printf("Capability \"%s\" is not available\n",
					vm_capability_type2name(captype));
			} else {
				break;
			}
		}
	}

	if (!error && (get_active_cpus || get_all)) {
		error = vm_active_cpus(ctx, &cpus);
		if (!error)
			print_cpus("active cpus", &cpus);
	}

	if (!error && (get_suspended_cpus || get_all)) {
		error = vm_suspended_cpus(ctx, &cpus);
		if (!error)
			print_cpus("suspended cpus", &cpus);
	}

	if (!error && (get_intinfo || get_all)) {
		error = vm_get_intinfo(ctx, vcpu, &info[0], &info[1]);
		if (!error) {
			print_intinfo("pending", info[0]);
			print_intinfo("current", info[1]);
		}
	}

	if (!error && run) {
		error = vm_get_register(ctx, vcpu, VM_REG_GUEST_RIP, &rip);
		assert(error == 0);

		error = vm_run(ctx, vcpu, rip, &vmexit);
		if (error == 0)
			dump_vm_run_exitcode(&vmexit, vcpu);
		else
			printf("vm_run error %d\n", error);
	}

	if (!error && force_reset)
		error = vm_suspend(ctx, VM_SUSPEND_RESET);

	if (!error && force_poweroff)
		error = vm_suspend(ctx, VM_SUSPEND_POWEROFF);

	if (error)
		printf("errno = %d\n", errno);

	if (!error && destroy)
		vm_destroy(ctx);

	exit(error);
}
