/*******************************************************************************
 *
 * Module Name: dbinput - user front-end to the AML debugger
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


#include "acpi.h"
#include "accommon.h"
#include "acdebug.h"


#ifdef ACPI_DEBUGGER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbinput")

/* Local prototypes */

static UINT32
AcpiDbGetLine (
    char                    *InputBuffer);

static UINT32
AcpiDbMatchCommand (
    char                    *UserCommand);

static void
AcpiDbSingleThread (
    void);

static void
AcpiDbDisplayHelp (
    void);


/*
 * Top-level debugger commands.
 *
 * This list of commands must match the string table below it
 */
enum AcpiExDebuggerCommands
{
    CMD_NOT_FOUND = 0,
    CMD_NULL,
    CMD_ALLOCATIONS,
    CMD_ARGS,
    CMD_ARGUMENTS,
    CMD_BATCH,
    CMD_BREAKPOINT,
    CMD_BUSINFO,
    CMD_CALL,
    CMD_CLOSE,
    CMD_DEBUG,
    CMD_DISASSEMBLE,
    CMD_DUMP,
    CMD_ENABLEACPI,
    CMD_EVENT,
    CMD_EXECUTE,
    CMD_EXIT,
    CMD_FIND,
    CMD_GO,
    CMD_GPE,
    CMD_GPES,
    CMD_HANDLERS,
    CMD_HELP,
    CMD_HELP2,
    CMD_HISTORY,
    CMD_HISTORY_EXE,
    CMD_HISTORY_LAST,
    CMD_INFORMATION,
    CMD_INTEGRITY,
    CMD_INTO,
    CMD_LEVEL,
    CMD_LIST,
    CMD_LOAD,
    CMD_LOCALS,
    CMD_LOCKS,
    CMD_METHODS,
    CMD_NAMESPACE,
    CMD_NOTIFY,
    CMD_OBJECT,
    CMD_OPEN,
    CMD_OSI,
    CMD_OWNER,
    CMD_PREDEFINED,
    CMD_PREFIX,
    CMD_QUIT,
    CMD_REFERENCES,
    CMD_RESOURCES,
    CMD_RESULTS,
    CMD_SET,
    CMD_SLEEP,
    CMD_STATS,
    CMD_STOP,
    CMD_TABLES,
    CMD_TERMINATE,
    CMD_THREADS,
    CMD_TRACE,
    CMD_TREE,
    CMD_TYPE,
    CMD_UNLOAD
};

#define CMD_FIRST_VALID     2


/* Second parameter is the required argument count */

