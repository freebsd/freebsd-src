/******************************************************************************
 * Xen Hypervisor Filesystem
 *
 * Copyright (c) 2019, SUSE Software Solutions Germany GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __XEN_PUBLIC_HYPFS_H__
#define __XEN_PUBLIC_HYPFS_H__

#include "xen.h"

/*
 * Definitions for the __HYPERVISOR_hypfs_op hypercall.
 */

/* Highest version number of the hypfs interface currently defined. */
#define XEN_HYPFS_VERSION      1

/* Maximum length of a path in the filesystem. */
#define XEN_HYPFS_MAX_PATHLEN  1024

struct xen_hypfs_direntry {
    uint8_t type;
#define XEN_HYPFS_TYPE_DIR     0
#define XEN_HYPFS_TYPE_BLOB    1
#define XEN_HYPFS_TYPE_STRING  2
#define XEN_HYPFS_TYPE_UINT    3
#define XEN_HYPFS_TYPE_INT     4
#define XEN_HYPFS_TYPE_BOOL    5
    uint8_t encoding;
#define XEN_HYPFS_ENC_PLAIN    0
#define XEN_HYPFS_ENC_GZIP     1
    uint16_t pad;              /* Returned as 0. */
    uint32_t content_len;      /* Current length of data. */
    uint32_t max_write_len;    /* Max. length for writes (0 if read-only). */
};
typedef struct xen_hypfs_direntry xen_hypfs_direntry_t;

struct xen_hypfs_dirlistentry {
    xen_hypfs_direntry_t e;
    /* Offset in bytes to next entry (0 == this is the last entry). */
    uint16_t off_next;
    /* Zero terminated entry name, possibly with some padding for alignment. */
    char name[XEN_FLEX_ARRAY_DIM];
};

/*
 * Hypercall operations.
 */

/*
 * XEN_HYPFS_OP_get_version
 *
 * Read highest interface version supported by the hypervisor.
 *
 * arg1 - arg4: all 0/NULL
 *
 * Possible return values:
 * >0: highest supported interface version
 * <0: negative Xen errno value
 */
#define XEN_HYPFS_OP_get_version     0

/*
 * XEN_HYPFS_OP_read
 *
 * Read a filesystem entry.
 *
 * Returns the direntry and contents of an entry in the buffer supplied by the
 * caller (struct xen_hypfs_direntry with the contents following directly
 * after it).
 * The data buffer must be at least the size of the direntry returned. If the
 * data buffer was not large enough for all the data -ENOBUFS and no entry
 * data is returned, but the direntry will contain the needed size for the
 * returned data.
 * The format of the contents is according to its entry type and encoding.
 * The contents of a directory are multiple struct xen_hypfs_dirlistentry
 * items.
 *
 * arg1: XEN_GUEST_HANDLE(path name)
 * arg2: length of path name (including trailing zero byte)
 * arg3: XEN_GUEST_HANDLE(data buffer written by hypervisor)
 * arg4: data buffer size
 *
 * Possible return values:
 * 0: success
 * <0 : negative Xen errno value
 */
#define XEN_HYPFS_OP_read              1

/*
 * XEN_HYPFS_OP_write_contents
 *
 * Write contents of a filesystem entry.
 *
 * Writes an entry with the contents of a buffer supplied by the caller.
 * The data type and encoding can't be changed. The size can be changed only
 * for blobs and strings.
 *
 * arg1: XEN_GUEST_HANDLE(path name)
 * arg2: length of path name (including trailing zero byte)
 * arg3: XEN_GUEST_HANDLE(content buffer read by hypervisor)
 * arg4: content buffer size
 *
 * Possible return values:
 * 0: success
 * <0 : negative Xen errno value
 */
#define XEN_HYPFS_OP_write_contents    2

#endif /* __XEN_PUBLIC_HYPFS_H__ */
