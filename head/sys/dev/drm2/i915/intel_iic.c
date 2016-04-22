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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>
#include <dev/drm2/i915/intel_drv.h>
#include <dev/drm2/i915/i915_drm.h>
#include <dev/drm2/i915/i915_drv.h>
#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include "iicbus_if.h"
#include "iicbb_if.h"

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

/*
 * FIXME Linux<->FreeBSD: dvo_ns2501.C wants the struct intel_gmbus
 * below but it just has the device_t at hand. It still uses
 * device_get_softc(), thus expects struct intel_gmbus to remain the
 * first member.
 */
struct intel_iic_softc {
	struct intel_gmbus *bus;
	device_t iic_dev;
	char name[32];
};

static inline struct intel_gmbus *
to_intel_gmbus(device_t i2c)
{
	struct intel_iic_softc *sc;

	sc = device_get_softc(i2c);
	return sc->bus;
}

bool intel_gmbus_is_forced_bit(device_t adapter)
{
	struct intel_iic_softc *sc = device_get_softc(adapter);
	struct intel_gmbus *bus = sc->bus;

	return bus->force_bit;
}

void
intel_i2c_reset(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	I915_WRITE(dev_priv->gpio_mmio_base + GMBUS0, 0);
}

static int
intel_iicbus_reset(device_t idev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct intel_iic_softc *sc;
	struct drm_device *dev;

	sc = device_get_softc(idev);
	dev = sc->bus->dev_priv->dev;

	intel_i2c_reset(dev);
	return (0);
}

static void intel_i2c_quirk_set(struct drm_i915_private *dev_priv, bool enable)
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

static u32 get_reserved(struct intel_gmbus *bus)
{
	struct drm_i915_private *dev_priv = bus->dev_priv;
	struct drm_device *dev = dev_priv->dev;
	u32 reserved = 0;

	/* On most chips, these bits must be preserved in software. */
	if (!IS_I830(dev) && !IS_845G(dev))
		reserved = I915_READ_NOTRACE(bus->gpio_reg) &
					     (GPIO_DATA_PULLUP_DISABLE |
					      GPIO_CLOCK_PULLUP_DISABLE);

	return reserved;
}

static int get_clock(device_t adapter)
{
	struct intel_iic_softc *sc = device_get_softc(adapter);
	struct intel_gmbus *bus = sc->bus;
	struct drm_i915_private *dev_priv = bus->dev_priv;
	u32 reserved = get_reserved(bus);
	I915_WRITE_NOTRACE(bus->gpio_reg, reserved | GPIO_CLOCK_DIR_MASK);
	I915_WRITE_NOTRACE(bus->gpio_reg, reserved);
	return (I915_READ_NOTRACE(bus->gpio_reg) & GPIO_CLOCK_VAL_IN) != 0;
}

static int get_data(device_t adapter)
{
	struct intel_iic_softc *sc = device_get_softc(adapter);
	struct intel_gmbus *bus = sc->bus;
	struct drm_i915_private *dev_priv = bus->dev_priv;
	u32 reserved = get_reserved(bus);
	I915_WRITE_NOTRACE(bus->gpio_reg, reserved | GPIO_DATA_DIR_MASK);
	I915_WRITE_NOTRACE(bus->gpio_reg, reserved);
	return (I915_READ_NOTRACE(bus->gpio_reg) & GPIO_DATA_VAL_IN) != 0;
}

static void set_clock(device_t adapter, int state_high)
{
	struct intel_iic_softc *sc = device_get_softc(adapter);
	struct intel_gmbus *bus = sc->bus;
	struct drm_i915_private *dev_priv = bus->dev_priv;
	u32 reserved = get_reserved(bus);
	u32 clock_bits;

	if (state_high)
		clock_bits = GPIO_CLOCK_DIR_IN | GPIO_CLOCK_DIR_MASK;
	else
		clock_bits = GPIO_CLOCK_DIR_OUT | GPIO_CLOCK_DIR_MASK |
			GPIO_CLOCK_VAL_MASK;

	I915_WRITE_NOTRACE(bus->gpio_reg, reserved | clock_bits);
	POSTING_READ(bus->gpio_reg);
}

