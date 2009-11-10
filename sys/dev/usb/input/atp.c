/*-
 * Copyright (c) 2009 Rohit Grover
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/poll.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbhid.h>
#include "usbdevs.h"

#define USB_DEBUG_VAR atp_debug
#include <dev/usb/usb_debug.h>

#include <sys/mouse.h>

#define ATP_DRIVER_NAME "atp"

/*
 * Driver specific options: the following options may be set by
 * `options' statements in the kernel configuration file.
 */

/* The multiplier used to translate sensor reported positions to mickeys. */
#ifndef ATP_SCALE_FACTOR
#define ATP_SCALE_FACTOR 48
#endif

/*
 * This is the age (in microseconds) beyond which a touch is
 * considered to be a slide; and therefore a tap event isn't registered.
 */
#ifndef ATP_TOUCH_TIMEOUT
#define ATP_TOUCH_TIMEOUT 125000
#endif

/*
 * A double-tap followed by a single-finger slide is treated as a
 * special gesture. The driver responds to this gesture by assuming a
 * virtual button-press for the lifetime of the slide. The following
 * threshold is the maximum time gap (in microseconds) between the two
 * tap events preceding the slide for such a gesture.
 */
#ifndef ATP_DOUBLE_TAP_N_DRAG_THRESHOLD
#define ATP_DOUBLE_TAP_N_DRAG_THRESHOLD 200000
#endif

/*
 * The device provides us only with pressure readings from an array of
 * X and Y sensors; for our algorithms, we need to interpret groups
 * (typically pairs) of X and Y readings as being related to a single
 * finger stroke. We can relate X and Y readings based on their times
 * of incidence. The coincidence window should be at least 10000us
 * since it is used against values from getmicrotime(), which has a
 * precision of around 10ms.
 */
#ifndef ATP_COINCIDENCE_THRESHOLD
#define ATP_COINCIDENCE_THRESHOLD  40000 /* unit: microseconds */
#if ATP_COINCIDENCE_THRESHOLD > 100000
#error "ATP_COINCIDENCE_THRESHOLD too large"
#endif
#endif /* #ifndef ATP_COINCIDENCE_THRESHOLD */

/*
 * The wait duration (in microseconds) after losing a touch contact
 * before zombied strokes are reaped and turned into button events.
 */
#define ATP_ZOMBIE_STROKE_REAP_WINDOW   50000
#if ATP_ZOMBIE_STROKE_REAP_WINDOW > 100000
#error "ATP_ZOMBIE_STROKE_REAP_WINDOW too large"
#endif

/* end of driver specific options */


/* Tunables */
SYSCTL_NODE(_hw_usb, OID_AUTO, atp, CTLFLAG_RW, 0, "USB atp");

#if USB_DEBUG
enum atp_log_level {
	ATP_LLEVEL_DISABLED = 0,
	ATP_LLEVEL_ERROR,
	ATP_LLEVEL_DEBUG,       /* for troubleshooting */
	ATP_LLEVEL_INFO,        /* for diagnostics */
};
static int atp_debug = ATP_LLEVEL_ERROR; /* the default is to only log errors */
SYSCTL_INT(_hw_usb_atp, OID_AUTO, debug, CTLFLAG_RW,
    &atp_debug, ATP_LLEVEL_ERROR, "ATP debug level");
#endif /* #if USB_DEBUG */

static u_int atp_touch_timeout = ATP_TOUCH_TIMEOUT;
SYSCTL_INT(_hw_usb_atp, OID_AUTO, touch_timeout, CTLFLAG_RW, &atp_touch_timeout,
    125000, "age threshold (in micros) for a touch");

static u_int atp_double_tap_threshold = ATP_DOUBLE_TAP_N_DRAG_THRESHOLD;
SYSCTL_INT(_hw_usb_atp, OID_AUTO, double_tap_threshold, CTLFLAG_RW,
    &atp_double_tap_threshold, ATP_DOUBLE_TAP_N_DRAG_THRESHOLD,
    "maximum time (in micros) between a double-tap");

static u_int atp_mickeys_scale_factor = ATP_SCALE_FACTOR;
static int atp_sysctl_scale_factor_handler(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_hw_usb_atp, OID_AUTO, scale_factor, CTLTYPE_UINT | CTLFLAG_RW,
    &atp_mickeys_scale_factor, sizeof(atp_mickeys_scale_factor),
    atp_sysctl_scale_factor_handler, "IU", "movement scale factor");

static u_int atp_small_movement_threshold = ATP_SCALE_FACTOR >> 3;
SYSCTL_UINT(_hw_usb_atp, OID_AUTO, small_movement, CTLFLAG_RW,
    &atp_small_movement_threshold, ATP_SCALE_FACTOR >> 3,
    "the small movement black-hole for filtering noise");
/*
 * The movement threshold for a stroke; this is the maximum difference
 * in position which will be resolved as a continuation of a stroke
 * component.
 */
static u_int atp_max_delta_mickeys = ((3 * ATP_SCALE_FACTOR) >> 1);
SYSCTL_UINT(_hw_usb_atp, OID_AUTO, max_delta_mickeys, CTLFLAG_RW,
    &atp_max_delta_mickeys, ((3 * ATP_SCALE_FACTOR) >> 1),
    "max. mickeys-delta which will match against an existing stroke");
/*
 * Strokes which accumulate at least this amount of absolute movement
 * from the aggregate of their components are considered as
 * slides. Unit: mickeys.
 */
static u_int atp_slide_min_movement = (ATP_SCALE_FACTOR >> 3);
SYSCTL_UINT(_hw_usb_atp, OID_AUTO, slide_min_movement, CTLFLAG_RW,
    &atp_slide_min_movement, (ATP_SCALE_FACTOR >> 3),
    "strokes with at least this amt. of movement are considered slides");

/*
 * The minimum age of a stroke for it to be considered mature; this
 * helps filter movements (noise) from immature strokes. Units: interrupts.
 */
static u_int atp_stroke_maturity_threshold = 2;
SYSCTL_UINT(_hw_usb_atp, OID_AUTO, stroke_maturity_threshold, CTLFLAG_RW,
    &atp_stroke_maturity_threshold, 2,
    "the minimum age of a stroke for it to be considered mature");

/* Accept pressure readings from sensors only if above this value. */
static u_int atp_sensor_noise_threshold = 2;
SYSCTL_UINT(_hw_usb_atp, OID_AUTO, sensor_noise_threshold, CTLFLAG_RW,
    &atp_sensor_noise_threshold, 2,
    "accept pressure readings from sensors only if above this value");

/* Ignore pressure spans with cumulative press. below this value. */
static u_int atp_pspan_min_cum_pressure = 10;
SYSCTL_UINT(_hw_usb_atp, OID_AUTO, pspan_min_cum_pressure, CTLFLAG_RW,
    &atp_pspan_min_cum_pressure, 10,
    "ignore pressure spans with cumulative press. below this value");

/* Maximum allowed width for pressure-spans.*/
static u_int atp_pspan_max_width = 4;
SYSCTL_UINT(_hw_usb_atp, OID_AUTO, pspan_max_width, CTLFLAG_RW,
    &atp_pspan_max_width, 4,
    "maximum allowed width (in sensors) for pressure-spans");

/* We support three payload protocols */
typedef enum {
	ATP_PROT_GEYSER1,
	ATP_PROT_GEYSER2,
	ATP_PROT_GEYSER3,
} atp_protocol;

/* Define the various flavours of devices supported by this driver. */
enum {
	ATP_DEV_PARAMS_0,
	ATP_DEV_PARAMS_PBOOK,
	ATP_DEV_PARAMS_PBOOK_15A,
	ATP_DEV_PARAMS_PBOOK_17,
	ATP_N_DEV_PARAMS
};
struct atp_dev_params {
	u_int            data_len;   /* for sensor data */
	u_int            n_xsensors;
	u_int            n_ysensors;
	atp_protocol     prot;
} atp_dev_params[ATP_N_DEV_PARAMS] = {
	[ATP_DEV_PARAMS_0] = {
		.data_len   = 64,
		.n_xsensors = 20,
		.n_ysensors = 10,
		.prot       = ATP_PROT_GEYSER3
	},
	[ATP_DEV_PARAMS_PBOOK] = {
		.data_len   = 81,
		.n_xsensors = 16,
		.n_ysensors = 16,
		.prot       = ATP_PROT_GEYSER1
	},
	[ATP_DEV_PARAMS_PBOOK_15A] = {
		.data_len   = 64,
		.n_xsensors = 15,
		.n_ysensors = 9,
		.prot       = ATP_PROT_GEYSER2
	},
	[ATP_DEV_PARAMS_PBOOK_17] = {
		.data_len   = 81,
		.n_xsensors = 26,
		.n_ysensors = 16,
		.prot       = ATP_PROT_GEYSER1
	},
};

