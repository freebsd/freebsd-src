/******************************************************************************
 *
 * Module Name: ahuuids - Table of known ACPI-related UUIDs
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("ahuuids")

/*
 * Table of "known" (ACPI-related) UUIDs
 */
const AH_UUID  AcpiUuids[] =
{
    {"PCI Host Bridge Device",
        "33db4d5b-1ff7-401c-9657-7441c03dd766"},

    {"Platform-wide Capabilities",
        "0811b06e-4a27-44f9-8d60-3cbbc22e7b48"},

    {"Dynamic Enumeration",
        "d8c1a3a6-be9b-4c9b-91bf-c3cb81fc5daf"},

    {"GPIO Controller",
        "4f248f40-d5e2-499f-834c-27758ea1cd3f"},

    {"Battery Thermal Limit",
        "4c2067e3-887d-475c-9720-4af1d3ed602e"},

    {"Thermal Extensions",
        "14d399cd-7a27-4b18-8fb4-7cb7b9f4e500"},

    {"USB Controller",
        "ce2ee385-00e6-48cb-9f05-2edb927c4899"},

    {"HID I2C Device",
        "3cdff6f7-4267-4555-ad05-b30a3d8938de"},

    {"Power Button Device",
        "dfbcf3c5-e7a5-44e6-9c1f-29c76f6e059c"},

    {"Device Labeling Interface",
        "e5c937d0-3553-4d7a-9117-ea4d19c3434d"},

    {"SATA Controller",
        "e4db149b-fcfe-425b-a6d8-92357d78fc7f"},

    {"Physical Presence Interface",
        "3dddfaa6-361b-4eb4-a424-8d10089d1653"},

    {"Device Properties for _DSD",
        "daffd814-6eba-4d8c-8a91-bc9bbf4aa301"},

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
        AcpiUtConvertStringToUuid (Info->String, UuidBuffer);

        if (!ACPI_MEMCMP (Data, UuidBuffer, UUID_BUFFER_LENGTH))
        {
            return (Info->Description);
        }
    }

    return (NULL);
}
