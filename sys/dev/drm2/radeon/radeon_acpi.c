/*
 * Copyright 2012 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm_crtc_helper.h>
#include "radeon.h"
#include "radeon_acpi.h"
#include "atom.h"

#define ACPI_AC_CLASS           "ac_adapter"

struct atif_verify_interface {
	u16 size;		/* structure size in bytes (includes size field) */
	u16 version;		/* version */
	u32 notification_mask;	/* supported notifications mask */
	u32 function_bits;	/* supported functions bit vector */
} __packed;

struct atif_system_params {
	u16 size;		/* structure size in bytes (includes size field) */
	u32 valid_mask;		/* valid flags mask */
	u32 flags;		/* flags */
	u8 command_code;	/* notify command code */
} __packed;

struct atif_sbios_requests {
	u16 size;		/* structure size in bytes (includes size field) */
	u32 pending;		/* pending sbios requests */
	u8 panel_exp_mode;	/* panel expansion mode */
	u8 thermal_gfx;		/* thermal state: target gfx controller */
	u8 thermal_state;	/* thermal state: state id (0: exit state, non-0: state) */
	u8 forced_power_gfx;	/* forced power state: target gfx controller */
	u8 forced_power_state;	/* forced power state: state id */
	u8 system_power_src;	/* system power source */
	u8 backlight_level;	/* panel backlight level (0-255) */
} __packed;

#define ATIF_NOTIFY_MASK	0x3
#define ATIF_NOTIFY_NONE	0
#define ATIF_NOTIFY_81		1
#define ATIF_NOTIFY_N		2

struct atcs_verify_interface {
	u16 size;		/* structure size in bytes (includes size field) */
	u16 version;		/* version */
	u32 function_bits;	/* supported functions bit vector */
} __packed;

/* Call the ATIF method
 */
/**
 * radeon_atif_call - call an ATIF method
 *
 * @handle: acpi handle
 * @function: the ATIF function to execute
 * @params: ATIF function params
 *
 * Executes the requested ATIF function (all asics).
 * Returns a pointer to the acpi output buffer.
 */
static ACPI_OBJECT *radeon_atif_call(ACPI_HANDLE handle, int function,
		ACPI_BUFFER *params)
{
	ACPI_STATUS status;
	ACPI_OBJECT atif_arg_elements[2];
	ACPI_OBJECT_LIST atif_arg;
	ACPI_BUFFER buffer = { ACPI_ALLOCATE_BUFFER, NULL };

	atif_arg.Count = 2;
	atif_arg.Pointer = &atif_arg_elements[0];

	atif_arg_elements[0].Type = ACPI_TYPE_INTEGER;
	atif_arg_elements[0].Integer.Value = function;

	if (params) {
		atif_arg_elements[1].Type = ACPI_TYPE_BUFFER;
		atif_arg_elements[1].Buffer.Length = params->Length;
		atif_arg_elements[1].Buffer.Pointer = params->Pointer;
	} else {
		/* We need a second fake parameter */
		atif_arg_elements[1].Type = ACPI_TYPE_INTEGER;
		atif_arg_elements[1].Integer.Value = 0;
	}

	status = AcpiEvaluateObject(handle, "ATIF", &atif_arg, &buffer);

	/* Fail only if calling the method fails and ATIF is supported */
	if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
		DRM_DEBUG_DRIVER("failed to evaluate ATIF got %s\n",
				 AcpiFormatException(status));
		AcpiOsFree(buffer.Pointer);
		return NULL;
	}

	return buffer.Pointer;
}

/**
 * radeon_atif_parse_notification - parse supported notifications
 *
 * @n: supported notifications struct
 * @mask: supported notifications mask from ATIF
 *
 * Use the supported notifications mask from ATIF function
 * ATIF_FUNCTION_VERIFY_INTERFACE to determine what notifications
 * are supported (all asics).
 */
