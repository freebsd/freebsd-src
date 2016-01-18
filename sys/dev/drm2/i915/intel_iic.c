/*
 * Copyright (c) 2006 Dave Airlie <airlied@linux.ie>
 * Copyright © 2006-2008,2010 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Eric Anholt <eric@anholt.net>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm.h>
#include <dev/drm2/i915/i915_drm.h>
#include <dev/drm2/i915/i915_drv.h>
#include <dev/drm2/i915/intel_drv.h>
#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include "iicbus_if.h"
#include "iicbb_if.h"

static void intel_teardown_gmbus_m(struct drm_device *dev, int m);

struct gmbus_port {
	const char *name;
	int reg;
};

static const struct gmbus_port gmbus_ports[] = {
	{ "ssc", GPIOB },
	{ "vga", GPIOA },
	{ "panel", GPIOC },
	{ "dpc", GPIOD },
	{ "dpb", GPIOE },
	{ "dpd", GPIOF },
};

/* Intel GPIO access functions */

#define I2C_RISEFALL_TIME 10

struct intel_iic_softc {
	struct drm_device *drm_dev;
	device_t iic_dev;
	bool force_bit_dev;
	char name[32];
	uint32_t reg;
	uint32_t reg0;
};

static void
intel_iic_quirk_set(struct drm_i915_private *dev_priv, bool enable)
{
	u32 val;

	/* When using bit bashing for I2C, this bit needs to be set to 1 */
	if (!IS_PINEVIEW(dev_priv->dev))
		return;

	val = I915_READ(DSPCLK_GATE_D);
	if (enable)
		val |= DPCUNIT_CLOCK_GATE_DISABLE;
	else
		val &= ~DPCUNIT_CLOCK_GATE_DISABLE;
	I915_WRITE(DSPCLK_GATE_D, val);
}

static u32
intel_iic_get_reserved(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;
	u32 reserved;

	sc = device_get_softc(idev);
	dev = sc->drm_dev;
	dev_priv = dev->dev_private;

	if (!IS_I830(dev) && !IS_845G(dev)) {
		reserved = I915_READ_NOTRACE(sc->reg) &
		    (GPIO_DATA_PULLUP_DISABLE | GPIO_CLOCK_PULLUP_DISABLE);
	} else {
		reserved = 0;
	}

	return (reserved);
}

void
intel_iic_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;

	dev_priv = dev->dev_private;
	I915_WRITE(dev_priv->gpio_mmio_base + GMBUS0, 0);
}

static int
intel_iicbus_reset(device_t idev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct intel_iic_softc *sc;
	struct drm_device *dev;

	sc = device_get_softc(idev);
	dev = sc->drm_dev;

	intel_iic_reset(dev);
	return (0);
}

static void
intel_iicbb_setsda(device_t idev, int val)
{
	struct intel_iic_softc *sc;
	struct drm_i915_private *dev_priv;
	u32 reserved;
	u32 data_bits;

	sc = device_get_softc(idev);
	dev_priv = sc->drm_dev->dev_private;

	reserved = intel_iic_get_reserved(idev);
	if (val)
		data_bits = GPIO_DATA_DIR_IN | GPIO_DATA_DIR_MASK;
	else
		data_bits = GPIO_DATA_DIR_OUT | GPIO_DATA_DIR_MASK |
		    GPIO_DATA_VAL_MASK;

	I915_WRITE_NOTRACE(sc->reg, reserved | data_bits);
	POSTING_READ(sc->reg);
}

static void
intel_iicbb_setscl(device_t idev, int val)
{
	struct intel_iic_softc *sc;
	struct drm_i915_private *dev_priv;
	u32 clock_bits, reserved;

	sc = device_get_softc(idev);
	dev_priv = sc->drm_dev->dev_private;

	reserved = intel_iic_get_reserved(idev);
	if (val)
		clock_bits = GPIO_CLOCK_DIR_IN | GPIO_CLOCK_DIR_MASK;
	else
		clock_bits = GPIO_CLOCK_DIR_OUT | GPIO_CLOCK_DIR_MASK |
		    GPIO_CLOCK_VAL_MASK;

	I915_WRITE_NOTRACE(sc->reg, reserved | clock_bits);
	POSTING_READ(sc->reg);
}

