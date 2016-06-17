/*
 * cs_types.h 1.18 2000/06/12 21:55:40
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License. 
 *
 * The initial developer of the original code is David A. Hinds
 * <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
 * are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the MPL or the GPL.
 */

#ifndef _LINUX_CS_TYPES_H
#define _LINUX_CS_TYPES_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <sys/types.h>
#endif

#if defined(__arm__) || defined(__mips__)
typedef u_int   ioaddr_t;
#else
typedef u_short	ioaddr_t;
#endif

typedef u_short	socket_t;
typedef u_int	event_t;
typedef u_char	cisdata_t;
typedef u_short	page_t;

struct client_t;
typedef struct client_t *client_handle_t;

struct window_t;
typedef struct window_t *window_handle_t;

struct region_t;
typedef struct region_t *memory_handle_t;

struct eraseq_t;
typedef struct eraseq_t *eraseq_handle_t;

#ifndef DEV_NAME_LEN
#define DEV_NAME_LEN 32
#endif

typedef char dev_info_t[DEV_NAME_LEN];

#endif /* _LINUX_CS_TYPES_H */
