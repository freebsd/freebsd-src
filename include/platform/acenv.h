/******************************************************************************
 *
 * Name: acenv.h - Generation environment specific items
 *       $Revision: 1.131 $
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

#ifndef __ACENV_H__
#define __ACENV_H__


/*
 * Configuration for ACPI tools and utilities
 */

#ifdef ACPI_LIBRARY
/*
 * Note: The non-debug version of the AcpiLibrary does not contain any
 * debug support, for minimimal size. The debug version uses ACPI_FULL_DEBUG
 */
#define ACPI_USE_LOCAL_CACHE
#endif

#ifdef ACPI_ASL_COMPILER
#define ACPI_DEBUG_OUTPUT
#define ACPI_APPLICATION
#define ACPI_DISASSEMBLER
#define ACPI_CONSTANT_EVAL_ONLY
#define ACPI_LARGE_NAMESPACE_NODE
#define ACPI_DATA_TABLE_DISASSEMBLY
#endif

#ifdef ACPI_EXEC_APP
#undef DEBUGGER_THREADING
#define DEBUGGER_THREADING      DEBUGGER_SINGLE_THREADED
#define ACPI_FULL_DEBUG
#define ACPI_APPLICATION
#define ACPI_DEBUGGER
#define ACPI_MUTEX_DEBUG
#define ACPI_DBG_TRACK_ALLOCATIONS
#endif

#ifdef ACPI_DASM_APP
#ifndef MSDOS
#define ACPI_DEBUG_OUTPUT
#endif
#define ACPI_APPLICATION
#define ACPI_DISASSEMBLER
#define ACPI_NO_METHOD_EXECUTION
#define ACPI_LARGE_NAMESPACE_NODE
#define ACPI_DATA_TABLE_DISASSEMBLY
#endif

#ifdef ACPI_APPLICATION
#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_USE_LOCAL_CACHE
#endif

#ifdef ACPI_FULL_DEBUG
#define ACPI_DEBUGGER
#define ACPI_DEBUG_OUTPUT
#define ACPI_DISASSEMBLER
#endif


/*
 * Environment configuration.  The purpose of this file is to interface to the
 * local generation environment.
 *
 * 1) ACPI_USE_SYSTEM_CLIBRARY - Define this if linking to an actual C library.
 *      Otherwise, local versions of string/memory functions will be used.
 * 2) ACPI_USE_STANDARD_HEADERS - Define this if linking to a C library and
 *      the standard header files may be used.
 *
 * The ACPI subsystem only uses low level C library functions that do not call
 * operating system services and may therefore be inlined in the code.
 *
 * It may be necessary to tailor these include files to the target
 * generation environment.
 *
 *
 * Functions and constants used from each header:
 *
 * string.h:    memcpy
 *              memset
 *              strcat
 *              strcmp
 *              strcpy
 *              strlen
 *              strncmp
 *              strncat
 *              strncpy
 *
 * stdlib.h:    strtoul
 *
 * stdarg.h:    va_list
 *              va_arg
 *              va_start
 *              va_end
 *
 */

/*! [Begin] no source code translation */

#if defined(__linux__)
#include "aclinux.h"

#elif defined(_AED_EFI)
#include "acefi.h"

#elif defined(WIN32)
#include "acwin.h"

#elif defined(WIN64)
#include "acwin64.h"

#elif defined(MSDOS)        /* Must appear after WIN32 and WIN64 check */
#include "acdos16.h"

#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <contrib/dev/acpica/acfreebsd.h>

#elif defined(__NetBSD__)
#include "acnetbsd.h"

#elif defined(MODESTO)
#include "acmodesto.h"

#elif defined(NETWARE)
#include "acnetware.h"

#elif defined(__sun)
#include "acsolaris.h"

#else

/* All other environments */

#define ACPI_USE_STANDARD_HEADERS

#define COMPILER_DEPENDENT_INT64   long long
#define COMPILER_DEPENDENT_UINT64  unsigned long long

#endif

/*! [End] no source code translation !*/


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
#endif /* !DEBUGGER_THREADING */


/******************************************************************************
 *
 * C library configuration
 *
 *****************************************************************************/

#define ACPI_IS_ASCII(c)  ((c) < 0x80)

#ifdef ACPI_USE_SYSTEM_CLIBRARY
/*
 * Use the standard C library headers.
 * We want to keep these to a minimum.
 */
#ifdef ACPI_USE_STANDARD_HEADERS
/*
 * Use the standard headers from the standard locations
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#endif /* ACPI_USE_STANDARD_HEADERS */

/*
 * We will be linking to the standard Clib functions
 */
#define ACPI_STRSTR(s1,s2)      strstr((s1), (s2))
#define ACPI_STRCHR(s1,c)       strchr((s1), (c))
#define ACPI_STRLEN(s)          (ACPI_SIZE) strlen((s))
#define ACPI_STRCPY(d,s)        (void) strcpy((d), (s))
#define ACPI_STRNCPY(d,s,n)     (void) strncpy((d), (s), (ACPI_SIZE)(n))
#define ACPI_STRNCMP(d,s,n)     strncmp((d), (s), (ACPI_SIZE)(n))
#define ACPI_STRCMP(d,s)        strcmp((d), (s))
#define ACPI_STRCAT(d,s)        (void) strcat((d), (s))
#define ACPI_STRNCAT(d,s,n)     strncat((d), (s), (ACPI_SIZE)(n))
#define ACPI_STRTOUL(d,s,n)     strtoul((d), (s), (ACPI_SIZE)(n))
#define ACPI_MEMCMP(s1,s2,n)    memcmp((const char *)(s1), (const char *)(s2), (ACPI_SIZE)(n))
#define ACPI_MEMCPY(d,s,n)      (void) memcpy((d), (s), (ACPI_SIZE)(n))
#define ACPI_MEMSET(d,s,n)      (void) memset((d), (s), (ACPI_SIZE)(n))

