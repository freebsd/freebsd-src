/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <vmmapi.h>

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "fdt.h"
#include "mem.h"
#include "pci_emul.h"
#include "pci_irq.h"
#include "rtc_pl031.h"
#include "uart_emul.h"

/* Start of mem + 1M */
#define	FDT_BASE	0x100000
#define	FDT_SIZE	(64 * 1024)

/* Start of lowmem + 64K */
#define	UART_MMIO_BASE	0x10000
#define	UART_MMIO_SIZE	0x1000
#define	UART_INTR	32
#define	RTC_MMIO_BASE	0x11000
#define	RTC_MMIO_SIZE	0x1000
#define	RTC_INTR	33

#define	GIC_DIST_BASE		0x2f000000
#define	GIC_DIST_SIZE		0x10000
#define	GIC_REDIST_BASE		0x2f100000
#define	GIC_REDIST_SIZE(ncpu)	((ncpu) * 2 * PAGE_SIZE_64K)

#define	PCIE_INTA	34
#define	PCIE_INTB	35
#define	PCIE_INTC	36
#define	PCIE_INTD	37

void
bhyve_init_config(void)
{
	init_config();

	/* Set default values prior to option parsing. */
	set_config_bool("acpi_tables", false);
	set_config_bool("acpi_tables_in_memory", false);
	set_config_value("memory.size", "256M");
}

void
bhyve_usage(int code)
{
	const char *progname;

	progname = getprogname();

	fprintf(stderr,
	    "Usage: %s [-CDHhSW]\n"
	    "       %*s [-c [[cpus=]numcpus][,sockets=n][,cores=n][,threads=n]]\n"
	    "       %*s [-k config_file] [-m mem] [-o var=value]\n"
	    "       %*s [-p vcpu:hostcpu] [-r file] [-s pci] [-U uuid] vmname\n"
	    "       -C: include guest memory in core file\n"
	    "       -c: number of CPUs and/or topology specification\n"
	    "       -D: destroy on power-off\n"
	    "       -h: help\n"
	    "       -k: key=value flat config file\n"
	    "       -m: memory size\n"
	    "       -o: set config 'var' to 'value'\n"
	    "       -p: pin 'vcpu' to 'hostcpu'\n"
	    "       -S: guest memory cannot be swapped\n"
	    "       -s: <slot,driver,configinfo> PCI slot config\n"
	    "       -U: UUID\n"
	    "       -W: force virtio to use single-vector MSI\n",
	    progname, (int)strlen(progname), "", (int)strlen(progname), "",
	    (int)strlen(progname), "");
	exit(code);
}

void
bhyve_optparse(int argc, char **argv)
{
	const char *optstr;
	int c;

	optstr = "hCDSWk:f:o:p:c:s:m:U:";
	while ((c = getopt(argc, argv, optstr)) != -1) {
		switch (c) {
		case 'c':
			if (bhyve_topology_parse(optarg) != 0) {
				errx(EX_USAGE, "invalid cpu topology '%s'",
				    optarg);
			}
			break;
		case 'C':
			set_config_bool("memory.guest_in_core", true);
			break;
		case 'D':
			set_config_bool("destroy_on_poweroff", true);
			break;
		case 'k':
			bhyve_parse_simple_config_file(optarg);
			break;
		case 'm':
			set_config_value("memory.size", optarg);
			break;
		case 'o':
			if (!bhyve_parse_config_option(optarg)) {
				errx(EX_USAGE,
				    "invalid configuration option '%s'",
				    optarg);
			}
			break;
		case 'p':
			if (bhyve_pincpu_parse(optarg) != 0) {
				errx(EX_USAGE,
				    "invalid vcpu pinning configuration '%s'",
				    optarg);
			}
			break;
		case 's':
			if (strncmp(optarg, "help", strlen(optarg)) == 0) {
				pci_print_supported_devices();
				exit(0);
			} else if (pci_parse_slot(optarg) != 0)
				exit(4);
			else
				break;
		case 'S':
			set_config_bool("memory.wired", true);
			break;
		case 'U':
			set_config_value("uuid", optarg);
			break;
		case 'W':
			set_config_bool("virtio_msix", false);
			break;
		case 'h':
			bhyve_usage(0);
		default:
			bhyve_usage(1);
		}
	}
}

void
bhyve_init_vcpu(struct vcpu *vcpu __unused)
{
}

void
bhyve_start_vcpu(struct vcpu *vcpu, bool bsp __unused)
{
	fbsdrun_addcpu(vcpu_id(vcpu));
}

/*
 * Load the specified boot code at the beginning of high memory.
 */
static void
load_bootrom(struct vmctx *ctx, const char *path, uint64_t *elrp)
{
	struct stat sb;
	void *data, *gptr;
	vm_paddr_t loadaddr;
	off_t size;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		err(1, "open(%s)", path);
	if (fstat(fd, &sb) != 0)
		err(1, "fstat(%s)", path);

	size = sb.st_size;

	loadaddr = vm_get_highmem_base(ctx);
	gptr = vm_map_gpa(ctx, loadaddr, round_page(size));

	data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED)
		err(1, "mmap(%s)", path);
	(void)close(fd);
	memcpy(gptr, data, size);

	if (munmap(data, size) != 0)
		err(1, "munmap(%s)", path);

	*elrp = loadaddr;
}

