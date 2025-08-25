BINDIR=		/usr/share/certs/untrusted

UNTRUSTED_CERTS!=	(cd ${.CURDIR} && echo *.pem)

FILES+=	 ${UNTRUSTED_CERTS}

cleancerts: .PHONY
	@(cd ${.CURDIR} && rm -f ${UNTRUSTED_CERTS})

.include <bsd.prog.mk>
