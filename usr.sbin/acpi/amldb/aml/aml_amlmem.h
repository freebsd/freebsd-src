/*-
 * Copyright (c) 1999 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$Id: aml_amlmem.h,v 1.12 2000/08/08 14:12:05 iwasaki Exp $
 *	$FreeBSD$
 */

#ifndef _AML_AMLMEM_H_
#define _AML_AMLMEM_H_

/*
 * AML Namespace Memory Management
 */

#include <dev/acpi/aml/aml_memman.h>

enum {
	memid_aml_namestr = 0,
	memid_aml_num,
	memid_aml_string,
	memid_aml_buffer,
	memid_aml_package,
	memid_aml_field,
	memid_aml_method,
	memid_aml_mutex,
	memid_aml_opregion,
	memid_aml_powerres,
	memid_aml_processor,
	memid_aml_bufferfield,
	memid_aml_event,
	memid_aml_objtype,
	memid_aml_name,
	memid_aml_name_group,
	memid_aml_objref,
	memid_aml_regfield,
	memid_aml_environ,
	memid_aml_local_stack,
	memid_aml_mutex_queue,
};

extern struct	memman	*aml_memman;

#endif /* !_AML_AMLMEM_H_ */
