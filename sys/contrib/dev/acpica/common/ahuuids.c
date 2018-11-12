/******************************************************************************
 *
 * Module Name: ahuuids - Table of known ACPI-related UUIDs
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acuuid.h>

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("ahuuids")


/*
 * Table of "known" (ACPI-related) UUIDs
 */
const AH_UUID  AcpiUuids[] =
{
    {"[Controllers]",               NULL},
    {"GPIO Controller",             UUID_GPIO_CONTROLLER},
    {"USB Controller",              UUID_USB_CONTROLLER},
    {"SATA Controller",             UUID_SATA_CONTROLLER},

    {"[Devices]",                   NULL},
    {"PCI Host Bridge Device",      UUID_PCI_HOST_BRIDGE},
    {"HID I2C Device",              UUID_I2C_DEVICE},
    {"Power Button Device",         UUID_POWER_BUTTON},

    {"[Interfaces]",                NULL},
    {"Device Labeling Interface",   UUID_DEVICE_LABELING},
    {"Physical Presence Interface", UUID_PHYSICAL_PRESENCE},

    {"[Non-volatile DIMM and NFIT table]",       NULL},
    {"Volatile Memory Region",      UUID_VOLATILE_MEMORY},
    {"Persistent Memory Region",    UUID_PERSISTENT_MEMORY},
    {"NVDIMM Control Region",       UUID_CONTROL_REGION},
    {"NVDIMM Data Region",          UUID_DATA_REGION},
    {"Volatile Virtual Disk",       UUID_VOLATILE_VIRTUAL_DISK},
    {"Volatile Virtual CD",         UUID_VOLATILE_VIRTUAL_CD},
    {"Persistent Virtual Disk",     UUID_PERSISTENT_VIRTUAL_DISK},
    {"Persistent Virtual CD",       UUID_PERSISTENT_VIRTUAL_CD},

    {"[Miscellaneous]",             NULL},
    {"Platform-wide Capabilities",  UUID_PLATFORM_CAPABILITIES},
    {"Dynamic Enumeration",         UUID_DYNAMIC_ENUMERATION},
    {"Battery Thermal Limit",       UUID_BATTERY_THERMAL_LIMIT},
    {"Thermal Extensions",          UUID_THERMAL_EXTENSIONS},
    {"Device Properties for _DSD",  UUID_DEVICE_PROPERTIES},

    {NULL, NULL}
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiAhMatchUuid
 *
 * PARAMETERS:  Data                - Data buffer containing a UUID
 *
 * RETURN:      ASCII description string for the UUID if it is found.
 *
 * DESCRIPTION: Returns a description string for "known" UUIDs, which are
 *              are UUIDs that are related to ACPI in some way.
 *
 ******************************************************************************/

const char *
AcpiAhMatchUuid (
    UINT8                   *Data)
{
    const AH_UUID           *Info;
    UINT8                   UuidBuffer[UUID_BUFFER_LENGTH];


    /* Walk the table of known ACPI-related UUIDs */

    for (Info = AcpiUuids; Info->Description; Info++)
    {
        /* Null string means desciption is a UUID class */

        if (!Info->String)
        {
            continue;
        }

        AcpiUtConvertStringToUuid (Info->String, UuidBuffer);

        if (!memcmp (Data, UuidBuffer, UUID_BUFFER_LENGTH))
        {
            return (Info->Description);
        }
    }

    return (NULL);
}
