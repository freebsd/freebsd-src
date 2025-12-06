/*
 * Copyright © 2018 Red Hat, Inc.
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
 */

#pragma once

#include "util.h"

#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>

/**
 * Handle to the quirks context.
 */
struct quirks_context;

/**
 * Contains all quirks set for a single device.
 */
struct quirks;

struct quirk_dimensions {
	size_t x, y;
};

struct quirk_range {
	int lower, upper;
};

struct quirk_tuples {
	struct {
		int first;
		int second;
		int third;
	} tuples[32];
	size_t ntuples;
};

/**
 * Quirks known to libinput. Moused does not support all of them.
 */
enum quirk {
	QUIRK_MODEL_ALPS_SERIAL_TOUCHPAD = 100,
	QUIRK_MODEL_APPLE_TOUCHPAD,
	QUIRK_MODEL_APPLE_TOUCHPAD_ONEBUTTON,
	QUIRK_MODEL_BOUNCING_KEYS,
	QUIRK_MODEL_CHROMEBOOK,
	QUIRK_MODEL_CLEVO_W740SU,
	QUIRK_MODEL_DELL_CANVAS_TOTEM,
	QUIRK_MODEL_HP_PAVILION_DM4_TOUCHPAD,
	QUIRK_MODEL_HP_ZBOOK_STUDIO_G3,
	QUIRK_MODEL_INVERT_HORIZONTAL_SCROLLING,
	QUIRK_MODEL_LENOVO_SCROLLPOINT,
	QUIRK_MODEL_LENOVO_T450_TOUCHPAD,
	QUIRK_MODEL_LENOVO_X1GEN6_TOUCHPAD,
	QUIRK_MODEL_LENOVO_X230,
	QUIRK_MODEL_SYNAPTICS_SERIAL_TOUCHPAD,
	QUIRK_MODEL_SYSTEM76_BONOBO,
	QUIRK_MODEL_SYSTEM76_GALAGO,
	QUIRK_MODEL_SYSTEM76_KUDU,
	QUIRK_MODEL_TABLET_MODE_NO_SUSPEND,
	QUIRK_MODEL_TABLET_MODE_SWITCH_UNRELIABLE,
	QUIRK_MODEL_TOUCHPAD_VISIBLE_MARKER,
	QUIRK_MODEL_TRACKBALL,
	QUIRK_MODEL_WACOM_TOUCHPAD,
	QUIRK_MODEL_PRESSURE_PAD,
	QUIRK_MODEL_TOUCHPAD_PHANTOM_CLICKS,

	_QUIRK_LAST_MODEL_QUIRK_, /* Guard: do not modify */

	QUIRK_ATTR_SIZE_HINT = 300,
	QUIRK_ATTR_TOUCH_SIZE_RANGE,
	QUIRK_ATTR_PALM_SIZE_THRESHOLD,
	QUIRK_ATTR_LID_SWITCH_RELIABILITY,
	QUIRK_ATTR_KEYBOARD_INTEGRATION,
	QUIRK_ATTR_TRACKPOINT_INTEGRATION,
	QUIRK_ATTR_TPKBCOMBO_LAYOUT,
	QUIRK_ATTR_PRESSURE_RANGE,
	QUIRK_ATTR_PALM_PRESSURE_THRESHOLD,
	QUIRK_ATTR_RESOLUTION_HINT,
	QUIRK_ATTR_TRACKPOINT_MULTIPLIER,
	QUIRK_ATTR_THUMB_PRESSURE_THRESHOLD,
	QUIRK_ATTR_USE_VELOCITY_AVERAGING,
	QUIRK_ATTR_TABLET_SMOOTHING,
	QUIRK_ATTR_THUMB_SIZE_THRESHOLD,
	QUIRK_ATTR_MSC_TIMESTAMP,
	QUIRK_ATTR_EVENT_CODE,
	QUIRK_ATTR_INPUT_PROP,

	_QUIRK_LAST_ATTR_QUIRK_, /* Guard: do not modify */


	/* Daemon parameters */
	MOUSED_GRAB_DEVICE = 1000,
	MOUSED_IGNORE_DEVICE,