static void set_data(device_t adapter, int state_high)
{
	struct intel_iic_softc *sc = device_get_softc(adapter);
	struct intel_gmbus *bus = sc->bus;
	struct drm_i915_private *dev_priv = bus->dev_priv;
	u32 reserved = get_reserved(bus);
	u32 data_bits;

	if (state_high)
		data_bits = GPIO_DATA_DIR_IN | GPIO_DATA_DIR_MASK;
	else
		data_bits = GPIO_DATA_DIR_OUT | GPIO_DATA_DIR_MASK |
			GPIO_DATA_VAL_MASK;

	I915_WRITE_NOTRACE(bus->gpio_reg, reserved | data_bits);
	POSTING_READ(bus->gpio_reg);
}

static int
intel_gpio_pre_xfer(device_t adapter)
{
	struct intel_iic_softc *sc = device_get_softc(adapter);
	struct intel_gmbus *bus = sc->bus;
	struct drm_i915_private *dev_priv = bus->dev_priv;

	intel_i2c_reset(dev_priv->dev);
	intel_i2c_quirk_set(dev_priv, true);
	IICBB_SETSDA(adapter, 1);
	IICBB_SETSCL(adapter, 1);
	udelay(I2C_RISEFALL_TIME);
	return 0;
}

static void
intel_gpio_post_xfer(device_t adapter)
{
	struct intel_iic_softc *sc = device_get_softc(adapter);
	struct intel_gmbus *bus = sc->bus;
	struct drm_i915_private *dev_priv = bus->dev_priv;

	IICBB_SETSDA(adapter, 1);
	IICBB_SETSCL(adapter, 1);
	intel_i2c_quirk_set(dev_priv, false);
}