static int
intel_iicbb_getsda(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_i915_private *dev_priv;
	u32 reserved;

	sc = device_get_softc(idev);
	dev_priv = sc->drm_dev->dev_private;

	reserved = intel_iic_get_reserved(idev);

	I915_WRITE_NOTRACE(sc->reg, reserved | GPIO_DATA_DIR_MASK);
	I915_WRITE_NOTRACE(sc->reg, reserved);
	return ((I915_READ_NOTRACE(sc->reg) & GPIO_DATA_VAL_IN) != 0);
}

static int
intel_iicbb_getscl(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_i915_private *dev_priv;
	u32 reserved;

	sc = device_get_softc(idev);
	dev_priv = sc->drm_dev->dev_private;

	reserved = intel_iic_get_reserved(idev);

	I915_WRITE_NOTRACE(sc->reg, reserved | GPIO_CLOCK_DIR_MASK);
	I915_WRITE_NOTRACE(sc->reg, reserved);
	return ((I915_READ_NOTRACE(sc->reg) & GPIO_CLOCK_VAL_IN) != 0);
}

static int
gmbus_xfer_read(struct drm_i915_private *dev_priv, struct iic_msg *msg,
    u32 gmbus1_index)
{
	int reg_offset = dev_priv->gpio_mmio_base;
	u16 len = msg->len;
	u8 *buf = msg->buf;

	I915_WRITE(GMBUS1 + reg_offset,
		   gmbus1_index |
		   GMBUS_CYCLE_WAIT |
		   (len << GMBUS_BYTE_COUNT_SHIFT) |
		   (msg->slave << (GMBUS_SLAVE_ADDR_SHIFT - 1)) |
		   GMBUS_SLAVE_READ | GMBUS_SW_RDY);
	while (len) {
		int ret;
		u32 val, loop = 0;
		u32 gmbus2;

		ret = _intel_wait_for(sc->drm_dev,
		    ((gmbus2 = I915_READ(GMBUS2 + reg_offset)) &
		    (GMBUS_SATOER | GMBUS_HW_RDY)),
		    50, 1, "915gbr");
		if (ret)
			return (-ETIMEDOUT);
		if (gmbus2 & GMBUS_SATOER)
			return (-ENXIO);

		val = I915_READ(GMBUS3 + reg_offset);
		do {
			*buf++ = val & 0xff;
			val >>= 8;
		} while (--len != 0 && ++loop < 4);
	}

	return 0;
}

static int
gmbus_xfer_write(struct drm_i915_private *dev_priv, struct iic_msg *msg)
{
	int reg_offset = dev_priv->gpio_mmio_base;
	u16 len = msg->len;
	u8 *buf = msg->buf;
	u32 val, loop;

	val = loop = 0;
	while (len && loop < 4) {
		val |= *buf++ << (8 * loop++);
		len -= 1;
	}

	I915_WRITE(GMBUS3 + reg_offset, val);
	I915_WRITE(GMBUS1 + reg_offset,
		   GMBUS_CYCLE_WAIT |
		   (msg->len << GMBUS_BYTE_COUNT_SHIFT) |
		   (msg->slave << (GMBUS_SLAVE_ADDR_SHIFT - 1)) |
		   GMBUS_SLAVE_WRITE | GMBUS_SW_RDY);
	while (len) {
		int ret;
		u32 gmbus2;

		val = loop = 0;
		do {
			val |= *buf++ << (8 * loop);
		} while (--len != 0 && ++loop < 4);

		I915_WRITE(GMBUS3 + reg_offset, val);

		ret = _intel_wait_for(sc->drm_dev,
		    ((gmbus2 = I915_READ(GMBUS2 + reg_offset)) &
		    (GMBUS_SATOER | GMBUS_HW_RDY)),
		    50, 1, "915gbw");
		if (ret)
			return (-ETIMEDOUT);
		if (gmbus2 & GMBUS_SATOER)
			return (-ENXIO);
	}
	return 0;
}

/*
 * The gmbus controller can combine a 1 or 2 byte write with a read that
 * immediately follows it by using an "INDEX" cycle.
 */
