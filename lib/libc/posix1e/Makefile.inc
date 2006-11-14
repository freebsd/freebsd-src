# $FreeBSD$

.PATH: ${.CURDIR}/posix1e

SRCS+=	acl_calc_mask.c			\
	acl_copy.c			\
	acl_delete.c			\
	acl_delete_entry.c		\
	acl_entry.c			\
	acl_free.c			\
	acl_from_text.c			\
	acl_get.c			\
	acl_init.c			\
	acl_perm.c			\
	acl_set.c			\
	acl_support.c			\
	acl_to_text.c			\
	acl_valid.c			\
	extattr.c			\
	mac.c				\
	mac_exec.c			\
	mac_get.c			\
	mac_set.c

MAN+=	acl.3				\
	acl_add_perm.3			\
	acl_calc_mask.3			\
	acl_clear_perms.3		\
	acl_copy_entry.3		\
	acl_create_entry.3		\
	acl_delete.3			\
	acl_delete_entry.3		\
	acl_delete_perm.3		\
	acl_dup.3			\
	acl_free.3			\
	acl_from_text.3			\
	acl_get.3			\
	acl_get_entry.3			\
	acl_get_permset.3		\
	acl_get_perm_np.3		\
	acl_get_qualifier.3		\
	acl_get_tag_type.3		\
	acl_init.3			\
	acl_set.3			\
	acl_set_permset.3		\
	acl_set_qualifier.3		\
	acl_set_tag_type.3		\
	acl_to_text.3			\
	acl_valid.3			\
	extattr.3			\
	mac.3				\
	mac.conf.5			\
	mac_free.3			\
	mac_is_present_np.3		\
	mac_get.3			\
	mac_prepare.3			\
	mac_set.3			\
	mac_text.3			\
	posix1e.3

MLINKS+=acl_delete.3 acl_delete_def_file.3	\
	acl_delete.3 acl_delete_file_np.3	\
	acl_delete.3 acl_delete_fd_np.3		\
	acl_get.3 acl_get_file.3		\
	acl_get.3 acl_get_fd.3			\
	acl_get.3 acl_get_fd_np.3		\
	acl_set.3 acl_set_file.3		\
	acl_set.3 acl_set_fd.3			\
	acl_set.3 acl_set_fd_np.3		\
	acl_valid.3 acl_valid_file_np.3		\
	acl_valid.3 acl_valid_fd_np.3		\
	extattr.3 extattr_namespace_to_string.3	\
	extattr.3 extattr_string_to_namespace.3 \
	mac_get.3 mac_get_fd.3			\
	mac_get.3 mac_get_file.3		\
	mac_get.3 mac_get_link.3		\
	mac_get.3 mac_get_pid.3			\
	mac_get.3 mac_get_proc.3		\
	mac_prepare.3 mac_prepare_file_label.3	\
	mac_prepare.3 mac_prepare_ifnet_label.3	\
	mac_prepare.3 mac_prepare_process_label.3	\
	mac_set.3 mac_set_link.3		\
	mac_set.3 mac_set_fd.3			\
	mac_set.3 mac_set_file.3		\
	mac_set.3 mac_set_proc.3		\
	mac_text.3 mac_from_text.3		\
	mac_text.3 mac_to_text.3
