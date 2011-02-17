/*******************************************************************************
 *
 * Module Name: rsdump - Functions to display the resource structures.
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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


#define __RSDUMP_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acresrc.h>

#define _COMPONENT          ACPI_RESOURCES
        ACPI_MODULE_NAME    ("rsdump")


#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)

/* Local prototypes */

static void
AcpiRsOutString (
    char                    *Title,
    char                    *Value);

static void
AcpiRsOutInteger8 (
    char                    *Title,
    UINT8                   Value);

static void
AcpiRsOutInteger16 (
    char                    *Title,
    UINT16                  Value);

static void
AcpiRsOutInteger32 (
    char                    *Title,
    UINT32                  Value);

static void
AcpiRsOutInteger64 (
    char                    *Title,
    UINT64                  Value);

static void
AcpiRsOutTitle (
    char                    *Title);

static void
AcpiRsDumpByteList (
    UINT16                  Length,
    UINT8                   *Data);

static void
AcpiRsDumpDwordList (
    UINT8                   Length,
    UINT32                  *Data);

static void
AcpiRsDumpShortByteList (
    UINT8                  Length,
    UINT8                  *Data);

static void
AcpiRsDumpResourceSource (
    ACPI_RESOURCE_SOURCE    *ResourceSource);

static void
AcpiRsDumpAddressCommon (
    ACPI_RESOURCE_DATA      *Resource);

static void
AcpiRsDumpDescriptor (
    void                    *Resource,
    ACPI_RSDUMP_INFO *Table);


#define ACPI_RSD_OFFSET(f)          (UINT8) ACPI_OFFSET (ACPI_RESOURCE_DATA,f)
#define ACPI_PRT_OFFSET(f)          (UINT8) ACPI_OFFSET (ACPI_PCI_ROUTING_TABLE,f)
#define ACPI_RSD_TABLE_SIZE(name)   (sizeof(name) / sizeof (ACPI_RSDUMP_INFO))


/*******************************************************************************
 *
 * Resource Descriptor info tables
 *
 * Note: The first table entry must be a Title or Literal and must contain
 * the table length (number of table entries)
 *
 ******************************************************************************/

ACPI_RSDUMP_INFO        AcpiRsDumpIrq[7] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpIrq),                "IRQ",                      NULL},
    {ACPI_RSD_UINT8 ,   ACPI_RSD_OFFSET (Irq.DescriptorLength),             "Descriptor Length",        NULL},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Irq.Triggering),                   "Triggering",               AcpiGbl_HeDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Irq.Polarity),                     "Polarity",                 AcpiGbl_LlDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Irq.Sharable),                     "Sharing",                  AcpiGbl_ShrDecode},
    {ACPI_RSD_UINT8 ,   ACPI_RSD_OFFSET (Irq.InterruptCount),               "Interrupt Count",          NULL},
    {ACPI_RSD_SHORTLIST,ACPI_RSD_OFFSET (Irq.Interrupts[0]),                "Interrupt List",           NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpDma[6] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpDma),                "DMA",                      NULL},
    {ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET (Dma.Type),                         "Speed",                    AcpiGbl_TypDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Dma.BusMaster),                    "Mastering",                AcpiGbl_BmDecode},
    {ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET (Dma.Transfer),                     "Transfer Type",            AcpiGbl_SizDecode},
    {ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (Dma.ChannelCount),                 "Channel Count",            NULL},
    {ACPI_RSD_SHORTLIST,ACPI_RSD_OFFSET (Dma.Channels[0]),                  "Channel List",             NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpStartDpf[4] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpStartDpf),           "Start-Dependent-Functions",NULL},
    {ACPI_RSD_UINT8 ,   ACPI_RSD_OFFSET (StartDpf.DescriptorLength),        "Descriptor Length",        NULL},
    {ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET (StartDpf.CompatibilityPriority),   "Compatibility Priority",   AcpiGbl_ConfigDecode},
    {ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET (StartDpf.PerformanceRobustness),   "Performance/Robustness",   AcpiGbl_ConfigDecode}
};

