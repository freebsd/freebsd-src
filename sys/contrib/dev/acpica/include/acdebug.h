/******************************************************************************
 *
 * Name: acdebug.h - ACPI/AML debugger
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

#ifndef __ACDEBUG_H__
#define __ACDEBUG_H__


#define ACPI_DEBUG_BUFFER_SIZE  0x4000      /* 16K buffer for return objects */

typedef struct CommandInfo
{
    char                    *Name;          /* Command Name */
    UINT8                   MinArgs;        /* Minimum arguments required */

} COMMAND_INFO;

typedef struct ArgumentInfo
{
    char                    *Name;          /* Argument Name */

} ARGUMENT_INFO;

typedef struct acpi_execute_walk
{
    UINT32                  Count;
    UINT32                  MaxCount;

} ACPI_EXECUTE_WALK;


#define PARAM_LIST(pl)                  pl
#define DBTEST_OUTPUT_LEVEL(lvl)        if (AcpiGbl_DbOpt_verbose)
#define VERBOSE_PRINT(fp)               DBTEST_OUTPUT_LEVEL(lvl) {\
                                            AcpiOsPrintf PARAM_LIST(fp);}

#define EX_NO_SINGLE_STEP               1
#define EX_SINGLE_STEP                  2


/*
 * dbxface - external debugger interfaces
 */
ACPI_STATUS
AcpiDbInitialize (
    void);

void
AcpiDbTerminate (
    void);

ACPI_STATUS
AcpiDbSingleStep (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  OpType);


/*
 * dbcmds - debug commands and output routines
 */
ACPI_STATUS
AcpiDbDisassembleMethod (
    char                    *Name);

void
AcpiDbDisplayTableInfo (
    char                    *TableArg);

void
AcpiDbUnloadAcpiTable (
    char                    *TableArg,
    char                    *InstanceArg);

void
AcpiDbSetMethodBreakpoint (
    char                    *Location,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDbSetMethodCallBreakpoint (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDbGetBusInfo (
    void);

void
AcpiDbDisassembleAml (
    char                    *Statements,
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDbDumpNamespace (
    char                    *StartArg,
    char                    *DepthArg);

void
AcpiDbDumpNamespaceByOwner (
    char                    *OwnerArg,
    char                    *DepthArg);

void
AcpiDbSendNotify (
    char                    *Name,
    UINT32                  Value);

void
AcpiDbSetMethodData (
    char                    *TypeArg,
    char                    *IndexArg,
    char                    *ValueArg);

ACPI_STATUS
AcpiDbDisplayObjects (
    char                    *ObjTypeArg,
    char                    *DisplayCountArg);

void
AcpiDbDisplayInterfaces (
    char                    *ActionArg,
    char                    *InterfaceNameArg);

ACPI_STATUS
AcpiDbFindNameInNamespace (
    char                    *NameArg);

void
AcpiDbSetScope (
    char                    *Name);

ACPI_STATUS
AcpiDbSleep (
    char                    *ObjectArg);

void
AcpiDbFindReferences (
    char                    *ObjectArg);

void
AcpiDbDisplayLocks (
    void);

void
AcpiDbDisplayResources (
    char                    *ObjectArg);

void
AcpiDbDisplayGpes (
    void);

void
AcpiDbCheckIntegrity (
    void);

void
AcpiDbGenerateGpe (
    char                    *GpeArg,
    char                    *BlockArg);

void
AcpiDbCheckPredefinedNames (
    void);

void
AcpiDbBatchExecute (
    char                    *CountArg);

/*
 * dbdisply - debug display commands
 */
void
AcpiDbDisplayMethodInfo (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDbDecodeAndDisplayObject (
    char                    *Target,
    char                    *OutputType);

void
AcpiDbDisplayResultObject (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDbDisplayAllMethods (
    char                    *DisplayCountArg);

void
AcpiDbDisplayArguments (
    void);

void
AcpiDbDisplayLocals (
    void);

void
AcpiDbDisplayResults (
    void);

void
AcpiDbDisplayCallingTree (
    void);

void
AcpiDbDisplayObjectType (
    char                    *ObjectArg);

void
AcpiDbDisplayArgumentObject (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState);


/*
 * dbexec - debugger control method execution
 */
void
AcpiDbExecute (
    char                    *Name,
    char                    **Args,
    UINT32                  Flags);

void
AcpiDbCreateExecutionThreads (
    char                    *NumThreadsArg,
    char                    *NumLoopsArg,
    char                    *MethodNameArg);

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
UINT32
AcpiDbGetCacheInfo (
    ACPI_MEMORY_LIST        *Cache);
#endif


/*
 * dbfileio - Debugger file I/O commands
 */
ACPI_OBJECT_TYPE
AcpiDbMatchArgument (
    char                    *UserArgument,
    ARGUMENT_INFO           *Arguments);

void
AcpiDbCloseDebugFile (
    void);

void
AcpiDbOpenDebugFile (
    char                    *Name);

ACPI_STATUS
AcpiDbLoadAcpiTable (
    char                    *Filename);

ACPI_STATUS
AcpiDbGetTableFromFile (
    char                    *Filename,
    ACPI_TABLE_HEADER       **Table);

ACPI_STATUS
AcpiDbReadTableFromFile (
    char                    *Filename,
    ACPI_TABLE_HEADER       **Table);


/*
 * dbhistry - debugger HISTORY command
 */
void
AcpiDbAddToHistory (
    char                    *CommandLine);

void
AcpiDbDisplayHistory (
    void);

char *
AcpiDbGetFromHistory (
    char                    *CommandNumArg);


/*
 * dbinput - user front-end to the AML debugger
 */
ACPI_STATUS
AcpiDbCommandDispatch (
    char                    *InputBuffer,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

void ACPI_SYSTEM_XFACE
AcpiDbExecuteThread (
    void                    *Context);

ACPI_STATUS
AcpiDbUserCommands (
    char                    Prompt,
    ACPI_PARSE_OBJECT       *Op);


/*
 * dbstats - Generation and display of ACPI table statistics
 */
void
AcpiDbGenerateStatistics (
    ACPI_PARSE_OBJECT       *Root,
    BOOLEAN                 IsMethod);

ACPI_STATUS
AcpiDbDisplayStatistics (
    char                    *TypeArg);


/*
 * dbutils - AML debugger utilities
 */
void
AcpiDbSetOutputDestination (
    UINT32                  Where);

void
AcpiDbDumpExternalObject (
    ACPI_OBJECT             *ObjDesc,
    UINT32                  Level);

void
AcpiDbPrepNamestring (
    char                    *Name);

ACPI_NAMESPACE_NODE *
AcpiDbLocalNsLookup (
    char                    *Name);

void
AcpiDbUInt32ToHexString (
    UINT32                  Value,
    char                    *Buffer);

#endif  /* __ACDEBUG_H__ */