static void radeon_atif_parse_notification(struct radeon_atif_notifications *n, u32 mask)
{
	n->display_switch = mask & ATIF_DISPLAY_SWITCH_REQUEST_SUPPORTED;
	n->expansion_mode_change = mask & ATIF_EXPANSION_MODE_CHANGE_REQUEST_SUPPORTED;
	n->thermal_state = mask & ATIF_THERMAL_STATE_CHANGE_REQUEST_SUPPORTED;
	n->forced_power_state = mask & ATIF_FORCED_POWER_STATE_CHANGE_REQUEST_SUPPORTED;
	n->system_power_state = mask & ATIF_SYSTEM_POWER_SOURCE_CHANGE_REQUEST_SUPPORTED;
	n->display_conf_change = mask & ATIF_DISPLAY_CONF_CHANGE_REQUEST_SUPPORTED;
	n->px_gfx_switch = mask & ATIF_PX_GFX_SWITCH_REQUEST_SUPPORTED;
	n->brightness_change = mask & ATIF_PANEL_BRIGHTNESS_CHANGE_REQUEST_SUPPORTED;
	n->dgpu_display_event = mask & ATIF_DGPU_DISPLAY_EVENT_SUPPORTED;
}

/**
 * radeon_atif_parse_functions - parse supported functions
 *
 * @f: supported functions struct
 * @mask: supported functions mask from ATIF
 *
 * Use the supported functions mask from ATIF function
 * ATIF_FUNCTION_VERIFY_INTERFACE to determine what functions
 * are supported (all asics).
 */
static void radeon_atif_parse_functions(struct radeon_atif_functions *f, u32 mask)
{
	f->system_params = mask & ATIF_GET_SYSTEM_PARAMETERS_SUPPORTED;
	f->sbios_requests = mask & ATIF_GET_SYSTEM_BIOS_REQUESTS_SUPPORTED;
	f->select_active_disp = mask & ATIF_SELECT_ACTIVE_DISPLAYS_SUPPORTED;
	f->lid_state = mask & ATIF_GET_LID_STATE_SUPPORTED;
	f->get_tv_standard = mask & ATIF_GET_TV_STANDARD_FROM_CMOS_SUPPORTED;
	f->set_tv_standard = mask & ATIF_SET_TV_STANDARD_IN_CMOS_SUPPORTED;
	f->get_panel_expansion_mode = mask & ATIF_GET_PANEL_EXPANSION_MODE_FROM_CMOS_SUPPORTED;
	f->set_panel_expansion_mode = mask & ATIF_SET_PANEL_EXPANSION_MODE_IN_CMOS_SUPPORTED;
	f->temperature_change = mask & ATIF_TEMPERATURE_CHANGE_NOTIFICATION_SUPPORTED;
	f->graphics_device_types = mask & ATIF_GET_GRAPHICS_DEVICE_TYPES_SUPPORTED;
}

/**
 * radeon_atif_verify_interface - verify ATIF
 *
 * @handle: acpi handle
 * @atif: radeon atif struct
 *
 * Execute the ATIF_FUNCTION_VERIFY_INTERFACE ATIF function
 * to initialize ATIF and determine what features are supported
 * (all asics).
 * returns 0 on success, error on failure.
 */
static int radeon_atif_verify_interface(ACPI_HANDLE handle,
		struct radeon_atif *atif)
{
	ACPI_OBJECT *info;
	struct atif_verify_interface output;
	size_t size;
	int err = 0;

	info = radeon_atif_call(handle, ATIF_FUNCTION_VERIFY_INTERFACE, NULL);
	if (!info)
		return -EIO;

	memset(&output, 0, sizeof(output));

	size = *(u16 *) info->Buffer.Pointer;
	if (size < 12) {
		DRM_INFO("ATIF buffer is too small: %zu\n", size);
		err = -EINVAL;
		goto out;
	}
	size = min(sizeof(output), size);

	memcpy(&output, info->Buffer.Pointer, size);

	/* TODO: check version? */
	DRM_DEBUG_DRIVER("ATIF version %u\n", output.version);

	radeon_atif_parse_notification(&atif->notifications, output.notification_mask);
	radeon_atif_parse_functions(&atif->functions, output.function_bits);

out:
	AcpiOsFree(info);
	return err;
}

/**
 * radeon_atif_get_notification_params - determine notify configuration
 *
 * @handle: acpi handle
 * @n: atif notification configuration struct
 *
 * Execute the ATIF_FUNCTION_GET_SYSTEM_PARAMETERS ATIF function
 * to determine if a notifier is used and if so which one
 * (all asics).  This is either Notify(VGA, 0x81) or Notify(VGA, n)
 * where n is specified in the result if a notifier is used.
 * Returns 0 on success, error on failure.
 */
static int radeon_atif_get_notification_params(ACPI_HANDLE handle,
		struct radeon_atif_notification_cfg *n)
{
	ACPI_OBJECT *info;
	struct atif_system_params params;
	size_t size;
	int err = 0;

