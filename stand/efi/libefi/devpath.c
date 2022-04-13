/*-
 * Copyright (c) 2016 John Baldwin <jhb@FreeBSD.org>
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

#include <efi.h>
#include <efilib.h>
#include <efichar.h>
#include <uuid.h>
#include <machine/_inttypes.h>

static EFI_GUID ImageDevicePathGUID =
    EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;
static EFI_GUID DevicePathToTextGUID = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
static EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *toTextProtocol;
static EFI_GUID DevicePathFromTextGUID =
    EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL_GUID;
static EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *fromTextProtocol;

EFI_DEVICE_PATH *
efi_lookup_image_devpath(EFI_HANDLE handle)
{
	EFI_DEVICE_PATH *devpath;
	EFI_STATUS status;

	status = OpenProtocolByHandle(handle, &ImageDevicePathGUID,
	    (void **)&devpath);
	if (EFI_ERROR(status))
		devpath = NULL;
	return (devpath);
}

EFI_DEVICE_PATH *
efi_lookup_devpath(EFI_HANDLE handle)
{
	EFI_DEVICE_PATH *devpath;
	EFI_STATUS status;

	status = OpenProtocolByHandle(handle, &DevicePathGUID,
	    (void **)&devpath);
	if (EFI_ERROR(status))
		devpath = NULL;
	return (devpath);
}

void
efi_close_devpath(EFI_HANDLE handle)
{
	EFI_STATUS status;

	status = BS->CloseProtocol(handle, &DevicePathGUID, IH, NULL);
	if (EFI_ERROR(status))
		printf("CloseProtocol error: %lu\n", EFI_ERROR_CODE(status));
}

static char *
efi_make_tail(char *suffix)
{
	char *tail;

	tail = NULL;
	if (suffix != NULL)
		(void)asprintf(&tail, "/%s", suffix);
	else
		tail = strdup("");
	return (tail);
}

typedef struct {
	EFI_DEVICE_PATH	Header;
	EFI_GUID	Guid;
	UINT8		VendorDefinedData[1];
} __packed VENDOR_DEVICE_PATH_WITH_DATA;

static char *
efi_vendor_path(const char *type, VENDOR_DEVICE_PATH *node, char *suffix)
{
	uint32_t size = DevicePathNodeLength(&node->Header) - sizeof(*node);
	VENDOR_DEVICE_PATH_WITH_DATA *dp = (VENDOR_DEVICE_PATH_WITH_DATA *)node;
	char *name, *tail, *head;
	char *uuid;
	int rv;

	uuid_to_string((const uuid_t *)(void *)&node->Guid, &uuid, &rv);
	if (rv != uuid_s_ok)
		return (NULL);

	tail = efi_make_tail(suffix);
	rv = asprintf(&head, "%sVendor(%s)[%x:", type, uuid, size);
	free(uuid);
	if (rv < 0)
		return (NULL);

	if (DevicePathNodeLength(&node->Header) > sizeof(*node)) {
		for (uint32_t i = 0; i < size; i++) {
			rv = asprintf(&name, "%s%02x", head,
			    dp->VendorDefinedData[i]);
			if (rv < 0) {
				free(tail);
				free(head);
				return (NULL);
			}
			free(head);
			head = name;
		}
	}

	if (asprintf(&name, "%s]%s", head, tail) < 0)
		name = NULL;
	free(head);
	free(tail);
	return (name);
}

static char *
efi_hw_dev_path(EFI_DEVICE_PATH *node, char *suffix)
{
	uint8_t subtype = DevicePathSubType(node);
	char *name, *tail;

	tail = efi_make_tail(suffix);
	switch (subtype) {
	case HW_PCI_DP:
		if (asprintf(&name, "Pci(%x,%x)%s",
		    ((PCI_DEVICE_PATH *)node)->Device,
		    ((PCI_DEVICE_PATH *)node)->Function, tail) < 0)
			name = NULL;
		break;
	case HW_PCCARD_DP:
		if (asprintf(&name, "PCCARD(%x)%s",
		    ((PCCARD_DEVICE_PATH *)node)->FunctionNumber, tail) < 0)
			name = NULL;
		break;
	case HW_MEMMAP_DP:
		if (asprintf(&name, "MMap(%x,%" PRIx64 ",%" PRIx64 ")%s",
		    ((MEMMAP_DEVICE_PATH *)node)->MemoryType,
		    ((MEMMAP_DEVICE_PATH *)node)->StartingAddress,
		    ((MEMMAP_DEVICE_PATH *)node)->EndingAddress, tail) < 0)
			name = NULL;
		break;
	case HW_VENDOR_DP:
		name = efi_vendor_path("Hardware",
		    (VENDOR_DEVICE_PATH *)node, tail);
		break;
	case HW_CONTROLLER_DP:
		if (asprintf(&name, "Ctrl(%x)%s",
		    ((CONTROLLER_DEVICE_PATH *)node)->Controller, tail) < 0)
			name = NULL;
		break;
	default:
		if (asprintf(&name, "UnknownHW(%x)%s", subtype, tail) < 0)
			name = NULL;
		break;
	}
	free(tail);
	return (name);
}

static char *
efi_acpi_dev_path(EFI_DEVICE_PATH *node, char *suffix)
{
	uint8_t subtype = DevicePathSubType(node);
	ACPI_HID_DEVICE_PATH *acpi = (ACPI_HID_DEVICE_PATH *)node;
	char *name, *tail;

	tail = efi_make_tail(suffix);
	switch (subtype) {
	case ACPI_DP:
		if ((acpi->HID & PNP_EISA_ID_MASK) == PNP_EISA_ID_CONST) {
			switch (EISA_ID_TO_NUM (acpi->HID)) {
			case 0x0a03:
				if (asprintf(&name, "PciRoot(%x)%s",
				    acpi->UID, tail) < 0)
					name = NULL;
				break;
			case 0x0a08:
				if (asprintf(&name, "PcieRoot(%x)%s",
				    acpi->UID, tail) < 0)
					name = NULL;
				break;
			case 0x0604:
				if (asprintf(&name, "Floppy(%x)%s",
				    acpi->UID, tail) < 0)
					name = NULL;
				break;
			case 0x0301:
				if (asprintf(&name, "Keyboard(%x)%s",
				    acpi->UID, tail) < 0)
					name = NULL;
				break;
			case 0x0501:
				if (asprintf(&name, "Serial(%x)%s",
				    acpi->UID, tail) < 0)
					name = NULL;
				break;
			case 0x0401:
				if (asprintf(&name, "ParallelPort(%x)%s",
				    acpi->UID, tail) < 0)
					name = NULL;
				break;
			default:
				if (asprintf(&name, "Acpi(PNP%04x,%x)%s",
				    EISA_ID_TO_NUM(acpi->HID),
				    acpi->UID, tail) < 0)
					name = NULL;
				break;
			}
		} else {
			if (asprintf(&name, "Acpi(%08x,%x)%s",
			    acpi->HID, acpi->UID, tail) < 0)
				name = NULL;
		}
		break;
	case ACPI_EXTENDED_DP:
	default:
		if (asprintf(&name, "UnknownACPI(%x)%s", subtype, tail) < 0)
			name = NULL;
		break;
	}
	free(tail);
	return (name);
}

static char *
efi_messaging_dev_path(EFI_DEVICE_PATH *node, char *suffix)
{
	uint8_t subtype = DevicePathSubType(node);
	char *name;
	char *tail;

	tail = efi_make_tail(suffix);
	switch (subtype) {
	case MSG_ATAPI_DP:
		if (asprintf(&name, "ATA(%s,%s,%x)%s",
		    ((ATAPI_DEVICE_PATH *)node)->PrimarySecondary == 1 ?
		    "Secondary" : "Primary",
		    ((ATAPI_DEVICE_PATH *)node)->SlaveMaster == 1 ?
		    "Slave" : "Master",
		    ((ATAPI_DEVICE_PATH *)node)->Lun, tail) < 0)
			name = NULL;
		break;
	case MSG_SCSI_DP:
		if (asprintf(&name, "SCSI(%x,%x)%s",
		    ((SCSI_DEVICE_PATH *)node)->Pun,
		    ((SCSI_DEVICE_PATH *)node)->Lun, tail) < 0)
			name = NULL;
		break;
	case MSG_FIBRECHANNEL_DP:
		if (asprintf(&name, "Fibre(%" PRIx64 ",%" PRIx64 ")%s",
		    ((FIBRECHANNEL_DEVICE_PATH *)node)->WWN,
		    ((FIBRECHANNEL_DEVICE_PATH *)node)->Lun, tail) < 0)
			name = NULL;
		break;
	case MSG_1394_DP:
		if (asprintf(&name, "I1394(%016" PRIx64 ")%s",
		    ((F1394_DEVICE_PATH *)node)->Guid, tail) < 0)
			name = NULL;
		break;
	case MSG_USB_DP:
		if (asprintf(&name, "USB(%x,%x)%s",
		    ((USB_DEVICE_PATH *)node)->ParentPortNumber,
		    ((USB_DEVICE_PATH *)node)->InterfaceNumber, tail) < 0)
			name = NULL;
		break;
	case MSG_USB_CLASS_DP:
		if (asprintf(&name, "UsbClass(%x,%x,%x,%x,%x)%s",
		    ((USB_CLASS_DEVICE_PATH *)node)->VendorId,
		    ((USB_CLASS_DEVICE_PATH *)node)->ProductId,
		    ((USB_CLASS_DEVICE_PATH *)node)->DeviceClass,
		    ((USB_CLASS_DEVICE_PATH *)node)->DeviceSubClass,
		    ((USB_CLASS_DEVICE_PATH *)node)->DeviceProtocol, tail) < 0)
			name = NULL;
		break;
	case MSG_MAC_ADDR_DP:
		if (asprintf(&name, "MAC(%02x:%02x:%02x:%02x:%02x:%02x,%x)%s",
		    ((MAC_ADDR_DEVICE_PATH *)node)->MacAddress.Addr[0],
		    ((MAC_ADDR_DEVICE_PATH *)node)->MacAddress.Addr[1],
		    ((MAC_ADDR_DEVICE_PATH *)node)->MacAddress.Addr[2],
		    ((MAC_ADDR_DEVICE_PATH *)node)->MacAddress.Addr[3],
		    ((MAC_ADDR_DEVICE_PATH *)node)->MacAddress.Addr[4],
		    ((MAC_ADDR_DEVICE_PATH *)node)->MacAddress.Addr[5],
		    ((MAC_ADDR_DEVICE_PATH *)node)->IfType, tail) < 0)
			name = NULL;
		break;
	case MSG_VENDOR_DP:
		name = efi_vendor_path("Messaging",
		    (VENDOR_DEVICE_PATH *)node, tail);
		break;
	case MSG_UART_DP:
		if (asprintf(&name, "UART(%" PRIu64 ",%u,%x,%x)%s",
		    ((UART_DEVICE_PATH *)node)->BaudRate,
		    ((UART_DEVICE_PATH *)node)->DataBits,
		    ((UART_DEVICE_PATH *)node)->Parity,
		    ((UART_DEVICE_PATH *)node)->StopBits, tail) < 0)
			name = NULL;
		break;
	case MSG_SATA_DP:
		if (asprintf(&name, "Sata(%x,%x,%x)%s",
		    ((SATA_DEVICE_PATH *)node)->HBAPortNumber,
		    ((SATA_DEVICE_PATH *)node)->PortMultiplierPortNumber,
		    ((SATA_DEVICE_PATH *)node)->Lun, tail) < 0)
			name = NULL;
		break;
	default:
		if (asprintf(&name, "UnknownMessaging(%x)%s",
		    subtype, tail) < 0)
			name = NULL;
		break;
	}
	free(tail);
	return (name);
}

static char *
efi_media_dev_path(EFI_DEVICE_PATH *node, char *suffix)
{
	uint8_t subtype = DevicePathSubType(node);
	HARDDRIVE_DEVICE_PATH *hd;
	char *name;
	char *str;
	char *tail;
	int rv;

	tail = efi_make_tail(suffix);
	name = NULL;
	switch (subtype) {
	case MEDIA_HARDDRIVE_DP:
		hd = (HARDDRIVE_DEVICE_PATH *)node;
		switch (hd->SignatureType) {
		case SIGNATURE_TYPE_MBR:
			if (asprintf(&name, "HD(%d,MBR,%08x,%" PRIx64
			    ",%" PRIx64 ")%s",
			    hd->PartitionNumber,
			    *((uint32_t *)(uintptr_t)&hd->Signature[0]),
			    hd->PartitionStart,
			    hd->PartitionSize, tail) < 0)
				name = NULL;
			break;
		case SIGNATURE_TYPE_GUID:
			name = NULL;
			uuid_to_string((const uuid_t *)(void *)
			    &hd->Signature[0], &str, &rv);
			if (rv != uuid_s_ok)
				break;
			rv = asprintf(&name, "HD(%d,GPT,%s,%" PRIx64 ",%"
			    PRIx64 ")%s",
			    hd->PartitionNumber, str,
			    hd->PartitionStart, hd->PartitionSize, tail);
			free(str);
			break;
		default:
			if (asprintf(&name, "HD(%d,%d,0)%s",
			    hd->PartitionNumber,
			    hd->SignatureType, tail) < 0) {
				name = NULL;
			}
			break;
		}
		break;
	case MEDIA_CDROM_DP:
		if (asprintf(&name, "CD(%x,%" PRIx64 ",%" PRIx64 ")%s",
		    ((CDROM_DEVICE_PATH *)node)->BootEntry,
		    ((CDROM_DEVICE_PATH *)node)->PartitionStart,
		    ((CDROM_DEVICE_PATH *)node)->PartitionSize, tail) < 0) {
			name = NULL;
		}
		break;
	case MEDIA_VENDOR_DP:
		name = efi_vendor_path("Media",
		    (VENDOR_DEVICE_PATH *)node, tail);
		break;
	case MEDIA_FILEPATH_DP:
		name = NULL;
		str = NULL;
		if (ucs2_to_utf8(((FILEPATH_DEVICE_PATH *)node)->PathName,
		    &str) == 0) {
			(void)asprintf(&name, "%s%s", str, tail);
			free(str);
		}
		break;
	case MEDIA_PROTOCOL_DP:
		name = NULL;
		uuid_to_string((const uuid_t *)(void *)
		    &((MEDIA_PROTOCOL_DEVICE_PATH *)node)->Protocol,
		    &str, &rv);
		if (rv != uuid_s_ok)
			break;
		rv = asprintf(&name, "Protocol(%s)%s", str, tail);
		free(str);
		break;
	default:
		if (asprintf(&name, "UnknownMedia(%x)%s",
		    subtype, tail) < 0)
			name = NULL;
	}
	free(tail);
	return (name);
}

static char *
efi_translate_devpath(EFI_DEVICE_PATH *devpath)
{
	EFI_DEVICE_PATH *dp = NextDevicePathNode(devpath);
	char *name, *ptr;
	uint8_t type;

	if (!IsDevicePathEnd(devpath))
		name = efi_translate_devpath(dp);
	else
		return (NULL);

	ptr = NULL;
	type = DevicePathType(devpath);
	switch (type) {
	case HARDWARE_DEVICE_PATH:
		ptr = efi_hw_dev_path(devpath, name);
		break;
	case ACPI_DEVICE_PATH:
		ptr = efi_acpi_dev_path(devpath, name);
		break;
	case MESSAGING_DEVICE_PATH:
		ptr = efi_messaging_dev_path(devpath, name);
		break;
	case MEDIA_DEVICE_PATH:
		ptr = efi_media_dev_path(devpath, name);
		break;
	case BBS_DEVICE_PATH:
	default:
		if (asprintf(&ptr, "UnknownPath(%x)%s", type,
		    name? name : "") < 0)
			ptr = NULL;
		break;
	}

	if (ptr != NULL) {
		free(name);
		name = ptr;
	}
	return (name);
}

static CHAR16 *
efi_devpath_to_name(EFI_DEVICE_PATH *devpath)
{
	char *name = NULL;
	CHAR16 *ptr = NULL;
	size_t len;
	int rv;

	name = efi_translate_devpath(devpath);
	if (name == NULL)
		return (NULL);

	/*
	 * We need to return memory from AllocatePool, so it can be freed
	 * with FreePool() in efi_free_devpath_name().
	 */
	rv = utf8_to_ucs2(name, &ptr, &len);
	free(name);
	if (rv == 0) {
		CHAR16 *out = NULL;
		EFI_STATUS status;

		status = BS->AllocatePool(EfiLoaderData, len, (void **)&out);
		if (EFI_ERROR(status)) {
			free(ptr);
                	return (out);
		}
		memcpy(out, ptr, len);
		free(ptr);
		ptr = out;
	}
	
	return (ptr);
}

