/*
 * Copyright (c) 2026 Hayzam Sherif <hayzam@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * MediaTek UART ("mediatek,mt6577-uart" and compatibles).
 *
 * A 16550 with 32-bit spaced registers and an extended baud rate
 * generator.  In mode 3 a bit lasts DLM:DLL * (SAMPLE_COUNT + 1) clock
 * cycles, and FRACDIV_L/M add one sample period to selected bits:
 * FRACDIV_L to the data bits, FRACDIV_M bit 0 to the parity bit and bit 1
 * to a stop bit.  With n extended bits in a frame of b bits:
 *
 *	baud = rclk / (div * (count + n / b))
 *
 * Measured on the MT7986: an extension only applies to bits the frame
 * has, and the stop bit one only with two stop bits.  With five data bits,
 * two stop bits are one sample period short, and the driver does not use
 * the stop bit extension there.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <dev/clk/clk.h>
#include <dev/fdt/fdt_pinctrl.h>
#include <dev/ic/ns16550.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>
#include <dev/uart/uart_cpu_fdt.h>
#include <dev/uart/uart_dev_ns8250.h>

#include "uart_if.h"

/* MediaTek registers, as register indices (4 bytes apart). */
#define	MTK_UART_HIGHSPEED	0x09
#define	 MTK_UART_HIGHSPEED_MODE	0x03
#define	 MTK_UART_HIGHSPEED_SAMPLE	3	/* baud from SAMPLE_COUNT */
#define	MTK_UART_SAMPLE_COUNT	0x0a
#define	MTK_UART_SAMPLE_POINT	0x0b
#define	MTK_UART_RATEFIX	0x0d
#define	 MTK_UART_RATEFIX_EN		0x01	/* divide the clock by 2 */
#define	 MTK_UART_RATEFIX_DIV4		0x04	/* by 4 instead */
#define	MTK_UART_FRACDIV_L	0x15	/* data bits 0-7 */
#define	MTK_UART_FRACDIV_M	0x16
#define	 MTK_UART_FRACDIV_M_PARITY	0x01
#define	 MTK_UART_FRACDIV_M_STOP	0x02

/* Fewer sample periods per bit need guard time from the sender (manual). */
#define	MTK_UART_MIN_COUNT	8

struct mtk_uart_softc {
	struct ns8250_softc	ns8250;
	clk_t			baud;
	clk_t			bus;		/* optional */
};

struct mtk_uart_divisor {
	uint32_t	div;		/* DLM:DLL */
	uint32_t	count;		/* sample periods per bit */
	uint32_t	fracl;
	uint32_t	fracm;
};

/* A frame lasts bits * count - trim sample periods, plus extensions. */
struct mtk_uart_frame {
	u_int		bits;
	u_int		databits;
	u_int		slots;		/* bits that can be extended */
	u_int		trim;
	bool		parity;
	bool		stop2;
};

/* The frame format in an LCR value. */
static void
mtk_uart_lcr_frame(struct mtk_uart_frame *f, uint8_t lcr)
{
	f->databits = (lcr & LCR_8BITS) + 5;
	f->parity = (lcr & LCR_PENAB) != 0;
	f->stop2 = (lcr & LCR_STOPB) != 0;
	f->bits = 1 + f->databits + (f->parity ? 1 : 0) + (f->stop2 ? 2 : 1);
	f->trim = (f->databits == 5 && f->stop2) ? 1 : 0;
	f->slots = f->databits + (f->parity ? 1 : 0) +
	    (f->stop2 && f->databits > 5 ? 1 : 0);
}

/*
 * Use the smallest divisor whose bit time fits SAMPLE_COUNT, and spread the
 * rest of the frame time over the bits that can be extended.
 */
static int
mtk_uart_divisor(u_int rclk, int baudrate, const struct mtk_uart_frame *f,
    struct mtk_uart_divisor *d)
{
	uint64_t periods;
	uint32_t div, i, k;

