
/******************************************************************************
 *
 * Module Name: aslmain - compiler main and utilities
 *              $Revision: 1.96 $
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


#define _DECLARE_GLOBALS

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/actables.h>
#include <contrib/dev/acpica/acapps.h>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslmain")

BOOLEAN                 AslToFile = TRUE;
BOOLEAN                 DoCompile = TRUE;
BOOLEAN                 DoSignon = TRUE;

char                    hex[] =
{
    '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};

/* Local prototypes */

static void
Options (
    void);

static void
HelpMessage (
    void);

static void
Usage (
    void);

static void
AslInitialize (
    void);

static void
AslCommandLine (
    int                     argc,
    char                    **argv);

#ifdef _DEBUG
#include <crtdbg.h>
#endif

/*******************************************************************************
 *
 * FUNCTION:    Options
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display option help message
 *
 ******************************************************************************/

static void
Options (
    void)
{

    printf ("General Output:\n");
    printf ("  -p <prefix>    Specify filename prefix for all output files (including .aml)\n");
    printf ("  -vi            Less verbose errors and warnings for use with IDEs\n");
    printf ("  -vo            Enable optimization comments\n");
    printf ("  -vr            Disable remarks\n");
    printf ("  -vs            Disable signon\n");
    printf ("  -w<1|2|3>      Set warning reporting level\n");

    printf ("\nAML Output Files:\n");
    printf ("  -s<a|c>        Create AML in assembler or C source file (*.asm or *.c)\n");
    printf ("  -i<a|c>        Create assembler or C include file (*.inc or *.h)\n");
    printf ("  -t<a|c>        Create AML in assembler or C hex table (*.hex)\n");

    printf ("\nAML Code Generation:\n");
    printf ("  -oa            Disable all optimizations (compatibility mode)\n");
    printf ("  -of            Disable constant folding\n");
    printf ("  -oi            Disable integer optimization to Zero/One/Ones\n");
    printf ("  -on            Disable named reference string optimization\n");
    printf ("  -r<Revision>   Override table header Revision (1-255)\n");

    printf ("\nListings:\n");
    printf ("  -l             Create mixed listing file (ASL source and AML) (*.lst)\n");
    printf ("  -ln            Create namespace file (*.nsp)\n");
    printf ("  -ls            Create combined source file (expanded includes) (*.src)\n");

    printf ("\nAML Disassembler:\n");
    printf ("  -d  [file]     Disassemble or decode binary ACPI table to file (*.dsl)\n");
    printf ("  -dc [file]     Disassemble AML and immediately compile it\n");
    printf ("                 (Obtain DSDT from current system if no input file)\n");
    printf ("  -e  [file]     Include ACPI table for external symbol resolution\n");
    printf ("  -2             Emit ACPI 2.0 compatible ASL code\n");
    printf ("  -g             Get ACPI tables and write to files (*.dat)\n");

    printf ("\nHelp:\n");
    printf ("  -h             Additional help and compiler debug options\n");
    printf ("  -hc            Display operators allowed in constant expressions\n");
    printf ("  -hr            Display ACPI reserved method names\n");
}


/*******************************************************************************
 *
 * FUNCTION:    HelpMessage
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display help message
 *
 ******************************************************************************/

static void
HelpMessage (
    void)
{

    printf ("AML output filename generation:\n");
    printf ("  Output filenames are generated by appending an extension to a common\n");
    printf ("  filename prefix.  The filename prefix is obtained via one of the\n");
    printf ("  following methods (in priority order):\n");
    printf ("    1) The -p option specifies the prefix\n");
    printf ("    2) The prefix of the AMLFileName in the ASL Definition Block\n");
    printf ("    3) The prefix of the input filename\n");
    printf ("\n");

    Options ();

    printf ("\nCompiler/Disassembler Debug Options:\n");
    printf ("  -b<p|t|b>      Create compiler debug/trace file (*.txt)\n");
    printf ("                   Types: Parse/Tree/Both\n");
    printf ("  -f             Ignore errors, force creation of AML output file(s)\n");
    printf ("  -c             Parse only, no output generation\n");
    printf ("  -ot            Display compile times\n");
    printf ("  -x<level>      Set debug level for trace output\n");
}


