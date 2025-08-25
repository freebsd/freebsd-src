
BINDIR=		/usr/share/certs/blacklisted

BLACKLISTED_CERTS!=	(cd ${.CURDIR} && echo *.pem)

FILES+=	 ${BLACKLISTED_CERTS}

cleancerts: .PHONY
	@(cd ${.CURDIR} && rm -f ${BLACKLISTED_CERTS})

.include <bsd.prog.mk>