	if (baudrate <= 0 || rclk == 0)
		return (EINVAL);
	div = howmany((uint64_t)rclk, (uint64_t)baudrate * 256);
	if (div > 0xffff)
		return (EINVAL);

	/* Frame time in sample periods, rounded. */
	periods = ((uint64_t)rclk * f->bits + (uint64_t)div * baudrate / 2) /
	    ((uint64_t)div * baudrate);
	d->div = div;
	d->count = (periods + f->trim) / f->bits;
	k = (periods + f->trim) % f->bits;
	if (k > f->slots) {
		/* Too many to extend: round to the nearer limit. */
		if (k - f->slots <= f->bits - k) {
			k = f->slots;
		} else {
			d->count++;
			k = 0;
		}
	}
	/* From 8 periods per bit, the rate is within 3 %. */
	if (d->count < MTK_UART_MIN_COUNT)
		return (EINVAL);

	d->fracl = d->fracm = 0;
	for (i = 0; i < f->slots; i++) {
		if ((i + 1) * k / f->slots == i * k / f->slots)
			continue;
		if (i < f->databits)
			d->fracl |= 1u << i;
		else if (i == f->databits && f->parity)
			d->fracm |= MTK_UART_FRACDIV_M_PARITY;
		else
			d->fracm |= MTK_UART_FRACDIV_M_STOP;
	}
	return (0);
}

static void
mtk_uart_set_divisor(struct uart_bas *bas, const struct mtk_uart_divisor *d)
{
	uint8_t lcr;

	uart_setreg(bas, MTK_UART_RATEFIX, 0);
	uart_setreg(bas, MTK_UART_HIGHSPEED, MTK_UART_HIGHSPEED_SAMPLE);
	uart_setreg(bas, MTK_UART_SAMPLE_COUNT, d->count - 1);
	/* Sample in the middle of the bit, as in the manual's Table 2-6. */
	uart_setreg(bas, MTK_UART_SAMPLE_POINT, (d->count - 3) / 2);
	uart_setreg(bas, MTK_UART_FRACDIV_L, d->fracl);
	uart_setreg(bas, MTK_UART_FRACDIV_M, d->fracm);

	lcr = uart_getreg(bas, REG_LCR);
	uart_setreg(bas, REG_LCR, lcr | LCR_DLAB);
	uart_barrier(bas);
	uart_setreg(bas, REG_DLL, d->div & 0xff);
	uart_setreg(bas, REG_DLH, (d->div >> 8) & 0xff);
	uart_setreg(bas, REG_LCR, lcr);
	uart_barrier(bas);
}

/* Back to plain 16550 mode, which the ns8250 code expects. */
static void
mtk_uart_reset_generator(struct uart_bas *bas)
{
	uart_setreg(bas, MTK_UART_RATEFIX, 0);
	uart_setreg(bas, MTK_UART_HIGHSPEED, 0);
	uart_setreg(bas, MTK_UART_FRACDIV_L, 0);
	uart_setreg(bas, MTK_UART_FRACDIV_M, 0);
	uart_barrier(bas);
}

/*
 * The baud rate generator functions for the ns8250 code.  They use the frame
 * format that LCR holds at the time.
 */
static int
mtk_uart_set_baudrate(struct uart_bas *bas, int baudrate)
{
	struct mtk_uart_divisor d;
	struct mtk_uart_frame f;

	mtk_uart_lcr_frame(&f, uart_getreg(bas, REG_LCR));
	if (mtk_uart_divisor(bas->rclk, baudrate, &f, &d) != 0)
		return (EINVAL);
	mtk_uart_set_divisor(bas, &d);
	return (0);
}