static const struct usb_device_id atp_devs[] = {
	/* Core Duo MacBook & MacBook Pro */
	{ USB_VPI(USB_VENDOR_APPLE, 0x0217, ATP_DEV_PARAMS_0) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0218, ATP_DEV_PARAMS_0) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0219, ATP_DEV_PARAMS_0) },

	/* Core2 Duo MacBook & MacBook Pro */
	{ USB_VPI(USB_VENDOR_APPLE, 0x021a, ATP_DEV_PARAMS_0) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x021b, ATP_DEV_PARAMS_0) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x021c, ATP_DEV_PARAMS_0) },

	/* Core2 Duo MacBook3,1 */
	{ USB_VPI(USB_VENDOR_APPLE, 0x0229, ATP_DEV_PARAMS_0) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x022a, ATP_DEV_PARAMS_0) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x022b, ATP_DEV_PARAMS_0) },

	/* 12 inch PowerBook and iBook */
	{ USB_VPI(USB_VENDOR_APPLE, 0x030a, ATP_DEV_PARAMS_PBOOK) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x030b, ATP_DEV_PARAMS_PBOOK) },

	/* 15 inch PowerBook */
	{ USB_VPI(USB_VENDOR_APPLE, 0x020e, ATP_DEV_PARAMS_PBOOK) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x020f, ATP_DEV_PARAMS_PBOOK) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0215, ATP_DEV_PARAMS_PBOOK_15A) },

	/* 17 inch PowerBook */
	{ USB_VPI(USB_VENDOR_APPLE, 0x020d, ATP_DEV_PARAMS_PBOOK_17) },

};

/*
 * The following structure captures the state of a pressure span along
 * an axis. Each contact with the touchpad results in separate
 * pressure spans along the two axes.
 */
typedef struct atp_pspan {
	u_int width;   /* in units of sensors */
	u_int cum;     /* cumulative compression (from all sensors) */
	u_int cog;     /* center of gravity */
	u_int loc;     /* location (scaled using the mickeys factor) */
	boolean_t matched; /* to track pspans as they match against strokes. */
} atp_pspan;

typedef enum atp_stroke_type {
	ATP_STROKE_TOUCH,
	ATP_STROKE_SLIDE,
} atp_stroke_type;

#define ATP_MAX_PSPANS_PER_AXIS 3

typedef struct atp_stroke_component {
	/* Fields encapsulating the pressure-span. */
	u_int loc;              /* location (scaled) */
	u_int cum_pressure;     /* cumulative compression */
	u_int max_cum_pressure; /* max cumulative compression */
	boolean_t matched; /*to track components as they match against pspans.*/

	/* Fields containing information about movement. */
	int   delta_mickeys;    /* change in location (un-smoothened movement)*/
	int   pending;          /* cum. of pending short movements */
	int   movement;         /* current smoothened movement */
} atp_stroke_component;

typedef enum atp_axis {
	X = 0,
	Y = 1
} atp_axis;

#define ATP_MAX_STROKES         (2 * ATP_MAX_PSPANS_PER_AXIS)

/*
 * The following structure captures a finger contact with the
 * touchpad. A stroke comprises two p-span components and some state.
 */
typedef struct atp_stroke {
	atp_stroke_type      type;
	struct timeval       ctime; /* create time; for coincident siblings. */
	u_int                age;   /*
				     * Unit: interrupts; we maintain
				     * this value in addition to
				     * 'ctime' in order to avoid the
				     * expensive call to microtime()
				     * at every interrupt.
				     */

	atp_stroke_component components[2];
	u_int                velocity_squared; /*
						* Average magnitude (squared)
						* of recent velocity.
						*/
	u_int                cum_movement; /* cum. absolute movement so far */

	uint32_t             flags;  /* the state of this stroke */
#define ATSF_ZOMBIE          0x1
} atp_stroke;

#define ATP_FIFO_BUF_SIZE        8 /* bytes */
#define ATP_FIFO_QUEUE_MAXLEN   50 /* units */

enum {
	ATP_INTR_DT,
	ATP_N_TRANSFER,
};

struct atp_softc {
	device_t               sc_dev;
	struct usb_device     *sc_usb_device;
#define MODE_LENGTH 8
	char                   sc_mode_bytes[MODE_LENGTH]; /* device mode */
	struct mtx             sc_mutex; /* for synchronization */
	struct usb_xfer       *sc_xfer[ATP_N_TRANSFER];
	struct usb_fifo_sc     sc_fifo;

	struct atp_dev_params *sc_params;

	mousehw_t              sc_hw;
	mousemode_t            sc_mode;
	u_int                  sc_pollrate;
	mousestatus_t          sc_status;
	u_int                  sc_state;
#define ATP_ENABLED            0x01
#define ATP_ZOMBIES_EXIST      0x02
#define ATP_DOUBLE_TAP_DRAG    0x04
#define ATP_VALID              0x08

	u_int                  sc_left_margin;
	u_int                  sc_right_margin;

	atp_stroke             sc_strokes[ATP_MAX_STROKES];
	u_int                  sc_n_strokes;

	int8_t                *sensor_data; /* from interrupt packet */
	int                   *base_x;      /* base sensor readings */
	int                   *base_y;
	int                   *cur_x;       /* current sensor readings */
	int                   *cur_y;
	int                   *pressure_x;  /* computed pressures */
	int                   *pressure_y;

	u_int                  sc_idlecount; /* preceding idle interrupts */
#define ATP_IDLENESS_THRESHOLD 10

	struct timeval         sc_reap_time;
	struct timeval         sc_reap_ctime; /*ctime of siblings to be reaped*/
};

/*
 * The last byte of the sensor data contains status bits; the
 * following values define the meanings of these bits.
 */
enum atp_status_bits {
	ATP_STATUS_BUTTON      = (uint8_t)0x01, /* The button was pressed */
	ATP_STATUS_BASE_UPDATE = (uint8_t)0x04, /* Data from an untouched pad.*/
};

typedef enum interface_mode {
	RAW_SENSOR_MODE = (uint8_t)0x04,
	HID_MODE        = (uint8_t)0x08
} interface_mode;

/*
 * function prototypes
 */
static usb_fifo_cmd_t   atp_start_read;
static usb_fifo_cmd_t   atp_stop_read;
static usb_fifo_open_t  atp_open;
static usb_fifo_close_t atp_close;
static usb_fifo_ioctl_t atp_ioctl;

static struct usb_fifo_methods atp_fifo_methods = {
	.f_open       = &atp_open,
	.f_close      = &atp_close,
	.f_ioctl      = &atp_ioctl,
	.f_start_read = &atp_start_read,
	.f_stop_read  = &atp_stop_read,
	.basename[0]  = ATP_DRIVER_NAME,
};

/* device initialization and shutdown */
static usb_error_t   atp_req_get_report(struct usb_device *udev, void *data);
static int           atp_set_device_mode(device_t dev, interface_mode mode);
static int           atp_enable(struct atp_softc *sc);
static void          atp_disable(struct atp_softc *sc);
static int           atp_softc_populate(struct atp_softc *);
static void          atp_softc_unpopulate(struct atp_softc *);

/* sensor interpretation */
static __inline void atp_interpret_sensor_data(const int8_t *, u_int, atp_axis,
			 int *, atp_protocol);
static __inline void atp_get_pressures(int *, const int *, const int *, int);
static void          atp_detect_pspans(int *, u_int, u_int, atp_pspan *,
			 u_int *);

/* movement detection */
static boolean_t     atp_match_stroke_component(atp_stroke_component *,
			 const atp_pspan *);
static void          atp_match_strokes_against_pspans(struct atp_softc *,
			 atp_axis, atp_pspan *, u_int, u_int);
static boolean_t     atp_update_strokes(struct atp_softc *,
			 atp_pspan *, u_int, atp_pspan *, u_int);
static __inline void atp_add_stroke(struct atp_softc *, const atp_pspan *,
			 const atp_pspan *);
static void          atp_add_new_strokes(struct atp_softc *, atp_pspan *,
			 u_int, atp_pspan *, u_int);
static void          atp_advance_stroke_state(struct atp_softc *,
			 atp_stroke *, boolean_t *);
static void          atp_terminate_stroke(struct atp_softc *, u_int);
static __inline boolean_t atp_stroke_has_small_movement(const atp_stroke *);
static __inline void atp_update_pending_mickeys(atp_stroke_component *);
static void          atp_compute_smoothening_scale_ratio(atp_stroke *, int *,
			 int *);
static boolean_t     atp_compute_stroke_movement(atp_stroke *);

/* tap detection */
static __inline void atp_setup_reap_time(struct atp_softc *, struct timeval *);
static void          atp_reap_zombies(struct atp_softc *, u_int *, u_int *);

/* updating fifo */
static void          atp_reset_buf(struct atp_softc *sc);
static void          atp_add_to_queue(struct atp_softc *, int, int, uint32_t);


usb_error_t
atp_req_get_report(struct usb_device *udev, void *data)
{
	struct usb_device_request req;

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_REPORT;
	USETW2(req.wValue, (uint8_t)0x03 /* type */, (uint8_t)0x00 /* id */);
	USETW(req.wIndex, 0);
	USETW(req.wLength, MODE_LENGTH);

	return (usbd_do_request(udev, NULL /* mutex */, &req, data));
}

static int
atp_set_device_mode(device_t dev, interface_mode mode)
{
	struct atp_softc     *sc;
	usb_device_request_t  req;
	usb_error_t           err;

	if ((mode != RAW_SENSOR_MODE) && (mode != HID_MODE))
		return (ENXIO);

	sc = device_get_softc(dev);

	sc->sc_mode_bytes[0] = mode;
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_REPORT;
	USETW2(req.wValue, (uint8_t)0x03 /* type */, (uint8_t)0x00 /* id */);
	USETW(req.wIndex, 0);
	USETW(req.wLength, MODE_LENGTH);
	err = usbd_do_request(sc->sc_usb_device, NULL, &req, sc->sc_mode_bytes);
	if (err != USB_ERR_NORMAL_COMPLETION)
		return (ENXIO);

	return (0);
}

