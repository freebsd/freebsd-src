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

#include <machine/segments.h>

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <pthread_np.h>

#include <machine/vmm.h>
#include <vmmapi.h>

#include "bhyverun.h"
#include "acpi.h"
#include "inout.h"
#include "dbgport.h"
#include "mem.h"
#include "mevent.h"
#include "mptbl.h"
#include "pci_emul.h"
#include "xmsr.h"
#include "ioapic.h"
#include "spinup_ap.h"

#define	DEFAULT_GUEST_HZ	100
#define	DEFAULT_GUEST_TSLICE	200

#define GUEST_NIO_PORT		0x488	/* guest upcalls via i/o port */

#define	VMEXIT_SWITCH		0	/* force vcpu switch in mux mode */
#define	VMEXIT_CONTINUE		1	/* continue from next instruction */
#define	VMEXIT_RESTART		2	/* restart current instruction */
#define	VMEXIT_ABORT		3	/* abort the vm run loop */
#define	VMEXIT_RESET		4	/* guest machine has reset */

#define MB		(1024UL * 1024)
#define GB		(1024UL * MB)

typedef int (*vmexit_handler_t)(struct vmctx *, struct vm_exit *, int *vcpu);

int guest_tslice = DEFAULT_GUEST_TSLICE;
int guest_hz = DEFAULT_GUEST_HZ;
char *vmname;

int guest_ncpus;

static int pincpu = -1;
static int guest_vcpu_mux;
static int guest_vmexit_on_hlt, guest_vmexit_on_pause, disable_x2apic;

static int foundcpus;

static int strictio;

static int acpi;

static char *progname;
static const int BSP = 0;

static int cpumask;

static void vm_loop(struct vmctx *ctx, int vcpu, uint64_t rip);

struct vm_exit vmexit[VM_MAXCPU];

struct fbsdstats {
        uint64_t        vmexit_bogus;
        uint64_t        vmexit_bogus_switch;
        uint64_t        vmexit_hlt;
        uint64_t        vmexit_pause;
        uint64_t        vmexit_mtrap;
        uint64_t        vmexit_paging;
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
                "Usage: %s [-aehABHIP][-g <gdb port>][-z <hz>][-s <pci>]"
		"[-S <pci>][-p pincpu][-n <pci>][-m lowmem][-M highmem]"
		" <vmname>\n"
		"       -a: local apic is in XAPIC mode (default is X2APIC)\n"
		"       -A: create an ACPI table\n"
		"       -g: gdb port (default is %d and 0 means don't open)\n"
		"       -c: # cpus (default 1)\n"
		"       -p: pin vcpu 'n' to host cpu 'pincpu + n'\n"
		"       -B: inject breakpoint exception on vm entry\n"
		"       -H: vmexit from the guest on hlt\n"
		"       -I: present an ioapic to the guest\n"
		"       -P: vmexit from the guest on pause\n"
		"	-e: exit on unhandled i/o access\n"
		"       -h: help\n"
		"       -z: guest hz (default is %d)\n"
		"       -s: <slot,driver,configinfo> PCI slot config\n"
		"       -S: <slot,driver,configinfo> legacy PCI slot config\n"
		"       -m: memory size in MB\n"
		"       -x: mux vcpus to 1 hcpu\n"
		"       -t: mux vcpu timeslice hz (default %d)\n",
		progname, DEFAULT_GDB_PORT, DEFAULT_GUEST_HZ,
		DEFAULT_GUEST_TSLICE);
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
fbsdrun_muxed(void)
{

	return (guest_vcpu_mux);
}

