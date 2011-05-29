/******************************************************************************
 *
 * Name: acexcep.h - Exception codes returned by the ACPI subsystem
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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

#ifndef __ACEXCEP_H__
#define __ACEXCEP_H__


/*
 * Exceptions returned by external ACPI interfaces
 */
#define AE_CODE_ENVIRONMENTAL           0x0000
#define AE_CODE_PROGRAMMER              0x1000
#define AE_CODE_ACPI_TABLES             0x2000
#define AE_CODE_AML                     0x3000
#define AE_CODE_CONTROL                 0x4000
#define AE_CODE_MASK                    0xF000


#define ACPI_SUCCESS(a)                 (!(a))
#define ACPI_FAILURE(a)                 (a)


#define AE_OK                           (ACPI_STATUS) 0x0000

/*
 * Environmental exceptions
 */
#define AE_ERROR                        (ACPI_STATUS) (0x0001 | AE_CODE_ENVIRONMENTAL)
#define AE_NO_ACPI_TABLES               (ACPI_STATUS) (0x0002 | AE_CODE_ENVIRONMENTAL)
#define AE_NO_NAMESPACE                 (ACPI_STATUS) (0x0003 | AE_CODE_ENVIRONMENTAL)
#define AE_NO_MEMORY                    (ACPI_STATUS) (0x0004 | AE_CODE_ENVIRONMENTAL)
#define AE_NOT_FOUND                    (ACPI_STATUS) (0x0005 | AE_CODE_ENVIRONMENTAL)
#define AE_NOT_EXIST                    (ACPI_STATUS) (0x0006 | AE_CODE_ENVIRONMENTAL)
#define AE_ALREADY_EXISTS               (ACPI_STATUS) (0x0007 | AE_CODE_ENVIRONMENTAL)
#define AE_TYPE                         (ACPI_STATUS) (0x0008 | AE_CODE_ENVIRONMENTAL)
#define AE_NULL_OBJECT                  (ACPI_STATUS) (0x0009 | AE_CODE_ENVIRONMENTAL)
#define AE_NULL_ENTRY                   (ACPI_STATUS) (0x000A | AE_CODE_ENVIRONMENTAL)
#define AE_BUFFER_OVERFLOW              (ACPI_STATUS) (0x000B | AE_CODE_ENVIRONMENTAL)
#define AE_STACK_OVERFLOW               (ACPI_STATUS) (0x000C | AE_CODE_ENVIRONMENTAL)
#define AE_STACK_UNDERFLOW              (ACPI_STATUS) (0x000D | AE_CODE_ENVIRONMENTAL)
#define AE_NOT_IMPLEMENTED              (ACPI_STATUS) (0x000E | AE_CODE_ENVIRONMENTAL)
#define AE_SUPPORT                      (ACPI_STATUS) (0x000F | AE_CODE_ENVIRONMENTAL)
#define AE_LIMIT                        (ACPI_STATUS) (0x0010 | AE_CODE_ENVIRONMENTAL)
#define AE_TIME                         (ACPI_STATUS) (0x0011 | AE_CODE_ENVIRONMENTAL)
#define AE_ACQUIRE_DEADLOCK             (ACPI_STATUS) (0x0012 | AE_CODE_ENVIRONMENTAL)
#define AE_RELEASE_DEADLOCK             (ACPI_STATUS) (0x0013 | AE_CODE_ENVIRONMENTAL)
#define AE_NOT_ACQUIRED                 (ACPI_STATUS) (0x0014 | AE_CODE_ENVIRONMENTAL)
#define AE_ALREADY_ACQUIRED             (ACPI_STATUS) (0x0015 | AE_CODE_ENVIRONMENTAL)
#define AE_NO_HARDWARE_RESPONSE         (ACPI_STATUS) (0x0016 | AE_CODE_ENVIRONMENTAL)
#define AE_NO_GLOBAL_LOCK               (ACPI_STATUS) (0x0017 | AE_CODE_ENVIRONMENTAL)
#define AE_ABORT_METHOD                 (ACPI_STATUS) (0x0018 | AE_CODE_ENVIRONMENTAL)
#define AE_SAME_HANDLER                 (ACPI_STATUS) (0x0019 | AE_CODE_ENVIRONMENTAL)
#define AE_NO_HANDLER                   (ACPI_STATUS) (0x001A | AE_CODE_ENVIRONMENTAL)
#define AE_OWNER_ID_LIMIT               (ACPI_STATUS) (0x001B | AE_CODE_ENVIRONMENTAL)