ACPI_RSDUMP_INFO        AcpiRsDumpEndDpf[1] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpEndDpf),             "End-Dependent-Functions",  NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpIo[6] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpIo),                 "I/O",                      NULL},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Io.IoDecode),                      "Address Decoding",         AcpiGbl_IoDecode},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Io.Minimum),                       "Address Minimum",          NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Io.Maximum),                       "Address Maximum",          NULL},
    {ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (Io.Alignment),                     "Alignment",                NULL},
    {ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (Io.AddressLength),                 "Address Length",           NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpFixedIo[3] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpFixedIo),            "Fixed I/O",                NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (FixedIo.Address),                  "Address",                  NULL},
    {ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (FixedIo.AddressLength),            "Address Length",           NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpVendor[3] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpVendor),             "Vendor Specific",          NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Vendor.ByteLength),                "Length",                   NULL},
    {ACPI_RSD_LONGLIST, ACPI_RSD_OFFSET (Vendor.ByteData[0]),               "Vendor Data",              NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpEndTag[1] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpEndTag),             "EndTag",                   NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpMemory24[6] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpMemory24),           "24-Bit Memory Range",      NULL},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Memory24.WriteProtect),            "Write Protect",            AcpiGbl_RwDecode},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Memory24.Minimum),                 "Address Minimum",          NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Memory24.Maximum),                 "Address Maximum",          NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Memory24.Alignment),               "Alignment",                NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Memory24.AddressLength),           "Address Length",           NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpMemory32[6] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpMemory32),           "32-Bit Memory Range",      NULL},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Memory32.WriteProtect),            "Write Protect",            AcpiGbl_RwDecode},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (Memory32.Minimum),                 "Address Minimum",          NULL},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (Memory32.Maximum),                 "Address Maximum",          NULL},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (Memory32.Alignment),               "Alignment",                NULL},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (Memory32.AddressLength),           "Address Length",           NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpFixedMemory32[4] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpFixedMemory32),      "32-Bit Fixed Memory Range",NULL},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (FixedMemory32.WriteProtect),       "Write Protect",            AcpiGbl_RwDecode},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (FixedMemory32.Address),            "Address",                  NULL},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (FixedMemory32.AddressLength),      "Address Length",           NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpAddress16[8] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpAddress16),          "16-Bit WORD Address Space",NULL},
    {ACPI_RSD_ADDRESS,  0,                                                  NULL,                       NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Address16.Granularity),            "Granularity",              NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Address16.Minimum),                "Address Minimum",          NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Address16.Maximum),                "Address Maximum",          NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Address16.TranslationOffset),      "Translation Offset",       NULL},
    {ACPI_RSD_UINT16,   ACPI_RSD_OFFSET (Address16.AddressLength),          "Address Length",           NULL},
    {ACPI_RSD_SOURCE,   ACPI_RSD_OFFSET (Address16.ResourceSource),         NULL,                       NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpAddress32[8] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpAddress32),         "32-Bit DWORD Address Space", NULL},
    {ACPI_RSD_ADDRESS,  0,                                                  NULL,                       NULL},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (Address32.Granularity),            "Granularity",              NULL},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (Address32.Minimum),                "Address Minimum",          NULL},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (Address32.Maximum),                "Address Maximum",          NULL},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (Address32.TranslationOffset),      "Translation Offset",       NULL},
    {ACPI_RSD_UINT32,   ACPI_RSD_OFFSET (Address32.AddressLength),          "Address Length",           NULL},
    {ACPI_RSD_SOURCE,   ACPI_RSD_OFFSET (Address32.ResourceSource),         NULL,                       NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpAddress64[8] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpAddress64),          "64-Bit QWORD Address Space", NULL},
    {ACPI_RSD_ADDRESS,  0,                                                  NULL,                       NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (Address64.Granularity),            "Granularity",              NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (Address64.Minimum),                "Address Minimum",          NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (Address64.Maximum),                "Address Maximum",          NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (Address64.TranslationOffset),      "Translation Offset",       NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (Address64.AddressLength),          "Address Length",           NULL},
    {ACPI_RSD_SOURCE,   ACPI_RSD_OFFSET (Address64.ResourceSource),         NULL,                       NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpExtAddress64[8] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpExtAddress64),       "64-Bit Extended Address Space", NULL},
    {ACPI_RSD_ADDRESS,  0,                                                  NULL,                       NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (ExtAddress64.Granularity),         "Granularity",              NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (ExtAddress64.Minimum),             "Address Minimum",          NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (ExtAddress64.Maximum),             "Address Maximum",          NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (ExtAddress64.TranslationOffset),   "Translation Offset",       NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (ExtAddress64.AddressLength),       "Address Length",           NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (ExtAddress64.TypeSpecific),        "Type-Specific Attribute",  NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpExtIrq[8] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpExtIrq),             "Extended IRQ",             NULL},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (ExtendedIrq.ProducerConsumer),     "Type",                     AcpiGbl_ConsumeDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (ExtendedIrq.Triggering),           "Triggering",               AcpiGbl_HeDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (ExtendedIrq.Polarity),             "Polarity",                 AcpiGbl_LlDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (ExtendedIrq.Sharable),             "Sharing",                  AcpiGbl_ShrDecode},
    {ACPI_RSD_SOURCE,   ACPI_RSD_OFFSET (ExtendedIrq.ResourceSource),       NULL,                       NULL},
    {ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (ExtendedIrq.InterruptCount),       "Interrupt Count",          NULL},
    {ACPI_RSD_DWORDLIST,ACPI_RSD_OFFSET (ExtendedIrq.Interrupts[0]),        "Interrupt List",           NULL}
};