static int
atp_enable(struct atp_softc *sc)
{
	/* Allocate the dynamic buffers */
	if (atp_softc_populate(sc) != 0) {
		atp_softc_unpopulate(sc);
		return (ENOMEM);
	}

	/* reset status */
	memset(sc->sc_strokes, 0, sizeof(sc->sc_strokes));
	sc->sc_n_strokes = 0;
	memset(&sc->sc_status, 0, sizeof(sc->sc_status));
	sc->sc_idlecount = 0;
	sc->sc_state |= ATP_ENABLED;

	DPRINTFN(ATP_LLEVEL_INFO, "enabled atp\n");
	return (0);
}

static void
atp_disable(struct atp_softc *sc)
{
	atp_softc_unpopulate(sc);

	sc->sc_state &= ~(ATP_ENABLED | ATP_VALID);
	DPRINTFN(ATP_LLEVEL_INFO, "disabled atp\n");
}

/* Allocate dynamic memory for some fields in softc. */
static int
atp_softc_populate(struct atp_softc *sc)
{
	const struct atp_dev_params *params = sc->sc_params;

	if (params == NULL) {
		DPRINTF("params uninitialized!\n");
		return (ENXIO);
	}
	if (params->data_len) {
		sc->sensor_data = malloc(params->data_len * sizeof(int8_t),
		    M_USB, M_WAITOK);
		if (sc->sensor_data == NULL) {
			DPRINTF("mem for sensor_data\n");
			return (ENXIO);
		}
	}

	if (params->n_xsensors != 0) {
		sc->base_x = malloc(params->n_xsensors * sizeof(*(sc->base_x)),
		    M_USB, M_WAITOK);
		if (sc->base_x == NULL) {
			DPRINTF("mem for sc->base_x\n");
			return (ENXIO);
		}

		sc->cur_x = malloc(params->n_xsensors * sizeof(*(sc->cur_x)),
		    M_USB, M_WAITOK);
		if (sc->cur_x == NULL) {
			DPRINTF("mem for sc->cur_x\n");
			return (ENXIO);
		}

		sc->pressure_x =
			malloc(params->n_xsensors * sizeof(*(sc->pressure_x)),
			    M_USB, M_WAITOK);
		if (sc->pressure_x == NULL) {
			DPRINTF("mem. for pressure_x\n");
			return (ENXIO);
		}
	}

	if (params->n_ysensors != 0) {
		sc->base_y = malloc(params->n_ysensors * sizeof(*(sc->base_y)),
		    M_USB, M_WAITOK);
		if (sc->base_y == NULL) {
			DPRINTF("mem for base_y\n");
			return (ENXIO);
		}

		sc->cur_y = malloc(params->n_ysensors * sizeof(*(sc->cur_y)),
		    M_USB, M_WAITOK);
		if (sc->cur_y == NULL) {
			DPRINTF("mem for cur_y\n");
			return (ENXIO);
		}

		sc->pressure_y =
			malloc(params->n_ysensors * sizeof(*(sc->pressure_y)),
			    M_USB, M_WAITOK);
		if (sc->pressure_y == NULL) {
			DPRINTF("mem. for pressure_y\n");
			return (ENXIO);
		}
	}

	return (0);
}

/* Free dynamic memory allocated for some fields in softc. */
static void
atp_softc_unpopulate(struct atp_softc *sc)
{
	const struct atp_dev_params *params = sc->sc_params;

	if (params == NULL) {
		return;
	}
	if (params->n_xsensors != 0) {
		if (sc->base_x != NULL) {
			free(sc->base_x, M_USB);
			sc->base_x = NULL;
		}

		if (sc->cur_x != NULL) {
			free(sc->cur_x, M_USB);
			sc->cur_x = NULL;
		}

		if (sc->pressure_x != NULL) {
			free(sc->pressure_x, M_USB);
			sc->pressure_x = NULL;
		}
	}
	if (params->n_ysensors != 0) {
		if (sc->base_y != NULL) {
			free(sc->base_y, M_USB);
			sc->base_y = NULL;
		}

		if (sc->cur_y != NULL) {
			free(sc->cur_y, M_USB);
			sc->cur_y = NULL;
		}

		if (sc->pressure_y != NULL) {
			free(sc->pressure_y, M_USB);
			sc->pressure_y = NULL;
		}
	}
	if (sc->sensor_data != NULL) {
		free(sc->sensor_data, M_USB);
		sc->sensor_data = NULL;
	}
}

/*
 * Interpret the data from the X and Y pressure sensors. This function
 * is called separately for the X and Y sensor arrays. The data in the
 * USB packet is laid out in the following manner:
 *
 * sensor_data:
 *            --,--,Y1,Y2,--,Y3,Y4,--,Y5,...,Y10, ... X1,X2,--,X3,X4
 *  indices:   0  1  2  3  4  5  6  7  8 ...  15  ... 20 21 22 23 24
 *
 * '--' (in the above) indicates that the value is unimportant.
 *
 * Information about the above layout was obtained from the
 * implementation of the AppleTouch driver in Linux.
 *
 * parameters:
 *   sensor_data
 *       raw sensor data from the USB packet.
 *   num
 *       The number of elements in the array 'arr'.
 *   axis
 *       Axis of data to fetch
 *   arr
 *       The array to be initialized with the readings.
 *   prot
 *       The protocol to use to interpret the data
 */
static __inline void
atp_interpret_sensor_data(const int8_t *sensor_data, u_int num, atp_axis axis,
    int	*arr, atp_protocol prot)
{
	u_int i;
	u_int di;   /* index into sensor data */

	switch (prot) {
	case ATP_PROT_GEYSER1:
		/*
		 * For Geyser 1, the sensors are laid out in pairs
		 * every 5 bytes.
		 */
		for (i = 0, di = (axis == Y) ? 1 : 2; i < 8; di += 5, i++) {
			arr[i] = sensor_data[di];
			arr[i+8] = sensor_data[di+2];
			if (axis == X && num > 16) 
				arr[i+16] = sensor_data[di+40];
		}

		break;
	case ATP_PROT_GEYSER2:
	case ATP_PROT_GEYSER3:
		for (i = 0, di = (axis == Y) ? 2 : 20; i < num; /* empty */ ) {
			arr[i++] = sensor_data[di++];
			arr[i++] = sensor_data[di++];
			di++;
		}
		break;
	}
}

static __inline void
atp_get_pressures(int *p, const int *cur, const int *base, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		p[i] = cur[i] - base[i];
		if (p[i] > 127)
			p[i] -= 256;
		if (p[i] < -127)
			p[i] += 256;
		if (p[i] < 0)
			p[i] = 0;

		/*
		 * Shave off pressures below the noise-pressure
		 * threshold; this will reduce the contribution from
		 * lower pressure readings.
		 */
		if (p[i] <= atp_sensor_noise_threshold)
			p[i] = 0; /* filter away noise */
		else
			p[i] -= atp_sensor_noise_threshold;
	}
}

static void
atp_detect_pspans(int *p, u_int num_sensors,
    u_int       max_spans, /* max # of pspans permitted */
    atp_pspan  *spans,     /* finger spans */
    u_int      *nspans_p)  /* num spans detected */
{
	u_int i;
	int   maxp;             /* max pressure seen within a span */
	u_int num_spans = 0;

	enum atp_pspan_state {
		ATP_PSPAN_INACTIVE,
		ATP_PSPAN_INCREASING,
		ATP_PSPAN_DECREASING,
	} state; /* state of the pressure span */

	/*
	 * The following is a simple state machine to track
	 * the phase of the pressure span.
	 */
	memset(spans, 0, max_spans * sizeof(atp_pspan));
	maxp = 0;
	state = ATP_PSPAN_INACTIVE;
	for (i = 0; i < num_sensors; i++) {
		if (num_spans >= max_spans)
			break;

		if (p[i] == 0) {
			if (state == ATP_PSPAN_INACTIVE) {
				/*
				 * There is no pressure information for this
				 * sensor, and we aren't tracking a finger.
				 */
				continue;
			} else {
				state = ATP_PSPAN_INACTIVE;
				maxp = 0;
				num_spans++;
			}
		} else {
			switch (state) {
			case ATP_PSPAN_INACTIVE:
				state = ATP_PSPAN_INCREASING;
				maxp  = p[i];
				break;

			case ATP_PSPAN_INCREASING:
				if (p[i] > maxp)
					maxp = p[i];
				else if (p[i] <= (maxp >> 1))
					state = ATP_PSPAN_DECREASING;
				break;

			case ATP_PSPAN_DECREASING:
				if (p[i] > p[i - 1]) {
					/*
					 * This is the beginning of
					 * another span; change state
					 * to give the appearance that
					 * we're starting from an
					 * inactive span, and then
					 * re-process this reading in
					 * the next iteration.
					 */
					num_spans++;
					state = ATP_PSPAN_INACTIVE;
					maxp  = 0;
					i--;
					continue;
				}
				break;
			}

			/* Update the finger span with this reading. */
			spans[num_spans].width++;
			spans[num_spans].cum += p[i];
			spans[num_spans].cog += p[i] * (i + 1);
		}
	}
	if (state != ATP_PSPAN_INACTIVE)
		num_spans++;    /* close the last finger span */

	/* post-process the spans */
	for (i = 0; i < num_spans; i++) {
		/* filter away unwanted pressure spans */
		if ((spans[i].cum < atp_pspan_min_cum_pressure) ||
		    (spans[i].width > atp_pspan_max_width)) {
			if ((i + 1) < num_spans) {
				memcpy(&spans[i], &spans[i + 1],
				    (num_spans - i - 1) * sizeof(atp_pspan));
				i--;
			}
			num_spans--;
			continue;
		}

		/* compute this span's representative location */
		spans[i].loc = spans[i].cog * atp_mickeys_scale_factor /
			spans[i].cum;

		spans[i].matched = FALSE; /* not yet matched against a stroke */
	}

	*nspans_p = num_spans;
}

