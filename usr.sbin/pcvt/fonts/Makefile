
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

#FONTS	= 	vt100pc.814 vt100sg.814 vt220l.814 vt220h.814 vt220l.808\
#	  	vt220h.808 vt220l.816 vt220h.816 vt220l.810 vt220h.810

FONTS	= 	vt220l.814 vt220h.814 vt220l.808 vt220h.808 \
		vt220l.816 vt220h.816 vt220l.810 vt220h.810

LIBMODE	=	644
LIBOWN	=	bin
LIBGRP	=	bin
CLEANFILES= 	${FONTS}

all: $(FONTS)

install:
	@if [ ! -d ${DESTDIR}${FONTDIR} ]; then mkdir ${DESTDIR}${FONTDIR};fi
	@for i in ${FONTS}; do \
		${ECHO} "installing font $$i into ${DESTDIR}${FONTDIR}"; \
		${INSTALL} -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
		$$i ${DESTDIR}${FONTDIR}; \
	done

clean:
	rm -f ${CLEANFILES}

.include <bsd.prog.mk>

# this seems to be the lowest common denominator

vt100pc.814: ${.CURDIR}/vt100pc.814.uu
	uudecode ${.CURDIR}/$@.uu

vt100sg.814: ${.CURDIR}/vt100sg.814.uu
	uudecode ${.CURDIR}/$@.uu

vt220l.814: ${.CURDIR}/vt220l.814.uu
	uudecode ${.CURDIR}/$@.uu

vt220h.814: ${.CURDIR}/vt220h.814.uu
	uudecode ${.CURDIR}/$@.uu

vt220l.808: ${.CURDIR}/vt220l.808.uu
	uudecode ${.CURDIR}/$@.uu

vt220h.808: ${.CURDIR}/vt220h.808.uu
	uudecode ${.CURDIR}/$@.uu

vt220l.816: ${.CURDIR}/vt220l.816.uu
	uudecode ${.CURDIR}/$@.uu

vt220h.816: ${.CURDIR}/vt220h.816.uu
	uudecode ${.CURDIR}/$@.uu

vt220l.810: ${.CURDIR}/vt220l.810.uu
	uudecode ${.CURDIR}/$@.uu

vt220h.810: ${.CURDIR}/vt220h.810.uu
	uudecode ${.CURDIR}/$@.uu

.endif