ACPI_RSDUMP_INFO        AcpiRsDumpGenericReg[6] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpGenericReg),         "Generic Register",         NULL},
    {ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (GenericReg.SpaceId),               "Space ID",                 NULL},
    {ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (GenericReg.BitWidth),              "Bit Width",                NULL},
    {ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (GenericReg.BitOffset),             "Bit Offset",               NULL},
    {ACPI_RSD_UINT8,    ACPI_RSD_OFFSET (GenericReg.AccessSize),            "Access Size",              NULL},
    {ACPI_RSD_UINT64,   ACPI_RSD_OFFSET (GenericReg.Address),               "Address",                  NULL}
};


/*
 * Tables used for common address descriptor flag fields
 */
static ACPI_RSDUMP_INFO AcpiRsDumpGeneralFlags[5] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpGeneralFlags),       NULL,                       NULL},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Address.ProducerConsumer),         "Consumer/Producer",        AcpiGbl_ConsumeDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Address.Decode),                   "Address Decode",           AcpiGbl_DecDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Address.MinAddressFixed),          "Min Relocatability",       AcpiGbl_MinDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Address.MaxAddressFixed),          "Max Relocatability",       AcpiGbl_MaxDecode}
};

static ACPI_RSDUMP_INFO AcpiRsDumpMemoryFlags[5] =
{
    {ACPI_RSD_LITERAL,  ACPI_RSD_TABLE_SIZE (AcpiRsDumpMemoryFlags),        "Resource Type",            (void *) "Memory Range"},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Address.Info.Mem.WriteProtect),    "Write Protect",            AcpiGbl_RwDecode},
    {ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET (Address.Info.Mem.Caching),         "Caching",                  AcpiGbl_MemDecode},
    {ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET (Address.Info.Mem.RangeType),       "Range Type",               AcpiGbl_MtpDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Address.Info.Mem.Translation),     "Translation",              AcpiGbl_TtpDecode}
};

static ACPI_RSDUMP_INFO AcpiRsDumpIoFlags[4] =
{
    {ACPI_RSD_LITERAL,  ACPI_RSD_TABLE_SIZE (AcpiRsDumpIoFlags),            "Resource Type",            (void *) "I/O Range"},
    {ACPI_RSD_2BITFLAG, ACPI_RSD_OFFSET (Address.Info.Io.RangeType),        "Range Type",               AcpiGbl_RngDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Address.Info.Io.Translation),      "Translation",              AcpiGbl_TtpDecode},
    {ACPI_RSD_1BITFLAG, ACPI_RSD_OFFSET (Address.Info.Io.TranslationType),  "Translation Type",         AcpiGbl_TrsDecode}
};


