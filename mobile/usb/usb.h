/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See camera.h for full license text.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include "camera.h"

#define MOCK_LOG(level, fmt, ...)	do {		\
	printf("cam_mock: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_camera_mock, "Software camera mock for testing");

#define MOCK_COLOR_BAR_HEIGHT	64
#define MOCK_FACE_RADIUS		48

static int mock_running = 0;
static uint32_t mock_frame_count = 0;
static int32_t mock_ts_offset = 0;
static enum cam_mock_pattern current_pattern = CAM_MOCK_COLORBARS;
static float mock_grad_phase = 0.0f;

static void
mock_write_color_bars(uint8_t *buf, uint32_t w, uint32_t h)
{
	static const uint8_t colors[8][3] = {
		{255, 255, 255},
		{255, 255,   0},
		{  0, 255, 255},
		{  0, 255,   0},
		{255,   0, 255},
		{255,   0,   0},
		{  0,   0, 255},
		{  0,   0,   0},
	};
	uint32_t bar_w = w / 8;

	for (uint32_t y = 0; y < h; y++) {
		for (uint32_t x = 0; x < w; x++) {
			uint32_t c_idx = (x / bar_w) % 8;
			uint32_t p = (y * w + x) * 3;
			buf[p + 0] = colors[c_idx][0];
			buf[p + 1] = colors[c_idx][1];
			buf[p + 2] = colors[c_idx][2];
		}
	}
}

static void
mock_write_gradient(uint8_t *buf, uint32_t w, uint32_t h)
{
	(void)mock_grad_phase;
	for (uint32_t y = 0; y < h; y++) {
		for (uint32_t x = 0; x < w; x++) {
			float fx = (float)x / w;
			float fy = (float)y / h;
			uint32_t p = (y * w + x) * 3;
			buf[p + 0] = (uint8_t)((0.5 + 0.5 * sinf(6.28318f * (fx + mock_grad_phase))) * 255);
			buf[p + 1] = (uint8_t)((0.5 + 0.5 * sinf(6.28318f * (fy + mock_grad_phase))) * 255);
			buf[p + 2] = (uint8_t)((0.5 + 0.5 * sinf(6.28318f * (fx + fy + mock_grad_phase))) * 255);
		}
	}
	mock_grad_phase += 0.02f;
	if (mock_grad_phase > 1.0f)
		mock_grad_phase -= 1.0f;
}

static void
mock_write_face(uint8_t *buf, uint32_t w, uint32_t h)
{
	uint32_t cx = w / 2;
	uint32_t cy = h / 2;

	for (uint32_t y = 0; y < h; y++) {
		for (uint32_t x = 0; x < w; x++) {
			float dx = (float)x - cx;
			float dy = (float)y - cy;
			float r = sqrtf(dx * dx + dy * dy);
			uint32_t p = (y * w + x) * 3;

			if (r <= MOCK_FACE_RADIUS) {
				buf[p + 0] = 200;
				buf[p + 1] = 160;
				buf[p + 2] = 120;
			} else {
				buf[p + 0] = 32;
				buf[p + 1] = 32;
				buf[p + 2] = 64;
			}
		}
	}
}

static int
mock_read_frame(struct cam_frame *frame, uint64_t seq, int64_t ts)
{
	if (frame->data == NULL || frame->size < 3)
		return (EINVAL);

	memset(frame->data, 0, frame->size);

	switch (current_pattern) {
	case CAM_MOCK_COLORBARS:
		mock_write_color_bars(frame->data, frame->width, frame->height);
		break;
	case CAM_MOCK_GRADIENT:
		mock_write_gradient(frame->data, frame->width, frame->height);
		break;
	case CAM_MOCK_FACE:
		mock_write_face(frame->data, frame->width, frame->height);
		break;
	default:
		memset(frame->data, 128, frame->size);
		break;
	}

	frame->seq = (uint32_t)seq;
	frame->timestamp = ts;
	mock_frame_count++;

	return (0);
}

static void
mock_frame_cb(struct cam_handle *h, struct cam_frame *f, void *arg)
{
	(void)h;
	(void)args;
	MOCK_LOG(LOG_DEBUG, "Mock frame seq=%u, ts=%ldus", f->seq, (long)f->timestamp);
}

int
cam_mock_init(void)
{
	mock_running = 1;
	mock_frame_count = 0;
	mock_ts_offset = 0;
	current_pattern = CAM_MOCK_COLORBARS;
	mock_grad_phase = 0.0f;

	MOCK_LOG(LOG_INFO, "Camera mock initialized: pattern=%u", current_pattern);
	return (0);
}

void
cam_mock_shutdown(void)
{
	mock_running = 0;
	MOCK_LOG(LOG_DEBUG, "Camera mock shutdown");
}

int
cam_mock_run(struct cam_handle *handle, enum cam_mock_pattern pattern)
{
	if (pattern < CAM_MOCK_COLORBARS || pattern > CAM_MOCK_NOISE)
		return (EINVAL);

	current_pattern = pattern;
	mock_frame_count = 0;

	MOCK_LOG(LOG_INFO, "Camera mock pattern set to %d", pattern);
	return (0);
}

struct cam_handle *
cam_mock_open(void)
{
	struct cam_handle *h;

	cam_mock_init();

	h = malloc(sizeof(*h), M_CAMERA, M_WAITOK | M_ZERO);
	if (h == NULL)
		return (NULL);

	h->id = 99;
	h->state = CAM_STATE_STREAMING;
	h->format = CAM_FORMAT_YUYV;
	h->width = 640;
	h->height = 480;
	h->fps = 30;

	MOCK_LOG(LOG_INFO, "Mock camera opened: %ux%u@%u", h->width, h->height, h->fps);
	return (h);
}

int
cam_mock_read(struct cam_handle *handle, struct cam_frame *frame)
{
	uint64_t seq;
	int64_t ts;

	if (!mock_running || handle == NULL || frame == NULL)
		return (EINVAL);

	seq = mock_frame_count;
	ts = (int64_t)mock_ts_offset + (int64_t)seq * 1000000LL / handle->fps;

	frame->width = handle->width;
	frame->height = handle->height;
	frame->format = CAM_FORMAT_YUYV;
	frame->size = handle->width * handle->height * 3;

	return (mock_read_frame(frame, seq, ts));
}

int
cam_mock_get_frame_count(void)
{
	return (mock_frame_count);
}

int
cam_mock_set_noise_seed(uint32_t seed)
{
	srand(seed);
	MOCK_LOG(LOG_DEBUG, "Noise seed set to %u", seed);
	return (0);
}

#define USB_MAX_DEVICES	128
#define USB_MAX_ENDPOINTS	32
#define USB_MAX_INTERFACES	16
#define USB_HUB_MAX_PORTS	8

enum usb_speed {
	USB_SPEED_LOW,
	USB_SPEED_FULL,
	USB_SPEED_HIGH,
	USB_SPEED_SUPER,
};

enum usb_request_type {
	USB_REQ_TYPE_STANDARD	= 0x00,
	USB_REQ_TYPE_CLASS	= 0x20,
	USB_REQ_TYPE_VENDOR	= 0x40,
};

enum usb_device_class {
	USB_CLASS_PER_INTERFACE	= 0x00,
	USB_CLASS_AUDIO		= 0x01,
	USB_CLASS_COMMUNICATIONS	= 0x02,
	USB_CLASS_HID		= 0x03,
	USB_CLASS_PHYSICAL	= 0x05,
	USB_CLASS_STILL_IMAGE	= 0x06,
	USB_CLASS_PRINTER	= 0x07,
	USB_CLASS_MASS_STORAGE	= 0x08,
	USB_CLASS_HUB		= 0x09,
	USB_CLASS_CDC_DATA	= 0x0A,
	USB_CLASS_CSCID		= 0x0B,
	USB_CLASS_CONTENT_SEC	= 0x0D,
	USB_CLASS_VIDEO		= 0x0E,
	USB_CLASS_WIRELESS	= 0xE0,
	USB_CLASS_VENDOR_SPEC	= 0xFF,
};

struct usb_device_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint16_t	bcdUSB;
	uint8_t	bDeviceClass;
	uint8_t	bDeviceSubClass;
	uint8_t	bDeviceProtocol;
	uint8_t	bMaxPacketSize0;
	uint16_t	idVendor;
	uint16_t	idProduct;
	uint16_t	bcdDevice;
	uint8_t	iManufacturer;
	uint8_t	iProduct;
	uint8_t	iSerialNumber;
	uint8_t	bNumConfigurations;
};

