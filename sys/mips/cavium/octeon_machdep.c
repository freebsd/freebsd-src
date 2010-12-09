/*-
 * Copyright (c) 2006 Wojciech A. Koszek <wkoszek@FreeBSD.org>
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
 * $FreeBSD$
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/imgact.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/cons.h>
#include <sys/exec.h>
#include <sys/ucontext.h>
#include <sys/proc.h>
#include <sys/kdb.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <machine/atomic.h>
#include <machine/cache.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cpuregs.h>
#include <machine/cpufunc.h>
#include <mips/cavium/octeon_pcmap_regs.h>
#include <machine/hwfunc.h>
#include <machine/intr_machdep.h>
#include <machine/locore.h>
#include <machine/md_var.h>
#include <machine/pcpu.h>
#include <machine/pte.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

#include <contrib/octeon-sdk/cvmx.h>
#include <contrib/octeon-sdk/cvmx-bootmem.h>
#include <contrib/octeon-sdk/cvmx-interrupt.h>
#include <contrib/octeon-sdk/cvmx-version.h>

#if defined(__mips_n64) 
#define MAX_APP_DESC_ADDR     0xffffffffafffffff
#else
#define MAX_APP_DESC_ADDR     0xafffffff
#endif

#define OCTEON_CLOCK_DEFAULT (500 * 1000 * 1000)

struct octeon_feature_description {
	octeon_feature_t ofd_feature;
	const char *ofd_string;
};

extern int	*edata;
extern int	*end;

static const struct octeon_feature_description octeon_feature_descriptions[] = {
	{ OCTEON_FEATURE_SAAD,			"SAAD" },
	{ OCTEON_FEATURE_ZIP,			"ZIP" },
	{ OCTEON_FEATURE_CRYPTO,		"CRYPTO" },
	{ OCTEON_FEATURE_DORM_CRYPTO,		"DORM_CRYPTO" },
	{ OCTEON_FEATURE_PCIE,			"PCIE" },
	{ OCTEON_FEATURE_SRIO,			"SRIO" },
	{ OCTEON_FEATURE_KEY_MEMORY,		"KEY_MEMORY" },
	{ OCTEON_FEATURE_LED_CONTROLLER,	"LED_CONTROLLER" },
	{ OCTEON_FEATURE_TRA,			"TRA" },
	{ OCTEON_FEATURE_MGMT_PORT,		"MGMT_PORT" },
	{ OCTEON_FEATURE_RAID,			"RAID" },
	{ OCTEON_FEATURE_USB,			"USB" },
	{ OCTEON_FEATURE_NO_WPTR,		"NO_WPTR" },
	{ OCTEON_FEATURE_DFA,			"DFA" },
	{ OCTEON_FEATURE_MDIO_CLAUSE_45,	"MDIO_CLAUSE_45" },
	{ OCTEON_FEATURE_NPEI,			"NPEI" },
	{ 0,					NULL }
};

uint64_t ciu_get_en_reg_addr_new(int corenum, int intx, int enx, int ciu_ip);
void ciu_dump_interrutps_enabled(int core_num, int intx, int enx, int ciu_ip);

static uint64_t octeon_get_ticks(void);
static unsigned octeon_get_timecount(struct timecounter *tc);

static void octeon_boot_params_init(register_t ptr);

static struct timecounter octeon_timecounter = {
	octeon_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffffffu,		/* octeon_mask */
	0,			/* frequency */
	"Octeon",		/* name */
	900,			/* quality (adjusted in code) */
};

void
platform_cpu_init()
{
	/* Nothing special yet */
}

/*
 * Perform a board-level soft-reset.
 */
void
platform_reset(void)
{
	cvmx_write_csr(CVMX_CIU_SOFT_RST, 1);
}

