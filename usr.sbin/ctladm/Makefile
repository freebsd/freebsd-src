# $FreeBSD$

PROG=		ctladm
SRCS=		ctladm.c util.c ctl_util.c ctl_scsi_all.c
.PATH:		${.CURDIR}/../../sys/cam/ctl
SDIR=		${.CURDIR}/../../sys
CFLAGS+=	-I${SDIR}
# This is necessary because of these warnings:
# warning: cast increases required alignment of target type
# The solution is to either upgrade the compiler (preferred), or do void
# pointer gymnastics to get around the warning.  For now, disable the
# warning instead of doing the void pointer workaround.
.if ${MACHINE_CPUARCH} == "arm"
WARNS?= 3
.endif

LIBADD=		cam sbuf bsdxml util
MAN=		ctladm.8

.include <bsd.prog.mk>
