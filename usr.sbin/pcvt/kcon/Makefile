
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

PROG	=	kcon
DEVICE	=	/dev/ttyv0
CFLAGS	+= 	-I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"

# the -Lfoo could be omitted if libkeycap.a were installed before
# making those programs here

LDADD =       -L${.CURDIR}/../keycap -L${.CURDIR}/../keycap/obj -lkeycap

.include <bsd.prog.mk>

$(PROG):	$(INCDIR)/pcvt_ioctl.h

.endif