static void
intel_gpio_setup(struct intel_gmbus *bus, u32 pin)
{
	struct drm_i915_private *dev_priv = bus->dev_priv;

	/* -1 to map pin pair to gmbus index */
	bus->gpio_reg = dev_priv->gpio_mmio_base + gmbus_ports[pin - 1].reg;
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

		ret = wait_for((gmbus2 = I915_READ(GMBUS2 + reg_offset)) &
			       (GMBUS_SATOER | GMBUS_HW_RDY),
			       50);
		if (ret)
			return -ETIMEDOUT;
		if (gmbus2 & GMBUS_SATOER)
			return -ENXIO;

		val = I915_READ(GMBUS3 + reg_offset);
		do {
			*buf++ = val & 0xff;
			val >>= 8;
		} while (--len && ++loop < 4);
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
		} while (--len && ++loop < 4);

		I915_WRITE(GMBUS3 + reg_offset, val);

		ret = wait_for((gmbus2 = I915_READ(GMBUS2 + reg_offset)) &
			       (GMBUS_SATOER | GMBUS_HW_RDY),
			       50);
		if (ret)
			return -ETIMEDOUT;
		if (gmbus2 & GMBUS_SATOER)
			return -ENXIO;
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
		!(msgs[i].flags & I2C_M_RD) && msgs[i].len <= 2 &&
		(msgs[i + 1].flags & I2C_M_RD));
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
gmbus_xfer(device_t adapter,
	   struct iic_msg *msgs,
	   uint32_t num)
{
	struct intel_iic_softc *sc = device_get_softc(adapter);
	struct intel_gmbus *bus = sc->bus;
	struct drm_i915_private *dev_priv = bus->dev_priv;
	int i, reg_offset;
	int ret = 0;

	sx_xlock(&dev_priv->gmbus_mutex);

	if (bus->force_bit) {
		ret = -IICBUS_TRANSFER(bus->bbbus, msgs, num);
		goto out;
	}

	reg_offset = dev_priv->gpio_mmio_base;

	I915_WRITE(GMBUS0 + reg_offset, bus->reg0);

	for (i = 0; i < num; i++) {
		u32 gmbus2;

		if (gmbus_is_index_read(msgs, i, num)) {
			ret = gmbus_xfer_index_read(dev_priv, &msgs[i]);
			i += 1;  /* set i to the index of the read xfer */
		} else if (msgs[i].flags & I2C_M_RD) {
			ret = gmbus_xfer_read(dev_priv, &msgs[i], 0);
		} else {
			ret = gmbus_xfer_write(dev_priv, &msgs[i]);
		}

		if (ret == -ETIMEDOUT)
			goto timeout;
		if (ret == -ENXIO)
			goto clear_err;

		ret = wait_for((gmbus2 = I915_READ(GMBUS2 + reg_offset)) &
			       (GMBUS_SATOER | GMBUS_HW_WAIT_PHASE),
			       50);
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
	if (wait_for((I915_READ(GMBUS2 + reg_offset) & GMBUS_ACTIVE) == 0,
		     10)) {
		DRM_DEBUG_KMS("GMBUS [%s] timed out waiting for idle\n",
			 device_get_desc(adapter));
		ret = -ETIMEDOUT;
	}
	I915_WRITE(GMBUS0 + reg_offset, 0);
	goto out;

clear_err:
	/*
	 * Wait for bus to IDLE before clearing NAK.
	 * If we clear the NAK while bus is still active, then it will stay
	 * active and the next transaction may fail.
	 *
	 * If no ACK is received during the address phase of a transaction, the
	 * adapter must report -ENXIO. It is not clear what to return if no ACK
	 * is received at other times. But we have to be careful to not return
	 * spurious -ENXIO because that will prevent i2c and drm edid functions
	 * from retrying. So return -ENXIO only when gmbus properly quiescents -
	 * timing out seems to happen when there _is_ a ddc chip present, but
	 * it's slow responding and only answers on the 2nd retry.
	 */
	ret = -ENXIO;
	if (wait_for((I915_READ(GMBUS2 + reg_offset) & GMBUS_ACTIVE) == 0,
		     10)) {
		DRM_DEBUG_KMS("GMBUS [%s] timed out after NAK\n",
			      device_get_desc(adapter));
		ret = -ETIMEDOUT;
	}

	/* Toggle the Software Clear Interrupt bit. This has the effect
	 * of resetting the GMBUS controller and so clearing the
	 * BUS_ERROR raised by the slave's NAK.
	 */
	I915_WRITE(GMBUS1 + reg_offset, GMBUS_SW_CLR_INT);
	I915_WRITE(GMBUS1 + reg_offset, 0);
	I915_WRITE(GMBUS0 + reg_offset, 0);

	DRM_DEBUG_KMS("GMBUS [%s] NAK for addr: %04x %c(%d)\n",
			 device_get_desc(adapter), msgs[i].slave >> 1,
			 (msgs[i].flags & I2C_M_RD) ? 'r' : 'w', msgs[i].len);

	goto out;

timeout:
	DRM_INFO("GMBUS [%s] timed out, falling back to bit banging on pin %d\n",
		 device_get_desc(adapter), bus->reg0 & 0xff);
	I915_WRITE(GMBUS0 + reg_offset, 0);

	/* Hardware may not support GMBUS over these pins? Try GPIO bitbanging instead. */
	bus->force_bit = 1;
	ret = -IICBUS_TRANSFER(bus->bbbus, msgs, num);

out:
	sx_xunlock(&dev_priv->gmbus_mutex);
	return -ret;
}

static int
intel_gmbus_probe(device_t dev)
{

	return (BUS_PROBE_SPECIFIC);
}

static int
intel_gmbus_attach(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;
	int pin, port;

	sc = device_get_softc(idev);
	pin = device_get_unit(idev);
	port = pin + 1; /* +1 to map gmbus index to pin pair */

	snprintf(sc->name, sizeof(sc->name), "i915 gmbus %s",
	    intel_gmbus_is_port_valid(port) ? gmbus_ports[pin].name :
	    "reserved");
	device_set_desc(idev, sc->name);

	dev = device_get_softc(device_get_parent(idev));
	dev_priv = dev->dev_private;
	sc->bus = &dev_priv->gmbus[pin];

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

	bus_generic_detach(idev);
	device_delete_children(idev);

	return (0);
}

static device_method_t intel_gmbus_methods[] = {
	DEVMETHOD(device_probe,		intel_gmbus_probe),
	DEVMETHOD(device_attach,	intel_gmbus_attach),
	DEVMETHOD(device_detach,	intel_gmbus_detach),
	DEVMETHOD(iicbus_reset,		intel_iicbus_reset),
	DEVMETHOD(iicbus_transfer,	gmbus_xfer),
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

static int
intel_iicbb_probe(device_t dev)
{

	return (BUS_PROBE_DEFAULT);
}

static int
intel_iicbb_attach(device_t idev)
{
	struct intel_iic_softc *sc;
	struct drm_device *dev;
	struct drm_i915_private *dev_priv;
	int pin, port;

	sc = device_get_softc(idev);
	pin = device_get_unit(idev);
	port = pin + 1;

	snprintf(sc->name, sizeof(sc->name), "i915 iicbb %s",
	    intel_gmbus_is_port_valid(port) ? gmbus_ports[pin].name :
	    "reserved");
	device_set_desc(idev, sc->name);

	dev = device_get_softc(device_get_parent(idev));
	dev_priv = dev->dev_private;
	sc->bus = &dev_priv->gmbus[pin];

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

	bus_generic_detach(idev);
	device_delete_children(idev);

	return (0);
}

static device_method_t intel_iicbb_methods[] =	{
	DEVMETHOD(device_probe,		intel_iicbb_probe),
	DEVMETHOD(device_attach,	intel_iicbb_attach),
	DEVMETHOD(device_detach,	intel_iicbb_detach),

	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	DEVMETHOD(iicbb_callback,	iicbus_null_callback),
	DEVMETHOD(iicbb_reset,		intel_iicbus_reset),
	DEVMETHOD(iicbb_setsda,		set_data),
	DEVMETHOD(iicbb_setscl,		set_clock),
	DEVMETHOD(iicbb_getsda,		get_data),
	DEVMETHOD(iicbb_getscl,		get_clock),
	DEVMETHOD(iicbb_pre_xfer,	intel_gpio_pre_xfer),
	DEVMETHOD(iicbb_post_xfer,	intel_gpio_post_xfer),
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

/**
 * intel_gmbus_setup - instantiate all Intel i2c GMBuses
 * @dev: DRM device
 */
int intel_setup_gmbus(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	device_t iic_dev;
	int ret, i;

	if (HAS_PCH_SPLIT(dev))
		dev_priv->gpio_mmio_base = PCH_GPIOA - GPIOA;
	else
		dev_priv->gpio_mmio_base = 0;

	sx_init(&dev_priv->gmbus_mutex, "gmbus");

	/*
	 * The Giant there is recursed, most likely.  Normally, the
	 * intel_setup_gmbus() is called from the attach method of the
	 * driver.
	 */
	mtx_lock(&Giant);
	for (i = 0; i < GMBUS_NUM_PORTS; i++) {
		struct intel_gmbus *bus = &dev_priv->gmbus[i];
		u32 port = i + 1; /* +1 to map gmbus index to pin pair */

		bus->dev_priv = dev_priv;

		/* By default use a conservative clock rate */
		bus->reg0 = port | GMBUS_RATE_100KHZ;

		/* gmbus seems to be broken on i830 */
		if (IS_I830(dev))
			bus->force_bit = 1;

		intel_gpio_setup(bus, port);

		/*
		 * bbbus_bridge
		 *
		 * Initialized bbbus_bridge before gmbus_bridge, since
		 * gmbus may decide to force quirk transfer in the
		 * attachment code.
		 */
		bus->bbbus_bridge = device_add_child(dev->dev,
		    "intel_iicbb", i);
		if (bus->bbbus_bridge == NULL) {
			DRM_ERROR("bbbus bridge %d creation failed\n", i);
			ret = -ENXIO;
			goto err;
		}
		device_quiet(bus->bbbus_bridge);
		ret = -device_probe_and_attach(bus->bbbus_bridge);
		if (ret != 0) {
			DRM_ERROR("bbbus bridge %d attach failed, %d\n", i,
			    ret);
			goto err;
		}

		/* bbbus */
		iic_dev = device_find_child(bus->bbbus_bridge,
		    "iicbb", -1);
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

		bus->bbbus = iic_dev;

		/* gmbus_bridge */
		bus->gmbus_bridge = device_add_child(dev->dev,
		    "intel_gmbus", i);
		if (bus->gmbus_bridge == NULL) {
			DRM_ERROR("gmbus bridge %d creation failed\n", i);
			ret = -ENXIO;
			goto err;
		}
		device_quiet(bus->gmbus_bridge);
		ret = -device_probe_and_attach(bus->gmbus_bridge);
		if (ret != 0) {
			DRM_ERROR("gmbus bridge %d attach failed, %d\n", i,
			    ret);
			ret = -ENXIO;
			goto err;
		}

		/* gmbus */
		iic_dev = device_find_child(bus->gmbus_bridge,
		    "iicbus", -1);
		if (iic_dev == NULL) {
			DRM_ERROR("gmbus bridge doesn't have iicbus child\n");
			goto err;
		}

		bus->gmbus = iic_dev;
	}
	mtx_unlock(&Giant);

	intel_i2c_reset(dev_priv->dev);

	return 0;

err:
	while (--i) {
		struct intel_gmbus *bus = &dev_priv->gmbus[i];
		if (bus->gmbus_bridge != NULL)
			device_delete_child(dev->dev, bus->gmbus_bridge);
		if (bus->bbbus_bridge != NULL)
			device_delete_child(dev->dev, bus->bbbus_bridge);
	}
	mtx_unlock(&Giant);
	sx_destroy(&dev_priv->gmbus_mutex);
	return ret;
}

device_t intel_gmbus_get_adapter(struct drm_i915_private *dev_priv,
					    unsigned port)
{
	WARN_ON(!intel_gmbus_is_port_valid(port));
	/* -1 to map pin pair to gmbus index */
	return (intel_gmbus_is_port_valid(port)) ?
		dev_priv->gmbus[port - 1].gmbus : NULL;
}

void intel_gmbus_set_speed(device_t adapter, int speed)
{
	struct intel_gmbus *bus = to_intel_gmbus(adapter);

	bus->reg0 = (bus->reg0 & ~(0x3 << 8)) | speed;
}

void intel_gmbus_force_bit(device_t adapter, bool force_bit)
{
	struct intel_gmbus *bus = to_intel_gmbus(adapter);

	bus->force_bit += force_bit ? 1 : -1;
	DRM_DEBUG_KMS("%sabling bit-banging on %s. force bit now %d\n",
		      force_bit ? "en" : "dis", device_get_desc(adapter),
		      bus->force_bit);
}

void intel_teardown_gmbus(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int i;
	int ret;

	for (i = 0; i < GMBUS_NUM_PORTS; i++) {
		struct intel_gmbus *bus = &dev_priv->gmbus[i];

		mtx_lock(&Giant);
		ret = device_delete_child(dev->dev, bus->gmbus_bridge);
		mtx_unlock(&Giant);

		KASSERT(ret == 0, ("unable to detach iic gmbus %s: %d",
		    device_get_desc(bus->gmbus_bridge), ret));

		mtx_lock(&Giant);
		ret = device_delete_child(dev->dev, bus->bbbus_bridge);
		mtx_unlock(&Giant);

		KASSERT(ret == 0, ("unable to detach iic bbbus %s: %d",
		    device_get_desc(bus->bbbus_bridge), ret));
	}

	sx_destroy(&dev_priv->gmbus_mutex);
}
