/*
 * Intel ACPI functions
 *
 * _DSM related code stolen from nouveau_acpi.c.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>
#include <dev/drm2/i915/i915_drv.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>

#define INTEL_DSM_REVISION_ID 1 /* For Calpella anyway... */

#define INTEL_DSM_FN_SUPPORTED_FUNCTIONS 0 /* No args */
#define INTEL_DSM_FN_PLATFORM_MUX_INFO 1 /* No args */

static struct intel_dsm_priv {
	ACPI_HANDLE dhandle;
} intel_dsm_priv;

static const u8 intel_dsm_guid[] = {
	0xd3, 0x73, 0xd8, 0x7e,
	0xd0, 0xc2,
	0x4f, 0x4e,
	0xa8, 0x54,
	0x0f, 0x13, 0x17, 0xb0, 0x1c, 0x2c
};

static int intel_dsm(ACPI_HANDLE handle, int func, int arg)
{
	ACPI_BUFFER output = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT_LIST input;
	ACPI_OBJECT params[4];
	ACPI_OBJECT *obj;
	u32 result;
	int ret = 0;

	input.Count = 4;
	input.Pointer = params;
	params[0].Type = ACPI_TYPE_BUFFER;
	params[0].Buffer.Length = sizeof(intel_dsm_guid);
	params[0].Buffer.Pointer = __DECONST(char *, intel_dsm_guid);
	params[1].Type = ACPI_TYPE_INTEGER;
	params[1].Integer.Value = INTEL_DSM_REVISION_ID;
	params[2].Type = ACPI_TYPE_INTEGER;
	params[2].Integer.Value = func;
	params[3].Type = ACPI_TYPE_INTEGER;
	params[3].Integer.Value = arg;

	ret = AcpiEvaluateObject(handle, "_DSM", &input, &output);
	if (ret) {
		DRM_DEBUG_DRIVER("failed to evaluate _DSM: %d\n", ret);
		return ret;
	}

	obj = (ACPI_OBJECT *)output.Pointer;

	result = 0;
	switch (obj->Type) {
	case ACPI_TYPE_INTEGER:
		result = obj->Integer.Value;
		break;

	case ACPI_TYPE_BUFFER:
		if (obj->Buffer.Length == 4) {
			result = (obj->Buffer.Pointer[0] |
				(obj->Buffer.Pointer[1] <<  8) |
				(obj->Buffer.Pointer[2] << 16) |
				(obj->Buffer.Pointer[3] << 24));
			break;
		}
	default:
		ret = -EINVAL;
		break;
	}
	if (result == 0x80000002)
		ret = -ENODEV;

	AcpiOsFree(output.Pointer);
	return ret;
}

static char *intel_dsm_port_name(u8 id)
{
	switch (id) {
	case 0:
		return "Reserved";
	case 1:
		return "Analog VGA";
	case 2:
		return "LVDS";
	case 3:
		return "Reserved";
	case 4:
		return "HDMI/DVI_B";
	case 5:
		return "HDMI/DVI_C";
	case 6:
		return "HDMI/DVI_D";
	case 7:
		return "DisplayPort_A";
	case 8:
		return "DisplayPort_B";
	case 9:
		return "DisplayPort_C";
	case 0xa:
		return "DisplayPort_D";
	case 0xb:
	case 0xc:
	case 0xd:
		return "Reserved";
	case 0xe:
		return "WiDi";
	default:
		return "bad type";
	}
}

static char *intel_dsm_mux_type(u8 type)
{
	switch (type) {
	case 0:
		return "unknown";
	case 1:
		return "No MUX, iGPU only";
	case 2:
		return "No MUX, dGPU only";
	case 3:
		return "MUXed between iGPU and dGPU";
	default:
		return "bad type";
	}
}

