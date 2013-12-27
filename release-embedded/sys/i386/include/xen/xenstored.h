/*
 * Simple prototyle Xen Store Daemon providing simple tree-like database.
 * Copyright (C) 2005 Rusty Russell IBM Corporation
 *
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef _XENSTORED_H
#define _XENSTORED_H

enum xsd_sockmsg_type
{
	XS_DEBUG,
	XS_SHUTDOWN,
	XS_DIRECTORY,
	XS_READ,
	XS_GET_PERMS,
	XS_WATCH,
	XS_WATCH_ACK,
	XS_UNWATCH,
	XS_TRANSACTION_START,
	XS_TRANSACTION_END,
	XS_OP_READ_ONLY = XS_TRANSACTION_END,
	XS_INTRODUCE,
	XS_RELEASE,
	XS_GETDOMAINPATH,
	XS_WRITE,
	XS_MKDIR,
	XS_RM,
	XS_SET_PERMS,
	XS_WATCH_EVENT,
	XS_ERROR,
};

#define XS_WRITE_NONE "NONE"
#define XS_WRITE_CREATE "CREATE"
#define XS_WRITE_CREATE_EXCL "CREATE|EXCL"

/* We hand errors as strings, for portability. */
struct xsd_errors
{
	int errnum;
	const char *errstring;
};
#define XSD_ERROR(x) { x, #x }
static struct xsd_errors xsd_errors[] __attribute__((unused)) = {
	XSD_ERROR(EINVAL),
	XSD_ERROR(EACCES),
	XSD_ERROR(EEXIST),
	XSD_ERROR(EISDIR),
	XSD_ERROR(ENOENT),
	XSD_ERROR(ENOMEM),
	XSD_ERROR(ENOSPC),
	XSD_ERROR(EIO),
	XSD_ERROR(ENOTEMPTY),
	XSD_ERROR(ENOSYS),
	XSD_ERROR(EROFS),
	XSD_ERROR(EBUSY),
	XSD_ERROR(ETIMEDOUT),
	XSD_ERROR(EISCONN),
};
struct xsd_sockmsg
{
	uint32_t type;
	uint32_t len; 		/* Length of data following this. */

	/* Generally followed by nul-terminated string(s). */
};

#endif /* _XENSTORED_H */
