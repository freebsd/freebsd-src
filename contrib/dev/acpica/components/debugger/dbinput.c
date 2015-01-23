/*******************************************************************************
 *
 * Module Name: dbinput - user front-end to the AML debugger
 *
 ******************************************************************************/

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
#include <contrib/dev/acpica/include/acdebug.h>


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
AcpiDbDisplayCommandInfo (
    char                    *Command,
    BOOLEAN                 DisplayAll);

static void
AcpiDbDisplayHelp (
    char                    *Command);

static BOOLEAN
AcpiDbMatchCommandHelp (
    char                        *Command,
    const ACPI_DB_COMMAND_HELP  *Help);


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
    CMD_BREAKPOINT,
    CMD_BUSINFO,
    CMD_CALL,
    CMD_CLOSE,
    CMD_DEBUG,
    CMD_DISASSEMBLE,
    CMD_DISASM,
    CMD_DUMP,
    CMD_ENABLEACPI,
    CMD_EVALUATE,
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
    CMD_OBJECTS,
    CMD_OPEN,
    CMD_OSI,
    CMD_OWNER,
    CMD_PATHS,
    CMD_PREDEFINED,
    CMD_PREFIX,
    CMD_QUIT,
    CMD_REFERENCES,
    CMD_RESOURCES,
    CMD_RESULTS,
    CMD_SCI,
    CMD_SET,
    CMD_SLEEP,
    CMD_STATS,
    CMD_STOP,
    CMD_TABLES,
    CMD_TEMPLATE,
    CMD_TERMINATE,
    CMD_TEST,
    CMD_THREADS,
    CMD_TRACE,
    CMD_TREE,
    CMD_TYPE,
    CMD_UNLOAD
};

#define CMD_FIRST_VALID     2


/* Second parameter is the required argument count */

static const ACPI_DB_COMMAND_INFO   AcpiGbl_DbCommands[] =
{
    {"<NOT FOUND>",  0},
    {"<NULL>",       0},
    {"ALLOCATIONS",  0},
    {"ARGS",         0},
    {"ARGUMENTS",    0},
    {"BREAKPOINT",   1},
    {"BUSINFO",      0},
    {"CALL",         0},
    {"CLOSE",        0},
    {"DEBUG",        1},
    {"DISASSEMBLE",  1},
    {"DISASM",       1},
    {"DUMP",         1},
    {"ENABLEACPI",   0},
    {"EVALUATE",     1},
    {"EVENT",        1},
    {"EXECUTE",      1},
    {"EXIT",         0},
    {"FIND",         1},
    {"GO",           0},
    {"GPE",          1},
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
    {"OBJECTS",      1},
    {"OPEN",         1},
    {"OSI",          0},
    {"OWNER",        1},
    {"PATHS",        0},
    {"PREDEFINED",   0},
    {"PREFIX",       0},
    {"QUIT",         0},
    {"REFERENCES",   1},
    {"RESOURCES",    0},
    {"RESULTS",      0},
    {"SCI",          0},
    {"SET",          3},
    {"SLEEP",        0},
    {"STATS",        1},
    {"STOP",         0},
    {"TABLES",       0},
    {"TEMPLATE",     1},
    {"TERMINATE",    0},
    {"TEST",        1},
    {"THREADS",      3},
    {"TRACE",        1},
    {"TREE",         0},
    {"TYPE",         1},
    {"UNLOAD",       1},
    {NULL,           0}
};

/*
 * Help for all debugger commands. First argument is the number of lines
 * of help to output for the command.
 */
