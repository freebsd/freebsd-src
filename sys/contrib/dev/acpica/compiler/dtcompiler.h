/******************************************************************************
 *
 * Module Name: dtcompiler.h - header for data table compiler
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
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

#define __DTCOMPILER_H__

#ifndef _DTCOMPILER
#define _DTCOMPILER

#include <stdio.h>
#include <contrib/dev/acpica/include/acdisasm.h>


#undef DT_EXTERN

#ifdef _DECLARE_DT_GLOBALS
#define DT_EXTERN
#define DT_INIT_GLOBAL(a,b)         (a)=(b)
#else
#define DT_EXTERN                   extern
#define DT_INIT_GLOBAL(a,b)         (a)
#endif


/* Types for individual fields (one per input line) */

#define DT_FIELD_TYPE_STRING            0
#define DT_FIELD_TYPE_INTEGER           1
#define DT_FIELD_TYPE_BUFFER            2
#define DT_FIELD_TYPE_PCI_PATH          3
#define DT_FIELD_TYPE_FLAG              4
#define DT_FIELD_TYPE_FLAGS_INTEGER     5
#define DT_FIELD_TYPE_INLINE_SUBTABLE   6


/*
 * Structure used for each individual field within an ACPI table
 */
typedef struct dt_field
{
    char                    *Name;
    char                    *Value;
    struct dt_field         *Next;
    UINT32                  Line;       /* Line number for this field */
    UINT32                  ByteOffset; /* Offset in source file for field */
    UINT32                  NameColumn; /* Start column for field name */
    UINT32                  Column;     /* Start column for field value */
    UINT8                   Flags;

} DT_FIELD;

/* Flags for above */

#define DT_FIELD_NOT_ALLOCATED      1


/*
 * Structure used for individual subtables within an ACPI table
 */
typedef struct dt_subtable
{
    struct dt_subtable      *Parent;
    struct dt_subtable      *Child;
    struct dt_subtable      *Peer;
    struct dt_subtable      *StackTop;
    UINT8                   *Buffer;
    UINT8                   *LengthField;
    UINT32                  Length;
    UINT32                  TotalLength;
    UINT32                  SizeOfLengthField;
    UINT8                   Flags;

} DT_SUBTABLE;


/*
 * Globals
 */

/* List of all field names and values from the input source */

DT_EXTERN DT_FIELD          DT_INIT_GLOBAL (*Gbl_FieldList, NULL);

/* List of all compiled tables and subtables */

DT_EXTERN DT_SUBTABLE       DT_INIT_GLOBAL (*Gbl_RootTable, NULL);

/* Stack for subtables */

DT_EXTERN DT_SUBTABLE       DT_INIT_GLOBAL (*Gbl_SubtableStack, NULL);


/* dtcompiler - main module */

ACPI_STATUS
DtCompileTable (
    DT_FIELD                **Field,
    ACPI_DMTABLE_INFO       *Info,
    DT_SUBTABLE             **RetSubtable,
    BOOLEAN                 Required);


/* dtio - binary and text input/output */

DT_FIELD *
DtScanFile (
    FILE                    *Handle);

void
DtOutputBinary (
    DT_SUBTABLE             *RootTable);


/* dtsubtable - compile subtables */

void
DtCreateSubtable (
    UINT8                   *Buffer,
    UINT32                  Length,
    DT_SUBTABLE             **RetSubtable);

UINT32
DtGetSubtableLength (
    DT_FIELD                *Field,
    ACPI_DMTABLE_INFO       *Info);

void
DtSetSubtableLength (
    DT_SUBTABLE             *Subtable);

void
DtPushSubtable (
    DT_SUBTABLE             *Subtable);

void
DtPopSubtable (
    void);

DT_SUBTABLE *
DtPeekSubtable (
    void);

void
DtInsertSubtable (
    DT_SUBTABLE             *ParentTable,
    DT_SUBTABLE             *Subtable);

DT_SUBTABLE *
DtGetNextSubtable (
    DT_SUBTABLE             *ParentTable,
    DT_SUBTABLE             *ChildTable);

DT_SUBTABLE *
DtGetParentSubtable (
    DT_SUBTABLE             *Subtable);


/* dtfield - Compile individual fields within a table */

void
DtCompileOneField (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength,
    UINT8                   Type,
    UINT8                   Flags);

void
DtCompileInteger (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  ByteLength,
    UINT8                   Flags);

UINT32
DtCompileBuffer (
    UINT8                   *Buffer,
    char                    *Value,
    DT_FIELD                *Field,
    UINT32                  ByteLength);

void
DtCompileFlag (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    ACPI_DMTABLE_INFO       *Info);