#define AE_CODE_ENV_MAX                 0x001B


/*
 * Programmer exceptions
 */
#define AE_BAD_PARAMETER                (ACPI_STATUS) (0x0001 | AE_CODE_PROGRAMMER)
#define AE_BAD_CHARACTER                (ACPI_STATUS) (0x0002 | AE_CODE_PROGRAMMER)
#define AE_BAD_PATHNAME                 (ACPI_STATUS) (0x0003 | AE_CODE_PROGRAMMER)
#define AE_BAD_DATA                     (ACPI_STATUS) (0x0004 | AE_CODE_PROGRAMMER)
#define AE_BAD_HEX_CONSTANT             (ACPI_STATUS) (0x0005 | AE_CODE_PROGRAMMER)
#define AE_BAD_OCTAL_CONSTANT           (ACPI_STATUS) (0x0006 | AE_CODE_PROGRAMMER)
#define AE_BAD_DECIMAL_CONSTANT         (ACPI_STATUS) (0x0007 | AE_CODE_PROGRAMMER)
#define AE_MISSING_ARGUMENTS            (ACPI_STATUS) (0x0008 | AE_CODE_PROGRAMMER)
#define AE_BAD_ADDRESS                  (ACPI_STATUS) (0x0009 | AE_CODE_PROGRAMMER)

#define AE_CODE_PGM_MAX                 0x0009


/*
 * Acpi table exceptions
 */
#define AE_BAD_SIGNATURE                (ACPI_STATUS) (0x0001 | AE_CODE_ACPI_TABLES)
#define AE_BAD_HEADER                   (ACPI_STATUS) (0x0002 | AE_CODE_ACPI_TABLES)
#define AE_BAD_CHECKSUM                 (ACPI_STATUS) (0x0003 | AE_CODE_ACPI_TABLES)
#define AE_BAD_VALUE                    (ACPI_STATUS) (0x0004 | AE_CODE_ACPI_TABLES)
#define AE_INVALID_TABLE_LENGTH         (ACPI_STATUS) (0x0005 | AE_CODE_ACPI_TABLES)

#define AE_CODE_TBL_MAX                 0x0005


/*
 * AML exceptions.  These are caused by problems with
 * the actual AML byte stream
 */
