/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

#ifndef _USB2_HUB_H_
#define	_USB2_HUB_H_

/*
 * The following structure defines an USB port.
 */
struct usb2_port {
	uint8_t	restartcnt;
#define	USB_RESTART_MAX 5
	uint8_t	device_index;		/* zero means not valid */
	uint8_t	usb2_mode:1;		/* current USB mode */
	uint8_t	unused:7;
};

/*
 * The following structure defines how many bytes are
 * left in an 1ms USB time slot.
 */
struct usb2_fs_isoc_schedule {
	uint16_t total_bytes;
	uint8_t	frame_bytes;
	uint8_t	frame_slot;
};

/*
 * The following structure defines an USB HUB.
 */
struct usb2_hub {
	struct usb2_fs_isoc_schedule fs_isoc_schedule[USB_ISOC_TIME_MAX];
	struct usb2_device *hubudev;	/* the HUB device */
	usb2_error_t (*explore) (struct usb2_device *hub);
	void   *hubsoftc;
	uint32_t uframe_usage[USB_HS_MICRO_FRAMES_MAX];
	uint16_t portpower;		/* mA per USB port */
	uint8_t	isoc_last_time;
	uint8_t	nports;
	struct usb2_port ports[0];
};

/* function prototypes */

uint8_t	usb2_intr_schedule_adjust(struct usb2_device *udev, int16_t len, uint8_t slot);
void	usb2_fs_isoc_schedule_init_all(struct usb2_fs_isoc_schedule *fss);
void	usb2_bus_port_set_device(struct usb2_bus *bus, struct usb2_port *up, struct usb2_device *udev, uint8_t device_index);
struct usb2_device *usb2_bus_port_get_device(struct usb2_bus *bus, struct usb2_port *up);
void	usb2_needs_explore(struct usb2_bus *bus, uint8_t do_probe);
void	usb2_needs_explore_all(void);

#endif					/* _USB2_HUB_H_ */
