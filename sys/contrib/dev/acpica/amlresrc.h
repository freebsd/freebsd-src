
/******************************************************************************
 *
 * Module Name: amlresrc.h - AML resource descriptors
 *              $Revision: 23 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/


#ifndef __AMLRESRC_H
#define __AMLRESRC_H


#define ASL_RESNAME_ADDRESS                     "_ADR"
#define ASL_RESNAME_ALIGNMENT                   "_ALN"
#define ASL_RESNAME_ADDRESSSPACE                "_ASI"
#define ASL_RESNAME_BASEADDRESS                 "_BAS"
#define ASL_RESNAME_BUSMASTER                   "_BM_"  /* Master(1), Slave(0) */
#define ASL_RESNAME_DECODE                      "_DEC"
#define ASL_RESNAME_DMA                         "_DMA"
#define ASL_RESNAME_DMATYPE                     "_TYP"  /* Compatible(0), A(1), B(2), F(3) */
#define ASL_RESNAME_GRANULARITY                 "_GRA"
#define ASL_RESNAME_INTERRUPT                   "_INT"
#define ASL_RESNAME_INTERRUPTLEVEL              "_LL_"  /* ActiveLo(1), ActiveHi(0) */
#define ASL_RESNAME_INTERRUPTSHARE              "_SHR"  /* Shareable(1), NoShare(0) */
#define ASL_RESNAME_INTERRUPTTYPE               "_HE_"  /* Edge(1), Level(0) */
#define ASL_RESNAME_LENGTH                      "_LEN"
#define ASL_RESNAME_MEMATTRIBUTES               "_MTP"  /* Memory(0), Reserved(1), ACPI(2), NVS(3) */
#define ASL_RESNAME_MEMTYPE                     "_MEM"  /* NonCache(0), Cacheable(1) Cache+combine(2), Cache+prefetch(3) */
#define ASL_RESNAME_MAXADDR                     "_MAX"
#define ASL_RESNAME_MINADDR                     "_MIN"
#define ASL_RESNAME_MAXTYPE                     "_MAF"
#define ASL_RESNAME_MINTYPE                     "_MIF"
#define ASL_RESNAME_REGISTERBITOFFSET           "_RBO"
#define ASL_RESNAME_REGISTERBITWIDTH            "_RBW"
#define ASL_RESNAME_RANGETYPE                   "_RNG"
#define ASL_RESNAME_READWRITETYPE               "_RW_"  /* ReadOnly(0), Writeable (1) */
#define ASL_RESNAME_TRANSLATION                 "_TRA"
#define ASL_RESNAME_TRANSTYPE                   "_TRS"  /* Sparse(1), Dense(0) */
#define ASL_RESNAME_TYPE                        "_TTP"  /* Translation(1), Static (0) */
#define ASL_RESNAME_XFERTYPE                    "_SIZ"  /* 8(0), 8And16(1), 16(2) */


/* Default sizes for "small" resource descriptors */

#define ASL_RDESC_IRQ_SIZE                      0x02
#define ASL_RDESC_DMA_SIZE                      0x02
#define ASL_RDESC_ST_DEPEND_SIZE                0x00
#define ASL_RDESC_END_DEPEND_SIZE               0x00
#define ASL_RDESC_IO_SIZE                       0x07
#define ASL_RDESC_FIXED_IO_SIZE                 0x03
#define ASL_RDESC_END_TAG_SIZE                  0x01


typedef struct asl_resource_node
{
    UINT32                      BufferLength;
    void                        *Buffer;
    struct asl_resource_node    *Next;

} ASL_RESOURCE_NODE;


/*
 * Resource descriptors defined in the ACPI specification.
 *
 * Alignment must be BYTE because these descriptors
 * are used to overlay the AML byte stream.
 */
#pragma pack(1)

typedef struct asl_irq_format_desc
{
    UINT8                       DescriptorType;
    UINT16                      IrqMask;
    UINT8                       Flags;

} ASL_IRQ_FORMAT_DESC;


typedef struct asl_irq_noflags_desc
{
    UINT8                       DescriptorType;
    UINT16                      IrqMask;

} ASL_IRQ_NOFLAGS_DESC;


typedef struct asl_dma_format_desc
{
    UINT8                       DescriptorType;
    UINT8                       DmaChannelMask;
    UINT8                       Flags;

} ASL_DMA_FORMAT_DESC;


typedef struct asl_start_dependent_desc
{
    UINT8                       DescriptorType;
    UINT8                       Flags;

} ASL_START_DEPENDENT_DESC;


typedef struct asl_start_dependent_noprio_desc
{
    UINT8                       DescriptorType;

} ASL_START_DEPENDENT_NOPRIO_DESC;


typedef struct asl_end_dependent_desc
{
    UINT8                       DescriptorType;

} ASL_END_DEPENDENT_DESC;


typedef struct asl_io_port_desc
{
    UINT8                       DescriptorType;
    UINT8                       Information;
    UINT16                      AddressMin;
    UINT16                      AddressMax;
    UINT8                       Alignment;
    UINT8                       Length;

} ASL_IO_PORT_DESC;