static bool
gmbus_is_index_read(struct iic_msg *msgs, int i, int num)
{
	return (i + 1 < num &&
		!(msgs[i].flags & IIC_M_RD) && msgs[i].len <= 2 &&
		(msgs[i + 1].flags & IIC_M_RD));
}

static int
gmbus_xfer_index_read(struct drm_i915_private *dev_priv, struct iic_msg *msgs)
{
	int reg_offset = dev_priv->gpio_mmio_base;
	u32 gmbus1_index = 0;
	u32 gmbus5 = 0;
	int ret;

	if (msgs[0].len == 2)
		gmbus5 = GMBUS_2BYTE_INDEX_EN |
			 msgs[0].buf[1] | (msgs[0].buf[0] << 8);
	if (msgs[0].len == 1)
		gmbus1_index = GMBUS_CYCLE_INDEX |
			       (msgs[0].buf[0] << GMBUS_SLAVE_INDEX_SHIFT);

	/* GMBUS5 holds 16-bit index */
	if (gmbus5)
		I915_WRITE(GMBUS5 + reg_offset, gmbus5);

	ret = gmbus_xfer_read(dev_priv, &msgs[1], gmbus1_index);

	/* Clear GMBUS5 after each index transfer */
	if (gmbus5)
		I915_WRITE(GMBUS5 + reg_offset, 0);

	return ret;
}

static int
intel_gmbus_transfer(device_t idev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct intel_iic_softc *sc;
	struct drm_i915_private *dev_priv;
	int error, i, ret, reg_offset, unit;

	error = 0;
	sc = device_get_softc(idev);
	dev_priv = sc->drm_dev->dev_private;
	unit = device_get_unit(idev);

	sx_xlock(&dev_priv->gmbus_sx);
	if (sc->force_bit_dev) {
		error = -IICBUS_TRANSFER(dev_priv->bbbus[unit], msgs, nmsgs);
		goto out;
	}

	reg_offset = dev_priv->gpio_mmio_base;

	I915_WRITE(GMBUS0 + reg_offset, sc->reg0);

	for (i = 0; i < nmsgs; i++) {
		u32 gmbus2;

		if (gmbus_is_index_read(msgs, i, nmsgs)) {
			error = gmbus_xfer_index_read(dev_priv, &msgs[i]);
			i += 1;  /* set i to the index of the read xfer */
		} else if (msgs[i].flags & IIC_M_RD) {
			error = gmbus_xfer_read(dev_priv, &msgs[i], 0);
		} else {
			error = gmbus_xfer_write(dev_priv, &msgs[i]);
		}

		if (error == -ETIMEDOUT)
			goto timeout;
		if (error == -ENXIO)
			goto clear_err;

		ret = _intel_wait_for(sc->drm_dev,
		    ((gmbus2 = I915_READ(GMBUS2 + reg_offset)) &
		    (GMBUS_SATOER | GMBUS_HW_WAIT_PHASE)),
		    50, 1, "915gbh");
		if (ret)
			goto timeout;
		if (gmbus2 & GMBUS_SATOER)
			goto clear_err;
	}

	/* Generate a STOP condition on the bus. Note that gmbus can't generata
	 * a STOP on the very first cycle. To simplify the code we
	 * unconditionally generate the STOP condition with an additional gmbus
	 * cycle. */
	I915_WRITE(GMBUS1 + reg_offset, GMBUS_CYCLE_STOP | GMBUS_SW_RDY);

	/* Mark the GMBUS interface as disabled after waiting for idle.
	 * We will re-enable it at the start of the next xfer,
	 * till then let it sleep.
 	 */
	if (_intel_wait_for(dev,
	    (I915_READ(GMBUS2 + reg_offset) & GMBUS_ACTIVE) == 0,
	    10, 1, "915gbu")) {
		DRM_DEBUG_KMS("GMBUS [%s] timed out waiting for idle\n",
		    sc->name);
		error = -ETIMEDOUT;
	}
	I915_WRITE(GMBUS0 + reg_offset, 0);
	goto out;

clear_err:
	/*
	 * Wait for bus to IDLE before clearing NAK.
	 * If we clear the NAK while bus is still active, then it will stay
	 * active and the next transaction may fail.
	 */
	if (_intel_wait_for(dev,
	    (I915_READ(GMBUS2 + reg_offset) & GMBUS_ACTIVE) == 0,
	    10, 1, "915gbu"))
		DRM_DEBUG_KMS("GMBUS [%s] timed out after NAK\n", sc->name);

	/* Toggle the Software Clear Interrupt bit. This has the effect
	 * of resetting the GMBUS controller and so clearing the
	 * BUS_ERROR raised by the slave's NAK.
	 */
	I915_WRITE(GMBUS1 + reg_offset, GMBUS_SW_CLR_INT);
	I915_WRITE(GMBUS1 + reg_offset, 0);
	I915_WRITE(GMBUS0 + reg_offset, 0);

	DRM_DEBUG_KMS("GMBUS [%s] NAK for addr: %04x %c(%d)\n",
			 sc->name, msgs[i].slave,
			 (msgs[i].flags & IIC_M_RD) ? 'r' : 'w', msgs[i].len);

	/*
	 * If no ACK is received during the address phase of a transaction,
	 * the adapter must report -ENXIO.
	 * It is not clear what to return if no ACK is received at other times.
	 * So, we always return -ENXIO in all NAK cases, to ensure we send
	 * it at least during the one case that is specified.
	 */
	error = -ENXIO;
	goto out;

timeout:
	DRM_INFO("GMBUS [%s] timed out, falling back to bit banging on pin %d\n",
	    sc->name, sc->reg0 & 0xff);
	I915_WRITE(GMBUS0 + reg_offset, 0);

	/*
	 * Hardware may not support GMBUS over these pins?
	 * Try GPIO bitbanging instead.
	 */
	sc->force_bit_dev = true;
	error = -IICBUS_TRANSFER(dev_priv->bbbus[unit], msgs, nmsgs);

out:
	sx_xunlock(&dev_priv->gmbus_sx);
	return (-error);
}

