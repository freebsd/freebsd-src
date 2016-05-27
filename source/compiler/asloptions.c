/******************************************************************************
 *
 * Module Name: asloptions - compiler command line processing
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

#include "aslcompiler.h"
#include "acapps.h"
#include "acdisasm.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asloption")


/* Local prototypes */

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
#define ASL_SUPPORTED_OPTIONS   "@:a:b|c|d^D:e:f^gh^i|I:l^m:no|p:P^r:s|t|T+G^v^w|x:z"


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

int
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
        Status = DtCreateTemplates (argv);
        if (ACPI_FAILURE (Status))
        {
            exit (-1);
        }
        exit (1);
    }

    /* Next parameter must be the input filename */

    if (!argv[AcpiGbl_Optind] &&
        !Gbl_DisasmFlag)
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

    if (BadCommandLine)
    {
        printf ("Use -h option for help information\n");
        exit (1);
    }

    return (AcpiGbl_Optind);
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
    ACPI_STATUS             Status;
    UINT32                  j;


    /* Get the command line options */

    while ((j = AcpiGetopt (argc, argv, ASL_SUPPORTED_OPTIONS)) != ACPI_OPT_END) switch (j)
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

    case 'a':   /* Debug options */

        switch (AcpiGbl_Optarg[0])
        {
        case 'r':

            Gbl_EnableReferenceTypechecking = TRUE;
            break;

        default:

            printf ("Unknown option: -a%s\n", AcpiGbl_Optarg);
            return (-1);
        }

        break;


    case 'b':   /* Debug options */

        switch (AcpiGbl_Optarg[0])
        {
        case 'f':

            AslCompilerdebug = 1; /* same as yydebug */
            DtParserdebug = 1;
            PrParserdebug = 1;
            Gbl_DebugFlag = TRUE;
            Gbl_KeepPreprocessorTempFile = TRUE;
            break;

        case 'p':   /* Prune ASL parse tree */

            /* Get the required argument */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            Gbl_PruneParseTree = TRUE;
            Gbl_PruneDepth = (UINT8) strtoul (AcpiGbl_Optarg, NULL, 0);
            break;

        case 's':

            Gbl_DebugFlag = TRUE;
            break;

        case 't':

            /* Get the required argument */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            Gbl_PruneType = (UINT8) strtoul (AcpiGbl_Optarg, NULL, 0);
            break;

        default:

            printf ("Unknown option: -b%s\n", AcpiGbl_Optarg);
            return (-1);
        }

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

        case 'f':

            AcpiGbl_ForceAmlDisassembly = TRUE;
            break;

        case 'l':   /* Use legacy ASL code (not ASL+) for disassembly */

            Gbl_DoCompile = FALSE;
            AcpiGbl_CstyleDisassembly = FALSE;
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

        /* Get entire list of external files */

        AcpiGbl_Optind--;
        argv[AcpiGbl_Optind] = AcpiGbl_Optarg;

        while (argv[AcpiGbl_Optind] &&
              (argv[AcpiGbl_Optind][0] != '-'))
        {
            Status = AcpiDmAddToExternalFileList (argv[AcpiGbl_Optind]);
            if (ACPI_FAILURE (Status))
            {
                printf ("Could not add %s to external list\n",
                    argv[AcpiGbl_Optind]);
                return (-1);
            }

            AcpiGbl_Optind++;
        }
        break;

    case 'f':

        switch (AcpiGbl_Optarg[0])
        {
        case '^':   /* Ignore errors and force creation of aml file */

            Gbl_IgnoreErrors = TRUE;
            break;

        case 'e':   /* Disassembler: Get external declaration file */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            Gbl_ExternalRefFilename = AcpiGbl_Optarg;
            break;

        default:

            printf ("Unknown option: -f%s\n", AcpiGbl_Optarg);
            return (-1);
        }
        break;

    case 'G':

        Gbl_CompileGeneric = TRUE;
        break;

    case 'g':   /* Get all ACPI tables */

        printf ("-g option is deprecated, use acpidump utility instead\n");
        exit (1);

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

            AslFilenameHelp ();
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
            AcpiGbl_DmOpt_Listing = TRUE;
            break;

        case 'i':

            /* Produce preprocessor output file */

            Gbl_PreprocessorOutputFlag = TRUE;
            break;

        case 'm':

            /* Produce hardware map summary file */

            Gbl_MapfileFlag = TRUE;
            break;

        case 'n':

            /* Produce namespace file */

            Gbl_NsOutputFlag = TRUE;
            break;

        case 's':

            /* Produce combined source file */

            Gbl_SourceOutputFlag = TRUE;
            break;

        case 'x':

            /* Produce cross-reference file */

            Gbl_CrossReferenceOutput = TRUE;
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

        case 'c':

            /* Display compile time(s) */

            Gbl_CompileTimesFlag = TRUE;
            break;

        case 'e':

            /* iASL: Disable External opcode generation */

            Gbl_DoExternals = FALSE;

            /* Disassembler: Emit embedded external operators */

            AcpiGbl_DmEmitExternalOpcodes = TRUE;
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

            /* Disable heavy typechecking */

            Gbl_DoTypechecking = FALSE;
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
        UtConvertBackslashes (Gbl_OutputFilenamePrefix);
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
        break;

    case 'v':   /* Version and verbosity settings */

        switch (AcpiGbl_Optarg[0])
        {
        case '^':

            printf (ACPI_COMMON_SIGNON (ASL_COMPILER_NAME));
            exit (0);

        case 'a':

            /* Disable all error/warning/remark messages */

            Gbl_NoErrors = TRUE;
            break;

        case 'e':

            /* Disable all warning/remark messages (errors only) */

            Gbl_DisplayRemarks = FALSE;
            Gbl_DisplayWarnings = FALSE;
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

        case 'w':

            /* Get the required argument */

            if (AcpiGetoptArgument (argc, argv))
            {
                return (-1);
            }

            Status = AslDisableException (AcpiGbl_Optarg);
            if (ACPI_FAILURE (Status))
            {
                return (-1);
            }
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
