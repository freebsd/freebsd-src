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
 * Support for POSIX.1e access control lists.
 */

#ifndef _SYS_ACL_H
#define	_SYS_ACL_H

/*
 * POSIX.1e ACL types and related constants.
 */

#define	POSIX1E_ACL_ACCESS_EXTATTR_NAMESPACE	EXTATTR_NAMESPACE_SYSTEM
#define	POSIX1E_ACL_ACCESS_EXTATTR_NAME		"posix1e.acl_access"
#define	POSIX1E_ACL_DEFAULT_EXTATTR_NAMESPACE	EXTATTR_NAMESPACE_SYSTEM
#define	POSIX1E_ACL_DEFAULT_EXTATTR_NAME	"posix1e.acl_default"
#define	ACL_MAX_ENTRIES		32 /* maximum entries in an ACL */

typedef int	acl_type_t;
typedef int	acl_tag_t;
typedef mode_t	acl_perm_t;
typedef mode_t *acl_permset_t;

struct acl_entry {
	acl_tag_t	ae_tag;
	uid_t		ae_id;
	acl_perm_t	ae_perm;
};
typedef struct acl_entry	*acl_entry_t;

/* internal ACL structure */
struct acl {
	int			acl_cnt;
	struct acl_entry	acl_entry[ACL_MAX_ENTRIES];
};

/* external ACL structure */
struct acl_t_struct {
	struct acl		ats_acl;
	int			ats_cur_entry;
};
typedef struct acl_t_struct *acl_t;

/*
 * Possible valid values for ae_tag field.
 */
#define	ACL_UNDEFINED_TAG	0x00000000
#define	ACL_USER_OBJ		0x00000001
#define	ACL_USER		0x00000002
#define	ACL_GROUP_OBJ		0x00000004
#define	ACL_GROUP		0x00000008
#define	ACL_MASK		0x00000010
#define	ACL_OTHER		0x00000020
#define	ACL_OTHER_OBJ		ACL_OTHER

/*
 * Possible valid values for acl_type_t arguments.
 */
#define	ACL_TYPE_ACCESS		0x00000000
#define	ACL_TYPE_DEFAULT	0x00000001
#define	ACL_TYPE_AFS		0x00000002
#define	ACL_TYPE_CODA		0x00000003
#define	ACL_TYPE_NTFS		0x00000004
#define	ACL_TYPE_NWFS		0x00000005

/*
 * Possible flags in ae_perm field.
 */
#define	ACL_EXECUTE		0x0001
#define	ACL_WRITE		0x0002
#define	ACL_READ		0x0004
#define	ACL_PERM_NONE		0x0000
#define	ACL_PERM_BITS		(ACL_EXECUTE | ACL_WRITE | ACL_READ)
#define	ACL_POSIX1E_BITS	(ACL_EXECUTE | ACL_WRITE | ACL_READ)

/*
 * Possible entry_id values for acl_get_entry()
 */
#define	ACL_FIRST_ENTRY		0
#define	ACL_NEXT_ENTRY		1

/*
 * Undefined value in ae_id field
 */
#define	ACL_UNDEFINED_ID	((uid_t)-1)


#ifdef _KERNEL

/*
 * POSIX.1e ACLs are capable of expressing the read, write, and execute
 * bits of the POSIX mode field.  We provide two masks: one that defines
 * the bits the ACL will replace in the mode, and the other that defines
 * the bits that must be preseved when an ACL is updating a mode.
 */
#define	ACL_OVERRIDE_MASK	(S_IRWXU | S_IRWXG | S_IRWXO)
#define	ACL_PRESERVE_MASK	(~ACL_OVERRIDE_MASK)

/*
 * Storage for ACLs and support structures.
 */
#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_ACL);
#endif

/*
 * File system independent code to move back and forth between POSIX mode
 * and POSIX.1e ACL representations.
 */
acl_perm_t		acl_posix1e_mode_to_perm(acl_tag_t tag, mode_t mode);
struct acl_entry	acl_posix1e_mode_to_entry(acl_tag_t tag, uid_t uid,
			    gid_t gid, mode_t mode);
mode_t			acl_posix1e_perms_to_mode(
			    struct acl_entry *acl_user_obj_entry,
			    struct acl_entry *acl_group_obj_entry,
			    struct acl_entry *acl_other_entry);
mode_t			acl_posix1e_acl_to_mode(struct acl *acl);
mode_t			acl_posix1e_newfilemode(mode_t cmode,
			    struct acl *dacl);

/*
 * File system independent syntax check for a POSIX.1e ACL.
 */
int			acl_posix1e_check(struct acl *acl);

#else /* !_KERNEL */