/*
 * Match a pressure-span against a stroke-component. If there is a
 * match, update the component's state and return TRUE.
 */
static boolean_t
atp_match_stroke_component(atp_stroke_component *component,
    const atp_pspan *pspan)
{
	int delta_mickeys = pspan->loc - component->loc;

	if (abs(delta_mickeys) > atp_max_delta_mickeys)
		return (FALSE); /* the finger span is too far out; no match */

	component->loc          = pspan->loc;
	component->cum_pressure = pspan->cum;
	if (pspan->cum > component->max_cum_pressure)
		component->max_cum_pressure = pspan->cum;

	/*
	 * If the cumulative pressure drops below a quarter of the max,
	 * then disregard the component's movement.
	 */
	if (component->cum_pressure < (component->max_cum_pressure >> 2))
		delta_mickeys = 0;

	component->delta_mickeys = delta_mickeys;
	return (TRUE);
}

static void
atp_match_strokes_against_pspans(struct atp_softc *sc, atp_axis axis,
    atp_pspan *pspans, u_int n_pspans, u_int repeat_count)
{
	u_int i, j;
	u_int repeat_index = 0;

	/* Determine the index of the multi-span. */
	if (repeat_count) {
		u_int cum = 0;
		for (i = 0; i < n_pspans; i++) {
			if (pspans[i].cum > cum) {
				repeat_index = i;
				cum = pspans[i].cum;
			}
		}
	}

	for (i = 0; i < sc->sc_n_strokes; i++) {
		atp_stroke *stroke  = &sc->sc_strokes[i];
		if (stroke->components[axis].matched)
			continue; /* skip matched components */

		for (j = 0; j < n_pspans; j++) {
			if (pspans[j].matched)
				continue; /* skip matched pspans */

			if (atp_match_stroke_component(
				    &stroke->components[axis], &pspans[j])) {
				/* There is a match. */
				stroke->components[axis].matched = TRUE;

				/* Take care to repeat at the multi-span. */
				if ((repeat_count > 0) && (j == repeat_index))
					repeat_count--;
				else
					pspans[j].matched = TRUE;

				break; /* skip to the next stroke */
			}
		} /* loop over pspans */
	} /* loop over strokes */
}

/*
 * Update strokes by matching against current pressure-spans.
 * Return TRUE if any movement is detected.
 */
static boolean_t
atp_update_strokes(struct atp_softc *sc, atp_pspan *pspans_x,
    u_int n_xpspans, atp_pspan *pspans_y, u_int n_ypspans)
{
	u_int       i, j;
	atp_stroke *stroke;
	boolean_t   movement = FALSE;
	u_int       repeat_count = 0;

	/* Reset X and Y components of all strokes as unmatched. */
	for (i = 0; i < sc->sc_n_strokes; i++) {
		stroke = &sc->sc_strokes[i];
		stroke->components[X].matched = FALSE;
		stroke->components[Y].matched = FALSE;
	}

	/*
	 * Usually, the X and Y pspans come in pairs (the common case
	 * being a single pair). It is possible, however, that
	 * multiple contacts resolve to a single pspan along an
	 * axis, as illustrated in the following:
	 *
	 *   F = finger-contact
	 *
	 *                pspan  pspan
	 *        +-----------------------+
	 *        |         .      .      |
	 *        |         .      .      |
	 *        |         .      .      |
	 *        |         .      .      |
	 *  pspan |.........F......F      |
	 *        |                       |
	 *        |                       |
	 *        |                       |
	 *        +-----------------------+
	 *
	 *
	 * The above case can be detected by a difference in the
	 * number of X and Y pspans. When this happens, X and Y pspans
	 * aren't easy to pair or match against strokes.
	 *
	 * When X and Y pspans differ in number, the axis with the
	 * smaller number of pspans is regarded as having a repeating
	 * pspan (or a multi-pspan)--in the above illustration, the
	 * Y-axis has a repeating pspan. Our approach is to try to
	 * match the multi-pspan repeatedly against strokes. The
	 * difference between the number of X and Y pspans gives us a
	 * crude repeat_count for matching multi-pspans--i.e. the
	 * multi-pspan along the Y axis (above) has a repeat_count of 1.
	 */
	repeat_count = abs(n_xpspans - n_ypspans);

	atp_match_strokes_against_pspans(sc, X, pspans_x, n_xpspans,
	    (((repeat_count != 0) && ((n_xpspans < n_ypspans))) ?
		repeat_count : 0));
	atp_match_strokes_against_pspans(sc, Y, pspans_y, n_ypspans,
	    (((repeat_count != 0) && (n_ypspans < n_xpspans)) ?
		repeat_count : 0));

	/* Update the state of strokes based on the above pspan matches. */
	for (i = 0; i < sc->sc_n_strokes; i++) {
		stroke = &sc->sc_strokes[i];
		if (stroke->components[X].matched &&
		    stroke->components[Y].matched) {
			atp_advance_stroke_state(sc, stroke, &movement);
		} else {
			/*
			 * At least one component of this stroke
			 * didn't match against current pspans;
			 * terminate it.
			 */
			atp_terminate_stroke(sc, i);
		}
	}

	/* Add new strokes for pairs of unmatched pspans */
	for (i = 0; i < n_xpspans; i++) {
		if (pspans_x[i].matched == FALSE) break;
	}
	for (j = 0; j < n_ypspans; j++) {
		if (pspans_y[j].matched == FALSE) break;
	}
	if ((i < n_xpspans) && (j < n_ypspans)) {
#if USB_DEBUG
		if (atp_debug >= ATP_LLEVEL_INFO) {
			printf("unmatched pspans:");
			for (; i < n_xpspans; i++) {
				if (pspans_x[i].matched)
					continue;
				printf(" X:[loc:%u,cum:%u]",
				    pspans_x[i].loc, pspans_x[i].cum);
			}
			for (; j < n_ypspans; j++) {
				if (pspans_y[j].matched)
					continue;
				printf(" Y:[loc:%u,cum:%u]",
				    pspans_y[j].loc, pspans_y[j].cum);
			}
			printf("\n");
		}
#endif /* #if USB_DEBUG */
		if ((n_xpspans == 1) && (n_ypspans == 1))
			/* The common case of a single pair of new pspans. */
			atp_add_stroke(sc, &pspans_x[0], &pspans_y[0]);
		else
			atp_add_new_strokes(sc,
			    pspans_x, n_xpspans,
			    pspans_y, n_ypspans);
	}

#if USB_DEBUG
	if (atp_debug >= ATP_LLEVEL_INFO) {
		for (i = 0; i < sc->sc_n_strokes; i++) {
			atp_stroke *stroke = &sc->sc_strokes[i];

			printf(" %s%clc:%u,dm:%d,pnd:%d,mv:%d%c"
			    ",%clc:%u,dm:%d,pnd:%d,mv:%d%c",
			    (stroke->flags & ATSF_ZOMBIE) ? "zomb:" : "",
			    (stroke->type == ATP_STROKE_TOUCH) ? '[' : '<',
			    stroke->components[X].loc,
			    stroke->components[X].delta_mickeys,
			    stroke->components[X].pending,
			    stroke->components[X].movement,
			    (stroke->type == ATP_STROKE_TOUCH) ? ']' : '>',
			    (stroke->type == ATP_STROKE_TOUCH) ? '[' : '<',
			    stroke->components[Y].loc,
			    stroke->components[Y].delta_mickeys,
			    stroke->components[Y].pending,
			    stroke->components[Y].movement,
			    (stroke->type == ATP_STROKE_TOUCH) ? ']' : '>');
		}
		if (sc->sc_n_strokes)
			printf("\n");
	}
#endif /* #if USB_DEBUG */

	return (movement);
}

/* Initialize a stroke using a pressure-span. */
static __inline void
atp_add_stroke(struct atp_softc *sc, const atp_pspan *pspan_x,
    const atp_pspan *pspan_y)
{
	atp_stroke *stroke;

	if (sc->sc_n_strokes >= ATP_MAX_STROKES)
		return;
	stroke = &sc->sc_strokes[sc->sc_n_strokes];

	memset(stroke, 0, sizeof(atp_stroke));

	/*
	 * Strokes begin as potential touches. If a stroke survives
	 * longer than a threshold, or if it records significant
	 * cumulative movement, then it is considered a 'slide'.
	 */
	stroke->type = ATP_STROKE_TOUCH;
	microtime(&stroke->ctime);
	stroke->age  = 1;       /* Unit: interrupts */

	stroke->components[X].loc              = pspan_x->loc;
	stroke->components[X].cum_pressure     = pspan_x->cum;
	stroke->components[X].max_cum_pressure = pspan_x->cum;
	stroke->components[X].matched          = TRUE;

	stroke->components[Y].loc              = pspan_y->loc;
	stroke->components[Y].cum_pressure     = pspan_y->cum;
	stroke->components[Y].max_cum_pressure = pspan_y->cum;
	stroke->components[Y].matched          = TRUE;

	sc->sc_n_strokes++;
	if (sc->sc_n_strokes > 1) {
		/* Reset double-tap-n-drag if we have more than one strokes. */
		sc->sc_state &= ~ATP_DOUBLE_TAP_DRAG;
	}

	DPRINTFN(ATP_LLEVEL_INFO, "[%u,%u], time: %u,%ld\n",
	    stroke->components[X].loc,
	    stroke->components[Y].loc,
	    (unsigned int)stroke->ctime.tv_sec,
	    (unsigned long int)stroke->ctime.tv_usec);
}

