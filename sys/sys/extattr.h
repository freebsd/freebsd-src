/*-
 * Copyright (c) 1999-2001 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
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
 * $FreeBSD$
 */
/*
 * Developed by the TrustedBSD Project.
 * Support for extended filesystem attributes.
 */

#ifndef _SYS_EXTATTR_H_
#define	_SYS_EXTATTR_H_

/*
 * Defined name spaces for extended attributes.  Numeric constants are passed
 * via system calls, but a user-friendly string is also defined.
 */
#define	EXTATTR_NAMESPACE_EMPTY		0x00000000
#define	EXTATTR_NAMESPACE_EMPTY_STRING	"empty"
#define	EXTATTR_NAMESPACE_USER		0x00000001
#define	EXTATTR_NAMESPACE_USER_STRING	"user"
#define	EXTATTR_NAMESPACE_SYSTEM	0x00000002
#define	EXTATTR_NAMESPACE_SYSTEM_STRING	"system"

/*
 * The following macro is designed to initialize an array that maps
 * extended-attribute namespace values to their names, e.g.,
 * char *extattr_namespace_names[] = EXTATTR_NAMESPACE_NAMES;
 */
#define EXTATTR_NAMESPACE_NAMES { \
	EXTATTR_NAMESPACE_EMPTY_STRING, \
	EXTATTR_NAMESPACE_USER_STRING, \
	EXTATTR_NAMESPACE_SYSTEM_STRING }

/*
 * This structure defines the required fields of an extended-attribute header.
 */
struct extattr {
	int32_t	ea_length;	    /* length of this attribute */
	int8_t	ea_namespace;	    /* name space of this attribute */
	int8_t	ea_contentpadlen;   /* bytes of padding at end of attribute */
	int8_t	ea_namelength;	    /* length of attribute name */
	char	ea_name[1];	    /* null-terminated attribute name */
	/* extended attribute content follows */
};

/*
 * These macros are used to access and manipulate an extended attribute:
 *
 * EXTATTR_NEXT(eap) returns a pointer to the next extended attribute
 *	following eap.
 * EXTATTR_CONTENT(eap) returns a pointer to the extended attribute
 *	content referenced by eap.
 * EXTATTR_CONTENT_SIZE(eap) returns the size of the extended attribute
 *	content referenced by eap.
 * EXTATTR_SET_LENGTHS(eap, contentsize) called after initializing the
 *	attribute name to calculate and set the ea_length, ea_namelength,
 *	and ea_contentpadlen fields of the extended attribute structure.
 */
#define EXTATTR_NEXT(eap) \
	((struct extattr *)(((void *)(eap)) + (eap)->ea_length))
#define EXTATTR_CONTENT(eap) (((void *)(eap)) + EXTATTR_BASE_LENGTH(eap))
#define EXTATTR_CONTENT_SIZE(eap) \
	((eap)->ea_length - EXTATTR_BASE_LENGTH(eap) - (eap)->ea_contentpadlen)
#define EXTATTR_BASE_LENGTH(eap) \
	((sizeof(struct extattr) + (eap)->ea_namelength + 7) & ~7)
#define EXTATTR_SET_LENGTHS(eap, contentsize) do { \
	KASSERT(((eap)->ea_name[0] != 0), \
		("Must initialize name before setting lengths")); \
	(eap)->ea_namelength = strlen((eap)->ea_name); \
	(eap)->ea_contentpadlen = ((contentsize) % 8) ? \
		8 - ((contentsize) % 8) : 0; \
	(eap)->ea_length = EXTATTR_BASE_LENGTH(eap) + \
		(contentsize) + (eap)->ea_contentpadlen; \
} while (0)

#ifdef _KERNEL

#define	EXTATTR_MAXNAMELEN	NAME_MAX
struct thread;
struct ucred;
struct vnode;
int	extattr_check_cred(struct vnode *vp, int attrnamespace,
	    struct ucred *cred, struct thread *td, int access);

#else
#include <sys/cdefs.h>

/* User-level definition of KASSERT for macros above */
#define KASSERT(cond, str) do { \
        if (cond) { printf("panic: "); printf(str); printf("\n"); exit(1); } \
} while (0)

struct iovec;

__BEGIN_DECLS
int	extattrctl(const char *_path, int _cmd, const char *_filename,
	    int _attrnamespace, const char *_attrname);
int	extattr_delete_fd(int _fd, int _attrnamespace, const char *_attrname);
int	extattr_delete_file(const char *_path, int _attrnamespace,
	    const char *_attrname);
int	extattr_delete_link(const char *_path, int _attrnamespace,
	    const char *_attrname);
ssize_t	extattr_get_fd(int _fd, int _attrnamespace, const char *_attrname,
	    void *_data, size_t _nbytes);
ssize_t	extattr_get_file(const char *_path, int _attrnamespace,
	    const char *_attrname, void *_data, size_t _nbytes);
ssize_t	extattr_get_link(const char *_path, int _attrnamespace,
	    const char *_attrname, void *_data, size_t _nbytes);
ssize_t	extattr_list_fd(int _fd, int _attrnamespace, void *_data,
	    size_t _nbytes);
ssize_t	extattr_list_file(const char *_path, int _attrnamespace, void *_data,
	    size_t _nbytes);
ssize_t	extattr_list_link(const char *_path, int _attrnamespace, void *_data,
	    size_t _nbytes);
int	extattr_set_fd(int _fd, int _attrnamespace, const char *_attrname,
	    const void *_data, size_t _nbytes);
int	extattr_set_file(const char *_path, int _attrnamespace,
	    const char *_attrname, const void *_data, size_t _nbytes);
int	extattr_set_link(const char *_path, int _attrnamespace,
	    const char *_attrname, const void *_data, size_t _nbytes);
__END_DECLS

#endif /* !_KERNEL */
#endif /* !_SYS_EXTATTR_H_ */