/*
 * Syscall interface -- use the library calls instead as the syscalls
 * have strict acl entry ordering requirements.
 */
__BEGIN_DECLS
int	__acl_aclcheck_fd(int _filedes, acl_type_t _type, struct acl *_aclp);
int	__acl_aclcheck_file(const char *_path, acl_type_t _type,
	    struct acl *_aclp);
int	__acl_aclcheck_link(const char *_path, acl_type_t _type,
	    struct acl *_aclp);
int	__acl_delete_fd(int _filedes, acl_type_t _type);
int	__acl_delete_file(const char *_path_p, acl_type_t _type);
int	__acl_delete_link(const char *_path_p, acl_type_t _type);
int	__acl_get_fd(int _filedes, acl_type_t _type, struct acl *_aclp);
int	__acl_get_file(const char *_path, acl_type_t _type, struct acl *_aclp);
int	__acl_get_link(const char *_path, acl_type_t _type, struct acl *_aclp);
int	__acl_set_fd(int _filedes, acl_type_t _type, struct acl *_aclp);
int	__acl_set_file(const char *_path, acl_type_t _type, struct acl *_aclp);
int	__acl_set_link(const char *_path, acl_type_t _type, struct acl *_aclp);
__END_DECLS

/*
 * Supported POSIX.1e ACL manipulation and assignment/retrieval API
 * _np calls are local extensions that reflect an environment capable of
 * opening file descriptors of directories, and allowing additional
 * ACL type for different filesystems (i.e., AFS).
 */
__BEGIN_DECLS
int	acl_add_perm(acl_permset_t _permset_d, acl_perm_t _perm);
int	acl_calc_mask(acl_t *_acl_p);
int	acl_clear_perms(acl_permset_t _permset_d);
int	acl_copy_entry(acl_entry_t _dest_d, acl_entry_t _src_d);
ssize_t	acl_copy_ext(void *_buf_p, acl_t _acl, ssize_t _size);
acl_t	acl_copy_int(const void *_buf_p);
int	acl_create_entry(acl_t *_acl_p, acl_entry_t *_entry_p);
int	acl_delete_entry(acl_t _acl, acl_entry_t _entry_d);
int	acl_delete_fd_np(int _filedes, acl_type_t _type);
int	acl_delete_file_np(const char *_path_p, acl_type_t _type);
int	acl_delete_link_np(const char *_path_p, acl_type_t _type);
int	acl_delete_def_file(const char *_path_p);
int	acl_delete_def_link_np(const char *_path_p);
int	acl_delete_perm(acl_permset_t _permset_d, acl_perm_t _perm);
acl_t	acl_dup(acl_t _acl);
int	acl_free(void *_obj_p);
acl_t	acl_from_text(const char *_buf_p);
int	acl_get_entry(acl_t _acl, int _entry_id, acl_entry_t *_entry_p);
acl_t	acl_get_fd(int _fd);
acl_t	acl_get_fd_np(int fd, acl_type_t _type);
acl_t	acl_get_file(const char *_path_p, acl_type_t _type);
acl_t	acl_get_link_np(const char *_path_p, acl_type_t _type);
void	*acl_get_qualifier(acl_entry_t _entry_d);
int	acl_get_perm_np(acl_permset_t _permset_d, acl_perm_t _perm);
int	acl_get_permset(acl_entry_t _entry_d, acl_permset_t *_permset_p);
int	acl_get_tag_type(acl_entry_t _entry_d, acl_tag_t *_tag_type_p);
acl_t	acl_init(int _count);
int	acl_set_fd(int _fd, acl_t _acl);
int	acl_set_fd_np(int _fd, acl_t _acl, acl_type_t _type);
int	acl_set_file(const char *_path_p, acl_type_t _type, acl_t _acl);
int	acl_set_link_np(const char *_path_p, acl_type_t _type, acl_t _acl);
int	acl_set_permset(acl_entry_t _entry_d, acl_permset_t _permset_d);
int	acl_set_qualifier(acl_entry_t _entry_d, const void *_tag_qualifier_p);
int	acl_set_tag_type(acl_entry_t _entry_d, acl_tag_t _tag_type);
ssize_t	acl_size(acl_t _acl);
char	*acl_to_text(acl_t _acl, ssize_t *_len_p);
int	acl_valid(acl_t _acl);
int	acl_valid_fd_np(int _fd, acl_type_t _type, acl_t _acl);
int	acl_valid_file_np(const char *_path_p, acl_type_t _type, acl_t _acl);
int	acl_valid_link_np(const char *_path_p, acl_type_t _type, acl_t _acl);
__END_DECLS

#endif /* !_KERNEL */

#endif /* !_SYS_ACL_H */
