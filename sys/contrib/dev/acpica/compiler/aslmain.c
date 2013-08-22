/******************************************************************************
 *
 * Module Name: aslmain - compiler main and utilities
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
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


#define _DECLARE_GLOBALS

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/acapps.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <signal.h>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslmain")

/* Local prototypes */

static void
Options (
    void);

static void
FilenameHelp (
    void);

static void
Usage (
    void);

static void ACPI_SYSTEM_XFACE
AslSignalHandler (
    int                     Sig);

static void
AslInitialize (
    void);

static int
AslCommandLine (
    int                     argc,
    char                    **argv);

static int
AslDoOptions (
    int                     argc,
    char                    **argv,
    BOOLEAN                 IsResponseFile);

static void
AslMergeOptionTokens (
    char                    *InBuffer,
    char                    *OutBuffer);

static int
AslDoResponseFile (
    char                    *Filename);


#define ASL_TOKEN_SEPARATORS    " \t\n"
#define ASL_SUPPORTED_OPTIONS   "@:b|c|d^D:e:fgh^i|I:l^m:no|p:P^r:s|t|T:G^v^w|x:z"


/*******************************************************************************
 *
 * FUNCTION:    Options
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display option help message.
 *              Optional items in square brackets.
 *
 ******************************************************************************/

static void
Options (
    void)
{

    printf ("\nGlobal:\n");
    ACPI_OPTION ("-@ <file>",       "Specify command file");
    ACPI_OPTION ("-I <dir>",        "Specify additional include directory");
    ACPI_OPTION ("-T <sig>|ALL|*",  "Create table template file for ACPI <Sig>");
    ACPI_OPTION ("-v",              "Display compiler version");

    printf ("\nPreprocessor:\n");
    ACPI_OPTION ("-D <symbol>",     "Define symbol for preprocessor use");
    ACPI_OPTION ("-li",             "Create preprocessed output file (*.i)");
    ACPI_OPTION ("-P",              "Preprocess only and create preprocessor output file (*.i)");
    ACPI_OPTION ("-Pn",             "Disable preprocessor");

    printf ("\nGeneral Processing:\n");
    ACPI_OPTION ("-p <prefix>",     "Specify path/filename prefix for all output files");
    ACPI_OPTION ("-va",             "Disable all errors and warnings (summary only)");
    ACPI_OPTION ("-vi",             "Less verbose errors and warnings for use with IDEs");
    ACPI_OPTION ("-vo",             "Enable optimization comments");
    ACPI_OPTION ("-vr",             "Disable remarks");
    ACPI_OPTION ("-vs",             "Disable signon");
    ACPI_OPTION ("-w1 -w2 -w3",     "Set warning reporting level");
    ACPI_OPTION ("-we",             "Report warnings as errors");

    printf ("\nAML Code Generation (*.aml):\n");
    ACPI_OPTION ("-oa",             "Disable all optimizations (compatibility mode)");
    ACPI_OPTION ("-of",             "Disable constant folding");
    ACPI_OPTION ("-oi",             "Disable integer optimization to Zero/One/Ones");
    ACPI_OPTION ("-on",             "Disable named reference string optimization");
    ACPI_OPTION ("-cr",             "Disable Resource Descriptor error checking");
    ACPI_OPTION ("-in",             "Ignore NoOp operators");
    ACPI_OPTION ("-r <revision>",   "Override table header Revision (1-255)");

    printf ("\nOptional Source Code Output Files:\n");
    ACPI_OPTION ("-sc -sa",         "Create source file in C or assembler (*.c or *.asm)");
    ACPI_OPTION ("-ic -ia",         "Create include file in C or assembler (*.h or *.inc)");
    ACPI_OPTION ("-tc -ta -ts",     "Create hex AML table in C, assembler, or ASL (*.hex)");
    ACPI_OPTION ("-so",             "Create offset table in C (*.offset.h)");

    printf ("\nOptional Listing Files:\n");
    ACPI_OPTION ("-l",              "Create mixed listing file (ASL source and AML) (*.lst)");
    ACPI_OPTION ("-ln",             "Create namespace file (*.nsp)");
    ACPI_OPTION ("-ls",             "Create combined source file (expanded includes) (*.src)");

    printf ("\nData Table Compiler:\n");
    ACPI_OPTION ("-G",              "Compile custom table that contains generic operators");
    ACPI_OPTION ("-vt",             "Create verbose template files (full disassembly)");

    printf ("\nAML Disassembler:\n");
    ACPI_OPTION ("-d  <f1,f2>",     "Disassemble or decode binary ACPI tables to file (*.dsl)");
    ACPI_OPTION ("",                "  (Optional, file type is automatically detected)");
    ACPI_OPTION ("-da <f1,f2>",     "Disassemble multiple tables from single namespace");
    ACPI_OPTION ("-db",             "Do not translate Buffers to Resource Templates");
    ACPI_OPTION ("-dc <f1,f2>",     "Disassemble AML and immediately compile it");
    ACPI_OPTION ("",                "  (Obtain DSDT from current system if no input file)");
    ACPI_OPTION ("-e  <f1,f2>",     "Include ACPI table(s) for external symbol resolution");
    ACPI_OPTION ("-g",              "Get ACPI tables and write to files (*.dat)");
    ACPI_OPTION ("-in",             "Ignore NoOp opcodes");
    ACPI_OPTION ("-vt",             "Dump binary table data in hex format within output file");

    printf ("\nHelp:\n");
    ACPI_OPTION ("-h",              "This message");
    ACPI_OPTION ("-hc",             "Display operators allowed in constant expressions");
    ACPI_OPTION ("-hf",             "Display help for output filename generation");
    ACPI_OPTION ("-hr",             "Display ACPI reserved method names");
    ACPI_OPTION ("-ht",             "Display currently supported ACPI table names");

    printf ("\nDebug Options:\n");
    ACPI_OPTION ("-bf -bt",         "Create debug file (full or parse tree only) (*.txt)");
    ACPI_OPTION ("-f",              "Ignore errors, force creation of AML output file(s)");
    ACPI_OPTION ("-m <size>",       "Set internal line buffer size (in Kbytes)");
    ACPI_OPTION ("-n",              "Parse only, no output generation");
    ACPI_OPTION ("-ot",             "Display compile times and statistics");
    ACPI_OPTION ("-x <level>",      "Set debug level for trace output");
    ACPI_OPTION ("-z",              "Do not insert new compiler ID for DataTables");
}


