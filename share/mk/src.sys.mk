# $FreeBSD$

# Allow user to configure things that only effect src tree builds.
SRCCONF?=	/etc/src.conf
.if exists(${SRCCONF}) || ${SRCCONF} != "/etc/src.conf"
.include "${SRCCONF}"
.endif
