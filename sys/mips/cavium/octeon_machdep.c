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
#include <mips/cavium/octeonreg.h>
#include <machine/hwfunc.h>
#include <machine/intr_machdep.h>
#include <machine/locore.h>
#include <machine/md_var.h>
#include <machine/pcpu.h>
#include <machine/pte.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

#if defined(__mips_n64) 
#define MAX_APP_DESC_ADDR     0xffffffffafffffff
#else
#define MAX_APP_DESC_ADDR     0xafffffff
#endif

extern int	*edata;
extern int	*end;

uint64_t ciu_get_en_reg_addr_new(int corenum, int intx, int enx, int ciu_ip);
void ciu_dump_interrutps_enabled(int core_num, int intx, int enx, int ciu_ip);

static void octeon_boot_params_init(register_t ptr);
static uint64_t ciu_get_intr_sum_reg_addr(int core_num, int intx, int enx);
static uint64_t ciu_get_intr_en_reg_addr(int core_num, int intx, int enx);

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
	((void(*)(void))(long)0x9fc00000)();	/* Jump to MIPS reset vector */
}


static inline uint32_t
octeon_disable_interrupts(void)
{
	uint32_t status_bits;

	status_bits = mips_rd_status();
	mips_wr_status(status_bits & ~MIPS_SR_INT_IE);
	return (status_bits);
}


static inline void
octeon_set_interrupts(uint32_t status_bits)
{
	mips_wr_status(status_bits);
}


void
octeon_led_write_char(int char_position, char val)
{
	uint64_t ptr = (OCTEON_CHAR_LED_BASE_ADDR | 0xf8);

	if (!octeon_board_real())
		return;

	char_position &= 0x7;  /* only 8 chars */
	ptr += char_position;
	oct_write8_x8(ptr, val);
}

void
octeon_led_write_char0(char val)
{
	uint64_t ptr = (OCTEON_CHAR_LED_BASE_ADDR | 0xf8);

	if (!octeon_board_real())
		return;
	oct_write8_x8(ptr, val);
}

void
octeon_led_write_hexchar(int char_position, char hexval)
{
	uint64_t ptr = (OCTEON_CHAR_LED_BASE_ADDR | 0xf8);
	char char1, char2;

	if (!octeon_board_real())
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

	if (!octeon_board_real())
		return;

	for (i=0; i<8; i++, ptr++) {
		if (str && *str)
			oct_write8_x8(ptr, *str++);
		else
			oct_write8_x8(ptr, ' ');
		oct_read64(OCTEON_MIO_BOOT_BIST_STAT);
	}
}

static char progress[8] = { '-', '/', '|', '\\', '-', '/', '|', '\\'};

void
octeon_led_run_wheel(int *prog_count, int led_position)
{
	if (!octeon_board_real())
		return;
	octeon_led_write_char(led_position, progress[*prog_count]);
	*prog_count += 1;
	*prog_count &= 0x7;
}

#define LSR_DATAREADY        0x01    /* Data ready */
#define LSR_THRE             0x20    /* Transmit holding register empty */
#define LSR_TEMT	     0x40    /* Transmitter Empty. THR, TSR & FIFO */
#define USR_TXFIFO_NOTFULL   0x02    /* Uart TX FIFO Not full */

/*
 * octeon_uart_write_byte
 * 
 * Put out a single byte off of uart port.
 */

void
octeon_uart_write_byte(int uart_index, uint8_t ch)
{
	uint64_t val, val2;
	if (uart_index < 0 || uart_index > 1)
		return;

	while (1) {
		val = oct_read64(OCTEON_MIO_UART0_LSR + (uart_index * 0x400));
		val2 = oct_read64(OCTEON_MIO_UART0_USR + (uart_index * 0x400));
		if ((((uint8_t) val) & LSR_THRE) ||
		    (((uint8_t) val2) & USR_TXFIFO_NOTFULL)) {
			break;
		}
	}

	/* Write the byte */
	oct_write8(OCTEON_MIO_UART0_THR + (uart_index * 0x400), (uint64_t) ch);

	/* Force Flush the IOBus */
	oct_read64(OCTEON_MIO_BOOT_BIST_STAT);
}


