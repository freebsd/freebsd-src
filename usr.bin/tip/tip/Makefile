#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#
# Files are:
#	/etc/remote		remote host description file
#	/etc/phones		phone number file, owned by ${OWNER} and
#				mode 6??
#	/var/log/aculog		ACU accounting file, owned by ${OWNER} and
#				mode 6?? {if ACULOG defined}

.if exists(${.CURDIR}/../libacu/obj)
LIBACU=${.CURDIR}/../libacu/obj/libacu.a
.else
LIBACU=${.CURDIR}/../libacu/libacu.a
.endif

PROG=	tip
LDADD+=	$(LIBACU)
LINKS=	${BINDIR}/tip
MAN1=	tip.1
MAN5=	modems.5
SRCS=	acu.c acutab.c cmds.c cmdtab.c cu.c hunt.c log.c partab.c \
	remote.c tip.c tipout.c uucplock.c value.c vars.c 

.include <bsd.prog.mk>
