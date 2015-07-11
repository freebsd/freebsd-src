# $FreeBSD$

PROG=	pw
MAN=	pw.conf.5 pw.8
SRCS=	pw.c pw_conf.c pw_user.c pw_group.c pw_log.c pw_nis.c pw_vpw.c \
	grupd.c pwupd.c psdate.c bitmap.c cpdir.c rm_r.c

WARNS?=	3

LIBADD=	crypt util sbuf

.include <src.opts.mk>

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