	info = radeon_atif_call(handle, ATIF_FUNCTION_GET_SYSTEM_PARAMETERS, NULL);
	if (!info) {
		err = -EIO;
		goto out;
	}

	size = *(u16 *) info->Buffer.Pointer;
	if (size < 10) {
		err = -EINVAL;
		goto out;
	}

	memset(&params, 0, sizeof(params));
	size = min(sizeof(params), size);
	memcpy(&params, info->Buffer.Pointer, size);

	DRM_DEBUG_DRIVER("SYSTEM_PARAMS: mask = %#x, flags = %#x\n",
			params.flags, params.valid_mask);
	params.flags = params.flags & params.valid_mask;

	if ((params.flags & ATIF_NOTIFY_MASK) == ATIF_NOTIFY_NONE) {
		n->enabled = false;
		n->command_code = 0;
	} else if ((params.flags & ATIF_NOTIFY_MASK) == ATIF_NOTIFY_81) {
		n->enabled = true;
		n->command_code = 0x81;
	} else {
		if (size < 11) {
			err = -EINVAL;
			goto out;
		}
		n->enabled = true;
		n->command_code = params.command_code;
	}

out:
	DRM_DEBUG_DRIVER("Notification %s, command code = %#x\n",
			(n->enabled ? "enabled" : "disabled"),
			n->command_code);
	AcpiOsFree(info);
	return err;
}

/**
 * radeon_atif_get_sbios_requests - get requested sbios event
 *
 * @handle: acpi handle
 * @req: atif sbios request struct
 *
 * Execute the ATIF_FUNCTION_GET_SYSTEM_BIOS_REQUESTS ATIF function
 * to determine what requests the sbios is making to the driver
 * (all asics).
 * Returns 0 on success, error on failure.
 */
static int radeon_atif_get_sbios_requests(ACPI_HANDLE handle,
		struct atif_sbios_requests *req)
{
	ACPI_OBJECT *info;
	size_t size;
	int count = 0;

	info = radeon_atif_call(handle, ATIF_FUNCTION_GET_SYSTEM_BIOS_REQUESTS, NULL);
	if (!info)
		return -EIO;

	size = *(u16 *)info->Buffer.Pointer;
	if (size < 0xd) {
		count = -EINVAL;
		goto out;
	}
	memset(req, 0, sizeof(*req));

	size = min(sizeof(*req), size);
	memcpy(req, info->Buffer.Pointer, size);
	DRM_DEBUG_DRIVER("SBIOS pending requests: %#x\n", req->pending);

	count = hweight32(req->pending);

out:
	AcpiOsFree(info);
	return count;
}

/**
 * radeon_atif_handler - handle ATIF notify requests
 *
 * @rdev: radeon_device pointer
 * @event: atif sbios request struct
 *
 * Checks the acpi event and if it matches an atif event,
 * handles it.
 * Returns NOTIFY code
 */
void radeon_atif_handler(struct radeon_device *rdev,
    UINT32 type)
{
	struct radeon_atif *atif = &rdev->atif;
	struct atif_sbios_requests req;
	ACPI_HANDLE handle;
	int count;

	DRM_DEBUG_DRIVER("event, type = %#x\n",
			type);

	if (!atif->notification_cfg.enabled ||
			type != atif->notification_cfg.command_code)
		/* Not our event */
		return;

	/* Check pending SBIOS requests */
	handle = rdev->acpi.handle;
	count = radeon_atif_get_sbios_requests(handle, &req);

	if (count <= 0)
		return;

	DRM_DEBUG_DRIVER("ATIF: %d pending SBIOS requests\n", count);

	if (req.pending & ATIF_PANEL_BRIGHTNESS_CHANGE_REQUEST) {
		struct radeon_encoder *enc = atif->encoder_for_bl;

		if (enc) {
			DRM_DEBUG_DRIVER("Changing brightness to %d\n",
					req.backlight_level);

			radeon_set_backlight_level(rdev, enc, req.backlight_level);

#ifdef FREEBSD_WIP
			if (rdev->is_atom_bios) {
				struct radeon_encoder_atom_dig *dig = enc->enc_priv;
				backlight_force_update(dig->bl_dev,
						       BACKLIGHT_UPDATE_HOTKEY);
			} else {
				struct radeon_encoder_lvds *dig = enc->enc_priv;
				backlight_force_update(dig->bl_dev,
						       BACKLIGHT_UPDATE_HOTKEY);
			}
#endif /* FREEBSD_WIP */
		}
	}
	/* TODO: check other events */

	/* We've handled the event, stop the notifier chain. The ACPI interface
	 * overloads ACPI_VIDEO_NOTIFY_PROBE, we don't want to send that to
	 * userspace if the event was generated only to signal a SBIOS
	 * request.
	 */
}