/*******************************************************************************
 *
 * FUNCTION:    FilenameHelp
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display help message for output filename generation
 *
 ******************************************************************************/

static void
FilenameHelp (
    void)
{

    printf ("\nAML output filename generation:\n");
    printf ("  Output filenames are generated by appending an extension to a common\n");
    printf ("  filename prefix. The filename prefix is obtained via one of the\n");
    printf ("  following methods (in priority order):\n");
    printf ("    1) The -p option specifies the prefix\n");
    printf ("    2) The prefix of the AMLFileName in the ASL Definition Block\n");
    printf ("    3) The prefix of the input filename\n");
    printf ("\n");
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

    printf ("%s\n\n", ASL_COMPLIANCE);
    ACPI_USAGE_HEADER ("iasl [Options] [Files]");
    Options ();
}


/******************************************************************************
 *
 * FUNCTION:    AslSignalHandler
 *
 * PARAMETERS:  Sig                 - Signal that invoked this handler
 *
 * RETURN:      None
 *
 * DESCRIPTION: Control-C handler. Delete any intermediate files and any
 *              output files that may be left in an indeterminate state.
 *
 *****************************************************************************/

static void ACPI_SYSTEM_XFACE
AslSignalHandler (
    int                     Sig)
{
    UINT32                  i;


    signal (Sig, SIG_IGN);
    printf ("Aborting\n\n");

    /* Close all open files */

    Gbl_Files[ASL_FILE_PREPROCESSOR].Handle = NULL; /* the .i file is same as source file */

    for (i = ASL_FILE_INPUT; i < ASL_MAX_FILE_TYPE; i++)
    {
        FlCloseFile (i);
    }

    /* Delete any output files */

    for (i = ASL_FILE_AML_OUTPUT; i < ASL_MAX_FILE_TYPE; i++)
    {
        FlDeleteFile (i);
    }

    exit (0);
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


    for (i = 0; i < ASL_NUM_FILES; i++)
    {
        Gbl_Files[i].Handle = NULL;
        Gbl_Files[i].Filename = NULL;
    }

    Gbl_Files[ASL_FILE_STDOUT].Handle   = stdout;
    Gbl_Files[ASL_FILE_STDOUT].Filename = "STDOUT";

    Gbl_Files[ASL_FILE_STDERR].Handle   = stderr;
    Gbl_Files[ASL_FILE_STDERR].Filename = "STDERR";

    /* Allocate the line buffer(s) */

    Gbl_LineBufferSize /= 2;
    UtExpandLineBuffers ();
}


/*******************************************************************************
 *
 * FUNCTION:    AslMergeOptionTokens
 *
 * PARAMETERS:  InBuffer            - Input containing an option string
 *              OutBuffer           - Merged output buffer
 *
 * RETURN:      None
 *
 * DESCRIPTION: Remove all whitespace from an option string.
 *
 ******************************************************************************/