static void
atp_add_new_strokes(struct atp_softc *sc, atp_pspan *pspans_x,
    u_int n_xpspans, atp_pspan *pspans_y, u_int n_ypspans)
{
	int       i, j;
	atp_pspan spans[2][ATP_MAX_PSPANS_PER_AXIS];
	u_int     nspans[2];

	/* Copy unmatched pspans into the local arrays. */
	for (i = 0, nspans[X] = 0; i < n_xpspans; i++) {
		if (pspans_x[i].matched == FALSE) {
			spans[X][nspans[X]] = pspans_x[i];
			nspans[X]++;
		}
	}
	for (j = 0, nspans[Y] = 0; j < n_ypspans; j++) {
		if (pspans_y[j].matched == FALSE) {
			spans[Y][nspans[Y]] = pspans_y[j];
			nspans[Y]++;
		}
	}

	if (nspans[X] == nspans[Y]) {
		/* Create new strokes from pairs of unmatched pspans */
		for (i = 0, j = 0; (i < nspans[X]) && (j < nspans[Y]); i++, j++)
			atp_add_stroke(sc, &spans[X][i], &spans[Y][j]);
	} else {
		u_int    cum = 0;
		atp_axis repeat_axis;      /* axis with multi-pspans */
		u_int    repeat_count;     /* repeat count for the multi-pspan*/
		u_int    repeat_index = 0; /* index of the multi-span */

		repeat_axis  = (nspans[X] > nspans[Y]) ? Y : X;
		repeat_count = abs(nspans[X] - nspans[Y]);
		for (i = 0; i < nspans[repeat_axis]; i++) {
			if (spans[repeat_axis][i].cum > cum) {
				repeat_index = i;
				cum = spans[repeat_axis][i].cum;
			}
		}

		/* Create new strokes from pairs of unmatched pspans */
		i = 0, j = 0;
		for (; (i < nspans[X]) && (j < nspans[Y]); i++, j++) {
			atp_add_stroke(sc, &spans[X][i], &spans[Y][j]);

			/* Take care to repeat at the multi-pspan. */
			if (repeat_count > 0) {
				if ((repeat_axis == X) &&
				    (repeat_index == i)) {
					i--; /* counter loop increment */
					repeat_count--;
				} else if ((repeat_axis == Y) &&
				    (repeat_index == j)) {
					j--; /* counter loop increment */
					repeat_count--;
				}
			}
		}
	}
}

/*
 * Advance the state of this stroke--and update the out-parameter
 * 'movement' as a side-effect.
 */
void
atp_advance_stroke_state(struct atp_softc *sc, atp_stroke *stroke,
    boolean_t *movement)
{
	stroke->age++;
	if (stroke->age <= atp_stroke_maturity_threshold) {
		/* Avoid noise from immature strokes. */
		stroke->components[X].delta_mickeys = 0;
		stroke->components[Y].delta_mickeys = 0;
	}

	/* Revitalize stroke if it had previously been marked as a zombie. */
	if (stroke->flags & ATSF_ZOMBIE)
		stroke->flags &= ~ATSF_ZOMBIE;

	if (atp_compute_stroke_movement(stroke))
		*movement = TRUE;

	/* Convert touch strokes to slides upon detecting movement or age. */
	if (stroke->type == ATP_STROKE_TOUCH) {
		struct timeval tdiff;

		/* Compute the stroke's age. */
		getmicrotime(&tdiff);
		if (timevalcmp(&tdiff, &stroke->ctime, >))
			timevalsub(&tdiff, &stroke->ctime);
		else {
			/*
			 * If we are here, it is because getmicrotime
			 * reported the current time as being behind
			 * the stroke's start time; getmicrotime can
			 * be imprecise.
			 */
			tdiff.tv_sec  = 0;
			tdiff.tv_usec = 0;
		}

		if ((tdiff.tv_sec > (atp_touch_timeout / 1000000)) ||
		    ((tdiff.tv_sec == (atp_touch_timeout / 1000000)) &&
			(tdiff.tv_usec > atp_touch_timeout)) ||
		    (stroke->cum_movement >= atp_slide_min_movement)) {
			/* Switch this stroke to being a slide. */
			stroke->type = ATP_STROKE_SLIDE;

			/* Are we at the beginning of a double-click-n-drag? */
			if ((sc->sc_n_strokes == 1) &&
			    ((sc->sc_state & ATP_ZOMBIES_EXIST) == 0) &&
			    timevalcmp(&stroke->ctime, &sc->sc_reap_time, >)) {
				struct timeval delta;
				struct timeval window = {
					atp_double_tap_threshold / 1000000,
					atp_double_tap_threshold % 1000000
				};

				delta = stroke->ctime;
				timevalsub(&delta, &sc->sc_reap_time);
				if (timevalcmp(&delta, &window, <=))
					sc->sc_state |= ATP_DOUBLE_TAP_DRAG;
			}
		}
	}
}

/*
 * Terminate a stroke. While SLIDE strokes are dropped, TOUCH strokes
 * are retained as zombies so as to reap all their siblings together;
 * this helps establish the number of fingers involved in the tap.
 */
static void
atp_terminate_stroke(struct atp_softc *sc,
    u_int index) /* index of the stroke to be terminated */
{
	atp_stroke *s = &sc->sc_strokes[index];

	if (s->flags & ATSF_ZOMBIE) {
		return;
	}

	if ((s->type == ATP_STROKE_TOUCH) &&
	    (s->age > atp_stroke_maturity_threshold)) {
		s->flags |= ATSF_ZOMBIE;

		/* If no zombies exist, then prepare to reap zombies later. */
		if ((sc->sc_state & ATP_ZOMBIES_EXIST) == 0) {
			atp_setup_reap_time(sc, &s->ctime);
			sc->sc_state |= ATP_ZOMBIES_EXIST;
		}
	} else {
		/* Drop this stroke. */
		memcpy(&sc->sc_strokes[index], &sc->sc_strokes[index + 1],
		    (sc->sc_n_strokes - index - 1) * sizeof(atp_stroke));
		sc->sc_n_strokes--;

		/*
		 * Reset the double-click-n-drag at the termination of
		 * any slide stroke.
		 */
		sc->sc_state &= ~ATP_DOUBLE_TAP_DRAG;
	}
}

static __inline boolean_t
atp_stroke_has_small_movement(const atp_stroke *stroke)
{
	return ((abs(stroke->components[X].delta_mickeys) <=
		atp_small_movement_threshold) &&
	    (abs(stroke->components[Y].delta_mickeys) <=
		atp_small_movement_threshold));
}

/*
 * Accumulate delta_mickeys into the component's 'pending' bucket; if
 * the aggregate exceeds the small_movement_threshold, then retain
 * delta_mickeys for later.
 */
static __inline void
atp_update_pending_mickeys(atp_stroke_component *component)
{
	component->pending += component->delta_mickeys;
	if (abs(component->pending) <= atp_small_movement_threshold)
		component->delta_mickeys = 0;
	else {
		/*
		 * Penalise pending mickeys for having accumulated
		 * over short deltas. This operation has the effect of
		 * scaling down the cumulative contribution of short
		 * movements.
		 */
		component->pending -= (component->delta_mickeys << 1);
	}
}


static void
atp_compute_smoothening_scale_ratio(atp_stroke *stroke, int *numerator,
    int *denominator)
{
	int   dxdt;
	int   dydt;
	u_int vel_squared; /* Square of the velocity vector's magnitude. */
	u_int vel_squared_smooth;

	/* Table holding (10 * sqrt(x)) for x between 1 and 256. */
	static uint8_t sqrt_table[256] = {
		10, 14, 17, 20, 22, 24, 26, 28,
		30, 31, 33, 34, 36, 37, 38, 40,
		41, 42, 43, 44, 45, 46, 47, 48,
		50, 50, 51, 52, 53, 54, 55, 56,
		57, 58, 59, 60, 60, 61, 62, 63,
		64, 64, 65, 66, 67, 67, 68, 69,
		70, 70, 71, 72, 72, 73, 74, 74,
		75, 76, 76, 77, 78, 78, 79, 80,
		80, 81, 81, 82, 83, 83, 84, 84,
		85, 86, 86, 87, 87, 88, 88, 89,
		90, 90, 91, 91, 92, 92, 93, 93,
		94, 94, 95, 95, 96, 96, 97, 97,
		98, 98, 99, 100, 100, 100, 101, 101,
		102, 102, 103, 103, 104, 104, 105, 105,
		106, 106, 107, 107, 108, 108, 109, 109,
		110, 110, 110, 111, 111, 112, 112, 113,
		113, 114, 114, 114, 115, 115, 116, 116,
		117, 117, 117, 118, 118, 119, 119, 120,
		120, 120, 121, 121, 122, 122, 122, 123,
		123, 124, 124, 124, 125, 125, 126, 126,
		126, 127, 127, 128, 128, 128, 129, 129,
		130, 130, 130, 131, 131, 131, 132, 132,
		133, 133, 133, 134, 134, 134, 135, 135,
		136, 136, 136, 137, 137, 137, 138, 138,
		138, 139, 139, 140, 140, 140, 141, 141,
		141, 142, 142, 142, 143, 143, 143, 144,
		144, 144, 145, 145, 145, 146, 146, 146,
		147, 147, 147, 148, 148, 148, 149, 149,
		150, 150, 150, 150, 151, 151, 151, 152,
		152, 152, 153, 153, 153, 154, 154, 154,
		155, 155, 155, 156, 156, 156, 157, 157,
		157, 158, 158, 158, 159, 159, 159, 160
	};
	const u_int N = sizeof(sqrt_table) / sizeof(sqrt_table[0]);

	dxdt = stroke->components[X].delta_mickeys;
	dydt = stroke->components[Y].delta_mickeys;

	*numerator = 0, *denominator = 0; /* default values. */

	/* Compute a smoothened magnitude_squared of the stroke's velocity. */
	vel_squared = dxdt * dxdt + dydt * dydt;
	vel_squared_smooth = (3 * stroke->velocity_squared + vel_squared) >> 2;
	stroke->velocity_squared = vel_squared_smooth; /* retained as history */
	if ((vel_squared == 0) || (vel_squared_smooth == 0))
		return; /* returning (numerator == 0) will imply zero movement*/

	/*
	 * In order to determine the overall movement scale factor,
	 * we're actually interested in the effect of smoothening upon
	 * the *magnitude* of velocity; i.e. we need to compute the
	 * square-root of (vel_squared_smooth / vel_squared) in the
	 * form of a numerator and denominator.
	 */

	/* Keep within the bounds of the square-root table. */
	while ((vel_squared > N) || (vel_squared_smooth > N)) {
		/* Dividing uniformly by 2 won't disturb the final ratio. */
		vel_squared        >>= 1;
		vel_squared_smooth >>= 1;
	}

	*numerator   = sqrt_table[vel_squared_smooth - 1];
	*denominator = sqrt_table[vel_squared - 1];
}

