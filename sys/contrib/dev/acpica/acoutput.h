/******************************************************************************
 *
 * Name: acoutput.h -- debug output
 *       $Revision: 77 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
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

#ifndef __ACOUTPUT_H__
#define __ACOUTPUT_H__

/*
 * Debug levels and component IDs.  These are used to control the
 * granularity of the output of the DEBUG_PRINT macro -- on a per-
 * component basis and a per-exception-type basis.
 */

/* Component IDs -- used in the global "DebugLayer" */

#define ACPI_UTILITIES              0x00000001
#define ACPI_HARDWARE               0x00000002
#define ACPI_EVENTS                 0x00000004
#define ACPI_TABLES                 0x00000008
#define ACPI_NAMESPACE              0x00000010
#define ACPI_PARSER                 0x00000020
#define ACPI_DISPATCHER             0x00000040
#define ACPI_EXECUTER               0x00000080
#define ACPI_RESOURCES              0x00000100
#define ACPI_DEVICES                0x00000200
#define ACPI_POWER                  0x00000400


#define ACPI_BUS_MANAGER            0x00001000
#define ACPI_POWER_CONTROL          0x00002000
#define ACPI_EMBEDDED_CONTROLLER    0x00004000
#define ACPI_PROCESSOR_CONTROL      0x00008000
#define ACPI_AC_ADAPTER             0x00010000
#define ACPI_BATTERY                0x00020000
#define ACPI_BUTTON                 0x00040000
#define ACPI_SYSTEM                 0x00080000
#define ACPI_THERMAL_ZONE           0x00100000

#define ACPI_DEBUGGER               0x01000000
#define ACPI_OS_SERVICES            0x02000000
#define ACPI_ALL_COMPONENTS         0x01FFFFFF

#define ACPI_COMPONENT_DEFAULT      (ACPI_ALL_COMPONENTS)


#define ACPI_COMPILER               0x10000000
#define ACPI_TOOLS                  0x20000000


/* Exception level -- used in the global "DebugLevel" */

#define ACPI_OK                     0x00000001
#define ACPI_INFO                   0x00000002
#define ACPI_WARN                   0x00000004
#define ACPI_ERROR                  0x00000008
#define ACPI_FATAL                  0x00000010
#define ACPI_DEBUG_OBJECT           0x00000020
#define ACPI_ALL                    0x0000003F


/* Trace level -- also used in the global "DebugLevel" */

#define TRACE_THREADS               0x00000080
#define TRACE_PARSE                 0x00000100
#define TRACE_DISPATCH              0x00000200
#define TRACE_LOAD                  0x00000400
#define TRACE_EXEC                  0x00000800
#define TRACE_NAMES                 0x00001000
#define TRACE_OPREGION              0x00002000
#define TRACE_BFIELD                0x00004000
#define TRACE_TRASH                 0x00008000
#define TRACE_TABLES                0x00010000
#define TRACE_FUNCTIONS             0x00020000
#define TRACE_VALUES                0x00040000
#define TRACE_OBJECTS               0x00080000
#define TRACE_ALLOCATIONS           0x00100000
#define TRACE_RESOURCES             0x00200000
#define TRACE_IO                    0x00400000
#define TRACE_INTERRUPTS            0x00800000
#define TRACE_USER_REQUESTS         0x01000000
#define TRACE_PACKAGE               0x02000000
#define TRACE_MUTEX                 0x04000000
#define TRACE_INIT                  0x08000000

#define TRACE_ALL                   0x0FFFFF80


/* Exceptionally verbose output -- also used in the global "DebugLevel"  */

#define VERBOSE_AML_DISASSEMBLE     0x10000000
#define VERBOSE_INFO                0x20000000
#define VERBOSE_TABLES              0x40000000
#define VERBOSE_EVENTS              0x80000000

#define VERBOSE_ALL                 0xF0000000


/* Defaults for DebugLevel, debug and normal */

#define DEBUG_DEFAULT               (ACPI_OK | ACPI_WARN | ACPI_ERROR | ACPI_DEBUG_OBJECT)
#define NORMAL_DEFAULT              (ACPI_OK | ACPI_WARN | ACPI_ERROR | ACPI_DEBUG_OBJECT)
#define DEBUG_ALL                   (VERBOSE_AML_DISASSEMBLE | TRACE_ALL | ACPI_ALL)

/* Misc defines */

#define HEX                         0x01
#define ASCII                       0x02
#define FULL_ADDRESS                0x04
#define CHARS_PER_LINE              16          /* used in DumpBuf function */


#endif /* __ACOUTPUT_H__ */
