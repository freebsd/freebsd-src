# $FreeBSD: src/sys/dev/patm/genrtab/Makefile,v 1.4 2004/02/25 13:12:51 johan Exp $
#
# This program is used to generate the if_patm_rtables.c file
# for the idt77252 driver. It is not installed.
#
PROG=	genrtab
INTERNALPROG=
WARNS?=	5
NOMAN=	true
DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