static void *
fbsdrun_start_thread(void *param)
{
	char tname[MAXCOMLEN + 1];
	struct mt_vmm_info *mtp;
	int vcpu;

	mtp = param;
	vcpu = mtp->mt_vcpu;

	snprintf(tname, sizeof(tname), "%s vcpu %d", vmname, vcpu);
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

	cpumask |= 1 << vcpu;
	foundcpus++;

	/*
	 * Set up the vmexit struct to allow execution to start
	 * at the given RIP
	 */
	vmexit[vcpu].rip = rip;
	vmexit[vcpu].inst_length = 0;

	if (vcpu == BSP || !guest_vcpu_mux){
		mt_vmm_info[vcpu].mt_ctx = ctx;
		mt_vmm_info[vcpu].mt_vcpu = vcpu;
	
		error = pthread_create(&mt_vmm_info[vcpu].mt_thr, NULL,
				fbsdrun_start_thread, &mt_vmm_info[vcpu]);
		assert(error == 0);
	}
}

static int
fbsdrun_get_next_cpu(int curcpu)
{

	/*
	 * Get the next available CPU. Assumes they arrive
	 * in ascending order with no gaps.
	 */
	return ((curcpu + 1) % foundcpus);
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
#if PG_DEBUG /* put all types of debug here */
        if (eax == 0) {
		pause_noswitch = 1;
	} else if (eax == 1) {
		pause_noswitch = 0;
	} else {
		pause_noswitch = 0;
		if (eax == 5) {
			vm_set_capability(ctx, *pvcpu, VM_CAP_MTRAP_EXIT, 1);
		}
	}
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
	if (error == 0 && in)
		error = vm_set_register(ctx, vcpu, VM_REG_GUEST_RAX, eax);

	if (error == 0)
		return (VMEXIT_CONTINUE);
	else {
		fprintf(stderr, "Unhandled %s%c 0x%04x\n",
			in ? "in" : "out",
			bytes == 1 ? 'b' : (bytes == 2 ? 'w' : 'l'), port);
		return (vmexit_catch_inout());
	}
}

static int
vmexit_rdmsr(struct vmctx *ctx, struct vm_exit *vme, int *pvcpu)
{
	fprintf(stderr, "vm exit rdmsr 0x%x, cpu %d\n", vme->u.msr.code,
	    *pvcpu);
	return (VMEXIT_ABORT);
}

static int
vmexit_wrmsr(struct vmctx *ctx, struct vm_exit *vme, int *pvcpu)
{
	int newcpu;
	int retval = VMEXIT_CONTINUE;

	newcpu = emulate_wrmsr(ctx, *pvcpu, vme->u.msr.code,vme->u.msr.wval);

	if (guest_vcpu_mux && *pvcpu != newcpu) {
                retval = VMEXIT_SWITCH;
                *pvcpu = newcpu;
        }
        
        return (retval);
}

static int
vmexit_spinup_ap(struct vmctx *ctx, struct vm_exit *vme, int *pvcpu)
{
	int newcpu;
	int retval = VMEXIT_CONTINUE;

	newcpu = spinup_ap(ctx, *pvcpu,
			   vme->u.spinup_ap.vcpu, vme->u.spinup_ap.rip);

	if (guest_vcpu_mux && *pvcpu != newcpu) {
		retval = VMEXIT_SWITCH;
		*pvcpu = newcpu;
	}
        
	return (retval);
}

