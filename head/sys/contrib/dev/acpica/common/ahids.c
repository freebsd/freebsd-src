/******************************************************************************
 *
 * Module Name: ahids - Table of ACPI/PNP _HID/_CID values
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("ahids")


/*
 * ACPI/PNP Device IDs with description strings
 */
const AH_DEVICE_ID  AslDeviceIds[] =
{
    {"10EC5640",    "Realtek I2S Audio Codec"},
    {"80860F09",    "Intel PWM Controller"},
    {"80860F0A",    "Intel Atom UART Controller"},
    {"80860F0E",    "Intel SPI Controller"},
    {"80860F14",    "Intel Baytrail SDIO/MMC Host Controller"},
    {"80860F28",    "Intel SST Audio DSP"},
    {"80860F41",    "Intel Baytrail I2C Host Controller"},
    {"ACPI0001",    "SMBus 1.0 Host Controller"},
    {"ACPI0002",    "Smart Battery Subsystem"},
    {"ACPI0003",    "Power Source Device"},
    {"ACPI0004",    "Module Device"},
    {"ACPI0005",    "SMBus 2.0 Host Controller"},
    {"ACPI0006",    "GPE Block Device"},
    {"ACPI0007",    "Processor Device"},
    {"ACPI0008",    "Ambient Light Sensor Device"},
    {"ACPI0009",    "I/O xAPIC Device"},
    {"ACPI000A",    "I/O APIC Device"},
    {"ACPI000B",    "I/O SAPIC Device"},
    {"ACPI000C",    "Processor Aggregator Device"},
    {"ACPI000D",    "Power Meter Device"},
    {"ACPI000E",    "Time and Alarm Device"},
    {"ACPI000F",    "User Presence Detection Device"},
    {"ADMA0F28",    "Intel Audio DMA"},
    {"AMCR0F28",    "Intel Audio Machine Driver"},
    {"ATK4001",     "Asus Radio Control Button"},
    {"ATML1000",    "Atmel Touchscreen Controller"},
    {"AUTH2750",    "AuthenTec AES2750"},
    {"BCM2E39",     "Broadcom BT Serial Bus Driver over UART Bus Enumerator"},
    {"BCM4752E",    "Broadcom GPS Controller"},
    {"BMG0160",     "Bosch Gyro Sensor"},
    {"CPLM3218",    "Capella Micro CM3218x Ambient Light Sensor"},
    {"DELLABCE",    "Dell Airplane Mode Switch Driver"},
    {"DLAC3002",    "Qualcomm Atheros Bluetooth UART Transport"},
    {"FTTH5506",    "FocalTech 5506 Touch Controller"},
    {"HAD0F28",     "Intel HDMI Audio Driver"},
    {"INBC0000",    "GPIO Expander"},
    {"INT0002",     "Virtual GPIO Controller"},
    {"INT0800",     "Intel 82802 Firmware Hub Device"},
    {"INT3394",     "ACPI System Fan"},
    {"INT3396",     "Standard Power Management Controller"},
    {"INT33A0",     "Intel Smart Connect Technology Device"},
    {"INT33A1",     "Intel Power Engine"},
    {"INT33BB",     "Intel Baytrail SD Host Controller"},
    {"INT33BD",     "Intel Baytrail Mailbox Device"},
    {"INT33BE",     "Camera Sensor OV5693"},
    {"INT33C0",     "Intel Serial I/O SPI Host Controller"},
    {"INT33C1",     "Intel Serial I/O SPI Host Controller"},
    {"INT33C2",     "Intel Serial I/O I2C Host Controller"},
    {"INT33C3",     "Intel Serial I/O I2C Host Controller"},
    {"INT33C4",     "Intel Serial I/O UART Host Controller"},
    {"INT33C5",     "Intel Serial I/O UART Host Controller"},
    {"INT33C6",     "Intel SD Host Controller"},
    {"INT33C7",     "Intel Serial I/O GPIO Host Controller"},
    {"INT33C8",     "Intel Smart Sound Technology Host Controller"},
    {"INT33C9",     "Wolfson Microelectronics Audio WM5102"},
    {"INT33CA",     "Intel SPB Peripheral"},
    {"INT33CB",     "Intel Smart Sound Technology Audio Codec"},
    {"INT33D1",     "Intel GPIO Buttons"},
    {"INT33D2",     "Intel GPIO Buttons"},
    {"INT33D3",     "Intel GPIO Buttons"},
    {"INT33D4",     "Intel GPIO Buttons"},
    {"INT33D6",     "Intel Virtual Buttons Device"},
    {"INT33F0",     "Camera Sensor MT9M114"},
    {"INT33F4",     "XPOWER PMIC Controller"},
    {"INT33F5",     "TI PMIC Controller"},
    {"INT33FB",     "MIPI-CSI Camera Sensor OV2722"},
    {"INT33FC",     "Intel Baytrail GPIO Controller"},
    {"INT33FD",     "Intel Baytrail Power Management IC"},
    {"INT33FE",     "XPOWER Battery Device"},
    {"INT3400",     "Intel Dynamic Power Performance Management"},
    {"INT3401",     "Intel Extended Thermal Model CPU"},
    {"INT3403",     "DPTF Temperature Sensor"},
    {"INT3406",     "Intel Dynamic Platform & Thermal Framework Display Participant"},
    {"INT3407",     "DPTF Platform Power Meter"},
    {"INT340E",     "Motherboard Resources"},
    {"INT3420",     "Intel Bluetooth RF Kill"},
    {"INT3F0D",     "ACPI Motherboard Resources"},
    {"INTCF1A",     "Sony IMX175 Camera Sensor"},
    {"INTCFD9",     "Intel Baytrail SOC GPIO Controller"},
    {"INTL9C60",    "Intel Baytrail SOC DMA Controller"},
    {"INVN6500",    "InvenSense MPU-6500 Six Axis Gyroscope and Accelerometer"},
    {"LNXCPU",      "Linux Logical CPU"},
    {"LNXPOWER",    "ACPI Power Resource (power gating)"},
    {"LNXPWRBN",    "System Power Button"},
    {"LNXSYBUS",    "System Bus"},
    {"LNXSYSTM",    "ACPI Root Node"},
    {"LNXTHERM",    "ACPI Thermal Zone"},
    {"LNXVIDEO",    "ACPI Video Controller"},
    {"MAX17047",    "Fuel Gauge Controller"},
    {"MSFT0101",    "TPM 2.0 Security Device"},
    {"NXP5442",     "NXP 5442 Near Field Communications Controller"},
    {"NXP5472",     "NXP NFC"},
    {"PNP0000",     "8259-compatible Programmable Interrupt Controller"},
    {"PNP0001",     "EISA Interrupt Controller"},
    {"PNP0002",     "MCA Interrupt Controller"},
    {"PNP0003",     "IO-APIC Interrupt Controller"},
    {"PNP0100",     "PC-class System Timer"},
    {"PNP0103",     "HPET System Timer"},
    {"PNP0200",     "PC-class DMA Controller"},
    {"PNP0300",     "IBM PC/XT Keyboard Controller (83 key)"},
    {"PNP0301",     "IBM PC/XT Keyboard Controller (86 key)"},
    {"PNP0302",     "IBM PC/XT Keyboard Controller (84 key)"},
    {"PNP0303",     "IBM Enhanced Keyboard (101/102-key, PS/2 Mouse)"},
    {"PNP0400",     "Standard LPT Parallel Port"},
    {"PNP0401",     "ECP Parallel Port"},
    {"PNP0500",     "Standard PC COM Serial Port"},
    {"PNP0501",     "16550A-compatible COM Serial Port"},
    {"PNP0510",     "Generic IRDA-compatible Device"},
    {"PNP0800",     "Microsoft Sound System Compatible Device"},
    {"PNP0A03",     "PCI Bus"},
    {"PNP0A05",     "Generic Container Device"},
    {"PNP0A06",     "Generic Container Device"},
    {"PNP0A08",     "PCI Express Bus"},
    {"PNP0B00",     "AT Real-Time Clock"},
    {"PNP0B01",     "Intel PIIX4-compatible RTC/CMOS Device"},
    {"PNP0B02",     "Dallas Semiconductor-compatible RTC/CMOS Device"},
    {"PNP0C01",     "System Board"},
    {"PNP0C02",     "PNP Motherboard Resources"},
    {"PNP0C04",     "x87-compatible Floating Point Processing Unit"},
    {"PNP0C08",     "ACPI Core Hardware"},
    {"PNP0C09",     "Embedded Controller Device"},
    {"PNP0C0A",     "Control Method Battery"},
    {"PNP0C0B",     "Fan (Thermal Solution)"},
    {"PNP0C0C",     "Power Button Device"},
    {"PNP0C0D",     "Lid Device"},
    {"PNP0C0E",     "Sleep Button Device"},
    {"PNP0C0F",     "PCI Interrupt Link Device"},
    {"PNP0C10",     "System Indicator Device"},
    {"PNP0C11",     "Thermal Zone"},
    {"PNP0C12",     "Device Bay Controller"},
    {"PNP0C14",     "Windows Management Instrumentation Device"},
    {"PNP0C15",     "Docking Station"},
    {"PNP0C40",     "Standard Button Controller"},
    {"PNP0C50",     "HID Protocol Device (I2C bus)"},
    {"PNP0C60",     "Display Sensor Device"},
    {"PNP0C70",     "Dock Sensor Device"},
    {"PNP0C80",     "Memory Device"},
    {"PNP0D10",     "XHCI USB Controller with debug"},
    {"PNP0D15",     "XHCI USB Controller without debug"},
    {"PNP0D20",     "EHCI USB Controller without debug"},
    {"PNP0D25",     "EHCI USB Controller with debug"},
    {"PNP0D40",     "SDA Standard Compliant SD Host Controller"},
    {"PNP0D80",     "Windows-compatible System Power Management Controller"},
    {"PNP0F03",     "Microsoft PS/2-style Mouse"},
    {"PNP0F13",     "PS/2 Mouse"},
    {"RTL8723",     "Realtek Wireless Controller"},
    {"SMB0349",     "Charger"},
    {"SMO91D0",     "Sensor Hub"},
    {"SMSC3750",    "SMSC 3750 USB MUX"},
    {"SSPX0000",    "Intel SSP Device"},
    {"TBQ24296",    "Charger"},

    {NULL, NULL}
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiAhMatchHardwareId
 *
 * PARAMETERS:  HardwareId          - String representation of an _HID or _CID
 *
 * RETURN:      ID info struct. NULL if HardwareId is not found
 *
 * DESCRIPTION: Lookup an _HID/_CID in the device ID table
 *
 ******************************************************************************/

const AH_DEVICE_ID *
AcpiAhMatchHardwareId (
    char                    *HardwareId)
{
    const AH_DEVICE_ID      *Info;


    for (Info = AslDeviceIds; Info->Name; Info++)
    {
        if (!strcmp (HardwareId, Info->Name))
        {
            return (Info);
        }
    }

    return (NULL);
}