static const ACPI_DB_COMMAND_HELP   AcpiGbl_DbCommandHelp[] =
{
    {0, "\nGeneral-Purpose Commands:",         "\n"},
    {1, "  Allocations",                       "Display list of current memory allocations\n"},
    {2, "  Dump <Address>|<Namepath>",         "\n"},
    {0, "       [Byte|Word|Dword|Qword]",      "Display ACPI objects or memory\n"},
    {1, "  EnableAcpi",                        "Enable ACPI (hardware) mode\n"},
    {1, "  Handlers",                          "Info about global handlers\n"},
    {1, "  Help [Command]",                    "This help screen or individual command\n"},
    {1, "  History",                           "Display command history buffer\n"},
    {1, "  Level <DebugLevel>] [console]",     "Get/Set debug level for file or console\n"},
    {1, "  Locks",                             "Current status of internal mutexes\n"},
    {1, "  Osi [Install|Remove <name>]",       "Display or modify global _OSI list\n"},
    {1, "  Quit or Exit",                      "Exit this command\n"},
    {8, "  Stats <SubCommand>",                "Display namespace and memory statistics\n"},
    {1, "     Allocations",                    "Display list of current memory allocations\n"},
    {1, "     Memory",                         "Dump internal memory lists\n"},
    {1, "     Misc",                           "Namespace search and mutex stats\n"},
    {1, "     Objects",                        "Summary of namespace objects\n"},
    {1, "     Sizes",                          "Sizes for each of the internal objects\n"},
    {1, "     Stack",                          "Display CPU stack usage\n"},
    {1, "     Tables",                         "Info about current ACPI table(s)\n"},
    {1, "  Tables",                            "Display info about loaded ACPI tables\n"},
    {1, "  Unload <Namepath>",                 "Unload an ACPI table via namespace object\n"},
    {1, "  ! <CommandNumber>",                 "Execute command from history buffer\n"},
    {1, "  !!",                                "Execute last command again\n"},

    {0, "\nNamespace Access Commands:",        "\n"},
    {1, "  Businfo",                           "Display system bus info\n"},
    {1, "  Disassemble <Method>",              "Disassemble a control method\n"},
    {1, "  Find <AcpiName> (? is wildcard)",   "Find ACPI name(s) with wildcards\n"},
    {1, "  Integrity",                         "Validate namespace integrity\n"},
    {1, "  Methods",                           "Display list of loaded control methods\n"},
    {1, "  Namespace [Object] [Depth]",        "Display loaded namespace tree/subtree\n"},
    {1, "  Notify <Object> <Value>",           "Send a notification on Object\n"},
    {1, "  Objects <ObjectType>",              "Display all objects of the given type\n"},
    {1, "  Owner <OwnerId> [Depth]",           "Display loaded namespace by object owner\n"},
    {1, "  Paths",                             "Display full pathnames of namespace objects\n"},
    {1, "  Predefined",                        "Check all predefined names\n"},
    {1, "  Prefix [<NamePath>]",               "Set or Get current execution prefix\n"},
    {1, "  References <Addr>",                 "Find all references to object at addr\n"},
    {1, "  Resources [DeviceName]",            "Display Device resources (no arg = all devices)\n"},
    {1, "  Set N <NamedObject> <Value>",       "Set value for named integer\n"},
    {1, "  Template <Object>",                 "Format/dump a Buffer/ResourceTemplate\n"},
    {1, "  Terminate",                         "Delete namespace and all internal objects\n"},
    {1, "  Type <Object>",                     "Display object type\n"},

    {0, "\nControl Method Execution Commands:","\n"},
    {1, "  Arguments (or Args)",               "Display method arguments\n"},
    {1, "  Breakpoint <AmlOffset>",            "Set an AML execution breakpoint\n"},
    {1, "  Call",                              "Run to next control method invocation\n"},
    {1, "  Debug <Namepath> [Arguments]",      "Single Step a control method\n"},
    {6, "  Evaluate",                          "Synonym for Execute\n"},
    {5, "  Execute <Namepath> [Arguments]",    "Execute control method\n"},
    {1, "     Hex Integer",                    "Integer method argument\n"},
    {1, "     \"Ascii String\"",               "String method argument\n"},
    {1, "     (Hex Byte List)",                "Buffer method argument\n"},
    {1, "     [Package Element List]",         "Package method argument\n"},
    {1, "  Go",                                "Allow method to run to completion\n"},
    {1, "  Information",                       "Display info about the current method\n"},
    {1, "  Into",                              "Step into (not over) a method call\n"},
    {1, "  List [# of Aml Opcodes]",           "Display method ASL statements\n"},
    {1, "  Locals",                            "Display method local variables\n"},
    {1, "  Results",                           "Display method result stack\n"},
    {1, "  Set <A|L> <#> <Value>",             "Set method data (Arguments/Locals)\n"},
    {1, "  Stop",                              "Terminate control method\n"},
    {1, "  Thread <Threads><Loops><NamePath>", "Spawn threads to execute method(s)\n"},
    {1, "  Trace <method name>",               "Trace method execution\n"},
    {1, "  Tree",                              "Display control method calling tree\n"},
    {1, "  <Enter>",                           "Single step next AML opcode (over calls)\n"},

    {0, "\nHardware Related Commands:",         "\n"},
    {1, "  Event <F|G> <Value>",               "Generate AcpiEvent (Fixed/GPE)\n"},
    {1, "  Gpe <GpeNum> [GpeBlockDevice]",     "Simulate a GPE\n"},
    {1, "  Gpes",                              "Display info on all GPEs\n"},
    {1, "  Sci",                               "Generate an SCI\n"},
    {1, "  Sleep [SleepState]",                "Simulate sleep/wake sequence(s) (0-5)\n"},

    {0, "\nFile I/O Commands:",                "\n"},
    {1, "  Close",                             "Close debug output file\n"},
    {1, "  Load <Input Filename>",             "Load ACPI table from a file\n"},
    {1, "  Open <Output Filename>",            "Open a file for debug output\n"},

    {0, "\nDebug Test Commands:",              "\n"},
    {3, "  Test <TestName>",                   "Invoke a debug test\n"},
    {1, "     Objects",                        "Read/write/compare all namespace data objects\n"},
    {1, "     Predefined",                     "Execute all ACPI predefined names (_STA, etc.)\n"},
    {0, NULL, NULL}
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbMatchCommandHelp
 *
 * PARAMETERS:  Command             - Command string to match
 *              Help                - Help table entry to attempt match
 *
 * RETURN:      TRUE if command matched, FALSE otherwise
 *
 * DESCRIPTION: Attempt to match a command in the help table in order to
 *              print help information for a single command.
 *
 ******************************************************************************/

static BOOLEAN
AcpiDbMatchCommandHelp (
    char                        *Command,
    const ACPI_DB_COMMAND_HELP  *Help)
{
    char                    *Invocation = Help->Invocation;
    UINT32                  LineCount;


    /* Valid commands in the help table begin with a couple of spaces */

    if (*Invocation != ' ')
    {
        return (FALSE);
    }

    while (*Invocation == ' ')
    {
        Invocation++;
    }

    /* Match command name (full command or substring) */

    while ((*Command) && (*Invocation) && (*Invocation != ' '))
    {
        if (ACPI_TOLOWER (*Command) != ACPI_TOLOWER (*Invocation))
        {
            return (FALSE);
        }

        Invocation++;
        Command++;
    }

    /* Print the appropriate number of help lines */

    LineCount = Help->LineCount;
    while (LineCount)
    {
        AcpiOsPrintf ("%-38s : %s", Help->Invocation, Help->Description);
        Help++;
        LineCount--;
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayCommandInfo
 *
 * PARAMETERS:  Command             - Command string to match
 *              DisplayAll          - Display all matching commands, or just
 *                                    the first one (substring match)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display help information for a Debugger command.
 *
 ******************************************************************************/

static void
AcpiDbDisplayCommandInfo (
    char                    *Command,
    BOOLEAN                 DisplayAll)
{
    const ACPI_DB_COMMAND_HELP  *Next;
    BOOLEAN                     Matched;


    Next = AcpiGbl_DbCommandHelp;
    while (Next->Invocation)
    {
        Matched = AcpiDbMatchCommandHelp (Command, Next);
        if (!DisplayAll && Matched)
        {
            return;
        }

        Next++;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayHelp
 *
 * PARAMETERS:  Command             - Optional command string to display help.
 *                                    if not specified, all debugger command
 *                                    help strings are displayed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display help for a single debugger command, or all of them.
 *
 ******************************************************************************/

static void
AcpiDbDisplayHelp (
    char                    *Command)
{
    const ACPI_DB_COMMAND_HELP  *Next = AcpiGbl_DbCommandHelp;


    if (!Command)
    {
        /* No argument to help, display help for all commands */

        while (Next->Invocation)
        {
            AcpiOsPrintf ("%-38s%s", Next->Invocation, Next->Description);
            Next++;
        }
    }
    else
    {
        /* Display help for all commands that match the subtring */

        AcpiDbDisplayCommandInfo (Command, TRUE);
    }
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
 * DESCRIPTION: Command line parsing. Get the next token on the command line
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
 * DESCRIPTION: Get the next command line from the user. Gets entire line
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


    if (AcpiUtSafeStrcpy (AcpiGbl_DbParsedBuf, sizeof (AcpiGbl_DbParsedBuf),
        InputBuffer))
    {
        AcpiOsPrintf ("Buffer overflow while parsing input line (max %u characters)\n",
            sizeof (AcpiGbl_DbParsedBuf));
        return (0);
    }

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


    /* Add all commands that come here to the history buffer */

    AcpiDbAddToHistory (InputBuffer);

    ParamCount = AcpiDbGetLine (InputBuffer);
    CommandIndex = AcpiDbMatchCommand (AcpiGbl_DbArgs[0]);
    Temp = 0;

    /* Verify that we have the minimum number of params */

    if (ParamCount < AcpiGbl_DbCommands[CommandIndex].MinArgs)
    {
        AcpiOsPrintf ("%u parameters entered, [%s] requires %u parameters\n",
            ParamCount, AcpiGbl_DbCommands[CommandIndex].Name,
            AcpiGbl_DbCommands[CommandIndex].MinArgs);

        AcpiDbDisplayCommandInfo (AcpiGbl_DbCommands[CommandIndex].Name, FALSE);
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
    case CMD_DISASM:

        (void) AcpiDbDisassembleMethod (AcpiGbl_DbArgs[1]);
        break;

    case CMD_DUMP:

        AcpiDbDecodeAndDisplayObject (AcpiGbl_DbArgs[1], AcpiGbl_DbArgs[2]);
        break;

    case CMD_ENABLEACPI:
#if (!ACPI_REDUCED_HARDWARE)

        Status = AcpiEnable();
        if (ACPI_FAILURE(Status))
        {
            AcpiOsPrintf("AcpiEnable failed (Status=%X)\n", Status);
            return (Status);
        }
#endif /* !ACPI_REDUCED_HARDWARE */
        break;

    case CMD_EVENT:

        AcpiOsPrintf ("Event command not implemented\n");
        break;

    case CMD_EVALUATE:
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

        AcpiDbDisplayHelp (AcpiGbl_DbArgs[1]);
        break;

    case CMD_HISTORY:

        AcpiDbDisplayHistory ();
        break;

    case CMD_HISTORY_EXE: /* ! command */

        CommandLine = AcpiDbGetFromHistory (AcpiGbl_DbArgs[1]);
        if (!CommandLine)
        {
            return (AE_CTRL_TRUE);
        }

        Status = AcpiDbCommandDispatch (CommandLine, WalkState, Op);
        return (Status);

    case CMD_HISTORY_LAST: /* !! command */

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

    case CMD_OBJECTS:

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

    case CMD_PATHS:

        AcpiDbDumpNamespacePaths ();
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

    case CMD_SCI:

        AcpiDbGenerateSci ();
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

    case CMD_TEMPLATE:

        AcpiDbDisplayTemplate (AcpiGbl_DbArgs[1]);
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

    case CMD_TEST:

        AcpiDbExecuteTest (AcpiGbl_DbArgs[1]);
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

        AcpiDbUnloadAcpiTable (AcpiGbl_DbArgs[1]);
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

        AcpiOsPrintf ("%s: unknown command\n", AcpiGbl_DbArgs[0]);
        return (AE_CTRL_TRUE);
    }

    if (ACPI_SUCCESS (Status))
    {
        Status = AE_CTRL_TRUE;
    }

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
 * DESCRIPTION: Debugger execute thread. Waits for a command line, then
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
 * DESCRIPTION: Debugger execute thread. Waits for a command line, then
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
 * DESCRIPTION: Command line execution for the AML debugger. Commands are
 *              matched and dispatched here.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbUserCommands (
    char                    Prompt,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_STATUS             Status = AE_OK;


    AcpiOsPrintf ("\n");

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
