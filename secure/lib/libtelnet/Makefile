#	$FreeBSD$

LIB=		telnet

SRCS=		genget.c getent.c misc.c encrypt.c auth.c \
		enc_des.c sra.c pk.c

CFLAGS+=        -DHAS_CGETENT -DENCRYPTION -DDES_ENCRYPTION -DAUTHENTICATION \
                -DSRA -I${TELNETDIR} \

NOPIC=          yes

INCS=		${TELNETDIR}/arpa/telnet.h
INCDIR=		/usr/include/arpa

.include <bsd.lib.mk>

.PATH:	${TELNETDIR}/libtelnet

#
# Before complaining about this, please *double-check* that you have
# updated the ldconfig path in /etc/rc to include /usr/lib/compat that
# was added in src/etc/rc rev 1.98.
# This is so that `ld' will not continue to generate binaries linked
# shared against libtelnet, so that in a future release we can move this
# off to a compat dist (like compat22).
#
beforeinstall:
	rm -f ${DESTDIR}${LIBDIR}/lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR}