typedef struct asl_fixed_io_port_desc
{
    UINT8                       DescriptorType;
    UINT16                      BaseAddress;
    UINT8                       Length;

} ASL_FIXED_IO_PORT_DESC;


typedef struct asl_small_vendor_desc
{
    UINT8                       DescriptorType;
    UINT8                       VendorDefined[7];

} ASL_SMALL_VENDOR_DESC;


typedef struct asl_end_tag_desc
{
    UINT8                       DescriptorType;
    UINT8                       Checksum;

} ASL_END_TAG_DESC;


/* LARGE descriptors */

typedef struct asl_memory_24_desc
{
    UINT8                       DescriptorType;
    UINT16                      Length;
    UINT8                       Information;
    UINT16                      AddressMin;
    UINT16                      AddressMax;
    UINT16                      Alignment;
    UINT16                      RangeLength;

} ASL_MEMORY_24_DESC;


typedef struct asl_large_vendor_desc
{
    UINT8                       DescriptorType;
    UINT16                      Length;
    UINT8                       VendorDefined[1];

} ASL_LARGE_VENDOR_DESC;


typedef struct asl_memory_32_desc
{
    UINT8                       DescriptorType;
    UINT16                      Length;
    UINT8                       Information;
    UINT32                      AddressMin;
    UINT32                      AddressMax;
    UINT32                      Alignment;
    UINT32                      RangeLength;

} ASL_MEMORY_32_DESC;


typedef struct asl_fixed_memory_32_desc
{
    UINT8                       DescriptorType;
    UINT16                      Length;
    UINT8                       Information;
    UINT32                      BaseAddress;
    UINT32                      RangeLength;

} ASL_FIXED_MEMORY_32_DESC;


typedef struct asl_qword_address_desc
{
    UINT8                       DescriptorType;
    UINT16                      Length;
    UINT8                       ResourceType;
    UINT8                       Flags;
    UINT8                       SpecificFlags;
    UINT64                      Granularity;
    UINT64                      AddressMin;
    UINT64                      AddressMax;
    UINT64                      TranslationOffset;
    UINT64                      AddressLength;
    UINT8                       OptionalFields[2];

} ASL_QWORD_ADDRESS_DESC;


typedef struct asl_dword_address_desc
{
    UINT8                       DescriptorType;
    UINT16                      Length;
    UINT8                       ResourceType;
    UINT8                       Flags;
    UINT8                       SpecificFlags;
    UINT32                      Granularity;
    UINT32                      AddressMin;
    UINT32                      AddressMax;
    UINT32                      TranslationOffset;
    UINT32                      AddressLength;
    UINT8                       OptionalFields[2];

} ASL_DWORD_ADDRESS_DESC;


typedef struct asl_word_address_desc
{
    UINT8                       DescriptorType;
    UINT16                      Length;
    UINT8                       ResourceType;
    UINT8                       Flags;
    UINT8                       SpecificFlags;
    UINT16                      Granularity;
    UINT16                      AddressMin;
    UINT16                      AddressMax;
    UINT16                      TranslationOffset;
    UINT16                      AddressLength;
    UINT8                       OptionalFields[2];

} ASL_WORD_ADDRESS_DESC;


typedef struct asl_extended_xrupt_desc
{
    UINT8                       DescriptorType;
    UINT16                      Length;
    UINT8                       Flags;
    UINT8                       TableLength;
    UINT32                      InterruptNumber[1];
    /* ResSourceIndex, ResSource optional fields follow */

} ASL_EXTENDED_XRUPT_DESC;


typedef struct asl_general_register_desc
{
    UINT8                       DescriptorType;
    UINT16                      Length;
    UINT8                       AddressSpaceId;
    UINT8                       BitWidth;
    UINT8                       BitOffset;
    UINT8                       Reserved;
    UINT64                      Address;

} ASL_GENERAL_REGISTER_DESC;

/* restore default alignment */

#pragma pack()

/* Union of all resource descriptors, sow we can allocate the worst case */

typedef union asl_resource_desc
{
    ASL_IRQ_FORMAT_DESC         Irq;
    ASL_DMA_FORMAT_DESC         Dma;
    ASL_START_DEPENDENT_DESC    Std;
    ASL_END_DEPENDENT_DESC      End;
    ASL_IO_PORT_DESC            Iop;
    ASL_FIXED_IO_PORT_DESC      Fio;
    ASL_SMALL_VENDOR_DESC       Smv;
    ASL_END_TAG_DESC            Et;

    ASL_MEMORY_24_DESC          M24;
    ASL_LARGE_VENDOR_DESC       Lgv;
    ASL_MEMORY_32_DESC          M32;
    ASL_FIXED_MEMORY_32_DESC    F32;
    ASL_QWORD_ADDRESS_DESC      Qas;
    ASL_DWORD_ADDRESS_DESC      Das;
    ASL_WORD_ADDRESS_DESC       Was;
    ASL_EXTENDED_XRUPT_DESC     Exx;
    ASL_GENERAL_REGISTER_DESC   Grg;
    UINT32                      U32Item;
    UINT16                      U16Item;
    UINT8                       U8Item;

} ASL_RESOURCE_DESC;


#endif

