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

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <machine/atomic.h>
#include <machine/segments.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <libgen.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <pthread_np.h>
#include <sysexits.h>

#include <machine/vmm.h>
#include <vmmapi.h>

#include "bhyverun.h"
#include "acpi.h"
#include "inout.h"
#include "dbgport.h"
#include "legacy_irq.h"
#include "mem.h"
#include "mevent.h"
#include "mptbl.h"
#include "pci_emul.h"
#include "pci_lpc.h"
#include "xmsr.h"
#include "spinup_ap.h"
#include "rtc.h"

#define GUEST_NIO_PORT		0x488	/* guest upcalls via i/o port */

#define	VMEXIT_SWITCH		0	/* force vcpu switch in mux mode */
#define	VMEXIT_CONTINUE		1	/* continue from next instruction */
#define	VMEXIT_RESTART		2	/* restart current instruction */
#define	VMEXIT_ABORT		3	/* abort the vm run loop */
#define	VMEXIT_RESET		4	/* guest machine has reset */
#define	VMEXIT_POWEROFF		5	/* guest machine has powered off */

#define MB		(1024UL * 1024)
#define GB		(1024UL * MB)

typedef int (*vmexit_handler_t)(struct vmctx *, struct vm_exit *, int *vcpu);

char *vmname;

int guest_ncpus;

static int pincpu = -1;
static int guest_vmexit_on_hlt, guest_vmexit_on_pause, disable_x2apic;
static int virtio_msix = 1;

static int strictio;
static int strictmsr = 1;

static int acpi;

static char *progname;
static const int BSP = 0;

static int cpumask;

static void vm_loop(struct vmctx *ctx, int vcpu, uint64_t rip);

struct vm_exit vmexit[VM_MAXCPU];

struct bhyvestats {
        uint64_t        vmexit_bogus;
        uint64_t        vmexit_bogus_switch;
        uint64_t        vmexit_hlt;
        uint64_t        vmexit_pause;
        uint64_t        vmexit_mtrap;
        uint64_t        vmexit_inst_emul;
        uint64_t        cpu_switch_rotate;
        uint64_t        cpu_switch_direct;
        int             io_reset;
} stats;

struct mt_vmm_info {
	pthread_t	mt_thr;
	struct vmctx	*mt_ctx;
	int		mt_vcpu;	
} mt_vmm_info[VM_MAXCPU];

static void
usage(int code)
{

        fprintf(stderr,
                "Usage: %s [-aehwAHIPW] [-g <gdb port>] [-s <pci>] [-S <pci>]\n"
		"       %*s [-c vcpus] [-p pincpu] [-m mem] [-l <lpc>] <vm>\n"
		"       -a: local apic is in XAPIC mode (default is X2APIC)\n"
		"       -A: create an ACPI table\n"
		"       -g: gdb port\n"
		"       -c: # cpus (default 1)\n"
		"       -p: pin vcpu 'n' to host cpu 'pincpu + n'\n"
		"       -H: vmexit from the guest on hlt\n"
		"       -P: vmexit from the guest on pause\n"
		"       -W: force virtio to use single-vector MSI\n"
		"       -e: exit on unhandled I/O access\n"
		"       -h: help\n"
		"       -s: <slot,driver,configinfo> PCI slot config\n"
		"       -S: <slot,driver,configinfo> legacy PCI slot config\n"
		"       -l: LPC device configuration\n"
		"       -m: memory size in MB\n"
		"       -w: ignore unimplemented MSRs\n",
		progname, (int)strlen(progname), "");

	exit(code);
}

void *
paddr_guest2host(struct vmctx *ctx, uintptr_t gaddr, size_t len)
{

	return (vm_map_gpa(ctx, gaddr, len));
}

int
fbsdrun_disable_x2apic(void)
{

	return (disable_x2apic);
}

int
fbsdrun_vmexit_on_pause(void)
{

	return (guest_vmexit_on_pause);
}

int
fbsdrun_vmexit_on_hlt(void)
{

	return (guest_vmexit_on_hlt);
}

int
fbsdrun_virtio_msix(void)
{

	return (virtio_msix);
}