device_t 
intel_gmbus_get_adapter(struct drm_i915_private *dev_priv,
    unsigned port)
{

	if (!intel_gmbus_is_port_valid(port))
		DRM_ERROR("GMBUS get adapter %d: invalid port\n", port);
	return (intel_gmbus_is_port_valid(port) ? dev_priv->gmbus[port - 1] :
	    NULL);
}

void
intel_gmbus_set_speed(device_t idev, int speed)
{
	struct intel_iic_softc *sc;

	sc = device_get_softc(device_get_parent(idev));

	sc->reg0 = (sc->reg0 & ~(0x3 << 8)) | speed;
}

void
intel_gmbus_force_bit(device_t idev, bool force_bit)
{
	struct intel_iic_softc *sc;

	sc = device_get_softc(device_get_parent(idev));
	sc->force_bit_dev = force_bit;
}

static int
intel_iicbb_pre_xfer(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_i915_private *dev_priv;

	sc = device_get_softc(idev);
	dev_priv = sc->drm_dev->dev_private;

	intel_iic_reset(sc->drm_dev);
	intel_iic_quirk_set(dev_priv, true);
	IICBB_SETSDA(idev, 1);
	IICBB_SETSCL(idev, 1);
	DELAY(I2C_RISEFALL_TIME);
	return (0);
}

static void
intel_iicbb_post_xfer(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_i915_private *dev_priv;

	sc = device_get_softc(idev);
	dev_priv = sc->drm_dev->dev_private;

	IICBB_SETSDA(idev, 1);
	IICBB_SETSCL(idev, 1);
	intel_iic_quirk_set(dev_priv, false);
}

static int
intel_gmbus_probe(device_t dev)
{

	return (BUS_PROBE_SPECIFIC);
}

static int
intel_gmbus_attach(device_t idev)
{
	struct drm_i915_private *dev_priv;
	struct intel_iic_softc *sc;
	int pin, port;

	sc = device_get_softc(idev);
	sc->drm_dev = device_get_softc(device_get_parent(idev));
	dev_priv = sc->drm_dev->dev_private;
	pin = device_get_unit(idev);
	port = pin + 1;

	snprintf(sc->name, sizeof(sc->name), "gmbus %s",
	    intel_gmbus_is_port_valid(port) ? gmbus_ports[pin].name :
	    "reserved");
	device_set_desc(idev, sc->name);

	/* By default use a conservative clock rate */
	sc->reg0 = port | GMBUS_RATE_100KHZ;

	/* gmbus seems to be broken on i830 */
	if (IS_I830(sc->drm_dev))
		sc->force_bit_dev = true;
#if 0
	if (IS_GEN2(sc->drm_dev)) {
		sc->force_bit_dev = true;
	}
#endif

	/* add bus interface device */
	sc->iic_dev = device_add_child(idev, "iicbus", -1);
	if (sc->iic_dev == NULL)
		return (ENXIO);
	device_quiet(sc->iic_dev);
	bus_generic_attach(idev);

	return (0);
}