	/* Standard moused parameters */
	MOUSED_CLICK_THRESHOLD,
	MOUSED_DRIFT_TERMINATE,
	MOUSED_DRIFT_DISTANCE,
	MOUSED_DRIFT_TIME,
	MOUSED_DRIFT_AFTER,
	MOUSED_EMULATE_THIRD_BUTTON,
	MOUSED_EMULATE_THIRD_BUTTON_TIMEOUT,
	MOUSED_EXPONENTIAL_ACCEL,
	MOUSED_EXPONENTIAL_OFFSET,
	MOUSED_LINEAR_ACCEL_X,
	MOUSED_LINEAR_ACCEL_Y,
	MOUSED_LINEAR_ACCEL_Z,
	MOUSED_MAP_Z_AXIS,
	MOUSED_VIRTUAL_SCROLL_ENABLE,
	MOUSED_HOR_VIRTUAL_SCROLL_ENABLE,
	MOUSED_VIRTUAL_SCROLL_SPEED,
	MOUSED_VIRTUAL_SCROLL_THRESHOLD,
	MOUSED_WMODE,

	/* Touchpad parameters from psm(4) driver */
	MOUSED_TWO_FINGER_SCROLL,
	MOUSED_NATURAL_SCROLL,
	MOUSED_THREE_FINGER_DRAG,
	MOUSED_SOFTBUTTON2_X,
	MOUSED_SOFTBUTTON3_X,
	MOUSED_SOFTBUTTONS_Y,
	MOUSED_TAP_TIMEOUT,
	MOUSED_TAP_PRESSURE_THRESHOLD,
	MOUSED_TAP_MAX_DELTA,
	MOUSED_TAPHOLD_TIMEOUT,
	MOUSED_VSCROLL_MIN_DELTA,
	MOUSED_VSCROLL_HOR_AREA,
	MOUSED_VSCROLL_VER_AREA,

	_MOUSED_LAST_OPTION_ /* Guard: do not modify */
};

/**
 * Returns a printable name for the quirk. This name is for developer
 * tools, not user consumption. Do not display this in a GUI.
 */
const char*
quirk_get_name(enum quirk q);

/**
 * Log priorities used if custom logging is enabled.
 */
enum quirks_log_priorities {
	QLOG_NOISE = LOG_DEBUG + 1,
	QLOG_DEBUG = LOG_DEBUG,
	QLOG_INFO = LOG_INFO,
	QLOG_ERROR = LOG_ERR,
	QLOG_PARSER_ERROR = LOG_CRIT,
};

/**
 * Log type to be used for logging. Use the moused logging to hook up a
 * moused log handler. This will cause the quirks to reduce the noise and
 * only provide useful messages.
 *
 * QLOG_CUSTOM_LOG_PRIORITIES enables more fine-grained and verbose logging,
 * allowing debugging tools to be more useful.
 */
enum quirks_log_type {
	QLOG_MOUSED_LOGGING,
	QLOG_CUSTOM_LOG_PRIORITIES,
};

/**
 * Initialize the quirks subsystem. This function must be called
 * before anything else.
 *
 * If log_type is QLOG_CUSTOM_LOG_PRIORITIES, the log handler is called with
 * the custom QLOG_* log priorities. Otherwise, the log handler only uses
 * the moused (syslog) log priorities.
 *
 * @param config_file A file path to main configuration file
 * @param quirks_path The directory containing the various quirk files
 * @param log_handler The moused log handler called for debugging output
 *
 * @return an opaque handle to the context
 */
struct quirks_context *
quirks_init_subsystem(const char *config_file,
		      const char *quirks_path,
		      moused_log_handler log_handler,
		      enum quirks_log_type log_type);

/**
 * Clean up after ourselves. This function must be called
 * as the last call to the quirks subsystem.
 *
 * All quirks returned to the caller in quirks_fetch_for_device() must be
 * unref'd before this call.
 *
 * @return Always NULL
 */
struct quirks_context *
quirks_context_unref(struct quirks_context *ctx);

DEFINE_UNREF_CLEANUP_FUNC(quirks_context);

