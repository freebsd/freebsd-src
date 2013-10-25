PROG=	rendering-bandwidth

MAN=

# TODO: Fix this. Life's too short to spend all of it failing to understand BSD Make.
LDADD+= -lcompositor \
	/pool/users/prw35/freebsd/root-cheribsd/usr/lib/libpixman-1.so
CFLAGS+= -I/usr/local/include/pixman-1

.include <bsd.prog.mk>
