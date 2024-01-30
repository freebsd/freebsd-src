/******************************************************************************
 *
 * Module Name: aemain - Main routine for the AcpiExec utility
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

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


#define AE_BUFFER_SIZE              1024
#define ASL_MAX_FILES               256

/* Execution modes */

#define AE_MODE_COMMAND_LOOP        0   /* Normal command execution loop */
#define AE_MODE_BATCH_MULTIPLE      1   /* -b option to execute a command line */
#define AE_MODE_BATCH_SINGLE        2   /* -m option to execute a single control method */


/* Globals */

BOOLEAN                     AcpiGbl_UseLocalFaultHandler = TRUE;
BOOLEAN                     AcpiGbl_VerboseHandlers = FALSE;
UINT8                       AcpiGbl_RegionFillValue = 0;
BOOLEAN                     AcpiGbl_IgnoreErrors = FALSE;
BOOLEAN                     AcpiGbl_AbortLoopOnTimeout = FALSE;
UINT8                       AcpiGbl_UseHwReducedFadt = FALSE;
BOOLEAN                     AcpiGbl_DoInterfaceTests = FALSE;
BOOLEAN                     AcpiGbl_LoadTestTables = FALSE;
BOOLEAN                     AcpiGbl_AeLoadOnly = FALSE;
static UINT8                AcpiGbl_ExecutionMode = AE_MODE_COMMAND_LOOP;
static char                 BatchBuffer[AE_BUFFER_SIZE];    /* Batch command buffer */
INIT_FILE_ENTRY             *AcpiGbl_InitEntries = NULL;
UINT32                      AcpiGbl_InitFileLineCount = 0;

#define ACPIEXEC_NAME               "AML Execution/Debug Utility"
#define AE_SUPPORTED_OPTIONS        "?b:d:e:f^ghlm^rt^v^:x:"


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
    ACPI_OPTION ("-df",                 "Disable Local fault handler");
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
    ACPI_OPTION ("-eo",                 "Enable object evaluation log");
    ACPI_OPTION ("-es",                 "Enable Interpreter Slack Mode");
    ACPI_OPTION ("-et",                 "Enable debug semaphore timeout");
    printf ("\n");

    ACPI_OPTION ("-fi <File>",          "Specify namespace initialization file");
    ACPI_OPTION ("-fv <Value>",         "Operation Region initialization fill value");
    printf ("\n");

    ACPI_OPTION ("-l",                  "Load tables and namespace only");
    ACPI_OPTION ("-r",                  "Use hardware-reduced FADT V5");
    ACPI_OPTION ("-te",                 "Exit loop on timeout instead of aborting method");
    ACPI_OPTION ("-to <Seconds>",       "Set timeout period for AML while loops");
    printf ("\n");

    ACPI_OPTION ("-v",                  "Display version information");
    ACPI_OPTION ("-va",                 "Display verbose dump of any memory leaks");
    ACPI_OPTION ("-vd",                 "Display build date and time");
    ACPI_OPTION ("-vh",                 "Verbose exception handler output");
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
            printf ("**** The length of command line (%u) exceeded maximum (%d)\n",
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

        case 'f':

            AcpiGbl_UseLocalFaultHandler = FALSE;
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

        case 'o':

            AcpiDbgLevel |= ACPI_LV_EVALUATION;
            AcpiGbl_DbConsoleDebugLevel |= ACPI_LV_EVALUATION;
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

    case 't':

        switch (AcpiGbl_Optarg[0])
        {
        case 'o':  /* -to: Set loop timeout in seconds */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            Temp = strtoul (AcpiGbl_Optarg, NULL, 0);
            if (!Temp || (Temp > ACPI_UINT16_MAX))
            {
                printf ("%s: Invalid loop timeout value\n",
                    AcpiGbl_Optarg);
                return (-1);
            }

            AcpiGbl_MaxLoopIterations = (UINT16) Temp;
            printf ("Automatic loop timeout after %u seconds\n",
                AcpiGbl_MaxLoopIterations);
            break;

        case 'e':

            AcpiGbl_AbortLoopOnTimeout = TRUE;
            break;

        default:

            printf ("Unknown option: -t%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'v':

        switch (AcpiGbl_Optarg[0])
        {
        case '^':  /* -v: (Version): signon already emitted, just exit */

            return (1);

        case 'a':

            AcpiGbl_VerboseLeakDump = TRUE;
            break;

        case 'd':

            printf (ACPI_COMMON_BUILD_TIME);
            return (1);

        case 'h':

            AcpiGbl_VerboseHandlers = TRUE;
            break;

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

        AcpiDbgLevel = strtoul (AcpiGbl_Optarg, NULL, 16);
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

    signal (SIGINT, AeSignalHandler);

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
        ExitCode = -1;
        goto ErrorExit;
    }

    /* Use a shorter timeout value for acpiexec */

    AcpiGbl_MaxLoopIterations = 10;

    /* Initialize the AML debugger */

    Status = AcpiInitializeDebugger ();
    ACPI_CHECK_OK (AcpiInitializeDebugger, Status);
    if (ACPI_FAILURE (Status))
    {
        ExitCode = -1;
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

    if (AcpiGbl_UseLocalFaultHandler)
    {
        signal (SIGSEGV, AeSignalHandler);
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
        ExitCode = -1;
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

    /* Read the entire namespace initialization file if requested */

    Status = AeProcessInitFile();
    if (ACPI_FAILURE (Status))
    {
        ExitCode = -1;
        goto ErrorExit;
    }

    Status = AeLoadTables ();
    if (ACPI_FAILURE (Status))
    {
        printf ("**** Could not load ACPI tables, %s\n",
            AcpiFormatException (Status));
        goto EnterDebugger;
    }

    /*
     * Exit namespace initialization for the "load namespace only" option.
     * No control methods will be executed. However, still enter the
     * the debugger.
     */
    if (AcpiGbl_AeLoadOnly)
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

    AeDisplayUnusedInitFileItems ();
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

        AcpiRunDebugger (NULL);
        break;

    case AE_MODE_BATCH_MULTIPLE:

        AcpiRunDebugger (BatchBuffer);
        break;

    case AE_MODE_BATCH_SINGLE:

        AcpiDbExecute (BatchBuffer, NULL, NULL, EX_NO_SINGLE_STEP);
        break;
    }

    /* Shut down the debugger and ACPICA */

    AcpiTerminateDebugger ();

    /* re-enable debug output for AcpiTerminate output */

    AcpiGbl_DbOutputFlags = ACPI_DB_CONSOLE_OUTPUT;

NormalExit:
    ExitCode = 0;

ErrorExit:
    AeLateTest ();

    AeDeleteInitFileList ();

    (void) AcpiTerminate ();
    AcDeleteTableList (ListHead);
    return (ExitCode);
}
