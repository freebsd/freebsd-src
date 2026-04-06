/*
 * Mobile Touchscreen Driver
 *
 * This driver provides support for capacitive touchscreens commonly found
 * in mobile devices. It interfaces with the HID multitouch subsystem and
 * provides touch event processing optimized for mobile OS.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/evdev/evdev.h>
#include <dev/evdev/input.h>

#define MOBILE_TOUCHSCREEN_VENDOR_ID	0x1234
#define MOBILE_TOUCHSCREEN_PRODUCT_ID	0x5678

struct mobile_touchscreen_softc {
	device_t	dev;
	struct mtx	mtx;
	struct evdev_dev *evdev;
	int		touch_count;
	int		max_touches;
};

static int
mobile_touchscreen_probe(device_t dev)
{
	struct hid_device_info *hw = hid_get_device_info(dev);

	if (hw == NULL)
		return (ENXIO);

	/* Check for touchscreen HID devices */
	if (hid_test_quirk(hw, HQ_HAS_MULTITOUCH)) {
		device_set_desc(dev, "Mobile Touchscreen");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
mobile_touchscreen_attach(device_t dev)
{
	struct mobile_touchscreen_softc *sc = device_get_softc(dev);
	int error;

	sc->dev = dev;
	sc->max_touches = 10; /* Support up to 10 touch points */
	sc->touch_count = 0;

	mtx_init(&sc->mtx, "mobile_touchscreen", NULL, MTX_DEF);

	/* Initialize evdev for input events */
	sc->evdev = evdev_alloc();
	if (sc->evdev == NULL) {
		device_printf(dev, "failed to allocate evdev\n");
		return (ENOMEM);
	}

	evdev_set_name(sc->evdev, "Mobile Touchscreen");
	evdev_set_id(sc->evdev, BUS_USB, MOBILE_TOUCHSCREEN_VENDOR_ID,
	    MOBILE_TOUCHSCREEN_PRODUCT_ID, 0);
	evdev_set_phys(sc->evdev, device_get_nameunit(dev));
	evdev_set_methods(sc->evdev, sc, &mobile_touchscreen_evdev_methods);

	/* Set up touch capabilities */
	evdev_support_event(sc->evdev, EV_SYN);
	evdev_support_event(sc->evdev, EV_KEY);
	evdev_support_event(sc->evdev, EV_ABS);

	/* Touch position */
	evdev_support_abs(sc->evdev, ABS_X, 0, 4095, 0, 0, 0);
	evdev_support_abs(sc->evdev, ABS_Y, 0, 4095, 0, 0, 0);

	/* Multi-touch */
	evdev_support_abs(sc->evdev, ABS_MT_POSITION_X, 0, 4095, 0, 0, 0);
	evdev_support_abs(sc->evdev, ABS_MT_POSITION_Y, 0, 4095, 0, 0, 0);
	evdev_support_abs(sc->evdev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0, 0);
	evdev_support_abs(sc->evdev, ABS_MT_TRACKING_ID, 0, sc->max_touches - 1, 0, 0, 0);

	error = evdev_register(sc->evdev);
	if (error) {
		device_printf(dev, "failed to register evdev\n");
		evdev_free(sc->evdev);
		return (error);
	}

	device_printf(dev, "Mobile touchscreen attached\n");
	return (0);
}

static int
mobile_touchscreen_detach(device_t dev)
{
	struct mobile_touchscreen_softc *sc = device_get_softc(dev);

	if (sc->evdev != NULL) {
		evdev_unregister(sc->evdev);
		evdev_free(sc->evdev);
	}

	mtx_destroy(&sc->mtx);
	return (0);
}

static device_method_t mobile_touchscreen_methods[] = {
	DEVMETHOD(device_probe, mobile_touchscreen_probe),
	DEVMETHOD(device_attach, mobile_touchscreen_attach),
	DEVMETHOD(device_detach, mobile_touchscreen_detach),
	DEVMETHOD_END
};

static driver_t mobile_touchscreen_driver = {
	"mobile_touchscreen",
	mobile_touchscreen_methods,
	sizeof(struct mobile_touchscreen_softc)
};

DRIVER_MODULE(mobile_touchscreen, hidbus, mobile_touchscreen_driver, 0, 0);
MODULE_DEPEND(mobile_touchscreen, hid, 1, 1, 1);
MODULE_DEPEND(mobile_touchscreen, evdev, 1, 1, 1);