static int
intel_gmbus_detach(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_i915_private *dev_priv;
	device_t child;
	int u;

	sc = device_get_softc(idev);
	u = device_get_unit(idev);
	dev_priv = sc->drm_dev->dev_private;

	child = sc->iic_dev;
	bus_generic_detach(idev);
	if (child != NULL)
		device_delete_child(idev, child);

	return (0);
}

static int
intel_iicbb_probe(device_t dev)
{

	return (BUS_PROBE_DEFAULT);
}

static int
intel_iicbb_attach(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_i915_private *dev_priv;
	int pin, port;

	sc = device_get_softc(idev);
	sc->drm_dev = device_get_softc(device_get_parent(idev));
	dev_priv = sc->drm_dev->dev_private;
	pin = device_get_unit(idev);
	port = pin + 1;

	snprintf(sc->name, sizeof(sc->name), "i915 iicbb %s",
	    intel_gmbus_is_port_valid(port) ? gmbus_ports[pin].name :
	    "reserved");
	device_set_desc(idev, sc->name);

	if (!intel_gmbus_is_port_valid(port))
		pin = 1 ; /* GPIOA, VGA */
	sc->reg0 = pin | GMBUS_RATE_100KHZ;
	sc->reg = dev_priv->gpio_mmio_base + gmbus_ports[pin].reg;

	/* add generic bit-banging code */
	sc->iic_dev = device_add_child(idev, "iicbb", -1);
	if (sc->iic_dev == NULL)
		return (ENXIO);
	device_quiet(sc->iic_dev);
	bus_generic_attach(idev);
	iicbus_set_nostop(idev, true);

	return (0);
}

static int
intel_iicbb_detach(device_t idev)
{
	struct intel_iic_softc *sc;
	device_t child;

	sc = device_get_softc(idev);
	child = sc->iic_dev;
	bus_generic_detach(idev);
	if (child)
		device_delete_child(idev, child);
	return (0);
}

static device_method_t intel_gmbus_methods[] = {
	DEVMETHOD(device_probe,		intel_gmbus_probe),
	DEVMETHOD(device_attach,	intel_gmbus_attach),
	DEVMETHOD(device_detach,	intel_gmbus_detach),
	DEVMETHOD(iicbus_reset,		intel_iicbus_reset),
	DEVMETHOD(iicbus_transfer,	intel_gmbus_transfer),
	DEVMETHOD_END
};
static driver_t intel_gmbus_driver = {
	"intel_gmbus",
	intel_gmbus_methods,
	sizeof(struct intel_iic_softc)
};
static devclass_t intel_gmbus_devclass;
DRIVER_MODULE_ORDERED(intel_gmbus, drmn, intel_gmbus_driver,
    intel_gmbus_devclass, 0, 0, SI_ORDER_FIRST);
DRIVER_MODULE(iicbus, intel_gmbus, iicbus_driver, iicbus_devclass, 0, 0);

static device_method_t intel_iicbb_methods[] =	{
	DEVMETHOD(device_probe,		intel_iicbb_probe),
	DEVMETHOD(device_attach,	intel_iicbb_attach),
	DEVMETHOD(device_detach,	intel_iicbb_detach),

	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	DEVMETHOD(iicbb_callback,	iicbus_null_callback),
	DEVMETHOD(iicbb_reset,		intel_iicbus_reset),
	DEVMETHOD(iicbb_setsda,		intel_iicbb_setsda),
	DEVMETHOD(iicbb_setscl,		intel_iicbb_setscl),
	DEVMETHOD(iicbb_getsda,		intel_iicbb_getsda),
	DEVMETHOD(iicbb_getscl,		intel_iicbb_getscl),
	DEVMETHOD(iicbb_pre_xfer,	intel_iicbb_pre_xfer),
	DEVMETHOD(iicbb_post_xfer,	intel_iicbb_post_xfer),
	DEVMETHOD_END
};
static driver_t intel_iicbb_driver = {
	"intel_iicbb",
	intel_iicbb_methods,
	sizeof(struct intel_iic_softc)
};
static devclass_t intel_iicbb_devclass;
DRIVER_MODULE_ORDERED(intel_iicbb, drmn, intel_iicbb_driver,
    intel_iicbb_devclass, 0, 0, SI_ORDER_FIRST);
