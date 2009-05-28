# $FreeBSD$

.PATH: ${.CURDIR}/posix1e

CFLAGS+=-D_ACL_PRIVATE

SRCS += \
	acl_calc_mask.c \
	acl_compat.c \
	acl_copy.c \
	acl_delete.c \
	acl_delete_entry.c \
	acl_entry.c \
	acl_free.c \
	acl_from_text.c \
	acl_get.c \
	acl_init.c \
	acl_perm.c \
	acl_set.c \
	acl_support.c \
	acl_to_text.c \
	acl_valid.c \
	extattr.c \
	mac.c \
	mac_exec.c \
	mac_get.c \
	mac_set.c

SYM_MAPS += ${.CURDIR}/posix1e/Symbol.map