/*
 * Table used to dump _PRT contents
 */
static ACPI_RSDUMP_INFO   AcpiRsDumpPrt[5] =
{
    {ACPI_RSD_TITLE,    ACPI_RSD_TABLE_SIZE (AcpiRsDumpPrt),                NULL,                       NULL},
    {ACPI_RSD_UINT64,   ACPI_PRT_OFFSET (Address),                          "Address",                  NULL},
    {ACPI_RSD_UINT32,   ACPI_PRT_OFFSET (Pin),                              "Pin",                      NULL},
    {ACPI_RSD_STRING,   ACPI_PRT_OFFSET (Source[0]),                        "Source",                   NULL},
    {ACPI_RSD_UINT32,   ACPI_PRT_OFFSET (SourceIndex),                      "Source Index",             NULL}
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpDescriptor
 *
 * PARAMETERS:  Resource
 *
 * RETURN:      None
 *
 * DESCRIPTION:
 *
 ******************************************************************************/

static void
AcpiRsDumpDescriptor (
    void                    *Resource,
    ACPI_RSDUMP_INFO        *Table)
{
    UINT8                   *Target = NULL;
    UINT8                   *PreviousTarget;
    char                    *Name;
    UINT8                    Count;


    /* First table entry must contain the table length (# of table entries) */

    Count = Table->Offset;

    while (Count)
    {
        PreviousTarget = Target;
        Target = ACPI_ADD_PTR (UINT8, Resource, Table->Offset);
        Name = Table->Name;

        switch (Table->Opcode)
        {
        case ACPI_RSD_TITLE:
            /*
             * Optional resource title
             */
            if (Table->Name)
            {
                AcpiOsPrintf ("%s Resource\n", Name);
            }
            break;

        /* Strings */

        case ACPI_RSD_LITERAL:
            AcpiRsOutString (Name, ACPI_CAST_PTR (char, Table->Pointer));
            break;

        case ACPI_RSD_STRING:
            AcpiRsOutString (Name, ACPI_CAST_PTR (char, Target));
            break;

        /* Data items, 8/16/32/64 bit */

        case ACPI_RSD_UINT8:
            AcpiRsOutInteger8 (Name, ACPI_GET8 (Target));
            break;

        case ACPI_RSD_UINT16:
            AcpiRsOutInteger16 (Name, ACPI_GET16 (Target));
            break;

        case ACPI_RSD_UINT32:
            AcpiRsOutInteger32 (Name, ACPI_GET32 (Target));
            break;

        case ACPI_RSD_UINT64:
            AcpiRsOutInteger64 (Name, ACPI_GET64 (Target));
            break;

        /* Flags: 1-bit and 2-bit flags supported */

        case ACPI_RSD_1BITFLAG:
            AcpiRsOutString (Name, ACPI_CAST_PTR (char,
                Table->Pointer [*Target & 0x01]));
            break;

        case ACPI_RSD_2BITFLAG:
            AcpiRsOutString (Name, ACPI_CAST_PTR (char,
                Table->Pointer [*Target & 0x03]));
            break;

        case ACPI_RSD_SHORTLIST:
            /*
             * Short byte list (single line output) for DMA and IRQ resources
             * Note: The list length is obtained from the previous table entry
             */
            if (PreviousTarget)
            {
                AcpiRsOutTitle (Name);
                AcpiRsDumpShortByteList (*PreviousTarget, Target);
            }
            break;

        case ACPI_RSD_LONGLIST:
            /*
             * Long byte list for Vendor resource data
             * Note: The list length is obtained from the previous table entry
             */
            if (PreviousTarget)
            {
                AcpiRsDumpByteList (ACPI_GET16 (PreviousTarget), Target);
            }
            break;

        case ACPI_RSD_DWORDLIST:
            /*
             * Dword list for Extended Interrupt resources
             * Note: The list length is obtained from the previous table entry
             */
            if (PreviousTarget)
            {
                AcpiRsDumpDwordList (*PreviousTarget,
                    ACPI_CAST_PTR (UINT32, Target));
            }
            break;

        case ACPI_RSD_ADDRESS:
            /*
             * Common flags for all Address resources
             */
            AcpiRsDumpAddressCommon (ACPI_CAST_PTR (ACPI_RESOURCE_DATA, Target));
            break;

        case ACPI_RSD_SOURCE:
            /*
             * Optional ResourceSource for Address resources
             */
            AcpiRsDumpResourceSource (ACPI_CAST_PTR (ACPI_RESOURCE_SOURCE, Target));
            break;

        default:
            AcpiOsPrintf ("**** Invalid table opcode [%X] ****\n",
                Table->Opcode);
            return;
        }

        Table++;
        Count--;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpResourceSource
 *
 * PARAMETERS:  ResourceSource      - Pointer to a Resource Source struct
 *
 * RETURN:      None
 *
 * DESCRIPTION: Common routine for dumping the optional ResourceSource and the
 *              corresponding ResourceSourceIndex.
 *
 ******************************************************************************/

static void
AcpiRsDumpResourceSource (
    ACPI_RESOURCE_SOURCE    *ResourceSource)
{
    ACPI_FUNCTION_ENTRY ();


    if (ResourceSource->Index == 0xFF)
    {
        return;
    }

    AcpiRsOutInteger8 ("Resource Source Index",
        ResourceSource->Index);

    AcpiRsOutString ("Resource Source",
        ResourceSource->StringPtr ?
            ResourceSource->StringPtr : "[Not Specified]");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpAddressCommon
 *
 * PARAMETERS:  Resource        - Pointer to an internal resource descriptor
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump the fields that are common to all Address resource
 *              descriptors
 *
 ******************************************************************************/

static void
AcpiRsDumpAddressCommon (
    ACPI_RESOURCE_DATA      *Resource)
{
    ACPI_FUNCTION_ENTRY ();


   /* Decode the type-specific flags */

    switch (Resource->Address.ResourceType)
    {
    case ACPI_MEMORY_RANGE:

        AcpiRsDumpDescriptor (Resource, AcpiRsDumpMemoryFlags);
        break;

    case ACPI_IO_RANGE:

        AcpiRsDumpDescriptor (Resource, AcpiRsDumpIoFlags);
        break;

    case ACPI_BUS_NUMBER_RANGE:

        AcpiRsOutString ("Resource Type", "Bus Number Range");
        break;

    default:

        AcpiRsOutInteger8 ("Resource Type",
            (UINT8) Resource->Address.ResourceType);
        break;
    }

    /* Decode the general flags */

    AcpiRsDumpDescriptor (Resource, AcpiRsDumpGeneralFlags);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpResourceList
 *
 * PARAMETERS:  ResourceList        - Pointer to a resource descriptor list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dispatches the structure to the correct dump routine.
 *
 ******************************************************************************/

void
AcpiRsDumpResourceList (
    ACPI_RESOURCE           *ResourceList)
{
    UINT32                  Count = 0;
    UINT32                  Type;


    ACPI_FUNCTION_ENTRY ();


    if (!(AcpiDbgLevel & ACPI_LV_RESOURCES) || !( _COMPONENT & AcpiDbgLayer))
    {
        return;
    }

    /* Walk list and dump all resource descriptors (END_TAG terminates) */

    do
    {
        AcpiOsPrintf ("\n[%02X] ", Count);
        Count++;

        /* Validate Type before dispatch */

        Type = ResourceList->Type;
        if (Type > ACPI_RESOURCE_TYPE_MAX)
        {
            AcpiOsPrintf (
                "Invalid descriptor type (%X) in resource list\n",
                ResourceList->Type);
            return;
        }

        /* Dump the resource descriptor */

        AcpiRsDumpDescriptor (&ResourceList->Data,
            AcpiGbl_DumpResourceDispatch[Type]);

        /* Point to the next resource structure */

        ResourceList = ACPI_ADD_PTR (ACPI_RESOURCE, ResourceList,
                            ResourceList->Length);

        /* Exit when END_TAG descriptor is reached */

    } while (Type != ACPI_RESOURCE_TYPE_END_TAG);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDumpIrqList
 *
 * PARAMETERS:  RouteTable      - Pointer to the routing table to dump.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print IRQ routing table
 *
 ******************************************************************************/

void
AcpiRsDumpIrqList (
    UINT8                   *RouteTable)
{
    ACPI_PCI_ROUTING_TABLE  *PrtElement;
    UINT8                   Count;


    ACPI_FUNCTION_ENTRY ();


    if (!(AcpiDbgLevel & ACPI_LV_RESOURCES) || !( _COMPONENT & AcpiDbgLayer))
    {
        return;
    }

    PrtElement = ACPI_CAST_PTR (ACPI_PCI_ROUTING_TABLE, RouteTable);

    /* Dump all table elements, Exit on zero length element */

    for (Count = 0; PrtElement->Length; Count++)
    {
        AcpiOsPrintf ("\n[%02X] PCI IRQ Routing Table Package\n", Count);
        AcpiRsDumpDescriptor (PrtElement, AcpiRsDumpPrt);

        PrtElement = ACPI_ADD_PTR (ACPI_PCI_ROUTING_TABLE,
                        PrtElement, PrtElement->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsOut*
 *
 * PARAMETERS:  Title       - Name of the resource field
 *              Value       - Value of the resource field
 *
 * RETURN:      None
 *
 * DESCRIPTION: Miscellaneous helper functions to consistently format the
 *              output of the resource dump routines
 *
 ******************************************************************************/

static void
AcpiRsOutString (
    char                    *Title,
    char                    *Value)
{
    AcpiOsPrintf ("%27s : %s", Title, Value);
    if (!*Value)
    {
        AcpiOsPrintf ("[NULL NAMESTRING]");
    }
    AcpiOsPrintf ("\n");
}

static void
AcpiRsOutInteger8 (
    char                    *Title,
    UINT8                   Value)
{
    AcpiOsPrintf ("%27s : %2.2X\n", Title, Value);
}

static void
AcpiRsOutInteger16 (
    char                    *Title,
    UINT16                  Value)
{
    AcpiOsPrintf ("%27s : %4.4X\n", Title, Value);
}

static void
AcpiRsOutInteger32 (
    char                    *Title,
    UINT32                  Value)
{
    AcpiOsPrintf ("%27s : %8.8X\n", Title, Value);
}

static void
AcpiRsOutInteger64 (
    char                    *Title,
    UINT64                  Value)
{
    AcpiOsPrintf ("%27s : %8.8X%8.8X\n", Title,
        ACPI_FORMAT_UINT64 (Value));
}

static void
AcpiRsOutTitle (
    char                    *Title)
{
    AcpiOsPrintf ("%27s : ", Title);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsDump*List
 *
 * PARAMETERS:  Length      - Number of elements in the list
 *              Data        - Start of the list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Miscellaneous functions to dump lists of raw data
 *
 ******************************************************************************/

static void
AcpiRsDumpByteList (
    UINT16                  Length,
    UINT8                   *Data)
{
    UINT8                   i;


    for (i = 0; i < Length; i++)
    {
        AcpiOsPrintf ("%25s%2.2X : %2.2X\n",
            "Byte", i, Data[i]);
    }
}

static void
AcpiRsDumpShortByteList (
    UINT8                  Length,
    UINT8                  *Data)
{
    UINT8                   i;


    for (i = 0; i < Length; i++)
    {
        AcpiOsPrintf ("%X ", Data[i]);
    }
    AcpiOsPrintf ("\n");
}

static void
AcpiRsDumpDwordList (
    UINT8                   Length,
    UINT32                  *Data)
{
    UINT8                   i;


    for (i = 0; i < Length; i++)
    {
        AcpiOsPrintf ("%25s%2.2X : %8.8X\n",
            "Dword", i, Data[i]);
    }
}

#endif

