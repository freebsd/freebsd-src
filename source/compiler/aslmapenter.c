/******************************************************************************
 *
 * Module Name: aslmapenter - Build resource descriptor/device maps
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

#include "acpi.h"
#include "accommon.h"
#include "acapps.h"
#include "aslcompiler.h"

/* This module used for application-level code only */

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslmapenter")

/* Local prototypes */

static ACPI_GPIO_INFO *
MpCreateGpioInfo (
    UINT16                  PinNumber,
    char                    *DeviceName);

static ACPI_SERIAL_INFO *
MpCreateSerialInfo (
    char                    *DeviceName,
    UINT16                  Address);


/*******************************************************************************
 *
 * FUNCTION:    MpSaveGpioInfo
 *
 * PARAMETERS:  Resource                - GPIO resource descriptor
 *              PinCount                - From GPIO descriptor
 *              PinList                 - From GPIO descriptor
 *              DeviceName              - The "ResourceSource" name
 *
 * RETURN:      None
 *
 * DESCRIPTION: External Interface.
 *              Save GPIO resource descriptor information.
 *              Creates new GPIO info blocks, one for each pin defined by the
 *              GPIO descriptor.
 *
 ******************************************************************************/

void
MpSaveGpioInfo (
    ACPI_PARSE_OBJECT       *Op,
    AML_RESOURCE            *Resource,
    UINT32                  PinCount,
    UINT16                  *PinList,
    char                    *DeviceName)
{
    ACPI_GPIO_INFO          *Info;
    UINT32                  i;


    /* Mapfile option enabled? */

    if (!Gbl_MapfileFlag)
    {
        return;
    }

    /* Create an info block for each pin defined in the descriptor */

    for (i = 0; i < PinCount; i++)
    {
        Info = MpCreateGpioInfo (PinList[i], DeviceName);

        Info->Op = Op;
        Info->DeviceName = DeviceName;
        Info->PinCount = PinCount;
        Info->PinIndex = i;
        Info->PinNumber = PinList[i];
        Info->Type = Resource->Gpio.ConnectionType;
        Info->Direction = (UINT8) (Resource->Gpio.IntFlags & 0x0003);       /* _IOR, for IO descriptor */
        Info->Polarity = (UINT8) ((Resource->Gpio.IntFlags >> 1) & 0x0003); /* _POL, for INT descriptor */
    }
}


/*******************************************************************************
 *
 * FUNCTION:    MpSaveSerialInfo
 *
 * PARAMETERS:  Resource                - A Serial resource descriptor
 *              DeviceName              - The "ResourceSource" name.
 *
 * RETURN:      None
 *
 * DESCRIPTION: External Interface.
 *              Save serial resource descriptor information.
 *              Creates a new serial info block.
 *
 ******************************************************************************/

void
MpSaveSerialInfo (
    ACPI_PARSE_OBJECT       *Op,
    AML_RESOURCE            *Resource,
    char                    *DeviceName)
{
    ACPI_SERIAL_INFO        *Info;
    UINT16                  Address;
    UINT32                  Speed;


    /* Mapfile option enabled? */

    if (!Gbl_MapfileFlag)
    {
        return;
    }

    if (Resource->DescriptorType != ACPI_RESOURCE_NAME_SERIAL_BUS)
    {
        return;
    }

    /* Extract address and speed from the resource descriptor */

    switch (Resource->CommonSerialBus.Type)
    {
    case AML_RESOURCE_I2C_SERIALBUSTYPE:

        Address = Resource->I2cSerialBus.SlaveAddress;
        Speed = Resource->I2cSerialBus.ConnectionSpeed;
        break;

    case AML_RESOURCE_SPI_SERIALBUSTYPE:

        Address = Resource->SpiSerialBus.DeviceSelection;
        Speed = Resource->SpiSerialBus.ConnectionSpeed;
        break;

    case AML_RESOURCE_UART_SERIALBUSTYPE:

        Address = 0;
        Speed = Resource->UartSerialBus.DefaultBaudRate;
        break;

    default:    /* Invalid bus subtype */
        return;
    }

    Info = MpCreateSerialInfo (DeviceName, Address);

    Info->Op = Op;
    Info->DeviceName = DeviceName;
    Info->Resource = Resource;
    Info->Address = Address;
    Info->Speed = Speed;
}


