#	$FreeBSD$

LIB=		telnet

INTERNALLIB=	yes
INTERNALSTATICLIB=yes
NOPIC=		yes

SRCS=		genget.c getent.c misc.c encrypt.c auth.c \
		enc_des.c sra.c pk.c

CFLAGS+=	-DHAS_CGETENT -DENCRYPTION -DDES_ENCRYPTION -DAUTHENTICATION \
		-DSRA -I${TELNETDIR}

INCS=		${TELNETDIR}/arpa/telnet.h
INCDIR=		/usr/include/arpa

.include <bsd.lib.mk>

.PATH:	${TELNETDIR}/libtelnet

#
# Remove obsolete shared libraries, if any.  We don't bother moving them
# to /usr/lib/compat, since they were only used by telnet, telnetd and
# tn3270.
#
beforeinstall: __remove-stale-libs
__remove-stale-libs: .PHONY
.if exists(${DESTDIR}${SHLIBDIR}/lib${LIB}.so.2.0)
	-chflags noschg ${DESTDIR}${SHLIBDIR}/lib${LIB}.so.2.0
	rm -f ${DESTDIR}${SHLIBDIR}/lib${LIB}.so.2.0
.endif