static void
AslMergeOptionTokens (
    char                    *InBuffer,
    char                    *OutBuffer)
{
    char                    *Token;


    *OutBuffer = 0;

    Token = strtok (InBuffer, ASL_TOKEN_SEPARATORS);
    while (Token)
    {
        strcat (OutBuffer, Token);
        Token = strtok (NULL, ASL_TOKEN_SEPARATORS);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AslDoResponseFile
 *
 * PARAMETERS:  Filename        - Name of the response file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Open a response file and process all options within.
 *
 ******************************************************************************/

static int
AslDoResponseFile (
    char                    *Filename)
{
    char                    *argv = StringBuffer2;
    FILE                    *ResponseFile;
    int                     OptStatus = 0;
    int                     Opterr;
    int                     Optind;


    ResponseFile = fopen (Filename, "r");
    if (!ResponseFile)
    {
        printf ("Could not open command file %s, %s\n",
            Filename, strerror (errno));
        return (-1);
    }

    /* Must save the current GetOpt globals */

    Opterr = AcpiGbl_Opterr;
    Optind = AcpiGbl_Optind;

    /*
     * Process all lines in the response file. There must be one complete
     * option per line
     */
    while (fgets (StringBuffer, ASL_MSG_BUFFER_SIZE, ResponseFile))
    {
        /* Compress all tokens, allowing us to use a single argv entry */

        AslMergeOptionTokens (StringBuffer, StringBuffer2);

        /* Process the option */

        AcpiGbl_Opterr = 0;
        AcpiGbl_Optind = 0;

        OptStatus = AslDoOptions (1, &argv, TRUE);
        if (OptStatus)
        {
            printf ("Invalid option in command file %s: %s\n",
                Filename, StringBuffer);
            break;
        }
    }

    /* Restore the GetOpt globals */

    AcpiGbl_Opterr = Opterr;
    AcpiGbl_Optind = Optind;

    fclose (ResponseFile);
    return (OptStatus);
}


/*******************************************************************************
 *
 * FUNCTION:    AslDoOptions
 *
 * PARAMETERS:  argc/argv           - Standard argc/argv
 *              IsResponseFile      - TRUE if executing a response file.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Command line option processing
 *
 ******************************************************************************/

static int
AslDoOptions (
    int                     argc,
    char                    **argv,
    BOOLEAN                 IsResponseFile)
{
    int                     j;
    ACPI_STATUS             Status;


    /* Get the command line options */

    while ((j = AcpiGetopt (argc, argv, ASL_SUPPORTED_OPTIONS)) != EOF) switch (j)
    {
    case '@':   /* Begin a response file */

        if (IsResponseFile)
        {
            printf ("Nested command files are not supported\n");
            return (-1);
        }

        if (AslDoResponseFile (AcpiGbl_Optarg))
        {
            return (-1);
        }
        break;


    case 'b':   /* Debug output options */
        switch (AcpiGbl_Optarg[0])
        {
        case 'f':
            AslCompilerdebug = 1; /* same as yydebug */
            DtParserdebug = 1;
            PrParserdebug = 1;
            break;

        case 't':
            break;

        default:
            printf ("Unknown option: -b%s\n", AcpiGbl_Optarg);
            return (-1);
        }

        /* Produce debug output file */

        Gbl_DebugFlag = TRUE;
        break;


    case 'c':
        switch (AcpiGbl_Optarg[0])
        {
        case 'r':
            Gbl_NoResourceChecking = TRUE;
            break;

        default:
            printf ("Unknown option: -c%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;


    case 'd':   /* Disassembler */
        switch (AcpiGbl_Optarg[0])
        {
        case '^':
            Gbl_DoCompile = FALSE;
            break;

        case 'a':
            Gbl_DoCompile = FALSE;
            Gbl_DisassembleAll = TRUE;
            break;

        case 'b':   /* Do not convert buffers to resource descriptors */
            AcpiGbl_NoResourceDisassembly = TRUE;
            break;

        case 'c':
            break;

        default:
            printf ("Unknown option: -d%s\n", AcpiGbl_Optarg);
            return (-1);
        }

        Gbl_DisasmFlag = TRUE;
        break;


    case 'D':   /* Define a symbol */
        PrAddDefine (AcpiGbl_Optarg, NULL, TRUE);
        break;


    case 'e':   /* External files for disassembler */
        Status = AcpiDmAddToExternalFileList (AcpiGbl_Optarg);
        if (ACPI_FAILURE (Status))
        {
            printf ("Could not add %s to external list\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;


    case 'f':   /* Ignore errors and force creation of aml file */
        Gbl_IgnoreErrors = TRUE;
        break;


    case 'G':
        Gbl_CompileGeneric = TRUE;
        break;


    case 'g':   /* Get all ACPI tables */

        Gbl_GetAllTables = TRUE;
        Gbl_DoCompile = FALSE;
        break;


    case 'h':
        switch (AcpiGbl_Optarg[0])
        {
        case '^':
            Usage ();
            exit (0);

        case 'c':
            UtDisplayConstantOpcodes ();
            exit (0);

        case 'f':
            FilenameHelp ();
            exit (0);

        case 'r':
            /* reserved names */

            ApDisplayReservedNames ();
            exit (0);

        case 't':
            UtDisplaySupportedTables ();
            exit (0);

        default:
            printf ("Unknown option: -h%s\n", AcpiGbl_Optarg);
            return (-1);
        }


    case 'I':   /* Add an include file search directory */
        FlAddIncludeDirectory (AcpiGbl_Optarg);
        break;


    case 'i':   /* Output AML as an include file */
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

        case 'n':

            /* Compiler/Disassembler: Ignore the NOOP operator */

            AcpiGbl_IgnoreNoopOperator = TRUE;
            break;

        default:
            printf ("Unknown option: -i%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;


    case 'l':   /* Listing files */
        switch (AcpiGbl_Optarg[0])
        {
        case '^':
            /* Produce listing file (Mixed source/aml) */

            Gbl_ListingFlag = TRUE;
            break;

        case 'i':
            /* Produce preprocessor output file */

            Gbl_PreprocessorOutputFlag = TRUE;
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
            return (-1);
        }
        break;


    case 'm':   /* Set line buffer size */
        Gbl_LineBufferSize = (UINT32) strtoul (AcpiGbl_Optarg, NULL, 0) * 1024;
        if (Gbl_LineBufferSize < ASL_DEFAULT_LINE_BUFFER_SIZE)
        {
            Gbl_LineBufferSize = ASL_DEFAULT_LINE_BUFFER_SIZE;
        }
        printf ("Line Buffer Size: %u\n", Gbl_LineBufferSize);
        break;


    case 'n':   /* Parse only */
        Gbl_ParseOnlyFlag = TRUE;
        break;


    case 'o':   /* Control compiler AML optimizations */
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
            return (-1);
        }
        break;


    case 'P':   /* Preprocessor options */
        switch (AcpiGbl_Optarg[0])
        {
        case '^':   /* Proprocess only, emit (.i) file */
            Gbl_PreprocessOnly = TRUE;
            Gbl_PreprocessorOutputFlag = TRUE;
            break;

        case 'n':   /* Disable preprocessor */
            Gbl_PreprocessFlag = FALSE;
            break;

        default:
            printf ("Unknown option: -P%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;


    case 'p':   /* Override default AML output filename */
        Gbl_OutputFilenamePrefix = AcpiGbl_Optarg;
        Gbl_UseDefaultAmlFilename = FALSE;
        break;


    case 'r':   /* Override revision found in table header */
        Gbl_RevisionOverride = (UINT8) strtoul (AcpiGbl_Optarg, NULL, 0);
        break;


    case 's':   /* Create AML in a source code file */
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

        case 'o':

            /* Produce AML offset table in C */

            Gbl_C_OffsetTableFlag = TRUE;
            break;

        default:
            printf ("Unknown option: -s%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;


    case 't':   /* Produce hex table output file */
        switch (AcpiGbl_Optarg[0])
        {
        case 'a':
            Gbl_HexOutputFlag = HEX_OUTPUT_ASM;
            break;

        case 'c':
            Gbl_HexOutputFlag = HEX_OUTPUT_C;
            break;

        case 's':
            Gbl_HexOutputFlag = HEX_OUTPUT_ASL;
            break;

        default:
            printf ("Unknown option: -t%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;


    case 'T':   /* Create a ACPI table template file */
        Gbl_DoTemplates = TRUE;
        Gbl_TemplateSignature = AcpiGbl_Optarg;
        break;


    case 'v':   /* Version and verbosity settings */
        switch (AcpiGbl_Optarg[0])
        {
        case '^':
            printf (ACPI_COMMON_SIGNON (ASL_COMPILER_NAME));
            exit (0);

        case 'a':
            /* Disable All error/warning messages */

            Gbl_NoErrors = TRUE;
            break;

        case 'i':
            /*
             * Support for integrated development environment(s).
             *
             * 1) No compiler signon
             * 2) Send stderr messages to stdout
             * 3) Less verbose error messages (single line only for each)
             * 4) Error/warning messages are formatted appropriately to
             *    be recognized by MS Visual Studio
             */
            Gbl_VerboseErrors = FALSE;
            Gbl_DoSignon = FALSE;
            Gbl_Files[ASL_FILE_STDERR].Handle = stdout;
            break;

        case 'o':
            Gbl_DisplayOptimizations = TRUE;
            break;

        case 'r':
            Gbl_DisplayRemarks = FALSE;
            break;

        case 's':
            Gbl_DoSignon = FALSE;
            break;

        case 't':
            Gbl_VerboseTemplates = TRUE;
            break;

        default:
            printf ("Unknown option: -v%s\n", AcpiGbl_Optarg);
            return (-1);
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

        case 'e':
            Gbl_WarningsAsErrors = TRUE;
            break;

        default:
            printf ("Unknown option: -w%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;


    case 'x':   /* Set debug print output level */
        AcpiDbgLevel = strtoul (AcpiGbl_Optarg, NULL, 16);
        break;


    case 'z':
        Gbl_UseOriginalCompilerId = TRUE;
        break;


    default:
        return (-1);
    }

    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    AslCommandLine
 *
 * PARAMETERS:  argc/argv
 *
 * RETURN:      Last argv index
 *
 * DESCRIPTION: Command line processing
 *
 ******************************************************************************/

static int
AslCommandLine (
    int                     argc,
    char                    **argv)
{
    int                     BadCommandLine = 0;
    ACPI_STATUS             Status;


    /* Minimum command line contains at least the command and an input file */

    if (argc < 2)
    {
        printf (ACPI_COMMON_SIGNON (ASL_COMPILER_NAME));
        Usage ();
        exit (1);
    }

    /* Process all command line options */

    BadCommandLine = AslDoOptions (argc, argv, FALSE);

    if (Gbl_DoTemplates)
    {
        Status = DtCreateTemplates (Gbl_TemplateSignature);
        if (ACPI_FAILURE (Status))
        {
            exit (-1);
        }
        exit (1);
    }

    /* Next parameter must be the input filename */

    if (!argv[AcpiGbl_Optind] &&
        !Gbl_DisasmFlag &&
        !Gbl_GetAllTables)
    {
        printf ("Missing input filename\n");
        BadCommandLine = TRUE;
    }

    if (Gbl_DoSignon)
    {
        printf (ACPI_COMMON_SIGNON (ASL_COMPILER_NAME));
        if (Gbl_IgnoreErrors)
        {
            printf ("Ignoring all errors, forcing AML file generation\n\n");
        }
    }

    /* Abort if anything went wrong on the command line */

    if (BadCommandLine)
    {
        printf ("\n");
        Usage ();
        exit (1);
    }

    return (AcpiGbl_Optind);
}


/*******************************************************************************
 *
 * FUNCTION:    main
 *
 * PARAMETERS:  Standard argc/argv
 *
 * RETURN:      Program termination code
 *
 * DESCRIPTION: C main routine for the Asl Compiler. Handle command line
 *              options and begin the compile for each file on the command line
 *
 ******************************************************************************/

int ACPI_SYSTEM_XFACE
main (
    int                     argc,
    char                    **argv)
{
    ACPI_STATUS             Status;
    int                     Index1;
    int                     Index2;


    signal (SIGINT, AslSignalHandler);

    AcpiGbl_ExternalFileList = NULL;
    AcpiDbgLevel = 0;

#ifdef _DEBUG
    _CrtSetDbgFlag (_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF |
                    _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
#endif

    /* Init and command line */

    Index1 = Index2 = AslCommandLine (argc, argv);

    AslInitialize ();
    PrInitializePreprocessor ();

    /* Options that have no additional parameters or pathnames */

    if (Gbl_GetAllTables)
    {
        Status = AslDoOneFile (NULL);
        if (ACPI_FAILURE (Status))
        {
            return (-1);
        }
        return (0);
    }

    if (Gbl_DisassembleAll)
    {
        while (argv[Index1])
        {
            Status = AslDoOnePathname (argv[Index1], AcpiDmAddToExternalFileList);
            if (ACPI_FAILURE (Status))
            {
                return (-1);
            }

            Index1++;
        }
    }

    /* Process each pathname/filename in the list, with possible wildcards */

    while (argv[Index2])
    {
        Status = AslDoOnePathname (argv[Index2], AslDoOneFile);
        if (ACPI_FAILURE (Status))
        {
            return (-1);
        }

        Index2++;
    }

    if (AcpiGbl_ExternalFileList)
    {
        AcpiDmClearExternalFileList();
    }

    return (0);
}