static void *
fbsdrun_start_thread(void *param)
{
	char tname[MAXCOMLEN + 1];
	struct mt_vmm_info *mtp;
	int vcpu;

	mtp = param;
	vcpu = mtp->mt_vcpu;

	snprintf(tname, sizeof(tname), "vcpu %d", vcpu);
	pthread_set_name_np(mtp->mt_thr, tname);

	vm_loop(mtp->mt_ctx, vcpu, vmexit[vcpu].rip);

	/* not reached */
	exit(1);
	return (NULL);
}

void
fbsdrun_addcpu(struct vmctx *ctx, int vcpu, uint64_t rip)
{
	int error;

	if (cpumask & (1 << vcpu)) {
		fprintf(stderr, "addcpu: attempting to add existing cpu %d\n",
		    vcpu);
		exit(1);
	}

	atomic_set_int(&cpumask, 1 << vcpu);

	/*
	 * Set up the vmexit struct to allow execution to start
	 * at the given RIP
	 */
	vmexit[vcpu].rip = rip;
	vmexit[vcpu].inst_length = 0;

	mt_vmm_info[vcpu].mt_ctx = ctx;
	mt_vmm_info[vcpu].mt_vcpu = vcpu;

	error = pthread_create(&mt_vmm_info[vcpu].mt_thr, NULL,
	    fbsdrun_start_thread, &mt_vmm_info[vcpu]);
	assert(error == 0);
}

static int
fbsdrun_deletecpu(struct vmctx *ctx, int vcpu)
{

	if ((cpumask & (1 << vcpu)) == 0) {
		fprintf(stderr, "addcpu: attempting to delete unknown cpu %d\n",
		    vcpu);
		exit(1);
	}

	atomic_clear_int(&cpumask, 1 << vcpu);
	return (cpumask == 0);
}

static int
vmexit_catch_reset(void)
{
        stats.io_reset++;
        return (VMEXIT_RESET);
}

static int
vmexit_catch_inout(void)
{
	return (VMEXIT_ABORT);
}

static int
vmexit_handle_notify(struct vmctx *ctx, struct vm_exit *vme, int *pvcpu,
		     uint32_t eax)
{
#if BHYVE_DEBUG
	/*
	 * put guest-driven debug here
	 */
#endif
        return (VMEXIT_CONTINUE);
}

static int
vmexit_inout(struct vmctx *ctx, struct vm_exit *vme, int *pvcpu)
{
	int error;
	int bytes, port, in, out;
	uint32_t eax;
	int vcpu;

	vcpu = *pvcpu;

	port = vme->u.inout.port;
	bytes = vme->u.inout.bytes;
	eax = vme->u.inout.eax;
	in = vme->u.inout.in;
	out = !in;

	/* We don't deal with these */
	if (vme->u.inout.string || vme->u.inout.rep)
		return (VMEXIT_ABORT);

	/* Special case of guest reset */
	if (out && port == 0x64 && (uint8_t)eax == 0xFE)
		return (vmexit_catch_reset());

        /* Extra-special case of host notifications */
        if (out && port == GUEST_NIO_PORT)
                return (vmexit_handle_notify(ctx, vme, pvcpu, eax));

	error = emulate_inout(ctx, vcpu, in, port, bytes, &eax, strictio);
	if (error == INOUT_OK && in)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_RAX, eax);

	switch (error) {
	case INOUT_OK:
		return (VMEXIT_CONTINUE);
	case INOUT_RESET:
		return (VMEXIT_RESET);
	case INOUT_POWEROFF:
		return (VMEXIT_POWEROFF);
	default:
		fprintf(stderr, "Unhandled %s%c 0x%04x\n",
			in ? "in" : "out",
			bytes == 1 ? 'b' : (bytes == 2 ? 'w' : 'l'), port);
		return (vmexit_catch_inout());
	}
}