CHAR16 *
efi_devpath_name(EFI_DEVICE_PATH *devpath)
{
	EFI_STATUS status;

	if (devpath == NULL)
		return (NULL);
	if (toTextProtocol == NULL) {
		status = BS->LocateProtocol(&DevicePathToTextGUID, NULL,
		    (VOID **)&toTextProtocol);
		if (EFI_ERROR(status))
			toTextProtocol = NULL;
	}
	if (toTextProtocol == NULL)
		return (efi_devpath_to_name(devpath));

	return (toTextProtocol->ConvertDevicePathToText(devpath, TRUE, TRUE));
}

void
efi_free_devpath_name(CHAR16 *text)
{
	if (text != NULL)
		BS->FreePool(text);
}

EFI_DEVICE_PATH *
efi_name_to_devpath(const char *path)
{
	EFI_DEVICE_PATH *devpath;
	CHAR16 *uv;
	size_t ul;

	uv = NULL;
	if (utf8_to_ucs2(path, &uv, &ul) != 0)
		return (NULL);
	devpath = efi_name_to_devpath16(uv);
	free(uv);
	return (devpath);
}

EFI_DEVICE_PATH *
efi_name_to_devpath16(CHAR16 *path)
{
	EFI_STATUS status;

	if (path == NULL)
		return (NULL);
	if (fromTextProtocol == NULL) {
		status = BS->LocateProtocol(&DevicePathFromTextGUID, NULL,
		    (VOID **)&fromTextProtocol);
		if (EFI_ERROR(status))
			fromTextProtocol = NULL;
	}
	if (fromTextProtocol == NULL)
		return (NULL);

	return (fromTextProtocol->ConvertTextToDevicePath(path));
}