#define AE_AML_BAD_OPCODE               (ACPI_STATUS) (0x0001 | AE_CODE_AML)
#define AE_AML_NO_OPERAND               (ACPI_STATUS) (0x0002 | AE_CODE_AML)
#define AE_AML_OPERAND_TYPE             (ACPI_STATUS) (0x0003 | AE_CODE_AML)
#define AE_AML_OPERAND_VALUE            (ACPI_STATUS) (0x0004 | AE_CODE_AML)
#define AE_AML_UNINITIALIZED_LOCAL      (ACPI_STATUS) (0x0005 | AE_CODE_AML)
#define AE_AML_UNINITIALIZED_ARG        (ACPI_STATUS) (0x0006 | AE_CODE_AML)
#define AE_AML_UNINITIALIZED_ELEMENT    (ACPI_STATUS) (0x0007 | AE_CODE_AML)
#define AE_AML_NUMERIC_OVERFLOW         (ACPI_STATUS) (0x0008 | AE_CODE_AML)
#define AE_AML_REGION_LIMIT             (ACPI_STATUS) (0x0009 | AE_CODE_AML)
#define AE_AML_BUFFER_LIMIT             (ACPI_STATUS) (0x000A | AE_CODE_AML)
#define AE_AML_PACKAGE_LIMIT            (ACPI_STATUS) (0x000B | AE_CODE_AML)
#define AE_AML_DIVIDE_BY_ZERO           (ACPI_STATUS) (0x000C | AE_CODE_AML)
#define AE_AML_BAD_NAME                 (ACPI_STATUS) (0x000D | AE_CODE_AML)
#define AE_AML_NAME_NOT_FOUND           (ACPI_STATUS) (0x000E | AE_CODE_AML)
#define AE_AML_INTERNAL                 (ACPI_STATUS) (0x000F | AE_CODE_AML)
#define AE_AML_INVALID_SPACE_ID         (ACPI_STATUS) (0x0010 | AE_CODE_AML)
#define AE_AML_STRING_LIMIT             (ACPI_STATUS) (0x0011 | AE_CODE_AML)
#define AE_AML_NO_RETURN_VALUE          (ACPI_STATUS) (0x0012 | AE_CODE_AML)
#define AE_AML_METHOD_LIMIT             (ACPI_STATUS) (0x0013 | AE_CODE_AML)
#define AE_AML_NOT_OWNER                (ACPI_STATUS) (0x0014 | AE_CODE_AML)
#define AE_AML_MUTEX_ORDER              (ACPI_STATUS) (0x0015 | AE_CODE_AML)
#define AE_AML_MUTEX_NOT_ACQUIRED       (ACPI_STATUS) (0x0016 | AE_CODE_AML)
#define AE_AML_INVALID_RESOURCE_TYPE    (ACPI_STATUS) (0x0017 | AE_CODE_AML)
#define AE_AML_INVALID_INDEX            (ACPI_STATUS) (0x0018 | AE_CODE_AML)
#define AE_AML_REGISTER_LIMIT           (ACPI_STATUS) (0x0019 | AE_CODE_AML)
#define AE_AML_NO_WHILE                 (ACPI_STATUS) (0x001A | AE_CODE_AML)
#define AE_AML_ALIGNMENT                (ACPI_STATUS) (0x001B | AE_CODE_AML)
#define AE_AML_NO_RESOURCE_END_TAG      (ACPI_STATUS) (0x001C | AE_CODE_AML)
#define AE_AML_BAD_RESOURCE_VALUE       (ACPI_STATUS) (0x001D | AE_CODE_AML)
#define AE_AML_CIRCULAR_REFERENCE       (ACPI_STATUS) (0x001E | AE_CODE_AML)
#define AE_AML_BAD_RESOURCE_LENGTH      (ACPI_STATUS) (0x001F | AE_CODE_AML)
#define AE_AML_ILLEGAL_ADDRESS          (ACPI_STATUS) (0x0020 | AE_CODE_AML)
#define AE_AML_INFINITE_LOOP            (ACPI_STATUS) (0x0021 | AE_CODE_AML)

#define AE_CODE_AML_MAX                 0x0021


/*
 * Internal exceptions used for control
 */
#define AE_CTRL_RETURN_VALUE            (ACPI_STATUS) (0x0001 | AE_CODE_CONTROL)
#define AE_CTRL_PENDING                 (ACPI_STATUS) (0x0002 | AE_CODE_CONTROL)
#define AE_CTRL_TERMINATE               (ACPI_STATUS) (0x0003 | AE_CODE_CONTROL)
#define AE_CTRL_TRUE                    (ACPI_STATUS) (0x0004 | AE_CODE_CONTROL)
#define AE_CTRL_FALSE                   (ACPI_STATUS) (0x0005 | AE_CODE_CONTROL)
#define AE_CTRL_DEPTH                   (ACPI_STATUS) (0x0006 | AE_CODE_CONTROL)
#define AE_CTRL_END                     (ACPI_STATUS) (0x0007 | AE_CODE_CONTROL)
#define AE_CTRL_TRANSFER                (ACPI_STATUS) (0x0008 | AE_CODE_CONTROL)
#define AE_CTRL_BREAK                   (ACPI_STATUS) (0x0009 | AE_CODE_CONTROL)
#define AE_CTRL_CONTINUE                (ACPI_STATUS) (0x000A | AE_CODE_CONTROL)
#define AE_CTRL_SKIP                    (ACPI_STATUS) (0x000B | AE_CODE_CONTROL)
#define AE_CTRL_PARSE_CONTINUE          (ACPI_STATUS) (0x000C | AE_CODE_CONTROL)
#define AE_CTRL_PARSE_PENDING           (ACPI_STATUS) (0x000D | AE_CODE_CONTROL)

#define AE_CODE_CTRL_MAX                0x000D


/* Exception strings for AcpiFormatException */

#ifdef DEFINE_ACPI_GLOBALS

/*
 * String versions of the exception codes above
 * These strings must match the corresponding defines exactly
 */
