#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	reboot
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN8=	reboot.8 boot_hp300.8 boot_i386.8 boot_sparc.8 boot_tahoe.8 boot_vax.8
MLINKS=	reboot.8 halt.8
LINKS=	${BINDIR}/reboot ${BINDIR}/halt

XXXBROKENafterinstall:
	${MINSTALL} boot_hp300.8 ${DESTDIR}${MANDIR}8/hp300/boot.8
	${MINSTALL} boot_i386.8 ${DESTDIR}${MANDIR}8/i386/boot.8
	${MINSTALL} boot_sparc.8 ${DESTDIR}${MANDIR}8/sparc/boot.8
	${MINSTALL} boot_tahoe.8 ${DESTDIR}${MANDIR}8/tahoe/boot.8
	${MINSTALL} boot_vax.8 ${DESTDIR}${MANDIR}8/vax/boot.8

.include <bsd.man.mk>
.include <bsd.prog.mk>