void
octeon_led_write_char(int char_position, char val)
{
	uint64_t ptr = (OCTEON_CHAR_LED_BASE_ADDR | 0xf8);

	if (octeon_is_simulation())
		return;

	char_position &= 0x7;  /* only 8 chars */
	ptr += char_position;
	oct_write8_x8(ptr, val);
}

void
octeon_led_write_char0(char val)
{
	uint64_t ptr = (OCTEON_CHAR_LED_BASE_ADDR | 0xf8);

	if (octeon_is_simulation())
		return;
	oct_write8_x8(ptr, val);
}

void
octeon_led_write_hexchar(int char_position, char hexval)
{
	uint64_t ptr = (OCTEON_CHAR_LED_BASE_ADDR | 0xf8);
	char char1, char2;

	if (octeon_is_simulation())
		return;

	char1 = (hexval >> 4) & 0x0f; char1 = (char1 < 10)?char1+'0':char1+'7';
	char2 = (hexval  & 0x0f); char2 = (char2 < 10)?char2+'0':char2+'7';
	char_position &= 0x7;  /* only 8 chars */
	if (char_position > 6)
		char_position = 6;
	ptr += char_position;
	oct_write8_x8(ptr, char1);
	ptr++;
	oct_write8_x8(ptr, char2);
}

void
octeon_led_write_string(const char *str)
{
	uint64_t ptr = (OCTEON_CHAR_LED_BASE_ADDR | 0xf8);
	int i;

	if (octeon_is_simulation())
		return;

	for (i=0; i<8; i++, ptr++) {
		if (str && *str)
			oct_write8_x8(ptr, *str++);
		else
			oct_write8_x8(ptr, ' ');
		(void)cvmx_read_csr(CVMX_MIO_BOOT_BIST_STAT);
	}
}

static char progress[8] = { '-', '/', '|', '\\', '-', '/', '|', '\\'};

void
octeon_led_run_wheel(int *prog_count, int led_position)
{
	if (octeon_is_simulation())
		return;
	octeon_led_write_char(led_position, progress[*prog_count]);
	*prog_count += 1;
	*prog_count &= 0x7;
}

void
octeon_led_write_hex(uint32_t wl)
{
	char nbuf[80];

	sprintf(nbuf, "%X", wl);
	octeon_led_write_string(nbuf);
}

/*
 * octeon_debug_symbol
 *
 * Does nothing.
 * Used to mark the point for simulator to begin tracing
 */
void
octeon_debug_symbol(void)
{
}

/*
 * octeon_ciu_reset
 *
 * Shutdown all CIU to IP2, IP3 mappings
 */
void
octeon_ciu_reset(void)
{
	/* Disable all CIU interrupts by default */
	cvmx_write_csr(CVMX_CIU_INTX_EN0(cvmx_get_core_num()*2), 0);
	cvmx_write_csr(CVMX_CIU_INTX_EN0(cvmx_get_core_num()*2+1), 0);
	cvmx_write_csr(CVMX_CIU_INTX_EN1(cvmx_get_core_num()*2), 0);
	cvmx_write_csr(CVMX_CIU_INTX_EN1(cvmx_get_core_num()*2+1), 0);

#ifdef SMP
	/* Enable the MBOX interrupts.  */
	cvmx_write_csr(CVMX_CIU_INTX_EN0(cvmx_get_core_num()*2+1),
		       (1ull << (CVMX_IRQ_MBOX0 - 8)) |
		       (1ull << (CVMX_IRQ_MBOX1 - 8)));
#endif
}

