#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	kgmon
MAN8=	kgmon.0
DPADD=	${LIBKVM}
LDADD=	-lkvm
#
# This program may safely be run setuid-root to allow non-root
# users to start, stop, and reset profiling buffers.
#
#BINOWN=root
#BINMODE=4555

.include <bsd.prog.mk>