void efi_devpath_free(EFI_DEVICE_PATH *devpath)
{

	BS->FreePool(devpath);
}

EFI_DEVICE_PATH *
efi_devpath_last_node(EFI_DEVICE_PATH *devpath)
{

	if (IsDevicePathEnd(devpath))
		return (NULL);
	while (!IsDevicePathEnd(NextDevicePathNode(devpath)))
		devpath = NextDevicePathNode(devpath);
	return (devpath);
}

EFI_DEVICE_PATH *
efi_devpath_trim(EFI_DEVICE_PATH *devpath)
{
	EFI_DEVICE_PATH *node, *copy;
	size_t prefix, len;

	if ((node = efi_devpath_last_node(devpath)) == NULL)
		return (NULL);
	prefix = (UINT8 *)node - (UINT8 *)devpath;
	if (prefix == 0)
		return (NULL);
	len = prefix + DevicePathNodeLength(NextDevicePathNode(node));
	copy = malloc(len);
	if (copy != NULL) {
		memcpy(copy, devpath, prefix);
		node = (EFI_DEVICE_PATH *)((UINT8 *)copy + prefix);
		SetDevicePathEndNode(node);
	}
	return (copy);
}

EFI_HANDLE
efi_devpath_handle(EFI_DEVICE_PATH *devpath)
{
	EFI_STATUS status;
	EFI_HANDLE h;

	/*
	 * There isn't a standard way to locate a handle for a given
	 * device path.  However, querying the EFI_DEVICE_PATH protocol
	 * for a given device path should give us a handle for the
	 * closest node in the path to the end that is valid.
	 */
	status = BS->LocateDevicePath(&DevicePathGUID, &devpath, &h);
	if (EFI_ERROR(status))
		return (NULL);
	return (h);
}

