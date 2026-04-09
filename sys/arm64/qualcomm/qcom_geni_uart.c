/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 UOS Project Contributors
 * Derived from OpenBSD qcuart.c (ISC licence)
 *
 * Qualcomm GENI SE (Generic Interface Serial Engine) UART Driver
 * ===============================================================
 * Supports: SM8450, SM8550, SM8650, SC8280XP Snapdragon UART
 *
 * The GENI SE is Qualcomm's unified serial engine supporting UART, SPI,
 * I2C. This driver handles the UART mode.
 *
 * Compatible strings (from OpenBSD + Linux):
 *   qcom,geni-debug-uart  - early debug UART (fixed 115200)
 *   qcom,geni-uart        - general GENI UART
 *
 * The GENI UART register layout differs significantly from ns8250:
 *   - Command-based TX/RX via GENI_TX_CMD and GENI_RX_CMD
 *   - FIFO mode (up to 16 DWORD entries, packed 4 bytes per word)
 *   - Baud rate via GENI_SER_M_CLK_CFG (clock divider)
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu_fdt.h>

/* GENI register offsets */
#define GENI_FORCE_DEFAULT_REG		0x020
#define GENI_OUTPUT_CTRL		0x024
#define GENI_CGC_CTRL			0x028
#define GENI_STATUS			0x040
#define GENI_SER_M_CLK_CFG		0x048
#define GENI_FW_REVISION_RO		0x068
#define GENI_CLK_SEL			0x07C
#define GENI_DMA_MODE_EN		0x258
#define GENI_M_CMD0			0x600
#define GENI_M_CMD_CTRL_REG		0x604
#define GENI_M_IRQ_STATUS		0x610
#define GENI_M_IRQ_EN			0x614
#define GENI_M_IRQ_CLEAR		0x618
#define GENI_S_CMD0			0x630
#define GENI_S_CMD_CTRL_REG		0x634
#define GENI_S_IRQ_STATUS		0x640
#define GENI_S_IRQ_EN			0x644
#define GENI_S_IRQ_CLEAR		0x648
#define GENI_TX_FW_INPUT_STATUS		0x008
#define GENI_TX_FIFO			0x700
#define GENI_RX_FIFO			0x780
#define GENI_TX_FIFO_STATUS		0x800
#define GENI_RX_FIFO_STATUS		0x804
#define GENI_TX_WATERMARK_REG		0x80C
#define GENI_RX_WATERMARK_REG		0x810
#define GENI_RX_LAST_BYTE_VALID		0x830

/* GENI UART commands */
#define UART_START_TX			0x1
#define UART_START_RX			0x1

/* IRQ bits */
#define M_TX_FIFO_WATERMARK_EN		(1 << 30)
#define M_CMD_DONE_EN			(1 << 0)
#define S_RX_FIFO_WATERMARK_EN		(1 << 26)
#define S_RX_FIFO_LAST_EN		(1 << 27)
#define S_CMD_DONE_EN			(1 << 0)

/* FIFO status */
#define TX_FIFO_WC_MSK			0x1FFFFFFF
#define RX_FIFO_WC_MSK			0x1FFFFFFF
#define RX_LAST_BYTE_VALID_MSK		0x7	/* bits [2:0]: bytes valid in last word */
#define RX_FIFO_WC(n)			((n) & RX_FIFO_WC_MSK)

/* Clock config */
#define CLK_DIV_SHFT			4
#define SER_CLK_EN			(1 << 0)

#define GENI_UART_CONSOLE_BAUD		115200

struct qcom_uart_softc {
	struct uart_softc	base;
};

/* ---- Hardware access ---- */
static inline uint32_t
greg_read(struct uart_bas *bas, int off)
{
	return (bus_space_read_4(bas->bst, bas->bsh, off));
}

static inline void
greg_write(struct uart_bas *bas, int off, uint32_t val)
{
	bus_space_write_4(bas->bst, bas->bsh, off, val);
}

/* ---- UART class operations ---- */

static int
qcom_uart_probe(struct uart_bas *bas)
{
	uint32_t ver;

	ver = greg_read(bas, GENI_FW_REVISION_RO);
	if (ver == 0 || ver == 0xdeadbeef)
		return (ENXIO);
	return (0);
}

static void
qcom_uart_init(struct uart_bas *bas, int baudrate, int databits,
    int stopbits, int parity)
{
	/* Enable CGC (Clock Gating Control) */
	greg_write(bas, GENI_CGC_CTRL, 0x7f);

	/* Force GENI output control */
	greg_write(bas, GENI_OUTPUT_CTRL, 0x7);

	/* Disable DMA mode - use FIFO mode */
	greg_write(bas, GENI_DMA_MODE_EN, 0);

	/* Enable RX watermark + last IRQ */
	greg_write(bas, GENI_S_IRQ_EN,
	    S_RX_FIFO_WATERMARK_EN | S_RX_FIFO_LAST_EN | S_CMD_DONE_EN);
}

static void
qcom_uart_putc(struct uart_bas *bas, int c)
{
	/* Wait for TX FIFO space */
	int limit = 100000;
	while ((greg_read(bas, GENI_TX_FIFO_STATUS) & TX_FIFO_WC_MSK) > 0
	    && limit-- > 0)
		;

	/* Start TX command (if not already running) */
	if (!(greg_read(bas, GENI_STATUS) & 0x1))
		greg_write(bas, GENI_M_CMD0,
		    (uint32_t)UART_START_TX << 27);

	/* Write character (1 byte packed in 32-bit FIFO word) */
	greg_write(bas, GENI_TX_FIFO, (uint32_t)c & 0xFF);
}

static int
qcom_uart_rxready(struct uart_bas *bas)
{
	return ((greg_read(bas, GENI_RX_FIFO_STATUS) & RX_FIFO_WC_MSK) > 0);
}

static int
qcom_uart_getc(struct uart_bas *bas, struct mtx *hwmtx)
{
	uint32_t word, last_valid;
	int c;

	uart_lock(hwmtx);
	/* Wait for character */
	while (!qcom_uart_rxready(bas))
		;

	word = greg_read(bas, GENI_RX_FIFO);
	last_valid = greg_read(bas, GENI_RX_LAST_BYTE_VALID);
	c = (int)(word & 0xFF);
	uart_unlock(hwmtx);
	return (c);
}

static struct uart_ops qcom_geni_uart_ops = {
	.probe   = qcom_uart_probe,
	.init    = qcom_uart_init,
	.term    = NULL,
	.putc    = qcom_uart_putc,
	.rxready = qcom_uart_rxready,
	.getc    = qcom_uart_getc,
};

static struct uart_class qcom_geni_uart_class = {
	"qcom-geni-uart",
	NULL,    /* methods filled by bus attach */
	sizeof(struct qcom_uart_softc),
	.uc_ops  = &qcom_geni_uart_ops,
	.uc_range = 0x1000,
	.uc_rclk = 7372800,	/* typical GENI ref clock */
	.uc_rshift = 0,
	.uc_riowidth = 4,
};

static struct ofw_compat_data qcom_uart_compat[] = {
	{ "qcom,geni-debug-uart",	(uintptr_t)&qcom_geni_uart_class },
	{ "qcom,geni-uart",		(uintptr_t)&qcom_geni_uart_class },
	{ NULL, 0 },
};

UART_FDT_CLASS_AND_DEVICE(qcom_uart_compat);