DRIVER_MODULE(iicbb, intel_iicbb, iicbb_driver, iicbb_devclass, 0, 0);

int
intel_setup_gmbus(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv;
	device_t iic_dev;
	int i, ret;

	dev_priv = dev->dev_private;
	sx_init(&dev_priv->gmbus_sx, "gmbus");
	if (HAS_PCH_SPLIT(dev))
		dev_priv->gpio_mmio_base = PCH_GPIOA - GPIOA;
	else
		dev_priv->gpio_mmio_base = 0;

	/*
	 * The Giant there is recursed, most likely.  Normally, the
	 * intel_setup_gmbus() is called from the attach method of the
	 * driver.
	 */
	mtx_lock(&Giant);
	for (i = 0; i <= GMBUS_NUM_PORTS; i++) {
		/*
		 * Initialized bbbus_bridge before gmbus_bridge, since
		 * gmbus may decide to force quirk transfer in the
		 * attachment code.
		 */
		dev_priv->bbbus_bridge[i] = device_add_child(dev->dev,
		    "intel_iicbb", i);
		if (dev_priv->bbbus_bridge[i] == NULL) {
			DRM_ERROR("bbbus bridge %d creation failed\n", i);
			ret = -ENXIO;
			goto err;
		}
		device_quiet(dev_priv->bbbus_bridge[i]);
		ret = -device_probe_and_attach(dev_priv->bbbus_bridge[i]);
		if (ret != 0) {
			DRM_ERROR("bbbus bridge %d attach failed, %d\n", i,
			    ret);
			goto err;
		}

		iic_dev = device_find_child(dev_priv->bbbus_bridge[i], "iicbb",
		    -1);
		if (iic_dev == NULL) {
			DRM_ERROR("bbbus bridge doesn't have iicbb child\n");
			goto err;
		}
		iic_dev = device_find_child(iic_dev, "iicbus", -1);
		if (iic_dev == NULL) {
			DRM_ERROR(
		"bbbus bridge doesn't have iicbus grandchild\n");
			goto err;
		}

		dev_priv->bbbus[i] = iic_dev;

		dev_priv->gmbus_bridge[i] = device_add_child(dev->dev,
		    "intel_gmbus", i);
		if (dev_priv->gmbus_bridge[i] == NULL) {
			DRM_ERROR("gmbus bridge %d creation failed\n", i);
			ret = -ENXIO;
			goto err;
		}
		device_quiet(dev_priv->gmbus_bridge[i]);
		ret = -device_probe_and_attach(dev_priv->gmbus_bridge[i]);
		if (ret != 0) {
			DRM_ERROR("gmbus bridge %d attach failed, %d\n", i,
			    ret);
			ret = -ENXIO;
			goto err;
		}

		iic_dev = device_find_child(dev_priv->gmbus_bridge[i],
		    "iicbus", -1);
		if (iic_dev == NULL) {
			DRM_ERROR("gmbus bridge doesn't have iicbus child\n");
			goto err;
		}
		dev_priv->gmbus[i] = iic_dev;

		intel_iic_reset(dev);
	}

	mtx_unlock(&Giant);
	return (0);

err:
	intel_teardown_gmbus_m(dev, i);
	mtx_unlock(&Giant);
	return (ret);
}

static void
intel_teardown_gmbus_m(struct drm_device *dev, int m)
{
	struct drm_i915_private *dev_priv;

	dev_priv = dev->dev_private;

	sx_destroy(&dev_priv->gmbus_sx);
}

void
intel_teardown_gmbus(struct drm_device *dev)
{

	mtx_lock(&Giant);
	intel_teardown_gmbus_m(dev, GMBUS_NUM_PORTS);
	mtx_unlock(&Giant);
}