static const COMMAND_INFO       AcpiGbl_DbCommands[] =
{
    {"<NOT FOUND>",  0},
    {"<NULL>",       0},
    {"ALLOCATIONS",  0},
    {"ARGS",         0},
    {"ARGUMENTS",    0},
    {"BATCH",        0},
    {"BREAKPOINT",   1},
    {"BUSINFO",      0},
    {"CALL",         0},
    {"CLOSE",        0},
    {"DEBUG",        1},
    {"DISASSEMBLE",  1},
    {"DUMP",         1},
    {"ENABLEACPI",   0},
    {"EVENT",        1},
    {"EXECUTE",      1},
    {"EXIT",         0},
    {"FIND",         1},
    {"GO",           0},
    {"GPE",          2},
    {"GPES",         0},
    {"HANDLERS",     0},
    {"HELP",         0},
    {"?",            0},
    {"HISTORY",      0},
    {"!",            1},
    {"!!",           0},
    {"INFORMATION",  0},
    {"INTEGRITY",    0},
    {"INTO",         0},
    {"LEVEL",        0},
    {"LIST",         0},
    {"LOAD",         1},
    {"LOCALS",       0},
    {"LOCKS",        0},
    {"METHODS",      0},
    {"NAMESPACE",    0},
    {"NOTIFY",       2},
    {"OBJECT",       1},
    {"OPEN",         1},
    {"OSI",          0},
    {"OWNER",        1},
    {"PREDEFINED",   0},
    {"PREFIX",       0},
    {"QUIT",         0},
    {"REFERENCES",   1},
    {"RESOURCES",    1},
    {"RESULTS",      0},
    {"SET",          3},
    {"SLEEP",        1},
    {"STATS",        0},
    {"STOP",         0},
    {"TABLES",       0},
    {"TERMINATE",    0},
    {"THREADS",      3},
    {"TRACE",        1},
    {"TREE",         0},
    {"TYPE",         1},
    {"UNLOAD",       1},
    {NULL,           0}
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayHelp
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print a usage message.
 *
 ******************************************************************************/

static void
AcpiDbDisplayHelp (
    void)
{

    AcpiOsPrintf ("\nGeneral-Purpose Commands:\n");
    AcpiOsPrintf ("  Allocations                         Display list of current memory allocations\n");
    AcpiOsPrintf ("  Dump <Address>|<Namepath>\n");
    AcpiOsPrintf ("       [Byte|Word|Dword|Qword]        Display ACPI objects or memory\n");
    AcpiOsPrintf ("  EnableAcpi                          Enable ACPI (hardware) mode\n");
    AcpiOsPrintf ("  Handlers                            Info about global handlers\n");
    AcpiOsPrintf ("  Help                                This help screen\n");
    AcpiOsPrintf ("  History                             Display command history buffer\n");
    AcpiOsPrintf ("  Level [<DebugLevel>] [console]      Get/Set debug level for file or console\n");
    AcpiOsPrintf ("  Locks                               Current status of internal mutexes\n");
    AcpiOsPrintf ("  Osi [Install|Remove <name>]         Display or modify global _OSI list\n");
    AcpiOsPrintf ("  Quit or Exit                        Exit this command\n");
    AcpiOsPrintf ("  Stats [Allocations|Memory|Misc|\n");
    AcpiOsPrintf ("        Objects|Sizes|Stack|Tables]   Display namespace and memory statistics\n");
    AcpiOsPrintf ("     Allocations                      Display list of current memory allocations\n");
    AcpiOsPrintf ("     Memory                           Dump internal memory lists\n");
    AcpiOsPrintf ("     Misc                             Namespace search and mutex stats\n");
    AcpiOsPrintf ("     Objects                          Summary of namespace objects\n");
    AcpiOsPrintf ("     Sizes                            Sizes for each of the internal objects\n");
    AcpiOsPrintf ("     Stack                            Display CPU stack usage\n");
    AcpiOsPrintf ("     Tables                           Info about current ACPI table(s)\n");
    AcpiOsPrintf ("  Tables                              Display info about loaded ACPI tables\n");
    AcpiOsPrintf ("  Unload <TableSig> [Instance]        Unload an ACPI table\n");
    AcpiOsPrintf ("  ! <CommandNumber>                   Execute command from history buffer\n");
    AcpiOsPrintf ("  !!                                  Execute last command again\n");

    AcpiOsPrintf ("\nNamespace Access Commands:\n");
    AcpiOsPrintf ("  Businfo                             Display system bus info\n");
    AcpiOsPrintf ("  Disassemble <Method>                Disassemble a control method\n");
    AcpiOsPrintf ("  Event <F|G> <Value>                 Generate AcpiEvent (Fixed/GPE)\n");
    AcpiOsPrintf ("  Find <AcpiName>  (? is wildcard)    Find ACPI name(s) with wildcards\n");
    AcpiOsPrintf ("  Gpe <GpeNum> <GpeBlock>             Simulate a GPE\n");
    AcpiOsPrintf ("  Gpes                                Display info on all GPEs\n");
    AcpiOsPrintf ("  Integrity                           Validate namespace integrity\n");
    AcpiOsPrintf ("  Methods                             Display list of loaded control methods\n");
    AcpiOsPrintf ("  Namespace [Object] [Depth]          Display loaded namespace tree/subtree\n");
    AcpiOsPrintf ("  Notify <Object> <Value>             Send a notification on Object\n");
    AcpiOsPrintf ("  Objects <ObjectType>                Display all objects of the given type\n");
    AcpiOsPrintf ("  Owner <OwnerId> [Depth]             Display loaded namespace by object owner\n");
    AcpiOsPrintf ("  Predefined                          Check all predefined names\n");
    AcpiOsPrintf ("  Prefix [<NamePath>]                 Set or Get current execution prefix\n");
    AcpiOsPrintf ("  References <Addr>                   Find all references to object at addr\n");
    AcpiOsPrintf ("  Resources <Device>                  Get and display Device resources\n");
    AcpiOsPrintf ("  Set N <NamedObject> <Value>         Set value for named integer\n");
    AcpiOsPrintf ("  Sleep <SleepState>                  Simulate sleep/wake sequence\n");
    AcpiOsPrintf ("  Terminate                           Delete namespace and all internal objects\n");
    AcpiOsPrintf ("  Type <Object>                       Display object type\n");

    AcpiOsPrintf ("\nControl Method Execution Commands:\n");
    AcpiOsPrintf ("  Arguments (or Args)                 Display method arguments\n");
    AcpiOsPrintf ("  Breakpoint <AmlOffset>              Set an AML execution breakpoint\n");
    AcpiOsPrintf ("  Call                                Run to next control method invocation\n");
    AcpiOsPrintf ("  Debug <Namepath> [Arguments]        Single Step a control method\n");
    AcpiOsPrintf ("  Execute <Namepath> [Arguments]      Execute control method\n");
    AcpiOsPrintf ("     Hex Integer                      Integer method argument\n");
    AcpiOsPrintf ("     \"Ascii String\"                   String method argument\n");
    AcpiOsPrintf ("     (Byte List)                      Buffer method argument\n");
    AcpiOsPrintf ("     [Package Element List]           Package method argument\n");
    AcpiOsPrintf ("  Go                                  Allow method to run to completion\n");
    AcpiOsPrintf ("  Information                         Display info about the current method\n");
    AcpiOsPrintf ("  Into                                Step into (not over) a method call\n");
    AcpiOsPrintf ("  List [# of Aml Opcodes]             Display method ASL statements\n");
    AcpiOsPrintf ("  Locals                              Display method local variables\n");
    AcpiOsPrintf ("  Results                             Display method result stack\n");
    AcpiOsPrintf ("  Set <A|L> <#> <Value>               Set method data (Arguments/Locals)\n");
    AcpiOsPrintf ("  Stop                                Terminate control method\n");
    AcpiOsPrintf ("  Thread <Threads><Loops><NamePath>   Spawn threads to execute method(s)\n");
    AcpiOsPrintf ("  Trace <method name>                 Trace method execution\n");
    AcpiOsPrintf ("  Tree                                Display control method calling tree\n");
    AcpiOsPrintf ("  <Enter>                             Single step next AML opcode (over calls)\n");

    AcpiOsPrintf ("\nFile I/O Commands:\n");
    AcpiOsPrintf ("  Close                               Close debug output file\n");
    AcpiOsPrintf ("  Load <Input Filename>               Load ACPI table from a file\n");
    AcpiOsPrintf ("  Open <Output Filename>              Open a file for debug output\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGetNextToken
 *
 * PARAMETERS:  String          - Command buffer
 *              Next            - Return value, end of next token
 *
 * RETURN:      Pointer to the start of the next token.
 *
 * DESCRIPTION: Command line parsing.  Get the next token on the command line
 *
 ******************************************************************************/

char *
AcpiDbGetNextToken (
    char                    *String,
    char                    **Next,
    ACPI_OBJECT_TYPE        *ReturnType)
{
    char                    *Start;
    UINT32                  Depth;
    ACPI_OBJECT_TYPE        Type = ACPI_TYPE_INTEGER;


    /* At end of buffer? */

    if (!String || !(*String))
    {
        return (NULL);
    }

    /* Remove any spaces at the beginning */

    if (*String == ' ')
    {
        while (*String && (*String == ' '))
        {
            String++;
        }

        if (!(*String))
        {
            return (NULL);
        }
    }

    switch (*String)
    {
    case '"':

        /* This is a quoted string, scan until closing quote */

        String++;
        Start = String;
        Type = ACPI_TYPE_STRING;

        /* Find end of string */

        while (*String && (*String != '"'))
        {
            String++;
        }
        break;

    case '(':

        /* This is the start of a buffer, scan until closing paren */

        String++;
        Start = String;
        Type = ACPI_TYPE_BUFFER;

        /* Find end of buffer */

        while (*String && (*String != ')'))
        {
            String++;
        }
        break;

    case '[':

        /* This is the start of a package, scan until closing bracket */

        String++;
        Depth = 1;
        Start = String;
        Type = ACPI_TYPE_PACKAGE;

        /* Find end of package (closing bracket) */

        while (*String)
        {
            /* Handle String package elements */

            if (*String == '"')
            {
                /* Find end of string */

                String++;
                while (*String && (*String != '"'))
                {
                    String++;
                }
                if (!(*String))
                {
                    break;
                }
            }
            else if (*String == '[')
            {
                Depth++;         /* A nested package declaration */
            }
            else if (*String == ']')
            {
                Depth--;
                if (Depth == 0) /* Found final package closing bracket */
                {
                    break;
                }
            }

            String++;
        }
        break;

    default:

        Start = String;

        /* Find end of token */

        while (*String && (*String != ' '))
        {
            String++;
        }
        break;
    }

    if (!(*String))
    {
        *Next = NULL;
    }
    else
    {
        *String = 0;
        *Next = String + 1;
    }

    *ReturnType = Type;
    return (Start);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGetLine
 *
 * PARAMETERS:  InputBuffer         - Command line buffer
 *
 * RETURN:      Count of arguments to the command
 *
 * DESCRIPTION: Get the next command line from the user.  Gets entire line
 *              up to the next newline
 *
 ******************************************************************************/

static UINT32
AcpiDbGetLine (
    char                    *InputBuffer)
{
    UINT32                  i;
    UINT32                  Count;
    char                    *Next;
    char                    *This;


    ACPI_STRCPY (AcpiGbl_DbParsedBuf, InputBuffer);

    This = AcpiGbl_DbParsedBuf;
    for (i = 0; i < ACPI_DEBUGGER_MAX_ARGS; i++)
    {
        AcpiGbl_DbArgs[i] = AcpiDbGetNextToken (This, &Next,
            &AcpiGbl_DbArgTypes[i]);
        if (!AcpiGbl_DbArgs[i])
        {
            break;
        }

        This = Next;
    }

    /* Uppercase the actual command */

    if (AcpiGbl_DbArgs[0])
    {
        AcpiUtStrupr (AcpiGbl_DbArgs[0]);
    }

    Count = i;
    if (Count)
    {
        Count--;  /* Number of args only */
    }

    return (Count);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbMatchCommand
 *
 * PARAMETERS:  UserCommand             - User command line
 *
 * RETURN:      Index into command array, -1 if not found
 *
 * DESCRIPTION: Search command array for a command match
 *
 ******************************************************************************/

static UINT32
AcpiDbMatchCommand (
    char                    *UserCommand)
{
    UINT32                  i;


    if (!UserCommand || UserCommand[0] == 0)
    {
        return (CMD_NULL);
    }

    for (i = CMD_FIRST_VALID; AcpiGbl_DbCommands[i].Name; i++)
    {
        if (ACPI_STRSTR (AcpiGbl_DbCommands[i].Name, UserCommand) ==
                         AcpiGbl_DbCommands[i].Name)
        {
            return (i);
        }
    }

    /* Command not recognized */

    return (CMD_NOT_FOUND);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCommandDispatch
 *
 * PARAMETERS:  InputBuffer         - Command line buffer
 *              WalkState           - Current walk
 *              Op                  - Current (executing) parse op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Command dispatcher.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbCommandDispatch (
    char                    *InputBuffer,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    UINT32                  Temp;
    UINT32                  CommandIndex;
    UINT32                  ParamCount;
    char                    *CommandLine;
    ACPI_STATUS             Status = AE_CTRL_TRUE;


    /* If AcpiTerminate has been called, terminate this thread */

    if (AcpiGbl_DbTerminateThreads)
    {
        return (AE_CTRL_TERMINATE);
    }

    ParamCount = AcpiDbGetLine (InputBuffer);
    CommandIndex = AcpiDbMatchCommand (AcpiGbl_DbArgs[0]);
    Temp = 0;

    /* Verify that we have the minimum number of params */

    if (ParamCount < AcpiGbl_DbCommands[CommandIndex].MinArgs)
    {
        AcpiOsPrintf ("%u parameters entered, [%s] requires %u parameters\n",
            ParamCount, AcpiGbl_DbCommands[CommandIndex].Name,
            AcpiGbl_DbCommands[CommandIndex].MinArgs);

        return (AE_CTRL_TRUE);
    }

    /* Decode and dispatch the command */

    switch (CommandIndex)
    {
    case CMD_NULL:
        if (Op)
        {
            return (AE_OK);
        }
        break;

    case CMD_ALLOCATIONS:

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
        AcpiUtDumpAllocations ((UINT32) -1, NULL);
#endif
        break;

    case CMD_ARGS:
    case CMD_ARGUMENTS:
        AcpiDbDisplayArguments ();
        break;

    case CMD_BATCH:
        AcpiDbBatchExecute (AcpiGbl_DbArgs[1]);
        break;

    case CMD_BREAKPOINT:
        AcpiDbSetMethodBreakpoint (AcpiGbl_DbArgs[1], WalkState, Op);
        break;

    case CMD_BUSINFO:
        AcpiDbGetBusInfo ();
        break;

    case CMD_CALL:
        AcpiDbSetMethodCallBreakpoint (Op);
        Status = AE_OK;
        break;

    case CMD_CLOSE:
        AcpiDbCloseDebugFile ();
        break;

    case CMD_DEBUG:
        AcpiDbExecute (AcpiGbl_DbArgs[1],
            &AcpiGbl_DbArgs[2], &AcpiGbl_DbArgTypes[2], EX_SINGLE_STEP);
        break;

    case CMD_DISASSEMBLE:
        (void) AcpiDbDisassembleMethod (AcpiGbl_DbArgs[1]);
        break;

    case CMD_DUMP:
        AcpiDbDecodeAndDisplayObject (AcpiGbl_DbArgs[1], AcpiGbl_DbArgs[2]);
        break;

    case CMD_ENABLEACPI:
        Status = AcpiEnable();
        if (ACPI_FAILURE(Status))
        {
            AcpiOsPrintf("AcpiEnable failed (Status=%X)\n", Status);
            return (Status);
        }
        break;

    case CMD_EVENT:
        AcpiOsPrintf ("Event command not implemented\n");
        break;

    case CMD_EXECUTE:
        AcpiDbExecute (AcpiGbl_DbArgs[1],
            &AcpiGbl_DbArgs[2], &AcpiGbl_DbArgTypes[2], EX_NO_SINGLE_STEP);
        break;

    case CMD_FIND:
        Status = AcpiDbFindNameInNamespace (AcpiGbl_DbArgs[1]);
        break;

    case CMD_GO:
        AcpiGbl_CmSingleStep = FALSE;
        return (AE_OK);

    case CMD_GPE:
        AcpiDbGenerateGpe (AcpiGbl_DbArgs[1], AcpiGbl_DbArgs[2]);
        break;

    case CMD_GPES:
        AcpiDbDisplayGpes ();
        break;

    case CMD_HANDLERS:
        AcpiDbDisplayHandlers ();
        break;

    case CMD_HELP:
    case CMD_HELP2:
        AcpiDbDisplayHelp ();
        break;

    case CMD_HISTORY:
        AcpiDbDisplayHistory ();
        break;

    case CMD_HISTORY_EXE:
        CommandLine = AcpiDbGetFromHistory (AcpiGbl_DbArgs[1]);
        if (!CommandLine)
        {
            return (AE_CTRL_TRUE);
        }

        Status = AcpiDbCommandDispatch (CommandLine, WalkState, Op);
        return (Status);

    case CMD_HISTORY_LAST:
        CommandLine = AcpiDbGetFromHistory (NULL);
        if (!CommandLine)
        {
            return (AE_CTRL_TRUE);
        }

        Status = AcpiDbCommandDispatch (CommandLine, WalkState, Op);
        return (Status);

    case CMD_INFORMATION:
        AcpiDbDisplayMethodInfo (Op);
        break;

    case CMD_INTEGRITY:
        AcpiDbCheckIntegrity ();
        break;

    case CMD_INTO:
        if (Op)
        {
            AcpiGbl_CmSingleStep = TRUE;
            return (AE_OK);
        }
        break;

    case CMD_LEVEL:
        if (ParamCount == 0)
        {
            AcpiOsPrintf ("Current debug level for file output is:    %8.8lX\n",
                AcpiGbl_DbDebugLevel);
            AcpiOsPrintf ("Current debug level for console output is: %8.8lX\n",
                AcpiGbl_DbConsoleDebugLevel);
        }
        else if (ParamCount == 2)
        {
            Temp = AcpiGbl_DbConsoleDebugLevel;
            AcpiGbl_DbConsoleDebugLevel = ACPI_STRTOUL (AcpiGbl_DbArgs[1],
                                            NULL, 16);
            AcpiOsPrintf (
                "Debug Level for console output was %8.8lX, now %8.8lX\n",
                Temp, AcpiGbl_DbConsoleDebugLevel);
        }
        else
        {
            Temp = AcpiGbl_DbDebugLevel;
            AcpiGbl_DbDebugLevel = ACPI_STRTOUL (AcpiGbl_DbArgs[1], NULL, 16);
            AcpiOsPrintf (
                "Debug Level for file output was %8.8lX, now %8.8lX\n",
                Temp, AcpiGbl_DbDebugLevel);
        }
        break;

    case CMD_LIST:
        AcpiDbDisassembleAml (AcpiGbl_DbArgs[1], Op);
        break;

    case CMD_LOAD:
        Status = AcpiDbGetTableFromFile (AcpiGbl_DbArgs[1], NULL);
        break;

    case CMD_LOCKS:
        AcpiDbDisplayLocks ();
        break;

    case CMD_LOCALS:
        AcpiDbDisplayLocals ();
        break;

    case CMD_METHODS:
        Status = AcpiDbDisplayObjects ("METHOD", AcpiGbl_DbArgs[1]);
        break;

    case CMD_NAMESPACE:
        AcpiDbDumpNamespace (AcpiGbl_DbArgs[1], AcpiGbl_DbArgs[2]);
        break;

    case CMD_NOTIFY:
        Temp = ACPI_STRTOUL (AcpiGbl_DbArgs[2], NULL, 0);
        AcpiDbSendNotify (AcpiGbl_DbArgs[1], Temp);
        break;

    case CMD_OBJECT:
        AcpiUtStrupr (AcpiGbl_DbArgs[1]);
        Status = AcpiDbDisplayObjects (AcpiGbl_DbArgs[1], AcpiGbl_DbArgs[2]);
        break;

    case CMD_OPEN:
        AcpiDbOpenDebugFile (AcpiGbl_DbArgs[1]);
        break;

    case CMD_OSI:
        AcpiDbDisplayInterfaces (AcpiGbl_DbArgs[1], AcpiGbl_DbArgs[2]);
        break;

    case CMD_OWNER:
        AcpiDbDumpNamespaceByOwner (AcpiGbl_DbArgs[1], AcpiGbl_DbArgs[2]);
        break;

    case CMD_PREDEFINED:
        AcpiDbCheckPredefinedNames ();
        break;

    case CMD_PREFIX:
        AcpiDbSetScope (AcpiGbl_DbArgs[1]);
        break;

    case CMD_REFERENCES:
        AcpiDbFindReferences (AcpiGbl_DbArgs[1]);
        break;

    case CMD_RESOURCES:
        AcpiDbDisplayResources (AcpiGbl_DbArgs[1]);
        break;

    case CMD_RESULTS:
        AcpiDbDisplayResults ();
        break;

    case CMD_SET:
        AcpiDbSetMethodData (AcpiGbl_DbArgs[1], AcpiGbl_DbArgs[2],
            AcpiGbl_DbArgs[3]);
        break;

    case CMD_SLEEP:
        Status = AcpiDbSleep (AcpiGbl_DbArgs[1]);
        break;

    case CMD_STATS:
        Status = AcpiDbDisplayStatistics (AcpiGbl_DbArgs[1]);
        break;

    case CMD_STOP:
        return (AE_NOT_IMPLEMENTED);

    case CMD_TABLES:
        AcpiDbDisplayTableInfo (AcpiGbl_DbArgs[1]);
        break;

    case CMD_TERMINATE:
        AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);
        AcpiUtSubsystemShutdown ();

        /*
         * TBD: [Restructure] Need some way to re-initialize without
         * re-creating the semaphores!
         */

        /*  AcpiInitialize (NULL);  */
        break;

    case CMD_THREADS:
        AcpiDbCreateExecutionThreads (AcpiGbl_DbArgs[1], AcpiGbl_DbArgs[2],
            AcpiGbl_DbArgs[3]);
        break;

    case CMD_TRACE:
        (void) AcpiDebugTrace (AcpiGbl_DbArgs[1],0,0,1);
        break;

    case CMD_TREE:
        AcpiDbDisplayCallingTree ();
        break;

    case CMD_TYPE:
        AcpiDbDisplayObjectType (AcpiGbl_DbArgs[1]);
        break;

    case CMD_UNLOAD:
        AcpiDbUnloadAcpiTable (AcpiGbl_DbArgs[1], AcpiGbl_DbArgs[2]);
        break;

    case CMD_EXIT:
    case CMD_QUIT:
        if (Op)
        {
            AcpiOsPrintf ("Method execution terminated\n");
            return (AE_CTRL_TERMINATE);
        }

        if (!AcpiGbl_DbOutputToFile)
        {
            AcpiDbgLevel = ACPI_DEBUG_DEFAULT;
        }

        AcpiDbCloseDebugFile ();
        AcpiGbl_DbTerminateThreads = TRUE;
        return (AE_CTRL_TERMINATE);

    case CMD_NOT_FOUND:
    default:
        AcpiOsPrintf ("Unknown Command\n");
        return (AE_CTRL_TRUE);
    }

    if (ACPI_SUCCESS (Status))
    {
        Status = AE_CTRL_TRUE;
    }

    /* Add all commands that come here to the history buffer */

    AcpiDbAddToHistory (InputBuffer);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbExecuteThread
 *
 * PARAMETERS:  Context         - Not used
 *
 * RETURN:      None
 *
 * DESCRIPTION: Debugger execute thread.  Waits for a command line, then
 *              simply dispatches it.
 *
 ******************************************************************************/

void ACPI_SYSTEM_XFACE
AcpiDbExecuteThread (
    void                    *Context)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_STATUS             MStatus;


    while (Status != AE_CTRL_TERMINATE)
    {
        AcpiGbl_MethodExecuting = FALSE;
        AcpiGbl_StepToNextCall = FALSE;

        MStatus = AcpiUtAcquireMutex (ACPI_MTX_DEBUG_CMD_READY);
        if (ACPI_FAILURE (MStatus))
        {
            return;
        }

        Status = AcpiDbCommandDispatch (AcpiGbl_DbLineBuf, NULL, NULL);

        MStatus = AcpiUtReleaseMutex (ACPI_MTX_DEBUG_CMD_COMPLETE);
        if (ACPI_FAILURE (MStatus))
        {
            return;
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbSingleThread
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Debugger execute thread.  Waits for a command line, then
 *              simply dispatches it.
 *
 ******************************************************************************/

static void
AcpiDbSingleThread (
    void)
{

    AcpiGbl_MethodExecuting = FALSE;
    AcpiGbl_StepToNextCall = FALSE;

    (void) AcpiDbCommandDispatch (AcpiGbl_DbLineBuf, NULL, NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbUserCommands
 *
 * PARAMETERS:  Prompt              - User prompt (depends on mode)
 *              Op                  - Current executing parse op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Command line execution for the AML debugger.  Commands are
 *              matched and dispatched here.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbUserCommands (
    char                    Prompt,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_STATUS             Status = AE_OK;


    /* TBD: [Restructure] Need a separate command line buffer for step mode */

    while (!AcpiGbl_DbTerminateThreads)
    {
        /* Force output to console until a command is entered */

        AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);

        /* Different prompt if method is executing */

        if (!AcpiGbl_MethodExecuting)
        {
            AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_COMMAND_PROMPT);
        }
        else
        {
            AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_EXECUTE_PROMPT);
        }

        /* Get the user input line */

        Status = AcpiOsGetLine (AcpiGbl_DbLineBuf,
            ACPI_DB_LINE_BUFFER_SIZE, NULL);
        if (ACPI_FAILURE (Status))
        {
            ACPI_EXCEPTION ((AE_INFO, Status, "While parsing command line"));
            return (Status);
        }

        /* Check for single or multithreaded debug */

        if (AcpiGbl_DebuggerConfiguration & DEBUGGER_MULTI_THREADED)
        {
            /*
             * Signal the debug thread that we have a command to execute,
             * and wait for the command to complete.
             */
            Status = AcpiUtReleaseMutex (ACPI_MTX_DEBUG_CMD_READY);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            Status = AcpiUtAcquireMutex (ACPI_MTX_DEBUG_CMD_COMPLETE);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
        else
        {
            /* Just call to the command line interpreter */

            AcpiDbSingleThread ();
        }
    }

    /*
     * Only this thread (the original thread) should actually terminate the
     * subsystem, because all the semaphores are deleted during termination
     */
    Status = AcpiTerminate ();
    return (Status);
}

#endif  /* ACPI_DEBUGGER */

