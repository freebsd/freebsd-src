/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017 Mark Johnston <markj@FreeBSD.org>
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _ACPI_ACPI_H_
#define _ACPI_ACPI_H_

/*
 * FreeBSD import of ACPICA has a typedef for BOOLEAN which conflicts with
 * amdgpu driver. Workaround it on preprocessor level.
 */
#define	ACPI_USE_SYSTEM_INTTYPES
#define	BOOLEAN			unsigned char
typedef unsigned char		UINT8;
typedef unsigned short		UINT16;
typedef short			INT16;
typedef unsigned int		UINT32;
typedef int			INT32;
typedef uint64_t		UINT64;
typedef int64_t			INT64;
#include <contrib/dev/acpica/include/acpi.h>
#undef BOOLEAN

typedef ACPI_HANDLE		acpi_handle;
typedef ACPI_OBJECT		acpi_object;
typedef ACPI_OBJECT_HANDLER	acpi_object_handler;
typedef ACPI_OBJECT_TYPE	acpi_object_type;
typedef ACPI_STATUS		acpi_status;
typedef ACPI_STRING		acpi_string;
typedef ACPI_SIZE		acpi_size;
typedef ACPI_WALK_CALLBACK	acpi_walk_callback;

static inline ACPI_STATUS
acpi_evaluate_object(ACPI_HANDLE Object, ACPI_STRING Pathname,
    ACPI_OBJECT_LIST *ParameterObjects, ACPI_BUFFER *ReturnObjectBuffer)
{
	return (AcpiEvaluateObject(
	    Object, Pathname, ParameterObjects, ReturnObjectBuffer));
}

static inline const char *
acpi_format_exception(ACPI_STATUS Exception)
{
	return (AcpiFormatException(Exception));
}

static inline ACPI_STATUS
acpi_get_handle(ACPI_HANDLE Parent, ACPI_STRING Pathname,
    ACPI_HANDLE *RetHandle)
{
	return (AcpiGetHandle(Parent, Pathname, RetHandle));
}

static inline ACPI_STATUS
acpi_get_data(ACPI_HANDLE ObjHandle, ACPI_OBJECT_HANDLER Handler, void **Data)
{
	return (AcpiGetData(ObjHandle, Handler, Data));
}

static inline ACPI_STATUS
acpi_get_name(ACPI_HANDLE Object, UINT32 NameType, ACPI_BUFFER *RetPathPtr)
{
	return (AcpiGetName(Object, NameType, RetPathPtr));
}

static inline ACPI_STATUS
acpi_get_table(ACPI_STRING Signature, UINT32 Instance,
    ACPI_TABLE_HEADER **OutTable)
{
	return (AcpiGetTable(Signature, Instance, OutTable));
}

#endif /* _ACPI_ACPI_H_ */