static void intel_dsm_platform_mux_info(void)
{
	ACPI_BUFFER output = { ACPI_ALLOCATE_BUFFER, NULL };
	ACPI_OBJECT_LIST input;
	ACPI_OBJECT params[4];
	ACPI_OBJECT *pkg;
	int i, ret;

	input.Count = 4;
	input.Pointer = params;
	params[0].Type = ACPI_TYPE_BUFFER;
	params[0].Buffer.Length = sizeof(intel_dsm_guid);
	params[0].Buffer.Pointer = __DECONST(char *, intel_dsm_guid);
	params[1].Type = ACPI_TYPE_INTEGER;
	params[1].Integer.Value = INTEL_DSM_REVISION_ID;
	params[2].Type = ACPI_TYPE_INTEGER;
	params[2].Integer.Value = INTEL_DSM_FN_PLATFORM_MUX_INFO;
	params[3].Type = ACPI_TYPE_INTEGER;
	params[3].Integer.Value = 0;

	ret = AcpiEvaluateObject(intel_dsm_priv.dhandle, "_DSM", &input,
				   &output);
	if (ret) {
		DRM_DEBUG_DRIVER("failed to evaluate _DSM: %d\n", ret);
		goto out;
	}

	pkg = (ACPI_OBJECT *)output.Pointer;

	if (pkg->Type == ACPI_TYPE_PACKAGE) {
		ACPI_OBJECT *connector_count = &pkg->Package.Elements[0];
		DRM_DEBUG_DRIVER("MUX info connectors: %lld\n",
			  (unsigned long long)connector_count->Integer.Value);
		for (i = 1; i < pkg->Package.Count; i++) {
			ACPI_OBJECT *obj = &pkg->Package.Elements[i];
			ACPI_OBJECT *connector_id =
				&obj->Package.Elements[0];
			ACPI_OBJECT *info = &obj->Package.Elements[1];
			DRM_DEBUG_DRIVER("Connector id: 0x%016llx\n",
				  (unsigned long long)connector_id->Integer.Value);
			DRM_DEBUG_DRIVER("  port id: %s\n",
			       intel_dsm_port_name(info->Buffer.Pointer[0]));
			DRM_DEBUG_DRIVER("  display mux info: %s\n",
			       intel_dsm_mux_type(info->Buffer.Pointer[1]));
			DRM_DEBUG_DRIVER("  aux/dc mux info: %s\n",
			       intel_dsm_mux_type(info->Buffer.Pointer[2]));
			DRM_DEBUG_DRIVER("  hpd mux info: %s\n",
			       intel_dsm_mux_type(info->Buffer.Pointer[3]));
		}
	}

out:
	AcpiOsFree(output.Pointer);
}

static bool intel_dsm_pci_probe(device_t dev)
{
	ACPI_HANDLE dhandle, intel_handle;
	ACPI_STATUS status;
	int ret;

	dhandle = acpi_get_handle(dev);
	if (!dhandle)
		return false;

	status = AcpiGetHandle(dhandle, "_DSM", &intel_handle);
	if (ACPI_FAILURE(status)) {
		DRM_DEBUG_KMS("no _DSM method for intel device\n");
		return false;
	}

	ret = intel_dsm(dhandle, INTEL_DSM_FN_SUPPORTED_FUNCTIONS, 0);
	if (ret < 0) {
		DRM_DEBUG_KMS("failed to get supported _DSM functions\n");
		return false;
	}

	intel_dsm_priv.dhandle = dhandle;

	intel_dsm_platform_mux_info();
	return true;
}

static bool intel_dsm_detect(void)
{
	char acpi_method_name[255] = { 0 };
	ACPI_BUFFER buffer = {sizeof(acpi_method_name), acpi_method_name};
	device_t dev = NULL;
	bool has_dsm = false;
	int vga_count = 0;

#ifdef FREEBSD_WIP
	while ((pdev = pci_get_class(PCI_CLASS_DISPLAY_VGA << 8, pdev)) != NULL) {
#endif /* FREEBSD_WIP */
	if ((dev = pci_find_class(PCIC_DISPLAY, PCIS_DISPLAY_VGA)) != NULL) {
		vga_count++;
		has_dsm |= intel_dsm_pci_probe(dev);
	}

	if (vga_count == 2 && has_dsm) {
		AcpiGetName(intel_dsm_priv.dhandle, ACPI_FULL_PATHNAME, &buffer);
		DRM_DEBUG_DRIVER("VGA switcheroo: detected DSM switching method %s handle\n",
				 acpi_method_name);
		return true;
	}

	return false;
}

void intel_register_dsm_handler(void)
{
	if (!intel_dsm_detect())
		return;
}

void intel_unregister_dsm_handler(void)
{
}