bool
efi_devpath_match_node(EFI_DEVICE_PATH *devpath1, EFI_DEVICE_PATH *devpath2)
{
	size_t len;

	if (devpath1 == NULL || devpath2 == NULL)
		return (false);
	if (DevicePathType(devpath1) != DevicePathType(devpath2) ||
	    DevicePathSubType(devpath1) != DevicePathSubType(devpath2))
		return (false);
	len = DevicePathNodeLength(devpath1);
	if (len != DevicePathNodeLength(devpath2))
		return (false);
	if (memcmp(devpath1, devpath2, len) != 0)
		return (false);
	return (true);
}

static bool
_efi_devpath_match(EFI_DEVICE_PATH *devpath1, EFI_DEVICE_PATH *devpath2,
    bool ignore_media)
{

	if (devpath1 == NULL || devpath2 == NULL)
		return (false);

	while (true) {
		if (ignore_media &&
		    IsDevicePathType(devpath1, MEDIA_DEVICE_PATH) &&
		    IsDevicePathType(devpath2, MEDIA_DEVICE_PATH))
			return (true);
		if (!efi_devpath_match_node(devpath1, devpath2))
			return false;
		if (IsDevicePathEnd(devpath1))
			break;
		devpath1 = NextDevicePathNode(devpath1);
		devpath2 = NextDevicePathNode(devpath2);
	}
	return (true);
}
/*
 * Are two devpaths identical?
 */