void
octeon_uart_write_byte0(uint8_t ch)
{
	uint64_t val, val2;

	while (1) {
		val = oct_read64(OCTEON_MIO_UART0_LSR);
		val2 = oct_read64(OCTEON_MIO_UART0_USR);
		if ((((uint8_t) val) & LSR_THRE) ||
		    (((uint8_t) val2) & USR_TXFIFO_NOTFULL)) {
			break;
		}
	}

	/* Write the byte */
	oct_write8(OCTEON_MIO_UART0_THR, (uint64_t) ch);

	/* Force Flush the IOBus */
	oct_read64(OCTEON_MIO_BOOT_BIST_STAT);
}

/*
 * octeon_uart_write_string
 * 
 */
void
octeon_uart_write_string(int uart_index, const char *str)
{
	/* Just loop writing one byte at a time */
    
	while (*str) {
		octeon_uart_write_byte(uart_index, *str);
		if (*str == '\n') {
			octeon_uart_write_byte(uart_index, '\r');
		}
		str++;
	}
}

static char wstr[30];

void
octeon_led_write_hex(uint32_t wl)
{
	char nbuf[80];

	sprintf(nbuf, "%X", wl);
	octeon_led_write_string(nbuf);
}


void octeon_uart_write_hex2(uint32_t wl, uint32_t wh)
{
	sprintf(wstr, "0x%X-0x%X  ", wh, wl);
	octeon_uart_write_string(0, wstr);
}

void
octeon_uart_write_hex(uint32_t wl)
{
	sprintf(wstr, " 0x%X  ", wl);
	octeon_uart_write_string(0, wstr);
}

/*
 * octeon_wait_uart_flush
 */
