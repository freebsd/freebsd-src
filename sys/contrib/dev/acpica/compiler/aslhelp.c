/******************************************************************************
 *
 * Module Name: aslhelp - iASL help screens
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslhelp")


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
    ACPI_OPTION ("-p <prefix>",     "Specify path/filename prefix for all output files");
    ACPI_OPTION ("-v",              "Display compiler version");
    ACPI_OPTION ("-vd",             "Display compiler build date and time");
    ACPI_OPTION ("-vo",             "Enable optimization comments");
    ACPI_OPTION ("-vs",             "Disable signon");

    printf ("\nHelp:\n");
    ACPI_OPTION ("-h",              "This message");
    ACPI_OPTION ("-hc",             "Display operators allowed in constant expressions");
    ACPI_OPTION ("-hd",             "Info for obtaining and disassembling binary ACPI tables");
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
    ACPI_OPTION ("-w <1|2|3>",      "Set warning reporting level");
    ACPI_OPTION ("-we",             "Report warnings as errors");

    printf ("\nAML Code Generation (*.aml):\n");
    ACPI_OPTION ("-oa",             "Disable all optimizations (compatibility mode)");
    ACPI_OPTION ("-of",             "Disable constant folding");
    ACPI_OPTION ("-oi",             "Disable integer optimization to Zero/One/Ones");
    ACPI_OPTION ("-on",             "Disable named reference string optimization");
    ACPI_OPTION ("-ot",             "Disable typechecking");
    ACPI_OPTION ("-cr",             "Disable Resource Descriptor error checking");
    ACPI_OPTION ("-in",             "Ignore NoOp operators");
    ACPI_OPTION ("-r <revision>",   "Override table header Revision (1-255)");

    printf ("\nListings:\n");
    ACPI_OPTION ("-l",              "Create mixed listing file (ASL source and AML) (*.lst)");
    ACPI_OPTION ("-lm",             "Create hardware summary map file (*.map)");
    ACPI_OPTION ("-ln",             "Create namespace file (*.nsp)");
    ACPI_OPTION ("-ls",             "Create combined source file (expanded includes) (*.src)");
    ACPI_OPTION ("-lx",             "Create cross-reference file (*.xrf)");

    printf ("\nFirmware Support - C Output:\n");
    ACPI_OPTION ("-tc",             "Create hex AML table in C (*.hex)");
    ACPI_OPTION ("-sc",             "Create named hex AML arrays in C (*.c)");
    ACPI_OPTION ("-ic",             "Create include file in C for -sc symbols (*.h)");
    ACPI_OPTION ("-so",             "Create namespace AML offset table in C (*.offset.h)");

    printf ("\nFirmware Support - Assembler Output:\n");
    ACPI_OPTION ("-ta",             "Create hex AML table in assembler (*.hex)");
    ACPI_OPTION ("-sa",             "Create named hex AML arrays in assembler (*.asm)");
    ACPI_OPTION ("-ia",             "Create include file in assembler for -sa symbols (*.inc)");

    printf ("\nFirmware Support - ASL Output:\n");
    ACPI_OPTION ("-ts",             "Create hex AML table in ASL (Buffer object) (*.hex)");

    printf ("\nData Table Compiler:\n");
    ACPI_OPTION ("-G",              "Compile custom table that contains generic operators");
    ACPI_OPTION ("-T <sig list>|ALL",   "Create ACPI table template/example files");
    ACPI_OPTION ("-T <count>",      "Emit DSDT and <count> SSDTs to same file");
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
    ACPI_OPTION ("-oc",             "Display compile times and statistics");
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

/*******************************************************************************
 *
 * FUNCTION:    AslDisassemblyHelp
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display help message for obtaining and disassembling AML/ASL
 *              files.
 *
 ******************************************************************************/

void
AslDisassemblyHelp (
    void)
{

    printf ("\nObtaining binary ACPI tables and disassembling to ASL source code.\n\n");
    printf ("Use the following ACPICA toolchain:\n");
    printf ("  AcpiDump: Dump all ACPI tables to a hex ascii file\n");
    printf ("  AcpiXtract: Extract one or more binary ACPI tables from AcpiDump output\n");
    printf ("  iASL -d <file>: Disassemble a binary ACPI table to ASL source code\n");
    printf ("\n");
}
