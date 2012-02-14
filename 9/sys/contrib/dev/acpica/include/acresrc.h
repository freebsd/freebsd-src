/******************************************************************************
 *
 * Name: acresrc.h - Resource Manager function prototypes
 *
 *****************************************************************************/

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

#ifndef __ACRESRC_H__
#define __ACRESRC_H__

/* Need the AML resource descriptor structs */

#include <contrib/dev/acpica/include/amlresrc.h>


/*
 * If possible, pack the following structures to byte alignment, since we
 * don't care about performance for debug output. Two cases where we cannot
 * pack the structures:
 *
 * 1) Hardware does not support misaligned memory transfers
 * 2) Compiler does not support pointers within packed structures
 */
#if (!defined(ACPI_MISALIGNMENT_NOT_SUPPORTED) && !defined(ACPI_PACKED_POINTERS_NOT_SUPPORTED))
#pragma pack(1)
#endif

/*
 * Individual entry for the resource conversion tables
 */
typedef const struct acpi_rsconvert_info
{
    UINT8                   Opcode;
    UINT8                   ResourceOffset;
    UINT8                   AmlOffset;
    UINT8                   Value;

} ACPI_RSCONVERT_INFO;

/* Resource conversion opcodes */

#define ACPI_RSC_INITGET                0
#define ACPI_RSC_INITSET                1
#define ACPI_RSC_FLAGINIT               2
#define ACPI_RSC_1BITFLAG               3
#define ACPI_RSC_2BITFLAG               4
#define ACPI_RSC_COUNT                  5
#define ACPI_RSC_COUNT16                6
#define ACPI_RSC_LENGTH                 7
#define ACPI_RSC_MOVE8                  8
#define ACPI_RSC_MOVE16                 9
#define ACPI_RSC_MOVE32                 10
#define ACPI_RSC_MOVE64                 11
#define ACPI_RSC_SET8                   12
#define ACPI_RSC_DATA8                  13
#define ACPI_RSC_ADDRESS                14
#define ACPI_RSC_SOURCE                 15
#define ACPI_RSC_SOURCEX                16
#define ACPI_RSC_BITMASK                17
#define ACPI_RSC_BITMASK16              18
#define ACPI_RSC_EXIT_NE                19
#define ACPI_RSC_EXIT_LE                20
#define ACPI_RSC_EXIT_EQ                21

/* Resource Conversion sub-opcodes */

#define ACPI_RSC_COMPARE_AML_LENGTH     0
#define ACPI_RSC_COMPARE_VALUE          1

#define ACPI_RSC_TABLE_SIZE(d)          (sizeof (d) / sizeof (ACPI_RSCONVERT_INFO))

#define ACPI_RS_OFFSET(f)               (UINT8) ACPI_OFFSET (ACPI_RESOURCE,f)
#define AML_OFFSET(f)                   (UINT8) ACPI_OFFSET (AML_RESOURCE,f)


typedef const struct acpi_rsdump_info
{
    UINT8                   Opcode;
    UINT8                   Offset;
    char                    *Name;
    const char              **Pointer;

} ACPI_RSDUMP_INFO;

/* Values for the Opcode field above */

#define ACPI_RSD_TITLE                  0
#define ACPI_RSD_LITERAL                1
#define ACPI_RSD_STRING                 2
#define ACPI_RSD_UINT8                  3
#define ACPI_RSD_UINT16                 4
#define ACPI_RSD_UINT32                 5
#define ACPI_RSD_UINT64                 6
#define ACPI_RSD_1BITFLAG               7
#define ACPI_RSD_2BITFLAG               8
#define ACPI_RSD_SHORTLIST              9
#define ACPI_RSD_LONGLIST               10
#define ACPI_RSD_DWORDLIST              11
#define ACPI_RSD_ADDRESS                12
#define ACPI_RSD_SOURCE                 13

/* restore default alignment */

#pragma pack()


/* Resource tables indexed by internal resource type */

extern const UINT8              AcpiGbl_AmlResourceSizes[];
extern ACPI_RSCONVERT_INFO      *AcpiGbl_SetResourceDispatch[];

/* Resource tables indexed by raw AML resource descriptor type */

extern const UINT8              AcpiGbl_ResourceStructSizes[];
extern ACPI_RSCONVERT_INFO      *AcpiGbl_GetResourceDispatch[];


typedef struct acpi_vendor_walk_info
{
    ACPI_VENDOR_UUID        *Uuid;
    ACPI_BUFFER             *Buffer;
    ACPI_STATUS             Status;

} ACPI_VENDOR_WALK_INFO;


/*
 * rscreate
 */
ACPI_STATUS
AcpiRsCreateResourceList (
    ACPI_OPERAND_OBJECT     *AmlBuffer,
    ACPI_BUFFER             *OutputBuffer);

ACPI_STATUS
AcpiRsCreateAmlResources (
    ACPI_RESOURCE           *LinkedListBuffer,
    ACPI_BUFFER             *OutputBuffer);

ACPI_STATUS
AcpiRsCreatePciRoutingTable (
    ACPI_OPERAND_OBJECT     *PackageObject,
    ACPI_BUFFER             *OutputBuffer);


/*
 * rsutils
 */