void
octeon_wait_uart_flush(int uart_index, uint8_t ch)
{
	uint64_t val;
	int64_t val3;
	uint32_t cpu_status_bits;

	if (uart_index < 0 || uart_index > 1)
		return;

	cpu_status_bits = octeon_disable_interrupts();
	/* Force Flush the IOBus */
	oct_read64(OCTEON_MIO_BOOT_BIST_STAT);
	for (val3 = 0xfffffffff; val3 > 0; val3--) {
		val = oct_read64(OCTEON_MIO_UART0_LSR + (uart_index * 0x400));
		if (((uint8_t) val) & LSR_TEMT)
			break;
	}
	octeon_set_interrupts(cpu_status_bits);
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

void
octeon_ciu_stop_gtimer(int timer)
{
	oct_write64(OCTEON_CIU_GENTIMER_ADDR(timer), 0ll);
}

void
octeon_ciu_start_gtimer(int timer, u_int one_shot, uint64_t time_cycles)
{
    	octeon_ciu_gentimer gentimer;

        gentimer.word64 = 0;
        gentimer.bits.one_shot = one_shot;
        gentimer.bits.len = time_cycles - 1;
        oct_write64(OCTEON_CIU_GENTIMER_ADDR(timer), gentimer.word64);
}

/*
 * octeon_ciu_reset
 *
 * Shutdown all CIU to IP2, IP3 mappings
 */
void
octeon_ciu_reset(void)
{

	octeon_ciu_stop_gtimer(CIU_GENTIMER_NUM_0);
	octeon_ciu_stop_gtimer(CIU_GENTIMER_NUM_1);
	octeon_ciu_stop_gtimer(CIU_GENTIMER_NUM_2);
	octeon_ciu_stop_gtimer(CIU_GENTIMER_NUM_3);

	ciu_disable_intr(CIU_THIS_CORE, CIU_INT_0, CIU_EN_0);
	ciu_disable_intr(CIU_THIS_CORE, CIU_INT_0, CIU_EN_1);
	ciu_disable_intr(CIU_THIS_CORE, CIU_INT_1, CIU_EN_0);
	ciu_disable_intr(CIU_THIS_CORE, CIU_INT_1, CIU_EN_1);

	ciu_clear_int_summary(CIU_THIS_CORE, CIU_INT_0, CIU_EN_0, 0ll);
	ciu_clear_int_summary(CIU_THIS_CORE, CIU_INT_1, CIU_EN_0, 0ll);
	ciu_clear_int_summary(CIU_THIS_CORE, CIU_INT_1, CIU_EN_1, 0ll);
}

/*
 * mips_disable_interrupt_controllers
 *
 * Disable interrupts in the CPU controller
 */
void
mips_disable_interrupt_controls(void)
{
	/*
	 * Disable interrupts in CIU.
	 */
	octeon_ciu_reset();
}

/*
 * ciu_get_intr_sum_reg_addr
 */
static uint64_t
ciu_get_intr_sum_reg_addr(int core_num, int intx, int enx)
{
	uint64_t ciu_intr_sum_reg_addr;

    	if (enx == CIU_EN_0)
            	ciu_intr_sum_reg_addr = OCTEON_CIU_SUMMARY_BASE_ADDR +
		    (core_num * 0x10) + (intx * 0x8);
	else
            	ciu_intr_sum_reg_addr = OCTEON_CIU_SUMMARY_INT1_ADDR;

        return (ciu_intr_sum_reg_addr);
}


/*
 * ciu_get_intr_en_reg_addr
 */
static uint64_t
ciu_get_intr_en_reg_addr(int core_num, int intx, int enx)
{
	uint64_t ciu_intr_reg_addr;

    	ciu_intr_reg_addr = OCTEON_CIU_ENABLE_BASE_ADDR + 
	    ((enx == 0) ? 0x0 : 0x8) + (intx * 0x10) +  (core_num * 0x20);
        return (ciu_intr_reg_addr);
}




/*
 * ciu_get_intr_reg_addr
 *
 * 200 ---int0,en0 ip2
 * 208 ---int0,en1 ip2 ----> this is wrong... this is watchdog
 * 
 * 210 ---int0,en0 ip3 --
 * 218 ---int0,en1 ip3 ----> same here.. .this is watchdog... right?
 * 
 * 220 ---int1,en0 ip2
 * 228 ---int1,en1 ip2
 * 230 ---int1,en0 ip3 --
 * 238 ---int1,en1 ip3
 *
 */
uint64_t
ciu_get_en_reg_addr_new(int corenum, int intx, int enx, int ciu_ip)
{
	uint64_t ciu_intr_reg_addr = OCTEON_CIU_ENABLE_BASE_ADDR;

	/* XXX kasserts? */
	if (enx < CIU_EN_0 || enx > CIU_EN_1) {
		printf("%s: invalid enx value %d, should be %d or %d\n",
		    __FUNCTION__, enx, CIU_EN_0, CIU_EN_1);
		return 0;
	}
	if (intx < CIU_INT_0 || intx > CIU_INT_1) {
		printf("%s: invalid intx value %d, should be %d or %d\n",
		    __FUNCTION__, enx, CIU_INT_0, CIU_INT_1);
		return 0;
	}
	if (ciu_ip < CIU_MIPS_IP2 || ciu_ip > CIU_MIPS_IP3) {
		printf("%s: invalid ciu_ip value %d, should be %d or %d\n",
		    __FUNCTION__, ciu_ip, CIU_MIPS_IP2, CIU_MIPS_IP3);
		return 0;
	}

	ciu_intr_reg_addr += (enx    * 0x8);
	ciu_intr_reg_addr += (ciu_ip * 0x10);
	ciu_intr_reg_addr += (intx   * 0x20);
	return (ciu_intr_reg_addr);
}

/*
 * ciu_get_int_summary
 */
uint64_t
ciu_get_int_summary(int core_num, int intx, int enx)
{
	uint64_t ciu_intr_sum_reg_addr;

	if (core_num == CIU_THIS_CORE)
        	core_num = octeon_get_core_num();
	ciu_intr_sum_reg_addr = ciu_get_intr_sum_reg_addr(core_num, intx, enx);
	return (oct_read64(ciu_intr_sum_reg_addr));
}

//#define DEBUG_CIU 1

#ifdef DEBUG_CIU
#define DEBUG_CIU_SUM 1
#define DEBUG_CIU_EN 1
#endif


/*
 * ciu_clear_int_summary
 */
void
ciu_clear_int_summary(int core_num, int intx, int enx, uint64_t write_bits)
{
	uint32_t cpu_status_bits;
	uint64_t ciu_intr_sum_reg_addr;

//#define DEBUG_CIU_SUM 1

#ifdef DEBUG_CIU_SUM
	uint64_t ciu_intr_sum_bits;
#endif


	if (core_num == CIU_THIS_CORE) {
        	core_num = octeon_get_core_num();
	}

#ifdef DEBUG_CIU_SUM
        printf(" CIU: core %u clear sum IntX %u  Enx %u  Bits: 0x%llX\n",
	    core_num, intx, enx, write_bits);
#endif

	cpu_status_bits = octeon_disable_interrupts();

	ciu_intr_sum_reg_addr = ciu_get_intr_sum_reg_addr(core_num, intx, enx);

#ifdef DEBUG_CIU_SUM
    	ciu_intr_sum_bits =  oct_read64(ciu_intr_sum_reg_addr);	/* unneeded dummy read */
        printf(" CIU: status: 0x%X  reg_addr: 0x%llX   Val: 0x%llX   ->  0x%llX",
	    cpu_status_bits, ciu_intr_sum_reg_addr, ciu_intr_sum_bits,
	    ciu_intr_sum_bits | write_bits);
#endif

	oct_write64(ciu_intr_sum_reg_addr, write_bits);
	oct_read64(OCTEON_MIO_BOOT_BIST_STAT);	/* Bus Barrier */

#ifdef DEBUG_CIU_SUM
        printf(" Readback: 0x%llX\n\n   ", (uint64_t) oct_read64(ciu_intr_sum_reg_addr));
#endif
    
	octeon_set_interrupts(cpu_status_bits);
}

/*
 * ciu_disable_intr
 */
void
ciu_disable_intr(int core_num, int intx, int enx)
{
	uint32_t cpu_status_bits;
	uint64_t ciu_intr_reg_addr;

	if (core_num == CIU_THIS_CORE)
        	core_num = octeon_get_core_num();

	cpu_status_bits = octeon_disable_interrupts();
    
	ciu_intr_reg_addr = ciu_get_intr_en_reg_addr(core_num, intx, enx);

	oct_read64(ciu_intr_reg_addr);	/* Dummy read */

	oct_write64(ciu_intr_reg_addr, 0LL);
	oct_read64(OCTEON_MIO_BOOT_BIST_STAT);	/* Bus Barrier */

	octeon_set_interrupts(cpu_status_bits);
}

void
ciu_dump_interrutps_enabled(int core_num, int intx, int enx, int ciu_ip)
{

	uint64_t ciu_intr_reg_addr;
	uint64_t ciu_intr_bits;

        if (core_num == CIU_THIS_CORE) {
            	core_num = octeon_get_core_num();
        }

#ifndef OCTEON_SMP_1
	ciu_intr_reg_addr = ciu_get_intr_en_reg_addr(core_num, intx, enx);
#else
	ciu_intr_reg_addr = ciu_get_en_reg_addr_new(core_num, intx, enx, ciu_ip);
#endif

        if (!ciu_intr_reg_addr) {
            printf("Bad call to %s\n", __FUNCTION__);
            while(1);
            return;
        }

	ciu_intr_bits =  oct_read64(ciu_intr_reg_addr);
        printf(" CIU core %d  int: %d  en: %d  ip: %d  Add: %#llx  enabled: %#llx  SR: %x\n",
	    core_num, intx, enx, ciu_ip, (unsigned long long)ciu_intr_reg_addr,
	    (unsigned long long)ciu_intr_bits, mips_rd_status());
}


/*
 * ciu_enable_interrupts
 */
void ciu_enable_interrupts(int core_num, int intx, int enx,
    uint64_t set_these_interrupt_bits, int ciu_ip)
{
	uint32_t cpu_status_bits;
	uint64_t ciu_intr_reg_addr;
	uint64_t ciu_intr_bits;

        if (core_num == CIU_THIS_CORE)
            	core_num = octeon_get_core_num();

//#define DEBUG_CIU_EN 1

#ifdef DEBUG_CIU_EN
        printf(" CIU: core %u enabling Intx %u  Enx %u IP %d  Bits: 0x%llX\n",
	    core_num, intx, enx, ciu_ip, set_these_interrupt_bits);
#endif

	cpu_status_bits = octeon_disable_interrupts();

#ifndef OCTEON_SMP_1
	ciu_intr_reg_addr = ciu_get_intr_en_reg_addr(core_num, intx, enx);
#else
	ciu_intr_reg_addr = ciu_get_en_reg_addr_new(core_num, intx, enx, ciu_ip);
#endif

        if (!ciu_intr_reg_addr) {
		printf("Bad call to %s\n", __FUNCTION__);
		while(1);
		return;	/* XXX */
        }

	ciu_intr_bits =  oct_read64(ciu_intr_reg_addr);

#ifdef DEBUG_CIU_EN
        printf(" CIU: status: 0x%X  reg_addr: 0x%llX   Val: 0x%llX   ->  0x%llX",
	    cpu_status_bits, ciu_intr_reg_addr, ciu_intr_bits, ciu_intr_bits | set_these_interrupt_bits);
#endif
	ciu_intr_bits |=  set_these_interrupt_bits;
	oct_write64(ciu_intr_reg_addr, ciu_intr_bits);
#ifdef OCTEON_SMP
	mips_wbflush();
#endif
	oct_read64(OCTEON_MIO_BOOT_BIST_STAT);	/* Bus Barrier */

#ifdef DEBUG_CIU_EN
        printf(" Readback: 0x%llX\n\n   ",
	    (uint64_t)oct_read64(ciu_intr_reg_addr));
#endif

	octeon_set_interrupts(cpu_status_bits);
}

void
platform_start(__register_t a0, __register_t a1, __register_t a2 __unused,
    __register_t a3)
{
	uint64_t platform_counter_freq;
	int i, mem = 0;

	/* Initialize pcpu stuff */
	mips_pcpu0_init();

	octeon_boot_params_init(a3);
	/* XXX octeon boot decriptor has args in it... */
        octeon_ciu_reset();
    	octeon_uart_write_string(0, "Platform Starting\n");

	bootverbose = 1;
	if (mem > 0)
		realmem = btoc(mem << 20);
	else
		realmem = btoc(32 << 20);

	for (i = 0; i < 10; i++)
		phys_avail[i] = 0;

	/* phys_avail regions are in bytes */
	phys_avail[0] = MIPS_KSEG0_TO_PHYS((vm_offset_t)&end);
	phys_avail[1] = ctob(realmem);

	physmem = realmem;

	pmap_bootstrap();
	mips_proc0_init();

	init_param1();
	/* TODO: parse argc,argv */
	platform_counter_freq = 330000000UL; /* XXX: from idt */
	mips_timer_init_params(platform_counter_freq, 1);
	cninit();
	init_param2(physmem);
	mips_cpu_init();
	mutex_init();
#ifdef DDB
	kdb_init();
#endif
}

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


typedef struct {
	uint32_t major_version;
	uint32_t minor_version;

	uint64_t stack_top;
	uint64_t heap_base;
	uint64_t heap_end;
	uint64_t desc_vaddr;

	uint32_t exception_base_addr;
	uint32_t stack_size;
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
} cvmx_bootinfo_t;

uint32_t octeon_cpu_clock;
uint64_t octeon_dram;
uint32_t octeon_bd_ver = 0, octeon_cvmx_bd_ver = 0, octeon_board_rev_major, octeon_board_rev_minor, octeon_board_type;
uint8_t octeon_mac_addr[6] = { 0 };
int octeon_core_mask, octeon_mac_addr_count;
int octeon_chip_rev_major = 0, octeon_chip_rev_minor = 0, octeon_chip_type = 0;

extern int32_t app_descriptor_addr;
static octeon_boot_descriptor_t *app_desc_ptr;
static cvmx_bootinfo_t *cvmx_desc_ptr;

#define OCTEON_BOARD_TYPE_NONE 0
#define OCTEON_BOARD_TYPE_SIM  1

#define OCTEON_CLOCK_MIN     (100 * 1000 * 1000)
#define OCTEON_CLOCK_MAX     (800 * 1000 * 1000)
#define OCTEON_DRAM_DEFAULT  (256 * 1024 * 1024)
#define OCTEON_DRAM_MIN	     30
#define OCTEON_DRAM_MAX	     3000


int
octeon_board_real(void)
{
	if ((octeon_board_type == OCTEON_BOARD_TYPE_NONE) ||
	    (octeon_board_type == OCTEON_BOARD_TYPE_SIM) ||
	    !octeon_board_rev_major)
		return 0;
	return 1;
}

static void
octeon_process_app_desc_ver_unknown(void)
{
    	printf(" Unknown Boot-Descriptor: Using Defaults\n");

    	octeon_cpu_clock = OCTEON_CLOCK_DEFAULT;
        octeon_dram = OCTEON_DRAM_DEFAULT;
        octeon_board_rev_major = octeon_board_rev_minor = octeon_board_type = 0;
        octeon_core_mask = 1;
        octeon_cpu_clock  = OCTEON_CLOCK_DEFAULT;
        octeon_chip_type = octeon_chip_rev_major = octeon_chip_rev_minor = 0;
        octeon_mac_addr[0] = 0x00; octeon_mac_addr[1] = 0x0f;
        octeon_mac_addr[2] = 0xb7; octeon_mac_addr[3] = 0x10;
        octeon_mac_addr[4] = 0x09; octeon_mac_addr[5] = 0x06;
        octeon_mac_addr_count = 1;
}

static int
octeon_process_app_desc_ver_6(void)
{
	/* XXX Why is 0x00000000ffffffffULL a bad value?  */
	if (app_desc_ptr->cvmx_desc_vaddr == 0 ||
	    app_desc_ptr->cvmx_desc_vaddr == 0xfffffffful) {
            	printf ("Bad cvmx_desc_ptr %p\n", cvmx_desc_ptr);
                return 1;
	}
    	cvmx_desc_ptr =
	    (cvmx_bootinfo_t *)(intptr_t)app_desc_ptr->cvmx_desc_vaddr;
        cvmx_desc_ptr =
	    (cvmx_bootinfo_t *) ((intptr_t)cvmx_desc_ptr | MIPS_KSEG0_START);
        octeon_cvmx_bd_ver = (cvmx_desc_ptr->major_version * 100) +
	    cvmx_desc_ptr->minor_version;
	/* Too early for panic? */
        if (cvmx_desc_ptr->major_version != 1) {
            	printf("Incompatible CVMX descriptor from bootloader: %d.%d %p\n",
                       (int) cvmx_desc_ptr->major_version,
                       (int) cvmx_desc_ptr->minor_version, cvmx_desc_ptr);
                while (1);	/*  Never return */
                return 1;	/*  Satisfy the compiler */
        }

        octeon_core_mask = cvmx_desc_ptr->core_mask;
        octeon_cpu_clock  = cvmx_desc_ptr->eclock_hz;
        octeon_board_type = cvmx_desc_ptr->board_type;
        octeon_board_rev_major = cvmx_desc_ptr->board_rev_major;
        octeon_board_rev_minor = cvmx_desc_ptr->board_rev_minor;
        octeon_chip_type = cvmx_desc_ptr->chip_type;
        octeon_chip_rev_major = cvmx_desc_ptr->chip_rev_major;
        octeon_chip_rev_minor = cvmx_desc_ptr->chip_rev_minor;
        octeon_mac_addr[0] = cvmx_desc_ptr->mac_addr_base[0];
        octeon_mac_addr[1] = cvmx_desc_ptr->mac_addr_base[1];
        octeon_mac_addr[2] = cvmx_desc_ptr->mac_addr_base[2];
        octeon_mac_addr[3] = cvmx_desc_ptr->mac_addr_base[3];
        octeon_mac_addr[4] = cvmx_desc_ptr->mac_addr_base[4];
        octeon_mac_addr[5] = cvmx_desc_ptr->mac_addr_base[5];
        octeon_mac_addr_count = cvmx_desc_ptr->mac_addr_count;

        if (app_desc_ptr->dram_size > 16*1024*1024)
            	octeon_dram = (uint64_t)app_desc_ptr->dram_size;
	else
            	octeon_dram = (uint64_t)app_desc_ptr->dram_size << 20;
        return 0;
}

static int
octeon_process_app_desc_ver_3_4_5(void)
{

    	octeon_cvmx_bd_ver = octeon_bd_ver;
        octeon_core_mask = app_desc_ptr->core_mask;

        if (app_desc_ptr->desc_version > 3)
            	octeon_cpu_clock = app_desc_ptr->eclock_hz;
	else
            	octeon_cpu_clock  = OCTEON_CLOCK_DEFAULT;
        if (app_desc_ptr->dram_size > 16*1024*1024)
            	octeon_dram = (uint64_t)app_desc_ptr->dram_size;
	else
            	octeon_dram = (uint64_t)app_desc_ptr->dram_size << 20;

        if (app_desc_ptr->desc_version > 4) {
            	octeon_board_type = app_desc_ptr->board_type;
                octeon_board_rev_major = app_desc_ptr->board_rev_major;
                octeon_board_rev_minor = app_desc_ptr->board_rev_minor;
                octeon_chip_type = app_desc_ptr->chip_type;
                octeon_chip_rev_major = app_desc_ptr->chip_rev_major;
                octeon_chip_rev_minor = app_desc_ptr->chip_rev_minor;

                octeon_mac_addr[0] = app_desc_ptr->mac_addr_base[0];
                octeon_mac_addr[1] = app_desc_ptr->mac_addr_base[1];
                octeon_mac_addr[2] = app_desc_ptr->mac_addr_base[2];
                octeon_mac_addr[3] = app_desc_ptr->mac_addr_base[3];
                octeon_mac_addr[4] = app_desc_ptr->mac_addr_base[4];
                octeon_mac_addr[5] = app_desc_ptr->mac_addr_base[5];
                octeon_mac_addr_count = app_desc_ptr->mac_addr_count;
        }
        return 0;
}


static void
octeon_boot_params_init(register_t ptr)
{
	int bad_desc = 1;

    	if (ptr != 0 && ptr < MAX_APP_DESC_ADDR) {
	        app_desc_ptr = (octeon_boot_descriptor_t *)(intptr_t)ptr;
		octeon_bd_ver = app_desc_ptr->desc_version;
                if ((octeon_bd_ver >= 3) && (octeon_bd_ver <= 5))
			bad_desc = octeon_process_app_desc_ver_3_4_5();
		else if (app_desc_ptr->desc_version == 6)
			bad_desc = octeon_process_app_desc_ver_6();
        }
        if (bad_desc)
        	octeon_process_app_desc_ver_unknown();

        printf("Boot Descriptor Ver: %u -> %u/%u",
               octeon_bd_ver, octeon_cvmx_bd_ver/100, octeon_cvmx_bd_ver%100);
        printf("  CPU clock: %uMHz\n", octeon_cpu_clock/1000000);
        printf("  Dram: %u MB", (uint32_t)(octeon_dram >> 20));
        printf("  Board Type: %u  Revision: %u/%u\n",
               octeon_board_type, octeon_board_rev_major, octeon_board_rev_minor);
        printf("  Octeon Chip: %u  Rev %u/%u",
               octeon_chip_type, octeon_chip_rev_major, octeon_chip_rev_minor);

        printf("  Mac Address %02X.%02X.%02X.%02X.%02X.%02X\n",
               octeon_mac_addr[0], octeon_mac_addr[1], octeon_mac_addr[2],
               octeon_mac_addr[3], octeon_mac_addr[4], octeon_mac_addr[5]);
}
