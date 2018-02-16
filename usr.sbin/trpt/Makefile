#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

PROG=	trpt
MAN=	trpt.8
BINGRP=	kmem
BINMODE= 2555

.if ${MACHINE_CPUARCH} != "aarch64" && ${MACHINE_CPUARCH} != "amd64" && \
    ${MACHINE_CPUARCH} != "i386" && ${MACHINE_CPUARCH} != "riscv"
WARNS?=	4
.endif

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "riscv"
# Several included system headers tickle this warning in ways that are
# difficult to work around in this program.
CFLAGS+= -Wno-missing-variable-declarations
.endif