static int
vmexit_vmx(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{

	fprintf(stderr, "vm exit[%d]\n", *pvcpu);
	fprintf(stderr, "\treason\t\tVMX\n");
	fprintf(stderr, "\trip\t\t0x%016lx\n", vmexit->rip);
	fprintf(stderr, "\tinst_length\t%d\n", vmexit->inst_length);
	fprintf(stderr, "\terror\t\t%d\n", vmexit->u.vmx.error);
	fprintf(stderr, "\texit_reason\t%u\n", vmexit->u.vmx.exit_reason);
	fprintf(stderr, "\tqualification\t0x%016lx\n",
	    vmexit->u.vmx.exit_qualification);

	return (VMEXIT_ABORT);
}

static int bogus_noswitch = 1;

static int
vmexit_bogus(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{
	stats.vmexit_bogus++;

	if (!guest_vcpu_mux || guest_ncpus == 1 || bogus_noswitch) {
		return (VMEXIT_RESTART);
	} else {
		stats.vmexit_bogus_switch++;
		vmexit->inst_length = 0;
		*pvcpu = -1;		
		return (VMEXIT_SWITCH);
	}
}

static int
vmexit_hlt(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{
	stats.vmexit_hlt++;
	if (fbsdrun_muxed()) {
		*pvcpu = -1;
		return (VMEXIT_SWITCH);
	} else {
		/*
		 * Just continue execution with the next instruction. We use
		 * the HLT VM exit as a way to be friendly with the host
		 * scheduler.
		 */
		return (VMEXIT_CONTINUE);
	}
}

static int pause_noswitch;

static int
vmexit_pause(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{
	stats.vmexit_pause++;

	if (fbsdrun_muxed() && !pause_noswitch) {
		*pvcpu = -1;
		return (VMEXIT_SWITCH);
        } else {
		return (VMEXIT_CONTINUE);
	}
}

static int
vmexit_mtrap(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{
	stats.vmexit_mtrap++;

	return (VMEXIT_RESTART);
}

static int
vmexit_paging(struct vmctx *ctx, struct vm_exit *vmexit, int *pvcpu)
{
	int err;
	stats.vmexit_paging++;

	err = emulate_mem(ctx, *pvcpu, vmexit->u.paging.gpa,
			  &vmexit->u.paging.vie);

	if (err) {
		if (err == EINVAL) {
			fprintf(stderr,
			    "Failed to emulate instruction at 0x%lx\n", 
			    vmexit->rip);
		} else if (err == ESRCH) {
			fprintf(stderr, "Unhandled memory access to 0x%lx\n",
			    vmexit->u.paging.gpa);
		}

		return (VMEXIT_ABORT);
	}

	return (VMEXIT_CONTINUE);
}

static void
sigalrm(int sig)
{
	return;
}

static void
setup_timeslice(void)
{
	struct sigaction sa;
	struct itimerval itv;
	int error;

	/*
	 * Setup a realtime timer to generate a SIGALRM at a
	 * frequency of 'guest_tslice' ticks per second.
	 */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = sigalrm;
	
	error = sigaction(SIGALRM, &sa, NULL);
	assert(error == 0);

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 1000000 / guest_tslice;
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = 1000000 / guest_tslice;
	
	error = setitimer(ITIMER_REAL, &itv, NULL);
	assert(error == 0);
}

static vmexit_handler_t handler[VM_EXITCODE_MAX] = {
	[VM_EXITCODE_INOUT]  = vmexit_inout,
	[VM_EXITCODE_VMX]    = vmexit_vmx,
	[VM_EXITCODE_BOGUS]  = vmexit_bogus,
	[VM_EXITCODE_RDMSR]  = vmexit_rdmsr,
	[VM_EXITCODE_WRMSR]  = vmexit_wrmsr,
	[VM_EXITCODE_MTRAP]  = vmexit_mtrap,
	[VM_EXITCODE_PAGING] = vmexit_paging,
	[VM_EXITCODE_SPINUP_AP] = vmexit_spinup_ap,
};

static void
vm_loop(struct vmctx *ctx, int vcpu, uint64_t rip)
{
	cpuset_t mask;
	int error, rc, prevcpu;

	if (guest_vcpu_mux)
		setup_timeslice();

	if (pincpu >= 0) {
		CPU_ZERO(&mask);
		CPU_SET(pincpu + vcpu, &mask);
		error = pthread_setaffinity_np(pthread_self(),
					       sizeof(mask), &mask);
		assert(error == 0);
	}

	while (1) {
		error = vm_run(ctx, vcpu, rip, &vmexit[vcpu]);
		if (error != 0) {
			/*
			 * It is possible that 'vmmctl' or some other process
			 * has transitioned the vcpu to CANNOT_RUN state right
			 * before we tried to transition it to RUNNING.
			 *
			 * This is expected to be temporary so just retry.
			 */
			if (errno == EBUSY)
				continue;
			else
				break;
		}

		prevcpu = vcpu;
                rc = (*handler[vmexit[vcpu].exitcode])(ctx, &vmexit[vcpu],
                                                       &vcpu);		
		switch (rc) {
                case VMEXIT_SWITCH:
			assert(guest_vcpu_mux);
			if (vcpu == -1) {
				stats.cpu_switch_rotate++;
				vcpu = fbsdrun_get_next_cpu(prevcpu);
			} else {
				stats.cpu_switch_direct++;
			}
			/* fall through */
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

int
main(int argc, char *argv[])
{
	int c, error, gdb_port, inject_bkpt, tmp, err, ioapic, bvmcons;
	int max_vcpus;
	struct vmctx *ctx;
	uint64_t rip;
	size_t memsize;

	bvmcons = 0;
	inject_bkpt = 0;
	progname = basename(argv[0]);
	gdb_port = DEFAULT_GDB_PORT;
	guest_ncpus = 1;
	ioapic = 0;
	memsize = 256 * MB;

	while ((c = getopt(argc, argv, "abehABHIPxp:g:c:z:s:S:n:m:")) != -1) {
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
		case 'B':
			inject_bkpt = 1;
			break;
		case 'x':
			guest_vcpu_mux = 1;
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
		case 'z':
			guest_hz = atoi(optarg);
			break;
		case 't':
			guest_tslice = atoi(optarg);
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
			memsize = strtoul(optarg, NULL, 0) * MB;
			break;
		case 'H':
			guest_vmexit_on_hlt = 1;
			break;
		case 'I':
			ioapic = 1;
			break;
		case 'P':
			guest_vmexit_on_pause = 1;
			break;
		case 'e':
			strictio = 1;
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

	/* No need to mux if guest is uni-processor */
	if (guest_ncpus <= 1)
		guest_vcpu_mux = 0;

	/* vmexit on hlt if guest is muxed */
	if (guest_vcpu_mux) {
		guest_vmexit_on_hlt = 1;
		guest_vmexit_on_pause = 1;
	}

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

	if (fbsdrun_vmexit_on_hlt()) {
		err = vm_get_capability(ctx, BSP, VM_CAP_HALT_EXIT, &tmp);
		if (err < 0) {
			fprintf(stderr, "VM exit on HLT not supported\n");
			exit(1);
		}
		vm_set_capability(ctx, BSP, VM_CAP_HALT_EXIT, 1);
		handler[VM_EXITCODE_HLT] = vmexit_hlt;
	}

        if (fbsdrun_vmexit_on_pause()) {
		/*
		 * pause exit support required for this mode
		 */
		err = vm_get_capability(ctx, BSP, VM_CAP_PAUSE_EXIT, &tmp);
		if (err < 0) {
			fprintf(stderr,
			    "SMP mux requested, no pause support\n");
			exit(1);
		}
		vm_set_capability(ctx, BSP, VM_CAP_PAUSE_EXIT, 1);
		handler[VM_EXITCODE_PAUSE] = vmexit_pause;
        }

	if (fbsdrun_disable_x2apic())
		err = vm_set_x2apic_state(ctx, BSP, X2APIC_DISABLED);
	else
		err = vm_set_x2apic_state(ctx, BSP, X2APIC_ENABLED);

	if (err) {
		fprintf(stderr, "Unable to set x2apic state (%d)\n", err);
		exit(1);
	}

	err = vm_setup_memory(ctx, memsize, VM_MMAP_ALL);
	if (err) {
		fprintf(stderr, "Unable to setup memory (%d)\n", err);
		exit(1);
	}

	init_mem();
	init_inout();
	init_pci(ctx);
	if (ioapic)
		ioapic_init(0);

	if (gdb_port != 0)
		init_dbgport(gdb_port);

	if (bvmcons)
		init_bvmcons();

	error = vm_get_register(ctx, BSP, VM_REG_GUEST_RIP, &rip);
	assert(error == 0);

	if (inject_bkpt) {
		error = vm_inject_event(ctx, BSP, VM_HW_EXCEPTION, IDT_BP);
		assert(error == 0);
	}

	/*
	 * build the guest tables, MP etc.
	 */
	mptable_build(ctx, guest_ncpus, ioapic);

	if (acpi) {
		error = acpi_build(ctx, guest_ncpus, ioapic);
		assert(error == 0);
	}

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
