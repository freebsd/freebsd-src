

/******************************************************************************
 *
 * Module Name: aslglobal.h - Global variable definitions
 *              $Revision: 1.56 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2007, Intel Corp.
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


#ifndef __ASLGLOBAL_H
#define __ASLGLOBAL_H


/*
 * Global variables.  Defined in aslmain.c only, externed in all other files
 */

#undef ASL_EXTERN

#ifdef _DECLARE_GLOBALS
#define ASL_EXTERN
#define ASL_INIT_GLOBAL(a,b)        (a)=(b)
#else
#define ASL_EXTERN                  extern
#define ASL_INIT_GLOBAL(a,b)        (a)
#endif


/*
 * Parser and other externals
 */
extern int                          yydebug;
extern FILE                         *AslCompilerin;
extern int                          AslCompilerdebug;
extern const ASL_MAPPING_ENTRY      AslKeywordMapping[];
extern char                         *AslCompilertext;
extern char                         hex[];

#define ASL_LINE_BUFFER_SIZE        512
#define ASL_MSG_BUFFER_SIZE         4096
#define HEX_TABLE_LINE_SIZE         8
#define HEX_LISTING_LINE_SIZE       16


/* Source code buffers and pointers for error reporting */

ASL_EXTERN char                     Gbl_CurrentLineBuffer[ASL_LINE_BUFFER_SIZE];
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_CurrentColumn, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_CurrentLineNumber, 1);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_LogicalLineNumber, 1);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_CurrentLineOffset, 0);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*Gbl_LineBufPtr, Gbl_CurrentLineBuffer);


/* Exception reporting */

ASL_EXTERN ASL_ERROR_MSG            ASL_INIT_GLOBAL (*Gbl_ErrorLog,NULL);
ASL_EXTERN ASL_ERROR_MSG            ASL_INIT_GLOBAL (*Gbl_NextError,NULL);
extern UINT32                       Gbl_ExceptionCount[];


/* Option flags */

ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_Acpi2, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_UseDefaultAmlFilename, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_NsOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_DebugFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_AsmOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_C_OutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_AsmIncludeOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_C_IncludeOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_ListingFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_IgnoreErrors, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_SourceOutputFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_ParseOnlyFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_CompileTimesFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_FoldConstants, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_VerboseErrors, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_DisasmFlag, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_GetAllTables, FALSE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_IntegerOptimizationFlag, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_ReferenceOptimizationFlag, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_DisplayRemarks, TRUE);
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_DisplayOptimizations, FALSE);
ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (Gbl_WarningLevel, ASL_WARNING);


#define HEX_OUTPUT_NONE         0
#define HEX_OUTPUT_C            1
#define HEX_OUTPUT_ASM          2
ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_HexOutputFlag, HEX_OUTPUT_NONE);


/* Files */

ASL_EXTERN ASL_FILE_INFO            Gbl_Files [ASL_NUM_FILES];

ASL_EXTERN char                     *Gbl_DirectoryPath;
ASL_EXTERN char                     ASL_INIT_GLOBAL (*Gbl_ExternalFilename, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*Gbl_IncludeFilename, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*Gbl_OutputFilenamePrefix, NULL);
ASL_EXTERN char                     *Gbl_CurrentInputFilename;

ASL_EXTERN BOOLEAN                  ASL_INIT_GLOBAL (Gbl_HasIncludeFiles, FALSE);


/* Statistics */

ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_InputByteCount, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_NsLookupCount, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (TotalKeywords, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (TotalNamedObjects, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (TotalExecutableOpcodes, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (TotalParseNodes, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (TotalMethods, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (TotalAllocations, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (TotalAllocated, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (TotalFolds, 0);


/* Misc */

ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (Gbl_RevisionOverride, 0);
ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (Gbl_TempCount, 0);
ASL_EXTERN ACPI_PARSE_OBJECT        ASL_INIT_GLOBAL (*RootNode, NULL);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_TableLength, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_SourceLine, 0);
ASL_EXTERN ASL_LISTING_NODE         ASL_INIT_GLOBAL (*Gbl_ListingNode, NULL);
ASL_EXTERN ACPI_PARSE_OBJECT        ASL_INIT_GLOBAL (*Gbl_NodeCacheNext, NULL);
ASL_EXTERN ACPI_PARSE_OBJECT        ASL_INIT_GLOBAL (*Gbl_NodeCacheLast, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*Gbl_StringCacheNext, NULL);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*Gbl_StringCacheLast, NULL);
ASL_EXTERN ACPI_PARSE_OBJECT        *Gbl_FirstLevelInsertionNode;


ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_CurrentHexColumn, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_CurrentAmlOffset, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_CurrentLine, 0);
ASL_EXTERN UINT8                    ASL_INIT_GLOBAL (Gbl_HexBytesWereWritten, FALSE);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_NumNamespaceObjects, 0);
ASL_EXTERN UINT32                   ASL_INIT_GLOBAL (Gbl_ReservedMethods, 0);
ASL_EXTERN char                     ASL_INIT_GLOBAL (*Gbl_TableSignature, "NO_SIG");
ASL_EXTERN char                     ASL_INIT_GLOBAL (*Gbl_TableId, "NO_ID");
ASL_EXTERN FILE                     *AcpiGbl_DebugFile; /* Placeholder for oswinxf only */


/* Static structures */

ASL_EXTERN ASL_ANALYSIS_WALK_INFO   AnalysisWalkInfo;
ASL_EXTERN ACPI_TABLE_HEADER        TableHeader;
extern const ASL_RESERVED_INFO      ReservedMethods[];

/* Event timing */

#define ASL_NUM_EVENTS              19
ASL_EXTERN ASL_EVENT_INFO           AslGbl_Events[ASL_NUM_EVENTS];
ASL_EXTERN UINT8                    AslGbl_NextEvent;
ASL_EXTERN UINT8                    AslGbl_NamespaceEvent;

/* Scratch buffers */

ASL_EXTERN UINT8                    Gbl_AmlBuffer[HEX_LISTING_LINE_SIZE];
ASL_EXTERN char                     MsgBuffer[ASL_MSG_BUFFER_SIZE];
ASL_EXTERN char                     StringBuffer[ASL_MSG_BUFFER_SIZE];
ASL_EXTERN char                     StringBuffer2[ASL_MSG_BUFFER_SIZE];

#endif /* __ASLGLOBAL_H */