struct quirks_context *
quirks_context_ref(struct quirks_context *ctx);

/**
 * Fetch the quirks for a given device. If no quirks are defined, this
 * function returns NULL.
 *
 * @return A new quirks struct, use quirks_unref() to release
 */
struct quirks *
quirks_fetch_for_device(struct quirks_context *ctx,
			struct device *device);

/**
 * Reduce the refcount by one. When the refcount reaches zero, the
 * associated struct is released.
 *
 * @return Always NULL
 */
struct quirks *
quirks_unref(struct quirks *q);

DEFINE_UNREF_CLEANUP_FUNC(quirks);

/**
 * Returns true if the given quirk applies is in this quirk list.
 */
bool
quirks_has_quirk(struct quirks *q, enum quirk which);

/**
 * Get the value of the given quirk, as unsigned integer.
 * This function will assert if the quirk type does not match the
 * requested type. If the quirk is not set for this device, val is
 * unchanged.
 *
 * @return true if the quirk value is valid, false otherwise.
 */
bool
quirks_get_uint32(struct quirks *q,
		  enum quirk which,
		  uint32_t *val);

/**
 * Get the value of the given quirk, as signed integer.
 * This function will assert if the quirk type does not match the
 * requested type. If the quirk is not set for this device, val is
 * unchanged.
 *
 * @return true if the quirk value is valid, false otherwise.
 */
bool
quirks_get_int32(struct quirks *q,
		 enum quirk which,
		 int32_t *val);

/**
 * Get the value of the given quirk, as double.
 * This function will assert if the quirk type does not match the
 * requested type. If the quirk is not set for this device, val is
 * unchanged.
 *
 * @return true if the quirk value is valid, false otherwise.
 */
bool
quirks_get_double(struct quirks *q,
		  enum quirk which,
		  double *val);

/**
 * Get the value of the given quirk, as string.
 * This function will assert if the quirk type does not match the
 * requested type. If the quirk is not set for this device, val is
 * unchanged.
 *
 * val is set to the string, do not modify or free it. The lifetime of the
 * returned string is bound to the lifetime of the quirk.
 *
 * @return true if the quirk value is valid, false otherwise.
 */
bool
quirks_get_string(struct quirks *q,
		  enum quirk which,
		  char **val);

/**
 * Get the value of the given quirk, as bool.
 * This function will assert if the quirk type does not match the
 * requested type. If the quirk is not set for this device, val is
 * unchanged.
 *
 * @return true if the quirk value is valid, false otherwise.
 */
bool
quirks_get_bool(struct quirks *q,
		enum quirk which,
		bool *val);

/**
 * Get the value of the given quirk, as dimension.
 * This function will assert if the quirk type does not match the
 * requested type. If the quirk is not set for this device, val is
 * unchanged.
 *
 * @return true if the quirk value is valid, false otherwise.
 */
bool
quirks_get_dimensions(struct quirks *q,
		      enum quirk which,
		      struct quirk_dimensions *val);

/**
 * Get the value of the given quirk, as range.
 * This function will assert if the quirk type does not match the
 * requested type. If the quirk is not set for this device, val is
 * unchanged.
 *
 * @return true if the quirk value is valid, false otherwise.
 */
bool
quirks_get_range(struct quirks *q,
		 enum quirk which,
		 struct quirk_range *val);

/**
 * Get the tuples of the given quirk.
 * This function will assert if the quirk type does not match the
 * requested type. If the quirk is not set for this device, tuples is
 * unchanged.
 *
 * @return true if the quirk value is valid, false otherwise.
 */
bool
quirks_get_tuples(struct quirks *q,
		  enum quirk which,
		  const struct quirk_tuples **tuples);

/**
 * Get the uint32 array of the given quirk.
 * This function will assert if the quirk type does not match the
 * requested type. If the quirk is not set for this device, tuples is
 * unchanged.
 *
 * @return true if the quirk value is valid, false otherwise.
 */
bool
quirks_get_uint32_array(struct quirks *q,
			enum quirk which,
			const uint32_t **array,
			size_t *nelements);
