#-
# Copyright (c) 2019 Vladimir Kondratyev
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <dev/hid/hid.h>

# Any function listed here can do unbound sleeps waiting for IO to complete.

INTERFACE hid;

# Interrupts interface

#
# Allocate memory and initialise interrupt transfers.
# intr callback function which is called if input data is available.
# context is the private softc pointer, which will be used to callback.
# rdesc is pointer to structire containing requested maximal sizes of input,
# output and feature reports.  It is used by hardware transport drivers
# to determine sizes of internal buffers.
# This function returns zero upon success. A non-zero return value indicates
# failure.
#
METHOD void intr_setup {
	device_t dev;
	hid_intr_t intr;
	void *context;
	struct hid_rdesc_info *rdesc;
};

#
# Release all allocated resources associated with interrupt transfers.
#
METHOD void intr_unsetup {
	device_t dev;
};

#
# Start the interrupt transfers if not already started.
#
METHOD int intr_start {
	device_t dev;
};

#
# Stop the interrupt transfers if not already stopped.
#
METHOD int intr_stop {
	device_t dev;
};

#
# The following function gets called from the HID keyboard driver
# when the system has paniced.
#
METHOD void intr_poll {
	device_t dev;
};

# HID interface

#
# Read out an report descriptor from the HID device.
#
METHOD int get_rdesc {
	device_t dev;
	void *data;
	hid_size_t len;
};

#
# Get input data from the device. Data should be read in chunks
# of the size prescribed by the report descriptor.
# This function interferes with interrupt transfers and should not be used.
#
METHOD int read {
	device_t dev;
	void *data;
	hid_size_t maxlen;
	hid_size_t *actlen;
};

#
# Send data to the device. Data should be written in
# chunks of the size prescribed by the report descriptor.
#
METHOD int write {
	device_t dev;
	const void *data;
	hid_size_t len;
};

#
# Get a report from the device without waiting for data on the interrupt.
# Copies a maximum of len bytes of the report data into the memory specified
# by data. Upon return actlen is set to the number of bytes copied. The type
# field indicates which report is requested. It should be HID_INPUT_REPORT,
# HID_OUTPUT_REPORT, or HID_FEATURE_REPORT. This call may fail if the device
# does not support this feature.
#
METHOD int get_report {
	device_t dev;
	void *data;
	hid_size_t maxlen;
	hid_size_t *actlen;
	uint8_t type;
	uint8_t id;
};

#
# Set a report in the device. The type field indicates which report is to be
# set. It should be HID_INPUT_REPORT, HID_OUTPUT_REPORT, or HID_FEATURE_REPORT.
# The value of the report is specified by the data and the len fields.
# This call may fail if the device does not support this feature.
#
METHOD int set_report {
	device_t dev;
	const void *data;
	hid_size_t len;
	uint8_t type;
	uint8_t id;
};

#
# Set duration between input reports (in mSec).
#
METHOD int set_idle {
	device_t dev;
	uint16_t duration;
	uint8_t id;
};

#
# Switch between the boot protocol and the report protocol (or vice versa).
#
METHOD int set_protocol {
	device_t dev;
	uint16_t protocol;
};