char const   *AcpiGbl_ExceptionNames_Env[] =
{
    "AE_OK",
    "AE_ERROR",
    "AE_NO_ACPI_TABLES",
    "AE_NO_NAMESPACE",
    "AE_NO_MEMORY",
    "AE_NOT_FOUND",
    "AE_NOT_EXIST",
    "AE_ALREADY_EXISTS",
    "AE_TYPE",
    "AE_NULL_OBJECT",
    "AE_NULL_ENTRY",
    "AE_BUFFER_OVERFLOW",
    "AE_STACK_OVERFLOW",
    "AE_STACK_UNDERFLOW",
    "AE_NOT_IMPLEMENTED",
    "AE_SUPPORT",
    "AE_LIMIT",
    "AE_TIME",
    "AE_ACQUIRE_DEADLOCK",
    "AE_RELEASE_DEADLOCK",
    "AE_NOT_ACQUIRED",
    "AE_ALREADY_ACQUIRED",
    "AE_NO_HARDWARE_RESPONSE",
    "AE_NO_GLOBAL_LOCK",
    "AE_ABORT_METHOD",
    "AE_SAME_HANDLER",
    "AE_NO_HANDLER",
    "AE_OWNER_ID_LIMIT"
};

char const   *AcpiGbl_ExceptionNames_Pgm[] =
{
    NULL,
    "AE_BAD_PARAMETER",
    "AE_BAD_CHARACTER",
    "AE_BAD_PATHNAME",
    "AE_BAD_DATA",
    "AE_BAD_HEX_CONSTANT",
    "AE_BAD_OCTAL_CONSTANT",
    "AE_BAD_DECIMAL_CONSTANT",
    "AE_MISSING_ARGUMENTS",
    "AE_BAD_ADDRESS"
};

char const   *AcpiGbl_ExceptionNames_Tbl[] =
{
    NULL,
    "AE_BAD_SIGNATURE",
    "AE_BAD_HEADER",
    "AE_BAD_CHECKSUM",
    "AE_BAD_VALUE",
    "AE_INVALID_TABLE_LENGTH"
};

char const   *AcpiGbl_ExceptionNames_Aml[] =
{
    NULL,
    "AE_AML_BAD_OPCODE",
    "AE_AML_NO_OPERAND",
    "AE_AML_OPERAND_TYPE",
    "AE_AML_OPERAND_VALUE",
    "AE_AML_UNINITIALIZED_LOCAL",
    "AE_AML_UNINITIALIZED_ARG",
    "AE_AML_UNINITIALIZED_ELEMENT",
    "AE_AML_NUMERIC_OVERFLOW",
    "AE_AML_REGION_LIMIT",
    "AE_AML_BUFFER_LIMIT",
    "AE_AML_PACKAGE_LIMIT",
    "AE_AML_DIVIDE_BY_ZERO",
    "AE_AML_BAD_NAME",
    "AE_AML_NAME_NOT_FOUND",
    "AE_AML_INTERNAL",
    "AE_AML_INVALID_SPACE_ID",
    "AE_AML_STRING_LIMIT",
    "AE_AML_NO_RETURN_VALUE",
    "AE_AML_METHOD_LIMIT",
    "AE_AML_NOT_OWNER",
    "AE_AML_MUTEX_ORDER",
    "AE_AML_MUTEX_NOT_ACQUIRED",
    "AE_AML_INVALID_RESOURCE_TYPE",
    "AE_AML_INVALID_INDEX",
    "AE_AML_REGISTER_LIMIT",
    "AE_AML_NO_WHILE",
    "AE_AML_ALIGNMENT",
    "AE_AML_NO_RESOURCE_END_TAG",
    "AE_AML_BAD_RESOURCE_VALUE",
    "AE_AML_CIRCULAR_REFERENCE",
    "AE_AML_BAD_RESOURCE_LENGTH",
    "AE_AML_ILLEGAL_ADDRESS",
    "AE_AML_INFINITE_LOOP"
};

char const   *AcpiGbl_ExceptionNames_Ctrl[] =
{
    NULL,
    "AE_CTRL_RETURN_VALUE",
    "AE_CTRL_PENDING",
    "AE_CTRL_TERMINATE",
    "AE_CTRL_TRUE",
    "AE_CTRL_FALSE",
    "AE_CTRL_DEPTH",
    "AE_CTRL_END",
    "AE_CTRL_TRANSFER",
    "AE_CTRL_BREAK",
    "AE_CTRL_CONTINUE",
    "AE_CTRL_SKIP",
    "AE_CTRL_PARSE_CONTINUE",
    "AE_CTRL_PARSE_PENDING"
};

#endif /* ACPI GLOBALS */

#endif /* __ACEXCEP_H__ */
