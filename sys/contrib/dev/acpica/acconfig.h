/******************************************************************************
 *
 * Name: acconfig.h - Global configuration constants
 *       $Revision: 94 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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

#ifndef _ACCONFIG_H
#define _ACCONFIG_H


/******************************************************************************
 *
 * Compile-time options
 *
 *****************************************************************************/

/*
 * ACPI_DEBUG           - This switch enables all the debug facilities of the
 *                        ACPI subsystem.  This includes the DEBUG_PRINT output
 *                        statements.  When disabled, all DEBUG_PRINT
 *                        statements are compiled out.
 *
 * ACPI_APPLICATION     - Use this switch if the subsystem is going to be run
 *                        at the application level.
 *
 */


/******************************************************************************
 *
 * Subsystem Constants
 *
 *****************************************************************************/


/* Version string */

#define ACPI_CA_VERSION             0x20020308

/* Version of ACPI supported */

#define ACPI_CA_SUPPORT_LEVEL       2

/* Maximum objects in the various object caches */

#define MAX_STATE_CACHE_DEPTH       64          /* State objects for stacks */
#define MAX_PARSE_CACHE_DEPTH       96          /* Parse tree objects */
#define MAX_EXTPARSE_CACHE_DEPTH    64          /* Parse tree objects */
#define MAX_OBJECT_CACHE_DEPTH      64          /* Interpreter operand objects */
#define MAX_WALK_CACHE_DEPTH        4           /* Objects for parse tree walks */

/* String size constants */

#define MAX_STRING_LENGTH           512
#define PATHNAME_MAX                256         /* A full namespace pathname */

/* Maximum count for a semaphore object */

#define MAX_SEMAPHORE_COUNT         256

/* Max reference count (for debug only) */

#define MAX_REFERENCE_COUNT         0x400

/* Size of cached memory mapping for system memory operation region */

#define SYSMEM_REGION_WINDOW_SIZE   4096


/******************************************************************************
 *
 * Configuration of subsystem behavior
 *
 *****************************************************************************/

/*
 * Debugger threading model
 * Use single threaded if the entire subsystem is contained in an application
 * Use multiple threaded when the subsystem is running in the kernel.
 *
 * By default the model is single threaded if ACPI_APPLICATION is set,
 * multi-threaded if ACPI_APPLICATION is not set.
 */
#define DEBUGGER_SINGLE_THREADED    0
#define DEBUGGER_MULTI_THREADED     1

#ifndef DEBUGGER_THREADING
#ifdef ACPI_APPLICATION
#define DEBUGGER_THREADING          DEBUGGER_SINGLE_THREADED

#else
#define DEBUGGER_THREADING          DEBUGGER_MULTI_THREADED
#endif
#endif

/*
 * Should the subystem abort the loading of an ACPI table if the
 * table checksum is incorrect?
 */
#define ACPI_CHECKSUM_ABORT         FALSE


/******************************************************************************
 *
 * ACPI Specification constants (Do not change unless the specification changes)
 *
 *****************************************************************************/

/* Number of distinct GPE register blocks */

#define ACPI_MAX_GPE_BLOCKS         2

/*
 * Method info (in WALK_STATE), containing local variables and argumetns
 */
#define MTH_NUM_LOCALS              8
#define MTH_MAX_LOCAL               7

#define MTH_NUM_ARGS                7
#define MTH_MAX_ARG                 6

/* Maximum length of resulting string when converting from a buffer */

#define ACPI_MAX_STRING_CONVERSION  200

/*
 * Operand Stack (in WALK_STATE), Must be large enough to contain MTH_MAX_ARG
 */
#define OBJ_NUM_OPERANDS            8
#define OBJ_MAX_OPERAND             7

/* Names within the namespace are 4 bytes long */

#define ACPI_NAME_SIZE              4
#define PATH_SEGMENT_LENGTH         5           /* 4 chars for name + 1 char for separator */
#define PATH_SEPARATOR              '.'

/* Constants used in searching for the RSDP in low memory */

#define LO_RSDP_WINDOW_BASE         0           /* Physical Address */
#define HI_RSDP_WINDOW_BASE         0xE0000     /* Physical Address */
#define LO_RSDP_WINDOW_SIZE         0x400
#define HI_RSDP_WINDOW_SIZE         0x20000
#define RSDP_SCAN_STEP              16

/* Operation regions */

#define ACPI_NUM_PREDEFINED_REGIONS 8
#define ACPI_USER_REGION_BEGIN      0x80

/* Maximum SpaceIds for Operation Regions */

#define ACPI_MAX_ADDRESS_SPACE      255

/* RSDP checksums */

#define ACPI_RSDP_CHECKSUM_LENGTH   20
#define ACPI_RSDP_XCHECKSUM_LENGTH  36


/******************************************************************************
 *
 * ACPI AML Debugger
 *
 *****************************************************************************/


#define ACPI_DEBUGGER_MAX_ARGS             8  /* Must be max method args + 1 */

#define ACPI_DEBUGGER_COMMAND_PROMPT      '-'
#define ACPI_DEBUGGER_EXECUTE_PROMPT      '%'


#endif /* _ACCONFIG_H */

