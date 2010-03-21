# $FreeBSD: src/sys/modules/dtrace/Makefile.inc,v 1.1.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $

IGNORE_PRAGMA=	1

load	:
	-kldload cyclic
	-kldload dtrace
.if ${MACHINE_ARCH} == "i386"
	-kldload sdt
	-kldload lockstat
	-kldload fbt
	-kldload prototype
.endif
	-kldload profile
	-kldload systrace
	kldstat

unload	:
	-kldunload systrace
	-kldunload profile
.if ${MACHINE_ARCH} == "i386"
	-kldunload prototype
	-kldunload fbt
	-kldunload lockstat
	-kldunload sdt
.endif
	-kldunload dtrace
	-kldunload cyclic
	kldstat