static int
vmexit_rdmsr(struct vmctx *ctx, struct vm_exit *vme, int *pvcpu)
{
	uint64_t val;
	uint32_t eax, edx;
	int error;

	val = 0;
	error = emulate_rdmsr(ctx, *pvcpu, vme->u.msr.code, &val);
	if (error != 0) {
		fprintf(stderr, "rdmsr to register %#x on vcpu %d\n",
		    vme->u.msr.code, *pvcpu);
		if (strictmsr)
			return (VMEXIT_ABORT);
	}

	eax = val;
	error = vm_set_register(ctx, *pvcpu, VM_REG_GUEST_RAX, eax);
	assert(error == 0);

	edx = val >> 32;
	error = vm_set_register(ctx, *pvcpu, VM_REG_GUEST_RDX, edx);
	assert(error == 0);

	return (VMEXIT_CONTINUE);
}

static int
vmexit_wrmsr(struct vmctx *ctx, struct vm_exit *vme, int *pvcpu)
{
	int error;

	error = emulate_wrmsr(ctx, *pvcpu, vme->u.msr.code, vme->u.msr.wval);
	if (error != 0) {
		fprintf(stderr, "wrmsr to register %#x(%#lx) on vcpu %d\n",
		    vme->u.msr.code, vme->u.msr.wval, *pvcpu);
		if (strictmsr)
			return (VMEXIT_ABORT);
	}
	return (VMEXIT_CONTINUE);
}

static int
vmexit_spinup_ap(struct vmctx *ctx, struct vm_exit *vme, int *pvcpu)
{
	int newcpu;
	int retval = VMEXIT_CONTINUE;

	newcpu = spinup_ap(ctx, *pvcpu,
			   vme->u.spinup_ap.vcpu, vme->u.spinup_ap.rip);

	return (retval);
}

static int
vmexit_spindown_cpu(struct vmctx *ctx, struct vm_exit *vme, int *pvcpu)
{
	int lastcpu;

	lastcpu = fbsdrun_deletecpu(ctx, *pvcpu);
	if (!lastcpu)
		pthread_exit(NULL);
	return (vmexit_catch_reset());
}

