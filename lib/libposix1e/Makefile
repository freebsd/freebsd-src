#	$FreeBSD: src/lib/libposix1e/Makefile,v 1.3 2000/01/28 20:06:56 rwatson Exp $

LIB=	posix1e
SRCS+=	acl_delete.c			\
	acl_free.c			\
	acl_from_text.c			\
	acl_get.c			\
	acl_init.c			\
	acl_set.c			\
	acl_support.c			\
	acl_to_text.c			\
	acl_valid.c

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
