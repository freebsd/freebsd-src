# $FreeBSD$

SRCDIR=		${.CURDIR}/../../../../contrib/pam_modules/pam_passwdqc

LIB=		pam_passwdqc
SRCS=		pam_passwdqc.c passwdqc_check.c passwdqc_random.c wordset_4k.c
CFLAGS+=	-I${SRCDIR} -DPAM_AUTHTOK_RECOVER_ERR=PAM_AUTHTOK_RECOVERY_ERR
MAN=		#pam_passwdqc.8

.include <bsd.lib.mk>

.PATH:		${SRCDIR}