static int
vmexit_vmx(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{

	fprintf(stderr, "vm exit[%d]\n", *pvcpu);
	fprintf(stderr, "\treason\t\tVMX\n");
	fprintf(stderr, "\trip\t\t0x%016lx\n", vmexit->rip);
	fprintf(stderr, "\tinst_length\t%d\n", vmexit->inst_length);
	fprintf(stderr, "\tstatus\t\t%d\n", vmexit->u.vmx.status);
	fprintf(stderr, "\texit_reason\t%u\n", vmexit->u.vmx.exit_reason);
	fprintf(stderr, "\tqualification\t0x%016lx\n",
	    vmexit->u.vmx.exit_qualification);
	fprintf(stderr, "\tinst_type\t\t%d\n", vmexit->u.vmx.inst_type);
	fprintf(stderr, "\tinst_error\t\t%d\n", vmexit->u.vmx.inst_error);

	return (VMEXIT_ABORT);
}

static int
vmexit_bogus(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{

	stats.vmexit_bogus++;

	return (VMEXIT_RESTART);
}

static int
vmexit_hlt(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{

	stats.vmexit_hlt++;

	/*
	 * Just continue execution with the next instruction. We use
	 * the HLT VM exit as a way to be friendly with the host
	 * scheduler.
	 */
	return (VMEXIT_CONTINUE);
}

static int
vmexit_pause(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{

	stats.vmexit_pause++;

	return (VMEXIT_CONTINUE);
}

static int
vmexit_mtrap(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{

	stats.vmexit_mtrap++;

	return (VMEXIT_RESTART);
}

static int
vmexit_inst_emul(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{
	int err;
	stats.vmexit_inst_emul++;

	err = emulate_mem(ctx, *pvcpu, vmexit->u.inst_emul.gpa,
			  &vmexit->u.inst_emul.vie);

	if (err) {
		if (err == EINVAL) {
			fprintf(stderr,
			    "Failed to emulate instruction at 0x%lx\n", 
			    vmexit->rip);
		} else if (err == ESRCH) {
			fprintf(stderr, "Unhandled memory access to 0x%lx\n",
			    vmexit->u.inst_emul.gpa);
		}

		return (VMEXIT_ABORT);
	}

	return (VMEXIT_CONTINUE);
}

static vmexit_handler_t handler[VM_EXITCODE_MAX] = {
	[VM_EXITCODE_INOUT]  = vmexit_inout,
	[VM_EXITCODE_VMX]    = vmexit_vmx,
	[VM_EXITCODE_BOGUS]  = vmexit_bogus,
	[VM_EXITCODE_RDMSR]  = vmexit_rdmsr,
	[VM_EXITCODE_WRMSR]  = vmexit_wrmsr,
	[VM_EXITCODE_MTRAP]  = vmexit_mtrap,
	[VM_EXITCODE_INST_EMUL] = vmexit_inst_emul,
	[VM_EXITCODE_SPINUP_AP] = vmexit_spinup_ap,
	[VM_EXITCODE_SPINDOWN_CPU] = vmexit_spindown_cpu,
};

static void
vm_loop(struct vmctx *ctx, int vcpu, uint64_t rip)
{
	cpuset_t mask;
	int error, rc, prevcpu;
	enum vm_exitcode exitcode;

	if (pincpu >= 0) {
		CPU_ZERO(&mask);
		CPU_SET(pincpu + vcpu, &mask);
		error = pthread_setaffinity_np(pthread_self(),
					       sizeof(mask), &mask);
		assert(error == 0);
	}

	while (1) {
		error = vm_run(ctx, vcpu, rip, &vmexit[vcpu]);
		if (error != 0)
			break;

		prevcpu = vcpu;

		exitcode = vmexit[vcpu].exitcode;
		if (exitcode >= VM_EXITCODE_MAX || handler[exitcode] == NULL) {
			fprintf(stderr, "vm_loop: unexpected exitcode 0x%x\n",
			    exitcode);
			exit(1);
		}

                rc = (*handler[exitcode])(ctx, &vmexit[vcpu], &vcpu);

		switch (rc) {
		case VMEXIT_CONTINUE:
                        rip = vmexit[vcpu].rip + vmexit[vcpu].inst_length;
			break;
		case VMEXIT_RESTART:
                        rip = vmexit[vcpu].rip;
			break;
		case VMEXIT_RESET:
			exit(0);
		default:
			exit(1);
		}
	}
	fprintf(stderr, "vm_run error %d, errno %d\n", error, errno);
}

static int
num_vcpus_allowed(struct vmctx *ctx)
{
	int tmp, error;

	error = vm_get_capability(ctx, BSP, VM_CAP_UNRESTRICTED_GUEST, &tmp);

	/*
	 * The guest is allowed to spinup more than one processor only if the
	 * UNRESTRICTED_GUEST capability is available.
	 */
	if (error == 0)
		return (VM_MAXCPU);
	else
		return (1);
}

void
fbsdrun_set_capabilities(struct vmctx *ctx, int cpu)
{
	int err, tmp;

	if (fbsdrun_vmexit_on_hlt()) {
		err = vm_get_capability(ctx, cpu, VM_CAP_HALT_EXIT, &tmp);
		if (err < 0) {
			fprintf(stderr, "VM exit on HLT not supported\n");
			exit(1);
		}
		vm_set_capability(ctx, cpu, VM_CAP_HALT_EXIT, 1);
		if (cpu == BSP)
			handler[VM_EXITCODE_HLT] = vmexit_hlt;
	}

        if (fbsdrun_vmexit_on_pause()) {
		/*
		 * pause exit support required for this mode
		 */
		err = vm_get_capability(ctx, cpu, VM_CAP_PAUSE_EXIT, &tmp);
		if (err < 0) {
			fprintf(stderr,
			    "SMP mux requested, no pause support\n");
			exit(1);
		}
		vm_set_capability(ctx, cpu, VM_CAP_PAUSE_EXIT, 1);
		if (cpu == BSP)
			handler[VM_EXITCODE_PAUSE] = vmexit_pause;
        }

	if (fbsdrun_disable_x2apic())
		err = vm_set_x2apic_state(ctx, cpu, X2APIC_DISABLED);
	else
		err = vm_set_x2apic_state(ctx, cpu, X2APIC_ENABLED);

	if (err) {
		fprintf(stderr, "Unable to set x2apic state (%d)\n", err);
		exit(1);
	}

	vm_set_capability(ctx, cpu, VM_CAP_ENABLE_INVPCID, 1);
}

int
main(int argc, char *argv[])
{
	int c, error, gdb_port, err, bvmcons;
	int max_vcpus;
	struct vmctx *ctx;
	uint64_t rip;
	size_t memsize;

	bvmcons = 0;
	progname = basename(argv[0]);
	gdb_port = 0;
	guest_ncpus = 1;
	memsize = 256 * MB;

	while ((c = getopt(argc, argv, "abehwAHIPWp:g:c:s:S:m:l:")) != -1) {
		switch (c) {
		case 'a':
			disable_x2apic = 1;
			break;
		case 'A':
			acpi = 1;
			break;
		case 'b':
			bvmcons = 1;
			break;
		case 'p':
			pincpu = atoi(optarg);
			break;
                case 'c':
			guest_ncpus = atoi(optarg);
			break;
		case 'g':
			gdb_port = atoi(optarg);
			break;
		case 'l':
			if (lpc_device_parse(optarg) != 0) {
				errx(EX_USAGE, "invalid lpc device "
				    "configuration '%s'", optarg);
			}
			break;
		case 's':
			if (pci_parse_slot(optarg, 0) != 0)
				exit(1);
			else
				break;
		case 'S':
			if (pci_parse_slot(optarg, 1) != 0)
				exit(1);
			else
				break;
                case 'm':
			error = vm_parse_memsize(optarg, &memsize);
			if (error)
				errx(EX_USAGE, "invalid memsize '%s'", optarg);
			break;
		case 'H':
			guest_vmexit_on_hlt = 1;
			break;
		case 'I':
			/*
			 * The "-I" option was used to add an ioapic to the
			 * virtual machine.
			 *
			 * An ioapic is now provided unconditionally for each
			 * virtual machine and this option is now deprecated.
			 */
			break;
		case 'P':
			guest_vmexit_on_pause = 1;
			break;
		case 'e':
			strictio = 1;
			break;
		case 'w':
			strictmsr = 0;
			break;
		case 'W':
			virtio_msix = 0;
			break;
		case 'h':
			usage(0);			
		default:
			usage(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage(1);

	vmname = argv[0];

	ctx = vm_open(vmname);
	if (ctx == NULL) {
		perror("vm_open");
		exit(1);
	}

	max_vcpus = num_vcpus_allowed(ctx);
	if (guest_ncpus > max_vcpus) {
		fprintf(stderr, "%d vCPUs requested but only %d available\n",
			guest_ncpus, max_vcpus);
		exit(1);
	}

	fbsdrun_set_capabilities(ctx, BSP);

	err = vm_setup_memory(ctx, memsize, VM_MMAP_ALL);
	if (err) {
		fprintf(stderr, "Unable to setup memory (%d)\n", err);
		exit(1);
	}

	init_mem();
	init_inout();
	legacy_irq_init();

	rtc_init(ctx);

	/*
	 * Exit if a device emulation finds an error in it's initilization
	 */
	if (init_pci(ctx) != 0)
		exit(1);

	if (gdb_port != 0)
		init_dbgport(gdb_port);

	if (bvmcons)
		init_bvmcons();

	error = vm_get_register(ctx, BSP, VM_REG_GUEST_RIP, &rip);
	assert(error == 0);

	/*
	 * build the guest tables, MP etc.
	 */
	mptable_build(ctx, guest_ncpus);

	if (acpi) {
		error = acpi_build(ctx, guest_ncpus);
		assert(error == 0);
	}

	/*
	 * Change the proc title to include the VM name.
	 */
	setproctitle("%s", vmname); 
	
	/*
	 * Add CPU 0
	 */
	fbsdrun_addcpu(ctx, BSP, rip);

	/*
	 * Head off to the main event dispatch loop
	 */
	mevent_dispatch();

	exit(1);
}