bool
efi_devpath_match(EFI_DEVICE_PATH *devpath1, EFI_DEVICE_PATH *devpath2)
{
	return _efi_devpath_match(devpath1, devpath2, false);
}

/*
 * Like efi_devpath_match, but stops at when we hit the media device
 * path node that specifies the partition information. If we match
 * up to that point, then we're on the same disk.
 */
bool
efi_devpath_same_disk(EFI_DEVICE_PATH *devpath1, EFI_DEVICE_PATH *devpath2)
{
	return _efi_devpath_match(devpath1, devpath2, true);
}

bool
efi_devpath_is_prefix(EFI_DEVICE_PATH *prefix, EFI_DEVICE_PATH *path)
{
	size_t len;

	if (prefix == NULL || path == NULL)
		return (false);

	while (1) {
		if (IsDevicePathEnd(prefix))
			break;

		if (DevicePathType(prefix) != DevicePathType(path) ||
		    DevicePathSubType(prefix) != DevicePathSubType(path))
			return (false);

		len = DevicePathNodeLength(prefix);
		if (len != DevicePathNodeLength(path))
			return (false);

		if (memcmp(prefix, path, len) != 0)
			return (false);

		prefix = NextDevicePathNode(prefix);
		path = NextDevicePathNode(path);
	}
	return (true);
}