static void
octeon_memory_init(void)
{
	vm_paddr_t phys_end;
	int64_t addr;
	unsigned i, j;

	phys_end = round_page(MIPS_KSEG0_TO_PHYS((vm_offset_t)&end));

	if (octeon_is_simulation()) {
		/* Simulator we limit to 96 meg */
		phys_avail[0] = phys_end;
		phys_avail[1] = 96 << 20;

		dump_avail[0] = phys_avail[0];
		dump_avail[1] = phys_avail[1];

		realmem = physmem = btoc(phys_avail[1] - phys_avail[0]);
		return;
	}

	/*
	 * Allocate memory from bootmem 1MB at a time and merge
	 * adjacent entries.
	 */
	i = 0;
	while (i < PHYS_AVAIL_ENTRIES) {
		addr = cvmx_bootmem_phy_alloc(1 << 20, phys_end,
					      ~(vm_paddr_t)0, PAGE_SIZE, 0);
		if (addr == -1)
			break;

		/*
		 * The SDK needs to be able to easily map any memory that might
		 * come to it e.g. in the form of an mbuf.  Because on !n64 we
		 * can't direct-map some addresses and we don't want to manage
		 * temporary mappings within the SDK, don't feed memory that
		 * can't be direct-mapped to the kernel.
		 */
#if !defined(__mips_n64)
		if (!MIPS_DIRECT_MAPPABLE(addr + (1 << 20) - 1))
			continue;
#endif

		physmem += btoc(1 << 20);

		if (i > 0 && phys_avail[i - 1] == addr) {
			phys_avail[i - 1] += 1 << 20;
			continue;
		}

		phys_avail[i + 0] = addr;
		phys_avail[i + 1] = addr + (1 << 20);

		i += 2;
	}

	for (j = 0; j < i; j++)
		dump_avail[j] = phys_avail[j];

	realmem = physmem;
}

void
platform_start(__register_t a0, __register_t a1, __register_t a2 __unused,
    __register_t a3)
{
	const struct octeon_feature_description *ofd;
	uint64_t platform_counter_freq;

	/*
	 * XXX
	 * octeon_boot_params_init() should be called before anything else,
	 * certainly before any output; we may find out from the boot
	 * descriptor's flags that we're supposed to use the PCI or UART1
	 * consoles rather than UART0.  No point doing that reorganization
	 * until we actually intercept UART_DEV_CONSOLE for the UART1 case
	 * and somehow handle the PCI console, which we lack code for
	 * entirely.
	 */

	/* Initialize pcpu stuff */
	mips_pcpu0_init();
	mips_timer_early_init(OCTEON_CLOCK_DEFAULT);
	cninit();

	octeon_ciu_reset();
	octeon_boot_params_init(a3);
	/*
	 * XXX
	 * We can certainly parse command line arguments or U-Boot environment
	 * to determine whether to bootverbose / single user / ...  I think
	 * stass has patches to add support for loader things to U-Boot even.
	 */
	bootverbose = 1;

	/*
	 * For some reason on the cn38xx simulator ebase register is set to
	 * 0x80001000 at bootup time.  Move it back to the default, but
	 * when we move to having support for multiple executives, we need
	 * to rethink this.
	 */
	mips_wr_ebase(0x80000000);

	octeon_memory_init();
	init_param1();
	init_param2(physmem);
	mips_cpu_init();
	pmap_bootstrap();
	mips_proc0_init();
	mutex_init();
	kdb_init();
#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif
	platform_counter_freq = cvmx_sysinfo_get()->cpu_clock_hz;

	octeon_timecounter.tc_frequency = cvmx_sysinfo_get()->cpu_clock_hz;
	platform_timecounter = &octeon_timecounter;

	mips_timer_init_params(platform_counter_freq, 0);

	set_cputicker(octeon_get_ticks, cvmx_sysinfo_get()->cpu_clock_hz, 0);

#ifdef SMP
	/*
	 * Clear any pending IPIs.
	 */
	cvmx_write_csr(CVMX_CIU_MBOX_CLRX(0), 0xffffffff);
#endif

	printf("Octeon SDK: %s\n", OCTEON_SDK_VERSION_STRING);
	printf("Available Octeon features:");
	for (ofd = octeon_feature_descriptions; ofd->ofd_string != NULL; ofd++)
		if (octeon_has_feature(ofd->ofd_feature))
			printf(" %s", ofd->ofd_string);
	printf("\n");
}

