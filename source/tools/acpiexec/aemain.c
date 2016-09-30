/******************************************************************************
 *
 * Module Name: aemain - Main routine for the AcpiExec utility
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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

#include "aecommon.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aemain")


/*
 * Main routine for the ACPI user-space execution utility.
 *
 * Portability note: The utility depends upon the host for command-line
 * wildcard support - it is not implemented locally. For example:
 *
 * Linux/Unix systems: Shell expands wildcards automatically.
 *
 * Windows: The setargv.obj module must be linked in to automatically
 * expand wildcards.
 */

/* Local prototypes */

static int
AeDoOptions (
    int                     argc,
    char                    **argv);

static void
AcpiDbRunBatchMode (
    void);


#define AE_BUFFER_SIZE              1024
#define ASL_MAX_FILES               256

/* Execution modes */

#define AE_MODE_COMMAND_LOOP        0   /* Normal command execution loop */
#define AE_MODE_BATCH_MULTIPLE      1   /* -b option to execute a command line */
#define AE_MODE_BATCH_SINGLE        2   /* -m option to execute a single control method */


/* Globals */

UINT8                       AcpiGbl_RegionFillValue = 0;
BOOLEAN                     AcpiGbl_IgnoreErrors = FALSE;
BOOLEAN                     AcpiGbl_DbOpt_NoRegionSupport = FALSE;
UINT8                       AcpiGbl_UseHwReducedFadt = FALSE;
BOOLEAN                     AcpiGbl_DoInterfaceTests = FALSE;
BOOLEAN                     AcpiGbl_LoadTestTables = FALSE;
BOOLEAN                     AcpiGbl_AeLoadOnly = FALSE;
static UINT8                AcpiGbl_ExecutionMode = AE_MODE_COMMAND_LOOP;
static char                 BatchBuffer[AE_BUFFER_SIZE];    /* Batch command buffer */
static char                 AeBuildDate[] = __DATE__;
static char                 AeBuildTime[] = __TIME__;

#define ACPIEXEC_NAME               "AML Execution/Debug Utility"
#define AE_SUPPORTED_OPTIONS        "?b:d:e:f^ghi:lm^rv^:x:"


/* Stubs for the disassembler */

void
MpSaveGpioInfo (
    ACPI_PARSE_OBJECT       *Op,
    AML_RESOURCE            *Resource,
    UINT32                  PinCount,
    UINT16                  *PinList,
    char                    *DeviceName)
{
}

void
MpSaveSerialInfo (
    ACPI_PARSE_OBJECT       *Op,
    AML_RESOURCE            *Resource,
    char                    *DeviceName)
{
}


/******************************************************************************
 *
 * FUNCTION:    usage
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print a usage message
 *
 *****************************************************************************/

static void
usage (
    void)
{

    ACPI_USAGE_HEADER ("acpiexec [options] AMLfile1 AMLfile2 ...");

    ACPI_OPTION ("-b \"CommandLine\"",  "Batch mode command line execution (cmd1;cmd2;...)");
    ACPI_OPTION ("-h -?",               "Display this help message");
    ACPI_OPTION ("-m [Method]",         "Batch mode method execution. Default=MAIN");
    printf ("\n");

    ACPI_OPTION ("-da",                 "Disable method abort on error");
    ACPI_OPTION ("-di",                 "Disable execution of STA/INI methods during init");
    ACPI_OPTION ("-do",                 "Disable Operation Region address simulation");
    ACPI_OPTION ("-dr",                 "Disable repair of method return values");
    ACPI_OPTION ("-ds",                 "Disable method auto-serialization");
    ACPI_OPTION ("-dt",                 "Disable allocation tracking (performance)");
    printf ("\n");

    ACPI_OPTION ("-ed",                 "Enable timer output for Debug Object");
    ACPI_OPTION ("-ef",                 "Enable display of final memory statistics");
    ACPI_OPTION ("-ei",                 "Enable additional tests for ACPICA interfaces");
    ACPI_OPTION ("-el",                 "Enable loading of additional test tables");
    ACPI_OPTION ("-em",                 "Enable grouping of module-level code");
    ACPI_OPTION ("-ep",                 "Enable TermList parsing for scope objects");
    ACPI_OPTION ("-es",                 "Enable Interpreter Slack Mode");
    ACPI_OPTION ("-et",                 "Enable debug semaphore timeout");
    printf ("\n");

    ACPI_OPTION ("-fi <File>",          "Specify namespace initialization file");
    ACPI_OPTION ("-fv <Value>",         "Operation Region initialization fill value");
    printf ("\n");

    ACPI_OPTION ("-i <Count>",          "Maximum iterations for AML while loops");
    ACPI_OPTION ("-l",                  "Load tables and namespace only");
    ACPI_OPTION ("-r",                  "Use hardware-reduced FADT V5");
    ACPI_OPTION ("-v",                  "Display version information");
    ACPI_OPTION ("-vd",                 "Display build date and time");
    ACPI_OPTION ("-vi",                 "Verbose initialization output");
    ACPI_OPTION ("-vr",                 "Verbose region handler output");
    ACPI_OPTION ("-x <DebugLevel>",     "Debug output level");

    printf ("\n  From within the interactive mode, use '?' or \"help\" to see\n"
        "  a list of available AML Debugger commands\n");
}