/* dtutils - Miscellaneous utilities */

typedef
void (*DT_WALK_CALLBACK) (
    DT_SUBTABLE             *Subtable,
    void                    *Context,
    void                    *ReturnValue);

void
DtWalkTableTree (
    DT_SUBTABLE             *StartTable,
    DT_WALK_CALLBACK        UserFunction,
    void                    *Context,
    void                    *ReturnValue);

void
DtError (
    UINT8                   Level,
    UINT8                   MessageId,
    DT_FIELD                *FieldObject,
    char                    *ExtraMessage);

void
DtNameError (
    UINT8                   Level,
    UINT8                   MessageId,
    DT_FIELD                *FieldObject,
    char                    *ExtraMessage);

void
DtFatal (
    UINT8                   MessageId,
    DT_FIELD                *FieldObject,
    char                    *ExtraMessage);

ACPI_STATUS
DtStrtoul64 (
    char                    *String,
    UINT64                  *ReturnInteger);

UINT32
DtGetFileSize (
    FILE                    *Handle);

char*
DtGetFieldValue (
    DT_FIELD                *Field,
    char                    *Name);

UINT8
DtGetFieldType (
    ACPI_DMTABLE_INFO       *Info);

UINT32
DtGetBufferLength (
    char                    *Buffer);

UINT32
DtGetFieldLength (
    DT_FIELD                *Field,
    ACPI_DMTABLE_INFO       *Info);

void
DtSetTableChecksum (
    UINT8                   *ChecksumPointer);

void
DtSetTableLength(
    void);

void
DtFreeFieldList (
    void);


/* dttable - individual table compilation */

ACPI_STATUS
DtCompileFacs (
    DT_FIELD                **PFieldList);

ACPI_STATUS
DtCompileRsdp (
    DT_FIELD                **PFieldList);

ACPI_STATUS
DtCompileAsf (
    void                    **PFieldList);

ACPI_STATUS
DtCompileCpep (
    void                    **PFieldList);

ACPI_STATUS
DtCompileDmar (
    void                    **PFieldList);

ACPI_STATUS
DtCompileEinj (
    void                    **PFieldList);

ACPI_STATUS
DtCompileErst (
    void                    **PFieldList);

ACPI_STATUS
DtCompileFadt (
    void                    **PFieldList);

ACPI_STATUS
DtCompileHest (
    void                    **PFieldList);

ACPI_STATUS
DtCompileIvrs (
    void                    **PFieldList);

ACPI_STATUS
DtCompileMadt (
    void                    **PFieldList);

ACPI_STATUS
DtCompileMcfg (
    void                    **PFieldList);

ACPI_STATUS
DtCompileMsct (
    void                    **PFieldList);

ACPI_STATUS
DtCompileRsdt (
    void                    **PFieldList);

ACPI_STATUS
DtCompileSlit (
    void                    **PFieldList);

ACPI_STATUS
DtCompileSrat (
    void                    **PFieldList);

ACPI_STATUS
DtCompileWdat (
    void                    **PFieldList);

ACPI_STATUS
DtCompileXsdt (
    void                    **PFieldList);

/* ACPI Table templates */

extern const unsigned char  TemplateAsf[];
extern const unsigned char  TemplateBoot[];
extern const unsigned char  TemplateBert[];
extern const unsigned char  TemplateCpep[];
extern const unsigned char  TemplateDbgp[];
extern const unsigned char  TemplateDmar[];
extern const unsigned char  TemplateEcdt[];
extern const unsigned char  TemplateEinj[];
extern const unsigned char  TemplateErst[];
extern const unsigned char  TemplateFadt[];
extern const unsigned char  TemplateHest[];
extern const unsigned char  TemplateHpet[];
extern const unsigned char  TemplateIvrs[];
extern const unsigned char  TemplateMadt[];
extern const unsigned char  TemplateMcfg[];
extern const unsigned char  TemplateMchi[];
extern const unsigned char  TemplateMsct[];
extern const unsigned char  TemplateRsdt[];
extern const unsigned char  TemplateSbst[];
extern const unsigned char  TemplateSlic[];
extern const unsigned char  TemplateSlit[];
extern const unsigned char  TemplateSpcr[];
extern const unsigned char  TemplateSpmi[];
extern const unsigned char  TemplateSrat[];
extern const unsigned char  TemplateTcpa[];
extern const unsigned char  TemplateUefi[];
extern const unsigned char  TemplateWaet[];
extern const unsigned char  TemplateWdat[];
extern const unsigned char  TemplateWddt[];
extern const unsigned char  TemplateWdrt[];
extern const unsigned char  TemplateXsdt[];

/* Debug */

#define MYDEBUG         printf

#endif