/* Call the ATCS method
 */
/**
 * radeon_atcs_call - call an ATCS method
 *
 * @handle: acpi handle
 * @function: the ATCS function to execute
 * @params: ATCS function params
 *
 * Executes the requested ATCS function (all asics).
 * Returns a pointer to the acpi output buffer.
 */
static union acpi_object *radeon_atcs_call(ACPI_HANDLE handle, int function,
					   ACPI_BUFFER *params)
{
	ACPI_STATUS status;
	ACPI_OBJECT atcs_arg_elements[2];
	ACPI_OBJECT_LIST atcs_arg;
	ACPI_BUFFER buffer = { ACPI_ALLOCATE_BUFFER, NULL };

	atcs_arg.Count = 2;
	atcs_arg.Pointer = &atcs_arg_elements[0];

	atcs_arg_elements[0].Type = ACPI_TYPE_INTEGER;
	atcs_arg_elements[0].Integer.Value = function;

	if (params) {
		atcs_arg_elements[1].Type = ACPI_TYPE_BUFFER;
		atcs_arg_elements[1].Buffer.Length = params->Length;
		atcs_arg_elements[1].Buffer.Pointer = params->Pointer;
	} else {
		/* We need a second fake parameter */
		atcs_arg_elements[1].Type = ACPI_TYPE_INTEGER;
		atcs_arg_elements[1].Integer.Value = 0;
	}

	status = AcpiEvaluateObject(handle, "ATCS", &atcs_arg, &buffer);

	/* Fail only if calling the method fails and ATIF is supported */
	if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
		DRM_DEBUG_DRIVER("failed to evaluate ATCS got %s\n",
				 AcpiFormatException(status));
		AcpiOsFree(buffer.Pointer);
		return NULL;
	}

	return buffer.Pointer;
}

/**
 * radeon_atcs_parse_functions - parse supported functions
 *
 * @f: supported functions struct
 * @mask: supported functions mask from ATCS
 *
 * Use the supported functions mask from ATCS function
 * ATCS_FUNCTION_VERIFY_INTERFACE to determine what functions
 * are supported (all asics).
 */
static void radeon_atcs_parse_functions(struct radeon_atcs_functions *f, u32 mask)
{
	f->get_ext_state = mask & ATCS_GET_EXTERNAL_STATE_SUPPORTED;
	f->pcie_perf_req = mask & ATCS_PCIE_PERFORMANCE_REQUEST_SUPPORTED;
	f->pcie_dev_rdy = mask & ATCS_PCIE_DEVICE_READY_NOTIFICATION_SUPPORTED;
	f->pcie_bus_width = mask & ATCS_SET_PCIE_BUS_WIDTH_SUPPORTED;
}

/**
 * radeon_atcs_verify_interface - verify ATCS
 *
 * @handle: acpi handle
 * @atcs: radeon atcs struct
 *
 * Execute the ATCS_FUNCTION_VERIFY_INTERFACE ATCS function
 * to initialize ATCS and determine what features are supported
 * (all asics).
 * returns 0 on success, error on failure.
 */
static int radeon_atcs_verify_interface(ACPI_HANDLE handle,
					struct radeon_atcs *atcs)
{
	ACPI_OBJECT *info;
	struct atcs_verify_interface output;
	size_t size;
	int err = 0;

	info = radeon_atcs_call(handle, ATCS_FUNCTION_VERIFY_INTERFACE, NULL);
	if (!info)
		return -EIO;

	memset(&output, 0, sizeof(output));

	size = *(u16 *) info->Buffer.Pointer;
	if (size < 8) {
		DRM_INFO("ATCS buffer is too small: %zu\n", size);
		err = -EINVAL;
		goto out;
	}
	size = min(sizeof(output), size);

	memcpy(&output, info->Buffer.Pointer, size);

	/* TODO: check version? */
	DRM_DEBUG_DRIVER("ATCS version %u\n", output.version);

	radeon_atcs_parse_functions(&atcs->functions, output.function_bits);

out:
	AcpiOsFree(info);
	return err;
}

