#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$FreeBSD$

LIB=	acl
CFLAGS+=-DDEBUG -DKERBEROS
SRCS=	acl_files.c
MAN3=	acl_check.3
MLINKS=	acl_check.3 acl_canonicalize_principal.3 \
	acl_check.3 acl_exact_match.3 acl_check.3 acl_add.3 \
	acl_check.3 acl_delete.3 acl_check.3 acl_initialize.3

.include <bsd.lib.mk>