struct usb_config_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint16_t	wTotalLength;
	uint8_t	bNumInterfaces;
	uint8_t	bConfigurationValue;
	uint8_t	iConfiguration;
	uint8_t	bmAttributes;
	uint8_t	bMaxPower;
};

struct usb_interface_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bInterfaceNumber;
	uint8_t	bAlternateSetting;
	uint8_t	bNumEndpoints;
	uint8_t	bInterfaceClass;
	uint8_t	bInterfaceSubClass;
	uint8_t	bInterfaceProtocol;
	uint8_t	iInterface;
};

struct usb_endpoint_descriptor {
	uint8_t	bLength;
	uint8_t	bDescriptorType;
	uint8_t	bEndpointAddress;
	uint8_t	bmAttributes;
	uint16_t	wMaxPacketSize;
	uint8_t	bInterval;
};

struct usb_device {
	uint32_t	id;
	uint8_t	port_number;
	enum usb_speed speed;
	struct usb_device_descriptor dev_desc;
	struct usb_config_descriptor config_desc;
	struct usb_interface_descriptor iface_desc;
	struct usb_endpoint_descriptor ep_desc[USB_MAX_ENDPOINTS];
	uint8_t	addr;
	uint8_t	configured;
	uint8_t	hub;
};

struct usb_dev_handle {
	uint32_t id;
	struct usb_device *dev;
	int fd;
};

