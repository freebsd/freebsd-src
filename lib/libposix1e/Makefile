# $FreeBSD$

MAINTAINER=	rwatson@FreeBSD.org
LIB=	posix1e
SHLIB_MAJOR= 2 
SHLIB_MINOR= 0
SRCS+=	acl_delete.c			\
	acl_free.c			\
	acl_from_text.c			\
	acl_get.c			\
	acl_init.c			\
	acl_set.c			\
	acl_support.c			\
	acl_to_text.c			\
	acl_valid.c			\
	cap_clear.c			\
	cap_dup.c			\
	cap_free.c			\
	cap_get_fd.c			\
	cap_get_file.c			\
	cap_get_flag.c			\
	cap_get_proc.c			\
	cap_init.c			\
	cap_set_fd.c			\
	cap_set_file.c			\
	cap_set_flag.c			\
	cap_set_proc.c			\
	cap_text.c


MAN3=	acl.3				\
	acl_delete.3			\
	acl_dup.3			\
	acl_free.3			\
	acl_from_text.3			\
	acl_get.3			\
	acl_init.3			\
	acl_set.3			\
	acl_to_text.3			\
	acl_valid.3			\
	cap.3				\
	cap_clear.3			\
	cap_dup.3			\
	cap_free.3			\
	cap_from_text.3			\
	cap_get_flag.3			\
	cap_get_proc.3			\
	cap_init.3			\
	cap_set_flag.3			\
	cap_set_proc.3			\
	cap_to_text.3			\
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
	acl_valid.3 acl_valid_fd_np.3

.include <bsd.lib.mk>