/* Input clock cycles per bit, in units of 1 / NS8250_DIVISOR_SCALE. */
static uint32_t
mtk_uart_get_divisor(struct uart_bas *bas)
{
	struct mtk_uart_frame f;
	uint64_t cycles;
	uint32_t div, ext, fracm, mode, rf;
	uint8_t lcr;

	lcr = uart_getreg(bas, REG_LCR);
	uart_setreg(bas, REG_LCR, lcr | LCR_DLAB);
	uart_barrier(bas);
	div = uart_getreg(bas, REG_DLL) | (uart_getreg(bas, REG_DLH) << 8);
	uart_setreg(bas, REG_LCR, lcr);
	uart_barrier(bas);

	cycles = (uint64_t)div * NS8250_DIVISOR_SCALE;
	rf = uart_getreg(bas, MTK_UART_RATEFIX);
	if ((rf & MTK_UART_RATEFIX_EN) != 0)
		cycles *= (rf & MTK_UART_RATEFIX_DIV4) != 0 ? 4 : 2;

	/* Modes 0-2 divide by 16, 8 or 4. */
	mode = uart_getreg(bas, MTK_UART_HIGHSPEED) & MTK_UART_HIGHSPEED_MODE;
	if (mode != MTK_UART_HIGHSPEED_SAMPLE)
		return (cycles * (16 >> mode));

	/* Mode 3: average the extended bits over the frame. */
	mtk_uart_lcr_frame(&f, lcr);
	fracm = uart_getreg(bas, MTK_UART_FRACDIV_M);
	ext = bitcount32(uart_getreg(bas, MTK_UART_FRACDIV_L) &
	    ((1u << f.databits) - 1));
	if (f.parity && (fracm & MTK_UART_FRACDIV_M_PARITY) != 0)
		ext++;
	if (f.stop2 && (fracm & MTK_UART_FRACDIV_M_STOP) != 0)
		ext++;
	cycles *= (uint64_t)(uart_getreg(bas, MTK_UART_SAMPLE_COUNT) + 1) *
	    f.bits - f.trim + ext;
	return ((cycles + f.bits / 2) / f.bits);
}

static int
mtk_uart_get_baudrate(struct uart_bas *bas)
{
	uint32_t divisor;

	divisor = mtk_uart_get_divisor(bas);
	if (divisor == 0)
		return (0);
	return ((uint64_t)bas->rclk * NS8250_DIVISOR_SCALE / divisor);
}

/*
 * The baud clock is named "baud" or is the only one.  The bus clock is
 * optional, but one that is listed must be found.
 */
static int
mtk_uart_get_clocks(device_t dev, clk_t *baud, clk_t *bus)
{
	int error;

	*bus = NULL;
	if (clk_get_by_ofw_name(dev, 0, "baud", baud) != 0 &&
	    clk_get_by_ofw_index(dev, 0, 0, baud) != 0)
		return (ENXIO);
	error = clk_get_by_ofw_name(dev, 0, "bus", bus);
	if (error != 0 && error != ENOENT) {
		clk_release(*baud);
		return (ENXIO);
	}
	return (0);
}

static int
mtk_uart_enable_clocks(clk_t baud, clk_t bus)
{
	int error;

	if (bus != NULL && (error = clk_enable(bus)) != 0)
		return (error);
	return (clk_enable(baud));
}

/* clk_release() also drops the enable references. */
static void
mtk_uart_release_clocks(clk_t baud, clk_t bus)
{
	clk_release(baud);
	if (bus != NULL)
		clk_release(bus);
}

static int
mtk_uart_get_rclk(clk_t baud, u_int *rclk)
{
	uint64_t freq;

	if (clk_get_freq(baud, &freq) != 0 || freq == 0)
		return (ENXIO);
	*rclk = freq;
	return (0);
}

/*
 * The ns8250 probe needs the clock rate, which the generic FDT attachment
 * does not know, and uses the registers.  The console runs on the clocks
 * the firmware set up; other ports have them enabled for the probe only.
 * The ns8250 probe also sets a rate the 16550 way, so ports other than the
 * console go back to that mode first.
 */
static int
mtk_uart_bus_probe(struct uart_softc *sc)
{
	clk_t baud, bus;
	int error;

	if (mtk_uart_get_clocks(sc->sc_dev, &baud, &bus) != 0)
		return (ENXIO);
	if ((error = mtk_uart_get_rclk(baud, &sc->sc_bas.rclk)) != 0)
		goto out;
	if (sc->sc_sysdev == NULL) {
		if ((error = mtk_uart_enable_clocks(baud, bus)) != 0)
			goto out;
		mtk_uart_reset_generator(&sc->sc_bas);
	}
	error = ns8250_bus_probe(sc);
	if (error == 0)
		device_set_desc(sc->sc_dev, "MediaTek UART");
out:
	mtk_uart_release_clocks(baud, bus);
	return (error);
}