static uint64_t
octeon_get_ticks(void)
{
	uint64_t cvmcount;

	CVMX_MF_CYCLE(cvmcount);
	return (cvmcount);
}

static unsigned
octeon_get_timecount(struct timecounter *tc)
{
	return ((unsigned)octeon_get_ticks());
}

/**
 * version of printf that works better in exception context.
 *
 * @param format
 *
 * XXX If this function weren't in cvmx-interrupt.c, we'd use the SDK version.
 */
void cvmx_safe_printf(const char *format, ...)
{
    char buffer[256];
    char *ptr = buffer;
    int count;
    va_list args;

    va_start(args, format);
#ifndef __U_BOOT__
    count = vsnprintf(buffer, sizeof(buffer), format, args);
#else
    count = vsprintf(buffer, format, args);
#endif
    va_end(args);

    while (count-- > 0)
    {
        cvmx_uart_lsr_t lsrval;

        /* Spin until there is room */
        do
        {
            lsrval.u64 = cvmx_read_csr(CVMX_MIO_UARTX_LSR(0));
#if !defined(CONFIG_OCTEON_SIM_SPEED)
            if (lsrval.s.temt == 0)
                cvmx_wait(10000);   /* Just to reduce the load on the system */
#endif
        }
        while (lsrval.s.temt == 0);

        if (*ptr == '\n')
            cvmx_write_csr(CVMX_MIO_UARTX_THR(0), '\r');
        cvmx_write_csr(CVMX_MIO_UARTX_THR(0), *ptr++);
    }
}

/* impSTART: This stuff should move back into the Cavium SDK */
/*
 ****************************************************************************************
 *
 * APP/BOOT  DESCRIPTOR  STUFF
 *
 ****************************************************************************************
 */

/* Define the struct that is initialized by the bootloader used by the 
 * startup code.
 *
 * Copyright (c) 2004, 2005, 2006 Cavium Networks.
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 */

#define OCTEON_CURRENT_DESC_VERSION     6
#define OCTEON_ARGV_MAX_ARGS            (64)
#define OCTOEN_SERIAL_LEN 20

typedef struct {
	/* Start of block referenced by assembly code - do not change! */
	uint32_t desc_version;
	uint32_t desc_size;

	uint64_t stack_top;
	uint64_t heap_base;
	uint64_t heap_end;
	uint64_t entry_point;   /* Only used by bootloader */
	uint64_t desc_vaddr;
	/* End of This block referenced by assembly code - do not change! */

	uint32_t exception_base_addr;
	uint32_t stack_size;
	uint32_t heap_size;
	uint32_t argc;  /* Argc count for application */
	uint32_t argv[OCTEON_ARGV_MAX_ARGS];
	uint32_t flags;
	uint32_t core_mask;
	uint32_t dram_size;  /**< DRAM size in megabyes */
	uint32_t phy_mem_desc_addr;  /**< physical address of free memory descriptor block*/
	uint32_t debugger_flags_base_addr;  /**< used to pass flags from app to debugger */
	uint32_t eclock_hz;  /**< CPU clock speed, in hz */
	uint32_t dclock_hz;  /**< DRAM clock speed, in hz */
	uint32_t spi_clock_hz;  /**< SPI4 clock in hz */
	uint16_t board_type;
	uint8_t board_rev_major;
	uint8_t board_rev_minor;
	uint16_t chip_type;
	uint8_t chip_rev_major;
	uint8_t chip_rev_minor;
	char board_serial_number[OCTOEN_SERIAL_LEN];
	uint8_t mac_addr_base[6];
	uint8_t mac_addr_count;
	uint64_t cvmx_desc_vaddr;
} octeon_boot_descriptor_t;

cvmx_bootinfo_t *octeon_bootinfo;

static octeon_boot_descriptor_t *app_desc_ptr;