/*
 * Compute a smoothened value for the stroke's movement from
 * delta_mickeys in the X and Y components.
 */
static boolean_t
atp_compute_stroke_movement(atp_stroke *stroke)
{
	int   num;              /* numerator of scale ratio */
	int   denom;            /* denominator of scale ratio */

	/*
	 * Short movements are added first to the 'pending' bucket,
	 * and then acted upon only when their aggregate exceeds a
	 * threshold. This has the effect of filtering away movement
	 * noise.
	 */
	if (atp_stroke_has_small_movement(stroke)) {
		atp_update_pending_mickeys(&stroke->components[X]);
		atp_update_pending_mickeys(&stroke->components[Y]);
	} else {                /* large movement */
		/* clear away any pending mickeys if there are large movements*/
		stroke->components[X].pending = 0;
		stroke->components[Y].pending = 0;
	}

	/* Get the scale ratio and smoothen movement. */
	atp_compute_smoothening_scale_ratio(stroke, &num, &denom);
	if ((num == 0) || (denom == 0)) {
		stroke->components[X].movement = 0;
		stroke->components[Y].movement = 0;
		stroke->velocity_squared >>= 1; /* Erode velocity_squared. */
	} else {
		stroke->components[X].movement =
			(stroke->components[X].delta_mickeys * num) / denom;
		stroke->components[Y].movement =
			(stroke->components[Y].delta_mickeys * num) / denom;

		stroke->cum_movement +=
			abs(stroke->components[X].movement) +
			abs(stroke->components[Y].movement);
	}

	return ((stroke->components[X].movement != 0) ||
	    (stroke->components[Y].movement != 0));
}

static __inline void
atp_setup_reap_time(struct atp_softc *sc, struct timeval *tvp)
{
	struct timeval reap_window = {
		ATP_ZOMBIE_STROKE_REAP_WINDOW / 1000000,
		ATP_ZOMBIE_STROKE_REAP_WINDOW % 1000000
	};

	microtime(&sc->sc_reap_time);
	timevaladd(&sc->sc_reap_time, &reap_window);

	sc->sc_reap_ctime = *tvp; /* ctime to reap */
}

static void
atp_reap_zombies(struct atp_softc *sc, u_int *n_reaped, u_int *reaped_xlocs)
{
	u_int       i;
	atp_stroke *stroke;

	*n_reaped = 0;
	for (i = 0; i < sc->sc_n_strokes; i++) {
		struct timeval  tdiff;

		stroke = &sc->sc_strokes[i];

		if ((stroke->flags & ATSF_ZOMBIE) == 0)
			continue;

		/* Compare this stroke's ctime with the ctime being reaped. */
		if (timevalcmp(&stroke->ctime, &sc->sc_reap_ctime, >=)) {
			tdiff = stroke->ctime;
			timevalsub(&tdiff, &sc->sc_reap_ctime);
		} else {
			tdiff = sc->sc_reap_ctime;
			timevalsub(&tdiff, &stroke->ctime);
		}

		if ((tdiff.tv_sec > (ATP_COINCIDENCE_THRESHOLD / 1000000)) ||
		    ((tdiff.tv_sec == (ATP_COINCIDENCE_THRESHOLD / 1000000)) &&
		     (tdiff.tv_usec > (ATP_COINCIDENCE_THRESHOLD % 1000000)))) {
			continue; /* Skip non-siblings. */
		}

		/*
		 * Reap this sibling zombie stroke.
		 */

		if (reaped_xlocs != NULL)
			reaped_xlocs[*n_reaped] = stroke->components[X].loc;

		/* Erase the stroke from the sc. */
		memcpy(&stroke[i], &stroke[i + 1],
		    (sc->sc_n_strokes - i - 1) * sizeof(atp_stroke));
		sc->sc_n_strokes--;

		*n_reaped += 1;
		--i; /* Decr. i to keep it unchanged for the next iteration */
	}

	DPRINTFN(ATP_LLEVEL_INFO, "reaped %u zombies\n", *n_reaped);

	/* There could still be zombies remaining in the system. */
	for (i = 0; i < sc->sc_n_strokes; i++) {
		stroke = &sc->sc_strokes[i];
		if (stroke->flags & ATSF_ZOMBIE) {
			DPRINTFN(ATP_LLEVEL_INFO, "zombies remain!\n");
			atp_setup_reap_time(sc, &stroke->ctime);
			return;
		}
	}

	/* If we reach here, then no more zombies remain. */
	sc->sc_state &= ~ATP_ZOMBIES_EXIST;
}


/* Device methods. */
static device_probe_t  atp_probe;
static device_attach_t atp_attach;
static device_detach_t atp_detach;
static usb_callback_t  atp_intr;

static const struct usb_config atp_config[ATP_N_TRANSFER] = {
	[ATP_INTR_DT] = {
		.type      = UE_INTERRUPT,
		.endpoint  = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = {
			.pipe_bof = 1,
			.short_xfer_ok = 1,
		},
		.bufsize   = 0, /* use wMaxPacketSize */
		.callback  = &atp_intr,
	},
};

static int
atp_probe(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if ((uaa->info.bInterfaceClass != UICLASS_HID) ||
	    (uaa->info.bInterfaceProtocol != UIPROTO_MOUSE))
		return (ENXIO);

	if (usbd_lookup_id_by_uaa(atp_devs, sizeof(atp_devs), uaa) == 0)
		return BUS_PROBE_SPECIFIC;
	else
		return ENXIO;
}

static int
atp_attach(device_t dev)
{
	struct atp_softc      *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	usb_error_t            err;

	/* ensure that the probe was successful */
	if (uaa->driver_info >= ATP_N_DEV_PARAMS) {
		DPRINTF("device probe returned bad id: %lu\n",
		    uaa->driver_info);
		return (ENXIO);
	}
	DPRINTFN(ATP_LLEVEL_INFO, "sc=%p\n", sc);

	sc->sc_dev        = dev;
	sc->sc_usb_device = uaa->device;

	/*
	 * By default the touchpad behaves like an HID device, sending
	 * packets with reportID = 2. Such reports contain only
	 * limited information--they encode movement deltas and button
	 * events,--but do not include data from the pressure
	 * sensors. The device input mode can be switched from HID
	 * reports to raw sensor data using vendor-specific USB
	 * control commands; but first the mode must be read.
	 */
	err = atp_req_get_report(sc->sc_usb_device, sc->sc_mode_bytes);
	if (err != USB_ERR_NORMAL_COMPLETION) {
		DPRINTF("failed to read device mode (%d)\n", err);
		return (ENXIO);
	}

	if (atp_set_device_mode(dev, RAW_SENSOR_MODE) != 0) {
		DPRINTF("failed to set mode to 'RAW_SENSOR' (%d)\n", err);
		return (ENXIO);
	}

	mtx_init(&sc->sc_mutex, "atpmtx", NULL, MTX_DEF | MTX_RECURSE);

	err = usbd_transfer_setup(uaa->device,
	    &uaa->info.bIfaceIndex, sc->sc_xfer, atp_config,
	    ATP_N_TRANSFER, sc, &sc->sc_mutex);

	if (err) {
		DPRINTF("error=%s\n", usbd_errstr(err));
		goto detach;
	}

	if (usb_fifo_attach(sc->sc_usb_device, sc, &sc->sc_mutex,
		&atp_fifo_methods, &sc->sc_fifo,
		device_get_unit(dev), 0 - 1, uaa->info.bIfaceIndex,
		UID_ROOT, GID_OPERATOR, 0644)) {
		goto detach;
	}

	device_set_usb_desc(dev);

	sc->sc_params           = &atp_dev_params[uaa->driver_info];

	sc->sc_hw.buttons       = 3;
	sc->sc_hw.iftype        = MOUSE_IF_USB;
	sc->sc_hw.type          = MOUSE_PAD;
	sc->sc_hw.model         = MOUSE_MODEL_GENERIC;
	sc->sc_hw.hwid          = 0;
	sc->sc_mode.protocol    = MOUSE_PROTO_MSC;
	sc->sc_mode.rate        = -1;
	sc->sc_mode.resolution  = MOUSE_RES_UNKNOWN;
	sc->sc_mode.accelfactor = 0;
	sc->sc_mode.level       = 0;
	sc->sc_mode.packetsize  = MOUSE_MSC_PACKETSIZE;
	sc->sc_mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
	sc->sc_mode.syncmask[1] = MOUSE_MSC_SYNC;

	sc->sc_state            = 0;

	sc->sc_left_margin  = atp_mickeys_scale_factor;
	sc->sc_right_margin = (sc->sc_params->n_xsensors - 1) *
		atp_mickeys_scale_factor;

	return (0);

detach:
	atp_detach(dev);
	return (ENOMEM);
}

