/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Mark Johnston <markj@FreeBSD.org>
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
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
 */

#ifndef _LINUXKPI_ACPI_ACPI_H_
#define _LINUXKPI_ACPI_ACPI_H_

/*
 * LINUXKPI_WANT_LINUX_ACPI is a temporary workaround to allow drm-kmod
 * to update all needed branches without breaking builds.
 * Once that happened and checks are implemented based on __FreeBSD_version
 * we will remove these conditions again.
 */

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

typedef	ACPI_IO_ADDRESS		acpi_io_address;
typedef ACPI_HANDLE		acpi_handle;
typedef ACPI_OBJECT_HANDLER	acpi_object_handler;
typedef ACPI_OBJECT_TYPE	acpi_object_type;
typedef ACPI_STATUS		acpi_status;
typedef ACPI_STRING		acpi_string;
typedef ACPI_SIZE		acpi_size;
typedef ACPI_WALK_CALLBACK	acpi_walk_callback;

union linuxkpi_acpi_object {
	acpi_object_type type;
	struct {
		acpi_object_type type;
		UINT64 value;
	} integer;
	struct {
		acpi_object_type type;
		UINT32 length;
		char *pointer;
	} string;
	struct {
		acpi_object_type type;
		UINT32 length;
		UINT8 *pointer;
	} buffer;
	struct {
		acpi_object_type type;
		UINT32 count;
		union linuxkpi_acpi_object *elements;
	} package;
	struct {
		acpi_object_type type;
		acpi_object_type actual_type;
		acpi_handle handle;
	} reference;
	struct {
		acpi_object_type type;
		UINT32 proc_id;
		acpi_io_address pblk_address;
		UINT32 pblk_length;
	} processor;
	struct {
		acpi_object_type type;
		UINT32 system_level;
		UINT32 resource_order;
	} power_resource;
};

#ifdef	LINUXKPI_WANT_LINUX_ACPI
struct linuxkpi_acpi_buffer {
	acpi_size length;	/* Length in bytes of the buffer */
	void *pointer;		/* pointer to buffer */
};

typedef	struct linuxkpi_acpi_buffer	lkpi_acpi_buffer_t;
#else
typedef	ACPI_BUFFER			lkpi_acpi_buffer_t;
#endif

static inline ACPI_STATUS
acpi_evaluate_object(ACPI_HANDLE Object, ACPI_STRING Pathname,
    ACPI_OBJECT_LIST *ParameterObjects, lkpi_acpi_buffer_t *ReturnObjectBuffer)
{
	return (AcpiEvaluateObject(
	    Object, Pathname, ParameterObjects, (ACPI_BUFFER *)ReturnObjectBuffer));
}

static inline const char *
acpi_format_exception(ACPI_STATUS Exception)
{
	return (AcpiFormatException(Exception));
}

static inline ACPI_STATUS
acpi_get_handle(ACPI_HANDLE Parent, const char *Pathname,
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
acpi_get_name(ACPI_HANDLE Object, UINT32 NameType, lkpi_acpi_buffer_t *RetPathPtr)
{
	return (AcpiGetName(Object, NameType, (ACPI_BUFFER *)RetPathPtr));
}

static inline ACPI_STATUS
acpi_get_table(ACPI_STRING Signature, UINT32 Instance,
    ACPI_TABLE_HEADER **OutTable)
{
	return (AcpiGetTable(Signature, Instance, OutTable));
}

static inline void
acpi_put_table(ACPI_TABLE_HEADER *Table)
{
	AcpiPutTable(Table);
}

#ifdef	LINUXKPI_WANT_LINUX_ACPI
#define	acpi_object		linuxkpi_acpi_object
#define	acpi_buffer		linuxkpi_acpi_buffer
#endif

#endif /* _LINUXKPI_ACPI_ACPI_H_ */
