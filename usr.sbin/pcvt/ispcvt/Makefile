
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

PROG	=	ispcvt
MAN8	=	ispcvt.${MAN8EXT}

.include <bsd.prog.mk>

# special bindir, this MUST stand after include <bsd.prog.mk>
BINDIR	=	/usr/sbin

$(PROG):	$(INCDIR)/pcvt_ioctl.h

beforeinstall:
	@if [ ! -d $(MANDIR)8 ] ; then		\
	echo making $(MANDIR)8; mkdir $(MANDIR)8; fi

.endif
