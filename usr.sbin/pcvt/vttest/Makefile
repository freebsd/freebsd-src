
.if !exists(${.CURDIR}/../Makefile.inc)
error:
	@echo
	@echo "                      You MUST link/copy"
	@echo
	@echo "       ../Makefile.inc.NetBSD    or   ../Makefile.inc.FreeBSD"
	@echo
	@echo "             to ../Makefile.inc before you can proceed !"
	@echo
.else

PROG=		vttest
CFLAGS+=	-traditional -DUSEMYSTTY
SRCS=		main.c esc.c

.include <bsd.prog.mk>

.endif
