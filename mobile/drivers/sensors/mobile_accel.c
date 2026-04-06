/*
 * Mobile Accelerometer Driver
 *
 * This driver provides support for accelerometer sensors commonly found
 * in mobile devices. It uses I2C interface and provides motion data.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>

#define ACCEL_I2C_ADDR		0x1D	/* Example accelerometer address */
#define ACCEL_REG_X_LOW		0x00
#define ACCEL_REG_X_HIGH	0x01
#define ACCEL_REG_Y_LOW		0x02
#define ACCEL_REG_Y_HIGH	0x03
#define ACCEL_REG_Z_LOW		0x04
#define ACCEL_REG_Z_HIGH	0x05
#define ACCEL_REG_CTRL		0x2A

struct mobile_accel_softc {
	device_t	dev;
	uint32_t	i2c_addr;
	struct mtx	mtx;
	int16_t		x, y, z;
};

static int
mobile_accel_probe(device_t dev)
{
	device_set_desc(dev, "Mobile Accelerometer");
	return (BUS_PROBE_DEFAULT);
}

static int
mobile_accel_attach(device_t dev)
{
	struct mobile_accel_softc *sc = device_get_softc(dev);
	uint8_t ctrl_val;

	sc->dev = dev;
	sc->i2c_addr = ACCEL_I2C_ADDR;

	mtx_init(&sc->mtx, "mobile_accel", NULL, MTX_DEF);

	/* Initialize accelerometer */
	ctrl_val = 0x01; /* Active mode */
	if (iicdev_writew(dev, sc->i2c_addr, ACCEL_REG_CTRL, ctrl_val) != 0) {
		device_printf(dev, "failed to initialize accelerometer\n");
		return (ENXIO);
	}

	device_printf(dev, "Mobile accelerometer attached at 0x%x\n", sc->i2c_addr);
	return (0);
}

static int
mobile_accel_detach(device_t dev)
{
	struct mobile_accel_softc *sc = device_get_softc(dev);

	mtx_destroy(&sc->mtx);
	return (0);
}

static int
mobile_accel_read_data(struct mobile_accel_softc *sc)
{
	uint8_t x_low, x_high, y_low, y_high, z_low, z_high;

	mtx_lock(&sc->mtx);

	if (iicdev_readfrom(sc->dev, sc->i2c_addr, ACCEL_REG_X_LOW, &x_low, 1, IIC_WAIT) != 0)
		goto error;
	if (iicdev_readfrom(sc->dev, sc->i2c_addr, ACCEL_REG_X_HIGH, &x_high, 1, IIC_WAIT) != 0)
		goto error;
	if (iicdev_readfrom(sc->dev, sc->i2c_addr, ACCEL_REG_Y_LOW, &y_low, 1, IIC_WAIT) != 0)
		goto error;
	if (iicdev_readfrom(sc->dev, sc->i2c_addr, ACCEL_REG_Y_HIGH, &y_high, 1, IIC_WAIT) != 0)
		goto error;
	if (iicdev_readfrom(sc->dev, sc->i2c_addr, ACCEL_REG_Z_LOW, &z_low, 1, IIC_WAIT) != 0)
		goto error;
	if (iicdev_readfrom(sc->dev, sc->i2c_addr, ACCEL_REG_Z_HIGH, &z_high, 1, IIC_WAIT) != 0)
		goto error;

	sc->x = (int16_t)((x_high << 8) | x_low);
	sc->y = (int16_t)((y_high << 8) | y_low);
	sc->z = (int16_t)((z_high << 8) | z_low);

	mtx_unlock(&sc->mtx);
	return (0);

error:
	mtx_unlock(&sc->mtx);
	return (EIO);
}

static device_method_t mobile_accel_methods[] = {
	DEVMETHOD(device_probe, mobile_accel_probe),
	DEVMETHOD(device_attach, mobile_accel_attach),
	DEVMETHOD(device_detach, mobile_accel_detach),
	DEVMETHOD_END
};

static driver_t mobile_accel_driver = {
	"mobile_accel",
	mobile_accel_methods,
	sizeof(struct mobile_accel_softc)
};

DRIVER_MODULE(mobile_accel, iicbus, mobile_accel_driver, 0, 0);
MODULE_DEPEND(mobile_accel, iicbus, 1, 1, 1);