/*
 * Skip over the 'prefix' part of path and return the part of the path
 * that starts with the first node that's a MEDIA_DEVICE_PATH.
 */
EFI_DEVICE_PATH *
efi_devpath_to_media_path(EFI_DEVICE_PATH *path)
{

	while (!IsDevicePathEnd(path)) {
		if (DevicePathType(path) == MEDIA_DEVICE_PATH)
			return (path);
		path = NextDevicePathNode(path);
	}
	return (NULL);
}

UINTN
efi_devpath_length(EFI_DEVICE_PATH  *path)
{
	EFI_DEVICE_PATH *start = path;

	while (!IsDevicePathEnd(path))
		path = NextDevicePathNode(path);
	return ((UINTN)path - (UINTN)start) + DevicePathNodeLength(path);
}

EFI_HANDLE
efi_devpath_to_handle(EFI_DEVICE_PATH *path, EFI_HANDLE *handles, unsigned nhandles)
{
	unsigned i;
	EFI_DEVICE_PATH *media, *devpath;
	EFI_HANDLE h;

	media = efi_devpath_to_media_path(path);
	if (media == NULL)
		return (NULL);
	for (i = 0; i < nhandles; i++) {
		h = handles[i];
		devpath = efi_lookup_devpath(h);
		if (devpath == NULL)
			continue;
		if (!efi_devpath_match_node(media, efi_devpath_to_media_path(devpath)))
			continue;
		return (h);
	}
	return (NULL);
}
