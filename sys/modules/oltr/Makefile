# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/dev/oltr
KMOD	= if_oltr
SRCS	= if_oltr.c trlldbm.c trlldhm.c trlldmac.c \
	  device_if.h bus_if.h pci_if.h
OBJS+=	trlld.o

trlld.o:
	uudecode < ${.CURDIR}/../../contrib/dev/oltr/i386-elf.trlld.o.uu

.include <bsd.kmod.mk>