/*******************************************************************************
 *
 * FUNCTION:    MpCreateGpioInfo
 *
 * PARAMETERS:  PinNumber               - GPIO pin number
 *              DeviceName              - The "ResourceSource" name
 *
 * RETURN:      New GPIO info block.
 *
 * DESCRIPTION: Create a new GPIO info block and place it on the global list.
 *              The list is sorted by GPIO device names first, and pin numbers
 *              secondarily.
 *
 ******************************************************************************/

static ACPI_GPIO_INFO *
MpCreateGpioInfo (
    UINT16                  PinNumber,
    char                    *DeviceName)
{
    ACPI_GPIO_INFO          *Info;
    ACPI_GPIO_INFO          *NextGpio;
    ACPI_GPIO_INFO          *PrevGpio;
    char                    *Buffer;


    /*
     * Allocate a new info block and insert it into the global GPIO list
     * sorted by both source device name and then the pin number. There is
     * one block per pin.
     */
    Buffer = UtStringCacheCalloc (sizeof (ACPI_GPIO_INFO));
    Info = ACPI_CAST_PTR (ACPI_GPIO_INFO, Buffer);

    NextGpio = Gbl_GpioList;
    PrevGpio = NULL;
    if (!Gbl_GpioList)
    {
        Gbl_GpioList = Info;
        Info->Next = NULL;
        return (Info);
    }

    /* Sort on source DeviceName first */

    while (NextGpio &&
            (strcmp (DeviceName, NextGpio->DeviceName) > 0))
    {
        PrevGpio = NextGpio;
        NextGpio = NextGpio->Next;
    }

    /* Now sort on the PinNumber */

    while (NextGpio &&
            (NextGpio->PinNumber < PinNumber) &&
            !strcmp (DeviceName, NextGpio->DeviceName))
    {
        PrevGpio = NextGpio;
        NextGpio = NextGpio->Next;
    }

    /* Finish the list insertion */

    if (PrevGpio)
    {
        PrevGpio->Next = Info;
    }
    else
    {
        Gbl_GpioList = Info;
    }

    Info->Next = NextGpio;
    return (Info);
}


/*******************************************************************************
 *
 * FUNCTION:    MpCreateSerialInfo
 *
 * PARAMETERS:  DeviceName              - The "ResourceSource" name.
 *              Address                 - Physical address for the device
 *
 * RETURN:      New Serial info block.
 *
 * DESCRIPTION: Create a new Serial info block and place it on the global list.
 *              The list is sorted by Serial device names first, and addresses
 *              secondarily.
 *
 ******************************************************************************/

static ACPI_SERIAL_INFO *
MpCreateSerialInfo (
    char                    *DeviceName,
    UINT16                  Address)
{
    ACPI_SERIAL_INFO        *Info;
    ACPI_SERIAL_INFO        *NextSerial;
    ACPI_SERIAL_INFO        *PrevSerial;
    char                    *Buffer;


    /*
     * Allocate a new info block and insert it into the global Serial list
     * sorted by both source device name and then the address.
     */
    Buffer = UtStringCacheCalloc (sizeof (ACPI_SERIAL_INFO));
    Info = ACPI_CAST_PTR (ACPI_SERIAL_INFO, Buffer);

    NextSerial = Gbl_SerialList;
    PrevSerial = NULL;
    if (!Gbl_SerialList)
    {
        Gbl_SerialList = Info;
        Info->Next = NULL;
        return (Info);
    }

    /* Sort on source DeviceName */

    while (NextSerial &&
        (strcmp (DeviceName, NextSerial->DeviceName) > 0))
    {
        PrevSerial = NextSerial;
        NextSerial = NextSerial->Next;
    }

    /* Now sort on the Address */

    while (NextSerial &&
        (NextSerial->Address < Address) &&
        !strcmp (DeviceName, NextSerial->DeviceName))
    {
        PrevSerial = NextSerial;
        NextSerial = NextSerial->Next;
    }

    /* Finish the list insertion */

    if (PrevSerial)
    {
        PrevSerial->Next = Info;
    }
    else
    {
        Gbl_SerialList = Info;
    }

    Info->Next = NextSerial;
    return (Info);
}