/**
 * radeon_acpi_event - handle notify events
 *
 * @nb: notifier block
 * @val: val
 * @data: acpi event
 *
 * Calls relevant radeon functions in response to various
 * acpi events.
 * Returns NOTIFY code
 */
static void radeon_acpi_event(ACPI_HANDLE handle, UINT32 type,
    void *context)
{
	struct radeon_device *rdev = (struct radeon_device *)context;

#ifdef FREEBSD_WIP
	if (strcmp(entry->device_class, ACPI_AC_CLASS) == 0) {
		if (power_supply_is_system_supplied() > 0)
			DRM_DEBUG_DRIVER("pm: AC\n");
		else
			DRM_DEBUG_DRIVER("pm: DC\n");

		radeon_pm_acpi_event_handler(rdev);
	}
#endif /* FREEBSD_WIP */

	/* Check for pending SBIOS requests */
	radeon_atif_handler(rdev, type);
}

/* Call all ACPI methods here */
/**
 * radeon_acpi_init - init driver acpi support
 *
 * @rdev: radeon_device pointer
 *
 * Verifies the AMD ACPI interfaces and registers with the acpi
 * notifier chain (all asics).
 * Returns 0 on success, error on failure.
 */
int radeon_acpi_init(struct radeon_device *rdev)
{
	ACPI_HANDLE handle;
	struct radeon_atif *atif = &rdev->atif;
	struct radeon_atcs *atcs = &rdev->atcs;
	int ret;

	/* Get the device handle */
	handle = acpi_get_handle(rdev->dev);

	/* No need to proceed if we're sure that ATIF is not supported */
	if (!ASIC_IS_AVIVO(rdev) || !rdev->bios || !handle)
		return 0;

	/* Call the ATCS method */
	ret = radeon_atcs_verify_interface(handle, atcs);
	if (ret) {
		DRM_DEBUG_DRIVER("Call to ATCS verify_interface failed: %d\n", ret);
	}

	/* Call the ATIF method */
	ret = radeon_atif_verify_interface(handle, atif);
	if (ret) {
		DRM_DEBUG_DRIVER("Call to ATIF verify_interface failed: %d\n", ret);
		goto out;
	}

	if (atif->notifications.brightness_change) {
		struct drm_encoder *tmp;
		struct radeon_encoder *target = NULL;

		/* Find the encoder controlling the brightness */
		list_for_each_entry(tmp, &rdev->ddev->mode_config.encoder_list,
				head) {
			struct radeon_encoder *enc = to_radeon_encoder(tmp);

			if ((enc->devices & (ATOM_DEVICE_LCD_SUPPORT)) &&
			    enc->enc_priv) {
				if (rdev->is_atom_bios) {
					struct radeon_encoder_atom_dig *dig = enc->enc_priv;
					if (dig->bl_dev) {
						target = enc;
						break;
					}
				} else {
					struct radeon_encoder_lvds *dig = enc->enc_priv;
					if (dig->bl_dev) {
						target = enc;
						break;
					}
				}
			}
		}

		atif->encoder_for_bl = target;
		if (!target) {
			/* Brightness change notification is enabled, but we
			 * didn't find a backlight controller, this should
			 * never happen.
			 */
			DRM_ERROR("Cannot find a backlight controller\n");
		}
	}

	if (atif->functions.sbios_requests && !atif->functions.system_params) {
		/* XXX check this workraround, if sbios request function is
		 * present we have to see how it's configured in the system
		 * params
		 */
		atif->functions.system_params = true;
	}

	if (atif->functions.system_params) {
		ret = radeon_atif_get_notification_params(handle,
				&atif->notification_cfg);
		if (ret) {
			DRM_DEBUG_DRIVER("Call to GET_SYSTEM_PARAMS failed: %d\n",
					ret);
			/* Disable notification */
			atif->notification_cfg.enabled = false;
		}
	}

out:
	rdev->acpi.handle = handle;
	rdev->acpi.notifier_call = radeon_acpi_event;
	AcpiInstallNotifyHandler(handle, ACPI_DEVICE_NOTIFY,
	    rdev->acpi.notifier_call, rdev);

	return ret;
}

/**
 * radeon_acpi_fini - tear down driver acpi support
 *
 * @rdev: radeon_device pointer
 *
 * Unregisters with the acpi notifier chain (all asics).
 */
void radeon_acpi_fini(struct radeon_device *rdev)
{
	AcpiRemoveNotifyHandler(rdev->acpi.handle, ACPI_DEVICE_NOTIFY,
	    rdev->acpi.notifier_call);
}