ACPI_STATUS
AcpiRsGetPrtMethodData (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiRsGetCrsMethodData (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiRsGetPrsMethodData (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiRsGetMethodData (
    ACPI_HANDLE             Handle,
    char                    *Path,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiRsSetSrsMethodData (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_BUFFER             *RetBuffer);


/*
 * rscalc
 */
ACPI_STATUS
AcpiRsGetListLength (
    UINT8                   *AmlBuffer,
    UINT32                  AmlBufferLength,
    ACPI_SIZE               *SizeNeeded);

ACPI_STATUS
AcpiRsGetAmlLength (
    ACPI_RESOURCE           *LinkedListBuffer,
    ACPI_SIZE               *SizeNeeded);

ACPI_STATUS
AcpiRsGetPciRoutingTableLength (
    ACPI_OPERAND_OBJECT     *PackageObject,
    ACPI_SIZE               *BufferSizeNeeded);

ACPI_STATUS
AcpiRsConvertAmlToResources (
    UINT8                   *Aml,
    UINT32                  Length,
    UINT32                  Offset,
    UINT8                   ResourceIndex,
    void                    *Context);

ACPI_STATUS
AcpiRsConvertResourcesToAml (
    ACPI_RESOURCE           *Resource,
    ACPI_SIZE               AmlSizeNeeded,
    UINT8                   *OutputBuffer);


/*
 * rsaddr
 */
void
AcpiRsSetAddressCommon (
    AML_RESOURCE            *Aml,
    ACPI_RESOURCE           *Resource);

BOOLEAN
AcpiRsGetAddressCommon (
    ACPI_RESOURCE           *Resource,
    AML_RESOURCE            *Aml);


/*
 * rsmisc
 */
ACPI_STATUS
AcpiRsConvertAmlToResource (
    ACPI_RESOURCE           *Resource,
    AML_RESOURCE            *Aml,
    ACPI_RSCONVERT_INFO     *Info);

ACPI_STATUS
AcpiRsConvertResourceToAml (
    ACPI_RESOURCE           *Resource,
    AML_RESOURCE            *Aml,
    ACPI_RSCONVERT_INFO     *Info);


/*
 * rsutils
 */
void
AcpiRsMoveData (
    void                    *Destination,
    void                    *Source,
    UINT16                  ItemCount,
    UINT8                   MoveType);

UINT8
AcpiRsDecodeBitmask (
    UINT16                  Mask,
    UINT8                   *List);

UINT16
AcpiRsEncodeBitmask (
    UINT8                   *List,
    UINT8                   Count);

ACPI_RS_LENGTH
AcpiRsGetResourceSource (
    ACPI_RS_LENGTH          ResourceLength,
    ACPI_RS_LENGTH          MinimumLength,
    ACPI_RESOURCE_SOURCE    *ResourceSource,
    AML_RESOURCE            *Aml,
    char                    *StringPtr);

ACPI_RSDESC_SIZE
AcpiRsSetResourceSource (
    AML_RESOURCE            *Aml,
    ACPI_RS_LENGTH          MinimumLength,
    ACPI_RESOURCE_SOURCE    *ResourceSource);

void
AcpiRsSetResourceHeader (
    UINT8                   DescriptorType,
    ACPI_RSDESC_SIZE        TotalLength,
    AML_RESOURCE            *Aml);

void
AcpiRsSetResourceLength (
    ACPI_RSDESC_SIZE        TotalLength,
    AML_RESOURCE            *Aml);


/*
 * rsdump
 */
void
AcpiRsDumpResourceList (
    ACPI_RESOURCE           *Resource);

void
AcpiRsDumpIrqList (
    UINT8                   *RouteTable);


/*
 * Resource conversion tables
 */
extern ACPI_RSCONVERT_INFO      AcpiRsConvertDma[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertEndDpf[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertIo[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertFixedIo[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertEndTag[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertMemory24[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertGenericReg[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertMemory32[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertFixedMemory32[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertAddress32[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertAddress16[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertExtIrq[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertAddress64[];
extern ACPI_RSCONVERT_INFO      AcpiRsConvertExtAddress64[];

/* These resources require separate get/set tables */

extern ACPI_RSCONVERT_INFO      AcpiRsGetIrq[];
extern ACPI_RSCONVERT_INFO      AcpiRsGetStartDpf[];
extern ACPI_RSCONVERT_INFO      AcpiRsGetVendorSmall[];
extern ACPI_RSCONVERT_INFO      AcpiRsGetVendorLarge[];

extern ACPI_RSCONVERT_INFO      AcpiRsSetIrq[];
extern ACPI_RSCONVERT_INFO      AcpiRsSetStartDpf[];
extern ACPI_RSCONVERT_INFO      AcpiRsSetVendor[];


#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)
/*
 * rsinfo
 */
extern ACPI_RSDUMP_INFO         *AcpiGbl_DumpResourceDispatch[];

/*
 * rsdump
 */
extern ACPI_RSDUMP_INFO         AcpiRsDumpIrq[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpDma[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpStartDpf[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpEndDpf[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpIo[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpFixedIo[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpVendor[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpEndTag[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpMemory24[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpMemory32[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpFixedMemory32[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpAddress16[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpAddress32[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpAddress64[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpExtAddress64[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpExtIrq[];
extern ACPI_RSDUMP_INFO         AcpiRsDumpGenericReg[];
#endif

#endif  /* __ACRESRC_H__ */