static int
atp_detach(device_t dev)
{
	struct atp_softc *sc;
	int err;

	sc = device_get_softc(dev);
	if (sc->sc_state & ATP_ENABLED) {
		mtx_lock(&sc->sc_mutex);
		atp_disable(sc);
		mtx_unlock(&sc->sc_mutex);
	}

	usb_fifo_detach(&sc->sc_fifo);

	usbd_transfer_unsetup(sc->sc_xfer, ATP_N_TRANSFER);

	mtx_destroy(&sc->sc_mutex);

	err = atp_set_device_mode(dev, HID_MODE);
	if (err != 0) {
		DPRINTF("failed to reset mode to 'HID' (%d)\n", err);
		return (err);
	}

	return (0);
}

static void
atp_intr(struct usb_xfer *xfer, usb_error_t error)
{
	struct atp_softc      *sc = usbd_xfer_softc(xfer);
	int                    len;
	struct usb_page_cache *pc;
	uint8_t                status_bits;
	atp_pspan  pspans_x[ATP_MAX_PSPANS_PER_AXIS];
	atp_pspan  pspans_y[ATP_MAX_PSPANS_PER_AXIS];
	u_int      n_xpspans = 0, n_ypspans = 0;
	u_int      reaped_xlocs[ATP_MAX_STROKES];
	u_int      tap_fingers = 0;

	usbd_xfer_status(xfer, &len, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (len > sc->sc_params->data_len) {
			DPRINTFN(ATP_LLEVEL_ERROR,
			    "truncating large packet from %u to %u bytes\n",
			    len, sc->sc_params->data_len);
			len = sc->sc_params->data_len;
		}
		if (len < sc->sc_params->data_len)
			goto tr_setup;

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, sc->sensor_data, sc->sc_params->data_len);

		/* Interpret sensor data */
		atp_interpret_sensor_data(sc->sensor_data,
		    sc->sc_params->n_xsensors, X, sc->cur_x,
		    sc->sc_params->prot);
		atp_interpret_sensor_data(sc->sensor_data,
		    sc->sc_params->n_ysensors, Y,  sc->cur_y,
		    sc->sc_params->prot);

		/*
		 * If this is the initial update (from an untouched
		 * pad), we should set the base values for the sensor
		 * data; deltas with respect to these base values can
		 * be used as pressure readings subsequently.
		 */
		status_bits = sc->sensor_data[sc->sc_params->data_len - 1];
		if ((sc->sc_params->prot == ATP_PROT_GEYSER3 &&
		    (status_bits & ATP_STATUS_BASE_UPDATE)) || 
		    !(sc->sc_state & ATP_VALID)) {
			memcpy(sc->base_x, sc->cur_x,
			    sc->sc_params->n_xsensors * sizeof(*(sc->base_x)));
			memcpy(sc->base_y, sc->cur_y,
			    sc->sc_params->n_ysensors * sizeof(*(sc->base_y)));
			sc->sc_state |= ATP_VALID;
			goto tr_setup;
		}

		/* Get pressure readings and detect p-spans for both axes. */
		atp_get_pressures(sc->pressure_x, sc->cur_x, sc->base_x,
		    sc->sc_params->n_xsensors);
		atp_detect_pspans(sc->pressure_x, sc->sc_params->n_xsensors,
		    ATP_MAX_PSPANS_PER_AXIS,
		    pspans_x, &n_xpspans);
		atp_get_pressures(sc->pressure_y, sc->cur_y, sc->base_y,
		    sc->sc_params->n_ysensors);
		atp_detect_pspans(sc->pressure_y, sc->sc_params->n_ysensors,
		    ATP_MAX_PSPANS_PER_AXIS,
		    pspans_y, &n_ypspans);

		/* Update strokes with new pspans to detect movements. */
		sc->sc_status.flags &= ~MOUSE_POSCHANGED;
		if (atp_update_strokes(sc,
			pspans_x, n_xpspans,
			pspans_y, n_ypspans))
			sc->sc_status.flags |= MOUSE_POSCHANGED;

		/* Reap zombies if it is time. */
		if (sc->sc_state & ATP_ZOMBIES_EXIST) {
			struct timeval now;

			getmicrotime(&now);
			if (timevalcmp(&now, &sc->sc_reap_time, >=))
				atp_reap_zombies(sc, &tap_fingers,
				    reaped_xlocs);
		}

		sc->sc_status.flags &= ~MOUSE_STDBUTTONSCHANGED;
		sc->sc_status.obutton = sc->sc_status.button;

		/* Get the state of the physical buttton. */
		sc->sc_status.button = (status_bits & ATP_STATUS_BUTTON) ?
			MOUSE_BUTTON1DOWN : 0;
		if (sc->sc_status.button != 0) {
			/* Reset DOUBLE_TAP_N_DRAG if the button is pressed. */
			sc->sc_state &= ~ATP_DOUBLE_TAP_DRAG;
		} else if (sc->sc_state & ATP_DOUBLE_TAP_DRAG) {
			/* Assume a button-press with DOUBLE_TAP_N_DRAG. */
			sc->sc_status.button = MOUSE_BUTTON1DOWN;
		}

		sc->sc_status.flags |=
			sc->sc_status.button ^ sc->sc_status.obutton;
		if (sc->sc_status.flags & MOUSE_STDBUTTONSCHANGED) {
			DPRINTFN(ATP_LLEVEL_INFO, "button %s\n",
			    ((sc->sc_status.button & MOUSE_BUTTON1DOWN) ?
				"pressed" : "released"));
		} else if ((sc->sc_status.obutton == 0) &&
		    (sc->sc_status.button == 0) &&
		    (tap_fingers != 0)) {
			/* Ignore single-finger taps at the edges. */
			if ((tap_fingers == 1) &&
			    ((reaped_xlocs[0] <= sc->sc_left_margin) ||
				(reaped_xlocs[0] > sc->sc_right_margin))) {
				tap_fingers = 0;
			}
			DPRINTFN(ATP_LLEVEL_INFO,
			    "tap_fingers: %u\n", tap_fingers);
		}

		if (sc->sc_status.flags &
		    (MOUSE_POSCHANGED | MOUSE_STDBUTTONSCHANGED)) {
			int   dx, dy;
			u_int n_movements;

			dx = 0, dy = 0, n_movements = 0;
			for (u_int i = 0; i < sc->sc_n_strokes; i++) {
				atp_stroke *stroke = &sc->sc_strokes[i];

				if ((stroke->components[X].movement) ||
				    (stroke->components[Y].movement)) {
					dx += stroke->components[X].movement;
					dy += stroke->components[Y].movement;
					n_movements++;
				}
			}
			/*
			 * Disregard movement if multiple
			 * strokes record motion.
			 */
			if (n_movements != 1)
				dx = 0, dy = 0;

			sc->sc_status.dx += dx;
			sc->sc_status.dy += dy;
			atp_add_to_queue(sc, dx, -dy, sc->sc_status.button);
		}

		if (tap_fingers != 0) {
			/* Add a pair of events (button-down and button-up). */
			switch (tap_fingers) {
			case 1: atp_add_to_queue(sc, 0, 0, MOUSE_BUTTON1DOWN);
				break;
			case 2: atp_add_to_queue(sc, 0, 0, MOUSE_BUTTON2DOWN);
				break;
			case 3: atp_add_to_queue(sc, 0, 0, MOUSE_BUTTON3DOWN);
				break;
			default: break;/* handle taps of only up to 3 fingers */
			}
			atp_add_to_queue(sc, 0, 0, 0); /* button release */
		}

		/*
		 * The device continues to trigger interrupts at a
		 * fast rate even after touchpad activity has
		 * stopped. Upon detecting that the device has
		 * remained idle beyond a threshold, we reinitialize
		 * it to silence the interrupts.
		 */
		if ((sc->sc_status.flags  == 0) &&
		    (sc->sc_n_strokes     == 0) &&
		    (sc->sc_status.button == 0)) {
			sc->sc_idlecount++;
			if (sc->sc_idlecount >= ATP_IDLENESS_THRESHOLD) {
				DPRINTFN(ATP_LLEVEL_INFO, "idle\n");
				sc->sc_idlecount = 0;

				mtx_unlock(&sc->sc_mutex);
				atp_set_device_mode(sc->sc_dev,RAW_SENSOR_MODE);
				mtx_lock(&sc->sc_mutex);
			}
		} else {
			sc->sc_idlecount = 0;
		}

	case USB_ST_SETUP:
	tr_setup:
		/* check if we can put more data into the FIFO */
		if (usb_fifo_put_bytes_max(
			    sc->sc_fifo.fp[USB_FIFO_RX]) != 0) {
			usbd_xfer_set_frame_len(xfer, 0,
			    sc->sc_params->data_len);
			usbd_transfer_submit(xfer);
		}
		break;

	default:                        /* Error */
		if (error != USB_ERR_CANCELLED) {
			/* try clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}

	return;
}

static void
atp_add_to_queue(struct atp_softc *sc, int dx, int dy, uint32_t buttons_in)
{
	uint32_t buttons_out;
	uint8_t  buf[8];

	dx = imin(dx,  254); dx = imax(dx, -256);
	dy = imin(dy,  254); dy = imax(dy, -256);

	buttons_out = MOUSE_MSC_BUTTONS;
	if (buttons_in & MOUSE_BUTTON1DOWN)
		buttons_out &= ~MOUSE_MSC_BUTTON1UP;
	else if (buttons_in & MOUSE_BUTTON2DOWN)
		buttons_out &= ~MOUSE_MSC_BUTTON2UP;
	else if (buttons_in & MOUSE_BUTTON3DOWN)
		buttons_out &= ~MOUSE_MSC_BUTTON3UP;

	DPRINTFN(ATP_LLEVEL_INFO, "dx=%d, dy=%d, buttons=%x\n",
	    dx, dy, buttons_out);

	/* Encode the mouse data in standard format; refer to mouse(4) */
	buf[0] = sc->sc_mode.syncmask[1];
	buf[0] |= buttons_out;
	buf[1] = dx >> 1;
	buf[2] = dy >> 1;
	buf[3] = dx - (dx >> 1);
	buf[4] = dy - (dy >> 1);
	/* Encode extra bytes for level 1 */
	if (sc->sc_mode.level == 1) {
		buf[5] = 0;                    /* dz */
		buf[6] = 0;                    /* dz - (dz / 2) */
		buf[7] = MOUSE_SYS_EXTBUTTONS; /* Extra buttons all up. */
	}

	usb_fifo_put_data_linear(sc->sc_fifo.fp[USB_FIFO_RX], buf,
	    sc->sc_mode.packetsize, 1);
}

static void
atp_reset_buf(struct atp_softc *sc)
{
	/* reset read queue */
	usb_fifo_reset(sc->sc_fifo.fp[USB_FIFO_RX]);
}

static void
atp_start_read(struct usb_fifo *fifo)
{
	struct atp_softc *sc = usb_fifo_softc(fifo);
	int rate;

	/* Check if we should override the default polling interval */
	rate = sc->sc_pollrate;
	/* Range check rate */
	if (rate > 1000)
		rate = 1000;
	/* Check for set rate */
	if ((rate > 0) && (sc->sc_xfer[ATP_INTR_DT] != NULL)) {
		/* Stop current transfer, if any */
		usbd_transfer_stop(sc->sc_xfer[ATP_INTR_DT]);
		/* Set new interval */
		usbd_xfer_set_interval(sc->sc_xfer[ATP_INTR_DT], 1000 / rate);
		/* Only set pollrate once */
		sc->sc_pollrate = 0;
	}

	usbd_transfer_start(sc->sc_xfer[ATP_INTR_DT]);
}

static void
atp_stop_read(struct usb_fifo *fifo)
{
	struct atp_softc *sc = usb_fifo_softc(fifo);

	usbd_transfer_stop(sc->sc_xfer[ATP_INTR_DT]);
}


static int
atp_open(struct usb_fifo *fifo, int fflags)
{
	DPRINTFN(ATP_LLEVEL_INFO, "\n");

	if (fflags & FREAD) {
		struct atp_softc *sc = usb_fifo_softc(fifo);
		int rc;

		if (sc->sc_state & ATP_ENABLED)
			return (EBUSY);

		if (usb_fifo_alloc_buffer(fifo,
			ATP_FIFO_BUF_SIZE, ATP_FIFO_QUEUE_MAXLEN)) {
			return (ENOMEM);
		}

		rc = atp_enable(sc);
		if (rc != 0) {
			usb_fifo_free_buffer(fifo);
			return (rc);
		}
	}

	return (0);
}

static void
atp_close(struct usb_fifo *fifo, int fflags)
{
	if (fflags & FREAD) {
		struct atp_softc *sc = usb_fifo_softc(fifo);

		atp_disable(sc);
		usb_fifo_free_buffer(fifo);
	}
}

int
atp_ioctl(struct usb_fifo *fifo, u_long cmd, void *addr, int fflags)
{
	struct atp_softc *sc = usb_fifo_softc(fifo);
	mousemode_t mode;
	int error = 0;

	mtx_lock(&sc->sc_mutex);

	switch(cmd) {
	case MOUSE_GETHWINFO:
		*(mousehw_t *)addr = sc->sc_hw;
		break;
	case MOUSE_GETMODE:
		*(mousemode_t *)addr = sc->sc_mode;
		break;
	case MOUSE_SETMODE:
		mode = *(mousemode_t *)addr;

		if (mode.level == -1)
			/* Don't change the current setting */
			;
		else if ((mode.level < 0) || (mode.level > 1)) {
			error = EINVAL;
			goto done;
		}
		sc->sc_mode.level = mode.level;
		sc->sc_pollrate   = mode.rate;
		sc->sc_hw.buttons = 3;

		if (sc->sc_mode.level == 0) {
			sc->sc_mode.protocol = MOUSE_PROTO_MSC;
			sc->sc_mode.packetsize = MOUSE_MSC_PACKETSIZE;
			sc->sc_mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
			sc->sc_mode.syncmask[1] = MOUSE_MSC_SYNC;
		} else if (sc->sc_mode.level == 1) {
			sc->sc_mode.protocol = MOUSE_PROTO_SYSMOUSE;
			sc->sc_mode.packetsize = MOUSE_SYS_PACKETSIZE;
			sc->sc_mode.syncmask[0] = MOUSE_SYS_SYNCMASK;
			sc->sc_mode.syncmask[1] = MOUSE_SYS_SYNC;
		}
		atp_reset_buf(sc);
		break;
	case MOUSE_GETLEVEL:
		*(int *)addr = sc->sc_mode.level;
		break;
	case MOUSE_SETLEVEL:
		if (*(int *)addr < 0 || *(int *)addr > 1) {
			error = EINVAL;
			goto done;
		}
		sc->sc_mode.level = *(int *)addr;
		sc->sc_hw.buttons = 3;

		if (sc->sc_mode.level == 0) {
			sc->sc_mode.protocol = MOUSE_PROTO_MSC;
			sc->sc_mode.packetsize = MOUSE_MSC_PACKETSIZE;
			sc->sc_mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
			sc->sc_mode.syncmask[1] = MOUSE_MSC_SYNC;
		} else if (sc->sc_mode.level == 1) {
			sc->sc_mode.protocol = MOUSE_PROTO_SYSMOUSE;
			sc->sc_mode.packetsize = MOUSE_SYS_PACKETSIZE;
			sc->sc_mode.syncmask[0] = MOUSE_SYS_SYNCMASK;
			sc->sc_mode.syncmask[1] = MOUSE_SYS_SYNC;
		}
		atp_reset_buf(sc);
		break;
	case MOUSE_GETSTATUS: {
		mousestatus_t *status = (mousestatus_t *)addr;

		*status = sc->sc_status;
		sc->sc_status.obutton = sc->sc_status.button;
		sc->sc_status.button  = 0;
		sc->sc_status.dx = 0;
		sc->sc_status.dy = 0;
		sc->sc_status.dz = 0;

		if (status->dx || status->dy || status->dz)
			status->flags |= MOUSE_POSCHANGED;
		if (status->button != status->obutton)
			status->flags |= MOUSE_BUTTONSCHANGED;
		break;
	}
	default:
		error = ENOTTY;
	}

done:
	mtx_unlock(&sc->sc_mutex);
	return (error);
}

static int
atp_sysctl_scale_factor_handler(SYSCTL_HANDLER_ARGS)
{
	int error;
	u_int tmp;
	u_int prev_mickeys_scale_factor;

	prev_mickeys_scale_factor = atp_mickeys_scale_factor;

	tmp = atp_mickeys_scale_factor;
	error = sysctl_handle_int(oidp, &tmp, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (tmp == prev_mickeys_scale_factor)
		return (0);     /* no change */

	atp_mickeys_scale_factor = tmp;
	DPRINTFN(ATP_LLEVEL_INFO, "%s: resetting mickeys_scale_factor to %u\n",
	    ATP_DRIVER_NAME, tmp);

	/* Update dependent thresholds. */
	if (atp_small_movement_threshold == (prev_mickeys_scale_factor >> 3))
		atp_small_movement_threshold = atp_mickeys_scale_factor >> 3;
	if (atp_max_delta_mickeys == ((3 * prev_mickeys_scale_factor) >> 1))
		atp_max_delta_mickeys = ((3 * atp_mickeys_scale_factor) >>1);
	if (atp_slide_min_movement == (prev_mickeys_scale_factor >> 3))
		atp_slide_min_movement = atp_mickeys_scale_factor >> 3;

	return (0);
}

static device_method_t atp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,  atp_probe),
	DEVMETHOD(device_attach, atp_attach),
	DEVMETHOD(device_detach, atp_detach),
	{ 0, 0 }
};

static driver_t atp_driver = {
	ATP_DRIVER_NAME,
	atp_methods,
	sizeof(struct atp_softc)
};

static devclass_t atp_devclass;

DRIVER_MODULE(atp, uhub, atp_driver, atp_devclass, NULL, 0);
MODULE_DEPEND(atp, usb, 1, 1, 1);
