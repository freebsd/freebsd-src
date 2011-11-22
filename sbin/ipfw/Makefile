# $FreeBSD$

PROG=	ipfw
SRCS=	ipfw2.c dummynet.c ipv6.c main.c nat.c altq.c
SRCS+=	diffuse_ui.c diffuse_modules.c diffuse_ui_classifier_c45.c
SRCS+=	diffuse_ui_classifier_nbayes.c diffuse_ui_feature_iat.c
SRCS+=	diffuse_ui_feature_iatbd.c diffuse_ui_feature_pcnt.c
SRCS+=	diffuse_ui_feature_plen.c diffuse_ui_feature_plenbd.c
SRCS+=	diffuse_ui_feature_skype.c
WARNS?=	2
DPADD=	${LIBUTIL}
DPADD+=	${LIBM}
LDADD=	-lutil -lm
MAN=	ipfw.8
SUBDIR=	diffuse_exporter

.include <bsd.prog.mk>