static int
mtk_uart_bus_attach(struct uart_softc *sc)
{
	struct mtk_uart_softc *msc = (struct mtk_uart_softc *)sc;
	device_t dev = sc->sc_dev;
	int error;

	if (mtk_uart_get_clocks(dev, &msc->baud, &msc->bus) != 0) {
		device_printf(dev, "cannot get clocks\n");
		return (ENXIO);
	}
	/* It reports its own errors; the rate is read back below. */
	clk_set_assigned(dev, ofw_bus_get_node(dev));
	if ((error = mtk_uart_enable_clocks(msc->baud, msc->bus)) != 0) {
		device_printf(dev, "cannot enable clocks\n");
		goto fail;
	}
	/* The assigned clocks can change the rate. */
	if ((error = mtk_uart_get_rclk(msc->baud, &sc->sc_bas.rclk)) != 0)
		goto fail;

	/* Apply the default pin state, if there is a pin controller. */
	fdt_pinctrl_configure_by_name(dev, "default");
	msc->ns8250.set_baudrate = mtk_uart_set_baudrate;
	msc->ns8250.get_baudrate = mtk_uart_get_baudrate;
	msc->ns8250.get_divisor = mtk_uart_get_divisor;
	error = ns8250_bus_attach(sc);
	if (error == 0)
		return (0);
fail:
	mtk_uart_release_clocks(msc->baud, msc->bus);
	return (error);
}

static int
mtk_uart_bus_detach(struct uart_softc *sc)
{
	struct mtk_uart_softc *msc = (struct mtk_uart_softc *)sc;
	int error;

	error = ns8250_bus_detach(sc);
	/* The low-level console code keeps using the console port. */
	if (sc->sc_sysdev == NULL)
		mtk_uart_release_clocks(msc->baud, msc->bus);
	return (error);
}

static kobj_method_t mtk_uart_methods[] = {
	KOBJMETHOD(uart_probe,		mtk_uart_bus_probe),
	KOBJMETHOD(uart_attach,		mtk_uart_bus_attach),
	KOBJMETHOD(uart_detach,		mtk_uart_bus_detach),
	KOBJMETHOD(uart_flush,		ns8250_bus_flush),
	KOBJMETHOD(uart_getsig,		ns8250_bus_getsig),
	KOBJMETHOD(uart_ioctl,		ns8250_bus_ioctl),
	KOBJMETHOD(uart_ipend,		ns8250_bus_ipend),
	KOBJMETHOD(uart_param,		ns8250_bus_param),
	KOBJMETHOD(uart_receive,	ns8250_bus_receive),
	KOBJMETHOD(uart_setsig,		ns8250_bus_setsig),
	KOBJMETHOD(uart_transmit,	ns8250_bus_transmit),
	KOBJMETHOD(uart_txbusy,		ns8250_bus_txbusy),
	KOBJMETHOD(uart_grab,		ns8250_bus_grab),
	KOBJMETHOD(uart_ungrab,		ns8250_bus_ungrab),
	KOBJMETHOD_END
};

static struct uart_class uart_mtk_class = {
	"mtk",
	mtk_uart_methods,
	sizeof(struct mtk_uart_softc),
	.uc_ops = &uart_ns8250_ops,
	.uc_range = (MTK_UART_FRACDIV_M + 1) << 2,	/* bytes */
	.uc_rshift = 2,
	.uc_riowidth = 4,
};

static struct ofw_compat_data compat_data[] = {
	{ "mediatek,mt6577-uart",	(uintptr_t)&uart_mtk_class },
	{ NULL,				0 }
};
UART_FDT_CLASS_AND_DEVICE(compat_data);
