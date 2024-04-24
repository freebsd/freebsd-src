#
#

OPENBSMDIR=${SRCTOP}/contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditd
.PATH: ${OPENBSMDIR}/man

CFLAGS+=	-I${OPENBSMDIR}

OPENBSMETCDIR=  ${OPENBSMDIR}/etc
CONFS=  ${OPENBSMETCDIR}/audit_class
CONFSMODE_${OPENBSMETCDIR}/audit_class= 444
CONFS+= ${OPENBSMETCDIR}/audit_control
CONFSMODE_${OPENBSMETCDIR}/audit_control=       600
CONFS+= ${OPENBSMETCDIR}/audit_event
CONFSMODE_${OPENBSMETCDIR}/audit_event= 444
CONFS+= ${OPENBSMETCDIR}/audit_user
CONFSMODE_${OPENBSMETCDIR}/audit_user=  600
CONFS+= ${OPENBSMETCDIR}/audit_warn
CONFSMODE_${OPENBSMETCDIR}/audit_warn=  500
CONFSDIR=       /etc/security

PACKAGE=audit
PROG=	auditd
SRCS=	auditd.c audit_warn.c auditd_fbsd.c
MAN=	auditd.8	\
	audit.log.5	\
	audit_class.5	\
	audit_control.5	\
	audit_event.5	\
	audit_user.5	\
	audit_warn.5

LIBADD=	auditd bsm

WARNS?=	3

.include <bsd.prog.mk>