int
octeon_is_simulation(void)
{
	switch (cvmx_sysinfo_get()->board_type) {
	case CVMX_BOARD_TYPE_SIM:
		return 1;
	default:
		return 0;
	}
}

static void
octeon_process_app_desc_ver_6(void)
{
	/* XXX Why is 0x00000000ffffffffULL a bad value?  */
	if (app_desc_ptr->cvmx_desc_vaddr == 0 ||
	    app_desc_ptr->cvmx_desc_vaddr == 0xfffffffful)
            	panic("Bad octeon_bootinfo %p", octeon_bootinfo);

    	octeon_bootinfo =
	    (cvmx_bootinfo_t *)(intptr_t)app_desc_ptr->cvmx_desc_vaddr;
        octeon_bootinfo =
	    (cvmx_bootinfo_t *) ((intptr_t)octeon_bootinfo | MIPS_KSEG0_START);
        if (octeon_bootinfo->major_version != 1)
            	panic("Incompatible CVMX descriptor from bootloader: %d.%d %p",
                       (int) octeon_bootinfo->major_version,
                       (int) octeon_bootinfo->minor_version, octeon_bootinfo);

	cvmx_sysinfo_minimal_initialize(octeon_bootinfo->phy_mem_desc_addr,
					octeon_bootinfo->board_type,
					octeon_bootinfo->board_rev_major,
					octeon_bootinfo->board_rev_minor,
					octeon_bootinfo->eclock_hz);
}

static void
octeon_boot_params_init(register_t ptr)
{
	if (ptr == 0 || ptr >= MAX_APP_DESC_ADDR)
		panic("app descriptor passed at invalid address %#jx",
		    (uintmax_t)ptr);

	app_desc_ptr = (octeon_boot_descriptor_t *)(intptr_t)ptr;
	if (app_desc_ptr->desc_version < 6)
		panic("Your boot code is too old to be supported.");
	octeon_process_app_desc_ver_6();

	KASSERT(octeon_bootinfo != NULL, ("octeon_bootinfo should be set"));

	if (cvmx_sysinfo_get()->phy_mem_desc_addr == (uint64_t)0)
		panic("Your boot loader did not supply a memory descriptor.");
	cvmx_bootmem_init(cvmx_sysinfo_get()->phy_mem_desc_addr);

        printf("Boot Descriptor Ver: %u -> %u/%u",
               app_desc_ptr->desc_version, octeon_bootinfo->major_version,
	       octeon_bootinfo->minor_version);
        printf("  CPU clock: %uMHz  Core Mask: %#x\n",
	       cvmx_sysinfo_get()->cpu_clock_hz / 1000000,
	       cvmx_sysinfo_get()->core_mask);
        printf("  Board Type: %u  Revision: %u/%u\n",
               cvmx_sysinfo_get()->board_type,
	       cvmx_sysinfo_get()->board_rev_major,
	       cvmx_sysinfo_get()->board_rev_minor);

        printf("  Mac Address %02X.%02X.%02X.%02X.%02X.%02X (%d)\n",
	    octeon_bootinfo->mac_addr_base[0],
	    octeon_bootinfo->mac_addr_base[1],
	    octeon_bootinfo->mac_addr_base[2],
	    octeon_bootinfo->mac_addr_base[3],
	    octeon_bootinfo->mac_addr_base[4],
	    octeon_bootinfo->mac_addr_base[5],
	    octeon_bootinfo->mac_addr_count);

#if defined(OCTEON_BOARD_CAPK_0100ND)
	if (cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_CN3010_EVB_HS5)
		printf("Compiled for CAPK-0100ND, but board type is %s\n",
		    cvmx_board_type_to_string(cvmx_sysinfo_get()->board_type));
#else
	printf("Board: %s\n",
	    cvmx_board_type_to_string(cvmx_sysinfo_get()->board_type));
#endif
	printf("Model: %s\n", octeon_model_get_string(cvmx_get_proc_id()));
}
/* impEND: This stuff should move back into the Cavium SDK */