/******************************************************************************
 *
 * FUNCTION:    AeDoOptions
 *
 * PARAMETERS:  argc/argv           - Standard argc/argv
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Command line option processing
 *
 *****************************************************************************/

static int
AeDoOptions (
    int                     argc,
    char                    **argv)
{
    int                     j;
    UINT32                  Temp;


    while ((j = AcpiGetopt (argc, argv, AE_SUPPORTED_OPTIONS)) != ACPI_OPT_END) switch (j)
    {
    case 'b':

        if (strlen (AcpiGbl_Optarg) > (AE_BUFFER_SIZE -1))
        {
            printf ("**** The length of command line (%u) exceeded maximum (%u)\n",
                (UINT32) strlen (AcpiGbl_Optarg), (AE_BUFFER_SIZE -1));
            return (-1);
        }
        AcpiGbl_ExecutionMode = AE_MODE_BATCH_MULTIPLE;
        strcpy (BatchBuffer, AcpiGbl_Optarg);
        break;

    case 'd':

        switch (AcpiGbl_Optarg[0])
        {
        case 'a':

            AcpiGbl_IgnoreErrors = TRUE;
            break;

        case 'i':

            AcpiGbl_DbOpt_NoIniMethods = TRUE;
            break;

        case 'o':

            AcpiGbl_DbOpt_NoRegionSupport = TRUE;
            break;

        case 'r':

            AcpiGbl_DisableAutoRepair = TRUE;
            break;

        case 's':

            AcpiGbl_AutoSerializeMethods = FALSE;
            break;

        case 't':

            #ifdef ACPI_DBG_TRACK_ALLOCATIONS
                AcpiGbl_DisableMemTracking = TRUE;
            #endif
            break;

        default:

            printf ("Unknown option: -d%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'e':

        switch (AcpiGbl_Optarg[0])
        {
        case 'd':

            AcpiGbl_DisplayDebugTimer = TRUE;
            break;

        case 'f':

            #ifdef ACPI_DBG_TRACK_ALLOCATIONS
                AcpiGbl_DisplayFinalMemStats = TRUE;
            #endif
            break;

        case 'i':

            AcpiGbl_DoInterfaceTests = TRUE;
            break;

        case 'l':

            AcpiGbl_LoadTestTables = TRUE;
            break;

        case 'm':

            AcpiGbl_GroupModuleLevelCode = TRUE;
            break;

        case 'p':

            AcpiGbl_ParseTableAsTermList = TRUE;
            break;

        case 's':

            AcpiGbl_EnableInterpreterSlack = TRUE;
            printf ("Enabling AML Interpreter slack mode\n");
            break;

        case 't':

            AcpiGbl_DebugTimeout = TRUE;
            break;

        default:

            printf ("Unknown option: -e%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'f':

        switch (AcpiGbl_Optarg[0])
        {
        case 'v':   /* -fv: region fill value */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            AcpiGbl_RegionFillValue = (UINT8) strtoul (AcpiGbl_Optarg, NULL, 0);
            break;

        case 'i':   /* -fi: specify initialization file */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            if (AeOpenInitializationFile (AcpiGbl_Optarg))
            {
                return (-1);
            }
            break;

        default:

            printf ("Unknown option: -f%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'g':

        AcpiGbl_DbFilename = NULL;
        break;

    case 'h':
    case '?':

        usage();
        return (1);

    case 'i':

        Temp = strtoul (AcpiGbl_Optarg, NULL, 0);
        if (!Temp || (Temp > ACPI_UINT16_MAX))
        {
            printf ("%s: Invalid max loops value\n", AcpiGbl_Optarg);
            return (-1);
        }

        AcpiGbl_MaxLoopIterations = (UINT16) Temp;
        printf ("Max Loop Iterations is %u (0x%X)\n",
            AcpiGbl_MaxLoopIterations, AcpiGbl_MaxLoopIterations);
        break;

    case 'l':

        AcpiGbl_AeLoadOnly = TRUE;
        break;

    case 'm':

        AcpiGbl_ExecutionMode = AE_MODE_BATCH_SINGLE;
        switch (AcpiGbl_Optarg[0])
        {
        case '^':

            strcpy (BatchBuffer, "MAIN");
            break;

        default:

            strcpy (BatchBuffer, AcpiGbl_Optarg);
            break;
        }
        break;

    case 'r':

        AcpiGbl_UseHwReducedFadt = TRUE;
        printf ("Using ACPI 5.0 Hardware Reduced Mode via version 5 FADT\n");
        break;

    case 'v':

        switch (AcpiGbl_Optarg[0])
        {
        case '^':  /* -v: (Version): signon already emitted, just exit */

            (void) AcpiOsTerminate ();
            return (1);

        case 'd':

            printf ("Build date/time: %s %s\n", AeBuildDate, AeBuildTime);
            return (1);

        case 'i':

            AcpiDbgLevel |= ACPI_LV_INIT_NAMES;
            break;

        case 'r':

            AcpiGbl_DisplayRegionAccess = TRUE;
            break;

        default:

            printf ("Unknown option: -v%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'x':

        AcpiDbgLevel = strtoul (AcpiGbl_Optarg, NULL, 0);
        AcpiGbl_DbConsoleDebugLevel = AcpiDbgLevel;
        printf ("Debug Level: 0x%8.8X\n", AcpiDbgLevel);
        break;

    default:

        usage();
        return (-1);
    }

    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    main
 *
 * PARAMETERS:  argc, argv
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Main routine for AcpiExec utility
 *
 *****************************************************************************/

int ACPI_SYSTEM_XFACE
main (
    int                     argc,
    char                    **argv)
{
    ACPI_NEW_TABLE_DESC     *ListHead = NULL;
    ACPI_STATUS             Status;
    UINT32                  InitFlags;
    int                     ExitCode = 0;


    ACPI_DEBUG_INITIALIZE (); /* For debug version only */
    signal (SIGINT, AeCtrlCHandler);

    /* Init debug globals */

    AcpiDbgLevel = ACPI_NORMAL_DEFAULT;
    AcpiDbgLayer = 0xFFFFFFFF;

    /*
     * Initialize ACPICA and start debugger thread.
     *
     * NOTE: After ACPICA initialization, AcpiTerminate MUST be called
     * before this procedure exits -- otherwise, the console may be
     * left in an incorrect state.
     */
    Status = AcpiInitializeSubsystem ();
    ACPI_CHECK_OK (AcpiInitializeSubsystem, Status);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    /* ACPICA runtime configuration */

    AcpiGbl_MaxLoopIterations = 400;


    /* Initialize the AML debugger */

    Status = AcpiInitializeDebugger ();
    ACPI_CHECK_OK (AcpiInitializeDebugger, Status);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    printf (ACPI_COMMON_SIGNON (ACPIEXEC_NAME));
    if (argc < 2)
    {
        usage ();
        goto NormalExit;
    }

    /* Get the command line options */

    ExitCode = AeDoOptions (argc, argv);
    if (ExitCode)
    {
        if (ExitCode > 0)
        {
            ExitCode = 0;
        }

        goto ErrorExit;
    }

    /* The remaining arguments are filenames for ACPI tables */

    if (!argv[AcpiGbl_Optind])
    {
        goto EnterDebugger;
    }

    AcpiGbl_CstyleDisassembly = FALSE; /* Not supported for AcpiExec */

    /* Get each of the ACPI table files on the command line */

    while (argv[AcpiGbl_Optind])
    {
        /* Get all ACPI AML tables in this file */

        Status = AcGetAllTablesFromFile (argv[AcpiGbl_Optind],
            ACPI_GET_ALL_TABLES, &ListHead);
        if (ACPI_FAILURE (Status))
        {
            ExitCode = -1;
            goto ErrorExit;
        }

        AcpiGbl_Optind++;
    }

    printf ("\n");

    /* Build a local RSDT with all tables and let ACPICA process the RSDT */

    Status = AeBuildLocalTables (ListHead);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorExit;
    }

    /* Install all of the ACPI tables */

    Status = AeInstallTables ();
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not install ACPI tables, %s\n",
            AcpiFormatException (Status));
        goto EnterDebugger;
    }

    /*
     * Install most of the handlers (Regions, Notify, Table, etc.)
     * Override the default region handlers, especially SystemMemory,
     * which is simulated in this utility.
     */
    Status = AeInstallEarlyHandlers ();
    if (ACPI_FAILURE (Status))
    {
        goto EnterDebugger;
    }

    /* Setup initialization flags for ACPICA */

    InitFlags = (ACPI_NO_HANDLER_INIT | ACPI_NO_ACPI_ENABLE);
    if (AcpiGbl_DbOpt_NoIniMethods)
    {
        InitFlags |= (ACPI_NO_DEVICE_INIT | ACPI_NO_OBJECT_INIT);
    }

    /*
     * Main initialization for ACPICA subsystem
     * TBD: Need a way to call this after the ACPI table "LOAD" command?
     *
     * NOTE: This initialization does not match the _Lxx and _Exx methods
     * to individual GPEs, as there are no real GPEs when the hardware
     * is simulated - because there is no namespace until AeLoadTables is
     * executed. This may have to change if AcpiExec is ever run natively
     * on actual hardware (such as under UEFI).
     */
    Status = AcpiEnableSubsystem (InitFlags);
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not EnableSubsystem, %s\n",
            AcpiFormatException (Status));
        goto EnterDebugger;
    }

    Status = AeLoadTables ();

    /*
     * Exit namespace initialization for the "load namespace only" option.
     * No control methods will be executed. However, still enter the
     * the debugger.
     */
    if (AcpiGbl_AeLoadOnly)
    {
        goto EnterDebugger;
    }

    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not load ACPI tables, %s\n",
            AcpiFormatException (Status));
        goto EnterDebugger;
    }

    /*
     * Install handlers for "device driver" space IDs (EC,SMBus, etc.)
     * and fixed event handlers
     */
    AeInstallLateHandlers ();

    /* Finish the ACPICA initialization */

    Status = AcpiInitializeObjects (InitFlags);
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not InitializeObjects, %s\n",
            AcpiFormatException (Status));
        goto EnterDebugger;
    }

    AeMiscellaneousTests ();


EnterDebugger:

    /* Exit if error above and we are in one of the batch modes */

    if (ACPI_FAILURE (Status) && (AcpiGbl_ExecutionMode > 0))
    {
        goto ErrorExit;
    }

    /* Run a batch command or enter the command loop */

    switch (AcpiGbl_ExecutionMode)
    {
    default:
    case AE_MODE_COMMAND_LOOP:

        AcpiDbUserCommands (ACPI_DEBUGGER_COMMAND_PROMPT, NULL);
        break;

    case AE_MODE_BATCH_MULTIPLE:

        AcpiDbRunBatchMode ();
        break;

    case AE_MODE_BATCH_SINGLE:

        AcpiDbExecute (BatchBuffer, NULL, NULL, EX_NO_SINGLE_STEP);
        break;
    }

    /* Shut down the debugger and ACPICA */

#if 0

    /* Temporarily removed */
    AcpiTerminateDebugger ();
    (void) AcpiTerminate ();
#endif

NormalExit:
    ExitCode = 0;

ErrorExit:
    (void) AcpiOsTerminate ();
    return (ExitCode);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiDbRunBatchMode
 *
 * PARAMETERS:  BatchCommandLine    - A semicolon separated list of commands
 *                                    to be executed.
 *                                    Use only commas to separate elements of
 *                                    particular command.
 * RETURN:      None
 *
 * DESCRIPTION: For each command of list separated by ';' prepare the command
 *              buffer and pass it to AcpiDbCommandDispatch.
 *
 *****************************************************************************/

static void
AcpiDbRunBatchMode (
    void)
{
    char                    *Ptr = BatchBuffer;
    char                    *Cmd = Ptr;
    UINT8                   Run = 0;


    AcpiGbl_MethodExecuting = FALSE;
    AcpiGbl_StepToNextCall = FALSE;

    while (*Ptr)
    {
        if (*Ptr == ',')
        {
            /* Convert commas to spaces */
            *Ptr = ' ';
        }
        else if (*Ptr == ';')
        {
            *Ptr = '\0';
            Run = 1;
        }

        Ptr++;

        if (Run || (*Ptr == '\0'))
        {
            (void) AcpiDbCommandDispatch (Cmd, NULL, NULL);
            Run = 0;
            Cmd = Ptr;
        }
    }
}
