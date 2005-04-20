/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: types.h,v 1.33 2002/07/19 03:39:44 marka Exp $ */

#ifndef ISC_TYPES_H
#define ISC_TYPES_H 1

/*
 * OS-specific types, from the OS-specific include directories.
 */
#include <isc/int.h>
#include <isc/offset.h>

/*
 * XXXDCL should isc_boolean_t be moved here, requiring an explicit include
 * of <isc/boolean.h> when ISC_TRUE/ISC_FALSE/ISC_TF() are desired?
 */
#include <isc/boolean.h>
/*
 * XXXDCL This is just for ISC_LIST and ISC_LINK, but gets all of the other
 * list macros too.
 */
#include <isc/list.h>

/***
 *** Core Types.  Alphabetized by defined type.
 ***/

typedef struct isc_bitstring		isc_bitstring_t;
typedef struct isc_buffer		isc_buffer_t;
typedef ISC_LIST(isc_buffer_t)		isc_bufferlist_t;
typedef struct isc_constregion		isc_constregion_t;
typedef struct isc_consttextregion	isc_consttextregion_t;
typedef struct isc_entropy		isc_entropy_t;
typedef struct isc_entropysource	isc_entropysource_t;
typedef struct isc_event		isc_event_t;
typedef ISC_LIST(isc_event_t)		isc_eventlist_t;
typedef unsigned int			isc_eventtype_t;
typedef isc_uint32_t			isc_fsaccess_t;
typedef struct isc_interface		isc_interface_t;
typedef struct isc_interfaceiter	isc_interfaceiter_t;
typedef struct isc_interval		isc_interval_t;
typedef struct isc_lex			isc_lex_t;
typedef struct isc_log 			isc_log_t;
typedef struct isc_logcategory		isc_logcategory_t;
typedef struct isc_logconfig		isc_logconfig_t;
typedef struct isc_logmodule		isc_logmodule_t;
typedef struct isc_mem			isc_mem_t;
typedef struct isc_mempool		isc_mempool_t;
typedef struct isc_msgcat		isc_msgcat_t;
typedef struct isc_ondestroy		isc_ondestroy_t;
typedef struct isc_netaddr		isc_netaddr_t;
typedef struct isc_quota		isc_quota_t;
typedef struct isc_random		isc_random_t;
typedef struct isc_ratelimiter		isc_ratelimiter_t;
typedef struct isc_region		isc_region_t;
typedef isc_uint64_t			isc_resourcevalue_t;
typedef unsigned int			isc_result_t;
typedef struct isc_rwlock		isc_rwlock_t;
typedef struct isc_sockaddr		isc_sockaddr_t;
typedef struct isc_socket		isc_socket_t;
typedef struct isc_socketevent		isc_socketevent_t;
typedef struct isc_socketmgr		isc_socketmgr_t;
typedef struct isc_symtab		isc_symtab_t;
typedef struct isc_task			isc_task_t;
typedef ISC_LIST(isc_task_t)		isc_tasklist_t;
typedef struct isc_taskmgr		isc_taskmgr_t;
typedef struct isc_textregion		isc_textregion_t;
typedef struct isc_time			isc_time_t;
typedef struct isc_timer		isc_timer_t;
typedef struct isc_timermgr		isc_timermgr_t;

typedef void (*isc_taskaction_t)(isc_task_t *, isc_event_t *);

typedef enum {
	isc_resource_coresize = 1,
	isc_resource_cputime,
	isc_resource_datasize,
	isc_resource_filesize,
	isc_resource_lockedmemory,
	isc_resource_openfiles,
	isc_resource_processes,
	isc_resource_residentsize,
	isc_resource_stacksize
} isc_resource_t;

#endif /* ISC_TYPES_H */
