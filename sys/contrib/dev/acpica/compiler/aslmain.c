/******************************************************************************
 *
 * Module Name: aslmain - compiler main and utilities
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslmain")

/*
 * Main routine for the iASL compiler.
 *
 * Portability note: The compiler depends upon the host for command-line
 * wildcard support - it is not implemented locally. For example:
 *
 * Linux/Unix systems: Shell expands wildcards automatically.
 *
 * Windows: The setargv.obj module must be linked in to automatically
 * expand wildcards.
 */

/* Local prototypes */

static void ACPI_SYSTEM_XFACE
AslSignalHandler (
    int                     Sig);

static void
AslInitialize (
    void);

UINT8
AcpiIsBigEndianMachine (
    void);


/*******************************************************************************
 *
 * FUNCTION:    AcpiIsBigEndianMachine
 *
 * PARAMETERS:  None
 *
 * RETURN:      TRUE if machine is big endian
 *              FALSE if machine is little endian
 *
 * DESCRIPTION: Detect whether machine is little endian or big endian.
 *
 ******************************************************************************/

UINT8
AcpiIsBigEndianMachine (
    void)
{
    union {
        UINT32              Integer;
        UINT8               Bytes[4];
    } Overlay = {0xFF000000};

    return (Overlay.Bytes[0]); /* Returns 0xFF (TRUE) for big endian */
}


/*******************************************************************************
 *
 * FUNCTION:    Usage
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display option help message.
 *              Optional items in square brackets.
 *
 ******************************************************************************/