/*******************************************************************************
 *
 * FUNCTION:    Usage
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display usage and option message
 *
 ******************************************************************************/

static void
Usage (
    void)
{

    printf ("Usage:    %s [Options] [InputFile]\n\n", CompilerName);
    Options ();
}


/*******************************************************************************
 *
 * FUNCTION:    AslInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialize compiler globals
 *
 ******************************************************************************/

static void
AslInitialize (
    void)
{
    UINT32                  i;


#ifdef _DEBUG
    _CrtSetDbgFlag (_CRTDBG_CHECK_ALWAYS_DF | _CrtSetDbgFlag(0));
#endif

    AcpiDbgLevel = 0;

    for (i = 0; i < ASL_NUM_FILES; i++)
    {
        Gbl_Files[i].Handle = NULL;
        Gbl_Files[i].Filename = NULL;
    }

    Gbl_Files[ASL_FILE_STDOUT].Handle   = stdout;
    Gbl_Files[ASL_FILE_STDOUT].Filename = "STDOUT";

    Gbl_Files[ASL_FILE_STDERR].Handle   = stderr;
    Gbl_Files[ASL_FILE_STDERR].Filename = "STDERR";
}


/*******************************************************************************
 *
 * FUNCTION:    AslCommandLine
 *
 * PARAMETERS:  argc/argv
 *
 * RETURN:      None
 *
 * DESCRIPTION: Command line processing
 *
 ******************************************************************************/