static void
mmio_uart_intr_assert(void *arg)
{
	struct vmctx *ctx = arg;

	vm_assert_irq(ctx, UART_INTR);
}

static void
mmio_uart_intr_deassert(void *arg)
{
	struct vmctx *ctx = arg;

	vm_deassert_irq(ctx, UART_INTR);
}

static int
mmio_uart_mem_handler(struct vcpu *vcpu __unused, int dir,
    uint64_t addr, int size __unused, uint64_t *val, void *arg1, long arg2)
{
	struct uart_pl011_softc *sc = arg1;
	long reg;

	reg = (addr - arg2) >> 2;
	if (dir == MEM_F_WRITE)
		uart_pl011_write(sc, reg, *val);
	else
		*val = uart_pl011_read(sc, reg);

	return (0);
}

static bool
init_mmio_uart(struct vmctx *ctx)
{
	struct uart_pl011_softc *sc;
	struct mem_range mr;
	const char *path;
	int error;

	path = get_config_value("console");
	if (path == NULL)
		return (false);

	sc = uart_pl011_init(mmio_uart_intr_assert, mmio_uart_intr_deassert,
	    ctx);
	if (uart_pl011_tty_open(sc, path) != 0) {
		EPRINTLN("Unable to initialize backend '%s' for mmio uart",
		    path);
		assert(0);
	}

	bzero(&mr, sizeof(struct mem_range));
	mr.name = "uart";
	mr.base = UART_MMIO_BASE;
	mr.size = UART_MMIO_SIZE;
	mr.flags = MEM_F_RW;
	mr.handler = mmio_uart_mem_handler;
	mr.arg1 = sc;
	mr.arg2 = mr.base;
	error = register_mem(&mr);
	assert(error == 0);

	return (true);
}

static void
mmio_rtc_intr_assert(void *arg)
{
	struct vmctx *ctx = arg;

	vm_assert_irq(ctx, RTC_INTR);
}

static void
mmio_rtc_intr_deassert(void *arg)
{
	struct vmctx *ctx = arg;

	vm_deassert_irq(ctx, RTC_INTR);
}

static int
mmio_rtc_mem_handler(struct vcpu *vcpu __unused, int dir,
    uint64_t addr, int size __unused, uint64_t *val, void *arg1, long arg2)
{
	struct rtc_pl031_softc *sc = arg1;
	long reg;

	reg = addr - arg2;
	if (dir == MEM_F_WRITE)
		rtc_pl031_write(sc, reg, *val);
	else
		*val = rtc_pl031_read(sc, reg);

	return (0);
}

static void
init_mmio_rtc(struct vmctx *ctx)
{
	struct rtc_pl031_softc *sc;
	struct mem_range mr;
	int error;

	sc = rtc_pl031_init(mmio_rtc_intr_assert, mmio_rtc_intr_deassert,
	    ctx);

	bzero(&mr, sizeof(struct mem_range));
	mr.name = "rtc";
	mr.base = RTC_MMIO_BASE;
	mr.size = RTC_MMIO_SIZE;
	mr.flags = MEM_F_RW;
	mr.handler = mmio_rtc_mem_handler;
	mr.arg1 = sc;
	mr.arg2 = mr.base;
	error = register_mem(&mr);
	assert(error == 0);
}

static vm_paddr_t
fdt_gpa(struct vmctx *ctx)
{
	return (vm_get_highmem_base(ctx) + FDT_BASE);
}

int
bhyve_init_platform(struct vmctx *ctx, struct vcpu *bsp)
{
	const char *bootrom;
	uint64_t elr;
	int error;
	int pcie_intrs[4] = {PCIE_INTA, PCIE_INTB, PCIE_INTC, PCIE_INTD};

	bootrom = get_config_value("bootrom");
	if (bootrom == NULL) {
		warnx("no bootrom specified");
		return (ENOENT);
	}
	load_bootrom(ctx, bootrom, &elr);
	error = vm_set_register(bsp, VM_REG_GUEST_PC, elr);
	if (error != 0) {
		warn("vm_set_register(GUEST_PC)");
		return (error);
	}

	error = fdt_init(ctx, guest_ncpus, fdt_gpa(ctx), FDT_SIZE);
	if (error != 0)
		return (error);

	fdt_add_gic(GIC_DIST_BASE, GIC_DIST_SIZE, GIC_REDIST_BASE,
	    GIC_REDIST_SIZE(guest_ncpus));
	error = vm_attach_vgic(ctx, GIC_DIST_BASE, GIC_DIST_SIZE,
	    GIC_REDIST_BASE, GIC_REDIST_SIZE(guest_ncpus));
	if (error != 0) {
		warn("vm_attach_vgic()");
		return (error);
	}

	if (init_mmio_uart(ctx))
		fdt_add_uart(UART_MMIO_BASE, UART_MMIO_SIZE, UART_INTR);
	init_mmio_rtc(ctx);
	fdt_add_rtc(RTC_MMIO_BASE, RTC_MMIO_SIZE, RTC_INTR);
	fdt_add_timer();
	pci_irq_init(pcie_intrs);
	fdt_add_pcie(pcie_intrs);

	return (0);
}

int
bhyve_init_platform_late(struct vmctx *ctx, struct vcpu *bsp __unused)
{
	int error;

	fdt_finalize();

	error = vm_set_register(bsp, VM_REG_GUEST_X0, fdt_gpa(ctx));
	assert(error == 0);

	return (0);
}
