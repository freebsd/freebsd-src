# $FreeBSD: src/sys/modules/iscsi/initiator/Makefile,v 1.1 2007/07/24 15:35:02 scottl Exp $

.PATH: ${.CURDIR}/../../../dev/iscsi/initiator
KMOD=iscsi_initiator

SRCS=  iscsi.h iscsivar.h
SRCS+= iscsi.c isc_cam.c isc_soc.c isc_sm.c isc_subr.c iscsi_subr.c
SRCS+= opt_cam.h opt_iscsi_initiator.h
SRCS+= bus_if.h device_if.h

# Debugging
# CFLAGS+= -DISCSI_INITIATOR_DEBUG=9

.include <bsd.kmod.mk>