#define USB_IOCTL_GETDEVICE	_IOWR('U', 0x01, struct usb_device)
#define USB_IOCTL_GETDESC	_IOWR('U', 0x02, struct usb_device_descriptor)
#define USB_IOCTL_OPEN		_IOW('U',  0x03, uint32_t)
#define USB_IOCTL_CLOSE		_IOW('U',  0x04, uint32_t)
#define USB_IOCTL_CLAIM		_IOW('U',  0x05, uint8_t)
#define USB_IOCTL_RELEASE	_IOW('U',  0x06, uint8_t)
#define USB_IOCTL_BULK		_IOWR('U', 0x07, void *)
#define USB_IOCTL_CTRL		_IOWR('U', 0x08, void *)
#define USB_IOCTL_ENUMERATE	_IO('U',  0x09)
#define USB_IOCTL_PORTRESET	_IOW('U',  0x0A, uint32_t)

#define USB_CTRL_DEVICE_TO_HOST	0x80
#define USB_CTRL_HOST_TO_DEVICE	0x00
#define USB_CTRL_RECIPIENT_DEVICE	0x00
#define USB_CTRL_RECIPIENT_INTERFACE	0x01
#define USB_CTRL_RECIPIENT_ENDPOINT	0x02

#define USB_SYSFS_BUS		"/sys/bus/usb/devices"
#define USB_DEVICE_PATH		"/dev/bus/usb"
#define USB_MAX_BUS		8

struct usb_bus {
	uint32_t id;
	struct usb_hub root_hub;
};

struct usb_hub {
	uint32_t port_count;
	uint32_t device_count;
	struct usb_device ports[USB_HUB_MAX_PORTS];
	struct usb_bus *bus;
};

struct usb_context {
	struct usb_bus buses[USB_MAX_BUS];
	uint32_t bus_count;
	struct usb_device devices[USB_MAX_DEVICES];
	uint32_t device_count;
};

int usb_init(void);
int usb_enumerate(struct usb_device devices[USB_MAX_DEVICES], uint32_t *count);
struct usb_dev_handle *usb_open(uint16_t vendor, uint16_t product);
struct usb_dev_handle *usb_open_device(uint32_t device_id);
int usb_close(struct usb_dev_handle *h);
int usb_claim_interface(struct usb_dev_handle *h, uint8_t iface);
int usb_release_interface(struct usb_dev_handle *h, uint8_t iface);
int usb_bulk_transfer(struct usb_dev_handle *h, uint8_t endpoint, void *data,
    uint32_t length, uint32_t *transferred, uint32_t timeout);
int usb_control_transfer(struct usb_dev_handle *h, uint8_t bmRequestType,
    uint8_t bRequest, uint16_t wValue, uint16_t wIndex, void *data,
    uint16_t length, uint32_t timeout);
int usb_get_device_descriptor(struct usb_dev_handle *h,
    struct usb_device_descriptor *desc);
const char *usb_speed_name(enum usb_speed speed);
const char *usb_class_name(uint8_t class);
void usb_shutdown(void);

#endif /* _MOBILE_USB_USB_H_ */