void
Usage (
    void)
{
    printf ("%s\n\n", ASL_COMPLIANCE);
    ACPI_USAGE_HEADER ("iasl [Options] [Files]");

    printf ("\nGeneral:\n");
    ACPI_OPTION ("-@ <file>",       "Specify command file");
    ACPI_OPTION ("-I <dir>",        "Specify additional include directory");
    ACPI_OPTION ("-T <sig>|ALL|*",  "Create table template file for ACPI <Sig>");
    ACPI_OPTION ("-p <prefix>",     "Specify path/filename prefix for all output files");
    ACPI_OPTION ("-v",              "Display compiler version");
    ACPI_OPTION ("-vo",             "Enable optimization comments");
    ACPI_OPTION ("-vs",             "Disable signon");

    printf ("\nHelp:\n");
    ACPI_OPTION ("-h",              "This message");
    ACPI_OPTION ("-hc",             "Display operators allowed in constant expressions");
    ACPI_OPTION ("-hf",             "Display help for output filename generation");
    ACPI_OPTION ("-hr",             "Display ACPI reserved method names");
    ACPI_OPTION ("-ht",             "Display currently supported ACPI table names");

    printf ("\nPreprocessor:\n");
    ACPI_OPTION ("-D <symbol>",     "Define symbol for preprocessor use");
    ACPI_OPTION ("-li",             "Create preprocessed output file (*.i)");
    ACPI_OPTION ("-P",              "Preprocess only and create preprocessor output file (*.i)");
    ACPI_OPTION ("-Pn",             "Disable preprocessor");

    printf ("\nErrors, Warnings, and Remarks:\n");
    ACPI_OPTION ("-va",             "Disable all errors/warnings/remarks");
    ACPI_OPTION ("-ve",             "Report only errors (ignore warnings and remarks)");
    ACPI_OPTION ("-vi",             "Less verbose errors and warnings for use with IDEs");
    ACPI_OPTION ("-vr",             "Disable remarks");
    ACPI_OPTION ("-vw <messageid>", "Disable specific warning or remark");
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
    ACPI_OPTION ("-lm",             "Create hardware summary map file (*.map)");
    ACPI_OPTION ("-ln",             "Create namespace file (*.nsp)");
    ACPI_OPTION ("-ls",             "Create combined source file (expanded includes) (*.src)");

    printf ("\nData Table Compiler:\n");
    ACPI_OPTION ("-G",              "Compile custom table that contains generic operators");
    ACPI_OPTION ("-vt",             "Create verbose template files (full disassembly)");

    printf ("\nAML Disassembler:\n");
    ACPI_OPTION ("-d  <f1 f2 ...>", "Disassemble or decode binary ACPI tables to file (*.dsl)");
    ACPI_OPTION ("",                "  (Optional, file type is automatically detected)");
    ACPI_OPTION ("-da <f1 f2 ...>", "Disassemble multiple tables from single namespace");
    ACPI_OPTION ("-db",             "Do not translate Buffers to Resource Templates");
    ACPI_OPTION ("-dc <f1 f2 ...>", "Disassemble AML and immediately compile it");
    ACPI_OPTION ("",                "  (Obtain DSDT from current system if no input file)");
    ACPI_OPTION ("-df",             "Force disassembler to assume table contains valid AML");
    ACPI_OPTION ("-dl",             "Emit legacy ASL code only (no C-style operators)");
    ACPI_OPTION ("-e  <f1 f2 ...>", "Include ACPI table(s) for external symbol resolution");
    ACPI_OPTION ("-fe <file>",      "Specify external symbol declaration file");
    ACPI_OPTION ("-in",             "Ignore NoOp opcodes");
    ACPI_OPTION ("-l",              "Disassemble to mixed ASL and AML code");
    ACPI_OPTION ("-vt",             "Dump binary table data in hex format within output file");

    printf ("\nDebug Options:\n");
    ACPI_OPTION ("-bf",             "Create debug file (full output) (*.txt)");
    ACPI_OPTION ("-bs",             "Create debug file (parse tree only) (*.txt)");
    ACPI_OPTION ("-bp <depth>",     "Prune ASL parse tree");
    ACPI_OPTION ("-bt <type>",      "Object type to be pruned from the parse tree");
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

void
AslFilenameHelp (
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

    Gbl_Files[ASL_FILE_PREPROCESSOR].Handle = NULL; /* the .pre file is same as source file */

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


    AcpiGbl_DmOpt_Verbose = FALSE;

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
    int                     ReturnStatus = 0;


    /*
     * Big-endian machines are not currently supported. ACPI tables must
     * be little-endian, and support for big-endian machines needs to
     * be implemented.
     */
    if (AcpiIsBigEndianMachine ())
    {
        fprintf (stderr,
            "iASL is not currently supported on big-endian machines.\n");
        return (-1);
    }

    AcpiOsInitialize ();
    ACPI_DEBUG_INITIALIZE (); /* For debug version only */

    /* Initialize preprocessor and compiler before command line processing */

    signal (SIGINT, AslSignalHandler);
    AcpiGbl_ExternalFileList = NULL;
    AcpiDbgLevel = 0;
    PrInitializePreprocessor ();
    AslInitialize ();

    Index1 = Index2 = AslCommandLine (argc, argv);

    /* Allocate the line buffer(s), must be after command line */

    Gbl_LineBufferSize /= 2;
    UtExpandLineBuffers ();

    /* Perform global actions first/only */

    if (Gbl_DisassembleAll)
    {
        while (argv[Index1])
        {
            Status = AcpiDmAddToExternalFileList (argv[Index1]);
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
        /*
         * If -p not specified, we will use the input filename as the
         * output filename prefix
         */
        if (Gbl_UseDefaultAmlFilename)
        {
            Gbl_OutputFilenamePrefix = argv[Index2];
            UtConvertBackslashes (Gbl_OutputFilenamePrefix);
        }

        Status = AslDoOneFile (argv[Index2]);
        if (ACPI_FAILURE (Status))
        {
            ReturnStatus = -1;
            goto CleanupAndExit;
        }

        Index2++;
    }


CleanupAndExit:

    UtFreeLineBuffers ();
    AslParserCleanup ();

    if (AcpiGbl_ExternalFileList)
    {
        AcpiDmClearExternalFileList();
    }

    return (ReturnStatus);
}