static void
AslCommandLine (
    int                     argc,
    char                    **argv)
{
    BOOLEAN                 BadCommandLine = FALSE;
    ACPI_NATIVE_INT         j;


    /* Minimum command line contains at least one option or an input file */

    if (argc < 2)
    {
        AslCompilerSignon (ASL_FILE_STDOUT);
        Usage ();
        exit (1);
    }

    /* Get the command line options */

    while ((j = AcpiGetopt (argc, argv, "2b:cd^e:fgh^i^l^o:p:r:s:t:v:w:x:")) != EOF) switch (j)
    {
    case '2':
        Gbl_Acpi2 = TRUE;
        break;


    case 'b':

        switch (AcpiGbl_Optarg[0])
        {
        case 'b':
            AslCompilerdebug = 1; /* same as yydebug */
            break;

        case 'p':
            AslCompilerdebug = 1; /* same as yydebug */
            break;

        case 't':
            break;

        default:
            printf ("Unknown option: -b%s\n", AcpiGbl_Optarg);
            BadCommandLine = TRUE;
            break;
        }

        /* Produce debug output file */

        Gbl_DebugFlag = TRUE;
        break;


    case 'c':

        /* Parse only */

        Gbl_ParseOnlyFlag = TRUE;
        break;


    case 'd':
        switch (AcpiGbl_Optarg[0])
        {
        case '^':
            DoCompile = FALSE;
            break;

        case 'c':
            break;

        default:
            printf ("Unknown option: -d%s\n", AcpiGbl_Optarg);
            BadCommandLine = TRUE;
            break;
        }

        Gbl_DisasmFlag = TRUE;
        break;


    case 'e':
        Gbl_ExternalFilename = AcpiGbl_Optarg;
        break;


    case 'f':

        /* Ignore errors and force creation of aml file */

        Gbl_IgnoreErrors = TRUE;
        break;


    case 'g':

        /* Get all ACPI tables */

        Gbl_GetAllTables = TRUE;
        DoCompile = FALSE;
        break;


    case 'h':

        switch (AcpiGbl_Optarg[0])
        {
        case '^':
            HelpMessage ();
            exit (0);

        case 'c':
            UtDisplayConstantOpcodes ();
            exit (0);

        case 'r':
            /* reserved names */

            MpDisplayReservedNames ();
            exit (0);

        default:
            printf ("Unknown option: -h%s\n", AcpiGbl_Optarg);
            BadCommandLine = TRUE;
            break;
        }
        break;


    case 'i':

        switch (AcpiGbl_Optarg[0])
        {
        case 'a':

            /* Produce assembly code include file */

            Gbl_AsmIncludeOutputFlag = TRUE;
            break;

        case 'c':

            /* Produce C include file */

            Gbl_C_IncludeOutputFlag = TRUE;
            break;

        default:
            printf ("Unknown option: -s%s\n", AcpiGbl_Optarg);
            BadCommandLine = TRUE;
            break;
        }
        break;


    case 'l':

        switch (AcpiGbl_Optarg[0])
        {
        case '^':
            /* Produce listing file (Mixed source/aml) */

            Gbl_ListingFlag = TRUE;
            break;

        case 'n':
            /* Produce namespace file */

            Gbl_NsOutputFlag = TRUE;
            break;

        case 's':
            /* Produce combined source file */

            Gbl_SourceOutputFlag = TRUE;
            break;

        default:
            printf ("Unknown option: -l%s\n", AcpiGbl_Optarg);
            BadCommandLine = TRUE;
            break;
        }
        break;


    case 'o':

        switch (AcpiGbl_Optarg[0])
        {
        case 'a':

            /* Disable all optimizations */

            Gbl_FoldConstants = FALSE;
            Gbl_IntegerOptimizationFlag = FALSE;
            Gbl_ReferenceOptimizationFlag = FALSE;
            break;

        case 'f':

            /* Disable folding on "normal" expressions */

            Gbl_FoldConstants = FALSE;
            break;

        case 'i':

            /* Disable integer optimization to constants */

            Gbl_IntegerOptimizationFlag = FALSE;
            break;

        case 'n':

            /* Disable named reference optimization */

            Gbl_ReferenceOptimizationFlag = FALSE;
            break;

        case 't':

            /* Display compile time(s) */

            Gbl_CompileTimesFlag = TRUE;
            break;

        default:
            printf ("Unknown option: -c%s\n", AcpiGbl_Optarg);
            BadCommandLine = TRUE;
            break;
        }
        break;


    case 'p':

        /* Override default AML output filename */

        Gbl_OutputFilenamePrefix = AcpiGbl_Optarg;
        Gbl_UseDefaultAmlFilename = FALSE;
        break;


    case 'r':
        Gbl_RevisionOverride = (UINT8) strtoul (AcpiGbl_Optarg, NULL, 0);
        break;


    case 's':

        switch (AcpiGbl_Optarg[0])
        {
        case 'a':

            /* Produce assembly code output file */

            Gbl_AsmOutputFlag = TRUE;
            break;

        case 'c':

            /* Produce C hex output file */

            Gbl_C_OutputFlag = TRUE;
            break;

        default:
            printf ("Unknown option: -s%s\n", AcpiGbl_Optarg);
            BadCommandLine = TRUE;
            break;
        }
        break;


    case 't':

        /* Produce hex table output file */

        switch (AcpiGbl_Optarg[0])
        {
        case 'a':
            Gbl_HexOutputFlag = HEX_OUTPUT_ASM;
            break;

        case 'c':
            Gbl_HexOutputFlag = HEX_OUTPUT_C;
            break;

        default:
            printf ("Unknown option: -t%s\n", AcpiGbl_Optarg);
            BadCommandLine = TRUE;
            break;
        }
        break;


    case 'v':

        switch (AcpiGbl_Optarg[0])
        {
        case 'i':
            /* Less verbose error messages */

            Gbl_VerboseErrors = FALSE;
            break;

        case 'o':
            Gbl_DisplayOptimizations = TRUE;
            break;

        case 'r':
            Gbl_DisplayRemarks = FALSE;
            break;

        case 's':
            DoSignon = FALSE;
            break;

        default:
            printf ("Unknown option: -v%s\n", AcpiGbl_Optarg);
            BadCommandLine = TRUE;
            break;
        }
        break;


    case 'w': /* Set warning levels */

        switch (AcpiGbl_Optarg[0])
        {
        case '1':
            Gbl_WarningLevel = ASL_WARNING;
            break;

        case '2':
            Gbl_WarningLevel = ASL_WARNING2;
            break;

        case '3':
            Gbl_WarningLevel = ASL_WARNING3;
            break;

        default:
            printf ("Unknown option: -w%s\n", AcpiGbl_Optarg);
            BadCommandLine = TRUE;
            break;
        }
        break;


    case 'x':

        AcpiDbgLevel = strtoul (AcpiGbl_Optarg, NULL, 16);
        break;


    default:

        BadCommandLine = TRUE;
        break;
    }

    /* Next parameter must be the input filename */

    Gbl_Files[ASL_FILE_INPUT].Filename = argv[AcpiGbl_Optind];

    if (!Gbl_Files[ASL_FILE_INPUT].Filename &&
        !Gbl_DisasmFlag &&
        !Gbl_GetAllTables)
    {
        printf ("Missing input filename\n");
        BadCommandLine = TRUE;
    }

    if (DoSignon)
    {
        AslCompilerSignon (ASL_FILE_STDOUT);
    }

    /* Abort if anything went wrong on the command line */

    if (BadCommandLine)
    {
        printf ("\n");
        Usage ();
        exit (1);
    }

    if ((AcpiGbl_Optind + 1) < argc)
    {
        printf ("Warning: extra arguments (%d) after input filename are ignored\n\n",
            argc - AcpiGbl_Optind - 1);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    main
 *
 * PARAMETERS:  Standard argc/argv
 *
 * RETURN:      Program termination code
 *
 * DESCRIPTION: C main routine for the Asl Compiler.  Handle command line
 *              options and begin the compile.
 *
 ******************************************************************************/

int ACPI_SYSTEM_XFACE
main (
    int                     argc,
    char                    **argv)
{
    ACPI_STATUS             Status;
    char                    *Prefix;


#ifdef _DEBUG
    _CrtSetDbgFlag (_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF |
                    _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
#endif

    /* Init and command line */

    AslInitialize ();
    AslCommandLine (argc, argv);

    /*
     * If -p not specified, we will use the input filename as the
     * output filename prefix
     */
    Status = FlSplitInputPathname (Gbl_Files[ASL_FILE_INPUT].Filename,
        &Gbl_DirectoryPath, &Prefix);
    if (ACPI_FAILURE (Status))
    {
        return -1;
    }

    if (Gbl_UseDefaultAmlFilename)
    {
        Gbl_OutputFilenamePrefix = Prefix;
    }

    /* AML Disassembly (Optional) */

    if (Gbl_DisasmFlag || Gbl_GetAllTables)
    {
        /* ACPI CA subsystem initialization */

        Status = AdInitialize ();
        if (ACPI_FAILURE (Status))
        {
            return -1;
        }

        Status = AcpiAllocateRootTable (4);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not initialize ACPI Table Manager, %s\n",
                AcpiFormatException (Status));
            return -1;
        }

        /* This is where the disassembly happens */

        AcpiGbl_DbOpt_disasm = TRUE;
        Status = AdAmlDisassemble (AslToFile,
                        Gbl_Files[ASL_FILE_INPUT].Filename,
                        Gbl_OutputFilenamePrefix,
                        &Gbl_Files[ASL_FILE_INPUT].Filename,
                        Gbl_GetAllTables);
        if (ACPI_FAILURE (Status))
        {
            return -1;
        }

        /*
         * Gbl_Files[ASL_FILE_INPUT].Filename was replaced with the
         * .DSL disassembly file, which can now be compiled if requested
         */
        if (DoCompile)
        {
            AcpiOsPrintf ("\nCompiling \"%s\"\n",
                Gbl_Files[ASL_FILE_INPUT].Filename);
        }
    }

    /*
     * ASL Compilation (Optional)
     */
    if (DoCompile)
    {
        /*
         * If -p not specified, we will use the input filename as the
         * output filename prefix
         */
        Status = FlSplitInputPathname (Gbl_Files[ASL_FILE_INPUT].Filename,
            &Gbl_DirectoryPath, &Prefix);
        if (ACPI_FAILURE (Status))
        {
            return -1;
        }

        if (Gbl_UseDefaultAmlFilename)
        {
            Gbl_OutputFilenamePrefix = Prefix;
        }

        /* ACPI CA subsystem initialization (Must be re-initialized) */

        Status = AcpiOsInitialize ();
        AcpiUtInitGlobals ();
        Status = AcpiUtMutexInitialize ();
        if (ACPI_FAILURE (Status))
        {
            return -1;
        }

        Status = AcpiNsRootInitialize ();
        if (ACPI_FAILURE (Status))
        {
            return -1;
        }
        Status = CmDoCompile ();
    }

    return (0);
}