#define ACPI_TOUPPER(i)         toupper((int) (i))
#define ACPI_TOLOWER(i)         tolower((int) (i))
#define ACPI_IS_XDIGIT(i)       isxdigit((int) (i))
#define ACPI_IS_DIGIT(i)        isdigit((int) (i))
#define ACPI_IS_SPACE(i)        isspace((int) (i))
#define ACPI_IS_UPPER(i)        isupper((int) (i))
#define ACPI_IS_PRINT(i)        isprint((int) (i))
#define ACPI_IS_ALPHA(i)        isalpha((int) (i))

#else

/******************************************************************************
 *
 * Not using native C library, use local implementations
 *
 *****************************************************************************/

 /*
 * Use local definitions of C library macros and functions
 * NOTE: The function implementations may not be as efficient
 * as an inline or assembly code implementation provided by a
 * native C library.
 */

#ifndef va_arg

#ifndef _VALIST
#define _VALIST
typedef char *va_list;
#endif /* _VALIST */

/*
 * Storage alignment properties
 */
#define  _AUPBND                (sizeof (ACPI_NATIVE_INT) - 1)
#define  _ADNBND                (sizeof (ACPI_NATIVE_INT) - 1)

/*
 * Variable argument list macro definitions
 */
#define _Bnd(X, bnd)            (((sizeof (X)) + (bnd)) & (~(bnd)))
#define va_arg(ap, T)           (*(T *)(((ap) += (_Bnd (T, _AUPBND))) - (_Bnd (T,_ADNBND))))
#define va_end(ap)              (void) 0
#define va_start(ap, A)         (void) ((ap) = (((char *) &(A)) + (_Bnd (A,_AUPBND))))

#endif /* va_arg */


#define ACPI_STRSTR(s1,s2)      AcpiUtStrstr ((s1), (s2))
#define ACPI_STRCHR(s1,c)       AcpiUtStrchr ((s1), (c))
#define ACPI_STRLEN(s)          (ACPI_SIZE) AcpiUtStrlen ((s))
#define ACPI_STRCPY(d,s)        (void) AcpiUtStrcpy ((d), (s))
#define ACPI_STRNCPY(d,s,n)     (void) AcpiUtStrncpy ((d), (s), (ACPI_SIZE)(n))
#define ACPI_STRNCMP(d,s,n)     AcpiUtStrncmp ((d), (s), (ACPI_SIZE)(n))
#define ACPI_STRCMP(d,s)        AcpiUtStrcmp ((d), (s))
#define ACPI_STRCAT(d,s)        (void) AcpiUtStrcat ((d), (s))
#define ACPI_STRNCAT(d,s,n)     AcpiUtStrncat ((d), (s), (ACPI_SIZE)(n))
#define ACPI_STRTOUL(d,s,n)     AcpiUtStrtoul ((d), (s), (ACPI_SIZE)(n))
#define ACPI_MEMCMP(s1,s2,n)    AcpiUtMemcmp((const char *)(s1), (const char *)(s2), (ACPI_SIZE)(n))
#define ACPI_MEMCPY(d,s,n)      (void) AcpiUtMemcpy ((d), (s), (ACPI_SIZE)(n))
#define ACPI_MEMSET(d,v,n)      (void) AcpiUtMemset ((d), (v), (ACPI_SIZE)(n))
#define ACPI_TOUPPER            AcpiUtToUpper
#define ACPI_TOLOWER            AcpiUtToLower

#endif /* ACPI_USE_SYSTEM_CLIBRARY */


/******************************************************************************
 *
 * Assembly code macros
 *
 *****************************************************************************/

/*
 * Handle platform- and compiler-specific assembly language differences.
 * These should already have been defined by the platform includes above.
 *
 * Notes:
 * 1) Interrupt 3 is used to break into a debugger
 * 2) Interrupts are turned off during ACPI register setup
 */

/* Unrecognized compiler, use defaults */

#ifndef ACPI_ASM_MACROS

/*
 * Calling conventions:
 *
 * ACPI_SYSTEM_XFACE        - Interfaces to host OS (handlers, threads)
 * ACPI_EXTERNAL_XFACE      - External ACPI interfaces
 * ACPI_INTERNAL_XFACE      - Internal ACPI interfaces
 * ACPI_INTERNAL_VAR_XFACE  - Internal variable-parameter list interfaces
 */
#define ACPI_SYSTEM_XFACE
#define ACPI_EXTERNAL_XFACE
#define ACPI_INTERNAL_XFACE
#define ACPI_INTERNAL_VAR_XFACE

#define ACPI_ASM_MACROS
#define BREAKPOINT3
#define ACPI_DISABLE_IRQS()
#define ACPI_ENABLE_IRQS()
#define ACPI_ACQUIRE_GLOBAL_LOCK(GLptr, Acq)
#define ACPI_RELEASE_GLOBAL_LOCK(GLptr, Acq)

#endif /* ACPI_ASM_MACROS */


#ifdef ACPI_APPLICATION

/* Don't want software interrupts within a ring3 application */

#undef BREAKPOINT3
#define BREAKPOINT3
#endif


/******************************************************************************
 *
 * Compiler-specific information is contained in the compiler-specific
 * headers.
 *
 *****************************************************************************/
#endif /* __ACENV_H__ */
