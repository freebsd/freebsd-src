# $FreeBSD$

LIB=	kadm5srv
CFLAGS+=-I${KRB5DIR}/include \
	-I${KRB5DIR}/lib/krb5 \
	-I${KRB5DIR}/lib/kadm5 \
	-I${KRB5DIR}/lib/hdb \
	-I${KRB5DIR}/lib/asn1 \
	-I${KRB5DIR}/lib/roken \
	-I${ASN1OBJDIR} \
	-I${HDBOBJDIR} \
	-I${.OBJDIR}

SRCS=	\
	acl.c					\
	admin.h					\
	bump_pw_expire.c			\
	chpass_s.c				\
	common_glue.c				\
	context_s.c				\
	create_s.c				\
	delete_s.c				\
	destroy_s.c				\
	ent_setup.c				\
	error.c					\
	flush_s.c				\
	free.c					\
	get_princs_s.c				\
	get_s.c					\
	init_s.c				\
	kadm5_err.c				\
	kadm5_err.h				\
	kadm5_locl.h				\
	keys.c					\
	log.c					\
	marshall.c				\
	modify_s.c				\
	private.h				\
	privs_s.c				\
	randkey_s.c				\
	rename_s.c				\
	set_keys.c				\
	set_modifier.c				\
	password_quality.c			\
	server_glue.c

.include <bsd.lib.mk>

.PATH:		${KRB5DIR}/lib/kadm5
