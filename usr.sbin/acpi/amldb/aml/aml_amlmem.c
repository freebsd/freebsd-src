/*-
 * Copyright (c) 1999, 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	$Id: aml_amlmem.c,v 1.15 2000/08/09 14:47:43 iwasaki Exp $
 *	$FreeBSD$
 */

/*
 * AML Namespace Memory Management
 */

#include <sys/param.h>

#include <dev/acpi/aml/aml_env.h>
#include <dev/acpi/aml/aml_memman.h>
#include <dev/acpi/aml/aml_name.h>

MEMMAN_INITIALSTORAGE_DESC(struct aml_namestr, _aml_namestr_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_num, _aml_num_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_string, _aml_string_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_buffer, _aml_buffer_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_package, _aml_package_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_field, _aml_field_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_method, _aml_method_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_mutex, _aml_mutex_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_opregion, _aml_opregion_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_powerres, _aml_powerres_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_processor, _aml_processor_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_bufferfield, _aml_bufferfield_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_event, _aml_event_storage);
MEMMAN_INITIALSTORAGE_DESC(enum aml_objtype, _aml_objtype_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_name, _aml_name_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_name_group, _aml_name_group_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_objref, _aml_objref_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_regfield, _aml_regfield_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_environ, _aml_environ_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_local_stack, _aml_local_stack_storage);
MEMMAN_INITIALSTORAGE_DESC(struct aml_mutex_queue, _aml_mutex_queue_storage);

struct	memman_blockman aml_blockman[] = {
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_namestr), _aml_namestr_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_num), _aml_num_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_string), _aml_string_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_buffer), _aml_buffer_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_package), _aml_package_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_field), _aml_field_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_method), _aml_method_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_mutex), _aml_mutex_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_opregion), _aml_opregion_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_powerres), _aml_powerres_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_processor), _aml_processor_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_bufferfield), _aml_bufferfield_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_event), _aml_event_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(enum aml_objtype), _aml_objtype_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_name), _aml_name_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_name_group), _aml_name_group_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_objref), _aml_objref_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_regfield), _aml_regfield_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_environ), _aml_environ_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_local_stack), _aml_local_stack_storage),
	MEMMAN_MEMBLOCK_DESC(sizeof(struct aml_mutex_queue), _aml_mutex_queue_storage),
};

struct	memman_histogram aml_histogram[MEMMAN_HISTOGRAM_SIZE];

static struct	memman _aml_memman = MEMMAN_MEMMANAGER_DESC(aml_blockman, 21,
    aml_histogram, 1);

struct	memman *aml_memman = &_aml_memman;

